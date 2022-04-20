#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class SDL_TextureWrapper
{
  protected:
	SDL_Texture *texture;

  public:
	SDL_TextureWrapper(SDL_Texture *);
	SDL_TextureWrapper(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper(SDL_TextureWrapper &&);
	virtual ~SDL_TextureWrapper();

	SDL_TextureWrapper &operator=(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper &operator=(SDL_TextureWrapper &&);

	SDL_Texture *&getTexture()
	{
		return texture;
	}
};
class CheckAudio : public SDL_TextureWrapper
{
  private:
	bool isRecording;

  public:
	CheckAudio(SDL_Texture *t, const bool b) : SDL_TextureWrapper(t), isRecording(b)
	{
	}
	CheckAudio(CheckAudio &&a) : SDL_TextureWrapper(a.texture), isRecording(a.isRecording)
	{
		a.texture = nullptr;
	}
	~CheckAudio()
	{
	}
};
using DisplayData = std::variant<std::monostate, SDL_TextureWrapper, String, Bytes, CheckAudio>;
class StreamDisplay
{
  private:
	MessageSource source;
	DisplayData data;
	TemStreamGui &gui;
	ImGuiWindowFlags flags;
	bool visible;

	friend class TemStreamGui;
	friend class StreamDisplayUpdate;
	friend class StreamDisplayContextMenu;
	friend class StreamDisplayDraw;

  protected:
	void drawContextMenu();

  public:
	StreamDisplay() = delete;
	StreamDisplay(TemStreamGui &, const MessageSource &);
	StreamDisplay(const StreamDisplay &) = delete;
	StreamDisplay(StreamDisplay &&);
	virtual ~StreamDisplay();

	StreamDisplay &operator=(const StreamDisplay &) = delete;
	StreamDisplay &operator=(StreamDisplay &&) = delete;

	ImGuiWindowFlags getFlags() const
	{
		return flags;
	}

	void setFlags(ImGuiWindowFlags flags)
	{
		this->flags = flags;
	}

	const MessageSource &getSource() const
	{
		return source;
	}

	bool draw();
	void drawFlagCheckboxes();

	bool operator()(TextMessage &&);
	bool operator()(ImageMessage &&);
	bool operator()(VideoMessage &&);
	bool operator()(AudioMessage &&);
	bool operator()(PeerInformation &&);
	bool operator()(PeerInformationList &&);
	bool operator()(RequestPeers &&);

	// For image message
	bool operator()(bool);
	bool operator()(const Bytes &);
};
struct StreamDisplayContextMenu
{
  private:
	StreamDisplay &display;

  public:
	StreamDisplayContextMenu(StreamDisplay &);
	~StreamDisplayContextMenu();

	void operator()(std::monostate);
	void operator()(const String &);
	void operator()(SDL_TextureWrapper &);
	void operator()(const Bytes &);
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
	bool operator()(CheckAudio &);
	bool operator()(const Bytes &);
};
} // namespace TemStream