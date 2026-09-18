# Reference options and license boundaries

Checked 2026-09-16. These are engineering references, not downloaded/admitted
release weights or evidence that our decoder matches published scores.

- Band-split RNN explicitly combines inter-band and temporal processing. This
  supports testing richer cross-frequency context rather than treating more
  output bins alone as separation capacity. It does not establish our source
  guidance, role classification or runtime. [Original paper](https://arxiv.org/abs/2209.15174).
- SCNet uses unequal subband compression to reduce cost. Its reported CPU result
  is relative to HT Demucs, not to Audi hardware, and its published separation
  task is not our foreground/supporting scene. [Original paper](https://arxiv.org/abs/2401.13276),
  [official implementation](https://github.com/starrytong/SCNet).
- Band-SCNet reports2.59Mparameters,92ms algorithmic latency and17.59GFLOP/s;
  its real-time-factor measurement uses a desktop Intel i7-13700H. Thus 'causal',
  'lightweight' and 'real-time' in this paper are not a Krait deadline admission.
  It is a stronger architectural reference, not a drop-in solution under our
  initial40ms latency/full-path target gate. The retrieved code search found an
  explicitly unofficial WIP, not a verified author-released checkpoint.
  [Official Interspeech abstract](https://www.isca-archive.org/interspeech_2025/yang25d_interspeech.html),
  [paper](https://www.isca-archive.org/interspeech_2025/yang25d_interspeech.pdf).

Q2's proposed local descriptor supervision and oracle-guidance TRAIN diagnostic
are testable interventions, not a claimed reproduction of these systems. Evaluate
learnability first, then full real training and a separate reference-to-student
quality/compute comparison. Do not present the existing H1 per-band GRU attempt
as a new architectural discovery or use a published MUSDB score as our result.

## Data and weight records

The MoisesDB publisher describes non-commercial research use underCC BY-NC-SA4.0.
Retain original attribution, dataset terms and per-source provenance in the
private research inventory. This note does not decide all legal consequences
of trained-weight distribution; no dataset audio is to be bundled/pushed with
a car package. [Publisher](https://music.ai/blog/press/introducing-moisesdb-the-ultimate-multitrack-dataset-for-source-separation-beyond-4-stems/),
[repository license](https://github.com/moises-ai/moises-db/blob/main/LICENSE).

SCNet source repository lists MIT, but code license must not be silently asserted
as checkpoint redistribution permission: a specific weight-license clarification
request was still open in the checked repository. Any actual downloaded reference
must record checkpoint origin/hash/terms separately; do not bundle an external
teacher with the target runtime by default. [Official repository](https://github.com/starrytong/SCNet),
[weight-license issue](https://github.com/starrytong/SCNet/issues/35).

Existing private NAS offline teachers are being inventoried before any additional
download. Their outputs remain estimates, never relabelled as isolated-source or
musical-role ground truth. Evaluate a common source grouping explicitly when a
teacher supplies only four stems; it cannot establish12-head instance recovery.

## Independent electronic coverage candidates, not admitted data

Cambridge-MT lists genuine electronic/dubstep multitracks, but its current FAQ
explicitly requires contributor permission for research and individual licensing
for AI training. A free mixing-practice ZIP is therefore not assumed authorized
training/evaluation material. No such audio was downloaded in this audit, and
no artist was contacted on Luka's behalf. [Publisher FAQ](https://www.cambridge-mt.com/ms3/mtk-faq/).

MedleyDB's publisher provides non-commercial research terms and a sample; full
MedleyDB/2.0 access uses a Zenodo request. Inspect exact accessible material,
artist/source overlap and license provenance before admission; it is not yet a
replacement confirmation set. No access request or personal information was
submitted. [Publisher downloads/terms](https://medleydb.weebly.com/downloads.html).
