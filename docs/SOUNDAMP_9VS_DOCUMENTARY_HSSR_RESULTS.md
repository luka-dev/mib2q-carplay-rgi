# HSSR Universal V11 through the documentary Audi 9VS topology

Date: 2026-09-07. Symphoria/3D is off.

This is the current Universal-only regression. Each input is the final V11
`hssr-universal` render at rear gain `0.25` with LR4/80 Hz LFE assist. The
Virtual Cabin applies the exact recovered J525 profile-0/bank-1 input matrix
and then expands its logical buses to the sixteen documented T38 amplifier
circuits.

The second stage is an explicitly inferred equal-power fan-out, not recovered
Audi per-speaker calibration. See
[`SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md`](SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md)
for the proof boundary.

## Public authored-surround corpus

| Track | Rear circuit energy excluding subwoofer | Rear/front energy | Maximum pre-clip peak | Clipped samples |
|---|---:|---:|---:|---:|
| Dohnanyi, piano and orchestra | 22.45% | -5.38 dB | -10.67 dBFS | 0 |
| Credo, voices and string quartet | 15.23% | -7.46 dB | -7.70 dBFS | 0 |
| Winterreigen, solo piano | 35.24% | -2.64 dB | -16.28 dBFS | 0 |
| Schumann, violin and orchestra | 28.23% | -4.05 dB | -9.74 dBFS | 0 |
| Baroque cello concerto | 22.34% | -5.41 dB | -7.29 dBFS | 0 |
| Atmos trumpet and orchestra | 13.05% | -8.24 dB | -7.18 dBFS | 0 |
| **Arithmetic mean** | **22.76%** | **-5.53 dB** | — | **0** |

The separate EBU spoken channel-identification fixture produces 0.25% rear
energy, -25.95 dB rear/front and a -15.30 dBFS peak. It is a routing check and
is excluded from the music aggregate.

## What this establishes

- The recovered J525 input matrix does not mute or unexpectedly attenuate the
  generated `Ls/Rs` buses.
- In the documentary topology, `Bus_Ls` reaches the rear-left door circuit and
  R209 effect circuit; `Bus_Rs` reaches the rear-right door circuit and R210.
- Universal V11 produces material-dependent rear activity rather than a fixed
  ambience copy. The tested music spans roughly 13.0% to 35.2% rear electrical
  energy after the known matrix.
- All seven renders retain substantial electrical headroom and have zero
  pre-clipping samples.

This still cannot predict the driver's perceived rear level. OEM per-circuit
gain, delay, crossover, EQ, polarity, loudspeaker directivity and cabin transfer
functions remain unbound. A measured driver-head BRIR is required before the
binaural mode can represent the real car instead of only its documented
electrical topology.

Generated 16-channel WAVs and complete per-circuit JSON reports are under the
ignored directory:

- `build/soundamp-virtual-cabin/hssr-selection-v11-public-authored/`
