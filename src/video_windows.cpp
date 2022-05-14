#include <main.hpp>

#include <video_windows.hpp>

BOOL CALLBACK EnumWindowsCallback(HWND, LPARAM) noexcept;
TemStream::unique_ptr<TemStream::WindowsScreenshot> CaptureScreenshot(HWND);
BOOL GetWindowSize(HWND, uint32_t&, uint32_t&);

namespace TemStream
{
WindowProcesses VideoSource::getRecordableWindows()
{
	WindowProcesses procs;
	EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>( & procs));
	return procs;
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
		data->nextFrame = std::chrono::time_point_cast<std::chrono::milliseconds>(now + delay);
	}

	if (!data->video->isRunning())
	{
		return false;
	}

	auto ss = CaptureScreenshot(reinterpret_cast<HWND>(data->window.data));
	if (!ss)
	{
		if (data->visible)
		{
			(*logger)(Logger::Level::Warning) << "Window '" << data->window.name << "' is not visible" << std::endl;
			data->visible = false;
		}
		return true;
	}
	if (!data->visible)
	{
		*logger << "Window '" << data->window.name << "' is visible again" << std::endl;
		data->visible = true;
	}

	const auto &source = data->video->getSource();

	auto converter = data->converter.lock();
	if (!converter)
	{
		return false;
	}

	try
	{
		auto frame = tem_unique<VideoSource::Frame>();
		frame->width = ss->getWidth();
		frame->height = ss->getHeight();
		frame->format = SDL_PIXELFORMAT_BGRA32;
		ss->getData(frame->bytes);

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

		converter->addFrame(std::move(ss));
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

TemStream::unique_ptr<TemStream::WindowsScreenshot> CaptureScreenshot(HWND hwnd)
{
	using namespace TemStream;

	unique_ptr<WindowsScreenshot> rval = nullptr;

	auto hdcWindow = GetDC(hwnd);
	HDC hdcMemDC = nullptr;
	HBITMAP hbmWindow = nullptr;

	if (hdcWindow == nullptr)
	{
		goto done;
	}

	hdcMemDC = CreateCompatibleDC(hdcWindow);
	if (hdcMemDC == nullptr)
	{
		goto done;
	}

	RECT rcClient;
	if (GetClientRect(hwnd, &rcClient) != TRUE)
	{
		goto done;
	}

	hbmWindow = CreateCompatibleBitmap(hdcWindow, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
	if (hbmWindow == nullptr)
	{
		goto done;
	}

	SelectObject(hdcMemDC, hbmWindow);

	if (!BitBlt(hdcMemDC, 0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, hdcWindow, 0, 0,
				SRCCOPY))
	{
		goto done;
	}

	BITMAP bmpScreen;
	GetObject(hbmWindow, sizeof(BITMAP), &bmpScreen);

	if (bmpScreen.bmWidth < 16 || bmpScreen.bmHeight < 16)
	{
		goto done;
	}

	BITMAPINFOHEADER bi;
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bmpScreen.bmWidth;
	bi.biHeight = -bmpScreen.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	const DWORD dwBmpSize = ((bmpScreen.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;
	//const DWORD dwBmpSize = bmpScreen.bmWidth * bmpScreen.bmHeight * 4u;
	auto hDIB = GlobalAlloc(GHND, dwBmpSize);
	if (hDIB == nullptr)
	{
		goto done;
	}
	auto lpbitmap = (uint8_t*)GlobalLock(hDIB);
	GetDIBits(hdcWindow, hbmWindow, 0, (UINT)bmpScreen.bmHeight, lpbitmap, (BITMAPINFO *)&bi, DIB_RGB_COLORS);

	rval = tem_unique<WindowsScreenshot>();
	rval->setWidth(static_cast<uint16_t>(bmpScreen.bmWidth));
	rval->setHeight(static_cast<uint16_t>(bmpScreen.bmHeight));
	rval->bytes.append(lpbitmap, dwBmpSize);

	/*auto hFile = CreateFileA("captureqwsx.bmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	BITMAPFILEHEADER bmfHeader;
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
	bmfHeader.bfSize = dwSizeofDIB;
	bmfHeader.bfType = 0x4D42;

	DWORD dwBytesWritten;
	WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
	WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
	WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);
	CloseHandle(hFile);*/

	GlobalUnlock(hDIB);
	GlobalFree(hDIB);

done:
	if (hdcMemDC != nullptr)
	{
		DeleteObject(hdcMemDC);
	}
	if (hbmWindow != nullptr)
	{
		DeleteObject(hbmWindow);
	}
	if (hdcWindow != nullptr)
	{
		ReleaseDC(hwnd, hdcWindow);
	}
	return rval;
}