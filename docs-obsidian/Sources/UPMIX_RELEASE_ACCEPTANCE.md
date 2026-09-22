# Frozen release acceptance protocol v1

Declared 2026-09-16 before any new release-candidate evaluation. This is a
prospective engineering specification, not evidence that any model passes.
Machine source: `tools/stereo-upmix-render/release_acceptance_v1.json`.
Seal exact protocol AND this specification into each study's fresh owner before
candidate metrics. Old Q1/R1/P1 results are development evidence, not regraded.
Any change creates a new revision, reason and all-candidate/control rerun; it
cannot rescue an already exposed final TEST. Never edit a frozen study copy.

## Scope, identity and coverage

All non-CABIN gates are mandatory for installation-ready controlled-car status;
CABIN is additionally mandatory for an acoustically validated release. Lack of
physical hardware or listener evidence is UNVERIFIED/BLOCKED, not a waived gate.
Current thresholds are conservative product engineering budgets, chosen before
new scores. They are NOT universal audibility/clinical/psychoacoustic standards.
Real blinded listening and car evidence are required precisely because electrical
thresholds cannot establish the user's spatial experience.

Partition/exposure audit must bind normalized artist, song/alternate-version group,
source-file hash and available PCM-duplicate evidence. TRAIN/DEV/TEST components
are disjoint including all mixtures, crops, effects and augmentations. Historical
TRAIN stays TRAIN; previously inspected DEV cannot become untouched TEST. Missing
historical records prevent an unqualified untouched claim. Do not decode sealed
TEST for development selection, labels, feature fitting, architecture or previews.
Independent annotation preparation is logged as exposure and cannot leak scores
or candidate-selected examples to development. If no defensible TEST exists,
obtain independent confirmation material; do not reshuffle old songs.

Final coverage minimum:12songs/4artists, with each supported positive source
family and protected role represented by at least3songs/2artists. Required roles
include lead/co-lead/double/wordless hook, principal guitar/synth/growl and verified
supporting vocals/percussion/guitar/synth/textures. Required conditions: sparse,
dense acoustic/electronic (including dubstep), mono/narrow/hard-pan, overlap,
silence/quiet and rapid section/role changes. Report exact uncovered cells as
UNVERIFIED; an overall count cannot fill a cell. Controlled authored mixtures
may establish their own role intent but do not establish natural-song role truth.
No genre-selected permissions. Separate curated metadata, human contextual role
judgments, source isolation truth and authored multichannel placement truth.

Exclusions are listed in JSON. Log exclusions/reasons before candidate scoring;
never silently discard a difficult or failing song. Unknown role/bleed/target
does not become an absent source. Relative metrics require target core RMS at
least-60dBFS and target/mix energy at least-40dB. Below either boundary, report
absolute error/false-output and quiet status; never a relative-quality pass.
These numeric floors define reproducibility, not perceptual audibility. Include
quiet controls explicitly and publish eligible/total denominators.

## Full-training and comparison contract

Bind exact data, code, optimizer, seeds, rate/gain/augmentation, schedule and
checkpoint hashes. Record unique song/source/minute coverage and repeated update
exposure separately. Every admitted TRAIN song and required labeled class must
be visited; the plan declares full-timeline sampling and at least one complete
coverage pass. This alone is not convergence. Convergence evidence includes
TRAIN/DEV learning curves across at least5checkpoints after coverage, gradient/
collapse diagnostics and a bounded LR/budget diagnostic where fit is weak.
Do not call a predetermined epoch count 'fully trained' without this evidence.
Select with DEV only. Architecture comparisons match data, initialization policy,
update/audio exposure and stated parameter/compute budgets; final quality/compute
tradeoffs are a separate comparison. No repeated unsuccessful extension without
a new diagnosed hypothesis. Keep rejected checkpoints and interrupted owners.

Controls: latency-aligned stereo/front preservation; simple declared DSP; mute;
one gain per source family fitted on original TRAIN only and frozen thereafter;
relevant available HSSR/P1/R1/Q1 frozen artifacts; equal-budget guided/disabled
guidance; true-source oracle and available stronger offline references explicitly
marked non-deployable. Different target vocabularies are mapped transparently to
the common declared source/render domain, not compared via unlike losses.
Unavailable artifacts are documented; missing relevant required comparison is
unverified until reproduced or its irrelevance is justified before scoring.

## Numerical definitions and rationale

Compute in aligned linear PCM at original/common gain, never independent source
normalization. Use per-song, per-channel and per-family/role/condition distributions,
worst cases and complete eligible counts. Unweighted song statistics prevent long
or loud songs dominating. Metrics are evaluated on full natural transitions and
continuous excerpts, not selected easy active frames. Fixed prefix/finish geometry
is part of the manifest; do not repeat Q1's unused-preroll ambiguity.

- Source NMSE = sum((estimate-target)^2)/sum(target^2), no SI rescaling. Recovery
  gain is10log10(controlNMSE/candidateNMSE) using the best available declared
  deployable baseline per song/family, including TRAIN-gain/mute controls. Require
  median>=3dB (half the squared error) and song-cluster bootstrap95%lower bound>=0;
  worst active song NMSE<=.8 rejects 'mostly mute'. Bootstrap10,000resamples,
  seed20260916; clustering includes all versions/crops of a song. Do not treat
  repeated frames as independent observations.
- Protected isolated output: rear energy / protected input energy, worst<=-30dB
  (<=.1% energy). Mixed response: rear(X)-rear(X-P), divided by known protected P
  energy; P95<=-24dB, worst<=-18dB. This is conditional sensitivity, NOT a pure
  causal source attribution; also report source projection/interference and
  redistribution cross-terms. The budgets prevent typical low-level contamination
  becoming a large rear copy; they do not promise perfect inaudibility/isolation.
- Useful support transfer is the target-aligned least-squares coefficient of
  the admitted rear estimate against the approved source/renderer reference,
  measured separately per source channel.10thpercentile amplitude>=-6dB and
  90th<=+1dB;>=80%eligible supporting intervals must be meaningfully routed.
  Coherent retained source body and timbre still need NMSE/attack/sustain checks.
  A low rear energy or always-abstain solution cannot pass.
- Foreground front transfer: compare candidate front response to X vs X-P against
  aligned stereo baseline's same difference, in both PCM and the known J525 input
  proxy. Worst absolute target-coherent gain error<=.5dB. Where cross-spectra have
  adequate energy/coherence, P95 interchannel phase-error<=5degrees; report
  unscorable cells and front residual/interference as well. Proxy is NOT cabin.
- Attacks: source-truth onsets fixed from TRAIN-calibrated detector,50ms windows.
  Sustains: remaining active100ms windows. P95 absolute RMS-envelope error<=3dB
  for both. This bounds factor-of-two energy smearing but does not certify timbre;
  also report multiresolution complex/magnitude error, interference and blind
  artifact/phrasing judgments. No changing onset threshold from candidate errors.
- Bass20–120Hz energy change: worst absolute<=1dB against aligned stereo proxy
  at common gain. No intentional new LFE boost or independent stem normalization.
  One SoundCheck/common gain, linked peak/matrix protection, all-channel finite
  output and no clipping. Test -9/0/+1dB common-gain cases as engineering probes,
  not a claimed universal Apple loudness target. Publish limiter duty/pumping and
  whole-render level changes; a common limiter must not conceal source regressions.

## Streaming, target and installation gates

Reference/native max normalized absolute difference<=5e-5 and finalS16<=2LSB
under identical frontend/windows/weights/gain; these are integration budgets,
not quantization permission to change musical scores. Re-evaluate quality after
export/quantization. Fresh reset silence must be exactzero. Test arbitrary PCM
chunks, fresh process/checkpoint reload, seeks, both finish/reset semantics,
generation changes and invalid input/model handling. Include full-song and at
least2hcontinuous stress, not only one ASSESS crop. No allocation/unbounded loops/
blocking I/O/locks in callback. Test worker stall/drop/late result, corruption,
bounded queues and latency-aligned click-free dry transitions. Audit all supported
sample formats/rates and channel order; unsupported formats fail safe explicitly.

Initial target engineering budgets:<=40ms algorithmic latency,<=32MiB task state/
weights/work memory, full-path loadedP99<=80%of actual callback audio period,
maximum<=100%, zero deadline misses/underruns during>=2hrepresentative competing
load.20%tail margin is an explicit design reserve, NOT measured freeCPU. Include
FFT/features/model/synthesis/renderer/common gain/safety/format/queue work and
separately log decoder/QSA contention, clock resolution and sample counts.
Only actual targetKrait evidence qualifies TARGET; QEMU instructions or hostMACs
cannot substitute. Record current3D OFF/Focus/Surround; document any assumption.

Package gate requires exact weights/config, hashes, dependencies/licenses,
reproducible training/export/build commands, six-slot ABI, signed-by-hash release
manifest, fresh native parity/results, preflight/backup/rollback/recovery and
failure-safe configuration. Installation remains separate user authorization.

## Blind listening and acoustic release

Generate randomized coded A/B order from a committed seed and hide identities
until judgments are locked. Use identical material/downstream processing and
one common full-render level-match per pair (<=.2dB difference); do not normalize
each stem. Include rear-only diagnostics AND full-render assessment; headphone
preview is not car spatial evidence. Minimum24real judged comparisons covering
>=12songs; repeat/order checks and hearing-condition notes are retained.
The unit is a song-balanced real judgment; repeated clicks are not independent
votes. Require lower95%Wilson preference bound>.5 (strict inequality handled by
the producer's gate value), no reported severe lead/front/bass/artifact regression.
Report actual listener count; one user's result is personal acceptance, not a
population claim. No synthetic/listener-model votes may satisfy this gate.

CABIN additionally requires authorized actual-car recordings/listening with
recorded seat, levels and MMI state, front/rear/source diagnostic comparisons,
safe playback levels and no unacceptable foreground/bass/reliability regression.
An electrical matrix, wiring drawing or headphone HRTF cannot satisfy it.
