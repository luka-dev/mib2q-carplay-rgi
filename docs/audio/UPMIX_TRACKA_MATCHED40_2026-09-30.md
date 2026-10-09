# Current Track A on the matched full40 harness

> **Retired 2026-10-09 with the Universal backend.** The renderer, models and knobs this note describes left the tree; the last tree that has them is git tag `upmix-universal-last-2026-10-09`.
> What runs now: [Stereo to 5.1 Upmix](STEREO_TO_5_1_UPMIX.md). Why, and with which numbers: [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md), [Upmix Code Retirement](UPMIX_CODE_RETIREMENT_2026-10-09.md).
> The LFE, amp and measurement facts below remain valid.

_2026-09-30. CPU host replay of the three deployed models, with current startup settings. Same inspected DEV40×60s as the causal/SIMO comparison; not an independent holdout or live in-car result._

## 📊 Result

| Metric | Current Track A | Goal floor/ceiling |
| --- | ---: | ---: |
| Guitar rear share |.164959|≥.15|
| Keys rear share |.152255|≥.15|
| Piano rear share |.095307|≥.10|
| Strings rear share |.069139|≥.14|
| Voice rear share |.075647|≤.02|
| Bass rear share |.149259|≤.06|
| Drums rear share |.296405|≤.08|
| Any-stem swing songs |29/40|≤5|
| Guitar depth/6dB |.005689|absolute≤.05|
| Keys depth/6dB |.011185|absolute≤.05|
| Winds depth/6dB |.016019|absolute≤.05|
| Median rear RMS |−34.518dBFS|reported, not an instrument-clarity score|

Coverage: all40expected songs and231expected stem rows. Five of11 observed render gates pass. No source-attributed entry/exit, device deadline, untouched20 or owner listening verdict was measured in this baseline. Rear fractions are measured in each stem's dominated TF cells above300Hz, excluding LFE; they are not percentages of an entire song or a localization score.

## ⚙️ Frozen conditions and evidence

[Complete result](UPMIX_TRACKA_MATCHED40_2026-09-30.json), [protocol](UPMIX_TRACKA_MATCHED40_2026-09-30_protocol.json), [all stem rows](UPMIX_TRACKA_MATCHED40_2026-09-30_stems.csv). Executed tool: `eval_tracka_matched40.py` (retired 2026-10-09, tag `upmix-universal-last-2026-10-09`); PC root `D:\upmix\runs\tracka_matched40_2026_09_30`.

- Three frozen blobs: q6d, V11 guard, v21a mask; renderer binary SHA256 `cd1025fc080ed066f6c6174c66d39b9622ad95f1e5d3a7e095aab5c8b914236a`. Full blob/helper hashes and CLI are in the protocol.
- Mask drive1.5, vocal gate.5, rear-air−9dB, surround.40, front retention.5, front bass−3dB, bed delays enabled, attack off, LFE assist0dB, mask treble0/gamma1. These reproduce current repository startup knobs, including−9dB air.
- Natural original60s excerpts; input gain `.7 / dry_mix_peak`; no oracle/rear-audio injection, no test-split read, no gain search. Output peak/level handling stays in the existing C renderer.
- Protocol SHA256 `858b86f37160205ed076c224931d04d16a76dd2fc0fa8acfaab310dca2616322`; CSV SHA256 `6d877d5b7e08c07edc61bd0f72ab5133f48e8df5485941bdd1a21ae78bbac2d2`.

The earlier failure-enriched20 mask screen used the helper's−12dB air, and different coverage; its15/20 swings and.225strings median are historical subset measurements. They must not stand in for this full40 current-settings baseline. Same input gain does not establish matched programme loudness for a blind listening test. This host runs synchronous scalar inference; actual async/int8 scheduling and cabin behavior remain separate device/ear checks. No deployment file, hook code or unit state was changed.
