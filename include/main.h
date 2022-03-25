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
#include <time.h>
#include <xcb/xcb.h>
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

#include <vorbis/codec.h>

#define USE_OPENCL false
#define TIME_VIDEO_STREAMING false
#define USE_SDL_CONVERSION false

#if USE_OPENCL

#else
extern bool
rgbaToYuv(const uint8_t* rgba,
          int width,
          int height,
          uint32_t* argb,
          uint8_t* yuv);
#endif

#define USE_VP8 true

#if USE_VP8
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_encoder.h>

extern int
vpx_img_plane_width(const vpx_image_t*, int);

extern int
vpx_img_plane_height(const vpx_image_t*, int);

extern vpx_codec_iface_t*
codec_encoder_interface();

extern vpx_codec_iface_t*
codec_decoder_interface();
#else
#include "open264.h"
#endif

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>

#include "circular_queue.h"

#define MAX_PACKET_SIZE KB(64)

#define TEM_STREAM_SERVER_KEY "TemStream Servers"
#define TEM_STREAM_SERVER_DIRTY_KEY "TemStream Servers Dirty"

#define LOG_READER false
#define PRINT_RENDER_INFO false

#define INIT_ALLOCATOR(S)                                                      \
    (Bytes)                                                                    \
    {                                                                          \
        .allocator = currentAllocator, .used = 0U,                             \
        .buffer = currentAllocator->allocate(S), .size = S                     \
    }

const extern Guid ZeroGuid;

extern SDL_atomic_t runningThreads;

typedef struct AudioState AudioState, *pAudioState;

extern bool
ClientGuidEquals(const pClient*, const Guid*);

extern TemLangString RandomClientName(pRandomState);

extern char
RandomChar(pRandomState rs);

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

typedef bool (*AuthenticateFunc)(int, char*);

extern int
printVersion();

extern bool appDone;

extern void
signalHandler(int);

extern Configuration
defaultConfiguration();

// ENet

extern ENetPeer*
FindPeerFromData(ENetPeer*, size_t, const void*);

typedef enum SendFlags
{
    SendFlags_Normal = ENET_PACKET_FLAG_RELIABLE,
    SendFlags_Video =
      ENET_PACKET_FLAG_UNSEQUENCED | ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT,
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

extern void
sendPacketToReaders(ENetHost*, ENetPacket*, const Access*);

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
printAuthentication(const Authentication*);

extern int
printSendingPacket(const ENetPacket*);

extern int
printReceivedPacket(const ENetPacket*);

extern int
printBytes(const uint8_t*, const size_t);

extern int
printAudioSpec(const SDL_AudioSpec*);

int
printPort(const Port* port);

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

typedef struct ServerData
{
    Bytes bytes;
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
    void (*sendGeneral)(const GeneralMessage*, pBytes, ENetPeer*);
    bool (*onConnect)(ENetPeer*, pServerData);
    bool (*handleMessage)(const void*, ENetPeer*, pServerData);
    void (*onDownTime)(pServerData);
    void (*freeMessage)(void*);
} ServerFunctions, *pServerFunctions;

extern int runServer(pConfiguration, ServerFunctions);

extern int
runClient(const Configuration*);

extern bool
getServerFileBytes(const ServerConfiguration* config, pBytes bytes);

extern void
writeServerFileBytes(const ServerConfiguration* config,
                     const Bytes* bytes,
                     const bool overwrite);

extern const char*
getServerFileName(const ServerConfiguration* config, char buffer[512]);

#define CAST_MESSAGE(name, ptr) name* message = (name*)ptr

extern bool
handleGeneralMessage(const GeneralMessage*, pServerData, pGeneralMessage);

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

#define DEFINE_RUN_SERVER(T)                                                   \
    int run##T##Server(pConfiguration configuration)                           \
    {                                                                          \
        return runServer(                                                      \
          configuration,                                                       \
          (ServerFunctions){ .serializeMessage = serialize##T##Message,        \
                             .deserializeMessage = deserialize##T##Message,    \
                             .getGeneralMessage = getGeneralMessageFrom##T,    \
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
    void sendGeneralMessageFor##T(                                             \
      const GeneralMessage* m, pBytes bytes, ENetPeer* peer)                   \
    {                                                                          \
        T##Message lm = { 0 };                                                 \
        lm.tag = T##MessageTag_general;                                        \
        lm.general = *m;                                                       \
        MESSAGE_SERIALIZE(T##Message, lm, (*bytes));                           \
        sendBytes(peer, 1, SERVER_CHANNEL, bytes, SendFlags_Normal);           \
    }

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

extern Bytes
rgbaToJpeg(const uint8_t*, uint16_t width, uint16_t height);

extern bool
authenticateClient(pClient,
                   const AuthenticateFunc,
                   const Authentication*,
                   pRandomState);

extern AuthenticateResult
handleClientAuthentication(pClient,
                           const AuthenticateFunc,
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

// Input

void
askQuestion(const char* string);

typedef enum UserInputResult
{
    UserInputResult_NoInput,
    UserInputResult_Error,
    UserInputResult_Input
} UserInputResult,
  *pUserInputResult;

extern UserInputResult
getIndexFromUser(struct pollfd, pBytes, const uint32_t, uint32_t*, const bool);

extern UserInputResult
getStringFromUser(struct pollfd, pBytes, const bool);

extern bool
startWindowAudioStreaming(const struct pollfd, pBytes, pAudioState);

extern bool
startRecording(const char*, const int, pAudioState);

#define STREAM_TIMEOUT (1000u * 10u)

// Audio
#define ENABLE_FEC false
#define TEST_DECODER false
#define USE_AUDIO_CALLBACKS true

#define HIGH_QUALITY_AUDIO true
#if HIGH_QUALITY_AUDIO
#define PCM_SIZE sizeof(float)
#else
#define PCM_SIZE sizeof(opus_int16)
#endif

#define CQUEUE_SIZE MB(1)

typedef struct AudioState
{
    CQueue storedAudio;
    SDL_AudioSpec spec;
    Guid id;
    TemLangString name;
    union
    {
        OpusEncoder* encoder;
        OpusDecoder* decoder;
    };
    int32_tList sinks;
    SDL_AudioDeviceID deviceId;
    float volume;
    SDL_bool running;
    SDL_bool isRecording;
} AudioState, *pAudioState;

extern void AudioStateFree(pAudioState);

typedef AudioState* AudioStatePtr;

MAKE_COPY_AND_FREE(AudioStatePtr);
MAKE_DEFAULT_LIST(AudioStatePtr);

extern void
AudioStateRemoveFromList(pAudioStatePtrList, const Guid*);

extern bool
AudioStateFromGuid(const AudioStatePtrList*,
                   const Guid*,
                   const bool isRecording,
                   const AudioState**,
                   size_t*);

extern bool
AudioStateFromId(const AudioStatePtrList*,
                 const SDL_AudioDeviceID,
                 const bool isRecording,
                 const AudioState**,
                 size_t*);

extern bool
decodeWAV(const void*, size_t, pBytes);

extern bool
decodeOgg(const void*, size_t, pBytes);

extern bool
decodeMp3(const void*, size_t, pBytes);

extern bool
decodeOpus(pAudioState, const Bytes*, void**, int*);

extern int
audioLengthToFrames(const int frequency, const int duration);

// Video

extern bool
startWindowRecording(const Guid* id, const struct pollfd inputfd, pBytes bytes);

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

MAKE_COPY_AND_FREE(SDL_FPoint);
MAKE_DEFAULT_LIST(SDL_FPoint);

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

extern void
cleanupConfigurationsInRedis(redisContext*);

extern bool
serverIsDirty(redisContext*);

extern void
setServerIsDirty(redisContext*, const bool);

// Base 64

extern TemLangString
b64_encode(const Bytes* bytes);

extern bool
b64_decode(const char* in, pBytes bytes);

// Access

extern bool
clientHasAccess(const Client* client, const Access* access);

extern bool
clientHasReadAccess(const Client* client, const ServerConfiguration* config);

extern bool
clientHasWriteAccess(const Client* client, const ServerConfiguration* config);

extern bool
lowMemory();

extern SDL_mutex* clientMutex;
extern ClientData clientData;

// Pulse Audio

MAKE_DEFAULT_LIST(SinkInput);

extern bool stringToSinkInputs(pTemLangStringList, pSinkInputList);

extern bool
getSinkName(const int32_t, pTemLangString);

// Process

extern int
processOutputToNumber(const char*, int*);

extern int
processOutputToString(const char*, pTemLangString);

extern int
processOutputToStrings(const char*, pTemLangStringList);