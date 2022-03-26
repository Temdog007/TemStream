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

#if USE_OPENCL
bool
rgbaToYuv(const uint8_t* data,
          int width,
          int height,
          void* ptrs[3],
          pOpenCLVideo vid)
{
    const size_t size = width * height;
    cl_int ret = clEnqueueWriteBuffer(vid->command_queue,
                                      vid->args[0],
                                      CL_TRUE,
                                      0,
                                      size * 4,
                                      data,
                                      0,
                                      NULL,
                                      NULL);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to write OpenCL argument buffer: %d\n", ret);
        return false;
    }

    ret = clEnqueueNDRangeKernel(
      vid->command_queue, vid->kernel, 1, NULL, &size, NULL, 0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to run OpenCL command: %d\n", ret);
        return false;
    }

    cl_event events[3] = { 0 };
    bool success = true;
    for (int i = 0; i < 3; ++i) {
        ret = clEnqueueReadBuffer(vid->command_queue,
                                  vid->args[i + 1],
                                  CL_FALSE,
                                  0,
                                  width * height / (i == 0 ? 1 : 4),
                                  ptrs[i],
                                  0,
                                  NULL,
                                  &events[i]);
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to read OpenCL buffer: %d\n", ret);
            success = false;
            break;
        }
    }
    if (success) {
        ret = clWaitForEvents(3, events);
        if (ret != CL_SUCCESS) {
            fprintf(stderr, "Failed to wait for OpenCL events: %d\n", ret);
            success = false;
        }
    }
    for (int i = 0; i < 3; ++i) {
        clReleaseEvent(events[i]);
    }
    return success;
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
    uint8_t* u = v + width * height / 4;
    const int halfWidth = width / 2;
    // static bool first = true;
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
                // if (first && index < 2000) {
                //     printf("%d (%d,%d)\n",
                //            index,
                //            index % halfWidth,
                //            index / halfWidth);
                // }
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
    // first = false;
    return true;
#endif
}
#endif