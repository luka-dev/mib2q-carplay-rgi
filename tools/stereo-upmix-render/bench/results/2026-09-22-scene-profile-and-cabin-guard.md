# Where the upmix spends its instructions, and the first three cuts (2026-09-22)

Offline only. QNX/ARMv7 under QEMU `-icount shift=0`, 8 blocks of 1024 frames,
2 warmup blocks, async controller, HSSR V11 model. Instruction retirement, not
Krait cycles and not vehicle timing.

## The benchmark was measuring a configuration the car never runs

`stereo_upmix_bench` built its config from the library defaults: rear 0.25, no
bed delays, no attack, no retention. The car runs rear 0.40, `bed_delays`,
`bed_attack=owner`, collapse 0.75 and the tapered retention, which adds the bed
ring, `spa_hssr_mix_bed` and the attack detector to every hop. `--scene` now
sets exactly what `carplay_startup.sh` exports.

| configuration | mean instructions per 1024-frame block |
|---|---:|
| library defaults (what was measured before) | 7,809,633 |
| `--scene`, what the car runs | 8,039,292 |

Everything below is `--scene`.

## Profile

`qemu_fnprof_plugin.c` (new) attributes retired instructions to functions from
an `nm -S` table, and with `focus=<symbol>` also to addresses inside one
function, which `addr2line` turns into source lines (build with
`BENCH_CFLAGS_EXTRA=-g`; `-g` does not change code generation).

Start of the session, whole process including model load and input generation:

| function | instructions | share |
|---|---:|---:|
| `spa_hssr_model_load_file` (startup) | 21,136,441 | 17.1% |
| `spa_hssr_fft` | 20,492,854 | 16.6% |
| `spa_hssr_run_hop` | 19,838,704 | 16.0% |
| `spa_stereo_upmix_process_s16` | 18,070,336 | 14.6% |
| `main` (input generation) | 12,950,871 | 10.5% |
| `spa_upmix_biquad` | 5,739,926 | 4.6% |
| `spa_hssr_dot_s8` (control worker thread) | 5,222,568 | 4.2% |

`spa_stereo_upmix_process_s16` cost **1,765 instructions per output frame**, and
per source line 68% of that was `spa_upmix_cabin_guard_s16`: two passes over the
8x6 input matrix per frame plus 16 divisions.

## Three cuts, all bit-identical

| change | mean per block | delta |
|---|---:|---:|
| baseline `--scene` | 8,039,292 | |
| cabin guard: early-out + skip non-binding divisions | 7,192,336 | −10.5% |
| `spa_upmix_biquad` forced inline | 7,074,435 | −1.6% |
| FFT: trivial first pass, walked twiddle index | 6,950,678 | −1.8% |
| **total** | **6,950,678** | **−13.5%** |

Every step keeps the bench checksum `0xe500b91a9b2c337a` and renders
`cmp`-identical 60 s output on four cuts (Boost Up, Crazy Train, take your vibes
and go, Heart-Shaped Box) at the deployed settings.

1. **Cabin guard.** `|bus value| <= max column sum * max |sample|`, and the
   largest column sum of `kInputMatrix` is 3.526, so below 9143 (−11.1 dBFS) no
   bus row can reach the 32275 limit, the LFE bounds cannot bind and the linked
   gain cannot trigger: the frame leaves before touching the matrix. Separately,
   seven of the eight buses have the matrix floor (1.12e-5) as their LFE
   coefficient, which puts their LFE bounds past 1e9 while `|base|` stays under
   1.2e5, so they can never bound an S16 sample: 14 of the 16 divisions per
   frame were arithmetic with no effect. `-icount` counts a division as one
   instruction, so the real gain on Krait is larger than the number above.
2. **Biquad.** Eight calls per frame, about 70 instructions each for a body of a
   dozen operations. GCC left it out of line because the caller is already
   large; `always_inline` keeps the same arithmetic in the same order.
3. **FFT.** The first pass has only the twiddle (1, 0), so a quarter of all
   butterflies carried four needless products, and the twiddle index is an
   accumulator, not a multiply per butterfly.

## Fourth cut: NEON butterflies, bit-identical on the target

The butterfly operands are a split real/imaginary layout, so they vector-load
directly; the twiddles did not, because a pass reads them with its own stride.
They are now regrouped once in `spa_hssr_dsp_prepare` into per-pass contiguous
tables (the pass with half h starts at h - 2, and the halves 2..256 sum to
FFT_SIZE - 2), which by itself is bit-identical, and the inner loop then does
four butterflies per iteration with separate multiplies and one subtract, the
same operations in the same order as the scalar tail that follows it.

| | mean per block | delta |
|---|---:|---:|
| after the first three cuts | 6,950,678 | |
| NEON butterflies | 6,220,931 | −10.5% |
| **total against the `--scene` baseline** | **6,220,931** | **−22.6%** |

`spa_hssr_fft` itself went from 20,492,854 to 11,680,987 instructions, −43%.

**Bit-exactness.** The bench checksum is unchanged, and the four 60 s cuts
render `cmp`-identical when the host is built the way the target computes. That
qualifier matters: ARMv7 with `-mfpu=neon` has no fused multiply-add, while the
Mac host contracts `wr*re - wi*im` into one FMA by default, so a contracted
host build disagrees with both its own NEON path and the car (up to 800 LSB on
a peak, mean about 1 LSB, from tracker decisions flipping on the last bit).
The host recipes now pass `-ffp-contract=off`, which makes offline renders equal
to the vehicle arithmetic again; use it for any future comparison.

## What is left, measured

After the three cuts, of the audio-path instructions: FFT about 31%, `run_hop`
about 30%, `process_s16` about 15%, biquads about 9%. Inside `run_hop` the
inlined `spa_hssr_dsp_smoothstep` is 8.9% and `spa_hssr_dsp_bound` 5.8%; inside
the FFT the butterfly body is about 61% and the bit reversal 8%.

After the NEON pass the shares are `run_hop` 19.9M, `process_s16` plus the LFE
crossover 14.5M, the FFT 11.7M and the control worker 5.2M. `run_hop` is now the
biggest and also the flattest: its hottest single source line is 4.5%
(`smoothstep`), then 2.9% (`bound`), 2.9% (analysis windowing), about 2%
(`mix_bed`). There is no second 10% cut hiding in it.

Remaining candidates, none of them free:

- **The rest of the vectorisable loops**: analysis windowing, the two
  overlap-add loops, `pack_hermitian`, `mix_bed`, the trivial first FFT pass.
  Together about 8% of `run_hop`, so a few percent overall, same technique.
- **Real-input analysis transform.** Two of the four transforms per hop have a
  zero imaginary part; the N/2 trick halves them, but it changes the arithmetic,
  and the earlier cross-channel packing was rejected because small differences
  flipped object-tracker decisions.
- **`smoothstep` divisions.** One division per call on runtime bounds. Cheap in
  instructions, expensive in Krait cycles; replacing them with precomputed
  reciprocals is not bit-identical.

## Fifth cut: the guard matrix in vectors, and what the deadline looks like now

The early-out does almost nothing for the case that actually matters. Measured
on the rendered PCM6 of the same four cuts, per 768-frame period (one QSA period
at 48 kHz, 16 ms), the share of frames that must run the whole guard is:

| track | median period | worst period |
|---|---:|---:|
| Boost Up | 0.4% | 89.8% |
| Crazy Train | 10.0% | 70.3% |
| take your vibes and go | 36.2% | 94.0% |
| Heart-Shaped Box | 0.0% | 66.3% |

So on the loud passage that trips the realtime guard, 66-94% of frames pay the
full 8x6 product. `kInputMatrix` is input-major, so one row is the eight bus
coefficients of one channel: the product is now six scaled row accumulations in
NEON instead of forty-eight scalar ones, with each product rounded separately
and the channels accumulated in the same order. The benchmark also compiled
`stereo_upmix.c` without `-mfpu=neon` while the shipping hook compiles every
source with it, which is fixed here too.

Measured against the pre-optimization build (48b9851) on the identical
workload, with the early-out forced off to model a fully loud period:

| | per 1024-frame block | delta |
|---|---:|---:|
| before, any frame | 8,039,373 | |
| now, typical | 6,024,357 | −25.1% |
| now, loud period (guard always full) | 6,228,611 | −22.5% |

## Against the vehicle deadline

The realtime path gets `SPA_RT_UPMIX_BUDGET_NS` = 8 ms per 768-frame period
(16 ms of audio) and disables itself after four consecutive periods over it.
What the car logged on the pre-optimization build, five sessions:

| session | periods | upmixed | worst | disabled |
|---|---:|---:|---:|---|
| 1 | 11,625 | 9,463 | 11,079 us | yes |
| 2 | 575 | 72 | 9,064 us | yes |
| 3 | 1,662 | 1,662 | 8,057 us | no |
| 4 | 15,075 | 9,928 | 9,064 us | yes |
| 5 | 5,902 | 5,902 | 10,071 us | no |

Scaling those by the measured −22.5% of the loud-period case gives 6.2, 7.0,
7.0, 7.8 and 8.6 ms. Four of the five observed worst cases move under the 8 ms
line and the largest still grazes it. This is an estimate: it assumes time
scales with retired instructions, while NEON changes the instruction mix and
the removed divisions cost more cycles than instructions, so the real margin
should be better than this. Only the unit settles it, and it already logs
`worst_us` at every stop.

