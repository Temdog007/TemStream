#include <include/main.h>

DEFINE_RUN_SERVER(Lobby);

uint64_t lastStreamRefresh = 0;

LobbyConfiguration
defaultLobbyConfiguration()
{
    return (LobbyConfiguration){ .maxStreams =
                                   ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT,
                                 .refreshRate = 30u };
}

void
updateLobbyClients(pServerData serverData)
{
    LobbyMessage message = { .tag = LobbyMessageTag_allStreams,
                             .allStreams = getStreams(serverData->ctx) };
    // Don't show: redis ip and port, save directory, timeout, record
    for (size_t i = 0; i < message.allStreams.used; ++i) {
        pServerConfiguration c = &message.allStreams.buffer[i];
        TemLangStringFree(&c->redisIp);
        c->redisPort = 0;
        TemLangStringFree(&c->saveDirectory);
        c->timeout = 0;
        c->record = false;
    }
    MESSAGE_SERIALIZE(LobbyMessage, message, serverData->bytes);
    ENetPacket* packet = BytesToPacket(
      serverData->bytes.buffer, serverData->bytes.used, SendFlags_Normal);
    sendPacketToReaders(serverData->host, packet, &serverData->config->readers);
    LobbyMessageFree(&message);
}

void
onLobbyDownTime(pServerData serverData)
{
    if (serverIsDirty(serverData->ctx)) {
        setServerIsDirty(serverData->ctx, false);
        updateLobbyClients(serverData);
    } else {
        const uint64_t now = SDL_GetTicks64();
        if (now - lastStreamRefresh >=
            serverData->config->data.lobby.refreshRate * 1000U) {
            lastStreamRefresh = now;
#if _DEBUG
            char buffer[KB(2)];
            time_t rawtime = time(NULL);
            struct tm* t = localtime(&rawtime);
            strftime(buffer, sizeof(buffer), "%c", t);
            printf("Refreshed streams to clients: %s\n", buffer);
#endif
            updateLobbyClients(serverData);
        }
    }
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

bool
onConnectForLobby(ENetPeer* peer, pServerData serverData)
{
    (void)peer;
    (void)serverData;
    lastStreamRefresh = 0u;
    return true;
}

bool
handleLobbyMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    LobbyMessage lobbyMessage = { 0 };
    bool result = false;
    CAST_MESSAGE(LobbyMessage, ptr);
    switch (message->tag) {
        case LobbyMessageTag_general:
            lobbyMessage.tag = LobbyMessageTag_general;
            result = handleGeneralMessage(
              &message->general, serverData, &lobbyMessage.general);
            break;
        default:
            break;
    }
    if (result) {
        MESSAGE_SERIALIZE(LobbyMessage, lobbyMessage, serverData->bytes);
        sendBytes(
          peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
    } else {
#if _DEBUG
        printf("Unexpected lobby message '%s' from client\n",
               LobbyMessageTagToCharString(message->tag));
#endif
    }
    LobbyMessageFree(&lobbyMessage);
    return result;
}