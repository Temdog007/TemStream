#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class IQuery
{
  protected:
	String streamName;
	TemStreamGui &gui;

  public:
	IQuery(TemStreamGui &);
	virtual ~IQuery();

	virtual bool draw();

	virtual void execute() const = 0;

	Message::Source getSource() const;
};
class QueryText : public IQuery
{
  private:
	String text;

  public:
	QueryText(TemStreamGui &);
	QueryText(TemStreamGui &, String &&);
	~QueryText();

	bool draw() override;

	void execute() const override;
};
class QueryImage : public IQuery
{
  private:
	String image;

  public:
	QueryImage(TemStreamGui &);
	QueryImage(TemStreamGui &, const String &);
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
	QueryAudio(TemStreamGui &);
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
		int32_t scale;
		int selected;
	};
	struct WebCamSelection
	{
		Video::FrameData frameData;
		int index;
		int32_t scale;
	};
	using VideoSelection = std::variant<WebCamSelection, WindowSelection>;
	VideoSelection selection;

  public:
	QueryVideo(TemStreamGui &);
	~QueryVideo();

	bool draw() override;

	void execute() const override;
};
} // namespace TemStream