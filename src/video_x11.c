#include <include/main.h>

bool
isDesktopWindow(xcb_connection_t*, xcb_window_t);

void
getDesktopWindows(xcb_connection_t*, xcb_window_t, pWindowDataList);

int screenRecordThread(pWindowData);

bool
startWindowRecording(const Guid* id, const struct pollfd inputfd, pBytes bytes)
{
    bool result = false;

    int screenNum;
    xcb_connection_t* con = xcb_connect(NULL, &screenNum);
    WindowDataList list = { .allocator = currentAllocator };

    if (xcb_connection_has_error(con)) {
        fputs("Cannot open display\n", stderr);
        goto end;
    }

    const xcb_setup_t* setup = xcb_get_setup(con);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);

    for (int i = 0; i < screenNum; ++i) {
        xcb_screen_next(&iter);
    }

    xcb_screen_t* screen = iter.data;
    const xcb_window_t root = screen->root;

    getDesktopWindows(con, root, &list);

    if (WindowDataListIsEmpty(&list)) {
        fputs("No windows to record were found\n", stderr);
        goto end;
    }

    askQuestion("Select a window to record");
    for (size_t i = 0; i < list.used; ++i) {
        const WindowData* win = &list.buffer[i];
        printf("%zu) %s\n", i + 1, win->name.buffer);
    }

    uint32_t selected = 0;
    if (list.used == 1) {
        puts("Using only available window");
    } else {
        if (getIndexFromUser(inputfd, bytes, list.used, &selected, true) !=
            UserInputResult_Input) {
            puts("Canceling window record");
            goto end;
        }
    }

    askQuestion("Enter FPS");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto end;
    }
    const uint16_t fps =
      SDL_clamp(strtoul((char*)bytes->buffer, NULL, 10), 1u, 1000u);
    printf("FPS set to: %u\n", fps);

#if USE_VP8
    askQuestion("Enter key frame interval");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto end;
    }
    const uint16_t keyInterval =
      SDL_clamp(strtoul((char*)bytes->buffer, NULL, 10), 1u, 1000u);
    printf("Key frame interval set to: %u\n", keyInterval);
#else
    const uint16_t keyInterval = 0;
#endif

    askQuestion("Enter bitrate (in kilobits per second)");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto end;
    }
    const uint32_t bitrate =
      SDL_clamp(strtoul((char*)bytes->buffer, NULL, 10), 100, 10000);
    printf("Bitrate set to: %u\n", bitrate);

    pWindowData win = currentAllocator->allocate(sizeof(WindowData));
    WindowDataCopy(win, &list.buffer[selected], currentAllocator);
    win->id = *id;
    win->fps = fps;
    win->keyFrameInterval = keyInterval;
    win->bitrate = bitrate;

    SDL_Thread* thread =
      SDL_CreateThread((SDL_ThreadFunction)screenRecordThread, "video", win);
    if (thread == NULL) {
        fprintf(stderr, "Failed to start thread: %s\n", SDL_GetError());
        WindowDataFree(win);
        currentAllocator->free(win);
        goto end;
    }

    SDL_DetachThread(thread);
    SDL_AtomicIncRef(&runningThreads);
    con = NULL;

    result = true;
end:
    WindowDataListFree(&list);
    xcb_disconnect(con);
    return result;
}

bool
isDesktopWindow(xcb_connection_t* con, const xcb_window_t window)
{
    bool isDesktop = false;
    xcb_generic_error_t* error = NULL;

    xcb_atom_t atom = { 0 };
    {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(
          con, 1, sizeof("_NET_WM_WINDOW_TYPE") - 1, "_NET_WM_WINDOW_TYPE");
        xcb_intern_atom_reply_t* reply =
          xcb_intern_atom_reply(con, cookie, &error);
        if (error) {
            // fprintf(stderr, "XCB Error: %d\n", error->error_code);
            free(error);
            if (reply) {
                free(reply);
            }
            goto end;
        }
        if (!reply) {
            goto end;
        }
        atom = reply->atom;
        free(reply);
    }

    {
        xcb_get_property_cookie_t cookie = xcb_get_property(
          con, 0, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, ~0U);
        xcb_get_property_reply_t* reply =
          xcb_get_property_reply(con, cookie, &error);
        if (error) {
            // fprintf(stderr, "XCB Error: %d\n", error->error_code);
            free(error);
            if (reply) {
                free(reply);
            }
            goto end;
        }
        if (!reply) {
            goto end;
        }

        const int len = xcb_get_property_value_length(reply);
        if (len <= 0) {
            free(reply);
            goto end;
        }

        xcb_atom_t* names = xcb_get_property_value(reply);
        const int count = len / sizeof(xcb_atom_t);
        for (int i = 0; i < count && !isDesktop; ++i) {
            xcb_get_atom_name_cookie_t cookie =
              xcb_get_atom_name(con, names[i]);
            xcb_get_atom_name_reply_t* reply =
              xcb_get_atom_name_reply(con, cookie, &error);
            if (error) {
                // fprintf(stderr, "XCB Error: %d\n", error->error_code);
                free(error);
                if (reply) {
                    free(reply);
                }
                continue;
            }
            if (!reply) {
                continue;
            }
            const int len = xcb_get_atom_name_name_length(reply);
            if (len > 0) {
                char* str = xcb_get_atom_name_name(reply);
                isDesktop =
                  strncmp("_NET_WM_WINDOW_TYPE_NORMAL", str, len) == 0;
            }
            free(reply);
        }
        free(reply);
    }
end:
    return isDesktop;
}

void
getDesktopWindows(xcb_connection_t* con,
                  const xcb_window_t window,
                  pWindowDataList list)
{
    xcb_generic_error_t* error = NULL;
    if (isDesktopWindow(con, window)) {
        WindowData data = { .connection = con, .windowId = window };
        {
            xcb_get_geometry_cookie_t cookie = xcb_get_geometry(con, window);
            xcb_get_geometry_reply_t* geom =
              xcb_get_geometry_reply(con, cookie, &error);
            if (error) {
                free(error);
                if (geom) {
                    free(geom);
                }
                goto endWin;
            }
            if (!geom) {
                goto endWin;
            }
            data.width = geom->width - (geom->width % 2);
            data.height = geom->height - (geom->height % 2);
            free(geom);
        }
        {
            xcb_atom_t atom = { 0 };
            {
                xcb_intern_atom_cookie_t cookie = xcb_intern_atom(
                  con, 1, sizeof("_NET_WM_NAME") - 1, "_NET_WM_NAME");
                xcb_intern_atom_reply_t* reply =
                  xcb_intern_atom_reply(con, cookie, &error);
                if (error) {
                    free(error);
                    if (reply) {
                        free(reply);
                    }
                    goto endWin;
                }
                if (!reply) {
                    goto endWin;
                }
                atom = reply->atom;
                free(reply);
            }
            xcb_get_property_cookie_t cookie = xcb_get_property(
              con, 0, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, ~0U);
            xcb_get_property_reply_t* reply =
              xcb_get_property_reply(con, cookie, &error);
            if (error) {
                free(error);
                if (reply) {
                    free(reply);
                }
                goto endWin;
            }
            if (!reply) {
                goto endWin;
            }
            const int len = xcb_get_property_value_length(reply);
            if (len > 0) {
                const char* name = (char*)xcb_get_property_value(reply);
                data.name =
                  TemLangStringCreateFromSize(name, len, currentAllocator);
            }
            WindowDataListAppend(list, &data);
            free(reply);
        }
    endWin:
        WindowDataFree(&data);
    }

    xcb_query_tree_cookie_t cookie = xcb_query_tree(con, window);
    xcb_query_tree_reply_t* reply = xcb_query_tree_reply(con, cookie, &error);
    if (error) {
        printf("XCB Error: %d\n", error->error_code);
        free(error);
    }
    if (reply) {
        const int childrenLen = xcb_query_tree_children_length(reply);
        xcb_window_t* windows = xcb_query_tree_children(reply);
        for (int i = 0; i < childrenLen; ++i) {
            getDesktopWindows(con, windows[i], list);
        }
        free(reply);
    }
}

#if USE_VP8
vpx_codec_iface_t*
codec_encoder_interface()
{
    return vpx_codec_vp8_cx();
}

vpx_codec_iface_t*
codec_decoder_interface()
{
    return vpx_codec_vp8_dx();
}
#endif

#if USE_COMPUTE_SHADER
bool
compileShader(const char* str, GLuint* progPtr)
{
    bool success = false;
    GLuint prog = glCreateProgram();
    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &str, NULL);
    glCompileShader(cs);
    {
        int result = 0;
        glGetShaderiv(cs, GL_COMPILE_STATUS, &result);
        if (!result) {
            GLchar log[KB(5)];
            glGetShaderInfoLog(cs, sizeof(log), &result, log);
            fprintf(stderr, "Failed to compiler compute shader: %s\n", log);
            goto end;
        }
    }
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    {
        int result;
        glGetProgramiv(prog, GL_LINK_STATUS, &result);
        if (!result) {
            GLchar log[KB(5)];
            glGetProgramInfoLog(prog, sizeof(log), &result, log);
            fprintf(stderr, "Failed to make compiler: %s\n", log);
            goto end;
        }
    }

    *progPtr = prog;
    success = true;
end:
    glDeleteShader(cs);
    return success;
}
const char*
glSourceString(GLenum source)
{
    switch (source) {
        case GL_DEBUG_SOURCE_API:
            return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            return "Window System";
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            return "Shader Compiler";
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            return "Third Party";
        case GL_DEBUG_SOURCE_APPLICATION:
            return "Application";
        default:
            return "Unknown";
    }
}
const char*
glTypeString(GLenum type)
{
    switch (type) {
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            return "Deprecated Behavoir";
        case GL_DEBUG_TYPE_ERROR:
            return "Error";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            return "Undefined Behavoir";
        case GL_DEBUG_TYPE_PORTABILITY:
            return "Portability";
        case GL_DEBUG_TYPE_PERFORMANCE:
            return "Performance";
        case GL_DEBUG_TYPE_MARKER:
            return "Marker";
        case GL_DEBUG_TYPE_PUSH_GROUP:
            return "Push Group";
        case GL_DEBUG_TYPE_POP_GROUP:
            return "Pop Group";
        case GL_DEBUG_TYPE_OTHER:
            return "Other";
        default:
            return "Unknown";
    }
}
const char*
glSeverityString(GLenum severity)
{
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            return "High";
        case GL_DEBUG_SEVERITY_MEDIUM:
            return "Medium";
        case GL_DEBUG_SEVERITY_LOW:
            return "Low";
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "Notification";
        default:
            return "Unknown";
    }
}
void GLAPIENTRY
OnGLMessage(GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar* message,
            const void* userParam)
{
    (void)length;
    (void)userParam;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }
    fprintf(
      stderr,
      "GL Callback %u: source=%s, type = %s, serverity = %s, message = %s\n",
      id,
      glSourceString(source),
      glTypeString(type),
      glSeverityString(severity),
      message);
}
#define RGBA_TO_YUV                                                            \
    rgbaToYuv(imageData, pbos[3], prog, data->width, data->height);            \
    for (int i = 0; i < 3; ++i) {                                              \
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);                           \
        glActiveTexture(GL_TEXTURE1 + i);                                      \
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);       \
    }                                                                          \
    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);               \
    GLint result;                                                              \
    glGetSynciv(sync, GL_SYNC_STATUS, sizeof(result), NULL, &result);          \
    for (int t = 0; t < 10 && result != GL_SIGNALED; ++t) {                    \
        SDL_Delay(0);                                                          \
        glGetSynciv(sync, GL_SYNC_STATUS, sizeof(result), NULL, &result);      \
    }                                                                          \
    for (int i = 0; i < 3; ++i) {                                              \
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);                           \
        ptrs[i] = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);             \
    }                                                                          \
    success = ptrs[0] != NULL && ptrs[1] != NULL && ptrs[2] != NULL;
#define ENCODE_TO_H264                                                         \
    encoded = h264_encode_separate(encoder,                                    \
                                   Y,                                          \
                                   U,                                          \
                                   V,                                          \
                                   data->width,                                \
                                   data->height,                               \
                                   message.video.buffer,                       \
                                   message.video.size)
#else
#define RGBA_TO_YUV                                                            \
    success = rgbaToYuv(imageData, data->width, data->height, ARGB, YUV)
#define ENCODE_TO_H264                                                         \
    encoded = h264_encode(encoder,                                             \
                          YUV,                                                 \
                          data->width,                                         \
                          data->height,                                        \
                          message.video.buffer,                                \
                          message.video.size)
#endif

#if USE_VP8
bool
initEncoder(vpx_codec_ctx_t* codec, vpx_image_t* img, const WindowData* data)
{
    vpx_img_free(img);
    vpx_codec_destroy(codec);

    vpx_codec_enc_cfg_t cfg = { 0 };

    if (vpx_img_alloc(img, VPX_IMG_FMT_I420, data->width, data->height, 1) ==
        NULL) {
        fprintf(stderr, "Failed to allocate image\n");
        return false;
    }

#if _DEBUG
    printf("Using encoder: %s with %d threads\n",
           vpx_codec_iface_name(codec_encoder_interface()),
           SDL_GetCPUCount());
#endif

    vpx_codec_err_t res =
      vpx_codec_enc_config_default(codec_encoder_interface(), &cfg, 0);
    if (res) {
        fprintf(stderr,
                "Failed to get default video encoder configuration: %s\n",
                vpx_codec_err_to_string(res));
        return false;
    }

    cfg.g_w = data->width;
    cfg.g_h = data->height;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = data->fps;
    cfg.rc_target_bitrate = data->bitrate;
    cfg.g_threads = SDL_GetCPUCount();
    cfg.g_error_resilient =
      VPX_ERROR_RESILIENT_DEFAULT | VPX_ERROR_RESILIENT_PARTITIONS;

    if (vpx_codec_enc_init(codec, codec_encoder_interface(), &cfg, 0) != 0) {
        fprintf(stderr, "Failed to initialize encoder\n");
        return false;
    }
    return true;
}
#else
bool
initEncoder(void** encoder, const WindowData* data)
{
    destroy_h264_encoder(*encoder);
    if (!create_h264_encoder(encoder,
                             data->width,
                             data->height,
                             data->fps,
                             data->bitrate,
                             SDL_GetCPUCount())) {
        fprintf(stderr, "Failed to initalize encoder\n");
        return false;
    }

    return true;
}
#endif

bool
sendWindowSize(const uint16_t width, const uint16_t height, const Guid* id)
{
    bool displayMissing = false;
    uint8_t buffer[KB(1)];
    Bytes bytes = { .buffer = buffer, .size = sizeof(buffer), .used = 0 };
    VideoMessage message = { .size = { width, height },
                             .tag = VideoMessageTag_size };
    MESSAGE_SERIALIZE(VideoMessage, message, bytes);
    ENetPacket* packet =
      BytesToPacket(bytes.buffer, bytes.used, SendFlags_Normal);
    USE_DISPLAY(clientData.mutex, end3, displayMissing, {
        NullValueListAppend(&display->outgoing, (NullValue)&packet);
    });
    if (displayMissing) {
        enet_packet_destroy(packet);
    }
    return displayMissing;
}

int
screenRecordThread(pWindowData data)
{
    const Guid* id = &data->id;
    bool displayMissing = sendWindowSize(data->width, data->height, id);

    Bytes bytes = { .allocator = currentAllocator };
    VideoMessage message = { .tag = VideoMessageTag_video };
    message.video = INIT_ALLOCATOR(MB(1));

#if USE_COMPUTE_SHADER
    SDL_Window* window = NULL;
    GLuint prog = 0;
    GLuint textures[4] = { 0 };
    GLuint pbos[4] = { 0 };
    void* context = NULL;
    void* ptrs[3] = { NULL };
#else
    uint8_t* YUV = currentAllocator->allocate(data->width * data->height * 3);
    uint32_t* ARGB = currentAllocator->allocate(data->width * data->height * 4);
#endif

#if USE_VP8
    int frame_count = 0;
    vpx_codec_ctx_t codec = { 0 };
    vpx_image_t img = { 0 };
    if (!initEncoder(&codec, &img, data)) {
        goto end;
    }
#else
    void* encoder = NULL;
    if (!initEncoder(&encoder, data)) {
        goto end;
    }
#endif

#if USE_COMPUTE_SHADER
    window = SDL_CreateWindow("unused",
                              0,
                              0,
                              256,
                              256,
                              SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL |
                                SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_TOOLTIP);
    if (window == NULL) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        goto end;
    }
    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        fprintf(
          stderr, "Failed to create opengl context: %s\n", SDL_GetError());
        goto end;
    }
    static SDL_atomic_t openglLoaded = { 0 };
    if (SDL_AtomicGet(&openglLoaded) == 0) {
        if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
            fprintf(stderr, "Failed to load opengl functions\n");
            goto end;
        }
        SDL_AtomicSet(&openglLoaded, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        printf("Vendor:   %s\n", glGetString(GL_VENDOR));
        printf("Renderer: %s\n", glGetString(GL_RENDERER));
        printf("Version:  %s\n", glGetString(GL_VERSION));

        int work_grp_cnt[3];

        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

        printf("max global (total) work group counts x:%i y:%i z:%i\n",
               work_grp_cnt[0],
               work_grp_cnt[1],
               work_grp_cnt[2]);
#if _DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OnGLMessage, NULL);
#endif
    }
    if (!compileShader(rgbaToYUVShader, &prog)) {
        goto end;
    }

    glUseProgram(prog);
    for (int i = 0; i < 4; ++i) {
        glUniform1i(i, i);
    }

    makeComputeShaderTextures(data->width, data->height, textures, pbos);
#endif

    const uint64_t delay = 1000 / data->fps;
    uint64_t lastGeoCheck = 0;
    uint64_t last = 0;
    bool windowHidden = false;
    while (!appDone && !displayMissing) {

        const uint64_t now = SDL_GetTicks64();
        const uint64_t diff = now - last;
        if (diff < delay) {
            SDL_Delay(diff);
            continue;
        }
        last = now;

#if USE_COMPUTE_SHADER
        // Tell the GPU to get the texture from pixel buffer
        // Image will be written to pixel buffer
        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[3]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER,
                     data->width * data->height * 4,
                     NULL,
                     GL_STREAM_DRAW);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        data->width,
                        data->height,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        NULL);
#endif

        xcb_generic_error_t* error = NULL;

        if (now - lastGeoCheck > 1000U) {
            bool needResize = false;
            xcb_get_geometry_cookie_t geomCookie =
              xcb_get_geometry(data->connection, data->windowId);
            xcb_get_geometry_reply_t* geom =
              xcb_get_geometry_reply(data->connection, geomCookie, &error);
            if (error) {
                printf("XCB Error: %d\n", error->error_code);
                free(error);
                if (geom != NULL) {
                    free(geom);
                }
                displayMissing = true;
            } else if (geom) {
                const int realWidth = geom->width - (geom->width % 2);
                const int realHeight = geom->height - (geom->height % 2);
                needResize =
                  realWidth != data->width || realHeight != data->height;
                if (needResize) {
                    printf("Window '%s' has changed size from [%ux%u] to "
                           "[%ux%u].\n",
                           data->name.buffer,
                           data->width,
                           data->height,
                           geom->width,
                           geom->height);
                    data->width = realWidth;
                    data->height = realHeight;
#if USE_VP8
                    displayMissing = !initEncoder(&codec, &img, data);
#else
                    displayMissing = !initEncoder(&encoder, data);
#endif
                    if (!displayMissing) {
                        displayMissing =
                          sendWindowSize(data->width, data->height, id);
                        makeComputeShaderTextures(
                          data->width, data->height, textures, pbos);
                    }
                }
                free(geom);
            }
            lastGeoCheck = now;
            if (displayMissing || needResize) {
                continue;
            }
        }

#if TIME_VIDEO_STREAMING
        xcb_get_image_cookie_t cookie = { 0 };
        xcb_get_image_reply_t* reply = NULL;
        TIME("Get window image", {
            cookie = xcb_get_image(data->connection,
                                   XCB_IMAGE_FORMAT_Z_PIXMAP,
                                   data->windowId,
                                   0,
                                   0,
                                   data->width,
                                   data->height,
                                   ~0U);
            reply = xcb_get_image_reply(data->connection, cookie, &error);
        });
#else
        xcb_get_image_cookie_t cookie = xcb_get_image(data->connection,
                                                      XCB_IMAGE_FORMAT_Z_PIXMAP,
                                                      data->windowId,
                                                      0,
                                                      0,
                                                      data->width,
                                                      data->height,
                                                      ~0U);
        xcb_get_image_reply_t* reply =
          xcb_get_image_reply(data->connection, cookie, &error);
#endif
        if (xcb_connection_has_error(data->connection)) {
            fprintf(stderr,
                    "X11 connection has an error. Recording will stop\n");
            displayMissing = true;
        } else if (error) {
            if (!windowHidden) {
                printf("XCB Error %d (window may not be visible. This "
                       "error will only show once until the window is "
                       "visible again)\n",
                       error->error_code);
                windowHidden = true;
            }
            free(error);
            if (reply) {
                free(reply);
            }
            // Window may be minimized. Don't end the thread for this
            // error
            continue;
        } else if (!reply) {
            continue;
        }

        windowHidden = false;
        void* imageData = xcb_get_image_data(reply);

        bool success;
#if TIME_VIDEO_STREAMING
        TIME("Convert pixels to YV12", { RGBA_TO_YUV; });
#else
        RGBA_TO_YUV;
#endif

#if USE_VP8
        if (success) {
#if USE_COMPUTE_SHADER
            memcpy(img.planes[0], ptrs[0], data->width * data->height);
            const int size = ((data->width + 1) / 2) * ((data->height + 1) / 2);
            memcpy(img.planes[1], ptrs[1], size);
            memcpy(img.planes[2], ptrs[2], size);
#else
            uint8_t* ptr = YUV;
            for (int plane = 0; plane < 3; ++plane) {
                unsigned char* buf = img.planes[plane];
                const int stride = img.stride[plane];
                const int w = vpx_img_plane_width(&img, plane) *
                              ((img.fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
                const int h = vpx_img_plane_height(&img, plane);

                for (int y = 0; y < h; ++y) {
                    memcpy(buf, ptr, w);
                    ptr += w;
                    buf += stride;
                }
            }
#endif
            int flags = 0;
            if (data->keyFrameInterval > 0 &&
                frame_count % data->keyFrameInterval == 0) {
                flags |= VPX_EFLAG_FORCE_KF;
            }

            vpx_codec_err_t res = { 0 };
#if TIME_VIDEO_STREAMING
            TIME("VPX encoding", {
                res = vpx_codec_encode(
                  &codec, &img, frame_count++, 1, flags, VPX_DL_REALTIME);
            });
#else
            res = vpx_codec_encode(
              &codec, &img, frame_count++, 1, flags, VPX_DL_REALTIME);
#endif
            if (res == VPX_CODEC_OK) {
                vpx_codec_iter_t iter = NULL;
                const vpx_codec_cx_pkt_t* pkt = NULL;
                while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != NULL) {
                    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT) {
                        continue;
                    }
                    message.video.used = 0;
                    uint8_tListQuickAppend(
                      &message.video, pkt->data.frame.buf, pkt->data.frame.sz);
                    // printf("Encoded %u -> %zu kilobytes\n",
                    //        (data->width * data->height * 3) / 1024,
                    //        pkt->data.frame.sz / 1024);
                    MESSAGE_SERIALIZE(VideoMessage, message, bytes);
                    ENetPacket* packet =
                      BytesToPacket(bytes.buffer, bytes.used, SendFlags_Video);
                    USE_DISPLAY(clientData.mutex, end2, displayMissing, {
                        NullValueListAppend(&display->outgoing,
                                            (NullValue)&packet);
                    });
                    if (displayMissing) {
                        enet_packet_destroy(packet);
                    }
                }
            } else {
                fprintf(stderr,
                        "Failed to encode frame: %s\n",
                        vpx_codec_err_to_string(res));
            }
        } else {
            fprintf(stderr, "Image conversion failure: %s\n", SDL_GetError());
        }
#else
        if (success) {
#if USE_COMPUTE_SHADER
            void* Y = ptrs[0];
            void* U = ptrs[1];
            void* V = ptrs[2];
#endif
            int encoded;
#if TIME_VIDEO_STREAMING
            TIME("H264 encoding", { ENCODE_TO_H264; });
#else
            ENCODE_TO_H264;
#endif
            if (encoded < 0) {
                fprintf(stderr, "Failed to encode frame\n");
            } else if (encoded == 0) {
                USE_DISPLAY(clientData.mutex, end564, displayMissing, {});
            } else {
                message.video.used = (uint32_t)encoded;
                MESSAGE_SERIALIZE(VideoMessage, message, bytes);
                ENetPacket* packet =
                  BytesToPacket(bytes.buffer, bytes.used, SendFlags_Video);
                USE_DISPLAY(clientData.mutex, end256, displayMissing, {
                    NullValueListAppend(&display->outgoing, (NullValue)&packet);
                });
                if (displayMissing) {
                    enet_packet_destroy(packet);
                }
            }
        } else {
            // GPU didn't copy buffer fast enough. Just skip this frame...
        }
#endif
#if USE_COMPUTE_SHADER
        for (int i = 0; i < 3; ++i) {
            if (ptrs[i] == NULL) {
                continue;
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
#endif
        free(reply);
    }

    goto end;

end:

    uint8_tListFree(&bytes);
    VideoMessageFree(&message);

#if USE_COMPUTE_SHADER
    glDeleteProgram(prog);
    glDeleteTextures(4, textures);
    glDeleteBuffers(4, pbos);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
#else
    currentAllocator->free(YUV);
    currentAllocator->free(ARGB);
#endif
#if USE_VP8
    vpx_img_free(&img);
    vpx_codec_destroy(&codec);
#else
    destroy_h264_encoder(encoder);
#endif

    xcb_disconnect(data->connection);
    WindowDataFree(data);
    currentAllocator->free(data);

    SDL_AtomicDecRef(&runningThreads);

    return EXIT_SUCCESS;
}