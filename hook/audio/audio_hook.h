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

/* The AirPlayAudioFormat bit for 5.1 PCM (PCM/48000/16/5.1), extracted statically from iOS 27
 * AirPlaySupport. See audio_hook.c / AUDIO.md sections 9-10. */
#define SPA_FMT_5_1   ((uint64_t)1 << 44)

/* Optional seam: add "spatialAudio" (head-tracked UX flag) to the control-SETUP enabledFeatures from
 * the existing altscreen SessionSetup wrapper, without a second AirPlayReceiverSessionSetup
 * interposition. No-op today; discrete 5.1 PCM does not depend on it. */
int spa_augment_setup_response(void *session, void *request, void *response);

#endif /* AUDIO_HOOK_H */
