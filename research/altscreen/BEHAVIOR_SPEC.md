# altScreen — Cluster Behavior Spec (authoritative)

_Captured 2026-07-28 from requirements. This is the target behavior the Java patch +
renderers must implement.  Keep this in sync; it kept getting lost._

## The two renderers = two DIFFERENT outputs

| render | id | what | where shown |
|---|---|---|---|
| **maneuver_render** | **98** | the **plane** inside the stock **KDK popup** — replaces the stock plane (id 20) with ours during CarPlay nav | KDK popup over ANY tab (music/radio/…), **EXCEPT** the altScreen tab |
| **altscreen_render** | **99** | the **full CarPlay cluster video** | **only** on the altScreen tab |

- The **KDK popup is ONE stock widget**.  The plane it displays is either the stock
  nav-drawn plane (displayable **20**) or **our** plane (**98**).  We choose which by
  switching the cluster context: stock KDK `dc[73]={20,102,101}` vs our KDK
  `dc[80]={98,102,101}` — same popup, different plane source.
- **maneuver_render (98) = cluster visual (KDK plane).  BAP maneuvers = a SEPARATE
  output that goes to the PROJECTION (main HU CarPlay screen).**  Two independent
  sinks of the same route data.

## Behavior matrix

Phone connect → **announce a new tab** (`DSIKombiSync setMenuState smartPhone=AVAILABLE`;
disconnect → `NOT_EXISTING`).  MU1316 never sends this → we add the sender.

| state | cluster (NOT on altScreen tab) | on the altScreen tab |
|---|---|---|
| **no CarPlay nav** | all stock, **do NOT trigger the maneuver widget** | our altScreen (video 99) |
| **stock nav active + altScreen tab opened** | all stock (KDK popup = stock plane 20) | **stock KDK maneuver (20) + bg (102/101) + our stream (99)** |
| **CarPlay nav started** | KDK popup plane = **our 98**; stock nav **fully cancelled** (cancel route guidance) + **block** stock attempts to (re)start RG until CarPlay nav ends/off | our altScreen (video 99) |

## KDK popup rule
The KDK popup always reflects the **current active nav** (stock plane 20 for stock nav,
our plane 98 for CarPlay nav) — **EXCEPT** while viewing the altScreen tab, where the
popup is **suppressed** (the full video 99 is already shown there).

## 🔴 CRITICAL — never cancel the BAP projection
When we cancel stock route guidance (on CarPlay-nav start), we cancel ONLY the stock
navigator's RG.  The maneuvers **we** send over BAP go to the **projection** — those
must **NEVER** be cancelled.  (The old patch could create the KDK popup itself and cancel
the stock navigator; the not-to-touch part is our own BAP projection stream.)

**BAP projection lifetime = navigation lifetime, NOT popup visibility.** Once CarPlay nav
is detected (replace *or* new_tab) we cancel the *stock navigator's* RG and take full
ownership of BAP.  The BAP projection then runs for the **entire** active-nav session.
Hiding/suppressing the KDK popup maneuver (cluster plane 98/20 — e.g. altScreen-tab focus)
does **NOT** stop BAP.  Popup visibility and BAP projection are **orthogonal** axes:
`popup {shown|hidden}` × `BAP {active while nav active}`.  Only actual nav-end (CarPlay-nav
off) stops our BAP sends — never "along with" a popup hide.

## Context mapping (corrected)

| context | displayables | when |
|---|---|---|
| stock KDK (existing) | `dc[73]={20,102,101}` | stock nav → KDK popup shows stock plane |
| our KDK popup plane | `dc[80]={98,102,101}` | CarPlay nav, any tab ≠ altScreen |
| altScreen video (CarPlay nav) | `{99,102,101}` | altScreen tab, CarPlay nav |
| altScreen video + stock KDK | `{99,102,101,20}` | altScreen tab, stock nav active |

NOTE: earlier `dc[82]={98,102,101,99}` (our maneuver OVER video) is **WRONG** — on the
altScreen tab the KDK popup (98) is suppressed; the tab shows video 99 (+ stock KDK 20 if
stock nav).  `dc[80]={98,102,101}` (our KDK plane) is correct.

## Implementation gap → plan

1. **Availability sender** — on activate push `setMenuState(smartPhone=AVAILABLE)` via the
   DSIKombiSync service (we already hold it in `KombiSyncController.addDSIService`); on
   deactivate `NOT_EXISTING`.  (audit #2 gap.)
2. **Context redesign** — align dc[80/81/82] to the real roles above (drop 98-over-99).
3. **KDK popup plane control** — CarPlay nav active ⇒ switch cluster to our KDK `dc[80]`
   (plane 98); on altScreen-tab focus (`KombiSyncController.isSmartphoneTabFocused()`) ⇒
   switch to the video context (99) and **suppress** the KDK popup; else stock KDK.
4. **Stock-nav cancel + RG block** — on CarPlay-nav start: cancel stock route guidance +
   gate stock RG-start (until CarPlay nav ends).  Port the old patch's KDK-popup-create /
   cancel path.  **Do not touch the BAP projection sends.**
5. **State machine** — drive 2/3/4 from (CarPlay-nav-active, altScreen-tab-focused,
   stock-nav-active).  Focus = KombiSyncController; CarPlay-nav-active = RouteGuidance
   state; stock-nav-active = Navigation/RouteManager.
