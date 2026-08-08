# CarPlay Immediate Startup Loader Plan

Status: implementation in `feature/carplay-startup-loader`, 2026-08-08.

Implemented off-unit:

- common deterministic 30-fps scene plus host RGBA tests;
- early reusable AltScreen RGBA window 99 and first-decoded-frame takeover;
- early stock `CScreenRender` window 59 with a layout-independent 32m
  breathing fill;
- verified stock-210 internal GOT adoption at `0xb97ac`, `0xb9d30` and
  `0xb9ca4`, including original-target validation, commit-last create patching,
  strict-config fallback, reconnect rearm and GOT restoration on shutdown;
- QNX ARM builds for hook, `altscreen_render` and `maneuver_render`, all with
  `emutls=0`.

Still requires on-unit validation before merge: both early windows, the
same-object main takeover, rapid reconnect and teardown ownership. The full
spinner on window 59 remains Phase B/C work because arbitrary shapes require
an APQ8064 32m tile-address mapper; the initial solid breathing fill is safe
without one.

## Goal

Make both CarPlay displays react immediately when the system detects a phone:

1. Java selects the already-declared stock CarPlay contexts.
2. A locally generated loading animation is already visible on both displays.
3. iPhone pairing, RTSP negotiation, stream setup and decoder startup continue in the background.
4. The first real CarPlay frame replaces the loader without destroying or recreating the visible window.

This changes perceived startup latency. It does not attempt to shorten or bypass the CarPlay protocol.

## Deployment constraints

- Production remains `LD_PRELOAD=libcarplay_hook.so` over stock `libairplay.so` 210.81 inside `dio_manager`.
- The cluster video renderer remains the separate `altscreen_render` process.
- Do not deploy the experimental from-source `libairplay-mhi2q`; it is not an authority for stock behavior.
- Do not introduce new Java display contexts. Reuse the existing compositions:
  - main CarPlay: `dc[78] = {16,59}`;
  - cluster map: `dc[81] = {99}`;
  - cluster map plus maneuver: `dc[80] = {98,101,102,99}`.
- The main and cluster loaders generate their frames locally. No cross-process frame transport is required.
- QNX 6.5 ARMv7: no TLS/`__thread`, no emutls, no GNU-only runtime assumptions.
- Calls made internally by stock `libairplay` bypass ordinary `LD_PRELOAD` symbol interposition. Internal `CScreenRender` adoption therefore requires a verified in-memory GOT or call-site patch.

## Target user-visible sequence

```text
startup supervisor
    ├── starts maneuver_render
    ├── starts altscreen_render → prepares RGBA window 99 + local loader
    └── execs dio_manager with hook
             └── hook prepares stock-format tiled window 59 + local loader

phone detected by Java
    ├── main context 78 becomes visible → loader already present on 59
    └── cluster context 81 becomes visible → loader already present on 99

iPhone negotiates in background
    ├── stock main stream 110 starts → decoder adopts existing window/buffers 59
    └── alt stream 111 starts → first decoded RGBA swap replaces loader on 99
```

No loading-specific Java context is needed. A prepared window is harmless while its displayable is not selected by the active context.

`AltScreenModule.start()` already publishes `connected=true` and context 81
before the renderer reports `VIDEO_ON`. Keep that distinction: the loader makes
the selected context responsive, while `VIDEO_ON` remains proof that a decoded
RGBA frame actually swapped.

## Shared generator

Implemented now:

- `common/carplay_loader.h`
- `common/carplay_loader.c`
- `tests/carplay_loader_test.c`

The shared code owns only scene and animation logic:

- dark vertical gradient;
- breathing center mark;
- rotating twelve-dot comet indicator;
- resolution-independent integer geometry;
- exact target rate: 30 fps (`33,333,333 ns`);
- no allocation, `libm`, platform API or thread-local storage.

The primary API is `carplay_loader_render(canvas, elapsed_ms)`. A consumer provides:

- `clear`;
- `fill_rect`;
- `fill_circle`.

The common module includes a reference byte-ordered RGBA8888 backend. Qualcomm 32m support is a separate backend because it requires hardware-specific tiled address calculation.

```text
common scene
    ├── RGBA8888 backend       → altscreen_render → displayable 99
    └── Qualcomm 32m backend   → dio hook         → displayable 59
```

Both consumers use the same 30 fps cadence. Window 99 renders the complete
common scene. Window 59 initially renders the same dark/breathing visual
language as a constant-plane 32m frame; it can render the complete common
scene only after the APQ8064 tile mapper is validated on-unit.

## Cluster path: RGBA displayable 99

### Current mismatch

`altscreen_render` starts before `dio_manager`, but `av_sink_open()` currently creates no window. `screen_egl_present_create()` is called only after the H.264 decoder reports its output-buffer count. That function creates all of the following together:

- managed RGBA output window 99;
- native Qualcomm 32m source buffers;
- EGL context/surface;
- EGLImage imports for decoder output.

Consequently Java can switch to context 81 immediately, but displayable 99 does not exist until video decoding begins.

### Required refactor

Split the presenter lifecycle into two stages:

1. **Output preparation at process startup**
   - create the managed RGBA8888 window 99 at 1440×542;
   - create its EGL surface/context;
   - start a 30 fps loader loop using the common RGBA scene;
   - do not allocate or start an OMX decoder.
2. **Decoder-source attachment after stream configuration**
   - keep the existing RGBA window and EGL surface;
   - create only the required Qualcomm 32m source buffers once OMX reports the count;
   - import those sources as EGLImages;
   - stop the loader under the existing GL mutex;
   - present the first decoded frame into the existing RGBA window.

The first successful decoded swap remains the data-plane `VIDEO_ON` truth. Loader visibility must not be reported as decoded video availability.

### Threading

The server thread blocks in `accept()` and frame reads, so it cannot maintain a 30 fps animation itself. Use one small loader thread owned by the presenter:

- it locks the existing GL/presenter mutex;
- makes the output context current;
- draws one loader frame and swaps;
- releases the context and sleeps to an absolute 30 fps deadline;
- exits before the first decoded presentation uses the same context.

No TLS is needed. Shutdown and output recovery must join the loader thread before destroying the EGL surface.

## Main path: stock tiled displayable 59

### Verified stock configuration

The main stock 210 path uses:

| Property | Stock value |
|---|---:|
| Visible size | 1024×480 |
| Screen format | Qualcomm tiled NV12 `0x1000C` |
| OMX color format | Qualcomm 32m `0x7F000004` |
| Displayable ID | 59 |
| Requested Screen usage | `0x484` |
| Output-buffer count | 12 |
| Observed stride | 1024 |
| Observed UV offset | `0x78000` |
| Observed buffer capacity | `0xB9000` |

Relevant stock 210 flow:

- `OmxVideoImpl::initialize @ 0x8c638` calls `CScreenRender::create` at `0x8c698`.
- `OmxVideoImpl::configure` calls `CScreenRender::config` at `0x8c540`
  (the return value is tested at `0x8c544`).
- `CScreenRender::config @ 0x8bcc8` normally creates a new context, window and buffer pool.
- `COMXVideoDecoder::setOutputBuffers @ 0x86c4c` obtains the existing Screen pointers through `CScreenRender::sharedBuffers @ 0x8b9e0`.
- The decoder can therefore attach OMX output headers to buffers that already belong to the adopted `CScreenRender`.
- `OmxVideoImpl::stop @ 0x8ca54` normally stops and deletes its render object.

### Required early preparation

During `dio_manager` initialization, before any phone session:

1. Resolve the original stock C++ methods and verify their ABI.
2. Create one stock `CScreenRender` object.
3. Configure it exactly as stock main video: 1024×480, ID 59, format `0x1000C`, usage `0x484`, 12 buffers.
4. Obtain its shared buffer pointers.
5. Start the 30 fps Qualcomm-32m loader loop and post frames through the stock render object.

Prefer starting preparation from a verified early `dio_manager → libairplay` boundary after Screen registration, instead of doing heavy Screen work synchronously in the ELF constructor. The ELF constructor installs internal patches and starts only bounded/lazy initialization.

### Internal adoption patch

Ordinary preloading is insufficient because the relevant calls originate inside `libairplay`.

Before the first session, install verified in-memory patches for the internal calls that reach:

1. `CScreenRender::create`
   - for the intended main `OmxVideoImpl` only, return the prepared loader render object;
   - never substitute the object for unrelated Screen users.
2. `CScreenRender::config`
   - if the object and all requested parameters exactly match the prepared configuration, return success without creating another window or buffer pool;
   - otherwise execute the original function.

The exact GOT slots or call-site bytes must be recomputed from the shipped stock 210 binary before implementation. Do not treat function offsets as patch locations.

The installed patch uses `create` as its commit point: `stop` and `config` are
published first, followed by a memory barrier and then `create`. It refuses to
install if any original GOT word no longer equals its stock method target. On
hook shutdown, `create` is restored first so no new adoption can begin, then
`config` and `stop` are restored for any surviving stock-owned object.

### Ownership handoff

Use an explicit state machine protected by a mutex:

```text
EMPTY
  → PREPARING
  → LOADER_ACTIVE
  → HANDOFF_PENDING
  → OMX_OWNED
  → EMPTY / REARM
```

Handoff ordering:

1. The create-wrapper atomically claims the prepared object.
2. Set `HANDOFF_PENDING`; stop and join the loader posting thread.
3. Ensure no loader writer still touches any of the twelve buffers.
4. Allow stock `setOutputBuffers` to retrieve those same pointers.
5. Mark `OMX_OWNED`; the hook must no longer post or destroy the object.
6. Let normal stock teardown destroy the adopted render object.
7. Clear the stale pointer and asynchronously prepare a fresh loader object for the next connection.

Never let both loader and OMX write or post the pool concurrently.

### Mandatory fallback

Adoption is allowed only when every runtime property matches:

- width and height;
- format;
- usage compatibility;
- displayable ID;
- buffer count and capacities;
- object is still healthy and loader thread has stopped.

On any mismatch or timeout:

1. hide and destroy the prepared loader object safely;
2. call the original stock create/config path;
3. prioritize working CarPlay over a seamless transition;
4. log one bounded reason code, not per-frame diagnostics.

## Qualcomm 32m backend

The cluster plane proved that raw Qualcomm 32m is not a generally scannable output format. The main stock window is different: stock itself presents 32m on displayable 59, so this backend is specifically for the stock main path.

Implementation must write directly into the exact Screen buffers later adopted by OMX.

### Phase A: safe layout-independent validation

Start with a full-frame solid or breathing luminance/chroma fill. Constant Y and UV values do not depend on pixel tile ordering. This validates:

- early window creation;
- buffer acquisition and post;
- context visibility;
- create/config adoption;
- tear-down and reconnect ownership.

Do this before implementing shapes in 32m.

### Phase B: authoritative tile mapping

To reproduce the full common spinner, implement `loader_qcom32m.c` only after deriving and validating the APQ8064/QNX layout:

- luma tile dimensions and macro-tile ordering;
- chroma tile dimensions and ordering;
- stride, slice-height and padding rules;
- UV addressing from the observed `0x78000` offset;
- required cache flush or Screen synchronization before post;
- RGB-to-YUV range/matrix used by this presentation path.

Every address calculation must bounds-check against the observed buffer capacity. Do not copy layouts from Tegra, modern Venus UBWC, the experimental from-source library or unrelated Linux drivers without an on-unit validation pattern.

### Phase C: full common scene

Implement the generic canvas callbacks over the validated tiled writer:

- `clear` as plane fill;
- `fill_rect` using tiled row spans;
- `fill_circle` using clipped spans;
- one Screen post per completed frame.

Target 30 fps but schedule with absolute monotonic deadlines. If a frame misses its deadline, drop the late animation frame; never queue multiple posts or delay the real decoder handoff.

## Build integration

Planned consumers:

```text
common/
  carplay_loader.c/.h          shared scene + RGBA reference backend (implemented)
  loader_qcom32m.c/.h          arbitrary-shape APQ8064 tiled backend (planned)

altscreen_render/
  screen_egl_present.c         early reusable RGBA output + loader draw
  sink_omx_qnx.c               later source attachment, first-frame takeover

hook/altscreen/
  main_loader.c/.h             stock render creation, animation and handoff
  altscreen_hook.c             early boundary trigger + internal patch lifecycle
```

Update both renderer and hook build lists explicitly. Keep the shared scene free of QNX headers so the host test remains authoritative for its raster behavior.

## Logging

Only lifecycle lines are required:

```text
loader[alt]: RGBA window 99 ready, 1440x542 @30fps
loader[main]: tiled window 59 ready, 1024x480 buffers=12 @30fps
loader[main]: handoff OK, same CScreenRender adopted by stock OMX
loader[alt]: first decoded frame replaced loader on existing RGBA window
```

Failures use one line with the failed stage and fallback result. No frame-by-frame logs.

## Verification stages

### 1. Shared generator — complete

- Host C99 build with `-Wall -Wextra -Werror`.
- Deterministic frames differ by animation time.
- RGBA alpha is opaque and stride padding is untouched.
- QNX ARMv7 compilation succeeds.
- ARM EABI5 object has no emutls references.

### 2. AltScreen loader only

- Start `altscreen_render` with no phone connected.
- Confirm managed RGBA window 99 exists.
- Switch to context 81 and observe the loader.
- Connect the phone and confirm the first decoded frame uses the same native RGBA window.
- Disconnect/reconnect repeatedly without stale window ownership or EGL errors.

### 3. Main solid tiled loader

- Prepare window 59 before phone connection.
- Switch to context 78 and verify a solid/pulsing pattern.
- Adoption is compiled in only for the exact stock-210 fingerprint and exact
  1024x480 / 12-buffer / format-0x1000c / displayable-59 config. Verify its
  post behavior against stock logs before merging the branch.

### 4. Main adoption

- Log object pointer, native window identity and all twelve buffer pointers before and after handoff.
- Confirm no second displayable-59 window is created.
- Confirm stock decoder reaches Idle/Executing and real type-110 video replaces the loader.
- Verify stock teardown owns destruction exactly once.

### 5. Full tiled animation

- Use a numbered/color calibration grid to validate tile mapping.
- Enable the complete common scene only after the calibration renders without mirrored, green, block-swapped or out-of-bounds regions.

### 6. Reliability

- cold boot with phone disconnected;
- phone already connected during boot;
- slow negotiation;
- rapid disconnect/reconnect;
- dio restart while renderers are supervised;
- CarPlay setup failure and fallback to stock screen creation;
- bounded `/tmp` logging and no system-wide stalls.

## Acceptance criteria

- Both displays show a generated loader within the first visible context transition.
- Both animations run at a target 30 fps without blocking Java, RTSP or decoder threads.
- Main video replaces the loader on the same window and buffer pool.
- Cluster video replaces the loader on the same RGBA window.
- No duplicate displayable 59 or 99 windows are created during normal takeover.
- No OMX start regression, Screen disown loop, double-free, stale buffer post or reconnect leak occurs.
- Any failed adoption cleanly falls back to the original stock main-screen path.

## Immediate next steps

1. Deploy this branch and verify window 99 with no phone video connected.
2. Verify window 59 appears before RTSP negotiation and posts the breathing fill.
3. Confirm `loader[main]: handoff OK` and that no second window 59 is created.
4. Exercise disconnect/reconnect and the phone-already-connected boot case.
5. Only then derive the full 32m tile mapper for the common spinner scene.
