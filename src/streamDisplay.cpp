#include <main.hpp>

namespace TemStream
{
StreamDisplay::StreamDisplay(TemStreamGui &gui, const MessageSource &source)
	: source(source), data(std::monostate{}), gui(gui), flags(ImGuiWindowFlags_None), visible(true)
{
}
StreamDisplay::StreamDisplay(StreamDisplay &&display)
	: source(std::move(display.source)), data(std::move(display.data)), gui(display.gui), flags(display.flags),
	  visible(display.visible)
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
	return std::visit(StreamDisplay::Draw(*this), data);
}
void StreamDisplay::drawContextMenu()
{
	if (ImGui::BeginPopupContextWindow("Stream Display Flags"))
	{
		drawFlagCheckboxes();
		std::visit(StreamDisplay::ContextMenu(*this), data);
		ImGui::EndPopup();
	}
}
void StreamDisplay::drawFlagCheckboxes()
{
	ImGui::Checkbox("Visible", &visible);
	bool showTitleBar = (flags & ImGuiWindowFlags_NoTitleBar) == 0;
	if (ImGui::Checkbox("Show Title Bar", &showTitleBar))
	{
		if (showTitleBar)
		{
			flags &= ~ImGuiWindowFlags_NoTitleBar;
		}
		else
		{
			flags |= ImGuiWindowFlags_NoTitleBar;
		}
	}
	bool movable = (flags & ImGuiWindowFlags_NoMove) == 0;
	if (ImGui::Checkbox("Movable", &movable))
	{
		if (movable)
		{
			flags &= ~ImGuiWindowFlags_NoMove;
		}
		else
		{
			flags |= ImGuiWindowFlags_NoMove;
		}
	}
	bool resizable = (flags & ImGuiWindowFlags_NoResize) == 0;
	if (ImGui::Checkbox("Resizable", &resizable))
	{
		if (resizable)
		{
			flags &= ~ImGuiWindowFlags_NoResize;
		}
		else
		{
			flags |= ImGuiWindowFlags_NoResize;
		}
	}
}
bool StreamDisplay::operator()(TextMessage &&message)
{
	data = std::move(message);
	return true;
}
bool StreamDisplay::operator()(ImageMessage &&message)
{
	return std::visit(ImageMessageHandler(*this), std::move(message));
}
StreamDisplay::ImageMessageHandler::ImageMessageHandler(StreamDisplay &display) : display(display)
{
}
StreamDisplay::ImageMessageHandler::~ImageMessageHandler()
{
}
bool StreamDisplay::ImageMessageHandler::operator()(const uint64_t size)
{
	Bytes bytes;
	bytes.reserve(size);
	display.data.emplace<Bytes>(std::move(bytes));
	return true;
}
bool StreamDisplay::ImageMessageHandler::operator()(std::monostate)
{
	if (Bytes *bytes = std::get_if<Bytes>(&display.data))
	{
		(*logger)(Logger::Trace) << "Reading image: " << bytes->size() / KB(1) << "KB" << std::endl;
		SDL_RWops *src = SDL_RWFromConstMem(bytes->data(), bytes->size());
		if (src == nullptr)
		{
			logSDLError("Failed to load image data");
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
			SDL_Texture *texture = SDL_CreateTextureFromSurface(display.gui.getRenderer(), surface);
			if (texture == nullptr)
			{
				logSDLError("Texture creation error");
				success = false;
			}
			display.data.emplace<SDL_TextureWrapper>(texture);
		}
		SDL_FreeSurface(surface);
		return success;
	}

	logger->AddError("Stream display is in an invalid state");
	return false;
}
bool StreamDisplay::ImageMessageHandler::operator()(Bytes &&bytes)
{
	auto ptr = std::get_if<Bytes>(&display.data);
	if (ptr == nullptr)
	{
		logger->AddError("Stream display is in an invalid state");
		return false;
	}

	ptr->insert(ptr->end(), bytes.begin(), bytes.end());
	return true;
}
bool StreamDisplay::operator()(VideoMessage &&)
{
	return true;
}
bool StreamDisplay::operator()(AudioMessage &&audio)
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
			return gui.addAudio(std::move(ptr));
		}
	}
	return true;
}
bool StreamDisplay::operator()(PeerInformation &&)
{
	logger->AddError(
		"Got 'PeerInformation' message from the server. Disconnecting from server for it may not be safe.");
	return false;
}
bool StreamDisplay::operator()(PeerInformationList &&)
{
	logger->AddError(
		"Got 'PeerInformationList' message from the server. Disconnecting from server for it may not be safe.");
	return false;
}
bool StreamDisplay::operator()(RequestPeers &&)
{
	logger->AddError("Got 'RequestPeers' message from the server. Disconnecting from server for it may not be safe.");
	return false;
}
StreamDisplay::Draw::Draw(StreamDisplay &d) : display(d)
{
}
StreamDisplay::Draw::~Draw()
{
}
bool StreamDisplay::Draw::operator()(std::monostate)
{
	return true;
}
bool StreamDisplay::Draw::operator()(const String &s)
{
	std::array<char, KB(8)> buffer;
	display.source.print(buffer);
	SetWindowMinSize(display.gui.getWindow());
	if (ImGui::Begin(buffer.data(), &display.visible, display.flags))
	{
		display.drawContextMenu();
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
bool StreamDisplay::Draw::operator()(SDL_TextureWrapper &t)
{
	auto &texture = t.getTexture();
	if (texture == nullptr)
	{
		return false;
	}
	std::array<char, KB(8)> buffer;
	display.source.print(buffer);
	SetWindowMinSize(display.gui.getWindow());
	if (ImGui::Begin(buffer.data(), &display.visible, display.flags))
	{
		display.drawContextMenu();
		const auto max = ImGui::GetWindowContentRegionMax();
		const auto min = ImGui::GetWindowContentRegionMin();
		ImGui::Image(texture, ImVec2(max.x - min.x, max.y - min.y));
	}
	ImGui::End();
	return true;
}
bool StreamDisplay::Draw::operator()(CheckAudio &t)
{
	auto ptr = display.gui.getAudio(display.getSource());
	if (ptr == nullptr)
	{
		// Maybe determine when this is an error and when the user removed the audio
		// (*logger)(Logger::Error) << "Audio is missing for stream: " << display.getSource() << std::endl;
		return false;
	}

	SDL_Renderer *renderer = display.gui.getRenderer();

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
			logSDLError("Failed to create texture");
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
			const float f = fdata[i];
			if (f < -1.f || f > 1.f || std::isinf(f) || std::isnan(f))
			{
				continue;
			}
			const float percent = (float)i / (float)fsize;
			point.x = audioWidth * percent;
			point.y = ((f + 1.f) / 2.f) * audioHeight;
			points.push_back(point);
		}
		SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
	}
	SDL_SetRenderTarget(renderer, target);

	return operator()(static_cast<SDL_TextureWrapper &>(t));
}
bool StreamDisplay::Draw::operator()(const Bytes &)
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
StreamDisplay::ContextMenu::ContextMenu(StreamDisplay &d) : display(d)
{
}
StreamDisplay::ContextMenu::~ContextMenu()
{
}
void StreamDisplay::ContextMenu::operator()(std::monostate)
{
}
void StreamDisplay::ContextMenu::operator()(const Bytes &)
{
}
void StreamDisplay::ContextMenu::operator()(const String &s)
{
	if (ImGui::Button("Copy"))
	{
		if (SDL_SetClipboardText(s.c_str()) == 0)
		{
			(*logger)(Logger::Info) << "Copied text to clipboard" << std::endl;
		}
		else
		{
			logSDLError("Failed to copy to clipboard");
		}
	}
}
void StreamDisplay::ContextMenu::operator()(SDL_TextureWrapper &w)
{
	auto texture = w.getTexture();
	if (texture != nullptr && ImGui::Button("Screenshot"))
	{
		int w, h;
		if (SDL_QueryTexture(texture, nullptr, nullptr, &w, &h) != 0)
		{
			logSDLError("Failed to query texture");
			return;
		}

		auto renderer = display.gui.getRenderer();
		auto surface = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
		auto t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
		if (t == nullptr || surface == nullptr)
		{
			logSDLError("Failed to create screenshot");
			goto end;
		}

		if (SDL_SetRenderTarget(renderer, t) != 0 || SDL_RenderCopy(renderer, texture, nullptr, nullptr) != 0 ||
			SDL_RenderReadPixels(renderer, nullptr, surface->format->format, surface->pixels, surface->pitch) != 0)
		{
			logSDLError("Error taking screenshot");
			goto end;
		}

		{
			char buffer[1024];
			const time_t t = time(NULL);
			strftime(buffer, sizeof(buffer), "screenshot_%y_%m_%d_%H_%M_%S.png", localtime(&t));
			if (IMG_SavePNG(surface, buffer) == 0)
			{
				*logger << "Saved screenshot to " << buffer << std::endl;
			}
			else
			{
				(*logger)(Logger::Error) << "Failed to save screenshot: " << IMG_GetError() << std::endl;
			}
		}

	end:
		SDL_DestroyTexture(t);
		SDL_FreeSurface(surface);
	}
}
} // namespace TemStream