# PRISM artwork, classic waiting lifecycle — 2026-09-14

The final user request is PRISM's glass Apple and animated spectral lighting,
with the old fast waiting-only behavior. Entrance/exit video animations remain
removed. This supersedes the intermediate classic Apple/spinner build.

The loader owns its code, layout, shader and assets in `altscreen_render/loader/`.
The desktop preview includes that implementation; `common/` has no loader code.

## Production path

- One existing managed RGBA plane 99, at 1440×542.
- A 30 Hz thread draws PRISM only while waiting. The single artwork shader
  has no video sampler, zoom, or refraction of video.
- Each waiting frame clears the window to opaque black, then draws logo and
  signature rectangles together in one batch. Shader coverage is 2.40% of the
  full-layout panel, or 1.67% in compact layout. Resolution and artwork stay
  unchanged; these figures are area, not a measured GPU speedup.
- The first decoded picture stops and joins that thread before drawing. Later
  pictures go straight from their native-32m EGLImage to the window, one draw
  and swap each. No intermediate framebuffer, frame copy, or compositor queue.
- Normal decoder teardown restarts waiting artwork after releasing its buffers.
  A caught-up or temporarily quiet video stream does not restart the loader.

The restored video path retains per-picture external-image rebinding for valid
Qualcomm chroma, `glFinish` before buffer recycling, the 750 ms stop watchdog,
1000 ms input deadline, receipt-time diagnostics, and safe decoded-output
catch-up. Only unused waiting-frame time is slept; video receives no added timer.
No new plane/process/Java context or shell monitoring is needed for this change.

## Verification

Before deleting the old shader, the production waiting artwork matched the
original PRISM at stream mix zero within 1/255 per channel across four VC layouts
and six phases. The retired video/zoom branches are now physically removed.
`test_prism_waiting.c` compares full-panel and bounded draws of the single shader,
checks its six artwork uniforms, black safe-area margins and opaque output.

`altscreen_present_test.c` executes the actual presenter with GL/Screen stubs.
It verifies loader stop/join before video, no waiting redraw after first frame,
one draw per video picture, external-image refresh, GPU completion before post,
swap failure reporting, and reconnect. ASan/UBSan and QNX hostcheck pass.

The Docker ARM build is the link/ABI check. It cannot establish timing on the
physical Adreno 320; live HU validation of startup latency, decode cadence,
reconnect, and timestamp propagation is still required.

Latest candidate: `build/releases/altscreen-prism-cleanup-20260914/`.
The earlier `altscreen-prism-waiting-20260914` candidate is superseded.
Previous `altscreen-classic-loader-20260914` and `altscreen-latency-20260914`
candidates are superseded and must not be deployed.
