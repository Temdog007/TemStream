#include <include/main.h>

ServerConfiguration
defaultServerConfiguration()
{
    return (ServerConfiguration){ .maxConsumers = 1024,
                                  .maxProducers = 8,
                                  .record = false };
}

bool
parseServerConfiguration(const int argc,
                         const char** argv,
                         pAllConfiguration configuration)
{
    pServerConfiguration server = &configuration->configuration.server;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-P", keyLen, { goto parseMaxProducers; });
        STR_EQUALS(key, "--producers", keyLen, { goto parseMaxProducers; });
        STR_EQUALS(key, "-C", keyLen, { goto parseMaxConsumers; });
        STR_EQUALS(key, "--consumers", keyLen, { goto parseMaxConsumers; });
        STR_EQUALS(key, "-R", keyLen, { goto parseRecord; });
        STR_EQUALS(key, "--record", keyLen, { goto parseRecord; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Producer", key, value);
            return false;
        }
        continue;

    parseMaxProducers : {
        char* end = NULL;
        server->maxProducers = (uint32_t)strtoul(value, &end, 10);
        continue;
    }
    parseMaxConsumers : {
        char* end = NULL;
        server->maxConsumers = (uint32_t)strtoul(value, &end, 10);
        continue;
    }
    parseRecord : {
        server->record = atoi(value);
        continue;
    }
    }
    return true;
}

int
printServerConfiguration(const ServerConfiguration* configuration)
{
    return printf("Max consumers: %u; Max producers: %u; Recording: %d\n",
                  configuration->maxConsumers,
                  configuration->maxProducers,
                  configuration->record);
}

int
runServer(const AllConfiguration* configuration)
{
    puts("Running server");
    printAllConfiguration(configuration);
    return EXIT_SUCCESS;
}