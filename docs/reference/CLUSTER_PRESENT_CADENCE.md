# Cluster present cadence — why altscreen_render juddered at a nominal 30 fps

Resolved 2026-08-30. Static reading of our own two renderers plus the QNX/Screen prior art in
`Tools/retroarch-qnx`. Reported symptom: the CarPlay cluster video "lags a bit, the animations do not
look like 30 fps", while `maneuver_render` on the same panel felt smooth.

## The chain

```
phone 30 fps  →  OMX AVC decode  →  post into a Screen window the HU composites at 60 Hz
              →  LVDS capture of that window at a fixed 30 Hz  →  VC
```

Three clocks, none locked to each other. Every stage is nominally 30 fps, and it still beats.

## The mechanism

`altscreen_render` posted with `eglSwapInterval(1)` on the 60 Hz surface, driven by frame arrival.
A posted frame therefore occupied **1, 2 or 3 scans** depending on the phase at which it landed. The
downstream capture samples on its own fixed 30 Hz grid, so the two unlocked 30 Hz sequences slid
against each other: periodically the capture took one frame twice and skipped the next. That is the
judder, and no stage is dropping frames for it to happen.

Interval 2 pins each post to exactly two scans = exactly one capture period, so the capture takes one
distinct frame per period at any phase.

## The in-tree A/B that settled it

| | swap interval | RGBA buffers | pacing | felt |
|---|---|---|---|---|
| `maneuver_render` | **2** (`platform_qnx.c:345`) | 2 | the swap itself | smooth |
| `altscreen_render` (before) | **1** (`screen_egl_present.c`) | 2 | frame arrival | juddery |

Same hardware, same panel, same buffer count. The only difference was the interval — which is why the
fix is the interval and not the buffer count.

`maneuver_render/main.c:838` had already written the rule down:

> ACTIVE 30 FPS pacing rides eglSwapBuffers (QNX: `eglSwapInterval(2)`) — **NEVER** [a timer]

`altscreen_render` violated both halves: interval 1, and a software timer pacing the loader.

## Prior art: `Tools/retroarch-qnx/src/gfx/drivers_context/qnx_ctx.c`

That port did the platform legwork on this exact Screen/Adreno stack. Transferable findings:

- **`:425-432`** — the native window carries its own `SCREEN_PROPERTY_SWAP_INTERVAL` (**45**), and
  this firmware accepts `eglSwapInterval()` *without reliably updating it*. Set both sides, and set
  the Screen side **before `eglCreateWindowSurface`** or the subdriver snapshots libdisplayinit's
  default of 1.
- **`:349-360`** — `display_create_window()` yields a **two-buffer** window; on this stack interval 0
  alone does not make it asynchronous, because with the compositor holding one buffer and GLES
  rendering into the other there is no third to dequeue, so the swap still waits for scanout.
  Relevant if we ever go to interval 0.
- **`:709-713`** — creating the EGL surface while the *old* OEM display context is still active makes
  this Adreno stack throttle the first GL submission of every frame to the old compositor cadence
  (~30 Hz) even at interval 0. Worth remembering: our cluster context routing (`dc[80]`/`dc[81]`) is
  driven from Java by `AltScreenModule.republish()`, asynchronously, so its order relative to our
  `eglCreateWindowSurface` is not guaranteed.
- **`:193-252`** — bounded explicit vsync: `screen_wait_vsync()` dlsym'd from
  `/proc/boot/libscreen.so.1`, wrapped in `TimerTimeout(CLOCK_MONOTONIC,
  _NTO_TIMEOUT_SEND|_NTO_TIMEOUT_REPLY, …, 25 ms)` so a missing display-manager ack costs one frame
  rather than freezing the thread, with a monotonic software pacer as fallback.
- Reverse-engineered Screen property ids: `BUFFER_COUNT=4`, `DISPLAY=11`, `SWAP_INTERVAL=45`,
  `RENDER_BUFFER_COUNT=53`, `DISPLAY_COUNT=59`, `DISPLAYS=60`, `CONTEXT=95`.

We do **not** need the `screen_wait_vsync` machinery: RetroArch wanted a free-running 60 Hz emulator,
we want a fixed 30 fps that the driver's own interval already produces.

## What was changed (commit `aae23c2`, `altscreen_render` only)

- `EP_SWAP_INTERVAL 2`, applied in `ep_init_egl` **and** in `screen_egl_present_recover_output` —
  the surface-recovery path still carried a separate hardcoded `1`.
- `ep_set_native_swap_interval()` sets Screen property 45 before `eglCreateWindowSurface`, with a
  readback logged. Non-fatal on failure since the EGL side is applied regardless.
- **Removed the loader's software deadline pacer.** It quantised loader posts onto a 33.33 ms grid
  unrelated to scanout — the same beat, which is why the boot spinner stuttered too. Under interval 2
  it would additionally have re-armed *after* the swap had already consumed the period and slept a
  second one, halving the loader to 15 fps.
- Cadence probe in the existing 600-frame log: mean and max **arrival** spacing (how fast decoded
  frames reach us) and mean and max **swap** duration, plus the interval in force.

## Reading the probe on the next session

```
arrival ≈ 33.3   swap ≈ 33.3   → correct; 30 fps end to end
arrival ≈ 33.3   swap ≈ 16.7   → interval 2 did not take; check the Screen property readback line
arrival ≫ 33.3                 → the phone is not sending 30; not a present-path problem
swap max ≈ 66                  → a scan was missed; that is when the items below become real
```

## Deliberately not done

The present still runs synchronously inside the OMX `FillBufferDone` callback: draw → `glFinish()` →
swap → `OMX_FillThisBuffer`. At interval 2 that blocks the decoder's own thread for up to 33 ms, the
whole frame budget, and the RGBA window has only two buffers so a late frame has no elasticity. The
fix would be a present thread with a 1-deep drop-oldest queue (recycling the OMX buffer from that
thread, which is legal — `OMX_FillThisBuffer` need not be called from the callback) plus a third RGBA
buffer, and `glFinish()` replaced by an EGL fence if `EGL_KHR_fence_sync` exists on this driver.

None of it was done because `maneuver_render` is smooth on two buffers with a blocking swap, so it is
speculation until `swap max` shows dropped scans. `glFinish()` in particular must not simply be
deleted: it is what guarantees the GPU has finished sampling the tiled buffer before it goes back to
the decoder.

`maneuver_render` was left untouched — it was already correct.
