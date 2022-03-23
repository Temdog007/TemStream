#include <api/svc/codec_api.h>
#include <open264.h>

#include <memory.h>
#include <stdio.h>

bool
create_h264_encoder(void** ptr,
                    const int width,
                    const int height,
                    const float frameRate,
                    const int bitrateKps)
{
    ISVCEncoder** encoderPtr = reinterpret_cast<ISVCEncoder**>(ptr);
    const int result = WelsCreateSVCEncoder(encoderPtr);
    ISVCEncoder* encoder = *encoderPtr;
    if (result != 0 || encoder == nullptr) {
        return false;
    }

    SEncParamBase param;
    memset(&param, 0, sizeof(SEncParamBase));
    param.iUsageType = EUsageType::SCREEN_CONTENT_REAL_TIME;
    param.fMaxFrameRate = frameRate;
    param.iPicWidth = width;
    param.iPicHeight = height;
    param.iTargetBitrate = bitrateKps * 1024;
    if (encoder->Initialize(&param) != 0) {
        return false;
    }

    int log = 2;
    int videoFormat = EVideoFormatType::videoFormatI420;
    return encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat) == 0 &&
           encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &log) == 0;
}

int
h264_encode(void* ptr,
            unsigned char* src,
            const int width,
            const int height,
            uint8_t* dst,
            const size_t dstSize)
{
    ISVCEncoder* encoder = reinterpret_cast<ISVCEncoder*>(ptr);

    SFrameBSInfo info;
    memset(&info, 0, sizeof(SFrameBSInfo));

    SSourcePicture pic;
    memset(&pic, 0, sizeof(SSourcePicture));

    pic.iPicWidth = width;
    pic.iPicHeight = height;
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = pic.iPicWidth;
    pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
    pic.pData[0] = src;
    pic.pData[1] = pic.pData[0] + width * height;
    pic.pData[2] = pic.pData[1] + (width * height >> 2);

    if (encoder->EncodeFrame(&pic, &info) != 0) {
        return -1;
    }
    if (info.eFrameType == videoFrameTypeSkip) {
        return 0;
    }
    int offset = 0;
    for (int layerNum = 0; layerNum < info.iLayerNum; ++layerNum) {
        const SLayerBSInfo* layer = &info.sLayerInfo[layerNum];
        const uint8_t* data = layer->pBsBuf;
        for (int i = 0; i < layer->iNalCount; ++i) {
            const int len = layer->pNalLengthInByte[i];
            if (offset + len > dstSize) {
                return offset;
            }
            memcpy(dst, data, len);
            dst += len;
            data += len;
            offset += len;
        }
    }
    return offset;
}

void
destroy_h264_encoder(void* ptr)
{
    ISVCEncoder* encoder = reinterpret_cast<ISVCEncoder*>(ptr);

    if (encoder) {
        encoder->Uninitialize();
        WelsDestroySVCEncoder(encoder);
    }
}

bool
create_h264_decoder(void** ptr)
{
    ISVCDecoder** decoderPtr = reinterpret_cast<ISVCDecoder**>(ptr);
    const int result = WelsCreateDecoder(decoderPtr);
    ISVCDecoder* decoder = *decoderPtr;
    if (result != 0 || decoder == nullptr) {
        return false;
    }

    SDecodingParam param;
    memset(&param, 0, sizeof(SDecodingParam));

    // param.eEcActiveIdc = ERROR_CON_DISABLE;
    param.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    param.bParseOnly = false;

    int log = 2;
    return decoder->Initialize(&param) == 0 &&
           decoder->SetOption(DECODER_OPTION_TRACE_LEVEL, &log) == 0;
}

bool
h264_decode(void* ptr,
            unsigned char* src,
            const int srcSize,
            unsigned char* ptrs[3],
            int* width,
            int* height,
            int strides[2],
            bool* status,
            const bool continuation)
{
    ISVCDecoder* decoder = reinterpret_cast<ISVCDecoder*>(ptr);

    SBufferInfo info;
    memset(&info, 0, sizeof(SBufferInfo));

    if (continuation) {
        if (decoder->DecodeFrame2(NULL, 0, ptrs, &info) != 0) {
            return false;
        }
    } else if (decoder->DecodeFrame2(src, srcSize, ptrs, &info) != 0) {
        return false;
    }

    *width = info.UsrData.sSystemBuffer.iWidth;
    *height = info.UsrData.sSystemBuffer.iHeight;
    strides[0] = info.UsrData.sSystemBuffer.iStride[0];
    strides[1] = info.UsrData.sSystemBuffer.iStride[1];
    *status = info.iBufferStatus == 1;
    return true;
}

void
destroy_h264_decoder(void* ptr)
{
    ISVCDecoder* decoder = reinterpret_cast<ISVCDecoder*>(ptr);

    if (decoder) {
        decoder->Uninitialize();
        WelsDestroyDecoder(decoder);
    }
}