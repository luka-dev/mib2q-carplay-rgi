# Stereo upmix WAV renderer

This host-only tool runs a 48 kHz, signed 16-bit stereo WAV through the exact
`hook/audio/stereo_upmix.c` implementation used by the CarPlay hook. It writes
an uncompressed six-channel WAV in this explicit order:

`L, R, C, LFE, Ls, Rs`

The output uses `WAVE_FORMAT_EXTENSIBLE` and the `FL|FR|FC|LFE|SL|SR` speaker
mask, so players do not have to guess the channel order. All three implemented
backends keep dry audio in L/R, write digital zero to C, derive bounded Ls/Rs,
and can synthesize a restrained bass-assist signal in LFE. `ambient-lite` uses
the low-cost six-band IIR/decorrelator path; `ambient-hq` uses the causal
1024-point/256-hop STFT path and has a fixed 1024-frame rear-field startup
latency. `hsr-td` is a clean-room causal time-domain spatial-field decoder: six
IIR bands track a real 2x2 covariance at a 6 kHz control rate and send the
unexplained residual directly to the rears without FFT or artificial
decorrelation. Native APAC 5.1 never passes through this tool or the stereo
upmixer: its original C, LFE, Ls, and Rs remain untouched.

Build from the repository root:

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror \
  -Ihook/audio \
  tools/stereo-upmix-render/stereo_upmix_render.c \
  hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
  -lm \
  -o /tmp/stereo_upmix_render
```

Render with the production candidate rear gain (about `0.18`, hard maximum
`0.25`) and explicitly enable bass assist:

```sh
/tmp/stereo_upmix_render \
  --mode ambient-lite --lfe assist --surround 0.18 \
  input_stereo_48k_s16.wav output_5_1.wav
```

Render the HQ candidate through the same output adapter and gain profile:

```sh
/tmp/stereo_upmix_render \
  --mode ambient-hq --lfe assist --surround 0.18 \
  input_stereo_48k_s16.wav output_hq_5_1.wav
```

Render the time-domain HSR candidate:

```sh
/tmp/stereo_upmix_render \
  --mode hsr-td --lfe off --surround 0.18 \
  input_stereo_48k_s16.wav output_hsr_td_5_1.wav
```

`hsr-td` starts causally in the current frame and has no block-fill latency.
It is inspired only by the public high-level time-domain/no-FFT HSR concept;
it contains no DAM Audio source or coefficients and does not claim output
parity with the proprietary product.

Render the development HSSR semantic-routing candidate:

```sh
/tmp/stereo_upmix_render \
  --mode hssr-neon --model "$PWD/build/hssr/hssr_model.bin" \
  --lfe assist --surround 0.20 \
  input_stereo_48k_s16.wav output_hssr_5_1.wav
```

HSSR uses a 512-point/128-hop spectral front end and a 2x160 quantized GRU
controller at 93.75 Hz. The model selects stable semantic regions while DSP
performs the waveform routing; it is not a waveform stem separator. Both front
and rear paths have the same 512-frame delay, and the routed rear contribution
is subtracted from the corresponding front so the canonical stereo fold-back
is preserved before limiting. Digital C remains exactly zero.

For the assertive vehicle profile, use the same model and rear headroom limit:

```sh
/tmp/stereo_upmix_render \
  --mode hssr-active --model "$PWD/build/hssr/hssr_model.bin" \
  --lfe assist --surround 0.25 \
  input_stereo_48k_s16.wav output_hssr_active_5_1.wav
```

`hssr-active` increases only semantic background/FX/spatial-percussion masks
and uses stronger matrix-aware front subtraction. Lead and low-end posteriors
receive steeper vetoes. Unlike conservative `hssr-neon`, exact canonical
stereo fold-back is intentionally not a contract for selected active objects.

To audition the object-selective vehicle profile at the same bounded rear
gain:

```sh
/tmp/stereo_upmix_render \
  --mode hssr-object --model "$PWD/build/hssr/hssr_model.bin" \
  --lfe assist --surround 0.25 \
  input_stereo_48k_s16.wav output_hssr_object_5_1.wav
```

`hssr-object` adds a temporally smoothed fine-bin candidate mask inside each
semantic band. Stable spectral peaks and matching pan components receive up to
`+4.1 dB` fine-bin makeup, the broad inter-peak bed is reduced, and a second
class-gated stage gives rearable background/FX/instrument bands up to `+3.2 dB`.
Selected rear material uses `2.0 x` front subtraction. The hard rear limit
remains `0.25`; this makes accepted objects louder without raising the maximum
SoundAmp input bound.

The persistent-object revision is independently selectable, so V1 remains a
one-line rollback:

```sh
/tmp/stereo_upmix_render \
  --mode hssr-object-v2 --model "$PWD/build/hssr/hssr_model.bin" \
  --lfe assist --surround 0.25 \
  input_stereo_48k_s16.wav output_hssr_object_v2_5_1.wav
```

`hssr-object-v2` keeps one coherent object slot in each stereo half-plane,
requires every selected ridge to match a held slot, and applies a rank-one
intensity-pan projection before rendering.  Slots hold for about 250 ms and
fine masks release over about 220 ms.  Only a quiet diffuse residual remains
outside accepted slots; exact centred mono cannot acquire an object slot.

Render the vehicle Object Focus profile with:

```sh
/tmp/stereo_upmix_render \
  --mode hssr-object-focus --model "$PWD/build/hssr/hssr_model.bin" \
  --lfe assist --surround 0.25 \
  input_stereo_48k_s16.wav output_hssr_object_focus_5_1.wav
```

Object Focus projects through V2's measured source vector and only then
hard-steers the accepted object toward SL or SR.  Opposite-side crossfeed is
about -18 dB or lower, presence/transient objects receive an object-only
0.8-11 kHz emphasis, and the diffuse bed is reduced.  The rear rail remains
bounded at `0.25`; its stronger `2.5 x` front transfer intentionally favours
rear localization over stereo fold-back fidelity.

The training/export tool consumes local MUSDB stem containers and writes only
under ignored `build/`:

```sh
build/hssr-train-venv/bin/python \
  tools/stereo-upmix-render/train_hssr.py --output build/hssr
```

MUSDB-derived weights are development-only and must not be shipped
commercially without licensing clearance or retraining on cleared stems.

The first 1024 HQ output frames intentionally have silent Ls/Rs while its
causal analysis window fills. L/R and the optional LFE are not delayed. The
renderer does not append a synthetic drain tail, matching the live hook's
one-output-frame-per-input-frame contract. A generated HQ file is an audition
candidate, not evidence by itself that it sounds better than `ambient-lite`;
compare them at matched levels.

The renderer defaults to `--lfe off`, making every audition unambiguous. Bass
assist is enabled only by the explicit `--lfe assist` above. The checked-in
vehicle startup selects assist explicitly; changing the renderer default does
not alter that deployment setting. The exact synthesis is:

```text
LFE = (5833 / 32768) * LR4_80Hz((L + R) / 2)
```

`LR4_80Hz` is a fourth-order Linkwitz-Riley low-pass made from two identical
second-order Butterworth sections at 80 Hz. Enabling it pairs the LFE gain
(`5833`, about `0.178`) with a front gain of `30802` (about `0.940`) to reserve
headroom. `--lfe off` restores the non-assist front gain of `32670` (about
`0.997`) and emits digital zero in LFE.

Render the fixed six-channel rail with the ambience generator disabled:

```sh
/tmp/stereo_upmix_render --mode off input_stereo_48k_s16.wav output_dry.wav
```

In `off` mode, the output is exactly `L,R,0,0,0,0`; it intentionally disables
the active profile's front trim so this file is also a compatibility oracle for
the hook's old widening path. This mode forces LFE off and unity L/R even if
`--lfe assist` is given. The tool reports the effective front, surround, and
LFE gains after every render.

The parser accepts integer PCM in classic WAV or PCM
`WAVE_FORMAT_EXTENSIBLE`, walks arbitrary RIFF chunks (including an unusual
`data`-before-`fmt` ordering), and rejects any input that is not exactly stereo,
48 kHz and 16 valid bits. Processing is streamed in blocks no larger than the
hook's 1024-frame realtime limit; no whole-track PCM buffer is allocated.

For a quick geometry check:

```sh
ffprobe -v error -show_entries \
  stream=codec_name,sample_rate,channels,channel_layout,duration \
  -of default=noprint_wrappers=1 output_5_1.wav
```

## Controlled multigenre 5.1 evaluation

`prepare_reference_corpus.py` creates deterministic 5.1 fixtures from open
multitrack stems, folds every fixture to stereo with
`L=FL+0.7071*FC+0.7071*SL` and the corresponding right equation, and records
the exact recipe in a manifest. `evaluate_reference_corpus.py` runs that same
stereo input through `hsr-td`, `ambient-lite`, and `ambient-hq`, then reports
rear envelope/waveform metrics, energy, image correlation, front preservation,
and fold-back error. With `--lfe assist` it additionally scores the shared LFE
bass-assist envelope, SI-SDR, energy ratio, and recovered visible SoundAmp bus
peak.

The retained tuning recipe uses seven genre-labelled tracks from the official
[MUSDB18 seven-second sample](https://github.com/sigsep/sigsep-mus-db/releases/tag/v0.4.0)
and the two tracks in the official
[MedleyDB sample](https://medleydb.weebly.com/downloads.html): 80s Rock,
Beatles-style, Britpop, Disco, Country, Gospel, Grunge, Singer/Songwriter, and
World/Folk. A deterministic filename-hash selection can add held-out MUSDB
tracks that are never used to choose HSR-TD coefficients. Download and extract
the datasets under ignored `build/reference-audio/`; do not commit or
redistribute their audio:

```sh
python3 tools/stereo-upmix-render/prepare_reference_corpus.py \
  --musdb-root "$PWD/build/reference-audio/musdb" \
  --medleydb-root "$PWD/build/reference-audio/medleydb/MedleyDB_sample/Audio" \
  --output "$PWD/build/reference-audio/hsr-td-corpus"

python3 tools/stereo-upmix-render/evaluate_reference_corpus.py \
  --renderer "$PWD/build/stereo_upmix_render" \
  --manifest "$PWD/build/reference-audio/hsr-td-corpus/manifest.json" \
  --output "$PWD/build/reference-audio/hsr-td-eval" \
  --lfe off --backends hsr-td ambient-lite ambient-hq
```

For the retained 24-track held-out validation and separate LFE report:

```sh
python3 tools/stereo-upmix-render/prepare_reference_corpus.py \
  --musdb-root "$PWD/build/reference-audio/musdb" \
  --medleydb-root "$PWD/build/reference-audio/medleydb/MedleyDB_sample/Audio" \
  --authored-5-1 "$PWD/build/reference-audio/sintel-master-51.flac" \
  --musdb-validation-count 24 \
  --output "$PWD/build/reference-audio/hsr-td-heldout-corpus"

python3 tools/stereo-upmix-render/evaluate_reference_corpus.py \
  --renderer "$PWD/build/stereo_upmix_render" \
  --manifest "$PWD/build/reference-audio/hsr-td-heldout-corpus/manifest.json" \
  --output "$PWD/build/reference-audio/hsr-td-heldout-eval-assist" \
  --lfe assist --backends hsr-td ambient-lite ambient-hq
```

The report keeps the nine development fixtures, 24 held-out fixtures, and
authored-film row in separate aggregates. LFE is not present in the stereo
fold-down as a discrete channel, so its score measures how well bass assist
follows the reference low-frequency envelope; it cannot prove recovery of the
original LFE channel. Exact retained results are in
[`bench/results/2026-09-04-hsr-td.md`](bench/results/2026-09-04-hsr-td.md).

As a separate authored-surround check, the optional input below adds a
30-second excerpt from Xiph.org's lossless
[Sintel 5.1 master](https://media.xiph.org/sintel/sintel-master-51.flac). It is
film score/effects, not presented as another song genre, and is excluded from
the song-fixture aggregates:

```sh
python3 tools/stereo-upmix-render/prepare_reference_corpus.py \
  --musdb-root "$PWD/build/reference-audio/musdb" \
  --medleydb-root "$PWD/build/reference-audio/medleydb/MedleyDB_sample/Audio" \
  --authored-5-1 "$PWD/build/reference-audio/sintel-master-51.flac" \
  --output "$PWD/build/reference-audio/hsr-td-corpus"
```

Preparation requires Python 3, NumPy, and FFmpeg; evaluation additionally
requires SciPy and SoundFile. These fixtures are controlled spatial renders,
not artist-authored 5.1 masters. Stereo fold-down is non-invertible, and
waveform metrics penalize useful decorrelation, so the report is a regression
and comparative test rather than a listening-test substitute.

## Public authored-surround corpus

`prepare_authored_corpus.py` converts the locally downloaded Pavlik Records
stereo/5.0 pairs, Dolby Atmos sample, and EBU 5.1 channel-identification file
to deterministic 48 kHz/16-bit evaluator fixtures. It selects a 30-second
window using the geometric mean of programme and rear power, keeps the EBU
18-second test whole, inserts a silent LFE channel for 5.0 sources, and uses
the fixed `FL FR FC LFE SL SR` order.

Each item contains the authored `reference_5_1.wav`, its exact canonical
`fold_down_stereo.wav`, and a resampled `publisher_stereo.wav` when the source
provides a separately mastered stereo version:

```sh
python3 tools/stereo-upmix-render/prepare_authored_corpus.py \
  --source-manifest "$PWD/build/reference-audio/public-authored/manifest.json" \
  --output "$PWD/build/reference-audio/public-authored-corpus"

python3 tools/stereo-upmix-render/evaluate_reference_corpus.py \
  --renderer "$PWD/build/stereo_upmix_render" \
  --manifest "$PWD/build/reference-audio/public-authored-corpus/manifest.json" \
  --output "$PWD/build/reference-audio/public-authored-eval" \
  --lfe assist --surround 0.20 \
  --backends hsr-td hsr-td-active music-stage ambient-hq
```

Add HSSR to an evaluation with
`--backends ... hssr-neon --hssr-model build/hssr/hssr_model.bin`.

The source recordings and generated fixtures stay under ignored `build/` and
must not be committed or redistributed.

## Apple rock/rap/dubstep stress corpus

`prepare_apple_genre_stress_corpus.py` downloads the current official 30-second
Apple previews for four rock, four rap and four dubstep tracks. These are
stereo-only stress inputs: there is no authored-surround reference and the
result must not be reported as an Atmos-accuracy score.

```sh
python3 tools/stereo-upmix-render/prepare_apple_genre_stress_corpus.py \
  --preview-root "$PWD/build/reference-audio/apple-public-previews/genre-stress" \
  --output "$PWD/build/reference-audio/apple-genre-stress-corpus"

python3 tools/stereo-upmix-render/evaluate_stereo_stress_corpus.py \
  --renderer /tmp/stereo_upmix_render \
  --manifest "$PWD/build/reference-audio/apple-genre-stress-corpus/manifest.json" \
  --output "$PWD/build/reference-audio/apple-genre-stress-eval" \
  --lfe assist --surround 0.25 \
  --hssr-model "$PWD/build/hssr/hssr_model.bin" \
  --backends hssr-object-v2 hssr-object-focus
```

The evaluator reports rear activity, local SL/SR balance, front and canonical
fold-down preservation, clipping and the predicted visible SoundAmp bus peak.
These are comparative safety/activity metrics; listening remains decisive.

## Archive.org six-channel stress corpus

The local source manifest under
`build/reference-audio/archive-surround-sources/manifest.json` records seven
downloaded sources that were verified by `ffprobe` as real six-channel audio:
three Kelly Industries DTS demos, NIN `Ghosts 19`, one Warsaw live track, and
two CC0 `wakaranai` tracks that also provide separately mixed stereo editions.
The manifest records each source URL, checksum and license status. Material
without an explicit license is local-evaluation-only.

```sh
python3 tools/stereo-upmix-render/prepare_authored_corpus.py \
  --source-manifest "$PWD/build/reference-audio/archive-surround-sources/manifest.json" \
  --output "$PWD/build/reference-audio/archive-surround-corpus" \
  --excerpt-duration 30 --excerpt-hop 1

python3 tools/stereo-upmix-render/evaluate_reference_corpus.py \
  --renderer /tmp/stereo_upmix_render \
  --manifest "$PWD/build/reference-audio/archive-surround-corpus/manifest.json" \
  --output "$PWD/build/reference-audio/archive-surround-eval" \
  --lfe assist --surround 0.25 \
  --hssr-model "$PWD/build/hssr/hssr_model.bin" \
  --backends hssr-object-v2 hssr-object-focus
```

The generated input for objective scoring is always the exact canonical fold
of the same six-channel excerpt. A separately mixed stereo file is retained
for listening when one is available, but is not mistaken for a sample-exact
reference input.
