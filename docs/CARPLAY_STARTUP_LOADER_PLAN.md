# PRISM startup presentation

Integrated into canonical master on 2026-09-14 (`7b6cc45`): the altScreen PRISM
implementation from `95b7709` shares its GPU scene with the new HU cursor process. See [HU startup](reference/HU_STARTUP_LOADER_AND_MENU.md) and
[cursor ownership/protocol](reference/HU_TOUCH_CURSOR_EXPERIMENT.md).

## Shared rendering

`common/prism_gl.c/.h` owns context-local shader/material/signature resources and
scene drawing. `common/prism.c/.h` supplies image decoding and layouts;
`common/prism_baked.h` embeds the shader/assets. `common/prism_handoff.h` is the
shared reversible critically damped transition. The previous CPU
`carplay_loader.c/.h` and `loader_assets.h` are removed.

The shared scene owns no windows, EGL lifecycle, video queues or threads.
Its callers retain those responsibilities:

- `altscreen_render/screen_egl_present.c`: cluster window 99, PRISM and the
  owned decoded-frame cache. The existing queue/handoff returns OMX buffers through
  their proper owner. Cluster layouts retain their safe visible rectangles.
- `cursor_render/`: independent HU managed window 103 between stock HMI 16 and
  main video 59. Its native engine owns pointer idle/fade and PRISM animation.
  HU PRISM uses the full canvas; an alpha aperture reveals the existing video
  surface without importing or modifying that video.

Stock `libairplay.so` owns stream 110/displayable 59. The hook only observes successful
main config/render; it creates no HU window and paints no loader/cursor into decoder
buffers. Stream 111 remains independent of stream 110 readiness. Java coordinates
HMI/menu, input state and observed readiness; it computes no cursor/loader opacity
or animation completion.

## Local validation

- `scripts/build_renderers.sh`: all three ARM executables, no emutls.
- `make -C altscreen_render hostcheck`: shared GPU scene and platform syntax.
- `make -C tools/loader_preview check check-bake`: PRISM transition, queue,
  owned GPU cache and reproducible baked assets.
- `scripts/test_hu_cursor.sh`: Java input/transport, native engine/session/process,
  independent fade, protocol v2, GPU premultiplied alpha and stale-buffer cleanup.
- `scripts/test_hu_startup.sh`: stock class replacement, HMI ownership,
  native completion/ACK click deadlines, video observer and session isolation.

This experiment is built/tested on the host. HU compositor adoption, physical
layering, transition timing and GPU cost still require a matched hardware deployment.
