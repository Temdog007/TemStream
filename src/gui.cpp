#include <main.hpp>

#include "fonts/Cousine.cpp"
#include "fonts/DroidSans.cpp"
#include "fonts/Karla.cpp"
#include "fonts/ProggyClean.cpp"
#include "fonts/ProggyTiny.cpp"
#include "fonts/Roboto.cpp"
#include "fonts/Ubuntuu.cpp"

bool CanHandleEvent(ImGuiIO &io, const SDL_Event &e);

namespace TemStream
{
int DefaultPort = 10000;

TemStreamGui::TemStreamGui()
	: connectToServer(), peer(nullptr), peerMutex(), fontIndex(1), window(nullptr), renderer(nullptr)
{
}

TemStreamGui::~TemStreamGui()
{
	SDL_DestroyRenderer(renderer);
	renderer = nullptr;
	SDL_DestroyWindow(window);
	window = nullptr;
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
		if (!peer->readAndHandle(0))
		{
			peer = nullptr;
		}
	}
}

void TemStreamGui::flush(MessagePackets &packets)
{
	std::lock_guard<std::mutex> guard(peerMutex);
	if (peer != nullptr)
	{
		peer->flush(packets);
	}
}

void TemStreamGui::pushFont(ImGuiIO &io)
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
	printf("Connecting to server: %s:%d\n", address.hostname.c_str(), address.port);
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
	printf("Connected to server: %s:%d\n", address.hostname.c_str(), address.port);
	return true;
}

void TemStreamGui::draw(ImGuiIO &io)
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
		ImGui::Separator();
		if (ImGui::BeginMenu("View"))
		{
			ImGui::SliderInt("Font", &fontIndex, 0, io.Fonts->Fonts.size() - 1);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Connections"))
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
					std::lock_guard<std::mutex> guard(peerMutex);
					const auto &info = peer->getInfo();
					ImGui::TextColored(Colors::Yellow, "Name: %s\n", info.name.c_str());

					const auto &addr = peer->getAddress();
					ImGui::TextColored(Colors::Yellow, "Address: %s:%d\n", addr.hostname.c_str(), addr.port);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	bool opened = true;
	if (connectToServer.has_value())
	{
		if (ImGui::Begin("Connect to server", &opened))
		{
			ImGui::InputText("Hostname", &connectToServer->hostname);
			ImGui::InputInt("Port", &connectToServer->port);
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
}

bool TemStreamGui::isConnected()
{
	std::lock_guard<std::mutex> guard(peerMutex);
	return peer != nullptr;
}

int runGui()
{
	TemStreamGui gui;
	if (!gui.init())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed", "Failed to start app", NULL);
		return EXIT_FAILURE;
	}

	puts("ImGui v" IMGUI_VERSION);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

	const float fontSize = 36.f;
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)Cousine_compressed_data, Cousine_compressed_size, fontSize);
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)DroidSans_compressed_data, DroidSans_compressed_size, fontSize);
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)Karla_compressed_data, Karla_compressed_size, fontSize);
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)ProggyClean_compressed_data, ProggyClean_compressed_size,
											 fontSize);
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)ProggyTiny_compressed_data, ProggyTiny_compressed_size, fontSize);
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)Roboto_compressed_data, Roboto_compressed_size, fontSize);
	io.Fonts->AddFontFromMemoryCompressedTTF((void *)Ubuntuu_compressed_data, Ubuntuu_compressed_size, fontSize);

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

		gui.pushFont(io);

		gui.draw(io);

		ImGui::PopFont();

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