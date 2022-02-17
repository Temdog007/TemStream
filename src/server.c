#include <include/main.h>

SDL_mutex* streamMutex = NULL;
StreamInformationList streams = { 0 };

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
            SDL_LockMutex(streamMutex);
            message.tag = MessageTag_currentStreams;
            message.currentStreams = streams;
            bytes->used = 0;
            MessageSerialize(&message, bytes, 0);
            SDL_UnlockMutex(streamMutex);
            if (sendMessage(fd, bytes->buffer, bytes->used, addr) <= 0) {
                perror("send");
                break;
            }
            result = true;
        } break;
        case MessageTag_createStream: {
            SDL_LockMutex(streamMutex);
            const bool exists = StreamInformationListFindIf(
              &streams,
              (StreamInformationListFindFunc)StreamInformationNameEqual,
              &message.createStream.name,
              NULL,
              NULL);
            if (exists) {
                socklen_t socklen = sizeof(*addr);
                if (addr == NULL &&
                    getpeername(fd, (struct sockaddr*)addr, &socklen) == -1) {
                    perror("getpeername");
                    break;
                }
                char buffer[INET6_ADDRSTRLEN] = { 0 };
                getAddrString(addr, buffer);
                printf("Client '%s' attempted to add a duplicate "
                       "stream '%s'\n",
                       buffer,
                       message.createStream.name.buffer);
            } else {
                StreamInformationListAppend(&streams, &message.createStream);
            }
            SDL_UnlockMutex(streamMutex);
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
        default:
            break;
    }
    return result;
}

int
runTcpServer(const AllConfiguration* configuration)
{
    ConnectionList connections = { .allocator = currentAllocator };
    int listener = INVALID_SOCKET;
    int result = EXIT_FAILURE;
    Bytes bytes = { .buffer = currentAllocator->allocate(UINT16_MAX),
                    .allocator = currentAllocator,
                    .size = UINT16_MAX };

    switch (configuration->address.tag) {
        case AddressTag_domainSocket:
            listener = openUnixSocket(
              configuration->address.domainSocket.buffer,
              SocketOptions_Server | SocketOptions_Tcp | SocketOptions_Local);
            break;
        case AddressTag_ipAddress:
            listener =
              openIpSocket(configuration->address.ipAddress.ip.buffer,
                           configuration->address.ipAddress.port.buffer,
                           SocketOptions_Server | SocketOptions_Tcp);
            break;
        default:
            break;
    }
    if (listener == INVALID_SOCKET) {
        goto end;
    }

    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = sizeof(addr);
    puts("Opened TCP port");

    {
        struct Connection connection = { .events = POLLIN,
                                         .revents = 0,
                                         .fd = listener };
        ConnectionListAppend(&connections, &connection);
    }

    while (!appDone) {
        switch (
          poll((struct pollfd*)connections.buffer, connections.used, 1000)) {
            case -1:
                perror("poll");
                continue;
            case 0:
                continue;
            default:
                break;
        }
        size_t i = 0;

        while (i < connections.used) {
            struct Connection connection = connections.buffer[i];
            if ((connection.revents & POLLIN) == 0) {
                goto nextIndex;
            }
            if (connection.fd == listener) {
                socklen = sizeof(addr);
                const int new_fd =
                  accept(listener, (struct sockaddr*)&addr, &socklen);
                if (new_fd <= 0) {
                    perror("accept");
                    goto nextIndex;
                }
                connection.fd = new_fd;
                ConnectionListAppend(&connections, &connection);
                goto nextIndex;
            }

            ssize_t size = recv(connection.fd, bytes.buffer, UINT16_MAX, 0);
            if (size <= 0) {
                ConnectionListSwapRemove(&connections, i);
                continue;
            }

            bytes.used = (uint32_t)size;

            if (handleServerMessage(&bytes, connection.fd, sendTcp, NULL)) {
                goto nextIndex;
            }
            ConnectionListSwapRemove(&connections, i);
            continue;
        nextIndex:
            ++i;
        }
    }

end:
    uint8_tListFree(&bytes);
    ConnectionListFree(&connections);
    return EXIT_SUCCESS;
}

int
runUdpServer(const AllConfiguration* configuration)
{
    int listener = INVALID_SOCKET;
    int result = EXIT_FAILURE;
    Bytes bytes = { .buffer = currentAllocator->allocate(UINT16_MAX),
                    .allocator = currentAllocator,
                    .size = UINT16_MAX };

    switch (configuration->address.tag) {
        case AddressTag_domainSocket:
            listener =
              openUnixSocket(configuration->address.domainSocket.buffer,
                             SocketOptions_Server | SocketOptions_Local);
            break;
        case AddressTag_ipAddress:
            listener =
              openIpSocket(configuration->address.ipAddress.ip.buffer,
                           configuration->address.ipAddress.port.buffer,
                           SocketOptions_Server);
            break;
        default:
            break;
    }
    if (listener == INVALID_SOCKET) {
        goto end;
    }

    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = sizeof(addr);
    puts("Opened UDP port");

    struct pollfd pfds = { .fd = listener, .events = POLLIN, .revents = 0 };
    while (!appDone) {
        switch (poll(&pfds, 1, 1000)) {
            case -1:
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
        const ssize_t size = recvfrom(listener,
                                      bytes.buffer,
                                      UINT16_MAX,
                                      0,
                                      (struct sockaddr*)&addr,
                                      &socklen);
        if (size <= 0) {
            continue;
        }

        bytes.used = (uint32_t)size;

        handleServerMessage(&bytes, listener, sendUdp, &addr);
    }

end:
    uint8_tListFree(&bytes);
    closeSocket(listener);
    return EXIT_SUCCESS;
}

uint32_t
checkValidStreams(uint32_t timeout)
{
    SDL_LockMutex(streamMutex);
    char ipstr[64] = { 0 };
    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = 0;
    size_t i = 0;
    while (i < streams.used) {
        socklen = sizeof(addr);
        const StreamInformation* info = &streams.buffer[i];
        if (getpeername(info->owner, (struct sockaddr*)&addr, &socklen) == -1) {
            printf("Producer for stream '%s' was closed\n", info->name.buffer);
            StreamInformationListSwapRemove(&streams, i);
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

    streams.allocator = currentAllocator;
    puts("Running server");
    printAllConfiguration(configuration);

    const SDL_TimerID id =
      SDL_AddTimer(3000U, (SDL_TimerCallback)checkValidStreams, NULL);
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
    SDL_LockMutex(streamMutex);
    StreamInformationListFree(&streams);
    SDL_UnlockMutex(streamMutex);
    SDL_DestroyMutex(streamMutex);
    SDL_Quit();
    return EXIT_SUCCESS;
}