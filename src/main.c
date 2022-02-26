#include <include/main.h>

#include "client.c"
#include "misc.c"
#include "rendering.c"
#include "server.c"

const Allocator* currentAllocator = NULL;
bool appDone = true;

int
main(const int argc, const char** argv)
{
    printVersion();
    if (argc < 2) {
        fprintf(stderr, "Expected C or S as the first argument\n");
        return EXIT_FAILURE;
    }

    {
        struct sigaction action;
        action.sa_handler = signalHandler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        sigaction(SIGINT, &action, NULL);
    }
    {
        // Look for -M or --memory
        uint64_t memory = MB(256);
        for (int i = 1; i < argc - 1; i += 2) {
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
    AllConfiguration allConfiguration = defaultAllConfiguration();
    const char c = argv[1][0];
    int result = EXIT_FAILURE;
    switch (c) {
        case 'c':
        case 'C':
            allConfiguration.configuration.tag = ConfigurationTag_client;
            allConfiguration.configuration.client =
              defaultClientConfiguration();
            if (!parseClientConfiguration(argc, argv, &allConfiguration)) {
                result = EXIT_FAILURE;
                break;
            }
            result = runClient(&allConfiguration);
            break;
        case 's':
        case 'S':
            allConfiguration.configuration.tag = ConfigurationTag_server;
            allConfiguration.configuration.server =
              defaultServerConfiguration();
            if (!parseServerConfiguration(argc, argv, &allConfiguration)) {
                result = EXIT_FAILURE;
                break;
            }
            result = runServer(&allConfiguration);
            break;
        default:
            fprintf(
              stderr, "Expected C or S as the first argument. Got %c\n", c);
            break;
    }
    AllConfigurationFree(&allConfiguration);
    return result;
}

int
printVersion()
{
    const ENetVersion version = enet_linked_version();
    return printf("TemStream %d.%d.%d\nENet %x\n",
                  VERSION_MAJOR,
                  VERSION_MINOR,
                  VERSION_PATCH,
                  version);
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
    return (Configuration){ .tag = ConfigurationTag_none, .none = NULL };
}

AllConfiguration
defaultAllConfiguration()
{
    return (AllConfiguration){
        .address = { .ip = TemLangStringCreate("localhost", currentAllocator),
                     .port = TemLangStringCreate("10000", currentAllocator) },
        .secure = { .tag = OptionalSecureIpAddressTag_none, .none = NULL },
        .configuration = defaultConfiguration()
    };
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
                         pAllConfiguration configuration)
{
    const size_t keyLen = strlen(key);
    STR_EQUALS(key, "-A", keyLen, {
        return parseIpAddress(value, &configuration->address);
    });
    STR_EQUALS(key, "--address", keyLen, {
        return parseIpAddress(value, &configuration->address);
    });
    return false;
}

int
printAllConfiguration(const AllConfiguration* configuration)
{
    int output = printIpAddress(&configuration->address);
    switch (configuration->configuration.tag) {
        case ConfigurationTag_client:
            output +=
              printClientConfiguration(&configuration->configuration.client);
            break;
        case ConfigurationTag_server:
            output +=
              printServerConfiguration(&configuration->configuration.server);
            break;
        default:
            break;
    }
    return output;
}