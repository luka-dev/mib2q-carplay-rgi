# HSSR Object checkpoint — 2026-09-05

`hssr-object` is an object-selective profile layered on the same 512/128 STFT,
451,464-parameter int8 2x160 GRU, and 512-frame latency as `hssr-active`.
`hssr-active` is unchanged and remains the immediate rollback.

The new profile adds a temporally smoothed fine-bin mask inside each semantic
band. Stable spectral peaks and bins whose measured pan agrees with the
semantic pan receive up to `+4.1 dB` relative makeup; low-contrast inter-peak
material stays about 14 dB below the band mask. Selected material retains 95%
of its original channel instead of 85%, semantic pan is expanded by 1.18, and
front subtraction increases from `1.75 x` to `2.0 x`. Rear Q15 remains bounded
at 8192 (`0.25`), digital C remains zero, and LFE is unchanged.

The retained stronger revision adds a class-gated post-mask stage. Rearable
background/FX/instrument bands receive up to `+3.2 dB`; lead and bass reduce
the objectness score. This is not a change to the external rear-gain limit.

## Objective checks

At rear gain `0.25` with LFE assist, the 24-song held-out aggregate is:

| backend | rear envelope corr | waveform corr | SI-SDR dB | L/R corr error | rear energy ratio | front SNR dB | fold SNR dB |
|---|---:|---:|---:|---:|---:|---:|---:|
| hssr-active | 0.384 | 0.537 | -3.99 | 0.207 | 0.135 | 15.97 | 20.47 |
| hssr-object | 0.401 | 0.534 | -4.11 | 0.147 | 0.171 | 12.85 | 16.64 |

The stronger revision raises held-out rear energy by about 27% while improving
reference-envelope tracking and Ls/Rs distinction over Active. It then moves
accepted material farther from the matching front. Predicted SoundAmp peak
mean/max was `0.392/0.489` for Active and `0.382/0.489` for Object.

On the public authored-music corpus, rear-envelope correlation improved from
`0.268` to `0.320`; on aligned Apple public previews it improved from `0.710`
to `0.741`. L/R-correlation error was mixed on those small public corpora, so
vehicle listening remains the decision gate.

Eight held-out isolated-stem probes show the intended protection/selectivity
tradeoff:

| stem | hssr-active mean/worst | hssr-object mean/worst |
|---|---:|---:|
| vocals | -26.13 / -16.74 dB | -24.15 / -13.47 dB |
| bass | -36.13 / -19.29 dB | -38.77 / -20.26 dB |
| drums | -25.83 / -20.77 dB | -24.83 / -18.82 dB |
| other | -15.18 / -12.18 dB | -12.90 / -8.88 dB |

MUSDB vocals combines lead, backing vocals, and vocal effects. The stronger
profile is about 2 dB more permissive on the aggregate vocal stem and must be
checked by ear. Bass remains more strongly rejected than Active, while the
`other` stem is about 2.3 dB louder on average.

## Krait benchmark and deployment

GCC 8.5 QNX benchmark, 512 calls of 1024 frames:

| backend | mean | P95 | P99 | max | mean deadline share |
|---|---:|---:|---:|---:|---:|
| hssr-active | 4.491 ms | 7.050 ms | 9.065 ms | 11.079 ms | 21.05% |
| hssr-object | 5.022 ms | 7.050 ms | 9.065 ms | 13.093 ms | 23.54% |

The stronger build was benchmarked while live audio and unit scheduling load
were active. Repeats showed system-wide jitter affecting Active as well as
Object; one representative Object run was 6.00 ms mean, 12.09 ms P95 and
21.15 ms P99, while later contention made both profiles exceed the nominal
wall-time deadline. The added work itself is bounded to 32 control gains and
one multiply per spectral bin. Therefore the release gate is the realtime
health trace rather than the unprioritized concurrent benchmark process.

The complete repository suite and production GCC 8.5 hook build pass. The
deployed/read-back hook SHA-256 is
`d9d6cf661fd2218a223014480c51b7e3ae06bb36362e0d647b5f60dfba61dae3`.
Live trace records `upmix=hssr-object rear_q15=8192 lfe=1`. After the single
startup zero-PCM packet, the retained health window has no decode failure,
ring wait, QSA cadence gap, or device underrun.

Unit rollback copies include
`libcarplay_hook.so.pre-hssr-object` and
`libcarplay_hook.so.pre-hssr-object-boost`; startup rollback is
`carplay_startup.sh.pre-hssr-object`. The direct policy rollback is
`AUDIO_STEREO_UPMIX=hssr-active` followed by a `dio_manager` restart.

The current MUSDB-derived model remains development-only.
