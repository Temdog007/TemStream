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
            xcb_get_geometry_reply_t* reply =
              xcb_get_geometry_reply(con, cookie, &error);
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
            data.width = reply->width;
            data.height = reply->height;
            free(reply);
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
    (void)source;
    (void)id;
    (void)length;
    (void)userParam;
    fprintf(stderr,
            "GL Callback: source=%s, type = %s, serverity = %s, message = %s\n",
            glSourceString(source),
            glTypeString(type),
            glSeverityString(severity),
            message);
}
#define RGBA_TO_YUV                                                            \
    rgbaToYuv(imageData, prog, geom->width, geom->height, textures, Y, U, V);  \
    success = true;
#define ENCODE_TO_H264                                                         \
    encoded = h264_encode_separate(encoder,                                    \
                                   Y,                                          \
                                   U,                                          \
                                   V,                                          \
                                   geom->width,                                \
                                   geom->height,                               \
                                   message.video.buffer,                       \
                                   message.video.size)
#else
#define RGBA_TO_YUV                                                            \
    success = rgbaToYuv(imageData, geom->width, geom->height, ARGB, YUV)
#define ENCODE_TO_H264                                                         \
    encoded = h264_encode(encoder,                                             \
                          YUV,                                                 \
                          geom->width,                                         \
                          geom->height,                                        \
                          message.video.buffer,                                \
                          message.video.size)
#endif

int
screenRecordThread(pWindowData data)
{
    const Guid* id = &data->id;
    bool displayMissing = false;

    Bytes bytes = { .allocator = currentAllocator };
    VideoMessage message = { .size = { data->width, data->height },
                             .tag = VideoMessageTag_size };

    MESSAGE_SERIALIZE(VideoMessage, message, bytes);
    {
        ENetPacket* packet =
          BytesToPacket(bytes.buffer, bytes.used, SendFlags_Normal);
        USE_DISPLAY(clientData.mutex, end3, displayMissing, {
            NullValueListAppend(&display->outgoing, (NullValue)&packet);
        });
        if (displayMissing) {
            enet_packet_destroy(packet);
        }
    }

    message.tag = VideoMessageTag_video;
    message.video = INIT_ALLOCATOR(MB(1));

    void* encoder = NULL;
#if USE_COMPUTE_SHADER
    SDL_Window* window = NULL;
    GLuint prog = 0;
    GLuint cs = 0;
    GLuint textures[4] = { 0 };
    uint8_t* Y = currentAllocator->allocate(data->width * data->height * 4);
    uint8_t* U = currentAllocator->allocate(data->width * data->height * 4);
    uint8_t* V = currentAllocator->allocate(data->width * data->height * 4);
    void* context = NULL;
    window = SDL_CreateWindow("",
                              0,
                              0,
                              512,
                              512,
                              SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL |
                                SDL_WINDOW_SKIP_TASKBAR);
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
    static bool openglLoaded = false;
    if (!openglLoaded) {
        if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
            fprintf(stderr, "Failed to load opengl functions\n");
            goto end;
        }
        openglLoaded = true;
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
        glDebugMessageCallback(OnGLMessage, NULL);
#endif
    }
    prog = glCreateProgram();
    cs = glCreateShader(GL_COMPUTE_SHADER);
    const char* ptr = (const char*)rgbaToYuvShader;
    glShaderSource(cs, 1, &ptr, NULL);
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

    glUseProgram(prog);
    for (int i = 0; i < 4; ++i) {
        glUniform1i(i, i);
    }

    makeComputeShaderTextures(data->width, data->height, textures);
#else
    uint8_t* YUV = currentAllocator->allocate(data->width * data->height * 3);
    uint32_t* ARGB = currentAllocator->allocate(data->width * data->height * 4);
#endif

#if USE_VP8
    int frame_count = 0;
    vpx_codec_ctx_t codec = { 0 };
    vpx_codec_enc_cfg_t cfg = { 0 };
    vpx_image_t img = { 0 };

    if (vpx_img_alloc(&img, VPX_IMG_FMT_I420, data->width, data->height, 1) ==
        NULL) {
        fprintf(stderr, "Failed to allocate image\n");
        goto end;
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
        goto end;
    }

    cfg.g_w = data->width;
    cfg.g_h = data->height;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = data->fps;
    cfg.rc_target_bitrate = data->bitrate;
    cfg.g_threads = SDL_GetCPUCount();
    cfg.g_error_resilient =
      VPX_ERROR_RESILIENT_DEFAULT | VPX_ERROR_RESILIENT_PARTITIONS;

    if (vpx_codec_enc_init(&codec, codec_encoder_interface(), &cfg, 0) != 0) {
        fprintf(stderr, "Failed to initialize encoder\n");
        goto end;
    }
#else
    if (!create_h264_encoder(
          &encoder, data->width, data->height, data->fps, data->bitrate)) {
        fprintf(stderr, "Failed to initalize encoder\n");
        goto end;
    }
#endif

    const uint64_t delay = 1000 / data->fps;
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

        xcb_generic_error_t* error = NULL;
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
        } else if (!geom) {
            continue;
        }

        if (geom->width != data->width || geom->height != data->height) {
            printf("Window '%s' has changed size. Recording will stop\n",
                   data->name.buffer);
            displayMissing = true;
        } else {
#if TIME_VIDEO_STREAMING
            xcb_get_image_cookie_t cookie = { 0 };
            xcb_get_image_reply_t* reply = NULL;
            TIME("Get window image", {
                cookie = xcb_get_image(data->connection,
                                       XCB_IMAGE_FORMAT_Z_PIXMAP,
                                       data->windowId,
                                       0,
                                       0,
                                       geom->width,
                                       geom->height,
                                       ~0U);
                reply = xcb_get_image_reply(data->connection, cookie, &error);
            });
#else
            xcb_get_image_cookie_t cookie =
              xcb_get_image(data->connection,
                            XCB_IMAGE_FORMAT_Z_PIXMAP,
                            data->windowId,
                            0,
                            0,
                            geom->width,
                            geom->height,
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
            uint32_t* imageData = (uint32_t*)xcb_get_image_data(reply);
#if TIME_JPEG
            Bytes jpegBytes = { 0 };
            uint8_t* rgb = NULL;
            TIME("Compress to JPEG", {
                rgb =
                  currentAllocator->allocate(data->width * data->height * 3);
                SDL_ConvertPixels(data->width,
                                  data->height,
                                  SDL_PIXELFORMAT_RGBA32,
                                  imageData,
                                  data->width * 4,
                                  SDL_PIXELFORMAT_RGB24,
                                  rgb,
                                  data->width * 3);
                jpegBytes = rgbaToJpeg(rgb, data->width, data->height);
            });
            printf("JPEG is %u kilobytes\n", jpegBytes.used / 1024);
            uint8_tListFree(&jpegBytes);
            currentAllocator->free(rgb);
#endif
            bool success;
#if TIME_VIDEO_STREAMING
            TIME("Convert pixels to YV12", { RGBA_TO_YUV; });
#else
            RGBA_TO_YUV;
#endif
#if USE_VP8
            if (success) {
                uint8_t* ptr = YUV;
                for (int plane = 0; plane < 3; ++plane) {
                    unsigned char* buf = img.planes[plane];
                    const int stride = img.stride[plane];
                    const int w =
                      vpx_img_plane_width(&img, plane) *
                      ((img.fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
                    const int h = vpx_img_plane_height(&img, plane);

                    for (int y = 0; y < h; ++y) {
                        memcpy(buf, ptr, w);
                        ptr += w;
                        buf += stride;
                    }
                }
                int flags = 0;
                if (data->keyFrameInterval > 0 &&
                    frame_count % data->keyFrameInterval == 0) {
                    flags |= VPX_EFLAG_FORCE_KF;
                }

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
                    while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) !=
                           NULL) {
                        if (pkt->kind != VPX_CODEC_CX_FRAME_PKT) {
                            continue;
                        }
                        message.video.used = 0;
                        uint8_tListQuickAppend(&message.video,
                                               pkt->data.frame.buf,
                                               pkt->data.frame.sz);
                        // printf("Encoded %u -> %zu kilobytes\n",
                        //        (data->width * data->height * 3) / 1024,
                        //        pkt->data.frame.sz / 1024);
                        MESSAGE_SERIALIZE(VideoMessage, message, bytes);
                        ENetPacket* packet = BytesToPacket(
                          bytes.buffer, bytes.used, SendFlags_Video);
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
                fprintf(
                  stderr, "Image conversion failure: %s\n", SDL_GetError());
            }
#else
            if (success) {
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
                    // printf("Encoded video %u -> %d kilobytes\n",
                    //        (geom->width * geom->height * 3u / 2u) / 1024,
                    //        encoded / 1024);
                    message.video.used = (uint32_t)encoded;
                    MESSAGE_SERIALIZE(VideoMessage, message, bytes);
                    ENetPacket* packet =
                      BytesToPacket(bytes.buffer, bytes.used, SendFlags_Video);
                    USE_DISPLAY(clientData.mutex, end256, displayMissing, {
                        NullValueListAppend(&display->outgoing,
                                            (NullValue)&packet);
                    });
                    if (displayMissing) {
                        enet_packet_destroy(packet);
                    }
                }
            } else {
                fprintf(
                  stderr, "Image conversion failure: %s\n", SDL_GetError());
            }
#endif
            free(reply);
        }
        free(geom);
    }

    goto end;

end:

    uint8_tListFree(&bytes);
    VideoMessageFree(&message);

#if USE_COMPUTE_SHADER
    currentAllocator->free(Y);
    currentAllocator->free(U);
    currentAllocator->free(V);
    glDeleteProgram(prog);
    glDeleteShader(cs);
    glDeleteTextures(4, textures);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
#else
    currentAllocator->free(YUV);
    currentAllocator->free(ARGB);
#endif
#if USE_VP8
    (void)encoder;
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