#include <main.hpp>

namespace TemStream
{
Video::Video(const Message::Source &source, const WindowProcess &wp) : source(source), windowProcress(wp)
{
}
Video::~Video()
{
}
Video::FrameEncoder::FrameEncoder(const Message::Source &source, const float ratio) : frames(), source(), ratio(ratio)
{
	this->source.author = source.author;
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "%s (%d%%)", source.destination.c_str(), static_cast<int32_t>(ratio * 100.f));
	this->source.destination = buffer;
}
Video::FrameEncoder::~FrameEncoder()
{
}
void Video::FrameEncoder::addFrame(Frame frame)
{
	frames.push(frame);
}
void Video::FrameEncoder::encodeFrames(shared_ptr<Video::FrameEncoder> &&ptr)
{
	(*logger)(Logger::Trace) << "Starting encoding thread: " << ptr->source << std::endl;
	if (!TemStreamGui::sendCreateMessage<Message::Video>(ptr->source))
	{
		return;
	}
	using namespace std::chrono_literals;
	const auto maxWaitTime = 1s;
	while (!appDone)
	{
		auto data = ptr->frames.pop(maxWaitTime);
		if (!data)
		{
			break;
		}

		// TODO: Resize, encode, make and send packet
	}
	(*logger)(Logger::Trace) << "Ending encoding thread: " << ptr->source << std::endl;
}
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> &&ptr)
{
	std::thread thread(encodeFrames, std::move(ptr));
	thread.detach();
}
} // namespace TemStream