#include <include/main.h>

bool
initLobby(redisContext* c)
{
    (void)c;
    return true;
}

void
closeLobby(redisContext* c)
{
    (void)c;
}

LobbyConfiguration
defaultLobbyConfiguration()
{
    return (LobbyConfiguration){
        .maxStreams = ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT,
        .nextPort = 10001u,
        .minPort = 10001u,
        .maxPort = 10256u,
    };
}

bool
parseLobbyConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    configuration->data.server.data.tag = ServerConfigurationDataTag_lobby;
    pLobbyConfiguration lobby = &configuration->data.server.data.lobby;
    *lobby = defaultLobbyConfiguration();
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "--max-streams", keyLen, { goto parseStreams; });
        STR_EQUALS(key, "--min-port", keyLen, { goto parseMinPort; });
        STR_EQUALS(key, "--max-port", keyLen, { goto parseMaxPort; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(
              key, value, &configuration->data.server)) {
            parseFailure("Lobby", key, value);
            return false;
        }
        continue;

    parseStreams : {
        const uint32_t i = (uint32_t)atoi(value);
        lobby->maxStreams =
          SDL_clamp(i, 1U, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT);
        continue;
    }
    parseMinPort : {
        const int i = atoi(value);
        lobby->minPort = SDL_clamp(i, 1000, 60000);
        lobby->nextPort = lobby->minPort;
        continue;
    }
    parseMaxPort : {
        const int i = atoi(value);
        lobby->maxPort = SDL_clamp(i, 1000, 60000);
        continue;
    }
    }
    return true;
}

void
serializeLobbyMessage(const void* ptr, pBytes bytes)
{
    CAST_MESSAGE(LobbyMessage, ptr);
    MESSAGE_SERIALIZE(LobbyMessage, (*message), (*bytes));
}

void*
deserializeLobbyMessage(const Bytes* bytes)
{
    pLobbyMessage message = currentAllocator->allocate(sizeof(LobbyMessage));
    MESSAGE_DESERIALIZE(LobbyMessage, (*message), (*bytes));
    return message;
}

void
freeLobbyMessage(void* ptr)
{
    CAST_MESSAGE(LobbyMessage, ptr);
    LobbyMessageFree(message);
    currentAllocator->free(message);
}

bool
handleLobbyMessage(const void* ptr,
                   pBytes bytes,
                   ENetPeer* peer,
                   redisContext* ctx)
{
    CAST_MESSAGE(LobbyMessage, ptr);
    switch (message->tag) {
        case LobbyMessageTag_allStreams: {
            StreamList streams = getStreams(ctx);
            LobbyMessage lobbyMessage = { 0 };
            lobbyMessage.tag = LobbyMessageTag_allStreamsAck;
            lobbyMessage.allStreamsAck = streams;
            MESSAGE_SERIALIZE(LobbyMessage, lobbyMessage, (*bytes));
            sendBytes(peer, 1, peer->mtu, SERVER_CHANNEL, bytes, true);
            StreamListFree(&streams);
        } break;
        default:
#if _DEBUG
            printf("Unexpected message '%s' from client\n",
                   LobbyMessageTagToCharString(message->tag));
#endif
            return false;
    }
    return true;
}