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

## What is left, measured

After the three cuts, of the audio-path instructions: FFT about 31%, `run_hop`
about 30%, `process_s16` about 15%, biquads about 9%. Inside `run_hop` the
inlined `spa_hssr_dsp_smoothstep` is 8.9% and `spa_hssr_dsp_bound` 5.8%; inside
the FFT the butterfly body is about 61% and the bit reversal 8%.

Remaining candidates, none of them free:

- **NEON butterflies** (about 31% of the path, the largest single item). Krait
  NEON is four floats wide but flushes denormals, and the twiddles are strided,
  so the win needs a restructured pass order. Not guaranteed bit-identical.
- **Real-input analysis transform.** Two of the four transforms per hop have a
  zero imaginary part; the N/2 trick halves them, but it changes the arithmetic,
  and the earlier cross-channel packing was rejected because small differences
  flipped object-tracker decisions.
- **`smoothstep` divisions.** One division per call on runtime bounds. Cheap in
  instructions, expensive in Krait cycles; replacing them with precomputed
  reciprocals is not bit-identical.
