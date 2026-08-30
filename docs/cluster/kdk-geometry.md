---
title: KDK plane geometry (backings 101/102, stages)
tags: [cluster, kdk, geometry, verified]
status: verified-source
sources:
  - code: java_patch/com/luka/carplay/cluster/ClusterLayerController.java
  - code: java_patch/com/luka/carplay/cluster/ClusterGeomOverride.java
  - firmware: LayoutMIB2HighB9 / B9Sport / Q7 (Layout.getIntegerConstant)
  - firmware: VC AU491 gtf2 (SV_LVDS_KDK)
reconciles:
  - docs/reference/CLUSTER_KDK_GEOMETRY.md
  - docs/reference/VC_LAYOUT_STYLE.md
---

# KDK plane geometry (backings 101/102, stages)

Where the maneuver overlay (98) and its KDK backing sit on the cluster, and who decides.

## Context

> [[display-contexts]] - ctx 80 planes -> **kdk-geometry** - position/crop 98 + 101/102.
> Applied by `ClusterLayerController.reapply()` on every context switch / view-size change.

## The VC obeys; the HU dictates

> `status: verified-decompile` - decompiled against AU491 `gtf2` (ELF ARM, image base 0x100000); the
> `FUN_*` addresses below are from this build.

VC-side (AU491 `gtf2`): the KDK surface is placed purely from data-pool variables
`LVDS_KDK_position_x/y/opacity/visible` (resolved by name at model-init - **no code xref** on any of
the four, nor on the `_DM_` twins / `LVDS_KDK_follow`). `openKDKHole` (`FUN_00177be8`) /
`closeKDKHole` (`FUN_00177dac`) carry no geometry - each only appends animation event 15 (open) /
14 (close) as a `uint32` to two message targets (`appendUInt8 1,1` and `1,2`, via CHMISendMessage)
and flips `m_KDKHoleOpen`. `resizeHole` (`FUN_00711490`) is a pass-through:
`INode2D::setTranslation` from its two args, `setScale` from model width/height - nothing
design-dependent. The only hard-coded hole rectangles in this area are Night Vision's
(`makeNvHole` `FUN_001e63b4`: big stage 640x360 centred; S-design(full) x=0, y=+48, 640x400) -
**not KDK**. So the exact KDK rectangle comes over from the HU via
`IDisplayManagerKombiControl.setCropping/setPosition`.

## HU-side authoritative table (MU1316)

`Layout.getIntegerConstant(id)`, chain `LayoutMIB2HighB9Sport -> LayoutMIB2HighB9 -> LayoutMIB2HighQ7`.
Classic overrides **only** 58/59/60/61; everything else falls through to Q7.

| id | meaning | Classic (B9) | Sport (B9Sport) |
|---:|---|---:|---:|
| 58 / 59 | in-tube anchor -> backing **101** | 1055, 207 | 984, 139 |
| 60 / 61 | popup anchor -> backing **102** | 1091, 110 | 1091, 110 |
| 122-125 | in-tube crop src (x,y,w,h) | 59, 27, 210x153 (Q7) | 0, 0, 328x180 |
| 118-121 | popup crop src (x,y,w,h) | 59, 27, 210x153 | 59, 27, 210x153 |
| 108 / 109 | map origin (displayables 33, 58) | 0, 26 | 0, 26 |
| 80 / 81 | small-stage map offset | 0, 0 (Q7) | -476, 0 |

## Stages

- **Backing 101** = sport 328x180 stage; **backing 102** = popup 210x153 stage.
- Stage selection follows the Audi View button (`NAV_VIEW_SIZE_CHOICE`): `popup = !small-stage`.
  `ClusterLayerController` re-applies the crop/anchor for plane 98 and its backing on every switch,
  and the maneuver plane carries the stage's crop so it follows the map.

## (!) Do NOT apply the small-stage offset (80/81) to the KDK panel

Stock adds the `-476,0` Sport singlescreen offset to the **map planes 33/58 only**. The KDK panel and
its backing have no view-size dependency (`positionKDKBackgrounds` / `handleKdkDualTerminal` read no
view size). Moving the panel by -476 in Sport singlescreen was measured on the car to break a view
the stock keeps correct, so `ClusterLayerController` **logs the offset but never applies it** to the
panel.

## Live tuning

`ClusterGeomOverride.poll()` reads an optional `/tmp/cluster_geom.cfg`; a saved file takes effect
within one reconcile tick (no restart). Absent file = pure stock layout.
