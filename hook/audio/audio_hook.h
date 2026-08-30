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
 *      audioOutputFormats:0x400000, audioOutputFormatsExtended:[65]}. The legacy value is the single
 *      AAC-LC/44.1 compatibility bit required by iOS's buffered-engine repackager; advertising the
 *      broader 0x1440800 transport-family mask makes wired iOS retain both PCM and AAC families and
 *      reject the whole CarPlay audio-format object before either stream is set up. The extended
 *      index is runtime tunable to 40 (AAC-LC/5.1.2), 42 (AAC-ELD/5.1.2), 50 (DD+/5.1),
 *      53 (DD+/9.1.6), 65 (APAC/5.1), or 66 (APAC/5.1.2); only decoded PCM6 can be sent directly
 *      to the HU, so larger layouts require a deliberate downmix.
 *   2. the control-SETUP response's `enabledFeatures` gains the token "mainBuffered", without which
 *      AirPlaySender never sets SupportsBufferedAudio and never opens `bufferStream`.
 * This remains a PROBE, not playable audio: the hook now supplies the complete legacy type-103
 * SETUP descriptor (listener port, buffer size, latency and dynamic stream ID), captures/drains
 * APAP, and receives the asynchronous SETMAGICCOOKIE binary plist on the existing RTSP connection.
 * The latter is installed through an exact MU1316 HTTPServer callback fingerprint; it does not need
 * controlPort/6033. There is still no APAC/AAC render path. Hence OFF by default and gated on a
 * marker file that can be removed and re-tested with a replug — no reboot. */
#define SPA_PROBE103_PATH   "/tmp/spatial_probe103"
#define SPA_103_INDEX_PATH  "/tmp/spatial_103_index"
#define SPA_103_MODE_PATH   "/tmp/spatial_103_mode"
#define SPA_103_DCX_PATH    "/tmp/spatial_103_dcx"
#define SPA_103_MASK_PATH   "/tmp/spatial_103_mask"
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

/* Called by the single SessionSetup interposer before stock libairplay. When the request contains
 * stream type 103 and the probe marker is present, records the stream fields and writes the complete
 * request to a mode-0600 binary plist in /tmp. Returns 1 iff a type-103 request was found. */
int spa_inspect_setup_request(void *request, uint32_t sequence);

/* Records what stock 210 did with the type-103 request and, if it unexpectedly returned a response,
 * writes that response beside the request. No-op when has103 == 0. */
void spa_note_setup_result(int has103, int result, void *response, uint32_t sequence);

/* Supply the missing legacy type-103 SETUP response, including AirPlaySender's mandatory
 * audioBufferSize field, install the fingerprint-locked SETMAGICCOOKIE request wrapper, and start a
 * bounded APAP capture/drain receiver before the response is returned. Codec decode/render is a
 * later stage. Returns an OSStatus-style error. */
int spa_setup_buffered_receiver(void *session, void *request, void *response, uint32_t sequence);

/* Stop the capture receiver for a per-stream type-103 or full-session teardown. */
void spa_teardown_buffered_receiver(void *session, void *request);

/* Captures the control request, then adds "mainBuffered" to enabledFeatures only in feature/both
 * mode AND only when the phone proposed that exact token. Preserves whatever the receiver already
 * accepted. Called from the altscreen SessionSetup wrapper (the single owner of that interposition),
 * but deliberately not gated on altScreen. Returns 1 iff it added the token. */
int spa_augment_setup_response(void *session, void *request, void *response, uint32_t sequence);

/* Capture the final control response after every hook has finished merging enabledFeatures. */
void spa_capture_setup_response(void *request, void *response, uint32_t sequence);

/* Fulfil AirPlaySender's strict feature/info pairing: after this session negotiated mainBuffered,
 * add the required top-level /info `mainBufferedInfo` dictionary. It stays empty by default. The
 * probe-only /tmp/spatial_103_dcx marker adds a real CFBoolean `DCXEnabled=true` so the Dolby/DCX
 * branch can be A/B tested without rebuilding; removing the master marker still disables all 103
 * mutations. */
void spa_augment_server_info(void *session, void *info);

/* Capture the final /info dictionary after all stock and hook-owned mutations. */
void spa_capture_server_info(void *info);

#endif /* AUDIO_HOOK_H */
