#include <include/main.h>

ClientData clientData = { 0 };

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
            TemLangStringInsertChar(str, ' ', c);
        }
    }
}

void
selectAStreamToConnectTo(struct pollfd inputfd, const int sockfd, pBytes bytes)
{
    while (!appDone) {
        uint32_t streamNum;
        IN_MUTEX(clientData.mutex, end0, {
            streamNum = clientData.allStreams.used;
            puts("Select a stream");
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Stream* stream = &clientData.allStreams.buffer[i];
                printf("%u) %s\n", i + 1U, stream->name.buffer);
            }
        });
        if (streamNum == 0) {
            puts("No streams to connect to");
            break;
        }
        while (!appDone) {
            switch (poll(&inputfd, 1, 1000)) {
                case -1:
                    perror("poll");
                    return;
                case 0:
                    continue;
                default:
                    goto readInput;
            }
        }

    readInput:
        if ((inputfd.revents & POLLIN) == 0) {
            continue;
        }

        const ssize_t size = read(STDIN_FILENO, bytes->buffer, bytes->size);
        if (size <= 0) {
            continue;
        }
        bytes->buffer[size] = '\0';
        char* end = NULL;
        const uint32_t index =
          (uint32_t)strtoul((const char*)bytes->buffer, &end, 10) - 1UL;
        if (end != (char*)&bytes->buffer[size] || index >= streamNum) {
            printf("Enter a number between 1 and %u\n", streamNum);
            continue;
        }

        Message message = { 0 };
        message.tag = MessageTag_connectToStream;
        IN_MUTEX(clientData.mutex, end1, {
            message.connectToStream = clientData.allStreams.buffer[index].id;
        });
        bytes->used = 0;
        MessageSerialize(&message, bytes, true);
        if (send(sockfd, bytes->buffer, bytes->used, 0) !=
            (ssize_t)bytes->used) {
            perror("send");
        }
        MessageFree(&message);
        break;
    }
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd,
                              const int sockfd,
                              pBytes bytes)
{
    while (!appDone) {
        uint32_t streamNum;
        IN_MUTEX(clientData.mutex, end0, {
            streamNum = clientData.ownStreams.used;
            puts("Select a stream");
            const Stream* stream = NULL;
            for (uint32_t i = 0; i < streamNum; ++i) {
                const Guid guid = clientData.ownStreams.buffer[i];
                GetStreamFromGuid(&clientData.allStreams, &guid, &stream, NULL);
                printf("%u) %s\n", i + 1U, stream->name.buffer);
            }
        });
        if (streamNum == 0) {
            puts("Not connected to any streams");
            break;
        }
        while (!appDone) {
            switch (poll(&inputfd, 1, 1000)) {
                case -1:
                    perror("poll");
                    return;
                case 0:
                    continue;
                default:
                    goto readInput;
            }
        }

    readInput:
        if ((inputfd.revents & POLLIN) == 0) {
            continue;
        }

        const ssize_t size = read(STDIN_FILENO, bytes->buffer, bytes->size);
        if (size <= 0) {
            continue;
        }
        bytes->buffer[size] = '\0';
        char* end = NULL;
        const uint32_t index =
          (uint32_t)strtoul((const char*)bytes->buffer, &end, 10) - 1UL;
        if (end != (char*)&bytes->buffer[size] || index >= streamNum) {
            printf("Enter a number between 1 and %u\n", streamNum);
            continue;
        }

        Message message = { 0 };
        message.tag = MessageTag_disconnectFromStream;
        IN_MUTEX(clientData.mutex, end1, {
            message.disconnectFromStream = clientData.ownStreams.buffer[index];
        });
        bytes->used = 0;
        MessageSerialize(&message, bytes, true);
        if (send(sockfd, bytes->buffer, bytes->used, 0) !=
            (ssize_t)bytes->used) {
            perror("send");
        }
        MessageFree(&message);
        break;
    }
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
        puts("Choose an option");
        for (size_t i = 0; i < MainMenu_Length; ++i) {
            TemLangString s = MainMenuToString(i);
            camelCaseToNormal(&s);
            printf("%zu) %s\n", i + 1UL, s.buffer);
            TemLangStringFree(&s);
        }
        while (!appDone) {
            switch (poll(&inputfd, 1, 1000)) {
                case -1:
                    perror("poll");
                    return EXIT_FAILURE;
                case 0:
                    continue;
                default:
                    goto readInput;
            }
        }

    readInput:
        if ((inputfd.revents & POLLIN) == 0) {
            continue;
        }

        const ssize_t size = read(STDIN_FILENO, bytes.buffer, bytes.size);
        if (size <= 0) {
            continue;
        }
        bytes.buffer[size] = '\0';
        char* end = NULL;
        const uint32_t index =
          (uint32_t)strtoul((const char*)bytes.buffer, &end, 10) - 1UL;
        if (end != (char*)&bytes.buffer[size - 1] || index >= MainMenu_Length) {
            printf("Enter a number between 1 and %d\n", MainMenu_Length);
            continue;
        }

        MessageFree(&message);
        switch (index) {
            case MainMenu_ConnectToStream: {
                message.tag = MessageTag_getAllStreams;
                message.getAllStreams = NULL;
            } break;
            case MainMenu_DisconnectFromStream: {
                message.tag = MessageTag_getConnectedStreams;
                message.getAllStreams = NULL;
            } break;
            case MainMenu_StartStreaming: {
                fprintf(stderr, "Streaming not implemented\n");
                continue;
            } break;
            case MainMenu_StopStreaming: {
                fprintf(stderr, "Streaming not implemented\n");
                continue;
            } break;
            default:
                continue;
        }
        bytes.used = 0;
        MessageSerialize(&message, &bytes, true);
        if (send(sockfd, bytes.buffer, bytes.used, 0) != (ssize_t)bytes.used) {
            perror("send");
            continue;
        }

        IN_MUTEX(clientData.mutex, end, {
            while (!appDone) {
                const int result =
                  SDL_CondWaitTimeout(clientData.cond, clientData.mutex, 1000U);
                if (result < 0) {
                    fprintf(stderr, "Cond error: %s\n", SDL_GetError());
                    break;
                } else if (result == 0) {
                    break;
                }
            }
        });

        switch (index) {
            case MainMenu_ConnectToStream:
                selectAStreamToConnectTo(inputfd, sockfd, &bytes);
                break;
            case MainMenu_DisconnectFromStream:
                selectAStreamToDisconnectFrom(inputfd, sockfd, &bytes);
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
    MessageDeserialize(&message, bytes, 0, true);
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
                doSignal = true;
                break;
            case MessageTag_getConnectedStreamsAck:
                GuidListCopy(&clientData.ownStreams,
                             &message.getClientStreamsAck,
                             currentAllocator);
                doSignal = true;
                break;
            case MessageTag_getAllStreamsAck:
                StreamListCopy(&clientData.allStreams,
                               &message.getAllStreamsAck,
                               currentAllocator);
                doSignal = true;
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
runClient(const AllConfiguration* configuration)
{
    int result = EXIT_FAILURE;
    puts("Running client");
    printAllConfiguration(configuration);

    const ClientConfiguration* config = &configuration->configuration.client;

    SDL_Thread* thread = NULL;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
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

    {
        Message message = { 0 };
        message.tag = MessageTag_authenticate;
        message.authenticate = config->authentication;
        bytes.used = 0;
        MessageSerialize(&message, &bytes, true);
        if (send(tcpListener, bytes.buffer, bytes.used, 0) !=
            (ssize_t)bytes.used) {
            perror("send");
            goto end;
        }
    }

    struct pollfd pfdsList[] = {
        { .events = POLLIN, .revents = 0, .fd = tcpListener },
        { .events = POLLIN, .revents = 0, .fd = udpListener }
    };
    const size_t len = sizeof(pfdsList) / sizeof(struct pollfd);

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

    window =
      SDL_CreateWindow("TemStream Client",
                       SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED,
                       config->windowWidth,
                       config->windowHeight,
                       config->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
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
      (SDL_ThreadFunction)handleUserInput, "Input", (void*)&tcpListener);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto end;
    }

    SDL_Event e = { 0 };
    appDone = false;
    while (!appDone) {
        switch (poll(pfdsList, len, 16)) {
            case -1:
                perror("poll");
                appDone = true;
                continue;
            case 0:
                break;
            default:
                for (size_t i = 0; i < len; ++i) {
                    struct pollfd pfds = pfdsList[i];
                    if ((pfds.revents & POLLIN) != 0) {
                        readSocket(pfds.fd, &bytes);
                    }
                }
                break;
        }
        if (SDL_PollEvent(&e) == 0) {
            continue;
        }
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

end:
    if (thread != NULL) {
        SDL_WaitThread(thread, &result);
    }
    uint8_tListFree(&bytes);
    closeSocket(udpListener);
    closeSocket(tcpListener);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    ClientDataFree(&clientData);
    return result;
}