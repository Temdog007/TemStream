#pragma once

#include <main.hpp>

namespace TemStream
{
struct StreamDisplay : public MessagePacketHandler
{
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Rect rect;
	MessageSource source;

	StreamDisplay() = delete;
	StreamDisplay(SDL_Renderer *, const MessageSource &);
	StreamDisplay(const StreamDisplay &) = delete;
	StreamDisplay(StreamDisplay &&);
	virtual ~StreamDisplay();

	void close();

	StreamDisplay &operator=(const StreamDisplay &) = delete;
	StreamDisplay &operator=(StreamDisplay &&);

	bool draw();

	bool operator()(const TextMessage &);
	bool operator()(const ImageMessage &);
	bool operator()(const VideoMessage &);
	bool operator()(const AudioMessage &);
	bool operator()(const PeerInformation &);
	bool operator()(const PeerInformationList &);
	bool operator()(const RequestPeers &);
};

} // namespace TemStream