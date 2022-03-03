#include <include/main.h>

LobbyConfiguration
defaultLobbyConfiguration()
{
    return (LobbyConfiguration){
        .redisIp = TemLangStringCreate("localhost", currentAllocator),
        .redisPort = 6379,
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
    configuration->data.tag = ServerConfigurationDataTag_lobby;
    pLobbyConfiguration lobby = &configuration->data.server.data.lobby;
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

bool
handleLobbyMessage(const LobbyMessage* message,
                   pBytes bytes,
                   ENetPeer* peer,
                   redisContext* ctx)
{
    switch (message->tag) {
        case LobbyMessageTag_allStreams: {
            StreamList streams = getStreams(ctx);
            LobbyMessage lobbyMessage = { 0 };
            lobbyMessage.tag = LobbyMessageTag_allStreamsAck;
            lobbyMessage.allStreamsAck = streams;
            MESSAGE_SERIALIZE(Lobby, lobbyMessage, (*bytes));
            StreamListFree(&streams);
        } break;
        default:
            return false;
    }
}

int
runLobbyServer(const int argc, const char* argv, pConfiguration configuration)
{
    int result = parseLobbyConfiguration(argc, argv, configuration);
    if (result != EXIT_SUCCESS) {
        return result;
    }

    redisContext* ctx = NULL;
    ENetHost* server = NULL;
    PayloadDataList payloads = { .allocator = currentAllocator };
    Bytes bytes = { .allocator = currentAllocator };
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    puts("Running lobby server");
    printAllConfiguration(configuration);

    appDone = false;

    const ServerConfiguration* config = &configuration->data.server;
    {
        char* end = NULL;
        ENetAddress address = { 0 };
        enet_address_set_host(&address, configuration->address.ip.buffer);
        address.port =
          (uint16_t)SDL_strtoul(configuration->address.port.buffer, &end, 10);
        server = enet_host_create(&address, config->maxClients, 2, 0, 0);
    }
    if (server == NULL) {
        fprintf(stderr, "Failed to create server\n");
        goto end;
    }

    ctx = redisConnect(config->data.lobby.redisIp.buffer,
                       config->data.lobby.redisPort);
    if (ctx == NULL || ctx->err) {
        if (ctx == NULL) {
            fprintf(stderr, "Can't make redis context\n");
        } else {
            fprintf(stderr, "Error: s\n", ctx->errstr);
        }
        goto end;
    }

    RandomState rs = makeRandomState();
    ENetEvent event = { 0 };
    while (!appDone) {
        while (enet_host_service(server, &event, 100U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    char buffer[512] = { 0 };
                    enet_address_get_host_ip(
                      &event.peer->address, buffer, sizeof(buffer));
                    printf("New client from %s:%u\n",
                           buffer,
                           event.peer->address.port);
                    pClient client = currentAllocator->allocate(sizeof(Client));
                    client->joinTime = (int64_t)time(NULL);
                    client->serverAuthentication = &config->authentication;
                    // name and id will be set after parsing authentication
                    // message
                    event.peer->data = client;
                } break;
                case ENET_EVENT_TYPE_DISCONNECT: {
                    pClient client = (pClient)event.peer->data;
                    printf("%s disconnected\n", client->name.buffer);
                    event.peer->data = NULL;
                    ClientFree(client);
                    currentAllocator->free(client);
                } break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    const Bytes temp = { .allocator = currentAllocator,
                                         .buffer = event.packet->data,
                                         .size = event.packet->dataLength,
                                         .used = event.packet->dataLength };
                    Message message = { 0 };
                    MESSAGE_DESERIALIZE(message, temp);
                    // printReceivedPacket(event.packet);
                    pClient client = (pClient)event.peer->data;
                    switch (handleClientAuthentication(
                      client,
                      &message.general,
                      message.tag == LobbyMessageTag_general,
                      &bytes,
                      &rs)) {
                        case AuthenticateResult_Success:
                            break;
                        case AuthenticateResult_NotNeeded:
                            if (!handleLobbyMessage(
                                  &message, &bytes, event.peer, ctx)) {
                                enet_peer_disconnect(event.peer, 0);
                            }
                            break;
                        default:
                            enet_peer_disconnect(event.peer, 0);
                            break;
                    }
                    LobbyMessageFree(&message);
                    enet_packet_destroy(event.packet);
                } break;
                case ENET_EVENT_TYPE_NONE:
                    break;
                default:
                    break;
            }
        }
    }

end:
    appDone = true;
    redisFree(ctx);
    cleanupServer(server);
    PayloadDataListFree(&payloads);
    uint8_tListFree(&bytes);
    SDL_Quit();
    return result;
}
