#include <include/main.h>

DEFINE_RUN_SERVER(Image);

ImageConfiguration
defaultImageConfiguration()
{
    return (ImageConfiguration){ .none = NULL };
}

int
printImageConfiguration(const ImageConfiguration* configuration)
{
    (void)configuration;
    return puts("Image");
}

bool
parseImageConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    (void)argc;
    (void)argv;
    configuration->tag = ServerConfigurationDataTag_image;
    pImageConfiguration image = &configuration->server.data.image;
    (void)image;
    return true;
}

bool
onConnectForImage(pClient client,
                  pBytes bytes,
                  ENetPeer* peer,
                  const ServerConfiguration* config)
{
    (void)client;
    (void)bytes;
    char buffer[512];
    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;
    if (!mapFile(getServerFileName(config, buffer),
                 &fd,
                 &ptr,
                 &size,
                 MapFileType_Read)) {
        perror("Failed to open file");
        return true;
    }

    ImageMessage message = { 0 };
    message.tag = ImageMessageTag_imageStart;
    message.imageStart = NULL;
    MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
    sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);

    message.tag = ImageMessageTag_imageChunk;
    message.imageChunk.allocator = currentAllocator;
    for (size_t i = 0; i < size; i += peer->mtu) {
        message.imageChunk.used = 0;
        uint8_tListQuickAppend(
          &message.imageChunk, (uint8_t*)ptr + i, SDL_min(peer->mtu, size - i));
        MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
        sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
        enet_host_flush(peer->host);
    }
    ImageMessageFree(&message);

    message.tag = ImageMessageTag_imageEnd;
    message.imageEnd = NULL;
    MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
    sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);

    unmapFile(fd, ptr, size);
    return true;
}

bool
handleImageMessage(const void* ptr,
                   pBytes bytes,
                   ENetPeer* peer,
                   redisContext* ctx,
                   const ServerConfiguration* serverConfig)
{
    (void)ctx;

    pClient client = peer->data;
    // const ImageConfiguration* config = &serverConfig->data.image;
    bool result = false;
    CAST_MESSAGE(ImageMessage, ptr);
    switch (message->tag) {
        case ImageMessageTag_general: {
            ImageMessage imageMessage = { 0 };
            imageMessage.tag = TextMessageTag_general;
            result = handleGeneralMessage(
              &message->general, peer, &imageMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(ImageMessage, imageMessage, (*bytes));
                sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
            }
            ImageMessageFree(&imageMessage);
        } break;
        case ImageMessageTag_imageStart:
        case ImageMessageTag_imageChunk:
        case ImageMessageTag_imageEnd:
            result = true;
            if (!clientHasWriteAccess(client, serverConfig)) {
                printf("Client '%s' sent an image message when it doesn't have "
                       "write access",
                       client->name.buffer);
                break;
            }
            MESSAGE_SERIALIZE(ImageMessage, (*message), (*bytes));
            switch (message->tag) {
                case ImageMessageTag_imageStart:
                    writeServerFileBytes(serverConfig, NULL, true);
                    break;
                case ImageMessageTag_imageChunk:
                    if (uint8_tListIsEmpty(&message->imageChunk)) {
                        goto end;
                    }
                    writeServerFileBytes(serverConfig, bytes, false);
                    break;
                default:
                    break;
            }
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, true);
            sendPacketToReaders(peer->host, packet, &serverConfig->readers);
            break;
        default:
            break;
    }
end:
    if (!result) {
#if _DEBUG
        printf("Unexpected message '%s' from client\n",
               ImageMessageTagToCharString(message->tag));
#endif
    }
    return result;
}