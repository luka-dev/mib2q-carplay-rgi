# Upmix Causal Role-Focus Probe

_2026-09-28. Track B offline mechanism test; no head-unit, Track A, device int8 or listening result._

## Single-change hypothesis

The [four-role probe](UPMIX_CAUSAL_ROLE_PROBE_2026-09-28.md) found that the true-stem four-role power mask passes all measured DEV40 real-C gates, while the trained causal role model emits eligible probability .495 on cells whose target mean is .920. Across all time/frequency cells, 6.1% are both audible and ≥.8 eligible-owned, versus 17.4% confident vocal and 27.1% confident drum cells under the same audibility filter. Sharpening its logits increased instrument transmission but left vocal/eligible transmission near the original frontier. This test adds **one training term**: equal-role cross-entropy on ≥.8 source-dominant, audible cells. The ordinary mixed-cell role CE and audio reconstruction losses remain unchanged. The hypothesis is that explicitly weighting rare pure eligible cells improves discrimination, rather than simply opening the mask.

The control is the frozen `causal_role4k/role_softmax/step4000.pt`. The focused arm uses the same causal four-role architecture (586,916 parameters, **125 M estimated neural MAC/s** before STFT/renderer), seed, 176 true training songs, 774 owner-library pseudo-labelled songs, half/half batches, 8-second crops, batch 12, 4k steps, 3e-4 LR and 2k warm-up. Both keep `--role-ce .279254`; only the focused arm adds `--role-focus .126919`. Four fixed CPU batches gave baseline/focus parameter-gradient ratios **.488/.221/1.004/.358**, where baseline includes the original audio and role CE terms; .126919 makes focus approximately **30% of the median baseline gradient norm** at initialization. This weight was fixed before DEV performance. Source SHA-256 for the runner guard: `14f362c41525620bd53b8c255db1447d57c0897b0dc84228ee598d741230f23f`.

## Frozen decision rule

At 4k, first use the existing fixed 12-song, reanalyzed source-attribution diagnostic. Eligible transmission must be at least **.432** (shared-mask anchor .462 minus .03); the per-song median vocal/eligible, drum/eligible and bass/eligible transmission ratios must each be at most **80%** of that same shared-mask anchor. The fixed 20-song transition diagnostic must reach vocal-entry retention median ≥.85 and p10 ≥.60. Also report eligible output on true ≥.8 eligible cells and on confident vocal cells, compared with the unfocused four-role control, to distinguish more confident role prediction from a gain change.

Only if the source and transition gates pass, run the enriched DEV20 real host C screen, level-match candidate and control rear RMS to within ±.1 dB with the same one-gain method as review 20, then require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new swing song before considering a matched longer train/full40 student screen. None of these DEV diagnostics replace independent real stems, on-device int8 ≤150 M MAC/s, ≤85 ms extra latency, Track A port or the owner's deferred blind CarPlay listening.

## Results and decision

The focused 4k run completed cleanly in 885 seconds. Its step-4000 scalar DEV SI-SDR/vocal-leak score was **−1.66 dB/.082**, versus **−1.35 dB/.087** for the unfocused four-role control; as in prior runs these scalars alone cannot decide the renderer goal. The fixed 12-song source diagnostic did:

| Reanalyzed median transmission or per-song ratio | Shared-mask anchor | Four-role control | Focused role model |
| --- | ---: | ---: | ---: |
| Eligible | .4624 | .3505 | .3372 |
| Vocal | .1563 | .1098 | .1058 |
| Drums/percussion | .0270 | .0280 | .0247 |
| Bass | .0154 | .0150 | .0102 |
| Vocal / eligible | .4050 | .4352 | .4390 |
| Drums / eligible | .0710 | .0633 | .0558 |
| Bass / eligible | .0424 | .0434 | .0350 |

Against the shared-mask anchor, the focused model fails eligible retention (.337 < .432), vocal/eligible (.439 > .324), and bass/eligible (.0350 > .0339); only drums/eligible meets the frozen 80% requirement. On the fixed 43 vocal entries, median/p10 eligible post/pre retention **.786/.520** misses both .85/.60 targets and is worse than the unfocused role model's **.788/.539**. The role calibration confirms the loss did change a class, but the wrong one for this objective: on confident bass cells the predicted own-role mean rose **.609→.794**, while on confident eligible cells it stayed **.495→.490** against the target .920. On confident vocal cells predicted eligible rose **.200→.207** against the target .028.

This one-change training term did not fix the rare-eligible confidence gap; it closed the eligible mask further while improving bass identification. Under the precommitted decision rule, **no C screen, 10k extension or full40 student test follows** for this checkpoint. The next diagnostic is to compare confident-cell outputs on real training songs with DEV songs, to distinguish an optimisation/capacity failure already present on seen material from a generalisation or pseudo-label problem. That will remain diagnostic, not independent validation or permission to ignore the failed gates.

That diagnostic found the eligible confidence gap in all three sampled domains. On 12 real training / 12 DEV / 12 pseudo-labelled owner songs, the unfocused model's mean predicted eligible probability on audible ≥.8 eligible-owned cells was **.391/.495/.443**, against target means **.923/.920/.934**. The focused model gave **.390/.490/.448**. It improved confident bass prediction on train/DEV/pseudo (**.481→.620/.609→.794/.593→.773**) but did not lift eligible confidence. The real-train excerpts were deterministically selected for diagnosis, not necessarily exact crops sampled during training; this weakens any claim about memorisation. The same broad underconfidence across domains weakens a pure real-vs-pseudo domain-shift explanation. On confident eligible versus confident protected cells, the unfocused eligible score had median per-song AUC **.857 on train** and **.892 on DEV**; the focused score had **.847/.880**. This ranking test ignores mixed-cell costs and cell energy, so its high AUC does not establish a useful rear operating point.

True-stem energy attribution on the fixed DEV12 shows median **62.9%** of eligible energy in ≥.8 eligible-owned audible cells, **30.8%** in mixed audible cells and **5.4%** in protected-dominant cells. For vocal energy, the analogous pure-eligible/mixed/pure-protected medians are **1.5%/20.7%/77.9%**. Thus the focus term addressed substantial eligible energy, but mixed cells still carry a material amount of both instrument and vocal energy.

An **exploratory hard-threshold ranking bound** (`p_eligible ≥ t` at fixed global t, no per-song oracle threshold) shows the score contains some usable ordering that the soft probability does not exploit. At `t=.55`, the focused model's fixed-DEV12 eligible transmission was **.485**, with vocal/drum/bass per-song median transmission ratios to eligible **.326/.041/.018**, compared with the original shared-mask anchor **.405/.071/.042**. The vocal ratio narrowly misses the preselected ≤.324 source gate; `t=.65` reaches **.289** but lowers eligible transmission to **.368**, below the .432 floor. These thresholds were inspected after the model result; a binary time/frequency mask can also cause musical noise and unstable ownership. They are a diagnostic upper bound on the frozen score, **not** a deployable candidate and not grounds to run the already-rejected raw checkpoint through C.

The next architecture test should preselect a **smooth, causal confidence mapping** and check source ratios, vocal-entry behavior, real C stability and matched rear level together. It should preserve the distinction between rank, output calibration and actual extracted audio. The current focus-loss hypothesis is closed as a negative result.

PC artifacts: `D:\upmix\runs\causal_role_focus4k\focused\step4000.pt`, `focus_vs_shared.json`, `focus_vs_role.json`, `transitions.txt`, `calibration_{role,focused}_{train,dev,pseudo}.txt`, `overlap_energy.txt` and `operating_point_{role,focused}.txt`. Checkpoint SHA-256: `0434C9E584427B8101148C68F11E31CDD97AB8A2152A741FE61B313EEB90E277`. [`run_causal_role_focus4k.ps1`](RETIRED_TOOLS.md#tool-e0615e9e41d7) enforces the single GPU lease and source hash above. GPU lease and rapid queue were empty after the diagnostics.
