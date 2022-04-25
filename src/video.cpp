#include <main.hpp>

namespace TemStream
{
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
Video::FrameEncoder::FrameEncoder(const Message::Source &source, const int32_t ratio) : frames(), source(), ratio(ratio)
{
	this->source.author = source.author;
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "%s (%d%%)", source.destination.c_str(), ratio);
	this->source.destination = buffer;
}
Video::FrameEncoder::~FrameEncoder()
{
}
void Video::FrameEncoder::addFrame(shared_ptr<Frame> frame)
{
	frames.push(frame);
}

void Video::FrameEncoder::encodeFrames(shared_ptr<Video::FrameEncoder> &&ptr, FrameData frameData)
{
	(*logger)(Logger::Trace) << "Starting encoding thread: " << ptr->source << std::endl;
	if (!TemStreamGui::sendCreateMessage<Message::Video>(ptr->source))
	{
		return;
	}

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

		auto result = ptr->frames.pop(maxWaitTime);
		if (!result)
		{
			break;
		}

		auto data = *result;
		if (!data)
		{
			continue;
		}

		if (ptr->ratio != 100)
		{
			data->resize(ptr->ratio);
		}

		{
			auto size = encoder->getSize();
			if (data->width != size->first || data->height != size->second)
			{
				FrameData fd = frameData;
				fd.width = data->width;
				fd.height = data->height;
				auto newVpx = createEncoder(fd);
				if (newVpx)
				{
					encoder.swap(newVpx);
				}
			}
		}
		encoder->encodeAndSend(data->bytes, ptr->source);
	}
	(*logger)(Logger::Trace) << "Ending encoding thread: " << ptr->source << std::endl;
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::StopVideoStream;
	auto source = allocate<Message::Source>(ptr->source);
	e.user.data1 = source;
	if (!tryPushEvent(e))
	{
		deallocate(source);
	}
}
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> &&ptr, FrameData frameData)
{
	Task::addTask(std::async(TaskPolicy, encodeFrames, std::move(ptr), frameData));
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
	ByteList V = resizePlane(bytes.data() + (width * height * 5 / 4), width / 2, height / 2, w / 2, h / 2);
	bytes = Y + U + V;
#endif
	width = w;
	height = h;
}
void Video::Frame::resize(uint32_t ratio)
{
	resizeTo(width * ratio / 100, height * ratio / 100);
}
} // namespace TemStream