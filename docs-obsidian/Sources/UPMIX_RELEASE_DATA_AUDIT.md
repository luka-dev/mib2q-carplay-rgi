# Release data and exposure audit

Snapshot: **2026-09-16 06:44 UTC**, private NAS `100.85.38.96`.
`B=/mnt/shared/users/luka/Audi/upmix-training-2026-09-11`.
The [machine-readable inventory](UPMIX_RELEASE_DATA_AUDIT.json) contains all240
song IDs,43normalized artists,2585original source identities, declared labels,
native headers, historical source hashes, split membership and proof bindings.
This audit freshly checked the catalog and **240 original metadata JSON hashes**,
not original WAV bodies or audio. No PCM—including final-test PCM—was decoded;
no remote files changed and no training, inference or downloads ran.

## Immediate conclusion

There is enough existing source material to progress beyond repeated six-song
pilots. The principal missing supervision is **musical role**, not disk space:
the catalog has zero human-reviewed or musical-placement-confirmed tracks.
Instrument identity, lead/backing metadata, waveform reconstruction and permission
to place something behind the listener are separate labels. A lead synth, a
secondary guitar and a sustained pad must not become rear-safe simply because
their instrument family was separated successfully.

## Live compute and preservation

NAS is idle: no matching Luka training/evaluation/supervisor processes. The
Ryzen3 2300X has4CPU cores,8.25GB RAM,7.43GB available, and7.549TB free disk
(7.03TiB). Existing environment is
`B/runtime/c2-teacher-env-a/venv-cpu/bin/python`: Python3.12.3,
Torch2.10.0+cpu, CUDA unavailable. A DRM device exists but is **not** evidence of
a supported training accelerator. No install or GPU-driver changes were made.

The Mac has Apple M3Max,14CPU cores and36GiB memory; Torch2.10.0 reports MPS
available. This is not a throughput/compatibility qualification. In particular,
existing Q1 uses float64/complex128 feature operations, so MPS requires a separately
tested backend/precision strategy; silent CPU fallback is not a speed claim.
Existing NAS CPU is the verified execution option. Prior safe limits were two
threads,4GiB RSS,2GiB available RAM and50GiB disk reserve. A new full training run
still needs an explicit wall/update budget; no cloud spending, new hardware or
unlimited extension is inferred by this audit.

Q1 data/run/cycle/summary owners are retained unchanged. Thirteen run snapshot
files and six data-stage files freshly matched their sealed manifests. The
completed cycle report remains
`7755ee4d5236221dc8eeda57c02a6e5bce625fa79526dcd5e80cd9bb293af2ff`;
this is an underfit diagnostic, not an accepted release model.

## Source coverage and honest split

Canonical catalog:
`B/project/build/reference-audio/v16-moises-catalog-sha-a/manifest.json`, SHA256
`ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d`.
Keep seed `upmix-v16-moises-artist-split-20260911-v1` and original song/source
groups. Artist sets are disjoint; the catalog has no cross-partition duplicate
song quarantine. Exact-hash groups do not rule out unrecorded aliases, alternate
mixes or transformed duplicates across other datasets.

| Existing partition | Songs / artists / sources | Exposure policy |
| --- | ---: | --- |
| TRAIN |181 /28 /1954 | Already development-exposed; all represented in G8/G12/G17 caches. Keep TRAIN. |
| DEV (`validation`) |40 /9 /400 | Repeated oracle, role, reconstruction and model evaluation; valid reused DEV, not untouched TEST. |
| Reserved heldout |19 /6 /231 | Preserve as final-test candidates; no audio opened here. Do not optimize or mine examples from it. |

Freshly bound G8/G12/G17/H1 and full-cache reports all declare heldout unopened.
That plus the catalog claim is positive provenance, **not an exhaustive audit
of every historical process or human listen**. Before final release, reconcile
the experiment/access ledger; if a reserved song was exposed, mark it exposed
and obtain genuinely independent replacement material—never relabel an old
assessment subset “virgin”. Keep a sealed manifest and final evaluator separate
from training/development selection. Current Q1's2ASSESS songs and R1's2ASSESS
songs are all historical TRAIN and cannot replenish final TEST.

TRAIN has187 lead-vocal,84 backing-vocal and2unknown-vocal source labels;
45songs contain both a declared-no-bleed lead and backing source. There are
339guitar,133other-keyboard,97percussion and32generic-other tracks; explicit
types include41synth leads and35pads. Among1954TRAIN tracks,1781 are declared
no-bleed isolated-source candidates. These declarations are not fresh listening
or stem-purity approval. Supporting/foreground, co-lead, hook and time-varying
role intervals remain missing.

Generic FX metadata includes processed sounds/scratches/explosions, not a clean
“noise-only texture” class. Hats/shakers stay percussion; do not duplicate their
targets into noise. Full drumkit/drum-machine/overhead stems can contain hats:
absence of an explicitly labelled hi-hat stem is not proof of acoustic absence.
Unknown/mixed sources require masked labels or an explicit unknown source head.

| Conservative native44100 common-span candidate | TRAIN | DEV | Reserved TEST metadata only |
| --- | ---: | ---: | ---: |
| Fully mapped Q1-like broad families; all sources declared no-bleed |64songs /21artists /3.35h |17 /7 /1.04h |9 /4 /0.59h |
| Same, additionally equal original lengths |50 /18 /2.47h |16 /7 /0.98h |6 /3 /0.37h |
| Explicit generic-other/unknown identity head allowed, still all sources no-bleed |81 /25 /4.44h |19 /8 /1.17h |13 /6 /0.84h |

These are metadata eligibility counts, not selected PCM, semantic admission or
a new split. Common-span crops stay within the intersection of original source
lengths without padding, SRC, independent gain or time shifts. The old Q1
59-song set used an additional76-second horizon and omitted `other_plucked`.
The64-song set adds four shorter songs and one plucked-instrument song;
no activity/quality-driven replacement was performed.

Across all181TRAIN songs, the shared no-padding span totals38,916.766s
(10.81h). Historical maximum-tail accounting was39,107.067s (10.86h); the
difference is unequal source lengths, not lost music. The572cached8s windows
cover4576unique seconds—11.70% of the historical timeline. Repeating these
windows is not whole-song training or measured optimizer coverage.

## Prior corpora and exposure

HSSR/v15's587clips/50user-library songs and teacher outputs are development
material with unresolved semantic labels, not clean source truth. The fresh
`v15-complete-audit/dataset-report.json` hash is
`6b9e3a41dfcba3afbf6d4e8a5e14f162e1544ccbcfd29379753f40903becec61`.
G early24-song/8DEV and two-song learnability diagnostics, later full TRAIN,
and H1's two-song pilot remain prior exposure. See the
[retrospective](UPMIX_RETROSPECTIVE_AND_STEREO71.md) and
[timeline evidence](UPMIX_H1_GENRE_AGNOSTIC.md).

AlbumDB masters/stems remain available: the completed822-WAV extraction is
`B/external/albumdb-stems-19683001-b`, not failed `-a`. Existing whole-song split
is01/04/05/06/07/08/09 TRAIN,02/03 DEV,10reserved; later P1 used five TRAIN
optimizer songs and two nested TRAIN assessment songs. This collection is not
artist-disjoint. The nine derived5.1 research conversions and two native5.1
imports retain `role_admission=false`; authored rear mixes are not automatically
our desired source-role targets. Their exact metadata hashes are in the JSON.

## Attainable next data implementation

Create a new full-timeline **index**, retaining all181TRAIN/40DEV/19reserved
identities and every native source header/hash. A bounded reader can then stream
contiguous native-rate crops with real past, explicit core boundaries, disjoint
source-family aggregation and unknown masks. Verify original body hashes before
admission, cache that proof against signatures, and recheck before completion;
do not decode TEST during preparation. Track actual seconds/source/artist
exposure rather than only updates. Retain short, quiet and absent cases under
predeclared rules, not activity-based replacements.

Use the81all-no-bleed TRAIN songs for the first full-timeline identity-target
corpus with the now-adopted explicit unknown head; retain all181 in the index for
later controlled clean-source mixtures and partial-label admission. This is not
permission to use contaminated targets from the remaining100songs. Source-local
detector targets must distinguish incomplete labels from negatives. A separate
role layer still needs TRAIN/DEV interval annotations; defaults should preserve
unknown/foreground material at the front.

An index plus bounded read cache avoids duplicating roughly10.8h into dense
multi-head arrays. Dense twelve-target stereo+mix+residual float32 storage would
be about192GB for the shared TRAIN timeline, before spectral caches; sufficient
disk exists, but duplication is unnecessary. Electronic coverage remains only
6TRAIN/8DEV songs and zero heldout songs labelled electronic; there is no
explicit dubstep metadata label. Additional independent, authorized electronic/
dense-production test material and genuine noise/texture-role labels are needed
before claiming genre-agnostic release quality. Neither more updates nor the
19-song holdout alone closes those gaps.

## Authorized follow-on data qualification, not training

At07:00:01UTC a separately authorized fresh owner
`B/runtime/q2-source-data-a` completed in30.16s;8NAS tests passed, peak RSS639MB.
Its index preserves all240identities and admits81TRAIN/19DEV for complete
track-category reconstruction. All19TEST identities stay sealed; no TEST PCM
was read. The initial64-song stratum is retained, not overwritten. The reader
uses maximum native song length with explicit zero extension after each shorter
source's declared end; it does **not** truncate tails to the common-span counts
in the earlier eligibility table.

The twelve disjoint identity heads are `lead_vocal, backing_vocal, other_vocal,
bass, drums, high_percussion, percussion, guitar, keys, strings_winds,
other_plucked, other_unknown`. Exact source reconstruction masks are distinct
from conservative detector-known masks. This does not create acoustic absence
or musical permission labels; `other_unknown` is explicitly not noise truth.

Four distinct TRAIN artists were selected before PCM inspection by greedy
new-category coverage, then saltedSHA ties. Fixed6s cores begin at
`min(58s, song_end−6s)`, with2s real past; no activity search or failed-crop
replacement.53original WAV body hashes and metadata were verified. The
100.97MB compressed bundle holds172.03MB decoded arrays; originals remain NAS.
Coverage is11/12source categories (`other_vocal` absent), not eleven demonstrated
active categories at the chosen time. Five actual arbitrary-origin/end SRC
probes are byte-exact against whole-song unit-gain float64-summed44.1→48kHz
resampling; native tail comparison is also exact. No optimizer ran.

Proofs: index `f0f8f562d21ea26af9908518045423835ad109173ada2abdb906d5e0fe067ccf`;
bundle manifest `3f05e50719fa3f13da3680a95056a0d42ccad37366919b3635ca7053a181edfc`;
report `4fb8be13962c1f61cc055385a52df4177c5736b4e5dbc85cf896d3dd87a57d7b`.

The authorized local qualification copy is
`output/q2-source-fit-input-a/manifest.json`:4NPZs plus proof metadata,
104,137,033bytes total. All file and decoded-array SHA/header checks passed.
Copy-proof SHA is
`eca0e6310edbf640f76d1b874478e3d1e580aad895928ec865ac70696af2c62e`.
A separate post-completion dependency-resolution sidecar preserves the old
reports: NumPy2.5.3, SciPy1.18.1, SoundFile0.14.0, libsndfile1.2.2, module body
hashes and default Kaiser5.0 resampling window. Its SHA is
`17ffde7f1e8422f5b8ff3dc7fcddea6a310e38a2e1f043bf76874a8d410887be`.
It is a fresh same-interpreter/cwd/environment probe, not introspection of the
already-finished worker.

## Existing stronger offline references

The exact local checkpoint/config bytes under
`B/project/build/hssr-teacher-models` were freshly hashed; see JSON for all8hashes.
Available models are HTDemucs6, BS-RoFormer ep317 all-vocals, karaoke Mel-Band
RoFormer, and MDX23C DrumSep. No SCNet or VocalRep weight is present in that
inspected directory; download-catalog entries are not downloaded assets.
`r1_teacher_pilot.py` and `prepare_specialist_teacher_audit.py` preserve the
existing X-only teacher paths and their limitations. No inference or download
was performed for this inventory.

A fair immediate four-output reference is Demucs6 folded to vocals/bass/drums/
other (guitar+piano+other). Q2 folds indices `[0,1,2] / [3] / [4,5] / [6..11]`
against exact Moises stem-category sums. This is a declared MUSDB-style comparison,
not the old Q1 voice/percussion/tonal/noise taxonomy and not a12-source oracle.
BS-RoFormer can benchmark all-vocals; karaoke output/remainder does not establish
clean lead/backing truth. The prior real hi-hat Demucs→DrumSep result had NMSE1.56
and is a useful error baseline, not approved source supervision.

Runtime `c2-teacher-env-a/venv-cpu` has audio-separator0.47.0 with package metadata
MIT. No weight-specific license file was bundled in the inspected directory;
checkpoint permission/provenance therefore remains **unverified here**, separate
from package/code licenses. Historical public download references and scores in
filenames are neither blanket redistribution permission nor measured quality
on this corpus.

## Full clean-source integrity and timeline coverage

The later, separately authorized body audit completed at
2026-09-16 07:11:40 UTC: **991/991 TRAIN+DEV original WAVs**, 54,274,830,060 bytes,
all full-body hashes and native headers matched. It decoded no PCM and opened
no TEST file. Runtime231.712s; peakRSS49.4MB. Fresh report is
`B/runtime/q2-source-body-audit-a/report.json`, SHA
`547b50818fb6b2512f459dbb64e265a5c9dc77bc3e3059b57463c3d226302ef0`;
verified local copy is `output/q2-source-body-audit-a/`.

Exact complete-source TRAIN coverage is81songs/25artists/813sources,
713,400,013native timeline frames (4.493575h); DEV is19songs/8artists/178sources,
185,575,719frames (1.168907h). These use each song's maximum source endpoint,
not its common span. Zero-extending declared shorter tracks preserves an
additional176.495s TRAIN and1.930s DEV beyond common-span truncation. The shared
48kHz resampler yields776,489,835TRAIN and201,987,183DEV frames, respectively.

Per-identity source/song/artist and conservative detection-known counts are in
`docs-obsidian/Sources/UPMIX_RELEASE_DATA_Q2_COVERAGE.json`. Backing vocals cover36TRAIN songs;
high percussion50; auxiliary percussion39. Other-vocal and other-plucked each
have only one TRAIN song and no DEV positive; they cannot support broad
generalization claims. Generic other remains unknown identity, not noise truth.
Counts reflect metadata, not inspected audible activity or support/rear roles.

Existing Keychain-backed read-only SMB access did not succeed. A no-UI NetFS
attempt returnedEPERM; an exact NAS/private/account Keychain lookup then timed
out without returning or recording a password. No volume was mounted and no
further Keychain access is planned. The empty new mount directory and failed
attempt proofs remain preserved. The authorized alternative is a finite,
SHA-bound SSH crop reader; full source audio remains on NAS.

That alternative is now qualified:14NAS tests passed and3fixed TRAIN requests
returned correct source identities, phases, masks and byte-exact full-crop
arrays; the finite worker completed and exited. Subsequent timing-only probes
measured about29s network/protocol cost for a33MB crop versus0.884s NAS
forward/objective/backward per128-frame reference-model chunk. See
`docs-obsidian/Sources/UPMIX_Q2_SOURCE_BRIDGE.md` for complete code/report bindings and limitations.
No full-corpus optimizer, DEV PCM, TEST PCM or vehicle deployment was run by
these audits and qualification probes.
