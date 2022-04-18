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
bool StreamDisplay::draw()
{
	if (!visible)
	{
		return true;
	}
	StreamDisplayDraw d(*this);
	return std::visit(d, data);
}
bool StreamDisplay::operator()(const TextMessage &message)
{
	data.emplace<String>(message);
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
		logger->AddTrace("Reading image: %zu KB\n", bytes->size() / KB(1));
		SDL_RWops *src = SDL_RWFromConstMem(bytes->data(), bytes->size());
		if (src == nullptr)
		{
			logger->AddError("Failed to load image data: %s\n", SDL_GetError());
			return false;
		}
		SDL_Surface *surface = IMG_Load_RW(src, 0);
		if (surface == nullptr)
		{
			logger->AddError("Surface load error: %s\n", IMG_GetError());
			return false;
		}

		bool success = true;
		{
			SDL_Texture *texture = SDL_CreateTextureFromSurface(gui.renderer, surface);
			if (texture == nullptr)
			{
				logger->AddError("Texture creation error: %s\n", SDL_GetError());
				success = false;
			}
			int w, h;
			SDL_GetWindowSize(gui.window, &w, &h);
			data = texture;
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
	logger->AddError(
		"Got 'PeerInformation' message from the server. Disconnecting from server for it may not be safe.\n");
	return false;
}
bool StreamDisplay::operator()(const PeerInformationList &)
{
	logger->AddError(
		"Got 'PeerInformationList' message from the server. Disconnecting from server for it may not be safe.\n");
	return false;
}
bool StreamDisplay::operator()(const RequestPeers &)
{
	logger->AddError("Got 'RequestPeers' message from the server. Disconnecting from server for it may not be safe.\n");
	return false;
}
StreamDisplayDraw::StreamDisplayDraw(StreamDisplay &d) : display(d)
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
	std::array<char, KB(8)> buffer;
	display.source.print(buffer);
	if (ImGui::Begin(buffer.data(), &display.visible, display.gui.getStreamDisplayFlags()))
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
bool StreamDisplayDraw::operator()(SDL_TextureWrapper &t)
{
	SDL_Texture *texture = t.getTexture();
	if (texture == nullptr)
	{
		return false;
	}
	std::array<char, KB(8)> buffer;
	display.source.print(buffer);
	if (ImGui::Begin(buffer.data(), &display.visible, display.gui.getStreamDisplayFlags()))
	{
		const auto max = ImGui::GetWindowContentRegionMax();
		const auto min = ImGui::GetWindowContentRegionMin();
		ImGui::Image(texture, ImVec2(max.x - min.x, max.y - min.y));
	}
	ImGui::End();
	return true;
}
bool StreamDisplayDraw::operator()(const Bytes &)
{
	return true;
}
SDL_TextureWrapper::SDL_TextureWrapper(SDL_Texture *texture) : texture(texture)
{
}
SDL_TextureWrapper::SDL_TextureWrapper(SDL_TextureWrapper &&w) : texture(w.texture)
{
	w.texture = nullptr;
}
SDL_TextureWrapper::~SDL_TextureWrapper()
{
	SDL_DestroyTexture(texture);
	texture = nullptr;
}
SDL_TextureWrapper &SDL_TextureWrapper::operator=(SDL_TextureWrapper &&w)
{
	texture = w.texture;
	w.texture = nullptr;
	return *this;
}
} // namespace TemStream