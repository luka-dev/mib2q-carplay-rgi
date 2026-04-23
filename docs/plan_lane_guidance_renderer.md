# Lane Guidance Renderer Implementation Plan

## Status: Planned

Prototype was built and tested (arrows render correctly at 328x181), then stripped out pending visual refinement. The BAP lane guidance timing fix is already deployed (narrowed dirty mask, `nowApproach && laneGuidanceShowing==1` gate, strict resolver).

## Context

The maneuver renderer (c_render) displays turn-by-turn navigation on the VC KOMO widget (328x181px). It currently shows maneuver icons, route arrows, and a bargraph -- but no lane guidance. Lane guidance data is available from iOS (via C hook -> PPS -> Java), and already sent to the BAP cluster display (HUD/VC text overlay). This plan adds lane guidance rendering to c_render so the KOMO video widget also shows lane indicators.

## Visual Reference

HUD lane guidance style (from VC cluster):
- Lane arrows centered below maneuver icon
- Active/preferred lane = light blue, inactive = grey/silver
- Dashed vertical separators (3-4 dots) between lanes
- No background -- arrows float on the scene
- Arrow glyphs: stem + curved branch + arrowhead (road-sign style)

## Approach: New `CMD_LANE_GUIDANCE` Command

**Why not extend CMD_MANEUVER**: The 48-byte packet is full (46 payload bytes all used by junction angles + perspective + bargraph). Lane data changes independently from maneuvers (iOS fires `DIRTY_LANE_GUIDANCE` separately). A dedicated command is cleaner.

## Protocol (protocol.h)

Add `CMD_LANE_GUIDANCE = 0x07`. Same 48-byte packet, payload layout:

```
[0]    u8   lane_count (0..8; 0 = clear)
[1]    u8   flags (reserved)

Per lane i (5 bytes each, max 8 lanes -> 40 bytes -> payload[2..41]):
  [2+i*5]     i16  primary_angle (big-endian, degrees; 1000=unknown)
  [4+i*5]     u8   status (0=NOT_GOOD, 1=GOOD, 2=PREFERRED)
  [5+i*5]     u8   extra_angle_count (0..2)
  [6+i*5]     u8   packed: bits[7:4]=extra[0] quantized, bits[3:0]=extra[1] quantized
```

Quantization table (9 entries, 4-bit index): -180, -135, -90, -45, 0, 45, 90, 135, 180

## C State (maneuver.h)

```c
#define MAX_LANE_COUNT    8
#define MAX_EXTRA_ANGLES  2

typedef struct {
    int16_t  primary_angle;    /* degrees, 1000=unknown */
    uint8_t  status;           /* 0/1/2 */
    uint8_t  extra_count;
    int16_t  extra_angles[MAX_EXTRA_ANGLES];
} lane_info_t;

typedef struct {
    int         lane_count;
    lane_info_t lanes[MAX_LANE_COUNT];
} lane_state_t;
```

## Main Loop (main.c)

- Static: `g_lane_state`, `g_lane_alpha` (fade), `g_lane_visible`
- CMD handler: decode payload -> `g_lane_state`, set `g_lane_visible`, mark dirty
- Fade: same pattern as bargraph alpha (`LANE_FADE_SPEED 0.08f`)
- Draw: `render_lane_guidance(&g_lane_state, g_lane_alpha * g_fade_alpha)` after bargraph, before debug grid

## Drawing (render.c)

2D overlay in NDC (identity MVP), same technique as `render_bargraph()`.

**Layout**: Centered below maneuver, within popup viewport bounds (210x153 @ 59,27).

**Per-lane arrow glyph** (road-sign style):
- Vertical stem from baseline
- Curved arc branch at split point for angled directions
- Triangular arrowhead at tip
- Extra arrows rendered as additional branches from the same stem

**Colors by status**:
- PREFERRED (2): light blue `(0.56, 0.82, 0.98)` -- matches HUD active lane
- GOOD (1): grey/silver `(0.76, 0.76, 0.79)`
- NOT_GOOD (0): muted grey `(0.61, 0.61, 0.64)`

**Dashed separators**: 3 small dots stacked vertically between each lane pair.

## Java Side

**RendererClient**: `sendLaneGuidance(count, directions[], statuses[], extraAngles[][])` -- packs raw degree angles (NOT BAP codes), quantizes extras to 4-bit.

**BAPBridge**: `sendRendererLaneGuidance(State s)` -- same resolver and gating as BAP lane path, extracts raw `mLaneDirections`, `mLaneStatus`, `mLaneAngles`. Extra angles = angles that differ from primary direction (dedup within 5 degrees).

## Lane Data Model (from iAP2)

Per-lane fields from iOS via C hook -> PPS:
- `direction` (ANGLE_HIGHLIGHT TLV 0x0003): the highlighted/primary arrow angle
- `angles[]` (ANGLES TLV 0x0002): all arrow directions for the lane
- `status` (STATUS TLV 0x0001): 0=NOT_GOOD, 1=GOOD, 2=PREFERRED

Apple is inconsistent: sometimes `angles[]` includes the highlight angle, sometimes not. The renderer deduplicates by filtering angles within 5 degrees of the primary.

## Files Changed

| File | Change |
|------|--------|
| `c_render/protocol.h` | `CMD_LANE_GUIDANCE`, decode macros |
| `c_render/maneuver.h` | `lane_info_t`, `lane_state_t` |
| `c_render/main.c` | State vars, decode, command handler, fade, draw call |
| `c_render/render.h` | `render_lane_guidance()` declaration |
| `c_render/render.c` | `render_lane_guidance()` + glyph helpers |
| `c_render/test_harness.c` | `L` key binding |
| `java_patch/.../RendererClient.java` | `sendLaneGuidance()`, `quantizeAngle()` |
| `java_patch/.../BAPBridge.java` | `sendRendererLaneGuidance()`, wire into dirty handler |

## Implementation Order

1. Protocol + C state structs
2. main.c command handler + fade logic
3. render.c arrow glyph drawing (iterate with test harness)
4. Test harness L key
5. Java RendererClient
6. BAPBridge integration
7. Visual tuning on 328x181 + real CarPlay data
