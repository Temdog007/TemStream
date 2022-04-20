#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
namespace Work
{
class CheckFile
{
  private:
	String filename;
	TemStreamGui &gui;

  public:
	CheckFile(const String &, TemStreamGui &);
	~CheckFile();

	void run() const;
};
class SendImage
{
  private:
	String filename;
	MessageSource source;

  public:
	SendImage(const String &, const MessageSource &);
	~SendImage();

	void run() const;
};
class LoadSurface
{
  private:
	MessageSource source;
	Bytes bytes;

  public:
	LoadSurface(const MessageSource &, Bytes &&);
	~LoadSurface();

	void run() const;
};
using Task = std::variant<CheckFile, SendImage, LoadSurface>;
} // namespace Work

class WorkQueue
{
  private:
	Deque<Work::Task> tasks;
	Mutex mutex;

  public:
	WorkQueue();
	~WorkQueue();

	void addWork(Work::Task &&);
	std::optional<Work::Task> getWork();

	template <typename T> void operator()(const T &t)
	{
		t.run();
	}
};
} // namespace TemStream