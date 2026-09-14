> Current candidate: [PRISM waiting only](ALTSCREEN_PRISM_WAITING_2026_09_14.md).
> PRISM artwork is retained; persistent video compositing and transitions are removed.

# altScreen delayed startup and repeated PRISM/video transitions

Status: **the PRISM candidate below is superseded; do not deploy it**. The user
cancelled the subsequent two-layer/process approach and requested the old simple
loader with immediate video takeover. See
[current implementation](ALTSCREEN_PRISM_WAITING_2026_09_14.md).
Both `100.127.17.86` and Audi AP `10.173.189.1` stopped responding to SSH during
this investigation. The following records the preceding investigation.

## Evidence

The user observed approximately 30 seconds of black output, repeated transitions
between PRISM and video, and badly delayed pictures. The captured
`build/hu-log-review-20260914/altscreen_render.log` contains three input-buffer
timeouts followed by transport reconnects/decoder generations 1–4. Wrapper logs
also show two renderer process restarts. Teardown OMX errors followed the input
timeouts; they do not establish the original cause.

Later logs reached 1800 posted pictures with approximately 64 ms interarrival and
2 ms average swap time. These counters prove activity, **not picture freshness**.
The original queue retained up to 1024 compressed records / 24 MiB and carried no
receipt timestamp into OMX. Its maximum size remains a memory safety bound.

## Candidate corrections

### Verified architectural regression boundary

Comparison with `7fce1c4^` identifies changes beyond timeout policy:

- Before `7fce1c4`, the first `screen_egl_present_frame()` explicitly called
  `screen_egl_present_stop_loader()` and joined its thread. Decoded external
  images were subsequently drawn straight into the display framebuffer and
  swapped from the OMX callback. The loader no longer participated in video.
- `7fce1c4` keeps a compositor thread alive for the smooth reversible PRISM
  handoff. Each OMX callback submits to `prism_queue` and waits for that thread
  to copy the native source into an owned RGBA FBO, run PRISM and swap, before
  `prism_queue_complete` releases the callback. This adds a rendezvous and an
  extra full-frame GPU pass. The owned cache enables animation between sparse
  video frames, but ties output-buffer recycling to compositor performance.
- Once handoff reaches exactly 1, the PRISM fragment shader already has a cheap
  texture passthrough branch and the compositor idles without new pictures.
  It does not continuously run the full glass effect on settled static video;
  however the cache copy and thread rendezvous remain for every new picture.
- Both the old swap-only pacing assumption and 500 ms input wait predate this
  change. They are existing constraints exposed by the new video/compositor
  coupling, not evidence that a newly added 500 ms timeout alone caused the bug.
- `glFinish` and per-frame external-image rebinding also existed in the direct
  path and must not be mistaken for newly introduced overhead or removed from
  source-lifetime protection.

This is a verified code-path comparison. How much each added stage contributes
on Adreno 320, including the first black-screen delay, still needs HU timings.

### Historical intermediate candidate (superseded)

- The compositor previously relied exclusively on swap interval 2 for pacing,
  immediately reacquiring `gl_lock` during PRISM/reveal. A fast-returning managed
  window could monopolize this lock and GPU submissions. The compositor now
  waits only the unused part of a 33.33 ms frame budget, outside `gl_lock`.
  Explicit maintenance priority lets native source attachment/output recovery
  acquire EGL even when rendering itself exceeds the frame budget. Driver
  hangs remain subject to existing isolated-process watchdogs.
- Input previously failed after 500 ms although the output callback's permitted
  compositor rendezvous is 750 ms. The input deadline is now a shared-policy
  1000 ms, established once per wait; spurious notifications cannot prolong it.
  GPU source ownership and the 750 ms post timeout remain unchanged.
- Each compressed record now retains local monotonic socket receipt time, passed
  through OMX's microsecond `nTimeStamp`. A decoded picture older than 200 ms may
  skip GPU presentation **only when a newer picture has already entered OMX**.
  Every compressed picture is still decoded in order, preserving H.264 reference
  chains. Skipping recycles decoded storage and does not set VIDEO_OFF, reset the
  decoder, or restart PRISM. A final/static picture still gets presented even if
  it is old. Missing/future timestamps fall back to ordinary presentation.
- Logs now identify Screen/EGL open duration, source-attachment lock wait,
  decoder startup phases, compressed queue age, decoded picture age/backlog,
  skipped output count and slow compositor frames with cache/draw/swap timings.

No hook-wire, Java, supervisor or shared PRISM shader changes are required for
this candidate. Existing native-32m window usage, per-frame UV rebinding and
`glFinish` before source recycling are preserved.

## Validation and limits

- Actual sink and queue tests pass with ASan/UBSan. Cases cover config/IDR gates,
  rejected/oversized inputs, bounded startup retries, replacement generations,
  a healthy 600 ms EmptyDone delay, a genuinely stalled input despite repeated
  notifications, stale-output recycling, unchanged video readiness, and final/
  missing-timestamp pictures.
- Timing tests cover fast-returning and blocking swaps, live cadence, old queue
  with a small OMX pool, final static picture and invalid timestamps.
- Existing PRISM rendezvous, reversible handoff and GPU cache/orientation tests
  pass. QNX source hostcheck and Docker ARM build pass; no emutls dependency.
- Tests do not emulate the Qualcomm decoder or prove its timestamp propagation.
  `FillBufferDone ... pts=` must be nonzero and meaningful on the target; zero
  disables catch-up safely. The 200 ms threshold is not a guaranteed latency cap.
  Measured age starts at the local receiver socket, excluding any earlier phone/
  hook/network delay. The exact location of the observed 30-second startup delay
  remains unconfirmed until the new phase timings are captured on HU.

Next: use the current PRISM-waiting candidate linked above, verify its readback SHA-256,
restart that renderer through the existing lifecycle, and capture a fresh
connection through settled video. Check startup phase times, generation stability,
nonzero output timestamps and falling queue/picture age; request visual feedback.
Do not change the cheap QNX shell monitor to diagnose this problem.
