# AltScreen PRISM startup loader

PRISM is the sole startup scene for the instrument-cluster renderer,
stream 111 / displayable 99. Its material, signature and shader are compiled
into `altscreen_render`; no external runtime artwork or font is required.

## Production behavior

1. The independent renderer opens its RGBA8888 window before RTSP/OMX video
   exists and renders the glass Apple from black at swap interval 2.
2. A decoded Qualcomm 32m frame is read through its refreshed external
   EGLImage into an owned RGBA cache. The callback waits for GPU read completion
   and a successful PRISM swap before releasing that exact OMX buffer.
3. PRISM zooms into the Apple and reveals video across all 1440 × 542 pixels.
   Its persistent compositor finishes the transition even if the phone sends
   only one frame. Settled video uses unchanged cache pixels.
4. Normal decoder teardown requests a reverse transition from the owned last
   frame to the waiting Apple. The initial contour is not replayed. The cache
   survives freed/recycled decoder buffers.
5. A full presenter destruction stops and joins the compositor before releasing
   its resources. Existing isolated-renderer timeout/restart behavior remains
   the fallback for a wedged GPU.

The resting loader follows the existing full/compact and Sport/Classic safe
areas. The stream always covers the complete 292 × 110 mm panel. The selected
SOFT `@LuKa_dev` signature is 122 × 18 px, approximately 24.74 × 3.65 mm.

## Ownership and scope

Stock `libairplay.so` 210.81 continues to own stream 110 and displayable 59.
The hook creates no main-HU loader, patches no stock loader/readiness GOT slots,
and does not gate stream 111 on stock readiness. Java does not force context 78
or alter displayable 59 opacity for this loader.

`common/prism_baked.h` holds production assets. Shared motion/layout and the
memory-only PNG decoder live in `common/prism*`. The production integration is
`altscreen_render/screen_egl_present.c` and `prism_queue.h`.
`scripts/build_renderers.sh` links PRISM only into `altscreen_render`.

The macOS demo includes a baked fictional map in its own header; that map is
absent from the QNX binary. See [PRISM build and verification instructions](../tools/loader_preview/PRISM.md).

## Validation boundary

Host motion, source-lifetime, GPU cache and viewport captures accompany the
QNX ARM build. Desktop captures preserve the approved appearance. Actual
Adreno 320 performance, physical-panel appearance, and real stream reconnects
require an on-unit run; cross-compilation cannot establish those results.
