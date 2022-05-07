#include <main.hpp>

namespace TemStream
{
StreamDisplay::StreamDisplay(TemStreamGui &gui, const Message::Source &source)
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
void StreamDisplay::updateTexture(const VideoSource::Frame &frame)
{
	if (!visible)
	{
		return;
	}
	if (!std::holds_alternative<SDL_TextureWrapper>(data))
	{
		(*logger)(Logger::Trace) << "Resized video texture " << frame.width << 'x' << frame.height << std::endl;
		SDL_Texture *texture =
			SDL_CreateTexture(gui.getRenderer(), frame.format, SDL_TEXTUREACCESS_STREAMING, frame.width, frame.height);
		data.emplace<SDL_TextureWrapper>(texture);
	}
	auto &wrapper = std::get<SDL_TextureWrapper>(data);
	auto &texture = *wrapper;
	{
		int w, h;
		uint32_t format;
		if (SDL_QueryTexture(texture, &format, 0, &w, &h) != 0)
		{
			logSDLError("Failed to query texture");
			return;
		}
		if (format != frame.format || frame.width != static_cast<uint32_t>(w) ||
			frame.height != static_cast<uint32_t>(h))
		{
			data.emplace<std::monostate>();
			updateTexture(frame);
			return;
		}
	}
	void *pixels = nullptr;
	int pitch;
	if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) == 0)
	{
		memcpy(pixels, frame.bytes.data(), frame.bytes.size());
	}
	else
	{
		logSDLError("Failed to update texture");
	}
	SDL_UnlockTexture(texture);
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
	{
		StringStream ss;
		ss << source;
		ImGui::PushID(ss.str().c_str());
	}

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

	ImGui::PopID();
}
bool StreamDisplay::setSurface(SDL_Surface *surface)
{
	SDL_Texture *texture = SDL_CreateTextureFromSurface(gui.getRenderer(), surface);
	if (texture == nullptr)
	{
		logSDLError("Texture creation error");
		return false;
	}
	data.emplace<SDL_TextureWrapper>(texture);
	return true;
}
bool StreamDisplay::operator()(Message::Text &message)
{
	data = std::move(message);
	return true;
}
bool StreamDisplay::operator()(Message::Chat &chat)
{
	if (!std::holds_alternative<ChatLog>(data))
	{
		data.emplace<ChatLog>();
	}

	auto &chatLog = std::get<ChatLog>(data);
	chatLog.logs.emplace_back(std::move(chat));
	return true;
}
bool StreamDisplay::operator()(Message::Image &message)
{
	return std::visit(ImageMessageHandler(*this), std::move(message.largeFile));
}
StreamDisplay::ImageMessageHandler::ImageMessageHandler(StreamDisplay &display) : display(display)
{
}
StreamDisplay::ImageMessageHandler::~ImageMessageHandler()
{
}
bool StreamDisplay::ImageMessageHandler::operator()(Message::LargeFile &&lf)
{
	return std::visit(*this, std::move(lf));
}
bool StreamDisplay::ImageMessageHandler::operator()(const uint64_t size)
{
	ByteList bytes(size);
	display.data.emplace<ByteList>(std::move(bytes));
	return true;
}
bool StreamDisplay::ImageMessageHandler::operator()(std::monostate)
{
	if (ByteList *bytes = std::get_if<ByteList>(&display.data))
	{
		WorkPool::workPool.addWork([source = display.getSource(), bytes = std::move(*bytes)]() {
			Work::loadSurface(source, std::move(bytes));
			return false;
		});
		display.data.emplace<std::monostate>();
		return true;
	}

	logger->AddError("Stream display is in an invalid state");
	return false;
}
bool StreamDisplay::ImageMessageHandler::operator()(ByteList &&bytes)
{
	auto ptr = std::get_if<ByteList>(&display.data);
	if (ptr == nullptr)
	{
		logger->AddError("Stream display is in an invalid state");
		return false;
	}

	ptr->append(bytes);
	return true;
}
bool StreamDisplay::operator()(Message::Audio &audio)
{
	if (!gui.useAudio(source, [this, &audio](AudioSource &a) {
			if (!std::holds_alternative<CheckAudio>(data))
			{
				data.emplace<CheckAudio>(nullptr, a.isRecording());
			}
		}))
	{
		auto ptr = AudioSource::startPlayback(source, nullptr, gui.getConfiguration().defaultVolume / 100.f);
		if (ptr && gui.addAudio(std::move(ptr)))
		{
			*logger << "Started playback on default audio device" << std::endl;
		}
	}
	return true;
}
bool StreamDisplay::operator()(Message::ServerLinks &serverLinks)
{
	data.emplace<Message::ServerLinks>(std::move(serverLinks));
	return true;
}
#define BAD_MESSAGE(X)                                                                                                 \
	logger->AddError("Got unexpected '" #X "' message from the server. Disconnecting from server.");                   \
	return false
bool StreamDisplay::operator()(std::monostate)
{
	BAD_MESSAGE(Empty);
}
bool StreamDisplay::operator()(Message::Video &)
{
	BAD_MESSAGE(Video);
}
bool StreamDisplay::operator()(Message::Credentials &)
{
	BAD_MESSAGE(Credentials);
}
bool StreamDisplay::operator()(Message::VerifyLogin &)
{
	BAD_MESSAGE(VerifyLogin);
}
bool StreamDisplay::operator()(Message::RequestPeers &)
{
	BAD_MESSAGE(RequestPeers);
}
bool StreamDisplay::operator()(Message::PeerList &)
{
	BAD_MESSAGE(PeerList);
}
bool StreamDisplay::operator()(Access &)
{
	BAD_MESSAGE(Access);
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
bool StreamDisplay::Draw::operator()(String &s)
{
	SetWindowMinSize(display.gui.getWindow());
	if (ImGui::Begin(display.source.serverName.c_str(), &display.visible, display.flags))
	{
		display.drawContextMenu();
		const char *str = s.c_str();
		std::array<char, KB(8)> buffer;
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
bool StreamDisplay::Draw::operator()(ChatLog &chatLog)
{
	SetWindowMinSize(display.gui.getWindow());
	if (ImGui::Begin(display.source.serverName.c_str(), &display.visible, display.flags))
	{
		display.drawContextMenu();
		ImGui::Checkbox("Auto Scroll", &chatLog.autoScroll);
		if (ImGui::BeginChild("Logs"))
		{
			if (ImGui::BeginTable("Chat Logs", 3, TableFlags))
			{
				ImGui::TableSetupColumn("Timestamp");
				ImGui::TableSetupColumn("Author");
				ImGui::TableSetupColumn("Message");
				ImGui::TableHeadersRow();
				char buffer[KB(1)];
				for (const auto &log : chatLog.logs)
				{
					ImGui::TableNextColumn();
					const auto t = static_cast<time_t>(log.timestamp);
					std::strftime(buffer, sizeof(buffer), "%D %T", std::gmtime(&t));
					ImGui::TextWrapped("%s", buffer);

					ImGui::TableNextColumn();
					ImGui::TextWrapped("%s", log.author.c_str());

					ImGui::TableNextColumn();
					ImGui::TextWrapped("%s", log.message.c_str());
				}
				ImGui::EndTable();
			}
		}
		if (chatLog.autoScroll)
		{
			ImGui::SetScrollHereY(1.f);
		}
		ImGui::EndChild();
	}
	ImGui::End();
	return true;
}
bool StreamDisplay::Draw::operator()(SDL_TextureWrapper &t)
{
	auto &texture = *t;
	if (texture == nullptr)
	{
		return true;
	}
	SetWindowMinSize(display.gui.getWindow());
	if (ImGui::Begin(display.source.serverName.c_str(), &display.visible, display.flags))
	{
		display.drawContextMenu();
		const auto max = ImGui::GetWindowContentRegionMax();
		const auto min = ImGui::GetWindowContentRegionMin();
		ImGui::Image(texture, ImVec2(max.x - min.x, max.y - min.y));
	}
	ImGui::End();
	return true;
}
bool StreamDisplay::Draw::operator()(Message::ServerLinks &links)
{
	SetWindowMinSize(display.gui.getWindow());
	if (ImGui::Begin(display.source.serverName.c_str(), &display.visible, display.flags))
	{
		display.drawContextMenu();
		if (ImGui::BeginTable("Available Servers", 3, TableFlags))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Connect");
			ImGui::TableHeadersRow();
			for (const auto &link : links)
			{
				{
					StringStream ss;
					ss << link.name << link.address << link.type;
					ImGui::PushID(ss.str().c_str());
				}

				ImGui::TableNextColumn();
				ImGui::Text("%s", link.name.c_str());

				{
					StringStream ss;
					ss << link.type;
					ImGui::TableNextColumn();
					ImGui::Text("%s", ss.str().c_str());
				}

				ImGui::TableNextColumn();
				if (ImGui::Button("Connect"))
				{
					display.gui.connect(link.address);
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
	return true;
}
void StreamDisplay::Draw::drawPoints(const List<float> &list, const float audioWidth, const float, const float minY,
									 const float maxY)
{
	SDL_Renderer *renderer = display.gui.getRenderer();

	SDL_SetRenderDrawColor(renderer, 0u, 0u, 255u, 255u);
	const float midY = (maxY + minY) * 0.5f;
	SDL_RenderDrawLineF(renderer, 0.f, midY, audioWidth, midY);

	List<SDL_FPoint> points;
	points.reserve(list.size());
	for (size_t i = 0; i < list.size(); ++i)
	{
		const float f = list[i];
		if (std::isinf(f) || std::isnan(f))
		{
			continue;
		}
		const float xpercent = (float)i / (float)list.size();
		SDL_FPoint point;
		point.x = audioWidth * xpercent;
		const float ypercent = ((f + 1.f) / 2.f);
		point.y = minY + (maxY - minY) * ypercent;
		points.push_back(point);
	}
	SDL_SetRenderDrawColor(renderer, 0u, 255u, 0u, 255u);
	SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
}
bool StreamDisplay::Draw::operator()(CheckAudio &t)
{
	if (!display.visible)
	{
		return true;
	}

	const auto func = [&t](const AudioSource &a) {
		a.getCurrentAudio(t.currentAudio);
		constexpr float speed = 0.75f;
		if (t.currentAudio.empty())
		{
			for (auto &f : t.left)
			{
				f = std::clamp(f * (1.f - speed), -1.f, 1.f);
			}
			for (auto &f : t.right)
			{
				f = std::clamp(f * (1.f - speed), -1.f, 1.f);
			}
		}
		else
		{
			const float *fdata = reinterpret_cast<const float *>(t.currentAudio.data());
			const size_t fsize = t.currentAudio.size() / sizeof(float);
			t.left.resize(fsize / 2, 0.f);
			t.right.resize(fsize / 2, 0.f);
			for (size_t i = 0; i < fsize - 1; i += 2)
			{
				const size_t half = i / 2;
				t.left[half] = std::clamp(lerp(fdata[i], t.left[half], speed), -1.f, 1.f);
				t.right[half] = std::clamp(lerp(fdata[i + 1], t.right[half], speed), -1.f, 1.f);
			}
		}
	};

	if (!display.gui.useAudio(display.getSource(), func))
	{
		// Maybe determine when this is an error and when the user removed the audio
		// (*logger)(Logger::Error) << "Audio is missing for stream: " << display.getSource() << std::endl;
		return false;
	}

	SDL_Renderer *renderer = display.gui.getRenderer();

	const int audioWidth = 2048;
	const int audioHeight = 512;
	auto &texture = *t.texture;
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

	if (!t.left.empty() && t.left.size() < INT32_MAX)
	{
		drawPoints(t.left, audioWidth, audioHeight, 0.f, audioHeight * 0.5f);
	}
	if (!t.right.empty() && t.right.size() < INT32_MAX)
	{
		drawPoints(t.right, audioWidth, audioHeight, audioHeight * 0.5f, audioHeight);
	}
	SDL_SetRenderTarget(renderer, target);

	return operator()(t.texture);
}
bool StreamDisplay::Draw::operator()(ByteList &)
{
	return true;
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
void StreamDisplay::ContextMenu::operator()(ByteList &)
{
}
void StreamDisplay::ContextMenu::operator()(String &s)
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
void StreamDisplay::ContextMenu::operator()(ChatLog &)
{
}
void StreamDisplay::ContextMenu::operator()(CheckAudio &a)
{
	operator()(a.texture);
}
void StreamDisplay::ContextMenu::operator()(Message::ServerLinks &)
{
}
void StreamDisplay::ContextMenu::operator()(SDL_TextureWrapper &w)
{
	auto &texture = *w;
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
			const time_t t = time(nullptr);
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