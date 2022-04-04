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

    askQuestion("Enter bitrate (in megabits per second)");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto end;
    }
    const double bitrate_double =
      SDL_clamp(strtod((char*)bytes->buffer, NULL), 1.0, 100.0);
    const uint32_t bitrate = (uint32_t)floor(bitrate_double);
    printf("Bitrate set to: %u Mbps\n", bitrate);

    askQuestion("Enter resolution scale (1% - 100%)");
    uint32_t p = 0;
    if (getIndexFromUser(inputfd, bytes, 100, &p, true) !=
        UserInputResult_Input) {
        puts("Canceling window record");
        goto end;
    }

    pWindowData win = currentAllocator->allocate(sizeof(WindowData));
    WindowDataCopy(win, &list.buffer[selected], currentAllocator);
    win->id = *id;
    win->fps = fps;
    win->keyFrameInterval = keyInterval;
    win->bitrateInMbps = bitrate;
    win->ratio.numerator = p + 1;
    win->ratio.denominator = 100;

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

#if USE_OPENCL
#define RGBA_TO_YUV                                                            \
    success = rgbaToYuv(imageData, data, VideoEncoderPlanes(&codec), &vid)
#else
#define RGBA_TO_YUV                                                            \
    success = rgbaToYuv(imageData, data->width, data->height, ARGB, YUV)
#endif

bool
sendWindowSize(const WindowData* data)
{
    bool displayMissing = false;
    uint8_t buffer[KB(1)];
    Bytes bytes = { .buffer = buffer, .size = sizeof(buffer), .used = 0 };
    const Guid* id = &data->id;
    VideoMessage message = { .size = { data->width * data->ratio.numerator /
                                         data->ratio.denominator,
                                       data->height * data->ratio.numerator /
                                         data->ratio.denominator },
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

bool
canUseShmExt(xcb_connection_t* con)
{
    bool found = false;
    xcb_generic_error_t* err = NULL;
    xcb_shm_query_version_cookie_t cookie = xcb_shm_query_version(con);
    xcb_shm_query_version_reply_t* reply =
      xcb_shm_query_version_reply(con, cookie, &err);
    if (err) {
        printf("Error: %d\n", err->error_code);
        free(err);
        goto end;
    }
    if (!reply) {
        goto end;
    }

    found = true;
    free(reply);

end:
    return found;
}

bool
createShmExtStuff(const WindowData* data,
                  xcb_image_t** img,
                  xcb_shm_segment_info_t* shmInfo)
{
    if (*img != NULL) {
        xcb_image_destroy(*img);
    }
    shmdt(shmInfo->shmaddr);
    xcb_shm_detach(data->connection, shmInfo->shmseg);

    *img = xcb_image_create_native(data->connection,
                                   data->width,
                                   data->height,
                                   XCB_IMAGE_FORMAT_Z_PIXMAP,
                                   32,
                                   NULL,
                                   0,
                                   NULL);
    if (*img == NULL) {
        fprintf(stderr, "Failed to create XCB image\n");
        return false;
    }
    int id =
      shmget(IPC_PRIVATE, data->width * data->height * 4, IPC_CREAT | 0600);
    if (id == -1) {
        fprintf(stderr, "No shared memory\n");
        return false;
    }
    shmInfo->shmid = id;

    shmInfo->shmaddr = (*img)->data = shmat(shmInfo->shmid, 0, 0);

    shmInfo->shmseg = xcb_generate_id(data->connection);
    xcb_shm_attach(data->connection, shmInfo->shmseg, shmInfo->shmid, false);
    return true;
}

int
screenRecordThread(pWindowData data)
{
    const Guid* id = &data->id;
    bool displayMissing = sendWindowSize(data);

    Bytes bytes = { .allocator = currentAllocator };
    VideoMessage message = { .tag = VideoMessageTag_video };
    message.video = INIT_ALLOCATOR(MB(1));

    VideoEncoder codec = { 0 };

    xcb_image_t* xcbImg = NULL;
    xcb_shm_segment_info_t shmInfo = { 0 };

#if USE_OPENCL
    OpenCLVideo vid = { 0 };
    if (!OpenCLVideoInit(&vid, data)) {
        goto end;
    }
#else
    uint8_t* YUV = currentAllocator->allocate(data->width * data->height * 3);
    uint32_t* ARGB = currentAllocator->allocate(data->width * data->height * 4);
#endif
    if (!VideoEncoderInit(&codec, data, false)) {
        goto end;
    }

    if (canUseShmExt(data->connection)) {
        puts("Using SHM extension");
        if (!createShmExtStuff(data, &xcbImg, &shmInfo)) {
            goto end;
        }
    }

    const uint64_t delay = 1000 / data->fps;
    uint64_t lastGeoCheck = 0;
    uint64_t last = 0;
    bool windowHidden = false;
    uint32_t errors = 0;
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
                    displayMissing =
                      !VideoEncoderInit(&codec, data, false) ||
                      (canUseShmExt(data->connection)
                         ? !createShmExtStuff(data, &xcbImg, &shmInfo)
                         : false);
                    if (!displayMissing) {
                        displayMissing = sendWindowSize(data);
#if USE_OPENCL
                        OpenCLVideoFree(&vid);
                        displayMissing = !OpenCLVideoInit(&vid, data);
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

        void* reply = NULL;
        void* imageData = NULL;
        if (xcbImg) {
            TIME("Get shared memory screenshot", {
                xcb_shm_get_image_cookie_t cookie =
                  xcb_shm_get_image(data->connection,
                                    data->windowId,
                                    0,
                                    0,
                                    data->width,
                                    data->height,
                                    ~0U,
                                    XCB_IMAGE_FORMAT_Z_PIXMAP,
                                    shmInfo.shmseg,
                                    0);
                reply =
                  xcb_shm_get_image_reply(data->connection, cookie, &error);
                if (error) {
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
                imageData = xcbImg->data;
            });
        } else {
            TIME("Get raw screenshot", {
                xcb_get_image_cookie_t cookie =
                  xcb_get_image(data->connection,
                                XCB_IMAGE_FORMAT_Z_PIXMAP,
                                data->windowId,
                                0,
                                0,
                                data->width,
                                data->height,
                                ~0U);
                reply = xcb_get_image_reply(data->connection, cookie, &error);
                if (error) {
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
                imageData = xcb_get_image_data(reply);
            });
        }

        if (imageData == NULL) {
            continue;
        }

        bool success;
        TIME("Convert pixels to YV12", { RGBA_TO_YUV; });

        if (success) {
#if !USE_OPENCL
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
            displayMissing =
              VideoEncoderEncode(&codec, &message, &bytes, id, data);
            errors = 0;
        } else {
            ++errors;
            if (errors > 3) {
                fprintf(
                  stderr,
                  "Too many consecutive errors. Ending video streaming...\n");
                displayMissing = true;
            }
        }
        free(reply);
    }

end:

    if (xcbImg != NULL) {
        xcb_image_destroy(xcbImg);
    }
    shmdt(&shmInfo.shmaddr);
    xcb_shm_detach(data->connection, shmInfo.shmseg);

    uint8_tListFree(&bytes);
    VideoMessageFree(&message);

#if USE_OPENCL
    OpenCLVideoFree(&vid);
#else
    currentAllocator->free(YUV);
    currentAllocator->free(ARGB);
#endif
    VideoEncoderFree(&codec);

    xcb_disconnect(data->connection);
    WindowDataFree(data);
    currentAllocator->free(data);

    SDL_AtomicDecRef(&runningThreads);

    return EXIT_SUCCESS;
}