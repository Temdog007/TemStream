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

int
screenRecordThread(pWindowData data)
{
    bool doneWithThread = false;

    vpx_codec_ctx_t codec = { 0 };
    vpx_codec_enc_cfg_t cfg = { 0 };
    int frame_count = 0;
    vpx_image_t raw = { 0 };

    Bytes bytes = { .allocator = currentAllocator };
    VideoMessage message = { .size = { data->width, data->height },
                             .tag = VideoMessageTag_size };

    MESSAGE_SERIALIZE(VideoMessage, message, bytes);
    {
        ENetPacket* packet = BytesToPacket(bytes.buffer, bytes.used, true);
        IN_MUTEX(clientData.mutex, end3, {
            size_t i = 0;
            if (GetStreamDisplayFromGuid(
                  &clientData.displays, &data->id, NULL, &i)) {
                NullValueListAppend(&clientData.displays.buffer[i].outgoing,
                                    (NullValue)&packet);
            } else {
                enet_packet_destroy(packet);
            }
        });
    }

    message.tag = VideoMessageTag_video;
    message.video = INIT_ALLOCATOR(MB(1));

    uint8_t* YUV = currentAllocator->allocate(data->width * data->height * 3);

    if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, data->width, data->height, 1)) {
        fprintf(stderr, "Failed to allocate image\n");
        goto end;
    }

    if (vpx_codec_enc_config_default(vpx_codec_vp8_dx(), &cfg, 0) != 0) {
        fprintf(stderr, "Failed to get default video encoder configuration\n");
        goto end;
    }

    cfg.g_w = data->width;
    cfg.g_h = data->height;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = data->fps;
    cfg.rc_target_bitrate = data->bitrate;
    cfg.g_error_resilient =
      VPX_ERROR_RESILIENT_DEFAULT | VPX_ERROR_RESILIENT_PARTITIONS;

    if (vpx_codec_enc_init(&codec, vpx_codec_vp8_dx(), &cfg, 0) != 0) {
        fprintf(stderr, "Failed to initialize encoder\n");
        goto end;
    }

    const uint64_t delay = 1000 / data->fps;
    uint64_t last = 0;
    while (!appDone && !doneWithThread) {

        const uint64_t now = SDL_GetTicks64();
        const uint64_t diff = now - last;
        if (diff < delay) {
            SDL_Delay(diff);
            continue;
        }
        last = now;

        xcb_generic_error_t* error = NULL;
        xcb_get_geometry_cookie_t cookie =
          xcb_get_geometry(data->connection, data->windowId);
        xcb_get_geometry_reply_t* geom =
          xcb_get_geometry_reply(data->connection, cookie, &error);
        if (error) {
            printf("XCB Error: %d\n", error->error_code);
            free(error);
            doneWithThread = true;
        } else if (geom) {
            if (geom->width != data->width || geom->height != data->height) {
                printf("Window '%s' has changed size. Recording will stop\n",
                       data->name.buffer);
                doneWithThread = true;
            } else {
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
                if (error) {
                    // printf("XCB Error: %d\n", error->error_code);
                    free(error);
                    // Window may be minimized. Don't end the thread for this
                    // error
                } else if (reply) {
                    uint8_t* imageData = xcb_get_image_data(reply);

                    if (SDL_ConvertPixels(geom->width,
                                          geom->height,
                                          SDL_PIXELFORMAT_RGBA32,
                                          imageData,
                                          geom->width * 4,
                                          SDL_PIXELFORMAT_YV12,
                                          YUV,
                                          geom->width) == 0) {
                        int flags = 0;
                        if (data->keyFrameInterval > 0 &&
                            frame_count % data->keyFrameInterval == 0) {
                            flags |= VPX_EFLAG_FORCE_KF;
                        }
                        if (vpx_codec_encode(&codec,
                                             &raw,
                                             frame_count++,
                                             1,
                                             flags,
                                             VPX_DL_REALTIME) == VPX_CODEC_OK) {
                            vpx_codec_iter_t iter = NULL;
                            const vpx_codec_cx_pkt_t* pkt = NULL;
                            while ((pkt = vpx_codec_get_cx_data(
                                      &codec, &iter)) != NULL) {
                                message.video.used = 0;
                                uint8_tListQuickAppend(&message.video,
                                                       pkt->data.frame.buf,
                                                       pkt->data.frame.sz);
                                MESSAGE_SERIALIZE(VideoMessage, message, bytes);
                                ENetPacket* packet = BytesToPacket(
                                  bytes.buffer, bytes.used, false);
                                IN_MUTEX(clientData.mutex, end2, {
                                    size_t i = 0;
                                    if (GetStreamDisplayFromGuid(
                                          &clientData.displays,
                                          &data->id,
                                          NULL,
                                          &i)) {
                                        NullValueListAppend(
                                          &clientData.displays.buffer[i]
                                             .outgoing,
                                          (NullValue)&packet);
                                    } else {
                                        enet_packet_destroy(packet);
                                    }
                                });
                            }
                        }
                    } else {
                        fprintf(stderr,
                                "Image conversion failure: %s\n",
                                SDL_GetError());
                    }
                }
                free(reply);
            }
        }
        free(geom);
    }

end:
    uint8_tListFree(&bytes);
    VideoMessageFree(&message);
    vpx_img_free(&raw);
    vpx_codec_destroy(&codec);
    currentAllocator->free(YUV);
    WindowDataFree(data);
    currentAllocator->free(data);
    return EXIT_SUCCESS;
}