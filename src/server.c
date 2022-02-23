#include <include/main.h>

typedef struct ServerData
{
    StreamList streams;
    pClientList clients;
    SDL_mutex* mutex;
    int32_t udpSocket;
    StreamMessageList storage;
} ServerData, *pServerData;

ServerData serverData = { 0 };

void
ServerDataFree(pServerData data)
{
    SDL_DestroyMutex(data->mutex);
    pClientListFree(&data->clients);
    StreamListFree(&data->streams);
    StreamMessageListFree(&data->storage);
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

bool
clientHasAccess(const Client* client,
                const Access* access,
                const Stream* stream)
{
    if (GuidEquals(&client->id, &stream->owner)) {
        return true;
    }

    switch (access->tag) {
        case AccessTag_anyone:
            return true;
        case AccessTag_list:
            return TemLangStringListFindIf(
              &access->list,
              (TemLangStringListFindFunc)TemLangStringsAreEqual,
              &client->name,
              NULL,
              NULL);
        default:
            break;
    }
    return false;
}

bool
clientHasReadAccess(const Client* client, const Stream* stream)
{
    return clientHasAccess(client, &stream->readers, stream);
}

bool
clientHasWriteAccess(const Client* client, const Stream* stream)
{
    return clientHasAccess(client, &stream->writers, stream);
}

bool
setStreamMessageTimestamp(const Client* client, pMessage message)
{
    switch (message->tag) {
        case MessageTag_streamMessage: {
            pStreamMessage streamMessage = &message->streamMessage;
            switch (streamMessage->data.tag) {
                case StreamMessageDataTag_chatMessage:
                    streamMessage->data.chatMessage.timeStamp =
                      (int64_t)time(NULL);
                    return TemLangStringCopy(
                      &streamMessage->data.chatMessage.author,
                      &client->name,
                      currentAllocator);
                default:
                    break;
            }
        } break;
        default:
            break;
    }
    return false;
}

void
copyMessageToClients(const Client* writer, pStreamMessage message, pBytes bytes)
{
    const bool udp = MessageUsesUdp(message);

    IN_MUTEX(serverData.mutex, endMutex, {
        const Stream* stream = NULL;
        if (!StreamListFindIf(&serverData.streams,
                              (StreamListFindFunc)StreamGuidEquals,
                              &message->id,
                              &stream,
                              NULL)) {
#if _DEBUG
            puts("Got stream message that doesn't belong to a stream");
#endif
            goto endMutex;
        }
        if (!clientHasWriteAccess(writer, stream)) {
            goto endMutex;
        }
        {
            pStreamMessage storage = NULL;
            size_t sIndex = 0;
            if (StreamMessageListFindIf(
                  &serverData.storage,
                  (StreamMessageListFindFunc)StreamMessageGuidEquals,
                  &message->id,
                  NULL,
                  &sIndex)) {
                storage = &serverData.storage.buffer[sIndex];
            } else {
                StreamMessage s = { 0 };
                s.id = message->id;
                switch (message->data.tag) {
                    case StreamMessageDataTag_chatMessage:
                        s.data.tag = StreamMessageDataTag_chatLogs;
                        s.data.chatLogs.allocator = currentAllocator;
                        break;
                    case StreamMessageDataTag_text:
                        s.data.tag = StreamMessageDataTag_text;
                        s.data.text.allocator = currentAllocator;
                        break;
                    default:
                        s.data.tag = StreamMessageDataTag_none;
                        s.data.none = NULL;
                        break;
                }
                StreamMessageListAppend(&serverData.storage, &s);
                storage =
                  &serverData.storage.buffer[serverData.storage.used - 1U];
            }
            switch (message->data.tag) {
                case StreamMessageDataTag_chatMessage:
                    ChatMessageListAppend(&storage->data.chatLogs,
                                          &message->data.chatMessage);
                    break;
                case StreamMessageDataTag_text:
                    TemLangStringCopy(&storage->data.text,
                                      &message->data.text,
                                      currentAllocator);
                    break;
                default:
                    break;
            }
        }
        Bytes tempBytes = { 0 };
        tempBytes.allocator = currentAllocator;
        for (size_t i = 0; i < serverData.clients.used; ++i) {
            const Client* client = serverData.clients.buffer[i];
            if (!GuidListFind(
                  &client->connectedStreams, &message->id, NULL, NULL) ||
                !clientHasReadAccess(client, stream)) {
                continue;
            }
            if (udp) {
                const size_t socklen = sizeof(client->addr);
                // Prepare client for data
                Message temp = { 0 };
                temp.tag = MessageTag_prepareForData;
                temp.prepareForData = bytes->used;
                MESSAGE_SERIALIZE(temp, tempBytes);
                sendto(serverData.udpSocket,
                       tempBytes.buffer,
                       tempBytes.used,
                       0,
                       (struct sockaddr*)&client->addr,
                       socklen);
                ssize_t target = (ssize_t)bytes->used;
                while (target > 0) {
                    const ssize_t sent = sendto(serverData.udpSocket,
                                                bytes->buffer,
                                                bytes->used,
                                                0,
                                                (struct sockaddr*)&client->addr,
                                                socklen);
                    if (sent == -1) {
                        perror("sendto");
                        break;
                    }
                    target -= sent;
                }
            } else {
                clientSend(client, bytes, true);
            }
        }
        uint8_tListFree(&tempBytes);
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

void
getClientsStreams(pClient client, pGuidList guids)
{
    if (guids->allocator == NULL) {
        guids->allocator = currentAllocator;
    }
    for (size_t i = 0; i < serverData.streams.used; ++i) {
        const Stream* stream = &serverData.streams.buffer[i];
        if (GuidEquals(&stream->owner, &client->id)) {
            GuidListAppend(guids, &stream->id);
        }
    }
}

bool
serverHandleMessage(pClient client,
                    const int sockfd,
                    pMessage message,
                    pBytes bytes,
                    const char* buffer,
                    pRandomState rs)
{
    if (setStreamMessageTimestamp(client, message)) {
        MESSAGE_SERIALIZE((*message), (*bytes));
    }
    switch (message->tag) {
        case MessageTag_prepareForData: {
            if (!readAllData(sockfd, message->prepareForData, message, bytes) ||
                !serverHandleMessage(
                  client, sockfd, message, bytes, buffer, rs)) {
                return false;
            }
        } break;
        case MessageTag_streamMessage:
            copyMessageToClients(client, &message->streamMessage, bytes);
            break;
        case MessageTag_connectToStream: {
            IN_MUTEX(serverData.mutex, cts, {
                const Stream* stream = NULL;
                const StreamMessage* storage = NULL;
                const bool success =
                  GetStreamFromGuid(&serverData.streams,
                                    &message->connectToStream,
                                    &stream,
                                    NULL);
                GetStreamMessageFromGuid(&serverData.storage,
                                         &message->connectToStream,
                                         &storage,
                                         NULL);
                if (success) {
                    if (!GuidListFind(
                          &client->connectedStreams, &stream->id, NULL, NULL)) {
                        GuidListAppend(&client->connectedStreams, &stream->id);
                        printf("Client '%s' connected to stream '%s'\n",
                               client->name.buffer,
                               stream->name.buffer);
                    }
                }

                MessageFree(message);
                message->tag = MessageTag_connectToStreamAck;
                if (success) {
                    message->connectToStreamAck.tag =
                      OptionalStreamMessageTag_streamMessage;
                    if (storage == NULL) {
                        message->connectToStreamAck.streamMessage.id =
                          stream->id;
                        message->connectToStreamAck.streamMessage.data.tag =
                          StreamMessageDataTag_none;
                        message->connectToStreamAck.streamMessage.data.none =
                          NULL;
                    } else {
                        StreamMessageCopy(
                          &message->connectToStreamAck.streamMessage,
                          storage,
                          currentAllocator);
                    }
                } else {
                    message->connectToStreamAck.none = NULL;
                    message->connectToStreamAck.tag =
                      OptionalStreamMessageTag_none;
                }
            });
            MESSAGE_SERIALIZE((*message), (*bytes));
            return clientSend(client, bytes, true);
        } break;
        case MessageTag_disconnectFromStream: {
            bool success;
            IN_MUTEX(serverData.mutex, dfs, {
                const Stream* stream = NULL;
                success = GetStreamFromGuid(&serverData.streams,
                                            &message->disconnectFromStream,
                                            &stream,
                                            NULL);
                if (success) {
                    while (GuidListSwapRemoveValue(&client->connectedStreams,
                                                   &stream->id))
                        ;
                    printf("Client '%s' disconnected to stream '%s'\n",
                           client->name.buffer,
                           stream->name.buffer);
                }
            })

            MessageFree(message);
            message->tag = MessageTag_disconnectFromStreamAck;
            message->disconnectFromStreamAck = success;
            MESSAGE_SERIALIZE((*message), (*bytes));
            return clientSend(client, bytes, true);
        } break;
        case MessageTag_startStreaming: {
            const Stream* stream = NULL;
            IN_MUTEX(serverData.mutex, start1, {
                GetStreamFromName(&serverData.streams,
                                  &message->startStreaming.name,
                                  &stream,
                                  NULL);
            });

            if (stream == NULL) {
                Stream newStream = message->startStreaming;
                newStream.owner = client->id;
                newStream.id = randomGuid(rs);
                printf("Client '%s' started a '%s' stream named '%s' "
                       "(record=%d)\n",
                       client->name.buffer,
                       StreamTypeToCharString(newStream.type),
                       newStream.name.buffer,
                       newStream.record);
                IN_MUTEX(serverData.mutex, ss2, {
                    StreamListAppend(&serverData.streams, &newStream);
                    switch (newStream.type) {
                        case StreamType_Chat: {
                            StreamMessage m = { 0 };
                            m.id = newStream.id;
                            m.data.tag = StreamMessageDataTag_chatLogs;
                            m.data.chatLogs.allocator = currentAllocator;
                            StreamMessageListAppend(&serverData.storage, &m);
                        } break;
                        default:
                            break;
                    }
                })
                MessageFree(message);
                message->tag = MessageTag_startStreamingAck;
                message->startStreamingAck.guid = newStream.id;
                message->startStreamingAck.tag = OptionalGuidTag_guid;
            } else {
#if _DEBUG
                printf("Client '%s' tried to add duplicate stream '%s'\n",
                       buffer,
                       stream->name.buffer);
#endif
                MessageFree(message);
                message->tag = MessageTag_startStreamingAck;
                message->startStreamingAck.none = NULL;
                message->startStreamingAck.tag = OptionalGuidTag_none;
            }

            MESSAGE_SERIALIZE((*message), (*bytes));
            return clientSend(client, bytes, true);
        } break;
        case MessageTag_stopStreaming: {
            IN_MUTEX(serverData.mutex, stop1, {
                const Stream* stream = NULL;
                size_t i = 0;
                GetStreamFromGuid(
                  &serverData.streams, &message->stopStreaming, &stream, &i);
                MessageFree(message);
                message->tag = MessageTag_stopStreamingAck;
                message->stopStreamingAck.tag = OptionalGuidTag_none;
                message->stopStreamingAck.none = NULL;
                if (stream != NULL && GuidEquals(&stream->owner, &client->id)) {
                    printf("Client '%s' ended stream '%s'\n",
                           client->name.buffer,
                           stream->name.buffer);
                    StreamListSwapRemove(&serverData.streams, i);
                    message->stopStreamingAck.tag = OptionalGuidTag_guid;
                    message->stopStreamingAck.guid = stream->id;
                }
                MESSAGE_SERIALIZE((*message), (*bytes));
            });

            return clientSend(client, bytes, true);
        } break;
        case MessageTag_getAllStreams: {
            IN_MUTEX(serverData.mutex, gas, {
                Message newMessage = { 0 };
                newMessage.tag = MessageTag_getAllStreamsAck;
                newMessage.getAllStreamsAck = serverData.streams;
                MESSAGE_SERIALIZE(newMessage, (*bytes));
            });

            return clientSend(client, bytes, true);
        } break;
        case MessageTag_getConnectedStreams: {
            IN_MUTEX(serverData.mutex, gcs, {
                Message newMessage = { 0 };
                newMessage.tag = MessageTag_getConnectedStreamsAck;
                newMessage.getConnectedStreamsAck = client->connectedStreams;
                MESSAGE_SERIALIZE(newMessage, (*bytes));
            });

            return clientSend(client, bytes, true);
        } break;
        case MessageTag_getClientStreams: {
            IN_MUTEX(serverData.mutex, gcs2, {
                message->tag = MessageTag_getClientStreamsAck;
                message->getClientStreamsAck.allocator = currentAllocator;
                getClientsStreams(client, &message->getClientStreamsAck);
                MESSAGE_SERIALIZE((*message), (*bytes));
            });

            return clientSend(client, bytes, true);
        } break;
        case MessageTag_getStream: {
            IN_MUTEX(serverData.mutex, gs, {
                const Stream* stream = NULL;
                switch (message->getStream.tag) {
                    case StringOrGuidTag_name:
                        GetStreamFromName(&serverData.streams,
                                          &message->getStream.name,
                                          &stream,
                                          NULL);
                        break;
                    default:
                        GetStreamFromGuid(&serverData.streams,
                                          &message->getStream.id,
                                          &stream,
                                          NULL);
                        break;
                }
                Message newMessage = { 0 };
                newMessage.tag = MessageTag_getStreamAck;
                if (stream == NULL) {
                    newMessage.getStreamAck.tag = OptionalStreamTag_none;
                    newMessage.getStreamAck.none = NULL;
                } else {
                    newMessage.getStreamAck.tag = OptionalStreamTag_stream;
                    newMessage.getStreamAck.stream = *stream;
                }

                MESSAGE_SERIALIZE(newMessage, (*bytes));
            });

            return clientSend(client, bytes, false);
        } break;
        case MessageTag_getClients: {
            IN_MUTEX(serverData.mutex, get2, {
                message->tag = MessageTag_getClientsAck;
                message->getClientsAck.allocator = currentAllocator;
                for (size_t i = 0; i < serverData.clients.used; ++i) {
                    const pClient* client = &serverData.clients.buffer[i];
                    TemLangStringListAppend(&message->getClientsAck,
                                            &(*client)->name);
                }
                MESSAGE_SERIALIZE((*message), (*bytes));
            });

            return clientSend(client, bytes, true);
        } break;
        case MessageTag_getAllData: {
            IN_MUTEX(serverData.mutex, get5641, {
                Message newMessage = { 0 };
                newMessage.tag = MessageTag_getAllDataAck;
                newMessage.getAllDataAck.allStreams = serverData.streams;
                newMessage.getAllDataAck.connectedStreams =
                  client->connectedStreams;
                getClientsStreams(client,
                                  &newMessage.getAllDataAck.clientStreams);
                MESSAGE_SERIALIZE(newMessage, (*bytes));
                GuidListFree(&newMessage.getAllDataAck.clientStreams);
            });

            return clientSend(client, bytes, true);
        } break;
        default:
            printf("Got invalid message '%s' (%d) from client '%s'\n",
                   MessageTagToCharString(message->tag),
                   message->tag,
                   buffer);
            break;
    }
    return true;
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

    switch (poll(&pfds, 1, LONG_POLL_WAIT)) {
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
    MESSAGE_DESERIALIZE(message, bytes);
    if (message.tag == MessageTag_authenticate) {
        if (authenticateClient(client, &message.authenticate, &rs)) {
            MessageFree(&message);
            message.tag = MessageTag_authenticateAck;
            TemLangStringCopy(
              &message.authenticateAck.name, &client->name, currentAllocator);
            message.authenticateAck.id = client->id;
            MESSAGE_SERIALIZE(message, bytes);
            clientSend(client, &bytes, false);
            printf("Client '%s' is named '%s'\n", buffer, client->name.buffer);
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

    size_t messages = 0;
    while (!appDone) {
        switch (poll(&pfds, 1, SERVER_POLL_WAIT)) {
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
        ++messages;
        printf("Got %zu messages from '%s'\n", messages, client->name.buffer);

        bytes.used = (uint32_t)size;
        MESSAGE_DESERIALIZE(message, bytes);
        if (!serverHandleMessage(
              client, pfds.fd, &message, &bytes, buffer, &rs)) {
            goto end;
        }
    }

end:
    printf("Closing connection: %s (%s)\n", client->name.buffer, buffer);
    IN_MUTEX(serverData.mutex, fend, {
        pClientListSwapRemoveValue(&serverData.clients, &client);
    });

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
        switch (poll(&pfds, 1, SERVER_POLL_WAIT)) {
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

        client->connectedStreams.allocator = currentAllocator;

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
        switch (poll(&pfds, 1, SERVER_POLL_WAIT)) {
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
        MESSAGE_DESERIALIZE(message, bytes);
        if (message.tag == MessageTag_streamMessage) {
            IN_MUTEX(serverData.mutex, endClientFind, {
                const pClient* writer = NULL;
                if (GetClientFromGuid(&serverData.clients,
                                      &message.streamMessage.id,
                                      &writer,
                                      NULL)) {
                    if (setStreamMessageTimestamp(*writer, &message)) {
                        MESSAGE_SERIALIZE(message, bytes);
                    }
                    copyMessageToClients(
                      *writer, &message.streamMessage, &bytes);
                }
            });
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
            if (GetClientFromGuid(
                  &serverData.clients, &stream->owner, NULL, NULL)) {
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
    serverData.storage.allocator = currentAllocator;
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