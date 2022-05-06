#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
struct CheckAudio
{
	List<float> left;
	List<float> right;
	ByteList currentAudio;
	SDL_TextureWrapper texture;
	const bool isRecording;

	CheckAudio(SDL_Texture *t, const bool b) : left(), right(), currentAudio(), texture(t), isRecording(b)
	{
	}
	CheckAudio(CheckAudio &&a)
		: left(std::move(left)), right(std::move(right)), currentAudio(std::move(a.currentAudio)),
		  texture(std::move(a.texture)), isRecording(a.isRecording)
	{
	}
	~CheckAudio()
	{
	}
};
using ChatLog = List<Message::Chat>;
using DisplayData =
	std::variant<std::monostate, SDL_TextureWrapper, String, ChatLog, ByteList, CheckAudio, Message::ServerLinks>;
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
		void operator()(ChatLog &);
		void operator()(SDL_TextureWrapper &);
		void operator()(CheckAudio &);
		void operator()(ByteList &);
		void operator()(Message::ServerLinks &);
	};
	struct ImageMessageHandler
	{
	  private:
		StreamDisplay &display;

	  public:
		ImageMessageHandler(StreamDisplay &);
		~ImageMessageHandler();

		bool operator()(Message::LargeFile &&);
		bool operator()(uint64_t);
		bool operator()(std::monostate);
		bool operator()(ByteList &&);
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
		bool operator()(ChatLog &);
		bool operator()(SDL_TextureWrapper &);
		bool operator()(CheckAudio &);
		bool operator()(ByteList &);
		bool operator()(Message::ServerLinks &);
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

	void updateTexture(const VideoSource::Frame &);

	bool setSurface(SDL_Surface *);

	bool draw();
	void drawFlagCheckboxes();

	MESSAGE_HANDLER_FUNCTIONS(bool);
};
} // namespace TemStream