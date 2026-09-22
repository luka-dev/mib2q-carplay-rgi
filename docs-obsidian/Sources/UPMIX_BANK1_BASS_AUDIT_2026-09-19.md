# Bank 1: independent bass and DSP-model audit, 2026-09-19

> Sep20 correction: [producer+consumer audit](SHARC_MATRIX_CONTROL_2026-09-20.md)
> found that all prior AmpChain versions through v6 transposed the A07 matrix.
> The orientation claim and speaker-group electronic bass numbers below are
> superseded. Direct renderer measurements are separate; rerender amp reports with v7/v8.

> Implementation follow-up: the omissions identified below are now addressed
> in model v2. This document retains the pre-fix audit and measurements.
> Current implementation, limitations and recomputed results:
> [SOUNDAMP_FULL_DSP_MODEL.md](SOUNDAMP_FULL_DSP_MODEL.md).

The current electronic model omits a **known, non-unity 16-output mixer**.
Separately, seven held-out music renders show a **roughly +3 dB front
bass/midrange balance shift caused by the upmix**, even with LFE returned to
4096. These are two different findings: an error in the analysis model and a
measured property of our current renderer. Neither alone proves the entire
cause of the cabin listening complaint.

No vehicle operations, production audio changes, model replacement, or commits.
Added a read-only diagnostic and this report. The original AmpChain remains
unchanged so the earlier reports can still be reproduced and compared.

## 1. Firmware extraction and coefficient checks

Image: raw V850 LE32, load/file base 0, 1,572,864 bytes, J525
8W0035465 SW0420 H11. SHA-256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.

- Fresh `audit_amp_output_bindings.audit()` matches the existing JSON exactly.
- Profile-0/bank-1 bytes at `0x101BF4` match AmpChain's matrix orientation and
  stored dB values. However, its `lin()` maps -100 to zero; the input-matrix
  writer clamps these cells near -99 dB. This is tiny leakage, not a large-bass
  explanation, but literal silence is not exact.
- The loader contains 79 scheduled descriptors, 55 profile input biquads and
  143 packed output biquads. Output permutation and packed grouping check out.
- Re-ran V850 builder tracing from `0xAB850` through `0xABF7C`, and inspected
  dispatcher `0xA96C8`: peak Q, crossover damping and shelf formulas agree
  with `amp_dsp_chain.py`. Shelf calls pass gain in r7 (printed as P by the
  generic tracer). No new filter-sign or operand-order error was found.
- The rounded ZETA table differs from firmware float values by at most
  0.00000419, not a material bass-level change.

## 2. Missing second pair mixer: direct evidence

The known chain includes:

`10->16 selector C4DF1 -> 32->16 pair mixer C4DFB -> normal selector C4E6A -> output EQ C4E74`.

AmpChain composes the selectors but **omits C4DFB**, effectively using A=1,
B=0 for every output. Firmware writer `0xA9FF4` loads alternating A/B gain
words into `0xE453A`. The split-half pair-mixer semantics are already verified:
`out[j] = A[j]*in[j] + B[j]*in[16+j]`.

Source fields:

- A gain: profile-0 `0x10032C` (`gp-0x13608`).
- A polarity: `0x10031C` (`gp-0x13618`), all `0xA5` here, so no inversion.
- B gain: global `0x102408` (`gp-0x1333C`).
- `AA06A..AA07C` and `AA0D4..AA0E6` load float at
  `0x69398 - 8*signed_byte`: **0.2 dB per code**, with -128 as exact mute.
  Reading the actual gain table confirms the scale; it is not guessed.

After the normal selector, the missing A-path gains are:

| Output candidates | Config indices | Missing gain |
|---|---|---:|
| Front mid/treble | 0, 1 | +3.8 dB |
| Rear doors | 8, 9 | 0 dB |
| Centre mid | 10 | +0.2 dB |
| Centre tweeter | 11 | -8.8 dB |
| Front woofers | 12, 13 | **+9.4 dB** |
| Sub windings | 14, 15 | -0.8 dB |

Thus the old model understates woofer/front-mid electrical level by **5.6 dB**
and overstates sub/woofer level by **10.2 dB**. Earlier claims that the sub
electrically dominates the woofers by 13 dB cannot stand as written. This is
an omitted factory calibration stage, not a new gain introduced by our hook;
driver sensitivity and acoustic summation still prevent translating voltage
ratios directly into cabin loudness.

Example, same A-path assumptions as AmpChain: L->front woofer at 63 Hz changes
from -8.5 to **+0.9 dB** re MOST L. At 125 Hz it changes from -10.1 to -0.7 dB.
The centre-tweeter peak reports also need revision because they omit -8.8 dB.

Independent numeric check: explicit selectors + split-half mixer + packed
five-word EQ recurrence + DSP/config permutations + delay match the old
output multiplied by the recovered A gains with max error **7.63e-15**.
The test sets B audio to zero explicitly. Some stored B gains are nonzero;
their actual audio must not be assumed absent without tracing the auxiliary path.

## 3. Other scheduled processing checked

- Two delay stages precede the bus EQ. Stored vectors are
  `[1632]*8` and `[87,87,125,204,403,403,367,367]` samples. The latter means
  relative bus timing up to 316 samples (6.58 ms) before the final output
  delays. AmpChain only includes final 0..70-sample delays. These omissions
  matter to phase/acoustic summation even when individual stationary band
  powers barely change. Live values were not read.
- The additional 8-channel EQ at `C4B27/E01B4`, selected by `AC9C4`, is absent
  from AmpChain. All 16 records in stored row 1 are unity (type-1, gain 0).
  Rows 0..4 were inspected and are likewise unity in this image.
- Two post-output-EQ banks at `E4594/E463C`, written from `0x1031B8/0x1031F8`,
  contain eight unity peaks each in the stored calibration. Their omission
  does not add a hidden boost for these stored values.
- Other gain/dynamics stages (`C4B59`, `C4DDA`, `C4E9A`, `C4ED3`, among others)
  exist in the recovered schedule but are not executed by AmpChain. Their
  complete runtime transfer was not recovered in this audit.
- Complex block `C4D17` is still represented as identity and first-mixer B
  audio as zero. Extraction of its instructions is not numerical emulation.
  These are explicit additional limits, not a reason to discard the verified
  matrix, filter, mixer and renderer measurements above.

## 4. Music experiment: bass survives while front mids are transferred away

Seven available held-out tracks: Boost Up, Crazy Train, Deep Down Low,
Heart-Shaped Box, The Blue-Eyed Devil, UP. UP. UP., take your vibes and go.
First 60 seconds, ffmpeg PCM S16 stereo 48 kHz; analyse source seconds 5..55.
Current C renderer, current `hssr_model.bin` and V11 guard, surround 0.40.
The variant with Q15 12288 uses `20*log10(3)` dB (9.542425...), not rounded 9.5.
This reproduces that level setting, not a bit-exact historical binary.

Method: run full excerpts through AmpChain **before** cropping, compensate
the renderer's 512-frame delay, Welch PSD with 32768-frame windows and 50%
overlap. Average linear powers within stereo output groups, then take dB;
average paired differences over tracks. No Sound Check gain; identical
programme scale throughout. The baseline is **L/R-only PCM6 in bank 1**,
not stock stereo/bank 0. No speaker signals are summed as a cabin waveform.

| Current renderer variant | Woofer 60–120 Hz vs baseline | Front mid 500–2000 Hz vs baseline | Bass/mid ratio shift |
|---|---:|---:|---:|
| LFE 12288, no front shelf | -0.79 dB | -3.59 dB | **+2.80 dB** |
| LFE 4096, no front shelf | -0.52 dB | -3.59 dB | **+3.07 dB** |
| LFE 4096, front shelf -3 dB | -3.46 dB | -3.61 dB | **+0.15 dB** |
| Same 4096 render, LFE channel zeroed afterward | -0.38 dB | -3.59 dB | **+3.21 dB** |

These paired differences are unchanged by adding the recovered, fixed A-path
gains: the constants cancel for each group. They remain conditional on the
other omitted stages. They describe front electrical balance, not the total
perceived cabin balance including rear speakers.

For 4096/no shelf, the bass/mid shift is positive on all seven tracks:
**+1.02..+4.47 dB**, mean +3.07. Centre mid 500–2000 Hz falls by 4.81 dB
on average relative to the same bank-1 baseline.

The effect already exists before the amplifier: PCM L/R 60–120 Hz falls by
0.38 dB, while 500–2000 Hz falls by 4.38/4.57 dB. Code explains it:
`front = dry * front_gain - 2.5 * surround * shelved_source`;
at surround 0.40, the source subtraction multiplier is one. Low bass is
largely protected from transfer. Thus lower LFE alone does not restore the
front bass/mid ratio. Zeroing LFE in the same render explicitly confirms this.

The -3 dB front shelf has a concrete model-based justification: it nearly
restores this particular front ratio on average. Its per-track residual is
-1.91..+1.53 dB, so it is not an exact universal correction or proof that
the user's MMI Bass -30% corresponds to -3 dB.

Separately, changing LFE 12288->4096 lowers modelled sub 20–60 Hz by about
9.54 dB. This is a second, independent level change. The earlier complaint
predates 12288, so it cannot explain the whole history.

## 5. Conclusion and next bounded work

The modelling does support a bass-heavy **front spectral balance** caused
by our transfer policy. It does not support dismissing the complaint as
just the sub, nor the old six-bus explanation of a full-band centre speaker.
The output crossover chain attenuates low bass on the centre/front-mid branches.

Before further absolute speaker-level tuning, integrate the recovered second
pair mixer and pre-EQ delays into the main model, with explicit runtime-state
assumptions for its B path and remaining stages. Preserve paired same-bank
comparisons and keep stock/bank-0 comparisons separate. Then assess a bounded
front-balance correction; do not change the amp's factory +9.4 dB coefficient
merely because the old model omitted it.

No-overload claims also exceed the model: it uses floating-point arithmetic
without hardware rail/dynamics emulation. Peaks above unity in an intermediate
stage prove neither actual clipping nor sufficient real hardware headroom.

## Reproduction and artifacts

- [Static diagnostic](../../tools/soundamp-virtual-cabin/audit_bank1_model.py)
- [Static results](../output/upmix-bank1-audit-20260919/static-audit.json)
- [Music results](../output/upmix-bank1-audit-20260919/music-results.json)
- [Music diagnostic](../output/upmix-bank1-audit-20260919/music_audit.py)
- [Second-mixer V850 writer](../output/upmix-bank1-audit-20260919/output-mixer-writer.asm)
- [Biquad builder listing](../output/upmix-bank1-audit-20260919/builders.asm)

To rerun the music diagnostic, compile the host renderer as in the handoff,
placing it at `output/upmix-bank1-audit-20260919/renderer`, then run
`python3 output/upmix-bank1-audit-20260919/music_audit.py`. WAVs/logs are generated
beside it; the original run's WAVs are in `/tmp/upmix-bank1-audit.BR9uxY/`.

Validation: 10 firmware binding tests and 11 virtual-cabin/digital-stage tests
passed. The new diagnostic validates the source image and independently checks
the complete known selector/mixer/packed-EQ composition numerically.
