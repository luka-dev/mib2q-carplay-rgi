# libairplay altScreen (stream 111) — deploy + open decision

From-source libairplay (`Patches/libairplay-mhi2q`) accepts/serves stream 111 (P1-P5 implemented).
This captures what's needed on-unit to make iOS actually negotiate 111 and where the video goes.

## DECISION: **B** (TCP → altscreen_render) — LOCKED 2026-07-29, no build flag
`screenstream.c` picks the backend **per stream** off the `AltScreen` property (no `SS_SW` flag):
- **main (110)**: OMX-decode + present, in-process (dio's decoder).
- **alt/cluster (111)**: converted to Annex-B (same AVCC→Annex-B path) and **forwarded over loopback
  TCP to altscreen_render (:19831)** → its OMX decode → DM ctx 82/83 → MOST → VC.

Rationale (three independent reasons, all pointing to B over "OMX-direct-to-cluster in dio"):
1. **One OMX decoder per process.** dio already runs ONE OMX decoder for main (110). Decoding 111 in
   the same process would add a SECOND OMX decoder → OMX_Init/Deinit refcounting (SCREENSTREAM_PARITY
   §68: "once per process"), concurrent Qualcomm/Venus HW decode-session contention, and NO crash
   isolation. B keeps each decoder in its own process (dio=110, altscreen_render=111).
2. **Fault isolation** (handle_alt_plan.md:221): a GL/OMX crash in the cluster path must not take
   down dio / main CarPlay. Only B gives this.
3. **Our whole Java stack** (AltScreenModule, AltscreenControlServer, ctx 82/83) is built for B, and
   B reuses the proven MOST/DM route (same as the maneuver popup).

The alternative (OMX-decode 111 in-process, render DIRECTLY to the cluster QNX Screen display) is
NOT implemented — it needs the cluster to be an enumerable, directly-renderable Screen display AND
the VC to accept our window on it, both UNVERIFIED on-unit. Kept only as a possible future
optimization if the multi-OMX-in-dio concerns are ever proven moot.

## Feature advertisement — done by the HOOK, not a config file
iOS only offers a 111 stream if `/info` advertises `extendedFeatures` containing `"altScreen"`
(CFArray → carEndpoint +63 → phone fetches cluster URLs, else -16721 → no stream 111).
**We inject this via the hook / dio patch (`SetProperty`), same as everything else in this project —
NOT via a hand-written `/etc/airplay.conf`.** (Verified: the declaration goes in the dio patch, not
in the lib.) The config-file path (`server->config` ← `configFilePath`) you see in the SDK sources is
`libairplay-mhi2q`'s **stock loader** — that tree is our custom rebuild attempt, not the shipping
lib, so its internal config mechanism is NOT the deploy requirement. Do not derive an `airplay.conf`
task from it.

## Forward target (B) — no env
`screenstream.c` (alt branch) forwards to a **fixed** `127.0.0.1:19831` (the altscreen_render
server). No env, no per-stream port table — the alt/cluster stream's receiver is always the local
altscreen_render process. Nothing to set in framework.json / dio_manager.json.

## Ports (final map)
| link | from | to | note |
|------|------|----|------|
| maneuver control+data | Java (server :19800) | maneuver_render (client) | popup graphics |
| alt VIDEO (H.264) | libairplay alt forward (client, `screenstream.c`) | altscreen_render (server **:19831**) | B path |
| alt CONTROL | altscreen_render (client) | Java AltscreenControlServer (server :19832) | video on/off → setVideoAvailable |

## Remaining
1. ~~Decide A vs B / build flag~~ — DONE: B locked, per-stream in `screenstream.c`, no flag.
2. **Feature advert**: hook / dio patch injects `extendedFeatures=["altScreen"]` via `SetProperty`
   (NOT a config file — see above).
3. **Compile** from-source libairplay on the build VM (last status: UNVERIFIED — VM SSH timed out).
4. **On-unit smoke**: does iOS offer/negotiate 111 given the 2-display /info? Test on Apple Maps
   (cluster scene; avoids -16721).
