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
        if (c >= 1 && c <= 8)   *channels = c;
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
        if (spa_enabled() && fmt == ((uint64_t)1 << bit) && asbd) {
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

/* ---- config seam (optional, called from altscreen SessionSetup wrapper) --------------------
 * Discrete 5.1 PCM rides the format bit above and needs nothing here. `spatialAudioSupported`
 * (head-tracked "Spatial Audio" UX, CARSessionConfiguration / altScreen-class) is a SEPARATE flag;
 * kept as a seam so altscreen can add "spatialAudio" to the control-SETUP enabledFeatures without a
 * second AirPlayReceiverSessionSetup interposition. No-op for now (the 6ch PCM path is independent). */
int spa_augment_setup_response(void *session, void *request, void *response)
{
    (void)session; (void)request; (void)response;
    return 0;
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
