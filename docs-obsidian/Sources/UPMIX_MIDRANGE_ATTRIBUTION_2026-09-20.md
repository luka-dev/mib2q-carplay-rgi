# Front/centre loss attribution — 2026-09-20

> Follow-up: [C guards fixed and independent retention sweep completed](UPMIX_GUARD_RETENTION_2026-09-20.md).
> The stale-guard section below records the pre-fix finding, not current code.

Offline follow-up to the [corrected seven-track rerender](UPMIX_MATRIX_RERENDER_2026-09-20.md).
No vehicle I/O, production changes, fresh upmix, PCM writes or deployment.
This locates the loss in the cached PCM/model, not in an observed live vehicle.

## Result

All seven cached `gain4096_fb0` clips and original stereo sources are hash-checked
against the v7 report. Same source window5..55s; known A-path delay and512-frame
upmix latency compensated. C is exactly zero throughout all seven PCM6 clips.

Before the amplifier, mean L/R power in500–2000Hz has already fallen **4.483dB**
(track range−7.054..−1.709dB). The logged front gain31392/32768 accounts for only
−0.373dB. The remaining mean change is−4.110dB, not a scalar volume offset.
These are paired per-track dB averages, not acoustic SPL or perceptual loudness.

After the amplifier's frequency weighting, the exact loss decomposition is:

| Group,500–2000Hz | Change in separate L/R contribution powers | Change in L/R summation gain | Other inputs | Total versus dry BANK1 |
|---|---:|---:|---:|---:|
| Front mid/treble | −4.683723dB | −0.000036dB | +0.000005dB | −4.683754dB |
| Centre mid | −4.149332dB | −0.662758dB | +0.000008dB | −4.812082dB |

Thus front loss is already in the authored front channels, not caused by
significant LFE/rear interference inside this amplifier scenario. The centre
also loses some of its original constructive L/R summation: summation gain is
still **positive on every clip** (dry+1.844..+2.777dB; authored+0.721..+2.240dB).
Do not call this an absolute phase inversion or a uniformly cancelling centre.
It is less constructive addition after the upmix altered the stereo content.

Per-track500–2000Hz changes, dB:

| Track | Raw front L/R | Front mid/treble output | Centre separate-power term | Centre summation change | Centre output |
|---|---:|---:|---:|---:|---:|
| Boost Up | −4.023 | −4.193 | −3.846 | −1.212 | −5.058 |
| Crazy Train | −4.024 | −4.139 | −3.528 | −1.123 | −4.651 |
| Deep Down Low | −2.989 | −3.104 | −2.868 | −0.386 | −3.254 |
| Heart-Shaped Box | −6.216 | −6.444 | −5.912 | −0.534 | −6.446 |
| The Blue-Eyed Devil | −1.709 | −1.665 | −1.811 | −0.215 | −2.025 |
| UP. UP. UP. | −7.054 | −7.542 | −6.280 | −0.659 | −6.939 |
| take your vibes and go | −5.363 | −5.700 | −4.800 | −0.510 | −5.311 |

## Relation to the source code

Current `hssr_dsp.c:1469` supplies exact delayed S16 dry samples for front L/R.
`stereo_upmix.c:745` scales them; `:757` onward subtracts the source-aligned
transfer via `spa_upmix_transfer_front`, with Q15 factor81920 (2.5) in the
non-cabin branch. At rear gain13107/32768≈0.4, the nominal product is≈1.
The low shelf, source estimator and limiter make the removed signal depend on
frequency/content. The explicit PCM C output remains0. This is consistent with
the measured midrange depletion; it is not an assertion that cached binaries
are bit-identical to today's dirty working-tree C sources.

The measured residual `gain * delayed_dry - cached_front` includes subtraction,
rounding and any clipping/history differences. It is **not** a recovered trace
of the internal source estimator. No semantic-vocal preservation claim follows.

## Diagnostic recovery experiment, not a production patch

Only replace the front PCM samples by
`front' = (1-alpha)*cached_front + alpha*gain*delayed_dry`.
C/LFE/Ls/Rs samples stay identical. Blend in float; no new quantization or guard.
All output figures are through the same v7 chain versus dry L/R-only BANK1.

| Metric | Existing | alpha0.5 | alpha1.0 |
|---|---:|---:|---:|
| Front mid/treble500–2000Hz | −4.684dB | −2.664dB | −0.373dB |
| Centre mid500–2000Hz | −4.812dB | −2.665dB | −0.373dB |
| Front woofers60–120Hz | −0.380dB | −0.377dB | −0.373dB |
| Sub20–60Hz | −1.290dB | −1.289dB | −1.289dB |
| Rear doors500–2000Hz | +0.125dB | +0.849dB | +1.896dB |
| Woofer60–120 / front-mid500–2000 relative tilt | +4.303dB | +2.287dB | ≈0dB |
| Maximum modeled output sample peak | +4.727dBFS | +4.831dBFS | +5.351dBFS |

The experiment recovers midrange without materially increasing low bass, but
the rear **output** also changes because the corrected matrix feeds it from
front L/R as well as Ls/Rs. Keeping rear PCM unchanged does not keep rear
speakers unchanged. Full recovery also abandons the original front-removal
policy; it is a diagnostic endpoint, not an accepted spatial mix.
Candidate PCM peaks remain−0.373dBFS, while modeled outputs exceed0dBFS.
Those pre-volume electrical numbers require headroom analysis, not a claim
that the physical power amplifier clips. A global gain adjustment alone cannot
restore the lost relative midrange or preserve the spatial transfer policy.

## Blocking issue for a production candidate: old C guard matrix

Read-only inspection found `stereo_upmix.c:60` still has the old6×8
`kInputMatrix`. `soundcheck.c:297` consumes it as `matrix[input*8+bus]`.
The float host renderer and `audio_hook.c:3473` pass that table to the guard.
It disagrees with corrected `AmpChain.M.T` in22/48 coefficients (rtol1e−7,
atol1e−10). Examples:

| Input → bus | Current C guard | Corrected fixed BANK1 model |
|---|---:|---:|
| L → centre bus2 | 0.501187234 | 0.398107171 |
| L → sub bus3 | 0.000011220 | 0.398107171 |
| L → rear bus4 | 0.000011220 | 0.707945764 |
| Ls → front bus0 | 0.707945784 | 0.000011220 |

The S16 `spa_upmix_lfe_cabin_limit` also uses the prior topology: for example,
front constraint includes rear and LFE while the old sub constraint has base0.
These observations invalidate using those helpers as proof of corrected-model
headroom. They do not prove current runtime guard enablement, vehicle state or
that this guard caused the bass complaint. No C guard changes were made here.

## Method / reproduction

`audit_amp_midrange.py` independently renders six contributions for each of
outputs0,1,10 through the recovered fixed A path. For each band:
`P(L+R)=P(L)+P(R)+cross`; signed cross terms are retained. Group powers are
averaged in linear units before conversion to dB. The change in total power
is split into separate-power change, summation-gain change and non-L/R change.
The fixed factorization explicitly rejects common-input/B/complex/runtime
scenarios; no superposition assumption is applied to adaptive/nonlinear DSP.

The recomposed front/centre band changes match the previous unfactored v7
report within **3.36e−12dB**. Six added tests cover all16 output sums,
individual input routes/delays, constructive/destructive cross terms, gain-only
attribution, front-blend isolation and scenario guards.411 tests pass:
121 extraction+290 model. `git diff --check` passes.

```sh
OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_midrange.py \
  --report output/upmix-matrix-corrected-20260920/music-v7/report.json \
  --out output/upmix-matrix-corrected-20260920/midrange-attribution.json
```

The JSON has `complete:true`, all seven source/log hashes, per-input band
powers, signed cross terms, attribution closure checks and both candidate
renders. The `/tmp` PCM cache is still required to reproduce it.

Next local/static work: correct and regression-test both C guard topology
representations against the extracted matrix **before** using them to validate
a new float candidate; then make front retention an explicit independent
experimental parameter and rerender spatial/bass/headroom tradeoffs. Do not
deploy or change live settings. Active B, common EQ and physical mapping remain
unverified; the conclusions above remain conditional on v7's stated hypotheses.
