---
title: carkitd - iOS 26 vs 27 connect divergence
tags: [re, ios, connect, verified]
status: verified-decompile
sources:
  - firmware: carkitd (iOS 26.6 / 23G71 vs 27.0b6 / 24A5418b)
  - firmware: -[CRCarKitServiceAgent fetchCarPlayControlAdvertisingForUSBWithReply:] (iOS 26 @ 0x10006d1d4, iOS 27 @ 0x1000678dc)
  - firmware: iOS 26 sync display-config helper @ 0x10006d680; iOS 27 async completion @ 0x100067ca8
  - firmware: -[CRCarKitServiceAgent updateBonjourHostForVehicle:reply:] (iOS 27 only @ 0x10006bfbc)
reconciles:
  - docs/reference/IOS26_VS_IOS27_BONJOUR.md
---

# carkitd - iOS 26 vs 27 connect divergence

Reproduced both directions: a phone on the iOS 27 beta started connecting; a phone left on old iOS
reproduced the failure. The iOS version is causal. `carkitd` builds the CarPlay control advertising;
the two versions differ in **how the display configuration behind the BonjourHost is built**.

## Context

> [[connect]] - airplayd needs a network + advert -> **carkitd** builds the display config -> BonjourHost.

## The delta (both versions, up to picking the vehicle, are identical)

`fetchCarPlayControlAdvertisingForUSBWithReply:` dispatches to the main queue, calls `_isRestricted:`,
then walks `messagingConnector.connectedVehicles` for one with `transportType==1`, `supportsUSBCarPlay`,
not `supportsCarPlayConnectionRequest`, then matches the stored vehicle via
`vehicleMatchingMessagingVehicle:inVehicles:` (against `vehicleStore.allStoredVehicles`). Then:

- **iOS 26.6** - synchronous, from the stored vehicle:
  `zoom = +[CRCarPlayCapabilities fetchCarCapabilitiesWithIdentifier:<vehicle UUIDString>].zoomFactor`
  -> `CARSessionRequestDisplayConfiguration(displayScaleMode, **zoomFactor**)`. **One zoom per car.**
- **iOS 27.0b6** - asynchronous, keyed differently:
  `carCapabilitiesManager fetchCapabilitiesWithCertificateSerialNumber:<messaging vehicle's MFi serial>
  clusterAssetIdentifier:<stored vehicle's cluster asset> callbackQueue: completionHandler:` -> the
  completion builds config with **`zoomFactorByDisplayIndex`**
  (`initWithDisplayScaleMode:zoomFactorByDisplayIndex:`). **One zoom per display.**

Proven: (1) `zoomFactor` (per car) became `zoomFactorByDisplayIndex` (per display); (2) the lookup key
moved from vehicle UUID to **MFi cert serial + `clusterAssetIdentifier`**.

## Why it points at this head unit

Every part of the delta is about **multiple displays and the cluster** - exactly where this HU is not
a stock car. A single car-wide `zoomFactor` is the kind of mismatch iOS 27's per-display model removes.
Causal role still open, but the direction is clear.

## Corrections (do not rebuild these)

- iOS <= 26 takes the zoom from the phone's own capabilities store (keyed by vehicle UUID), **not**
  from our advertised `widthPhysical`/`heightPhysical`. Our density never enters that path.
- `updateBonjourHostForVehicle:reply:` (iOS 27 only) is a **re-publish** path: it re-looks-up the stored
  vehicle, re-runs `fetchCapabilitiesWithCertificateSerialNumber:clusterAssetIdentifier:...`, rebuilds the
  DisplayConfiguration + BonjourHost, and sends it via `sessionRequestClient updateBonjourHost:completion:`.
  It is **not** a repair path for a malformed accessory advert (its only failure exits are nil identifier
  -> error 9 and vehicle-not-in-store -> error 6).

## One real bug found along the way

The cluster's advertised physical size was simply wrong (203 dpi / ~7.6\" implied vs the real 12.3\" /
125 dpi). Corrected to 292x110 mm. This is a **correctness fix, not a proven connect fix** - the
advertised physical size is read by CarKit display parsing (not decompiled for that field).
