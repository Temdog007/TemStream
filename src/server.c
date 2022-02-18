#include <include/main.h>

typedef struct ServerData
{
    SDL_mutex* mutex;
    StreamList streams;
    pClientList clients;
    int32_t udpSocket;
} ServerData, *pServerData;

ServerData serverData = { 0 };

void
ServerDataFree(pServerData data)
{
    SDL_DestroyMutex(data->mutex);
    StreamListFree(&data->streams);
}

void
copyMessageToClients(const StreamMessage* message, const Bytes* bytes)
{
    const bool udp = MessageUsesUdp(message);

    SDL_LockMutex(serverData.mutex);
    if (!StreamListFindIf(&serverData.streams,
                          (StreamListFindFunc)StreamGuidEquals,
                          &message->id,
                          NULL,
                          NULL)) {
#if _DEBUG
        printf("Got stream message that doesn't belong to a stream\n");
#endif
        goto end;
    }
    for (size_t i = 0; i < serverData.clients.used; ++i) {
        const Client* client = serverData.clients.buffer[i];
        if (!GuidListFind(
              &client->connectedStreams, &message->id, NULL, NULL)) {
            continue;
        }
        if (udp) {
            const size_t socklen = sizeof(client->addr);
            if (sendto(serverData.udpSocket,
                       bytes->buffer,
                       bytes->used,
                       0,
                       (struct sockaddr*)&client->addr,
                       socklen) != (ssize_t)bytes->used) {
                perror("sendto");
            }
        } else {
            if (send(client->sockfd, bytes->buffer, bytes->used, 0) !=
                (ssize_t)bytes->used) {
                perror("send");
            }
        }
    }
end:
    SDL_UnlockMutex(serverData.mutex);
}

ServerConfiguration
defaultServerConfiguration()
{
    return (ServerConfiguration){
        .maxClients = 1024,
        .authentication = { .none = NULL, .tag = ServerAuthenticationTag_none }
    };
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
        STR_EQUALS(key, "-C", keyLen, { goto parseMaxClients; });
        STR_EQUALS(key, "--clients", keyLen, { goto parseMaxClients; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Server", key, value);
            return false;
        }
        continue;

    parseMaxClients : {
        char* end = NULL;
        server->maxClients = (uint32_t)strtoul(value, &end, 10);
        continue;
    }
    }
    return true;
}

int
printServerConfiguration(const ServerConfiguration* configuration)
{
    return printf("Max clients: %u\n", configuration->maxClients);
}

int
handleTcpConnection(pClient client)
{
    int result = EXIT_FAILURE;

    char buffer[64];
    getAddrString(&client->addr, buffer, NULL);
    printf("New connection: %s\n", buffer);

    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };
    struct pollfd pfds = { .events = POLLIN,
                           .revents = 0,
                           .fd = client->sockfd };
    Message message = { 0 };

    switch (poll(&pfds, 1, 5000)) {
        case -1:
            perror("poll");
            goto end;
        case 0:
            printf("Failed to get authentication from client '%s'\n", buffer);
            goto end;
        default:
            break;
    }
    if ((pfds.revents & POLLIN) == 0) {
        printf("Failed to get authentication from client '%s'\n", buffer);
        goto end;
    }

    ssize_t size = recv(pfds.fd, bytes.buffer, bytes.size, 0);
    if (size <= 0) {
        goto end;
    }

    bytes.used = (uint32_t)size;
    MessageDeserialize(&message, &bytes, 0, true);
    if (message.tag == MessageTag_authenticate) {
        client->authentication = message.authenticate;
    } else {
        printf("Expected authentication from client '%s'. Got '%s'\n",
               buffer,
               MessageTagToCharString(message.tag));
        goto end;
    }

    while (!appDone) {
        switch (poll(&pfds, 1, 1000)) {
            case -1:
                perror("poll");
                goto end;
            case 0:
                continue;
            default:
                break;
        }
        if ((pfds.revents & POLLIN) == 0) {
            continue;
        }

        size = recv(pfds.fd, bytes.buffer, bytes.size, 0);
        if (size <= 0) {
            goto end;
        }

        bytes.used = (uint32_t)size;
        MessageFree(&message);
        MessageDeserialize(&message, &bytes, 0, true);
        switch (message.tag) {
            case MessageTag_streamMessage:
                copyMessageToClients(&message.streamMessage, &bytes);
                break;
            case MessageTag_connectToStream:
                fprintf(stderr, "MessageTag_connectToStream not implemented\n");
                break;
            case MessageTag_disconnectFromStream:
                fprintf(stderr,
                        "MessageTag_disconnectFromStream not implemented\n");
                break;
            case MessageTag_startStreaming:
                fprintf(stderr, "MessageTag_startStreaming not implemented\n");
                break;
            case MessageTag_stopStreaming:
                fprintf(stderr, "MessageTag_stopStreaming not implemented\n");
                break;
            case MessageTag_getStreams: {
                // Start Mutex
                SDL_LockMutex(serverData.mutex);
                message.tag = MessageTag_currentStreams;
                message.currentStreams.allocator = currentAllocator;
                for (size_t i = 0; i < serverData.streams.used; ++i) {
                    const Stream* stream = &serverData.streams.buffer[i];
                    StreamInformation si = { .name = stream->name,
                                             .type = stream->type };
                    StreamInformationListAppend(&message.currentStreams, &si);
                }
                bytes.used = 0;
                MessageSerialize(&message, &bytes, 0);
                SDL_UnlockMutex(serverData.mutex);
                // End Mutex

                if (send(client->sockfd, bytes.buffer, bytes.used, 0) !=
                    (ssize_t)bytes.used) {
                    perror("send");
                    goto end;
                }
            } break;
            case MessageTag_getClients: {
                // Start Mutex
                SDL_LockMutex(serverData.mutex);
                message.tag = MessageTag_currentClients;
                message.currentClients.allocator = currentAllocator;
                for (size_t i = 0; i < serverData.clients.used; ++i) {
                    const pClient* client = &serverData.clients.buffer[i];
                    TemLangStringListAppend(&message.currentClients,
                                            &(*client)->name);
                }
                bytes.used = 0;
                MessageSerialize(&message, &bytes, 0);
                SDL_UnlockMutex(serverData.mutex);
                // End Mutex

                if (send(client->sockfd, bytes.buffer, bytes.used, 0) !=
                    (ssize_t)bytes.used) {
                    perror("send");
                    goto end;
                }
            } break;
            default:
                printf("Got invalid message '%s' from client '%s'\n",
                       MessageTagToCharString(message.tag),
                       buffer);
                goto end;
        }
    }

end:
    SDL_LockMutex(serverData.mutex);
    pClientListSwapRemoveValue(&serverData.clients, &client);
    SDL_UnlockMutex(serverData.mutex);

    printf("Closing connection: %s\n", buffer);
    MessageFree(&message);
    uint8_tListFree(&bytes);
    ClientFree(client);
    currentAllocator->free(client);
    return result;
}

int
runTcpServer(const AllConfiguration* configuration)
{
    Bytes bytes = { .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .allocator = currentAllocator,
                    .size = MAX_PACKET_SIZE };

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

    RandomState rs = makeRandomState();
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

        pClient client = currentAllocator->allocate(sizeof(Client));
        client->addr = addr;
        client->guid = randomGuid(&rs);
        // authentication will be set after parsing first message
        client->sockfd = new_fd;

        SDL_Thread* thread = SDL_CreateThread(
          (SDL_ThreadFunction)handleTcpConnection, "Tcp", client);
        if (thread == NULL) {
            fprintf(stderr, "Failed to created thread: %s\n", SDL_GetError());
            ClientFree(client);
            currentAllocator->free(client);
        } else {
            SDL_LockMutex(serverData.mutex);
            pClientListAppend(&serverData.clients, &client);
            SDL_UnlockMutex(serverData.mutex);
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
    Message message = { 0 };
    Bytes bytes = { .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .allocator = currentAllocator,
                    .size = MAX_PACKET_SIZE };

    serverData.udpSocket =
      openSocketFromAddress(&configuration->address, SocketOptions_Server);
    if (serverData.udpSocket == INVALID_SOCKET) {
        goto end;
    }

    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = sizeof(addr);
    puts("Opened UDP port");
#if _DEBUG
    printf("Udp port: %d\n", serverData.udpSocket);
#endif

    struct pollfd pfds = { .fd = serverData.udpSocket,
                           .events = POLLIN,
                           .revents = 0 };
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
        const ssize_t size = recvfrom(serverData.udpSocket,
                                      bytes.buffer,
                                      bytes.size,
                                      0,
                                      (struct sockaddr*)&addr,
                                      &socklen);
        if (size <= 0) {
            continue;
        }

        bytes.used = (uint32_t)size;
        MessageFree(&message);
        MessageDeserialize(&message, &bytes, 0, true);
        if (message.tag == MessageTag_streamMessage) {
            copyMessageToClients(&message.streamMessage, &bytes);
        } else {
#if _DEBUG
            printf("Got invalid message on UDP port: %s\n",
                   MessageTagToCharString(message.tag));
#endif
        }
    }

end:
    MessageFree(&message);
    uint8_tListFree(&bytes);
    closeSocket(serverData.udpSocket);
    serverData.udpSocket = INVALID_SOCKET;
    return EXIT_SUCCESS;
}

uint32_t
cleanupThread(uint32_t timeout)
{
    SDL_LockMutex(serverData.mutex);
    size_t i = 0;
    while (i < serverData.streams.used) {
        const Stream* stream = &serverData.streams.buffer[i];
        if (pClientListFindIf(&serverData.clients,
                              (pClientListFindFunc)ClientGuidEquals,
                              &stream->owner,
                              NULL,
                              NULL)) {
            ++i;
        } else {
            printf("Owner of stream '%s' has disconnected\n",
                   stream->name.buffer);
            StreamListSwapRemove(&serverData.streams, i);
        }
    }
    SDL_UnlockMutex(serverData.mutex);
    return timeout;
}

int
runServer(const AllConfiguration* configuration)
{
    if (SDL_Init(SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    serverData.mutex = SDL_CreateMutex();
    if (serverData.mutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
    }

    serverData.streams.allocator = currentAllocator;
    serverData.clients.allocator = currentAllocator;
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

    do {
        SDL_LockMutex(serverData.mutex);
        const bool done = pClientListIsEmpty(&serverData.clients);
        SDL_UnlockMutex(serverData.mutex);
        if (done) {
            break;
        } else {
            SDL_Delay(1000U);
        }
    } while (true);

    ServerDataFree(&serverData);
    SDL_Quit();
    return EXIT_SUCCESS;
}