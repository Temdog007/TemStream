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

struct WindowSelector
{
	std::optional<Address> connectToServer;
	std::optional<std::vector<PeerInformation>> connectedPeers;
};

struct SDLContext
{
	bool loaded;

	SDLContext() : loaded()
	{
		if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		{
			fprintf(stderr, "SDL error: %s\n", SDL_GetError());
			loaded = false;
		}
		loaded = true;
	}

	~SDLContext()
	{
		SDL_Quit();
	}
};

struct TemStreamGui
{
	SDL_Window *window;
	SDL_Renderer *renderer;

	TemStreamGui() : window(NULL), renderer(NULL)
	{
	}

	~TemStreamGui()
	{
		close();
	}

	void close()
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
		SDL_DestroyWindow(window);
		window = NULL;
	}

	bool init()
	{
		close();

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
	const auto fd = address.makeSocket();
	std::cout << fd.has_value() << std::endl;
	if (!fd.has_value())
	{
		char buffer[KB(1)];
		snprintf(buffer, sizeof(buffer), "Failed to connect to server: %s:%d", address.hostname.c_str(), address.port);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Connection failed", buffer, gui.window);
		return;
	}
	if (!peerMap.add(address, messageList, *fd))
	{
		char buffer[KB(1)];
		snprintf(buffer, sizeof(buffer), "Already connected to server: %s:%d", address.hostname.c_str(), address.port);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Duplicate connection", buffer, gui.window);
		return;
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

void drawMenu(TemStreamGui &gui, WindowSelector &ws)
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
			if (ImGui::MenuItem("Connect to a server", "", &selected, !ws.connectToServer.has_value()))
			{
				ws.connectToServer = Address();
			}
			if (ImGui::MenuItem("Show current connections", "", &selected, !ws.connectedPeers.has_value()))
			{
				ws.connectedPeers = std::vector<PeerInformation>();
				peerMap.forPeer([&ws](const auto &pair) { ws.connectedPeers->push_back(pair.second.getInfo()); });
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	bool closed = false;
	if (ws.connectToServer.has_value())
	{
		if (ImGui::Begin("Connect to server", &closed))
		{
			ImGui::InputText("Hostname", &ws.connectToServer->hostname);
			ImGui::InputInt("Port", &ws.connectToServer->port);
			if (ImGui::Button("Connect"))
			{
				createClientPeer(gui, *ws.connectToServer);
				ws.connectToServer = std::nullopt;
			}
		}
		ImGui::End();
	}
	if (closed)
	{
		ws.connectToServer = std::nullopt;
	}

	closed = false;
	if (ws.connectedPeers.has_value())
	{
		if (ImGui::Begin("Connected peers", &closed))
		{
			for (const auto &info : *ws.connectedPeers)
			{
				ImGui::Text("Name: %s; Role: %s", info.name.c_str(), PeerTypeToString(info.type));
			}
		}
		ImGui::End();
	}
	if (closed)
	{
		ws.connectedPeers = std::nullopt;
	}
}

int TemStream::runGui()
{
	const SDLContext ctx;
	if (!ctx.loaded)
	{
		return EXIT_FAILURE;
	}

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
	WindowSelector ws;
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
		}

		// ImGui Rendering
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		drawMenu(gui, ws);

		ImGui::Render();

		// App Rendering
		SDL_SetRenderDrawColor(gui.renderer, 128u, 128u, 128u, 255u);
		SDL_RenderClear(gui.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		for (auto &pair : displays)
		{
			pair.second.draw(gui.renderer);
		}
		SDL_RenderPresent(gui.renderer);
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	return EXIT_SUCCESS;
}