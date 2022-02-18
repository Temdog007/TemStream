#pragma once

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <DefaultExternalFunctions.h>

#include <generated/data.h>

#define MAX_PACKET_SIZE KB(10)

typedef struct Client
{
    struct sockaddr_storage addr;
    Guid guid;
    TemLangString name;
    ClientAuthentication authentication;
    GuidList connectedStreams;
    int32_t sockfd;
} Client, *pClient;

extern void ClientFree(pClient);

extern bool
ClientGuidEquals(const pClient*, const Guid*);

MAKE_COPY_AND_FREE(pClient);
MAKE_DEFAULT_LIST(pClient);

extern bool
StreamTypeMatchStreamMessage(const StreamType, const StreamMessageDataTag);

extern bool
StreamGuidEquals(const Stream*, const Guid*);

extern bool
MessageUsesUdp(const StreamMessage*);

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 1

#define INVALID_SOCKET (-1)

extern int
printVersion();

extern bool appDone;

extern void
signalHandler(int);

// Networking

extern const char*
getAddrString(const struct sockaddr_storage*, char[64], int*);

extern const char*
getAddrInfoString(const struct addrinfo*, char[64], int*);

extern void
closeSocket(int);

extern int
openSocket(void*, SocketOptions);

extern int
openIpSocket(const char* ip, const char* port, SocketOptions);

extern int
openUnixSocket(const char* filename, SocketOptions);

extern int
openSocketFromAddress(const Address*, SocketOptions);

extern int
sendTcp(const int, const void*, const size_t, const struct sockaddr_storage*);

extern int
sendUdp(const int, const void*, const size_t, const struct sockaddr_storage*);

// Defaults

extern ClientConfiguration
defaultClientConfiguration();

extern ServerConfiguration
defaultServerConfiguration();

extern Configuration
defaultConfiguration();

extern AllConfiguration
defaultAllConfiguration();

// Printing

extern int
printAddress(const Address*);

extern int
printAllConfiguration(const AllConfiguration*);

extern int
printClientConfiguration(const ClientConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

// Parsing

extern bool
parseProducerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseClientConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseServerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseAllConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseCommonConfiguration(const char*, const char*, pAllConfiguration);

extern bool
parseAddress(const char*, pAddress);

extern bool
parseCredentials(const char*, pCredentials);

// Run

extern int
runApp(const int, const char**);

extern int
runClient(const AllConfiguration*);

extern int
runServer(const AllConfiguration*);

extern int
runProducer(const AllConfiguration*);

// Parse failures

extern void
parseFailure(const char* type, const char* arg1, const char* arg2);