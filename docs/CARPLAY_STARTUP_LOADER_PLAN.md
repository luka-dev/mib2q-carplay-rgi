# AltScreen Startup Loader

Status: the startup loader is supported only for the instrument-cluster
altScreen renderer (stream 111 / displayable 99).

The attempted main-HU loader for stock stream 110 was removed. It did not
produce a reliable early image on MU1316 and added unsafe coupling between the
stock main decoder and the independent cluster decoder.

## Production behavior

- Stock `libairplay.so` 210.81 owns stream 110 and displayable 59 completely.
- The hook does not create, adopt or patch a `CScreenRender` for stream 110.
- No stock-210 GOT slots are rewritten for loader handoff or OMX readiness.
- Java does not force context 78 or toggle displayable 59 opacity for a loader.
- Stream 111 is not gated on stock stream 110 readiness.
- `altscreen_render` retains its early RGBA8888 loader on displayable 99 and
  replaces it with the first successfully presented decoded cluster frame.

This restores the normal stock lifecycle for the main CarPlay screen while
keeping the already-working, process-local cluster loader.

## Remaining loader implementation

- `common/carplay_loader.c/.h`: platform-neutral deterministic loader scene.
- `altscreen_render/screen_egl_present.c`: early RGBA window 99, loader thread,
  and first-decoded-frame takeover.
- `scripts/build_renderers.sh`: links the common loader only into
  `altscreen_render`.

The hook build intentionally does not link `common/carplay_loader.c` and has no
`main_loader.c/.h` module.

## Verification

1. With no stream 111 video yet, displayable 99 can show the local loader.
2. The first decoded and successfully swapped RGBA frame replaces that loader.
3. Stream 110 starts and tears down exclusively through stock code.
4. Opening or closing stream 110 does not disconnect, rearm or gate stream 111.
5. Hook and renderer QNX ARM builds contain no emutls symbols.
