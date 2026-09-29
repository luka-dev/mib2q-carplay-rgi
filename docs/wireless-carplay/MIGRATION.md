# Wireless documentation migration

_Canonical paths and preservation record — 2026-09-29._

---

[Wireless overview](README.md) · [Documentation home](../README.md)

## 📋 Scope and preservation

All 30 tracked Markdown documents previously under `wireless/` now have a
canonical home in this section. Two existing Wi-Fi/hardware notes moved from
`docs/connect-and-usb/` to join them. Source READMEs retain short navigation
pointers; the vendor license and crate sources remain in their original place.

Technical offsets, wire layouts, pinned hashes, failure traces and commands
are preserved. Earlier conclusions are marked as historical where later
stock210 work superseded them. New component diagrams summarize those records;
no additional hardware result is claimed by this documentation migration.

## 🔗 Path map

| Previous path | Canonical document |
|---|---|
| `wireless/README.md` | [README.md](README.md) |
| `wireless/CODEX_HANDOFF.md` | [history/CODEX_HANDOFF.md](history/CODEX_HANDOFF.md) |
| `wireless/airplay280_pairing_recipe.md` | [AIRPLAY280_PAIRING_RECIPE.md](AIRPLAY280_PAIRING_RECIPE.md) |
| `wireless/airplay_feature_patch/README.md` | [history/FEATURE_BIT_EXPERIMENT.md](history/FEATURE_BIT_EXPERIMENT.md) |
| `wireless/carplay_discovery.md` | [BLUETOOTH_DISCOVERY.md](BLUETOOTH_DISCOVERY.md) |
| `wireless/daemon/README.md` | [IAP2_DAEMON.md](IAP2_DAEMON.md) |
| `wireless/daemon/vendor/README.md` | [IAP2_VENDOR_PROVENANCE.md](IAP2_VENDOR_PROVENANCE.md) |
| `wireless/firewall/README.md` | [FIREWALL.md](FIREWALL.md) |
| `wireless/graft/README.md` | [BTSTACK_GRAFT.md](BTSTACK_GRAFT.md) |
| `wireless/graft/launcher_lifecycle.md` | [GRAFT_LAUNCHER_LIFECYCLE.md](GRAFT_LAUNCHER_LIFECYCLE.md) |
| `wireless/graft_insertion_plan.md` | [GRAFT_INSERTION_PLAN.md](GRAFT_INSERTION_PLAN.md) |
| `wireless/integration/install_review.md` | [history/EXPERIMENTAL280_INSTALLER.md](history/EXPERIMENTAL280_INSTALLER.md) |
| `wireless/integration/si_ownership.md` | [DIO_OWNERSHIP.md](DIO_OWNERSHIP.md) |
| `wireless/ios_auth_selection_audit.md` | [IOS_AUTH_SELECTION.md](IOS_AUTH_SELECTION.md) |
| `wireless/ios_wireless_session_gates.md` | [IOS_WIRELESS_SESSION_GATES.md](IOS_WIRELESS_SESSION_GATES.md) |
| `wireless/offline_bundle.md` | [history/EXPERIMENTAL280_BUNDLE.md](history/EXPERIMENTAL280_BUNDLE.md) |
| `wireless/receiver_bridge/README.md` | [RECEIVER_BRIDGE.md](RECEIVER_BRIDGE.md) |
| `wireless/receiver_bridge/bonjour_gate.md` | [history/BONJOUR_READINESS.md](history/BONJOUR_READINESS.md) |
| `wireless/receiver_bridge/identity_and_pairing.md` | [history/RECEIVER_IDENTITY.md](history/RECEIVER_IDENTITY.md) |
| `wireless/receiver_bridge/independent_pairing_probe.md` | [history/INDEPENDENT_PAIRING_PROBE.md](history/INDEPENDENT_PAIRING_PROBE.md) |
| `wireless/receiver_bridge/info_cfdiag.md` | [history/INFO_SERIALIZATION.md](history/INFO_SERIALIZATION.md) |
| `wireless/receiver_start.md` | [RECEIVER_START.md](RECEIVER_START.md) |
| `wireless/reference_bootstrap_comparison.md` | [BOOTSTRAP_COMPARISON.md](BOOTSTRAP_COMPARISON.md) |
| `wireless/rfcomm_channel_layout.md` | [RFCOMM_CHANNEL_LAYOUT.md](RFCOMM_CHANNEL_LAYOUT.md) |
| `wireless/rfcomm_duplex.md` | [RFCOMM_DUPLEX.md](RFCOMM_DUPLEX.md) |
| `wireless/stock210_hook_architecture.md` | [STOCK210_ARCHITECTURE.md](STOCK210_ARCHITECTURE.md) |
| `wireless/stock210_http_pairing_abi.md` | [STOCK210_HTTP_PAIRING_ABI.md](STOCK210_HTTP_PAIRING_ABI.md) |
| `wireless/stock210_media_crypto_abi.md` | [STOCK210_MEDIA_CRYPTO_ABI.md](STOCK210_MEDIA_CRYPTO_ABI.md) |
| `wireless/stock210_pairing_validation.md` | [STOCK210_VALIDATION.md](STOCK210_VALIDATION.md) |
| `wireless/stock210_receiver_lifecycle.md` | [STOCK210_RECEIVER_LIFECYCLE.md](STOCK210_RECEIVER_LIFECYCLE.md) |
| `docs/connect-and-usb/WIRELESS_CARPLAY_AND_WIFI.md` | [WIFI_AND_DISCOVERY.md](WIFI_AND_DISCOVERY.md) |
| `docs/connect-and-usb/WIRELESS_CARPLAY_HU_FEASIBILITY.md` | [HARDWARE_AND_DONORS.md](HARDWARE_AND_DONORS.md) |

## 📦 Tooling and generated files

The review builders read the canonical paths in `docs/wireless-carplay/`.
The source tree and output directories stay under `wireless/`. Existing runtime
artifacts and test logs remain valid evidence; rebuilding an archive with the
moved/edited documentation changes that archive's digest.

Repository references point to the new documents. The map above intentionally
retains the old names as a migration ledger. The original handoff is historical;
its task wording is preserved for context, not as a new instruction.

## 🧪 Migration validation

Validation completed on 2026-09-29:

| Check | Result |
|---|---|
| Source coverage | All 32 source documents accounted for: 30 from `wireless/`, two from Connect/USB |
| Canonical section | 34 Markdown documents, including the new migration ledger and history index |
| Relative links and anchors | Passed for the section, affected indexes and source README pointers |
| Technical evidence preservation | All original hexadecimal address and SHA-1/SHA-256 tokens retained in their mapped documents |
| Mermaid rendering | All 37 diagrams rendered with Mermaid CLI 11.17.0 in both light and dark themes |
| Visual inspection | Architecture, DIO ownership and receiver lifecycle diagrams checked for legibility |
| Stock210 review packaging | 34 canonical documents included; all 46 manifest entries verified by size and SHA-256 |
| Historical bundle inputs | All three relocated document inputs exist |
| Builder syntax and patch whitespace | Passed |

The packaging check reused the previously tested runtime artifacts; it did not
rerun the runtime build or QNX tests. Rendered previews remain under ignored
`build/wireless-doc-preview/`; the committed Mermaid text is authoritative.
This migration changes documentation, source comments and builder document
paths. It adds no runtime behavior or hardware validation.
