# TODO

## Open

### Wife's phone — CarPlay connect restart-loop (needs slog to confirm)
Reported 2026-08-18. Logs: `New Folder With Items 2` (build `g67b96d5`).

- **Symptom:** owner's phone connects first try; wife's phone → ~12+ back-to-back
  `dio_manager` restarts over ~3 min before one session succeeds, then works
  ~8 min, drops, and loops again.
- **Not altScreen:** when it connects, the pipeline is fine —
  `control SETUP → 111 → receiver → VIDEO_ON` (twice), clean decode + reconnects.
- **Actual failure:** connection/pairing layer. Failing cycles: `dio` starts,
  hook loads, `/info` served, but the phone never reaches `control SETUP`
  (no RTSP) → SI kills `dio` at ~15 s (TIMEOUT_STARTUP) → restart.
- **USB reset recovery didn't fire:** `reset port 3 250 1` targets "USB interface
  never up"; here the interface *was* up, phone just didn't start/kept dropping RTSP.
- **Most likely (phone-side):** flaky/non-MFi or loose cable (works→drops→re-enum
  pattern), and/or first-time CarPlay pairing (unlock + confirm prompt, "Allow
  CarPlay While Locked", car present in Settings→General→CarPlay).

**Next step:** capture `sloginfo`/SI log from a looping run and confirm the kill
reason — SI `TIMEOUT_STARTUP` (dio never got the phone) vs phone-initiated
`disconnect`. That decides: cable/pairing (phone-side) vs give the phone more
time per attempt (SI `maxSetupRetries` / startup timeout — stock config, risky).
Then retest with a known-good MFi cable and a freshly-paired phone.

## Other open (tracked elsewhere)
- On-unit lifecycle validation (5 fault-replay tests + drive) — `docs/reference/CARPLAY_SESSION_LIFECYCLE_AUDIT.md`.
- Java cleanup groups 1–3 + KOMO reflection (group B) — `docs/reference/JAVA_PRODUCTION_CLEANUP_AUDIT.md`.
- `routeState==5` accept-all-maneuvers logic not implemented (MHI3 has it) — decide if needed.
