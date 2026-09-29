# MU1316 btstack launcher path (offline, 2026-09-28)

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/graft/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Component flow

```mermaid
flowchart LR
    accTitle: Factory launcher in QEMU
    accDescr: The factory launcher and Bluetooth client reach the stack registration path in a reduced emulator; a diagnostic persistence bypass is not vehicle validation.
    services["Factory service manager"]
    launcher["Connectivity launcher"]
    btstack["Spawn exact btstack image"]
    client["Bluetooth client readiness"]
    gate["Persistence gate in QEMU"]
    graft["Observe graft registration"]
    services --> launcher --> btstack
    services --> client --> gate
    gate -->|Diagnostic bypass| graft
    btstack --> graft
```

In the initial loader-only QEMU mesh, the grafted ELF loaded but its state
remained `once=0`; that test alone did not exercise SDP/RFCOMM registration.
The factory-client run documented below subsequently reached the hook and
completed graft initialization. IDA identifies the
enclosing function, `0x11EA20`, by its `"BluetoothServices"` and
`"startBtStack"` format arguments at `0x11EA70`; it is
`BluetoothServices::startBtStack`, not process startup. Its path makes several
initialization calls at `0x11EB50..0x11EBC4`, then reaches the
hook after `0x11DDC8`, `0x133140`, and `0x14B2A4` at
`0x11ECA0..0x11ECB0`. The hook displaces the `LDRB R3,[R5,#0x764]` at
`0x11ECD0`. Thus `once=0` specifically says the factory start command has
not reached this method; it does not mean the modified ELF failed to load.

The target `btstack`'s `main` at `0x1188D8` (decompiled in the MU1316 IDB)
asserts `argc >= 3` at `0x118AA0`, parses **`argv[2]` as a decimal instance
ID** with `strtol` at `0x118AB4`, and registers a
`LauncherClientServiceRegistration` for interface UUID
`fe1720e3-31d6-4eb8-8a64-b6ed89dc4461` with that ID. The value `131072`
in `connectivity.json` is its application **bitValue**, not evidence that it
is the launcher instance ID. The standalone QEMU loader wrapper now defaults
to diagnostic instance ID `0` and forwards both arguments if a launcher
supplies them.

The factory `connectivity_launcher` has ID 315 in MU1316
`framework.json`. Its `main` at `0x108890` calls `servmngt::init` and then
`servmngt::startApp`; it cannot be started like an ordinary standalone app.
The MU1316 `libservmngt.so` function
`servmngt::BLCommandlineArguments::BLCommandlineArguments` at `0x19F38`
parses `-sm-id`, `-sm-name`, `-sm-restart`, and `-sm-time`, and marks the
command line invalid unless the first ID is nonzero, the name and time are
present, and the restart field is set. An actual QEMU run with no arguments
failed at `servmngt::init` and then asserted `no AgentStarter instance?`.
Supplying diagnostic values
`-sm-id 315 -sm-name connectivity_launcher -sm-restart 0 -sm-time 1`
gets past that check and into `PersistenceServicesLifeCycleListener`, but no
INIT/RUN command was observed.

The factory `dsistartup.json` initializes and runs `connectivity_launcher`
before setting its runmode to 925184. The `connectivity.json` launcher entry
then owns the `btstack` lifecycle; its `btstack` entry also requires
`/tmp/mvloaded` before launch. Reproducing this service-manager command path,
including the call to `BluetoothServices::startBtStack`, is the next QEMU
step. Merely passing two arguments to `btstack` leaves its workers idle and
does not reach the graft hook. A synthetic direct call to the hook before
this method has initialized the Blue SDK would test the trampoline only; it
would not validate RFCOMM registration.

The launcher configuration also identifies the *client* that issues the
start call. Its `initRunMode` is `201216` (`0x31200`): it includes the
`btstack` bit `131072` (`0x20000`) but excludes the `bluetooth` bit `2048`
(`0x800`). The later `set_app_runmode` value `925184` (`0xE1E00`) includes
both bits. In the factory `eso/bin/apps/bluetooth`, IDA's
`CBT_APP::checkBluetoothAppState` function at `0x1399D0` logs
`Starting BT stack. CL-Version: 10.735.888` near `0x139E18`, loads a
method from its live `BluetoothServices` proxy at `0x13ABEC`, and calls it.
That call passes three 16-bit values and a device-record array, matching the parameters read
by `btstack`'s `BluetoothServices::startBtStack` at `0x11EA20`. Its branch
requires the Bluetooth app's persistence-loaded byte (`a1+1396`), a non-null
proxy at `a1+1368`, and a successful proxy `isAlive()` check. This links the
missing QEMU step to the factory `bluetooth` client, after the launcher has
brought up `btstack`; starting only the server cannot reach this branch.
The exact interface-message dispatch was later observed in the expanded
factory-client QEMU run described below.
An optional `qemu_loader.py --variant graft --storage shmem --probe-bluetooth`
run now stages an executable copy of the factory `bluetooth` app. It enters
`PersistenceServicesLifeCycleListener` without an immediate loader failure.
A state-probe process started before the guest runs out of reliable late
`exec` capacity reads the graft after that client has initialized: the code
bytes still match and `once=0`. This is consistent with the absent launcher
INIT/RUN cycle and the readiness/proxy gates above; the log alone does not
identify which gate prevented the call. The first attempt hit `EACCES (13)`
because the extracted factory app lacked its executable bit; the diagnostic
image now sets that bit only on its temporary copy. A first immediate
`graft_state_probe` may fail with `errno 2` in this large QEMU IFS; the
prestarted late probe provides the post-client sample.

<a id="factory-startup-in-the-disposable-qemu-mesh"></a>

## 🧪 Factory startup in the disposable QEMU mesh

The MU1316 `servicemgrmibhigh` image contains the `default_startup_mode`
configuration key. Its startup branch at `0x1192E4..0x119314` compares that
configured string with the literal `on_startup_normal` at `0x1752A8` and
enters the matching startup path. Merely setting `wait_for_ooc=false` did
not start applications. The optional `--probe-startup-config` mode now
stages a temporary factory `dsistartup.json` with
`default_startup_mode="on_startup_normal"`, `wait_for_ooc=false`, and
display-manager supervision disabled. These are QEMU simulation settings,
not proposed head-unit configuration changes.

With that mode, the real service manager executes the factory startup list.
In this mesh it runs from `/`, so factory `framework.json`'s relative
`./bin/apps/connectivity_launcher` path needs a corresponding `/bin/apps`
entry in the test IFS. Once staged, `connectivity_launcher` is spawned by the
service manager, reads `/config/connectivity.json`, and connects to
`/dev/eso/connectivitylauncher`. It then waits on the factory `btstack`
precondition `/tmp/mvloaded`; an IFS namespace link to `/dev/null` satisfies
that diagnostic precondition. The launcher proceeds to
`stat64("/eso/bin/apps/btstack") -> 0` and loads its launcher-client factory
`fe1720e3…`.

The same wrapper that successfully runs by direct QEMU command was staged
at `/eso/bin/apps/btstack` through an IFS link to `/proc/boot/btstack_copy`.
`dumpifs` confirms the link and executable in a reduced 25,829,380-byte
image. A passive QEMU-only `spawn()` trace shows the launcher successfully
starts `/eso/bin/apps/btstack` and receives a child PID. The `errno=22`
printed by the first probe was stale after a successful `spawn()` return.
`waitid()` subsequently reported signal 11 (`SIGSEGV`). Markers inside the
wrapper reached stage `X`, after copying the exact ELF and before `execv()`;
the early boot-script PID check had simply finished before the launcher spawn.
The wrapper inherited the service manager's QEMU-only `LD_PRELOAD` tracers
because the launcher passed `envp=NULL`. Clearing those tracers before its
`execv()` made the child's graft state readable at `0x510000` (`once=0`). The
launcher later reaped the child after signal 9; factory service readiness is
not established in this radio-less mesh. Subsequent attempts to start the
IFS path still return `ENOENT`, consistent with the mesh's known late IFS
exec instability. A separate
`--probe-exec-path` run starts **that exact path directly** before the
service manager: the wrapper executes, the graft code bytes match, and the
late state probe reads `once=0`. The factory launch now proves that the
grafted ELF maps when the QEMU trace preloads are removed from the child; it
still does not drive `BluetoothServices::startBtStack` or the RFCOMM hook.
The direct-path run is not a
test of the factory lifecycle or RFCOMM registration.

The revised `--probe-startup-config` check reads the first 16 graft-code bytes
at `0x508000` and state words at `0x510000` from the launcher's live child via
`/proc/PID/as`, and compares the code against the exact offline build. It now
passes for the grafted image; the same factory-spawn test with stock btstack
also reaches a readable child process. Both later receive signal 9 in this
QEMU mesh. Neither run exercises the `startBtStack` hook.

The factory `connectivity_launcher` also tries to spawn `/eso/bin/apps/bluetooth`
once runmode includes its bit. Staging the unmodified client through the same
QEMU-only `/dev/shmem` wrapper gives it a live, readable process beside
btstack. The launcher later reaps both after signal 9. Thirteen passive state
samples across this interval all read graft `once=0`, so merely spawning the
client does not enter `BluetoothServices::startBtStack`.

The shortened mesh originally omitted the service manager's
`./bin/apps/persistenceTNG_mibhigh`. Staging its exact ELF through a QEMU-only
wrapper caused the service manager to publish `/dev/eso/persistence`, but the
service initially reported a missing factory for interface UUID
`e0f8829c-7990-5dd2-b13d-5d15671ccd6b`. Including the matching factory
library removed that error. Bluetooth and btstack still reached only
`once=0` in thirteen samples. The precise remaining client readiness gate is
not yet identified. Injecting even the passive OSAL mesh tracer into the
Bluetooth client made that diagnostic child exit on signal 11, so negative
results from that variant cannot establish application behavior.

A disposable QEMU copy of the Bluetooth ELF can instead replace one verified
ARM instruction with `UDF #0`; the original factory file is never modified.
Trapping at `0x139DD0` (persistence/proxy gate within
`CBT_APP::checkBluetoothAppState`) and then at the function entry `0x1399D0`
did **not** produce `SIGILL`: the launcher spawned a readable client, then
reaped it after signal 9. Thus the application never reached that method in
this mesh, rather than merely taking an early branch inside it. A positive
control trap at Bluetooth `main` (`0x1155C0`) produced `SIGILL` (`waitid`
status 4), confirming the trap mechanism catches executed code. Further
traps at `0x1159CC` and `0x1159E4` also produce `SIGILL`: `main` reaches
`comm::ServiceRegistration::registerService` and takes its success branch
into the event loop. The remaining gap is after launcher-client registration;
the next diagnostic should identify why no lifecycle callback invokes
`checkBluetoothAppState` before the launcher kills the client.

The factory Bluetooth vtable at `0x252FC0` identifies `initApplication`
at `0x120A4C` and `runApplication` at `0x136DB8`. The latter creates timers,
sets up action management, reads and subscribes to persistence, connects the
SWaP/FEC proxy, then registers Bluetooth information/control services before
starting its DSI interfaces. QEMU traps prove `runApplication` is entered,
reaches the persistence read at `0x136E68` and the
`BluetoothInformationService` registration call at `0x136ECC`, and then
takes the failure branch at `0x137068`. Neither the later control-service
call at `0x136EF4` nor the success branch at `0x136FF8` executes. The
remaining failure is therefore within BluetoothInformationService
registration; the run fails before any observed persistence callback or
`startBtStack` call.
`comm::ServiceRegistration::registerService` in factory `libcomm.so` returns
zero for success and a nonzero status for its failure path; a QEMU-only
interposer is used to capture this return without altering the factory ELF.
In the baseline reduced mesh it records three calls: an earlier registration
`self=0x265220` returns `0`, the launcher-client registration
`self=0x258EEC` returns `0`, and the information-service registration
`self=0x259364` returns `1`. The last pointer equals the Bluetooth singleton
base `0x258EE8` plus the registration-member offset `0x47C`; this independently
confirms the failing callsite. The information-service interface UUID is
`133c937f-bbad-4723-a502-4020cab96d54`, and its factory library was
absent from the reduced mesh's selected factory set. The QEMU staging list
now includes that library and the adjacent BluetoothControl factory for the
next run; their effect must be measured separately from this baseline.
With those two factory libraries present, the trace changes to success for
`self=0x259364` (information service) and `self=0x2592E0` (control service).
The next call, `self=0x25904C`, returns `1`. Relative to the singleton base,
that is offset `0x164`, exactly the registration member used by
`startDSIBluetooth` (`0x132C64` -> `0x209BE8`). Its interface UUID is
`7019ab65-bfac-5d07-8e83-e9a702af571a`; that factory is now staged for
the next disposable run. This A/B sequence shows that the earlier failure
was a missing BluetoothInformationService factory in the shortened QEMU mesh,
not a defect in the factory Bluetooth client.
Adding the factory libraries for DSIBluetooth (`7019ab65…`),
DSIMobileEquipment (`9afc0af0…`), and DSIObexAuthentication (`2d68efa1…`)
makes all seven observed service-registration calls return zero. A trap on
`runApplication`'s success branch at `0x136FF8` then fires. This proves the
factory client reaches successful application initialization in the shortened
mesh; it still does not, by itself, prove that the later persistence callback
or `startBtStack` runs.
With the same factory libraries, a trap at
`CBT_APP::checkBluetoothAppState` entry `0x1399D0` now also fires. Earlier
negative entry-trap results were caused by incomplete factory registration
in the reduced mesh. The remaining check is whether this method passes its
persistence and live-proxy gates to call `startBtStack`.
The next trap at `0x139DD0` also fires, showing execution reaches the
persistence-loaded test after the preceding state checks. The branch at
`0x139DDC` goes directly to `0x13A20C` when the accessor at `0x156E14`
reports false; `0x139DE0` is the next instruction only if it reports true.
The accessor reads byte `+4` of `CBluetoothPersistencyConcrete`, whose
constructor at `0x157FB0` initializes that byte to zero. In this factory
client the instance is embedded at singleton `0x258EE8 + 0x570`, making
the tested byte `0x25945C`. A QEMU-only launcher probe sampled that byte
read-only through `/proc/<Bluetooth PID>/as` while the client was alive:
all 28 successful samples returned zero. The adjacent shared/proxy member
at `0x259440` was often non-null, so a missing proxy alone does not explain
the failed byte gate. This observation is limited to the reduced QEMU mesh;
the exact code path that changes the byte to one remains untraced.
To isolate the next dependency, `qemu_loader.py` can NOP the verified
`0x139DDC` branch in its disposable Bluetooth ELF. A UDF at `0x139DF4`
then fires, proving the first proxy pointer is non-null. A UDF at
`0x139E00` did not fire with only this branch bypassed in the earlier
reduced image, consistent with the first proxy `isAlive()` returning false.
NOPing the subsequent `0x139DFC` branch made the `0x139E00` trap fire,
but the eventual proxy dispatch at `0x13AC04` was still unhit. Static
disassembly shows another `isAlive()` call at `0x13A010`.

The missing server-side factories explain that result. A QEMU-only
`ServiceRegistration::registerService` interposer in `btstack` recorded
four calls. Before adding the BluetoothServices factory,
`self=0x2C2044` (singleton `0x2C2040 + 4`) returned `1`; adding exact
factory `3b8fb5ed-21b3-47c4-cafe-f6b50bdbfec1` changed it to `0`.
The next `self=0x2C27CC` call remained `1`. Static code maps it to
`GemBackend::start` at `0x1342FC`, which registers the Attributes service
from singleton member `+0x788`; adding its exact factory
`a58995e5-3a88-5f1a-9a3c-1786b9a38589` changed that call to `0`.
All four observed `btstack` registrations now succeed. These factory
libraries are QEMU staging only; they exist in the original firmware.

With both factories staged and only the persistence-ready BEQ at
`0x139DDC` NOPed in the disposable Bluetooth client, UDF traps at
`0x139E00` and then at the real proxy dispatch instruction `0x13AC04`
fire without bypassing either proxy check. An untrapped run changes the
graft's `once` field at `0x510000` from `0` to `1`; its final status at
`0x510004` is `0`, and the persistent RFCOMM granted-channel byte at
`0x510030` is `3`. In `graft_init`, status `0` is written only after
RFCOMM registration, both SDP additions, and worker-thread creation
succeed. This proves the graft's initialization path executes in QEMU
and obtains an RFCOMM server channel. It does not prove a phone can
connect, that iAP2 bytes flow, or that the radio works without its
hardware. The only bypass remains QEMU-only; the original Bluetooth
client and target btstack were not edited.
Even with factory `fec` and `diagnosis` apps launched, a 90-second QEMU
trap at `0x139DE0` does not fire. The `persistenceLoaded` callback at
`0x13B428` also does not fire in a separate 90-second run. Static
disassembly shows that callback would obtain a value from the object's
`+0x6C8` member, assign it to the `+0x570` property, and then call
`checkBluetoothAppState`. The QEMU persistence service published its node,
but the reduced image omitted `/config/persistence.json`. The exact factory
configuration is now staged in the diagnostic IFS for the next run.
That exact-config run stalled the service-manager startup before it spawned
btstack, consistent with its RCC persistence backend being unavailable in
QEMU. A second QEMU-only configuration replaced that backend and its
database path with SQLite files under `/dev/shmem`; it also failed to reach
the btstack spawn. A minimal persistence-child `open64` interposer confirms
that it did open `/config/persistence.json` successfully, along with
`framework.json`, `broker.json`, and `dsiagentmapping.json`. Thus the latter
failure is after configuration-file access, not a missing path. No SQLite
open was captured before the launcher was killed; the exact failing startup
step remains to be identified. Both configurations are optional QEMU
diagnostics; the baseline mesh omits this file so the factory client can
progress to its persistence gate.

The factory startup also tried `stat64("./bin/apps/fec")` and found no file
in the reduced mesh. An optional QEMU-only wrapper now stages its exact
228,440-byte ELF: the service manager spawns it and opens `/dev/eso/fec`.
With FEC present the client still does not reach the control-service call,
so that missing app was not the immediate registration failure. This is
diagnostic staging only, not a head-unit change.

Evidence: `out/qemu_loader_{stock,graft}_shmem.log` show live `btstack`; the
graft probe prints matching code bytes at `0x508000` and `once=0` at
`0x510000`. The optional `--probe-launcher` run is in
`out/qemu_loader_stock_shmem_launcher.log`. The client run is in
`out/qemu_loader_graft_shmem_bluetooth.log`. Factory startup and direct-path
logs are `out/qemu_loader_graft_shmem_startup{,_path}.log`. These logs are from
a disposable QEMU image; no head-unit command or write was made.
The instruction-trap runs are in
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_trap_{1155c0,1159cc,1159e4,1399d0,139dd0}.log`.
Later service-registration and readiness probes are in
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_reg_trace*.log`
and the corresponding `trap_{136db8,136ecc,136ff8,137068,1399d0,139dd0,139de0}.log`
files. Persistence configuration reads are in
`out/qemu_loader_graft_shmem_startup_persistence_pconfig_localps_ps_open.log`.
The passive persistency-byte samples are in
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence.log`.
The disposable branch-bypass/UDF runs are
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_trap_{139df4,139e00,13ac04}_bypass_persistence*.log`.
Server registration A/B logs are
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_btreg_trace{_before_factory,_bluetoothservices_factory,}.log`.
The successful live-graft status and granted-channel sample is in
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_bypass_persistence.log`.
The QEMU-only `socket_probe.c` peer listened on `127.0.0.1:57342`,
closed each accepted connection, and logged accepts 1 through 4 in
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_socket_probe_bypass_persistence.log`.
The graft's `once=1`, status `0`, and granted RFCOMM channel `3`
were sampled in the same run. This proves the graft worker reconnects
after loopback peer closure in QNX; it does not exercise Blue SDK
receive callbacks or actual iAP2 bytes.

The separate QEMU-only synthetic RFCOMM run used a diagnostic graft build
that called its own callback with a connection event and `IAP2-QEMU-RX` data.
`socket_probe.c` accepted the worker's connection and checked the generation,
CONNECTED frame(s), and exact DATA payload. Its log is
`out/qemu_loader_graft_shmem_startup_factory_bluetooth_wrapped_persistence_socket_probe_synthetic_rfcomm_bypass_persistence.log`.
The graft again reached `once=1`, status `0`, channel `3`. This verifies the
in-process callback/queue/socket path only; a real Blue SDK receive callback,
phone pairing, iAP2 exchange, and MFi chip remain untested.
