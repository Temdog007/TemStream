#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

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
    Guid id;
    TemLangString name;
    GuidList connectedStreams;
    const ServerAuthentication* serverAuthentication;
    int32_t sockfd;
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
StreamMessageGuidEquals(const StreamMessage*, const Guid*);

extern bool
StreamDisplayGuidEquals(const StreamDisplay*, const Guid*);

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

struct addrinfo;
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

bool
clientSend(const Client*, const Bytes*, const bool sendSize);

bool
socketSend(const int, const Bytes*, bool);

bool
readAllData(const int sockfd, const uint64_t, pMessage, pBytes);

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

// Custom Events

typedef enum CustomEvent
{
    CustomEvent_Render = 0x31ab,
    CustomEvent_AddTexture,
    CustomEvent_UpdateStreamDisplay,
} CustomEvent,
  *pCustomEvent;

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