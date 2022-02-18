#include <include/main.h>

ClientConfiguration
defaultClientConfiguration()
{
    return (ClientConfiguration){ .fullscreen = false,
                                  .windowWidth = 800,
                                  .windowHeight = 600,
                                  .ttfFile = TemLangStringCreate(
                                    "/usr/fonts/share/ubuntu/Ubuntu-M.ttf",
                                    currentAllocator) };
}

bool
parseClientConfiguration(const int argc,
                         const char** argv,
                         pAllConfiguration configuration)
{
    pClientConfiguration client = &configuration->configuration.client;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-F", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "--font", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "-W", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "--width", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "-H", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "--height", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "-F", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "--fullscreen", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "-T", keyLen, { goto parseToken; });
        STR_EQUALS(key, "--token", keyLen, { goto parseToken; });
        STR_EQUALS(key, "-C", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "--credentials", keyLen, { goto parseCredentials; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Client", key, value);
            return false;
        }
        continue;
    parseTTF : {
        client->ttfFile = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseWidth : {
        client->windowWidth = atoi(value);
        continue;
    }
    parseHeight : {
        client->windowHeight = atoi(value);
        continue;
    }
    parseFullscreen : {
        client->fullscreen = atoi(value);
        continue;
    }
    parseToken : {
        client->authentication.tag = ClientAuthenticationTag_token;
        client->authentication.token =
          TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseCredentials : {
        client->authentication.tag = ClientAuthenticationTag_credentials;
        if (!parseCredentials(value, &client->authentication.credentials)) {
            return false;
        }
        continue;
    }
    }
    return true;
}

void
ClientFree(pClient client)
{
    closeSocket(client->sockfd);
    GuidListFree(&client->connectedStreams);
    TemLangStringFree(&client->name);
}

int
printClientConfiguration(const ClientConfiguration* configuration)
{
    return printf("Width: %d; Height: %d; Fullscreen: %d; TTF file: %s\n",
                  configuration->windowHeight,
                  configuration->windowWidth,
                  configuration->fullscreen,
                  configuration->ttfFile.buffer);
}

bool
ClientGuidEquals(const pClient* client, const Guid* guid)
{
    return memcmp(&(*client)->guid, guid, sizeof(Guid)) == 0;
}

int
runClient(const AllConfiguration* configuration)
{
    puts("Running client");
    printAllConfiguration(configuration);
    return EXIT_SUCCESS;
}