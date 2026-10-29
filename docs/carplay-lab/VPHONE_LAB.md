# vphone CarPlay laboratory

The active sender backend is native iOS in vphone, paired with the stock210 ARM/QNX receiver. Use the pinned guest profile below for reproducible runs; Inferno/iOS 14 is retained historical tooling.

[Laboratory index](README.md) · [Profile boundaries and acceptance](WIRELESS_COMPLETION_PLAN.md) · [Timing](VPHONE_TIMING.md)

## Choose a profile

| Task | Current procedure |
| --- | --- |
| Bootstrap, VM association, pairing and reconnect | [Native wireless QNX](NATIVE_WIRELESS_QNX.md) |
| Main/AltScreen video | [Native screen test](NATIVE_SCREEN_TEST.md) |
| Codec PCM, microphone and real SI/J9 startup | [Media validation](WIRELESS_MEDIA_VALIDATION.md) |
| Queue, pacing and fault load | [Media stress](MEDIA_STRESS_TEST.md), [clock measurements](VPHONE_TIMING.md) |
| Sender/bootstrap API diagnostics | `scripts/carplay_lab.sh` and [vphone source](../../tests/carplay_lab/vphone/) |

A phone screenshot is guest UI evidence. A decoded receiver capture is media evidence. Neither establishes physical HU presentation. Native profiles name their association, authentication, HMI and peripheral substitutions in the receipt.

## 📦 Reproducible guest profile

[The tracked profile](../../tests/carplay_lab/vphone/profile.json) pins both firmware
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

The steps below describe setup on a new host. Research Guests and the privileged helper are host prerequisites documented by upstream.[^5]
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

The commands below exercise shared capture and receiver tools. Interpret every result using the [profile matrix](WIRELESS_COMPLETION_PLAN.md#test-profiles); dated counts and captures are retained in the historical record.

```sh
clang -Wall -Wextra -Werror -Wframe-larger-than=4096 -fsanitize=address,undefined \
  tests/carplay_lab/qnx/test_qnx_input_memory.c -o build/vphone-lab/test-qnx-input-memory
build/vphone-lab/test-qnx-input-memory
```

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
| `tests/carplay_lab/vphone/` | Active guest profile, lifecycle, native USB diagnostics and owned NCM bridge |
| `tests/carplay_lab/common/` | QNX receiver, checked MFi fixture, signer, AVC extraction and pacing |
| `tests/carplay_lab/legacy_ios/` | Retained Inferno implementation and compatible old entry points |
| `tests/carplay_lab/qnx/` | QEMU capture harness, synthetic phones and QNX-only native fixtures |
| `tests/carplay_lab/fixtures/` | Public test identities and provenance |
| `build/vphone-lab/` | Ignored private runtime reports and captured evidence |

Legacy entry points explicitly select the old artifact directory; they are not
automatic fallbacks for vphone. Historical iOS 14 address-pinned framebuffer and
codec helpers are not installed into the new guest.

[^1]: Lakr233/vphone-cli. (2026). "GPU acceleration in the guest." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Research/Guest/gpu_acceleration.md
[^2]: Lakr233/vphone-cli. (2026). "Firmware compatibility." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Documents/Guides/compatibility.md
[^3]: Lakr233/vphone-cli. (2026). "Create and run a VM." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Documents/Guides/create-and-run.md
[^4]: Lakr233/vphone-cli. (2026). "Networking." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Documents/Guides/networking.md
[^5]: Lakr233/vphone-cli. (2026). "vphone guest control skill." https://github.com/Lakr233/vphone-cli/blob/0da1bea0e58199a1888581135b94a69063d71779/Skills/vphone-guest-control/SKILL.md
[^6]: Apple. "XNU IOUserClient.cpp, is_io_catalog_send_data." https://github.com/apple-oss-distributions/xnu/blob/main/iokit/Kernel/IOUserClient.cpp
[^7]: Apple. "IOHIDProviderPropertyMerger.cpp." https://github.com/apple-oss-distributions/IOHIDFamily/blob/main/IOHIDFamily/IOHIDProviderPropertyMerger.cpp

## Historical investigations

The [earlier vphone record](../archive/internal-reference-2026-10-29/carplay-lab/VPHONE_LAB.md) retains USB/accessory experiments, API traces, Siri capability-cache analysis, input captures and dated pacing measurements. [Inferno/iOS 14](../archive/internal-reference-2026-10-29/carplay-lab/IOS_VM_LAB.md) preserves the older backend; its address-pinned helpers and acceptance results do not apply to this guest.
