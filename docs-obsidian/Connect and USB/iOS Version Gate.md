# iOS Version Gate

*Connect failures on this head unit are iOS-version-gated — iOS 27 carkitd repairs the CarPlay advertisement itself, iOS 26 does not, but the head unit's advertisement is still the broken side.*

---

## 🔍 The observation

Connect failures on the A5 head unit are **iOS-version-gated**. **[car]** verified both directions 2026-08-23: a phone updated to the iOS 27 beta connects; another phone on old iOS reproduces the failure. Doc `docs/reference/IOS26_VS_IOS27_BONJOUR.md`.

The instinct — "iOS 27 fixed a bug" — is backwards. iOS 27 did not gain a repair for the accessory; it gained machinery that happens to **tolerate** our non-stock advertisement. The head unit's advertisement is the broken side either way; old iOS simply stops forgiving it.

## 📊 The decompiled delta

Decompiled `-[CRCarKitServiceAgent fetchCarPlayControlAdvertisingForUSBWithReply:]` in both builds. Both pick the vehicle identically and both **construct** the `CARSessionRequestBonjourHost` on the phone — neither takes a host from the accessory. The delta is the display model behind it. **[decompile]** verified on `23G71` (26.6) + `24A5418b` (27.0) carkitd.

| | iOS 26 | iOS 27 |
|---|---|---|
| Display config | `buildDisplayConfig` @0x10006D680 = **one** displayConfig | @0x100066FB8 = per-display |
| Zoom | one scalar `zoomFactor` from `fetchCarCapabilitiesWithIdentifier` | `zoomFactorByDisplayIndex` dict |
| Cluster asset | none | full `updateClusterAssetIdentifier:assetVersion:sdkVersion:`, theme/Ultra |
| Session init | `initWithDisplayScaleMode:zoomFactor:` | `initWithDisplays:certificateSerialNumber:` |

Every part of the delta is about **multiple displays and the cluster** — exactly where this unit is not a stock car. We advertise a second display for the cluster altScreen (stream 111) with fabricated identifiers, and `clusterAssetIdentifier` is literally the cluster asset.

## ⚠️ The honest limit

> The causal link **advert → failure is [open]**, not proven. Two readings from strings alone were wrong and must not be repeated: iOS 26 is **not** "declarations only", and iOS 27 does **not** repair a host sent by the accessory. Critically, iOS 26 does **not** reject a multi-display array by count — `fetchScaledDisplays…displays:` @0x1000728B8 scales each display with no bail. iOS 26 only *lacks* the cluster-asset / per-display machinery.

And there is a layer confusion to keep straight: the advert is a **CarPlay-session-layer** thing, while `No Network Interface` (see [[USB Replug Recovery]]) is **USB-layer** (`carplay0` destroyed by an SI stack restart). So the failure the iOS gate produces could also be a timing race against the startup churn, not the advert itself.

## 💡 The decisive test

Turn the cluster-display advertisement off and retry on the phone that reproduces the failure. **[open]** — this is the cheap test that would settle it, not yet run to conclusion.

- Marker `/mnt/app/carplay_altscreen` absent, or `ALTSCREEN_ADVERT_DISPLAYS=0` in `carplay_startup.sh` (needs a dio-supervisor relaunch).
- If it then connects, our cluster-display advertisement is what iOS ≤ 26 chokes on. Until then, do not claim "26 rejects the advert" as fact.

<details>
<summary>IPSW tooling gotchas (cost hours)</summary>

- The beta ships as an OTA that is a `RIDIFF10` **delta** — its payload BOMs list the boot tree only, no system binaries.
- The full beta IPSW is indexed by **AppleDB**, not ipsw.me: `ipsw download appledb --os iOS --device iPhone18,3 --build 24A5418b --urls`.
- Extract by decrypting the fs DMG (`ipsw fw aea -o .`) then `hdiutil attach -readonly -nobrowse`; `ipsw extract --files` fails on AEA images.
- `ipsw` leaves multi-GB decrypted DMGs in `/var/folders/**/T/ipsw_extract_*` after a failure — clean them or the next run dies on "no space left on device". Artefacts: `Refferences/ios_carplay_diff/`.

</details>

Related: [[USB Replug Recovery]], [[Home]], [[Landmines]].

---
