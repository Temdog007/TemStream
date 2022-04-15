#include <include/main.h>

bool
authenticateClient(pClient client,
                   const AuthenticateFunc func,
                   const char* credentials)
{
    if (func) {
        char buffer[256] = { 0 };
        client->role = (ClientRole)func(credentials, buffer);
        client->name = TemLangStringCreate(buffer, currentAllocator);
        return clientHasRole(client, ClientRole_Admin) ||
               clientHasRole(client, ClientRole_Producer) ||
               clientHasRole(client, ClientRole_Consumer);
    } else {
        // Give client random name and id
        client->name = RandomClientName(&globalServerData.rs);
        client->role =
          ClientRole_Consumer | ClientRole_Producer | ClientRole_Admin;
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
        .port = 10000u,
        .maxClients = 1024u,
        .timeout = STREAM_TIMEOUT,
        .authenticationHandle = NULL,
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
    STR_EQUALS(key, "-N", keyLen, { goto parseName; });
    STR_EQUALS(key, "--name", keyLen, { goto parseName; });
    STR_EQUALS(key, "-A", keyLen, { goto parseAuthentication; });
    STR_EQUALS(key, "--authentication", keyLen, { goto parseAuthentication; });
    return false;

parseName : {
    TemLangStringFree(&config->name);
    config->name = TemLangStringCreate(value, currentAllocator);
    return true;
}
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
parsePort : {
    const int i = atoi(value);
    config->port = SDL_clamp(i, 1000, 60000);
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
parseAuthentication : {
    if (config->authenticationHandle != NULL) {
        dlclose(config->authenticationHandle);
    }
    config->authenticationHandle = dlopen(value, RTLD_LAZY);
    return config->authenticationHandle != NULL;
}
}

int
printServerConfiguration(const ServerConfiguration* configuration)
{
    int offset =
      printf("Save Directory: %s\nHostname: %s\nPort: %u\nRedis: %s:%u\nMax "
             "clients: %u\nTimeout: %" PRIu64
             "\nHas Authentication function: %s\nRecording: %s\n",
             configuration->saveDirectory.buffer,
             configuration->hostname.buffer,
             configuration->port,
             configuration->redisIp.buffer,
             configuration->redisPort,
             configuration->maxClients,
             configuration->timeout,
             configuration->authenticationHandle == NULL ? "No" : "Yes",
             configuration->record ? "Yes" : " No");
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
        case ServerConfigurationDataTag_video:
            offset += printVideoConfiguration(&configuration->data.video);
            break;
        case ServerConfigurationDataTag_audio:
            offset += printAudioConfiguration(&configuration->data.audio);
            break;
        case ServerConfigurationDataTag_image:
            offset += printImageConfiguration(&configuration->data.image);
            break;
        case ServerConfigurationDataTag_replay:
            offset += printReplayConfiguration(&configuration->data.replay);
            break;
        default:
            break;
    }
    return offset;
}

bool
handleGeneralMessage(const GeneralMessage* message,
                     ENetPeer* peer,
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
                ClientListAppend(&output->getClientsAck, client);
            }
        } break;
        case GeneralMessageTag_removeClient: {
            pClient sender = peer->data;
            if (sender == NULL) {
                return false;
            }
            if (!clientHasRole(sender, ClientRole_Admin)) {
                return false;
            }
            ENetHost* host = serverData->host;
            for (size_t i = 0; i < host->peerCount; ++i) {
                ENetPeer* p = &host->peers[i];
                pClient client = p->data;
                if (client == NULL) {
                    continue;
                }
                if (TemLangStringsAreEqual(&client->name,
                                           &message->removeClient)) {
                    enet_peer_disconnect(p, 0);
                    break;
                }
            }
        } break;
        default:
#if _DEBUG
            printf("Unexpected general message '%s' from client\n",
                   GeneralMessageTagToCharString(message->tag));
#endif
            return false;
    }
    return true;
}

AuthenticateResult
handleClientAuthentication(pClient client,
                           const AuthenticateFunc func,
                           const GeneralMessage* message)
{
    if (client->name.buffer == NULL) {
        if (message != NULL && message->tag == GeneralMessageTag_authenticate) {
            if (authenticateClient(
                  client, func, message->authenticate.buffer)) {
                TemLangString s = getAllRoles(client);
                printf("Client assigned name '%s' (%s)\n",
                       client->name.buffer,
                       s.buffer);
                TemLangStringFree(&s);
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
getServerFileName(const ServerConfiguration* config, char buffer[KB(1)])
{
    if (TemLangStringIsEmpty(&config->saveDirectory)) {
        snprintf(buffer, KB(1), "%s.temstream", config->name.buffer);
    } else {
        snprintf(buffer,
                 KB(1),
                 "%s/%s.temstream",
                 config->saveDirectory.buffer,
                 config->name.buffer);
    }
    return buffer;
}

bool
getServerFileBytes(const ServerConfiguration* config, pBytes bytes)
{
    char buffer[KB(1)];
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

int
VerifyClientPacket(ENetHost* host, ENetEvent* e)
{
    (void)e;
    if (lowMemory()) {
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
            return (clientHasRole(client, ClientRole_Admin) ||
                    clientHasRole(client, ClientRole_Producer))
                     ? 0
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
            config->port = address.port;                                       \
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
        if (now - client->joinTime > 10000LL && client->name.buffer == NULL) {
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
        address.port = config->port;
        CHECK_SERVER;
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

    AuthenticateFunc func = NULL;
    if (globalServerData.config->authenticationHandle != NULL) {
        func =
          dlsym(globalServerData.config->authenticationHandle, "authenticate");
    }

    globalServerData.rs = makeRandomState();
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
                      client, func, funcs.getGeneralMessage(message))) {
                        case AuthenticateResult_Success: {
                            GeneralMessage gm = { 0 };
                            gm.tag = GeneralMessageTag_authenticateAck;
                            ClientCopy(
                              &gm.authenticateAck, client, currentAllocator);
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
