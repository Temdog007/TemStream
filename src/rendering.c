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