# Cross-Corpus True-Stem Remix Probe

_Track B, 2026-09-29. Candidate sampler and comparison frozen before a PC training run._

## Question and data boundary

The [MoisesDB-only remix](UPMIX_MOISES_REMIX_PROBE_2026-09-29.md), [five-song curated OnAir supplement](UPMIX_ONAIR_DATA_PROBE_2026-09-29.md), and [six-family MoisesDB remix](UPMIX_MOISES_INSTRUMENT_SIDECAR_2026-09-29.md) each failed a matched 4k screen. They did not test **source families crossing corpora**: the current sampler uses MoisesDB donors for a MoisesDB base song, while an OnAir window remains from one OnAir song. This probe asks whether independently combining real MoisesDB and audited OnAir vocal/drum/bass/eligible stems helps at the same marginal corpus exposure. It is separate from review 43's causal, distinct-MoisesDB-song fine-role run.

The only extra true-stem pool is the existing five-song `D:\upmix\datasets\onair_cache7`, whose admission, stem roles and master-alignment checks are in the [OnAir report](UPMIX_ONAIR_DATA_PROBE_2026-09-29.md). All its songs are TRAIN only. The 176 MoisesDB TRAIN caches supply the other true-stem pool; the 774 owner-library teacher estimates remain pseudo labels and are never donors for a true-stem remix. Natural DEV songs stay unmodified. The test cannot answer whether a substantially larger or different corpus would help.

## Frozen single-change comparison

Both arms use the same `sep_net.py` BandSepNet rapid recipe, seed 1, 4k steps, 8 s crops, 50% true / 50% pseudo windows, `--extra-truth-frac 0.25`, `--remix 0.5`, and the same OnAir cache. In the control, only MoisesDB truth windows remix, with all donors from MoisesDB; OnAir truth windows remain same-song. In the candidate, `--cross-corpus-remix` lets each remixed true-stem family independently choose MoisesDB with probability .75 or OnAir with probability .25, including when the vocal/base song is OnAir. The vocal family stays with its base song; drums, bass and eligible `other` get independently drawn song paths and offsets. A donor may equal the base song by chance; this flag does not enforce distinct song identities. Slot gains remain independent ±6 dB. The target is recomputed as the exact sum of the selected eligible source; no mastered mix is forced to match it.

The same marginal true-stem pool selection applies to every family in the candidate. With the frozen half-truth, half-pseudo, .5-remix and .25-OnAir probabilities, roughly **17% of all windows** should contain at least one cross-corpus source family. This is a larger intervention than adding OnAir intact but does not change the student architecture, loss, pseudo pool, held-out songs, or nominal corpus share. Both arms must run under one shared code hash after the current review-43 GPU job and its diagnostic finish.

The sampler flag is in [`sep_net.py`](../../tools/stereo-upmix-render/sep_net.py). A synthetic 2,000-window check with distinguishable MoisesDB/OnAir source markers and `--remix 1.0`, 50% OnAir among true windows found **0%** cross-corpus eligible/vocal pairings in the control and **48.25%** in the candidate, with eligible and vocal OnAir shares **48.0%/49.95%** in the candidate. Maximum `mixture − (eligible + vocal + drums + bass)` error was **2.98e-8** for the candidate. This checks source selection and target closure; it is not a separation result. The flag requires `--extra-truth` and positive `--remix` and rejects the unrelated instrument-sidecar path.

## Decision sequence

Before PC training, freeze both command lines, source/cache/renderer hashes and candidate exposure counts from the actual sampler; use an idle GPU lease and no PC environment installs. Compare the matched 4k checkpoints on the same DEV12 source-attribution, 43 vocal-entry events and fixed-gain real-C DEV20 screen. Require useful eligible or instrument rear-share gain at similar rear RMS **jointly** with no material regression in voice, bass, drums, vocal-entry median/p10, absolute eligible pre/post, or any-stem swings. A +.03 eligible or +.02 family share is only a screening signal; near seed variability, repeat with seed 2. A surviving signal needs matched 10k and then a causal `norm=stream` comparison, because this rapid default uses a noncausal whole-window level. Do not tune a gain or threshold on inspected DEV to rescue a failure.

If this pair fails, preserve its per-song results and stop treating the five-song OnAir pool as a likely way through the current extraction shelf. The product goal still requires a causal candidate through full40 real C at stable gain, a new adequately covered real-stem set, measured device int8 cost and latency, a Track A-owned hook port, and the owner's blind live CarPlay verdict. This training comparison alone cannot satisfy any of those final gates.

## Preflight and smoke after review 43

The causal review-43 cross-song run and diagnostic finished with a failed joint gate; its GPU lease was released. The PC shared `sep_net.py` differed from the committed local copy **only** by this new cross-corpus flag, so the latter was copied and read back at SHA-256 `11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905`. The rapid daemon remains byte-identical to the earlier matched screens (`BE2094DD…`). All 176 MoisesDB TRAIN caches are at least **56.48 s** and all five admitted OnAir TRAIN caches at least **152.46 s**, so an 8 s crop cannot silently skip a donor. A 10,000-window symbolic pass at the frozen .5 truth / .5 remix / .25 OnAir settings produced **50.62% truth windows** and **17.53% of all windows with at least one cross-corpus donor**, with source-sum closure error **4.77e-7**. That symbolic pass used distinguishable source markers; it did not train or measure separation.

The guarded [`run_cross_corpus_smoke.ps1`](../../tools/stereo-upmix-render/pc/run_cross_corpus_smoke.ps1) tested the real Python, cache and GPU path for 20 steps per arm. Both reported **176 MoisesDB true / 774 pseudo / five OnAir** songs and **586,916 parameters / 125 M estimated neural MAC/s**. A first PowerShell wrapper attempt passed the candidate's flag as separate characters and stopped before candidate training; the control reached step 20, released the lease and wrote its checkpoint. The wrapper was corrected to splat a string array, preserve the completed control, and rerun only the candidate. Both arms now have `train_done.txt` and `step20.pt`; no GPU lease remained. These short runs prove execution, not a source or C result.

The exact main rapid queue lines are:

```text
crosscorp_ctrl_s1|--remix 0.5 --extra-truth D:\upmix\datasets\onair_cache7 --extra-truth-frac 0.25
crosscorp_mix_s1|--remix 0.5 --extra-truth D:\upmix\datasets\onair_cache7 --extra-truth-frac 0.25 --cross-corpus-remix
```

The control and candidate will use the same committed code, seed and rapid daemon. Check an empty queue, no running trainer, a free lease and absent tag folders immediately before enqueueing; then hold one GPU tenant through both 4k trainings and diagnostics. A C screen may run on CPU after a checkpoint, but its numbers do not override the joint promotion rule above.
