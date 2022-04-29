#include <main.hpp>

namespace TemStream
{
Video::Video(const Message::Source &source) : source(source), windowProcress(), running(true)
{
}
Video::Video(const Message::Source &source, const WindowProcess &wp) : source(source), windowProcress(wp), running(true)
{
}
Video::~Video()
{
}
void Video::logDroppedPackets(const size_t count, const Message::Source &source)
{
	(*logger)(Logger::Warning) << "Dropping " << count << " video frames from " << source << std::endl;
}
bool WebCamCapture::execute()
{
	if (first)
	{
		*logger << "Ending webcam recording: " << arg << std::endl;
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

	cv::Mat yuv;

	if (!cap.read(image) || image.empty())
	{
		return false;
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
		return false;
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
		nextFrame = now + delay;
		return true;
	}

	return false;
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
		auto e = tem_shared<FrameEncoder>(video, scale, fd, true);
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
#else
	return nullptr;
#endif
}
Video::FrameEncoder::FrameEncoder(shared_ptr<Video> v, const int32_t ratio, const FrameData frameData,
								  const bool forCamera)
	: frames(), frameData(frameData), lastReset(std::chrono::system_clock::now()), encoder(nullptr), video(v),
	  ratio(ratio), first(true)
{
	TemStreamGui::sendCreateMessage<Message::Video>(v->getSource());

	this->frameData.width -= this->frameData.width % 2;
	this->frameData.width = this->frameData.width * ratio / 100;

	this->frameData.height -= this->frameData.height % 2;
	this->frameData.height = this->frameData.height * ratio / 100;

	encoder = createEncoder(frameData, forCamera);
}
Video::FrameEncoder::~FrameEncoder()
{
}
void Video::FrameEncoder::addFrame(Frame &&frame)
{
	frames.push(std::move(frame));
}
bool Video::FrameEncoder::encodeFrames()
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

	while (true)
	{
		if (auto result = frames.clearIfGreaterThan(20))
		{
			logDroppedPackets(*result, video->getSource());
		}

		using namespace std::chrono_literals;
		auto frame = frames.pop(0s);
		if (!frame)
		{
			return true;
		}

		if (ratio != 100)
		{
			frame->resize(ratio);
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
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> ptr)
{
	WorkPool::workPool.addWork([ptr]() {
		if (!ptr->encodeFrames())
		{
			(*logger) << "Ending encoding: " << ptr->video->getSource() << std::endl;
			return false;
		}
		return true;
	});
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