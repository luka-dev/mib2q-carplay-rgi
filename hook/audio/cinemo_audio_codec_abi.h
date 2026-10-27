/*
 * cinemo_audio_codec_abi.h -- recovered ARMv7 Cinemo AAC decoder ABI for
 * CarPlay buffered stream type 103.
 *
 * This is an ABI contract, not an enabled decoder.  The declarations below
 * were recovered from the target MU1316 libNmeSDK.so and checked against the
 * working MH2p AirPlay AAC adapter.  Keep SPATIAL_103 disabled until
 * the authenticated APAP receiver, bounded queue, decoder and renderer are
 * connected as one fail-closed path.
 *
 * Important: use the exported C wrappers named below.  Do not copy MH2p C++
 * objects or call target-private vtable addresses directly.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef CINEMO_AUDIO_CODEC_ABI_H
#define CINEMO_AUDIO_CODEC_ABI_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_CINEMO_MEDIA_TYPE_PCM       = 2,
    SPA_CINEMO_MEDIA_TYPE_AUDIO_ES  = 5,

    SPA_CINEMO_SUBTYPE_AAC_LC       = 0x101a,
    SPA_CINEMO_SUBTYPE_AAC_ELD      = 0x1022,
    SPA_CINEMO_SUBTYPE_PCM          = 0x1023,

    SPA_CINEMO_PCM_FORMAT_S16       = 2,
    SPA_CINEMO_HEADER_TYPE_ASC      = 5,

    SPA_CINEMO_RATE_48K             = 48000,
    SPA_CINEMO_CHANNELS_5_1_2       = 8,
    SPA_CINEMO_ASC_SIZE             = 4
};

/* Exact 0x80-byte CinemoMediaType layout used at the public wrapper boundary.
 * The target is 32-bit ARM, so the pointer stored at +0x78 is represented as
 * a uint32_t.  That also keeps host-side layout checks deterministic. */
typedef struct spa_cinemo_media_type {
    uint32_t media_type;             /* +0x00 */
    uint32_t media_subtype;          /* +0x04 */
    uint32_t sample_format;          /* +0x08: 2 for decoded S16 PCM */
    uint32_t sample_rate;            /* +0x0c */
    uint32_t channels;               /* +0x10 */
    uint8_t  reserved_14[0x60];      /* +0x14 .. +0x73 */
    uint32_t header_type;            /* +0x74: 5 for AudioSpecificConfig */
    uint32_t header_data_arm32;      /* +0x78: target address */
    uint32_t header_size;            /* +0x7c */
} spa_cinemo_media_type_t;

typedef int32_t (*spa_cinemo_decoded_callback_t)(
    void *context,
    const void *pcm,
    uint32_t pcm_bytes,
    const spa_cinemo_media_type_t *format,
    int64_t pts,
    uint32_t flags);

/* dlsym names and exact call shapes:
 *   CinemoCreateAudioCodec
 *   ICinemoAudioCodec_Initialize
 *   ICinemoAudioCodec_Write
 *   ICinemoAudioCodec_Drain
 *   ICinemoAudioCodec_Flush
 *   ICinemoAudioCodec_Release
 */
typedef int32_t (*spa_cinemo_create_audio_codec_fn)(void **out_codec);
typedef int32_t (*spa_cinemo_audio_initialize_fn)(
    void *codec,
    void *callback_context,
    spa_cinemo_decoded_callback_t callback,
    const spa_cinemo_media_type_t *input,
    const spa_cinemo_media_type_t *output);
typedef int32_t (*spa_cinemo_audio_write_fn)(
    void *codec,
    const void *access_unit,
    uint32_t access_unit_bytes,
    uint32_t mh2p_arg3,
    int64_t pts,
    uint32_t mh2p_arg6);
typedef int32_t (*spa_cinemo_audio_simple_fn)(void *codec);
typedef uint32_t (*spa_cinemo_audio_release_fn)(void *codec);

/* Exact MH2p-generated 48 kHz / 8-channel configs.  Index 40 is the first
 * production candidate.  Index 42 remains a fallback until separately tested. */
static const uint8_t SPA_CINEMO_ASC_AAC_LC_48K_5_1_2[SPA_CINEMO_ASC_SIZE] = {
    0x11, 0xc0, 0x00, 0x00
};
static const uint8_t SPA_CINEMO_ASC_AAC_ELD_48K_5_1_2[SPA_CINEMO_ASC_SIZE] = {
    0x3e, 0x39, 0xc0, 0x00
};

/* C99-compatible ABI assertions (the QNX hook is built as gnu99). */
typedef char spa_cinemo_assert_media_type_size[
    sizeof(spa_cinemo_media_type_t) == 0x80 ? 1 : -1];
typedef char spa_cinemo_assert_header_type_offset[
    offsetof(spa_cinemo_media_type_t, header_type) == 0x74 ? 1 : -1];
typedef char spa_cinemo_assert_header_data_offset[
    offsetof(spa_cinemo_media_type_t, header_data_arm32) == 0x78 ? 1 : -1];
typedef char spa_cinemo_assert_header_size_offset[
    offsetof(spa_cinemo_media_type_t, header_size) == 0x7c ? 1 : -1];

#endif /* CINEMO_AUDIO_CODEC_ABI_H */
