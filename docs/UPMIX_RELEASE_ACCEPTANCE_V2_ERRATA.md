# Release acceptance v2: prospective errata

2026-09-16, before real Q2 fit or candidate release metrics. This supersedes the
v1 evaluator for future studies; it does not reinterpret an exposed final TEST,
rescore old failures, or edit `output/upmix-release-protocol-v1`. All38 gate IDs,
thresholds, comparison directions, scopes and numeric minimum counts are unchanged.
The self-contained specification is [v2](UPMIX_RELEASE_ACCEPTANCE_V2.md), with
[machine protocol](../tools/stereo-upmix-render/release_acceptance_v2.json).

## Demonstrated defects and repairs

Read-only synthetic v1 counterexamples returned PASS for negative silence peak,
negative native absolute error, -2.5 missed deadlines, negative callback period
ratio and Wilson lower bound2.0. A non-timestamp string and an unmaterialized
domain hash also passed with an unrelated valid artifact. These were validator
counterexamples, not real model measurements. V2 rejects them.

- Numeric domains are explicit per gate. Error magnitudes, ratios, time/memory
  and counts cannot be negative; coverage and probability bounds are[0,1].
  Missed deadlines/underruns and S16 LSB maximum are integers. dB recovery/leakage
  and retained amplitude remain signed where their definitions require it.
- A measured report references a listed, hash-verified domain-manifest body.
  Its scope/protocol/candidate and gate-specific count unit/eligible IDs must
  match. IDs are unique, eligible and ineligible sets disjoint, and the report's
  IDs/count exactly reproduce the manifest. Repeated frames are not songs.
- Every TARGET report must bind the same loaded-cohort domain SHA, including
  actual APQ8064/Krait/QNX identity, non-emulated status, native build/workload
  artifact hashes, sample format/channel order, rate and callback period.
  Unloaded P99 cannot be combined with a different two-hour stress run. Both
  LISTENING gates likewise share one domain manifest.
- UTC timestamps are parsed and cannot be future-dated. Candidate performance
  must follow protocol freeze. Only AUDIT may reuse an earlier measurement,
  with an explicit post-freeze source-body reverification timestamp/reason.
- Duplicate JSON keys/non-object reports fail; missing/blocked evidence remains
  non-passing. No vacuous release with only acoustic gates. The evaluator itself
  is copied and SHA-bound at every new seal. Hashes are not digital signatures.

## Exact dB boundary cases, not arbitrary floors

For declared dB gates, report either an ordinary finite value or `value:null`
with `exact_log_ratio:{numerator,denominator,multiplier}`. Components are finite,
nonnegative and multiplier is the gate's10(power) or20(amplitude). A positive
ordinary denominator is expected; exact boundary cases retain their meaning:

| Ratio | Signed result | Qualification meaning |
|---|---|---|
|0 / positive|negative infinity|Zero leakage passes; zero support fails its lower bound |
|positive /0|positive infinity|Perfect candidate versus nonzero baseline error can pass recovery; leakage fails |
|0 /0|unknown|UNVERIFIED, not a quality pass |
|0 /0 for recovery only|0dB|Both already perfect: no improvement, so the3dB recovery gate fails |

Absolute-error dB gates apply absolute value **after** the logarithm: zero bass
or front transfer is infinite error, not negative-infinity success. Finite value
and supplied ratio must agree (1e-12 representation consistency, no gate-margin
relaxation). JSON emits null plus an `exact_case`, never NaN/Infinity. Signed
coherent support/front transfer requires `coherent_sign` in{-1,0,1}; nonpositive
transfer is a known failure, not gain after taking an absolute coefficient.
Zero amplitude and its sign must agree.

## Listening count ambiguity resolved before scoring

The numeric minimum24 now explicitly means **24 distinct song-cluster verdicts**,
not24 clicks over12 songs. This strengthens, not weakens, the previous minimum.
Alternate recordings/excerpts/repeats of one song remain one trial. Ties are
nonpreference; actual listener IDs/count and personal-acceptance ownership are
reported separately. There is no population claim. Wilson uses the independent
song-verdict successes/trials; repeated/order checks are diagnostics, not extraN.
This is not permission to relabel exposed material as untouched TEST.

## Preservation, APIs and residual boundaries

Exact v1 evaluator bytes were preserved at
[`output/upmix-release-v1-evaluator-preservation-a/release_acceptance.py`](../output/upmix-release-v1-evaluator-preservation-a/release_acceptance.py),
SHA `9041f1ff80cac0108cf2a64eac52ce96506ebc4d421091aa8bd7a1f468bf4439`,
matching the old freeze. V1 JSON/specification/freeze are unchanged.

`seal(protocol_path,specification_path,output)`, `load_freeze(owner)` and
`evaluate(freeze_owner,evidence_owner,candidate_sha256)` retain their signatures.
New schemas are `upmix-release-protocol-v2`, `upmix-release-freeze-v2`,
`upmix-release-gate-evidence-v2`, `upmix-release-domain-manifest-v2`; evaluator ID
is `upmix-release-acceptance-evaluator-v2`. V1 is deliberately not accepted by v2.

The ledger does not calculate source metrics, inspect scientific truth in every
artifact, infer source roles, authenticate hardware or conduct listening. Valid
shapes/hashes do not prove those claims. Exact deterministic producers, scoring
interval/coherence/onset policies and coverage audits must be separately frozen
before their candidate scoring; missing producers remain UNVERIFIED. There is
still no model or installation approval from this repair.
