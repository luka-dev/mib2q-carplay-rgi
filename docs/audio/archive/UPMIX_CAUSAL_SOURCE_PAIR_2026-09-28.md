# Upmix Causal Shared-Mask Source-Loss Pair

_2026-09-28. Track B offline mechanism screen. No hook or head-unit change, int8 device measurement, independent validation or listening result._

## Why this pair

The [direct-object renderer bound](UPMIX_DIRECT_OBJECT_RENDERER_PROBE_2026-09-28.md) passed the measured DEV40 C-render gates with true eligible audio and with ideal bounded real-mask audio. The frozen sep9 estimate through the same C path resolved five of six enriched-20 swing songs but still missed guitar (.105), keys (.144), piano (.089) and bass protection (.070). Another continuation of that checkpoint would leave its noncausal whole-window normalization and content objective untouched. This pair tests whether explicit, source-attributed protection and eligible retention change that extraction frontier in a budget-sized causal graph.

Both arms use `BandSepNet` with one bounded real mask shared by L/R, `--norm stream`, no time bidirectionality and zero lookahead. The neural cost estimate is **107 M MAC/s** at 48 kHz / 512-hop, excluding STFT and renderer; no device cost is claimed. Both use the same seed, 176 MoisesDB truth songs plus 774 pseudo-labelled owner-library songs at half/half sampling, 8-second crops, batch 12, 4k steps, learning rate 3e-4, 2k warm-up and the original audio reconstruction bundle. This is a matched-age **mechanism screen**, not a trained product candidate. `--norm stream` removes whole-window future information in the network input; the Python whole-file convenience inference still needs a stateful chunk implementation before causality and latency can be certified on device.

Only the candidate adds `--source-attr .054`. [`sep_net.py`](RETIRED_TOOLS.md#tool-9d28f37ffbb4) now returns the aligned eligible, vocal, drum/percussion and bass stems from the seven-slot cache. The auxiliary loss applies the **same predicted mask** to each source: eligible reconstruction error plus the mean transmitted protected-source energy, each normalised by its own source energy with a mixture-relative silence floor. Unlike the older `--attr` term, it explicitly protects bass and drums as well as vocals. [`diag_source_attr_grad.py`](RETIRED_TOOLS.md#tool-7b5de5513461) measured finite parameter-gradient norms on four fixed CPU batches. Their baseline/source ratios were **.258/.346/1.205/.374**; .054 makes the auxiliary contribution about **15% of the median baseline parameter-gradient norm** at initialisation. This weight was frozen before training and is not adjusted from DEV results.

[`run_causal_source_pair4k.ps1`](RETIRED_TOOLS.md#tool-a8a0b78bcc77) holds the PC's single GPU lease, guards the empty rapid queue and active processes, and trains the anchor then candidate sequentially. It checks the PC `sep_net.py` SHA-256 (`f017e818feb372b65c12af6a319fbd02cfdf1d745066d2ae9e6d5eabc891af81`), writes to `D:\upmix\runs\causal_source_pair4k\{anchor,source_attr}\`, and releases only its own lease token. The run started on 2026-09-28; results are pending below.

## Frozen evaluation before looking at results

The two step-4000 checkpoints will use [`run_causal_source_pair_diag.ps1`](RETIRED_TOOLS.md#tool-2b85d0184ea1) for source-attributed eligible/protected transmission and the established vocal-entry detector. [`run_causal_source_pair_screen20.ps1`](RETIRED_TOOLS.md#tool-1370ad771835) will pass both through the **same** real host C control and the frozen direct/no-bed/body-0 point (surround .3235) on the enriched DEV20 manifest. Within each arm it verifies byte-identical injected WAVs between C conditions. The pair is compared by song, not by stem row as independent samples.

The preselected rule for extending **both** arms to 10k is: candidate median eligible transmission rises by ≥.05; at least one of guitar/keys C rear share rises by ≥.02 and neither falls by >.01; each protected-stem C share rises by at most .01; no new swing song; paired median rear RMS remains within ±.5 dB; and vocal-entry ducking p10 does not fall. Passing this screen is a mechanism signal, not acceptance. The product limits remain guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14; voice ≤.02, bass ≤.06, drums ≤.08; ≤5/39 all-stem swing songs on full40, slopes within ±.05, entry ducking median ≥.85 and p10 ≥.60. A short screen cannot replace 10k/40k, independent stems, a stateful implementation, measured ≤150 M int8 MAC/s and ≤85 ms extra latency, or the owner's blind in-car verdict. The earlier shared-mask improvement at 4k reversed at 10k, so any positive 4k result requires a matched longer follow-up before promotion.

## Results and decision

Both step-4000 runs completed cleanly. On the fixed 12-song source-attribution diagnostic, the anchor/candidate median eligible recall was **.46/.49**, vocal transmission **.156/.195**, drum/percussion transmission **.070/.088**, and SI-SDR **−1.49/−1.30 dB**. The added source loss improved output resemblance by .19 dB while passing more of both eligible and protected audio. Its .03 eligible-recall gain misses the preselected ≥.05 extension requirement before any C result is considered. The ideal ratio-mask reference on the same diagnostic has .77 eligible recall and .105 vocal transmission.

On the fixed 20-song transition diagnostic (43 vocal entries), anchor/candidate median entry post/pre eligible retention was **.835/.847**, with p10 **.588/.618**. The candidate clears the .60 p10 target on this sample but remains just below the .85 median target, and its median 0–2 s vocal transmission is higher (**.196/.241**). The source-loss effect is therefore consistent with opening the mask, not yet selective source protection. These are diagnostic transmission ratios before C rendering, not rear-channel shares.

The matched real-C enriched DEV20 screen completed with **115 stem rows from 20 songs per arm**. The injected WAVs were byte-identical between original and direct C within each arm. The selected direct renderer removed both swing songs in each arm (**2→0/20**, no new); both student outputs stayed below the full40 promotion gate. The direct-renderer result by source is:

| Source / metric | Anchor | Source-loss candidate | Product limit |
| --- | ---: | ---: | ---: |
| Guitar rear share | .104 | .118 | ≥.15 |
| Other keys rear share | .143 | .156 | ≥.15 |
| Piano rear share | .109 | .110 | ≥.10 |
| Bowed strings rear share | .182 | .189 | ≥.14 |
| Vocal rear share | .0157 | .0218 | ≤.02 |
| Bass rear share | .0680 | .0822 | ≤.06 |
| Drum rear share | .0084 | .0098 | ≤.08 |
| Any-stem swinging songs | 0/20 | 0/20 | ≤5/39 on full40 |
| Rear RMS across these songs | −37.389 dB | −36.743 dB | matched-level comparison |

The song-paired median candidate-minus-anchor change was **+.0157** guitar (90% bootstrap CI .0110–.0175), **+.0152** keys (.0021–.0184), **+.0058** vocals (.0029–.0073) and **+.0129** bass (.0076–.0208). Paired median rear RMS rose **+.668 dB** (CI +.439–+.750), outside the frozen ±.5 dB mechanism limit. Guitar gained less than the required +.02, while protected bass rose by more than +.01. Eligible recall rose only +.03, below the +.05 requirement. Although keys crossed .15 and vocal-entry p10 improved, the complete preselected 4k→10k rule **fails on four separate conditions**. No 10k continuation or full40 student promotion is warranted for this source-loss variant.

This loss produced a more open mask: more eligible audio reached the rear, but so did vocal and bass energy, at a louder rear level. It does **not** prove a bounded shared mask is impossible: the ideal bounded-mask/audio upper bound passes the DEV40 numerical C gates, while this 4k trained estimate is far below its .77 eligible-recall/.105 vocal-transmission reference. More steps alone have no positive selection signal here, and the earlier shared-mask gain reversed between 4k and 10k. The next offline model experiment should explicitly test selective source ownership at matched rear energy and matched training age, with the same diagnostic and C screen fixed in advance. A distinct real-stem evaluation set is still needed: the existing owner-library teacher cache is pseudo-labelled, and all 40 DEV plus 20 of 24 TEST MoisesDB songs have already been inspected. Device int8 MAC/s, streaming latency, hook integration and the owner's in-car listening remain unmeasured.

PC artifacts: `D:\upmix\runs\causal_source_pair4k\{anchor,source_attr}\step4000.pt`, `gap.txt`, `transitions.txt`, `screen_{anchor,source_attr}\decision.json`, and `source_attr_vs_anchor_direct_paired.json`. SHA-256 of the anchor checkpoint: `C8489D0BDAE774548124300870471CFCE271F74F06EB907291D14E248B264A95`; source-loss checkpoint: `2F1AC420CB8F4ACC8546E06E4EB58F19BB6D0D045D879BBC2A9ACEC36EBC3BC3`. The GPU lease was released and the rapid queue is empty.
