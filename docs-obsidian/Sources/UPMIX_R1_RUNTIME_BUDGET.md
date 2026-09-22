# R1 runtime budget: evidence and screening limits

2026-09-15. Sections1–6 recover existing measurements locally; section7 records a
later isolated synthetic native probe. No actual-target benchmark, NAS access,
installation or audio-device execution was performed for this document. R1 is
an offline source-conditioned reconstruction experiment, not a deployed backend.

The old HSSR implementation had useful headroom in an isolated Krait run, but
**there is no established free 75–80% CPU budget**. Under contention its observed
P99 approached the audio period, and later runs exceeded it. A small model is a
reasonable experiment; neither its parameter count nor MAC count admits it to
the car.

## 1. Units and scheduling periods

At 48 kHz, a PCM frame means one simultaneous sample from every channel, not one
STFT frame. The benchmark's 1024-PCM-frame call has a 21.333333-ms period.

| Work item | FFT / hop, PCM frames | Calls per second | Work per 1024 PCM frames |
|---|---:|---:|---:|
| HSSR analysis/synthesis; G9 source model | 512 / 128 | 375 | 8 hops |
| HSSR neural controller | every 4 HSSR hops = 512 PCM frames | 93.75 | 2 controls |
| P1 full-bin model | 1024 / 256 | 187.5 | 4 hops |
| Existing upmix block benchmark | up to 1024 PCM frames | 46.875 at full blocks | 1 call |

An every-hop worker consequently needs sustained throughput at 2.667 ms/hop128
or 5.333 ms/hop256; the old controller period is 10.667 ms. Buffering can move an
individual deadline and add latency, but cannot relax sustained throughput.
The P1 callback's 1024-sample / 21.333-ms alignment delay is **not** measured CPU
time or an additional computational allowance. Source definitions are
[HSSR constants](../../hook/audio/hssr_dsp.h),
[block bound](../../hook/audio/stereo_upmix.h),
[P1 native core](../tools/stereo-upmix-render/p1_native.h) and
[callback contract](UPMIX_P1_NATIVE.md).

## 2. Actual APQ8064/Krait wall time

These GCC 8.5 QNX standalone upmix measurements used 512 calls of 1024 PCM frames.
`CLOCK_MONOTONIC` resolution was approximately 1.007 ms, so individual percentiles
are quantized. This is measured upmix work, not an instrumented complete
decoder → upmix → transport → QSA deadline trace.

| Historical profile | Mean ms | P95 ms | P99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| music-stage | 1.812 | 3.022 | 4.029 | 6.043 |
| HSSR-NEON | 4.283 | 6.043 | 8.057 | 9.065 |
| HSSR-active | 4.491 | 7.050 | 9.065 | 11.079 |
| HSSR-object | 5.022 | 7.050 | 9.065 | 13.093 |
| Object, representative loaded repeat | 6.00 | 12.09 | 21.15 | not reported |

The first pair is recorded in
[HSSR-NEON checkpoint](../../tools/stereo-upmix-render/bench/results/2026-09-05-hssr-neon.md):
HSSR added about 2.47 ms to music-stage in that run. The
[Object checkpoint](../../tools/stereo-upmix-render/bench/results/2026-09-05-hssr-object.md)
explicitly reports later contention exceeding the nominal deadline for both
Active and Object. Its healthy retained production trace is useful historical
evidence, not a worst-case guarantee for new code. Subtracting an old mean from
21.333 ms therefore does not yield a safe second-model allowance.

## 3. Calibrated QEMU instruction estimates

These are dynamic ARM instruction estimates, **not Krait cycles or milliseconds**.
The harness calibrates guest clock ticks using exact two-/four-instruction loops.
QEMU's Cortex-A15 TCG configuration does not reproduce Krait cache, instruction
throughput, memory contention, thermal behavior or scheduling.

| Frozen measurement | Original mean unit | Mean normalized to 1024 PCM frames | Included / excluded |
|---|---:|---:|---|
| V12 legacy | 6,100,282 / block1024 | 6.100282M | Historical upmix path |
| V12 float + SoundCheck + matrix guard | 6,612,581 / block1024 | 6.612581M | Full measured float upmix/safety path, not decode/QSA |
| V13 cabin synchronous | 7,800,664 / block1024 | 7.800664M | Cabin upmix path; maximum was 7,807,540 |
| G9 reliable, fast-tanh, step64 | 4,153,515.35468 / hop128 | 33.228123M | FFT + model + WOLA; no renderer, SoundCheck, rails or callback |
| P1 final default libm | 7,738,214.89097 / hop256 | 30.952860M | FFT + model + WOLA; cabin checked separately, outside timing |
| P1 final experimental fast-SiLU | 4,490,952.32043 / hop256 | 17.963809M | Same timing boundary; optional build, not default approval |

Sources: [V12](UPMIX_CABIN_V12.md),
[V13 exact benchmark](UPMIX_V16_SOURCE_G_PREFLIGHT.md),
[G9 report](../build/upmix-g-control/source-g9-audio-64-fast-a/report.json),
[P1 default report](../build/upmix-p1-control/native-qnx-geometry3e4-default-shared-a/report.json),
[P1 optional report](../build/upmix-p1-control/native-qnx-geometry3e4-fast-shared-a/report.json).
G9 timed 376 hops; each P1 report timed 1098. P1's
[checker](../tools/stereo-upmix-render/p1_native_check.c) times steady-state
`p1_native_push`, not finish, cabin processing or the callback addon.

The important unit trap: **P1's 7.738M is per 256-sample hop, whereas V12's
6.613M is per 1024-sample block**. Their similar-looking numbers do not imply
similar total work. Different timing boundaries also prevent treating the table
as a complete-chain speed ranking or simply adding every row together.

Dense MAC estimates are a third, separate quantity. HSSR's int8 dense controller
has 449,280 MAC/control, or 42.120M MAC/s at 93.75 Hz
([GEMV shapes](../../tools/stereo-upmix-render/bench/hssr_neon_bench.c)). The G9
direct decoder model has 605,936 MAC/hop128 = 227.226M MAC/s
([architecture record](UPMIX_TO_VEHICLE_EXECUTION.md)); P1 baseline has
138.687M MAC/s ([cost record](UPMIX_P1_MICRO_FREQUENCY.md)). These exclude FFT,
features, nonlinearities, memory movement and scheduling. Int8 NEON and float
MACs are not interchangeable timings; parameter count is not a cost model.

## 4. What the existing async split actually owns

The [async checkpoint](../../tools/stereo-upmix-render/bench/results/2026-09-07-hssr-async-refactor.md)
and current code agree: FFT, masks and WOLA stay on the audio thread. Only the
368-feature → 264-control recurrent network uses the persistent worker, with
two-entry SPSC job/result queues and generation/sequence ownership. The
[hook](../../hook/audio/audio_hook.c) requests worker priority 17; the
[worker](../../hook/audio/hssr_control_worker.c) attempts `SCHED_RR` but ignores the
priority-setting return value, so the requested priority is not proof it took
effect. Submission does not wait. In
[DSP control handling](../../hook/audio/hssr_dsp.c), missing work retains the last
control initially; three consecutive missed periods trigger the bounded rear
failsafe. Reset invalidates stale generations.

Historical host calling-thread mean fell from 0.405638 to 0.333291 ms; this is
Apple Silicon evidence, not Krait headroom. The corresponding QEMU sampled
intervals were 5.459845M synchronous versus 5.470226M asynchronous instructions
per block. Pacing is outside the timed interval: worker work occurring there is
not reliably represented as complete process work by the sampled interval.
Measure worker throughput, backlog and calling-thread latency separately.

An every-hop R1 waveform residual cannot simply reuse this control ABI. Adding
it needs an explicit owner, bounded queues/state, reset/reload generations,
sample-aligned delayed-dry fallback and measured worker deadlines. Offloading
does not erase computation or make a second core uncontended. Sharing X/E STFTs
requires identical rate, FFT, hop, window and alignment; otherwise transformations
and buffering must be counted. A second model must not duplicate FFTs implicitly
or apply SoundCheck twice.

## 5. Conservative first-probe scope and promotion gates

For the initial width8 R1 residual experiment, use **at most 15M additional dense
MAC/s** as a proposed engineering screening cap (the current proposal is about
14M). This is a deliberately small experiment scope, **not recovered available
capacity or a deployment threshold**. At 48 kHz it means at most 80,000 added
MAC/hop256 or 40,000/hop128. Longer causal context is allowed only with explicit
state, memory and updated cost accounting; no architecture expansion is justified
by the old mean timing alone.

Before any target promotion:

1. Freeze architecture, preprocessing, sample rate/cadence, exact checkpoint,
   compiler/options and weights. Report dense MACs, all remaining operations,
   state/work bytes and algorithmic latency separately.
2. Qualify numerical parity and failure/reset/chunk handling; keep allocations,
   I/O and blocking locks out of the audio path. Run native per-stage and combined
   instruction checks with the same fixture and boundaries, both baseline and
   candidate. Report incremental work per 1024 PCM frames, not just model time.
3. Include actual extraction E, reconstruction, renderer, one normalization pass,
   linked safety, quantization and queue copies. Historical HSSR profiles used
   LFE assist; new C/LFE-zero policy and offline C1 code are not already benchmarked
   by those runs. Do not sum unrelated mean timings into a P99 guarantee.
4. Obtain paired **actual Krait** full-chain traces under representative concurrent
   audio/navigation/system load: clock resolution, P50/P95/P99/max, overruns,
   worker submitted/completed/busy/stale/deadline misses, callback/ring/QSA gaps
   and device underruns. Require sustained throughput and correct fallback,
   not merely a low average. Select a supported tail margin from these traces;
   no numerical safe extra-millisecond budget has yet been established.

Runtime qualification does not prove source quality, lead-front protection,
bass balance, cabin acoustics or musical acceptance.

## 6. Existing local harnesses; commands for later authorized checks

From repository root, the maintained HSSR benchmark uses production upmix/DSP
sources and [APAC flags](../../scripts/apac_flags.sh). The QEMU wrapper builds first:

```sh
tools/stereo-upmix-render/bench/qnx_qemu_bench.sh \
  --variant 8.5 --backend hssr-universal \
  --model models/hssr-selection-v11/hssr_guard.bin \
  --cabin-render --warmup 32 --blocks 128 --realtime-pacing
```

Repeat with `--async-controller` for the calling-thread/worker split, recording
worker counters as well. The model must exist and match the intended snapshot.
Prerequisites are the installed `qnx65-armv7-toolchain:8.5` Docker image,
`qemu-system-arm`, and `Tools/qnx-65-sdp-docker/tests/qemu` assets. The
[build script](../../tools/stereo-upmix-render/bench/build_qnx.sh) replaces its
dedicated `build/stereo-upmix-bench/qnx-8.5` directory: preserve old evidence and
run from a fresh source snapshot. These commands were not executed for this note.

For the frozen P1 ABI, a separate fresh-directory build is available:

```sh
tools/stereo-upmix-render/p1_native_build_qnx.sh 8.5
python tools/stereo-upmix-render/verify_p1_native_qnx.py \
  --run RUN --data DATA --package PACKAGE --qnx-build QNX_BUILD --output FRESH_OUTPUT
```

The verifier binds checkpoint/data/model/source/binary hashes, checks actual PCM
with host sanitizers and QNX guest execution, and refuses an existing output
directory. `RUN`, `DATA`, `PACKAGE`, `QNX_BUILD` are matching diagnostic artifacts,
not arbitrary models. It does not export R1 or validate new architecture timing.
Use [P1 native documentation](UPMIX_P1_NATIVE.md) for its existing ABI limitations;
the old default-bypass package and its failed musical qualification remain intact.

## 7. Completed frozen144 native frame probe

Completed2026-09-15 at approximately21:23UTC, before any larger-context native
implementation. [Probe source/usage](../tools/stereo-upmix-render/r1_native_probe_README.md)
and [final report](../build/upmix-p1-control/r1-native-probe-c/report.json) are
separate from all production hooks and prior P1 packages. Report SHA256:
`fe93dd04392158daeb5aeee34199e83ccd8d56aa2934c51e4b961773fff26f34`.
The Python model remains the frozen144-parameter source SHA
`0f5b75f94585d23e53d58c6627cd3473f2cc52d5911b098bbad76197e44ee8db`;
weights are deterministic, nonzero and **untrained**.

Host ASan/UBSan and GCC8.5 QNX guest checks passed for both conditioning arms:
64 complex stereo frames, maximum output error1.192093e-7, final history error
at most3.725291e-9, unchanged rtol3e-5/atol3e-7. Tests include silence with prior
context, missing estimate bins, exact reset/replay, bad shape/alias/endpoints,
corrupt and valid-CRC nonfinite weights, zero-head identity, reload rejection
and large-finite-weight arithmetic-overflow rollback. Two exporter tests pass.

| Arm | QEMU mean instructions/hop256 | Maximum/hop256 | Mean normalized/1024 PCM frames |
|---|---:|---:|---:|
| original X + estimate E |1,743,747.667|1,769,782.968|6,974,990.666|
| estimate E only |1,744,040.472|1,770,086.349|6,976,161.887|

Each count covers60 steady-state complete kernel calls after four warm-up frames:
144-weight graph, validation, intermediate finite scans, history and output
copies. It excludes loading/reset, fixture I/O/comparisons, extraction, FFT/WOLA,
renderer, normalization, safety, callback and scheduling. This is a first plain-C
`-O2` probe with strict FP semantics, not an optimal SIMD implementation or a
physical-Krait measurement. Host timings include sanitizers and are not a target
performance estimate. Counts are already of the same order as the historical
complete V13 upmix count, despite the small13.851M-MAC/s graph; this does not
establish a ratio of wall-clock costs across the different paths.

Model storage is600B, state32,864B and work57,456B:90,920B before caller spectra
and output. Each independent source needs its own history and another neural
evaluation. Read-only weights can be shared; scratch can be reused for serial
calls, but concurrent invocations need separate work buffers. N independent
refiners therefore multiply neural work approximately by N; extraction and
E-analysis costs must still be added, while sharing X-analysis is conditional
on an identical STFT contract. No second-source capacity has been approved.

Independent readback verified20 artifact hashes, eight frozen source hashes and
their eight current-file matches. The initial image's missing second invocation
is retained as incomplete `r1-native-probe-a`; the final image uses separate
filenames for the same checker binary. These results remain evidence for this
frozen architecture only: a new context model requires fresh parity/cost work.

## 8. Completed full-bin linear matrix probe

The [linear reconstruction control](UPMIX_R1_LINEAR_CONTROL.md) now has a separate
float32 C frame kernel and [usage/scope note](../tools/stereo-upmix-render/r1_linear_native_probe_README.md).
This is a fixed-matrix primitive, **not** the proposed ML filter predictor,
the context neural model or a complete streaming audio path.
[Final report](../build/upmix-p1-control/r1-linear-native-b/report.json), SHA256:
`9fce6991fccc04f49fb702702d8f984158b612fbe76b7e6253f7e830a182cc7c`.
Frozen C SHA256:
`542a48116e3b622790486a5e44a971055fdf880ec6a6f8e00679862cbc3150eb`.

Host ASan/UBSan and QNX guest checks pass all four fixtures: nonzero synthetic
E-only/X+E matrices and the actual fitted E-only/X+E coefficients, always applied
to deterministic **synthetic spectra**, not music PCM. Each fixture has64 frames,
with60 measured after four warm-ups. Silence, missing E bins, stateless replay,
identity, shape/alias/nonfinite/endpoint rejection, CRC validation and numerical
overflow rollback are covered. Float32 model load is transactional; no allocation,
I/O or lock occurs in the frame kernel. Caller allocations must be correctly
sized and model memory immutable after loading.

| Actual fitted coefficients | QEMU mean instructions/hop256 | Maximum/hop256 | Mean normalized/1024 PCM frames |
| --- | ---: | ---: | ---: |
| E-only | 462365.574 | 488628.120 | 1849462.297 |
| X+E | 663150.388 | 689407.340 | 2652601.552 |

Counts include repeated active-coefficient/input/endpoint validation, alias
checks, the full matrix operation, scratch finite scan and output copy. They
exclude model loading, input generation, comparisons, extraction, FFT/WOLA,
controller/statistic inference, cabin renderer, linked safety and callback
scheduling. This is materially less frame-kernel work than the earlier frozen144
probe's roughly6.975M/1024, but is **not** a full-chain speedup or available
physical-Krait headroom. Compiler/code paths differ; target wall time remains
unmeasured. No denominator change to a single256-sample hop is hidden here.

Model allocation is32848B in either mode; active float32 coefficients occupy
16416B for E-only or32832B for X+E. Scratch is8208B, temporal state zero, before
caller spectra and PCM frontend state. Multiple source reconstructions still
require multiple matrix applications; the missing real-time extractor may
dominate total cost.

Maximum per-component C error versus the once-quantized reference is1.192e-7
for fitted E-only and2.384e-7 for fitted X+E. Separately, original complex128 to
float32 coefficient quantization gives maximum complex output differences
1.333e-7/1.201e-7 on these synthetic inputs; both comparisons pass unchanged
rtol3e-5/atol3e-7. This is not native music-PCM/WOLA parity or a source-quality
check. The combined R1/C1/C2 Python suite now passes124 tests, including two
new export tests.

Independent readback checked26 artifact hashes and11 snapshot/current-code
hash pairs. Four fitted-study external bindings were also checked by the
verifier. The initial strict-warning compile failure remains preserved as
`r1-linear-native-a`; the final fresh owner is `r1-linear-native-b`. No production
integration, car installation or real-time deadline approval followed.
