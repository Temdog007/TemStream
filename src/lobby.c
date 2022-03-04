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
    return (LobbyConfiguration){ .maxStreams =
                                   ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT };
}

bool
parseLobbyConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_lobby;
    pLobbyConfiguration lobby = &configuration->server.data.lobby;
    *lobby = defaultLobbyConfiguration();
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "--max-streams", keyLen, { goto parseStreams; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
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

const GeneralMessage*
getGeneralMessageFromLobby(const void* ptr)
{
    CAST_MESSAGE(LobbyMessage, ptr);
    return message->tag == LobbyMessageTag_general ? &message->general : NULL;
}

bool
handleLobbyMessage(const void* ptr,
                   pBytes bytes,
                   ENetPeer* peer,
                   redisContext* ctx,
                   const ServerConfiguration* s)
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
            return true;
        } break;
        case LobbyMessageTag_startStreaming: {
            ServerConfiguration newConfig = { 0 };
            StreamList streams = getStreams(ctx);
            const Stream* newStream = &message->startStreaming;
            pClient client = peer->data;
            if (!GetStreamFromName(&streams, &newStream->name, NULL, NULL)) {
#if _DEBUG
                printf("Client '%s' attempted to make duplicate stream '%s'\n",
                       client->name.buffer,
                       newStream->name.buffer);
#endif
                LobbyMessage lobbyMessage = { 0 };
                lobbyMessage.tag = LobbyMessageTag_startStreamingAck;
                lobbyMessage.startStreamingAck.none = NULL;
                lobbyMessage.startStreamingAck.tag = OptionalStreamTag_none;
                MESSAGE_SERIALIZE(LobbyMessage, lobbyMessage, (*bytes));
                sendBytes(peer, 1, peer->mtu, SERVER_CHANNEL, bytes, true);
                goto ssEnd;
            }

            ServerConfigurationCopy(&newConfig, s, currentAllocator);
            switch (newStream->type) {
                case StreamType_Text: {
                } break;
                default: {
                    fprintf(
                      stderr,
                      "Client '%s' tried to make unknown stream type: %s\n",
                      client->name.buffer,
                      StreamTypeToCharString(newStream->type));
                    LobbyMessage lobbyMessage = { 0 };
                    lobbyMessage.tag = LobbyMessageTag_startStreamingAck;
                    lobbyMessage.startStreamingAck.none = NULL;
                    lobbyMessage.startStreamingAck.tag = OptionalStreamTag_none;
                    MESSAGE_SERIALIZE(LobbyMessage, lobbyMessage, (*bytes));
                    sendBytes(peer, 1, peer->mtu, SERVER_CHANNEL, bytes, true);
                    goto ssEnd;
                }
            }

        ssEnd:
            ServerConfigurationFree(&newConfig);
            StreamListFree(&streams);
            return true;
        } break;
        case LobbyMessageTag_general: {
            LobbyMessage lobbyMessage = { 0 };
            if (!handleGeneralMessage(
                  &message->general, peer, &lobbyMessage.general)) {
                break;
            }
            lobbyMessage.tag = LobbyMessageTag_general;
            MESSAGE_SERIALIZE(LobbyMessage, lobbyMessage, (*bytes));
            sendBytes(peer, 1, peer->mtu, SERVER_CHANNEL, bytes, true);
            LobbyMessageFree(&lobbyMessage);
            return true;
        } break;
        default:
            break;
    }
#if _DEBUG
    printf("Unexpected message '%s' from client\n",
           LobbyMessageTagToCharString(message->tag));
#endif
    return false;
}