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
 */
#ifndef AUDIO_HOOK_H
#define AUDIO_HOOK_H

#include <stdint.h>

/* Default AirPlayAudioFormat bit: 5.1 PCM (PCM/48000/16/5.1), from the iOS 27 descriptor table.
 * Which bit we actually advertise is RUNTIME-TUNABLE (see SPA_TUNE_PATH) so alternatives can be tried
 * without a reboot — the env var only reads once at supervisor start. See AUDIO.md sections 9-10, 24. */
#define SPA_DEFAULT_BIT   44u
#define SPA_FMT_5_1       ((uint64_t)1 << SPA_DEFAULT_BIT)

/* "<bit>" or "<bit>:<channels>", e.g. "44:6" (the default), "38:8", "47:6". Re-read on every call, so
 * the experiment loop is: echo 38 > /tmp/spatial_bit ; replug the phone ; read the log. Absent file =
 * defaults. Bit is clamped to 32..63 so a typo can never disturb the stock stereo/mono bits (2..25). */
#define SPA_TUNE_PATH     "/tmp/spatial_bit"

/* ---- type-103 buffered-media negotiation probe (AUDIO.md §26, docs/HANDOFF_SPATIAL_AUDIO.md) -----
 * iOS 27 reaches multichannel over CarPlay through a SECOND, buffered stream, not the realtime one.
 * Two accessory-controlled prerequisites, both metadata:
 *   1. /info `audioFormats` gains an entry {type:103, audioType:"media",
 *      audioOutputFormats:0x1440800, audioOutputFormatsExtended:[65]}  (65 = APAC/48000/5.1, the only
 *      index our 6-channel 48 kHz hardware could ever render).
 *   2. the control-SETUP response's `enabledFeatures` gains the token "mainBuffered", without which
 *      AirPlaySender never sets SupportsBufferedAudio and never opens `bufferStream`.
 * This is a NEGOTIATION PROBE, not playable audio: we have no type-103 receiver and no APAC decoder,
 * so if the phone actually starts the stream it must fail closed. Hence OFF by default and gated on a
 * marker file that can be removed and re-tested with a replug — no reboot. */
#define SPA_PROBE103_PATH   "/tmp/spatial_probe103"
#define SPA_APAC_48K_5_1    65      /* APAudioFormatIndex, NOT a bitmask bit */
#define SPA_BUFFERED_LEGACY_MASK  0x1440800  /* required companion mask: bits 11,18,22,24 */

/* Adds "mainBuffered" to the control-SETUP response's enabledFeatures, preserving whatever the
 * receiver already accepted. Called from the altscreen SessionSetup wrapper (the single owner of that
 * interposition) but deliberately NOT gated on altScreen being enabled. Returns 1 if it added the
 * token, 0 otherwise. No-op unless the probe marker exists. */
int spa_augment_setup_response(void *session, void *request, void *response);

#endif /* AUDIO_HOOK_H */
