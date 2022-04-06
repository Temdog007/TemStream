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

size_t
updateUiActors(const SDL_Event* e,
               SDL_Window* window,
               UiActor* actor,
               const size_t s)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    size_t r = 0;
    for (size_t i = 0; i < s; ++i) {
        if (updateUiActor(e, &actor[i], w, h)) {
            ++r;
        }
    }
    return r;
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
checkActorFocus(SDL_FPoint point, pUiActor actor, const float w, const float h)
{
    const SDL_FRect rect = getUiActorRect(actor, w, h);
    const SDL_bool inRect = SDL_PointInFRect(&point, &rect);
    const bool changed = actor->hasFocus != inRect;
    actor->hasFocus = inRect;
    if (changed && actorNeedsText(actor)) {
        if (actor->hasFocus) {
            SDL_StartTextInput();
        } else {
            SDL_StopTextInput();
        }
    }
    return changed;
}

bool
updateEditText(const SDL_Event* e, pUiActor actor, const float w, const float h)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONDOWN: {
            SDL_FPoint point = { .x = e->button.x, .y = e->button.y };
            return checkActorFocus(point, actor, w, h);
        } break;
        case SDL_MOUSEMOTION: {
            SDL_FPoint point = { .x = e->motion.x, .y = e->motion.y };
            return checkActorFocus(point, actor, w, h);
        } break;
        case SDL_KEYDOWN:
            switch (e->key.keysym.sym) {
                case SDLK_BACKSPACE:
                case SDLK_DELETE:
                    if (e->key.keysym.mod & KMOD_CTRL) {
                        actor->data.editText.used = 0;
                    } else {
                        TemLangStringPop(&actor->data.editText);
                    }
                    return true;
                default:
                    break;
            }
            break;
        case SDL_TEXTINPUT:
            if (!actor->hasFocus) {
                break;
            }
            return TemLangStringAppendChars(&actor->data.editText,
                                            e->text.text);
        default:
            break;
    }
    return false;
}

bool
updateNumberRangeFromPoint(pUiActor actor,
                           const float w,
                           const float h,
                           const SDL_FPoint point)
{
    const bool changed = checkActorFocus(point, actor, w, h);
    const uint32_t state = SDL_GetMouseState(NULL, NULL);
    if (changed && actor->hasFocus && state != 0) {
        const SDL_FRect rect = getUiActorRect(actor, w, h);
        const bool horizontal = actor->rect.w > actor->rect.h;
        const float t = horizontal
                          ? glm_percentc(rect.x, rect.x + rect.w, point.x)
                          : glm_percentc(rect.y, rect.y + rect.h, point.y);
        actor->data.numberRange.value = (int32_t)glm_lerpc(
          actor->data.numberRange.min, actor->data.numberRange.max, t);
    }
    return changed;
}

bool
updateNumberRange(const SDL_Event* e,
                  pUiActor actor,
                  const float w,
                  const float h)
{
    switch (e->type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            const SDL_FPoint point = { .x = e->button.x, .y = e->button.y };
            return updateNumberRangeFromPoint(actor, w, h, point);
        } break;
        case SDL_MOUSEMOTION: {
            const SDL_FPoint point = { .x = e->motion.x, .y = e->motion.y };
            return updateNumberRangeFromPoint(actor, w, h, point);
        } break;
        default:
            break;
    }
    return false;
}

bool
updateUiActor(const SDL_Event* e, pUiActor actor, const float w, const float h)
{
    switch (actor->data.tag) {
        case UiDataTag_editText:
            return updateEditText(e, actor, w, h);
        case UiDataTag_numberRange:
            return updateNumberRange(e, actor, w, h);
        default:
            return false;
    }
}

void
renderUiActors(SDL_Window* window,
               SDL_Renderer* renderer,
               TTF_Font* ttfFont,
               UiActor* actor,
               const size_t s)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    for (size_t i = 0; i < s; ++i) {
        const SDL_FRect rect = getUiActorRect(&actor[i], w, h);
        renderUiActor(renderer, ttfFont, &actor[i], rect);
    }
}

void
renderUiActor(SDL_Renderer* renderer,
              TTF_Font* font,
              UiActor* actor,
              SDL_FRect rect)
{
    const SDL_Color fg = { 255u, 255u, 255u, 255u };
    const SDL_Color highlight = { 255u, 255u, 0u, 255u };
    const SDL_Color bg = { 0u, 0u, 0u, 255u };
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;

    switch (actor->data.tag) {
        case UiDataTag_label:
            surface = TTF_RenderUTF8_Shaded_Wrapped(
              font, actor->data.label.buffer, fg, bg, 0);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderCopyF(renderer, texture, NULL, &rect);
            break;
        case UiDataTag_editText:
            surface =
              TTF_RenderUTF8_Shaded_Wrapped(font,
                                            actor->data.editText.buffer,
                                            actor->hasFocus ? highlight : fg,
                                            bg,
                                            0);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderCopyF(renderer, texture, NULL, &rect);
            break;
        case UiDataTag_numberRange: {
            const bool horizontal = rect.w > rect.h;
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderDrawRectF(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, fg.r, fg.g, fg.b, fg.a);
            rect.w *= 0.5f;
            rect.h *= 0.5f;
            rect.w = SDL_min(rect.w, rect.h);
            rect.h = rect.w;
            const float t = glm_percentc(actor->data.numberRange.min,
                                         actor->data.numberRange.max - rect.w,
                                         actor->data.numberRange.value);
            if (horizontal) {
                rect.x = glm_lerpc(actor->data.numberRange.min,
                                   actor->data.numberRange.max - rect.w,
                                   t);
            } else {
                rect.y = glm_lerpc(actor->data.numberRange.min,
                                   actor->data.numberRange.max - rect.w,
                                   t);
            }
            SDL_RenderDrawRectF(renderer, &rect);
        } break;
        default:
            break;
    }
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}