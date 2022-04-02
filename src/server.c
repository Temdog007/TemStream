#include <include/main.h>

ServerData globalServerData = { 0 };

// Assigns client a name and id also
bool
authenticateClient(pClient client,
                   const AuthenticateFunc func,
                   const Authentication* auth,
                   pRandomState rs)
{
    char buffer[KB(1)];
    memcpy(buffer, auth->value.buffer, auth->value.used);
    if (func) {
        return func(auth->type, buffer);
    } else {
        switch (auth->type) {
            case 0:
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
        .saveDirectory = TemLangStringCreate("", currentAllocator),
        .port = { .port = 10000u, .tag = PortTag_port },
        .maxClients = 1024u,
        .timeout = STREAM_TIMEOUT,
        .writers = { .anyone = NULL, .tag = AccessTag_anyone },
        .readers = { .anyone = NULL, .tag = AccessTag_anyone },
        .authenticationFunction = NULL,
        .data = { .none = NULL, .tag = ServerConfigurationDataTag_none },
        .record = true
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
    STR_EQUALS(key, "-D", keyLen, { goto parseDirectory; });
    STR_EQUALS(key, "--directory", keyLen, { goto parseDirectory; });
    STR_EQUALS(key, "-R", keyLen, { goto parseRecord; });
    STR_EQUALS(key, "--replay", keyLen, { goto parseRecord; });
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
parseDirectory : {
    TemLangStringFree(&config->saveDirectory);
    config->saveDirectory = TemLangStringCreate(value, currentAllocator);
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
parseRecord : {
    config->record = atoi(value);
    return true;
}
}

int
printAccess(const Access* access)
{
    switch (access->tag) {
        case AccessTag_anyone:
            return puts("Anyone");
        case AccessTag_allowed: {
            int offset = 0;
            printf("Allowed: ");
            for (size_t i = 0; i < access->allowed.used; ++i) {
                offset += printf("%s%s",
                                 access->allowed.buffer[i].buffer,
                                 i == access->allowed.used - 1U ? "\n" : ", ");
            }
            return offset;
        } break;
        case AccessTag_disallowed: {
            int offset = 0;
            printf("Disallowed: ");
            for (size_t i = 0; i < access->disallowed.used; ++i) {
                offset +=
                  printf("%s%s",
                         access->disallowed.buffer[i].buffer,
                         i == access->disallowed.used - 1U ? "\n" : ", ");
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
      printf("Save Directory: %s\nHostname: %s\nRedis: %s:%u\nMax clients: "
             "%u\nTimeout: %" PRIu64
             "\nHas Authentication function: %s\nRecording: %s\n",
             configuration->saveDirectory.buffer,
             configuration->hostname.buffer,
             configuration->redisIp.buffer,
             configuration->redisPort,
             configuration->maxClients,
             configuration->timeout,
             configuration->authenticationFunction == NULL ? "No" : "Yes",
             configuration->record ? "Yes" : " No") +
      printPort(&configuration->port) + printf("Read Access: ") +
      printAccess(&configuration->readers) + printf("Write Access: ") +
      printAccess(&configuration->writers);
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
                     pServerData serverData,
                     pGeneralMessage output)
{
    // Authentication message should have been handled previosly.
    // Don't handle here
    switch (message->tag) {
        case GeneralMessageTag_getClients: {
            output->tag = GeneralMessageTag_getClientsAck;
            output->getClientsAck.allocator = currentAllocator;
            ENetHost* host = serverData->host;
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
                           const AuthenticateFunc func,
                           const GeneralMessage* message,
                           pRandomState rs)
{
    if (GuidEquals(&client->id, &ZeroGuid)) {
        if (message != NULL && message->tag == GeneralMessageTag_authenticate) {
            if (authenticateClient(client, func, &message->authenticate, rs)) {
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
    if (server == NULL) {
        return;
    }
    for (size_t i = 0; i < server->peerCount; ++i) {
        ENetPeer* peer = &server->peers[i];
        pClient client = peer->data;
        if (client == NULL) {
            continue;
        }
        ClientFree(client);
        currentAllocator->free(client);
        peer->data = NULL;
    }
    enet_host_destroy(server);
}

const char*
getServerFileName(const ServerConfiguration* config, char buffer[512])
{
    if (TemLangStringIsEmpty(&config->saveDirectory)) {
        snprintf(buffer, 512, "%s.temstream", config->name.buffer);
    } else {
        snprintf(buffer,
                 512,
                 "%s/%s.temstream",
                 config->saveDirectory.buffer,
                 config->name.buffer);
    }
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

bool
lowMemory()
{
    return currentAllocator->used() >
           (currentAllocator->totalSize() * 95u / 100u);
}

int
VerifyClientPacket(ENetHost* host, ENetEvent* e)
{
    (void)e;
    if (host->receivedDataLength > MAX_PACKET_SIZE || lowMemory()) {
#if _DEBUG
        puts("Dropping packet");
#endif
        return 1;
    }
    const ENetProtocolHeader* header = (ENetProtocolHeader*)host->receivedData;
    const enet_uint16 peerId = ENET_NET_TO_HOST_16(header->peerID);

    ENetPeer* peer = &host->peers[peerId];
    switch (peer->state) {
        case ENET_PEER_STATE_CONNECTED: {
            pClient client = peer->data;
            if (client == NULL) {
                break;
            }
            return clientHasWriteAccess(client, globalServerData.config) ? 0
                                                                         : 1;
        } break;
        default:
            break;
    }
    return 0;
}

#define CHECK_SERVER                                                           \
    globalServerData.host =                                                    \
      enet_host_create(&address, config->maxClients, 2, 0, 0);                 \
    if (globalServerData.host != NULL) {                                       \
        char buffer[1024] = { 0 };                                             \
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));            \
        printf("Opened server at %s:%u\n", buffer, address.port);              \
        if (config->data.tag != ServerConfigurationDataTag_lobby) {            \
            config->port.tag = PortTag_port;                                   \
            config->port.port = address.port;                                  \
        }                                                                      \
        globalServerData.host->maximumPacketSize = MB(1);                      \
        goto continueServer;                                                   \
    }

void
checkClientTime(uint64_t* lastCheck, const ServerConfiguration* config)
{
    const uint64_t now = SDL_GetTicks64();
    uint64_t connectedPeers = 0;
    for (size_t i = 0; i < globalServerData.host->peerCount; ++i) {
        ENetPeer* peer = &globalServerData.host->peers[i];
        pClient client = peer->data;
        if (client == NULL) {
            continue;
        }
        ++connectedPeers;
        if (now - client->joinTime > 10000LL &&
            GuidEquals(&client->id, &ZeroGuid)) {
            char buffer[KB(1)] = { 0 };
            enet_address_get_host_ip(&peer->address, buffer, sizeof(buffer));
            printf("Removing client '%s:%u' because it failed to send "
                   "authentication\n",
                   buffer,
                   peer->address.port);
            enet_peer_disconnect(peer, 0);
        }
    }
    if (connectedPeers == 0) {
        if (config->timeout > 0 && (now - *lastCheck) > config->timeout) {
            printf("Ending server '%s(%s)' due to no connected clients in "
                   "%" PRIu64 " second(s)\n",
                   config->name.buffer,
                   ServerConfigurationDataTagToCharString(config->data.tag),
                   config->timeout / 1000U);
            appDone = true;
        }
    } else {
        *lastCheck = now;
    }
}

bool
serverCanRecord(const ServerConfiguration* config)
{
    switch (config->data.tag) {
        case ServerConfigurationDataTag_lobby:
        case ServerConfigurationDataTag_replay:
            return false;
        default:
            return true;
    }
}

int
runServer(pConfiguration configuration, ServerFunctions funcs)
{
    PRINT_MEMORY;

    int result = EXIT_FAILURE;

    globalServerData.config = &configuration->server;
    globalServerData.bytes = (Bytes){ .allocator = currentAllocator };
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    printf(
      "Running server: '%s(%s)'\n",
      configuration->server.name.buffer,
      ServerConfigurationDataTagToCharString(configuration->server.data.tag));
    printConfiguration(configuration);

    PRINT_MEMORY;
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
    PRINT_MEMORY;

    globalServerData.host->intercept = VerifyClientPacket;

continueServer:
    PRINT_MEMORY;
    globalServerData.ctx =
      redisConnect(config->redisIp.buffer, config->redisPort);
    if (globalServerData.ctx == NULL || globalServerData.ctx->err) {
        if (globalServerData.ctx == NULL) {
            fprintf(stderr, "Can't make redis context\n");
        } else {
            fprintf(stderr, "Redis error: %s\n", globalServerData.ctx->errstr);
        }
        goto end;
    }

    PRINT_MEMORY;
    if (config->data.tag == ServerConfigurationDataTag_lobby) {
        cleanupConfigurationsInRedis(globalServerData.ctx);
    } else if (!writeConfigurationToRedis(globalServerData.ctx, config)) {
        fprintf(stderr, "Failed to write to redis\n");
        goto end;
    }
    PRINT_MEMORY;

    RandomState rs = makeRandomState();
    ENetEvent event = { 0 };
    uint64_t lastCheck = SDL_GetTicks64();
    while (!appDone) {
        while (!appDone &&
               enet_host_service(globalServerData.host, &event, 100U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    if (event.data != (enet_uint32)config->data.tag) {
                        enet_peer_disconnect(event.peer, 0);
                        break;
                    }
                    char buffer[KB(1)] = { 0 };
                    enet_address_get_host_ip(
                      &event.peer->address, buffer, sizeof(buffer));
                    printf("New client from %s:%u\n",
                           buffer,
                           event.peer->address.port);
                    pClient client = currentAllocator->allocate(sizeof(Client));
                    client->payload.allocator = currentAllocator;
                    client->name.allocator = currentAllocator;
                    client->joinTime = SDL_GetTicks64();
                    // name and id will be set after parsing authentication
                    // message
                    event.peer->data = client;
                } break;
                case ENET_EVENT_TYPE_DISCONNECT: {
                    pClient client = (pClient)event.peer->data;
                    if (client == NULL) {
                        break;
                    }
                    printf("%s disconnected\n", client->name.buffer);
                    event.peer->data = NULL;
                    ClientFree(client);
                    currentAllocator->free(client);
                } break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    pClient client = (pClient)event.peer->data;
                    if (client == NULL) {
                        fprintf(stderr, "Got peer without a client\n");
                        enet_peer_disconnect(event.peer, 0);
                        enet_packet_destroy(event.packet);
                        break;
                    }
                    const Bytes temp = { .allocator = currentAllocator,
                                         .buffer = event.packet->data,
                                         .size = event.packet->dataLength,
                                         .used = event.packet->dataLength };

                    void* message = funcs.deserializeMessage(&temp);

                    switch (handleClientAuthentication(
                      client,
                      config->authenticationFunction,
                      funcs.getGeneralMessage(message),
                      &rs)) {
                        case AuthenticateResult_Success: {
                            if ((!clientHasReadAccess(client, config) &&
                                 !clientHasWriteAccess(client, config))) {
                                enet_peer_disconnect(event.peer, 0);
                                break;
                            }

                            GeneralMessage gm = { 0 };
                            gm.tag = GeneralMessageTag_authenticateAck;
                            TemLangStringCopy(&gm.authenticateAck,
                                              &client->name,
                                              currentAllocator);
                            funcs.sendGeneral(
                              &gm, &globalServerData.bytes, event.peer);
                            GeneralMessageFree(&gm);

                            if (!funcs.onConnect(event.peer,
                                                 &globalServerData)) {
                                enet_peer_disconnect(event.peer, 0);
                                break;
                            }
                        } break;
                        case AuthenticateResult_NotNeeded:
                            if (!funcs.handleMessage(
                                  message, event.peer, &globalServerData)) {
                                printf("Disconnecting %s\n",
                                       client->name.buffer);
                                enet_peer_disconnect(event.peer, 0);
                            }
                            const GeneralMessage* m =
                              funcs.getGeneralMessage(message);
                            if (m == NULL && config->record &&
                                serverCanRecord(config)) {
                                ServerMessage srvMssage =
                                  funcs.getServerMessage(message);
                                storeClientMessage(&globalServerData,
                                                   &srvMssage);
                                ServerMessageFree(&srvMssage);
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
            checkClientTime(&lastCheck, config);
        }
        checkClientTime(&lastCheck, config);
        funcs.onDownTime(&globalServerData);
    }

    result = EXIT_SUCCESS;

end:
    PRINT_MEMORY;
    appDone = true;
    while (SDL_AtomicGet(&runningThreads) > 0) {
        SDL_Delay(1);
    }
    ServerDataFree(&globalServerData);
    SDL_Quit();
    return result;
}

void
ServerDataFree(pServerData server)
{
    cleanupServer(server->host);
    PRINT_MEMORY;
    uint8_tListFree(&server->bytes);
    PRINT_MEMORY;
    if (server->ctx != NULL) {
        PRINT_MEMORY;
        removeConfigurationFromRedis(server->ctx, server->config);
        PRINT_MEMORY;
        redisFree(server->ctx);
        PRINT_MEMORY;
    }
    memset(server, 0, sizeof(ServerData));
}