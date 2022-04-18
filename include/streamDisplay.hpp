#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
struct Texture
{
	enum Mode
	{
		None,
		Move,
		Resize
	};
	SDL_Rect rect;
	SDL_Texture *texture;
	Mode mode;

	Texture(SDL_Texture *texture, const int w, const int h) : rect(), texture(texture), mode(Mode::None)
	{
		rect.x = 0;
		rect.y = 0;
		rect.w = w;
		rect.h = h;
	}
	Texture(const Texture &) = delete;
	Texture(Texture &&t) : rect(t.rect), texture(t.texture), mode(Mode::None)
	{
		t.texture = nullptr;
	}
	~Texture()
	{
		SDL_DestroyTexture(texture);
		texture = nullptr;
	}

	Texture &operator=(const Texture &) = delete;
	Texture &operator=(Texture &&t)
	{
		rect = t.rect;
		texture = t.texture;
		mode = t.mode;
		t.texture = nullptr;
		return *this;
	}
};

using DisplayData = std::variant<std::monostate, Texture, String, Bytes>;
class StreamDisplay : public MessagePacketHandler
{
  private:
	MessageSource source;
	DisplayData data;
	TemStreamGui &gui;
	bool visible;

	friend class TemStreamGui;
	friend class StreamDisplayUpdate;
	friend class StreamDisplayDraw;

  public:
	StreamDisplay() = delete;
	StreamDisplay(TemStreamGui &, const MessageSource &);
	StreamDisplay(const StreamDisplay &) = delete;
	StreamDisplay(StreamDisplay &&);
	virtual ~StreamDisplay();

	StreamDisplay &operator=(const StreamDisplay &) = delete;
	StreamDisplay &operator=(StreamDisplay &&) = delete;

	const MessageSource &getSource() const
	{
		return source;
	}

	void handleEvent(const SDL_Event &);
	bool draw(bool usingUi);

	bool operator()(const TextMessage &);
	bool operator()(const ImageMessage &);
	bool operator()(const VideoMessage &);
	bool operator()(const AudioMessage &);
	bool operator()(const PeerInformation &);
	bool operator()(const PeerInformationList &);
	bool operator()(const RequestPeers &);

	// For image message
	bool operator()(bool);
	bool operator()(const Bytes &);
};
struct StreamDisplayUpdate
{
  private:
	StreamDisplay &display;
	const SDL_Event &event;

  public:
	StreamDisplayUpdate(StreamDisplay &, const SDL_Event &);
	~StreamDisplayUpdate();

	void operator()(std::monostate);
	void operator()(String &);
	void operator()(Texture &);
	void operator()(Bytes &);
};
struct StreamDisplayDraw
{
  private:
	StreamDisplay &display;
	const bool usingUi;

  public:
	StreamDisplayDraw(StreamDisplay &, bool);
	~StreamDisplayDraw();

	bool operator()(std::monostate);
	bool operator()(const String &);
	bool operator()(Texture &);
	bool operator()(const Bytes &);
};
} // namespace TemStream