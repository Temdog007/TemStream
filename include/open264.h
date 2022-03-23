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

    int h264_decode(void*,
                    unsigned char* src,
                    int size,
                    uint8_t* dst,
                    size_t,
                    const bool continuation);

    void destroy_h264_decoder(void*);

#if __cplusplus
}
#endif