#include <include/main.h>

int
xioctl(int fd, int request, void* arg);

typedef struct WebCamData
{
    WindowData data;
    int fd;
} WebCamData, *pWebCamData;

int recordWebcamThread(pWebCamData);

bool
recordWebcam(const Guid* id, const struct pollfd inputfd, pBytes bytes)
{
    int fd = -1;
    pWebCamData data = currentAllocator->allocate(sizeof(WebCamData));
    askQuestion("What is the device name?");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling webcam recording");
        goto err;
    }

    fd = open((char*)bytes->buffer, O_RDWR);
    if (fd == -1) {
        perror("Opening video device");
        goto err;
    }

    struct v4l2_capability caps = { 0 };
    if (xioctl(fd, VIDIOC_QUERYCAP, &caps) == -1) {
        perror("Querying video capabilities");
        goto err;
    }

    if ((caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        fprintf(stderr, "Webcam doesn't support video capture\n");
        goto err;
    }

    if ((caps.capabilities & V4L2_CAP_STREAMING) == 0) {
        fprintf(stderr, "Webcam doesn't support streaming\n");
        goto err;
    }

    struct v4l2_format fmt = { 0 };
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    askQuestion("What is the recording width?");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling webcam recording");
        goto err;
    }
    fmt.fmt.pix.width = (uint32_t)strtoul((char*)bytes->buffer, NULL, 10);
    fmt.fmt.pix.width -= fmt.fmt.pix.width % 2;

    askQuestion("What is the recording height?");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling webcam recording");
        goto err;
    }
    fmt.fmt.pix.height = (uint32_t)strtoul((char*)bytes->buffer, NULL, 10);
    fmt.fmt.pix.height -= fmt.fmt.pix.height % 2;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Setting video format");
        goto err;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUV420) {
        fprintf(stderr,
                "Webcam doesn't support YUV. Got 0x%08X\n",
                fmt.fmt.pix.pixelformat);
        goto err;
    }

    printf("Webcam format: dimensions [%dx%d]\n",
           fmt.fmt.pix.width,
           fmt.fmt.pix.height);

    struct v4l2_requestbuffers req = { 0 };
    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Requesting video buffer");
        goto err;
    }

    askQuestion("Enter FPS");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto err;
    }
    const uint16_t fps =
      SDL_clamp(strtoul((char*)bytes->buffer, NULL, 10), 1u, 1000u);
    printf("FPS set to: %u\n", fps);

    askQuestion("Enter key frame interval");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto err;
    }
    const uint16_t keyInterval =
      SDL_clamp(strtoul((char*)bytes->buffer, NULL, 10), 1u, 1000u);
    printf("Key frame interval set to: %u\n", keyInterval);

    askQuestion("Enter bitrate (in megabits per second)");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        puts("Canceling window record");
        goto err;
    }
    const double bitrate_double =
      SDL_clamp(strtod((char*)bytes->buffer, NULL), 1.0, 100.0);
    const uint32_t bitrate = (uint32_t)floor(bitrate_double);
    printf("Bitrate set to: %u Mbps\n", bitrate);

    data->data.width = fmt.fmt.pix.width;
    data->data.height = fmt.fmt.pix.height;
    data->data.fps = fps;
    data->data.bitrate = bitrate;
    data->data.keyFrameInterval = keyInterval;
    data->data.id = *id;
    data->fd = fd;

    SDL_Thread* thread =
      SDL_CreateThread((SDL_ThreadFunction)recordWebcamThread, "webcam", data);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto err;
    }
    SDL_AtomicIncRef(&runningThreads);
    SDL_DetachThread(thread);

    data = NULL;

    return true;

err:
    close(fd);
    if (data != NULL) {
        WindowDataFree(&data->data);
        currentAllocator->free(data);
    }
    return false;
}

int
xioctl(int fd, int request, void* arg)
{
    int r = -1;
    for (int i = 0; r == -1 && i < 3; ++i) {
        r = ioctl(fd, request, arg);
        SDL_Delay(0);
    }
    return r;
}

int
recordWebcamThread(pWebCamData ptr)
{
    const int fd = ptr->fd;
    pWindowData data = &ptr->data;
    const Guid* id = &data->id;
    bool displayMissing = false;

    struct Buffer
    {
        void* start;
        size_t length;
    };
    struct Buffer buffers[3] = { 0 };

    int frame_count = 0;
    vpx_codec_ctx_t codec = { 0 };
    vpx_image_t img = { 0 };

    VideoMessage message = { .tag = VideoMessageTag_video,
                             .video = { .allocator = currentAllocator,
                                        .size = MB(1),
                                        .buffer =
                                          currentAllocator->allocate(MB(1)) } };
    Bytes bytes = { .allocator = currentAllocator };

    for (int i = 0; i < 3; ++i) {
        struct v4l2_buffer buf = { 0 };
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Querying video buffer");
            goto end;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL,
                                buf.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd,
                                buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            perror("Map memory");
            goto end;
        }
    }
    {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
            perror("Starting video capture");
            goto end;
        }
    }

    if (!initEncoder(&codec, &img, data)) {
        goto end;
    }

    const uint64_t delay = 1000 / data->fps;
    uint64_t last = 0;
    size_t planesAdded[3] = { 0 };
    while (!appDone && !displayMissing) {
        const uint64_t now = SDL_GetTicks64();
        const uint64_t diff = now - last;
        if (diff < delay) {
            SDL_Delay(diff);
            continue;
        }
        last = now;

        USE_DISPLAY(clientData.mutex, end425, displayMissing, {});

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = MILLI_TO_MICRO(1) };
        if (select(fd + 1, &fds, NULL, NULL, &tv) == -1) {
            perror("Waiting for video frame");
            goto end;
        }

        struct v4l2_buffer buf = { 0 };
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            switch (errno) {
                case EAGAIN:
                case ETIME:
                    continue;
                case EIO:
                    break;
                default:
                    perror("Retrieving video frame");
                    goto end;
            }
        }

        memcpy(img.planes[buf.index],
               buffers[buf.index].start,
               buffers[buf.index].length);
        ++planesAdded[buf.index];

        if (planesAdded[0] != planesAdded[1] ||
            planesAdded[1] != planesAdded[2]) {
            continue;
        }

        int flags = 0;
        if (data->keyFrameInterval > 0 &&
            frame_count % data->keyFrameInterval == 0) {
            flags |= VPX_EFLAG_FORCE_KF;
        }

        vpx_codec_err_t res = vpx_codec_encode(
          &codec, &img, frame_count++, 1, flags, VPX_DL_REALTIME);

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
                //        (data->width * data->height * 4) / 1024,
                //        pkt->data.frame.sz / 1024);
                MESSAGE_SERIALIZE(VideoMessage, message, bytes);
                ENetPacket* packet =
                  BytesToPacket(bytes.buffer, bytes.used, SendFlags_Video);
                USE_DISPLAY(clientData.mutex, end2, displayMissing, {
                    NullValueListAppend(&display->outgoing, (NullValue)&packet);
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
    }

end : {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd, VIDIOC_STREAMOFF, &type);
}
    uint8_tListFree(&bytes);
    VideoMessageFree(&message);
    vpx_img_free(&img);
    vpx_codec_destroy(&codec);
    close(fd);
    WindowDataFree(data);
    currentAllocator->free(ptr);
    SDL_AtomicDecRef(&runningThreads);

    return EXIT_SUCCESS;
}