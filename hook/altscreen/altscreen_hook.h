/*
 * CarPlay AltScreen (instrument-cluster stream 111) — MHI2Q.
 *
 * dio-side `extendedFeatures = ["altScreen"]` injector. The VERIFIED activation gate
 * (iOS 26.1 sender decompile) is: car /info advertises extendedFeatures containing
 * "altScreen" -> phone fetches cluster URLs -> SETUP stream 111. This module supplies
 * that DECLARATION by interposing the libairplay export AirPlayReceiverServerSetProperty
 * (which dio calls at server bring-up) and injecting the extra property once. The 111
 * stream is received/decoded by our from-source libairplay (../../../libairplay-mhi2q);
 * physical cluster delivery is maneuver_render (libairplay screenstream SS_FWD_PORT_ALT).
 *
 * Always-on: injects exactly once, at the first server SetProperty (bring-up).
 * QNX: NO __thread (gcc 4.4 emutls trap) — globals + pthread_mutex throughout.
 */
#ifndef CARPLAY_ALTSCREEN_HOOK_H
#define CARPLAY_ALTSCREEN_HOOK_H

void altscreen_init(void);
void altscreen_shutdown(void);

#endif /* CARPLAY_ALTSCREEN_HOOK_H */
