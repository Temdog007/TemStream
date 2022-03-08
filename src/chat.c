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
onChatDownTime(ENetHost* host, pBytes b)
{
    (void)host;
    (void)b;
}

bool
parseChatConfiguration(const int argc,
                       const char** argv,
                       pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_chat;
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
onConnectForChat(pClient client,
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
handleChatMessage(const void* ptr,
                  pBytes bytes,
                  ENetPeer* peer,
                  redisContext* ctx,
                  const ServerConfiguration* serverConfig)
{
    (void)ctx;
    ChatMessage chatMessage = { 0 };
    pClient client = peer->data;
    const ChatConfiguration* config = &serverConfig->data.chat;
    bool result = false;
    CAST_MESSAGE(ChatMessage, ptr);
    switch (message->tag) {
        case ChatMessageTag_general:
            chatMessage.tag = ChatMessageTag_general;
            result = handleGeneralMessage(
              &message->general, peer, &chatMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(ChatMessage, chatMessage, (*bytes));
                sendBytes(peer, 1, SERVER_CHANNEL, bytes, true);
            }
            break;
        case ChatMessageTag_message:
            result = true;
            if (config->maxLength > 0 &&
                config->maxLength < message->message.used) {
                printf("Got a chat message with the length of %u. But %u is "
                       "the max\n",
                       message->message.used,
                       config->maxLength);
                break;
            }
            const uint64_t now = SDL_GetTicks64();
            if (now - client->lastMessage < config->interval) {
                printf(
                  "Client '%s' sent a chat message sooner than the interval\n",
                  client->name.buffer);
                break;
            }
            client->lastMessage = now;

            Chat newChat = { 0 };
            newChat.timestamp = (int64_t)time(NULL);
            TemLangStringCopy(&newChat.author, &client->name, currentAllocator);
            TemLangStringCopy(
              &newChat.message, &message->message, currentAllocator);

            chatMessage.tag = ChatMessageTag_newChat;
            ChatCopy(&chatMessage.newChat, &newChat, currentAllocator);

            MESSAGE_SERIALIZE(ChatMessage, chatMessage, (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, true);
            sendPacketToReaders(peer->host, packet, &serverConfig->readers);

            if (getServerFileBytes(serverConfig, bytes)) {
                MESSAGE_DESERIALIZE(ChatMessage, chatMessage, (*bytes));
            } else {
                ChatMessageFree(&chatMessage);
                chatMessage.tag = ChatMessageTag_logs;
                chatMessage.logs.allocator = currentAllocator;
            }
            ChatListAppend(&chatMessage.logs, &newChat);
            MESSAGE_SERIALIZE(ChatMessage, chatMessage, (*bytes));
            writeServerFileBytes(serverConfig, bytes, true);
            ChatFree(&newChat);
            break;
        default:
            break;
    }
    if (!result) {
#if _DEBUG
        printf("Unexpected message '%s' from client\n",
               ChatMessageTagToCharString(message->tag));
#endif
    }
    ChatMessageFree(&chatMessage);
    return result;
}
