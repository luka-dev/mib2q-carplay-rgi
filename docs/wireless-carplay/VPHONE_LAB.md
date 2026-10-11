# vphone CarPlay laboratory

_Active guest platform · 2026-10-11 · Branch `feat/vphone-carplay-lab`._

[Wireless overview](README.md) · [Retained Inferno evidence](IOS_VM_LAB.md) · [Software MFi](TEST_SOFTWARE_MFI.md)

## 📋 Decision and measured state

The laboratory now targets **vphone** for the iOS guest and retains the native
stock-210 QNX receiver. The user requested this migration on a separate branch.
Inferno work is preserved in checkpoint `001fee47`; its captures are historical
evidence and are not acceptance evidence for vphone.

vphone uses Apple's Virtualization.framework/PCC platform and installs the
paravirtual Metal driver. Upstream measurements demonstrate GPU execution;
they do not demonstrate our CarPlay sender, encoder, MFi or AltScreen path.[^1]

| Component | Local state |
|---|---|
| Host | Apple M3 Max, macOS 26.5.1 |
| Launchpad | Notarized 2.9.2 installed; archive SHA-256 verified |
| Core Bundle | 2.9.2 installed; SHA-256, policy and host preflight passed |
| Research Guests | Enabled; verified after the owner's Recovery step |
| Developer Tools / helper | Access allowed; signed helper `ready (16)` |
| Dedicated guest | `mhi2-carplay` created from template `17d93ffd69ab`, first boot passed, native UI and Maps render |
| QNX receiver | Fresh build passed ABI checks; native `/info` verified in the new directory |
| Shared signer | Loopback runtime returned a bounded 711-byte MFi-SAP response; host crypto tests passed |
| USB trust | Native pairing accepted by the dedicated guest |
| Ethernet attachment | Original NCM primary interface exchanges IPv6 with QNX; native `/info` returns HTTP 200 / 210.81 |
| iAP2 attachment | HID registration reaches the guest driver; live USB replacement stops VZ before acceptance |

`scripts/carplay_lab.sh doctor` writes the actual host report to
`build/vphone-lab/doctor.json` and exits nonzero until its checks pass. Host Setup
now passes. At the user's explicit request, the agent used local sudo to enable
Developer Mode and install the exact signed helper; its client-signature and
administrator-authorization checks remain intact. No credential was saved in
files or Git. The installer does not change SIP, boot arguments or Research Guests.

## 📦 Reproducible guest profile

[The tracked profile](../../wireless/vphone_lab/profile.json) pins both firmware
sources instead of following the newest catalog entry. Upstream records the
iPhone17,3 iOS 26.6.2 / cloudOS 26.4 beta pairing as tested.[^2]

| Setting | Value |
|---|---|
| Machine / library | `mhi2-carplay` / `~/.vphone/machines` |
| Launchpad and Core Bundle | `2.9.2` |
| iPhone | `iPhone17,3`, iOS `26.6.2`, build `23G90` |
| PCC components | cloudOS `26.4` beta, build `23E5207q` |
| CPU / memory / virtual disk | 6 cores / 6144 MB / 64 GB |
| Initial network | `nat` |
| Native services | `--slim off`; keep applications and services |
| Creation disk guard | At least 90 GiB free on the home volume |

Creation uses Launchpad's official client and owns one machine in one library.
It refuses an existing target, concurrent machine activity and incomplete host
setup. Launchpad 2.9.2 leaves its verified first boot running; the standalone
CLI's stopped-at-return convention is different. The wrapper accepts a running
or stopped matching guest only after the creation report proves every step and
`firstBoot` passed. `recover-create` can finalize that saved report without
restoring again. First-boot verification checks vphoned, not CarPlay video.[^3]

The live guest reports iOS `26.6.2`, kernel `25.4.0`, six processors and about
6 GiB of memory. Its model is the research platform `iPhone99,11`; iPhone17,3
describes the source userland and board configuration. The frozen template
preserves all system apps and services (`slim off`), and the dedicated clone has
its own identity. Native Maps drew the globe and map tiles; this is phone UI
evidence, not a CarPlay screen stream.

The creation report and machine ledger live under `build/vphone-lab/`. Later
commands check the ledger, library, Core Bundle and recorded UDID before operating
the guest. They never select a physical phone or fall back to Inferno.

## 🔄 Target architecture and acceptance boundary

```mermaid
flowchart LR
    accTitle: vphone CarPlay Laboratory Migration
    accDescr: vphone renders native applications and reaches QNX through its original NCM link. iAP2 attachment remains blocked by USB reinitialization, so MFi and video acceptance are pending.

    launchpad["Launchpad guest control"] --> ios_guest["iOS guest with Metal"]
    ios_guest --> ethernet_adapter["Owned NCM Ethernet adapter"]
    ethernet_adapter --> qnx_receiver["Native stock210 QNX receiver"]
    ios_guest -.-> iap_adapter["iAP2 USB initialization blocker"]
    iap_adapter -.-> qnx_receiver
    mfi_signer["Public fixture MFi signer"] --> qnx_receiver
    qnx_receiver --> video_capture["Authenticated AVC capture"]
    video_capture --> independent_decode["Independent FFmpeg decoding"]
```

The initial NAT interface provides guest connectivity and diagnostics. It is
not a completed CarPlay transport. Upstream's `tunnel` mode drops IPv6, so it
cannot carry the existing IPv6 link-local recipe unchanged.[^4] The running
guest exposes USBMUX and two NCM pairs through `AppleVirtIOUSBDeviceController`.
Primary USB data interface 2 maps to guest `en2`; auxiliary data interface 4
carries the guest's `anpi0` management link. Host names (`en22` / `en21` in the
measured run) are discovered below the owned USB serial and registry entries,
never hardcoded. The retained ARM-only, fixed-UDID USB probe is not applied to
vphone and its identity guard is not relaxed.

The original primary NCM link reached QNX at
`fe80::4254:ff:fe12:3456%en2:5000`. Packet evidence includes neighbor discovery,
TCP establishment, the guest's `GET /info`, and QNX's 775-byte `HTTP/1.1 200 OK`
response. Independent parsing of the captured binary plist returned
`sourceVersion=210.81` and `model=AirPlayGeneric1,1`. This establishes bidirectional
Ethernet and native receiver discovery; it does not establish CarPlay or MFi.

## 🔌 USB initialization blocker

A bounded, guest-only helper verifies the recorded UDID, the research product
string, controller class and chosen chip `FE01` before using native USB SPI.
It saves the original description and runs as a one-shot launchd job through
Launchpad RPC. Read-only `probe` is the normal action. Live replacement actions
require `--experimental` and are excluded from startup.

Three live descriptor experiments stopped the VM with `VZErrorDomain` code 1:
the four-interface combined layout, adding HID while preserving both original
NCM channels, and adding HID without an extra `GoOffAndOnBus` call. The guest
registered `IapOverUsbHid`, but this is not evidence of a usable host HID endpoint.
The host's `com.apple.VirtualizationCore` / `avp.usb-device-controller` log reports:

> Guest sent an available buffer notification without completing the device initialization first.

Restarts recovered the original USB description, native UI and NCM. Removing the
extra bus cycle did not fix the ordering failure. The next investigation is
native AVP queue initialization and reset ordering; no kernel patch has been
applied. Static triage found the controller and completion-queue code in the
pristine research kernel's `AppleVirtualPlatform` component. Host errors and
disassembly remain ignored evidence under `build/vphone-lab/`.

A separate `DeviceDestroy` experiment returned `0xe0000001`; the VM remained
running but its USB description was no longer available. An error return does
not imply that teardown had no side effects. The helper now attempts rollback
on that failure as well. Recovery uses a clean Launchpad stop/start, then reads
the original description again. This is not a working iAP2 reset recipe.

The shared QNX image retains pairing, native MFi-SAP, media authentication and
the explicit QEMU-only paired AltScreen gate. Hardware decoder output still
comes from mock OMX: exported AVC must be independently decoded on the host to
establish real picture evidence.

| Acceptance step | Required evidence |
|---|---|
| Guest startup | vphoned response, correct iOS identity, visible native UI |
| Graphics | Metal device and actual Maps rendering in this guest |
| Authentication | Native iAP2 acceptance and valid/invalid MFi controls |
| Transport | Actual guest/QNX packets; an open socket is insufficient |
| Primary screen | Authenticated AVC and independent 800×480 decode |
| AltScreen | Native stream 111 AVC and independent cluster decode |
| Frame pacing | AVC arrival/decode intervals in a stated clock domain |

A VM display's 60/120 Hz setting does not establish CarPlay's encoded frame rate.
No physical 2.4 GHz radio behavior is measured by this virtual stand.

## 🚀 Host setup and guest lifecycle

The verified release archives are cached locally. `bootstrap` can reproduce
their verification and install Launchpad without replacing an unrelated existing
installation. Extraction is temporary so a second persistent app with the same
bundle ID cannot confuse LaunchServices or UI automation. Existing executables
must match the verified release even when their version strings match.

```sh
scripts/carplay_lab.sh bootstrap
scripts/carplay_lab.sh plan
scripts/carplay_lab.sh doctor
```

Host Setup has passed on this Mac; the steps below describe a new host. Its owner
authorizes the security and helper changes, as required by the upstream skill.[^5]
Enable Research Guests in Recovery before continuing:

```sh
# In macOS Recovery, Terminal:
csrutil allow-research-guests enable
```

On a new host, open `/Applications/vphone-launchpad.app` and complete
**Host Setup**: grant Developer Tools access and install its helper using the
administrator prompt. Then install the explicitly pinned Core Bundle through
the official client. The helper may request administrator authorization.[^5]

```sh
~/.local/bin/vphone-launchpad-cli bundle install-release 2.9.2
scripts/carplay_lab.sh doctor
scripts/carplay_lab.sh create
# Launchpad normally leaves the verified guest running. Use start only when stopped.
scripts/carplay_lab.sh status
scripts/carplay_lab.sh snapshot
scripts/carplay_lab.sh diagnose
scripts/carplay_lab.sh screenshot
scripts/carplay_lab.sh stop
```

`start` waits for vphoned. `diagnose` collects bounded native process and media
logs. A phone screenshot proves the phone UI only; it is not a CarPlay frame.
Creation stays owned by Launchpad if its waiting CLI is interrupted; inspect the
existing operation rather than starting another restore.[^5]

## 🧪 Shared receiver and evidence tools

Migration validation passed 26 guest/bootstrap/transport tests and the 88 retained
laboratory regression tests. A fresh QNX build, native `/info` and the shared
signer's loopback response were checked. These results establish the migrated
tools; they do not establish a vphone sender session.

```sh
scripts/carplay_lab.sh qnx-build
scripts/carplay_lab.sh qnx-start
scripts/carplay_lab.sh qnx-info
scripts/carplay_lab.sh mfi --seconds 300
scripts/carplay_lab.sh qnx-stop
```

The receiver uses `build/vphone-lab/qnx-network`, a local QEMU Ethernet socket
on port `32501`, and the bounded loopback MFi-SAP service on port `32502`.
`qnx-build` compiles the paired laboratory hook before building its image;
compile failure propagates instead of reporting a stale artifact as success.
The Ethernet adapter connects that socket to the original owned NCM interface.
It holds an exclusive capture lock, rejects malformed QEMU records, checks both
endpoints during the run and records PCAP timestamps in the host realtime domain.
Incomplete runs are explicitly marked in `ethernet.json`.

Run the bridge in one terminal, then probe from the guest in another:

```sh
scripts/carplay_lab.sh usb probe
scripts/carplay_lab.sh ethernet --inspect
scripts/carplay_lab.sh ethernet --seconds 30
```

```sh
scripts/carplay_lab.sh usb network
```

The bridge uses the host's existing NCM driver and libpcap; it preserves USBMUX
and the auxiliary management channel. It does not change USB configurations,
host routes or physical interfaces. The network probe is a bounded native
guest socket request, not a host request or a synthetic sender session.

After an actual native sender session supplies complete capture records:

```sh
scripts/carplay_lab.sh extract build/vphone-lab/qnx-network/serial.log \
  --output build/vphone-lab/main.h264
scripts/carplay_lab.sh extract --cluster build/vphone-lab/qnx-network/serial.log \
  --output build/vphone-lab/cluster.h264
scripts/carplay_lab.sh pacing build/vphone-lab/qnx-network/serial.log
```

The extractor rejects incomplete, reordered or malformed records. Main and
cluster captures remain separate. Timestamp domains are reported separately;
cross-guest clock subtraction is not a latency measurement.

## 🗂️ Source ownership and retained history

| Directory | Responsibility |
|---|---|
| `wireless/vphone_lab/` | Active guest profile, lifecycle, native USB diagnostics and owned NCM bridge |
| `wireless/lab_common/` | QNX receiver, checked MFi fixture, signer, AVC extraction and pacing |
| `wireless/ios_vm_lab/` | Retained Inferno implementation and compatible old entry points |
| `build/vphone-lab/` | Ignored private runtime reports and captured evidence |

Legacy entry points explicitly select the old artifact directory; they are not
automatic fallbacks for vphone. Historical iOS 14 address-pinned framebuffer and
codec helpers are not installed into the new guest.

[^1]: Lakr233/vphone-cli. (2026). "GPU acceleration in the guest." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Research/Guest/gpu_acceleration.md
[^2]: Lakr233/vphone-cli. (2026). "Firmware compatibility." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Documents/Guides/compatibility.md
[^3]: Lakr233/vphone-cli. (2026). "Create and run a VM." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Documents/Guides/create-and-run.md
[^4]: Lakr233/vphone-cli. (2026). "Networking." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Documents/Guides/networking.md
[^5]: Lakr233/vphone-cli. (2026). "vphone guest control skill." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Skills/vphone-guest-control/SKILL.md
