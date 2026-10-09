# UPMIX radical directions (2026-10-07)

*The owner's question after the [rework baseline](UPMIX_REWORK_BASELINE_2026-10-07.md) and the [incremental proposal](UPMIX_REWORK_PROPOSAL_2026-10-07.md): what could be changed globally, something radically new, to get an incredible rear scene? Seven directions were each researched by an independent agent against the repo and external sources, scored by two judges (wow lens, feasibility lens) and merged into the ranked menu below. Nothing here has been built, deployed or heard.*

---

## 0. Coordinator's notes (read first)

**Panel.** Directions: Atmos-corpus teacher, compute envelope, sidecar box, cabin/amp (8-channel sink + amp matrix flash), driver-seat acoustics (cabin IRs + rear-lead), venue + events, content-first (authored APAC + the Spotify realtime guard). Six cards came back structured; the cabin/amp card did not: its agent's answer was stopped by a safety classifier around the amp-flashing (UDS RequestDownload) material and its structured output was lost. What survives of it is summarised in §0.1 below from the agent's partial text; the judges did not score it.

**The two findings that change the order of work:**

1. **The precedence lever was never used in our favour.** Every rear component today arrives ~24 ms after the front (12 ms hook delay + 6.6 ms amp bus + 5.2 ms amp output [model]), so precedence pulls anything present in both channels to the front by design. Inverting it (front delayed, eligible rear undelayed, 2–5 ms lead) is the only zero-compute change in the whole record that changes the perceptual *category* of the rear. It needs the cabin measured first, which in three weeks of amp modelling was never done: a six-channel sweep at the driver's head is ~60 lines of hook code and one parked hour.
2. **The owner's "slightly in the rear" may be partly a switched-off rear.** Spotify runs on the type-100 realtime path whose guard silences the rears after four 12 ms overruns until a 500 ms gap; the post-fix period histogram was never collected. One `grep 'realtime upmix' /tmp/carplay_hook.log` after a Spotify drive answers it.

**Coordinator's ranking adjustments to the menu:** none to the order. Two emphases: (a) the sidecar box is the only direction whose ceiling is "specific instruments standing behind me" (SIMO + direct route passed 14/14 offline), so if the owner's definition of incredible is objects rather than a stage, its free offline kill test should start on the idle RTX 5080 immediately; (b) the Atmos-corpus capture is a private-research measurement that breaches Apple's terms and must never enter the repo; the two-night *static profile* is worth it regardless of whether the learned controller is ever trained.

### 0.1 Cabin/amp direction (recovered summary, PARK)

- **8-channel sink 0x32** (positions 7/8 = BCL/BCR → effect buses 6/7 at 0 dB in A07 bank 2) would make the C-pillar effect speakers an independent feed instead of a +9 dB copy of Ls: four rear-ish zones instead of two. Cost: the whole HU chain is six-wide by construction (deva channel map {1,2,3,6}, `deva_csd_start_csd_session_playback` hardcodes 6, no 8-channel output node, LPASS COPP/AFE slot table, MOST allocation BlockWidth 26 → 34, sink 0x32 Connect). We already ship a modified deva mirror driver, so deva is patchable; CSD/LPASS/MOST are not known to be. Weeks-to-months of RE with no second unit. PARK until a reason bigger than "two more zones" exists.
- **Amp matrix flash.** The agent found that A07 (matrix banks) sits at 0x100000+ outside the CRC-covered V850 code region, that the DSP region is verified only by an 8-byte label compare, that B01 banks carry a recomputable CRC-16, and that the amp exposes a UDS RequestDownload path to its flash. Editing bank-1 cells would delete the L → busLs/bus6 −3 dB front bleed at the source and set Ls/Rs gains. **Parked on risk, not on value:** homemade flashing of a safety-relevant ECU over MOST with no readback transport, no second amp and no recovery path is a brick risk the project cannot absorb; the hook-side bleed cancel (proposal §2.5) reaches most of the same result in software once the cabin sweep gives the offset.
- **The amp's own first-B spatial upmixer** on the stereo sink as a zero-compute baseline: covered by the venue card's step 0 (MMI Surround min → max with our PCM6 session playing tells whether the first-B tail is live) and by the content-first Atmos A/B.

---

Synthesis of six direction cards and two judge scorecards against the rework baseline and proposal (`docs/audio/UPMIX_REWORK_BASELINE_2026-10-07.md`, `UPMIX_REWORK_PROPOSAL_2026-10-07.md`). Evidence tags as in the baseline: [car] > [log] > [offline] > [model] > [inferred]; [verified] = re-checked in the repo today. Owner = one person, Mac M3 Max, Windows RTX 5080, 4-core NAS, the car, no amp-flashing tools.

## 1. The one-paragraph answer

Stop defining the rear as "what a classifier permits from the stereo mixture, rendered 24 ms late at −34 dBFS" and define it by the cabin and by time: measure the six channels at the driver's head (1 h, 0 EUR), then let the eligible rear component arrive FIRST at the ears (front delayed ~25 ms, rear delay 0) on top of the structural rear the proposal already plans, and calibrate its level and body not from harness gates that pin rear RMS at −37 dBFS ±0.5 dB [offline, baseline §6] but from what professionals actually put behind (Apple's own Atmos renders, measured on the Mac). This is the only lever in the whole three-week record that changes the perceptual category of the rear — from "a quieter, later echo of the front that precedence fuses forward" (12 ms hook + 6.6 ms bus + 5.2 ms output [model, baseline §1.11]) to "the first wavefront from behind-left/right with the front as support" — and it costs zero MAC/s, zero training and no hardware. Every other radical option is either a calibration of that (Atmos profile), a layer on it (venue), or a second-stage bet on extraction that is gated by one free offline experiment (the sidecar box with 100–300 ms lookahead; the only direction whose ceiling is "instruments standing behind me", SIMO + direct route 14/14 gates [offline 29.09]). More Krait compute is already measured insufficient: the repo's own 2.17 G MAC/s causal reference (14× budget) failed the absolute gates (guitar .121, strings .109, entry .804/.420 [offline, baseline §3.2]). The honest ceiling: ~60 % the sweep says the rear path is ~12 ms and rear-lead is testable; of that about half that it reads "behind" rather than "wider" — but the measured cabin table is kept either way, and it is the first [car] number the project has ever had for the amp.

## 2. Ranked menu

Composite of the two judges (wow lens: REAR-LEAD 23, CONTENT-FIRST 23, SIDECAR 19, VENUE 18, ATMOS 17, COMPUTE 12; feasibility lens: CONTENT-FIRST 22.5, VENUE 20.5, REAR-LEAD 18.5, ATMOS 16, SIDECAR 14.5, COMPUTE 11), re-ordered by what the owner asked for (incredible, not safe).

### 2.1 Measured rear-lead (cabin IRs first, then rear leads / front follows) — DO FIRST

**What changes.** Two assumptions die: "the rear is a copy that arrives 12 ms later so the front anchors" becomes "the eligible rear arrives 2–5 ms before its front copy"; "the amp is a [model]" becomes a measured driver-seat IR per channel (L, R, C, LFE, Ls, Rs → head). Mechanism: ~60 lines in `stereo_upmix.c` (env `AUDIO_STEREO_PROBE=1`, static −30 dBFS 2 s log-sweep table, sequencer under the cabin-guard early-out); Mac mic at the headrest at 48 kHz (the phone is the CarPlay source and cannot record); `tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py` already has `probe` / `deconvolve` / `binaural` subcommands and a BRIR manifest [verified :1164-1176, never run]. Then: front delay line 512 → ~1200–1500 frames (5–6 KB S16), rear delay 0 (the 576-frame ring at `stereo_upmix.c:557` becomes a signed knob), 18 biquads ≈ 4 M MAC/s, alpha relocation 0.35 of the coherent off-centre term so the lagging front copy is 3–6 dB quieter in those bins. CTC/transaural and head tracking stay parked: the J525 plant between PCM6 and the ears is fixed, bleeding (L→busLs −3 dB), possibly adaptive (C4D17) and unreadable; published car CTC that worked used dedicated headrest/dipole/array geometry and ±2–5 cm head tolerance (Qiu/Masiero/Vorländer 2009).

**Why incredible (honest).** Panned guitars/keys/pads stop being an echo and become the localised source behind; centred lead/bass/kick stay front by O = 0 (no veto, no time constant). Not incredible for: narrow/mono masters (static floor only), bass guitar (doors HP ~100 Hz [model]), and if precedence does not hold at 2–5 ms lead against a louder lagging front — then it degrades to "wider" (merely better). The Q2 comb lesson (zero-delay coherent rear "dragged the image" with colouration [baseline §5]) says the drag is real but can read phasey.

**Evidence.** [model] rear 6.6 + 5.2 ms later than front mid/treble before the hook's 12 ms; [car 17.09] the only wow was level + delay with full transfer (energy moved), not duplication; [offline] passive side matrix sits on the student frontier for free (guitar .13 / keys .21 / voice .05, 4/39 swings, baseline §3.3a); literature: fusion from ~1 ms, echo threshold 5–40 ms; cabin localisation 67 % within ±20° vs 77 % in a room (FA2026).

**Feasibility / cost.** 3–4 days to first listen (probe 0.5–1 d, 1 h car, 1 d knobs + EQ table, 0.5 d Docker); 10–14 days shippable; 0–100 EUR (USB measurement mic optional). Latency +15–20 ms must be re-advertised: `ALL_OUT_LATENCY=80` is in the firmware's `dio_manager.json` [verified `wireless/receiver_bridge/out/inspect_ifs/etc/eso/production/dio_manager.json:331`], not in `build/`.

**Kill fact.** Ls-vs-L arrival at the head > ~40 ms (1024-unit Dolby delays or a live first-B 32 ms tail) → no front delay inside the 85 ms gate makes the rear lead; the direction collapses to a measured EQ/level table.

**First experiment.** One parked hour: sweep on all six channels, deconvolve, read four numbers: (1) Ls−L arrival (expect ~+12 ms [model]); (2) late arrival inside the L response = amp bleed level and delay in the rear doors (expect −3 dB + 6 dB, ~12 ms); (3) Ls spectrum at the head (real door HP/notches); (4) any 30–60 ms diffuse tail = first-B live. Same day if (1) ≤ 20 ms: second replug, front delay = Ls−L + 3 ms, rear 0, structural eligible term, alpha 0.35, versus condition B = today's 12 ms lag; one question per hard-panned rock song: behind, or only wider?

**Verdict: DO FIRST** (the measurement now; rear-lead as condition C in structural session 1).

### 2.2 Content-first: prove the guard is not silencing Spotify; authored APAC as the A/B reference — DO FIRST (diagnostics only)

**What changes.** Nothing in the renderer; it supplies the two cheapest facts in the project. Atmos-tier Apple Music already arrives as discrete APAC-65 and bypasses the upmix [car since 06.09]; Spotify is denied the buffered stream by bundle ID in MediaToolbox [decompile] and runs on the type-100 realtime path with a 12 ms budget, 4 strikes → `spa_pcm_expand_fallback` = exact L,R,0,0,0,0 until a ≥ 500 ms gap [verified `rt_upmix_guard.h:27-41`].

**Why incredible (honest).** Wow 2: nothing new to hear. But if the stop line shows `disables ≥ 1`, "mostly in the front" on Spotify was partly a switched-off rear, and the fix (proposal's realtime middle tier, ~30 lines) is urgent. And the Atmos-on/off same-album A/B is the first in-car comparison with a ground truth: it tells whether the ceiling is the cabin or the renderer before any code is written.

**Evidence.** [log 21.09] old 8 ms build disabled in 3/5 sessions ("rears blinking"); post-fix histogram never collected (baseline §2.10, LANDMINES:198 "the realtime budget was invented, not measured"); WARN lines present without verbose marker [verified `audio_hook.c:4229` stop line, `:4301` DISABLED]; the listening log does not record which app the "faint" verdicts were on.

**Feasibility / cost.** 0 days, 0 code, 0 money. Index 66 (APAC 5.1.2) and `DCXEnabled` are PARKED: native 8-ch order unverified, iOS selects by source channel count so a wrong map degrades all Atmos playback; renditions already arrive with DCXEnabled absent (`audio_hook.c:9025`).

**Kill fact.** `disables=0`, nothing ≥ 12 ms in `ms_hist`, AND the owner calls an APAC-65 track (hook log `buffer103: APAC decoder up … 6 ch`) equally faint → neither the guard nor the stream is the limit; the cabin/amp presentation is, and every stream-side idea is dead.

**First experiment.** After a normal Spotify drive, unit still online: `grep 'realtime upmix' /tmp/carplay_hook.log`. Then one Atmos-badged album with iPhone Settings > Music > Dolby Atmos = Automatic vs Off, phone mic ≤ 0.5 dB level match.

**Verdict: DO FIRST** (the two reads); engineering half PARK.

### 2.3 Sidecar separation box (Jetson Orin Nano Super over the stock USB-Ethernet host path) — DO NEXT (offline kill test now, hardware later)

**What changes.** "Everything fits 150 M MAC/s on the Krait" and "causal within 10.7 + 12 ms" both go: the box has 10–100× the compute, and 100–300 ms of lookahead becomes legal for media if `ALL_OUT_LATENCY` is raised. Link is stock firmware: `usblauncher.lua` ASIX AX88772B/AX88179 → `extnet.sh` → `devnp-asix.so /dev/io-net/en0` [verified firmware image :288-307], `en0 = dbg_if` in all four PF profiles [verified `deploy/pf/stock/pf.ecm0.conf:6`]. Hook: ~500 lines cloned from the `bus.c` connector pattern, seq-framed ring, D-delay line, per-block crossfade to the local structural rear when late/absent (never silence). Day-1 model = the already-trained residual96x4 reference exported to TensorRT; day-2 = chunked-lookahead retrain on the 5080.

**Why incredible (honest).** The only card whose ceiling is "instruments standing behind me": exact eligible audio through the NN-free C renderer → ~39 % rear share at 2/40 swings [oracle 30.09]; BSRNN SIMO + direct route .200/.211/.219/.204 instrument share, voice .0022, 0/40 swings, 14/14 gates [offline 29.09]. Without a lookahead win it is "more causal compute", which already failed (2.17 G reference; published 23 ms separators at 3.6–4.3 dB "other" cSDR, HS-TasNet arXiv 2402.17701, RT-STT arXiv 2511.13146).

**Feasibility / cost.** ~$340 (devkit $249 + NVMe + dongle + 12 V buck + fan); 5–7 days to a first listen with the causal reference, 3–5 weeks shippable; devkit rated 0–35 °C (a parked cabin exceeds it; fallback covers throttling); cold boot 27 s stock; `ALL_OUT_LATENCY` 80 → 280–380 ms is unverified against the 300 ms AirPlay redundant-audio constant (memory `carplay_audio_latency_config`).

**Kill fact.** Windowed SIMO with ≤ 300 ms future context on full40 through the direct-route gates collapses to the causal frontier (guitar/strings ≤ .12, entry p10 < .60, swings > 10/40) → no streamable model reaches the oracle rows; do not buy.

**First experiment.** (A) ~1 day of the idle 5080: `bsrnn_reference_eval` + `build_direct_object_host.py` with a `--lookahead-ms {0,100,300}` windowing wrapper; report the frozen gate table beside the SIMO 14/14 and causal-reference rows. (B) 30 min read-only in the car: ASIX dongle into the console USB with CarPlay running, `ifconfig en0`, `pidin | grep devnp-asix`, `pfctl -sr | grep en0`, `nc -z 172.16.250.10 9000`.

**Verdict: DO NEXT** — (A) now in parallel, hardware only if (A) passes and the structural rear reads "wider, not behind".

### 2.4 Atmos-corpus control teacher (learn the professionals' per-band rear) — DO NEXT (static profile); learned controller PARK

**What changes.** The target. Every net in the car was trained to predict a vetoed, capped "eligibility" and judged by gates that forbid a louder rear [offline, baseline §2.11]; nobody ever defined what a good rear is. Capture Apple Music Atmos renders on the Mac (BlackHole 16ch, free; Music renders 7.1.4 to a 12-output virtual device per RME forum evidence), pair with the stereo master, compute per-band rear/front log-ratio, pan and Ls/Rs MSC at the existing 93.75 Hz control grid. Stage 0 = corpus medians as env values for the structural rear (REAR_FLOOR/AIR/SURROUND/body/HP) instead of guesses. Stage 1 = GRU160 on the existing 368-feature / 264-target npz contract (same HSSRNEO1 ABI, V11 and mask retired, −42 M MAC/s −1.33 ms/control).

**Why incredible (honest).** Wow 3: the Spotify/non-Atmos rear acquires the level, 250 Hz–1 kHz body, treble and verse/chorus breathing of Apple's renders — "as full and as wide as Apple's render", not instruments behind; it learns the average professional, and pros put vocal doubles/reverb behind (owner's 09-13 policy forbids it).

**Evidence.** [car] APAC-65 plays Apple's rear end to end; [code] npz contract `q6_hssr_moises_cache.py:12,130`, `train_hssr.py:643,683`; [code] `AUDIO_SPATIAL_TAP` one-shot 6-ch dump exists in diagnostics builds only [verified `audio_hook.c:6368-6371`] for the one-track Mac-vs-phone validation; [offline] AlbumDB: immersive RMS 8 dB below stereo, authors put lead-guitar delays rearward.

**Feasibility / cost.** 40 tracks ≈ 5 h ≈ two unattended Mac nights, $0; 1,000 tracks ≈ 117 h ≈ 15 Mac nights, ~120 GB (Mac disk needs Docker.raw 51.5 GB / ~/.codex 16 GB cleared, baseline §8); training minutes/epoch on the 5080 (q6d: 181 songs × 5 epochs = 2,889 s on the NAS CPU [log]). Legal: recording decrypted Apple Music output breaches the terms and is DMCA-arguable; captures and weights never enter the public repo.

**Kill fact.** On 40 artist-disjoint aligned pairs, ridge and GRU160 predict the per-band log-ratio with held-out R² < 0.3 after removing the per-track mean → the authored rear is not inferable from stereo at control level; keep the static profile, drop the learning.

**First experiment.** Two Mac nights, zero car: 40 tracks × 2 passes, align by cross-correlating the 5.1 fold with the stereo, report corpus median/spread per band (how far below the front the pros sit at 250–1000 Hz and 2–10 kHz vs our −34 dBFS), within- vs between-track variance, ridge R².

**Verdict: DO NEXT** for the profile; PARK the learned controller until R² > 0.3.

### 2.5 VENUE: synthesized ER + FDN hall as the rear's second stream — DO NEXT as Layer 3, gated by one owner question; EVENTS half PARK

**What changes.** Rear = a signal the front never contains (a causal reverberant tail fed by HP250(S) + the structural ambient term), decorrelated to MSC ≈ 0 by orthogonal FDN output taps (today 0.40 after the bed comb [offline]), present on every track including narrow masters, opening over 2–4 s with arrangement density. ~250 lines C, ≈ 4.5 M ops/s [calculated] ≈ 0.1 ms per 21.3 ms block vs the measured 1.81 ms no-net DSP [log], 180 KB rings, +0 ms latency, no nets.

**Why incredible (honest).** For a generic listener a real 3–4 (envelopment is the percept a cabin delivers; Kahle 2019: single reflections at −10/−15 dB at 70–150 ms give strong envelopment from 60–150°). For THIS owner wow 2: he wrote "not ambience, not a mix copy" (09-17) and "reverb faking not allowed" (09-13); his only wow was bright coherent objects at level; a tail cannot carry attacks (09-30 target). Wide/doubled vocals and the recording's own vocal reverb live in S and will be reverberant behind at −12..−18 dB.

**Feasibility / cost.** 2–3 days, $0; fills the two holes the proposal admits (narrow/mono masters "thin, honestly"; the −16 dB floor is a lead copy).

**Kill fact.** One replug, venue-only rear on his three reference songs: verdict "ambience/mush, not objects behind" ends it; no RT60/level/pre-delay tuning changes what a side-fed tail is. Events/choreography of per-instrument placement is already dead: needs per-instrument audio (~75 on-budget variants failed), SEND is offline with stem inputs, mixture latches worsened swings 6 → 9/20 [offline].

**First experiment.** Step 0, zero code, 5 min: with our PCM6 session playing, MMI Surround min → max → min; an audible rear change = the J525 first-B tail is live (a free venue that stacks with ours; settles the baseline's open "B live state"). Step 1: `AUDIO_STEREO_VENUE=db:rt60:predelay`, conditions A / A+V / V-only on 5 songs.

**Verdict: DO NEXT** as Layer 3 inside the structural .so, only after the owner reopens the policy; PARK events.

### 2.6 Blow up the Krait compute envelope (pinned core + lookahead, GPU side-bet) — PARK (measurement afternoon only)

**What changes.** A second int8 worker pinned via `ThreadCtl(_NTO_TCTL_RUNMASK)` (none exists anywhere in `hook/` or `common/` [verified grep]; the worker discards its `pthread_setschedparam` result [verified `hssr_control_worker.c:150`]), ~1 G MAC/s HS-TasNet-Small-class separator with 50–85 ms lookahead. GPU: `hssr_gles2_bench.c` exists, never built (no target in `bench/build_qnx.sh` [verified]), GLES2-only, no float render targets, shared with Kanzi and both renderers.

**Why incredible (honest).** Wow 2: a fuller, steadier rear comparable to the 2.17 G reference — which failed absolute gates. Whole-chip int8 envelope ~9.6 G MAC/s theoretical (2.4 GMAC/s/core GEMV-only [log]), ~1 G realistically free on one core, DRAM-bound at 16 MB weights (L2 1–2 MB) — 30× below SIMO class.

**Kill fact.** Already on file (2.17 G causal reference, 14× budget: guitar .121, strings .109, entry .804/.420, 6–10 kHz near zero [offline]). Cheap new kill: a pinned 30-min 1 G MAC/s soak during Spotify + cluster video; guard disables or HMI stutter = no dedicated core.

**First experiment (keep this).** 2 h, no reboot, no hook code: `pidin info`; `pidin -F "%a %N %p %c %t"` every 5 s for 10 min under CarPlay + Spotify + cluster video (first per-process baseline — today's record contradicts itself: "whole-unit 91–99 %" vs "busiest process 3 % of one core" [log]); `hssr_neon_bench.c` looped to ~1 G MAC/s pinned for 30 min (thermal/throttle proxy); build and run the GLES2 bench once.

**Verdict: PARK** the model; the measurement afternoon serves every other direction (structural "≤ 1.81 ms" claim, venue 0.1 ms, middle tier, sidecar fallback budget).

## 3. Combinations that compound — 3-stage roadmap

**Stage 1 — "cabin or stream or guard?" + first radical listen (today → 2026-10-14).** One diagnostics build, one parked hour, zero training: realtime stop-line grep (2.2), Atmos-on/off same-album A/B (2.2), six-channel sweep (2.1), MMI Surround probe (2.5 step 0), the 2 h `pidin`/pinned-soak afternoon (2.6). In parallel the structural rear build (proposal, 3–4 days) with the sweep's Ls−L timing turning `AUDIO_STEREO_REAR_DELAY_MS` into a signed knob; session 1 conditions A (Track A) / B (structural, 12 ms lag) / B+α / C (rear-lead, front delay = Ls−L + 3 ms, alpha 0.35). On the idle 5080 the sidecar kill test (A) runs from day 1. Proves: whether the guard silences Spotify; whether Apple's own rear is "incredible" on these doors (if not, the cabin is the wall and no stream or extractor fixes it); whether a leading rear reads "behind"; whether a dedicated core exists; whether 300 ms lookahead reaches SIMO rows.

**Stage 2 — calibrate and fill (2026-10-14 → 2026-11-04).** Two Mac nights give the professional per-band profile (2.4 stage 0) → measured REAR_FLOOR/AIR/SURROUND/body/HP values for session 2 instead of guesses; the Atmos A/B becomes the acceptance test. If the owner reopened the policy: venue as Layer 3 behind `AUDIO_STEREO_VENUE`, one lever in session 2. If the sweep found a null: bleed cancel 0.25:<offset>. Realtime middle tier ships (cheap structural rear on a strike instead of silence). Then the 28.09 blind X/Y protocol on the survivor. Proves: a stable, full, never-empty rear at a professionally calibrated level, judged blind for the first time.

**Stage 3 — objects behind, only if stages 1–2 say "strong and stable but wider, not behind" (2026-11-04 → ~2026-12-16).** Path A (if kill test (A) passed): order the ~$340 sidecar, dongle check (B), hook sidecar path + D-delay line, `ALL_OUT_LATENCY` 80 → 80+D, day-1 model = the 2.17 G reference, then the chunked-lookahead retrain on the 5080; the structural rear stays as the always-on fallback and the measured IRs give the direct route its arrival alignment. Path B (if R² > 0.3 on the 40-pair audit): 15 Mac nights of capture, GRU160 controller on the existing ABI, V11 and mask retired. Proves: whether "a specific guitar stands behind me" is achievable in this cabin at all.

## 4. Cheapest diagnostics to run before anything

1. **Spotify realtime stop line** (0 code, 0.5 day): `grep 'realtime upmix' /tmp/carplay_hook.log` after a normal Spotify drive. Look at `disables=`, `revivals=`, `ms_hist=…` on the stop line (`audio_hook.c:4229`) and any `DISABLED, 4 consecutive periods over 12000 us` (`:4301`). `disables ≥ 1` = the rear was being switched off mid-track; `ms_hist` tail ≥ 12 ms with `disables=0` = marginal.
2. **Atmos-on vs off, same album** (0 code): iPhone Settings > Music > Dolby Atmos = Automatic (hook log `APAC decoder up … 6 ch`, Apple's rear, upmix bypassed) vs Off (AAC-23 → Track A); phone mic at the headrest, ≤ 0.5 dB match. Look at: is Apple's rear "incredible" or also faint; does he tolerate the vocal doubles/reverb that pros put behind (settles proposal question 1 for free).
3. **Six-channel sweep at the driver's head** (~60 lines, 1 h parked): read Ls−L arrival (kill test for rear-lead at > 40 ms), bleed level and delay inside the L response, door spectrum (real HP/notches vs [model] 100/113/138/174 Hz), any 30–60 ms tail (first-B live). Freeze and write down MMI Surround/Sound Focus/Bass/tone, AGC off on the recorder.
4. **MMI Surround min → max → min with our PCM6 session playing** (5 min, 0 code): audible rear change = first-B tail live and stacking.
5. **`pidin info` + per-process baseline + pinned 30-min 1 G MAC/s soak + GLES2 bench** (2 h, no reboot): a core ≥ 70 % idle, zero guard disables, flat bench timing over 30 min = headroom exists; anything else kills every "more compute on the Krait" idea for good.
6. **Sidecar kill test on the 5080** (~1 day GPU, $0): windowed SIMO at 0/100/300 ms lookahead through the direct-route gates; pass = relative and absolute gates hold at 300 ms within SIMO's margins.
7. **ASIX dongle `ifconfig en0`** (30 min, read-only, only if 6 passes): confirms link, static vs `/var/dhcp`, and PF direction for hook-initiated outbound TCP.

## 5. Questions for the owner that change the choice

1. **What is "incredible"?** (a) the band plays around me — a stable, full stage with body (structural + venue), or (b) specific instruments standing behind me (rear-lead now, sidecar later, phrase-rate movement accepted). The default order is (a) then judge; if (b) from the start, the sidecar kill test jumps the queue.
2. **Wide/doubled vocals and vocal reverb behind as "width": acceptable?** Every radical direction except content-first puts them there by construction; Apple's own renders do too — diagnostic 2 lets him hear his own answer.
3. **Which app were the "faint" verdicts on?** Spotify (realtime, guard-exposed) or Apple Music, and were any of them already Atmos tracks? This decides whether diagnostic 1 or 2 is the first fact.
4. **Acceptable media latency.** Rear-lead needs 80 → ~95 ms; the sidecar needs 280–380 ms (phone/Siri unchanged). Is a visible lag on track skip/scrub acceptable for music?
5. **Hardware in the car?** A Linux box under a seat (~$340, 12 V buck, fan, 0–35 °C devkit rating, 27 s cold boot with local-rear fallback). Yes/no decides whether stage 3 path A exists.
6. **Reopen the 09-13 "reverb faking not allowed" / 09-17 "not ambience" policy?** Without a conscious yes the venue is pre-judged and should not be written.
7. **Amp flashing / CTC appetite.** Honest answer: not needed and not recommended — the J525 plant cannot be controlled from PCM6 and no readback exists; the rear speakers are physically behind him, so no synthetic pinna cues are required. Confirm he agrees to leave the amp alone.
8. **Mac nights and legal comfort.** 2 unattended nights for the profile, ~15 for the learned controller, on his working Mac; captures breach Apple's terms and stay private forever. Acceptable?
9. **What he listens with:** MMI Surround/Sound Focus/Bass settings frozen and recorded for every session, and whether the passenger seat ever matters (rear-lead is driver-seat only; for the passenger the front leads anyway).