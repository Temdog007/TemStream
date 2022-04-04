#include <include/main.h>

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
    pWebCamData webcam = currentAllocator->allocate(sizeof(WebCamData));
    askQuestion("What is the device name?");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        goto err;
    }

    fd = v4l2_open((char*)bytes->buffer, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        perror("Opening video device");
        goto err;
    }

    struct v4l2_capability caps = { 0 };
    if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &caps) == -1) {
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
        goto err;
    }
    fmt.fmt.pix.width = (uint32_t)strtoul((char*)bytes->buffer, NULL, 10);
    fmt.fmt.pix.width -= fmt.fmt.pix.width % 2;

    askQuestion("What is the recording height?");
    if (getStringFromUser(inputfd, bytes, true) != UserInputResult_Input) {
        goto err;
    }
    fmt.fmt.pix.height = (uint32_t)strtoul((char*)bytes->buffer, NULL, 10);
    fmt.fmt.pix.height -= fmt.fmt.pix.height % 2;

    if (v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
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
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Requesting video buffer");
        goto err;
    }

    if (req.count == 0) {
        fprintf(stderr, "Failed to get video buffer\n");
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

    webcam->data.width = fmt.fmt.pix.width;
    webcam->data.height = fmt.fmt.pix.height;
    webcam->data.fps = fps;
    webcam->data.bitrate = bitrate;
    webcam->data.keyFrameInterval = keyInterval;
    webcam->data.id = *id;
    webcam->data.ratio = (Ratio){ .numerator = 1u, .denominator = 1u };
    webcam->fd = fd;

    SDL_Thread* thread = SDL_CreateThread(
      (SDL_ThreadFunction)recordWebcamThread, "webcam", webcam);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
        goto err;
    }
    SDL_AtomicIncRef(&runningThreads);
    SDL_DetachThread(thread);

    return true;

err:
    puts("Canceling webcam recording");
    close(fd);
    if (webcam != NULL) {
        WindowDataFree(&webcam->data);
        currentAllocator->free(webcam);
    }
    return false;
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
    struct Buffer buffer = { 0 };

    VideoCodec codec = { 0 };

    VideoMessage message = { .tag = VideoMessageTag_video,
                             .video = { .allocator = currentAllocator,
                                        .size = MB(1),
                                        .buffer =
                                          currentAllocator->allocate(MB(1)) } };
    Bytes bytes = { .allocator = currentAllocator };

    struct v4l2_buffer buf = { 0 };
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (v4l2_ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
        perror("Querying video buffer");
        goto end;
    }

    buffer.length = buf.length;
    buffer.start = v4l2_mmap(
      NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer.start == MAP_FAILED) {
        perror("Map memory");
        goto end;
    }

    buf = (struct v4l2_buffer){ 0 };
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (v4l2_ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        perror("Query video buffer");
        goto end;
    }

    {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
            perror("Starting video capture");
            goto end;
        }
    }

    if (!VideoCodecInit(&codec, data)) {
        goto end;
    }

    const uint64_t delay = 1000 / data->fps;
    uint64_t last = 0;
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
        // struct timeval tv = { .tv_sec = 0, .tv_usec = MILLI_TO_MICRO(1) };
        struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        switch (select(fd + 1, &fds, NULL, NULL, &tv)) {
            case -1:
                perror("Waiting for video frame");
                goto end;
            case 0:
                continue;
            default:
                break;
        }

        struct v4l2_buffer buf = { 0 };
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (v4l2_ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
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

#if USE_VPX
        uint8_t* ptr = buffer.start;
        for (int plane = 0; plane < 3; ++plane) {
            unsigned char* buf = codec.img.planes[plane];
            const int stride = codec.img.stride[plane];
            const int w = vpx_img_plane_width(&codec.img, plane) *
                          ((codec.img.fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
            const int h = vpx_img_plane_height(&codec.img, plane);

            for (int y = 0; y < h; ++y) {
                memcpy(buf, ptr, w);
                ptr += w;
                buf += stride;
            }
        }
#endif

        displayMissing = VideoCodecEncode(&codec, &message, &bytes, id, data);
        v4l2_ioctl(fd, VIDIOC_QBUF, &buf);
    }

end:
    uint8_tListFree(&bytes);
    VideoMessageFree(&message);
    {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    VideoCodecFree(&codec);
    v4l2_munmap(buffer.start, buffer.length);
    v4l2_close(fd);
    WindowDataFree(data);
    currentAllocator->free(ptr);
    SDL_AtomicDecRef(&runningThreads);

    return EXIT_SUCCESS;
}