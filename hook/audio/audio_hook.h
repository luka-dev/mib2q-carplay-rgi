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
 * OFF by default: enabled only when env SPATIAL_5_1 is truthy (value-checked). The 6-voice mixer side
 * (B&O agent) must be ready before arming, else the 6ch open has nowhere to land.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef AUDIO_HOOK_H
#define AUDIO_HOOK_H

#include <stddef.h>
#include <stdint.h>

#include "../framework/hook_framework.h"

/* Default AirPlayAudioFormat bit: 5.1 PCM (PCM/48000/16/5.1), from the iOS 27 descriptor table.
 * Which bit we actually advertise is RUNTIME-TUNABLE (see SPA_TUNE_PATH) so alternatives can be tried
 * without a reboot — the env var only reads once at supervisor start. See AUDIO.md sections 9-10, 24. */
#define SPA_DEFAULT_BIT   44u
#define SPA_FMT_5_1       ((uint64_t)1 << SPA_DEFAULT_BIT)

/* "<bit>" or "<bit>:<channels>", e.g. "44:6" (the default), "38:8", "47:6". Re-read on every call, so
 * the experiment loop is: echo 38 > /tmp/spatial_bit ; replug the phone ; read the log. Absent file =
 * defaults. Bit is clamped to 32..63 so a typo can never disturb the stock stereo/mono bits (2..25). */
#define SPA_TUNE_PATH     "/tmp/spatial_bit"

/* User-facing audio policy. Values are strict booleans:
 * 1/true/yes/on and 0/false/no/off (case-insensitive).
 * AUDIO_SPATAIL is accepted only as a warned compatibility alias for the
 * historically published typo; new configurations must use AUDIO_SPATIAL. */
#define SPA_AUDIO_ENV                 "AUDIO_SPATIAL"
#define SPA_AUDIO_TYPO_ENV            "AUDIO_SPATAIL"
#define SPA_AUDIO_ATMOS_ENV           "AUDIO_SPATIAL_ATMOS"
#define SPA_AUDIO_ATMOS_MODE_ENV      "AUDIO_SPATIAL_ATMOS_MODE"

/* ---- type-103 buffered-media negotiation probe (AUDIO.md §26, docs/HANDOFF_SPATIAL_AUDIO.md) -----
 * iOS 27 reaches multichannel over CarPlay through a SECOND, buffered stream, not the realtime one.
 * Two accessory-controlled prerequisites, both metadata:
 *   1. /info `audioFormats` gains an entry {type:103, audioType:"media",
 *      audioOutputFormats:0x400000, audioOutputFormatsExtended:[65]}. The legacy value is the single
 *      AAC-LC/44.1 compatibility bit required by iOS's buffered-engine repackager; advertising the
 *      broader 0x1440800 transport-family mask makes wired iOS retain both PCM and AAC families and
 *      reject the whole CarPlay audio-format object before either stream is set up. The extended
 *      canonical policy selects 65 (APAC/5.1) or 66 (APAC/5.1.2). Legacy debug variables retain
 *      the older AAC/DD+ probe indices only when no AUDIO_SPATIAL controls are present.
 *   2. the control-SETUP response's `enabledFeatures` gains the token "mainBuffered", without which
 *      AirPlaySender never sets SupportsBufferedAudio and never opens `bufferStream`.
 * The hook supplies SETUP, control methods and APAP receive/decrypt, then loads the optional APAC
 * codec and renders PCM6/PCM8 through the existing DIO5 QSA handle. The private HTTP dispatcher is
 * exact-profile fingerprinted and uses the existing RTSP connection, so controlPort/6033 is not
 * needed. Policy variables must be exported before AirTunesServer_EnsureCreated so the dispatcher
 * is installed before the first control connection. */
#define SPA_PROBE103_ENV   "SPATIAL_103"
#define SPA_103_INDEX_ENV  "SPATIAL_103_INDEX"
#define SPA_103_EXTRA_INDEX_ENV "SPATIAL_103_EXTRA_INDEX"
#define SPA_103_MODE_ENV   "SPATIAL_103_MODE"
#define SPA_103_DCX_ENV    "SPATIAL_103_DCX"
#define SPA_103_MASK_ENV   "SPATIAL_103_MASK"
#define SPA_103_MODE_INFO       1u  /* advertise the type-103 /info descriptor only */
#define SPA_103_MODE_FEATURE    2u  /* negotiate mainBuffered only (when proposed) */
#define SPA_103_MODE_BOTH       (SPA_103_MODE_INFO | SPA_103_MODE_FEATURE)
#define SPA_AAC_LC_48K_5_1_2 40      /* APAudioFormatIndex; AAC-LC/48000/5.1.2 */
#define SPA_AAC_ELD_48K_5_1_2 42     /* APAudioFormatIndex; AAC-ELD/48000/5.1.2 */
#define SPA_DDP_48K_5_1      50      /* APAudioFormatIndex; DDPLUS/48000/5.1 */
#define SPA_DDP_48K_9_1_6    53      /* APAudioFormatIndex; DDPLUS/48000/9.1.6 */
#define SPA_APAC_48K_5_1    65      /* APAudioFormatIndex; APAC/48000/5.1 */
#define SPA_APAC_48K_5_1_2  66      /* APAudioFormatIndex; APAC/48000/5.1.2 */
#define SPA_BUFFERED_AAC_MASK        0x400000   /* bit 22: AAC-LC/44100/2; valid type-103 codec family */
#define SPA_BUFFERED_BROAD_MASK      0x1440800  /* old failed probe: mixed bits 11,18,22,24 */

/* Registered by the framework from hook_module_table (hook/main.c).  The
 * AirPlay SETUP/ /info/TearDown work this module needs is declared as seam
 * callbacks in that def and implemented statically in audio_hook.c; nothing
 * outside this module calls into the type-103 negotiation any more.  on_init is
 * lazy, never an ELF constructor: QNX must finish loading dio_manager before
 * the hook may inspect files, initialise logging or create a writer thread. */
extern const hook_module_def_t audio_module_def;

/* ---- the decoded type-103 PCM6 rail (stage j) -----------------------------------------------
 * While a type-103 stream is carrying APAC media the receiver decodes it, reorders it into the
 * declared CarPlay order (L R C LFE Ls Rs) and queues it here.  These two calls are the seam the
 * type-100 read path takes over through: when spa_buffered_pcm6_active() is true it serves
 * spa_buffered_pcm6_read() instead of its own samples, and the instant it is false stock type-100
 * behaviour resumes unchanged.  That fallback is the whole safety story of this path.
 *
 * spa_buffered_pcm6_read() writes `frames` * 6 interleaved S16 samples at most and returns the
 * number of frames actually written; a short return is an underrun and the caller decides whether
 * to pad with silence or hold.  Both are safe to call from any thread. */
int spa_buffered_pcm6_active(void);
size_t spa_buffered_pcm6_read(int16_t *out, size_t frames);

/* Generic rail used by the sink: 6 channels for 5.1 and for Atmos
 * downmix/drop, 8 only for explicit passthrough. The read returns zero if the
 * caller's expected channel count no longer matches the active stream. */
int spa_buffered_pcm_active(void);
unsigned spa_buffered_pcm_channels(void);
size_t spa_buffered_pcm_read(int16_t *out, size_t frames, unsigned channels);

#endif /* AUDIO_HOOK_H */
