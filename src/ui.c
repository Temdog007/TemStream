#include <include/main.h>

const UiActor*
findUiActor(const UiActorList* list, const int32_t type)
{
    for (size_t i = 0; i < list->used; ++i) {
        if (list->buffer[i].type == type) {
            return &list->buffer[i];
        }
    }
    return NULL;
}

SDL_Rect
getUiActorRect(const UiActor* actor, const int w, const int h)
{
    SDL_Rect rect = { .x = actor->rect.x * w / 1000,
                      .y = actor->rect.y * h / 1000,
                      .w = actor->rect.w * w / 1000,
                      .h = actor->rect.h * h / 1000 };
    switch (actor->horizontal) {
        case HorizontalAlignment_Center:
            rect.x -= rect.w / 2;
            break;
        case HorizontalAlignment_Right:
            rect.x -= rect.w;
            break;
        default:
            break;
    }
    switch (actor->vertical) {
        case VerticalAlignment_Center:
            rect.y -= rect.h / 2;
            break;
        case VerticalAlignment_Bottom:
            rect.y -= rect.h;
            break;
        default:
            break;
    }
    return rect;
}

void
updateUiActors(const SDL_Event* e, pRenderInfo info)
{
    int w, h;
    SDL_GetWindowSize(info->window, &w, &h);
    for (size_t i = 0; i < info->uiActors.used; ++i) {
        updateUiActor(e, &info->uiActors.buffer[i], w, h, &info->focusId);
    }
}

bool
actorNeedsText(const UiActor* actor)
{
    switch (actor->data.tag) {
        case UiDataTag_editText:
            return true;
        default:
            return false;
    }
}

bool
checkActorFocus(const SDL_Point* point,
                pUiActor actor,
                const int w,
                const int h,
                int32_t* focusId)
{
    const SDL_Rect rect = getUiActorRect(actor, w, h);
    const SDL_bool inRect = SDL_PointInRect(point, &rect);
    bool changed = false;
    if (inRect == SDL_TRUE) {
        changed = *focusId != actor->id;
        *focusId = actor->id;
        if (actorNeedsText(actor)) {
            SDL_StartTextInput();
        }
    } else if (*focusId == actor->id) {
        *focusId = UINT_MAX;
        if (actorNeedsText(actor)) {
            SDL_StopTextInput();
        }
    }
    return changed;
}

void
uiUpdate(pUiActor actor, const int32_t code)
{
    SDL_Event ev = { .user = {
                       .type = SDL_USEREVENT, .code = code, .data1 = actor } };
    SDL_PushEvent(&ev);
}

void
updateEditText(const SDL_Event* e,
               pUiActor actor,
               const float w,
               const float h,
               int32_t* focusId)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONDOWN:
            uiUpdate(actor,
                     *focusId == actor->id ? CustomEvent_UiClicked
                                           : CustomEvent_UiChanged);
            break;
        case SDL_MOUSEBUTTONUP: {
            SDL_Point point = { .x = e->button.x, .y = e->button.y };
            checkActorFocus(&point, actor, w, h, focusId);
            uiUpdate(actor,
                     *focusId == actor->id ? CustomEvent_UiClicked
                                           : CustomEvent_UiChanged);
        } break;
        case SDL_MOUSEMOTION: {
            SDL_Point point = { .x = e->motion.x, .y = e->motion.y };
            if (checkActorFocus(&point, actor, w, h, focusId)) {
                uiUpdate(actor, CustomEvent_UiChanged);
            }
        } break;
        case SDL_KEYDOWN:
            switch (e->key.keysym.sym) {
                case SDLK_BACKSPACE:
                case SDLK_DELETE:
                    if (actor->id != *focusId) {
                        break;
                    }
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        actor->data.editText.text.used = 0;
                        actor->data.editText.text.buffer[0] = '\0';
                    } else {
                        TemLangStringPop(&actor->data.editText.text);
                    }
                    uiUpdate(actor, CustomEvent_UiChanged);
                    break;
                case SDLK_RETURN:
                case SDLK_RETURN2:
                case SDLK_KP_ENTER:
                    if (SDL_GetTicks64() < 1000u) {
                        break;
                    }
                    if (actor->id != *focusId || e->key.repeat) {
                        break;
                    }
                    uiUpdate(actor, CustomEvent_UiClicked);
                    break;
                case SDLK_c:
                    if ((e->key.keysym.mod & KMOD_CTRL) == 0) {
                        break;
                    }
                    if (SDL_SetClipboardText(
                          actor->data.editText.text.buffer) == 0) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                                 "Message",
                                                 "Copied text",
                                                 NULL);
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                                 "Failed to copy text",
                                                 SDL_GetError(),
                                                 NULL);
                    }
                    break;
                case SDLK_v:
                    if ((e->key.keysym.mod & KMOD_CTRL) == 0) {
                        break;
                    }
                    char* text = SDL_GetClipboardText();
                    if (text == NULL) {
                        break;
                    }
                    actor->data.editText.text.used = 0;
                    TemLangStringAppendChars(&actor->data.editText.text, text);
                    SDL_free(text);
                    uiUpdate(actor, CustomEvent_UiChanged);
                    break;
                default:
                    break;
            }
            break;
        case SDL_TEXTINPUT:
            if (actor->id != *focusId) {
                break;
            }
            TemLangStringAppendChars(&actor->data.editText.text, e->text.text);
            uiUpdate(actor, CustomEvent_UiChanged);
            break;
        default:
            break;
    }
}

bool
updateSliderFromPoint(pUiActor actor,
                      const int w,
                      const int h,
                      int32_t* focusId)
{
    SDL_Point point;
    const int state = SDL_GetMouseState(&point.x, &point.y);
    bool changed = checkActorFocus(&point, actor, w, h, focusId);
    if (actor->id == *focusId && (state & SDL_BUTTON_LMASK) != 0) {
        const SDL_Rect rect = getUiActorRect(actor, w, h);
        const bool horizontal = rect.w > rect.h;
        const float t = horizontal
                          ? glm_percentc(rect.x, rect.x + rect.w, point.x)
                          : glm_percentc(rect.y, rect.y + rect.h, point.y);
        actor->data.slider.value = (int32_t)floorf(
          glm_lerpc(actor->data.slider.min, actor->data.slider.max, t));
        changed = true;
    }
    return changed;
}

bool
updateSlider(const SDL_Event* e,
             pUiActor actor,
             const float w,
             const float h,
             int32_t* focusId)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return updateSliderFromPoint(actor, w, h, focusId);
        case SDL_MOUSEMOTION:
            return updateSliderFromPoint(actor, w, h, focusId);
        default:
            break;
    }
    return false;
}

void
updateLabel(const SDL_Event* e,
            pUiActor actor,
            const float w,
            const float h,
            int32_t* focusId)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONDOWN:
            uiUpdate(actor, CustomEvent_UiChanged);
            break;
        case SDL_MOUSEBUTTONUP: {
            SDL_Point point = { .x = e->button.x, .y = e->button.y };
            checkActorFocus(&point, actor, w, h, focusId);
            uiUpdate(actor,
                     *focusId == actor->id ? CustomEvent_UiClicked
                                           : CustomEvent_UiChanged);
        } break;
        case SDL_MOUSEMOTION: {
            SDL_Point point = { .x = e->motion.x, .y = e->motion.y };
            if (checkActorFocus(&point, actor, w, h, focusId)) {
                uiUpdate(actor, CustomEvent_UiChanged);
            }
        } break;
        default:
            break;
    }
}

void
updateUiActor(const SDL_Event* e,
              pUiActor actor,
              const float w,
              const float h,
              int32_t* focusId)
{
    switch (actor->data.tag) {
        case UiDataTag_editText:
            updateEditText(e, actor, w, h, focusId);
            break;
        case UiDataTag_slider:
            if (updateSlider(e, actor, w, h, focusId)) {
                uiUpdate(actor, CustomEvent_UiChanged);
            }
            break;
        case UiDataTag_label:
            updateLabel(e, actor, w, h, focusId);
            break;
        default:
            break;
    }
}

void
renderUiActors(pRenderInfo info)
{
    int w, h;
    SDL_GetWindowSize(info->window, &w, &h);
    for (size_t i = 0; i < info->uiActors.used; ++i) {
        const SDL_Rect rect = getUiActorRect(&info->uiActors.buffer[i], w, h);
        renderUiActor(info, &info->uiActors.buffer[i], rect, w, h);
    }
}

void
renderUiActor(pRenderInfo info,
              pUiActor actor,
              SDL_Rect rect,
              const int w,
              const int h)
{
    (void)h;
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;

    SDL_Color fg;
    SDL_Color bg;
    if (actor->id == info->focusId) {
        if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0) {
            fg = (SDL_Color){ 255u, 0u, 0u, 255u };
            bg = (SDL_Color){ 255u, 255u, 0u, 255u };
        } else {
            fg = (SDL_Color){ 255u, 255u, 0u, 255u };
            bg = (SDL_Color){ 0u, 0u, 0u, 255u };
        }
    } else {
        fg = (SDL_Color){ 255u, 255u, 255u, 255u };
        bg = (SDL_Color){ 0u, 0u, 0u, 255u };
    }

    switch (actor->data.tag) {
        case UiDataTag_label:
            surface =
              TTF_RenderUTF8_Shaded_Wrapped(info->font,
                                            actor->data.label.label.buffer,
                                            fg,
                                            bg,
                                            actor->data.label.wrap * w / 1000u);
            texture = SDL_CreateTextureFromSurface(info->renderer, surface);
            SDL_RenderCopy(info->renderer, texture, NULL, &rect);
            break;
        case UiDataTag_editText: {
            char buffer[KB(4)];
            snprintf(buffer,
                     sizeof(buffer),
                     "%s: %s",
                     actor->data.editText.label.buffer,
                     actor->data.editText.text.buffer);
            surface = TTF_RenderUTF8_Shaded_Wrapped(info->font,
                                                    buffer,
                                                    fg,
                                                    bg,
                                                    actor->data.editText.wrap *
                                                      w / 1000u);
            texture = SDL_CreateTextureFromSurface(info->renderer, surface);
            SDL_RenderCopy(info->renderer, texture, NULL, &rect);
        } break;
        case UiDataTag_slider: {
            const int w = rect.w;
            const int h = rect.h;
            const bool horizontal = w > h;

            SDL_SetRenderDrawColor(info->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(info->renderer, &rect);

            SDL_SetRenderDrawColor(info->renderer, fg.r, fg.g, fg.b, fg.a);
            rect.w /= 2;
            rect.h /= 2;
            rect.w = SDL_min(rect.w, rect.h);
            rect.h = rect.w;
            const float t = glm_percentc(actor->data.slider.min,
                                         actor->data.slider.max,
                                         actor->data.slider.value);
            if (horizontal) {
                rect.x = (int)floorf(glm_lerpc(rect.x, rect.x + w - rect.w, t));
                rect.y += rect.h / 2;
            } else {
                rect.x += rect.w / 2;
                rect.y = (int)floorf(glm_lerpc(rect.y, rect.y + h - rect.h, t));
            }
            SDL_RenderFillRect(info->renderer, &rect);
        } break;
        default:
            break;
    }
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

pUiActor
addUiLabel(pUiActorList list, const char* message, const uint32_t wrap)
{
    UiActor actor = { 0 };
    actor.data.tag = UiDataTag_label;
    actor.data.label.label = TemLangStringCreate(message, currentAllocator);
    actor.data.label.wrap = wrap;
    pUiActor rval = NULL;
    if (UiActorListAppend(list, &actor)) {
        rval = &list->buffer[list->used - 1UL];
    }
    UiActorFree(&actor);
    return rval;
}

pUiActor
addTextBox(pUiActorList list, const char* message, const uint32_t wrap)
{
    UiActor actor = { 0 };
    actor.data.tag = UiDataTag_editText;
    actor.data.editText.label = TemLangStringCreate(message, currentAllocator);
    actor.data.editText.text = TemLangStringCreate("", currentAllocator);
    actor.data.editText.wrap = wrap;
    pUiActor rval = NULL;
    if (UiActorListAppend(list, &actor)) {
        rval = &list->buffer[list->used - 1UL];
    }
    UiActorFree(&actor);
    return rval;
}

pUiActor
addSlider(pUiActorList list, const int32_t min, const int32_t max)
{
    UiActor actor = { 0 };
    actor.data.tag = UiDataTag_slider;
    actor.data.slider.min = min;
    actor.data.slider.max = max;
    actor.data.slider.value = max;
    pUiActor rval = NULL;
    if (UiActorListAppend(list, &actor)) {
        rval = &list->buffer[list->used - 1UL];
    }
    UiActorFree(&actor);
    return rval;
}

size_t
playbackCount(const AudioStatePtrList* list)
{
    size_t count = 0;
    for (size_t i = 0; i < list->used; ++i) {
        AudioStatePtr ptr = list->buffer[i];
        if (!ptr->isRecording) {
            ++count;
        }
    }
    return count;
}

UiActorList
getUiMenuActors(const Menu* menu)
{
    UiActorList list = { .allocator = currentAllocator };
    switch (menu->tag) {
        case MenuTag_Main: {
            IN_MUTEX(clientData.mutex, end, {
                int32_t nextId = 0;
                if (ServerConfigurationListIsEmpty(&clientData.allStreams)) {
                    pUiActor label =
                      addUiLabel(&list, "No streams available", 0u);
                    label->rect.x = 500;
                    label->rect.y = 100;
                    label->rect.w = 500;
                    label->rect.h = 100;
                    label->horizontal = HorizontalAlignment_Center;
                    label->vertical = VerticalAlignment_Top;
                    label->id = nextId++;
                    goto end;
                }
                {
                    pUiActor label = addUiLabel(&list, "Current streams", 0u);
                    label->rect.x = 500;
                    label->rect.y = 100;
                    label->rect.w = 500;
                    label->rect.h = 100;
                    label->horizontal = HorizontalAlignment_Center;
                    label->vertical = VerticalAlignment_Top;
                    label->id = nextId++;
                    label->type = MainButton_Label;

                    pUiActor hide = addUiLabel(&list, "X", 0u);
                    hide->rect.x = 1000;
                    hide->rect.y = 1000;
                    hide->rect.w = 125;
                    hide->rect.h = 125;
                    hide->horizontal = HorizontalAlignment_Right;
                    hide->vertical = VerticalAlignment_Bottom;
                    hide->id = nextId++;
                    hide->type = MainButton_Hide;
                }
                char buffer[KB(1)];
                const uint32_t total =
                  clientData.allStreams.used + playbackCount(&audioStates);
                const int size = SDL_min(100, 750 / total);

                const int width = 190;
                const int padding = 10;
                for (uint32_t i = 0; i < clientData.allStreams.used; ++i) {
                    const StreamDisplay* display = NULL;
                    pServerConfiguration config =
                      &clientData.allStreams.buffer[i];
                    const bool connected = GetStreamDisplayFromName(
                      &clientData.displays, &config->name, &display, NULL);
                    int x = padding / 2;
                    snprintf(
                      buffer,
                      sizeof(buffer),
                      "%s (%s)",
                      config->name.buffer,
                      ServerConfigurationDataTagToCharString(config->data.tag));
                    pUiActor streamName = addUiLabel(&list, buffer, 0u);
                    streamName->rect.x = x;
                    streamName->rect.y = 250 + (i * size);
                    streamName->rect.w = width;
                    streamName->rect.h = size * 9 / 10;
                    streamName->horizontal = HorizontalAlignment_Left;
                    streamName->vertical = VerticalAlignment_Top;
                    streamName->id = nextId++;
                    streamName->type = MainButton_Label;
                    streamName->userData = config;
                    x += width + padding;

                    pUiActor isConnected = addUiLabel(
                      &list, connected ? "Disconnect" : "Connect", 0u);
                    isConnected->rect.x = x;
                    isConnected->rect.y = 250 + (i * size);
                    isConnected->rect.w = width;
                    isConnected->rect.h = size * 9 / 10;
                    isConnected->horizontal = HorizontalAlignment_Left;
                    isConnected->vertical = VerticalAlignment_Top;
                    isConnected->id = nextId++;
                    isConnected->type = MainButton_Connect;
                    isConnected->userData = config;
                    x += width + padding;

                    if (!connected || display == NULL ||
                        !clientHasWriteAccess(&display->client, config)) {
                        continue;
                    }

                    pUiActor hide =
                      addUiLabel(&list, display->visible ? "Hide" : "Show", 0u);
                    hide->rect.x = x;
                    hide->rect.y = 250 + (i * size);
                    hide->rect.w = width;
                    hide->rect.h = size * 9 / 10;
                    hide->horizontal = HorizontalAlignment_Left;
                    hide->vertical = VerticalAlignment_Top;
                    hide->id = nextId++;
                    hide->type = MainButton_Visible;
                    hide->userData = config;
                    x += width + padding;

                    switch (config->data.tag) {
                        case ServerConfigurationDataTag_chat:
                        case ServerConfigurationDataTag_text: {
                            pUiActor t = addUiLabel(&list, "Send text", 0u);
                            t->rect.x = x;
                            t->rect.y = 250 + (i * size);
                            t->rect.w = width;
                            t->rect.h = size * 9 / 10;
                            t->horizontal = HorizontalAlignment_Left;
                            t->vertical = VerticalAlignment_Top;
                            t->id = nextId++;
                            t->type = MainButton_Data;
                            t->userData = config;
                        } break;
                        case ServerConfigurationDataTag_image: {
                            pUiActor t = addUiLabel(&list, "Send image", 0u);
                            t->rect.x = x;
                            t->rect.y = 250 + (i * size);
                            t->rect.w = width;
                            t->rect.h = size * 9 / 10;
                            t->horizontal = HorizontalAlignment_Left;
                            t->vertical = VerticalAlignment_Top;
                            t->id = nextId++;
                            t->type = MainButton_Data;
                            t->userData = config;
                        } break;
                        case ServerConfigurationDataTag_audio: {
                            pUiActor t = addUiLabel(
                              &list,
                              AudioStateFromGuid(
                                &audioStates, &display->id, true, NULL, NULL)
                                ? "Stop recording"
                                : "Start recording",
                              0u);
                            t->rect.x = x;
                            t->rect.y = 250 + (i * size);
                            t->rect.w = width;
                            t->rect.h = size * 9 / 10;
                            t->horizontal = HorizontalAlignment_Left;
                            t->vertical = VerticalAlignment_Top;
                            t->id = nextId++;
                            t->type = MainButton_Data;
                            t->userData = config;
                        } break;
                        default:
                            break;
                    }
                    x += width + padding;
                    if (display->visible) {
                        pUiActor t = addUiLabel(&list, "Screenshot", 0u);
                        t->rect.x = x;
                        t->rect.y = 250 + (i * size);
                        t->rect.w = width;
                        t->rect.h = size * 9 / 10;
                        t->horizontal = HorizontalAlignment_Left;
                        t->vertical = VerticalAlignment_Top;
                        t->id = nextId++;
                        t->type = MainButton_Screenshot;
                        t->userData = config;
                    }
                }
                for (size_t i = 0; i < audioStates.used; ++i) {
                    AudioStatePtr ptr = audioStates.buffer[i];
                    if (ptr->isRecording) {
                        continue;
                    }
                    const StreamDisplay* display = NULL;
                    if (!GetStreamDisplayFromGuid(
                          &clientData.displays, &ptr->id, &display, NULL)) {
                        continue;
                    }

                    const int y =
                      250 + ((clientData.allStreams.used + i) * size);
                    snprintf(buffer,
                             sizeof(buffer),
                             "%s playback\n(%d%%)",
                             display->config.name.buffer,
                             (int)floorf(ptr->volume * 100.f));
                    pUiActor label = addUiLabel(&list, buffer, 0u);
                    label->rect.x = 50;
                    label->rect.y = y;
                    label->rect.w = 200;
                    label->rect.h = size * 9 / 10;
                    label->horizontal = HorizontalAlignment_Left;
                    label->vertical = VerticalAlignment_Top;
                    label->id = nextId++;
                    label->type = MainButton_Label;
                    label->userData = ptr;

                    pUiActor slider = addSlider(&list, 0, 100);
                    slider->rect.x = 250;
                    slider->rect.y = y;
                    slider->rect.w = 700;
                    slider->rect.h = size * 9 / 10;
                    slider->horizontal = HorizontalAlignment_Left;
                    slider->vertical = VerticalAlignment_Top;
                    slider->id = nextId++;
                    slider->type = MainButton_Slider;
                    slider->data.slider.value = floorf(ptr->volume * 100.f);
                    slider->userData = ptr;
                }
            });
        } break;
        case MenuTag_EnterText: {
            const Guid* id = &menu->id;
            bool displayMissing = false;
            int32_t nextId = 0;
            USE_DISPLAY(clientData.mutex, end2, displayMissing, {
                pUiActor textbox =
                  addTextBox(&list, "Enter text to send\n", 0u);
                textbox->rect.x = 500;
                textbox->rect.y = 125;
                textbox->rect.w = 750;
                textbox->rect.h = 500;
                textbox->horizontal = HorizontalAlignment_Center;
                textbox->vertical = VerticalAlignment_Top;
                textbox->type = EnterTextButton_TextBox;
                textbox->id = nextId++;
                textbox->userData = &display->config;

                pUiActor sendButton = addUiLabel(&list, "Send", 0u);
                sendButton->rect.x = 125;
                sendButton->rect.y = 1000;
                sendButton->rect.w = 125;
                sendButton->rect.h = 125;
                sendButton->horizontal = HorizontalAlignment_Left;
                sendButton->vertical = VerticalAlignment_Bottom;
                sendButton->type = EnterTextButton_Send;
                sendButton->id = nextId++;
                sendButton->userData = &display->config;

                pUiActor backButton = addUiLabel(&list, "Back", 0u);
                backButton->rect.x = 875;
                backButton->rect.y = 1000;
                backButton->rect.w = 125;
                backButton->rect.h = 125;
                backButton->horizontal = HorizontalAlignment_Right;
                backButton->vertical = VerticalAlignment_Bottom;
                backButton->type = EnterTextButton_Back;
                backButton->id = nextId++;
                backButton->userData = &display->config;
            });
            if (displayMissing) {
                setUiMenu(MenuTag_Main);
            }
        } break;
        case MenuTag_EnterImage: {
            const Guid* id = &menu->id;
            bool displayMissing = false;
            int32_t nextId = 0;
            USE_DISPLAY(clientData.mutex, end52, displayMissing, {
                pUiActor textbox = addTextBox(&list, "Enter filename\n", 0u);
                textbox->rect.x = 500;
                textbox->rect.y = 125;
                textbox->rect.w = 750;
                textbox->rect.h = 500;
                textbox->horizontal = HorizontalAlignment_Center;
                textbox->vertical = VerticalAlignment_Top;
                textbox->type = EnterTextButton_TextBox;
                textbox->id = nextId++;
                textbox->userData = &display->config;

                pUiActor sendButton = addUiLabel(&list, "Send", 0u);
                sendButton->rect.x = 125;
                sendButton->rect.y = 1000;
                sendButton->rect.w = 125;
                sendButton->rect.h = 125;
                sendButton->horizontal = HorizontalAlignment_Left;
                sendButton->vertical = VerticalAlignment_Bottom;
                sendButton->type = EnterTextButton_Send;
                sendButton->id = nextId++;
                sendButton->userData = &display->config;

                pUiActor backButton = addUiLabel(&list, "Back", 0u);
                backButton->rect.x = 875;
                backButton->rect.y = 1000;
                backButton->rect.w = 125;
                backButton->rect.h = 125;
                backButton->horizontal = HorizontalAlignment_Right;
                backButton->vertical = VerticalAlignment_Bottom;
                backButton->type = EnterTextButton_Back;
                backButton->id = nextId++;
                backButton->userData = &display->config;
            });
            if (displayMissing) {
                setUiMenu(MenuTag_Main);
            }
        } break;
        case MenuTag_SendAudio: {
            const Guid* id = &menu->id;
            bool displayMissing = false;
            int32_t nextId = 0;
            const int32_t count = SDL_GetNumAudioDevices(SDL_TRUE);
            USE_DISPLAY(clientData.mutex, end5562, displayMissing, {
                for (; nextId < count; ++nextId) {
                    pUiActor actor = addUiLabel(
                      &list, SDL_GetAudioDeviceName(nextId, SDL_TRUE), 0U);
                    actor->rect.x = 100;
                    actor->rect.y = 250 + (nextId * 100);
                    actor->rect.w = 500;
                    actor->rect.h = 75;
                    actor->horizontal = HorizontalAlignment_Left;
                    actor->vertical = VerticalAlignment_Top;
                    actor->type = EnterTextButton_Send;
                    actor->id = nextId;
                    actor->userData = display;
                }

                pUiActor textbox = addTextBox(&list, "Select audio source", 0u);
                textbox->rect.x = 500;
                textbox->rect.y = 125;
                textbox->rect.w = 500;
                textbox->rect.h = 100;
                textbox->horizontal = HorizontalAlignment_Center;
                textbox->vertical = VerticalAlignment_Top;
                textbox->type = EnterTextButton_Label;
                textbox->id = nextId++;

                pUiActor backButton = addUiLabel(&list, "Back", 0u);
                backButton->rect.x = 875;
                backButton->rect.y = 1000;
                backButton->rect.w = 125;
                backButton->rect.h = 125;
                backButton->horizontal = HorizontalAlignment_Right;
                backButton->vertical = VerticalAlignment_Bottom;
                backButton->type = EnterTextButton_Back;
                backButton->id = nextId++;
            });
            if (displayMissing) {
                setUiMenu(MenuTag_Main);
            }
        } break;
        case MenuTag_SendVideo: {
            const Guid* id = &menu->id;
            bool displayMissing = false;
            int32_t nextId = 0;
            USE_DISPLAY(clientData.mutex, end_52, displayMissing, {
                pUiActor textbox = addTextBox(&list, "Select video source", 0u);
                textbox->rect.x = 500;
                textbox->rect.y = 125;
                textbox->rect.w = 500;
                textbox->rect.h = 100;
                textbox->horizontal = HorizontalAlignment_Center;
                textbox->vertical = VerticalAlignment_Top;
                textbox->type = EnterTextButton_TextBox;
                textbox->id = nextId++;
                textbox->userData = &display->config;

                pUiActor backButton = addUiLabel(&list, "Back", 0u);
                backButton->rect.x = 875;
                backButton->rect.y = 1000;
                backButton->rect.w = 125;
                backButton->rect.h = 125;
                backButton->horizontal = HorizontalAlignment_Right;
                backButton->vertical = VerticalAlignment_Bottom;
                backButton->type = EnterTextButton_Back;
                backButton->id = nextId++;
                backButton->userData = &display->config;
            });
            if (displayMissing) {
                setUiMenu(MenuTag_Main);
            }
        } break;
        default:
            break;
    }
    return list;
}