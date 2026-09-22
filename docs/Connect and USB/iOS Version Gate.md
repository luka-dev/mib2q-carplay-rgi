# iOS Version Gate

*Connect failures on this head unit were iOS-version-gated — iOS 27 carkitd tolerates our CarPlay advertisement, iOS 26 does not, and the head unit's advertisement is still the suspect side.*

---

## 🔍 The observation

Connect failures on the A5 head unit are iOS-version-gated. **[car]** verified both directions 2026-08-23: a phone updated to the iOS 27 beta connects; another phone on old iOS reproduces the failure. Source: `IOS26_VS_IOS27_BONJOUR`.

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

Details of the two flows **[decompile]**:

- Both versions run `_isRestricted:`, then walk `messagingConnector.connectedVehicles` for one with `transportType == 1`, `supportsUSBCarPlay` and **not** `supportsCarPlayConnectionRequest`, then `vehicleMatchingMessagingVehicle:inVehicles:` against `vehicleStore.allStoredVehicles`.
- iOS 26.6 builds synchronously: `displayScaleMode = storedVehicle.displayScaleMode` (2 when no vehicle), `caps = +[CRCarPlayCapabilities fetchCarCapabilitiesWithIdentifier: vehicle.identifier.UUIDString]`, one `caps.zoomFactor`, `CARSessionRequestDisplayConfiguration`, then `CARSessionRequestBonjourHost initWithDisplayConfiguration:`.
- iOS 27.0b6 fetches asynchronously through `carCapabilitiesManager fetchCapabilitiesWithCertificateSerialNumber:clusterAssetIdentifier:callbackQueue:completionHandler:` — the lookup key moved from the **vehicle UUID** to the **MFi certificate serial plus `clusterAssetIdentifier`** — and builds the config from `zoomFactorByDisplayIndex` (`CRZoomFactorByDisplayIndexKey`), extracting a fallback scalar for compat.
- The new `updateBonjourHostForVehicle:reply:` is a re-publish path for when a vehicle's display configuration changes (same fetch, rebuilt host), **not** a repair path for a malformed accessory advertisement.

| Daemon | iOS 26.6 `23G71` | iOS 27.0b6 `24A5418b` | Delta |
|---|---:|---:|---|
| `accessoryd` | 4,060,672 | 4,306,192 | +245 KB |
| `carkitd` | 1,278,000 | 1,287,040 | +9 KB — the relevant change |
| `caraccessoryd` | 781,824 | 1,121,168 | +339 KB, almost all a new Swift `CAFDLatencyMonitor` (write-timeout / high-frequency-alert sampling), unrelated to setup |
| `airplayd` | 89,568 | 89,840 | +272 B |

Because iOS ≤ 26 keys its zoom on a capabilities record stored on the phone by vehicle UUID, that record's state is itself a variable: forgetting the car in CarPlay settings and re-pairing resets it. **[open]** untested.

Sources: `IOS26_VS_IOS27_BONJOUR`

## 📊 Correctness fixes the diff turned up

Independent of any iOS theory, the cluster display advertised the wrong physical size. **[code]** now `CLUSTER_PHYS_W 292`, `CLUSTER_PHYS_H 110` (`hook/altscreen/altscreen_hook.c:251-252`, applied at `:415-416`).

| Display | Pixels | Physical | px/mm | dpi | Implied diagonal |
|---|---|---|---:|---:|---:|
| main, stock `sysConst(522)==2` | 1024×480 | 188 × 88 mm | 5.447 | 138.3 | 8.17" (owner: 8.25/8.3") |
| main, other branch | 1024×480 | 152 × 91 mm | 6.737 | 171.1 | 6.61" |
| cluster, ours now | 1440×542 | 292 × 110 mm | 4.932 | 125.3 | 12.28" (confirmed 12.3" / 31.2 cm) |
| cluster, ours before | 1440×542 | 180 × 68 mm | 8.000 | 203.2 | 7.57" |

A real Audi presents a cluster slightly coarser than its main screen; the cluster/main density ratio is now **0.905**, it was **1.469**. Resolutions are firmware-confirmed from `qcdisplaycfg_MMX2.xml`. The main panel's size is not zero: `dio_manager` logs `Physical Dimensions(w,h)` at `<START_CARPLAY>` from `CDSICarplayImpl`, fed over DSI `ServiceConfiguration.physicalDisplayWidth/Height` by `AbstractTerminalModeConfiguration.getPhysicalDisplayWidth()` = `sysConst(522) == 2 ? 188 : 152` and height `? 88 : 91` (`EvoTMConfiguration` overrides `screenRes == 4` to 145 × 73). The probe in `main_disp_modernize` stays to learn which `sysConst(522)` branch this car takes.

`maxFPS` had the same kind of mismatch: `dio_manager.json` declares `"screen": { "maxFPS": 30 }` for main, and 30 is what the cluster delivers (`AltScreenModule.CLUSTER_FPS`, `maneuver_render` `CR_TARGET_FPS`, `altscreen_render` present rate). The cluster now advertises 30 instead of 60 (`altscreen_hook.c:408-412`). **[code]**

> ⚠️ These are correctness fixes, not known fixes for the connect failure. iOS ≤ 26 takes zoom from its own capabilities store, not from our `widthPhysical`/`heightPhysical`; where CarKit reads the advertised physical size has not been decompiled.

Sources: `IOS26_VS_IOS27_BONJOUR`

## ⚙️ What the hook advertises now

**[code]**

- A second display for the cluster altScreen (stream 111), gated in `advert_preflight()` by `ALTSCREEN_ADVERT_DISPLAYS` and a usable stock main display (`hook/altscreen/altscreen_hook.c:717-721`, called from the control-SETUP path at `:1842`) and switched on by `export ALTSCREEN_ADVERT_DISPLAYS=1` plus `export ALTSCREEN_HANDLE_ALT=1` (`deploy/smartphone_integrator/carplay_startup.sh:53-54`).
- The cluster display carries a synthesized constant uuid `C1057E12-…` that must differ from the main one (`altscreen_hook.c:422-428`).
- The main display is **no longer fabricated**: `uid` and `initialVideoStreams` uuid are copied from the real stock `ScreenCopyMain` uuid, and the advert is refused if that uuid is missing (`altscreen_hook.c:550-579`). The old `A0000000…` main-uid constant — a candidate in the 2026-08-23 era — is gone.

## ⚠️ The honest limit

> The causal link **advert → failure is [open]**, not proven. Two readings from strings alone were wrong and must not be repeated: iOS 26 is **not** "declarations only", and iOS 27 does **not** repair a host sent by the accessory. Critically, iOS 26 does **not** reject a multi-display array by count — `fetchScaledDisplays…displays:` @0x1000728B8 scales each display with no bail. iOS 26 only *lacks* the cluster-asset / per-display machinery.

And there is a layer confusion to keep straight: the advert is a CarPlay-session-layer thing, while `No Network Interface` (see [[USB and NCM Connect]]) is USB-layer (`carplay0` destroyed by an SI stack restart, [[SI Stack and Replug Race]]). So the failure the iOS gate produces could also be a timing race against the startup churn, not the advert itself.

The owner reported connection and reconnection closed on the current car on 2026-09-17 (`TODO.md:5-10`). That report does not say which iOS version was tested, so it does not settle this gate. **[open]**

## 💡 The decisive test

Turn the cluster-display advertisement off and retry on a phone that reproduces the failure. **[open]** — the cheap test that would settle it, not yet run to conclusion.

- Set `export ALTSCREEN_ADVERT_DISPLAYS=0` in `/mnt/app/root/hooks/carplay_startup.sh`. The hook checks the value, not mere presence, so `0`, `false`, `no` and `off` all disable it (`env_on()`, `altscreen_hook.c:704-711`). There is no marker-file switch — no `/mnt/app/carplay_altscreen` check exists in `hook/`, `deploy/` or the installer.
- The wrapper is SI's `carplay` child `exec` (`deploy/smartphone_integrator/carplay_child.json:2-3`) and `exec`s `dio_manager` with the exported env (`carplay_startup.sh:95`), so the edit applies to the next dio generation. **[open]** not confirmed on the unit that no reboot is needed.
- If it then connects, our cluster-display advertisement is what iOS ≤ 26 chokes on. Until then, do not claim "26 rejects the advert" as fact.
- Superseded: the 2026-08-25 source says `carplay_startup.sh` gates the advert behind `/mnt/app/carplay_altscreen` "off by default". The current gate is the env value above and it ships ON.

A second, complementary measurement planned on the unit during a failing cycle: `dns-sd -B _airplay._tcp` (is the service published at all?) and `dns-sd -L CarPlay _airplay._tcp` (host/port it resolves to), and whether the record survives `stopncm.sh`/`startncm.sh` recreating `carplay0` each generation — `mdnsd` would have to re-register per cycle and could lose the race against SI killing dio at ~15 s. **[open]**

<details>
<summary><strong>IPSW tooling gotchas (cost hours)</strong></summary>

- The beta ships as an OTA that is a `RIDIFF10` delta — its payload BOMs list the boot tree only, no system binaries.
- The full beta IPSW is indexed by AppleDB, not ipsw.me: `ipsw download appledb --os iOS --device iPhone18,3 --build 24A5418b --urls`.
- Extract by decrypting the fs DMG (`ipsw fw aea -o .`) then `hdiutil attach -readonly -nobrowse`; `ipsw extract --files` fails on AEA images.
- `ipsw` leaves multi-GB decrypted DMGs in `/var/folders/**/T/ipsw_extract_*` after a failure — clean them or the next run dies on "no space left on device". Artefacts: `Refferences/ios_carplay_diff/` — both daemon sets, `analysis/*.strings` and the `new_<daemon>.txt` "added in 27" lists; the `carkitd.i64` databases sit under `ios26|ios27/.../usr/libexec/`. Re-running the strings comparison is `comm -13` on the sorted string sets.

</details>

---

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

- `IOS26_VS_IOS27_BONJOUR` — daemon diff, decompiled `carkitd` flows for 26.6 and 27.0, physical-size and `maxFPS` fixes, the honesty boundary, IPSW gotchas.
- `CONNECT_FAILURE_ROOT_CAUSE` — the phone-side `No Network Interface` verdict that moved the failure to the USB layer (canonical in [[USB and NCM Connect]]).

Related: [[USB Replug Recovery]], [[USB and NCM Connect]], [[AltScreen Cluster Video]], [[Home]], [[Landmines]].
