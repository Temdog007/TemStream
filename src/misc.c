#include <include/main.h>

const Guid ZeroGuid = { .numbers = { 0ULL, 0ULL } };

bool
parseIpAddress(const char* str, pIpAddress address)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        address->ip = TemLangStringCreateFromSize(str, i + 1, currentAllocator);
        address->port = TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    return false;
}

bool
parseCredentials(const char* str, pCredentials c)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        c->username = TemLangStringCreateFromSize(str, i + 1, currentAllocator);
        c->password = TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    return false;
}

TemLangString
RandomClientName(pRandomState rs)
{
    TemLangString name = RandomString(rs, 3, 10);
    TemLangStringInsertChar(&name, '@', 0);
    return name;
}

TemLangString
RandomString(pRandomState rs, const uint64_t min, const uint64_t max)
{
    const uint64_t len = min + (random64(rs) % (max - min));
    TemLangString name = { .allocator = currentAllocator,
                           .buffer = currentAllocator->allocate(len),
                           .size = len,
                           .used = 0 };
    for (uint64_t i = 0; i < len; ++i) {
        do {
            const char c = (char)(random64(rs) % 128ULL);
            switch (c) {
                case ':':
                    continue;
                case '_':
                case ' ':
                    break;
                default:
                    if (isalnum(c)) {
                        break;
                    }
                    continue;
            }
            TemLangStringAppendChar(&name, c);
            break;
        } while (true);
    }
    return name;
}

bool
filenameToExtension(const char* filename, pFileExtension f)
{
    const size_t size = strlen(filename);
    ssize_t index = (ssize_t)size - 2LL;
    for (; index >= 0; --index) {
        if (filename[index] == '.') {
            break;
        }
    }
    if (index < 0) {
        return false;
    }
    char buffer[512];
    TemLangString str = {
        .buffer = buffer, .allocator = NULL, .size = sizeof(buffer), .used = 0
    };
    for (size_t i = index + 1UL; i < size; ++i) {
        TemLangStringAppendChar(&str, (char)toupper(filename[i]));
    }
#if _DEBUG
    printf("File extensions: %s\n", str.buffer);
#endif

    if (TemLangStringEquals(&str, "TTF")) {
        f->tag = FileExtensionTag_font;
        f->font = NULL;
        return true;
    }

    f->image = ImageExtensionFromString(&str);
    if (f->image != ImageExtension_Invalid) {
        f->tag = FileExtensionTag_image;
        return true;
    }

    f->audio = AudioExtensionFromString(&str);
    if (f->audio != ImageExtension_Invalid) {
        f->tag = FileExtensionTag_audio;
        return true;
    }

    f->video = VideoExtensionFromString(&str);
    if (f->video != ImageExtension_Invalid) {
        f->tag = FileExtensionTag_video;
        return true;
    }

    return false;
}

StreamType
FileExtenstionToStreamType(const FileExtensionTag tag)
{
    switch (tag) {
        case FileExtensionTag_audio:
            return StreamType_Audio;
        case FileExtensionTag_video:
            return StreamType_Video;
        case FileExtensionTag_image:
            return StreamType_Image;
        default:
            return StreamType_Invalid;
    }
}

ENetPeer*
FindPeerFromData(ENetPeer* peers, const size_t count, const void* data)
{
    for (size_t i = 0; i < count; ++i) {
        if (peers[i].data == data) {
            return &peers[i];
        }
    }
    return NULL;
}

ENetPacket*
BytesToPacket(const Bytes* bytes, const bool reliable)
{
    return enet_packet_create(bytes->buffer,
                              bytes->used,
                              (reliable
                                 ? ENET_PACKET_FLAG_RELIABLE
                                 : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT));
}

bool
streamMessageIsReliable(const StreamMessage* m)
{
    switch (m->data.tag) {
        case StreamMessageDataTag_audio:
        case StreamMessageDataTag_Invalid:
            return false;
        default:
            return true;
    }
}

int
printBytes(const uint8_t* bytes, const size_t size)
{
    int result = 0;
    for (size_t i = 0; i < size; ++i) {
        result += printf("%02hhX%s", bytes[i], i == size - 1 ? "" : " ");
    }
    puts("");
    return result;
}

int
printSendingPacket(const ENetPacket* packet)
{
    return printf("Sending %zu bytes: ", packet->dataLength) +
           printBytes(packet->data, packet->dataLength);
}

int
printReceivedPacket(const ENetPacket* packet)
{
    return printf("Received %zu bytes: ", packet->dataLength) +
           printBytes(packet->data, packet->dataLength);
}

int
printIpAddress(const IpAddress* address)
{
    return printf(
      "Ip address: %s:%s\n", address->ip.buffer, address->port.buffer);
}

int
printStream(const Stream* stream)
{
    char guid[128];
    getGuidString(&stream->id, guid);
    return printf("%s: %s (%s)\n",
                  guid,
                  stream->name.buffer,
                  StreamTypeToCharString(stream->type));
}

int
printAudioSpec(const SDL_AudioSpec* spec)
{
    return printf("Frequency: %d Hz\nChannels: %u\nSilence: %u\nSamples: "
                  "%u\nBuffer size: %u bytes\n",
                  spec->freq,
                  spec->channels,
                  spec->silence,
                  spec->samples,
                  spec->size);
}

bool
StreamTypeMatchStreamMessage(const StreamType type,
                             const StreamMessageDataTag tag)
{
    return (type == StreamType_Text && tag == StreamMessageDataTag_text) ||
           (type == StreamType_Chat && tag == StreamMessageDataTag_chatMessage);
}

bool
StreamGuidEquals(const Stream* stream, const Guid* guid)
{
    return GuidEquals(&stream->id, guid);
}

bool
StreamNameEquals(const Stream* stream, const TemLangString* name)
{
    return TemLangStringsAreEqual(&stream->name, name);
}

bool
StreamTypeEquals(const Stream* stream, const StreamType* type)
{
    return stream->type == *type;
}

bool
StreamMessageGuidEquals(const StreamMessage* message, const Guid* guid)
{
    return GuidEquals(&message->id, guid);
}

bool
StreamDisplayGuidEquals(const StreamDisplay* display, const Guid* guid)
{
    return GuidEquals(&display->id, guid);
}

bool
GetStreamFromName(const StreamList* streams,
                  const TemLangString* name,
                  const Stream** stream,
                  size_t* index)
{
    return StreamListFindIf(
      streams, (StreamListFindFunc)StreamNameEquals, name, stream, index);
}

bool
GetStreamFromType(const StreamList* streams,
                  const StreamType type,
                  const Stream** stream,
                  size_t* index)
{
    return StreamListFindIf(
      streams, (StreamListFindFunc)StreamTypeEquals, &type, stream, index);
}

bool
GetStreamFromGuid(const StreamList* streams,
                  const Guid* guid,
                  const Stream** stream,
                  size_t* index)
{
    return StreamListFindIf(
      streams, (StreamListFindFunc)StreamGuidEquals, guid, stream, index);
}

bool
GetStreamMessageFromGuid(const StreamMessageList* streams,
                         const Guid* guid,
                         const StreamMessage** stream,
                         size_t* index)
{
    return StreamMessageListFindIf(
      streams,
      (StreamMessageListFindFunc)StreamMessageGuidEquals,
      guid,
      stream,
      index);
}

bool
GetStreamDisplayFromGuid(const StreamDisplayList* displays,
                         const Guid* guid,
                         const StreamDisplay** display,
                         size_t* index)
{
    return StreamDisplayListFindIf(
      displays,
      (StreamDisplayListFindFunc)StreamDisplayGuidEquals,
      guid,
      display,
      index);
}

bool
GetClientFromGuid(const pClientList* list,
                  const Guid* guid,
                  const pClient** client,
                  size_t* index)
{
    return pClientListFindIf(
      list, (pClientListFindFunc)ClientGuidEquals, guid, client, index);
}

bool
ClientGuidEquals(const pClient* client, const Guid* guid)
{
    return GuidEquals(&(*client)->id, guid);
}

typedef struct ThreadSafeAllocator
{
    SDL_mutex* mutex;
    const Allocator* allocator;
} TSAllocator, *pTSAllocator;

TSAllocator tsAllocator = { 0 };

void*
tsAllocate(const size_t size)
{
    void* data = NULL;
    IN_MUTEX(tsAllocator.mutex, end, {
        data = tsAllocator.allocator->allocate(size);
    });
    return data;
}

void*
tsReallocate(void* ptr, const size_t size)
{
    void* data = NULL;
    IN_MUTEX(tsAllocator.mutex, end, {
        data = tsAllocator.allocator->reallocate(ptr, size);
    });
    return data;
}

void
tsFree(void* data)
{
    IN_MUTEX(tsAllocator.mutex, end, { tsAllocator.allocator->free(data); });
}

size_t
tsUsed()
{
    size_t i;
    IN_MUTEX(tsAllocator.mutex, end, { i = tsAllocator.allocator->used(); });
    return i;
}

size_t
tsTotalSize()
{
    size_t i;
    IN_MUTEX(
      tsAllocator.mutex, end, { i = tsAllocator.allocator->totalSize(); });
    return i;
}

MAKE_FREE_LIST_ALLOCATOR(TemStreamAllocator);

Allocator
makeTSAllocator(const size_t memory)
{
    TemStreamAllocator = FreeListAllocatorCreate(
      "TemStream allocator", memory, PlacementPolicy_First);
    static Allocator a = { 0 };
    a = make_TemStreamAllocator_allocator();
    tsAllocator.allocator = &a;
    tsAllocator.mutex = SDL_CreateMutex();
    return (Allocator){ .allocate = tsAllocate,
                        .free = tsFree,
                        .reallocate = tsReallocate,
                        .used = tsUsed,
                        .totalSize = tsTotalSize };
}

void
freeTSAllocator()
{
    FreeListAllocatorDelete(&TemStreamAllocator);
}

bool
streamTypeMatchesMessage(const StreamType type, const StreamMessageDataTag tag)
{
    switch (type) {
        case StreamType_Audio:
            return tag == StreamMessageDataTag_audio;
        case StreamType_Text:
            return tag == StreamMessageDataTag_text;
        case StreamType_Chat:
            return tag == StreamMessageDataTag_chatMessage;
        case StreamType_Image:
            return tag == StreamMessageDataTag_image;
        default:
            return false;
    }
}

SDL_AudioSpec
makeAudioSpec(SDL_AudioCallback callback, void* userdata)
{
#if AUDIO == HIGH_AUDIO
    return (SDL_AudioSpec){ .freq = 44100,
                            .format = AUDIO_F32,
                            .channels = 2,
                            .samples = 4096,
                            .callback = callback,
                            .userdata = userdata };
#elif AUDIO == MED_AUDIO
    return (SDL_AudioSpec){ .freq = 22050,
                            .format = AUDIO_S16,
                            .channels = 1,
                            .samples = 2048,
                            .callback = callback,
                            .userdata = userdata };
#else
    return (SDL_AudioSpec){ .freq = 8000,
                            .format = AUDIO_S8,
                            .channels = 1,
                            .samples = 2048,
                            .callback = callback,
                            .userdata = userdata };
#endif
}