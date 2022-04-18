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

bool CanHandleEvent(ImGuiIO &io, const SDL_Event &e);
const ImWchar MinCharacters = 0x1;
const ImWchar MaxCharacters = 0x1FFFF;

namespace TemStream
{
String32 allUTF32;
TemStreamGui::TemStreamGui(ImGuiIO &io)
	: connectToServer(), peerInfo({"User", false}), peerMutex(), outgoingPackets(), peer(nullptr), queryData(nullptr),
	  fontFiles(), io(io), fontSize(24.f), fontIndex(1), showLogs(false), showFont(false),
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

	if (peer != nullptr)
	{
		for (const auto &packet : outgoingPackets)
		{
			if (!(*peer)->sendPacket(packet))
			{
				peer = nullptr;
				break;
			}
		}
		if (!peer->readAndHandle(0))
		{
			peer = nullptr;
		}
	}
	outgoingPackets.clear();
}

void TemStreamGui::flush(MessagePackets &packets)
{
	LOCK(peerMutex);
	if (peer != nullptr)
	{
		peer->flush(packets);
	}
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
		logger->AddError("SDL error: %s\n", SDL_GetError());
		return false;
	}

	const int flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP;
	if (IMG_Init(flags) != flags)
	{
		logger->AddError("Image error: %s\n", IMG_GetError());
		return false;
	}

	window = SDL_CreateWindow("TemStream", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
							  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == nullptr)
	{
		logger->AddError("Failed to create window: %s\n", SDL_GetError());
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr)
	{
		logger->AddError("Failed to create renderer: %s\n", SDL_GetError());
		return false;
	}

	if (SDL_AddTimer(1, (SDL_TimerCallback)updatePeer, this) == 0)
	{
		logger->AddError("Failed to add timer: %s\n", SDL_GetError());
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
				ImGui::SliderInt("Font Type", &fontIndex, 0, io.Fonts->Fonts.size() - 1);
				ImGui::EndMenu();
			}
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

	if (showFont)
	{
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
		if (ImGui::Begin("Logs", &showLogs))
		{
			InMemoryLogger &mLogger = dynamic_cast<InMemoryLogger &>(*logger);
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

void TemStreamGui::sendPacket(const MessagePacket &packet, const bool handleLocally)
{
	LOCK(peerMutex);
	outgoingPackets.push_back(packet);
	if (handleLocally && peer != nullptr)
	{
		peer->addPacket(packet);
	}
}

void TemStreamGui::sendPackets(const MessagePackets &packets, const bool handleLocally)
{
	LOCK(peerMutex);
	outgoingPackets.insert(outgoingPackets.end(), packets.begin(), packets.end());
	if (handleLocally && peer != nullptr)
	{
		peer->addPackets(packets);
	}
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

int runGui()
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
	logger->AddInfo("ImGui v" IMGUI_VERSION "\n");

	if (!gui.init())
	{
		String total;
		InMemoryLogger &mLogger = dynamic_cast<InMemoryLogger &>(*logger);
		mLogger.viewLogs([&total](const Logger::Log &log) {
			total += log.second;
			total += '\n';
		});
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to start application", total.c_str(), gui.window);
		return EXIT_FAILURE;
	}

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

	gui.LoadFonts();

	MessagePackets messages;
	while (!appDone)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (!CanHandleEvent(io, event))
			{
				continue;
			}
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
					gui.sendPacket(*packet);
					delete packet;
				}
				break;
				case TemStreamEvent::SendMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					gui.sendPackets(*packets);
					delete packets;
				}
				break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		messages.clear();
		gui.flush(messages);
		for (const auto &m : messages)
		{
			auto iter = gui.displays.find(m.source);
			if (iter == gui.displays.end())
			{
				StreamDisplay display(gui, m.source);
				auto pair = gui.displays.emplace(m.source, std::move(display));
				if (!pair.second)
				{
					continue;
				}
				iter = pair.first;
			}

			if (!std::visit(iter->second, m.message))
			{
				gui.displays.erase(iter);
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
}
void TemStreamGuiLogger::Add(const Level lvl, const char *fmt, va_list args)
{
	if (lvl == Level::Error)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "An error has occurred. Check logs for more detail",
								 gui.window);
		gui.setShowLogs(true);
	}
	InMemoryLogger::Add(lvl, fmt, args);
}
} // namespace TemStream

bool CanHandleEvent(ImGuiIO &io, const SDL_Event &e)
{
	switch (e.type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_TEXTINPUT:
	case SDL_TEXTEDITING:
		return !io.WantCaptureKeyboard;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEWHEEL:
	case SDL_MOUSEMOTION:
	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
		return !io.WantCaptureMouse;
	default:
		return true;
	}
}