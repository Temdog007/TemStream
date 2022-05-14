#pragma once

#include <main.hpp>

#include <WinUser.h>

namespace TemStream
{
class WindowsScreenshot : public Screenshot
{
  public:
	ByteList bytes;
	
	uint8_t *getData() override
	{
		return bytes.data();
	}

	void getData(ByteList &dst) const
	{
		dst.append(bytes);
	}
};
class Screenshotter
{
  private:
	std::weak_ptr<Converter> converter;
	WindowProcess window;
	TimePoint nextFrame;
	std::shared_ptr<VideoSource> video;
	uint32_t fps;
	bool visible;
	bool first;

	static bool takeScreenshot(shared_ptr<Screenshotter>);

  public:
	Screenshotter(const WindowProcess &w, shared_ptr<Converter> ptr, shared_ptr<VideoSource> v, const uint32_t fps);
	~Screenshotter();

	static void startTakingScreenshots(shared_ptr<Screenshotter>);
};
} // namespace TemStream