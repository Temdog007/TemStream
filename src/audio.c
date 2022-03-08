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
onAudioDownTime(ENetHost* host, pBytes b)
{
    (void)host;
    (void)b;
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
onConnectForAudio(pClient client,
                  pBytes bytes,
                  ENetPeer* peer,
                  const ServerConfiguration* config)
{
    (void)client;
    (void)bytes;
    (void)peer;
    (void)config;
    return true;
}

bool
handleAudioMessage(const void* ptr,
                   pBytes bytes,
                   ENetPeer* peer,
                   redisContext* ctx,
                   const ServerConfiguration* serverConfig)
{
    (void)ctx;

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
                MESSAGE_SERIALIZE(AudioMessage, audioMessage, (*bytes));
                sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
            }
            AudioMessageFree(&audioMessage);
        } break;
        case AudioMessageTag_audio: {
            result = true;
            MESSAGE_SERIALIZE(AudioMessage, (*message), (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, true);
            sendPacketToReaders(peer->host, packet, &serverConfig->readers);
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