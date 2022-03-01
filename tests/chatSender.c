#include <include/main.h>

#include <src/misc.c>

const Allocator* currentAllocator = NULL;
bool appDone = true;

int
main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "Need ip address and name of chat stream\n");
        return EXIT_FAILURE;
    }

    const int messages = argc == 3 ? 10 : atoi(argv[3]);

    Allocator allocator = makeDefaultAllocator();
    currentAllocator = &allocator;

    IpAddress ipAddress = { 0 };
    if (!parseIpAddress(argv[1], &ipAddress)) {
        return EXIT_FAILURE;
    }

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    Message message = { 0 };

    host = enet_host_create(NULL, 1, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        goto end;
    }
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, ipAddress.ip.buffer);
        char* end = NULL;
        address.port = (uint16_t)strtoul(ipAddress.port.buffer, &end, 10);
        peer = enet_host_connect(host, &address, 2, 0);
        char buffer[512] = { 0 };
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));
        printf("Connecting to server: %s:%u...\n", buffer, address.port);
    }
    if (peer == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        goto end;
    }

    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };

    ENetEvent event = { 0 };
    if (enet_host_service(host, &event, 500) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        char buffer[512] = { 0 };
        enet_address_get_host_ip(&event.peer->address, buffer, sizeof(buffer));
        printf(
          "Connected to server: %s:%u\n", buffer, event.peer->address.port);
        message.tag = MessageTag_authenticate;
        message.authenticate.tag = ClientAuthenticationTag_none;
        message.authenticate.none = NULL;
        MESSAGE_SERIALIZE(message, bytes);
        ENetPacket* packet = BytesToPacket(&bytes, true);
        // printSendingPacket(packet);
        PEER_SEND(peer, CLIENT_CHANNEL, packet);
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        goto end;
    }

    while (enet_host_service(host, &event, 500) >= 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("Disconnecting...");
                goto end;
            case ENET_EVENT_TYPE_RECEIVE: {
                const Bytes packetBytes = { .allocator = currentAllocator,
                                            .buffer = event.packet->data,
                                            .size = event.packet->dataLength,
                                            .used = event.packet->dataLength };
                MESSAGE_DESERIALIZE(message, packetBytes);
                switch (message.tag) {
                    case MessageTag_authenticateAck:
                        printf("Client name: %s\n",
                               message.authenticateAck.name.buffer);
                        break;
                    case MessageTag_serverData: {
                        TemLangString streamName =
                          TemLangStringCreate(argv[2], currentAllocator);
                        const Stream* stream = NULL;
                        if (!GetStreamFromName(&message.serverData.allStreams,
                                               &streamName,
                                               &stream,
                                               NULL)) {
                            fprintf(
                              stderr, "Failed to find stream: %s\n", argv[2]);
                            enet_peer_disconnect(event.peer, 0);
                            goto end2;
                        }

                        Message newMessage = { 0 };
                        newMessage.tag = MessageTag_streamMessage;
                        newMessage.streamMessage.id = stream->id;
                        newMessage.streamMessage.data.tag =
                          StreamMessageDataTag_chatMessage;
                        newMessage.streamMessage.data.chatMessage.message
                          .allocator = currentAllocator;
                        printf("Sending %d messages...\n", messages);
                        // RandomState rs = makeRandomState();
                        const time_t t = time(NULL);
                        char timeBuffer[512] = { 0 };
                        strftime(
                          timeBuffer, sizeof(timeBuffer), "%c", localtime(&t));
                        for (int i = 0; i < messages; ++i) {
                            newMessage.streamMessage.data.chatMessage.message
                              .used = 0;
                            // message.streamMessage.data.chatMessage.message =
                            //   RandomString(&rs, 32, 1024);
                            TemLangStringAppendFormat(
                              newMessage.streamMessage.data.chatMessage.message,
                              "%s\nmessage #%d",
                              timeBuffer,
                              i);
                            MESSAGE_SERIALIZE(newMessage, bytes);
                            // printf("Sending message #%d\n", i);
                            ENetPacket* packet = BytesToPacket(&bytes, true);
                            PEER_SEND(peer, CLIENT_CHANNEL, packet);
                        }
                        puts("Done sending messages");
                        MessageFree(&newMessage);
                        enet_peer_disconnect_later(peer, 0);

                    end2:
                        TemLangStringFree(&streamName);
                    } break;
                    default:
                        break;
                }
                enet_packet_destroy(event.packet);
            } break;
            default:
                break;
        }
    }

end:
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
    MessageFree(&message);

    return EXIT_SUCCESS;
}