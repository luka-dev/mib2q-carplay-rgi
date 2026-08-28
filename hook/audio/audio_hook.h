/*
 * audio_hook.h — CarPlay multichannel (Spatial 5.1 PCM) audio hook.
 *
 * Goal: make the iPhone push discrete 5.1 PCM over wired CarPlay to the B&O amp, by widening the
 * AirPlay audio-format negotiation from the stock stereo ceiling to 6-channel PCM.  See
 * docs/reference/AUDIO.md (sections 5-8) for the full plan.
 *
 * Strategy = LD_PRELOAD interposition of stock libairplay.so exports (all verified exported, T):
 *   - AudioSessionGetSupportedOutputFormats  (0x497bc, returns __int64) — advertise the 5.1 bit.
 *   - AirPlayAudioFormatToPCM                (0x2c308) — emit a 6-channel ASBD for our format.
 * The 6ch count then flows unpatched through AirPlayAudioPrepare -> CQNXSoundIF -> snd_pcm on
 * mpl5_dio_ent (channels come straight from the ASBD, verified 0x4a9d0). The dio/mixer side must
 * provision mpl5_dio_ent for 6 voices -> six_channel_speakerp (B&O firmware agent's dependency).
 *
 * spatialAudioSupported lives in CARSessionConfiguration (altScreen-class): it is injected from the
 * EXISTING altscreen AirPlayReceiverSessionSetup wrapper via spa_augment_setup_response() below — this
 * module must NOT define its own AirPlayReceiverSessionSetup (altscreen owns that symbol).
 *
 * OFF by default: enabled only when env SPATIAL_5_1 is truthy (value-checked). Until the exact
 * multichannel format bit is probe-confirmed on a live connect, SPA_FMT_5_1 stays 0 and every
 * augmentation is inert — the stock stereo path is untouched.
 */
#ifndef AUDIO_HOOK_H
#define AUDIO_HOOK_H

#include <stdint.h>

/* The AirPlayAudioFormat bit for 5.1 PCM (PCM/48000/16/5.1), extracted statically from iOS 27
 * AirPlaySupport. Shared so the altscreen SETUP wrapper can recognise the phone's 64-bit selection
 * before stock truncates it to 32-bit. See audio_hook.c / AUDIO.md sections 9-10. */
#define SPA_FMT_5_1   ((uint64_t)1 << 44)

/* Called from the altscreen SessionSetup wrapper (control SETUP only) to add "spatialAudio" to the
 * response enabledFeatures set. No-op unless the module is enabled. Returns 1 if it added anything.
 * Kept as a seam so we do not duplicate the AirPlayReceiverSessionSetup interposition. */
int spa_augment_setup_response(void *session, void *request, void *response);

/* SETUP-layer 5.1 bridge (AUDIO.md section 10). The altscreen wrapper owns the CF helpers and the
 * `request["streams"]` walk (alt_find_stream_type), so it does the detection + the pre-real rewrite
 * of the audio stream's `audioFormat` (bit 44 -> a stereo bit stock accepts), then calls
 * spa_set_want_5_1(1). Our AirPlayAudioFormatToPCM hook reads the flag and forces the 6ch ASBD, since
 * bit 44 is truncated to 0 in the 32-bit format arg and cannot be recognised there. Cleared on
 * teardown so alt/prompt streams stay stereo. */
void spa_set_want_5_1(int on);
int  spa_want_5_1(void);

#endif /* AUDIO_HOOK_H */
