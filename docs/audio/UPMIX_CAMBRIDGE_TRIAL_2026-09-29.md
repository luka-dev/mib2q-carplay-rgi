# Cambridge Multitrack Data Trial — 2026-09-29

_Track B. Small matched training test while DSD100 transfers; no model promotion or car claim._

## Scope and provenance

The owner stated on 2026-09-29 that permission for this test has been obtained and will add the license record to the repository. The [Cambridge library FAQ](https://www.cambridge-mt.com/ms3/mtk-faq/) says AI training requires individual contributor licensing; the user's permission statement is the basis for this private test. Keep the trial archives, caches and checkpoints on the owner's PC. Do not treat the missing repository record as permission to redistribute audio or weights.

This first trial uses three direct [Cambridge multitrack library](https://www.cambridge-mt.com/ms3/mtk/) ZIPs with clearly named instrument tracks: Ajay Nagarkoti & Vibhu Dixit — _Ae Mere Humsafar_ (guitars, piano, strings, vocals); Finlay — _Same Kind Of Life_ (guitars, piano, organ, vocals); Killa-L — _Window_ (strings, organ, synths, vocals). The ZIP central directories were inspected before downloading, but names alone do not prove clean sources or 60-second alignment. The explicit [role map](../../tools/stereo-upmix-render/cambridge_trial_map.json) assigns every WAV index to one of seven training slots or an explicit exclusion; Ajay's ambiguous `SFX` track is excluded. This is a deliberately small data intervention, not an independently covered 20-song evaluation set.

The guarded [downloader](../../tools/stereo-upmix-render/pc/download_cambridge_trial.ps1) leaves all three archives as `.zip.part` and checks the published HTTP lengths (398,950,192, 347,121,142 and 404,201,579 bytes). The [cache converter](../../tools/stereo-upmix-render/cache_cambridge_trial.py) verifies each full ZIP CRC and SHA-256, confirms every WAV is accounted for, requires a ≥60 s session, and writes 48 kHz seven-slot int16 TRAIN-only caches. The training mixture is exactly the sum of selected mapped tracks; no mastered mix is used as a false closure target. A synthetic 60-second, three-track ZIP yielded the expected `(7, 2, 2,880,000)` int16 cache. All three real archives subsequently passed this validation; execution details follow below.

The first actual ZIP has substantially different source **end** lengths: Ajay's mapped backing vocal 21 is 28.28 s, while bass and synth extend to 266.67 s. This makes a 150 ms equal-length gate inappropriate. The [library's import instructions](https://www.cambridge-mt.com/ms3/) say to put all WAVs at the same timeline origin; the converter therefore records each length and zero-pads tails to the longest mapped track. That source-alignment assumption still needs a mix/preview timing audit before treating the corpus as reliable. The first strict run failed without publishing a cache; the converter was changed before any training.

## Frozen comparison

Use the existing BandSepNet rapid recipe and the same source/entry/real-C DEV20 screens as the previous cross-corpus pair. Two seed-1, 4k arms differ only in the extra truth pool:

```text
cambridge3_ctrl_s1|--remix 0.5 --extra-truth D:\upmix\datasets\cambridge_moises_control3 --extra-truth-frac 0.5
cambridge3_add_s1|--remix 0.5 --extra-truth D:\upmix\datasets\cambridge_trial_cache7 --extra-truth-frac 0.5
```

The control uses three SHA-selected MoisesDB TRAIN songs, hard-linked into `cambridge_moises_control3` with no DEV/TEST IDs. Both arms sample truth versus teacher pseudo-stems 50/50 and draw the extra corpus for half of truth windows, about **25% of all windows in expectation**. In this `sep_net.py` path, `--remix 0.5` acts on the **base MoisesDB truth pool only**; the extra-pool songs stay intact in both arms, and cross-corpus remix is off. This isolates adding the three songs. Dataset size is controlled, not instrument distribution. The cache path and both lines were frozen before either 4k run.

The preselected screening gate is a useful eligible source-transmission gain ≥.03 or instrument-family rear-share gain ≥.02 at rear RMS within ±.5 dB, together with no material voice, bass, drum, vocal-entry retention or swing regression. A borderline result requires seed 2. Because three songs are a tiny, potentially repeated pool, even a passing 4k signal only justifies a larger licensed cohort and matched 10k causal confirmation. A failure bounds this three-song intervention; it does not rule out Cambridge or data augmentation generally. Neither arm is a device-ready model: the rapid default uses noncausal `norm=window`, and estimated neural MACs exclude the total device chain.

## Execution status

At start, DSD100 downloader PIDs 48584/20888 were alive, the global GPU lease and rapid queue were empty, and no Python trainer was running. All three Cambridge ZIPs subsequently completed at the expected lengths and passed full ZIP CRC during conversion. The per-archive SHA-256 values are:

| TRAIN song | ZIP SHA-256 | Mapped WAVs | Cache length | Shortest mapped source |
| --- | --- | ---: | ---: | ---: |
| Ajay — _Ae Mere Humsafar_ | `df17d2bdb95467906179d7e8412b4220d7ac8644a8af5bc1ca757f52790b0176` | 21 | 266.7 s | 28.3 s |
| Finlay — _Same Kind Of Life_ | `89070e4734d5f4869a6835aa429269eb5d4e8958f5c4efea9305e792234a8bfb` | 20 | 224.5 s | 132.2 s |
| Killa-L — _Window_ | `38064001bffd7e598535be21b51de53e7a7b53f99a1eff4ffb06720cac53e8bb` | 36 | 215.5 s | 19.9 s |

The published three-song TRAIN split SHA-256 is `484DB95F51E9C6C1079F8A9339D752D0A097032A99519F45E3AB2CF9CDC090CA`; the three-song MoisesDB control split is `338FF50105BE8ABC8722084ABECF449050276914CACA117B8ED7BBBB9C6D1022`. Both 20-step GPU smoke arms completed with 586,916 parameters and **125 M estimated neural MAC/s**. The control and candidate had equal step-20 DEV SI-SDR (−6.11 dB) and vocal leak (.097); those are execution checks, not promotion evidence.

The frozen pair was queued after checking the empty queue/lease and absent tag folders. The first detached `Start-Process` reported a PID but was absent immediately after SSH returned, leaving the queue and launch lease intact; an explicit process snapshot showed no daemon or trainer, so no job was restarted. The daemon was then launched in a persistent SSH session. Its initial read-back found one `sep_net.py train` control process (PID **26496**), a `rapid` lease and both expected queue lines. Its control header reported 176 MoisesDB truth, 774 teacher pseudo-labelled and three extra songs. The local launcher was corrected to run the daemon in the persistent session on future use. No car or device result follows.

**Control completed, 15:39 PC local time.** The control has `step4000.pt`, its source and vocal-entry reports, and the fixed-gain real-C DEV20 `screen/stems.csv`. DEV12 median eligible transmission is **.43** and vocal leak **.201**; eligible 8–14 kHz transmission remains **−23.1 dB**. Across 43 entries, post/pre eligible retention median/p10 is **.759/.419**. In the C screen, rear-share medians are guitar **.106** (n20), keys **.105** (n15), piano **.063** (n8), strings **.115** (n5), voice **.045** (n20), bass **.062** (n11 finite), drums **.096** (n20); **3/20** songs have any-stem swings by the >.30 range rule. The candidate started next with a live trainer PID **20604**; no candidate checkpoint or pair verdict exists at this snapshot. The control alone misses the goal gates.


## Completed pair and audit decision

Both 4k jobs and DEV12 source / DEV20 entry / real-C screens finished. Candidate versus the three-song Moises control: guitar **.1062→.0963**, keys **.1045→.0897**, piano **.0634→.0581**, strings **.1145→.1090**; voice **.0452→.0459**, bass **.0617→.0576**, drums **.0959→.0894**. Any-stem swing songs are **3→2/20**. Rear level with one observation per song is **−37.423→−37.828 dBFS**. Entry retention median/p10 is **.759/.419→.757/.395**, with absolute pre/post **.554/.427→.620/.408**. The current joint gate rejects extending this recipe to 10k.

The [effectiveness audit](UPMIX_EXPERIMENT_EFFECTIVENESS_AUDIT_2026-09-29.md) distinguishes historical vocal-owned leak **.20085→.21197** from correct all-cell vocal transmission **.22091→.23145**. An independent read-only audit corrects the vocal-owned interference calculation to **.19167→.19517** without editing the other agent's `diag_gap.py` or overwriting old reports. Eligible transmission stays **.43190→.43191**; drum/bass transmission rises **.03187/.02311→.04305/.03252**.

The aggregate 8–14 kHz gain **−23.1→−14.8 dB** is distributed unevenly: one song accounts for **87.4%** of the net output-energy increase, paired-song median gain is **+1.98 dB**, and 3–8 kHz worsens on **11/12** songs. This does not establish broad instrument recovery. Guitar's paired median delta is **−.0124** (90% song-bootstrap interval **[−.0173, −.0043]**), keys **−.0099 [−.0149, −.0022]**; these intervals do not measure seed variance. The small, noncausal, repeated-extra-pool comparison does not rule out a larger or converged Cambridge recipe.

The queue and GPU lease cleared; the verified idle rapid daemon PID 7744 was stopped. The next plan uses a fixed TRAIN fitting check, a causal same-song baseline and one longer larger-corpus comparison. No Track A, device or owner's ear result changed.
