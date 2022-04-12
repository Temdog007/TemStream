#include <include/main.h>

DEFINE_RUN_SERVER(Chat);

ChatConfiguration
defaultChatConfiguration()
{
    return (ChatConfiguration){ .interval = 3, .maxLength = 128 };
}

int
printChatConfiguration(const ChatConfiguration* configuration)
{
    return printf("Chat\nMessage interval: %u second(s)\nMax length: %u\n",
                  configuration->interval,
                  configuration->maxLength);
}

void
onChatDownTime(pServerData serverData)
{
    (void)serverData;
}

bool
parseChatConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_chat;
    configuration->server.data.chat = defaultChatConfiguration();
    pChatConfiguration chat = &configuration->server.data.chat;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-I", keyLen, { goto parseInterval; });
        STR_EQUALS(key, "--interval", keyLen, { goto parseInterval; });
        STR_EQUALS(key, "-L", keyLen, { goto parseLength; });
        STR_EQUALS(key, "--length", keyLen, { goto parseLength; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Chat", key, value);
            return false;
        }
        continue;

    parseInterval : {
        const int i = atoi(value);
        chat->interval = SDL_max(i, 32);
        continue;
    }
    parseLength : {
        const int i = (int)atoi(value);
        chat->maxLength = SDL_max(i, KB(1));
        continue;
    }
    }
    return true;
}

bool
onConnectForChat(ENetPeer* peer, pServerData serverData)
{
    char buffer[KB(4)];
    FILE* file = fopen(getServerFileName(serverData->config, buffer), "rb");
    if (file != NULL) {
        ChatMessage m = { 0 };
        m.tag = ChatMessageTag_logs;
        m.logs.allocator = currentAllocator;
        while (fgets(buffer, sizeof(buffer), file)) {
            const size_t len = strlen(buffer);
            if (len == 0) {
                continue;
            }
            // Remove newline character
            if (buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            if (!b64_decode(buffer, &serverData->bytes)) {
                fprintf(
                  stderr,
                  "Failed to decode base64 string (size %zu) in chat log\n",
                  len);
                continue;
            }
            Chat c = { 0 };
            MESSAGE_DESERIALIZE(Chat, c, serverData->bytes);
            ChatListAppend(&m.logs, &c);
            ChatFree(&c);
        }
#if _DEBUG
        printf("Sending %u chat messages from log\n", m.logs.used);
#endif
        MESSAGE_SERIALIZE(ChatMessage, m, serverData->bytes);
        sendBytes(
          peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
        ChatMessageFree(&m);
        enet_host_flush(serverData->host);

        fclose(file);
    }

    ChatMessage m = { 0 };
    m.tag = ChatMessageTag_configuration;
    m.configuration = serverData->config->data.chat;
    MESSAGE_SERIALIZE(ChatMessage, m, serverData->bytes);
    sendBytes(peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
    // Don't free. Configuration wasn't allocated

    return true;
}

void
sendReject(ENetPeer* peer, pBytes bytes)
{
    ChatMessage m = { .tag = ChatMessageTag_reject, .reject = NULL };
    MESSAGE_SERIALIZE(ChatMessage, m, (*bytes));
    sendBytes(peer, 1, SERVER_CHANNEL, bytes, SendFlags_Normal);
}

bool
handleChatMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    ChatMessage chatMessage = { 0 };
    pClient client = peer->data;
    const ChatConfiguration* config = &serverData->config->data.chat;
    bool result = false;
    CAST_MESSAGE(ChatMessage, ptr);
    switch (message->tag) {
        case ChatMessageTag_general:
            chatMessage.tag = ChatMessageTag_general;
            result = handleGeneralMessage(
              &message->general, peer, serverData, &chatMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(ChatMessage, chatMessage, serverData->bytes);
                sendBytes(peer,
                          1,
                          SERVER_CHANNEL,
                          &serverData->bytes,
                          SendFlags_Normal);
            }
            break;
        case ChatMessageTag_message:
            result = true;
            if (config->maxLength > 0 &&
                config->maxLength < message->message.used) {
                sendReject(peer, &serverData->bytes);
                printf("Got a chat message with the length of %u. But %u is "
                       "the max\n",
                       message->message.used,
                       config->maxLength);
                break;
            }
            const uint64_t now = SDL_GetTicks64();
            if (now - client->lastMessage < config->interval * 1000u) {
                sendReject(peer, &serverData->bytes);
                printf("Client '%s' sent a chat message sooner than the "
                       "interval: %u second(s)\n",
                       client->name.buffer,
                       config->interval);
                break;
            }
            client->lastMessage = now;

            chatMessage.tag = ChatMessageTag_newChat;
            chatMessage.newChat.timestamp = (int64_t)time(NULL);
            TemLangStringCopy(
              &chatMessage.newChat.author, &client->name, currentAllocator);
            TemLangStringCopy(&chatMessage.newChat.message,
                              &message->message,
                              currentAllocator);

            MESSAGE_SERIALIZE(ChatMessage, chatMessage, serverData->bytes);
            ENetPacket* packet = BytesToPacket(serverData->bytes.buffer,
                                               serverData->bytes.used,
                                               SendFlags_Normal);
            sendPacketToReaders(serverData->host, packet);

            MESSAGE_SERIALIZE(Chat, chatMessage.newChat, serverData->bytes);
            TemLangString str = b64_encode(&serverData->bytes);
            TemLangStringAppendChar(&str, '\n');
            Bytes temp = { .buffer = (uint8_t*)str.buffer,
                           .used = str.used,
                           .size = str.size };
            writeServerFileBytes(serverData->config, &temp, false);
            TemLangStringFree(&str);

            enet_host_flush(serverData->host);
            break;
        default:
#if _DEBUG
            printf("Unexpected chat message '%s' from client\n",
                   ChatMessageTagToCharString(message->tag));
#endif
            break;
    }
    ChatMessageFree(&chatMessage);
    return result;
}
