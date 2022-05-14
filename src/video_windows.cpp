#include <main.hpp>

#include <video_windows.hpp>

namespace TemStream
{
WindowProcesses VideoSource::getRecordableWindows()
{
	return WindowProcesses();
}

std::optional<VideoSource::Frame> Converter::convertToFrame(Screenshot &&s)
{
	return std::nullopt;
}

Screenshotter::Screenshotter(const WindowProcess &w, shared_ptr<Converter> ptr, shared_ptr<VideoSource> v,
							 const uint32_t fps)
	: converter(ptr), window(w), video(v), fps(fps), visible(true), first(true)
{
}
Screenshotter::~Screenshotter()
{
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
bool Screenshotter::takeScreenshot(shared_ptr<Screenshotter>)
{
	return false;
}
shared_ptr<VideoSource> VideoSource::recordWindow(const WindowProcess &wp, const Message::Source &source, FrameData fd)
{
	return nullptr;
}
} // namespace TemStream