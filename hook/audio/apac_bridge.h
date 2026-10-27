/*
 * apac_bridge.h — the hook's C view of the APAC decoder.
 *
 * Our decoder under apac_decoder/ is C++; the hook is C.  This is the whole seam
 * between them: one opaque handle and a versioned C function table, with no
 * C++ types crossing over.  The table lets the main LD_PRELOAD hook dlopen the
 * decoder only when Spatial audio is enabled; ordinary CarPlay remains usable
 * if the optional decoder library is absent or rejected.
 *
 * The decoder object is 1.4 MB (its synthesis filter banks carry a
 * channels x 1024 float overlap buffer each), so it is heap-allocated at stream
 * start and freed at teardown.  It must never go on a stack, and there is
 * exactly one buffered media stream, so there is never more than one alive.
 *
 * Output is INTERLEAVED S16, in the DECODER's own channel order (C L R Ls Rs
 * LFE for the measured CarPlay index-65 stream) — no permutation is applied here.
 * spa_reorder_5_1_decoder_to_declared_s16() in spatial_downmix.h is what turns
 * that into the declared order, and it is a separate step on purpose: the two
 * orders were established independently and conflating them is how the channel
 * map went wrong the first time.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef APAC_BRIDGE_H
#define APAC_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes.  Everything below kSpaApacDecodeError is a setup problem the
 * caller can report and stop on; a decode error is per packet. */
enum {
    SPA_APAC_OK = 0,
    SPA_APAC_ERR_ARGS = 1,
    SPA_APAC_ERR_NO_MEMORY = 2,
    SPA_APAC_ERR_COOKIE = 3,        /* no `dapa` atom, or it is too short */
    SPA_APAC_ERR_CONFIG = 4,        /* GlobalConfig::Deserialize refused it */
    SPA_APAC_ERR_INIT = 5,          /* Decoder::Initialize refused the config */
    SPA_APAC_ERR_CHANNELS = 6,      /* outside the decoder's 1..16 channel capacity */
    SPA_APAC_ERR_FRAME_LENGTH = 7,  /* not 1024 samples per packet */
    SPA_APAC_ERR_DECODE = 8,        /* one packet failed; see the decoder status */
    SPA_APAC_ERR_OUTPUT_SPACE = 9
};

typedef struct spa_apac_decoder spa_apac_decoder_t;

enum {
    SPA_APAC_CODEC_ABI_VERSION = 1,
    SPA_APAC_MAX_CHANNELS = 16,
    SPA_APAC_FRAME_LENGTH = 1024
};

typedef struct {
    uint32_t channels;
    uint32_t frameLength;
    uint32_t sampleRate;
    uint32_t channelLayoutTag; /* cookie GlobalConfig layout; 0 means unspecified */
} spa_apac_stream_info_t;

/* Parse `cookie` (the whole SETMAGICCOOKIE payload, atom chain and all) and
 * bring up a decoder for it.  On success *out receives the handle and the three
 * geometry outputs are filled; on failure *out is left null.
 *
 * The decoder created here is the ASP one, which is what APACDecoder_Create
 * hands back and what a CarPlay stream needs: its frame-type field is two bits
 * wide, and an Audio Synchronization Packet is the only legal entry point into
 * a running stream — so it is also the only legal resume point after a flush.
 */
int spa_apac_create(const uint8_t *cookie, size_t cookieLength,
                    spa_apac_decoder_t **out,
                    uint32_t *channels, uint32_t *frameLength,
                    uint32_t *sampleRate);

/* Decode one APAC packet to interleaved S16 in the decoder's own channel order.
 * `outCapacity` is in SAMPLES, not bytes, and must be at least
 * channels * frameLength.  A zero `length` is the flush path, exactly as
 * APACDecoder_DecodeFrame treats it: reset, and produce no frames.
 *
 * Returns SPA_APAC_OK and sets *framesOut, or a status code.
 */
int spa_apac_decode_s16(spa_apac_decoder_t *decoder,
                        const uint8_t *packet, size_t length,
                        int16_t *out, size_t outCapacity, uint32_t *framesOut);

/* APACDecoder::Reset — clears the filter banks' overlap-add memory only, which
 * is what a FLUSHBUFFERED has to do.  The next packet is then windowed against
 * silence rather than against the frame before the flush. */
void spa_apac_reset(spa_apac_decoder_t *decoder);

void spa_apac_destroy(spa_apac_decoder_t *decoder);

int spa_apac_get_info(const spa_apac_decoder_t *decoder, spa_apac_stream_info_t *info);

/* Stable ABI exported by libcarplay_apac.so.  `structSize` and `abiVersion`
 * are checked on both sides so an independently updated decoder fails closed
 * instead of calling through a drifted function table. */
typedef struct {
    uint32_t structSize;
    uint32_t abiVersion;
    int (*create)(const uint8_t *, size_t, spa_apac_decoder_t **,
                  uint32_t *, uint32_t *, uint32_t *);
    int (*decodeS16)(spa_apac_decoder_t *, const uint8_t *, size_t,
                     int16_t *, size_t, uint32_t *);
    void (*reset)(spa_apac_decoder_t *);
    void (*destroy)(spa_apac_decoder_t *);
    int (*getInfo)(const spa_apac_decoder_t *, spa_apac_stream_info_t *);
} spa_apac_codec_api_t;

int spa_apac_codec_get_api(uint32_t requestedVersion,
                           spa_apac_codec_api_t *api, size_t apiSize);

#ifdef __cplusplus
}
#endif

#endif /* APAC_BRIDGE_H */
