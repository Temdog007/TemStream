#include <main.hpp>

namespace TemStream
{
StreamDisplay::StreamDisplay() : texture(nullptr), rect()
{
}
StreamDisplay::StreamDisplay(StreamDisplay &&display) : texture(display.texture), rect(display.rect)
{
	display.texture = nullptr;
}
StreamDisplay::~StreamDisplay()
{
	close();
}
void StreamDisplay::close()
{
	SDL_DestroyTexture(texture);
}
StreamDisplay &StreamDisplay::operator=(StreamDisplay &&display)
{
	texture = display.texture;
	rect = display.rect;
	display.texture = nullptr;
	return *this;
}
bool StreamDisplay::draw(SDL_Renderer *renderer)
{
	if (SDL_RenderCopy(renderer, texture, NULL, &rect) == 0)
	{
		return true;
	}
	fprintf(stderr, "Failed to draw stream display: %s\n", SDL_GetError());
	return false;
}
bool StreamDisplay::operator()(const TextMessage &)
{
	return true;
}
bool StreamDisplay::operator()(const ImageMessage &)
{
	return true;
}
bool StreamDisplay::operator()(const VideoMessage &)
{
	return true;
}
bool StreamDisplay::operator()(const AudioMessage &)
{
	return true;
}
bool StreamDisplay::operator()(const PeerInformation &)
{
	fprintf(stderr,
			"Got 'PeerInformation' message from the server. Disconnecting from server for it may not be safe.\n");
	return false;
}
bool StreamDisplay::operator()(const PeerInformationList &)
{
	fprintf(stderr,
			"Got 'PeerInformationList' message from the server. Disconnecting from server for it may not be safe.\n");
	return false;
}
bool StreamDisplay::operator()(const RequestPeers &)
{
	fprintf(stderr, "Got 'RequestPeers' message from the server. Disconnecting from server for it may not be safe.\n");
	return false;
}
} // namespace TemStream