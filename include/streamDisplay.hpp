#pragma once

#include <main.hpp>

namespace TemStream
{
struct Texture
{
	SDL_Rect rect;
	SDL_Texture *texture;
	bool moving;

	Texture(SDL_Texture *texture) : rect(), texture(texture), moving(false)
	{
	}
	Texture(const Texture &) = delete;
	Texture(Texture &&t) : rect(t.rect), texture(t.texture), moving(t.moving)
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
		moving = t.moving;
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
	SDL_Renderer *renderer;

	friend class StreamDisplayDraw;

  public:
	StreamDisplay() = delete;
	StreamDisplay(SDL_Renderer *, const MessageSource &);
	StreamDisplay(const StreamDisplay &) = delete;
	StreamDisplay(StreamDisplay &&);
	virtual ~StreamDisplay();

	StreamDisplay &operator=(const StreamDisplay &) = delete;
	StreamDisplay &operator=(StreamDisplay &&);

	void handleEvent(const SDL_Event &);
	bool draw(bool usingUi);

	bool operator()(const TextMessage &);
	bool operator()(const ImageMessage &);
	bool operator()(const VideoMessage &);
	bool operator()(const AudioMessage &);
	bool operator()(const PeerInformation &);
	bool operator()(const PeerInformationList &);
	bool operator()(const RequestPeers &);

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