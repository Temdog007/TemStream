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
makeComputeShaderTextures(int width,
                          int height,
                          GLuint textures[4],
                          GLuint pbos[4])
{
    glDeleteBuffers(4, pbos);
    glGenBuffers(4, pbos);
    for (int i = 0; i < 4; ++i) {
        if (i == 3) {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
            glBufferData(
              GL_PIXEL_UNPACK_BUFFER, width * height * 4, NULL, GL_STREAM_DRAW);
            glMapBuffer(pbos[i], GL_STREAM_DRAW);
        } else {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
            glBufferData(
              GL_PIXEL_PACK_BUFFER, width * height, NULL, GL_STREAM_READ);
            glMapBuffer(pbos[i], GL_STREAM_READ);
        }
        // If reload, need to unmap buffers
        glUnmapBuffer(pbos[i]);
    }

    glDeleteTextures(4, textures);
    glGenTextures(4, textures);
    void* data = currentAllocator->allocate(width * height * 4);
    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        // first texture: input texture
        // seconds texture: Y texture
        // third and fourth texture: U/V textures
        switch (i) {
            case 0:
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA8,
                             width,
                             height,
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             data);
                glBindImageTexture(
                  i, textures[i], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
                break;
            case 1:
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_R8,
                             width,
                             height,
                             0,
                             GL_RED,
                             GL_UNSIGNED_BYTE,
                             data);
                glBindImageTexture(
                  i, textures[i], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
                break;
            default:
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_R8,
                             width / 2,
                             height / 2,
                             0,
                             GL_RED,
                             GL_UNSIGNED_BYTE,
                             data);
                glBindImageTexture(
                  i, textures[i], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
                break;
        }
    }
    currentAllocator->free(data);
}

void
rgbaToYuv(const void* imageData,
          const uint32_t pbo,
          GLuint prog,
          const int width,
          const int height)
{
    // GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    // GLint result;
    // do {
    //     glGetSynciv(sync, GL_SYNC_STATUS, sizeof(result), NULL, &result);
    //     SDL_Delay(0);
    // } while (result != GL_SIGNALED);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    void* rgba = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(rgba, imageData, width * height * 4);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glUseProgram(prog);
    glDispatchCompute(width, height, 1);

    // glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // glActiveTexture(GL_TEXTURE0 + 1);
    // glGetTexSubImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, y);
    // glActiveTexture(GL_TEXTURE0 + 2);
    // glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, u);
    // glActiveTexture(GL_TEXTURE0 + 3);
    // glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, v);
}
#else
bool
rgbaToYuv(const uint8_t* rgba,
          const int width,
          const int height,
          uint32_t* argb,
          uint8_t* yuv)
{
#if USE_SDL_CONVERSION
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
#else
    (void)argb;
    uint8_t* y = yuv;
    uint8_t* v = y + (width * height);
    uint8_t* u = v + ((width + 1) / 2 * (height + 1) / 2);
    const int halfWidth = width / 2;
    static bool first = true;
    for (int j = 0; j < height; ++j) {
        const uint8_t* rgbaPtr = rgba + width * j * 4;
        for (int i = 0; i < width; ++i) {
            uint8_t r = rgbaPtr[0];
            uint8_t g = rgbaPtr[1];
            uint8_t b = rgbaPtr[2];
            // uint8_t a = rgbaPtr[3];
            rgbaPtr += 4;

            *y = (uint8_t)((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            *y = SDL_clamp(*y, 0, 255);
            ++y;

            const int index = halfWidth * (j / 2) + (i / 2) % halfWidth;
            if (j % 2 == 0 && i % 2 == 0) {
                if (first && index < 2000) {
                    printf("%d (%d,%d)\n",
                           index,
                           index % halfWidth,
                           index / halfWidth);
                }
                u[index] =
                  (uint8_t)((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                u[index] = SDL_clamp(u[index], 0, 255);
            } else if (i % 2 == 0) {
                v[index] =
                  (uint8_t)((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                v[index] = SDL_clamp(v[index], 0, 255);
            }
        }
    }
    first = false;
    return true;
#endif
}
#endif