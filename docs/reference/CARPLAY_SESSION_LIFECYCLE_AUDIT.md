# CarPlay session lifecycle audit (MHI2Q / stock libairplay 210)

Update 2026-09-17: the owner confirms connection/reconnection and mode
transitions are closed on the current vehicle setup. The August audit and
fault-replay recipes below are historical; they are not current release
blockers. See [current status and later fixes](../../TODO.md).

Status: source/log lifecycle audit and remediation completed in the current
worktree, 2026-08-12. Deployment in scope is stock `dio_manager` and stock
`libairplay.so` 210.81 with `libcarplay_hook.so`, plus the two external
renderers. The experimental from-source `libairplay-mhi2q` tree is not evidence.

The current source and generated ARM artifacts pass the QNX builds, renderer
host syntax checks, Java 1.4 build, `emutls=0`, shell syntax checks and
`git diff --check`. They have not yet passed the required on-unit fault replay;
“fixed” below means implemented and verified off-unit, not release-proven.

## Remediation status in the current worktree

All lifecycle defects identified by this audit are now addressed:

1. Stream 111 requests preferred port `-6031`; the non-native 500 ms handoff
   to stock stream 110 was deleted. Stock 110 remains the sole owner of `-6030`
   (`hook/altscreen/altscreen_hook.c`).
2. Control-SETUP enables `altScreen`/`viewAreas` only after constructing a
   complete real-main + cluster display tree. The same retained tree is then
   inserted atomically as `/info` `extendedFeatures`, `displays` and
   `displayPanels`. Missing real main `uuid`, allocation failure, a disabled
   display gate, or a stale control-SETUP all leave altScreen disabled. A
   monotonic advert sequence prevents a delayed old SETUP from replacing the
   new session's tree.
3. `AirPlayReceiverSessionSendCommand` is no longer called while `alt_lock` is
   held or on a dio/receiver/HTTP-completion caller. A bounded 32-entry worker
   queue snapshots the committed generation and retains each command. Its sole
   worker retains the live session under `alt_lock`, releases the lock, calls
   stock, then releases both objects. This removes both the TearDown lock edge
   and the added synchronous `HTTPClientSendMessage` wait from stock threads.
4. The stream-111 listener accept is a finite one-second loop, reads observe a
   60 ms stop tick, renderer sends are capped, receiver generations own all
   config/forward/close and stateful command completions, and teardown joins
   only after the terminal flag.
5. `altscreen_render` drains TCP on a dedicated bounded queue while OMX starts,
   bounds loader/control/decoder-worker shutdown, and has isolated-process
   watchdogs for Screen/EGL startup and final OMX/EGL teardown.
   `maneuver_render` now watches platform initialization, every render/EGL swap,
   and final teardown; a wedged graphics client exits before becoming a
   persistent live-PID zombie.
6. Java `CarplayBus`, `AltscreenControlServer` and file logging no longer write
   sockets/files from stock HMI/BAP callbacks. Generation-scoped bounded queues
   move all potentially blocking I/O onto daemon writers.
7. The wrapper performs a single guarded stock port-3 reset only after three
   long-lived generations with no RTSP SETUP marker and the exact PPS state
   `drivers_matched::2` / `drivers_running::1`. Any successful SETUP or non-stuck
   PPS state clears and rearms the latch.
8. Initial stale-renderer replacement uses a one-second pre-dio grace; explicit
   system shutdown retains the four-second Qualcomm WFD/EGL grace. Ordinary dio
   generation churn leaves persistent renderers alive.
9. The C hook logger now formats into a fixed 1024-byte record and enqueues it
   to a bounded 64-record ring. Only its daemon writer touches `/tmp`; overflow
   drops the oldest diagnostic. This also fixes the old `vsnprintf` length bug,
   where an overlong formatted message could make `write()` read beyond the
   stack buffer. Logger shutdown waits only for a published terminal flag and
   never blindly joins a writer wedged in filesystem I/O.
10. MU1316's stock PF rules permit inbound `carplay0` TCP 6030 and then drop all
    other traffic. The native 111 listener on TCP 6031 therefore bound
    successfully but never received the iPhone's IPv6 connection. The wrapper
    now validates and loads an otherwise-stock, once-per-boot runtime ruleset
    extending only that allow-list entry to `6030:6031` before starting dio.

Current source anchors:

| Fix | Implementation |
|---|---|
| Monotonic advert transaction | `hook/altscreen/altscreen_hook.c:663-713`, control gate at `:1783` |
| Finite 111 accept | `hook/altscreen/altscreen_hook.c:1456-1464` |
| Native 111 preferred port | `hook/altscreen/altscreen_hook.c:1593-1597` |
| Generation-scoped async `/command` worker | `hook/altscreen/altscreen_hook.c` (`alt_send_async_for_gen`, `alt_command_worker_main`, retained call outside `alt_lock`) |
| Async OMX queue + renderer watchdogs | `altscreen_render/main.c:40-72`, `:241-318`, `:429-490` |
| Bounded loader handoff | `altscreen_render/screen_egl_present.c:898-934` |
| Maneuver EGL progress watchdog | `maneuver_render/main.c:89-117`, `:320-332`, `:395-412` |
| Java bus/control/log writers | `CarplayBus.java:67-219`, `AltscreenControlServer.java:46-139`, `Log.java:24-100` |
| Nonblocking C hook logging | `hook/framework/logging.c` (`log_writer_main`, bounded `log_write`, terminal-only shutdown) |
| Guarded USB recovery | `carplay_processes.sh:23-103`, consumed/recorded by `carplay_startup.sh:121,132` |
| Native 111 firewall admission | `carplay_processes.sh` (`cp_enable_altscreen_firewall`), invoked before dio by `carplay_startup.sh` |

### The watchdog-hang path, separated from its trigger

The `-6030` collision is the binary- and log-proven trigger for the captured
ten-second black-110 teardown loop. The strongest code-forced mechanism for the
separate SI `TIMEOUT_WATCHDOG` symptom was the old command path: it held
`alt_lock` across stock `AirPlayReceiverSessionSendCommand`, while full TearDown
first tried to acquire that same lock. Stock 210 builds and serializes a POST
`/command` and calls `HTTPClientSendMessage` at `0x28584`/`0x28618`; it also
dereferences `session+704` at `0x28468` before its own optional retain at
`0x28548`. Therefore that old lock scope was both unbounded and unsafe. The new
generation-scoped worker plus outer retain removes the deadlock/UAF edges and
keeps the remaining stock HTTP wait off every dio-owned caller.

## Bottom line

The `-6030` collision is sufficient, by itself, to explain the captured
black-main-screen -> ten-second timeout -> whole AirPlay teardown sequence. It
was the primary RTSP defect and is fixed in the current worktree.

The non-atomic advert, renderer blocking paths and independent pre-RTSP USB
failure were also real. Their corrections are listed above; the historical
evidence and reasoning remain below so none of these findings is lost.

Unless explicitly labelled “current”, source line numbers in the historical
sections below describe the pre-remediation snapshot; addresses and captured
log line numbers remain stable evidence.

## Confirmed primary RTSP defect: stream 111 uses stream 110's port

This is a binary-forced finding, not an inference from a timeout.

Our type-111 setup opens its listener with preferred port `-6030` at
`hook/altscreen/altscreen_hook.c:1430`. Stock 210's type-110 `_ScreenSetup`
also passes `-6030` to `ServerSocketOpen` at `0x270e4-0x27118`.

The native MHI3 implementation explicitly separates the two streams in
`AirPlayReceiverSessionSetup`:

- `0x63eb0`: compare stream type with 110;
- `0x63eb4`: load `-6031`;
- `0x63ebc`: conditional increment when the type is 110;
- `0x63ed8`: call `ServerSocketOpen`.

Therefore the native mapping is:

| Stream | Preferred port |
|---|---:|
| main screen, type 110 | `-6030` |
| cluster altScreen, type 111 | `-6031` |

Stock 210's `ServerSocketOpen@0x7fb00` treats a negative port as a preferred
port with dynamic fallback: it takes the absolute value at
`0x7fb84-0x7fb90`, first binds that port at `0x7fbd8`, and, only when the
original input was negative, retries with port zero at `0x7fc04-0x7fc24`.
Thus `-6030` is not a shared service port. Giving it to both listeners creates
a race in which one stream falls back to an ephemeral port.

The failed on-unit run is the exact manifestation:

- `log/live_20260812_black110_restart/carplay_hook.log:818`: stream 111 got
  port 6030;
- `:822`: 111 SETUP returned at `00:18:24.732`;
- `:824`: stock 110 SETUP returned at `00:18:25.039`;
- `:825`: 111 accepted and closed its listener only at `00:18:25.059`, 20 ms
  after stock 110 SETUP;
- `log/live_20260812_black110_restart/sloginfo.txt:3014`: stock 110 entered
  `INITIALIZING`;
- `:3939-3941`: exactly ten seconds later its accept path stopped the screen
  session;
- `:3943-3968`: that failed 110 then caused the complete AirPlay session
  teardown and connection error `-6753`.

Stock `_ScreenThread@0x24220` calls `SocketAccept(listener, 10, ...)` at
`0x242a0-0x242b0`; the error path reaches StopSession through
`0x242b8 -> 0x24460 -> 0x243f0`, with StopSession at `0x24400`. Consequently,
the later `Shutdown signaled: 1` is a teardown consequence, not the initiating
failure.

The successful retry confirms the timing dependency rather than disproving
the collision: 111 accepted at
`log/live_20260812_black110_restart/carplay_hook.log:922` (`00:18:44.852`),
before stock 110 setup at `:949-950` (`00:18:45.080`), and stock 110 then
reached READY at `sloginfo.txt:5486`.

### Exact correction (implemented)

1. In `hook/altscreen/altscreen_hook.c:1430`, change the type-111 preferred
   port from `-6030` to `-6031`.
2. Delete `ALT_MAIN_PORT_HANDOFF_WAIT_MS` and
   `alt_wait_for_main_port_handoff` (`hook/altscreen/altscreen_hook.c:793-871`)
   and its pre-stock-SETUP call. A 500 ms ordering delay is not part of the
   native lifecycle and cannot make two listeners sharing one preferred port
   deterministic.
3. Log the returned `dataPort` for both response stream descriptors during
   validation. Expected steady state is 110=6030 and 111=6031.

## Independent pre-RTSP failure class: USB enumeration

Some “nothing starts until replug” runs fail before the hook receives any
control SETUP. In `log/live_20260812_dynamic_port_failed/sloginfo.txt:7646-7648`
the EHCI stack reports a control-transfer timeout; the same repeats at
`:7764-7766`, and smartphone_integrator publishes `FAILED_OTHER` at `:7772`.
Only after SI reaches CONNECTED at `:11017` does control SETUP begin and stock
110 later reach READY at `:11343`.

This is independent of the 110/111 port collision: the hook, AirPlay SETUP,
Java patch and renderers have not entered the failing transaction yet. The
stock reset machinery and the production recovery gap are tracked in
`docs/reference/USB_STARTUP_RECOVERY_DIAGNOSIS.md` and remain under audit here.

The existing Audi binary already has the correct low-level reset machinery:
`onJob_resetPort@0x194294`, its command builder at `sub_19B984`, and the exact
stock command `reset port 3 250 1`. However, smartphone_integrator's
`verifyPortReset@0x184030-0x184124` does not select that path for the observed
TIMEOUT_STARTUP/state-14 case. The production scripts currently contain no
one-shot recovery for a session which never reaches control SETUP.

This must remain a separate recovery class. Resetting USB after an RTSP failure
would hide the real port defect and add unnecessary physical-link churn.

## Captured healthy activation timeline

`log/live_20260812_0004/carplay_hook.log` shows that the hook itself does not add
the perceived multi-second startup latency:

| Time | Event | Evidence |
|---|---|---|
| `00:00:47.093-47.112` | iAP2 Identify accepted/patched | hook log, early session records |
| `00:00:47.433` | authentication path active | hook log |
| `00:00:48.016-48.041` | initial RGI injection queued/succeeds | hook log |
| `00:00:48.563` | control SETUP response gets `enabledFeatures` | `carplay_hook.log:32` |
| `00:00:48.614` | `/info` gets the display arrays | `:35-39` |
| `00:00:48.680-48.685` | type 111 SETUP is built | `:40` and following records |
| `00:00:48.808` | phone connects to 111 listener | `:48` |
| `00:00:48.864` | stock type 110 SETUP | later SETUP timing record |
| `00:00:48.931` | first 111 IDR | `:54` |

Control negotiation, `/info`, 111 setup and stock 110 setup are separated by
milliseconds. Delays before this sequence belong to USB/SI/iAP2 startup; delays
after it belong to the phone's data connection, stock screen decoder or the
external renderer.

## Additional confirmed lifecycle defects

### P1: altScreen negotiation is not atomic with display readiness

The phone sees `altScreen` before `/info` is built. This is forced by the actual
message order above: control SETUP is at `48.563`, `/info` at `48.614`.

Our control-SETUP interpose appends `altScreen` and `viewAreas` whenever
`alt_enabled`, the SETUP response and `alt_resolve()` succeed
(`hook/altscreen/altscreen_hook.c:1599-1648`). `alt_resolve()` verifies the
receiver/CF primitives, but does not require `disp_ok` or a usable stock main
display (`:680-707`). In contrast, display injection is separately gated by
`disp_ok` and `ALTSCREEN_ADVERT_DISPLAYS` (`:571-601`), and
`build_displays()` legitimately returns NULL when ScreenCopyMain is not ready
(`:464-475`).

Therefore all of these states in the pre-remediation snapshot negotiated a contract which the later
`/info` cannot fulfil:

- `ALTSCREEN_ADVERT_DISPLAYS=0`;
- a transient missing display-building symbol/state;
- a cold `/info` for which the stock main display is unavailable;
- failure to allocate the display dictionaries/arrays.

This is a session-killer, not merely a missing cluster picture: iOS has already
set the negotiated altScreen flag, then receives no valid two-display shape.

Implemented correction: a control-SETUP preflight now requires
`ALTSCREEN_ADVERT_DISPLAYS`, `disp_ok`, a successfully copied stock main display
and a real `uuid` before adding `altScreen`/`viewAreas` to `enabledFeatures`.
Release the probe object immediately. If preflight fails, preserve only stock's
existing `enabledFeatures` and do not advertise either altScreen contract for
that activation.

### P1: fabricated main-display fallback is internally inconsistent

When stock ScreenCopyMain has no `uuid`, `main_disp_modernize()` writes
`uid=A000...0A1` at `hook/altscreen/altscreen_hook.c:438-442`, but writes a
different UUID, `A000...0001`, into `initialVideoStreams` at `:456-458`. The
same function's own valid-path invariant requires raw `uuid`, physical `uid`
and initial-stream UUID to be identical (`:419-437`).

The fallback is therefore not safe degradation: it creates a main display
which cannot route the stock type-110 stream consistently. It was not exercised
in the captured runs (they all logged a real main UUID), but if reached it is a
direct black-110 candidate.

Implemented correction: the fabricated fallback is deleted. A missing real main UUID now
fails the display preflight and suppresses altScreen negotiation for that
activation.

### P1: loader-to-video handoff has an unbounded Adreno wait

The startup-loader thread holds `p->gl_lock` across `eglSwapBuffers()` in
`altscreen_render/screen_egl_present.c:783-835`. On the first decoded frame,
`screen_egl_present_frame()` calls `screen_egl_present_stop_loader()`
(`:940-944`). That stop function first waits for the same lock and then performs
an unconditional `pthread_join()` (`:891-907`). The caller is the OMX
`FillBufferDone` callback through `_PresentGpu()`
(`altscreen_render/sink_omx_qnx.c:158-223`).

If the old QNX Adreno stack wedges inside the loader swap, the first decoded
callback is permanently blocked before the existing callback-drain timeout can
help. The process may still exist and own listening port 19831, so the
supervisor's PID/LISTEN health checks can accept a renderer which can never
present video. This matches the class of historical EGL-client crashes, but no
current log proves that this exact edge fired in the captured 110 timeout.

This defect cannot directly hang `dio_manager`: the renderer is a separate
process and the hook's loopback send is bounded. Its failure mode is a black or
wedged 111 renderer.

Implemented correction: the loader publishes a terminal flag and the decoded
callback waits only 750 ms; a wedged isolated renderer exits without freeing
storage underneath the EGL thread. Startup, runtime progress and final teardown
now have matching isolated-renderer watchdogs. The longer-term single-EGL-owner
design remains optional, not required to eliminate the unbounded wait.

Original recommendation: remove the unbounded cross-thread EGL handoff. Prefer a
single owner for the EGL context (loader and decoded draws serialized on one
render thread). For a minimal patch, publish a loader-exited flag, wait a
bounded interval without freeing its storage, and `_exit()` the isolated
renderer if the Adreno call does not return; the supervisor can then restart a
clean process after backoff.

### P1: hook back-pressure budget is shorter than decoder startup budget

`altscreen_render` starts OMX synchronously from its sole TCP server path when
the first config/frame reaches `av_sink_frame()`
(`altscreen_render/sink_omx_qnx.c:723-776`). Loaded -> Idle is allowed to take
up to ten seconds (`:237-240`, `:531-576`). While that happens the renderer is
not draining further loopback frames.

The hook gives the link a 512 KiB send buffer but drops it after only one second
of sustained EAGAIN (`hook/altscreen/altscreen_av.c:26-30`, `:100-176`). At
1440x542/30 fps, a slow but valid cold OMX startup can fill the socket before
the renderer resumes reading. The result is a false renderer disconnect and a
black 111/retry cycle, not a stock 110 teardown.

Implemented correction: the socket thread continuously drains into a
generation-scoped queue capped at 1024 records / 24 MiB, while one worker owns
OMX startup, frame delivery and teardown. Queue failure drops the connection
instead of feeding a partial GOP. The hook send remains bounded at one second;
the old eight-second stall is not restored inside `dio_manager`.

## Resilience risks requiring an on-unit fault replay

### R1: infinite accept relies on cross-thread close semantics

The type-111 receiver calls `SocketAccept(listenSock, -1, ...)`
(`hook/altscreen/altscreen_hook.c:1288-1297`). Teardown closes the listener and
waits only 1800 ms for the receiver terminal flag; it calls `pthread_join()`
only after that flag is present (`:888-959`). Thus there is no blind unbounded
join in dio. However, the design assumes that QNX's stock `SocketAccept`
reliably wakes when another thread closes the listener.

If that assumption is false for this wrapper, teardown remains bounded but the
joinable accept thread and one of four slots (`:760-781`) survive. Four such
events make later 111 SETUP fail with `-6728`, which then fails the whole SETUP
at `:1650-1656`.

Implemented correction: `SocketAccept` now runs with a finite one-second
deadline and rechecks the slot stop flag. Validation still must start a 111
listener without letting the phone connect, issue
full TEARDOWN repeatedly, and require `threadExited`/slot GC on every cycle.

### R2: live night-mode refresh can outlive its stock session

On the first IDR the receiver calls `alt_refresh_night_mode(slot->session)`
(`hook/altscreen/altscreen_hook.c:1207-1215`). That function dereferences the
stock session/server and makes a synchronous stock callback
(`:719-756`). Full teardown revokes command ownership and waits 1800 ms for the
receiver, but deliberately proceeds if that bound expires (`:1678-1778`).

If the platform property callback blocks longer than that bound, stock teardown
can free the session/server while the receiver call is still live. Normal runs
show the callback completing immediately, so this is not the current root
cause, but the lifetime contract is unsafe.

Implemented correction: the receive thread no longer calls the stock server. It uses the
night-mode value already cached by `/info`/SetNightMode and lets later
generation-scoped updates correct it.

### R3: two Java writes still run synchronously on stock callbacks

`CombiMapController.processModelUpdateEvent()` calls
`AltScreenModule.setViewAreaMode()` directly
(`java_patch/de/esolutions/hmi/widgets/audi/evo/high/widgets/CombiMapController.java:101-140`).
That path writes and flushes both to the renderer control socket
(`AltScreenModule.java:169-192`, `AltscreenControlServer.java:42-64`) and to the
hook bus (`CarplayBus.java:118-153`). The steering-wheel BAP callback similarly
calls the direct hook-bus write through
`AltScreenCombiBAPListener.java:49-56` and
`AltScreenModule.java:91-101`.

Java sockets have no write deadline here. Small loopback commands normally fit
the socket buffer, so this is a low-frequency HMI-freeze risk rather than an
observed RTSP failure. It nevertheless violates the lifecycle rule already
used by `RendererServer`, whose stock callers only enqueue and whose dedicated
writer owns blocking I/O (`RendererServer.java:232-273`, `:551-579`).

Implemented correction: `CarplayBus` and `AltscreenControlServer` now use bounded,
drop-oldest writer queues. Stock Navigation/HMI callbacks must only publish the
latest desired state and return.

### R4: replacing a stale renderer consumes half the SI startup budget

When initial adoption finds an altscreen renderer PID without port 19831,
`cp_kill_renderer()` waits four seconds before SIGKILL
(`deploy/smartphone_integrator/carplay_processes.sh:90-105`). Startup then adds
the fixed one-second EGL serialization delay before it can exec dio
(`carplay_startup.sh:123-150`). The SI child startup budget is ten seconds
(`carplay_child.json:9`).

This path can consume five seconds before stock `dio_manager` even starts. It
is not present in the captured successful timeline, but it makes cold recovery
needlessly fragile.

Implemented correction: initial stale-renderer replacement uses a one-second
grace, while the four-second grace remains reserved for explicit system
shutdown. Renderer-internal watchdogs now make a persistent live-but-wedged PID
terminal as well.

## Components cleared as primary session killers

- **altScreen teardown join:** bounded. `alt_reap_thread()` waits at most
  1800 ms for `threadExited` and joins only an already-terminal thread
  (`hook/altscreen/altscreen_hook.c:888-959`). It cannot explain a 60-second SI
  watchdog by itself.
- **C hook bus writes:** isolated on a writer thread and protected by a 500 ms
  send timeout (`hook/framework/bus.c:450-489`). A dead Java peer does not pin
  the stock iAP2 dispatch thread.
- **RGI SendIAP2 retry:** isolated on the process-lifetime injection worker;
  no framework lock is held across the synchronous stock call
  (`hook/framework/hook_framework.c:85-197`). A stuck stock call can strand
  that worker, but not dio's transport/watchdog thread.
- **fast-reconnect altav ownership:** the current monotonic generation owner,
  generation-scoped config/forward/shutdown and opportunistic slot GC prevent
  an old receiver from closing the new renderer transport
  (`hook/altscreen/altscreen_av.c:38-96`,
  `hook/altscreen/altscreen_hook.c:962-987`, `:1462-1513`).
- **Java CarPlay activation:** hot TerminalMode callbacks only publish desired
  state (`TerminalModeBapCombi.java:89-143`, `CarPlayApp.java:78-110`). A daemon
  worker serializes module stop/start, applies a 400 ms deactivate debounce and
  retries late services indefinitely (`CarPlayApp.java:178-260`, `:305-404`).
  The former ABBA lock order is absent (`:317-322`).
- **SI stop/restart timings:** current `stopTimeout=8000` and
  `restartDelay=3000` (`carplay_child.json:10-20`) are no longer the old
  2 s/500 ms graphics-thrash configuration.
- **latest captured teardown:** no `TIMEOUT_WATCHDOG` occurs in the supplied
  `live_20260812*` logs. The failed run reaches stock's ten-second screen
  accept timeout first; `Shutdown signaled: 1` follows it.
- **maneuver renderer transport:** Java sends to it on a dedicated writer, so a
  dead process does not synchronously block stock HMI. Its own heartbeat and
  EGL swap share one render loop (`maneuver_render/main.c:749-769`,
  `platform_qnx.c:351-371`), so an Adreno hang can leave maneuver graphics dead,
  but that does not produce the captured stock-110 accept failure.

## Ranked on-unit validation plan

The code corrections are complete; release is now gated by destructive/fault
timing that cannot be proven off-unit:

1. Cold boot and ten rapid unplug/replug cycles. Require every successful
   response to report main 110 on 6030 and cluster 111 on 6031, with no stock
   ten-second `_ScreenThread` accept timeout.
2. Delay the phone's 111 TCP connect, then TEARDOWN repeatedly. Require the
   finite accept loop to publish `threadExited`, all four hook slots to remain
   reusable, and SETUP hook time to remain below the SI budget.
3. Kill/restart each renderer while video is active and while OMX is starting.
   `dio_manager` and stream 110 must remain alive; a renderer may exit with the
   documented isolated watchdog codes 86-93 and restart after backoff.
4. Stall/disconnect Java ports 19810/19832 while spinning the steering-wheel
   controls. HMI callbacks and the SI watchdog must stay responsive; stale
   generation commands must be discarded on reconnect.
5. Reproduce the exact USB PPS mismatch three times before any RTSP marker.
   Require exactly one `reset port 3 250 1` on the next generation, and prove
   that any successful SETUP or detached/non-stuck PPS state rearms it.

## Lifecycle matrix (current worktree)

| Phase | Current verdict | Evidence / remaining validation |
|---|---|---|
| USB attach and SI classification | Guarded recovery implemented | Exact pre-RTSP PPS signature, three-generation threshold, one-shot latch; replay on HU required |
| Wrapper before `exec dio_manager` | Startup budget hardened | Initial stale-renderer grace 1 s; normal persistent renderer path has no kill/recreate |
| iAP2 Identify/Auth/RGI | No session blocker found | Identify reset, retry worker and bus reconnection are generation-aware/bounded |
| RTSP control negotiation | Transactional and monotonic | Valid two-display tree exists before flag; stale old SETUP cannot replace new advert |
| RTSP 111/110 listener setup | Captured root corrected | Native mapping restored: 110=`-6030`, 111=`-6031`; on-unit port log required |
| Active 111 video | Blocking paths isolated | Socket reader/decoder queue split; EGL/OMX startup/runtime/teardown watchdogs |
| Per-stream and full teardown | Bounded in dio | Finite accept/read ticks, terminal-only join, no stock call under `alt_lock` |
| Fast reconnect / generation ownership | Structurally hardened | Old generation cannot reclaim advert/transport or command/close the new owner |
| Java/HMI lifecycle | Callback I/O removed | Socket and file I/O owned by bounded/coalescing daemon writers |
