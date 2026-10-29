# CarPlay emulator laboratory — archived documentation

> Historical snapshot before the current-state documentation restructure. Commands, status and measurements below belong to their recorded revisions.

_vphone sender, QNX/QEMU receiver and reproducible timing evidence._

[Documentation home](../../../README.md) · [Test source tree](../../../../tests/carplay_lab/README.md) · [Wireless implementation](../../../wireless-carplay/README.md)

## 📋 Current stand

The [October 22 media/HMI checkpoint](../../../carplay-lab/WIRELESS_MEDIA_VALIDATION.md) adds real
ARM/QNX AAC/Opus tests, a rapid-zoom AVC forwarder/decode test and concurrent
native SI with release Java HMI. Individual vphone AAC/APAC runs reached independently checked PCM with explicit test models. Full integration of native 110/111 video, audio, microphone and real Java HMI, plus physical zoom acceptance, remains open. The [completion ledger](../../../carplay-lab/WIRELESS_COMPLETION_PLAN.md) separates those gates; these results are not full wireless acceptance.

The active backend is vphone with native iOS 26.6.2 CarPlay, a software MFi
fixture and the stock210 QNX receiver. Both Maps streams have independently
decoded snapshots at 800×480 and 1440×542. The measured clock correction, BPF
buffering and remaining QEMU queue are documented separately from physical
wireless implementation and on-unit radio measurements.

The [October 18 Bluetooth API checkpoint](../../internal-reference-2026-10-29/carplay-lab/VPHONE_LAB.md#native-bluetooth-bootstrap-checkpoint-2026-10-18)
adds native non-USB MFi/Identification acceptance and an actual `5702` Wi-Fi
configuration request. It also found and corrected rejected Identification
message declarations in the Rust daemon. The subsequent
[QNX/native integration](../../../carplay-lab/NATIVE_WIRELESS_QNX.md) now answers `5702` and delivers
a receiver-bound `4301` through the actual ARM Rust daemon. With the opt-in VM association adapter, native iOS completes PairSetup and
PairVerify, starts media and sends genuine 800×480 H.264 to stock210. An
independent decoder reads 31 frames and the final frame shows Maps. The optional
paired profile also decodes native AltScreen at 1440×542 in the wireless session. Abrupt
native disconnect/reconnect also passes with the same receiver identity and
without repeating PairSetup. This
checks wireless protocol over virtual Ethernet; physical Wi-Fi/RFCOMM remains
a hardware test.

| Note | Scope |
|---|---|
| [Wireless media and native HMI](../../../carplay-lab/WIRELESS_MEDIA_VALIDATION.md) | AAC/Opus, zoom stress, live SI/J9 and explicit remaining acceptance |
| [Wireless completion plan](../../../carplay-lab/WIRELESS_COMPLETION_PLAN.md) | MainHigh RTCP, user menus and transport/consent policy, real audio focus, two phones and full-cycle acceptance |
| [Concurrent media stress](../../../carplay-lab/MEDIA_STRESS_TEST.md) | Two 30-fps AVC sources, shared capacity/outage model, production AltAV/Opus and independent pixel checks |
| [vphone runbook](../../../carplay-lab/VPHONE_LAB.md) | Current setup, ownership checks, pairing, input and native video |
| [Native wireless and QNX](../../../carplay-lab/NATIVE_WIRELESS_QNX.md) | Native wireless bootstrap, association adapter, pairing and independently decoded Maps |
| [Timing and queue evidence](../../../carplay-lab/VPHONE_TIMING.md) | Host/QNX clocks, native packet stages and sender sink pressure |
| [Retained iOS 14 laboratory](../../../carplay-lab/IOS_VM_LAB.md) | Inferno build recipe and historical bring-up |
| [Retained native screen tests](../../../carplay-lab/NATIVE_SCREEN_TEST.md) | Software-surface and paired-111 experiments on the older guest |
| [iOS/stock210 review](../../../carplay-lab/IOS210_CODE_REVIEW.md) | Receiver/protocol findings and reproduced corrections |
| [Wireless software MFi tests](../../../wireless-carplay/TEST_SOFTWARE_MFI.md) | Daemon crypto/control isolation and synthetic bootstrap tests |

## 🔗 Source boundary

```mermaid
flowchart LR
    accTitle: CarPlay Laboratory Source Ownership
    accDescr: The active and retained iOS backends share laboratory tools and QNX fixtures. The fixtures exercise production receiver and hook sources without placing emulator management in the wireless implementation.

    vphone["tests/carplay_lab/vphone"]
    legacy_ios["tests/carplay_lab/legacy_ios"]
    common["tests/carplay_lab/common"]
    qnx["tests/carplay_lab/qnx"]
    production["wireless and hook sources"]
    artifacts["Ignored build artifacts"]

    vphone --> common
    legacy_ios --> common
    common --> qnx
    qnx -->|build and exercise| production
    common --> artifacts
    vphone --> artifacts
```

The stable launcher is `scripts/carplay_lab.sh`; Python modules now live under
`tests.carplay_lab`. Runtime images and evidence keep their existing ignored
paths. Production receiver code stays in `wireless/receiver_bridge/`; the
emulator harness and synthetic phones live in the test tree.

## 🧪 Acceptance limits

Emulator results establish the exercised native protocol and decoded captures.
They do not establish physical OMX presentation, a 2.4 GHz CarPlay session,
stable 30 fps or turn-by-turn navigation. The [timing report](../../../carplay-lab/VPHONE_TIMING.md)
records the remaining backlog and unsuccessful URB-count experiment.

The source separation passes 101 vphone and 97 retained Python tests, native
RAM/input tests with 10,000 seeded inputs, and 200,000 native trace-parser
fuzzer inputs under ASan/UBSan. QNX toolchain 8.5 builds both the lab image and
the ordinary stock210 receiver; the diagnostic QEMU relinks, boots and serves
native `/info` and clock RPCs through the existing launcher.

The broader repository Python run still reports one amplifier Java-contract
failure and 16 errors from absent amplifier analysis artifacts (48 skipped).
The unchanged root-only suite reproduces those same failures; they are outside
the emulator test suite. Detailed local logs use the `layout-*` prefix under
`build/vphone-lab/`.
