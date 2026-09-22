# Cluster nav side menus: why FctID 54 never blocked them

The cluster's left/right nav drawers (Kartenfarbe, Kartentyp, Kartendarstellung, Autozoom,
Stau umfahren, Zielführung abbrechen) kept opening during CarPlay despite the FctID 54 override in
`AltScreenCombiBAPListener.setMapPresentation`.

## Why the old approach could not work

FctID 54 is the **wrong direction**. `BAPIndicationHandlerNavi.processMapPresentationSetGet`
(stock MU1316) shows the VC sending a **SetGet** — *"I opened the left drawer"* — and the HU
answering with a Status. The drawer is already open by the time the HU is told.

gtf2 confirms the open is VC-local:

- `eEventIdKEY_SideMenu_left_pressed` / `_right_pressed` / `_longPressed` / `_doubleClick` — the
  VC handles the MFW side-menu keys itself;
- `eEventIdINT_OptionMenu_ForceOpen` / `_ForceClose` are **internal** (`INT_`) events with no BAP
  entry point; `openKDKHole`/`closeKDKHole` (`sub_1765BC`/`sub_176780`) only post animation events;
- the master lock `Intern_SideMenusNavigation_Locked` is `Intern_`-prefixed — VC-owned, not
  reachable from the head unit.

So there is no "close the menu" command. Rewriting the FctID 54 echo changes nothing.

## What the VC does honour

The per-entry gates are `Nav_`-prefixed, i.e. fed from the navigation function block:

    Nav_SideMenusNav_MapColors_visible
    Nav_SideMenusNav_MapType_visible
    Nav_SideMenusNav_MapDisplay_visible
    Nav_SideMenusNav_AutoZoom_visible
    Nav_SideMenusNav_StopRouteGuideance_visible

and the HU publishes exactly those as capability bitfields:

| BAP | HU seam | bits |
|---|---|---|
| FctID 43 `MapColorAndType_Status.supportedMapTypes` | `AppConnectorNavi.updateSupportedMapTypes(mainMapSetup, mask)` | destinationMap / position2D / position3D / overviewMap / rangeMap / mainMapSetup |
| FctID 44 `MapViewAndOrientation_Status.supportedMapViews` | `AppConnectorNavi.updateSupportedMapViews(views, supplementary)` | standardMap / googleEarth / trafficMap; 1Box / compass / intersectionZoom |

## Outcome: removed

Both attempts are gone from the tree. Neither worked on the car:

- the FctID 54 override in `AltScreenCombiBAPListener.setMapPresentation` (removed) — wrong
  direction, as shown above, and it lied to stock navigation about the drawer state;
- reporting empty FctID 43/44 capability bitfields from `GatedCombiService` (removed) — the VC
  kept opening the drawers anyway, so the capability bits are not what gates them, or not the
  only thing.

`GatedCombiService.updateSupportedMapTypes/updateSupportedMapViews` are plain pass-throughs again
and stock owns FctID 43/44/54 completely.

## If this is attempted again

Do not retry either of the above; they are measured failures, not untested ideas. What is left:

- `Intern_SideMenusNavigation_Locked` is the VC's own master lock. `Intern_`-prefixed data points
  are written by VC app logic, so reaching it means patching the VC, not the head unit.
- The per-entry `Nav_SideMenusNav_*_visible` gates are real, but the FctID 43/44 capability bits
  are evidently not their only input — find the rest before wiring anything to them.
- The drawer open itself is `eEventIdKEY_SideMenu_left/right_pressed`, handled inside the VC.
  There is no BAP path to it.
