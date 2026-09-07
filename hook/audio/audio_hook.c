/*
 * audio_hook.c — CarPlay buffered APAC/AAC receive and fixed-width PCM rendering
 * for the MHI2Q six-channel DIO media rail. See audio_hook.h and
 * docs/AUDIO_5_1_STATE.md for the accepted vehicle path.
 *
 * The canonical AUDIO_SPATIAL policy advertises buffered APAC/5.1 index 65 plus
 * AAC-LC/stereo index 23. A changed inline APAP cookie selects the decoder;
 * decoded geometry/layout then selects either the measured native-5.1 reorder
 * or the optional stereo ambience/LFE renderer. The QSA, SAD, CSD and MOST
 * contracts remain fixed at PCM6/48 kHz/S16 for the session.
 *
 * The earlier realtime-format interposers and SPATIAL_* controls are retained as
 * migration/debug paths. The checked-in deployment uses AUDIO_SPATIAL=1; the
 * already vehicle-confirmed deva/mixer provisioning must be present before it is
 * armed.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "audio_hook.h"
#include "apac_codec_loader.h"
#include "apap_crypto.h"
#include "apap_lifecycle.h"
#include "feedback_clock.h"
#include "pcm_period.h"
#include "soundcheck.h"
#include "spatial_downmix.h"
#include "stereo_upmix.h"
#include "media_route.h"
#include "../framework/logging.h"
#include "../framework/state_trace.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <time.h>
#include <sys/asoundlib.h>

DEFINE_LOG_MODULE(audio);

/* A LOG=0 build is the pre-production artifact: it must remove not only the
 * logger thread and LOG_* formatting, but also the historical probe files.
 * These captures are diagnostic-only; none participates in negotiation,
 * decoding, timing, or rendering.  Keep one switch so a supposedly quiet
 * build cannot continue writing APAP/plist/cookie/tap data to /tmp. */
#ifndef ENABLE_AUDIO_DIAGNOSTICS
#define ENABLE_AUDIO_DIAGNOSTICS ENABLE_LOGGING
#endif

/* ---- the format we emit ---------------------------------------------------------------------
 * Rate/depth are fixed by the hardware: every media device in deva is 48000-only, 16-bit on the wire
 * (measured on-unit, AUDIO.md §21a/§24). Only the channel count is variable, and 6 is the ceiling —
 * deva's `voices > 2` branch is hardcoded to 6 channels (§16).
 * Channel order = Apple canonical 5.1 (L R C LFE Ls Rs). CONFIRMED correct: deva's multichannel stream
 * map is 01 02 03 06 08 09 = FL FR FC LFE LB RB = exactly that (§16) — no reorder needed. */
#define SPA_SR        48000
#define SPA_BITS      16
#define SPA_CHANNELS  6

/* Vehicle runtime exposes one stereo-upmix switch.  ON always means the sole
 * production HSSR Universal renderer; OFF is the exact safety bypass. */

/* ---- gating -------------------------------------------------------------------------------- */

/* value-checked (mirrors the altscreen env_on / getenv-presence-check trap): "0"/"false"/"no"/"off"
 * /empty all mean OFF, so `export SPATIAL_5_1=0` really disables it. */
static int spa_env_on(const char *name)
{
    const char *v = getenv(name);
    if (!v || !v[0]) return 0;
    if (v[0] == '0' && !v[1]) return 0;
    if (!strcmp(v, "false") || !strcmp(v, "no") || !strcmp(v, "off")) return 0;
    return 1;
}

static int spa_cold_connect_enabled(void)
{
    const char *value = getenv(SPA_AUDIO_COLD_CONNECT_ENV);
    return !value || spa_env_on(SPA_AUDIO_COLD_CONNECT_ENV);
}

enum {
    SPA_ATMOS_DOWNMIX = 1,
    SPA_ATMOS_DROP = 2,
    SPA_ATMOS_PASSTHROUGH = 3
};

typedef struct {
    int usesNewControls;
    int realtimePCM;
    int buffered;
    int atmos;
    int atmosMode;
    enum spa_stereo_upmix_backend stereoUpmixBackend;
    uint32_t stereoSurroundQ15;
    int stereoLfeAssist;
    spa_soundcheck_config_t soundCheck;
} spa_audio_config_t;

static spa_audio_config_t spa_audio_config_value;
static pthread_once_t spa_audio_config_once = PTHREAD_ONCE_INIT;

static int spa_parse_bool(const char *name, int *present)
{
    const char *value = getenv(name);

    *present = value != 0;
    if (!value || !value[0] || !strcasecmp(value, "0") ||
        !strcasecmp(value, "false") || !strcasecmp(value, "no") ||
        !strcasecmp(value, "off"))
        return 0;
    if (!strcasecmp(value, "1") || !strcasecmp(value, "true") ||
        !strcasecmp(value, "yes") || !strcasecmp(value, "on"))
        return 1;
    LOG_ERROR(LOG_MODULE,
              "audio config: invalid %s='%s'; expected 0/1/true/false; treating as false",
              name, value);
    return 0;
}

static enum spa_stereo_upmix_backend spa_parse_stereo_upmix(void)
{
    const char *value = getenv(SPA_AUDIO_STEREO_UPMIX_ENV);

    if (!value || !value[0] || !strcasecmp(value, "off") ||
        !strcasecmp(value, "0") || !strcasecmp(value, "false") ||
        !strcasecmp(value, "no"))
        return SPA_STEREO_UPMIX_OFF;
    if (!strcasecmp(value, "on") || !strcasecmp(value, "1") ||
        !strcasecmp(value, "true") || !strcasecmp(value, "yes"))
        return SPA_STEREO_UPMIX_HSSR_UNIVERSAL;

    LOG_ERROR(LOG_MODULE,
              "audio config: invalid %s='%s'; expected on/off; using off",
              SPA_AUDIO_STEREO_UPMIX_ENV, value);
    return SPA_STEREO_UPMIX_OFF;
}

static uint32_t spa_parse_stereo_surround_q15(void)
{
    const char *value = getenv(SPA_AUDIO_STEREO_SURROUND_ENV);
    char *end = 0;
    double gain;

    if (!value || !value[0]) return SPA_STEREO_UPMIX_REAR_Q15_DEFAULT;
    errno = 0;
    gain = strtod(value, &end);
    if (errno || end == value || *end || gain != gain ||
        gain < 0.0 || gain > 0.25) {
        LOG_ERROR(LOG_MODULE,
                  "audio config: invalid %s='%s'; expected 0.0..0.25; using 0.18",
                  SPA_AUDIO_STEREO_SURROUND_ENV, value);
        return SPA_STEREO_UPMIX_REAR_Q15_DEFAULT;
    }
    return (uint32_t)(gain * 32768.0 + 0.5);
}

static int spa_parse_stereo_lfe(void)
{
    const char *value = getenv(SPA_AUDIO_STEREO_LFE_ENV);

    if (!value || !value[0] || !strcasecmp(value, "off")) return 0;
    if (!strcasecmp(value, "assist")) return 1;

    LOG_ERROR(LOG_MODULE,
              "audio config: invalid %s='%s'; expected off/assist; using off",
              SPA_AUDIO_STEREO_LFE_ENV, value);
    return 0;
}

static int spa_parse_soundcheck_fallback(void)
{
    const char *value = getenv(SPA_AUDIO_SOUNDCHECK_FALLBACK_ENV);
    if (!value || !value[0] || !strcasecmp(value, "off"))
        return SPA_SOUNDCHECK_FALLBACK_OFF;
    if (!strcasecmp(value, "rms")) return SPA_SOUNDCHECK_FALLBACK_RMS;
    LOG_ERROR(LOG_MODULE,
              "audio config: invalid %s='%s'; expected off/rms; using off",
              SPA_AUDIO_SOUNDCHECK_FALLBACK_ENV, value);
    return SPA_SOUNDCHECK_FALLBACK_OFF;
}

static int spa_parse_soundcheck_limiter(void)
{
    const char *value = getenv(SPA_AUDIO_SOUNDCHECK_LIMITER_ENV);
    if (!value || !value[0] || !strcasecmp(value, "off"))
        return SPA_SOUNDCHECK_LIMITER_OFF;
    if (!strcasecmp(value, "peak")) return SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK;
    LOG_ERROR(LOG_MODULE,
              "audio config: invalid %s='%s'; expected off/peak; using off",
              SPA_AUDIO_SOUNDCHECK_LIMITER_ENV, value);
    return SPA_SOUNDCHECK_LIMITER_OFF;
}

static void spa_audio_config_init_once(void)
{
    const char *modeValue;
    int spatialPresent, typoPresent, atmosPresent;
    int spatial = spa_parse_bool(SPA_AUDIO_ENV, &spatialPresent);
    int typo = spa_parse_bool(SPA_AUDIO_TYPO_ENV, &typoPresent);
    int atmos = spa_parse_bool(SPA_AUDIO_ATMOS_ENV, &atmosPresent);

    memset(&spa_audio_config_value, 0, sizeof(spa_audio_config_value));
    spa_audio_config_value.usesNewControls =
        spatialPresent || typoPresent || atmosPresent;
    spa_audio_config_value.atmosMode = SPA_ATMOS_DOWNMIX;
    spa_audio_config_value.stereoUpmixBackend = spa_parse_stereo_upmix();
    spa_audio_config_value.stereoSurroundQ15 = spa_parse_stereo_surround_q15();
    spa_audio_config_value.stereoLfeAssist = spa_parse_stereo_lfe();
    spa_soundcheck_default_config(&spa_audio_config_value.soundCheck);
    {
        int present = 0;
        int enabled = spa_parse_bool(SPA_AUDIO_SOUNDCHECK_ENV, &present);
        if (present) spa_audio_config_value.soundCheck.enabled = enabled;
    }
    spa_audio_config_value.soundCheck.fallbackMode =
        spa_parse_soundcheck_fallback();
    spa_audio_config_value.soundCheck.limiterMode =
        spa_parse_soundcheck_limiter();

    if (spa_audio_config_value.usesNewControls) {
        if (!spatialPresent && typoPresent) {
            spatial = typo;
            LOG_WARN(LOG_MODULE,
                     "audio config: %s is a deprecated typo alias; use %s",
                     SPA_AUDIO_TYPO_ENV, SPA_AUDIO_ENV);
        } else if (spatialPresent && typoPresent && spatial != typo) {
            LOG_WARN(LOG_MODULE,
                     "audio config: %s conflicts with typo alias %s; canonical value wins",
                     SPA_AUDIO_ENV, SPA_AUDIO_TYPO_ENV);
        }
        spa_audio_config_value.atmos = atmos;
        spa_audio_config_value.buffered = spatial || atmos;
        spa_audio_config_value.realtimePCM = spatial || atmos;
    } else {
        /* Migration only: old deployments retain their exact behavior until
         * carplay_startup.sh is replaced with the canonical variables. */
        spa_audio_config_value.realtimePCM = spa_env_on("SPATIAL_5_1");
        spa_audio_config_value.buffered = spa_env_on(SPA_PROBE103_ENV);
        spa_audio_config_value.atmos =
            spa_audio_config_value.buffered &&
            getenv(SPA_103_INDEX_ENV) &&
            !strcmp(getenv(SPA_103_INDEX_ENV), "66");
    }

    modeValue = getenv(SPA_AUDIO_ATMOS_MODE_ENV);
    if (modeValue && modeValue[0]) {
        if (!strcasecmp(modeValue, "downmix"))
            spa_audio_config_value.atmosMode = SPA_ATMOS_DOWNMIX;
        else if (!strcasecmp(modeValue, "drop"))
            spa_audio_config_value.atmosMode = SPA_ATMOS_DROP;
        else if (!strcasecmp(modeValue, "passthrough"))
            spa_audio_config_value.atmosMode = SPA_ATMOS_PASSTHROUGH;
        else
            LOG_ERROR(LOG_MODULE,
                      "audio config: invalid %s='%s'; expected downmix/drop/passthrough; "
                      "using downmix",
                      SPA_AUDIO_ATMOS_MODE_ENV, modeValue);
    }
}

static const spa_audio_config_t *spa_audio_config(void)
{
    pthread_once(&spa_audio_config_once, spa_audio_config_init_once);
    return &spa_audio_config_value;
}

static int spa_enabled(void)
{
    return spa_audio_config()->realtimePCM;
}

/* Runtime tuning of WHICH format bit we advertise, re-read on every call (these run once per stream
 * SETUP, not per frame). The env var is only read at supervisor start, so without this every attempt
 * at a different bit would cost a reboot; with it the loop is
 *     echo 38 > /tmp/spatial_bit ; replug the phone ; read the log.
 * File absent/garbage = defaults. Bit clamped to 32..63: the stock stereo/mono bits live at 2..25
 * (AUDIO.md section 19), so a typo can never disturb the working stereo advert. */
static void spa_tune(unsigned *bit, unsigned *channels)
{
    unsigned b = 0, c = 0;
    FILE *f;

    *bit = SPA_DEFAULT_BIT;
    *channels = SPA_CHANNELS;

    f = fopen(SPA_TUNE_PATH, "r");
    if (!f) return;
    if (fscanf(f, "%u:%u", &b, &c) >= 1) {
        if (b >= 32 && b <= 63) *bit = b;
        /* channels == 0 is PROBE-ONLY: advertise the bit, but refuse it if the phone actually picks
         * it (see ToASBD). Needed because our ASBD always describes 16-bit lpcm — if the phone selects
         * a bit that means something else on the wire (bit 38 = 8 channels, 47 = 32-bit float,
         * 40/42 = multichannel AAC), emitting that ASBD would feed the amp full-scale noise instead
         * of music. */
        if (c <= 8) *channels = c;
    }
    fclose(f);
}

/*
 * The AirPlayAudioFormat bit for 5.1 PCM (PCM/48000/16/5.1), extracted statically from iOS 27
 * AirPlaySupport `_APAudioFormatCopyAudioFormatsToReadableString` (bit->string table) = bit 44.
 * Other useful bits from the same table: PCM/48000/32f/5.1 = 1<<47; PCM/48000/16/5.1.2 = 1<<38;
 * DDPLUS/48000/5.1 = 1<<50; DDPLUS/48000/9.1.6 = 1<<53. SPA_FMT_5_1 is defined in audio_hook.h.
 * Channel order = Apple canonical 5.1 (L R C LFE Ls Rs). CONFIRMED correct: deva's multichannel
 * stream map is 01 02 03 06 08 09 = FL FR FC LFE LB RB = exactly that (AUDIO.md §16) — no reorder.
 * Note also (§16): deva's `voices > 2` branch is hardcoded to 6 channels, so 6 is the hard ceiling. */
/* ---- format probe -------------------------------------------------------------------------
 * Decode a negotiated AirPlayAudioFormat (uint64) to a human string so ONE connect reveals whether
 * iOS chose multichannel PCM (our path) or a Dolby/EC3 bitstream (Atmos → needs the decode path).
 * Bit map from iOS 27 AirPlaySupport (AUDIO.md §9): 38=PCM 5.1.2, 44=PCM 5.1, 47=PCM 32f 5.1,
 * 50=DDP/EC3 5.1, 53=DDP/EC3 9.1.6. Stock low bits (<=31) = stereo/mono PCM + AAC. */
static const char *spa_fmt_desc(uint64_t fmt)
{
    if (fmt == 0) return "0 (invalid/none)";
    if (__builtin_popcountll(fmt) == 1) {
        switch (__builtin_ctzll(fmt)) {
            case 38: return "bit38 PCM 48k/16/5.1.2 (multichannel PCM)";
            case 44: return "bit44 PCM 48k/16/5.1 (multichannel PCM = our target)";
            case 47: return "bit47 PCM 48k/32f/5.1 (multichannel PCM)";
            case 50: return "bit50 DDP/EC3 48k/5.1 (Dolby Atmos BITSTREAM -> needs decode)";
            case 53: return "bit53 DDP/EC3 48k/9.1.6 (Dolby Atmos BITSTREAM -> needs decode)";
            default: break;
        }
        /* single bit, not in the table */
        return __builtin_ctzll(fmt) <= 31 ? "low-bit (stock stereo/mono PCM or AAC)"
                                          : "high-bit (multichannel? not in known table)";
    }
    return "multi-bit mask (see hex)";
}

/* ---- AudioSessionGetSupportedOutputFormats -------------------------------------------------
 * The 64-bit capability mask the AirPlay negotiation offers the phone. OR-in the 5.1 bit so the
 * phone is allowed to send 6ch. Stereo path untouched. */
typedef int64_t (*fn_get_formats_t)(int /*streamType*/);

int64_t AudioSessionGetSupportedOutputFormats(int streamType)
{
    static fn_get_formats_t real;
    if (!real) real = (fn_get_formats_t)dlsym(RTLD_NEXT, "AudioSessionGetSupportedOutputFormats");
    int64_t base = real ? real(streamType) : 0;
    int64_t caps = base;
    unsigned bit, channels;

    spa_tune(&bit, &channels);
    if (spa_enabled() && streamType == 100 /* main audio only */)
        caps |= (int64_t)((uint64_t)1 << bit);

    /* PROBE (always): what we advertise to the phone, per stream type. Stock main is 0x8010 = bits
     * {4,15} = PCM/48000/16/2 (+mic), measured on-unit (AUDIO.md section 24) — so `stock` here is the
     * regression check, and `offered` is what the phone gets to choose from. */
    if (caps != base)
        LOG_INFO(LOG_MODULE, "getSupportedFormats(type=%d): stock=%#llx offered=%#llx (+bit%u, %uch)",
                 streamType, (unsigned long long)base, (unsigned long long)caps, bit, channels);
    else
        LOG_INFO(LOG_MODULE, "getSupportedFormats(type=%d): stock=%#llx offered=%#llx",
                 streamType, (unsigned long long)base, (unsigned long long)caps);
    return caps;
}

/* ---- AirPlayAudioFormatToASBD --------------------------------------------------------------
 * Stock (0x2bda0) maps a 64-bit AirPlayAudioFormat to a 36-byte / 9-dword ASBD and returns -6735 for
 * anything outside its exact-match stereo/mono table (incl. every high bit). We intercept our 5.1 bit
 * and fill the ASBD ourselves; everything else falls through to stock unchanged.
 *
 * ASBD dword layout (CONFIRMED from the stock decompile — NON-standard, sampleRate is UInt32):
 *   [0]=sampleRate [1]='lpcm'(1819304813) [2]=formatFlags(0xC=signed+packed) [3]=bytesPerPacket
 *   [4]=framesPerPacket(1) [5]=bytesPerFrame [6]=channelsPerFrame [7]=bitsPerChannel [8]=reserved(0)
 * The optional 3rd arg (`bits_out`) receives bitsPerChannel when non-NULL (stock's LABEL_24). */
typedef uint32_t (*fn_fmt_to_asbd_t)(uint64_t /*fmt*/, uint32_t * /*asbd*/, uint32_t * /*bits_out*/);

uint32_t AirPlayAudioFormatToASBD(uint64_t fmt, uint32_t *asbd, uint32_t *bits_out)
{
    /* PROBE (always): the phone's NEGOTIATED format arrives here during SETUP/_UpdateStreams. This is
     * the decisive line — it says exactly what iOS chose. A bit50/53 here = Dolby Atmos bitstream (we
     * must decode, not PCM). A bit44/etc = multichannel PCM (our path works). NOTE: if the phone picks
     * an EC3/passthrough format that libairplay routes WITHOUT calling ToASBD, you'll see it in the
     * offered caps above but NOT here — absence of a main-audio ToASBD call is itself the EC3 signal. */
    LOG_INFO(LOG_MODULE, "ToASBD(fmt=%#llx) => %s", (unsigned long long)fmt, spa_fmt_desc(fmt));

    {
        unsigned bit, channels;
        spa_tune(&bit, &channels);
        if (spa_enabled() && fmt == ((uint64_t)1 << bit) && channels == 0) {
            /* PROBE-ONLY hit: this is the answer we were after — the phone WILL select this bit. Say so
             * loudly, then fall through to stock (which rejects it with -6735) rather than describing
             * foreign wire data as 16-bit lpcm and blasting the amp. */
            LOG_ERROR(LOG_MODULE, "*** PHONE SELECTED bit%u (fmt=%#llx) — probe-only, rejecting to "
                      "stock. Multichannel IS reachable on this path. ***",
                      bit, (unsigned long long)fmt);
        } else if (spa_enabled() && fmt == ((uint64_t)1 << bit) && asbd) {
            uint32_t bpf = channels * (SPA_BITS / 8);
            asbd[0] = SPA_SR;             /* sampleRate      */
            asbd[1] = 1819304813;         /* 'lpcm'          */
            asbd[2] = 0xC;                /* signed + packed */
            asbd[3] = bpf;                /* bytesPerPacket  */
            asbd[4] = 1;                  /* framesPerPacket */
            asbd[5] = bpf;                /* bytesPerFrame   */
            asbd[6] = channels;           /* channelsPerFrame*/
            asbd[7] = SPA_BITS;           /* bitsPerChannel  */
            asbd[8] = 0;                  /* reserved        */
            if (bits_out) *bits_out = SPA_BITS;
            LOG_INFO(LOG_MODULE, "ASBD MULTICH: %u Hz, %u ch, %u-bit, bpf=%u (bit%u, fmt=%#llx)",
                     SPA_SR, channels, SPA_BITS, bpf, bit, (unsigned long long)fmt);
            return 0;
        }
    }

    static fn_fmt_to_asbd_t real;
    if (!real) real = (fn_fmt_to_asbd_t)dlsym(RTLD_NEXT, "AirPlayAudioFormatToASBD");
    if (!real) { LOG_ERROR(LOG_MODULE, "AirPlayAudioFormatToASBD real==NULL"); return (uint32_t)-6735; }
    return real(fmt, asbd, bits_out);
}

/* ================= type-103 buffered-media negotiation probe (see audio_hook.h) ==================
 * CFLite is linked INTO libairplay.so and every symbol below is GLOBAL there, so plain dlsym works.
 * ARITIES VERIFIED AGAINST THE DISASSEMBLY — this stack's CFLite is not stock CoreFoundation:
 *   CFStringGetCString(str, buf, size)          3 args — NO encoding parameter (stock CF has 4)
 *   CFArrayAppendCString(arr, cstr, len)        3 args, len == -1 means strlen
 *   CFDictionarySetCString(dict, key, cstr,len) 4 args, len == -1 means strlen
 *   CFDictionarySetInt64(dict, key, int64)      3 C args (rides two ARM regs)
 *   CFNumberCreateInt64(int64)                  1 arg  — NOT (alloc, type, ptr)
 * Getting an arity wrong here puts register garbage into CFLite; see the CFLite ABI trap note. */
#define SPA_CFENC_UTF8  134217984u   /* kCFStringEncodingUTF8, the literal libairplay itself passes */

typedef void *(*cf_str_t)(void *, const char *, unsigned);
typedef int   (*cf_equal_t)(const void *, const void *);
typedef void *(*cf_dictm_t)(void *, long, const void *, const void *);
typedef void *(*cf_arrm_t)(void *, long, const void *);
typedef void  (*cf_arrapp_t)(void *, const void *);
typedef int   (*cf_arrappc_t)(void *, const char *, int);
typedef long  (*cf_arrcount_t)(const void *);
typedef void *(*cf_arridx_t)(const void *, long);
typedef void  (*cf_dictset_t)(void *, const void *, const void *);
typedef void  (*cf_dictseti_t)(void *, const void *, int64_t);
typedef int   (*cf_dictsetd_t)(void *, const void *, double);
typedef int   (*cf_dictsetc_t)(void *, const void *, const char *, int);
typedef void *(*cf_dictget_t)(const void *, const void *);
typedef int64_t (*cf_dictgeti_t)(const void *, const void *, int32_t *);
typedef int   (*cf_strgetc_t)(const void *, char *, size_t);
typedef int   (*cf_plwrite_t)(const void *, const char *, FILE *);
typedef const void *(*cf_retain_t)(const void *);
typedef void  (*cf_rel_t)(const void *);

static cf_str_t      cfa_str;
static cf_equal_t    cfa_equal;
static cf_dictm_t    cfa_dictm;
static cf_arrm_t     cfa_arrm;
static cf_arrapp_t   cfa_arrapp;
static cf_arrappc_t  cfa_arrappc;
static cf_arrcount_t cfa_arrcount;
static cf_arridx_t   cfa_arridx;
static cf_dictset_t  cfa_dictset;
static cf_dictseti_t cfa_dictseti;
static cf_dictsetd_t cfa_dictsetd;
static cf_dictsetc_t cfa_dictsetc;
static cf_dictget_t  cfa_dictget;
static cf_dictgeti_t cfa_dictgeti;
static cf_strgetc_t  cfa_strgetc;
static cf_plwrite_t  cfa_plwrite;
static cf_retain_t   cfa_retain;
static cf_rel_t      cfa_rel;
static void         *cfa_arr_cb, *cfa_dk_cb, *cfa_dv_cb;
static void         *cfa_bool_true;
static int           cfa_ready = -1;
static int spa103_private_image_compatible(void);
static int spa_pcm_sink_detached_feedback_rate(double *rateHz, int *measured);
static int spa_pcm_sink_bootstrap_buffered(void);
static void spa103_retry_now_playing_after_media(void);

/* AirPlaySender validates enabledFeatures against /info after control SETUP. In particular,
 * negotiated mainBuffered requires a top-level mainBufferedInfo key and rejects the entire CarPlay
 * session when the key is absent (-16720 in iOS 27's validator). Remember the exact session whose
 * response we changed so an unrelated/stale /info can never inherit the contract. */
static pthread_mutex_t spa_contract_lock = PTHREAD_MUTEX_INITIALIZER;
static void            *spa_mainbuffered_session;
static uint32_t         spa_contract_sequence;

/* updateFeedback belongs to the stock realtime stream, but iOS deliberately
 * tears that stream down when the phone/app leaves the foreground while the
 * buffered media hose is still playing.  Retain the last stock clock as a
 * compatibility fallback.  The detached writer normally publishes the same
 * observed shape, {streams:[{type:100,sr:<rate>}]}, using either the 48-kHz
 * contract during measurement warmup or its own measured QSA cadence.
 *
 * The accepted MHI2Q libairplay.so (sha256 01c116b...) exports
 * CFDictionarySetDouble at 0x56ad0.  Its ARM body consumes the double in r2/r3,
 * passes kCFNumberFloat64Type (13) to CFNumberCreate, then inserts that object
 * with CFDictionarySetValue.  This is the recovered constructor contract used
 * below; no opaque object layout is dereferenced. */
static pthread_mutex_t spa_feedback_lock = PTHREAD_MUTEX_INITIALIZER;
static void            *spa_feedback_session;
static const void      *spa_feedback_response;

static void spa_set_mainbuffered_contract(void *session, int enabled, uint32_t sequence)
{
    pthread_mutex_lock(&spa_contract_lock);
    if ((int32_t)(sequence - spa_contract_sequence) >= 0) {
        spa_contract_sequence = sequence;
        spa_mainbuffered_session = enabled ? session : 0;
    }
    pthread_mutex_unlock(&spa_contract_lock);
}

static int spa_has_mainbuffered_contract(void *session)
{
    int match;
    pthread_mutex_lock(&spa_contract_lock);
    match = session && spa_mainbuffered_session == session;
    pthread_mutex_unlock(&spa_contract_lock);
    return match;
}

static int spa_cf_resolve(void)
{
    if (cfa_ready >= 0) return cfa_ready;
    cfa_str      = (cf_str_t)      dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
    cfa_equal    = (cf_equal_t)    dlsym(RTLD_DEFAULT, "CFEqual");
    cfa_dictm    = (cf_dictm_t)    dlsym(RTLD_DEFAULT, "CFDictionaryCreateMutable");
    cfa_arrm     = (cf_arrm_t)     dlsym(RTLD_DEFAULT, "CFArrayCreateMutable");
    cfa_arrapp   = (cf_arrapp_t)   dlsym(RTLD_DEFAULT, "CFArrayAppendValue");
    cfa_arrappc  = (cf_arrappc_t)  dlsym(RTLD_DEFAULT, "CFArrayAppendCString");
    cfa_arrcount = (cf_arrcount_t) dlsym(RTLD_DEFAULT, "CFArrayGetCount");
    cfa_arridx   = (cf_arridx_t)   dlsym(RTLD_DEFAULT, "CFArrayGetValueAtIndex");
    cfa_dictset  = (cf_dictset_t)  dlsym(RTLD_DEFAULT, "CFDictionarySetValue");
    cfa_dictseti = (cf_dictseti_t) dlsym(RTLD_DEFAULT, "CFDictionarySetInt64");
    cfa_dictsetd = (cf_dictsetd_t) dlsym(RTLD_DEFAULT, "CFDictionarySetDouble");
    cfa_dictsetc = (cf_dictsetc_t) dlsym(RTLD_DEFAULT, "CFDictionarySetCString");
    cfa_dictget  = (cf_dictget_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
    cfa_dictgeti = (cf_dictgeti_t) dlsym(RTLD_DEFAULT, "CFDictionaryGetInt64");
    cfa_strgetc  = (cf_strgetc_t)  dlsym(RTLD_DEFAULT, "CFStringGetCString");
    cfa_plwrite  = (cf_plwrite_t)  dlsym(RTLD_DEFAULT, "CFPropertyListWriteToANSIFile");
    cfa_retain   = (cf_retain_t)   dlsym(RTLD_DEFAULT, "CFRetain");
    cfa_rel      = (cf_rel_t)      dlsym(RTLD_DEFAULT, "CFRelease");
    cfa_arr_cb   = dlsym(RTLD_DEFAULT, "kCFLArrayCallBacksCFLTypes");
    cfa_dk_cb    = dlsym(RTLD_DEFAULT, "kCFLDictionaryKeyCallBacksCFLTypes");
    cfa_dv_cb    = dlsym(RTLD_DEFAULT, "kCFLDictionaryValueCallBacksCFLTypes");

    cfa_ready = (cfa_str && cfa_equal && cfa_dictm && cfa_arrm && cfa_arrapp && cfa_arrcount &&
                 cfa_arridx && cfa_dictset && cfa_dictseti && cfa_dictsetd && cfa_dictsetc && cfa_dictget &&
                 cfa_dictgeti && cfa_retain && cfa_rel && cfa_arr_cb && cfa_dk_cb &&
                 cfa_dv_cb) ? 1 : 0;
    if (!cfa_ready)
        LOG_ERROR(LOG_MODULE, "probe103: CFLite symbol resolution FAILED — probe disabled");
    return cfa_ready;
}

static int spa_feedback_has_realtime_clock(const void *response)
{
    void *streamsKey = 0, *typeKey = 0, *srKey = 0, *streams;
    long i, count;
    int found = 0;

    if (!response || !spa_cf_resolve()) return 0;
    streamsKey = cfa_str(0, "streams", SPA_CFENC_UTF8);
    typeKey = cfa_str(0, "type", SPA_CFENC_UTF8);
    srKey = cfa_str(0, "sr", SPA_CFENC_UTF8);
    if (!streamsKey || !typeKey || !srKey) goto done;
    streams = cfa_dictget(response, streamsKey);
    count = streams ? cfa_arrcount(streams) : 0;
    for (i = 0; i < count; i++) {
        void *desc = cfa_arridx(streams, i);
        int32_t err = 0;
        int64_t type;
        if (!desc) continue;
        type = cfa_dictgeti(desc, typeKey, &err);
        if (!err && type == 100 && cfa_dictget(desc, srKey)) {
            found = 1;
            break;
        }
    }

done:
    if (srKey) cfa_rel(srKey);
    if (typeKey) cfa_rel(typeKey);
    if (streamsKey) cfa_rel(streamsKey);
    return found;
}

/* Build the exact two-field clock descriptor observed in every captured stock
 * response.  Fresh containers avoid mutating a retained stock plist that a
 * concurrent control caller may still own. */
static void *spa_feedback_create_realtime_clock(double rateHz)
{
    void *response = 0, *streams = 0, *stream = 0;
    void *streamsKey = 0, *typeKey = 0, *srKey = 0;

    if (!spa_cf_resolve() || rateHz <= 0.0 || rateHz != rateHz) return 0;
    response = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
    streams = cfa_arrm(0, 0, cfa_arr_cb);
    stream = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
    streamsKey = cfa_str(0, "streams", SPA_CFENC_UTF8);
    typeKey = cfa_str(0, "type", SPA_CFENC_UTF8);
    srKey = cfa_str(0, "sr", SPA_CFENC_UTF8);
    if (!response || !streams || !stream || !streamsKey || !typeKey || !srKey)
        goto fail;

    cfa_dictseti(stream, typeKey, 100);
    if (cfa_dictsetd(stream, srKey, rateHz) != 0) goto fail;
    cfa_arrapp(streams, stream);
    cfa_dictset(response, streamsKey, streams);
    if (!spa_feedback_has_realtime_clock(response)) goto fail;

    cfa_rel(srKey);
    cfa_rel(typeKey);
    cfa_rel(streamsKey);
    cfa_rel(stream);
    cfa_rel(streams);
    return response;

fail:
    if (srKey) cfa_rel(srKey);
    if (typeKey) cfa_rel(typeKey);
    if (streamsKey) cfa_rel(streamsKey);
    if (stream) cfa_rel(stream);
    if (streams) cfa_rel(streams);
    if (response) cfa_rel(response);
    return 0;
}

static void spa_feedback_cache(void *session, const void *response)
{
    const void *retained;
    const void *old;

    if (!session || !response || !spa_feedback_has_realtime_clock(response)) return;
    retained = cfa_retain(response);
    if (!retained) return;
    pthread_mutex_lock(&spa_feedback_lock);
    old = spa_feedback_response;
    spa_feedback_session = session;
    spa_feedback_response = retained;
    pthread_mutex_unlock(&spa_feedback_lock);
    if (old) cfa_rel(old);
}

/* Caller owns the returned reference, exactly as if stock had populated
 * responseOut itself. */
static void *spa_feedback_copy_cached(void *session)
{
    const void *response = 0;

    pthread_mutex_lock(&spa_feedback_lock);
    if (session && spa_feedback_session == session && spa_feedback_response)
        response = cfa_retain(spa_feedback_response);
    pthread_mutex_unlock(&spa_feedback_lock);
    return (void *)response;
}

static void spa_feedback_forget(void *session, int anySession)
{
    const void *old = 0;

    pthread_mutex_lock(&spa_feedback_lock);
    if (spa_feedback_response &&
        (anySession || (session && spa_feedback_session == session))) {
        old = spa_feedback_response;
        spa_feedback_response = 0;
        spa_feedback_session = 0;
    }
    pthread_mutex_unlock(&spa_feedback_lock);
    if (old) cfa_rel(old);
}

/* Type-103 is armed only by the startup environment, before AirTunesServer_EnsureCreated. */
static int spa_probe103(void)
{
    return spa_audio_config()->buffered && spa103_private_image_compatible();
}

static int spa_probe103_dcx(void)
{
    return spa_probe103() && spa_env_on(SPA_103_DCX_ENV);
}

/* Split the two independent pieces of the buffered-audio contract. SPATIAL_103 remains the master
 * kill switch; an absent mode selects the established combined behavior. */
static unsigned spa_probe103_mode(void)
{
    const spa_audio_config_t *config = spa_audio_config();
    const char *value;
    unsigned numeric;

    if (!spa_probe103()) return 0;
    if (config->usesNewControls) return SPA_103_MODE_BOTH;
    value = getenv(SPA_103_MODE_ENV);
    if (!value || !value[0]) return SPA_103_MODE_BOTH;
    if (!strcmp(value, "info") || !strcmp(value, "1")) numeric = SPA_103_MODE_INFO;
    else if (!strcmp(value, "feature") || !strcmp(value, "features") || !strcmp(value, "2"))
        numeric = SPA_103_MODE_FEATURE;
    else if (!strcmp(value, "both") || !strcmp(value, "3")) numeric = SPA_103_MODE_BOTH;
    else {
        numeric = SPA_103_MODE_BOTH;
        LOG_ERROR(LOG_MODULE,
                  "probe103: invalid %s value '%s'; expected info/feature/both; using both",
                  SPA_103_MODE_ENV, value);
    }
    return numeric;
}

/* audioOutputFormatsExtended is an array of APAudioFormatIndex values. For indices below 64 the
 * mapping is exactly the legacy format bit number (`IndexToAPAudioFormat(i) == 1 << i`). The format
 * tables identify 40 as AAC-LC/48k/5.1.2 and 42 as AAC-ELD/48k/5.1.2. Both have a 32-bit Cinemo
 * decoder path in the target MHI2Q firmware. The same firmware also contains a licensed Dolby
 * Digital Plus core, so indices 50/53 are useful fail-closed probes. Keep established APAC/5.1 as
 * the default, but permit only exact known format indices. This selector changes metadata only;
 * none of these payloads is accepted or played by this hook yet. */
static int spa_probe103_index(void)
{
    const spa_audio_config_t *config = spa_audio_config();
    const char *value = getenv(SPA_103_INDEX_ENV);
    char *end = 0;
    int index = SPA_APAC_48K_5_1;
    long candidate;

    if (config->usesNewControls)
        return config->atmos ? SPA_APAC_48K_5_1_2 : SPA_APAC_48K_5_1;
    if (!value || !value[0]) return index;
    candidate = strtol(value, &end, 0);
    if (end && end != value && !*end &&
        (candidate == SPA_AAC_LC_48K_5_1_2 || candidate == SPA_AAC_ELD_48K_5_1_2 ||
         candidate == SPA_DDP_48K_5_1 ||
         candidate == SPA_DDP_48K_9_1_6 || candidate == SPA_APAC_48K_5_1 ||
         candidate == SPA_APAC_48K_5_1_2)) {
        index = (int)candidate;
    } else {
        LOG_ERROR(LOG_MODULE,
                  "probe103: invalid %s value '%s'; allowed indices: %d/%d/%d/%d/%d/%d; using %d",
                  SPA_103_INDEX_ENV, value, SPA_AAC_LC_48K_5_1_2, SPA_AAC_ELD_48K_5_1_2,
                  SPA_DDP_48K_5_1, SPA_DDP_48K_9_1_6, SPA_APAC_48K_5_1,
                  SPA_APAC_48K_5_1_2, index);
    }
    return index;
}

/* Experimental AAC bridge: iOS's spatial gate looks for at least one APAC-family index in the
 * extended array, while the type-103 format copier preserves lower AAC indices.  Keep the normal
 * profiles single-format.  An explicit extra 65/66 lets us test [40,65] or [42,65] without
 * weakening the primary selector or silently changing proven APAC negotiation. */
static int spa_probe103_extra_index(int primaryIndex)
{
    /* The new-controls path used to refuse a second index outright.  That is what left the
     * advertisement at [65] no matter what SPATIAL_103_EXTRA_INDEX said, and a non-spatial track
     * with no format it can arrive in is a track the phone skips.  The variable is explicit, so
     * honour it here too; absent or empty still means one index. */
    const char *value = getenv(SPA_103_EXTRA_INDEX_ENV);
    char *end = 0;
    long candidate;

    if (!value || !value[0]) return -1;
    candidate = strtol(value, &end, 0);
    /* 22 is AAC-LC/44.1/stereo.  It is allowed because the sender's own buffered set contains it
     * unconditionally: APAudioFormatGetSupportedTransportAudioFormatsForBufferedSender returns
     * 0x400000 (bit 22) plus the spatial bits, and APAudioFormatCopyPreferredBufferedAudioFormats
     * reaches for 0x400000 first.  Advertising 65 alone leaves a non-spatial track with no format
     * it can arrive in, which is why the phone skips those.  PCM is not an option despite being
     * third in that preference list: APAudioFormatGetBufferedPassthroughSupportedFormats returns
     * zero unless the device is an APM speaker, so bit 11 never reaches a CarPlay session. */
    if (end && end != value && !*end &&
        (candidate == SPA_APAC_48K_5_1 || candidate == SPA_APAC_48K_5_1_2 ||
         candidate == SPA_APAC_48K_STEREO || candidate == SPA_AAC_LC_44K_STEREO ||
         candidate == SPA_AAC_LC_48K_STEREO || candidate == SPA_ALAC_48K_24_STEREO)) {
        if (candidate == primaryIndex) return -1;
        return (int)candidate;
    }

    LOG_ERROR(LOG_MODULE,
              "probe103: invalid %s value '%s'; allowed extra gate indices: "
              "%d/%d/%d/%d/%d/%d; ignored",
              SPA_103_EXTRA_INDEX_ENV, value, SPA_ALAC_48K_24_STEREO, SPA_AAC_LC_44K_STEREO,
              SPA_AAC_LC_48K_STEREO, SPA_APAC_48K_STEREO, SPA_APAC_48K_5_1,
              SPA_APAC_48K_5_1_2);
    return -1;
}

/* iOS repackages every bit in the legacy mask into one buffered-engine codec family. On wired
 * CarPlay its local support filter leaves bits 11 (PCM) and 22 (AAC-LC) from the old 0x1440800
 * transport-family mask, then rejects the mixed family with -6705 before endpoint activation.
 * Advertise the single AAC-LC compatibility bit by default. Keep the old mask selectable only to
 * reproduce the negative result without another rebuild. */
static uint32_t spa_probe103_legacy_mask(void)
{
    const char *value = getenv(SPA_103_MASK_ENV);
    char *end = 0;
    unsigned long candidate;

    if (!value || !value[0]) return SPA_BUFFERED_AAC_MASK;

    candidate = strtoul(value, &end, 0);
    if (!end || end == value || *end ||
        (candidate != SPA_BUFFERED_AAC_MASK && candidate != SPA_BUFFERED_BROAD_MASK)) {
        LOG_ERROR(LOG_MODULE,
                  "probe103: invalid %s value '%s'; allowed %#x/%#x; using %#x",
                  SPA_103_MASK_ENV, value, SPA_BUFFERED_AAC_MASK,
                  SPA_BUFFERED_BROAD_MASK, SPA_BUFFERED_AAC_MASK);
        return SPA_BUFFERED_AAC_MASK;
    }
    return (uint32_t)candidate;
}

static int spa_cf_get_i64(const void *dict, const char *name, int64_t *value)
{
    void *key;
    int32_t err = 0;
    if (!dict || !name || !value) return 0;
    key = cfa_str(0, name, SPA_CFENC_UTF8);
    if (!key) return 0;
    if (!cfa_dictget(dict, key)) {
        cfa_rel(key);
        return 0;
    }
    *value = cfa_dictgeti(dict, key, &err);
    cfa_rel(key);
    return err == 0;
}

static int spa_cf_get_string(const void *dict, const char *name,
                             char *buffer, size_t capacity)
{
    void *key, *value;
    int result = 0;

    if (!dict || !name || !buffer || !capacity || !cfa_strgetc) return 0;
    key = cfa_str(0, name, SPA_CFENC_UTF8);
    if (!key) return 0;
    value = cfa_dictget(dict, key);
    if (value) result = cfa_strgetc(value, buffer, capacity) != 0;
    cfa_rel(key);
    return result;
}

/* SETUP carries a dictionary mapping media-kind numbers (as strings) to a
 * signed target dB.  Do not dereference CFLite objects: use only the recovered
 * dictionary/string/int accessor ABI. */
static int spa103_read_soundcheck_target(const void *streamDesc,
                                         uint8_t *mediaKind, int *targetDb)
{
    void *outerKey, *targets;
    unsigned kind;

    if (!streamDesc || !mediaKind || !targetDb) return 0;
    outerKey = cfa_str(0, "soundCheckMediaKind", SPA_CFENC_UTF8);
    if (!outerKey) return 0;
    targets = cfa_dictget(streamDesc, outerKey);
    cfa_rel(outerKey);
    if (!targets) return 0;

    for (kind = 0; kind <= 255; kind++) {
        char name[4];
        void *key;
        int32_t err = 0;
        int64_t value;

        snprintf(name, sizeof(name), "%u", kind);
        key = cfa_str(0, name, SPA_CFENC_UTF8);
        if (!key) continue;
        if (!cfa_dictget(targets, key)) {
            cfa_rel(key);
            continue;
        }
        value = cfa_dictgeti(targets, key, &err);
        cfa_rel(key);
        if (!err && value >= -128 && value <= 127) {
            *mediaKind = (uint8_t)kind;
            *targetDb = (int)value;
            return 1;
        }
    }
    return 0;
}

static int spa103_index_supported_on_fixed_48k(int64_t index)
{
    return index == SPA_AAC_LC_48K_STEREO ||
           index == SPA_APAC_48K_STEREO ||
           index == SPA_APAC_48K_5_1 ||
           index == SPA_APAC_48K_5_1_2;
}

static int spa103_validate_setup_stream(const void *streamDesc,
                                        int64_t formatIndex, int haveKey)
{
    int64_t value = -1;
    char text[32] = { 0 };

    if (!spa103_index_supported_on_fixed_48k(formatIndex)) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: rejecting SETUP index=%lld: fixed 48-kHz rail supports 23/64/65/66",
                  (long long)formatIndex);
        return 0;
    }
    if (!haveKey) {
        LOG_ERROR(LOG_MODULE, "buffer103: rejecting SETUP without a 32-byte shk");
        return 0;
    }
    if (!spa_cf_get_i64(streamDesc, "packetFormatAPAP", &value) || !value) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: rejecting SETUP without packetFormatAPAP=true");
        return 0;
    }
    value = -1;
    if (!spa_cf_get_i64(streamDesc, "spf", &value) ||
        value != 1024) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: rejecting SETUP spf=%lld; only %u is implemented",
                  (long long)value, 1024u);
        return 0;
    }
    if (!spa_cf_get_i64(streamDesc, "isMedia", &value) || !value) {
        LOG_ERROR(LOG_MODULE, "buffer103: rejecting SETUP without isMedia=true");
        return 0;
    }
    if (!spa_cf_get_string(streamDesc, "audioType", text, sizeof(text)) ||
        strcmp(text, "media")) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: rejecting SETUP audioType='%s'",
                  text[0] ? text : "<absent>");
        return 0;
    }
    if (spa_cf_get_string(streamDesc, "audioMode", text, sizeof(text)) &&
        strcmp(text, "default")) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: rejecting unsupported audioMode='%s'", text);
        return 0;
    }
    value = -1;
    if (!spa_cf_get_i64(streamDesc, "audioFormat", &value) ||
        value != (formatIndex == SPA_AAC_LC_48K_STEREO ?
                  INT64_C(0x800000) : 0)) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: rejecting SETUP index=%lld audioFormat=%#llx",
                  (long long)formatIndex, (unsigned long long)value);
        return 0;
    }
    if (spa_cf_get_i64(streamDesc, "ct", &value)) {
        int64_t expected = formatIndex == SPA_AAC_LC_48K_STEREO ? 4 : 0;
        if (value != expected) {
            LOG_ERROR(LOG_MODULE,
                      "buffer103: rejecting SETUP index=%lld ct=%lld expected=%lld",
                      (long long)formatIndex, (long long)value,
                      (long long)expected);
            return 0;
        }
    }
    return 1;
}

static void *spa_find_setup_stream(const void *request, int64_t wantedType)
{
    void *kStreams, *streams, *found = 0;
    long count, i;
    int64_t type;

    kStreams = cfa_str(0, "streams", SPA_CFENC_UTF8);
    if (!kStreams) return 0;
    streams = cfa_dictget(request, kStreams);
    cfa_rel(kStreams);
    if (!streams) return 0;

    count = cfa_arrcount(streams);
    for (i = 0; i < count; i++) {
        void *desc = cfa_arridx(streams, i);
        if (desc && spa_cf_get_i64(desc, "type", &type) && type == wantedType) {
            found = desc;
            break;
        }
    }
    return found;
}

static void spa_log_setup_i64(const void *desc, const char *key)
{
    int64_t value;
    if (spa_cf_get_i64(desc, key, &value))
        LOG_INFO(LOG_MODULE, "probe103: request stream.%s=%lld (%#llx)", key,
                 (long long)value, (unsigned long long)value);
}

static void spa_log_setup_string(const void *desc, const char *keyName)
{
    void *key, *value;
    char buf[96];
    if (!cfa_strgetc) return;
    key = cfa_str(0, keyName, SPA_CFENC_UTF8);
    if (!key) return;
    value = cfa_dictget(desc, key);
    cfa_rel(key);
    if (value && cfa_strgetc(value, buf, sizeof(buf)))
        LOG_INFO(LOG_MODULE, "probe103: request stream.%s=%s", keyName, buf);
}

static int spa_write_plist_path(const void *object, const char *path)
{
#if !ENABLE_AUDIO_DIAGNOSTICS
    (void)object;
    (void)path;
    return 1;
#else
    int fd, err;
    FILE *file;

    if (!object || !cfa_plwrite) {
        LOG_ERROR(LOG_MODULE, "probe103: cannot dump %s — CFPropertyListWriteToANSIFile unavailable",
                  path ? path : "plist");
        return 0;
    }
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        LOG_ERROR(LOG_MODULE, "probe103: open(%s) failed", path);
        return 0;
    }
    file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        LOG_ERROR(LOG_MODULE, "probe103: fdopen(%s) failed", path);
        return 0;
    }
    err = cfa_plwrite(object, "binary1", file);
    if (fflush(file) != 0 && err == 0) err = -1;
    fclose(file);
    if (err != 0) {
        LOG_ERROR(LOG_MODULE, "probe103: plist writer failed for %s (err=%d)", path, err);
        return 0;
    }
    LOG_INFO(LOG_MODULE, "probe103: captured %s", path);
    return 1;
#endif
}

static int spa_write_setup103_plist(const void *object, const char *kind, uint32_t sequence)
{
#if !ENABLE_AUDIO_DIAGNOSTICS
    (void)object;
    (void)kind;
    (void)sequence;
    return 1;
#else
    char path[128];
    snprintf(path, sizeof(path), "/tmp/spatial_setup103_%u_%s.bplist",
             (unsigned)sequence, kind);
    return spa_write_plist_path(object, path);
#endif
}

static int spa_write_control_plist(const void *object, const char *kind, uint32_t sequence)
{
#if !ENABLE_AUDIO_DIAGNOSTICS
    (void)object;
    (void)kind;
    (void)sequence;
    return 1;
#else
    char path[128];
    snprintf(path, sizeof(path), "/tmp/spatial_control_%u_%s.bplist",
             (unsigned)sequence, kind);
    return spa_write_plist_path(object, path);
#endif
}

static int spa_array_contains_string(const void *array, const char *wanted)
{
    void *needle;
    long count, i;
    int found = 0;

    if (!array || !wanted) return 0;
    needle = cfa_str(0, wanted, SPA_CFENC_UTF8);
    if (!needle) return 0;
    count = cfa_arrcount(array);
    for (i = 0; i < count; i++) {
        void *value = cfa_arridx(array, i);
        if (value && cfa_equal(value, needle)) {
            found = 1;
            break;
        }
    }
    cfa_rel(needle);
    return found;
}

#if ENABLE_STATE_TRACE
/* Summarise the known CFLite SETUP schema without serialising it.  Values are
 * copied through the same verified accessor ABI used by the live receiver;
 * no opaque object is dereferenced and no UUID/cookie/payload is logged. */
static void spa_state_trace_setup(const void *object, uint32_t sequence,
                                  const char *phase, int result)
{
    void *features_key = 0, *enabled_key = 0, *streams_key = 0;
    void *features = 0, *enabled = 0, *streams = 0;
    long stream_count = 0, i;

    if (!object || !spa_cf_resolve()) {
        STATE_TRACE("AUDIO_SETUP_SUMMARY",
                    "phase=%s setup_seq=%u result=%d object=%d cf_ready=%d",
                    phase, (unsigned)sequence, result, object ? 1 : 0,
                    cfa_ready > 0 ? 1 : 0);
        return;
    }
    features_key = cfa_str(0, "features", SPA_CFENC_UTF8);
    enabled_key = cfa_str(0, "enabledFeatures", SPA_CFENC_UTF8);
    streams_key = cfa_str(0, "streams", SPA_CFENC_UTF8);
    if (features_key) features = cfa_dictget(object, features_key);
    if (enabled_key) enabled = cfa_dictget(object, enabled_key);
    if (streams_key) streams = cfa_dictget(object, streams_key);

    if (features)
        STATE_TRACE("AIRPLAY_FEATURES",
                    "phase=%s setup_seq=%u kind=proposed count=%ld "
                    "mainBuffered=%d result=%d",
                    phase, (unsigned)sequence, cfa_arrcount(features),
                    spa_array_contains_string(features, "mainBuffered"), result);
    if (enabled)
        STATE_TRACE("AIRPLAY_FEATURES",
                    "phase=%s setup_seq=%u kind=enabled count=%ld "
                    "mainBuffered=%d result=%d",
                    phase, (unsigned)sequence, cfa_arrcount(enabled),
                    spa_array_contains_string(enabled, "mainBuffered"), result);

    stream_count = streams ? cfa_arrcount(streams) : 0;
    STATE_TRACE("AUDIO_SETUP_SUMMARY",
                "phase=%s setup_seq=%u result=%d control=%d streams=%ld",
                phase, (unsigned)sequence, result, features ? 1 : 0,
                stream_count);
    for (i = 0; i < stream_count; i++) {
        void *stream = cfa_arridx(streams, i);
        int64_t type = -1, format_index = -1, format = -1;
        int64_t sample_rate = -1, spf = -1, stream_id = -1;
        int64_t data_port = -1, buffer_size = -1;
        if (!stream) continue;
        (void)spa_cf_get_i64(stream, "type", &type);
        (void)spa_cf_get_i64(stream, "audioFormatIndex", &format_index);
        (void)spa_cf_get_i64(stream, "audioFormat", &format);
        (void)spa_cf_get_i64(stream, "sampleRate", &sample_rate);
        (void)spa_cf_get_i64(stream, "spf", &spf);
        (void)spa_cf_get_i64(stream, "streamID", &stream_id);
        (void)spa_cf_get_i64(stream, "dataPort", &data_port);
        (void)spa_cf_get_i64(stream, "audioBufferSize", &buffer_size);
        STATE_TRACE("AUDIO_SETUP_STREAM",
                    "phase=%s setup_seq=%u ordinal=%ld type=%lld "
                    "format_index=%lld format=%lld sample_rate=%lld spf=%lld "
                    "stream_id=%lld data_port=%lld buffer_size=%lld",
                    phase, (unsigned)sequence, i,
                    (long long)type, (long long)format_index,
                    (long long)format, (long long)sample_rate,
                    (long long)spf, (long long)stream_id,
                    (long long)data_port, (long long)buffer_size);
    }

    if (streams_key) cfa_rel(streams_key);
    if (enabled_key) cfa_rel(enabled_key);
    if (features_key) cfa_rel(features_key);
}
#else
#define spa_state_trace_setup(object, sequence, phase, result) ((void)0)
#endif

static void spa_log_feature_array(const char *label, const void *array)
{
    long count, i;
    char value[96];

    if (!array) {
        LOG_INFO(LOG_MODULE, "probe103: %s absent", label);
        return;
    }
    count = cfa_arrcount(array);
    LOG_INFO(LOG_MODULE, "probe103: %s count=%ld", label, count);
    if (!cfa_strgetc) return;
    for (i = 0; i < count; i++) {
        void *item = cfa_arridx(array, i);
        if (item && cfa_strgetc(item, value, sizeof(value)))
            LOG_INFO(LOG_MODULE, "probe103: %s[%ld]=%s", label, i, value);
    }
}

static int spa_inspect_setup_request(void *request, uint32_t sequence)
{
    void *desc;
    if (!request || !spa_probe103_mode() || !spa_cf_resolve()) return 0;
    desc = spa_find_setup_stream(request, 103);
    if (!desc) return 0;

    LOG_INFO(LOG_MODULE, "probe103: intercepted type-103 SETUP before stock (sequence=%u)",
             (unsigned)sequence);
    spa_log_setup_i64(desc, "type");
    spa_log_setup_i64(desc, "audioFormat");
    spa_log_setup_i64(desc, "audioFormatIndex");
    spa_log_setup_i64(desc, "streamConnectionID");
    spa_log_setup_i64(desc, "controlPort");
    spa_log_setup_i64(desc, "timingPort");
    spa_log_setup_i64(desc, "eventPort");
    spa_log_setup_i64(desc, "dataPort");
    spa_log_setup_i64(desc, "latencyMs");
    spa_log_setup_i64(desc, "supportsDynamicStreamID");
    spa_log_setup_string(desc, "audioType");
    spa_write_setup103_plist(request, "request", sequence);
    return 1;
}

static void spa_note_setup_result(int has103, int result, void *response, uint32_t sequence)
{
    if (!has103) return;
    if (result == 0) {
        LOG_WARN(LOG_MODULE,
                 "probe103: stock libairplay returned success for unsupported type-103 SETUP; "
                 "hook must supply the missing response/transport (sequence=%u)",
                 (unsigned)sequence);
        if (response) spa_write_setup103_plist(response, "response", sequence);
    } else {
        LOG_INFO(LOG_MODULE,
                 "probe103: stock libairplay rejected type-103 SETUP as expected: err=%d (sequence=%u); "
                 "type-100 fallback remains available through the QSA sink bridge",
                 result, (unsigned)sequence);
    }
}

/* ---- self-contained type-103 capture receiver ----------------------------------------------
 *
 * Stock MHI2Q logs "Unsupported stream type: 103" but still returns err=0 with an empty response.
 * AirPlaySender treats that as a fatal SETUP failure and tears the complete CarPlay session down.
 * The iOS 27 sender's legacy contract for the request we captured requires
 *
 *     response["streams"] += {
 *         type:103, dataPort:<TCP listener>,
 *         audioBufferSize:<format-specific receiver default>, arrivalToRenderLatencyMs:0,
 *         streamID:<unique nonzero ID>
 *     }
 *
 * dataPort and audioBufferSize are mandatory in AirPlaySender's no-streamConnections branch. The
 * iOS 27 receiver's matching legacy response can return controlPort as well.  The current CarPlay
 * sender, however, uses legacy APAP (not APAT), never connected to 6033 in the hardware trace, and
 * sends its codec cookies as SETMAGICCOOKIE on the existing RTSP control connection.  Therefore a
 * fresh build opens only the APAP data listener. Latency is optional (zero is the sender's own
 * fallback) and streamID is returned because this request explicitly advertises
 * supportsDynamicStreamID. APAC is decoded into the bounded PCM rail below and drained through the
 * existing type-100-owned DIO5/QSA handle; type 100 remains the fallback scheduler/source until
 * the buffered decoder reaches RUNNING.
 */
#define SPA_103_STREAM_TYPE       103
#define SPA_103_PREFERRED_PORT   (-6032) /* -6030 main screen, -6031 alt screen */
#define SPA_103_CAPTURE_LIMIT    (2u * 1024u * 1024u)
/* Dormant compatibility branch in the capture thread; fresh setup never creates this listener. */
#define SPA_103_CONTROL_CAPTURE_LIMIT (512u * 1024u)
#define SPA_103_TEARDOWN_WAIT_MS 2500
#define SPA_103_APAP_PREFILL_FRAMES 32u
#define SPA_103_APAP_FRAME_NS 21333333L /* 1024 samples / 48000 Hz */
#define SPA_103_APAP_MAX_FRAME (1024u * 1024u)
#define SPA_103_MAX_MAGIC_COOKIES 16u
#define SPA_103_MAX_MAGIC_COOKIE_SIZE (1024u * 1024u)
#define SPA_103_MAX_MAGIC_COOKIE_BODY_SIZE (SPA_103_MAX_MAGIC_COOKIE_SIZE + 65536u)

#define SPA_103_ANCHOR_LEAD_MS           150u

/* The public type-103 hooks are portable, but the HTTP dispatcher, request
 * layout and session-clock bridge use private libairplay addresses.  Keep all
 * of those values in one fingerprinted firmware profile instead of embedding
 * a release name in common SPA103 logic.  K1004, for example, places
 * EnsureCreated at 0x2e94c rather than 0x2e9ac and therefore needs its own
 * independently reviewed profile.
 *
 * This check is first reached from a real interposed call.  No hook-owned ELF
 * constructor may call dlsym, inspect code bytes, log, or create a thread. */
typedef struct {
    const char *name;
    uintptr_t ensureCreatedVA;
    uintptr_t setPlistResponseVA;
    uintptr_t httpProcessVA;
    uintptr_t serverGlobalVA;
    size_t httpProcessSlotOffset;
    size_t sessionClockOffset;
    size_t sessionFamilyOffset;
    size_t httpMethodPointerOffset;
    size_t httpMethodLengthOffset;
    size_t httpBodyPointerOffset;
    size_t httpBodyLengthOffset;
    uint8_t ensureCreatedPrologue[12];
    uint8_t setPlistResponsePrologue[12];
} spa103_private_profile_t;

static const spa103_private_profile_t spa103_private_profiles[] = {
    {
        "MHI2Q AirPlay 210.81",
        0x0002e9acu, 0x0002f040u, 0x000302e0u, 0x000babccu,
        0x64u, 5072u, 85u,
        0x201cu, 0x2020u, 0x209cu, 0x20a8u,
        { 0xf0, 0x45, 0x2d, 0xe9, 0xdc, 0x41, 0x9f, 0xe5,
          0xdc, 0x61, 0x9f, 0xe5 },
        { 0xf0, 0x41, 0x2d, 0xe9, 0xc8, 0x50, 0x90, 0xe5,
          0x03, 0x70, 0xa0, 0xe1 }
    }
};

static const spa103_private_profile_t *spa103_private_profile(void)
{
    static const spa103_private_profile_t *selected;
    static volatile int checked; /* 0 unknown, 1 selected, -1 rejected */
    uintptr_t ensureAddress;
    size_t i;

    if (checked) return checked > 0 ? selected : 0;

    ensureAddress = (uintptr_t)dlsym(RTLD_NEXT, "AirTunesServer_EnsureCreated");
    for (i = 0; i < sizeof(spa103_private_profiles) / sizeof(spa103_private_profiles[0]); i++) {
        const spa103_private_profile_t *profile = &spa103_private_profiles[i];
        uintptr_t imageBase;

        if (ensureAddress < profile->ensureCreatedVA) continue;
        imageBase = ensureAddress - profile->ensureCreatedVA;
        if ((imageBase & 0xfffu) == 0 &&
            memcmp((const void *)(imageBase + profile->ensureCreatedVA),
                   profile->ensureCreatedPrologue,
                   sizeof(profile->ensureCreatedPrologue)) == 0 &&
            memcmp((const void *)(imageBase + profile->setPlistResponseVA),
                   profile->setPlistResponsePrologue,
                   sizeof(profile->setPlistResponsePrologue)) == 0) {
            selected = profile;
            __sync_synchronize();
            checked = 1;
            LOG_INFO(LOG_MODULE, "probe103: selected private profile '%s'", profile->name);
            return profile;
        }
    }

    checked = -1;
    LOG_WARN(LOG_MODULE,
             "probe103 requested but no reviewed private libairplay profile matched; "
             "disabled fail-closed");
    return 0;
}

static int spa103_private_image_compatible(void)
{
    return spa103_private_profile() != 0;
}

typedef int (*spa_srv_sockopen_t)(int, int, int, int, int *, int, int *);
typedef int (*spa_sock_accept_t)(int, int, int *, void *);
typedef int (*spa_http_process_t)(void *, void *);
typedef void *(*spa_cf_create_plist_bytes_t)(const void *, size_t, uint32_t, uint32_t, int32_t *);
typedef uint32_t (*spa_cf_type_id_t)(void);
typedef void *(*spa_cf_dict_get_typed_t)(const void *, const void *, uint32_t, int32_t *);
typedef long (*spa_cf_data_length_t)(const void *);
typedef const uint8_t *(*spa_cf_data_bytes_t)(const void *);
typedef int (*spa_http_init_response_t)(void *, int);
typedef int (*spa_http_send_response_t)(void *);
typedef int (*spa_http_set_plist_response_t)(void *, void *, void *, int *);
typedef uint64_t (*spa_up_ticks_t)(void);
/* TickUtils.h and the ARM EABI body both take uint64_t. Declaring this as uint32_t leaves r1
 * undefined, turning the intended +150 ms anchor into a random 64-bit interval. */
typedef uint64_t (*spa_milliseconds_to_up_ticks_t)(uint64_t);

/* AirTunesTime for the AirPlay 210.81 profile, verified from
 * AirTunesClock_GetSynchronizedTimeNearUpTicks @ 0x2dc30:
 * seconds at +0, reserved padding at +4, and 64-bit fractional seconds at +8. Unlike the newer
 * 32-byte Pioneer structure, this target image does not return a timeline ID here. */
typedef struct {
    uint32_t secs;
    uint32_t reserved;
    uint64_t frac;
} spa103_air_tunes_time_t;

typedef void (*spa_air_tunes_clock_get_near_ticks_t)(
    void *, spa103_air_tunes_time_t *, uint64_t);

typedef struct {
    int valid;
    int64_t rtpTime;
    int64_t mediaTimeValue;
    int64_t mediaTimeScale;
} spa103_transport_time_t;

typedef struct {
    int valid;
    int64_t secs;
    int64_t frac;
    int64_t timelineID;
    int64_t flags;
} spa103_network_time_t;

typedef struct {
    int64_t id;
    uint8_t *bytes;
    size_t size;
} spa103_magic_cookie_t;

typedef struct {
    void *session;
    int listenSock;
    int clientSock;
    pthread_t thread;
    int threadCreated;
    int threadExited;
    int controlListenSock;
    int controlClientSock;
    pthread_t controlThread;
    int controlThreadCreated;
    int controlThreadExited;
    int settingUp;
    volatile int stop;
    uint32_t generation;
    uint32_t sequence;
    spa103_magic_cookie_t magicCookies[SPA_103_MAX_MAGIC_COOKIES];
    size_t magicCookieCount;
    /* Bumped whenever a stored cookie's CONTENT changes.  The id stays the same across a track
     * change, so it cannot be what tells the decoder to reconfigure. */
    uint32_t cookieGeneration;
    int64_t rate;
    spa103_transport_time_t anchor;
    spa103_transport_time_t firstAudible;
    spa103_network_time_t networkTime;
    uint32_t controlRequestCount;
    uint32_t nowPlayingRetryAttempts;
    uint64_t nowPlayingRetryAfterNs;
    int nowPlayingRetryQueued;
    /* The type-103 SETUP `shk`, copied verbatim: it IS the ChaCha20-Poly1305 key,
     * with no HKDF anywhere on this path.  Cleared on every teardown. */
    uint8_t shk[SPA_APAP_KEY_SIZE];
    int haveKey;
    int64_t formatIndex;
    int haveSoundCheckTarget;
    uint8_t soundCheckMediaKind;
    int8_t soundCheckTargetDb;
} spa103_receiver_t;

typedef struct {
    uint32_t generation;
    uint32_t sequence;
    int listenSock;
    int isControl;
} spa103_thread_arg_t;

static pthread_mutex_t spa103_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  spa103_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t spa103_http_lock = PTHREAD_MUTEX_INITIALIZER;
static spa103_receiver_t spa103_receiver = {
    .listenSock = -1,
    .clientSock = -1,
    .controlListenSock = -1,
    .controlClientSock = -1
};
static spa_srv_sockopen_t spa103_srv_sockopen;
static spa_sock_accept_t  spa103_sock_accept;
static spa_http_process_t spa103_original_http_process;
static void **spa103_http_process_slot;
static spa_cf_create_plist_bytes_t spa103_cf_create_plist_bytes;
static spa_cf_type_id_t spa103_cf_dictionary_type_id;
static spa_cf_type_id_t spa103_cf_data_type_id;
static spa_cf_dict_get_typed_t spa103_cf_dict_get_typed;
static spa_cf_data_length_t spa103_cf_data_length;
static spa_cf_data_bytes_t spa103_cf_data_bytes;
static spa_http_init_response_t spa103_http_init_response;
static spa_http_send_response_t spa103_http_send_response;
static spa_http_set_plist_response_t spa103_http_set_plist_response;
static spa_up_ticks_t spa103_up_ticks;
static spa_milliseconds_to_up_ticks_t spa103_milliseconds_to_up_ticks;
static spa_air_tunes_clock_get_near_ticks_t spa103_clock_get_near_ticks;

static int spa103_http_process(void *connection, void *request);
static int spa103_resolve(void);
static size_t spa103_write_capture(int fd, const uint8_t *data, size_t size);
static int spa103_response_set_i64(void *responseDesc, const char *name, int64_t value);
static int64_t spa103_media_time_to_rtp(int64_t mediaValue, int64_t mediaScale);

/* ---- decode / render ------------------------------------------------------------------------
 *
 * The APAP receiver decodes every buffered frame using the codec selected by
 * the inline magic cookie:
 *
 *   BE32-inclusive framing -> ChaCha20-Poly1305 open -> TLV split -> APAC/AAC decode
 *   -> S16 -> native 5.1 reorder / experimental 5.1.2 policy / stereo upmix
 *   -> bounded PCM6 ring
 *
 * The ring is where the existing PCM6/DIO5 rail collects it: while type 103 is
 * carrying media the type-100 read path hands out these samples instead of its
 * own. That seam is vehicle-proven for APAC 5.1 and AAC stereo in both switch
 * directions. If type 100 disappears, this file's existing APAP thread becomes
 * a one-period-at-a-time QSA render pump; with no consumer the producer remains
 * paced and the bounded ring may still drop its oldest data.
 *
 * Failure policy.  A frame that will not authenticate or decode is counted and dropped; it does
 * not stop the receiver, because the drain has to keep the phone's session alive whatever the
 * codec does.  Unsupported formats retain a distinct status.  Native 5.1 remains on its measured
 * reorder; ordinary stereo stays on fixed PCM6 geometry and may add only bounded Ls/Rs ambience.
 */
#define SPA_103_RENDER_MAX_CHANNELS SPA_PCM_5_1_2_CHANNELS
#define SPA_103_RENDER_FRAMES     1024u
/* Where the framing probe writes raw access units, and how much it will write. */
#define SPA_103_ES_DUMP_PATH      "/tmp/spatial_aac_es.bin"
#define SPA_103_ES_DUMP_LIMIT     (6u * 1024u * 1024u)
#define SPA_103_RENDER_MAX_SAMPLES (SPA_103_RENDER_MAX_CHANNELS * SPA_103_RENDER_FRAMES)
/* Half a second of 48 kHz 5.1: 288 KB.  Big enough to absorb the phone's burst between two
 * reads of the sink, small enough that a stalled sink cannot hide a minute of latency. */
#define SPA_103_RING_FRAMES       24000u
/* Above this the producer paces itself.  With no consumer the ring sits full and every frame
 * pauses, which is the backpressure the capture-only build faked with an unconditional sleep. */
#define SPA_103_RING_HIGH_FRAMES  (spa103_ring_frames() / 2u)
#define SPA_PCM_SINK_PERIOD_MS 16u
#define SPA_PCM_SINK_PERIOD_FRAMES ((SPA_SR * SPA_PCM_SINK_PERIOD_MS) / 1000u)
/* Serving the sink from an almost-empty ring is what the car heard.  The stock
 * type-100 write cadence, not the phone's buffered delivery, decides when a
 * fragment must be handed over; starting at fill=0 meant the first second of
 * every session was padded with silence -- roughly 50 frames missing out of each
 * 768, a ~1 ms hole every 16 ms, which is a 62 Hz buzz rather than a dropout.
 * Wait for a cushion before the first serve.  400 ms is 25 fragments of head
 * start and four fifths of the 24000-frame ring. */
#define SPA_103_PRIME_FRAMES      ((SPA_SR * 400u) / 1000u)
/* After an explicit FLUSHBUFFERED the old item is invalid and must be dropped,
 * but waiting for the full cold-start cushion adds avoidable silence.  Ten
 * measured 16-ms device fragments are enough to satisfy QSA START_FULL; the
 * persistent high-water pump grows the software cushion from there. */
#define SPA_103_REPRIME_FRAMES    ((SPA_SR * 160u) / 1000u)

/* The three numbers above are a latency/robustness trade the car is the only judge of:
 * steady-state delay is set by the high-water mark, time-to-first-sound by the prime cushion.
 * They are tunable at runtime so a value can be tried on the unit without a rebuild, because
 * guessing them from a desk is how the 400 ms cushion got picked in the first place.
 *
 *   AUDIO_RING_MS  ring capacity        default 500
 *   AUDIO_HIGH_MS  backpressure mark    default 250
 *   AUDIO_PRIME_MS first-serve cushion  default 400
 *   AUDIO_REPRIME_MS post-flush cushion default 160
 *
 * Clamped so a typo cannot produce a ring smaller than its own cushion. */
static unsigned spa103_ms_env(const char *name, unsigned defMs)
{
    const char *v = getenv(name);
    char *end;
    unsigned long n;
    if (!v || !*v) return defMs;
    n = strtoul(v, &end, 0);
    if (*end || n < 20u || n > 4000u) return defMs;
    return (unsigned)n;
}

static unsigned spa103_ring_frames(void)
{
    static unsigned cached;
    if (!cached) cached = (SPA_SR * spa103_ms_env("AUDIO_RING_MS", 500u)) / 1000u;
    return cached;
}

static unsigned spa103_high_frames(void)
{
    static unsigned cached;
    if (!cached) {
        unsigned ring = spa103_ring_frames();
        cached = (SPA_SR * spa103_ms_env("AUDIO_HIGH_MS", 250u)) / 1000u;
        if (cached >= ring) cached = ring / 2u;
    }
    return cached;
}

static unsigned spa103_prime_frames(void)
{
    static unsigned cached;
    if (!cached) {
        unsigned ring = spa103_ring_frames();
        cached = (SPA_SR * spa103_ms_env("AUDIO_PRIME_MS", 400u)) / 1000u;
        if (cached >= ring) cached = (ring * 4u) / 5u;
    }
    return cached;
}

static unsigned spa103_reprime_frames(void)
{
    static unsigned cached;
    if (!cached) {
        unsigned ring = spa103_ring_frames();

        cached = (SPA_SR * spa103_ms_env(
            "AUDIO_REPRIME_MS",
            SPA_103_REPRIME_FRAMES * 1000u / SPA_SR)) / 1000u;
        /* QSA only accepts complete periods.  Round up so crossing this
         * threshold can always seed an integral START_FULL queue. */
        cached = ((cached + SPA_PCM_SINK_PERIOD_FRAMES - 1u) /
                  SPA_PCM_SINK_PERIOD_FRAMES) * SPA_PCM_SINK_PERIOD_FRAMES;
        if (cached > ring)
            cached = (ring / SPA_PCM_SINK_PERIOD_FRAMES) *
                     SPA_PCM_SINK_PERIOD_FRAMES;
        if (!cached) cached = ring;
    }
    return cached;
}

enum {
    SPA_103_RENDER_IDLE = 0,        /* not started, or type 103 not active */
    SPA_103_RENDER_WAITING = 1,     /* have the key, waiting for a magic cookie */
    SPA_103_RENDER_RUNNING = 2,
    SPA_103_RENDER_NO_KEY = 3,      /* SETUP carried no usable shk */
    SPA_103_RENDER_UNSUPPORTED_FORMAT = 4,  /* an index this rail cannot carry at all */
    SPA_103_RENDER_UNSUPPORTED_LAYOUT = 5,  /* index 66: decodable, order unmeasured */
    SPA_103_RENDER_COOKIE_FAILED = 6,       /* the cookie would not configure a decoder */
    SPA_103_RENDER_NO_MEMORY = 7,
    SPA_103_RENDER_NO_DECODER = 8
};

typedef struct {
    pthread_mutex_t lock;
    int state;
    const spa_apac_codec_api_t *codec;
    spa_apac_decoder_t *decoder;
    int64_t cookieId;               /* the cookie the decoder was built from */
    int haveCookieId;
    uint32_t cookieGeneration;      /* ...and which revision of its bytes */
    int16_t *ring;                  /* spa103_ring_frames() * 6 samples */
    uint32_t *ringSequence;         /* authenticated APAP owner of each PCM frame */
    uint32_t *ringRTP;
    size_t head;                    /* frame index of the oldest buffered frame */
    size_t fill;                    /* frames buffered */
    uint64_t framesIn;
    uint64_t framesOut;
    uint64_t dropped;               /* frames overwritten because nothing drained */
    uint64_t authFailed;
    uint64_t tlvFailed;
    uint64_t decodeCalls;
    uint64_t decodeFailed;
    uint64_t zeroFrameDecodes;      /* non-empty AU accepted, but no PCM released */
    uint64_t stateOnlyPackets;      /* authenticated APAP extension with no audio AU */
    uint64_t sequenceGaps;          /* authenticated 24-bit APAP sequence discontinuities */
    uint32_t lastSequence;
    int haveLastSequence;
    uint64_t upmixFailed;
    uint64_t formatAnnouncements;
    uint64_t formatSwitches;
    uint64_t samplesPerFrameMismatches;
    uint64_t transitionAnnouncements;
    uint64_t sourceFormatAnnouncements;
    uint64_t tracerAnnouncements;
    uint64_t unknownExtensionFields;
    spa_apap_lifecycle_t lifecycle; /* rate, paused compressed queue and FLUSH window */
    int primed;                      /* ring reached the start cushion at least once */
    size_t primeTargetFrames;        /* cold 400 ms; post-FLUSH START_FULL depth */
    uint64_t statsFrames;
    uint64_t statsAbs[SPA_103_RENDER_MAX_CHANNELS];
    uint64_t statsZeroCrossings[SPA_103_RENDER_MAX_CHANNELS];
    uint32_t statsPeak[SPA_103_RENDER_MAX_CHANNELS];
    int16_t statsPrevious[SPA_103_RENDER_MAX_CHANNELS];
    int statsHavePrevious;
    uint32_t sampleRate;
    unsigned decoderChannels;       /* what the cookie asked the decoder to produce */
    const spa_apac_codec_api_t *apacCodec;  /* the dlopen'd APAC port, kept across a switch */
    int probeFraming;               /* negotiated a codec we cannot decode: open one packet,
                                     * report how it is framed, decode nothing */
    uint32_t sourceChannels;
    uint32_t outputChannels;
    uint32_t channelLayoutTag;
    int64_t formatIndex;           /* active TLV-2 index, corrected by cookie/PCM geometry */
    enum spa_decoded_pcm_kind decodedPcmKind;
    int64_t ignoredFormatIndex;    /* last announcement we had no decoder for, to log it once */
    uint64_t lastTransitionId;
    int haveLastTransitionId;
    uint64_t lastSourceFormatIndex;
    int haveLastSourceFormatIndex;
    uint8_t extensionMediaKind;
    int haveExtensionMediaKind;
    float extensionLoudness[3];
    int haveExtensionLoudness;
    int atmosMode;
    struct spa_stereo_upmix stereoUpmix;
    spa_soundcheck_state_t soundCheck;
} spa103_render_t;

static spa103_render_t spa103_render = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .state = SPA_103_RENDER_IDLE
};

static struct spa_hssr_model spa_hssr_model_value;
static pthread_once_t spa_hssr_model_once = PTHREAD_ONCE_INIT;
static int spa_hssr_model_status = SPA_HSSR_ERROR_NOT_READY;

static void spa_hssr_model_init_once(void)
{
    const char *path = getenv(SPA_AUDIO_HSSR_MODEL_ENV);

    if (!path || !path[0]) path = SPA_AUDIO_HSSR_MODEL_DEFAULT;
    spa_hssr_model_status = spa_hssr_model_load_file(
        &spa_hssr_model_value, path);
    if (spa_hssr_model_status == SPA_HSSR_OK) {
        LOG_INFO(LOG_MODULE,
                 "HSSR: model ready path=%s bytes=%u crc=%08x",
                 path, (unsigned)spa_hssr_model_value._allocation_bytes,
                 (unsigned)spa_hssr_model_value.payload_crc32);
    } else {
        LOG_ERROR(LOG_MODULE,
                  "HSSR: model rejected path=%s status=%d; stereo upmix will stay off",
                  path, spa_hssr_model_status);
    }
}

static const struct spa_hssr_model *spa_hssr_runtime_model(void)
{
    pthread_once(&spa_hssr_model_once, spa_hssr_model_init_once);
    return spa_hssr_model_status == SPA_HSSR_OK
        ? &spa_hssr_model_value : NULL;
}

static const char *spa103_render_state_name(int state)
{
    switch (state) {
    case SPA_103_RENDER_IDLE:                return "idle";
    case SPA_103_RENDER_WAITING:             return "waiting-for-cookie";
    case SPA_103_RENDER_RUNNING:             return "running";
    case SPA_103_RENDER_NO_KEY:              return "no-key";
    case SPA_103_RENDER_UNSUPPORTED_FORMAT:  return "unsupported-format";
    case SPA_103_RENDER_UNSUPPORTED_LAYOUT:  return "unsupported-layout";
    case SPA_103_RENDER_COOKIE_FAILED:       return "cookie-failed";
    case SPA_103_RENDER_NO_MEMORY:           return "no-memory";
    case SPA_103_RENDER_NO_DECODER:          return "decoder-unavailable";
    default:                                 return "?";
    }
}

static const char *spa_stereo_upmix_backend_name(enum spa_stereo_upmix_backend backend)
{
    switch (backend) {
    case SPA_STEREO_UPMIX_OFF:           return "off";
    case SPA_STEREO_UPMIX_HSSR_UNIVERSAL:return "hssr-universal";
    default:                             return "?";
    }
}

static const char *spa103_decoded_pcm_kind_name(enum spa_decoded_pcm_kind kind)
{
    switch (kind) {
    case SPA_DECODED_PCM_STEREO:       return "stereo";
    case SPA_DECODED_PCM_APAC_5_1:     return "APAC-5.1";
    case SPA_DECODED_PCM_APAC_5_1_2:   return "APAC-5.1.2";
    default:                           return "other";
    }
}

static const spa_apac_codec_api_t *spa103_alternate_codec(const spa_apac_codec_api_t *current);

static const spa_apac_codec_api_t *spa103_codec_for_index(int64_t index);

/* Called with the receiver lock NOT held. */
static void spa103_render_start(int64_t formatIndex, int haveKey,
                                size_t compressedBufferBytes,
                                int haveSoundCheckTarget,
                                uint8_t soundCheckMediaKind,
                                int soundCheckTargetDb)
{
    const spa_audio_config_t *config = spa_audio_config();
    const char *codecPath = 0;
    struct spa_stereo_upmix_config upmixConfig;
    enum spa_stereo_upmix_backend activeUpmixBackend;
    uint32_t activeFrontGainQ15, activeRearGainQ15, activeLfeGainQ15;
    int activeLfeAssist;
    uint32_t outputChannels =
        formatIndex == SPA_APAC_48K_5_1_2 &&
        config->atmosMode == SPA_ATMOS_PASSTHROUGH ?
        SPA_PCM_5_1_2_CHANNELS : SPA_PCM_5_1_CHANNELS;
    int state, upmixStatus;

    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.decoder && spa103_render.codec) {
        spa103_render.codec->destroy(spa103_render.decoder);
        spa103_render.decoder = 0;
    }
    /* Two decoders, chosen by the negotiated index.  APAC comes from the optional library that
     * is dlopen'd; AAC-LC is compiled in, because libNmeSDK is already in dio_manager's
     * DT_NEEDED and exports every Cinemo entry point by name. */
    spa103_render.apacCodec = spa_apac_runtime_load(&codecPath);
    spa103_render.codec = spa103_codec_for_index(formatIndex);
    if (spa103_render.codec && spa103_render.codec != spa103_render.apacCodec)
        codecPath = "built-in (Cinemo AAC-LC)";
    free(spa103_render.ring);
    free(spa103_render.ringSequence);
    free(spa103_render.ringRTP);
    spa_apap_lifecycle_dispose(&spa103_render.lifecycle);
    spa103_render.ring = (int16_t *)malloc((size_t)spa103_ring_frames() *
                                           outputChannels * sizeof(int16_t));
    spa103_render.ringSequence = (uint32_t *)malloc(
        (size_t)spa103_ring_frames() * sizeof(uint32_t));
    spa103_render.ringRTP = (uint32_t *)malloc(
        (size_t)spa103_ring_frames() * sizeof(uint32_t));
    spa_apap_lifecycle_init(&spa103_render.lifecycle,
                            compressedBufferBytes);
    LOG_INFO(LOG_MODULE,
             "buffer103: ring %u frames (%u ms), backpressure at %u (%u ms), "
             "first serve after %u (%u ms), post-flush after %u (%u ms)",
             spa103_ring_frames(), spa103_ring_frames() * 1000u / SPA_SR,
             spa103_high_frames(), spa103_high_frames() * 1000u / SPA_SR,
             spa103_prime_frames(), spa103_prime_frames() * 1000u / SPA_SR,
             spa103_reprime_frames(), spa103_reprime_frames() * 1000u / SPA_SR);
    spa103_render.head = 0;
    spa103_render.fill = 0;
    spa103_render.primed = 0;
    spa103_render.primeTargetFrames = spa103_prime_frames();
    spa103_render.framesIn = 0;
    spa103_render.framesOut = 0;
    spa103_render.dropped = 0;
    spa103_render.authFailed = 0;
    spa103_render.tlvFailed = 0;
    spa103_render.decodeCalls = 0;
    spa103_render.decodeFailed = 0;
    spa103_render.zeroFrameDecodes = 0;
    spa103_render.stateOnlyPackets = 0;
    spa103_render.sequenceGaps = 0;
    spa103_render.lastSequence = 0;
    spa103_render.haveLastSequence = 0;
    spa103_render.upmixFailed = 0;
    spa103_render.formatAnnouncements = 0;
    spa103_render.formatSwitches = 0;
    spa103_render.samplesPerFrameMismatches = 0;
    spa103_render.transitionAnnouncements = 0;
    spa103_render.sourceFormatAnnouncements = 0;
    spa103_render.tracerAnnouncements = 0;
    spa103_render.unknownExtensionFields = 0;
    spa103_render.statsFrames = 0;
    memset(spa103_render.statsAbs, 0, sizeof(spa103_render.statsAbs));
    memset(spa103_render.statsZeroCrossings, 0,
           sizeof(spa103_render.statsZeroCrossings));
    memset(spa103_render.statsPeak, 0, sizeof(spa103_render.statsPeak));
    memset(spa103_render.statsPrevious, 0, sizeof(spa103_render.statsPrevious));
    spa103_render.statsHavePrevious = 0;
    spa103_render.cookieId = 0;
    spa103_render.haveCookieId = 0;
    spa103_render.sampleRate = 0;
    spa103_render.sourceChannels = 0;
    spa103_render.outputChannels = outputChannels;
    spa103_render.channelLayoutTag = 0;
    spa103_render.formatIndex = formatIndex;
    spa103_render.decodedPcmKind = SPA_DECODED_PCM_OTHER;
    spa103_render.ignoredFormatIndex = -1;
    spa103_render.haveLastTransitionId = 0;
    spa103_render.haveLastSourceFormatIndex = 0;
    spa103_render.haveExtensionMediaKind = 0;
    spa103_render.haveExtensionLoudness = 0;
    spa103_render.probeFraming = 0;
    spa103_render.atmosMode = config->atmosMode;
    spa_soundcheck_init(&spa103_render.soundCheck, &config->soundCheck,
                        haveSoundCheckTarget, soundCheckMediaKind,
                        soundCheckTargetDb);

    spa_stereo_upmix_default_config(&upmixConfig);
    upmixConfig.backend = config->stereoUpmixBackend;
    upmixConfig.rear_gain_q15 = config->stereoSurroundQ15;
    if (upmixConfig.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {
        const struct spa_hssr_model *hssrModel = spa_hssr_runtime_model();

        if (hssrModel) {
            spa_stereo_upmix_config_set_hssr_model(
                &upmixConfig, hssrModel);
        } else {
            upmixConfig.backend = SPA_STEREO_UPMIX_OFF;
        }
    }
    spa_stereo_upmix_config_set_lfe(
        &upmixConfig,
        config->stereoLfeAssist &&
        upmixConfig.backend != SPA_STEREO_UPMIX_OFF);
    /* OFF is an exact compatibility bypass, including the front samples. */
    if (upmixConfig.backend == SPA_STEREO_UPMIX_OFF) {
        upmixConfig.front_gain_q15 = 32768u;
        upmixConfig.rear_gain_q15 = 0;
    }
    upmixStatus = spa_stereo_upmix_init(&spa103_render.stereoUpmix,
                                        &upmixConfig);
    if (upmixStatus != SPA_STEREO_UPMIX_OK) {
        upmixConfig.backend = SPA_STEREO_UPMIX_OFF;
        spa_stereo_upmix_config_set_lfe(&upmixConfig, 0);
        upmixConfig.front_gain_q15 = 32768u;
        upmixConfig.rear_gain_q15 = 0;
        (void)spa_stereo_upmix_init(&spa103_render.stereoUpmix,
                                    &upmixConfig);
    }
    activeUpmixBackend = spa103_render.stereoUpmix.config.backend;
    activeFrontGainQ15 = spa103_render.stereoUpmix.config.front_gain_q15;
    activeRearGainQ15 = spa103_render.stereoUpmix.config.rear_gain_q15;
    activeLfeAssist = spa103_render.stereoUpmix.config.lfe_enabled != 0;
    activeLfeGainQ15 = activeLfeAssist ?
        spa103_render.stereoUpmix.config.lfe_gain_q15 : 0;

    if (!spa103_render.ring || !spa103_render.ringSequence ||
        !spa103_render.ringRTP)
        state = SPA_103_RENDER_NO_MEMORY;
    else if (formatIndex != SPA_AAC_LC_44K_STEREO &&
             formatIndex != SPA_AAC_LC_48K_STEREO &&
             formatIndex != SPA_APAC_48K_STEREO &&
             formatIndex != SPA_APAC_48K_5_1 &&
             formatIndex != SPA_APAC_48K_5_1_2)
        state = SPA_103_RENDER_UNSUPPORTED_FORMAT;
    else if (!spa103_render.codec)
        state = SPA_103_RENDER_NO_DECODER;
    else if (!haveKey)
        state = SPA_103_RENDER_NO_KEY;
    else
        state = SPA_103_RENDER_WAITING;
    spa103_render.state = state;
    pthread_mutex_unlock(&spa103_render.lock);

    LOG_INFO(LOG_MODULE,
             "buffer103: render %s formatIndex=%lld key=%s output=%uch atmosMode=%s "
             "stereoUpmix=%s frontGainQ15=%u rearGainQ15=%u "
             "stereoLfe=%s lfeGainQ15=%u soundCheck=%s target=%s%ddB/kind%u "
             "fallback=%d limiter=%d compressedQueueLimit=%u initialRate=0",
             spa103_render_state_name(state), (long long)formatIndex,
             haveKey ? "yes" : "no", (unsigned)outputChannels,
             config->atmosMode == SPA_ATMOS_DOWNMIX ? "downmix" :
             config->atmosMode == SPA_ATMOS_DROP ? "drop" : "passthrough",
             spa_stereo_upmix_backend_name(activeUpmixBackend),
             (unsigned)activeFrontGainQ15, (unsigned)activeRearGainQ15,
             activeLfeAssist ? "assist" : "off",
             (unsigned)activeLfeGainQ15,
             config->soundCheck.enabled ? "on" : "off",
             haveSoundCheckTarget ? "" : "absent/",
             soundCheckTargetDb, (unsigned)soundCheckMediaKind,
             config->soundCheck.fallbackMode,
             config->soundCheck.limiterMode,
             (unsigned)compressedBufferBytes);
    STATE_TRACE("BUFFERED_RENDER_START",
                "setup_index=%lld state=%s key=%d output_channels=%u "
                "upmix=%s rear_q15=%u lfe=%d soundcheck=%d "
                "soundcheck_target_present=%d soundcheck_target_db=%d "
                "soundcheck_media_kind=%u initial_rate=0 "
                "compressed_limit=%u",
                (long long)formatIndex, spa103_render_state_name(state),
                haveKey ? 1 : 0, (unsigned)outputChannels,
                spa_stereo_upmix_backend_name(activeUpmixBackend),
                (unsigned)activeRearGainQ15, activeLfeAssist ? 1 : 0,
                config->soundCheck.enabled ? 1 : 0,
                haveSoundCheckTarget ? 1 : 0, soundCheckTargetDb,
                (unsigned)soundCheckMediaKind,
                (unsigned)compressedBufferBytes);
    if (upmixStatus != SPA_STEREO_UPMIX_OK)
        LOG_ERROR(LOG_MODULE,
                  "buffer103: stereo upmix init failed status=%d; exact L/R passthrough retained",
                  upmixStatus);
    if (state == SPA_103_RENDER_NO_DECODER)
        LOG_ERROR(LOG_MODULE,
                  "buffer103: optional APAC codec unavailable/rejected at %s; capture continues",
                  codecPath ? codecPath : "<unknown>");
}

static void spa103_render_stop(void)
{
    uint64_t formatAnnouncements, formatSwitches;
    uint64_t framesIn, framesOut, dropped, authFailed, tlvFailed;
    uint64_t decodeCalls, decodeFailed, zeroFrameDecodes;
    uint64_t stateOnlyPackets, sequenceGaps, upmixFailed;
    uint64_t spfMismatches, transitions, sourceFormats, tracers, unknownFields;
    uint64_t soundCheckUpdates, soundCheckRejected, legacySoundCheck;
    uint64_t fallbackUpdates, limitedFrames;
    uint64_t flushDropped, queueAllocationFailures;
    size_t compressedPackets, compressedBytes;
    int64_t finalFormatIndex;

    pthread_mutex_lock(&spa103_render.lock);
    formatAnnouncements = spa103_render.formatAnnouncements;
    formatSwitches = spa103_render.formatSwitches;
    framesIn = spa103_render.framesIn;
    framesOut = spa103_render.framesOut;
    dropped = spa103_render.dropped;
    authFailed = spa103_render.authFailed;
    tlvFailed = spa103_render.tlvFailed;
    decodeCalls = spa103_render.decodeCalls;
    decodeFailed = spa103_render.decodeFailed;
    zeroFrameDecodes = spa103_render.zeroFrameDecodes;
    stateOnlyPackets = spa103_render.stateOnlyPackets;
    sequenceGaps = spa103_render.sequenceGaps;
    upmixFailed = spa103_render.upmixFailed;
    spfMismatches = spa103_render.samplesPerFrameMismatches;
    transitions = spa103_render.transitionAnnouncements;
    sourceFormats = spa103_render.sourceFormatAnnouncements;
    tracers = spa103_render.tracerAnnouncements;
    unknownFields = spa103_render.unknownExtensionFields;
    soundCheckUpdates = spa103_render.soundCheck.metadataUpdates;
    soundCheckRejected = spa103_render.soundCheck.metadataRejected;
    legacySoundCheck = spa103_render.soundCheck.legacyRecords;
    fallbackUpdates = spa103_render.soundCheck.fallbackUpdates;
    limitedFrames = spa103_render.soundCheck.limitedFrames;
    flushDropped = spa103_render.lifecycle.total_flush_dropped;
    queueAllocationFailures = spa103_render.lifecycle.allocation_failures;
    compressedPackets = spa103_render.lifecycle.queued_packets;
    compressedBytes = spa103_render.lifecycle.queued_bytes;
    finalFormatIndex = spa103_render.formatIndex;
    if (spa103_render.decoder && spa103_render.codec) {
        spa103_render.codec->destroy(spa103_render.decoder);
        spa103_render.decoder = 0;
    }
    (void)spa_stereo_upmix_reset(&spa103_render.stereoUpmix);
    free(spa103_render.ring);
    free(spa103_render.ringSequence);
    free(spa103_render.ringRTP);
    spa_apap_lifecycle_dispose(&spa103_render.lifecycle);
    spa103_render.ring = 0;
    spa103_render.ringSequence = 0;
    spa103_render.ringRTP = 0;
    spa103_render.head = 0;
    spa103_render.fill = 0;
    spa103_render.primed = 0;
    spa103_render.primeTargetFrames = 0;
    spa103_render.state = SPA_103_RENDER_IDLE;
    pthread_mutex_unlock(&spa103_render.lock);
    LOG_INFO(LOG_MODULE,
             "buffer103: render stopped finalFormatIndex=%lld formatAnnouncements=%llu "
             "formatSwitches=%llu soundCheck=%llu/%llu fallback=%llu limited=%llu "
             "queued=%u/%uB flushDropped=%llu queueAllocFail=%llu",
             (long long)finalFormatIndex, (unsigned long long)formatAnnouncements,
             (unsigned long long)formatSwitches,
             (unsigned long long)soundCheckUpdates,
             (unsigned long long)soundCheckRejected,
             (unsigned long long)fallbackUpdates,
             (unsigned long long)limitedFrames,
             (unsigned)compressedPackets, (unsigned)compressedBytes,
             (unsigned long long)flushDropped,
             (unsigned long long)queueAllocationFailures);
    STATE_TRACE("BUFFERED_RENDER_STOP",
                "final_index=%lld format_announcements=%llu format_switches=%llu "
                "frames_in=%llu frames_out=%llu dropped=%llu auth_fail=%llu "
                "tlv_fail=%llu decode_calls=%llu decode_fail=%llu zero_pcm=%llu "
                "state_only=%llu sequence_gaps=%llu upmix_fail=%llu "
                "spf_mismatch=%llu transitions=%llu source_formats=%llu "
                "tracers=%llu unknown_tlvs=%llu soundcheck_updates=%llu "
                "soundcheck_rejected=%llu legacy_soundcheck_records=%llu "
                "fallback_updates=%llu limited_frames=%llu "
                "queued_packets=%u queued_bytes=%u flush_dropped=%llu "
                "queue_alloc_fail=%llu",
                (long long)finalFormatIndex,
                (unsigned long long)formatAnnouncements,
                (unsigned long long)formatSwitches,
                (unsigned long long)framesIn,
                (unsigned long long)framesOut,
                (unsigned long long)dropped,
                (unsigned long long)authFailed,
                (unsigned long long)tlvFailed,
                (unsigned long long)decodeCalls,
                (unsigned long long)decodeFailed,
                (unsigned long long)zeroFrameDecodes,
                (unsigned long long)stateOnlyPackets,
                (unsigned long long)sequenceGaps,
                (unsigned long long)upmixFailed,
                (unsigned long long)spfMismatches,
                (unsigned long long)transitions,
                (unsigned long long)sourceFormats,
                (unsigned long long)tracers,
                (unsigned long long)unknownFields,
                (unsigned long long)soundCheckUpdates,
                (unsigned long long)soundCheckRejected,
                (unsigned long long)legacySoundCheck,
                (unsigned long long)fallbackUpdates,
                (unsigned long long)limitedFrames,
                (unsigned)compressedPackets, (unsigned)compressedBytes,
                (unsigned long long)flushDropped,
                (unsigned long long)queueAllocationFailures);
}

static uint32_t spa103_apap_sequence(const uint8_t *packet);

/* Keep anomaly traffic bounded even if a damaged stream fails continuously:
 * the first occurrence and powers of two preserve onset and growth without
 * turning the compact trace into another high-volume audio log. */
static int spa103_trace_milestone(uint64_t count)
{
    return count != 0 && (count == 1 || (count & (count - 1u)) == 0);
}

static void spa103_trace_packet_anomaly(const char *stage, uint64_t count,
                                        int status)
{
    (void)stage;
    (void)status;
    if (!spa103_trace_milestone(count)) return;
    STATE_TRACE("BUFFERED_PACKET_ANOMALY",
                "stage=%s count=%llu status=%d",
                stage, (unsigned long long)count, status);
}

/* Sequence is in the authenticated APAP header.  TCP cannot legitimately lose
 * one complete record, so a discontinuity names a framing/sender boundary,
 * not a QSA problem.  FLUSHBUFFERED clears the baseline below because it may
 * intentionally move to a later inclusive boundary. */
static void spa103_note_authenticated_sequence(const uint8_t *packet)
{
    uint32_t sequence = spa103_apap_sequence(packet);
    uint32_t expected = 0;
    uint64_t gaps = 0;

    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.haveLastSequence) {
        expected = (spa103_render.lastSequence + 1u) & 0x00ffffffu;
        if (sequence != expected)
            gaps = ++spa103_render.sequenceGaps;
    }
    spa103_render.lastSequence = sequence;
    spa103_render.haveLastSequence = 1;
    pthread_mutex_unlock(&spa103_render.lock);

    (void)expected;
    if (gaps && spa103_trace_milestone(gaps))
        STATE_TRACE("BUFFERED_SEQUENCE_GAP",
                    "expected=%u actual=%u count=%llu",
                    (unsigned)expected, (unsigned)sequence,
                    (unsigned long long)gaps);
}

#if ENABLE_STATE_TRACE
typedef struct {
    int state;
    int playing;
    int primed;
    size_t primeTargetFrames;
    size_t fill;
    size_t compressedPackets;
    size_t compressedBytes;
    size_t compressedLimit;
    int flushPending;
    uint64_t flushDropped;
    uint64_t framesIn;
    uint64_t framesOut;
    uint64_t dropped;
    uint64_t authFailed;
    uint64_t tlvFailed;
    uint64_t decodeCalls;
    uint64_t decodeFailed;
    uint64_t zeroFrameDecodes;
    uint64_t stateOnlyPackets;
    uint64_t sequenceGaps;
    uint64_t upmixFailed;
} spa103_health_snapshot_t;

static void spa103_health_snapshot(spa103_health_snapshot_t *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    pthread_mutex_lock(&spa103_render.lock);
    snapshot->state = spa103_render.state;
    snapshot->playing = spa_apap_lifecycle_playing(&spa103_render.lifecycle);
    snapshot->primed = spa103_render.primed;
    snapshot->primeTargetFrames = spa103_render.primeTargetFrames;
    snapshot->fill = spa103_render.fill;
    snapshot->compressedPackets = spa103_render.lifecycle.queued_packets;
    snapshot->compressedBytes = spa103_render.lifecycle.queued_bytes;
    snapshot->compressedLimit = spa103_render.lifecycle.byte_limit;
    snapshot->flushPending = spa103_render.lifecycle.flush_pending;
    snapshot->flushDropped = spa103_render.lifecycle.total_flush_dropped;
    snapshot->framesIn = spa103_render.framesIn;
    snapshot->framesOut = spa103_render.framesOut;
    snapshot->dropped = spa103_render.dropped;
    snapshot->authFailed = spa103_render.authFailed;
    snapshot->tlvFailed = spa103_render.tlvFailed;
    snapshot->decodeCalls = spa103_render.decodeCalls;
    snapshot->decodeFailed = spa103_render.decodeFailed;
    snapshot->zeroFrameDecodes = spa103_render.zeroFrameDecodes;
    snapshot->stateOnlyPackets = spa103_render.stateOnlyPackets;
    snapshot->sequenceGaps = spa103_render.sequenceGaps;
    snapshot->upmixFailed = spa103_render.upmixFailed;
    pthread_mutex_unlock(&spa103_render.lock);
}

static void spa103_trace_buffered_health(uint64_t apapFrames,
                                         uint64_t intervalMaxGapNs,
                                         uint64_t frameGaps)
{
    spa103_health_snapshot_t health;

    spa103_health_snapshot(&health);
    STATE_TRACE("BUFFERED_HEALTH",
                "apap_frames=%llu interval_max_gap_ms=%llu frame_gaps=%llu "
                "state=%s playing=%d primed=%d prime_target=%u "
                "ring_fill=%u ring_capacity=%u compressed_packets=%u "
                "compressed_bytes=%u compressed_limit=%u flush_pending=%d "
                "flush_dropped=%llu "
                "frames_in=%llu frames_out=%llu dropped=%llu auth_fail=%llu "
                "tlv_fail=%llu decode_calls=%llu decode_fail=%llu zero_pcm=%llu "
                "state_only=%llu sequence_gaps=%llu upmix_fail=%llu",
                (unsigned long long)apapFrames,
                (unsigned long long)(intervalMaxGapNs / UINT64_C(1000000)),
                (unsigned long long)frameGaps,
                spa103_render_state_name(health.state),
                health.playing, health.primed,
                (unsigned)health.primeTargetFrames,
                (unsigned)health.fill, spa103_ring_frames(),
                (unsigned)health.compressedPackets,
                (unsigned)health.compressedBytes,
                (unsigned)health.compressedLimit, health.flushPending,
                (unsigned long long)health.flushDropped,
                (unsigned long long)health.framesIn,
                (unsigned long long)health.framesOut,
                (unsigned long long)health.dropped,
                (unsigned long long)health.authFailed,
                (unsigned long long)health.tlvFailed,
                (unsigned long long)health.decodeCalls,
                (unsigned long long)health.decodeFailed,
                (unsigned long long)health.zeroFrameDecodes,
                (unsigned long long)health.stateOnlyPackets,
                (unsigned long long)health.sequenceGaps,
                (unsigned long long)health.upmixFailed);
}

static void spa103_trace_apap_gap(uint64_t apapFrame, uint64_t gapNs,
                                  uint64_t gapCount)
{
    spa103_health_snapshot_t health;

    spa103_health_snapshot(&health);
    STATE_TRACE("APAP_FRAME_GAP",
                "apap_frame=%llu gap_ms=%llu count=%llu ring_fill_before=%u "
                "playing=%d primed=%d prime_target=%u compressed_packets=%u "
                "compressed_bytes=%u frames_in=%llu frames_out=%llu "
                "decode_fail=%llu zero_pcm=%llu",
                (unsigned long long)apapFrame,
                (unsigned long long)(gapNs / UINT64_C(1000000)),
                (unsigned long long)gapCount, (unsigned)health.fill,
                health.playing, health.primed,
                (unsigned)health.primeTargetFrames,
                (unsigned)health.compressedPackets,
                (unsigned)health.compressedBytes,
                (unsigned long long)health.framesIn,
                (unsigned long long)health.framesOut,
                (unsigned long long)health.decodeFailed,
                (unsigned long long)health.zeroFrameDecodes);
}

static void spa103_trace_qsa_ring_wait(uint64_t gapNs, size_t pending,
                                       uint64_t waitCount)
{
    spa103_health_snapshot_t health;

    spa103_health_snapshot(&health);
    STATE_TRACE("QSA_RING_WAIT",
                "owner=detached gap_ms=%llu pending=%u wait_count=%llu "
                "playing=%d primed=%d ring_fill=%u frames_in=%llu "
                "frames_out=%llu auth_fail=%llu tlv_fail=%llu decode_fail=%llu "
                "zero_pcm=%llu state_only=%llu",
                (unsigned long long)(gapNs / UINT64_C(1000000)),
                (unsigned)pending, (unsigned long long)waitCount,
                health.playing, health.primed, (unsigned)health.fill,
                (unsigned long long)health.framesIn,
                (unsigned long long)health.framesOut,
                (unsigned long long)health.authFailed,
                (unsigned long long)health.tlvFailed,
                (unsigned long long)health.decodeFailed,
                (unsigned long long)health.zeroFrameDecodes,
                (unsigned long long)health.stateOnlyPackets);
}

static void spa103_trace_qsa_gap(uint64_t gapNs, uint64_t writes,
                                 uint64_t clockResets, int autoRecovery)
{
    spa103_health_snapshot_t health;

    spa103_health_snapshot(&health);
    STATE_TRACE("QSA_CADENCE_GAP",
                "owner=detached gap_ms=%llu writes=%llu clock_resets=%llu "
                "auto_recovery=%d playing=%d primed=%d ring_fill=%u "
                "frames_in=%llu frames_out=%llu decode_fail=%llu zero_pcm=%llu",
                (unsigned long long)(gapNs / UINT64_C(1000000)),
                (unsigned long long)writes,
                (unsigned long long)clockResets, autoRecovery,
                health.playing, health.primed, (unsigned)health.fill,
                (unsigned long long)health.framesIn,
                (unsigned long long)health.framesOut,
                (unsigned long long)health.decodeFailed,
                (unsigned long long)health.zeroFrameDecodes);
}

static void spa103_trace_qsa_health(const char *owner, uint64_t writes,
                                    uint64_t zeroWrites, uint64_t shortWrites,
                                    uint64_t recoveries, int deviceStatus,
                                    uint64_t deviceUnderruns,
                                    uint64_t oddSizeWrites,
                                    uint64_t ringWaits,
                                    uint64_t clockResets,
                                    uint64_t ringFallbacks,
                                    uint64_t softwareUnderrunFrames)
{
    spa103_health_snapshot_t health;

    spa103_health_snapshot(&health);
    STATE_TRACE("QSA_HEALTH",
                "owner=%s writes=%llu zero=%llu short=%llu recoveries=%llu "
                "status=%d device_underruns=%llu odd_periods=%llu "
                "ring_waits=%llu clock_resets=%llu ring_fallbacks=%llu "
                "software_underrun_frames=%llu ring_fill=%u "
                "frames_in=%llu frames_out=%llu decode_fail=%llu zero_pcm=%llu",
                owner, (unsigned long long)writes,
                (unsigned long long)zeroWrites,
                (unsigned long long)shortWrites,
                (unsigned long long)recoveries, deviceStatus,
                (unsigned long long)deviceUnderruns,
                (unsigned long long)oddSizeWrites,
                (unsigned long long)ringWaits,
                (unsigned long long)clockResets,
                (unsigned long long)ringFallbacks,
                (unsigned long long)softwareUnderrunFrames,
                (unsigned)health.fill,
                (unsigned long long)health.framesIn,
                (unsigned long long)health.framesOut,
                (unsigned long long)health.decodeFailed,
                (unsigned long long)health.zeroFrameDecodes);
}
#endif

static int64_t spa103_apap_media_value(const uint8_t *packet)
{
    uint64_t value = ((uint64_t)packet[0] << 56) | ((uint64_t)packet[1] << 48) |
                     ((uint64_t)packet[2] << 40) | ((uint64_t)packet[3] << 32) |
                     ((uint64_t)packet[4] << 24) | ((uint64_t)packet[5] << 16) |
                     ((uint64_t)packet[6] << 8) | (uint64_t)packet[7];
    return (int64_t)value;
}

static int32_t spa103_apap_media_scale(const uint8_t *packet)
{
    uint32_t value = ((uint32_t)packet[8] << 24) | ((uint32_t)packet[9] << 16) |
                     ((uint32_t)packet[10] << 8) | (uint32_t)packet[11];
    return (int32_t)value;
}

static uint32_t spa103_apap_sequence(const uint8_t *packet)
{
    return ((uint32_t)packet[12] << 16) | ((uint32_t)packet[13] << 8) |
           (uint32_t)packet[14];
}

static void spa103_render_set_rate(int64_t rate)
{
    int previous;
    size_t packets, bytes;

    pthread_mutex_lock(&spa103_render.lock);
    previous = spa_apap_lifecycle_playing(&spa103_render.lifecycle);
    if (previous && rate != 1)
        (void)spa_stereo_upmix_reset(&spa103_render.stereoUpmix);
    spa_apap_lifecycle_set_rate(&spa103_render.lifecycle, rate);
    packets = spa103_render.lifecycle.queued_packets;
    bytes = spa103_render.lifecycle.queued_bytes;
    pthread_mutex_unlock(&spa103_render.lock);
    (void)packets;
    (void)bytes;
    STATE_TRACE("BUFFERED_RATE_GATE",
                "old_playing=%d new_playing=%d queued_packets=%u queued_bytes=%u",
                previous, rate == 1, (unsigned)packets, (unsigned)bytes);
}

/* Remove the exact FLUSH range from decoded PCM without destroying later
 * frames which Pioneer keeps.  Source tags are copied with every decoder
 * output frame, so the compressed and decoded queues use one boundary rule. */
static size_t spa103_ring_prune_locked(const spa_apap_flush_range_t *range)
{
    size_t offset, kept = 0;
    size_t oldFill = spa103_render.fill;
    const size_t capacity = spa103_ring_frames();
    const unsigned channels = spa103_render.outputChannels;

    if (!spa103_render.ring || !spa103_render.ringSequence ||
        !spa103_render.ringRTP || !oldFill || !channels)
        return 0;
    for (offset = 0; offset < oldFill; offset++) {
        size_t source = (spa103_render.head + offset) % capacity;
        size_t destination;

        if (spa_apap_flush_range_contains(
                range, spa103_render.ringSequence[source],
                spa103_render.ringRTP[source]))
            continue;
        destination = (spa103_render.head + kept) % capacity;
        if (destination != source) {
            memmove(spa103_render.ring + destination * channels,
                    spa103_render.ring + source * channels,
                    channels * sizeof(int16_t));
            spa103_render.ringSequence[destination] =
                spa103_render.ringSequence[source];
            spa103_render.ringRTP[destination] = spa103_render.ringRTP[source];
        }
        kept++;
    }
    spa103_render.fill = kept;
    return oldFill - kept;
}

/* Defined beside the QSA ownership state below.  A prefix FLUSH is a new
 * media timeline, so pruning the software queues is not enough: periods which
 * were already accepted by QSA must be discarded as well. */
static void spa_pcm_sink_flush_buffered_timeline(void);

/* Pioneer treats FLUSHBUFFERED as an inclusive range over both its compressed
 * packet queue and decoded output.  An until-only request is a prefix reset;
 * a bounded from..until request prunes just that interval and retains decoder
 * continuity outside it. */
static void spa103_render_flush(const spa_apap_flush_range_t *range)
{
    size_t removedPackets = 0, removedBytes = 0, removedFrames = 0;
    size_t queuedPackets = 0, queuedBytes = 0;
    int prefixReset;

    if (!range) return;
    prefixReset = !range->have_from_sequence && !range->have_from_rtp;
    pthread_mutex_lock(&spa103_render.lock);
    spa_apap_lifecycle_begin_flush(&spa103_render.lifecycle, range,
                                   &removedPackets, &removedBytes);
    removedFrames = spa103_ring_prune_locked(range);
    /* The next authenticated packet reasserts its item loudness. Preserve the
     * audible gain itself so a seek cannot jump to unity between packets. */
    spa_soundcheck_discontinuity(&spa103_render.soundCheck);
    spa103_render.haveExtensionMediaKind = 0;
    spa103_render.haveExtensionLoudness = 0;
    if (prefixReset) {
        if (spa103_render.decoder && spa103_render.codec)
            spa103_render.codec->reset(spa103_render.decoder);
        (void)spa_stereo_upmix_reset(&spa103_render.stereoUpmix);
        spa103_render.haveLastSequence = 0;
    }
    spa103_render.primed = 0;
    spa103_render.primeTargetFrames = spa103_reprime_frames();
    queuedPackets = spa103_render.lifecycle.queued_packets;
    queuedBytes = spa103_render.lifecycle.queued_bytes;
    pthread_mutex_unlock(&spa103_render.lock);
    /* Keep the global lock order ioLock -> sink.lock -> render.lock.  In
     * particular, never enter QSA while holding render.lock: a concurrent
     * stock/detached write may already own ioLock and be reading this ring. */
    if (prefixReset) spa_pcm_sink_flush_buffered_timeline();
    LOG_WARN(LOG_MODULE,
             "buffer103: flush %s removed compressed=%u/%uB PCM=%u; "
             "queued=%u/%uB restart after %u frames (%u ms)",
             prefixReset ? "prefix-reset" : "bounded",
             (unsigned)removedPackets, (unsigned)removedBytes,
             (unsigned)removedFrames, (unsigned)queuedPackets,
             (unsigned)queuedBytes,
             spa103_reprime_frames(),
             spa103_reprime_frames() * 1000u / SPA_SR);
    STATE_TRACE("BUFFERED_FLUSH_APPLIED",
                "kind=%s removed_packets=%u removed_bytes=%u "
                "removed_pcm_frames=%u queued_packets=%u queued_bytes=%u",
                prefixReset ? "prefix" : "bounded",
                (unsigned)removedPackets, (unsigned)removedBytes,
                (unsigned)removedFrames, (unsigned)queuedPackets,
                (unsigned)queuedBytes);
}

/* Called only after the packet authenticated, so an unauthenticated header cannot move the flush
 * window. Returns 1 while the packet is inside the inclusive FLUSH range. */
static int spa103_render_drop_for_flush(const uint8_t *packet)
{
    uint32_t sequence = spa103_apap_sequence(packet);
    uint32_t rtp = (uint32_t)spa103_media_time_to_rtp(
        spa103_apap_media_value(packet), spa103_apap_media_scale(packet));
    uint64_t discarded = 0;
    int drop, crossed = 0;

    pthread_mutex_lock(&spa103_render.lock);
    drop = spa_apap_lifecycle_observe(&spa103_render.lifecycle,
                                      sequence, rtp, &crossed);
    discarded = spa103_render.lifecycle.current_flush_dropped;
    pthread_mutex_unlock(&spa103_render.lock);

    if (crossed)
        LOG_INFO(LOG_MODULE,
                 "buffer103: flush boundary crossed at seq=%u rtp=%u discarded=%llu",
                 (unsigned)sequence, (unsigned)rtp, (unsigned long long)discarded);
    if (crossed)
        STATE_TRACE("BUFFERED_FLUSH_BOUNDARY",
                    "sequence=%u rtp=%u discarded=%llu",
                    (unsigned)sequence, (unsigned)rtp,
                    (unsigned long long)discarded);
    return drop;
}

static int spa103_render_flush_pending(void)
{
    int pending;

    pthread_mutex_lock(&spa103_render.lock);
    pending = spa103_render.lifecycle.flush_pending;
    pthread_mutex_unlock(&spa103_render.lock);
    return pending;
}

/* Copy of the magic cookie for `id`, or the only stored cookie when the stream never named one.
 * The caller owns the returned block. */
static uint8_t *spa103_copy_cookie(int haveId, int64_t id, size_t *sizeOut)
{
    uint8_t *copy = 0;
    size_t i;

    pthread_mutex_lock(&spa103_lock);
    for (i = 0; i < spa103_receiver.magicCookieCount; i++) {
        const spa103_magic_cookie_t *slot = &spa103_receiver.magicCookies[i];
        if (haveId && slot->id != id) continue;
        if (!haveId && spa103_receiver.magicCookieCount != 1) break;
        if (!slot->bytes || !slot->size) break;
        copy = (uint8_t *)malloc(slot->size);
        if (copy) {
            memcpy(copy, slot->bytes, slot->size);
            *sizeOut = slot->size;
        }
        break;
    }
    pthread_mutex_unlock(&spa103_lock);
    return copy;
}

/* Push one decoded packet, already in the DECLARED channel order, into the ring.  Overwrites the
 * oldest frames when nothing is draining; that is preferable to unbounded growth and to blocking
 * the socket reader, and it is counted. */
static void spa103_ring_push_locked(const int16_t *pcm, size_t frames,
                                    uint32_t sequence, uint32_t rtp)
{
    size_t at = 0;

    if (!spa103_render.ring || !spa103_render.ringSequence ||
        !spa103_render.ringRTP || !frames)
        return;
    if (frames > spa103_ring_frames()) {
        pcm += (frames - spa103_ring_frames()) * spa103_render.outputChannels;
        spa103_render.dropped += frames - spa103_ring_frames();
        frames = spa103_ring_frames();
    }
    while (at < frames) {
        const size_t tail = (spa103_render.head + spa103_render.fill) % spa103_ring_frames();
        size_t run = spa103_ring_frames() - tail;
        if (run > frames - at) run = frames - at;
        memcpy(spa103_render.ring + tail * spa103_render.outputChannels,
               pcm + at * spa103_render.outputChannels,
               run * spa103_render.outputChannels * sizeof(int16_t));
        {
            size_t tag;
            for (tag = 0; tag < run; tag++) {
                spa103_render.ringSequence[tail + tag] =
                    sequence & UINT32_C(0x00ffffff);
                spa103_render.ringRTP[tail + tag] = rtp;
            }
        }
        if (spa103_render.fill + run > spa103_ring_frames()) {
            const size_t lost = spa103_render.fill + run - spa103_ring_frames();
            spa103_render.head = (spa103_render.head + lost) % spa103_ring_frames();
            spa103_render.fill = spa103_ring_frames();
            spa103_render.dropped += lost;
        } else {
            spa103_render.fill += run;
        }
        at += run;
    }
}

/* Bounded, privacy-preserving PCM telemetry.  Mean absolute level, peak and zero-crossing rate are
 * enough to spot a misplaced LFE channel or a decoder that has shifted the spectrum, without
 * retaining any playable audio.  Called with spa103_render.lock held. */
static void spa103_pcm_stats_locked(const int16_t *pcm, size_t frames)
{
    size_t frame;
    unsigned channels = spa103_render.outputChannels;
    unsigned channel;

    if (!pcm || !frames || !channels || channels > SPA_103_RENDER_MAX_CHANNELS) return;
    for (frame = 0; frame < frames; frame++) {
        for (channel = 0; channel < channels; channel++) {
            int16_t sample = pcm[frame * channels + channel];
            uint32_t magnitude = sample < 0 ? (uint32_t)(-(int32_t)sample) :
                                              (uint32_t)sample;
            spa103_render.statsAbs[channel] += magnitude;
            if (magnitude > spa103_render.statsPeak[channel])
                spa103_render.statsPeak[channel] = magnitude;
            if (spa103_render.statsHavePrevious &&
                ((sample < 0) != (spa103_render.statsPrevious[channel] < 0)))
                spa103_render.statsZeroCrossings[channel]++;
            spa103_render.statsPrevious[channel] = sample;
        }
        spa103_render.statsHavePrevious = 1;
    }
    spa103_render.statsFrames += frames;

    if (spa103_render.statsFrames >= 192u * SPA_103_RENDER_FRAMES && channels >= 6) {
        uint64_t count = spa103_render.statsFrames;
        uint32_t mean[6], zcHz[6];
        for (channel = 0; channel < 6; channel++) {
            mean[channel] = (uint32_t)(spa103_render.statsAbs[channel] / count);
            zcHz[channel] = (uint32_t)((spa103_render.statsZeroCrossings[channel] * SPA_SR) /
                                       (2u * count));
        }
        LOG_INFO(LOG_MODULE,
                 "buffer103: pcm stats frames=%llu fill=%u in=%llu out=%llu drop=%llu "
                 "meanAbs=[%u,%u,%u,%u,%u,%u] peak=[%u,%u,%u,%u,%u,%u] "
                 "zcHz=[%u,%u,%u,%u,%u,%u]",
                 (unsigned long long)count, (unsigned)spa103_render.fill,
                 (unsigned long long)spa103_render.framesIn,
                 (unsigned long long)spa103_render.framesOut,
                 (unsigned long long)spa103_render.dropped,
                 mean[0], mean[1], mean[2], mean[3], mean[4], mean[5],
                 spa103_render.statsPeak[0], spa103_render.statsPeak[1],
                 spa103_render.statsPeak[2], spa103_render.statsPeak[3],
                 spa103_render.statsPeak[4], spa103_render.statsPeak[5],
                 zcHz[0], zcHz[1], zcHz[2], zcHz[3], zcHz[4], zcHz[5]);
        spa103_render.statsFrames = 0;
        memset(spa103_render.statsAbs, 0, sizeof(spa103_render.statsAbs));
        memset(spa103_render.statsZeroCrossings, 0,
               sizeof(spa103_render.statsZeroCrossings));
        memset(spa103_render.statsPeak, 0, sizeof(spa103_render.statsPeak));
    }
}

/* One whole APAP packet (the bytes after the BE32 length prefix).  Returns nothing: a frame that
 * cannot be decoded is counted and dropped, never fatal to the receiver. */
/* Which decoder serves an index.  APAC comes from the optional library that was dlopen'd at
 * stream start; AAC-LC is compiled in.  Returning 0 means we cannot play that index. */
/* The two codecs the buffered path can hold.  A cookie names its format only implicitly, so
 * when one codec rejects it the other is tried: an AAC cookie is an MPEG-4 ES_Descriptor and an
 * APAC one is not, and each create() already validates what it is handed. */
static const spa_apac_codec_api_t *spa103_alternate_codec(const spa_apac_codec_api_t *current)
{
    extern const spa_apac_codec_api_t *spa_aac_codec_api(void);
    const spa_apac_codec_api_t *aac = spa_aac_codec_api();
    if (current == spa103_render.apacCodec) return aac;
    if (current == aac)                     return spa103_render.apacCodec;
    return 0;
}

static const spa_apac_codec_api_t *spa103_codec_for_index(int64_t index)
{
    extern const spa_apac_codec_api_t *spa_aac_codec_api(void);
    if (index == SPA_AAC_LC_44K_STEREO || index == SPA_AAC_LC_48K_STEREO)
        return spa_aac_codec_api();
    if (index == SPA_APAC_48K_STEREO || index == SPA_APAC_48K_5_1 ||
        index == SPA_APAC_48K_5_1_2)
        return spa103_render.apacCodec;
    return 0;
}

static int64_t spa103_verified_index(const spa_apac_codec_api_t *codec,
                                     uint32_t channels, uint32_t sampleRate)
{
    if (codec == spa103_render.apacCodec && sampleRate == SPA_SR) {
        if (channels == 2) return SPA_APAC_48K_STEREO;
        if (channels == 6) return SPA_APAC_48K_5_1;
        if (channels == 8) return SPA_APAC_48K_5_1_2;
    } else if (codec && sampleRate == SPA_SR && channels == 2) {
        return SPA_AAC_LC_48K_STEREO;
    } else if (codec && sampleRate == 44100 && channels == 2) {
        return SPA_AAC_LC_44K_STEREO;
    }
    return -1;
}

/* Plain geometry adapter used by the OFF path and as the audible fail-safe if
 * the optional ambience renderer ever rejects a block.  Mono is duplicated to
 * the two fronts; all other absent channels are digital zero. */
static void spa103_widen_s16(const int16_t *input, unsigned inputChannels,
                             int16_t *output, unsigned outputChannels,
                             size_t frames)
{
    size_t frame;

    for (frame = 0; frame < frames; frame++) {
        const int16_t *src = input + frame * inputChannels;
        int16_t *dst = output + frame * outputChannels;
        unsigned channel;

        for (channel = 0; channel < outputChannels; channel++) {
            if (channel < inputChannels)
                dst[channel] = src[channel];
            else if (inputChannels == 1 && channel == 1)
                dst[channel] = src[0];
            else
                dst[channel] = 0;
        }
    }
}

/* APAC index 65 is decoded as C,L,R,Ls,Rs,LFE.  Normalise it directly into
 * the ring stride so the rare 8-wide passthrough session can also switch back
 * to 65 without exposing decoder order in its first six positions. */
static void spa103_reorder_5_1_s16(const int16_t *input, int16_t *output,
                                   unsigned outputChannels, size_t frames)
{
    static const unsigned kFromDeclared[SPA_PCM_5_1_CHANNELS] = {
        1, 2, 0, 5, 3, 4
    };
    size_t frame;

    for (frame = 0; frame < frames; frame++) {
        const int16_t *src = input + frame * SPA_PCM_5_1_CHANNELS;
        int16_t *dst = output + frame * outputChannels;
        unsigned channel;

        for (channel = 0; channel < outputChannels; channel++)
            dst[channel] = channel < SPA_PCM_5_1_CHANNELS ?
                           src[kFromDeclared[channel]] : 0;
    }
}

enum spa103_render_frame_result {
    SPA103_RENDER_FRAME_CONSUMED = 0,
    SPA103_RENDER_FRAME_RETRY = 1
};

static uint32_t spa103_cookie_generation_snapshot(void)
{
    uint32_t generation;

    pthread_mutex_lock(&spa103_lock);
    generation = spa103_receiver.cookieGeneration;
    pthread_mutex_unlock(&spa103_lock);
    return generation;
}

/* Apply metadata state before deciding whether the packet also carries audio.
 * State-only transition packets therefore remain observable without inventing
 * a decoder reset (the Pioneer build also treats field 10 as informational). */
static void spa103_observe_extension_locked(const spa_apap_extension_t *ext)
{
    if (!ext) return;
    if (ext->hasFormatIndex) spa103_render.formatAnnouncements++;
    if (ext->hasSamplesPerFrame &&
        ext->samplesPerFrame != SPA_103_RENDER_FRAMES)
        spa103_render.samplesPerFrameMismatches++;
    if (ext->hasTransitionId) {
        if (!spa103_render.haveLastTransitionId ||
            spa103_render.lastTransitionId != ext->transitionId) {
            spa103_render.transitionAnnouncements++;
            spa103_render.lastTransitionId = ext->transitionId;
            spa103_render.haveLastTransitionId = 1;
            STATE_TRACE("BUFFERED_TRANSITION",
                        "transition_id=%llu",
                        (unsigned long long)ext->transitionId);
        }
    }
    if (ext->hasSourceFormatIndex) {
        if (!spa103_render.haveLastSourceFormatIndex ||
            spa103_render.lastSourceFormatIndex != ext->sourceFormatIndex) {
            spa103_render.sourceFormatAnnouncements++;
            spa103_render.lastSourceFormatIndex = ext->sourceFormatIndex;
            spa103_render.haveLastSourceFormatIndex = 1;
            STATE_TRACE("BUFFERED_SOURCE_FORMAT",
                        "source_index=%llu transport_index=%lld",
                        (unsigned long long)ext->sourceFormatIndex,
                        (long long)spa103_render.formatIndex);
        }
    }
    if (ext->hasSbufTracer) spa103_render.tracerAnnouncements++;
    if (ext->hasSbufTracerExtended) spa103_render.tracerAnnouncements++;
    spa103_render.unknownExtensionFields += ext->unknownFieldCount;

    if (ext->soundCheckRecordCount)
        spa_soundcheck_note_legacy(&spa103_render.soundCheck,
                                   ext->soundCheckRecordCount);
    if (ext->hasMediaKind) {
        spa103_render.extensionMediaKind = ext->mediaKind;
        spa103_render.haveExtensionMediaKind = 1;
    }
    if (ext->hasLoudnessParameters) {
        memcpy(spa103_render.extensionLoudness, ext->loudnessParameters,
               sizeof(spa103_render.extensionLoudness));
        spa103_render.haveExtensionLoudness = 1;
    }
    if (spa103_render.haveExtensionMediaKind &&
        spa103_render.haveExtensionLoudness &&
        spa_soundcheck_update_main(&spa103_render.soundCheck,
                                   spa103_render.extensionMediaKind,
                                   spa103_render.extensionLoudness)) {
        STATE_TRACE("BUFFERED_SOUNDCHECK",
                    "kind=%u target_db=%d gain_milli=%u updates=%llu",
                    (unsigned)spa103_render.extensionMediaKind,
                    (int)spa103_render.soundCheck.setupTargetDb,
                    (unsigned)(spa103_render.soundCheck.targetGain * 1000.0f + 0.5f),
                    (unsigned long long)spa103_render.soundCheck.metadataUpdates);
    }
}

static int spa103_render_frame(const uint8_t *packet, size_t length,
                               int sequenceAlreadyNoted)
{
    static uint8_t plaintext[SPA_103_APAP_MAX_FRAME];
    static int16_t decoded[SPA_103_RENDER_MAX_SAMPLES];
    static int16_t rendered[SPA_103_RENDER_MAX_SAMPLES];
    uint8_t key[SPA_APAP_KEY_SIZE];
    spa_apap_extension_t ext;
    const uint8_t *audio = 0;
    size_t audioLength = 0, plainLength = 0;
    uint32_t frames = 0;
    uint32_t packetSequence = 0, packetRTP = 0;
    uint32_t cookieGeneration;
    uint64_t anomalyCount = 0;
    int variant = 0, rc, state;

    pthread_mutex_lock(&spa103_render.lock);
    state = spa103_render.state;
    pthread_mutex_unlock(&spa103_render.lock);
    if (state != SPA_103_RENDER_WAITING && state != SPA_103_RENDER_RUNNING)
        return SPA103_RENDER_FRAME_CONSUMED;

    pthread_mutex_lock(&spa103_lock);
    memcpy(key, spa103_receiver.shk, sizeof(key));
    pthread_mutex_unlock(&spa103_lock);

    if (!spa_apap_open(key, packet, length, plaintext, sizeof(plaintext),
                       &plainLength, &variant)) {
        pthread_mutex_lock(&spa103_render.lock);
        anomalyCount = ++spa103_render.authFailed;
        pthread_mutex_unlock(&spa103_render.lock);
        spa103_trace_packet_anomaly("auth", anomalyCount, SPA_APAC_ERR_DECODE);
        memset(key, 0, sizeof(key));
        return SPA103_RENDER_FRAME_CONSUMED;
    }
    memset(key, 0, sizeof(key));

    packetSequence = spa103_apap_sequence(packet);
    packetRTP = (uint32_t)spa103_media_time_to_rtp(
        spa103_apap_media_value(packet), spa103_apap_media_scale(packet));
    if (spa103_render_drop_for_flush(packet))
        return SPA103_RENDER_FRAME_CONSUMED;

    /* A flush may intentionally jump across thousands of queued sequence
     * numbers.  Discarded pre-boundary packets must not seed the continuity
     * baseline or the first valid post-seek record looks like transport loss. */
    if (!sequenceAlreadyNoted) spa103_note_authenticated_sequence(packet);

    memset(&ext, 0, sizeof(ext));
    if (variant == SPA_APAP_AAD_EXTENSION) {
        if (!spa_apap_parse_extension(plaintext, plainLength, &ext, &audio, &audioLength)) {
            pthread_mutex_lock(&spa103_render.lock);
            anomalyCount = ++spa103_render.tlvFailed;
            pthread_mutex_unlock(&spa103_render.lock);
            spa103_trace_packet_anomaly("extension", anomalyCount,
                                        SPA_APAC_ERR_DECODE);
            return SPA103_RENDER_FRAME_CONSUMED;
        }
    } else {
        audio = plaintext;
        audioLength = plainLength;
    }
    if (!audioLength) {
        pthread_mutex_lock(&spa103_render.lock);
        spa103_observe_extension_locked(&ext);
        spa103_render.stateOnlyPackets++;
        pthread_mutex_unlock(&spa103_render.lock);
        return SPA103_RENDER_FRAME_CONSUMED;
    }

    pthread_mutex_lock(&spa103_render.lock);
    state = spa_apap_lifecycle_playing(&spa103_render.lifecycle);
    pthread_mutex_unlock(&spa103_render.lock);
    if (!state)
        return SPA103_RENDER_FRAME_RETRY; /* authenticated AU remains receiver-owned */

    if (spa103_render.probeFraming) {
#if ENABLE_AUDIO_DIAGNOSTICS
        static int reported;
        /* Elementary-stream dump for offline decoder work: 4-byte little-endian length, then
         * that many bytes of raw access unit, repeated.  Only under an index we cannot decode,
         * so it never touches a working audio path, and bounded so a long session cannot fill
         * /dev/shmem.  This is decrypted audio on disk -- fine for debugging one's own unit,
         * but delete it when done and do not ship a build that writes it. */
        static FILE *es;
        static size_t esBytes;
        if (audio && audioLength && esBytes < SPA_103_ES_DUMP_LIMIT) {
            if (!es) {
                es = fopen(SPA_103_ES_DUMP_PATH, "wb");
                if (es) setvbuf(es, 0, _IOFBF, 32768);
            }
            if (es) {
                uint8_t len[4];
                len[0] = (uint8_t)(audioLength & 0xff);
                len[1] = (uint8_t)((audioLength >> 8) & 0xff);
                len[2] = (uint8_t)((audioLength >> 16) & 0xff);
                len[3] = (uint8_t)((audioLength >> 24) & 0xff);
                fwrite(len, 1, 4, es);
                fwrite(audio, 1, audioLength, es);
                esBytes += audioLength + 4;
                if (esBytes >= SPA_103_ES_DUMP_LIMIT) {
                    fflush(es);
                    LOG_WARN(LOG_MODULE,
                             "buffer103: elementary-stream dump full at %u bytes: %s",
                             (unsigned)esBytes, SPA_103_ES_DUMP_PATH);
                } else if ((esBytes & 0x3ffff) < (unsigned)audioLength) {
                    fflush(es);
                }
            }
        }
        if (!reported && audio && audioLength) {
            size_t n = audioLength < 24 ? audioLength : 24;
            char hex[3 * 24 + 1];
            size_t i;
            reported = 1;
            for (i = 0; i < n; i++) sprintf(hex + i * 3, "%02x ", audio[i]);
            hex[n * 3 - 1] = 0;
            LOG_WARN(LOG_MODULE,
                     "buffer103: framing probe, index %lld, access unit %u bytes, first: %s"
                     " -- ADTS starts ff f?, a raw AU does not; elementary stream -> %s",
                     (long long)spa103_render.formatIndex, (unsigned)audioLength, hex,
                     SPA_103_ES_DUMP_PATH);
        }
#endif
        return SPA103_RENDER_FRAME_CONSUMED;
    }

    /* Bring the decoder up on the first frame that names a cookie we hold.  Doing it here rather
     * than at SETUP is not laziness: SETMAGICCOOKIE arrives asynchronously on the RTSP control
     * connection and may land after the first APAP frame. */
    cookieGeneration = spa103_cookie_generation_snapshot();
    pthread_mutex_lock(&spa103_render.lock);
    if (!spa_apap_lifecycle_playing(&spa103_render.lifecycle)) {
        pthread_mutex_unlock(&spa103_render.lock);
        return SPA103_RENDER_FRAME_RETRY;
    }
    if (!spa103_render.decoder ||
        (ext.hasMagicCookieId && spa103_render.haveCookieId &&
         spa103_render.cookieId != (int64_t)ext.magicCookieId) ||
        spa103_render.cookieGeneration != cookieGeneration) {
        spa_soundcheck_discontinuity(&spa103_render.soundCheck);
        spa103_render.haveExtensionMediaKind = 0;
        spa103_render.haveExtensionLoudness = 0;
    }
    spa103_observe_extension_locked(&ext);
    /* An announcement we have no decoder for is never a reason to tear down one that is
     * working. TLV 2 is a real index, but the authenticated inline cookie remains the decoder
     * authority. Drop an unsupported hint and keep decoding. */
    if (ext.hasFormatIndex &&
        !spa103_codec_for_index((int64_t)ext.formatIndex)) {
        if (spa103_render.ignoredFormatIndex != (int64_t)ext.formatIndex) {
            spa103_render.ignoredFormatIndex = (int64_t)ext.formatIndex;
            LOG_WARN(LOG_MODULE,
                     "buffer103: ignoring unsupported format announcement %llu; staying on %lld",
                     (unsigned long long)ext.formatIndex,
                     (long long)spa103_render.formatIndex);
        }
        ext.hasFormatIndex = 0;
    }
    if (!spa103_render.decoder ||
        (ext.hasMagicCookieId && spa103_render.haveCookieId &&
         spa103_render.cookieId != (int64_t)ext.magicCookieId) ||
        spa103_render.cookieGeneration != cookieGeneration) {
        uint8_t *cookie = 0;
        size_t cookieSize = 0;
        spa_apac_stream_info_t info;
        enum spa_decoded_pcm_kind decodedPcmKind = SPA_DECODED_PCM_OTHER;
        uint32_t channels = 0, frameLength = 0, sampleRate = 0;
        int64_t verifiedIndex = -1;
        const char *codecName;

        /* A new decoder/cookie is a discontinuity even when its index happens
         * to stay the same.  Never let rear-filter history cross it. */
        (void)spa_stereo_upmix_reset(&spa103_render.stereoUpmix);
        spa103_render.decodedPcmKind = SPA_DECODED_PCM_OTHER;
        pthread_mutex_unlock(&spa103_render.lock);
        /* Every packet carries the cookie for the format it is in, so a track that changes
         * codec brings its own configuration and there is nothing to wait for.  Fall back to
         * the stored one only for a stream that does not repeat it. */
        if (ext.magicCookie && ext.magicCookieLength) {
            cookie = (uint8_t *)malloc(ext.magicCookieLength);
            if (cookie) {
                memcpy(cookie, ext.magicCookie, ext.magicCookieLength);
                cookieSize = ext.magicCookieLength;
            }
        }
        if (!cookie)
            cookie = spa103_copy_cookie(ext.hasMagicCookieId, (int64_t)ext.magicCookieId,
                                        &cookieSize);
        if (!cookie) return SPA103_RENDER_FRAME_RETRY;

        pthread_mutex_lock(&spa103_render.lock);
        if (!spa_apap_lifecycle_playing(&spa103_render.lifecycle)) {
            pthread_mutex_unlock(&spa103_render.lock);
            free(cookie);
            return SPA103_RENDER_FRAME_RETRY;
        }
        if (ext.hasFormatIndex && (int64_t)ext.formatIndex != spa103_render.formatIndex) {
            const spa_apac_codec_api_t *next = spa103_codec_for_index((int64_t)ext.formatIndex);
            const spa_apac_codec_api_t *previous = spa103_render.codec;
            LOG_WARN(LOG_MODULE,
                     "buffer103: stream switched format index %lld -> %llu; %s",
                     (long long)spa103_render.formatIndex,
                     (unsigned long long)ext.formatIndex,
                     next ? "rebuilding the decoder" : "no decoder for it, muting");
            STATE_TRACE("BUFFERED_FORMAT_SWITCH",
                        "old_index=%lld new_index=%llu decoder_available=%d",
                        (long long)spa103_render.formatIndex,
                        (unsigned long long)ext.formatIndex, next ? 1 : 0);
            /* The decoder handle belongs to the API which created it.  Destroy it before
             * publishing the next API: AAC and APAC intentionally share one opaque interface,
             * but their handle layouts and destructors are unrelated. */
            if (spa103_render.decoder && previous) {
                previous->destroy(spa103_render.decoder);
                spa103_render.decoder = 0;
            }
            spa103_render.formatSwitches++;
            spa103_render.formatIndex = (int64_t)ext.formatIndex;
            spa103_render.codec = next;
        }
        if (!spa103_render.codec) {
            pthread_mutex_unlock(&spa103_render.lock);
            free(cookie);
            return SPA103_RENDER_FRAME_CONSUMED;
        }
        if (spa103_render.decoder && spa103_render.codec) {
            spa103_render.codec->destroy(spa103_render.decoder);
            spa103_render.decoder = 0;
        }
        rc = spa103_render.codec->create(cookie, cookieSize, &spa103_render.decoder,
                                         &channels, &frameLength, &sampleRate);
        /* Apple changes codec inside one type-103 session -- stereo AAC-LC for ordinary tracks,
         * APAC for spatial ones. TLV 2 announces the transport index, but the inline cookie is
         * still authoritative: if the announced decoder rejects it, try the alternate and retain
         * only the API which accepted the cookie. */
        if (rc != SPA_APAC_OK) {
            const spa_apac_codec_api_t *alt = spa103_alternate_codec(spa103_render.codec);
            if (alt) {
                spa_apac_decoder_t *other = 0;
                uint32_t altChannels = 0, altFrameLength = 0, altSampleRate = 0;
                int altRc = alt->create(cookie, cookieSize, &other,
                                        &altChannels, &altFrameLength, &altSampleRate);
                if (altRc == SPA_APAC_OK) {
                    if (spa103_render.decoder)
                        spa103_render.codec->destroy(spa103_render.decoder);
                    spa103_render.decoder = other;
                    spa103_render.codec   = alt;
                    channels = altChannels; frameLength = altFrameLength; sampleRate = altSampleRate;
                    rc = SPA_APAC_OK;
                    spa103_render.formatSwitches++;
                }
            }
        }
        codecName = spa103_render.codec == spa103_render.apacCodec ? "APAC" : "AAC-LC";
        spa103_render.cookieGeneration = cookieGeneration;
        free(cookie);
        memset(&info, 0, sizeof(info));
        if (rc == SPA_APAC_OK)
            rc = spa103_render.codec->getInfo(spa103_render.decoder, &info);
        if (rc == SPA_APAC_OK)
            decodedPcmKind = spa_classify_decoded_pcm(
                spa103_render.codec == spa103_render.apacCodec,
                channels, sampleRate, info.channelLayoutTag);
        if (rc == SPA_APAC_OK)
            verifiedIndex = spa103_verified_index(spa103_render.codec,
                                                  channels, sampleRate);
        /* A track that is not natively spatial may decode to a narrower layout,
         * including AAC-LC stereo. The sender's preferred buffered list starts
         * there, so this is ordinary content, not an error. Refusing it destroyed
         * the decoder, so nothing played and the phone skipped the item; that is
         * why only natively-Atmos tracks worked. Take
         * anything from mono up to our output width and widen it below. */
        {
            int narrower = channels >= 1 && channels < SPA_PCM_5_1_CHANNELS;
            int nativeSpatial =
                decodedPcmKind == SPA_DECODED_PCM_APAC_5_1 ||
                (decodedPcmKind == SPA_DECODED_PCM_APAC_5_1_2 &&
                 spa103_render.formatIndex == SPA_APAC_48K_5_1_2);
            if (rc != SPA_APAC_OK ||
                frameLength != SPA_103_RENDER_FRAMES || sampleRate != SPA_SR ||
                (!narrower && !nativeSpatial)) {
            if (spa103_render.decoder) {
                spa103_render.codec->destroy(spa103_render.decoder);
                spa103_render.decoder = 0;
            }
            if (rc == SPA_APAC_OK)
                spa103_render.state = SPA_103_RENDER_UNSUPPORTED_LAYOUT;
            else
                spa103_render.state = SPA_103_RENDER_COOKIE_FAILED;
            pthread_mutex_unlock(&spa103_render.lock);
            LOG_ERROR(LOG_MODULE,
                      "buffer103: %s decoder setup rejected status=%d cookie=%u bytes "
                      "setupIndex=%lld geometry=%uHz/%uch/%u layout=%#x kind=%s; "
                      "capture continues",
                      codecName, rc, (unsigned)cookieSize,
                      (long long)spa103_render.formatIndex,
                      (unsigned)sampleRate, (unsigned)channels, (unsigned)frameLength,
                      (unsigned)info.channelLayoutTag,
                      spa103_decoded_pcm_kind_name(decodedPcmKind));
            STATE_TRACE("BUFFERED_DECODER_REJECT",
                        "codec=%s status=%d cookie_bytes=%u setup_index=%lld "
                        "rate=%u channels=%u frame_length=%u layout=0x%x kind=%s",
                        codecName, rc, (unsigned)cookieSize,
                        (long long)spa103_render.formatIndex,
                        (unsigned)sampleRate, (unsigned)channels,
                        (unsigned)frameLength, (unsigned)info.channelLayoutTag,
                        spa103_decoded_pcm_kind_name(decodedPcmKind));
            return SPA103_RENDER_FRAME_CONSUMED;
            }
        }
        if (verifiedIndex >= 0 && verifiedIndex != spa103_render.formatIndex) {
            LOG_WARN(LOG_MODULE,
                     "buffer103: cookie/geometry corrected active index %lld -> %lld",
                     (long long)spa103_render.formatIndex,
                     (long long)verifiedIndex);
            STATE_TRACE("BUFFERED_FORMAT_VERIFIED",
                        "announced_index=%lld verified_index=%lld",
                        (long long)spa103_render.formatIndex,
                        (long long)verifiedIndex);
            spa103_render.formatIndex = verifiedIndex;
            spa103_render.formatSwitches++;
        }
        spa103_render.decoderChannels = channels;
        spa103_render.cookieId = (int64_t)ext.magicCookieId;
        spa103_render.haveCookieId = ext.hasMagicCookieId;
        spa103_render.sampleRate = sampleRate;
        spa103_render.sourceChannels = channels;
        spa103_render.channelLayoutTag = info.channelLayoutTag;
        spa103_render.decodedPcmKind = decodedPcmKind;
        spa103_render.state = SPA_103_RENDER_RUNNING;
        LOG_INFO(LOG_MODULE,
                 "buffer103: %s decoder up setupIndex=%lld id=%lld %u Hz %u ch "
                 "%u frames/packet kind=%s%s",
                 codecName, (long long)spa103_render.formatIndex,
                 (long long)spa103_render.cookieId, (unsigned)sampleRate,
                 (unsigned)channels, (unsigned)frameLength,
                 spa103_decoded_pcm_kind_name(decodedPcmKind),
                 channels < spa103_render.outputChannels ? " -- widened to the sink width" : "");
        STATE_TRACE("BUFFERED_DECODER_READY",
                    "codec=%s setup_index=%lld cookie_id=%lld rate=%u "
                    "channels=%u frame_length=%u layout=0x%x kind=%s "
                    "output_channels=%u",
                    codecName, (long long)spa103_render.formatIndex,
                    (long long)spa103_render.cookieId, (unsigned)sampleRate,
                    (unsigned)channels, (unsigned)frameLength,
                    (unsigned)info.channelLayoutTag,
                    spa103_decoded_pcm_kind_name(decodedPcmKind),
                    (unsigned)spa103_render.outputChannels);
    }

    spa103_render.decodeCalls++;
    rc = spa103_render.codec->decodeS16(spa103_render.decoder, audio, audioLength,
                                        decoded, SPA_103_RENDER_MAX_SAMPLES, &frames);
    if (rc != SPA_APAC_OK) {
        anomalyCount = ++spa103_render.decodeFailed;
        pthread_mutex_unlock(&spa103_render.lock);
        spa103_trace_packet_anomaly("decode", anomalyCount, rc);
        return SPA103_RENDER_FRAME_CONSUMED;
    }
    if (!frames) {
        anomalyCount = ++spa103_render.zeroFrameDecodes;
        pthread_mutex_unlock(&spa103_render.lock);
        spa103_trace_packet_anomaly("zero-pcm", anomalyCount, rc);
        return SPA103_RENDER_FRAME_CONSUMED;
    }
    if (frames) {
        unsigned dch = spa103_render.decoderChannels;
        unsigned out = spa103_render.outputChannels;
        size_t sourceStart = 0, renderFrames = frames, at = 0;

        if (!dch || dch > SPA_103_RENDER_MAX_CHANNELS ||
            !out || out > SPA_103_RENDER_MAX_CHANNELS ||
            frames > SPA_103_RENDER_MAX_SAMPLES / dch) {
            anomalyCount = ++spa103_render.decodeFailed;
            pthread_mutex_unlock(&spa103_render.lock);
            spa103_trace_packet_anomaly("decode-output", anomalyCount,
                                        SPA_APAC_ERR_OUTPUT_SPACE);
            return SPA103_RENDER_FRAME_CONSUMED;
        }

        /* Observe source PCM before channel synthesis.  In particular, an
         * optional metadata-free fallback must measure the stereo programme,
         * not a six-wide buffer containing synthesized/zero channels. */
        spa_soundcheck_observe_s16(&spa103_render.soundCheck, decoded,
                                   frames, dch);

        /* APAP fields 4/5 are CMTime durations, not decoder commands.  Match
         * Pioneer: decode the complete compressed access unit first, then
         * remove whole interleaved PCM frames before reorder/upmix/ring.  In
         * particular, trimmed stereo never enters the upmixer's history. */
        spa_apap_trim_window(&ext, spa103_render.sampleRate, frames,
                             &sourceStart, &renderFrames);

        /* Cinemo may release several AAC callbacks for one access unit (up to
         * 4096 stereo frames).  The former backwards in-place widening then
         * wrote beyond decoded[8192].  Keep decoded as the source and render
         * bounded, chronological blocks into a separate PCM8-sized buffer. */
        while (at < renderFrames) {
            const int16_t *src = decoded + (sourceStart + at) * dch;
            enum spa_decoded_pcm_action action = spa_select_decoded_pcm_action(
                spa103_render.decodedPcmKind, dch, out,
                spa103_render.sampleRate);
            size_t run = renderFrames - at;

            if (run > SPA_STEREO_UPMIX_MAX_FRAMES)
                run = SPA_STEREO_UPMIX_MAX_FRAMES;

            if (action == SPA_DECODED_PCM_ACTION_REORDER_5_1) {
                if (out == SPA_PCM_5_1_CHANNELS)
                    spa_reorder_5_1_decoder_to_declared_s16(src, run,
                                                            rendered);
                else
                    spa103_reorder_5_1_s16(src, rendered, out, run);
            } else if (action == SPA_DECODED_PCM_ACTION_STEREO_UPMIX) {
                rc = spa_stereo_upmix_process_s16(
                    &spa103_render.stereoUpmix, src, run, rendered, run);
                if (rc != SPA_STEREO_UPMIX_OK) {
                    spa103_render.upmixFailed++;
                    if (spa103_render.upmixFailed == 1)
                        LOG_ERROR(LOG_MODULE,
                                  "buffer103: stereo upmix failed status=%d; "
                                  "falling back to exact L/R widening",
                                  rc);
                    spa103_widen_s16(src, dch, rendered, out, run);
                }
            } else if (dch < out) {
                spa103_widen_s16(src, dch, rendered, out, run);
            } else if (spa103_render.decodedPcmKind ==
                           SPA_DECODED_PCM_APAC_5_1_2 &&
                       spa103_render.formatIndex == SPA_APAC_48K_5_1_2 &&
                       dch == SPA_PCM_5_1_2_CHANNELS &&
                       out == SPA_PCM_5_1_CHANNELS) {
                /* The legacy experimental SETUP=66 path may still keep a
                 * six-wide output ring. Passthrough cannot fit that ring, so
                 * its configured fallback is the explicit height drop. The
                 * decoder's native PCM8 order remains unmeasured; never enter
                 * this branch from a stale 65/23 SETUP hint. */
                if (spa103_render.atmosMode == SPA_ATMOS_DOWNMIX)
                    spa_downmix_5_1_2_s16(src, run, rendered);
                else
                    spa_drop_5_1_2_s16(src, run, rendered);
            } else if (dch == out) {
                memcpy(rendered, src,
                       run * out * sizeof(*rendered));
            } else {
                spa103_widen_s16(src, dch, rendered, out, run);
            }

            /* MainLoudnessParameters is a programme gain, not an HSSR input
             * feature. Apply it uniformly after reorder/upmix so AAC gain does
             * not shift the model's absolute log-energy features or alter its
             * semantic/object masks. */
            spa_soundcheck_apply_s16(&spa103_render.soundCheck, rendered,
                                     run, out);
            spa_soundcheck_limit_s16(&spa103_render.soundCheck, rendered,
                                     run, out);
            spa103_pcm_stats_locked(rendered, run);
            spa103_ring_push_locked(rendered, run, packetSequence, packetRTP);
            spa103_render.framesIn += run;
            at += run;
        }
    }
    pthread_mutex_unlock(&spa103_render.lock);
    return SPA103_RENDER_FRAME_CONSUMED;
}

/* Validate/authenticate while rate=0, then retain the complete encrypted
 * record.  It is authenticated again immediately before decode because the
 * raw packet is the durable queue object and carries no pointers into a
 * temporary plaintext buffer.  Paused/cold media pays that cost; steady
 * rate=1 playback keeps the direct single-auth path. */
static int spa103_queue_frame(const uint8_t *packet, size_t length,
                              uint8_t *plaintext, size_t plaintextCapacity,
                              int sequenceAlreadyNoted)
{
    uint8_t key[SPA_APAP_KEY_SIZE];
    spa_apap_extension_t ext;
    const uint8_t *audio = 0;
    size_t audioLength = 0, plainLength = 0;
    uint32_t sequence, rtp;
    uint64_t anomalyCount = 0, discarded = 0;
    int variant = 0, result, crossed = 0;

    if (!packet || !length || !plaintext) return 0;
    pthread_mutex_lock(&spa103_lock);
    memcpy(key, spa103_receiver.shk, sizeof(key));
    pthread_mutex_unlock(&spa103_lock);
    if (!spa_apap_open(key, packet, length, plaintext, plaintextCapacity,
                       &plainLength, &variant)) {
        memset(key, 0, sizeof(key));
        pthread_mutex_lock(&spa103_render.lock);
        anomalyCount = ++spa103_render.authFailed;
        pthread_mutex_unlock(&spa103_render.lock);
        spa103_trace_packet_anomaly("auth-queue", anomalyCount,
                                    SPA_APAC_ERR_DECODE);
        return 0;
    }
    memset(key, 0, sizeof(key));
    memset(&ext, 0, sizeof(ext));
    if (variant == SPA_APAP_AAD_EXTENSION) {
        if (!spa_apap_parse_extension(plaintext, plainLength, &ext,
                                      &audio, &audioLength)) {
            pthread_mutex_lock(&spa103_render.lock);
            anomalyCount = ++spa103_render.tlvFailed;
            pthread_mutex_unlock(&spa103_render.lock);
            spa103_trace_packet_anomaly("extension-queue", anomalyCount,
                                        SPA_APAC_ERR_DECODE);
            return 0;
        }
    } else {
        audio = plaintext;
        audioLength = plainLength;
    }

    sequence = spa103_apap_sequence(packet);
    rtp = (uint32_t)spa103_media_time_to_rtp(
        spa103_apap_media_value(packet), spa103_apap_media_scale(packet));
    pthread_mutex_lock(&spa103_render.lock);
    if (!audioLength) {
        spa103_observe_extension_locked(&ext);
        result = spa_apap_lifecycle_observe(&spa103_render.lifecycle,
                                            sequence, rtp, &crossed) ?
                 SPA_APAP_ENQUEUE_DROPPED_FLUSH : SPA_APAP_ENQUEUE_QUEUED;
        if (result == SPA_APAP_ENQUEUE_QUEUED)
            spa103_render.stateOnlyPackets++;
    } else {
        result = spa_apap_lifecycle_enqueue(&spa103_render.lifecycle,
                                            packet, length, sequence, rtp,
                                            &crossed);
    }
    discarded = spa103_render.lifecycle.current_flush_dropped;
    pthread_mutex_unlock(&spa103_render.lock);

    if (crossed) {
        LOG_INFO(LOG_MODULE,
                 "buffer103: flush boundary crossed at seq=%u rtp=%u discarded=%llu",
                 (unsigned)sequence, (unsigned)rtp,
                 (unsigned long long)discarded);
        STATE_TRACE("BUFFERED_FLUSH_BOUNDARY",
                    "sequence=%u rtp=%u discarded=%llu",
                    (unsigned)sequence, (unsigned)rtp,
                    (unsigned long long)discarded);
    }
    if (result == SPA_APAP_ENQUEUE_DROPPED_FLUSH) return 1;
    if (result == SPA_APAP_ENQUEUE_FULL || result == SPA_APAP_ENQUEUE_NO_MEMORY) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: compressed queue rejected authenticated seq=%u "
                  "status=%d; closing APAP receiver to avoid silent loss",
                  (unsigned)sequence, result);
        STATE_TRACE("BUFFERED_QUEUE_FAILURE",
                    "sequence=%u status=%d", (unsigned)sequence, result);
        return -1;
    }
    if (!sequenceAlreadyNoted) spa103_note_authenticated_sequence(packet);
    return 1;
}

static int spa103_render_should_queue(void)
{
    int queue;

    pthread_mutex_lock(&spa103_render.lock);
    queue = !spa_apap_lifecycle_playing(&spa103_render.lifecycle) ||
            spa103_render.lifecycle.queued_packets != 0 ||
            spa103_render.lifecycle.inflight != 0;
    pthread_mutex_unlock(&spa103_render.lock);
    return queue;
}

static int spa103_render_queue_state(int *playing, size_t *queuedPackets,
                                     size_t *queuedBytes, int *saturated)
{
    int haveQueue;

    pthread_mutex_lock(&spa103_render.lock);
    if (playing)
        *playing = spa_apap_lifecycle_playing(&spa103_render.lifecycle);
    if (queuedPackets)
        *queuedPackets = spa103_render.lifecycle.queued_packets;
    if (queuedBytes)
        *queuedBytes = spa103_render.lifecycle.queued_bytes;
    if (saturated)
        *saturated = spa_apap_lifecycle_saturated(&spa103_render.lifecycle);
    haveQueue = spa103_render.lifecycle.queued_packets != 0;
    pthread_mutex_unlock(&spa103_render.lock);
    return haveQueue;
}

/* 1 consumed, -1 retained for a later rate/cookie edge, 0 no work. */
static int spa103_render_drain_one_queued(void)
{
    const uint8_t *packet;
    size_t length = 0;
    size_t primeTarget;
    int result;

    pthread_mutex_lock(&spa103_render.lock);
    primeTarget = spa103_render.primeTargetFrames;
    if (!primeTarget) primeTarget = spa103_prime_frames();
    if (!spa_pcm_queue_should_decode(spa103_render.fill,
                                     spa103_high_frames(), primeTarget,
                                     spa103_render.primed)) {
        pthread_mutex_unlock(&spa103_render.lock);
        return 0;
    }
    packet = spa_apap_lifecycle_take(&spa103_render.lifecycle,
                                     &length, 0, 0);
    pthread_mutex_unlock(&spa103_render.lock);
    if (!packet) return 0;

    result = spa103_render_frame(packet, length, 1);
    pthread_mutex_lock(&spa103_render.lock);
    spa_apap_lifecycle_finish(&spa103_render.lifecycle,
                              result == SPA103_RENDER_FRAME_RETRY);
    pthread_mutex_unlock(&spa103_render.lock);
    return result == SPA103_RENDER_FRAME_RETRY ? -1 : 1;
}

static int spa103_receive_frame(const uint8_t *packet, size_t length,
                                uint8_t *queuePlaintext,
                                size_t queuePlaintextCapacity)
{
    int result;

    if (spa103_render_should_queue())
        return spa103_queue_frame(packet, length, queuePlaintext,
                                  queuePlaintextCapacity, 0);
    result = spa103_render_frame(packet, length, 0);
    if (result == SPA103_RENDER_FRAME_RETRY)
        return spa103_queue_frame(packet, length, queuePlaintext,
                                  queuePlaintextCapacity, 1);
    return 1;
}

/* 1 while the producer should pace itself: nothing is draining the ring, or it is already half
 * full.  When decoding is off this is always true, which reproduces the capture-only build's
 * unconditional one-packet-per-frame-period sleep exactly. */
static int spa103_render_backpressure(void)
{
    int paced;

    pthread_mutex_lock(&spa103_render.lock);
    paced = spa103_render.state != SPA_103_RENDER_RUNNING ||
            !spa_apap_lifecycle_playing(&spa103_render.lifecycle) ||
            spa103_render.fill >= spa103_high_frames();
    pthread_mutex_unlock(&spa103_render.lock);
    return paced;
}

/* Caller holds spa103_render.lock.  The cold start and an explicit seek/item
 * flush deliberately use different cushions: 400 ms protects first attach;
 * post-flush only has to refill QSA's ten 16-ms START_FULL fragments. */
static void spa103_render_maybe_prime_locked(void)
{
    size_t target = spa103_render.primeTargetFrames;

    if (!target) target = spa103_prime_frames();
    if (!spa103_render.primed &&
        spa_pcm_prime_ready(spa103_render.fill, target))
        spa103_render.primed = 1;
}

/* The type-103 SETUP stream descriptor's `shk`: 32 raw bytes of CFData, the ChaCha20-Poly1305
 * key verbatim.  Copied into the receiver and never logged, printed or written anywhere. */
static int spa103_read_shk(const void *streamDesc, uint8_t out[SPA_APAP_KEY_SIZE])
{
    void *key, *data;
    const uint8_t *bytes;
    long length;
    int32_t err = 0;

    if (!streamDesc || !spa103_cf_dict_get_typed || !spa103_cf_data_type_id ||
        !spa103_cf_data_length || !spa103_cf_data_bytes)
        return 0;
    key = cfa_str(0, "shk", SPA_CFENC_UTF8);
    if (!key) return 0;
    data = spa103_cf_dict_get_typed(streamDesc, key, spa103_cf_data_type_id(), &err);
    cfa_rel(key);
    if (!data || err) return 0;
    length = spa103_cf_data_length(data);
    bytes = spa103_cf_data_bytes(data);
    if (length != (long)SPA_APAP_KEY_SIZE || !bytes) return 0;
    memcpy(out, bytes, SPA_APAP_KEY_SIZE);
    return 1;
}

int spa_buffered_pcm6_active(void)
{
    return spa_buffered_pcm_active() &&
           spa_buffered_pcm_channels() == SPA_PCM_5_1_CHANNELS;
}

int spa_buffered_pcm_active(void)
{
    int active;
    pthread_mutex_lock(&spa103_render.lock);
    active = spa103_render.state == SPA_103_RENDER_RUNNING && spa103_render.ring != 0;
    pthread_mutex_unlock(&spa103_render.lock);
    return active;
}

unsigned spa_buffered_pcm_channels(void)
{
    unsigned channels = 0;

    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.state == SPA_103_RENDER_RUNNING && spa103_render.ring)
        channels = spa103_render.outputChannels;
    pthread_mutex_unlock(&spa103_render.lock);
    return channels;
}

size_t spa_buffered_pcm_read(int16_t *out, size_t frames, unsigned channels)
{
    size_t done = 0;
    size_t take = 0;

    if (!out || !frames) return 0;
    pthread_mutex_lock(&spa103_render.lock);
    /* Returning 0 here is deliberate: the sink then expands the phone's own
     * realtime stereo for this fragment, which is real audio.  Padding silence
     * instead is what made the gap audible. */
    spa103_render_maybe_prime_locked();
    if (spa103_render.state == SPA_103_RENDER_RUNNING &&
        spa_apap_lifecycle_playing(&spa103_render.lifecycle) &&
        spa103_render.primed && spa103_render.ring &&
        channels == spa103_render.outputChannels) {
        /* QSA consumes one indivisible period.  Leaving a 1024/768 cadence
         * remainder (normally 256 or 512 frames) queued is preferable to the
         * old [decoded prefix + zero tail] fragment, which produced the
         * measured 5.3 ms transition glitch. */
        take = spa_pcm_whole_period_frames(spa103_render.fill, frames);
        while (done < take) {
            size_t run = spa103_ring_frames() - spa103_render.head;
            if (run > spa103_render.fill) run = spa103_render.fill;
            if (run > take - done) run = take - done;
            memcpy(out + done * channels,
                   spa103_render.ring + spa103_render.head * channels,
                   run * channels * sizeof(int16_t));
            spa103_render.head = (spa103_render.head + run) % spa103_ring_frames();
            spa103_render.fill -= run;
            done += run;
        }
        spa103_render.framesOut += done;
    }
    pthread_mutex_unlock(&spa103_render.lock);
    return done;
}

size_t spa_buffered_pcm6_read(int16_t *out, size_t frames)
{
    return spa_buffered_pcm_read(out, frames, SPA_PCM_5_1_CHANNELS);
}

/* ---- stock DIO5/QSA sink bridge -------------------------------------------------------------
 *
 * Stock 210.81 reaches snd_pcm_plugin_write from both CQNXSoundIF write variants. The live
 * type-100 fallback is normally stereo even when type 103 carries Spatial media, so merely
 * replacing its bytes with PCM6 would make QSA interpret three PCM frames as one. Interpose the
 * matching params call as well: configure the existing mpl5_dio_ent handle for 6/8 voices while
 * leaving CQNXSoundIF's own stereo cadence untouched, expand each stock write to the same number
 * of output frames, and report the original byte count back to the stock caller.
 *
 * This reuses the already-owned CarPlay device/focus path; no second PCM handle races it. Before
 * type 103 is running, stereo is copied to L/R and the other outputs are silent. While it is
 * running, decoded ring samples replace that fallback. A short ring read is padded with silence.
 */
#define SPA_PCM_SINK_NAME "mpl5_dio_ent"
#define SPA_PCM_SINK_MAX_WRITE (1024u * 1024u)
/* Eight fragments is the floor the RetroArch QNX QSA driver established on this
 * head unit; below it the CSD session cycles instead of absorbing jitter. */
#define SPA_PCM_SINK_MIN_FRAGS     8


typedef int (*spa_snd_open_name_t)(snd_pcm_t **, char *, int);
typedef int (*spa_snd_params_t)(snd_pcm_t *, snd_pcm_channel_params_t *);
typedef int (*spa_snd_setup_t)(snd_pcm_t *, snd_pcm_channel_setup_t *);
typedef ssize_t (*spa_snd_write_t)(snd_pcm_t *, const void *, size_t);
typedef int (*spa_snd_prepare_t)(snd_pcm_t *, int);
typedef int (*spa_snd_flush_t)(snd_pcm_t *, int);
typedef int (*spa_snd_close_t)(snd_pcm_t *);
typedef unsigned int (*spa_snd_disable_t)(snd_pcm_t *, unsigned int);
typedef void (*spa_dio_audio_type_callback_t)(int, void *);
typedef void (*spa_dio_audio_torn_down_callback_t)(void *);

/* Exact exported Pioneer callbacks recovered from dio_manager MU1316.
 * AudioStreamPrepare invokes s_audioTypeCallback(kAudioTypeMedia, context)
 * before it opens mpl5_dio_ent, then waits for the HMI routing ACK.  A hook-
 * owned type-103 stream has no AudioStream object, so opening QSA directly
 * skips this higher-level DSI media-route declaration.  The callback's second
 * argument is not read; it only packages CAirPlayServiceEvent_UpdateMainAudioType.
 * s_audioTornDownCallback packages the matching release event. */
#define SPA_DIO_AUDIO_TYPE_CALLBACK_SYM \
    "_ZN3dio11CDIOManager19s_audioTypeCallbackEiPv"
#define SPA_DIO_AUDIO_TORN_DOWN_CALLBACK_SYM \
    "_ZN3dio11CDIOManager23s_audioTornDownCallbackEPv"
#define SPA_DIO_AUDIO_TYPE_MEDIA 1

enum {
    SPA_PCM_RECLAIM_NONE = 0,
    SPA_PCM_RECLAIM_OPEN,
    SPA_PCM_RECLAIM_PARAMS
};

typedef struct {
    pthread_mutex_t lock;
    pthread_mutex_t ioLock;         /* serializes real QSA I/O without blocking write_internal */
    snd_pcm_t *handle;
    int sourceFormat;
    unsigned sourceChannels;
    unsigned outputChannels;
    int expandedParams;
    int detached;                   /* type-100 released it; type-103 temporarily owns it */
    int closeFlushSuppressed;       /* stock flush was deferred until close decides ownership */
    int closeFlushChannel;
    int detachedNeedsPrepare;       /* legacy fallback when close's flush could not be skipped */
    int reclaimStage;               /* same live handle is moving back to stock type-100 */
    int16_t *scratch;
    size_t scratchBytes;
    uint64_t writes;
    uint64_t setupCalls;            /* snd_pcm_plugin_setup is polled at the fragment rate */
#if ENABLE_STATE_TRACE
    int firstSuccessfulWriteTraced;
    int detachedRingWaitReported;
    uint64_t detachedRingWaits;
    uint64_t tracedDeviceUnderruns;
#endif
    uint64_t detachedWrites;
    uint64_t detachedZeroWrites;
    uint64_t detachedRecoveries;   /* prepare calls while detached after rendering began */
    uint64_t timelineFlushes;      /* prefix FLUSHes propagated into the live QSA queue */
    uint64_t timelineFlushFailures;
    uint64_t detachedClockDiscontinuities;
    spa_feedback_clock_t detachedClock;
    uint64_t bufferedWrites;
    uint64_t underrunFrames;        /* defensive: must remain zero with whole-period reads */
    uint64_t shortRingFallbacks;    /* whole periods sent from realtime instead of zero-padded */
    uint64_t avoidedSilenceFrames;  /* missing tails the old bridge would have zero-padded */
    size_t   lastShortRingFrames;
    uint64_t bufferedSilencePeriods;/* full silence: never cross type-103/type-100 timelines */
    uint64_t bufferedSilenceFrames;
    /* What the device actually accepted.  `writes` counts our calls, not their
     * success, and QSA masks a failing snd_pcm_write_internal as zero bytes
     * written -- measured in the QNX harness, where every fragment after the
     * first returned -EIO and surfaced as 0.  A stock caller reading 0 rewrites
     * the same buffer, which is a repeated seam at the fragment rate, and none of
     * the existing counters can see it. */
    uint64_t shortWrites;
    uint64_t zeroWrites;
    int64_t  lastWriteResult;
    uint64_t oddSizeWrites;          /* caller wrote a fragment that was not our negotiated one */
    size_t   lastOddSize;
    int      deviceStatus;           /* SND_PCM_STATUS_* from the device, -1 = never asked */
    uint64_t deviceUnderruns;
    uint64_t suppressedFlushes;
    uint64_t reclaimedOpens;
    uint64_t skippedReclaimParams;
    uint64_t skippedReclaimPrepares;
    uint64_t coldOpenAttempts;
    uint64_t coldOpenFailures;
    uint64_t coldOpenRetryAfterNs;
    uint64_t detachedRouteReacquireAfterNs;
    int      hookMediaRouteRequested; /* hook owns DSI Media route until stock type-100 takes over */
    uint64_t coldMediaRouteRequests;
    uint64_t coldMediaRouteFailures;
} spa_pcm_sink_t;

static spa_pcm_sink_t spa_pcm_sink = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .ioLock = PTHREAD_MUTEX_INITIALIZER
};

static int spa_sink_device_supports_voices(snd_pcm_t *handle, unsigned voices);

/* Caller holds ioLock, but not lock.  Keeping the main-program callback out of
 * spa_pcm_sink.lock avoids re-entrancy through the active-object dispatcher.
 * ioLock still makes the ownership decision atomic with a concurrent stock
 * type-100 snd_pcm_open_name(). */
static int spa_pcm_sink_request_hook_media_route(const char *owner,
                                                 const char *reason)
{
    static spa_dio_audio_type_callback_t callback;
    int alreadyRequested;

    pthread_mutex_lock(&spa_pcm_sink.lock);
    alreadyRequested = spa_pcm_sink.hookMediaRouteRequested;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (alreadyRequested) return 1;

    if (!callback)
        callback = (spa_dio_audio_type_callback_t)
            dlsym(RTLD_DEFAULT, SPA_DIO_AUDIO_TYPE_CALLBACK_SYM);
    if (!callback) {
        uint64_t failures;
        pthread_mutex_lock(&spa_pcm_sink.lock);
        failures = ++spa_pcm_sink.coldMediaRouteFailures;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        STATE_TRACE("AUDIO_ROUTE",
                    "action=request_failed owner=%s reason=%s audio_type=%d "
                    "failures=%llu symbol=%s",
                    owner ? owner : "hook_buffered",
                    reason ? reason : "unknown",
                    SPA_DIO_AUDIO_TYPE_MEDIA,
                    (unsigned long long)failures,
                    SPA_DIO_AUDIO_TYPE_CALLBACK_SYM);
        LOG_ERROR(LOG_MODULE,
                  "audio sink: cannot resolve Pioneer media-route callback %s",
                  SPA_DIO_AUDIO_TYPE_CALLBACK_SYM);
        return 0;
    }

    callback(SPA_DIO_AUDIO_TYPE_MEDIA, 0);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    spa_pcm_sink.hookMediaRouteRequested = 1;
    spa_pcm_sink.detachedRouteReacquireAfterNs = 0;
    spa_pcm_sink.coldMediaRouteRequests++;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    STATE_TRACE("AUDIO_ROUTE",
                "action=request owner=%s reason=%s audio_type=%d result=queued symbol=%s",
                owner ? owner : "hook_buffered",
                reason ? reason : "unknown",
                SPA_DIO_AUDIO_TYPE_MEDIA, SPA_DIO_AUDIO_TYPE_CALLBACK_SYM);
    LOG_WARN(LOG_MODULE,
             "audio sink: queued Pioneer media-route declaration owner=%s reason=%s",
             owner ? owner : "hook_buffered",
             reason ? reason : "unknown");
    return 1;
}

static void spa_pcm_sink_release_cold_media_route(void)
{
    static spa_dio_audio_torn_down_callback_t callback;

    if (!callback)
        callback = (spa_dio_audio_torn_down_callback_t)
            dlsym(RTLD_DEFAULT, SPA_DIO_AUDIO_TORN_DOWN_CALLBACK_SYM);
    if (!callback) {
        STATE_TRACE("AUDIO_ROUTE",
                    "action=release_failed owner=cold_buffered symbol=%s",
                    SPA_DIO_AUDIO_TORN_DOWN_CALLBACK_SYM);
        LOG_ERROR(LOG_MODULE,
                  "audio sink: cannot resolve Pioneer media-route release callback %s",
                  SPA_DIO_AUDIO_TORN_DOWN_CALLBACK_SYM);
        return;
    }

    callback(0);
    STATE_TRACE("AUDIO_ROUTE",
                "action=release owner=cold_buffered result=queued symbol=%s",
                SPA_DIO_AUDIO_TORN_DOWN_CALLBACK_SYM);
    LOG_WARN(LOG_MODULE,
             "audio sink: queued Pioneer media-route release for cold buffered playback");
}

static uint64_t spa_monotonic_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}

/* Pioneer asks for Now Playing only once, during physical identification.
 * iOS's handler is explicitly restartable: another identical 0x5000 removes
 * and re-adds the AccessoryNowPlaying subscription, which is the only static
 * recovery path found for its invalidated/stale XPC publisher.  Queue one
 * refresh only after real buffered media has arrived, through the framework's
 * retained ICinemoIAP object so Cinemo owns the link sequence and ACK state.
 * A transiently unavailable injection context is retried at one-second
 * intervals, bounded to eight attempts for this type-103 generation. */
static void spa103_retry_now_playing_after_media(void)
{
    uint8_t payload[144];
    uint64_t nowNs;
    uint32_t generation, attempt;
    hook_result_t result;
    int hasMedia;

    if (!spa_cold_connect_enabled()) return;
    pthread_mutex_lock(&spa103_render.lock);
    hasMedia = spa103_render.state == SPA_103_RENDER_RUNNING &&
               spa_apap_lifecycle_playing(&spa103_render.lifecycle) &&
               spa103_render.framesIn != 0;
    pthread_mutex_unlock(&spa103_render.lock);
    if (!hasMedia) return;
    nowNs = spa_monotonic_ns();

    pthread_mutex_lock(&spa103_lock);
    if (!spa103_receiver.session || spa103_receiver.stop ||
        spa103_receiver.settingUp || spa103_receiver.nowPlayingRetryQueued ||
        spa103_receiver.nowPlayingRetryAttempts >= 8u ||
        (nowNs && spa103_receiver.nowPlayingRetryAfterNs &&
         nowNs < spa103_receiver.nowPlayingRetryAfterNs)) {
        pthread_mutex_unlock(&spa103_lock);
        return;
    }
    generation = spa103_receiver.generation;
    attempt = ++spa103_receiver.nowPlayingRetryAttempts;
    spa103_receiver.nowPlayingRetryAfterNs =
        nowNs ? nowNs + UINT64_C(1000000000) : 0;
    pthread_mutex_unlock(&spa103_lock);

    if (iap2_build_pioneer_now_playing_start(payload, sizeof(payload)) !=
        sizeof(payload)) {
        STATE_TRACE("IAP_NOW_PLAYING_RETRY",
                    "action=build_failed generation=%u attempt=%u",
                    (unsigned)generation, (unsigned)attempt);
        return;
    }

    result = hook_inject_message(IAP2_MSG_NOW_PLAYING_START,
                                 payload, sizeof(payload));
    if (result == HOOK_OK) {
        pthread_mutex_lock(&spa103_lock);
        if (spa103_receiver.generation == generation &&
            spa103_receiver.session && !spa103_receiver.stop)
            spa103_receiver.nowPlayingRetryQueued = 1;
        pthread_mutex_unlock(&spa103_lock);
        LOG_WARN(LOG_MODULE,
                 "buffer103: queued one stock-equivalent Now Playing refresh "
                 "generation=%u attempt=%u",
                 (unsigned)generation, (unsigned)attempt);
        STATE_TRACE("IAP_NOW_PLAYING_RETRY",
                    "action=queued generation=%u attempt=%u payload_bytes=%u",
                    (unsigned)generation, (unsigned)attempt,
                    (unsigned)sizeof(payload));
    } else {
        LOG_WARN(LOG_MODULE,
                 "buffer103: Now Playing refresh context unavailable "
                 "generation=%u attempt=%u result=%d; will retry",
                 (unsigned)generation, (unsigned)attempt, (int)result);
        STATE_TRACE("IAP_NOW_PLAYING_RETRY",
                    "action=retry generation=%u attempt=%u result=%d",
                    (unsigned)generation, (unsigned)attempt, (int)result);
    }
}

static void spa_pcm_sink_reset_detached_clock_locked(void)
{
    spa_feedback_clock_reset(&spa_pcm_sink.detachedClock);
    spa_pcm_sink.detachedClockDiscontinuities = 0;
}

/* Drop periods from the previous buffered timeline which have already crossed
 * the software ring boundary.  The live trace proved that an until-only
 * FLUSHBUFFERED used to clear both APAP queues, wait for the 160-ms re-prime,
 * and only then recover QSA after its first zero write.  During that interval
 * QSA could still render the old song.  Serialize with every open/write/close,
 * bypass our close-adjacent flush interposer, and leave the same handle
 * prepared for the first complete period of the new timeline.
 *
 * A bounded from..until FLUSH deliberately does not use this operation: QSA
 * periods do not retain APAP sequence/RTP tags, so they cannot be pruned
 * selectively without also discarding valid samples outside the interval. */
static void spa_pcm_sink_flush_buffered_timeline(void)
{
    static spa_snd_flush_t realFlush;
    static spa_snd_prepare_t realPrepare;
    snd_pcm_t *handle = 0;
    int detached = 0;
    int flushResult = -1;
    int prepareResult = -1;
    uint64_t flushes = 0;
    uint64_t failures = 0;

    pthread_mutex_lock(&spa_pcm_sink.ioLock);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.handle && spa_pcm_sink.expandedParams) {
        handle = spa_pcm_sink.handle;
        detached = spa_pcm_sink.detached;
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);

    if (!handle) {
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        STATE_TRACE("QSA_TIMELINE_FLUSH",
                    "action=skip reason=no_expanded_handle");
        return;
    }

    if (!realFlush)
        realFlush = (spa_snd_flush_t)dlsym(RTLD_NEXT,
                                           "snd_pcm_plugin_flush");
    if (!realPrepare)
        realPrepare = (spa_snd_prepare_t)dlsym(RTLD_NEXT,
                                               "snd_pcm_plugin_prepare");
    if (realFlush)
        flushResult = realFlush(handle, SND_PCM_CHANNEL_PLAYBACK);
    if (flushResult == 0 && realPrepare)
        prepareResult = realPrepare(handle, SND_PCM_CHANNEL_PLAYBACK);

    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.handle == handle && spa_pcm_sink.expandedParams) {
        /* A timeline discontinuity invalidates the detached feedback window
         * even when stock type-100 currently owns the shared QSA handle. */
        spa_feedback_clock_reset(&spa_pcm_sink.detachedClock);
        spa_pcm_sink.detachedClockDiscontinuities++;
#if ENABLE_STATE_TRACE
        spa_pcm_sink.detachedRingWaitReported = 0;
#endif
        if (flushResult == 0 && prepareResult == 0) {
            flushes = ++spa_pcm_sink.timelineFlushes;
            spa_pcm_sink.detachedNeedsPrepare = 0;
        } else {
            failures = ++spa_pcm_sink.timelineFlushFailures;
        }
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);

    if (flushResult == 0 && prepareResult == 0) {
        LOG_WARN(LOG_MODULE,
                 "audio sink: flushed previous buffered timeline handle=%p "
                 "owner=%s flushes=%llu",
                 handle, detached ? "detached" : "stock",
                 (unsigned long long)flushes);
        STATE_TRACE("QSA_TIMELINE_FLUSH",
                    "action=flush_prepare owner=%s flush_result=%d "
                    "prepare_result=%d flushes=%llu",
                    detached ? "detached" : "stock", flushResult,
                    prepareResult, (unsigned long long)flushes);
    } else {
        LOG_ERROR(LOG_MODULE,
                  "audio sink: buffered timeline flush failed handle=%p "
                  "owner=%s flush=%d prepare=%d failures=%llu",
                  handle, detached ? "detached" : "stock", flushResult,
                  prepareResult, (unsigned long long)failures);
        STATE_TRACE("QSA_TIMELINE_FLUSH",
                    "action=failed owner=%s flush_result=%d "
                    "prepare_result=%d failures=%llu",
                    detached ? "detached" : "stock", flushResult,
                    prepareResult, (unsigned long long)failures);
    }
}

/* During the first ~20 seconds the nominal 48-kHz contract is a much safer
 * bootstrap than the last stock sample: the vehicle trace caught a transient
 * 48080.23-Hz response immediately before type-100 disappeared, and the old
 * cache replayed it for 276 seconds.  Once the detached writer has a clean
 * 1024-period window, publish the measured blocking-write cadence instead. */
static int spa_pcm_sink_detached_feedback_rate(double *rateHz, int *measured)
{
    int active = 0, valid = 0;
    double rate = 0.0;

    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.detached && spa_pcm_sink.handle &&
        spa_pcm_sink.expandedParams) {
        active = 1;
        rate = spa_feedback_clock_get(&spa_pcm_sink.detachedClock, &valid);
        if (!valid) rate = (double)SPA_SR;
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (rateHz) *rateHz = rate;
    if (measured) *measured = valid;
    return active;
}

static void spa_pcm_sink_close_detached(const char *reason)
{
    static spa_snd_close_t realClose;
    snd_pcm_t *handle = 0;
    uint64_t writes = 0, zeroWrites = 0;
    int releaseMediaRoute = 0;
    int result = 0;

    pthread_mutex_lock(&spa_pcm_sink.ioLock);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.detached) {
        handle = spa_pcm_sink.handle;
        writes = spa_pcm_sink.detachedWrites;
        zeroWrites = spa_pcm_sink.detachedZeroWrites;
        spa_pcm_sink.handle = 0;
        spa_pcm_sink.detached = 0;
        spa_pcm_sink.expandedParams = 0;
        spa_pcm_sink.closeFlushSuppressed = 0;
        spa_pcm_sink.detachedNeedsPrepare = 0;
        spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
        spa_pcm_sink.sourceChannels = 0;
        spa_pcm_sink.outputChannels = 0;
        spa_pcm_sink_reset_detached_clock_locked();
    }
    if (spa_pcm_sink.hookMediaRouteRequested) {
        spa_pcm_sink.hookMediaRouteRequested = 0;
        releaseMediaRoute = 1;
    }
    if (!spa_pcm_sink.handle) {
        spa_pcm_sink.coldOpenAttempts = 0;
        spa_pcm_sink.coldOpenFailures = 0;
        spa_pcm_sink.coldOpenRetryAfterNs = 0;
        spa_pcm_sink.detachedRouteReacquireAfterNs = 0;
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (!handle) {
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        if (releaseMediaRoute) spa_pcm_sink_release_cold_media_route();
        return;
    }

    if (!realClose) realClose = (spa_snd_close_t)dlsym(RTLD_NEXT, "snd_pcm_close");
    result = realClose ? realClose(handle) : -1;
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);
    if (releaseMediaRoute) spa_pcm_sink_release_cold_media_route();
    LOG_WARN(LOG_MODULE,
             "audio sink: closed detached %s handle=%p reason=%s writes=%llu zero=%llu result=%d",
             SPA_PCM_SINK_NAME, handle, reason ? reason : "?",
             (unsigned long long)writes, (unsigned long long)zeroWrites, result);
    STATE_TRACE("QSA_CLOSE",
                "owner=detached sink=%s reason=\"%s\" writes=%llu zero=%llu result=%d",
                SPA_PCM_SINK_NAME, reason ? reason : "unknown",
                (unsigned long long)writes,
                (unsigned long long)zeroWrites, result);
}

static size_t spa_buffered_pcm_pending(unsigned channels)
{
    size_t pending = 0;

    pthread_mutex_lock(&spa103_render.lock);
    /* The detached owner asks whether a full period exists before it calls
     * read().  Let that path cross the same initial-prime threshold itself;
     * otherwise a session whose type-100 owner disappeared early could reach
     * the cushion yet wait forever because only read() used to set primed. */
    spa103_render_maybe_prime_locked();
    if (spa103_render.state == SPA_103_RENDER_RUNNING &&
        spa_apap_lifecycle_playing(&spa103_render.lifecycle) &&
        spa103_render.primed &&
        spa103_render.ring && channels == spa103_render.outputChannels)
        pending = spa103_render.fill;
    pthread_mutex_unlock(&spa103_render.lock);
    return pending;
}

/* Snapshot only enough state for the APAP/QSA pump's scheduling decision.
 * Do not hold the sink mutex while taking the render mutex: stock writes use
 * both paths and ownership can change while the receiver is polling.  The
 * one-period writer revalidates everything under ioLock before touching QSA. */
static int spa_pcm_sink_detached_pending(size_t *pendingOut)
{
    unsigned channels = 0;
    int detached = 0;
    size_t pending = 0;

    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.detached && spa_pcm_sink.handle &&
        spa_pcm_sink.expandedParams) {
        detached = 1;
        channels = spa_pcm_sink.outputChannels;
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);

    if (detached && channels)
        pending = spa_buffered_pcm_pending(channels);
    if (pendingOut) *pendingOut = pending;
    return detached;
}

/* When iOS backgrounds the phone/app it tears down the otherwise-silent
 * realtime type-100 stream several seconds before buffered type-103.  The
 * existing QSA handle is already configured and prepared for PCM6, so let the
 * APAP receiver thread temporarily drive it instead of adding another thread
 * or racing a second handle against the same device.  QSA's blocking write is
 * the render clock; 1024-frame APAP packets are re-fragmented through the
 * existing ring into the device's measured 768-frame periods.
 *
 * One invocation writes AT MOST one period.  The former drain-to-empty loop
 * collapsed a 400-ms prime cushion to the normal 0/256/512-frame cadence
 * remainder in one burst, so the next network/scheduler hiccup became an
 * audible underrun.  The APAP loop now calls this once per pump iteration and
 * applies TCP backpressure around the configured high-water mark. */
static void spa_pcm_sink_write_one_detached_period(void)
{
    static spa_snd_write_t realWrite;
    static spa_snd_prepare_t realPrepare;
    static int (*realStatus)(snd_pcm_t *, snd_pcm_channel_status_t *);
    snd_pcm_t *handle;
    unsigned channels;
    size_t bytes, got, pending;
    ssize_t written;
    int firstWrite, needsPrepare, logWrite, probeStatus;
    int reacquireMediaRoute = 0;
    int autoRecovery = 0, clockEvent, clockValid;
    int statusResult = -1;
    uint64_t writeNs, previousWriteNs, writeGapNs = 0;
    double clockRate;
    snd_pcm_channel_status_t status;
#if ENABLE_STATE_TRACE
    int traceRingWait = 0;
    int traceCadenceGap = 0;
    int traceDeviceUnderrun = 0;
    int traceHealth = 0;
    uint64_t traceRingWaitGapNs = 0;
    uint64_t traceRingWaitCount = 0;
    uint64_t traceRingWaits = 0;
    uint64_t traceWrites = 0;
    uint64_t traceZeroWrites = 0;
    uint64_t traceRecoveries = 0;
    uint64_t traceClockResets = 0;
    uint64_t traceDeviceUnderruns = 0;
    int traceDeviceStatus = -1;
    int traceCanStarve = 0;
#endif

        pthread_mutex_lock(&spa_pcm_sink.ioLock);
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (!spa_pcm_sink.detached || !spa_pcm_sink.handle ||
            !spa_pcm_sink.expandedParams) {
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            pthread_mutex_unlock(&spa_pcm_sink.ioLock);
            return;
        }
        {
            uint64_t nowNs = spa_monotonic_ns();
            if (spa_media_route_reacquire_due(
                    spa_pcm_sink.detached,
                    spa_pcm_sink.hookMediaRouteRequested,
                    spa_pcm_sink.detachedRouteReacquireAfterNs, nowNs)) {
                /* Claim this attempt while ioLock excludes a simultaneous
                 * stock reopen.  The Pioneer callback stays outside the main
                 * state lock because its active-object dispatch can re-enter. */
                spa_pcm_sink.detachedRouteReacquireAfterNs = 0;
                reacquireMediaRoute = 1;
            }
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        if (reacquireMediaRoute &&
            !spa_pcm_sink_request_hook_media_route(
                "detached_buffered", "stock_type100_torn_down")) {
            uint64_t nowNs = spa_monotonic_ns();
            pthread_mutex_lock(&spa_pcm_sink.lock);
            if (spa_pcm_sink.detached &&
                !spa_pcm_sink.hookMediaRouteRequested)
                spa_pcm_sink.detachedRouteReacquireAfterNs =
                    nowNs + SPA_MEDIA_ROUTE_RETRY_DELAY_NS;
            pthread_mutex_unlock(&spa_pcm_sink.lock);
        }
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (!spa_pcm_sink.detached || !spa_pcm_sink.handle ||
            !spa_pcm_sink.expandedParams) {
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            pthread_mutex_unlock(&spa_pcm_sink.ioLock);
            return;
        }
        handle = spa_pcm_sink.handle;
        channels = spa_pcm_sink.outputChannels;
        bytes = (size_t)SPA_PCM_SINK_PERIOD_FRAMES * channels * sizeof(int16_t);
        pthread_mutex_unlock(&spa_pcm_sink.lock);

        /* Respect the global lock order documented by
         * spa_pcm_sink_detached_pending(): never take render.lock while holding
         * spa_pcm_sink.lock. ioLock keeps the QSA handle/scratch ownership
         * stable while this scheduling snapshot is collected. */
        pending = spa_buffered_pcm_pending(channels);
#if ENABLE_STATE_TRACE
        pthread_mutex_lock(&spa103_render.lock);
        traceCanStarve =
            spa103_render.state == SPA_103_RENDER_RUNNING &&
            spa_apap_lifecycle_playing(&spa103_render.lifecycle) &&
            spa103_render.primed &&
            spa103_render.ring != 0;
        pthread_mutex_unlock(&spa103_render.lock);
#endif

        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (!spa_pcm_sink.detached || spa_pcm_sink.handle != handle ||
            !spa_pcm_sink.expandedParams || !channels ||
            spa_pcm_sink.outputChannels != channels ||
            spa_pcm_sink.scratchBytes < bytes ||
            pending < SPA_PCM_SINK_PERIOD_FRAMES) {
#if ENABLE_STATE_TRACE
            /* A sub-period 1024->768 cadence remainder is normal.  Diagnose
             * starvation only after the 100-ms QSA cadence budget expires;
             * the pump calls us again when either source data or a poll
             * timeout advances the loop. */
            if (traceCanStarve && spa_pcm_sink.detachedClock.lastWriteNs) {
                uint64_t nowNs = spa_monotonic_ns();
                uint64_t lastNs = spa_pcm_sink.detachedClock.lastWriteNs;
                if (nowNs > lastNs &&
                    nowNs - lastNs > SPA_FEEDBACK_CLOCK_MAX_GAP_NS &&
                    !spa_pcm_sink.detachedRingWaitReported) {
                    spa_pcm_sink.detachedRingWaitReported = 1;
                    traceRingWaitCount = ++spa_pcm_sink.detachedRingWaits;
                    traceRingWaitGapNs = nowNs - lastNs;
                    traceRingWait = 1;
                }
            }
#endif
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            pthread_mutex_unlock(&spa_pcm_sink.ioLock);
#if ENABLE_STATE_TRACE
            if (traceRingWait)
                spa103_trace_qsa_ring_wait(traceRingWaitGapNs, pending,
                                           traceRingWaitCount);
#endif
            return;
        }
        firstWrite = spa_pcm_sink.detachedWrites == 0;
        needsPrepare = firstWrite && spa_pcm_sink.detachedNeedsPrepare;
        probeStatus = ((spa_pcm_sink.detachedWrites + 1u) % 256u) == 0;
        previousWriteNs = spa_pcm_sink.detachedClock.lastWriteNs;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        /* Normally our flush interposer keeps this exact handle running across
         * CQNXSoundIF's close.  Retain the prepare fallback for the race where
         * type-103 became active between stock's flush and close: that flush was
         * real, so the first detached write still has to restart QSA. */
        if (needsPrepare) {
            int prepareResult;
            if (!realPrepare)
                realPrepare = (spa_snd_prepare_t)dlsym(RTLD_NEXT,
                                                       "snd_pcm_plugin_prepare");
            prepareResult = realPrepare ?
                realPrepare(handle, SND_PCM_CHANNEL_PLAYBACK) : -1;
            if (prepareResult != 0) {
                LOG_ERROR(LOG_MODULE,
                          "audio sink: detached prepare failed handle=%p result=%d",
                          handle, prepareResult);
                pthread_mutex_unlock(&spa_pcm_sink.ioLock);
                return;
            }
        }
        got = spa_buffered_pcm_read(spa_pcm_sink.scratch,
                                    SPA_PCM_SINK_PERIOD_FRAMES, channels);
        if (got != SPA_PCM_SINK_PERIOD_FRAMES) {
            pthread_mutex_unlock(&spa_pcm_sink.ioLock);
            return;
        }
        if (!realWrite)
            realWrite = (spa_snd_write_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_write");
        written = realWrite ? realWrite(handle, spa_pcm_sink.scratch, bytes) : -1;
        if (written == 0) {
            int prepareResult = -1;

            if (!realPrepare)
                realPrepare = (spa_snd_prepare_t)dlsym(RTLD_NEXT,
                                                       "snd_pcm_plugin_prepare");
            if (realPrepare)
                prepareResult = realPrepare(handle, SND_PCM_CHANNEL_PLAYBACK);
            if (prepareResult == 0) {
                autoRecovery = 1;
                written = realWrite(handle, spa_pcm_sink.scratch, bytes);
            }
        }
        writeNs = spa_monotonic_ns();
        if (previousWriteNs && writeNs > previousWriteNs)
            writeGapNs = writeNs - previousWriteNs;
        (void)writeGapNs;
        memset(&status, 0, sizeof(status));
        status.channel = SND_PCM_CHANNEL_PLAYBACK;
        if (probeStatus) {
            if (!realStatus)
                realStatus = (int (*)(snd_pcm_t *, snd_pcm_channel_status_t *))
                             dlsym(RTLD_NEXT, "snd_pcm_plugin_status");
            if (realStatus) statusResult = realStatus(handle, &status);
        }
        pthread_mutex_lock(&spa_pcm_sink.lock);
        spa_pcm_sink.detachedWrites++;
        if (firstWrite) spa_pcm_sink.detachedNeedsPrepare = 0;
        if (autoRecovery) spa_pcm_sink.detachedRecoveries++;
        if (written == 0) spa_pcm_sink.detachedZeroWrites++;
#if ENABLE_STATE_TRACE
        if (written == (ssize_t)bytes)
            spa_pcm_sink.detachedRingWaitReported = 0;
#endif
        if (written == (ssize_t)bytes) {
            clockEvent = spa_feedback_clock_note_write(
                &spa_pcm_sink.detachedClock, writeNs,
                SPA_PCM_SINK_PERIOD_FRAMES, autoRecovery);
        } else {
            /* A short/failed write did not consume a whole period.  Invalidate
             * the clock, but do not count that write as warmup #1. */
            spa_feedback_clock_reset(&spa_pcm_sink.detachedClock);
            clockEvent = -1;
        }
        if (clockEvent < 0) spa_pcm_sink.detachedClockDiscontinuities++;
        clockRate = spa_feedback_clock_get(&spa_pcm_sink.detachedClock,
                                           &clockValid);
        if (statusResult == 0) {
            spa_pcm_sink.deviceStatus = status.status;
            spa_pcm_sink.deviceUnderruns = (uint64_t)status.underrun;
#if ENABLE_STATE_TRACE
            if (spa_pcm_sink.deviceUnderruns >
                spa_pcm_sink.tracedDeviceUnderruns) {
                spa_pcm_sink.tracedDeviceUnderruns =
                    spa_pcm_sink.deviceUnderruns;
                traceDeviceUnderrun = 1;
            }
#endif
        }
#if ENABLE_STATE_TRACE
        traceCadenceGap = writeGapNs > SPA_FEEDBACK_CLOCK_MAX_GAP_NS;
        traceHealth = probeStatus;
        traceWrites = spa_pcm_sink.detachedWrites;
        traceZeroWrites = spa_pcm_sink.detachedZeroWrites;
        traceRecoveries = spa_pcm_sink.detachedRecoveries;
        traceClockResets = spa_pcm_sink.detachedClockDiscontinuities;
        traceRingWaits = spa_pcm_sink.detachedRingWaits;
        traceDeviceStatus = spa_pcm_sink.deviceStatus;
        traceDeviceUnderruns = spa_pcm_sink.deviceUnderruns;
#endif
        logWrite = spa_pcm_sink.detachedWrites == 1 ||
                   (spa_pcm_sink.detachedWrites % 1024u) == 0 ||
                   written != (ssize_t)bytes || autoRecovery || clockEvent < 0;
        if (autoRecovery)
            LOG_WARN(LOG_MODULE,
                     "audio sink: detached write returned zero; prepare+retry "
                     "recovery=%llu retry=%lld expected=%u",
                     (unsigned long long)spa_pcm_sink.detachedRecoveries,
                     (long long)written, (unsigned)bytes);
        if (clockEvent > 0)
            LOG_INFO(LOG_MODULE,
                     "audio sink: detached QSA clock measured %.6f Hz over %u writes",
                     clockRate, (unsigned)SPA_FEEDBACK_CLOCK_WINDOW_WRITES);
        else if (clockEvent < 0 && !autoRecovery)
            LOG_WARN(LOG_MODULE,
                     "audio sink: detached QSA clock window reset discontinuities=%llu",
                     (unsigned long long)spa_pcm_sink.detachedClockDiscontinuities);
        if (logWrite)
            LOG_WARN(LOG_MODULE,
                     "audio sink: detached writes=%llu zero=%llu recoveries=%llu "
                     "last=%lld expected=%u status=%d underruns=%llu "
                     "clock=%.6f/%s clockResets=%llu handoff=%s",
                     (unsigned long long)spa_pcm_sink.detachedWrites,
                     (unsigned long long)spa_pcm_sink.detachedZeroWrites,
                     (unsigned long long)spa_pcm_sink.detachedRecoveries,
                     (long long)written, (unsigned)bytes,
                     spa_pcm_sink.deviceStatus,
                     (unsigned long long)spa_pcm_sink.deviceUnderruns,
                     clockRate, clockValid ? "measured" : "warming",
                     (unsigned long long)spa_pcm_sink.detachedClockDiscontinuities,
                     needsPrepare ? "prepared" : "continuous");
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);

        if (firstWrite)
            STATE_TRACE("QSA_DETACHED_FIRST_WRITE",
                        "result=%lld expected=%u prepared=%d auto_recovery=%d",
                        (long long)written, (unsigned)bytes,
                        needsPrepare ? 1 : 0, autoRecovery ? 1 : 0);
        if (autoRecovery || written != (ssize_t)bytes || clockEvent < 0)
            STATE_TRACE("QSA_WRITE_ANOMALY",
                        "owner=detached result=%lld expected=%u auto_recovery=%d "
                        "clock_reset=%d gap_ms=%llu",
                        (long long)written, (unsigned)bytes,
                        autoRecovery ? 1 : 0, clockEvent < 0 ? 1 : 0,
                        (unsigned long long)(writeGapNs / UINT64_C(1000000)));
#if ENABLE_STATE_TRACE
        if (traceCadenceGap)
            spa103_trace_qsa_gap(writeGapNs, traceWrites,
                                 traceClockResets, autoRecovery);
        if (traceDeviceUnderrun)
            STATE_TRACE("QSA_DEVICE_UNDERRUN",
                        "owner=detached writes=%llu status=%d device_underruns=%llu",
                        (unsigned long long)traceWrites, traceDeviceStatus,
                        (unsigned long long)traceDeviceUnderruns);
        if (traceHealth)
            spa103_trace_qsa_health(
                "detached", traceWrites, traceZeroWrites, 0,
                traceRecoveries, traceDeviceStatus, traceDeviceUnderruns,
                0, traceRingWaits, traceClockResets, 0, 0);
#endif

        if (written != (ssize_t)bytes) return;
}

static unsigned spa_audio_requested_output_channels(void)
{
    const spa_audio_config_t *config = spa_audio_config();

    if (!config->realtimePCM) return 0;
    return config->atmos && config->atmosMode == SPA_ATMOS_PASSTHROUGH ?
           SPA_PCM_5_1_2_CHANNELS : SPA_PCM_5_1_CHANNELS;
}

/* ── dio::CQNXSoundIF::createMetadata ───────────────────────────────────────
 *
 * The SAD (speaker-allocation) header that rides beside the PCM on MOST is built here and
 * pushed to the driver with DCMD_PCM_SET_SAD_METADATA.  deva logs it as
 * "convert_sad_header_to_csd: ... asad=0x%x ... sample_width=0x%x".
 *
 * We widen the ASBD to six channels, but this call still carries the stock stereo count, so
 * the header announces asad=0xc0 -- two active channels -- while six slots are actually
 * formed.  The working internal-media path announces asad=0xfc with sample_width=24.
 *
 * The function documents its own arguments in its log string:
 *     "[%s]<AUDIO>nChannels: %u, nBitsPerChannel: %u, nSampleRate: %u"
 * so a2/a3/a4 are channels, bits, rate.  Both template instantiations are exported weak, and
 * libairplay reaches them through its PLT, which is the same seam SessionSetup uses.
 *
 * AUDIO_SAD_CHANNELS / AUDIO_SAD_BITS override the forced values; 0 disables the override. */
typedef int (*spa_create_meta_t)(void *, uint32_t, uint32_t, uint32_t, int, int);

static unsigned spa_env_uint(const char *name, unsigned fallback)
{
    const char *v = getenv(name);
    char *end;
    unsigned long n;
    if (!v || !*v) return fallback;
    n = strtoul(v, &end, 0);
    if (*end) return fallback;
    return (unsigned)n;
}

static int spa_create_metadata_common(spa_create_meta_t real, const char *which,
                                      void *self, uint32_t channels, uint32_t bits,
                                      uint32_t rate, int encryption, int codec)
{
    unsigned want = spa_audio_requested_output_channels();
    uint32_t forcedCh = channels, forcedBits = bits;

    if (want) {
        forcedCh   = spa_env_uint("AUDIO_SAD_CHANNELS", want);
        forcedBits = spa_env_uint("AUDIO_SAD_BITS", 24u);
        if (!forcedCh)   forcedCh = channels;
        if (!forcedBits) forcedBits = bits;
    }
    if (forcedCh != channels || forcedBits != bits)
        LOG_WARN(LOG_MODULE,
                 "createMetadata(%s): announcing %uch/%ubit instead of %uch/%ubit "
                 "at %u Hz -- the SAD header is what the MOST side reads",
                 which, (unsigned)forcedCh, (unsigned)forcedBits,
                 (unsigned)channels, (unsigned)bits, (unsigned)rate);
    else
        LOG_INFO(LOG_MODULE, "createMetadata(%s): %uch/%ubit/%uHz unchanged",
                 which, (unsigned)channels, (unsigned)bits, (unsigned)rate);
    return real(self, forcedCh, forcedBits, rate, encryption, codec);
}

#define SPA_CREATE_META_SYM_ST \
    "_ZN3dio11CQNXSoundIFINS_18SingleThreadPolicyEE14createMetadataEjjjNS_15en_encryption_tENS_10en_codec_tE"
#define SPA_CREATE_META_SYM_OL \
    "_ZN3dio11CQNXSoundIFINS_16ObjectLockPolicyILNS_6CMutex10en_mutex_tE2ELNS_14en_lock_type_tE0EEEE14createMetadataEjjjNS_15en_encryption_tENS_10en_codec_tE"

int _ZN3dio11CQNXSoundIFINS_18SingleThreadPolicyEE14createMetadataEjjjNS_15en_encryption_tENS_10en_codec_tE(
        void *self, uint32_t ch, uint32_t bits, uint32_t rate, int enc, int codec)
{
    static spa_create_meta_t real;
    if (!real) real = (spa_create_meta_t)dlsym(RTLD_NEXT, SPA_CREATE_META_SYM_ST);
    if (!real) return 0;
    return spa_create_metadata_common(real, "single", self, ch, bits, rate, enc, codec);
}

int _ZN3dio11CQNXSoundIFINS_16ObjectLockPolicyILNS_6CMutex10en_mutex_tE2ELNS_14en_lock_type_tE0EEEE14createMetadataEjjjNS_15en_encryption_tENS_10en_codec_tE(
        void *self, uint32_t ch, uint32_t bits, uint32_t rate, int enc, int codec)
{
    static spa_create_meta_t real;
    if (!real) real = (spa_create_meta_t)dlsym(RTLD_NEXT, SPA_CREATE_META_SYM_OL);
    if (!real) return 0;
    return spa_create_metadata_common(real, "locked", self, ch, bits, rate, enc, codec);
}

static void spa_pcm_sink_reset_stock_counters_locked(void)
{
    spa_pcm_sink.writes = 0;
    spa_pcm_sink.setupCalls = 0;
#if ENABLE_STATE_TRACE
    spa_pcm_sink.firstSuccessfulWriteTraced = 0;
    spa_pcm_sink.detachedRingWaitReported = 0;
    spa_pcm_sink.detachedRingWaits = 0;
    spa_pcm_sink.tracedDeviceUnderruns = 0;
#endif
    spa_pcm_sink.bufferedWrites = 0;
    spa_pcm_sink.underrunFrames = 0;
    spa_pcm_sink.shortRingFallbacks = 0;
    spa_pcm_sink.avoidedSilenceFrames = 0;
    spa_pcm_sink.lastShortRingFrames = 0;
    spa_pcm_sink.bufferedSilencePeriods = 0;
    spa_pcm_sink.bufferedSilenceFrames = 0;
    spa_pcm_sink.shortWrites = 0;
    spa_pcm_sink.zeroWrites = 0;
    spa_pcm_sink.lastWriteResult = 0;
    spa_pcm_sink.oddSizeWrites = 0;
    spa_pcm_sink.lastOddSize = 0;
    spa_pcm_sink.deviceStatus = -1;
    spa_pcm_sink.deviceUnderruns = 0;
}

/* A buffered stream can be the first and only media stream after connection.
 * Stock 210 then never constructs CQNXSoundIF, so the old bridge had decoded
 * PCM but no QSA handle.  Open the same named sink from the already-existing
 * APAP receiver thread after the 400-ms prime cushion is ready.  ioLock makes
 * this atomic with a concurrent stock open; if type 100 arrives later its
 * normal open interposer reclaims this exact handle without a second device
 * open or a second ring consumer. */
static int spa_pcm_sink_bootstrap_buffered(void)
{
    static spa_snd_open_name_t realOpen;
    static spa_snd_disable_t realDisable;
    static spa_snd_params_t realParams;
    static spa_snd_prepare_t realPrepare;
    static spa_snd_setup_t realSetup;
    static spa_snd_close_t realClose;
    snd_pcm_channel_params_t params;
    snd_pcm_channel_setup_t setup;
    snd_pcm_t *handle = 0;
    int16_t *scratch = 0;
    const char *stage = "resolve";
    uint64_t nowNs, attempt, failures = 0;
    unsigned channels;
    size_t bytes, pending;
    int result = -1;

    if (!spa_cold_connect_enabled()) return 0;
    channels = spa_buffered_pcm_channels();
    if (!channels || channels > SPA_103_RENDER_MAX_CHANNELS) return 0;
    pending = spa_buffered_pcm_pending(channels);
    if (pending < SPA_PCM_SINK_PERIOD_FRAMES) return 0;
    bytes = (size_t)SPA_PCM_SINK_PERIOD_FRAMES * channels * sizeof(int16_t);
    nowNs = spa_monotonic_ns();

    pthread_mutex_lock(&spa_pcm_sink.ioLock);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.handle) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        return 1;
    }
    if (nowNs && spa_pcm_sink.coldOpenRetryAfterNs &&
        nowNs < spa_pcm_sink.coldOpenRetryAfterNs) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        return 0;
    }
    attempt = ++spa_pcm_sink.coldOpenAttempts;
    spa_pcm_sink.coldOpenRetryAfterNs =
        nowNs ? nowNs + UINT64_C(1000000000) : 0;
    pthread_mutex_unlock(&spa_pcm_sink.lock);

    /* QSA only transports PCM.  Stock AudioStreamPrepare first declares
     * kAudioTypeMedia through dio_manager so HMI/DSI route DIO media to the
     * active output.  A hook-owned type-103 stream otherwise skips that layer
     * completely, which the vehicle A/B exposed as successful silent writes. */
    stage = "media-route";
    if (!spa_pcm_sink_request_hook_media_route("cold_buffered",
                                               "cold_bootstrap")) {
        result = -1;
        goto fail;
    }

    if (!realOpen)
        realOpen = (spa_snd_open_name_t)dlsym(RTLD_NEXT, "snd_pcm_open_name");
    if (!realDisable)
        realDisable = (spa_snd_disable_t)dlsym(RTLD_NEXT,
                                               "snd_pcm_plugin_set_disable");
    if (!realParams)
        realParams = (spa_snd_params_t)dlsym(RTLD_NEXT,
                                             "snd_pcm_plugin_params");
    if (!realPrepare)
        realPrepare = (spa_snd_prepare_t)dlsym(RTLD_NEXT,
                                               "snd_pcm_plugin_prepare");
    if (!realSetup)
        realSetup = (spa_snd_setup_t)dlsym(RTLD_NEXT,
                                           "snd_pcm_plugin_setup");
    if (!realClose)
        realClose = (spa_snd_close_t)dlsym(RTLD_NEXT, "snd_pcm_close");
    if (!realOpen || !realDisable || !realParams || !realPrepare ||
        !realSetup || !realClose)
        goto fail;

    stage = "open";
    result = realOpen(&handle, (char *)SPA_PCM_SINK_NAME,
                      SND_PCM_OPEN_PLAYBACK);
    if (result != 0 || !handle) goto fail;

    /* MMAP must be disabled before params on this exact QSA stack; the same
     * ordering is vehicle-proven by qsa_play and RetroArch. */
    (void)realDisable(handle, PLUGIN_DISABLE_MMAP);
    stage = "device-width";
    if (!spa_sink_device_supports_voices(handle, channels)) {
        result = -1;
        goto fail;
    }

    memset(&params, 0, sizeof(params));
    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_BLOCK;
    /* START_FULL lets all eight requested periods prefill before the device
     * timer consumes slot 1.  The decoded ring is already much deeper than
     * those eight periods when this cold bootstrap is attempted. */
    params.start_mode = SND_PCM_START_FULL;
    params.stop_mode = SND_PCM_STOP_STOP;
    params.format.interleave = 1;
    params.format.format = SND_PCM_SFMT_S16_LE;
    params.format.rate = SPA_SR;
    params.format.voices = (int32_t)channels;
    params.buf.block.frag_size = (int32_t)bytes;
    params.buf.block.frags_min = 1;
    params.buf.block.frags_max = SPA_PCM_SINK_MIN_FRAGS;

    stage = "params";
    result = realParams(handle, &params);
    if (result != 0) goto fail;
    stage = "prepare";
    result = realPrepare(handle, SND_PCM_CHANNEL_PLAYBACK);
    if (result != 0) goto fail;

    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    stage = "setup";
    result = realSetup(handle, &setup);
    if (result != 0 || setup.mode != SND_PCM_MODE_BLOCK ||
        setup.format.format != SND_PCM_SFMT_S16_LE ||
        setup.format.rate != SPA_SR ||
        setup.format.voices != (int32_t)channels ||
        setup.buf.block.frag_size != (int32_t)bytes) {
        if (result == 0) result = -1;
        goto fail;
    }

    stage = "scratch";
    scratch = (int16_t *)malloc(bytes);
    if (!scratch) {
        result = -1;
        goto fail;
    }

    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (spa_pcm_sink.handle) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        stage = "lost-race";
        result = -1;
        goto fail;
    }
    free(spa_pcm_sink.scratch);
    spa_pcm_sink.scratch = scratch;
    scratch = 0;
    spa_pcm_sink.scratchBytes = bytes;
    spa_pcm_sink.handle = handle;
    spa_pcm_sink.sourceFormat = SND_PCM_SFMT_S16_LE;
    spa_pcm_sink.sourceChannels = channels;
    spa_pcm_sink.outputChannels = channels;
    spa_pcm_sink.expandedParams = 1;
    spa_pcm_sink.detached = 1;
    spa_pcm_sink.closeFlushSuppressed = 0;
    spa_pcm_sink.closeFlushChannel = 0;
    spa_pcm_sink.detachedNeedsPrepare = 0;
    spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
    spa_pcm_sink_reset_stock_counters_locked();
    spa_pcm_sink.detachedWrites = 0;
    spa_pcm_sink.detachedZeroWrites = 0;
    spa_pcm_sink.detachedRecoveries = 0;
    spa_pcm_sink_reset_detached_clock_locked();
    spa_pcm_sink.suppressedFlushes = 0;
    spa_pcm_sink.reclaimedOpens = 0;
    spa_pcm_sink.skippedReclaimParams = 0;
    spa_pcm_sink.skippedReclaimPrepares = 0;
    spa_pcm_sink.coldOpenRetryAfterNs = 0;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);

    LOG_WARN(LOG_MODULE,
             "audio sink: cold buffered bootstrap opened %s handle=%p "
             "%uHz/%uch/%u-byte period pending=%u attempt=%llu",
             SPA_PCM_SINK_NAME, handle, (unsigned)SPA_SR, channels,
             (unsigned)bytes, (unsigned)pending,
             (unsigned long long)attempt);
    STATE_TRACE("QSA_OPEN",
                "sink=%s action=cold_buffered result=0 handle=%p mode=0x%x "
                "channels=%u rate=%u fragment=%u frags=%u pending=%u attempt=%llu",
                SPA_PCM_SINK_NAME, handle, (unsigned)SND_PCM_OPEN_PLAYBACK,
                channels, (unsigned)SPA_SR, (unsigned)bytes,
                (unsigned)SPA_PCM_SINK_MIN_FRAGS, (unsigned)pending,
                (unsigned long long)attempt);
    STATE_TRACE("QSA_SETUP",
                "owner=cold_buffered result=0 target_channels=%u rate=%d "
                "target_fragment=%d frags=%d",
                channels, (int)setup.format.rate,
                (int)setup.buf.block.frag_size, (int)setup.buf.block.frags);
    return 1;

fail:
    free(scratch);
    if (handle && realClose) (void)realClose(handle);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    failures = ++spa_pcm_sink.coldOpenFailures;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);
    if (spa103_trace_milestone(failures))
        STATE_TRACE("QSA_COLD_BOOTSTRAP",
                    "stage=%s result=%d attempt=%llu failures=%llu channels=%u "
                    "pending=%u",
                    stage, result, (unsigned long long)attempt,
                    (unsigned long long)failures, channels, (unsigned)pending);
    LOG_WARN(LOG_MODULE,
             "audio sink: cold buffered bootstrap failed stage=%s result=%d "
             "attempt=%llu; retrying in one second",
             stage, result, (unsigned long long)attempt);
    return 0;
}

int snd_pcm_open_name(snd_pcm_t **handle, char *name, int mode)
{
    static spa_snd_open_name_t real;
    snd_pcm_t *reclaimedHandle = 0;
    uint64_t detachedWrites = 0, detachedZeroWrites = 0;
    int routeHandoff = 0;
    int sinkPlayback;
    int result;

    if (!real) real = (spa_snd_open_name_t)dlsym(RTLD_NEXT, "snd_pcm_open_name");
    if (!real) return -1;

    sinkPlayback = handle && name && !strcmp(name, SPA_PCM_SINK_NAME) &&
                   (mode & SND_PCM_OPEN_PLAYBACK);
    if (sinkPlayback) {
        int candidate;

        /* Stock creates a new CQNXSoundIF object on foregrounding, but the
         * previous object's QSA handle is still valid and is being clocked by
         * the buffered receiver.  Hand that same pointer to the new object.
         * ioLock makes the ownership change land between complete 768-frame
         * writes, so the ring has exactly one consumer throughout. */
        pthread_mutex_lock(&spa_pcm_sink.ioLock);
        pthread_mutex_lock(&spa_pcm_sink.lock);
        spa_pcm_sink.detachedRouteReacquireAfterNs = 0;
        if (spa_pcm_sink.hookMediaRouteRequested) {
            /* AudioStreamPrepare queued its own kAudioTypeMedia declaration
             * before reaching snd_pcm_open_name.  Stock now owns the matching
             * torn-down callback, whether the hook acquired the prior route
             * during cold bootstrap or after an earlier type-100 close. */
            spa_pcm_sink.hookMediaRouteRequested = 0;
            routeHandoff = 1;
        }
        candidate = spa_pcm_sink.detached && spa_pcm_sink.handle &&
                    spa_pcm_sink.expandedParams;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        if (candidate && spa_buffered_pcm_active()) {
            pthread_mutex_lock(&spa_pcm_sink.lock);
            if (spa_pcm_sink.detached && spa_pcm_sink.handle &&
                spa_pcm_sink.expandedParams) {
                reclaimedHandle = spa_pcm_sink.handle;
                detachedWrites = spa_pcm_sink.detachedWrites;
                detachedZeroWrites = spa_pcm_sink.detachedZeroWrites;
                spa_pcm_sink.detached = 0;
                spa_pcm_sink.detachedNeedsPrepare = 0;
                spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_OPEN;
                spa_pcm_sink.reclaimedOpens++;
                spa_pcm_sink_reset_stock_counters_locked();
                *handle = reclaimedHandle;
            }
            pthread_mutex_unlock(&spa_pcm_sink.lock);
        }
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        if (routeHandoff)
            STATE_TRACE("AUDIO_ROUTE",
                        "action=handoff from=hook_buffered to=stock result=owned");
        if (reclaimedHandle) {
            LOG_WARN(LOG_MODULE,
                     "audio sink: reclaimed live %s handle=%p for stock type-100 "
                     "without close/open; detachedWrites=%llu zero=%llu",
                     SPA_PCM_SINK_NAME, reclaimedHandle,
                     (unsigned long long)detachedWrites,
                     (unsigned long long)detachedZeroWrites);
            STATE_TRACE("QSA_OPEN",
                        "sink=%s action=reclaim result=0 handle=%p "
                        "detached_writes=%llu detached_zero=%llu",
                        SPA_PCM_SINK_NAME, reclaimedHandle,
                        (unsigned long long)detachedWrites,
                        (unsigned long long)detachedZeroWrites);
            return 0;
        }
        spa_pcm_sink_close_detached("stock type-100 reopened");
    }
    result = real(handle, name, mode);
    if (result == 0 && handle && *handle && sinkPlayback) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        spa_pcm_sink.handle = *handle;
        spa_pcm_sink.sourceChannels = 0;
        spa_pcm_sink.outputChannels = 0;
        spa_pcm_sink.expandedParams = 0;
        spa_pcm_sink.detached = 0;
        spa_pcm_sink.closeFlushSuppressed = 0;
        spa_pcm_sink.closeFlushChannel = 0;
        spa_pcm_sink.detachedNeedsPrepare = 0;
        spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
        spa_pcm_sink_reset_stock_counters_locked();
        spa_pcm_sink.detachedWrites = 0;
        spa_pcm_sink.detachedZeroWrites = 0;
        spa_pcm_sink.detachedRecoveries = 0;
        spa_pcm_sink_reset_detached_clock_locked();
        spa_pcm_sink.suppressedFlushes = 0;
        spa_pcm_sink.reclaimedOpens = 0;
        spa_pcm_sink.skippedReclaimParams = 0;
        spa_pcm_sink.skippedReclaimPrepares = 0;
        spa_pcm_sink.coldOpenAttempts = 0;
        spa_pcm_sink.coldOpenFailures = 0;
        spa_pcm_sink.coldOpenRetryAfterNs = 0;
        spa_pcm_sink.detachedRouteReacquireAfterNs = 0;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        LOG_INFO(LOG_MODULE, "audio sink: captured %s playback handle=%p",
                 SPA_PCM_SINK_NAME, *handle);
    }
    if (sinkPlayback)
        STATE_TRACE("QSA_OPEN",
                    "sink=%s action=real result=%d handle_valid=%d handle=%p mode=0x%x",
                    SPA_PCM_SINK_NAME, result,
                    result == 0 && handle && *handle ? 1 : 0,
                    handle ? *handle : (snd_pcm_t *)0, (unsigned)mode);
    return result;
}

/* Ask the DEVICE what it can do, not the plugin layer.
 *
 * snd_pcm_plugin_setup reflects the CLIENT's geometry back: measured in the QNX
 * harness (tools/qsa-emu), a stereo-only mpl5_dio_ent accepted a six-voice
 * plugin_params AND reported "6 voices, frag 9216" from plugin_setup, because
 * QSA had quietly inserted a channel conversion.  Both guards below used to rest
 * on that readback, so a unit whose deva patch had not taken effect would report
 * perfect health -- zero underruns, right rate, right fragment -- while a hidden
 * downmix ran underneath.
 *
 * snd_pcm_channel_info reports the hardware's own limits.  In the same harness it
 * said voices 2..2 against the stock driver and 2..6 against the patched one,
 * which is exactly the distinction that was missing. */
static int spa_sink_device_supports_voices(snd_pcm_t *handle, unsigned voices)
{
    static int (*real_info)(snd_pcm_t *, snd_pcm_channel_info_t *);
    snd_pcm_channel_info_t info;

    if (!real_info)
        real_info = (int (*)(snd_pcm_t *, snd_pcm_channel_info_t *))
                    dlsym(RTLD_NEXT, "snd_pcm_channel_info");
    /* Unavailable: say yes rather than silently disabling Spatial on a build
     * where the symbol cannot be resolved. */
    if (!real_info) return 1;

    memset(&info, 0, sizeof(info));
    info.channel = SND_PCM_CHANNEL_PLAYBACK;
    if (real_info(handle, &info) != 0) return 1;

    LOG_INFO(LOG_MODULE,
             "audio sink: device reports voices %d..%d, rate %d..%d, frag %d..%d, formats %#x",
             (int)info.min_voices, (int)info.max_voices,
             (int)info.min_rate, (int)info.max_rate,
             (int)info.min_fragment_size, (int)info.max_fragment_size,
             (unsigned)info.formats);

    if (info.max_voices > 0 && info.max_voices < (int)voices) {
        LOG_ERROR(LOG_MODULE,
                  "audio sink: device tops out at %d voices, %u requested -- the deva "
                  "patch is not in effect; refusing to expand (plugin_setup would "
                  "have claimed success)",
                  (int)info.max_voices, voices);
        return 0;
    }
    return 1;
}

int snd_pcm_plugin_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params)
{
    static spa_snd_params_t real;
    snd_pcm_channel_params_t adjusted;
    unsigned outputChannels, existingOutputChannels = 0;
    size_t sourceFrameBytes, outputFrameBytes;
    int isSink, reclaimOpen = 0, result, renegotiated;

    if (!real) real = (spa_snd_params_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_params");
    if (!real) return -1;

    outputChannels = spa_audio_requested_output_channels();
    pthread_mutex_lock(&spa_pcm_sink.lock);
    isSink = handle && handle == spa_pcm_sink.handle;
    if (isSink) {
        reclaimOpen = spa_pcm_sink.reclaimStage == SPA_PCM_RECLAIM_OPEN &&
                      spa_pcm_sink.expandedParams;
        existingOutputChannels = spa_pcm_sink.outputChannels;
        if (!reclaimOpen) spa_pcm_sink.expandedParams = 0;
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);

    /* A reclaimed handle is already configured to this physical PCM6 geometry.
     * Reapplying plugin_params tears down the QSA queue even when every value is
     * identical.  Validate every stock-facing field whose type is known here,
     * update only the logical source geometry used by our write translation,
     * and let the following real plugin_setup report the live device state. */
    if (reclaimOpen) {
        int compatible = params && outputChannels &&
                         outputChannels == existingOutputChannels &&
                         params->channel == SND_PCM_CHANNEL_PLAYBACK &&
                         params->mode == SND_PCM_MODE_BLOCK &&
                         params->format.format == SND_PCM_SFMT_S16_LE &&
                         params->format.rate == SPA_SR &&
                         params->format.voices >= 1 &&
                         params->format.voices <= SPA_PCM_5_1_2_CHANNELS;

        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (compatible && handle == spa_pcm_sink.handle &&
            spa_pcm_sink.reclaimStage == SPA_PCM_RECLAIM_OPEN &&
            spa_pcm_sink.expandedParams &&
            spa_pcm_sink.outputChannels == outputChannels) {
            spa_pcm_sink.sourceFormat = params->format.format;
            spa_pcm_sink.sourceChannels = (unsigned)params->format.voices;
            spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_PARAMS;
            spa_pcm_sink.skippedReclaimParams++;
            params->why_failed = 0;
        } else {
            compatible = 0;
            if (handle == spa_pcm_sink.handle) {
                spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
                spa_pcm_sink.expandedParams = 0;
            }
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        if (compatible) {
            LOG_WARN(LOG_MODULE,
                     "audio sink: reclaimed params kept live handle=%p "
                     "source=%dch target=%uch rate=%d format=%d frag=%d",
                     handle, (int)params->format.voices, outputChannels,
                     (int)params->format.rate, params->format.format,
                     (int)params->buf.block.frag_size);
            STATE_TRACE("QSA_PARAMS",
                        "action=reclaim result=0 source_channels=%d target_channels=%u "
                        "rate=%d format=%d fragment=%d",
                        (int)params->format.voices, outputChannels,
                        (int)params->format.rate, params->format.format,
                        (int)params->buf.block.frag_size);
            return 0;
        }
        LOG_WARN(LOG_MODULE,
                 "audio sink: reclaimed handle geometry changed; applying real params");
    }
    if (!isSink || !params || !outputChannels ||
        params->channel != SND_PCM_CHANNEL_PLAYBACK ||
        params->mode != SND_PCM_MODE_BLOCK ||
        params->format.format != SND_PCM_SFMT_S16_LE ||
        params->format.rate != SPA_SR ||
        params->format.voices < 1 || params->format.voices > SPA_PCM_5_1_2_CHANNELS)
        return real(handle, params);

    /* Before touching anything, confirm the hardware really has the voices.  A
     * plugin-level "yes" is not evidence. */
    if (!spa_sink_device_supports_voices(handle, outputChannels)) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle) spa_pcm_sink.expandedParams = 0;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        STATE_TRACE("QSA_PARAMS",
                    "action=device_width_reject requested_target_channels=%u",
                    outputChannels);
        return real(handle, params);
    }

    adjusted = *params;
    adjusted.format.voices = (int32_t)outputChannels;
    sourceFrameBytes = (size_t)params->format.voices * sizeof(int16_t);
    outputFrameBytes = (size_t)outputChannels * sizeof(int16_t);
    if (adjusted.mode == SND_PCM_MODE_BLOCK && adjusted.buf.block.frag_size > 0) {
        size_t fragmentBytes = (size_t)adjusted.buf.block.frag_size;
        if (fragmentBytes % sourceFrameBytes != 0 ||
            fragmentBytes / sourceFrameBytes > SPA_PCM_SINK_MAX_WRITE / outputFrameBytes) {
            LOG_ERROR(LOG_MODULE,
                      "audio sink: fragment size %u is not safe for %u->%u channels; "
                      "leaving stock params",
                      (unsigned)fragmentBytes, (unsigned)params->format.voices,
                      outputChannels);
            return real(handle, params);
        }
        adjusted.buf.block.frag_size =
            (int32_t)((fragmentBytes / sourceFrameBytes) * outputFrameBytes);
        /* Widening the fragment without touching the fragment COUNT leaves the
         * ring as deep as stock sized it for a fragment a third this size.  The
         * RetroArch QNX driver, tuned on this exact head unit, records that the
         * device needs at least eight fragments or the CSD session start/stops
         * around render jitter -- audible as a periodic tick rather than a
         * dropout.  deva's caps allow ten of the widened fragment
         * (max_buffer 0x2D000 = 10 * 0x4800), so ask for eight when stock asked
         * for fewer; never reduce a deeper request. */
        if (adjusted.buf.block.frags_max < SPA_PCM_SINK_MIN_FRAGS)
            adjusted.buf.block.frags_max = SPA_PCM_SINK_MIN_FRAGS;
        if (adjusted.buf.block.frags_min > adjusted.buf.block.frags_max)
            adjusted.buf.block.frags_min = adjusted.buf.block.frags_max;
    }
    result = real(handle, &adjusted);
    /* Never let our widening be the reason CarPlay ends up with no audio at all.
     * If the device refuses the expanded geometry -- the eight-fragment floor is
     * a new way that can happen -- hand it back exactly what stock asked for and
     * carry on in stereo. */
    if (result != 0) {
        LOG_ERROR(LOG_MODULE,
                  "audio sink: expanded params rejected (why=%#x); reopening stock %dch",
                  (unsigned)adjusted.why_failed, (int)params->format.voices);
        result = real(handle, params);
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle) spa_pcm_sink.expandedParams = 0;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        STATE_TRACE("QSA_PARAMS",
                    "action=expanded_reject_fallback result=%d source_channels=%d "
                    "target_channels=%u rate=%d why=0x%x",
                    result, (int)params->format.voices, outputChannels,
                    (int)params->format.rate, (unsigned)adjusted.why_failed);
        return result;
    }

    /* This exact QSA plugin build writes its negotiated values back into params,
     * although the public QNX contract calls these fields write-only.  Use that
     * observed behavior as an early, pre-sample guard and confirm it again from
     * the authoritative setup readback below. */
    renegotiated = adjusted.format.rate != params->format.rate ||
                   adjusted.format.voices != (int32_t)outputChannels;
    /* mpl5's hardware timer consumes one fragment every fixed 16 ms.  The bad
     * caps-only deva returned 3072 client bytes = 256 S16/6ch frames per tick,
     * hence 16 kframes/s and exactly 3x-slow playback.  The dynamic deva patch
     * raises the internal S32 fragment to 0x4800 for six voices; QSA exposes it
     * here as 9216 S16 bytes = 768 frames, exactly 16 ms at 48 kHz.  The caller's
     * original 480-frame request is not the clock contract: setup translates the
     * negotiated 768-frame period back to a 3072-byte stereo-facing fragment. */
    if (!renegotiated && adjusted.mode == SND_PCM_MODE_BLOCK &&
        adjusted.buf.block.frag_size > 0 && params->buf.block.frag_size > 0) {
        size_t gotFrames = (size_t)adjusted.buf.block.frag_size / outputFrameBytes;
        if ((size_t)adjusted.buf.block.frag_size % outputFrameBytes != 0 ||
            gotFrames != SPA_PCM_SINK_PERIOD_FRAMES) {
            LOG_ERROR(LOG_MODULE,
                      "audio sink: device fragment carries %u frames, expected %u per %u ms "
                      "(%d bytes at %uch); expansion abandoned",
                      (unsigned)gotFrames, (unsigned)SPA_PCM_SINK_PERIOD_FRAMES,
                      (unsigned)SPA_PCM_SINK_PERIOD_MS,
                      (int)adjusted.buf.block.frag_size, outputChannels);
            renegotiated = 1;
        }
    }
    if (result == 0 && renegotiated) {
        LOG_ERROR(LOG_MODULE,
                  "audio sink: QSA renegotiated rate %d->%d voices %d->%d frag %d->%d; "
                  "expansion abandoned, reopening stock %dch",
                  (int)params->format.rate, (int)adjusted.format.rate,
                  (int)outputChannels, (int)adjusted.format.voices,
                  (int)params->buf.block.frag_size, (int)adjusted.buf.block.frag_size,
                  (int)params->format.voices);
        result = real(handle, params);
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle) spa_pcm_sink.expandedParams = 0;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        STATE_TRACE("QSA_PARAMS",
                    "action=renegotiated_fallback result=%d source_channels=%d "
                    "target_channels=%u actual_channels=%d actual_rate=%d",
                    result, (int)params->format.voices, outputChannels,
                    (int)adjusted.format.voices, (int)adjusted.format.rate);
        return result;
    }

    pthread_mutex_lock(&spa_pcm_sink.lock);
    if (handle == spa_pcm_sink.handle) {
        spa_pcm_sink.sourceFormat = params->format.format;
        spa_pcm_sink.sourceChannels = (unsigned)params->format.voices;
        spa_pcm_sink.outputChannels = outputChannels;
        spa_pcm_sink.expandedParams = result == 0;
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (result != 0) params->why_failed = adjusted.why_failed;
    LOG_INFO(LOG_MODULE,
             "audio sink: params source=%uch target=%uch rate=%d->%d format=%d "
             "frag=%d->%d frags=%d..%d result=%d why=%#x",
             (unsigned)params->format.voices, outputChannels,
             (int)params->format.rate, (int)adjusted.format.rate,
             params->format.format,
             (int)params->buf.block.frag_size, (int)adjusted.buf.block.frag_size,
             (int)adjusted.buf.block.frags_min, (int)adjusted.buf.block.frags_max,
             result, (unsigned)adjusted.why_failed);
    STATE_TRACE("QSA_PARAMS",
                "action=expanded result=%d source_channels=%d target_channels=%u "
                "source_rate=%d target_rate=%d format=%d source_fragment=%d "
                "target_fragment=%d frags_min=%d frags_max=%d why=0x%x",
                result, (int)params->format.voices, outputChannels,
                (int)params->format.rate, (int)adjusted.format.rate,
                params->format.format, (int)params->buf.block.frag_size,
                (int)adjusted.buf.block.frag_size,
                (int)adjusted.buf.block.frags_min,
                (int)adjusted.buf.block.frags_max,
                (unsigned)adjusted.why_failed);
    return result;
}

int snd_pcm_plugin_prepare(snd_pcm_t *handle, int channel)
{
    static spa_snd_prepare_t real;
    static int (*realStatus)(snd_pcm_t *, snd_pcm_channel_status_t *);
    snd_pcm_channel_status_t status;
    int reclaimPrepare = 0, detachedRecovery = 0, statusResult = -1, skip = 0;

    if (!real)
        real = (spa_snd_prepare_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_prepare");
    if (!real) return -1;

    pthread_mutex_lock(&spa_pcm_sink.lock);
    reclaimPrepare = handle && handle == spa_pcm_sink.handle &&
                     channel == SND_PCM_CHANNEL_PLAYBACK &&
                     spa_pcm_sink.reclaimStage == SPA_PCM_RECLAIM_PARAMS &&
                     spa_pcm_sink.expandedParams;
    detachedRecovery = handle && handle == spa_pcm_sink.handle &&
                       channel == SND_PCM_CHANNEL_PLAYBACK &&
                       spa_pcm_sink.detached && spa_pcm_sink.expandedParams &&
                       spa_pcm_sink.detachedWrites != 0;
    if (detachedRecovery) spa_pcm_sink.detachedRecoveries++;
    pthread_mutex_unlock(&spa_pcm_sink.lock);

    if (detachedRecovery)
        LOG_WARN(LOG_MODULE,
                 "audio sink: QSA requested prepare during detached playback -- "
                 "device underrun recovery=%llu",
                 (unsigned long long)spa_pcm_sink.detachedRecoveries);

    /* An iOS FLUSHBUFFERED can intentionally leave the detached QSA stream in
     * UNDERRUN before type-100 returns.  Reusing the handle is still correct,
     * but skipping prepare in that state only makes the first write return 0
     * and pushes the same prepare into CQNXSoundIF's recovery path.  Skip only
     * when the device itself says it is still running. */
    memset(&status, 0, sizeof(status));
    status.channel = channel;
    if (reclaimPrepare) {
        if (!realStatus)
            realStatus = (int (*)(snd_pcm_t *, snd_pcm_channel_status_t *))
                         dlsym(RTLD_NEXT, "snd_pcm_plugin_status");
        if (realStatus) statusResult = realStatus(handle, &status);

        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle &&
            spa_pcm_sink.reclaimStage == SPA_PCM_RECLAIM_PARAMS &&
            spa_pcm_sink.expandedParams) {
            spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
            if (statusResult == 0 && status.status == SND_PCM_STATUS_RUNNING) {
                spa_pcm_sink.skippedReclaimPrepares++;
                skip = 1;
            }
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
    }

    if (skip) {
        LOG_WARN(LOG_MODULE,
                 "audio sink: reclaimed prepare kept live handle=%p channel=%d",
                 handle, channel);
        return 0;
    }
    if (reclaimPrepare)
        LOG_WARN(LOG_MODULE,
                 "audio sink: reclaimed handle requires real prepare statusResult=%d "
                 "status=%d underruns=%u",
                 statusResult, statusResult == 0 ? status.status : -1,
                 statusResult == 0 ? (unsigned)status.underrun : 0u);
    return real(handle, channel);
}

int snd_pcm_plugin_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup)
{
    static spa_snd_setup_t real;
    snd_pcm_channel_setup_t actual;
    unsigned sourceChannels, outputChannels;
    int translate, result, logSetup;

    if (!real) real = (spa_snd_setup_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_setup");
    if (!real) return -1;
    if (!setup) return real(handle, setup);

    pthread_mutex_lock(&spa_pcm_sink.lock);
    translate = handle && handle == spa_pcm_sink.handle &&
                spa_pcm_sink.expandedParams;
    sourceChannels = spa_pcm_sink.sourceChannels;
    outputChannels = spa_pcm_sink.outputChannels;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (!translate || !sourceChannels || !outputChannels)
        return real(handle, setup);

    actual = *setup;
    result = real(handle, &actual);
    if (result != 0) {
        STATE_TRACE("QSA_SETUP", "result=%d source_channels=%u target_channels=%u",
                    result, sourceChannels, outputChannels);
        return result;
    }

    if (actual.mode != SND_PCM_MODE_BLOCK || actual.buf.block.frag_size <= 0) {
        LOG_ERROR(LOG_MODULE,
                  "audio sink: setup rejected mode=%d fragment=%d; expected block mode",
                  (int)actual.mode, (int)actual.buf.block.frag_size);
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle) spa_pcm_sink.expandedParams = 0;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        return -1;
    }

    *setup = actual;
    setup->format.voices = (int32_t)sourceChannels;
    {
        size_t actualFrameBytes = outputChannels * sizeof(int16_t);
        size_t stockFrameBytes = sourceChannels * sizeof(int16_t);
        size_t actualFragmentBytes = (size_t)setup->buf.block.frag_size;
        size_t actualFrames;

        if (actualFragmentBytes % actualFrameBytes != 0) {
            LOG_ERROR(LOG_MODULE,
                      "audio sink: QSA setup fragment %u not aligned to %uch; refusing translation",
                      (unsigned)actualFragmentBytes, outputChannels);
            pthread_mutex_lock(&spa_pcm_sink.lock);
            if (handle == spa_pcm_sink.handle) spa_pcm_sink.expandedParams = 0;
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            return -1;
        }
        actualFrames = actualFragmentBytes / actualFrameBytes;
        if (actual.format.rate != SPA_SR ||
            actual.format.voices != (int32_t)outputChannels ||
            actualFrames != SPA_PCM_SINK_PERIOD_FRAMES) {
            LOG_ERROR(LOG_MODULE,
                      "audio sink: setup rejected actual=%dHz/%dch/%u frames; "
                      "expected %dHz/%uch/%u frames per %u ms",
                      (int)actual.format.rate, (int)actual.format.voices,
                      (unsigned)actualFrames, SPA_SR, outputChannels,
                      (unsigned)SPA_PCM_SINK_PERIOD_FRAMES,
                      (unsigned)SPA_PCM_SINK_PERIOD_MS);
            pthread_mutex_lock(&spa_pcm_sink.lock);
            if (handle == spa_pcm_sink.handle) spa_pcm_sink.expandedParams = 0;
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            return -1;
        }
        setup->buf.block.frag_size =
            (int32_t)((actualFragmentBytes / actualFrameBytes) * stockFrameBytes);
        setup->buf.block.max_frag_size =
            (uint32_t)(((size_t)actual.buf.block.max_frag_size / actualFrameBytes) *
                       stockFrameBytes);
    }
    /* CQNXSoundIF asks for setup before essentially every fragment.  This used
     * to emit the same INFO line about 62 times a second, exhausting all three
     * diagnostic log generations in minutes and hiding the later lock/resume
     * control sequence.  The validated geometry is invariant for this handle;
     * log its first successful readback and keep the write-side health counter
     * as the periodic signal. */
    pthread_mutex_lock(&spa_pcm_sink.lock);
    logSetup = handle == spa_pcm_sink.handle && ++spa_pcm_sink.setupCalls == 1;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (logSetup)
        LOG_INFO(LOG_MODULE,
                 "audio sink: setup actual=%uch/%dHz/%d-byte fragment -> "
                 "stock=%uch/%d-byte fragment",
                 outputChannels, (int)actual.format.rate, actual.buf.block.frag_size,
                 sourceChannels, setup->buf.block.frag_size);
    if (logSetup)
        STATE_TRACE("QSA_SETUP",
                    "result=0 source_channels=%u target_channels=%u rate=%d "
                    "target_fragment=%d source_fragment=%d",
                    sourceChannels, outputChannels, (int)actual.format.rate,
                    actual.buf.block.frag_size, setup->buf.block.frag_size);
    return 0;
}

static void spa_pcm_expand_fallback(const int16_t *input, size_t frames,
                                    unsigned sourceChannels, int16_t *output,
                                    unsigned outputChannels)
{
    size_t frame;

    for (frame = 0; frame < frames; frame++) {
        const int16_t *src = input + frame * sourceChannels;
        int16_t *dst = output + frame * outputChannels;
        unsigned copyChannels = sourceChannels < outputChannels ?
                                sourceChannels : outputChannels;
        unsigned channel;

        memset(dst, 0, outputChannels * sizeof(*dst));
        for (channel = 0; channel < copyChannels; channel++)
            dst[channel] = src[channel];
        if (sourceChannels == 1 && outputChannels >= 2)
            dst[1] = src[0];
    }
}

/* ---- what QSA's plugin chain actually produces ----------------------------
 * snd_pcm_plugin_write runs the conversion chain and hands the result to the
 * device through snd_pcm_write_internal, which libasound exports and reaches
 * through its own PLT -- the same property that lets us interpose inside
 * libairplay.  Capturing here therefore shows the bytes AFTER conversion, which
 * is the one thing neither an on-unit counter nor the decoded PCM can show:
 * whether QSA's S16->S32 widening at six voices is what corrupts the audio.
 *
 * A bounded one-shot dump.  AUDIO_SPATIAL_TAP=<bytes> arms it; the file pairs
 * with the pre-conversion dump below, so the two can be diffed offline against
 * a trivial s32 = s16 << 16 reference. */
#define SPA_TAP_ENV        "AUDIO_SPATIAL_TAP"
#define SPA_TAP_OUT_PATH   "/tmp/spatial_tap_out.bin"
#define SPA_TAP_IN_PATH    "/tmp/spatial_tap_in.bin"

static size_t spa_tap_limit(void)
{
#if !ENABLE_AUDIO_DIAGNOSTICS
    return 0;
#else
    static long cached = -1;
    if (cached < 0) {
        const char *v = getenv(SPA_TAP_ENV);
        cached = (v && v[0]) ? strtol(v, 0, 0) : 0;
        if (cached < 0) cached = 0;
    }
    return (size_t)cached;
#endif
}

static void spa_tap_write(const char *path, size_t *taken, const void *buf, size_t len)
{
    size_t limit = spa_tap_limit();
    int fd;
    if (!limit || *taken >= limit || !buf || !len) return;
    if (len > limit - *taken) len = limit - *taken;
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return;
    (void)write(fd, buf, len);
    close(fd);
    *taken += len;
    if (*taken >= limit)
        LOG_WARN(LOG_MODULE, "audio tap: %s complete (%u bytes)",
                 path, (unsigned)*taken);
}

typedef ssize_t (*spa_snd_write_internal_t)(snd_pcm_t *, const void *, size_t);

ssize_t snd_pcm_write_internal(snd_pcm_t *handle, const void *buffer, size_t size)
{
    static spa_snd_write_internal_t real;
    static size_t taken;
    int isSink;

    if (!real) real = (spa_snd_write_internal_t)dlsym(RTLD_NEXT, "snd_pcm_write_internal");
    if (!real) return -1;

    pthread_mutex_lock(&spa_pcm_sink.lock);
    isSink = handle && handle == spa_pcm_sink.handle && spa_pcm_sink.expandedParams;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (isSink) spa_tap_write(SPA_TAP_OUT_PATH, &taken, buffer, size);

    return real(handle, buffer, size);
}

ssize_t snd_pcm_plugin_write(snd_pcm_t *handle, const void *buffer, size_t size)
{
    static spa_snd_write_t real;
    unsigned sourceChannels, outputChannels;
    size_t sourceFrameBytes, outputFrameBytes, frames, outputBytes;
    size_t bufferedFrames = 0;
    enum spa_pcm_period_source periodSource;
    ssize_t written;
    int useBridge, bufferedOwnsMedia;
#if ENABLE_STATE_TRACE
    int traceFirstSuccessfulWrite = 0;
    int traceQsaHealth = 0;
    int traceDeviceUnderrun = 0;
    int traceBufferedSilence = 0;
    uint64_t traceWriteNumber = 0;
    uint64_t traceZeroWrites = 0;
    uint64_t traceShortWrites = 0;
    uint64_t traceDeviceUnderruns = 0;
    uint64_t traceOddSizeWrites = 0;
    uint64_t traceRingFallbacks = 0;
    uint64_t traceSoftwareUnderrunFrames = 0;
    uint64_t traceBufferedSilencePeriods = 0;
    uint64_t traceBufferedSilenceFrames = 0;
    size_t traceBufferedReadyFrames = 0;
    int traceDeviceStatus = -1;
#endif

    if (!real) real = (spa_snd_write_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_write");
    if (!real) return -1;

    pthread_mutex_lock(&spa_pcm_sink.ioLock);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    useBridge = handle && handle == spa_pcm_sink.handle && spa_pcm_sink.expandedParams;
    sourceChannels = spa_pcm_sink.sourceChannels;
    outputChannels = spa_pcm_sink.outputChannels;
    if (!useBridge || !buffer || !sourceChannels || !outputChannels) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        return real(handle, buffer, size);
    }

    sourceFrameBytes = sourceChannels * sizeof(int16_t);
    outputFrameBytes = outputChannels * sizeof(int16_t);
    if (size == 0 || size % sourceFrameBytes != 0) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        return real(handle, buffer, size);
    }
    frames = size / sourceFrameBytes;
    if (frames > SPA_PCM_SINK_MAX_WRITE / outputFrameBytes) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
        LOG_ERROR(LOG_MODULE,
                  "audio sink: refusing oversized expanded write size=%u frames=%u",
                  (unsigned)size, (unsigned)frames);
        return real(handle, buffer, size);
    }
    outputBytes = frames * outputFrameBytes;
    /* We told the caller its period is SPA_PCM_SINK_PERIOD_FRAMES, but it asked for a
     * shorter one during params.  If it ever writes its own size anyway we hand the device
     * a partial fragment, which is audible as a click -- so count it rather than assume. */
    if (frames != SPA_PCM_SINK_PERIOD_FRAMES) {
        spa_pcm_sink.oddSizeWrites++;
        spa_pcm_sink.lastOddSize = size;
    }
    if (spa_pcm_sink.scratchBytes < outputBytes) {
        int16_t *replacement = (int16_t *)realloc(spa_pcm_sink.scratch, outputBytes);
        if (!replacement) {
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            pthread_mutex_unlock(&spa_pcm_sink.ioLock);
            return real(handle, buffer, size);
        }
        spa_pcm_sink.scratch = replacement;
        spa_pcm_sink.scratchBytes = outputBytes;
    }

    /* One locked channel snapshot is also the ownership snapshot.  Avoid two
     * independently timed active/channels reads across a concurrent teardown. */
    bufferedOwnsMedia = spa_buffered_pcm_channels() == outputChannels;
    if (bufferedOwnsMedia)
        bufferedFrames = spa_buffered_pcm_read(spa_pcm_sink.scratch, frames,
                                               outputChannels);
    periodSource = spa_pcm_period_source(bufferedOwnsMedia, bufferedFrames);
    if (periodSource == SPA_PCM_PERIOD_FROM_SILENCE) {
        size_t readyFrames = spa_buffered_pcm_pending(outputChannels);

        /* The buffered and realtime streams are independent sender
         * timelines.  Once type 103 owns media, never splice type-100 PCM
         * into its pause, flush/re-prime, or shortage: that can play item C
         * while iAP2 still advertises item B.  A whole silent QSA period is
         * a clean gap and, unlike the historical decoded-prefix/zero-tail
         * fragment, cannot make a periodic seam. */
        memset(spa_pcm_sink.scratch, 0, outputBytes);
        spa_pcm_sink.bufferedSilencePeriods++;
        spa_pcm_sink.bufferedSilenceFrames += frames;
        spa_pcm_sink.lastShortRingFrames = readyFrames;
#if ENABLE_STATE_TRACE
        if (spa103_trace_milestone(spa_pcm_sink.bufferedSilencePeriods)) {
            traceBufferedSilence = 1;
            traceBufferedSilencePeriods = spa_pcm_sink.bufferedSilencePeriods;
            traceBufferedSilenceFrames = spa_pcm_sink.bufferedSilenceFrames;
            traceBufferedReadyFrames = readyFrames;
        }
#endif
    } else if (periodSource == SPA_PCM_PERIOD_FROM_BUFFERED) {
        if (bufferedFrames < frames) {
            memset(spa_pcm_sink.scratch + bufferedFrames * outputChannels, 0,
                   (frames - bufferedFrames) * outputFrameBytes);
            spa_pcm_sink.underrunFrames += frames - bufferedFrames;
        }
        spa_pcm_sink.bufferedWrites++;
    } else {
        spa_pcm_expand_fallback((const int16_t *)buffer, frames, sourceChannels,
                                spa_pcm_sink.scratch, outputChannels);
    }
    spa_pcm_sink.writes++;
    {
        static size_t takenIn;
        spa_tap_write(SPA_TAP_IN_PATH, &takenIn, spa_pcm_sink.scratch,
                      frames * outputChannels * sizeof(int16_t));
    }
    /* libasound reaches snd_pcm_write_internal through its PLT.  That
     * interposer needs the state mutex to identify this handle, so never carry
     * the state lock into the real plugin write.  ioLock keeps scratch and the
     * handle stable until the call returns. */
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    /* QSA widens S16 to the device's native S32 itself, and the QNX harness
     * measured that widening to be exactly a left shift by 16 with channel order
     * intact -- so there is nothing for this path to improve on. */
    written = real(handle, spa_pcm_sink.scratch, outputBytes);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    spa_pcm_sink.lastWriteResult = (int64_t)written;
#if ENABLE_STATE_TRACE
    if (written > 0 && !spa_pcm_sink.firstSuccessfulWriteTraced) {
        spa_pcm_sink.firstSuccessfulWriteTraced = 1;
        traceFirstSuccessfulWrite = 1;
        traceWriteNumber = spa_pcm_sink.writes;
    }
#endif
    {
        int firstBadWrite = 0;

        if (written == 0) {
            spa_pcm_sink.zeroWrites++;
            firstBadWrite = spa_pcm_sink.zeroWrites == 1;
        } else if (written > 0 && (size_t)written < outputBytes) {
            spa_pcm_sink.shortWrites++;
            firstBadWrite = spa_pcm_sink.shortWrites == 1;
        }
        if (firstBadWrite)
            LOG_WARN(LOG_MODULE,
                     "audio sink: device did not take the whole fragment (%d of %u bytes) "
                     "at write %llu -- the stock caller will rewrite this buffer",
                     (int)written, (unsigned)outputBytes,
                     (unsigned long long)spa_pcm_sink.writes);
    }
    if (spa_pcm_sink.writes == 1 ||
#if ENABLE_STATE_TRACE
        (spa_pcm_sink.writes % 256u) == 0
#else
        (spa_pcm_sink.writes % 1024u) == 0
#endif
       ) {
        static int (*real_status)(snd_pcm_t *, snd_pcm_channel_status_t *);
        snd_pcm_channel_status_t st;
        if (!real_status)
            real_status = (int (*)(snd_pcm_t *, snd_pcm_channel_status_t *))
                          dlsym(RTLD_NEXT, "snd_pcm_plugin_status");
        if (real_status) {
            memset(&st, 0, sizeof(st));
            st.channel = SND_PCM_CHANNEL_PLAYBACK;
            if (real_status(handle, &st) == 0) {
                spa_pcm_sink.deviceStatus = st.status;
                spa_pcm_sink.deviceUnderruns = (uint64_t)st.underrun;
#if ENABLE_STATE_TRACE
                if (spa_pcm_sink.deviceUnderruns >
                    spa_pcm_sink.tracedDeviceUnderruns) {
                    spa_pcm_sink.tracedDeviceUnderruns =
                        spa_pcm_sink.deviceUnderruns;
                    traceDeviceUnderrun = 1;
                }
#endif
            }
        }
        LOG_WARN(LOG_MODULE,
                 "audio sink: writes=%llu buffered=%llu underrunFrames=%llu "
                 "shortRingFallbacks=%llu avoidedSilenceFrames=%llu lastReady=%u "
                 "bufferedSilence=%llu/%llu "
                 "zeroWrites=%llu shortWrites=%llu last=%lld "
                 "source=%uch target=%uch ringRead=%u/%u "
                     "odd=%llu lastOdd=%u devStatus=%d devUnderruns=%llu "
                     "handoffFlush=%llu reclaim=%llu/%llu/%llu",
                 (unsigned long long)spa_pcm_sink.writes,
                 (unsigned long long)spa_pcm_sink.bufferedWrites,
                 (unsigned long long)spa_pcm_sink.underrunFrames,
                 (unsigned long long)spa_pcm_sink.shortRingFallbacks,
                 (unsigned long long)spa_pcm_sink.avoidedSilenceFrames,
                 (unsigned)spa_pcm_sink.lastShortRingFrames,
                 (unsigned long long)spa_pcm_sink.bufferedSilencePeriods,
                 (unsigned long long)spa_pcm_sink.bufferedSilenceFrames,
                 (unsigned long long)spa_pcm_sink.zeroWrites,
                 (unsigned long long)spa_pcm_sink.shortWrites,
                 (long long)spa_pcm_sink.lastWriteResult,
                 sourceChannels, outputChannels,
                 (unsigned)bufferedFrames, (unsigned)frames,
                 (unsigned long long)spa_pcm_sink.oddSizeWrites,
                     (unsigned)spa_pcm_sink.lastOddSize,
                     spa_pcm_sink.deviceStatus,
                     (unsigned long long)spa_pcm_sink.deviceUnderruns,
                     (unsigned long long)spa_pcm_sink.suppressedFlushes,
                     (unsigned long long)spa_pcm_sink.reclaimedOpens,
                     (unsigned long long)spa_pcm_sink.skippedReclaimParams,
                     (unsigned long long)spa_pcm_sink.skippedReclaimPrepares);
#if ENABLE_STATE_TRACE
        traceQsaHealth = 1;
        traceWriteNumber = spa_pcm_sink.writes;
        traceZeroWrites = spa_pcm_sink.zeroWrites;
        traceShortWrites = spa_pcm_sink.shortWrites;
        traceDeviceStatus = spa_pcm_sink.deviceStatus;
        traceDeviceUnderruns = spa_pcm_sink.deviceUnderruns;
        traceOddSizeWrites = spa_pcm_sink.oddSizeWrites;
        traceRingFallbacks = spa_pcm_sink.shortRingFallbacks;
        traceSoftwareUnderrunFrames = spa_pcm_sink.underrunFrames;
#endif
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);

#if ENABLE_STATE_TRACE
    if (traceBufferedSilence)
        STATE_TRACE("QSA_BUFFERED_SILENCE",
                    "periods=%llu frames=%llu ready_frames=%u requested_frames=%u "
                    "result=%lld",
                    (unsigned long long)traceBufferedSilencePeriods,
                    (unsigned long long)traceBufferedSilenceFrames,
                    (unsigned)traceBufferedReadyFrames, (unsigned)frames,
                    (long long)written);
    if (traceFirstSuccessfulWrite)
        STATE_TRACE("QSA_FIRST_WRITE",
                    "owner=stock write=%llu result=%lld target_bytes=%u "
                    "source_bytes=%u source_channels=%u target_channels=%u "
                    "buffered_frames=%u total_frames=%u",
                    (unsigned long long)traceWriteNumber,
                    (long long)written, (unsigned)outputBytes, (unsigned)size,
                    sourceChannels, outputChannels,
                    (unsigned)bufferedFrames, (unsigned)frames);
    if (traceDeviceUnderrun)
        STATE_TRACE("QSA_DEVICE_UNDERRUN",
                    "owner=stock writes=%llu status=%d device_underruns=%llu",
                    (unsigned long long)traceWriteNumber, traceDeviceStatus,
                    (unsigned long long)traceDeviceUnderruns);
    if (traceQsaHealth)
        spa103_trace_qsa_health(
            "stock", traceWriteNumber, traceZeroWrites, traceShortWrites,
            0, traceDeviceStatus, traceDeviceUnderruns,
            traceOddSizeWrites, 0, 0, traceRingFallbacks,
            traceSoftwareUnderrunFrames);
    if (written != (ssize_t)outputBytes ||
        frames != SPA_PCM_SINK_PERIOD_FRAMES)
        STATE_TRACE("QSA_WRITE_ANOMALY",
                    "owner=stock result=%lld expected=%u submitted_frames=%u "
                    "period_frames=%u",
                    (long long)written, (unsigned)outputBytes,
                    (unsigned)frames, (unsigned)SPA_PCM_SINK_PERIOD_FRAMES);
#endif

    if (written < 0) return written;
    if ((size_t)written >= outputBytes) return (ssize_t)size;
    return (ssize_t)(((size_t)written / outputFrameBytes) * sourceFrameBytes);
}

int snd_pcm_plugin_flush(snd_pcm_t *handle, int channel)
{
    static spa_snd_flush_t real;
    int candidate, suppress = 0;
#if ENABLE_STATE_TRACE
    int isTracked;
#endif

    if (!real) real = (spa_snd_flush_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_flush");
    if (!real) return -1;

    /* Static audit of the accepted stock libairplay.so (sha256
     * 01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2):
     * its only flush xrefs are 0x47744 and 0x48a94, each immediately followed
     * by snd_pcm_close at 0x4774c/0x48a9c.  Suppress only that playback flush
     * for our live expanded sink while type-103 can inherit it. */
    pthread_mutex_lock(&spa_pcm_sink.ioLock);
    pthread_mutex_lock(&spa_pcm_sink.lock);
#if ENABLE_STATE_TRACE
    isTracked = handle && handle == spa_pcm_sink.handle;
#endif
    candidate = handle && handle == spa_pcm_sink.handle &&
                channel == SND_PCM_CHANNEL_PLAYBACK &&
                spa_pcm_sink.expandedParams && !spa_pcm_sink.detached;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    if (candidate && spa_buffered_pcm_active()) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle && spa_pcm_sink.expandedParams &&
            !spa_pcm_sink.detached) {
            spa_pcm_sink.closeFlushSuppressed = 1;
            spa_pcm_sink.closeFlushChannel = channel;
            spa_pcm_sink.suppressedFlushes++;
            suppress = 1;
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
    }
    if (!suppress) {
        int result = real(handle, channel);
        pthread_mutex_unlock(&spa_pcm_sink.ioLock);
#if ENABLE_STATE_TRACE
        if (isTracked)
            STATE_TRACE("QSA_FLUSH", "action=real channel=%d result=%d",
                        channel, result);
#endif
        return result;
    }
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);
    LOG_WARN(LOG_MODULE,
             "audio sink: deferred close-adjacent flush handle=%p channel=%d "
             "for continuous type-103 handoff",
             handle, channel);
    STATE_TRACE("QSA_FLUSH", "action=deferred channel=%d result=0", channel);
    return 0;
}

int snd_pcm_close(snd_pcm_t *handle)
{
    static spa_snd_close_t real;
    static spa_snd_flush_t realFlush;
    int isSink, keepForBuffered, result;
    int restoreFlush = 0, restoreFlushChannel = 0, continuous = 0;

    if (!real) real = (spa_snd_close_t)dlsym(RTLD_NEXT, "snd_pcm_close");
    if (!real) return -1;
    pthread_mutex_lock(&spa_pcm_sink.ioLock);
    pthread_mutex_lock(&spa_pcm_sink.lock);
    isSink = handle && handle == spa_pcm_sink.handle;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    keepForBuffered = isSink && spa_buffered_pcm_active();
    if (keepForBuffered) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle && spa_pcm_sink.expandedParams) {
            continuous = spa_pcm_sink.closeFlushSuppressed;
            spa_pcm_sink.detached = 1;
            spa_pcm_sink.detachedNeedsPrepare = !continuous;
            spa_pcm_sink.closeFlushSuppressed = 0;
            spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
            spa_pcm_sink.detachedWrites = 0;
            spa_pcm_sink.detachedZeroWrites = 0;
            spa_pcm_sink.detachedRecoveries = 0;
            spa_pcm_sink.detachedRouteReacquireAfterNs =
                spa_monotonic_ns() + SPA_MEDIA_ROUTE_REACQUIRE_DELAY_NS;
            spa_pcm_sink_reset_detached_clock_locked();
            pthread_mutex_unlock(&spa_pcm_sink.lock);
            LOG_WARN(LOG_MODULE,
                     "audio sink: retained %s handle=%p for active buffered type-103 "
                     "handoff=%s",
                     SPA_PCM_SINK_NAME, handle,
                     continuous ? "continuous" : "prepare-required");
            STATE_TRACE("QSA_CLOSE",
                        "owner=stock action=retain_for_buffered result=0 "
                        "handoff=%s",
                        continuous ? "continuous" : "prepare_required");
            pthread_mutex_unlock(&spa_pcm_sink.ioLock);
            return 0;
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
    }

    /* If type-103 vanished in the few instructions between the intercepted
     * flush and close, restore the deferred operation before honoring the real
     * close.  This keeps the non-handoff path byte-for-byte stock in effect. */
    if (isSink) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle && spa_pcm_sink.closeFlushSuppressed) {
            restoreFlush = 1;
            restoreFlushChannel = spa_pcm_sink.closeFlushChannel;
            spa_pcm_sink.closeFlushSuppressed = 0;
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
    }
    if (restoreFlush) {
        if (!realFlush)
            realFlush = (spa_snd_flush_t)dlsym(RTLD_NEXT,
                                               "snd_pcm_plugin_flush");
        if (realFlush) (void)realFlush(handle, restoreFlushChannel);
    }
    result = real(handle);
    if (isSink) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle) {
            spa_pcm_sink.handle = 0;
            spa_pcm_sink.detached = 0;
            spa_pcm_sink.expandedParams = 0;
            spa_pcm_sink.detachedNeedsPrepare = 0;
            spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
            spa_pcm_sink.sourceChannels = 0;
            spa_pcm_sink.outputChannels = 0;
            spa_pcm_sink_reset_detached_clock_locked();
            spa_pcm_sink.coldOpenAttempts = 0;
            spa_pcm_sink.coldOpenFailures = 0;
            spa_pcm_sink.coldOpenRetryAfterNs = 0;
            spa_pcm_sink.detachedRouteReacquireAfterNs = 0;
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        LOG_INFO(LOG_MODULE, "audio sink: released %s handle=%p result=%d",
                 SPA_PCM_SINK_NAME, handle, result);
        STATE_TRACE("QSA_CLOSE",
                    "owner=stock action=real result=%d restored_flush=%d",
                    result, restoreFlush ? 1 : 0);
    }
    pthread_mutex_unlock(&spa_pcm_sink.ioLock);
    return result;
}

/* Stock 210 has no object for the hook-owned buffered stream, so its feedback enumerator keeps
 * reporting the still-present realtime type-100 stream while 103 is carrying media.  Keep the
 * ownership test under the same lock as setup/teardown so a reconnect cannot rewrite feedback for
 * a stale session. */
static int spa103_is_active_for_session(void *session)
{
    int active;

    pthread_mutex_lock(&spa103_lock);
    active = session && spa103_receiver.session == session &&
             !spa103_receiver.settingUp && !spa103_receiver.stop;
    pthread_mutex_unlock(&spa103_lock);
    return active;
}

static void spa103_clear_magic_cookies_locked(void)
{
    size_t i;

    for (i = 0; i < spa103_receiver.magicCookieCount; i++) {
        free(spa103_receiver.magicCookies[i].bytes);
        spa103_receiver.magicCookies[i].bytes = 0;
        spa103_receiver.magicCookies[i].size = 0;
        spa103_receiver.magicCookies[i].id = 0;
    }
    spa103_receiver.magicCookieCount = 0;
}

/* Takes ownership of bytes. Pioneer keeps the first value for an ID and ignores duplicates; match
 * that behavior so a late duplicate cannot silently change the decoder configuration. A valid
 * command outside an active type-103 generation is acknowledged and discarded, also matching
 * AirPlayReceiverSessionSetMagicCookie when Pioneer has no stream-103 context. */
static int spa103_store_magic_cookie(int64_t id, uint8_t *bytes, size_t size)
{
#if ENABLE_AUDIO_DIAGNOSTICS
    char path[160];
#endif
    size_t i;
    uint32_t sequence = 0;
    int stored = 0;
#if ENABLE_AUDIO_DIAGNOSTICS
    int captureFd = -1;
#endif

    pthread_mutex_lock(&spa103_lock);
    if (!spa103_receiver.session || spa103_receiver.stop || spa103_receiver.settingUp) {
        pthread_mutex_unlock(&spa103_lock);
        free(bytes);
        LOG_WARN(LOG_MODULE,
                 "buffer103: SETMAGICCOOKIE id=%lld accepted without active stream; discarded",
                 (long long)id);
        STATE_TRACE("BUFFERED_COOKIE", "action=discard_inactive cookie_id=%lld bytes=%u",
                    (long long)id, (unsigned)size);
        return 0;
    }
    sequence = spa103_receiver.sequence;
    for (i = 0; i < spa103_receiver.magicCookieCount; i++) {
        spa103_magic_cookie_t *slot = &spa103_receiver.magicCookies[i];
        if (slot->id != id) continue;
        /* Same id, same bytes: a genuine duplicate, still ignored -- the phone re-sends these
         * routinely.  Same id, different bytes: a new track's decoder configuration.  Dropping
         * that leaves the decoder built for the previous track, which is silence on everything
         * but whatever played first. */
        if (slot->size == size && bytes && slot->bytes &&
            memcmp(slot->bytes, bytes, size) == 0) {
            pthread_mutex_unlock(&spa103_lock);
            free(bytes);
            LOG_INFO(LOG_MODULE,
                     "buffer103: duplicate SETMAGICCOOKIE id=%lld ignored sequence=%u",
                     (long long)id, (unsigned)sequence);
            STATE_TRACE("BUFFERED_COOKIE",
                        "action=duplicate setup_seq=%u cookie_id=%lld bytes=%u",
                        (unsigned)sequence, (long long)id, (unsigned)size);
            return 0;
        }
        free(slot->bytes);
        slot->bytes = bytes;
        slot->size = size;
        spa103_receiver.cookieGeneration++;
        sequence = spa103_receiver.sequence;
        pthread_mutex_unlock(&spa103_lock);
        LOG_WARN(LOG_MODULE,
                 "buffer103: SETMAGICCOOKIE id=%lld replaced, %u bytes, generation %u "
                 "-- the decoder will be rebuilt",
                 (long long)id, (unsigned)size, (unsigned)spa103_receiver.cookieGeneration);
        STATE_TRACE("BUFFERED_COOKIE",
                    "action=replace setup_seq=%u cookie_id=%lld bytes=%u cookie_gen=%u",
                    (unsigned)sequence, (long long)id, (unsigned)size,
                    (unsigned)spa103_receiver.cookieGeneration);
        return 1;
    }
    if (spa103_receiver.magicCookieCount < SPA_103_MAX_MAGIC_COOKIES) {
        spa103_magic_cookie_t *slot =
            &spa103_receiver.magicCookies[spa103_receiver.magicCookieCount++];
        slot->id = id;
        slot->bytes = bytes;
        slot->size = size;
        stored = 1;
#if ENABLE_AUDIO_DIAGNOSTICS
        snprintf(path, sizeof(path), "/tmp/spatial_magiccookie_%u_%016llx.bin",
                 (unsigned)sequence, (unsigned long long)(uint64_t)id);
        captureFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (captureFd >= 0) {
            size_t written = spa103_write_capture(captureFd, bytes, size);
            close(captureFd);
            if (written != size)
                LOG_ERROR(LOG_MODULE,
                          "buffer103: short magic-cookie capture path=%s written=%u size=%u",
                          path, (unsigned)written, (unsigned)size);
        } else {
            LOG_ERROR(LOG_MODULE, "buffer103: cannot capture magic cookie path=%s errno=%d",
                      path, errno);
        }
#endif
    }
    pthread_mutex_unlock(&spa103_lock);

    if (!stored) {
        free(bytes);
        LOG_ERROR(LOG_MODULE,
                  "buffer103: SETMAGICCOOKIE map full id=%lld limit=%u sequence=%u",
                  (long long)id, (unsigned)SPA_103_MAX_MAGIC_COOKIES, (unsigned)sequence);
        STATE_TRACE("BUFFERED_COOKIE",
                    "action=map_full setup_seq=%u cookie_id=%lld bytes=%u",
                    (unsigned)sequence, (long long)id, (unsigned)size);
        return -1;
    }
    LOG_INFO(LOG_MODULE,
             "buffer103: stored SETMAGICCOOKIE id=%lld bytes=%u sequence=%u",
             (long long)id, (unsigned)size, (unsigned)sequence);
    STATE_TRACE("BUFFERED_COOKIE",
                "action=store setup_seq=%u cookie_id=%lld bytes=%u",
                (unsigned)sequence, (long long)id, (unsigned)size);
    return 0;
}

static int spa103_send_http_status(void *connection, int status)
{
    int err;

    err = spa103_http_init_response(connection, status);
    if (!err) err = spa103_http_send_response(connection);
    return err;
}

static int spa103_method_equal(const uint8_t *method, uint32_t methodLength,
                               const char *wanted)
{
    size_t wantedLength = strlen(wanted);
    return method && methodLength == wantedLength &&
           memcmp(method, wanted, wantedLength) == 0;
}

/* The stock request callback already waited for the complete HTTP body. All buffered-control
 * methods use the same binary-plist envelope as SETUP and SETMAGICCOOKIE. */
static void *spa103_parse_control_plist(void *request, const char *methodName,
                                       uint32_t *sessionSequence, uint32_t *requestNumber)
{
    const spa103_private_profile_t *profile = spa103_private_profile();
    const uint8_t *body;
    uint32_t bodyLength;
    void *plist;
    int32_t err = 0;
    uint32_t sequence = 0, number = 0;
    int active;
    char captureKind[64];

    if (!profile) return 0;
    pthread_mutex_lock(&spa103_lock);
    active = spa103_receiver.session && !spa103_receiver.stop;
    if (active) {
        sequence = spa103_receiver.sequence;
        number = ++spa103_receiver.controlRequestCount;
    }
    pthread_mutex_unlock(&spa103_lock);
    if (!active) {
        LOG_WARN(LOG_MODULE, "buffer103: %s received without active type-103 stream",
                 methodName);
        return 0;
    }

    body = *(const uint8_t * const *)((const uint8_t *)request +
                                      profile->httpBodyPointerOffset);
    bodyLength = *(const uint32_t *)((const uint8_t *)request +
                                     profile->httpBodyLengthOffset);
    if (!body || bodyLength == 0 || bodyLength > SPA_103_MAX_MAGIC_COOKIE_BODY_SIZE) {
        LOG_ERROR(LOG_MODULE, "buffer103: invalid %s body length=%u",
                  methodName, (unsigned)bodyLength);
        return 0;
    }

    plist = spa103_cf_create_plist_bytes(body, bodyLength, 0,
                                          spa103_cf_dictionary_type_id(), &err);
    if (!plist || err) {
        if (plist) cfa_rel(plist);
        LOG_ERROR(LOG_MODULE, "buffer103: cannot parse %s binary plist err=%d",
                  methodName, (int)err);
        return 0;
    }

    snprintf(captureKind, sizeof(captureKind), "%u_%s", (unsigned)number, methodName);
    spa_write_control_plist(plist, captureKind, sequence);
    if (sessionSequence) *sessionSequence = sequence;
    if (requestNumber) *requestNumber = number;
    return plist;
}

/* Return 1 when all three keys exist. `valid` mirrors APSAudioTransportTime validity sufficiently
 * for this bridge: a non-zero media timescale is required for rate-1 anchors, while FLUSH may carry
 * an explicitly invalid triple and still be a well-formed request. */
static int spa103_read_transport_time(const void *dict, const char *rtpKey,
                                      const char *valueKey, const char *scaleKey,
                                      spa103_transport_time_t *out)
{
    spa103_transport_time_t value;

    memset(&value, 0, sizeof(value));
    if (!spa_cf_get_i64(dict, rtpKey, &value.rtpTime) ||
        !spa_cf_get_i64(dict, valueKey, &value.mediaTimeValue) ||
        !spa_cf_get_i64(dict, scaleKey, &value.mediaTimeScale))
        return 0;
    value.valid = value.mediaTimeScale != 0;
    *out = value;
    return 1;
}

/* Pioneer ConvertMediaTimeToSampleTime computes floor(mediaValue/mediaScale*sampleRate) modulo
 * 2^32. Live hardware closes this exactly: 46670057988423/1e9 at 48 kHz becomes 0x85862bdf, the
 * media-time value in the first APAP fixed header. Avoid floating point while preserving that
 * quotient/remainder result for the positive media timeline used by CarPlay. */
static int64_t spa103_media_time_to_rtp(int64_t mediaValue, int64_t mediaScale)
{
    uint64_t whole, remainder, samples;

    if (mediaValue < 0 || mediaScale <= 0) return 0;
    whole = (uint64_t)mediaValue / (uint64_t)mediaScale;
    remainder = (uint64_t)mediaValue % (uint64_t)mediaScale;
    samples = whole * SPA_SR + (remainder * SPA_SR) / (uint64_t)mediaScale;
    return (int64_t)(uint32_t)samples;
}

/* Apple's serializers omit an invalid RTP component.  Live iOS 27 FLUSHBUFFERED uses this
 * media-only form (flushUntilMediaTimeValue/Scale) and may omit the entire "from" boundary. */
static int spa103_read_optional_rtp_transport_time(const void *dict, const char *rtpKey,
                                                   const char *valueKey, const char *scaleKey,
                                                   spa103_transport_time_t *out)
{
    spa103_transport_time_t value;

    memset(&value, 0, sizeof(value));
    if (!spa_cf_get_i64(dict, valueKey, &value.mediaTimeValue) ||
        !spa_cf_get_i64(dict, scaleKey, &value.mediaTimeScale) ||
        value.mediaTimeScale == 0)
        return 0;
    if (!spa_cf_get_i64(dict, rtpKey, &value.rtpTime))
        value.rtpTime = spa103_media_time_to_rtp(value.mediaTimeValue,
                                                  value.mediaTimeScale);
    value.valid = 1;
    *out = value;
    return 1;
}

static int spa103_read_network_time(const void *dict, const char *prefix,
                                    spa103_network_time_t *out)
{
    spa103_network_time_t value;
    char secsKey[64], fracKey[64], timelineKey[80], flagsKey[64];

    memset(&value, 0, sizeof(value));
    snprintf(secsKey, sizeof(secsKey), "%sSecs", prefix);
    snprintf(fracKey, sizeof(fracKey), "%sFrac", prefix);
    snprintf(timelineKey, sizeof(timelineKey), "%sTimelineID", prefix);
    snprintf(flagsKey, sizeof(flagsKey), "%sFlags", prefix);
    if (!spa_cf_get_i64(dict, secsKey, &value.secs) ||
        !spa_cf_get_i64(dict, fracKey, &value.frac) ||
        !spa_cf_get_i64(dict, timelineKey, &value.timelineID) ||
        !spa_cf_get_i64(dict, flagsKey, &value.flags))
        return 0;
    value.valid = 1;
    *out = value;
    return 1;
}

static int spa103_set_transport_time(void *dict, const char *rtpKey,
                                     const char *valueKey, const char *scaleKey,
                                     const spa103_transport_time_t *value)
{
    return spa103_response_set_i64(dict, rtpKey, value->rtpTime) ||
           spa103_response_set_i64(dict, valueKey, value->mediaTimeValue) ||
           spa103_response_set_i64(dict, scaleKey, value->mediaTimeScale) ? -1 : 0;
}

static int spa103_set_network_time(void *dict, const char *prefix,
                                   const spa103_network_time_t *value)
{
    char secsKey[64], fracKey[64], timelineKey[80], flagsKey[64];

    snprintf(secsKey, sizeof(secsKey), "%sSecs", prefix);
    snprintf(fracKey, sizeof(fracKey), "%sFrac", prefix);
    snprintf(timelineKey, sizeof(timelineKey), "%sTimelineID", prefix);
    snprintf(flagsKey, sizeof(flagsKey), "%sFlags", prefix);
    return spa103_response_set_i64(dict, secsKey, value->secs) ||
           spa103_response_set_i64(dict, fracKey, value->frac) ||
           spa103_response_set_i64(dict, timelineKey, value->timelineID) ||
           spa103_response_set_i64(dict, flagsKey, value->flags) ? -1 : 0;
}

static void spa103_make_fallback_network_time(spa103_network_time_t *value)
{
    struct timespec now;
    uint64_t fraction;
    uint32_t ntpSeconds;

    clock_gettime(CLOCK_REALTIME, &now);
    ntpSeconds = (uint32_t)((uint64_t)now.tv_sec + UINT64_C(2208988800));
    fraction = ((uint64_t)(uint32_t)now.tv_nsec << 32) / UINT64_C(1000000000);
    memset(value, 0, sizeof(*value));
    value->valid = 1;
    /* Apple's receiver transports the NTP seconds field as a signed 32-bit integer in the plist. */
    value->secs = (int64_t)(int32_t)ntpSeconds;
    value->frac = (int64_t)fraction;
}

/* Pioneer ReturnAnchorForRTP anchors playback 150 ms in the future on the receiver's synchronized
 * AirTunes clock. The profiled target already owns the same NTP clock for the stock session; reuse it so
 * the sender sees the actual negotiated timeline instead of the previous timelineID=0 wall-clock
 * placeholder. AirTunesClock_Create receives session+0x13d0 as an output slot and stock callers
 * load the resulting clock pointer from that slot. The private session offset is protected by the
 * same exact-image fingerprint as the buffered HTTP dispatcher. */
static int spa103_get_session_network_time(void *session, spa103_network_time_t *value)
{
    const spa103_private_profile_t *profile = spa103_private_profile();
    spa103_air_tunes_time_t synchronized;
    void *clock;
    uint64_t anchorTicks;

    if (!profile || !session || !value || !spa103_up_ticks || !spa103_milliseconds_to_up_ticks ||
        !spa103_clock_get_near_ticks)
        return 0;
    clock = *(void **)((uint8_t *)session + profile->sessionClockOffset);
    if (!clock) return 0;

    memset(&synchronized, 0, sizeof(synchronized));
    anchorTicks = spa103_up_ticks() +
                  spa103_milliseconds_to_up_ticks((uint64_t)SPA_103_ANCHOR_LEAD_MS);
    spa103_clock_get_near_ticks(clock, &synchronized, anchorTicks);

    memset(value, 0, sizeof(*value));
    value->valid = 1;
    value->secs = (int64_t)(int32_t)synchronized.secs;
    value->frac = (int64_t)synchronized.frac;
    value->timelineID = 0;
    value->flags = 0;
    return 1;
}

/* The selected profile's private SETUP/info helper is the stock, already-reviewed path that
 * serializes a CFLite dictionary as application/x-apple-binary-plist and initializes the response
 * with the request's RTSP/HTTP version. The surrounding callback fingerprint must match before the
 * pointer is published. */
static int spa103_send_http_plist(void *connection, void *request, void *plist)
{
    int bodyErr = 0;
    int status;

    if (!spa103_http_set_plist_response || !plist)
        return spa103_send_http_status(connection, 500);
    status = spa103_http_set_plist_response(connection, request, plist, &bodyErr);
    if (status != 200 || bodyErr) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: binary-plist response build failed status=%d err=%d",
                  status, bodyErr);
        return spa103_send_http_status(connection, status == 200 ? 500 : status);
    }
    return spa103_http_send_response(connection);
}

enum spa103_control_method {
    SPA103_CONTROL_NONE = 0,
    SPA103_CONTROL_GET_ANCHOR,
    SPA103_CONTROL_SET_RATE,
    SPA103_CONTROL_SET_RATE_ANCHOR,
    SPA103_CONTROL_FLUSH
};

static int spa103_process_buffered_control(void *connection, void *request,
                                           enum spa103_control_method method,
                                           const char *methodName)
{
    void *params = 0, *response = 0;
    spa103_transport_time_t anchor, firstAudible, fromTime, untilTime;
    spa103_network_time_t networkTime;
    spa_apap_flush_range_t flushRange;
    int64_t rate = 0, fromSeq = 0, untilSeq = 0;
    uint32_t sequence = 0, requestNumber = 0;
    int haveFromSeq = 0, haveUntilSeq = 0;
    int haveFromTime = 0, haveUntilTime = 0;
    int status = 200;
    int result;
    char responseKind[96];

    memset(&anchor, 0, sizeof(anchor));
    memset(&firstAudible, 0, sizeof(firstAudible));
    memset(&fromTime, 0, sizeof(fromTime));
    memset(&untilTime, 0, sizeof(untilTime));
    memset(&networkTime, 0, sizeof(networkTime));
    memset(&flushRange, 0, sizeof(flushRange));

    params = spa103_parse_control_plist(request, methodName, &sequence, &requestNumber);
    if (!params) {
        STATE_TRACE("BUFFERED_CONTROL",
                    "method=%s action=parse_reject http_status=400",
                    methodName);
        return spa103_send_http_status(connection, 400);
    }

    if (method == SPA103_CONTROL_GET_ANCHOR || method == SPA103_CONTROL_SET_RATE ||
        method == SPA103_CONTROL_SET_RATE_ANCHOR) {
        if (!spa_cf_get_i64(params, "rate", &rate)) {
            status = 400;
            goto done;
        }
    }

    switch (method) {
        case SPA103_CONTROL_SET_RATE_ANCHOR:
            if (rate == 1) {
                /* A resume that we cannot fully parse must still resume.  Rejecting it with 400
                 * leaves `playing` at zero and the session silent until the user presses pause
                 * and play, which is exactly the "sometimes no sound after changing track"
                 * symptom: the anchor only positions the timeline, while audibility depends on
                 * nothing but the rate.  Take what parsed, note what did not, and play. */
                if (!spa103_read_transport_time(params, "rtpTime", "mediaTimeValue",
                                                "mediaTimeScale", &anchor) || !anchor.valid ||
                    !spa103_read_transport_time(params, "firstAudibleRTPTime",
                                                "firstAudibleMediaTimeValue",
                                                "firstAudibleMediaTimeScale", &firstAudible) ||
                    !spa103_read_network_time(params, "networkTime", &networkTime))
                    LOG_WARN(LOG_MODULE,
                             "buffer103: %s[%u] rate=1 with an incomplete anchor "
                             "(anchor=%d firstAudible=%d networkTime=%d); resuming anyway",
                             methodName, (unsigned)requestNumber, anchor.valid,
                             firstAudible.valid, networkTime.valid);
            }
            pthread_mutex_lock(&spa103_lock);
            if (spa103_receiver.session && !spa103_receiver.stop &&
                spa103_receiver.sequence == sequence) {
                spa103_receiver.rate = rate;
                if (rate == 1) {
                    spa103_receiver.anchor = anchor;
                    spa103_receiver.firstAudible = firstAudible;
                    spa103_receiver.networkTime = networkTime;
                }
            }
            pthread_mutex_unlock(&spa103_lock);
            spa103_render_set_rate(rate);
            LOG_WARN(LOG_MODULE,
                     "buffer103: %s[%u] ACK rate=%lld rtp=%lld media=%lld/%lld "
                     "firstAudible=%lld timeline=%lld",
                     methodName, (unsigned)requestNumber, (long long)rate,
                     (long long)anchor.rtpTime, (long long)anchor.mediaTimeValue,
                     (long long)anchor.mediaTimeScale, (long long)firstAudible.rtpTime,
                     (long long)networkTime.timelineID);
            /* Apple's own receiver sends an empty 200 response on the success path. */
            break;

        case SPA103_CONTROL_SET_RATE:
            if (rate == 1) {
                /* The live iOS 27 hose emitted only mediaTimeValue/mediaTimeScale here. Apple's
                 * CFDictionaryGetAudioTransportTime accepts that valid media-time form with an
                 * absent RTP component; its sender-side serializer omits invalid rtpTime.
                 * As above: an unparseable anchor is not a reason to refuse to play. */
                if (spa_cf_get_i64(params, "mediaTimeValue", &anchor.mediaTimeValue) &&
                    spa_cf_get_i64(params, "mediaTimeScale", &anchor.mediaTimeScale) &&
                    anchor.mediaTimeScale != 0) {
                    if (!spa_cf_get_i64(params, "rtpTime", &anchor.rtpTime))
                        anchor.rtpTime = spa103_media_time_to_rtp(anchor.mediaTimeValue,
                                                                   anchor.mediaTimeScale);
                    anchor.valid = 1;
                } else {
                    LOG_WARN(LOG_MODULE,
                             "buffer103: %s[%u] rate=1 without a usable media time; "
                             "resuming anyway", methodName, (unsigned)requestNumber);
                }
            }
            pthread_mutex_lock(&spa103_lock);
            if (spa103_receiver.session && !spa103_receiver.stop &&
                spa103_receiver.sequence == sequence) {
                spa103_receiver.rate = rate;
                if (rate == 1) spa103_receiver.anchor = anchor;
                if (!anchor.valid) anchor = spa103_receiver.anchor;
                if (!spa103_get_session_network_time(spa103_receiver.session, &networkTime))
                    networkTime = spa103_receiver.networkTime;
                else
                    spa103_receiver.networkTime = networkTime;
            }
            pthread_mutex_unlock(&spa103_lock);
            spa103_render_set_rate(rate);
            if (!anchor.valid) {
                anchor.valid = 1;
                anchor.mediaTimeScale = SPA_SR;
            }
            if (!networkTime.valid) spa103_make_fallback_network_time(&networkTime);
            response = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
            if (!response ||
                spa103_set_transport_time(response, "rtpTime", "mediaTimeValue",
                                          "mediaTimeScale", &anchor) != 0 ||
                spa103_set_network_time(response, "networkTime", &networkTime) != 0 ||
                spa103_response_set_i64(response, "status", 0) != 0) {
                status = 500;
                goto done;
            }
            LOG_WARN(LOG_MODULE,
                     "buffer103: %s[%u] ACK rate=%lld rtp=%lld anchor=%lld/%lld "
                     "network=%lld/%lld timeline=%lld flags=%lld",
                     methodName, (unsigned)requestNumber, (long long)rate,
                     (long long)anchor.rtpTime,
                     (long long)anchor.mediaTimeValue, (long long)anchor.mediaTimeScale,
                     (long long)networkTime.secs, (long long)networkTime.frac,
                     (long long)networkTime.timelineID, (long long)networkTime.flags);
            break;

        case SPA103_CONTROL_GET_ANCHOR:
            pthread_mutex_lock(&spa103_lock);
            if (spa103_receiver.session && !spa103_receiver.stop &&
                spa103_receiver.sequence == sequence) {
                anchor = spa103_receiver.anchor;
                if (!spa103_get_session_network_time(spa103_receiver.session, &networkTime))
                    networkTime = spa103_receiver.networkTime;
                else
                    spa103_receiver.networkTime = networkTime;
            }
            pthread_mutex_unlock(&spa103_lock);
            if (!anchor.valid) {
                anchor.valid = 1;
                anchor.mediaTimeScale = SPA_SR;
            }
            if (!networkTime.valid) spa103_make_fallback_network_time(&networkTime);
            response = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
            if (!response ||
                spa103_set_transport_time(response, "rtpTime", "mediaTimeValue",
                                          "mediaTimeScale", &anchor) != 0 ||
                spa103_set_network_time(response, "networkTime", &networkTime) != 0 ||
                spa103_response_set_i64(response, "status", 0) != 0) {
                status = 500;
                goto done;
            }
            LOG_WARN(LOG_MODULE,
                     "buffer103: %s[%u] ACK rate=%lld rtp=%lld anchor=%lld/%lld "
                     "network=%lld/%lld timeline=%lld flags=%lld",
                     methodName, (unsigned)requestNumber, (long long)rate,
                     (long long)anchor.rtpTime,
                     (long long)anchor.mediaTimeValue, (long long)anchor.mediaTimeScale,
                     (long long)networkTime.secs, (long long)networkTime.frac,
                     (long long)networkTime.timelineID, (long long)networkTime.flags);
            break;

        case SPA103_CONTROL_FLUSH:
            haveFromSeq = spa_cf_get_i64(params, "flushFromSeq", &fromSeq);
            haveUntilSeq = spa_cf_get_i64(params, "flushUntilSeq", &untilSeq);
            haveFromTime = spa103_read_optional_rtp_transport_time(
                params, "flushFromTS", "flushFromMediaTimeValue",
                "flushFromMediaTimeScale", &fromTime);
            haveUntilTime = spa103_read_optional_rtp_transport_time(
                params, "flushUntilTS", "flushUntilMediaTimeValue",
                "flushUntilMediaTimeScale", &untilTime);
            /* At least one boundary must be present. Each side and each sequence/RTP component
             * is optional; the live pause path carries only untilSeq + until media time. */
            if ((!haveFromSeq && !haveFromTime) && (!haveUntilSeq && !haveUntilTime)) {
                status = 400;
                goto done;
            }
            pthread_mutex_lock(&spa103_lock);
            if (spa103_receiver.session && !spa103_receiver.stop &&
                spa103_receiver.sequence == sequence && haveUntilTime) {
                spa103_receiver.anchor = untilTime;
            }
            pthread_mutex_unlock(&spa103_lock);
            flushRange.have_from_sequence = haveFromSeq;
            flushRange.from_sequence = (uint32_t)fromSeq;
            flushRange.have_until_sequence = haveUntilSeq;
            flushRange.until_sequence = (uint32_t)untilSeq;
            flushRange.have_from_rtp = haveFromTime;
            flushRange.from_rtp = (uint32_t)fromTime.rtpTime;
            flushRange.have_until_rtp = haveUntilTime;
            flushRange.until_rtp = (uint32_t)untilTime.rtpTime;
            spa103_render_flush(&flushRange);
            response = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
            if (!response) {
                status = 500;
                goto done;
            }
            LOG_WARN(LOG_MODULE,
                     "buffer103: %s[%u] ACK fromSeq=%s%lld untilSeq=%s%lld "
                     "from=%s%lld/%lld until=%s%lld/%lld",
                     methodName, (unsigned)requestNumber, haveFromSeq ? "" : "absent/",
                     (long long)fromSeq, haveUntilSeq ? "" : "absent/",
                     (long long)untilSeq, haveFromTime ? "" : "absent/",
                     (long long)fromTime.mediaTimeValue, (long long)fromTime.mediaTimeScale,
                     haveUntilTime ? "" : "absent/",
                     (long long)untilTime.mediaTimeValue, (long long)untilTime.mediaTimeScale);
            break;

        default:
            status = 501;
            break;
    }

done:
    if (status != 200) {
        LOG_ERROR(LOG_MODULE, "buffer103: %s[%u] rejected HTTP status=%d",
                  methodName, (unsigned)requestNumber, status);
        result = spa103_send_http_status(connection, status);
    } else if (response) {
        snprintf(responseKind, sizeof(responseKind), "%u_%s_response",
                 (unsigned)requestNumber, methodName);
        spa_write_control_plist(response, responseKind, sequence);
        result = spa103_send_http_plist(connection, request, response);
    } else {
        result = spa103_send_http_status(connection, 200);
    }
    if (response) cfa_rel(response);
    cfa_rel(params);
    if (method == SPA103_CONTROL_FLUSH) {
        STATE_TRACE("BUFFERED_CONTROL",
                    "method=%s setup_seq=%u request=%u http_status=%d send_result=%d "
                    "from_seq_present=%d from_seq=%lld until_seq_present=%d "
                    "until_seq=%lld from_time_present=%d until_time_present=%d "
                    "reprime_frames=%u",
                    methodName, (unsigned)sequence, (unsigned)requestNumber,
                    status, result, haveFromSeq, (long long)fromSeq,
                    haveUntilSeq, (long long)untilSeq,
                    haveFromTime, haveUntilTime,
                    spa103_reprime_frames());
    } else if (method == SPA103_CONTROL_SET_RATE ||
               method == SPA103_CONTROL_SET_RATE_ANCHOR) {
        STATE_TRACE("BUFFERED_CONTROL",
                    "method=%s setup_seq=%u request=%u http_status=%d "
                    "send_result=%d rate=%lld anchor_valid=%d rtp=%lld "
                    "media_value=%lld media_scale=%lld timeline=%lld",
                    methodName, (unsigned)sequence, (unsigned)requestNumber,
                    status, result, (long long)rate, anchor.valid,
                    (long long)anchor.rtpTime,
                    (long long)anchor.mediaTimeValue,
                    (long long)anchor.mediaTimeScale,
                    (long long)networkTime.timelineID);
    }
    return result;
}

/* Request callback installed only in a fingerprinted HTTPServer object. The request is already
 * fully read when this callback runs, so the binary-plist body lives at the same HTTPMessage fields
 * used by the stock POST/SET_PARAMETER handlers. */
static int spa103_http_process(void *connection, void *request)
{
    static const char cookieMethod[] = "SETMAGICCOOKIE";
    const spa103_private_profile_t *profile = spa103_private_profile();
    const uint8_t *method;
    const uint8_t *body;
    const uint8_t *cookieBytes;
    const char *controlMethodName = 0;
    enum spa103_control_method controlMethod = SPA103_CONTROL_NONE;
    uint32_t methodLength, bodyLength;
    void *plist = 0, *idKey = 0, *cookieKey = 0, *cookieData = 0;
    uint8_t *cookieCopy = 0;
    int64_t cookieID = 0;
    long cookieLength;
    int32_t err = 0;
    int status = 200;

    if (!profile || !request || !connection || !spa103_original_http_process)
        return -6714;

    method = *(const uint8_t * const *)((const uint8_t *)request +
                                         profile->httpMethodPointerOffset);
    methodLength = *(const uint32_t *)((const uint8_t *)request +
                                       profile->httpMethodLengthOffset);
    if (!spa103_method_equal(method, methodLength, cookieMethod)) {
        if (spa103_method_equal(method, methodLength, "GETANCHOR")) {
            controlMethod = SPA103_CONTROL_GET_ANCHOR;
            controlMethodName = "GETANCHOR";
        } else if (spa103_method_equal(method, methodLength, "SETRATE")) {
            controlMethod = SPA103_CONTROL_SET_RATE;
            controlMethodName = "SETRATE";
        } else if (spa103_method_equal(method, methodLength, "SETRATEANCHORTIME")) {
            controlMethod = SPA103_CONTROL_SET_RATE_ANCHOR;
            controlMethodName = "SETRATEANCHORTIME";
        } else if (spa103_method_equal(method, methodLength, "FLUSHBUFFERED")) {
            controlMethod = SPA103_CONTROL_FLUSH;
            controlMethodName = "FLUSHBUFFERED";
        } else {
            return spa103_original_http_process(connection, request);
        }
        if (!spa_probe103_mode())
            return spa103_original_http_process(connection, request);
        return spa103_process_buffered_control(connection, request, controlMethod,
                                               controlMethodName);
    }
    if (!spa_probe103_mode())
        return spa103_original_http_process(connection, request);

    body = *(const uint8_t * const *)((const uint8_t *)request +
                                      profile->httpBodyPointerOffset);
    bodyLength = *(const uint32_t *)((const uint8_t *)request +
                                     profile->httpBodyLengthOffset);
    if (!body || bodyLength == 0 || bodyLength > SPA_103_MAX_MAGIC_COOKIE_BODY_SIZE) {
        LOG_ERROR(LOG_MODULE, "buffer103: invalid SETMAGICCOOKIE body length=%u",
                  (unsigned)bodyLength);
        return spa103_send_http_status(connection, 400);
    }

    plist = spa103_cf_create_plist_bytes(body, bodyLength, 0,
                                          spa103_cf_dictionary_type_id(), &err);
    if (!plist || err) {
        status = 400;
        goto done;
    }
    idKey = cfa_str(0, "magicCookieID", SPA_CFENC_UTF8);
    cookieKey = cfa_str(0, "magicCookie", SPA_CFENC_UTF8);
    if (!idKey || !cookieKey) {
        status = 500;
        goto done;
    }

    err = 0;
    cookieID = cfa_dictgeti(plist, idKey, &err);
    if (err) {
        status = 400;
        goto done;
    }
    err = 0;
    cookieData = spa103_cf_dict_get_typed(plist, cookieKey,
                                           spa103_cf_data_type_id(), &err);
    if (!cookieData || err) {
        status = 400;
        goto done;
    }
    cookieLength = spa103_cf_data_length(cookieData);
    cookieBytes = spa103_cf_data_bytes(cookieData);
    if (cookieLength <= 0 || (unsigned long)cookieLength > SPA_103_MAX_MAGIC_COOKIE_SIZE ||
        !cookieBytes) {
        status = 400;
        goto done;
    }
    cookieCopy = (uint8_t *)malloc((size_t)cookieLength);
    if (!cookieCopy) {
        status = 500;
        goto done;
    }
    memcpy(cookieCopy, cookieBytes, (size_t)cookieLength);
    if (spa103_store_magic_cookie(cookieID, cookieCopy, (size_t)cookieLength) != 0) {
        cookieCopy = 0; /* store helper consumed/freed ownership on every path */
        status = 500;
        goto done;
    }
    cookieCopy = 0;

done:
    free(cookieCopy);
    if (cookieKey) cfa_rel(cookieKey);
    if (idKey) cfa_rel(idKey);
    if (plist) cfa_rel(plist);
    if (status != 200)
        LOG_ERROR(LOG_MODULE, "buffer103: SETMAGICCOOKIE rejected HTTP status=%d err=%d",
                  status, err);
    return spa103_send_http_status(connection, status);
}

static int spa103_install_buffered_dispatcher(void)
{
    const spa103_private_profile_t *profile;
    uintptr_t ensureAddress, imageBase;
    void *outer, *server;
    void **callbackSlot;
    int installed = 0;

    pthread_mutex_lock(&spa103_http_lock);
    profile = spa103_private_profile();
    if (!profile || !spa_cf_resolve() || !spa103_resolve()) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: cannot resolve CFLite/HTTP runtime before dispatcher install");
        goto done;
    }
    ensureAddress = (uintptr_t)dlsym(RTLD_NEXT, "AirTunesServer_EnsureCreated");
    if (ensureAddress < profile->ensureCreatedVA) {
        LOG_ERROR(LOG_MODULE, "buffer103: AirTunesServer_EnsureCreated unavailable");
        goto done;
    }
    imageBase = ensureAddress - profile->ensureCreatedVA;
    if ((imageBase & 0xfffu) != 0 ||
        memcmp((const void *)(imageBase + profile->ensureCreatedVA),
               profile->ensureCreatedPrologue,
               sizeof(profile->ensureCreatedPrologue)) != 0 ||
        memcmp((const void *)(imageBase + profile->setPlistResponseVA),
               profile->setPlistResponsePrologue,
               sizeof(profile->setPlistResponsePrologue)) != 0) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: libairplay private dispatcher fingerprint mismatch; refusing patch");
        goto done;
    }
    spa103_http_set_plist_response = (spa_http_set_plist_response_t)
        (imageBase + profile->setPlistResponseVA);

    outer = *(void **)(imageBase + profile->serverGlobalVA);
    if (!outer || ((uintptr_t)outer & 3u) != 0) {
        LOG_ERROR(LOG_MODULE, "buffer103: AirTunes server object unavailable");
        goto done;
    }
    server = *(void **)outer;
    if (!server || ((uintptr_t)server & 3u) != 0) {
        LOG_ERROR(LOG_MODULE, "buffer103: HTTPServer object unavailable");
        goto done;
    }
    callbackSlot = (void **)((uint8_t *)server + profile->httpProcessSlotOffset);
    if (*callbackSlot == (void *)spa103_http_process && spa103_original_http_process) {
        spa103_http_process_slot = callbackSlot;
        installed = 1;
        goto done;
    }
    if (*callbackSlot != (void *)(imageBase + profile->httpProcessVA)) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: HTTPServer request slot mismatch actual=%p expected=%p; refusing patch",
                  *callbackSlot, (void *)(imageBase + profile->httpProcessVA));
        goto done;
    }

    spa103_original_http_process = (spa_http_process_t)*callbackSlot;
    __sync_synchronize();
    *callbackSlot = (void *)spa103_http_process;
    spa103_http_process_slot = callbackSlot;
    __sync_synchronize();
    LOG_WARN(LOG_MODULE,
             "buffer103: installed fingerprint-locked buffered RTSP dispatcher server=%p slot=%p",
             server, callbackSlot);
    installed = 1;

done:
    pthread_mutex_unlock(&spa103_http_lock);
    return installed;
}

/* AirTunesServer_EnsureCreated is called through libairplay's preemptible PLT. Install immediately
 * after the stock function creates its HTTPServer, before the first control connection copies the
 * server callback table. Installing later from type-103 SETUP is too late for that existing TCP
 * connection: hardware then receives APAP but none of SETMAGICCOOKIE/SETRATE* reaches our slot.
 *
 * Keep the wrapper for the dio_manager process lifetime. It delegates every stock verb byte-for-
 * byte and custom verbs additionally require SPATIAL_103, so type-100 PCM remains stock.
 */
typedef int (*spa_air_tunes_ensure_created_t)(void);

int AirTunesServer_EnsureCreated(void)
{
    static spa_air_tunes_ensure_created_t real_ensure_created;
    int err;

    if (!real_ensure_created)
        real_ensure_created = (spa_air_tunes_ensure_created_t)
            dlsym(RTLD_NEXT, "AirTunesServer_EnsureCreated");
    if (!real_ensure_created) {
        LOG_ERROR(LOG_MODULE, "buffer103: stock AirTunesServer_EnsureCreated unavailable");
        return -6735;
    }

    err = real_ensure_created();
    if (!err && spa_probe103_mode() && !spa103_install_buffered_dispatcher())
        LOG_ERROR(LOG_MODULE,
                  "buffer103: early RTSP dispatcher install failed; buffered audio unavailable");
    return err;
}

static int spa103_resolve(void)
{
    if (!spa103_srv_sockopen)
        spa103_srv_sockopen = (spa_srv_sockopen_t)dlsym(RTLD_DEFAULT, "ServerSocketOpen");
    if (!spa103_sock_accept)
        spa103_sock_accept = (spa_sock_accept_t)dlsym(RTLD_DEFAULT, "SocketAccept");
    if (!spa103_cf_create_plist_bytes)
        spa103_cf_create_plist_bytes =
            (spa_cf_create_plist_bytes_t)dlsym(RTLD_DEFAULT, "CFCreateWithPlistBytes");
    if (!spa103_cf_dictionary_type_id)
        spa103_cf_dictionary_type_id =
            (spa_cf_type_id_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetTypeID");
    if (!spa103_cf_data_type_id)
        spa103_cf_data_type_id =
            (spa_cf_type_id_t)dlsym(RTLD_DEFAULT, "CFDataGetTypeID");
    if (!spa103_cf_dict_get_typed)
        spa103_cf_dict_get_typed =
            (spa_cf_dict_get_typed_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetTypedValue");
    if (!spa103_cf_data_length)
        spa103_cf_data_length =
            (spa_cf_data_length_t)dlsym(RTLD_DEFAULT, "CFDataGetLength");
    if (!spa103_cf_data_bytes)
        spa103_cf_data_bytes =
            (spa_cf_data_bytes_t)dlsym(RTLD_DEFAULT, "CFDataGetBytePtr");
    if (!spa103_http_init_response)
        spa103_http_init_response =
            (spa_http_init_response_t)dlsym(RTLD_DEFAULT, "HTTPConnectionInitResponse");
    if (!spa103_http_send_response)
        spa103_http_send_response =
            (spa_http_send_response_t)dlsym(RTLD_DEFAULT, "HTTPConnectionSendResponse");
    if (!spa103_up_ticks)
        spa103_up_ticks = (spa_up_ticks_t)dlsym(RTLD_DEFAULT, "UpTicks");
    if (!spa103_milliseconds_to_up_ticks)
        spa103_milliseconds_to_up_ticks = (spa_milliseconds_to_up_ticks_t)
            dlsym(RTLD_DEFAULT, "MillisecondsToUpTicks");
    if (!spa103_clock_get_near_ticks)
        spa103_clock_get_near_ticks = (spa_air_tunes_clock_get_near_ticks_t)
            dlsym(RTLD_DEFAULT, "AirTunesClock_GetSynchronizedTimeNearUpTicks");
    if (!spa103_srv_sockopen || !spa103_sock_accept || !spa103_cf_create_plist_bytes ||
        !spa103_cf_dictionary_type_id || !spa103_cf_data_type_id ||
        !spa103_cf_dict_get_typed || !spa103_cf_data_length || !spa103_cf_data_bytes ||
        !spa103_http_init_response || !spa103_http_send_response || !spa103_up_ticks ||
        !spa103_milliseconds_to_up_ticks || !spa103_clock_get_near_ticks) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: socket/CFLite/HTTP symbols unavailable; receiver disabled");
        return 0;
    }
    return 1;
}

static void spa103_deadline_after_ms(struct timespec *deadline, int milliseconds)
{
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_sec += milliseconds / 1000;
    deadline->tv_nsec += (long)(milliseconds % 1000) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

static int spa103_add_response_stream(void *response, void *streamDesc)
{
    void *key, *streams;

    key = cfa_str(0, "streams", SPA_CFENC_UTF8);
    if (!key) return -1;
    streams = cfa_dictget(response, key);
    if (streams) {
        cfa_arrapp(streams, streamDesc);
    } else {
        streams = cfa_arrm(0, 0, cfa_arr_cb);
        if (!streams) {
            cfa_rel(key);
            return -1;
        }
        cfa_arrapp(streams, streamDesc);
        cfa_dictset(response, key, streams);
        cfa_rel(streams);
    }
    cfa_rel(key);
    return 0;
}

static int spa103_response_set_i64(void *responseDesc, const char *name, int64_t value)
{
    void *key = cfa_str(0, name, SPA_CFENC_UTF8);
    if (!key) return -1;
    cfa_dictseti(responseDesc, key, value);
    cfa_rel(key);
    return 0;
}

static int spa103_audio_buffer_size(const void *requestStream)
{
    int64_t formatIndex = 0;
    int primaryIndex = spa_probe103_index();
    int extraIndex = spa_probe103_extra_index(primaryIndex);

    if (!spa_cf_get_i64(requestStream, "audioFormatIndex", &formatIndex))
        return SPA_103_AUDIO_BUFFER_SIZE_OTHER;
    return spa_type103_audio_buffer_size_for_offer(formatIndex,
                                                    primaryIndex,
                                                    extraIndex);
}

static size_t spa103_write_capture(int fd, const uint8_t *data, size_t size)
{
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(fd, data + done, size - done);
        if (n > 0) {
            done += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    return done;
}

static void spa103_log_prefix(const char *transport, const uint8_t *data, size_t size)
{
    char text[3 * 32 + 1];
    size_t i, count = size < 32 ? size : 32;
    char *p = text;

    for (i = 0; i < count; i++) {
        int n = snprintf(p, (size_t)(text + sizeof(text) - p),
                         i ? " %02x" : "%02x", data[i]);
        if (n <= 0) break;
        p += n;
    }
    *p = '\0';
    LOG_INFO(LOG_MODULE, "buffer103: %s first %u bytes: %s", transport,
             (unsigned)count, text);
}

static int spa103_receiver_should_stop(uint32_t generation)
{
    int stop;

    pthread_mutex_lock(&spa103_lock);
    stop = spa103_receiver.stop || spa103_receiver.generation != generation;
    pthread_mutex_unlock(&spa103_lock);
    return stop;
}

static void *spa103_thread_main(void *opaque)
{
    spa103_thread_arg_t *arg = (spa103_thread_arg_t *)opaque;
    const uint32_t generation = arg->generation;
    const uint32_t sequence = arg->sequence;
    const int listenSock = arg->listenSock;
    const int isControl = arg->isControl;
    const char *transport = isControl ? "RTCP" : "APAP";
    const size_t captureLimit = ENABLE_AUDIO_DIAGNOSTICS ?
        (isControl ? SPA_103_CONTROL_CAPTURE_LIMIT : SPA_103_CAPTURE_LIMIT) : 0;
    uint8_t buffer[16384];
    uint8_t prefix[32];
    char path[128] = { 0 };
    uint64_t total = 0;
    uint64_t apapFrames = 0;
#if ENABLE_STATE_TRACE
    uint64_t lastApapFrameNs = 0;
    uint64_t intervalMaxApapGapNs = 0;
    uint64_t apapFrameGaps = 0;
#endif
    size_t saved = 0;
    size_t prefixSize = 0;
    /* The frame body is retained now, so a whole APAP packet can be handed to the decoder.
     * spa_apap_framer_* owns the assembly; the loop keeps the one-framing-phase-per-read clamp
     * that stops the drain from defeating TCP flow control. */
    spa_apap_framer_t framer;
    uint8_t *frameBuf = 0;
    uint8_t *queuePlaintext = 0;
    int fd = -1, captureFd = -1, accepted = 0, closeListen = 0;
    int prefixLogged = 0;
    int queueSaturationReported = 0;
    free(arg);

    /* This thread becomes the physical QSA writer after stock type-100 is
     * backgrounded.  Leaving it at QNX's default priority 10 allowed a >128 ms
     * scheduling stall under a 98% CPU spike to drain all eight 16-ms QSA
     * fragments; libasound then hid the underrun behind STOP/prepare/GO while
     * still returning a full write.  It is an existing receiver thread, not a
     * new render thread, and priority 18 matches the stock AirPlay audio
     * receiver on this image. */
    if (!isControl) {
        struct sched_param sched;
        unsigned priority = spa_env_uint("AUDIO_BUFFERED_THREAD_PRIO",
                                         spa_env_uint("AUDIO_RECEIVER_THREAD_PRIO", 18u));
        int result;

        if (priority < 1u) priority = 1u;
        if (priority > 63u) priority = 63u;
        memset(&sched, 0, sizeof(sched));
        sched.sched_priority = (int)priority;
        result = pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
        if (result == 0)
            LOG_INFO(LOG_MODULE,
                     "buffer103: APAP receiver/QSA writer priority=%u policy=SCHED_RR",
                     priority);
        else
            LOG_ERROR(LOG_MODULE,
                      "buffer103: cannot raise APAP receiver/QSA writer priority to %u "
                      "result=%d; underrun protection is reduced",
                      priority, result);
    }

    while (!spa103_receiver_should_stop(generation)) {
        fd = -1;
        if (spa103_sock_accept(listenSock, 1, &fd, 0) == 0 && fd >= 0) break;
    }

    pthread_mutex_lock(&spa103_lock);
    if (spa103_receiver.generation == generation &&
        (isControl ? spa103_receiver.controlListenSock : spa103_receiver.listenSock) ==
            listenSock) {
        if (isControl)
            spa103_receiver.controlListenSock = -1;
        else
            spa103_receiver.listenSock = -1;
        closeListen = 1;
        if (!spa103_receiver.stop && fd >= 0) {
            if (isControl)
                spa103_receiver.controlClientSock = fd;
            else
                spa103_receiver.clientSock = fd;
            accepted = 1;
        }
    }
    pthread_mutex_unlock(&spa103_lock);
    if (closeListen) close(listenSock);
    if (!accepted) goto done;

    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
            LOG_WARN(LOG_MODULE, "buffer103: failed to set client socket non-blocking errno=%d",
                     errno);
    }
    if (ENABLE_AUDIO_DIAGNOSTICS) {
        snprintf(path, sizeof(path), isControl ? "/tmp/spatial_rtcp_%u.bin"
                                               : "/tmp/spatial_apap_%u.bin",
                 (unsigned)sequence);
        captureFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (captureFd < 0)
            LOG_ERROR(LOG_MODULE,
                      "buffer103: cannot open %s errno=%d; draining without capture",
                      path, errno);
    }
    if (!isControl) {
        frameBuf = (uint8_t *)malloc(SPA_103_APAP_MAX_FRAME);
        queuePlaintext = (uint8_t *)malloc(SPA_103_APAP_MAX_FRAME);
        if (!frameBuf || !queuePlaintext)
            LOG_ERROR(LOG_MODULE,
                      "buffer103: cannot allocate APAP framing/lifecycle scratch (%u bytes each)",
                      (unsigned)SPA_103_APAP_MAX_FRAME);
        if (!frameBuf || !queuePlaintext) goto done;
        spa_apap_framer_init(&framer, frameBuf, SPA_103_APAP_MAX_FRAME);
    }
    LOG_INFO(LOG_MODULE,
             "buffer103: iPhone connected; draining %s generation=%u capture=%s limit=%u",
             transport, (unsigned)generation, path, (unsigned)captureLimit);

    while (!spa103_receiver_should_stop(generation)) {
        struct pollfd pfd;
        int pollResult;
        int pollTimeoutMs = 100;
        ssize_t n;
        size_t want = sizeof(buffer);
        size_t pumpPending = 0;

        if (!isControl) {
            int playing = 0, saturated = 0;
            int detached;
            size_t queuedPackets = 0;
            int haveQueue = spa103_render_queue_state(
                &playing, &queuedPackets, 0, &saturated);

            if (playing && haveQueue) {
                int drainResult = spa103_render_drain_one_queued();

                if (drainResult != 0) {
                    if (drainResult > 0) {
                        spa103_retry_now_playing_after_media();
                        (void)spa_pcm_sink_bootstrap_buffered();
                    }
                    spa_pcm_sink_write_one_detached_period();
                    if (drainResult < 0 &&
                        !spa_pcm_sink_detached_pending(0)) {
                        struct timespec waitForCookie = { 0, 1000000L };
                        while (nanosleep(&waitForCookie, &waitForCookie) != 0 &&
                               errno == EINTR) {}
                    }
                    continue;
                }
                /* Preserve packet order while decoded PCM is at high water.
                 * Retry cold bootstrap here too: a transient first open
                 * failure must not require another decoded packet to make
                 * progress once the ring is already at its decode limit.  A
                 * detached owner consumes one blocking QSA period; a stock
                 * owner drains asynchronously, so yield briefly. */
                (void)spa_pcm_sink_bootstrap_buffered();
                detached = spa_pcm_sink_detached_pending(&pumpPending);
                if (detached)
                    spa_pcm_sink_write_one_detached_period();
                else {
                    struct timespec yield = { 0, 1000000L };
                    while (nanosleep(&yield, &yield) != 0 && errno == EINTR) {}
                }
                continue;
            }
            if (!playing && saturated) {
                struct timespec wait = { 0, 10000000L };

                if (!queueSaturationReported) {
                    queueSaturationReported = 1;
                    LOG_WARN(LOG_MODULE,
                             "buffer103: paused compressed queue reached advertised limit; "
                             "applying TCP backpressure");
                    STATE_TRACE("BUFFERED_QUEUE_BACKPRESSURE",
                                "queued_packets=%u state=paused",
                                (unsigned)queuedPackets);
                }
                spa_pcm_sink_write_one_detached_period();
                while (nanosleep(&wait, &wait) != 0 && errno == EINTR) {}
                continue;
            }
            queueSaturationReported = 0;

            detached = spa_pcm_sink_detached_pending(&pumpPending);

            if (!spa_pcm_pump_should_read_source(
                    detached, pumpPending, SPA_PCM_SINK_PERIOD_FRAMES,
                    spa103_high_frames())) {
                /* At/above high water, do not consume another phone packet.
                 * One blocking device period is this entire pump iteration. */
                spa_pcm_sink_write_one_detached_period();
                continue;
            }
            pollTimeoutMs = spa_pcm_pump_poll_timeout_ms(
                detached, pumpPending, SPA_PCM_SINK_PERIOD_FRAMES, 100);
        }
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        pollResult = poll(&pfd, 1, pollTimeoutMs);
        if (pollResult < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pollResult == 0 ||
            !(pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
            if (!isControl) spa_pcm_sink_write_one_detached_period();
            continue;
        }
        if (!isControl) {
            /* Buffered APAP is BE32-inclusive-length framed.  Read exactly one framing phase at a
             * time so the capture drain itself cannot defeat TCP flow control by swallowing the
             * phone's entire multi-minute burst in a few seconds. */
            want = spa_apap_framer_want(&framer, want);
        }
        n = read(fd, buffer, want);
        if (n > 0) {
            size_t keep;
            total += (uint64_t)n;
            if (ENABLE_AUDIO_DIAGNOSTICS &&
                !prefixLogged && prefixSize < sizeof(prefix)) {
                size_t copy = sizeof(prefix) - prefixSize;
                if (copy > (size_t)n) copy = (size_t)n;
                memcpy(prefix + prefixSize, buffer, copy);
                prefixSize += copy;
                if (prefixSize == sizeof(prefix)) {
                    spa103_log_prefix(transport, prefix, prefixSize);
                    prefixLogged = 1;
                }
            }
            keep = saved < captureLimit ? captureLimit - saved : 0;
            if (keep > (size_t)n) keep = (size_t)n;
            if (captureFd >= 0 && keep)
                saved += spa103_write_capture(captureFd, buffer, keep);

            if (!isControl) {
                size_t at = 0;
                int completedFrame = 0;
                int framingFailed = 0;

                while (at < (size_t)n) {
                    const uint8_t *frame = 0;
                    size_t frameLength = 0, consumed = 0;
                    const int rc = spa_apap_framer_push(&framer, buffer + at, (size_t)n - at,
                                                        &consumed, &frame, &frameLength);
                    at += consumed;
                    if (rc < 0) {
                        LOG_ERROR(LOG_MODULE,
                                  "buffer103: invalid APAP inclusive frame length after %llu "
                                  "frames; closing capture receiver",
                                  (unsigned long long)apapFrames);
                        framingFailed = 1;
                        break;
                    }
                    if (rc == 0) break;
                    completedFrame = 1;

                    {
                        struct timespec pace = { 0, SPA_103_APAP_FRAME_NS };
                        apapFrames++;

#if ENABLE_STATE_TRACE
                        {
                            uint64_t nowNs = spa_monotonic_ns();
                            uint64_t gapNs = 0;

                            if (nowNs && lastApapFrameNs && nowNs > lastApapFrameNs)
                                gapNs = nowNs - lastApapFrameNs;
                            if (gapNs > intervalMaxApapGapNs)
                                intervalMaxApapGapNs = gapNs;
                            if (gapNs > SPA_FEEDBACK_CLOCK_MAX_GAP_NS) {
                                apapFrameGaps++;
                                /* Snapshot before decoding this record.  A
                                 * low ring here means the existing PCM cushion
                                 * really was exposed during the gap. */
                                spa103_trace_apap_gap(apapFrames, gapNs,
                                                     apapFrameGaps);
                            }
                            if (nowNs) lastApapFrameNs = nowNs;
                        }
#endif

                        if (frame && frameLength) {
                            if (spa103_receive_frame(
                                    frame, frameLength, queuePlaintext,
                                    SPA_103_APAP_MAX_FRAME) < 0) {
                                framingFailed = 1;
                                break;
                            }
                            spa103_retry_now_playing_after_media();
                            (void)spa_pcm_sink_bootstrap_buffered();
                        }

#if ENABLE_STATE_TRACE
                        if ((apapFrames % 256u) == 0) {
                            spa103_trace_buffered_health(
                                apapFrames, intervalMaxApapGapNs,
                                apapFrameGaps);
                            intervalMaxApapGapNs = 0;
                        }
#endif

                        /* Apple's buffered hose queues 32 packages and then suspends network
                         * reads until playback consumes one.  Real consumption is the
                         * backpressure now: spa103_render_backpressure() is false only while a
                         * sink is actually draining the decoded ring, so with decoding off or
                         * unconsumed this is the same unconditional one-package-per-1024/48k
                         * pace the capture-only build used, and the receiver still never
                         * advertises 21 MB and swallows a hundred seconds of audio in four and a
                         * half. */
                        if (apapFrames >= SPA_103_APAP_PREFILL_FRAMES &&
                            spa_pcm_source_should_pace(
                                spa103_render_backpressure(),
                                spa_pcm_sink_detached_pending(0),
                                spa103_render_flush_pending())) {
                            while (nanosleep(&pace, &pace) != 0 && errno == EINTR) {}
                        }
                    }
                }
                if (framingFailed) break;
                if (!completedFrame && spa_pcm_sink_detached_pending(0)) {
                    struct pollfd more;

                    /* A header and body are separate framing phases by
                     * contract.  If the next phase is already queued, finish
                     * the same APAP record before consuming a QSA period; this
                     * prevents two sink writes per one 1024-frame packet. */
                    more.fd = fd;
                    more.events = POLLIN;
                    more.revents = 0;
                    if (poll(&more, 1, 0) > 0 &&
                        (more.revents & POLLIN))
                        continue;
                }
            }
        } else if (n == 0) {
            break;
        } else if (errno != EINTR && errno != EAGAIN) {
            break;
        }
        if (!isControl) spa_pcm_sink_write_one_detached_period();
    }

done:
#if ENABLE_STATE_TRACE
    if (!isControl && apapFrames && (apapFrames % 256u) != 0)
        spa103_trace_buffered_health(apapFrames, intervalMaxApapGapNs,
                                     apapFrameGaps);
#endif
    free(frameBuf);
    free(queuePlaintext);
    if (captureFd >= 0) close(captureFd);
    pthread_mutex_lock(&spa103_lock);
    if (spa103_receiver.generation == generation) {
        int *clientSlot = isControl ? &spa103_receiver.controlClientSock
                                    : &spa103_receiver.clientSock;
        if (*clientSlot == fd) {
            *clientSlot = -1;
        } else {
            fd = -1; /* teardown already took and closed ownership */
        }
        if (isControl)
            spa103_receiver.controlThreadExited = 1;
        else
            spa103_receiver.threadExited = 1;
        pthread_cond_broadcast(&spa103_cond);
    }
    pthread_mutex_unlock(&spa103_lock);
    if (fd >= 0) close(fd);
    {
        int stopped = spa103_receiver_should_stop(generation);
        LOG_INFO(LOG_MODULE,
             "buffer103: %s receiver exit generation=%u total=%llu saved=%u frames=%llu stop=%d",
             transport, (unsigned)generation, (unsigned long long)total,
             (unsigned)saved, (unsigned long long)apapFrames, stopped);
    }
    return 0;
}

/* Stop and join the one CarPlay buffered-media receiver. There is only one main media stream, so
 * replacing a stale session globally is safer than letting a reconnect retain an orphaned port. */
static void spa103_stop_receiver(void *session, int anySession)
{
    pthread_t thread, controlThread;
    uint32_t generation;
    int created, controlCreated;
    int listenSock = -1, clientSock = -1;
    int controlListenSock = -1, controlClientSock = -1;

    pthread_mutex_lock(&spa103_lock);
    if (!spa103_receiver.session ||
        (!anySession && spa103_receiver.session != session)) {
        pthread_mutex_unlock(&spa103_lock);
        if (anySession) {
            spa_pcm_sink_close_detached("buffered receiver absent");
            spa_feedback_forget(0, 1);
        }
        return;
    }
    generation = spa103_receiver.generation;
    spa103_receiver.stop = 1;
    listenSock = spa103_receiver.listenSock;
    clientSock = spa103_receiver.clientSock;
    controlListenSock = spa103_receiver.controlListenSock;
    controlClientSock = spa103_receiver.controlClientSock;
    spa103_receiver.listenSock = -1;
    spa103_receiver.clientSock = -1;
    spa103_receiver.controlListenSock = -1;
    spa103_receiver.controlClientSock = -1;
    thread = spa103_receiver.thread;
    created = spa103_receiver.threadCreated;
    controlThread = spa103_receiver.controlThread;
    controlCreated = spa103_receiver.controlThreadCreated;
    pthread_mutex_unlock(&spa103_lock);

    if (listenSock >= 0) close(listenSock);
    if (clientSock >= 0) close(clientSock);
    if (controlListenSock >= 0) close(controlListenSock);
    if (controlClientSock >= 0) close(controlClientSock);
    if (created && !pthread_equal(thread, pthread_self())) pthread_join(thread, 0);
    if (controlCreated && !pthread_equal(controlThread, pthread_self()))
        pthread_join(controlThread, 0);

    /* The APAP thread is now unable to issue another detached write. */
    spa_pcm_sink_close_detached("buffered receiver stopped");

    pthread_mutex_lock(&spa103_lock);
    if (spa103_receiver.generation == generation) {
        spa103_receiver.session = 0;
        spa103_receiver.listenSock = -1;
        spa103_receiver.clientSock = -1;
        spa103_receiver.controlListenSock = -1;
        spa103_receiver.controlClientSock = -1;
        spa103_receiver.threadCreated = 0;
        spa103_receiver.threadExited = 0;
        spa103_receiver.controlThreadCreated = 0;
        spa103_receiver.controlThreadExited = 0;
        spa103_receiver.settingUp = 0;
        spa103_receiver.stop = 0;
        spa103_receiver.sequence = 0;
        spa103_receiver.rate = 0;
        memset(&spa103_receiver.anchor, 0, sizeof(spa103_receiver.anchor));
        memset(&spa103_receiver.firstAudible, 0, sizeof(spa103_receiver.firstAudible));
        memset(&spa103_receiver.networkTime, 0, sizeof(spa103_receiver.networkTime));
        spa103_receiver.controlRequestCount = 0;
        spa103_receiver.nowPlayingRetryAttempts = 0;
        spa103_receiver.nowPlayingRetryAfterNs = 0;
        spa103_receiver.nowPlayingRetryQueued = 0;
        memset(spa103_receiver.shk, 0, sizeof(spa103_receiver.shk));
        spa103_receiver.haveKey = 0;
        spa103_receiver.formatIndex = 0;
        spa103_receiver.haveSoundCheckTarget = 0;
        spa103_receiver.soundCheckMediaKind = 0;
        spa103_receiver.soundCheckTargetDb = 0;
        spa103_clear_magic_cookies_locked();
        pthread_cond_broadcast(&spa103_cond);
    }
    pthread_mutex_unlock(&spa103_lock);
    spa_feedback_forget(session, anySession);
    /* No stale decoder, ring or key survives into the next CarPlay generation. */
    spa103_render_stop();
}

static int spa_setup_buffered_receiver(void *session, void *request, void *response,
                                uint32_t sequence)
{
    const spa103_private_profile_t *profile = spa103_private_profile();
    spa103_thread_arg_t *threadArg = 0;
    void *streamDesc = 0, *responseDesc = 0;
    uint8_t shk[SPA_APAP_KEY_SIZE] = { 0 };
    int64_t formatIndex = 0;
    int haveKey = 0;
    uint64_t streamID = 0;
    int64_t supportsDynamicStreamID = 0;
    int haveSoundCheckTarget = 0;
    uint8_t soundCheckMediaKind = 0;
    int soundCheckTargetDb = 0;
    uint32_t generation = 0;
    int audioBufferSize, family, port = 0;
    int sock = -1, err = -6728;
    int started = 0;

    if (!profile || !session || !request || !response || !spa_probe103() ||
        !spa_cf_resolve() || !spa103_resolve()) return -6714;
    streamDesc = spa_find_setup_stream(request, SPA_103_STREAM_TYPE);
    if (!streamDesc) return 0;
    audioBufferSize = spa103_audio_buffer_size(streamDesc);
    haveKey = spa103_read_shk(streamDesc, shk);
    if (!spa_cf_get_i64(streamDesc, "audioFormatIndex", &formatIndex)) formatIndex = 0;
    haveSoundCheckTarget = spa103_read_soundcheck_target(
        streamDesc, &soundCheckMediaKind, &soundCheckTargetDb);
    if (!spa103_validate_setup_stream(streamDesc, formatIndex, haveKey)) {
        memset(shk, 0, sizeof(shk));
        return -6714;
    }

    spa103_stop_receiver(0, 1);
    pthread_mutex_lock(&spa103_lock);
    spa103_receiver.generation++;
    if (!spa103_receiver.generation) spa103_receiver.generation++;
    generation = spa103_receiver.generation;
    spa103_receiver.session = session;
    spa103_receiver.sequence = sequence;
    spa103_receiver.stop = 0;
    spa103_receiver.settingUp = 1;
    spa103_receiver.listenSock = -1;
    spa103_receiver.clientSock = -1;
    spa103_receiver.controlListenSock = -1;
    spa103_receiver.controlClientSock = -1;
    spa103_receiver.threadCreated = 0;
    spa103_receiver.controlThreadCreated = 0;
    spa103_receiver.rate = 0;
    memset(&spa103_receiver.anchor, 0, sizeof(spa103_receiver.anchor));
    memset(&spa103_receiver.firstAudible, 0, sizeof(spa103_receiver.firstAudible));
    memset(&spa103_receiver.networkTime, 0, sizeof(spa103_receiver.networkTime));
    spa103_receiver.controlRequestCount = 0;
    spa103_receiver.nowPlayingRetryAttempts = 0;
    spa103_receiver.nowPlayingRetryAfterNs = 0;
    spa103_receiver.nowPlayingRetryQueued = 0;
    memcpy(spa103_receiver.shk, shk, sizeof(spa103_receiver.shk));
    spa103_receiver.haveKey = haveKey;
    spa103_receiver.formatIndex = formatIndex;
    spa103_receiver.haveSoundCheckTarget = haveSoundCheckTarget;
    spa103_receiver.soundCheckMediaKind = soundCheckMediaKind;
    spa103_receiver.soundCheckTargetDb = (int8_t)soundCheckTargetDb;
    pthread_mutex_unlock(&spa103_lock);
    memset(shk, 0, sizeof(shk));
    spa103_render_start(formatIndex, haveKey, (size_t)audioBufferSize,
                        haveSoundCheckTarget, soundCheckMediaKind,
                        soundCheckTargetDb);

    family = *(const uint8_t *)((const char *)session + profile->sessionFamilyOffset);
    err = spa103_srv_sockopen(family, 1 /* SOCK_STREAM */, 6 /* IPPROTO_TCP */,
                              SPA_103_PREFERRED_PORT, &port, -180224, &sock);
    LOG_INFO(LOG_MODULE,
             "buffer103: ServerSocketOpen err=%d family=%d port=%d sock=%d sequence=%u",
             err, family, port, sock, (unsigned)sequence);
    if (err || sock < 0 || port <= 0) goto fail;

    /* SETMAGICCOOKIE and the four buffered-control verbs use the existing RTSP connection, not a
     * 6033/RTCP payload. AirTunesServer_EnsureCreated installed the dispatcher before that
     * connection was accepted; re-check its fingerprinted slot before publishing this SETUP. */
    if (!spa103_install_buffered_dispatcher()) {
        err = -6735;
        goto fail;
    }

    responseDesc = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
    if (!responseDesc) { err = -6728; goto fail; }
    if (spa103_response_set_i64(responseDesc, "type", SPA_103_STREAM_TYPE) != 0 ||
        spa103_response_set_i64(responseDesc, "dataPort", port) != 0 ||
        spa103_response_set_i64(responseDesc, "audioBufferSize", audioBufferSize) != 0 ||
        spa103_response_set_i64(responseDesc, "arrivalToRenderLatencyMs", 0) != 0) {
        err = -6728;
        goto fail;
    }
    if (spa_cf_get_i64(streamDesc, "supportsDynamicStreamID", &supportsDynamicStreamID) &&
        supportsDynamicStreamID) {
        /* One 103 stream exists at a time. Pair the monotonically increasing hook sequence with the
         * nonzero receiver generation so reconnects cannot reuse a live ID. */
        streamID = ((uint64_t)sequence << 32) | (uint64_t)generation;
        if (!streamID) streamID = 1;
        if (spa103_response_set_i64(responseDesc, "streamID", (int64_t)streamID) != 0) {
            err = -6728;
            goto fail;
        }
    }

    threadArg = (spa103_thread_arg_t *)calloc(1, sizeof(*threadArg));
    if (!threadArg) { err = -6728; goto fail; }
    threadArg->generation = generation;
    threadArg->sequence = sequence;
    threadArg->listenSock = sock;
    threadArg->isControl = 0;

    pthread_mutex_lock(&spa103_lock);
    if (spa103_receiver.generation != generation || spa103_receiver.stop ||
        !spa103_receiver.settingUp || spa103_receiver.session != session) {
        pthread_mutex_unlock(&spa103_lock);
        err = -6719;
        goto fail;
    }
    spa103_receiver.listenSock = sock;
    sock = -1;        /* receiver state owns the APAP listening socket from here */
    if (pthread_create(&spa103_receiver.thread, 0, spa103_thread_main, threadArg) == 0) {
        spa103_receiver.threadCreated = 1;
        spa103_receiver.threadExited = 0;
        started = 1;
        threadArg = 0;
    } else {
        LOG_ERROR(LOG_MODULE, "buffer103: failed to create APAP receiver thread errno=%d", errno);
    }
    pthread_mutex_unlock(&spa103_lock);
    if (!started) { err = -6728; goto fail; }

    if (spa103_add_response_stream(response, responseDesc) != 0) {
        err = -6728;
        goto fail;
    }
    cfa_rel(responseDesc);
    responseDesc = 0;
    pthread_mutex_lock(&spa103_lock);
    if (spa103_receiver.generation != generation || spa103_receiver.stop) {
        pthread_mutex_unlock(&spa103_lock);
        err = -6719;
        goto fail;
    }
    spa103_receiver.settingUp = 0;
    pthread_cond_broadcast(&spa103_cond);
    pthread_mutex_unlock(&spa103_lock);
    spa_write_setup103_plist(response, "hook_response", sequence);
    LOG_WARN(LOG_MODULE,
             "buffer103: response committed {type:103,dataPort:%d,audioBufferSize:%d,"
             "arrivalToRenderLatencyMs:0,streamID:%llu}; APAP receiver/render rail live",
             port, audioBufferSize, (unsigned long long)streamID);
    return 0;

fail:
    if (responseDesc) cfa_rel(responseDesc);
    if (threadArg) free(threadArg);
    if (sock >= 0) close(sock);
    pthread_mutex_lock(&spa103_lock);
    if (spa103_receiver.generation == generation) {
        spa103_receiver.stop = 1;
        spa103_receiver.settingUp = 0;
        pthread_cond_broadcast(&spa103_cond);
    }
    pthread_mutex_unlock(&spa103_lock);
    spa103_stop_receiver(session, 0);
    LOG_ERROR(LOG_MODULE, "buffer103: setup failed err=%d sequence=%u", err,
              (unsigned)sequence);
    return err;
}

static void spa_teardown_buffered_receiver(void *session, void *request)
{
    int requestHas103 = 0;
    int tearDown103 = request == 0;

    if (request && spa_cf_resolve()) {
        requestHas103 = spa_find_setup_stream(request, SPA_103_STREAM_TYPE) != 0;
        if (requestHas103) tearDown103 = 1;
    }
    if (tearDown103) {
        struct timespec deadline;
        int waitResult = 0;
        pthread_mutex_lock(&spa103_lock);
        if (spa103_receiver.session == session) {
            spa103_receiver.stop = 1;
            spa103_deadline_after_ms(&deadline, SPA_103_TEARDOWN_WAIT_MS);
            while (spa103_receiver.settingUp && waitResult != ETIMEDOUT)
                waitResult = pthread_cond_timedwait(&spa103_cond, &spa103_lock, &deadline);
        }
        pthread_mutex_unlock(&spa103_lock);
        spa103_stop_receiver(session, 0);
        LOG_INFO(LOG_MODULE,
                 "buffer103: teardown session=%p requestType103=%d setupWait=%s",
                 session, requestHas103, waitResult == ETIMEDOUT ? "timeout" : "done");
    }

    if (!request) {
        pthread_mutex_lock(&spa_contract_lock);
        if (spa_mainbuffered_session == session) spa_mainbuffered_session = 0;
        pthread_mutex_unlock(&spa_contract_lock);
    }
}

/* ---- /info: append the type-103 buffered-media entry ---------------------------------------
 * Stock AirPlayReceiverServerPlatformCopyProperty @0x1fe08 builds `audioFormats` as a MUTABLE array
 * of two mutable dicts ('d'=100 main, 'e'=101 alt), each {type, audioInputFormats?, audioOutputFormats}.
 * Because the array it returns is mutable and unowned by anyone else yet, appending a third entry is
 * enough — no copy, no reallocation of the caller's object. */
typedef void *(*fn_copyprop_t)(void *, int, const void *, const void *, int32_t *);

void *AirPlayReceiverServerPlatformCopyProperty(void *server, int sessionID, const void *property,
                                                const void *qualifier, int32_t *outErr)
{
    static fn_copyprop_t real;
    void *result;
    void *kAudioFormats, *dict, *ext, *kType, *kAudioType, *kOut, *kExt;
    uint32_t legacyMask;

    if (!real) real = (fn_copyprop_t)dlsym(RTLD_NEXT, "AirPlayReceiverServerPlatformCopyProperty");
    if (!real) { LOG_ERROR(LOG_MODULE, "CopyProperty real==NULL"); if (outErr) *outErr = -6714; return 0; }

    result = real(server, sessionID, property, qualifier, outErr);
    if (!result || !property || !(spa_probe103_mode() & SPA_103_MODE_INFO) ||
        !spa_cf_resolve()) return result;

    kAudioFormats = cfa_str(0, "audioFormats", SPA_CFENC_UTF8);
    if (!kAudioFormats) return result;
    if (!cfa_equal(property, kAudioFormats)) { cfa_rel(kAudioFormats); return result; }
    cfa_rel(kAudioFormats);

    dict = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
    ext  = cfa_arrm(0, 2, cfa_arr_cb);
    legacyMask = spa_probe103_legacy_mask();
    kType      = cfa_str(0, "type",                       SPA_CFENC_UTF8);
    kAudioType = cfa_str(0, "audioType",                  SPA_CFENC_UTF8);
    kOut       = cfa_str(0, "audioOutputFormats",         SPA_CFENC_UTF8);
    kExt       = cfa_str(0, "audioOutputFormatsExtended", SPA_CFENC_UTF8);

    if (dict && ext && kType && kAudioType && kOut && kExt) {
        /* CFNumberCreateInt64 takes ONE argument on this stack (verified: libairplay calls it as
         * `CFNumberCreateInt64(4, 0)` = one int64 across two ARM regs), not (alloc, type, ptr). */
        void *num = 0, *extraNum = 0;
        void *(*cfnum)(int64_t) = (void *(*)(int64_t))dlsym(RTLD_DEFAULT, "CFNumberCreateInt64");
        int formatIndex = spa_probe103_index();
        int extraIndex = spa_probe103_extra_index(formatIndex);

        cfa_dictseti(dict, kType,      103);
        cfa_dictsetc(dict, kAudioType, "media", -1);
        cfa_dictseti(dict, kOut,       legacyMask);

        if (cfnum) {
            num = cfnum(formatIndex);
            if (extraIndex >= 0) extraNum = cfnum(extraIndex);
        }
        if (num && (extraIndex < 0 || extraNum)) {
            cfa_arrapp(ext, num);
            if (extraNum) cfa_arrapp(ext, extraNum);
            cfa_dictset(dict, kExt, ext);
            cfa_arrapp(result, dict);
            if (extraNum) {
                LOG_INFO(LOG_MODULE,
                         "probe103: /info audioFormats += {type:103, audioType:media, "
                         "audioOutputFormats:%#x, audioOutputFormatsExtended:[%d,%d]} "
                         "(experimental AAC bridge; now %ld entries)",
                         legacyMask, formatIndex, extraIndex, cfa_arrcount(result));
            } else {
                LOG_INFO(LOG_MODULE,
                         "probe103: /info audioFormats += {type:103, audioType:media, "
                         "audioOutputFormats:%#x, audioOutputFormatsExtended:[%d]} "
                         "(now %ld entries)",
                         legacyMask, formatIndex, cfa_arrcount(result));
            }
        } else {
            LOG_ERROR(LOG_MODULE,
                      "probe103: CFNumberCreateInt64 unavailable — entry NOT added");
        }
        if (extraNum) cfa_rel(extraNum);
        if (num) cfa_rel(num);
    }

    if (kExt) cfa_rel(kExt);
    if (kOut) cfa_rel(kOut);
    if (kAudioType) cfa_rel(kAudioType);
    if (kType) cfa_rel(kType);
    if (ext) cfa_rel(ext);
    if (dict) cfa_rel(dict);
    return result;
}

/* ---- control-SETUP response: add the "mainBuffered" feature token ---------------------------
 * The response key is `enabledFeatures` (the phone proposes under `features` in the REQUEST and reads
 * the negotiated set from `enabledFeatures` in the RESPONSE — established by the altscreen work).
 * Seed from whatever the receiver already accepted and add exactly one token; never echo the phone's
 * proposal, since each string flips a sibling endpoint flag. */
static int spa_augment_setup_response(void *session, void *request, void *response, uint32_t sequence)
{
    void *kReq, *proposed, *kEnab, *cur, *neg;
    long curN, i;
    int added = 0;
    unsigned mode;

    (void)session;
    if (!request || !response || !spa_cf_resolve()) return 0;

    /* control SETUP only — the media SETUPs carry no `features` proposal */
    kReq = cfa_str(0, "features", SPA_CFENC_UTF8);
    if (!kReq) return 0;
    proposed = cfa_dictget(request, kReq);
    if (!proposed) { cfa_rel(kReq); return 0; }
    cfa_rel(kReq);

    /* A new control SETUP retires any previous session's contract. Once this response enables the
     * feature we set the new owner below. */
    spa_set_mainbuffered_contract(session, 0, sequence);
    mode = spa_probe103_mode();
    if (!mode) return 0;

    spa_write_control_plist(request, "request", sequence);
    spa_log_feature_array("control request features", proposed);
    if (!(mode & SPA_103_MODE_FEATURE)) {
        LOG_INFO(LOG_MODULE, "probe103: mode=info; mainBuffered response mutation disabled");
        return 0;
    }
    if (!spa_array_contains_string(proposed, "mainBuffered")) {
        LOG_WARN(LOG_MODULE,
                 "probe103: phone did not propose mainBuffered; not adding an unoffered feature");
        return 0;
    }

    kEnab = cfa_str(0, "enabledFeatures", SPA_CFENC_UTF8);
    if (!kEnab) return 0;
    cur  = cfa_dictget(response, kEnab);
    curN = cur ? cfa_arrcount(cur) : 0;
    neg  = cfa_arrm(0, 0, cfa_arr_cb);
    if (neg) {
        for (i = 0; i < curN; i++) {
            void *e = cfa_arridx(cur, i);
            if (e) cfa_arrapp(neg, e);
        }
        if (spa_array_contains_string(cur, "mainBuffered")) {
            added = 1; /* already accepted by stock: preserve it without duplicating the token */
        } else if (cfa_arrappc && cfa_arrappc(neg, "mainBuffered", -1) == 0) {
            added = 1;
        } else {
            void *s = cfa_str(0, "mainBuffered", SPA_CFENC_UTF8);
            if (s) { cfa_arrapp(neg, s); cfa_rel(s); added = 1; }
        }
        if (added) {
            cfa_dictset(response, kEnab, neg);
            spa_set_mainbuffered_contract(session, 1, sequence);
            LOG_INFO(LOG_MODULE,
                     "probe103: control SETUP response enabledFeatures includes proposed "
                     "mainBuffered (preserved %ld existing)", curN);
        }
        cfa_rel(neg);
    }
    cfa_rel(kEnab);
    return added;
}

static void spa_augment_server_info(void *session, void *info)
{
    void *key, *dict;
    int dcx = 0;

    if (!session || !info || !spa_has_mainbuffered_contract(session) || !spa_cf_resolve()) return;
    key = cfa_str(0, "mainBufferedInfo", SPA_CFENC_UTF8);
    dict = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
    if (key && dict) {
        if (spa_probe103_dcx()) {
            void *dcxKey = cfa_str(0, "DCXEnabled", SPA_CFENC_UTF8);
            void **trueSlot = (void **)dlsym(RTLD_DEFAULT, "kCFLBooleanTrue");

            /* kCFLBooleanTrue is an exported pointer object on CFLite/ARM32: dlsym returns the
             * address of that slot, so dereference it exactly once before CFDictionarySetValue. */
            cfa_bool_true = trueSlot ? *trueSlot : 0;
            if (dcxKey && cfa_bool_true) {
                cfa_dictset(dict, dcxKey, cfa_bool_true);
                dcx = 1;
            } else {
                LOG_ERROR(LOG_MODULE,
                          "probe103: %s enabled but CFLite Boolean unavailable; DCX disabled",
                          SPA_103_DCX_ENV);
            }
            if (dcxKey) cfa_rel(dcxKey);
        }
        cfa_dictset(info, key, dict);
        if (dcx)
            LOG_WARN(LOG_MODULE,
                     "probe103: /info mainBufferedInfo={DCXEnabled:true} added (probe-only claim)");
        else
            LOG_INFO(LOG_MODULE,
                     "probe103: /info mainBufferedInfo={} added for negotiated mainBuffered "
                     "(DCXEnabled absent)");
    } else {
        LOG_ERROR(LOG_MODULE,
                  "probe103: failed to build required /info mainBufferedInfo — session will reject");
    }
    if (dict) cfa_rel(dict);
    if (key) cfa_rel(key);
}

static void spa_capture_setup_response(void *request, void *response, uint32_t sequence)
{
    void *kReq, *proposed, *kEnab, *enabled;

    if (!request || !response || !spa_probe103_mode() || !spa_cf_resolve()) return;
    kReq = cfa_str(0, "features", SPA_CFENC_UTF8);
    if (!kReq) return;
    proposed = cfa_dictget(request, kReq);
    cfa_rel(kReq);
    if (!proposed) return; /* not the control SETUP */

    spa_write_control_plist(response, "response", sequence);
    kEnab = cfa_str(0, "enabledFeatures", SPA_CFENC_UTF8);
    if (!kEnab) return;
    enabled = cfa_dictget(response, kEnab);
    cfa_rel(kEnab);
    spa_log_feature_array("final control response enabledFeatures", enabled);
}

static void spa_capture_server_info(void *info)
{
#if ENABLE_AUDIO_DIAGNOSTICS
    static volatile uint32_t info_sequence;
    char path[128];
    uint32_t sequence;

    if (!info || !spa_probe103_mode() || !spa_cf_resolve()) return;
    sequence = __sync_add_and_fetch(&info_sequence, 1);
    snprintf(path, sizeof(path), "/tmp/spatial_info_%u.bplist", (unsigned)sequence);
    spa_write_plist_path(info, path);
#else
    (void)info;
#endif
}

/* ---- control-plane trace for buffered media ------------------------------------------------
 *
 * A type-103 hose is not represented in stock 210's receiver-session object.  The data socket can
 * therefore be perfectly healthy while a later RTSP control request (feedback, rate/anchor, etc.)
 * observes only the stock 100/101 streams and causes the sender to pause.  Trace the public control
 * boundary before changing any semantics: old libairplay's request processors call this exported
 * entry through the PLT, so one interposer captures the exact command and its generated response.
 *
 * Vehicle measurement proved the stock {type:100,sr:<measured>} response is the
 * sender's only clock reference even while buffered media is active. Preserve it.
 * The old empty-list rewrite remains only behind explicit AUDIO_FEEDBACK_BLANK=1
 * for reproducing that failure; SPATIAL_103 gates logs and plist files. */
typedef int (*fn_session_control_t)(void *, uint32_t, const void *, const void *,
                                    const void *, void **);

int AirPlayReceiverSessionControl(void *session, uint32_t flags, const void *command,
                                  const void *qualifier, const void *params, void **responseOut)
{
    static fn_session_control_t real;
#if ENABLE_AUDIO_DIAGNOSTICS
    static volatile uint32_t controlSequence;
    uint32_t sequence = 0;
    char path[144];
    int tracing = 0;
#endif
    char commandText[96];
    int result;

    if (!real)
        real = (fn_session_control_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionControl");
    if (!real) {
        LOG_ERROR(LOG_MODULE, "probe103 control: AirPlayReceiverSessionControl real==NULL");
        return -6714;
    }

    commandText[0] = '\0';
    if (spa_probe103() && spa_cf_resolve()) {
        if (!command || !cfa_strgetc ||
            !cfa_strgetc(command, commandText, sizeof(commandText)))
            snprintf(commandText, sizeof(commandText), "<non-string:%p>", command);

#if ENABLE_AUDIO_DIAGNOSTICS
        sequence = __sync_add_and_fetch(&controlSequence, 1);
        tracing = 1;
        LOG_WARN(LOG_MODULE,
                 "probe103 control[%u]: -> command=%s flags=%#x session=%p qualifier=%p params=%p",
                 (unsigned)sequence, commandText, (unsigned)flags, session, qualifier, params);
        if (qualifier) {
            snprintf(path, sizeof(path), "/tmp/spatial_controlcmd_%u_qualifier.bplist",
                     (unsigned)sequence);
            spa_write_plist_path(qualifier, path);
        }
        if (params) {
            snprintf(path, sizeof(path), "/tmp/spatial_controlcmd_%u_params.bplist",
                     (unsigned)sequence);
            spa_write_plist_path(params, path);
        }
#endif
    }

    result = real(session, flags, command, qualifier, params, responseOut);

    /* A live trace of phone-lock/backgrounding establishes the exact gap:
     * type-100 TearDown closes its QSA stream while type-103 and its retained
     * QSA handle continue playing.  Stock then returns NULL here.  Do not
     * replay its final sample-rate forever: a track-switch trace captured
     * 48082.29 -> 48080.23 Hz during the handoff and the old cache advertised
     * that transient for 276 seconds.  Publish the retained sink's nominal
     * contract during measurement warmup, then its measured blocking-write
     * cadence.  The cached stock response remains a fail-safe only when the
     * detached QSA ownership state is unavailable. */
    if (result == 0 && responseOut &&
        strcmp(commandText, "updateFeedback") == 0 &&
        !spa_env_on("AUDIO_FEEDBACK_BLANK") &&
        spa103_is_active_for_session(session)) {
        if (*responseOut) {
            spa_feedback_cache(session, *responseOut);
        } else {
            double rateHz = 0.0;
            int measured = 0;

            if (spa_pcm_sink_detached_feedback_rate(&rateHz, &measured))
                *responseOut = spa_feedback_create_realtime_clock(rateHz);
            if (*responseOut) {
                LOG_WARN(LOG_MODULE,
                         "buffer103: updateFeedback published detached QSA clock "
                         "sr=%.6f source=%s session=%p",
                         rateHz, measured ? "measured" : "nominal-warmup", session);
            } else {
                *responseOut = spa_feedback_copy_cached(session);
                if (*responseOut)
                    LOG_WARN(LOG_MODULE,
                             "buffer103: updateFeedback fell back to the last stock clock "
                             "while detached QSA rate is unavailable session=%p",
                             session);
            }
            /* A buffered stream may be the first media stream in a cold
             * CarPlay session.  In that shape there is no stock type-100
             * response to cache and no detached QSA handle from which to
             * measure cadence yet.  Returning NULL repeats the exact broken
             * contract measured on the car: type 103 is active and decoded,
             * but the sender has no render-clock reference until foregrounding
             * Music happens to create type 100.  Bootstrap with the endpoint's
             * already-negotiated 48-kHz contract; once a stock or detached
             * clock exists, the branches above supersede this nominal value. */
            if (!*responseOut) {
                *responseOut = spa_feedback_create_realtime_clock((double)SPA_SR);
                if (*responseOut)
                    LOG_WARN(LOG_MODULE,
                             "buffer103: updateFeedback published nominal cold-start "
                             "clock sr=%u session=%p",
                             (unsigned)SPA_SR, session);
            }
            if (!*responseOut)
                LOG_ERROR(LOG_MODULE,
                          "buffer103: updateFeedback has no stock response and no cached "
                          "clock for active type-103 session=%p",
                          session);
        }
    }

    /* Blanking the feedback's stream list is how the phone was told nothing about our buffer.
     * The failing session shows what that costs: a flush boundary armed at 00:44:31 took until
     * 00:47:42 to clear, 191 seconds during which 212 packets arrived -- about one a second
     * against the ~47 a second the stream needs.  We discard until the boundary, the boundary
     * needs packets, and the phone will not send them because its flow control has been handed
     * an empty list.  AUDIO_FEEDBACK_BLANK=1 restores the old behaviour for an A/B. */
    if (result == 0 && responseOut && *responseOut &&
        strcmp(commandText, "updateFeedback") == 0 &&
        spa_env_on("AUDIO_FEEDBACK_BLANK") &&
        spa103_is_active_for_session(session)) {
        void *streamsKey = cfa_str(0, "streams", SPA_CFENC_UTF8);
        void *emptyStreams = cfa_arrm(0, 0, cfa_arr_cb);

        if (streamsKey && emptyStreams) {
            cfa_dictset(*responseOut, streamsKey, emptyStreams);
            LOG_WARN(LOG_MODULE,
                     "buffer103: updateFeedback stock streams replaced with [] for active "
                     "type-103 session=%p",
                     session);
        } else {
            LOG_ERROR(LOG_MODULE,
                      "buffer103: failed to build empty updateFeedback streams array; "
                      "leaving stock response unchanged");
        }
        if (emptyStreams) cfa_rel(emptyStreams);
        if (streamsKey) cfa_rel(streamsKey);
    }

#if ENABLE_AUDIO_DIAGNOSTICS
    if (tracing) {
        LOG_WARN(LOG_MODULE,
                 "probe103 control[%u]: <- command=%s result=%d response=%p",
                 (unsigned)sequence, commandText, result,
                 responseOut ? *responseOut : (void *)0);
        if (responseOut && *responseOut) {
            snprintf(path, sizeof(path), "/tmp/spatial_controlcmd_%u_response.bplist",
                     (unsigned)sequence);
            spa_write_plist_path(*responseOut, path);
        }
    }
#endif
#if ENABLE_STATE_TRACE
    if (commandText[0]) {
        int hasResponse = responseOut && *responseOut;
        int hasClock = hasResponse &&
                       strcmp(commandText, "updateFeedback") == 0 &&
                       spa_feedback_has_realtime_clock(*responseOut);
        STATE_TRACE("AIRPLAY_CONTROL",
                    "command=%s flags=0x%x session=%p result=%d response=%d "
                    "realtime_clock=%d buffered_active=%d",
                    commandText, (unsigned)flags, session, result,
                    hasResponse ? 1 : 0, hasClock ? 1 : 0,
                    spa103_is_active_for_session(session));
    }
#endif
    return result;
}

/* ---- module init ---------------------------------------------------------------------------- */

/* Same test spa_inspect_setup_request() makes, re-run on the response side.
 * The two callbacks used to share a local `setupHas103` inside altscreen's
 * wrapper; re-deriving it from the request is one CF lookup and keeps the seam
 * callbacks free of state that would have to be keyed by sequence. */
static int spa_request_has_103(void *request)
{
    if (!request || !spa_probe103_mode() || !spa_cf_resolve()) return 0;
    return spa_find_setup_stream(request, SPA_103_STREAM_TYPE) != 0;
}

/* ---- AirPlay seam callbacks (declared in audio_module_def) ------------------
 * Deliberately NOT gated on altScreen: type-103 buffered media has nothing to
 * do with the cluster stream and must work with the altScreen advert off.  Each
 * of these gates itself on its own policy variables and is a no-op without
 * them. */

/* Stock AirPlay 210 rejects type 103 before producing a response.  Inspect and
 * capture the phone's request first, so the receiver is built from what it
 * really sends rather than a guessed schema. */
static void spa_on_setup_request(hook_setup_ctx_t *setup)
{
    spa_state_trace_setup(setup->request, setup->sequence, "request", -1);
    spa_inspect_setup_request(setup->request, setup->sequence);
}

static void spa_on_setup_response(hook_setup_ctx_t *setup)
{
    int has103 = spa_request_has_103(setup->request);

    spa_note_setup_result(has103, setup->result, setup->response, setup->sequence);
    if (!setup->session || !setup->request || !setup->response) return;
    if (setup->result != 0) return;

    /* Stock 210 logs type 103 as unsupported but returns success with an empty
     * dictionary.  Supply the receiver-owned dataPort before the response
     * leaves the seam; otherwise iOS closes the complete screen session as soon
     * as playback starts. */
    if (has103) {
        int rc = spa_setup_buffered_receiver(setup->session, setup->request,
                                             setup->response, setup->sequence);
        if (rc != 0) { setup->result = rc; return; }
    }
    spa_augment_setup_response(setup->session, setup->request, setup->response,
                               setup->sequence);
}

/* After every module has merged its own keys into the response. */
static void spa_on_setup_response_final(hook_setup_ctx_t *setup)
{
    spa_state_trace_setup(setup->response, setup->sequence, "response",
                          setup->result);
    if (setup->request && setup->response)
        spa_capture_setup_response(setup->request, setup->response, setup->sequence);
}

/* spa_capture_server_info() predates the seam and takes only the dictionary. */
static void spa_capture_server_info_seam(void *session, void *info)
{
    (void)session;
    spa_capture_server_info(info);
}

static void spa_on_teardown(void *session, void *request, int reason)
{
    STATE_TRACE("AUDIO_TEARDOWN",
                "session=%p scope=%s reason=%d",
                session, request ? "stream" : "full", reason);
    /* A live type-103 socket belongs to this session and must be stopped before
     * stock frees it, independent of altScreen and of the runtime marker. */
    spa_teardown_buffered_receiver(session, request);
}

/* Stop the type-103 receiver at process teardown.  Without this the receiver
 * thread outlived every stock object it reads, because its only stop path was
 * a per-session AirPlay TearDown that a crashing or killed session never
 * delivers. */
static void audio_shutdown(void)
{
    spa103_stop_receiver(0, 1);
}

static void audio_init(void)
{
    const spa_audio_config_t *config = spa_audio_config();
    unsigned bit, channels;
    spa_tune(&bit, &channels);
    if (spa_enabled()) {
        LOG_INFO(LOG_MODULE,
                 "audio policy: spatial=%d atmos=%d mode=%s buffered=%d; "
                 "PCM fallback armed bit%u/%uch; codec=%s",
                 config->buffered && !config->atmos, config->atmos,
                 config->atmosMode == SPA_ATMOS_DOWNMIX ? "downmix" :
                 config->atmosMode == SPA_ATMOS_DROP ? "drop" : "passthrough",
                 config->buffered, bit, channels,
                 getenv(SPA_APAC_CODEC_PATH_ENV) ?
                 getenv(SPA_APAC_CODEC_PATH_ENV) : SPA_APAC_CODEC_DEFAULT_PATH);
    }
    else
        LOG_INFO(LOG_MODULE,
                 "audio policy OFF (AUDIO_SPATIAL=0, AUDIO_SPATIAL_ATMOS=0) -> stock stereo");
}

/* Audio is an AirPlay-side module: it needs the SETUP request before stock sees
 * it, the response after stock and again after every other module has merged
 * into it, the finished /info dictionary, and the session teardown.  It reads
 * no iAP2 message.  AirPlayReceiverServerPlatformCopyProperty and the QSA
 * writers stay interposed in this file — each has exactly one consumer, so they
 * need no framework seam. */
const hook_module_def_t audio_module_def = {
    .name = "audio",
    .priority = HOOK_PRIORITY_HIGH,
    .on_init = audio_init,
    .on_shutdown = audio_shutdown,
    .airplay = {
        .on_setup_request = spa_on_setup_request,
        .on_setup_response = spa_on_setup_response,
        .on_setup_response_final = spa_on_setup_response_final,
        .on_server_info = spa_augment_server_info,
        .on_server_info_final = spa_capture_server_info_seam,
        .on_teardown = spa_on_teardown
    }
};
