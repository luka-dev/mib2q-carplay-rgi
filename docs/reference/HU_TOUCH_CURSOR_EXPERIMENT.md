# HU CarPlay touchpad cursor experiment — 2026-09-13

Implemented and built locally on `experiment/hu-touch-cursor`, worktree
`/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay-cursor`, based on master `9720844`.
Not deployed or visually verified on HU. Canonical master and its prepared release
bundle are untouched. Test the JAR, cursor_render and supervisor scripts together.
The rebuilt hook removes the previous in-DIO cursor prototype; it now only observes
stock main-video config/render for the [HU loader](HU_STARTUP_LOADER_AND_MENU.md).
Do not leave that old in-DIO cursor hook installed, or two
processes could compete for displayable 103.

## Composition and ownership

The user requires our own cursor, not the stock NAV crosshair, and explicitly
requested a separate displayable between the CarPlay video and stock HMI.

HU context 78 now contains **`{16, 103, 59}`**, front to back:

* 16: stock HMI, including its foreground UI.
* 103: our native loader during startup, then transparent cursor surface.
* 59: stock main CarPlay video (stream 110).

Other contexts do not include 103. G24 keeps stock context 78 and its +79 twin;
loader, pointer input and the extra touch capability are disabled on G24.

`NativeCursorOverlay` creates no widget, EAL node or texture. Java handles input coordinates, gesture ordering, HMI context/session gating and
DSI click pairing. It sends events/state through a private TCP link. Native
`engine.c` owns appearance, idle hold/fade, PRISM time and the video reveal. `cursor_render/` owns the QNX Screen window and its two RGBA
buffers on its main event thread, in a separate executable. DIO continues to own
the stock main video displayable 59. No cursor code remains in the preload hook.
This is a managed GLES2 RGBA surface, not an extra hardware video overlay allocation.
The superseded HMI-widget and in-hook implementations have both been removed.

The window uses the configured full HU canvas; cursor pixels are clipped to the
CarPlay viewport and include its screen offsets. The cursor is a procedural optical ring drawn by `cursor_render/glass_shader.h`,
about 74 px in diameter (previous ring: about 43 px), with a 112×112 px draw quad.
No cursor bitmap, sprite texture or CPU raster remains. The centre is fully
transparent. A torus surface, bright specular lips, chromatic edge dispersion and
an annular contact shadow provide the PRISM-like glass appearance. This is a local
optical material; it does not sample/refract actual stream-110 pixels.

Native motion filtering estimates direction from received positions. It slightly
stretches the ring and moves its highlights/dispersion, then settles after input
stops. The click centre remains exactly at the acknowledged cursor coordinates.
A six-second wrapped native phase drives a stronger moving chromatic highlight
even after input stops; it does not reset or extend the cursor idle/fade timers. RGB is premultiplied
by alpha, matching MU1316 DisplayManager's surface blending.

## Preventing a cursor over stock UI

1. Java requires active CarPlay, TerminalMode screen **3200000**, and confirmed main
   display context **78** before sending visible state.
2. The window starts with `VISIBLE=0`. Both buffers are cleared and a transparent
   frame is posted before offering it to DisplayManager. Our module never sets
   `VISIBLE=1` itself.
3. No coloured frame is posted before adoption is confirmed through `MANAGER_STRING`.
   Context membership then controls composition; leaving CarPlay removes 103 from
   the active context independently of the native fade.
4. Ordinary dismiss fades from current opacity over at most 350 ms, then destroys
   the window. Nonzero `updateRotary` force-hides immediately and preempts queued
   input/ACKs. Screen/session teardown still revokes the layer immediately. A 500 ms native lease also removes it after commands stop or the TCP client
   disconnects. Polling adds up to approximately 25 ms when Screen calls return
   normally; this is not a hardware-verified hard deadline.
5. Each SHOW needs the current, unexpired native token. Tokens rotate after successful
   presentation; stale queued commands cannot revive an expired cursor.

Adoption confirms registration, not actual visibility or correct layering on the
physical HU. Those remain hardware checks.

## Transport and redraw

The renderer listens on **127.0.0.1:19840**. Java is its reconnecting client, with
all socket I/O on the dedicated CarPlay-Cursor thread. This does not use CarplayBus
or the hook's single-client transport. Former cursor bus command IDs 0x0116/0x0117
were removed. A new connection closes the old plane and invalidates its lease.

Requests are raw fixed **40-byte, big-endian v2** records, with no outer bus header.
Use the matching JAR and native executable; v1 is deliberately rejected.

| Offset | Value |
| --- | --- |
| 0, 1 | Version 2; HIDE 0 / PROBE 1 / pointer input 2 / LOADER 3 |
| 2 | u16 displayable 103 |
| 4, 8 | u32 sequence, native lease token |
| 12, 14 | u16 canvas width, height |
| 16, 18, 20, 22 | u16 video input viewport x, y, width, height |
| 24, 26 | u16 target cursor x, y, local to video viewport |
| 28 | Pointer: idle 0 / touching 1 / released 2 / graceful dismiss 3 / force hide 4; loader: video-ready 0/1 |
| 29–31 | Reserved, zero |
| 32 | u32 input activity serial; increments only on contact/movement |
| 36 | u32 input revision, or loader video configuration epoch |

ACK is 32 bytes: BE32 sequence, signed status, next token, lease ms at 0–15;
BE32 native phase at 16 (hidden 0 / pointer 2 / loader 3 / reveal complete 4);
u16 displayed x/y at 20/22; BE32 click-valid duration at 24; BE32 applied revision
at 28. Status 1 acknowledges PROBE, 0 an applied state, -2 requests a new token,
and -1 reports native failure. Hidden pointer heartbeats renew the session without
creating a window. A LOADER_DONE reply is completion, not a visible pointer.

Java sends changed input immediately through coalesced dispatcher requests. A pending
100 ms context/lease poll does not delay new input. Unchanged state has a 150 ms
minimum heartbeat interval (normally about 200 ms at that polling cadence).
Java computes no opacity, animation timestamp or animation completion. HIDE preempts
pending adoption. ACK timeout is 650 ms; initial native response timeout is 1200 ms.
Socket recovery dismisses old touch activity; it cannot replay a held gesture.

The native loop draws independently at approximately 30 Hz, even between packets.
Duplicate activity never extends idle life. A released gesture may be coalesced
without losing its last contact/movement; a late release cannot revive an expired
cursor. The engine owns the 900 ms hold and 350 ms smooth fade. Each GPU draw clears
the whole surface, including both alternating buffers; final invisibility destroys
the window. Graceful dismiss does not restart on duplicate messages, never
brightens an existing fade, and relinquishes click ownership immediately. Force
hide clears opacity and optical motion immediately. There is no dependency on
new CarPlay video frames.

PRISM uses the same baked shader/material/signature and shared `common/prism_gl.c`
as altScreen. HU layout is full-canvas regardless of the video input viewport.
The native handoff uses `common/prism_handoff.h`; readiness retargets it, and a new
video epoch invalidates the old aperture. Java only consumes native completion.

Java clicks only with a live native presentation and native-provided click deadline,
measured conservatively from request send time. Coordinates come from the displayed
ACK, not the latest requested target. A delayed ACK cannot re-arm after dismiss,
geometry/session change or transport replacement.

Screen APIs are resolved with dlsym. The separate process owns EGL/GLES resources;
it neither imports nor modifies OMX/DIO pixels. The context and baked assets are
retained across cursor gestures, while windows/surfaces are released. No
`eglTerminate` is used on this Qualcomm stack. Adoption starts from transparent
buffers with swap interval 0; the native loop provides pacing.

## Process lifecycle and isolation

The existing carplay_startup.sh supervisor starts cursor_render without LD_PRELOAD,
after exec of DIO has already been initiated. Startup is not added to DIO's SI
startup deadline. It adopts an existing healthy :19840 listener, restarts exits,
preserves the process across ordinary DIO replacement and includes its captured
PID in explicit cleanup. Its /tmp log is capped with the other renderer logs.

No Screen context/window is created until a valid leased state actually needs visible pixels. A separate watchdog
thread checks main-loop progress: if a Screen call stalls for over 1500 ms, it exits
**only cursor_render** (code 70). Window reclamation on real QNX and overall graphics
server behavior still need HU validation. Normal main-loop polling continues to
handle HIDE, socket replacement and lease expiry while waiting for adoption.

## Input and provisional click mapping

* Relative movement uses existing touchpad callbacks; first contact anchors without
  jumping. Gain is isotropic. Input-1 taps remain local to the CarPlay video window.
* Last activity holds for 900 ms, then fades for 350 ms to zero. Stationary contact
  can expire; lifting afterwards does not revive the cursor.
* Nonzero `updateRotary` (rotation ticks) force-hides. Ordinary select, other keys,
  touchscreen or multiple fingers request graceful hide. Both routes cancel/retire
  click ownership and suppress that held contact until it lifts. A subsequent soft
  event cannot downgrade an already queued force-hide. A zero rotary delta does nothing.
* **Provisional:** centre-console `DDS_SELECT` clicks the last acknowledged cursor
  position when visible and selects stock focus when hidden. Select gracefully hides the
  cursor. Press/release routing, coordinates and sink stay latched as a pair.
* Steering-wheel raw-key-40 suppression runs first. Real touchscreen input cancels
  the cursor and follows the existing path. Pad coordinates are not screen positions.
* Teardown releases before closing its DSI proxy; phone disconnect abandons the old
  click without sending through a potentially rebound proxy.
* Service input flags add low-fidelity touchscreen bit 2 on touchpad configurations,
  retaining knob/pad bits, to support the experimental input-1 taps.

**Unresolved:** the user described separate mechanical touchpad-panel and rotary
presses. `touchPadPressed` means finger contact, not a mechanical click. Existing
`Key` handling exposes `DDS_SELECT`; distinct physical IDs have not been verified.
Inspect raw DSIKeyPanel keyboard/key/state events before claiming those buttons
are distinguished. iOS acceptance of the touch capability also needs device testing.

## Verified local firmware evidence

MU1316 `libdm_modMain.so`, SHA-256
`bd4c042457d4b3c0b9b5f3bcf8ef1e64a30842435e7bc70b477237acaa5ef6ec`, from
`Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/lib/`:

* `CScreenHandler::evtNewWindow`, 0x819e8: parses a numeric Screen window ID,
  obtains its displayable, attaches a window surface and stamps the manager string.
* `CDisplayManager::getDisplayable`, 0x39c08: creates and inserts a displayable when
  its map has no entry. Managed surfaces do not require a fixed DSI ID whitelist.
* `CGLRenderer::drawDisplayable`, 0x6e1fc, blend setup near 0x6e5e8: RGB factors
  `GL_ONE` / `GL_ONE_MINUS_SRC_ALPHA` require premultiplied surface pixels.

The earlier failed cluster `createImageDisplayable(103)` attempt does not establish
that native managed windows cannot use 103. No ImageDisplayable is created for this
cursor. Existing cluster backing displayables 101/102 are unchanged.

All 13 dynamically used Screen symbols exist in MU1316
`Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/ifs2/ifs_display/proc/boot/libscreen.so.1`.
BSP transparency property is **17**, as in working `common/cluster_surface.c`;
the newer compatibility header's enum 46 is not this BSP's value. Other constants
come from the checked-in ABI header. Focused disassembly is saved under
`output/r2/hu-cursor-plane-20260913/`; moving process ownership does not change
that display-manager registration evidence.

Historical commit `1e44597` painted directly into decoder frames and ghosted through
H.264 reference frames. An anonymous replacement buffer later hung on first touch.
Neither technique is used here. The old separate-window attempt also had incorrect
QNX constants; its failure alone did not rule out a managed cursor surface.

## Checks and next steps

* `bash scripts/build_renderers.sh`: all three ARM executables build, with no emutls.
* `bash scripts/build_hook.sh`: hook with read-only main-video observer; export/runtime guards pass.
* `bash scripts/test_java_transports.sh`: Java 1.4 build plus bus, renderer and
  alt-screen transport regressions pass.
* `bash scripts/test_hu_cursor.sh`: 29 Java input/click checks plus native engine/session ASan/UBSan and GPU tests pass.
  Covers graceful/force hide, motion response/settling, click pairing, Java/C wire, malformed packets, premultiplied alpha,
  alternating-buffer cleanup, leases, stale commands, disconnect during adoption,
  and creation failure. Lifecycle tests mock Screen; they do not verify the real
  compositor. Direct Java transport tests cover 30 start/stop generations, fragmented
  ACKs and no stale replay. Subprocess tests run the shipping C socket loop/session
  against a fake Screen backend: connection replacement, fragmented/partial frames,
  expiration, hide during adoption, failures and a stuck Screen call ending only
  the renderer through its watchdog.
* `bash scripts/test_supervisor_lifecycle.sh`: all three renderer start paths and
  captured stop generations pass.

Artifacts: `build/cursor_render` (~240 KiB), `build/carplay_hook.jar`, and the clean
`build/libcarplay_hook.so`. Deploy the two updated smartphone_integrator supervisor
scripts alongside them when testing; old supervisor scripts do not start cursor_render.
Next HU checks: 103 registration/z-order, static CarPlay with no new frames, fade,
alignment, stock menus/popups, rotary takeover, disconnect/reconnect, physical
click IDs, Screen posting latency and load. No commit, push, merge or deployment
of this experiment yet.

## Desktop reveal regression (2026-09-14)

The old preview switched off its entire startup scene when mouse input disabled
the automatic demo, and clicking/scrolling also cleared the loader timer. This
bypassed the native reveal. Preview loader ownership is now independent of pointer
input and persists until native completion, matching the Java startup contract.
The 5.8-second cutoff is removed. `cursor_preview_handoff_test.c` reproduces mouse,
click and wheel interruptions plus delayed readiness/frame delivery, and checks
the actual GPU output: 16 intermediate coverage frames at 25 ms cadence and no
single-frame cut from opaque to transparent. This fixes the desktop reproduction;
it is not a hardware trace proving every HU handoff path.
