#include <include/main.h>

#include "consumer.c"
#include "producer.c"
#include "server.c"

const Allocator* currentAllocator = NULL;

int
main(const int argc, const char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Expected P, C, or S as the first argument\n");
        return EXIT_FAILURE;
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
        case 'p':
        case 'P':
            allConfiguration.configuration.tag = ConfigurationTag_producer;
            allConfiguration.configuration.producer =
              defaultProducerConfiguration();
            if (!parseProducerConfiguration(argc, argv, &allConfiguration)) {
                result = EXIT_FAILURE;
                break;
            }
            result = runProducer(&allConfiguration);
            break;
        case 'c':
        case 'C':
            allConfiguration.configuration.tag = ConfigurationTag_consumer;
            allConfiguration.configuration.consumer =
              defaultConsumerConfiguration();
            if (!parseConsumerConfiguration(argc, argv, &allConfiguration)) {
                result = EXIT_FAILURE;
                break;
            }
            result = runConsumer(&allConfiguration);
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
              stderr, "Expected P, C, or S as the first argument. Got %c\n", c);
            break;
    }
    AllConfigurationFree(&allConfiguration);
    return result;
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
                               .authentication = false,
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
    STR_EQUALS(key, "-AU", keyLen, { goto parseAuthentication; });
    STR_EQUALS(key, "--authentication", keyLen, { goto parseAuthentication; })
    return false;

parseAuthentication : {
    configuration->authentication = atoi(value);
    return true;
}
}

bool
parseAddress(const char* str, pAddress address)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        for (size_t j = i; j < len; ++j) {
            if (str[j] != ':') {
                continue;
            }
            for (size_t k = j; k < len; ++k) {
                if (str[k] != ':') {
                    continue;
                }
                address->tag = AddressTag_secure;
                address->secure.address.ip =
                  TemLangStringCreateFromSize(str, i, currentAllocator);
                address->secure.address.port = TemLangStringCreateFromSize(
                  str + i + 1, j - i, currentAllocator);
                address->secure.certFile = TemLangStringCreateFromSize(
                  str + j + 1, k - j, currentAllocator);
                address->secure.keyFile =
                  TemLangStringCreate(str + k + 1, currentAllocator);
                return true;
            }
        }

        address->tag = AddressTag_unsecure;
        address->unsecure.ip =
          TemLangStringCreateFromSize(str, i, currentAllocator);
        address->unsecure.port =
          TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    address->tag = AddressTag_domainSocket;
    address->domainSocket = TemLangStringCreate(str, currentAllocator);
    return true;
}

int
printAllConfiguration(const AllConfiguration* configuration)
{
    int output = printAddress(&configuration->address);
    output += printf("Authentication: %d\n", configuration->authentication);
    switch (configuration->configuration.tag) {
        case ConfigurationTag_consumer:
            output += printConsumerConfiguration(
              &configuration->configuration.consumer);
            break;
        case ConfigurationTag_producer:
            output += printProducerConfiguration(
              &configuration->configuration.producer);
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
        case AddressTag_secure:
            return printf("Secure socket: %s:%s:%s:%s\n",
                          address->secure.address.ip.buffer,
                          address->secure.address.port.buffer,
                          address->secure.certFile.buffer,
                          address->secure.keyFile.buffer);
        case AddressTag_unsecure:
            return printf("Unsecure socket: %s:%s\n",
                          address->unsecure.ip.buffer,
                          address->unsecure.port.buffer);
        default:
            return 0;
    }
}