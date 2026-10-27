# The remaining free cuts: -O3, fused FFT passes, L/R biquad pairs (2026-09-25)

Offline only. QNX/ARMv7 under QEMU `-icount shift=0`, GCC 8.5 (`:latest`),
`--scene --async-controller`, 512 blocks of 1024 frames after 2 warmup.
Instruction retirement, not Krait cycles.

| step | mean per block | delta | checksum |
|---|---:|---:|---|
| baseline (HEAD 94bd713) | 5,732,852 | | `0xa3c479b3942b2431` |
| `-O3 -fno-math-errno` on the upmix DSP | 5,337,005 | −6.9% | same |
| FFT passes 1+2 fused in NEON (radix-2², `vld4q`) | 4,946,073 | −7.3% | same |
| bit reversal as a 240-pair swap table | 4,851,294 | −1.9% | same |
| L/R biquads as NEON pairs (6 per frame) | 4,634,693 | −4.5% | same |
| **total** | **4,634,693** | **−19.2%** | same |

Other flags measured and not taken: `-mtune=cortex-a15` alone +2.1% instructions
(scheduling may still help Krait cycles, QEMU cannot show it);
`-mfpu=neon-vfpv4` was not tried because fused multiply-add changes the
arithmetic.

Bit-exactness beyond the synthetic bench input: the host renderer
(`-ffp-contract=off`, arm64, which takes the same NEON paths) renders three car
captures folded to stereo (`logs/captures/.../session5/6_stereo_fold.wav`,
`live6_stereo_fold.wav`, 50-121 s) `cmp`-identical before and after at the car's
settings.

- **Flags.** `scripts/apac_flags.sh` defines `UPMIX_DSP_SRCS` and
  `UPMIX_DSP_CFLAGS`; the hook build compiles those five files with them and the
  bench compiles with them too. The only change in the hook's dynamic imports is
  that `sqrtf` is gone (now an inline `vsqrt`); the exports are identical.
- **FFT.** The half=2 pass was scalar because the four-wide loop needs half ≥ 4.
  `vld4q` puts element k of four consecutive groups of four in lane set k, so
  both first passes become lane-wise vector arithmetic with the same operations
  in the same order.
- **Biquad pairs.** Every per-frame biquad runs on L and R with the same
  coefficients. The NEON difference is denormal flushing: the state already
  flushes below 1e-20, and a denormal output is 0 in S16.

What is left in `run_hop` is ~23% `smoothstep` and `bound` per bin plus
data-dependent slot decisions; the remaining libm calls (`logf`, `expf`) are
~11%. None of these can be removed without changing results.
