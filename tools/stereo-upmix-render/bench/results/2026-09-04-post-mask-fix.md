# `ambient-hq` post-mask-fix benchmark

Recorded on 2026-09-04 at 04:55 EDT (08:55 UTC), after the ambient-mask fix,
the stable symmetric-eigenvector branch fix for channel-swap equivariance, and
the final delay-allpass denormal-tail snap. The measured `stereo_upmix.c`
SHA-256 is
`2bd8913d1ed4e18ad423af02e95db5cf0187212f617ecd09f0e82f053e398ae9`.
Both runs process 128 measured calls of 1024 stereo frames after 8 warm-up
calls. Checksums cover the resulting PCM6 as canonical little-endian S16.

## Host wall time

Machine: Apple `Mac15,11`, ARM64/T6031, Darwin 25.5.0.

```text
metric=host_wall_time unit=ns n=128 min=140916 mean=144015 p50=142333 p95=159375 p99=162042 max=175459
checksum_fnv1a64_le=0x2cd85256da3bd0be
```

This is a point-in-time wall-clock measurement on that Mac. Scheduler load,
temperature, power state, compiler changes, and unrelated host activity can
move every timing percentile; it is useful as a same-machine regression
baseline, not as an MHI2Q CPU estimate.

## QNX 6.5 ARMv7 under QEMU `-icount`

Cross-build: QNX GCC 4.9 with the production C code-generation flags. Runner:
single-thread TCG, `-icount shift=0`.

```text
metric=qemu_icount unit=estimated_instructions n=128 min=4033625 mean=4034828 p50=4034820 p95=4035664 p99=4035856 max=4035999
exact_loop_agreement=0.999502
checksum_fnv1a64_le=0x845172f60b9a95bb
```

The exact-loop calibration validates the guest clock as an instruction-count
source for this run. These values estimate retired instructions of the target
ARMv7 binary; they are not QEMU host wall time, Krait cycles, cache behavior,
IPC, or proof of realtime headroom on the vehicle. Only an on-unit measurement
can settle those quantities.

The host and QNX checksums are intentionally treated as per-build regression
values. Floating-point targets are not required to produce bit-identical PCM,
so their differing hashes do not by themselves indicate a DSP mismatch.

## Change from the preceding stable-eigen build

Both output checksums are unchanged by the denormal-tail snap on this normal
level input. Host mean moved from 145621 ns to 144015 ns; that direction and
magnitude are normal wall-time variability and are not attributed to the code
change. The deterministic QEMU instruction estimate did change: mean rose from
3977474 to 4034828 instructions (+57354, +1.442%), and p99 rose from 3978216 to
4035856 (+57640, +1.449%). This measures the added target instructions, not a
corresponding percentage of Krait cycles or wall time.
