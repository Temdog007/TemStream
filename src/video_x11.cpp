/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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
		(*logger)(Logger::Level::Error) << "Error establishing connection with X11 server" << std::endl;
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
WindowProcesses VideoSource::getRecordableWindows()
{
	int screenNum;
	auto con = getXCBConnection(screenNum);

	return getAllX11Windows(con, screenNum);
}

Screenshotter::Screenshotter(XCB_Connection &&con, const WindowProcess &w, shared_ptr<Converter> ptr,
							 shared_ptr<VideoSource> v, const uint32_t fps)
	: converter(ptr), window(w), video(v), con(std::move(con)), fps(fps), visible(true), first(true)
{
}
Screenshotter::~Screenshotter()
{
}
Dimensions Screenshotter::getSize(xcb_connection_t *con)
{
	return getWindowSize(con, window.windowId);
}
void Screenshotter::startTakingScreenshots(shared_ptr<Screenshotter> ss)
{
	WorkPool::addWork([ss]() {
		if (!takeScreenshot(ss))
		{
			*logger << "Ending recording " << ss->window.name << std::endl;
			ss->video->setRunning(false);
			return false;
		}
		return true;
	});
}
bool Screenshotter::takeScreenshot(shared_ptr<Screenshotter> data)
{
	if (data->first)
	{
		*logger << "Starting recording " << data->window.name << std::endl;
		data->first = false;
	}
	{
		const auto now = std::chrono::system_clock::now();
		if (now < data->nextFrame)
		{
			return true;
		}

		const auto delay = std::chrono::duration<double, std::milli>(1000.0 / data->fps);
		data->nextFrame = now + delay;
	}

	Dimensions dim = data->getSize(data->con.get());
	if (!dim)
	{
		(*logger)(Logger::Level::Error) << "Failed to get size of window: " << data->window.name << std::endl;
		return false;
	}

	if (!data->video->isRunning())
	{
		return false;
	}

	const auto &source = data->video->getSource();

	auto converter = data->converter.lock();
	if (!converter)
	{
		return false;
	}

	dim = data->getSize(data->con.get());
	if (!dim)
	{
		if (data->visible)
		{
			(*logger)(Logger::Level::Warning) << "Window '" << data->window.name << "' is not visible." << std::endl;
			data->visible = false;
		}
		return true;
	}

	xcb_get_image_cookie_t cookie = xcb_get_image(data->con.get(), XCB_IMAGE_FORMAT_Z_PIXMAP, data->window.windowId, 0,
												  0, dim->first, dim->second, ~0U);
	xcb_generic_error_t *errorPtr = nullptr;
	auto replyPtr = xcb_get_image_reply(data->con.get(), cookie, &errorPtr);
	auto error = makeXCB(errorPtr);
	auto reply = makeXCB(replyPtr);
	if (error || !reply)
	{
		if (data->visible)
		{
			(*logger)(Logger::Level::Warning) << "Window '" << data->window.name << "' is not visible." << std::endl;
			data->visible = false;
		}
		return true;
	}

	if (!data->visible)
	{
		*logger << "Window '" << data->window.name << "' is visible again." << std::endl;
		data->visible = true;
	}

	try
	{
		auto frame = tem_unique<VideoSource::Frame>();
		frame->width = dim->first;
		frame->height = dim->second;
		frame->format = SDL_PIXELFORMAT_BGRA32;
		uint8_t *data = xcb_get_image_data(reply.get());
		frame->bytes.append(data, frame->width * frame->height * 4);

		auto sourcePtr = tem_unique<Message::Source>(source);

		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::HandleFrame;
		e.user.data1 = frame.get();
		e.user.data2 = sourcePtr.get();
		if (tryPushEvent(e))
		{
			// Pointers are now owned by the event
			frame.release();
			sourcePtr.release();
		}

		auto s = tem_unique<X11Screenshot>();
		s->reply = std::move(reply);
		s->setWidth(dim->first);
		s->setHeight(dim->second);
		converter->addFrame(std::move(s));
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Level::Error) << "Ran out of memory" << std::endl;
		return false;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Level::Error) << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}
shared_ptr<VideoSource> VideoSource::recordWindow(const WindowProcess &wp, const Message::Source &source, FrameData fd)
{

	int s;
	auto con = getXCBConnection(s);
	auto size = getWindowSize(con.get(), wp.windowId);
	if (!size)
	{
		return nullptr;
	}
	fd.width = size->first;
	fd.height = size->second;
	auto video = tem_shared<VideoSource>(source, wp);
	if (fd.delay.has_value())
	{
		auto converter = tem_shared<Converter>(video, fd);
		auto screenshotter = tem_shared<Screenshotter>(std::move(con), wp, converter, video, fd.fps);
		Converter::startConverteringFrames(converter);
		Screenshotter::startTakingScreenshots(screenshotter);
	}
	else
	{
		auto encoder = tem_shared<FrameEncoder>(video, fd, false);
		FrameEncoder::startEncodingFrames(encoder);

		auto converter = tem_shared<Converter>(encoder, video, fd);

		auto screenshotter = tem_shared<Screenshotter>(std::move(con), wp, converter, video, fd.fps);
		Converter::startConverteringFrames(converter);
		Screenshotter::startTakingScreenshots(screenshotter);
	}
	return video;
}
} // namespace TemStream