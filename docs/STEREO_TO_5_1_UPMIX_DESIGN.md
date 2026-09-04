# MHI2Q stereo-to-5.1 upmix design and candidate audit

Date: 2026-09-04

Status: `ambient-lite`, `ambient-hq`, the clean-room time-domain `hsr-td`, and
the opt-in LR4/80 Hz LFE assist are implemented and host/QNX verified. The
checked-in and deployed vehicle policy now selects `hsr-td`, rear gain `0.18`,
and LFE assist for the requested stationary A/B.  Offline renders and
ARMv7/QEMU instruction measurements exist, but subjective vehicle acceptance
and real on-unit Krait timing are still open. The Pioneer oracle and CUpGAN
tiers remain candidates, not implemented claims.

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
therefore split into five measured candidates:

| tier | candidate | purpose |
| --- | --- | --- |
| safe canary | six-band, panning-aware time-domain PAE | low-risk vehicle bring-up and fallback |
| deterministic HQ candidate | direction-aware STFT primary/ambient decomposition | implemented; pending vehicle A/B and real Krait timing |
| deterministic time-domain challenger | six-band covariance/projector residual (`hsr-td`) | implemented; no FFT/block latency, pending vehicle A/B and real Krait timing |
| recovered OEM challenger | Pioneer's CarPlay rear-field generator | production automotive reference/oracle; requires a clean-room ARMv7 port |
| ML challenger | CUpGAN ambience extractor | strongest directly relevant 2026 automotive research candidate; requires weights/training and ARM benchmarking |

The implemented HQ challenger is the deterministic hybrid: the STFT PAE decides
which time-frequency energy is not the coherent primary image, then renders
that residual through the same bounded Pioneer-style all-pass/delay adapter as
the canary. Keep the exact Pioneer RER network as an OEM comparator, not an
assumed winner. The winner is selected only after the common SoundAmp render
and blind A/B described below.

All four candidates use the same amplifier-aware adapter. The initial active
profile places bounded synthesized signals only in `LFE/Ls/Rs`:

```text
PCM6 = 0.940 * L,
       0.940 * R,
       0,
       0.178 * LR4_LP80((L+R)/2),
       0.18 * ambience_left,
       0.18 * ambience_right
```

The LFE-disabled profile retains `front_gain=0.997`; enabling LFE assist pairs
it with `front_gain=0.940` and `lfe_gain=0.178`. The front trim is part of the
profile, not an independent tuning knob: it reserves the recovered SoundAmp
matrix headroom consumed by LFE leakage. `surround_gain=0.18` (-14.9 dB at the
PCM6 input) remains the initial rear value, with `0.20` as a later A/B value.
No explicit center is synthesized.

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

`ambient-lite` implements the six-band time-domain candidate described below,
not the separate HQ STFT or ML tiers. Its graph is six complementary IIR bands at
approximately `220/500/1000/2000/5000 Hz`, smoothed-power amplitude-pan
removal, distinct per-band phase all-passes, then two clean-room Schroeder
all-pass sections per rear. Their delay/coefficient sets are
`Ls=(149,+0.55)->(263,-0.47)` and
`Rs=(211,-0.51)->(293,+0.43)`. A linked instantaneous-attack/slow-release
limiter bounds both rear outputs together.

The exact output contract is:

```text
ambient-lite + LFE off:    0.997*L, 0.997*R, 0, 0,          bounded_Ls, bounded_Rs
ambient-lite + LFE assist: 0.940*L, 0.940*R, 0, LR4_LFE,    bounded_Ls, bounded_Rs
off:                       L,       R,       0, 0,          0,          0
native 5.1:                bypasses this module entirely
```

Processing is C99, fixed-memory and single-owner: no allocation, mutex, logging,
thread or sample-path `libm` call. The `ambient-lite` branch itself performs no
FFT; the caller-owned public state is now 59624 bytes because the same object
also contains all fixed HQ FFT/WOLA and HSR-TD covariance storage. One call accepts at most 1024
stereo frames and first stages all input, so exact or partial input/output
overlap is safe. Current shared object/stack measurements are recorded with the
HQ implementation below; the earlier 8120-byte/4284-text/128-byte-frame figures
describe the pre-HQ translation unit and are no longer current.

Integration is after the common decoder and before PCM ring statistics/push.
It is selected only for a decoder that reports two channels at 48 kHz, a
six-channel output ring, and a successfully classified stereo cookie. It does
not consult the session's potentially stale SETUP index. The 48 kHz condition
means index 22 remains rejected until a real resampler is present. APAC 5.1
still takes the measured
`C,L,R,Ls,Rs,LFE -> L,R,C,LFE,Ls,Rs` permutation; index 66 still takes the
pre-existing experimental fold/drop/passthrough policy only when SETUP itself
was 66. Upmix state resets on stream start/stop, rate `1 -> 0`, flush and every
decoder/cookie rebuild, so an
Atmos -> stereo -> Atmos change cannot carry a rear delay tail across tracks.

The integration also removes an independent latent overflow. Cinemo may return
as many as 4096 stereo frames from one `Write`, while the old backwards
in-place `2 -> 6` loop had only 8192 S16 sample slots and could write past it.
Decoded stereo now remains in the source buffer and is rendered chronologically
through a separate PCM8-sized buffer in blocks of at most 1024 frames.

Runtime controls are parsed once and do not themselves enable Spatial or alter
the advertised formats:

```text
AUDIO_STEREO_UPMIX=off|ambient-lite|ambient-hq|hsr-td
AUDIO_STEREO_SURROUND=0.0..0.25
AUDIO_STEREO_LFE=off|assist
```

Unknown upmix values and the reserved `pioneer-rer` and `cupgan` names are
logged and fail to exact `off`; an absent or invalid LFE value fails to LFE
`off`. There is no silent substitution. The checked-in
`carplay_startup.sh` explicitly selects `hsr-td`, rear `0.18`, and LFE
`assist`. These values are read once by `dio_manager`; changing them requires a
service/process restart. `AUDIO_STEREO_UPMIX=off` is the one-variable exact
rollback. None of these controls enables Spatial by itself, and the SAD, CSD,
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
- the LFE LR4 normalized magnitude at `20/40/80/120/200 Hz` is respectively
  `0.996116`, `0.941169`, `0.500026`, `0.164947`, and `0.024942`; correlation against the input
  at 80 Hz is `-1.000000`, confirming the expected -180 degree phase;
- correlated DC settles at the bounded Q15 LFE value, anti-phase `L=-R`
  produces exact LFE zero, and reset clears the complete bass tail;
- reset, arbitrary chunking, independent instances, overlap, full-scale input
  and the recovered SoundAmp bus bounds all pass deterministically.

The host renderer writes WAVE_FORMAT_EXTENSIBLE `5.1(side)` in the exact
`L,R,C,LFE,Ls,Rs` order. On the retained 121.493-second
`session6_stereo_fold.wav`, the LFE-assist canary produced front RMS
`-22.59/-22.98 dBFS`, digital-zero C, LFE peak/RMS
`-25.66/-44.50 dBFS`, and rear RMS `-57.79/-57.96 dBFS` before the amplifier's
known +6 dB surround input gain. That particular fold therefore receives a
very conservative rear field; this is a measurement, not a quality success.
The rendered audition file is
`build/upmix-audition/session6_ambient_lite_lfe_5_1.wav`. The accompanying
`session6_off_5_1.wav` has digital-zero C/LFE/Ls/Rs and its extracted L/R PCM
SHA-256 equals the input (`2cd4dcedc5c4b34e191ca45bd4fa750a7cd50b6cdb6b1025a4b8dab83a854c2f`).

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
  tests, or an ARMv7/QNX benchmark to reproduce or rank the product. It remains
  a licensing or black-box A/B candidate. The implemented `hsr-td` below is an
  independent algorithm in the same broad time-domain class; it contains no DAM
  code or coefficients and does not claim product/output parity. Sources:
  [DAM HSR](https://www.dam-audio.com/research/hsr-high-space-resolution),
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

## Implemented HQ candidate: direction-aware STFT PAE

`ambient-hq` is now a clean-room deterministic implementation, not an alias for
`ambient-lite` or a transcription of Pioneer's coefficients. It operates on
time-frequency regions while leaving the dry fronts, zero center, optional LFE
assist and native-5.1 bypass exactly as described above.

The causal WOLA geometry is fixed:

```text
N = 1024, H = 256, four overlaps, Fs = 48000
w[n] = sin(pi * (n + 1/2) / N)                 # periodic sqrt-Hann
ws[n] = w[n] / sum(j=0..3, w[n mod H + jH]^2) # synthesis window
rear latency = N = 1024 frames = 21.333 ms
```

The inverse FFT is normalized by `1/N`; multiplying its result by `ws` makes
the four `w*ws` products sum to unity. A ring schedules each completed frame
onto its original sample positions and emits it exactly 1024 input frames
later. The first 1024 Ls/Rs frames after init/reset are therefore digital zero.
Dry L/R and the LR4 LFE path are not delayed. Tables are built by bounded
double-precision recurrences at init/reset; the sample path calls no `libm`.

Two forward and two inverse full complex 1024-point FFTs run per hop. Positive
bins are accumulated into 24 Bark-like control bands with boundaries:

```text
0,100,200,300,400,510,630,770,920,1080,1270,1480,
1720,2000,2320,2700,3150,3700,4400,5300,6400,7700,
9500,12000,24001 Hz
```

For each band, one shared EMA coefficient updates every element of its
Hermitian covariance, so a positive-semidefinite instantaneous matrix remains
positive semidefinite after smoothing. The coefficient is `0.20` when the
trace rises and `0.021` when it falls, approximately 24 ms attack and 251 ms
release at this hop rate:

```text
R = [[P_L, C], [conj(C), P_R]], C = sum(X_L * conj(X_R))
T = P_L + P_R
D = min(T, sqrt((P_L-P_R)^2 + 4*abs(C)^2))
lambda1 = (T+D)/2
lambda2 = max(0,(T-D)/2)
```

The `D<=T` clamp contains round-off from the self-contained two-Newton-step
positive square root. Power decisions use `1e-16`; division denominators add
`1e-20`. `gamma` is clamped to `[0,1]`, `cosPhi` to `[-1,1]`, and a band below
the power threshold has no direct subtraction and closes its mask. These are
numerical guards, not tunable acoustic gates.

Let `v` be the normalized principal eigenvector, with its left component real
and non-negative. The implementation constructs it without an angle, selecting
between two algebraically equivalent forms so the small component is never
obtained by subtracting two nearly equal diagonal powers:

```text
if P_L >= P_R:
    v = normalize([lambda1-P_R, conj(C)])
else:
    v = normalize([abs(C), (lambda1-P_L)*conj(C)/abs(C)])
```

When `abs(C)<=1e-16` it selects the higher-power axis. Direct confidence is
limited to coherent, positive-phase material, except for an effectively
one-sided source:

```text
gamma = abs(C) / sqrt(P_L*P_R)
cosPhi = Re(C) / abs(C)
edge = abs(P_L-P_R) / T
s = max(gamma*smoothstep(cosPhi,0.50,0.90),
        smoothstep(edge,0.96,0.995))
wDirect = 1 - lambda2/lambda1
A = X - s*wDirect*v*(v^H*X)
```

Thus mono, an ordinary fixed positive amplitude pan and a genuinely hard-panned
source cancel from the rear estimate; coherent anti-phase material deliberately
does not. The first implementation additionally multiplied `A` by a diffuseness
gate. That was removed after it was measured to suppress legitimate quiet room
energy twice: the projector above is the separation, so the residual is not
gated a second time.

The remaining spectral mask is only an attack protector. Per-bin positive
magnitude flux is summed by band,
`F=sum(max(0,M-M_previous))/sum(M)`, then:

```text
gTransient = 1 - 0.85*smoothstep(F,0.20,0.60)
target = T > 1e-16 ? gTransient : 0
mask += (target < mask ? 0.35 : 0.035) * (target-mask)
```

Those mask coefficients are approximately 12 ms close and 150 ms open. A
frequency mask is zero through 120 Hz, rises smoothly to unity at 220 Hz,
stays unity through 5 kHz, falls smoothly to zero at 7 kHz and remains zero
above. The two inverse results retain the residual's L/R image, then enter the
same unequal two-stage Schroeder all-pass networks and linked limiter as
`ambient-lite`. No bulk delay is added after the STFT.

The shared limiter takes the larger absolute rear sample. Above unity it
reduces the linked gain immediately to `1/peak`; otherwise it recovers by
`gain += 0.0005*(1-gain)` per sample, approximately 41.7 ms. Both outputs are
then bounded to unity before the configured Q15 rear gain. Thus the HQ-specific
mask cannot bypass the same output bound used by lite.

The final review also snaps both the shared delay-allpass output and feedback
to exact zero when `abs(x)<1e-20`. This terminates inaudible binary32 denormal
tails on an MHI2Q whose FPSCR flush mode is not assumed. It applies to lite and
HQ, changes no normal-level benchmark PCM checksum, and adds no allocation or
new scheduling mechanism.

The branch choice above matters in S16 output: always using the second form is
ill-conditioned when `P_L` dominates, and an earlier build left a false loud
left residual. A channel-swap regression now proves the stable branch swaps
the output energy rather than retaining a fixed-left leak.

The implementation is C99, fixed-memory and single-owner. On the retained
pre-HSR-TD QNX 4.9 ARMv7 measurement,
`sizeof(struct spa_stereo_upmix)=59432`, its runtime member was 55304 bytes, the
complete `spa_stereo_upmix_process_s16` frame was 208 bytes, the FFT helper
frame was 72 bytes, and `stereo_upmix.o` had 8180 bytes of text (72904-byte
unstripped object). Current combined-object GCC 8.5 sizes are recorded in the
HSR-TD section below.
There is no render-time heap allocation, new thread, lock, logging, environment
lookup or filesystem operation.

Source basis: [Kraft and Zoelzer, "Stereo Signal Separation and Upmixing by
Mid-Side Decomposition in the Frequency-Domain," DAFx-15](https://www.dafx.de/paper-archive/2015/DAFx-15_submission_9.pdf).

### Measured cost and remaining shipping gate

The fixed 1024-frame host benchmark on the current arm64 Mac, 128 measured
blocks after eight warm-ups, reports mean `144015 ns` and p99 `162042 ns`;
the p99 is 0.760% of the 21.333 ms audio duration. This is useful only as a
same-host regression measurement.

The shipping-flags QNX/ARMv7 binary was also booted under single-thread QEMU TCG
with `-icount shift=0`. Calibration of exact two- and four-instruction loops
agreed to 0.999502. The 128 calls measured mean `4,034,828` and p99
`4,035,856` estimated retired ARM instructions per 1024 frames. The host and
QNX output checksums were respectively `0x2cd85256da3bd0be` and
`0x845172f60b9a95bb`; they are optimizer/build regression values, not a
cross-target bit-exact promise.

The complete percentile rows, toolchain details and measured source hash are
retained in
[`tools/stereo-upmix-render/bench/results/2026-09-04-post-mask-fix.md`](../tools/stereo-upmix-render/bench/results/2026-09-04-post-mask-fix.md).

The QEMU result supersedes the earlier 1.4-1.8-million estimate: this
implementation deliberately uses four complete complex-1024 transforms per
hop rather than a real-FFT wrapper around complex-512. Crucially, QEMU icount
is target instruction retirement, **not** Krait cycles, cache behavior, IPC or
wall time. It cannot prove the p99 `<5 ms` on-unit gate. Startup remains on
`ambient-lite` until a real MHI2Q timing run and stationary acoustic A/B pass.
The denormal snap raised QEMU's estimated instruction mean and p99 by about
1.44% versus the preceding stable-eigen build; that is an instruction delta,
not a claim about Krait cycles or elapsed time. Normal-level host and QNX PCM
checksums were unchanged.

The post-stable-eigenvector retained-file render is
`build/upmix-audition/session6_ambient_hq_lfe_5_1.wav`. With rear gain 0.18 and
the same LFE-assist adapter, Ls/Rs peak at `-40.398/-43.864 dBFS` and have RMS
`-68.918/-71.926 dBFS`. Swapping the stereo input swaps the measured rear RMS
to `-71.927/-68.918 dBFS`; the residual therefore follows source image rather
than a fixed implementation side. The rears in this particular fold are very
quiet, and that is a measurement rather than a quality claim. Do not add
automatic energy equality before listening: stereo does not retain a uniquely
recoverable native surround image, and normalization would manufacture energy
on the opposite side.

That extreme subtlety is content-dependent, not a fixed output level. On the
current ordinary AAC capture `spatial_aac.wav`, HQ Ls/Rs RMS is
`-37.827/-37.436 dBFS`, versus `-39.803/-40.183 dBFS` for `ambient-lite`.
This capture is nearly balanced while the highly coherent fold preserves its
source-relative asymmetry. Neither result proves that HQ is universally better
or louder; both must be judged in the stationary vehicle A/B.

Final host regression passes cover strict compilation, exact zero mono,
positive static-pan and hard-pan rejection, anti-phase passband survival,
independent-field decorrelation, fixed 1024-frame latency, arbitrary callback
chunking, reset/tail removal, saturation and recovered SoundAmp row bounds.
ASan/UBSan pass for both the DSP and AAC<->APAC switch harness. The complete
hook audio suite, 3980-frame APAC integration (`3977` authenticated/reference-
identical APAC packets), the geometry-selected render-policy harness in both
SETUP-65/23 orders for lite, HQ, and HSR-TD, card install test, shipping-flags QNX
object build and QEMU benchmark all pass. The policy harness supplies decoded
geometry directly; it does not instantiate Cinemo/APAC or exercise the live
alternate-decoder retry, which is separately vehicle-confirmed and unchanged.

## Implemented time-domain challenger: `hsr-td`

`hsr-td` is a clean-room time-domain spatial-field decoder in the broad class
described by DAM's public HSR material. It is not a reconstruction of DAM
Upmix: no proprietary code, coefficient, binary, or measured product output was
used, and coefficient/output parity is not claimed.

The signal path reuses the complementary `220/500/1000/2000/5000 Hz` six-band
split. Each band tracks a real covariance matrix with one common EMA
coefficient for all entries, preserving its positive-semidefinite form:

```text
R = [[P_L, C], [C, P_R]]
alpha = 0.020 on attack, 0.0026 on release
lambda1, lambda2 = eigenvalues(R)
v = normalized principal eigenvector
rho = C / sqrt(P_L*P_R)
s = max(smoothstep(rho,0.45,0.92),
        smoothstep(abs(P_L-P_R)/(P_L+P_R),0.96,0.997))
d = s * (1 - lambda2/lambda1)
A = X - d*v*(v^T*X)
```

The covariance updates run per sample; the eigenvector/projector parameters are
refreshed every eight samples (6 kHz control rate). The selected attack/release
coefficients have about 1 ms and 8 ms state time constants at 48 kHz. They and the
unchanged `0.45..0.92` correlation knee are compile-time-only macros so an
environment typo cannot weaken dialogue rejection. A sweep rejected wider
correlation knees because they leaked more near-coherent center material.

Positive coherent mono and stable amplitude pans remain in dry L/R. Anti-phase
or unexplained spatial energy survives into the weighted rear residual. Unlike
lite/HQ, HSR-TD does not pass that residual through the long Schroeder
all-passes: it retains the input residual image and uses only the shared linked
limiter and Q15 output gain. It performs no FFT, allocation, lookahead, block
scheduling, logging, lock, thread creation, or sample-path `libm` call. Output
begins in the current frame; the IIR analysis still has its ordinary causal
filter response, so this is a no-block-latency statement rather than a
zero-phase claim.

Regression covers immediate output, exact mono and positive fixed/moving-pan
rear rejection, diffuse-to-dialogue tail cleanup, near-coherent center
rejection with simultaneous anti-phase survival, exact L/R-swap equivariance,
arbitrary callback chunking, reset, saturation, overlap safety, recovered
SoundAmp row bounds, and fresh state across AAC/APAC cookie transitions. After
a diffuse-to-mono transition the selected release reduces the following 80 ms
rear tail from -48.07 to -61.92 dB, while exact mono remains digital zero.

With the QNX 6.5 ARMv7 GCC 8.5 build, 128 measured 1024-frame calls under
calibrated QEMU `-icount` report mean/p99 `1,089,350/1,091,467` estimated
retired instructions, versus `3,947,842/3,949,085` for `ambient-hq` built in
the same run. The means are about `51.06/185.06` million estimated instructions
per audio second, making HSR-TD 3.62x lighter by this one measure. These are not
Krait cycles, cache behavior, IPC, or wall time. The current GCC 8.5 layout is
`sizeof(state/runtime)=59624/55496`, process stack usage is 216 bytes, and the
unstripped object is 100632 bytes with 9706 bytes of text.

A reproducible open-multitrack evaluation creates controlled 5.1 fixtures in
nine development genres, folds them to stereo, and compares all three backends
to the known fixture. Another 24 deterministically selected MUSDB tracks are
held out from tuning. At rear gain 0.18 the development rear waveform
correlations are `0.264/0.085/0.151` for HSR-TD/lite/HQ and the held-out values
are `0.395/-0.010/0.101`. Held-out SI-SDR is
`-8.20/-14.98/-12.86 dB`; fold-back SNR is `33.82/34.66/32.42 dB`. HSR-TD
therefore improves waveform/scale agreement, while lite still wins envelope,
interchannel-correlation error, energy restraint, and fold-back SNR.

The shared LFE assist is also scored independently. On the nine development
fixtures its reference-envelope correlation/SI-SDR/energy ratio are
`0.816/4.22 dB/1.232`; on the 24 held-out fixtures they are
`0.603/1.13 dB/0.769`. The recovered visible SoundAmp bus peak is at most
`0.620` and `0.503` respectively. Because discrete LFE is omitted from the
stereo fold, this measures bass-following behavior, not recovery of the
original LFE. The fixtures are not artist-authored 5.1 masters, stereo
fold-down is non-invertible, and decorrelation invalidates waveform-only
ranking. This is regression/comparative evidence, not a quality verdict.

Exact commands, percentiles, hashes, corpus limitations and aggregate results:
[`2026-09-04-hsr-td.md`](../tools/stereo-upmix-render/bench/results/2026-09-04-hsr-td.md).

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

## Gain pairing and headroom proof

The LFE-disabled profile remains:

```text
PCM L/R  = 0.997 * source L/R
PCM Ls/Rs = 0.20 * bounded ambience L/R
PCM C/LFE = 0
```

With this, the visible SoundAmp profile-0/bank-1 buses become approximately:

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

Allow a runtime surround-gain range of `0.0..0.25` (the useful initial range is
roughly `0.15..0.25`, or -16.5 to -12 dB at
the PCM input). Do not expose values above 0.25 until a retained PCM capture has
been passed through the amplifier model and checked for bus overload.

`front_gain=0.997` closes the bounded discrete-sample calculation but leaves
almost no inter-sample true-peak margin. If the policy is a strict -1 dB ceiling
at every visible logical bus, start near `front_gain=0.889` instead and restore
loudness only after oversampled measurement.

The LFE-assist profile is a separately bounded pair:

```text
F = 30802 / 32768 = 0.94000244       front gain
B =  5833 / 32768 = 0.17800903       LFE gain after clamped LR4
R =  5898 / 32768 = 0.17999268       checked-in rear gain
Rmax = 8192 / 32768 = 0.25           allowed rear maximum
```

Using absolute row sums from the recovered profile-0/bank-1 matrix gives:

```text
at R=0.18:
  Bus L/R  <= 0.794328*F + 0.398107*B + 0.707946*R = 0.944962
  Bus C    <= 2*0.501187*F + 0.316228*B             = 0.998525
  Bus LFE  <= 1.412538*B                            = 0.251445
  Bus Ls/Rs<= 0.199526*B + 1.995262*R               = 0.394650

at Rmax=0.25:
  Bus L/R  <= 0.994523
  Bus C    <= 0.998525
  Bus LFE  <= 0.251445
  Bus Ls/Rs<= 0.534333
```

The module clamps the normalized LR4 output before applying `B`, so the proof
also covers the filter's finite step overshoot. Configuration validation uses a
conservative integer Center guard:

```text
1002375*front_q15 + 316228*lfe_q15 <= 1000000*32768
```

These are discrete-sample bounds at the recovered logical input matrix, not
true-peak or acoustic bounds. Bus C has only about 0.013 dB of margin, and the
unknown downstream SHARC stages may add gain. The assist is therefore a vehicle
canary, not a proved no-clip production profile.

## Center remains zero; LFE is explicit and reversible

This applies only when the decoder produced a two-channel source: there is no
original center or LFE to discard. Native APAC 5.1 retains its real C and LFE
samples unchanged (apart from the already-measured channel permutation).

### Center

Bank 1 already makes `Bus C ~= (L+R)/2`. An explicit center channel would mostly
duplicate the same correlated material and alter front tonal balance through
the C-to-L/R routes. A later cinema/dialogue profile may add a small, coherence-
controlled C feed, but only after the ambience-only profile is measured.

### LFE

The stereo source's full-range bass remains in dry L/R, so synthesizing LFE does
not restore missing source content; it is a restrained bass-assist experiment.
The implemented signal is:

```text
M = (L + R) / 2
PCM LFE = 0.178 * LR4_lowpass_80Hz(M)
```

The LR4 is two identical second-order Butterworth sections in transposed DF-II,
with double-precision state and no `libm` dependency. It is -6.02 dB and -180
degrees at 80 Hz. The fronts are deliberately still full-range; this is not a
complementary crossover. Because the SoundAmp matrix leaks LFE into L/R and C,
the electrical model predicts a small notch near 80 Hz, while the physical
subwoofer polarity and downstream crossover phase remain unknown. LP80 also
passes DC; decoded music should have negligible offset, but the behavior is
explicitly tested. A stationary bass sweep, polarity check, transition-click
test and level-matched LFE-off/assist A/B are required before calling it final.

## Integration point

The stateful `hook/audio/stereo_upmix.c/.h` helper is called from
`spa103_render_frame()` after the selected decoder returns PCM and before ring
statistics/push. The active check is successfully decoded stereo at 48 kHz and
a six-channel ring. It deliberately ignores the possibly stale SETUP index:
the decoder that accepts the changed inline cookie, followed by its reported
geometry, is the source of truth. This covers advertised index 23 today without
coupling the DSP module to AAC or Cinemo.

The old backwards in-place widening was replaced. Decoder output stays in its
8192-sample source buffer and a separate 8192-sample render buffer is filled
forward in blocks of at most 1024 frames. This is both the temporal-DSP order
required by the filters and the bound required when Cinemo releases several
AAC callback blocks from one access unit.

State lifetime:

- initialize at buffered-render start even if the first format is index 65;
- reset filters, delay lines, coherence and limiter on stream start,
  `FLUSHBUFFERED`, rate-to-zero and every decoder/cookie replacement;
- reset immediately on every AAC <-> APAC cookie transition;
- bypass native APAC 5.1 through only its measured channel reorder, with no
  stereo synthesis; retain the unmeasured PCM8/5.1.2 policy solely behind the
  pre-existing experimental SETUP=66 gate;
- perform all work on the existing APAP receiver thread.

Implemented runtime gate:

```text
AUDIO_STEREO_UPMIX=off            exact L,R,0,0,0,0 compatibility behavior
AUDIO_STEREO_UPMIX=ambient-lite   six-band time-domain canary
AUDIO_STEREO_UPMIX=ambient-hq     implemented 1024/256 STFT PAE candidate
AUDIO_STEREO_UPMIX=hsr-td         clean-room six-band time-domain covariance candidate
AUDIO_STEREO_UPMIX=pioneer-rer    reserved; logs unsupported and uses off
AUDIO_STEREO_UPMIX=cupgan         reserved; logs unsupported and uses off
AUDIO_STEREO_SURROUND=0.18        bounded initial tuning value
AUDIO_STEREO_LFE=off              code default; front Q15 32670, LFE zero
AUDIO_STEREO_LFE=assist           LR4/80 Hz; front Q15 30802, LFE Q15 5833
```

The reusable code default is LFE `off`; the checked-in startup explicitly opts
the current canary into `ambient-lite` plus `assist`. `AUDIO_STEREO_UPMIX=off`
forces exact `L,R,0,0,0,0` regardless of the LFE variable. Controls are parsed
once, so a rollback takes effect after restarting `dio_manager`/the CarPlay
service. A codec transition must not change PCM6 geometry, SAD, CSD or the
amplifier connection. Although `ambient-hq` and `hsr-td` are selectable,
startup deliberately does not select either until the vehicle A/B and real
Krait timing gate pass.

## Offline proof before deployment

The host renderer in `tools/stereo-upmix-render/` runs `off`, `ambient-lite`,
`ambient-hq`, and `hsr-td` against any 48 kHz/S16 stereo WAV and writes generated
`L,R,C,LFE,Ls,Rs` PCM6. It remains the common shell into which the recovered
Pioneer rear oracle and any future CUpGAN backend should be added.
The remaining oracle work should write both:

1. generated `L,R,C,LFE,Ls,Rs` PCM6;
2. predicted logical SoundAmp buses after multiplying it by the recovered
   profile-0/bank-1 matrix.

Required deterministic tests:

- `L=R`: no Ls/Rs after filter settling; Bus C remains the mid signal;
- `L=-R`: Bus C and synthesized LFE are zero and rear ambience is active;
- hard-left and hard-right: the direct source stays in front and synthesized
  rear leakage stays below the limit;
- silence and reset: no stale all-pass/delay tail after reset;
- full-scale correlated and anti-correlated vectors: no S16 wrap and no visible
  bus above the chosen limit;
- LFE response at `20/40/80/120/200 Hz`, phase at 80 Hz, DC steady state,
  clamp, denormal decay and all four recovered SoundAmp row bounds;
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

1. `off`: exact `L,R,0,0,0,0` baseline;
2. `ambient-lite`, rear 0.18, LFE off;
3. `ambient-lite`, rear 0.18, LFE assist;
4. repeat step 3 with bass-heavy material and a slow 20-160 Hz sweep;
5. only if clean, compare implemented `hsr-td`, HQ, and future Pioneer
   candidates at matched predicted rear-bus levels; record real on-unit p99
   callback timing for each.

`pioneer-rer` enters this sequence only after the host oracle is reproducible,
the clean-room result nulls against it, and the same realtime gate is met.

Accept the upmix only if dialogue remains anchored in front/center, rear vocal
leakage is low, reverberation and wide effects become audible behind the seats,
bass-assist has no cancellation/boom or polarity defect, no
Atmos-to-stereo-to-Atmos transition click appears, and the native APAC 5.1 path
remains identical. “Party mode” copies of L/R into Ls/Rs are excluded:
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
