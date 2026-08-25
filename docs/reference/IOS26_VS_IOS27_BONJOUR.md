# Why iOS 26 fails to connect and iOS 27 does not

Reproduced by the user, both directions, 2026-08-23: the wife's phone updated to the iOS 27 beta
started connecting; a different phone left on old iOS reproduced her exact failure. So the iOS
version is causal, not correlated.

## The diff

Binaries compared (same build family, same arch):

| daemon | iOS 26.6 `23G71` | iOS 27.0b6 `24A5418b` | delta |
|---|---:|---:|---|
| `accessoryd` | 4,060,672 | 4,306,192 | +245 KB |
| `carkitd` | 1,278,000 | 1,287,040 | +9 KB |
| `caraccessoryd` | 781,824 | 1,121,168 | +339 KB |
| `airplayd` | 89,568 | 89,840 | +272 B |

`caraccessoryd`'s growth is almost entirely a new Swift `CAFDLatencyMonitor` (write-timeout and
high-frequency-alert sampling) — unrelated to session setup.

The relevant change is in **`carkitd`**, and decompiling it (2026-08-24) settles what the
strings could only hint at. Two earlier readings in this note were wrong and are corrected below.

### `-[CRCarKitServiceAgent fetchCarPlayControlAdvertisingForUSBWithReply:]`

Both versions do the same thing up to picking the vehicle: `_isRestricted:`, then walk
`messagingConnector.connectedVehicles` for one with `transportType == 1`, `supportsUSBCarPlay`
and **not** `supportsCarPlayConnectionRequest`, then
`vehicleMatchingMessagingVehicle:inVehicles:` against `vehicleStore.allStoredVehicles`.

They differ in how the display configuration behind the BonjourHost is built.

**iOS 26.6** builds it synchronously from the stored vehicle:

    displayScaleMode = storedVehicle.displayScaleMode                 // 2 when no vehicle
    caps  = +[CRCarPlayCapabilities fetchCarCapabilitiesWithIdentifier: vehicle.identifier.UUIDString]
    zoom  = caps.zoomFactor                                           // ONE value for the car
    cfg   = [CARSessionRequestDisplayConfiguration initWithDisplayScaleMode: zoom Factor: zoom]
    host  = [CARSessionRequestBonjourHost initWithDisplayConfiguration: cfg]

**iOS 27.0b6** fetches capabilities asynchronously and keys them differently:

    [carCapabilitiesManager fetchCapabilitiesWithCertificateSerialNumber: msgVehicle.certificateSerialNumber
                                                  clusterAssetIdentifier: storedVehicle.clusterAssetIdentifier
                                                           callbackQueue: main
                                                       completionHandler: ^(caps) {
        cfg  = [CARSessionRequestDisplayConfiguration initWithDisplayScaleMode: storedVehicle.displayScaleMode
                                                     zoomFactorByDisplayIndex: caps.zoomFactorByDisplayIndex]
        host = [CARSessionRequestBonjourHost initWithDisplayConfiguration: cfg]
    }]

So, proven:

1. `zoomFactor` (one per car) became **`zoomFactorByDisplayIndex`** (one per display);
2. the capabilities lookup key moved from the **vehicle UUID** to the **MFi certificate serial
   number plus `clusterAssetIdentifier`**;
3. `updateBonjourHostForVehicle:reply:` is new, but it is a **re-publish** path for when a
   vehicle's display configuration changes — it runs the same
   `fetchCapabilitiesWithCertificateSerialNumber:clusterAssetIdentifier:` and rebuilds the host.
   It is *not* a repair path for a malformed accessory advertisement.

### Corrections to earlier readings in this file

- "iOS 26 carries only declarations, no implementation" — **wrong**. `fetchCarPlayControlAdvertising*`
  is implemented in both.
- "iOS 27 repairs a broken BonjourHost coming from the head unit" — **wrong**. Neither version
  takes a host *from* the accessory here; both **construct** it on the phone. The delta is the
  display model used to construct it.

### What this points at

Every part of the delta is about **multiple displays and the cluster**, and that is exactly where
this head unit is not a stock car: we advertise a second display for the cluster altScreen
(stream 111) with fabricated identifiers, and `clusterAssetIdentifier` is literally the cluster
asset. A single `zoomFactor` for a car that presents two displays is the kind of mismatch iOS 27's
per-display model removes.

### One bug this already found

Independent of any iOS theory, the cluster's advertised physical size was simply wrong.

| display | pixels | diagonal | px/mm | dpi | physical |
|---|---|---|---:|---:|---|
| main MMI | 1024x480 | 8.25" | 5.40 | 137 | 190 x 89 mm |
| cluster, real | 1440x542 | 12.3" | 4.93 | 125 | 292 x 110 mm |
| cluster, **as advertised** | 1440x542 | — | 8.00 | **203** | **180 x 68 mm** |

180 x 68 implies a **7.6"** cluster. It also inverts the density relationship: a real Audi presents
a cluster slightly *coarser* than its main screen (**0.91x**); we claimed **1.47x**. Fixed to
292 x 110 in `hook/altscreen/altscreen_hook.c` (`CLUSTER_PHYS_W/H`).

**This is a correctness fix, not a known fix for the connect failures — do not rebuild a causal
story on it.** An earlier revision of this note argued that a single car-wide `zoomFactor` cannot
reconcile a 1.47x density mismatch. The decompilation does not support that: iOS <= 26 takes the
zoom from the phone's own `+[CRCarPlayCapabilities fetchCarCapabilitiesWithIdentifier:<vehicle
UUID>]` store, **not** from our `widthPhysical`/`heightPhysical`. Our density never enters that
path. Where the advertised physical size *is* read is CarKit's display parsing, which has not been
decompiled for this field.

What the decompilation *does* imply is worth a separate test: if iOS <= 26 derives the zoom from a
capabilities record keyed by **vehicle UUID**, the state of that record on the phone is a variable.
Forgetting the car in CarPlay settings and re-pairing resets it.

**Where the main panel's physical size comes from — and it is not zero.** `dio_manager` logs it at
`<START_CARPLAY>` as `Physical Dimensions(w,h)` from `CDSICarplayImpl`, i.e. the HMI supplies it
over DSI in `ServiceConfiguration.physicalDisplayWidth/Height`
(`org/dsi/ifc/carplay/ServiceConfiguration`). The Java side fills it from
`ITerminalModeConfiguration.getCarPlayPhysicalDisplay{Width,Height}`, which in
`AbstractTerminalModeConfiguration` is:

    getPhysicalDisplayWidth():   sysConst(522) == 2 ? 188 : 152
    getPhysicalDisplayHeight():  sysConst(522) == 2 ?  88 :  91

(`EvoTMConfiguration` overrides the `screenRes == 4` case to 145 x 73.)

So stock really does advertise a physical size, and the numbers line up with the hardware:

| display | pixels | physical | px/mm | dpi | implied diagonal |
|---|---|---|---:|---:|---:|
| main, stock `sysConst522==2` | 1024x480 | **188 x 88** | 5.447 | 138.3 | **8.17"** |
| main, other branch | 1024x480 | 152 x 91 | 6.737 | 171.1 | 6.61" |
| cluster, ours **now** | 1440x542 | **292 x 110** | 4.932 | 125.3 | **12.28"** |
| cluster, ours **before** | 1440x542 | 180 x 68 | 8.000 | 203.2 | 7.57" |

Both confirmed values recover their real diagonals — 8.17" against the owner's 8.25/8.3", and
12.28" against the confirmed 12.3" / 31.2 cm. Density ratio cluster/main is **0.905** now and was
**1.469**. Resolutions are firmware-confirmed from `qcdisplaycfg_MMX2.xml` (1440x542 LVDS cluster,
1024x480 main).

The probe in `main_disp_modernize` is kept for a smaller question: which `sysConst(522)` branch this
car takes, and that dio passes the value into `/info` unchanged.

`maxFPS` was the same kind of mismatch and is fixed too: `dio_manager.json` declares
`"screen": { "maxFPS": 30 }` for the main screen, and 30 is also what we actually deliver on the
cluster — `AltScreenModule.CLUSTER_FPS` is 30, `maneuver_render`'s `CR_TARGET_FPS` is 30, and
`altscreen_render` presents at 30. Advertising 60 promised twice the rate that exists, so the
cluster now advertises 30.

Either way, claiming a 7.6" 203 dpi Virtual Cockpit was wrong on its own merits.

**Cheap decisive test, no new tooling:** we already gate the second-display advertisement behind
an env knob. Set `ALTSCREEN_ADVERT_DISPLAYS=0` in `carplay_startup.sh` (`env_on()` checks the
value, so `=0` really disables it; the change needs a dio-supervisor relaunch) and try the phone
that reproduces the failure. If it then connects reliably, our cluster-display advertisement is
what iOS <= 26 chokes on.

## Next measurement (unchanged, now better motivated)

On the unit during a **failing** cycle:

    dns-sd -B _airplay._tcp          # is the service published at all?
    dns-sd -L CarPlay _airplay._tcp  # what host/port does it resolve to?

and whether the record survives `stopncm.sh` / `startncm.sh` recreating `carplay0` each dio
generation — the working hypothesis is that `mdnsd` must re-register per cycle and loses the race
against SI killing dio at ~15 s.

## Artefacts

`Refferences/ios_carplay_diff/` — both daemon sets plus `analysis/*.strings` and the
`new_<daemon>.txt` "added in 27" lists. Re-running the comparison is `comm -13` on the sorted
string sets.

Note for a repeat: the beta ships only as an OTA on Apple's public feed, and that OTA is a
`RIDIFF10` delta whose payload BOMs contain the boot tree only — no system binaries. The full
beta IPSW exists but is indexed by **AppleDB**, not ipsw.me:
`ipsw download appledb --os iOS --device iPhone18,3 --build 24A5418b --urls`.
`ipsw extract` leaves multi-GB decrypted DMGs in `/var/folders/**/T/ipsw_extract_*` when it fails
or is interrupted; clean them up or the next run dies on "no space left on device".


---

# Decompiler verification on the exact 23G71 (26.6) and 24A5418b (27.0) binaries — 2026-08-25

Both `carkitd` re-analyzed side by side (`ios_carplay_diff/ios26|ios27/.../usr/libexec/carkitd.i64`).
This supersedes the earlier strings-only reading.

## Proven (decompiler, not strings)

**iOS 26.6 `ios26_buildDisplayConfig_singleZoom` @0x10006D680** builds exactly ONE
`CARSessionRequestDisplayConfiguration` with ONE `displayScaleMode` and ONE **scalar** `zoomFactor`,
taken from `+[CRCarPlayCapabilities fetchCarCapabilitiesWithIdentifier:]` for the whole vehicle.
No loop, no per-display index, no cluster-asset handling anywhere in this path.

**iOS 27.0 start-session build @0x100066FB8** uses a `zoomFactorByDisplayIndex` **dictionary**
(`CRZoomFactorByDisplayIndexKey`), extracts a fallback scalar for compat, and then does full
cluster-asset work: `clusterAssetVersion`, `SDKVersion`,
`_mainQueue_updateClusterAssetIdentifier:assetVersion:sdkVersion:forVehicle:`, `supportsThemeAssets`,
and the "Ultra-capable HU delivered Classic…" opt-out. 27 also adds the initializer
`initWithDisplays:certificateSerialNumber:` and `initWithDisplayScaleMode:zoomFactorByDisplayIndex:`
that 26 does not have (26 has only `initWithDisplayScaleMode:zoomFactor:`).

Net: the display-capability model diverges **structurally** — 26 is single-display / scalar-zoom /
no-cluster-asset; 27 is per-display-zoom-dict + cluster-asset + theme/Ultra.

## What this does NOT prove (honesty boundary)

- **26 does not reject a multi-display advert by count.**
  `-[CRCarKitServiceAgent fetchScaledDisplaysWithCertificateSerialNumber:displays:reply:]`
  @0x1000728B8 hands the whole `displays` array to `CRScreenScaleHeuristics
  scaledDisplays:withDisplayScaling:reply:` — it scales each element, no count guard, no bail.
  So a bare extra display is not obviously fatal to 26. What 26 categorically lacks is the
  **cluster-asset / per-display-zoom** machinery — exactly what the cluster altScreen relies on
  (our advert carries a `clusterAssetIdentifier`).

- **This is a different layer from the observed failure.** `USB eligible: N (No Network Interface)`
  is a USB-layer symptom (carplay0 destroyed = SI stack restart, see SI_STACK_RESTART_RE.md). The
  display advert is consumed later, at the CarPlay-session layer, after NCM is already up. For the
  advert incompatibility to cause `No Network Interface`, 26 would have to fail the session hard
  enough that dio errors out and SI restarts the stack — plausible, multi-hop, unproven. It is
  equally possible the 26/27 split is a timing race against the startup-budget churn and not the
  advert at all.

## Decisive test (now enabled)

`carplay_startup.sh` gates the cluster advert behind `/mnt/app/carplay_altscreen` (off by default).
With the marker absent, base CarPlay is stock and should connect on every iOS. If the iOS-26 phone
that reproduced the failure then connects, the cluster-display advert was the 26 blocker and the
above chain is confirmed. Until that A/B run, the structural divergence is proven but its causal
role in the connect failure is not.
