#include <main.hpp>

namespace TemStream
{
StreamDisplay::StreamDisplay(SDL_Renderer *renderer, const MessageSource &source)
	: source(source), data(std::monostate{}), renderer(renderer)
{
}
StreamDisplay::StreamDisplay(StreamDisplay &&display)
	: source(std::move(source)), data(std::move(display.data)), renderer(display.renderer)
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
	return *this;
}
void StreamDisplay::handleEvent(const SDL_Event &e)
{
	StreamDisplayUpdate u(*this, e);
	std::visit(u, data);
}
bool StreamDisplay::draw(const bool usingUi)
{
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
	}
	else if (Bytes *bytes = std::get_if<Bytes>(&data))
	{
		SDL_RWops *src = SDL_RWFromConstMem(bytes->data(), bytes->size());
		SDL_Surface *surface = IMG_Load_RW(src, 0);
		if (surface == nullptr)
		{
			fprintf(stderr, "Surface load error: %s\n", IMG_GetError());
			goto end;
		}

		{
			SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
			if (texture == nullptr)
			{
				fprintf(stderr, "Texture creation error: %s\n", SDL_GetError());
				goto end;
			}
			data = Texture(texture);
		}
	end:
		SDL_FreeSurface(surface);
	}
	return true;
}
bool StreamDisplay::operator()(const Bytes &bytes)
{
	auto ptr = std::get_if<Bytes>(&data);
	if (ptr != nullptr)
	{
		ptr->insert(ptr->end(), bytes.begin(), bytes.end());
	}
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
void StreamDisplayUpdate::operator()(std::string &s)
{
	switch (event.type)
	{
	case SDL_DROPFILE: {
		FILE *file = fopen(event.drop.file, "r");
		if (file == nullptr)
		{
			break;
		}
		s.clear();
		char ch;
		if ((ch = fgetc(file)) != EOF)
		{
			s += ch;
		}
		fclose(file);
	}
	break;
	case SDL_DROPTEXT:
		s = event.drop.file;
		break;
	default:
		break;
	}
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
bool StreamDisplayDraw::operator()(const std::string &s)
{
	if (!usingUi)
	{
		return true;
	}

	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "Text: %s", display.source.destination.c_str());
	if (ImGui::Begin(buffer))
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
		return true;
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