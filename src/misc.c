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

int
printServerConfigurationForClient(const ServerConfiguration* config)
{
    return printf("%s (%s)\n",
                  config->name.buffer,
                  ServerConfigurationDataTagToCharString(config->data.tag));
}

bool
lowMemory()
{
    return currentAllocator->used() >
           (currentAllocator->totalSize() * 95u / 100u);
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
#if TEMSTREAM_SERVER
    hiredisAllocFuncs funcs = {
        .mallocFn = tsAllocate,
        .reallocFn = tsReallocate,
        .freeFn = tsFree,
        .callocFn = tsCalloc,
        .strdupFn = tsStrDup,
    };
    hiredisSetAllocators(&funcs);
#endif
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

#if !TEMSTREAM_SERVER

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
#endif

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
    }
    if (packet->referenceCount == 0) {
        enet_packet_destroy(packet);
    }
    enet_host_flush(host);
}

double
diff_timespec(const struct timespec* time1, const struct timespec* time0)
{
    return (time1->tv_sec - time0->tv_sec) +
           (time1->tv_nsec - time0->tv_nsec) / 1000000000.0;
}

#if USE_OPENCL
bool
OpenCLVideoInit(pOpenCLVideo vid, const WindowData* data)
{
    cl_platform_id platform_id = NULL;

    cl_int ret = clGetPlatformIDs(1, &platform_id, NULL);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to get OpenCL platforms: %d\n", ret);
        return false;
    }

    cl_uint deviceNum = 0;
    cl_device_id device_id = NULL;

    ret = clGetDeviceIDs(
      platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &deviceNum);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to get device ID: %d\n", ret);
        return false;
    }

    vid->context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL context: %d\n", ret);
        return false;
    }

    vid->command_queue =
      clCreateCommandQueueWithProperties(vid->context, device_id, 0, &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL command queue: %d\n", ret);
        return false;
    }

    const char* c = imageConversionsKernel;
    vid->program = clCreateProgramWithSource(vid->context, 1, &c, NULL, &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL program: %d\n", ret);
        return false;
    }

    ret = clBuildProgram(vid->program, 1, &device_id, NULL, NULL, NULL);
    if (ret != CL_SUCCESS) {
        char buffer[KB(4)];
        clGetProgramBuildInfo(vid->program,
                              device_id,
                              CL_PROGRAM_BUILD_LOG,
                              sizeof(buffer),
                              buffer,
                              0);
        fprintf(
          stderr, "Failed to build OpenCL program: %d\n%s\n", ret, buffer);
        return false;
    }

    vid->rgba2YuvKernel = clCreateKernel(vid->program, "rgba2Yuv", &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL kernel: %d\n", ret);
        return false;
    }

    vid->scaleImageKernel = clCreateKernel(vid->program, "scaleYUV", &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to create OpenCL kernel: %d\n", ret);
        return false;
    }

    const size_t size = data->width * data->height;
    const size_t scaledSize =
      (data->width * data->ratio.numerator / data->ratio.denominator) *
      (data->height * data->ratio.numerator / data->ratio.denominator);

    for (int i = 0; i < 4; ++i) {
        switch (i) {
            case 0:
                vid->rgba2YuvArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_READ_ONLY, size * 4, NULL, &ret);
                if (ret != CL_SUCCESS) {
                    fprintf(stderr,
                            "Failed to create OpenCL buffer %d: %d\n",
                            i,
                            ret);
                    return false;
                }
                vid->scaleImageArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_WRITE_ONLY, scaledSize, NULL, &ret);
                break;
            case 1:
                vid->rgba2YuvArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_READ_WRITE, size, NULL, &ret);
                if (ret != CL_SUCCESS) {
                    fprintf(stderr,
                            "Failed to create OpenCL buffer %d: %d\n",
                            i,
                            ret);
                    return false;
                }
                vid->scaleImageArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_WRITE_ONLY, scaledSize / 4, NULL, &ret);
                break;
            case 2:
                vid->rgba2YuvArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_READ_WRITE, size / 4, NULL, &ret);
                if (ret != CL_SUCCESS) {
                    fprintf(stderr,
                            "Failed to create OpenCL buffer %d: %d\n",
                            i,
                            ret);
                    return false;
                }
                vid->scaleImageArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_WRITE_ONLY, scaledSize / 4, NULL, &ret);
                break;
            case 3:
                vid->rgba2YuvArgs[i] = clCreateBuffer(
                  vid->context, CL_MEM_READ_WRITE, size / 4, NULL, &ret);
                break;
            default:
                return false;
        }
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to create OpenCL buffer %d: %d\n", i, ret);
            return false;
        }

        ret = clSetKernelArg(
          vid->rgba2YuvKernel, i, sizeof(cl_mem), (void*)&vid->rgba2YuvArgs[i]);
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to set OpenCL argument %d: %d\n", i, ret);
            return false;
        }
    }

    {
        const cl_uint rWidth = data->width;
        ret = clSetKernelArg(
          vid->rgba2YuvKernel, 4, sizeof(cl_uint), (void*)&rWidth);
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to set OpenCL argument %d: %d\n", 4, ret);
            return false;
        }
    }

    {
        cl_uint2 size = { .x = data->width, .y = data->height };
        ret = clSetKernelArg(vid->scaleImageKernel,
                             3,
                             sizeof(cl_uint2),
                             (void*)&size); // inSize
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to set OpenCL argument %d: %d\n", 3, ret);
            return false;
        }

        size.x = data->width * data->ratio.numerator / data->ratio.denominator;
        size.y = data->height * data->ratio.numerator / data->ratio.denominator;
        ret = clSetKernelArg(vid->scaleImageKernel,
                             7,
                             sizeof(cl_uint2),
                             (void*)&size); // outSize
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to set OpenCL argument %d: %d\n", 7, ret);
            return false;
        }
    }

    for (int i = 0; i < 3; ++i) {
        ret = clSetKernelArg(vid->scaleImageKernel,
                             i,
                             sizeof(cl_mem),
                             (void*)&vid->rgba2YuvArgs[i + 1]); // in Y,U,V
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to set OpenCL argument %d: %d\n", i, ret);
            return false;
        }
        ret = clSetKernelArg(vid->scaleImageKernel,
                             i + 4,
                             sizeof(cl_mem),
                             (void*)&vid->scaleImageArgs[i]); // out Y,U,V
        if (ret != CL_SUCCESS) {
            fprintf(
              stderr, "Failed to set OpenCL argument %d: %d\n", i + 4, ret);
            return false;
        }
    }

    return true;
}

void
OpenCLVideoFree(pOpenCLVideo vid)
{
    clFlush(vid->command_queue);
    clFinish(vid->command_queue);

    clReleaseKernel(vid->rgba2YuvKernel);
    for (int i = 0; i < 4; ++i) {
        clReleaseMemObject(vid->rgba2YuvArgs[i]);
    }

    clReleaseKernel(vid->scaleImageKernel);
    for (int i = 0; i < 3; ++i) {
        clReleaseMemObject(vid->scaleImageArgs[i]);
    }

    clReleaseProgram(vid->program);
    clReleaseCommandQueue(vid->command_queue);
    clReleaseContext(vid->context);
    memset(vid, 0, sizeof(*vid));
}
#endif