#include <api/svc/codec_api.h>
#include <include/main.h>

bool
VideoEncoderInit(pVideoEncoder codec,
                 const WindowData* data,
                 const bool forCamera)
{
    VideoEncoderFree(codec);

    const int rv = WelsCreateSVCEncoder((ISVCEncoder**)&codec->ctx);
    if (rv != cmResultSuccess || codec->ctx == NULL) {
        fprintf(stderr, "Failed to initialize encoder\n");
        return false;
    }

    ISVCEncoder* encoder = (ISVCEncoder*)(codec->ctx);
    OpenH264Version version = { 0 };
    WelsGetCodecVersionEx(&version);
    printf("Using encoder: Open H264 %u.%u.%u\n",
           version.uMajor,
           version.uMinor,
           version.uRevision);

    {
        SEncParamExt param = { 0 };
        if ((*encoder)->GetDefaultParams(encoder, &param) != cmResultSuccess) {
            return false;
        }

        param.iUsageType =
          forCamera ? CAMERA_VIDEO_REAL_TIME : SCREEN_CONTENT_REAL_TIME;
        param.fMaxFrameRate = data->fps;
        param.iPicWidth = data->width;
        param.iPicHeight = data->height;
        param.iTargetBitrate = data->bitrateInMbps * 1024u * 1024u;
        param.iMaxBitrate = param.iTargetBitrate;
        param.iTemporalLayerNum = true;
        param.iSpatialLayerNum = true;
        param.bEnableDenoise = 0;
        param.bEnableBackgroundDetection = true;
        param.bEnableAdaptiveQuant = true;
        param.bEnableFrameSkip = false;
        param.bEnableLongTermReference = 0;
        param.iLtrMarkPeriod = 30;
        param.iMultipleThreadIdc = SDL_GetCPUCount();

        param.sSpatialLayers[0].iVideoWidth = param.iPicWidth;
        param.sSpatialLayers[0].iVideoHeight = param.iPicHeight;
        param.sSpatialLayers[0].fFrameRate = param.fMaxFrameRate;
        param.sSpatialLayers[0].iSpatialBitrate = param.iTargetBitrate;
        param.sSpatialLayers[0].iMaxSpatialBitrate = param.iMaxBitrate;
        param.sSpatialLayers[0].uiProfileIdc = PRO_BASELINE;

        if ((*encoder)->InitializeExt(encoder, &param) != cmResultSuccess) {
            return false;
        }
    }

    for (int i = 0; i < 3; ++i) {
        codec->planes[i] =
          currentAllocator->allocate(data->width * data->height);
    }

#if _DEBUG
    int log_level = WELS_LOG_INFO;
#else
    int log_level = WELS_LOG_WARNING;
#endif

    int videoFormat = videoFormatI420;
    return (*encoder)->SetOption(encoder,
                                 ENCODER_OPTION_TRACE_LEVEL,
                                 &log_level) == cmResultSuccess &&
           (*encoder)->SetOption(encoder,
                                 ENCODER_OPTION_DATAFORMAT,
                                 &videoFormat) == cmResultSuccess;
}

void**
VideoEncoderPlanes(pVideoEncoder codec)
{
    return (void**)&codec->planes;
}

bool
VideoEncoderEncode(pVideoEncoder codec,
                   pVideoMessage message,
                   pBytes bytes,
                   const Guid* id,
                   const WindowData* data)
{
    ISVCEncoder* encoder = (ISVCEncoder*)(codec->ctx);

    SSourcePicture pic = { 0 };
    pic.iPicWidth = data->width;
    pic.iPicHeight = data->height;
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = pic.iPicWidth;
    pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
    for (int i = 0; i < 3; ++i) {
        pic.pData[i] = (unsigned char*)codec->planes[i];
    }

    bool displayMissing = false;
    SFrameBSInfo info = { 0 };
    const int rv = (*encoder)->EncodeFrame(encoder, &pic, &info);

    if (rv != cmResultSuccess) {
        // Don't stop encoding
        goto end;
    }

    if (info.eFrameType == videoFrameTypeSkip) {
        goto end;
    }

    {
        size_t layerSize[MAX_LAYER_NUM_OF_FRAME] = { 0 };
        for (int layerNum = 0; layerNum < info.iLayerNum; ++layerNum) {
            for (int i = 0; i < info.sLayerInfo[layerNum].iNalCount; ++i) {
                layerSize[layerNum] +=
                  info.sLayerInfo[layerNum].pNalLengthInByte[i];
            }
        }

        message->video.used = 0;
        for (int layerNum = 0; layerNum < info.iLayerNum; ++layerNum) {
            uint8_tListQuickAppend(&message->video,
                                   info.sLayerInfo[layerNum].pBsBuf,
                                   layerSize[layerNum]);
        }
    }

    {
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

end:
    return displayMissing;
}

void
VideoEncoderFree(pVideoEncoder codec)
{
    if (codec->ctx != NULL) {
        WelsDestroySVCEncoder(codec->ctx);
    }
    for (int i = 0; i < 3; ++i) {
        currentAllocator->free(codec->planes[i]);
    }
    memset(codec, 0, sizeof(VideoEncoder));
}

bool
VideoDecoderInit(pVideoDecoder codec)
{
    VideoDecoderFree(codec);

    if (WelsCreateDecoder((ISVCDecoder**)&codec->ctx)) {
        fprintf(stderr, "Failed to create decoder\n");
        return false;
    }

    ISVCDecoder* decoder = (ISVCDecoder*)codec->ctx;

    SDecodingParam param = { 0 };
    if ((*decoder)->Initialize(decoder, &param) != cmResultSuccess) {
        fprintf(stderr, "Failed to initialize decoder\n");
        return false;
    }

    return true;
}

void
VideoDecoderDecode(pVideoDecoder codec,
                   const Bytes* bytes,
                   const Guid* id,
                   uint64_t* lastError)
{
    ISVCDecoder* decoder = (ISVCDecoder*)codec->ctx;

    unsigned char* ptrs[3] = { 0 };
    SBufferInfo info = { 0 };
    const DECODING_STATE state = (*decoder)->DecodeFrameNoDelay(
      decoder, bytes->buffer, bytes->used, ptrs, &info);

    if (state != dsErrorFree) {
        const uint64_t now = SDL_GetTicks64();
        if (now - *lastError > 1000) {
            fprintf(stderr, "Failed to decode video frame\n");
        }
        *lastError = now;
        return;
    }
    if (info.iBufferStatus != 1) {
        // No frame produced
        return;
    }

    const size_t size = info.UsrData.sSystemBuffer.iWidth *
                        info.UsrData.sSystemBuffer.iHeight * 2;

    if (size + currentAllocator->used() > currentAllocator->totalSize()) {
        return;
    }

    pVideoFrame m = currentAllocator->allocate(sizeof(VideoFrame));
    m->id = *id;
    m->width = info.UsrData.sSystemBuffer.iWidth;
    m->height = info.UsrData.sSystemBuffer.iHeight;
    m->video.allocator = currentAllocator;
    m->video.buffer = currentAllocator->allocate(size);
    m->video.size = size;

#if 1
    int strides[3] = {
        info.UsrData.sSystemBuffer.iStride[0],
        info.UsrData.sSystemBuffer.iStride[1],
        info.UsrData.sSystemBuffer.iStride[1],
    };
    int ws[3] = {
        info.UsrData.sSystemBuffer.iWidth,
        info.UsrData.sSystemBuffer.iWidth / 2,
        info.UsrData.sSystemBuffer.iWidth / 2,
    };
    int hs[3] = {
        info.UsrData.sSystemBuffer.iHeight,
        info.UsrData.sSystemBuffer.iHeight / 2,
        info.UsrData.sSystemBuffer.iHeight / 2,
    };
    for (int plane = 0; plane < 3; ++plane) {
        const unsigned char* buf = ptrs[plane];
        const int stride = strides[plane];
        const int w = ws[plane];
        const int h = hs[plane];
        for (int y = 0; y < h; ++y) {
            uint8_tListQuickAppend(&m->video, buf, w);
            buf += stride;
        }
    }
#else
    uint8_tListQuickAppend(&m->video,
                           ptrs[0],
                           info.UsrData.sSystemBuffer.iWidth *
                             info.UsrData.sSystemBuffer.iHeight);
    uint8_tListQuickAppend(
      &m->video,
      ptrs[1],
      (info.UsrData.sSystemBuffer.iWidth * info.UsrData.sSystemBuffer.iHeight) /
        4);
    uint8_tListQuickAppend(
      &m->video,
      ptrs[2],
      (info.UsrData.sSystemBuffer.iWidth * info.UsrData.sSystemBuffer.iHeight) /
        4);
#endif

    // printf("Decoded %u -> %u kilobytes\n",
    //        message.video.used / 1024,
    //        m->video.used / 1024);
    SDL_Event e = { 0 };
    e.type = SDL_USEREVENT;
    e.user.code = CustomEvent_UpdateVideoDisplay;
    e.user.data1 = m;
    SDL_PushEvent(&e);
}

void
VideoDecoderFree(pVideoDecoder codec)
{
    ISVCDecoder* decoder = (ISVCDecoder*)codec->ctx;
    if (decoder != NULL) {
        WelsDestroyDecoder(decoder);
    }
    memset(codec, 0, sizeof(VideoDecoder));
}