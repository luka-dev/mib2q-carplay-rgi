# MHI2Q RFCOMM graft: offline build (2026-09-27)

`python3 wireless/graft/build_offline.py` produces `out/btstack.graft.offline`
from the SHA-256-pinned MU1316 target. The source btstack is read and rehashed;
the build never writes it or contacts the head unit. The output is a review
artifact, not a deployable release. QNX loader acceptance is checked below;
no live Bluetooth test has been performed.

The copy replaces the ARM instruction at `0x11ECD0` with a branch to
`graft_hook`. The trampoline saves caller-saved registers, runs `graft_init`,
replays `LDRB R3,[R5,#0x764]`, and returns to `0x11ECD4`. A second hook at
`0x12A710` augments each refreshed Extended Inquiry Response with the CarPlay
UUID, replays `STRB R0,[R6,#0x40C]`, and resumes at `0x12A714`. The code is in a new
RX `PT_LOAD` at `0x508000`; zero-initialized state is in a new RW `PT_LOAD` at
`0x510000`. A seven-entry program-header table is moved into the original
`0x1B9978..0x1BA000` file gap and the original RX mapping is extended to cover
it. Existing image bytes differ only in the ELF header, new PHDR bytes, and the
two four-byte hooks; code is appended after the original file. The old section
table remains unchanged, so ordinary section listings do not name the new
segments. Program headers are authoritative for loading.

`graft_init` allocates no heap objects: the channel, granted-channel byte, SDP
record and attributes, eight inbound frame slots, and one persistent transmit
packet live in the new state segment. It registers RFCOMM through `0x277B98`,
fills the assigned channel into the donor's iAP SDP descriptor, then calls
`0x2073C4`. It also registers the factory-style, one-attribute CarPlay SDP
record. Failure removes any preceding records and deregisters RFCOMM. Worker
creation failure removes both SDP records through `0x207344` and deregisters
RFCOMM. The RFCOMM callback
accepts event 1 only while the local daemon socket is connected, copies event-5
payloads into a bounded queue, and reuses the
static transmit packet only after event 6. Its socket I/O runs in a worker
thread. The worker grants four initial receive credits after a connected
notification reaches the daemon, then one per delivered data frame. A full
queue disconnects the iAP channel instead of silently dropping stream bytes.
Late data and duplicate disconnection callbacks after the active link has
ended are ignored before they can consume queue slots or advance the
connection generation.
The same fail-closed path now applies if a connected or disconnected event
cannot enter the queue: the worker closes its daemon socket, discards the
queued frames, and disconnects any still-active RFCOMM channel. That makes
the daemon reset its iAP2 state on EOF even when the lost frame was a
control event. Inbound daemon frames with a nonzero reserved byte are also
rejected before any RFCOMM transmit.
The one-time guard stays set after an initialization failure. This is
intentional until cleanup and retry conditions have been validated: clearing
it unconditionally could register a second channel while the first still
owns callbacks. In particular, `SDP_ADD` status `18` is a record-validation
failure at target `0x2073C4` / `0x204C38`, not evidence that the SDP server
is temporarily unready. The disposable QEMU factory-start path has
registered both records successfully, but it does not establish every
failure path or the timing of EIR publication on the real unit.
The EIR hook now checks a separate, release-published `discovery_ready` flag.
`once` becomes one before `status` leaves its zero-filled initial value; using
those fields as the EIR gate allowed a concurrent refresh to advertise
CarPlay before RFCOMM/SDP registration completed. The flag is set only after
both records and the socket worker succeed, and remains zero after any init
failure. An earlier EIR refresh is skipped; whether the stack refreshes again
afterward was previously uncertain. The target's successful `SDP_ADD` path
queues EIR refresh through `0x1DB648` before graft readiness. After the flag
is published, the graft calls that same verified Blue SDK request once more.
The donor/target call chain is documented in `../carplay_discovery.md`;
controller advertisement still needs on-unit verification.
The QNX 6.5 `POLLERR`, `POLLHUP`, and `POLLNVAL` bits all trigger the same
socket reset; their values come from the SDK's `sys/poll.h`.
A disposable QEMU run with `--probe-graft-socket` starts a loopback peer
on port 57342, closes each accepted connection, and observes four
successive connections from the graft worker. The same run reports graft
status zero and RFCOMM channel 3. This verifies the socket reconnect path
in QNX; no RFCOMM data event or iAP2 payload is synthesized by that peer.
The current production graft still made four connections after the bridge
readiness change. In the separate synthetic-RFCOMM run, the fake link sent
the expected payload, then the worker held off reconnection after socket
loss because the fake stack has no event-4 disconnect callback. This is the
intended closed state, not proof of a real Blue SDK teardown response.

A separate disposable build with `--probe-synthetic-rfcomm` invokes the
graft's callback with a synthetic connection and 12-byte `IAP2-QEMU-RX`
payload after registration. The QEMU loopback peer verified the generation-1
CONNECTED frame(s) and exact DATA payload; graft status was zero and the
granted channel was 3 in the same run. The normal offline graft remains
byte-for-byte unchanged. This tests callback-to-queue-to-socket delivery,
not a Blue SDK radio receive event, an iAP2 session, or MFi authentication.
Evidence: `out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_socket_probe_synthetic_rfcomm_bypass_persistence.log`.

The `--probe-daemon-bridge` variant now places the ordinary QNX Rust daemon
beside the graft in one disposable QEMU image. Its separate
`btstack.graft.iap2-qemu.offline` injects the six-byte iAP2 marker through
`graft_rf_callback`; the daemon reports `iAP2 marker received from RFCOMM`
after the stock launcher starts the grafted process. This verifies the direct
callback-to-daemon input path. A passive read of the graft's dedicated RW
segment then reports reply flags `0x7`: its worker received the daemon's exact
iAP2 marker and the `FF 5A` SYN prefix over the socket, and split the reply
at a synthetic three-byte peer limit. Because QEMU has no RFCOMM peer, this
variant synthesizes transmit-completion events without calling the Blue SDK
transmit function; the normal graft calls the mapped transmit entry point.
The companion standalone
daemon probe verifies marker/SYN output and the later iAP2/MFi/CarPlay path.
Evidence:
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_daemon_bridge_synthetic_rfcomm_bypass_persistence.log`.
The factory launcher later kills this QEMU child because its simulated
environment lacks real peripheral services; this run is not a stability test
on the head unit.

The worker connects to a local TCP listener at `127.0.0.1:57342`. The listener
is **not** included here; a separate iAP2 daemon must implement it. Frames in
both directions use an eight-byte little-endian header:

| Offset | Width | Meaning |
|---|---:|---|
| 0 | 1 | type: 1 connected, 2 data, 3 disconnected |
| 1 | 1 | reserved, zero |
| 2 | 2 | payload length, 0..5035 |
| 4 | 4 | connection generation |
| 8 | length | RFCOMM payload; only type 2 has data |

The daemon may send only type-2 frames with a nonzero length and the current
generation. It must close its socket on a malformed frame or stream reset.
The worker reconnects on socket failure. If a phone is connected, it requests
RFCOMM disconnection once and waits for event 4 before opening a new daemon
socket. New inbound requests are rejected while the daemon socket is absent;
an already connected iAP byte stream is held off the next daemon session. If
the stack never sends event 4, the bridge stays closed until btstack exits.
An inbound request accepted just before socket failure still needs a real
radio timing check. Outbound packets are serialized until SDK completion
event 6. The worker accepts daemon frames up to 5035 bytes and divides each
at the connected peer's `RF_MAX_PAYLOAD` limit, waiting for event 6 between
pieces. This removes the former 20-byte daemon transmit cap; real radio
throughput and completion timing remain unverified. The daemon must supply
the higher iAP2/MFi protocol; SDP/RFCOMM transport alone cannot enable CarPlay.

Static checks in `build_offline.py` verify the original digest, displaced
instruction, PHDR gap, code/state nonoverlap, ARM branch range and destination,
trampoline return literal, no unresolved relocations, ELF load segments,
permitted byte changes, and unchanged original digest. `build_report.json`
records the generated digest and placement. `readelf -l` and
`wireless/verify_graft_points.py` were also run. `test_eir.py` checks the
bounded EIR insertion against 1000 generated layouts. The factory evidence
behind the new record and hook is in `../carplay_discovery.md`. These checks do not establish
QNX loader compatibility, callback behavior under real radio traffic, or
thread-safety of the new worker against every stack lifecycle state. Those
remain blockers to installation or live activation.

`qemu_loader.py` stages either stock or grafted btstack in a disposable QNX
IFS with its direct libraries. New executables must be placed near the early
boot programs: even an identical copy of the working `devstub` failed to spawn
when appended after the mesh's large factory tree. The mesh's original libc
also lacked `__stack_chk_guard`, which factory btstack imports; the diagnostic
image now uses the MU1316 factory libc. These are QEMU-image fixes only.
With `--storage shmem`, the helper embeds the **exact** stock or grafted ELF,
copies it to `/dev/shmem`, and executes it. The diagnostic image now includes
MU1316 `connectivity.json`, starts btstack after the comm broker and
servicemgr, and moves its launcher-proxy factory `fe1720e3…` from the end of
the large IFS to the early mapped region. Both stock and graft remain alive at
the checkpoint. A read-only process-memory probe checks the first 16 bytes at
`0x508000` against the build output before interpreting state at `0x510000`;
the code bytes match and `once` remains zero. The natural startup has **not
yet reached** the hook at `0x11ECD0` in this loader-only test. The current two launcher arguments are diagnostic values;
the factory launcher has not issued a real INIT/RUN cycle. The reconstructed
launcher path and optional QEMU probe are in `launcher_lifecycle.md`. This establishes
loader acceptance and mapped graft state, not a working radio or service.
Serial logs remain under ignored `out/qemu_loader_*.log`.

The optional `--probe-bluetooth` variant stages the factory `bluetooth`
client and starts a delayed read-only state probe before the QEMU guest loses
the ability to spawn some later diagnostics. The client enters its lifecycle
listener, while the delayed sample still shows `once=0`; it has not issued
the `BluetoothServices::startBtStack` call in that early, reduced image.
The factory runmode bits and the client's proxy-call gate are traced in
`launcher_lifecycle.md`.

`--probe-startup-config` now exercises the factory service manager and
`connectivity_launcher` in a reduced disposable IFS. The real launcher gets
through its `/tmp/mvloaded` gate and successfully spawns the staged btstack
path. The initial child inherited QEMU-only tracing preloads and died with
`SIGSEGV`; the wrapper had actually reached `execv()`, while the early PID
check ran before the factory spawn. The startup probe now clears those tracers
in the QEMU-only wrapper and reads the graft code/state from the factory child
while it is alive. This startup probe now passes with matching code bytes and
`once=0`; a stock-btstack control also reaches a readable child. Both children
are later reaped after signal 9 in the radio-less QEMU mesh. A separate
`--probe-exec-path` proves the same path and exact-byte wrapper run when
invoked directly. The real launcher now starts both `btstack` and `bluetooth`.
Adding the factory Bluetooth service libraries to the reduced QEMU image lets
the client finish `runApplication` and reach `checkBluetoothAppState`.
Its persistence gate remains false; a 90-second probe did not reach the
`persistenceLoaded` callback even with factory FEC and diagnosis apps staged.
Read-only samples of the Bluetooth client confirm the gate byte stays zero.
Two missing factory libraries initially kept the `btstack` server
registrations from succeeding. With the exact BluetoothServices and
Attributes factories staged, both registrations succeed; a QEMU-only NOP
of the persistence-ready branch then reaches the real `startBtStack`
proxy dispatch. An untrapped run executes the graft, which reports
status zero and RFCOMM channel 3 after adding both SDP records and
starting its socket worker. No Bluetooth radio or phone connection
has been validated in QEMU.
Staging exact or local-SQLite persistence configuration causes an earlier
service-manager startup stall. See `launcher_lifecycle.md` for the evidence.

Direct `--storage ifs` is a separate negative result: stock btstack starts,
but the graft fails with QNX `ELIBACC` (errno 83). `mkifs` rewrites the
graft's nonstandard relocated program-header table at file offset `0x1B9978`
to offset 52 and overwrites its original `.interp` string; an extracted test
image reports the interpreter as `^A`. The planned deployment is a regular
filesystem ELF, so the exact-byte `/dev/shmem` test is more representative.
The IFS variant cannot be used to validate this graft without changing its
ELF layout or the IFS builder. Neither QEMU path has exercised the graft hook
or RFCOMM callbacks under a radio workload.
