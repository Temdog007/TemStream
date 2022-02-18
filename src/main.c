#include <include/main.h>

#include "client.c"
#include "networking.c"
#include "server.c"

const Allocator* currentAllocator = NULL;
bool appDone = false;

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

    static Allocator allocator = { 0 };
    allocator = makeDefaultAllocator();
    currentAllocator = &allocator;

    return runApp(argc, argv);
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
    return printf(
      "TemStream %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
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
    return (AllConfiguration){ .address = { .domainSocket = TemLangStringCreate(
                                              "app.sock", currentAllocator),
                                            .tag = AddressTag_domainSocket },
                               .configuration = defaultConfiguration() };
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
        return parseAddress(value, &configuration->address);
    });
    STR_EQUALS(key, "--address", keyLen, {
        return parseAddress(value, &configuration->address);
    });
    return false;
}

bool
parseAddress(const char* str, pAddress address)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        address->tag = AddressTag_ipAddress;
        address->ipAddress.ip =
          TemLangStringCreateFromSize(str, i + 1, currentAllocator);
        address->ipAddress.port =
          TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    address->tag = AddressTag_domainSocket;
    address->domainSocket = TemLangStringCreate(str, currentAllocator);
    return true;
}

bool
parseCredentials(const char* str, pCredentials c)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        c->username = TemLangStringCreateFromSize(str, i + 1, currentAllocator);
        c->password = TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    return false;
}

int
printAllConfiguration(const AllConfiguration* configuration)
{
    int output = printAddress(&configuration->address);
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

int
printAddress(const Address* address)
{
    switch (address->tag) {
        case AddressTag_domainSocket:
            return printf("Domain socket: %s\n", address->domainSocket.buffer);
        case AddressTag_ipAddress:
            return printf("Ip socket: %s:%s\n",
                          address->ipAddress.ip.buffer,
                          address->ipAddress.port.buffer);
        default:
            return 0;
    }
}

bool
StreamTypeMatchStreamMessage(const StreamType type,
                             const StreamMessageDataTag tag)
{
    return (type == StreamType_Text && tag == StreamMessageDataTag_text) ||
           (type == StreamType_Chat && tag == StreamMessageDataTag_chatMessage);
}

bool
MessageUsesUdp(const StreamMessage* message)
{
    switch (message->data.tag) {
        case StreamMessageDataTag_none:
            return true;
        default:
            break;
    }
    return false;
}

extern bool
StreamGuidEquals(const Stream* stream, const Guid* guid)
{
    return memcmp(&stream->id, guid, sizeof(Guid)) == 0;
}