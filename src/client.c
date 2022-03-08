#include <include/main.h>

#define MIN_WIDTH 32
#define MIN_HEIGHT 32

#define DEFAULT_CHAT_COUNT 5

SDL_mutex* clientMutex = NULL;
ClientData clientData = { 0 };

#define USE_DISPLAY(mutex, endLabel, displayMissing, f)                        \
    IN_MUTEX(mutex, endLabel, {                                                \
        pStreamDisplay display = NULL;                                         \
        const ServerConfiguration* config = NULL;                              \
        {                                                                      \
            size_t index = 0;                                                  \
            if (!GetStreamDisplayFromGuid(                                     \
                  &clientData.displays, id, NULL, &index)) {                   \
                displayMissing = true;                                         \
                goto endLabel;                                                 \
            }                                                                  \
            display = &clientData.displays.buffer[index];                      \
        }                                                                      \
        config = &display->config;                                             \
        displayMissing = false;                                                \
        f                                                                      \
    });

#define TRY_USE_DISPLAY(mutex, endLabel, displayMissing, f)                    \
    TRY_IN_MUTEX(mutex, endLabel, {                                            \
        pStreamDisplay display = NULL;                                         \
        const ServerConfiguration* config = NULL;                              \
        {                                                                      \
            size_t index = 0;                                                  \
            if (!GetStreamDisplayFromGuid(                                     \
                  &clientData.displays, id, NULL, &index)) {                   \
                displayMissing = true;                                         \
                goto endLabel;                                                 \
            }                                                                  \
            display = &clientData.displays.buffer[index];                      \
        }                                                                      \
        config = &display->config;                                             \
        displayMissing = false;                                                \
        f                                                                      \
    });

void
sendAudioPackets(OpusEncoder* encoder,
                 pBytes audio,
                 const SDL_AudioSpec spec,
                 ENetPeer*);

bool
selectAudioStreamSource(struct pollfd inputfd,
                        pBytes bytes,
                        pStreamDisplay,
                        pAudioState state);

void
consumeAudio(const Bytes*, const Guid*);

bool
startRecording(struct pollfd inputfd, pBytes bytes, pAudioState);

bool
startPlayback(struct pollfd inputfd, pBytes bytes, pAudioState state);

void
storeOpusAudio(pAudioState state, const Bytes* compressed);

void
handleUserInput(ENetPeer* peer,
                pBytes,
                const ServerConfigurationDataTag,
                const UserInput*);

bool
clientHandleLobbyMessage(const LobbyMessage* message,
                         ENetPeer* peer,
                         pClient client);

bool
clientHandleGeneralMessage(const GeneralMessage* message,
                           pClient client,
                           ENetPeer* peer);

void
renderDisplays()
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_Render;
    SDL_PushEvent(&e);
}

void
updateStreamDisplay(const StreamDisplay* display)
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_UpdateStreamDisplay;
    e.user.data1 = (void*)(size_t)display->id.numbers[0];
    e.user.data2 = (void*)(size_t)display->id.numbers[1];
    SDL_PushEvent(&e);
}

void
askQuestion(const char* string)
{
    const size_t size = strlen(string);
    for (size_t i = 0; i < size; ++i) {
        putchar('-');
    }
    printf("\n%s|\n", string);
    for (size_t i = 0; i < size; ++i) {
        putchar('-');
    }
    puts("");
}

ClientConfiguration
defaultClientConfiguration()
{
    return (ClientConfiguration){
        .fullscreen = false,
        .windowWidth = 800,
        .windowHeight = 600,
        .fontSize = 48,
        .noGui = false,
        .noAudio = false,
        .hostname = TemLangStringCreate("localhost", currentAllocator),
        .port = 10000,
        .ttfFile = TemLangStringCreate(
          "/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf", currentAllocator)
    };
}

bool
parseClientConfiguration(const int argc,
                         const char** argv,
                         pConfiguration configuration)
{
    configuration->tag = ConfigurationTag_client;
    configuration->client = defaultClientConfiguration();
    pClientConfiguration client = &configuration->client;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-F", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "--font", keyLen, { goto parseTTF; });
        STR_EQUALS(key, "-S", keyLen, { goto parseSize; });
        STR_EQUALS(key, "--font-size", keyLen, { goto parseSize; });
        STR_EQUALS(key, "-W", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "--width", keyLen, { goto parseWidth; });
        STR_EQUALS(key, "-H", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "--height", keyLen, { goto parseHeight; });
        STR_EQUALS(key, "-F", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "--fullscreen", keyLen, { goto parseFullscreen; });
        STR_EQUALS(key, "-T", keyLen, { goto parseToken; });
        STR_EQUALS(key, "--token", keyLen, { goto parseToken; });
        STR_EQUALS(key, "-C", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "--credentials", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "-NG", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "--no-gui", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "-NA", keyLen, { goto parseNoAudio; });
        STR_EQUALS(key, "--no-audio", keyLen, { goto parseNoAudio; });
        STR_EQUALS(key, "-H", keyLen, { goto parseHostname; });
        STR_EQUALS(key, "--host-name", keyLen, { goto parseHostname; });
        STR_EQUALS(key, "-P", keyLen, { goto parsePort; });
        STR_EQUALS(key, "--port", keyLen, { goto parsePort; });
        // TODO: parse authentication
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Client", key, value);
            return false;
        }
        continue;
    parseTTF : {
        client->ttfFile = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseSize : {
        client->fontSize = atoi(value);
        continue;
    }
    parseWidth : {
        client->windowWidth = atoi(value);
        continue;
    }
    parseHeight : {
        client->windowHeight = atoi(value);
        continue;
    }
    parseFullscreen : {
        client->fullscreen = atoi(value);
        continue;
    }
    parseToken : {
        client->authentication.tag = ClientAuthenticationTag_token;
        client->authentication.token =
          TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseCredentials : {
        client->authentication.tag = ClientAuthenticationTag_credentials;
        if (!parseCredentials(value, &client->authentication.credentials)) {
            return false;
        }
        continue;
    }
    parseNoGui : {
        client->noGui = atoi(value);
        continue;
    }
    parseNoAudio : {
        client->noAudio = atoi(value);
        continue;
    }
    parseHostname : {
        client->hostname = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parsePort : {
        const int i = atoi(value);
        client->port = SDL_clamp(i, 1000, 60000);
        continue;
    }
    }
    return true;
}

int
printClientConfiguration(const ClientConfiguration* configuration)
{
    return printf("Width: %d; Height: %d; Fullscreen: %d; TTF file: %s\n",
                  configuration->windowWidth,
                  configuration->windowHeight,
                  configuration->fullscreen,
                  configuration->ttfFile.buffer);
}

void
camelCaseToNormal(pTemLangString str)
{
    for (size_t i = 1; i < str->used; ++i) {
        const char c = str->buffer[i];
        if (isupper(c) && islower(str->buffer[i - 1])) {
            TemLangStringInsertChar(str, ' ', i);
        }
    }
}

typedef enum UserInputResult
{
    UserInputResult_NoInput,
    UserInputResult_Error,
    UserInputResult_Input
} UserInputResult,
  *pUserInputResult;

UserInputResult
getUserInput(struct pollfd inputfd,
             pBytes bytes,
             ssize_t* output,
             const int timeout)
{
    switch (poll(&inputfd, 1, timeout)) {
        case -1:
            perror("poll");
            return UserInputResult_Error;
        case 0:
            return UserInputResult_NoInput;
        default:
            break;
    }

    if ((inputfd.revents & POLLIN) == 0) {
        return UserInputResult_NoInput;
    }

    const size_t size = read(STDIN_FILENO, bytes->buffer, bytes->size) - 1;
    if (size <= 0) {
        perror("read");
        return UserInputResult_Error;
    }
    // Remove new line character
    bytes->buffer[size] = '\0';
    if (output != NULL) {
        *output = size;
    }
    return UserInputResult_Input;
}

UserInputResult
getIndexFromUser(struct pollfd inputfd,
                 pBytes bytes,
                 const uint32_t max,
                 uint32_t* index,
                 const bool keepPolling)
{
    if (max < 2) {
        *index = 0;
        return UserInputResult_Input;
    }
    ssize_t size = 0;
    while (!appDone) {
        switch (getUserInput(inputfd, bytes, &size, 0)) {
            case UserInputResult_Error:
                return UserInputResult_Error;
            case UserInputResult_NoInput:
                if (keepPolling) {
                    continue;
                } else {
                    return UserInputResult_NoInput;
                }
            default:
                break;
        }
        char* end = NULL;
        *index = (uint32_t)strtoul((const char*)bytes->buffer, &end, 10) - 1UL;
        if (end != (char*)&bytes->buffer[size] || *index >= max) {
            printf("Enter a number between 1 and %u\n", max);
            return UserInputResult_Error;
        }
        return UserInputResult_Input;
    }
    return UserInputResult_NoInput;
}

void
sendAuthentication(ENetPeer* peer, const ServerConfigurationDataTag type)
{
    uint8_t buffer[KB(2)] = { 0 };
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = buffer,
                    .used = 0,
                    .size = sizeof(buffer) };
    switch (type) {
        case ServerConfigurationDataTag_lobby: {
            LobbyMessage message = { 0 };
            message.tag = LobbyMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(LobbyMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_chat: {
            ChatMessage message = { 0 };
            message.tag = ChatMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(ChatMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_text: {
            TextMessage message = { 0 };
            message.tag = TextMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(TextMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_audio: {
            AudioMessage message = { 0 };
            message.tag = AudioMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(AudioMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_image: {
            ImageMessage message = { 0 };
            message.tag = ImageMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate =
              *(pClientAuthentication)clientData.authentication;
            MESSAGE_SERIALIZE(ImageMessage, message, bytes);
        } break;
        default:
            break;
    }

    sendBytes(peer, 1, CLIENT_CHANNEL, &bytes, true);
}

bool
clientHandleTextMessage(const Bytes* bytes, pStreamDisplay display)
{
    TextMessage message = { 0 };
    MESSAGE_DESERIALIZE(TextMessage, message, (*bytes));
    bool success = false;
    switch (message.tag) {
        case TextMessageTag_text: {
            StreamDisplayDataFree(&display->data);
            display->data.tag = StreamDisplayDataTag_text;
            success = TemLangStringCopy(
              &display->data.text, &message.text, currentAllocator);
            printf("Got text message: '%s'\n", message.text.buffer);
            updateStreamDisplay(display);
        } break;
        case TextMessageTag_general:
            success = clientHandleGeneralMessage(&message.general, NULL, NULL);
            break;
        default:
            printf("Unexpected text message: %s\n",
                   TextMessageTagToCharString(message.tag));
            break;
    }
    TextMessageFree(&message);
    return success;
}

bool
clientHandleChatMessage(const Bytes* bytes, pStreamDisplay display)
{
    ChatMessage message = { 0 };
    MESSAGE_DESERIALIZE(ChatMessage, message, (*bytes));
    bool success = false;
    switch (message.tag) {
        case ChatMessageTag_logs: {
            StreamDisplayDataFree(&display->data);
            display->data.tag = StreamDisplayDataTag_chat;
            pStreamDisplayChat chat = &display->data.chat;
            chat->count = DEFAULT_CHAT_COUNT;
            success =
              ChatListCopy(&chat->logs, &message.logs, currentAllocator);
            chat->offset = chat->logs.used;
            updateStreamDisplay(display);
        } break;
        case ChatMessageTag_newChat: {
            pStreamDisplayChat chat = &display->data.chat;
            if (display->data.tag != StreamDisplayDataTag_chat) {
                StreamDisplayDataFree(&display->data);
                display->data.tag = StreamDisplayDataTag_chat;
                display->data.chat.logs.allocator = currentAllocator;
                chat->count = DEFAULT_CHAT_COUNT;
                chat->offset = chat->logs.used;
            }
            success = ChatListAppend(&chat->logs, &message.newChat);
            if (chat->offset >= chat->logs.used - chat->count) {
                chat->offset = chat->logs.used;
            }
            updateStreamDisplay(display);
        } break;
        case ChatMessageTag_general:
            success = clientHandleGeneralMessage(&message.general, NULL, NULL);
            break;
        default:
            printf("Unexpected chat message: %s\n",
                   ChatMessageTagToCharString(message.tag));
            break;
    }
    ChatMessageFree(&message);
    return success;
}

bool
clientHandleImageMessage(const Bytes* bytes, pStreamDisplay display)
{
    ImageMessage message = { 0 };
    MESSAGE_DESERIALIZE(ImageMessage, message, (*bytes));
    bool success = false;
    switch (message.tag) {
        case ImageMessageTag_imageStart:
            success = true;
            StreamDisplayDataFree(&display->data);
            display->data.tag = StreamDisplayDataTag_image;
            display->data.image.allocator = currentAllocator;
            break;
        case ImageMessageTag_imageChunk:
            success = true;
            if (display->data.tag != StreamDisplayDataTag_image) {
                StreamDisplayDataFree(&display->data);
                display->data.tag = StreamDisplayDataTag_image;
                display->data.image.allocator = currentAllocator;
            }
            uint8_tListQuickAppend(&display->data.image,
                                   message.imageChunk.buffer,
                                   message.imageChunk.used);
            break;
        case ImageMessageTag_imageEnd:
            success = true;
            updateStreamDisplay(display);
            break;
        case ImageMessageTag_general:
            success = clientHandleGeneralMessage(&message.general, NULL, NULL);
            break;
        default:
            printf("Unexpected image message: %s\n",
                   ImageMessageTagToCharString(message.tag));
            break;
    }
    ImageMessageFree(&message);
    return success;
}

bool
clientHandleAudioMessage(const Bytes* packetBytes,
                         pBytes bytes,
                         pStreamDisplay display,
                         pAudioState record,
                         pAudioState playback)
{
    AudioMessage message = { 0 };
    MESSAGE_DESERIALIZE(AudioMessage, message, (*packetBytes));
    bool success = false;
    switch (message.tag) {
        case AudioMessageTag_general: {
            Client client = { 0 };
            success =
              clientHandleGeneralMessage(&message.general, &client, NULL);
            if (!success ||
                message.general.tag != GeneralMessageTag_authenticateAck) {
                break;
            }
            const struct pollfd inputfd = { .events = POLLIN,
                                            .revents = 0,
                                            .fd = STDIN_FILENO };
            if (clientHasWriteAccess(&client, &display->config)) {
                success =
                  selectAudioStreamSource(inputfd, bytes, display, record);
                if (record->encoder != NULL) {
                    SDL_PauseAudioDevice(record->deviceId, SDL_FALSE);
                }
            }
            if (success && clientHasReadAccess(&client, &display->config)) {
                success = startPlayback(inputfd, bytes, playback);
                if (playback->decoder != NULL) {
                    SDL_PauseAudioDevice(playback->deviceId, SDL_FALSE);
                }
            }
            ClientFree(&client);
        } break;
        case AudioMessageTag_audio:
            if (playback->decoder == NULL) {
                success = false;
                break;
            }
            success = true;
            storeOpusAudio(playback, &message.audio);
            break;
        default:
            printf("Unexpected audio message: %s\n",
                   AudioMessageTagToCharString(message.tag));
            break;
    }
    AudioMessageFree(&message);
    return success;
}

int
streamConnectionThread(void* ptr)
{
    const Guid* id = (const Guid*)ptr;

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    pENetPacketList packetList = { .allocator = currentAllocator };
    Bytes bytes = { .allocator = currentAllocator,
                    .used = 0,
                    .size = MAX_PACKET_SIZE,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE) };
    Bytes audioBytes = { .allocator = currentAllocator };
    int result = EXIT_FAILURE;
    bool displayMissing = false;
    AudioState record = { 0 };
    AudioState playback = { 0 };
    USE_DISPLAY(clientData.mutex, fend, displayMissing, {
        if (config->port.tag != PortTag_port) {
            printf(
              "Cannot opening connection to stream due to an invalid port\n");
            goto fend;
        }

        host = enet_host_create(NULL, 1, 2, 0, 0);
        if (host == NULL) {
            fprintf(stderr, "Failed to create client host\n");
            goto fend;
        }
        {
            ENetAddress address = { 0 };
            enet_address_set_host(&address, config->hostname.buffer);
            address.port = config->port.port;
            peer = enet_host_connect(host, &address, 2, config->data.tag);
            char buffer[512] = { 0 };
            enet_address_get_host_ip(&address, buffer, sizeof(buffer));
            printf("Connecting to server: %s:%u...\n", buffer, address.port);
        }
        if (peer == NULL) {
            fprintf(stderr, "Failed to connect to server\n");
            goto fend;
        }
    });
    if (host == NULL || peer == NULL) {
        goto end;
    }

    ENetEvent event = { 0 };
    if (!appDone && enet_host_service(host, &event, 5000U) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        char buffer[KB(1)] = { 0 };
        enet_address_get_host_ip(&event.peer->address, buffer, sizeof(buffer));
        printf(
          "Connected to server: %s:%u\n", buffer, event.peer->address.port);
        USE_DISPLAY(clientData.mutex, fend2, displayMissing, {
            sendAuthentication(peer, config->data.tag);
        });
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        goto end;
    }

    while (!appDone && !displayMissing) {
        if (enet_host_service(host, &event, 100U) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    USE_DISPLAY(clientData.mutex, fend3, displayMissing, {
                        printf(
                          "Unexpected connect message from '%s(%s)' stream\n",
                          config->name.buffer,
                          ServerConfigurationDataTagToCharString(
                            config->data.tag));
                    });
                    goto end;
                case ENET_EVENT_TYPE_DISCONNECT:
                    USE_DISPLAY(clientData.mutex, fend4, displayMissing, {
                        printf("Disconnected from '%s(%s)' stream\n",
                               config->name.buffer,
                               ServerConfigurationDataTagToCharString(
                                 config->data.tag));
                    });
                    goto end;
                case ENET_EVENT_TYPE_RECEIVE:
                    pENetPacketListAppend(&packetList, &event.packet);
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
                default:
                    break;
            }
        }
        if (record.encoder != NULL) {
            bytes.used =
              SDL_DequeueAudio(record.deviceId, bytes.buffer, bytes.size);
            uint8_tListQuickAppend(&audioBytes, bytes.buffer, bytes.used);
            if (audioBytes.used != 0) {
                sendAudioPackets(
                  record.encoder, &audioBytes, record.spec, peer);
            }
        }
        TRY_USE_DISPLAY(clientData.mutex, fend563, displayMissing, {
            for (size_t i = 0; i < packetList.used; ++i) {
                pENetPacket packet = packetList.buffer[i];
                Bytes packetBytes = { 0 };
                packetBytes.allocator = currentAllocator;
                packetBytes.buffer = packet->data;
                packetBytes.size = packet->dataLength;
                packetBytes.used = packet->dataLength;
                bool success = false;
                switch (config->data.tag) {
                    case ServerConfigurationDataTag_text:
                        success =
                          clientHandleTextMessage(&packetBytes, display);
                        break;
                    case ServerConfigurationDataTag_chat:
                        success =
                          clientHandleChatMessage(&packetBytes, display);
                        break;
                    case ServerConfigurationDataTag_image:
                        success =
                          clientHandleImageMessage(&packetBytes, display);
                        break;
                    case ServerConfigurationDataTag_audio:
                        success = clientHandleAudioMessage(
                          &packetBytes, &bytes, display, &record, &playback);
                        break;
                    default:
                        fprintf(stderr,
                                "Server '%s' not implemented\n",
                                ServerConfigurationDataTagToCharString(
                                  config->data.tag));
                        break;
                }
                if (!success) {
                    printf("Disconnecting from server: ");
                    printServerConfigurationForClient(config);
                    enet_peer_disconnect(event.peer, 0);
                }
                enet_packet_destroy(packet);
            }
            pENetPacketListFree(&packetList);
            packetList.allocator = currentAllocator;
            for (size_t i = 0; i < display->userInputs.used; ++i) {
                UserInput input = { 0 };
                UserInputCopy(
                  &input, &display->userInputs.buffer[i], currentAllocator);
                SDL_UnlockMutex(clientData.mutex);
                handleUserInput(peer, &bytes, display->config.data.tag, &input);
                UserInputFree(&input);
                SDL_LockMutex(clientData.mutex);
            }
            UserInputListFree(&display->userInputs);
            display->userInputs.allocator = currentAllocator;
        });
    }

    result = EXIT_SUCCESS;

end:
    IN_MUTEX(clientData.mutex, fend645, {
        size_t i = 0;
        const StreamDisplay* display = NULL;
        if (GetStreamDisplayFromGuid(&clientData.displays, id, &display, &i)) {
            printf(
              "Disconnecting from server %s(%s)\n",
              display->config.name.buffer,
              ServerConfigurationDataTagToCharString(display->config.data.tag));
            StreamDisplayListSwapRemove(&clientData.displays, i);
        }
    });
    for (size_t i = 0; i < packetList.used; ++i) {
        enet_packet_destroy(packetList.buffer[i]);
    }
    pENetPacketListFree(&packetList);
    AudioStateFree(&record);
    AudioStateFree(&playback);
    uint8_tListFree(&bytes);
    uint8_tListFree(&audioBytes);
    currentAllocator->free(ptr);
    closeHostAndPeer(host, peer);
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_RefreshStreams;
    SDL_PushEvent(&e);
    SDL_AtomicDecRef(&runningThreads);
    return result;
}

void
selectAStreamToConnectTo(struct pollfd inputfd, pBytes bytes, pRandomState rs)
{
    const uint32_t streamNum = clientData.allStreams.used;
    if (streamNum == 0) {
        puts("No streams to connect to");
        return;
    }

    askQuestion("Select a stream to connect to");
    for (uint32_t i = 0; i < streamNum; ++i) {
        const ServerConfiguration* config = &clientData.allStreams.buffer[i];
        printf("%u) ", i + 1U);
        printServerConfigurationForClient(config);
    }
    puts("");

    uint32_t i = 0;
    if (streamNum == 1) {
        puts("Connecting to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, streamNum, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling connecting to stream");
        return;
    }

    const TemLangString str = { .allocator = NULL,
                                .buffer = (char*)bytes->buffer,
                                .size = bytes->size,
                                .used = bytes->used };

    if (GetStreamDisplayFromName(&clientData.displays, &str, NULL, NULL)) {
        puts("Already connected to stream");
        return;
    }

    StreamDisplay display = { 0 };
    display.userInputs.allocator = currentAllocator;
    display.id = randomGuid(rs);
    ServerConfigurationCopy(
      &display.config, &clientData.allStreams.buffer[i], currentAllocator);
    StreamDisplayListAppend(&clientData.displays, &display);
    pGuid id = currentAllocator->allocate(sizeof(Guid));
    *id = display.id;
    StreamDisplayFree(&display);

    SDL_Thread* thread = SDL_CreateThread(
      (SDL_ThreadFunction)streamConnectionThread, "stream", id);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        currentAllocator->free(id);
        StreamDisplayListPop(&clientData.displays);
    } else {
        SDL_AtomicIncRef(&runningThreads);
        SDL_DetachThread(thread);
    }
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd, pBytes bytes)
{
    pStreamDisplayList list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("Not connected to any streams");
        return;
    }

    askQuestion("Select a stream to disconnect from");
    for (uint32_t i = 0; i < list->used; ++i) {
        printf("%u) ", i + 1U);
        printServerConfigurationForClient(&list->buffer[i].config);
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1) {
        puts("Disconnecting from only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceled disconnecting from stream");
        return;
    }

    printf(
      "Disconnecting from %s(%s)...\n",
      list->buffer[i].config.name.buffer,
      ServerConfigurationDataTagToCharString(list->buffer[i].config.data.tag));
    StreamDisplayListSwapRemove(list, i);
}

int
audioLengthToFrames(const int frequency, const int duration)
{
    switch (duration) {
        case OPUS_FRAMESIZE_2_5_MS:
            return frequency / 400;
        case OPUS_FRAMESIZE_5_MS:
            return frequency / 200;
        case OPUS_FRAMESIZE_10_MS:
            return frequency / 100;
        case OPUS_FRAMESIZE_20_MS:
            return frequency / 50;
        case OPUS_FRAMESIZE_40_MS:
            return frequency / 25;
        case OPUS_FRAMESIZE_60_MS:
            return audioLengthToFrames(frequency, OPUS_FRAMESIZE_20_MS) * 3;
        case OPUS_FRAMESIZE_80_MS:
            return audioLengthToFrames(frequency, OPUS_FRAMESIZE_40_MS) * 2;
        case OPUS_FRAMESIZE_100_MS:
            return frequency / 10;
        case OPUS_FRAMESIZE_120_MS:
            return audioLengthToFrames(frequency, OPUS_FRAMESIZE_60_MS) * 2;
        default:
            return 0;
    }
}

int
closestValidFrameCount(const int frequency, const int frames)
{
    const int values[] = { OPUS_FRAMESIZE_120_MS, OPUS_FRAMESIZE_100_MS,
                           OPUS_FRAMESIZE_80_MS,  OPUS_FRAMESIZE_60_MS,
                           OPUS_FRAMESIZE_40_MS,  OPUS_FRAMESIZE_20_MS,
                           OPUS_FRAMESIZE_10_MS,  OPUS_FRAMESIZE_5_MS,
                           OPUS_FRAMESIZE_2_5_MS };
    const int arrayLength = sizeof(values) / sizeof(int);
    if (frames >= audioLengthToFrames(frequency, values[0])) {
        return audioLengthToFrames(frequency, values[0]);
    }
    if (frames <= audioLengthToFrames(frequency, values[arrayLength - 1])) {
        return audioLengthToFrames(frequency, values[arrayLength - 1]);
    }
    for (int i = 1; i < arrayLength; ++i) {
        const int prev = audioLengthToFrames(frequency, values[i - 1]);
        const int current = audioLengthToFrames(frequency, values[i]);
        if (prev >= frames && frames >= current) {
            return audioLengthToFrames(frequency, values[i]);
        }
    }
    return audioLengthToFrames(frequency, values[arrayLength - 1]);
}

void
storeOpusAudio(pAudioState state, const Bytes* compressed)
{
    void* uncompressed = currentAllocator->allocate(MAX_PACKET_SIZE);

    const int result =
#if HIGH_QUALITY_AUDIO
      opus_decode_float(
        state->decoder,
        (unsigned char*)compressed->buffer,
        compressed->used,
        (float*)uncompressed,
        audioLengthToFrames(state->spec.freq, OPUS_FRAMESIZE_120_MS),
        ENABLE_FEC);
#else
      opus_decode(state->decoder,
                  (unsigned char*)compressed->buffer,
                  compressed->used,
                  (opus_int16*)uncompressed,
                  audioLengthToFrames(state->spec.freq, OPUS_FRAMESIZE_120_MS),
                  ENABLE_FEC);
#endif

    if (result < 0) {
        fprintf(stderr, "Failed to decode audio: %s\n", opus_strerror(result));
        goto end;
    }

    const int byteSize = result * state->spec.channels * PCM_SIZE;
    SDL_QueueAudio(state->deviceId, uncompressed, byteSize);

end:
    currentAllocator->free(uncompressed);
}

void
sendAudioPackets(OpusEncoder* encoder,
                 pBytes audio,
                 const SDL_AudioSpec spec,
                 ENetPeer* peer)
{
    uint8_t buffer[KB(2)] = { 0 };

    const int minDuration =
      audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_10_MS);

    AudioMessage message = { 0 };
    message.tag = AudioMessageTag_audio;
    message.audio.allocator = currentAllocator;
    message.audio.buffer = buffer;
    message.audio.size = sizeof(buffer);
    message.audio.used = 0;
    Bytes temp = { .allocator = currentAllocator };
    while (!uint8_tListIsEmpty(audio)) {
        int frame_size = audio->used / (spec.channels * PCM_SIZE);

        if (frame_size < minDuration) {
            break;
        }

        // Only certain durations are valid for encoding
        frame_size = closestValidFrameCount(spec.freq, frame_size);

        const int result =
#if HIGH_QUALITY_AUDIO
          opus_encode_float(encoder,
                            (float*)audio->buffer,
                            frame_size,
                            message.audio.buffer,
                            message.audio.size);
#else
          opus_encode(encoder,
                      (opus_int16*)audio->buffer,
                      frame_size,
                      message.audio.buffer,
                      message.audio.size);
#endif
        if (result < 0) {
            fprintf(stderr,
                    "Failed to encode recorded packet: %s; Frame size %d\n",
                    opus_strerror(result),
                    frame_size);
            break;
        }

        message.audio.used = (uint32_t)result;

        const size_t bytesUsed = (frame_size * spec.channels * PCM_SIZE);
        uint8_tListQuickRemove(audio, 0, bytesUsed);

        MESSAGE_SERIALIZE(AudioMessage, message, temp);
        sendBytes(peer, 1, CLIENT_CHANNEL, &temp, false);
    }
    uint8_tListFree(&temp);
}

bool
selectAudioStreamSource(struct pollfd inputfd,
                        pBytes bytes,
                        pStreamDisplay display,
                        pAudioState state)
{
    askQuestion("Select audio source to stream from");
    for (AudioStreamSource i = 0; i < AudioStreamSource_Length; ++i) {
        printf("%d) %s\n", i + 1, AudioStreamSourceToCharString(i));
    }

    uint32_t selected;
    if (getIndexFromUser(
          inputfd, bytes, AudioStreamSource_Length, &selected, true) !=
        UserInputResult_Input) {
        puts("Canceled audio streaming seleciton");
        return false;
    }

    switch (selected) {
        case AudioStreamSource_File:
            askQuestion("Enter file to stream");
            while (!appDone) {
                switch (getUserInput(inputfd, bytes, NULL, CLIENT_POLL_WAIT)) {
                    case UserInputResult_Input: {
                        UserInput ui = { 0 };
                        ui.tag = UserInputTag_file;
                        ui.file = TemLangStringCreate((char*)bytes->buffer,
                                                      currentAllocator);
                        const bool result =
                          UserInputListAppend(&display->userInputs, &ui);
                        UserInputFree(&ui);
                        return result;
                    } break;
                    case UserInputResult_NoInput:
                        continue;
                    default:
                        puts("Canceled audio streaming file");
                        break;
                }
            }
            break;
        case AudioStreamSource_Microphone:
            return startRecording(inputfd, bytes, state);
        case AudioStreamSource_Window:
            fprintf(stderr, "Window audio streaming not implemented\n");
            break;
        default:
            fprintf(stderr, "Unknown option: %d\n", selected);
            break;
    }
    return false;
}

bool
startRecording(struct pollfd inputfd, pBytes bytes, pAudioState state)
{
    const int devices = SDL_GetNumAudioDevices(SDL_TRUE);
    if (devices == 0) {
        fprintf(stderr, "No recording devices found to send audio\n");
        return false;
    }

    state->isRecording = SDL_TRUE;
    askQuestion("Select a device to record from");
    for (int i = 0; i < devices; ++i) {
        printf("%d) %s\n", i + 1, SDL_GetAudioDeviceName(i, SDL_TRUE));
    }

    uint32_t selected;
    if (getIndexFromUser(inputfd, bytes, devices, &selected, true) !=
        UserInputResult_Input) {
        puts("Canceled recording selection");
        return false;
    }
    const SDL_AudioSpec desiredRecordingSpec = makeAudioSpec(NULL, NULL);
    state->deviceId =
      SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selected, SDL_TRUE),
                          SDL_TRUE,
                          &desiredRecordingSpec,
                          &state->spec,
                          0);
    if (state->deviceId == 0) {
        fprintf(stderr, "Failed to start recording: %s\n", SDL_GetError());
        return false;
    }
    puts("Recording audio specification");
    printAudioSpec(&state->spec);

    printf("Opened recording audio device: %u\n", state->deviceId);

    const int size = opus_encoder_get_size(state->spec.channels);
    state->encoder = currentAllocator->allocate(size);
    const int error = opus_encoder_init(state->encoder,
                                        state->spec.freq,
                                        state->spec.channels,
                                        OPUS_APPLICATION_VOIP);
    if (error < 0) {
        fprintf(
          stderr, "Failed to create voice encoder: %s\n", opus_strerror(error));
        return false;
    }
#if _DEBUG
    printf("Encoder: %p (%d)\n", state->encoder, size);
#endif

    return true;
}

bool
startPlayback(struct pollfd inputfd, pBytes bytes, pAudioState state)
{
    const int devices = SDL_GetNumAudioDevices(SDL_FALSE);
    if (devices == 0) {
        fprintf(stderr, "No playback devices found to play audio\n");
        return false;
    }

    askQuestion("Select a audio device to play audio from");
    state->isRecording = SDL_FALSE;
    state->packetLoss = 0u;
    for (int i = 0; i < devices; ++i) {
        printf("%d) %s\n", i + 1, SDL_GetAudioDeviceName(i, SDL_FALSE));
    }

    uint32_t selected;
    if (getIndexFromUser(inputfd, bytes, devices, &selected, true) !=
        UserInputResult_Input) {
        puts("Playback canceled");
        return false;
    }

    const SDL_AudioSpec desiredRecordingSpec = makeAudioSpec(NULL, NULL);
    state->deviceId =
      SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selected, SDL_FALSE),
                          SDL_FALSE,
                          &desiredRecordingSpec,
                          &state->spec,
                          0);
    if (state->deviceId == 0) {
        fprintf(stderr, "Failed to start playback: %s\n", SDL_GetError());
        return false;
    }
    puts("Playback audio specification");
    printAudioSpec(&state->spec);

    printf("Opened playback audio device: %u\n", state->deviceId);

    const int size = opus_decoder_get_size(state->spec.channels);
    state->decoder = currentAllocator->allocate(size);
    const int error =
      opus_decoder_init(state->decoder, state->spec.freq, state->spec.channels);
    if (error < 0) {
        fprintf(
          stderr, "Failed to create voice decoder: %s\n", opus_strerror(error));
        return false;
    }
#if _DEBUG
    printf("Decoder: %p (%d)\n", state->decoder, size);
#endif

    return true;
}

bool
sendDecodedAudioFrames(AVCodecContext* dec_ctx,
                       AVPacket* pkt,
                       AVFrame* frame,
                       OpusEncoder* encoder,
                       ENetPeer* peer,
                       pBytes bytes)
{
    int ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder\n");
        return false;
    }

    int sampleRate;
    switch (dec_ctx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
            sampleRate = AUDIO_U8;
            break;
        case AV_SAMPLE_FMT_S16:
            sampleRate = AUDIO_S16;
            break;
        case AV_SAMPLE_FMT_S32:
            sampleRate = AUDIO_S32;
            break;
        case AV_SAMPLE_FMT_FLT:
            sampleRate = AUDIO_F32;
            break;
        default:
            fprintf(
              stderr, "Cannot convert audio format: %d\n", dec_ctx->sample_fmt);
            return false;
    }

    const SDL_AudioSpec spec = makeAudioSpec(NULL, NULL);
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt,
                      sampleRate,
                      dec_ctx->channels,
                      dec_ctx->sample_rate,
                      spec.format,
                      spec.channels,
                      spec.freq);

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return false;
        }

        const int dataSize = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        if (dataSize < 0) {
            fprintf(stderr, "Failed to calculate size during decoding\n");
            return false;
        }
        for (int i = 0; i < frame->nb_samples; ++i) {
            for (int ch = 0; ch < dec_ctx->channels; ++ch) {
                while (bytes->size < (size_t)dataSize * cvt.len_mult) {
                    uint8_tListRellocateIfNeeded(bytes);
                }
                memcpy(bytes->buffer, frame->data[ch] + dataSize * i, dataSize);
                if (cvt.needed) {
                    cvt.buf = bytes->buffer;
                    cvt.len = dataSize;
                    SDL_ConvertAudio(&cvt);
                    bytes->used = cvt.len_cvt;
                } else {
                    bytes->used = dataSize;
                }
                sendAudioPackets(encoder, bytes, spec, peer);
            }
        }
    }

    return true;
}

void
decodeAudioData(const AudioExtension ext,
                const uint8_t* data,
                const size_t dataSize,
                ENetPeer* peer,
                pBytes bytes)
{
    printf("Sending audio data...\n");
    AVCodecContext* c = NULL;
    AVCodecParserContext* parser = NULL;
    AVPacket* pkt = NULL;
    AVFrame* frame = NULL;
    const AVCodec* codec = NULL;

    const int encoderSize = opus_encoder_get_size(2);
    OpusEncoder* encoder = currentAllocator->allocate(encoderSize);
    const int encoderError =
      opus_encoder_init(encoder, 48000, 2, OPUS_APPLICATION_AUDIO);
    if (encoderError < 0) {
        fprintf(stderr,
                "Failed to make Opus encoder: %s\n",
                opus_strerror(encoderError));
        goto audioEnd;
    }

    switch (ext) {
        case AudioExtension_FLAC:
            codec = avcodec_find_decoder(AV_CODEC_ID_FLAC);
            break;
        case AudioExtension_OGG:
            codec = avcodec_find_decoder(AV_CODEC_ID_VORBIS);
            break;
        case AudioExtension_MP3:
            codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
            break;
        default:
            break;
    }
    if (codec == NULL) {
        fprintf(stderr, "Failed to find audio decoder for file\n");
        goto audioEnd;
    }
    parser = av_parser_init(codec->id);
    if (parser == NULL) {
        fprintf(stderr, "Parser not found\n");
        goto audioEnd;
    }
    c = avcodec_alloc_context3(codec);
    if (c == NULL) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        goto audioEnd;
    }
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto audioEnd;
    }
    pkt = av_packet_alloc();
    size_t bytesRead = 0;
    while (bytesRead < dataSize) {
        if (frame == NULL) {
            frame = av_frame_alloc();
            if (frame == NULL) {
                fprintf(stderr, "Could not allocate frame\n");
                goto audioEnd;
            }
        }

        const int ret = av_parser_parse2(parser,
                                         c,
                                         &pkt->data,
                                         &pkt->size,
                                         data + bytesRead,
                                         dataSize - bytesRead,
                                         AV_NOPTS_VALUE,
                                         AV_NOPTS_VALUE,
                                         0);
        if (ret < 0) {
            fprintf(stderr, "Error while parsing\n");
            goto audioEnd;
        }
        if (pkt->size > 0 &&
            !sendDecodedAudioFrames(c, pkt, frame, encoder, peer, bytes)) {
            goto audioEnd;
        }
        bytesRead += ret;
    }
    pkt->data = NULL;
    pkt->size = 0;
    sendDecodedAudioFrames(c, pkt, frame, encoder, peer, bytes);
audioEnd:
    avcodec_free_context(&c);
    av_parser_close(parser);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    currentAllocator->free(encoder);
    printf("Done sending audio data\n");
}

void
selectStreamToStart(struct pollfd inputfd,
                    pBytes bytes,
                    ENetPeer* peer,
                    pClient client)
{
    LobbyMessage message = { 0 };

    askQuestion("Select the type of stream to create");
    for (uint32_t i = 0; i < ServerConfigurationDataTag_Length; ++i) {
        printf("%u) %s\n", i + 1U, ServerConfigurationDataTagToCharString(i));
    }
    puts("");

    uint32_t index;
    if (getIndexFromUser(
          inputfd, bytes, ServerConfigurationDataTag_Length, &index, true) !=
        UserInputResult_Input) {
        puts("Canceling start stream");
        goto end;
    }

    message.tag = LobbyMessageTag_startStreaming;
    message.startStreaming.data.tag = (ServerConfigurationDataTag)index;
    switch (index) {
        case ServerConfigurationDataTag_text:
            message.startStreaming.data.text = defaultTextConfiguration();
            break;
        case ServerConfigurationDataTag_chat:
            message.startStreaming.data.chat = defaultChatConfiguration();
            break;
        case ServerConfigurationDataTag_image:
            message.startStreaming.data.image = defaultImageConfiguration();
            break;
        case ServerConfigurationDataTag_audio:
            message.startStreaming.data.audio = defaultAudioConfiguration();
            break;
        default:
            puts("Canceling start stream");
            goto end;
    }

    askQuestion("What's the name of the stream?");
    if (getUserInput(inputfd, bytes, NULL, -1) != UserInputResult_Input) {
        puts("Canceling start stream");
        goto end;
    }
    puts("");

    message.startStreaming.name =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);

    askQuestion("Do you want exclusive write access? (y or n)");
    while (!appDone) {
        switch (getUserInput(inputfd, bytes, NULL, CLIENT_POLL_WAIT)) {
            case UserInputResult_Input:
                switch ((char)bytes->buffer[0]) {
                    case 'y':
                        index = 1;
                        goto continueMessage;
                    case 'n':
                        index = 0;
                        goto continueMessage;
                    default:
                        break;
                }
                break;
            case UserInputResult_NoInput:
                continue;
            default:
                break;
        }
        puts("Enter 'y' or 'n'");
    }

continueMessage:
    if (index) {
        message.startStreaming.writers.tag = AccessTag_list;
        message.startStreaming.writers.list.allocator = currentAllocator;
        TemLangStringListAppend(&message.startStreaming.writers.list,
                                &client->name);
    } else {
        message.startStreaming.writers.tag = AccessTag_anyone;
        message.startStreaming.writers.anyone = NULL;
    }

    message.startStreaming.readers.tag = AccessTag_anyone;
    message.startStreaming.readers.anyone = NULL;

    MESSAGE_SERIALIZE(LobbyMessage, message, (*bytes));
    printf(
      "\nCreating '%s' stream named '%s'...\n",
      ServerConfigurationDataTagToCharString(message.startStreaming.data.tag),
      message.startStreaming.name.buffer);
    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
end:
    LobbyMessageFree(&message);
}

void
selectStreamToStop(struct pollfd inputfd, pBytes bytes)
{
    const StreamDisplayList* list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("No streams to stop");
        return;
    }

    askQuestion("Select a stream to stop");
    for (uint32_t i = 0; i < clientData.displays.used; ++i) {
        const ServerConfiguration* config = NULL;
        if (GetStreamFromName(&clientData.allStreams,
                              &clientData.displays.buffer[i].config.name,
                              &config,
                              NULL)) {
            printf("%u) ", i + 1U);
            printServerConfigurationForClient(config);
        }
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1U) {
        puts("Stopping to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceled stopping stream");
        goto end;
    }

end:
    return;
}

void
selectStreamToSendTextTo(struct pollfd inputfd, pBytes bytes, pClient client)
{
    const StreamDisplayList* list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("No streams to send text to");
        return;
    }

    askQuestion("Send text to which stream?");
    for (uint32_t i = 0; i < clientData.displays.used; ++i) {
        const ServerConfiguration* config =
          &clientData.displays.buffer[i].config;
        printf("%u) ", i + 1U);
        printServerConfigurationForClient(config);
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1) {
        puts("Sending text to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling text send");
        goto end;
    }

    const ServerConfiguration* config = &clientData.displays.buffer[i].config;
    if (!clientHasWriteAccess(client, config)) {
        puts("Write access is not granted for this stream");
        goto end;
    }

    askQuestion("Enter text to send");
    if (getUserInput(inputfd, bytes, NULL, POLL_FOREVER) !=
        UserInputResult_Input) {
        puts("Canceling text send");
        goto end;
    }
    UserInput userInput = { 0 };
    userInput.text =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);
    userInput.tag = UserInputTag_text;
    UserInputListAppend(&list->buffer[i].userInputs, &userInput);
    UserInputFree(&userInput);
end:
    return;
}

void
selectStreamToUploadFileTo(struct pollfd inputfd, pBytes bytes, pClient client)
{
    const StreamDisplayList* list = &clientData.displays;
    if (StreamDisplayListIsEmpty(list)) {
        puts("No streams to upload file to");
        return;
    }

    askQuestion("Send file to which stream?");
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        const StreamDisplay* display = &clientData.displays.buffer[i];
        printf("%zu) ", i + 1U);
        printServerConfigurationForClient(&display->config);
    }
    puts("");

    uint32_t i = 0;
    if (list->used == 1U) {
        puts("Sending data to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    const ServerConfiguration* config = &clientData.displays.buffer[i].config;
    if (!clientHasWriteAccess(client, config)) {
        puts("Write access is not granted for this stream");
        goto end;
    }

    askQuestion("Enter file name");
    if (getUserInput(inputfd, bytes, NULL, -1) != UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    pStreamDisplay display = &clientData.displays.buffer[i];
    UserInput userInput = { 0 };
    userInput.tag = UserInputTag_file;
    userInput.file =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);
    UserInputListAppend(&display->userInputs, &userInput);
    UserInputFree(&userInput);

end:
    return;
}

void
saveScreenshot(SDL_Renderer* renderer, const StreamDisplay* display)
{
    if (renderer == NULL) {
        return;
    }

    SDL_Texture* temp = NULL;
    SDL_Surface* surface = NULL;

    if (display->texture == NULL) {
        puts("ServerConfiguration display has no texture");
        goto end;
    }

    int width;
    int height;
    if (SDL_QueryTexture(display->texture, 0, 0, &width, &height) != 0) {
        fprintf(stderr, "Failed to query texture: %s\n", SDL_GetError());
        goto end;
    }
    temp = SDL_CreateTexture(renderer,
                             SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET,
                             width,
                             height);
    if (temp == NULL) {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        goto end;
    }

    if (SDL_SetTextureBlendMode(temp, SDL_BLENDMODE_NONE) != 0 ||
        SDL_SetRenderTarget(renderer, temp) != 0 ||
        SDL_RenderCopy(renderer, display->texture, NULL, NULL) != 0) {
        fprintf(stderr, "Error with texture: %s\n", SDL_GetError());
        goto end;
    }

    surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create surface: %s\n", SDL_GetError());
        goto end;
    }

    if (SDL_RenderReadPixels(renderer,
                             NULL,
                             surface->format->format,
                             surface->pixels,
                             surface->pitch) != 0) {
        fprintf(stderr, "Failed to read pixels: %s\n", SDL_GetError());
        goto end;
    }

    char buffer[1024];
    const time_t t = time(NULL);
    strftime(buffer,
             sizeof(buffer),
             "screenshot_%y_%m_%d_%H_%M_%S.png",
             localtime(&t));
    if (IMG_SavePNG(surface, buffer) != 0) {
        fprintf(stderr, "Failed to save screenshot: %s\n", IMG_GetError());
        goto end;
    }

    printf("Saved screenshot to '%s'\n", buffer);

end:
    SDL_DestroyTexture(temp);
    SDL_FreeSurface(surface);
}

void
displayUserOptions()
{
    askQuestion("Choose an option");
    for (size_t i = 0; i < ClientCommand_Length; ++i) {
        TemLangString s = ClientCommandToString(i);
        camelCaseToNormal(&s);
        printf("%zu) %s\n", i + 1UL, s.buffer);
        TemLangStringFree(&s);
    }
    puts("");
}

void
checkUserInput(SDL_Renderer* renderer,
               pBytes bytes,
               ENetPeer* peer,
               pClient client,
               pRandomState rs)
{
    struct pollfd inputfd = { .events = POLLIN,
                              .revents = 0,
                              .fd = STDIN_FILENO };
    uint32_t index = 0;
    switch (
      getIndexFromUser(inputfd, bytes, ClientCommand_Length, &index, false)) {
        case UserInputResult_Input:
            break;
        case UserInputResult_Error:
            displayUserOptions();
            return;
        default:
            return;
    }

    SDL_LockMutex(clientData.mutex);
    switch (index) {
        case ClientCommand_PrintName:
            printf("Client name: %s\n", client->name.buffer);
            break;
        case ClientCommand_Quit:
            appDone = true;
            break;
        case ClientCommand_StartStreaming:
            selectStreamToStart(inputfd, bytes, peer, client);
            break;
        case ClientCommand_StopStreaming:
            selectStreamToStop(inputfd, bytes);
            break;
        case ClientCommand_ConnectToStream:
            selectAStreamToConnectTo(inputfd, bytes, rs);
            break;
        case ClientCommand_DisconnectFromStream:
            selectAStreamToDisconnectFrom(inputfd, bytes);
            break;
        case ClientCommand_ShowAllStreams: {
            LobbyMessage lm = { 0 };
            lm.tag = LobbyMessageTag_allStreams;
            lm.allStreams = NULL;
            MESSAGE_SERIALIZE(LobbyMessage, lm, (*bytes));
            sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
        } break;
        case ClientCommand_ShowConnectedStreams: {
            askQuestion("Connected Streams");
            const ServerConfiguration* config = NULL;
            for (size_t i = 0; i < clientData.displays.used; ++i) {
                if (GetStreamFromName(
                      &clientData.allStreams,
                      &clientData.displays.buffer[i].config.name,
                      &config,
                      NULL)) {
                    printServerConfigurationForClient(config);
                }
            }
        } break;
        case ClientCommand_SaveScreenshot: {
            const StreamDisplayList* list = &clientData.displays;
            if (StreamDisplayListIsEmpty(list)) {
                puts("No streams to take screenshot from");
                break;
            }
            askQuestion("Select stream to take screenshot from");
            for (size_t i = 0; i < clientData.displays.used; ++i) {
                const StreamDisplay* display = &list->buffer[i];
                printf("%zu) ", i + 1U);
                printServerConfigurationForClient(&display->config);
            }
            puts("");

            uint32_t i = 0;
            if (list->used == 1) {
                puts("Taking screenshot from only available stream");
            } else if (getIndexFromUser(inputfd, bytes, list->used, &i, true) !=
                       UserInputResult_Input) {
                puts("Canceling screenshot");
                break;
            }
            saveScreenshot(renderer, &list->buffer[i]);
        } break;
        case ClientCommand_UploadFile:
            selectStreamToUploadFileTo(inputfd, bytes, client);
            break;
        case ClientCommand_UploadText:
            selectStreamToSendTextTo(inputfd, bytes, client);
            break;
        default:
            fprintf(stderr,
                    "Command '%s' is not implemented\n",
                    ClientCommandToCharString(index));
            break;
    }
    SDL_UnlockMutex(clientData.mutex);
}

void
updateTextDisplay(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  pStreamDisplay display)
{
    if (renderer == NULL) {
        return;
    }
    SDL_Surface* surface = NULL;

    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }

    if (TemLangStringIsEmpty(&display->data.text)) {
        goto end;
    }

    SDL_Color fg = { 0 };
    fg.r = fg.g = fg.b = fg.a = 0xffu;
    SDL_Color bg = { 0 };
    bg.a = 128u;
    surface = TTF_RenderUTF8_Shaded_Wrapped(
      ttfFont, display->data.text.buffer, fg, bg, 0);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create text surface: %s\n", TTF_GetError());
        goto end;
    }

    display->srcRect.tag = OptionalRectTag_none;
    display->srcRect.none = NULL;

    // display->dstRect.x = 0.f;
    // display->dstRect.y = 0.f;
    display->dstRect.w = surface->w;
    display->dstRect.h = surface->h;

    display->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (display->texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
    }

end:
    SDL_FreeSurface(surface);
}

void
updateImageDisplay(SDL_Renderer* renderer, pStreamDisplay display)
{
    if (renderer == NULL) {
        return;
    }

    const Bytes* bytes = &display->data.image;
    SDL_Surface* surface = NULL;

    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }

    if (uint8_tListIsEmpty(bytes)) {
        goto end;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(bytes->buffer, bytes->used);
    if (rw == NULL) {
        fprintf(stderr, "Failed to load image memory: %s\n", SDL_GetError());
        goto end;
    }
    surface = IMG_Load_RW(rw, 0);
    if (surface == NULL) {
        fprintf(stderr, "Failed to load image: %s\n", SDL_GetError());
        goto end;
    }

    display->srcRect.tag = OptionalRectTag_none;
    display->srcRect.none = NULL;

    // display->dstRect.x = 0.f;
    // display->dstRect.y = 0.f;
    display->dstRect.w = surface->w;
    display->dstRect.h = surface->h;

    display->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (display->texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
    }
end:
    SDL_FreeSurface(surface);
}

SDL_Rect
renderText(SDL_Renderer* renderer,
           TTF_Font* ttfFont,
           const char* text,
           const int x,
           const int y,
           const SDL_Color fg,
           const SDL_Color bg,
           const uint32_t wrapped)
{
    SDL_Rect rect = { 0 };
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    if (renderer == NULL) {
        goto end;
    }

    surface = TTF_RenderUTF8_Shaded_Wrapped(ttfFont, text, fg, bg, wrapped);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create text surface: %s\n", TTF_GetError());
        goto end;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
        goto end;
    }
    SDL_SetTextureColorMod(texture, 0xffu, 0xffu, 0xffu);
    SDL_SetTextureAlphaMod(texture, 0xffu);

    rect = (SDL_Rect){ .x = x, .y = y, .w = surface->w, .h = surface->h };
    if (SDL_RenderCopy(renderer, texture, NULL, &rect) != 0) {
        fprintf(stderr, "Failed to render text surface: %s\n", SDL_GetError());
        goto end;
    }

end:
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return rect;
}

size_t
countChar(const char* str, const char target)
{
    size_t count = 0;
    while (*str != '\0') {
        if (*str == target) {
            ++count;
        }
        ++str;
    }
    return count;
}

void
updateChatDisplay(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  const uint32_t windowWidth,
                  const uint32_t windowHeight,
                  pStreamDisplay display)
{
    if (renderer == NULL) {
        return;
    }
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }

    const StreamDisplayChat* chat = &display->data.chat;
    if (ChatListIsEmpty(&chat->logs)) {
        return;
    }

    // SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 0xffu);

    display->data.tag = StreamDisplayDataTag_chat;
    char buffer[128] = { 0 };

    const SDL_Color white = { .r = 0xffu, .g = 0xffu, .b = 0xffu, .a = 0xffu };
    const SDL_Color purple = { .r = 0xffu, .g = 0x0u, .b = 0xffu, .a = 0xffu };
    const SDL_Color yellow = { .r = 0xffu, .g = 0xffu, .b = 0x0u, .a = 0xffu };
    const SDL_Color bg = { .r = 0u, .g = 0u, .b = 0u, .a = 0u };
    const SDL_Color black = { .r = 0u, .g = 0u, .b = 0u, .a = 0xffu };

    uint32_t offset = chat->offset;
    if (chat->logs.used >= chat->count) {
        offset = SDL_clamp(offset, 0U, chat->logs.used - chat->count);
    } else {
        offset = 0;
    }

    display->texture = SDL_CreateTexture(renderer,
                                         SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         (int)windowWidth,
                                         (int)windowHeight);
    if (display->texture == NULL) {
        fprintf(stderr,
                "Failed to update text display texture: %s\n",
                SDL_GetError());
        return;
    }

    SDL_SetTextureBlendMode(display->texture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(renderer, display->texture);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu, 128u);
    SDL_RenderClear(renderer);

    SDL_Rect rect = { 0 };
    float maxW = 0.f;
    uint32_t y = 0;
    for (uint32_t j = offset, n = offset + chat->count;
         j < n && j < chat->logs.used;
         ++j) {
        const Chat* cm = &chat->logs.buffer[j];
        const time_t t = (time_t)cm->timestamp;

        strftime(buffer, sizeof(buffer), "%c", localtime(&t));
        rect = renderText(
          renderer, ttfFont, buffer, 0, y, purple, black, windowWidth);
        maxW = SDL_max(maxW, rect.w);

        const float oldW = rect.w;
        rect = renderText(renderer,
                          ttfFont,
                          cm->author.buffer,
                          rect.w,
                          y,
                          yellow,
                          black,
                          windowWidth);
        maxW = SDL_max(maxW, oldW + rect.w);
        y += rect.h;

        rect = renderText(
          renderer, ttfFont, cm->message.buffer, 0, y, white, bg, windowWidth);
        maxW = SDL_max(maxW, rect.w);
        y += rect.h;
    }

    display->srcRect.tag = OptionalRectTag_rect;
    display->srcRect.rect.x = 0;
    display->srcRect.rect.y = 0;
    display->srcRect.rect.w = maxW;
    display->srcRect.rect.h = y;

    if (display->dstRect.w < MIN_WIDTH) {
        display->dstRect.w = maxW;
    }
    if (display->dstRect.h < MIN_HEIGHT) {
        display->dstRect.h = y;
    }
}

void
updateAllDisplays(SDL_Renderer* renderer,
                  TTF_Font* ttfFont,
                  const int w,
                  const int h)
{
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        switch (display->data.tag) {
            case StreamDisplayDataTag_chat:
                updateChatDisplay(renderer, ttfFont, w, h, display);
                break;
            case StreamDisplayDataTag_text:
                updateTextDisplay(renderer, ttfFont, display);
                break;
            default:
                break;
        }
    }
}

void
drawTextures(SDL_Renderer* renderer,
             const size_t target,
             const float maxX,
             const float maxY)
{
    if (renderer == NULL) {
        return;
    }
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0x33u, 0x33u, 0x33u, 0xffu);
    SDL_RenderClear(renderer);

    if (target < clientData.displays.used) {
        const StreamDisplay* display = &clientData.displays.buffer[target];
        if (display->texture != NULL) {
            SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0x0u, 0xffu);
            const SDL_FRect rect =
              expandRect((const SDL_FRect*)&display->dstRect, 1.025f, 1.025f);
            SDL_RenderFillRectF(renderer, &rect);
        }
    }
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        SDL_Texture* texture = display->texture;
        if (texture == NULL) {
            continue;
        }
        SDL_FRect* rect = (SDL_FRect*)&display->dstRect;
        rect->x = SDL_clamp(rect->x, 0, maxX);
        rect->y = SDL_clamp(rect->y, 0, maxY);
        SDL_SetTextureColorMod(texture, 0xffu, 0xffu, 0xffu);
        SDL_SetTextureAlphaMod(texture, 0xffu);
        int result;
        switch (display->srcRect.tag) {
            case OptionalRectTag_rect: {
                SDL_Rect r = { 0 };
                r.x = display->srcRect.rect.x;
                r.y = display->srcRect.rect.y;
                r.w = display->srcRect.rect.w;
                r.h = display->srcRect.rect.h;
                result = SDL_RenderCopyF(renderer,
                                         texture,
                                         &r,
                                         //   NULL,
                                         rect);
            } break;
            default:
                result = SDL_RenderCopyF(renderer, texture, NULL, rect);
                break;
        }
        if (result != 0) {
            fprintf(stderr, "Failed to render: %s\n", SDL_GetError());
        }
    }

    // uint8_t background[4] = { 0xff, 0xffu, 0x0u, 0xffu };
    // renderFont(renderer, font, "Hello World", 0, 0, 5.f, NULL,
    // background);
    SDL_RenderPresent(renderer);
}

void
printRenderInfo(const SDL_RendererInfo* info)
{
    puts(info->name);
    for (size_t i = 0; i < info->num_texture_formats; ++i) {
        printf(
          "%zu) %s\n", i + 1, SDL_GetPixelFormatName(info->texture_formats[i]));
    }
}

bool
findDisplayFromPoint(const SDL_FPoint* point, size_t* targetDisplay)
{
    for (int64_t i = (int64_t)clientData.displays.used - 1LL; i >= 0LL; --i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        if (display->texture == NULL) {
            continue;
        }
        if (!SDL_PointInFRect(point, (const SDL_FRect*)&display->dstRect)) {
            continue;
        }
        *targetDisplay = (size_t)i;
        return true;
    }
    return false;
}

bool
loadNewFont(const char* filename, const int fontSize, TTF_Font** ttfFont)
{
    TTF_Font* newFont = TTF_OpenFont(filename, fontSize);
    if (newFont == NULL) {
        fprintf(stderr, "Failed to load new font: %s\n", TTF_GetError());
        return false;
    }
    TTF_CloseFont(*ttfFont);
    *ttfFont = newFont;
    return true;
}

void
refrehClients(ENetPeer* peer, pBytes bytes)
{
    LobbyMessage lm = { 0 };
    lm.tag = LobbyMessageTag_general;
    lm.general.getClients = NULL;
    lm.general.tag = GeneralMessageTag_getClients;
    MESSAGE_SERIALIZE(LobbyMessage, lm, (*bytes));
    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
}

void
refreshStreams(ENetPeer* peer)
{
    LobbyMessage lm = { 0 };
    lm.tag = LobbyMessageTag_allStreams;
    lm.allStreams = NULL;
    uint8_t buffer[KB(1)] = { 0 };
    Bytes bytes = {
        .allocator = NULL, .buffer = buffer, .size = sizeof(buffer), .used = 0
    };
    MESSAGE_SERIALIZE(LobbyMessage, lm, bytes);
    sendBytes(peer, 1, CLIENT_CHANNEL, &bytes, true);
}

bool
clientHandleGeneralMessage(const GeneralMessage* message,
                           pClient client,
                           ENetPeer* peer)
{
    switch (message->tag) {
        case GeneralMessageTag_getClientsAck:
            askQuestion("Clients");
            for (size_t i = 0; i < message->getClientsAck.used; ++i) {
                puts(message->getClientsAck.buffer[i].buffer);
            }
            puts("");
            return true;
        case GeneralMessageTag_authenticateAck:
            if (client == NULL) {
                return true;
            }
            TemLangStringCopy(
              &client->name, &message->authenticateAck, currentAllocator);
            printf("Client authenticated as %s\n", client->name.buffer);
            if (peer != NULL) {
                refreshStreams(peer);
            }
            return true;
        default:
            printf("Unexpected message from lobby server: %s\n",
                   GeneralMessageTagToCharString(message->tag));
            return false;
    }
}

bool
clientHandleLobbyMessage(const LobbyMessage* message,
                         ENetPeer* peer,
                         pClient client)
{
    bool result = false;
    switch (message->tag) {
        case LobbyMessageTag_allStreamsAck:
            result = ServerConfigurationListCopy(&clientData.allStreams,
                                                 &message->allStreamsAck,
                                                 currentAllocator);
            askQuestion("All Streams");
            for (size_t i = 0; i < clientData.allStreams.used; ++i) {
                printServerConfigurationForClient(
                  &clientData.allStreams.buffer[i]);
            }
            renderDisplays();
            goto end;
        case LobbyMessageTag_startStreamingAck:
            result = true;
            if (message->startStreamingAck) {
                puts("Started stream.");
            } else {
                puts("Failed to start stream");
            }
            refreshStreams(peer);
            goto end;
        case LobbyMessageTag_general:
            result =
              clientHandleGeneralMessage(&message->general, client, peer);
            goto end;
        default:
            break;
    }
    printf("Unexpected message from lobby server: %s\n",
           LobbyMessageTagToCharString(message->tag));
    appDone = true;
end:
    return result;
}

bool
checkForMessagesFromLobby(ENetHost* host, ENetEvent* event, pClient client)
{
    bool result = false;
    while (!appDone && enet_host_service(host, event, 0U) >= 0) {
        switch (event->type) {
            case ENET_EVENT_TYPE_CONNECT:
                puts("Unexpected connect event from server");
                appDone = true;
                goto end;
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("Disconnected from server");
                appDone = true;
                goto end;
            case ENET_EVENT_TYPE_NONE:
                result = true;
                goto end;
            case ENET_EVENT_TYPE_RECEIVE: {
                result = true;
                const Bytes temp = { .allocator = currentAllocator,
                                     .buffer = event->packet->data,
                                     .size = event->packet->dataLength,
                                     .used = event->packet->dataLength };

                LobbyMessage message = { 0 };
                MESSAGE_DESERIALIZE(LobbyMessage, message, temp);

                IN_MUTEX(clientData.mutex, f, {
                    result =
                      clientHandleLobbyMessage(&message, event->peer, client);
                });

                enet_packet_destroy(event->packet);
                LobbyMessageFree(&message);
            } break;
            default:
                break;
        }
    }
end:
    return result;
}

int
displayError(SDL_Window* window, const char* e, const bool force)
{
    if (window != NULL || force) {
        return SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, "SDL error", e, window);
    } else {
        return 0;
    }
}

void
handleUserInput(ENetPeer* peer,
                pBytes bytes,
                const ServerConfigurationDataTag serverType,
                const UserInput* userInput)
{
    switch (userInput->tag) {
        case UserInputTag_disconnect:
            enet_peer_disconnect(peer, 0);
            break;
        case UserInputTag_text:
            switch (serverType) {
                case ServerConfigurationDataTag_text: {
                    TextMessage message = { 0 };
                    message.tag = TextMessageTag_text;
                    message.text =
                      TemLangStringClone(&userInput->text, currentAllocator);
                    MESSAGE_SERIALIZE(TextMessage, message, (*bytes));
                    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
                    TextMessageFree(&message);
                } break;
                case ServerConfigurationDataTag_chat: {
                    ChatMessage message = { 0 };
                    message.tag = ChatMessageTag_message;
                    message.message =
                      TemLangStringClone(&userInput->text, currentAllocator);
                    MESSAGE_SERIALIZE(ChatMessage, message, (*bytes));
                    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
                    ChatMessageFree(&message);
                } break;
                default:
                    fprintf(stderr,
                            "Cannot send text to '%s' server\n",
                            ServerConfigurationDataTagToCharString(serverType));
                    break;
            }
            break;
        case UserInputTag_file: {
            int fd = -1;
            char* ptr = NULL;
            size_t size = 0;

            FileExtension ext = { 0 };
            if (!filenameToExtension(userInput->file.buffer, &ext)) {
                puts("Failed to find file extension");
                goto endDropFile;
            }

            if (!CanSendFileToStream(ext.tag, serverType)) {
                fprintf(stderr,
                        "Cannot send '%s' file to '%s' server\n",
                        FileExtensionTagToCharString(ext.tag),
                        ServerConfigurationDataTagToCharString(serverType));
                goto endDropFile;
            }

            if (!mapFile(
                  userInput->file.buffer, &fd, &ptr, &size, MapFileType_Read)) {
                fprintf(stderr,
                        "Error opening file '%s': %s\n",
                        userInput->file.buffer,
                        strerror(errno));
                goto endDropFile;
            }

            switch (serverType) {
                case ServerConfigurationDataTag_text: {
                    TextMessage message = { 0 };
                    message.tag = TextMessageTag_text;
                    message.text =
                      TemLangStringCreateFromSize(ptr, size, currentAllocator);
                    MESSAGE_SERIALIZE(TextMessage, message, (*bytes));
                    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
                    TextMessageFree(&message);
                } break;
                case ServerConfigurationDataTag_chat: {
                    ChatMessage message = { 0 };
                    message.tag = ChatMessageTag_message;
                    message.message =
                      TemLangStringCreateFromSize(ptr, size, currentAllocator);
                    MESSAGE_SERIALIZE(ChatMessage, message, (*bytes));
                    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
                    ChatMessageFree(&message);
                } break;
                case ServerConfigurationDataTag_image: {
                    ImageMessage message = { 0 };

                    message.tag = ImageMessageTag_imageStart;
                    message.imageStart = NULL;
                    MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
                    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);

                    message.tag = ImageMessageTag_imageChunk;
                    for (size_t i = 0; i < size; i += peer->mtu) {
                        message.imageChunk.buffer = (uint8_t*)ptr + i;
                        const size_t s = SDL_min(peer->mtu, size - i);
                        message.imageChunk.used = s;
                        message.imageChunk.size = s;
                        MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
                        sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
                        enet_host_flush(peer->host);
                    }

                    message.tag = ImageMessageTag_imageEnd;
                    message.imageEnd = NULL;
                    MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
                    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, true);
                } break;
                case ServerConfigurationDataTag_audio: {
                    if (ext.tag != FileExtensionTag_audio) {
                        fprintf(stderr,
                                "Must send audio file to audio stream\n");
                        break;
                    }
                    decodeAudioData(
                      ext.audio, (uint8_t*)ptr, size, peer, bytes);
                } break;
                default:
                    fprintf(stderr,
                            "Sending '%s' not implemented\n",
                            ServerConfigurationDataTagToCharString(serverType));
                    break;
            }

            // Don't free message since it wasn't allocated
        endDropFile:
            unmapFile(fd, ptr, size);
        } break;
        default:
            break;
    }
}

void
handleUserEvent(const SDL_UserEvent* e,
                SDL_Window* window,
                SDL_Renderer* renderer,
                TTF_Font* ttfFont,
                const size_t targetDisplay,
                ENetPeer* peer)
{
    int w;
    int h;
    SDL_GetWindowSize(window, &w, &h);
    switch (e->code) {
        case CustomEvent_Render:
            drawTextures(
              renderer, targetDisplay, (float)w - 32.f, (float)h - 32.f);
            break;
        case CustomEvent_RefreshStreams:
            refreshStreams(peer);
            break;
        case CustomEvent_ShowSimpleMessage:
            SDL_ShowSimpleMessageBox(
              SDL_MESSAGEBOX_ERROR, (char*)e->data1, (char*)e->data2, window);
            break;
        case CustomEvent_UpdateStreamDisplay: {
            const Guid id = { .numbers = { (uint64_t)(size_t)e->data1,
                                           (uint64_t)(size_t)e->data2 } };
            size_t index = 0;
            if (!GetStreamDisplayFromGuid(
                  &clientData.displays, &id, NULL, &index)) {
                fprintf(stderr,
                        "Cannot update stream display because it was removed "
                        "from list\n");
                break;
            }
            pStreamDisplay display = &clientData.displays.buffer[index];
            switch (display->data.tag) {
                case StreamDisplayDataTag_text:
                    updateTextDisplay(renderer, ttfFont, display);
                    break;
                case StreamDisplayDataTag_chat:
                    updateChatDisplay(renderer, ttfFont, w, h, display);
                    break;
                case StreamDisplayDataTag_image:
                    updateImageDisplay(renderer, display);
                    break;
                default:
                    break;
            }
            renderDisplays();
        } break;
        default:
            break;
    }
}

int
runClient(const Configuration* configuration)
{
    clientData.authentication =
      (NullValue)&configuration->client.authentication;

    int result = EXIT_FAILURE;
    puts("Running client");
    printConfiguration(configuration);

    const ClientConfiguration* config = &configuration->client;

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    // Font font = { 0 };
    TTF_Font* ttfFont = NULL;
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };

    clientData.displays.allocator = currentAllocator;
    clientData.allStreams.allocator = currentAllocator;
    const bool showWindow = !config->noGui;
    {
        uint32_t flags = 0;
        if (showWindow) {
            flags |= SDL_INIT_VIDEO;
        }
        if (!config->noAudio) {
            flags |= SDL_INIT_AUDIO;
        }
        if (SDL_Init(flags) != 0) {
            fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
    }

    clientData.mutex = SDL_CreateMutex();
    if (clientData.mutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
    }

    if (showWindow) {
        const uint32_t flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
        if (IMG_Init(flags) != flags) {
            fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
        if (TTF_Init() == -1) {
            fprintf(stderr, "Failed to init TTF: %s\n", TTF_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
    }

    if (showWindow) {
        window = SDL_CreateWindow(
          "TemStream Client",
          SDL_WINDOWPOS_UNDEFINED,
          SDL_WINDOWPOS_UNDEFINED,
          config->windowWidth,
          config->windowHeight,
          SDL_WINDOW_RESIZABLE |
            (config->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
        if (window == NULL) {
            fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }

        {
            SDL_RendererInfo info = { 0 };
#if _DEBUG
            int drivers = SDL_GetNumRenderDrivers();
            for (int i = 0; i < drivers; ++i) {
                SDL_GetRenderDriverInfo(i, &info);
                printRenderInfo(&info);
            }
#endif

            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (renderer == NULL) {
                fprintf(
                  stderr, "Failed to create renderer: %s\n", SDL_GetError());
                displayError(window, "Failed to start", showWindow);
                goto end;
            }

            SDL_GetRendererInfo(renderer, &info);
            printf("Current renderer: %s\n", info.name);
        }

        // if (!loadFont(config->ttfFile.buffer, config->fontSize, renderer,
        // &font))
        // {
        //     fprintf(stderr, "Failed to load font\n");
        //     goto end;
        // }

        ttfFont = TTF_OpenFont(config->ttfFile.buffer, config->fontSize);
        if (ttfFont == NULL) {
            fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
            displayError(window, "Failed to start", showWindow);
            goto end;
        }
    }

    appDone = false;

    SDL_AtomicSet(&runningThreads, 0);

    SDL_Event e = { 0 };

    size_t targetDisplay = UINT32_MAX;
    MoveMode moveMode = MoveMode_None;
    bool hasTarget = false;

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    Client client = { 0 };

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        appDone = true;
        goto end;
    }
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, configuration->client.hostname.buffer);
        address.port = configuration->client.port;
        peer = enet_host_connect(
          host, &address, 2, ServerConfigurationDataTag_lobby);
        char buffer[512] = { 0 };
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));
        printf("Connecting to server: %s:%u...\n", buffer, address.port);
    }
    if (peer == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        appDone = true;
        goto end;
    }

    ENetEvent event = { 0 };
    if (enet_host_service(host, &event, 5000U) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        char buffer[KB(1)] = { 0 };
        enet_address_get_host_ip(&event.peer->address, buffer, sizeof(buffer));
        printf(
          "Connected to server: %s:%u\n", buffer, event.peer->address.port);
        displayUserOptions();
        sendAuthentication(peer, ServerConfigurationDataTag_lobby);
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        appDone = true;
        goto end;
    }

    RandomState rs = makeRandomState();
    while (!appDone) {
        if (!checkForMessagesFromLobby(host, &event, &client)) {
            appDone = true;
            break;
        }
        checkUserInput(renderer, &bytes, peer, &client, &rs);
        while (!appDone && SDL_PollEvent(&e)) {
            SDL_LockMutex(clientData.mutex);
            switch (e.type) {
                case SDL_QUIT:
                    appDone = true;
                    break;
                case SDL_WINDOWEVENT:
                    renderDisplays();
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_F1:
                        case SDLK_ESCAPE:
                            displayUserOptions();
                            break;
                        case SDLK_F2:
                            printf("Memory: %zu (%zu MB) / %zu (%zu MB) \n",
                                   currentAllocator->used(),
                                   currentAllocator->used() / (MB(1)),
                                   currentAllocator->totalSize(),
                                   currentAllocator->totalSize() / (MB(1)));
                            break;
                        case SDLK_F3:
                            if (hasTarget) {
                                printf("Target display: %zu\n", targetDisplay);
                            } else {
                                puts("No target display");
                            }
                            break;
                        case SDLK_v:
                            if ((e.key.keysym.mod & KMOD_CTRL) == 0) {
                                break;
                            }
                            e.type = SDL_DROPTEXT;
                            e.drop.file = SDL_GetClipboardText();
                            e.drop.timestamp = SDL_GetTicks64();
                            e.drop.windowID = SDL_GetWindowID(window);
                            SDL_PushEvent(&e);
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT:
                            moveMode = MoveMode_Position;
                            break;
                        case SDL_BUTTON_RIGHT:
                        case SDL_BUTTON_MIDDLE:
                            moveMode = MoveMode_Size;
                            break;
                        default:
                            moveMode = MoveMode_None;
                            break;
                    }
                    SDL_FPoint point = { 0 };
                    point.x = e.button.x;
                    point.y = e.button.y;
                    if (findDisplayFromPoint(&point, &targetDisplay)) {
                        hasTarget = true;
                        SDL_SetWindowGrab(window, true);
                        renderDisplays();
                    }
                } break;
                case SDL_MOUSEBUTTONUP:
                    hasTarget = false;
                    targetDisplay = UINT32_MAX;
                    SDL_SetWindowGrab(window, false);
                    renderDisplays();
                    break;
                case SDL_MOUSEMOTION:
                    if (hasTarget) {
                        if (targetDisplay >= clientData.displays.used) {
                            break;
                        }
                        pStreamDisplay display =
                          &clientData.displays.buffer[targetDisplay];
                        if (display->texture == NULL) {
                            break;
                        }
                        int w;
                        int h;
                        SDL_GetWindowSize(window, &w, &h);
                        switch (moveMode) {
                            case MoveMode_Position:
                                display->dstRect.x =
                                  display->dstRect.x + e.motion.xrel;
                                display->dstRect.y =
                                  display->dstRect.y + e.motion.yrel;
                                break;
                            case MoveMode_Size:
                                display->dstRect.w =
                                  SDL_clamp(display->dstRect.w + e.motion.xrel,
                                            MIN_WIDTH,
                                            w);
                                display->dstRect.h =
                                  SDL_clamp(display->dstRect.h + e.motion.yrel,
                                            MIN_HEIGHT,
                                            h);
                                break;
                            default:
                                break;
                        }
                        display->dstRect.x = SDL_clamp(
                          display->dstRect.x, 0.f, w - display->dstRect.w);
                        display->dstRect.y = SDL_clamp(
                          display->dstRect.y, 0.f, h - display->dstRect.h);
                        renderDisplays();
                    } else {
                        SDL_FPoint point = { 0 };
                        int x;
                        int y;
                        SDL_GetMouseState(&x, &y);
                        point.x = (float)x;
                        point.y = (float)y;
                        targetDisplay = UINT_MAX;
                        findDisplayFromPoint(&point, &targetDisplay);
                        renderDisplays();
                    }
                    break;
                case SDL_MOUSEWHEEL: {
                    if (targetDisplay >= clientData.displays.used) {
                        break;
                    }
                    int w;
                    int h;
                    SDL_GetWindowSize(window, &w, &h);
                    pStreamDisplay display =
                      &clientData.displays.buffer[targetDisplay];
                    switch (display->data.tag) {
                        case StreamDisplayDataTag_chat: {
                            pStreamDisplayChat chat = &display->data.chat;
                            int64_t offset = (int64_t)chat->offset;

                            // printf("Before: %" PRId64 "\n", offset);
                            offset += (e.wheel.y > 0) ? -1LL : 1LL;
                            offset = SDL_clamp(
                              offset, 0LL, (int64_t)chat->logs.used - 1LL);
                            chat->offset = (uint32_t)offset;
                            // printf("After: %u\n", chat->offset);
                            // printf("Logs %u\n", chat->logs.used);

                            const float width = display->dstRect.w;
                            const float height = display->dstRect.h;
                            updateChatDisplay(renderer, ttfFont, w, h, display);
                            display->dstRect.w = width;
                            display->dstRect.h = height;
                        } break;
                        default:
                            break;
                    }
                    renderDisplays();
                } break;
                case SDL_USEREVENT:
                    handleUserEvent(
                      &e.user, window, renderer, ttfFont, targetDisplay, peer);
                    break;
                case SDL_DROPFILE: {
                    // Look for image stream
                    printf("Got dropped file: %s\n", e.drop.file);
                    FileExtension ext = { 0 };
                    if (!filenameToExtension(e.drop.file, &ext)) {
                        puts("Failed to find file extension");
                        goto endDropFile;
                    }
                    if (ext.tag == FileExtensionTag_font) {
                        puts("Loading new font...");
                        if (loadNewFont(
                              e.drop.file, config->fontSize, &ttfFont)) {
                            int w, h;
                            SDL_GetWindowSize(window, &w, &h);
                            updateAllDisplays(renderer, ttfFont, w, h);
                            puts("Loaded new font");
                        }
                        goto endDropFile;
                    }
                    pStreamDisplay display = NULL;
                    if (targetDisplay < clientData.displays.used) {
                        display = &clientData.displays.buffer[targetDisplay];
                    } else {
                        for (size_t i = 0; i < clientData.displays.used; ++i) {
                            if (CanSendFileToStream(
                                  ext.tag,
                                  clientData.displays.buffer[i]
                                    .config.data.tag)) {
                                display = &clientData.displays.buffer[i];
                                break;
                            }
                        }
                    }
                    if (display == NULL) {
                        puts("No stream to send file too...");
                        goto endDropFile;
                    }
                    UserInput userInput = { 0 };
                    userInput.tag = UserInputTag_file;
                    userInput.file =
                      TemLangStringCreate(e.drop.file, currentAllocator);
                    UserInputListAppend(&display->userInputs, &userInput);
                    UserInputFree(&userInput);
                endDropFile:
                    SDL_free(e.drop.file);
                } break;
                case SDL_DROPTEXT: {
                    // Look for a text or chat stream
                    puts("Got dropped text");
                    pStreamDisplay display = NULL;
                    if (targetDisplay < clientData.displays.used) {
                        display = &clientData.displays.buffer[targetDisplay];
                    } else {
                        for (size_t i = 0; i < clientData.displays.used; ++i) {
                            if (CanSendFileToStream(
                                  FileExtensionTag_text,
                                  clientData.displays.buffer[i]
                                    .config.data.tag)) {
                                display = &clientData.displays.buffer[i];
                                break;
                            }
                        }
                    }
                    if (display == NULL) {
                        puts("No stream to send text too...");
                        goto endDropText;
                    }
                    UserInput userInput = { 0 };
                    userInput.tag = UserInputTag_text;
                    userInput.text =
                      TemLangStringCreate(e.drop.file, currentAllocator);
                    UserInputListAppend(&display->userInputs, &userInput);
                    UserInputFree(&userInput);
                endDropText:
                    SDL_free(e.drop.file);
                } break;
                default:
                    break;
            }
            SDL_UnlockMutex(clientData.mutex);
        }
        SDL_Delay(1);
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    // FontFree(&font);
    while (SDL_AtomicGet(&runningThreads) > 0) {
        SDL_Delay(1);
    }
    closeHostAndPeer(host, peer);
    ClientFree(&client);
    uint8_tListFree(&bytes);
    ClientDataFree(&clientData);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(ttfFont);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return result;
}