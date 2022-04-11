#include <include/main.h>

DEFINE_RUN_SERVER(Video);

VideoConfiguration
defaultVideoConfiguration()
{
    return (VideoConfiguration){ .none = NULL };
}

int
printVideoConfiguration(const VideoConfiguration* configuration)
{
    (void)configuration;
    return puts("Video");
}

void
onVideoDownTime(pServerData data)
{
    (void)data;
}

bool
parseVideoConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_video;
    configuration->server.data.video = defaultVideoConfiguration();
    pVideoConfiguration video = &configuration->server.data.video;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        // const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Video", key, value);
            return false;
        }
        (void)video;
    }
    return true;
}

VideoMessage sizeMessage = { .tag = VideoMessageTag_size, .size = { 0, 0 } };

bool
onConnectForVideo(ENetPeer* peer, pServerData serverData)
{
    MESSAGE_SERIALIZE(VideoMessage, sizeMessage, serverData->bytes);
    sendBytes(peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
    return true;
}

bool
handleVideoMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    // const VideoConfiguration* config = &serverConfig->data.video;
    bool result = false;
    CAST_MESSAGE(VideoMessage, ptr);
    switch (message->tag) {
        case VideoMessageTag_general: {
            VideoMessage videoMessage = { 0 };
            videoMessage.tag = TextMessageTag_general;
            result = handleGeneralMessage(
              &message->general, serverData, &videoMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(
                  VideoMessage, videoMessage, serverData->bytes);
                sendBytes(peer,
                          1,
                          SERVER_CHANNEL,
                          &serverData->bytes,
                          SendFlags_Normal);
            }
            VideoMessageFree(&videoMessage);
        } break;
        case VideoMessageTag_size: {
            MESSAGE_SERIALIZE(VideoMessage, (*message), serverData->bytes);
            ENetPacket* packet = BytesToPacket(serverData->bytes.buffer,
                                               serverData->bytes.used,
                                               SendFlags_Normal);
            sendPacketToReaders(
              serverData->host, packet, &serverData->config->readers);
            result = VideoMessageCopy(&sizeMessage, message, currentAllocator);
        } break;
        case VideoMessageTag_video: {
            result = true;
            MESSAGE_SERIALIZE(VideoMessage, (*message), serverData->bytes);
            ENetPacket* packet = BytesToPacket(serverData->bytes.buffer,
                                               serverData->bytes.used,
                                               SendFlags_Video);
            sendPacketToReaders(
              serverData->host, packet, &serverData->config->readers);
        } break;
        default:
#if _DEBUG
            printf("Unexpected video message '%s' from client\n",
                   VideoMessageTagToCharString(message->tag));
#endif
            break;
    }
    return result;
}