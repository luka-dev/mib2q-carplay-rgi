# Intermittent AirPlay `/info` serialization failure (offline)

One strict QNX/QEMU media run returned HTTP 500 to the SDK pairing client's
encrypted `/info` request. The receiver logged `CFLValidObject` assertions,
`Unsupported object type: 0`, and failure in `CFPropertyListCreateData` while
serializing `AirPlayCopyServerInfo`. A closer read of the saved trace puts the
**first** invalid-object assertion immediately after the `bluetoothIDs`
delegate returned a non-NULL value with error zero. `AirPlayCopyServerInfo`
inserts that value into the plist and releases it; the first assertion is
`CFLRetain` at that insertion. The same trace object ID had been returned
for `rightHandDrive` in an earlier successful `/info` response. This is
consistent with a stale callback result, but the trace ID alone does not
prove pointer lifetime or exclude allocator reuse. The later
`rightHandDrive` query and binary-plist error are downstream of the first
assertion, so they do not identify the bad field. The independent client later
completed encrypted `/info` and media in the same run. Preserved log:
`out/qemu_screen_audio_pcm_stream_pairinfo500.log`.

`stage_airplay280_qemu.py` now inserts a bounded dictionary/array type walk
before `/info` serialization. It is compiled **only** when
`MHI2Q_QEMU_INFO_CFDIAG=1` is passed to `build_airplay280_qemu.sh qemu`; the
normal QEMU and on-unit binaries exclude it. The saved diagnostic receiver is
`out/airplay280_qemu/libairplay.qemu.cfdiag.so` (SHA-256
`8b1fa8e56d8c743fb6a53000bddd52f5958920f3b012e424a8a93a0eb58b495f`).
Six pairing-only QEMU runs and four screen-plus-audio runs with the diagnostic
receiver all completed. No type-0 object or `/info` HTTP 500 appeared in
`out/qemu_info_cfdiag_{1..6}.log` and
`out/qemu_info_cfdiag_media_{1..4}.log`. Each media run accepted all 80
encrypted audio packets.

The failure is unresolved. Diagnostic traversal changes scheduling, and ten
successful runs cannot establish that the original race or malformed object
has been fixed. Keep the ordinary QEMU image as the validation baseline;
use the diagnostic image only to investigate a reproduced failure. Do not
remove or coerce a property in the on-unit receiver until the invalid entry
and its source are identified.

A second QEMU-only mode compiles the same bounded walk but calls it **only
after** `CFPropertyListCreateData` has returned `NULL` for `/info`. Build it
with `MHI2Q_QEMU_INFO_FAILDIAG=1 sh
wireless/receiver_bridge/build_airplay280_qemu.sh qemu`; the resulting image
is saved as `out/airplay280_qemu/libairplay.qemu.faildiag.so` (SHA-256
`be6c0bf7fc454c2d80dae639bfe60ad18ed87e0d31d0c3b7001b0c96963db9e2`).
The build script forwards the diagnostic flags into Docker and checks for
the diagnostic marker in the linked binary. This mode adds no dictionary
walk or diagnostic write to a successful `/info` call. Four strict QEMU
screen-plus-audio runs with the verified instrumented image each accepted
80 encrypted audio packets and completed teardown; no `/info` failure
occurred, so the failure-only walk has not yet captured a bad field. Logs:
`out/qemu_info_faildiag_real_media_{1..4}.log`. The first four attempted
faildiag runs used an ordinary image because Docker had not received the
flag; they are not counted as diagnostic runs.

The ordinary QEMU image was rebuilt afterward (SHA-256
`196545d4c0434ad7eebf43f8cc3faf3aeed55a4ab2b4b23499815365b85eb89a`),
contains no diagnostic marker, and passed the same strict 80-packet media
path (`out/qemu_info_faildiag_normal_media.log`). The on-unit staged source
remained byte-identical and its library SHA-256 remains
`34b13f6f335f4c891ffb4c9df7a9eb5275f8d4702491565cfcf01ad5a839d363`.

Twelve further pairing-only runs with the failure-only image all completed
the SDK client's and independent client's encrypted `/info` checks
(`out/qemu_info_faildiag_pair_{1..12}.log`). The trace shows 36
`bluetoothIDs` callbacks, all with NULL values; none exercised the non-NULL
case that preceded the original HTTP 500. Run 2 did log one
`CFLRelease`/`CFLValidObject` assertion near the end of a successful `/info`
response, with HTTP 200. This is evidence of a separate or milder object
lifetime problem, but the log does not identify the released object. These
successful runs do not establish that the bad non-NULL `bluetoothIDs`
result is fixed.

A negative control verified the failure-only hook itself: one disposable
QEMU source copy was made to release and clear a successfully serialized
`/info` response before the failure check. Its binary
(`out/airplay280_qemu/libairplay.qemu.forcefail.so`, SHA-256
`d57279c9d7b2153bbc453c88792f01a62637336ee5869e56a75db09b2e261d74`)
logged `info-forcefail-qemu`, then `info-faildiag` and the bounded field walk,
then HTTP 500 (`out/qemu_info_faildiag_forcefail.log`). The pairing harness
rejected that run as expected. This proves the diagnostic executes when
serialization returns NULL; it does not prove that the original malformed
object is safe to walk. Restaging rebuilt the ordinary QEMU image with SHA
`196545d4c0434ad7eebf43f8cc3faf3aeed55a4ab2b4b23499815365b85eb89a`;
the forced source and both diagnostic markers are absent from it. The
on-unit receiver and review archive hashes stayed unchanged.

## Source of the non-NULL `bluetoothIDs` branch

The pinned stock `dio_manager` implementation at `0x160028` reads the
Bluetooth-ID count from `manager+0x288`. A zero count returns the not-found
error; a nonzero count creates a CF array at `0x160080`, reads its elements
from `manager+0x28C`, advances by `0x28` bytes per element, converts each
`util::UnicodeString8` to UTF-8, and appends it at `0x16018C`. This agrees
with the recovered `BtVec` header `{count, data, capacity}` at
`manager+0x288/+0x28C/+0x290` in the stock Bluetooth event handler
`0x15BE14`.

The normal DSI start dispatch calls `0x15A758(manager, payload+0x44)` on its
nonempty-entry branch. That function destroys the previous 40-byte entries
and deep-copies the DSI-supplied strings into the manager vector; the caller
then queues the AirPlay start event. The earlier QEMU repeats left this vector
empty and therefore did not test the branch seen immediately before the
saved HTTP 500. Writing a synthetic count or pointer directly into the
manager would bypass construction and destruction of the 40-byte elements.

`qemu_capture.py --seed-bt-ids-qemu` now exercises the **stock copier** on a
disposable QEMU manager before posting StartAirPlay. The capture preload
constructs one 40-byte `util::UnicodeString8` through the exported QNX
constructor, supplies the nested source-list shape read by `0x15A758`, calls
that function, destroys the source string after the deep copy, and requires
the manager vector to contain one element. This calls the stock DSI helper
directly; it does **not** exercise the full DSI event dispatcher. The QEMU
runner also requires the cleartext `/info` plist to contain exactly
`bluetoothIDs=["02:00:00:00:00:01"]`.

Eight pairing-only runs with the failure-only diagnostic receiver each
recorded three `bluetoothIDs` property callbacks, a cleartext `/info` with
the seeded ID, and HTTP 200 from both the SDK client's and independent
client's encrypted `/info`. Logs:
`out/qemu_seed_bt_ids_pair_{1..8}.log`. No CFLite retain/release assertion,
HTTP 500, or failure-only diagnostic occurred in these runs. All eight show
the separate, preexisting QEMU `_GetPrimaryMACAddress` assertion; it does not
prevent the synthetic MAC path from completing. A further run with the
ordinary QEMU receiver (`out/qemu_seed_bt_ids_media_normal.log`) passed the
same encrypted `/info` checks, an encrypted screen frame, 80 encrypted PCM
RTP packets, and screen teardown. The ordinary receiver SHA remains
`196545d4c0434ad7eebf43f8cc3faf3aeed55a4ab2b4b23499815365b85eb89a`.

These runs establish that a correctly constructed non-NULL ID array can be
serialized. They do not explain the original intermittent invalid `CFLRetain`
or prove it fixed: the real DSI event's timing and source objects may differ,
and nine successful runs cannot exclude a rare lifetime error. The following
callback-result trace instead investigates how the stock provider hands
each property's value back to the AirPlay SDK.

## Shared callback result mismatch in QEMU

The saved HTTP 500 trace was made **without** the deliberate Bluetooth-ID
seed. Its first `/info` returned no `bluetoothIDs`; the failing encrypted
`/info` then returned a non-NULL pointer for that key which had appeared as
the previous `/info`'s `rightHandDrive` value. Pointer reuse remains possible,
so that trace alone cannot identify the owner of the returned object.

`qemu_capture.py --bt-property-trace-qemu` now wraps the stock
`AirPlayReceiverServerSetDelegate` copy-property slot in the disposable QEMU
preload. It calls the original callback and reads the captured manager's
Bluetooth vector, sync flag at `manager+0x191`, and shared value/error slots
at `manager+0x1F0/+0x1F4` immediately around the call. The wrapper verifies
that the callback's context is the captured manager. Its first pairing run
is `out/qemu_bt_property_trace_airplay280.log`; twelve strict screen/audio
runs are `out/qemu_bt_property_trace_media_{1..12}.log`. All twelve completed
80 encrypted PCM RTP packets and both encrypted `/info` checks without a
CFLite assertion or HTTP 500. Across their 36 `bluetoothIDs` callbacks, the
vector was `count=0, data=NULL` before and after every call. The returned
pointer was NULL every time, but the callback's error was `0` in 20 calls
and `-6714` in 16. The pinned stock value builder at `0x160058..0x16006C`
sets `-6714` for a zero count. Thus the server callback does not always
return the current value builder's error even when the manager is unchanged.

A later single trace with the expanded slot logging showed
`bluetoothIDs` entering with sync flag `1` and shared slot `NULL/-6714`,
then returning `NULL/0` while the flag stayed `1`. In the same trace the
next property entered with the previous error in the shared slot. The stock
provider `0x158690` posts an event, calls wait helper `0x157454`, then reads
these shared slots; worker `0x160A28` clears them, calls value builder
`0x15FE1C`, writes the answer and signals `manager+0x190`. Disassembly of
wait `0x176488` shows an already-set flag at `+1` can return success without
clearing it (`0x17667C..0x17669C`). This is a concrete path for a callback
to read a prior or incomplete result. We have **not** captured the original
non-NULL stale pointer again under this wrapper, so its exact cause is still
an inference.

`--bt-property-direct-qemu` is a separate A/B experiment: the wrapper calls
the same stock worker-side value builder directly and bypasses the shared
post/wait/result slots. The actual ABI at call site `0x160A74..0x160A88` is
`value = 0x15FE1C(manager, NULL, key, NULL, &error)`; the older three-argument
reconstruction is incorrect. The first disposable QEMU attempt using three
arguments faulted in `dio_manager` and was replaced with the verified
five-argument call. With the corrected ABI, four strict media runs with an
empty vector returned `NULL/-6714` on all 12 `bluetoothIDs` callbacks;
four more runs with the stock-copied ID returned a non-NULL array and zero
error on all 12. Each run passed SDK and independent encrypted `/info`,
screen frame, 80 encrypted PCM packets, and screen teardown, with no CFLite
assertion. Logs:
`out/qemu_bt_property_direct_{empty,seed}_media_{1..4}.log`.

Direct execution changes the thread on which the value builder runs. The
QEMU probe establishes a consistent result for the exercised properties,
not full thread safety or physical-device behavior. The direct mode is
excluded from the normal QEMU and on-unit receiver builds. The on-unit
receiver SHA remains
`34b13f6f335f4c891ffb4c9df7a9eb5275f8d4702491565cfcf01ad5a839d363`;
the offline review archive remains
`9dd5324c93ed82b377dd1bfe038b9c4ed99d6c1bb39d48418fef9d5b749758a8`.

## Worker-thread signal reset and CF ownership

`--bt-property-reset-qemu` keeps the original callback and worker-side
builder. Its delegate wrapper serializes copy-property calls, locks the
stock sync object's mutex at `manager+0x1A0`, clears the signalled byte at
`manager+0x191`, unlocks, then posts/waits through the original callback.
Eight strict media runs with an empty vector and eight with the stock-copied
ID returned the expected `bluetoothIDs` value/error in all 48 calls and
completed pairing, encrypted `/info`, screen, 80 PCM packets and teardown.
But **every** run emitted `CFLRelease`/`CFLValidObject` at manager shutdown:
`out/qemu_bt_property_reset_{empty,seed}_media_{1..8}.log`. The preceding
normal-receiver seed run had the same assert. By comparison, none of the
twelve trace-only or eight direct-builder media runs had this assertion.
Signal reset alone is therefore not a usable fix.

The exact manager destructor explains the new failure path: at
`0x15B498..0x15B4A4` it releases a non-NULL CF object left at
`manager+0x1F0`. The AirPlay SDK also consumes/releases a successful
copy-property callback result. Once the reset makes the callback reliably
return the worker's current object, leaving the same pointer in the shared
slot exposes a second release at shutdown. The stock worker's assignment
at `0x160A8C` and the provider's return from `+0x1F0` at `0x158714`
confirm that this is the same object in the observed path.

`--bt-property-handoff-qemu` adds a bounded ownership transfer to the reset
experiment: under the sync mutex, it clears `manager+0x1F0` only when that
slot equals the object returned by the original callback. A mismatch is
logged and makes the QEMU runner fail. Eight empty-vector and eight seeded
strict media runs passed with the expected 48 `bluetoothIDs` responses,
screen frame, 80 encrypted PCM packets per run, teardown, and **no** CFLite
assertion or slot mismatch:
`out/qemu_bt_property_handoff_{empty,seed}_media_{1..8}.log`. One additional
seeded run with the ordinary QEMU receiver also passed
(`out/qemu_bt_property_handoff_seed_media_normal.log`). The combined QEMU
owner-wrapper/supervisor fixture with this wrapper, the AirPlay 280 receiver,
Rust daemon and synthetic phone completed iAP2, mock MFi, CarPlay Control,
stock receiver STOP and Wi-Fi IE cleanup
(`out/qemu_owner_supervisor_bt_handoff_airplay280.log`).

This is the strongest offline candidate for a production fix because it
retains the stock worker thread and prevents the observed double release.
The minimal, non-tracing implementation is now in `property_handoff.c`.
`build_offline.py --property-handoff` builds it as a **separate** candidate,
`out/libdio_receiver_bridge.handoff.offline.so` (SHA-256
`6336ad67b9ec3764bd69831347e511a00d9744fdd6afa2e2079060f007b002cc`).
It checks the exact delegate, provider, wait, signal, worker-result and
destructor instruction bytes before replacing the copy-property slot. The
bridge readiness gate requires this replacement to be installed and healthy.
The stock `libairplay.so` SetDelegate at `0x201E4` copies 15 words, but both
the on-unit and QEMU AirPlay 280 builds copy **seven** words at
`0x23CCA8+0x1C` and `0x23AEB8+0x1C`, respectively. The replacement now passes
only seven slots and checks the loaded SetDelegate copy instructions before
using a stack-backed delegate. In QEMU, `RTLD_NEXT` first reaches the
`libdioprobe.so` fixture; a QEMU-only path verifies that intermediate library
and the loaded AirPlay 280 implementation. The on-unit build requires the
direct seven-word implementation.
`verify_capture_point.py` now also pins those seven instruction ranges against
the exact MU1316 `dio_manager` on disk (14 image checks total). A disposable
QEMU build with a forced guard failure logged rejection and never logged
installation (`out/qemu_property_handoff_guard_fail.log`).

The separate production-module QEMU fixture passed a seeded-ID run with
pairing, encrypted `/info`, one screen frame, 80 encrypted PCM packets and
teardown (`out/qemu_property_handoff_production_airplay280.log`). The combined
owner-wrapper/supervisor fixture also passed through synthetic iAP2, mock MFi,
CarPlay Control, stock receiver STOP and Wi-Fi IE cleanup
(`out/qemu_owner_supervisor.log`). Neither run logged a CFLite assertion.
After the atomic readiness-state change, the seeded-ID media scenario passed
again, as did bridge START/STOP followed by stock manager teardown and retained
server release (`out/qemu_property_handoff_production_teardown.log`).
The older install archive and on-unit receiver artifact were not changed.
The capture probe clears the manager pointer at `operator delete`, after the
manager destructor. A new QEMU-only stress fixture starts a second `/info`
client, waits until a copy-property callback is active, then requests bridge
STOP. The first STOP returned `BUSY` (`3`). Deliberately forcing stock manager
teardown anyway reproduced a `dio_manager` SIGSEGV in
`AirPlayCopyServerInfo` after manager release
(`out/qemu_property_handoff_forced_busy_teardown.log`). Another repeat of
that sequence did not crash, so the failure depends on scheduling. The
corrected fixture stops new requests, waits for the first callback to drain,
retries STOP to `OK` (`0`), and only then invokes teardown; it passed without
a `dio_manager` crash or CFLite assertion (`out/qemu_property_handoff_race.log`).
This demonstrates a required STOP acknowledgement boundary, not a general
proof against all concurrent manager destruction. No real MFi, radio or
phone behavior has been tested.

The review-only handoff variant now has a terminal bridge `QUIESCE` operation
(CPRX v2 operation 3). It sets the property callback gate before waiting for
an in-flight callback, then stops the AirPlay listener and acknowledges only
after port 7000 closes. Later START requests remain rejected for that owner
generation. The guarded QEMU fixture entered a callback, sent QUIESCE, received
status 0, rejected a later START with status 1, and released the stock manager
without a crash or CFLite assertion (`out/qemu_property_handoff_quiesce.log`).
The owner helper and native supervisor require that acknowledgement before
signalling the owner. The review archive compiles this requirement into both
executables; its wrapper and environment template also set
`MHI2Q_REQUIRE_BRIDGE_QUIESCE=1`. Host tests verify that BUSY preserves the
owner and OK precedes SIGTERM. A QNX/QEMU supervisor fixture, with the runtime
flag deliberately absent, returned BUSY once for each owner generation and
then OK; wired takeover and shutdown both waited for OK before terminating
their owners (`integration/out/qemu_wireless_supervisor_quiesce.log`). An
unreachable bridge causes the guarded takeover to fail closed. Hardware
behavior and QNX6 persist-FS locks remain unverified, so this does not
promote the archive to on-unit use.

## Active media at QUIESCE: unresolved QEMU assertion

The earlier QUIESCE fixture drained an overlapping `/info` callback, but did
not keep a media session active. A new QEMU-only independent client paired,
sent one encrypted screen AVCC frame and 80 PCM RTP packets, and requested
QUIESCE before its own TEARDOWN. With the ordinary AirPlay 280 QEMU image,
two of three repeats logged two `CFLite.c:473` `CFLRelease` /
`CFLValidObject` assertions during forced session close, before QUIESCE
acknowledgement (`out/qemu_active_session_normal_{1,2,3}.log`). QUIESCE
returned 0 and stock manager shutdown completed, but this is **not a clean
active-media stop**. Three runs of an instrumented `CFLRelease` image passed;
the diagnostic build changes timing and does not disprove the race
(`out/qemu_active_session_cfrelease_{1,2,3}.log`).

The preload source now gates and drains the stock server delegate's
`copyProperty`, `sessionCreated`, and `sessionFailed` callbacks, and pins
their seven-slot delegate layout against the MU1316 callsite. This source
change was newer than the earlier guarded review archive.

The QEMU diagnostic located the two invalid releases in
`HTTPServer.c::_HTTPConnectionFinalize`: its `ForgetCF` calls for
`cnx->requestMsg` and `cnx->responseMsg` reached `CFLRelease` with the first
four bytes of each object already zero. In the diagnostic image, the return
addresses were `0x1240e0` and `0x12410c` (image offsets, immediately after
the two `CFRelease` calls). This identifies the cleanup site, **not yet the
earlier operation that invalidated both objects**. The diagnostic run is
`out/qemu_active_session_cftop_4.log`; all such instrumentation is QEMU-only.

The bridge and AirPlay 280 source now implement an idle-only stop on the
receiver's HTTP queue. It returns BUSY when `playing=1`; otherwise it closes
new SETUP admission before scheduling the HTTP server stop. The source
rejects later SETUP with 503 until the bridge resumes it for a new generation.
This replaces the separate playing read that left a new-SETUP race. The
independent QEMU client verified `QUIESCE=3` during screen/audio, sent its own
encrypted TEARDOWN (200), and retried to `QUIESCE=0`. Three ordinary-image
runs of the queue-serialized variant passed without a CFLite assertion or
manager crash (`out/qemu_active_session_serialized_{1,2,3}.log`). The QNX
receiver candidate SHA-256 is
`1fc381d03234f825544e596ac113377d9251c3cf76d44c1fca2e1aee4387a05d`;
the QNX bridge candidate is
`f6b3b4ab840068cbff47a4725a3f89d45a1abd6956b3283b4a2bfb21473022c9`.

The bridge's `/info` readiness parser had a separate bug: it rejected the
AirPlay 280 binary plist's extended dictionary count (`DF 10 0F`), even
though HTTP returned 200 and its features contained both required bits.
After bounded parsing of that extended count was added, the full
START→STOP→START→STOP QEMU fixture passed on first attempt for both STARTs
(`out/qemu_property_handoff_production_airplay280.log`). The overlapping
`/info` callback, QUIESCE, rejected-START and manager-release fixture also
passed as a full runner (`out/qemu_property_handoff_quiesce.log`). The
active-media BUSY→TEARDOWN→QUIESCE fixture passed again afterward
(`out/qemu_active_session_quiesce.log`).

An independent QEMU client also simulated an unexpected phone disconnect:
after encrypted screen setup, one AVCC frame and 80 PCM RTP packets, it
closed the control TCP socket with `SO_LINGER={1,0}` (RST) without TEARDOWN.
Three ordinary-image runs ended the session with `kConnectionErr`, released
the manager and had no `CFLRelease`/`CFLite.c` assertion
(`out/qemu_screen_abrupt_{1,2,3}.log`). `SocketReadData` logged the expected
`ECONNRESET` assertion, which did not abort the process. This checks one
abrupt-close timing. A follow-up fixture requested QUIESCE while media was
active (BUSY), closed timing UDP and control TCP without TEARDOWN, then
retried QUIESCE. Its first two runs passed, but a third hung: the AirPlay 280
timing thread repeatedly reported `ECONNREFUSED` after losing the peer UDP
port, producing a multi-megabyte log while the control-session cleanup did
not complete before the runner deadline. The SDK source uses `select()` on
the readable error socket and immediately retries. The staged receiver now
suppresses that expected receive error's assertion and backs off 100 ms in
the timing thread. With this candidate, three ordinary QEMU runs passed
BUSY→RST→`kConnectionErr`→QUIESCE OK with no `CFLRelease` assertion
(`out/qemu_active_session_rst_quiesce_backoff_{1,2,3}.log`). A fourth run
held the control connection open three seconds after closing the timing UDP
socket, then RSTed it and also passed
(`out/qemu_active_session_rst_quiesce_delay.log`). The original
BUSY→TEARDOWN→QUIESCE fixture passed again
(`out/qemu_active_session_teardown_backoff.log`). The new QEMU and on-unit
receiver images have SHA-256 `49488806e622a4044365fd047c86a96538a201a55fb8e9f0ea308b418a047175`
and `1fc381d03234f825544e596ac113377d9251c3cf76d44c1fca2e1aee4387a05d`.
The failed run's overwritten log was not preserved; this source-level cause
is supported by the SDK loop and the observed error flood, not a complete
trace of the stalled cleanup. Repeated reconnects and a physical phone
remain untested. The earlier forced-stop invalidation of both HTTP messages
also remains unexplained.

These QEMU tests and compile results do not verify physical phone, MFi,
radio or on-unit ownership. The guarded review archive was rebuilt with the
current receiver and bridge. Its manifest records each candidate hash;
host sandbox install/rollback and disposable QNX installer lifecycle passed.
It remains a review artifact, not a deployable image.

The repeat-media QEMU fixture (`qemu_capture.py --repeat-media-qemu` with the
active-session flags) now attempts two independently verified encrypted
screen/PCM sessions against one receiver and reserves `QUIESCE` for the
second session. Its first session passes pair-verify, encrypted AVCC plus 80
PCM RTP packets, and TEARDOWN HTTP 200. In the current stock-dio mesh image,
`dio_manager` then unregisters its services and exits before the second
pair-verify. The fixture now also models the bridge's generation lifecycle:
first `START=0`, first `STOP=0`, then second `START=1` (`NOT_READY`) because
the manager is being released. A diagnostic run delayed first pairing by 12
seconds; the manager stayed alive through the delay and still exited just
after the first media session (`out/qemu_repeat_media_delayed_start.log`).
That rules out a simple fixed-time shutdown in this QEMU configuration. A
QEMU-only `CFRunLoopStop` interposer then captured two calls from return
address `0x15897c` during the first media
TEARDOWN, before the bridge STOP; this is the stock stop path previously
identified at `0x158974..0x158978`. With the calls forwarded normally,
the manager releases and second bridge START returns `NOT_READY`
(`out/qemu_repeat_media_quiesce.log`). Holding only those calls in an A/B run
kept the manager alive, but after STOP the second `startServer` bound the TCP
ports and never answered the bridge within five seconds
(`out/qemu_repeat_media_held_runloop_start_hang.log`). Keeping the server
running instead let the second pair-verify finish; its first encrypted
`/info` then timed out (probe status `-3`, `read` errno 11). The dedicated
QEMU NTCP trace proves that the second pair-verify configured a fresh
encrypted transport and decrypted the 63-byte `/info` request successfully
(`err=0`). A stock property event was then enqueued, but unlike the first
`/info`, no `cb.copyProperty` completion appeared before the client's
five-second read timeout (`out/qemu_repeat_media_ntcp_trace.log`). This
locates the immediate stall after HTTP decryption, in the stock property
response path; it does not prove which worker operation is stuck. A separate
one-second cleanup-pause run had the same timeout. The second media session
remains **unverified**. These two hold modes are explicitly QEMU-only flags;
they are not in the production bridge. The ordinary one-session
BUSY→TEARDOWN→QUIESCE test still passes with the unchanged on-unit receiver
and bridge. The existing `wireless_dio_supervisor` restarts the whole
wireless `dio_manager`/daemon generation after its child exits, without
restarting `btstack`. The next integration fixture should test a *new*
generation after real media TEARDOWN; retaining this stopped stock worker in
one process is only an A/B experiment, not yet a production requirement.

## Active-media wired takeover with the owner helper

The guarded wired helper and QEMU receiver were run together with the exact
generated wrapper and stock `dio_manager`. The independent client sent an
encrypted AVCC frame and 80 PCM RTP packets. A direct DRAIN request without
the wired-priority `fcntl` lock returned NOT_READY; the wired helper then
claimed priority and received QUIESCE BUSY, DRAIN OK and QUIESCE OK. Its
wired child launched and the wireless manager exited with status 0
(`out/qemu_active_owner_wired.log`). Three earlier integrated repeats also
completed before the priority-lock gate was added
(`out/qemu_active_owner_wired_{1,2,3}.log`). None captured the previous
`CFLRelease` invalidation or a `dio_manager` signal crash. All four printed
a DispatchLite assertion for `select()` interrupted by `EINTR` as the stock
manager exited; that warning is not explained yet. The receiver's
`sessionDied` command starts stock manager cleanup before QUIESCE OK, although
the wired helper itself sends no TERM until confirmation. This test uses a
QEMU bridge bound to `lo0`, a synthetic display/audio backend and a QNX4
lock filesystem. It does not establish the timing of a physical USB event,
the real iPhone, or `/mnt/persist` QNX6 locking.

The warning came from the AirPlay 280 `__LibDispatch_SelectDrain` loop: its
QNX `select()` may return `EINTR` when the owner exits, and the source treated
that result as an unexpected error. Staging now retries the loop on `EINTR`,
rebuilding the descriptor sets and timer timeout before waiting again. An
integrated active-media USB takeover/restart QEMU run with the new receiver
completed both generations without the warning or a CF invalidation
(`out/qemu_active_owner_supervisor.log`). The earlier warning is preserved in
`out/qemu_active_owner_supervisor_pre_eintr.log`. This change does not by
itself prove crash-free teardown on physical hardware.
