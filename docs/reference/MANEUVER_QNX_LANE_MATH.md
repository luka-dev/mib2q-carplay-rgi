# QNX lane-update watchdog, 2026-09-16

On the HU, the first drawable lane event (two lanes) was followed by
`progress watchdog TIMEOUT phase=lane-update`. Java lost the renderer heartbeat,
hid ctx80, and the supervisor restarted the process. The replacement renderer
connected and confirmed a new frame. This was independent of the pending
altScreen Java startup fix; the running HU generation had not restarted.

An isolated CPU-only ARM probe linked against the deployed lane object hung
inside `cr_lane_panel_layout`, before any GL or socket work. A four-second alarm
terminated the probe with exit 90. In the GCC 8.5 / QNX 6.5 build, the SDK
`xtgmath.h` float template selected by `std::fmin(w, 328.f)` recursively calls
itself. At `-O2`, the layout's drawable branch ended in ARM `b .` at offset
`0x114`. Host sanitizer tests could not detect this target-header problem.

Use explicit `::fminf` / `::fmaxf` libm entry points in lane layout, glyphs,
fade, camera framing and scene-bound projection. These preserve float/NaN
semantics without going through the recursive overloads. Do not work around
the fault by extending the watchdog or hiding lanes.

## Why the earlier toolchain fix did not cover this

Toolchain commit `95917883440213f3b96b67cd5795df0f9d16afd4` (2026-08-24)
corrected the libstdc++ crossconfig `HAVE_*F/HAVE_*L` table so the library stopped
defining recursive compatibility `powf`/`sqrtf`/etc. stubs. This fix is present
in the renderer's Docker image (`5d9d97c0944b`, built 2026-08-26): the static and
shared libstdc++ libraries no longer define the affected float stubs. The
renderer itself does not link libstdc++.

The remaining fault is in the consumer header configuration:

- `gcc/port/qnx-os_defines.h` sets `_NO_CPP_INLINES=1`, removing QNX's concrete
  float/long-double math overloads.
- It also sets `__CORRECT_ISO_CPP11_MATH_H_PROTO_FP=1` to avoid duplicate
  classification functions. This flag suppresses GNU cmath's float/long-double
  `fmin`/`fmax` overloads too, although the QNX equivalents were just removed.
- QNX's generic `_TGEN_R2(fmin/fmax)` templates remain enabled. With two float
  arguments the template wins over the double C function and recursively
  selects itself. The compiler folds the call into a self-branch in user code.

A separate four-function object reproduces this with only `<cmath>`:
`std::fmin(float,float)` and `std::fmax(float,float)` contain a self-branch with
no external relocation, while `::fminf(float,float)` and
`std::fmin(double,double)` have normal relocations to libm. Evidence:
`build/diagnostics/qnx-fmin-toolchain-20260916-222845/overloads-arm.txt`.
The toolchain header configuration still needs a general fix plus executable
C++ overload coverage; the renderer's explicit float calls are the local fix.

## Flag provenance

Both changes are attributed by Git to `luka`; those commits do not identify an
assistant/session. Dates below use EDT (UTC-04:00).

- `_NO_CPP_INLINES` is already present in the toolchain's initial commit
  `6950f106` (2026-07-22 04:39:51), inside the imported GCC 4.9 installed header.
  `66677048` (07:49:52 that day) vendors the source port from the prior external
  `qnx-gcc49` project. The original edit predates or coincides with the initial
  snapshot; this repository cannot date it more precisely.
- `__CORRECT_ISO_CPP11_MATH_H_PROTO_FP` and `_INT` first appear in `618d38e1`,
  in `gcc8/port/qnx-os_defines.h` (author time 2026-08-15 04:01:40, commit time
  06:03:32), alongside the GCC 8.5/C++17 port and C99 compatibility gates.
  `2089bd10` (15:19:20 that day) moves this configuration into `gcc/port`;
  ordinary blame of the current file misleadingly stops at that move.
- RetroArch's N64 core separately adds `-D_NO_CPP_INLINES` in `f9139dae`
  (2026-08-09 01:33:36), `cores-src/mupen64plus_next/Makefile:111`. This is later
  than the shared toolchain flag. RetroArch adds PPSSPP in `e1651bc4`
  (2026-08-15 18:52:01) and rebuilds with GCC 8.5 in `6b44a9bf` (21:31:57).
  The timeline is consistent with related bring-up work, but does not prove
  that the toolchain flag was introduced by a RetroArch task/session.

Commit metadata, original headers and N64 blame are saved under
`build/diagnostics/qnx-math-flag-provenance-20260916/`.

Validation:

- `make -C tools/maneuver-preview test`: host scene/geometry/lane/mask tests pass.
- `bash scripts/build_renderers.sh`: production ARM build passes, no emutls or
  C++ runtime dependency.
- `bash scripts/build_maneuver_cpu_test.sh`: builds the existing CPU lane test
  for QNX. Running `build/maneuver_qnx_cpu_test` on the HU passes 729 lane
  combinations, 32 maneuver camera fits, compact/full layout and reset cases.
  It creates no Screen/EGL window, opens no renderer socket, and has a ten-second
  timeout so a regression cannot leave a spinning test process.

Evidence is retained under
`build/diagnostics/maneuver-dead-20260916-220232/`: runtime snapshot, old ARM
objects/disassembly, old probe timeout and corrected native test output.
The updated production binary still needs a visual lane event after the user's
next renderer/HU restart. No production process was restarted for these tests.
