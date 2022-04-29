#include <main.hpp>

namespace TemStream
{
Video::Video(const Message::Source &source) : source(source), windowProcress()
{
}
Video::Video(const Message::Source &source, const WindowProcess &wp) : source(source), windowProcress(wp)
{
}
Video::~Video()
{
}
void Video::logDroppedPackets(const size_t count, const Message::Source &source)
{
	(*logger)(Logger::Warning) << "Dropping " << count << " video frames from " << source << std::endl;
}
void handleVideoCapture(cv::VideoCapture &&cap, cv::Mat &&image, Video::FrameData fd, VideoCaptureArg &&arg,
						std::weak_ptr<Video> video, const Message::Source &source,
						std::weak_ptr<Video::FrameEncoder> encoder)
{
	(*logger) << "Starting video capture stream " << arg << std::endl;
	const uint32_t delay = 1000U / fd.fps;
	uint32_t last = SDL_GetTicks();
	cv::Mat yuv;
	while (!appDone && cap.isOpened())
	{
		const uint32_t now = SDL_GetTicks();
		const uint32_t diff = now - last;
		if (diff < delay)
		{
			SDL_Delay(diff);
			continue;
		}
		last = now;
		if (video.expired())
		{
			break;
		}
		if (!cap.read(image) || image.empty())
		{
			break;
		}

		// Need a better way that will always work
		switch (image.channels())
		{
		case 3:
			cv::cvtColor(image, yuv, cv::COLOR_BGR2YUV_IYUV);
			break;
		case 4:
			cv::cvtColor(image, yuv, cv::COLOR_BGRA2YUV_IYUV);
			break;
		default:
			(*logger)(Logger::Error) << "Unknown image type" << std::endl;
			goto end;
		}

		Video::Frame frame;
		frame.width = yuv.cols;
		frame.height = yuv.rows;
		frame.format = SDL_PIXELFORMAT_IYUV;
		frame.bytes.append(yuv.data, yuv.elemSize() * yuv.total());

		{
			auto ptr = allocateAndConstruct<Video::Frame>(frame);
			auto sourcePtr = allocateAndConstruct<Message::Source>(source);

			SDL_Event e;
			e.type = SDL_USEREVENT;
			e.user.code = TemStreamEvent::HandleFrame;
			e.user.data1 = ptr;
			e.user.data2 = sourcePtr;
			if (!tryPushEvent(e))
			{
				destroyAndDeallocate(ptr);
				destroyAndDeallocate(sourcePtr);
			}
		}
		if (auto e = encoder.lock())
		{
			e->addFrame(std::move(frame));
		}
		else
		{
			break;
		}
	}
end:
	(*logger) << "Stopping video capture stream " << arg << std::endl;
	stopVideoStream(source);
}
shared_ptr<Video> Video::recordWebcam(const VideoCaptureArg &arg, const Message::Source &source, const int32_t scale,
									  FrameData fd)
{
#if TEMSTREAM_USE_OPENCV
	cv::VideoCapture cap(std::visit(MakeVideoCapture{}, arg));
	if (!cap.isOpened())
	{
		return nullptr;
	}

	cv::Mat image;
	if (!cap.read(image) || image.empty())
	{
		return nullptr;
	}
	fd.width = image.cols;
	fd.height = image.rows;

	auto video = tem_shared<Video>(source);
	std::weak_ptr<FrameEncoder> encoder;
	{
		auto e = tem_shared<FrameEncoder>(source, scale);
		FrameEncoder::startEncodingFrames(e, fd, true);
		encoder = e;
	}

	WorkPool::workPool.addWork([cap = std::move(cap), image = std::move(image), fd, arg = std::move(arg), video, source,
								encoder]() mutable {
		handleVideoCapture(std::move(cap), std::move(image), fd, std::move(arg), std::weak_ptr(video), source, encoder);
	});
	return video;
#else
#endif
	return nullptr;
}
Video::FrameEncoder::FrameEncoder(const Message::Source &source, const int32_t ratio)
	: frames(), source(source), ratio(ratio)
{
}
Video::FrameEncoder::~FrameEncoder()
{
}
void Video::FrameEncoder::addFrame(Frame &&frame)
{
	frames.push(std::move(frame));
}
void Video::FrameEncoder::encodeFrames(shared_ptr<Video::FrameEncoder> ptr, FrameData frameData, const bool forCamera)
{
	struct RecordLog
	{
		Video::FrameEncoder &encoder;
		RecordLog(Video::FrameEncoder &encoder) : encoder(encoder)
		{
			(*logger)(Logger::Trace) << "Starting encoding thread: " << encoder.source << std::endl;
		}
		~RecordLog()
		{
			(*logger)(Logger::Trace) << "Ending encoding thread: " << encoder.source << std::endl;
			stopVideoStream(encoder.source);
		}
	};

	RecordLog recordLog(*ptr);

	unique_ptr<EncoderDecoder> encoder = nullptr;
	if (!TemStreamGui::sendCreateMessage<Message::Video>(ptr->source))
	{
		return;
	}

	frameData.width -= frameData.width % 2;
	frameData.width = frameData.width * ptr->ratio / 100;

	frameData.height -= frameData.height % 2;
	frameData.height = frameData.height * ptr->ratio / 100;
	encoder = createEncoder(frameData, forCamera);
	if (!encoder)
	{
		return;
	}

	auto lastReset = std::chrono::system_clock::now();
	const auto resetRate =
		std::chrono::duration<double, std::milli>((1000.0 / frameData.fps) * frameData.keyFrameInterval);
	using namespace std::chrono_literals;
	while (!appDone)
	{
		if (auto result = ptr->frames.clearIfGreaterThan(20))
		{
			logDroppedPackets(*result, ptr->source);
		}

		auto frame = ptr->frames.pop(3s);
		if (!frame)
		{
			break;
		}

		if (ptr->ratio != 100)
		{
			frame->resize(ptr->ratio);
		}

		const auto now = std::chrono::system_clock::now();
		auto size = encoder->getSize();
		if (frame->width != size->first || frame->height != size->second)
		{
			(*logger)(Logger::Trace) << "Resizing video encoder to " << frame->width << 'x' << frame->height
									 << std::endl;
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

		encoder->encodeAndSend(frame->bytes, ptr->source);
	}
}
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> ptr, FrameData frameData, const bool forCamera)
{
	WorkPool::workPool.addWork([ptr, frameData, forCamera]() { encodeFrames(ptr, frameData, forCamera); });
}
void Video::Frame::resizeTo(const uint32_t w, const uint32_t h)
{
#if TEMSTREAM_USE_OPENCV
	cv::Mat output;
	{
		cv::Mat m(height + height / 2U, width, CV_8UC1, bytes.data());
		cv::resize(m, output, cv::Size(), (double)w / (double)width, (double)h / (double)height, cv::INTER_LANCZOS4);
	}
	bytes = ByteList(output.data, output.total() * output.elemSize());
#else
	ByteList Y = resizePlane(bytes.data(), width, height, w, h);
	ByteList U = resizePlane(bytes.data() + (width * height), width / 2, height / 2, w / 2, h / 2);
	ByteList V = resizePlane(U + (width * height / 4), width / 2, height / 2, w / 2, h / 2);
	bytes = Y + U + V;
#endif
	width = w;
	height = h;
}
void Video::Frame::resize(uint32_t ratio)
{
	uint32_t w = width * ratio / 100;
	w -= w % 2;
	uint32_t h = height * ratio / 100;
	h -= h % 2;
	resizeTo(w, h);
}
} // namespace TemStream