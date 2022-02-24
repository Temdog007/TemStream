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

    static Allocator allocator = { 0 };
    allocator = makeDefaultAllocator();
    currentAllocator = &allocator;
    if (enet_initialize() != 0) {
        fprintf(stderr, "Failed to initialize ENet\n");
        return EXIT_FAILURE;
    }

    const int result = runApp(argc, argv);
    enet_deinitialize();
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
printIpAddress(const IpAddress* address)
{
    return printf(
      "Ip address: %s:%s\n", address->ip.buffer, address->port.buffer);
}

int
printStream(const Stream* stream)
{
    char guid[128];
    getGuidString(&stream->id, guid);
    return printf("%s: %s (%s)\n",
                  guid,
                  stream->name.buffer,
                  StreamTypeToCharString(stream->type));
}

bool
StreamTypeMatchStreamMessage(const StreamType type,
                             const StreamMessageDataTag tag)
{
    return (type == StreamType_Text && tag == StreamMessageDataTag_text) ||
           (type == StreamType_Chat && tag == StreamMessageDataTag_chatMessage);
}

bool
StreamGuidEquals(const Stream* stream, const Guid* guid)
{
    return GuidEquals(&stream->id, guid);
}

bool
StreamNameEquals(const Stream* stream, const TemLangString* name)
{
    return TemLangStringsAreEqual(&stream->name, name);
}

bool
StreamTypeEquals(const Stream* stream, const StreamType* type)
{
    return stream->type == *type;
}

bool
StreamMessageGuidEquals(const StreamMessage* message, const Guid* guid)
{
    return GuidEquals(&message->id, guid);
}

bool
StreamDisplayGuidEquals(const StreamDisplay* display, const Guid* guid)
{
    return GuidEquals(&display->id, guid);
}

bool
GetStreamFromName(const StreamList* streams,
                  const TemLangString* name,
                  const Stream** stream,
                  size_t* index)
{
    return StreamListFindIf(
      streams, (StreamListFindFunc)StreamNameEquals, name, stream, index);
}

bool
GetStreamFromType(const StreamList* streams,
                  const StreamType type,
                  const Stream** stream,
                  size_t* index)
{
    return StreamListFindIf(
      streams, (StreamListFindFunc)StreamTypeEquals, &type, stream, index);
}

bool
GetStreamFromGuid(const StreamList* streams,
                  const Guid* guid,
                  const Stream** stream,
                  size_t* index)
{
    return StreamListFindIf(
      streams, (StreamListFindFunc)StreamGuidEquals, guid, stream, index);
}

bool
GetStreamMessageFromGuid(const StreamMessageList* streams,
                         const Guid* guid,
                         const StreamMessage** stream,
                         size_t* index)
{
    return StreamMessageListFindIf(
      streams,
      (StreamMessageListFindFunc)StreamMessageGuidEquals,
      guid,
      stream,
      index);
}

bool
GetStreamDisplayFromGuid(const StreamDisplayList* displays,
                         const Guid* guid,
                         const StreamDisplay** display,
                         size_t* index)
{
    return StreamDisplayListFindIf(
      displays,
      (StreamDisplayListFindFunc)StreamDisplayGuidEquals,
      guid,
      display,
      index);
}

bool
GetClientFromGuid(const pClientList* list,
                  const Guid* guid,
                  const pClient** client,
                  size_t* index)
{
    return pClientListFindIf(
      list, (pClientListFindFunc)ClientGuidEquals, guid, client, index);
}
