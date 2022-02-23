#include <include/main.h>

#define FONT_TEXTURE_WIDTH 1024
#define FONT_TEXTURE_HEIGHT 1024

#define MAX_UTF32_CHARACTERS 0x10FFFFU

void
FontFree(pFont font)
{
    SDL_DestroyTexture(font->texture);
    CharacterListFree(&font->characters);
}

bool
loadFont(const char* filename,
         const FT_UInt fontSize,
         SDL_Renderer* renderer,
         pFont font)
{
    bool result = false;
    FT_Library ft = NULL;
    FT_Face face = NULL;

    uint8_t* dst =
      currentAllocator->allocate(FONT_TEXTURE_WIDTH * FONT_TEXTURE_HEIGHT * 4);

    FontFree(font);
    font->characters.allocator = currentAllocator;

    font->texture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_STATIC,
                                      FONT_TEXTURE_WIDTH,
                                      FONT_TEXTURE_HEIGHT);
    SDL_SetTextureBlendMode(font->texture, SDL_BLENDMODE_BLEND);

    if (font->texture == NULL) {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        goto cleanup;
    }

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Failed to init Freetype Library\n");
        goto cleanup;
    }

    if (FT_New_Face(ft, filename, 0, &face)) {
        fprintf(stderr, "Failed to load font\n");
        goto cleanup;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    int32_t x = 0;
    int32_t y = fontSize;
    uint32_t c = 0;
    for (; c < MAX_UTF32_CHARACTERS; ++c) {
        Character character = { 0 };
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            fprintf(stderr,
                    "Failed to load character %u"
                    "\n",
                    c);
            goto addChar;
        }

        const uint32_t width = face->glyph->bitmap.width;
        const uint32_t height = face->glyph->bitmap.rows;
        if (x + width >= FONT_TEXTURE_WIDTH) {
            x = 0;
            y += fontSize;
        }
        if (y + fontSize >= FONT_TEXTURE_HEIGHT) {
            break;
        }
        const uint8_t* buffer = (uint8_t*)face->glyph->bitmap.buffer;
        if (buffer == NULL) {
            goto addChar;
        }
        size_t j = 0;
        for (size_t i = 0; i < width * height; ++i) {
            dst[j] = dst[j + 1] = dst[j + 2] = dst[j + 3] = buffer[i];
            j += 4;
        }
        character =
          (Character){ .rect = { .x = x, .y = y, .w = width, .h = height },
                       .size = { width, height },
                       .bearing = { face->glyph->bitmap_left,
                                    face->glyph->bitmap_top },
                       .advance = face->glyph->advance.x };
        if (SDL_UpdateTexture(font->texture, &character.rect, dst, width * 4) !=
            0) {
            fprintf(stderr, "Failed to update texture: %s\n", SDL_GetError());
            goto cleanup;
        }
        x += width + 5;
    addChar:
        CharacterListAppend(&font->characters, &character);
    }

#if _DEBUG
    printf("Loaded %u characters from font\n", c);
#endif
    result = true;

cleanup:
    currentAllocator->free(dst);
    if (face != NULL) {
        FT_Done_Face(face);
    }
    if (ft != NULL) {
        FT_Done_FreeType(ft);
    }
    return true;
}

SDL_FRect
renderFont(SDL_Renderer* renderer,
           pFont font,
           const char* text,
           float x,
           const float y,
           const float scale,
           const uint8_t foreground[4],
           const uint8_t background[4])
{
    SDL_FRect totalRect = { .x = x, .y = 0, .w = 0, .h = 0 };
    if (background != NULL) {
        const char* copy = text;
        while (*copy != '\0') {
            const Character c = font->characters.buffer[(int)*copy];
            totalRect.h = SDL_max(totalRect.h, c.size[1] * scale);
            totalRect.w += (c.advance >> 6) * scale;
            ++copy;
        }
        SDL_SetRenderDrawColor(
          renderer, background[0], background[1], background[2], background[3]);
        SDL_RenderFillRectF(renderer, &totalRect);
    }
    if (foreground) {
        SDL_SetTextureColorMod(
          font->texture, foreground[0], foreground[1], foreground[2]);
        SDL_SetTextureAlphaMod(font->texture, foreground[3]);
    } else {
        SDL_SetTextureColorMod(font->texture, 0xffu, 0xffu, 0xffu);
        SDL_SetTextureAlphaMod(font->texture, 0xffu);
    }
    while (*text != '\0') {
        const Character c = font->characters.buffer[(int)*text];
        const SDL_FRect rect = { .x = x + c.bearing[0] * scale,
                                 .y = y - (c.size[1] - c.bearing[1]) * scale,
                                 .w = c.size[0] * scale,
                                 .h = c.size[1] * scale };
        if (SDL_RenderCopyF(renderer, font->texture, &c.rect, &rect) != 0) {
            fprintf(
              stderr, "Failed to render text character: %s\n", SDL_GetError());
            break;
        }
        x += (c.advance >> 6) * scale;
        ++text;
    }
    return totalRect;
}