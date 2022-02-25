#include <include/main.h>

#define MIN_WIDTH 32
#define MIN_HEIGHT 32

ClientData clientData = { 0 };

void
refreshStreams(ENetPeer* peer, pBytes);

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
    puts("Cleaned up stream display");
#endif
    size_t i = 0;
    while (i < clientData.displays.used) {
        if (GuidListFind(&clientData.connectedStreams,
                         &clientData.displays.buffer[i].id,
                         NULL,
                         NULL)) {
            ++i;
        } else {
            StreamDisplayListSwapRemove(&clientData.displays, i);
        }
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

bool
ClientGuidEquals(const pClient* client, const Guid* guid)
{
    return GuidEquals(&(*client)->id, guid);
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
userIndexFromUser(struct pollfd inputfd,
                  pBytes bytes,
                  const uint32_t max,
                  uint32_t* index,
                  const int timeout)
{
    ssize_t size = 0;
    switch (getUserInput(inputfd, bytes, &size, timeout)) {
        case UserInputResult_Error:
            return UserInputResult_Error;
        case UserInputResult_NoInput:
            return UserInputResult_NoInput;
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

void
refreshStreams(ENetPeer* peer, pBytes bytes)
{
    puts("Requesting stream information from server...");
    Message message = { 0 };

    message.tag = MessageTag_getAllData;
    message.getAllData = NULL;
    MESSAGE_SERIALIZE(message, (*bytes));
    ENetPacket* packet = BytesToPacket(bytes, true);
    enet_peer_send(peer, CLIENT_CHANNEL, packet);
}

void
selectAStreamToConnectTo(struct pollfd inputfd, ENetPeer* peer, pBytes bytes)
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
    } else if (userIndexFromUser(
                 inputfd, bytes, streamNum, &i, USER_INPUT_TIMEOUT) !=
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
    enet_peer_send(peer, CLIENT_CHANNEL, packet);
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd,
                              ENetPeer* peer,
                              pBytes bytes)
{
    StreamList streams = { 0 };
    const uint32_t streamNum = clientData.connectedStreams.used;
    askQuestion("Select a stream to disconnect from");
    const Stream* stream = NULL;
    uint32_t streamsFound = 0;
    StreamListFree(&streams);
    streams.allocator = currentAllocator;
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.connectedStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            ++streamsFound;
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streamsFound == 0) {
        puts("Not connected to any streams");
        goto end;
    }
    uint32_t i = 0;
    if (streamsFound == 1) {
        puts("Disconnecting from only stream available");
    } else if (userIndexFromUser(
                 inputfd, bytes, streamsFound, &i, USER_INPUT_TIMEOUT) !=
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
    enet_peer_send(peer, CLIENT_CHANNEL, packet);
end:
    StreamListFree(&streams);
}

void
selectStreamToStart(struct pollfd inputfd, ENetPeer* peer, pBytes bytes)
{
    askQuestion("Select the type of stream to create");
    for (uint32_t i = 0; i < StreamType_Length; ++i) {
        printf("%u) %s\n", i + 1U, StreamTypeToCharString(i));
    }
    puts("");

    uint32_t index;
    if (userIndexFromUser(
          inputfd, bytes, StreamType_Length, &index, USER_INPUT_TIMEOUT) !=
        UserInputResult_Input) {
        puts("Canceling start stream");
        return;
    }

    Message message = { 0 };
    message.tag = MessageTag_startStreaming;
    message.startStreaming.type = (StreamType)index;

    askQuestion("What's the name of the stream?");
    if (getUserInput(inputfd, bytes, NULL, USER_INPUT_TIMEOUT) !=
        UserInputResult_Input) {
        puts("Canceling start stream");
        return;
    }
    puts("");

    message.startStreaming.name =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);

    askQuestion("Do want the stream to be recorded on the server (y or n)?");
    while (!appDone) {
        if (getUserInput(inputfd, bytes, NULL, USER_INPUT_TIMEOUT) !=
            UserInputResult_Input) {
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

    // TODO: Allow readers and writers to be set

    MESSAGE_SERIALIZE(message, (*bytes));
    printf("\nCreating '%s' stream named '%s'...\n",
           StreamTypeToCharString(message.startStreaming.type),
           message.startStreaming.name.buffer);
    MessageFree(&message);
    ENetPacket* packet = BytesToPacket(bytes, true);
    enet_peer_send(peer, CLIENT_CHANNEL, packet);
}

void
selectStreamToStop(struct pollfd inputfd, ENetPeer* peer, pBytes bytes)
{
    StreamList streams = { 0 };

    const Stream* stream = NULL;
    const uint32_t streamNum = clientData.ownStreams.used;
    uint32_t streamsFound = 0;
    askQuestion("Select a stream to stop");
    StreamListFree(&streams);
    streams.allocator = currentAllocator;
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.ownStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            ++streamsFound;
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streamsFound == 0) {
        puts("No streams stop");
        goto end;
    }

    uint32_t i = 0;
    if (streamsFound == 1) {
        puts("Stopping to only stream available");
    } else if (userIndexFromUser(
                 inputfd, bytes, streamsFound, &i, USER_INPUT_TIMEOUT)) {
        goto end;
    }

    Message message = { 0 };
    message.tag = MessageTag_stopStreaming;
    message.stopStreaming = streams.buffer[i].id;

    MESSAGE_SERIALIZE(message, (*bytes));
    MessageFree(&message);

    ENetPacket* packet = BytesToPacket(bytes, true);
    enet_peer_send(peer, CLIENT_CHANNEL, packet);

end:
    StreamListFree(&streams);
}

void
selectStreamToSendTextTo(struct pollfd inputfd, ENetPeer* peer, pBytes bytes)
{
    StreamList streams = { 0 };
    const Stream* stream = NULL;
    const uint32_t streamNum = clientData.connectedStreams.used;
    uint32_t streamsFound = 0;
    askQuestion("Send text to which stream?");
    streams.allocator = currentAllocator;
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.connectedStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            ++streamsFound;
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streamsFound == 0) {
        puts("No streams to send text to");
        goto end;
    }

    uint32_t i = 0;
    if (streamsFound == 1) {
        puts("Sending text to only stream available");
    } else if (userIndexFromUser(
                 inputfd, bytes, streamsFound, &i, USER_INPUT_TIMEOUT) !=
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
            if (getUserInput(inputfd, bytes, NULL, USER_INPUT_TIMEOUT) !=
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
            if (getUserInput(inputfd, bytes, NULL, USER_INPUT_TIMEOUT) !=
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
    enet_peer_send(peer, CLIENT_CHANNEL, packet);

end:
    StreamListFree(&streams);
}

void
selectStreamToUploadFileTo(struct pollfd inputfd, ENetPeer* peer, pBytes bytes)
{
    StreamList streams = { 0 };
    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;

    const Stream* stream = NULL;
    const uint32_t streamNum = clientData.connectedStreams.used;
    uint32_t streamsFound = 0;
    askQuestion("Send file to which stream?");
    StreamListFree(&streams);
    streams.allocator = currentAllocator;
    for (uint32_t i = 0; i < streamNum; ++i) {
        const Guid guid = clientData.connectedStreams.buffer[i];
        if (GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL)) {
            printf("%u) %s\n", i + 1U, stream->name.buffer);
            ++streamsFound;
            StreamListAppend(&streams, stream);
        }
    }
    puts("");

    if (streamNum == 0 || streamsFound == 0) {
        puts("No streams to send data");
        goto end;
    }

    uint32_t i = 0;
    if (streamsFound == 1) {
        puts("Sending data to only stream available");
    } else if (userIndexFromUser(
                 inputfd, bytes, streamsFound, &i, USER_INPUT_TIMEOUT) !=
               UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    stream = &streams.buffer[i];

    askQuestion("Enter file name");
    if (getUserInput(inputfd, bytes, NULL, USER_INPUT_TIMEOUT) !=
        UserInputResult_Input) {
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
    enet_peer_send(peer, CLIENT_CHANNEL, packet);
end:
    unmapFile(fd, ptr, size);
    StreamListFree(&streams);
}

void
saveScreenshot(SDL_Renderer* renderer, const Guid* id)
{
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
handleUserInput(ENetPeer* peer, pBytes bytes)
{
    struct pollfd inputfd = { .events = POLLIN,
                              .revents = 0,
                              .fd = STDIN_FILENO };

    uint32_t index;
    switch (
      userIndexFromUser(inputfd, bytes, ClientCommand_Length, &index, 0)) {
        case UserInputResult_Input:
            break;
        case UserInputResult_Error:
            displayUserOptions();
            goto end;
        default:
            goto end;
    }

    bool sendEvent = false;
    switch (index) {
        case ClientCommand_PrintName:
            printf("Client name: %s\n", clientData.name.buffer);
            break;
        case ClientCommand_GetClients: {
            puts("Getting clients from server...");
            Message message = { 0 };
            message.tag = MessageTag_getClients;
            message.getClients = NULL;
            MESSAGE_SERIALIZE(message, (*bytes));
            ENetPacket* packet = BytesToPacket(bytes, true);
            enet_peer_send(peer, CLIENT_CHANNEL, packet);
        } break;
        case ClientCommand_Quit:
            appDone = true;
            break;
        case ClientCommand_StartStreaming:
            selectStreamToStart(inputfd, peer, bytes);
            break;
        case ClientCommand_SaveScreenshot: {
            StreamList streams = { .allocator = currentAllocator };
            askQuestion("Select stream to take screenshot from");
            for (size_t i = 0; i < clientData.connectedStreams.used; ++i) {
                const Stream* stream = NULL;
                if (!GetStreamFromGuid(&clientData.allStreams,
                                       &clientData.connectedStreams.buffer[i],
                                       &stream,
                                       NULL)) {
                    continue;
                }
                printf("%zu) %s\n", streams.used + 1UL, stream->name.buffer);
                StreamListAppend(&streams, stream);
            }
            puts("");

            uint32_t i = 1;
            if (StreamListIsEmpty(&streams)) {
                puts("No streams to take screenshot from");
                goto saveEnd;
            } else if (streams.used == 1) {
                puts("Taking screenshot from only available stream");
            } else if (userIndexFromUser(inputfd,
                                         bytes,
                                         streams.used,
                                         &i,
                                         USER_INPUT_TIMEOUT) !=
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
            break;
        }
        case ClientCommand_ConnectToStream:
        case ClientCommand_ShowAllStreams:
        case ClientCommand_ShowConnectedStreams:
        case ClientCommand_ShowOwnStreams:
        case ClientCommand_DisconnectFromStream:
        case ClientCommand_StopStreaming:
        case ClientCommand_UploadFile:
        case ClientCommand_UploadText:
            refreshStreams(peer, bytes);
            sendEvent = true;
            break;
        default:
            fprintf(stderr,
                    "Command '%s' is not implemented\n",
                    ClientCommandToCharString(index));
            break;
    }

    if (sendEvent) {
        SDL_Event e = { 0 };
        e.type = SDL_USEREVENT;
        e.user.code = CustomEvent_ClientCommand;
        e.user.data1 = (void*)(size_t)index;
        SDL_PushEvent(&e);
    }

end:
    return;
}

void
handleClientCommand(const ClientCommand command, ENetPeer* peer, pBytes bytes)
{
    const struct pollfd inputfd = { .revents = 0,
                                    .events = POLLIN,
                                    .fd = STDIN_FILENO };
    switch (command) {
        case ClientCommand_GetClients:
            askQuestion("Clients");
            for (size_t i = 0; i < clientData.otherClients.used; ++i) {
                printf(
                  "%zu) %s\n", i + 1, clientData.otherClients.buffer[i].buffer);
            }
            puts("");
            break;
        case ClientCommand_ConnectToStream:
            selectAStreamToConnectTo(inputfd, peer, bytes);
            break;
        case ClientCommand_DisconnectFromStream:
            selectAStreamToDisconnectFrom(inputfd, peer, bytes);
            break;
        case ClientCommand_StopStreaming:
            selectStreamToStop(inputfd, peer, bytes);
            break;
        case ClientCommand_UploadText:
            selectStreamToSendTextTo(inputfd, peer, bytes);
            break;
        case ClientCommand_UploadFile:
            selectStreamToUploadFileTo(inputfd, peer, bytes);
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
clientHandleMessage(ENetPeer* peer, pMessage message, pBytes bytes)
{
    switch (message->tag) {
        case MessageTag_authenticateAck:
            printf("Client name: %s\n", message->authenticateAck.name.buffer);
            TemLangStringCopy(&clientData.name,
                              &message->authenticateAck.name,
                              currentAllocator);
            clientData.id = message->authenticateAck.id;
            break;
        case MessageTag_getAllDataAck: {
            GuidListCopy(&clientData.connectedStreams,
                         &message->getAllDataAck.connectedStreams,
                         currentAllocator);
            GuidListCopy(&clientData.ownStreams,
                         &message->getAllDataAck.clientStreams,
                         currentAllocator);
            StreamListCopy(&clientData.allStreams,
                           &message->getAllDataAck.allStreams,
                           currentAllocator);
            cleanupStreamDisplays();
            puts("Updated server data");
        } break;
        case MessageTag_getClientsAck:
            TemLangStringListCopy(&clientData.otherClients,
                                  &message->getClientsAck,
                                  currentAllocator);
            puts("Updated client list");
            break;
        case MessageTag_getConnectedStreamsAck:
            GuidListCopy(&clientData.connectedStreams,
                         &message->getConnectedStreamsAck,
                         currentAllocator);
            cleanupStreamDisplays();
            puts("Updated connected streams");
            break;
        case MessageTag_getClientStreamsAck:
            GuidListCopy(&clientData.ownStreams,
                         &message->getClientStreamsAck,
                         currentAllocator);
            puts("Updated client's streams");
            break;
        case MessageTag_getAllStreamsAck:
            StreamListCopy(&clientData.allStreams,
                           &message->getAllStreamsAck,
                           currentAllocator);
            puts("Updated stream list");
            break;
        case MessageTag_startStreamingAck:
            switch (message->startStreamingAck.tag) {
                case OptionalGuidTag_none:
                    puts("Server failed to start stream");
                    break;
                default: {
                    message->tag = MessageTag_getClientStreams;
                    message->getClientStreams = NULL;
                    MESSAGE_SERIALIZE((*message), (*bytes));
                    ENetPacket* packet = BytesToPacket(bytes, true);
                    enet_peer_send(peer, CLIENT_CHANNEL, packet);
                } break;
            }
            refreshStreams(peer, bytes);
            break;
        case MessageTag_stopStreamingAck:
            switch (message->stopStreamingAck.tag) {
                case OptionalGuidTag_none:
                    puts("Server failed to stop stream");
                    break;
                default:
                    message->tag = MessageTag_getClientStreams;
                    message->getClientStreams = NULL;
                    MESSAGE_SERIALIZE((*message), (*bytes));
                    ENetPacket* packet = BytesToPacket(bytes, true);
                    enet_peer_send(peer, CLIENT_CHANNEL, packet);
                    break;
            }
            refreshStreams(peer, bytes);
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
            refreshStreams(peer, bytes);
            break;
        case MessageTag_disconnectFromStreamAck:
            if (message->disconnectFromStreamAck) {
                puts("Disconnected from stream");
            } else {
                puts("Failed to disconnect from stream");
            }
            refreshStreams(peer, bytes);
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
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    SDL_Rect rect = { 0 };

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
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }

    pStreamDisplayChat chat = &display->data.chat;
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
        if (offset == chat->count) {
            chat->offset = chat->logs.used;
        }
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
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        if (!SDL_PointInFRect(point, (const SDL_FRect*)&display->dstRect)) {
            continue;
        }
        *targetDisplay = i;
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
runClient(const AllConfiguration* configuration)
{
    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    int result = EXIT_FAILURE;
    puts("Running client");
    printAllConfiguration(configuration);

    const ClientConfiguration* config = &configuration->configuration.client;

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    // Font font = { 0 };
    TTF_Font* ttfFont = NULL;
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };

    clientData.displays.allocator = currentAllocator;
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    {
        const uint32_t flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
        if (IMG_Init(flags) != flags) {
            fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
            goto end;
        }
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Failed to init TTF: %s\n", TTF_GetError());
        goto end;
    }

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
            fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
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
        goto end;
    }

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        goto end;
    }
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, configuration->address.ip.buffer);
        char* end = NULL;
        address.port =
          (uint16_t)strtoul(configuration->address.port.buffer, &end, 10);
        peer = enet_host_connect(host, &address, 2, 0);
    }
    if (peer == NULL) {
        fprintf(stderr, "Failed to connect to client\n");
        goto end;
    }

    SDL_Event e = { 0 };
    ENetEvent event = { 0 };
    size_t targetDisplay = UINT32_MAX;
    MoveMode moveMode = MoveMode_None;
    bool hasTarget = false;
    appDone = false;
    while (!appDone) {
        if (SDL_PollEvent(&e) == 0) {
            goto checkHost;
        }
        switch (e.type) {
            case SDL_QUIT:
                appDone = true;
                break;
            case SDL_WINDOWEVENT:
                renderDisplays();
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
                        int64_t offset = chat->offset;

                        // printf("Before: %" PRId64 "\n", offset);
                        offset += (e.wheel.y > 0) ? -1LL : 1LL;
                        offset = SDL_clamp(offset, 0LL, chat->logs.used);
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
                    case CustomEvent_ClientCommand:
                        handleClientCommand(
                          (ClientCommand)(size_t)e.user.data1, peer, &bytes);
                        break;
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
                enet_peer_send(peer, CLIENT_CHANNEL, packet);
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
                        enet_peer_send(peer, CLIENT_CHANNEL, packet);
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
                        enet_peer_send(peer, CLIENT_CHANNEL, packet);
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
    checkHost:
        handleUserInput(peer, &bytes);
        result = enet_host_service(host, &event, 0);
        if (result < 0) {
            fprintf(stderr, "Connection error\n");
            appDone = true;
            break;
        }
        if (result == 0) {
            continue;
        }
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                printf("Connected to server: %x:%u\n",
                       event.peer->address.host,
                       event.peer->address.port);
                displayUserOptions();
                Message message = { 0 };
                message.tag = MessageTag_authenticate;
                message.authenticate = config->authentication;
                MESSAGE_SERIALIZE(message, bytes);
                ENetPacket* packet = BytesToPacket(&bytes, true);
                enet_peer_send(peer, CLIENT_CHANNEL, packet);
            } break;
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("Disconnected from server");
                appDone = true;
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                Message message = { 0 };
                const Bytes packetBytes = { .allocator = NULL,
                                            .buffer = event.packet->data,
                                            .size = event.packet->dataLength,
                                            .used = event.packet->dataLength };
                MESSAGE_DESERIALIZE(message, packetBytes);
                clientHandleMessage(peer, &message, &bytes);
                MessageFree(&message);
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
    appDone = true;
    // FontFree(&font);
    uint8_tListFree(&bytes);
    if (peer != NULL) {
        enet_peer_disconnect(peer, 0);
        while (enet_host_service(host, &event, 3000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE:
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    puts("Disconnected gracefully from server");
                    goto endPeer;
                default:
                    break;
            }
        }
    endPeer:
        enet_peer_reset(peer);
    }
    if (host != NULL) {
        enet_host_destroy(host);
    }
    ClientDataFree(&clientData);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(ttfFont);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return result;
}