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

#include <DefaultExternalFunctions.h>

#include <generated/data.h>

extern bool
StreamInformationNameEqual(const StreamInformation*, const TemLangString*);

extern const char*
getAddrString(struct sockaddr_storage*, char[64]);

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

extern void
closeSocket(int);

extern int
openSocket(void*, SocketOptions);

extern int
openIpSocket(const char* ip, const char* port, SocketOptions);

extern int
openUnixSocket(const char* filename, SocketOptions);

typedef int (*SendFunc)(const int,
                        const void*,
                        const size_t,
                        const struct sockaddr_storage*);

extern int
sendTcp(const int, const void*, const size_t, const struct sockaddr_storage*);

extern int
sendUdp(const int, const void*, const size_t, const struct sockaddr_storage*);

// Defaults

extern ConsumerConfiguration
defaultConsumerConfiguration();

extern ProducerConfiguration
defaultProducerConfiguration();

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
printConsumerConfiguration(const ConsumerConfiguration*);

extern int
printProducerConfiguration(const ProducerConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

// Parsing

extern bool
parseProducerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseConsumerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseServerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseAllConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseCommonConfiguration(const char*, const char*, pAllConfiguration);

extern bool
parseAddress(const char*, pAddress);

// Run

extern int
runApp(const int, const char**);

extern int
runConsumer(const AllConfiguration*);

extern int
runServer(const AllConfiguration*);

extern int
runProducer(const AllConfiguration*);

// Parse failures

extern void
parseFailure(const char* type, const char* arg1, const char* arg2);