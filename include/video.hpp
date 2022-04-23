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

	const Message::Source &getSource() const
	{
		return source;
	}

	static WindowProcesses getRecordableWindows();
	static std::shared_ptr<Video> recordWindow(const WindowProcess &, const Message::Source &, const List<float> &,
											   const uint32_t fps);
};
} // namespace TemStream