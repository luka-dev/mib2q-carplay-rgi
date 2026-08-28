/*
 * audio_hook.c — CarPlay Spatial 5.1 PCM: widen the AirPlay audio-format negotiation to 6 channels.
 * See audio_hook.h and docs/reference/AUDIO.md (sections 5-8).
 *
 * ponytail: scaffold. Two things are deliberately left as probe-determined constants because they can
 * only be pinned on a live iOS connect / on the unit — until then the module is inert (SPA_FMT_5_1==0):
 *   1. SPA_FMT_5_1 — the AirPlayAudioFormat bit iOS 27 assigns to 5.1 PCM (bits 31+ of the uint64;
 *      the on-cache extraction is tooling-blocked, see CARPLAY_SPATIAL_DOLBY_PLAN.md Phase 0). Pin it
 *      by advertising a candidate bit and reading the negotiated format back from the SETUP exchange.
 *   2. The exact ASBD field offsets for the channel override (below) — verified shape from the stock
 *      AirPlayAudioFormatToPCM decompile, to be confirmed by logging a real 6ch ASBD on the unit.
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
 * The AirPlayAudioFormat bit for 5.1 PCM (PCM/48000/16/5.1).  EXTRACTED STATICALLY from iOS 27
 * AirPlaySupport `_APAudioFormatCopyAudioFormatsToReadableString` (bit->string table): bit 44.
 * Other useful bits from the same table: PCM/48000/32f/5.1 = 1<<47; PCM/48000/16/5.1.2 = 1<<38;
 * DDPLUS/48000/5.1 = 1<<50; DDPLUS/48000/9.1.6 = 1<<53. Channel order = Apple canonical 5.1
 * (L R C LFE Ls Rs) — must be matched to the Audi six_channel_speakerp order on the mixer side.
 *
 * WARNING — 32-bit pipeline: bit 44 is > 31, but stock `AirPlayAudioFormatToPCM(int a1)` receives the
 * format as a 32-bit int (and the stored format at AudioStreamPrivate+16 is a _DWORD). So the low 32
 * bits of bit 44 are 0 -> the ToPCM hook below CANNOT match our format; the multichannel case must be
 * handled at the 64-bit negotiation layer (SETUP) and drive the AudioStream setup directly. The ToPCM
 * hook stays for the (unreachable-for-bit44) stereo-compatible case + as a probe log. See AUDIO.md.
 */
#define SPA_FMT_5_1   ((uint64_t)1 << 44)   /* PCM/48000/16/5.1 — verified in AirPlaySupport */
#define SPA_CHANNELS  6                     /* L R C LFE Ls Rs (Apple order) */

/* Kept OFF regardless until the 64-bit SETUP-layer path is built (bit 44 can't ride the 32-bit ToPCM
 * pipeline). Flip to 1 only alongside that work. */
#define SPA_FMT_ARMED 0

/* ---- AudioSessionGetSupportedOutputFormats -------------------------------------------------
 * Returns the 64-bit capability mask the AirPlay negotiation offers the phone. We OR-in the 5.1 bit
 * so the phone is allowed to send 6ch. Stereo path is untouched. */
typedef int64_t (*fn_get_formats_t)(int /*streamType*/);

int64_t AudioSessionGetSupportedOutputFormats(int streamType)
{
    static fn_get_formats_t real;
    if (!real) real = (fn_get_formats_t)dlsym(RTLD_NEXT, "AudioSessionGetSupportedOutputFormats");
    int64_t caps = real ? real(streamType) : 0;

#if SPA_FMT_ARMED
    if (spa_enabled() && streamType == 100 /* main audio only */) {
        caps |= (int64_t)SPA_FMT_5_1;
        LOG_INFO(LOG_MODULE, "advertise 5.1: caps now %#llx (type=%d)",
                 (unsigned long long)caps, streamType);
    }
#endif
    return caps;
}

/* ---- AirPlayAudioFormatToPCM ---------------------------------------------------------------
 * Fills a 36-byte PCM ASBD (a3) for the negotiated format a1. For our 5.1 format we let the real fill
 * a base ASBD, then rewrite the channel-dependent fields to 6 channels. ASBD dword layout CONFIRMED
 * from AirPlayAudioFormatToASBD (0x2bda0) — NOT the standard CoreAudio ASBD (sampleRate is UInt32,
 * not Float64):
 *   a3[0]=sampleRate(UInt32) a3[1]='lpcm'(1819304813) a3[2]=formatFlags(0xC=signed+packed)
 *   a3[3]=bytesPerPacket a3[4]=framesPerPacket(1) a3[5]=bytesPerFrame
 *   a3[6]=channelsPerFrame a3[7]=bitsPerChannel a3[8]=reserved(0)
 * Stock stereo defaults: 48000/24/2 -> bpf=6 ; 48000/16/2 -> bpf=4 (bpf = channels*(bits/8)). */
typedef int (*fn_fmt_to_pcm_t)(int /*a1*/, int /*a2*/, uint32_t * /*a3 ASBD*/);

int AirPlayAudioFormatToPCM(int a1, int a2, uint32_t *a3)
{
    static fn_fmt_to_pcm_t real;
    if (!real) real = (fn_fmt_to_pcm_t)dlsym(RTLD_NEXT, "AirPlayAudioFormatToPCM");
    if (!real) { LOG_ERROR(LOG_MODULE, "AirPlayAudioFormatToPCM real==NULL"); return -1; }

    int rc = real(a1, a2, a3);

#if SPA_FMT_ARMED
    /* NOTE: bit 44 is truncated to 0 in this 32-bit `a1`, so this can never match SPA_FMT_5_1 as-is.
     * The real 5.1 path drives the AudioStream setup from the 64-bit SETUP layer. Left here only for
     * the eventual low-bit / probe case and as a channel-rewrite reference (offsets verified above). */
    if (rc == 0 && a3 && spa_enabled() && ((uint64_t)(uint32_t)a1 == (uint32_t)SPA_FMT_5_1)) {
        uint32_t bits  = a3[7] ? a3[7] : 16;
        uint32_t bpf   = SPA_CHANNELS * (bits / 8);   /* bytes per frame  */
        a3[6] = SPA_CHANNELS;                         /* channelsPerFrame */
        a3[5] = bpf;                                  /* bytesPerFrame    */
        a3[3] = bpf;                                  /* bytesPerPacket (framesPerPacket=1) */
        LOG_INFO(LOG_MODULE, "ASBD -> %u ch, %u-bit, bpf=%u (fmt=%#x)",
                 SPA_CHANNELS, bits, bpf, (unsigned)a1);
    }
#endif
    return rc;
}

/* ---- config seam (called from altscreen SessionSetup wrapper) ------------------------------
 * spatialAudioSupported rides CARSessionConfiguration. To avoid a second AirPlayReceiverSessionSetup
 * interposition (altscreen owns that symbol), altscreen calls this to add "spatialAudio" to the
 * response enabledFeatures on the control SETUP. TODO(wire): implement the CF array append using the
 * altscreen CF helpers (cf_str/cf_arrapp) once the enabledFeatures key handling is shared; for now a
 * documented no-op so the build and the stereo path stay clean. */
int spa_augment_setup_response(void *session, void *request, void *response)
{
    (void)session; (void)request; (void)response;
    if (!spa_enabled()) return 0;
    /* TODO(wire): append CFSTR("spatialAudio") to response["enabledFeatures"] (control SETUP only),
     * mirroring how altscreen adds "altScreen"/"viewAreas". Needs the shared CF helper seam. */
    return 0;
}

/* ---- module init ---------------------------------------------------------------------------- */

__attribute__((constructor)) static void audio_module_init(void)
{
    if (spa_enabled())
        LOG_INFO(LOG_MODULE, "Spatial 5.1 audio hook armed (SPATIAL_5_1=on)%s",
                 SPA_FMT_5_1 ? "" : " but SPA_FMT_5_1 unset -> inert until format bit probed");
    else
        LOG_INFO(LOG_MODULE, "Spatial 5.1 audio hook present, OFF (SPATIAL_5_1 unset) -> stock stereo");
}
