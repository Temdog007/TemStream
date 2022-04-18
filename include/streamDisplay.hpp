#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class SDL_TextureWrapper
{
  private:
	SDL_Texture *texture;

  public:
	SDL_TextureWrapper(SDL_Texture *);
	SDL_TextureWrapper(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper(SDL_TextureWrapper &&);
	~SDL_TextureWrapper();

	SDL_TextureWrapper &operator=(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper &operator=(SDL_TextureWrapper &&);

	SDL_Texture *getTexture()
	{
		return texture;
	}
};
using DisplayData = std::variant<std::monostate, SDL_TextureWrapper, String, Bytes>;
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

	bool draw();

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
struct StreamDisplayDraw
{
  private:
	StreamDisplay &display;

  public:
	StreamDisplayDraw(StreamDisplay &);
	~StreamDisplayDraw();

	bool operator()(std::monostate);
	bool operator()(const String &);
	bool operator()(SDL_TextureWrapper &);
	bool operator()(const Bytes &);
};
} // namespace TemStream