# J525 remaining DSP blocks — 2026-09-19

Source: raw V850 LE32 CodeFlash, file/load base 0, 0x180000 bytes,
SHA-256 `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
SHARC PCs are **16-bit short-word addresses**. No vehicle writes/deploy.

This extends the known electronic model to `bank1-known-electronics-v5`.
It does **not** certify the complete amplifier or explain the listening report
by itself. Decoded instructions, reconstructed math and live state are separate
levels of evidence.

**V5 correction:** v3/v4's C4D17 reference wrongly used `10^(code/10)` for
control conversions. The instructions load `0x3D4CCCCD` = float32(0.05), then
multiply by ln(10): the correct mathematical law is `10^(code/20)`, including
power-vector scales, floor, cap and common gain. The old synthetic target
numbers and claimed 24/20-dB combined ceilings are withdrawn. Corrected
ceilings are **15/10 dB** for profiles0/1 with the extracted unity other factor.
Tests now pin the actual instruction constants, not just a matching formula.
Old static identity-path music reports are unaffected by this correction.

## Newly recovered

| Block | Result | Remaining boundary |
|---|---|---|
| C4B59 / C4DDA / C4EE3, handler 12879C | Gain magnitude smoothing, then target sign; block-constant multiplication. Modes 0/2/3 implemented independently. | Mode 1 step generation and live gain targets/ramp state not emulated. Final stored gain remains steady-state in AmpChain. |
| C4E9A / C4ED3, handler 12823C | Output-feedback peak limiters; independent and linked kernels implemented, including state. | Actual live parameters, especially the second limiter's protection-dependent thresholds. |
| C4E89 / C4EAA, handler 126A95 | Packed software EQ; two records each on cfg12/13/14/15 only. Non-unity binding now supported. | Live coefficient changes; stored records are unity. |
| First mixer B, wrapper 124DF3 | Fixed mode2/5 references: mode5 adds scaled third input to both stereo lanes, then spectral wrappers, two/six/four tails and eight-output finish, with explicit parameters/state. | Upstream input graph, other modes, lifecycle, hardware FFT/rounding validation and live parameters/selector; B=0 is still the default model hypothesis. |
| Second mixer B | Schedule 21..50 traces to auxiliary graph inputs 8/9/10; no direct music-input 0..7 edge. | Input meanings/live activity and residual state; do not equate source separation with unconditional silence. |
| C4D17 reference | Full-mode composition; corrected control scales; separate startup/mask/calibration sequencer and noise-core references. | Calibration audio composition/rounding, physical auxiliary source/level, live state and hardware numerical equivalence. Default remains identity. |

The [reproducible inventory](../tools/audit_amp_remaining_blocks.py) emits
[instruction evidence and JSON](../assets/soundamp/remaining-blocks-20260919/audit.json).
The newly covered handlers contain 172 gain, 208 limiter, 127 software-EQ and
243 B-wrapper/callback instructions, with no decoder UNKNOWN operations.
This is decoding coverage, **not execution-equivalence proof**.

## What −99 means

An input-matrix entry of −100 dB is clamped by its writer to −99 dB:
amplitude `10^(-99/20) = 0.00001122018` of the input. It is not a +99 dB boost
and not exact silence. The pair mixers have different mute encodings; do not
apply one block's sentinel to another. This leakage cannot explain a broad
audible bass surplus at ordinary signal levels.

## Gain stages: targets are not loader zeros

`12879C` fields +9/current, +12/target, +10/mode, +22/previous-mode.
The multiply kernel `1288C4` uses one resulting gain per channel for the block.

Mode 0 copies target immediately. Mode 2 adds a signed step to magnitude.
Mode 3 multiplies magnitude by the selected up/down ratio, clamps at the target,
and copies the target sign. With a nonzero target it first floors magnitude at
float word `3727C5AC` (~1e-5). Zero-target mode 3 decay is asymptotic, not an
immediate exact mute. Mode 1 computes steps using further state; not implemented.

V850 `A7D2C` writes ten targets at `C0414`. In tenths of a dB:

```
10 * s8(gp-5E2F) - 82
  + table_1028E8[channel][9+s8(gp-3E38)]
  + table_102A64[channel][9+s8(gp-3E34)]
```

Channel 3 additionally uses table `102C52[9+s8(gp-3E2F)]` when gp-3E30 bit0
is set, otherwise target is −1000. Final clamp is [−1000,+400]. All three
tables have **zero at the neutral index**. The −82 is a **common −8.2 dB
offset**, not a bass-specific boost. This does not prove the live indices.
`A7E2A` writes the same `u8(gp-458E)` tenths-dB target to all ten channels at
`C203A`; its live value is not in static calibration.

## Limiters: mode zero is NOT bypass

For independent channels (descriptor +15 bit0 clear):

```
y[t,c] = x[t,c] * gain[t,c]
rate = attack[c] if abs(y[t,c]) > threshold[c] else release[c]
gain[t+1,c] = clamp(gain[t,c] * rate, minimum[c], maximum[c])
```

Linked mode uses one gain for every channel, attacks if **any** channel exceeds
its threshold, and uses channel-zero attack/release/bounds. No lookahead or
hard clipping: the first overload sample is emitted before gain changes.

Loader gain/max = 1 and min ~1e-6. Calibration-derived thresholds, config order:

| Config candidates | C4E9A static | C4ED3 calibration basis |
|---|---:|---:|
| cfg0..11 | 100 | 0.81..0.95 |
| cfg12/13, front woofers | 1 | 0.59 |
| cfg14/15, sub windings | 1 | 0.50 |

Values are internal floating amplitudes, **not acoustic SPL**, and not directly
comparable to the unattenuated v2 report: its volume stages were omitted.
The second limiter's writers `A8548/A8710` additionally use gp-41AA,
gp-3E2B, tables `6A3FC/6A37C` and the result of `9B070`. Thus the stored
0.59/0.50 numbers are not certified live thresholds. With maximum gain 1 and
initial gain <=1 these limiter kernels cannot themselves boost amplitude.

## C4D17 five-band kernel

Initialization `126FAD..126FC2` binds coefficients from descriptor +71 to the
helper structure +74; coefficient address is `E07EA`, 24 floats. Each triple
is `[a,b1,b0]`, with transfer `(b0+b1*z^-1)/(1-a*z^-1)`.
The four LP/HP pairs split at approximately 250, 500, 1000 and 2000 Hz.
Each next pair processes the previous HP remainder. Four LP bands plus the
last HP remainder form five bands; adjacent channels are SIMD stereo pairs.
All five stereo pairs share the five band gains. Per-block interpolation uses
`gain + (sample_index+1)*step`; stored gain advances by `frame_count*step`.

The LP/HP responses sum to unity within extracted float32 coefficient error.
Consequently all five gains=1/steps=0 reconstruct the input; unequal gains
change spectral balance. Tests cover neutral reconstruction, a common ramp
and partition invariance with unequal gains. This does not prove the firmware
uses neutral gains during playback. Their loader defaults are not live values.

The full block has stage gates 0..26. Full-mode sideband masks differ at bits
9 (this five-band kernel), 11 (common output gain ramp), and 25 (gain update).
Neither sideband state alone proves whole-block identity: other stages remain.
An automatically extracted stage/call inventory is in the evidence JSON.

### Follow-up: recovered adaptive control and startup gates

Implementation: [amp_complex_control.py](../tools/soundamp-virtual-cabin/amp_complex_control.py).
This was initially isolated kernel coverage; see the v4 composition below.
The old music reports still substitute identity for C4D17; do not use those
reports as proof that adaptive processing cannot cause a bass difference.

V850 startup `B64E0` compares eight-byte signature `AD301A07` at `94C54`
with `100000` and `10FFF0`. All three match in this exact dump. That branch
copies fixed calibration and sets `gp-1250C=A5`; fallback would use `5A`.
`B675C` sets r8=165 and `B69E6` stores it to `gp-3D62`, without an intervening
r8 write/call. `B6810` initializes profile selector `gp-4592` to **0**.
Subsequent commands can change these fields: this is a startup path, not live
readback. In particular `D4002` accepts a new `gp-3D62` from a command payload,
and `D6EEA` can change the profile. **MOST bank 1 is not profile index 1.**

Given both flags A5, writers `AA912/AA9B6` select descriptor +15=1 (adaptation)
and +39=4 (full stage mask). Graph97's first-word bit0 chooses the full masks
and sets descriptor +6, which inhibits adaptation/cap application. Thus static
startup evidence does not justify declaring this block disabled.

`AAC88` writes 20 descriptor fields from `103238 + profile*20644`, or the
fallback table `8F3FC`. `AAFC0` sets cap code +16 from an unsigned byte divided
by 10. Complete fields, addresses, FIR-bank hashes and profile distinctions
are now in `audit.json / complex_control_profiles`.

| Parameter | Calibrated profile 0 | Calibrated profile 1 |
|---|---:|---:|
| +16 cap code | 15 | 10 |
| +22 adaptive rise ratio/update | 1.01449525 | same |
| +23 adaptive fall ratio/update | 0.95361924 | same |
| +24 other-factor rise ratio | 1.00722158 | same |
| +25 other-factor fall ratio | 0.99283028 | same |
| +34 skip common output ramp | 1 | 1 |
| +38 link upper two bands | 0 (independent) | 0 |
| FIR bank nonzero words | 5120 | 10, all subnormal when interpreted as float32 |

Profile1's unusual FIR bank is a reason to check the actual selector, not to
silently replace it with profile0. Static FIR coefficients are initialization
data: helper `125F36` subsequently adapts them.

Stage19 scales the two five-band power vectors with `10^(code/20)`. Field37
applies a **prefix minimum** to vector +136. With x=+135 and y=+136, helper
`12796D` returns 1 if x<=xmin or y<=ymin; otherwise:

```
p = float32(0.27), q = float32(1.8518518)
x = min(x,1); y = min(y,1)
g = max(1, min((1 + (y**p - c)*x**(-p))**q, helper_cap))
```

This expression describes the positive log-domain case. The actual log helper
returns zero and errno=33 on nonpositive arguments; the reference preserves
the numeric fallback rather than silently taking log of a negative number.
Stored helper parameters are xmin≈1e-10, ymin≈1.58489e-9,
c≈0.0042072665, helper_cap≈15.848932. Initialization sets that cap from field46.
`124393` and `12512E` are natural log/exp approximations: scalar reconstruction
agrees with independent mathematical log/exp to <1e-9 absolute / <3e-10 relative
over the tested normal-domain grids. That is not a SHARC bit-rounding claim.

Stages22..26 implement freeze, enable/inhibit, separate other-factor scaling,
rate clamps and endpoint interpolation. Stage23 requests adaptive gains=1
when disabled; stage25 can still smooth that request from older boosted state
when its force-smoothing flag is set. Enable/cap changes normally skip that
rate clamp. Field38 **set**, not clear, links band3=(band2+1)/2 and band4=1.
Final band endpoint is `min(max(adaptive*other,1), cap)`; cap is
`10^(field16/20)` when allowed, otherwise 1. Ramp slope is endpoint difference
divided by **1024** samples. These endpoints are amplitude multipliers, not SPL.
For the extracted unity other factor, steady helper/cap limits are about
15 dB (profile0) and 10 dB (profile1); these are ceilings, **not measured boosts**.
The common target `10^(-field17/20)` is computed, but field34=1 skips its audio
application. Do not assume it cancels the band boosts.

### Delay and detector kernel

Stage8/helper `1279ED` writes a stereo sample, advances a ring of length
`clamp(requested,0,maximum)+1`, then reads. Five loader parameter pairs are
requested=maximum=1023. Therefore full-mask stage8 contributes **1023 samples
(21.3125 ms at 48 kHz)** to all ten music channels, not 1024 or zero. The new
`ComplexStereoDelay` tests cover impulse position, clamping and partitioned
processing. This conditional delay is still excluded from AmpChain's default
identity hypothesis and its reported total delays; it does not itself boost bass.

Helper `125F36` computes a residual between a weighted auxiliary signal and
ten weighted music-reference FIR predictions, then updates the 10×512 FIR
coefficients with residual×step×history (LMS form). Reference power chooses
a step-table entry, with immediate power rise and smoothed fall. `AdaptiveResidual`
implements this in canonical tap order; tests cover independent convolution,
coefficient-update timing and power-dependent adaptation. The subsequent v4
tests also compare the canonical history against a literal 5120-word global
ring for 1040 detector samples, including coefficient updates (error <1e-14).
This validates the reconstructed addressing, not processor rounding/execution.
MODE1 `0x2000` here is **ALUSAT**, not TRUNC; FIX rounding is an explicit input
to this reference. The physical identity of graph input11 remains unverified.

### V4: composed full-mode reference and exact detector binding

Implementation: [amp_complex_path.py](../tools/soundamp-virtual-cabin/amp_complex_path.py).
Root scheduler `12677D..12682B` reads root descriptor `C4F92+9=64` into R11
at `1267AA`, then passes it to node R12 at `1267E7`. Therefore a normal
C4D17 audio call is **64 frames**. Descriptor +13=4 detector frames and
+127=16 history blocks produce one target update per **1024 audio samples**.

The full-mode path is now composed in this order:

1. Stage5 filters auxiliary graph11 through two scalar biquads (field36 bit0
   bypasses them). Input must be supplied **after** upstream scalar node C4D11.
2. Stage6/helper `1283F6` forms a weighted **sum of ten INPUT music channels**.
   It does not apply ten pre-gains to the output audio. Stage7 packs five music
   pairs plus a sixth pair `(auxiliary, music sum)`.
3. Stage8 delays the five music pairs; stage9 applies five-band gains unless
   sideband bit0 is set. Stage10 writes output audio; stage11 optionally applies
   the common ramp only there (default field34=1 skips it). The common ramp
   starts at the old gain, whereas the five-band ramp uses sample index+1.
4. Stage12 filters all six packed pairs through eight biquads, with global input
   scale descriptor +97. Coefficients at E1C2E are `[-a2,-a1,b2,b1]`, b0=1.
5. Stage13/helper `127A80` takes the **first four frames** from each filtered
   64-frame pair: stride2 interleaved loads, not every sixteenth audio sample.
   This is a literal reconstruction, not an invented ideal resampler. No
   hardware validation of this unusual selection is claimed.
6. Scratch graph96 offsets are auxiliary=0, ten references=4,8,…,40,
   residual=44, music sum=48 (four words each). Initializer `126F3B` advances
   the final pointer by an extra four before `126F40`; residual and sum are
   separate buffers. LMS computes auxiliary minus FIR-predicted references.
7. Stage16 applies five **parallel**, not cascaded, detector biquads to
   `(residual, music sum)`. Stage17 accumulates four-sample energy per band/lane.
   Stage19 takes the maximum **block energy** over sixteen blocks ×0.25 when
   field18 bit0 is set; the alternative is sum×0.015625. This is not sample peak.
   Lane0 residual becomes y/+136; lane1 input music sum becomes x/+135.
8. The recovered target controller runs after audio. New ramps start on the
   **next** audio block and span the following 1024 samples.

Added instruction ranges: scheduler 82, detector prefilter 93, pair biquad 68,
unpack 27 and weighted sum 45 instructions; all decode without UNKNOWN.
The FIR reference remains float64; SIMD/40-bit rounding and startup history
are not fully emulated. The model starts from zero history and neutral gains,
with an explicit caller-supplied initial FIR bank and fixed mode/sideband.

### Corrected V5 synthetic sensitivity check — not a bass diagnosis

[Runner](../tools/soundamp-virtual-cabin/audit_amp_complex_scenarios.py) verifies
the CodeFlash SHA and initial FIR-bank SHA before use. It runs five seconds of
four synthetic tones at the C4D17 music boundary, explicit graph11 63-Hz tone,
stored base power tables **with zero additive runtime offsets**, enable=1 and
nearest FIX rounding. No MMI knob positions or microphone calibration are
inferred. It is not a music render, cabin-noise model or actual measured gain.

| Profile / fixed sideband | Auxiliary amplitude | Final five-band target dB |
|---|---:|---|
| 0 / 0 | 0 | 7.05, 6.33, 6.97, 4.66, 4.51 |
| 0 / 0 | 0.01 | 6.00, 6.10, 6.72, 5.84, 5.66 |
| 0 / 0 | 0.1 | 8.44, 9.24, 10.08, 12.75, 12.59 |
| 0 / 1 | 0.1 | 0, 0, 0, 0, 0 |
| 1 / 0 | 0 | 0, 0, 0, 0, 0 |

Each run performs 234 target updates; endpoint dB is **not output spectral dB**.
Profile0 zero auxiliary is not bypass: its nonzero initial FIR can itself
produce a nonzero residual from music references. Profile1 has radically
different initial FIR data. This is why neither zero auxiliary nor swapping
profiles is a safe stand-in for the live amplifier state. These synthetic
scenarios establish sensitivity, not which scenario the car uses.
Artifacts: `complex_scenarios_profile0.json` / `complex_scenarios_profile1.json`
beside the instruction audit.

## C4D17 startup, mode selection and calibration sequence

[amp_complex_modes.py](../tools/soundamp-virtual-cabin/amp_complex_modes.py)
is a separate control/noise-core reference, **not silently enabled in AmpChain**.

`126DDE` first tests descriptor +40 bit0. Clear means return with **no output
write**, not output zeroing and not guaranteed bypass. Next +5 bit0 guards
the one-time binding of pointers; the binding call sets +5 and returns without
audio processing. Turning master enable off/on does not itself clear +5.
Loader +40=0/+5=0 are not playback state (V850 commands can change them).

At `127021..127050`, mode word +39 is tested bitwise in order. Bit2 wins over
bit1 over bit0; mode7 is therefore full-mode, not a special unknown enum.

| Selected bits | Mask | Effect after initialization |
|---|---|---|
| none of 0..2 | 0x2 | Clear ten music outputs |
| bit0, without higher bits | 0x1 | Copy ten music inputs |
| bit1, without bit2 | 0xF4BA | Calibration/test sequence |
| bit2, sideband bit0 clear | 0xFFF7FE5 | Full reference path |
| bit2, sideband bit0 set | 0xDFF75E5 | Full path with stages9/11/25 skipped |

Calibration mask contains only stages1,3,4,5,7,10,12,13,14,15. It clears
music outputs first, generates a test signal on selected channels, filters the
auxiliary/reference data and learns FIR. It does **not** run the five-band
audio gain or stages16..26 power/target controller.

Stage3 implements descriptor +11 states configure4 → wait1 → measure2.
Channel bit +41 scans 1..512, skipping bits absent from +31. Configure sets
the test-output gates and FIR reference weights, clears FIR **history** at
+78 (not the coefficient bank at +77), and returns. Wait adds each audio
block to +43. Measurement adds each block to +44 at stage15. Stored thresholds
are +42=144000 (3 seconds) and +32=5760000 (120 seconds), at 48 kHz; they are
not inferred live timing. At the end of the tenth channel, `1270FD` marks
completion but jumps to stage4: one final test block uses the previous gates.
The next call clears outputs and returns on +10. Tests cover this fall-through.

Test generator `126CEC..126DB2` now decodes completely (96 instructions).
Its integer core uses 16807, 127773, 2836 and modulus 2^31−1, with a 32-word
shuffle table. This is reconstructed rather than substituted with a host RNG.
The modular reference requires seed/shuffle/previous value and an explicit
ALUSAT choice; exact FIX/FLOAT rounding near shuffle boundaries is not claimed.
Generator type1 uses two built-in biquads at B94F4/B94F9, type9 uses descriptor
+14/+15; other types use the raw core. Those optional shaping paths are decoded
but not composed in `CalibrationNoiseCore`.

Upstream graph11 node C4D11 is a scalar multiplier (`127A4D`, descriptor+5).
Loader scalar `0x3FFF64C1`≈1.9952623 is **+6 dB**, not unity. Full-mode scenarios
continue to accept auxiliary **after this node**; do not multiply it twice.
This establishes a stored digital factor, not physical microphone identity,
live scaling or acoustic calibration.

## First B processor

`124DF3` copies graph inputs `[24,23,22,97,97]` into five 512-frame lanes,
then copies eight result lanes to `[20,19,18,17,34,35,40,41]`. Input base
`B95EA`, output base `BA9EA`, buffer selector and sub-block offset at descriptor
+7/+6. On each completed 512-frame bank it schedules callback `124F56` via
event 0x29. Callback calls `12DD7D` for initialization, `12DE3B` for processing,
`131A65` for parameters; some inactive/error branches clear the result bank.
There is no proof that the user's playback takes those clearing branches.

Further decoded: library init `12DD7D..12DE3B`, processing wrapper
`12DE3B..12DFA8`, clear helper `12DD16..12DD67` and algorithm dispatcher
`131208..13163D` (82/161/34/477 instructions, no UNKNOWN operations).
Readiness is word B89BA, the allocated-object pointer is B89B9, frame count
B89BC and algorithm selector a float at object+1128. The dispatcher selects
multiple algorithm callees; decoding it does not emulate those callees.

B909B=1 is a **transition** state: clear five inputs, still run the algorithm,
then clear eight outputs. After counter B89BF reaches 10, state/counter are
cleared (assuming successful calls and no intervening state changes). Writers
13246F/1324AE/132540 set this state during parameter updates. This transient
mute does not establish permanent B-path silence. Snapshot B89BA/B89B9/B89BC/
B909B/B89BF and object+1128/+1176/+1177 to select a meaningful next branch.

### First-B filter subgraph

`12DBDE` reads a rounded section count from the parameter bank. Its state2
branch clears channel audio and calls reset `12D94C`; it is not an identity
bypass. With zero sections and a non-state2 control it leaves audio untouched.
The active branch interpolates coefficients through `12FAF2`, validates poles
through `13163D`, then processes per-channel cascades through `12DAA5`.

`12FAF2` uses registered target/step arrays B903B/B8CFD relative to base B903A:
if |target−current|>|step|, add the supplied signed step; otherwise land on
target and clear step. A zero or wrong-sign step is not repaired by firmware.
`13163D` checks strict `|a2|<1 && |a1|<1+a2` after negating feedback words.

Source records `[b0,b1,b2,-a1,-a2]` are reversed by `12DAA5` for kernels.
Scalar `12B529` computes `v=x+fb1*z1+fb2*z2`, then
`y=b0*v+b1*z1+b2*z2`. Pairs use `12D9A9→12B719`; an odd final section uses
`12DA2F`. That wrapper switches to first-order `12BB91` when |fb2|<float32(1e−5),
dropping b2 as well: do not silently make that test depend on b2.
[amp_b_filters.py](../tools/soundamp-virtual-cabin/amp_b_filters.py) implements
the fixed-bank mathematical cascade plus independent guard/ramp helpers.
Tests compare against scipy SOS and cover the odd-section exception.
This does not emulate all library reset/interpolation state or its spectral
and mixing processors. Object constructor `130EF3` and copy helper `132E08`
were also decoded, not used to fabricate a live object or selector.

### Two-channel B tail and its actual mixer destinations

`132F86..1330E6` processes two channels, ultimately B output lanes6/7:
copy inputs → common delay → separate channel delays → common scalar gain →
shared EQ bank → per-channel nested all-pass → per-channel single all-pass →
common final scalar gain. The fixed successful branch is implemented in
[amp_b_decorrelator.py](../tools/soundamp-virtual-cabin/amp_b_decorrelator.py).
Parameters must be supplied explicitly; this is not plugged into AmpChain's
default B=0 hypothesis. Changing delays/resampling, allocation/error paths,
coefficient transitions and EQ reset/mute state are not emulated.

Parameter bindings relative to the allocated library object (word offsets):

| Component | Binding |
|---|---|
| Shared delay, `12F411` | pointer loaded from +1160 |
| `12EB9B`: two delays, gain, EQ | inline bank +615; fields +0/+1 delays, +2 gain, +3 onward EQ |
| Left nested / single all-pass | inline banks +659 / +664 |
| Right nested / single all-pass | inline banks +667 / +672 |
| Final gain, `132D0B` | pointer loaded from +1158 |

Single all-pass `12B90B`: `v=x−g*d; y=d+g*v; ring←v`.
Nested `12D10E` is **not two ordinary all-passes in series**:
`u=x−g2*d2; v=u−g1*d1; w=d1+g1*v; ring1←v; ring2←w; y=d2+g2*u`.
Parallel clauses at `12D187` read old operands. Fixed positive integer delays
and stable |g|<1 yield flat unit magnitude for each all-pass; coherent addition
with another path need not be flat. This does not make the surrounding gains
or EQ neutral. Independent transfer-polynomial tests cover both all-passes,
the full active tail and state continuity across arbitrary block partitions.

The graph join gives a crucial qualification to any bass hypothesis:

| B output lanes | Graph ports | First mixer B slots | Stored row1 gains |
|---|---|---|---|
| 0/1 | 20/19 | 0/1 | 0/0 dB |
| 2/3 | 18/17 | 4/5 | 0/0 dB |
| 4/5 | 34/35 | 6/7 | 0/0 dB |
| 6/7 (two-channel tail) | 40/41 | 8/9 → output graph58/59 | −99/−99 dB |

This is extracted from C4AD9 and C4B4F ports, with row1 gains from CodeFlash,
and regression-tested. Thus the newly modelled all-pass pair has a strongly
attenuated **direct** contribution under row1, not a unity route to the first
bass buses. Conversely, the other six B lanes are not muted by that row.
This is a stored-row fact, not a readback of the user's active state, nor a
bound on arbitrary upstream gain or all possible indirect interactions.

### Spectral helpers: recovered math, not a completed spectral upmixer

Wrappers `12E80E`, `12E1BB`, `12E30A` (ends at `12E3E6`) and `12E521` are
decoded. They schedule 1024-point transforms/512-frame output blocks and
process interleaved real/imaginary arrays. Real packing, wrapper scaling and
ideal window/overlap and fixed-mode2 composition are now tested below;
whole-dispatcher and hardware execution equivalence remain open.

[amp_b_spectral.py](../tools/soundamp-virtual-cabin/amp_b_spectral.py) covers:

- `131835`: square real/imag components (`12AF36`), strided sum (`12D73B`),
  clamp (`12BB49`) to float32(1e−10)..float32(1e30), then sqrt (`12EF05`).
- `1317A2`: always compute the same clamped power into supplied or fallback
  scratch storage; add
  float32(1e−20), obtain reciprocal sqrt (`131787`), magnitude via reciprocal
  (`131773`), and multiply original components by reciprocal sqrt.
  A zero bin still has zero normalized components: the positive magnitude
  floor is not itself an injected audio/noise signal.
- `12E3E6`: `(L+R)/2, (L−R)/2`; `12E44B`: `L+R, L−R` over 2*N real words.
  A half-scaled transform followed by the unscaled transform reconstructs
  unity, not +6 dB. This is a helper identity, not whole-algorithm gain proof.

Decoder opcode C5 now identifies `RSQRTS` from ADI Programming Reference
rev2.4 table12-4 / §11-43. It is a seed followed by Newton iterations, not an
exact sqrt instruction. The mathematical references use float64 sqrt/division;
SHARC seed-ROM values, flags and instruction-by-instruction rounding are not
emulated. 110 bounded ranges decode with zero UNKNOWN operations; that number
does not mean all 110 ranges have numerical implementations.

**Correction:** the previous claim that a supplied power buffer bypassed
recomputation/clamping was wrong and is withdrawn, together with its test/API.
`1317BD` tests the scratch pointer, `1317C0` selects fallback storage, but
`1317C2` resets EQ from the **bin count** before `1317C7`'s conditional jump.
The jump skips work for count zero, not for a non-null pointer. `normalize`
now accepts `power_workspace`, overwrites its old contents, and rejects the
old `supplied_power` keyword. Existing baseline reports did not use that option.

### Real transforms, window and overlap (latest continuation)

[amp_b_transform.py](../tools/soundamp-virtual-cabin/amp_b_transform.py) isolates
the following math, with independent FFT/DFT and streaming tests:

- `130A1C` constructs N/2 window values using `125C2B` at `(i+.5)*pi/N`.
  `130AEB` multiplies the first half then reverses the stored coefficients
  for the second half. It is **window application, not an FFT**. The reference
  uses ideal sine, not emulation of the trig polynomial or float32 rounding.
- `12D49F` assembles previous/current hops; `132B2B` adds the previous second
  half to the new first half, retaining the new second half. Reset helpers
  `12D546` and `132BB4` clear histories. N=1024 and hop=512 here.
- `12B588` packs even+j*odd samples into an N/2 complex FFT, then creates
  N/2+1 complex bins (513, not 512). DC/Nyquist imaginary parts are zero;
  the quarter bin is conjugated. There is no global forward normalization.
- `1330E6` real prepacking uses `0.5*2^(-LOGB(N/2)) = 1/N` for power-of-two
  N, with `2/N` on the quarter bin. It then calls the **same forward complex
  kernel `12EF65`**, not an inverse FFT. The reference preserves this order.
- Loader PM `E4754` has cos(theta), −sin(theta) pairs: all 512 pairs checked
  against theta=2*pi*k/1024, maximum complex error `9.56313e−8`.
  The audit records a hash of these 1024 float words. Reference functions
  accept explicit twiddles; their default is ideal double-precision values.

`NeutralOverlapReference` explicitly substitutes **identity spectral
processing** between the recovered analysis/synthesis steps. Two sine windows
at 50% overlap give unity because their **squares**, not amplitudes, sum to 1.
Random audio and 30/60/120/1000/10000-Hz tones reconstruct within 3e−15 with
one hop (512 samples) latency and a final zero-hop flush. Multiplying each
window by two correctly gives x4, so gain is not silently normalized away.

This rules out a bass boost in this ideal neutral reference, **not in the
live B algorithm**. The complex kernel uses numpy's unscaled FFT as an
explicit mathematical primitive, not instruction-level execution. SIMD/DAG,
kernel scaling/ordering on hardware, trig approximation and rounding remain
validation boundaries. The spatial reference is now joined to the spectral
wrappers below; integration into AmpChain with validated live state remains open.

### Spatial processor 130771 (latest continuation)

[amp_b_spatial.py](../tools/soundamp-virtual-cabin/amp_b_spatial.py) now provides
`SpatialFrameReference`: **finite successful-path math** of `130771`, composed
from statistics, controls, separator, diffuse mask and output blend. Caller
supplies boundary complex spectra, a contiguous complete band map (60..74
bands), all histories, the 28-word parameter bank, object+10/+11 balance
coefficients and explicit converted selectors. No live preset is invented.
Exceptions, invalid buffers, allocation/reset, alias side effects, exact
floating-point conversion/rounding and firmware error-path state commits are
outside this model. It is not connected to AmpChain's default B=0 hypothesis.

`130658` computes clamped per-bin power (`130609`), **sums** it over each band,
and smooths with p11/p12. `13039C`/`12ECC7` supply `L*conj(R)`; real and imaginary
components are summed and smoothed separately, with no magnitude substitution.
Output band powers clamp to float32(1e−9)..float32(1e30), but their history was
already copied **before** this clamp. Cross history is not clamped.

`13027B` divides complex cross by `sqrt(PL*PR)+2^-52`, invokes `131835` magnitude,
then clamps to [0,1]. Because that magnitude helper floors power at float32(1e−10),
zero cross produces coherence approximately1e−5, not zero. This is a control
floor, not generated audio. A 90-degree interchannel phase is coherent too;
phase cancellation within a band can reduce the summed cross magnitude.

Controls are distinct:

- `130359` calls `12A530`, a **minimum** reduction over all band coherence.
  The minimum is temporally smoothed with p13/p14, history at object+9.
- `130321` averages exactly entries9..59 inclusive (51 values), then smooths
  with the same p13/p14, history through object+8. These are **band indices**,
  not fixed FFT bins or a known Hz interval without the live band map.
  The reciprocal-51 compensation and `MANT` overflow guard are decoded.
- `130237` uses the smoothed **minimum**, not that mean: target0 if minimum
  exceeds float32(.99), target1 otherwise. Gate history object+7 uses rising
  p23/p24 or falling p25/p26. No coefficient-sum normalization is assumed.
- `1303B5` builds the band ramp with endpoints p4/p5, converted indices p6/p7,
  and **supplied step p8**. Endpoints below float32(.001) have .001 added,
  not clamped to .001. Both zero indices become1 after valid-bounds checking.

`12FFBA` separator uses a complex matrix per band. Let A/B be supplied band
powers, C complex cross, r ramp, epsilon=float32(1e−30), floor=float32(1e−6).
Start `s=p10*1e6`. If converted p1==1, compute before the power floors:

```
lambda = (A+B-sqrt((A-B)^2+4*Re(C)^2))/2
if lambda>floor: s=1/(lambda+epsilon)
A=max(A,floor); B=max(B,floor)
d=(A+B)*s-2+10*p9
q=r/d
M = q * [[A*s-1, C*s], [conj(C)*s, B*s-1]]
```

**Re(C)^2, not |C|^2** in the lambda estimate is intentional and pinned by
instruction/test evidence (`130204..130210`). There is no automatic division
by matrix row sum or extra output normalization. A zero/nonfinite denominator
is rejected by the reference, not patched with an invented epsilon.

When converted p2!=0, `t=(A-B)/(A+B+epsilon)`, `g=1/(1+abs(t))`;
`(u,v)=(g,1)` if t<0 else `(1,g)`. With
`h=clamp(object[10]*smoothed_fixed_mean+object[11],0,1)`, set
`wL=h*u²+(1-h)*u`, `wR=h*v²+(1-h)*v`. Replace the diagonals by
`1+wL*(M00-1)` / `1+wR*(M11-1)` and scale off-diagonal rows by wL/wR.
Then apply the resulting 2x2 matrix to each complex bin in its half-open band.
Object+10/+11 must not be confused with parameter-bank p10/p11.

`1304E4` derives `sqrt(1-coherence)`, smooths with p19/p20, stores that history
at object+6, and expands bands to bins. `13045E` independently frequency-smooths
the expanded mask with **low pair p17/p18** and **high pair p15/p16**, blending
with weight `(N-i)/(N+epsilon)`. The last bin still has 1/N of the low version.
Clamp to p21/p22, then multiply both original complex input lanes by this mask.

Finally `130901..130A0B` computes residual=input−separator. Only converted
p3==1 gates this residual; the `BTGL`/SZ sequence tests **equality to1**, not
arbitrary nonzero. Output is `(1-p27)*residual+p27*diffuse`; p27 and final audio
are not clamped here. p0==0 bypass copies inputs and preserves histories in
the valid-band-layout case. The mean-history store happens at successful exit;
reference exceptions are outside its claimed state-equivalence domain.

35 new numerical tests include independent covariance-matrix composition,
literal finite register-pipeline checks, complex phase/sign, floors and clamp
ordering, selector equality, asymmetric history and repeated-frame state.
They are **not hardware execution proof** or measurements of active gain.
In particular, knowing these equations does not establish the cause of the
reported bank1 bass excess without real parameters and the surrounding graph.

### Six/four-channel branches and the common output finish

[amp_b_output.py](../tools/soundamp-virtual-cabin/amp_b_output.py) adds fixed
successful-path references with **supplied time-domain boundary audio**, not
an invented spectral generator. The `131208` dispatcher enters the six/four
channel processing for selector **2 or 5**. The `BTGL … BY 1` + SZ sequence
compares the toggled result with zero (selector==2); it is not a general bit1
enable check. Other selectors clear six output lanes **before stateful EQ**,
so existing filter history can still emit samples. Mode6 copies inputs3/4 to
outputs6/7 and returns after six-channel EQ, skipping the common final eight
channel stage. Library selectors are not established MMI mode labels.

`12A7F9..12AB01`:

1. Copy a supplied stereo pair, gain at object+422, common delay through
   pointer +1159, two nested all-passes on each channel (parameter pointers
   +1167/+1168 left and +1169/+1170 right).
2. Copy this pair to three pairs **before** private pair processing.
3. Pair delay/gain/EQ banks are inline +444/+495/+546. First two pairs also
   have single all-passes at +488/+491 and +539/+542; third has none.
4. Pair gains are pointers +1171/+1172/+1173; final six-channel gain is
   pointer +1157. Dispatcher then applies another scalar bank +592.

`12EDE6..12EF05`: supplied stereo → common delay inline +331 → two copies →
separate delay/gain/EQ banks +332/+376 → common gain pointer +1156.
No single all-pass or private pair-final gain occurs here. Dispatcher applies
scalar bank +421 and adds these four outputs to the first four six-lane
outputs using ordinary addition, with **no implicit averaging or clipping**.
The separate two-channel tail is scaled at bank +676.

Final order is six-channel EQ +593 and two-channel EQ +677, then `12B87B`
applies a shared eight-channel EQ bank +698 and gain +719. `BOutputFinishReference`
implements this mode2/5 merge given explicit outputs of the three tail functions.
All bank offsets are object-relative words; pointer bindings and inline banks
must not be interchanged. Delay changes, reset/mute/error behavior and changing
coefficients remain outside these fixed references.

Synthetic routing check: equal coherent six/four inputs and unity gains yield
2× amplitude (+6.0206 dB) in the first four lanes, unity in the other four;
opposite-phase inputs cancel in the first four. This proves the addition
topology, **not** actual music correlation, runtime gains, active mode, or the
cause of the user's bass surplus. Independent scipy transfer tests cover the
active filters, nested all-pass cascade, fan-out and block state continuity.

### Adaptive band processing now reconstructed separately

[amp_b_adaptive.py](../tools/soundamp-virtual-cabin/amp_b_adaptive.py) requires
explicit parameters, band boundaries and histories. It does not substitute
zero histories or a guessed band layout into a whole-B music render.
Band boundaries come through B9097 and band count through B9098; getters
`13176A`/`131761` alone do not establish their live values.

`1316C1` sums values in half-open bands; it does **not divide by bandwidth**.
`13165D` expands one value to each bin in the band. References currently
require an ordered contiguous map covering the input; empty bands are allowed.
The code paths for missing layout pointers/errors are not modelled.

`12F90A` (`BandLevelMaskReference`, up to74 bands), parameter bank `p`:

```
energy = sum(magnitude^2) per band
power = p2*energy + p3*previous_power
fast = 10*log10(1+power)
slow = p4*fast + p5*previous_slow
excess = fast-slow-p13
target = slow+p12*excess+p13 if excess>0 else fast
raw_gain = clamp(10^((target-fast)*float32(0.1)), p14, p15)
gain = p6*raw_gain+p7*previous_gain if previous_gain<raw_gain
       else p8*raw_gain+p9*previous_gain
```

Gain bands are expanded and frequency-smoothed with p10/p11. There is no
second clamp after that smoothing in this function. Disabled mode fills
unity gains without updating histories, given a valid band-layout context.
`12F87B` loads **0x3DCCCCCD = 0.1**, unlike C4D17's **0x3D4CCCCD = 0.05**.
Do not globally change one law to the other. The returned gain is applied to
both complex components by strided `12D6A3` calls; this is not permission to
silently take its square root in the reference.

`12F103` (`BandTransientMaskReference`) always processes **74 bands**, with
an 88-word parameter bank. It sums the supplied scalar spectrum without an
extra squaring step. For each band, with old level `l` and current sum `s`:

```
level = p78*s+p80*l if l<s else p79*s+p81*l
raw_gain = ((p[2+band]*(s-level)+level)/s)^2 if level<s else 1
gain = p82*raw_gain+p83*previous_gain if previous_gain<raw_gain
       else p84*raw_gain+p85*previous_gain
```

Expand bands, apply p86/p87 frequency smoothing, then clamp to p76/p77.
Notice the first smoother's coefficient order **78/80 vs79/81**, not adjacent
pairs. Histories store pre-frequency-smoothing levels/gains. The `12F250`
wrapper chooses shared averaged L/R scalar spectra when rounded selector==1,
otherwise independent left/right histories. The wrapper is now composed below;
shared mode updates only the left histories, leaving right histories untouched.

`131948` frequency smoothing makes forward and reverse recursive passes:
`out=g*input+h*previous_output`, each initialized with its endpoint input.
It bypasses for |g−1|≤float32(1e−30), regardless of h. Do not assume g+h=1.
Tests compare both passes and endpoint initialization with independent scipy
IIRs. `12D355` instead smooths vectors **between calls**: first enabled call
copies input into history unchanged; later calls use input_gain*input +
history_gain*previous. Disabled calls preserve both history and first flag;
`12D3E0` resets history and flag. Calls are firmware update units, not arbitrary
audio chunk partitions.

`12ADFF` subtracts a frequency-dependent ramp times a supplied reference from
two scalar spectra, keeping the nonnegative part and multiplying the negative
part by bank field5 (each part clamps at ±float32(1e30)). `12AD70` ramp reference
preserves the low_bin==0 adjustment and rejects invalid bounds/equal endpoints;
the firmware's invalid-index/error fallthrough is not claimed equivalent.
This is not an audio bass shelf.

These are float64 mathematical references, not SHARC bit-exact log/pow/rounding
emulation or reconstructed allocation/reset for all mask states. No preset or
gain from them has been applied to the car.

### Composed spectral wrappers and fixed mode2 B output

[amp_b_pipeline.py](../tools/soundamp-virtual-cabin/amp_b_pipeline.py) adds
`StereoTransientReference`, `BSpectralFrameReference` and `BMode2FixedReference`.
The last accepts two **prepared library PCM inputs**, 512 samples per call,
and produces eight B lanes. It is not an iPhone-to-speakers or all-mode emulator.
Every adaptive/tail parameter and component state is explicit; absent analysis
and synthesis histories mean a documented zero-reset test state.

`12E80E` uses a 1024-point transform, 513 bins, separate synthesis histories
for three branches, and pre/post processing only for library selectors 2/5.
Main runs whenever this wrapper runs; top-level selector6 bypasses the wrapper.
Skipped pre/post branches return `None`, not invented zero outputs, and their
histories freeze. Selector values are not MMI 3D labels.

Recovered object-relative bindings (SHARC word offsets):

| Branch / component | Parameter / state binding | Data flow |
|---|---|---|
| Pre `12E1BB` | params +98, synthesis +8 | Subtract ramp times `0.5*abs(L+R)` from magnitudes; remodulate original phases |
| Main `12E30A` | transient params +211, state +12, synthesis +10 | `12F250` masks original complex spectra and supplied scalar spectra once each |
| Post `12E521` | synthesis +9 | Starts from saved **original**, not main-filtered L/R |
| Post level masks | shared params +104, separate states +13/+14 | Unscaled sum/difference, masks, half-scaled reconstruction |
| Crossfeed / spatial | coefficient pointer +1161; spatial params pointer +1166, state +15 | Symmetric crossfeed then `130771` |
| Magnitude smoothing | shared params +120, separate states +16/+17 | Only first **256 of 513** magnitudes, then phase remodulation |
| Post transient | params +123, state +18 | Unscaled sum/difference, `12F250`, half-scaled reconstruction |

`12E487` crossfeed is `((1-c)L+cR, (1-c)R+cL)` without a clamp on c.
`130B6D` multiplies both complex components by its scalar; it does not repair
signs or square the scalar. Transient history layout is levelL, levelR,
gainL, gainR, each 74 words. Selector==1 masks both channels using averaged
scalar spectra and only the left history; other selectors use independent states.

The wrapper has scratch A (4096 words) and B (4617 = 9*513 words). Before main,
it saves original L/R at A+4N/B+0 and magnitudes at A+6N/B+8N, N=513.
These saves explain why changing main's transient gain must not change post.
The reference models successful-path values with independent arrays, not literal
scratch aliasing or allocator/error behavior. The audit's
`first_b_spectral_bindings` records call sites, scratch and parameter bindings.

Mode2 tail routing, with S the five-input base and P the object+1174 scratch:

| Spectral branch | PCM destinations | Tail | B contribution |
|---|---|---|---|
| Pre | P+0, S+1536 | four-channel `12EDE6` | Added into lanes0..3 |
| Post | S+2048, P+512 | six-channel `12A7F9` | Lanes0..5 |
| Main | P+1024, P+1536 | two-channel `132F86` | Lanes6..7 |

Execution order is main/two-tail, post/six-tail, pre/four-tail, then finish.
Independent branch-gain tests (pre=1, post=.5, main=.25, neutral tails) yield
`[1.5,1.5,1.5,1.5,.5,.5,.25,.25]`, proving routing and unnormalized addition
in this synthetic case, **not a measured live bass boost**. Other tests cover
coherent versus antiphase subtraction, shared-history isolation, scalar applied
once, 256-bin smoothing, branch pause/resume, streaming overlap, 60-Hz gain
separation and a common 17-sample tail delay plus 512-sample transform latency.

Mode5's additional input preparation now has its own fixed reference, below.
Ready/transition handling, other dispatcher modes, runtime updates and hardware
numerical equivalence remain outside these APIs. Neither is silently connected
to AmpChain's B input.

### Fixed mode5 and the Bank1 control link

`BMode5FixedReference` accepts three wrapper PCM lanes,512 samples each,
and explicitly supplied `input_mix_gain`. It uses the same spectral/tail/finish
components as mode2 but invokes the spectral wrapper with mode5 after:

```text
prepared_left  = S0 + g*S2
prepared_right = S1 + g*S2
```

There is no half scale, dB conversion or limiter in this preparation. `g` is
the effective linear value at library object+97. Firmware `132D0B` invokes
`12FAF2` once before applying the scalar through `12FCE1`, but the Sep20 audit
below establishes that normal initialization makes this update return without
changing object+97. The reference still requires the effective value explicitly:
parameter delivery, initialization order and later writes are not live-verified.

Address proof (`S`=five-input buffer base, `P`=object+1174 workspace pointer):

| Site | Operation |
|---|---|
| `1315D3..1315DB` | copy S+1024 (third lane) to P+512 |
| `1315E5..1315F3` | gain address object+0x61, one channel,512 frames |
| `131606` | add P+512 and S+0, write S+2048 |
| `13161B` | add P+512 and S+512, write P+512 in-place |
| `131622..131629` | redirect spectral input pointers to S+2048 and P+512 |

The parallel store at1315D3 saves the old R12=P+512 before1315D6 replaces R12
with the frame count. This distinction is pinned by extraction tests.
Wrapper input ports are graph24/23/22; upstream interleave/processing/deinterleave
nodes precede them. **Do not substitute raw MOST L/R/C for these boundary lanes
without modelling the upstream graph.** In particular, authored C=0 is not
proof that graph22 is zero. Its full upstream numerical composition remains open.

The earlier activation note was overcautious/incorrect in saying the V850 index
was not the matrix-bank index. `C2852` reads **the same gp−15760 byte**, shifts
by6 and selects the64-byte matrix bank. `C2886` queues event48 (library control)
and `C2896` queues15 (matrix writer). Event48's `AA388` sends API16 from table
6EAA0: **matrix bank0→library2, bank1→library5, bank2→library5**. These numeric
enums differ, but their writer establishes a direct relationship.

Sink index is SinkNr−0x21 (`D0036/D003C`, also `D953E/D9542`). The record for
sink0x26 at95314+4*5 has raw bank1; `C2806..C283C` leaves bank1 unchanged for
every ASAD byte. Thus the normal bank-refresh path for sink0x26 **requests
mode5**, not mode2. This is conditional on taking that refresh, delivering and
applying API16 (object+1178==0), and not superseding it with another command.
It does not prove current playback mode or that the graph/callback runs.
Library initialization to2 can precede the later bank1 request to5; both facts
are consistent. MMI3D level remains a separate control.

Tests cover shared-sum/difference identities, extra-only impulses through all
three output branches, zero-extra equivalence to mode2, non-clipping stream
composition, and rejection of invalid input before state changes. These use
synthetic explicit coefficients, not a claimed factory preset or measured bass
boost. No default AmpChain model or production gain changed.

### Activation audit: B=0 is not established playback state

The default report's **B=0 remains a hypothesis**, not a measurement or a
deduction from Bank1/5.1. New `first_b_activation` audit records three distinct
controls that must not be conflated:

1. **Graph handler:** class C53E1 contains process `124DF3` and alternate
   handler `124D64`. The scheduler loads the active handler from descriptor+4
   (`C4ADD`) at `1267C9`. The alternate handler copies input lanes0/1 into four
   output pairs (`124D73..124DD6`): **bypass is not output mute**. With C4AD9's
   ports this means graph24/23 copied to B outputs20/19,18/17,34/35,40/41.
   Command dispatch is now resolved below; the active playback handler still
   requires readback, including the graph-tracing saved-handler exception.
2. **Callback inhibit — correction of the previous audit:** in callback state
   `B95E2==3`, `124F73` reads `BC9EC`; BTGL-by0 produces zero exactly when the
   original word equals1. `124F7C IF SZ` jumps to **output clearing**, not the
   library call. Thus **BC9EC==1 clears; BC9EC!=1 calls `12DE3B`**. The previous
   statement of the opposite polarity is withdrawn. `124FBB..124FC6` supplies
   R8=0 and count4096 to word-fill `1257D0`, whose store is `1257DD`.
   Loader BC9EC=0 therefore permits the library call once callback state reaches3;
   loader callback state1 is initialization, not playback. This is not proof of
   successful/nonzero processing: ready/transition/error paths still apply.
   No direct writer of BC9EC was recovered; absence of a literal store does not
   exclude indirect/host writes. The host parser itself has generic word-write
   records (`1296B9..1296E2`). Callback clears concern the buffered process path,
   not the alternate graph handler or instantaneous stale-buffer timing.
3. **Library algorithm:** object+1128 is a float selector, independent of both
   controls above. Init `130F90` supplies object+59 to defaults; `12B26D` writes
   2.0 at relative+1069, thus object+1128. API parameter16 queues at B911E and
   `132468` applies it at object+1149−2−19, when object+1178 is zero. V850
   `AA388` cases45/48 send parameter16 through `AC648` to C4AD9+8/+9, then
   wrapper `124F28..124F35` forwards it to `131A65`. Table6EAA0 maps control
   indices0/1/2 to **2.0/5.0/5.0**, not zero. Index `gp-3D90` starts at0
   (`B6B86`) and has source/config writers `C2802/C2822/C283C` plus command
   writer `D4C2E`. This byte IS also the matrix-bank index at `C2852`; the library
   selector is a different numeric enum mapped from it (see mode5 section).

#### Graph commands and their V850 queue origin

SHARC `129533` parses host records. `1295F4` extracts bits16..30 of the header;
kind2 reaches `12969F`, extracts the low16 selector at `1296A5` and calls
`1292FD` with the target descriptor. For C4AD9 the class flags are zero, so the
flags>2 override at `12931B..129326` does not apply.

| Command | Setter path | C4AD9 effect |
|---|---|---|
| `0x20001` | selector1 → `129365..129374`, class+1 | main process `124DF3` |
| `0x20003` | selector3 → `129345..129351`, class+2 | copy bypass `124D64`, NOT mute |

`1292D8` normally returns descriptor+4 as the write location. When the parent
graph object's +31 word names this traced node, it instead returns graph
object+32 (saved handler behind the tracing wrapper). A readback must account
for this exception; C4ADD alone can otherwise expose a wrapper rather than the
selected process/copy handler.

V850 `AC818` sends20003 when its event argument equals5, otherwise20001.
Dispatcher `A76B0` routes events5 and44 to it (`A79C2/A7C0E` respectively).
These are **internal event IDs, not Bank or MMI3D values**.

The queue is not populated only through `C3990`: bulk initializer `C28A8`
writes it directly at gp−16772, resets head gp−16776 and sets tail gp−16774.
With argument1 its three ascending loops enqueue0..29,30..32,33..53.
Consequently **event5 selects bypass first and event44 later selects process**
if this sequence is delivered successfully and no later command overrides it.
The direct caller `C2658` reaches this initializer only after `C264A` tests
r29==1. Argument8 would enqueue only0..29; its live use is not established.
Queue consumer `A74EC` reads events in ascending queue order and calls `A76B0`.
This closes a static producer→consumer→SHARC setter path, not live completion,
source/Bank association, or all subsequent writes.

Reproduce with `first_b_activation_writers.asm`, bounded listings
`graph_handler_slot`, `graph_handler_select`, `host_command_records`, `word_fill`,
and regression tests in `test_audit_amp_remaining_blocks.py`. The gate test
follows the decoded branch for values0/1/2/3/FFFFFFFF and verifies the reached
call, rather than merely testing the prose formula. Register BSET/BCLR decoding
uses ADI programming rev2.4 register-shifter definitions (local primary manual).
Current tests:69 extraction/decoder +176 model = **245 passed**;110 bounded
ranges without UNKNOWNs. These are not hardware execution-equivalence tests.

**Next discriminating check:** trace the caller's configuration phase/source
and later handler changes, or read active C4ADD (plus tracing indirection),
B95E2, BC9EC, B89BA, B89B9 and object+1128 during playback. Capture B audio with
nonzero input and the actual mixer row. Only then choose silence, bypass copy
or algorithmic B as the vehicle scenario. The static default path is compatible
with active B; retaining B=0 in the baseline is only an explicit hypothesis,
not a validated Bank1/5.1 configuration. No car or production changes made.

## Model use and remaining work

`AmpChain(runtime=...)` accepts an **explicit scenario** with
`pre_complex_gain` (10), `post_complex_gain` (10) and `limiters` (two
`PeakLimiter` parameter dictionaries, 16-channel config order). No live values
are guessed. Each `process` call resets limiter/filter state. `response()`
rejects nonlinear scenarios; use audio rendering. Default C4D17 remains the
explicit identity hypothesis, preserving the earlier numeric baseline.

Opt in using `AmpChain(complex_block='full_mode_reference', complex_scenario=...)`.
The scenario contains all `ComplexFullModeReference` constructor arguments;
`profile` must match the main audit's profile. Render with
`process(x6, complex_auxiliary=mono_samples)`. Missing auxiliary is rejected,
not replaced with zero. Pre/post runtime gains bracket C4D17 correctly;
metadata includes parameter/FIR hashes and the extra conditional 1023-sample
A-path delay. Each process call resets state; last partial block is padded
and output trimmed. `response()` rejects this time-varying scenario.
The existing music-report CLI still selects the identity baseline.

Next: independently validate the composed reference against actual SHARC
execution (especially prefix selection and hardware rounding); compose the
calibration noise-shaping/audio path if needed; recover graph11 physical source
and runtime scaling; validate `130771` against execution (including actual
buffer aliases and error/lifecycle state), validate the complex FFT primitive,
validate the fixed-mode2/5 composition; recover mode5 object+97 parameter source/
delivery/init order (its source and normal interpolation bypass are now recovered
below) and complete upstream graph24/23/22 input preparation;
cover other dispatcher/lifecycle paths; obtain read-only playback state (including actual
library selector, parameter objects, B9097 band map, B9098 count and histories).
Production hooks, EQ defaults,
LFE levels and vehicle state were not changed by this work.

Validation after the Sep20 follow-up: 74 image-extraction/decoder tests and 183
virtual-cabin/model/report tests pass;121 bounded ranges decode with UNKNOWN0.
The earlier independent default-path composition baseline was `7.30e-15`;
this follow-up does not claim a new measurement of that baseline.
These tests verify the stated local reconstruction, not the real amplifier.

```sh
python3 tools/audit_amp_remaining_blocks.py IMAGE \
  assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin \
  --output-dir assets/soundamp/remaining-blocks-20260919
python3 -m unittest discover -s tests -p 'test_*amp*.py'
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p 'test_amp*.py'
python3 tools/soundamp-virtual-cabin/audit_amp_complex_scenarios.py IMAGE \
  --output assets/soundamp/remaining-blocks-20260919/complex_scenarios_profile0.json
```

## Sep20: mode5 gain provenance and pre-B EQ

The new `first_b_input_config` audit record pins source-copy instructions,
interpolation bounds, upstream descriptors and coefficient orders. Both input
images retain the hash checks used by the existing auditor. This is a static
initialization scenario, not proof of the current vehicle's configuration.

### Object+97 is B01 word43, approximately −6 dB

`B95E5` contains `C3C6A`. Callback124FD9 passes that pointer as init argument5;
12DDEF/12DDF1 forwards it to130EF3. The object constructor at130F90/130F93
calls12B4D0 with `R4=object+59`, `R8=source`. Its12B4E6/12B4E8 call passes
source+38 to12B2BF, which advances the destination33 words and copies
103 SIMD pairs plus one scalar: **source38..244 → object92..298**.
Therefore object97 corresponds to source43, not source97.

| Source | Location | Word | Linear g |
|---|---|---|---:|
| SHARC loader | DM C3C95 | 3F000000 | 0.5 |
| V850 B01 payload | CodeFlash1200B8 | 3F004DCE | 0.5011872053146362 |

`ACFB4..AD0A6` uploads B01 only if gp−455C==A5 and 0<count<=8000. This dump
has header `AD301B01` and3562 words. AD038 sets destination C3C6A+word index;
AD04A/AD056..AD060 reads12000C+4*index into the upload packet, in80-word chunks.
Thus B01 can override the loader value before object construction; delivery,
init order and subsequent writes remain conditions, not established live facts.

`130F33/35` prepares object+299; `1310B7/1310BF` passes it to12FC3C, which
stores it to B903A/B903B at12FC60/63. These stores are in branch delay slots
and run even for zero duration. When132D0B calls12FAF2 with object+97,
12FAFC compares unsigned base against the requested address; base>address
jumps12FB36 **before target lookup or coefficient write**. Normal initialization
therefore does not interpolate this g. This does not rule out later direct
writes or alternate initialization. The model keeps g explicit.

About−6dB describes only the shared third-input coefficient. It does not mean
the whole B branch attenuates by6dB: coherent S0 and S2 still add. For the
synthetic S0=S2 case, preparation alone gives1+g, approximately+3.53dB.
That arithmetic is not a diagnosis or measurement of excess bass in the car.

### Upstream topology and the new fixed EQ kernel

Corrected graph order (ports are graph IDs, not physical MOST labels):

```text
C4A54 transfer/delay state machine: 0..7 → 34..41
C4A6C shared planar biquad cascade: 34..41 → 24,23,22,21,20,19,18,17
C4A7B per-channel gain: those eight ports → 34..41
C4A92/97/9C/AA1 pack pairs: (34,35)..(40,41) → 42..45
C4AA6 shared five-section biquad cascade: 42..45 → 46..49
C4AB7 independent banked three-section EQ: 46..49 → 42..45
C4ACF unpack42 → 24,23; C4AD4 unpack43 → 22,21
C4AD9 B inputs: 24,23,22,97,97
... C4AED A07 matrix is downstream of B
```

The previous description of graph22 as a post-matrix input was incorrect.
C4A54 has a transfer/delay state machine with indirect dispatch and early
clear paths; its complete source/active-state binding is not yet modelled here.
The filter topology alone must not be promoted to a physical C→graph22 proof.

C4AA6 calls1262DA once for each of five coefficient records, then126685→12610B.
Current coefficients live at descriptor+10 (loader E004E), targets at+15
(E0068). Format is `[-a1,-a2,b0,b1,b2]`, shared across all four packed stereo
pairs with separate histories. Smoothing includes a feedback-discriminant
check, stability cap and epsilon snap, not merely a linear blend.

C4AB7 reads eight selectors at descriptor+15 (E0082), clamps each independently
to0..15, selects gains from C015A and12-word banks from C016A, and interleaves
left/right records.1263AB smooths feedback/feedforward records, while gains
are smoothed separately; effective packed records are at E00D4.12618C applies
each channel's gain once, then its three sections. **Its four-word record is
[-a2,-a1,b2,b1], with implicit b0=1**, different from12610B.

`BInputBankEQReference` in `amp_b_input_eq.py` implements that fixed effective
12618C audio kernel. For each section, with positive feedback values f1/f2:

```text
y = x + z1
z1_next = z2 + b1*x + f1*y
z2_next = b2*x + f2*y
```

Each SIMD lane has its own coefficient and state words, stride2; there is no
cross-lane audio sum or swap in12618C. The shared12610B kernel likewise keeps
audio/state lanes separate. The reference accepts explicit coefficients,
gains and optional initial histories; it does not infer live selectors or
emulate12682B's parameter-update lifecycle. Tests compare8 channels ×3 sections
against independent SciPy SOS with nonzero histories, check every isolated
channel, pin record order/gain-once, streaming and invalid-input nonmutation.

This milestone's follow-up is now implemented in
[the Sep20 input-path audit](SHARC_INPUT_PATH_2026-09-20.md): calibrated writer
sources, selector rules, C4A54 state machine/TCBs and an explicit settled
upstream composition. The16-bank input EQ is shared by A and B, with55Hz
boost up to20.5dB independently of tone Bass. No live bank was read.
Latest validation:277 tests,127 bounded ranges UNKNOWN0. Physical input binding,
active control/readback and lifecycle/hardware equivalence remain open.
No default AmpChain, production hook, EQ/LFE setting or vehicle state changed.
