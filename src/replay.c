#include <include/main.h>

DEFINE_RUN_SERVER(Replay);

bool
updateTimeRange(pServerData serverData);

ReplayConfiguration
defaultReplayConfiguration()
{
    return (ReplayConfiguration){ .filename =
                                    TemLangStringCreate("", currentAllocator) };
}

int
printReplayConfiguration(const ReplayConfiguration* configuration)
{
    return printf("Replay file: %s\n", configuration->filename.buffer);
}

void
onReplayDownTime(pServerData data)
{
    updateTimeRange(data);
}

bool
parseReplayConfiguration(const int argc,
                         const char** argv,
                         pConfiguration configuration)
{
    configuration->server.data.tag = ServerConfigurationDataTag_replay;
    pReplayConfiguration replay = &configuration->server.data.replay;
    for (int i = 2; i < argc - 1; i += 2) {
        const char* key = argv[i];
        const size_t keyLen = strlen(key);
        const char* value = argv[i + 1];
        STR_EQUALS(key, "-F", keyLen, { goto parseFile; });
        STR_EQUALS(key, "--file", keyLen, { goto parseFile; });
        if (!parseCommonConfiguration(key, value, configuration) &&
            !parseServerConfiguration(key, value, &configuration->server)) {
            parseFailure("Replay", key, value);
            return false;
        }
        continue;

    parseFile : {
        TemLangStringFree(&replay->filename);
        replay->filename = TemLangStringCreate(value, currentAllocator);
        continue;
    }
    }
    return true;
}

ReplayMessage timeRangeMessage = { .tag = ReplayMessageTag_timeRange,
                                   .timeRange = { 0, 0 } };

bool
updateTimeRange(pServerData serverData)
{
    bool result = false;

    char* buffer = currentAllocator->allocate(MB(1));
    FILE* file = fopen(
      getServerReplayFileNameFromReplayConfig(serverData->config, buffer), "r");
    if (file == NULL) {
        perror("Failed to open file");
        goto end;
    }
    if (fgets(buffer, MB(1), file)) {
        timeRangeMessage.timeRange[0] = (int64_t)strtoull(buffer, NULL, 10);
    }
    while (fgets(buffer, MB(1), file))
        ;
    timeRangeMessage.timeRange[1] = (int64_t)strtoull(buffer, NULL, 10);
    result = true;
end:
    if (file != NULL) {
        fclose(file);
    }
    currentAllocator->free(buffer);
    return result;
}

bool
onConnectForReplay(ENetPeer* peer, pServerData serverData)
{
    if (updateTimeRange(serverData)) {
        MESSAGE_SERIALIZE(ReplayMessage, timeRangeMessage, serverData->bytes);
        sendBytes(
          peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
        return true;
    }
    return false;
}

bool
handleReplayMessage(const void* ptr, ENetPeer* peer, pServerData serverData)
{
    // const ReplayConfiguration* config = &serverData->config->data.text;
    bool result = false;
    CAST_MESSAGE(ReplayMessage, ptr);
    switch (message->tag) {
        case ReplayMessageTag_general: {
            ReplayMessage replayMessage = { 0 };
            replayMessage.tag = ReplayMessageTag_general;
            result = handleGeneralMessage(
              &message->general, serverData, &replayMessage.general);
            if (result) {
                MESSAGE_SERIALIZE(
                  ReplayMessage, replayMessage, serverData->bytes);
                sendBytes(peer,
                          1,
                          SERVER_CHANNEL,
                          &serverData->bytes,
                          SendFlags_Normal);
            }
            ReplayMessageFree(&replayMessage);
        } break;
        case ReplayMessageTag_getTimeRange: {
            MESSAGE_SERIALIZE(
              ReplayMessage, timeRangeMessage, serverData->bytes);
            sendBytes(
              peer, 1, SERVER_CHANNEL, &serverData->bytes, SendFlags_Normal);
            result = true;
        } break;
        case ReplayMessageTag_request: {
            char* buffer = currentAllocator->allocate(MB(1));
            FILE* file = fopen(getServerReplayFileNameFromReplayConfig(
                                 serverData->config, buffer),
                               "r");
            if (file == NULL) {
                perror("Failed to open file");
                goto requestEnd;
            }
            result = true;
            ReplayMessage response = { .tag = ReplayMessageTag_response };
            while (fgets(buffer, MB(1), file)) {
                char* end = NULL;
                const int64_t t = (int64_t)strtoull(buffer, &end, 10);
                if (t < message->request) {
                    continue;
                }
                if (t > message->request) {
                    break;
                }
                ++end;
                if (!isalnum(*end) && *end != '/' && *end != '+') {
                    continue;
                }
                const size_t l = strlen(end);
                if (l == 0) {
                    continue;
                }
                if (end[l - 1] == '\n') {
                    end[l - 1] = '\0';
                }
                if (!b64_decode(end, &serverData->bytes)) {
                    continue;
                }
                MESSAGE_DESERIALIZE(
                  ServerMessage, response.response, serverData->bytes);
                MESSAGE_SERIALIZE(ReplayMessage, response, serverData->bytes);
                sendBytes(peer,
                          1,
                          SERVER_CHANNEL,
                          &serverData->bytes,
                          SendFlags_Normal);
                if (lowMemory()) {
                    enet_host_flush(serverData->host);
                }
            }
            ReplayMessageFree(&response);
        requestEnd:
            if (file != NULL) {
                fclose(file);
            }
            currentAllocator->free(buffer);
        } break;
        default:
#if _DEBUG
            printf("Unexpected replay message '%s' from client\n",
                   ReplayMessageTagToCharString(message->tag));
#endif
            break;
    }
    return result;
}

void
storeClientMessage(pServerData data, const ServerMessage* m)
{
    char buffer[512];
    FILE* file =
      fopen(getServerReplayFileNameFromConfig(data->config, buffer), "a");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }
    MESSAGE_SERIALIZE(ServerMessage, (*m), data->bytes);
    TemLangString str = b64_encode(&data->bytes);
    const time_t t = time(NULL);
    fprintf(file, "%" PRId64 ":%s\n", t, str.buffer);
    TemLangStringFree(&str);
    fclose(file);
}

const char*
getServerReplayFileNameFromConfig(const ServerConfiguration* config,
                                  char buffer[KB(1)])
{
    if (TemLangStringIsEmpty(&config->saveDirectory)) {
        snprintf(buffer, KB(1), "%s.temstream_replay", config->name.buffer);
    } else {
        snprintf(buffer,
                 KB(1),
                 "%s/%s.temstream_replay",
                 config->saveDirectory.buffer,
                 config->name.buffer);
    }
    return buffer;
}

const char*
getServerReplayFileNameFromReplayConfig(const ServerConfiguration* config,
                                        char buffer[KB(1)])
{
    if (TemLangStringIsEmpty(&config->saveDirectory)) {
        snprintf(buffer, KB(1), "%s", config->data.replay.filename.buffer);
    } else {
        snprintf(buffer,
                 KB(1),
                 "%s/%s",
                 config->saveDirectory.buffer,
                 config->data.replay.filename.buffer);
    }
    return buffer;
}