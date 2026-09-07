# HSSR Universal V11 + Cabin-Aware LFE V2 through the Audi 9VS topology

Date: 2026-09-07. Symphoria/3D is off.

This is the current Universal-only electrical regression. Each input uses the
V11 `hssr-universal` renderer at rear gain `0.25` with Cabin-Aware LFE V2. The
first Virtual Cabin stage is exact: it applies the recovered J525
profile-0/bank-1 matrix to `L,R,C,LFE,Ls,Rs`.

The later logical-bus-to-sixteen-circuit fan-out remains an inferred
equal-power visualization, not recovered Audi per-speaker calibration. See
[`SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md`](SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md)
for that proof boundary.

## 100-excerpt music stress corpus

The current regression uses 50 locally supplied songs, two deterministic
10-second excerpts per song. It has no authored surround reference, so these
numbers measure LFE activity, stereo compatibility and exact J525 electrical
headroom—not studio-object recovery.

| Path | mean LFE energy | mean LFE/front | mean/max LFE peak | maximum J525 bus | clipped samples |
|---|---:|---:|---:|---:|---:|
| Removed fixed assist baseline | 0.362% | -22.54 dB | -22.32/-15.44 dBFS | -0.27 dBFS | 0 |
| **Cabin-Aware LFE V2** | **0.448%** | **-21.77 dB** | **-20.72/-14.11 dBFS** | **-0.48 dBFS** | **0** |

V2 raises mean LFE energy by 23.9%, mean LFE/front level by 0.77 dB and
mean LFE peak by 1.60 dB. Despite that increase, the worst recovered J525 bus
gains 0.21 dB of headroom because the limiter solves the allowed LFE interval
from the current front/rear samples instead of relying on a fixed scalar cap.
The paired front trim costs about 0.20 dB in the corpus front/fold SNR checks.

## Controlled invariants

- A full-level 40 Hz coherent tone produces LFE RMS `0.135107`.
- The same tone with exact `L=-R` produces the same RMS `0.135107`; the removed
  mono-sum path produced digital zero.
- A 500 Hz tone produces LFE RMS `0.000131`, about 60.3 dB below 40 Hz.
- Unit tests recompute all six J525 logical buses per sample and require each
  to remain below `0.985` full scale.
- Arbitrary callback chunking and reset/re-render remain byte-identical.

The current implementation contains no legacy LFE mode or runtime fallback.
`AUDIO_STEREO_LFE=assist` always means V2; `off` remains the explicit no-LFE
safety choice.

These electrical checks still cannot predict the driver's perceived subwoofer
level. OEM per-circuit gain, delay, crossover, EQ, polarity, loudspeaker
directivity and cabin transfer functions remain unbound. A stationary vehicle
A/B is the final perceptual gate.

Complete machine-readable results are generated under the ignored directory
`build/lfe-v2-eval/`. The reproducible implementation and performance record is
[`../tools/stereo-upmix-render/bench/results/2026-09-07-cabin-aware-lfe-v2.md`](../tools/stereo-upmix-render/bench/results/2026-09-07-cabin-aware-lfe-v2.md).
