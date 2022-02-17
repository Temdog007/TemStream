#include <include/main.h>

ProducerConfiguration
defaultProducerConfiguration()
{
    char hostname[_SC_HOST_NAME_MAX];
    gethostname(hostname, sizeof(hostname));
    ProducerConfiguration config = { .streamType = StreamType_Invalid,
                                     .name = { .allocator =
                                                 currentAllocator } };
    TemLangStringAppendFormat(config.name, "%s's stream", hostname);
    return config;
}

bool
parseProducerConfiguration(const int argc,
                           const char** argv,
                           pAllConfiguration configuration)
{
    pProducerConfiguration prod = &configuration->configuration.producer;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const char* value = argv[i + 1];
        const size_t keyLen = strlen(key);
        STR_EQUALS(key, "-T", keyLen, { goto parseType; });
        STR_EQUALS(key, "--type", keyLen, { goto parseType; });
        STR_EQUALS(key, "-N", keyLen, { goto parseName; });
        STR_EQUALS(key, "--name", keyLen, { goto parseName; });
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Producer", key, value);
            return false;
        }
        continue;

    parseType : {
        const size_t valueSize = strlen(value);
        const TemLangString temp = { .buffer = (char*)value,
                                     .size = valueSize,
                                     .used = valueSize };
        prod->streamType = StreamTypeFromString(&temp);
        continue;
    }
    parseName : {
        prod->name = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    }
    return true;
}

int
printProducerConfiguration(const ProducerConfiguration* configuration)
{
    return printf("Mode: %s\n",
                  StreamTypeToCharString(configuration->streamType));
}

int
runTextProducer(const TemLangString* name, const int fd)
{
    Bytes bytes = { .allocator = currentAllocator };
    Message message = { 0 };
    message.tag = MessageTag_createStream;
    message.createStream.name = *name;
    message.createStream.type = StreamType_Text;
    message.createStream.authentication = 0;
    MessageSerialize(&message, &bytes, true);
    if (send(fd, bytes.buffer, bytes.used, 0) != (ssize_t)bytes.used) {
        perror("send");
        goto end;
    }

    char buffer[KB(4)] = { 0 };
    struct pollfd pfds = { .events = POLLIN, .revents = 0, .fd = STDIN_FILENO };

    while (!appDone) {
        printf("Enter text to send to server: ");
        fflush(stdout);
        while (!appDone) {
            switch (poll(&pfds, 1, 1000)) {
                case -1:
                case 0:
                    continue;
                default:
                    break;
            }
            ssize_t size = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (size <= 0) {
                continue;
            }

            message.tag = MessageTag_streamMessage;
            message.streamMessage.tag = StreamMessageTag_text;
            message.streamMessage.text =
              TemLangStringCreateFromSize(buffer, size + 1, currentAllocator);
            bytes.used = 0;
            MessageSerialize(&message, &bytes, true);
            {
                struct pollfd temp = { .events = POLLERR | POLLHUP,
                                       .revents = 0,
                                       .fd = fd };
                if (poll(&temp, 1, 100) < 0 ||
                    (temp.revents & (POLLERR | POLLHUP)) != 0) {
                    fputs("Pipe is broken. Cannot send message\n", stderr);
                    goto end;
                } else if (send(fd, bytes.buffer, bytes.used, 0) !=
                           (ssize_t)bytes.used) {
                    perror("send");
                    goto end;
                }
            }
            MessageFree(&message);
            break;
        }
    }
end:
    MessageFree(&message);
    uint8_tListFree(&bytes);
    return EXIT_SUCCESS;
}

int
runProducer(const AllConfiguration* configuration)
{
    puts("Running producer");
    printAllConfiguration(configuration);

    int result = EXIT_FAILURE;
    const int tcp =
      openSocketFromAddress(&configuration->address, SocketOptions_Tcp);
    const int udp = openSocketFromAddress(&configuration->address, 0);
    if (tcp == INVALID_SOCKET || udp == INVALID_SOCKET) {
        fprintf(stderr, "%s failed\n", tcp == INVALID_SOCKET ? "tcp" : "udp");
        goto end;
    }
#if _DEBUG
    printf("tcp: %d; udp: %d\n", tcp, udp);
#endif

    const ProducerConfiguration* config =
      &configuration->configuration.producer;
    switch (config->streamType) {
        case StreamType_Text:
            result = runTextProducer(&config->name, tcp);
            break;
        default:
            fprintf(stderr,
                    "Producer type '%s' has not been implemented...\n",
                    StreamTypeToCharString(config->streamType));
            break;
    }
end:
    closeSocket(tcp);
    closeSocket(udp);
    return result;
}