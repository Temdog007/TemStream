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

bool
parseTextConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->tag = ServerConfigurationDataTag_text;
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

void
serializeTextMessage(const void* ptr, pBytes bytes)
{
    CAST_MESSAGE(TextMessage, ptr);
    MESSAGE_SERIALIZE(TextMessage, (*message), (*bytes));
}

void*
deserializeTextMessage(const Bytes* bytes)
{
    TextMessage* message = currentAllocator->allocate(sizeof(TextMessage));
    MESSAGE_DESERIALIZE(TextMessage, (*message), (*bytes));
    return message;
}

void
freeTextMessage(void* ptr)
{
    CAST_MESSAGE(TextMessage, ptr);
    TextMessageFree(message);
    currentAllocator->free(message);
}

const GeneralMessage*
getGeneralMessageFromText(const void* ptr)
{
    CAST_MESSAGE(TextMessage, ptr);
    return message->tag == LobbyMessageTag_general ? &message->general : NULL;
}

void
sendGeneralMessageForText(const GeneralMessage* m, pBytes bytes, ENetPeer* peer)
{
    TextMessage lm = { 0 };
    lm.tag = TextMessageTag_general;
    lm.general = *m;
    MESSAGE_SERIALIZE(TextMessage, lm, (*bytes));
    sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
}

bool
onConnectForText(pClient client,
                 pBytes bytes,
                 ENetPeer* peer,
                 const ServerConfiguration* config)
{
    (void)client;
    if (getServerFileBytes(config, bytes)) {
        sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
    }
    return true;
}

bool
handleTextMessage(const void* ptr,
                  pBytes bytes,
                  ENetPeer* peer,
                  redisContext* ctx,
                  const ServerConfiguration* serverConfig)
{
    (void)ctx;

    pClient client = peer->data;
    const TextConfiguration* config = &serverConfig->data.text;
    bool result = false;
    CAST_MESSAGE(TextMessage, ptr);
    switch (message->tag) {
        case TextMessageTag_general: {
            TextMessage textMessage = { 0 };
            textMessage.tag = TextMessageTag_general;
            result = handleGeneralMessage(
              &message->general, peer, &textMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(TextMessage, textMessage, (*bytes));
                sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
            }
            TextMessageFree(&textMessage);
        } break;
        case TextMessageTag_text:
            result = true;
            if (!clientHasWriteAccess(client, serverConfig)) {
                printf("Client '%s' sent a text message when it doesn't have "
                       "write access",
                       client->name.buffer);
                break;
            }
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
            MESSAGE_SERIALIZE(TextMessage, (*message), (*bytes));
            writeServerFileBytes(serverConfig, bytes, true);
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, true);
            sendPacketToReaders(peer->host, packet, &serverConfig->readers);
            break;
        default:
            break;
    }
    if (!result) {
#if _DEBUG
        printf("Unexpected message '%s' from client\n",
               TextMessageTagToCharString(message->tag));
#endif
    }
    return result;
}