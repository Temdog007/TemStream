#include <include/main.h>

#define MIN_WIDTH 32
#define MIN_HEIGHT 32

#define RENDER_NOW(w, h, info)                                                 \
    drawTextures(info.renderer,                                                \
                 info.font,                                                    \
                 targetDisplay,                                                \
                 (float)w - MIN_WIDTH,                                         \
                 (float)h - MIN_HEIGHT,                                        \
                 mouseState == (MouseState_Visible | MouseState_InWindow));    \
    if (info.showUi) {                                                         \
        const SDL_Rect rect = { .x = 0, .y = 0, .w = w, .h = h };              \
        if (SDL_SetRenderDrawBlendMode(info.renderer, SDL_BLENDMODE_BLEND) !=  \
              0 ||                                                             \
            SDL_SetRenderDrawColor(info.renderer, 0u, 0u, 0u, 96u) != 0 ||     \
            SDL_RenderFillRect(info.renderer, &rect) != 0) {                   \
            fprintf(stderr, "Failed to render: %s\n", SDL_GetError());         \
        }                                                                      \
        renderUiActors(&info);                                                 \
    }                                                                          \
    SDL_RenderPresent(info.renderer);

#define DEFAULT_CHAT_COUNT 5

SDL_mutex* clientMutex = NULL;
ClientData clientData = { 0 };
UserInputList userInputs = { 0 };
AudioStatePtrList audioStates = { 0 };
SDL_atomic_t continueSelection = { 0 };

int
displayError(SDL_Window* window, const char* e, const bool force);

size_t
encodeAudioData(OpusEncoder* encoder,
                const Bytes* audio,
                const SDL_AudioSpec spec,
                pNullValueList);

bool
startPlaybackFromName(const char*, pAudioState);

bool
selectAudioStreamSource(struct pollfd inputfd,
                        pBytes bytes,
                        pAudioState state,
                        pUserInput);

void
consumeAudio(const Bytes*, const Guid*);

bool
askAndStartRecording(struct pollfd inputfd, pBytes bytes, pAudioState);

bool
startPlayback(struct pollfd inputfd,
              pBytes bytes,
              pAudioState state,
              const bool);

void
handleUserInput(const struct pollfd, const UserInput*, pBytes);

bool
clientHandleLobbyMessage(const LobbyMessage* message, pClient client);

bool
clientHandleGeneralMessage(const GeneralMessage* message, pClient client);

void
updateStreamDisplay(const Guid* id)
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_UpdateStreamDisplay;
    pGuid guid = (pGuid)currentAllocator->allocate(sizeof(Guid));
    *guid = *id;
    e.user.data1 = guid;
    SDL_PushEvent(&e);
}

void
updateCurrentAudioDisplay(pCurrentAudio ptr)
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_UpdateAudioDisplay;
    e.user.data1 = ptr;
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

bool
askYesOrNoQuestion(const char* q, const struct pollfd inputfd, pBytes bytes);

ClientConfiguration
defaultClientConfiguration()
{
    return (ClientConfiguration){
        .fullscreen = false,
        .windowWidth = 800,
        .windowHeight = 600,
        .silenceThreshold = 0.f,
        .fontSize = 48,
        .noGui = false,
        .noTui = false,
        .showLabel = true,
        .noAudio = false,
        .authentication = { .type = 0,
                            .value =
                              TemLangStringCreate("", currentAllocator) },
        .talkMode = { .none = NULL, .tag = TalkModeTag_none },
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
    for (int i = 1; i < argc - 1; i += 2) {
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
        STR_EQUALS(key, "-CT", keyLen, { goto parseCredentialType; });
        STR_EQUALS(
          key, "--credential-type", keyLen, { goto parseCredentialType; });
        STR_EQUALS(key, "-C", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "--credentials", keyLen, { goto parseCredentials; });
        STR_EQUALS(key, "-NG", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "--no-gui", keyLen, { goto parseNoGui; });
        STR_EQUALS(key, "-NT", keyLen, { goto parseNoTui; });
        STR_EQUALS(key, "--no-tui", keyLen, { goto parseNoTui; });
        STR_EQUALS(key, "-NA", keyLen, { goto parseNoAudio; });
        STR_EQUALS(key, "--no-audio", keyLen, { goto parseNoAudio; });
        STR_EQUALS(key, "-ST", keyLen, { goto parseSilence; });
        STR_EQUALS(key, "--silence-threshold", keyLen, { goto parseSilence; });
        STR_EQUALS(key, "-SL", keyLen, { goto parseLabel; });
        STR_EQUALS(key, "--show-label", keyLen, { goto parseLabel; });
        STR_EQUALS(key, "--press-to-talk", keyLen, {
            TalkModeFree(&client->talkMode);
            client->talkMode.tag = TalkModeTag_pressToTalk;
            client->talkMode.pressToTalk =
              TemLangStringCreate(value, currentAllocator);
            continue;
        });
        STR_EQUALS(key, "--press-to-mute", keyLen, {
            TalkModeFree(&client->talkMode);
            client->talkMode.tag = TalkModeTag_pressToMute;
            client->talkMode.pressToMute =
              TemLangStringCreate(value, currentAllocator);
            continue;
        });
        // TODO: parse authentication
        if (!parseCommonConfiguration(key, value, configuration)) {
            parseFailure("Client", key, value);
            return false;
        }
        continue;
    parseTTF : {
        TemLangStringFree(&client->ttfFile);
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
    parseCredentialType : {
        client->authentication.type = atoi(value);
        continue;
    }
    parseCredentials : {
        TemLangStringFree(&client->authentication.value);
        client->authentication.value =
          TemLangStringCreate(value, currentAllocator);
        continue;
    }
    parseNoGui : {
        client->noGui = atoi(value);
        continue;
    }
    parseNoTui : {
        client->noTui = atoi(value);
        continue;
    }
    parseNoAudio : {
        client->noAudio = atoi(value);
        continue;
    }
    parseLabel : {
        client->showLabel = atoi(value);
        continue;
    }
    parseSilence : {
        client->silenceThreshold = atof(value);
        continue;
    }
    }
    return true;
}

int
printTalkMode(const TalkMode* mode)
{
    int offset = printf("Talk Mode: ");
    switch (mode->tag) {
        case TalkModeTag_pressToMute:
            offset += printf("Press to mute=%s\n", mode->pressToMute.buffer);
            break;
        case TalkModeTag_pressToTalk:
            offset += printf("Press to talk=%s\n", mode->pressToTalk.buffer);
            break;
        default:
            offset += puts("None");
            break;
    }
    return offset;
}

int
printClientConfiguration(const ClientConfiguration* configuration)
{
    return printf(
             "Width: %d\nHeight: %d\nFullscreen: %d\nTTF file: %s\nFont Size: "
             "%d\nNo Gui: %d\nNo Tui: %d\nShow label: %d\nSilence Threshold: "
             "%f\n",
             configuration->windowWidth,
             configuration->windowHeight,
             configuration->fullscreen,
             configuration->ttfFile.buffer,
             configuration->fontSize,
             configuration->noGui,
             configuration->noTui,
             configuration->showLabel,
             configuration->silenceThreshold) +
           printTalkMode(&configuration->talkMode) +
           printAuthentication(&configuration->authentication);
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

UserInputResult
getUserInput(struct pollfd inputfd, pBytes bytes)
{
    switch (poll(&inputfd, 1, CLIENT_POLL_WAIT)) {
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

    ssize_t size = read(inputfd.fd, bytes->buffer, bytes->size);
    if (size == 0) {
        return UserInputResult_NoInput;
    }
    if (size < 0) {
        perror("read");
        return UserInputResult_Error;
    }
    // Remove new line character
    --size;
    bytes->buffer[size] = '\0';
    bytes->used = size;
    return UserInputResult_Input;
}

UserInputResult
getStringFromUser(struct pollfd inputfd, pBytes bytes, const bool keepPolling)
{
    uint8_tListRellocateIfNeeded(bytes);

    UserInputResult result = UserInputResult_Error;
    SDL_AtomicSet(&continueSelection, 1);
    while (!appDone && SDL_AtomicGet(&continueSelection) == 1) {
        switch (getUserInput(inputfd, bytes)) {
            case UserInputResult_Error:
                result = UserInputResult_Error;
                goto end;
            case UserInputResult_NoInput:
                if (keepPolling) {
                    continue;
                } else {
                    result = UserInputResult_NoInput;
                    goto end;
                }
                break;
            case UserInputResult_Input:
                result = UserInputResult_Input;
                goto end;
            default:
                break;
        }
    }
end:
    return result;
}

UserInputResult
getIndexFromUser(struct pollfd inputfd,
                 pBytes bytes,
                 const uint32_t max,
                 uint32_t* index,
                 const bool keepPolling)
{
    UserInputResult result = UserInputResult_Error;
    if (max < 2) {
        *index = 0;
        result = UserInputResult_Input;
        goto end;
    }

    switch (getStringFromUser(inputfd, bytes, keepPolling)) {
        case UserInputResult_Error:
            result = UserInputResult_Error;
            goto end;
        case UserInputResult_NoInput:
            result = UserInputResult_NoInput;
            goto end;
        default:
            break;
    }
    char* end = NULL;
    *index = (uint32_t)strtoul((const char*)bytes->buffer, &end, 10) - 1UL;
    if (end != (char*)&bytes->buffer[bytes->used] || *index >= max) {
        printf("Enter a number between 1 and %u\n", max);
        result = UserInputResult_Error;
        goto end;
    }
    result = UserInputResult_Input;
    goto end;

end:
    return result;
}

void
sendAuthentication(ENetPeer* peer, const ServerConfigurationDataTag type)
{
    uint8_t buffer[KB(2)] = { 0 };
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = buffer,
                    .used = 0,
                    .size = sizeof(buffer) };
    const Authentication authentication =
      ((const ClientConfiguration*)clientData.configuration)->authentication;
    // printAuthentication(&authentication);
    switch (type) {
        case ServerConfigurationDataTag_lobby: {
            LobbyMessage message = { 0 };
            message.tag = LobbyMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(LobbyMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_chat: {
            ChatMessage message = { 0 };
            message.tag = ChatMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(ChatMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_text: {
            TextMessage message = { 0 };
            message.tag = TextMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(TextMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_audio: {
            AudioMessage message = { 0 };
            message.tag = AudioMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(AudioMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_image: {
            ImageMessage message = { 0 };
            message.tag = ImageMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(ImageMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_video: {
            VideoMessage message = { 0 };
            message.tag = VideoMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(VideoMessage, message, bytes);
        } break;
        case ServerConfigurationDataTag_replay: {
            ReplayMessage message = { 0 };
            message.tag = ReplayMessageTag_general;
            message.general.tag = GeneralMessageTag_authenticate;
            message.general.authenticate = authentication;
            MESSAGE_SERIALIZE(ReplayMessage, message, bytes);
        } break;
        default:
            fprintf(stderr,
                    "Failed to send authentication to server: %s\n",
                    ServerConfigurationDataTagToCharString(type));
            break;
    }

    sendBytes(peer, 1, CLIENT_CHANNEL, &bytes, SendFlags_Normal);
}

bool
clientHandleTextMessage(const TextMessage* message, pStreamDisplay display)
{
    bool success = false;
    switch (message->tag) {
        case TextMessageTag_text: {
            StreamDisplayDataFree(&display->data);
            display->data.tag = StreamDisplayDataTag_text;
            success = TemLangStringCopy(
              &display->data.text, &message->text, currentAllocator);
            printf("Got text message: '%s'\n", message->text.buffer);
            updateStreamDisplay(&display->id);
        } break;
        case TextMessageTag_general:
            success =
              clientHandleGeneralMessage(&message->general, &display->client);
            break;
        default:
            printf("Unexpected text message: %s\n",
                   TextMessageTagToCharString(message->tag));
            break;
    }
    return success;
}

bool
clientHandleTextMessageFromBytes(const Bytes* bytes, pStreamDisplay display)
{
    TextMessage message = { 0 };
    MESSAGE_DESERIALIZE(TextMessage, message, (*bytes));
    const bool result = clientHandleTextMessage(&message, display);
    TextMessageFree(&message);
    return result;
}

bool
clientHandleChatMessage(const ChatMessage* message, pStreamDisplay display)
{
    bool success = false;
    switch (message->tag) {
        case ChatMessageTag_logs: {
            StreamDisplayDataFree(&display->data);
            display->data.tag = StreamDisplayDataTag_chat;
            pStreamDisplayChat chat = &display->data.chat;
            chat->count = DEFAULT_CHAT_COUNT;
            success =
              ChatListCopy(&chat->logs, &message->logs, currentAllocator);
            chat->offset = chat->logs.used;
            updateStreamDisplay(&display->id);
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
            success = ChatListAppend(&chat->logs, &message->newChat);
            if (chat->offset >= chat->logs.used - chat->count) {
                chat->offset = chat->logs.used;
            }
            updateStreamDisplay(&display->id);
        } break;
        case ChatMessageTag_general:
            success =
              clientHandleGeneralMessage(&message->general, &display->client);
            break;
        default:
            printf("Unexpected chat message: %s\n",
                   ChatMessageTagToCharString(message->tag));
            break;
    }
    return success;
}

bool
clientHandleChatMessageFromBytes(const Bytes* bytes, pStreamDisplay display)
{
    ChatMessage message = { 0 };
    MESSAGE_DESERIALIZE(ChatMessage, message, (*bytes));
    const bool result = clientHandleChatMessage(&message, display);
    ChatMessageFree(&message);
    return result;
}

bool
clientHandleImageMessage(const ImageMessage* message, pStreamDisplay display)
{
    bool success = false;
    switch (message->tag) {
        case ImageMessageTag_imageStart:
            StreamDisplayDataFree(&display->data);
            display->data.tag = StreamDisplayDataTag_image;
            display->data.image.allocator = currentAllocator;
            success = true;
            break;
        case ImageMessageTag_imageChunk:
            if (display->data.tag != StreamDisplayDataTag_image) {
                StreamDisplayDataFree(&display->data);
                display->data.tag = StreamDisplayDataTag_image;
                display->data.image.allocator = currentAllocator;
            }
            success = uint8_tListQuickAppend(&display->data.image,
                                             message->imageChunk.buffer,
                                             message->imageChunk.used);
            break;
        case ImageMessageTag_imageEnd:
            success = true;
            updateStreamDisplay(&display->id);
            break;
        case ImageMessageTag_general:
            success =
              clientHandleGeneralMessage(&message->general, &display->client);
            break;
        default:
            printf("Unexpected image message: %s\n",
                   ImageMessageTagToCharString(message->tag));
            break;
    }
    return success;
}

bool
clientHandleImageMessageFromBytes(const Bytes* bytes, pStreamDisplay display)
{
    ImageMessage message = { 0 };
    MESSAGE_DESERIALIZE(ImageMessage, message, (*bytes));
    const bool result = clientHandleImageMessage(&message, display);
    ImageMessageFree(&message);
    return result;
}

bool
clientHandleAudioMessage(const AudioMessage* message,
                         pStreamDisplay display,
                         pAudioState playback,
                         const Guid* id,
                         const bool isRecording)
{
    bool success = false;
    switch (message->tag) {
        case AudioMessageTag_general: {
            if (clientData.window != NULL) {
                Client client = { 0 };
                success =
                  clientHandleGeneralMessage(&message->general, &client);
                ClientFree(&client);
                if (!success) {
                    break;
                }
                if (message->general.tag == GeneralMessageTag_authenticateAck &&
                    playback == NULL && !isRecording) {
                    playback = currentAllocator->allocate(sizeof(AudioState));
                    playback->storedAudio = CQueueCreate(CQUEUE_SIZE);
                    playback->volume = 1.f;
                    playback->id = *id;
                    success = startPlaybackFromName(NULL, playback);
                    if (success) {
                        SDL_PauseAudioDevice(playback->deviceId, SDL_FALSE);
                        IN_MUTEX(clientData.mutex, endPlayback, {
                            AudioStatePtrListAppend(&audioStates, &playback);
                        });
                        sendUpdateUiEvent();
                    } else {
                        AudioStateFree(playback);
                        currentAllocator->free(playback);
                    }
                } else {
                    success = true;
                }
                break;
            }
            UserInput input = { 0 };
            input.id = display->id;
            input.data.tag = UserInputDataTag_queryAudio;
            display->choosingPlayback = true;
            success = clientHandleGeneralMessage(&message->general,
                                                 &input.data.queryAudio.client);
            if (success &&
                message->general.tag == GeneralMessageTag_authenticateAck) {
                input.data.queryAudio.writeAccess = clientHasWriteAccess(
                  &input.data.queryAudio.client, &display->config);
                input.data.queryAudio.readAccess = clientHasReadAccess(
                  &input.data.queryAudio.client, &display->config);
                success = UserInputListAppend(&userInputs, &input);
            }
            UserInputFree(&input);
        } break;
        case AudioMessageTag_audio:
            if (playback == NULL || playback->decoder == NULL) {
                if (isRecording || display->choosingPlayback) {
                    success = true;
                } else {
                    fprintf(stderr,
                            "No playback device assigned to this "
                            "stream. Disconnecting from audio server...\n");
                    success = false;
                }
                break;
            }
            void* data = NULL;
            int byteSize = 0;
            if (decodeOpus(playback, &message->audio, &data, &byteSize)) {
#if USE_AUDIO_CALLBACKS
                SDL_LockAudioDevice(playback->deviceId);
                CQueueEnqueue(&playback->storedAudio, data, byteSize);
                success = true;
                SDL_UnlockAudioDevice(playback->deviceId);
#else
                success =
                  SDL_QueueAudio(playback->deviceId, data, byteSize) == 0;
#endif
            } else {
                // Bad frames can be recovered ?
                success = true;
            }
            currentAllocator->free(data);
            break;
        default:
            printf("Unexpected audio message: %s\n",
                   AudioMessageTagToCharString(message->tag));
            break;
    }
    return success;
}

bool
clientHandleAudioMessageFromBytes(const Bytes* packetBytes,
                                  pStreamDisplay display,
                                  pAudioState playback,
                                  const Guid* id,
                                  const bool isRecording)
{
    AudioMessage message = { 0 };
    MESSAGE_DESERIALIZE(AudioMessage, message, (*packetBytes));
    const bool result =
      clientHandleAudioMessage(&message, display, playback, id, isRecording);
    AudioMessageFree(&message);
    return result;
}

bool
clientHandleVideoMessage(const VideoMessage* message,
                         pStreamDisplay display,
                         pVideoDecoder codec,
                         uint64_t* lastError)
{
    bool success = true;
    switch (message->tag) {
        case VideoMessageTag_general: {
            UserInput input = { 0 };
            input.id = display->id;
            input.data.tag = UserInputDataTag_queryVideo;
            display->choosingPlayback = true;
            success = clientHandleGeneralMessage(&message->general,
                                                 &input.data.queryVideo.client);
            if (success &&
                message->general.tag == GeneralMessageTag_authenticateAck) {
                input.data.queryVideo.writeAccess = clientHasWriteAccess(
                  &input.data.queryVideo.client, &display->config);
                input.data.queryVideo.readAccess = clientHasReadAccess(
                  &input.data.queryVideo.client, &display->config);
                success = UserInputListAppend(&userInputs, &input);
            }
            UserInputFree(&input);
        } break;
        case VideoMessageTag_size: {
            pVideoFrame m = currentAllocator->allocate(sizeof(VideoFrame));
            m->id = display->id;
            m->width = message->size[0];
            m->height = message->size[1];
            m->video.allocator = currentAllocator;
            printf("Video size set to %dx%d\n", m->width, m->height);
            SDL_Event e = { 0 };
            e.type = SDL_USEREVENT;
            e.user.code = CustomEvent_UpdateVideoDisplay;
            e.user.data1 = m;
            SDL_PushEvent(&e);
        } break;
        case VideoMessageTag_video: {
            if (lowMemory()) {
                const uint64_t now = SDL_GetTicks64();
                if (now - *lastError > 1000) {
                    fprintf(stderr, "Low memory. Not decoding video...\n");
                }
                *lastError = now;
                break;
            }
            VideoDecoderDecode(codec, &message->video, &display->id, lastError);
        } break;
        default:
            printf("Unexpected video message: %s\n",
                   VideoMessageTagToCharString(message->tag));
            success = false;
            break;
    }
    return success;
}

bool
clientHandleVideoMessageFromBytes(const Bytes* packetBytes,
                                  pStreamDisplay display,
                                  pVideoDecoder codec,
                                  uint64_t* lastError)
{
    VideoMessage message = { 0 };
    MESSAGE_DESERIALIZE(VideoMessage, message, (*packetBytes));
    const bool result =
      clientHandleVideoMessage(&message, display, codec, lastError);
    VideoMessageFree(&message);
    return result;
}

bool
clientHandleServerMessage(const ServerMessage* m,
                          pStreamDisplay display,
                          pAudioState playback,
                          const bool isRecording,
                          pVideoDecoder codec,
                          uint64_t* lastError)
{
    switch (m->tag) {
        case ServerMessageTag_Text:
            return clientHandleTextMessage(&m->Text, display);
        case ServerMessageTag_Chat:
            return clientHandleChatMessage(&m->Chat, display);
        case ServerMessageTag_Image:
            return clientHandleImageMessage(&m->Image, display);
        case ServerMessageTag_Audio:
            return clientHandleAudioMessage(
              &m->Audio, display, playback, &display->id, isRecording);
        case ServerMessageTag_Video:
            return clientHandleVideoMessage(
              &m->Video, display, codec, lastError);
        default:
            break;
    }
    return false;
}

bool
clientHandleReplayMessage(const ReplayMessage* replay,
                          pStreamDisplay display,
                          pAudioState playback,
                          const bool isRecording,
                          pVideoDecoder codec,
                          uint64_t* lastError)
{
    bool success = false;
    switch (replay->tag) {
        case ReplayMessageTag_general:
            success =
              clientHandleGeneralMessage(&replay->general, &display->client);
            break;
        case ReplayMessageTag_timeRange:
            display->data.tag = StreamDisplayDataTag_timeRange;
            memcpy(&display->data.timeRange,
                   replay->timeRange,
                   sizeof(replay->timeRange));
            success = true;
            break;
        case ReplayMessageTag_response:
            success = clientHandleServerMessage(&replay->response,
                                                display,
                                                playback,
                                                isRecording,
                                                codec,
                                                lastError);
            break;
        default:
            fprintf(stderr,
                    "Unexpected replay message: %s(%d)\n",
                    ReplayMessageTagToCharString(replay->tag),
                    replay->tag);
            break;
    }
    return success;
}

bool
clientHandleReplayMessageFromBytes(const Bytes* bytes,
                                   pStreamDisplay display,
                                   pAudioState playback,
                                   const bool isRecording,
                                   pVideoDecoder codec,
                                   uint64_t* lastError)
{
    ReplayMessage replay = { 0 };
    MESSAGE_DESERIALIZE(ReplayMessage, replay, (*bytes));
    const bool result = clientHandleReplayMessage(
      &replay, display, playback, isRecording, codec, lastError);
    ReplayMessageFree(&replay);
    return result;
}

int
VerifyMemory(ENetHost* host, ENetEvent* e)
{
    (void)e;
    (void)host;
    if (lowMemory()) {
#if _DEBUG
        puts("Dropping packet");
#endif
        return 1;
    }
    return 0;
}

int
replayRequestConnectionThread(void* ptr)
{
    const Guid* id = (const Guid*)ptr;

    uint8_t buffer[KB(4)];
    Bytes bytes = {
        .buffer = buffer, .size = sizeof(buffer), .used = 0, .allocator = NULL
    };
    ReplayMessage message = { .tag = ReplayMessageTag_request, .request = 0 };
    bool displayMissing = false;
    while (!appDone && !displayMissing) {
        SDL_Delay(1000U);
        USE_DISPLAY(clientData.mutex, fend, displayMissing, {
            if (display->data.tag != StreamDisplayDataTag_timeRange) {
                goto fend;
            }
            const int64_t newValue = SDL_clamp(message.request,
                                               display->data.timeRange[0],
                                               display->data.timeRange[1]);
            if (message.request == newValue) {
                ++message.request;
            } else {
                message.request = newValue;
            }
            MESSAGE_SERIALIZE(ReplayMessage, message, bytes);
            ENetPacket* packet =
              BytesToPacket(bytes.buffer, bytes.used, SendFlags_Normal);
            if (!NullValueListAppend(&display->outgoing, (NullValue)&packet)) {
                enet_packet_destroy(packet);
            }
        });
    }
    ReplayMessageFree(&message);
    return EXIT_SUCCESS;
}

int
streamConnectionThread(void* ptr)
{
    const Guid* id = (const Guid*)ptr;

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    SDL_Thread* replayThread = NULL;
    NullValueList incomingPackets = { .allocator = currentAllocator };
    NullValueList outgoingPackets = { .allocator = currentAllocator };
    Bytes bytes = { .allocator = currentAllocator,
                    .used = 0,
                    .size = MAX_PACKET_SIZE,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE) };
    int result = EXIT_FAILURE;
    bool displayMissing = false;
    ServerConfigurationDataTag tag = { 0 };
    USE_DISPLAY(clientData.mutex, fend, displayMissing, {
        tag = config->data.tag;

        host = enet_host_create(NULL, 1, 2, 0, 0);
        if (host == NULL) {
            fprintf(stderr, "Failed to create client host\n");
            goto fend;
        }
        {
            ENetAddress address = { 0 };
            enet_address_set_host(&address, config->hostname.buffer);
            address.port = config->port;
            peer = enet_host_connect(host, &address, 2, config->data.tag);
            char buffer[512] = { 0 };
            enet_address_get_host_ip(&address, buffer, sizeof(buffer));
            printf("Connecting to server: %s:%u...\n", buffer, address.port);
        }
        if (peer == NULL) {
            fprintf(stderr, "Failed to connect to server\n");
            displayError(NULL, "Failed to connect to server", false);
            goto fend;
        }
    });
    if (displayMissing || host == NULL || peer == NULL) {
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
            display->mtu = peer->mtu;
            sendAuthentication(peer, config->data.tag);
        });
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        goto end;
    }

    // If not UI, use default audio playback device for audio streams
    // For video connections
    uint64_t lastVideoError = 0;
    VideoDecoder decoder = { 0 };
    switch (tag) {
        case ServerConfigurationDataTag_video:
        case ServerConfigurationDataTag_replay:
            if (!VideoDecoderInit(&decoder)) {
                goto end;
            }
            break;
        default:
            break;
    }

    // Start audio for replay connections
    switch (tag) {
        case ServerConfigurationDataTag_replay: {
            pAudioState playback =
              currentAllocator->allocate(sizeof(AudioState));
            playback->storedAudio = CQueueCreate(CQUEUE_SIZE);
            playback->volume = 1.f;
            playback->id = *id;
            if (startPlaybackFromName(NULL, playback)) {
                SDL_PauseAudioDevice(playback->deviceId, SDL_FALSE);
                IN_MUTEX(clientData.mutex, endPlayback, {
                    AudioStatePtrListAppend(&audioStates, &playback);
                });
            } else {
                AudioStateFree(playback);
                currentAllocator->free(playback);
                fprintf(stderr, "Failed to start audio for replay stream\n");
            }
            replayThread = SDL_CreateThread(
              (SDL_ThreadFunction)replayRequestConnectionThread, "replay", ptr);
            if (replayThread == NULL) {
                fprintf(stderr, "Failed to start thread: %s\n", SDL_GetError());
                goto end;
            }
            puts("Started replay request thread");
        } break;
        default:
            break;
    }

    host->intercept = VerifyMemory;

    while (!appDone && !displayMissing) {
        USE_DISPLAY(clientData.mutex, endPakce, displayMissing, {
            uint32_t i = 0;
            for (; i < display->outgoing.used && !lowMemory(); ++i) {
                NullValue packet = display->outgoing.buffer[i];
                PEER_SEND(peer, CLIENT_CHANNEL, packet);
            }

            if (i < display->outgoing.used) {
                fprintf(stderr,
                        "Bandwith issue. Dropping %u outgoing packets\n",
                        display->outgoing.used - i);
                for (; i < display->outgoing.used; ++i) {
                    NullValue packet = display->outgoing.buffer[i];
                    enet_packet_destroy(packet);
                }
                NullValueListFree(&display->outgoing);
                display->outgoing.allocator = currentAllocator;
            } else {
                display->outgoing.used = 0;
            }
        });
        while (incomingPackets.used < MAX_PACKETS &&
               enet_host_service(host, &event, 0U) > 0) {
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
                    NullValueListAppend(&incomingPackets,
                                        (NullValue*)&event.packet);
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
                default:
                    break;
            }
        }

        pAudioState record = NULL;
        IN_MUTEX(clientData.mutex, endRecord, {
            size_t listIndex = 0;
            if (AudioStateFromGuid(&audioStates, id, true, NULL, &listIndex)) {
                record = audioStates.buffer[listIndex];
            }
        });

        if (record != NULL && record->encoder != NULL) {
#if USE_AUDIO_CALLBACKS
            SDL_LockAudioDevice(record->deviceId);
            const size_t queueSize = CQueueCount(&record->storedAudio);
            Bytes audioBytes = { .allocator = currentAllocator,
                                 .buffer =
                                   currentAllocator->allocate(queueSize),
                                 .size = queueSize,
                                 .used = 0 };
            audioBytes.used = CQueueDequeue(
              &record->storedAudio, audioBytes.buffer, audioBytes.size, false);
            const size_t bytesRead = encodeAudioData(
              record->encoder, &audioBytes, record->spec, &outgoingPackets);
            CQueueEnqueue(&record->storedAudio,
                          audioBytes.buffer + bytesRead,
                          audioBytes.used - bytesRead);
            uint8_tListFree(&audioBytes);
            SDL_UnlockAudioDevice(record->deviceId);
#else
            const int result =
              SDL_DequeueAudio(record->deviceId, bytes.buffer, bytes.size);
            if (result < 0) {
                fprintf(
                  stderr, "Failed to dequeue audio: %s\n", SDL_GetError());
            } else {
                CQueueEnqueue(&record->storedAudio, bytes.buffer, result);
                const size_t queueSize = CQueueCount(&record->storedAudio);
                if (queueSize != 0) {
                    Bytes audioBytes = { .allocator = currentAllocator,
                                         .buffer = currentAllocator->allocate(
                                           queueSize),
                                         .size = queueSize,
                                         .used = 0 };
                    audioBytes.used = CQueueDequeue(&record->storedAudio,
                                                    audioBytes.buffer,
                                                    audioBytes.size,
                                                    false);
                    const size_t bytesRead = encodeAudioData(record->encoder,
                                                             &audioBytes,
                                                             record->spec,
                                                             &outgoingPackets);
                    CQueueEnqueue(&record->storedAudio,
                                  audioBytes.buffer + bytesRead,
                                  audioBytes.used - bytesRead);
                    uint8_tListFree(&audioBytes);
                }
            }
#endif
        }
        for (size_t i = 0; i < outgoingPackets.used; ++i) {
            NullValue packet = outgoingPackets.buffer[i];
            PEER_SEND(peer, CLIENT_CHANNEL, packet);
        }
        outgoingPackets.used = 0;
        if (incomingPackets.used == 0) {
            SDL_Delay(1U);
            continue;
        }
        USE_DISPLAY(clientData.mutex, fend563, displayMissing, {
            pAudioState playback = NULL;
            {
                size_t listIndex = 0;
                if (AudioStateFromGuid(
                      &audioStates, id, false, NULL, &listIndex)) {
                    playback = audioStates.buffer[listIndex];
                }
            }
            for (size_t i = 0; i < incomingPackets.used; ++i) {
                ENetPacket* packet = (ENetPacket*)incomingPackets.buffer[i];
                Bytes packetBytes = { 0 };
                packetBytes.allocator = currentAllocator;
                packetBytes.buffer = packet->data;
                packetBytes.size = packet->dataLength;
                packetBytes.used = packet->dataLength;
                bool success = false;
                switch (config->data.tag) {
                    case ServerConfigurationDataTag_text:
                        success = clientHandleTextMessageFromBytes(&packetBytes,
                                                                   display);
                        break;
                    case ServerConfigurationDataTag_chat:
                        success = clientHandleChatMessageFromBytes(&packetBytes,
                                                                   display);
                        break;
                    case ServerConfigurationDataTag_image:
                        success = clientHandleImageMessageFromBytes(
                          &packetBytes, display);
                        break;
                    case ServerConfigurationDataTag_audio:
                        success = clientHandleAudioMessageFromBytes(
                          &packetBytes,
                          display,
                          playback,
                          id,
                          display->recordings != 0 || record != NULL);
                        break;
                    case ServerConfigurationDataTag_video:
                        success = clientHandleVideoMessageFromBytes(
                          &packetBytes, display, &decoder, &lastVideoError);
                        break;
                    case ServerConfigurationDataTag_replay:
                        success = clientHandleReplayMessageFromBytes(
                          &packetBytes,
                          display,
                          playback,
                          display->recordings != 0 || record != NULL,
                          &decoder,
                          &lastVideoError);
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
                    displayMissing = true;
                }
                enet_packet_destroy(packet);
            }
            incomingPackets.used = 0;
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
        if (AudioStateFromGuid(&audioStates, id, true, NULL, &i)) {
            AudioStateFree(audioStates.buffer[i]);
            currentAllocator->free(audioStates.buffer[i]);
            AudioStatePtrListSwapRemove(&audioStates, i);
        }
        if (AudioStateFromGuid(&audioStates, id, false, NULL, &i)) {
            AudioStateFree(audioStates.buffer[i]);
            currentAllocator->free(audioStates.buffer[i]);
            AudioStatePtrListSwapRemove(&audioStates, i);
        }
    });
    SDL_WaitThread(replayThread, NULL);
    for (size_t i = 0; i < incomingPackets.used; ++i) {
        enet_packet_destroy(incomingPackets.buffer[i]);
    }
    NullValueListFree(&incomingPackets);
    for (size_t i = 0; i < outgoingPackets.used; ++i) {
        enet_packet_destroy(outgoingPackets.buffer[i]);
    }
    VideoDecoderFree(&decoder);
    NullValueListFree(&outgoingPackets);
    uint8_tListFree(&bytes);
    currentAllocator->free(ptr);
    closeHostAndPeer(host, peer);
    SDL_AtomicDecRef(&runningThreads);
    return result;
}

bool
connectToStream(const ServerConfiguration* config, pRandomState rs)
{
    StreamDisplay display = { 0 };
    display.visible = true;
    display.outgoing.allocator = currentAllocator;
    if (rs == NULL) {
        RandomState frs = makeRandomState();
        display.id = randomGuid(&frs);
    } else {
        display.id = randomGuid(rs);
    }
    pGuid id = currentAllocator->allocate(sizeof(Guid));
    *id = display.id;
    ServerConfigurationCopy(&display.config, config, currentAllocator);
    IN_MUTEX(clientData.mutex, end3, {
        StreamDisplayListAppend(&clientData.displays, &display);
    });
    StreamDisplayFree(&display);

    SDL_Thread* thread = SDL_CreateThread(
      (SDL_ThreadFunction)streamConnectionThread, "stream", id);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        currentAllocator->free(id);
        IN_MUTEX(clientData.mutex, end4, {
            StreamDisplayListPop(&clientData.displays);
        });
        return false;
    } else {
        SDL_AtomicIncRef(&runningThreads);
        SDL_DetachThread(thread);
        return true;
    }
}

void
selectAStreamToConnectTo(struct pollfd inputfd, pBytes bytes, pRandomState rs)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    IN_MUTEX(clientData.mutex, end2, {
        ServerConfigurationListCopy(
          &list, &clientData.allStreams, currentAllocator);
    });

    if (ServerConfigurationListIsEmpty(&list)) {
        puts("No streams to connect to");
        goto end;
    }

    askQuestion("Select a stream to connect to");
    for (uint32_t i = 0; i < list.used; ++i) {
        const ServerConfiguration* config = &list.buffer[i];
        printf("%u) ", i + 1U);
        printServerConfigurationForClient(config);
    }
    puts("");

    uint32_t i = 0;
    if (list.used == 1U) {
        puts("Connecting to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list.used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling connecting to stream");
        goto end;
    }

    bool exists;
    IN_MUTEX(clientData.mutex, end5, {
        exists = GetStreamDisplayFromName(
          &clientData.displays, &list.buffer[i].name, NULL, NULL);
    });

    if (exists) {
        puts("Already connected to stream");
        goto end;
    }

    connectToStream(&list.buffer[i], rs);

end:
    ServerConfigurationListFree(&list);
}

void
selectAStreamToDisconnectFrom(struct pollfd inputfd, pBytes bytes)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    IN_MUTEX(clientData.mutex, end2, {
        for (size_t i = 0; i < clientData.displays.used; ++i) {
            ServerConfigurationListAppend(
              &list, &clientData.displays.buffer[i].config);
        }
    });

    if (ServerConfigurationListIsEmpty(&list)) {
        puts("Not connected to any streams");
        goto end;
    }

    askQuestion("Select a stream to disconnect from");
    for (uint32_t i = 0; i < list.used; ++i) {
        printf("%u) ", i + 1U);
        printServerConfigurationForClient(&list.buffer[i]);
    }
    puts("");

    uint32_t i = 0;
    if (list.used == 1) {
        puts("Disconnecting from only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list.used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceled disconnecting from stream");
        goto end;
    }

    printf("Disconnecting from %s(%s)...\n",
           list.buffer[i].name.buffer,
           ServerConfigurationDataTagToCharString(list.buffer[i].data.tag));

    const ServerConfiguration* config = &list.buffer[i];
    IN_MUTEX(clientData.mutex, end4, {
        size_t listIndex = 0;
        if (GetStreamDisplayFromName(
              &clientData.displays, &config->name, NULL, &listIndex)) {
            StreamDisplayListSwapRemove(&clientData.displays, listIndex);
        } else {
            fprintf(
              stderr, "Stream (%s) doesn't exists!\n", config->name.buffer);
        }
    })

end:
    ServerConfigurationListFree(&list);
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

size_t
encodeAudioData(OpusEncoder* encoder,
                const Bytes* audio,
                const SDL_AudioSpec spec,
                pNullValueList list)
{
    uint8_t buffer[KB(4)] = { 0 };

    const int minDuration =
      audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_10_MS);

    AudioMessage message = { 0 };
    message.tag = AudioMessageTag_audio;
    message.audio.allocator = currentAllocator;
    message.audio.buffer = buffer;
    message.audio.size = sizeof(buffer);
    message.audio.used = 0;
    Bytes temp = { .allocator = currentAllocator };
    size_t bytesRead = 0;
    while (bytesRead < audio->used) {
        int frame_size = (audio->used - bytesRead) / (spec.channels * PCM_SIZE);

        if (frame_size < minDuration) {
            break;
        }

        // Only certain durations are valid for encoding
        frame_size = closestValidFrameCount(spec.freq, frame_size);

        const int result =
#if HIGH_QUALITY_AUDIO
          opus_encode_float(encoder,
                            (float*)(audio->buffer + bytesRead),
                            frame_size,
                            message.audio.buffer,
                            message.audio.size);
#else
          opus_encode(encoder,
                      (opus_int16*)(audio->buffer + bytesRead),
                      frame_size,
                      message.audio.buffer,
                      message.audio.size);
#endif
        if (result < 0) {
            fprintf(stderr,
                    "Failed to encode audio packet: %s; Frame "
                    "size %d\n",
                    opus_strerror(result),
                    frame_size);
            break;
        }

        message.audio.used = (uint32_t)result;

        const size_t bytesUsed = (frame_size * spec.channels * PCM_SIZE);
        // printf("Bytes encoded: %zu -> %d\n", bytesUsed,
        // result);
        bytesRead += bytesUsed;

        MESSAGE_SERIALIZE(AudioMessage, message, temp);
        NullValue packet =
          BytesToPacket(temp.buffer, temp.used, SendFlags_Audio);
        NullValueListAppend(list, &packet);
    }
    uint8_tListFree(&temp);
    return bytesRead;
}

bool
selectVideoStreamSource(struct pollfd inputfd,
                        pBytes bytes,
                        const UserInput* ui)
{
    if (!askYesOrNoQuestion(
          "Do you want record video and stream it?", inputfd, bytes)) {
        return false;
    }

    askQuestion("Select video source to stream from");
    for (VideoStreamSource i = 0; i < VideoStreamSource_Length; ++i) {
        printf("%d) %s\n", i + 1, VideoStreamSourceToCharString(i));
    }

    uint32_t selected;
    if (getIndexFromUser(
          inputfd, bytes, AudioStreamSource_Length, &selected, true) !=
        UserInputResult_Input) {
        puts("Canceled video streaming seleciton");
        return false;
    }

    switch (selected) {
        case VideoStreamSource_None:
            puts("Canceled video streaming seleciton");
            break;
        case VideoStreamSource_Webcam:
            return recordWebcam(&ui->id, inputfd, bytes);
        case VideoStreamSource_Window:
            return startWindowRecording(&ui->id, inputfd, bytes);
        default:
            fprintf(stderr, "Unknown option: %d\n", selected);
            break;
    }
    return true;
}

bool
selectAudioStreamSource(struct pollfd inputfd,
                        pBytes bytes,
                        pAudioState state,
                        pUserInput ui)
{
    if (!askYesOrNoQuestion(
          "Do you want record audio and stream it?", inputfd, bytes)) {
        return false;
    }

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
        case AudioStreamSource_None:
            puts("Canceled audio streaming seleciton");
            break;
        case AudioStreamSource_File:
            askQuestion("Enter file to stream");
            switch (getStringFromUser(inputfd, bytes, true)) {
                case UserInputResult_Input:
                    // Set id before function call
                    ui->data.tag = UserInputDataTag_file;
                    ui->data.file = TemLangStringCreate((char*)bytes->buffer,
                                                        currentAllocator);
                    return true;
                default:
                    puts("Canceled audio streaming file");
                    break;
            }
            break;
        case AudioStreamSource_Microphone:
            return askAndStartRecording(inputfd, bytes, state);
        case AudioStreamSource_Window:
            return startWindowAudioStreaming(inputfd, bytes, state);
        default:
            fprintf(stderr, "Unknown option: %d\n", selected);
            break;
    }
    return false;
}

void
recordCallback(pAudioState state, uint8_t* data, int len)
{
#if HIGH_QUALITY_AUDIO
    const float* fdata = (float*)data;
    const int fsize = len / sizeof(float);
    float sum = 0.f;
    for (int i = 0; i < fsize; ++i) {
        sum += fabsf(fdata[i]);
    }
    const ClientConfiguration* config =
      (const ClientConfiguration*)clientData.configuration;
    if (sum / fsize < config->silenceThreshold) {
        return;
    }
#else
    const int16_t* fdata = (int16_t*)data;
    const int fsize = len / sizeof(int16_t);
    float sum = 0.f;
    for (int i = 0; i < fsize; ++i) {
        sum += fabsf((float)fdata[i]);
    }
    const ClientConfiguration* config =
      (const ClientConfiguration*)clientData.configuration;
    if (sum / fsize < config->silenceThreshold) {
        return;
    }
#endif
    CQueueEnqueue(&state->storedAudio, data, len);
}

bool
askAndStartRecording(struct pollfd inputfd, pBytes bytes, pAudioState state)
{
    const int devices = SDL_GetNumAudioDevices(SDL_TRUE);
    if (devices == 0) {
        fprintf(stderr, "No recording devices found to send audio\n");
        return false;
    }
    if (devices < 0) {
        fprintf(
          stderr, "Failed to find recording devices: %s\n", SDL_GetError());
        return false;
    }

    askQuestion("Select a device to record from");
    for (int i = 0; i < devices; ++i) {
        printf("%d) %s\n", i + 1, SDL_GetAudioDeviceName(i, SDL_TRUE));
    }

    uint32_t selected = 0;
    if (getIndexFromUser(inputfd, bytes, devices, &selected, true) !=
        UserInputResult_Input) {
        puts("Canceled recording selection");
        return false;
    }
    return startRecording(
      SDL_GetAudioDeviceName(selected, SDL_TRUE), OPUS_APPLICATION_VOIP, state);
}

bool
startRecording(const char* name, const int application, pAudioState state)
{
    state->isRecording = SDL_TRUE;
    const SDL_AudioSpec desiredRecordingSpec =
#if USE_AUDIO_CALLBACKS
      makeAudioSpec((SDL_AudioCallback)recordCallback, state);
#else
      makeAudioSpec(NULL, NULL);
#endif
    state->name = TemLangStringCreate(name, currentAllocator);
    state->deviceId = SDL_OpenAudioDevice(
      name, SDL_TRUE, &desiredRecordingSpec, &state->spec, 0);
    if (state->deviceId == 0) {
        fprintf(stderr, "Failed to start recording: %s\n", SDL_GetError());
        return false;
    }
    puts("Recording audio specification");
    printAudioSpec(&state->spec);

    printf("Opened recording audio device: %u\n", state->deviceId);

    const int size = opus_encoder_get_size(state->spec.channels);
    state->encoder = currentAllocator->allocate(size);
    const int error = opus_encoder_init(
      state->encoder, state->spec.freq, state->spec.channels, application);
    if (error < 0) {
        fprintf(
          stderr, "Failed to create voice encoder: %s\n", opus_strerror(error));
        return false;
    }
#if _DEBUG
    printf("Encoder: %p (%d)\n", state->encoder, size);
#endif

    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_RecordingAudio;
    e.user.data1 = (void*)(size_t)state->deviceId;
    SDL_PushEvent(&e);
    return true;
}

void
playbackCallback(pAudioState state, uint8_t* data, int len)
{
    const size_t queueSize = CQueueCount(&state->storedAudio);

    pCurrentAudio current = currentAllocator->allocate(sizeof(CurrentAudio));
    current->id = state->id;

    current->audio = (Bytes){ .allocator = currentAllocator,
                              .buffer = currentAllocator->allocate(queueSize),
                              .size = queueSize,
                              .used = 0 };
    current->audio.used = CQueueDequeue(
      &state->storedAudio, current->audio.buffer, current->audio.size, true);
    updateCurrentAudioDisplay(current);

    memset(data, state->spec.silence, len);
    CQueueDequeue(&state->storedAudio, data, len, false);
    len = SDL_min(len, (int)queueSize);
#if HIGH_QUALITY_AUDIO
    float* f = (float*)data;
    const int size = len / sizeof(float);
    for (int i = 0; i < size; ++i) {
        f[i] = SDL_clamp(f[i], -1.f, 1.f);
        f[i] *= state->volume;
    }
#else
    opus_int16* s = (opus_int16*)data;
    const int size = len / sizeof(opus_int16);
    for (int i = 0; i < size; ++i) {
        s[i] *= state->volume;
    }
#endif
}

bool
startPlayback(struct pollfd inputfd,
              pBytes bytes,
              pAudioState state,
              const bool ask)
{
    if (ask) {
        if (!askYesOrNoQuestion(
              "Do you want play audio from stream?", inputfd, bytes)) {
            return false;
        }
    } else {
        puts("Need to select audio source to play audio");
    }

    const int devices = SDL_GetNumAudioDevices(SDL_FALSE);
    if (devices == 0) {
        fprintf(stderr, "No playback devices found to play audio\n");
        return false;
    }

    askQuestion("Select an audio device to play audio from");
    state->isRecording = SDL_FALSE;
    for (int i = 0; i < devices; ++i) {
        printf("%d) %s\n", i + 1, SDL_GetAudioDeviceName(i, SDL_FALSE));
    }

    uint32_t selected = 0;
    if (devices == 1) {
        puts("Using the only playback device");
    } else if (getIndexFromUser(inputfd, bytes, devices, &selected, true) !=
               UserInputResult_Input) {
        puts("Playback canceled");
        return false;
    }

    return startPlaybackFromName(SDL_GetAudioDeviceName(selected, SDL_FALSE),
                                 state);
}

bool
startPlaybackFromName(const char* name, pAudioState state)
{
    const SDL_AudioSpec desiredRecordingSpec =
#if USE_AUDIO_CALLBACKS
      makeAudioSpec((SDL_AudioCallback)playbackCallback, state);
#else
      makeAudioSpec(NULL, NULL);
#endif
    if (name == NULL) {
        state->name =
          TemLangStringCreate("Default audio device", currentAllocator);
    } else {
        state->name = TemLangStringCreate(name, currentAllocator);
    }
    state->deviceId = SDL_OpenAudioDevice(
      name, SDL_FALSE, &desiredRecordingSpec, &state->spec, 0);
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

int
sendAudioThread(pAudioSendThreadData data)
{
    const Guid* id = &data->id;
    bool displayMissing = false;
    USE_DISPLAY(
      clientData.mutex, end2, displayMissing, { ++display->recordings; });

    pNullValueList packets = &data->packets;
    printf("Sending %u audio packets\n", packets->used);
    size_t i = 0;
    for (; i < packets->used && !appDone; ++i) {
        USE_DISPLAY(clientData.mutex, end, displayMissing, {
            NullValueListAppend(&display->outgoing, &packets->buffer[i]);
        });
        if (displayMissing) {
            break;
        }
        // Each packet is aprox 120 ms. Sleep for less to not
        // flood server but also not have breaks in audio.
        SDL_Delay(120U);
    }
    for (; i < packets->used; ++i) {
        enet_packet_destroy(packets->buffer[i]);
    }
    AudioSendThreadDataFree(data);
    currentAllocator->free(data);
    USE_DISPLAY(
      clientData.mutex, end3, displayMissing, { --display->recordings; });
    puts("Done sening audio");
    SDL_AtomicDecRef(&runningThreads);
    return EXIT_SUCCESS;
}

bool
decodeAudioData(const AudioExtension ext,
                const uint8_t* data,
                const size_t dataSize,
                pBytes bytes,
                const Guid* id)
{
    puts("Decoding audio data...");

    bool success = false;
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

    bytes->used = 0;
    switch (ext) {
        case AudioExtension_WAV:
            success = decodeWAV(data, dataSize, bytes);
            break;
        case AudioExtension_OGG:
            success = decodeOgg(data, dataSize, bytes);
            break;
        case AudioExtension_MP3:
            success = decodeMp3(data, dataSize, bytes);
            break;
        default:
            fprintf(stderr, "Unknown audio extension\n");
            break;
    }

    if (!success) {
        goto audioEnd;
    }

    const SDL_AudioSpec spec = makeAudioSpec(NULL, NULL);
#if TEST_DECODER
    (void)peer;
    SDL_AudioSpec obtained;
    const int deviceId =
      SDL_OpenAudioDevice(NULL, SDL_FALSE, &spec, &obtained, 0);
    if (deviceId == 0) {
        fprintf(stderr, "Failed to open playback device: %s\n", SDL_GetError());
        goto audioEnd;
    }
    SDL_PauseAudioDevice(deviceId, SDL_FALSE);
    SDL_QueueAudio(deviceId, bytes->buffer, bytes->used);
    while (!appDone && SDL_GetQueuedAudioSize(deviceId) > 0) {
        SDL_Delay(1000);
    }
    SDL_CloseAudioDevice(deviceId);
#else
    puts("Encoding audio for streaming...");
    pAudioSendThreadData audioSendData =
      currentAllocator->allocate(sizeof(AudioSendThreadData));
    audioSendData->id = *id;
    audioSendData->packets.allocator = currentAllocator;
    const size_t bytesRead =
      encodeAudioData(encoder, bytes, spec, &audioSendData->packets);
    if (bytesRead < bytes->used) {
        uint8_tListQuickRemove(bytes, 0, bytesRead);
        const int minDuration =
          audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_10_MS);
        for (int i = bytes->used; i < minDuration; ++i) {
            uint8_tListAppend(bytes, &spec.silence);
        }
        encodeAudioData(encoder, bytes, spec, &audioSendData->packets);
    }
    puts("Audio encoded");

    uint8_tListFree(bytes);
    (*bytes) = INIT_ALLOCATOR(MAX_PACKET_SIZE);

    SDL_Thread* thread = SDL_CreateThread(
      (SDL_ThreadFunction)sendAudioThread, "audio_file", audioSendData);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        for (size_t i = 0; i < audioSendData->packets.used; ++i) {
            enet_packet_destroy(audioSendData->packets.buffer[i]);
        }
        AudioSendThreadDataFree(audioSendData);
        currentAllocator->free(audioSendData);
        goto audioEnd;
    }

    SDL_AtomicIncRef(&runningThreads);
    SDL_DetachThread(thread);
#endif

audioEnd:
    currentAllocator->free(encoder);
    return success;
}

bool
askYesOrNoQuestion(const char* q, const struct pollfd inputfd, pBytes bytes)
{
    char buffer[KB(1)];
    snprintf(buffer, sizeof(buffer), "%s (y or n)", q);
    askQuestion(buffer);
    while (true) {
        switch (getStringFromUser(inputfd, bytes, false)) {
            case UserInputResult_Input:
                if (bytes->used != 1u) {
                    break;
                }
                switch ((char)bytes->buffer[0]) {
                    case 'y':
                        return true;
                    case 'n':
                        return false;
                    default:
                        break;
                }
                break;
            case UserInputResult_NoInput:
                continue;
            default:
                goto end;
        }
        puts("Enter 'y' or 'n'");
    }

end:
    puts("Canceling input counts as selecting no");
    return false;
}

void
selectStreamToSendTextTo(struct pollfd inputfd, pBytes bytes, pClient client)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    IN_MUTEX(clientData.mutex, end2, {
        for (size_t i = 0; i < clientData.displays.used; ++i) {
            ServerConfigurationListAppend(
              &list, &clientData.displays.buffer[i].config);
        }
    });

    if (ServerConfigurationListIsEmpty(&list)) {
        puts("No streams to send text to");
        goto end;
    }

    askQuestion("Send text to which stream?");
    for (uint32_t i = 0; i < list.used; ++i) {
        printf("%u) ", i + 1U);
        printServerConfigurationForClient(&list.buffer[i]);
    }
    puts("");

    uint32_t i = 0;
    if (list.used == 1) {
        puts("Sending text to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list.used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling text send");
        goto end;
    }

    if (!clientHasWriteAccess(client, &list.buffer[i])) {
        puts("Write access is not granted for this stream");
        goto end;
    }

    askQuestion("Enter text to send");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling text send");
        goto end;
    }
    UserInput userInput = { 0 };
    // If stream display is gone, guid will stay at all zeroes
    IN_MUTEX(clientData.mutex, end3, {
        const StreamDisplay* display = NULL;
        if (GetStreamDisplayFromName(
              &clientData.displays, &list.buffer[i].name, &display, NULL)) {
            userInput.id = display->id;
        }
    });
    userInput.data.text =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);
    userInput.data.tag = UserInputDataTag_text;
    UserInputListAppend(&userInputs, &userInput);
    UserInputFree(&userInput);
end:
    ServerConfigurationListFree(&list);
}

void
selectStreamToUploadFileTo(struct pollfd inputfd, pBytes bytes, pClient client)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    IN_MUTEX(clientData.mutex, end2, {
        for (size_t i = 0; i < clientData.displays.used; ++i) {
            ServerConfigurationListAppend(
              &list, &clientData.displays.buffer[i].config);
        }
    });

    if (ServerConfigurationListIsEmpty(&list)) {
        puts("No streams to upload file to");
        goto end;
    }

    askQuestion("Send file to which stream?");
    for (size_t i = 0; i < list.used; ++i) {
        printf("%zu) ", i + 1U);
        printServerConfigurationForClient(&list.buffer[i]);
    }
    puts("");

    uint32_t i = 0;
    if (list.used == 1U) {
        puts("Sending data to only stream available");
    } else if (getIndexFromUser(inputfd, bytes, list.used, &i, true) !=
               UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    if (!clientHasWriteAccess(client, &list.buffer[i])) {
        puts("Write access is not granted for this stream");
        goto end;
    }

    askQuestion("Enter file name");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling file upload");
        goto end;
    }

    UserInput userInput = { 0 };
    // If stream display is gone, guid will stay at all zeroes
    IN_MUTEX(clientData.mutex, end3, {
        const StreamDisplay* display = NULL;
        if (GetStreamDisplayFromName(
              &clientData.displays, &list.buffer[i].name, &display, NULL)) {
            userInput.id = display->id;
        }
    });
    userInput.data.tag = UserInputDataTag_file;
    userInput.data.file =
      TemLangStringCreate((char*)bytes->buffer, currentAllocator);
    UserInputListAppend(&userInputs, &userInput);
    UserInputFree(&userInput);

end:
    ServerConfigurationListFree(&list);
}

void
saveScreenshot(SDL_Window* window,
               SDL_Renderer* renderer,
               const StreamDisplay* display)
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
                             SDL_PIXELFORMAT_RGBA32,
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

    char buffer2[KB(2)];
    snprintf(buffer2, sizeof(buffer2), "Saved screenshot to '%s'\n", buffer);
    SDL_ShowSimpleMessageBox(
      SDL_MESSAGEBOX_INFORMATION, "Screenshot", buffer2, window);

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
selectStreamToChangeVolume(struct pollfd inputfd, pBytes bytes)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    IN_MUTEX(clientData.mutex, end2, {
        for (size_t i = 0; i < clientData.displays.used; ++i) {
            if (clientData.displays.buffer[i].config.data.tag ==
                ServerConfigurationDataTag_audio) {
                ServerConfigurationListAppend(
                  &list, &clientData.displays.buffer[i].config);
            }
        }
    });

    if (ServerConfigurationListIsEmpty(&list)) {
        puts("Not connected to any audio streams...");
        goto end;
    }

    for (size_t i = 0; i < list.used; ++i) {
        IN_MUTEX(clientData.mutex, end3, {
            const StreamDisplay* display = NULL;
            const AudioState* ptr = NULL;
            if (GetStreamDisplayFromName(
                  &clientData.displays, &list.buffer[i].name, &display, NULL) &&
                AudioStateFromGuid(
                  &audioStates, &display->id, false, &ptr, NULL)) {
                printf("%zu) %s (Volume: %d)\n",
                       i + 1,
                       display->config.name.buffer,
                       (int)roundf(ptr->volume * 100.f));
            }
        });
    }
    uint32_t i = 0;
    if (list.used == 1) {
        puts("Using only available audio stream");
    } else {
        askQuestion("Select audio stream");
        if (getIndexFromUser(inputfd, bytes, list.used, &i, true) !=
            UserInputResult_Input) {
            puts("Canceling audio volume change");
            goto end;
        }
    }

    const ServerConfiguration* config = &list.buffer[i];

    askQuestion("Enter a number between 0 - 100");
    if (getIndexFromUser(inputfd, bytes, 101, &i, true) !=
        UserInputResult_Input) {
        puts("Canceling audio volume change");
        goto end;
    }

    i = SDL_clamp(i, 0U, 100U);
    const float vf = SDL_clamp((float)i / 100.f, 0.f, 1.f);

    IN_MUTEX(clientData.mutex, end4, {
        const StreamDisplay* display = NULL;
        size_t sIndex = 0;
        if (GetStreamDisplayFromName(
              &clientData.displays, &config->name, &display, NULL) &&
            AudioStateFromGuid(
              &audioStates, &display->id, false, NULL, &sIndex)) {
            AudioStatePtr ptr = audioStates.buffer[sIndex];
            SDL_LockAudioDevice(ptr->deviceId);
            ptr->volume = vf;
            SDL_UnlockAudioDevice(ptr->deviceId);
            printf("Changed volume of stream: %s\n", config->name.buffer);
        }
    });

end:
    ServerConfigurationListFree(&list);
}

int
storeKey(void* userdata, SDL_Event* e)
{
    if (e->type == SDL_KEYDOWN) {
        SDL_atomic_t* ptr = (SDL_atomic_t*)userdata;
        SDL_AtomicSet(ptr, e->key.keysym.sym);
        SDL_DelEventWatch(storeKey, userdata);
    }
    return EXIT_SUCCESS;
}

void
changePressToTalk(const struct pollfd inputfd, pBytes bytes)
{
    pClientConfiguration config =
      (pClientConfiguration)clientData.configuration;
    printTalkMode(&config->talkMode);

    askQuestion("Select press to talk option");
    for (size_t i = 0; i < TalkModeTag_Length; ++i) {
        printf("%zu) %s\n", i + 1, TalkModeTagToCharString(i));
    }

    uint32_t i = 0;
    if (getIndexFromUser(inputfd, bytes, TalkModeTag_Length, &i, true) !=
        UserInputResult_Input) {
        puts("Canceling press to talk change");
        goto end;
    }

    switch (i) {
        case TalkModeTag_none:
            IN_MUTEX(clientData.mutex, end4, {
                TalkModeFree(&config->talkMode);
                config->talkMode.tag = TalkModeTag_none;
                config->talkMode.none = NULL;
            });
            break;
        case TalkModeTag_pressToMute: {
            askQuestion("Press key (in the WINDOW, not the CONSOLE!!!) to set "
                        "the mute button");
            SDL_atomic_t key = { 0 };
            SDL_AddEventWatch((SDL_EventFilter)storeKey, &key);
            while (!appDone && SDL_AtomicGet(&key) == 0) {
                SDL_Delay(1);
            }
            IN_MUTEX(clientData.mutex, end2, {
                TalkModeFree(&config->talkMode);
                config->talkMode.tag = TalkModeTag_pressToMute;
                config->talkMode.pressToMute = TemLangStringCreate(
                  SDL_GetKeyName(SDL_AtomicGet(&key)), currentAllocator);
            });
        } break;
        case TalkModeTag_pressToTalk: {
            askQuestion("Press key (in the WINDOW, not the CONSOLE!!!) to set "
                        "the talk button");
            SDL_atomic_t key = { 0 };
            SDL_AddEventWatch((SDL_EventFilter)storeKey, &key);
            while (!appDone && SDL_AtomicGet(&key) == 0) {
                SDL_Delay(1);
            }
            IN_MUTEX(clientData.mutex, end3, {
                TalkModeFree(&config->talkMode);
                config->talkMode.tag = TalkModeTag_pressToTalk;
                config->talkMode.pressToTalk = TemLangStringCreate(
                  SDL_GetKeyName(SDL_AtomicGet(&key)), currentAllocator);
            });
        } break;
        default:
            break;
    }
    printTalkMode(&config->talkMode);

end:
    return;
}

void
changeSilenceThreshold(const struct pollfd inputfd, pBytes bytes)
{
    pClientConfiguration config =
      (pClientConfiguration)clientData.configuration;
    printf("Current silence threshold: %f\n", config->silenceThreshold);

    askQuestion("Enter new silence threshold");

    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling silence threshold change");
        goto end;
    }

    config->silenceThreshold = atof((char*)bytes->buffer);

end:
    return;
}

void
selectStreamToChangeAudioSource(struct pollfd inputfd, pBytes bytes)
{
    ServerConfigurationList list = { .allocator = currentAllocator };
    IN_MUTEX(clientData.mutex, end2, {
        for (size_t i = 0; i < clientData.displays.used; ++i) {
            if (clientData.displays.buffer[i].config.data.tag ==
                ServerConfigurationDataTag_audio) {
                ServerConfigurationListAppend(
                  &list, &clientData.displays.buffer[i].config);
            }
        }
    });

    if (ServerConfigurationListIsEmpty(&list)) {
        puts("Not connected to any audio streams...");
        goto end;
    }

    for (size_t i = 0; i < list.used; ++i) {
        IN_MUTEX(clientData.mutex, end3, {
            const StreamDisplay* display = NULL;
            const AudioState* ptr = NULL;
            if (GetStreamDisplayFromName(
                  &clientData.displays, &list.buffer[i].name, &display, NULL) &&
                AudioStateFromGuid(
                  &audioStates, &display->id, false, &ptr, NULL)) {
                printf("%zu) %s (Audio device: %s)\n",
                       i + 1,
                       display->config.name.buffer,
                       ptr->name.buffer);
            }
        });
    }
    uint32_t i = 0;
    if (list.used == 1) {
        puts("Using only available audio stream");
    } else {
        askQuestion("Select audio stream");
        if (getIndexFromUser(inputfd, bytes, list.used, &i, true) !=
            UserInputResult_Input) {
            puts("Canceling audio source change");
            goto end;
        }
    }

    IN_MUTEX(clientData.mutex, end23, {
        size_t listIndex = 0;
        if (GetStreamDisplayFromName(
              &clientData.displays, &list.buffer[i].name, NULL, &listIndex)) {
            pStreamDisplay display = &clientData.displays.buffer[i];
            AudioStateRemoveFromList(&audioStates, &display->id);
            UserInput ui = { 0 };
            ui.id = display->id;
            display->choosingPlayback = true;
            ui.data.tag = UserInputDataTag_queryAudio;
            ui.data.queryAudio.readAccess =
              clientHasReadAccess(&display->client, &display->config);
            ui.data.queryAudio.writeAccess =
              clientHasWriteAccess(&display->client, &display->config);
            UserInputListAppend(&userInputs, &ui);
            UserInputFree(&ui);
        }
    });

end:
    ServerConfigurationListFree(&list);
}

int
userInputThread(void* ptr)
{
    displayUserOptions();

    pClient client = (pClient)ptr;
    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(KB(1)),
                    .size = KB(1),
                    .used = 0 };
    RandomState rs = makeRandomState();
    struct pollfd inputfd = { .events = POLLIN,
                              .revents = 0,
                              .fd = STDIN_FILENO };
    uint32_t index = 0;
    while (!appDone) {

        IN_MUTEX(clientData.mutex, end4, {
            for (size_t i = 0; i < userInputs.used; ++i) {
                UserInput ui = { 0 };
                UserInputCopy(&ui, &userInputs.buffer[i], currentAllocator);
                SDL_UnlockMutex(clientData.mutex);
                handleUserInput(inputfd, &ui, &bytes);
                SDL_LockMutex(clientData.mutex);
                UserInputFree(&ui);
            }
            UserInputListFree(&userInputs);
            userInputs.allocator = currentAllocator;
        });

        switch (getIndexFromUser(
          inputfd, &bytes, ClientCommand_Length, &index, false)) {
            case UserInputResult_Input:
                break;
            case UserInputResult_Error:
                displayUserOptions();
                continue;
            default:
                continue;
        }

        switch (index) {
            case ClientCommand_Quit:
                appDone = true;
                goto end;
            case ClientCommand_ConnectToStream:
                selectAStreamToConnectTo(inputfd, &bytes, &rs);
                break;
            case ClientCommand_DisconnectFromStream:
                selectAStreamToDisconnectFrom(inputfd, &bytes);
                break;
            case ClientCommand_ChangeAudioVolume:
                selectStreamToChangeVolume(inputfd, &bytes);
                break;
            case ClientCommand_ChangeAudioSource:
                selectStreamToChangeAudioSource(inputfd, &bytes);
                break;
            case ClientCommand_UploadFile:
                selectStreamToUploadFileTo(inputfd, &bytes, client);
                break;
            case ClientCommand_UploadText:
                selectStreamToSendTextTo(inputfd, &bytes, client);
                break;
            case ClientCommand_ShowStatus:
                IN_MUTEX(clientData.mutex, endShowStatus, {
                    askQuestion("Streams Available");
                    if (ServerConfigurationListIsEmpty(
                          &clientData.allStreams)) {
                        puts("None");
                    } else {
                        for (size_t i = 0; i < clientData.allStreams.used;
                             ++i) {
                            printServerConfigurationForClient(
                              &clientData.allStreams.buffer[i]);
                        }
                    }
                    askQuestion("Streams connected to");
                    if (StreamDisplayListIsEmpty(&clientData.displays)) {
                        puts("None");
                    } else {
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
                    }
                    askQuestion("Audio State");
                    if (AudioStatePtrListIsEmpty(&audioStates)) {
                        puts("None");
                    } else {
                        for (size_t i = 0; i < audioStates.used; ++i) {
                            const AudioStatePtr ptr = audioStates.buffer[i];
                            printf("%zu) %s (%s)\n",
                                   i + 1,
                                   ptr->name.buffer,
                                   ptr->isRecording ? "Recording" : "Playback");
                        }
                    }
                });
                break;
            case ClientCommand_ChangePressToTalk:
                changePressToTalk(inputfd, &bytes);
                break;
            case ClientCommand_ChangeSilenceThreshold:
                changeSilenceThreshold(inputfd, &bytes);
                break;
            default:
                fprintf(stderr,
                        "Command '%s' is not implemented\n",
                        ClientCommandToCharString(index));
                break;
        }
        displayUserOptions();
    }
end:
    uint8_tListFree(&bytes);
    SDL_AtomicDecRef(&runningThreads);
    return EXIT_SUCCESS;
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

    if (!display->visible) {
        goto end;
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

    if (!display->visible) {
        goto end;
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

#define AUDIO_SIZE 2048.f
#define HALF_AUDIO_SIZE (AUDIO_SIZE * 0.5f)

void
updateAudioDisplay(SDL_Renderer* renderer,
                   pStreamDisplay display,
                   const Bytes* bytes)
{
    if (renderer == NULL) {
        return;
    }

    if (display->texture == NULL) {
        display->texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_TARGET,
                                             AUDIO_SIZE,
                                             AUDIO_SIZE);
    }
    SDL_SetRenderTarget(renderer, display->texture);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0u, 0u, 0u, 255u);
    SDL_RenderClear(renderer);

    if (uint8_tListIsEmpty(bytes)) {
        goto end;
    }

    if (!display->visible) {
        goto end;
    }

    SDL_SetRenderDrawColor(renderer, 0u, 0u, 255u, 255u);
    SDL_RenderDrawLineF(
      renderer, 0.f, HALF_AUDIO_SIZE, AUDIO_SIZE, HALF_AUDIO_SIZE);

    SDL_SetRenderDrawColor(renderer, 0u, 255u, 0u, 255u);

#if HIGH_QUALITY_AUDIO
    const floatList list = { .buffer = (float*)bytes->buffer,
                             .used = bytes->used / sizeof(float),
                             .size = bytes->size / sizeof(float) };
#else
    const int16_tList list = { .buffer = (int16_t*)bytes->buffer,
                               .used = bytes->used / sizeof(int16_t),
                               .size = bytes->size / sizeof(int16_t) };
#endif
    SDL_FPointList points = { .allocator = currentAllocator };
    for (size_t i = 0; i < list.used; ++i) {
#if HIGH_QUALITY_AUDIO
        const SDL_FPoint p = { .x = ((float)i / (float)list.used) * AUDIO_SIZE,
                               .y = HALF_AUDIO_SIZE * list.buffer[i] +
                                    HALF_AUDIO_SIZE };
#else
        const SDL_FPoint p = { .x = ((float)i / (float)list.used) * AUDIO_SIZE,
                               .y = HALF_AUDIO_SIZE *
                                      ((list.buffer[i] * 2.f) / UINT16_MAX) +
                                    HALF_AUDIO_SIZE };
#endif
        SDL_FPointListAppend(&points, &p);
    }
    SDL_RenderDrawLinesF(renderer, points.buffer, points.used);
    SDL_FPointListFree(&points);

    display->srcRect.none = NULL;
    display->srcRect.tag = OptionalRectTag_none;

    if (display->dstRect.w < MIN_WIDTH) {
        display->dstRect.w = 512.f;
    }
    if (display->dstRect.h < MIN_HEIGHT) {
        display->dstRect.h = 128.f;
    }

end:
    return;
}

void
updateVideoDisplay(SDL_Renderer* renderer,
                   pStreamDisplay display,
                   const int width,
                   const int height,
                   const Bytes* bytes)
{
    if (renderer == NULL || display->visible == false) {
        return;
    }

    void* pixels = NULL;
    int pitch = 0;
    const bool validDimensions = width != 0 && height != 0;
    if (!validDimensions) {
        if (display->texture == NULL) {
            return;
        }
        goto doDraw;
    }
    if (display->texture == NULL || display->data.videoDimensions[0] != width ||
        display->data.videoDimensions[1] != height) {
        SDL_DestroyTexture(display->texture);
        display->texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_IYUV,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width,
                                             height);
        if (display->texture == NULL) {
            fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
            return;
        }
        printf("Created texture for video display: %dx%d\n", width, height);
        display->srcRect.none = NULL;
        display->srcRect.tag = OptionalRectTag_none;

        display->dstRect.x = 0;
        display->dstRect.y = 0;
        display->dstRect.w = width;
        display->dstRect.h = height;

        display->data.videoDimensions[0] = width;
        display->data.videoDimensions[1] = height;
    }

doDraw:
    if (SDL_LockTexture(display->texture, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "Failed to update texture: %s\n", SDL_GetError());
        goto end;
    }

    memcpy(pixels, bytes->buffer, bytes->used);

    SDL_UnlockTexture(display->texture);

end:
    return;
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
    if (renderer == NULL || *text == '\0') {
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

    if (!display->visible) {
        return;
    }

    const StreamDisplayChat* chat = &display->data.chat;
    if (ChatListIsEmpty(&chat->logs)) {
        return;
    }

    // SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0xffu,
    // 0xffu);

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
             TTF_Font* ttfFont,
             const size_t target,
             const float maxX,
             const float maxY,
             const bool canShowLabel)
{
    if (renderer == NULL) {
        return;
    }
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0x33u, 0x33u, 0x33u, 0xffu);
    SDL_RenderClear(renderer);

    const StreamDisplay* display = NULL;
    if (target < clientData.displays.used && canShowLabel) {
        display = &clientData.displays.buffer[target];
        if (display->texture != NULL && display->visible) {
            SDL_SetRenderDrawColor(renderer, 0xffu, 0xffu, 0x0u, 0xffu);
            const SDL_FRect rect =
              expandRect((const SDL_FRect*)&display->dstRect, 1.025f, 1.025f);
            SDL_RenderFillRectF(renderer, &rect);
        }
    }
    for (size_t i = 0; i < clientData.displays.used; ++i) {
        pStreamDisplay display = &clientData.displays.buffer[i];
        if (!display->visible) {
            continue;
        }
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
                result = SDL_RenderCopyF(renderer, texture, &r, rect);
            } break;
            default:
                result = SDL_RenderCopyF(renderer, texture, NULL, rect);
                break;
        }
        if (result != 0) {
            fprintf(stderr, "Failed to render: %s\n", SDL_GetError());
        }
    }

    // draw label
    const ClientConfiguration* config =
      (const ClientConfiguration*)clientData.configuration;
    if (config->showLabel && canShowLabel && display != NULL) {
        const SDL_Color fg = { .a = 128u, .r = 255u, .g = 255u, .b = 255u };
        const SDL_Color bg = { .a = 128u, .r = 0u, .g = 0u, .b = 0u };
        SDL_Surface* text =
          TTF_RenderText_Shaded(ttfFont, display->config.name.buffer, fg, bg);
        if (text != NULL) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, text);
            SDL_Rect dst = { .w = text->w, .h = text->h };
            SDL_GetMouseState(&dst.x, &dst.y);
            dst.x += MIN_WIDTH;
            SDL_RenderCopy(renderer, t, NULL, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(text);
    }

    // uint8_t background[4] = { 0xff, 0xffu, 0x0u, 0xffu };
    // renderFont(renderer, font, "Hello World", 0, 0, 5.f,
    // NULL, background);
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
        if (display->texture == NULL || !display->visible) {
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
refreshClients(ENetPeer* peer, pBytes bytes)
{
    LobbyMessage lm = { 0 };
    lm.tag = LobbyMessageTag_general;
    lm.general.getClients = NULL;
    lm.general.tag = GeneralMessageTag_getClients;
    MESSAGE_SERIALIZE(LobbyMessage, lm, (*bytes));
    sendBytes(peer, 1, CLIENT_CHANNEL, bytes, SendFlags_Normal);
}

bool
clientHandleGeneralMessage(const GeneralMessage* message, pClient client)
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
            TemLangStringCopy(
              &client->name, &message->authenticateAck, currentAllocator);
            printf("Client authenticated as %s\n", client->name.buffer);
            return true;
        default:
            printf("Unexpected message from lobby server: %s\n",
                   GeneralMessageTagToCharString(message->tag));
            return false;
    }
}

bool
sendUpdateUiEvent()
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_UpdateUi;
    return SDL_PushEvent(&e) == 0;
}

bool
setUiMenu(const MenuTag m)
{
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_SetUiMenu;
    e.user.data1 = (void*)(size_t)m;
    return SDL_PushEvent(&e) == 0;
}

bool
clientHandleLobbyMessage(const LobbyMessage* message, pClient client)
{
    bool result = false;
    switch (message->tag) {
        case LobbyMessageTag_allStreams:
            result = ServerConfigurationListCopy(
              &clientData.allStreams, &message->allStreams, currentAllocator);
            sendUpdateUiEvent();
            goto end;
        case LobbyMessageTag_general:
            result = clientHandleGeneralMessage(&message->general, client);
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
                    result = clientHandleLobbyMessage(&message, client);
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

bool
sendTextToServer(const char* text,
                 const ServerConfigurationDataTag serverType,
                 const Guid* id,
                 pBytes bytes,
                 SDL_Window* window)
{
    if (text == NULL) {
        displayError(window, "Cannot send empty text", false);
        return true;
    }
    switch (serverType) {
        case ServerConfigurationDataTag_text: {
            TextMessage message = { 0 };
            message.tag = TextMessageTag_text;
            message.text = TemLangStringCreate(text, currentAllocator);
            MESSAGE_SERIALIZE(TextMessage, message, (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);
            IN_MUTEX(clientData.mutex, textEnd, {
                size_t i = 0;
                if (GetStreamDisplayFromGuid(
                      &clientData.displays, id, NULL, &i)) {
                    NullValueListAppend(&clientData.displays.buffer[i].outgoing,
                                        (NullValue)&packet);
                } else {
                    enet_packet_destroy(packet);
                }
            });
            TextMessageFree(&message);
        } break;
        case ServerConfigurationDataTag_chat: {
            ChatMessage message = { 0 };
            message.tag = ChatMessageTag_message;
            message.message = TemLangStringCreate(text, currentAllocator);
            MESSAGE_SERIALIZE(ChatMessage, message, (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);
            IN_MUTEX(clientData.mutex, chatEnd, {
                size_t i = 0;
                if (GetStreamDisplayFromGuid(
                      &clientData.displays, id, NULL, &i)) {
                    NullValueListAppend(&clientData.displays.buffer[i].outgoing,
                                        (NullValue)&packet);
                } else {
                    enet_packet_destroy(packet);
                }
            });
            ChatMessageFree(&message);
        } break;
        default:
            fprintf(stderr,
                    "Cannot send text to '%s' server\n",
                    ServerConfigurationDataTagToCharString(serverType));
            return false;
    }
    return true;
}

bool
sendFileToServer(const char* buffer,
                 const ServerConfigurationDataTag serverType,
                 const Guid* id,
                 pBytes bytes,
                 SDL_Window* window)
{
    if (buffer == false) {
        displayError(window, "Invalid file name", false);
        return true;
    }
    bool result = false;
    int fd = -1;
    char* ptr = NULL;
    size_t size = 0;

    FileExtension ext = { 0 };
    if (!filenameToExtension(buffer, &ext)) {
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

    if (!mapFile(buffer, &fd, &ptr, &size, MapFileType_Read)) {
        fprintf(
          stderr, "Error opening file '%s': %s\n", buffer, strerror(errno));
        goto endDropFile;
    }

    result = true;
    switch (serverType) {
        case ServerConfigurationDataTag_text: {
            TextMessage message = { 0 };
            message.tag = TextMessageTag_text;
            message.text =
              TemLangStringCreateFromSize(ptr, size, currentAllocator);
            MESSAGE_SERIALIZE(TextMessage, message, (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);
            IN_MUTEX(clientData.mutex, fileTextEnd, {
                size_t i = 0;
                if (GetStreamDisplayFromGuid(
                      &clientData.displays, id, NULL, &i)) {
                    NullValueListAppend(&clientData.displays.buffer[i].outgoing,
                                        (NullValue)&packet);
                } else {
                    enet_packet_destroy(packet);
                }
            });
            TextMessageFree(&message);
        } break;
        case ServerConfigurationDataTag_chat: {
            ChatMessage message = { 0 };
            message.tag = ChatMessageTag_message;
            message.message =
              TemLangStringCreateFromSize(ptr, size, currentAllocator);
            MESSAGE_SERIALIZE(ChatMessage, message, (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);
            IN_MUTEX(clientData.mutex, fileChatEnd, {
                size_t i = 0;
                if (GetStreamDisplayFromGuid(
                      &clientData.displays, id, NULL, &i)) {
                    NullValueListAppend(&clientData.displays.buffer[i].outgoing,
                                        (NullValue)&packet);
                } else {
                    enet_packet_destroy(packet);
                }
            });
            ChatMessageFree(&message);
        } break;
        case ServerConfigurationDataTag_image: {
            ImageMessage message = { 0 };

            uint32_t mtu = 1024;
            {
                message.tag = ImageMessageTag_imageStart;
                message.imageStart = NULL;
                MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
                ENetPacket* packet =
                  BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);

                IN_MUTEX(clientData.mutex, imageEnd1, {
                    size_t i = 0;
                    const StreamDisplay* display = NULL;
                    if (GetStreamDisplayFromGuid(
                          &clientData.displays, id, &display, &i)) {
                        NullValueListAppend(
                          &clientData.displays.buffer[i].outgoing,
                          (NullValue)&packet);
                        mtu = SDL_max(mtu, display->mtu);
                    } else {
                        enet_packet_destroy(packet);
                    }
                });
            }

            printf("Sending image: %zu bytes\n", size);

            message.tag = ImageMessageTag_imageChunk;
            for (size_t i = 0; i < size; i += mtu) {
                message.imageChunk.buffer = (uint8_t*)ptr + i;
                const size_t s = SDL_min(mtu, size - i);
                message.imageChunk.used = s;
                message.imageChunk.size = s;
                MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
                ENetPacket* packet =
                  BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);
                bool sent = false;
                IN_MUTEX(clientData.mutex, imageEnd2, {
                    size_t i = 0;
                    if (GetStreamDisplayFromGuid(
                          &clientData.displays, id, NULL, &i)) {
                        NullValueListAppend(
                          &clientData.displays.buffer[i].outgoing,
                          (NullValue)&packet);
                        sent = true;
                    } else {
                        enet_packet_destroy(packet);
                    }
                });
                if (sent) {
                    printf("Sent image chunk: %zu "
                           "bytes (%zu left) \n",
                           s,
                           size - (i + s));
                    if (lowMemory()) {
                        SDL_Delay(0);
                    }
                    continue;
                }
                break;
            }

            message.tag = ImageMessageTag_imageEnd;
            message.imageEnd = NULL;
            MESSAGE_SERIALIZE(ImageMessage, message, (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, SendFlags_Normal);
            IN_MUTEX(clientData.mutex, imageEnd3, {
                size_t i = 0;
                if (GetStreamDisplayFromGuid(
                      &clientData.displays, id, NULL, &i)) {
                    NullValueListAppend(&clientData.displays.buffer[i].outgoing,
                                        (NullValue)&packet);
                } else {
                    enet_packet_destroy(packet);
                }
            });
        } break;
        case ServerConfigurationDataTag_audio:
            if (ext.tag != FileExtensionTag_audio) {
                fprintf(stderr,
                        "Must send audio file to "
                        "audio stream\n");
                break;
            }
            result = decodeAudioData(ext.audio, (uint8_t*)ptr, size, bytes, id);
            break;
        default:
            fprintf(stderr,
                    "Sending file to '%s' not valid\n",
                    ServerConfigurationDataTagToCharString(serverType));
            break;
    }

    // Don't free message since it wasn't allocated
endDropFile:
    unmapFile(fd, ptr, size);
    return result;
}

void
handleUserInput(const struct pollfd inputfd,
                const UserInput* userInput,
                pBytes bytes)
{
    ServerConfigurationDataTag serverType = ServerConfigurationDataTag_Invalid;
    IN_MUTEX(clientData.mutex, end2, {
        const StreamDisplay* display = NULL;
        if (GetStreamDisplayFromGuid(
              &clientData.displays, &userInput->id, &display, NULL)) {
            serverType = display->config.data.tag;
        }
    });
    const Guid* id = &userInput->id;
    switch (userInput->data.tag) {
        case UserInputDataTag_queryVideo: {
            bool d;
            USE_DISPLAY(
              clientData.mutex, endD, d, { display->choosingPlayback = true; });
            if (userInput->data.queryAudio.writeAccess) {
                selectVideoStreamSource(inputfd, bytes, userInput);
            }
            USE_DISPLAY(clientData.mutex, endD26, d, {
                display->choosingPlayback = false;
            });
        } break;
        case UserInputDataTag_queryAudio: {
            bool d;
            USE_DISPLAY(clientData.mutex, endD2, d, {
                display->choosingPlayback = true;
            });
            bool ask = false;
            if (userInput->data.queryAudio.writeAccess) {
                UserInput ui = { 0 };
                ui.id = userInput->id;
                ui.data.tag = UserInputDataTag_Invalid;
                pAudioState record =
                  currentAllocator->allocate(sizeof(AudioState));
                record->storedAudio = CQueueCreate(CQUEUE_SIZE);
                record->volume = 1.f;
                if (selectAudioStreamSource(inputfd, bytes, record, &ui)) {
                    handleUserInput(inputfd, &ui, bytes);
                    ask = true;
                    if (record->encoder != NULL) {
                        record->id = userInput->id;
                        IN_MUTEX(clientData.mutex, endRecord, {
                            AudioStatePtrListAppend(&audioStates, &record);
                        });
                        record = NULL;
                    }
                }
                if (record != NULL) {
                    AudioStateFree(record);
                    currentAllocator->free(record);
                }
                UserInputFree(&ui);
            }
            if (userInput->data.queryAudio.readAccess) {
                pAudioState playback =
                  currentAllocator->allocate(sizeof(AudioState));
                playback->storedAudio = CQueueCreate(CQUEUE_SIZE);
                playback->volume = 1.f;
                if (startPlayback(inputfd, bytes, playback, ask)) {
                    playback->id = userInput->id;
                    SDL_PauseAudioDevice(playback->deviceId, SDL_FALSE);
                    IN_MUTEX(clientData.mutex, endPlayback, {
                        AudioStatePtrListAppend(&audioStates, &playback);
                    });
                    playback = NULL;
                }
                if (playback != NULL) {
                    AudioStateFree(playback);
                    currentAllocator->free(playback);
                }
            }
            USE_DISPLAY(clientData.mutex, endD26m, d, {
                display->choosingPlayback = false;
            });
        } break;
        case UserInputDataTag_text:
            sendTextToServer(userInput->data.text.buffer,
                             serverType,
                             &userInput->id,
                             bytes,
                             NULL);
            break;
        case UserInputDataTag_file:
            sendFileToServer(userInput->data.text.buffer,
                             serverType,
                             &userInput->id,
                             bytes,
                             NULL);
            break;
        default:
            break;
    }
}

int
handleMicrophoneMute(void* userdata, SDL_Event* e)
{
    bool keyDown;
    switch (e->type) {
        case SDL_KEYDOWN:
            if (e->key.repeat) {
                return SDL_FALSE;
            }
            keyDown = true;
            break;
        case SDL_KEYUP:
            keyDown = false;
            break;
        default:
            return SDL_FALSE;
    }
    const SDL_AudioDeviceID id = (SDL_AudioDeviceID)(size_t)userdata;
    bool exists;
    IN_MUTEX(clientData.mutex, end, {
        exists = AudioStateFromId(&audioStates, id, true, NULL, NULL);
    })
    if (!exists) {
        SDL_DelEventWatch((SDL_EventFilter)handleMicrophoneMute, userdata);
        return SDL_FALSE;
    }
    const ClientConfiguration* config =
      (const ClientConfiguration*)clientData.configuration;
    switch (config->talkMode.tag) {
        case TalkModeTag_pressToMute:
            if (SDL_GetKeyFromName(config->talkMode.pressToMute.buffer) !=
                e->key.keysym.sym) {
                break;
            }
            SDL_PauseAudioDevice(id, keyDown);
            break;
        case TalkModeTag_pressToTalk:
            if (SDL_GetKeyFromName(config->talkMode.pressToTalk.buffer) !=
                e->key.keysym.sym) {
                break;
            }
            SDL_PauseAudioDevice(id, !keyDown);
            break;
        default:
            break;
    }
    return SDL_TRUE;
}

bool
handleUiClicked(pUiActor actor, pRenderInfo info)
{
    if (actor == NULL || actor->id != info->focusId) {
        return false;
    }
    size_t index = 0;
    const StreamDisplay* display = NULL;
    GetStreamDisplayFromGuid(
      &clientData.displays, &info->menu.id, &display, &index);
    bool changed = true;
    switch (info->menu.tag) {
        case MenuTag_Main: {
            switch (actor->type) {
                case MainButton_Hide:
                    info->showUi = false;
                    goto end;
                default:
                    break;
            }
            if (actor->userData == NULL) {
                break;
            }
            const ServerConfiguration* config =
              (const ServerConfiguration*)actor->userData;
            if (GetStreamDisplayFromName(
                  &clientData.displays, &config->name, &display, &index)) {
                switch (actor->type) {
                    case MainButton_Connect:
                        AudioStateRemoveFromList(&audioStates, &display->id);
                        StreamDisplayListSwapRemove(&clientData.displays,
                                                    index);
                        break;
                    case MainButton_Screenshot:
                        saveScreenshot(info->window, info->renderer, display);
                        break;
                    case MainButton_Visible:
                        clientData.displays.buffer[index].visible =
                          !display->visible;
                        break;
                    case MainButton_Data:
                        switch (config->data.tag) {
                            case ServerConfigurationDataTag_chat:
                            case ServerConfigurationDataTag_text:
                                info->menu.tag = MenuTag_EnterText;
                                info->menu.id = display->id;
                                break;
                            case ServerConfigurationDataTag_image:
                                info->menu.tag = MenuTag_EnterImage;
                                info->menu.id = display->id;
                                break;
                            case ServerConfigurationDataTag_audio: {
                                size_t index = 0;
                                if (AudioStateFromGuid(&audioStates,
                                                       &display->id,
                                                       true,
                                                       NULL,
                                                       &index)) {
                                    pAudioState state =
                                      audioStates.buffer[index];
                                    AudioStateFree(state);
                                    currentAllocator->free(state);
                                    changed = AudioStatePtrListSwapRemove(
                                      &audioStates, index);
                                } else {
                                    info->menu.tag = MenuTag_SendAudio;
                                    info->menu.id = display->id;
                                }
                            } break;
                            case ServerConfigurationDataTag_video:
                                info->menu.tag = MenuTag_SendVideo;
                                info->menu.id = display->id;
                                break;
                            default:
                                break;
                        }
                        sendUpdateUiEvent();
                        break;
                    default:
                        changed = false;
                        break;
                }
            } else if (actor->type == MainButton_Connect) {
                if (!connectToStream(config, NULL)) {
                    displayError(
                      info->window, "Failed to connect to stream", false);
                }
            } else {
                changed = false;
            }
        } break;
        case MenuTag_EnterText:
            switch (actor->type) {
                case EnterTextButton_Send: {
                    Bytes bytes = { .allocator = currentAllocator };
                    const UiActor* a =
                      findUiActor(&info->uiActors, EnterTextButton_TextBox);
                    if (a != NULL &&
                        !sendTextToServer(a->data.editText.text.buffer,
                                          display->config.data.tag,
                                          &display->id,
                                          &bytes,
                                          info->window)) {
                        displayError(
                          info->window, "Failed to send text", false);
                    }
                    uint8_tListFree(&bytes);
                    setUiMenu(MenuTag_Main);
                } break;
                case EnterTextButton_Back:
                    setUiMenu(MenuTag_Main);
                    break;
                default:
                    changed = false;
                    break;
            }
            break;
        case MenuTag_EnterImage:
            switch (actor->type) {
                case EnterTextButton_Send: {
                    Bytes bytes = { .allocator = currentAllocator };
                    const UiActor* a =
                      findUiActor(&info->uiActors, EnterTextButton_TextBox);
                    if (a != NULL &&
                        !sendFileToServer(a->data.editText.text.buffer,
                                          display->config.data.tag,
                                          &display->id,
                                          &bytes,
                                          info->window)) {
                        displayError(
                          info->window, "Failed to send image", false);
                    }
                    uint8_tListFree(&bytes);
                    setUiMenu(MenuTag_Main);
                } break;
                case EnterTextButton_Back:
                    setUiMenu(MenuTag_Main);
                    break;
                default:
                    changed = false;
                    break;
            }
            break;
        case MenuTag_SendAudio:
            switch (actor->type) {
                case EnterTextButton_Send: {
                    pAudioState record =
                      currentAllocator->allocate(sizeof(AudioState));
                    record->storedAudio = CQueueCreate(CQUEUE_SIZE);
                    record->volume = 1.f;
                    record->id = display->id;
                    if (startRecording(
                          SDL_GetAudioDeviceName(actor->id, SDL_TRUE),
                          OPUS_APPLICATION_VOIP,
                          record)) {
                        AudioStatePtrListAppend(&audioStates, &record);
                    } else {
                        displayError(info->window,
                                     "Failed to start recording audio",
                                     false);
                        AudioStateFree(record);
                        currentAllocator->free(record);
                    }
                    setUiMenu(MenuTag_Main);
                } break;
                case EnterTextButton_Send2: {
                    pAudioState record =
                      currentAllocator->allocate(sizeof(AudioState));
                    record->storedAudio = CQueueCreate(CQUEUE_SIZE);
                    record->volume = 1.f;
                    record->id = display->id;
                    pSinkInput sink = actor->userData;
                    if (recordSink(sink, record)) {
                        AudioStatePtrListAppend(&audioStates, &record);
                    } else {
                        displayError(info->window,
                                     "Failed to start recording audio",
                                     false);
                        AudioStateFree(record);
                        currentAllocator->free(record);
                    }
                    setUiMenu(MenuTag_Main);
                } break;
                case EnterTextButton_Back:
                    setUiMenu(MenuTag_Main);
                    break;
                default:
                    break;
            }
            break;
        default:
            changed = false;
            break;
    }
end:
    return changed;
}

void
handleUserEvent(const SDL_UserEvent* e,
                pRenderInfo info,
                const size_t targetDisplay,
                ENetPeer* peer,
                pBytes bytes,
                const MouseState mouseState,
                const int w,
                const int h)
{
    switch (e->code) {
        case CustomEvent_ShowSimpleMessage:
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     (char*)e->data1,
                                     (char*)e->data2,
                                     info->window);
            break;
        case CustomEvent_UpdateStreamDisplay: {
            const Guid id = *(const Guid*)e->data1;
            currentAllocator->free(e->data1);
            size_t index = 0;
            if (!GetStreamDisplayFromGuid(
                  &clientData.displays, &id, NULL, &index)) {
                fprintf(stderr,
                        "Cannot update stream display because "
                        "it was removed "
                        "from list\n");
                break;
            }
            pStreamDisplay display = &clientData.displays.buffer[index];
            switch (display->data.tag) {
                case StreamDisplayDataTag_text:
                    updateTextDisplay(info->renderer, info->font, display);
                    break;
                case StreamDisplayDataTag_chat:
                    updateChatDisplay(
                      info->renderer, info->font, w, h, display);
                    break;
                case StreamDisplayDataTag_image:
                    updateImageDisplay(info->renderer, display);
                    break;
                default:
                    break;
            }
            RENDER_NOW(w, h, (*info));
        } break;
        case CustomEvent_SaveScreenshot: {
            const Guid id = *(const Guid*)e->data1;
            currentAllocator->free(e->data1);
            const StreamDisplay* display = NULL;
            if (!GetStreamDisplayFromGuid(
                  &clientData.displays, &id, &display, NULL)) {
                fprintf(stderr,
                        "Cannot screenshot stream display '%s' because it was "
                        "removed from list\n",
                        display->config.name.buffer);
                break;
            }
            saveScreenshot(info->window, info->renderer, display);
        } break;
        case CustomEvent_SendLobbyMessage: {
            MESSAGE_SERIALIZE(
              LobbyMessage, (*(const LobbyMessage*)e->data1), (*bytes));
            sendBytes(peer, 1, CLIENT_CHANNEL, bytes, SendFlags_Normal);
            LobbyMessageFree(e->data1);
            currentAllocator->free(e->data1);
        } break;
        case CustomEvent_UpdateAudioDisplay: {
            pCurrentAudio ptr = (pCurrentAudio)e->data1;
            const Guid id = ptr->id;
            size_t i = 0;
            if (GetStreamDisplayFromGuid(&clientData.displays, &id, NULL, &i)) {
                updateAudioDisplay(
                  info->renderer, &clientData.displays.buffer[i], &ptr->audio);
            }
            CurrentAudioFree(ptr);
            currentAllocator->free(ptr);
            RENDER_NOW(w, h, (*info));
        } break;
        case CustomEvent_RecordingAudio: {
            const SDL_AudioDeviceID deviceId =
              (SDL_AudioDeviceID)(size_t)e->data1;
            const ClientConfiguration* config =
              (const ClientConfiguration*)clientData.configuration;
            switch (config->talkMode.tag) {
                case TalkModeTag_pressToMute:
                    SDL_PauseAudioDevice(deviceId, SDL_FALSE);
                    break;
                case TalkModeTag_pressToTalk:
                    SDL_PauseAudioDevice(deviceId, SDL_TRUE);
                    break;
                default:
                    SDL_PauseAudioDevice(deviceId, SDL_FALSE);
                    break;
            }
            SDL_AddEventWatch((SDL_EventFilter)handleMicrophoneMute, e->data1);
        } break;
        case CustomEvent_UpdateVideoDisplay: {
            pVideoFrame ptr = (pVideoFrame)e->data1;
            size_t i = 0;
            if (GetStreamDisplayFromGuid(
                  &clientData.displays, &ptr->id, NULL, &i)) {
                updateVideoDisplay(info->renderer,
                                   &clientData.displays.buffer[i],
                                   ptr->width,
                                   ptr->height,
                                   &ptr->video);
            } else {
                fprintf(stderr,
                        "Tried to update stream display that doesn't exist\n");
            }
            VideoFrameFree(ptr);
            currentAllocator->free(ptr);
            RENDER_NOW(w, h, (*info));
        } break;
        case CustomEvent_SetUiMenu:
            info->menu.tag = MenuTag_Main;
            sendUpdateUiEvent();
            break;
        case CustomEvent_UpdateUi: {
            UiActorListFree(&info->uiActors);
            info->uiActors = getUiMenuActors(&info->menu);
            RENDER_NOW(w, h, (*info));
        } break;
        case CustomEvent_UiChanged:
            switch (info->menu.tag) {
                case MenuTag_Main: {
                    pUiActor actor = e->data1;
                    if (actor->id != info->focusId || actor->userData == NULL) {
                        break;
                    }
                    switch (actor->type) {
                        case MainButton_Slider: {
                            AudioStatePtr ptr = actor->userData;
                            ptr->volume = actor->data.slider.value / 100.f;
                            sendUpdateUiEvent();
                        } break;
                        default:
                            break;
                    }
                } break;
                default:
                    break;
            }
            RENDER_NOW(w, h, (*info));
            break;
        case CustomEvent_UiClicked:
            if (handleUiClicked(e->data1, info)) {
                sendUpdateUiEvent();
            }
            break;
        default:
            break;
    }
}

#define HANDLE_MENU_BUTTON                                                     \
    if (window == NULL) {                                                      \
        displayUserOptions();                                                  \
    } else {                                                                   \
        info.showUi = !info.showUi;                                            \
        RENDER_NOW(w, h, info);                                                \
    }

int
doRunClient(const Configuration* configuration,
            SDL_Window* window,
            SDL_Renderer* renderer,
            TTF_Font* ttfFont,
            const char* hostname,
            const uint16_t port)
{
    clientData.configuration = (NullValue)&configuration->client;
    clientData.window = window;

    int result = EXIT_FAILURE;
    puts("Running client...");

    SDL_RenderClear(renderer);
    {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        const SDL_Rect rect = {
            .x = w / 10, .y = h * 4 / 10, .w = w * 8 / 10, .h = h * 2 / 10
        };
        const SDL_Color fg = { 255u, 255u, 255u, 255u };
        const SDL_Color bg = { 0u, 0u, 0u, 255u };
        SDL_Surface* surface =
          TTF_RenderUTF8_Shaded_Wrapped(ttfFont, "Connecting...", fg, bg, 0);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
        SDL_RenderPresent(renderer);
    }

    const ClientConfiguration* config = &configuration->client;

    Bytes bytes = { .allocator = currentAllocator,
                    .buffer = currentAllocator->allocate(MAX_PACKET_SIZE),
                    .size = MAX_PACKET_SIZE,
                    .used = 0 };

    ENetHost* host = NULL;
    ENetPeer* peer = NULL;
    Client client = { 0 };

    RenderInfo info = { .window = window,
                        .renderer = renderer,
                        .font = ttfFont,
                        .showUi = window != NULL,
                        .menu = { .tag = MenuTag_Main,
                                  .sinks = { .allocator = currentAllocator } },
                        .focusId = INT_MAX,
                        .uiActors = { .allocator = currentAllocator } };

    clientData.displays.allocator = currentAllocator;
    clientData.allStreams.allocator = currentAllocator;

    clientData.mutex = SDL_CreateMutex();
    if (clientData.mutex == NULL) {
        fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
        goto end;
    }

    SDL_AtomicSet(&runningThreads, 0);

    SDL_Event e = { 0 };

    size_t targetDisplay = UINT32_MAX;
    MoveMode moveMode = MoveMode_None;
    bool hasTarget = false;

    userInputs.allocator = currentAllocator;
    audioStates.allocator = currentAllocator;

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL) {
        fprintf(stderr, "Failed to create client host\n");
        appDone = true;
        goto end;
    }
    {
        ENetAddress address = { 0 };
        enet_address_set_host(&address, hostname);
        address.port = port;
        peer = enet_host_connect(
          host, &address, 2, ServerConfigurationDataTag_lobby);
        char buffer[512] = { 0 };
        enet_address_get_host_ip(&address, buffer, sizeof(buffer));
        printf("Connecting to server: %s:%u...\n", buffer, address.port);
    }
    if (peer == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        displayError(window, "Failed to connect to server", false);
        appDone = true;
        goto end;
    }

    ENetEvent event = { 0 };
    for (size_t i = 0; i < 50 && !appDone; ++i) {
        if (enet_host_service(host, &event, 100U) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT) {
            char buffer[KB(1)] = { 0 };
            enet_address_get_host_ip(
              &event.peer->address, buffer, sizeof(buffer));
            printf(
              "Connected to server: %s:%u\n", buffer, event.peer->address.port);
            sendAuthentication(peer, ServerConfigurationDataTag_lobby);
            goto runServerProcedure;
        }
    }

    fprintf(stderr, "Failed to connect to server\n");
    displayError(window, "Failed to connect to server", false);
    enet_peer_reset(peer);
    peer = NULL;
    appDone = true;
    goto end;

runServerProcedure:
    if (enet_host_service(host, &event, 5000U) > 0 &&
        event.type == ENET_EVENT_TYPE_RECEIVE) {
        const Bytes packetBytes = { .allocator = currentAllocator,
                                    .buffer = event.packet->data,
                                    .size = event.packet->dataLength,
                                    .used = event.packet->dataLength };
        LobbyMessage m = { 0 };
        MESSAGE_DESERIALIZE(LobbyMessage, m, packetBytes);
        enet_packet_destroy(event.packet);
        const bool success = m.tag == LobbyMessageTag_general &&
                             m.general.tag == GeneralMessageTag_authenticateAck;
        if (success) {
            TemLangStringCopy(
              &client.name, &m.general.authenticateAck, currentAllocator);
            LobbyMessageFree(&m);
        } else {
            LobbyMessageFree(&m);
            fprintf(stderr, "Failed to authenticate to server\n");
            enet_peer_reset(peer);
            peer = NULL;
            appDone = true;
            goto end;
        }
    } else {
        fprintf(stderr, "Failed to connect to server\n");
        enet_peer_reset(peer);
        peer = NULL;
        appDone = true;
        goto end;
    }

    if (!config->noTui) {
        SDL_Thread* thread = SDL_CreateThread(
          (SDL_ThreadFunction)userInputThread, "user_input", &client);
        if (thread == NULL) {
            fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
            goto end;
        }
        SDL_AtomicIncRef(&runningThreads);
        SDL_DetachThread(thread);
    }

    uint32_t lastMouseMove = 0;
    MouseState mouseState = MouseState_Visible | MouseState_InWindow;
    if (window != NULL) {
        info.uiActors = getUiMenuActors(&info.menu);
    }

    while (!appDone) {
        if (!checkForMessagesFromLobby(host, &event, &client)) {
            appDone = true;
            break;
        }
        while (!appDone && SDL_PollEvent(&e)) {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            SDL_LockMutex(clientData.mutex);
            switch (e.type) {
                case SDL_QUIT:
                    appDone = true;
                    break;
                case SDL_WINDOWEVENT:
                    switch (e.window.event) {
                        case SDL_WINDOWEVENT_ENTER:
                            mouseState |= MouseState_InWindow;
                            break;
                        case SDL_WINDOWEVENT_LEAVE:
                            mouseState &= ~MouseState_InWindow;
                            break;
                        default:
                            break;
                    }
                    RENDER_NOW(w, h, info);
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
                        HANDLE_MENU_BUTTON;
                        break;
                    }
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            SDL_AtomicSet(&continueSelection, 0);
                            HANDLE_MENU_BUTTON;
                            break;
                        case SDLK_F1:
                            HANDLE_MENU_BUTTON;
                            break;
                        case SDLK_F2:
                            printf("Memory: %zu (%zu MB) / %zu "
                                   "(%zu MB) \n",
                                   currentAllocator->used(),
                                   currentAllocator->used() / (MB(1)),
                                   currentAllocator->totalSize(),
                                   currentAllocator->totalSize() / (MB(1)));
                            break;
#if _DEBUG
                        case SDLK_F3:
                            if (hasTarget) {
                                printf("Target display: %zu\n", targetDisplay);
                            } else {
                                puts("No target display");
                            }
                            break;
#endif
                        case SDLK_v:
                            if ((e.key.keysym.mod & KMOD_CTRL) == 0 ||
                                info.showUi) {
                                break;
                            }
                            e.type = SDL_DROPTEXT;
                            e.drop.file = SDL_GetClipboardText();
                            e.drop.timestamp = SDL_GetTicks64();
                            e.drop.windowID = SDL_GetWindowID(window);
                            SDL_PushEvent(&e);
                            break;
                        case SDLK_RETURN:
                        case SDLK_RETURN2:
                        case SDLK_KP_ENTER:
                            if (info.showUi) {
                                break;
                            }
                            HANDLE_MENU_BUTTON;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    if (info.showUi) {
                        break;
                    }
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
                        RENDER_NOW(w, h, info);
                    }
                } break;
                case SDL_MOUSEBUTTONUP:
                    if (info.showUi) {
                        break;
                    }
                    hasTarget = false;
                    targetDisplay = UINT32_MAX;
                    SDL_SetWindowGrab(window, false);
                    RENDER_NOW(w, h, info);
                    break;
                case SDL_MOUSEMOTION:
                    lastMouseMove = e.motion.timestamp;
                    if (info.showUi) {
                        break;
                    }
                    if (hasTarget) {
                        if (targetDisplay >= clientData.displays.used) {
                            break;
                        }
                        pStreamDisplay display =
                          &clientData.displays.buffer[targetDisplay];
                        if (display->texture == NULL) {
                            break;
                        }
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
                        RENDER_NOW(w, h, info);
                    } else {
                        SDL_FPoint point = { 0 };
                        int x, y;
                        SDL_GetMouseState(&x, &y);
                        point.x = (float)x;
                        point.y = (float)y;
                        targetDisplay = UINT_MAX;
                        findDisplayFromPoint(&point, &targetDisplay);
                        RENDER_NOW(w, h, info);
                    }
                    break;
                case SDL_MOUSEWHEEL: {
                    if (info.showUi) {
                        break;
                    }
                    if (targetDisplay >= clientData.displays.used) {
                        break;
                    }
                    pStreamDisplay display =
                      &clientData.displays.buffer[targetDisplay];
                    switch (display->data.tag) {
                        case StreamDisplayDataTag_chat: {
                            pStreamDisplayChat chat = &display->data.chat;
                            int64_t offset = (int64_t)chat->offset;

                            // printf("Before: %" PRId64 "\n",
                            // offset);
                            offset += (e.wheel.y > 0) ? -1LL : 1LL;
                            offset = SDL_clamp(
                              offset, 0LL, (int64_t)chat->logs.used - 1LL);
                            chat->offset = (uint32_t)offset;
                            // printf("After: %u\n",
                            // chat->offset); printf("Logs
                            // %u\n", chat->logs.used);

                            const float width = display->dstRect.w;
                            const float height = display->dstRect.h;
                            updateChatDisplay(renderer, ttfFont, w, h, display);
                            display->dstRect.w = width;
                            display->dstRect.h = height;
                        } break;
                        default:
                            break;
                    }
                    RENDER_NOW(w, h, info);
                } break;
                case SDL_USEREVENT:
                    handleUserEvent(&e.user,
                                    &info,
                                    targetDisplay,
                                    peer,
                                    &bytes,
                                    mouseState,
                                    w,
                                    h);
                    break;
                case SDL_DROPFILE: {
                    if (info.showUi) {
                        goto endDropFile;
                    }
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
                    if (!clientHasWriteAccess(&display->client,
                                              &display->config)) {
                        printf("Cannot send file to stream '%s' because client "
                               "doesn't have write access",
                               display->config.name.buffer);
                        goto endDropFile;
                    }
                    UserInput userInput = { 0 };
                    userInput.id = display->id;
                    userInput.data.tag = UserInputDataTag_file;
                    userInput.data.file =
                      TemLangStringCreate(e.drop.file, currentAllocator);
                    UserInputListAppend(&userInputs, &userInput);
                    UserInputFree(&userInput);
                endDropFile:
                    SDL_free(e.drop.file);
                } break;
                case SDL_DROPTEXT: {
                    if (info.showUi) {
                        goto endDropText;
                    }
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
                    userInput.id = display->id;
                    userInput.data.tag = UserInputDataTag_text;
                    userInput.data.text =
                      TemLangStringCreate(e.drop.file, currentAllocator);
                    UserInputListAppend(&userInputs, &userInput);
                    UserInputFree(&userInput);
                endDropText:
                    SDL_free(e.drop.file);
                } break;
                default:
                    break;
            }
            SDL_UnlockMutex(clientData.mutex);
            if (info.showUi) {
                updateUiActors(&e, &info);
            }
        }
        if (SDL_GetTicks() - lastMouseMove > 1000U) {
            SDL_ShowCursor(SDL_DISABLE);
            mouseState &= ~MouseState_Visible;
        } else {
            SDL_ShowCursor(SDL_ENABLE);
            mouseState |= MouseState_Visible;
        }
        SDL_Delay(1u);
    }

    result = EXIT_SUCCESS;

end:
    appDone = true;
    UiActorListFree(&info.uiActors);
    // FontFree(&font);
    while (SDL_AtomicGet(&runningThreads) > 0) {
        SDL_Delay(1);
    }
    for (size_t i = 0; i < audioStates.used; ++i) {
        AudioStateFree(audioStates.buffer[i]);
        currentAllocator->free(audioStates.buffer[i]);
    }
    AudioStatePtrListFree(&audioStates);

    MenuFree(&info.menu);
    closeHostAndPeer(host, peer);
    ClientFree(&client);
    uint8_tListFree(&bytes);
    UserInputListFree(&userInputs);

    ClientDataFree(&clientData);
    // Ensure that SDL user events don't still have memory
    while (SDL_WaitEventTimeout(&e, 100) == 1) {
        if (e.type != SDL_USEREVENT) {
            continue;
        }
        switch (e.user.code) {
            case CustomEvent_SaveScreenshot:
                currentAllocator->free(e.user.data1);
                break;
            case CustomEvent_SendLobbyMessage:
                LobbyMessageFree(e.user.data1);
                currentAllocator->free(e.user.data1);
                break;
            case CustomEvent_UpdateStreamDisplay:
                currentAllocator->free(e.user.data1);
                break;
            case CustomEvent_UpdateAudioDisplay:
                CurrentAudioFree(e.user.data1);
                currentAllocator->free(e.user.data1);
                break;
            case CustomEvent_UpdateVideoDisplay:
                VideoFrameFree(e.user.data1);
                currentAllocator->free(e.user.data1);
                break;
            default:
                break;
        }
    }
    return result;
}

int
getHostnameFromTUI(const Configuration* configuration)
{
    uint8_t hostname[KB(1)];
    uint8_t portStr[KB(1)];
    const struct pollfd inputfd = { .fd = STDIN_FILENO,
                                    .events = POLLIN,
                                    .revents = 0 };
    askQuestion("Enter hostname");
    {
        Bytes bytes = { .allocator = NULL,
                        .buffer = hostname,
                        .size = sizeof(hostname),
                        .used = 0 };
        switch (getStringFromUser(inputfd, &bytes, true)) {
            case UserInputResult_Input:
                break;
            default:
                return EXIT_FAILURE;
        }
    }
    askQuestion("Enter port");
    {
        Bytes bytes = { .allocator = NULL,
                        .buffer = portStr,
                        .size = sizeof(portStr),
                        .used = 0 };
        switch (getStringFromUser(inputfd, &bytes, true)) {
            case UserInputResult_Input:
                break;
            default:
                return EXIT_FAILURE;
        }
    }
    const uint16_t port = strtoul((char*)portStr, NULL, 10);
    return doRunClient(configuration, NULL, NULL, NULL, (char*)hostname, port);
}

void
renderUi(pRenderInfo info)
{
    SDL_SetRenderDrawColor(info->renderer, 0x33u, 0x33u, 0x33u, 0xffu);
    SDL_RenderClear(info->renderer);
    renderUiActors(info);
    SDL_RenderPresent(info->renderer);
}

int
getHostnameFromGUI(const Configuration* configuration)
{
    const ClientConfiguration* config = &configuration->client;

    int result = EXIT_FAILURE;

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    TTF_Font* ttfFont = NULL;
    UiActor actors[4] = { 0 };
    const int64_t actorCount = sizeof(actors) / sizeof(UiActor);

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
        displayError(window, "Failed to start", true);
        goto end;
    }

    {
        SDL_RendererInfo info = { 0 };
#if PRINT_RENDER_INFO
        int drivers = SDL_GetNumRenderDrivers();
        for (int i = 0; i < drivers; ++i) {
            SDL_GetRenderDriverInfo(i, &info);
            printRenderInfo(&info);
        }
#endif

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == NULL) {
            fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
            displayError(window, "Failed to start", true);
            goto end;
        }

        SDL_GetRendererInfo(renderer, &info);
        printf("Current renderer: %s\n", info.name);
    }

    ttfFont = TTF_OpenFont(config->ttfFile.buffer, config->fontSize);
    if (ttfFont == NULL) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        displayError(window, "Failed to start", true);
        goto end;
    }

    for (size_t i = 0; i < actorCount; ++i) {
        actors[i].id = i;
    }

    actors[0].horizontal = HorizontalAlignment_Center;
    actors[0].vertical = VerticalAlignment_Center;
    actors[0].rect = (Rect){ .x = 500, .y = 100, .w = 500, .h = 100 };
    actors[0].data.tag = UiDataTag_label;
    actors[0].data.label.label =
      TemLangStringCreate("Enter stream connection", currentAllocator);

    actors[1].horizontal = HorizontalAlignment_Center;
    actors[1].vertical = VerticalAlignment_Center;
    actors[1].rect = (Rect){ .x = 500, .y = 350, .w = 500, .h = 100 };
    actors[1].data.tag = UiDataTag_editText;
    actors[1].data.editText =
      (EditText){ .label = TemLangStringCreate("Hostname", currentAllocator),
                  .text = TemLangStringCreate("localhost", currentAllocator) };

    actors[2].horizontal = HorizontalAlignment_Center;
    actors[2].vertical = VerticalAlignment_Center;
    actors[2].rect = (Rect){ .x = 500, .y = 500, .w = 500, .h = 100 };
    actors[2].data.tag = UiDataTag_editText;
    actors[2].data.editText =
      (EditText){ .label = TemLangStringCreate("Port", currentAllocator),
                  .text = TemLangStringCreate("10000", currentAllocator) };

    actors[3].horizontal = HorizontalAlignment_Center;
    actors[3].vertical = VerticalAlignment_Center;
    actors[3].rect = (Rect){ .x = 500, .y = 650, .w = 500, .h = 100 };
    actors[3].data.tag = UiDataTag_label;
    actors[3].data.label.label =
      TemLangStringCreate("Connect", currentAllocator);

    RenderInfo info = {
        .focusId = 1,
        .font = ttfFont,
        .renderer = renderer,
        .window = window,
        .showUi = true,
        .uiActors = { .buffer = actors, .used = actorCount, .size = actorCount }
    };
    renderUi(&info);

    SDL_Event e = { 0 };

    while (!appDone) {
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type) {
                case SDL_QUIT:
                    appDone = true;
                    goto end;
                case SDL_WINDOWEVENT:
                    renderUi(&info);
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_UP:
                            info.focusId =
                              SDL_clamp(info.focusId - 1, 0, actorCount);
                            renderUi(&info);
                            break;
                        case SDLK_TAB:
                        case SDLK_KP_TAB:
                        case SDLK_DOWN:
                            info.focusId =
                              SDL_clamp(info.focusId + 1, 0, actorCount);
                            renderUi(&info);
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_USEREVENT:
                    switch (e.user.code) {
                        case CustomEvent_UiChanged:
                            renderUi(&info);
                            break;
                        case CustomEvent_UiClicked:
                            if (e.user.data1 != &actors[3]) {
                                break;
                            }
                            result = doRunClient(
                              configuration,
                              window,
                              renderer,
                              ttfFont,
                              actors[1].data.editText.text.buffer,
                              strtoul(
                                actors[2].data.editText.text.buffer, NULL, 10));
                            if (result != EXIT_SUCCESS) {
                                appDone = false;
                                break;
                            }
                            goto end;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            updateUiActors(&e, &info);
        }
        SDL_Delay(1u);
    }
end:
    MenuFree(&info.menu);
    for (size_t i = 0; i < actorCount; ++i) {
        UiActorFree(&actors[i]);
    }
    TTF_CloseFont(ttfFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return result;
}

int
runClient(const Configuration* configuration)
{
    int result = EXIT_FAILURE;
    printConfiguration(configuration);

    {
        const bool showWindow = !configuration->client.noGui;
        uint32_t flags = 0;
        if (showWindow) {
            flags |= SDL_INIT_VIDEO;
        }
        if (!configuration->client.noAudio) {
            flags |= SDL_INIT_AUDIO;
        }
        if (SDL_Init(flags) != 0) {
            fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
            displayError(NULL, "Failed to start", showWindow);
            goto end;
        }
        if (showWindow) {
            const uint32_t flags = IMG_INIT_PNG | IMG_INIT_WEBP | IMG_INIT_TIF;
            if (IMG_Init(flags) != flags) {
                fprintf(
                  stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
                displayError(NULL, "Failed to start", showWindow);
                goto end;
            }
            if (TTF_Init() == -1) {
                fprintf(stderr, "Failed to init TTF: %s\n", TTF_GetError());
                displayError(NULL, "Failed to start", showWindow);
                goto end;
            }
        }
    }

    appDone = false;
    while (!appDone) {
        if (configuration->client.noGui) {
            result = getHostnameFromTUI(configuration);
        } else {
            result = getHostnameFromGUI(configuration);
        }
    }
end:
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return result;
}