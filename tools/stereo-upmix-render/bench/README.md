# Stereo-upmix fixed-block benchmark

This benchmark calls the production `spa_stereo_upmix_process_s16()` with
either the `ambient-hq` or `hsr-td` backend, bass assist enabled, and exactly
1024 input frames per call. It reports:

- `sizeof(struct spa_stereo_upmix)` and its runtime subobject;
- min, mean, p50, p95, p99, and max for 1024-frame calls;
- a deterministic FNV-1a checksum over all PCM6 output, so the optimizer cannot
  discard the work. It is a per-build regression value, not a promise that two
  floating-point targets produce bit-identical PCM;
- clock-read overhead separately from the unadjusted call measurements.

All state, PCM, and statistics arrays have fixed static storage. Initialization
and deterministic input generation are outside the timed regions. There is no
benchmark heap allocation and no render-thread/filesystem work inside a sample.

## Host wall-time run

```sh
tools/stereo-upmix-render/bench/build_host.sh
build/stereo-upmix-bench/host/stereo_upmix_bench --backend hsr-td
```

The host result is wall time in nanoseconds on the current machine. It is useful
for regressions on that same host, but says nothing directly about the MHI2Q's
Cortex-A15/Krait budget.

Use `--blocks N --warmup N` to change the bounded defaults (128 measured, 8
warm-up; maximum 512 each).

## Shipping QNX compiler check

```sh
tools/stereo-upmix-render/bench/build_qnx.sh --variant 4.9
```

The cross-build sources `scripts/apac_flags.sh`, which is also used by the hook
build, and adds the hook's `-fPIC -D__QNX__` flags. Thus `stereo_upmix.c` is
compiled as GNU C99 at `-O2` with the shipping data/function-section settings,
not with a convenient host-only benchmark profile.

## ARMv7/QNX instruction-count run

```sh
tools/stereo-upmix-render/bench/qnx_qemu_bench.sh --variant 8.5 --backend hsr-td
```

This builds a small QNX IFS using the existing
`Tools/qnx-65-sdp-docker/tests/qemu` boot assets, then runs it with single-thread
TCG and `-icount shift=0`. In that mode guest time advances with retired target
instructions. The benchmark calibrates `ClockCycles()` with exact two- and
four-instruction ARM loops and refuses to label the result as instructions when
they disagree by more than two percent.

The QEMU number is target ARMv7 **instruction retirement**, not elapsed host
time, target CPU cycles, cache behavior, or proof of realtime safety on the
Krait. Conversion to target seconds needs a measured target IPC/frequency and
must not be inferred from QEMU wall time. A real on-unit timing run is the final
CPU/headroom measurement.
