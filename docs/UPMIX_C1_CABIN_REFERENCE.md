# C1: candidate-stem Virtual Cabin rendering reference

2026-09-15 UTC. Implementation of the first offline step in
[the cabin-first redesign](UPMIX_CABIN_FIRST_REDESIGN.md).
**Not a trained model, approved musical-role target, acoustic cabin simulation,
production HSSR replacement or vehicle installation.**

**Complete:** all six cases and four modes finished on NAS. A separate fresh
WAV/matrix audit passed for all 120 audio files. The compact review subset is
available locally at `output/c1-review-a/` (36 WAVs); full results remain on NAS.

## Implemented comparison

New independent files under `tools/stereo-upmix-render/`:

- `c1_role_fixture.py`: source-SHA-verified native TRAIN crops and explicit
  mix/support/protected/unknown accounting. No teacher, resampling, padding,
  independent stem normalization or automatic semantic admission.
- `c1_cabin_renderer.py`: deterministic streaming reference with optional causal
  high-pass, external source permission, bounded M/S electrical compensation,
  rear headroom control before one common linked limiter, and S16 rail checks.
- `c1_reference_metrics.py`: independent electrical-bus measurements with
  raw and eligible ratios; no automatic music-quality pass.
- `run_c1_cabin_reference.py`: fixed four-mode comparison, core-only scoring
  after real pre-roll, counterfactual controls and bound audio/report artifacts.
- `qualify_run_c1_reference.py`: bounded qualification/build/render subprocesses
  with retained logs; no optimizer, model export, deletion or installation.

Both 44.1 and 48 kHz are supported offline; the original fixture stays at
44.1 kHz. This is not a change to the car's transport sample-rate contract.

| Mode | Role in the experiment |
| --- | --- |
| bypass | Stereo front reference through the same C1 safety stage; zero rear. |
| additive | Original front plus selected supporting source in rear, no front subtraction. |
| generic_closure | Historical `sqrt(.5)` rear-fold geometry with C1 safety, not bit parity with P1 or production HSSR. |
| cabin_balanced | Same requested rear, zero deliberate source removal, bounded front/center compensation using the known J525 proxy. |

The **additive / cabin_balanced** pair isolates the compensation request;
actual rear attenuation and linked gains are separately compared because
headroom can make their realized rear levels differ. The generic-fold control
does not share the zero-front-subtraction policy of that pair.

Initial settings are explicitly exploratory, not cabin calibration:
rear gain .25, first-order supporting-source HP 120 Hz, front/center weights
1/1, regularization 0, M/S correction coefficient cap 1, removal gain 0,
uniform SoundCheck-stage gain 1, rail .98, rear/limiter release 50 ms.
There is no inferred Apple SoundCheck metadata and no extra gain stage.
PCM C/LFE remain zero; physical center and other-channel bass do not.

The optional source-removal implementation is tested but NOT enabled in this
first comparison. A later relocation experiment must explicitly change its
plan and compare against these retained controls.

## Fixed original-stem fixture

Private NAS prefix:
`B=/mnt/shared/users/luka/Audi/upmix-training-2026-09-11`.

Fixture: `B/external/c1-role-fixture-a`.
Completed in 25.293 seconds, full SHA checks on **82 original stem files**.
Six cases, native 44,100 Hz; each includes actual 58–60 s history followed by
the fixed **60–76 s scored passage**. No quiet-case replacement or activity
search after selecting those coordinates.

| TRAIN song | Candidate / control |
| --- | --- |
| Scream And Shout | Background-vocal metadata candidate. |
| Enouement | Backing candidate; known historical H1 song, not a new untouched test. |
| Stolen Car | Synth-pad candidate. |
| Lifes Changes | One clean-guitar and one pad candidate. |
| Real | Hi-hat candidate; catalog foreground foundation remains protected. |
| We're On Again | Zero-support negative; both catalog lead anchors protected. |

Six artists, all from the frozen TRAIN partition. No new DEV/held-out audio
opened. Electronic metadata is not proof of dubstep coverage or supporting
role. Names and dataset families justify *candidate selection*, not approval.
No listening judgment has been fabricated.

`protected` conservatively sums catalog lead and foundation stems. It is not
a pure vocal stem or guaranteed exhaustive foreground annotation. Stored wet
effects remain with their sources, but wet-send completeness is unknown.
Unselected material is retained as `unknown`, not sent to rear as ambience.
The mix is the common-origin unit-gain sum of the original stems, not a claim
to reproduce a separately mastered commercial stereo file.

The fixture contains 152,411,712 audio bytes (about 145.35 MiB), with SHA-bound
plan/manifest, source headers/splits/IDs, original-file hashes, builder source
hash/environment, real pre-roll and source-sum checks. Manifest SHA256:
`39e00d26f492e02a407315465bbf392fd6384787690a4a40633debcabf978d22`.
Roles remain `candidate`, `semantic_role_approved=false`, `car_approved=false`.

## Qualification and bounded run history

- Mac: **60 tests passed** (39 C1 plus 21 existing P1 tests), 1.362 s on the
  final output-storage revision. Existing P1 source files were not edited.
- Renderer tests include both native rates, exact arbitrary chunks/reset,
  causality, invalid-input and derived-overflow transactionality, immediate
  permission veto, common-gain ownership and float/S16/input-bus rails.
- Fixture tests reject held-out/non-TRAIN data, protected/bleeding rear
  candidates, SRC/padding, source/WAV changes, metadata hash races and builder
  mutation; final rechecks cover every WAV. These are provenance controls,
  not human confirmation of musical roles.
- Independent metric tests cover generic-fold equations, the weighted
  compensation objective, phase versus magnitude errors, quiet/zero/short
  domains, LF energy and limiter effects. None declares an audible winner.

Owner A: `B/runtime/c1-cabin-reference-a`; frozen archive SHA256
`7a348f3f8faf69e544f14c94a5697f4181855293384b5767df82865c31dd0a95`.
Its NAS 39-test qualification and real fixture build completed successfully.
Its comparison stopped **before creating comparison audio** because an
over-conservative derivative-output estimate exceeded the 2 GiB budget.
Logs and the unchanged fixture were retained, not overwritten or deleted.

Owner B: `B/runtime/c1-cabin-reference-b`; frozen archive SHA256
`93d05c15111e045eaff690c51f324643f619ca82f63ad8271ca9379c8e2442eb`.
The revision removes duplicate audio derivatives and counts the actual 52
FLOAT output channels per scored frame plus metadata allowance. It does not
change the renderer, source selections, source samples, metrics or gains.
It explicitly reuses and revalidates the existing fixture by manifest hash.

Owner B launched 2026-09-15 18:48:17 UTC. NAS **39 tests passed** in 1.896 s
(qualification subprocess 2.069 s); log SHA256
`7bd6799d50fc066ee63fc807548ba758700ece91210434150ece29b9ca470a7b`.
Fixture reuse verification passed; comparison began at 18:48:20 UTC.
The render has a 1,800-second internal budget, 2 GiB output cap and 100 GiB
free-space reserve. Terminal results must be read before reporting completion.

Owner B finished normally after 288.882 comparison-subprocess seconds. Final
comparison report SHA256:
`2f260cf625c2e6448690aed529c3314306f26ee7e0ec045bb4bbd3e409fec42d`.
The initial launch timestamps above remain historical, not a running status.

### Completed result and independent WAV audit

`audit_c1_reference.py` reread all **120 WAV files**, verified their recorded
hashes/geometry, source-sum accounting, zero C/LFE and PCM/bus rails, recomputed
rear mean-square and the independently specified input matrix, and checked
power-matched derivatives. Its two positive/negative tests passed on Mac and
NAS. This is fresh PCM verification, but **not a learned-model or renderer
replay, musical-role judgment or listening test**.

- Auditor SHA256: `cd2602442d2656ba89279616714e41824bc0a1030f3d89824c5beb011dd8bc4e`.
- `final-audit.json` SHA256: `ef4d8a8b9d3c93d9d11ee19c5304023072c48c7fcba9475a9b0d4753d30a4248`.
- Review ZIP SHA256: `417f66bfb55a87a8b0959f513e0c0feafe671bcbe5cc3d83d41b0ff137eae8ca`,
  153,614,207 bytes. Local download and all 44 indexed members were verified;
  36 are stereo audition/source WAVs. This subset cannot replay the full study.

All six additive/balanced rear PCM comparisons are exact matches. All 24
case/mode combinations have no scored-core rear attenuation or linked-limiter
attenuation, and zero protected-counterfactual rear response. This only checks
the renderer with fixed supplied support: it does not demonstrate vocal
separation, complete wet protection, or a runtime role recognizer.
Maximum observed bus-proxy sample peak across the study was about .64723;
this is not true-peak, excursion or acoustic safety certification.

| Case | Supplied support core MSE | Sum of L/R/center squared change: balanced / additive |
| --- | ---: | ---: |
| Scream And Shout | 6.919151e-4 | .346984 |
| Lifes Changes, combined guitar+pad | 1.143440e-5 | .242996 |
| Real, hi-hat | 1.488178e-5 | .429048 |
| Stolen Car | 4.538578e-9 | quiet: do not promote a ratio to positive-quality evidence |
| Enouement | 0 | unavailable; incidentally inactive backing in this crop |
| We're On Again | 0 | unavailable; intentionally zero-support negative |

The three active cases have lower **chosen electrical objective** with the
same rear signal; this is expected from the compensation formula, not a
57–76% improvement in perceived sound. There are only three active positives
in this fixed set. The silent and quiet cases remain in the report; subsequent
active-source selection must be a new named fixture, not replacement of these
observations. Separate source relocation and sustained backing/pad/guitar
examples are the next C2 experiment, not an accepted vehicle preset.

## How to interpret and audition the outputs

Each case report includes raw six-slot PCM and original mix/support/protected/
unknown stereo references. Front/rear/center audition WAVs represent the
**known electrical buses**, not binaural audio, measured seats or independently
controlled physical door drivers. Do not play six-channel files through an
unknown browser downmix and treat the result as the cabin surround effect.

Audition variants use attenuation-only matching to the minimum positive total
six-bus sample power. The exact scalar for each mode is recorded. This is
**not perceptual LUFS matching**, not acoustic loudness matching, and not a
second SoundCheck application inside the renderer. Raw PCM and metrics remain
unaltered; raw bus audio is reproducible from that PCM and the recorded matrix.

The full-minus-protected counterfactual holds supplied support/roles fixed and
starts each renderer from the same state history. Its difference includes
nonlinear rear-headroom and limiter responses. It cannot certify learned-role
recognition, perfect source attribution or front-only lead in the physical car.

Read useful rear level alongside front/center changes, LF excess, limiter and
rear attenuation. Quiet/zero reference ratios can be unavailable; zero rear
is a safe control, not a successful surround result. Numerical compensation
improvement is expected from its mathematical objective and is not an audible
quality discovery by itself.

Next: inspect completed artifacts and source roles, then real-chain listening
or valid measured playback for target approval. A strong offline separator and
causal student must later be evaluated separately from this supplied-source,
supplied-role reference. No full training is justified by engineering tests alone.
