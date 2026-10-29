# SCENE compute budget

The current stereo backend is SCENE. Its renderer runs on the audio path; the fine mask network normally runs on a persistent worker. If worker startup fails, the hook logs that failure and falls back to inference on the audio thread. MMI UpMix settings select Off, Normal or Intense. `AUDIO_STEREO_UPMIX` is not a runtime switch.

[Audio runtime](STEREO_TO_5_1_UPMIX.md) · [Algorithm](UPMIX_SCENE_ALGORITHM_2026-10-08.md) · [Build and deployment](../build-and-deploy/SCENE_BACKEND_DEPLOY.md)

## Timing units

| Unit | Current value at 48 kHz | Meaning |
| --- | --- | --- |
| Renderer analysis | 1024 samples / 256-sample hop | 21.33 ms window; 187.5 analysis steps/s |
| Mask analysis | 2048 samples / 512-sample hop | 42.67 ms window; 93.75 control rows/s |
| Type-100 QSA period | 768 frames | 16 ms callback deadline |
| Type-100 DSP guard | 12 ms | Leaves time for the stock callback's other work |
| Guard warm-up | 64 periods | Cold periods are counted separately |
| Guard trip | 4 consecutive periods above 12 ms | Skip upmix until a stream gap allows another attempt |
| Guard re-arm | Gap at least 500 ms | Re-arm with a fresh warm-up window |

The window size is not the callback budget. `scene_engine_push()` accepts blocks up to 1024 frames and produces delayed output. The engine delay is 1024 samples; the AirPlay hook advertises the **additional** SCENE latency relative to the 512-sample baseline as 11 ms. See `arrivalToRenderLatencyMs` in [audio_hook.c](../../hook/audio/audio_hook.c).

## Renderer and worker cost

The renderer uses float32 arithmetic, paired real FFTs and no allocation after initialization. One analysis hop uses three complex FFTs: one for the two input channels and two inverse transforms for front subtraction and rear output. Clustering, transfer, protection and overlap-add also belong to the callback cost.

The mask network has 8,131 stored floats after a 20-byte header: 32,544 bytes in `SMSKNET1` format. It analyses 1025 frequency bins and emits four posteriors on a 257-bin grid. File size and instruction count do not measure elapsed time on the HU.

The worker has two job slots and two result slots. The audio path takes the newest completed row for its generation, then submits the next block without waiting. A full mailbox refuses the submission and increments `busy`; the renderer retains the last mask. Generation and sequence checks prevent old-stream results from entering a new stream. The worker requests `SCHED_RR` priority 17; the scheduling call's return is not checked, so the requested priority must not be reported as an observed runtime priority.

A completed row normally takes effect at the next 512-sample control boundary. Worker delay adds mask age, rather than blocking the audio callback. Measure submitted/completed, busy, stale and superseded counts together with callback timing. A low callback mean can coexist with an overloaded mask worker.

Sources: [engine contract](../../hook/audio/scene_engine.h), [mask layout](../../hook/audio/scene_mask.h), [worker implementation](../../hook/audio/scene_mask_worker.c), [deadline policy](../../hook/audio/rt_upmix_guard.h).

## Reproduce an offline measurement

Run from the repository root with the QNX toolchain and local QEMU assets described in [Build System](../build-and-deploy/BUILD_SYSTEM.md):

```sh
bash tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --bench engine --seconds 60
bash tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --bench engine --mask --handoff --seconds 60
bash tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --bench mask --weights /path/to/masknet.bin --seconds 60
```

The engine handoff profile measures renderer/mailbox work; measure the real network separately with `--bench mask`. The harness calibrates guest `ClockCycles` against known instruction loops before accepting an instruction count. QEMU `-icount` does not model cache or HU wall-clock scheduling. An optional `BENCH_QEMU_PLUGIN` adds a cache model, still not a hardware timing measurement.

Record the source revision, toolchain, blob hash, profile and warm-up policy with each result. For target acceptance, measure the complete path under concurrent video/HMI load: callback tail latency, guard trips, worker backlog and QSA underruns. Do not convert instructions to milliseconds using an assumed IPC.

The [historical budget paper](../archive/internal-reference-2026-10-29/audio/UPMIX_COMPUTE_BUDGET.md) retains HSSR timings, CUpGAN bounds and training-machine comparisons. Those figures belong to retired research and are not SCENE performance results.
