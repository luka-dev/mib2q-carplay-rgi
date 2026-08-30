---
title: MHI2Q CarPlay - Knowledge Index
tags: [moc]
status: complete
---

# MHI2Q CarPlay - Knowledge Index

Map of Content for the reverse-engineering and implementation notes. Each note covers **one topic**;
every factual claim is validated against a source (code / firmware / iOS binary) and states only the
final verified fact. `reconciles:` frontmatter records which legacy docs were folded in.

> **All topics seeded [x]** - 29 notes. `(!)` items inside notes are real product TODOs, not doc gaps.

## [architecture](architecture.md) - process topology, threading, boot / init - build & deploy  [x]

## Hook - `libcarplay_hook.so`  [x]
- [iap2-interception](hook/iap2-interception.md) - recv/read hooks, FF-5A framing, Identify patch
- [bus-protocol](hook/bus-protocol.md) - localhost TCP :19810, sticky event/command frames
- [cover-art](hook/cover-art.md) - chunked JPEG reassembly, async decode -> VC picture
- [integration-seam](hook/integration-seam.md) - what LD_PRELOAD interposes vs stays stock; NME injection ABI; hardening

## Route guidance - RGD -> BAP  [x]
- [rgd-tlv](rgd/rgd-tlv.md) - iAP2 RouteGuidanceUpdate TLV map (0x5200-0x5204)
- [rgd-activation](rgd/rgd-activation.md) - route state machine, `visible_in_app`, gating
- [maneuver-mapping](rgd/maneuver-mapping.md) - full EManeuverType 0-53 -> BAP descriptor
- [bap-fctids](rgd/bap-fctids.md) - CarPlay-owned FctID matrix + gating
- [bargraph-sync](rgd/bargraph-sync.md) - distance fill + call-for-action blink
- [navsd-catalogue](rgd/navsd-catalogue.md) - complete NavSD FctID catalogue (1-56)

## Cluster  [x]
- [display-contexts](cluster/display-contexts.md) - dc[74]/dc[80], displayables 98/33/101/102, switch worker
- [compositing](cluster/compositing.md) - maneuver overlay over native map, HU->MOST->VC H.264
- [kdk-geometry](cluster/kdk-geometry.md) - KDK backings 101/102, stages, HU-side geometry table

## Input  [x]
- [touchpad-dpad](input/touchpad-dpad.md) - MMI touchpad -> DPAD bridge (CursorController)
- [steering-wheel](input/steering-wheel.md) - MFW roller: rotation = stock zoom, press = route-info toggle

## Deploy  [x]
- [supervisor-lifecycle](deploy/supervisor-lifecycle.md) - install map (what goes where), smartphone_integrator, renderer ownership
- [connect](deploy/connect.md) - USB / NCM / Bonjour connect flow + failure root cause
- [session-lifecycle](deploy/session-lifecycle.md) - session audit: watchdog-hang, USB pre-RTSP class, resilience risks R1-R4

## Maintenance  [x]
- [java-cleanup-audit](maintenance/java-cleanup-audit.md) - Java patch cleanup status; the remaining `forceGfxAvailable` reflection ladder

## Reverse engineering - iOS  [x]
- [accessoryd-rgd](re/ios/accessoryd-rgd.md) - ACCNav RGUpdate enum (accessoryd 23G71)
- [carkitd-bonjour](re/ios/carkitd-bonjour.md) - iOS 26 vs 27 connect divergence
- [maps-maneuvers](re/ios/maps-maneuvers.md) - Maps accNav enum + signed exit angle

## Reverse engineering - firmware (MIB2Q MU1316)  [x]
- [display-manager](re/firmware/display-manager.md) - DisplayManager + dmdt, window binding
- [komo-widget-video](re/firmware/komo-widget-video.md) - KOMO widget video + gfxAvailable gate
- [dsi-carkombi](re/firmware/dsi-carkombi.md) - DSICarKombi + DSIKombiSync2
- [vc-aio-arrow](re/firmware/vc-aio-arrow.md) - why the VC-native AIO arrow path is blocked (InfoStates=6 rejected) -> BAP HUD instead
- [phone-tab-gating](re/firmware/phone-tab-gating.md) - abandoned experiment: hiding the stock PHONE2 tab (why the lever failed)

---

## Verification

Every note carries a `status` recording how its facts were checked.

| status | meaning | notes |
|---|---|---|
| `verified-decompile` | confirmed by reading the disassembly/decompilation of the actual binary | rgd-tlv, rgd-activation, accessoryd-rgd, maps-maneuvers, carkitd-bonjour, display-manager, compositing, kdk-geometry (VC section) |
| `verified-trace` | confirmed against the actual on-device log / config | connect |
| `verified-source` | confirmed against this repo's source (ground truth for our own code) | architecture, iap2-interception, bus-protocol, cover-art, integration-seam, maneuver-mapping, bap-fctids, bargraph-sync, display-contexts, kdk-geometry (HU), touchpad-dpad, steering-wheel, supervisor-lifecycle, session-lifecycle, java-cleanup-audit, dsi-carkombi, navsd-catalogue |
| `partially-verified` | code paths confirmed; some symbols only string-level / inferred | komo-widget-video (gfx-gate chain) |
| `from-re-notes` | carried faithfully from prior RE notes; not re-verified in the binary this pass | vc-aio-arrow |
| `abandoned` | investigation record of a feature that was tried and rolled back (not shipped) | phone-tab-gating |

**Corrections the verification pass caught** (wrong facts inherited from legacy docs, now fixed):
- `display-manager` - invented symbol names (`display_create_window` / `screen_manage_window`) -> real `CTerminal` + `CScreenHandler::evtNewWindow` / `CASIMostEncoder::setActiveDisplayable`.
- `komo-widget-video` - fabricated `KVS_RGI2 363x260` -> real `KVS_FPK 210x153 / 328x181`.
- `connect` - denominator 256 not 257; the ~3 s lag is Bonjour resolution, not addressing (addressing ~8 ms).
