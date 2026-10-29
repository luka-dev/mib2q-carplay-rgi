# Cross-Corpus True-Stem Remix Probe

_Track B, 2026-09-29. Candidate sampler and comparison frozen before a PC training run._

## Question and data boundary

The [MoisesDB-only remix](UPMIX_MOISES_REMIX_PROBE_2026-09-29.md), [five-song curated OnAir supplement](UPMIX_ONAIR_DATA_PROBE_2026-09-29.md), and [six-family MoisesDB remix](UPMIX_MOISES_INSTRUMENT_SIDECAR_2026-09-29.md) each failed a matched 4k screen. They did not test **source families crossing corpora**: the current sampler uses MoisesDB donors for a MoisesDB base song, while an OnAir window remains from one OnAir song. This probe asks whether independently combining real MoisesDB and audited OnAir vocal/drum/bass/eligible stems helps at the same marginal corpus exposure. It is separate from review 43's causal, distinct-MoisesDB-song fine-role run.

The only extra true-stem pool is the existing five-song `D:\upmix\datasets\onair_cache7`, whose admission, stem roles and master-alignment checks are in the [OnAir report](UPMIX_ONAIR_DATA_PROBE_2026-09-29.md). All its songs are TRAIN only. The 176 MoisesDB TRAIN caches supply the other true-stem pool; the 774 owner-library teacher estimates remain pseudo labels and are never donors for a true-stem remix. Natural DEV songs stay unmodified. The test cannot answer whether a substantially larger or different corpus would help.

## Frozen single-change comparison

Both arms use the same `sep_net.py` BandSepNet rapid recipe, seed 1, 4k steps, 8 s crops, 50% true / 50% pseudo windows, `--extra-truth-frac 0.25`, `--remix 0.5`, and the same OnAir cache. In the control, only MoisesDB truth windows remix, with all donors from MoisesDB; OnAir truth windows remain same-song. In the candidate, `--cross-corpus-remix` lets each remixed true-stem family independently choose MoisesDB with probability .75 or OnAir with probability .25, including when the vocal/base song is OnAir. The vocal family stays with its base song; drums, bass and eligible `other` get independently drawn song paths and offsets. A donor may equal the base song by chance; this flag does not enforce distinct song identities. Slot gains remain independent ±6 dB. The target is recomputed as the exact sum of the selected eligible source; no mastered mix is forced to match it.

The same marginal true-stem pool selection applies to every family in the candidate. With the frozen half-truth, half-pseudo, .5-remix and .25-OnAir probabilities, roughly **17% of all windows** should contain at least one cross-corpus source family. This is a larger intervention than adding OnAir intact but does not change the student architecture, loss, pseudo pool, held-out songs, or nominal corpus share. Both arms must run under one shared code hash after the current review-43 GPU job and its diagnostic finish.

The sampler flag is in [`sep_net.py`](RETIRED_TOOLS.md#tool-9d28f37ffbb4). A synthetic 2,000-window check with distinguishable MoisesDB/OnAir source markers and `--remix 1.0`, 50% OnAir among true windows found **0%** cross-corpus eligible/vocal pairings in the control and **48.25%** in the candidate, with eligible and vocal OnAir shares **48.0%/49.95%** in the candidate. Maximum `mixture − (eligible + vocal + drums + bass)` error was **2.98e-8** for the candidate. This checks source selection and target closure; it is not a separation result. The flag requires `--extra-truth` and positive `--remix` and rejects the unrelated instrument-sidecar path.

## Decision sequence

Before PC training, freeze both command lines, source/cache/renderer hashes and candidate exposure counts from the actual sampler; use an idle GPU lease and no PC environment installs. Compare the matched 4k checkpoints on the same DEV12 source-attribution, 43 vocal-entry events and fixed-gain real-C DEV20 screen. Require useful eligible or instrument rear-share gain at similar rear RMS **jointly** with no material regression in voice, bass, drums, vocal-entry median/p10, absolute eligible pre/post, or any-stem swings. A +.03 eligible or +.02 family share is only a screening signal; near seed variability, repeat with seed 2. A surviving signal needs matched 10k and then a causal `norm=stream` comparison, because this rapid default uses a noncausal whole-window level. Do not tune a gain or threshold on inspected DEV to rescue a failure.

If this pair fails, preserve its per-song results and stop treating the five-song OnAir pool as a likely way through the current extraction shelf. The product goal still requires a causal candidate through full40 real C at stable gain, a new adequately covered real-stem set, measured device int8 cost and latency, a Track A-owned hook port, and the owner's blind live CarPlay verdict. This training comparison alone cannot satisfy any of those final gates.

## Preflight and smoke after review 43

The causal review-43 cross-song run and diagnostic finished with a failed joint gate; its GPU lease was released. The PC shared `sep_net.py` differed from the committed local copy **only** by this new cross-corpus flag, so the latter was copied and read back at SHA-256 `11E0C4FE89C82D3BE3814E5D2A421C5FD73C40D4E3FA505904C57AE1D7DBA905`. The rapid daemon remains byte-identical to the earlier matched screens (`BE2094DD…`). All 176 MoisesDB TRAIN caches are at least **56.48 s** and all five admitted OnAir TRAIN caches at least **152.46 s**, so an 8 s crop cannot silently skip a donor. A 10,000-window symbolic pass at the frozen .5 truth / .5 remix / .25 OnAir settings produced **50.62% truth windows** and **17.53% of all windows with at least one cross-corpus donor**, with source-sum closure error **4.77e-7**. That symbolic pass used distinguishable source markers; it did not train or measure separation.

The guarded [`run_cross_corpus_smoke.ps1`](RETIRED_TOOLS.md#tool-e04d4ed1d2d9) tested the real Python, cache and GPU path for 20 steps per arm. Both reported **176 MoisesDB true / 774 pseudo / five OnAir** songs and **586,916 parameters / 125 M estimated neural MAC/s**. A first PowerShell wrapper attempt passed the candidate's flag as separate characters and stopped before candidate training; the control reached step 20, released the lease and wrote its checkpoint. The wrapper was corrected to splat a string array, preserve the completed control, and rerun only the candidate. Both arms now have `train_done.txt` and `step20.pt`; no GPU lease remained. These short runs prove execution, not a source or C result.

The exact main rapid queue lines are:

```text
crosscorp_ctrl_s1|--remix 0.5 --extra-truth D:\upmix\datasets\onair_cache7 --extra-truth-frac 0.25
crosscorp_mix_s1|--remix 0.5 --extra-truth D:\upmix\datasets\onair_cache7 --extra-truth-frac 0.25 --cross-corpus-remix
```

The control and candidate will use the same committed code, seed and rapid daemon. Check an empty queue, no running trainer, a free lease and absent tag folders immediately before enqueueing; then hold one GPU tenant through both 4k trainings and diagnostics. A C screen may run on CPU after a checkpoint, but its numbers do not override the joint promotion rule above.

**Matched pair launched, 13:39 PC local time.** The preflight found an empty queue, no rapid daemon/trainer, no GPU lease and no existing `crosscorp_ctrl_s1`/`crosscorp_mix_s1` folders. PC SHA-256 values matched source `11E0C4FE…`, rapid daemon `BE2094DD…`, OnAir split `4B3955D2…` and real-C executable `730A2CA9…`. The two exact queue lines above were written, and detached daemon PID **10088** was launched. Read-back after SSH disconnected found that daemon, one control `sep_net.py train` process **17096**, the `rapid` GPU lease and the expected 176/774/five-song header. The candidate remains queued behind the control. No checkpoint or source/entry/C result existed at this read-back; process launch alone proves none. Do not start another GPU task or modify the PC Python environment until this pair is terminal.

## Completed 4k pair and decision

Both arms finished on 2026-09-29 PC local time: control at **14:02:36**, cross-corpus candidate at **14:27:06**. Both have `train_done.txt`, `step4000.pt`, `gap.txt`, `transitions.txt`, and a 20-song `screen/stems.csv` under `D:\upmix\runs\rapid\<tag>`. The matched comparison is `D:\upmix\runs\rapid\crosscorp_pair_s1.json`, produced by the hash-matched `compare_rapid_pair.py` with `--expected-songs 20`. The queue and GPU lease are empty, no training or stem-screen process remains, and rapid daemon PID 10088 was stopped only after both jobs were terminal.

| Fixed DEV diagnostic | Control | Cross-corpus candidate |
| --- | ---: | ---: |
| DEV12 eligible transmission, source attribution | .38 | .36 |
| DEV12 vocal transmission | .201 | .174 |
| DEV12 protected transmission | .087 | .092 |
| Eligible 8–14 kHz transmission | −22.3 dB | −22.2 dB |
| 43 vocal entries, retention post/pre median | .717 | .720 |
| 43 vocal entries, retention post/pre p10 | .412 | .384 |
| 43 vocal entries, absolute pre/post median | .546 / .386 | .487 / .345 |
| Real-C rear RMS, 20 songs, fixed G 1.0 | −37.5 dBFS | −37.2 dBFS |
| Any-stem swing songs, 20-song paired screen | 4 | 4, no new or resolved |

| Real-C rear share, fixed-gain paired screen | Control median | Candidate median | Paired median delta (90% song-bootstrap CI) |
| --- | ---: | ---: | ---: |
| Vocal, 20 finite songs | .049 | .048 | −.003 (−.004, +.004) |
| Guitar, 20 | .095 | .102 | −.005 (−.011, +.008) |
| Other keys, 15 | .094 | .088 | −.004 (−.008, +.007) |
| Piano, 8 | .060 | .061 | +.002 (−.004, +.009) |
| Bowed strings, 5 | .092 | .151 | +.016 (−.004, +.059) |
| Bass, 11 finite paired songs | .057 | .053 | −.004 (−.011, +.002) |
| Drums, 20 | .083 | .087 | +.000 (−.002, +.007) |

The strings difference is based on only five songs; its paired interval includes zero. Their difference of medians (+.059) is larger than the paired median gain (+.016), so it is not evidence of a repeatable family improvement. The screen's `n=20` bass line includes songs whose per-song `median_rear` is `nan`; the paired finite bass count is 11. Neither arm meets the product's guitar/keys/piano, voice or drum thresholds. The candidate's lower vocal transmission came with lower eligible transmission, a worse entry p10 and nearly unchanged C protection at a **+.3 dB** rear level. The precommitted **joint** gate therefore rejects the cross-corpus five-song OnAir recipe. Do not promote it to 10k, causal `norm=stream`, full40, device or car testing. This result tests one sampler and one small corpus, not all cross-dataset mixing. Preserve the checkpoints and song-level CSVs; pursue a larger, verified real-stem pool or one measured causal architecture mechanism with a fresh matched control.
