#include <main.hpp>

#include "fonts/Cousine.cpp"
#include "fonts/DroidSans.cpp"
#include "fonts/Karla.cpp"
#include "fonts/ProggyClean.cpp"
#include "fonts/ProggyTiny.cpp"
#include "fonts/Roboto.cpp"
#include "fonts/Ubuntuu.cpp"

const void *Fonts[]{Cousine_compressed_data,	 DroidSans_compressed_data,	 Karla_compressed_data,
					ProggyClean_compressed_data, ProggyTiny_compressed_data, Roboto_compressed_data,
					Ubuntuu_compressed_data};
const unsigned int FontSizes[]{Cousine_compressed_size,		DroidSans_compressed_size,	Karla_compressed_size,
							   ProggyClean_compressed_size, ProggyTiny_compressed_size, Roboto_compressed_size,
							   Ubuntuu_compressed_size};

const ImWchar MinCharacters = 0x1;
const ImWchar MaxCharacters = 0x1FFFF;

namespace TemStream
{
String32 allUTF32;
TemStreamGui::TemStreamGui(ImGuiIO &io)
	: connectToServer(), peerInfo({"User", false}), peerMutex(), peer(nullptr), queryData(nullptr), fontFiles(), io(io),
	  fontSize(24.f), fontIndex(1), showLogs(false), showAudio(false), showFont(false),
	  streamDisplayFlags(ImGuiWindowFlags_None), window(nullptr), renderer(nullptr)
{
}

TemStreamGui::~TemStreamGui()
{
	SDL_DestroyRenderer(renderer);
	renderer = nullptr;
	SDL_DestroyWindow(window);
	window = nullptr;
	IMG_Quit();
	SDL_Quit();
}

uint32_t updatePeer(uint32_t interval, TemStreamGui *gui)
{
	gui->update();
	return interval;
}

void TemStreamGui::update()
{
	LOCK(peerMutex);

	if (peer == nullptr)
	{
		return;
	}

	if (!peer->readAndHandle(0))
	{
		peer = nullptr;
		(*logger)(Logger::Trace) << "TemStreamGui::update: error" << std::endl;
		onDisconnect();
	}
	for (const auto &a : audio)
	{
		if (!a.second->encodeAndSendAudio(*peer))
		{
			peer = nullptr;
			(*logger)(Logger::Trace) << "TemStreamGui::update: error" << std::endl;
			onDisconnect();
		}
	}
}

void TemStreamGui::onDisconnect()
{
	(*logger)(Logger::Error) << "Lost connection to server" << std::endl;
}

void TemStreamGui::pushFont()
{
	if (fontIndex >= io.Fonts->Fonts.size())
	{
		fontIndex = 0;
	}
	ImGui::PushFont(io.Fonts->Fonts[fontIndex]);
}

bool TemStreamGui::init()
{
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		(*logger)(Logger::Error) << "SDL error: " << SDL_GetError() << std::endl;
		return false;
	}

	const int flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP;
	if (IMG_Init(flags) != flags)
	{
		(*logger)(Logger::Error) << "Image error: " << IMG_GetError() << std::endl;
		return false;
	}

	window = SDL_CreateWindow("TemStream", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
							  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == nullptr)
	{
		(*logger)(Logger::Error) << "Failed to create window: " << SDL_GetError() << std::endl;
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr)
	{
		(*logger)(Logger::Error) << "Failed to create renderer: " << SDL_GetError() << std::endl;
		return false;
	}

	if (SDL_AddTimer(1, (SDL_TimerCallback)updatePeer, this) == 0)
	{
		(*logger)(Logger::Error) << "Failed to add timer: " << SDL_GetError() << std::endl;
		return false;
	}

	return true;
}

bool TemStreamGui::connect(const Address &address)
{
	*logger << "Connecting to server: " << address << std::endl;
	auto s = address.makeTcpSocket();
	if (s == nullptr)
	{
		(*logger)(Logger::Error) << "Failed to connect to server: " << address << std::endl;
		return false;
	}

	LOCK(peerMutex);
	peer = std::make_unique<ClientPeer>(address, std::move(s));
	*logger << "Connected to server: " << address << std::endl;
	MessagePacket packet;
	packet.message = peerInfo;
	return (*peer)->sendPacket(packet);
}

ImVec2 TemStreamGui::drawMainMenuBar(const bool connectedToServer)
{
	ImVec2 size;
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", "", nullptr, false))
			{
			}
			if (ImGui::MenuItem("Exit", "", nullptr))
			{
				TemStream::appDone = true;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::BeginMenu("Stream Display Flags"))
			{
				bool showTitleBar = (streamDisplayFlags & ImGuiWindowFlags_NoTitleBar) == 0;
				if (ImGui::Checkbox("Show Title Bar", &showTitleBar))
				{
					if (showTitleBar)
					{
						streamDisplayFlags &= ~ImGuiWindowFlags_NoTitleBar;
					}
					else
					{
						streamDisplayFlags |= ImGuiWindowFlags_NoTitleBar;
					}
				}
				bool movable = (streamDisplayFlags & ImGuiWindowFlags_NoMove) == 0;
				if (ImGui::Checkbox("Movable", &movable))
				{
					if (movable)
					{
						streamDisplayFlags &= ~ImGuiWindowFlags_NoMove;
					}
					else
					{
						streamDisplayFlags |= ImGuiWindowFlags_NoMove;
					}
				}
				bool resizable = (streamDisplayFlags & ImGuiWindowFlags_NoResize) == 0;
				if (ImGui::Checkbox("Resizable", &resizable))
				{
					if (resizable)
					{
						streamDisplayFlags &= ~ImGuiWindowFlags_NoResize;
					}
					else
					{
						streamDisplayFlags |= ImGuiWindowFlags_NoResize;
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Font"))
			{
				ImGui::Checkbox("Font Display", &showFont);
				int value = fontSize;
				if (ImGui::SliderInt("Font Size", &value, 12, 96, "%d",
									 ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput))
				{
					fontSize = value;
				}
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					SDL_Event e;
					e.type = SDL_USEREVENT;
					e.user.code = TemStreamEvent::ReloadFont;
					tryPushEvent(e);
				}
				ImGui::SliderInt("Font Type", &fontIndex, 0, io.Fonts->Fonts.size() - 1, "%d",
								 ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
				ImGui::EndMenu();
			}
			ImGui::Checkbox("Audio", &showAudio);
			ImGui::Checkbox("Logs", &showLogs);
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("Connect"))
		{
			if (ImGui::MenuItem("Connect to server", "", nullptr, !connectedToServer))
			{
				connectToServer = Address();
			}
			if (ImGui::MenuItem("Disconnect from server", "", nullptr, connectedToServer))
			{
				connectToServer = std::nullopt;
				LOCK(peerMutex);
				peer = nullptr;
			}
			else
			{
				ImGui::Separator();
				if (connectedToServer)
				{
					ImGui::TextColored(Colors::Lime, "Logged in as: %s\n", peerInfo.name.c_str());

					{
						LOCK(peerMutex);
						const auto &info = peer->getInfo();
						ImGui::TextColored(Colors::Yellow, "Server: %s\n", info.name.c_str());

						const auto &addr = peer->getAddress();
						ImGui::TextColored(Colors::Yellow, "Address: %s:%d\n", addr.hostname.c_str(), addr.port);
					}

					ImGui::Separator();
					if (queryData == nullptr)
					{
						if (ImGui::MenuItem("Send Data", "", nullptr))
						{
							queryData = std::make_unique<QueryText>(*this);
						}
					}
				}
			}
			ImGui::EndMenu();
		}

		if (!displays.empty())
		{
			ImGui::Separator();
			if (ImGui::BeginMenu("Displays"))
			{
				std::array<char, KB(1)> buffer;
				for (auto &pair : displays)
				{
					auto &display = pair.second;
					const auto &source = display.getSource();
					source.print(buffer);
					ImGui::Checkbox(buffer.data(), &display.visible);
				}
				ImGui::EndMenu();
			}
		}

		size = ImGui::GetWindowSize();
		ImGui::EndMainMenuBar();
	}
	return size;
}

void TemStreamGui::draw()
{
	const bool connectedToServer = isConnected();

	const auto size = drawMainMenuBar(connectedToServer);

	if (ImGui::Begin("##afdasy4", nullptr,
					 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
						 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
						 ImGuiWindowFlags_NoScrollbar))
	{
		ImColor color;
		const char *text;
		if (connectedToServer)
		{
			color = Colors::Lime;
			text = "Connected";
		}
		else
		{
			color = Colors::Red;
			text = "Disconnected";
		}
		const auto textSize = ImGui::CalcTextSize(text);
		auto draw = ImGui::GetForegroundDrawList();
		const float x = size.x - (textSize.x + size.x * 0.01f);
		const float radius = size.y * 0.5f;
		draw->AddCircleFilled(ImVec2(x - radius, radius), radius, color);
		draw->AddText(ImVec2(x, 0), color, text);
	}
	ImGui::End();

	if (!audio.empty() && showAudio)
	{
		if (ImGui::Begin("Audio", &showAudio, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (ImGui::BeginTable("Audio", 6, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Device Name");
				ImGui::TableSetupColumn("Source/Destination");
				ImGui::TableSetupColumn("Recording");
				ImGui::TableSetupColumn("Volume");
				ImGui::TableSetupColumn("Muted");
				ImGui::TableSetupColumn("Stop");
				ImGui::TableHeadersRow();

				std::array<char, KB(1)> buffer;
				for (auto iter = audio.begin(); iter != audio.end();)
				{
					auto &a = iter->second;

					ImGui::TableNextColumn();
					ImGui::Text("%s", a->getName().c_str());

					iter->first.print(buffer);
					ImGui::TableNextColumn();
					ImGui::Text("%s", buffer.data());

					const bool recording = a->isRecording();
					if (recording)
					{
						ImGui::TableNextColumn();
						ImGui::Text("Yes");

						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::Yellow, "N\\A");
					}
					else
					{
						ImGui::TableNextColumn();
						ImGui::Text("No");

						float v = a->getVolume();
						ImGui::TableNextColumn();
						if (ImGui::SliderFloat("Volume", &v, 0.f, 1.f))
						{
							a->setVolume(v);
						}
					}

					ImGui::TableNextColumn();
					const bool isMuted = a->isMuted();
					if (isMuted)
					{
						if (ImGui::Button("Yes"))
						{
							a->setMuted(false);
							a->clearAudio();
						}
					}
					else
					{
						if (ImGui::Button("No"))
						{
							a->setMuted(true);
							a->clearAudio();
						}
					}

					ImGui::TableNextColumn();
					if (ImGui::Button("Stop"))
					{
						iter = audio.erase(iter);
					}
					else
					{
						++iter;
					}
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (showFont)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("Font", &showFont))
		{
			std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
			const String s = cvt.to_bytes(allUTF32);
			const size_t size = 1000;
			for (size_t i = 0; i < s.size(); i += size)
			{
				if (ImGui::BeginChild(1 + (i / size)))
				{
					const String k(s.begin() + i, (i + size > s.size()) ? s.end() : (s.begin() + i + size));
					ImGui::TextWrapped("%s", k.c_str());
				}
				ImGui::EndChild();
			}
		}
		ImGui::End();
	}

	if (connectToServer.has_value())
	{
		bool opened = true;
		if (ImGui::Begin("Connect to server", &opened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputText("Hostname", &connectToServer->hostname);
			ImGui::InputInt("Port", &connectToServer->port);
			ImGui::InputText("Name", &peerInfo.name);
			if (ImGui::Button("Connect"))
			{
				connect(*connectToServer);
				opened = false;
			}
		}
		ImGui::End();
		if (!opened)
		{
			connectToServer = std::nullopt;
		}
	}

	if (queryData != nullptr)
	{
		bool opened = true;
		if (ImGui::Begin("Send data to server", &opened,
						 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			static const char *options[]{"Text", "Image", "Audio", "Video"};
			int selected = getSelectedQuery();
			if (ImGui::Combo("Data Type", &selected, options, IM_ARRAYSIZE(options)))
			{
				switch (selected)
				{
				case 0:
					queryData = std::make_unique<QueryText>(*this);
					break;
				case 1:
					queryData = std::make_unique<QueryImage>(*this);
					break;
				case 2:
					queryData = std::make_unique<QueryAudio>(*this);
					break;
				case 3:
					queryData = std::make_unique<QueryVideo>(*this);
					break;
				default:
					break;
				}
			}
			if (queryData->draw())
			{
				queryData->execute();
				opened = false;
			}
		}
		ImGui::End();
		if (!opened)
		{
			queryData = nullptr;
		}
	}

	if (showLogs)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("Logs", &showLogs))
		{
			InMemoryLogger &mLogger = static_cast<InMemoryLogger &>(*logger);
			mLogger.viewLogs([](const Logger::Log &log) {
				switch (log.first)
				{
				case Logger::Trace:
					ImGui::TextColored(Colors::Cyan, "%s", log.second.c_str());
					break;
				case Logger::Info:
					ImGui::TextColored(Colors::White, "%s", log.second.c_str());
					break;
				case Logger::Warning:
					ImGui::TextColored(Colors::Yellow, "%s", log.second.c_str());
					break;
				case Logger::Error:
					ImGui::TextColored(Colors::Red, "%s", log.second.c_str());
					break;
				default:
					break;
				}
			});
		}
		ImGui::End();
	}
}

int TemStreamGui::getSelectedQuery() const
{
	if (queryData == nullptr)
	{
		return -1;
	}
	if (dynamic_cast<QueryText *>(queryData.get()) != nullptr)
	{
		return 0;
	}
	if (dynamic_cast<QueryImage *>(queryData.get()) != nullptr)
	{
		return 1;
	}
	if (dynamic_cast<QueryAudio *>(queryData.get()) != nullptr)
	{
		return 2;
	}
	if (dynamic_cast<QueryVideo *>(queryData.get()) != nullptr)
	{
		return 3;
	}
	return -1;
}

const char *getExtension(const char *filename)
{
	size_t len = strlen(filename);
	const char *c = filename + (len - 1);
	for (; *c != *filename; --c)
	{
		if (*c == '.')
		{
			return c + 1;
		}
	}
	return filename;
}

void TemStreamGui::sendPacket(MessagePacket &&packet, const bool handleLocally)
{
	LOCK(peerMutex);
	if (peer == nullptr)
	{
		return;
	}
	if (!(*peer)->sendPacket(packet))
	{
		peer = nullptr;
		(*logger)(Logger::Trace) << "TemStreamGui::sendPacket: error" << std::endl;
		onDisconnect();
		return;
	}
	if (handleLocally)
	{
		peer->addPacket(std::move(packet));
	}
}

void TemStreamGui::sendPackets(MessagePackets &&packets, const bool handleLocally)
{
	LOCK(peerMutex);
	if (peer == nullptr)
	{
		return;
	}
	for (const auto &packet : packets)
	{
		if (!(*peer)->sendPacket(packet))
		{
			peer = nullptr;
			(*logger)(Logger::Trace) << "TemStreamGui::sendPackets: error" << std::endl;
			onDisconnect();
			return;
		}
	}
	if (handleLocally)
	{
		peer->addPackets(std::move(packets));
	}
}

bool TemStreamGui::addAudio(std::shared_ptr<Audio> ptr)
{
	auto pair = audio.emplace(ptr->getSource(), std::move(ptr));
	return pair.second;
}

std::shared_ptr<Audio> TemStreamGui::getAudio(const MessageSource &source) const
{
	auto iter = audio.find(source);
	if (iter == audio.end())
	{
		return nullptr;
	}

	return iter->second;
}

bool TemStreamGui::isConnected()
{
	LOCK(peerMutex);
	return peer != nullptr;
}

void TemStreamGui::LoadFonts()
{
	io.Fonts->Clear();
	static ImWchar ranges[] = {MinCharacters, MaxCharacters, 0};
	static ImFontConfig cfg;
	cfg.OversampleH = cfg.OversampleV = 1;
	cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
	for (size_t i = 0; i < IM_ARRAYSIZE(Fonts); ++i)
	{
		io.Fonts->AddFontFromMemoryCompressedTTF(Fonts[i], FontSizes[i], fontSize, &cfg, ranges);
	}
	for (const auto &file : fontFiles)
	{
		io.Fonts->AddFontFromFileTTF(file.c_str(), fontSize);
	}
	io.Fonts->Build();
	ImGui_ImplSDLRenderer_DestroyFontsTexture();
}

void TemStreamGui::handleMessage(MessagePacket &&m)
{
	auto iter = displays.find(m.source);
	if (iter == displays.end())
	{
		StreamDisplay display(*this, m.source);
		auto pair = displays.emplace(m.source, std::move(display));
		if (!pair.second)
		{
			LOCK(peerMutex);
			(*logger)(Logger::Trace) << "TemStreamGui::handleMessage: error" << std::endl;
			peer = nullptr;
			onDisconnect();
			return;
		}
		iter = pair.first;
	}

	if (!std::visit(iter->second, std::move(m.message)))
	{
		displays.erase(iter);
	}
}

int TemStreamGui::run()
{
	for (ImWchar c = MinCharacters; c < MaxCharacters; ++c)
	{
		allUTF32 += c;
	}
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();

	ImGui::StyleColorsDark();

	TemStreamGui gui(io);
	logger = std::make_unique<TemStreamGuiLogger>(gui);
	initialLogs();
	(*logger)(Logger::Info) << "ImGui v" IMGUI_VERSION << std::endl;

	if (!gui.init())
	{
		return EXIT_FAILURE;
	}

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

	gui.LoadFonts();

	while (!appDone)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.type)
			{
			case SDL_QUIT:
				appDone = true;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(gui.window))
				{
					appDone = true;
				}
				break;
			case SDL_DROPTEXT: {
				const size_t size = strlen(event.drop.file);
				String s(event.drop.file, event.drop.file + size);
				gui.queryData = std::make_unique<QueryText>(gui, std::move(s));
				SDL_free(event.drop.file);
			}
			break;
			case SDL_DROPFILE: {
				if (isTTF(event.drop.file))
				{
					*logger << "Adding new font: " << event.drop.file << std::endl;
					gui.fontFiles.emplace_back(event.drop.file);
					gui.fontIndex = IM_ARRAYSIZE(Fonts) + gui.fontFiles.size() - 1;
					gui.LoadFonts();
				}
				else if (isImage(event.drop.file))
				{
					const size_t size = strlen(event.drop.file);
					String s(event.drop.file, event.drop.file + size);
					gui.queryData = std::make_unique<QueryImage>(gui, std::move(s));
				}
				else
				{
					std::ifstream file(event.drop.file);
					String s((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
					gui.queryData = std::make_unique<QueryText>(gui, std::move(s));
				}
				SDL_free(event.drop.file);
			}
			break;
			case SDL_USEREVENT:
				switch (event.user.code)
				{
				case TemStreamEvent::SendSingleMessagePacket: {
					MessagePacket *packet = reinterpret_cast<MessagePacket *>(event.user.data1);
					gui.sendPacket(std::move(*packet));
					delete packet;
				}
				break;
				case TemStreamEvent::HandleMessagePacket: {
					MessagePacket *packet = reinterpret_cast<MessagePacket *>(event.user.data1);
					gui.handleMessage(std::move(*packet));
					delete packet;
				}
				break;
				case TemStreamEvent::SendMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					gui.sendPackets(std::move(*packets));
					delete packets;
				}
				break;
				case TemStreamEvent::HandleMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					const auto start = std::make_move_iterator(packets->begin());
					const auto end = std::make_move_iterator(packets->end());
					std::for_each(start, end, [&gui](MessagePacket &&packet) { gui.handleMessage(std::move(packet)); });
					delete packets;
				}
				break;
				case TemStreamEvent::ReloadFont:
					gui.LoadFonts();
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		gui.pushFont();

		gui.draw();

		for (auto iter = gui.displays.begin(); iter != gui.displays.end();)
		{
			if (iter->second.draw())
			{
				++iter;
			}
			else
			{
				iter = gui.displays.erase(iter);
			}
		}

		ImGui::PopFont();

		ImGui::Render();

		SDL_SetRenderDrawColor(gui.renderer, 128u, 128u, 128u, 255u);
		SDL_RenderClear(gui.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(gui.renderer);
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	while (runningThreads > 0)
	{
		SDL_Delay(100);
	}
	return EXIT_SUCCESS;
}
TemStreamGuiLogger::TemStreamGuiLogger(TemStreamGui &gui) : gui(gui)
{
}
TemStreamGuiLogger::~TemStreamGuiLogger()
{
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "TemStream_log_%zu.txt", time(NULL));
	std::ofstream file(buffer);
	this->viewLogs([&file](const Log &log) { file << log.first << ": " << log.second; });
}
void TemStreamGuiLogger::Add(const Level level, const String &s, const bool b)
{
	checkError(level);
	InMemoryLogger::Add(level, s, b);
}
void TemStreamGuiLogger::checkError(const Level level)
{
	if (level == Level::Error && !gui.isShowingLogs())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "An error has occurred. Check logs for more detail",
								 gui.window);
		gui.setShowLogs(true);
	}
}
} // namespace TemStream
