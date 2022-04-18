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

	static void getPackets(String, MessageSource);

  public:
	QueryImage(TemStreamGui &);
	QueryImage(TemStreamGui &, String &&);
	~QueryImage();

	bool draw() override;

	void execute() const override;
};
class QueryAudio : public IQuery
{
  public:
	QueryAudio(TemStreamGui &);
	~QueryAudio();

	bool draw() override;

	void execute() const override;
};
class QueryVideo : public IQuery
{
  public:
	QueryVideo(TemStreamGui &);
	~QueryVideo();

	bool draw() override;

	void execute() const override;
};
} // namespace TemStream