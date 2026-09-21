# Corrected guards and independent front retention — 2026-09-20

> Follow-up: [105-render grid, bass/LFE, headroom and matched comparison](UPMIX_TUNING_SWEEP_2026-09-20.md).

All three requested local steps are implemented: C guard topology correction,
independent front-retention control, and a fresh seven-track candidate sweep.
No vehicle I/O, deployment, startup-setting change or commit. This changes local
production sources; it does **not** change what is running in the car.

## Implementation

`hook/audio/stereo_upmix.c` now has one corrected input-major6×8 table shared
by the float matrix detector and S16 protection. All48 coefficients match
`AmpChain.M.T` from the extracted schema2 audit, including−99dB floor and the
two extra buses. The stale hand-written S16 LFE topology was removed.

S16 first tries reducing LFE within its original sign/magnitude to satisfy
all eight bus constraints. It never invents LFE from zero. If LFE alone cannot
repair a frame, a linked attenuation of all six channels bounds all eight
buses and PCM rails. This fallback also works with LFE disabled. It is an
instantaneous emergency guard, not a transparent mastering limiter; OFF remains
the exact bypass. The S16 ceiling is32275 units with8 units reserved during
fallback for weighted quantization error. Float keeps the existing linked−1dBFS
guard/release policy, now watching the corrected table. Monitoring all8 buses
is conservative even where a downstream A route is muted in the chosen row.

Also fixed two S16 transfer boundary errors: the2.5× intermediate no longer
narrows to int16 before subtraction (16384×2.5=40960 must not wrap), and the
positive unity-gain clamp no longer casts+32768 to−32768.

New `front_retention_q15`,0..32768, scales only source removal by
`1-retention`. Rear synthesis gain, source estimator and LFE synthesis are not
retuned. Final linked limiting and amplifier routing can still change other
output levels. Zero retains the existing transfer policy; one disables front
source removal. Both float and S16 implement the control.

Interfaces:

- Host: `--front-retention 0..1`.
- Hook: `AUDIO_STEREO_FRONT_RETENTION=0..1`, parsed once and logged as Q15.
- Default remains0. This does not enable float; that remains a separate opt-in.
- No startup scripts or live environment values were changed.

## Fresh music sweep

Authoritative result: `output/upmix-retention-candidates-final-20260920/report.json`.
It contains35 completed renders, logs/commands, binary/source/model hashes,
PCM hashes, per-group bands, preguard peaks, limiter activity and matrix bounds.
WAV paths point into its private temporary PCM directory. The earlier
`output/upmix-retention-candidates-20260920/` run is retained as an intermediate;
the final run includes the positive-unity-clamp fix.

Inputs are the same seven hash-verified stereo clips used in the v7 report.
Fresh C synthesis uses `build/hssr_model.bin` (SHA256
`96abe57d9ee703f2e39c0231de97580c303ce896ce044b7450c14240002b0a0e`)
and `build/hssr_guard.bin` (SHA256
`ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae`).
Fixed surround0.4, front bass0dB, LFE assist0dB re4096, unity programme gain.
No invented Sound Check metadata. Full60s processed;5..55s source window,
512-frame upmix and known output A delays compensated.

AmpChain v7: profile0/row1, B1/B2=0, C4D17 identity, no extra common-input EQ
or runtime gain/limiter. These are hypotheses, not observed vehicle state.
Reference is dry L/R-only PCM6 in BANK1, **not stock/bank0 or cabin SPL**.
Numbers are mean paired dB changes across7 tracks. The bass/mid tilt is an
electrical cross-group diagnostic, not a perceptual loudness score.

| Metric | Fresh S16,retain0 | Float0 | Float50% | Float75% | Float100% |
|---|---:|---:|---:|---:|---:|
| Front mid500–2000Hz | −4.681 | −4.856 | −2.880 | −1.723 | −0.662 |
| Centre mid500–2000Hz | −4.814 | −4.969 | −2.857 | −1.696 | −0.633 |
| Front woofers60–120Hz | −0.385 | −0.840 | −0.833 | −0.843 | −0.869 |
| Sub20–60Hz | −1.291 | −1.422 | −1.424 | −1.437 | −1.467 |
| Rear doors500–2000Hz | +0.090 | −0.152 | +0.567 | +1.051 | +1.557 |
| Bass/mid tilt | +4.296 | +4.017 | +2.047 | +0.880 | −0.207 |
| Maximum modeled output peak,dBFS | +4.727 | +4.029 | +4.242 | +4.330 | +4.964 |

The guard correction alone does not fix the relative midrange depletion.
Float75% is a useful **next offline/listening candidate**, not an accepted
production default: it leaves some source removal while reducing mean tilt to
+0.880dB (per-track−0.538..+1.514). Float100% is closer to the dry tonal balance
but removes no source from front and increases rear output further. Neither
variant has been listened to in this task; source/object selectivity and real
cabin preference remain untested.

## Headroom: what passed and what did not

All35 outputs: zero PCM rail-hit samples; all eight modeled input-matrix buses
within the corresponding guard ceiling. S16 maximum0.984947FS; float maxima
≤0.891278FS (S16 quantization tolerance included).

At75% retention, the linked guard attenuates13.355% of frames across the corpus;
minimum instantaneous gain0.253580 (about−11.92dB). At0/50/100%, attenuated
fractions are10.767/11.707/16.652%. These peaks/release intervals may affect
dynamics; an input-bound guarantee is not a transparency guarantee.

**After downstream EQ**, v7 still gives up to+4.330dBFS for75%. The input guard
does not bound this full path. For these already-rendered PCM files under this
fixed linear model, a further constant **post-guard** attenuation of at least
5.34dB would put the maximum below−1dBFS. This is only a finite-corpus/model
calculation, not a proposed hardware volume or Sound Check value; applying gain
before the nonlinear guard would require rerendering. Unknown live gains,
limiters, B audio and common EQ prevent a physical-clipping verdict.

## Verification and reproduction

416 Python tests pass:121 extraction+295 model/tool tests. New compiled-C
checks compare all48 coefficients directly to the extraction, then729 signed
corners plus20000 random PCM6 frames in each guard mode. C tests additionally
exercise30000 frames, transfer overflow/clamp limits, independent float/S16
retention, invalid controls, chunk/reset/inference invariance and matrix peaks.

S16, float and real-hook host interposer tests pass with AddressSanitizer and
UndefinedBehaviorSanitizer. Hook parser tests cover bounds, NaN/Inf and junk.
Two pre-existing float test fixtures still described pre-scene transfer/delay;
they now inject `source_ola` and assert the current shelf/576-frame delay rather
than synthetic-rear subtraction. No cabin-mode algorithm change was made.

`scripts/run_tests.sh` passed its audio section but **is not globally green**:
the unrelated card installer tests fail on missing payload tree/generated
uninstall mismatch (20 tests;9 failures/16 subtest errors). Those files were not
changed. No ARM/QNX target build or realtime vehicle CPU measurement was made.

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook/audio \
  tools/stereo-upmix-render/stereo_upmix_render.c hook/audio/soundcheck.c \
  hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
  -lm -o build/upmix-retention-20260920/renderer

OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_retention_candidates.py \
  --report output/upmix-matrix-corrected-20260920/music-v7/report.json \
  --renderer build/upmix-retention-20260920/renderer \
  --model build/hssr_model.bin --guard build/hssr_guard.bin \
  --out output/upmix-retention-candidates-NEW
```

Next safe work: evaluate75% with explicit downstream headroom scenarios and
level-matched offline audition/source-selectivity tests; decide whether to keep
75% or100% only with those tradeoffs visible. Retain static-only scope. Do not
interpret this candidate as proof of active B=0, pinout, stock equivalence, or
resolution of the live listening complaint. Existing unrelated WIP is retained.
