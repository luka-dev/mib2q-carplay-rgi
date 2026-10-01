# Android Auto implementation

This directory contains the implementation tested on MU0918:

- `hook`: negotiates a cluster display, receives H.264 on Android Auto channel 64, acknowledges frames,
  sends cockpit input on channel 65 and presents the decoded viewport in QNX Screen window 99.
- `java`: subscribes Audi's Android Auto DSI to navigation events and connects those events to the
  cockpit/HUD renderer, route state, view state, steering-wheel input and options menu.
- `deploy`: launches the player and renderer beside GAL without replacing any stock executable.

The fixed native ABI seams are intentionally guarded by the firmware profile. Do not remove the hash
checks to try another firmware. Follow the porting guide and add a new reviewed profile.
