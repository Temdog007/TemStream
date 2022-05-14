#include <main.hpp>

#include <video_windows.hpp>

BOOL CALLBACK EnumWindowsCallback(HWND, LPARAM) noexcept;
BOOL CaptureImage(HWND);
BOOL GetWindowSize(HWND, uint32_t&, uint32_t&);

namespace TemStream
{
WindowProcesses VideoSource::getRecordableWindows()
{
	WindowProcesses procs;
	EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>( & procs));
	return procs;
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
	if (!GetWindowSize(reinterpret_cast<HWND>(wp.data), fd.width, fd.height))
	{
		return nullptr;
	}
	auto video = tem_shared<VideoSource>(source, wp);
	if (fd.delay.has_value())
	{
		auto converter = tem_shared<Converter>(video, fd);
		auto screenshotter = tem_shared<Screenshotter>(wp, converter, video, fd.fps);
		Converter::startConverteringFrames(converter);
		Screenshotter::startTakingScreenshots(screenshotter);
	}
	else
	{
		auto encoder = tem_shared<FrameEncoder>(video, fd, false);
		FrameEncoder::startEncodingFrames(encoder);

		auto converter = tem_shared<Converter>(encoder, video, fd);

		auto screenshotter = tem_shared<Screenshotter>(wp, converter, video, fd.fps);
		Converter::startConverteringFrames(converter);
		Screenshotter::startTakingScreenshots(screenshotter);
	}
	return video;
}
} // namespace TemStream

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) noexcept
{
	using namespace TemStream;
	CHAR windowTitle[1024];

	GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));

	const int length = ::GetWindowTextLengthA(hwnd);

	String title(&windowTitle[0]);
	if (!IsWindowVisible(hwnd) || length == 0 || title == "Program Manager")
	{
		return TRUE;
	}

	auto procs = reinterpret_cast<WindowProcesses *>(lParam);
	WindowProcess proc;
	proc.id = GetWindowThreadProcessId(hwnd, nullptr);
	proc.name = std::move(title);
	proc.data = hwnd;
	procs->emplace(std::move(proc));
	return TRUE;
}

BOOL GetWindowSize(HWND hwnd, uint32_t& width, uint32_t& height)
{
	RECT rect;
	if (GetClientRect(hwnd, &rect) == TRUE)
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
		return TRUE;
	}
	return FALSE;
}