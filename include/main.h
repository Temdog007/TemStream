#pragma once

#include <enet/enet.h>
#include <errno.h>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include <DefaultExternalFunctions.h>
#include <IO.h>

#include <generated/general.h>

#include "circular_queue.h"

extern bool
CQueueCopy(pCQueue, const CQueue*, const Allocator*);

#define MAX_PACKET_SIZE KB(64)

#define INIT_ALLOCATOR(S)                                                      \
    (Bytes)                                                                    \
    {                                                                          \
        .allocator = currentAllocator, .used = 0U,                             \
        .buffer = currentAllocator->allocate(S), .size = S                     \
    }

const extern Guid ZeroGuid;

extern SDL_atomic_t runningThreads;

extern TemLangString RandomClientName(pRandomState);

extern char
RandomChar(pRandomState rs);

extern TemLangString
RandomString(pRandomState, size_t min, size_t max, const bool);

extern bool
clientHasRole(const Client*, ClientRole);

extern TemLangString
getAllRoles(const Client*);

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 1

#define INVALID_SOCKET (-1)

extern int
printVersion();

extern bool appDone;

extern void
signalHandler(int);

extern Configuration
defaultConfiguration();

// ENet

#define MAX_PACKETS 5

#define ENET_USE_CUSTOM_ALLOCATOR true

extern ENetPeer*
FindPeerFromData(ENetPeer*, size_t, const void*);

typedef enum SendFlags
{
    SendFlags_Normal = ENET_PACKET_FLAG_RELIABLE,
    SendFlags_Video = ENET_PACKET_FLAG_RELIABLE,
    // ENET_PACKET_FLAG_UNSEQUENCED | ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT,
    SendFlags_Audio = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT
} SendFlags,
  *pSendFlags;

extern ENetPacket*
BytesToPacket(const void* data, const size_t length, const SendFlags);

extern void
sendBytes(ENetPeer*,
          const size_t peerCount,
          const enet_uint32,
          const Bytes*,
          const SendFlags);

extern void
cleanupServer(ENetHost*);

extern void
closeHostAndPeer(ENetHost*, ENetPeer*);

#define CLIENT_CHANNEL 0
#define SERVER_CHANNEL 1

#define ENABLE_PRINT_MEMORY 0
#define PRINT_CHUNKS 0

#if ENABLE_PRINT_MEMORY
#define PRINT_MEMORY                                                           \
    printf("%s:%d) %zu\n", __FILE__, __LINE__, currentAllocator->used());
#else
#define PRINT_MEMORY
#endif

#define PEER_SEND(peer, channelID, packet)                                     \
    if (enet_peer_send(peer, channelID, packet) == -1) {                       \
        fprintf(stderr,                                                        \
                "Failed to send packet to channel ID: %u\n",                   \
                (uint8_t)channelID);                                           \
        enet_packet_destroy(packet);                                           \
    }

// Printing

extern int
printConfiguration(const Configuration*);

extern int
printClientConfiguration(const ClientConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

extern int
printServerConfigurationForClient(const ServerConfiguration*);

extern int
printSendingPacket(const ENetPacket*);

extern int
printReceivedPacket(const ENetPacket*);

extern int
printBytes(const uint8_t*, const size_t);

extern int
printAudioSpec(const SDL_AudioSpec*);

// Parsing

extern bool
parseClientConfiguration(const int, const char**, pConfiguration);

extern bool
parseServerConfiguration(const char*, const char*, pServerConfiguration);

extern bool
parseConfiguration(const int, const char**, pConfiguration);

extern bool
parseCommonConfiguration(const char*, const char*, pConfiguration);

// Run

extern int
runApp(const int, const char**, pConfiguration);

// Parse failures

extern void
parseFailure(const char* type, const char* arg1, const char* arg2);

// Searches

extern bool
GetStreamFromName(const ServerConfigurationList*,
                  const TemLangString*,
                  const ServerConfiguration**,
                  size_t*);

extern bool
GetStreamFromType(const ServerConfigurationList*,
                  const ServerConfigurationDataTag,
                  const ServerConfiguration**,
                  size_t*);

extern bool
ServerConfigurationNameEquals(const ServerConfiguration*, const TemLangString*);

extern bool
ServerConfigurationTagEquals(const ServerConfiguration*,
                             const ServerConfigurationDataTag*);

// Misc

#define IN_MUTEX(mutex, endLabel, f)                                           \
    SDL_LockMutex(mutex);                                                      \
    f;                                                                         \
    goto endLabel;                                                             \
    endLabel:                                                                  \
    SDL_UnlockMutex(mutex);

#define USE_DISPLAY(mutex, endLabel, displayMissing, f)                        \
    IN_MUTEX(mutex, endLabel, {                                                \
        pStreamDisplay display = NULL;                                         \
        size_t streamDisplayIndex = 0;                                         \
        const ServerConfiguration* config = NULL;                              \
        if (!GetStreamDisplayFromGuid(                                         \
              &clientData.displays, id, NULL, &streamDisplayIndex)) {          \
            displayMissing = true;                                             \
            goto endLabel;                                                     \
        }                                                                      \
        display = &clientData.displays.buffer[streamDisplayIndex];             \
        config = &display->config;                                             \
        displayMissing = false;                                                \
        f                                                                      \
    });

extern double
diff_timespec(const struct timespec*, const struct timespec*);

#define TIME_VIDEO_STREAMING false

#if TIME_VIDEO_STREAMING
#define TIME(str, f)                                                           \
    {                                                                          \
        struct timespec start = { 0 };                                         \
        struct timespec end = { 0 };                                           \
        timespec_get(&start, TIME_UTC);                                        \
        f;                                                                     \
        timespec_get(&end, TIME_UTC);                                          \
        const double diff = diff_timespec(&end, &start);                       \
        printf("'%s' took %f seconds (%f milliseconds) \n",                    \
               str,                                                            \
               diff,                                                           \
               diff * 1000.0);                                                 \
    }
#else
#define TIME(str, f) f
#endif

#define MESSAGE_SERIALIZE(name, message, bytes)                                \
    bytes.used = 0;                                                            \
    name##Serialize(&message, &bytes, true)

#define MESSAGE_DESERIALIZE(name, message, bytes)                              \
    name##Free(&message);                                                      \
    name##Deserialize(&message, &bytes, 0, true)

#define POLL_FOREVER (-1)
#define CLIENT_POLL_WAIT 100
#define SERVER_POLL_WAIT 1000
#define LONG_POLL_WAIT 5000

#define STREAM_TIMEOUT (1000u * 10u)

// Allocator

extern Allocator makeTSAllocator(size_t);

extern void
freeTSAllocator();

// Base 64

extern TemLangString
b64_encode(const Bytes* bytes);

extern bool
b64_decode(const char* in, pBytes bytes);

extern bool
lowMemory();

#if TEMSTREAM_SERVER
#include "server.h"
#else
#include "client.h"
#endif

typedef struct ThreadSafeAllocator
{
    SDL_mutex* mutex;
    const Allocator* allocator;
} TSAllocator, *pTSAllocator;

extern TSAllocator tsAllocator;

extern void*
tsAllocate(const size_t size);

extern void
tsFree(void* data);