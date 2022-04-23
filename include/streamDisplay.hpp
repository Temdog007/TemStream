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
	SDL_TextureWrapper &operator=(SDL_TextureWrapper &&) = delete;

	SDL_Texture *&getTexture()
	{
		return texture;
	}
};
struct CheckAudio : public SDL_TextureWrapper
{
	List<float> left;
	List<float> right;
	const bool isRecording;

  public:
	CheckAudio(SDL_Texture *t, const bool b) : SDL_TextureWrapper(t), left(), right(), isRecording(b)
	{
	}
	CheckAudio(CheckAudio &&a)
		: SDL_TextureWrapper(a.texture), left(std::move(left)), right(std::move(right)), isRecording(a.isRecording)
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
	Message::Source source;
	DisplayData data;
	TemStreamGui &gui;
	ImGuiWindowFlags flags;
	bool visible;

	friend class TemStreamGui;

	struct ContextMenu
	{
	  private:
		StreamDisplay &display;

	  public:
		ContextMenu(StreamDisplay &);
		~ContextMenu();

		void operator()(std::monostate);
		void operator()(const String &);
		void operator()(SDL_TextureWrapper &);
		void operator()(const Bytes &);
	};
	struct ImageMessageHandler
	{
	  private:
		StreamDisplay &display;

	  public:
		ImageMessageHandler(StreamDisplay &);
		~ImageMessageHandler();

		bool operator()(uint64_t);
		bool operator()(std::monostate);
		bool operator()(Bytes &&);
	};
	struct Draw
	{
	  private:
		StreamDisplay &display;

		void drawPoints(const List<float> &, float, float, float, float);

	  public:
		Draw(StreamDisplay &);
		~Draw();

		bool operator()(std::monostate);
		bool operator()(const String &);
		bool operator()(SDL_TextureWrapper &);
		bool operator()(CheckAudio &);
		bool operator()(const Bytes &);
	};

  protected:
	void drawContextMenu();

  public:
	StreamDisplay() = delete;
	StreamDisplay(TemStreamGui &, const Message::Source &);
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

	const Message::Source &getSource() const
	{
		return source;
	}

	bool setSurface(SDL_Surface *);

	bool draw();
	void drawFlagCheckboxes();

	MESSAGE_HANDLER_FUNCTIONS(bool);
};
} // namespace TemStream