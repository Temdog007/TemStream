#include <main.hpp>

namespace TemStream
{
const char *VideoExtension = ".mkv";
const size_t VideoSource::MaxVideoPackets = 20;
VideoSource::VideoSource(const Message::Source &source, String &&name)
	: source(source), windowProcress(), name(std::move(name)), running(true)
{
}
VideoSource::VideoSource(const Message::Source &source, const WindowProcess &wp)
	: source(source), windowProcress(wp), name(wp.name), running(true)
{
}
VideoSource::VideoSource(const Message::Source &source, const Address &address)
	: source(source), windowProcress(), name(), running(true)
{
	StringStream ss;
	ss << address;
	name = ss.str();
}
VideoSource::~VideoSource()
{
}
void VideoSource::logDroppedPackets(const size_t count, const Message::Source &source, const char *target)
{
	(*logger)(Logger::Level::Warning) << target << " is dropping " << count << " video frames from "
									  << source.serverName << std::endl;
}
bool WebCamCapture::execute()
{
	if (first)
	{
		*logger << "Starting webcam recording: " << arg << "; FPS:" << frameData.fps << std::endl;
		first = false;
	}
	if (!cap.isOpened())
	{
		return false;
	}

	if (!video->isRunning())
	{
		return false;
	}

	const auto now = std::chrono::system_clock::now();

	if (now < nextFrame)
	{
		return true;
	}

	const auto delay = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::duration<double, std::milli>(1000.0 / frameData.fps));

	if (!cap.read(image) || image.empty())
	{
		return false;
	}

	{
		auto frame = allocateAndConstruct<VideoSource::Frame>();
		frame->width = image.cols;
		frame->height = image.rows;
		frame->format = SDL_PIXELFORMAT_BGR24;
		frame->bytes.append(image.data, static_cast<uint32_t>(image.elemSize() * image.total()));

		auto sourcePtr = allocateAndConstruct<Message::Source>(source);

		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::HandleFrame;
		e.user.data1 = frame;
		e.user.data2 = sourcePtr;
		if (!tryPushEvent(e))
		{
			destroyAndDeallocate(frame);
			destroyAndDeallocate(sourcePtr);
		}
	}

	if (auto e = encoder.lock())
	{
		cv::Mat yuv;
		cv::cvtColor(image, yuv, cv::COLOR_BGR2YUV_IYUV);
		VideoSource::Frame frame;
		frame.width = image.cols;
		frame.height = image.rows;
		frame.bytes.append(yuv.data, static_cast<uint32_t>(yuv.elemSize() * yuv.total()));
		e->addFrame(std::move(frame));
		nextFrame = std::chrono::time_point_cast<std::chrono::milliseconds>(now + delay);
		return true;
	}

	return false;
}
shared_ptr<VideoSource> VideoSource::recordWebcam(const VideoCaptureArg &arg, const Message::Source &source,
												  FrameData fd)
{
	cv::VideoCapture cap(std::visit(MakeVideoCapture{}, arg));
	if (!cap.isOpened())
	{
		(*logger)(Logger::Level::Error) << "Failed to start video capture" << std::endl;
		return nullptr;
	}

	cv::Mat image;
	if (!cap.read(image) || image.empty())
	{
		(*logger)(Logger::Level::Error) << "Failed to read initial image from video capture" << std::endl;
		return nullptr;
	}
	fd.width = image.cols;
	fd.height = image.rows;
	fd.fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));

	struct Foo
	{
		String operator()(const String &name)
		{
			return name;
		}
		String operator()(const int32_t i)
		{
			return String(std::to_string(i));
		}
	};
	auto name = std::visit(Foo(), arg);
	auto video = tem_shared<VideoSource>(source, std::move(name));
	std::weak_ptr<FrameEncoder> encoder;
	{
		auto e = tem_shared<FrameEncoder>(video, fd, true);
		FrameEncoder::startEncodingFrames(e);
		encoder = e;
	}

	auto capture = tem_shared<WebCamCapture>();
	capture->cap = std::move(cap);
	capture->image = std::move(image);
	capture->frameData = std::move(fd);
	capture->video = video;
	capture->encoder = encoder;
	capture->source = source;
	capture->first = true;

	WorkPool::workPool.addWork([capture = std::move(capture)]() mutable {
		if (!capture->execute())
		{
			*logger << "Ending webcam recording: " << capture->arg << std::endl;
			capture->video->setRunning(false);
			return false;
		}
		return true;
	});
	return video;
}
shared_ptr<VideoSource> VideoSource::listenToUdpPort(const Address &address, const Message::Source &source)
{
	auto ptr = address.create<UdpSocket>();
	if (ptr == nullptr)
	{
		return nullptr;
	}

	shared_ptr<UdpSocket> udp = std::move(ptr);

	auto video = tem_shared<VideoSource>(source, address);
	(*logger)(Logger::Level::Trace) << "Listening to port: " << address << std::endl;
	WorkPool::workPool.addWork([udp, video]() {
		if (!video->isRunning())
		{
			return false;
		}
		ByteList bytes;
		if (!udp->read(0, bytes, false))
		{
			video->setRunning(false);
			return false;
		}

		if (bytes.empty())
		{
			return true;
		}

		auto packet = allocateAndConstruct<Message::Packet>();
		packet->source = video->getSource();
		Message::Frame frame{};
		frame.bytes = std::move(bytes);
		Message::Video v;
		v.emplace<Message::Frame>(std::move(frame));
		packet->payload.emplace<Message::Video>(std::move(v));

		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::SendSingleMessagePacket;
		e.user.data1 = packet;
		e.user.data2 = &e;
		if (!tryPushEvent(e))
		{
			destroyAndDeallocate(packet);
		}
		return true;
	});
	return video;
}
VideoSource::FrameEncoder::FrameEncoder(shared_ptr<VideoSource> v, const FrameData frameData, const bool forCamera)
	: frames(), frameData(frameData), lastReset(std::chrono::system_clock::now()), encoder(nullptr), video(v),
	  first(true)
{
	this->frameData.width -= this->frameData.width % 2;
	this->frameData.width = this->frameData.width * frameData.scale / 100;

	this->frameData.height -= this->frameData.height % 2;
	this->frameData.height = this->frameData.height * frameData.scale / 100;

	encoder = createEncoder(frameData, forCamera);
}
VideoSource::FrameEncoder::~FrameEncoder()
{
}
void VideoSource::FrameEncoder::addFrame(Frame &&frame)
{
	frames.push(std::move(frame));
}
bool VideoSource::FrameEncoder::encodeFrames()
{
	if (first)
	{
		*logger << "Starting encoding: " << video->getSource() << std::endl;
		first = false;
	}
	if (!video->isRunning())
	{
		return false;
	}
	if (!encoder)
	{
		return false;
	}

	while (!appDone)
	{
		if (auto result = frames.clearIfGreaterThan(MaxVideoPackets))
		{
#if TEMSTREAM_USE_OPENH264
			static const char *encoderName = "OpenH264 encoder";
#else
			static const char *encoderName = "VPX encoder";
#endif
			logDroppedPackets(*result, video->getSource(), encoderName);
		}

		using namespace std::chrono_literals;
		auto frame = frames.pop(0s);
		if (!frame)
		{
			return true;
		}

		if (frameData.scale != 100)
		{
			frame->resize(frameData.scale);
		}

		const auto now = std::chrono::system_clock::now();
		auto size = encoder->getSize();
		if (frame->width != size->first || frame->height != size->second)
		{
			(*logger)(Logger::Level::Trace)
				<< "Resizing video encoder to " << frame->width << 'x' << frame->height << std::endl;
			FrameData fd = frameData;
			fd.width = frame->width;
			fd.height = frame->height;
			auto newVpx = createEncoder(fd);
			if (newVpx)
			{
				encoder.swap(newVpx);
				lastReset = now;
			}
		}

		const auto resetRate =
			std::chrono::duration<double, std::milli>((1000.0 / frameData.fps) * frameData.keyFrameInterval);
		if (now - lastReset > resetRate)
		{
			FrameData fd = frameData;
			fd.width = frame->width;
			fd.height = frame->height;
			auto newVpx = createEncoder(fd);
			if (newVpx)
			{
				encoder.swap(newVpx);
				lastReset = now;
			}
		}

		encoder->encodeAndSend(frame->bytes, video->getSource());
	}
	return true;
}
void VideoSource::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> ptr)
{
	WorkPool::workPool.addWork([ptr]() {
		if (!ptr->encodeFrames())
		{
			(*logger) << "Ending encoding: " << ptr->video->getSource().serverName << std::endl;
			return false;
		}
		return true;
	});
}
void VideoSource::Frame::resizeTo(const uint32_t w, const uint32_t h)
{
	cv::Mat output;
	{
		cv::Mat m(height + height / 2U, width, CV_8UC1, bytes.data());
		cv::resize(m, output, cv::Size(), (double)w / (double)width, (double)h / (double)height,
				   cv::InterpolationFlags::INTER_AREA);
	}
	bytes = ByteList(output.data, static_cast<uint32_t>(output.total() * output.elemSize()));

	width = w;
	height = h;
}
void VideoSource::Frame::resize(uint32_t ratio)
{
	uint32_t w = width * ratio / 100;
	w -= w % 2;
	uint32_t h = height * ratio / 100;
	h -= h % 2;
	resizeTo(w, h);
}
VideoSource::FrameData::FrameData()
	: width(320), height(240), delay(std::nullopt), fps(24), bitrateInMbps(10), keyFrameInterval(300), scale(100)
{
}
VideoSource::FrameData::~FrameData()
{
}
VideoSource::Writer::Writer() : filename(), writer(), vidsWritten(0), framesWritten(0)
{
}
VideoSource::Writer::~Writer()
{
}
bool VideoSource::resetVideo(VideoSource::Writer &w, shared_ptr<VideoSource> video, FrameData frameData)
{
	StringStream ss;
	ss << video->getSource() << "_" << w.vidsWritten << VideoExtension;
	w.filename = cv::String(ss.str());
	if (w.writer == nullptr)
	{
		w.writer = tem_shared<cv::VideoWriter>(
			w.filename, VideoSource::getFourcc(), frameData.fps,
			cv::Size(frameData.width * frameData.scale / 100u, frameData.height * frameData.scale / 100u));
		if (!w.writer->isOpened())
		{
			(*logger)(Logger::Level::Error) << "Failed to create new video" << std::endl;
			return false;
		}
	}
	else if (!w.writer->open(
				 w.filename, VideoSource::getFourcc(), frameData.fps,
				 cv::Size(frameData.width * frameData.scale / 100u, frameData.height * frameData.scale / 100u)))
	{
		(*logger)(Logger::Level::Error) << "Failed to create new video" << std::endl;
		return false;
	}
	return true;
}
int VideoSource::getFourcc()
{
	return cv::VideoWriter::fourcc('H', '2', '6', '4');
}
} // namespace TemStream