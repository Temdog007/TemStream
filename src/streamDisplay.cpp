#include <main.hpp>

namespace TemStream
{
StreamDisplay::StreamDisplay(TemStreamGui &gui, const MessageSource &source)
	: source(source), data(std::monostate{}), gui(gui), visible(true)
{
}
StreamDisplay::StreamDisplay(StreamDisplay &&display)
	: source(std::move(display.source)), data(std::move(display.data)), gui(display.gui), visible(display.visible)
{
}
StreamDisplay::~StreamDisplay()
{
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
			SDL_Texture *texture = SDL_CreateTextureFromSurface(gui.renderer, surface);
			if (texture == nullptr)
			{
				fprintf(stderr, "Texture creation error: %s\n", SDL_GetError());
				success = false;
			}
			int w, h;
			SDL_GetWindowSize(gui.window, &w, &h);
			data = Texture(texture, SDL_min(w, surface->w), SDL_min(surface->h, h));
		}
		SDL_FreeSurface(surface);
		return success;
	}

	logger->AddError("Stream display is in an invalid state\n");
	return false;
}
bool StreamDisplay::operator()(const Bytes &bytes)
{
	auto ptr = std::get_if<Bytes>(&data);
	if (ptr == nullptr)
	{
		logger->AddError("Stream display is in an invalid state\n");
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
void StreamDisplayUpdate::operator()(Texture &t)
{
	switch (event.type)
	{
	case SDL_MOUSEBUTTONDOWN: {
		SDL_Point point;
		point.x = event.button.x;
		point.y = event.button.y;
		if (SDL_PointInRect(&point, &t.rect))
		{
			switch (event.button.button)
			{
			case SDL_BUTTON_LEFT:
				t.mode = Texture::Mode::Move;
				break;
			case SDL_BUTTON_RIGHT:
				t.mode = Texture::Mode::Resize;
				break;
			default:
				break;
			}
		}
	}
	break;
	case SDL_MOUSEMOTION: {
		if (t.mode == Texture::Mode::None)
		{
			break;
		}
		int w, h;
		SDL_GetWindowSize(display.gui.window, &w, &h);
		switch (t.mode)
		{
		case Texture::Mode::Move:
			t.rect.x = SDL_clamp(t.rect.x + event.motion.xrel, 0, w - t.rect.w);
			t.rect.y = SDL_clamp(t.rect.y + event.motion.yrel, 0, h - t.rect.h);
			break;
		case Texture::Mode::Resize:
			t.rect.w = SDL_clamp(t.rect.w + event.motion.xrel, 32, w);
			t.rect.h = SDL_clamp(t.rect.h + event.motion.yrel, 32, h);
			break;
		default:
			break;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP:
		t.mode = Texture::Mode::None;
		break;
	default:
		break;
	}
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

	std::array<char, KB(8)> buffer;
	display.source.print(buffer);
	if (ImGui::Begin(buffer.data(), &display.visible))
	{
		const char *str = s.c_str();
		for (size_t i = 0; i < s.size(); i += sizeof(buffer))
		{
			if (ImGui::BeginChild(1 + (i / sizeof(buffer))))
			{
				strncpy(buffer.data(), str + i, sizeof(buffer));
				ImGui::TextWrapped("%s", buffer.data());
			}
			ImGui::EndChild();
		}
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
	if (SDL_RenderCopy(display.gui.renderer, t.texture, NULL, &t.rect) == 0)
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