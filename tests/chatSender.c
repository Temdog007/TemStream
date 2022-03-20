#include <include/main.h>

#include <src/base64.c>
#include <src/misc.c>

const Allocator* currentAllocator = NULL;
bool appDone = true;

int
main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "Need ip address\n");
        return EXIT_FAILURE;
    }

    int result = EXIT_FAILURE;
    const int messages = argc == 3 ? 10 : atoi(argv[3]);

    Allocator allocator = makeDefaultAllocator();
    currentAllocator = &allocator;

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    ChatMessage message = { 0 };

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        goto end;
    }
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, argv[1]);
        address.port = (uint16_t)atoi(argv[2]);
        peer =
          enet_host_connect(host, &address, 2, ServerConfigurationDataTag_chat);
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
        message.tag = ChatMessageTag_general;
        message.general.tag = GeneralMessageTag_authenticate;
        message.general.authenticate.tag = ClientAuthenticationTag_none;
        message.general.authenticate.none = NULL;
        MESSAGE_SERIALIZE(ChatMessage, message, bytes);
        sendBytes(peer, 1, CLIENT_CHANNEL, &bytes, SendFlags_Normal);
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        goto end;
    }

    if (SDL_Init(0) != 0) {
        goto end;
    }

    RandomState rs = makeRandomState();
    int sent = 0;
    // Wait for chat interval
    while (enet_host_service(host, &event, 3100U) >= 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("Disconnecting...");
                goto end;
            case ENET_EVENT_TYPE_RECEIVE: {
                const Bytes packetBytes = { .allocator = currentAllocator,
                                            .buffer = event.packet->data,
                                            .size = event.packet->dataLength,
                                            .used = event.packet->dataLength };
                MESSAGE_DESERIALIZE(ChatMessage, message, packetBytes);
                switch (message.tag) {
                    case ChatMessageTag_general:
                        switch (message.general.tag) {
                            case GeneralMessageTag_authenticateAck: {
                                printf("Client name: %s\n",
                                       message.general.authenticateAck.buffer);
                            } break;
                            default:
                                break;
                        }
                        break;
                    default:
                        break;
                }
                enet_packet_destroy(event.packet);
            } break;
            case ENET_EVENT_TYPE_NONE: {
                ChatMessageFree(&message);
                message.tag = ChatMessageTag_message;
                message.message = RandomString(&rs, 10U, 128U);
                printf("Sending message: %s\n", message.message.buffer);
                MESSAGE_SERIALIZE(ChatMessage, message, bytes);
                sendBytes(peer, 1, CLIENT_CHANNEL, &bytes, SendFlags_Normal);
                ++sent;
                if (sent >= messages) {
                    enet_peer_disconnect(peer, 0);
                }
            } break;
            default:
                break;
        }
    }

end:
    uint8_tListFree(&bytes);
    closeHostAndPeer(host, peer);
    ChatMessageFree(&message);
    SDL_Quit();

    return result;
}