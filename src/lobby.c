#include <include/main.h>

DEFINE_RUN_SERVER(Lobby);

uint64_t lastStreamRefresh = 0;

LobbyConfiguration
defaultLobbyConfiguration()
{
    return (LobbyConfiguration){ .maxStreams =
                                   ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT,
                                 .refreshRate = 3u };
}

void
onLobbyDownTime(pServerData serverData)
{
    const uint64_t now = SDL_GetTicks64();
    if (now - lastStreamRefresh <
        serverData->config->data.lobby.refreshRate * 1000U) {
        return;
    }

    lastStreamRefresh = now;
    LobbyMessage message = { .tag = LobbyMessageTag_allStreams,
                             .allStreams = getStreams(serverData->ctx) };
    MESSAGE_SERIALIZE(LobbyMessage, message, serverData->bytes);
    ENetPacket* packet =
      BytesToPacket(serverData->bytes.buffer, serverData->bytes.used, true);
    sendPacketToReaders(serverData->host, packet, &serverData->config->readers);
    LobbyMessageFree(&message);
}

int
printLobbyConfiguration(const LobbyConfiguration* configuration)
{
    return printf("Lobby\nMax streams: %u\nRefresh rate: %zu seconds\n",
                  configuration->maxStreams,
                  configuration->refreshRate);
}

bool
parseLobbyConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_lobby;
    pLobbyConfiguration lobby = &configuration->server.data.lobby;
    *lobby = defaultLobbyConfiguration();
    lobby->runCommand = (NullValue)argv[0];
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-MS", keyLen, { goto parseStreams; });
        STR_EQUALS(key, "--max-streams", keyLen, { goto parseStreams; });
        STR_EQUALS(key, "-R", keyLen, { goto parseRefresh; });
        STR_EQUALS(key, "--refresh-rate", keyLen, { goto parseRefresh; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Lobby", key, value);
            return false;
        }
        continue;

    parseStreams : {
        const int i = atoi(value);
        lobby->maxStreams =
          SDL_clamp(i, 1, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT);
        continue;
    }
    parseRefresh : {
        const int i = atoi(value);
        lobby->refreshRate = SDL_max(1, i);
        continue;
    }
    }
    return true;
}

int
waitForChildProcess(void* ptr)
{
    const size_t fs = (size_t)ptr;
    const pid_t f = (pid_t)fs;
    const int result = waitpid(f, NULL, 0);
    SDL_AtomicDecRef(&runningThreads);
    return result;
}

bool
startNewServer(const ServerConfiguration* serverConfig)
{
    const pid_t f = fork();
    if (f == 0) {
        Configuration c = { .server = *serverConfig,
                            .tag = ConfigurationTag_server };
        Bytes bytes = { .allocator = currentAllocator };
        MESSAGE_SERIALIZE(Configuration, c, bytes);
        TemLangString str = b64_encode(&bytes);
        const int result = execl(serverConfig->data.lobby.runCommand,
                                 serverConfig->data.lobby.runCommand,
                                 "-B",
                                 str.buffer,
                                 NULL);
        TemLangStringFree(&str);
        uint8_tListFree(&bytes);
        exit(result);
        return false;
    } else {
        printf("Server '%s(%s)' started has process %d\n",
               serverConfig->name.buffer,
               ServerConfigurationDataTagToCharString(serverConfig->data.tag),
               f);
        const size_t fs = (size_t)f;
        SDL_Thread* thread = SDL_CreateThread(
          (SDL_ThreadFunction)waitForChildProcess, "wait", (void*)fs);
        if (thread == NULL) {
            fprintf(
              stderr, "Failed to create new thread: %s\n", SDL_GetError());
            return false;
        }
        SDL_AtomicIncRef(&runningThreads);
        SDL_DetachThread(thread);
        return true;
    }
}

bool
onConnectForLobby(ENetPeer* peer, pServerData serverData)
{
    (void)peer;
    (void)serverData;
    lastStreamRefresh = 0;
    return true;
}

bool
handleLobbyMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    LobbyMessage lobbyMessage = { 0 };
    pClient client = peer->data;
    bool result = false;
    CAST_MESSAGE(LobbyMessage, ptr);
    switch (message->tag) {
        case LobbyMessageTag_general:
            lobbyMessage.tag = LobbyMessageTag_general;
            result = handleGeneralMessage(
              &message->general, peer, &lobbyMessage.general);
            break;
        case LobbyMessageTag_startStreaming: {
            lobbyMessage.tag = LobbyMessageTag_startStreamingAck;
            lobbyMessage.startStreamingAck = false;
            result = true;

            ServerConfigurationList streams = getStreams(serverData->ctx);
            const ServerConfiguration* newConfig = &message->startStreaming;
            if (GetStreamFromName(&streams, &newConfig->name, NULL, NULL)) {
#if _DEBUG
                printf("Client '%s' attempted to make duplicate stream '%s'\n",
                       client->name.buffer,
                       newConfig->name.buffer);
#endif
                goto ssEnd;
            }

            switch (newConfig->data.tag) {
                case ServerConfigurationDataTag_text:
                case ServerConfigurationDataTag_chat:
                case ServerConfigurationDataTag_audio:
                case ServerConfigurationDataTag_image: {
                    ServerConfiguration c = *serverData->config;
                    c.name = newConfig->name;
                    c.readers = newConfig->readers;
                    c.writers = newConfig->writers;
                    c.data = newConfig->data;
                    c.data.lobby.runCommand =
                      serverData->config->data.lobby.runCommand;
                    // Streams started with client must always have a timeout
                    // since there isn't a way to manually stop them
                    c.timeout = STREAM_TIMEOUT;
                    const bool success = startNewServer(&c);
                    const uint64_t now = SDL_GetTicks64();
                    if (now > 1000U) {
                        lastStreamRefresh = now - 1000;
                    } else {
                        lastStreamRefresh = 0;
                    }
                    if (success) {
                        lobbyMessage.startStreamingAck = true;
                    } else {
                        fprintf(stderr, "Failed to create new server\n");
                        goto ssEnd;
                    }
                } break;
                default:
                    printf("Client '%s' tried to create a  '%s' stream \n",
                           client->name.buffer,
                           ServerConfigurationDataTagToCharString(
                             newConfig->data.tag));
                    break;
            }

        ssEnd:
            ServerConfigurationListFree(&streams);
        } break;
        default:
            break;
    }
    if (result) {
        MESSAGE_SERIALIZE(LobbyMessage, lobbyMessage, serverData->bytes);
        sendBytes(peer, 1, SERVER_CHANNEL, &serverData->bytes, true);
    } else {
#if _DEBUG
        printf("Unexpected lobby message '%s' from client\n",
               LobbyMessageTagToCharString(message->tag));
#endif
    }
    LobbyMessageFree(&lobbyMessage);
    return result;
}