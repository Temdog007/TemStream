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
const float fontSize = 36.f;

namespace TemStream
{
TemStreamGui::TemStreamGui(ImGuiIO &io)
	: connectToServer(), pendingFile(std::nullopt), peerInfo({"User", false}), peerMutex(), outgoingPackets(),
	  peer(nullptr), queryData(nullptr), io(io), fontIndex(1), showLogs(false), window(nullptr), renderer(nullptr)
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
		*logger << Logger::Error << "Failed to connect to server: " << address << std::endl;
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
			if (ImGui::MenuItem("Open", "", nullptr, !pendingFile.has_value()))
			{
				pendingFile = "";
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
			ImGui::SliderInt("Font", &fontIndex, 0, io.Fonts->Fonts.size() - 1);
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

	if (pendingFile.has_value())
	{
		bool opened = true;
		if (ImGui::Begin("Enter file path", &opened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputText("File path", &*pendingFile);
			if (ImGui::Button("Load"))
			{
				char *ptr = reinterpret_cast<char *>(SDL_malloc(pendingFile->size() + 1));
				strcpy(ptr, pendingFile->c_str());
				opened = false;

				SDL_Event e;
				e.type = SDL_DROPFILE;
				e.drop.file = ptr;
				e.drop.timestamp = SDL_GetTicks();
				e.drop.windowID = SDL_GetWindowID(window);
				if (SDL_PushEvent(&e) != 1)
				{
					fprintf(stderr, "Failed to push event: %s\n", SDL_GetError());
					SDL_free(ptr);
				}
			}
		}
		ImGui::End();
		if (!opened)
		{
			pendingFile = std::nullopt;
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

bool TemStreamGui::handleFile(const char *filename)
{
	if (queryData != nullptr)
	{
		if (queryData->handleDropFile(filename))
		{
			queryData->execute();
			queryData = nullptr;
		}
	}
	return true;
}

bool TemStreamGui::handleText(const char *text)
{
	if (queryData != nullptr)
	{
		if (queryData->handleDropText(text))
		{
			queryData->execute();
			queryData = nullptr;
		}
	}
	return true;
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

int runGui()
{
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

	for (size_t i = 0; i < IM_ARRAYSIZE(Fonts); ++i)
	{
		io.Fonts->AddFontFromMemoryCompressedTTF(Fonts[i], FontSizes[i], fontSize);
	}

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
			case SDL_DROPTEXT:
				if (!gui.handleText(event.drop.file))
				{
					*logger << Logger::Error << "Failed to handle dropped text" << std::endl;
				}
				SDL_free(event.drop.file);
				break;
			case SDL_DROPFILE:
				logger->AddInfo("Dropped file: %s\n", event.drop.file);
				if (!gui.handleFile(event.drop.file))
				{
					*logger << Logger::Error << "Failed to handle dropped file: " << event.drop.file << std::endl;
				}
				SDL_free(event.drop.file);
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