#pragma once

#include <main.hpp>

namespace TemStream
{
struct Screenshot
{
	uint16_t width;
	uint16_t height;
};
class Converter : public VideoSource::RGBA2YUV<Screenshot>
{
  private:
	ByteList temp;

	std::optional<VideoSource::Frame> convertToFrame(Screenshot &&) override;

  public:
	Converter(std::shared_ptr<VideoSource::FrameEncoder> encoder, std::shared_ptr<VideoSource> video,
			  VideoSource::FrameData frameData)
		: VideoSource::RGBA2YUV<Screenshot>(encoder, video, frameData), temp()
	{
	}
	Converter(std::shared_ptr<VideoSource> video, VideoSource::FrameData frameData)
		: VideoSource::RGBA2YUV<Screenshot>(video, frameData), temp()
	{
	}
	~Converter()
	{
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