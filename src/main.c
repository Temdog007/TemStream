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
checkMemory();

int
main(const int argc, const char** argv)
{
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
        uint64_t memory = MB(8);
        for (int i = 1; i < argc - 1; ++i) {
            if (strcmp("-B", argv[i]) == 0 ||
                strcmp("--binary", argv[i]) == 0) {
                binaryIndex = i;
                continue;
            }
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
            goto end;
        }

        static Allocator allocator = { 0 };
        allocator = makeTSAllocator(memory);
        printf("Using %zu MB of memory\n", memory / (1024 * 1024));
        currentAllocator = &allocator;
        if (binaryIndex == -1) {
            configuration = defaultConfiguration();
        } else {
            TemLangString str = { .allocator = currentAllocator };
            const bool result = b64_decode(argv[binaryIndex], &str);
            Bytes bytes = { .allocator = currentAllocator,
                            .buffer = (uint8_t*)str.buffer,
                            .size = str.size,
                            .used = str.used };
            if (result) {
                ConfigurationDeserialize(&configuration, &bytes, 0, true);
            }
            TemLangStringFree(&str);
            if (!result) {
                fprintf(stderr, "Failed to decode binary into configuration\n");
                goto end;
            }
        }
    }

    const ENetCallbacks callbacks = { .free = tsFree,
                                      .malloc = tsAllocate,
                                      .no_memory = NULL };
    if (enet_initialize_with_callbacks(ENET_VERSION, &callbacks) != 0) {
        fprintf(stderr, "Failed to initialize ENet\n");
        goto end;
    }

    result = runApp(argc, argv, &configuration);
    enet_deinitialize();

end:
    ConfigurationFree(&configuration);
#if _DEBUG
    result = checkMemory();
#endif
    freeTSAllocator();
    return result;
}

int
checkMemory()
{
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
                    goto runAudio;
                case ServerConfigurationDataTag_chat:
                    goto runChat;
                case ServerConfigurationDataTag_image:
                    goto runImage;
                case ServerConfigurationDataTag_text:
                    goto runText;
                case ServerConfigurationDataTag_lobby:
                    goto runLobbySkipParsing;
                default:
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
    configuration->server.runCommand = (NullValue)argv[0];
    if (parseLobbyConfiguration(argc, argv, configuration)) {
    runLobbySkipParsing:
        result = runServer(configuration,
                           (ServerFunctions){
                             .serializeMessage = serializeLobbyMessage,
                             .deserializeMessage = deserializeLobbyMessage,
                             .handleMessage = handleLobbyMessage,
                             .sendGeneral = lobbySendGeneralMessage,
                             .freeMessage = freeLobbyMessage,
                             .getGeneralMessage = getGeneralMessageFromLobby,
                           });
    }
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