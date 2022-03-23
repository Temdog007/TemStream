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
                                      SDL_PIXELFORMAT_RGBA32,
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

#if USE_COMPUTE_SHADER
void
makeComputeShaderTextures(int width, int height, GLuint textures[4])
{
    glGenTextures(4, textures);
    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        // last texture is the input rgba texture
        if (i < 3) {
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RED,
                         width,
                         height,
                         0,
                         GL_RED,
                         GL_UNSIGNED_BYTE,
                         NULL);
            glBindImageTexture(
              i, textures[i], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RED);
        } else {
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         width,
                         height,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         NULL);
        }
    }
}

void
rgbaToYuv(const uint32_t* rgba,
          GLuint prog,
          const int width,
          const int height,
          GLuint textures[4],
          uint8_t* y,
          uint8_t* u,
          uint8_t* v)
{
    (void)textures;
    glUseProgram(prog);
    // Uniform should always be zero
    glActiveTexture(GL_TEXTURE0 + 3);
    glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glUniform1i(0, 3);
    glDispatchCompute(width, height, 1);

    glActiveTexture(GL_TEXTURE0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, y);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, u);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, v);
}
#else
bool
rgbaToYuv(const uint32_t* rgba,
          const int width,
          const int height,
          uint32_t* argb,
          uint8_t* yuv)
{
    // for (size_t i = 0, n = width * height; i < n; ++i) {
    //     const uint32_t r = rgba[i] >> 24;
    //     const uint32_t g = rgba[i] >> 16;
    //     const uint32_t b = rgba[i] >> 8;
    //     const uint32_t a = rgba[i] >> 0;
    //     argb[i] = (a << 24) | (r << 16) | (g << 8) | b;
    // }
    return SDL_ConvertPixels(width,
                             height,
                             SDL_PIXELFORMAT_RGBA32,
                             rgba,
                             width * 4,
                             SDL_PIXELFORMAT_ARGB8888,
                             argb,
                             width * 4) == 0 &&
           SDL_ConvertPixels(width,
                             height,
                             SDL_PIXELFORMAT_ARGB8888,
                             argb,
                             width * 4,
                             SDL_PIXELFORMAT_YV12,
                             yuv,
                             width) == 0;
}
#endif