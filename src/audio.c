#include <include/main.h>

DEFINE_RUN_SERVER(Audio);

AudioConfiguration
defaultAudioConfiguration()
{
    return (AudioConfiguration){ .none = NULL };
}

int
printAudioConfiguration(const AudioConfiguration* configuration)
{
    (void)configuration;
    return puts("Audio");
}

void
onAudioDownTime(pServerData data)
{
    (void)data;
}

bool
parseAudioConfiguration(const int argc,
                        const char** argv,
                        pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_audio;
    pAudioConfiguration audio = &configuration->server.data.audio;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        // const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Audio", key, value);
            return false;
        }
        (void)audio;
    }
    return true;
}

bool
onConnectForAudio(ENetPeer* peer, pServerData serverData)
{
    (void)serverData;
    (void)peer;
    return true;
}

bool
handleAudioMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    // const AudioConfiguration* config = &serverConfig->data.audio;
    bool result = false;
    CAST_MESSAGE(AudioMessage, ptr);
    switch (message->tag) {
        case AudioMessageTag_general: {
            AudioMessage audioMessage = { 0 };
            audioMessage.tag = TextMessageTag_general;
            result = handleGeneralMessage(
              &message->general, peer, &audioMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(
                  AudioMessage, audioMessage, serverData->bytes);
                sendBytes(peer, 1, SERVER_CHANNEL, &serverData->bytes, true);
            }
            AudioMessageFree(&audioMessage);
        } break;
        case AudioMessageTag_audio: {
            result = true;
            MESSAGE_SERIALIZE(AudioMessage, (*message), serverData->bytes);
            ENetPacket* packet = BytesToPacket(
              serverData->bytes.buffer, serverData->bytes.used, true);
            sendPacketToReaders(
              peer->host, packet, &serverData->config->readers);
        } break;
        default:
            break;
    }
    if (!result) {
#if _DEBUG
        printf("Unexpected message '%s' from client\n",
               AudioMessageTagToCharString(message->tag));
#endif
    }
    return result;
}