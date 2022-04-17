#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class IQuery
{
  protected:
	std::string streamName;
	TemStreamGui &gui;

  public:
	IQuery(TemStreamGui &);
	virtual ~IQuery();

	virtual bool draw();
	virtual bool handleDropFile(const char *);
	virtual bool handleDropText(const char *);

	virtual MessagePackets getPackets() const = 0;
};
class QueryText : public IQuery
{
  private:
	std::string text;

  public:
	QueryText(TemStreamGui &);
	~QueryText();

	bool draw() override;
	virtual bool handleDropFile(const char *) override;
	virtual bool handleDropText(const char *) override;

	MessagePackets getPackets() const override;
};
class QueryImage : public IQuery
{
  private:
	std::string image;

  public:
	QueryImage(TemStreamGui &);
	~QueryImage();

	bool draw() override;
	virtual bool handleDropFile(const char *) override;

	MessagePackets getPackets() const override;

  private:
	bool readChunkFromFile(FILE *, MessagePackets &) const;
};
class QueryAudio : public IQuery
{
  public:
	QueryAudio(TemStreamGui &);
	~QueryAudio();

	bool draw() override;

	MessagePackets getPackets() const override;
};
class QueryVideo : public IQuery
{
  public:
	QueryVideo(TemStreamGui &);
	~QueryVideo();

	bool draw() override;

	MessagePackets getPackets() const override;
};
} // namespace TemStream