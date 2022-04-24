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
	~SDL_TextureWrapper();

	SDL_TextureWrapper &operator=(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper &operator=(SDL_TextureWrapper &&);

	SDL_Texture *&getTexture()
	{
		return texture;
	}
};
struct CheckAudio
{
	List<float> left;
	List<float> right;
	SDL_TextureWrapper texture;
	const bool isRecording;

	CheckAudio(SDL_Texture *t, const bool b) : left(), right(), texture(t), isRecording(b)
	{
	}
	CheckAudio(CheckAudio &&a)
		: left(std::move(left)), right(std::move(right)), texture(std::move(a.texture)), isRecording(a.isRecording)
	{
	}
	~CheckAudio()
	{
	}
};
struct VideoDecoder
{
	Video::VPX vpx;
	std::optional<std::future<std::optional<Video::Frame>>> decodeWork;
	SDL_TextureWrapper texture;

	VideoDecoder(Video::VPX &&);
	VideoDecoder(VideoDecoder &&);
	~VideoDecoder();
};
using VideoDecoderPtr = shared_ptr<VideoDecoder>;
using DisplayData = std::variant<std::monostate, SDL_TextureWrapper, String, Bytes, CheckAudio, VideoDecoderPtr>;
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
		void operator()(String &);
		void operator()(VideoDecoderPtr &);
		void operator()(SDL_TextureWrapper &);
		void operator()(Bytes &);

		template <typename T> void operator()(T &t)
		{
			operator()(t.texture);
		}
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
		bool operator()(String &);
		bool operator()(SDL_TextureWrapper &);
		bool operator()(CheckAudio &);
		bool operator()(VideoDecoderPtr &);
		bool operator()(Bytes &);

		template <typename T> bool operator()(T &t)
		{
			return operator()(t.texture);
		}
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