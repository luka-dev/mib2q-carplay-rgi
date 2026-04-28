# MIB3 (MHI3) RouteGuidance / TBT Parity Notes

Goal: match **MHI3 (MIB3)** behavior for CarPlay TBT parsing + processing + BAP emission
using our `c_hook` + `java_patch` implementation.

This document captures **verified** findings (IDA reverse-engineering evidence) and the current patch state.

## Reference Firmware

Reference binaries from MHI3 firmware `MHI3_ER_AU_P4364_8W0906961DR`:

| Binary | Role |
|--------|------|
| `dio_manager` | CarPlay nav metadata (iAP2 -> internal structs) |
| `tbt_renderer` | Maneuver mapping, formatting, RSI/cluster objects |
| `DSINavBap` | BAP emission (ManeuverDescriptor, ExitView, distance) |

## High-Level Pipeline (MHI3)

Conceptually:

1. `dio_manager` receives CarPlay nav metadata (ASI / CarPlay stack).
2. `tbt_renderer` maps maneuvers, computes bargraph, filters TurnToInfo signpost, etc.
3. `DSINavBap` forwards:
   - BAP maneuver descriptors (`mainElement`, `direction`, `zLevelGuidance`, `sideStreets`)
   - plus a separate `exitViewId` (junction/exit view selection)
   - plus TurnToInfo / DistanceToNextManeuver, etc.

Our patch repo is not running `tbt_renderer`; we re-implement the minimum required mapping
in Java and forward to the Combi BAP Navi API.

## Verified IDA Evidence (MHI3)

### ExitInfo is a string in MHI3 `dio_manager`

`dio_manager`:
- `CRouteGuidanceUpdateProcessorImpl::getRouteGuidanceManeuverInformation` @ `0x308ea0`
  - if presence flag set, it creates `util::UnicodeString8` from a raw pointer and assigns it
    to the maneuver info object (`exitInfo` field).
- Maneuver-toString includes `exitInfo='%s'`:
  - fmt string @ `0x5126a8`
  - used by `CDIONavigationMetadataTypeInfo::toString(SRouteGuidanceManeuverInformation)` @ `0x299c58`

Conclusion:
- iAP2 `MAN_TLV_EXIT_INFO` should be treated as a string-like payload, not a packed struct.

### Bargraph rule (MHI3 `tbt_renderer`)

`tbt_renderer` `sub_2B9C90` (xrefs from log string "Distance to next maneuver..."):

- `denom = distanceBetweenManeuver`
- if `denom > 2999` then denom is treated as `3000`
- `bar = (distanceToNext / denom) * 255`
- if **`bar > 255`** => **bargraphVisible=false** and **bargraph=0**
  (this is **not** saturation)

### TurnToInfo signpost filtering (MHI3 `tbt_renderer`)

`tbt_renderer` `handle_updateTurnToInfo` (`sub_295ED0`):

- input: `street`, `signpost`
- produces a "filtered" signpost by removing occurrences of `street` from `signpost`
  (to avoid duplication)

### Distance Formatting / Encoding (MHI3 vs MHI2Q BAP Semantics)

Important distinction:

- **MHI3 (`tbt_renderer` + `dio_manager` + `DSINavBap`)** receives iAP2 distances as **raw meters** plus **distance strings** and **unit strings**.
- The **BAP value/unit encoding** used by the VC/HUD is a **CombiBAP convention** (not visible in MHI3 `lsd.jar`), and for MHI2Q it is implemented by:
  `de.audi.tghu.navi.app.cluster.BAPDistanceFormatter` (in `mu1316_lsd.jar`).

Verified behavior (MHI2Q `BAPDistanceFormatter`):

- If the raw meters input is `<= 0`, the distance is treated as invalid (handled by AppCombiBAP as `value=-1`).
- Distance `value` sent over BAP is **tenths of the display unit**:
  `value = round(displayDistance * 10.0f)`.
  (The decompiler sometimes shows the float constant as `1092616192`; that is `0x41200000` = `10.0f`.)
- **Quarter-mile special case** (only for `unit=MILES`):
  if the fractional part is a quarter-mile multiple (`25/50/75`), encode as:
  `unit=QUARTER_MILE` and `value=10*(#quarterMiles)`.

Implication for our patch:

- When we call `AppConnectorNavi.updateDistanceToNextManeuver(int value, int unit, ...)`
  and `updateDistanceToDestination(int value, int unit, ...)`, we must pass **tenths-encoded values**
  (and use QUARTER_MILE where applicable), not plain rounded integers.

### DSINavBap sends descriptors + ExitViewId separately

`DSINavBap`:

- `sub_551800` (debug name in string):  
  `"ce_dev::NavBapService::send_maneuver_descriptors(...)"`
  - logs descriptor array and `ExitViewId=%d`
  - forwards call:  
    `listener_updateBapManeuverInformation(descriptors, exit_view_id, VALID)`
  - descriptor fields printed:
    - `mainElement`
    - `direction`
    - `zLevelGuidance`
    - `sideStreets` (byte payload; printed via helper on `v8 + 4`)

- `sub_6E3740` is the state machine driving updates:
  - calls `sub_6E1FF0(..., exitViewId)` which wraps a CIPtrArray and calls `sub_551800`.

- Junction/Exit view IDs are derived from navcore junction view 2D data:
  - `sub_6E42E0` ("on_junction_2d_data") computes IDs and triggers an update.

Conclusion:
- ExitView/JunctionView is fundamentally sourced from navcore assets, not CarPlay iAP2.
  We cannot fully reproduce junction views from CarPlay-only packets.

### DSINavBap: `zLevelGuidance` is always 0, `direction` is an int16 BAP code

`DSINavBap`:

- Descriptor constructor helper: `sub_8ACF30(mainElement, direction_i16, sideStreetsSharedPtr, outSharedPtr)`
  - allocates `0x28` bytes for the descriptor object
  - writes:
    - `*(obj + 0x00) = mainElement` (uint32)
    - `*(obj + 0x04) = (int16)direction` (stored in a 32-bit slot)
    - `*(obj + 0x08) = 0` (**zLevelGuidance hardcoded to 0**)
    - `*(obj + 0x10) = sideStreets` (`util::SharedPtrDefaultBase`)

Implications for our patch:
- We should always pass `zLevelGuidance=0` to the Java `BapManeuverDescriptor` constructor.
- `direction` values are interpreted/printed as hex in logs but are just the BAP direction codes
  we already use (`0/32/64/96/128/160/192/224` plus the 16-step set `0..240 step 16` for roundabout exits).

### DSINavBap: where `direction` comes from (forwarded, sometimes overridden)

`DSINavBap`:

- `sub_8C5210(outDesc, maneuverInfo)` builds a single `BapManeuverDescriptor` from a maneuver-info struct:
  - `mainElement = *(maneuverInfo + 188)` (uint32)
  - `direction = *(int16 *)(maneuverInfo + 184)` (forwarded BAP direction code)
  - `sideStreets` may be computed via `sub_8C4F40(...)` for specific junction-view-capable cases

- `sub_6E3740(navBapSvc, guidanceState, reason)` (state machine) sometimes overrides direction:
  - default: `direction = *(int16 *)(guidanceState + 184)`
  - for a subset of states (gated by `*(guidanceState + 196)` and `*(guidanceState + 192)`),
    direction is forced to one of `{0, 64, 128, 192}` (straight/left/uturn/right).

This confirms DSINavBap does not "format" direction; it forwards (or coarsens) the direction code.

## Direction From Angle (MHI3 `tbt_renderer`)

MHI3 uses a fixed 22.5deg quantization + lookup tables, not ad-hoc rounding.

### Tables

`tbt_renderer` init (`sub_30090`) loads:

- `unk_530F68` (17 floats): exit-angle bins `[-180.0 .. 180.0 step 22.5]`
- `unk_531010` (17 pairs `{float angleBin, int code}`) into `std::map<float,int>`:
  - `-180.0 -> 128`
  - `-157.5 -> 112`
  - `-135.0 -> 96`
  - `-112.5 -> 80`
  - `-90.0 -> 64`
  - `-67.5 -> 48`
  - `-45.0 -> 32`
  - `-22.5 -> 16`
  - `0.0 -> 0`
  - `22.5 -> 240`
  - `45.0 -> 224`
  - `67.5 -> 208`
  - `90.0 -> 192`
  - `112.5 -> 176`
  - `135.0 -> 160`
  - `157.5 -> 144`
  - `180.0 -> 128`

### Quantization Rule (nearest bin)

In `updateCurrentManeuver` (`sub_2B9C90`) for the roundabout-exit maneuvers, MHI3 does:

1. Pick the **closest** bin from the 17-bin float list via `bestMatchingExits`:
   - `sub_2B31A0(exitAngleInt, topN=1, exits=&unk_67BB80)` returns the nearest `angleBin` (float).
2. Lookup `angleBin` in the `std::map<float,int>` built from `unk_531010` and use the mapped `code`.

Implication:
- Behavior is "closest bin" selection in `[-180..180]`, effectively a **clamp + nearest(22.5deg)** mapping.

## Direction Rules (MHI3 `tbt_renderer sub_2B9C90`) Summary

This section is only about the `direction` field of the BAP maneuver descriptor (int16 codes).

- `DIR_STRAIGHT = 0`
- `DIR_SLIGHT_LEFT = 32`
- `DIR_LEFT = 64`
- `DIR_SHARP_LEFT = 96`
- `DIR_UTURN = 128`
- `DIR_SHARP_RIGHT = 160`
- `DIR_RIGHT = 192`
- `DIR_SLIGHT_RIGHT = 224`

Validated behaviors:

- `MANEUVER_TYPE_ENTER_ROUNDABOUT (6)`: `TURN + (LHT? LEFT : RIGHT)`
- `MANEUVER_TYPE_EXIT_ROUNDABOUT (7)`: `EXIT_ROUNDABOUT_TRS_(LHT? LEFT : RIGHT) + (LHT? LEFT : RIGHT)`
- `MANEUVER_TYPE_ROUNDABOUT_UTURN (19)`: `ROUNDABOUT_TRS_(LHT? LEFT : RIGHT) + UTURN`
- `MANEUVER_TYPE_ROUNDABOUT_EXIT_1..19 (28..46)`: `ROUNDABOUT_TRS_(LHT? LEFT : RIGHT) + directionFromAngle16(closestBin(exitAngle))`
- `MANEUVER_TYPE_UTURN (4)`, `MANEUVER_TYPE_UTURN_PROCEED_TO_ROUTE (18)`, `MANEUVER_TYPE_UTURN_WHEN_POSSIBLE (26)`:
  - `UTURN + (LHT? RIGHT : LEFT)` (note: direction flips with driving side)
- `MANEUVER_TYPE_TURN_LEFT_END_OF_ROAD (20)`:
  - default `LEFT`
  - if `exitAngle` is in `[-180..-1]`:
    - `[-67..-1]` => `SLIGHT_LEFT`
    - `[-180..-113]` => `SHARP_LEFT`
    - else => `LEFT`
- `MANEUVER_TYPE_TURN_RIGHT_END_OF_ROAD (21)`:
  - default `RIGHT`
  - if `exitAngle` in `[1..180]`:
    - `[1..67]` => `SLIGHT_RIGHT`
    - `[113..180]` => `SHARP_RIGHT`
    - else => `RIGHT`
- `MANEUVER_TYPE_KEEP_LEFT (13)` and `MANEUVER_TYPE_SLIGHT_LEFT (49)`: `TURN + SLIGHT_LEFT`
- `MANEUVER_TYPE_KEEP_RIGHT (14)` and `MANEUVER_TYPE_SLIGHT_RIGHT (50)`: `TURN + SLIGHT_RIGHT`
- `MANEUVER_TYPE_SHARP_LEFT (47)`: `TURN + SHARP_LEFT`
- `MANEUVER_TYPE_SHARP_RIGHT (48)`: `TURN + SHARP_RIGHT`
- `MANEUVER_TYPE_OFF_RAMP (8)` and `MANEUVER_TYPE_ON_RAMP (9)`:
  - default `TURN + SLIGHT_RIGHT`
  - special-case for LHT with "extreme/negative" angle (`(uint32)exitAngle > 0xB3`): `TURN + SLIGHT_LEFT`
- `MANEUVER_TYPE_CHANGE_HIGHWAY_LEFT (52)`: `CHANGE_LANE + LEFT`
- `MANEUVER_TYPE_CHANGE_HIGHWAY_RIGHT (53)`: `CHANGE_LANE + RIGHT`

## MainElement (Icon) Mapping (MHI3 `tbt_renderer sub_2B9C90`)

MHI3 builds the BAP maneuver descriptor as a `(mainElement, direction)` pair.
In `sub_2B9C90`, `LODWORD(v105)` is `mainElement` and `HIDWORD(v105)` is `direction`.

Validated mainElement mapping for iAP2 `ManeuverType` (0..53):

- `0=NO_TURN`, `5=FOLLOW_ROAD/CONTINUE` -> `FOLLOW_STREET (11)`
- `1=LEFT_TURN`, `2=RIGHT_TURN`, `3=STRAIGHT_AHEAD` -> `TURN (13)`
- `4=UTURN`, `18=UTURN_PROCEED_TO_ROUTE`, `26=UTURN_WHEN_POSSIBLE` -> `UTURN (25)`
- `6=ENTER_ROUNDABOUT` -> `TURN (13)` (direction depends on driving side)
- `7=EXIT_ROUNDABOUT` -> `EXIT_ROUNDABOUT_TRS_(RIGHT/LEFT) (26/27)` (depends on driving side)
- `19=ROUNDABOUT_UTURN` -> `ROUNDABOUT_TRS_(RIGHT/LEFT) (21/22)` (depends on driving side)
- `28..46=ROUNDABOUT_EXIT_1..19` -> `ROUNDABOUT_TRS_(RIGHT/LEFT) (21/22)` (depends on driving side)
- `8=OFF_RAMP`, `9=ON_RAMP`, `22=HIGHWAY_OFF_RAMP_LEFT`, `23=HIGHWAY_OFF_RAMP_RIGHT` -> `TURN (13)`
- `13=KEEP_LEFT`, `14=KEEP_RIGHT`, `47=SHARP_LEFT`, `48=SHARP_RIGHT`, `49=SLIGHT_LEFT`, `50=SLIGHT_RIGHT` -> `TURN (13)`
- `11=START_ROUTE`, `16=EXIT_FERRY`, `51=CHANGE_HIGHWAY` -> `TURN (13)`
- `52=CHANGE_HIGHWAY_LEFT`, `53=CHANGE_HIGHWAY_RIGHT` -> `CHANGE_LANE (12)`
- `10=ARRIVE_END_OF_NAVIGATION`, `12=ARRIVE`, `27=ARRIVE_END_OF_DIRECTIONS` -> `ARRIVED (3)`
- `24=ARRIVE_LEFT`, `25=ARRIVE_RIGHT` -> `ARRIVED (3)` (direction encodes left/right)
- `15=ENTER_FERRY`, `17=CHANGE_FERRY` -> `FERRY (44)`

Note:
- The fallback path in `sub_2B9C90` uses `NO_INFO (1)` when a maneuver cannot be mapped,
  but in our patch we only call the mapper for valid iAP2 types (0..53).

## DSINavBap Direction Override (MHI3 `DSINavBap sub_6E3740`)

Even after a `BapManeuverInfo` contains a mapped `ManeuverDir` (int16), DSINavBap can override it
right before emitting the `BapManeuverDescriptor`.

Where:
- `BapManeuverInfo.Turn` is stored at `+0xC0` (int32) and logged as `TURN_*` in `sub_6E1130`
- `BapManeuverInfo.Action` is stored at `+0xC4` (int32) and logged as `ACTION_*` via `sub_BE4AC0`

Override logic (in `sub_6E3740` case `BapState==2`):

- If `Action` is **NOT** in `{5..8}`:
  - (`5=ACTION_ROUNDABOUT`, `6=ACTION_ENTER_ROUNDABOUT`, `7=ACTION_EXIT_ROUNDABOUT`, `8=ACTION_UTURN`)
  then DSINavBap ignores the pre-mapped `ManeuverDir` and coarsens direction based on `Turn`:
  - `Turn` in `{3..6}` (`TURN_KEEP_RIGHT`, `TURN_LIGHT_RIGHT`, `TURN_QUITE_RIGHT`, `TURN_HEAVY_RIGHT`) => `direction=192` (RIGHT)
  - `Turn` in `{7..10}` (`TURN_KEEP_LEFT`, `TURN_LIGHT_LEFT`, `TURN_QUITE_LEFT`, `TURN_HEAVY_LEFT`) => `direction=64` (LEFT)
  - `Turn==11` (`TURN_RETURN`) => `direction=128` (UTURN)
  - else => `direction=0` (STRAIGHT)

- If `Action` **is** in `{5..8}`, DSINavBap keeps the pre-mapped `ManeuverDir` (this is required for
  roundabout 16-step directions and uturn variants).

Implication for CarPlay patch:
- If we want exact MHI3 parity at the BAP emission stage, we should apply the same coarsening for
  non-roundabout, non-uturn maneuvers (we don't have Carlo Action/Turn, so we approximate based on iAP2 maneuverType).

## Patch Repo Implementation Status

### Native hook (`c_hook`)

Key code:
- `c_hook/routeguidance/rgd_tlv.c`
- `c_hook/routeguidance/rgd_hook.c`

Implemented:
- Decodes iAP2 RG updates + maneuver updates into PPS keys for Java consumption.
- `exit_angle` sentinel behavior: if missing, default to `1000` (mirrors MHI3).
- Publishes both `m{slot}_turn_angle` (legacy) and `m{slot}_exit_angle` (explicit).
- ExitInfo:
  - still publishes debug keys: `m{slot}_exit_info_len`, `m{slot}_exit_info_hex`
  - now also publishes decoded string: `m{slot}_exit_info`
  - decoding preserves UTF-8 bytes (>=0x80), replaces ASCII control bytes with `?`.

Build output: `libcarplay_hook.so`

### Java patch (`java_patch`)

Key code:
- `java_patch/com/luka/carplay/routeguidance/RouteGuidance.java`
- `java_patch/com/luka/carplay/routeguidance/BAPBridge.java`
- `java_patch/com/luka/carplay/routeguidance/SideStreets.java`

Implemented:
- ExitInfo full cycle:
  - Parse PPS key `m{slot}_exit_info` into `State.mExitInfo[slot]`
  - Call `updateTurnToInfo(nextStreet, exitInfo)` (second arg now used)
  - Apply MHI3-style signpost filtering (remove street substring from signpost).

- Bargraph rule updated to match MHI3:
  - if computed `bar > 255` => send `bargraphOn=false` and `bargraph=0`.

- SideStreets encoding fixed:
  - `sideStreets` must be a byte[] payload (one byte per entry), not LE-packed int32.
  - SideStreets content parity (tbt_renderer):
    - Exit-bin tables verified from rodata:
      - `EXITS_FULL_17` @ `0x530F68` (17 floats: `-180..180` step `22.5`)
      - `EXITS_15` @ `0x530FB0` (15 floats: `-157.5..157.5` step `22.5`)
      - `EXITS_7` @ `0x530FF0` (7 floats: `135,90,45,0,-45,-90,-135`)
      - fixed 1-entry patterns @ `0x5310E0`: `192` and `64` (and zeros)
    - Roundabout `occupiedSidestreetsRoundabout` (`sub_2B9030`) rules matched:
      - special-case `exitMatch==+/-180`: uses whole `EXITS_15` as a single `"before"` partition (independent of driving side)
      - normal case: partitions `EXITS_15` around `exitMatch` (prefix + reversed suffix), then swaps before/after by driving side
      - if too many sidestreets before exit => empty
      - if too many after exit => omit after-exit sidestreets
    - Intersection `occupiedSidestreetsIntersection` (`sub_2B97D0`) rules matched:
      - partitions `EXITS_7` around `exitMatch` (prefix + reversed suffix), excludes pivot
      - if too many between entry and exit => empty
    - Emission order parity:
      - native builds a list of mapping entries (`sub_2B3570`), then sorts them by best-match diff (`sub_2B71C0` uses `sub_2B4050/sub_2B38F0`),
        then emits unique direction codes in that sorted order.
      - implemented in `java_patch/com/luka/carplay/routeguidance/SideStreets.java`.

Build output: `carplay_hook.jar`

## What Is Still Missing / Not Fully Proven

### A) Icon (`mainElement`) mapping parity

This is the largest remaining surface:
- Verify our `ManeuverMapper.map(...)` against MHI3 `tbt_renderer sub_2B9C90` mapping
  for all maneuver types / turn angles / driving side combinations we care about.

Update (implemented in our patch):
- **junctionType gating parity** from MHI3 `tbt_renderer sub_2B9C90`:
  - The mapping logic treats `junctionType==0` as "normal intersection" and `junctionType==1` as "roundabout mode".
  - In **roundabout mode**, most maneuver types are suppressed to `NO_INFO (1)` except:
    - `MT_EXIT_ROUNDABOUT (7)` and `MT_ROUNDABOUT_EXIT_1..19 (28..46)` are mapped to the roundabout icons.
    - Some "global" maneuver types are mapped before the junctionType gate (e.g. follow-road, arrived, ferry, change-lane, etc).
  - For `junctionType` values other than `0` or `1`, native returns `NO_INFO (1)`.
- We replicated this ordering/gate in:
  - `java_patch/com/luka/carplay/routeguidance/ManeuverMapper.java`
  - and ensured `sideStreets` is empty when `mainElement==NO_INFO`:
    `java_patch/com/luka/carplay/routeguidance/BAPBridge.java`
  - `EXIT_ROUNDABOUT (7)` sideStreets forced empty to match native:
    `java_patch/com/luka/carplay/routeguidance/SideStreets.java`

### B) SideStreets content parity (not just encoding)

Encoding + core behavior now matched against MHI3 `tbt_renderer` (`sub_2B9030/sub_2B97D0/sub_2B7510/sub_2B3570/sub_2B71C0`).
Remaining risk is primarily "input parity" (do our `junctionAngles/exitAngle/junctionType` inputs match what MHI3 receives for the same real-world maneuver).

Update (implemented in our patch):
- Fixed a subtle input-order assumption in our `splitAnglesNative(...)` helper.
  - Native `tbt_renderer sub_2B7510` partitions junction angles by comparing each angle to `exitAngle`:
    - `> exitAngle` -> "above"
    - `< exitAngle` -> "below"
    - `== exitAngle` -> ignored
    - no dependency on list ordering (it preserves input order within each partition)
  - Our earlier Java code assumed a descending prefix then "past" state, which could diverge if iOS ever sends angles out of order.
  - Updated:
    `java_patch/com/luka/carplay/routeguidance/SideStreets.java`

### C) ExitView/JunctionView

MHI3 uses navcore junction assets to compute `exitViewId`.
From CarPlay iAP2 we do not get those assets.

Current behavior:
- we always call Combi `updateExitView(0,0)` (disabled).

Decision point:
- confirm the cluster accepts "no exit view" without regressions.

### D) RoadNumber in TurnToInfo

MHI3 TurnToInfo uses (street + road number + signpost).
CarPlay iAP2 maneuver payload does not provide a dedicated "road number" field.

Current behavior:
- we approximate signpost using ExitInfo string.

### E) Distance formatting quirks (decimals / thresholds)

We now match:
- bargraph visibility rule
- basic unit conversions + decimal parsing from strings

Still not fully proven:
- whether some units should map to BAP quarter-mile mode (`BAP_QUARTER_MILE`)
- display rounding/threshold policies beyond what we inferred

## Remaining Gaps

1. **mainElement mapping coverage** -- core mapping is implemented in `ManeuverMapper.java` and matches MHI3 for all known types (0..53). Could use more real-world testing for edge cases.

2. **SideStreets input parity** -- encoding and partitioning logic matches MHI3 `tbt_renderer`. Remaining risk is whether iAP2 `junctionAngles`/`exitAngle` inputs match what MHI3 receives for the same real-world scenarios.

3. **ExitView** -- always disabled (`updateExitView(0,0)`). MHI3 sources these from navcore junction assets which CarPlay iAP2 doesn't provide. No regression observed.

4. **Distance formatting** -- quarter-mile BAP encoding (`BAP_QUARTER_MILE`) not yet validated for imperial units.
