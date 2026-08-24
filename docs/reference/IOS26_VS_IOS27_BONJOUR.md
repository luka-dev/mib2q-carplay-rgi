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
a cluster slightly *coarser* than its main screen (125/137 = **0.91x**); we claimed **1.48x**.
With one car-wide `zoomFactor` no value can be right for a 1.48x mismatch. Fixed to 292 x 110 in
`hook/altscreen/altscreen_hook.c` (`CLUSTER_PHYS_W/H`), which also lands the ratio at 0.91x.

**What is confirmed and what is not.** Both resolutions are firmware-confirmed -
`qcdisplaycfg_MMX2.xml` declares `uVisWidth/uVisHeight` of 1440x542 for the LVDS cluster panel and
1024x480 for the main. The cluster diagonal is confirmed by the owner as 12.3" /
31.2 cm, which puts 1440x542 at 4.932 px/mm = 125.3 dpi, i.e. 292.0 x 109.9 mm - matching the
committed `CLUSTER_PHYS_W/H` of 292 x 110 exactly. Against the main's 5.397 px/mm / 137.1 dpi that
is a density ratio of **0.914** (a bigger screen is legitimately coarser); the old 180 x 68 claimed
**1.48**. The one remaining unknown is the main side: **nothing in the firmware states a physical size for either panel**:
the only nearby setting is Android Auto's `gal.json`, which leaves `physicalDisplayWidth` at 0 with
the comment "use the values provided by the HMI".

So it is not yet known what stock reports in the main display's `widthPhysical`. If it reports 0,
the phone has no density for display 0 and the cluster's numbers cannot be inconsistent with it -
which would leave `CLUSTER_PHYS_W/H` merely truthful rather than the mechanism. `main_disp_modernize`
now logs the stock main dict once (`stock main panel: … px, physical … mm`); it needs
`touch /mnt/app/carplay_verbose` because production runs at WARN.

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
