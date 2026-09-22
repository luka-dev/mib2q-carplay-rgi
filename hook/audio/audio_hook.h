/*
 * audio_hook.h — CarPlay multichannel (Spatial 5.1 PCM) audio hook.
 *
 * Goal: make the iPhone push discrete 5.1 PCM over wired CarPlay to the B&O amp, by widening the
 * AirPlay audio-format negotiation from the stock stereo ceiling to 6-channel PCM.  See
 * docs/reference/AUDIO.md (sections 5-8) for the full plan.
 *
 * Strategy = LD_PRELOAD interposition of stock libairplay.so exports (all verified exported, T; both
 * reached via .plt/GOT so intra-library calls are caught too — same mechanism altscreen uses):
 *   - AudioSessionGetSupportedOutputFormats  (0x497bc, returns __int64) — advertise the 5.1 bit.
 *   - AirPlayAudioFormatToASBD               (0x2bda0, u64 format)      — emit a 6ch ASBD for it.
 * The whole format pipeline is 64-bit clean (no truncation): SETUP reads audioFormat via
 * CFDictionaryGetInt64, stores the u64 at stream+0x10, and both the SETUP validation and the stream
 * build call ASBD(u64) intact. ASBD's exact-match table is the only blocker (rejects high bits with
 * -6735); interposing it removes the rejection AND supplies the 6ch ASBD for both callers. The 6ch
 * count then flows unpatched through AudioStreamPrepare -> CQNXSoundIF -> snd_pcm on mpl5_dio_ent
 * (channels come straight from the ASBD, verified 0x4a9d0). The dio/mixer side must provision
 * mpl5_dio_ent for 6 voices -> six_channel_speakerp (B&O firmware agent's dependency).
 *
 * No SETUP rewrite / flag / sentinel is needed (an earlier design assumed a 32-bit truncation that
 * does not exist). spatialAudioSupported (head-tracked "Spatial Audio" UX) is a SEPARATE optional flag
 * on CARSessionConfiguration, injectable from the existing altscreen SessionSetup wrapper via
 * spa_augment_setup_response() below; discrete 5.1 PCM does not depend on it.
 *
 * Runtime-gated: canonical deployments use AUDIO_SPATIAL/AUDIO_SPATIAL_ATMOS;
 * the checked-in policy enables AUDIO_SPATIAL. SPATIAL_5_1 is read only as a
 * migration fallback when none of the canonical controls is present. The
 * six-voice mixer/deva side must be ready before arming.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef AUDIO_HOOK_H
#define AUDIO_HOOK_H

#include <stddef.h>
#include <stdint.h>

#include "../framework/hook_framework.h"

/* Advertised main-audio format bit: 5.1 PCM (PCM/48000/16/5.1), from the iOS 27 descriptor table.
 * See AUDIO.md sections 9-10, 24. */
#define SPA_DEFAULT_BIT   44u
#define SPA_FMT_5_1       ((uint64_t)1 << SPA_DEFAULT_BIT)

/* User-facing audio policy. Values are strict booleans:
 * 1/true/yes/on and 0/false/no/off (case-insensitive). */
#define SPA_AUDIO_ENV                 "AUDIO_SPATIAL"
#define SPA_AUDIO_ATMOS_ENV           "AUDIO_SPATIAL_ATMOS"
#define SPA_AUDIO_ATMOS_MODE_ENV      "AUDIO_SPATIAL_ATMOS_MODE"
/* Default-on recovery for a buffered stream which is the first media stream
 * after connection. Set AUDIO_COLD_CONNECT=0 only as a rollback switch. */
#define SPA_AUDIO_COLD_CONNECT_ENV     "AUDIO_COLD_CONNECT"

/* Optional stereo-to-5.1 ambience renderer.  This is deliberately independent
 * of the transport policy above: setting it must never advertise or enable a
 * buffered stream by itself.  The code default is off; deployment policy may
 * opt in explicitly.
 *
 * AUDIO_STEREO_UPMIX: off | on.  ON always selects HSSR Universal.
 * AUDIO_STEREO_SURROUND: rear input gain, 0.0 .. 0.5 (default 0.18).
 * HSSR Universal uses persistent half-plane object routing and a bounded,
 * phase-distinct mid/high fallback so difficult or nearly mono masters retain
 * rear activity instead of collapsing to silence.  Its separated Ls/Rs then
 * receive a fixed +3 dB high shelf with a 4-kHz midpoint before the linked
 * rear limiter; the shelf is not applied to dry L/R, C or LFE.
 * AUDIO_STEREO_LFE: off | assist (code default off). `assist` selects only
 * Cabin-Aware LFE V2: a phase-safe 48-Hz LR4 sub-band, HSSR bass/drum level
 * steering, paired front headroom and a recovered-J525-matrix limiter.
 * AUDIO_HSSR_MODEL: model blob path.  The default is next to the hook; an
 * invalid or missing model leaves stereo upmix safely OFF. */
#define SPA_AUDIO_STEREO_UPMIX_ENV    "AUDIO_STEREO_UPMIX"
#define SPA_AUDIO_STEREO_SURROUND_ENV "AUDIO_STEREO_SURROUND"
#define SPA_AUDIO_STEREO_LFE_ENV      "AUDIO_STEREO_LFE"
#define SPA_AUDIO_STEREO_FRONT_BASS_ENV "AUDIO_STEREO_FRONT_BASS_DB"
/* Independent experimental front retention0..1, default0 (existing removal).
 * This does not change the rear synthesis gain or select float rendering. */
#define SPA_AUDIO_STEREO_FRONT_RETENTION_ENV "AUDIO_STEREO_FRONT_RETENTION"
/* Scene-bed distribution experiments (plan 2026-09-21); all default off. */
#define SPA_AUDIO_HSSR_BED_DELAYS_ENV      "AUDIO_HSSR_BED_DELAYS"     /* on/off */
#define SPA_AUDIO_HSSR_BED_ATTACK_ENV      "AUDIO_HSSR_BED_ATTACK"     /* off|owner|front */
#define SPA_AUDIO_HSSR_ATTACK_COLLAPSE_ENV "AUDIO_HSSR_ATTACK_COLLAPSE" /* 0..1, default 1 */
/* Front retention taper in Hz, "LO:HI"; HI <= LO disables it (retention flat
 * over frequency).  Default SPA_STEREO_UPMIX_RETENTION_HF_{LO,HI}_HZ. */
#define SPA_AUDIO_HSSR_RETENTION_HF_ENV "AUDIO_HSSR_RETENTION_HF"
/* HSSR upmix of the stock realtime (type-100) media PCM, e.g. Spotify, which
 * never gets buffered type 103.  on/off, default off. */
#define SPA_AUDIO_REALTIME_UPMIX_ENV       "AUDIO_REALTIME_UPMIX"
/* AUDIO_STEREO_LFE_GAIN: assist level in dB re4096 (-24..+12; 0=default).
 * Corrected BANK1 also feeds the sub from other channels; this is NOT the
 * whole subwoofer level. */
#define SPA_AUDIO_STEREO_LFE_GAIN_ENV "AUDIO_STEREO_LFE_GAIN"
#define SPA_AUDIO_HSSR_MODEL_ENV       "AUDIO_HSSR_MODEL"
#define SPA_AUDIO_HSSR_MODEL_DEFAULT   "/mnt/app/root/hooks/hssr_model.bin"

/* Buffered-stream loudness policy.  Apple's/Pioneer's SoundCheck metadata is
 * enabled by default.  The local RMS substitute and sample-peak limiter are
 * explicit laboratory/safety options, not replacements for that metadata. */
#define SPA_AUDIO_SOUNDCHECK_ENV          "AUDIO_SOUNDCHECK"
#define SPA_AUDIO_SOUNDCHECK_FALLBACK_ENV "AUDIO_SOUNDCHECK_FALLBACK"
#define SPA_AUDIO_SOUNDCHECK_LIMITER_ENV  "AUDIO_SOUNDCHECK_LIMITER"

/* ---- type-103 buffered-media negotiation probe (AUDIO.md §26, docs/HANDOFF_SPATIAL_AUDIO.md) -----
 * iOS 27 reaches multichannel over CarPlay through a SECOND, buffered stream, not the realtime one.
 * Two accessory-controlled prerequisites, both metadata:
 *   1. /info `audioFormats` gains an entry {type:103, audioType:"media",
 *      audioOutputFormats:0x400000, audioOutputFormatsExtended:[65]}. The legacy value is the single
 *      AAC-LC/44.1 compatibility bit required by iOS's buffered-engine repackager; advertising the
 *      broader 0x1440800 transport-family mask makes wired iOS retain both PCM and AAC families and
 *      reject the whole CarPlay audio-format object before either stream is set up. The extended
 *      canonical policy selects 65 (APAC/5.1) or 66 (APAC/5.1.2). An explicit secondary index 23
 *      adds AAC-LC/48k/stereo; iOS may switch between the two codecs inside the same buffered
 *      session. APAP field 2 announces the per-packet format index, but a changed inline magic
 *      cookie remains the decoder authority; successful decoded geometry/layout selects PCM processing. Legacy
 *      debug variables retain the older AAC/DD+ probe indices only when no AUDIO_SPATIAL controls
 *      are present.
 *   2. the control-SETUP response's `enabledFeatures` gains the token "mainBuffered", without which
 *      AirPlaySender never sets SupportsBufferedAudio and never opens `bufferStream`.
 * The hook supplies SETUP, control methods and APAP receive/decrypt, then loads the optional APAC
 * codec and renders PCM6/PCM8 through the existing DIO5 QSA handle. The private HTTP dispatcher is
 * exact-profile fingerprinted and uses the existing RTSP connection, so controlPort/6033 is not
 * needed. Policy variables must be exported before AirTunesServer_EnsureCreated so the dispatcher
 * is installed before the first control connection. */
#define SPA_103_EXTRA_INDEX_ENV "SPATIAL_103_EXTRA_INDEX"
#define SPA_103_MODE_INFO       1u  /* advertise the type-103 /info descriptor only */
#define SPA_103_MODE_FEATURE    2u  /* negotiate mainBuffered only (when proposed) */
#define SPA_103_MODE_BOTH       (SPA_103_MODE_INFO | SPA_103_MODE_FEATURE)
#define SPA_AAC_LC_48K_5_1_2 40      /* APAudioFormatIndex; AAC-LC/48000/5.1.2 */
#define SPA_AAC_ELD_48K_5_1_2 42     /* APAudioFormatIndex; AAC-ELD/48000/5.1.2 */
#define SPA_DDP_48K_5_1      50      /* APAudioFormatIndex; DDPLUS/48000/5.1 */
#define SPA_DDP_48K_9_1_6    53      /* APAudioFormatIndex; DDPLUS/48000/9.1.6 */
#define SPA_APAC_48K_STEREO 64      /* APAudioFormatIndex; APAC/48000/stereo. Same codec/rate as
                                     * 5.1; after cookie acceptance its 2ch geometry selects the
                                     * common stereo renderer before the PCM6 ring. iOS gates it
                                     * behind bufferedAPACStereo, whose static default is false, so
                                     * the phone may simply never offer it. */
#define SPA_APAC_48K_5_1    65      /* APAudioFormatIndex; APAC/48000/5.1 */
#define SPA_APAC_48K_5_1_2  66      /* APAudioFormatIndex; APAC/48000/5.1.2 */
#define SPA_ALAC_48K_24_STEREO 21   /* APAudioFormatIndex; ALAC/48000/24/stereo -- lossless and
                                     * already at the sink rate, so no resampler.  In the
                                     * buffered sender's set only under an extra condition. */
#define SPA_AAC_LC_48K_STEREO 23    /* APAudioFormatIndex; AAC-LC/48000/stereo -- in the set
                                     * whenever Atmos is allowed, and needs no resampler. */
#define SPA_AAC_LC_44K_STEREO 22    /* APAudioFormatIndex; AAC-LC/44100/stereo -- indices below
                                     * 64 map to 1<<index, so this is legacy bit 22, the one the
                                     * buffered sender prefers first for ordinary content. */
#define SPA_BUFFERED_AAC_MASK        0x400000   /* bit 22: AAC-LC/44100/2; valid type-103 codec family */

/* APReceiverAudioSessionBufferedCreate's single-format defaults.  AAC-LC and
 * AAC-ELD map to compression types 4/8 and get the smaller receiver buffer;
 * APAC remains compression type 0.  Our advertised [65,23] stream is different:
 * iOS changes codec inside the already-created type-103 connection, so its one
 * immutable buffer size must cover the largest format in the offer. */
#define SPA_103_AUDIO_BUFFER_SIZE_AAC   5000000
#define SPA_103_AUDIO_BUFFER_SIZE_OTHER 21168000

static inline int spa_type103_audio_buffer_size_for_index(int64_t formatIndex)
{
    return formatIndex == SPA_AAC_LC_44K_STEREO ||
           formatIndex == SPA_AAC_LC_48K_STEREO ||
           formatIndex == SPA_AAC_LC_48K_5_1_2 ||
           formatIndex == SPA_AAC_ELD_48K_5_1_2 ?
           SPA_103_AUDIO_BUFFER_SIZE_AAC : SPA_103_AUDIO_BUFFER_SIZE_OTHER;
}

static inline int spa_type103_audio_buffer_size_for_offer(int64_t selectedIndex,
                                                           int primaryIndex,
                                                           int extraIndex)
{
    int size = spa_type103_audio_buffer_size_for_index(selectedIndex);

    if (primaryIndex >= 0 &&
        spa_type103_audio_buffer_size_for_index(primaryIndex) > size)
        size = spa_type103_audio_buffer_size_for_index(primaryIndex);
    if (extraIndex >= 0 &&
        spa_type103_audio_buffer_size_for_index(extraIndex) > size)
        size = spa_type103_audio_buffer_size_for_index(extraIndex);
    return size;
}

/* Registered by the framework from hook_module_table (hook/main.c).  The
 * AirPlay SETUP/ /info/TearDown work this module needs is declared as seam
 * callbacks in that def and implemented statically in audio_hook.c; nothing
 * outside this module calls into the type-103 negotiation any more.  on_init is
 * lazy, never an ELF constructor: QNX must finish loading dio_manager before
 * the hook may inspect files, initialise logging or create a writer thread. */
extern const hook_module_def_t audio_module_def;

/* ---- the decoded type-103 PCM6 rail ----------------------------------------------------------
 * While type 103 carries buffered media, the receiver follows the inline cookie,
 * decodes APAC 5.1 or AAC stereo, applies the corresponding reorder/upmix action
 * and queues declared CarPlay order (L R C LFE Ls Rs) here. These two calls are
 * the seam the type-100 read path takes over through: when
 * spa_buffered_pcm6_active() is true it serves spa_buffered_pcm6_read() instead
 * of its own samples, and the instant it is false stock type-100 behaviour
 * resumes unchanged. That fallback is the whole safety story of this path.
 *
 * spa_buffered_pcm6_read() writes exactly `frames` * 6 interleaved S16 samples
 * and returns `frames`, or writes nothing and returns zero.  A decoded prefix
 * followed by a synthesized zero tail is forbidden: the caller can use its
 * realtime fallback for the whole indivisible QSA period.  Both calls are safe
 * from any thread. */
int spa_buffered_pcm6_active(void);
size_t spa_buffered_pcm6_read(int16_t *out, size_t frames);

/* Generic rail used by the sink: 6 channels for 5.1 and for Atmos
 * downmix/drop, 8 only for explicit passthrough. The read is all-or-zero and
 * also returns zero if the caller's expected channel count no longer matches
 * the active stream. */
int spa_buffered_pcm_active(void);
unsigned spa_buffered_pcm_channels(void);
size_t spa_buffered_pcm_read(int16_t *out, size_t frames, unsigned channels);

/* Transactional sink API. `peek` copies one whole period without consuming it;
 * `commit` advances only the prefix QSA accepted and releases the unwritten
 * tail. There may be only one outstanding peek; the QSA bridge serializes it
 * with its existing ioLock. The immediate `read` above remains for callers
 * which do not need device-acceptance tracking. */
size_t spa_buffered_pcm_peek(int16_t *out, size_t frames, unsigned channels);
size_t spa_buffered_pcm_commit(size_t accepted_frames, unsigned channels);

#endif /* AUDIO_HOOK_H */
