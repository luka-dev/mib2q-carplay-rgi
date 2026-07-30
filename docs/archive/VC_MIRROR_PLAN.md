# VC Mirror Plan — CarPlay video mirror on displayable 33

## Goal

Render CarPlay video (Apple Maps / app screens) into the cluster on
**displayable 33** (`KOMBI_MAP_VIEW`, native map slot in cluster context 74),
in parallel with the existing custom 3D arrow renderer on displayable 20.

Block iOS screen-stream pause when the HU UI hides CarPlay, so the mirror
keeps receiving frames even while the main HU display is showing a
non-CarPlay source.

## Resulting architecture

```
cluster context 74 composition (cluster TFT, direct LVDS2):
    33  KOMBI_MAP_VIEW   ← CarPlay video mirror (this plan)
    20  RG widget slot   ← existing maneuver_render (3D arrows)
    102 cluster overlay  (native)
    101 cluster overlay  (native)

dmdt dc 74 33 20 102 101    ← 33 first → encoder reads mirror
                              setActiveDisplayable(4, 33) → MOST → VC LVDS map tab

iPhone H.264
    │
    ▼ (USB / Wi-Fi)
libairplay.so  ProcessFrames
    │
    ▼
dio_manager  COMXVideoDecoder  (Qualcomm OMX HW decode → tiled NV12)
    │
    ├─ CScreenRender::render → screen_post_window  (untouched, native pipeline lives)
    │
    └─ onFillBufferDoneCallback ── HOOKED ─→ POSIX SHM  (raw tiled NV12 + flag)
                                                  │
                                                  ▼
                                       mirror_render (new ELF on displayable 33)
                                                  │  GLES2 fragment shader:
                                                  │   • detile 64×32 + 2×2 zigzag
                                                  │   • YUV → RGB (BT.601)
                                                  ▼
                                               eglSwapBuffers
                                                  │
                                                  ▼
                                       displayable 33 in cluster ctx 74
                                                  │
                                                  ├─ cluster TFT (LVDS2) — composited under our 20
                                                  └─ MOST encoder reads it (VC sees mirror)
```

## Reverse-engineering findings (mechanism behind hide → pause)

### Hide is not RTSP TEARDOWN

It is the **CarPlay Modes Protocol** carried over the AirPlay control connection.
When the HU UI switches away from CarPlay it sends a DSI request to dio_manager,
which forwards it to iOS. iOS sees `screen.type = untake` and pauses video.
Session and stream stay alive; resume on next `take` is fast.

### Call chain

```
HMI process (HU UI)
    │  DSI: hmi2IOSChangeModesRequest(ModeState{screen,audio}, AppStates)
    ▼
dio_manager :: CDSICarplayImpl::hmi2IOSChangeModesRequest        (sub_168434 @ 0x168434)
    │  parses ModeState[screen{type,priority,takeConstraint,borrowUnborrowConstraint}, audio{…}]
    │  + AppStates{speech, phone, navigation}
    ▼
dio_manager :: CDIOManager::hmiRequestOrReleaseResources         (sub_155CEC @ 0x155cec)
    │  populates this+0x244..0x260 (ModeState fields, see field map below)
    │  logs:
    │    [CDIOManager] hmiRequestOrReleaseResources : <SCREEN> Type: %s | Priority: %s | …
    │    [CDIOManager] hmiRequestOrReleaseResources : <AUDIO>  Type: %s | Priority: %s | …
    ▼
libairplay.so :: AirPlayReceiverSessionChangeModes               (0x28aec)
    │  builds CFDict { "type":"changeModes", "params": <modes-dict> }
    │  via AirPlayCreateModesDictionary
    ▼
libairplay.so :: AirPlayReceiverSessionSendCommand               (0x18880)
    │  sends over CarPlay command channel to iOS
    ▼
iOS CarKit (CarKitVideoSender / CarKitScreenManager)
    │  observes screen.type=untake → pauses video stream (no frames sent)
    │  on take → resumes
```

### ModeState field map (CDIOManager `this` offsets)

In `sub_155CEC`, the `this` pointer is treated as an int array (`a1[…]`):

| index   | meaning                        | values                                                    |
|--------:|--------------------------------|-----------------------------------------------------------|
| `[145]` | screen.type                    | 0=n/a, **1=take**, **2=untake**, 3=borrow, 4=unborrow     |
| `[146]` | screen.priority                | 100=niceToHave, 500=userInitiated                         |
| `[147]` | screen.takeConstraint          | 100=anytime, 500=userInitiated, 1000=never                |
| `[148]` | screen.borrowUnborrowConstraint| 100=anytime, 500=userInitiated, 1000=never                |
| `[149]` | audio.type                     | same vocabulary as screen.type                            |
| `[150]` | audio.priority                 | same vocabulary as screen.priority                        |
| `[151]` | audio.takeConstraint           | same vocabulary                                           |
| `[152]` | audio.borrowUnborrowConstraint | same vocabulary                                           |

The completion is delivered async via
`CDIOManager::s_changeModeCompletionCallback(int rc, CFLDictionary*, void*)`
(`@ 0x1562dc`).

### libairplay PlatformControl operations (separate from modes)

`AirPlayReceiverSessionPlatformControl` (`0x1f190`) dispatches CFString commands:

* `duckAudio` / `unduckAudio`
* `setUpStreams` / `tearDownStreams`
* `startSession`

`tearDownStreams` is what gets invoked from `AirPlayReceiverSessionTearDown`
(`0x26b60`) when the HU sends a real RTSP TEARDOWN. The dict's `streams` array
holds entries `{type: 100|101|110}` (audio / secondary audio / **MainScreen**).
Stream 110 → `_ScreenTearDown` (`sub_266E4` @ `0x266e4`):

1. `AirPlayReceiverSessionScreen_UpdateState(state=3 STOPPING)`
2. `AirPlayReceiverSessionScreen_SendCommand(113 'q', 0, 0)` — quit byte to ProcessFrames pipe
3. `pthread_join(receiver_thread)` + `close(socket)`
4. `AirPlayReceiverSessionScreen_UpdateState(state=4 STOPPED)`

State machine values (only four): `0 NONE / 1 INITIALIZING / 2 READY / 3 STOPPING / 4 STOPPED`.
There is **no PAUSED state at this layer** — pause lives entirely in iOS.

### Session-level teardown

`CDIOManagerControllerJob_TeardownSession` → `CSIService::terminate` (`sub_165030`)
→ `CDIOManager::tearDownTheSession` (`sub_15FA58`). Calls
`AirPlayReceiverSessionTearDown` only when forced; this is the full session
shutdown path, distinct from mode-driven pause.

## Prior implementation we are reusing

`Patches/carplay_vc/` (a sibling project) already contains a working version
of the OMX hook + EGL renderer pair. We adapt rather than rewrite:

* `Patches/carplay_vc/carplay_vc_frame_hook.c` — LD_PRELOAD into dio_manager:
  * Hooks `_ZN3dio16COMXVideoDecoder24onFillBufferDoneCallbackEPvP20OMX_BUFFERHEADERTYPE`.
  * Tracks the CarPlay screen window via `screen_create_window_group` /
    `screen_destroy_window`; reads dimensions via
    `screen_get_window_property_iv(SCREEN_PROPERTY_SIZE)`.
  * **CPU detile** of GPU 64×32 tiled NV12 with 2×2 zigzag block layout
    (`detile_64x32` and `tiled_plane_size`).
  * Writes linearised Y + UV planes into POSIX SHM `/cp_vc_frame_shm`
    (header + sequence counter + raw bytes).
  * `CP_VC_KEEP_ALIVE` env keeps the libairplay pipeline up by blocking
    `SetVisibility(false)`, `StopSession`, and `CScreenRender::stop` —
    **superseded** by the modes hook in this plan (different layer, cleaner).

* `Patches/carplay_vc/carplay_vmaneuver_render_egl.c` — EGL+GLES2 renderer:
  * EGL via libdisplayinit, parametrised display/context/displayable IDs.
  * Two textures (`GL_LUMINANCE` for Y, `GL_LUMINANCE_ALPHA` for UV).
  * Simple BT.601 YUV→RGB fragment shader (CPU already detiled).

What changes in our adaptation:

1. **Move detile to GPU.** Hook becomes a single `memcpy` of the raw tiled
   buffer. Saves ~7–10 ms CPU per frame in dio_manager (armv7).
2. **New format flag** in SHM (`NV12_TILED_64x32` vs `NV12_LINEAR`) so the
   renderer picks the matching shader path.
3. **Drop `CP_VC_KEEP_ALIVE`** — modes hook does the same job at a higher
   layer (HMI side) and avoids touching libairplay internals.
4. **Routing** to `display=1, context=74, displayable=33` for cluster.
5. **Reuse `maneuver_render/platform_qnx.c` patterns** for window claim, 5 s reclaim
   watchdog, and `screen_destroy_window` on shutdown.

### Adreno 64×32 tile layout (carry forward from prior CPU code)

* Tile is 64 bytes × 32 rows.
* Tiles are grouped in 2×2 “blocks” (128 px × 64 row).
* Sub-tile order alternates by block column:
  * even `bx` block → `[TL, TR, BL, BR]` (sub_idx = `sy*2 + sx`)
  * odd  `bx` block → `[BL, BR, TL, TR]` (sub_idx = `(1-sy)*2 + sx`)
* Last partial block-row when `nty` is odd: tiles stored sequentially after
  all full block-rows, **2 tiles per block, no zigzag**:
  `tile_idx = full_pairs * nbx * 4 + tx`.
* Detile only valid when `width % 64 == 0`. Other widths arrive linear; in
  practice CarPlay sizes seen so far need probing during bring-up.

### SHM layout (existing — keep same magic, bump version)

```c
#define SHM_MAGIC   0x43505643  /* "CPVC" */
#define SHM_VERSION 2u          /* was 1 (linear-only) */

/* New format flags */
#define SHM_FMT_NV12_LINEAR     0x00
#define SHM_FMT_NV12_TILED_64x32 0x01

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t width, height;
    uint32_t stride;            /* tiled: 64; linear: width */
    uint32_t format;            /* SHM_FMT_* */
    uint32_t y_size;            /* bytes for Y plane (tiled or linear) */
    uint32_t uv_size;           /* bytes for UV plane */
    uint32_t sequence;          /* atomically bumped per frame */
    uint32_t frame_ready;       /* 1 once first valid frame is in */
    uint8_t  data[];            /* y_size + uv_size bytes */
} cp_vc_shm_t;
```

## Phase 1 — Modes hook (block screen untake)

Independent. Validates without touching the mirror at all; keep this as the
first deliverable.

### Files

* `c_hook/carplay_modes/modes_hook.c`
* `c_hook/carplay_modes/modes_state.h` (small state tracker for idempotence)

### Hook target

`CDIOManager::hmiRequestOrReleaseResources(this, reason)` —
sub_155CEC @ 0x155cec in dio_manager.

Mangled name (verify after build): export from dio_manager `.dynsym` is
`_ZN3dio11CDIOManager28hmiRequestOrReleaseResourcesEPKc` (best guess, confirm
with `nm dio_manager | grep hmiRequestOrRelease`).

### Logic

```c
int hook_hmiRequestOrRelease(void *self, const char *reason) {
    int *fields = (int *)self;
    int saved_screen_type = fields[145];    /* offset 0x244 */

    if (saved_screen_type == 2 /* untake */) {
        log_msg("modes_hook: blocking screen untake (reason=%s)", reason ?: "");
        fields[145] = 0;   /* n/a — AirPlay sends no screen mode change */
    }

    int rv = real_hmiRequestOrRelease(self, reason);

    /* Restore the field so HMI state-machine sees the same data it sent.
       Audio half (fields[149]..[152]) is forwarded normally. */
    fields[145] = saved_screen_type;

    return rv;
}
```

### Idempotence concern

When HU later sends `take=1`, iOS already considers the screen taken.
Need to verify that iOS accepts the redundant take without error. If it
rejects:

* Track `g_screen_taken` in `modes_state.h`.
* If a `take` request arrives and we believe iOS already has it taken, set
  `fields[145] = 0` again on the way out and synthesize a success
  completion via `s_changeModeCompletionCallback`.
* If a `take` request arrives and `g_screen_taken == false` (e.g. after
  full session restart), forward as-is.

### Validation

1. Build & deploy the hook into dio_manager via existing LD_PRELOAD config.
2. Start CarPlay. Switch HU UI to Radio (or any non-CarPlay source).
3. Watch hook log: expect `blocking screen untake`.
4. iPhone: connection indicator stays active.
5. Switch HU UI back to CarPlay → expect normal take, no error.
6. CarPlay video on the main HU display continues without restart.

If iOS rejects the second take, add the synthetic-completion path and
re-test.

## Phase 2 — OMX frame hook + SHM (adapt from carplay_vc)

### Files

* `c_hook/carplay_video/cv_omx_hook.c` (copy of `carplay_vc_frame_hook.c`)
* `c_hook/carplay_video/cv_shm.h` (header with shared struct, magic, flags)

### Diff from prior

1. **Remove** the CPU detile path; `memcpy` the raw OMX `pBuffer` for
   `y_tiled_sz + uv_tiled_sz` bytes into SHM.
2. **Compute** `y_tiled_sz` / `uv_tiled_sz` via `tiled_plane_size()` — same
   formula already in prior file.
3. **Set** `g_shm->format = SHM_FMT_NV12_TILED_64x32` when
   `(w % 64) == 0 && (w >= 64) && (h >= 32)`; otherwise
   `SHM_FMT_NV12_LINEAR` and write `w*h + w*(h/2)` bytes raw.
4. **Bump** `SHM_VERSION` to 2 and rename `data[]` layout to header above.
5. **Drop** `CP_VC_KEEP_ALIVE` block of `SetVisibility` / `StopSession` /
   `CScreenRender::stop` overrides — Phase 1 supersedes them.
6. **Frame-format probe** at first capture: log `nFilledLen` and the two
   tiled-plane sizes vs the two linear-plane sizes for the actual `w×h` we
   see. Decides whether iOS sends padded tiled NV12 for unaligned widths
   or always linear for them. Affects the `(w % 64) == 0` gate above.

### Validation

* `tools/cv_dump.c` — small reader that opens `/cp_vc_frame_shm`, polls
  `sequence`, decodes one frame in software (port the original
  `detile_64x32` here), and writes a PPM. Confirms geometry / format.

## Phase 3 — Mirror renderer with GPU detile

### Files

```
mirror_render/
    main.c           — main loop, FPS pacing, lifecycle handshake
    platform_qnx.c   — adapted from maneuver_render/platform_qnx.c (claim 33, reclaim, release)
    platform_macos.c — dev path (read SHM dump replay)
    engine.c         — GL state, FBOs, uniform setup
    shaders.c        — vertex + fragment shader sources (string literals)
    cv_consumer.c    — open SHM, wait for sequence, expose latest frame ptr
    Makefile / build.sh
```

### Routing

```c
#define MR_DISPLAY_ID     1   /* cluster — LVDS2 */
#define MR_CONTEXT_ID     74  /* cluster MAP context */
#define MR_DISPLAYABLE_ID 33  /* KOMBI_MAP_VIEW (native map slot) */
```

Window group string: same as maneuver_render — `"How are you gentlemen?"`.

### dmdt restore on startup

```sh
dmdt dc 74 33 20 102 101    # 33 first → MOST encoder reads mirror
dmdt sc 1 74
```

If both 33 and 20 are claimed, native compositor lays out by context order.
Encoder source: first-in-context = 33 → VC sees mirror.

### GPU detile shader

Y plane tiled buffer is uploaded as a `GL_LUMINANCE` 2-D texture with
**`width = 64`** and **`height = y_tiled_size / 64`**. Each tile is then
exactly 32 contiguous rows in that texture.

Same scheme for UV (interleaved CrCb) on a `GL_LUMINANCE_ALPHA` texture
with width=32 (each chroma tile is 32×16 in 2-component samples) — or
keep `width=64` and treat each sample as 2 packed chroma bytes; pick
whichever maps cleanly given `glTexImage2D` layout. Decide once during
Phase 2 dump verification.

```glsl
precision mediump float;

varying vec2 v_tex;            /* [0..1] target frame UV */

uniform sampler2D u_tile_y;
uniform sampler2D u_tile_uv;
uniform vec2 u_frame_dim;      /* width, height */

/* Y plane geometry */
uniform float u_ntx_y;         /* width / 64 */
uniform float u_nbx_y;         /* ntx_y / 2 */
uniform float u_nty_y;         /* ceil(height / 32) */
uniform float u_full_pairs_y;  /* floor(nty_y / 2) */
uniform float u_last_partial_y;/* 0 or 1 */
uniform float u_tex_h_y;       /* tiled-Y texture height in pixels */

/* UV plane geometry (half-resolution) */
uniform float u_ntx_uv;
uniform float u_nbx_uv;
uniform float u_nty_uv;
uniform float u_full_pairs_uv;
uniform float u_last_partial_uv;
uniform float u_tex_h_uv;

float tile_idx_for(float tx, float ty,
                    float ntx, float nbx, float nty,
                    float full_pairs, float last_partial)
{
    if (last_partial > 0.5 && abs(ty - (nty - 1.0)) < 0.5)
        return full_pairs * nbx * 4.0 + tx;

    float bx = floor(tx / 2.0);
    float by = floor(ty / 2.0);
    float sx = mod(tx, 2.0);
    float sy = mod(ty, 2.0);
    float block_idx = by * nbx + bx;

    float sub_idx = (mod(bx, 2.0) < 0.5)
                  ? sy * 2.0 + sx
                  : (1.0 - sy) * 2.0 + sx;

    return block_idx * 4.0 + sub_idx;
}

vec2 detile_uv(vec2 px, float ntx, float nbx, float nty,
                float full_pairs, float last_partial, float tex_h)
{
    float tx = floor(px.x / 64.0);
    float ty = floor(px.y / 32.0);
    float fx = mod(px.x, 64.0);
    float fy = mod(px.y, 32.0);

    float tile_idx = tile_idx_for(tx, ty, ntx, nbx, nty, full_pairs, last_partial);
    float src_y = tile_idx * 32.0 + fy;
    return vec2((fx + 0.5) / 64.0, (src_y + 0.5) / tex_h);
}

void main() {
    vec2 px = v_tex * u_frame_dim;

    vec2 y_uv  = detile_uv(px,
                           u_ntx_y,  u_nbx_y,  u_nty_y,
                           u_full_pairs_y,  u_last_partial_y,  u_tex_h_y);

    /* UV plane is half-resolution in NV12 — sample at px*0.5. */
    vec2 uv_uv = detile_uv(px * 0.5,
                           u_ntx_uv, u_nbx_uv, u_nty_uv,
                           u_full_pairs_uv, u_last_partial_uv, u_tex_h_uv);

    float y  = texture2D(u_tile_y,  y_uv).r;
    vec2  uv = texture2D(u_tile_uv, uv_uv).ra - vec2(0.5, 0.5);

    /* BT.601 limited-range; tweak to BT.709 if Apple Maps looks off. */
    float r = y + 1.402   * uv.y;
    float g = y - 0.344136 * uv.x - 0.714136 * uv.y;
    float b = y + 1.772   * uv.x;

    gl_FragColor = vec4(r, g, b, 1.0);
}
```

### Fallback shader for `NV12_LINEAR`

Same YUV→RGB body, but `y_uv = v_tex` and `uv_uv = v_tex` (no detile
math). Pick at program-link time based on `g_shm->format`.

### Reuse maneuver_render lifecycle helpers

* `platform_init` — EGL/GLES2 setup, create window group, `display_create_window`
  for `displayable_id=33`.
* `platform_reclaim_displayable` — every 5 s, call `screen_manage_window`.
* `platform_release_displayable` — `screen_destroy_window` on shutdown
  (registered via `atexit`, ordered before `restore_display`).
* SIGTERM handler returns without `raise`, allowing main loop to exit
  gracefully and run atexit chain.

## Phase 4 — Lifecycle integration

### Java side (BAPBridge.java or new CarPlayMirrorBridge.java)

```java
/* On CarPlay session start: */
restoreContext74("33 20 102 101");      // 33 first
launchMirrorRender();                    // /eso/bin/apps/mirror_render
launchCustomRender();                    // /eso/bin/apps/maneuver_render (existing)

/* On CarPlay session end: */
sendShutdown(mirrorRender);              // CMD_SHUTDOWN over its TCP control port
sendShutdown(customRender);              // existing
sleep(500);                              // grace window for screen_destroy_window
slay(mirrorRender);
slay(customRender);
```

Both processes are independent and have their own reclaim watchdogs.

### Encoder behavior

`setActiveDisplayable(4, X)` is called by stock cluster firmware in
`preContextSwitchHook` and reads the **first** displayable in the context.
With our `dmdt dc 74 33 20 102 101` ordering, the encoder reads 33 — the
mirror — and that is what lands on VC LVDS via MOST.

The cluster physical TFT shows the full composite (33 mirror as
background + 20 arrows widget on top + 102/101 overlays), since direct
LVDS2 carries the compositor’s composition, not the encoder output.

## Modes hook decision tree (consolidated)

```
hmi2IOSChangeModesRequest arrives at dio_manager
    │
    ▼
Has screen.type field?
    │
    ├─ no → forward unchanged
    │
    └─ yes:
        screen.type == 2 (untake)?
            ├─ no → forward unchanged
            └─ yes →
                set screen.type = 0 (n/a) before forwarding
                restore screen.type after AirPlay call returns
                (audio mode forwards normally either way)
```

## Open questions (resolve at HW)

1. iOS tolerance of redundant `take` after blocked `untake`. Test, and add
   synthetic-completion fallback if rejected.
2. Actual CarPlay video resolution(s) on this HU and whether
   `width % 64 == 0`. Determines tiled vs linear path. Probe at first
   `onFillBufferDone` and log.
3. Whether MOST encoder rate gating still applies to a non-native
   displayable owner (we own 33 instead of native nav). If
   `setKOMODataRate` and friends interfere, we may need to call
   `forceClusterRouteInfoState(true)` and `setKOMODataRate(2)` to keep
   encoder pumping at a useful rate even without native RG.
4. UV plane texture format (`GL_LUMINANCE_ALPHA` width=32 vs width=64
   packed) — confirm during Phase 2 dump.
5. UV-plane tile geometry: NV12 chroma is half-resolution; verify the
   GPU produces a separately-tiled UV plane with `tiled_plane_size(w, h/2)`
   bytes (already assumed in prior CPU code) and zigzag rules unchanged.

## Validation matrix

| Phase | Test                                | Expected                                                   |
|------:|-------------------------------------|------------------------------------------------------------|
| 1     | Hide CarPlay → log line             | `blocking screen untake`                                   |
| 1     | Hide → iPhone status                | indicator stays connected                                  |
| 1     | Hide → reshow                       | clean resume, no AirPlay error                             |
| 2     | First OMX capture                   | log `nFilledLen` + computed tiled / linear sizes match     |
| 2     | `cv_dump` PPM                       | recognisable Apple Maps frame (after software detile)      |
| 3     | mirror_render on HU                 | cluster TFT shows mirror behind arrows widget              |
| 3     | mirror_render fps                   | ≥ 20 fps from `eglSwapBuffers` timing log                  |
| 4     | VC LVDS map tab                     | shows mirror, not arrows widget                            |
| 4     | Hide CarPlay (with Phase 1 active)  | mirror keeps updating; VC LVDS keeps streaming             |

## Reference index

| Subject                                  | Location                                                                  |
|-------------------------------------------|---------------------------------------------------------------------------|
| Modes DSI entry                           | dio_manager `sub_168434` @ 0x168434                                       |
| Modes resource handler                    | dio_manager `sub_155CEC` @ 0x155cec (`hmiRequestOrReleaseResources`)      |
| ModeState completion                      | dio_manager `0x1562dc` (`s_changeModeCompletionCallback`)                 |
| Send modes to iOS                         | libairplay `0x28aec` (`AirPlayReceiverSessionChangeModes`)                |
| AirPlay command sender                    | libairplay `0x18880` (`AirPlayReceiverSessionSendCommand`)                |
| Screen tear-down                          | libairplay `0x266e4` (`_ScreenTearDown`)                                  |
| Stream-list teardown dispatcher           | libairplay `0x26b60` (`AirPlayReceiverSessionTearDown`)                   |
| Platform control dispatcher               | libairplay `0x1f190` (`AirPlayReceiverSessionPlatformControl`)            |
| OMX capture point                         | libairplay/dio symbol `_ZN3dio16COMXVideoDecoder24onFillBufferDoneCallbackEPvP20OMX_BUFFERHEADERTYPE` |
| Stock screen state values                 | libairplay `0x29fa8` (`AirPlayReceiverSessionScreen_UpdateState`): 0/1/2/3/4 |
| Tile detile algorithm (CPU reference)     | `Patches/carplay_vc/carplay_vc_frame_hook.c:330` (`detile_64x32`)         |
| Tile size formula                         | `Patches/carplay_vc/carplay_vc_frame_hook.c:394` (`tiled_plane_size`)     |
| Existing OMX hook reference               | `Patches/carplay_vc/carplay_vc_frame_hook.c:489`                          |
| Existing EGL renderer reference           | `Patches/carplay_vc/carplay_vmaneuver_render_egl.c`                              |
| Window claim / reclaim pattern            | `maneuver_render/platform_qnx.c` (`platform_reclaim_displayable`, atexit chain)  |
| Window group string                       | `maneuver_render/platform_qnx.c` `LIBDISPLAYINIT_WINDOW_GROUP = "How are you gentlemen?"` |
| Existing protocol header                  | `maneuver_render/protocol.h`                                                     |
| BAP / cluster lifecycle code              | `java_patch/com/luka/carplay/routeguidance/BAPBridge.java`                |
