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

The relevant change is in **`carkitd`**. iOS 26 contains only the *declarations*:

    _OBJC_CLASS_$_CARSessionRequestBonjourHost
    v24@0:8@?<v@?B@"CARSessionRequestBonjourHost"@"NSError">16
    v32@0:8@"NSString"16@?<v@?B@"CARSessionRequestBonjourHost"@"NSError">24

iOS 27 has the class, the same signatures, **and a live implementation**:

    fetchCarPlayControlAdvertisingForUSBWithReply:
    fetchCarPlayControlAdvertisingForWiFiUUID:reply:
    updateBonjourHost:completion:
    updateBonjourHostForVehicle:reply:
    [DISPLAY_CONFIGURATION] fetchCarPlayControlAdvertisingForUSBWithReply: YES for vehicle %@, host: %@
    [DISPLAY_CONFIGURATION] fetchCarPlayControlAdvertisingForUSBWithReply: Requested advertising with host
    [DISPLAY_CONFIGURATION] fetchCarPlayControlAdvertisingForUSBWithReply: _isRestricted, should not advertise
    [DISPLAY_CONFIGURATION] updateBonjourHostForVehicle: updating BonjourHost for vehicle %@ with displayScaleMode=%ld
    [DISPLAY_CONFIGURATION] updateBonjourHostForVehicle: sending updated BonjourHost to AirPlay: %@
    [DISPLAY_CONFIGURATION] updateBonjourHostForVehicle: successfully updated BonjourHost for vehicle %@
    [DISPLAY_CONFIGURATION] updateBonjourHostForVehicle: failed to update BonjourHost in AirPlay: %@

In iOS 27 `carkitd` decides for itself whether to advertise CarPlay control over USB and **with
which host**, then pushes a corrected `BonjourHost` into AirPlay. In iOS 26 that plumbing is
declared but not driven: the phone must take the head unit's `_airplay._tcp` advertisement as it
finds it.

## What this means for us

The head unit is still the thing that is wrong. iOS 27 merely stopped depending on it.

This reconciles two observations that looked contradictory:

- 2026-08-21: the HU was not advertising `CarPlay._airplay._tcp.local` at all — `carplay0` carried
  ~5 packets per failing cycle (IPv6 Neighbor Discovery only), and `Created APEndpointCarPlay`
  appeared exactly once across a whole evening of attempts. A **head-unit** reboot fixed it, and
  both phones then connected first try.
- 2026-08-23: iOS version alone flips the behaviour on the same head unit.

Both are true if our Bonjour registration on `carplay0` is unreliable — missing, stale, or
carrying a host record that does not resolve. iOS 26 gives up after ~10 s; iOS 27 repairs it.

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
