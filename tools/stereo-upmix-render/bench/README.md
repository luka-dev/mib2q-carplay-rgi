# HSSR Universal benchmark

The benchmark calls the same `spa_stereo_upmix_process_s16()` used by the hook
with 1024-frame blocks, LFE assist enabled, and the single Universal backend.
It reports caller-owned state size, host wall time and a rolling checksum.

```sh
tools/stereo-upmix-render/bench/build_host.sh
build/stereo-upmix-bench/host/stereo_upmix_bench \
  --backend hssr-universal \
  --model build/hssr-selection-v11/hssr_model.bin
```

Cross-build with the same ARMv7/QNX flags as the hook:

```sh
tools/stereo-upmix-render/bench/build_qnx.sh --variant 8.5
```

When QEMU assets are installed, the calibrated instruction-retirement run is:

```sh
tools/stereo-upmix-render/bench/qnx_qemu_bench.sh \
  --variant 8.5 --backend hssr-universal \
  --model build/hssr-selection-v11/hssr_model.bin
```

QEMU retired instructions are a deterministic regression metric, not Krait
cycles or proof of the vehicle realtime margin.  The final measured results
are in
[`results/2026-09-07-hssr-selection-v11.md`](results/2026-09-07-hssr-selection-v11.md).
