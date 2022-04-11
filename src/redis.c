#include <include/main.h>

void
setServerIsDirty(redisContext* ctx, const bool state)
{
    redisReply* reply =
      redisCommand(ctx, "SET %s %d", TEM_STREAM_SERVER_DIRTY_KEY, state);
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Redis error: %s\n", reply->str);
        }
        freeReplyObject(reply);
    }
}

bool
serverIsDirty(redisContext* ctx)
{
    redisReply* reply =
      redisCommand(ctx, "GET %s", TEM_STREAM_SERVER_DIRTY_KEY);
    if (reply == NULL) {
        return false;
    }

    bool result = false;
    switch (reply->type) {
        case REDIS_REPLY_BOOL:
        case REDIS_REPLY_INTEGER:
            result = reply->integer == 1;
            break;
        case REDIS_REPLY_STRING:
            result = atoi(reply->str);
            break;
        case REDIS_REPLY_ERROR:
            fprintf(stderr, "Redis error: %s\n", reply->str);
            break;
        default:
            break;
    }
    freeReplyObject(reply);
    return result;
}

void
cleanupConfigurationsInRedis(redisContext* ctx)
{
    PRINT_MEMORY;
    ServerConfigurationList servers = getStreams(ctx);
    PRINT_MEMORY;
    printf("Verifying %u server(s)\n", servers.used);
    for (uint32_t i = 0; i < servers.used; ++i) {
        const ServerConfiguration* config = &servers.buffer[i];

        ENetAddress address = { 0 };
        enet_address_set_host(&address, config->hostname.buffer);
        address.port = config->port;
        ENetHost* host = enet_host_create(NULL, 2, 1, 0, 0);
        ENetPeer* peer = NULL;
        bool verified = false;
        if (host == NULL) {
            goto endLoop;
        }

        peer = enet_host_connect(host, &address, 2, 0);
        if (peer == NULL) {
            goto endLoop;
        }

        ENetEvent e = { 0 };
        while (enet_host_service(host, &e, 100U) > 0) {
            switch (e.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    verified = true;
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    enet_packet_destroy(e.packet);
                    break;
                default:
                    break;
            }
        }

    endLoop:
        closeHostAndPeer(host, peer);
        if (verified) {
            printf("Verified server: '%s'\n", config->name.buffer);
        } else {
            printf("Removing '%s' server from list\n", config->name.buffer);
            removeConfigurationFromRedis(ctx, config);
        }
    }
    ServerConfigurationListFree(&servers);
    PRINT_MEMORY;
}

bool
writeConfigurationToRedis(redisContext* ctx, const ServerConfiguration* c)
{
    Bytes bytes = { .allocator = currentAllocator };
    MESSAGE_SERIALIZE(ServerConfiguration, (*c), bytes);

    TemLangString str = b64_encode(&bytes);

    redisReply* reply =
      redisCommand(ctx, "LPUSH %s %s", TEM_STREAM_SERVER_KEY, str.buffer);

    const bool result = reply->type == REDIS_REPLY_INTEGER;
    if (!result && reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
    }

    freeReplyObject(reply);
    TemLangStringFree(&str);
    uint8_tListFree(&bytes);

    setServerIsDirty(ctx, true);
    return result;
}

bool
removeConfigurationFromRedis(redisContext* ctx, const ServerConfiguration* c)
{
    if (ctx == NULL) {
        return true;
    }
    Bytes bytes = { .allocator = currentAllocator };
    MESSAGE_SERIALIZE(ServerConfiguration, (*c), bytes);

    TemLangString str = b64_encode(&bytes);

    bool result = false;
    redisReply* reply =
      redisCommand(ctx, "LREM %s 0 %s", TEM_STREAM_SERVER_KEY, str.buffer);
    if (reply != NULL) {
        result = reply->type == REDIS_REPLY_INTEGER;
        if (!result && reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Redis error: %s\n", reply->str);
        }
        freeReplyObject(reply);
    }
    TemLangStringFree(&str);
    uint8_tListFree(&bytes);

    setServerIsDirty(ctx, true);
    return result;
}

ServerConfigurationList
getStreams(redisContext* ctx)
{
    ServerConfigurationList list = { .allocator = currentAllocator };

    Bytes bytes = { .allocator = currentAllocator };
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

        if (!b64_decode(r->str, &bytes)) {
            continue;
        }
        ServerConfiguration s = { 0 };
        MESSAGE_DESERIALIZE(ServerConfiguration, s, bytes);
        ServerConfigurationListAppend(&list, &s);
        ServerConfigurationFree(&s);
    }
end:
    uint8_tListFree(&bytes);
    freeReplyObject(reply);
    return list;
}
