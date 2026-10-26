# Android Auto implementation

This directory contains the implementation tested on MU0918 and the profile-specific Java port for
MU1316 P5145:

- `hook`: negotiates a cluster display, receives H.264 on Android Auto channel 64, acknowledges frames,
  sends cockpit input on channel 65 and presents the decoded viewport in QNX Screen window 99.
- `java`: subscribes Audi's Android Auto DSI to navigation events and connects those events to the
  cockpit/HUD renderer, route state, view state, steering-wheel input and options menu.
- `deploy`: launches the player and renderer beside GAL without replacing any stock executable.

Shared Java sources live under `java/src`; a firmware directory under `java/firmware/<profile-id>/src`
overrides only classes whose stock implementation differs. The build checks the resulting 13 replaced
classes against that profile's stock JAR.

The fixed native ABI seams are intentionally guarded by the firmware profile. MU1316 remains
non-installable until its exact stock `gal` and `libautoreceiver.so` are available and reviewed. Do not
remove the hash checks to try another firmware.
