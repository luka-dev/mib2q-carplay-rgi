# C2: completed supplied-source scene-placement reference

2026-09-15 UTC. **Offline reference, not ML separation, listening approval or a
vehicle build.** Seven fixed TRAIN cases × four configurations completed at
20:08:52 UTC: comparison450.999s; full supervisor488.326s; all phase exits0.

## Scope and reproducibility

```text
B=/mnt/shared/users/luka/Audi/upmix-training-2026-09-11
OWNER=B/runtime/c2-scene-reference-a
FIXTURE=B/external/c2-scene-fixture-a
PYTHON=B/runtime/source-g8-linux-20260912/venv/bin/python
```

Only these new NAS directories were created; C1 artifacts/originals were
preserved. The [frozen seven-case selection](../tools/stereo-upmix-render/c2_reference_selections_v2.json)
retains [documented TRAIN-selection limits](UPMIX_C2_SOURCE_SELECTION.md).
Each native44.1kHz passage contains2s real past plus16s scored audio:
112 scored seconds total. No DEV/held-out, resampling or independent gain.

The unchanged builder freshly verified **99 original source WAV bodies**,
metadata/signatures and source-sum closure. Mix is the unit-gain original-stem
sum, not an independently mastered stereo. Protected means catalog
lead+foundation; unknown stays front. Musical roles and wet-lead completeness
remain unapproved.

**51 C1/C2 tests passed in3.710s.** Frozen snapshot: eight runtime modules, six
test modules and selection. Python3.12.3/NumPy2.5.1/SoundFile0.13.1/libsndfile1.2.2;
numerical threads2. Reserve100GiB, comparison ceiling2GiB, external bound1800s.

Fixture WAVs:177,813,664 bytes; comparison with reports:803,674,583 bytes;
free space:7,551,962,578,944 bytes. Final readback freshly checked **112 WAV
bodies/headers**,968,093,952 bytes, plus code/selection/plan/report bindings and
final signatures. It aggregated recorded metrics without rerendering.
Full evidence/audit: `OWNER/readback.json`, `OWNER/readback_audit.py`.

## Electrical placement results

All cells use identical supplied support, causal120Hz high-pass, permission1
and unity common SoundCheck scalar—not inferred Apple metadata.

| Cell | Rear gain | Independent removal fraction |
| --- | ---: | ---: |
| control025 | .25 | 0 |
| rear040 | .40 | 0 |
| move035 | .40 | .35 |
| move070 | .40 | .70 |

All14 zero-removal C1 controls matched all eight output fields byte-exactly.
All14 rear040-versus-removal pairs had identical realized rear PCM,
rear-scale and linked-gain traces.

Below, F/F0 is known-source three-front-bus power versus its original front-only
power; R/F is two-rear-bus versus front power, with observed gains fixed.
These are electrical accounting ratios, not acoustic localization or loudness.

| Candidate; scored seconds | rear040 F/F0 | move035 F/F0 | move070 F/F0 | move070 R/F |
| --- | ---: | ---: | ---: | ---: |
| Scream backing;60–76 | 1.02635 | .46615 | .14361 | 4.20082 |
| Real hi-hat;60–76 | 1.03075 | .45418 | .12222 | 4.65748 |
| We're On Again negative;60–76 | unknown | unknown | unknown | unknown |
| Enouement backing;168–184 | 1.02151 | .45846 | .13427 | 5.04503 |
| Stolen Car pad;161–177 | 1.03035 | .46067 | .13268 | 4.24178 |
| Lifes Changes guitar;25–41 | 1.01963 | .48003 | .16935 | 3.91645 |
| Feel Low auxiliary percussion;5–21 | 1.03020 | .45297 | .12063 | 4.79342 |

Six positives satisfy unchanged source-MSE≥1e-5; raw/eligible placement ratios
agree. Negative source power is exactly0; ratios remain null, never a success.
Rear-reference channel-band eligibility: five positives8/8, guitar3/8,
negative0/8 per cell; total172 eligible/52 unknown. These are **not quality gates**.

## Safety and remaining uncertainty

All28 core gain traces were unity. Maximum PCM/bus sample peaks:
.7067462802/.6882244317; no full-scale exceedances; C/LFE PCM slots zero.
Each renderer block checked S16 rails; saved audition files are FLOAT.
No intersample/acoustic safety claim follows.

Protected-group counterfactual rear MSE=0 throughout; maximum front-LR/center
difference2.3673e-8/2.9873e-8 is float32-scale. This is supplied-source response,
not learned vocal rejection. Maximum positive0–200Hz spectral excess:
3.4538e-7, descriptive—not proof cabin bass is solved.

Outputs include raw PCM6 and attenuation-only electrical-power-matched
front/rear stereo pairs, not LUFS/acoustic matching. Only high-passed support
is removed. No listening, role predictor, training, native export or deployment
occurred. Perceptual quality remains null; semantic/acoustic/car approval false.

## SHA-256 bindings

Other runtime/test/case/WAV hashes are retained in the bound stage and reports.

| Artifact | SHA-256 |
| --- | --- |
| Catalog manifest | `ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d` |
| Selection v2 | `7dff438a0e36dfa15d4b84a31919ec5fc0dca7f9063cd8220b13b063300ac0a2` |
| OWNER/stage-manifest.json | `1daf62e49292cbde1285c76da52e5d448eba7f583ebafd030d887fe9311d861c` |
| FIXTURE/manifest.json | `84338115b5bad6d03db2a90d56431d92adb6d344585e7b4b23a8a74761e1233a` |
| OWNER/comparison/plan.json | `97b81ff0d65b094596c821bf4bc99f1e7b5240fff0bca0afec427bf2b53ae805` |
| OWNER/comparison/report.json | `ec4405399ff7248828b558c7508de2132fcadccaf86282c61259ce4d526cb4f9` |
| OWNER/readback.json | `837c6a2d28e8e32eaa37b9a59a5d03ee48fba9f003663a3c499094c6c12ccd6d` |
| OWNER/code/run_c2_scene_reference.py | `84404fb2a174d092c69a5b1a2408bb2ac062eca8c7f01ed865497613f15d2e3c` |
| OWNER/code/c2_scene_renderer.py | `70b724cfbb26632a5c91da4c8ffdbe7b3de11fa54b08901d5322eff775ebd880` |
