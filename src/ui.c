#include <include/main.h>

SDL_FRect
getUiActorRect(const UiActor* actor, const float w, const float h)
{
    SDL_FRect rect = { .x = actor->rect.x * w,
                       .y = actor->rect.y * h,
                       .w = actor->rect.w * w,
                       .h = actor->rect.h * h };
    switch (actor->horizontal) {
        case HorizontalAlignment_Center:
            rect.x -= rect.w * 0.5f;
            break;
        case HorizontalAlignment_Right:
            rect.x -= rect.w;
            break;
        default:
            break;
    }
    switch (actor->vertical) {
        case VerticalAlignment_Center:
            rect.y -= rect.h * 0.5f;
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
updateUiActors(const SDL_Event* e,
               SDL_Window* window,
               UiActor* actor,
               const size_t s,
               uint32_t* focusId)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    for (size_t i = 0; i < s; ++i) {
        updateUiActor(e, &actor[i], w, h, focusId);
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
checkActorFocus(const SDL_FPoint* point,
                pUiActor actor,
                const float w,
                const float h,
                uint32_t* focusId)
{
    const SDL_FRect rect = getUiActorRect(actor, w, h);
    const SDL_bool inRect = SDL_PointInFRect(point, &rect);
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
uiUpdate(const int32_t code, const uint32_t focusId)
{
    SDL_Event ev = { .user = { .type = SDL_USEREVENT,
                               .code = code,
                               .data1 = (void*)(size_t)focusId } };
    SDL_PushEvent(&ev);
}

void
updateEditText(const SDL_Event* e,
               pUiActor actor,
               const float w,
               const float h,
               uint32_t* focusId)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONUP: {
            SDL_FPoint point = { .x = e->button.x, .y = e->button.y };
            checkActorFocus(&point, actor, w, h, focusId);
            uiUpdate(*focusId == actor->id ? CustomEvent_UiClicked
                                           : CustomEvent_UiChanged,
                     *focusId);
        } break;
        case SDL_MOUSEMOTION: {
            SDL_FPoint point = { .x = e->motion.x, .y = e->motion.y };
            if (checkActorFocus(&point, actor, w, h, focusId)) {
                uiUpdate(CustomEvent_UiChanged, *focusId);
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
                    uiUpdate(CustomEvent_UiChanged, *focusId);
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
                    uiUpdate(CustomEvent_UiClicked, *focusId);
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
            uiUpdate(CustomEvent_UiChanged, *focusId);
            break;
        default:
            break;
    }
}

bool
updateSliderFromPoint(pUiActor actor,
                      const float w,
                      const float h,
                      const SDL_FPoint* point,
                      uint32_t* focusId)
{
    const bool changed = checkActorFocus(point, actor, w, h, focusId);
    if (changed && actor->id == *focusId &&
        (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0) {
        const SDL_FRect rect = getUiActorRect(actor, w, h);
        const bool horizontal = actor->rect.w > actor->rect.h;
        const float t = horizontal
                          ? glm_percentc(rect.x, rect.x + rect.w, point->x)
                          : glm_percentc(rect.y, rect.y + rect.h, point->y);
        actor->data.slider.value =
          (int32_t)glm_lerpc(actor->data.slider.min, actor->data.slider.max, t);
    }
    return changed;
}

bool
updateSlider(const SDL_Event* e,
             pUiActor actor,
             const float w,
             const float h,
             uint32_t* focusId)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONUP: {
            const SDL_FPoint point = { .x = e->button.x, .y = e->button.y };
            return updateSliderFromPoint(actor, w, h, &point, focusId);
        } break;
        case SDL_MOUSEMOTION: {
            const SDL_FPoint point = { .x = e->motion.x, .y = e->motion.y };
            return updateSliderFromPoint(actor, w, h, &point, focusId);
        } break;
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
            uint32_t* focusId)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONUP: {
            SDL_FPoint point = { .x = e->button.x, .y = e->button.y };
            checkActorFocus(&point, actor, w, h, focusId);
            uiUpdate(*focusId == actor->id ? CustomEvent_UiClicked
                                           : CustomEvent_UiChanged,
                     *focusId);
        } break;
        case SDL_MOUSEMOTION: {
            SDL_FPoint point = { .x = e->motion.x, .y = e->motion.y };
            if (checkActorFocus(&point, actor, w, h, focusId)) {
                uiUpdate(CustomEvent_UiChanged, *focusId);
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
              uint32_t* focusId)
{
    switch (actor->data.tag) {
        case UiDataTag_editText:
            updateEditText(e, actor, w, h, focusId);
            break;
        case UiDataTag_slider:
            updateSlider(e, actor, w, h, focusId);
            break;
        case UiDataTag_label:
            updateLabel(e, actor, w, h, focusId);
            break;
        default:
            break;
    }
}

void
renderUiActors(SDL_Window* window,
               SDL_Renderer* renderer,
               TTF_Font* ttfFont,
               UiActor* actor,
               const size_t s,
               const uint32_t focusId)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    for (size_t i = 0; i < s; ++i) {
        const SDL_FRect rect = getUiActorRect(&actor[i], w, h);
        renderUiActor(renderer, ttfFont, &actor[i], rect, focusId);
    }
}

void
renderUiActor(SDL_Renderer* renderer,
              TTF_Font* font,
              UiActor* actor,
              SDL_FRect rect,
              const uint32_t focusId)
{
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;

    SDL_Color fg;
    SDL_Color bg;
    if (actor->id == focusId) {
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
            surface = TTF_RenderUTF8_Shaded_Wrapped(
              font, actor->data.label.buffer, fg, bg, 0);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderCopyF(renderer, texture, NULL, &rect);
            break;
        case UiDataTag_editText: {
            char buffer[KB(4)];
            snprintf(buffer,
                     sizeof(buffer),
                     "%s: %s",
                     actor->data.editText.label.buffer,
                     actor->data.editText.text.buffer);
            surface = TTF_RenderUTF8_Shaded_Wrapped(font, buffer, fg, bg, 0);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderCopyF(renderer, texture, NULL, &rect);
        } break;
        case UiDataTag_slider: {
            const bool horizontal = rect.w > rect.h;
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderDrawRectF(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, fg.r, fg.g, fg.b, fg.a);
            rect.w *= 0.5f;
            rect.h *= 0.5f;
            rect.w = SDL_min(rect.w, rect.h);
            rect.h = rect.w;
            const float t = glm_percentc(actor->data.slider.min,
                                         actor->data.slider.max - rect.w,
                                         actor->data.slider.value);
            if (horizontal) {
                rect.x = glm_lerpc(
                  actor->data.slider.min, actor->data.slider.max - rect.w, t);
            } else {
                rect.y = glm_lerpc(
                  actor->data.slider.min, actor->data.slider.max - rect.w, t);
            }
            SDL_RenderDrawRectF(renderer, &rect);
        } break;
        default:
            break;
    }
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}