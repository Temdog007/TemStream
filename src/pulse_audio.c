#include <include/main.h>

const char createNullSinkCommand[] =
  "pactl load-module module-null-sink sink_name=%s_sink "
  "sink_properties=device.description=%s_sink";

const char createComboSinkCommand[] =
  "pactl load-module module-combine-sink sink_name=%s_combo "
  "sink_properties=device.description=%s_combo slaves=%s_sink,%s";

const char remapSourceCommand[] =
  "pactl load-module module-remap-source source_name=%s_remapped "
  "master=%s_sink.monitor source_properties=device.description=%s_remapped";

const char moveSinkCommand[] = "pactl move-sink-input %d %s_combo";

int
printSinkInput(const SinkInput* sink)
{
    return printf("%s (process: %d; current sink id: %d; sink input id: %d)",
                  sink->name.buffer,
                  sink->processId,
                  sink->currentSinkId,
                  sink->inputId);
}

int
processOutputToStrings(const char* command, pTemLangStringList list)
{
    FILE* file = popen(command, "r");
    if (file == NULL) {
        perror("popen");
        return EXIT_FAILURE;
    }

    char buffer[KB(4)] = { 0 };
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        TemLangString str = { .allocator = currentAllocator };
        TemLangStringAppendChars(&str, buffer);
        TemLangStringListAppend(list, &str);
        TemLangStringFree(&str);
    }

    return pclose(file);
}

int
processOutputToString(const char* command, pTemLangString str)
{
    FILE* file = popen(command, "r");
    if (file == NULL) {
        perror("popen");
        return EXIT_FAILURE;
    }

    char buffer[KB(4)] = { 0 };
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        TemLangStringAppendChars(str, buffer);
    }

    return pclose(file);
}

int
processOutputToNumber(const char* command, int* output)
{
    TemLangString str = { .allocator = currentAllocator };
    const int result = processOutputToString(command, &str);
    if (result == 0) {
        char* end = NULL;
        *output = (int)strtol(str.buffer, &end, 10);
    }
    TemLangStringFree(&str);
    return result;
}

bool
startWindowStreaming(const struct pollfd inputfd,
                     pBytes bytes,
                     pAudioState state)
{
    if (state->sinks.allocator == NULL) {
        state->sinks.allocator = currentAllocator;
    }
    bool result = false;

    char buffer[KB(4)] = { 0 };
    TemLangString realSinkName = { .allocator = currentAllocator };
    TemLangStringList output = { .allocator = currentAllocator };
    SinkInputList sinks = { .allocator = currentAllocator };
    puts("Checking for audio playing processes...");

    if (processOutputToStrings("pactl list sink-inputs", &output) != 0 ||
        TemLangStringListIsEmpty(&output)) {
        fputs("Failed to find audio playing processes\n", stderr);
        goto end;
    }

    if (!stringToSinkInputs(&output, &sinks) || SinkInputListIsEmpty(&sinks)) {
        fputs("Failed to find any audio sinks\n", stderr);
        goto end;
    }

    uint32_t index = 0;
    if (sinks.used == 1) {
        const SinkInput* sink = &sinks.buffer[0];
        printf("Selecting only available process: ");
        printSinkInput(sink);
        puts("");
    } else {
        askQuestion("Select a process");
        for (size_t i = 0; i < sinks.used; ++i) {
            const SinkInput* sink = &sinks.buffer[i];
            printf("%zu) ", i + 1);
            printSinkInput(sink);
            puts("");
        }
        if (getIndexFromUser(inputfd, bytes, sinks.used, &index, true) !=
            UserInputResult_Input) {
            goto end;
        }
    }

    const SinkInput* target = &sinks.buffer[index];

    char sinkName[KB(2)];
    snprintf(sinkName,
             sizeof(sinkName),
             "%s_%d",
             target->name.buffer,
             target->processId);

    // Make null sink
    snprintf(buffer, sizeof(buffer), createNullSinkCommand, sinkName, sinkName);
    int nullHandle = 0L;
    if (processOutputToNumber(buffer, &nullHandle) != EXIT_SUCCESS) {
        fputs("Failed to create new audio sink\n", stderr);
        goto end;
    }

    int32_tListAppend(&state->sinks, &nullHandle);

    // Make combo sink so that audio is still sent to the current playback
    // device
    if (!getSinkName(target->currentSinkId, &realSinkName)) {
        fprintf(stderr,
                "Failed to get sink name from ID: %d\n",
                target->currentSinkId);
        goto end;
    }
    snprintf(buffer,
             sizeof(buffer),
             createComboSinkCommand,
             sinkName,
             sinkName,
             sinkName,
             realSinkName.buffer);
    int comboHandle = 0L;
    if (processOutputToNumber(buffer, &comboHandle) != EXIT_SUCCESS) {
        fputs("Failed to create new audio sink\n", stderr);
        goto end;
    }

    int32_tListAppend(&state->sinks, &comboHandle);

    // Re-map null source so it can work like a microphone (needed for SDL to
    // detect)
    snprintf(
      buffer, sizeof(buffer), remapSourceCommand, sinkName, sinkName, sinkName);
    int remapHandle = 0L;
    if (processOutputToNumber(buffer, &remapHandle) != EXIT_SUCCESS) {
        fputs("Failed to remap audio source\n", stderr);
        goto end;
    }
    int32_tListAppend(&state->sinks, &remapHandle);

    // Move the target process to send audio to the null sink
    snprintf(
      buffer, sizeof(buffer), moveSinkCommand, target->inputId, sinkName);
#if _DEBUG
    puts(buffer);
#endif
    if (system(buffer) != 0) {
        fputs("Failed to move process audio source\n", stderr);
        goto end;
    }

    snprintf(buffer, sizeof(buffer), "%s_remapped", sinkName);
#if _DEBUG
    puts(buffer);
#endif
    result = startRecording(buffer, OPUS_APPLICATION_AUDIO, state);
    if (!result) {
        unloadSink(nullHandle);
        unloadSink(comboHandle);
    }

end:
    SinkInputListFree(&sinks);
    TemLangStringFree(&realSinkName);
    TemLangStringListFree(&output);
    return result;
}

void
unloadSink(const int32_t id)
{
    char buffer[KB(1)] = { 0 };
    snprintf(buffer, sizeof(buffer), "pactl unload-module %d", id);
    system(buffer);
}

bool
getSinkName(const int32_t id, pTemLangString str)
{
    FILE* file = popen("pactl list sinks short", "r");
    if (file == NULL) {
        perror("popen");
        return false;
    }

    bool result = false;
    char buffer[KB(1)] = { 0 };
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        char* start = NULL;
        const int fid = (int)strtol(buffer, &start, 10);
        if (id != fid) {
            continue;
        }
        while (isspace(*start) && *start != '\0') {
            ++start;
        }

        char* end = start;
        while (!isspace(*end) && *end != '\0') {
            ++end;
        }

        const size_t len = (size_t)(end - start);
        result = TemLangStringAppendSizedBuffer(str, start, len);
    }

    pclose(file);
    return result;
}

bool
stringToSinkInput(pTemLangStringList list, size_t* offset, pSinkInput sink)
{
    bool foundSink = false;
    bool foundName = false;
    bool foundPID = false;
    bool foundId = false;
    size_t i = *offset;
    for (; i < list->used && !(foundId && foundName && foundSink && foundPID);
         ++i) {
        pTemLangString str = &list->buffer[i];
        // Look for Sink, application.name, and application.process.id
        TemLangStringTrim(str);
        char* end = NULL;
        if (!foundName &&
            TemLangStringStartsWith(str, "application.name = \"")) {
            // Will have quotes
            foundName = true;
            TemLangStringFree(&sink->name);
            sink->name = TemLangStringCreate(
              str->buffer + (sizeof("application.name = \"") - 1),
              currentAllocator);
            TemLangStringPop(&sink->name);
            continue;
        }
        if (!foundPID &&
            TemLangStringStartsWith(str, "application.process.id = \"")) {
            foundPID = true;
            sink->processId = (int32_t)strtol(
              str->buffer + (sizeof("application.process.id = \"") - 1),
              &end,
              10);
            continue;
        }
        if (!foundSink && TemLangStringStartsWith(str, "Sink: ")) {
            foundSink = true;
            sink->currentSinkId =
              (int32_t)strtol(str->buffer + (sizeof("Sink: ") - 1), &end, 10);
#if _DEBUG
            printf("%s = %d\n", str->buffer, sink->currentSinkId);
#endif
            continue;
        }
        if (!foundId && TemLangStringStartsWith(str, "Sink Input #")) {
            foundId = true;
            sink->inputId = (int32_t)strtol(
              str->buffer + (sizeof("Sink Input #") - 1), &end, 10);
#if _DEBUG
            printf("%s = %d\n", str->buffer, sink->inputId);
#endif
            continue;
        }
    }
    *offset = i;
    return foundId && foundName && foundSink && foundPID;
}

bool
stringToSinkInputs(pTemLangStringList list, pSinkInputList sinks)
{
    size_t i = 0;
    while (i < list->used) {
        SinkInput sink = { 0 };
        if (stringToSinkInput(list, &i, &sink)) {
            SinkInputListAppend(sinks, &sink);
        }
        SinkInputFree(&sink);
    }
    return true;
}