# C2: separate sustained-source reference selection

2026-09-15 UTC. **Frozen TRAIN selection, not a built fixture, musical-role
approval, listening result, learned separator or vehicle approval.**

[c2_reference_selections.json](../tools/stereo-upmix-render/c2_reference_selections.json)
is a six-row list directly accepted by the unchanged `c1_role_fixture.build_fixture`.
The caller must use a new output owner. This selection does not change C1 code,
its original six cases, their recorded activity, or any comparison result.
No fixture or NAS output was written while preparing this selection.

## Selected passages

All signals retain native stereo 44,100Hz, with no resampling, independent
source gain, alignment shift or padding. Every prepared passage is 18s:
2s actual past followed by 16s scored audio. Source amplitudes are unchanged.

| New case | Scored interval | Source interval including pre-roll | Origin |
| --- | --- | --- | --- |
| `c2-scream-backing` | 60–76s | 58–76s | Unchanged C1 backing candidate control |
| `c2-real-hihat` | 60–76s | 58–76s | Unchanged C1 hi-hat candidate control |
| `c2-were-on-again-front-negative` | 60–76s | 58–76s | Unchanged intentional zero-support control |
| `c2-enouement-backing-sustained` | 168–184s | 166–184s | New sustained backing-candidate coverage |
| `c2-stolen-car-pad-sustained` | 161–177s | 159–177s | New sustained pad-candidate coverage |
| `c2-lifes-changes-guitar-sustained` | 25–41s | 23–41s | New guitar-only candidate; pad excluded from support |

This is 96 scored seconds plus 12s pre-roll. The three unchanged controls were
not selected or certified by the new sustained-activity rule. In particular,
the intentional no-support case cannot establish useful rear material.

All six songs retain their original Moises **TRAIN** membership. No DEV/held-out
PCM was opened or used for selection. These are development references chosen
using TRAIN source activity, not an untouched evaluation set or proof of genre
independence. Enouement is also a historically used H1 song.

## Declared sustained-activity criterion

For each specified individual candidate, examine every integer-second core
start `t >= 2` whose entire native `[t-2, t+16)` interval exists. Compute source
MSE over both stereo channels separately for each of the 16 complete one-second
bins `[t+j, t+j+1)`. A bin is active when its unnormalized MSE is at least
`1e-5`. Choose the earliest start satisfying all three conditions:

1. Candidate is active in at least 8/16 bins.
2. The **lead-only** reference is active in at least 4/16 bins.
3. Candidate and lead-only are active together in at least 4/16 bins.

Lead-only is the sum of catalog `lead` stems, not the larger lead+foundation
protection group. This scan retains the original stored signals and does not
claim that the metadata establishes every audible foreground/wet source.
Whole-core MSE is reported separately; it is not an additional selection gate
under this bin-based rule. All three selected positives also happen to exceed
`1e-5` in whole-core candidate MSE. No threshold was lowered to admit the pad
that did not qualify.

| Source | Tested integer starts | Joint qualifying starts | Selected candidate / lead / overlapping active bins | Candidate / lead-only core MSE |
| --- | --- | ---: | --- | --- |
| Enouement backing | 2–224 inclusive: 223 | 17 | 8 / 13 / 5 | `2.0875447776344362e-4` / `1.4610610812775806e-3` |
| Stolen Car pad | 2–186 inclusive: 185 | 26 | 8 / 14 / 6 | `2.7884421875303598e-5` / `4.894163377841235e-4` |
| Lifes Changes guitar | 2–190 inclusive: 189 | 64 | 8 / 16 / 8 | `1.2846320028986566e-5` / `1.4896091601610787e-3` |
| Lifes Changes pad, examined separately | 2–190 inclusive: 189 | **0** | At most 6 candidate-active bins in any window | Not selected |

There were 786 candidate-window observations across four source candidates;
the two Lifes Changes candidates share a song, so these are not 786 independent
musical examples. Every sustained-support-eligible window in this particular
scan also met the lead-overlap criteria (17/26/64/0). No separate no-vocal-only
fallback was needed. The selected Stolen Car window still contains a pad
entrance; the criterion requires eight active seconds, not sixteen continuous
seconds of sound. Energy overlap is not proof of backing/secondary role.

## Scan provenance and preserved negative observations

Private NAS root:

```text
B = /mnt/shared/users/luka/Audi/upmix-training-2026-09-11
CORPUS = B/external/moisesdb/moisesdb_v0.1
```

Inputs were restricted to the 21 exact original files listed below, already
referenced by the frozen C1 fixture. Full source bodies were freshly SHA-256
checked against its original-stem records before decoding; signatures were
checked during reads and again at scan completion. All 21 matched and remained
unchanged. The sustained scan completed in 5.373820593988057s. Its output was
terminal JSON only: no PCM, curve cache, fixture or other NAS file was written.

Bindings:

- Existing C1 `B/external/c1-role-fixture-a/manifest.json` SHA-256:
  `39e00d26f492e02a407315465bbf392fd6384787690a4a40633debcabf978d22`.
- Historical source catalog
  `B/project/build/reference-audio/v16-moises-catalog-sha-a/manifest.json`
  SHA-256: `ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d`.
- The existing C1 manifest and catalog retain each individual file's expected
  full SHA, original header, song/artist identity and split. Source IDs and
  relative paths below identify the exact subset used by the read-only scan.

For each row, the exact path is `CORPUS/<song UUID>/<relative file>`.
`P` means included in the conservative protection group, `L` additionally
identifies the lead-only reference, and `S` means a scanned support candidate.
These are computational assignments, not new musical-role approvals.

| Song UUID | Relative file | Assignment |
| --- | --- | --- |
| `fac94d9a-59da-4f83-9027-3eafe082ad16` | `bass/0e820dec-614a-46f3-9d3f-b3b97ac3a202.wav` | P |
| `fac94d9a-59da-4f83-9027-3eafe082ad16` | `vocals/ccec0f21-0adb-4491-877c-46bf7bc58aec.wav` | P,L |
| `fac94d9a-59da-4f83-9027-3eafe082ad16` | `drums/9ca89ef2-5160-48ef-a259-0e39f1ceb40e.wav` | P |
| `fac94d9a-59da-4f83-9027-3eafe082ad16` | `vocals/e2a2ed92-c1e7-4b2e-8752-bafc9587127f.wav` | S |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `bass/ed79ec4c-5a1e-495d-9bfb-dc8ff9cad3df.wav` | P |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `bass/02e4e949-9ab3-4823-ad2c-bb128252fc19.wav` | P |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `bass/ed462280-9679-41b6-bffa-2878155ba679.wav` | P |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `bass/7b40509e-03e5-42d5-8022-d63f5f130244.wav` | P |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `bass/72e687a2-0f28-4396-abb5-becca6af1443.wav` | P |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `vocals/83e618cd-18c6-43b7-a9a3-6593ea36dedc.wav` | P,L |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `drums/bf98e41c-c959-4ece-8c85-f3529e658b78.wav` | P |
| `28748b6e-6125-42f7-998d-2ad734e39b6c` | `other_keys/27281734-bc48-4c07-bdce-ad3d719c5b47.wav` | S |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `bass/bc0b0d9d-c17f-4e8d-99f5-6e9c0e457938.wav` | P |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `vocals/c885ebf9-28e0-4def-ba7b-1fcc53f5c61b.wav` | P,L |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `drums/2c9d4c27-55d1-4e49-bc4b-b218e3e049f1.wav` | P |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `other_keys/0b9ccadb-edfc-4484-a50c-5769b5918342.wav` | P |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `other_keys/35cccd51-dd45-4352-934a-ad24956abe50.wav` | P |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `other_keys/878895d1-bc6a-4b6b-8cd9-50b4f6a5c0c4.wav` | P |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `other_keys/7a06b910-c50f-4a7b-9176-2f2a6ee6edb3.wav` | P |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `guitar/b538ff33-47b6-438b-99bb-e5f8fcd48b4a.wav` | S |
| `dbb07bdc-2706-4e67-8b59-43f98cf1608a` | `other_keys/c97d654a-0105-42a1-abf3-810d76410eb9.wav` | S, not selected |

The diagnostic one-second curve SHA-256 hashes use little-endian float64 rows
`[integer second, candidate MSE, lead-only MSE, protected-group MSE]` over all
complete seconds in the common available extent. They bind the reported scan
observations; no curve file was saved and they are not an independent audit:

- Enouement backing: `eca5a73fc49414adedffa0142680a8d3b2c6e82df8206fb20248b43f18fa25f9`.
- Stolen Car pad: `efd560bb1d2d3039f9204b8ccaacc7416b8c8f908aad26eef885920e1fcae9ba`.
- Lifes Changes guitar: `6e1b10772b8e654bf7be50cc09f371e3c115a6bff08f57ddbba1b41e2c653c31`.
- Lifes Changes pad: `f7c20ec4db77286d14b048fe7d6f971f3f193ba56b66c63bea04d6a58c211fe3`.

An earlier read-only scan of the same 21 files used a whole-16s mean criterion:
candidate and protected-group MSE each at least `1e-5`. Its earliest Enouement
154–170s and Stolen Car33–49s windows had only one candidate-active second;
the latter had exactly zero lead-only signal. Lifes Changes guitar first met
that rule at22–38s. Those observations were retained, not called sustained
references. The stricter temporal/lead-overlap protocol was specified before
the second scan; it is a different exploratory TRAIN-selection rule, not a
retrospective redefinition of a C1 quality gate. The Lifes Changes pad failed
both rules (maximum whole-core MSE `9.111083670565567e-6`; maximum six active
one-second bins), so it is not amplified or silently added to this selection.

Original C1 60–76s observations remain valid: Enouement backing is exactly zero;
Stolen Car pad MSE is `4.5385783161643075e-9`; Lifes Changes guitar and pad are
individually below the whole-core domain, although their combined support is
above it. C2 does not delete or relabel those negative/quiet observations.

## Role and renderer limits

Every selection remains `role_status=candidate`, with no listening or musical
review claimed. Background-vocal, guitar and pad labels are not sufficient
permission for rear placement. Wet-lead completeness, primary versus supporting
instrument role and coherent in-car placement still require review. In the
guitar-only case, the unselected pad and all other unselected stems remain in
the front/unknown accounting group; they are not an automatic ambience target.

The unchanged fixture builder protects catalog lead+foundation sources and
preserves source-sum accounting. That does not certify all foreground identities.
The recorded lead-only overlap criterion is distinct from the larger protected
group used by the renderer's counterfactual. A future build must freshly verify
all original stems needed for its mixture, not only these 21 scanned files.
No model training, placement approval, native compatibility or vehicle action
is authorized or implied by this selection artifact.

## Appendix: seven-case v2 adds auxiliary percussion, without rewriting six cases

The user explicitly reiterated that the distinct rear musical stage is not
vocal-only: supporting synths, guitars, hi-hats and auxiliary percussion such
as shakers/rattles also matter. The original six-row JSON remains immutable
(SHA-256 `7a612f119cb1c1a2c2d43dd6a73f61627d0caeab11b726a949a464bfe9f0e4b4`).
The new [c2_reference_selections_v2.json](../tools/stereo-upmix-render/c2_reference_selections_v2.json)
contains those exact six row objects, unchanged and in the same order, followed
by **one** auxiliary-percussion candidate. It is a direct seven-row selection
list for the unchanged C1 builder, not a model or an approved fixture.

The extra case `c2-feel-low-aux-percussion-sustained` is **Feel Low — Rivum**,
scored 5–21s with real 3–5s pre-roll at native 44,100Hz. Its selection numbers
are `start_sample=132300`, `frames=793800`, `preroll_frames=88200`.
The new list therefore requests 112 scored seconds plus 14s pre-roll.

A bounded metadata query of nonquarantined TRAIN songs with a labelled lead
found 88 individual `percussion` sources with metadata `has_bleed=false` and
`isolated_source_candidate=true`: 75 generic a-tonal and 13 pitched percussion.
These are source-file counts, not independent songs or semantic approvals.
The inspected subtype is
`a-tonal_percussion_(claps,_shakers,_congas,_cowbell_etc)`; neither the catalog
nor the two inspected original track metadata records identifies a specific
shaker, tambourine or rattle. This is an **auxiliary-percussion candidate**,
not a renamed full-drums stem and not proof of the exact instrument.

Two candidates on new artists were fixed before PCM inspection: Seattle —
The Unfortunates and Feel Low — Rivum. Only their two auxiliary-percussion WAVs
and two lead-only WAVs were decoded. Their four complete source SHA-256s and
both original song metadata SHA-256 bindings were checked against the pinned
catalog above; all signatures remained unchanged. The scan completed in
2.2399344610166736s, with terminal JSON only and no NAS writes.

| Candidate | Tested integer starts | Joint eligible starts | Earliest selected interval | Active candidate / lead / overlap bins |
| --- | --- | ---: | --- | --- |
| Feel Low auxiliary percussion | 2–238 inclusive: 237 | 208 | 5–21s | 8 / 7 / 7 |
| Seattle auxiliary percussion | 2–240 inclusive: 239 | **0** | None | Maximum zero candidate-active bins at `1e-5` |

The same predeclared 8/4/4 rule and `1e-5` one-second MSE threshold were used.
Feel Low's selected candidate core MSE is `1.3839648844433624e-4`; lead-only
core MSE is `9.369793056162295e-4`. The passage contains an entrance: eight
candidate-active seconds, not sixteen uninterrupted seconds. Seattle's failure
is preserved; lack of above-threshold bins is **not** a claim of exact silence.
It was not substituted silently or amplified to qualify, and it is not added
to the seven-case selection.

Exact newly scanned source paths relative to `CORPUS`, with full source SHA-256:

| Source | Relative path | SHA-256 |
| --- | --- | --- |
| Feel Low auxiliary percussion | `13f233aa-a2e5-4683-8533-2f1e344b55b4/percussion/7c781ac0-5df8-4b5d-8b82-e36b46ad5fca.wav` | `607810c01ca473aa5d091a8c0ce68ee07cca34dc4ace9e3addc07d89b8f32f02` |
| Feel Low lead-only | `13f233aa-a2e5-4683-8533-2f1e344b55b4/vocals/88f864ea-a555-4e85-84bc-9cf1f891815b.wav` | `e6d228489161c3d2c5efeed30eadfbf608af45ccfa55b46d462fa714403d43ff` |
| Seattle auxiliary percussion | `07c17c22-a321-4a98-a486-ef7ee1bae50e/percussion/c0e672d6-0f99-495d-b0fb-5dc5c12c23e7.wav` | `a9b37a781266f11972a31db8133f2ffbc4c0a4ac38b4fb732ecbad46c810871c` |
| Seattle lead-only | `07c17c22-a321-4a98-a486-ef7ee1bae50e/vocals/4e17ce06-b9d7-4f8f-97d4-2d35498df8f6.wav` | `56e4e284b77c7114a9a99779aa0f91ce0d8da6af97756855cbfd1be98a7226d4` |

The corresponding original metadata paths are
`13f233aa-a2e5-4683-8533-2f1e344b55b4/data.json` and
`07c17c22-a321-4a98-a486-ef7ee1bae50e/data.json`. Their full metadata SHA-256s
were verified against `songs[id].metadata_sha256` in catalog
`ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d`.
The literal metadata digest strings were not printed in the scan's terminal
output; the bound catalog is the reference rather than an invented digest.
The future builder independently rechecks and records the literal hashes.

Diagnostic whole-second curves `[integer second, candidate MSE, lead-only MSE]`
in little-endian float64 had hashes:

- Feel Low: `ae16ee77ef47c96db43d77ec89c3c74e93e52f6c2b26ac37ceb85190f9a1f51e`.
- Seattle: `f3687c89d2ea881a542252662f84fbf4a40f3f4f18f4fbe8da90a60a03007772`.

No curve file was saved. Metadata absence of bleed is not independently
confirmed acoustic/source purity, and energetic lead overlap is not musical
support-role approval. All seven cases retain `role_status=candidate`.
Source-role listening, exact auxiliary instrument identity, wet protection,
genre coverage and vehicle suitability remain unverified. No C1 or six-case
C2 result is relabelled by the addition.
