# HSSR Active checkpoint — 2026-09-05

`hssr-active` is an explicit assertive profile built on the same 512/128 STFT
and int8 2x160 GRU as `hssr-neon`. The conservative backend is unchanged.

Active policy changes:

- background/FX and spatial-percussion posteriors receive higher semantic makeup;
- lead uses an eighth-power veto and low end a fifth-power veto;
- rear-mask attack is faster and release is slower;
- selected rear material is subtracted from the matching front at `1.75 x`
  instead of the conservative `sqrt(0.5)` fold coefficient;
- rear output remains capped at Q15 `8192` (`0.25`), FC remains zero, and the
  existing LFE/headroom contract is unchanged.

## Objective checks

At rear gain `0.25`, the 24-song held-out aggregate changed from rear energy
ratio `0.117` to `0.137` and envelope correlation from `0.376` to `0.392`.
The stronger intentional front relocation changes front SNR from `25.15 dB`
to `15.86 dB` and fold-back SNR from `75.60 dB` to `20.35 dB`.

On the public authored corpus, rear-envelope correlation changed from `0.199`
to `0.268`; on aligned Apple public previews it changed from `0.661` to
`0.710`. Mean/max predicted SoundAmp peaks did not increase.

Eight held-out isolated-stem probes:

| stem | mean rear/input | worst track |
|---|---:|---:|
| vocals | -22.33 dB | -15.11 dB |
| bass | -34.57 dB | -26.69 dB |
| drums | -25.37 dB | -21.49 dB |
| other | -15.66 dB | -12.74 dB |

MUSDB's vocals stem combines lead, backing and effects, so vehicle listening
remains the decision gate.

## Krait benchmark and deployment

GCC 8.5 QNX benchmark, 512 calls of 1024 frames:

| backend | mean | P95 | P99 | max | mean deadline share |
|---|---:|---:|---:|---:|---:|
| hssr-neon | 4.591 ms | 7.050 ms | 8.057 ms | 9.065 ms | 21.52% |
| hssr-active | 4.694 ms | 7.050 ms | 9.065 ms | 10.072 ms | 22.00% |

The complete repository suite and GCC 8.5 hook build passed. The deployed hook
SHA-256 is `b9a374a2ce1b384280689ba18592d407804832ae719ebaf6340e577604e1d76d`
and matches the local artifact. Live trace records
`upmix=hssr-active rear_q15=8192 lfe=1`; after the bounded startup clock resets,
health counters show no upmix failure, drop, ring wait, cadence failure or
device underrun.

Rollback copies are installed on the unit as
`libcarplay_hook.so.pre-hssr-active` and
`carplay_startup.sh.pre-hssr-active`. The direct policy rollback is
`AUDIO_STEREO_UPMIX=hssr-neon` plus a `dio_manager` restart.

The current MUSDB-derived model remains development-only.
