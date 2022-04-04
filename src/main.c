#include <include/main.h>

#include "audio.c"
#include "audio_decode.c"
#include "audio_pulse.c"
#include "base64.c"
#include "chat.c"
#include "circular_queue.c"
#include "client.c"
#include "image.c"
#include "lobby.c"
#include "misc.c"
#include "rendering.c"
#include "replay.c"
#include "server.c"
#include "text.c"
#include "video.c"
#include "video_x11.c"
#include "webcam.c"

#if USE_VPX
#include "vpx.c"
#endif

const Allocator* currentAllocator = NULL;
bool appDone = true;

int checkMemory(pConfiguration);

#define PRINT_ARGS 0

int
main(const int argc, const char** argv)
{
#if PRINT_ARGS
    puts("Arguments");
    for (int i = 0; i < argc; ++i) {
        printf("%d) %s\n", i, argv[i]);
    }
#endif
    int result = EXIT_FAILURE;
    Configuration configuration = { 0 };
    printVersion();

    {
        struct sigaction action;
        action.sa_handler = signalHandler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        sigaction(SIGINT, &action, NULL);
    }
    {
        // Look for -M or --memory
        int binaryIndex = -1;
        uint64_t memory = 256ULL;
        for (int i = 1; i < argc - 1; ++i) {
            if (strcmp("-B", argv[i]) == 0 ||
                strcmp("--binary", argv[i]) == 0) {
                binaryIndex = i + 1;
                continue;
            }
            if (strcmp("-M", argv[i]) != 0 &&
                strcmp("--memory", argv[i]) != 0) {
                continue;
            }

            memory = SDL_strtoull(argv[i + 1], NULL, 10);
            break;
        }
        if (memory == 0) {
            fprintf(stderr, "Memory allocator cannot be sized at 0\n");
            goto end;
        }

        static Allocator allocator = { 0 };
        allocator = makeTSAllocator(MB(memory));
        printf("Using %zu MB of memory\n", memory);
        currentAllocator = &allocator;
        if (binaryIndex == -1) {
            configuration = defaultConfiguration();
        } else {
            Bytes bytes = { .allocator = currentAllocator };
            const bool result = b64_decode(argv[binaryIndex], &bytes);
            if (result) {
                MESSAGE_DESERIALIZE(Configuration, configuration, bytes);
                printf("Configuration set to %s\n",
                       ServerConfigurationDataTagToCharString(
                         configuration.server.data.tag));
            }
            uint8_tListFree(&bytes);
            if (!result) {
                fprintf(stderr, "Failed to decode binary into configuration\n");
                goto end;
            }
        }
    }

#if ENET_USE_CUSTOM_ALLOCATOR
    const ENetCallbacks callbacks = { .free = tsFree,
                                      .malloc = tsAllocate,
                                      .no_memory = NULL };
    if (enet_initialize_with_callbacks(ENET_VERSION, &callbacks) != 0) {
        fprintf(stderr, "Failed to initialize ENet\n");
        goto end;
    }
#else
    if (enet_initialize() != 0) {
        fprintf(stderr, "Failed to initialize ENet\n");
        goto end;
    }
#endif

    result = runApp(argc, argv, &configuration);
    enet_deinitialize();

end:
#if _DEBUG
    result = checkMemory(&configuration);
#else
    ConfigurationFree(&configuration);
#endif
    freeTSAllocator();
    return result;
}

int
checkMemory(pConfiguration config)
{
    if (config->tag == ConfigurationTag_server) {
        printf("Checking memory of server: '%s'\n", config->server.name.buffer);
    }
    ConfigurationFree(config);
    const size_t used = currentAllocator->used();
    if (used == 0) {
        printf("No memory leaked\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Leaked %zu bytes\n", used);
        return EXIT_FAILURE;
    }
}

int
runApp(const int argc, const char** argv, pConfiguration configuration)
{
    int result = EXIT_FAILURE;

    if (argc > 1) {
        const char* streamType = argv[1];
        const size_t len = strlen(streamType);

        STR_EQUALS(streamType, "L", len, { goto runLobby; });
        STR_EQUALS(streamType, "lobby", len, { goto runLobby; });
        STR_EQUALS(streamType, "T", len, { goto runText; });
        STR_EQUALS(streamType, "text", len, { goto runText; });
        STR_EQUALS(streamType, "C", len, { goto runChat; });
        STR_EQUALS(streamType, "chat", len, { goto runChat; });
        STR_EQUALS(streamType, "I", len, { goto runImage; });
        STR_EQUALS(streamType, "image", len, { goto runImage; });
        STR_EQUALS(streamType, "A", len, { goto runAudio; });
        STR_EQUALS(streamType, "audio", len, { goto runAudio; });
        STR_EQUALS(streamType, "V", len, { goto runVideo; });
        STR_EQUALS(streamType, "video", len, { goto runVideo; });
        STR_EQUALS(streamType, "R", len, { goto runReplay; });
        STR_EQUALS(streamType, "replay", len, { goto runReplay; });
    }
    switch (configuration->tag) {
        case ConfigurationTag_none:
            if (parseClientConfiguration(argc, argv, configuration)) {
                result = runClient(configuration);
                break;
            }
            break;
        case ConfigurationTag_client:
            result = runClient(configuration);
            break;
        case ConfigurationTag_server:
            switch (configuration->server.data.tag) {
                case ServerConfigurationDataTag_audio:
                    result = runAudioServer(configuration);
                    goto end;
                case ServerConfigurationDataTag_chat:
                    result = runChatServer(configuration);
                    goto end;
                case ServerConfigurationDataTag_image:
                    result = runImageServer(configuration);
                    goto end;
                case ServerConfigurationDataTag_text:
                    result = runTextServer(configuration);
                    goto end;
                case ServerConfigurationDataTag_lobby:
                    result = runLobbyServer(configuration);
                    goto end;
                case ServerConfigurationDataTag_video:
                    result = runVideoServer(configuration);
                    goto end;
                case ServerConfigurationDataTag_replay:
                    result = runReplayServer(configuration);
                    goto end;
                default:
                    fprintf(stderr,
                            "Unknown configuration: %s\n",
                            ServerConfigurationDataTagToCharString(
                              configuration->server.data.tag));
                    break;
            }
            break;
        default:
            break;
    }
    goto end;

runLobby : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseLobbyConfiguration(argc, argv, configuration)) {
        result = runLobbyServer(configuration);
    }
    goto end;
}
runText : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseTextConfiguration(argc, argv, configuration)) {
        result = runTextServer(configuration);
    }
    goto end;
}
runChat : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseChatConfiguration(argc, argv, configuration)) {
        result = runChatServer(configuration);
    }
    goto end;
}
runImage : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseImageConfiguration(argc, argv, configuration)) {
        result = runImageServer(configuration);
    }
    goto end;
}
runAudio : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseAudioConfiguration(argc, argv, configuration)) {
        result = runAudioServer(configuration);
    }
    goto end;
}
runVideo : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseVideoConfiguration(argc, argv, configuration)) {
        result = runVideoServer(configuration);
    }
    goto end;
}
runReplay : {
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    if (parseReplayConfiguration(argc, argv, configuration)) {
        result = runReplayServer(configuration);
    }
    goto end;
}

end:
    return result;
}

int
printVersion()
{
    const ENetVersion version = enet_linked_version();
    SDL_version linkedSdl;
    SDL_GetVersion(&linkedSdl);
    SDL_version compiledSdl;
    SDL_VERSION(&compiledSdl);

    SDL_version ttfVersion;
    SDL_TTF_VERSION(&ttfVersion);

    SDL_version imgVersion;
    SDL_IMAGE_VERSION(&imgVersion);

    return printf("TemStream %d.%d.%d\nSDL compiled %u.%u.%u\nSDL linked "
                  "%u.%u.%u\nTTF %d.%d.%d\nIMG %d.%d.%d\nENet %x\nOpus %s\n",
                  VERSION_MAJOR,
                  VERSION_MINOR,
                  VERSION_PATCH,
                  compiledSdl.major,
                  compiledSdl.minor,
                  compiledSdl.patch,
                  linkedSdl.major,
                  linkedSdl.minor,
                  linkedSdl.patch,
                  ttfVersion.major,
                  ttfVersion.minor,
                  ttfVersion.patch,
                  imgVersion.major,
                  imgVersion.minor,
                  imgVersion.patch,
                  version,
                  opus_get_version_string());
}

void
signalHandler(int signal)
{
    if (signal == SIGINT) {
        puts("Ending TemStream");
        appDone = true;
    }
}

Configuration
defaultConfiguration()
{
    return (Configuration){ .none = NULL, .tag = ConfigurationTag_none };
}

void
parseFailure(const char* type, const char* arg1, const char* arg2)
{
    fprintf(stderr,
            "Failed to parse arguments '%s' and '%s' for type %s\n",
            arg1,
            arg2,
            type);
}

bool
parseCommonConfiguration(const char* key,
                         const char* value,
                         pConfiguration configuration)
{
    (void)value;
    (void)configuration;
    const size_t keyLen = strlen(key);
    STR_EQUALS(key, "-M", keyLen, { return true; });
    STR_EQUALS(key, "--memory", keyLen, { return true; });
    return false;
}

int
printConfiguration(const Configuration* configuration)
{
    switch (configuration->tag) {
        case ConfigurationTag_client:
            return printClientConfiguration(&configuration->client);
            break;
        case ConfigurationTag_server:
            return printServerConfiguration(&configuration->server);
            break;
        default:
            break;
    }
    return 0;
}