# CUpGAN-A feasibility for the MHI2Q hook

Status on 2026-09-04: **architecture recovered from the primary paper;
trained weights and reference code are not public; production backend remains
blocked.** The runtime value `AUDIO_STEREO_UPMIX=cupgan` must continue to report
unsupported rather than silently selecting another algorithm.

Primary source: C. Uhle, P. Weyers, and J. Zimmermann, “Implementing Two
Upmixing Concepts With One Conditional GAN,” AES 2026 Automotive Audio
Conference, paper 9, Fraunhofer IIS:
<https://aes.org/publications/elibrary-page/?id=23403>.

## What is specified

CUpGAN is one conditional generator shared by two tasks. `CUpGAN-A` estimates
a two-channel ambient component; `CUpGAN-C` estimates a two-channel center
component. For the ambience mode the paper defines the complementary direct
signal as `d_hat = x - a_hat`.

The generator uses a 1024-point STFT with a 1024-sample window and a 512-sample
hop at 44.1 kHz. Each stereo complex coefficient is represented by three real
features - log magnitude, normalized real part and normalized imaginary part -
for six input features per time-frequency bin. An inverted bottleneck expands
these to 21 channels. Four conditioned ConvNeXt blocks then process tensors of
shape `(B, 21, 513, T)`. Each block contains:

- causal `3 x 3` depthwise convolution;
- layer normalization;
- pointwise `21 -> 84`, GELU, pointwise `84 -> 21`;
- FiLM and a residual connection.

A final linear layer returns six spectral features and iSTFT produces stereo
audio. A small four-layer control network modulates FiLM parameters to switch
between ambience and center extraction. The paper reports about 14,000 model
parameters, 264 million MAC/s at 44.1 kHz, and 23 ms algorithmic latency.

Training uses synthetic direct-plus-reverberant mixtures built from MUSDB18,
speech audiobooks and ReverbFX impulse responses. The authors report 2,400
stereo pairs of six seconds, 400,000 Adam iterations, batch size 16 with
0.5-second excerpts, and about 32 hours on an A100. The generator loss combines
multi-resolution magnitude STFT, complex STFT and high-passed time-domain MSE
with least-squares adversarial and feature-matching losses.

## What is missing

Neither the paper nor the authors' public GitHub search results provide:

- trained CUpGAN-A weights or a model license;
- source code or a reference inference graph;
- exact IBN channel grouping and bias/activation details;
- the exact conversion from the final six features back to complex STFT;
- FiLM/control-net output layout and initialization;
- inference precision, quantization scheme, padding edge policy, or GELU
  approximation;
- a 48 kHz model or evidence that a 44.1 kHz model is valid after resampling.

Those are output-correctness blockers. They do not prevent a lower-bound
performance measurement of the fully dimensioned ConvNeXt core.

## MAC-count discrepancy

For one time-frequency position, the four disclosed blocks contain:

```text
4 * (21 * 3 * 3 + 21 * 84 + 84 * 21) = 14,868 MAC
```

With 513 bins and `44,100 / 512` STFT frames per second, that is about
656.96 million MAC/s before IBN, output projection, STFT/iSTFT, normalization,
GELU or FiLM. At 48 kHz it is about 715.06 million MAC/s. By comparison, the
paper's stated total scales from 264 to 287.35 million MAC/s at 48 kHz.

The straightforward reading of the disclosed architecture is therefore 2.49x
the paper's stated computation. Possible alternative accounting or an omitted
optimization cannot be inferred safely. Until reference code is available,
both numbers must be reported with their definitions.

`tools/cupgan-bench/` implements the disclosed dense four-block core and can
measure the exact ARMv7 instruction stream emitted by the QNX GCC 4.9 or 8.5
toolchain.
It intentionally excludes every unspecified stage, so its result is still a
lower bound for complete CUpGAN inference even though its MAC count exceeds the
paper's figure.

## Measured verdict on this target

The benchmark was also built with the available QNX GCC 8.5.0 toolchain. A
shipping-like scalar build needs 48.90 million ARMv7 instructions per
512-sample hop, or 4.584 billion instructions per second of audio. GCC 8.5
auto-vectorization alone is insufficient. Packing pointwise weights by four
output channels and using explicit NEON reduces the disclosed core to 17.997
million instructions per hop (1.687 billion per audio-second).

That optimized lower bound consumes 99.2% of one 1.7 GHz core at assumed IPC
1.0, 70.9% even at IPC 1.4, and 165.4% at IPC 0.6. A 1024-frame hook callback
contains two model hops, corresponding to about 21.17 ms of core CPU time at
IPC 1.0. It therefore misses the existing 5 ms callback gate by more than 4x
before STFT/iSTFT, IBN, output projection, decoder work and scheduling are
included. Full commands and raw counters are recorded in
`tools/cupgan-bench/results/2026-09-04.md`.

GCC 8.5 is a useful production candidate and manual NEON is mandatory, but a
straight dense realization of the disclosed network is not deployable on the
APQ8064 path. Integration should remain blocked pending the authors' reference
implementation or a smaller/quantized retrained model with a new performance
measurement.

## Product integration gate

Do not connect `cupgan` to the PCM callback until all of these are true:

1. Obtain licensed weights and exact inference semantics, or train and freeze a
   clearly named clean-room 48 kHz model rather than calling it the authors'
   CUpGAN.
2. Match a trusted reference implementation on stored spectra and audio.
3. Measure the complete backend on QNX/ARMv7, including STFT/iSTFT and format
   conversion. QEMU instruction counts are useful, but the real head unit must
   still pass the existing callback p99 deadline and dropout soak test.
4. Keep model storage, scratch buffers and initialization outside the audio
   callback; processing must allocate, lock, log and spawn nothing.
5. Loudness-match the rear output against `ambient-lite` and the deterministic
   HQ candidate, then pass stationary blind listening tests and phase/downmix
   safety checks.
6. Preserve native APAC 5.1 bypass. The neural backend is only for decoded
   stereo and must reuse the bounded PCM2-to-PCM6 adapter rather than touching
   native center, LFE or surround channels.

At 48 kHz the 1024-sample analysis window is 21.33 ms and the 512-sample hop is
10.67 ms. CPU time per hop must fit inside that cadence; the separate product
gate remains stricter so decoder, hook and scheduling work retain headroom.
