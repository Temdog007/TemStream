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
	Message::Source source;

  public:
	SendImage(const String &, const Message::Source &);
	~SendImage();

	void run() const;
};
class LoadSurface
{
  private:
	Message::Source source;
	Bytes bytes;

  public:
	LoadSurface(const Message::Source &, Bytes &&);
	~LoadSurface();

	void run() const;
};
class StartPlayback
{
  private:
	Message::Source source;
	std::optional<String> name;
	float volume;

  public:
	StartPlayback(const Message::Source &, const std::optional<String> &, float);
	~StartPlayback();

	void run() const;
};
class StartRecording
{
  private:
	Message::Source source;
	std::optional<String> name;
	float silenceThreshold;

  public:
	StartRecording(const Message::Source &, const std::optional<String> &, float);
	~StartRecording();

	void run() const;
};
class StartWindowAudioRecording
{
  private:
	Message::Source source;
	WindowProcess windowProcess;
	float silenceThreshold;

  public:
	StartWindowAudioRecording(const Message::Source &, const WindowProcess &, float);
	~StartWindowAudioRecording();

	void run() const;
};
using Task = std::variant<CheckFile, SendImage, LoadSurface, StartPlayback, StartRecording, StartWindowAudioRecording>;
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