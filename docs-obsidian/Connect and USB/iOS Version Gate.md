# iOS Version Gate

*Connect failures on this head unit were iOS-version-gated — iOS 27 carkitd tolerates our CarPlay advertisement, iOS 26 does not, and the head unit's advertisement is still the suspect side.*

---

## 🔍 The observation

Connect failures on the A5 head unit are iOS-version-gated. **[car]** verified both directions 2026-08-23: a phone updated to the iOS 27 beta connects; another phone on old iOS reproduces the failure. Doc `docs-obsidian/Sources/reference/IOS26_VS_IOS27_BONJOUR.md`.

The instinct — "iOS 27 fixed a bug" — is backwards. iOS 27 did not gain a repair for the accessory; it gained machinery that happens to tolerate our non-stock advertisement. The head unit's advertisement is the suspect side either way; old iOS simply stops forgiving it.

## 📊 The decompiled delta

Decompiled `-[CRCarKitServiceAgent fetchCarPlayControlAdvertisingForUSBWithReply:]` in both builds. Both pick the vehicle identically and both construct the `CARSessionRequestBonjourHost` on the phone — neither takes a host from the accessory. The delta is the display model behind it. **[decompile]** verified on `23G71` (26.6) + `24A5418b` (27.0) carkitd.

| | iOS 26 | iOS 27 |
|---|---|---|
| Display config | `buildDisplayConfig` @0x10006D680 = one displayConfig | @0x100066FB8 = per-display |
| Zoom | one scalar `zoomFactor` from `fetchCarCapabilitiesWithIdentifier` | `zoomFactorByDisplayIndex` dict |
| Cluster asset | none | full `updateClusterAssetIdentifier:assetVersion:sdkVersion:`, theme/Ultra |
| Session init | `initWithDisplayScaleMode:zoomFactor:` | `initWithDisplays:certificateSerialNumber:` |

Every part of the delta is about multiple displays and the cluster — exactly where this unit is not a stock car.

**[code]** What the current hook advertises:

- A second display for the cluster altScreen (stream 111), gated by `ALTSCREEN_ADVERT_DISPLAYS` (`hook/altscreen/altscreen_hook.c:721`) and switched on by `export ALTSCREEN_ADVERT_DISPLAYS=1` plus `export ALTSCREEN_HANDLE_ALT=1` (`deploy/smartphone_integrator/carplay_startup.sh:53-54`).
- The cluster display carries a synthesized constant uuid `C1057E12-…` that must differ from the main one (`altscreen_hook.c:424-428`).
- The main display is **no longer fabricated**: `uid` and `initialVideoStreams` uuid are copied from the real stock `ScreenCopyMain` uuid, and the advert is refused if that uuid is missing (`altscreen_hook.c:550-579`). The old `A0000000…` main-uid constant — a candidate in the 2026-08-23 era — is gone.

## ⚠️ The honest limit

> The causal link **advert → failure is [open]**, not proven. Two readings from strings alone were wrong and must not be repeated: iOS 26 is **not** "declarations only", and iOS 27 does **not** repair a host sent by the accessory. Critically, iOS 26 does **not** reject a multi-display array by count — `fetchScaledDisplays…displays:` @0x1000728B8 scales each display with no bail. iOS 26 only *lacks* the cluster-asset / per-display machinery.

And there is a layer confusion to keep straight: the advert is a CarPlay-session-layer thing, while `No Network Interface` (see [[USB Replug Recovery]]) is USB-layer (`carplay0` destroyed by an SI stack restart). So the failure the iOS gate produces could also be a timing race against the startup churn, not the advert itself.

The owner reported connection and reconnection closed on the current car on 2026-09-17 (`TODO.md:5-10`). That report does not say which iOS version was tested, so it does not settle this gate. **[open]**

## 💡 The decisive test

Turn the cluster-display advertisement off and retry on a phone that reproduces the failure. **[open]** — the cheap test that would settle it, not yet run to conclusion.

- Set `export ALTSCREEN_ADVERT_DISPLAYS=0` in `/mnt/app/root/hooks/carplay_startup.sh`. The hook checks the value, not mere presence, so `0`, `false`, `no` and `off` all disable it (`env_on()`, `altscreen_hook.c:704-711`). There is no marker-file switch — no `/mnt/app/carplay_altscreen` check exists in `hook/`, `deploy/` or the installer.
- The wrapper is SI's `carplay` child `exec` (`deploy/smartphone_integrator/carplay_child.json:2-3`) and `exec`s `dio_manager` with the exported env (`carplay_startup.sh:95`), so the edit applies to the next dio generation. **[open]** not confirmed on the unit that no reboot is needed.
- If it then connects, our cluster-display advertisement is what iOS ≤ 26 chokes on. Until then, do not claim "26 rejects the advert" as fact.

<details>
<summary><strong>IPSW tooling gotchas (cost hours)</strong></summary>

- The beta ships as an OTA that is a `RIDIFF10` delta — its payload BOMs list the boot tree only, no system binaries.
- The full beta IPSW is indexed by AppleDB, not ipsw.me: `ipsw download appledb --os iOS --device iPhone18,3 --build 24A5418b --urls`.
- Extract by decrypting the fs DMG (`ipsw fw aea -o .`) then `hdiutil attach -readonly -nobrowse`; `ipsw extract --files` fails on AEA images.
- `ipsw` leaves multi-GB decrypted DMGs in `/var/folders/**/T/ipsw_extract_*` after a failure — clean them or the next run dies on "no space left on device". Artefacts: `Refferences/ios_carplay_diff/`.

</details>

---

Related: [[USB Replug Recovery]], [[AltScreen Cluster Video]], [[Home]], [[Landmines]].
