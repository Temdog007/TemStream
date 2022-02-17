#include <include/main.h>

SDL_mutex* streamMutex = NULL;
SDL_cond* streamCond = NULL;
StreamInformationList streams = { 0 };
MarkedMessageList pendingMessages = { 0 };
int32_t udpSocket = INVALID_SOCKET;
SDL_atomic_t consumerCount = { 0 };

ServerConfiguration
defaultServerConfiguration()
{
    return (ServerConfiguration){ .maxConsumers = 1024,
                                  .maxProducers = 8,
                                  .record = false };
}

bool
parseServerConfiguration(const int argc,
                         const char** argv,
                         pAllConfiguration configuration)
{
    pServerConfiguration server = &configuration->configuration.server;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-P", keyLen, { goto parseMaxProducers; });
        STR_EQUALS(key, "--producers", keyLen, { goto parseMaxProducers; });
        STR_EQUALS(key, "-C", keyLen, { goto parseMaxConsumers; });
        STR_EQUALS(key, "--consumers", keyLen, { goto parseMaxConsumers; });
        STR_EQUALS(key, "-R", keyLen, { goto parseRecord; });
        STR_EQUALS(key, "--record", keyLen, { goto parseRecord; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Producer", key, value);
            return false;
        }
        continue;

    parseMaxProducers : {
        char* end = NULL;
        server->maxProducers = (uint32_t)strtoul(value, &end, 10);
        continue;
    }
    parseMaxConsumers : {
        char* end = NULL;
        server->maxConsumers = (uint32_t)strtoul(value, &end, 10);
        continue;
    }
    parseRecord : {
        server->record = atoi(value);
        continue;
    }
    }
    return true;
}

int
printServerConfiguration(const ServerConfiguration* configuration)
{
    return printf("Max consumers: %u; Max producers: %u; Recording: %d\n",
                  configuration->maxConsumers,
                  configuration->maxProducers,
                  configuration->record);
}

bool
handleServerMessage(pBytes bytes,
                    const int fd,
                    SendFunc sendMessage,
                    struct sockaddr_storage* addr)
{
    bool result = false;
    Message message = { 0 };
    MessageDeserialize(&message, bytes, 0, true);
    switch (message.tag) {
        case MessageTag_getStreams: {
            // Start Mutex
            SDL_LockMutex(streamMutex);
            message.tag = MessageTag_currentStreams;
            message.currentStreams = streams;
            bytes->used = 0;
            MessageSerialize(&message, bytes, 0);
            SDL_UnlockMutex(streamMutex);
            // End Mutex

            if (sendMessage(fd, bytes->buffer, bytes->used, addr) !=
                (int)bytes->used) {
                perror("send");
                break;
            }
            result = true;
        } break;
        case MessageTag_createStream: {
            // Start Mutex
            SDL_LockMutex(streamMutex);
            const bool exists = StreamInformationListFindIf(
              &streams,
              (StreamInformationListFindFunc)StreamInformationNameEqual,
              &message.createStream.name,
              NULL,
              NULL);
            char buffer[INET6_ADDRSTRLEN] = { 0 };
            int port;
            getAddrString(addr, buffer, &port);
            if (exists) {
                printf("Client '%s:%d' attempted to add a duplicate "
                       "stream '%s'\n",
                       buffer,
                       port,
                       message.createStream.name.buffer);
            } else {
                printf("Creating stream '%s' for producer '%s:%d'\n",
                       message.createStream.name.buffer,
                       buffer,
                       port);
                message.createStream.owner = fd;
                StreamInformationListAppend(&streams, &message.createStream);
            }
            SDL_UnlockMutex(streamMutex);
            // End Mutex

            message.tag = MessageTag_createStreamAck;
            message.createStreamAck = !exists;
            bytes->used = 0;
            MessageSerialize(&message, bytes, 0);
            if (sendMessage(fd, bytes->buffer, bytes->used, addr) <= 0) {
                perror("send");
                break;
            }
            result = true;
        } break;
        case MessageTag_streamMessage: {
            // Start Mutex
            SDL_LockMutex(streamMutex);
            for (size_t i = 0; i < streams.used; ++i) {
                const StreamInformation* info = &streams.buffer[i];
                if (!StreamTypeMatchStreamMessage(info->type,
                                                  message.streamMessage.tag) ||
                    info->owner != fd) {
                    continue;
                }
                RandomState rs = makeRandomState();
                const MarkedMessage newMessage = { .message = message,
                                                   .guid = randomGuid(&rs) };
                MarkedMessageListAppend(&pendingMessages, &newMessage);
                SDL_CondBroadcast(streamCond);
#if _DEBUG
                printf("Sending message to '%d' consumers\n",
                       SDL_AtomicGet(&consumerCount));
#endif
                break;
            }
            SDL_UnlockMutex(streamMutex);
            // End Mutex
            result = true;
        } break;
        default:
            printf("Unexpected message: %s\n",
                   MessageTagToCharString(message.tag));
            break;
    }
    MessageFree(&message);
    return result;
}

int
handleTcpConnection(pConsumer consumer)
{
#if _DEBUG
    char buffer[64];
    int port;
    getAddrString(&consumer->addr, buffer, &port);
    printf("New connection: %s:%d (%d connections)\n",
           buffer,
           port,
           SDL_AtomicGet(&consumerCount));
#endif
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(UINT16_MAX),
                    .size = UINT16_MAX,
                    .used = 0 };
    struct pollfd pfds = { .events = POLLIN,
                           .revents = 0,
                           .fd = consumer->sockfd };
    socklen_t socklen = 0;
    while (!appDone) {
        switch (poll(&pfds, 1, 1000)) {
            case INVALID_SOCKET:
                perror("poll");
                continue;
            case 0:
                continue;
            default:
                break;
        }

        const ssize_t size = recv(pfds.fd, bytes.buffer, UINT16_MAX, 0);
        if (size <= 0) {
            break;
        }

        socklen = sizeof(struct sockaddr_storage);
        if (getpeername(pfds.fd, (struct sockaddr*)&consumer->addr, &socklen) ==
            INVALID_SOCKET) {
            perror("getpeername");
            break;
        } else {
#if _DEBUG
            char buffer[64];
            int port;
            getAddrString(&consumer->addr, buffer, &port);
            printf("Got %zd bytes from client '%s:%d'\n", size, buffer, port);
#endif
        }

        bytes.used = (uint32_t)size;
        if (handleServerMessage(&bytes, pfds.fd, sendTcp, &consumer->addr)) {
            continue;
        }
        break;
    }
    uint8_tListFree(&bytes);
    closeSocket(consumer->sockfd);
    currentAllocator->free(consumer);
    return EXIT_SUCCESS;
}

int
runTcpServer(const AllConfiguration* configuration)
{
    Bytes bytes = { .buffer = currentAllocator->allocate(UINT16_MAX),
                    .allocator = currentAllocator,
                    .size = UINT16_MAX };

    const int listener = openSocketFromAddress(
      &configuration->address, SocketOptions_Server | SocketOptions_Tcp);
    if (listener == INVALID_SOCKET) {
        goto end;
    }

    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = sizeof(addr);
    puts("Opened TCP port");
#if _DEBUG
    printf("Tcp port: %d\n", listener);
#endif

    struct pollfd pfds = { .events = POLLIN, .revents = 0, .fd = listener };
    while (!appDone) {
        switch (poll(&pfds, 1, 1000)) {
            case INVALID_SOCKET:
                perror("poll");
                continue;
            case 0:
                continue;
            default:
                break;
        }

        if ((pfds.revents & POLLIN) == 0) {
            continue;
        }
        socklen = sizeof(addr);
        const int new_fd = accept(listener, (struct sockaddr*)&addr, &socklen);
        if (new_fd == INVALID_SOCKET) {
#if _DEBUG
            printf("Listen error on socket: %d\n", listener);
#endif
            perror("accept");
            continue;
        }

        pConsumer consumer = currentAllocator->allocate(sizeof(Consumer));
        consumer->sockfd = new_fd;
        consumer->addr = addr;
        SDL_Thread* thread = SDL_CreateThread(
          (SDL_ThreadFunction)handleTcpConnection, "Tcp", consumer);
        if (thread == NULL) {
            fprintf(stderr, "Failed to created thread: %s\n", SDL_GetError());
            closeSocket(new_fd);
            currentAllocator->free(consumer);
        } else {
            SDL_DetachThread(thread);
        }
    }

end:
    uint8_tListFree(&bytes);
    closeSocket(listener);
    return EXIT_SUCCESS;
}

int
runUdpServer(const AllConfiguration* configuration)
{
    Bytes bytes = { .buffer = currentAllocator->allocate(UINT16_MAX),
                    .allocator = currentAllocator,
                    .size = UINT16_MAX };

    udpSocket =
      openSocketFromAddress(&configuration->address, SocketOptions_Server);
    if (udpSocket == INVALID_SOCKET) {
        goto end;
    }

    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = sizeof(addr);
    puts("Opened UDP port");
#if _DEBUG
    printf("Udp port: %d\n", udpSocket);
#endif

    struct pollfd pfds = { .fd = udpSocket, .events = POLLIN, .revents = 0 };
    while (!appDone) {
        switch (poll(&pfds, 1, 1000)) {
            case INVALID_SOCKET:
                perror("poll");
                continue;
            case 0:
                continue;
            default:
                break;
        }

        if ((pfds.revents & POLLIN) == 0) {
            continue;
        }

        socklen = sizeof(addr);
        const ssize_t size = recvfrom(udpSocket,
                                      bytes.buffer,
                                      UINT16_MAX,
                                      0,
                                      (struct sockaddr*)&addr,
                                      &socklen);
        if (size <= 0) {
            continue;
        }

        bytes.used = (uint32_t)size;

        handleServerMessage(&bytes, udpSocket, sendUdp, &addr);
    }

end:
    uint8_tListFree(&bytes);
    closeSocket(udpSocket);
    udpSocket = INVALID_SOCKET;
    return EXIT_SUCCESS;
}

uint32_t
cleanupThread(uint32_t timeout)
{
    SDL_LockMutex(streamMutex);
    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = 0;
    size_t i = 0;
    while (i < streams.used) {
        socklen = sizeof(addr);
        const StreamInformation* info = &streams.buffer[i];
        if (getpeername(info->owner, (struct sockaddr*)&addr, &socklen) ==
            INVALID_SOCKET) {
            printf("Producer for stream '%s' was closed\n", info->name.buffer);
            StreamInformationListSwapRemove(&streams, i);
        } else {
            ++i;
        }
    }
    i = 0;
    const int consumers = SDL_AtomicGet(&consumerCount);
    while (i < pendingMessages.used) {
        const MarkedMessage* message = &pendingMessages.buffer[i];
        if (message->sent >= consumers) {
            MarkedMessageListSwapRemove(&pendingMessages, i);
        } else {
            ++i;
        }
    }
    SDL_UnlockMutex(streamMutex);
    return timeout;
}

int
runServer(const AllConfiguration* configuration)
{
    if (SDL_Init(SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    streamMutex = SDL_CreateMutex();
    if (streamMutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
    }

    streamCond = SDL_CreateCond();
    if (streamCond == NULL) {
        fprintf(stderr, "Failed to creat cond: %s\n", SDL_GetError());
        goto end;
    }

    streams.allocator = currentAllocator;
    pendingMessages.allocator = currentAllocator;
    puts("Running server");
    printAllConfiguration(configuration);

    const SDL_TimerID id =
      SDL_AddTimer(3000U, (SDL_TimerCallback)cleanupThread, NULL);
    if (id == 0) {
        fprintf(stderr, "Failed to start timer: %s\n", SDL_GetError());
        goto end;
    }

    SDL_Thread* threads[] = {
        SDL_CreateThread(
          (SDL_ThreadFunction)runTcpServer, "Tcp", (void*)configuration),
        SDL_CreateThread(
          (SDL_ThreadFunction)runUdpServer, "Udp", (void*)configuration),
    };
    for (size_t i = 0; i < sizeof(threads) / sizeof(SDL_Thread*); ++i) {
        SDL_WaitThread(threads[i], NULL);
    }
    SDL_RemoveTimer(id);

end:
    if (configuration->address.tag == AddressTag_domainSocket) {
        char ipstr[256] = { 0 };
        snprintf(ipstr,
                 sizeof(ipstr),
                 "%s_udp",
                 configuration->address.domainSocket.buffer);
        unlink(ipstr);
        snprintf(ipstr,
                 sizeof(ipstr),
                 "%s_tcp",
                 configuration->address.domainSocket.buffer);
        unlink(ipstr);
    }
    // Start Mutex
    SDL_LockMutex(streamMutex);
    StreamInformationListFree(&streams);
    MarkedMessageListFree(&pendingMessages);
    while (SDL_AtomicGet(&consumerCount) > 0) {
        SDL_CondWaitTimeout(streamCond, streamMutex, 1000);
    }
    SDL_UnlockMutex(streamMutex);
    // End Mutex

    SDL_DestroyMutex(streamMutex);
    SDL_DestroyCond(streamCond);
    SDL_Quit();
    return EXIT_SUCCESS;
}