# HSSR through the documentary Audi 9VS topology

Date: 2026-09-06. Symphoria/3D is off.

The test applies the exact recovered J525 profile-0/bank-1 input matrix and
then expands its logical buses to the sixteen documented T38 speaker circuits.
The second stage is an explicitly inferred equal-power fan-out; it is not Audi
per-speaker calibration. See
[`SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md`](SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md)
for the proof boundary.

## Ten-track held-out set

These are the retained `hssr-object-heldout-eval-v4` renders.

| Track | HSSR Active rear fraction | HSSR Object rear fraction | Object delta |
|---|---:|---:|---:|
| authored-sintel-5-1 | 8.172% | 6.766% | -1.407 pp |
| liznelson-rainfall | 2.865% | 1.526% | -1.338 pp |
| music-delta---80s-rock | 3.014% | 3.172% | +0.158 pp |
| music-delta---beatles | 1.207% | 1.279% | +0.072 pp |
| music-delta---britpop | 5.373% | 4.706% | -0.667 pp |
| music-delta---country1 | 0.978% | 0.654% | -0.324 pp |
| music-delta---disco | 0.633% | 0.488% | -0.145 pp |
| music-delta---gospel | 0.198% | 0.201% | +0.003 pp |
| music-delta---grunge | 2.638% | 2.573% | -0.065 pp |
| phoenix-scotchmorris | 15.108% | 11.968% | -3.140 pp |
| **Arithmetic mean** | **4.019%** | **3.333%** | **-0.686 pp** |

Mean rear/front electrical energy is `-16.468 dB` for Active and `-17.259 dB`
for Object. There are no pre-clipping samples in either condition. Object is
quieter in aggregate rear energy on seven of ten tracks even though its
fine-bin selection can make one accepted guitar or effect easier to identify.
This is consistent with the vehicle report: object selectivity improved, but
the rear bed can still feel sparse.

## Six aligned Apple public-preview renders

On this smaller, differently distributed set, Object is more active:

| Condition | Mean rear fraction | Mean rear/front energy |
|---|---:|---:|
| HSSR Active | 18.788% | -6.616 dB |
| HSSR Object | 20.361% | -6.126 dB |

This corpus-dependent reversal is important. A single global rear-gain change
will not reliably produce both stronger isolation and stable activity across
genres. The likely next algorithmic target is the acceptance/hold policy for
semantic objects, not an invented cabin EQ.

## Interpretation

- The documented physical fan-out does not reveal a hidden extra upmix stage
  with Symphoria/3D off. `Bus_Ls` drives the rear-left door circuit and R209;
  `Bus_Rs` drives the rear-right door circuit and R210 in the documentary
  architecture.
- HSSR already supplies enough rear energy on some wide/orchestral material,
  while several dense/pop mixes remain below 1–3% rear electrical energy.
- The subjective request for "more isolated instrument behind" cannot be
  solved honestly by assigning the recovered but unbound OEM EQ/crossover
  catalog to speakers. That would manufacture calibration data.
- A safe next profile change should increase object persistence and makeup only
  after semantic acceptance, while retaining the existing lead/bass veto and
  matrix peak guard.

Generated reports and 16-channel circuit WAVs are under the ignored directory:

- `build/soundamp-virtual-cabin/documentary-circuits/`
- `build/soundamp-virtual-cabin/heldout-v4-circuits/`
