# Upmix Stability: "the sound goes back and front" (2026-09-25)

Two CarPlay users on the 2026-09-22 build reported that the music "goes back and
front".  This note records what was measured, what causes it, what changed and
how to check it again.  Everything here is reproducible without the car: the
host renderer is bit-exact with the hook, and the tests below run on synthetic
scenes, on MoisesDB stems (ground truth per instrument) and on ordinary songs.

## 🧪 Tools

| Tool | What it measures | Time |
|---|---|---|
| `tools/stereo-upmix-render/upmix_behavior_tests.py --check` | 41 synthetic scenes with a known "lead" (pan, pitch, timing, arrangement changes, hostile input), asserted limits, XFAIL = known defect, XPASS fails so a fix must be recorded | ~20 s, in `scripts/run_tests.sh` |
| `… --sweep`, `--sweep-time`, `--sweep-timbre` | pan × pitch × timbre × background grid (216 clips), note length × level × 3 spots, harmonics × vibrato | 25 s each, heat maps |
| `tools/stereo-upmix-render/upmix_stem_tests.py` | MoisesDB: one render per song, then every stem in turn is the lead; rear share of the stem's own T-F cells per band, back-and-forth within 60 s (`range_1s`), 250 ms flicker, level dependence | 30 songs ≈ 5 min |
| `tools/stereo-upmix-render/upmix_music_tests.py` | Any folder of songs via ffmpeg (no stems): whole-mix rear share per band, its flicker and swing, front/rear treble vs the dry mix, A/B renders kept for listening | 200 songs ≈ 3 min on the training PC |

The metric everywhere is the **rear share**: energy in Ls+Rs divided by energy
in L+R+C+Ls+Rs, computed in the T-F cells the instrument owns (its dry stem is
≥ 10 dB above everything else), per 53 ms frame.  A stable instrument has a
constant rear share; "back and front" is that share moving.

The training PC (`<training-pc>`) holds `D:\upmix\tools\upmix_render.exe`
(mingw cross build, 3 LSB max from the Mac build), ffmpeg, the models and the
three scripts; MoisesDB is at `D:\upmix\datasets\moisesdb\moisesdb_v0.1`, the
user's library at `D:\music`, results under `D:\upmix\runs\<date>\`.

## 🔍 What was found

Measured on the 22.09 knobs (retention 0.5 with the 1000:2500 Hz HF taper,
bed delays, owner attack 0.75, rear 0.40).

### Real music (30 MoisesDB songs)

Rear share of each stem's own cells by band, median over songs:

| stem | 0.3–1 k | 1–2.5 k | 2.5–4 k | 4–6 k | 6–10 k |
|---|---|---|---|---|---|
| vocals | 0.08 | 0.24 | 0.34 | 0.38 | 0.38 |
| guitar | 0.12 | 0.40 | 0.47 | 0.53 | 0.51 |
| drums | 0.07 | 0.35 | 0.26 | 0.37 | 0.38 |
| bass | 0.11 | 0.39 | 0.46 | 0.33 | 0.16 |

Every instrument is split in depth: body in front, top behind.  For a voice
that means vowels in front and presence/sibilance behind, flickering at
syllable rate — the rear share of vocal cells moved by more than 0.30 within
60 s in 24 of 30 songs and exceeded 15 % overall in 11.  This is the object
path, not the bed (removing the bed changes nothing, removing the object mask
removes all of it), and it is not transient-driven (steady frames 0.08,
transient frames 0.12).

The cause is timing, not classification.  The control model calls the voice
in **every** band (p_vocal = 1.0 from 350 Hz to 10 kHz, rearability target 0),
but three fast-attack / slow-release memories sit between that target and the
rendered object: the band mask (~0.3 s release), the slot band weight (~1 s)
and the per-bin mask (~0.13 s).  A band that just belonged to a guitar keeps
sending the next vocal syllable's top to the rear for 0.3–1 s.

The **attack detector** shipped on 22.09 (`AUDIO_HSSR_BED_ATTACK=owner`) is
the largest single jump generator on real music: with it off, the 250 ms
flicker of every stem drops 30–40 % and the within-song swing 20–30 %, before
any other change; `front` behaves like `owner`.  The HF retention taper that
rides on it adds another +0.07 of flicker in every combination.

### Synthetic scenes

* Depth followed pitch: one instrument at one position went 47 % rear at
  440 Hz and 89 % at 1760 Hz (taper + the −4 dB/500 Hz rear low shelf), so
  melodies rode back and forth with their contour.
* For high notes near the centre the front/rear decision depended on the
  lead's level against the bed (pan −0.25: rear at −6 dB, front at +6 dB) —
  loud + centred reads as lead vocal.  Weak on real songs (2/30).
* Each entry after a gap snapped to the rear within ~100 ms (`gappy` 0.69).
* Hi-hat hits were front or rear per hit; double-tracked hard L/R guitars
  flickered; the bed's rear share followed a pad's slow swell.

What held: absolute level (−40 dB), STFT hop phase, determinism, note length,
lead level at 880 Hz, two L/R objects, pan jumps, breaks, ducking beds, 90 s
runs.

## 🛠️ What changed

1. **Vocal gate** (`hook/audio/hssr_dsp.c`, knob `AUDIO_HSSR_VOCAL_GATE`
   0..1, default 0.95, renderer `--vocal-gate`).  The model's per-band vocal
   veto (`smoothstep(p_vocal, 0.5, 0.9)`, fast attack, ~0.35 s release) is
   applied to the *final* per-bin object mask, after all three memories,
   scaled by a whole-voice presence (share of the 350–2500 Hz bands called
   vocal, ~0.1 s attack, ~0.5 s release, so one band misread on a solo guitar
   does not engage it) and per bin by comodulation with the voice body
   (running correlation of the bin's log energy with the voice body's,
   ~0.2 s): vocal partials rise and fall with the syllables, a guitar sharing
   the band does not.  Pan proximity was tried first and rejected (the voice's
   own top bins have a noisy pan).
2. **Attack detector off**, taper therefore off (`AUDIO_HSSR_BED_ATTACK=off`);
   retention stays 0.5, time-domain, flat.
3. New knob `AUDIO_STEREO_REAR_BODY_DB` (−12..0, default −4 = the shipped
   shelf) for the rear low shelf, so the pitch dependence can be traded
   against low-mid in the rear without a rebuild.  Left at −4: on real songs
   0 raised guitar bodies in the rear (0.14 → 0.20) for little gain.
4. **Rear air bed** `AUDIO_STEREO_REAR_AIR_DB=-12` (see below) and the
   front-only treble shelf `AUDIO_STEREO_FRONT_TREBLE_DB` (default off).

Effect on the 12-song MoisesDB matrix (rear share / swing within 60 s /
250 ms flicker / 4–6 kHz rear share):

| | vocals | guitar | drums | front 2–16 kHz vs dry |
|---|---|---|---|---|
| 22.09 car | 0.13 / 0.62 / 0.26 / 0.50 | 0.31 / 0.51 / 0.18 / 0.60 | 0.33 / 0.65 / 0.22 / 0.49 | −5.9 dB |
| attack off only | 0.12 / 0.41 / 0.15 / 0.32 | 0.26 / 0.43 / 0.14 / 0.43 | 0.24 / 0.50 / 0.16 / 0.34 | −3.5 dB |
| **candidate** (gate + attack off) | 0.02 / 0.32 / 0.14 / 0.19 | 0.14 / 0.44 / 0.17 / 0.32 | 0.14 / 0.37 / 0.16 / 0.30 | −2.7 dB |
| gate with hold, no comodulation | 0.00 / 0.30 / 0.13 / 0.08 | 0.07 / 0.49 / 0.17 / 0.20 | 0.07 / 0.49 / 0.16 / 0.23 | −1.9 dB |

The candidate keeps the voice in front (0 of 30 songs above 15 % rear, from
11), halves the vocal top leak, cuts the 250 ms flicker of every stem by
30–45 % and the within-song swing of drums by 40 %, and leaves guitars and
drums about 30 % rear in their top bands (the "bright objects behind" of the
21.09 listening).  The price is a front that is 3 dB brighter in 2–16 kHz
than the 22.09 car (the taper is gone); lowering retention to 0.35 buys back
0.6 dB at +0.03 flicker, so it was left at 0.5 for the first car test.

Verified on the training PC with the Windows build on all 30 songs
(`D:\upmix\runs\2026-09-25\stems_car`, `stems_cand`), medians:

| stem | rear share 22.09 → candidate | songs with a > 0.30 swing | 53 ms std | own-cell rear 2.5–10 kHz |
|---|---|---|---|---|
| vocals | 0.10 → **0.01** | 24/30 → **10/30** | 0.18 → 0.09 | 0.34–0.38 → 0.06–0.20 |
| guitar | 0.21 → 0.07 | 20/28 → 16/28 | 0.15 → 0.12 | 0.47–0.53 → 0.21–0.25 |
| piano | 0.20 → 0.05 | 8/12 → 6/12 | 0.12 → 0.09 | 0.30–0.43 → 0.14–0.20 |
| drums | 0.26 → 0.14 | 28/29 → 22/29 | 0.20 → 0.13 | — |
| bass | 0.16 → 0.06 | 3/30 → 3/30 | 0.08 → 0.05 | — |
| percussion | 0.41 → 0.15 | 4/6 → 4/6 | 0.26 → 0.18 | — |

Songs leaking more than 15 % of the voice to the rear: 11 of 30 → 0.

### The user's library (200 songs from `D:\music`, no stems)

`upmix_music_tests.py`, medians over songs, whole mix (`D:\upmix\runs\2026-09-25\music`):

| | 22.09 | candidate |
|---|---|---|
| 250 ms flicker of the 2.5–10 kHz rear share | 0.141 | **0.064** |
| swing of that share within 60 s (1 s medians) | 0.52 | **0.27** |
| songs with a swing > 0.30 | 162/200 | **86/200** |
| rear share 300 Hz–1 kHz / 1–2.5 k / 2.5–4 k / 4–6 k / 6–10 k | 0.12 / 0.19 / 0.14 / 0.16 / 0.05 | 0.01 / 0.03 / 0.05 / 0.08 / 0.04 |
| rear 2–16 kHz vs dry | −11.2 dB | −13.6 dB |
| rear 300 Hz–2 kHz vs dry | −11.4 dB | −18.3 dB |
| front 2–16 kHz vs dry | −2.7 dB | −1.0 dB |

The stability gain is large and general (flicker halved, swing halved), and so
is the price: on ordinary masters the vocal owns most of the low-mid, so the
rear loses 7 dB there and 2.3 dB in the treble.  The env knobs around the
candidate on another 120 songs of the same library (`music_knobs.log`):

| | flicker | swing | songs > 0.30 | rear 2–16 kHz | rear 0.3–2 kHz | front 2–16 kHz |
|---|---|---|---|---|---|---|
| 22.09 | 0.147 | 0.50 | 104/120 | −10.7 dB | −11.3 dB | −3.0 dB |
| candidate | 0.068 | 0.28 | 53/120 | −13.2 dB | −17.6 dB | −1.2 dB |
| + surround 0.5 | 0.099 | 0.41 | 82/120 | −11.3 dB | −15.7 dB | −1.4 dB |
| + vocal gate 0.7 | 0.073 | 0.30 | 59/120 | −12.5 dB | −15.6 dB | −1.4 dB |
| + retention 0.35 | 0.079 | 0.32 | 65/120 | −13.2 dB | −17.6 dB | −1.5 dB |

Surround is the wrong lever: it buys the 22.09 rear treble back with more
than half of the stability gain.  The rear air bed below is the right one.

### Giving the rears their treble back without the jumps

The candidate's rear is darker than the 22.09 car for two reasons: the gate
takes the voice out of it (intended), and with the attack detector off the
retention is flat again, so half of every object's top stays in front.  Every
lever that returns treble through the *object* path returns the flicker with
it, because that is where the decisions live — measured on the 12-song matrix
(rear 2–16 kHz vs dry / 250 ms flicker of vocals, guitar, drums):

| | rear 2–16 kHz | flicker v / g / d | front 2–16 kHz |
|---|---|---|---|
| 22.09 car | −7.2 dB | 0.26 / 0.18 / 0.22 | −5.9 dB |
| candidate | −8.4 dB | 0.13 / 0.17 / 0.15 | −2.6 dB |
| + `AUDIO_STEREO_SURROUND=0.5` | −6.5 dB | 0.19 / 0.21 / 0.21 | −3.1 dB |
| + HF taper without the detector | −8.4 dB | 0.23 / 0.23 / 0.22 | −4.2 dB |
| + `AUDIO_HSSR_VOCAL_GATE=0.7` | −8.1 dB | 0.14 / 0.16 / 0.15 | −2.9 dB |
| + `AUDIO_STEREO_FRONT_RETENTION=0.35` | −8.4 dB | 0.18 / 0.20 / 0.19 | −3.2 dB |
| **+ `AUDIO_STEREO_REAR_AIR_DB=-12`** | **−6.9 dB** | **0.14 / 0.17 / 0.16** | −2.6 dB |
| + air −9 dB | −6.2 dB | 0.15 / 0.17 / 0.17 | −2.6 dB |

The **rear air bed** is a copy of the input above ~2.5 kHz (0 at 2 kHz, full
from 4 kHz) added to the scene bed at a constant gain: it rides the bed's
per-band delays into the rears (decorrelated, the front leads, so it reads as
room), it is never subtracted from the front, and it contains no decision, so
it cannot jump.  At −12 dB it puts the rear treble back at the 22.09 level
with the candidate's flicker; the price is a static, delayed copy of the
voice's top in the rears (vocal 4–6 kHz own-cell rear share 0.17 → 0.25,
constant).  Surround 0.5 buys the same treble for +45 % flicker; the taper and
retention 0.35 only darken the front and add flicker; gate 0.7 does little.

Confirmed on the same 120 library songs (`music_knobs2.log`, whole mix):

| | flicker | swing | songs > 0.30 | rear 2–16 kHz | rear 0.3–2 kHz |
|---|---|---|---|---|---|
| 22.09 | 0.147 | 0.50 | 104/120 | −10.7 dB | −11.3 dB |
| candidate, no air | 0.068 | 0.28 | 53/120 | −13.2 dB | −17.6 dB |
| **candidate + air −12** | **0.078** | **0.31** | **63/120** | **−11.1 dB** | −17.6 dB |
| candidate + air −9 | 0.081 | 0.31 | 64/120 | −10.2 dB | −17.7 dB |
| candidate + air −6 | 0.081 | 0.32 | 68/120 | −8.9 dB | −17.7 dB |

On ordinary masters air −12 lands 0.4 dB under the 22.09 rear treble while
keeping 47 % less flicker and 39 % fewer swinging songs than the car; the
small swing increase is the static copy following the mix's own treble
balance (the whole-mix share moves with the spectrum, not with a decision).
The rear low-mid stays 6 dB down: that is the voice, and it stays in front.

`AUDIO_STEREO_FRONT_TREBLE_DB` (−12..0, high shelf at 4 kHz on the front pair
only, default off) is the static lever for a front that sounds too bright —
the 21.09 complaint — without touching the object decisions; −3 dB brings the
front's 2–16 kHz close to the 22.09 balance.

On the synthetic grids the candidate is *less* stable than the 22.09 knobs
(pan × pitch grid 36 → 46 of 216 clips with std > 0.10, note-length grid
26 → 38 of 90): the guard reads the synthetic pluck and sustain tones as a
voice often enough for the gate to toggle them.  Real solo guitars get that
call in ~17 % of frames, real voices in ~100 %, so the synthetic scenes are
the ones out of calibration here, and MoisesDB is the arbiter; the grids stay
as regression guards for placement, not as the stability verdict.

Retention, surround gain and the rear body shelf remain env knobs; the vocal
gate can be switched off with `AUDIO_HSSR_VOCAL_GATE=0` for an A/B.  Env
changes need the dio supervisor relaunch (a reboot); the `.so` alone is
per-fork.

## 📋 Still open

* The guard calls a solo pluck/guitar "vocal" in ~17 % of frames; with the
  gate that pulls a solo intro forward until the band enters (synthetic
  `band_enters` / `bed_dropout` 0.6, XFAIL).  Needs a better solo-instrument
  call, not a gate change.
* Guitars still swing ~0.44 within a song (the gate follows the vocal phrases
  in shared bands); a per-bin vocal mask model would separate them properly.
* Synthetic formant voices are not recognised by the guard (XFAIL
  `vocal_formant`); real voices are, so the synthetic scene is the weak one.
* Listening in the car is pending: A/B renders of eight songs from the user's
  library are in `D:\upmix\runs\2026-09-25\music\ab\` (`*.car_0922.6ch.wav`
  vs `*.candidate.6ch.wav`, the dry excerpt in `..\src\`).
