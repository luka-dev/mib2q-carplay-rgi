# Production logging

Production emits **WARN and ERROR only**, on every side. INFO stays compiled in — it is the
state-transition trace worth having during a diagnostic drive — and is enabled without a rebuild,
and without a unit restart, by one marker file.

    touch /mnt/app/carplay_verbose     # or /tmp/carplay_verbose

Java reads it when `Log` first initialises; the C hook reads it in `log_init()`. Both fall back to
WARN when it is absent. Remove the file to go back to production output.

## What each side does

| component | level | destination | bounded by |
|---|---|---|---|
| Java `Log` | `W` (`I` with the marker) | `/tmp/carplay_java.log` | 512 KB, one `.1` generation |
| C hook | `LOG_LEVEL_WARN` (`INFO` with the marker) | `/tmp/carplay_hook.log` | 1 MB × 3 files |
| `maneuver_render` | stderr | `/tmp/maneuver_render.log` | 512 KB, capped by the supervisor |
| `altscreen_render` | stderr | `/tmp/altscreen_render.log` | 512 KB, capped by the supervisor |
| supervisor | — | `/tmp/carplay_wrapper.log` | 512 KB, capped by the supervisor |

`/tmp` is a ramdisk and the unit stays powered for weeks, so an uncapped append is the one logging
problem here that could actually break something. The Java logger and the hook rotate themselves;
the three plain `>>` redirects did not. `cp_cap_all_logs` in `carplay_processes.sh` truncates them
in place every ~5 minutes of the monitor loop. Truncating a file held open with `O_APPEND` restarts
it at offset 0, so no descriptor is reopened and neither `cp` nor `mv` is needed — neither is
guaranteed present on this firmware. The size comes from `ls -l` field 5 read through positional
parameters, so the helper depends on no external utility at all.

## Removed

- **`traceBap` / `traceDescriptor`** (26 + 2 call sites, two methods, one flag). They were already
  dead at compile time via `BAP_TRACE_ENABLED = false`, but `javac` still built every argument
  string before calling the empty method.
- **All 36 standalone `Log.d` call sites.** Debug spam by definition, and their messages were
  concatenated before the level check rejected them.
- **The raw-key census** in `SteeringWheelInputModule`. It did its job — it proved MFW OK arrives as
  `board=4 code=40` — and the listener already filters to that one key, so nothing else is read.

Five `Log.d` sites were **not** deleted but promoted to `Log.w`: they sat in one-line
`catch (Throwable t) { ... }` blocks. A swallowed exception is exactly what production should
surface, and deleting the line would have left a silent empty catch.

`Log.i` call sites are kept. They are the state-transition record — session start, module ready,
context and geometry decisions — and they are what the marker file turns back on. Removing them
would trade a few string allocations on ~1 Hz paths for the ability to diagnose the car at all.

## Note on `docs/reference/JAVA_PRODUCTION_CLEANUP_AUDIT.md`

That audit's "remove disabled diagnostics" section is now done for `traceBap` and `Log.d`. Its
implicit suggestion to strip logging wholesale is not: the cost it measured is real but lands on
paths that run about once a second, where one `StringBuffer` does not matter.
