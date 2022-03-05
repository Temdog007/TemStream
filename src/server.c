#include <include/main.h>

// Assigns client a name and id also
bool
authenticateClient(pClient client,
                   const ServerAuthentication* sAuth,
                   const ClientAuthentication* cAuth,
                   pRandomState rs)
{
    switch (sAuth->tag) {
        case ServerAuthenticationTag_file:
            fprintf(stderr, "Failed authentication is not implemented\n");
            return false;
        default:
            switch (cAuth->tag) {
                case ClientAuthenticationTag_credentials:
                    client->id = randomGuid(rs);
                    return TemLangStringCopy(&client->name,
                                             &cAuth->credentials.username,
                                             currentAllocator);
                case ClientAuthenticationTag_token:
                    fprintf(stderr,
                            "Token authentication is not implemented\n");
                    return false;
                case ClientAuthenticationTag_none:
                    // Give client random name and id
                    client->name = RandomClientName(rs);
                    client->id = randomGuid(rs);
                    return true;
                default:
#if _DEBUG
                    puts("Unknown authentication type from client");
#endif
                    return false;
            }
            return true;
    }
}

bool
clientHasAccess(const Client* client, const Access* access)
{
    switch (access->tag) {
        case AccessTag_anyone:
            return true;
        case AccessTag_list:
            return TemLangStringListFindIf(
              &access->list,
              (TemLangStringListFindFunc)TemLangStringsAreEqual,
              &client->name,
              NULL,
              NULL);
        default:
            break;
    }
    return false;
}

bool
clientHasReadAccess(const Client* client, const ServerConfiguration* config)
{
    return clientHasAccess(client, &config->readers);
}

bool
clientHasWriteAccess(const Client* client, const ServerConfiguration* config)
{
    return clientHasAccess(client, &config->writers);
}

ImageConfiguration
defaultImageConfiguration()
{
    return (ImageConfiguration){ .none = NULL };
}

AudioConfiguration
defaultAudioConfiguration()
{
    return (AudioConfiguration){ .none = NULL };
}

ChatConfiguration
defaultChatConfiguration()
{
    return (ChatConfiguration){ .chatInterval = 5 };
}

ServerConfiguration
defaultServerConfiguration()
{
    return (ServerConfiguration){
        .name = TemLangStringCreate("server", currentAllocator),
        .redisIp = TemLangStringCreate("localhost", currentAllocator),
        .redisPort = 6379,
        .hostname = TemLangStringCreate("localhost", currentAllocator),
        .port = { .port = 10000u, .tag = PortTag_port },
        .maxClients = 1024u,
        .authentication = { .none = NULL, .tag = ServerAuthenticationTag_none },
        .data = { .none = NULL, .tag = ServerConfigurationDataTag_none }
    };
}

bool
parseServerConfiguration(const char* key,
                         const char* value,
                         pServerConfiguration config)
{
    const size_t keyLen = strlen(key);
    STR_EQUALS(key, "-C", keyLen, { goto parseMaxClients; });
    STR_EQUALS(key, "--max-clients", keyLen, { goto parseMaxClients; });
    STR_EQUALS(key, "-H", keyLen, { goto parseHostname; });
    STR_EQUALS(key, "--host-name", keyLen, { goto parseHostname; });
    STR_EQUALS(key, "-mp", keyLen, { goto parseMinPort; });
    STR_EQUALS(key, "--min-port", keyLen, { goto parseMinPort; });
    STR_EQUALS(key, "-MP", keyLen, { goto parseMaxPort; });
    STR_EQUALS(key, "--max-port", keyLen, { goto parseMaxPort; });
    STR_EQUALS(key, "-P", keyLen, { goto parsePort; });
    STR_EQUALS(key, "--port", keyLen, { goto parsePort; });
    STR_EQUALS(key, "-RI", keyLen, { goto parseIp; });
    STR_EQUALS(key, "--redis-ip", keyLen, { goto parseIp; });
    STR_EQUALS(key, "-RP", keyLen, { goto parseRedisPort; });
    STR_EQUALS(key, "--redis-port", keyLen, { goto parseRedisPort; });
    STR_EQUALS(key, "-T", keyLen, { goto parseTimeout; });
    STR_EQUALS(key, "--timeout", keyLen, { goto parseTimeout; });
    // TODO: parse authentication
    return false;

parseHostname : {
    TemLangStringFree(&config->hostname);
    config->hostname = TemLangStringCreate(value, currentAllocator);
    return true;
}
parseMaxClients : {
    const int i = atoi(value);
    config->maxClients = SDL_max(i, 1);
    return true;
}
parseIp : {
    TemLangStringFree(&config->redisIp);
    config->redisIp = TemLangStringCreate(value, currentAllocator);
    return true;
}
parseRedisPort : {
    const int i = atoi(value);
    config->redisPort = SDL_clamp(i, 1000, 60000);
    return true;
}
parseMinPort : {
    const int i = atoi(value);
    config->port.tag = PortTag_portRange;
    config->port.portRange.minPort = SDL_clamp(i, 1000, 60000);
    return true;
}
parseMaxPort : {
    const int i = atoi(value);
    config->port.tag = PortTag_portRange;
    config->port.portRange.maxPort = SDL_clamp(i, 1000, 60000);
    return true;
}
parsePort : {
    const int i = atoi(value);
    config->port.tag = PortTag_port;
    config->port.port = SDL_clamp(i, 1000, 60000);
    return true;
}
parseTimeout : {
    const int i = atoi(value);
    config->timeout = SDL_max(i, 0);
    return true;
}
}

bool
parseChatConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->tag = ServerConfigurationDataTag_chat;
    pChatConfiguration chat = &configuration->server.data.chat;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-I", keyLen, { goto parseInterval; });
        STR_EQUALS(key, "--interval", keyLen, { goto parseInterval; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Chat", key, value);
            return false;
        }
        continue;

    parseInterval : {
        const int i = atoi(value);
        chat->chatInterval = SDL_max(i, 32);
        continue;
    }
    }
    return true;
}

int
printAuthenticate(const ServerAuthentication* auth)
{
    int offset = printf("Autentication: ");
    switch (auth->tag) {
        case ServerAuthenticationTag_file:
            offset += printf("from file (%s)\n", auth->file.buffer);
            break;
        default:
            offset += puts("none");
            break;
    }
    return offset;
}

int
printServerConfiguration(const ServerConfiguration* configuration)
{
    int offset =
      printf("Hostname: %s\nRedis: %s:%u\nMax clients: %u\nTimeout: %" PRIu64
             "\n",
             configuration->hostname.buffer,
             configuration->redisIp.buffer,
             configuration->redisPort,
             configuration->maxClients,
             configuration->timeout) +
      printPort(&configuration->port) +
      printServerAuthentication(&configuration->authentication);
    switch (configuration->data.tag) {
        case ServerConfigurationDataTag_chat:
            offset += printChatConfiguration(&configuration->data.chat);
            break;
        case ServerConfigurationDataTag_text:
            offset += printTextConfiguration(&configuration->data.text);
            break;
        case ServerConfigurationDataTag_lobby:
            offset += printLobbyConfiguration(&configuration->data.lobby);
            break;
        default:
            break;
    }
    return offset;
}

int
printServerConfigurationForClient(const ServerConfiguration* config)
{
    return printf("%s (%s)",
                  config->name.buffer,
                  ServerConfigurationDataTagToCharString(config->data.tag));
}

int
printTextConfiguration(const TextConfiguration* configuration)
{
    return printf("Text\nMax length: %u\n", configuration->maxLength);
}

int
printChatConfiguration(const ChatConfiguration* configuration)
{
    return printf("Chat\nMessage interval: %u second(s)\n",
                  configuration->chatInterval);
}

int
printLobbyConfiguration(const LobbyConfiguration* configuration)
{
    return printf("Lobby\nMax streams: %u\n", configuration->maxStreams);
}

bool
handleGeneralMessage(const GeneralMessage* message,
                     ENetPeer* peer,
                     pGeneralMessage output)
{
    // Authentication message should have been handled previosly.
    // Don't handle here
    switch (message->tag) {
        case GeneralMessageTag_getClients: {
            output->tag = GeneralMessageTag_getClientsAck;
            output->getClientsAck.allocator = currentAllocator;
            ENetHost* host = peer->host;
            for (size_t i = 0; i < host->peerCount; ++i) {
                ENetPeer* p = &host->peers[i];
                pClient client = p->data;
                if (client == NULL) {
                    continue;
                }
                TemLangStringListAppend(&output->getClientsAck, &client->name);
            }
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

AuthenticateResult
handleClientAuthentication(pClient client,
                           const ServerAuthentication* sAuth,
                           const GeneralMessage* message,
                           pRandomState rs)
{
    if (GuidEquals(&client->id, &ZeroGuid)) {
        if (message != NULL && message->tag == GeneralMessageTag_authenticate) {
            if (authenticateClient(client, sAuth, &message->authenticate, rs)) {
                char buffer[128];
                getGuidString(&client->id, buffer);
                printf("Client assigned name '%s' (%s)\n",
                       client->name.buffer,
                       buffer);
                return AuthenticateResult_Success;
            } else {
                puts("Client failed authentication");
                return AuthenticateResult_Failed;
            }
        } else {
            printf("Expected authentication from client. Got '%s'\n",
                   GeneralMessageTagToCharString(message->tag));
            return AuthenticateResult_Failed;
        }
    }
    return AuthenticateResult_NotNeeded;
}

void
cleanupServer(ENetHost* server)
{
    for (size_t i = 0; i < server->peerCount; ++i) {
        pClient client = server->peers[i].data;
        if (client == NULL) {
            continue;
        }
        ClientFree(client);
        currentAllocator->free(client);
        server->peers[i].data = NULL;
    }
    if (server != NULL) {
        enet_host_destroy(server);
    }
}

void
sendBytes(ENetPeer* peers,
          const size_t peerCount,
          const enet_uint32 channel,
          const Bytes* bytes,
          const bool reliable)
{
    ENetPacket* packet = BytesToPacket(bytes->buffer, bytes->used, reliable);
    for (size_t i = 0; i < peerCount; ++i) {
        PEER_SEND(&peers[i], channel, packet);
    }
}

#define CHECK_SERVER                                                           \
    server = enet_host_create(&address, config->maxClients, 2, 0, 0);          \
    if (server != NULL) {                                                      \
        char buffer[1024] = { 0 };                                             \
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));            \
        printf("Opened server at %s:%u\n", buffer, address.port);              \
        config->port.tag = PortTag_port;                                       \
        config->port.port = address.port;                                      \
        goto continueServer;                                                   \
    }

int
runServer(pConfiguration configuration, ServerFunctions funcs)
{
    int result = EXIT_FAILURE;

    redisContext* ctx = NULL;
    ENetHost* server = NULL;
    Bytes bytes = { .allocator = currentAllocator };
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    printf("Running %s server\n", configuration->server.name.buffer);
    printConfiguration(configuration);

    appDone = false;

    pServerConfiguration config = &configuration->server;
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, config->hostname.buffer);
        switch (config->port.tag) {
            case PortTag_port:
                address.port = config->port.port;
                CHECK_SERVER;
                break;
            case PortTag_portRange:
                for (enet_uint16 port = config->port.portRange.minPort;
                     port <= config->port.portRange.maxPort;
                     ++port) {
                    address.port = port;
                    CHECK_SERVER;
                }
                break;
            default:
                break;
        }
        fprintf(stderr, "Failed to create server\n");
        goto end;
    }

continueServer:
    ctx = redisConnect(config->redisIp.buffer, config->redisPort);
    if (ctx == NULL || ctx->err) {
        if (ctx == NULL) {
            fprintf(stderr, "Can't make redis context\n");
        } else {
            fprintf(stderr, "Redis error: %s\n", ctx->errstr);
        }
        goto end;
    }

    if (config->data.tag != ServerConfigurationDataTag_lobby &&
        !writeConfigurationToRedis(ctx, config)) {
        fprintf(stderr, "Failed to write to redis\n");
        goto end;
    }

    RandomState rs = makeRandomState();
    ENetEvent event = { 0 };
    uint64_t lastCheck = SDL_GetTicks64();
    while (!appDone) {
        while (!appDone && enet_host_service(server, &event, 100U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    char buffer[KB(1)] = { 0 };
                    enet_address_get_host_ip(
                      &event.peer->address, buffer, sizeof(buffer));
                    printf("New client from %s:%u\n",
                           buffer,
                           event.peer->address.port);
                    pClient client = currentAllocator->allocate(sizeof(Client));
                    client->payload.allocator = currentAllocator;
                    client->joinTime = SDL_GetTicks64();
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
                    pClient client = (pClient)event.peer->data;
                    const Bytes temp = { .allocator = currentAllocator,
                                         .buffer = event.packet->data,
                                         .size = event.packet->dataLength,
                                         .used = event.packet->dataLength };

                    void* message = funcs.deserializeMessage(&temp);

                    switch (handleClientAuthentication(
                      client,
                      &config->authentication,
                      funcs.getGeneralMessage(message),
                      &rs)) {
                        case AuthenticateResult_Success: {
                            GeneralMessage gm = { 0 };
                            gm.tag = GeneralMessageTag_authenticateAck;
                            TemLangStringCopy(&gm.authenticateAck,
                                              &client->name,
                                              currentAllocator);
                            funcs.sendGeneral(&gm, &bytes, event.peer);
                            GeneralMessageFree(&gm);
                        } break;
                        case AuthenticateResult_NotNeeded:
                            if (!funcs.handleMessage(
                                  message, &bytes, event.peer, ctx, config)) {
                                printf("Disconnecting %s\n",
                                       client->name.buffer);
                                enet_peer_disconnect(event.peer, 0);
                            }
                            break;
                        default:
                            enet_peer_disconnect(event.peer, 0);
                            break;
                    }

                    funcs.freeMessage(message);
                    enet_packet_destroy(event.packet);
                } break;
                case ENET_EVENT_TYPE_NONE:
                    break;
                default:
                    break;
            }
        }
        const uint64_t now = SDL_GetTicks64();
        uint64_t connectedPeers = 0;
        for (size_t i = 0; i < server->peerCount; ++i) {
            ENetPeer* peer = &server->peers[i];
            pClient client = peer->data;
            if (client == NULL) {
                continue;
            }
            ++connectedPeers;
            if (now - client->joinTime > 10000LL &&
                GuidEquals(&client->id, &ZeroGuid)) {
                char buffer[KB(1)] = { 0 };
                enet_address_get_host_ip(
                  &peer->address, buffer, sizeof(buffer));
                printf("Removing client '%s:%u' because it failed to send "
                       "authentication",
                       buffer,
                       peer->address.port);
                enet_peer_disconnect(peer, 0);
            }
        }
        if (connectedPeers == 0 && config->timeout > 0 &&
            now - lastCheck > config->timeout) {
            printf("Ending server due to no connected clients in %" PRIu64
                   " seconds\n",
                   config->timeout);
            appDone = true;
        } else {
            lastCheck = now;
        }
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    removeConfigurationFromRedis(ctx, config);
    redisFree(ctx);
    cleanupServer(server);
    uint8_tListFree(&bytes);
    SDL_Quit();
    return result;
}

#define TEM_STREAM_SERVER_KEY "TemStream Server"

ServerConfigurationList
getStreams(redisContext* ctx)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    redisReply* reply =
      redisCommand(ctx, "LRANGE %s 0 -1", TEM_STREAM_SERVER_KEY);
    if (reply->type != REDIS_REPLY_ARRAY) {
        if (reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Redis error: %s\n", reply->str);
        } else {
            fprintf(stderr, "Failed to get list from redis\n");
        }
        goto end;
    }

    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply* r = reply->element[i];
        if (r->type != REDIS_REPLY_STRING) {
            continue;
        }
        TemLangString str = { .allocator = currentAllocator };
        if (!b64_decode(r->str, &str)) {
            continue;
        }
        Bytes bytes = { .allocator = currentAllocator,
                        .buffer = (uint8_t*)str.buffer,
                        .size = str.size,
                        .used = str.used };

        ServerConfiguration s = { 0 };
        ServerConfigurationDeserialize(&s, &bytes, 0, true);
        ServerConfigurationListAppend(&list, &s);
        ServerConfigurationFree(&s);
    }
end:
    freeReplyObject(reply);
    return list;
}

bool
writeConfigurationToRedis(redisContext* ctx, const ServerConfiguration* c)
{
    Bytes bytes = { .allocator = currentAllocator };
    ServerConfigurationSerialize(c, &bytes, true);

    TemLangString str = b64_encode((unsigned char*)bytes.buffer, bytes.used);

    redisReply* reply =
      redisCommand(ctx, "LPUSH %s %s", TEM_STREAM_SERVER_KEY, str.buffer);

    const bool result =
      reply->type == REDIS_REPLY_INTEGER && reply->integer == 1LL;
    if (!result && reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
    }

    freeReplyObject(reply);
    TemLangStringFree(&str);
    uint8_tListFree(&bytes);
    return result;
}

bool
removeConfigurationFromRedis(redisContext* ctx, const ServerConfiguration* c)
{
    Bytes bytes = { .allocator = currentAllocator };
    ServerConfigurationSerialize(c, &bytes, true);

    TemLangString str = b64_encode((unsigned char*)bytes.buffer, bytes.used);

    redisReply* reply =
      redisCommand(ctx, "LREM %s 0 %s", TEM_STREAM_SERVER_KEY, str.buffer);

    const bool result =
      reply->type == REDIS_REPLY_INTEGER && reply->integer == 1LL;
    if (!result && reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
    }

    freeReplyObject(reply);
    TemLangStringFree(&str);
    uint8_tListFree(&bytes);
    return result;
}

void
sendPacketToReaders(ENetHost* host, ENetPacket* packet, const Access* acccess)
{
    for (size_t i = 0; i < host->peerCount; ++i) {
        ENetPeer* peer = &host->peers[i];
        pClient client = peer->data;
        if (client == NULL || !clientHasAccess(client, acccess)) {
            continue;
        }
        PEER_SEND(peer, SERVER_CHANNEL, packet);
    }
}