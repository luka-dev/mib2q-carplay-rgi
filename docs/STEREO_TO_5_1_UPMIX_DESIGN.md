# MHI2Q stereo-to-5.1 upmix design and candidate audit

Date: 2026-09-04

Status: `ambient-lite` implemented and host/QNX verified; checked-in vehicle
policy remains `off` until a stationary listening A/B accepts it. The HQ,
Pioneer-oracle and CUpGAN tiers remain candidates, not implemented claims.

Target path: buffered CarPlay stereo decoded inside `dio_manager` (production
candidate index 23, AAC-LC/48 kHz/stereo), rendered through the existing fixed
`PCM6/48 kHz/S16` contract to SoundAmp sink `0x26`. Index 64 can use the same
post-decode path if iOS ever selects buffered APAC stereo. Index 22 is eligible
only after its decoder really returns resampled 48 kHz PCM.

## Decision

Build the upmixer on the head unit immediately after AAC decode and before the
PCM6 ring. Keep the current stream, SAD and CSD geometry fixed. Native APAC 5.1
must bypass it bit-exactly.

There is no justified claim that one algorithm is the universal "best" stereo
upmixer. Stereo does not uniquely determine the missing channels, and the final
SHARC bus-to-speaker processing is not fully visible. The implementation is
therefore split into four measured candidates:

| tier | candidate | purpose |
| --- | --- | --- |
| safe canary | six-band, panning-aware time-domain PAE | low-risk vehicle bring-up and fallback |
| deterministic production candidate | direction-aware STFT primary/ambient decomposition | best controllable quality/CPU trade-off presently available in source form |
| recovered OEM challenger | Pioneer's CarPlay rear-field generator | production automotive reference/oracle; requires a clean-room ARMv7 port |
| ML challenger | CUpGAN ambience extractor | strongest directly relevant 2026 automotive research candidate; requires weights/training and ARM benchmarking |

The current front-runner to build is a deterministic hybrid: use the STFT PAE
to decide which time-frequency energy is genuinely diffuse, then render only
that residual through bounded Pioneer-style all-pass/delay decorrelation. Keep
the exact Pioneer RER network as an OEM comparator, not an assumed winner. The
winner is selected only after the common SoundAmp render and blind A/B described
below.

All four candidates use the same amplifier-aware adapter and place only a
bounded synthesized rear signal in `Ls/Rs`:

```text
PCM6 = front_gain * L,
       front_gain * R,
       0,
       0,
       surround_gain * ambience_left,
       surround_gain * ambience_right
```

Recommended initial values are `front_gain=0.997` and
`surround_gain=0.18` (-14.9 dB at the PCM6 input), with `0.20` as the first
post-canary A/B value. Do not synthesize an explicit center or LFE in the first
vehicle build.

This is amplifier-aware rather than a generic `2 -> 6` copy. The recovered
SoundAmp bank-1 matrix already constructs a unity mid/center bus from L/R and
boosts the explicit surround inputs by +6 dB.

The earlier one-correlation-value-per-1024-frames prototype remains useful as a
test baseline, not as the production recommendation. It treats stable
hard-panned sources as ambience and makes one separation decision for the whole
spectrum.

## Implemented safe canary — 2026-09-04

The first bounded implementation now lives in:

```text
hook/audio/stereo_upmix.h
hook/audio/stereo_upmix.c
tests/stereo_upmix_test.c
tools/stereo-upmix-render/stereo_upmix_render.c
```

It implements the six-band time-domain candidate described below, not the
unbuilt STFT or ML tiers. The graph is six complementary IIR bands at
approximately `220/500/1000/2000/5000 Hz`, smoothed-power amplitude-pan
removal, distinct per-band phase all-passes, then two clean-room Schroeder
all-pass sections per rear. Their delay/coefficient sets are
`Ls=(149,+0.55)->(263,-0.47)` and
`Rs=(211,-0.51)->(293,+0.43)`. A linked instantaneous-attack/slow-release
limiter bounds both rear outputs together.

The exact output contract is:

```text
ambient-lite: 0.997*L, 0.997*R, 0, 0, bounded_Ls, bounded_Rs
off:          L,       R,       0, 0, 0,          0
native 5.1:   bypasses this module entirely
```

The state is caller-owned and 8072 bytes. Processing is C99, fixed-memory and
single-owner: no allocation, mutex, logging, thread, FFT or `libm` call. The
QNX ARMv7 object reports a 128-byte process stack frame and undefined references
only to `memcpy`/`memset`. One call accepts at most 1024 stereo frames and first
stages all input, so exact or partial input/output overlap is safe.

Integration is after the common decoder and before PCM ring statistics/push.
It is selected only for a decoder that reports two channels at 48 kHz, a
six-channel output ring, and a known stereo media index (`22`, `23` or `64`).
The 48 kHz condition means index 22 remains rejected until a real resampler is
present. Index 65 still takes the measured
`C,L,R,Ls,Rs,LFE -> L,R,C,LFE,Ls,Rs` permutation; index 66 still takes the
existing fold/drop/passthrough policy. Upmix state resets on stream start/stop,
rate `1 -> 0`, flush and every decoder/cookie/index rebuild, so an
Atmos -> stereo -> Atmos change cannot carry a rear delay tail across tracks.

The integration also removes an independent latent overflow. Cinemo may return
as many as 4096 stereo frames from one `Write`, while the old backwards
in-place `2 -> 6` loop had only 8192 S16 sample slots and could write past it.
Decoded stereo now remains in the source buffer and is rendered chronologically
through a separate PCM8-sized buffer in blocks of at most 1024 frames.

Runtime controls are parsed once and do not themselves enable Spatial or alter
the advertised formats:

```text
AUDIO_STEREO_UPMIX=off|ambient-lite
AUDIO_STEREO_SURROUND=0.0..0.25
```

Unknown values and the reserved `ambient-hq`, `pioneer-rer` and `cupgan` names
are logged and fail to exact `off`; there is no silent substitution. The
checked-in `carplay_startup.sh` explicitly selects `off` and `0.18`. For the
stationary canary, change only `AUDIO_STEREO_UPMIX=ambient-lite`; the SAD, CSD,
MOST connection and amplifier identification remain fixed PCM6.

Verification completed before vehicle deployment:

- strict C99 host compile, ASan and UBSan pass;
- complete hook audio and integration suites pass, including 3977 APAC packets
  byte-identical to their accepted reference;
- full QNX 6.5 ARMv7 Docker build passes all hook guards;
- exact mono and static `2:1` amplitude pan produce zero measured rear leakage;
- a 1 kHz anti-phase test produces rear/input RMS ratios `0.1408` and `0.1338`
  at the `0.18` setting;
- independent diffuse noise produces `abs(corr(Ls,Rs))=0.0576`;
- reset, arbitrary chunking, independent instances, overlap, full-scale input
  and the recovered SoundAmp bus bounds all pass deterministically.

The host renderer writes WAVE_FORMAT_EXTENSIBLE `5.1(side)` in the exact
`L,R,C,LFE,Ls,Rs` order. On the retained 121.493-second
`session6_stereo_fold.wav`, the canary produced front RMS `-22.08/-22.47 dBFS`,
digital-zero C/LFE, and rear RMS `-57.79/-57.96 dBFS` before the amplifier's
known +6 dB surround input gain. That particular fold therefore receives a
very conservative rear field; this is a measurement, not a quality success.
The rendered audition file is
`build/upmix-audition/session6_ambient_lite_5_1.wav`.

## What Pioneer actually supplies

Two separate Pioneer paths must not be conflated.

### The APAC library: decoder and fixed fold

The previously verified table at VA `0x4316c8`, file offset `0x4216c8`, in
`libSPF_CP_PlugInWrapper.so` is `kAudioMixMatrixSpeakerConfig5_1_AInt`. It is an
8-to-6 downmix:

```text
L'   = L
R'   = R
C'   = C
LFE' = 0.5 * LFE
Ls'  = Ls + 0.5 * height_left
Rs'  = Rs + 0.5 * height_right
```

That table does not generate six channels from ordinary stereo. The same
AArch64 library also contains the FDK MPEG Surround decoder
(`FDK_SpatialDecOpen @ 0x26ef90`, `SpatialDecApplyFrame @ 0x26f670` and
`mpegSurroundDecoder_Apply @ 0x271dd0`). That engine reconstructs spatial audio
using codec side information; it is not a ready generic PCM2 upmixer and cannot
be loaded into ARMv7/QNX.

Library checked:

```text
/Users/luka/Desktop/AUDI_2/Firmwares/Other/Pioneer_SPHERA_OTA/
  extracted_01030010/rootfs/usr/local/lib/libSPF_CP_PlugInWrapper.so
SHA256 d8ac9aa2247d43627228d6538496ee0757e8f6eaa50526babc44dd697b971790
ELF64 AArch64 Linux, stripped
```

### The Media executable: a real CarPlay stereo upmixer

An independent path was recovered from Pioneer's `Media` executable. Its own
CarPlay state machine selects Stereo, UpMix or Spatial per source, and its UI
offers Low, High and Max for non-Atmos CarPlay/Android Auto stereo. Pioneer also
documents that behavior publicly as "Stereo Spatial Sound." This is a genuine
generic interleaved S16 stereo-to-5.0 renderer, not the fixed APAC fold above.

```text
/Users/luka/Desktop/AUDI_2/Firmwares/Other/Pioneer_SPHERA_OTA/
  extracted_01030010/rootfs/usr/local/bin/Media
SHA256 e98dbaf3f7b6e58d0d9aaaa138559aa57a549fbdd946a82a2335e55391b21895
ELF64 AArch64 Linux, stripped
```

Verified call and output path:

```text
controller 0x4471a0 -> wrapper 0x4a8480 -> core 0x4a6b50
input:  interleaved S16 [L,R]
output: interleaved S16 [front L, front R, center, rear L, rear R]
                         +0        +2        +4      +6      +8
frame stride: 10 bytes
```

The five conversion calls and stores are at `0x4a6d84..0x4a6df4`. Helper
`0x4a8b40` proves that the center is exactly a saturating
`0.5 * (original_L + original_R)`, not a learned dialogue stem. The stereo input
is normalized to float at `0x4a8910` and scaled by `0.17782794` (-15 dB); the
graph then uses front/rear all-pass filters, early reflections, reverb,
correction filters, mixers and output EQ
before saturating back to S16. Parameter names at `0x8609e0..0x8617a8` expose
the `FRT_*`, `RER_*`, `RVB_*`, `ERG_*` and `RG_*` blocks. It is therefore a
deterministic ambience/reverberation renderer, not evidence of PCA/PAD or a
neural source separator.

Independent instruction-level review found no sample-dependent branch or
floating-point comparison in the processing closure before final S16
clamp/quantization. For one sample-rate/preset it is a causal fixed LTI network,
not an adaptive separator. Its confirmed audio-array heap lower bound is about
1.25 MB, excluding filter/delay bookkeeping, so a literal port is not
automatically the cheapest QNX implementation.

For 48 kHz, UI `Off/Low/High/Max` maps to internal modes `0/1/3/4`; internal
mode 2 is not exposed. Their final `OUT_ATT` gains are respectively
`1.0/0.944060862/0.749894202/0.668343902` (0/-0.5/-2.5/-3.5 dB). The tables are
constructed from constants in `Media`, not loaded from an external preset
file. The largest visible 48 kHz delay tap is 4664 samples, about 97.2 ms.
The controller's default internal mode is 3 (the UI High preset).

The recovered 48 kHz front/rear mixer gains explain what the three levels do:

| UI | front original | front reverb | rear original | rear reverb |
| --- | ---: | ---: | ---: | ---: |
| Off/core 0 | 0 dB | off | 0 dB | off |
| Low/core 1 | 0 dB | -10.5 dB | +4 dB | -8.5 dB |
| High/core 3 | 0 dB | -10 dB | +8 dB | -5 dB |
| Max/core 4 | 0 dB | -6 dB | +8 dB | 0 dB |

Those original/reverb branches operate inside the -15 dB internal field path;
the processed fronts also retain a dry source path. In High, the coherent rear
component alone is about -9.5 dB at the Pioneer PCM output after `OUT_ATT`, and
Audi's bank 1 then adds +6 dB on the surround bus. A separate Audi rear trim and
linked limiter are therefore mandatory; Pioneer's raw rear output must not be
copied at unity.

This binary cannot be loaded into the ARMv7/QNX hook. A non-invasive AArch64
host oracle is feasible: instantiate the engine in an arm64 Linux process and
render impulses, sweeps and the test corpus through RVAs `0x4a8300`, `0x4a8560`,
`0x4a84f0` and `0x4a8480`. That is the next way to recover exact five-channel
impulse responses before writing a clean-room port.

Source for the user-visible behavior: [Pioneer SPHERA compatibility guide,
Stereo Spatial Sound](https://usa.pioneer/blogs/tech-tips/pioneer-sphera-dmh-wt8000nex-dolby-atmos%C2%AE-iphone-compatibility-guide-may-2026).

### Audi adapter for the Pioneer engine

Do not map all five Pioneer outputs directly into Audi PCM6. The recovered
SoundAmp matrix already derives `Bus C = 0.501 * (L+R)`. Pioneer's explicit
center is the same mid signal again; feeding both makes the bounded full-scale
mono center bus exceed unity even before rear effects. Independently bounded
five-channel inputs would require at least 6 dB of global attenuation under the
visible matrix row sums.

The safe comparison is rear-only:

```text
PCM6 = dry_gain * original L,
       dry_gain * original R,
       0,
       0,
       rear_gain * Pioneer rear L,
       rear_gain * Pioneer rear R
```

Discard Pioneer's center and initially discard its processed fronts. Do not add
another decorrelator: the recovered rear graph already contains all-pass and
reverberation stages. Compare the Pioneer rear generator against the STFT and
CUpGAN rear generators only after matching the predicted `Bus Ls/Rs` level.
The rule applies only to synthesized stereo; semantic native APAC 5.1 remains a
bit-exact bypass.

## 2025-2026 candidate and claim audit

This list separates papers that were actually read from names inferred by search
or product marketing. It is intentionally conservative: a wrong architecture
choice would sit on the realtime APAP receive path.

### CUpGAN: verified, relevant, not yet drop-in

The full open-access AES 2026 paper was inspected, not only its abstract. Its
ambience mode is unusually close to this project: it takes stereo STFT features
and predicts a two-channel ambient signal; the residual can form the direct
signal. The generator has 14,000 parameters, requires 264 million MAC/s of
audio, and uses a 1024-sample window with a 512-sample hop at 44.1 kHz. Reported
structural latency is 23 ms. The convolution is causal.

The paper evaluates a two-to-four speaker result and explicitly leaves extension
to more output channels as future work. That limitation does not prevent our
rear-only use: original L/R can remain in front, the two predicted ambient
channels can feed Ls/Rs, and C/LFE can remain zero. However, no public reference
implementation or trained weights were located as of this audit. A clean-room
retraining would also have to reproduce its MUSDB18/audiobook/ReverbFX synthetic
training recipe and unspecified implementation details. Therefore CUpGAN is a
quality challenger, not something that can be copied into the hook today.

Source: [Uhle, Weyers and Zimmermann, "Implementing Two Upmixing Concepts With
One Conditional GAN," AES 2026](https://aes.org/publications/elibrary-page/?id=23403).

### ImmersiveFlow: verified, unsuitable for this realtime target

ImmersiveFlow is a genuine stereo-to-7.1.4 generative model, but it is not a
small streaming 5.1 upmixer. It processes 10-second training clips through a
pretrained waveform VAE and a 12-block, 16-head, 1024-hidden-dimension DiT, then
solves an ODE and decodes twelve output channels. The paper supplies no
Cortex-A15 realtime result. Its own subjective table also ranks commercial Halo
above the VAE-flow model (`80.62` versus `73.04`); the paper's generative metrics
and subjective result must not be collapsed into a claim that it simply beats
all commercial upmixers.

Dropping six of its 7.1.4 outputs is not a validated 5.1 model. It would require
layout-specific training/render validation and is outside the latency and
runtime envelope of the current ARMv7/QNX hook.

Source: [Liang et al., "ImmersiveFlow," arXiv:2601.12950](https://arxiv.org/abs/2601.12950).

### Other names that were checked

- **APESI (2025): verified, but not a neural network.** The IEEE paper extends
  ambient-phase estimation with joint sparsity and primary/ambient independence
  constraints. Its abstract reports better extraction and ambient spatial
  accuracy, especially for equal primary amplitudes. It is a worthwhile offline
  deterministic comparator, not evidence for the claimed DNN phase estimator.
  Source: [IEEE DOI 10.1109/TCE.2025.3563989](https://doi.org/10.1109/TCE.2025.3563989).
- **Upmixing Via Style Transfer: verified, but from 2022.** It learns a latent
  representation of spatial images for transfer/interactive panning. Neither
  the paper nor its abstract supports the quoted "2026 innovation" or "100x"
  speedup. Source: [arXiv:2203.12053](https://arxiv.org/abs/2203.12053).
- **Learning to Upsample and Upmix Audio in the Latent Domain: verified, from
  2025, but a different task.** This is the paper that actually reports up to
  roughly 100x lower compute. Its upmix experiment is mono-to-stereo inside an
  already VAE-compressed transmission pipeline, not stereo-to-5.1 PCM. Our AAC
  decoder produces waveform PCM and has no such VAE latent, so the result is
  not a deployable shortcut here. Source:
  [arXiv:2506.00681](https://arxiv.org/abs/2506.00681).
- **VCNAC: verified, but it is a variable-channel neural codec.** It preserves
  and decodes already represented mono/stereo/surround content; it is not a
  stereo-to-surround inference algorithm. Source:
  [arXiv:2601.14960](https://arxiv.org/abs/2601.14960).
- **The four-stem DNN recipe is plausible, but not a cited result.** Separating
  vocals/bass/drums/other and assigning them to C/LFE/front/rear is a possible
  product architecture. The supplied summary gives no identifiable paper,
  weights, causal latency, ARM cost, or listening test, so it cannot be ranked
  or treated as evidence for this target.
- **DAM HSR: real but proprietary.** DAM and an IRCAM Forum 2026 presentation
  describe a time-domain, no-FFT, five-sample-latency stereo renderer. Public
  material does not expose enough coefficients, source, neutral third-party
  tests, or an ARMv7/QNX benchmark to reproduce or rank it. It is a licensing or
  black-box A/B candidate, not a clean-room algorithm specification. Source:
  [IRCAM Forum presentation](https://forum.ircam.fr/article/on-the-use-of-an-hsr-as-an-upmix-solution-for-stereo-reproduction-on-multi-speaker-systems/).
- **"MaestroNet PCA-PAD": not verified.** No primary publication or product
  document matching that name and upmix description was located. The name also
  appears in unrelated music/mixing products. It is excluded until a real
  citation is supplied.

Confidence is high for paper dates, stated architectures and published numbers;
medium for the absence of public CUpGAN weights; and deliberately none for the
unverified MaestroNet claim. Confidence in the Pioneer process ABI, five-channel
order and center equation is very high because two independent instruction-flow
audits and a real caller agree; confidence in semantic names for every internal
Pioneer block is medium-high.

## The amplifier transfer we must design through

For the normal SoundAmp profile-0/bank-1 path, the six relevant input slots map
to logical buses as follows. Linear gains are shown in parentheses:

```text
L   -> Bus L  -2 dB (0.794), Bus C  -6 dB (0.501)
R   -> Bus R  -2 dB (0.794), Bus C  -6 dB (0.501)
C   -> Bus L/R -8 dB (0.398), Bus C -3 dB (0.708)
LFE -> Bus L/R -8 dB, Bus C -10 dB, Bus LFE +3 dB,
       Bus Ls/Rs -14 dB
Ls  -> Bus L  -3 dB (0.708), Bus Ls +6 dB (1.995)
Rs  -> Bus R  -3 dB (0.708), Bus Rs +6 dB (1.995)
```

With only stereo L/R present:

```text
Bus C = 0.501 * (L + R) ~= (L + R) / 2
```

The amplifier already creates exactly the mid signal needed by the physical
center path. Populating input C with another copy of `(L+R)/2` would duplicate
dialogue and leak more of it back into both front buses.

The full numerical inverse of this six-input/six-bus matrix is poorly suited to
PCM16 production: representative rows need coefficients up to 3.07 and an
ordinary stereo-to-surround target needs a worst absolute row sum of about 3.71,
costing roughly 11.4 dB of headroom. Do not pre-invert the complete amplifier
matrix.

## Production candidate: direction-aware STFT PAE

The quality-first deterministic design operates per time-frequency region,
instead of applying one decision to a complete 1024-frame AAC block:

```text
STFT: sqrt-Hann, N=1024, hop=256, causal streaming overlap-add
statistics: 24 Bark-like bands, smoothed over time
for each band:
    estimate direct-source pan from L/R level and phase
    estimate inter-channel coherence and confidence
    extract a soft ambient residual A_L/A_R
    reduce the ambient mask on spectral transients
```

The generalized mid/side model estimates the amplitude-pan vector before
forming the residual. Consequently, a stable source remains primary whether it
is centered, partially panned, hard-left, or hard-right. A broadband `L-R`
extractor cannot provide that property. Coherence is a confidence term rather
than the only decision variable.

A complementary covariance check can be formed from the smoothed 2x2 spectral
matrix:

```text
R = [[P_L, C], [conj(C), P_R]]
trace = P_L + P_R
delta = sqrt((P_L - P_R)^2 + 4*abs(C)^2)
diffuseness = 1 - delta / (trace + epsilon)
```

A rank-one source, including a hard-panned one, has diffuseness near zero;
uncorrelated equal-power material approaches one. This value must drive a soft,
smoothed mask. Hard bin assignment creates musical noise and source motion.

Initial rear passband is 120 Hz to 7 kHz, but the vehicle canary should start
more conservatively at roughly 220 Hz to 5 kHz. Use frequency-dependent phase
rotation or separate stable all-pass networks to decorrelate Ls and Rs. The
STFT already delays the rear path, so do not add the earlier proposed 10-15 ms
bulk delay. Protect attacks with a spectral-flux gate with fast attack and
roughly 150-300 ms release, then apply a linked/bounded surround limiter.

Kraft and Zoelzer report that reducing their 2048-sample design to 1024 was only
barely distinguishable and that aggregating control into 24 Bark bands produced
no obvious artifacts in their tests. Our renderer needs only two input FFTs and
two rear inverse FFTs because the original fronts bypass the STFT.

Source: [Kraft and Zoelzer, "Stereo Signal Separation and Upmixing by Mid-Side
Decomposition in the Frequency-Domain," DAFx-15](https://www.dafx.de/paper-archive/2015/DAFx-15_submission_9.pdf).

### Estimated MHI2Q cost and hard gate

The existing QNX/ARMv7 measurement gives approximately 75,800 instructions per
`ComplexFFT(512)`. Four transforms over four 256-frame hops imply about 1.21
million FFT instructions per 1024-frame AAC packet. Including masks, statistics
and overlap-add gives a provisional total of 1.4-1.8 million instructions, or
roughly 4-8% of one 1.7 GHz Cortex-A15 core under the same IPC assumptions used
by the APAC benchmark. This is an estimate, not a target measurement.

Shipping gate: an ARMv7/QNX benchmark must show p99 below 5 ms per 1024 source
frames, with no allocation, lock, logging, or new thread in the audio callback.
The fail-safe result for low confidence or deadline pressure is less rear
ambience, never a modified front signal.

## Low-cost canary: six-band panning-aware PAE

If the STFT implementation misses its deadline, use a complementary IIR bank
with approximate boundaries `220/500/1000/2000/5000 Hz`. For each band:

```text
P_L = EMA(L^2)
P_R = EMA(R^2)
g_L = sqrt(P_L / (P_L + P_R + epsilon))
g_R = sqrt(P_R / (P_L + P_R + epsilon))

A = (g_L*R - g_R*L) / (g_L + g_R + epsilon)
```

Different bounded decorrelators form `A_L/A_R` from `A`. This cancels identical
mono and every stable non-inverting amplitude pan, including hard-left/right,
while preserving anti-phase and unrelated ambience. It is materially safer
than the discarded full-band correlation prototype, but it has poorer spectral
separation and is not the quality-first winner.

Published time-domain variants of this family require about 8.4-20.3 MFLOP/s
and less than 5 ms group delay, depending on filter-bank complexity.

Source: [Kraft and Zoelzer, "Time-Domain Implementation of a Stereo to Surround
Sound Upmix Algorithm," DAFx-16](https://www.dafx.de/paper-archive/2016/dafxpapers/17-DAFx-16_paper_32-PN.pdf).

## ML challenger: CUpGAN-A

If trained 48 kHz weights become available, use only CUpGAN's ambience mode and
feed its two outputs through the same bounded rear renderer. At 48 kHz, the
paper's 44.1 kHz figure scales to approximately 287 million MAC/s, about 6.1
million MAC per 1024-frame input packet. The 14,000 weights fit easily in cache;
compute implementation and worst-case callback time, not parameter storage,
are the open risks.

Do not run the published 44.1 kHz model at 48 kHz without retraining or explicit
validation: the physical frequencies represented by its STFT bins change. Do
not enable this backend until it beats the deterministic STFT candidate in a
loudness-matched blind test and passes the same QNX p99 deadline.

## Initial gain and headroom proof

Using:

```text
PCM L/R  = 0.997 * source L/R
PCM Ls/Rs = 0.20 * bounded ambience L/R
PCM C/LFE = 0
```

the visible SoundAmp profile-0/bank-1 buses become approximately:

```text
Bus L  = 0.792 * L + 0.142 * ambience_left
Bus R  = 0.792 * R + 0.142 * ambience_right
Bus C  = 0.500 * (L + R)
Bus Ls = 0.399 * ambience_left       (-8.0 dB)
Bus Rs = 0.399 * ambience_right      (-8.0 dB)
Bus LFE = 0
```

For bounded inputs, a hard-left/right worst case reaches about 0.934 on its
front bus, while correlated full-scale mono reaches just under 1.0 on Bus C.
Profile 1 lowers the direct surround result by 1 dB and does not invalidate the
headroom. This is substantially safer than a generic copy or matrix inverse.

Allow a runtime surround-gain range of roughly `0.15..0.25` (-16.5 to -12 dB at
the PCM input). Do not expose values above 0.25 until a retained PCM capture has
been passed through the amplifier model and checked for bus overload.

`front_gain=0.997` closes the bounded discrete-sample calculation but leaves
almost no inter-sample true-peak margin. If the policy is a strict -1 dB ceiling
at every visible logical bus, start near `front_gain=0.889` instead and restore
loudness only after oversampled measurement.

## Why C and LFE stay zero initially

This applies only when the decoder produced a two-channel source: there is no
original center or LFE to discard. Native APAC 5.1 retains its real C and LFE
samples unchanged (apart from the already-measured channel permutation).

### Center

Bank 1 already makes `Bus C ~= (L+R)/2`. An explicit center channel would mostly
duplicate the same correlated material and alter front tonal balance through
the C-to-L/R routes. A later cinema/dialogue profile may add a small, coherence-
controlled C feed, but only after the ambience-only profile is measured.

### LFE

The final SHARC crossover/bass-management stage is not fully recovered, while
the input LFE slot is boosted +3 dB and also leaks into five other logical buses.
The stereo source's full-range bass remains in dry L/R; zero in the synthetic
LFE slot therefore does not delete it. Generating another LFE copy immediately
risks doubled or phase-cancelled bass. Keep it zero first. If a sweep on the car
proves bass is missing, the bounded follow-up is:

```text
PCM LFE = 0.178 * lowpass_80Hz(M)     (-15 dB input, about -12 dB at Bus LFE)
```

That needs polarity/phase and sweep measurements before becoming a default.

## Integration point

The stateful `hook/audio/stereo_upmix.c/.h` helper is called from
`spa103_render_frame()` after the selected decoder returns PCM and before ring
statistics/push. The active check is decoded stereo, 48 kHz, a six-channel ring
and a known stereo index. This covers index 23 today without coupling the DSP
module to AAC or Cinemo.

The old backwards in-place widening was replaced. Decoder output stays in its
8192-sample source buffer and a separate 8192-sample render buffer is filled
forward in blocks of at most 1024 frames. This is both the temporal-DSP order
required by the filters and the bound required when Cinemo releases several
AAC callback blocks from one access unit.

State lifetime:

- initialize at buffered-render start even if the first format is index 65;
- reset filters, delay lines, coherence and limiter on stream start,
  `FLUSHBUFFERED`, rate-to-zero and every decoder/cookie/index replacement;
- reset immediately on index 23 -> 65 and 65 -> 23;
- bypass native APAC 5.1 and the existing verified 5.1.2 fold without changing
  a sample;
- perform all work on the existing APAP receiver thread.

Implemented runtime gate:

```text
AUDIO_STEREO_UPMIX=off            current L,R,0,0,0,0 behavior
AUDIO_STEREO_UPMIX=ambient-lite   six-band time-domain canary
AUDIO_STEREO_UPMIX=ambient-hq     reserved; logs unsupported and uses off
AUDIO_STEREO_UPMIX=pioneer-rer    reserved; logs unsupported and uses off
AUDIO_STEREO_UPMIX=cupgan         reserved; logs unsupported and uses off
AUDIO_STEREO_SURROUND=0.18        bounded initial tuning value
```

Keep `off` as the checked-in default until the vehicle A/B passes. A codec
transition must not change PCM6 geometry, SAD, CSD or the amplifier connection.

## Offline proof before deployment

The host renderer in `tools/stereo-upmix-render/` now runs `off` and
`ambient-lite` against any 48 kHz/S16 stereo WAV and writes generated
`L,R,C,LFE,Ls,Rs` PCM6. It is also the common shell into which `ambient-hq`, the
recovered Pioneer rear oracle and any future CUpGAN backend should be added.
The remaining oracle work should write both:

1. generated `L,R,C,LFE,Ls,Rs` PCM6;
2. predicted logical SoundAmp buses after multiplying it by the recovered
   profile-0/bank-1 matrix.

Required deterministic tests:

- `L=R`: no Ls/Rs after filter settling; Bus C remains the mid signal;
- `L=-R`: Bus C is zero and rear ambience is active;
- hard-left and hard-right: the direct source stays in front and synthesized
  rear leakage stays below the limit;
- silence and reset: no stale all-pass/delay tail after reset;
- full-scale correlated and anti-correlated vectors: no S16 wrap and no visible
  bus above the chosen limit;
- impulse: finite, stable decay and identical result across runs;
- random/fuzz PCM: bounded output and no state corruption;
- native APAC 5.1: byte-identical bypass.

Measure per-channel peak/RMS, L/R-to-surround correlation, mono cancellation and
the predicted bus peaks. Render at least vocal-heavy mono, wide studio stereo,
live ambience and bass-heavy material before creating a vehicle build.

For hard-left/right direct sources, "survives" above means that the source
remains in its original front channel while rear leakage stays below the limit;
only separately detected diffuse energy may appear behind. Target checks are:

```text
mono and static amplitude-pan rear leakage < -45 dB
direct/dialogue rear leakage             < -20 dB
diffuse rear abs(inter-channel corr)      < 0.3
front loudness change                     < 0.3 LU
front spectral delta                      < 0.5 dB
```

Use true-peak measurement with 4x oversampling and test both recovered SoundAmp
profiles. A quality claim requires a level-matched blind comparison, not only a
spectrogram or extraction metric.

For material with a retained native 5.1 master, use a hidden-reference MUSHRA
comparison after the SoundAmp model: native 5.1 is the reference,
`L,R,0,0,0,0` is the stereo baseline, and direct L/R copies in the rears are the
low-quality anchor. Match the candidate rear-bus loudness before listening.
Waveform SI-SDR alone is not a valid ranker because a useful decorrelator is
intentionally not sample-identical to the reference surrounds.

## Vehicle A/B

Use one stationary session and the same tracks for:

1. `off`: current `L,R,0,0,0,0` baseline;
2. `ambient-lite`, gain 0.15;
3. `ambient-hq`, gain 0.15;
4. the preferred backend at gain 0.18;
5. only if clean, gain 0.20 and then 0.25.

`pioneer-rer` enters this sequence only after the host oracle is reproducible,
the clean-room result nulls against it, and the same realtime gate is met.

Accept the upmix only if dialogue remains anchored in front/center, rear vocal
leakage is low, reverberation and wide effects become audible behind the seats,
bass does not change, no format-transition click appears, and the native APAC
5.1 path remains identical. “Party mode” copies of L/R into Ls/Rs are excluded:
they fill speakers but collapse spatial intent and make mono vocals come from
behind the listener.

## Static boundary

The known V850-controlled 8x8 matrix ends at logical SHARC buses. The final
bus-to-16-amplifier-channel processing and Symphoria algorithm remain in the
unavailable SHARC application. The design above relies only on the measured
fact that native discrete 5.1 now works end to end and on the exact bank-1 input
coefficients. It does not claim to reproduce Symphoria.

## Related evidence

- [`AMP_STEREO_5_1_MATRIX_DIFF.md`](AMP_STEREO_5_1_MATRIX_DIFF.md)
- [`SHARC_INPUT_OUTPUT_MATRIX.md`](SHARC_INPUT_OUTPUT_MATRIX.md)
- [`PIONEER_TYPE103_STATIC_SPEC.md`](PIONEER_TYPE103_STATIC_SPEC.md)
- [`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md)
