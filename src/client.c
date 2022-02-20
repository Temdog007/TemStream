#include <include/main.h>

ClientData clientData = { 0 };

bool
refreshStreams(const int, pBytes);

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
        .fontSize = 24,
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

#define EVENT_RENDER 0x31ab

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
        switch (poll(&inputfd, 1, POLL_WAIT)) {
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

            uint32_t i;
            if (!userIndexFromUser(inputfd, bytes, streamNum, &i)) {
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
    IN_MUTEX(clientData.mutex, end0, {
        while (!appDone) {
            const uint32_t streamNum = clientData.connectedStreams.used;
            askQuestion("Select a stream to disconnect from");
            const Stream* stream = NULL;
            uint32_t streamsFound = 0;
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Guid guid = clientData.connectedStreams.buffer[i];
                if (GetStreamFromGuid(
                      &clientData.allStreams, &guid, &stream, NULL)) {
                    printf("%u) %s\n", i + 1U, stream->name.buffer);
                    ++streamsFound;
                }
            }
            puts("");

            if (streamNum == 0 || streamsFound == 0) {
                puts("Not connected to any streams");
                break;
            }
            uint32_t i;
            if (!userIndexFromUser(inputfd, bytes, streamNum, &i)) {
                continue;
            }

            Message message = { 0 };
            message.tag = MessageTag_disconnectFromStream;
            message.disconnectFromStream =
              clientData.connectedStreams.buffer[i];

            MESSAGE_SERIALIZE(message, (*bytes));
            socketSend(sockfd, bytes, true);
            MessageFree(&message);
            break;
        }
    });
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
    IN_MUTEX(clientData.mutex, end0, {
        while (!appDone) {
            const Stream* stream = NULL;
            const uint32_t streamNum = clientData.ownStreams.used;
            uint32_t streamsFound = 0;
            askQuestion("Select a stream to stop");
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Guid guid = clientData.ownStreams.buffer[i];
                if (GetStreamFromGuid(
                      &clientData.allStreams, &guid, &stream, NULL)) {
                    printf("%u) %s\n", i + 1U, stream->name.buffer);
                    ++streamsFound;
                }
            }
            puts("");

            if (streamNum == 0 || streamsFound == 0) {
                puts("No streams stop");
                break;
            }

            uint32_t index;
            if (!userIndexFromUser(inputfd, bytes, streamNum, &index)) {
                continue;
            }

            Message message = { 0 };
            message.tag = MessageTag_stopStreaming;
            message.stopStreaming = clientData.ownStreams.buffer[index];

            MESSAGE_SERIALIZE(message, (*bytes));
            MessageFree(&message);

            socketSend(sockfd, bytes, true);
            break;
        }
    });
}

int
handleUserInput(const int* sockfdPtr)
{
    const int sockfd = *sockfdPtr;
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
                MESSAGE_SERIALIZE(message, bytes);
                if (!socketSend(sockfd, &bytes, true)) {
                    continue;
                }
                break;
            case ClientCommand_Quit:
                appDone = true;
                continue;
            case ClientCommand_ConnectToStream:
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
                const int result =
                  SDL_CondWaitTimeout(clientData.cond, clientData.mutex, 5000U);
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
readSocket(const int sockfd, pBytes bytes)
{
    const ssize_t size = recv(sockfd, bytes->buffer, bytes->size, 0);
    if (size <= 0) {
        return;
    }

    bytes->used = (uint32_t)size;
    Message message = { 0 };
    MESSAGE_DESERIALIZE(message, (*bytes));
    IN_MUTEX(clientData.mutex, end, {
        bool doSignal = false;
        switch (message.tag) {
            case MessageTag_authenticateAck:
                printf("Client name: %s\n",
                       message.authenticateAck.name.buffer);
                TemLangStringCopy(&clientData.name,
                                  &message.authenticateAck.name,
                                  currentAllocator);
                clientData.id = message.authenticateAck.id;
                break;
            case MessageTag_getAllDataAck:
                GuidListCopy(&clientData.connectedStreams,
                             &message.getAllDataAck.connectedStreams,
                             currentAllocator);
                GuidListCopy(&clientData.ownStreams,
                             &message.getAllDataAck.clientStreams,
                             currentAllocator);
                StreamListCopy(&clientData.allStreams,
                               &message.getAllDataAck.allStreams,
                               currentAllocator);
                doSignal = true;
                puts("Updated server data");
                break;
            case MessageTag_getClientsAck:
                TemLangStringListCopy(&clientData.otherClients,
                                      &message.getClientsAck,
                                      currentAllocator);
                doSignal = true;
                puts("Updated client list");
                break;
            case MessageTag_getConnectedStreamsAck:
                GuidListCopy(&clientData.connectedStreams,
                             &message.getConnectedStreamsAck,
                             currentAllocator);
                doSignal = true;
                puts("Updated connected streams");
                break;
            case MessageTag_getClientStreamsAck:
                GuidListCopy(&clientData.ownStreams,
                             &message.getClientStreamsAck,
                             currentAllocator);
                doSignal = true;
                puts("Updated client's streams");
                break;
            case MessageTag_getAllStreamsAck:
                StreamListCopy(&clientData.allStreams,
                               &message.getAllStreamsAck,
                               currentAllocator);
                doSignal = true;
                puts("Updated stream list");
                break;
            case MessageTag_startStreamingAck:
                switch (message.startStreamingAck.tag) {
                    case OptionalGuidTag_none:
                        puts("Server failed to start stream");
                        break;
                    default:
                        message.tag = MessageTag_getClientStreams;
                        message.getClientStreams = NULL;
                        MESSAGE_SERIALIZE(message, (*bytes));
                        socketSend(sockfd, bytes, true);
                        break;
                }
                break;
            case MessageTag_stopStreamingAck:
                switch (message.stopStreamingAck.tag) {
                    case OptionalGuidTag_none:
                        puts("Server failed to stop stream");
                        break;
                    default:
                        message.tag = MessageTag_getClientStreams;
                        message.getClientStreams = NULL;
                        MESSAGE_SERIALIZE(message, (*bytes));
                        socketSend(sockfd, bytes, true);
                        break;
                }
                break;
            case MessageTag_connectToStreamAck:
                switch (message.connectToStreamAck.tag) {
                    case OptionalStreamStorageTag_streamStorage:
                        puts("Connected to stream");
                        switch (
                          message.connectToStreamAck.streamStorage.data.tag) {
                            default:
                                break;
                        }
                    default:
                        puts("Failed to connect to stream");
                        break;
                }
                break;
            case MessageTag_disconnectFromStreamAck:
                if (message.disconnectFromStreamAck) {
                    puts("Disconnected from stream");
                } else {
                    puts("Failed to disconnect from stream");
                }
                break;
            default:
                printf("Unexpected message: %s\n",
                       MessageTagToCharString(message.tag));
                break;
        }
        if (doSignal) {
            SDL_CondSignal(clientData.cond);
        }
    });
    MessageFree(&message);
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
    const int tcpListener =
      openSocketFromAddress(&configuration->address, SocketOptions_Tcp);
    const int udpListener = openSocketFromAddress(&configuration->address, 0);
    if (tcpListener == INVALID_SOCKET || udpListener == INVALID_SOCKET) {
        goto end;
    }

    struct pollfd pfdsList[] = { { .events = POLLIN | POLLERR | POLLHUP,
                                   .revents = 0,
                                   .fd = tcpListener },
                                 { .events = POLLIN | POLLERR | POLLHUP,
                                   .revents = 0,
                                   .fd = udpListener } };
    const size_t len = sizeof(pfdsList) / sizeof(struct pollfd);

    {
        Message message = { 0 };
        message.tag = MessageTag_authenticate;
        message.authenticate = config->authentication;
        MESSAGE_SERIALIZE(message, bytes);
        if (!socketSend(tcpListener, &bytes, true)) {
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

    thread = SDL_CreateThread(
      (SDL_ThreadFunction)handleUserInput, "Input", (void*)&tcpListener);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto end;
    }

    while (!appDone) {
        switch (poll(pfdsList, len, POLL_WAIT)) {
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
    closeSocket(udpListener);
    closeSocket(tcpListener);
    puts("Closed server connection");
    return result;
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

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    {
        // const uint32_t flags =
        //   IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP;
        const uint32_t flags = IMG_INIT_PNG;
        if (IMG_Init(flags) != flags) {
            fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
            goto end;
        }
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

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        goto end;
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

    SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 0xffu);

    thread = SDL_CreateThread(
      (SDL_ThreadFunction)readFromServer, "Read", (void*)configuration);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto end;
    }

    SDL_Event e = { 0 };
    appDone = false;
    while (!appDone) {
        while (!appDone && SDL_WaitEventTimeout(&e, POLL_WAIT)) {
            switch (e.type) {
                case SDL_QUIT:
                    appDone = true;
                    break;
                case SDL_USEREVENT:
                    switch (e.user.code) {
                        case EVENT_RENDER: {
                            SDL_RenderClear(renderer);
                            SDL_RenderPresent(renderer);
                        } break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
    }

end:
    SDL_WaitThread(thread, &result);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    ClientDataFree(&clientData);
    return result;
}