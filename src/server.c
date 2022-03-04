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
        .name = TemLangStringCreate("server", currentAllocator),
        .redisIp = TemLangStringCreate("localhost", currentAllocator),
        .redisPort = 6379,
        .hostname = TemLangStringCreate("localhost", currentAllocator),
        .minPort = 10000u,
        .maxPort = 10255u,
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
    STR_EQUALS(key, "-RI", keyLen, { goto parseIp; });
    STR_EQUALS(key, "--redis-ip", keyLen, { goto parseIp; });
    STR_EQUALS(key, "-RP", keyLen, { goto parsePort; });
    STR_EQUALS(key, "--redis-port", keyLen, { goto parsePort; });
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
parsePort : {
    const int i = atoi(value);
    config->redisPort = SDL_clamp(i, 1000, 60000);
    return true;
}
parseMinPort : {
    const int i = atoi(value);
    config->minPort = SDL_clamp(i, 1000, 60000);
    return true;
}
parseMaxPort : {
    const int i = atoi(value);
    config->maxPort = SDL_clamp(i, 1000, 60000);
    return true;
}
}

bool
parseTextConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->tag = ServerConfigurationDataTag_text;
    pTextConfiguration text = &configuration->server.data.text;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-L", keyLen, { goto parseLength; });
        STR_EQUALS(key, "--max-length", keyLen, { goto parseLength; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
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
      printf("Hostname: %s\nPorts: %u-%u\nRedis: %s:%u\nMax clients: %u\n",
             configuration->hostname.buffer,
             configuration->minPort,
             configuration->maxPort,
             configuration->redisIp.buffer,
             configuration->redisPort,
             configuration->maxClients) +
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
          const enet_uint32 channel,
          const Bytes* bytes,
          const bool reliable)
{
    Bytes newBytes = { .allocator = currentAllocator };
    if (bytes->used > mtu) {
        {
            Payload payload = { .tag = PayloadTag_dataStart,
                                .dataStart = NULL };
            MESSAGE_SERIALIZE(Payload, payload, newBytes);
            ENetPacket* packet =
              BytesToPacket(newBytes.buffer, newBytes.used, true);
            for (size_t i = 0; i < peerCount; ++i) {
                PEER_SEND(&peers[i], channel, packet);
            }
        }

        size_t offset = 0;
        for (const size_t n = bytes->used - mtu; offset < n; offset += mtu) {
            Payload payload = { .tag = PayloadTag_dataChunk,
                                .dataChunk = { .buffer = bytes->buffer + offset,
                                               .used = bytes->used - offset } };
            MESSAGE_SERIALIZE(Payload, payload, newBytes);
            ENetPacket* packet =
              BytesToPacket(newBytes.buffer, newBytes.used, reliable);
            for (size_t i = 0; i < peerCount; ++i) {
                PEER_SEND(&peers[i], channel, packet);
            }
        }
        if (offset < bytes->used) {
            ENetPacket* packet = BytesToPacket(
              bytes->buffer + offset, bytes->used - offset, reliable);
            for (size_t i = 0; i < peerCount; ++i) {
                PEER_SEND(&peers[i], channel, packet);
            }
        }

        {
            Payload payload = { .tag = PayloadTag_dataEnd, .dataEnd = NULL };
            MESSAGE_SERIALIZE(Payload, payload, newBytes);
            ENetPacket* packet =
              BytesToPacket(newBytes.buffer, newBytes.used, true);
            for (size_t i = 0; i < peerCount; ++i) {
                PEER_SEND(&peers[i], channel, packet);
            }
        }
    } else {
        Payload payload = { .tag = PayloadTag_fullData, .fullData = *bytes };
        MESSAGE_SERIALIZE(Payload, payload, newBytes);
        ENetPacket* packet =
          BytesToPacket(newBytes.buffer, newBytes.used, reliable);
        for (size_t i = 0; i < peerCount; ++i) {
            PEER_SEND(&peers[i], channel, packet);
        }
    }
    uint8_tListFree(&newBytes);
}

int
runServer(const int argc,
          const char** argv,
          pConfiguration configuration,
          ServerFunctions funcs)
{
    configuration->tag = ConfigurationTag_server;
    configuration->server = defaultServerConfiguration();
    configuration->server.runCommand = (NullValue)argv[0];
    if (!funcs.parseConfiguration(argc, argv, configuration)) {
        return EXIT_FAILURE;
    }
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

    const ServerConfiguration* config = &configuration->server;
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, configuration->server.hostname.buffer);
        for (enet_uint16 port = config->minPort; port < config->maxPort;
             ++port) {
            address.port = port;
            server = enet_host_create(&address, config->maxClients, 2, 0, 0);
            if (server != NULL) {
                goto continueServer;
            }
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
                    client->payload.allocator = currentAllocator;
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
                      &rs)) {
                        case AuthenticateResult_Success:
                            break;
                        case AuthenticateResult_NotNeeded:
                            if (!funcs.handleMessage(
                                  message, &bytes, event.peer, ctx, config)) {
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
      redisCommand(ctx, "LEM 0 %s", TEM_STREAM_SERVER_KEY, str.buffer);

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