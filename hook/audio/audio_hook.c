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
 */
#include "audio_hook.h"
#include "../framework/logging.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <time.h>

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

static int spa_enabled(void)
{
    static int cached = -1;
    if (cached < 0) cached = spa_env_on("SPATIAL_5_1") ? 1 : 0;
    return cached;
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

/* Marker file, not an env var: the env is read once at supervisor start, and this must be revertible
 * with a replug when the phone bites and our (absent) type-103 receiver fails the session. */
static int spa_probe103(void)
{
    FILE *f = fopen(SPA_PROBE103_PATH, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int spa_probe103_dcx(void)
{
    FILE *f;

    if (!spa_probe103()) return 0;
    f = fopen(SPA_103_DCX_PATH, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* Split the two independent pieces of the buffered-audio contract so one replug identifies which
 * one the phone rejects. The marker remains the master kill switch. With the marker present and no
 * mode file, preserve the old combined behavior. Re-read on every negotiation call so no HU reboot
 * is needed between `info`, `feature`, and `both` bench runs. */
static unsigned spa_probe103_mode(void)
{
    char value[24];
    unsigned numeric = 0;
    FILE *f;

    if (!spa_probe103()) return 0;
    f = fopen(SPA_103_MODE_PATH, "r");
    if (!f) return SPA_103_MODE_BOTH;
    value[0] = '\0';
    if (fscanf(f, "%23s", value) == 1) {
        if (!strcmp(value, "info")) numeric = SPA_103_MODE_INFO;
        else if (!strcmp(value, "feature") || !strcmp(value, "features"))
            numeric = SPA_103_MODE_FEATURE;
        else if (!strcmp(value, "both")) numeric = SPA_103_MODE_BOTH;
        else if (sscanf(value, "%u", &numeric) != 1 || numeric < SPA_103_MODE_INFO ||
                 numeric > SPA_103_MODE_BOTH) {
            numeric = SPA_103_MODE_BOTH;
            LOG_ERROR(LOG_MODULE,
                      "probe103: invalid %s value '%s'; expected info/feature/both; using both",
                      SPA_103_MODE_PATH, value);
        }
    } else {
        numeric = SPA_103_MODE_BOTH;
        LOG_ERROR(LOG_MODULE, "probe103: empty %s; using both", SPA_103_MODE_PATH);
    }
    fclose(f);
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
    int index = SPA_APAC_48K_5_1;
    int candidate = 0;
    FILE *f = fopen(SPA_103_INDEX_PATH, "r");
    if (!f) return index;
    if (fscanf(f, "%d", &candidate) == 1 &&
        (candidate == SPA_AAC_LC_48K_5_1_2 || candidate == SPA_AAC_ELD_48K_5_1_2 ||
         candidate == SPA_DDP_48K_5_1 ||
         candidate == SPA_DDP_48K_9_1_6 || candidate == SPA_APAC_48K_5_1 ||
         candidate == SPA_APAC_48K_5_1_2)) {
        index = candidate;
    } else {
        LOG_ERROR(LOG_MODULE,
                  "probe103: invalid %s; allowed indices: %d/%d/%d/%d/%d/%d; using %d",
                  SPA_103_INDEX_PATH, SPA_AAC_LC_48K_5_1_2, SPA_AAC_ELD_48K_5_1_2,
                  SPA_DDP_48K_5_1, SPA_DDP_48K_9_1_6, SPA_APAC_48K_5_1,
                  SPA_APAC_48K_5_1_2, index);
    }
    fclose(f);
    return index;
}

/* iOS repackages every bit in the legacy mask into one buffered-engine codec family. On wired
 * CarPlay its local support filter leaves bits 11 (PCM) and 22 (AAC-LC) from the old 0x1440800
 * transport-family mask, then rejects the mixed family with -6705 before endpoint activation.
 * Advertise the single AAC-LC compatibility bit by default. Keep the old mask selectable only to
 * reproduce the negative result without another rebuild. */
static uint32_t spa_probe103_legacy_mask(void)
{
    char value[32];
    char *end = 0;
    unsigned long candidate;
    FILE *f = fopen(SPA_103_MASK_PATH, "r");

    if (!f) return SPA_BUFFERED_AAC_MASK;
    value[0] = '\0';
    if (fscanf(f, "%31s", value) != 1) {
        fclose(f);
        LOG_ERROR(LOG_MODULE, "probe103: empty %s; using %#x",
                  SPA_103_MASK_PATH, SPA_BUFFERED_AAC_MASK);
        return SPA_BUFFERED_AAC_MASK;
    }
    fclose(f);

    candidate = strtoul(value, &end, 0);
    if (!end || end == value || *end ||
        (candidate != SPA_BUFFERED_AAC_MASK && candidate != SPA_BUFFERED_BROAD_MASK)) {
        LOG_ERROR(LOG_MODULE,
                  "probe103: invalid %s value '%s'; allowed %#x/%#x; using %#x",
                  SPA_103_MASK_PATH, value, SPA_BUFFERED_AAC_MASK,
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

int spa_inspect_setup_request(void *request, uint32_t sequence)
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

void spa_note_setup_result(int has103, int result, void *response, uint32_t sequence)
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
                 "type-100 PCM/6 code path remains untouched",
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
 * supportsDynamicStreamID. This stage intentionally does not decode or render audio yet; type-100/
 * DIO5 remains entirely stock and available as fallback.
 */
#define SPA_103_STREAM_TYPE       103
#define SPA_103_SESSION_FAMILY_OFF 85  /* peer sockaddr family, verified on stock 210.81 */
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

/* MU1316 libairplay.so fingerprinted private request-dispatch seam.  AirTunesServer_EnsureCreated
 * passes a 0x2c-byte callback table to HTTPServerCreate; HTTPServerCreate copies it to server+0x40,
 * putting the request callback at server+0x64.  Replacing this heap slot is safer than rewriting
 * ARM instructions.  The installer below refuses every image/slot that does not exactly match the
 * reviewed MU1316 layout. */
#define SPA_103_MU1316_ENSURE_CREATED_VA 0x0002e9acu
#define SPA_103_MU1316_HTTP_PROCESS_VA   0x000302e0u
#define SPA_103_MU1316_SERVER_GLOBAL_VA  0x000babccu
#define SPA_103_HTTP_PROCESS_SLOT_OFF    0x64u

/* Target HTTPMessage layout used by the stock 210.81 dispatcher. */
#define SPA_103_HTTP_METHOD_PTR_OFF      0x201cu
#define SPA_103_HTTP_METHOD_LEN_OFF      0x2020u
#define SPA_103_HTTP_BODY_PTR_OFF        0x209cu
#define SPA_103_HTTP_BODY_LEN_OFF        0x20a8u

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
static uint32_t spa103_http_generation;
static spa_cf_create_plist_bytes_t spa103_cf_create_plist_bytes;
static spa_cf_type_id_t spa103_cf_dictionary_type_id;
static spa_cf_type_id_t spa103_cf_data_type_id;
static spa_cf_dict_get_typed_t spa103_cf_dict_get_typed;
static spa_cf_data_length_t spa103_cf_data_length;
static spa_cf_data_bytes_t spa103_cf_data_bytes;
static spa_http_init_response_t spa103_http_init_response;
static spa_http_send_response_t spa103_http_send_response;

static int spa103_http_process(void *connection, void *request);
static size_t spa103_write_capture(int fd, const uint8_t *data, size_t size);

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

/* Request callback installed only in the verified MU1316 HTTPServer object. The request is already
 * fully read when this callback runs, so the binary-plist body lives at the same HTTPMessage fields
 * used by the stock POST/SET_PARAMETER handlers. */
static int spa103_http_process(void *connection, void *request)
{
    static const char methodName[] = "SETMAGICCOOKIE";
    const uint8_t *method;
    const uint8_t *body;
    const uint8_t *cookieBytes;
    uint32_t methodLength, bodyLength;
    void *plist = 0, *idKey = 0, *cookieKey = 0, *cookieData = 0;
    uint8_t *cookieCopy = 0;
    int64_t cookieID = 0;
    long cookieLength;
    int32_t err = 0;
    int status = 200;

    if (!request || !connection || !spa103_original_http_process)
        return -6714;

    method = *(const uint8_t * const *)((const uint8_t *)request +
                                         SPA_103_HTTP_METHOD_PTR_OFF);
    methodLength = *(const uint32_t *)((const uint8_t *)request +
                                       SPA_103_HTTP_METHOD_LEN_OFF);
    if (!method || methodLength != sizeof(methodName) - 1 ||
        memcmp(method, methodName, sizeof(methodName) - 1) != 0)
        return spa103_original_http_process(connection, request);

    body = *(const uint8_t * const *)((const uint8_t *)request + SPA_103_HTTP_BODY_PTR_OFF);
    bodyLength = *(const uint32_t *)((const uint8_t *)request + SPA_103_HTTP_BODY_LEN_OFF);
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

static int spa103_install_magic_cookie_dispatcher(uint32_t generation)
{
    static const uint8_t ensureCreatedPrologue[12] = {
        0xf0, 0x45, 0x2d, 0xe9, 0xdc, 0x41, 0x9f, 0xe5,
        0xdc, 0x61, 0x9f, 0xe5
    };
    uintptr_t ensureAddress, imageBase;
    void *outer, *server;
    void **callbackSlot;
    int installed = 0;

    pthread_mutex_lock(&spa103_http_lock);
    ensureAddress = (uintptr_t)dlsym(RTLD_NEXT, "AirTunesServer_EnsureCreated");
    if (ensureAddress < SPA_103_MU1316_ENSURE_CREATED_VA) {
        LOG_ERROR(LOG_MODULE, "buffer103: AirTunesServer_EnsureCreated unavailable");
        goto done;
    }
    imageBase = ensureAddress - SPA_103_MU1316_ENSURE_CREATED_VA;
    if ((imageBase & 0xfffu) != 0 ||
        memcmp((const void *)(imageBase + SPA_103_MU1316_ENSURE_CREATED_VA),
               ensureCreatedPrologue, sizeof(ensureCreatedPrologue)) != 0) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: libairplay private dispatcher fingerprint mismatch; refusing patch");
        goto done;
    }

    outer = *(void **)(imageBase + SPA_103_MU1316_SERVER_GLOBAL_VA);
    if (!outer || ((uintptr_t)outer & 3u) != 0) {
        LOG_ERROR(LOG_MODULE, "buffer103: AirTunes server object unavailable");
        goto done;
    }
    server = *(void **)outer;
    if (!server || ((uintptr_t)server & 3u) != 0) {
        LOG_ERROR(LOG_MODULE, "buffer103: HTTPServer object unavailable");
        goto done;
    }
    callbackSlot = (void **)((uint8_t *)server + SPA_103_HTTP_PROCESS_SLOT_OFF);
    if (*callbackSlot == (void *)spa103_http_process && spa103_original_http_process) {
        spa103_http_process_slot = callbackSlot;
        spa103_http_generation = generation;
        installed = 1;
        goto done;
    }
    if (*callbackSlot != (void *)(imageBase + SPA_103_MU1316_HTTP_PROCESS_VA)) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: HTTPServer request slot mismatch actual=%p expected=%p; refusing patch",
                  *callbackSlot, (void *)(imageBase + SPA_103_MU1316_HTTP_PROCESS_VA));
        goto done;
    }

    spa103_original_http_process = (spa_http_process_t)*callbackSlot;
    __sync_synchronize();
    *callbackSlot = (void *)spa103_http_process;
    spa103_http_process_slot = callbackSlot;
    spa103_http_generation = generation;
    __sync_synchronize();
    LOG_WARN(LOG_MODULE,
             "buffer103: installed fingerprint-locked SETMAGICCOOKIE dispatcher server=%p slot=%p",
             server, callbackSlot);
    installed = 1;

done:
    pthread_mutex_unlock(&spa103_http_lock);
    return installed;
}

static void spa103_restore_magic_cookie_dispatcher(uint32_t generation)
{
    void **callbackSlot;

    pthread_mutex_lock(&spa103_http_lock);
    callbackSlot = spa103_http_process_slot;
    if (!callbackSlot || !spa103_original_http_process ||
        spa103_http_generation != generation) {
        pthread_mutex_unlock(&spa103_http_lock);
        return;
    }
    if (*callbackSlot == (void *)spa103_http_process) {
        __sync_synchronize();
        *callbackSlot = (void *)spa103_original_http_process;
        __sync_synchronize();
        LOG_INFO(LOG_MODULE, "buffer103: restored stock RTSP request dispatcher slot=%p",
                 callbackSlot);
    } else if (*callbackSlot != (void *)spa103_original_http_process) {
        LOG_ERROR(LOG_MODULE,
                  "buffer103: RTSP request slot changed by another owner actual=%p; not restoring",
                  *callbackSlot);
    }
    spa103_http_process_slot = 0;
    spa103_http_generation = 0;
    pthread_mutex_unlock(&spa103_http_lock);
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
    if (!spa103_srv_sockopen || !spa103_sock_accept || !spa103_cf_create_plist_bytes ||
        !spa103_cf_dictionary_type_id || !spa103_cf_data_type_id ||
        !spa103_cf_dict_get_typed || !spa103_cf_data_length || !spa103_cf_data_bytes ||
        !spa103_http_init_response || !spa103_http_send_response) {
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
    uint8_t apapHeader[4];
    char path[128];
    uint64_t total = 0;
    uint64_t apapFrames = 0;
    size_t saved = 0;
    size_t prefixSize = 0;
    size_t apapHeaderSize = 0;
    uint32_t apapPayloadRemaining = 0;
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
            if (apapPayloadRemaining == 0 && apapHeaderSize < sizeof(apapHeader))
                want = sizeof(apapHeader) - apapHeaderSize;
            else if (apapPayloadRemaining < want)
                want = apapPayloadRemaining;
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
                int frameComplete = 0;

                if (apapPayloadRemaining == 0 && apapHeaderSize < sizeof(apapHeader)) {
                    uint32_t frameLength;
                    memcpy(apapHeader + apapHeaderSize, buffer, (size_t)n);
                    apapHeaderSize += (size_t)n;
                    if (apapHeaderSize == sizeof(apapHeader)) {
                        frameLength = ((uint32_t)apapHeader[0] << 24) |
                                      ((uint32_t)apapHeader[1] << 16) |
                                      ((uint32_t)apapHeader[2] << 8) |
                                      (uint32_t)apapHeader[3];
                        if (frameLength < sizeof(apapHeader) ||
                            frameLength > SPA_103_APAP_MAX_FRAME) {
                            LOG_ERROR(LOG_MODULE,
                                      "buffer103: invalid APAP inclusive frame length=%u after "
                                      "%llu frames; closing capture receiver",
                                      (unsigned)frameLength,
                                      (unsigned long long)apapFrames);
                            break;
                        }
                        apapPayloadRemaining = frameLength - sizeof(apapHeader);
                        if (apapPayloadRemaining == 0) frameComplete = 1;
                    }
                } else {
                    apapPayloadRemaining -= (uint32_t)n;
                    if (apapPayloadRemaining == 0) frameComplete = 1;
                }

                if (frameComplete) {
                    struct timespec pace = { 0, SPA_103_APAP_FRAME_NS };
                    apapFrames++;
                    apapHeaderSize = 0;

                    /* Apple's buffered hose queues 32 packages and then suspends network reads
                     * until playback consumes one.  We do not decode yet, but pacing one package
                     * per 1024/48k interval reproduces that backpressure instead of advertising a
                     * 21 MB receiver and draining roughly 100 seconds of audio in 4.5 seconds. */
                    if (apapFrames >= SPA_103_APAP_PREFILL_FRAMES) {
                        while (nanosleep(&pace, &pace) != 0 && errno == EINTR) {}
                    }
                }
            }
        } else if (n == 0) {
            break;
        } else if (errno != EINTR && errno != EAGAIN) {
            break;
        }
    }

done:
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
        spa103_clear_magic_cookies_locked();
        pthread_cond_broadcast(&spa103_cond);
    }
    pthread_mutex_unlock(&spa103_lock);
    spa103_restore_magic_cookie_dispatcher(generation);
}

int spa_setup_buffered_receiver(void *session, void *request, void *response,
                                uint32_t sequence)
{
    spa103_thread_arg_t *threadArg = 0;
    void *streamDesc = 0, *responseDesc = 0;
    uint64_t streamID = 0;
    int64_t supportsDynamicStreamID = 0;
    uint32_t generation = 0;
    int audioBufferSize, family, port = 0;
    int sock = -1, err = -6728;
    int started = 0;

    if (!session || !request || !response || !spa_probe103() ||
        !spa_cf_resolve() || !spa103_resolve()) return -6714;
    streamDesc = spa_find_setup_stream(request, SPA_103_STREAM_TYPE);
    if (!streamDesc) return 0;
    audioBufferSize = spa103_audio_buffer_size(streamDesc);

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
    pthread_mutex_unlock(&spa103_lock);

    family = *(const uint8_t *)((const char *)session + SPA_103_SESSION_FAMILY_OFF);
    err = spa103_srv_sockopen(family, 1 /* SOCK_STREAM */, 6 /* IPPROTO_TCP */,
                              SPA_103_PREFERRED_PORT, &port, -180224, &sock);
    LOG_INFO(LOG_MODULE,
             "buffer103: ServerSocketOpen err=%d family=%d port=%d sock=%d sequence=%u",
             err, family, port, sock, (unsigned)sequence);
    if (err || sock < 0 || port <= 0) goto fail;

    /* SETMAGICCOOKIE is a custom method on the already-open RTSP connection, not a 6033/RTCP
     * payload. Install its strictly fingerprinted dispatcher before publishing this SETUP. */
    if (!spa103_install_magic_cookie_dispatcher(generation)) {
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
             "arrivalToRenderLatencyMs:0,streamID:%llu}; capture receiver live; "
             "audio decode/render intentionally pending",
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

void spa_teardown_buffered_receiver(void *session, void *request)
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
    ext  = cfa_arrm(0, 1, cfa_arr_cb);
    legacyMask = spa_probe103_legacy_mask();
    kType      = cfa_str(0, "type",                       SPA_CFENC_UTF8);
    kAudioType = cfa_str(0, "audioType",                  SPA_CFENC_UTF8);
    kOut       = cfa_str(0, "audioOutputFormats",         SPA_CFENC_UTF8);
    kExt       = cfa_str(0, "audioOutputFormatsExtended", SPA_CFENC_UTF8);

    if (dict && ext && kType && kAudioType && kOut && kExt) {
        /* CFNumberCreateInt64 takes ONE argument on this stack (verified: libairplay calls it as
         * `CFNumberCreateInt64(4, 0)` = one int64 across two ARM regs), not (alloc, type, ptr). */
        void *num = 0;
        void *(*cfnum)(int64_t) = (void *(*)(int64_t))dlsym(RTLD_DEFAULT, "CFNumberCreateInt64");

        cfa_dictseti(dict, kType,      103);
        cfa_dictsetc(dict, kAudioType, "media", -1);
        cfa_dictseti(dict, kOut,       legacyMask);

        {
            int formatIndex = spa_probe103_index();
            if (cfnum) num = cfnum(formatIndex);
            if (num) {
                cfa_arrapp(ext, num);
                cfa_rel(num);
                cfa_dictset(dict, kExt, ext);
                cfa_arrapp(result, dict);
                LOG_INFO(LOG_MODULE,
                         "probe103: /info audioFormats += {type:103, audioType:media, "
                         "audioOutputFormats:%#x, audioOutputFormatsExtended:[%d]} (now %ld entries)",
                         legacyMask, formatIndex, cfa_arrcount(result));
            } else {
                LOG_ERROR(LOG_MODULE, "probe103: CFNumberCreateInt64 unavailable — entry NOT added");
            }
        }
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
int spa_augment_setup_response(void *session, void *request, void *response, uint32_t sequence)
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

    /* A new control SETUP retires any previous session's contract even when the marker was removed
     * between reconnects. Once this response enables the feature we set the new owner below. */
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

void spa_augment_server_info(void *session, void *info)
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
                          "probe103: %s present but CFLite Boolean unavailable; DCX disabled",
                          SPA_103_DCX_PATH);
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

void spa_capture_setup_response(void *request, void *response, uint32_t sequence)
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

void spa_capture_server_info(void *info)
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
 * type-100 fallback response stays stock.  The master type-103 marker gates logs and plist files. */
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

__attribute__((constructor)) static void audio_module_init(void)
{
    unsigned bit, channels;
    spa_tune(&bit, &channels);
    if (spa_enabled())
        LOG_INFO(LOG_MODULE, "multichannel PCM hook ARMED (SPATIAL_5_1=on): bit%u, %uch ASBD "
                 "(tune live via `echo <bit>[:<ch>] > %s`, no reboot)", bit, channels, SPA_TUNE_PATH);
    else
        LOG_INFO(LOG_MODULE, "multichannel PCM hook present, OFF (SPATIAL_5_1 unset) -> stock stereo");
}
