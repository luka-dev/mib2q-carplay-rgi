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
         * 42 = AAC), emitting that ASBD would feed the amp full-scale noise instead of music. */
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
static cf_rel_t      cfa_rel;
static void         *cfa_arr_cb, *cfa_dk_cb, *cfa_dv_cb;
static int           cfa_ready = -1;

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
    cfa_rel      = (cf_rel_t)      dlsym(RTLD_DEFAULT, "CFRelease");
    cfa_arr_cb   = dlsym(RTLD_DEFAULT, "kCFLArrayCallBacksCFLTypes");
    cfa_dk_cb    = dlsym(RTLD_DEFAULT, "kCFLDictionaryKeyCallBacksCFLTypes");
    cfa_dv_cb    = dlsym(RTLD_DEFAULT, "kCFLDictionaryValueCallBacksCFLTypes");

    cfa_ready = (cfa_str && cfa_equal && cfa_dictm && cfa_arrm && cfa_arrapp && cfa_arrcount &&
                 cfa_arridx && cfa_dictset && cfa_dictseti && cfa_dictsetc && cfa_dictget &&
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

    if (!real) real = (fn_copyprop_t)dlsym(RTLD_NEXT, "AirPlayReceiverServerPlatformCopyProperty");
    if (!real) { LOG_ERROR(LOG_MODULE, "CopyProperty real==NULL"); if (outErr) *outErr = -6714; return 0; }

    result = real(server, sessionID, property, qualifier, outErr);
    if (!result || !property || !spa_probe103() || !spa_cf_resolve()) return result;

    kAudioFormats = cfa_str(0, "audioFormats", SPA_CFENC_UTF8);
    if (!kAudioFormats) return result;
    if (!cfa_equal(property, kAudioFormats)) { cfa_rel(kAudioFormats); return result; }
    cfa_rel(kAudioFormats);

    dict = cfa_dictm(0, 0, cfa_dk_cb, cfa_dv_cb);
    ext  = cfa_arrm(0, 1, cfa_arr_cb);
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
        cfa_dictseti(dict, kOut,       SPA_BUFFERED_LEGACY_MASK);

        if (cfnum) num = cfnum(SPA_APAC_48K_5_1);
        if (num) {
            cfa_arrapp(ext, num);
            cfa_rel(num);
            cfa_dictset(dict, kExt, ext);
            cfa_arrapp(result, dict);
            LOG_INFO(LOG_MODULE,
                     "probe103: /info audioFormats += {type:103, audioType:media, "
                     "audioOutputFormats:%#x, audioOutputFormatsExtended:[%d]} (now %ld entries)",
                     SPA_BUFFERED_LEGACY_MASK, SPA_APAC_48K_5_1, cfa_arrcount(result));
        } else {
            LOG_ERROR(LOG_MODULE, "probe103: CFNumberCreateInt64 unavailable — entry NOT added");
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
int spa_augment_setup_response(void *session, void *request, void *response)
{
    void *kReq, *kEnab, *cur, *neg;
    long curN, i;
    int added = 0;

    (void)session;
    if (!request || !response || !spa_probe103() || !spa_cf_resolve()) return 0;

    /* control SETUP only — the media SETUPs carry no `features` proposal */
    kReq = cfa_str(0, "features", SPA_CFENC_UTF8);
    if (!kReq) return 0;
    if (!cfa_dictget(request, kReq)) { cfa_rel(kReq); return 0; }
    cfa_rel(kReq);

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
        if (cfa_arrappc && cfa_arrappc(neg, "mainBuffered", -1) == 0) {
            added = 1;
        } else {
            void *s = cfa_str(0, "mainBuffered", SPA_CFENC_UTF8);
            if (s) { cfa_arrapp(neg, s); cfa_rel(s); added = 1; }
        }
        if (added) {
            cfa_dictset(response, kEnab, neg);
            LOG_INFO(LOG_MODULE,
                     "probe103: control SETUP response enabledFeatures += mainBuffered "
                     "(preserved %ld existing)", curN);
        }
        cfa_rel(neg);
    }
    cfa_rel(kEnab);
    return added;
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
