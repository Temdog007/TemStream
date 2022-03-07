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
        .timeout = 10u * 1000u,
        .writers = { .anyone = NULL, .tag = AccessTag_anyone },
        .readers = { .anyone = NULL, .tag = AccessTag_anyone },
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
printAccess(const Access* access)
{
    switch (access->tag) {
        case AccessTag_anyone:
            return puts("Anyone");
        case AccessTag_list: {
            int offset = 0;
            for (size_t i = 0; i < access->list.used; ++i) {
                offset += printf("%s%s",
                                 access->list.buffer[i].buffer,
                                 i == access->list.used - 1U ? "\n" : ", ");
            }
            return offset;
        } break;
        default:
            return 0;
    }
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
      printServerAuthentication(&configuration->authentication) +
      printf("Read Access: ") + printAccess(&configuration->readers) +
      printf("Write Access: ") + printAccess(&configuration->writers);
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
    return printf("%s (%s)\n",
                  config->name.buffer,
                  ServerConfigurationDataTagToCharString(config->data.tag));
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

const char*
getServerFileName(const ServerConfiguration* config, char buffer[512])
{
    snprintf(buffer, 512, "%s.temstream", config->name.buffer);
    return buffer;
}

bool
getServerFileBytes(const ServerConfiguration* config, pBytes bytes)
{
    char buffer[512];
    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;
    if (!mapFile(getServerFileName(config, buffer),
                 &fd,
                 &ptr,
                 &size,
                 MapFileType_Read)) {
        perror("Failed to open file");
        return false;
    }

    bytes->used = 0;
    const bool result = uint8_tListQuickAppend(bytes, (uint8_t*)ptr, size);
    unmapFile(fd, ptr, size);
    return result;
}

void
writeServerFileBytes(const ServerConfiguration* config,
                     const Bytes* bytes,
                     const bool overwrite)
{
    char buffer[512];
    FILE* file = fopen(getServerFileName(config, buffer),
                       bytes == NULL || overwrite ? "wb" : "ab");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }
    if (bytes != NULL) {
        fwrite(bytes->buffer, sizeof(uint8_t), bytes->used, file);
    }
    fclose(file);
}

#define CHECK_SERVER                                                           \
    server = enet_host_create(                                                 \
      &address, config->maxClients, 2, currentAllocator->totalSize() / 4, 0);  \
    if (server != NULL) {                                                      \
        char buffer[1024] = { 0 };                                             \
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));            \
        printf("Opened server at %s:%u\n", buffer, address.port);              \
        if (config->data.tag != ServerConfigurationDataTag_lobby) {            \
            config->port.tag = PortTag_port;                                   \
            config->port.port = address.port;                                  \
        }                                                                      \
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

    printf(
      "Running server: '%s(%s)'\n",
      configuration->server.name.buffer,
      ServerConfigurationDataTagToCharString(configuration->server.data.tag));
    printConfiguration(configuration);

    appDone = false;
    SDL_AtomicSet(&runningThreads, 0);

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

    if (config->data.tag == ServerConfigurationDataTag_lobby) {
        cleanupConfigurationsInRedis(ctx);
    } else if (!writeConfigurationToRedis(ctx, config)) {
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
                            if ((!clientHasReadAccess(client, config) &&
                                 !clientHasWriteAccess(client, config)) ||
                                !funcs.onConnect(
                                  client, &bytes, event.peer, config)) {
                                enet_peer_disconnect(event.peer, 0);
                                break;
                            }
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
                       "authentication\n",
                       buffer,
                       peer->address.port);
                enet_peer_disconnect(peer, 0);
            }
        }
        if (connectedPeers == 0) {
            if (config->timeout > 0 && now - lastCheck > config->timeout) {
                printf("Ending server '%s(%s)' due to no connected clients in "
                       "%" PRIu64 " seconds\n",
                       config->name.buffer,
                       ServerConfigurationDataTagToCharString(config->data.tag),
                       config->timeout / 1000U);
                appDone = true;
            }
        } else {
            lastCheck = now;
        }
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    while (SDL_AtomicGet(&runningThreads) > 0) {
        SDL_Delay(1);
    }
    removeConfigurationFromRedis(ctx, config);
    redisFree(ctx);
    uint8_tListFree(&bytes);
    cleanupServer(server);
    SDL_Quit();
    return result;
}
