#include <main.hpp>

namespace TemStream
{
StreamDisplay::StreamDisplay(SDL_Renderer *renderer, const MessageSource &source)
	: source(source), data(std::monostate{}), renderer(renderer), visible(true)
{
}
StreamDisplay::StreamDisplay(StreamDisplay &&display)
	: source(std::move(display.source)), data(std::move(display.data)), renderer(display.renderer),
	  visible(display.visible)
{
}
StreamDisplay::~StreamDisplay()
{
}
StreamDisplay &StreamDisplay::operator=(StreamDisplay &&display)
{
	renderer = display.renderer;
	display.renderer = nullptr;
	data = std::move(display.data);
	source = std::move(display.source);
	visible = display.visible;
	return *this;
}
void StreamDisplay::handleEvent(const SDL_Event &e)
{
	StreamDisplayUpdate u(*this, e);
	std::visit(u, data);
}
bool StreamDisplay::draw(const bool usingUi)
{
	if (!visible)
	{
		return true;
	}
	StreamDisplayDraw d(*this, usingUi);
	return std::visit(d, data);
}
bool StreamDisplay::operator()(const TextMessage &message)
{
	data = message;
	return true;
}
bool StreamDisplay::operator()(const ImageMessage &message)
{
	return std::visit(*this, message);
}
bool StreamDisplay::operator()(const bool imageState)
{
	if (imageState)
	{
		data = Bytes();
		return true;
	}
	if (Bytes *bytes = std::get_if<Bytes>(&data))
	{
#ifndef NDEBUG
		printf("Reading %zu image KB\n", bytes->size() / KB(1));
#endif
		SDL_RWops *src = SDL_RWFromConstMem(bytes->data(), bytes->size());
		if (src == nullptr)
		{
			fprintf(stderr, "Failed to load image data: %s\n", SDL_GetError());
			return false;
		}
		SDL_Surface *surface = IMG_Load_RW(src, 0);
		if (surface == nullptr)
		{
			fprintf(stderr, "Surface load error: %s\n", IMG_GetError());
			return false;
		}

		bool success = true;
		{
			SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
			if (texture == nullptr)
			{
				fprintf(stderr, "Texture creation error: %s\n", SDL_GetError());
				success = false;
			}
			data = Texture(texture, surface->w, surface->h);
		}
		SDL_FreeSurface(surface);
		return success;
	}

	fprintf(stderr, "Stream display is in an invalid state\n");
	return false;
}
bool StreamDisplay::operator()(const Bytes &bytes)
{
	auto ptr = std::get_if<Bytes>(&data);
	if (ptr == nullptr)
	{
		fprintf(stderr, "Stream display is in an invalid state\n");
		return false;
	}

	ptr->insert(ptr->end(), bytes.begin(), bytes.end());
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
StreamDisplayUpdate::StreamDisplayUpdate(StreamDisplay &d, const SDL_Event &e) : display(d), event(e)
{
}
StreamDisplayUpdate::~StreamDisplayUpdate()
{
}
void StreamDisplayUpdate::operator()(std::monostate)
{
}
void StreamDisplayUpdate::operator()(String &)
{
}
void StreamDisplayUpdate::operator()(Texture &)
{
	// TODO: Move rect
}
void StreamDisplayUpdate::operator()(Bytes &)
{
}
StreamDisplayDraw::StreamDisplayDraw(StreamDisplay &d, const bool b) : display(d), usingUi(b)
{
}
StreamDisplayDraw::~StreamDisplayDraw()
{
}
bool StreamDisplayDraw::operator()(std::monostate)
{
	return true;
}
bool StreamDisplayDraw::operator()(const String &s)
{
	if (!usingUi)
	{
		return true;
	}

	std::array<char, KB(1)> buffer;
	display.source.print(buffer);
	if (ImGui::Begin(buffer.data(), &display.visible))
	{
		ImGui::TextWrapped("%s", s.c_str());
	}
	ImGui::End();
	return true;
}
bool StreamDisplayDraw::operator()(Texture &t)
{
	if (usingUi)
	{
		return true;
	}
	if (t.texture == nullptr)
	{
		return false;
	}
	if (SDL_RenderCopy(display.renderer, t.texture, NULL, &t.rect) == 0)
	{
		return true;
	}
	fprintf(stderr, "Failed to draw stream display: %s\n", SDL_GetError());
	return false;
}
bool StreamDisplayDraw::operator()(const Bytes &)
{
	return true;
}
} // namespace TemStream