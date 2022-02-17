#include <include/main.h>

ConsumerConfiguration
defaultConsumerConfiguration()
{
    return (ConsumerConfiguration){ .fullscreen = false,
                                    .width = 800,
                                    .height = 600,
                                    .ttfFile = TemLangStringCreate(
                                      "/usr/fonts/share/ubuntu/Ubuntu-M.ttf",
                                      currentAllocator) };
}

bool
parseConsumerConfiguration(const int argc,
                           const char** argv,
                           pAllConfiguration configuration)
{
    pConsumerConfiguration consumer = &configuration->configuration.consumer;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-T", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "--ttf", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "-W", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "--width", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "-H", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "--height", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "-F", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "--fullscreen", keyLen, { goto parseFullscreen; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Consumer", key, value);
            return false;
        }
        continue;
    parseTTF : {
        consumer->ttfFile = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseWidth : {
        consumer->width = atoi(value);
        continue;
    }
    parseHeight : {
        consumer->height = atoi(value);
        continue;
    }
    parseFullscreen : {
        consumer->fullscreen = atoi(value);
        continue;
    }
    }
    return true;
}

int
printConsumerConfiguration(const ConsumerConfiguration* configuration)
{
    return printf("Width: %d; Height: %d; Fullscreen: %d; TTF file: %s\n",
                  configuration->width,
                  configuration->height,
                  configuration->fullscreen,
                  configuration->ttfFile.buffer);
}

int
runConsumer(const AllConfiguration* configuration)
{
    puts("Running consumer");
    printAllConfiguration(configuration);
    return EXIT_SUCCESS;
}