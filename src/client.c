#include <include/main.h>

#define MIN_WIDTH 32
#define MIN_HEIGHT 32

ClientData clientData = { 0 };

SDL_mutex* packetMutex = NULL;

pENetPacketList outgoingPackets = { 0 };

AudioRecordStatePtrList audioRecordings = { 0 };

SDL_atomic_t runningThreads = { 0 };

SDL_AudioDeviceID playbackId = 0;

Bytes audioBytes = { 0 };

void
startPlayback(const char* name);

bool
enqueuePacket(ENetPacket* packet)
{
    bool result;
    IN_MUTEX(packetMutex, end, {
        result = pENetPacketListAppend(&outgoingPackets, &packet);
    });
    return result;
}

void
renderDisplays()
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_Render;
    SDL_PushEvent(&e);
}

void
cleanupStreamDisplays()
{
#if _DEBUG
    printf("Cleaning up stream displays. Connected to %u streams. Has %u "
           "stream displays\n",
           clientData.connectedStreams.used,
           clientData.displays.used);
#endif
    if (GuidListIsEmpty(&clientData.connectedStreams)) {
        StreamDisplayListFree(&clientData.displays);
        clientData.displays.allocator = currentAllocator;
#if _DEBUG
        puts("Removed all stream displays");
#endif
    } else {
        size_t i = 0;
        size_t removed = 0;
        while (i < clientData.displays.used) {
            if (GuidListFind(&clientData.connectedStreams,
                             &clientData.displays.buffer[i].id,
                             NULL,
                             NULL)) {
                ++i;
            } else {
                StreamDisplayListSwapRemove(&clientData.displays, i);
                ++removed;
            }
        }
        printf("Removed %zu stream displays\n", removed);
    }
    renderDisplays();
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
        .ttfFile = TemLangStringCreate(
          "/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf", currentAllocator)
    };
}

bool
parseClientConfiguration(const int argc,
                         const char** argv,
                         pAllConfiguration configuration)
{
    pClientConfiguration client = &configuration->configuration.client;
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
        STR_EQUALS(key, "-N", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "--no-gui", keyLen, { goto parseNoGui; });
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
    }
    return true;
}

void
ClientFree(pClient client)
{
    GuidListFree(&client->connectedStreams);
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
                 uint32_t* index)
{
    ssize_t size = 0;
    while (!appDone) {
        switch (getUserInput(inputfd, bytes, &size, CLIENT_POLL_WAIT)) {
            case UserInputResult_Error:
                return UserInputResult_Error;
            case UserInputResult_NoInput:
                continue;
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
selectAStreamToConnectTo(struct pollfd inputfd, pBytes bytes)
{
    const uint32_t streamNum = clientData.allStreams.used;
    askQuestion("Select a stream to connect to");
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Stream* stream = &clientData.allStreams.buffer[i];
        printf("%u) %s (%s)\n",
               i + 1U,
               stream->name.buffer,
               StreamTypeToCharString(stream->type));
    }
    puts("");

    if (streamNum == 0) {
        puts("No streams to connect to");
        return;
    }

    uint32_t i = 0;
    if (streamNum == 1) {
        puts("Connecting to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streamNum, &i) !=
               UserInputResult_Input) {
        puts("Canceling connecting to stream");
        return;
    }

    Message message = { 0 };
    message.tag = MessageTag_connectToStream;
    message.connectToStream = clientData.allStreams.buffer[i].id;
    MESSAGE_SERIALIZE(message, (*bytes));
    MessageFree(&message);
    ENetPacket* packet = BytesToPacket(bytes, true);
    enqueuePacket(packet);
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd, pBytes bytes)
{
    StreamList streams = { .allocator = currentAllocator };
    const uint32_t streamNum = clientData.connectedStreams.used;
    askQuestion("Select a stream to disconnect from");
    const Stream* stream = NULL;
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.connectedStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streams.used == 0) {
        puts("Not connected to any streams");
        goto end;
    }
    uint32_t i = 0;
    if (streams.used == 1) {
        puts("Disconnecting from only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streams.used, &i) !=
               UserInputResult_Input) {
        puts("Canceled disconnecting from stream");
        goto end;
    }

    Message message = { 0 };
    message.tag = MessageTag_disconnectFromStream;
    message.disconnectFromStream = streams.buffer[i].id;
    MESSAGE_SERIALIZE(message, (*bytes));
    MessageFree(&message);
    ENetPacket* packet = BytesToPacket(bytes, true);
    enqueuePacket(packet);
end:
    StreamListFree(&streams);
}

void
audioCaptureCallback(const AudioRecordState* state, uint8_t* data, int len)
{
    const Bytes audioBytes = {
        .allocator = NULL, .buffer = data, .size = len, .used = len
    };

    Bytes bytes = { .allocator = currentAllocator };
    Message message = { 0 };
    message.tag = MessageTag_streamMessage;
    message.streamMessage.id = state->id;
    message.streamMessage.data.tag = StreamMessageDataTag_audio;
    message.streamMessage.data.audio = audioBytes;
    MESSAGE_SERIALIZE(message, bytes);
    ENetPacket* packet = BytesToPacket(&bytes, false);
    enqueuePacket(packet);
    // Don't free messages. Bytes weren't allocated
    uint8_tListFree(&bytes);
}

void
audioPlaybackCallback(void* ptr, uint8_t* audioStream, const int len)
{
    (void)ptr;
    size_t sLen = len;
    sLen = SDL_min(sLen, audioBytes.used);
    if (sLen > 0) {
        memcpy(audioStream, audioBytes.buffer, sLen);
        uint8_tListQuickRemove(&audioBytes, 0, sLen);
    } else {
        memset(audioStream, 0, len);
    }
}

int
audioRecordThread(pAudioRecordState state)
{
    SDL_PauseAudioDevice(state->deviceId, SDL_FALSE);
    while (!appDone && state->running == SDL_TRUE) {
        SDL_Delay(100);
    }
    printf("Closing audio device: %u\n", state->deviceId);
    SDL_CloseAudioDevice(state->deviceId);
    currentAllocator->free(state);
    SDL_AtomicDecRef(&runningThreads);
    return EXIT_SUCCESS;
}

void
startRecording(const Guid* guid)
{
    const int devices = SDL_GetNumAudioDevices(SDL_TRUE);
    if (devices == 0) {
        fprintf(stderr, "No recording devices found to send audio\n");
        return;
    }

    AudioRecordStatePtr state =
      currentAllocator->allocate(sizeof(AudioRecordState));
    state->id = *guid;
    SDL_MessageBoxButtonData* buttons =
      currentAllocator->allocate(devices * sizeof(SDL_MessageBoxButtonData));
    for (int i = 0; i < devices; ++i) {
        buttons[i].buttonid = i;
        buttons[i].flags = 0;
        const char* name = SDL_GetAudioDeviceName(i, SDL_TRUE);
        const size_t len = strlen(name);
        char* ptr = currentAllocator->allocate(sizeof(char) * (len + 1));
        memcpy(ptr, name, len);
        buttons[i].text = ptr;
    }

    const SDL_MessageBoxData data = { .flags = SDL_MESSAGEBOX_INFORMATION,
                                      .window = NULL,
                                      .title = "Audio recording",
                                      .message =
                                        "Select an audio device to stream",
                                      .numbuttons = devices,
                                      .buttons = buttons,
                                      .colorScheme = NULL };
    int selected;
    if (SDL_ShowMessageBox(&data, &selected) != 0) {
        fprintf(stderr, "Failed to show message box: %s\n", SDL_GetError());
        goto audioStartEnd;
    }
    const SDL_AudioSpec desiredRecordingSpec =
      makeAudioSpec((SDL_AudioCallback)audioCaptureCallback, state);
    state->deviceId =
      SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selected, SDL_TRUE),
                          SDL_TRUE,
                          &desiredRecordingSpec,
                          &state->spec,
                          SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (state->deviceId == 0) {
        fprintf(stderr, "Failed to start recording: %s\n", SDL_GetError());
        goto audioStartEnd;
    }
    puts("Recording audio specification");
    printAudioSpec(&state->spec);

    printf("Open audio device: %u\n", state->deviceId);

    state->running = SDL_TRUE;
    SDL_Thread* thread = SDL_CreateThread(
      (SDL_ThreadFunction)audioRecordThread, "audio", (void*)state);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto audioStartEnd;
    }
    SDL_AtomicIncRef(&runningThreads);
    SDL_DetachThread(thread);

    AudioRecordStatePtrListAppend(&audioRecordings, &state);
    state = NULL;
audioStartEnd:
    if (state != NULL) {
        SDL_CloseAudioDevice(state->deviceId);
        currentAllocator->free(state);
    }
    for (int i = 0; i < devices; ++i) {
        currentAllocator->free((char*)buttons[i].text);
    }
    currentAllocator->free(buttons);
}

void
selectStreamToStart(struct pollfd inputfd, pBytes bytes)
{
    askQuestion("Select the type of stream to create");
    for (uint32_t i = 0; i < StreamType_Length; ++i) {
        printf("%u) %s\n", i + 1U, StreamTypeToCharString(i));
    }
    puts("");

    uint32_t index;
    if (getIndexFromUser(inputfd, bytes, StreamType_Length, &index) !=
        UserInputResult_Input) {
        puts("Canceling start stream");
        return;
    }

    Message message = { 0 };
    message.tag = MessageTag_startStreaming;
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

    MESSAGE_SERIALIZE(message, (*bytes));
    printf("\nCreating '%s' stream named '%s'...\n",
           StreamTypeToCharString(message.startStreaming.type),
           message.startStreaming.name.buffer);
    ENetPacket* packet = BytesToPacket(bytes, true);
    enqueuePacket(packet);
    MessageFree(&message);
}

void
selectStreamToStop(struct pollfd inputfd, pBytes bytes)
{
    StreamList streams = { .allocator = currentAllocator };

    const Stream* stream = NULL;
    const uint32_t streamNum = clientData.ownStreams.used;
    askQuestion("Select a stream to stop");
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.ownStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streams.used == 0) {
        puts("No streams stop");
        goto end;
    }

    uint32_t i = 0;
    if (streams.used == 1U) {
        puts("Stopping to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streams.used, &i) !=
               UserInputResult_Input) {
        puts("Canceled stopping stream");
        goto end;
    }

    Message message = { 0 };
    message.tag = MessageTag_stopStreaming;
    message.stopStreaming = streams.buffer[i].id;

    MESSAGE_SERIALIZE(message, (*bytes));
    MessageFree(&message);

    ENetPacket* packet = BytesToPacket(bytes, true);
    enqueuePacket(packet);

end:
    StreamListFree(&streams);
}

void
selectStreamToSendTextTo(struct pollfd inputfd, pBytes bytes)
{
    StreamList streams = { .allocator = currentAllocator };
    const Stream* stream = NULL;
    const uint32_t streamNum = clientData.connectedStreams.used;
    askQuestion("Send text to which stream?");
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.connectedStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streams.used == 0) {
        puts("No streams to send text to");
        goto end;
    }

    uint32_t i = 0;
    if (streams.used == 1) {
        puts("Sending text to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streams.used, &i) !=
               UserInputResult_Input) {
        puts("Canceling text send");
        goto end;
    }

    stream = &streams.buffer[i];

    Message message = { 0 };
    switch (stream->type) {
        case StreamType_Text:
            printf("Enter text to update text stream '%s'\n",
                   stream->name.buffer);
            if (getUserInput(inputfd, bytes, NULL, -1) !=
                UserInputResult_Input) {
                puts("Canceling text send");
                goto end;
            }
            message.streamMessage.data.tag = StreamMessageDataTag_text;
            message.streamMessage.data.text =
              TemLangStringCreate((char*)bytes->buffer, currentAllocator);
            break;
        case StreamType_Chat:
            printf("Enter message for chat stream '%s'\n", stream->name.buffer);
            if (getUserInput(inputfd, bytes, NULL, -1) !=
                UserInputResult_Input) {
                puts("Canceling text send");
                goto end;
            }
            message.streamMessage.data.tag = StreamMessageDataTag_chatMessage;
            message.streamMessage.data.chatMessage.message =
              TemLangStringCreate((char*)bytes->buffer, currentAllocator);
            break;
        default:
            printf("Cannot send text to stream type '%s'\n",
                   StreamTypeToCharString(stream->type));
            goto end;
    }

    message.streamMessage.id = stream->id;
    message.tag = MessageTag_streamMessage;
    MESSAGE_SERIALIZE(message, (*bytes));
    MessageFree(&message);
    ENetPacket* packet = BytesToPacket(bytes, true);
    enqueuePacket(packet);

end:
    StreamListFree(&streams);
}

void
selectStreamToUploadFileTo(struct pollfd inputfd, pBytes bytes)
{
    StreamList streams = { .allocator = currentAllocator };
    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;

    const Stream* stream = NULL;
    const uint32_t streamNum = clientData.connectedStreams.used;
    askQuestion("Send file to which stream?");
    StreamListFree(&streams);
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.connectedStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streams.used == 0) {
        puts("No streams to send data");
        goto end;
    }

    uint32_t i = 0;
    if (streams.used == 1U) {
        puts("Sending data to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streams.used, &i) !=
               UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    stream = &streams.buffer[i];

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

    Bytes fileBytes = { 0 };
    fileBytes.size = size;
    fileBytes.used = size;
    fileBytes.buffer = (uint8_t*)ptr;

    bool doFree = true;

    Message message = { 0 };
    switch (stream->type) {
        case StreamType_Image:
            message.streamMessage.data.tag = StreamMessageDataTag_image;
            message.streamMessage.data.image = fileBytes;
            doFree = false;
            break;
        case StreamType_Video:
            message.streamMessage.data.tag = StreamMessageDataTag_video;
            message.streamMessage.data.video = fileBytes;
            doFree = false;
            break;
        case StreamType_Audio:
            message.streamMessage.data.tag = StreamMessageDataTag_audio;
            message.streamMessage.data.audio = fileBytes;
            doFree = false;
            break;
        case StreamType_Text:
            message.streamMessage.data.tag = StreamMessageDataTag_text;
            message.streamMessage.data.text =
              TemLangStringCreateFromSize(ptr, size, currentAllocator);
            break;
        case StreamType_Chat:
            message.streamMessage.data.tag = StreamMessageDataTag_chatMessage;
            message.streamMessage.data.chatMessage.message =
              TemLangStringCreateFromSize(ptr, size, currentAllocator);
            break;
        default:
            printf("Cannot send file data to stream type '%s'\n",
                   StreamTypeToCharString(stream->type));
            goto end;
    }

    message.streamMessage.id = stream->id;
    message.tag = MessageTag_streamMessage;
    MESSAGE_SERIALIZE(message, (*bytes));
    if (doFree) {
        MessageFree(&message);
    } else {
        memset(&message, 0, sizeof(message));
    }

    ENetPacket* packet = BytesToPacket(bytes, true);
    enqueuePacket(packet);
    puts("Sent file");
end:
    unmapFile(fd, ptr, size);
    StreamListFree(&streams);
}

void
saveScreenshot(SDL_Renderer* renderer, const Guid* id)
{
    if (renderer == NULL) {
        return;
    }
    const StreamDisplay* display = NULL;
    SDL_Texture* temp = NULL;
    SDL_Surface* surface = NULL;
    if (!GetStreamDisplayFromGuid(&clientData.displays, id, &display, NULL)) {
        fprintf(stderr, "Guid missing from stream list\n");
        goto end;
    }

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
handleClientCommand(const ClientCommand command, pBytes bytes);

int
userInputThread(void* ptr)
{
    (void)ptr;
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(KB(1)),
                    .size = KB(1),
                    .used = 0 };
    struct pollfd inputfd = { .events = POLLIN,
                              .revents = 0,
                              .fd = STDIN_FILENO };
    while (!appDone) {
        uint32_t index = 0;
        switch (
          getIndexFromUser(inputfd, &bytes, ClientCommand_Length, &index)) {
            case UserInputResult_Input:
                break;
            case UserInputResult_Error:
                displayUserOptions();
                continue;
            default:
                continue;
        }

        switch (index) {
            case ClientCommand_PrintName:
                printf("Client name: %s\n", clientData.name.buffer);
                break;
            case ClientCommand_GetClients: {
                Message message = { 0 };
                message.tag = MessageTag_getClients;
                message.getClients = NULL;
                MESSAGE_SERIALIZE(message, bytes);
                ENetPacket* packet = BytesToPacket(&bytes, true);
                enqueuePacket(packet);
            } break;
            case ClientCommand_Quit:
                appDone = true;
                break;
            case ClientCommand_StartStreaming:
                selectStreamToStart(inputfd, &bytes);
                break;
            case ClientCommand_SaveScreenshot: {
                StreamList streams = { .allocator = currentAllocator };
                askQuestion("Select stream to take screenshot from");
                for (size_t i = 0; i < clientData.connectedStreams.used; ++i) {
                    const Stream* stream = NULL;
                    if (!GetStreamFromGuid(
                          &clientData.allStreams,
                          &clientData.connectedStreams.buffer[i],
                          &stream,
                          NULL)) {
                        continue;
                    }
                    printf(
                      "%zu) %s\n", streams.used + 1UL, stream->name.buffer);
                    StreamListAppend(&streams, stream);
                }
                puts("");

                uint32_t i = 1;
                if (StreamListIsEmpty(&streams)) {
                    puts("No streams to take screenshot from");
                    goto saveEnd;
                } else if (streams.used == 1) {
                    puts("Taking screenshot from only available stream");
                } else if (getIndexFromUser(
                             inputfd, &bytes, streams.used, &i) !=
                           UserInputResult_Input) {
                    puts("Canceling screenshot");
                    goto saveEnd;
                }
                SDL_Event e = { 0 };
                e.type = SDL_USEREVENT;
                e.user.code = CustomEvent_SaveScreenshot;
                Guid* guid = currentAllocator->allocate(sizeof(Guid));
                *guid = streams.buffer[i].id;
                e.user.data1 = guid;
                SDL_PushEvent(&e);
            saveEnd:
                StreamListFree(&streams);
            } break;
            case ClientCommand_ChangePlaybackDevice: {
                const int devices = SDL_GetNumAudioDevices(SDL_FALSE);
                if (devices == 0) {
                    puts("No playback devices detected");
                    break;
                }
                uint32_t index = 0;
                if (devices != 1) {
                    askQuestion("Select a playback device");
                    for (int i = 0; i < devices; ++i) {
                        printf("%d) %s\n",
                               i + 1,
                               SDL_GetAudioDeviceName(i, SDL_FALSE));
                    }
                    if (getIndexFromUser(inputfd, &bytes, devices, &index) !=
                        UserInputResult_Input) {
                        puts("Cancel changing playback device");
                        break;
                    }
                }
                startPlayback(SDL_GetAudioDeviceName(index, SDL_FALSE));
            } break;
            case ClientCommand_ConnectToStream:
            case ClientCommand_ShowAllStreams:
            case ClientCommand_ShowConnectedStreams:
            case ClientCommand_ShowOwnStreams:
            case ClientCommand_DisconnectFromStream:
            case ClientCommand_StopStreaming:
            case ClientCommand_UploadFile:
            case ClientCommand_UploadText:
                handleClientCommand(index, &bytes);
                break;
            default:
                fprintf(stderr,
                        "Command '%s' is not implemented\n",
                        ClientCommandToCharString(index));
                break;
        }
    }

    uint8_tListFree(&bytes);
    return EXIT_SUCCESS;
}

void
handleClientCommand(const ClientCommand command, pBytes bytes)
{
    const struct pollfd inputfd = { .revents = 0,
                                    .events = POLLIN,
                                    .fd = STDIN_FILENO };
    switch (command) {
        case ClientCommand_ConnectToStream:
            selectAStreamToConnectTo(inputfd, bytes);
            break;
        case ClientCommand_DisconnectFromStream:
            selectAStreamToDisconnectFrom(inputfd, bytes);
            break;
        case ClientCommand_StopStreaming:
            selectStreamToStop(inputfd, bytes);
            break;
        case ClientCommand_UploadText:
            selectStreamToSendTextTo(inputfd, bytes);
            break;
        case ClientCommand_UploadFile:
            selectStreamToUploadFileTo(inputfd, bytes);
            break;
        case ClientCommand_ShowAllStreams:
            askQuestion("All Streams");
            for (size_t i = 0; i < clientData.allStreams.used; ++i) {
                printStream(&clientData.allStreams.buffer[i]);
            }
            break;
        case ClientCommand_ShowConnectedStreams: {
            askQuestion("Connected Streams");
            const Stream* stream = NULL;
            for (size_t i = 0; i < clientData.connectedStreams.used; ++i) {
                if (GetStreamFromGuid(&clientData.allStreams,
                                      &clientData.connectedStreams.buffer[i],
                                      &stream,
                                      NULL)) {
                    printStream(stream);
                }
            }
        } break;
        case ClientCommand_ShowOwnStreams: {
            askQuestion("Own Streams");
            const Stream* stream = NULL;
            for (size_t i = 0; i < clientData.ownStreams.used; ++i) {
                if (GetStreamFromGuid(&clientData.allStreams,
                                      &clientData.ownStreams.buffer[i],
                                      &stream,
                                      NULL)) {
                    printStream(stream);
                }
            }
        } break;
        default:
            break;
    }
}

void
clientHandleMessage(pMessage message)
{
    switch (message->tag) {
        case MessageTag_authenticateAck: {
#if _DEBUG
            char buffer[128];
            getGuidString(&message->authenticateAck.id, buffer);
            printf("Client name: %s (%s)\n",
                   message->authenticateAck.name.buffer,
                   buffer);
#else
            printf("Client name: %s\n", message->authenticateAck.name.buffer);
#endif
            TemLangStringCopy(&clientData.name,
                              &message->authenticateAck.name,
                              currentAllocator);
            clientData.id = message->authenticateAck.id;
        } break;
        case MessageTag_serverData: {
            GuidListCopy(&clientData.connectedStreams,
                         &message->serverData.connectedStreams,
                         currentAllocator);
            GuidListCopy(&clientData.ownStreams,
                         &message->serverData.clientStreams,
                         currentAllocator);
            StreamListCopy(&clientData.allStreams,
                           &message->serverData.allStreams,
                           currentAllocator);
            cleanupStreamDisplays();
            puts("Updated server data");
        } break;
        case MessageTag_getClientsAck: {
            pTemLangStringList clients = &message->getClientsAck;
            askQuestion("Clients");
            for (size_t i = 0; i < clients->used; ++i) {
                puts(clients->buffer[i].buffer);
            }
        } break;
        case MessageTag_startStreamingAck:
            switch (message->startStreamingAck.tag) {
                case OptionalGuidTag_guid: {
                    puts("Stream started");
                    SDL_Event e = { 0 };
                    e.type = SDL_USEREVENT;
                    e.user.code = CustomEvent_CheckAddedStream;
                    pGuid guid = currentAllocator->allocate(sizeof(Guid));
                    *guid = message->startStreamingAck.guid;
                    e.user.data1 = guid;
                    SDL_PushEvent(&e);
                } break;
                default:
                    puts("Server failed to start stream");
                    break;
            }
            break;
        case MessageTag_stopStreamingAck:
            switch (message->stopStreamingAck.tag) {
                case OptionalGuidTag_none:
                    puts("Server failed to stop stream");
                    break;
                default:
                    puts("Stopped stream");
                    break;
            }
            break;
        case MessageTag_connectToStreamAck:
            switch (message->connectToStreamAck.tag) {
                case OptionalStreamMessageTag_streamMessage: {
                    puts("Connected to stream");
                    const StreamMessage* streamMessage =
                      &message->connectToStreamAck.streamMessage;

                    SDL_Event e = { 0 };
                    e.type = SDL_USEREVENT;
                    e.user.code = CustomEvent_AddTexture;
                    Guid* guid = currentAllocator->allocate(sizeof(Guid));
                    *guid = streamMessage->id;
                    e.user.data1 = guid;
                    SDL_PushEvent(&e);

                    switch (streamMessage->data.tag) {
                        case StreamMessageDataTag_text:
                        case StreamMessageDataTag_chatMessage:
                        case StreamMessageDataTag_chatLogs:
                        case StreamMessageDataTag_video:
                        case StreamMessageDataTag_audio:
                        case StreamMessageDataTag_image: {
                            e.user.code = CustomEvent_UpdateStreamDisplay;
                            pStreamMessage m =
                              currentAllocator->allocate(sizeof(StreamMessage));
                            StreamMessageCopy(
                              m, streamMessage, currentAllocator);
                            e.user.data1 = m;
                            SDL_PushEvent(&e);
                        } break;
                        default:
                            break;
                    }
                } break;
                default:
                    puts("Failed to connect to stream");
                    break;
            }
            break;
        case MessageTag_disconnectFromStreamAck:
            if (message->disconnectFromStreamAck) {
                puts("Disconnected from stream");
            } else {
                puts("Failed to disconnect from stream");
            }
            break;
        case MessageTag_streamMessage:
            switch (message->streamMessage.data.tag) {
                case StreamMessageDataTag_text:
                case StreamMessageDataTag_chatLogs:
                case StreamMessageDataTag_chatMessage:
                case StreamMessageDataTag_video:
                case StreamMessageDataTag_audio:
                case StreamMessageDataTag_image: {
                    SDL_Event e = { 0 };
                    e.type = SDL_USEREVENT;
                    e.user.code = CustomEvent_UpdateStreamDisplay;
                    pStreamMessage m =
                      currentAllocator->allocate(sizeof(StreamMessage));
                    StreamMessageCopy(
                      m, &message->streamMessage, currentAllocator);
                    e.user.data1 = m;
                    SDL_PushEvent(&e);
                } break;
                default:
                    fprintf(stderr,
                            "Unexpected '%s' stream message\n",
                            StreamMessageDataTagToCharString(
                              message->streamMessage.data.tag));
                    break;
            }
            break;
        default:
            printf("Unexpected message: %s\n",
                   MessageTagToCharString(message->tag));
            break;
    }
}

void
updateTextDisplay(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  const Guid* id,
                  const TemLangString* string)
{
    if (renderer == NULL) {
        return;
    }
    size_t i = 0;
    SDL_Surface* surface = NULL;
    if (!GetStreamDisplayFromGuid(&clientData.displays, id, NULL, &i)) {
        puts("Missing stream display for text stream");
        goto end;
    }

    pStreamDisplay display = &clientData.displays.buffer[i];
    display->data.tag = StreamDisplayDataTag_none;
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }
    if (TemLangStringIsEmpty(string)) {
        goto end;
    }

    SDL_Color fg = { 0 };
    fg.r = fg.g = fg.b = fg.a = 0xffu;
    SDL_Color bg = { 0 };
    bg.a = 128u;
    surface = TTF_RenderUTF8_Shaded_Wrapped(ttfFont, string->buffer, fg, bg, 0);
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
updateImageDisplay(SDL_Renderer* renderer, const Guid* id, const Bytes* bytes)
{
    if (renderer == NULL) {
        return;
    }
    size_t i = 0;
    SDL_Surface* surface = NULL;
    if (!GetStreamDisplayFromGuid(&clientData.displays, id, NULL, &i)) {
        puts("Missing stream display for image stream");
        goto end;
    }
    if (uint8_tListIsEmpty(bytes)) {
        goto end;
    }

    pStreamDisplay display = &clientData.displays.buffer[i];
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
    if (ChatMessageListIsEmpty(&chat->logs)) {
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
        const ChatMessage* cm = &chat->logs.buffer[j];
        const time_t t = (time_t)cm->timeStamp;

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
                          const Guid* id,
                          const ChatMessageList* list)
{
    if (renderer == NULL) {
        return;
    }
    size_t i = 0;
    if (!GetStreamDisplayFromGuid(&clientData.displays, id, NULL, &i)) {
        puts("Missing stream display for chat stream");
        return;
    }

    pStreamDisplay display = &clientData.displays.buffer[i];
    if (display->data.chat.logs.allocator == NULL) {
        display->data.chat.count = DEFAULT_CHAT_COUNT;
    }
    ChatMessageListCopy(&display->data.chat.logs, list, currentAllocator);
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
                             const Guid* id,
                             const ChatMessage* message)
{
    if (renderer == NULL) {
        return;
    }
    size_t i = 0;
    if (!GetStreamDisplayFromGuid(&clientData.displays, id, NULL, &i)) {
        puts("Missing stream display for chat stream");
        return;
    }

    pStreamDisplay display = &clientData.displays.buffer[i];
    pStreamDisplayChat chat = &display->data.chat;
    pChatMessageList list = &chat->logs;
    if (list->allocator == NULL) {
        list->allocator = currentAllocator;
        chat->count = DEFAULT_CHAT_COUNT;
    }
    ChatMessageListAppend(list, message);
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

int
clientConnectionThread(const AllConfiguration* configuration)
{
    const ClientConfiguration* config = &configuration->configuration.client;

    int result = EXIT_FAILURE;
    ENetHost* host = NULL;
    ENetPeer* peer = NULL;

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
        peer = enet_host_connect(host, &address, 2, 0);
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
    if (enet_host_service(host, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        char buffer[KB(1)] = { 0 };
        enet_address_get_host_ip(&event.peer->address, buffer, sizeof(buffer));
        printf(
          "Connected to server: %s:%u\n", buffer, event.peer->address.port);
        displayUserOptions();
        Message message = { 0 };
        message.tag = MessageTag_authenticate;
        message.authenticate = config->authentication;
        Bytes bytes = { .allocator = currentAllocator,
                        .buffer = (uint8_t*)buffer,
                        .used = 0,
                        .size = sizeof(buffer) };
        MESSAGE_SERIALIZE(message, bytes);
        ENetPacket* packet = BytesToPacket(&bytes, true);
        enet_peer_send(peer, CLIENT_CHANNEL, packet);
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        appDone = true;
        goto end;
    }

    while (!appDone) {
        IN_MUTEX(packetMutex, end2, {
            for (size_t i = 0; i < outgoingPackets.used; ++i) {
                enet_peer_send(peer, CLIENT_CHANNEL, outgoingPackets.buffer[i]);
            }
            outgoingPackets.used = 0;
        });
        const int status = enet_host_service(host, &event, 100U);
        if (status < 0) {
            fprintf(stderr, "Connection error\n");
            SDL_Event e = { 0 };
            e.user.code = CustomEvent_ShowSimpleMessage;
            e.user.data1 = "Connection error";
            e.user.data2 = "Lost connection from server";
            SDL_PushEvent(&e);
            appDone = true;
            goto end;
        }
        if (status == 0) {
            continue;
        }
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                puts("Unexpected connect event from server");
                appDone = true;
                goto end;
            case ENET_EVENT_TYPE_DISCONNECT: {
                puts("Disconnected from server");
                appDone = true;
                SDL_Event e = { 0 };
                e.user.code = CustomEvent_ShowSimpleMessage;
                e.user.data1 = "Connection error";
                e.user.data2 = "Lost connection from server";
                SDL_PushEvent(&e);
                goto end;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                const Bytes packetBytes = { .allocator = currentAllocator,
                                            .buffer = event.packet->data,
                                            .size = event.packet->dataLength,
                                            .used = event.packet->dataLength };
                pMessage message = currentAllocator->allocate(sizeof(Message));
                MESSAGE_DESERIALIZE((*message), packetBytes);
                SDL_Event e = { 0 };
                e.type = SDL_USEREVENT;
                e.user.code = CustomEvent_HandleMessage;
                e.user.data1 = message;
                SDL_PushEvent(&e);
                enet_packet_destroy(event.packet);
            } break;
            case ENET_EVENT_TYPE_NONE:
                break;
            default:
                break;
        }
    }

    result = EXIT_SUCCESS;

end:
    if (peer != NULL) {
        enet_peer_disconnect(peer, 0);
        while (enet_host_service(host, &event, 3000) > 0) {
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
    return result;
}

int
displayError(SDL_Window* window, const char* e)
{
    return SDL_ShowSimpleMessageBox(
      SDL_MESSAGEBOX_ERROR, "SDL error", e, window);
}

void
startPlayback(const char* name)
{
    if (playbackId != 0) {
        SDL_CloseAudioDevice(playbackId);
        playbackId = 0;
    }
    const SDL_AudioSpec desired =
      makeAudioSpec((SDL_AudioCallback)audioPlaybackCallback, NULL);
    SDL_AudioSpec obtained = { 0 };
    playbackId = SDL_OpenAudioDevice(
      name, SDL_FALSE, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (playbackId == 0) {
        fprintf(stderr,
                "Failed to open playback device. No audio will be played\n");
    } else {
        puts("Playback audio specification");
        printAudioSpec(&obtained);
        SDL_PauseAudioDevice(playbackId, SDL_FALSE);
#if _DEBUG
        printf("Playback id: %u\n", playbackId);
#endif
    }
}

int
runClient(const AllConfiguration* configuration)
{
    int result = EXIT_FAILURE;
    puts("Running client");
    printAllConfiguration(configuration);

    const ClientConfiguration* config = &configuration->configuration.client;

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    // Font font = { 0 };
    TTF_Font* ttfFont = NULL;
    SDL_Thread* threads[2] = { 0 };
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };

    clientData.displays.allocator = currentAllocator;
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        displayError(window, "Failed to start");
        goto end;
    }

    {
        const uint32_t flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
        if (IMG_Init(flags) != flags) {
            fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
            displayError(window, "Failed to start");
            goto end;
        }
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Failed to init TTF: %s\n", TTF_GetError());
        displayError(window, "Failed to start");
        goto end;
    }

    packetMutex = SDL_CreateMutex();
    if (packetMutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        displayError(window, "Failed to start");
        goto end;
    }

    outgoingPackets.allocator = currentAllocator;
    audioRecordings.allocator = currentAllocator;
    audioBytes.allocator = currentAllocator;

    if (!config->noGui) {
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
            displayError(window, "Failed to start");
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
                displayError(window, "Failed to start");
                goto end;
            }

            SDL_GetRendererInfo(renderer, &info);
            printf("Current renderer: %s\n", info.name);
        }

        startPlayback(NULL);
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
        displayError(window, "Failed to start");
        goto end;
    }

    appDone = false;
    threads[0] = SDL_CreateThread(
      (SDL_ThreadFunction)clientConnectionThread, "enet", (void*)configuration);
    if (threads[0] == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        displayError(window, "Failed to start");
        goto end;
    }
    threads[1] = SDL_CreateThread(
      (SDL_ThreadFunction)userInputThread, "user", (void*)configuration);
    if (threads[1] == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        displayError(window, "Failed to start");
        goto end;
    }

    SDL_AtomicSet(&runningThreads, 0);

    SDL_Event e = { 0 };

    size_t targetDisplay = UINT32_MAX;
    MoveMode moveMode = MoveMode_None;
    bool hasTarget = false;

    while (!appDone) {
        for (size_t i = 0; i < audioRecordings.used; ++i) {
            AudioRecordStatePtr ptr = audioRecordings.buffer[i];
            if (GetStreamFromGuid(
                  &clientData.allStreams, &ptr->id, NULL, NULL)) {
                continue;
            }
            ptr->running = SDL_FALSE;
            AudioRecordStatePtrListRemove(&audioRecordings, i);
        }
        switch (SDL_WaitEventTimeout(&e, CLIENT_POLL_WAIT)) {
            case -1:
                fprintf(stderr, "Event error: %s\n", SDL_GetError());
                displayError(window, "Failed to start");
                appDone = true;
                continue;
            case 0:
                continue;
            default:
                break;
        }
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
                    case SDLK_v: {
                        if ((e.key.keysym.mod & KMOD_CTRL) == 0) {
                            break;
                        }
                        e.type = SDL_DROPTEXT;
                        e.drop.file = SDL_GetClipboardText();
                        e.drop.timestamp = SDL_GetTicks64();
                        e.drop.windowID = SDL_GetWindowID(window);
                        SDL_PushEvent(&e);
                    } break;
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
            case SDL_MOUSEBUTTONUP: {
                hasTarget = false;
                targetDisplay = UINT32_MAX;
                SDL_SetWindowGrab(window, false);
                renderDisplays();
            } break;
            case SDL_MOUSEMOTION: {
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
                            display->dstRect.w = SDL_clamp(
                              display->dstRect.w + e.motion.xrel, MIN_WIDTH, w);
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
            } break;
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
                    case CustomEvent_SaveScreenshot:
                        saveScreenshot(renderer, (const Guid*)e.user.data1);
                        currentAllocator->free(e.user.data1);
                        break;
                    case CustomEvent_HandleMessage: {
                        pMessage message = e.user.data1;
                        clientHandleMessage(message);
                        MessageFree(message);
                        currentAllocator->free(e.user.data1);
                    } break;
                    case CustomEvent_ShowSimpleMessage:
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                                 (char*)e.user.data1,
                                                 (char*)e.user.data2,
                                                 window);
                        break;
                    case CustomEvent_CheckAddedStream: {
                        pGuid guid = e.user.data1;
                        const Stream* stream = NULL;
                        if (GetStreamFromGuid(
                              &clientData.allStreams, guid, &stream, NULL) &&
                            stream->type == StreamType_Audio) {
                            startRecording(guid);
                        }
                        currentAllocator->free(guid);
                    } break;
                    case CustomEvent_AddTexture: {
                        StreamDisplay s = { 0 };
                        s.dstRect.x = 0.f;
                        s.dstRect.y = 0.f;
                        s.dstRect.w = (float)w;
                        s.dstRect.h = (float)h;
                        s.id = *(Guid*)e.user.data1;
                        // Create texture once data is received
                        StreamDisplayListAppend(&clientData.displays, &s);
                        currentAllocator->free(e.user.data1);
                        renderDisplays();
                    } break;
                    case CustomEvent_UpdateStreamDisplay: {
                        pStreamMessage m = e.user.data1;
                        switch (m->data.tag) {
                            case StreamMessageDataTag_text:
                                updateTextDisplay(
                                  renderer, ttfFont, &m->id, &m->data.text);
                                break;
                            case StreamMessageDataTag_chatMessage:
                                updateChatDisplayFromMessage(
                                  renderer,
                                  ttfFont,
                                  w,
                                  h,
                                  &m->id,
                                  &m->data.chatMessage);
                                break;
                            case StreamMessageDataTag_chatLogs:
                                updateChatDisplayFromList(renderer,
                                                          ttfFont,
                                                          w,
                                                          h,
                                                          &m->id,
                                                          &m->data.chatLogs);
                                break;
                            case StreamMessageDataTag_image:
                                updateImageDisplay(
                                  renderer, &m->id, &m->data.image);
                                break;
                            case StreamMessageDataTag_audio: {
                                if (playbackId == 0) {
                                    break;
                                }
                                SDL_LockAudioDevice(playbackId);
                                uint8_tListQuickAppend(&audioBytes,
                                                       m->data.audio.buffer,
                                                       m->data.audio.used);
                                SDL_UnlockAudioDevice(playbackId);
                            } break;
                            default:
                                printf("Cannot update display of stream '%s'\n",
                                       StreamMessageDataTagToCharString(
                                         m->data.tag));
                                break;
                        }
                        StreamMessageFree(m);
                        currentAllocator->free(m);
                        renderDisplays();
                    } break;
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
                    loadNewFont(e.drop.file, config->fontSize, &ttfFont);
                    goto endDropFile;
                }

                if (!mapFile(e.drop.file, &fd, &ptr, &size, MapFileType_Read)) {
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
                    if (!GetStreamFromType(&clientData.allStreams,
                                           FileExtenstionToStreamType(ext.tag),
                                           &stream,
                                           NULL)) {
                        puts("No stream to send data too...");
                        goto endDropFile;
                    }
                } else {
                    const StreamDisplay* display =
                      &clientData.displays.buffer[targetDisplay];
                    if (!GetStreamFromGuid(&clientData.allStreams,
                                           &display->id,
                                           &stream,
                                           NULL)) {
                        puts("No stream to send data too...");
                        goto endDropFile;
                    }
                }

                Message message = { 0 };
                message.tag = MessageTag_streamMessage;
                message.streamMessage.id = stream->id;
                switch (ext.tag) {
                    case FileExtensionTag_audio:
                        message.streamMessage.data.tag =
                          StreamMessageDataTag_audio;
                        message.streamMessage.data.audio = fileBytes;
                        break;
                    case FileExtensionTag_video:
                        message.streamMessage.data.tag =
                          StreamMessageDataTag_video;
                        message.streamMessage.data.video = fileBytes;
                        break;
                    default:
                        message.streamMessage.data.tag =
                          StreamMessageDataTag_image;
                        message.streamMessage.data.image = fileBytes;
                        break;
                }
                MESSAGE_SERIALIZE(message, bytes);
                ENetPacket* packet = BytesToPacket(&bytes, true);
                enqueuePacket(packet);
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
                if (!GetStreamFromGuid(
                      &clientData.allStreams, &display->id, &stream, NULL)) {
                    puts("No stream to text too...");
                    break;
                }
                switch (stream->type) {
                    case StreamType_Text: {
                        Message message = { 0 };
                        message.tag = MessageTag_streamMessage;
                        message.streamMessage.id = stream->id;
                        message.streamMessage.data.tag =
                          StreamMessageDataTag_text;
                        message.streamMessage.data.text =
                          TemLangStringCreate(e.drop.file, currentAllocator);
                        MESSAGE_SERIALIZE(message, bytes);
                        MessageFree(&message);
                        ENetPacket* packet = BytesToPacket(&bytes, true);
                        enqueuePacket(packet);
                    } break;
                    case StreamType_Chat: {
                        Message message = { 0 };
                        message.tag = MessageTag_streamMessage;
                        message.streamMessage.id = stream->id;
                        message.streamMessage.data.tag =
                          StreamMessageDataTag_chatMessage;
                        message.streamMessage.data.chatMessage.message =
                          TemLangStringCreate(e.drop.file, currentAllocator);
                        MESSAGE_SERIALIZE(message, bytes);
                        MessageFree(&message);
                        ENetPacket* packet = BytesToPacket(&bytes, true);
                        enqueuePacket(packet);
                    } break;
                    default:
                        printf("Cannot send text to '%s' stream\n",
                               StreamTypeToCharString(stream->type));
                        break;
                }
                SDL_free(e.drop.file);
            } break;
            default:
                break;
        }
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    // FontFree(&font);
    for (size_t i = 0; i < sizeof(threads) / sizeof(SDL_Thread*); ++i) {
        SDL_WaitThread(threads[i], NULL);
    }
    while (SDL_AtomicGet(&runningThreads) > 0) {
        SDL_Delay(1);
    }
    pENetPacketListFree(&outgoingPackets);
    AudioRecordStatePtrListFree(&audioRecordings);
    SDL_CloseAudioDevice(playbackId);
    SDL_DestroyMutex(packetMutex);
    uint8_tListFree(&bytes);
    uint8_tListFree(&audioBytes);
    ClientDataFree(&clientData);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(ttfFont);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return result;
}