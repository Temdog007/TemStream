#pragma once

#include "main.h"

#include <ft2build.h>
#include FT_FREETYPE_H

// X11
#include <sys/ipc.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <libv4l2.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include <opus/opus.h>
#include <vorbis/codec.h>

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define USE_OPENCL true
#define USE_SDL_CONVERSION false

#if USE_OPENCL
#include <CL/cl.h>
typedef struct OpenCLVideo
{
    cl_context context;
    cl_command_queue command_queue;
    cl_program program;
    cl_kernel rgba2YuvKernel;
    cl_mem rgba2YuvArgs[4];
    cl_kernel scaleImageKernel;
    cl_mem scaleImageArgs[3];
} OpenCLVideo, *pOpenCLVideo;

extern bool
OpenCLVideoInit(pOpenCLVideo, const WindowData*);

extern void OpenCLVideoFree(pOpenCLVideo);

extern bool
rgbaToYuv(const uint8_t*, const WindowData*, void* ptrs[3], pOpenCLVideo);
#else
extern bool
rgbaToYuv(const uint8_t* rgba,
          const uint32_t width,
          const uint32_t height,
          uint32_t* argb,
          uint8_t* yuv);
#endif

typedef struct VideoEncoder VideoEncoder, *pVideoEncoder;
typedef struct VideoDecoder VideoDecoder, *pVideoDecoder;

#define USE_VPX true

#if USE_VPX
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

typedef struct VideoEncoder
{
    vpx_codec_ctx_t ctx;
    vpx_image_t img;
    int frameCount;
} VideoEncoder, *pVideoEncoder;

typedef struct VideoDecoder
{
    vpx_codec_ctx_t ctx;
} VideoDecoder, *pVideoDecoder;

#else
typedef struct VideoEncoder
{
    void* ctx;
    void* planes[3];
    int frameCount;
} VideoEncoder, *pVideoEncoder;

typedef struct VideoDecoder
{
    void* ctx;
} VideoDecoder, *pVideoDecoder;
#endif

extern bool
VideoEncoderInit(pVideoEncoder, const WindowData*, const bool forCamera);

extern void** VideoEncoderPlanes(pVideoEncoder);

extern void VideoEncoderFree(pVideoEncoder);

extern bool
VideoEncoderEncode(pVideoEncoder,
                   pVideoMessage,
                   pBytes,
                   const Guid*,
                   const WindowData*);

extern bool VideoDecoderInit(pVideoDecoder);

extern void
VideoDecoderDecode(pVideoDecoder, const Bytes*, const Guid*, uint64_t*);

extern void VideoDecoderFree(pVideoDecoder);

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>

#define PRINT_RENDER_INFO false

typedef struct AudioState AudioState, *pAudioState;

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

extern int
runClient(const Configuration*);

extern Bytes
rgbaToJpeg(const uint8_t*, uint16_t width, uint16_t height);

extern bool
filenameToExtension(const char*, pFileExtension);

extern bool CanSendFileToStream(FileExtensionTag, ServerConfigurationDataTag);

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
    SDL_bool isRecording;
} AudioState, *pAudioState;

extern void AudioStateFree(pAudioState);

typedef AudioState* AudioStatePtr;

MAKE_COPY_AND_FREE(AudioStatePtr);
MAKE_DEFAULT_LIST(AudioStatePtr);

extern AudioStatePtrList audioStates;

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

extern bool
recordWebcam(const Guid* id, const struct pollfd inputfd, pBytes bytes);

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

typedef struct RenderInfo RenderInfo, *pRenderInfo;

extern void
updateUiActors(const SDL_Event*, pRenderInfo);

extern void
updateUiActor(const SDL_Event*, pUiActor, const float, const float, int32_t*);

extern void
renderUiActor(pRenderInfo, pUiActor, SDL_Rect, int, int);

extern void renderUiActors(pRenderInfo);

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

extern SDL_mutex* clientMutex;
extern ClientData clientData;

// Pulse Audio

extern SinkInputList
getSinkInputs();

extern bool
recordSink(const SinkInput*, pAudioState);

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

// UI

extern UiActorList getUiMenuActors(pMenu);

typedef struct RenderInfo
{
    UiActorList uiActors;
    RandomState rs;
    Menu menu;
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    int32_t focusId;
    bool showUi;
} RenderInfo, *pRenderInfo;

extern bool
sendUpdateUiEvent();

extern bool setUiMenu(MenuTag);

extern bool
sendTextToServer(const char*,
                 const ServerConfigurationDataTag,
                 const Guid*,
                 pBytes,
                 SDL_Window*);

extern bool
sendFileToServer(const char*,
                 const ServerConfigurationDataTag,
                 const Guid*,
                 pBytes,
                 SDL_Window*);

extern const UiActor*
findUiActor(const UiActorList*, int32_t id);