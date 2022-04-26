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
	printf("image %d\n", image.channels());
	fd.width = image.cols;
	fd.height = image.rows;

	auto video = tem_shared<Video>(source);
	std::weak_ptr<FrameEncoder> encoder;
	{
		auto e = tem_shared<FrameEncoder>(source, scale);
		FrameEncoder::startEncodingFrames(e, fd);
		encoder = e;
	}
	auto weak = std::weak_ptr(video);

	Task::addTask(std::async(
		TaskPolicy, [cap = std::move(cap), image = std::move(image), weak, source, scale, fd, encoder]() mutable {
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
				if (weak.expired())
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
					return;
				}

				Video::Frame frame;
				frame.width = image.cols;
				frame.height = image.rows;
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
		}));
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
void Video::FrameEncoder::encodeFrames(shared_ptr<Video::FrameEncoder> ptr, FrameData frameData)
{
	(*logger)(Logger::Trace) << "Starting encoding thread: " << ptr->source << std::endl;
	if (!TemStreamGui::sendCreateMessage<Message::Video>(ptr->source))
	{
		return;
	}

	frameData.width -= frameData.width % 2;
	frameData.width = frameData.width * ptr->ratio / 100;

	frameData.height -= frameData.height % 2;
	frameData.height = frameData.height * ptr->ratio / 100;
	auto encoder = createEncoder(frameData);
	if (!encoder)
	{
		return;
	}

	using namespace std::chrono_literals;
	const auto maxWaitTime = 3s;
	while (!appDone)
	{
		if (auto result = ptr->frames.clearIfGreaterThan(5))
		{
			logDroppedPackets(*result, ptr->source);
		}

		auto frame = ptr->frames.pop(maxWaitTime);
		if (!frame)
		{
			break;
		}

		if (ptr->ratio != 100)
		{
			frame->resize(ptr->ratio);
		}

		{
			auto size = encoder->getSize();
			if (frame->width != size->first || frame->height != size->second)
			{
				(*logger)(Logger::Trace) << "Resizing vidoe encoder to " << frame->width << 'x' << frame->height
										 << std::endl;
				FrameData fd = frameData;
				fd.width = frame->width;
				fd.height = frame->height;
				auto newVpx = createEncoder(fd);
				if (newVpx)
				{
					encoder.swap(newVpx);
				}
			}
		}
		encoder->encodeAndSend(frame->bytes, ptr->source);
	}
	(*logger)(Logger::Trace) << "Ending encoding thread: " << ptr->source << std::endl;
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::StopVideoStream;
	auto source = allocateAndConstruct<Message::Source>(ptr->source);
	e.user.data1 = source;
	if (!tryPushEvent(e))
	{
		destroyAndDeallocate(source);
	}
}
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> ptr, FrameData frameData)
{
	Task::addTask(std::async(TaskPolicy, encodeFrames, ptr, frameData));
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