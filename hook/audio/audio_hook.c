/*
 * audio_hook.c — CarPlay discrete 5.1 PCM: widen the AirPlay audio-format negotiation to 6 channels.
 * See audio_hook.h and docs/reference/AUDIO.md (sections 5-10).
 *
 * The whole libairplay format pipeline is 64-bit clean (verified in IDA): SETUP reads `audioFormat`
 * via CFDictionaryGetInt64, stores the full u64 at stream+0x10, and both the SETUP validation
 * (AirPlayReceiverSessionSetup@0x2770c) and the stream build (_UpdateStreams -> ToPCM ->) call
 * AirPlayAudioFormatToASBD(u64) with the format INTACT — no truncation anywhere. The only thing that
 * blocks 5.1 is that ASBD's format->ASBD table is exact-match and rejects any high bit
 * (`if (HIDWORD(a1)) return -6735`). Both callers reach ASBD through the .plt/GOT, so a single
 * LD_PRELOAD interposer on AirPlayAudioFormatToASBD (same mechanism altscreen already uses for
 * AirPlayReceiverSessionSetup) removes the rejection AND supplies the 6ch ASBD everywhere.
 *
 * Two hooks, nothing else:
 *   1. AudioSessionGetSupportedOutputFormats — OR-in the 5.1 bit so the phone offers/selects it.
 *   2. AirPlayAudioFormatToASBD               — emit a 48000/16/6ch ASBD for our format.
 * OFF by default: only when env SPATIAL_5_1 is truthy (value-checked). The mixer/dio side must
 * provision mpl5_dio_ent for 6 voices -> six_channel_speakerp (B&O firmware agent's dependency).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "audio_hook.h"
#include "apac_codec_loader.h"
#include "apap_crypto.h"
#include "spatial_downmix.h"
#include "../framework/logging.h"

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
#include <time.h>
#include <sys/asoundlib.h>

DEFINE_LOG_MODULE(audio);

/* ---- the format we emit ---------------------------------------------------------------------
 * Rate/depth are fixed by the hardware: every media device in deva is 48000-only, 16-bit on the wire
 * (measured on-unit, AUDIO.md §21a/§24). Only the channel count is variable, and 6 is the ceiling —
 * deva's `voices > 2` branch is hardcoded to 6 channels (§16).
 * Channel order = Apple canonical 5.1 (L R C LFE Ls Rs). CONFIRMED correct: deva's multichannel stream
 * map is 01 02 03 06 08 09 = FL FR FC LFE LB RB = exactly that (§16) — no reorder needed. */
#define SPA_SR        48000
#define SPA_BITS      16
#define SPA_CHANNELS  6

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
typedef int   (*cf_dictsetc_t)(void *, const void *, const char *, int);
typedef void *(*cf_dictget_t)(const void *, const void *);
typedef int64_t (*cf_dictgeti_t)(const void *, const void *, int32_t *);
typedef int   (*cf_strgetc_t)(const void *, char *, size_t);
typedef int   (*cf_plwrite_t)(const void *, const char *, FILE *);
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
static cf_dictsetc_t cfa_dictsetc;
static cf_dictget_t  cfa_dictget;
static cf_dictgeti_t cfa_dictgeti;
static cf_strgetc_t  cfa_strgetc;
static cf_plwrite_t  cfa_plwrite;
static cf_rel_t      cfa_rel;
static void         *cfa_arr_cb, *cfa_dk_cb, *cfa_dv_cb;
static void         *cfa_bool_true;
static int           cfa_ready = -1;
static int spa103_private_image_compatible(void);

/* AirPlaySender validates enabledFeatures against /info after control SETUP. In particular,
 * negotiated mainBuffered requires a top-level mainBufferedInfo key and rejects the entire CarPlay
 * session when the key is absent (-16720 in iOS 27's validator). Remember the exact session whose
 * response we changed so an unrelated/stale /info can never inherit the contract. */
static pthread_mutex_t spa_contract_lock = PTHREAD_MUTEX_INITIALIZER;
static void            *spa_mainbuffered_session;
static uint32_t         spa_contract_sequence;

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
    cfa_dictsetc = (cf_dictsetc_t) dlsym(RTLD_DEFAULT, "CFDictionarySetCString");
    cfa_dictget  = (cf_dictget_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
    cfa_dictgeti = (cf_dictgeti_t) dlsym(RTLD_DEFAULT, "CFDictionaryGetInt64");
    cfa_strgetc  = (cf_strgetc_t)  dlsym(RTLD_DEFAULT, "CFStringGetCString");
    cfa_plwrite  = (cf_plwrite_t)  dlsym(RTLD_DEFAULT, "CFPropertyListWriteToANSIFile");
    cfa_rel      = (cf_rel_t)      dlsym(RTLD_DEFAULT, "CFRelease");
    cfa_arr_cb   = dlsym(RTLD_DEFAULT, "kCFLArrayCallBacksCFLTypes");
    cfa_dk_cb    = dlsym(RTLD_DEFAULT, "kCFLDictionaryKeyCallBacksCFLTypes");
    cfa_dv_cb    = dlsym(RTLD_DEFAULT, "kCFLDictionaryValueCallBacksCFLTypes");

    cfa_ready = (cfa_str && cfa_equal && cfa_dictm && cfa_arrm && cfa_arrapp && cfa_arrcount &&
                 cfa_arridx && cfa_dictset && cfa_dictseti && cfa_dictsetc && cfa_dictget &&
                 cfa_dictgeti &&
                 cfa_rel && cfa_arr_cb && cfa_dk_cb && cfa_dv_cb) ? 1 : 0;
    if (!cfa_ready)
        LOG_ERROR(LOG_MODULE, "probe103: CFLite symbol resolution FAILED — probe disabled");
    return cfa_ready;
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
    if (spa_audio_config()->usesNewControls) return -1;
    const char *value = getenv(SPA_103_EXTRA_INDEX_ENV);
    char *end = 0;
    long candidate;

    if (!value || !value[0]) return -1;
    candidate = strtol(value, &end, 0);
    if (end && end != value && !*end &&
        (candidate == SPA_APAC_48K_5_1 || candidate == SPA_APAC_48K_5_1_2)) {
        if (candidate == primaryIndex) return -1;
        return (int)candidate;
    }

    LOG_ERROR(LOG_MODULE,
              "probe103: invalid %s value '%s'; allowed extra gate indices: %d/%d; ignored",
              SPA_103_EXTRA_INDEX_ENV, value, SPA_APAC_48K_5_1, SPA_APAC_48K_5_1_2);
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
}

static int spa_write_setup103_plist(const void *object, const char *kind, uint32_t sequence)
{
    char path[128];
    snprintf(path, sizeof(path), "/tmp/spatial_setup103_%u_%s.bplist",
             (unsigned)sequence, kind);
    return spa_write_plist_path(object, path);
}

static int spa_write_control_plist(const void *object, const char *kind, uint32_t sequence)
{
    char path[128];
    snprintf(path, sizeof(path), "/tmp/spatial_control_%u_%s.bplist",
             (unsigned)sequence, kind);
    return spa_write_plist_path(object, path);
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
#define SPA_103_AUDIO_BUFFER_SIZE_AAC   5000000 /* receiver default for compression types 4/8 */
#define SPA_103_AUDIO_BUFFER_SIZE_OTHER 21168000 /* receiver default for type 0, including APAC */
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
    int64_t rate;
    spa103_transport_time_t anchor;
    spa103_transport_time_t firstAudible;
    spa103_network_time_t networkTime;
    uint32_t controlRequestCount;
    /* The type-103 SETUP `shk`, copied verbatim: it IS the ChaCha20-Poly1305 key,
     * with no HKDF anywhere on this path.  Cleared on every teardown. */
    uint8_t shk[SPA_APAP_KEY_SIZE];
    int haveKey;
    int64_t formatIndex;
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
 * Stage j: the APAP receiver stops being a capture drain and becomes a decoder.  Per frame:
 *
 *   BE32-inclusive framing -> ChaCha20-Poly1305 open -> TLV split -> APAC decode (float32)
 *   -> S16 -> spa_reorder_5_1_decoder_to_declared_s16 -> bounded PCM6 ring
 *
 * The ring is where the existing PCM6/DIO5 rail collects it: while type 103 is carrying media
 * the type-100 read path hands out these samples instead of its own (the interposition itself
 * is the remaining on-car step, see PORT_STATUS.md stage j).  spa_buffered_pcm6_read() is that
 * seam, and nothing in this file drains the ring, so with no consumer attached the behaviour is
 * exactly what it was before: paced at one packet per 1024/48k, oldest dropped.
 *
 * Failure policy.  A frame that will not authenticate or will not decode is counted and
 * dropped; it does not stop the receiver, because the capture/drain has to keep the phone's
 * session alive whatever the codec does.  A format this rail cannot carry - anything but index
 * 65 - disables decoding for the whole stream with its own distinct status and is logged once.
 * Index 66 lands there deliberately: its eight-channel native order has never been measured, and
 * guessing a permutation would produce plausible, wrong audio.
 */
#define SPA_103_RENDER_MAX_CHANNELS SPA_PCM_5_1_2_CHANNELS
#define SPA_103_RENDER_FRAMES     1024u
#define SPA_103_RENDER_MAX_SAMPLES (SPA_103_RENDER_MAX_CHANNELS * SPA_103_RENDER_FRAMES)
/* Half a second of 48 kHz 5.1: 288 KB.  Big enough to absorb the phone's burst between two
 * reads of the sink, small enough that a stalled sink cannot hide a minute of latency. */
#define SPA_103_RING_FRAMES       24000u
/* Above this the producer paces itself.  With no consumer the ring sits full and every frame
 * pauses, which is the backpressure the capture-only build faked with an unconditional sleep. */
#define SPA_103_RING_HIGH_FRAMES  (SPA_103_RING_FRAMES / 2u)
/* Serving the sink from an almost-empty ring is what the car heard.  The stock
 * type-100 write cadence, not the phone's buffered delivery, decides when a
 * fragment must be handed over; starting at fill=0 meant the first second of
 * every session was padded with silence -- roughly 50 frames missing out of each
 * 768, a ~1 ms hole every 16 ms, which is a 62 Hz buzz rather than a dropout.
 * Wait for a cushion before the first serve.  400 ms is 25 fragments of head
 * start and one sixtieth of the 24000-frame ring. */
#define SPA_103_PRIME_FRAMES      ((SPA_SR * 400u) / 1000u)

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
    int16_t *ring;                  /* SPA_103_RING_FRAMES * 6 samples */
    size_t head;                    /* frame index of the oldest buffered frame */
    size_t fill;                    /* frames buffered */
    uint64_t framesIn;
    uint64_t framesOut;
    uint64_t dropped;               /* frames overwritten because nothing drained */
    uint64_t authFailed;
    uint64_t tlvFailed;
    uint64_t decodeFailed;
    uint64_t flushDiscarded;
    int playing;
    int primed;                      /* ring reached the start cushion at least once */
    int flushPending;
    int haveFlushUntilSequence;
    uint32_t flushUntilSequence;
    int haveFlushUntilRTP;
    uint32_t flushUntilRTP;
    uint64_t statsFrames;
    uint64_t statsAbs[SPA_103_RENDER_MAX_CHANNELS];
    uint64_t statsZeroCrossings[SPA_103_RENDER_MAX_CHANNELS];
    uint32_t statsPeak[SPA_103_RENDER_MAX_CHANNELS];
    int16_t statsPrevious[SPA_103_RENDER_MAX_CHANNELS];
    int statsHavePrevious;
    uint32_t sampleRate;
    uint32_t sourceChannels;
    uint32_t outputChannels;
    uint32_t channelLayoutTag;
    int64_t formatIndex;
    int atmosMode;
} spa103_render_t;

static spa103_render_t spa103_render = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .state = SPA_103_RENDER_IDLE
};

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

/* Called with the receiver lock NOT held. */
static void spa103_render_start(int64_t formatIndex, int haveKey)
{
    const spa_audio_config_t *config = spa_audio_config();
    const char *codecPath = 0;
    uint32_t outputChannels =
        formatIndex == SPA_APAC_48K_5_1_2 &&
        config->atmosMode == SPA_ATMOS_PASSTHROUGH ?
        SPA_PCM_5_1_2_CHANNELS : SPA_PCM_5_1_CHANNELS;
    int state;

    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.decoder && spa103_render.codec) {
        spa103_render.codec->destroy(spa103_render.decoder);
        spa103_render.decoder = 0;
    }
    spa103_render.codec = spa_apac_runtime_load(&codecPath);
    free(spa103_render.ring);
    spa103_render.ring = (int16_t *)malloc((size_t)SPA_103_RING_FRAMES *
                                           outputChannels * sizeof(int16_t));
    spa103_render.head = 0;
    spa103_render.fill = 0;
    spa103_render.primed = 0;
    spa103_render.framesIn = 0;
    spa103_render.framesOut = 0;
    spa103_render.dropped = 0;
    spa103_render.authFailed = 0;
    spa103_render.tlvFailed = 0;
    spa103_render.decodeFailed = 0;
    spa103_render.flushDiscarded = 0;
    spa103_render.playing = 1;
    spa103_render.flushPending = 0;
    spa103_render.haveFlushUntilSequence = 0;
    spa103_render.haveFlushUntilRTP = 0;
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
    spa103_render.atmosMode = config->atmosMode;

    if (!spa103_render.ring)
        state = SPA_103_RENDER_NO_MEMORY;
    else if (formatIndex != SPA_APAC_48K_5_1 &&
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
             "buffer103: render %s formatIndex=%lld key=%s output=%uch atmosMode=%s",
             spa103_render_state_name(state), (long long)formatIndex,
             haveKey ? "yes" : "no", (unsigned)outputChannels,
             config->atmosMode == SPA_ATMOS_DOWNMIX ? "downmix" :
             config->atmosMode == SPA_ATMOS_DROP ? "drop" : "passthrough");
    if (state == SPA_103_RENDER_NO_DECODER)
        LOG_ERROR(LOG_MODULE,
                  "buffer103: optional APAC codec unavailable/rejected at %s; capture continues",
                  codecPath ? codecPath : "<unknown>");
}

static void spa103_render_stop(void)
{
    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.decoder && spa103_render.codec) {
        spa103_render.codec->destroy(spa103_render.decoder);
        spa103_render.decoder = 0;
    }
    free(spa103_render.ring);
    spa103_render.ring = 0;
    spa103_render.head = 0;
    spa103_render.fill = 0;
    spa103_render.primed = 0;
    spa103_render.playing = 0;
    spa103_render.flushPending = 0;
    spa103_render.state = SPA_103_RENDER_IDLE;
    pthread_mutex_unlock(&spa103_render.lock);
}

/* Signed modular comparison on the wire's wrapping counters. */
static int spa103_counter24_after(uint32_t value, uint32_t boundary)
{
    uint32_t delta = (value - boundary) & 0x00ffffffu;
    return delta != 0 && delta < 0x00800000u;
}

static int spa103_counter32_after(uint32_t value, uint32_t boundary)
{
    uint32_t delta = value - boundary;
    return delta != 0 && delta < 0x80000000u;
}

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
    pthread_mutex_lock(&spa103_render.lock);
    spa103_render.playing = rate == 1;
    pthread_mutex_unlock(&spa103_render.lock);
}

/* FLUSHBUFFERED has two queues to clear.  The decoded PCM ring is local and can be emptied now;
 * APAP frames already accepted by TCP must be discarded through the sender's inclusive until
 * boundary.  Without the latter, pressing Pause audibly drains stale media for seconds. */
static void spa103_render_flush(int haveUntilSequence, uint32_t untilSequence,
                                int haveUntilRTP, uint32_t untilRTP)
{
    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.decoder && spa103_render.codec)
        spa103_render.codec->reset(spa103_render.decoder);
    spa103_render.head = 0;
    spa103_render.fill = 0;
    spa103_render.primed = 0;
    spa103_render.flushPending = haveUntilSequence || haveUntilRTP;
    spa103_render.haveFlushUntilSequence = haveUntilSequence;
    spa103_render.flushUntilSequence = untilSequence & 0x00ffffffu;
    spa103_render.haveFlushUntilRTP = haveUntilRTP;
    spa103_render.flushUntilRTP = untilRTP;
    spa103_render.flushDiscarded = 0;
    pthread_mutex_unlock(&spa103_render.lock);
}

/* Called only after the packet authenticated, so an unauthenticated header cannot move the flush
 * window.  Returns 1 while the packet is at/before the inclusive boundary. */
static int spa103_render_drop_for_flush(const uint8_t *packet)
{
    uint32_t sequence = spa103_apap_sequence(packet);
    uint32_t rtp = (uint32_t)spa103_media_time_to_rtp(
        spa103_apap_media_value(packet), spa103_apap_media_scale(packet));
    uint64_t discarded = 0;
    int drop = 0, crossed = 0;

    pthread_mutex_lock(&spa103_render.lock);
    if (spa103_render.flushPending) {
        int after = spa103_render.haveFlushUntilSequence ?
            spa103_counter24_after(sequence, spa103_render.flushUntilSequence) :
            spa103_counter32_after(rtp, spa103_render.flushUntilRTP);
        if (!after) {
            spa103_render.flushDiscarded++;
            drop = 1;
        } else {
            discarded = spa103_render.flushDiscarded;
            spa103_render.flushPending = 0;
            spa103_render.haveFlushUntilSequence = 0;
            spa103_render.haveFlushUntilRTP = 0;
            crossed = 1;
        }
    }
    pthread_mutex_unlock(&spa103_render.lock);

    if (crossed)
        LOG_INFO(LOG_MODULE,
                 "buffer103: flush boundary crossed at seq=%u rtp=%u discarded=%llu",
                 (unsigned)sequence, (unsigned)rtp, (unsigned long long)discarded);
    return drop;
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
static void spa103_ring_push_locked(const int16_t *pcm, size_t frames)
{
    size_t at = 0;

    if (!spa103_render.ring || !frames) return;
    if (frames > SPA_103_RING_FRAMES) {
        pcm += (frames - SPA_103_RING_FRAMES) * spa103_render.outputChannels;
        spa103_render.dropped += frames - SPA_103_RING_FRAMES;
        frames = SPA_103_RING_FRAMES;
    }
    while (at < frames) {
        const size_t tail = (spa103_render.head + spa103_render.fill) % SPA_103_RING_FRAMES;
        size_t run = SPA_103_RING_FRAMES - tail;
        if (run > frames - at) run = frames - at;
        memcpy(spa103_render.ring + tail * spa103_render.outputChannels,
               pcm + at * spa103_render.outputChannels,
               run * spa103_render.outputChannels * sizeof(int16_t));
        if (spa103_render.fill + run > SPA_103_RING_FRAMES) {
            const size_t lost = spa103_render.fill + run - SPA_103_RING_FRAMES;
            spa103_render.head = (spa103_render.head + lost) % SPA_103_RING_FRAMES;
            spa103_render.fill = SPA_103_RING_FRAMES;
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
static void spa103_render_frame(const uint8_t *packet, size_t length)
{
    static uint8_t plaintext[SPA_103_APAP_MAX_FRAME];
    static int16_t decoded[SPA_103_RENDER_MAX_SAMPLES];
    uint8_t key[SPA_APAP_KEY_SIZE];
    spa_apap_extension_t ext;
    const uint8_t *audio = 0;
    size_t audioLength = 0, plainLength = 0;
    uint32_t frames = 0;
    int variant = 0, rc, state;

    pthread_mutex_lock(&spa103_render.lock);
    state = spa103_render.state;
    pthread_mutex_unlock(&spa103_render.lock);
    if (state != SPA_103_RENDER_WAITING && state != SPA_103_RENDER_RUNNING) return;

    pthread_mutex_lock(&spa103_lock);
    memcpy(key, spa103_receiver.shk, sizeof(key));
    pthread_mutex_unlock(&spa103_lock);

    if (!spa_apap_open(key, packet, length, plaintext, sizeof(plaintext),
                       &plainLength, &variant)) {
        pthread_mutex_lock(&spa103_render.lock);
        spa103_render.authFailed++;
        pthread_mutex_unlock(&spa103_render.lock);
        memset(key, 0, sizeof(key));
        return;
    }
    memset(key, 0, sizeof(key));

    if (spa103_render_drop_for_flush(packet)) return;

    memset(&ext, 0, sizeof(ext));
    if (variant == SPA_APAP_AAD_EXTENSION) {
        if (!spa_apap_parse_extension(plaintext, plainLength, &ext, &audio, &audioLength)) {
            pthread_mutex_lock(&spa103_render.lock);
            spa103_render.tlvFailed++;
            pthread_mutex_unlock(&spa103_render.lock);
            return;
        }
    } else {
        audio = plaintext;
        audioLength = plainLength;
    }
    if (!audioLength) return;   /* a state-only packet: authentic, and carries no audio */

    pthread_mutex_lock(&spa103_render.lock);
    state = spa103_render.playing;
    pthread_mutex_unlock(&spa103_render.lock);
    if (!state) return;         /* rate=0: authenticate/drain, but never render stale media */

    /* Bring the decoder up on the first frame that names a cookie we hold.  Doing it here rather
     * than at SETUP is not laziness: SETMAGICCOOKIE arrives asynchronously on the RTSP control
     * connection and may land after the first APAP frame. */
    pthread_mutex_lock(&spa103_render.lock);
    if (!spa103_render.decoder ||
        (ext.hasMagicCookieId && spa103_render.haveCookieId &&
         spa103_render.cookieId != (int64_t)ext.magicCookieId)) {
        uint8_t *cookie;
        size_t cookieSize = 0;
        spa_apac_stream_info_t info;
        uint32_t channels = 0, frameLength = 0, sampleRate = 0;

        pthread_mutex_unlock(&spa103_render.lock);
        cookie = spa103_copy_cookie(ext.hasMagicCookieId, (int64_t)ext.magicCookieId,
                                    &cookieSize);
        if (!cookie) return;    /* no cookie yet; the next frame tries again */

        pthread_mutex_lock(&spa103_render.lock);
        if (spa103_render.decoder && spa103_render.codec) {
            spa103_render.codec->destroy(spa103_render.decoder);
            spa103_render.decoder = 0;
        }
        rc = spa103_render.codec->create(cookie, cookieSize, &spa103_render.decoder,
                                         &channels, &frameLength, &sampleRate);
        free(cookie);
        memset(&info, 0, sizeof(info));
        if (rc == SPA_APAC_OK)
            rc = spa103_render.codec->getInfo(spa103_render.decoder, &info);
        if (rc != SPA_APAC_OK ||
            frameLength != SPA_103_RENDER_FRAMES || sampleRate != SPA_SR ||
            (spa103_render.formatIndex == SPA_APAC_48K_5_1 &&
             (channels != SPA_PCM_5_1_CHANNELS ||
              info.channelLayoutTag != 0x007c0006u)) ||
            (spa103_render.formatIndex == SPA_APAC_48K_5_1_2 &&
             (channels != SPA_PCM_5_1_2_CHANNELS ||
              info.channelLayoutTag != 0x00c20008u))) {
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
                      "buffer103: APAC decoder setup rejected status=%d cookie=%u bytes "
                      "index=%lld geometry=%uHz/%uch/%u layout=%#x; capture continues",
                      rc, (unsigned)cookieSize, (long long)spa103_render.formatIndex,
                      (unsigned)sampleRate, (unsigned)channels, (unsigned)frameLength,
                      (unsigned)info.channelLayoutTag);
            return;
        }
        spa103_render.cookieId = (int64_t)ext.magicCookieId;
        spa103_render.haveCookieId = ext.hasMagicCookieId;
        spa103_render.sampleRate = sampleRate;
        spa103_render.sourceChannels = channels;
        spa103_render.channelLayoutTag = info.channelLayoutTag;
        spa103_render.state = SPA_103_RENDER_RUNNING;
        LOG_INFO(LOG_MODULE,
                 "buffer103: APAC decoder up id=%lld %u Hz %u ch %u frames/packet",
                 (long long)spa103_render.cookieId, (unsigned)sampleRate,
                 (unsigned)channels, (unsigned)frameLength);
    }

    rc = spa103_render.codec->decodeS16(spa103_render.decoder, audio, audioLength,
                                        decoded, SPA_103_RENDER_MAX_SAMPLES, &frames);
    if (rc != SPA_APAC_OK) {
        spa103_render.decodeFailed++;
        pthread_mutex_unlock(&spa103_render.lock);
        return;
    }
    if (frames) {
        if (spa103_render.formatIndex == SPA_APAC_48K_5_1) {
            /* Measured APAC cookie order C L R Ls Rs LFE -> DIO declared order. */
            spa_reorder_5_1_decoder_to_declared_s16(decoded, frames, decoded);
        } else if (spa103_render.atmosMode == SPA_ATMOS_DOWNMIX) {
            /* Atmos tag 0x00c20008 is already L R C LFE Ls Rs Ltm Rtm. */
            spa_downmix_5_1_2_s16(decoded, frames, decoded);
        } else if (spa103_render.atmosMode == SPA_ATMOS_DROP) {
            spa_drop_5_1_2_s16(decoded, frames, decoded);
        }
        spa103_pcm_stats_locked(decoded, frames);
        spa103_ring_push_locked(decoded, frames);
        spa103_render.framesIn += frames;
    }
    pthread_mutex_unlock(&spa103_render.lock);
}

/* 1 while the producer should pace itself: nothing is draining the ring, or it is already half
 * full.  When decoding is off this is always true, which reproduces the capture-only build's
 * unconditional one-packet-per-frame-period sleep exactly. */
static int spa103_render_backpressure(void)
{
    int paced;

    pthread_mutex_lock(&spa103_render.lock);
    paced = spa103_render.state != SPA_103_RENDER_RUNNING || !spa103_render.playing ||
            spa103_render.fill >= SPA_103_RING_HIGH_FRAMES;
    pthread_mutex_unlock(&spa103_render.lock);
    return paced;
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

    if (!out || !frames) return 0;
    pthread_mutex_lock(&spa103_render.lock);
    /* Returning 0 here is deliberate: the sink then expands the phone's own
     * realtime stereo for this fragment, which is real audio.  Padding silence
     * instead is what made the gap audible. */
    if (!spa103_render.primed && spa103_render.fill >= SPA_103_PRIME_FRAMES)
        spa103_render.primed = 1;
    if (spa103_render.state == SPA_103_RENDER_RUNNING && spa103_render.playing &&
        spa103_render.primed && spa103_render.ring &&
        channels == spa103_render.outputChannels) {
        while (done < frames && spa103_render.fill) {
            size_t run = SPA_103_RING_FRAMES - spa103_render.head;
            if (run > spa103_render.fill) run = spa103_render.fill;
            if (run > frames - done) run = frames - done;
            memcpy(out + done * channels,
                   spa103_render.ring + spa103_render.head * channels,
                   run * channels * sizeof(int16_t));
            spa103_render.head = (spa103_render.head + run) % SPA_103_RING_FRAMES;
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
#define SPA_PCM_SINK_PERIOD_MS 16u
#define SPA_PCM_SINK_PERIOD_FRAMES ((SPA_SR * SPA_PCM_SINK_PERIOD_MS) / 1000u)
/* Eight fragments is the floor the RetroArch QNX QSA driver established on this
 * head unit; below it the CSD session cycles instead of absorbing jitter. */
#define SPA_PCM_SINK_MIN_FRAGS     8


typedef int (*spa_snd_open_name_t)(snd_pcm_t **, char *, int);
typedef int (*spa_snd_params_t)(snd_pcm_t *, snd_pcm_channel_params_t *);
typedef int (*spa_snd_setup_t)(snd_pcm_t *, snd_pcm_channel_setup_t *);
typedef ssize_t (*spa_snd_write_t)(snd_pcm_t *, const void *, size_t);
typedef int (*spa_snd_close_t)(snd_pcm_t *);

typedef struct {
    pthread_mutex_t lock;
    snd_pcm_t *handle;
    int sourceFormat;
    unsigned sourceChannels;
    unsigned outputChannels;
    int expandedParams;
    int16_t *scratch;
    size_t scratchBytes;
    uint64_t writes;
    uint64_t bufferedWrites;
    uint64_t underrunFrames;
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
} spa_pcm_sink_t;

static spa_pcm_sink_t spa_pcm_sink = {
    .lock = PTHREAD_MUTEX_INITIALIZER
};

static unsigned spa_audio_requested_output_channels(void)
{
    const spa_audio_config_t *config = spa_audio_config();

    if (!config->realtimePCM) return 0;
    return config->atmos && config->atmosMode == SPA_ATMOS_PASSTHROUGH ?
           SPA_PCM_5_1_2_CHANNELS : SPA_PCM_5_1_CHANNELS;
}

int snd_pcm_open_name(snd_pcm_t **handle, char *name, int mode)
{
    static spa_snd_open_name_t real;
    int result;

    if (!real) real = (spa_snd_open_name_t)dlsym(RTLD_NEXT, "snd_pcm_open_name");
    if (!real) return -1;
    result = real(handle, name, mode);
    if (result == 0 && handle && *handle && name &&
        !strcmp(name, SPA_PCM_SINK_NAME) && (mode & SND_PCM_OPEN_PLAYBACK)) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        spa_pcm_sink.handle = *handle;
        spa_pcm_sink.sourceChannels = 0;
        spa_pcm_sink.outputChannels = 0;
        spa_pcm_sink.expandedParams = 0;
        spa_pcm_sink.writes = 0;
        spa_pcm_sink.bufferedWrites = 0;
        spa_pcm_sink.underrunFrames = 0;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        LOG_INFO(LOG_MODULE, "audio sink: captured %s playback handle=%p",
                 SPA_PCM_SINK_NAME, *handle);
    }
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
    unsigned outputChannels;
    size_t sourceFrameBytes, outputFrameBytes;
    int isSink, result, renegotiated;

    if (!real) real = (spa_snd_params_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_params");
    if (!real) return -1;

    outputChannels = spa_audio_requested_output_channels();
    pthread_mutex_lock(&spa_pcm_sink.lock);
    isSink = handle && handle == spa_pcm_sink.handle;
    if (isSink) spa_pcm_sink.expandedParams = 0;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
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
    return result;
}

int snd_pcm_plugin_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup)
{
    static spa_snd_setup_t real;
    snd_pcm_channel_setup_t actual;
    unsigned sourceChannels, outputChannels;
    int translate, result;

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
    if (result != 0) return result;

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
    LOG_INFO(LOG_MODULE,
             "audio sink: setup actual=%uch/%dHz/%d-byte fragment -> "
             "stock=%uch/%d-byte fragment",
             outputChannels, (int)actual.format.rate, actual.buf.block.frag_size,
             sourceChannels, setup->buf.block.frag_size);
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
    static long cached = -1;
    if (cached < 0) {
        const char *v = getenv(SPA_TAP_ENV);
        cached = (v && v[0]) ? strtol(v, 0, 0) : 0;
        if (cached < 0) cached = 0;
    }
    return (size_t)cached;
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
    ssize_t written;
    int useBridge;

    if (!real) real = (spa_snd_write_t)dlsym(RTLD_NEXT, "snd_pcm_plugin_write");
    if (!real) return -1;

    pthread_mutex_lock(&spa_pcm_sink.lock);
    useBridge = handle && handle == spa_pcm_sink.handle && spa_pcm_sink.expandedParams;
    sourceChannels = spa_pcm_sink.sourceChannels;
    outputChannels = spa_pcm_sink.outputChannels;
    if (!useBridge || !buffer || !sourceChannels || !outputChannels) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        return real(handle, buffer, size);
    }

    sourceFrameBytes = sourceChannels * sizeof(int16_t);
    outputFrameBytes = outputChannels * sizeof(int16_t);
    if (size == 0 || size % sourceFrameBytes != 0) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        return real(handle, buffer, size);
    }
    frames = size / sourceFrameBytes;
    if (frames > SPA_PCM_SINK_MAX_WRITE / outputFrameBytes) {
        pthread_mutex_unlock(&spa_pcm_sink.lock);
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
            return real(handle, buffer, size);
        }
        spa_pcm_sink.scratch = replacement;
        spa_pcm_sink.scratchBytes = outputBytes;
    }

    if (spa_buffered_pcm_active() &&
        spa_buffered_pcm_channels() == outputChannels) {
        bufferedFrames = spa_buffered_pcm_read(spa_pcm_sink.scratch, frames,
                                               outputChannels);
        if (bufferedFrames == 0) {
            /* Nothing decoded is ready -- the ring is still filling its start
             * cushion, or it ran dry.  Play the phone's own realtime stereo
             * widened instead: that is real audio.  Filling the whole fragment
             * with zeroes here is what made a not-yet-primed session silent. */
            spa_pcm_expand_fallback((const int16_t *)buffer, frames, sourceChannels,
                                    spa_pcm_sink.scratch, outputChannels);
        } else {
            if (bufferedFrames < frames) {
                memset(spa_pcm_sink.scratch + bufferedFrames * outputChannels, 0,
                       (frames - bufferedFrames) * outputFrameBytes);
                spa_pcm_sink.underrunFrames += frames - bufferedFrames;
            }
            spa_pcm_sink.bufferedWrites++;
        }
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
    /* QSA widens S16 to the device's native S32 itself, and the QNX harness
     * measured that widening to be exactly a left shift by 16 with channel order
     * intact -- so there is nothing for this path to improve on. */
    written = real(handle, spa_pcm_sink.scratch, outputBytes);
    spa_pcm_sink.lastWriteResult = (int64_t)written;
    if (written == 0) spa_pcm_sink.zeroWrites++;
    else if (written > 0 && (size_t)written < outputBytes) spa_pcm_sink.shortWrites++;
    if (spa_pcm_sink.zeroWrites == 1 || spa_pcm_sink.shortWrites == 1)
        LOG_WARN(LOG_MODULE,
                 "audio sink: device did not take the whole fragment (%d of %u bytes) "
                 "at write %llu -- the stock caller will rewrite this buffer",
                 (int)written, (unsigned)outputBytes,
                 (unsigned long long)spa_pcm_sink.writes);
    if (spa_pcm_sink.writes == 1 || (spa_pcm_sink.writes % 1024u) == 0) {
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
            }
        }
        LOG_WARN(LOG_MODULE,
                 "audio sink: writes=%llu buffered=%llu underrunFrames=%llu "
                 "zeroWrites=%llu shortWrites=%llu last=%lld "
                 "source=%uch target=%uch ringRead=%u/%u "
                 "odd=%llu lastOdd=%u devStatus=%d devUnderruns=%llu",
                 (unsigned long long)spa_pcm_sink.writes,
                 (unsigned long long)spa_pcm_sink.bufferedWrites,
                 (unsigned long long)spa_pcm_sink.underrunFrames,
                 (unsigned long long)spa_pcm_sink.zeroWrites,
                 (unsigned long long)spa_pcm_sink.shortWrites,
                 (long long)spa_pcm_sink.lastWriteResult,
                 sourceChannels, outputChannels,
                 (unsigned)bufferedFrames, (unsigned)frames,
                 (unsigned long long)spa_pcm_sink.oddSizeWrites,
                 (unsigned)spa_pcm_sink.lastOddSize,
                 spa_pcm_sink.deviceStatus,
                 (unsigned long long)spa_pcm_sink.deviceUnderruns);
    }
    pthread_mutex_unlock(&spa_pcm_sink.lock);

    if (written < 0) return written;
    if ((size_t)written >= outputBytes) return (ssize_t)size;
    return (ssize_t)(((size_t)written / outputFrameBytes) * sourceFrameBytes);
}

int snd_pcm_close(snd_pcm_t *handle)
{
    static spa_snd_close_t real;
    int isSink, result;

    if (!real) real = (spa_snd_close_t)dlsym(RTLD_NEXT, "snd_pcm_close");
    if (!real) return -1;
    pthread_mutex_lock(&spa_pcm_sink.lock);
    isSink = handle && handle == spa_pcm_sink.handle;
    pthread_mutex_unlock(&spa_pcm_sink.lock);
    result = real(handle);
    if (isSink) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        if (handle == spa_pcm_sink.handle) {
            spa_pcm_sink.handle = 0;
            spa_pcm_sink.expandedParams = 0;
            spa_pcm_sink.sourceChannels = 0;
            spa_pcm_sink.outputChannels = 0;
        }
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        LOG_INFO(LOG_MODULE, "audio sink: released %s handle=%p result=%d",
                 SPA_PCM_SINK_NAME, handle, result);
    }
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
    char path[160];
    size_t i;
    uint32_t sequence = 0;
    int stored = 0;
    int captureFd = -1;

    pthread_mutex_lock(&spa103_lock);
    if (!spa103_receiver.session || spa103_receiver.stop || spa103_receiver.settingUp) {
        pthread_mutex_unlock(&spa103_lock);
        free(bytes);
        LOG_WARN(LOG_MODULE,
                 "buffer103: SETMAGICCOOKIE id=%lld accepted without active stream; discarded",
                 (long long)id);
        return 0;
    }
    sequence = spa103_receiver.sequence;
    for (i = 0; i < spa103_receiver.magicCookieCount; i++) {
        if (spa103_receiver.magicCookies[i].id == id) {
            pthread_mutex_unlock(&spa103_lock);
            free(bytes);
            LOG_INFO(LOG_MODULE,
                     "buffer103: duplicate SETMAGICCOOKIE id=%lld ignored sequence=%u",
                     (long long)id, (unsigned)sequence);
            return 0;
        }
    }
    if (spa103_receiver.magicCookieCount < SPA_103_MAX_MAGIC_COOKIES) {
        spa103_magic_cookie_t *slot =
            &spa103_receiver.magicCookies[spa103_receiver.magicCookieCount++];
        slot->id = id;
        slot->bytes = bytes;
        slot->size = size;
        stored = 1;
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
    }
    pthread_mutex_unlock(&spa103_lock);

    if (!stored) {
        free(bytes);
        LOG_ERROR(LOG_MODULE,
                  "buffer103: SETMAGICCOOKIE map full id=%lld limit=%u sequence=%u",
                  (long long)id, (unsigned)SPA_103_MAX_MAGIC_COOKIES, (unsigned)sequence);
        return -1;
    }
    LOG_INFO(LOG_MODULE,
             "buffer103: stored SETMAGICCOOKIE id=%lld bytes=%u sequence=%u",
             (long long)id, (unsigned)size, (unsigned)sequence);
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

    params = spa103_parse_control_plist(request, methodName, &sequence, &requestNumber);
    if (!params) return spa103_send_http_status(connection, 400);

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
                if (!spa103_read_transport_time(params, "rtpTime", "mediaTimeValue",
                                                "mediaTimeScale", &anchor) || !anchor.valid ||
                    !spa103_read_transport_time(params, "firstAudibleRTPTime",
                                                "firstAudibleMediaTimeValue",
                                                "firstAudibleMediaTimeScale", &firstAudible) ||
                    !spa103_read_network_time(params, "networkTime", &networkTime)) {
                    status = 400;
                    goto done;
                }
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
                 * absent RTP component; its sender-side serializer omits invalid rtpTime. */
                if (!spa_cf_get_i64(params, "mediaTimeValue", &anchor.mediaTimeValue) ||
                    !spa_cf_get_i64(params, "mediaTimeScale", &anchor.mediaTimeScale) ||
                    anchor.mediaTimeScale == 0) {
                    status = 400;
                    goto done;
                }
                if (!spa_cf_get_i64(params, "rtpTime", &anchor.rtpTime))
                    anchor.rtpTime = spa103_media_time_to_rtp(anchor.mediaTimeValue,
                                                               anchor.mediaTimeScale);
                anchor.valid = 1;
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
            /* Drop the queued PCM and clear the filter banks' overlap memory.  Resuming is only
             * legal on an Audio Synchronization Packet; every packet of this stream is one. */
            spa103_render_flush(haveUntilSeq, (uint32_t)untilSeq,
                                haveUntilTime, (uint32_t)untilTime.rtpTime);
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

    /* AirPlaySupport maps index 40 to 'aac ' -> compression type 4 and index 42 to 'aace' -> type 8.
     * APReceiverAudioSessionBufferedCreate uses 5,000,000 for both types 4 and 8. APAC indices 65/66
     * and the Dolby probe indices stay type 0 and use 21,168,000. */
    if (spa_cf_get_i64(requestStream, "audioFormatIndex", &formatIndex) &&
        (formatIndex == SPA_AAC_LC_48K_5_1_2 ||
         formatIndex == SPA_AAC_ELD_48K_5_1_2))
        return SPA_103_AUDIO_BUFFER_SIZE_AAC;
    return SPA_103_AUDIO_BUFFER_SIZE_OTHER;
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

static void *spa103_thread_main(void *opaque)
{
    spa103_thread_arg_t *arg = (spa103_thread_arg_t *)opaque;
    const uint32_t generation = arg->generation;
    const uint32_t sequence = arg->sequence;
    const int listenSock = arg->listenSock;
    const int isControl = arg->isControl;
    const char *transport = isControl ? "RTCP" : "APAP";
    const size_t captureLimit = isControl ? SPA_103_CONTROL_CAPTURE_LIMIT
                                          : SPA_103_CAPTURE_LIMIT;
    uint8_t buffer[16384];
    uint8_t prefix[32];
    char path[128];
    uint64_t total = 0;
    uint64_t apapFrames = 0;
    size_t saved = 0;
    size_t prefixSize = 0;
    /* The frame body is retained now, so a whole APAP packet can be handed to the decoder.
     * spa_apap_framer_* owns the assembly; the loop keeps the one-framing-phase-per-read clamp
     * that stops the drain from defeating TCP flow control. */
    spa_apap_framer_t framer;
    uint8_t *frameBuf = 0;
    int fd = -1, captureFd = -1, accepted = 0, closeListen = 0;
    int prefixLogged = 0;
    free(arg);

    while (!spa103_receiver.stop) {
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
    snprintf(path, sizeof(path), isControl ? "/tmp/spatial_rtcp_%u.bin"
                                           : "/tmp/spatial_apap_%u.bin",
             (unsigned)sequence);
    captureFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (captureFd < 0)
        LOG_ERROR(LOG_MODULE, "buffer103: cannot open %s errno=%d; draining without capture",
                  path, errno);
    if (!isControl) {
        frameBuf = (uint8_t *)malloc(SPA_103_APAP_MAX_FRAME);
        if (!frameBuf)
            LOG_ERROR(LOG_MODULE,
                      "buffer103: cannot allocate the %u-byte frame buffer; draining without "
                      "decode", (unsigned)SPA_103_APAP_MAX_FRAME);
        spa_apap_framer_init(&framer, frameBuf, SPA_103_APAP_MAX_FRAME);
    }
    LOG_INFO(LOG_MODULE,
             "buffer103: iPhone connected; draining %s generation=%u capture=%s limit=%u",
             transport, (unsigned)generation, path, (unsigned)captureLimit);

    while (!spa103_receiver.stop) {
        struct pollfd pfd;
        ssize_t n;
        size_t want = sizeof(buffer);
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 100) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (!(pfd.revents & (POLLIN | POLLHUP | POLLERR))) continue;
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
            if (!prefixLogged && prefixSize < sizeof(prefix)) {
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

                    {
                        struct timespec pace = { 0, SPA_103_APAP_FRAME_NS };
                        apapFrames++;

                        if (frame && frameLength)
                            spa103_render_frame(frame, frameLength);

                        /* Apple's buffered hose queues 32 packages and then suspends network
                         * reads until playback consumes one.  Real consumption is the
                         * backpressure now: spa103_render_backpressure() is false only while a
                         * sink is actually draining the decoded ring, so with decoding off or
                         * unconsumed this is the same unconditional one-package-per-1024/48k
                         * pace the capture-only build used, and the receiver still never
                         * advertises 21 MB and swallows a hundred seconds of audio in four and a
                         * half. */
                        if (apapFrames >= SPA_103_APAP_PREFILL_FRAMES &&
                            spa103_render_backpressure()) {
                            while (nanosleep(&pace, &pace) != 0 && errno == EINTR) {}
                        }
                    }
                }
                if (framingFailed) break;
            }
        } else if (n == 0) {
            break;
        } else if (errno != EINTR && errno != EAGAIN) {
            break;
        }
    }

done:
    free(frameBuf);
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
    LOG_INFO(LOG_MODULE,
             "buffer103: %s receiver exit generation=%u total=%llu saved=%u frames=%llu stop=%d",
             transport, (unsigned)generation, (unsigned long long)total,
             (unsigned)saved, (unsigned long long)apapFrames, spa103_receiver.stop);
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
        memset(spa103_receiver.shk, 0, sizeof(spa103_receiver.shk));
        spa103_receiver.haveKey = 0;
        spa103_receiver.formatIndex = 0;
        spa103_clear_magic_cookies_locked();
        pthread_cond_broadcast(&spa103_cond);
    }
    pthread_mutex_unlock(&spa103_lock);
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
    memcpy(spa103_receiver.shk, shk, sizeof(spa103_receiver.shk));
    spa103_receiver.haveKey = haveKey;
    spa103_receiver.formatIndex = formatIndex;
    pthread_mutex_unlock(&spa103_lock);
    memset(shk, 0, sizeof(shk));
    spa103_render_start(formatIndex, haveKey);

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
    static volatile uint32_t info_sequence;
    char path[128];
    uint32_t sequence;

    if (!info || !spa_probe103_mode() || !spa_cf_resolve()) return;
    sequence = __sync_add_and_fetch(&info_sequence, 1);
    snprintf(path, sizeof(path), "/tmp/spatial_info_%u.bplist", (unsigned)sequence);
    spa_write_plist_path(info, path);
}

/* ---- control-plane trace for buffered media ------------------------------------------------
 *
 * A type-103 hose is not represented in stock 210's receiver-session object.  The data socket can
 * therefore be perfectly healthy while a later RTSP control request (feedback, rate/anchor, etc.)
 * observes only the stock 100/101 streams and causes the sender to pause.  Trace the public control
 * boundary before changing any semantics: old libairplay's request processors call this exported
 * entry through the PLT, so one interposer captures the exact command and its generated response.
 *
 * While the hook-owned type-103 stream is active, stock's {type:100,sr:48000} feedback is stale.
 * Apple's current receiver returns {streams:[]} for this command, including with buffered media,
 * so replace only that array for the owning session.  Every other command and the pre/post-103
 * type-100 fallback response stays stock. SPATIAL_103 gates logs and plist files. */
typedef int (*fn_session_control_t)(void *, uint32_t, const void *, const void *,
                                    const void *, void **);

int AirPlayReceiverSessionControl(void *session, uint32_t flags, const void *command,
                                  const void *qualifier, const void *params, void **responseOut)
{
    static fn_session_control_t real;
    static volatile uint32_t controlSequence;
    uint32_t sequence = 0;
    char commandText[96];
    char path[144];
    int tracing = 0;
    int result;

    if (!real)
        real = (fn_session_control_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionControl");
    if (!real) {
        LOG_ERROR(LOG_MODULE, "probe103 control: AirPlayReceiverSessionControl real==NULL");
        return -6714;
    }

    commandText[0] = '\0';
    if (spa_probe103() && spa_cf_resolve()) {
        sequence = __sync_add_and_fetch(&controlSequence, 1);
        tracing = 1;
        if (!command || !cfa_strgetc ||
            !cfa_strgetc(command, commandText, sizeof(commandText)))
            snprintf(commandText, sizeof(commandText), "<non-string:%p>", command);

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
    }

    result = real(session, flags, command, qualifier, params, responseOut);

    if (result == 0 && responseOut && *responseOut &&
        strcmp(commandText, "updateFeedback") == 0 &&
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
    (void)reason;
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
