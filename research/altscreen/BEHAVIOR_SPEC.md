# altScreen — Cluster Behavior Spec (authoritative)

_Captured 2026-07-28 from requirements. This is the target behavior the Java patch +
renderers must implement.  Keep this in sync; it kept getting lost._

## The two renderers = two DIFFERENT outputs

| render | id | what | where shown |
|---|---|---|---|
| **maneuver_render** | **98** | the CarPlay maneuver plane composed with the KDK backings during CarPlay nav | whenever CarPlay RGI is active |
| **altscreen_render** | **99** | the **full CarPlay cluster video** | throughout the CarPlay cluster session |

- The **KDK popup is ONE stock widget**.  The plane it displays is either the stock
  nav-drawn plane (displayable **20**) or **our** plane (**98**).  Our active composition
  is `dc[80]={98,102,101,99}`: maneuver + KDK backings over stream 111.
- **maneuver_render (98) = cluster visual (KDK plane).  BAP maneuvers = a SEPARATE
  output that goes to the PROJECTION (main HU CarPlay screen).**  Two independent
  sinks of the same route data.

## Behavior matrix

VC tab focus is deliberately not observed and never participates in presentation state.
Only CarPlay RGI activity selects the local composition; the Audi View button independently
selects the iOS fullscreen/duo `viewArea`.

| state | local context | BAP lower bar | iOS viewArea |
|---|---|---|---|
| **no CarPlay RGI** | `ctx81={99}` | inactive | last selected fullscreen/duo |
| **CarPlay RGI active** | `ctx80={98,102,101,99}` | always `arrow + next direction` | fullscreen or duo from View button |

There is no MAP-tab special case and no dynamic FctID44 or NAVSD FunctionList manipulation.

## 🔴 CRITICAL — never cancel the BAP projection
When we cancel stock route guidance (on CarPlay-nav start), we cancel ONLY the stock
navigator's RG.  The maneuvers **we** send over BAP go to the **projection** — those
must **NEVER** be cancelled.  (The old patch could create the KDK popup itself and cancel
the stock navigator; the not-to-touch part is our own BAP projection stream.)

**BAP projection lifetime = navigation lifetime, NOT popup visibility.** Once CarPlay nav
is detected (replace *or* new_tab) we cancel the *stock navigator's* RG and take full
ownership of BAP.  The BAP projection then runs for the **entire** active-nav session.
Only actual nav-end stops our BAP sends. Fullscreen/duo changes never stop RGI.

## Context mapping (corrected)

| context | displayables | when |
|---|---|---|
| stock KDK (existing) | `dc[73]={20,102,101}` | stock nav → KDK popup shows stock plane |
| CarPlay video + maneuver | `dc[80]={98,102,101,99}` | confirmed CarPlay RGI |
| CarPlay video only | `dc[81]={99}` | no confirmed CarPlay RGI |

The former third `dc[82]` is unnecessary. Context selection is driven only by the confirmed
RGI presentation latch.

## Implementation status

1. `AltScreenModule` selects ctx80/81 only from confirmed RGI presentation.
2. Fullscreen/duo only sends `updateViewArea`; it does not alter context or BAP ownership.
3. `BAPBridge` keeps RGI/HUD active for the route lifetime and always publishes FctID19 as
   `Unicode arrow + next direction/road`, with `↑` as the non-empty startup fallback.
4. `GatedCombiService` blocks stock route-guidance writers, leaves initializing/compass/map
   visibility stock, and hides only MapScale + altitude. Every steering-wheel MapScale SetGet
   is still acknowledged with the hidden status; the last stock scale/altitude is restored on release.
5. Both layouts request exactly `maps:/car/instrumentcluster/map`: no Maps
   `maneuverLayout` and no Maps-owned ETA overlay. Arrival time remains available
   independently through NAVSD FctID 22.
