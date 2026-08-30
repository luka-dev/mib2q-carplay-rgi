---
title: MU1316 NavSD FctID catalogue (complete, LSG 0x32)
tags: [rgd, bap, reference, verified]
status: verified-source
sources:
  - stock: FunctionIDsNavi.java (ID/name 1-56)
  - stock: FunctionRegistrationNavi.java (kinds)
  - stock: FunctionListNaviEvo.java (advertised variants)
  - code: java_patch/com/luka/carplay/rgd/GatedCombiService.java
  - code: java_patch/com/luka/carplay/rgd/BAPBridge.java
reconciles:
  - docs/reference/NAVSD_FCTID_MATRIX.md
---

# MU1316 NavSD FctID catalogue (complete, LSG 0x32)

The full stock Navigation-BAP function catalogue. The CarPlay-ownership subset and gating live in
[[bap-fctids]]; this is the exhaustive reference. Variant legend: `S`=Std `H`=HighPrem `M`=MMIKombi
`MH`=MMIKombiHUD; `All`=all four. China additionally disables FctID 47. MHI2Q VC ~ `H`, but the live
`FunctionList` (FctID 3) is the final capability truth.

| ID | Hex | Name | Kind | Advertised | Role | CarPlay policy |
|---:|---:|---|---|---|---|---|
| 1 | 01 | GetAll | protocol | All | status snapshot | framework |
| 2 | 02 | BAP_Config | property | All | BAP version/config | framework |
| 3 | 03 | FunctionList | property | All | runtime availability bitmap | capability truth |
| 4 | 04 | HeartBeat | protocol | All | LSG liveness | framework |
| 5-14 | - | Reserved/unknown | - | None | no function | do not use |
| 15 | 0F | FSG_OperationState | property | All | READY/INITIALIZING | stock (high-risk lifecycle) |
| 16 | 10 | CompassInfo | property | All | heading + N/NE... | stock |
| 17 | 11 | RG_Status | property | All | RG active; starts FctSync | **CarPlay** |
| 18 | 12 | DistanceToNextManeuver | property | All | next-turn dist + bargraph | **CarPlay** -> [[bargraph-sync]] |
| 19 | 13 | CurrentPositionInfo | property | All | lower-bar road/info line | **CarPlay** (gated only during RGI) |
| 20 | 14 | TurnToInfo | property | All | turn-to/signpost text | stock (not sent) |
| 21 | 15 | DistanceToDestination | property | All | trip distance | **CarPlay** (gated) |
| 22 | 16 | TimeToDestination | property | All | ETA / remaining | **CarPlay** (gated) |
| 23 | 17 | ManeuverDescriptor | property | S/H/MH | up to 3 maneuvers | **CarPlay** -> [[maneuver-mapping]] |
| 24 | 18 | LaneGuidance | array | S/H/MH | lane arrows | **CarPlay** |
| 25 | 19 | TMCinfo | property | S/H | traffic messages | stock |
| 26 | 1A | MagnetFieldZone | property | None | compass calib region | do not use |
| 27 | 1B | Calibration | property | None | compass calib state | do not use |
| 28 | 1C | ASG_Capabilities | property | None | capabilities | framework |
| 29 | 1D | LastDest_List | array | S/H | last-destination menu | stock |
| 30 | 1E | FavoriteDest_List | array | S/H | favorites menu | stock |
| 31 | 1F | PreferredDest_List | property | None | preferred source list | not advertised |
| 32 | 20 | NavBook | array | None | address book | not advertised |
| 33 | 21 | Address_List | array | S/H | address/home | stock |
| 34 | 22 | RG_ActDeact | method | S/H | start/stop-guidance result | stock command |
| 35 | 23 | RepeatLastNavAnnouncement | method | None | repeat result | not advertised |
| 36 | 24 | VoiceGuidance | property | S/H | voice mode | stock |
| 37 | 25 | FunctionSynchronisation | property | All | atomic RG sync (17/18/23/49) | framework (implicit) |
| 38 | 26 | InfoStates | property | All | no-GPS / init-map / etc. | stock (high-risk global) |
| 39 | 27 | ActiveRGType | property | S/H | guidance presentation type | **CarPlay** (sends 0) |
| 40 | 28 | TrafficBlock_Indication | property | S/H/MH | traffic-block icon | stock |
| 41 | 29 | GetNextListPos | method | S/H | list paging | framework |
| 42 | 2A | NB_Speller | method | None | address-book input | not advertised |
| 43 | 2B | MapColorAndType | property | H | stock map colour/type | stock |
| 44 | 2C | MapViewAndOrientation | property | H | stock map view/orient | stock (high-risk map lifecycle) |
| 45 | 2D | MapScale | property | H | native scale + SW SetGet ACK | **stock passthrough** (see [[bap-fctids]]) |
| 46 | 2E | DestinationInfo | property | All | destination detail | **CarPlay** (gated) |
| 47 | 2F | Altitude | property | H/M/MH (!China) | native lower-bar altitude | **stock passthrough** |
| 48 | 30 | OnlineNavigationState | property | H/M/MH | online-map status | stock |
| 49 | 31 | Exitview | property | S/H/MH | junction/exit-view + FctSync | **CarPlay** (toggled to sync) |
| 50 | 32 | SemidynamicRouteGuidance | property | All | traffic delay / alt route | stock |
| 51 | 33 | POI_Search | method | None | POI search result | not advertised |
| 52 | 34 | POI_List | array | None | POI list | stock no-op stub |
| 53 | 35 | FSG_Setup | property | S/H | voice/POI capabilities | stock |
| 54 | 36 | Map_Presentation | property | H | map size + side-menu states | stock (echo can't close a VC drawer) |
| 55 | 37 | ManeuverState | property | S/H | maneuver transition state | **CarPlay** |
| 56 | 38 | ETC_Status | property | M/MH | electronic-toll status | stock |
