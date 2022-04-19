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
bool StreamDisplay::operator()(TextMessage message)
{
	data = std::move(message);
	return true;
}
bool StreamDisplay::operator()(ImageMessage message)
{
	return std::visit(*this, std::move(message));
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
		(*logger)(Logger::Trace) << "Reading image: " << bytes->size() / KB(1) << "KB" << std::endl;
		SDL_RWops *src = SDL_RWFromConstMem(bytes->data(), bytes->size());
		if (src == nullptr)
		{
			(*logger)(Logger::Error) << "Failed to load image data: " << SDL_GetError() << std::endl;
			return false;
		}
		SDL_Surface *surface = IMG_Load_RW(src, 0);
		if (surface == nullptr)
		{
			(*logger)(Logger::Error) << "Surface load error: " << IMG_GetError() << std::endl;
			return false;
		}

		bool success = true;
		{
			SDL_Texture *texture = SDL_CreateTextureFromSurface(gui.renderer, surface);
			if (texture == nullptr)
			{
				(*logger)(Logger::Error) << "Texture creation error: " << SDL_GetError() << std::endl;
				success = false;
			}
			data = SDL_TextureWrapper(texture);
		}
		SDL_FreeSurface(surface);
		return success;
	}

	logger->AddError("Stream display is in an invalid state");
	return false;
}
bool StreamDisplay::operator()(const Bytes &bytes)
{
	auto ptr = std::get_if<Bytes>(&data);
	if (ptr == nullptr)
	{
		logger->AddError("Stream display is in an invalid state");
		return false;
	}

	ptr->insert(ptr->end(), bytes.begin(), bytes.end());
	return true;
}
bool StreamDisplay::operator()(VideoMessage)
{
	return true;
}
bool StreamDisplay::operator()(AudioMessage audio)
{
	if (auto a = gui.getAudio(source))
	{
		const bool isRecording = a->isRecording();
		if (!isRecording)
		{
			a->enqueueAudio(audio.bytes);
		}
		if (!std::holds_alternative<CheckAudio>(data))
		{
			data.emplace<CheckAudio>(nullptr, isRecording);
		}
	}
	else
	{
		auto ptr = Audio::startPlayback(source, NULL);
		if (ptr)
		{
			return gui.addAudio(ptr);
		}
	}
	return true;
}
bool StreamDisplay::operator()(PeerInformation)
{
	logger->AddError(
		"Got 'PeerInformation' message from the server. Disconnecting from server for it may not be safe.");
	return false;
}
bool StreamDisplay::operator()(PeerInformationList)
{
	logger->AddError(
		"Got 'PeerInformationList' message from the server. Disconnecting from server for it may not be safe.");
	return false;
}
bool StreamDisplay::operator()(RequestPeers)
{
	logger->AddError("Got 'RequestPeers' message from the server. Disconnecting from server for it may not be safe.");
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
	SetWindowMinSize(display.gui.window);
	if (ImGui::Begin(buffer.data(), &display.visible, display.gui.getStreamDisplayFlags()))
	{
		const char *str = s.c_str();
		for (size_t i = 0; i < s.size(); i += sizeof(buffer))
		{
			if (ImGui::BeginChild(1 + (i / sizeof(buffer))))
			{
				strncpy(buffer.data(), str + i, sizeof(buffer) - 1);
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
	auto &texture = t.getTexture();
	if (texture == nullptr)
	{
		return false;
	}
	std::array<char, KB(8)> buffer;
	display.source.print(buffer);
	SetWindowMinSize(display.gui.window);
	if (ImGui::Begin(buffer.data(), &display.visible, display.gui.getStreamDisplayFlags()))
	{
		const auto max = ImGui::GetWindowContentRegionMax();
		const auto min = ImGui::GetWindowContentRegionMin();
		ImGui::Image(texture, ImVec2(max.x - min.x, max.y - min.y));
	}
	ImGui::End();
	return true;
}
bool StreamDisplayDraw::operator()(CheckAudio &t)
{
	auto ptr = display.gui.getAudio(display.getSource());
	if (ptr == nullptr)
	{
		// Maybe determine when this is an error and when the user removed the audio
		// (*logger)(Logger::Error) << "Audio is missing for stream: " << display.getSource() << std::endl;
		return false;
	}

	SDL_Renderer *renderer = display.gui.renderer;

	Bytes current;
	ptr->useCurrentAudio([&current](const Bytes &b) { current.insert(current.end(), b.begin(), b.end()); });

	const int audioWidth = 2048;
	const int audioHeight = 512;
	auto &texture = t.getTexture();
	if (texture == nullptr)
	{
		SDL_DestroyTexture(texture);
		texture =
			SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, audioWidth, audioHeight);
		if (texture == nullptr)
		{
			(*logger)(Logger::Error) << "Failed to create texture: " << SDL_GetError() << std::endl;
			return false;
		}
	}

	SDL_Texture *const target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, texture);
	SDL_SetRenderDrawColor(renderer, 0u, 0u, 0u, 255u);
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 0u, 0u, 255u, 255u);
	SDL_RenderDrawLineF(renderer, 0.f, audioHeight / 2, audioWidth, audioHeight / 2);

	if (!current.empty() && current.size() < INT32_MAX)
	{
		SDL_SetRenderDrawColor(renderer, 0u, 255u, 0u, 255u);
		const float *fdata = reinterpret_cast<const float *>(current.data());
		const size_t fsize = current.size() / sizeof(float);

		List<SDL_FPoint> points;
		points.reserve(fsize);
		SDL_FPoint point;
		for (size_t i = 0; i < fsize; ++i)
		{
			if (fdata[i] < -1.f || fdata[i] > 1.f)
			{
				continue;
			}
			const float percent = (float)i / (float)fsize;
			point.x = audioWidth * percent;
			point.y = ((fdata[i] + 1.f) / 2.f) * audioHeight;
			points.push_back(point);
		}
		SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
	}
	SDL_SetRenderTarget(renderer, target);

	return operator()(static_cast<SDL_TextureWrapper &>(t));
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