# Cabin-Aware LFE V2 — 2026-09-07

## Replacement scope

The fixed mono-sum LR4 assist was removed from the working tree. `assist` now
selects only Cabin-Aware LFE V2; there is no version switch, compatibility
branch or fallback to the deleted implementation.

V2 uses two per-side LR4 branches at 55 and 95 Hz, a smoothed low-band
correlation fold that preserves coherent `L-R` bass, sample-domain transient
detection, and the V11 controller's delayed low-band bass/drum posteriors. The
waveform path remains sample-rate and shares the existing 512-frame HSSR
latency. The learned controls use a separate sample-indexed 512-frame ring, so
they are aligned and callback-size invariant.

The LFE ceiling is Q15 `8192` (`0.25`) and the paired front gain is Q15 `30100`
(`0.91858`). A per-sample feasible-interval limiter covers all recovered J525
profile-0/bank-1 logical bus equations and limits them to `0.985` full scale.

## Objective audio regression

Renderer: HSSR Universal V11, rear Q15 `8192`, 50 private songs, two
deterministic ten-second excerpts per song (100 renders). The baseline was
rendered with a saved pre-replacement binary; the current result uses the exact
shipping C implementation.

```text
                              fixed baseline       LFE V2       change
mean LFE energy fraction        0.3619%             0.4482%      +23.9%
mean LFE/front level           -22.542 dB          -21.772 dB    +0.770 dB
mean LFE sample peak           -22.319 dBFS        -20.718 dBFS  +1.601 dB
maximum LFE sample peak        -15.437 dBFS        -14.108 dBFS  +1.329 dB
maximum recovered J525 bus      -0.272 dBFS         -0.483 dBFS  +0.212 dB headroom
clipped samples                  0                   0
front preservation SNR           8.727 dB             8.527 dB    -0.200 dB
fold-back SNR                   11.615 dB            11.414 dB    -0.201 dB
```

The rear waveform correlation is unchanged; the small rear-energy-fraction
change comes from the paired front trim, not a rear-routing change.

Controlled one-second S16 tests after settling:

```text
40 Hz, L=R:   LFE RMS 0.135107
40 Hz, L=-R:  LFE RMS 0.135107 (fixed baseline: 0)
500 Hz, L=R:  LFE RMS 0.000131 (60.3 dB below 40 Hz)
```

## Cost

GCC 8.5 ARMv7/QNX, QEMU `-icount`, 32 warm-up plus 128 measured 1024-frame
blocks with real-time pacing:

```text
V2 sync:   mean 6,150,299, p99 6,162,710 estimated instructions/block
V2 async:  mean 6,162,208, p99 6,174,487 estimated instructions/block
```

The deterministic synchronous checksums are
`0xa7f32b4252812162` on the little-endian host render and
`0x8b0b3795a6936fbb` on the ARMv7/QNX build.

Relative to the last fixed-assist measurement, total retired work increases
12.65%. The async mailbox overhead remains 0.19%; all 319 jobs completed with
`busy=0`, `stale=0`. QEMU counts target instructions, not Krait wall time.

An Apple Silicon paced host run measured the audio callback at mean/p99
`364166/631083 ns` with the controller worker, or `1.71%/2.96%` of the
21.333 ms block budget. Host scheduling makes this a sanity check rather than
a vehicle timing claim.

## Verification

- full host suite: pass;
- V2 phase, frequency, six-bus bound, chunk and reset tests: pass;
- ASan/UBSan upmixer and worker tests: pass;
- ThreadSanitizer worker test: pass;
- QNX GCC 8.5 shipping hook build and standalone ARMv7 benchmark: pass;
- 100/100 music renders: zero clipped samples.

The remaining release gate is a stationary vehicle A/B for sub polarity,
crossover integration and perceived driver-seat level.
