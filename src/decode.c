#include <include/main.h>

bool
decodeWAV(const void* data, const size_t size, pBytes bytes)
{
    const Bytes audio = { .buffer = (uint8_t*)data,
                          .size = size,
                          .used = size };

    union
    {
        uint32_t u;
        char c[4];
    } type;

    size_t offset = 0;
    offset += uint32_tDeserialize(&type.u, &audio, offset, true);

    if (memcmp(&type, "RIFF", 4) != 0) {
        fprintf(stderr,
                "Did not get 'RIFF' in WAV header. Got '%c' '%c' '%c' '%c'\n",
                type.c[0],
                type.c[1],
                type.c[2],
                type.c[3]);
        return false;
    }

    uint32_t audioSize;
    offset += uint32_tDeserialize(&audioSize, &audio, offset, true);

    offset += uint32_tDeserialize(&type.u, &audio, offset, true);
    if (memcmp(&type, "WAVE", 4) != 0) {
        fprintf(stderr,
                "Did not get 'WAVE' in WAV header. Got '%c' '%c' '%c' '%c'\n",
                type.c[0],
                type.c[1],
                type.c[2],
                type.c[3]);
        return false;
    }

    offset += uint32_tDeserialize(&type.u, &audio, offset, true);
    if (memcmp(&type, "fmt ", 4) != 0) {
        fprintf(stderr,
                "Did not get 'fmt ' in WAV header. Got '%c' '%c' '%c' '%c'\n",
                type.c[0],
                type.c[1],
                type.c[2],
                type.c[3]);
        return false;
    }

    uint32_t chunkSize;
    offset += uint32_tDeserialize(&chunkSize, &audio, offset, true);

    int16_t formatType;
    offset += int16_tDeserialize(&formatType, &audio, offset, true);

    if (formatType != 1) {
        fprintf(stderr,
                "Only PCM wav files are supported. Got %d (1 = PCM)\n",
                formatType);
        return false;
    }

    int16_t channels;
    offset += int16_tDeserialize(&channels, &audio, offset, true);

    uint32_t sampleRate;
    offset += uint32_tDeserialize(&sampleRate, &audio, offset, true);

    uint32_t avgBytesPerSec;
    offset += uint32_tDeserialize(&avgBytesPerSec, &audio, offset, true);

    int16_t bytesPerSample;
    offset += int16_tDeserialize(&bytesPerSample, &audio, offset, true);

    int16_t bitsPerSample;
    offset += int16_tDeserialize(&bitsPerSample, &audio, offset, true);

    offset += uint32_tDeserialize(&type.u, &audio, offset, true);
    if (memcmp(&type, "data", 4) != 0) {
        fprintf(stderr,
                "Did not get 'data' in WAV header. Got '%c' '%c' '%c' '%c'\n",
                type.c[0],
                type.c[1],
                type.c[2],
                type.c[3]);
        return false;
    }

    uint32_t dataSize;
    SDL_AudioFormat format;
    switch (bitsPerSample) {
        case 8:
            format = AUDIO_U8;
            goto readAudio;
        case 16:
            format = AUDIO_S16;
            goto readAudio;
        case 32:
            format = AUDIO_S32;
            goto readAudio;
        default:
            break;
    }
    fprintf(stderr,
            "Wav has invalid channels (%d) or bitsPerSample (%d)\n",
            channels,
            bitsPerSample);
    return false;

readAudio:

    offset += uint32_tDeserialize(&dataSize, &audio, offset, true);

    uint8_tListQuickAppend(bytes, &audio.buffer[offset], dataSize);
    printf("Wav data %d bit format %u; channels %d; sample rate %u\n",
           bitsPerSample,
           format,
           channels,
           sampleRate);

    const SDL_AudioSpec spec = makeAudioSpec(NULL, NULL);
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt,
                          format,
                          channels,
                          sampleRate,
                          spec.format,
                          spec.channels,
                          spec.freq) < 0) {
        fprintf(stderr, "Failed make audio converter: %s\n", SDL_GetError());
        return false;
    }

    if (cvt.needed) {
        cvt.len = bytes->used;
        cvt.buf = bytes->buffer;
        const uint32_t newSize = (uint32_t)(cvt.len * cvt.len_mult);
        if (bytes->size < newSize) {
            bytes->buffer =
              currentAllocator->reallocate(bytes->buffer, newSize);
            bytes->size = newSize;
        }
        printf("Converting %d Hz %d channels to %d Hz %d channels\n",
               sampleRate,
               channels,
               spec.freq,
               spec.channels);
        if (SDL_ConvertAudio(&cvt) != 0) {
            fprintf(stderr, "Failed to convert audio: %s\n", SDL_GetError());
            return false;
        }
        bytes->used = cvt.len_cvt;
    }

    return true;
}

bool
decodeOgg(const void* data, const size_t dataSize, pBytes bytes)
{
    ogg_sync_state oy;
    ogg_sync_init(&oy);
    ogg_stream_state os;

    vorbis_comment vc;
    vorbis_info vi;

    size_t bytesRead = 0;

    bool success = true;
    const SDL_AudioSpec spec = makeAudioSpec(NULL, NULL);
    while (bytesRead < dataSize) {
        size_t writeSize;

        writeSize = SDL_min(KB(4), dataSize - bytesRead);
        char* buffer = ogg_sync_buffer(&oy, writeSize);
        memcpy(buffer, data + bytesRead, writeSize);
        ogg_sync_wrote(&oy, writeSize);
        bytesRead += writeSize;

        ogg_page og;
        if (ogg_sync_pageout(&oy, &og) != 1) {
            if (bytesRead >= writeSize) {
                break;
            }

            fprintf(stderr, "Failed to parse ogg dta\n");
            success = false;
            break;
        }

        ogg_stream_init(&os, ogg_page_serialno(&og));

        vorbis_info_init(&vi);
        vorbis_comment_init(&vc);

        if (ogg_stream_pagein(&os, &og) < 0) {
            fprintf(stderr, "Failed to read first page of ogg data\n");
            success = false;
            break;
        }

        ogg_packet op;
        if (ogg_stream_packetout(&os, &op) != 1) {
            fprintf(stderr, "Failed to read header packet\n");
            success = false;
            break;
        }

        if (vorbis_synthesis_headerin(&vi, &vc, &op) < 0) {
            fprintf(stderr, "Vorbis audio data not present\n");
            success = false;
            break;
        }

        int i = 0;
        while (i < 2) {
            while (i < 2) {
                int result = ogg_sync_pageout(&oy, &og);
                if (result == 0) {
                    break;
                }
                if (result == 1) {
                    ogg_stream_pagein(&os, &og);
                    while (i < 2) {
                        result = ogg_stream_packetout(&os, &op);
                        if (result == 0) {
                            break;
                        }
                        if (result < 0) {
                            fprintf(stderr, "Corrupt header in ogg data\n");
                            success = false;
                            goto end;
                        }
                        result = vorbis_synthesis_headerin(&vi, &vc, &op);
                        if (result < 0) {
                            fprintf(stderr, "Corrupt header in ogg data\n");
                            success = false;
                            goto end;
                        }
                        ++i;
                    }
                }
            }
            writeSize = SDL_min(KB(4), dataSize - bytesRead);
            buffer = ogg_sync_buffer(&oy, writeSize);
            memcpy(buffer, data + bytesRead, writeSize);
            ogg_sync_wrote(&oy, writeSize);
            bytesRead += writeSize;
        }

        if (vc.user_comments != NULL) {
            char** ptr = vc.user_comments;
            while (*ptr != NULL) {
                printf("%s\n", *ptr);
                ++ptr;
            }
            printf("BitStream is %d channel(s), %ldHz\n", vi.channels, vi.rate);
            printf("Encoded by: %s\n", vc.vendor);
        }

        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt,
#if !HIGH_QUALITY_AUDIO
                              AUDIO_S16,
#else
                              AUDIO_F32,
#endif
                              vi.channels,
                              vi.rate,
                              spec.format,
                              spec.channels,
                              spec.freq) < 0) {
            fprintf(
              stderr, "Failed to make audio converter: %s\n", SDL_GetError());
        }

        vorbis_dsp_state vd;
        if (vorbis_synthesis_init(&vd, &vi) == 0) {
            vorbis_block vb;
            vorbis_block_init(&vd, &vb);

            bool done = false;
            while (!done) {
                while (!done) {
                    int result = ogg_sync_pageout(&oy, &og);
                    if (result == 0) {
                        break;
                    }
                    if (result < 0) {
                        fprintf(stderr,
                                "Ogg data might be corrupt; Cotinuing...\n");
                        continue;
                    }
                    ogg_stream_pagein(&os, &og);
                    while (true) {
                        result = ogg_stream_packetout(&os, &op);
                        if (result == 0) {
                            break;
                        }
                        if (result < 0) {
                            continue;
                        }
                        float** pcm;
                        int samples;

                        if (vorbis_synthesis(&vb, &op) == 0) {
                            vorbis_synthesis_blockin(&vd, &vb);
                        }

                        int convbuffersize = MAX_PACKET_SIZE;
#if !HIGH_QUALITY_AUDIO
                        ogg_int16_t*
#else
                        float*
#endif
                          convbuffer =
                            currentAllocator->allocate(convbuffersize);
                        while ((samples = vorbis_synthesis_pcmout(&vd, &pcm)) >
                               0) {
                            const int bout = samples < convbuffersize
                                               ? samples
                                               : convbuffersize;
#if !HIGH_QUALITY_AUDIO
                            bool clipflag = false;
                            for (int i = 0; i < vi.channels; ++i) {
                                ogg_int16_t* ptr = convbuffer + i;
                                const float* mono = pcm[i];
                                for (int j = 0; j < samples; ++j) {
                                    int val =
                                      (int)floorf(mono[j] * 32767.f + 0.5f);
                                    if (val > 32767) {
                                        val = 32767;
                                        clipflag = true;
                                    }
                                    if (val < -32768) {
                                        val = -32768;
                                        clipflag = true;
                                    }
                                    *ptr = val;
                                    ptr += vi.channels;
                                }
                            }
                            if (clipflag) {
                                fprintf(stderr,
                                        "Clipping in frame %" PRId64 "\n",
                                        vd.sequence);
                            }
#else
                            for (int i = 0; i < vi.channels; ++i) {
                                float* ptr = convbuffer + i;
                                const float* mono = pcm[i];
                                for (int j = 0; j < samples; ++j) {
                                    *ptr = mono[j];
                                    ptr += vi.channels;
                                }
                            }
#endif
                            if (cvt.needed) {
#if !HIGH_QUALITY_AUDIO
                                cvt.len =
                                  sizeof(ogg_int16_t) * vi.channels * bout;
#else
                                cvt.len = sizeof(float) * vi.channels * bout;
                                memcpy(convbuffer, *pcm, cvt.len);
#endif
                                cvt.buf = (uint8_t*)convbuffer;
                                if (convbuffersize < cvt.len * cvt.len_mult) {
                                    convbuffersize = cvt.len * cvt.len_mult;
                                    convbuffer = currentAllocator->reallocate(
                                      convbuffer, convbuffersize);
                                }
                                SDL_ConvertAudio(&cvt);
                                uint8_tListQuickAppend(
                                  bytes, (uint8_t*)convbuffer, cvt.len_cvt);
                            } else {
#if !HIGH_QUALITY_AUDIO
                                uint8_tListQuickAppend(bytes,
                                                       (uint8_t*)convbuffer,
                                                       sizeof(ogg_int16_t) *
                                                         vi.channels * bout);
#else
                                uint8_tListQuickAppend(bytes,
                                                       (uint8_t*)convbuffer,
                                                       sizeof(float) *
                                                         vi.channels * bout);
#endif
                            }
                            vorbis_synthesis_read(&vd, bout);
                        }
                        currentAllocator->free(convbuffer);
                    }
                    if (ogg_page_eos(&og)) {
                        done = true;
                    }
                }
                if (!done) {
                    writeSize = SDL_min(KB(4), dataSize - bytesRead);
                    buffer = ogg_sync_buffer(&oy, writeSize);
                    memcpy(buffer, data + bytesRead, writeSize);
                    ogg_sync_wrote(&oy, writeSize);
                    bytesRead += writeSize;
                    done = bytesRead >= dataSize;
                }
            }

            vorbis_block_clear(&vb);
            vorbis_dsp_clear(&vd);
        } else {
            fprintf(stderr, "Corrupt header during ogg decoding\n");
        }

        ogg_stream_clear(&os);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
    }

end:
    ogg_stream_clear(&os);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    ogg_sync_clear(&oy);
    return success;
}

bool
decodeMp3(const void* data, const size_t dataSize, pBytes bytes)
{
    mp3dec_frame_info_t info;
    mp3d_sample_t* pcm = currentAllocator->allocate(MAX_PACKET_SIZE);
    mp3dec_t* mp3d = currentAllocator->allocate(sizeof(mp3dec_t));
    mp3dec_init(mp3d);
    size_t bytesRead = 0;
    const SDL_AudioSpec spec = makeAudioSpec(NULL, NULL);
    while (bytesRead < dataSize) {
        const size_t readSize = SDL_min(KB(16), dataSize - bytesRead);
        const int samples =
          mp3dec_decode_frame(mp3d, data + bytesRead, readSize, pcm, &info);
        printf("Read %d samples (%d bytes)\n", samples, info.frame_bytes);
        bytesRead += info.frame_bytes;

        if (info.frame_bytes == 0 && samples == 0) {
            fprintf(stderr, "Insufficient mp3 data\n");
            break;
        }

        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt,
                              AUDIO_S16,
                              info.channels,
                              info.hz,
                              spec.format,
                              spec.channels,
                              spec.freq) < 0) {
            fprintf(stderr, "Failed to build converter: %s\n", SDL_GetError());
            break;
        }
        const size_t len = samples * sizeof(mp3d_sample_t) * info.channels;
        if (cvt.needed) {
            cvt.buf = (uint8_t*)pcm;
            cvt.len = len;
            if (SDL_ConvertAudio(&cvt) != 0) {
                fprintf(
                  stderr, "Failed to convert audio: %s\n", SDL_GetError());
                break;
            }
            uint8_tListQuickAppend(bytes, cvt.buf, cvt.len_cvt);
        } else {
            uint8_tListQuickAppend(bytes, (uint8_t*)pcm, len);
        }
    }
    currentAllocator->free(pcm);
    currentAllocator->free(mp3d);
    return true;
}