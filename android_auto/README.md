# Android Auto implementation

This directory contains the shared Android Auto implementation plus firmware profiles. MU0918 is
validated on-car, MU1316 P5145 has a profile-specific Java port awaiting native verification, and
additional Audi MHI2Q versions can be added as their exact files are collected, audited and tested:

- `hook`: negotiates a cluster display, receives H.264 on Android Auto channel 64, acknowledges frames,
  sends cockpit input on channel 65 and presents the decoded viewport in QNX Screen window 99.
- `java`: subscribes Audi's Android Auto DSI to navigation events and connects those events to the
  cockpit/HUD renderer, route state, view state, steering-wheel input and options menu.
- `deploy`: launches the player and renderer beside GAL without replacing any stock executable.

Shared Java sources live under `java/src`; a firmware directory under `java/firmware/<profile-id>/src`
overrides only classes whose stock implementation differs. The build checks the resulting 13 replaced
classes against that profile's stock JAR.

The fixed native ABI seams are intentionally guarded by the firmware profile. Every firmware we later
support receives exact hashes and any required Java/native overrides; the shared stream, decoder,
cockpit and HUD implementation stays common. MU1316 remains non-installable until its exact stock
`gal` and `libautoreceiver.so` are available and reviewed. Unknown or incomplete profiles are refused.
Do not remove the hash checks to try another firmware.
