#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <enet/enet.h>
#include <hiredis.h>
#endif

#include <opus/opus.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <DefaultExternalFunctions.h>
#include <IO.h>

#include <generated/general.h>

#define MAX_PACKET_SIZE KB(64)

const extern Guid ZeroGuid;

typedef struct Client
{
    TemLangString name;
    Bytes payload;
    Guid id;
    int64_t joinTime;
} Client, *pClient;

extern void ClientFree(pClient);

extern bool
ClientGuidEquals(const pClient*, const Guid*);

extern TemLangString RandomClientName(pRandomState);

extern TemLangString
RandomString(pRandomState, size_t min, size_t max);

MAKE_COPY_AND_FREE(pClient);
MAKE_DEFAULT_LIST(pClient);

extern bool
StreamNameEquals(const Stream*, const TemLangString*);

extern bool
StreamTypeEquals(const Stream*, const StreamType*);

extern bool
StreamDisplayGuidEquals(const StreamDisplay*, const Guid*);

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 1

#define INVALID_SOCKET (-1)

typedef SDL_Thread* pSDL_Thread;
MAKE_COPY_AND_FREE(pSDL_Thread);
MAKE_DEFAULT_LIST(pSDL_Thread);

extern int
printVersion();

extern bool appDone;

extern void
signalHandler(int);

// Defaults

extern ClientConfiguration
defaultClientConfiguration();

extern ServerConfiguration
defaultServerConfiguration();

extern LobbyConfiguration
defaultLobbyConfiguration();

extern Configuration
defaultConfiguration();

// ENet

extern ENetPeer*
FindPeerFromData(ENetPeer*, size_t, const void*);

extern ENetPacket*
BytesToPacket(const void* data, const size_t length, const bool);

extern void
sendBytes(ENetPeer*,
          size_t peerCount,
          size_t mtu,
          const enet_uint32,
          const Bytes*,
          const bool);

extern void
cleanupServer(ENetHost*);

extern PayloadParseResult
parsePayload(const Payload*, pClient);

extern void
closeHostAndPeer(ENetHost*, ENetPeer*);

typedef ENetPacket* pENetPacket;

MAKE_COPY_AND_FREE(pENetPacket);
MAKE_DEFAULT_LIST(pENetPacket);

#define CLIENT_CHANNEL 0
#define SERVER_CHANNEL 1

#define PEER_SEND(peer, channelID, packet)                                     \
    if (enet_peer_send(peer, channelID, packet) == -1) {                       \
        fprintf(stderr,                                                        \
                "Failed to send packet to channel ID: %u\n",                   \
                (uint8_t)channelID);                                           \
        enet_packet_destroy(packet);                                           \
    }

// Printing

extern int
printIpAddress(const IpAddress*);

extern int
printConfiguration(const Configuration*);

extern int
printClientConfiguration(const ClientConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

extern int
printLobbyConfiguration(const LobbyConfiguration*);

extern int
printTextConfiguration(const TextConfiguration*);

extern int
printChatConfiguration(const ChatConfiguration*);

extern int
printAuthenticate(const ServerAuthentication*);

extern int
printStream(const Stream*);

extern int
printSendingPacket(const ENetPacket*);

extern int
printReceivedPacket(const ENetPacket*);

extern int
printBytes(const uint8_t*, const size_t);

extern int
printAudioSpec(const SDL_AudioSpec*);

extern int
printServerAuthentication(const ServerAuthentication*);

// Parsing

extern bool
parseClientConfiguration(const int, const char**, pConfiguration);

extern bool
parseServerConfiguration(const char*, const char*, pServerConfiguration);

extern bool
parseConfiguration(const int, const char**, pConfiguration);

extern bool
parseCommonConfiguration(const char*, const char*, pConfiguration);

extern bool
parseIpAddress(const char*, pIpAddress);

extern bool
parseCredentials(const char*, pCredentials);

// Run

extern int
runApp(const int, const char**);

typedef struct ServerFunctions
{
    bool (*parseConfiguration)(int, const char**, pConfiguration);

    void (*serializeMessage)(const void*, pBytes);
    void* (*deserializeMessage)(const Bytes*);
    const GeneralMessage* (*getGeneralMessage)(const void*);
    bool (*handleMessage)(const void*, pBytes, ENetPeer*, redisContext*);
    void (*freeMessage)(void*);

    bool (*init)(redisContext*);
    void (*close)(redisContext*);
    const char* name;
} ServerFunctions, *pServerFunctions;

extern int
runServer(const int, const char**, pConfiguration, ServerFunctions);

extern int
runClient(const int, const char** argv, pConfiguration);

#define CAST_MESSAGE(name, ptr) name* message = (name*)ptr

// Lobby

extern bool
initLobby(redisContext*);

extern void
closeLobby(redisContext*);

extern bool
parseLobbyConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration);

extern void
serializeLobbyMessage(const void*, pBytes bytes);

extern void*
deserializeLobbyMessage(const Bytes* bytes);

extern bool
handleLobbyMessage(const void*, pBytes, ENetPeer*, redisContext*);

extern const GeneralMessage*
getGeneralMessageFromLobby(const void*);

extern void
freeLobbyMessage(void*);

// Parse failures

extern void
parseFailure(const char* type, const char* arg1, const char* arg2);

// Searches

extern bool
GetStreamFromName(const StreamList*,
                  const TemLangString*,
                  const Stream**,
                  size_t*);

extern bool
GetStreamFromType(const StreamList*, StreamType, const Stream**, size_t*);

extern bool
GetStreamDisplayFromGuid(const StreamDisplayList*,
                         const Guid*,
                         const StreamDisplay**,
                         size_t*);

extern bool
GetClientFromGuid(const pClientList*, const Guid*, const pClient**, size_t*);

// Misc

#define IN_MUTEX(mutex, endLabel, f)                                           \
    SDL_LockMutex(mutex);                                                      \
    f;                                                                         \
    goto endLabel;                                                             \
    endLabel:                                                                  \
    SDL_UnlockMutex(mutex);

extern bool
authenticateClient(pClient,
                   const ServerAuthentication*,
                   const ClientAuthentication*,
                   pRandomState);

extern AuthenticateResult
handleClientAuthentication(pClient,
                           const ServerAuthentication* sAuth,
                           const GeneralMessage*,
                           pRandomState);

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

extern bool
filenameToExtension(const char*, pFileExtension);

extern StreamType FileExtenstionToStreamType(FileExtensionTag);

// Audio

#define AUDIO_FRAME_SIZE KB(128)
#define MAX_AUDIO_PACKET_LOSS 10
#define ENABLE_FEC 0

#define HIGH_QUALITY_AUDIO 1
#if HIGH_QUALITY_AUDIO
#define PCM_SIZE sizeof(float)
#else
#define PCM_SIZE sizeof(opus_int16)
#endif

typedef struct AudioState
{
    SDL_AudioSpec spec;
    union
    {
        OpusEncoder* encoder;
        OpusDecoder* decoder;
    };
    SDL_AudioDeviceID deviceId;
    uint32_t packetLoss;
    SDL_bool running;
    SDL_bool isRecording;
} AudioState, *pAudioState;

extern void AudioStateFree(pAudioState);

// Font
typedef struct Character
{
    vec2 size;
    vec2 bearing;
    SDL_Rect rect;
    FT_Pos advance;
} Character, *pCharacter;

MAKE_COPY_AND_FREE(Character);
MAKE_DEFAULT_LIST(Character);

typedef struct Font
{
    SDL_Texture* texture;
    CharacterList characters;
} Font, *pFont;

extern void FontFree(pFont);

extern bool
loadFont(const char* filename,
         const FT_UInt fontSize,
         SDL_Renderer* renderer,
         pFont font);

extern SDL_FRect
renderFont(SDL_Renderer* renderer,
           pFont font,
           const char* text,
           const float x,
           const float y,
           const float scale,
           const uint8_t foreground[4],
           const uint8_t background[4]);

// SDL

SDL_FORCE_INLINE SDL_bool
SDL_PointInFRect(const SDL_FPoint* p, const SDL_FRect* r)
{
    return ((p->x >= r->x) && (p->x < (r->x + r->w)) && (p->y >= r->y) &&
            (p->y < (r->y + r->h)))
             ? SDL_TRUE
             : SDL_FALSE;
}

SDL_FORCE_INLINE SDL_Rect
FRect_to_Rect(const SDL_FRect* r)
{
    return (SDL_Rect){ .x = r->x, .y = r->y, .w = r->w, .h = r->h };
}

SDL_FORCE_INLINE SDL_FRect
expandRect(const SDL_FRect* rect, const float sw, const float sh)
{
    const float centerX = (rect->x + (rect->x + rect->w)) * 0.5f;
    const float centerY = (rect->y + (rect->y + rect->h)) * 0.5f;
    const float newWidth = rect->w * sw;
    const float newHeight = rect->h * sh;
    return (SDL_FRect){ .x = centerX - newWidth * 0.5f,
                        .y = centerY - newHeight * 0.5f,
                        .w = newWidth,
                        .h = newHeight };
}

extern SDL_AudioSpec
makeAudioSpec(SDL_AudioCallback, void* userdata);

// Allocator

extern Allocator makeTSAllocator(size_t);

extern void
freeTSAllocator();

// Redis

extern StreamList
getStreams(redisContext*);

extern bool
addStream(redisContext*, const Stream*);