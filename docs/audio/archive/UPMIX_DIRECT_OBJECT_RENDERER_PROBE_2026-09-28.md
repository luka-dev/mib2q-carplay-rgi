# Upmix Direct-Object Renderer Probe

_2026-09-28. Host-only, offline architecture experiment with true stem audio, ideal bounded-mask audio, and a frozen sep9 estimate. No head-unit file changed and no listening verdict exists._

## Hypothesis and test boundary

The [architecture gate](UPMIX_ARCHITECTURE_GATE_2026-09-28.md) isolated rear-side depth variation in the original C renderer even with true eligible audio: across the ten original swing traces, rear output energy per owned source energy changed by a median **+6.14 dB**, versus **−2.20 dB** at the front. The test here asks whether a different **renderer object/bed interface**, while holding the oracle input and measurement harness fixed, can reduce full-song motion at useful rear level. It does not test extraction, a deployable mask, in-car acoustics or subjective quality.

[`build_direct_object_host.py`](../../../tools/stereo-upmix-render/build_direct_object_host.py) reads `hook/audio/hssr_dsp.c`, inserts a test-only branch in a copy outside the repository, and cross-compiles three Windows host executables. The control is built from the unchanged source. For injected oracle audio only, the prototype steers the isolated left/right object directly to the rear and uses the same object for front transfer, bypassing the original dynamic pan projection and universal mixture fallback. The `direct_object` arm retains ambience/air; `direct_no_bed` removes them. Neither branch is suitable for the head unit. Because the latter changes both object projection and bed/fallback, its outcome cannot be assigned to any one of those subcomponents.

The source SHA-256 was `d071d8c77c38b810c061ca4812ee6c289b00780ae5b76fcad95acd28e90610d8`. The three host executable hashes are control `f67798d6f254da7f7920ca47643e72e8d0804da0fcda410e71cfc8d13e38a563`, direct object `81b61226bcf3f2dcf17f606acd2a2a065e86cc33c1c7d23831703410f33e6a74`, and direct/no-bed `eaf28ccfef2d3fc30f0ac3bb2fce440ed53308d9144da81d89cff377249c26a7`. The build manifest is in `D:\upmix\tools\direct_object_host\manifest.json`; each PC runner checks these hashes. [`oracle_renderer_sweep.py`](../../../tools/stereo-upmix-render/oracle_renderer_sweep.py) selects the same binaries and explicit point settings for every song. All renders use the real C host renderer, fixed gain within a condition, 60 seconds per song and `CUDA_VISIBLE_DEVICES=-1`. The rapid queue was empty; no GPU training or PC environment install occurred during these tests.

## Enriched 20-song mechanism screen

The frozen screen contains all eight original true-audio swing songs and twelve deterministic nonswing controls. A fresh control render reproduced the saved 115 stem rows within **0.005** in both median rear share and one-second range. The predefined promotion gate required at least three original swing songs resolved, no new swing, rear RMS within ±0.5 dB of the control, and all stem-share and level-slope limits. A swing is a unique song with any stem whose one-second rear-share range exceeds 0.30.

| Variant | Swing songs / 20 | Resolved / new | Guitar / keys / piano / strings | Voice / bass / drums | Rear RMS vs control | Screen verdict |
| --- | ---: | ---: | --- | --- | ---: | --- |
| Original control | 8 | — | .256 / .230 / .171 / .272 | .009 / .012 / .069 | −36.638 dBFS | Reference |
| Direct object, old bed | 8 | 1 / 1 | .301 / .323 / .217 / .393 | .006 / .010 / .038 | +0.564 dB | Reject: new swing, level, resolution |
| Direct object, no bed | 6 | 2 / 0 | .285 / .316 / .214 / .379 | .005 / .010 / .017 | +0.326 dB | Reject: only two resolved |
| Direct/no-bed, rear body 0, surround .40 | 2 | 6 / 0 | .396 / .399 / .395 / .402 | .006 / .011 / .017 | +2.114 dB | Reject: level mismatch |
| Direct/no-bed, rear body 0, surround **.3235** | **0** | **8 / 0** | **.237 / .236 / .246 / .239** | **.004 / .008 / .011** | **+0.270 dB** | **Pass this mechanism screen** |

The old bed leaves the direct-object path moving. Removing it resolves two songs; disabling rear body resolves six but raises rear level. This is an **iteration sequence**, not a controlled factorial attribution of individual C terms. At the baseline-selected ten extreme windows, the direct/no-bed path reduced median rear energy change per unit source energy from **+6.14 to +3.40 dB** and left **4/10** original fixed-window share differences above 0.30, versus **10/10** in the control. These are mechanism traces; the table above is the full-song count.

The .3235 coefficient was determined once from the **twelve nonswing controls**: their paired median rear-RMS increase at surround .40 was +1.843525 dB, giving `.4 × 10^(−1.843525/20) = .323507`. It was rounded to .3235 before the final screen rerender, then frozen for the full-40 test. This is still DEV-informed calibration, and the all-20 screen was repeatedly inspected during architecture design. The passing point must not be described as independent validation. The calibrated screen's guitar/keys/wind level slopes were **+.0122 / +.0134 / +.0122**, within ±.05. PC evidence: `D:\upmix\runs\direct_object_probe20\` (`decision.json`, `body0_decision.json`, `cal_decision.json`, point CSVs and paired JSONs); the matching runners are [`run_direct_object_probe20.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_probe20.ps1), [`run_direct_object_depth_components.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_depth_components.ps1), [`run_direct_object_body_probe20.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_body_probe20.ps1) and [`run_direct_object_cal_probe20.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_cal_probe20.ps1).

## Full DEV confirmation

[`run_direct_object_full40.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_full40.ps1) took the exact calibrated point and binaries to the original 40-song DEV manifest. It verified a fresh control against the saved **231 stem rows** within 0.005 and applied the original full-40 C-render gates. There was no further gain selection.

| True eligible audio, 40 DEV songs | Any-stem swings | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Original C control | **8/40** | .25513 | .22959 | .17104 | .18040 | .00937 | .01333 | .06875 | −37.619 |
| Direct/no-bed/body-0, surround .3235 | **0/40** | **.23286** | **.23475** | **.24198** | **.23137** | **.00377** | **.00836** | **.00967** | **−37.146** |

All eight original swing songs resolved, with **zero new** songs. Rear RMS increased **0.472 dB**, just inside the ±0.5 dB gate; the small margin should be kept visible. Guitar/keys/wind level-following slopes were **+.01263 / +.01322 / +.00496**, all within ±.05. The [full-40 evaluator](../../../tools/stereo-upmix-render/evaluate_oracle_renderer_probe.py) reported **all measured render gates passed**. Its scope is the numerical oracle-audio renderer check, not the complete product goal. PC evidence is `D:\upmix\runs\direct_object_full40\decision.json` and the paired per-song outputs.

All eight original swing songs were deliberately included in the enriched-20 design screen. Full40 therefore checks for new failures among the other 20 songs and the aggregate shares/level; it does **not** independently replicate resolution of those eight songs.

Setting rear body to zero changes the rear spectrum, and removing the bed changes ambience. Matching broadband rear RMS does not show that timbre, envelopment or cabin depth are acceptable. Those are open listening questions, not numerical passes.

## Ideal bounded-mask/audio representation check

[`run_direct_object_irm_full40.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_irm_full40.ps1) reused the **same** binaries and surround .3235 on the original 40 DEV songs, but reconstructed eligible audio with the ideal bounded real magnitude mask (`[0,1]` ratio from true stems applied to the stereo mixture STFT). This is a privileged, noncausal upper bound for that **representation**, not an estimator result. A fresh control reproduced the saved ideal-mask/audio baseline's 231 stem rows within 0.005.

| Ideal bounded-mask audio, 40 DEV songs | Any-stem swings | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Original C control | **7/40** | .23455 | .23464 | .15417 | .16879 | .01237 | .01633 | **.08266** | −37.836 |
| Direct/no-bed/body-0, surround .3235 | **0/40** | **.20501** | **.21465** | **.21258** | **.20842** | **.00442** | **.01124** | **.00905** | **−37.760** |

All seven original swing songs resolved, with zero new songs. Rear RMS rose **0.075 dB**. Guitar/keys/wind level slopes were **+.01701 / +.01938 / +.01602**, and every measured full-40 render gate passed. This overturns an overly broad diagnosis that a real-mask output cannot work: **the ideal bounded mask/audio representation passes with the changed renderer**. It does not show that an on-budget causal network can estimate that mask. The earlier sep9 and shared-mask models remained far below the ideal extraction frontier and were tested through the original renderer, so their existing scores cannot be transferred to this path. PC evidence: `D:\upmix\runs\direct_object_irm_full40\decision.json`, paired rows and C output WAVs.

## Secondary TEST20 and remaining gates

The numerical C-render gate does not include the required source-attributed vocal-entry ducking median/p10. It also cannot certify causal extraction, measured ≤150 M int8 MAC/s, ≤85 ms extra latency, or a blind owner verdict. The existing 20-song MoisesDB TEST split has already been inspected for the previous retention experiment, leaving only four unopened TEST songs; it must be labelled secondary robustness if reused, never a new independent 20-song confirmation. All 40 DEV songs have also steered this work. A new adequately covered set of real stems is needed for an independent architecture gate.

[`run_direct_object_test20.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_test20.ps1) reused the prior TEST20 manifest, checked a fresh C control against its saved baseline within 0.005, and applied the frozen renderer with true eligible audio. It found **5→0/20** any-stem swing songs, zero new, guitar/keys/piano/strings **.23896/.23192/.25154/.20859**, voice/bass/drums **.00264/.00461/.00432**, and rear RMS **+0.490 dB** relative to its control, again close to the ±0.5 dB boundary. Guitar and keys level slopes were **+.00967 / +.00721**; all observed limits passed. The evaluator's formal verdict is **NO** solely because there is **no valid wind slope** in these songs; it treats missing coverage as failure, as it should. Keys have five songs and strings only one, so these family values are weak evidence. This check is **secondary robustness**, not pristine validation. PC evidence: `D:\upmix\runs\direct_object_test20\decision.json`.

[`run_direct_object_irm_test20.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_irm_test20.ps1) applied the unchanged renderer and .3235 coefficient to ideal bounded-mask/audio on the same previously inspected TEST20. It found **4→0/20** swing songs, zero new, guitar/keys/piano/strings **.21555/.20555/.22543/.17705**, voice/bass/drums **.00295/.00631/.00450**, rear RMS **+0.044 dB**, and guitar/keys slopes **+.01225/+.01122**. Every observable limit passed; the formal decision is **NO** for the same missing wind slope. This baseline had no prior saved ideal-mask/audio TEST rows, so the control is a fresh C render without a historical row-by-row reproduction check. PC evidence: `D:\upmix\runs\direct_object_irm_test20\decision.json`. Neither TEST20 result restores the goal's unavailable untouched 20-DEV-song gate.

## Frozen sep9 model through the new route

To measure the remaining extraction gap without another training run, [`run_direct_object_sep9_probe20.ps1`](../../../tools/stereo-upmix-render/pc/run_direct_object_sep9_probe20.ps1) fed the same best sep9 checkpoint, `D:\upmix\runs\sep9\a\sep_net.pt` (SHA-256 `ea312723d9ac327b35472e23337c47376c58a9dca9331765f4b622384c30dac7`), to both host C conditions on the enriched DEV20. [`oracle_renderer_sweep.py`](../../../tools/stereo-upmix-render/oracle_renderer_sweep.py) now supports a frozen `student` source; the runner verified that all **20 injected WAVs were byte-identical** between the C conditions. This separates the renderer comparison from inference variability. No weights changed. Sep9 still uses whole-window normalization and is not causal as written.

| Sep9 estimate, enriched DEV20 | Swing songs | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Original C control | **6/20** | .09896 | .10518 | .09770 | .18339 | **.02508** | **.06684** | **.10028** | −37.333 |
| Direct/no-bed/body-0, surround .3235 | **1/20** | **.10533** | **.14368** | **.08893** | .18826 | .01861 | **.07045** | .01611 | −37.699 |

Five original swing songs resolved and none appeared; rear RMS fell **0.366 dB**, inside the ±.5 dB screen. Voice and drums moved within their limits, while the model still missed **guitar ≥.15, keys ≥.15, piano ≥.10 and bass ≤.06**. Guitar/keys/wind level slopes were **+.01487 / +.02686 / +.03189**, within ±.05. The [student20 evaluator](../../../tools/stereo-upmix-render/evaluate_oracle_renderer_probe.py) therefore rejected full-40 promotion. The changed renderer repairs much of sep9's measured motion but cannot create eligible guitar/piano energy or remove enough bass from its estimate. This is a bounded result for this checkpoint and screen, not proof that further training or another model is impossible. PC evidence: `D:\upmix\runs\direct_object_sep9_probe20\decision.json`, paired rows and injected WAVs.

The owner had suggested a possible keys limit of .12; that change was not adopted before these tests. Even if it were used now, sep9 would still fail guitar, piano and bass on this screen.

The next research step is to build a **causal eligible-audio or real-mask estimator** for this fixed renderer and compare it against both upper bounds, including source-attributed vocal-entry and protected-stem leakage. The loss and checkpoint gate must reward instrument transmission while penalizing bass/voice/drum leakage at matched rear level; the sep9 comparison rules out merely continuing that checkpoint as a complete fix. The model's output must be made available to this object path explicitly; the test-only injection branch is not a port. Any future hook port belongs to Track A, after host evidence. Listening and device measurements remain separate product gates.
