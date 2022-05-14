#pragma once

#include <main.hpp>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

namespace TemStream
{
template <typename T> struct XCB_Deleter
{
	constexpr XCB_Deleter() noexcept = default;
	template <typename U, typename = std::_Require<std::is_convertible<U *, T *>>>
	XCB_Deleter(const XCB_Deleter<U> &) noexcept
	{
	}
	void operator()(T *t) const
	{
		if (t != nullptr)
		{
			free(t);
		}
	}
};

template <typename T> using xcb_ptr = std::unique_ptr<T, XCB_Deleter<T>>;

template <typename T> xcb_ptr<T> makeXCB(T *t)
{
	return xcb_ptr<T>(t, XCB_Deleter<T>());
}

struct XCB_ConnectionDeleter
{
	void operator()(xcb_connection_t *t) const
	{
		xcb_disconnect(t);
	}
};

using XCB_Connection = std::unique_ptr<xcb_connection_t, XCB_ConnectionDeleter>;

class X11Screenshot : public Screenshot
{
  public:
	xcb_ptr<xcb_get_image_reply_t> reply;

	uint8_t *getData() override
	{
		return xcb_get_image_data(reply.get());
	}
};

class Screenshotter
{
  private:
	std::weak_ptr<Converter> converter;
	WindowProcess window;
	TimePoint nextFrame;
	std::shared_ptr<VideoSource> video;
	XCB_Connection con;
	uint32_t fps;
	bool visible;
	bool first;

	Dimensions getSize(xcb_connection_t *);
	static bool takeScreenshot(shared_ptr<Screenshotter>);

  public:
	Screenshotter(XCB_Connection &&con, const WindowProcess &w, shared_ptr<Converter> ptr, shared_ptr<VideoSource> v,
				  const uint32_t fps);
	~Screenshotter();

	static void startTakingScreenshots(shared_ptr<Screenshotter>);
};
} // namespace TemStream