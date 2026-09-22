# HSSR Universal: packed transforms and band-hoisted mask loop

Date: 2026-09-21

## Outcome

The realtime (type-100) upmix path was overrunning its per-period budget on the
car (9-11 ms of a 16 ms period) and disabling itself. This pass removes work
from the shared DSP without changing the design.

| build | estimated retired instructions / 1024-frame block |
|---|---:|
| before | 9,711,018 |
| after | **7,856,061 (-19.1%)** |

QNX 6.5 ARMv7, GCC 8.5, single-thread TCG, `-icount shift=0`, n=32 blocks after
4 warmup, `--async-controller` (the production split, as the car runs it).
QEMU retired instructions are a deterministic regression metric, not Krait
cycles or a realtime-margin proof.

## What changed

1. **Synthesis: 4 inverse transforms per hop -> 2.** Both synthesis outputs are
   real, so two Hermitian half-spectra pack as `Z = A + jB` and one inverse
   transform yields both signals (real part = left, imaginary = right). Applied
   to the main rear pair and to the source image pair. Exact in real arithmetic.
2. **Bit reversal from a table** instead of recomputing the permutation per call.
3. **Inverse sign hoisted out of the butterfly** into a negated twiddle table
   (it was a branch on each of 2304 butterflies per call).
4. **`1/N` folded into the synthesis window**, removing a 1024-element scale
   loop with a finiteness check per element from every inverse transform.
5. **Mask loop: band-only work hoisted.** 257 bins share 32 bands, so two
   divisions and three smoothsteps per bin were being recomputed about eight
   times per distinct value. Now computed once per band.
6. **Object-slot scan early-out.** `score = band_weight * pan_match` with
   `pan_match` in [0,1], so a slot whose weight cannot beat the incumbent is
   skipped without its smoothstep. Exact, not an approximation.
7. **Coherence uses one square root** (`sqrt(a/b)`) instead of two plus a divide.

## Rejected: packed analysis

Packing the two real analysis inputs into one forward transform is equally exact
(isolated error 7.6e-6 against separate transforms) and would have saved a
further ~3%. It was rejected: that float-level perturbation reaches the discrete
decisions of the object tracker (`score > best_score`, smoothstep gates, the
94-hop slot hold), flipping slot selection and changing the output by up to 1461
LSB (-27 dBFS). The result is a different valid realization of the same
algorithm, not a defect, but it is not acceptable for a scene tuned by ear.

## Differential acceptance

Seven private held-out 60-second cuts, rendered against the exact preceding
commit with the shipped settings (surround 0.40, front bass -3, LFE 0,
retention 0.5, bed delays on, attack owner 0.75):

* max absolute difference **4 LSB (-78.3 dBFS)** on every track, 0.8-2.2% of samples;
* band metrics identical: front mid 500-2000 Hz and rear/front 300-3000 Hz both
  0.00000 dB, peak 0.0000 dB, attack smear 0.0000 dB, rear coherence 2-8 kHz
  equal to the sixth decimal.

The residual 4 LSB is float reassociation plus occasional threshold crossings in
the int16 rounding and the linked rear limiter; it is bounded and self-clearing
within the limiter's ~42 ms release. No scene decision changed, which the
identical band metrics confirm.

## Car-side expectation

The measured realtime period cost scales to roughly 8.5 ms from 10.5 ms. The
existing 8 ms deadline guard was left untouched by request, so the realtime path
may still trip it occasionally; the buffered (type-103) path has no such guard.
Further reduction needs the mask loop (now memory-bound across nine 257-element
bin arrays) or a NEON butterfly, both larger projects.
