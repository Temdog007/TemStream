#include <include/main.h>

void
cleanupServer(ENetHost*);

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
clientHasReadAccess(const Client* client, const Stream* stream)
{
    return clientHasAccess(client, &stream->readers);
}

bool
clientHasWriteAccess(const Client* client, const Stream* stream)
{
    return clientHasAccess(client, &stream->writers);
}

TextConfiguration
defaultTextConfiguration()
{
    return (TextConfiguration){ .maxLength = 4096 };
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
        .maxClients = 1024u,
        .redisIp = TemLangStringCreate("localhost", currentAllocator),
        .redisPort = 6379,
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
    STR_EQUALS(key, "-RI", keyLen, { goto parseIp; });
    STR_EQUALS(key, "--redis-ip", keyLen, { goto parseIp; });
    STR_EQUALS(key, "-RP", keyLen, { goto parsePort; });
    STR_EQUALS(key, "--redis-port", keyLen, { goto parsePort; });
    // TODO: parse authentication
    return false;

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
parsePort : {
    const int i = atoi(value);
    config->redisPort = SDL_clamp(value, 1000, 60000);
    return true;
}
}

bool
parseTextConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->data.tag = ServerConfigurationDataTag_text;
    pTextConfiguration text = &configuration->data.server.data.text;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-L", keyLen, { goto parseLength; });
        STR_EQUALS(key, "--max-length", keyLen, { goto parseLength; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(
              key, value, &configuration->data.server)) {
            parseFailure("Text", key, value);
            return false;
        }
        continue;

    parseLength : {
        const int i = atoi(value);
        text->maxLength = SDL_max(i, 32);
        continue;
    }
    }
    return true;
}

bool
parseChatConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->data.tag = ServerConfigurationDataTag_chat;
    pChatConfiguration chat = &configuration->data.server.data.chat;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-I", keyLen, { goto parseInterval; });
        STR_EQUALS(key, "--interval", keyLen, { goto parseInterval; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(
              key, value, &configuration->data.server)) {
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
    int offset = printf("Max clients: %u\n", configuration->maxClients) +
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
printLobbyConfiguration(const LobbyConfiguration* configuration)
{
    return printf("Max streams: %u\nMin port: %u\nMax port: %u\n",
                  configuration->maxStreams,
                  configuration->minPort,
                  configuration->maxPort);
}

AuthenticateResult
handleClientAuthentication(pClient client,
                           const ServerAuthentication* sAuth,
                           const GeneralMessage* message,
                           pBytes bytes,
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
                   MessageTagToCharString(message->tag));
            return AuthenticateResult_Failed;
        }
    }
    return message == NULL ? AuthenticateResult_Failed
                           : AuthenticateResult_NotNeeded;
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

PayloadParseResult
parsePayload(const Payload* payload, pClient client)
{
    switch (payload->tag) {
        case PayloadTag_dataStart:
            client->payload.used = 0;
            break;
        case PayloadTag_dataEnd:
            return PayloadParseResult_Done;
        case PayloadTag_dataChunk:
            uint8_tListQuickAppend(&client->payload,
                                   payload->dataChunk.buffer,
                                   payload->dataChunk.used);
            break;
        case PayloadTag_fullData:
            return PayloadParseResult_UsePayload;
        default:
            break;
    }
    return PayloadParseResult_Continuing;
}

void
sendBytes(ENetPeer* peers,
          const size_t peerCount,
          const size_t mtu,
          const Bytes* bytes,
          const bool reliable)
{
    if (bytes->used > mtu) {
        size_t offset = 0;
        for (const size_t n = bytes->used - mtu; offset < n; offset += mtu) {
            ENetPacket* packet = BytesToPacket(
              bytes->buffer + offset, bytes->used - offset, reliable);
            for (size_t i = 0; i < peerCount; ++i) {
                PEER_SEND(&peers[i], SERVER_CHANNEL, packet);
            }
        }
        if (offset < bytes->used) {
            ENetPacket* packet = BytesToPacket(
              bytes->buffer + offset, bytes->used - offset, reliable);
            for (size_t i = 0; i < peerCount; ++i) {
                PEER_SEND(&peers[i], SERVER_CHANNEL, packet);
            }
        }
    } else {
        ENetPacket* packet =
          BytesToPacket(bytes->buffer, bytes->used, reliable);
        for (size_t i = 0; i < peerCount; ++i) {
            PEER_SEND(&peers[i], SERVER_CHANNEL, packet);
        }
    }
}

int
runServer(const int argc,
          const char* argv,
          pConfiguration configuration,
          ServerFunctions funcs)
{
    int result = funcs.parseConfiguration(argc, argv, configuration);
    if (result != EXIT_SUCCESS) {
        return result;
    }

    redisContext* ctx = NULL;
    ENetHost* server = NULL;
    Bytes bytes = { .allocator = currentAllocator };
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    printf("Running %s server", funcs.name);
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

    ctx = redisConnect(config->redisIp.buffer, config->redisPort);
    if (ctx == NULL || ctx->err) {
        if (ctx == NULL) {
            fprintf(stderr, "Can't make redis context\n");
        } else {
            fprintf(stderr, "Error: %s\n", ctx->errstr);
        }
        goto end;
    }

    if (!funcs.init(ctx)) {
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
                    pClient client =
                      (pClient)
                        event.peer->data; // printReceivedPacket(event.packet);
                    const Bytes temp = { .allocator = currentAllocator,
                                         .buffer = event.packet->data,
                                         .size = event.packet->dataLength,
                                         .used = event.packet->dataLength };
                    Payload payload = { 0 };
                    MESSAGE_DESERIALIZE(Payload, payload, temp);

                    void* message = NULL;

                    switch (parsePayload(&payload, client)) {
                        case PayloadParseResult_UsePayload:
                            message =
                              funcs.deserializeMessage(&payload.fullData);
                            break;
                        case PayloadParseResult_Done:
                            message =
                              funcs.deserializeMessage(&client->payload);
                            break;
                        default:
                            goto fend;
                    }

                    switch (handleClientAuthentication(
                      client,
                      &config->authentication,
                      funcs.getGeneralMessage(message),
                      &bytes,
                      &rs)) {
                        case AuthenticateResult_Success:
                            break;
                        case AuthenticateResult_NotNeeded:
                            if (!funcs.handleMessage(
                                  message, &bytes, event.peer, ctx)) {
                                enet_peer_disconnect(event.peer, 0);
                            }
                            break;
                        default:
                            enet_peer_disconnect(event.peer, 0);
                            break;
                    }
                fend:
                    PayloadFree(&payload);
                    funcs.freeMessage(message);
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
    funcs.close(ctx);
    redisFree(ctx);
    cleanupServer(server);
    uint8_tListFree(&bytes);
    SDL_Quit();
    return result;
}
