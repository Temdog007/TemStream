#pragma once

#include <main.hpp>

namespace TemStream
{
class Video
{
	friend class Allocator<Video>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

	friend class Deleter<Video>;

  private:
	Message::Source source;
	WindowProcess windowProcress;

	Video(const Message::Source &, const WindowProcess &);
	~Video();

  public:
	const WindowProcess &getInfo() const
	{
		return windowProcress;
	}

	static WindowProcesses getRecordableWindows();
	static std::shared_ptr<Video> recordWindow(WindowProcess &&, const Message::Source &, const std::vector<float> &,
											   const uint32_t fps);
};
} // namespace TemStream