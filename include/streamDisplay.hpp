/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class StreamDisplay;
struct StreamDisplayText
{
	String message;
	float scale;
};
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
		: left(std::move(a.left)), right(std::move(a.right)), currentAudio(std::move(a.currentAudio)),
		  texture(std::move(a.texture)), isRecording(a.isRecording)
	{
	}
	~CheckAudio()
	{
	}
};
struct ReplayData
{
	StringList packets;
	Message::TimeRange timeRange;
	unique_ptr<StreamDisplay> display;
	TimePoint lastUpdate;
	int64_t replayCursor;
	bool hasData;

	ReplayData(TemStreamGui &, const Message::Source &, const Message::TimeRange &);
	ReplayData(ReplayData &&);
	~ReplayData();
};
struct ChatLog
{
	List<Message::Chat> logs;
	bool autoScroll;

	ChatLog() : logs(), autoScroll(true)
	{
	}
	~ChatLog()
	{
	}
};
using DisplayData = std::variant<std::monostate, SDL_TextureWrapper, StreamDisplayText, ChatLog, ByteList, CheckAudio,
								 Message::ServerLinks, ReplayData>;
class StreamDisplay
{
  private:
	Message::Source source;
	DisplayData data;
	TemStreamGui &gui;
	ImGuiWindowFlags flags;
	bool visible;
	bool enableContextMenu;

	struct ContextMenu
	{
	  private:
		StreamDisplay &display;

	  public:
		ContextMenu(StreamDisplay &);
		~ContextMenu();

		bool operator()(std::monostate);
		bool operator()(StreamDisplayText &);
		bool operator()(ChatLog &);
		bool operator()(SDL_TextureWrapper &);
		bool operator()(CheckAudio &);
		bool operator()(ByteList &);
		bool operator()(Message::ServerLinks &);
		bool operator()(ReplayData &);
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
		bool operator()(StreamDisplayText &);
		bool operator()(ChatLog &);
		bool operator()(SDL_TextureWrapper &);
		bool operator()(CheckAudio &);
		bool operator()(ByteList &);
		bool operator()(Message::ServerLinks &);
		bool operator()(ReplayData &);
	};

  protected:
	void drawContextMenu();

  public:
	StreamDisplay() = delete;
	StreamDisplay(TemStreamGui &, const Message::Source &, const bool enableContextMenu = true);
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

	bool isReplay() const;

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