#include <main.hpp>

#include <video_x11.hpp>

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
Dimensions getWindowSize(xcb_connection_t *con, const xcb_window_t id)
{
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(con, id);
	xcb_generic_error_t *errorPtr = nullptr;
	xcb_get_geometry_reply_t *geomPtr = xcb_get_geometry_reply(con, cookie, &errorPtr);
	auto error = makeXCB(errorPtr);
	auto geom = makeXCB(geomPtr);

	if (error || !geom)
	{
		if (error)
		{
			printf("%u\n", error->error_code);
		}
		if (!geom)
		{
			printf("No geometry\n");
		}
		return std::nullopt;
	}
	return std::make_pair(geom->width, geom->height);
}
bool isDesktopWindow(xcb_connection_t *con, const xcb_window_t window)
{
	auto atom = getAtom(con, "_NET_WM_WINDOW_TYPE");
	if (!atom.has_value())
	{
		return false;
	}

	auto dim = getWindowSize(con, window);
	if (!dim)
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
				set.emplace(String(name, name + len), window);
			}
		}
	}
}
XCB_Connection getXCBConnection(int &screenNum)
{
	auto con = XCB_Connection(xcb_connect(NULL, &screenNum), XCB_ConnectionDeleter());
	if (xcb_connection_has_error(con.get()))
	{
		(*logger)(Logger::Error) << "Error establishing connection with X11 server" << std::endl;
		return nullptr;
	}

	return con;
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

shared_ptr<Video::Frame> Converter::convertToFrame(Screenshot &&s) const
{
	void *data = xcb_get_image_data(s.reply.get());

	auto frame = tem_shared<Video::Frame>();
	frame->width = s.width;
	frame->height = s.height;
	frame->bytes.resize((s.width * s.height) * 2, '\0');
	if (SDL_ConvertPixels(s.width, s.height, SDL_PIXELFORMAT_RGBA8888, data, s.width * 4, SDL_PIXELFORMAT_IYUV,
						  frame->bytes.data(), s.width) == 0)
	{
		return frame;
	}
	else
	{
		logSDLError("Failed to convert pixels");
		return nullptr;
	}
}
Dimensions Screenshotter::getSize(xcb_connection_t *con)
{
	return getWindowSize(con, window.windowId);
}
void Screenshotter::startTakingScreenshots(shared_ptr<Screenshotter> &&ss)
{
	Task::addTask(std::async(TaskPolicy, takeScreenshots, std::move(ss)));
}
void Screenshotter::takeScreenshots(shared_ptr<Screenshotter> &&data)
{
	(*logger)(Logger::Trace) << "Starting to record: " << data->window.name << ' ' << data->fps << "FPS" << std::endl;
	int unused;
	auto con = getXCBConnection(unused);

	const uint32_t delay = 1000 / data->fps;
	uint32_t last = 0;

	Dimensions dim = data->getSize(con.get());
	if (!dim)
	{
		(*logger)(Logger::Error) << "Failed to get size of window: " << data->window.name << std::endl;
		goto end;
	}

	while (!appDone)
	{
		const uint32_t now = SDL_GetTicks();
		const uint32_t diff = now - last;
		if (diff < delay)
		{
			SDL_Delay(diff);
			continue;
		}
		last = now;

		auto video = data->video.lock();
		if (!video)
		{
			break;
		}

		auto &source = video->getSource();

		auto converter = data->converter.lock();
		if (!converter)
		{
			break;
		}

		dim = data->getSize(con.get());
		if (!dim)
		{
			(*logger)(Logger::Warning) << "Window " << source << " is hidden" << std::endl;
			data->windowHidden = true;
			continue;
		}

		xcb_get_image_cookie_t cookie = xcb_get_image(con.get(), XCB_IMAGE_FORMAT_Z_PIXMAP, data->window.windowId, 0, 0,
													  dim->first, dim->second, ~0U);
		xcb_generic_error_t *errorPtr = nullptr;
		auto replyPtr = xcb_get_image_reply(con.get(), cookie, &errorPtr);
		auto error = makeXCB(errorPtr);
		auto reply = makeXCB(replyPtr);
		if (error || !reply)
		{
			break;
		}

		if (data->windowHidden)
		{
			(*logger)(Logger::Info) << "Window is visible again" << std::endl;
			data->windowHidden = false;
		}

		Screenshot s;
		s.reply = std::move(reply);
		s.width = dim->first;
		s.height = dim->second;
		converter->addFrame(std::move(s));
	}

end:
	(*logger)(Logger::Trace) << "Ending recording of: " << data->window.name << std::endl;
}

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
	auto converter = tem_shared<Converter>(std::move(encoders), source);
	auto video = tem_shared<Video>(source, wp);
	auto screenshotter = tem_shared<Screenshotter>(wp, converter, video, fps);
	Converter::startConverteringFrames(std::move(converter));
	Screenshotter::startTakingScreenshots(std::move(screenshotter));
	return video;
}
} // namespace TemStream