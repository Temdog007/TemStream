#include <include/main.h>

#include "base64.c"
#include "client.c"
#include "lobby.c"
#include "misc.c"
#include "rendering.c"
#include "server.c"

const Allocator* currentAllocator = NULL;
bool appDone = true;

int
main(const int argc, const char** argv)
{
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
        uint64_t memory = MB(8);
        for (int i = 1; i < argc - 1; ++i) {
            if (strcmp("-M", argv[i]) != 0 &&
                strcmp("--memory", argv[i]) != 0) {
                continue;
            }

            char* end = NULL;
            memory = SDL_strtoull(argv[i + 1], &end, 10);
            break;
        }
        if (memory == 0) {
            fprintf(stderr, "Memory allocator cannot be sized at 0\n");
            return EXIT_FAILURE;
        }

        static Allocator allocator = { 0 };
        allocator = makeTSAllocator(memory);
        printf("Using %zu MB of memory\n", memory / (1024 * 1024));
        currentAllocator = &allocator;
    }

    const ENetCallbacks callbacks = { .free = tsFree,
                                      .malloc = tsAllocate,
                                      .no_memory = NULL };
    if (enet_initialize_with_callbacks(ENET_VERSION, &callbacks) != 0) {
        fprintf(stderr, "Failed to initialize ENet\n");
        return EXIT_FAILURE;
    }

    const int result = runApp(argc, argv);
    enet_deinitialize();

#if _DEBUG
    const size_t used = currentAllocator->used();
    if (used == 0) {
        printf("No memory leaked\n");
    } else {
        fprintf(stderr, "Leaked %zu bytes\n", used);
    }
#endif
    freeTSAllocator();
    return result;
}

int
runApp(const int argc, const char** argv)
{
    Configuration configuration = defaultConfiguration();
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
    }
    result = runClient(argc, argv, &configuration);
    goto end;

runLobby : {
    result = runServer(argc,
                       argv,
                       &configuration,
                       (ServerFunctions){
                         .parseConfiguration = parseLobbyConfiguration,
                         .serializeMessage = serializeLobbyMessage,
                         .deserializeMessage = deserializeLobbyMessage,
                         .handleMessage = handleLobbyMessage,
                         .freeMessage = freeLobbyMessage,
                         .getGeneralMessage = getGeneralMessageFromLobby,
                       });
    goto end;
}
runText : {
    // result = runTextServer(argc, argv, &configuration);
    goto end;
}
runChat : {
    // result = runChatServer(argc, argv, &configuration);
    goto end;
}
runImage : {
    // result = runImageServer(argc, argv, &configuration);
    goto end;
}
runAudio : {
    // result = runAudioServer(argc, argv, &configuration);
    goto end;
}

end:
    ConfigurationFree(&configuration);
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
    return printf(
      "TemStream %d.%d.%d\nSDL compiled %u.%u.%u\nSDL linked %u.%u.%u\nENet "
      "%x\nOpus %s\n",
      VERSION_MAJOR,
      VERSION_MINOR,
      VERSION_PATCH,
      compiledSdl.major,
      compiledSdl.minor,
      compiledSdl.patch,
      linkedSdl.major,
      linkedSdl.minor,
      linkedSdl.patch,
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