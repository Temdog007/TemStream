#include <include/main.h>

#include <src/misc.c>
#include <src/networking.c>

const Allocator* currentAllocator = NULL;
bool appDone = true;

int
main(int argc, char** argv)
{
    if (argc < 4) {
        fprintf(stderr,
                "Need ip address, name of chat stream, and number of messages "
                "to send\n");
        return EXIT_FAILURE;
    }

    Allocator allocator = makeDefaultAllocator();
    currentAllocator = &allocator;

    Address address = { 0 };
    if (!parseAddress(argv[1], &address)) {
        return EXIT_FAILURE;
    }

    {
        const int s = openSocketFromAddress(&address, SocketOptions_Tcp);
        if (s <= 0) {
            return EXIT_FAILURE;
        }

        Bytes bytes = { .allocator = currentAllocator,
                        .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                        .size = MAX_PACKET_SIZE,
                        .used = 0 };
        ssize_t size = 0;
        Message message = { 0 };

        message.tag = MessageTag_authenticate;
        message.authenticate.tag = ClientAuthenticationTag_none;
        message.authenticate.none = NULL;
        MESSAGE_SERIALIZE(message, bytes);
        if (!socketSend(s, &bytes, false)) {
            goto end;
        }

        size = recv(s, bytes.buffer, bytes.size, 0);
        if (size <= 0) {
            goto end;
        }
        bytes.used = (uint32_t)size;

        MESSAGE_DESERIALIZE(message, bytes);
        if (message.tag != MessageTag_authenticateAck) {
            goto end;
        }

        MessageFree(&message);
        message.tag = MessageTag_getStream;
        message.getStream.tag = StringOrGuidTag_name;
        message.getStream.name = TemLangStringCreate(argv[2], currentAllocator);
        MESSAGE_SERIALIZE(message, bytes);
        if (!socketSend(s, &bytes, false)) {
            goto end;
        }

        size = recv(s, bytes.buffer, bytes.size, 0);
        if (size <= 0) {
            goto end;
        }
        bytes.used = (uint32_t)size;

        MESSAGE_DESERIALIZE(message, bytes);
        if (message.tag != MessageTag_getStreamAck ||
            message.getStreamAck.tag != OptionalStreamTag_stream) {
            fprintf(stderr, "Failed to find the stream: %s\n", argv[2]);
            goto end;
        }

        const Guid id = message.getStreamAck.stream.id;
        MessageFree(&message);
        message.tag = MessageTag_streamMessage;
        message.streamMessage.id = id;
        message.streamMessage.data.tag = StreamMessageDataTag_chatMessage;
        const int messages = atoi(argv[3]);
        printf("Sending %d messages...\n", messages);
        RandomState rs = makeRandomState();
        for (int i = 0; i < messages; ++i) {
            TemLangStringFree(&message.streamMessage.data.chatMessage.message);
            message.streamMessage.data.chatMessage.message =
              RandomClientName(&rs);
            TemLangStringAppendFormat(
              message.streamMessage.data.chatMessage.message, "_%d", i);
            MESSAGE_SERIALIZE(message, bytes);
            printf("Sending message #%d\n", i);
            if (!socketSend(s, &bytes, false)) {
                goto end;
            }
        }
        puts("Done sending messages");

    end:
        MessageFree(&message);
        uint8_tListFree(&bytes);
        closeSocket(s);
    }
    return EXIT_SUCCESS;
}