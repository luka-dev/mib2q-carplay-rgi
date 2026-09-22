# Stock integration seam audit

Date: 2026-08-11

Target: MU1316 / MHI2Q, QNX 6.5 ARMv7, stock `libairplay.so` 210.81
Production path: stock `dio_manager` + `LD_PRELOAD` `libcarplay_hook.so` + Java patch + external renderers

## Verdict

The central production design is technically sound. The native hook uses real, ABI-compatible seams in the shipped binaries; the altScreen advertisement and stream-111 implementation match the iOS 26.6 sender contract; the Java replacement classes preserve the stock public/protected class ABI; and the current renderer no longer posts raw Qualcomm tiled NV12 to the cluster plane.

This audit does **not** support replacing stock libairplay. `Patches/libairplay-mhi2q` is an experiment and was excluded as an authority. All native conclusions below come from the actual MU1316 binaries, stock `libairplay.so` 210.81, or the extracted iOS 26.6 binaries.

There are three remaining material hardening findings. A fourth startup finding was closed during
the K1004 investigation on 2026-08-31: hook module registration no longer runs from ELF constructors.

1. **P1 — stream-111 same-slot replacement can hold RTSP SETUP for up to 1.8 s.** The old 8 s sender stall is gone, but reconnect latency is still not strictly non-blocking.
2. **P1 — terminal-1 context protection is intentionally broader than the stock contract.** While CarPlay is considered connected, every non-owner stock context write is rejected. This prevents context theft but can also suppress a legitimate stock transition.
3. **P1/P2 — supervisor ownership has zero scheduling margin.** Old-generation cleanup waits 2 s, exactly the configured restart delay. A delayed replacement owner publication can still stop adopted renderers and cause a visible restart.

The remainder is mostly bounded hardening: an excessive ELF export surface, a fail-open process-name gate, and a few behaviors that are operationally proven on the car but not fully derivable from one phone-side binary.

## Authority and method

The following artifacts were inspected directly:

- Stock AirPlay: `Patches/carplay_vc/libairplay.so`.
- Stock MU1316 executable and Cinemo libraries from `MU1316-appimg`.
- Stock Java API: `MU1316-combined-final.jar` and its decompile.
- Exact K1004 comparison set from `MHI2Q_CN_AUG22_K1004_requested_files`.
- Current production hook, Java patch, supervisor, and renderers in this repository.
- iOS 26.6 `AirPlaySender` and `CarKit` extracted from iPhone18,2 build 23G71.

ARM/ARM64 callsites were followed in IDA, including PLT/GOT indirection. Java replacement classes were compared with `javap -protected -s`. All three current builds completed: native hook with `emutls=0`, Java target 1.4, and both QNX ARM renderers.

## 1. What `LD_PRELOAD` can really intercept

The former blanket statement “a library's calls to its own exports always bypass `LD_PRELOAD`” is false for this binary. The correct rule is: **classify every callsite as PLT/GOT or direct/local**.

| Seam | Stock implementation | Stock caller | Result |
|---|---:|---:|---|
| `AirPlayCopyServerInfo` | `0x22254` | `_requestProcessInfo@0x30034` calls PLT `0x1abb4` / GOT `0xb9910` at `0x301ec` | Interposable, even though the caller is stock libairplay itself. This is the working `/info` advertisement seam. |
| `AirPlayReceiverSessionSetup` | `0x272b8` | `_requestProcessSetupPlist@0x2f198` calls PLT `0x18c04` / GOT `0xb8e80` at `0x2f79c` | Interposable. This is why the control SETUP response and stream-111 SETUP are visible to the hook. |
| `AirPlayReceiverSessionTearDown` | `0x26b60` | Stock paths use PLT `0x19264` / GOT `0xb90a0`; setup-failure call at `0x275fc`; dio also imports it | Interposable. |
| `AirPlayReceiverSessionSetNightMode` | `0x28768` | dio `CDSICarPlay_requestNightMode@0x155848`, call at `0x15589c` | Genuine dio-to-libairplay boundary. |
| `CinemoCreateIAP` | libNmeSDK `0x5848c` | dio calls through PLT at `0x17d330` and `0x17d360` | Genuine dio-to-Cinemo boundary. |

A direct branch to a local address still bypasses symbol preemption. No future hook target should be accepted merely because it is exported: the exact caller instruction must be checked.

### Stock SETUP behavior

`AirPlayReceiverSessionSetup@0x272b8` creates the response dictionary and loops over request `streams`. Stock handles 100, 101, and 110. Type 111 reaches the unsupported branch at `0x27564–0x27580`. The hook therefore is not duplicating hidden stock altScreen support; it is supplying a genuinely absent case.

Stock `_ScreenSetup@0x26f9c` has one screen object per receiver session (`session+5136/+5140`), derives screen crypto from the 16-byte session key at `session+456`, opens a server socket, applies QoS 12, and returns `{type:110,dataPort}`. The hook's type-111 response uses the same required shape. No sender-side evidence was found that a timing port or an additional latency field is mandatory for screen 111.

## 2. Cinemo/NME and iAP2 injection ABI

The route-guidance and cover-art hooks use real Cinemo ABI boundaries, not guessed C layouts.

### NmeArray and message codec

In stock libNmeBase:

- `NmeTransport::Recv` is at `0x186ff8`; `NmeArray<unsigned char>` is `{data@+0,len@+4,capacity@+8,growth@+12}` and return value 0 means success.
- `NmeTransport::Send` is at `0x186e60`; return value 0 means success.
- `NmeIAP2Message::Decode` is at `0x120880`; it validates the `0x40 0x40` frame prefix and exact length.
- `NmeIAP2Message::Encode` is at `0x11ffe8`; it grows the same NmeArray storage with process-libc `realloc`.

Stock libNmeVfs imports all four mangled symbols through its PLT. Relevant callsites include Recv `0x12af20`, Send `0x12af70`, Decode `0xe4924`, and Encode `0xd834c`, `0xd84f8`, `0xdf49c`, `0xdf888`, and `0xdf988`. These are valid preemption seams.

The Identify-buffer resize in `rgd_resize_identify_buffer` consequently matches the stock allocator and layout. It also checks pointer, length, capacity, and an 8192-byte ceiling before touching Cinemo-owned storage ([rgd_hook.c](../../../hook/routeguidance/rgd_hook.c#L1477)).

### Injection object lifetime

Stock `CinemoCreateIAP@0x5848c` returns an ICinemoIAP object. Its vtable ABI is:

- AddRef: `0x9b60c`, slot 0.
- Release: `0x9b620`, slot `+4`.
- SendIAP2: `0x9b748`, slot `+60`, arguments `(self, session, data, size)`.

The hook captures that object only after the real factory succeeds, takes a persistent reference, then takes a transient reference around every injected SendIAP2 call ([hook_framework.c](../../../hook/framework/hook_framework.c#L545)). This closes replacement/free races without inventing a parallel iAP2 transport.

The stock outbound frame is sent first; only after successful stock Send does LocationInfo trigger a separate semantic injection ([hook_framework.c](../../../hook/framework/hook_framework.c#L698)). Thus LocationInfo is neither consumed nor used as a delayed carrier. The current 0x5200 request contains component ID plus the three stock-shaped presence TLVs ([rgd_hook.c](../../../hook/routeguidance/rgd_hook.c#L1205)).

The component-0x001E schema and the 0x520x payloads are consistent with stock libesoiap2 behavior and on-unit operation. This pass verified their transport and memory ABI; it did not fully reconstruct the matching iPhone iAP2 accessory daemon from the dyld cache. Treat exact optional RGI-field semantics as operationally verified, not independently proven by `AirPlaySender`.

## 3. iOS 26.6 altScreen contract

### Feature negotiation

The phone proposes features under request key `features` in `_apsession_requestControlSetupWithResponse` / its builder (`0x227785570`, insertion at `0x227785eb8`). It reads the HU's accepted array from response key **`enabledFeatures`** in `_apsession_ensureControlStreamSetup@0x22786db80`, lookup at `0x22786dd2c`.

During activation, `___carEndpoint_Activate_block_invoke_2` tests that accepted array:

- `altScreen` sets endpoint byte `+0x3e` at `0x2277f977c`.
- `uiContext` sets `+0x3f` at `0x2277f97a8`.
- `viewAreas` sets `+0x3d` at `0x2277f97d4`.

The current hook correctly merges `altScreen` and `viewAreas` into the existing response `enabledFeatures`, rather than reflecting every phone proposal ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L1440)).

### Cluster URL handshake

When endpoint `+0x3e` is set, activation calls `APCarPlay_CRFetchInstrumentClusterURLs` at `0x2277f9ff4`. A NULL result or error aborts activation at `0x2277f9ff8–0x2277fa000`. The returned phone-side URL dictionary is stored under **`altScreenURLs`** at `0x2277fa090` and sent in the subsequent `/info` request body.

Therefore the HU does not provide cluster application URLs to the phone. The phone fetches them from its own CarKit service, then gives them to the HU. If the phone reaches stream-111 SETUP, this phone-side fetch has necessarily succeeded.

CarKit confirms that `CRFetchInstrumentClusterURLs@0x1caffe6b8` creates a `CARInstrumentClusterURLController`, whose fetch implementation delegates over XPC via `serviceFetchInstrumentClusterURLs:` at `0x1caffc034`. No HU certificate-response handler exists in this CarKit layer.

### Display selection and required keys

`carEndpoint_setupStreams@0x2277ff304` takes the endpoint's processed display-description array at `0x2277ff830`. It uses all displays only when count is at least two (`0x2277ffa9c`) and endpoint `+0x3e` is set (`0x2277ffaac`); otherwise it creates exactly one screen stream.

`APEndpointStreamScreenCreate@0x227850b74` reads each logical display UUID at `0x227850e8c` and compares it with display 0 at `0x227850ecc`. The `CINC` at `0x227850ed8` selects 110 for the same UUID and 111 for a different UUID. A dictionary field `type:111` is descriptive; it is **not** the stream selector. AirPlaySender maps the UUID property to raw key **`uuid`** at `0x2277bb02c–0x2277bb04c`.

CarKit's physical parser has a different contract:

- `-[CARDisplayInfo initWithPhysicalScreenDictionary:]@0x1cafbed6c` requires `widthPixels` and `heightPixels` through `CRSizeFromAirPlayDictionary@0x1cafbf110`.
- It requires `widthPhysical` and `heightPhysical` through `CRPhysicalSizeFromAirPlayDictionary@0x1cafbf590`.
- It reads physical identifier **`uid`** at `0x1cafbedf0`; missing/wrong type rejects the display.
- It parses `properties`; comparison with `Instrument Cluster` at `0x1cafdea70` sets instrument-cluster support.
- It reads `initialVideoStreams` at `0x1cafbef9c`; an invalid stream list rejects the physical display.
- `CARSessionConfiguration@0x1cafba0dc` gets physical panels from **`displayPanels`** at `0x1cafbaf8c`; its gauge-cluster predicate is evaluated at `0x1cafbb1d8`.

The current cluster dictionary provides both `uuid` and `uid`, distinct from the main UUID, plus pixel/physical geometry, `properties:["Instrument Cluster"]`, view areas, and `initialVideoStreams` ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L288)). Both logical `displays` and physical `displayPanels` are injected ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L510)). This shape matches both consumers.

One source comment is stale: [altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L175) says gauge-cluster state comes from `displays`; on 26.6 the CarKit gauge predicate is driven by parsed `displayPanels`. The emitted data is correct; only the comment should be corrected.

### Commands and screen crypto

`carEndpoint_handleEventCreatingReply@0x2276d8214` parses command `type` plus `params`.

- `forceKeyFrame` reads params key **`uuid`** at `0x2276d86a4–0x2276d86bc`. The current hook is correct; `displayUUID` would be wrong.
- `carEndpoint_updateViewArea@0x227802b90` requires viewAreas feature byte `+0x3d`, then reads `uuid`, `viewAreaIndex`, `adjacentViewAreas`, and `animationDurationMillis` at `0x227802be4–0x227802c4c`. The hook sends all four ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L1700)).
- `carEndpoint_setNightMode@0x22780305c` reads optional `uuid` and `nightMode` at `0x2278030dc–0x22780314c`; the hook matches it ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L1820)).

The sender handles stream types 110 and 111 through the same screen crypto path in `_apsession_setupStreamCreatingResponseInternal@0x2276e5250`, with shared derivation at `0x2276e5df8–0x2276e6080`. Stock receiver `AirPlay_DeriveAESKeySHA512ForScreen@0x2d5fc` derives labels `AirPlayStreamKey%llu` and `AirPlayStreamIV%llu` from the session AES key and `streamConnectionID`; the hook calls this exact stock function.

Stock `AirPlayReceiverSessionScreen_ProcessFrames@0x2b154` reads a 128-byte header, decrypts opcode-0 bodies with AES-CTR, sends their data to ScreenStream, and handles opcode 1 as unencrypted codec configuration. The self-contained stream-111 receiver follows that same wire format. `showUI` and `changeMapZoomLevel` were not fully traced to their final CarKit/DashBoard consumer in this pass; their ACKs and real Maps content on the unit remain the authoritative operational proof.

## 4. Stream-111 ownership and reconnect

The previous global-renderer-socket race has been materially fixed:

- `altav_claim` accepts only a strictly newer generation under its lock ([altscreen_av.c](../../../hook/altscreen/altscreen_av.c#L62)).
- Configuration, frame forwarding, and shutdown check ownership inside the same lock ([altscreen_av.c](../../../hook/altscreen/altscreen_av.c#L201), [altscreen_av.c](../../../hook/altscreen/altscreen_av.c#L242)).
- The generation is assigned only to a successfully created receiver; response insertion succeeds before ownership is claimed ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L1335)).
- A superseded receive thread checks ownership before data forwarding and before control commands, then uses owner-scoped shutdown ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L1011)).

Two bounded risks remain:

1. A same-slot replacement calls the synchronous reaper before setup. Its ceiling is `ALT_REAP_WAIT_MS=1800` ([altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L741), [altscreen_hook.c](../../../hook/altscreen/altscreen_hook.c#L787)). This is much safer than the previous 8-second path, but it can still consume a meaningful part of a 10-second startup budget.
2. Renderer socket backpressure is declared stalled after 1000 ms ([altscreen_av.c](../../../hook/altscreen/altscreen_av.c#L26)). If cold OMX startup leaves a small QNX send buffer full for longer, a healthy new generation can be treated as disconnected. This threshold needs a reconnect stress test on the unit.

An older, slower control SETUP could theoretically finish after a newer one and still return negotiated feature keys even if its transport ownership claim is rejected. RTSP requests are normally serialized, so this is a low-probability protocol-state edge, not evidence of the observed regular startup behavior.

## 5. Decode and presentation path

The current renderer is not the old black zero-copy design. Its actual path is:

1. Request native Qualcomm 32m output `0x7f000004` ([sink_omx_qnx.c](../../../altscreen_render/sink_omx_qnx.c#L337)). Linear NV12 is deliberately not requested because this decoder cannot complete Loaded→Idle with it.
2. Allocate native tiled Screen buffers and bind their pointers to the decoder with `OMX_UseBuffer` ([sink_omx_qnx.c](../../../altscreen_render/sink_omx_qnx.c#L421)).
3. Import the exact decoded Screen buffer as an EGL external image, sample it with the GPU, and draw to a managed RGBA8888 plane-99 window ([sink_omx_qnx.c](../../../altscreen_render/sink_omx_qnx.c#L156), [screen_egl_present.c](../../../altscreen_render/screen_egl_present.c)).
4. Recycle the OMX output buffer only after synchronous GPU presentation returns ([sink_omx_qnx.c](../../../altscreen_render/sink_omx_qnx.c#L202)).

The callback-close gate waits for active callbacks before destroying OMX/Screen arrays ([sink_omx_qnx.c](../../../altscreen_render/sink_omx_qnx.c#L95)). Presentation health is asserted only after a successful RGBA swap; three consecutive failures arm output-only recovery. These mechanics address the earlier use-after-free and “VIDEO_ON merely means TCP connected” failures.

The maneuver renderer independently creates a managed RGBA8888 window 98 and creates its Screen context before `eglGetDisplay`, which is required by this Adreno/QNX stack ([platform_qnx.c](../../../maneuver_render/platform_qnx.c#L56)). Both external renderers are launched with `LD_PRELOAD` explicitly cleared and `GRAPHICS_ROOT=/proc/boot` ([carplay_startup.sh](../../../deploy/smartphone_integrator/carplay_startup.sh#L47)).

The remaining renderer risks are runtime resource/ownership failures, not an unhandled tiled pixel format: EGL-image import support, plane manager disown/recreate behavior, and Qualcomm OMX pool recovery still require on-unit stress validation.

## 6. Java replacement ABI and lifecycle

The six stock outer classes replaced by the patch were compared against the stock jar with `javap -protected -s`. No stock public/protected constructor, method, or field descriptor was missing. Their superclass/interface declarations also match. Added helper and inner classes do not alter the stock caller ABI.

The lifecycle no longer performs module startup synchronously inside the stock device-listener callback. `CarPlayApp.onActivate` records desired state and wakes a daemon lifecycle worker ([CarPlayApp.java](../../../java_patch/com/luka/carplay/core/CarPlayApp.java#L78)). Module start/stop is serialized by `lifecycleLock`, but the general state lock is not held across `Module.start`, avoiding the former lock inversion ([CarPlayApp.java](../../../java_patch/com/luka/carplay/core/CarPlayApp.java#L292)). Pending modules retry quickly at first and then indefinitely at a lower rate.

`AltScreenNavStatusGate` moves service mutation to the Navigation dispatcher and installs its wrapper synchronously at the stock service-availability edge before the stock `updateAll` can publish stale navigation state ([AltScreenNavStatusGate.java](../../../java_patch/com/luka/carplay/core/AltScreenNavStatusGate.java#L73)). Route-guidance and current-position gates are separate.

### Strong context ownership

Custom cluster contexts are declared in the replacement DisplayManager. While `AltScreenModule.isConnected()` is true, `DisplayManagerMIB2High.switchContext` rejects every terminal-1 physical context write except the serialized AltScreen worker's own write ([DisplayManagerMIB2High.java](../../../java_patch/de/audi/tghu/fwhmi/DisplayManagerMIB2High.java#L290)). The worker reconciles every 250 ms and restores stock context/rate on release ([AltScreenModule.java](../../../java_patch/com/luka/carplay/core/AltScreenModule.java#L402)).

This is effective against stock context theft, but it is deliberately stronger than any stock ABI guarantee. `isConnected` is intent-based (`connected || videoAvailable`), so it can block a legitimate stock terminal-1 transition while CarPlay is connected but video is recovering. This should remain a conscious product policy and be validated for warning/safety screens, not treated as a transparent patch.

The map-scale sentinel is also presentation behavior, not a proven stock-nav control command. `GatedCombiService` publishes the hidden form `updateMapScale(15,false,0xffff,0xff,false)` and a hidden altitude sentinel ([GatedCombiService.java](../../../java_patch/com/luka/carplay/rgd/GatedCombiService.java#L43)). Stock `CombiBAPListener.setMapScaleSetting` changes native map setup before status resend; the emitted hidden BAP status itself does not call that setter. Therefore historical “Navigation currently unavailable” flashes during wheel zoom cannot be attributed to the sentinel from code alone.

## 7. Startup, cleanup, and process ownership

The wrapper design preserves smartphone_integrator's required PID: it starts/adopts renderers, forks a monitor, then `exec`s dio_manager with the hook only in dio's environment ([carplay_startup.sh](../../../deploy/smartphone_integrator/carplay_startup.sh#L122)). `IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production` remains in the child definition.

The cleanup script correctly avoids global renderer PID files and the global stop token because smartphone_integrator does not identify which dio generation it is cleaning ([carplay_cleanup.sh](../../../deploy/smartphone_integrator/carplay_cleanup.sh#L1)). The owner monitor snapshots its own renderer PIDs and stops only those if it remains owner.

One race remains: the old monitor waits exactly two seconds for a replacement owner ([carplay_startup.sh](../../../deploy/smartphone_integrator/carplay_startup.sh#L98)), while the child restart delay is also two seconds. Scheduling, shell startup, an initial `pidin`, or an unhealthy-renderer probe can delay owner publication beyond the grace period. The old monitor may then stop renderers which the replacement intended to adopt; the new monitor will restart them, but stream 111 and the EGL planes visibly reset. Use a larger grace than restartDelay or an atomic generation-specific owner token.

The helper script uses QNX-available commands and paths; it does not assume GNU options. Its recorded-PID check validates `/proc/<pid>` existence but not executable identity. That is a bounded PID-reuse risk. The initial health check is also intentionally fail-open when `netstat` returns no usable data ([carplay_processes.sh](../../../deploy/smartphone_integrator/carplay_processes.sh#L73)).

## 8. Remaining hardening findings

### FIXED: pre-Identify module/runtime initialization

The standalone RGI hook installed on K1004 had an ELF constructor which called `rgd_init()` during
`dlopen`. Module registration logged immediately; the logger's lazy initialization could therefore
create a pthread while QNX `ldqnx` still held the loader lock, before the sender could reach Identify.
The combined hook also had eager RGD, altScreen, and audio module constructors. All module registration
now happens through one `pthread_once` at the first real Cinemo interpose. The build requires a
four-byte `.init_array` containing only the compiler `frame_dummy` entry and rejects the former module
constructor/destructor symbols. A one-time WARN log marks this boundary:
`lazy runtime init complete (constructor-free; first Cinemo boundary)`.

Static comparison of the supplied K1004 files found the public DIO/Cinemo seams used by the hook ABI-
compatible with MU1316; no missing dynamic import explains the pre-Identify stop. K1004's private
`libairplay.so` layout does differ. The MU1316-only type-103 request-dispatch patch now requires an
exact MU1316 code fingerprint and fails closed on K1004 or an unknown image, instead of advertising a
103 session which the private handler cannot sustain.

### FIXED: eager cover-art constructor

The cover-art loader constructor and detached worker have been removed. The framework now installs the receive sink from its existing lazy first-Cinemo-call boundary ([hook_framework.c](../../../hook/framework/hook_framework.c#L383)); no thread is created there. The first complete image starts one joinable latest-wins worker ([coverart_hook.c](../../../hook/coverart/coverart_hook.c#L747)), and shutdown unregisters the sink, quiesces copied callbacks, then joins the worker before stopping the bus ([coverart_hook.c](../../../hook/coverart/coverart_hook.c#L1041)).

The framework also protects sink/reset pointer publication and copying with its mutex, while invoking callbacks outside that mutex ([hook_framework.c](../../../hook/framework/hook_framework.c#L108)). A stale pointer copied immediately before unregister is harmless because the cover-art runtime callback counter rejects/waits it during shutdown.

### P2: excessive ELF export surface

The production shared object intentionally interposes nine symbols, but currently exports roughly 125 defined dynamic symbols. No extra collision with the inspected current firmware was found, yet every exported helper is a future accidental-preemption surface. Build with hidden default visibility and mark only the nine hook entry points default-visible.

### P2: fail-open process gate

`hook_process_is_dio_manager` returns true when `/proc/self/cmdline` cannot be opened/read ([hook_framework.c](../../../hook/framework/hook_framework.c#L22)). The startup script now restricts `LD_PRELOAD` to dio_manager, so this usually does not matter. Fail-closed is nevertheless safer if an environment regression causes helpers to inherit the hook.

### P2: framework state concurrency assumption

The ICinemoIAP object and injection call are protected, but not every read/write of the shared `g_fw.ctx` fields is under one state lock. Stock normally serializes the relevant NME operations, and no current log proves a race. Still, that serialization is an external assumption. A future refactor should either document the single-threaded stock callbacks with call-thread evidence or make Identify/session state atomic/locked.

## 9. Release decision and verification matrix

Core altScreen protocol and rendering: **confirmed**.

Hook ABI against current MU1316 stock: **confirmed**.

Java outer-class ABI: **confirmed**.
Cold-boot/reconnect hardening: **module and cover-art loader blockers closed; remaining script/reconnect findings above still require on-unit validation**.

Before release, perform these focused checks:

1. Cold boot with the phone already connected: no worker/logging activity before the first real NME callback; dio reaches control SETUP every time.
2. Twenty quick unplug/replug cycles: SETUP wall time, assigned/claimed stream generation, and owner-scoped shutdown; no setup delay near 1.8 s.
3. Cold OMX start with renderer socket pressure: no false disconnect at the 1000 ms send-stall threshold.
4. Kill/restart dio exactly at the two-second child restart boundary: the old monitor must not terminate adopted renderer PIDs.
5. Force EGL swap and plane-disown failures: VIDEO_OFF only after real present failures, output-only recovery restores RGBA without rebuilding healthy OMX buffers.
6. While CarPlay is connected but stream 111 is unavailable, exercise every cluster warning/status path and verify the blanket context guard does not hide required stock UI.
7. Verify exported symbols after visibility hardening: only the nine intended interposes remain globally defined.

## Bottom line

The project is no longer relying on a fictional stock altScreen implementation or on undefined `LD_PRELOAD` behavior. The decisive seams are present in the exact shipped binaries, and the iOS 26.6 gate from accepted feature through two displays to type 111 is matched correctly. Module registration and cover-art are now absent from the loader phase. The largest remaining startup risks are the bounded same-slot 111 reaper, the 1000 ms renderer send-stall threshold, and supervisor-generation ownership around the two-second restart boundary—not the negotiated CarPlay protocol or the tiled-to-RGBA video path.
