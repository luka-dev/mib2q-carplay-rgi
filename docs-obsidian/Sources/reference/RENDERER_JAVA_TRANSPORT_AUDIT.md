# Java maneuver transport cleanup — 2026-09-13

The current renderer packets preserve the accepted HUD/renderer correspondence.
Removing the lane-road experiment does not justify restoring the older independent
CarPlay type table: that table mapped on-ramps to MERGE and off-ramps to EXIT,
producing the S-shaped arrows the user explicitly replaced with slight turns.

## History and current path

Before `b71ecb9`, BAPBridge called `mapIcon`, `mapDirection` and `mapExitAngle`
separately, then sent degree-valued commands. `b71ecb9` made the resolved HUD
maneuver the semantic fallback and retained compatible raw CarPlay geometry.
`3f87f64` restored the continuing forward road for off-ramps. Those changes
precede the lane-road experiment and remain intentional.

Current flow:

`RouteGuidance.State → shared ManeuverMapper → RendererMapper.map → RendererServer → native engine`

The shared mapper chooses the maneuver family and fallback direction. The
renderer adapter converts those codes to wire units and preserves continuous
angles/full circles where allowed. Only the native engine owns scene geometry,
camera motion, progress interpolation and shadows. Independent lane events use
the same active-event selector as HUD but their own transport; they do not enter
this maneuver path.

## Removed

- The side-road round trip through `maneuverSideStreets` and BAP direction bins.
  TURN/APPROACH/ROUNDABOUT immediately replaced those converted arrays with raw
  CarPlay roads; the other native families do not consume side-road arrays.
- Shipping compatibility helpers `mapIcon`, `mapDirection`, `mapExitAngle` and
  their duplicate `legacy` mapping path. Only tests still called them. Tests now
  exercise the same single mapping entry as BAPBridge.
- Three unused `sendBapManeuver` overloads, the test-only `sendBargraph` wrapper
  and an intermediate serializer overload. The active progress sender and the
  degree-valued startup/compatibility sender share one packet encoder.
- The redundant packet-loop bound check after the enforced 18-road limit.
- Stale comments about the removed renderer blink timer, wrong displayable 20
  and the former bargraph presentation. Wire command/flag values are unchanged.

## Retained deliberately

- Validity/presence handling for ±1000 and real versus missing -1-degree data.
- Typed direction winning when a raw angle contradicts it.
- Full roundabouts, controlled snap-to-road and real sharp-turn angles.
- The forward side road on off-ramps and the accepted single-bend ramp shape.
- Version-aware maneuver transitions, refresh-only updates, enqueue retry,
  explicit HUD blink phases and renderer readiness/reconnect handling.
- Independent raw lane reception for a future panel below the maneuver.

These are active semantic/transport requirements, not remnants of lane-road
geometry. Deleting them would change behavior.

## Verification

Before/after execution of `ManeuverChainAudit` against the actual BAPBridge and
stock BAP serializer produced **7,890 identical HUD packets and 7,890 identical
48-byte renderer packets**. Icon, angle, driving side, side roads, flags and
progress bytes are unchanged in every tested row.

`test_route_info.sh` passes 1,088 maneuver checks plus presence/sentinel,
viewport, distance/blink and independent lane-transport regressions. Additional
checks ensure irrelevant road changes do not refresh U-turn/lane-change scenes,
while a new version of an otherwise identical maneuver still transitions.
`test_java_transports.sh` passes queue/CLEAR ordering, fragmented transport,
stale writers and 30 restart cycles. The production Java 1.4 JAR builds.

Local audit artifacts: `build/java-renderer-cleanup/{before,after}.tsv`,
`comparison.json`, `cleanup.patch` and test logs. The native renderer was not
changed during this Java cleanup; no new vehicle deployment was performed.
