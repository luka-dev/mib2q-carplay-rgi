# HSSR Universal rear-presence high shelf

Date: 2026-09-08

## Decision

The single production Universal profile now applies one fixed RBJ high shelf
to the separated Ls/Rs signals only. The 48-kHz coefficients implement `S=1`,
a 4-kHz midpoint and `+3 dB` high-frequency gain:

```text
frequency    shelf magnitude
1 kHz        +0.011 dB
2 kHz        +0.168 dB
4 kHz        +1.500 dB
8 kHz        +2.865 dB
12 kHz       +2.984 dB
20 kHz       +3.000 dB
```

The shelf is after HSSR separation and before the linked rear limiter. The
un-EQ'd, limiter-tracked object signal still drives the existing `2.5x` front
transfer; therefore the speaker-voicing EQ is not applied to dry L/R. Digital
C, LFE V2, the Q15 `8192` (`0.25`) rear ceiling and all selection masks are
unchanged. There is no new runtime profile or toggle.

## Held-out music A/B

Fourteen deterministic ten-second excerpts from seven private held-out songs
were rendered against the exact preceding Git commit. Both versions used HSSR
Universal V11, rear `0.25`, LFE `assist`, and the same model. A sixth-order
analysis low-pass below 2 kHz and high-pass above 4 kHz measured the emitted
PCM rear/front bands after the common 2048-frame settling prefix.

```text
metric                              previous        shelf       change
mean Ls/Rs level above 4 kHz             —             —        +2.150 dB
per-excerpt Ls/Rs >4-kHz change          —             —        +1.561..+2.552 dB
mean Ls/Rs level below 2 kHz              —             —        -0.115 dB
mean rear energy fraction             2.8374%        3.0876%      +8.82% relative
mean rear/front level                -18.167 dB     -17.542 dB    +0.625 dB
mean rear L/R waveform correlation     0.4786         0.4792      +0.0006
mean block-balance P90                 6.714 dB       6.483 dB    -0.231 dB
front preservation SNR                11.243 dB      11.407 dB    +0.164 dB
fold-back SNR                          14.131 dB      14.542 dB    +0.411 dB
maximum J525 matrix peak               -0.308 dBFS    -0.308 dBFS unchanged
mean LFE/front level                  -27.738 dB     -27.750 dB    -0.012 dB
clipped samples                              0               0
```

The slight low-rear reduction comes from the linked limiter reacting to the
new high-frequency peaks; it is not a low-frequency EQ cut. L/R balance and
correlation are effectively unchanged, so the shelf raises presence without
collapsing the existing side distinction. These stereo-only tracks contain no
authored rear reference, so this is an activity/safety regression test rather
than proof of studio-object recovery.

Ignored detailed renders and reports are reproducible under:

```text
build/reference-audio/rear-high-shelf-baseline-eval/
build/reference-audio/rear-high-shelf-current-eval/
```

## Performance and verification

GCC 8.5 ARMv7/QNX under calibrated QEMU `-icount`, 32 warm-up and 128 measured
1024-frame blocks:

```text
shelf sync:   mean 6,052,311, p99 6,065,835 estimated instructions/block
shelf async:  mean 6,064,588, p99 6,077,675 estimated instructions/block
```

Against the preceding LP48 result, the mean cost rises about `3.5%`. The async
controller completed all 319 jobs with `busy=0`, `stale=0`.

- exact 1/4/12-kHz shelf-response regression: pass;
- full `scripts/run_tests.sh`: pass;
- ASan/UBSan upmixer test: pass;
- QNX GCC 8.5 full hook build: pass;
- QNX GCC 8.5 standalone ARMv7 sync/async runs: pass;
- 14/14 held-out excerpts: zero clipped samples.

The remaining gate is a stationary vehicle A/B. The intended audible change is
clearer cymbal, guitar-edge, room and effect presence from the rear tweeters,
without raising rear bass or changing which material HSSR selects.
