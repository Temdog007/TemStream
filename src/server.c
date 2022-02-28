#include <include/main.h>

typedef struct ServerData
{
    SDL_mutex* mutex;
    SDL_cond* cond;

    bool needToUpdateClients;
    StreamList streams;
    ServerIncomingList incoming;
    ServerOutgoingList outgoing;
    StreamMessageList storage;
} ServerData, *pServerData;

ServerData serverData = { 0 };

void
sendServerDataToClient(pClient client);

void
ServerDataFree(pServerData data)
{
    SDL_DestroyMutex(data->mutex);
    SDL_DestroyCond(data->cond);

    StreamListFree(&data->streams);

    ServerIncomingListFree(&data->incoming);
    ServerOutgoingListFree(&data->outgoing);
    StreamMessageListFree(&data->storage);
}

// Assigns client a name and id also
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
                    client->id = randomGuid(rs);
                    return TemLangStringCopy(&client->name,
                                             &cAuth->credentials.username,
                                             currentAllocator);
                case ClientAuthenticationTag_token:
                    fprintf(stderr,
                            "Token authentication is not implemented\n");
                    return false;
                default:
                    // Give client random name and id
                    client->name = RandomClientName(rs);
                    client->id = randomGuid(rs);
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

void
sendResponse(const ServerOutgoing* r)
{
    IN_MUTEX(serverData.mutex, end, {
        ServerOutgoingListAppend(&serverData.outgoing, r);
    });
}

void
sendDisconnect(pClient client)
{
    ServerOutgoing o = { .tag = ServerOutgoingTag_disconnect,
                         .disconnect = client };
    sendResponse(&o);
}

bool
setStreamMessageTimestamp(const Client* client, pStreamMessage streamMessage)
{
    switch (streamMessage->data.tag) {
        case StreamMessageDataTag_chatMessage:
            streamMessage->data.chatMessage.timeStamp = (int64_t)time(NULL);
            return TemLangStringCopy(&streamMessage->data.chatMessage.author,
                                     &client->name,
                                     currentAllocator);
        default:
            break;
    }
    return false;
}

void
copyMessageToClients(ENetHost* server,
                     const Client* writer,
                     const StreamMessage* message,
                     const Bytes* bytes)
{
    const bool reliable = streamMessageIsReliable(message);
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

        if (!streamTypeMatchesMessage(stream->type, message->data.tag) ||
            !clientHasWriteAccess(writer, stream)) {
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
                    case StreamMessageDataTag_audio:
                        s.data.tag = StreamMessageDataTag_audio;
                        s.data.audio.allocator = currentAllocator;
                    default:
                        s.data.tag = StreamMessageDataTag_none;
                        s.data.none = NULL;
                        break;
                }
                StreamMessageListAppend(&serverData.storage, &s);
                StreamMessageFree(&s);
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
                case StreamMessageDataTag_image:
                    uint8_tListCopy(&storage->data.image,
                                    &message->data.image,
                                    currentAllocator);
                    break;
                case StreamMessageDataTag_audio:
                    // Keep initial audio empty. This is just to signal
                    // to client that they have connected to an audio stream
                    // and should prepare for playback
                    break;
                default:
                    printf("Unexpected message type: %s\n",
                           StreamMessageDataTagToCharString(message->data.tag));
                    goto endMutex;
            }
        }

        for (size_t i = 0; i < server->peerCount; ++i) {
            ENetPeer* peer = &server->peers[i];
            pClient client = peer->data;
            if (client == NULL) {
                continue;
            }
            if (serverData.needToUpdateClients) {
                sendServerDataToClient(client);
            }
            if (!GuidListFind(
                  &client->connectedStreams, &message->id, NULL, NULL) ||
                !clientHasReadAccess(client, stream)) {
                continue;
            }
            ENetPacket* packet = BytesToPacket(bytes, reliable);
            enet_peer_send(peer, SERVER_CHANNEL, packet);
        }
        serverData.needToUpdateClients = false;
    });
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
    GuidListFree(guids);
    guids->allocator = currentAllocator;
    for (size_t i = 0; i < serverData.streams.used; ++i) {
        const Stream* stream = &serverData.streams.buffer[i];
        if (GuidEquals(&stream->owner, &client->id)) {
            GuidListAppend(guids, &stream->id);
        }
    }
}

void
cleanupConnectedStreams(pClient client)
{
    size_t i = 0;
    while (i < client->connectedStreams.used) {
        if (GetStreamFromGuid(&serverData.streams,
                              &client->connectedStreams.buffer[i],
                              NULL,
                              NULL)) {
            ++i;
        } else {
            GuidListSwapRemove(&client->connectedStreams, i);
        }
    }
}

void
sendServerDataToClient(pClient client)
{
    cleanupConnectedStreams(client);
    ServerOutgoing o = { 0 };
    o.tag = ServerOutgoingTag_response;
    o.response.client = client;
    pMessage message = &o.response.message;
    message->tag = MessageTag_serverData;
    message->serverData.allStreams = serverData.streams;
    message->serverData.connectedStreams = client->connectedStreams;
    getClientsStreams(client, &message->serverData.clientStreams);
    sendResponse(&o);
    GuidListFree(&message->serverData.clientStreams);
}

void
serverHandleMessage(pClient client, const Message* message, pRandomState rs)
{
    ServerOutgoing outgoing = { 0 };
    outgoing.tag = ServerOutgoingTag_response;
    outgoing.response.client = (void*)client;
    pMessage rMessage = &outgoing.response.message;
    switch (message->tag) {
        case MessageTag_connectToStream: {
            const Stream* stream = NULL;
            const StreamMessage* storage = NULL;
            const bool success = GetStreamFromGuid(
              &serverData.streams, &message->connectToStream, &stream, NULL);
            GetStreamMessageFromGuid(
              &serverData.storage, &message->connectToStream, &storage, NULL);
            if (success) {
                if (!GuidListFind(
                      &client->connectedStreams, &stream->id, NULL, NULL)) {
                    GuidListAppend(&client->connectedStreams, &stream->id);
                    printf("Client '%s' connected to stream '%s'\n",
                           client->name.buffer,
                           stream->name.buffer);
                }
            }

            rMessage->tag = MessageTag_connectToStreamAck;
            if (success) {
                rMessage->connectToStreamAck.tag =
                  OptionalStreamMessageTag_streamMessage;
                if (storage == NULL) {
                    rMessage->connectToStreamAck.streamMessage.id = stream->id;
                    rMessage->connectToStreamAck.streamMessage.data.tag =
                      StreamMessageDataTag_none;
                    rMessage->connectToStreamAck.streamMessage.data.none = NULL;
                } else {
                    StreamMessageCopy(
                      &rMessage->connectToStreamAck.streamMessage,
                      storage,
                      currentAllocator);
                }
            } else {
                rMessage->connectToStreamAck.none = NULL;
                rMessage->connectToStreamAck.tag =
                  OptionalStreamMessageTag_none;
            }
            sendResponse(&outgoing);
            sendServerDataToClient(client);
        } break;
        case MessageTag_disconnectFromStream: {
            const Stream* stream = NULL;
            const bool success =
              GetStreamFromGuid(&serverData.streams,
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

            rMessage->tag = MessageTag_disconnectFromStreamAck;
            rMessage->disconnectFromStreamAck = success;
            sendResponse(&outgoing);
            sendServerDataToClient(client);
        } break;
        case MessageTag_startStreaming: {
            const Stream* stream = NULL;
            GetStreamFromName(&serverData.streams,
                              &message->startStreaming.name,
                              &stream,
                              NULL);
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
                StreamListAppend(&serverData.streams, &newStream);
                switch (newStream.type) {
                    case StreamType_Chat: {
                        StreamMessage m = { 0 };
                        m.id = newStream.id;
                        m.data.tag = StreamMessageDataTag_chatLogs;
                        m.data.chatLogs.allocator = currentAllocator;
                        StreamMessageListAppend(&serverData.storage, &m);
                        StreamMessageFree(&m);
                    } break;
                    case StreamType_Image: {
                        StreamMessage m = { 0 };
                        m.id = newStream.id;
                        m.data.tag = StreamMessageDataTag_image;
                        m.data.image.allocator = currentAllocator;
                        StreamMessageListAppend(&serverData.storage, &m);
                        StreamMessageFree(&m);
                    } break;
                    case StreamType_Audio: {
                        StreamMessage m = { 0 };
                        m.id = newStream.id;
                        m.data.tag = StreamMessageDataTag_audio;
                        m.data.audio.allocator = currentAllocator;
                        StreamMessageListAppend(&serverData.storage, &m);
                        StreamMessageFree(&m);
                    } break;
                    default:
                        break;
                }

                rMessage->tag = MessageTag_startStreamingAck;
                rMessage->startStreamingAck.guid = newStream.id;
                rMessage->startStreamingAck.tag = OptionalGuidTag_guid;
                serverData.needToUpdateClients = true;
            } else {
#if _DEBUG
                printf("Client '%s' tried to add duplicate stream '%s'\n",
                       client->name.buffer,
                       stream->name.buffer);
#endif
                rMessage->tag = MessageTag_startStreamingAck;
                rMessage->startStreamingAck.none = NULL;
                rMessage->startStreamingAck.tag = OptionalGuidTag_none;
            }
            sendResponse(&outgoing);
            sendServerDataToClient(client);
        } break;
        case MessageTag_stopStreaming: {
            const Stream* stream = NULL;
            size_t i = 0;
            GetStreamFromGuid(
              &serverData.streams, &message->stopStreaming, &stream, &i);
            rMessage->tag = MessageTag_stopStreamingAck;
            rMessage->stopStreamingAck.tag = OptionalGuidTag_none;
            rMessage->stopStreamingAck.none = NULL;
            if (stream != NULL && GuidEquals(&stream->owner, &client->id)) {
                printf("Client '%s' ended stream '%s'\n",
                       client->name.buffer,
                       stream->name.buffer);
                StreamListSwapRemove(&serverData.streams, i);
                rMessage->stopStreamingAck.tag = OptionalGuidTag_guid;
                rMessage->stopStreamingAck.guid = stream->id;
            } else {
                printf("Client '%s' tried end a stream that it either didn't "
                       "own or didn't exists\n",
                       client->name.buffer);
            }
            sendResponse(&outgoing);
            serverData.needToUpdateClients = true;
            sendServerDataToClient(client);
        } break;
        case MessageTag_getClients: {
            outgoing.tag = ServerOutgoingTag_getClients;
            outgoing.getClients = client;
            sendResponse(&outgoing);
        } break;
        default:
            printf("Got invalid message '%s' (%d) from client '%s'\n",
                   MessageTagToCharString(message->tag),
                   message->tag,
                   client->name.buffer);
            break;
    }
    ServerOutgoingFree(&outgoing);
}

void
handleServerIncoming(ServerIncoming incoming, pRandomState rs)
{
    pMessage message = incoming.message;
    pClient client = incoming.client;
    if (message == NULL) {
        ClientFree(client);
        currentAllocator->free(client);
        return;
    }
    if (GuidEquals(&client->id, &ZeroGuid)) {
        if (message->tag == MessageTag_authenticate) {
            if (authenticateClient(client, &message->authenticate, rs)) {
                ServerOutgoing outgoing = { 0 };
                outgoing.tag = ServerOutgoingTag_response;
                outgoing.response.client = client;
                pMessage rMessage = &outgoing.response.message;
                rMessage->tag = MessageTag_authenticateAck;
                TemLangStringCopy(&rMessage->authenticateAck.name,
                                  &client->name,
                                  currentAllocator);
                rMessage->authenticateAck.id = client->id;
                sendResponse(&outgoing);
                ServerOutgoingFree(&outgoing);
                char buffer[128];
                getGuidString(&client->id, buffer);
                printf("Client assigned name '%s' (%s)\n",
                       client->name.buffer,
                       buffer);
                sendServerDataToClient(client);
            } else {
                puts("Client failed authentication\n");
                sendDisconnect(client);
            }
        } else {
            printf("Expected authentication from client. Got '%s'\n",
                   MessageTagToCharString(message->tag));
            sendDisconnect(client);
        }
    } else {
        serverHandleMessage(client, message, rs);
    }
    MessageFree(message);
    currentAllocator->free(message);
}

int
serverIncomingThread(void* ptr)
{
    (void)ptr;

    RandomState rs = makeRandomState();
    pServerIncomingList list = &serverData.incoming;
    while (!appDone) {
        IN_MUTEX(serverData.mutex, end, {
            while (!appDone && ServerIncomingListIsEmpty(list)) {
                SDL_CondWait(serverData.cond, serverData.mutex);
            }
            if (appDone) {
                goto end;
            }
            size_t i = 0;
            while (i < list->used) {
                const ServerIncoming incoming = list->buffer[i];
                ++i;
                handleServerIncoming(incoming, &rs);
            }
            list->used = 0;
        });
    }

    return EXIT_SUCCESS;
}

int
runServer(const AllConfiguration* configuration)
{
    ENetHost* server = NULL;
    int result = EXIT_FAILURE;
    SDL_Thread* thread = NULL;
    Bytes bytes = { .allocator = currentAllocator };
    ServerIncomingList localIncoming = { .allocator = currentAllocator };
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        goto end;
    }

    serverData.mutex = SDL_CreateMutex();
    if (serverData.mutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
    }

    serverData.cond = SDL_CreateCond();
    if (serverData.cond == NULL) {
        fprintf(stderr, "Failed to create cond: %s\n", SDL_GetError());
        goto end;
    }

    serverData.streams.allocator = currentAllocator;
    serverData.storage.allocator = currentAllocator;
    serverData.incoming.allocator = currentAllocator;
    serverData.outgoing.allocator = currentAllocator;
    puts("Running server");
    printAllConfiguration(configuration);

    appDone = false;

    const ServerConfiguration* config = &configuration->configuration.server;

    {
        char* end = NULL;
        ENetAddress address = { 0 };
        enet_address_set_host(&address, configuration->address.ip.buffer);
        address.port =
          (uint16_t)SDL_strtoul(configuration->address.port.buffer, &end, 10);
        server = enet_host_create(&address, config->maxClients, 2, 0, 0);
    }
    if (server == NULL) {
        fprintf(stderr, "Failed to create server\n");
        goto end;
    }

    thread = SDL_CreateThread(
      (SDL_ThreadFunction)serverIncomingThread, "packet", NULL);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto end;
    }

    ENetEvent event = { 0 };
    while (!appDone) {
        if (localIncoming.used > 100) {
            goto checkOutgoing;
        }
        while (enet_host_service(server, &event, 0U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    char buffer[512] = { 0 };
                    enet_address_get_host_ip(
                      &event.peer->address, buffer, sizeof(buffer));
                    printf("New client from %s:%u\n",
                           buffer,
                           event.peer->address.port);
                    pClient client = currentAllocator->allocate(sizeof(Client));
                    client->serverAuthentication = &config->authentication;
                    // name, id, and authentication will be set after parsing
                    // authentication message
                    client->connectedStreams.allocator = currentAllocator;
                    event.peer->data = client;
                } break;
                case ENET_EVENT_TYPE_DISCONNECT: {
                    pClient client = (pClient)event.peer->data;
                    printf("%s disconnected\n", client->name.buffer);
                    event.peer->data = NULL;
                    IN_MUTEX(serverData.mutex, disEnd, {
                        size_t i = 0;
                        while (i < serverData.streams.used) {
                            const Stream* stream =
                              &serverData.streams.buffer[i];
                            if (GuidEquals(&stream->owner, &client->id)) {
                                printf("Closing stream '%s'\n",
                                       stream->name.buffer);
                                StreamListSwapRemove(&serverData.streams, i);
                            } else {
                                ++i;
                            }
                        }
                        ServerIncoming g = { 0 };
                        g.message = NULL;
                        g.client = client;
                        ServerIncomingListAppend(&localIncoming, &g);
                    });
                } break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    const Bytes temp = { .allocator = currentAllocator,
                                         .buffer = event.packet->data,
                                         .size = event.packet->dataLength,
                                         .used = event.packet->dataLength };
                    pMessage message =
                      currentAllocator->allocate(sizeof(Message));
                    MESSAGE_DESERIALIZE((*message), temp);
                    // printReceivedPacket(event.packet);
                    pClient client = (pClient)event.peer->data;
                    if (message->tag == MessageTag_streamMessage) {
                        if (setStreamMessageTimestamp(
                              client, &message->streamMessage)) {
                            MESSAGE_SERIALIZE((*message), bytes);
                            copyMessageToClients(
                              server, client, &message->streamMessage, &bytes);
                        } else {
                            copyMessageToClients(
                              server, client, &message->streamMessage, &temp);
                        }
                        MessageFree(message);
                        currentAllocator->free(message);
                    } else {
                        ServerIncoming incoming = { 0 };
                        incoming.message = message;
                        incoming.client = client;
                        ServerIncomingListAppend(&localIncoming, &incoming);
                    }
                    enet_packet_destroy(event.packet);
                    continue;
                } break;
                case ENET_EVENT_TYPE_NONE:
                    break;
                default:
                    break;
            }
        }
    checkOutgoing:
        SDL_Delay(1);
        IN_MUTEX(serverData.mutex, oEnd, {
            for (size_t i = 0; i < localIncoming.used; ++i) {
                ServerIncomingListAppend(&serverData.incoming,
                                         &localIncoming.buffer[i]);
            }
            localIncoming.used = 0;
            SDL_CondSignal(serverData.cond);
            for (size_t i = 0; i < serverData.outgoing.used; ++i) {
                const ServerOutgoing* so = &serverData.outgoing.buffer[i];
                switch (so->tag) {
                    case ServerOutgoingTag_disconnect: {
                        ENetPeer* peer = FindPeerFromData(
                          server->peers, server->peerCount, so->disconnect);
                        if (peer != NULL) {
                            enet_peer_disconnect(peer, 0U);
                        }
                    } break;
                    case ServerOutgoingTag_response: {
                        ENetPeer* peer = FindPeerFromData(server->peers,
                                                          server->peerCount,
                                                          so->response.client);
                        if (peer != NULL) {
                            MESSAGE_SERIALIZE(so->response.message, bytes);
                            ENetPacket* packet = BytesToPacket(&bytes, true);
                            // printSendingPacket(packet);
                            enet_peer_send(peer, SERVER_CHANNEL, packet);
                        }
                    } break;
                    case ServerOutgoingTag_getClients: {
                        pClient target = so->getClients;
                        ENetPeer* peer = NULL;
                        Message message = { 0 };
                        message.tag = MessageTag_getClientsAck;
                        message.getClientsAck.allocator = currentAllocator;
                        for (size_t i = 0; i < server->peerCount; ++i) {
                            const Client* client = server->peers[i].data;
                            if (client == NULL) {
                                continue;
                            }
                            if (client == target) {
                                peer = &server->peers[i];
                            }
                            TemLangStringListAppend(&message.getClientsAck,
                                                    &client->name);
                        }
                        if (peer != NULL) {
                            MESSAGE_SERIALIZE(message, bytes);
                            ENetPacket* packet = BytesToPacket(&bytes, true);
                            // printSendingPacket(packet);
                            enet_peer_send(peer, SERVER_CHANNEL, packet);
                        }
                        MessageFree(&message);
                    } break;
                    default:
                        break;
                }
            }
            ServerOutgoingListFree(&serverData.outgoing);
            serverData.outgoing.allocator = currentAllocator;
        });
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    IN_MUTEX(serverData.mutex, end2, { SDL_CondSignal(serverData.cond); });
    SDL_WaitThread(thread, NULL);
    for (size_t i = 0; i < server->peerCount; ++i) {
        pClient client = server->peers[i].data;
        if (client == NULL) {
            continue;
        }
        ClientFree(client);
        currentAllocator->free(client);
        server->peers[i].data = NULL;
    }
    if (server != NULL) {
        enet_host_destroy(server);
    }
    ServerIncomingListFree(&localIncoming);
    uint8_tListFree(&bytes);
    ServerDataFree(&serverData);
    SDL_Quit();
    return result;
}