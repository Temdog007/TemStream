#include <main.hpp>

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

void drawMenu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			bool selected = false;
			if (ImGui::MenuItem("Exit", "Alt+F4", &selected))
			{
				TemStream::appDone = true;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

namespace TemStream
{
int runGui()
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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

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
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		drawMenu();

		ImGui::Render();
		SDL_SetRenderDrawColor(gui.renderer, 128u, 128u, 128u, 255u);
		SDL_RenderClear(gui.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(gui.renderer);
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	return EXIT_SUCCESS;
}
} // namespace TemStream