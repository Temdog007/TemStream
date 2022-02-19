#include <include/main.h>

bool
clientSend(const Client* client, const Bytes* bytes);

typedef struct ServerData
{
    StreamList streams;
    pClientList clients;
    SDL_mutex* mutex;
    int32_t udpSocket;
} ServerData, *pServerData;

ServerData serverData = { 0 };

void
ServerDataFree(pServerData data)
{
    SDL_DestroyMutex(data->mutex);
    pClientListFree(&data->clients);
    StreamListFree(&data->streams);
}

// Assigns client a name also
bool
authenticateClient(pClient client,
                   const ClientAuthentication* cAuth,
                   pRandomState rs)
{
    const ServerAuthentication* sAuth = client->serverAuthentication;
    switch (sAuth->tag) {
        case ServerAuthenticationTag_file:
            fprintf(stderr, "Failed authentication is not implemented\n");
            return false;
        default:
            switch (cAuth->tag) {
                case ClientAuthenticationTag_credentials:
                    return TemLangStringCopy(&client->name,
                                             &cAuth->credentials.username,
                                             currentAllocator);
                case ClientAuthenticationTag_token:
                    fprintf(stderr,
                            "Token authentication is not implemented\n");
                    return false;
                default:
                    // Give client random name
                    client->name = RandomClientName(rs);
                    break;
            }
            return true;
    }
}

void
copyMessageToClients(const StreamMessage* message, const Bytes* bytes)
{
    const bool udp = MessageUsesUdp(message);

    IN_MUTEX(serverData.mutex, endMutex, {
        if (!StreamListFindIf(&serverData.streams,
                              (StreamListFindFunc)StreamGuidEquals,
                              &message->id,
                              NULL,
                              NULL)) {
#if _DEBUG
            printf("Got stream message that doesn't belong to a stream\n");
#endif
            goto endMutex;
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
                clientSend(client, bytes);
            }
        }
    })
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

    RandomState rs = makeRandomState();
    bytes.used = (uint32_t)size;
    MessageDeserialize(&message, &bytes, 0, true);
    if (message.tag == MessageTag_authenticate) {
        if (authenticateClient(client, &message.authenticate, &rs)) {
            MessageFree(&message);
            message.tag = MessageTag_authenticateAck;
            TemLangStringCopy(
              &message.authenticateAck.name, &client->name, currentAllocator);
            message.authenticateAck.id = client->id;
            bytes.used = 0;
            MessageSerialize(&message, &bytes, true);
            clientSend(client, &bytes);
        } else {
            printf("Client '%s' failed authentication\n", buffer);
            goto end;
        }
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
            case MessageTag_connectToStream: {
                bool success;
                IN_MUTEX(serverData.mutex, cts, {
                    const Stream* stream = NULL;
                    success = GetStreamFromGuid(&serverData.streams,
                                                &message.connectToStream,
                                                &stream,
                                                NULL);
                    if (success) {
                        if (!GuidListFind(&client->connectedStreams,
                                          &stream->id,
                                          NULL,
                                          NULL)) {
                            GuidListAppend(&client->connectedStreams,
                                           &stream->id);
                        }
                    }
                });

                MessageFree(&message);
                message.tag = MessageTag_connectToStreamAck;
                message.connectToStreamAck = success;
                bytes.used = 0;
                MessageSerialize(&message, &bytes, true);
                clientSend(client, &bytes);
            } break;
            case MessageTag_disconnectFromStream: {
                bool success;
                IN_MUTEX(serverData.mutex, dfs, {
                    const Stream* stream = NULL;
                    success = GetStreamFromGuid(&serverData.streams,
                                                &message.disconnectFromStream,
                                                &stream,
                                                NULL);
                    if (success) {
                        while (GuidListSwapRemoveValue(
                          &client->connectedStreams, &stream->id))
                            ;
                    }
                })

                MessageFree(&message);
                message.tag = MessageTag_disconnectFromStreamAck;
                message.disconnectFromStreamAck = success;
                bytes.used = 0;
                MessageSerialize(&message, &bytes, true);
                clientSend(client, &bytes);
            } break;
            case MessageTag_startStreaming: {
                const Stream* stream = NULL;
                IN_MUTEX(serverData.mutex, start1, {
                    GetStreamFromName(&serverData.streams,
                                      &message.startStreaming.name,
                                      &stream,
                                      NULL);
                });

                if (stream == NULL) {
                    Stream newStream = message.startStreaming;
                    newStream.owner = client->id;
                    newStream.id = randomGuid(&rs);
                    printf("Client '%s' started a '%s' stream named '%s' "
                           "(record=%d)\n",
                           buffer,
                           StreamTypeToCharString(newStream.type),
                           newStream.name.buffer,
                           newStream.record);
                    IN_MUTEX(serverData.mutex, ss2, {
                        StreamListAppend(&serverData.streams, &newStream);
                    })
                    MessageFree(&message);
                    message.tag = MessageTag_startStreamingAck;
                    message.startStreamingAck.guid = newStream.id;
                    message.startStreamingAck.tag = OptionalGuidTag_guid;
                } else {
#if _DEBUG
                    printf("Client '%s' tried to add duplicate stream '%s'\n",
                           buffer,
                           stream->name.buffer);
#endif
                    MessageFree(&message);
                    message.tag = MessageTag_startStreamingAck;
                    message.startStreamingAck.none = NULL;
                    message.startStreamingAck.tag = OptionalGuidTag_none;
                }

                bytes.used = 0;
                MessageSerialize(&message, &bytes, 0);
                clientSend(client, &bytes);
            } break;
            case MessageTag_stopStreaming: {
                IN_MUTEX(serverData.mutex, stop1, {
                    const Stream* stream = NULL;
                    size_t i = 0;
                    GetStreamFromGuid(
                      &serverData.streams, &message.stopStreaming, &stream, &i);
                    MessageFree(&message);
                    message.tag = MessageTag_stopStreamingAck;
                    message.stopStreamingAck.tag = OptionalGuidTag_none;
                    message.stopStreamingAck.none = NULL;
                    if (stream != NULL &&
                        GuidEquals(&stream->owner, &client->id)) {
                        StreamListSwapRemove(&serverData.streams, i);
                        message.stopStreamingAck.tag = OptionalGuidTag_guid;
                        message.stopStreamingAck.guid = stream->id;
                    }
                    bytes.used = 0;
                    MessageSerialize(&message, &bytes, 0);
                });

                clientSend(client, &bytes);
            } break;
            case MessageTag_getAllStreams: {
                IN_MUTEX(serverData.mutex, gas, {
                    Message newMessage = { 0 };
                    newMessage.tag = MessageTag_getAllStreamsAck;
                    newMessage.getAllStreamsAck = serverData.streams;
                    bytes.used = 0;
                    MessageSerialize(&newMessage, &bytes, 0);
                });

                clientSend(client, &bytes);
            } break;
            case MessageTag_getConnectedStreams: {
                IN_MUTEX(serverData.mutex, gcs, {
                    Message newMessage = { 0 };
                    newMessage.tag = MessageTag_getConnectedStreamsAck;
                    newMessage.getConnectedStreamsAck =
                      client->connectedStreams;
                    bytes.used = 0;
                    MessageSerialize(&newMessage, &bytes, 0);
                });

                clientSend(client, &bytes);
            } break;
            case MessageTag_getClientStreams: {
                IN_MUTEX(serverData.mutex, gcs2, {
                    message.tag = MessageTag_getClientStreamsAck;
                    message.getClientStreamsAck.allocator = currentAllocator;
                    for (size_t i = 0; i < serverData.streams.used; ++i) {
                        const Stream* stream = &serverData.streams.buffer[i];
                        if (GuidEquals(&stream->owner, &client->id)) {
                            GuidListAppend(&message.getClientStreamsAck,
                                           &stream->id);
                        }
                    }
                    bytes.used = 0;
                    MessageSerialize(&message, &bytes, 0);
                });

                clientSend(client, &bytes);
            } break;
            case MessageTag_getStream: {
                IN_MUTEX(serverData.mutex, gs, {
                    const Stream* stream = NULL;
                    GetStreamFromGuid(
                      &serverData.streams, &message.getStream, &stream, NULL);
                    Message newMessage = { 0 };
                    newMessage.tag = MessageTag_getStreamAck;
                    if (stream == NULL) {
                        newMessage.getStreamAck.tag = OptionalStreamTag_none;
                        newMessage.getStreamAck.none = NULL;
                    } else {
                        newMessage.getStreamAck.tag = OptionalStreamTag_stream;
                        newMessage.getStreamAck.stream = *stream;
                    }

                    bytes.used = 0;
                    MessageSerialize(&newMessage, &bytes, 0);
                });

                clientSend(client, &bytes);
            } break;
            case MessageTag_getClients: {
                IN_MUTEX(serverData.mutex, get2, {
                    message.tag = MessageTag_getClientsAck;
                    message.getClientsAck.allocator = currentAllocator;
                    for (size_t i = 0; i < serverData.clients.used; ++i) {
                        const pClient* client = &serverData.clients.buffer[i];
                        TemLangStringListAppend(&message.getClientsAck,
                                                &(*client)->name);
                    }
                    bytes.used = 0;
                    MessageSerialize(&message, &bytes, 0);
                    MessageFree(&message);
                });

                clientSend(client, &bytes);
            } break;
            default:
                printf("Got invalid message '%s' from client '%s'\n",
                       MessageTagToCharString(message.tag),
                       buffer);
                goto end;
        }
    }

end:
    IN_MUTEX(serverData.mutex, fend, {
        pClientListSwapRemoveValue(&serverData.clients, &client);
    });

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

    const ServerConfiguration* config = &configuration->configuration.server;

    struct sockaddr_storage addr = { 0 };
    socklen_t socklen = sizeof(addr);
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

        pClient client = NULL;
        {
            bool atMax;
            IN_MUTEX(serverData.mutex, fend, {
                atMax = serverData.clients.used >= config->maxClients;
            });
            if (atMax) {
#if _DEBUG
                printf("Max clients (%u) has been reached\n",
                       config->maxClients);
#endif
                goto closeNewFd;
            }
        }

        client = currentAllocator->allocate(sizeof(Client));
        client->addr = addr;
        client->serverAuthentication = &config->authentication;
        client->id = randomGuid(&rs);
        // name and authentication will be set after parsing first message
        client->sockfd = new_fd;

        SDL_Thread* thread = SDL_CreateThread(
          (SDL_ThreadFunction)handleTcpConnection, "Tcp", client);
        if (thread == NULL) {
            fprintf(stderr, "Failed to created thread: %s\n", SDL_GetError());
            goto closeNewFd;
        }

        IN_MUTEX(serverData.mutex, fend2, {
            pClientListAppend(&serverData.clients, &client);
        });
        SDL_DetachThread(thread);
        continue;

    closeNewFd:
        if (client != NULL) {
            ClientFree(client);
            currentAllocator->free(client);
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
    IN_MUTEX(serverData.mutex, end, {
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
    });
    return timeout;
}

int
runServer(const AllConfiguration* configuration)
{
    int result = EXIT_FAILURE;
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

    appDone = false;

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
    result = EXIT_SUCCESS;

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
        bool done;
        IN_MUTEX(serverData.mutex, fend, {
            done = pClientListIsEmpty(&serverData.clients);
        });
        if (done) {
            break;
        }
        SDL_Delay(1000U);
    } while (true);

    ServerDataFree(&serverData);
    SDL_Quit();
    return result;
}