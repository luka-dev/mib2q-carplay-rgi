# iAP2 Route Guidance TLVs

*What iOS sends in the iAP2 route-guidance messages, what our C hook parses, what reaches the cluster, how every maneuver type maps to a BAP icon, the angle sentinels, and how the hook starts the subscription without breaking the stock iAP2 link.*

---

## 📋 The messages

| Msg | Name | Direction |
|---|---|---|
| `0x5200` | StartRouteGuidanceUpdates | HU → phone (we inject it) |
| `0x5201` | RouteGuidanceUpdate | phone → HU |
| `0x5202` | RouteGuidanceManeuverUpdate | phone → HU |
| `0x5203` | StopRouteGuidanceUpdates | HU → phone |
| `0x5204` | RouteGuidanceLaneGuidanceInformation | phone → HU |

`[code]` (`hook/routeguidance/rgd_tlv.h:5-9`) The ground truth for field names is accessoryd 23G71 (iOS 26.6) `+[ACCNavigation* keyForType:]`, decompiled. `[decompile]` Where those fields come from on the phone (`CPRouteGuidance` / `CPManeuver`) is in [Third-party Nav Apps](THIRD_PARTY_NAV_APPS.md).

## 📊 RouteGuidanceUpdate (0x5201) coverage

| Type | Apple field | Parsed | Reaches cluster as |
|---|---|---|---|
| 0x01 | RouteGuidanceState | yes | `routeState` |
| 0x02 | ManeuverState | yes | `maneuverState` |
| 0x03 | CurrentRoadName | yes | `currentRoad` |
| 0x04 | DestinationName | yes | `destination` |
| 0x05 | EstimatedTimeOfArrival | yes | `etaSeconds` |
| 0x06 | TimeRemainingToDestination | yes | `timeRemainingSeconds` |
| 0x07 | DistanceRemaining | yes | `distDestM` |
| 0x08 / 0x09 | DistanceRemaining DisplayString / Units | no | ignored on purpose; we format for the cluster locale |
| 0x0A | DistanceRemainingToNextManeuver | yes | `distManeuverM` |
| 0x0B / 0x0C | DistToNextManeuver DisplayString / Units | no | ignored on purpose |
| 0x0D | ManeuverCurrentList | yes | `maneuverOrder` |
| 0x0E | ManeuverCount | yes | `maneuverCount` |
| 0x0F | RouteGuidanceBeingShownInApp | yes | `visibleInApp` (not route-active, see [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md)) |
| 0x10 / 0x11 / 0x12 | LaneGuidance CurrentIndex / TotalCount / Showing | yes | `laneGuidanceIndex/Total/Showing` |
| 0x13 | SourceName | yes, sent on the bus | **dropped by Java** |
| 0x14 | SourceSupportsRouteGuidance | yes | `sourceSupportsRg` |
| 0x15 | DestinationTimeZoneOffsetMinutes | recognised, skipped | no |
| 0x16 | StopType | recognised, skipped | no |
| 0x17 | ChargingStationInfoList (accessoryd) | recognised, skipped | no |
| 0x18 | ArrivalBatteryLevel | recognised, skipped | no |
| 0x19 | DepartureBatteryLevel | recognised, skipped | no |
| 0x1A | FinalWaypointBatteryLevel | recognised, skipped | no |

`[decompile]` for the Apple names, `[code]` for our handling. `rgd_tlv.h` names macros only up to `0x14` (`hook/routeguidance/rgd_tlv.h:62-82`); `rgd_tlv.c` recognises `0x15-0x1A` and skips them silently (`hook/routeguidance/rgd_tlv.c:437-450`). The code comments there label `0x17` "electricVehicleDestination" and `0x18-0x1A` "reserved", which disagrees with the accessoryd enum above. `[open]`

**SourceName** is parsed (`rgd_tlv.c:420-427`) and written to the bus as `source_name` (`hook/routeguidance/rgd_hook.c:1047-1048`). `RouteGuidance.State` has no field for it and never reads the key. Surfacing it is Java-only work. `[code]`

## 📊 ManeuverUpdate (0x5202) and lanes (0x5204)

ManeuverUpdate is fully covered: our `MAN_TLV_*` match Apple `ACCNav_MIUpdate_*` one for one (`rgd_tlv.h:107-120`). `[code]`, `[decompile]`

| Type | Field |
|---|---|
| 0x01 | Index |
| 0x02 | ManeuverDescription |
| 0x03 | ManeuverType |
| 0x04 | AfterManeuverRoadName |
| 0x05 / 0x06 / 0x07 | DistanceBetweenManeuver / DisplayString / DisplayUnits |
| 0x08 | DrivingSide |
| 0x09 | JunctionType |
| 0x0A | JunctionElementAngle |
| 0x0B | JunctionElementExitAngle |
| 0x0C | LinkedLaneGuidanceInfo |
| 0x0D | ExitInfo |

The per-maneuver arrays are parsed for up to `MANEUVER_CACHE_SIZE = 32` maneuvers; BAP and the HUD show only the current one. Lane arrow geometry is not missed: per-lane positions, directions (angles) and status reach Java as independent lane events (`lgN_*`, `RouteGuidance.State.lgLane*`), see [Lane Guidance](LANE_GUIDANCE.md). The lane instruction text (`0x5204` / `0x03`, `lane_guidance_description`, for example "Use the 2 left lanes") is parsed and sent as `lg%u_lane_desc`, but Java does not read it, so it never reaches the cluster. `[code]` (`rgd_hook.c:589`)

## 🔍 Not handled at all

**RoadObjectDetection** (`ACCNavigationRoadObjectDetectionInfo`) is an accessory sensor-input channel. Correction, 2026-09-30: iPhone sends start `0x0D00` and stop `0x0D02`; the car sends update `0x0D01` into iPhone navigation providers. Update fields: `0x00` component ID, `0x01` Timestamp, `0x02` EgoSpeed, `0x03` EgoYawRate, `0x04` RoadSign, `0x05` RoadLane, `0x06` RoadObject. RoadSign has ID, State, Type, Value, LateralOffset, ForwardOffset; these are accessory detections, not exported Maps limits. The earlier inference that the phone returns detections was wrong. Both 26.6 and 27 incoming-message tables and handler/server call chains confirm the direction. See [CarPlay Speed Limits](CARPLAY_SPEED_LIMIT.md) for addresses and artifacts. `[decompile]`

The [Road Object Detection research](IAP2_ROAD_OBJECT_DETECTION.md) also finds explicit rejection of advertised `0x0D00–0x0D02` in both builds' ordinary Navigation identification path. Start categories `1–3` are distinct from sign/object subtype `Type` bytes; their semantic subtype enums remain unresolved. `[decompile]` / `[open]`

`ACCNavigationAccessoryInfo`, `ACCNavigationProviderInfo` and `ACCNavigationUpdateInfo` also exist in accessoryd but were not decompiled. They look like internal plumbing. `[open]`

Opportunities, ranked by value against effort:

1. **SourceName (0x13):** show which nav app is guiding. Java field only.
2. **EV fields (0x17, 0x18, 0x1A):** charging stops and battery at arrival. Only worth it with a cluster widget to drive.
3. **StopType (0x16), destination time zone (0x15):** minor. We render the ETA as an absolute clock in the vehicle's current zone (`convertUtcToLocalMs` = HU UTCOffset DSI, DST-aware). On a cross-zone trip our clock and the iPhone's (destination zone) differ until the car crosses the boundary. 0x15 would close that edge case.
4. **Speed-limit display:** Maps cluster URLs accept `showSpeedLimit=yes` on both 26.6 and 27; our hook currently requests `no`. Test the rendered sign on-car. Numeric extraction through RGI is not established, and RoadObjectDetection has the opposite direction. [Evidence](CARPLAY_SPEED_LIMIT.md). `[decompile]` / `[open]`

## 📊 Every maneuver type maps to something

`convertManeuverTypeForFunc` is a bounds-checked pass-through (valid 0..53, ≥ 54 → `0xFF`), so wire values equal our `MT_*` enum. As of 2026-08-28 every value maps in `ManeuverMapper.map()`. `[code]` (`java_patch/com/luka/carplay/rgd/ManeuverMapper.java:155-163`, `:538`)

| Type | → BAP main / direction |
|---|---|
| 0 NO_TURN, 5 FOLLOW_ROAD | FOLLOW_STREET |
| 1 LEFT_TURN, 2 RIGHT_TURN, 3 STRAIGHT | TURN + L / R / straight |
| 4 U_TURN, 18 START_ROUTE_WITH_U_TURN, 26 U_TURN_WHEN_POSSIBLE | UTURN |
| 6 ENTER_ROUNDABOUT | TURN + L/R (no roundabout approach glyph) |
| 7 EXIT_ROUNDABOUT | EXIT_ROUNDABOUT_TRS (junctionType 1 only) |
| 8 OFF_RAMP | EXIT_L/R + angle-derived direction |
| 9 ON_RAMP | TURN + slight |
| 10, 12, 27 ARRIVE_* | ARRIVED |
| 11 START_ROUTE, 16 EXIT_FERRY, 51 CHANGE_HIGHWAY | TURN + angle |
| 13 KEEP_LEFT, 49 SLIGHT_LEFT | TURN + SLIGHT_LEFT |
| 14 KEEP_RIGHT, 50 SLIGHT_RIGHT | TURN + SLIGHT_RIGHT |
| 15 ENTER_FERRY, 17 CHANGE_FERRY | FOLLOW_STREET |
| 19 U_TURN_AT_ROUNDABOUT | ROUNDABOUT_TRS + UTURN |
| 20 / 21 LEFT/RIGHT_TURN_AT_END | TURN + end-of-road angle |
| 22 / 23 HWY_OFF_RAMP_LEFT/RIGHT | EXIT_L/R + angle-derived direction |
| 24, 25 ARRIVE_DESTINATION_L/R | ARRIVED + L/R |
| 28-46 ROUNDABOUT_EXIT_1..19 | ROUNDABOUT_TRS + angle16 (junctionType 1 only) |
| 47 SHARP_LEFT, 48 SHARP_RIGHT | TURN + SHARP |
| 52 / 53 CHANGE_HIGHWAY_LEFT/RIGHT | CHANGE_LANE + L/R |
| 255 NOT_SET | NO_SYMBOL |

No type is dropped outright. The only conditional NO_INFO is the roundabout family (7, 28-46) when `junctionType != 1`, and that is sound: `convertJunctionTypeForFunc` shows iOS emits only 0 (intersection) and 1 (roundabout). The simplifications (ENTER_ROUNDABOUT → TURN, CHANGE_HIGHWAY → CHANGE_LANE, ferries → FOLLOW_STREET/TURN) are deliberate. This table is the 2026-08-28 state; the current icon selection is documented in [CARPLAY_HUD_ICON_SELECTION](MANEUVER_MAPPING.md) and [HUD Renderer](HUD_RENDERER.md). `[decompile]`, `[code]`

## ⚠️ Angle sentinels

An angle reaching `ManeuverMapper` can mean "absent" for different reasons, and they are not interchangeable. `[code]`

| Value | Origin | Meaning |
|---|---|---|
| `0` | wire | a real straight angle |
| `+1000` | C parser | exit angle omitted for an indexed maneuver; `rgd_tlv.c:618` sets it and the present bit, so it is published explicitly |
| `-1000` | mapper | defensive case only; the hook never emits it |
| `-1` | Java | legacy default of an unfilled `RouteGuidance.State` slot; also a legal real −1° on the wire |

The 2026-08-30 fix (ported from `Patches/mib2q-carplay-rgi-old`) stopped an unfilled slot rendering as a left turn: `directionFromTurnAngle(-1)` used to take the signed path and give DIR_LEFT, and `rampDirection()` used `dirFromEndOfRoadAngleLeft(-1)`. The 2026-09-11 correction then separated a real −1° from the slot default. **Current code** resolves `-1` by presence, not by value: `RouteGuidance.State.mTurnAnglePresent[]` records a received angle, `map(..., present)` is the presence-aware entry, and `hasDirectionalTurnAngle()` tests only `0`, `+1000` and `-1000` (`ManeuverMapper.java:155-163`, `:385-390`; `java_patch/com/luka/carplay/rgd/BAPBridge.java:2452-2455`; `RouteGuidance.java:167`, `:1191-1193`). The old rule "treat −1 as no angle everywhere" is superseded. `mExitAngle` resets to 1000 while `mTurnAngle` resets to −1, so the two fields do not share a default. `tests/RendererMapperDirectionTest.java` covers the fallbacks but is wired into no build script. `[code]` Verification against iOS 23B85 / 24A5418b is in [IOS_MANEUVER_ANGLE_SENTINELS](MANEUVER_MAPPING.md).

## 🔧 Starting the subscription without breaking the link

The hook sends `0x5200` itself. The first attempt at doing that safely failed. `[car]`

**Observed failure:** one `dio_manager` completed Identify and authentication, then sent `0x5200` nine times over more than two minutes without a first `0x5201`. RTSP streams 110/111 kept running, but RGI and later media metadata stalled. A physical reconnect (new `dio_manager` link) got `0x5201` about 1.19 s after `0x5200`.

**Failed experiment (commit `687596f`):** replace the next outgoing LocationInfo (`0xFFFB`) Encode result with `0x5200`, to avoid copying FF5A sequence state. On the unit, iOS commonly answered `STOP_LOCATION` and AirPlay never reached control SETUP across repeated generations. LocationInfo is not an expendable carrier at startup. Known-good logs keep LocationInfo, inject `0x5200` as an extra packet, and reach control SETUP about 1 s after authentication.

**Fix:** keep every stock message and submit `0x5200` through the stock Cinemo object, which builds a fresh FF5A packet and owns sequence, ack and checksums. `[code]`

- The hook interposes the dio → libNmeSDK factory `CinemoCreateIAP`, keeps the returned `ICinemoIAP` with `AddRef`, and logs `Captured stock ICinemoIAP=%p via CinemoCreateIAP` (`hook/framework/hook_framework.c:826-867`).
- It captures only the link-session byte from a successfully sent stock FF5A packet.
- After stock LocationInfo has gone through `NmeTransport::Send`, the RGI trigger queues `0x5200`, and `ICinemoIAP_SendIAP2(iap, session, frame, len)` sends it, logging `Injected semantic frame via stock SendIAP2 session=%u len=%u` (`hook_framework.c:225`, `:325`; trigger `hook/routeguidance/rgd_hook.c:1493-1500`).
- **Newer than the source write-up:** the send runs on a single process-lifetime worker thread with a frame queue, never on NmeTransport's thread, the 1 Hz bus timer or a destructor. A stuck synchronous `SendIAP2` used to make `bus_shutdown()` join a non-returning thread and SI reported `TIMEOUT_WATCHDOG`. Shutdown waits only a bounded time (`hook_framework.c:91-100`, `:273-285`).
- Incoming Identify start (`0x1D00`) clears the captured transport state and per-session RGI retry state, so a fast reconnect inside a surviving `dio_manager` cannot inherit `got_520x` or a pending request (`hook_framework.c:451-467`, `rgd_hook.c:1619-1621`).
- `0x5200` is retried: every 2 s for 5 attempts, then every 30 s, until the first real RGI message arrives. A `0x5200` sent while the phone restores an existing Maps route can be accepted without the initial snapshot (`rgd_hook.c:35-43`). The request asks for `source_name + source_supports_rg + exit_info` (`:1284`).

Stock symbols (MU1316 NME image and database): `[decompile]`

| Symbol | Address |
|---|---|
| `NmeIAP2Message::Create` / `AddParam` / `Encode` | `0x11FD78` / `0x11FDE8` / `0x11FFE8` |
| `NmeTransport::Send` | `0x186E60` |
| dio PLT call to `CinemoCreateIAP` | `dio_manager:0x17D330` (out block at `wrapper+0x9C`, `ICinemoIAP*` at `+0x00`) |
| `CinemoCreateIAP` | `libNmeSDK.so:0x5848C` |
| `ICinemoIAP_AddRef` / `Release` / `SendIAP2` | `libNmeSDK.so:0x9B60C` / `0x9B620` / `0x9B748` |
| concrete `SendIAP2` vtable target | `libNmeSDK.so:0x58914` |
| `IAPPassthru::SendIAP2` (explicit SESSION and SIZE) | `libNmeVfs.so:0x49D60`; receive dispatch `0x60554`, vtable forwarder `0x64800` |

`Encode` produces the `0x4040` session message in `NmeArray<unsigned char>`; the stock path wraps it in FF5A. The Identify patch uses the same Encode boundary.

### Frame layout the hook parses

Two layers, both big-endian. `[code]`

| Layer | Bytes | Fields | Where |
|---|---|---|---|
| Link packet | `FF 5A` + 7 | `len` BE16 at +2, `ctrl` +4, `seq` +5, `ack` +6, `session` +7, header checksum +8 | `hook/framework/iap2_protocol.c:241-254` |
| Session message (control session) | `40 40` + 4 | `len` BE16 at +2 (whole message, minimum 6), `msgid` BE16 at +4, parameters from +6 | `iap2_protocol.c:13-30` |
| Parameter (TLV) | 4 + value | `len` BE16 (includes the 4-byte header), `id` BE16, value from +4 | `iap2_protocol.c:50-57`, `:88-104` |

The link checksum is the negated 8-bit sum of the covered bytes (`iap2_cksum_neg`, `iap2_protocol.c:230-239`). The hook never builds a link packet itself: injected messages go through the stock Cinemo object, which owns sequence, ack and checksums (see above).

### Unknown 0x52xx messages

Any `0x52xx` message in either direction that is not one of the five handled IDs (`0x5200`–`0x5204`) logs `Unknown 0x52xx msgid=0x%04X dir=IN|OUT len=%zu` at WARN (`hook/framework/hook_framework.c:348-368`, called at `:893` for incoming and `:972` for outgoing). It shows in a normal build without `carplay_verbose`. The raw bytes follow as `IAP2 0x52xx raw`, but that dump is DEBUG level and only 32 bytes, so it is not visible at WARN or INFO. If iOS adds a route-guidance message, this line is the first place it appears.

On-unit acceptance: both proof lines appear, then control SETUP with LocationInfo intact; first `0x5201` within 1-2 s; song metadata and cover art keep changing while RGI runs; a fast replug gives a new Identify-start reset and a new successful `0x5200`; no `Injected frame with prefix` or `Semantic iAP2 send: replaced carrier` line. Whether all five were observed on the current build is `[open]`.

## 🔗 Related

- [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md): what Java does with these fields.
- [Third-party Nav Apps](THIRD_PARTY_NAV_APPS.md): which apps fill which fields.
- [Hook to Java Bus](../reference/HOOK_TO_JAVA_BUS.md): the key/value framing of `source_name`, `route_generation` and the slot keys.

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

- `IAP2_RG_TLV_COVERAGE`: accessoryd TLV enums vs our parser, maneuver type coverage, angle sentinels.
- `IAP2_RGI_INJECTION_FIX`: the stalled `0x5200`, the failed carrier replacement, the `SendIAP2` fix and its acceptance list.
