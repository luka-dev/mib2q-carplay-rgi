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
#include <stdlib.h>
#include <string.h>

DEFINE_LOG_MODULE(audio);

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

/*
 * The AirPlayAudioFormat bit for 5.1 PCM (PCM/48000/16/5.1), extracted statically from iOS 27
 * AirPlaySupport `_APAudioFormatCopyAudioFormatsToReadableString` (bit->string table) = bit 44.
 * Other useful bits from the same table: PCM/48000/32f/5.1 = 1<<47; PCM/48000/16/5.1.2 = 1<<38;
 * DDPLUS/48000/5.1 = 1<<50; DDPLUS/48000/9.1.6 = 1<<53. SPA_FMT_5_1 is defined in audio_hook.h.
 * Channel order = Apple canonical 5.1 (L R C LFE Ls Rs) — match to the Audi six_channel_speakerp
 * order on the mixer side. */
#define SPA_SR        48000
#define SPA_BITS      16
#define SPA_CHANNELS  6
#define SPA_BPF       (SPA_CHANNELS * (SPA_BITS / 8))   /* bytes per frame = 12 */

/* ---- AudioSessionGetSupportedOutputFormats -------------------------------------------------
 * The 64-bit capability mask the AirPlay negotiation offers the phone. OR-in the 5.1 bit so the
 * phone is allowed to send 6ch. Stereo path untouched. */
typedef int64_t (*fn_get_formats_t)(int /*streamType*/);

int64_t AudioSessionGetSupportedOutputFormats(int streamType)
{
    static fn_get_formats_t real;
    if (!real) real = (fn_get_formats_t)dlsym(RTLD_NEXT, "AudioSessionGetSupportedOutputFormats");
    int64_t caps = real ? real(streamType) : 0;

    if (spa_enabled() && streamType == 100 /* main audio only */) {
        caps |= (int64_t)SPA_FMT_5_1;
        LOG_INFO(LOG_MODULE, "advertise 5.1: caps now %#llx (type=%d)",
                 (unsigned long long)caps, streamType);
    }
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
    if (spa_enabled() && fmt == SPA_FMT_5_1 && asbd) {
        asbd[0] = SPA_SR;             /* sampleRate      */
        asbd[1] = 1819304813;         /* 'lpcm'          */
        asbd[2] = 0xC;                /* signed + packed */
        asbd[3] = SPA_BPF;            /* bytesPerPacket  */
        asbd[4] = 1;                  /* framesPerPacket */
        asbd[5] = SPA_BPF;            /* bytesPerFrame   */
        asbd[6] = SPA_CHANNELS;       /* channelsPerFrame*/
        asbd[7] = SPA_BITS;           /* bitsPerChannel  */
        asbd[8] = 0;                  /* reserved        */
        if (bits_out) *bits_out = SPA_BITS;
        LOG_INFO(LOG_MODULE, "ASBD 5.1: %u Hz, %u ch, %u-bit, bpf=%u (fmt=%#llx)",
                 SPA_SR, SPA_CHANNELS, SPA_BITS, SPA_BPF, (unsigned long long)fmt);
        return 0;
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
    if (spa_enabled())
        LOG_INFO(LOG_MODULE, "5.1 PCM audio hook ARMED (SPATIAL_5_1=on) — advertising bit44 + 6ch ASBD");
    else
        LOG_INFO(LOG_MODULE, "5.1 PCM audio hook present, OFF (SPATIAL_5_1 unset) -> stock stereo");
}
