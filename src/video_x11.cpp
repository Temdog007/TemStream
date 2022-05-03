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

std::optional<Video::Frame> Converter::convertToFrame(Screenshot &&s)
{
	uint8_t *data = xcb_get_image_data(s.reply.get());

	Video::Frame frame;
	frame.width = (s.width - (s.width % 2));
	frame.height = (s.height - (s.height % 2));

#if TEMSTREAM_USE_OPENCV
	// Must copy in case other scalings use this image
	temp.clear();
	temp.append(data, s.width * s.height * 4);
	cv::Mat m(frame.height, frame.width, CV_8UC4, temp.data());
	cv::cvtColor(m, m, cv::COLOR_BGRA2YUV_IYUV);
	frame.bytes.append(m.data, m.total() * m.elemSize());
	return frame;
#else
	frame->bytes.resize((s.width * s.height) * 2, '\0');
	if (SDL_ConvertPixels(s.width, s.height, SDL_PIXELFORMAT_BGRA32, data, s.width * 4, SDL_PIXELFORMAT_YV12,
						  frame->bytes.data(), s.width) == 0)
	{
		return frame;
	}
	else
	{
		logSDLError("Failed to convert pixels");
		return nullptr;
	}
#endif
}
Screenshotter::Screenshotter(XCB_Connection &&con, const WindowProcess &w, shared_ptr<Converter> ptr,
							 shared_ptr<Video> v, const uint32_t fps)
	: converter(ptr), window(w), video(v), con(std::move(con)), fps(fps), first(true)
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
	WorkPool::workPool.addWork([ss]() {
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
		(*logger)(Logger::Error) << "Failed to get size of window: " << data->window.name << std::endl;
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
		(*logger)(Logger::Error) << "Window '" << data->window.name << "' is not visible. Ending stream" << std::endl;
		return false;
	}

	xcb_get_image_cookie_t cookie = xcb_get_image(data->con.get(), XCB_IMAGE_FORMAT_Z_PIXMAP, data->window.windowId, 0,
												  0, dim->first, dim->second, ~0U);
	xcb_generic_error_t *errorPtr = nullptr;
	auto replyPtr = xcb_get_image_reply(data->con.get(), cookie, &errorPtr);
	auto error = makeXCB(errorPtr);
	auto reply = makeXCB(replyPtr);
	if (error || !reply)
	{
		(*logger)(Logger::Error) << "Window '" << data->window.name << "' is not visible. Ending stream" << std::endl;
		return false;
	}

	try
	{
		auto frame = tem_unique<Video::Frame>();
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

		Screenshot s;
		s.reply = std::move(reply);
		s.width = dim->first;
		s.height = dim->second;
		converter->addFrame(std::move(s));
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
		return false;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}
bool Converter::handleWriter(Video::Writer &w)
{
#if TEMSTREAM_USE_OPENCV
	if (first)
	{
		if (!TemStreamGui::sendCreateMessage<Message::Video>(video->getSource()) ||
			!Video::resetVideo(w, video, frameData))
		{
			return false;
		}

		*logger << "Starting video converter: " << video->getSource() << std::endl;
		first = false;
	}
	if (!video->isRunning())
	{
		return false;
	}
	try
	{
		using namespace std::chrono_literals;
		while (!appDone)
		{
			auto result = frames.clearIfGreaterThan(Video::MaxVideoPackets);
			if (result)
			{
				Video::logDroppedPackets(*result, video->getSource(), "Video writer");
			}

			auto data = frames.pop(0s);
			if (!data)
			{
				return true;
			}

			{
				if (frameData.width != data->width || frameData.height != data->height)
				{
					frameData.width = data->width;
					frameData.height = data->height;
					if (!Video::resetVideo(w, video, frameData))
					{
						return false;
					}
					continue;
				}
				cv::Mat image(data->height, data->width, CV_8UC4, xcb_get_image_data(data->reply.get()));
				{
					cv::Mat dst;
					cv::cvtColor(image, dst, cv::COLOR_BGRA2BGR);
					image = std::move(dst);
				}
				cv::Mat output;
				if (frameData.scale == 100)
				{
					output = std::move(image);
				}
				else
				{
					const double scale = frameData.scale / 100.0;
					cv::resize(image, output, cv::Size(), scale, scale, cv::InterpolationFlags::INTER_CUBIC);
				}
				*w.writer << output;
				++w.framesWritten;
				// (*logger)(Logger::Trace) << "JPEG size: " << jpegBytes.size() << std::endl;
			}

			if (w.framesWritten < frameData.fps * (*frameData.delay))
			{
				continue;
			}

			shared_ptr<cv::VideoWriter> oldVideo = tem_shared<cv::VideoWriter>();
			oldVideo.swap(w.writer);
			WorkPool::workPool.addWork(
				[oldFilename = w.filename, video = this->video, oldVideo = std::move(oldVideo)]() mutable {
					try
					{
						oldVideo->release();
						oldVideo.reset();

						const auto fileSize = std::filesystem::file_size(oldFilename);
						ByteList bytes(fileSize);
						{
							std::ifstream file(oldFilename.c_str(), std::ios::in | std::ios::binary);
							// Stop eating new lines in binary mode!!!
							file.unsetf(std::ios::skipws);

							(*logger)(Logger::Trace) << "Saving file of size " << printMemory(fileSize) << std::endl;

							// reserve capacity
							bytes.reallocate(fileSize);
							bytes.append(std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
							if (fileSize != bytes.size())
							{
								(*logger)(Logger::Warning) << "Failed to write entire video file. Wrote "
														   << bytes.size() << ". Expected " << fileSize << std::endl;
							}
						}

						auto packets = allocateAndConstruct<MessagePackets>();

						Message::prepareLargeBytes(bytes,
												   [&packets, &source = video->getSource()](Message::LargeFile &&lf) {
													   Message::Packet packet;
													   packet.source = source;
													   packet.payload.emplace<Message::Video>(std::move(lf));
													   packets->emplace_back(std::move(packet));
												   });

						SDL_Event e;
						e.type = SDL_USEREVENT;
						e.user.code = TemStreamEvent::SendMessagePackets;
						e.user.data1 = packets;
						e.user.data2 = nullptr;
						if (!tryPushEvent(e))
						{
							destroyAndDeallocate(packets);
						}

						std::filesystem::remove(oldFilename);
					}
					catch (const std::bad_alloc &)
					{
						(*logger)(Logger::Error) << "Ran out of memory saving video file" << std::endl;
					}
					catch (const std::exception &e)
					{
						(*logger)(Logger::Error) << e.what() << std::endl;
					}
					return false;
				});
			++w.vidsWritten;
			w.framesWritten = 0;
			if (!Video::resetVideo(w, video, frameData))
			{
				return false;
			}
		}
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
		return false;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Convert thread error: " << e.what() << std::endl;
		return false;
	}
	return true;
#else
	return false;
#endif
}
shared_ptr<Video> Video::recordWindow(const WindowProcess &wp, const Message::Source &source, FrameData fd)
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
	auto video = tem_shared<Video>(source, wp);
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