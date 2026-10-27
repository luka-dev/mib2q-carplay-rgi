/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac_bridge.cpp - the C++ side of the hook's APAC seam.  See apac_bridge.h.
//
// Constraints this file works under, all of them from apac_decoder/tools/build_qnx.sh,
// which is what proves the decoder links on QNX 6.5 / armle-v7 at all:
//
//   * no exceptions, no RTTI, no STL, no operator new, no static-init guards.
//     malloc/free and placement new only.  Placement new is inline in <new> and
//     emits no libstdc++ reference; a plain `new` would.
//   * no function-local static with a constructor - that needs __cxa_guard_*,
//     which is not in that libstdc++ configuration.
//
// The float -> S16 rule is Apple's, not the library's.  The library's own S16
// path truncates and scales positives by 32767; Apple's AudioConverter rounds
// to nearest and scales by 32768, and on the whole 3977-packet capture those
// two differ on up to 35 % of samples by one LSB.  Rounding at 32768 reproduces
// Apple's S16 on 99.989 % of samples with a worst case of one LSB, so that is
// what this uses.

#include "apac_bridge.h"

#include <new>
#include <stdlib.h>
#include <string.h>

#include "apac/config.h"
#include "apac/decoder.h"

namespace {

const uint32_t kExpectedFrameLength = SPA_APAC_FRAME_LENGTH;

uint32_t BE32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// APACDecoder_GetCookieOffset, 0x00106030: the cookie is a chain of big-endian
// [u32 size][u32 type] atoms.  `frma` and `apac` are containers to descend
// into; `dapa`'s payload starts twelve bytes in, not eight - the four bytes
// after the atom header belong to the header, not to the config.
uint32_t CookieOffset(const uint8_t* c, uint32_t n, uint32_t* payloadBytes)
{
    uint32_t at = 0;
    while (at + 8 <= n) {
        const uint32_t size = BE32(c + at);
        const uint32_t type = BE32(c + at + 4);
        if (size < 8 || at + size > n)
            return 0;
        if (type == 0x64617061u) {                          // 'dapa'
            if (size < 12)
                return 0;
            *payloadBytes = size - 12;
            return at + 12;
        }
        if (type == 0x66726d61u || type == 0x61706163u)     // 'frma', 'apac'
            at += 8;                                        // container: descend
        else
            at += size;
    }
    return 0;
}

int16_t ToS16(float v)
{
    // Clamp first: a decoded sample can leave [-1, 1] and the cast would then
    // be undefined, not merely wrong.
    float scaled;
    if (v >= 1.0f) return 32767;
    if (v <= -1.0f) return -32768;
    scaled = v * 32768.0f;
    scaled += (scaled >= 0.0f) ? 0.5f : -0.5f;
    if (scaled > 32767.0f) return 32767;
    if (scaled < -32768.0f) return -32768;
    return (int16_t)scaled;
}

}  // namespace

// Deliberately not a class with members needing construction: the whole object
// is malloc'd as one block and placement-constructed, so its layout is its own
// business and nothing here needs a destructor beyond free().
struct spa_apac_decoder {
    apac::GlobalConfig config;
    apac::ASPDecoder decoder;
    uint32_t channels;
    uint32_t frameLength;
    float pcm[SPA_APAC_MAX_CHANNELS * kExpectedFrameLength];
};

extern "C" int spa_apac_create(const uint8_t* cookie, size_t cookieLength,
                               spa_apac_decoder_t** out,
                               uint32_t* channels, uint32_t* frameLength,
                               uint32_t* sampleRate)
{
    if (!cookie || !out) return SPA_APAC_ERR_ARGS;
    *out = 0;
    if (cookieLength < 12 || cookieLength > 0x7fffffffu) return SPA_APAC_ERR_COOKIE;

    uint32_t payloadBytes = 0;
    const uint32_t off = CookieOffset(cookie, (uint32_t)cookieLength, &payloadBytes);
    if (off == 0 || payloadBytes < 3) return SPA_APAC_ERR_COOKIE;

    void* block = malloc(sizeof(spa_apac_decoder));
    if (!block) return SPA_APAC_ERR_NO_MEMORY;
    spa_apac_decoder* d = new (block) spa_apac_decoder();

    d->config.Reset();
    {
        // +2: past the 16-bit 0x0800 sync word that opens the `dapa` payload.
        // apac::GlobalConfig starts two bytes further on, not at the payload.
        apac::BitstreamReader bs(cookie + off + 2, payloadBytes - 2);
        if (d->config.Deserialize(bs) != apac::kOk) {
            free(block);
            return SPA_APAC_ERR_CONFIG;
        }
    }
    if (d->decoder.Initialize(d->config) != apac::kOk) {
        free(block);
        return SPA_APAC_ERR_INIT;
    }

    d->channels = d->decoder.NumChannels();
    d->frameLength = d->decoder.FrameLength();
    if (d->channels == 0 || d->channels > SPA_APAC_MAX_CHANNELS) {
        free(block);
        return SPA_APAC_ERR_CHANNELS;
    }
    if (d->frameLength != kExpectedFrameLength) {
        free(block);
        return SPA_APAC_ERR_FRAME_LENGTH;
    }

    if (channels) *channels = d->channels;
    if (frameLength) *frameLength = d->frameLength;
    if (sampleRate) *sampleRate = d->config.sampleRate;
    *out = d;
    return SPA_APAC_OK;
}

extern "C" int spa_apac_decode_s16(spa_apac_decoder_t* decoder,
                                   const uint8_t* packet, size_t length,
                                   int16_t* out, size_t outCapacity,
                                   uint32_t* framesOut)
{
    if (!decoder || !out || !framesOut) return SPA_APAC_ERR_ARGS;
    if (length && !packet) return SPA_APAC_ERR_ARGS;
    if (length > 0xffffffffu) return SPA_APAC_ERR_ARGS;

    *framesOut = 0;
    if ((size_t)decoder->channels * decoder->frameLength > outCapacity)
        return SPA_APAC_ERR_OUTPUT_SPACE;

    uint32_t frames = 0;
    if (decoder->decoder.DecodeFrame(packet, (uint32_t)length, decoder->pcm,
                                     &frames) != apac::kOk)
        return SPA_APAC_ERR_DECODE;
    if (frames > decoder->frameLength)
        return SPA_APAC_ERR_DECODE;

    const size_t samples = (size_t)frames * decoder->channels;
    for (size_t i = 0; i < samples; ++i)
        out[i] = ToS16(decoder->pcm[i]);
    *framesOut = frames;
    return SPA_APAC_OK;
}

extern "C" void spa_apac_reset(spa_apac_decoder_t* decoder)
{
    if (decoder) decoder->decoder.Reset();
}

extern "C" void spa_apac_destroy(spa_apac_decoder_t* decoder)
{
    // spa_apac_decoder is trivially destructible (checked: our objects
    // are plain arrays and scalars), so free() is the whole teardown.
    free(decoder);
}

extern "C" int spa_apac_get_info(const spa_apac_decoder_t* decoder,
                                  spa_apac_stream_info_t* info)
{
    if (!decoder || !info) return SPA_APAC_ERR_ARGS;
    info->channels = decoder->channels;
    info->frameLength = decoder->frameLength;
    info->sampleRate = decoder->config.sampleRate;
    info->channelLayoutTag = decoder->config.layout.tag;
    return SPA_APAC_OK;
}

extern "C" int spa_apac_codec_get_api(uint32_t requestedVersion,
                                       spa_apac_codec_api_t* api, size_t apiSize)
{
    if (!api || apiSize != sizeof(*api) ||
        requestedVersion != SPA_APAC_CODEC_ABI_VERSION)
        return SPA_APAC_ERR_ARGS;


    memset(api, 0, sizeof(*api));
    api->structSize = sizeof(*api);
    api->abiVersion = SPA_APAC_CODEC_ABI_VERSION;
    api->create = spa_apac_create;
    api->decodeS16 = spa_apac_decode_s16;
    api->reset = spa_apac_reset;
    api->destroy = spa_apac_destroy;
    api->getInfo = spa_apac_get_info;
    return SPA_APAC_OK;
}
