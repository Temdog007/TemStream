#include <main.hpp>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

template <typename T> struct XCB_Deleter
{
	constexpr XCB_Deleter() noexcept = default;
	template <typename U, typename = std::_Require<std::is_convertible<U *, T *>>>
	XCB_Deleter(const XCB_Deleter<U> &) noexcept
	{
	}
	void operator()(T *t) const
	{
		if (t != nullptr)
		{
			free(t);
		}
	}
};

template <typename T> using xcb_ptr = std::unique_ptr<T, XCB_Deleter<T>>;

struct XCB_ConnectionDeleter
{
	void operator()(xcb_connection_t *t) const
	{
		xcb_disconnect(t);
	}
};

using XCB_Connection = std::unique_ptr<xcb_connection_t, XCB_ConnectionDeleter>;

template <typename T> xcb_ptr<T> makeXCB(T *t)
{
	return xcb_ptr<T>(t, XCB_Deleter<T>());
}

namespace TemStream
{
template <const size_t N> std::optional<xcb_atom_t> getAtom(xcb_connection_t *con, const char (&name)[N])
{
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(con, 1, N - 1, name);
	xcb_generic_error_t *errorPtr = nullptr;
	xcb_intern_atom_reply_t *replyPtr = xcb_intern_atom_reply(con, cookie, &errorPtr);
	auto error = makeXCB(errorPtr);
	auto reply = makeXCB(replyPtr);
	if (error || !reply)
	{
		return std::nullopt;
	}
	return reply->atom;
}
bool isDesktopWindow(xcb_connection_t *con, const xcb_window_t window)
{
	auto atom = getAtom(con, "_NET_WM_WINDOW_TYPE");
	if (!atom.has_value())
	{
		return false;
	}

	auto property = makeXCB<xcb_get_property_reply_t>(nullptr);
	{
		xcb_get_property_cookie_t cookie = xcb_get_property(con, 0, window, *atom, XCB_GET_PROPERTY_TYPE_ANY, 0, ~0U);
		xcb_generic_error_t *errorPtr = nullptr;
		xcb_get_property_reply_t *replyPtr = xcb_get_property_reply(con, cookie, &errorPtr);
		auto error = makeXCB(errorPtr);
		property = makeXCB(replyPtr);
		if (error || !property)
		{
			return false;
		}
	}
	const int len = xcb_get_property_value_length(property.get());
	if (len <= 0)
	{
		return false;
	}

	xcb_atom_t *names = reinterpret_cast<xcb_atom_t *>(xcb_get_property_value(property.get()));
	const int count = len / sizeof(xcb_atom_t);
	for (int i = 0; i < count; ++i)
	{
		xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(con, names[i]);
		xcb_generic_error_t *errorPtr = nullptr;
		xcb_get_atom_name_reply_t *replyPtr = xcb_get_atom_name_reply(con, cookie, &errorPtr);
		auto error = makeXCB(errorPtr);
		auto reply = makeXCB(replyPtr);
		if (error || !reply)
		{
			continue;
		}
		const int len = xcb_get_atom_name_name_length(reply.get());
		if (len > 0)
		{
			char *str = xcb_get_atom_name_name(reply.get());
			if (strncmp("_NET_WM_WINDOW_TYPE_NORMAL", str, len) == 0)
			{
				return true;
			}
		}
	}

	return false;
}
void getX11Windows(xcb_connection_t *con, const xcb_window_t window, WindowProcesses &set)
{
	{
		xcb_query_tree_cookie_t cookie = xcb_query_tree(con, window);
		xcb_generic_error_t *errorPtr = nullptr;
		xcb_query_tree_reply_t *replyPtr = xcb_query_tree_reply(con, cookie, &errorPtr);
		auto error = makeXCB(errorPtr);
		auto reply = makeXCB(replyPtr);
		if (!error && reply)
		{
			const int childrenLen = xcb_query_tree_children_length(reply.get());
			// windows free'd when reply is free'd
			xcb_window_t *windows = xcb_query_tree_children(reply.get());
			for (int i = 0; i < childrenLen; ++i)
			{
				getX11Windows(con, windows[i], set);
			}
		}
	}

	if (!isDesktopWindow(con, window))
	{
		return;
	}

	const auto atom = getAtom(con, "_NET_WM_NAME");
	if (atom.has_value())
	{
		xcb_generic_error_t *errorPtr = nullptr;
		xcb_get_property_cookie_t cookie = xcb_get_property(con, 0, window, *atom, XCB_GET_PROPERTY_TYPE_ANY, 0, ~0U);
		xcb_get_property_reply_t *replyPtr = xcb_get_property_reply(con, cookie, &errorPtr);
		auto error = makeXCB(errorPtr);
		auto reply = makeXCB(replyPtr);
		if (!error && reply)
		{
			const int len = xcb_get_property_value_length(reply.get());
			if (len > 0)
			{
				const char *name = (char *)xcb_get_property_value(reply.get());
				set.emplace(String(name, name + len), *atom);
			}
		}
	}
}
XCB_Connection getXCBConnection(int &screenNum)
{
	xcb_connection_t *conPtr = xcb_connect(NULL, &screenNum);
	if (xcb_connection_has_error(conPtr))
	{
		(*logger)(Logger::Error) << "Error establishing connection with X11 server" << std::endl;
	}

	return XCB_Connection(conPtr, XCB_ConnectionDeleter());
}
WindowProcesses getAllX11Windows(XCB_Connection &con, int screenNum)
{
	const xcb_setup_t *setup = xcb_get_setup(con.get());
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);

	for (int i = 0; i < screenNum; ++i)
	{
		xcb_screen_next(&iter);
	}

	xcb_screen_t *screen = iter.data;
	const xcb_window_t root = screen->root;

	WindowProcesses set;
	getX11Windows(con.get(), root, set);
	return set;
}
WindowProcesses Video::getRecordableWindows()
{
	int screenNum;
	auto con = getXCBConnection(screenNum);

	return getAllX11Windows(con, screenNum);
}

using Converter = Video::RGBA2YUV<unique_ptr<xcb_shm_get_image_reply_t>>;
class Screenshotter
{
  private:
	std::weak_ptr<Converter> converter;
	WindowProcess window;
	std::weak_ptr<Video> video;
	uint64_t fps;

  public:
	Screenshotter(const WindowProcess &w, const shared_ptr<Converter> &ptr, const shared_ptr<Video> &v,
				  const uint32_t fps)
		: converter(ptr), window(w), video(v), fps(fps)
	{
	}
	~Screenshotter()
	{
	}

	static void startTakingScreenshots(shared_ptr<Screenshotter> &&ss)
	{
		std::thread thread(takeScreenshots, std::move(ss));
		thread.detach();
	}

	static void takeScreenshots(shared_ptr<Screenshotter> &&data)
	{
		(*logger)(Logger::Trace) << "Starting to record " << data->window.name << std::endl;
		int unused;
		auto con = getXCBConnection(unused);
		const uint64_t delay = 1000 / data->fps;
		uint64_t last = 0;
		while (!appDone)
		{
			const uint64_t now = SDL_GetTicks64();
			const uint64_t diff = now - last;
			if (diff < delay)
			{
				SDL_Delay(diff);
				continue;
			}
			last = now;

			{
				auto ptr = data->video.lock();
				if (!ptr)
				{
					break;
				}
			}
			auto converter = data->converter.lock();
			if (!converter)
			{
				break;
			}

			// Get screenshot, send to converter
		}

		(*logger)(Logger::Trace) << "Ending recording of " << data->window.name << std::endl;
	}
};

shared_ptr<Video> Video::recordWindow(const WindowProcess &wp, const Message::Source &source,
									  const List<int32_t> &ratios, const uint32_t fps)
{

	FrameEncoders encoders;
	for (const auto ratio : ratios)
	{
		auto encoder = tem_shared<FrameEncoder>(source, ratio);
		encoders.push_back(std::weak_ptr<FrameEncoder>(encoder));
		FrameEncoder::startEncodingFrames(std::move(encoder));
	}
	auto converter = tem_shared<Converter>(std::move(encoders));
	auto video = tem_shared<Video>(source, wp);
	auto screenshotter = tem_shared<Screenshotter>(wp, converter, video, fps);
	Converter::startConverteringFrames(std::move(converter));
	Screenshotter::startTakingScreenshots(std::move(screenshotter));
	return video;
}
} // namespace TemStream