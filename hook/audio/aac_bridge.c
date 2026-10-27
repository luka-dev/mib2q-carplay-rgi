/* aac_bridge.c — AAC-LC decoding for the buffered type-103 stream, via Cinemo.
 *
 * Why this exists.  iOS does not upmix stereo into the spatial format, so a receiver that
 * advertises only APAC gets Atmos tracks and nothing else -- the phone skips what it cannot
 * deliver.  Index 23 carries ordinary content as AAC-LC at 48000 Hz, which needs no resampler
 * because every HU media endpoint is 48 kHz. iOS has an in-session transport-format-change
 * path; APAP carries both a per-packet format hint and a changed inline cookie, so this adapter
 * can alternate with APAC without recreating the type-103 session. Full reasoning:
 * docs/audio/BUFFERED_AUDIO_PATH.md.
 *
 * It presents the same function table as the APAC decoder (spa_apac_codec_api_t). The render path
 * tries the current API and then its alternate; the cookie-accepting decoder plus returned PCM
 * geometry/layout selects stereo upmix or native APAC processing.
 *
 * Unlike the APAC decoder this is not a separate library: libNmeSDK.so is already in
 * dio_manager's DT_NEEDED and exports every entry point by name, so plain dlsym reaches them
 * and no vtable address appears here.  dio_manager imports only Cinemo's Config, Metapool and
 * iAP helpers and never creates an audio codec, so nothing in this process shares one with us.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "apac_bridge.h"
#include "aac_fifo.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

/* CinemoMediaType: 0x80 bytes, zero-initialised, fields at the offsets recovered from the
 * working donor.  Declared as a byte array with accessors rather than a struct so a wrong
 * assumption about padding cannot silently shift a field. */
#define CMT_SIZE            0x80
#define CMT_MEDIA_TYPE      0x00
#define CMT_SUBTYPE         0x04
#define CMT_SAMPLE_FORMAT   0x08
#define CMT_SAMPLE_RATE     0x0c
#define CMT_CHANNELS        0x10
#define CMT_HEADER_TYPE     0x74
#define CMT_HEADER_DATA     0x78
#define CMT_HEADER_SIZE     0x7c

#define CMT_MEDIA_AUDIO_ES  5
#define CMT_MEDIA_PCM       2
#define CMT_SUB_AAC_LC      0x101a
#define CMT_SUB_PCM         0x1023
#define CMT_FMT_S16         2
#define CMT_HDR_ASC         5

/* One AAC-LC access unit is nominally 1024 frames, but the decoder may emit zero or several
 * callbacks per Write, so the adapter accumulates rather than assuming one. */
#define AAC_FIFO_FRAMES     (4u * 1024u)
#define AAC_MAX_CHANNELS    2u

typedef int32_t (*cinemo_create_t)(void **);
typedef int32_t (*cinemo_init_t)(void *, void *, void *, const void *, const void *);
typedef int32_t (*cinemo_write_t)(void *, const void *, uint32_t, int32_t, int64_t, int32_t);
typedef int32_t (*cinemo_simple_t)(void *);

static struct {
    int resolved;
    cinemo_create_t create;
    cinemo_init_t   initialize;
    cinemo_write_t  write;
    cinemo_simple_t drain;
    cinemo_simple_t flush;
    cinemo_simple_t release;
} g_cinemo;

struct spa_aac_decoder {
    void     *codec;
    uint8_t  *asc;                 /* heap, handed to the codec: IT frees this, we must not */
    uint32_t  ascLength;
    uint32_t  channels;
    uint32_t  sampleRate;
    int16_t   fifo[AAC_FIFO_FRAMES * AAC_MAX_CHANNELS];
    uint32_t  fifoFrames;
    uint32_t  fifoCapacityFrames; /* current caller's output budget, capped by storage */
    int       fifoOverflow;       /* fail/recover; never report a truncated AU as success */
    int       formatBad;           /* a callback reported something other than S16/48k/2ch */
};

/* dio_manager loads Cinemo for iAP, transport and VFS only: its core dump carries no
 * libNmeAudioAAC at all, and the SDK's class factory answers CinemoErrorInterface (23) while it
 * knows no class implementing INmeAudioCodec.  The factory learns its classes from a plugin
 * directory, which nothing in dio_manager ever sets.  Setting it ourselves makes the AAC plugin
 * loadable -- verified end to end with tools/cinemo-probe, which goes from 23 to a clean 0.
 *
 * Done lazily and only after a create has actually failed, so a firmware that already
 * configures its own directory is left completely alone.
 */
static void aac_register_plugin_directory(void)
{
    typedef int32_t (*cinemo_cfg_t)(void **);
    typedef int32_t (*cinemo_setdir_t)(void *, const char *);
    static int attempted = 0;
    cinemo_cfg_t     mkcfg;
    cinemo_setdir_t  setdir;
    const char      *dir;
    void            *cfg = 0;
    int32_t          rc;

    if (attempted) return;
    attempted = 1;

    mkcfg  = (cinemo_cfg_t)dlsym(RTLD_DEFAULT, "CinemoCreateConfig");
    setdir = (cinemo_setdir_t)dlsym(RTLD_DEFAULT, "ICinemoConfig_SetPluginDirectory");
    if (!mkcfg || !setdir) return;
    dir = getenv("AUDIO_CINEMO_PLUGIN_DIR");
    if (!dir || !*dir) dir = "/mnt/app/armle/usr/lib/cinemo";

    rc = mkcfg(&cfg);
    if (rc != 0 || !cfg) return;
    /* The Config is deliberately never released: it owns the class registry the codec needs
     * for as long as this process decodes anything. */
    (void)setdir(cfg, dir);    /* this file has no logger; the caller reports the outcome */
}

static int aac_resolve(void)
{
    if (g_cinemo.resolved) return g_cinemo.create != 0;
    g_cinemo.resolved  = 1;
    g_cinemo.create    = (cinemo_create_t)dlsym(RTLD_DEFAULT, "CinemoCreateAudioCodec");
    g_cinemo.initialize= (cinemo_init_t)  dlsym(RTLD_DEFAULT, "ICinemoAudioCodec_Initialize");
    g_cinemo.write     = (cinemo_write_t) dlsym(RTLD_DEFAULT, "ICinemoAudioCodec_Write");
    g_cinemo.drain     = (cinemo_simple_t)dlsym(RTLD_DEFAULT, "ICinemoAudioCodec_Drain");
    g_cinemo.flush     = (cinemo_simple_t)dlsym(RTLD_DEFAULT, "ICinemoAudioCodec_Flush");
    g_cinemo.release   = (cinemo_simple_t)dlsym(RTLD_DEFAULT, "ICinemoAudioCodec_Release");
    if (!g_cinemo.create || !g_cinemo.initialize || !g_cinemo.write || !g_cinemo.release) {
        g_cinemo.create = 0;
        return 0;
    }
    return 1;
}

static void cmt_set32(uint8_t *m, size_t off, uint32_t v) { memcpy(m + off, &v, 4); }
static void cmt_setp(uint8_t *m, size_t off, const void *p) { memcpy(m + off, &p, sizeof(p)); }
static uint32_t cmt_get32(const uint8_t *m, size_t off)
{
    uint32_t v; memcpy(&v, m + off, 4); return v;
}

/* The magic cookie for index 23 is an MPEG-4 ES_Descriptor, not a bare ASC: tag 0x03 holds a
 * DecoderConfigDescriptor (0x04) which holds the DecSpecificInfo (0x05) that Cinemo wants.
 * Lengths use the long form 0x80 0x80 0x80, so the ASC does not sit at a fixed offset -- walk
 * to the 0x05 tag instead of indexing into the blob. */
static int aac_extract_asc(const uint8_t *cookie, size_t length,
                           uint8_t *out, uint32_t outCapacity, uint32_t *outLength)
{
    size_t i = 0;
    while (i < length) {
        uint8_t tag = cookie[i++];
        uint32_t size = 0;
        int lengthBytes = 0;
        while (i < length && lengthBytes < 4) {
            uint8_t b = cookie[i++];
            size = (size << 7) | (uint32_t)(b & 0x7f);
            lengthBytes++;
            if (!(b & 0x80)) break;
        }
        if (size > length - i) return 0;
        if (tag == 0x05) {                    /* DecSpecificInfo == AudioSpecificConfig */
            if (!size || size > outCapacity) return 0;
            memcpy(out, cookie + i, size);
            *outLength = size;
            return 1;
        }
        if (tag == 0x03) {                    /* ES_Descriptor: skip its fixed prologue */
            if (size < 3 || length - i < 3) return 0;
            i += 3;                           /* ES_ID(2) + flags/priority(1) */
            continue;                         /* descend rather than skip the payload */
        }
        if (tag == 0x04) {                    /* DecoderConfigDescriptor: skip its 13 fixed bytes */
            if (size < 13 || length - i < 13) return 0;
            i += 13;
            continue;
        }
        i += size;                            /* anything else: step over it */
    }
    return 0;
}

/* Runs inline inside Write; no Cinemo worker thread exists for this plugin pair, so there is
 * nothing to wait on and no lock to take here. */
static int32_t aac_decoded_callback(void *context, const void *pcm, uint32_t pcmBytes,
                                    const void *format, int64_t pts, uint32_t flags)
{
    struct spa_aac_decoder *d = (struct spa_aac_decoder *)context;
    const uint8_t *fmt = (const uint8_t *)format;
    uint32_t frames;

    (void)pts; (void)flags;
    if (!d || !pcm || !pcmBytes) return 0;

    if (fmt) {
        if (cmt_get32(fmt, CMT_MEDIA_TYPE) != CMT_MEDIA_PCM ||
            cmt_get32(fmt, CMT_SUBTYPE) != CMT_SUB_PCM ||
            cmt_get32(fmt, CMT_SAMPLE_FORMAT) != CMT_FMT_S16 ||
            cmt_get32(fmt, CMT_SAMPLE_RATE) != d->sampleRate ||
            cmt_get32(fmt, CMT_CHANNELS) != d->channels) {
            d->formatBad = 1;
            return 0;
        }
    }
    if (!spa_aac_fifo_callback_frames(pcmBytes, d->channels, &frames) ||
        !spa_aac_fifo_can_append(d->fifoFrames, frames,
                                 d->fifoCapacityFrames)) {
        d->fifoOverflow = 1;
        return 0;
    }
    if (!frames) return 0;
    memcpy(d->fifo + (size_t)d->fifoFrames * d->channels, pcm,
           (size_t)frames * d->channels * sizeof(int16_t));
    d->fifoFrames += frames;
    return 0;
}

static int spa_aac_create(const uint8_t *cookie, size_t cookieLength,
                    struct spa_aac_decoder **out,
                          uint32_t *channels, uint32_t *frameLength, uint32_t *sampleRate)
{
    static const uint32_t kRates[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                         22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0 };
    uint8_t input[CMT_SIZE], output[CMT_SIZE];
    struct spa_aac_decoder *d;
    uint32_t aot, freqIndex, chanConfig;
    int32_t rc;

    if (!cookie || !cookieLength || !out) return SPA_APAC_ERR_ARGS;
    *out = 0;
    if (!aac_resolve()) return SPA_APAC_ERR_INIT;

    d = (struct spa_aac_decoder *)calloc(1, sizeof(*d));
    if (!d) return SPA_APAC_ERR_NO_MEMORY;

    /* The codec takes ownership of the header pointer and frees it when it is released, so
     * this must be its own allocation.  Pointing it at a field inside the handle aborted
     * dio_manager with "free malloc object that is not allocated" -- verified both ways with
     * tools/cinemo-probe. */
    d->asc = (uint8_t *)malloc(8);
    if (!d->asc) { free(d); return SPA_APAC_ERR_NO_MEMORY; }
    if (!aac_extract_asc(cookie, cookieLength, d->asc, 8, &d->ascLength)) {
        free(d->asc);
        free(d);
        return SPA_APAC_ERR_COOKIE;
    }
    if (d->ascLength < 2) { free(d->asc); free(d); return SPA_APAC_ERR_COOKIE; }

    /* AudioSpecificConfig: 5 bits object type, 4 bits sampling index, 4 bits channel config. */
    aot        = (uint32_t)(d->asc[0] >> 3);
    freqIndex  = (uint32_t)(((d->asc[0] & 0x07) << 1) | (d->asc[1] >> 7));
    chanConfig = (uint32_t)((d->asc[1] >> 3) & 0x0f);
    if (aot != 2) { free(d->asc); free(d); return SPA_APAC_ERR_CONFIG; }      /* AAC-LC only */
    if (!chanConfig || chanConfig > AAC_MAX_CHANNELS) { free(d->asc); free(d); return SPA_APAC_ERR_CHANNELS; }
    if (!kRates[freqIndex]) { free(d->asc); free(d); return SPA_APAC_ERR_CONFIG; }
    d->channels   = chanConfig;
    d->sampleRate = kRates[freqIndex];

    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
    cmt_set32(input, CMT_MEDIA_TYPE, CMT_MEDIA_AUDIO_ES);
    cmt_set32(input, CMT_SUBTYPE,    CMT_SUB_AAC_LC);
    cmt_set32(input, CMT_HEADER_TYPE, CMT_HDR_ASC);
    cmt_setp (input, CMT_HEADER_DATA, d->asc);       /* ownership passes to the codec below */
    cmt_set32(input, CMT_HEADER_SIZE, d->ascLength);
    /* Rate and channels stay zero on the input: the ASC carries them, as in the donor. */
    cmt_set32(output, CMT_MEDIA_TYPE,    CMT_MEDIA_PCM);
    cmt_set32(output, CMT_SUBTYPE,       CMT_SUB_PCM);
    cmt_set32(output, CMT_SAMPLE_FORMAT, CMT_FMT_S16);
    cmt_set32(output, CMT_SAMPLE_RATE,   d->sampleRate);
    cmt_set32(output, CMT_CHANNELS,      d->channels);

    rc = g_cinemo.create(&d->codec);
    if (rc != 0 || !d->codec) { free(d->asc); free(d); return SPA_APAC_ERR_INIT; }
    rc = g_cinemo.initialize(d->codec, d, (void *)aac_decoded_callback, input, output);
    if (rc != 0) {
        /* One retry: the usual first failure here is an empty class registry, which the
         * plugin directory fixes.  A second failure is a real one. */
        g_cinemo.release(d->codec);
        d->codec = 0;
        aac_register_plugin_directory();
        rc = g_cinemo.create(&d->codec);
        if (rc == 0 && d->codec)
            rc = g_cinemo.initialize(d->codec, d, (void *)aac_decoded_callback, input, output);
    }
    if (rc != 0) {
        /* Initialize failing means the codec never took the header, so it is still ours.  A
         * successful Initialize hands it over for good: from here on the codec frees it. */
        if (d->codec) g_cinemo.release(d->codec);
        free(d->asc);
        free(d);
        return SPA_APAC_ERR_INIT;
    }
    d->asc = 0;                    /* ownership transferred; nulled so nothing here frees it */

    if (channels)    *channels = d->channels;
    if (frameLength) *frameLength = SPA_APAC_FRAME_LENGTH;
    if (sampleRate)  *sampleRate = d->sampleRate;
    *out = d;
    return SPA_APAC_OK;
}

static int spa_aac_decode_s16(struct spa_aac_decoder *d, const uint8_t *packet, size_t length,
                        int16_t *out, size_t outCapacity, uint32_t *framesOut)
{
    uint32_t frames, samples;
    size_t capacityFrames;
    int32_t writeResult;

    if (!d || !out || !framesOut) return SPA_APAC_ERR_ARGS;
    *framesOut = 0;
    d->fifoFrames = 0;
    d->fifoOverflow = 0;
    capacityFrames = outCapacity / d->channels;
    d->fifoCapacityFrames = capacityFrames > AAC_FIFO_FRAMES ?
                            AAC_FIFO_FRAMES : (uint32_t)capacityFrames;

    if (!length) {                                   /* the flush path, as APAC treats it */
        if (g_cinemo.flush) g_cinemo.flush(d->codec);
        return SPA_APAC_OK;
    }
    if (!packet) return SPA_APAC_ERR_ARGS;

    /* The donor's argument shape, kept literally: (AU, bytes, 1, pts=0, 1).  Timing comes from
     * the receiver's own ring clock, so no PTS is invented here. */
    writeResult = g_cinemo.write(d->codec, packet, (uint32_t)length, 1, 0, 1);
    if (d->fifoOverflow) {
        /* Write consumed the compressed AU but its PCM cannot be represented
         * intact.  Flush Cinemo so the next packet starts from an explicit
         * recovery boundary instead of continuing after hidden sample loss. */
        if (g_cinemo.flush) (void)g_cinemo.flush(d->codec);
        d->fifoFrames = 0;
        return SPA_APAC_ERR_OUTPUT_SPACE;
    }
    if (writeResult != 0) return SPA_APAC_ERR_DECODE;
    if (d->formatBad) return SPA_APAC_ERR_CONFIG;

    frames = d->fifoFrames;
    samples = frames * d->channels;
    if (samples > outCapacity) return SPA_APAC_ERR_OUTPUT_SPACE;
    if (samples) memcpy(out, d->fifo, (size_t)samples * sizeof(int16_t));
    *framesOut = frames;
    return SPA_APAC_OK;
}

static void spa_aac_reset(struct spa_aac_decoder *d)
{
    if (!d || !d->codec) return;
    if (g_cinemo.flush) g_cinemo.flush(d->codec);
    d->fifoFrames = 0;
    d->fifoCapacityFrames = AAC_FIFO_FRAMES;
    d->fifoOverflow = 0;
    d->formatBad = 0;
}

static void spa_aac_destroy(struct spa_aac_decoder *d)
{
    if (!d) return;
    if (d->codec) {
        if (g_cinemo.drain) g_cinemo.drain(d->codec);
        g_cinemo.release(d->codec);
    }
    free(d);
}

static int spa_aac_get_info(const struct spa_aac_decoder *d, spa_apac_stream_info_t *info)
{
    if (!d || !info) return SPA_APAC_ERR_ARGS;
    memset(info, 0, sizeof(*info));
    info->channels = d->channels;
    info->frameLength = SPA_APAC_FRAME_LENGTH;
    info->sampleRate = d->sampleRate;
    info->channelLayoutTag = 0;          /* stereo; no layout tag comes from an ASC */
    return SPA_APAC_OK;
}

/* The one exported symbol. Same table shape as the APAC decoder so the render path
 * can retain whichever API accepts the current inline cookie, then dispatch on
 * decoded geometry/layout. The handle is this file's own type, cast through the
 * table's opaque one. Nothing else here is exported, because the hook is
 * LD_PRELOAD'd and a name shared with the APAC library would interpose globally. */
const spa_apac_codec_api_t *spa_aac_codec_api(void)
{
    static spa_apac_codec_api_t api;
    if (!api.structSize) {
        api.structSize = (uint32_t)sizeof(api);
        api.abiVersion = SPA_APAC_CODEC_ABI_VERSION;
        api.create     = (int (*)(const uint8_t *, size_t, spa_apac_decoder_t **,
                                  uint32_t *, uint32_t *, uint32_t *))spa_aac_create;
        api.decodeS16  = (int (*)(spa_apac_decoder_t *, const uint8_t *, size_t,
                                  int16_t *, size_t, uint32_t *))spa_aac_decode_s16;
        api.reset      = (void (*)(spa_apac_decoder_t *))spa_aac_reset;
        api.destroy    = (void (*)(spa_apac_decoder_t *))spa_aac_destroy;
        api.getInfo    = (int (*)(const spa_apac_decoder_t *, spa_apac_stream_info_t *))spa_aac_get_info;
    }
    return &api;
}
