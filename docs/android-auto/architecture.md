# Architecture

## One Android Auto session, two displays

The head unit keeps one Android Auto connection. The stock centre display remains the primary video
sink. The preload hook advertises another video sink and input service; the phone assigns video channel
64 and input channel 65 to the cockpit display.

The hook requests Android Auto protocol 4.3. Newer phone responses are rewritten to the older version
understood by the 2016 GAL receiver after the hook has consumed the newer behavior. This enables safe
mid-session `0x8009` layout updates without replacing GAL or `libautoreceiver.so`.

## Video path

```text
Android phone
  -> encrypted Android Auto transport
  -> GAL/libautoreceiver hook
  -> bounded shared-memory encoded-frame ring
  -> cluster-player
       -> Qualcomm OMX H.264 decoder
       -> Qualcomm 64x32 tiled NV12 de-tiler
       -> 1:1 YUV-to-RGB presenter
  -> QNX Screen window 99
  -> MHI2Q Virtual Cockpit context
```

The player falls back to a narrowly configured FFmpeg H.264 decoder when OMX cannot start or stops
producing frames. It never reuses or moves the stock centre-screen window.

## Viewport and layouts

The phone receives a 1920x1080 display request with a 1440x540 logical viewport. The player displays
the visible 1440x455 section at 1:1 scale. Large, Classic and Sport states come from the HMI view-size
and B9/B9Sport layout state written by the Java bridge.

The hook sends layout message `0x8009` on a view or menu change. Insets are expressed in the full phone
frame coordinate system. The validated base presets are maintained in `hook/src/uiconfig.c`.

## Route data and HUD

Audi's stock MU0918 Android Auto listener does not subscribe to next-turn attributes. The Java overlay
adds those subscriptions, converts the resulting DSI events into the existing LuKa route-guidance
model, and keeps one BAP writer for the Virtual Cockpit and HUD. This path is independent of cluster
video, so HUD guidance continues if video decoding falls back or is disabled.

## Steering-wheel input

The Java side owns the cockpit menu and route state. The native hook maps the navigation roller to the
cluster input channel. Map zoom changes the phone's safe area and sends `0x8009`; Google redraws the map
at a new scale instead of the player enlarging decoded pixels.
