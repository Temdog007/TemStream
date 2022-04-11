#include <include/main.h>

DEFINE_RUN_SERVER(Text);

TextConfiguration
defaultTextConfiguration()
{
    return (TextConfiguration){ .maxLength = 4096 };
}

int
printTextConfiguration(const TextConfiguration* configuration)
{
    return printf("Text\nMax length: %u\n", configuration->maxLength);
}

void
onTextDownTime(pServerData data)
{
    (void)data;
}

bool
parseTextConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_text;
    configuration->server.data.text = defaultTextConfiguration();
    pTextConfiguration text = &configuration->server.data.text;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-L", keyLen, { goto parseLength; });
        STR_EQUALS(key, "--max-length", keyLen, { goto parseLength; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Text", key, value);
            return false;
        }
        continue;

    parseLength : {
        const int i = atoi(value);
        text->maxLength = SDL_max(i, 32);
        continue;
    }
    }
    return true;
}

bool
onConnectForText(ENetPeer* peer, pServerData serverData)
{
    if (getServerFileBytes(serverData->config, &serverData->bytes)) {
        sendBytes(
          peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
    }
    return true;
}

bool
handleTextMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    const TextConfiguration* config = &serverData->config->data.text;
    bool result = false;
    CAST_MESSAGE(TextMessage, ptr);
    switch (message->tag) {
        case TextMessageTag_general: {
            TextMessage textMessage = { 0 };
            textMessage.tag = TextMessageTag_general;
            result = handleGeneralMessage(
              &message->general, serverData, &textMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(TextMessage, textMessage, serverData->bytes);
                sendBytes(peer,
                          1,
                          SERVER_CHANNEL,
                          &serverData->bytes,
                          SendFlags_Normal);
            }
            TextMessageFree(&textMessage);
        } break;
        case TextMessageTag_text: {
            result = true;
            if (TemLangStringIsEmpty(&message->text)) {
                break;
            }
            if (config->maxLength > 0 &&
                config->maxLength < message->text.used) {
                printf("Got a text message with the length of %u. But %u is "
                       "the max\n",
                       message->text.used,
                       config->maxLength);
                break;
            }
            printf("Text server updated with '%s'\n", message->text.buffer);
            MESSAGE_SERIALIZE(TextMessage, (*message), serverData->bytes);
            writeServerFileBytes(serverData->config, &serverData->bytes, true);
            ENetPacket* packet = BytesToPacket(serverData->bytes.buffer,
                                               serverData->bytes.used,
                                               SendFlags_Normal);
            sendPacketToReaders(
              serverData->host, packet, &serverData->config->readers);
        } break;
        default:
#if _DEBUG
            printf("Unexpected text message '%s' from client\n",
                   TextMessageTagToCharString(message->tag));
#endif
            break;
    }
    return result;
}