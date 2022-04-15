#pragma once

#include "main.h"

#include <dlfcn.h>
#include <hiredis.h>

#define TEM_STREAM_SERVER_KEY "TemStream Servers"
#define TEM_STREAM_SERVER_DIRTY_KEY "TemStream Servers Dirty"

typedef int (*AuthenticateFunc)(const char *, char *);

extern void sendPacketToReaders(ENetHost *, ENetPacket *);

class ServerData
{
    Bytes bytes;
    RandomState rs;
    ENetHost *host;
    const ServerConfiguration *config;
    redisContext *ctx;

    ServerData();
    ~ServerData();
} ServerData, *pServerData;

template <typename T> class IServer
{
  protected:
    ServerData serverData;

  public:
    IServer() : serverData()
    {
    }
    virtual ~IServer()
    {
    }

    virtual void serializeMessage(const void *, pBytes) = 0;
    virtual T deserializeMessage(const Bytes *) = 0;
    virtual const GeneralMessage *getGeneralMessage(const T *) = 0;
    virtual ServerMessage getServerMessage(const T *) = 0;
    virtual void sendGeneral(const GeneralMessage *, pBytes, ENetPeer *) = 0;
    virtual bool onConnect(ENetPeer *, pServerData) = 0;
    virtual bool handleMessage(const void *, ENetPeer *, pServerData) = 0;
    virtual void onDownTime(pServerData) = 0;
    virtual void freeMessage(T *) = 0;

    virtual int run(pConfiguration) = 0;
};

extern bool getServerFileBytes(const ServerConfiguration *config, pBytes bytes);

extern void writeServerFileBytes(const ServerConfiguration *config, const Bytes *bytes, const bool overwrite);

extern const char *getServerFileName(const ServerConfiguration *config, char buffer[KB(1)]);

extern const char *getServerReplayFileNameFromReplayConfig(const ServerConfiguration *, char buffer[KB(1)]);

extern const char *getServerReplayFileNameFromConfig(const ServerConfiguration *, char buffer[KB(1)]);

extern void storeClientMessage(ServerData &, const ServerMessage *);

#define CAST_MESSAGE(name, ptr) name *message = (name *)ptr

extern bool handleGeneralMessage(const GeneralMessage *, ENetPeer *, ServerData &, pGeneralMessage);

extern bool authenticateClient(pClient, const AuthenticateFunc, const char *);

extern AuthenticateResult handleClientAuthentication(pClient, const AuthenticateFunc, const GeneralMessage *);

// Redis

extern ServerConfigurationList getStreams(redisContext *);

extern bool streamExists(const ServerConfiguration *);

extern bool writeConfigurationToRedis(redisContext *, const ServerConfiguration *);

extern bool removeConfigurationFromRedis(redisContext *, const ServerConfiguration *);

extern void cleanupConfigurationsInRedis(redisContext *);

extern bool serverIsDirty(redisContext *);

extern void setServerIsDirty(redisContext *, const bool);