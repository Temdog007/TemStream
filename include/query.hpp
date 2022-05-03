#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class IQuery
{
  protected:
	Message::Source source;
	TemStreamGui &gui;

  public:
	IQuery(TemStreamGui &, const Message::Source &);
	virtual ~IQuery();

	virtual bool draw();

	virtual void execute() const = 0;

	const Message::Source &getSource() const
	{
		return source;
	}
};
class QueryText : public IQuery
{
  private:
	String text;

  public:
	QueryText(TemStreamGui &, const Message::Source &);
	QueryText(TemStreamGui &, const Message::Source &, String &&);
	~QueryText();

	bool draw() override;

	void setText(String &&s) noexcept
	{
		text.swap(s);
	}

	void execute() const override;
};
class QueryImage : public IQuery
{
  private:
	String image;

  public:
	QueryImage(TemStreamGui &, const Message::Source &);
	QueryImage(TemStreamGui &, const Message::Source &, const String &);
	~QueryImage();

	bool draw() override;

	void execute() const override;
};
class QueryAudio : public IQuery
{
  private:
	WindowProcesses windowNames;
	enum Source
	{
		Device,
		Window
	};
	Source source;
	int selected;

  public:
	QueryAudio(TemStreamGui &, const Message::Source &);
	~QueryAudio();

	bool draw() override;

	void execute() const override;
};
class QueryVideo : public IQuery
{
  private:
	struct WindowSelection
	{
		Video::FrameData frameData;
		WindowProcesses windows;
		int selected;
	};
	struct WebCamSelection
	{
		Video::FrameData frameData;
		VideoCaptureArg arg;
	};
	using VideoSelection = std::variant<WebCamSelection, WindowSelection>;
	VideoSelection selection;

  public:
	QueryVideo(TemStreamGui &, const Message::Source &);
	QueryVideo(TemStreamGui &, const Message::Source &, const String &);
	~QueryVideo();

	bool draw() override;

	void execute() const override;
};
} // namespace TemStream