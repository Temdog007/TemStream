#include <include/main.h>

#define MIN_WIDTH 32
#define MIN_HEIGHT 32

SDL_mutex* clientMutex = NULL;
ClientData clientData = { 0 };

SDL_atomic_t runningThreads = { 0 };

void
sendAudioPackets(OpusEncoder* encoder,
                 pBytes audio,
                 const SDL_AudioSpec spec,
                 ENetPeer*);

void
consumeAudio(const Bytes*, const Guid*);

void
renderDisplays()
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_Render;
    SDL_PushEvent(&e);
}

void
askQuestion(const char* string)
{
    const size_t size = strlen(string);
    for (size_t i = 0; i < size; ++i) {
        putchar('-');
    }
    printf("\n%s|\n", string);
    for (size_t i = 0; i < size; ++i) {
        putchar('-');
    }
    puts("");
}

ClientConfiguration
defaultClientConfiguration()
{
    return (ClientConfiguration){
        .fullscreen = false,
        .windowWidth = 800,
        .windowHeight = 600,
        .fontSize = 48,
        .noGui = false,
        .noAudio = false,
        .ttfFile = TemLangStringCreate(
          "/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf", currentAllocator)
    };
}

bool
parseClientConfiguration(const int argc,
                         const char** argv,
                         pConfiguration configuration)
{
    configuration->data.tag = ConfigurationDataTag_client;
    configuration->data.client = defaultClientConfiguration();
    pClientConfiguration client = &configuration->data.client;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-F", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "--font", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "-S", keyLen, { goto parseSize; });
        STR_EQUALS(key, "--font-size", keyLen, { goto parseSize; });
        STR_EQUALS(key, "-W", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "--width", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "-H", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "--height", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "-F", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "--fullscreen", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "-T", keyLen, { goto parseToken; });
        STR_EQUALS(key, "--token", keyLen, { goto parseToken; });
        STR_EQUALS(key, "-C", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "--credentials", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "-NG", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "--no-gui", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "-NA", keyLen, { goto parseNoAudio; });
        STR_EQUALS(key, "--no-audio", keyLen, { goto parseNoAudio; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Client", key, value);
            return false;
        }
        continue;
    parseTTF : {
        client->ttfFile = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseSize : {
        client->fontSize = atoi(value);
        continue;
    }
    parseWidth : {
        client->windowWidth = atoi(value);
        continue;
    }
    parseHeight : {
        client->windowHeight = atoi(value);
        continue;
    }
    parseFullscreen : {
        client->fullscreen = atoi(value);
        continue;
    }
    parseToken : {
        client->authentication.tag = ClientAuthenticationTag_token;
        client->authentication.token =
          TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseCredentials : {
        client->authentication.tag = ClientAuthenticationTag_credentials;
        if (!parseCredentials(value, &client->authentication.credentials)) {
            return false;
        }
        continue;
    }
    parseNoGui : {
        client->noGui = atoi(value);
        continue;
    }
    parseNoAudio : {
        client->noAudio = atoi(value);
        continue;
    }
    }
    return true;
}

void
ClientFree(pClient client)
{
    uint8_tListFree(&client->payload);
    TemLangStringFree(&client->name);
}

int
printClientConfiguration(const ClientConfiguration* configuration)
{
    return printf("Width: %d; Height: %d; Fullscreen: %d; TTF file: %s\n",
                  configuration->windowWidth,
                  configuration->windowHeight,
                  configuration->fullscreen,
                  configuration->ttfFile.buffer);
}

void
camelCaseToNormal(pTemLangString str)
{
    for (size_t i = 1; i < str->used; ++i) {
        const char c = str->buffer[i];
        if (isupper(c) && islower(str->buffer[i - 1])) {
            TemLangStringInsertChar(str, ' ', i);
        }
    }
}

typedef enum UserInputResult
{
    UserInputResult_NoInput,
    UserInputResult_Error,
    UserInputResult_Input
} UserInputResult,
  *pUserInputResult;

UserInputResult
getUserInput(struct pollfd inputfd,
             pBytes bytes,
             ssize_t* output,
             const int timeout)
{
    switch (poll(&inputfd, 1, timeout)) {
        case -1:
            perror("poll");
            return UserInputResult_Error;
        case 0:
            return UserInputResult_NoInput;
        default:
            break;
    }

    if ((inputfd.revents & POLLIN) == 0) {
        return UserInputResult_NoInput;
    }

    const size_t size = read(STDIN_FILENO, bytes->buffer, bytes->size) - 1;
    if (size <= 0) {
        perror("read");
        return UserInputResult_Error;
    }
    // Remove new line character
    bytes->buffer[size] = '\0';
    if (output != NULL) {
        *output = size;
    }
    return UserInputResult_Input;
}

UserInputResult
getIndexFromUser(struct pollfd inputfd,
                 pBytes bytes,
                 const uint32_t max,
                 uint32_t* index,
                 const bool keepPolling)
{
    if (max < 2) {
        *index = 0;
        return UserInputResult_Input;
    }
    ssize_t size = 0;
    while (!appDone) {
        switch (getUserInput(inputfd, bytes, &size, CLIENT_POLL_WAIT)) {
            case UserInputResult_Error:
                return UserInputResult_Error;
            case UserInputResult_NoInput:
                if (keepPolling) {
                    continue;
                } else {
                    return UserInputResult_NoInput;
                }
            default:
                break;
        }
        char* end = NULL;
        *index = (uint32_t)strtoul((const char*)bytes->buffer, &end, 10) - 1UL;
        if (end != (char*)&bytes->buffer[size] || *index >= max) {
            printf("Enter a number between 1 and %u\n", max);
            return UserInputResult_Error;
        }
        return UserInputResult_Input;
    }
    return UserInputResult_NoInput;
}

void
sendAuthentication(ENetPeer* peer, const StreamType type)
{
    uint8_t buffer[KB(2)] = { 0 };
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = buffer,
                    .used = 0,
                    .size = sizeof(buffer) };
    switch (type) {
        case StreamType_Lobby: {
            LobbyMessage message = { 0 };
            message.tag = LobbyMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(LobbyMessage, message, bytes);
        } break;
        case StreamType_Chat: {
            ChatMessage message = { 0 };
            message.tag = ChatMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(ChatMessage, message, bytes);
        } break;
        case StreamType_Text: {
            TextMessage message = { 0 };
            message.tag = TextMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(TextMessage, message, bytes);
        } break;
        case StreamType_Audio: {
            AudioMessage message = { 0 };
            message.tag = AudioMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(AudioMessage, message, bytes);
        } break;
        case StreamType_Image: {
            ImageMessage message = { 0 };
            message.tag = ImageMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(ImageMessage, message, bytes);
        } break;
        default:
            break;
    }

    sendBytes(peer, 1, peer->mtu, CLIENT_CHANNEL, &bytes, true);
}

int
streamConnectionThread(void* ptr)
{
    pStream stream = (pStream)ptr;

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        appDone = true;
        goto end;
    }
    {
        ENetAddress* address = (ENetAddress*)&stream->address;
        peer = enet_host_connect(host, address, 2, stream->type);
        char buffer[512] = { 0 };
        enet_address_get_host_ip(address, buffer, sizeof(buffer));
        printf("Connecting to server: %s:%u...\n", buffer, address->port);
    }
    if (peer == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        appDone = true;
        goto end;
    }

    ENetEvent event = { 0 };
    if (enet_host_service(host, &event, 5000U) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT &&
        event.data == (uint32_t)stream->type) {
        char buffer[KB(1)] = { 0 };
        enet_address_get_host_ip(&event.peer->address, buffer, sizeof(buffer));
        printf(
          "Connected to server: %s:%u\n", buffer, event.peer->address.port);
        sendAuthentication(peer, stream->type);
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        appDone = true;
        goto end;
    }

    while (!appDone) {
        while (enet_host_service(host, &event, 100U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("Unexpected connect message from '%s(%s)' stream",
                           stream->name.buffer,
                           StreamTypeToCharString(stream->type));
                    goto end;
                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("Disconnect from '%s(%s)' stream",
                           stream->name.buffer,
                           StreamTypeToCharString(stream->type));
                    goto end;
                case ENET_EVENT_TYPE_RECEIVE: {
                } break;
                case ENET_EVENT_TYPE_NONE:
                    break;
                default:
                    break;
            }
        }
    }

end:
    StreamFree(stream);
    currentAllocator->free(stream);
    closeHostAndPeer(host, peer);
    SDL_AtomicDecRef(&runningThreads);
    return EXIT_SUCCESS;
}

void
selectAStreamToConnectTo(struct pollfd inputfd, pBytes bytes)
{
    const uint32_t streamNum = clientData.allStreams.used;
    if (streamNum == 0) {
        puts("No streams to connect to");
        return;
    }

    askQuestion("Select a stream to connect to");
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Stream* stream = &clientData.allStreams.buffer[i];
        printf("%u) ", i + 1U);
        printStream(stream);
    }
    puts("");

    uint32_t i = 0;
    if (streamNum == 1) {
        puts("Connecting to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streamNum, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling connecting to stream");
        return;
    }

    pStream s = currentAllocator->allocate(sizeof(Stream));
    StreamCopy(s, &clientData.allStreams.buffer[i], currentAllocator);
    SDL_Thread* thread =
      SDL_CreateThread((SDL_ThreadFunction)streamConnectionThread, "stream", s);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        StreamFree(s);
        currentAllocator->free(s);
    }

    SDL_AtomicIncRef(&runningThreads);
    SDL_DetachThread(thread);
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd, pBytes bytes)
{
    pStreamDisplayList list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("Not connected to any streams");
        return;
    }

    askQuestion("Select a stream to disconnect from");
    for (uint32_t i = 0; i < list->used; ++i) {
        printf("%u) ", i + 1U);
        printStream(&list->buffer[i].stream);
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1) {
        puts("Disconnecting from only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceled disconnecting from stream");
        return;
    }

    StreamDisplayListSwapRemove(list, i);
}

int
audioLengthToFrames(const int frequency, const int duration)
{
    switch (duration) {
        case OPUS_FRAMESIZE_2_5_MS:
            return frequency / 400;
        case OPUS_FRAMESIZE_5_MS:
            return frequency / 200;
        case OPUS_FRAMESIZE_10_MS:
            return frequency / 100;
        case OPUS_FRAMESIZE_20_MS:
            return frequency / 50;
        case OPUS_FRAMESIZE_40_MS:
            return frequency / 25;
        case OPUS_FRAMESIZE_60_MS:
            return audioLengthToFrames(frequency, OPUS_FRAMESIZE_20_MS) * 3;
        case OPUS_FRAMESIZE_80_MS:
            return audioLengthToFrames(frequency, OPUS_FRAMESIZE_40_MS) * 2;
        case OPUS_FRAMESIZE_100_MS:
            return frequency / 10;
        case OPUS_FRAMESIZE_120_MS:
            return audioLengthToFrames(frequency, OPUS_FRAMESIZE_60_MS) * 2;
        default:
            return 0;
    }
}

int
closestValidFrameCount(const int frequency, const int frames)
{
    const int values[] = { OPUS_FRAMESIZE_120_MS, OPUS_FRAMESIZE_100_MS,
                           OPUS_FRAMESIZE_80_MS,  OPUS_FRAMESIZE_60_MS,
                           OPUS_FRAMESIZE_40_MS,  OPUS_FRAMESIZE_20_MS,
                           OPUS_FRAMESIZE_10_MS,  OPUS_FRAMESIZE_5_MS,
                           OPUS_FRAMESIZE_2_5_MS };
    const int arrayLength = sizeof(values) / sizeof(int);
    if (frames >= audioLengthToFrames(frequency, values[0])) {
        return audioLengthToFrames(frequency, values[0]);
    }
    if (frames <= audioLengthToFrames(frequency, values[arrayLength - 1])) {
        return audioLengthToFrames(frequency, values[arrayLength - 1]);
    }
    for (int i = 1; i < arrayLength; ++i) {
        const int prev = audioLengthToFrames(frequency, values[i - 1]);
        const int current = audioLengthToFrames(frequency, values[i]);
        if (prev >= frames && frames >= current) {
            return audioLengthToFrames(frequency, values[i]);
        }
    }
    return audioLengthToFrames(frequency, values[arrayLength - 1]);
}

void
storeOpusAudio(pAudioState state, const Bytes* compressed)
{
    void* uncompressed = currentAllocator->allocate(MAX_PACKET_SIZE);

    const int result =
#if HIGH_QUALITY_AUDIO
      opus_decode_float(
        state->decoder,
        (unsigned char*)compressed->buffer,
        compressed->used,
        (float*)uncompressed,
        audioLengthToFrames(state->spec.freq, OPUS_FRAMESIZE_120_MS),
        ENABLE_FEC);
#else
      opus_decode(state->decoder,
                  (unsigned char*)compressed->buffer,
                  compressed->used,
                  (opus_int16*)uncompressed,
                  audioLengthToFrames(state->spec.freq, OPUS_FRAMESIZE_120_MS),
                  ENABLE_FEC);
#endif

    if (result < 0) {
        fprintf(stderr, "Failed to decode audio: %s\n", opus_strerror(result));
        goto end;
    }

    const int byteSize = result * state->spec.channels * PCM_SIZE;
    SDL_QueueAudio(state->deviceId, uncompressed, byteSize);

end:
    currentAllocator->free(uncompressed);
}

void
sendAudioPackets(OpusEncoder* encoder,
                 pBytes audio,
                 const SDL_AudioSpec spec,
                 ENetPeer* peer)
{
    uint8_t buffer[KB(2)] = { 0 };
    Bytes converted = { .allocator = currentAllocator,
                        .buffer = buffer,
                        .size = sizeof(buffer),
                        .used = 0 };
    const int minDuration =
      audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_10_MS);

    while (!uint8_tListIsEmpty(audio)) {
        int frame_size = audio->used / (spec.channels * PCM_SIZE);

        if (frame_size < minDuration) {
            break;
        }

        // Only certain durations are valid for encoding
        frame_size = closestValidFrameCount(spec.freq, frame_size);

        const int result =
#if HIGH_QUALITY_AUDIO
          opus_encode_float(encoder,
                            (float*)audio->buffer,
                            frame_size,
                            converted.buffer,
                            converted.size);
#else
          opus_encode(encoder,
                      (opus_int16*)audio->buffer,
                      frame_size,
                      converted.buffer,
                      converted.size);
#endif
        if (result < 0) {
            fprintf(stderr,
                    "Failed to encode recorded packet: %s; Frame size %d\n",
                    opus_strerror(result),
                    frame_size);
            break;
        }

        converted.used = (uint32_t)result;

        const size_t bytesUsed = (frame_size * spec.channels * PCM_SIZE);
        uint8_tListQuickRemove(audio, 0, bytesUsed);

        sendBytes(peer, 1, peer->mtu, CLIENT_CHANNEL, &converted, false);
    }
}

bool
startRecording(struct pollfd inputfd, pBytes bytes, pAudioState);

void
selectAudioStreamSource(struct pollfd inputfd, pBytes bytes)
{
    askQuestion("Select audio source to stream from");
    for (AudioStreamSource i = 0; i < AudioStreamSource_Length; ++i) {
        printf("%d) %s\n", i + 1, AudioStreamSourceToCharString(i));
    }

    uint32_t selected;
    if (getIndexFromUser(
          inputfd, bytes, AudioStreamSource_Length, &selected, true) !=
        UserInputResult_Input) {
        puts("Canceled audio streaming seleciton");
        return;
    }

    AudioState state = { 0 };
    switch (selected) {
        case AudioStreamSource_File:
            askQuestion("Enter file to stream");
            while (!appDone) {
                switch (getUserInput(inputfd, bytes, NULL, CLIENT_POLL_WAIT)) {
                    case UserInputResult_Input: {
                        SDL_Event e = { 0 };
                        e.type = SDL_DROPFILE;
                        char* c = SDL_malloc(bytes->used + 1);
                        memcpy(c, bytes->buffer, bytes->used);
                        c[bytes->used] = '\0';
                        e.drop.file = c;
                        e.drop.timestamp = SDL_GetTicks();
                        SDL_PushEvent(&e);
                    } break;
                    case UserInputResult_NoInput:
                        continue;
                    default:
                        puts("Canceled audio streaming file");
                        goto end;
                }
            }
            break;
        case AudioStreamSource_Microphone:
            if (!startRecording(inputfd, bytes, &state)) {
                goto end;
            }
            break;
        case AudioStreamSource_Window:
            fprintf(stderr, "Window audio streaming not implemented\n");
            goto end;
        default:
            fprintf(stderr, "Unknown option: %d\n", selected);
            goto end;
    }
end:
    AudioStateFree(&state);
}

bool
startRecording(struct pollfd inputfd, pBytes bytes, pAudioState state)
{
    const int devices = SDL_GetNumAudioDevices(SDL_TRUE);
    if (devices == 0) {
        fprintf(stderr, "No recording devices found to send audio\n");
        return false;
    }

    state->isRecording = SDL_TRUE;
    askQuestion("Select a device to record from");
    for (int i = 0; i < devices; ++i) {
        printf("%d) %s\n", i + 1, SDL_GetAudioDeviceName(i, SDL_TRUE));
    }

    uint32_t selected;
    if (getIndexFromUser(inputfd, bytes, devices, &selected, true) !=
        UserInputResult_Input) {
        puts("Canceled recording selection");
        return false;
    }
    const SDL_AudioSpec desiredRecordingSpec = makeAudioSpec(NULL, NULL);
    state->deviceId =
      SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selected, SDL_TRUE),
                          SDL_TRUE,
                          &desiredRecordingSpec,
                          &state->spec,
                          0);
    if (state->deviceId == 0) {
        fprintf(stderr, "Failed to start recording: %s\n", SDL_GetError());
        return false;
    }
    puts("Recording audio specification");
    printAudioSpec(&state->spec);

    printf("Opened recording audio device: %u\n", state->deviceId);

    const int size = opus_encoder_get_size(state->spec.channels);
    state->encoder = currentAllocator->allocate(size);
    const int error = opus_encoder_init(state->encoder,
                                        state->spec.freq,
                                        state->spec.channels,
                                        OPUS_APPLICATION_VOIP);
    if (error < 0) {
        fprintf(
          stderr, "Failed to create voice encoder: %s\n", opus_strerror(error));
        return false;
    }
#if _DEBUG
    printf("Encoder: %p (%d)\n", state->encoder, size);
#endif

    return true;
}

bool
startPlayback(struct pollfd inputfd, pBytes bytes, pAudioState state)
{
    const int devices = SDL_GetNumAudioDevices(SDL_FALSE);
    if (devices == 0) {
        fprintf(stderr, "No playback devices found to play audio\n");
        return false;
    }

    askQuestion("Select a audio device to play audio from");
    state->isRecording = SDL_FALSE;
    state->packetLoss = 0u;
    for (int i = 0; i < devices; ++i) {
        printf("%d) %s\n", i + 1, SDL_GetAudioDeviceName(i, SDL_FALSE));
    }

    uint32_t selected;
    if (getIndexFromUser(inputfd, bytes, devices, &selected, true) !=
        UserInputResult_Input) {
        puts("Playback canceled");
        return false;
    }

    const SDL_AudioSpec desiredRecordingSpec = makeAudioSpec(NULL, NULL);
    state->deviceId =
      SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selected, SDL_FALSE),
                          SDL_FALSE,
                          &desiredRecordingSpec,
                          &state->spec,
                          0);
    if (state->deviceId == 0) {
        fprintf(stderr, "Failed to start playback: %s\n", SDL_GetError());
        return false;
    }
    puts("Playback audio specification");
    printAudioSpec(&state->spec);

    printf("Opened playback audio device: %u\n", state->deviceId);

    const int size = opus_decoder_get_size(state->spec.channels);
    state->decoder = currentAllocator->allocate(size);
    const int error =
      opus_decoder_init(state->decoder, state->spec.freq, state->spec.channels);
    if (error < 0) {
        fprintf(
          stderr, "Failed to create voice decoder: %s\n", opus_strerror(error));
        return false;
    }
#if _DEBUG
    printf("Decoder: %p (%d)\n", state->decoder, size);
#endif

    return true;
}

void
selectStreamToStart(struct pollfd inputfd, pBytes bytes, ENetPeer* peer)
{
    askQuestion("Select the type of stream to create");
    for (uint32_t i = 0; i < StreamType_Length; ++i) {
        printf("%u) %s\n", i + 1U, StreamTypeToCharString(i));
    }
    puts("");

    uint32_t index;
    if (getIndexFromUser(inputfd, bytes, StreamType_Length, &index, true) !=
        UserInputResult_Input) {
        puts("Canceling start stream");
        return;
    }

    LobbyMessage message = { 0 };
    message.tag = LobbyMessageTag_startStreaming;
    message.startStreaming.type = (StreamType)index;

    askQuestion("What's the name of the stream?");
    if (getUserInput(inputfd, bytes, NULL, -1) != UserInputResult_Input) {
        puts("Canceling start stream");
        return;
    }
    puts("");

    message.startStreaming.name =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);

    askQuestion("Do want the stream to be recorded on the server (y or n)?");
    while (!appDone) {
        if (getUserInput(inputfd, bytes, NULL, -1) != UserInputResult_Input) {
            goto write_Y_N_error;
        }
        switch ((char)bytes->buffer[0]) {
            case 'y':
                message.startStreaming.record = true;
                break;
            case 'n':
                message.startStreaming.record = false;
                break;
            default:
                goto write_Y_N_error;
        }
        break;
    write_Y_N_error:
        puts("Enter y or n");
    }

    MESSAGE_SERIALIZE(LobbyMessage, message, (*bytes));
    printf("\nCreating '%s' stream named '%s'...\n",
           StreamTypeToCharString(message.startStreaming.type),
           message.startStreaming.name.buffer);
    sendBytes(peer, 1, peer->mtu, CLIENT_CHANNEL, bytes, true);
    TemLangStringListAppend(&clientData.ownStreams,
                            &message.startStreaming.name);
    LobbyMessageFree(&message);
}

void
selectStreamToStop(struct pollfd inputfd, pBytes bytes)
{
    const StreamDisplayList* list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("No streams to stop");
        return;
    }

    askQuestion("Select a stream to stop");
    for (uint32_t i = 0; i < clientData.ownStreams.used; ++i) {
        const Stream* stream = NULL;
        if (GetStreamFromName(&clientData.allStreams,
                              &clientData.ownStreams.buffer[i],
                              &stream,
                              NULL)) {
            printf("%u) ", i + 1U);
            printStream(stream);
        }
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1U) {
        puts("Stopping to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceled stopping stream");
        goto end;
    }

end:
    return;
}

void
selectStreamToSendTextTo(struct pollfd inputfd, pBytes bytes)
{
    const StreamDisplayList* list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("No streams to send text to");
        return;
    }

    askQuestion("Send text to which stream?");
    for (uint32_t i = 0; i < clientData.displays.used; ++i) {
        const Stream* stream = &clientData.displays.buffer[i].stream;
        printf("%u) ", i + 1U);
        printStream(stream);
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1) {
        puts("Sending text to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling text send");
        goto end;
    }

end:
    return;
}

void
selectStreamToUploadFileTo(struct pollfd inputfd, pBytes bytes)
{
    const StreamDisplayList* list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("No streams to upload file to");
        return;
    }

    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;

    askQuestion("Send file to which stream?");
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        const StreamDisplay* display = &clientData.displays.buffer[i];
        printf("%zu) ", i + 1U);
        printStream(&display->stream);
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1U) {
        puts("Sending data to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    askQuestion("Enter file name");
    if (getUserInput(inputfd, bytes, NULL, -1) != UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    if (!mapFile((char*)bytes->buffer, &fd, &ptr, &size, MapFileType_Read)) {
        fprintf(stderr,
                "Error opening file '%s': %s\n",
                (char*)bytes->buffer,
                strerror(errno));
        goto end;
    }

end:
    unmapFile(fd, ptr, size);
}

void
saveScreenshot(SDL_Renderer* renderer, const StreamDisplay* display)
{
    if (renderer == NULL) {
        return;
    }

    SDL_Texture* temp = NULL;
    SDL_Surface* surface = NULL;

    if (display->texture == NULL) {
        puts("Stream display has no texture");
        goto end;
    }

    int width;
    int height;
    if (SDL_QueryTexture(display->texture, 0, 0, &width, &height) != 0) {
        fprintf(stderr, "Failed to query texture: %s\n", SDL_GetError());
        goto end;
    }
    temp = SDL_CreateTexture(renderer,
                             SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET,
                             width,
                             height);
    if (temp == NULL) {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        goto end;
    }

    if (SDL_SetTextureBlendMode(temp, SDL_BLENDMODE_NONE) != 0 ||
        SDL_SetRenderTarget(renderer, temp) != 0 ||
        SDL_RenderCopy(renderer, display->texture, NULL, NULL) != 0) {
        fprintf(stderr, "Error with texture: %s\n", SDL_GetError());
        goto end;
    }

    surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create surface: %s\n", SDL_GetError());
        goto end;
    }

    if (SDL_RenderReadPixels(renderer,
                             NULL,
                             surface->format->format,
                             surface->pixels,
                             surface->pitch) != 0) {
        fprintf(stderr, "Failed to read pixels: %s\n", SDL_GetError());
        goto end;
    }

    char buffer[1024];
    const time_t t = time(NULL);
    strftime(buffer,
             sizeof(buffer),
             "screenshot_%y_%m_%d_%H_%M_%S.png",
             localtime(&t));
    if (IMG_SavePNG(surface, buffer) != 0) {
        fprintf(stderr, "Failed to save screenshot: %s\n", IMG_GetError());
        goto end;
    }

    printf("Saved screenshot to '%s'\n", buffer);

end:
    SDL_DestroyTexture(temp);
    SDL_FreeSurface(surface);
}

void
displayUserOptions()
{
    askQuestion("Choose an option");
    for (size_t i = 0; i < ClientCommand_Length; ++i) {
        TemLangString s = ClientCommandToString(i);
        camelCaseToNormal(&s);
        printf("%zu) %s\n", i + 1UL, s.buffer);
        TemLangStringFree(&s);
    }
    puts("");
}

void
checkUserInput(SDL_Renderer* renderer, pBytes bytes, ENetPeer* peer)
{
    struct pollfd inputfd = { .events = POLLIN,
                              .revents = 0,
                              .fd = STDIN_FILENO };
    uint32_t index = 0;
    switch (
      getIndexFromUser(inputfd, bytes, ClientCommand_Length, &index, false)) {
        case UserInputResult_Input:
            break;
        case UserInputResult_Error:
            displayUserOptions();
            return;
        default:
            return;
    }

    SDL_LockMutex(clientData.mutex);
    switch (index) {
        case ClientCommand_PrintName:
            printf("Client name: %s\n", clientData.name.buffer);
            break;
        case ClientCommand_Quit:
            appDone = true;
            break;
        case ClientCommand_StartStreaming:
            selectStreamToStart(inputfd, bytes, peer);
            break;
        case ClientCommand_StopStreaming:
            selectStreamToStop(inputfd, bytes);
            break;
        case ClientCommand_ConnectToStream:
            selectAStreamToConnectTo(inputfd, bytes);
            break;
        case ClientCommand_DisconnectFromStream:
            selectAStreamToDisconnectFrom(inputfd, bytes);
            break;
        case ClientCommand_ShowAllStreams:
            askQuestion("All Streams");
            for (size_t i = 0; i < clientData.allStreams.used; ++i) {
                printStream(&clientData.allStreams.buffer[i]);
            }
            break;
        case ClientCommand_ShowOwnStreams: {
            askQuestion("Own Streams");
            const Stream* stream = NULL;
            for (size_t i = 0; i < clientData.ownStreams.used; ++i) {
                if (GetStreamFromName(&clientData.allStreams,
                                      &clientData.ownStreams.buffer[i],
                                      &stream,
                                      NULL)) {
                    printStream(stream);
                }
            }
        } break;
        case ClientCommand_SaveScreenshot: {
            const StreamDisplayList* list = &clientData.displays;
            if (StreamDisplayListIsEmpty(list)) {
                puts("No streams to take screenshot from");
                break;
            }
            askQuestion("Select stream to take screenshot from");
            for (size_t i = 0; i < clientData.displays.used; ++i) {
                const StreamDisplay* display = &list->buffer[i];
                printf("%zu) ", i + 1U);
                printStream(&display->stream);
            }
            puts("");

            uint32_t i = 0;
            if (list->used == 1) {
                puts("Taking screenshot from only available stream");
            } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
                       UserInputResult_Input) {
                puts("Canceling screenshot");
                break;
            }
            saveScreenshot(renderer, &list->buffer[i]);
        } break;
        case ClientCommand_UploadFile:
            selectStreamToUploadFileTo(inputfd, bytes);
            break;
        case ClientCommand_UploadText:
            selectStreamToSendTextTo(inputfd, bytes);
            break;
        default:
            fprintf(stderr,
                    "Command '%s' is not implemented\n",
                    ClientCommandToCharString(index));
            break;
    }
    SDL_UnlockMutex(clientData.mutex);
}

void
updateTextDisplay(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  pStreamDisplay display,
                  const TemLangString* string)
{
    if (renderer == NULL) {
        return;
    }
    SDL_Surface* surface = NULL;

    display->data.tag = StreamDisplayDataTag_none;
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }
    if (string != NULL) {
        TemLangStringCopy(&display->data.text, string, currentAllocator);
    }

    if (TemLangStringIsEmpty(&display->data.text)) {
        goto end;
    }

    SDL_Color fg = { 0 };
    fg.r = fg.g = fg.b = fg.a = 0xffu;
    SDL_Color bg = { 0 };
    bg.a = 128u;
    surface = TTF_RenderUTF8_Shaded_Wrapped(
      ttfFont, display->data.text.buffer, fg, bg, 0);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create text surface: %s\n", TTF_GetError());
        goto end;
    }

    display->srcRect.tag = OptionalRectTag_none;
    display->srcRect.none = NULL;

    // display->dstRect.x = 0.f;
    // display->dstRect.y = 0.f;
    display->dstRect.w = surface->w;
    display->dstRect.h = surface->h;

    display->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (display->texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
    }

end:
    SDL_FreeSurface(surface);
}

void
updateImageDisplay(SDL_Renderer* renderer,
                   pStreamDisplay display,
                   const Bytes* bytes)
{
    if (renderer == NULL) {
        return;
    }

    SDL_Surface* surface = NULL;
    if (uint8_tListIsEmpty(bytes)) {
        goto end;
    }

    display->data.tag = StreamDisplayDataTag_none;
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(bytes->buffer, bytes->used);
    if (rw == NULL) {
        fprintf(stderr, "Failed to load image memory: %s\n", SDL_GetError());
        goto end;
    }
    surface = IMG_Load_RW(rw, 0);
    if (surface == NULL) {
        fprintf(stderr, "Failed to load image: %s\n", SDL_GetError());
        goto end;
    }

    display->srcRect.tag = OptionalRectTag_none;
    display->srcRect.none = NULL;

    // display->dstRect.x = 0.f;
    // display->dstRect.y = 0.f;
    display->dstRect.w = surface->w;
    display->dstRect.h = surface->h;

    display->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (display->texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
    }
end:
    SDL_FreeSurface(surface);
}

SDL_Rect
renderText(SDL_Renderer* renderer,
           TTF_Font* ttfFont,
           const char* text,
           const int x,
           const int y,
           const SDL_Color fg,
           const SDL_Color bg,
           const uint32_t wrapped)
{
    SDL_Rect rect = { 0 };
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    if (renderer == NULL) {
        goto end;
    }

    surface = TTF_RenderUTF8_Shaded_Wrapped(ttfFont, text, fg, bg, wrapped);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create text surface: %s\n", TTF_GetError());
        goto end;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
        goto end;
    }
    SDL_SetTextureColorMod(texture, 0xffu, 0xffu, 0xffu);
    SDL_SetTextureAlphaMod(texture, 0xffu);

    rect = (SDL_Rect){ .x = x, .y = y, .w = surface->w, .h = surface->h };
    if (SDL_RenderCopy(renderer, texture, NULL, &rect) != 0) {
        fprintf(stderr, "Failed to render text surface: %s\n", SDL_GetError());
        goto end;
    }

end:
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return rect;
}

void
updateChatDisplay(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  const uint32_t w,
                  const uint32_t h,
                  pStreamDisplay display)
{
    if (renderer == NULL) {
        return;
    }
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }

    const StreamDisplayChat* chat = &display->data.chat;
    if (ChatListIsEmpty(&chat->logs)) {
        return;
    }

    display->texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (display->texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
        return;
    }
    SDL_SetTextureBlendMode(display->texture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(renderer, display->texture);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 128u);
    SDL_RenderClear(renderer);

    // SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 0xffu);

    display->data.tag = StreamDisplayDataTag_chat;
    char buffer[128] = { 0 };

    const SDL_Color white = { .r = 0xffu, .g = 0xffu, .b = 0xffu, .a = 0xffu };
    const SDL_Color purple = { .r = 0xffu, .g = 0x0u, .b = 0xffu, .a = 0xffu };
    const SDL_Color yellow = { .r = 0xffu, .g = 0xffu, .b = 0x0u, .a = 0xffu };
    const SDL_Color bg = { .r = 0u, .g = 0u, .b = 0u, .a = 0u };
    const SDL_Color black = { .r = 0u, .g = 0u, .b = 0u, .a = 0xffu };

    SDL_Rect rect = { 0 };
    float maxW = 0.f;
    uint32_t y = 0;
    uint32_t offset = chat->offset;
    if (chat->logs.used >= chat->count) {
        offset = SDL_clamp(chat->offset, 0U, chat->logs.used - chat->count);
    } else {
        offset = 0;
    }

    for (uint32_t j = offset, n = offset + chat->count;
         y < h && j < n && j < chat->logs.used;
         ++j) {
        const Chat* cm = &chat->logs.buffer[j];
        const time_t t = (time_t)cm->timestamp;

        strftime(buffer, sizeof(buffer), "%c", localtime(&t));
        rect = renderText(renderer, ttfFont, buffer, 0, y, purple, black, w);
        maxW = SDL_max(maxW, rect.w);

        const float oldW = rect.w;
        rect = renderText(
          renderer, ttfFont, cm->author.buffer, rect.w, y, yellow, black, w);
        maxW = SDL_max(maxW, oldW + rect.w);
        y += rect.h;

        rect =
          renderText(renderer, ttfFont, cm->message.buffer, 0, y, white, bg, w);
        maxW = SDL_max(maxW, rect.w);
        y += rect.h;
    }

    display->srcRect.tag = OptionalRectTag_rect;
    display->srcRect.rect.x = 0;
    display->srcRect.rect.y = 0;
    display->srcRect.rect.w = maxW;
    display->srcRect.rect.h = y;

    display->dstRect.w = maxW;
    display->dstRect.h = y;
}

#define DEFAULT_CHAT_COUNT 5

void
updateChatDisplayFromList(SDL_Renderer* renderer,
                          TTF_Font* ttfFont,
                          const uint32_t w,
                          const uint32_t h,
                          pStreamDisplay display,
                          const ChatList* list)
{
    if (renderer == NULL) {
        return;
    }

    if (display->data.chat.logs.allocator == NULL) {
        display->data.chat.count = DEFAULT_CHAT_COUNT;
    }
    ChatListCopy(&display->data.chat.logs, list, currentAllocator);
    display->data.chat.offset = list->used;
#if _DEBUG
    printf("Got %u chat messages\n", list->used);
#endif
    updateChatDisplay(renderer, ttfFont, w, h, display);
}

void
updateChatDisplayFromMessage(SDL_Renderer* renderer,
                             TTF_Font* ttfFont,
                             const uint32_t w,
                             const uint32_t h,
                             pStreamDisplay display,
                             const Chat* message)
{
    if (renderer == NULL) {
        return;
    }

    pStreamDisplayChat chat = &display->data.chat;
    pChatList list = &chat->logs;
    if (list->allocator == NULL) {
        list->allocator = currentAllocator;
        chat->count = DEFAULT_CHAT_COUNT;
    }
    ChatListAppend(list, message);
    if (list->used > chat->count &&
        display->data.chat.offset >= list->used - chat->count) {
        display->data.chat.offset = list->used - 1U;
    }
    const FRect rect = display->dstRect;
    updateChatDisplay(renderer, ttfFont, w, h, display);
    if (list->used > 2) {
        display->dstRect = rect;
    }
}

void
updateAllDisplays(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  const int w,
                  const int h)
{
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        switch (display->data.tag) {
            case StreamDisplayDataTag_chat:
                updateChatDisplay(renderer, ttfFont, w, h, display);
                break;
            case StreamDisplayDataTag_text:
                updateTextDisplay(renderer, ttfFont, display, NULL);
                break;
            default:
                break;
        }
    }
}

void
drawTextures(SDL_Renderer* renderer,
             const size_t target,
             const float maxX,
             const float maxY)
{
    if (renderer == NULL) {
        return;
    }
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0x33u, 0x33u, 0x33u, 0xffu);
    SDL_RenderClear(renderer);

    if (target < clientData.displays.used) {
        const StreamDisplay* display = &clientData.displays.buffer[target];
        if (display->texture != NULL) {
            SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0x0u, 0xffu);
            const SDL_FRect rect =
              expandRect((const SDL_FRect*)&display->dstRect, 1.025f, 1.025f);
            SDL_RenderFillRectF(renderer, &rect);
        }
    }
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        SDL_Texture* texture = display->texture;
        if (texture == NULL) {
            continue;
        }
        SDL_FRect* rect = (SDL_FRect*)&display->dstRect;
        rect->x = SDL_clamp(rect->x, 0, maxX);
        rect->y = SDL_clamp(rect->y, 0, maxY);
        SDL_SetTextureColorMod(texture, 0xffu, 0xffu, 0xffu);
        SDL_SetTextureAlphaMod(texture, 0xffu);
        int result;
        switch (display->srcRect.tag) {
            case OptionalRectTag_rect: {
                SDL_Rect r = { 0 };
                r.x = display->srcRect.rect.x;
                r.y = display->srcRect.rect.y;
                r.w = display->srcRect.rect.w;
                r.h = display->srcRect.rect.h;
                result = SDL_RenderCopyF(renderer,
                                         texture,
                                         &r,
                                         //   NULL,
                                         rect);
            } break;
            default:
                result = SDL_RenderCopyF(renderer, texture, NULL, rect);
                break;
        }
        if (result != 0) {
            fprintf(stderr, "Failed to render: %s\n", SDL_GetError());
        }
    }

    // uint8_t background[4] = { 0xff, 0xffu, 0x0u, 0xffu };
    // renderFont(renderer, font, "Hello World", 0, 0, 5.f, NULL,
    // background);
    SDL_RenderPresent(renderer);
}

void
printRenderInfo(const SDL_RendererInfo* info)
{
    puts(info->name);
    for (size_t i = 0; i < info->num_texture_formats; ++i) {
        printf(
          "%zu) %s\n", i + 1, SDL_GetPixelFormatName(info->texture_formats[i]));
    }
}

bool
findDisplayFromPoint(const SDL_FPoint* point, size_t* targetDisplay)
{
    for (int64_t i = (int64_t)clientData.displays.used - 1LL; i >= 0LL; --i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        if (display->texture == NULL) {
            continue;
        }
        if (!SDL_PointInFRect(point, (const SDL_FRect*)&display->dstRect)) {
            continue;
        }
        *targetDisplay = (size_t)i;
        return true;
    }
    return false;
}

bool
loadNewFont(const char* filename, const int fontSize, TTF_Font** ttfFont)
{
    TTF_Font* newFont = TTF_OpenFont(filename, fontSize);
    if (newFont == NULL) {
        fprintf(stderr, "Failed to load new font: %s\n", TTF_GetError());
        return false;
    }
    TTF_CloseFont(*ttfFont);
    *ttfFont = newFont;
    return true;
}

bool
checkForMessagesFromLobby(ENetHost* host, ENetEvent* event, pClient client)
{
    bool result = false;
    while (enet_host_service(host, event, 0U) >= 0) {
        switch (event->type) {
            case ENET_EVENT_TYPE_CONNECT:
                puts("Unexpected connect event from server");
                appDone = true;
                goto end;
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("Disconnected from server");
                appDone = true;
                goto end;
            case ENET_EVENT_TYPE_NONE:
                result = true;
                goto end;
            case ENET_EVENT_TYPE_RECEIVE: {
                result = true;
                const Bytes temp = { .allocator = currentAllocator,
                                     .buffer = event->packet->data,
                                     .size = event->packet->dataLength,
                                     .used = event->packet->dataLength };
                Payload payload = { 0 };
                MESSAGE_DESERIALIZE(Payload, payload, temp);

                LobbyMessage message = { 0 };
                switch (parsePayload(&payload, client)) {
                    case PayloadParseResult_UsePayload:
                        MESSAGE_DESERIALIZE(
                          LobbyMessage, message, payload.fullData);
                        break;
                    case PayloadParseResult_Done:
                        MESSAGE_DESERIALIZE(
                          LobbyMessage, message, client->payload);
                        break;
                    default:
                        goto fend;
                }

                IN_MUTEX(clientData.mutex, f, {
                    switch (message.tag) {
                        case LobbyMessageTag_allStreamsAck:
                            result = StreamListCopy(&clientData.allStreams,
                                                    &message.allStreamsAck,
                                                    currentAllocator);
                            goto f;
                        case LobbyMessageTag_general:
                            switch (message.general.tag) {
                                case GeneralMessageTag_authenticateAck:
                                    result = TemLangStringCopy(
                                      &client->name,
                                      &message.general.authenticateAck,
                                      currentAllocator);
                                    goto f;
                                default:
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                    printf("Unexpected message from lobby server: %s\n",
                           LobbyMessageTagToCharString(message.tag));
                });

            fend:
                enet_packet_destroy(event->packet);
                PayloadFree(&payload);
                LobbyMessageFree(&message);
            } break;
            default:
                break;
        }
    }
end:
    return result;
}

int
displayError(SDL_Window* window, const char* e, const bool force)
{
    if (window != NULL || force) {
        return SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, "SDL error", e, window);
    } else {
        return 0;
    }
}

int
runClient(const int argc, const char** argv, pConfiguration configuration)
{
    if (!parseClientConfiguration(argc, argv, configuration)) {
        return EXIT_FAILURE;
    }
    clientData.authentication = &configuration->data.client.authentication;

    int result = EXIT_FAILURE;
    puts("Running client");
    printConfiguration(configuration);

    const ClientConfiguration* config = &configuration->data.client;

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    // Font font = { 0 };
    TTF_Font* ttfFont = NULL;
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };

    clientData.displays.allocator = currentAllocator;
    clientData.ownStreams.allocator = currentAllocator;
    clientData.allStreams.allocator = currentAllocator;
    const bool showWindow = !config->noGui;
    {
        uint32_t flags = 0;
        if (showWindow) {
            flags |= SDL_INIT_VIDEO;
        }
        if (!config->noAudio) {
            flags |= SDL_INIT_AUDIO;
        }
        if (SDL_Init(flags) != 0) {
            fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
    }

    clientData.mutex = SDL_CreateMutex();
    if (clientData.mutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
    }

    if (showWindow) {
        const uint32_t flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
        if (IMG_Init(flags) != flags) {
            fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
        if (TTF_Init() == -1) {
            fprintf(stderr, "Failed to init TTF: %s\n", TTF_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
    }

    if (showWindow) {
        window = SDL_CreateWindow(
          "TemStream Client",
          SDL_WINDOWPOS_UNDEFINED,
          SDL_WINDOWPOS_UNDEFINED,
          config->windowWidth,
          config->windowHeight,
          SDL_WINDOW_RESIZABLE |
            (config->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
        if (window == NULL) {
            fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }

        {
            SDL_RendererInfo info = { 0 };
#if _DEBUG
            int drivers = SDL_GetNumRenderDrivers();
            for (int i = 0; i < drivers; ++i) {
                SDL_GetRenderDriverInfo(i, &info);
                printRenderInfo(&info);
            }
#endif

            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (renderer == NULL) {
                fprintf(
                  stderr, "Failed to create renderer: %s\n", SDL_GetError());
                displayError(window, "Failed to start", showWindow);
                goto end;
            }

            SDL_GetRendererInfo(renderer, &info);
            printf("Current renderer: %s\n", info.name);
        }

        // if (!loadFont(config->ttfFile.buffer, config->fontSize, renderer,
        // &font))
        // {
        //     fprintf(stderr, "Failed to load font\n");
        //     goto end;
        // }

        ttfFont = TTF_OpenFont(config->ttfFile.buffer, config->fontSize);
        if (ttfFont == NULL) {
            fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
    }

    appDone = false;

    SDL_AtomicSet(&runningThreads, 0);

    SDL_Event e = { 0 };

    size_t targetDisplay = UINT32_MAX;
    MoveMode moveMode = MoveMode_None;
    bool hasTarget = false;

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    Client client = { 0 };

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        appDone = true;
        goto end;
    }
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, configuration->address.ip.buffer);
        char* end = NULL;
        address.port =
          (uint16_t)strtoul(configuration->address.port.buffer, &end, 10);
        peer = enet_host_connect(host, &address, 2, StreamType_Lobby);
        char buffer[512] = { 0 };
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));
        printf("Connecting to server: %s:%u...\n", buffer, address.port);
    }
    if (peer == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        appDone = true;
        goto end;
    }

    ENetEvent event = { 0 };
    if (enet_host_service(host, &event, 5000U) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT &&
        event.data == StreamType_Lobby) {
        char buffer[KB(1)] = { 0 };
        enet_address_get_host_ip(&event.peer->address, buffer, sizeof(buffer));
        printf(
          "Connected to server: %s:%u\n", buffer, event.peer->address.port);
        displayUserOptions();
        sendAuthentication(peer, StreamType_Lobby);
    } else {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            fprintf(stderr,
                    "Failed to connect to server. Got %u when expecting %u\n",
                    event.data,
                    StreamType_Lobby);
        } else {
            fprintf(stderr, "Failed to connect to server\n");
        }
        enet_peer_reset(peer);
        peer = NULL;
        appDone = true;
        goto end;
    }

    while (!appDone) {
        if (!checkForMessagesFromLobby(host, &event, &client)) {
            appDone = true;
            break;
        }
        checkUserInput(renderer, &bytes, peer);
        while (SDL_PollEvent(&e)) {
            SDL_LockMutex(clientData.mutex);
            switch (e.type) {
                case SDL_QUIT:
                    appDone = true;
                    break;
                case SDL_WINDOWEVENT:
                    renderDisplays();
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_F1:
                        case SDLK_ESCAPE:
                            displayUserOptions();
                            break;
                        case SDLK_F2:
                            printf("Memory: %zu / %zu\n",
                                   currentAllocator->used(),
                                   currentAllocator->totalSize());
                            break;
                        case SDLK_F3:
                            if (hasTarget) {
                                printf("Target display: %zu\n", targetDisplay);
                            } else {
                                puts("No target display");
                            }
                            break;
                        case SDLK_v:
                            if ((e.key.keysym.mod & KMOD_CTRL) == 0) {
                                break;
                            }
                            e.type = SDL_DROPTEXT;
                            e.drop.file = SDL_GetClipboardText();
                            e.drop.timestamp = SDL_GetTicks64();
                            e.drop.windowID = SDL_GetWindowID(window);
                            SDL_PushEvent(&e);
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT:
                            moveMode = MoveMode_Position;
                            break;
                        case SDL_BUTTON_RIGHT:
                        case SDL_BUTTON_MIDDLE:
                            moveMode = MoveMode_Size;
                            break;
                        default:
                            moveMode = MoveMode_None;
                            break;
                    }
                    SDL_FPoint point = { 0 };
                    point.x = e.button.x;
                    point.y = e.button.y;
                    if (findDisplayFromPoint(&point, &targetDisplay)) {
                        hasTarget = true;
                        SDL_SetWindowGrab(window, true);
                        renderDisplays();
                    }
                } break;
                case SDL_MOUSEBUTTONUP:
                    hasTarget = false;
                    targetDisplay = UINT32_MAX;
                    SDL_SetWindowGrab(window, false);
                    renderDisplays();
                    break;
                case SDL_MOUSEMOTION:
                    if (hasTarget) {
                        if (targetDisplay >= clientData.displays.used) {
                            break;
                        }
                        pStreamDisplay display =
                          &clientData.displays.buffer[targetDisplay];
                        if (display->texture == NULL) {
                            break;
                        }
                        int w;
                        int h;
                        SDL_GetWindowSize(window, &w, &h);
                        switch (moveMode) {
                            case MoveMode_Position:
                                display->dstRect.x =
                                  display->dstRect.x + e.motion.xrel;
                                display->dstRect.y =
                                  display->dstRect.y + e.motion.yrel;
                                break;
                            case MoveMode_Size:
                                display->dstRect.w =
                                  SDL_clamp(display->dstRect.w + e.motion.xrel,
                                            MIN_WIDTH,
                                            w);
                                display->dstRect.h =
                                  SDL_clamp(display->dstRect.h + e.motion.yrel,
                                            MIN_HEIGHT,
                                            h);
                                break;
                            default:
                                break;
                        }
                        display->dstRect.x = SDL_clamp(
                          display->dstRect.x, 0.f, w - display->dstRect.w);
                        display->dstRect.y = SDL_clamp(
                          display->dstRect.y, 0.f, h - display->dstRect.h);
                        renderDisplays();
                    } else {
                        SDL_FPoint point = { 0 };
                        int x;
                        int y;
                        SDL_GetMouseState(&x, &y);
                        point.x = (float)x;
                        point.y = (float)y;
                        targetDisplay = UINT_MAX;
                        findDisplayFromPoint(&point, &targetDisplay);
                        renderDisplays();
                    }
                    break;
                case SDL_MOUSEWHEEL: {
                    if (targetDisplay >= clientData.displays.used) {
                        break;
                    }
                    int w;
                    int h;
                    SDL_GetWindowSize(window, &w, &h);
                    pStreamDisplay display =
                      &clientData.displays.buffer[targetDisplay];
                    switch (display->data.tag) {
                        case StreamDisplayDataTag_chat: {
                            pStreamDisplayChat chat = &display->data.chat;
                            int64_t offset = (int64_t)chat->offset;

                            // printf("Before: %" PRId64 "\n", offset);
                            offset += (e.wheel.y > 0) ? -1LL : 1LL;
                            offset = SDL_clamp(
                              offset, 0LL, (int64_t)chat->logs.used - 1LL);
                            chat->offset = (uint32_t)offset;
                            // printf("After: %u\n", chat->offset);
                            // printf("Logs %u\n", chat->logs.used);

                            const float width = display->dstRect.w;
                            const float height = display->dstRect.h;
                            updateChatDisplay(renderer, ttfFont, w, h, display);
                            display->dstRect.w = width;
                            display->dstRect.h = height;
                        } break;
                        default:
                            break;
                    }
                    renderDisplays();
                } break;
                case SDL_USEREVENT: {
                    int w;
                    int h;
                    SDL_GetWindowSize(window, &w, &h);
                    switch (e.user.code) {
                        case CustomEvent_Render:
                            drawTextures(renderer,
                                         targetDisplay,
                                         (float)w - 32.f,
                                         (float)h - 32.f);
                            break;
                        case CustomEvent_ShowSimpleMessage:
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                                     (char*)e.user.data1,
                                                     (char*)e.user.data2,
                                                     window);
                            break;
                        case CustomEvent_UpdateStreamDisplay:
                            renderDisplays();
                            break;
                        default:
                            break;
                    }
                } break;
                case SDL_DROPFILE: {
                    // Look for image stream
                    printf("Got dropped file: %s\n", e.drop.file);
                    int fd = -1;
                    char* ptr = NULL;
                    size_t size = 0;

                    FileExtension ext = { 0 };
                    if (!filenameToExtension(e.drop.file, &ext)) {
                        puts("Failed to find file extension");
                        goto endDropFile;
                    }
                    if (ext.tag == FileExtensionTag_font) {
                        puts("Loading new font...");
                        if (loadNewFont(
                              e.drop.file, config->fontSize, &ttfFont)) {
                            int w, h;
                            SDL_GetWindowSize(window, &w, &h);
                            updateAllDisplays(renderer, ttfFont, w, h);
                            puts("Loaded new font");
                        }
                        goto endDropFile;
                    }

                    if (!mapFile(
                          e.drop.file, &fd, &ptr, &size, MapFileType_Read)) {
                        fprintf(stderr,
                                "Error opening file '%s': %s\n",
                                e.drop.file,
                                strerror(errno));
                        goto endDropFile;
                    }

                    Bytes fileBytes = { .allocator = NULL,
                                        .buffer = (uint8_t*)ptr,
                                        .size = size,
                                        .used = size };
                    const Stream* stream = NULL;
                    if (targetDisplay >= clientData.displays.used) {
                        if (!GetStreamFromType(
                              &clientData.allStreams,
                              FileExtenstionToStreamType(ext.tag),
                              &stream,
                              NULL)) {
                            puts("No stream to send data too...");
                            goto endDropFile;
                        }
                    } else {
                        const StreamDisplay* display =
                          &clientData.displays.buffer[targetDisplay];
                        if (!GetStreamFromName(&clientData.allStreams,
                                               &display->stream.name,
                                               &stream,
                                               NULL)) {
                            puts("No stream to send data too...");
                            goto endDropFile;
                        }
                    }

                    // TODO: Send bytes to stream
                    (void)fileBytes;

                    // Don't free message since it wasn't allocated
                endDropFile:
                    unmapFile(fd, ptr, size);
                    SDL_free(e.drop.file);
                } break;
                case SDL_DROPTEXT: {
                    // Look for a text or chat stream
                    puts("Got dropped text");
                    if (targetDisplay >= clientData.displays.used) {
                        puts("No stream to text too...");
                        break;
                    }
                    const StreamDisplay* display =
                      &clientData.displays.buffer[targetDisplay];
                    const Stream* stream = NULL;
                    if (!GetStreamFromName(&clientData.allStreams,
                                           &display->stream.name,
                                           &stream,
                                           NULL)) {
                        puts("No stream to text too...");
                        break;
                    }
                    SDL_free(e.drop.file);
                } break;
                default:
                    break;
            }
            SDL_UnlockMutex(clientData.mutex);
        }
        SDL_Delay(1);
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    // FontFree(&font);
    while (SDL_AtomicGet(&runningThreads) > 0) {
        SDL_Delay(1);
    }
    closeHostAndPeer(host, peer);
    ClientFree(&client);
    uint8_tListFree(&bytes);
    ClientDataFree(&clientData);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(ttfFont);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return result;
}