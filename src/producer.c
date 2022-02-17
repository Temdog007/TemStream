#include <include/main.h>

ProducerConfiguration
defaultProducerConfiguration()
{
    return (ProducerConfiguration){ .unused = NULL };
}

bool
parseProducerConfiguration(const int argc,
                           const char** argv,
                           pAllConfiguration configuration)
{
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const char* value = argv[i + 1];
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Producer", key, value);
            return false;
        }
    }
    return true;
}

int
printProducerConfiguration(const ProducerConfiguration* configuration)
{
    return printf("%d\n", configuration->unused);
}

int
runProducer(const AllConfiguration* configuration)
{
    puts("Running producer");
    printAllConfiguration(configuration);
    return EXIT_SUCCESS;
}