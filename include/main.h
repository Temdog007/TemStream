#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <enet/enet.h>
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

#include <generated/data.h>

#define MAX_PACKET_SIZE KB(64)

const extern Guid ZeroGuid;

typedef struct Client
{
    Guid id;
    TemLangString name;
    GuidList connectedStreams;
    const ServerAuthentication* serverAuthentication;
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
StreamTypeMatchStreamMessage(const StreamType, const StreamMessageDataTag);

extern bool
StreamGuidEquals(const Stream*, const Guid*);

extern bool
StreamNameEquals(const Stream*, const TemLangString*);

extern bool
StreamTypeEquals(const Stream*, const StreamType*);

extern bool
StreamMessageGuidEquals(const StreamMessage*, const Guid*);

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

extern Configuration
defaultConfiguration();

extern AllConfiguration
defaultAllConfiguration();

// ENet

extern ENetPeer*
FindPeerFromData(ENetPeer*, size_t, const void*);

extern ENetPacket*
BytesToPacket(const Bytes*, bool);

extern bool
streamMessageIsReliable(const StreamMessage*);

typedef ENetPacket* pENetPacket;

MAKE_COPY_AND_FREE(pENetPacket);
MAKE_DEFAULT_LIST(pENetPacket);

#define SERVER_CHANNEL 0
#define CLIENT_CHANNEL 1

// Printing

extern int
printIpAddress(const IpAddress*);

extern int
printAllConfiguration(const AllConfiguration*);

extern int
printClientConfiguration(const ClientConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

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
parseIpAddress(const char*, pIpAddress);

extern bool
parseCredentials(const char*, pCredentials);

// Run

extern int
runApp(const int, const char**);

extern int
runClient(const AllConfiguration*);

extern int
runServer(const AllConfiguration*);

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
GetStreamFromGuid(const StreamList*, const Guid*, const Stream**, size_t*);

extern bool
GetStreamMessageFromGuid(const StreamMessageList*,
                         const Guid*,
                         const StreamMessage**,
                         size_t*);

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
authenticateClient(pClient client,
                   const ClientAuthentication* cAuth,
                   pRandomState rs);

#define MESSAGE_SERIALIZE(message, bytes)                                      \
    bytes.used = 0;                                                            \
    MessageSerialize(&message, &bytes, true)

#define MESSAGE_DESERIALIZE(message, bytes)                                    \
    MessageFree(&message);                                                     \
    MessageDeserialize(&message, &bytes, 0, true)

#define CLIENT_POLL_WAIT 100
#define SERVER_POLL_WAIT 1000
#define LONG_POLL_WAIT 5000

#define USER_INPUT_TIMEOUT 3000

extern bool
filenameToExtension(const char*, pFileExtension);

extern StreamType FileExtenstionToStreamType(FileExtensionTag);

extern bool streamTypeMatchesMessage(StreamType, StreamMessageDataTag);

// Audio

#define AUDIO_FRAME_SIZE KB(128)

#define HIGH_QUALITY_AUDIO 1
#if HIGH_QUALITY_AUDIO
#define PCM_SIZE sizeof(float)
#else
#define PCM_SIZE sizeof(opus_int16)
#endif

#define DELAY_AUDIO_QUEUE 0
#define TEST_MIC 0

typedef struct AudioState
{
    SDL_AudioSpec spec;
    Bytes audio;
    Guid id;
    union
    {
        OpusEncoder* encoder;
        OpusDecoder* decoder;
    };
    SDL_AudioDeviceID deviceId;
    SDL_bool running;
    SDL_bool isRecording;
} AudioState, *pAudioState;

extern void AudioStateFree(pAudioState);

typedef pAudioState AudioStatePtr;

MAKE_COPY_AND_FREE(AudioStatePtr);
MAKE_DEFAULT_LIST(AudioStatePtr);

extern bool
GetPlaybackAudioStateFromGuid(const AudioStatePtrList*,
                              const Guid*,
                              const AudioStatePtr**,
                              size_t*);

extern bool
AudioStateGuidEquals(const AudioState*, const Guid*);

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