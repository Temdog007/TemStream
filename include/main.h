#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <enet/enet.h>
#include <errno.h>
#include <hiredis.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#endif

#include <opus/opus.h>

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

#define TEM_STREAM_SERVER_KEY "TemStream Servers"

const extern Guid ZeroGuid;

extern SDL_atomic_t runningThreads;

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
StreamDisplayNameEquals(const StreamDisplay*, const TemLangString*);

extern bool
StreamDisplayGuidEquals(const StreamDisplay*, const Guid*);

extern bool
GetStreamDisplayFromName(const StreamDisplayList*,
                         const TemLangString*,
                         const StreamDisplay**,
                         size_t*);

extern bool
GetStreamDisplayFromGuid(const StreamDisplayList*,
                         const Guid*,
                         const StreamDisplay**,
                         size_t*);

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

extern TextConfiguration
defaultTextConfiguration();

extern ImageConfiguration
defaultImageConfiguration();

extern AudioConfiguration
defaultAudioConfiguration();

extern ChatConfiguration
defaultChatConfiguration();

extern Configuration
defaultConfiguration();

// ENet

extern ENetPeer*
FindPeerFromData(ENetPeer*, size_t, const void*);

extern ENetPacket*
BytesToPacket(const void* data, const size_t length, const bool);

extern void
sendBytes(ENetPeer*,
          const size_t peerCount,
          const enet_uint32,
          const Bytes*,
          const bool);

extern void
cleanupServer(ENetHost*);

extern void
closeHostAndPeer(ENetHost*, ENetPeer*);

extern void
sendPacketToReaders(ENetHost*, ENetPacket*, const Access*);

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
printConfiguration(const Configuration*);

extern int
printClientConfiguration(const ClientConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

extern int
printServerConfigurationForClient(const ServerConfiguration*);

extern int
printLobbyConfiguration(const LobbyConfiguration*);

extern int
printTextConfiguration(const TextConfiguration*);

extern int
printChatConfiguration(const ChatConfiguration*);

extern int
printAuthenticate(const ServerAuthentication*);

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
parseCredentials(const char*, pCredentials);

// Run

extern int
runApp(const int, const char**, pConfiguration);

typedef struct ServerFunctions
{
    void (*serializeMessage)(const void*, pBytes);
    void* (*deserializeMessage)(const Bytes*);
    const GeneralMessage* (*getGeneralMessage)(const void*);
    void (*sendGeneral)(const GeneralMessage*, pBytes, ENetPeer*);
    bool (*onConnect)(pClient, pBytes, ENetPeer*, const ServerConfiguration*);
    bool (*handleMessage)(const void*,
                          pBytes,
                          ENetPeer*,
                          redisContext*,
                          const ServerConfiguration*);
    void (*freeMessage)(void*);
} ServerFunctions, *pServerFunctions;

extern int runServer(pConfiguration, ServerFunctions);

extern int
runClient(const Configuration*);

extern bool
getServerFileBytes(const ServerConfiguration* config, pBytes bytes);

extern void
appendServerFileBytes(const ServerConfiguration* config,
                      const Bytes* bytes,
                      const bool overwrite);

#define CAST_MESSAGE(name, ptr) name* message = (name*)ptr

extern bool
handleGeneralMessage(const GeneralMessage*, ENetPeer*, pGeneralMessage);

// Lobby

extern bool
parseLobbyConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration);

extern void
serializeLobbyMessage(const void*, pBytes bytes);

extern void*
deserializeLobbyMessage(const Bytes* bytes);

extern void
lobbySendGeneralMessage(const GeneralMessage*, pBytes, ENetPeer*);

extern bool
lobbyOnConnect(pClient client,
               pBytes bytes,
               ENetPeer* peer,
               const ServerConfiguration* config);

extern bool
handleLobbyMessage(const void*,
                   pBytes,
                   ENetPeer*,
                   redisContext*,
                   const ServerConfiguration*);

extern const GeneralMessage*
getGeneralMessageFromLobby(const void*);

extern void
freeLobbyMessage(void*);

// Text

extern bool
parseTextConfiguration(const int, const char**, pConfiguration);

extern void
serializeTextMessage(const void*, pBytes bytes);

extern void*
deserializeTextMessage(const Bytes* bytes);

extern void
textSendGeneralMessage(const GeneralMessage*, pBytes, ENetPeer*);

extern bool
textOnConnect(pClient client,
              pBytes bytes,
              ENetPeer* peer,
              const ServerConfiguration* config);

extern bool
handleTextMessage(const void*,
                  pBytes,
                  ENetPeer*,
                  redisContext*,
                  const ServerConfiguration*);

extern const GeneralMessage*
getGeneralMessageFromText(const void*);

extern void
freeTextMessage(void*);

// Parse failures

extern void
parseFailure(const char* type, const char* arg1, const char* arg2);

// Searches

extern bool
GetStreamDisplayFromGuid(const StreamDisplayList*,
                         const Guid*,
                         const StreamDisplay**,
                         size_t*);

extern bool
GetClientFromGuid(const pClientList*, const Guid*, const pClient**, size_t*);

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

extern bool CanSendFileToStream(FileExtensionTag, ServerConfigurationDataTag);

extern void
cleanupServer(ENetHost*);

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

extern ServerConfigurationList
getStreams(redisContext*);

extern bool
streamExists(const ServerConfiguration*);

extern bool
writeConfigurationToRedis(redisContext*, const ServerConfiguration*);

extern bool
removeConfigurationFromRedis(redisContext*, const ServerConfiguration*);

// Base 64

TemLangString
b64_encode(const Bytes* bytes);

bool
b64_decode(const char* in, pBytes bytes);

// Access

bool
clientHasAccess(const Client* client, const Access* access);

bool
clientHasReadAccess(const Client* client, const ServerConfiguration* config);

bool
clientHasWriteAccess(const Client* client, const ServerConfiguration* config);