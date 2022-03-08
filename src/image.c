#include <include/main.h>

DEFINE_RUN_SERVER(Image);

bool needImageSend = false;
size_t imageSendOffset = 0;

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

void
onImageDownTime(ENetHost* host, pBytes b)
{
    if (!needImageSend) {
        return;
    }

    ENetPacket* packet = NULL;
    ImageMessage m = { 0 };
    if (imageSendOffset == 0U) {
        m.tag = ImageMessageTag_imageStart;
        m.imageStart = NULL;
        MESSAGE_SERIALIZE(ImageMessage, m, (*b));
        packet = BytesToPacket(b->buffer, b->used, true);
        sendPacketToReaders(host, packet, &gServerConfig->readers);
    }

    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;
    char buffer[512];
    if (!mapFile(getServerFileName(gServerConfig, buffer),
                 &fd,
                 &ptr,
                 &size,
                 MapFileType_Read)) {
        return;
    }

    m.tag = ImageMessageTag_imageChunk;
    while (imageSendOffset < size && !lowMemory()) {
        m.imageChunk.used = 0;
        m.imageChunk.buffer = (uint8_t*)ptr + imageSendOffset;
        const size_t s = SDL_min(size - imageSendOffset, host->mtu);
        m.imageChunk.used = s;
        m.imageChunk.size = s;
        MESSAGE_SERIALIZE(ImageMessage, m, (*b));
        packet = BytesToPacket(b->buffer, b->used, true);
        sendPacketToReaders(host, packet, &gServerConfig->readers);
        imageSendOffset += s;
#if PRINT_CHUNKS
        printf("Sending image chunk: %zu\n", imageSendOffset);
#endif
    }

    if (imageSendOffset >= size) {
        m.tag = ImageMessageTag_imageEnd;
        m.imageEnd = NULL;
        MESSAGE_SERIALIZE(ImageMessage, m, (*b));
        packet = BytesToPacket(b->buffer, b->used, true);
        sendPacketToReaders(host, packet, &gServerConfig->readers);
        needImageSend = false;
    }

    unmapFile(fd, ptr, size);
}

bool
parseImageConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_image;
    pImageConfiguration image = &configuration->server.data.image;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        // const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Image", key, value);
            return false;
        }
        (void)image;
    }
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
    (void)peer;
    (void)config;
    needImageSend = true;
    imageSendOffset = 0;
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
            result = true;
            writeServerFileBytes(serverConfig, NULL, true);
            break;
        case ImageMessageTag_imageChunk:
            result = true;
            writeServerFileBytes(serverConfig, &message->imageChunk, false);
            break;
        case ImageMessageTag_imageEnd:
            result = true;
            needImageSend = true;
            imageSendOffset = 0U;
            break;
        default:
            break;
    }

    if (!result) {
#if _DEBUG
        printf("Unexpected message '%s' from client\n",
               ImageMessageTagToCharString(message->tag));
#endif
    }
    return result;
}