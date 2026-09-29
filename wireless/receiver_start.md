# Receiver startup gap for wireless CarPlay

Static/QEMU assessment, 2026-09-27. The current RFCOMM graft and Rust daemon
can negotiate an iAP2 wireless control session, but they do not cause stock
`dio_manager` to start its AirPlay receiver. The `libairplay` feature-bit patch
only changes a value returned *after* a receiver exists. Merely opening TCP
7000 through PF does not create the listener.

The MU1316 `dio_manager` imports `AirPlayReceiverServerCreate`,
`AirPlayReceiverServerSetDelegate`, `AirPlayReceiverServerSetProperty`, and
`AirPlayReceiverServerControl`. The sole `AirPlayReceiverServerCreate` call is
at `0x15C518` inside `CAirPlayMainThread::run` (`0x15C214`); the runnable's
vtable at `0x1A4820` contains that entry at `+8`. The prior QEMU Track-F work
in `Tools/qnx-carplay-emu/carplay/TRACK_F_STATUS.md` and
`trackf_stock_spawn.c` reproduced this run path in the real stock dio ELF:
Create → properties → delegate → Control(startServer), producing a TCP 7000
listener bound on all interfaces. That harness used a staged software
`libairplay`, synthetic config, and a force-spawn preload; it did **not** prove
that the wireless daemon naturally reaches this path on the unit.

The natural dio path is tied to its Cinemo/USB iAP2 session and CarPlay
manager reaction, as `TRACK_P1_STATUS.md` records. The new wireless daemon
terminates iAP2 itself and sends no Cinemo event or message to dio. Therefore
the current implementation has no causal edge from wireless iAP2
authentication to `CAirPlayMainThread::run`. This is a required integration
piece, separate from SDP/RFCOMM, Device IE, mDNS and PF.

## Stock object and start path (MU1316 static disassembly)

The ordinary `CDIOManager` constructor is `0x15A064`. Its sole direct caller,
`0x171C50` inside `CDIOController` virtual init `0x171C10` (vtable
`0x1A5328`, entry `0x1A5350`), allocates **0x2F0** bytes with
`operator new(std::nothrow)` and stores the resulting manager pointer at
`controller+0x38` (`0x171C58`). The constructor zeroes `manager+0x168`
(AirPlay thread pointer), `+0x174` (flags),
and `+0x224..+0x240` (display/configuration words) at `0x15A1B8..0x15A244`.
The `CDIOManager` vtable is at `0x1A4840`, with object vptr `0x1A4848`.
`wireless/receiver_bridge/capture_probe.c` now contains a guarded, capture-only
`LD_PRELOAD` hook for the two nothrow `operator new` calls surrounding manager
construction at `0x171C44` and `0x171C68`. A local stock-code QEMU run
confirmed the exact calls, constructor-complete vptr and captured pointer.
An earlier experiment hooking the controller's vtable init slot did not run:
normal startup calls the method directly. The current probe also observed
the ordinary deleting-destructor path in QEMU and cleared its pointer before
`operator delete`. It cross-compiles for QNX ARM and does not expose a START
listener or alter AirPlay lifecycle.

The stock start method is `0x15AF50` (vtable entry at `0x1A485C`, offset
`+0x14` from the object vptr). It takes `r0=CDIOManager*`, `r1` pointing to a
`util::UnicodeString8` thread name, and `r2` passed on to the thread-start
method (exact C++ type of `r2` is not yet established). If `manager+0x168` is
null, it allocates **0x18** bytes at `0x15AF9C`, calls the
`CAirPlayMainThread` constructor `0x158B20` with `r1=manager`, and stores the
thread at `manager+0x168` (`0x15AFB0`). The constructor puts the manager
pointer at `thread+8`. At `0x15B020`, the start method invokes the thread's
virtual method at vptr `+0x0C`, `0x176BAC`, which allocates a `util::Thread`,
calls `osal::Thread::start`, and stores it at `thread+4`. This is the stock
ownership/lifecycle path; `thread+8` is **the actual manager object**, not a
standalone config block.

`CAirPlayMainThread::run` reads `manager+0x174` and the eight words
`manager+0x224..+0x240` at `0x15C2A4..0x15C340` before the server-create
call. These fields start as zero. A wired session handler at `0x16A7CC`
reaches the manager through its owner's `+0xE8` pointer, copies the session
flag from `event+4 -> +0x5C` to `manager+0x174` (`0x16A830`), obtains positive
display dimensions via `0x1675A4`, and writes them plus four other event
values to `manager+0x224..+0x240` (`0x16A878..0x16A8A4`). It also copies
event values to `manager+0x16C/+0x170`. The exact semantic names for every
config word still require mapping. Invoking `0x15AF50` on a freshly constructed manager
would pass zero configuration to `run()` and is therefore **not a proven
wireless receiver-start recipe**.

The event owner's identity is now established. `manager+0x1E0` holds a
`CDSICarplayImpl*`: `0x156EC8` allocates **0x11C** bytes, calls its constructor
`0x1683D0` with `r1=manager`, then stores the result at `manager+0x1E0`
(`0x156F14`). The constructor stores the manager at `CDSICarplayImpl+0xE8`
(`0x1683F4`). Its vtable is `0x1A4EF8`, object vptr `0x1A4F00`, and the
config-filling handler is the entry at vptr `+8` (`0x1A4F08 -> 0x16A7CC`).
That callback takes an event wrapper in `r1`; its payload pointer is
`*(r1+4)`. It uses payload offsets `+0x5C` for the flag, `+0x18` to select the
dimension source, `+0x54/+0x58` and `+0x24/+0x28` for four configuration
words, and `+0x3C/+0x40` for the two copied state words. For the ordinary
dimension path it calls `0x1675A4`; when payload `+0x18 == 3`, it reads
dimensions directly from payload `+0x1C/+0x20`. It writes positive dimensions
twice (`+0x224/+0x228` and `+0x22C/+0x230`). This is the **DSI CarPlay event
path**, so feeding the manager from the wireless daemon requires reproducing
the event contract or an equivalent bounded setter, not just starting a thread.
The source of `0x1675A4` is a three-entry read-only table at `0x19A190`:
mode 0 = 800×480, mode 1 = 960×540, mode 2 = 1024×480. The bridge calls this
stock routine and fills the four dimension words only when the manager has
no dimensions. `verify_capture_point.py` pins the function and table bytes.
This removes the QEMU display seed from the integrated first-start test,
although the correct mode for the physical display and the other DSI values
still need validation.

## Exact stock start dispatch

The start event closes the previously missing call edge. Once the DSI handler
has populated the manager, it checks payload `+0x44` (a count) for nonzero,
payload `+0x48` (an array pointer) for non-null, and the first word of the
first array entry for nonzero
(`0x16A9A0..0x16A9C0`). On the true branch it calls `0x15A758(manager,
payload+0x44)` at `0x16A9DC`, followed by `0x157460(manager, 0)` at
`0x16A9E8`. The false branch calls `0x1564E4(manager)` instead. The
false-branch operation is still being mapped; do not omit it from a proposed
production sequence.

`0x15A758` consumes the pair `{count, entries}` at payload `+0x44/+0x48`.
Each source entry has a 12-byte stride and a `util::UnicodeString8*` at entry
`+4` (`0x15A8C4..0x15A8D8`, `0x15A96C..0x15A984`). It clears and frees the
manager's previous 40-byte-stride UnicodeString8 vector at `+0x288` (count),
`+0x28C` (array), `+0x290` (capacity), copies each source string into the new
vector (`0x15A9F8..0x15AA20`), and calls `0x135EB8` then `0x135EF4` on a
local helper object. This is a deep copy of DSI-supplied string entries, not a
generic zero-argument start prerequisite. Its input must be a valid DSI value
array, so the wireless bridge must either construct that array correctly or
provide an equivalent state update after the remaining side effects are
understood.

`0x157460` builds a `util::UnicodeString8` from the literal `AirPlayThread`
and calls `0x12124C(manager+0x148, &name, 0)` (`0x1574C8..0x1574D8`). The
builder copies the string into a `CAirPlayServiceEvent_StartAirPlay` at
`event+0x1C`, writes the trailing word at `event+0x44`, and queues the event
(`0x12124C..0x121290`). Its execute method at `0x1223B4` forwards the string
and word to the listener virtual slot `+8` (`0x12240C..0x122428`). The manager
has an `IAirPlayServiceEventListener` subobject at `manager+0xAC`: its
secondary vtable has offset-to-top `-0xAC` at `0x1A49E0`, object vptr
`0x1A49E8`, and slot `+8` points to thunk `0x15AF48`. That thunk subtracts
`0xAC` from `this` and enters the normal start method `0x15AF50`. Thus the
ordinary path is:

```text
DSI payload -> CDSICarplayImpl::event (+manager config)
            -> 0x15A758 -> 0x157460 -> StartAirPlay event
            -> listener thunk 0x15AF48 -> manager start 0x15AF50
            -> CAirPlayMainThread::run 0x15C214 -> AirPlay server create
```

There is a second direct call to `0x157460` at `0x15BF40`; it comes from a
different manager path. Neither call proves a wireless session can skip the
DSI event's configuration and ownership steps.

## Stock stop path and restart constraint

The routine whose log name is `signalStopAirPlayMainThread` is `0x154AD8`.
It checks `manager+0x168` and, when a thread object exists, calls `0x154188`
on it (`0x154B0C`) to clear `thread+0x10`, then calls its vptr `+0x10`
method (`0x154B10..0x154B20`). The `CAirPlayMainThread` vptr at `0x1A4820`
resolves this method to `0x158928`: it calls `CFRunLoopGetMain` and
`CFRunLoopStop`, then tail-calls the common thread stop helper `0x176DF8`.
That helper wakes the `util::Thread` at `thread+4`, requests state change 2,
joins it, destroys it through its virtual method, and clears `thread+4`
(`0x176E20..0x176E68`). A manager method at `0x15FA58` invokes `0x154AD8`
at `0x15FB68` on one of its teardown branches; another branch tears down the
AirPlay session through `AirPlayReceiverSessionTearDown` at `0x15FB60`.

The stop sequence **does not itself clear** `manager+0x168`. The start method
`0x15AF50` tests that pointer at `0x15AF84` and branches past allocation and
thread start when it is non-null (`0x15AF8C -> 0x15B0AC`). Therefore a second
StartAirPlay event on the *same manager* does not restart the joined thread.
The owner/destructor path for the 0x18-byte thread object and the normal
manager recreation boundary still need to be proved. A bridge must not treat
stop as permission to allocate a second thread or overwrite `manager+0x168`.

A full aligned ARM-instruction scan of the MU1316 `.text` section
(`0x113308..0x18A2C4`) found only two direct stores with displacement
`+0x168`: the constructor's zero store at `0x15A1B8` and the start method's
thread-pointer store at `0x15AFB0`. The stop routine (`0x154AD8`) and the
manager destructor (`0x15B314..0x15B704`) contain no such store. This scan
does not rule out an indirect or computed-address write, but strengthens the
single-start conclusion: no stock direct-offset path resets the pointer on
STOP. The destructor walks other members and does not visibly own this
pointer. Do not infer that the 0x18-byte allocation may be manually freed;
the complete ownership contract is still unresolved.

## QEMU stop/restart A/B result

The stock thread-stop method `0x154AD8` is unsuitable for a wireless-session
STOP. In the real stock `dio_manager` under the QEMU broker/servicemgr mesh,
it stopped the AirPlay server and deregistered Bonjour, retained the
`manager+0x168` thread pointer with `thread+4 == 0`, and then caused the
manager to be released and `dio_manager` to log `Shutdown signaled: 1` and
exit. A second run calling STOP **without** queuing another StartAirPlay
showed the same process shutdown. The method calls
`CFRunLoopStop(CFRunLoopGetMain())` at `0x158974..0x158978`; that is the
probable shutdown trigger, though QEMU does not isolate every side effect.
Do not call this method from a production bridge.

There is a narrower viable server lifecycle. A QEMU-only experiment captured
the `AirPlayReceiverServerCreate` output from the stock thread, created
CoreFoundation strings `stopServer` and `startServer`, and passed each to
`AirPlayReceiverServerControl(server, 1, command, NULL, NULL, NULL)`. The
direct stop returned 0, deregistered `_airplay._tcp.`, and closed TCP 5000.
The direct start returned 0, reopened TCP 5000, and registered
`_airplay._tcp.` a **second** time. A new `GET /info` returned HTTP 200 with
feature bit 37, and the same manager remained captured through the run.
The exact experiment is `qemu_capture.py --mesh --start-experiment
--seed-display --skip-hid-qemu --feature37 --mdnsd --mock-mac-qemu
--interface-lo0-qemu --soft-restart-qemu`; its log is
`receiver_bridge/out/qemu_soft_restart.log`. This proves a local
stop/start cycle without killing stock dio in the emulator. It does not yet
prove session teardown, repeated phone reconnection, multicast delivery, or
that the server pointer remains valid across all manager lifetimes. The
production C bridge should retain/observe the server reference safely,
serialize its control operations, and gate each START on fresh `/info` and
Bonjour evidence rather than trying to recreate `CAirPlayMainThread`.

The earlier Track-F harness used a synthetic object and bypasses to test the
AirPlay library in QEMU. Its successful TCP listener test does not validate
manager capture, session-config population, virtual start dispatch, or stop
handling in the production process. The newer broker/servicemgr mesh keeps a
stock manager alive and the capture preload posts its real StartAirPlay event.
That reaches stock screen setup and, with a QEMU-only HID bypass, the AirPlay
server create/properties/delegate/control path. Staging `/dev/urandom` and
`/dev/random` in the temporary IFS lets the control call return 0 and logs
“Servers successfully started.” The observed `bind` is TCP 5000 and `/info`
there returns HTTP 200; TCP 7000 remains closed (`ECONNREFUSED`), while Bonjour
returns `kNotFoundErr`. This is partial stock receiver startup, not wireless
receiver readiness. The next offline work is to decide whether the phone can
use the stock advertised port 5000, validate discovery on a real interface,
map the remaining `0x15A758` helper effects and restart/teardown ownership,
then make a QNX bridge that validates the complete path under QEMU before any
on-unit test.

The discovery failure was narrowed further in `receiver_bridge/bonjour_gate.md`.
Stock dio sets `interfaceName=carplay0` at `0x15C57C`; stock libairplay
returns `-6727` when `if_nametoindex("carplay0")` is zero, before calling
`DNSServiceRegister`. The observed HU AP is `uap0`. In a QEMU-only A/B run,
substituting `lo0` through the original property setter plus supplying a
synthetic MAC let `DNSServiceRegister` succeed and register
`_airplay._tcp.` on 5000 with factory `mdnsd`. QEMU multicast sends still
fail on its loopback-only network; no phone session has been exercised.

For implementation, keep the in-process ABI seam in **C** (or ABI-controlled
C++ with an `extern "C"` boundary) and the standalone wireless protocol daemon
in **Rust**. This matches the existing `wireless/graft` and `wireless/daemon`
split. The local `Tools/qnx-65-sdp-docker` image can build both for QNX 6.5
ARM; its GCC 8 C++ runtime does not match the unit's older `libstdc++`, so a
C++ bridge would need a statically linked or bundled runtime. C avoids that
extra dependency for a small injected module. The daemon now has a synchronous
generation-tagged localhost START/STOP contract at `127.0.0.1:57343` (C wire
definition: `wireless/receiver_bridge/protocol.h`); it withholds
`CarPlayStartSession` until START is acknowledged. An offline C bridge now
implements this contract in `wireless/receiver_bridge/bridge.c`. In QEMU,
two complete generations passed through its listener, including the bridge's
first stock StartAirPlay event, direct server stop/start, two Bonjour
registrations, and bit-37 `/info` checks. The QNX build is
`wireless/receiver_bridge/out/libdio_receiver_bridge.offline.so`. It remains
an offline prototype: the bridge's first START now obtains display dimensions
from the stock table without a QEMU display seed,
and neither manager destructor concurrency nor phone/session callbacks are
validated on the HU.
The integrated QEMU run also exposed startup latency: starting only when
the first availability request arrived needed a retry after the bridge's
initial wait. The offline bridge now waits for its AP interface and posts
the stock event ahead of the request; in QEMU, the first availability START
then succeeded on its first attempt. Production AP timing remains untested.

A production bridge should make dio start its own AirPlay thread with a real
configuration and lifecycle, then verify stock `libairplay` binds the same
port used in `CarPlayStartSession`, publishes `_airplay._tcp` on `uap0`, serves
`/info` with feature bit 37, and
delivers session callbacks to dio's audio/video/HMI path. The Track-F
`_exit` suppression, global `getifaddrs` failure, and synthetic zero-config
are QEMU test devices, not deployment behavior. Before adapting the preload,
the manager's destructor boundary and the source of wireless display
configuration need to be mapped in stock dio. No head-unit process or
binary has been changed for this analysis.
