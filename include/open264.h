#pragma once

#include <stddef.h>
#include <stdint.h>

#if __cplusplus
extern "C"
{
#endif

    bool create_h264_encoder(void** encoder,
                             const int width,
                             const int height,
                             const float frameRate,
                             const int bitrate);

    // -1 = failure
    // 0 = skip frame
    // > 0 = bytes encoded
    int h264_encode(void*,
                    unsigned char* src,
                    const int width,
                    const int height,
                    uint8_t* dst,
                    size_t);

    void destroy_h264_encoder(void*);

    bool create_h264_decoder(void** decoder);

    bool h264_decode(void*,
                     unsigned char* src,
                     int size,
                     unsigned char* yuv[3],
                     int* width,
                     int* height,
                     int strides[2],
                     bool* status,
                     const bool continuation);

    void destroy_h264_decoder(void*);

#if __cplusplus
}
#endif