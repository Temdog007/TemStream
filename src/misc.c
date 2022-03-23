#include <include/main.h>

const Guid ZeroGuid = { .numbers = { 0ULL, 0ULL } };
SDL_atomic_t runningThreads = { 0 };

void
sendBytes(ENetPeer* peers,
          const size_t peerCount,
          const enet_uint32 channel,
          const Bytes* bytes,
          const SendFlags flags)
{
    if (bytes->used == 0) {
        return;
    }
    ENetPacket* packet = BytesToPacket(bytes->buffer, bytes->used, flags);
    for (size_t i = 0; i < peerCount; ++i) {
        PEER_SEND(&peers[i], channel, packet);
    }
    if (packet->referenceCount == 0) {
        enet_packet_destroy(packet);
    }
}

bool
clientHasAccess(const Client* client, const Access* access)
{
    switch (access->tag) {
        case AccessTag_anyone:
            return true;
        case AccessTag_allowed:
            return TemLangStringListFindIf(
              &access->allowed,
              (TemLangStringListFindFunc)TemLangStringsAreEqual,
              &client->name,
              NULL,
              NULL);
        case AccessTag_disallowed:
            return TemLangStringListFindIf(
              &access->disallowed,
              (TemLangStringListFindFunc)TemLangStringsAreEqual,
              &client->name,
              NULL,
              NULL);
        default:
            break;
    }
    return false;
}

bool
clientHasReadAccess(const Client* client, const ServerConfiguration* config)
{
    return clientHasAccess(client, &config->readers);
}

bool
clientHasWriteAccess(const Client* client, const ServerConfiguration* config)
{
    return clientHasAccess(client, &config->writers);
}

uint64_t
randomBetween64(pRandomState rs, const uint64_t min, const uint64_t max)
{
    return min + (random64(rs) % (max - min));
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
    const uint64_t len = randomBetween64(rs, min, max);
    TemLangString name = { .allocator = currentAllocator };
    for (size_t i = 0; i < len; ++i) {
        TemLangStringAppendChar(&name, RandomChar(rs));
    }
    return name;
}

char
RandomChar(pRandomState rs)
{
    do {
        const char c = (char)(random64(rs) % 128ULL);
        switch (c) {
            case ':':
                continue;
            case '_':
            case ' ':
                return c;
            default:
                if (isalnum(c)) {
                    return c;
                }
                continue;
        }
    } while (true);
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
    char buffer[512] = { 0 };
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

    if (TemLangStringEquals(&str, "TXT")) {
        f->tag = FileExtensionTag_text;
        f->text = NULL;
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

    return false;
}

bool
CanSendFileToStream(const FileExtensionTag tag,
                    const ServerConfigurationDataTag s)
{
    switch (s) {
        case ServerConfigurationDataTag_audio:
            return tag == FileExtensionTag_audio;
        case ServerConfigurationDataTag_chat:
        case ServerConfigurationDataTag_text:
            return tag == FileExtensionTag_text;
        case ServerConfigurationDataTag_image:
            return tag == FileExtensionTag_image;
        default:
            return false;
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
BytesToPacket(const void* data, const size_t length, const SendFlags flags)
{
    return enet_packet_create(data, length, flags);
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
printAuthentication(const Authentication* auth)
{
    return printf("Authentication: %s (%d)\n", auth->value.buffer, auth->type);
}

int
printPort(const Port* port)
{
    int offset = printf("Port: ");
    switch (port->tag) {
        case PortTag_port:
            offset += printf("%u\n", port->port);
            break;
        case PortTag_portRange:
            offset += printf("min=%u;max=%u\n",
                             port->portRange.minPort,
                             port->portRange.maxPort);
            break;
        default:
            break;
    }
    return offset;
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
GetClientFromGuid(const pClientList* list,
                  const Guid* guid,
                  const pClient** client,
                  size_t* index)
{
    return pClientListFindIf(
      list, (pClientListFindFunc)ClientGuidEquals, guid, client, index);
}

bool
GetStreamFromName(const ServerConfigurationList* list,
                  const TemLangString* name,
                  const ServerConfiguration** s,
                  size_t* i)
{
    return ServerConfigurationListFindIf(
      list,
      (ServerConfigurationListFindFunc)ServerConfigurationNameEquals,
      name,
      s,
      i);
}

bool
GetStreamFromType(const ServerConfigurationList* list,
                  const ServerConfigurationDataTag tag,
                  const ServerConfiguration** s,
                  size_t* i)
{
    return ServerConfigurationListFindIf(
      list,
      (ServerConfigurationListFindFunc)ServerConfigurationTagEquals,
      &tag,
      s,
      i);
}

bool
StreamDisplayNameEquals(const StreamDisplay* display, const TemLangString* name)
{
    return TemLangStringsAreEqual(&display->config.name, name);
}

bool
StreamDisplayGuidEquals(const StreamDisplay* display, const Guid* id)
{
    return GuidEquals(&display->id, id);
}

bool
GetStreamDisplayFromName(const StreamDisplayList* list,
                         const TemLangString* name,
                         const StreamDisplay** display,
                         size_t* index)
{
    return StreamDisplayListFindIf(
      list,
      (StreamDisplayListFindFunc)StreamDisplayNameEquals,
      name,
      display,
      index);
}

bool
GetStreamDisplayFromGuid(const StreamDisplayList* list,
                         const Guid* id,
                         const StreamDisplay** display,
                         size_t* index)
{
    return StreamDisplayListFindIf(
      list,
      (StreamDisplayListFindFunc)StreamDisplayGuidEquals,
      id,
      display,
      index);
}

bool
ServerConfigurationNameEquals(const ServerConfiguration* c,
                              const TemLangString* s)
{
    return TemLangStringsAreEqual(&c->name, s);
}

bool
ServerConfigurationTagEquals(const ServerConfiguration* c,
                             const ServerConfigurationDataTag* tag)
{
    return c->data.tag == *tag;
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

void*
tsCalloc(const size_t count, const size_t size)
{
    return tsAllocate(count * size);
}

char*
tsStrDup(const char* c)
{
    size_t len = strlen(c);
    char* rval = tsAllocate(len + 1);
    memcpy(rval, c, len);
    return rval;
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
    hiredisAllocFuncs funcs = {
        .mallocFn = tsAllocate,
        .reallocFn = tsReallocate,
        .freeFn = tsFree,
        .callocFn = tsCalloc,
        .strdupFn = tsStrDup,
    };
    hiredisSetAllocators(&funcs);
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

SDL_AudioSpec
makeAudioSpec(SDL_AudioCallback callback, void* userdata)
{
#if HIGH_QUALITY_AUDIO
    return (SDL_AudioSpec){ .freq = 48000,
                            .format = AUDIO_F32,
                            .channels = 2,
                            .samples = 4096,
                            .callback = callback,
                            .userdata = userdata };
#else
    return (SDL_AudioSpec){ .freq = 16000,
                            .format = AUDIO_S16,
                            .channels = 1,
                            .samples = 2048,
                            .callback = callback,
                            .userdata = userdata };
#endif
}

void
closeHostAndPeer(ENetHost* host, ENetPeer* peer)
{
    if (peer != NULL && peer->state != ENET_PEER_STATE_DISCONNECTED) {
        enet_peer_disconnect(peer, 0);
        ENetEvent event = { 0 };
        while (enet_host_service(host, &event, 3000U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE:
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    puts("Disconnected gracefully from server");
                    goto continueEnd;
                default:
                    break;
            }
        }
        enet_peer_reset(peer);
    }
continueEnd:
    if (host != NULL) {
        enet_host_destroy(host);
    }
}

void
AudioStateFree(pAudioState state)
{
    SDL_CloseAudioDevice(state->deviceId);
    if (state->isRecording) {
        if (state->encoder != NULL) {
            currentAllocator->free(state->encoder);
        }
    } else {
        if (state->decoder != NULL) {
            currentAllocator->free(state->decoder);
        }
    }
    for (size_t i = 0; i < state->sinks.used; ++i) {
        unloadSink(state->sinks.buffer[i]);
    }
    int32_tListFree(&state->sinks);
    TemLangStringFree(&state->name);
    CQueueFree(&state->storedAudio);
}

void
AudioStateRemoveFromList(pAudioStatePtrList list, const Guid* id)
{
    size_t i = 0;
    while (i < list->used) {
        pAudioState ptr = list->buffer[i];
        if (GuidEquals(&ptr->id, id)) {
            AudioStateFree(ptr);
            currentAllocator->free(ptr);
            AudioStatePtrListSwapRemove(list, i);
        } else {
            ++i;
        }
    }
}

bool
AudioStateFromGuid(const AudioStatePtrList* list,
                   const Guid* id,
                   const bool isRecording,
                   const AudioState** state,
                   size_t* index)
{
    for (size_t i = 0; i < list->used; ++i) {
        const AudioState* ptr = list->buffer[i];
        if (ptr->isRecording == isRecording && GuidEquals(&ptr->id, id)) {
            if (state != NULL) {
                *state = ptr;
            }
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

bool
AudioStateFromId(const AudioStatePtrList* list,
                 const SDL_AudioDeviceID id,
                 const bool isRecording,
                 const AudioState** state,
                 size_t* index)
{
    for (size_t i = 0; i < list->used; ++i) {
        const AudioState* ptr = list->buffer[i];
        if (ptr->isRecording == isRecording && ptr->deviceId == id) {
            if (state != NULL) {
                *state = ptr;
            }
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

bool
serverIsDirty(redisContext* ctx)
{
    redisReply* reply =
      redisCommand(ctx, "GET %s", TEM_STREAM_SERVER_DIRTY_KEY);

    bool result = false;
    switch (reply->type) {
        case REDIS_REPLY_BOOL:
        case REDIS_REPLY_INTEGER:
            result = reply->integer == 1;
            break;
        case REDIS_REPLY_STRING:
            result = atoi(reply->str);
            break;
        case REDIS_REPLY_ERROR:
            fprintf(stderr, "Redis error: %s\n", reply->str);
            break;
        default:
            break;
    }
    freeReplyObject(reply);
    return result;
}

void
setServerIsDirty(redisContext* ctx, const bool state)
{
    redisReply* reply =
      redisCommand(ctx, "SET %s %d", TEM_STREAM_SERVER_DIRTY_KEY, state);
    if (reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
    }
    freeReplyObject(reply);
}

bool
writeConfigurationToRedis(redisContext* ctx, const ServerConfiguration* c)
{
    Bytes bytes = { .allocator = currentAllocator };
    MESSAGE_SERIALIZE(ServerConfiguration, (*c), bytes);

    TemLangString str = b64_encode(&bytes);

    redisReply* reply =
      redisCommand(ctx, "LPUSH %s %s", TEM_STREAM_SERVER_KEY, str.buffer);

    const bool result = reply->type == REDIS_REPLY_INTEGER;
    if (!result && reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
    }

    freeReplyObject(reply);
    TemLangStringFree(&str);
    uint8_tListFree(&bytes);

    setServerIsDirty(ctx, true);
    return result;
}

bool
removeConfigurationFromRedis(redisContext* ctx, const ServerConfiguration* c)
{
    Bytes bytes = { .allocator = currentAllocator };
    MESSAGE_SERIALIZE(ServerConfiguration, (*c), bytes);

    TemLangString str = b64_encode(&bytes);

    redisReply* reply =
      redisCommand(ctx, "LREM %s 0 %s", TEM_STREAM_SERVER_KEY, str.buffer);

    const bool result = reply->type == REDIS_REPLY_INTEGER;
    if (!result && reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error: %s\n", reply->str);
    }

    freeReplyObject(reply);
    TemLangStringFree(&str);
    uint8_tListFree(&bytes);

    setServerIsDirty(ctx, true);
    return result;
}

void
sendPacketToReaders(ENetHost* host, ENetPacket* packet, const Access* access)
{
    for (size_t i = 0; i < host->peerCount; ++i) {
        ENetPeer* peer = &host->peers[i];
        pClient client = peer->data;
        if (client == NULL || !clientHasAccess(client, access)) {
            continue;
        }
        PEER_SEND(peer, SERVER_CHANNEL, packet);
#if LOG_READER
        printf("Sending packet to client '%s'\n", client->name.buffer);
#endif
    }
    if (packet->referenceCount == 0) {
        enet_packet_destroy(packet);
    }
    enet_host_flush(host);
}

ServerConfigurationList
getStreams(redisContext* ctx)
{
    ServerConfigurationList list = { .allocator = currentAllocator };

    Bytes bytes = { .allocator = currentAllocator };
    redisReply* reply =
      redisCommand(ctx, "LRANGE %s 0 -1", TEM_STREAM_SERVER_KEY);
    if (reply->type != REDIS_REPLY_ARRAY) {
        if (reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Redis error: %s\n", reply->str);
        } else {
            fprintf(stderr, "Failed to get list from redis\n");
        }
        goto end;
    }

    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply* r = reply->element[i];
        if (r->type != REDIS_REPLY_STRING) {
            continue;
        }

        if (!b64_decode(r->str, &bytes)) {
            continue;
        }
        ServerConfiguration s = { 0 };
        MESSAGE_DESERIALIZE(ServerConfiguration, s, bytes);
        ServerConfigurationListAppend(&list, &s);
        ServerConfigurationFree(&s);
    }
end:
    uint8_tListFree(&bytes);
    freeReplyObject(reply);
    return list;
}

void
cleanupConfigurationsInRedis(redisContext* ctx)
{
    PRINT_MEMORY;
    ServerConfigurationList servers = getStreams(ctx);
    PRINT_MEMORY;
    printf("Verifying %u server(s)\n", servers.used);
    for (uint32_t i = 0; i < servers.used; ++i) {
        const ServerConfiguration* config = &servers.buffer[i];

        ENetAddress address = { 0 };
        enet_address_set_host(&address, config->hostname.buffer);
        address.port = config->port.port;
        ENetHost* host = enet_host_create(NULL, 2, 1, 0, 0);
        ENetPeer* peer = NULL;
        bool verified = false;
        if (host == NULL) {
            goto endLoop;
        }

        peer = enet_host_connect(host, &address, 2, 0);
        if (peer == NULL) {
            goto endLoop;
        }

        ENetEvent e = { 0 };
        while (enet_host_service(host, &e, 100U) > 0) {
            switch (e.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    verified = true;
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    enet_packet_destroy(e.packet);
                    break;
                default:
                    break;
            }
        }

    endLoop:
        closeHostAndPeer(host, peer);
        if (!verified) {
            printf("Removing '%s' server from list\n", config->name.buffer);
            removeConfigurationFromRedis(ctx, config);
        }
    }
    ServerConfigurationListFree(&servers);
    PRINT_MEMORY;
}

#if USE_VP8
int
vpx_img_plane_width(const vpx_image_t* img, const int plane)
{
    if (plane > 0 && img->x_chroma_shift > 0) {
        return (img->d_w + 1) >> img->x_chroma_shift;
    } else {
        return img->d_w;
    }
}

int
vpx_img_plane_height(const vpx_image_t* img, const int plane)
{
    if (plane > 0 && img->y_chroma_shift > 0) {
        return (img->d_h + 1) >> img->y_chroma_shift;
    } else {
        return img->d_h;
    }
}
#endif

double
diff_timespec(const struct timespec* time1, const struct timespec* time0)
{
    return (time1->tv_sec - time0->tv_sec) +
           (time1->tv_nsec - time0->tv_nsec) / 1000000000.0;
}

void
initJpegDest(j_compress_ptr cinfo)
{
    pBytes bytes = (pBytes)cinfo->client_data;
    cinfo->dest->next_output_byte = &bytes->buffer[0];
    cinfo->dest->free_in_buffer = bytes->size;
}

boolean
emptyJpegDst(j_compress_ptr cinfo)
{
    pBytes bytes = (pBytes)cinfo->client_data;
    const uint32_t oldSize = bytes->size;
    bytes->used = bytes->size - cinfo->dest->free_in_buffer;
    const bool result = uint8_tListRellocateIfNeeded(bytes);
    cinfo->dest->next_output_byte = &bytes->buffer[oldSize];
    cinfo->dest->free_in_buffer = bytes->size - oldSize;
    return result;
}

void
termJpegDest(j_compress_ptr cinfo)
{
    pBytes bytes = (pBytes)cinfo->client_data;
    bytes->used = bytes->size - cinfo->dest->free_in_buffer;
}

Bytes
rgbaToJpeg(const uint8_t* buffer, const uint16_t width, const uint16_t height)
{
    struct jpeg_compress_struct cinfo;

    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    const uint32_t byteSize = width * height * 3;

    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(byteSize),
                    .size = byteSize,
                    .used = 0 };
    cinfo.client_data = &bytes;

    struct jpeg_destination_mgr dest = { .init_destination = &initJpegDest,
                                         .empty_output_buffer = &emptyJpegDst,
                                         .term_destination = &termJpegDest };
    cinfo.dest = &dest;

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, 50, TRUE);

    cinfo.dct_method = JDCT_FLOAT;

    jpeg_start_compress(&cinfo, TRUE);

    const int stride = width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row[] = { (JSAMPROW)&buffer[cinfo.next_scanline * stride] };
        jpeg_write_scanlines(&cinfo, row, 1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);

    return bytes;
}