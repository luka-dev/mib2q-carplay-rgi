# Cluster KDK geometry: where the maneuver and its backing actually belong

Two bugs were reported: the maneuver coordinates are wrong in Classic layout, and the backing
is displaced in singlescreen. Both come from the same place, and it is not the VC.

## VC side: gtf2 owns no rectangle

Binary: `Firmwares/VC/AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018/extracted/stage2_fs/STAGE2.1/bin/gtf2`
(ARM EABI5, stripped, image base `0x100000`).

- The KDK surface is placed from four data-pool variables — `LVDS_KDK_position_x`,
  `LVDS_KDK_position_y`, `LVDS_KDK_opacity`, `LVDS_KDK_visible` (plus a `_DM_` twin). None of the
  four name strings has a single code xref: they are resolved by name at model-init, so no
  constant in gtf2 feeds them.
- `openKDKHole`/`closeKDKHole` (`sub_1765BC` / `sub_176780`, the pair that brackets
  `isSportDesign@0x176740`) carry no geometry at all. They only post animation events 15 / 14
  to two targets and flip `m_KDKHoleOpen`.
- The one function that does move a hole, `resizeHole` (`sub_6C5BE0`), is a pure pass-through:
  `INode2D::setTranslation(node, (float)x, (float)y)` from its two arguments and
  `INode2D::setScale` from the model's width/height. Nothing design-dependent.
- The only hard-coded rectangles in this area belong to **Night Vision**, not KDK — `makeNvHole`
  (`sub_1E04FC`): big stage `640x360` centred, small stage width-scaled, S-design(full)
  `x=0, y=+48, 640x400`. That last one matches `config/kdgui_2dmgr.json`'s
  `analog_source_rect = {0,0,640,400}`, confirming it is the analog NV input.
- `stage3_fs/data/gem/engdefs/*.esd` mention KDK exactly once — a fade duration. No coordinates.

**Conclusion: the VC obeys. The exact location is dictated by the head unit** and shipped over
MOST via `IDisplayManagerKombiControl.setCropping/setPosition` → `LVDS_KDK_position_x/y`.

## HU side: the authoritative table

Source of truth is `Layout.getIntegerConstant(id)`. The chain is
`LayoutMIB2HighB9Sport` → `LayoutMIB2HighB9` → `LayoutMIB2HighQ7` (`INTEGER_CONSTANTS[132]`).
Resolved values, MU1316:

| id | meaning | **Classic** (`LayoutMIB2HighB9`) | **Sport** (`LayoutMIB2HighB9Sport`) |
|---:|---|---:|---:|
| 58 / 59 | in-tube anchor → backing **101** | **1055, 207** | 984, 139 |
| 60 / 61 | popup anchor → backing **102** | 1091, 110 | 1091, 110 |
| 122-125 | in-tube crop src x,y,w,h | **59, 27, 210x153** ← Q7 fallback | **0, 0, 328x180** |
| 118-121 | popup crop src x,y,w,h | 59, 27, 210x153 | 59, 27, 210x153 |
| 108 / 109 | map origin (displayables 33, 58) | 0, 26 | 0, 26 |
| 80 / 81 | **small-stage map offset** | **0, 0** ← Q7 fallback | **-476, 0** |

Classic overrides only 58/59/60/61. Everything else falls through to Q7.

### Consequence 1 - the stage was never derived, it was a stale cache

The stage (in-tube vs popup) is NOT a skin property. Stock decides it in
`ClusterKDKHandlerImpl.setKDKPositionHints()`:

    isSmallStageActive() -> addHint(8)     "small stage - kdk in tube"
    else                 -> removeHint(8)  "big stage - kdk in flap"

Small stage is the same axis as `NAV_VIEW_SIZE_CHOICE == 1`, i.e. the Audi View button.

`ClusterLayerController` took the stage from that stock KDK hint - but during CarPlay the stock
navigation sends **no KDK model updates at all**, so `apply()` never ran and `reapply()` replayed
whatever `bind()` had defaulted to (`lastPopup = true`). That is the whole bug: fullscreen always
looked right because popup happens to be the fullscreen stage, and every small-stage view was
wrong in both skins.

**Fix:** `popup = !AltScreenModule.isSmallScreenViewArea()`, plus `reapply()` straight from
`setViewAreaMode()` because no stock delta will arrive to do it. Skin-independent - the skins
differ only in the numbers the Layout returns, and those are read live:

| singlescreen (= in-tube) | anchor | crop |
|---|---:|---|
| Sport | 984, 139 | 0,0 328x180 |
| Classic | 1055, 207 | 59,27 210x153 |

Classic does have an in-tube stage; it just inherits the 210x153 crop from `LayoutMIB2HighQ7`
instead of overriding it. `maneuver_render` is built for exactly that rect - its popup crop is
horizontally centred on the 328-wide canvas (59 + 210/2 = 164 = 328/2) and `render.c` shifts the
projection by -0.15 to centre the junction vertically inside it.

### Consequence 2 - the small-stage offset belongs to the map, not the panel

`CombiMapController.positionMap()` adds constants 80/81 when `NAV_VIEW_SIZE_CHOICE == 1`, and it
applies them to displayables **33 and 58 only**. In Sport that is -476 px in X.

Applying that offset to the maneuver and its backing was tried and **measured wrong on the car**:
it tore apart a Sport singlescreen view that stock keeps correct. `positionKDKBackgrounds` and
`handleKdkDualTerminal` read no view size at all - the KDK panel is anchored to the tube, not to
the map. The offset is logged for diagnosis and never applied.

Our CarPlay map is plane 99, a full 1440x542 window (`altscreen_render/screen_egl_present.c`:
`EP_DISPLAYABLE_ID 99`); it is not the stock map window and must not be translated either. The
content shift lives inside the advertised safe area in `hook/altscreen/altscreen_hook.c`.

## Measure it on the unit

`/tmp/carplay_java.log`, one line per distinct decision - every value that can change the picture
is in it, so a config edit always re-logs:

    [CP/I][ClusterLayers] apply ...LayoutMIB2HighB9 style=classic view=single stage=inTube \
        backing=101 src=(59,27 210x153) dst=(1055,207) smallStageOffset=(0,0) [not applied]

## MFW OK (cluster route info / ETA)

Separate bug on the same screen, found while chasing the above.
`SteeringWheelInputModule` required `IMMICombiScreenChangeManager` to start. That interface has
**no implementation anywhere in MU1316** - it belongs to the "MMI inside the cluster" variant - so
the module sat in the retry loop forever (`modules still pending after 10 s`) and the MFW OK key
was never hooked in either view. Both failure paths logged at `Log.d`, which is below the
production level, so a permanently failing start was completely silent.

The service is optional now; without it the map-tab gate falls back to CarPlay owning the cluster
with RGI active, which `onSteeringWheelOkPressed()` re-checks anyway.

Note for the archaeology: stock `KbdListener` handles keycode 40 only in `updateEncoderInternal`
(roller **turn**), and nothing in MU1316 handles a key-40 **press**. That does not mean the press
does not exist - the DSI layer delivers it and stock simply ignores it, which is why the code is
free for us to take. Confirmed on the car by the bounded raw-key census in
`SteeringWheelInputModule.noteRawKey()`:

    raw key board=4 code=40 state=1 / state=0     MFW roller press/release
    MFW OK pressed board=4 mapTab=true
    route-info phase=1 applied view=fullscreen    FctID 19: ETA clock + remaining time

Other codes the census caught on the same keyboard, for future use:
`board=4 code=35/99/100`, `board=1 code=78`.

## FctID 22: the cluster has only an arrival clock

Smallscreen OK used to flip `timeInfoType` between 1 (absolute arrival) and 0 (remaining travel
duration). On the car that did not switch the display, it **erased** it.

The VC's whole distance-to-destination block is five data points:

    NavFPK_DTD_ArrivalTime
    NavFPK_DTD_ArrivalTime_visible
    NavFPK_DTD_Distance
    NavFPK_DTD_Distance_Unit
    NavFPK_DTD_Distance_visible

There is no remaining/duration field anywhere in `gtf2` - no `RemainingTime`, no `Duration`, no
`TTD`. Stock `AppConnectorNavi.updateTimeToDestination` encodes type 0 perfectly well (hour/minute
from seconds, year/month/day flags cleared); the cluster simply has nowhere to put it and drops
`ArrivalTime_visible`.

`publishTimeToDestinationForMode()` now always sends type 1, and MFW OK is a no-op in smallscreen.
Do not re-add the toggle: it cannot work without a VC-side widget.
