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
	: connectToServer(), pendingFile(std::nullopt), peerInfo({"User", false}), peerMutex(), peer(nullptr),
	  queryData(nullptr), io(io), fontIndex(1), window(nullptr), renderer(nullptr)
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
	std::lock_guard<std::mutex> guard(peerMutex);

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
	std::lock_guard<std::mutex> guard(peerMutex);
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
		fprintf(stderr, "SDL error: %s\n", SDL_GetError());
		return false;
	}

	const int flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP;
	if (IMG_Init(flags) != flags)
	{
		fprintf(stderr, "Image error: %s\n", IMG_GetError());
		return false;
	}

	window = SDL_CreateWindow("TemStream", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
							  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == nullptr)
	{
		fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr)
	{
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return false;
	}

	if (SDL_AddTimer(1, (SDL_TimerCallback)updatePeer, this) == 0)
	{
		fprintf(stderr, "Failed to add timer: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

bool TemStreamGui::connect(const Address &address)
{
	std::cout << "Connecting to server: " << address << std::endl;
	auto s = address.makeTcpSocket();
	if (s == nullptr)
	{
		char buffer[KB(1)];
		snprintf(buffer, sizeof(buffer), "Failed to connect to server: %s:%d", address.hostname.c_str(), address.port);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Connection failed", buffer, window);
		return false;
	}

	std::lock_guard<std::mutex> guard(peerMutex);
	peer = std::make_unique<ClientPeer>(address, std::move(s));
	std::cout << "Connected to server: " << address << std::endl;
	MessagePacket packet;
	packet.message = peerInfo;
	return (*peer)->sendPacket(packet);
}

void TemStreamGui::draw()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			bool selected = false;
			if (!pendingFile.has_value() && ImGui::MenuItem("Open", "", &selected))
			{
				pendingFile = "";
			}
			if (ImGui::MenuItem("Exit", "", &selected))
			{
				TemStream::appDone = true;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("View"))
		{
			ImGui::SliderInt("Font", &fontIndex, 0, io.Fonts->Fonts.size() - 1);
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("Connection"))
		{
			const bool connectedToServer = isConnected();
			bool selected = false;
			if (ImGui::MenuItem("Connect to server", "", &selected, !connectedToServer))
			{
				connectToServer = Address();
			}
			selected = false;
			if (ImGui::MenuItem("Disconnect from server", "", &selected, connectedToServer))
			{
				connectToServer = std::nullopt;
				std::lock_guard<std::mutex> guard(peerMutex);
				peer = nullptr;
			}
			else
			{
				ImGui::Separator();
				if (connectedToServer)
				{
					ImGui::TextColored(Colors::Lime, "Logged in as: %s\n", peerInfo.name.c_str());

					{
						std::lock_guard<std::mutex> guard(peerMutex);
						const auto &info = peer->getInfo();
						ImGui::TextColored(Colors::Yellow, "Server: %s\n", info.name.c_str());

						const auto &addr = peer->getAddress();
						ImGui::TextColored(Colors::Yellow, "Address: %s:%d\n", addr.hostname.c_str(), addr.port);
					}

					ImGui::Separator();
					if (queryData == nullptr)
					{
						if (ImGui::MenuItem("Send Text", "", &selected))
						{
							queryData = std::make_unique<QueryText>(*this);
						}
						if (ImGui::MenuItem("Send Image", "", &selected))
						{
							queryData = std::make_unique<QueryImage>(*this);
						}
						if (ImGui::MenuItem("Send Video", "", &selected))
						{
							queryData = std::make_unique<QueryVideo>(*this);
						}
						if (ImGui::MenuItem("Send Audio", "", &selected))
						{
							queryData = std::make_unique<QueryAudio>(*this);
						}
					}
				}
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();

		ImGui::EndMainMenuBar();
	}
	bool opened = true;
	if (connectToServer.has_value())
	{
		if (ImGui::Begin("Connect to server", &opened, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::InputText("Hostname", &connectToServer->hostname);
			ImGui::InputInt("Port", &connectToServer->port);
			ImGui::InputText("Name", &peerInfo.name);
			if (ImGui::Button("Connect"))
			{
				connect(*connectToServer);
				connectToServer = std::nullopt;
			}
		}
		ImGui::End();
	}
	if (!opened)
	{
		connectToServer = std::nullopt;
	}

	if (pendingFile.has_value())
	{
		opened = true;
		if (ImGui::Begin("Enter file path", &opened, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::InputText("File path", &*pendingFile);
			ImGui::SameLine();
			if (ImGui::Button("Load"))
			{
				char *ptr = reinterpret_cast<char *>(SDL_malloc(pendingFile->size() + 1));
				strcpy(ptr, pendingFile->c_str());
				pendingFile = std::nullopt;

				SDL_Event e;
				e.type = SDL_DROPFILE;
				e.drop.file = ptr;
				e.drop.timestamp = SDL_GetTicks();
				e.drop.windowID = SDL_GetWindowID(window);
				if (SDL_PushEvent(&e) != 0)
				{
					SDL_free(ptr);
				}
			}
		}
		ImGui::End();
	}

	bool sendingData = queryData != nullptr;
	if (sendingData)
	{
		if (ImGui::Begin("Send data to server", &sendingData, ImGuiWindowFlags_NoCollapse))
		{
			if (queryData->draw())
			{
				sendPackets(queryData->getPackets());
				sendingData = false;
			}
		}
		ImGui::End();
	}
	if (!sendingData)
	{
		queryData = nullptr;
	}
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
			sendPackets(queryData->getPackets());
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
			sendPackets(queryData->getPackets());
			queryData = nullptr;
		}
	}
	return true;
}

void TemStreamGui::sendPacket(const MessagePacket &packet)
{
	std::lock_guard<std::mutex> guard(peerMutex);
	outgoingPackets.emplace_back(packet);
}

void TemStreamGui::sendPackets(const MessagePackets &packets)
{
	std::lock_guard<std::mutex> guard(peerMutex);
	outgoingPackets.insert(outgoingPackets.end(), packets.begin(), packets.end());
}

bool TemStreamGui::isConnected()
{
	std::lock_guard<std::mutex> guard(peerMutex);
	return peer != nullptr;
}

int runGui()
{
	puts("ImGui v" IMGUI_VERSION);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();

	ImGui::StyleColorsDark();

	TemStreamGui gui(io);
	if (!gui.init())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed", "Failed to start app", gui.window);
		return EXIT_FAILURE;
	}

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

	for (size_t i = 0; i < IM_ARRAYSIZE(Fonts); ++i)
	{
		io.Fonts->AddFontFromMemoryCompressedTTF(Fonts[i], FontSizes[i], fontSize);
	}

	std::unordered_map<MessageSource, StreamDisplay> displays;
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
			for (auto &display : displays)
			{
				display.second.handleEvent(event);
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
					SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dropped text error",
											 "Failed to handle dropped text", gui.window);
				}
				SDL_free(event.drop.file);
				break;
			case SDL_DROPFILE:
				if (!gui.handleFile(event.drop.file))
				{
					char buffer[KB(2)];
					snprintf(buffer, sizeof(buffer), "Failed to handle dropped file: %s", event.drop.file);
					SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dropped text error", buffer, gui.window);
				}
				SDL_free(event.drop.file);
				break;
			default:
				break;
			}
		}

		messages.clear();
		gui.flush(messages);
		for (const auto &m : messages)
		{
			auto iter = displays.find(m.source);
			if (iter == displays.end())
			{
				StreamDisplay display(gui.renderer, m.source);
				auto pair = displays.emplace(m.source, std::move(display));
				if (!pair.second)
				{
					continue;
				}
				iter = pair.first;
			}

			if (!std::visit(iter->second, m.message))
			{
				displays.erase(iter);
			}
		}

		// ImGui Rendering
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		gui.pushFont();

		gui.draw();

		for (auto &pair : displays)
		{
			pair.second.draw(true);
		}

		ImGui::PopFont();

		ImGui::Render();

		// App Rendering
		SDL_SetRenderDrawColor(gui.renderer, 128u, 128u, 128u, 255u);
		SDL_RenderClear(gui.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		for (auto &pair : displays)
		{
			pair.second.draw(false);
		}
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