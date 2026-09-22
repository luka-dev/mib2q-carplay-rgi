# AlbumDB: supplied AES paper and explicit 5.1 import

2026-09-14. Supplied local PDF was read in full; Table2 and section2.3 were also
checked in the rendered page. The repository wrapper adds one page: Table2 is
PDF page5 / manuscript page4. Paper: McKenzie, Moliner, Wright, AlbumDB, AES
Europe2026, paper10278 (author manuscript dated May12).

Local source: `/Users/luka/Downloads/2026_McK_Dataset_AES_Europe_2026_2_.pdf`.
SHA256 `27914e7c99d736f82be001cfee9fdbb21ad5997d2c2362ac50fd636482034b2b`.
[Publisher-hosted manuscript](https://www.research.ed.ac.uk/files/656673398/2026_McK_Dataset_AES_Europe_2026_2_.pdf).
The supplied file was necessary after online retrieval returned403; no access
restriction was bypassed. It includes a repository cover, so its digest/size
need not equal another rendering of the accepted manuscript.

## What the paper resolves

Table2 explicitly gives loudspeaker numbers for the delivered master formats,
not just the number of WAV channels. Adopt the following **publisher-declared**
12-channel order, not a guessed WAVEFORMATEXTENSIBLE order:

| File index (1-based) | P1 name | Azimuth / elevation, degrees |
|---|---|---|
|1 /2|FL /FR|-30,0 /+30,0|
|3|FC|0,0|
|4|LFE|not applicable|
|5 /6|SL /SR|-100,0 /+100,0|
|7 /8|BL /BR|-140,0 /+140,0|
|9 /10|TFL /TFR|-45,45 /+45,45|
|11 /12|TBL /TBR|-135,45 /+135,45|

This closes the missing documentary assignment. It does NOT independently prove
that every exported file has correctly wired speaker identities. Full pinned
archive/file integrity and actual WAV geometry must be checked separately.
Existing historical header/catalog reports remain immutable and correctly say
that channel count alone could not supply this evidence.

**A real file exception to the paper was found before conversion:** the immersive
`06 - Let The Light In.wav` is PCM24, not PCM16. It is293,527,914 bytes, SHA256
`3f829f6f1258be20fcaf14fc0e0992ce86bf27142710965e3361cc3ff3b2c7e0` in the acquired
publisher archive. The other19 master headers are PCM16; all20 are44.1kHz and
unmasked RIFF PCM. The importer pins this per-song/per-format exception rather
than accepting arbitrary bit depths or truncating06 to16bit. A PCM24 low-bit
preservation test checks the conversion against an independent whole-file result.

The authors recommend front heights9/10 into the front pair and channels
5/6/7/8/11/12 into the surround pair for a5.1 derivative. They do **not** specify
numerical coefficients. Their prose also says backing guitars between pairs
1/5 and2/7, an asymmetric example relative to Table2. Do not infer routing from
that example or silently repair it; use the explicit numbered table.

## Consequences for training, not just channel layout

- Stereo was mixed first from **Mixed** stems. Immersive was mixed later from
  **Mixed-Dry**, retaining existing EQ/dynamics/panning and some Haas delays, with
  separately built spatial reverbs and delays. These are two author mixes, not
  a verified sample-exact stereo downmix pair. Matching durations is insufficient.
- Mixed-Dry is not universally free of every delay: the paper explicitly retains
  Haas widening. Raw mono sources are unpanned; Mixed/Mixed-Dry stems are stereo.
  Do not treat width as permission to send lead/primary guitar to the rear.
- The author places kick/snare/rim/bass/vocals and some lead guitar in FC; keys,
  secondary guitars, strings and additional percussion populate other pairs.
  Spatial reverbs surround the listener; lead-guitar delay sends can be rear.
  Thus authored rear is useful reconstruction supervision, **not certified
  lead-free role supervision**. The user's stricter foreground policy still wins.
- Immersive RMS is reported8dB below stereo to preserve headroom/dynamics.
  This is neither a bass boost nor proof that multiplying by+8dB aligns waveforms.
  Our import applies no+8dB boost and uses only one later shared attenuation.
- Ten indie rock/pop/folk songs by one artist are additional useful material,
  not evidence of genre-agnostic behavior or dubstep coverage. The paper itself
  identifies the small dataset as a complement/fine-tuning/evaluation resource.
- MEGAMI is demonstrated as an automatic mixer of supplied stems, not a tested
  stereo-to-5.1 upmixer. Its30s example and speech-derived reverb metric do not
  establish that our separator or vocal-protection tests should pass.

## Implemented explicit derivative

New isolated modules `albumdb_paper_contract.py` and `prepare_albumdb_51.py` leave
all running training code, historical controls and original audio untouched.
The source PDF hash binds the manually checked channel declaration. The existing
P1 numerical fold is frozen explicitly (a research choice, not an AES standard):

```text
h = sqrt(0.5)
FL5 = FL12 + h*TFL12
FR5 = FR12 + h*TFR12
FC5 = FC12
SL5 = h*SL12 + h*BL12 + 0.5*TBL12
SR5 = h*SR12 + h*BR12 + 0.5*TBR12
LFE5 = 0
```

The original LFE remains in untouched12-channel masters. Zeroing the derivative
is the already chosen P1 policy, not a claim that original bass is absent. The
later front-folded model folds FC into L/R and keeps PCM C/LFE zero; it is not an
inverse of the measured or unmeasured physical J525 speaker transfer.

Preparation rechecks both publisher ZIP MD5s and20 master-file SHA256/header
geometries. Only songs1–9 immersive audio are decoded/derived/resampled to48k
FLOAT; song10 remains heldout, integrity/header only. Original frozen splits:
7TRAIN,2DEV,1heldout, shared across every stem/master/format, not artist-disjoint.
Input stereo for research is a known downmix of the SAME derivative; separately
mastered stereo is retained but not passed off as an exact pair.

Fixed linear fold precedes the qualified160/147 FIR conversion. Entire files,
true-edge padding only, no independent channel normalization, no crop/loop/time
stretch. Generated PCM is independently reread and hashed. One common gain caps
canonical target and known-downmix sample peaks at0.8; this is not true-peak,
speaker-excursion or cabin safety certification.

`candidate-catalog.json` retains roles=pending and the normal loader refuses it.
`research-catalog.json` permits ONLY explicit `authored_reconstruction_only` use,
with cabin/role approval false. No training or vehicle activation is implicit.
NAS bounds: one-hour preparation budget,100GiB reserve plus8GiB output allowance;
fresh private outputs, originals and interrupted partials retained.

## Verification / remaining work

Seven new tests check independent12-impulse routing, wrong-paper rejection, exact
split/version selection, whole-file PCM/SRC equality, source-tamper/heldout/output
guards, and full synthetic preparation including default role-loader rejection.
The focused19-test source/import suite passed on Mac. Synthetic tests do not
establish actual AlbumDB audio integrity; live NAS import is a separate step.

The same19 tests passed on NAS with all13 selected source/test hashes matching
Mac. Final frozen importer `B/runtime/p1-albumdb-paper-import-c` contains81 Python
files and an independently SHA-checked private copy of the supplied PDF. Preflight
`albumdb-preflight.json` SHA256
`79b1fe513c7c9ec0ddf3fd9f08704e56f7d595414c91309c33ee842b55efff7e`.
This revision additionally rejects a changed copied paper, or an earlier checked
source/code that changes before completion; full synthetic tests exercise both
paper and late source tampering. The earlier `p1-paper-extension-tools-a` and
`p1-albumdb-paper-import-b` remain preserved, not used for real AlbumDB import.
The complete local P1 suite now passes164 tests (82.075s). This includes the
separate continuation-tool tests, not164 musical acceptance tests.

Existing independently extracted stem inventories were also compared with the
paper: all three variants have per-song counts24/25/31/35/33/29/30/23/29/13,
272 stems per variant. Frame lengths match across stems and variants within each
song; raw has mono/stereo, Mixed-Dry and Mixed are stereo. This is inventory/header
consistency, not independent waveform alignment or a new heldout quality analysis.

## Completed NAS import

Real import finished in130.894s after the two frequency training workers completed:
PID125820, output `B/external/albumdb-paper51-48k-a`. Nine full immersive decodes,
all20 master checks, all generated PCM rereads; no incomplete report. New FLOAT
PCM is2,067,162,720 bytes (1.9252GiB) plus headers/reports. The original
archives/masters remain untouched. NAS free space remained about7041GiB.

Plan SHA256 `64e01bb7251d041c8783503a11313f3c3d7b7a671cb1a7fc80d7e59216a18e1a`.
Report SHA256 `9ce0d868591950b92816ab890ed36baf47d177428de0f5cab2d8834f9df909ff`.
Research catalog SHA256
`f9f4860cb7d09a0b65860eae95df3c39160f3f8a2b492f5f84047b4a46770c5f`.
A separate process independently rehashed/verified all9 final WAVs through the
research loader; the default loader correctly refused pending musical roles.
Proof: `independent-admission.json`. Shared per-song gains range.724731–.977433;
none boosts the data. This is9 prepared research songs, not9 newly trained songs.

The full164-test suite also passed on NAS in165.501s, bound to all81 frozen source
files (`full-preflight.json`, SHA256
`fc2030c663c0d3f871f978191df3beea4ec651e6ba21cff9e97442620ce834dd`).

Current micro-protection sources were independently checked: they are raw
`vox_lead.wav` stems from songs01/07, mono duplicated exactly toL/R (side energy0),
not whole stereo masters. Their semantic purity is not human-verified. Panned
and wide lead/doubles/effects coverage must be added before any broad protection
claim, while retaining these original controlled tests.
