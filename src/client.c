#include <include/main.h>

#define MIN_WIDTH 32
#define MIN_HEIGHT 32

ClientData clientData = { 0 };

bool
refreshStreams(const int, pBytes);

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
    IN_MUTEX(clientData.mutex, end, {
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
    });
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
    closeSocket(client->sockfd);
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

bool
getUserInput(struct pollfd inputfd, pBytes bytes, ssize_t* output)
{
    while (!appDone) {
        switch (poll(&inputfd, 1, CLIENT_POLL_WAIT)) {
            case -1:
                perror("poll");
                return false;
            case 0:
                continue;
            default:
                goto readInput;
        }
    }

readInput:
    if ((inputfd.revents & POLLIN) == 0) {
        return false;
    }

    const size_t size = read(STDIN_FILENO, bytes->buffer, bytes->size) - 1;
    if (size <= 0) {
        return false;
    }
    // Remove new line character
    bytes->buffer[size] = '\0';
    if (output != NULL) {
        *output = size;
    }
    return true;
}

bool
userIndexFromUser(struct pollfd inputfd,
                  pBytes bytes,
                  const uint32_t max,
                  uint32_t* index)
{
    ssize_t size = 0;
    if (!getUserInput(inputfd, bytes, &size)) {
        return false;
    }
    char* end = NULL;
    *index = (uint32_t)strtoul((const char*)bytes->buffer, &end, 10) - 1UL;
    if (end != (char*)&bytes->buffer[size] || *index >= max) {
        printf("Enter a number between 1 and %u\n", max);
        return false;
    }
    return true;
}

bool
refreshStreams(const int sockfd, pBytes bytes)
{
    puts("Requesting stream information from server...");
    Message message = { 0 };

    message.tag = MessageTag_getAllData;
    message.getAllData = NULL;
    MESSAGE_SERIALIZE(message, (*bytes));
    return socketSend(sockfd, bytes, true);
}

void
selectAStreamToConnectTo(struct pollfd inputfd, const int sockfd, pBytes bytes)
{
    IN_MUTEX(clientData.mutex, end0, {
        while (!appDone) {
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
                break;
            }

            uint32_t i = 0;
            if (streamNum == 1) {
                puts("Connecting to only stream available");
            } else if (!userIndexFromUser(inputfd, bytes, streamNum, &i)) {
                continue;
            }

            Message message = { 0 };
            message.tag = MessageTag_connectToStream;
            message.connectToStream = clientData.allStreams.buffer[i].id;
            MESSAGE_SERIALIZE(message, (*bytes));
            socketSend(sockfd, bytes, true);
            MessageFree(&message);
            break;
        }
    });
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd,
                              const int sockfd,
                              pBytes bytes)
{
    StreamList streams = { 0 };
    IN_MUTEX(clientData.mutex, end0, {
        while (!appDone) {
            const uint32_t streamNum = clientData.connectedStreams.used;
            askQuestion("Select a stream to disconnect from");
            const Stream* stream = NULL;
            uint32_t streamsFound = 0;
            StreamListFree(&streams);
            streams.allocator = currentAllocator;
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Guid guid = clientData.connectedStreams.buffer[i];
                if (GetStreamFromGuid(
                      &clientData.allStreams, &guid, &stream, NULL)) {
                    printf("%u) %s\n", i + 1U, stream->name.buffer);
                    ++streamsFound;
                    StreamListAppend(&streams, stream);
                }
            }
            puts("");

            if (streamNum == 0 || streamsFound == 0) {
                puts("Not connected to any streams");
                break;
            }
            uint32_t i = 0;
            if (streamsFound == 1) {
                puts("Disconnecting from only stream available");
            } else if (!userIndexFromUser(inputfd, bytes, streamsFound, &i)) {
                continue;
            }

            Message message = { 0 };
            message.tag = MessageTag_disconnectFromStream;
            message.disconnectFromStream = streams.buffer[i].id;

            MESSAGE_SERIALIZE(message, (*bytes));
            socketSend(sockfd, bytes, true);
            MessageFree(&message);
            break;
        }
    });
    StreamListFree(&streams);
}

void
selectStreamToStart(struct pollfd inputfd, const int sockfd, pBytes bytes)
{
    while (!appDone) {
        askQuestion("Select the type of stream to create");
        for (uint32_t i = 0; i < StreamType_Length; ++i) {
            printf("%u) %s\n", i + 1U, StreamTypeToCharString(i));
        }
        puts("");

        uint32_t index;
        if (!userIndexFromUser(inputfd, bytes, StreamType_Length, &index)) {
            continue;
        }

        Message message = { 0 };
        message.tag = MessageTag_startStreaming;
        message.startStreaming.type = (StreamType)index;

        askQuestion("What's the name of the stream?");
        if (!getUserInput(inputfd, bytes, NULL)) {
            continue;
        }
        puts("");

        message.startStreaming.name =
          TemLangStringCreate((char*)bytes->buffer, currentAllocator);

        askQuestion(
          "Do want the stream to be recorded on the server (y or n)?");
        while (!appDone) {
            if (!getUserInput(inputfd, bytes, NULL)) {
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
        puts("");

        // TODO: Allow readers and writers to be set

        MESSAGE_SERIALIZE(message, (*bytes));
        MessageFree(&message);
        socketSend(sockfd, bytes, true);
        break;
    }
}

void
selectStreamToStop(struct pollfd inputfd, const int sockfd, pBytes bytes)
{
    StreamList streams = { 0 };
    IN_MUTEX(clientData.mutex, end0, {
        while (!appDone) {
            const Stream* stream = NULL;
            const uint32_t streamNum = clientData.ownStreams.used;
            uint32_t streamsFound = 0;
            askQuestion("Select a stream to stop");
            StreamListFree(&streams);
            streams.allocator = currentAllocator;
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Guid guid = clientData.ownStreams.buffer[i];
                if (GetStreamFromGuid(
                      &clientData.allStreams, &guid, &stream, NULL)) {
                    printf("%u) %s\n", i + 1U, stream->name.buffer);
                    ++streamsFound;
                    StreamListAppend(&streams, stream);
                }
            }
            puts("");

            if (streamNum == 0 || streamsFound == 0) {
                puts("No streams stop");
                break;
            }

            uint32_t i = 0;
            if (streamsFound == 1) {
                puts("Stopping to only stream available");
            } else if (!userIndexFromUser(inputfd, bytes, streamsFound, &i)) {
                continue;
            }

            Message message = { 0 };
            message.tag = MessageTag_stopStreaming;
            message.stopStreaming = streams.buffer[i].id;

            MESSAGE_SERIALIZE(message, (*bytes));
            MessageFree(&message);

            socketSend(sockfd, bytes, true);
            break;
        }
    });
    StreamListFree(&streams);
}

void
selectStreamToSendDataTo(struct pollfd inputfd, const int sockfd, pBytes bytes)
{
    StreamList streams = { 0 };
    IN_MUTEX(clientData.mutex, end0, {
        while (!appDone) {
            const Stream* stream = NULL;
            const uint32_t streamNum = clientData.connectedStreams.used;
            uint32_t streamsFound = 0;
            askQuestion("Send data to which stream?");
            StreamListFree(&streams);
            streams.allocator = currentAllocator;
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Guid guid = clientData.connectedStreams.buffer[i];
                if (GetStreamFromGuid(
                      &clientData.allStreams, &guid, &stream, NULL)) {
                    printf("%u) %s\n", i + 1U, stream->name.buffer);
                    ++streamsFound;
                    StreamListAppend(&streams, stream);
                }
            }
            puts("");

            if (streamNum == 0 || streamsFound == 0) {
                puts("No streams to send data");
                break;
            }

            uint32_t i = 0;
            if (streamsFound == 1) {
                puts("Sending data to only stream available");
            } else if (!userIndexFromUser(inputfd, bytes, streamsFound, &i)) {
                continue;
            }

            stream = &streams.buffer[i];

            Message message = { 0 };
            switch (stream->type) {
                case StreamType_Text:
                    printf("Enter text to update text stream '%s'\n",
                           stream->name.buffer);
                    if (!getUserInput(inputfd, bytes, NULL)) {
                        continue;
                    }
                    message.streamMessage.data.tag = StreamMessageDataTag_text;
                    message.streamMessage.data.text = TemLangStringCreate(
                      (char*)bytes->buffer, currentAllocator);
                    break;
                case StreamType_Chat:
                    printf("Enter message for chat stream '%s'\n",
                           stream->name.buffer);
                    if (!getUserInput(inputfd, bytes, NULL)) {
                        continue;
                    }
                    message.streamMessage.data.tag =
                      StreamMessageDataTag_chatMessage;
                    message.streamMessage.data.chatMessage.message =
                      TemLangStringCreate((char*)bytes->buffer,
                                          currentAllocator);
                    break;
                default:
                    printf("Cannot manually send data to stream type '%s'\n",
                           StreamTypeToCharString(stream->type));
                    continue;
            }

            message.streamMessage.id = stream->id;
            message.tag = MessageTag_streamMessage;
            MESSAGE_SERIALIZE(message, (*bytes));
            MessageFree(&message);

            socketSend(sockfd, bytes, true);
            break;
        }
    });
    StreamListFree(&streams);
}

int
handleUserInput(const void* ptr)
{
    (void)ptr;
    const int sockfd = clientData.tcpSocket;
    struct pollfd inputfd = { .events = POLLIN,
                              .revents = 0,
                              .fd = STDIN_FILENO };
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };
    Message message = { 0 };
    while (!appDone) {
        askQuestion("Choose an option");
        for (size_t i = 0; i < ClientCommand_Length; ++i) {
            TemLangString s = ClientCommandToString(i);
            camelCaseToNormal(&s);
            printf("%zu) %s\n", i + 1UL, s.buffer);
            TemLangStringFree(&s);
        }
        puts("");

        uint32_t index;
        if (!userIndexFromUser(inputfd, &bytes, ClientCommand_Length, &index)) {
            continue;
        }

        MessageFree(&message);
        switch (index) {
            case ClientCommand_PrintName:
                IN_MUTEX(clientData.mutex, printName, {
                    printf("Client name: %s\n", clientData.name.buffer);
                });
                continue;
            case ClientCommand_GetClients:
                message.tag = MessageTag_getClients;
                message.getClients = NULL;
                MESSAGE_SERIALIZE(message, bytes);
                if (!socketSend(sockfd, &bytes, true)) {
                    continue;
                }
                break;
            case ClientCommand_Quit:
                appDone = true;
                continue;
            case ClientCommand_ConnectToStream:
            case ClientCommand_ShowAllStreams:
            case ClientCommand_ShowConnectedStreams:
            case ClientCommand_ShowOwnStreams:
                if (!refreshStreams(sockfd, &bytes)) {
                    appDone = true;
                    continue;
                }
                break;
            case ClientCommand_DisconnectFromStream:
                if (!refreshStreams(sockfd, &bytes)) {
                    continue;
                }
                break;
            case ClientCommand_StartStreaming: {
                selectStreamToStart(inputfd, sockfd, &bytes);
                continue;
            } break;
            case ClientCommand_StopStreaming:
                if (!refreshStreams(sockfd, &bytes)) {
                    continue;
                }
                break;
            case ClientCommand_SendStreamData:
                if (!refreshStreams(sockfd, &bytes)) {
                    continue;
                }
                break;
            default:
                fprintf(stderr,
                        "Command '%s' is not implemented\n",
                        ClientCommandToCharString(index));
                continue;
        }

        puts("Waiting for stream information from server...");
        bool gotData = false;
        IN_MUTEX(clientData.mutex, end, {
            while (!appDone) {
                const int result = SDL_CondWaitTimeout(
                  clientData.cond, clientData.mutex, LONG_POLL_WAIT);
                if (result < 0) {
                    fprintf(stderr, "Cond error: %s\n", SDL_GetError());
                    break;
                } else if (result == 0) {
                    gotData = true;
                    break;
                } else {
                    puts("Failed to get information from server");
                    break;
                }
            }
        });
        if (!gotData) {
            continue;
        }
        puts("Got stream information from server");

        switch (index) {
            case ClientCommand_GetClients:
                IN_MUTEX(clientData.mutex, endClients, {
                    askQuestion("Clients");
                    for (size_t i = 0; i < clientData.otherClients.used; ++i) {
                        printf("%zu) %s\n",
                               i + 1,
                               clientData.otherClients.buffer[i].buffer);
                    }
                    puts("");
                });
                break;
            case ClientCommand_ConnectToStream:
                selectAStreamToConnectTo(inputfd, sockfd, &bytes);
                break;
            case ClientCommand_DisconnectFromStream:
                selectAStreamToDisconnectFrom(inputfd, sockfd, &bytes);
                break;
            case ClientCommand_StopStreaming:
                selectStreamToStop(inputfd, sockfd, &bytes);
                break;
            case ClientCommand_SendStreamData:
                selectStreamToSendDataTo(inputfd, sockfd, &bytes);
                break;
            case ClientCommand_ShowAllStreams:
                askQuestion("All Streams");
                IN_MUTEX(clientData.mutex, endShowAll, {
                    for (size_t i = 0; i < clientData.allStreams.used; ++i) {
                        printStream(&clientData.allStreams.buffer[i]);
                    }
                });
                break;
            case ClientCommand_ShowConnectedStreams:
                askQuestion("Connected Streams");
                IN_MUTEX(clientData.mutex, endShowConnected, {
                    const Stream* stream = NULL;
                    for (size_t i = 0; i < clientData.connectedStreams.used;
                         ++i) {
                        if (GetStreamFromGuid(
                              &clientData.allStreams,
                              &clientData.connectedStreams.buffer[i],
                              &stream,
                              NULL)) {
                            printStream(stream);
                        }
                    }
                });
                break;
            case ClientCommand_ShowOwnStreams:
                askQuestion("Own Streams");
                IN_MUTEX(clientData.mutex, endShowOwn, {
                    const Stream* stream = NULL;
                    for (size_t i = 0; i < clientData.ownStreams.used; ++i) {
                        if (GetStreamFromGuid(&clientData.allStreams,
                                              &clientData.ownStreams.buffer[i],
                                              &stream,
                                              NULL)) {
                            printStream(stream);
                        }
                    }
                });
                break;
            default:
                continue;
        }
        MessageFree(&message);
    }
    MessageFree(&message);
    uint8_tListFree(&bytes);
    return EXIT_SUCCESS;
}

void
clientHandleMessage(pMessage message, const int sockfd, pBytes bytes)
{
    IN_MUTEX(clientData.mutex, end, {
        bool doSignal = false;
        switch (message->tag) {
            case MessageTag_prepareForData: {
                // Don't lock mutex while waiting for server data
                SDL_UnlockMutex(clientData.mutex);
                if (readAllData(
                      sockfd, message->prepareForData, message, bytes)) {
                    clientHandleMessage(message, sockfd, bytes);
                }
                SDL_LockMutex(clientData.mutex);
            } break;
            case MessageTag_authenticateAck:
                printf("Client name: %s\n",
                       message->authenticateAck.name.buffer);
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
                doSignal = true;
                cleanupStreamDisplays();
                puts("Updated server data");
            } break;
            case MessageTag_getClientsAck:
                TemLangStringListCopy(&clientData.otherClients,
                                      &message->getClientsAck,
                                      currentAllocator);
                doSignal = true;
                puts("Updated client list");
                break;
            case MessageTag_getConnectedStreamsAck:
                GuidListCopy(&clientData.connectedStreams,
                             &message->getConnectedStreamsAck,
                             currentAllocator);
                doSignal = true;
                cleanupStreamDisplays();
                puts("Updated connected streams");
                break;
            case MessageTag_getClientStreamsAck:
                GuidListCopy(&clientData.ownStreams,
                             &message->getClientStreamsAck,
                             currentAllocator);
                doSignal = true;
                puts("Updated client's streams");
                break;
            case MessageTag_getAllStreamsAck:
                StreamListCopy(&clientData.allStreams,
                               &message->getAllStreamsAck,
                               currentAllocator);
                doSignal = true;
                puts("Updated stream list");
                break;
            case MessageTag_startStreamingAck:
                switch (message->startStreamingAck.tag) {
                    case OptionalGuidTag_none:
                        puts("Server failed to start stream");
                        break;
                    default:
                        message->tag = MessageTag_getClientStreams;
                        message->getClientStreams = NULL;
                        MESSAGE_SERIALIZE((*message), (*bytes));
                        socketSend(sockfd, bytes, true);
                        break;
                }
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
                        socketSend(sockfd, bytes, true);
                        break;
                }
                refreshStreams(sockfd, bytes);
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
                            case StreamMessageDataTag_videoChunk:
                            case StreamMessageDataTag_audioChunk:
                            case StreamMessageDataTag_image: {
                                e.user.code = CustomEvent_UpdateStreamDisplay;
                                pStreamMessage m = currentAllocator->allocate(
                                  sizeof(StreamMessage));
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
                refreshStreams(sockfd, bytes);
                break;
            case MessageTag_streamMessage:
                switch (message->streamMessage.data.tag) {
                    case StreamMessageDataTag_text:
                    case StreamMessageDataTag_chatLogs:
                    case StreamMessageDataTag_chatMessage:
                    case StreamMessageDataTag_videoChunk:
                    case StreamMessageDataTag_audioChunk:
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
        if (doSignal) {
            SDL_CondSignal(clientData.cond);
        }
    });
}

void
readSocket(const int sockfd, pBytes bytes)
{
    const ssize_t size = recv(sockfd, bytes->buffer, bytes->size, 0);
    switch (size) {
        case -1:
            perror("recv");
            return;
        case 0:
            return;
        default:
            break;
    }

    bytes->used = (uint32_t)size;
    Message message = { 0 };
    MESSAGE_DESERIALIZE(message, (*bytes));
    clientHandleMessage(&message, sockfd, bytes);
    MessageFree(&message);
}

void
updateTextDisplay(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  const Guid* id,
                  const TemLangString* string)
{
    IN_MUTEX(clientData.mutex, endMutex, {
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

        SDL_Color fg = { 0 };
        fg.r = fg.g = fg.b = fg.a = 0xffu;
        SDL_Color bg = { 0 };
        bg.a = 128u;
        surface =
          TTF_RenderUTF8_Shaded_Wrapped(ttfFont, string->buffer, fg, bg, 0);
        if (surface == NULL) {
            fprintf(
              stderr, "Failed to create text surface: %s\n", TTF_GetError());
            goto end;
        }

        display->srcRect.tag = OptionalRectTag_none;
        display->srcRect.none = NULL;

        display->dstRect.x = 0.f;
        display->dstRect.y = 0.f;
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
    });
}

void
updateImageDisplay(SDL_Renderer* renderer, const Guid* id, const Bytes* bytes)
{
    IN_MUTEX(clientData.mutex, endMutex, {
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
            fprintf(
              stderr, "Failed to load image memory: %s\n", SDL_GetError());
            goto end;
        }
        surface = IMG_Load_RW(rw, 0);
        if (surface == NULL) {
            fprintf(stderr, "Failed to load image: %s\n", SDL_GetError());
            goto end;
        }

        display->srcRect.tag = OptionalRectTag_none;
        display->srcRect.none = NULL;

        display->dstRect.x = 0.f;
        display->dstRect.y = 0.f;
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
    });
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

    SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 0x0u);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 0xffu);

    display->data.tag = StreamDisplayDataTag_chat;
    char buffer[128] = { 0 };

    const SDL_Color white = { .r = 0xffu, .g = 0xffu, .b = 0xffu, .a = 0xffu };
    const SDL_Color purple = { .r = 0xffu, .g = 0x0u, .b = 0xffu, .a = 0xffu };
    const SDL_Color yellow = { .r = 0xffu, .g = 0xffu, .b = 0x0u, .a = 0xffu };
    const SDL_Color bg = { .r = 0u, .g = 0u, .b = 0u, .a = 128u };
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
    IN_MUTEX(clientData.mutex, endMutex, {
        size_t i = 0;
        if (!GetStreamDisplayFromGuid(&clientData.displays, id, NULL, &i)) {
            puts("Missing stream display for chat stream");
            goto endMutex;
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
    });
}

void
updateChatDisplayFromMessage(SDL_Renderer* renderer,
                             TTF_Font* ttfFont,
                             const uint32_t w,
                             const uint32_t h,
                             const Guid* id,
                             const ChatMessage* message)
{
    IN_MUTEX(clientData.mutex, endMutex, {
        size_t i = 0;
        if (!GetStreamDisplayFromGuid(&clientData.displays, id, NULL, &i)) {
            puts("Missing stream display for chat stream");
            goto endMutex;
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
        updateChatDisplay(renderer, ttfFont, w, h, display);
    });
}

int
readFromServer(struct AllConfiguration* configuration)
{
    puts("Connecting to server...");

    int result = EXIT_FAILURE;
    SDL_Thread* thread = NULL;
    const ClientConfiguration* config = &configuration->configuration.client;
    Bytes bytes = { .allocator = currentAllocator,
                    .size = MAX_PACKET_SIZE,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .used = 0 };
    clientData.tcpSocket =
      openSocketFromAddress(&configuration->address, SocketOptions_Tcp);
    clientData.udpSocket = openSocketFromAddress(&configuration->address, 0);
    if (clientData.tcpSocket == INVALID_SOCKET ||
        clientData.udpSocket == INVALID_SOCKET) {
        goto end;
    }

    struct pollfd pfdsList[] = { { .events = POLLIN | POLLERR | POLLHUP,
                                   .revents = 0,
                                   .fd = clientData.tcpSocket },
                                 { .events = POLLIN | POLLERR | POLLHUP,
                                   .revents = 0,
                                   .fd = clientData.udpSocket } };
    const size_t len = sizeof(pfdsList) / sizeof(struct pollfd);

    {
        Message message = { 0 };
        message.tag = MessageTag_authenticate;
        message.authenticate = config->authentication;
        MESSAGE_SERIALIZE(message, bytes);
        if (!socketSend(clientData.tcpSocket, &bytes, true)) {
            goto end;
        }
        puts("Waiting for server authorization...");
        switch (poll(pfdsList, 1, 5000U)) {
            case -1:
                perror("poll");
                appDone = true;
                goto end;
            case 0:
                puts("Server failed to authorize!");
                appDone = true;
                goto end;
            default:
                puts("Server has authorized");
                break;
        }
    }

    thread =
      SDL_CreateThread((SDL_ThreadFunction)handleUserInput, "Input", NULL);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto end;
    }

    while (!appDone) {
        switch (poll(pfdsList, len, CLIENT_POLL_WAIT)) {
            case -1:
                perror("poll");
                appDone = true;
                continue;
            case 0:
                break;
            default:
                for (size_t i = 0; i < len; ++i) {
                    struct pollfd pfds = pfdsList[i];
                    if ((pfds.revents & (POLLERR | POLLHUP)) != 0) {
                        puts("Connection to server lost!");
                        IN_MUTEX(clientData.mutex, dfsa, {
                            SDL_CondSignal(clientData.cond);
                        });
                        appDone = true;
                        break;
                    }
                    if ((pfds.revents & POLLIN) != 0) {
                        readSocket(pfds.fd, &bytes);
                    }
                }
                break;
        }
    }
    result = EXIT_SUCCESS;

end:
    appDone = true;
    puts("Closing server connection...");
    IN_MUTEX(clientData.mutex, dfsa2, { SDL_CondSignal(clientData.cond); });
    SDL_WaitThread(thread, &result);
    uint8_tListFree(&bytes);
    puts("Closed server connection");
    return result;
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

    IN_MUTEX(clientData.mutex, end, {
        if (target < clientData.displays.used) {
            const StreamDisplay* display = &clientData.displays.buffer[target];
            if (display->texture != NULL) {
                SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0x0u, 0xffu);
                const SDL_FRect rect = expandRect(
                  (const SDL_FRect*)&display->dstRect, 1.025f, 1.025f);
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
    });

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

int
runClient(const AllConfiguration* configuration)
{
    int result = EXIT_FAILURE;
    puts("Running client");
    printAllConfiguration(configuration);

    const ClientConfiguration* config = &configuration->configuration.client;

    SDL_Thread* thread = NULL;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    // Font font = { 0 };
    TTF_Font* ttfFont = NULL;

    Message message = { 0 };
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
        // const uint32_t flags =
        //   IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP;
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

    clientData.cond = SDL_CreateCond();
    if (clientData.cond == NULL) {
        fprintf(stderr, "Failed to create cond: %s\n", SDL_GetError());
        goto end;
    }

    clientData.mutex = SDL_CreateMutex();
    if (clientData.mutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
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

    thread = SDL_CreateThread(
      (SDL_ThreadFunction)readFromServer, "Read", (void*)configuration);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto end;
    }

    SDL_Event e = { 0 };
    size_t targetDisplay = UINT32_MAX;
    MoveMode moveMode = MoveMode_None;
    appDone = false;
    while (!appDone) {
        if (appDone || SDL_WaitEventTimeout(&e, CLIENT_POLL_WAIT) == 0) {
            continue;
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
                IN_MUTEX(clientData.mutex, endMouseButton, {
                    SDL_FPoint point = { 0 };
                    point.x = e.button.x;
                    point.y = e.button.y;
                    for (size_t i = 0; i < clientData.displays.used; ++i) {
                        pStreamDisplay display = &clientData.displays.buffer[i];
                        if (!SDL_PointInFRect(
                              &point, (const SDL_FRect*)&display->dstRect)) {
                            continue;
                        }
                        targetDisplay = i;
                        SDL_SetWindowGrab(window, true);
                        renderDisplays();
                        goto endMouseButton;
                    }
                });
            } break;
            case SDL_MOUSEBUTTONUP: {
                targetDisplay = UINT32_MAX;
                SDL_SetWindowGrab(window, false);
                renderDisplays();
            } break;
            case SDL_MOUSEMOTION: {
                IN_MUTEX(clientData.mutex, endMotion, {
                    if (targetDisplay >= clientData.displays.used) {
                        goto endMotion;
                    }
                    pStreamDisplay display =
                      &clientData.displays.buffer[targetDisplay];
                    if (display->texture == NULL) {
                        goto endMotion;
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
                });
            } break;
            case SDL_MOUSEWHEEL: {
                IN_MUTEX(clientData.mutex, endWheel, {
                    if (targetDisplay >= clientData.displays.used) {
                        goto endWheel;
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
                });
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
                    case CustomEvent_AddTexture: {
                        StreamDisplay s = { 0 };
                        s.dstRect.x = 0.f;
                        s.dstRect.y = 0.f;
                        s.dstRect.w = (float)w;
                        s.dstRect.h = (float)h;
                        s.id = *(Guid*)e.user.data1;
                        // Create texture once data is received
                        IN_MUTEX(clientData.mutex, addTextureEnd, {
                            StreamDisplayListAppend(&clientData.displays, &s);
                        });
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
                        currentAllocator->free(m);
                        renderDisplays();
                    } break;
                    default:
                        break;
                }
            } break;
            case SDL_DROPFILE: {
                // Look for image stream
                puts("Got dropped file");
                int fd = 0;
                char* ptr = NULL;
                size_t size = 0;
                if (!mapFile(e.drop.file, &fd, &ptr, &size, MapFileType_Read)) {
                    fprintf(stderr,
                            "Error opening file '%s': %s\n",
                            e.drop.file,
                            strerror(errno));
                    goto endDropFile2;
                }
                Bytes fileBytes = { .allocator = NULL,
                                    .buffer = (uint8_t*)ptr,
                                    .size = size,
                                    .used = size };
                IN_MUTEX(clientData.mutex, endDropFile, {
                    const GuidList* streams = &clientData.connectedStreams;
                    const Stream* stream = NULL;
                    for (size_t i = 0; i < streams->used; ++i) {
                        if (!GetStreamFromGuid(&clientData.allStreams,
                                               &streams->buffer[i],
                                               &stream,
                                               NULL) ||
                            stream->type != StreamType_Image) {
                            printf("%d\n", stream->type);
                            continue;
                        }
                        MessageFree(&message);
                        message.tag = MessageTag_streamMessage;
                        message.streamMessage.id = stream->id;
                        message.streamMessage.data.tag =
                          StreamMessageDataTag_image;
                        message.streamMessage.data.image = fileBytes;
                        MESSAGE_SERIALIZE(message, bytes);
                        sendPrepareMessage(clientData.tcpSocket, bytes.used);
                        socketSend(clientData.tcpSocket, &bytes, true);
                        // Don't free the image bytes since they weren't
                        // allocated
                        memset(&message, 0, sizeof(message));
                        goto endDropFile;
                    }
                    puts("No stream to send image too...");
                });
            endDropFile2:
                unmapFile(fd, ptr, size);
                SDL_free(e.drop.file);
            } break;
            case SDL_DROPTEXT:
                // Look for a text or chat stream
                puts("Got dropped text");
                IN_MUTEX(clientData.mutex, endDropText, {
                    const GuidList* streams = &clientData.connectedStreams;
                    const Stream* stream = NULL;
                    for (size_t i = 0; i < streams->used; ++i) {
                        if (!GetStreamFromGuid(&clientData.allStreams,
                                               &streams->buffer[i],
                                               &stream,
                                               NULL)) {
                            continue;
                        }
                        switch (stream->type) {
                            case StreamType_Text: {
                                MessageFree(&message);
                                message.tag = MessageTag_streamMessage;
                                message.streamMessage.id = stream->id;
                                message.streamMessage.data.tag =
                                  StreamMessageDataTag_text;
                                message.streamMessage.data.text =
                                  TemLangStringCreate(e.drop.file,
                                                      currentAllocator);
                                MESSAGE_SERIALIZE(message, bytes);
                                socketSend(clientData.tcpSocket, &bytes, true);
                                goto endDropText;
                            }
                            case StreamType_Chat: {
                                MessageFree(&message);
                                message.tag = MessageTag_streamMessage;
                                message.streamMessage.id = stream->id;
                                message.streamMessage.data.tag =
                                  StreamMessageDataTag_chatMessage;
                                message.streamMessage.data.chatMessage.message =
                                  TemLangStringCreate(e.drop.file,
                                                      currentAllocator);
                                MESSAGE_SERIALIZE(message, bytes);
                                socketSend(clientData.tcpSocket, &bytes, true);
                                goto endDropText;
                            }
                            default:
                                break;
                        }
                    }
                    puts("No stream to send text too...");
                });
                SDL_free(e.drop.file);
                break;
            default:
                break;
        }
    }

end:
    // FontFree(&font);
    MessageFree(&message);
    uint8_tListFree(&bytes);
    TTF_CloseFont(ttfFont);
    TTF_Quit();
    SDL_WaitThread(thread, &result);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    ClientDataFree(&clientData);
    return result;
}