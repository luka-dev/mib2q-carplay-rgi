# Wireless CarPlay on MHI2Q

_Current SI-owned stock210 architecture, with dated vehicle and laboratory evidence. Documentation reconciled with the working tree on 2026-10-29._

---

[Documentation home](../README.md) · [Connect and USB](../connect-and-usb/README.md) · [Source tree](../../wireless/)

## 📋 Current status

The implementation keeps factory AirPlay **210.81** and makes the existing **SI the single CarPlay-child owner**. The Rust daemon handles iAP2/MFi/Wi-Fi bootstrap; the Bluetooth graft supplies RFCOMM, and the hook/receiver bridge provides pairing, media and semantic transport.

The selected network configuration is the patched Wi-Fi driver `47a370bc` with stock SDIO and stock io-pkt. PF includes the required CarPlay port changes. Exact binaries and fingerprints are in the [stock patch catalog](../stock-patches/README.md).

Main video, input and AltScreen have partial vehicle confirmation. Native lab profiles cover paired screens, AAC/APAC PCM, microphone transport, Now Playing, cleanup and reconnect behavior. **Full wireless acceptance is incomplete**: MainHigh control/RTCP, connection policy, native HMI audio focus and combined long-running vehicle sessions remain tracked in the [completion ledger](../carplay-lab/WIRELESS_COMPLETION_PLAN.md). The last recorded HU configuration is restored wired operation.

Use [production channels](WIRELESS_PRODUCTION_CHANNELS.md) for the current feature path, [SI integration](SI_STAGE2_OFFLINE.md) for ownership/admission, and the [laboratory](../carplay-lab/README.md) for reproducible tests.

<a id="components-and-ownership"></a>

## ⚙️ Architecture and ownership

```mermaid
flowchart TB
    accTitle: Stock210 wireless CarPlay architecture
    accDescr: Bluetooth bootstrap reaches the Rust daemon through the btstack graft, while Wi-Fi media reaches the C hook and original receiver. The existing smartphone_integrator owns the DIO lifetime; Rust handles bootstrap.
    phone["Phone Bluetooth and Wi-Fi"]
    btstack["btstack RFCOMM graft"]
    daemon["Rust iAP2 bootstrap daemon"]
    mfi["Existing MFi coprocessor"]
    bridge["DIO receiver bridge"]
    hook["C pairing and media hook"]
    receiver["Factory AirPlay 210.81"]
    owner["smartphone_integrator: single DIO owner"]
    phone -->|Bluetooth| btstack --> daemon
    daemon -->|Certificate and signature| mfi
    daemon -->|START and IDENTITY| bridge
    phone -->|Wi-Fi| hook --> receiver
    bridge --> receiver
    owner -->|DIO lifecycle| bridge
```

| Component | Responsibility | Implementation |
|---|---|---|
| [btstack graft](BTSTACK_GRAFT.md) | Discovery, RFCOMM and bounded byte bridge | [C graft](../../wireless/graft/graft.c) |
| [iAP2 daemon](IAP2_DAEMON.md) | MFi, Wi-Fi handover, CarPlay Control | [Rust daemon](../../wireless/daemon/src/) |
| [Receiver bridge](RECEIVER_BRIDGE.md) | Real stock manager, readiness and identity | [C bridge](../../wireless/receiver_bridge/bridge.c) |
| [Wireless hook](STOCK210_HTTP_PAIRING_ABI.md) | Pairing, encrypted transport and media | [C module](../../hook/wireless/) |
| [Owner integration](DIO_OWNERSHIP.md) | SI owns admission, child lifecycle and wired/wireless handover | [SI controller](../../wireless/si/si_controller.c) and [adapter](../../wireless/si/si_adapter.c) |

Bonjour, `/info`, signed PairSetup and iAP2 StartSession share one persistent
pairing identifier `pi`. It is separate from the Ed25519 public key. Pairing
keys, owner locks and the advisory generation marker use `/mnt/persist`;
`/tmp` is transient RAM. Pairing initialization and the media SETUP guard are
scoped to wireless owner mode. See [review corrections](../archive/internal-reference-2026-10-29/carplay-lab/IOS210_CODE_REVIEW.md#-corrections-and-regression-coverage) and
[receiver lifecycle](STOCK210_RECEIVER_LIFECYCLE.md#queue-and-owner-lifecycle).

## 📚 Reading paths

| Topic | Notes |
|---|---|
| Current code review | [iOS and stock210 findings, reproductions and fixes required](../carplay-lab/IOS210_CODE_REVIEW.md) |
| First physical test | [Current test kit, baseline checks and rollback](STOCK210_HU_TEST.md) |
| SI stage-1 HU proof | [Native queue, deletion, USB regression and manual rollback](SI_STAGE1_HU_TEST.md) |
| SI-owned wireless target | [MH2p-guided SI extension plan](SI_WIRELESS_EXTENSION_PLAN.md) · [Ownership decision](DIO_OWNERSHIP.md) |
| Current receiver | [Architecture](STOCK210_ARCHITECTURE.md) · [Startup](RECEIVER_START.md) · [Bridge](RECEIVER_BRIDGE.md) · [Readiness and restart](STOCK210_RECEIVER_LIFECYCLE.md) |
| Bluetooth transport | [Graft](BTSTACK_GRAFT.md) · [Channel layout](RFCOMM_CHANNEL_LAYOUT.md) · [Duplex ABI](RFCOMM_DUPLEX.md) · [Discovery](BLUETOOTH_DISCOVERY.md) |
| Graft construction | [Insertion plan](GRAFT_INSERTION_PLAN.md) · [Factory launcher experiments](GRAFT_LAUNCHER_LIFECYCLE.md) |
| Pairing and media | [HTTP ABI](STOCK210_HTTP_PAIRING_ABI.md) · [Media crypto ABI](STOCK210_MEDIA_CRYPTO_ABI.md) · [280 source recipe](AIRPLAY280_PAIRING_RECIPE.md) |
| Bootstrap and ownership | [iAP2 daemon](IAP2_DAEMON.md) · [Software MFi tests](TEST_SOFTWARE_MFI.md) · [iOS VM laboratory](../carplay-lab/IOS_VM_LAB.md) · [DIO ownership](DIO_OWNERSHIP.md) · [Vendor provenance](IAP2_VENDOR_PROVENANCE.md) |
| Network and hardware | [Wi-Fi discovery](WIFI_AND_DISCOVERY.md) · [Final stock patches](../stock-patches/README.md) · [Firewall](FIREWALL.md) · [Hardware and donors](HARDWARE_AND_DONORS.md) |
| iOS and references | [Authentication selection](IOS_AUTH_SELECTION.md) · [Wireless session gates](IOS_WIRELESS_SESSION_GATES.md) · [Bootstrap comparison](BOOTSTRAP_COMPARISON.md) |
| Evidence and history | [Pairing/media milestones](STOCK210_VALIDATION.md) · [WCP0627 source handoff review](WCP0627_SOURCE_REVIEW.md) · [Historical experiments](history/README.md) · [Migration map](history/MIGRATION.md) |

The history section preserves the earlier feature-bit probe, replacement280
receiver, installer experiment and failure traces. These are evidence records,
not the current deployment recipe. In particular, the historical280 installer
must not install the stock210 hook candidate.

<a id="build-and-reproduce"></a>

## 📦 Build and reproduce

Run from the repository root. Prerequisites are the local
`qnx65-armv7-toolchain:latest` C image and explicit `8.5-rust` image
(see [toolchain reproduction](STOCK210_HU_TEST.md#-build-and-baseline)), pinned firmware inputs under
`Firmwares/HU`, and owner-provided SDK sources under `Refferences/Carplay`.
Those external inputs are not committed here.

```sh
python3 wireless/si/build_si_review.py --full-cycle
```

This builder assembles matched SI, daemon, graft, receiver and hook artifacts and runs the native integrated fault/stress profiles. Its manifest and reports identify the exact inputs. See [SI build sources](../../wireless/si/README.md) and [production-channel validation](WIRELESS_PRODUCTION_CHANNELS.md) for individual profiles. Generated binaries, logs and external firmware/SDK inputs remain outside Git.

<a id="verified-offline"></a>

## 🧪 Validation

The [media/HMI guide](../carplay-lab/WIRELESS_MEDIA_VALIDATION.md) documents codec, PCM and native Java profiles; [production channels](WIRELESS_PRODUCTION_CHANNELS.md) covers paired AltScreen and semantic transport. Each result identifies its inputs, models and build. Physical radio, OEM MFi and audible vehicle output need their corresponding hardware checks.

<a id="next-steps-and-constraints"></a>

## 🎯 Remaining work

Use the [wireless completion ledger](../carplay-lab/WIRELESS_COMPLETION_PLAN.md) as the current acceptance checklist.

Implementation, model boundaries, dated receipts and the remaining MainHigh/control, policy, real-HMI and vehicle gates are maintained there. This overview and the runbooks link to that list rather than maintaining separate status tables.

Original offsets, byte layouts, hashes and negative results remain in the
technical notes. [The migration map](history/MIGRATION.md) accounts for every document
moved from `wireless/`; source-directory READMEs now point here.

The [MH2p SI comparison](SI_MH2P_COMPARATIVE_RE.md) preserves donor-specific device-registration and lifecycle RE used by the SI extension.
