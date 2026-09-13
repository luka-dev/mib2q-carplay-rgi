# iOS 26.6 CarPlay maneuver decompile

This note records the authoritative Apple-side source of the accNav maneuver
values consumed by the MHI2 CarPlay route-guidance hook. It was re-derived from
the iPhone18,2 iOS 26.6 (23G71) binaries, not from the MHI3 implementation or a
third-party fork.

## Binaries

- `Maps`, extracted from `/private/var/staged_system_apps/Maps.app/Maps` in the
  23G71 root filesystem: arm64e UUID
  `79CFEB20-5456-34E9-92F8-006F902D7B77`, SHA-256
  `86ede9b3b76f3784883c87deb1bd4ceb5c56fd77c2899a0081d2354f457c8fe4`.
- `CarPlay`, extracted from
  `/System/Library/Frameworks/CarPlay.framework/CarPlay` in the 23G71 dyld
  cache: arm64e UUID `1602A652-319F-3FF9-9879-6D9C6919F901`, SHA-256
  `064993976618cd75b0c6561bfd43aa255a319985de1c27954564ba80f05578ae`.

All addresses below are unslid virtual addresses from those extracted images.

## accNav maneuver enum

`+[CarClusterUpdateManeuverInfo _enumProperties]` initializes the complete
accNav maneuver-name dictionary in block `sub_1010B3424`.

The U-turn family is forced by the binary to be:

| accNav value | Name | Evidence |
|---:|---|---|
| 4 | `U_TURN` | name at `Maps:0x1010b35cc`; NSNumber key object `0x101875448` contains value 4 |
| 18 | `START_ROUTE_WITH_U_TURN` | name at `Maps:0x1010b3700`; key `0x101875598` contains value 18 |
| 19 | `U_TURN_AT_ROUNDABOUT` | name at `Maps:0x1010b3728`; key `0x1018755b0` contains value 19 |
| 26 | `U_TURN_WHEN_POSSIBLE` | name at `Maps:0x1010b37b0`; key `0x101875658` contains value 26 |

This confirms the values already used by `ManeuverMapper`. Type 19 remains a
roundabout maneuver; it should not be folded into the ordinary U-turn renderer.

## GEO-to-accNav conversion

`+[CarClusterUpdate _accNavManeuverTypeForGEOManeuverType:]` at
`Maps:0x100e0b8c8` accepts GEO values 1 through 88 and indexes the 16-bit table
at `Maps:0x101388934`.

Relevant table entries are:

| GEO type | Apple name | accNav output | Table entry |
|---:|---|---:|---|
| 4 | `U_TURN` | 4 | `0x10138893a = 04 00` |
| 25 | `START_ROUTE_WITH_U_TURN` | 18 | `0x101388964 = 12 00` |
| 35 | `U_TURN_WHEN_POSSIBLE` | 26 | `0x101388978 = 1a 00` |
| 86 | `RESUME_ROUTE_WITH_U_TURN` | 4 | `0x1013889de = 04 00` |
| 88 | `TURN_AROUND` | 4 | `0x1013889e2 = 04 00` |

Thus the receiver must support signed direction for accNav 4, 18 and 26. GEO
86 and 88 are intentionally collapsed to accNav 4 by Apple.

## Signed `JunctionElementExitAngle`

Apple has two maneuver-building paths and both preserve a signed 32-bit junction
angle as an `NSNumber`, then publish the element whose role is 2 as
`JunctionElementExitAngle`.

### Route-step path

`+[CarClusterUpdateManeuverInfo maneuverUpdateWithStep:component:]` starts at
`Maps:0x1010b3d9c`.

- Element role 2 is selected at `0x1010b40e0` through `0x1010b40e4`.
- GEO maneuver type 4 is selected at `0x1010b4108` through `0x1010b410c`.
- For driving side 0, a positive angle branches to negation at
  `0x1010b4114` through `0x1010b4128`.
- For driving side 1, a negative angle branches to negation at
  `0x1010b4130` through `0x1010b4148`.
- The actual sign flip is `FNEG D0,D0` at `0x1010b4158`.
- The resulting signed number is written by
  `setJunctionElementExitAngle:` at `0x1010b4180`.

### Guidance-event path

`+[CarClusterUpdateManeuverInfo maneuverUpdateWithGuidanceEvent:routeStep:component:]`
starts at `Maps:0x1010b42a0` and independently repeats the same normalization:

- element role 2 at `0x1010b4494` through `0x1010b4498`;
- GEO type 4 at `0x1010b44a4` through `0x1010b44a8`;
- side/sign comparisons at `0x1010b44b0` through `0x1010b44e4`;
- `FNEG` at `0x1010b44f4`;
- `setJunctionElementExitAngle:` at `0x1010b451c`.

The normalized output is therefore:

| `drivingSide` | Meaning | Ordinary U-turn exit angle |
|---:|---|---|
| 0 | right-hand traffic | negative, hence left U-turn |
| 1 | left-hand traffic | positive, hence right U-turn |

The traffic-side meanings are also independently named by
`+[CPManeuver _descriptionForTrafficSide:]` in the iOS 26.6 CarPlay framework:
`CarPlay:0x23d91c7e8` returns `right` for 0 and `left` for 1.

For GEO 25/35/86/88 Apple does not apply the GEO-4 traffic-side normalization;
it forwards their signed exit angle. Reading the sign is therefore still the
correct primary rule for accNav types 18, 26, and the additional sources that
collapse to type 4. A traffic-side fallback is required when the field is absent.

## No later sign conversion

`+[ACCNavigationInfoBuilder accNavigationManeuverUpdateInfoFrom:]` at
`Maps:0x100da87f0` enumerates the `accNavFormat` dictionary. Its block at
`Maps:0x100da88c8` calls `setInfo:data:` directly. It does not clamp, take the
absolute value, or otherwise alter `JunctionElementExitAngle` after the logic
above.

On the HU side, `rgd_tlv.c` reads TLV 0x000B as a signed big-endian 16-bit value
(`hook/routeguidance/rgd_tlv.c:537-540`) and publishes the same value as both
`turn_angle` and `exit_angle` (`hook/routeguidance/rgd_hook.c:479-483`). The sign
therefore reaches both `ManeuverMapper` and `RendererMapper` intact.

## Sentinel correction

Apple Maps does not synthesize angle 1000 in either 26.6 builder. If no role-2
exit element exists, it simply does not call `setJunctionElementExitAngle:`.
The value 1000 is our MHI3-compatible receiver default, inserted when TLV 0x000B
is absent (`hook/routeguidance/rgd_tlv.c:578-585`). Consequently `0` and
`+/-1000` must be treated as unavailable pipeline values, not as directional
angles.

## Verdict for the current implementation

The current signed U-turn implementation is correct:

- negative angle maps to left;
- positive angle maps to right;
- 0 or `+/-1000` falls back to traffic side;
- the rule applies to accNav 4, 18 and 26 in both the BAP mapper and the external
  maneuver renderer;
- accNav 19 remains on the roundabout path.

The historical `RendererMapper.mapExitAngle()` adapter supplied `-180` for a missing U-turn
angle, but this cannot reverse an LHT U-turn: native `ICON_UTURN` rendering uses
the explicit `direction` first and only falls back to `driving_side`
(`maneuver_render/maneuver.c:1132-1135` and `2182-2185`); it does not use
`exit_angle` for that icon.

No functional change to the signed U-turn mapping is required after the iOS
26.6 audit.

The old renderer helper API was removed during the
[2026-09-13 Java transport cleanup](RENDERER_JAVA_TRANSPORT_AUDIT.md).
Its required semantic fallbacks remain in the single `RendererMapper.map` path.
