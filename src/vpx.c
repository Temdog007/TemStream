#include <include/main.h>

void
handleVideoFrame(const Bytes* bytes,
                 const Guid* id,
                 pVideoCodecContext codec,
                 uint64_t* lastError)
{
    vpx_codec_err_t res = vpx_codec_decode(
      codec, bytes->buffer, bytes->used, NULL, VPX_DL_REALTIME);
    if (res != VPX_CODEC_OK) {
        const uint64_t now = SDL_GetTicks64();
        if (now - *lastError > 1000) {
            fprintf(stderr,
                    "Failed to decode video frame: %s\n",
                    vpx_codec_err_to_string(res));
        }
        *lastError = now;
        return;
    }
    vpx_codec_iter_t iter = NULL;
    vpx_image_t* img = NULL;
    while ((img = vpx_codec_get_frame(codec, &iter)) != NULL) {
        if (lowMemory()) {
            continue;
        }
        const int width = vpx_img_plane_width(img, 0);
        const int height = vpx_img_plane_height(img, 0);

        const size_t size = width * height * 2;

        if (size + currentAllocator->used() > currentAllocator->totalSize()) {
            continue;
        }

        pVideoFrame m = currentAllocator->allocate(sizeof(VideoFrame));
        m->id = *id;
        m->width = width;
        m->height = height;
        m->video.allocator = currentAllocator;
        m->video.buffer = currentAllocator->allocate(size);
        m->video.size = size;

        for (int plane = 0; plane < 3; ++plane) {
            const unsigned char* buf = img->planes[plane];
            const int stride = img->stride[plane];
            const int w = vpx_img_plane_width(img, plane) *
                          ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
            const int h = vpx_img_plane_height(img, plane);
            for (int y = 0; y < h; ++y) {
                uint8_tListQuickAppend(&m->video, buf, w);
                buf += stride;
            }
        }
        // printf("Decoded %u -> %u kilobytes\n",
        //        message.video.used / 1024,
        //        m->video.used / 1024);
        SDL_Event e = { 0 };
        e.type = SDL_USEREVENT;
        e.user.code = CustomEvent_UpdateVideoDisplay;
        e.user.data1 = m;
        SDL_PushEvent(&e);
    }
}

int
vpx_img_plane_width(const vpx_image_t* img, const int plane)
{
    if (plane > 0 && img->x_chroma_shift > 0) {
        return (img->d_w + 1) >> img->x_chroma_shift;
    } else {
        return img->d_w;
    }
}

int
vpx_img_plane_height(const vpx_image_t* img, const int plane)
{
    if (plane > 0 && img->y_chroma_shift > 0) {
        return (img->d_h + 1) >> img->y_chroma_shift;
    } else {
        return img->d_h;
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

bool
VideoCodecInit(pVideoCodec codec, const WindowData* data)
{
    VideoCodecFree(codec);

    vpx_codec_enc_cfg_t cfg = { 0 };

    const size_t width =
      data->width * data->ratio.numerator / data->ratio.denominator;
    const size_t height =
      data->height * data->ratio.numerator / data->ratio.denominator;
    if (vpx_img_alloc(&codec->img, VPX_IMG_FMT_I420, width, height, 1) ==
        NULL) {
        fprintf(stderr, "Failed to allocate image\n");
        return false;
    }

    printf("Using encoder: %s with %d threads\n",
           vpx_codec_iface_name(codec_encoder_interface()),
           SDL_GetCPUCount());

    const vpx_codec_err_t res =
      vpx_codec_enc_config_default(codec_encoder_interface(), &cfg, 0);
    if (res) {
        fprintf(stderr,
                "Failed to get default video encoder configuration: %s\n",
                vpx_codec_err_to_string(res));
        return false;
    }

    cfg.g_w = width;
    cfg.g_h = height;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = data->fps;
    cfg.rc_target_bitrate = data->bitrate * 1024;
    cfg.g_threads = SDL_GetCPUCount();
    cfg.g_error_resilient =
      VPX_ERROR_RESILIENT_DEFAULT | VPX_ERROR_RESILIENT_PARTITIONS;

    if (vpx_codec_enc_init(&codec->ctx, codec_encoder_interface(), &cfg, 0) !=
        0) {
        fprintf(stderr, "Failed to initialize encoder\n");
        return false;
    }
    return true;
}

bool
VideoCodecEncode(pVideoCodec codec,
                 pVideoMessage message,
                 pBytes bytes,
                 const Guid* id,
                 const WindowData* data)
{
    int flags = 0;
    if (data->keyFrameInterval > 0 &&
        ++codec->frameCount % data->keyFrameInterval == 0) {
        flags |= VPX_EFLAG_FORCE_KF;
    }

    vpx_codec_err_t res = { 0 };
    TIME("VPX encoding", {
        res = vpx_codec_encode(&codec->ctx,
                               &codec->img,
                               codec->frameCount++,
                               1,
                               flags,
                               VPX_DL_REALTIME);
    });
    bool displayMissing = false;
    if (res == VPX_CODEC_OK) {
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t* pkt = NULL;
        while (!displayMissing &&
               (pkt = vpx_codec_get_cx_data(&codec->ctx, &iter)) != NULL) {
            if (pkt->kind != VPX_CODEC_CX_FRAME_PKT) {
                continue;
            }
            message->video.used = 0;
            uint8_tListQuickAppend(
              &message->video, pkt->data.frame.buf, pkt->data.frame.sz);
            // printf("Encoded %u -> %zu kilobytes\n",
            //        (data->width * data->height * 4) / 1024,
            //        pkt->data.frame.sz / 1024);
            MESSAGE_SERIALIZE(VideoMessage, (*message), (*bytes));
            ENetPacket* packet =
              BytesToPacket(bytes->buffer, bytes->used, SendFlags_Video);
            USE_DISPLAY(clientData.mutex, end2, displayMissing, {
                NullValueListAppend(&display->outgoing, (NullValue)&packet);
            });
            if (displayMissing) {
                enet_packet_destroy(packet);
            }
        }
    } else {
        fprintf(
          stderr, "Failed to encode frame: %s\n", vpx_codec_err_to_string(res));
    }
    return displayMissing;
}

void
VideoCodecFree(pVideoCodec codec)
{
    vpx_img_free(&codec->img);
    vpx_codec_destroy(&codec->ctx);
    memset(codec, 0, sizeof(VideoCodec));
}

void**
VideoCodecPlanes(pVideoCodec codec)
{
    return (void**)&codec->img.planes;
}