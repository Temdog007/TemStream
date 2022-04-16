#include <main.hpp>

using namespace TemStream;

MessageList messageList;
ClientPeerMap peerMap;

uint32_t updatePeerMap(uint32_t, void *)
{
	peerMap.update();
	return 1;
}

int TemStream::DefaultPort = 10000;

struct TemStreamGui
{
	std::optional<Address> connectToServer;
	std::optional<std::vector<PeerInformation>> connectedPeers;
	SDL_Window *window;
	SDL_Renderer *renderer;

	TemStreamGui() : window(NULL), renderer(NULL), connectToServer(), connectedPeers()
	{
	}

	~TemStreamGui()
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
		SDL_DestroyWindow(window);
		window = NULL;
		SDL_Quit();
	}

	bool init()
	{
		if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		{
			fprintf(stderr, "SDL error: %s\n", SDL_GetError());
			return false;
		}
		window = SDL_CreateWindow("TemStream", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
								  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		if (window == NULL)
		{
			fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
			return false;
		}

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (renderer == NULL)
		{
			fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
			return false;
		}

		return true;
	}
};

void createClientPeer(TemStreamGui &gui, const Address &address)
{
	printf("Connecting to server: %s:%d\n", address.hostname.c_str(), address.port);
	const auto fd = address.makeSocket();
	if (!fd.has_value())
	{
		char buffer[KB(1)];
		snprintf(buffer, sizeof(buffer), "Failed to connect to server: %s:%d", address.hostname.c_str(), address.port);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Connection failed", buffer, gui.window);
		return;
	}
	if (peerMap.add(address, messageList, *fd))
	{
		printf("Connected to server: %s:%d\n", address.hostname.c_str(), address.port);
	}
	else
	{
		::close(*fd);
		char buffer[KB(1)];
		snprintf(buffer, sizeof(buffer), "Already connected to server: %s:%d", address.hostname.c_str(), address.port);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Duplicate connection", buffer, gui.window);
	}
}

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

void drawMenu(TemStreamGui &gui)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			bool selected = false;
			if (ImGui::MenuItem("Exit", "", &selected))
			{
				TemStream::appDone = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Connections"))
		{
			bool selected = false;
			if (ImGui::MenuItem("Connect to a server", "", &selected, !gui.connectToServer.has_value()))
			{
				gui.connectToServer = Address();
			}
			if (ImGui::MenuItem("Show current connections", "", &selected, !gui.connectedPeers.has_value()))
			{
				gui.connectedPeers = std::vector<PeerInformation>();
				peerMap.forAllPeers([&gui](const auto &pair) { gui.connectedPeers->push_back(pair.second.getInfo()); });
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	bool opened = true;
	if (gui.connectToServer.has_value())
	{
		if (ImGui::Begin("Connect to server", &opened))
		{
			ImGui::InputText("Hostname", &gui.connectToServer->hostname);
			ImGui::InputInt("Port", &gui.connectToServer->port);
			if (ImGui::Button("Connect"))
			{
				createClientPeer(gui, *gui.connectToServer);
				gui.connectToServer = std::nullopt;
			}
		}
		ImGui::End();
	}
	if (!opened)
	{
		gui.connectToServer = std::nullopt;
	}

	opened = true;
	if (gui.connectedPeers.has_value())
	{
		if (ImGui::Begin("Connected peers", &opened))
		{
			for (const auto &info : *gui.connectedPeers)
			{
				ImGui::Text("Name: %s; IsServer: %s", info.name.c_str(), info.isServer ? "Yes" : "No");
			}
		}
		ImGui::End();
	}
	if (!opened)
	{
		gui.connectedPeers = std::nullopt;
	}
}

int TemStream::runGui()
{
	TemStreamGui gui;
	if (!gui.init())
	{
		return EXIT_FAILURE;
	}

	SDL_AddTimer(1, (SDL_TimerCallback)updatePeerMap, NULL);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

	std::unordered_map<MessageSource, StreamDisplay> displays;
	std::vector<MessagePacket> messages;
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
			default:
				break;
			}
		}

		messages.clear();
		messageList.flush(messages);
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

		drawMenu(gui);

		ImGui::Render();

		// App Rendering
		SDL_SetRenderDrawColor(gui.renderer, 128u, 128u, 128u, 255u);
		SDL_RenderClear(gui.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		for (auto &pair : displays)
		{
			pair.second.draw();
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