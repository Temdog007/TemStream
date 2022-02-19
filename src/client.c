#include <include/main.h>

ClientConfiguration
defaultClientConfiguration()
{
    return (ClientConfiguration){
        .fullscreen = false,
        .windowWidth = 800,
        .windowHeight = 600,
        .fontSize = 24,
        .ttfFile = TemLangStringCreate(
          "/usr/share/fonts/truetype/ubuntu//Ubuntu-M.ttf", currentAllocator)
    };
}

bool
parseClientConfiguration(const int argc,
                         const char** argv,
                         pAllConfiguration configuration)
{
    pClientConfiguration client = &configuration->configuration.client;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-F", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "--font", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "-W", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "--width", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "-H", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "--height", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "-F", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "--fullscreen", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "-T", keyLen, { goto parseToken; });
        STR_EQUALS(key, "--token", keyLen, { goto parseToken; });
        STR_EQUALS(key, "-C", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "--credentials", keyLen, { goto parseCredentials; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Client", key, value);
            return false;
        }
        continue;
    parseTTF : {
        client->ttfFile = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseWidth : {
        client->windowWidth = atoi(value);
        continue;
    }
    parseHeight : {
        client->windowHeight = atoi(value);
        continue;
    }
    parseFullscreen : {
        client->fullscreen = atoi(value);
        continue;
    }
    parseToken : {
        client->authentication.tag = ClientAuthenticationTag_token;
        client->authentication.token =
          TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseCredentials : {
        client->authentication.tag = ClientAuthenticationTag_credentials;
        if (!parseCredentials(value, &client->authentication.credentials)) {
            return false;
        }
        continue;
    }
    }
    return true;
}

void
ClientFree(pClient client)
{
    closeSocket(client->sockfd);
    GuidListFree(&client->connectedStreams);
    TemLangStringFree(&client->name);
}

int
printClientConfiguration(const ClientConfiguration* configuration)
{
    return printf("Width: %d; Height: %d; Fullscreen: %d; TTF file: %s\n",
                  configuration->windowWidth,
                  configuration->windowHeight,
                  configuration->fullscreen,
                  configuration->ttfFile.buffer);
}

bool
ClientGuidEquals(const pClient* client, const Guid* guid)
{
    return GuidEquals(&(*client)->id, guid);
}

#define EVENT_RENDER 0x31ab

int
runClient(const AllConfiguration* configuration)
{
    int result = EXIT_FAILURE;
    puts("Running client");
    printAllConfiguration(configuration);

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    {
        // const uint32_t flags =
        //   IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP;
        const uint32_t flags = IMG_INIT_PNG;
        if (IMG_Init(flags) != flags) {
            fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
            goto end;
        }
    }

    const ClientConfiguration* config = &configuration->configuration.client;

    window =
      SDL_CreateWindow("TemStream Client",
                       SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED,
                       config->windowWidth,
                       config->windowHeight,
                       config->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (window == NULL) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        goto end;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        goto end;
    }

    SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 0xffu);

    SDL_Event e = { 0 };
    while (!appDone) {
        if (SDL_WaitEvent(&e) != 0) {
            fprintf(
              stderr, "Failed to wait for an event: %s\n", SDL_GetError());
            goto end;
        }
        switch (e.type) {
            case SDL_QUIT:
                appDone = true;
                break;
            case SDL_USEREVENT:
                switch (e.user.code) {
                    case EVENT_RENDER: {
                        SDL_RenderClear(renderer);
                        SDL_RenderPresent(renderer);
                    } break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    result = EXIT_SUCCESS;
end:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return result;
}