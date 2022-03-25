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

    askQuestion("Enter key frame interval");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto end;
    }
    const uint16_t keyInterval =
      SDL_clamp(strtoul((char*)bytes->buffer, NULL, 10), 1u, 1000u);
    printf("Key frame interval set to: %u\n", keyInterval);

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

#if USE_OPENCL
#define RGBA_TO_YUV                                                            \
    success = rgbaToYuv(imageData, data->width, data->height, ptrs, &vid)
#else
#define RGBA_TO_YUV                                                            \
    success = rgbaToYuv(imageData, data->width, data->height, ARGB, YUV)
#endif

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

    int frame_count = 0;
    vpx_codec_ctx_t codec = { 0 };
    vpx_image_t img = { 0 };

#if USE_OPENCL
    OpenCLVideo vid = { 0 };
    if (!OpenCLVideoInit(&vid, data->width, data->height)) {
        goto end;
    }
    void* ptrs[3] = { NULL };
#else
    uint8_t* YUV = currentAllocator->allocate(data->width * data->height * 3);
    uint32_t* ARGB = currentAllocator->allocate(data->width * data->height * 4);
#endif
    if (!initEncoder(&codec, &img, data)) {
        goto end;
    }

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
                           realWidth,
                           realHeight);
                    data->width = realWidth;
                    data->height = realHeight;
                    displayMissing = !initEncoder(&codec, &img, data);
                    if (!displayMissing) {
                        displayMissing =
                          sendWindowSize(data->width, data->height, id);
#if USE_OPENCL
                        OpenCLVideoFree(&vid);
                        displayMissing =
                          !OpenCLVideoInit(&vid, data->width, data->height);
#else
                        YUV = currentAllocator->reallocate(
                          YUV, data->width * data->height * 3);
                        ARGB = currentAllocator->reallocate(
                          ARGB, data->width * data->height * 4);
#endif
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

        if (success) {
#if USE_OPENCL
            for (int plane = 0; plane < 3; ++plane) {
                unsigned char* buf = img.planes[plane];
                const int stride = img.stride[plane];
                const int w = vpx_img_plane_width(&img, plane) *
                              ((img.fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
                const int h = vpx_img_plane_height(&img, plane);

                uint8_t* ptr = ptrs[plane];
                for (int y = 0; y < h; ++y) {
                    memcpy(buf, ptr, w);
                    ptr += w;
                    buf += stride;
                }
            }
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
#if USE_OPENCL
        for (int i = 0; i < 3; ++i) {
            if (ptrs[i] == NULL) {
                continue;
            }
            clEnqueueUnmapMemObject(
              vid.command_queue, vid.args[i + 1], ptrs[i], 0, NULL, NULL);
        }
#endif
        free(reply);
    }

    goto end;

end:

    uint8_tListFree(&bytes);
    VideoMessageFree(&message);

#if USE_OPENCL
    OpenCLVideoFree(&vid);
#else
    currentAllocator->free(YUV);
    currentAllocator->free(ARGB);
#endif
    vpx_img_free(&img);
    vpx_codec_destroy(&codec);

    xcb_disconnect(data->connection);
    WindowDataFree(data);
    currentAllocator->free(data);

    SDL_AtomicDecRef(&runningThreads);

    return EXIT_SUCCESS;
}