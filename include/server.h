#pragma once

#include "main.h"

#include <dlfcn.h>
#include <hiredis.h>

#define TEM_STREAM_SERVER_KEY "TemStream Servers"
#define TEM_STREAM_SERVER_DIRTY_KEY "TemStream Servers Dirty"

typedef int (*AuthenticateFunc)(const char*, char*);

extern void
sendPacketToReaders(ENetHost*, ENetPacket*);

typedef struct ServerData
{
    Bytes bytes;
    RandomState rs;
    ENetHost* host;
    const ServerConfiguration* config;
    redisContext* ctx;
} ServerData, *pServerData;

extern void ServerDataFree(pServerData);

typedef struct ServerFunctions
{
    void (*serializeMessage)(const void*, pBytes);
    void* (*deserializeMessage)(const Bytes*);
    const GeneralMessage* (*getGeneralMessage)(const void*);
    ServerMessage (*getServerMessage)(const void*);
    void (*sendGeneral)(const GeneralMessage*, pBytes, ENetPeer*);
    bool (*onConnect)(ENetPeer*, pServerData);
    bool (*handleMessage)(const void*, ENetPeer*, pServerData);
    void (*onDownTime)(pServerData);
    void (*freeMessage)(void*);
} ServerFunctions, *pServerFunctions;

extern int runServer(pConfiguration, ServerFunctions);

extern bool
getServerFileBytes(const ServerConfiguration* config, pBytes bytes);

extern void
writeServerFileBytes(const ServerConfiguration* config,
                     const Bytes* bytes,
                     const bool overwrite);

extern const char*
getServerFileName(const ServerConfiguration* config, char buffer[KB(1)]);

extern const char*
getServerReplayFileNameFromReplayConfig(const ServerConfiguration*,
                                        char buffer[KB(1)]);

extern const char*
getServerReplayFileNameFromConfig(const ServerConfiguration*,
                                  char buffer[KB(1)]);

extern void
storeClientMessage(pServerData data, const ServerMessage*);

#define CAST_MESSAGE(name, ptr) name* message = (name*)ptr

extern bool
handleGeneralMessage(const GeneralMessage*,
                     ENetPeer*,
                     pServerData,
                     pGeneralMessage);

#define SERVER_FUNCTIONS(T)                                                    \
    extern bool parse##T##Configuration(                                       \
      const int, const char**, pConfiguration);                                \
                                                                               \
    extern void serialize##T##Message(const void*, pBytes bytes);              \
    extern void* deserialize##T##Message(const Bytes* bytes);                  \
    extern void sendGeneralMessageFor##T(                                      \
      const GeneralMessage*, pBytes, ENetPeer*);                               \
    extern bool onConnectFor##T(ENetPeer* peer, pServerData);                  \
    extern bool handle##T##Message(const void*, ENetPeer*, pServerData);       \
    extern const GeneralMessage* getGeneralMessageFrom##T(const void*);        \
    extern ServerMessage getServerMessageFrom##T(const void*);                 \
    extern void free##T##Message(void*);                                       \
    extern T##Configuration default##T##Configuration();                       \
    extern int print##T##Configuration(const T##Configuration*);               \
    extern int run##T##Server(Configuration*);                                 \
    extern void on##T##DownTime(pServerData);

SERVER_FUNCTIONS(Lobby);
SERVER_FUNCTIONS(Text);
SERVER_FUNCTIONS(Chat);
SERVER_FUNCTIONS(Image);
SERVER_FUNCTIONS(Audio);
SERVER_FUNCTIONS(Video);
SERVER_FUNCTIONS(Replay);

#define DEFINE_RUN_SERVER(T)                                                   \
    int run##T##Server(pConfiguration configuration)                           \
    {                                                                          \
        return runServer(                                                      \
          configuration,                                                       \
          (ServerFunctions){ .serializeMessage = serialize##T##Message,        \
                             .deserializeMessage = deserialize##T##Message,    \
                             .getGeneralMessage = getGeneralMessageFrom##T,    \
                             .getServerMessage = getServerMessageFrom##T,      \
                             .sendGeneral = sendGeneralMessageFor##T,          \
                             .onConnect = onConnectFor##T,                     \
                             .handleMessage = handle##T##Message,              \
                             .freeMessage = free##T##Message,                  \
                             .onDownTime = on##T##DownTime });                 \
    }                                                                          \
                                                                               \
    void serialize##T##Message(const void* ptr, pBytes bytes)                  \
    {                                                                          \
        CAST_MESSAGE(T##Message, ptr);                                         \
        MESSAGE_SERIALIZE(T##Message, (*message), (*bytes));                   \
    }                                                                          \
                                                                               \
    void* deserialize##T##Message(const Bytes* bytes)                          \
    {                                                                          \
        p##T##Message message =                                                \
          currentAllocator->allocate(sizeof(T##Message));                      \
        MESSAGE_DESERIALIZE(T##Message, (*message), (*bytes));                 \
        return message;                                                        \
    }                                                                          \
                                                                               \
    void free##T##Message(void* ptr)                                           \
    {                                                                          \
        CAST_MESSAGE(T##Message, ptr);                                         \
        T##MessageFree(message);                                               \
        currentAllocator->free(message);                                       \
    }                                                                          \
                                                                               \
    const GeneralMessage* getGeneralMessageFrom##T(const void* ptr)            \
    {                                                                          \
        CAST_MESSAGE(T##Message, ptr);                                         \
        return message->tag == T##MessageTag_general ? &message->general       \
                                                     : NULL;                   \
    }                                                                          \
                                                                               \
    ServerMessage getServerMessageFrom##T(const void* ptr)                     \
    {                                                                          \
        ServerMessage m = { .tag = ServerMessageTag_##T };                     \
        CAST_MESSAGE(T##Message, ptr);                                         \
        T##MessageCopy((T##Message*)&m.Chat, message, currentAllocator);       \
        return m;                                                              \
    }                                                                          \
                                                                               \
    void sendGeneralMessageFor##T(                                             \
      const GeneralMessage* m, pBytes bytes, ENetPeer* peer)                   \
    {                                                                          \
        T##Message lm = { 0 };                                                 \
        lm.tag = T##MessageTag_general;                                        \
        lm.general = *m;                                                       \
        MESSAGE_SERIALIZE(T##Message, lm, (*bytes));                           \
        sendBytes(peer, 1, SERVER_CHANNEL, bytes, SendFlags_Normal);           \
    }

extern bool
authenticateClient(pClient, const AuthenticateFunc, const char*);

extern AuthenticateResult
handleClientAuthentication(pClient,
                           const AuthenticateFunc,
                           const GeneralMessage*);

// Redis

extern ServerConfigurationList
getStreams(redisContext*);

extern bool
streamExists(const ServerConfiguration*);

extern bool
writeConfigurationToRedis(redisContext*, const ServerConfiguration*);

extern bool
removeConfigurationFromRedis(redisContext*, const ServerConfiguration*);

extern void
cleanupConfigurationsInRedis(redisContext*);

extern bool
serverIsDirty(redisContext*);

extern void
setServerIsDirty(redisContext*, const bool);