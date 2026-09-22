# Virtual cabin: static readiness and the next upmix

2026-09-08. Research only. No production audio code, trained model, cabin JSON,
amplifier firmware or vehicle settings changed.

## Outcome

The next upmix can be designed now, but the current virtual cabin is not an
exact digital/acoustic twin. This pass closes the **HU 3D enum conversion**,
expands instruction coverage of the complex SHARC block, identifies its
separate runtime controls, and reproduces a Sound Check headroom failure.
It does not claim that all remaining static analysis has been exhausted.

## 1. MMI 3D OFF: two numbering domains, now verified

The user-supplied Java tree is:
`/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf`.

- `org/dsi/ifc/audio/Constants.java:280`: OFF=1, LOW=2, MID=4, HIGH=8.
- `de/audi/tone/app/sound/ThreeDMode.java:13`: UI indices 0/1/2/3 map to
  DSI values 1/2/4/8. The choice listener actually calls `setThreeDMode`.
- `de/audi/tone/app/dsi/DSISoundHandler.java:346`: forwards the mode on the
  active entertainment connection, excluding basic/standard sound variants.
- `de/esolutions/fw/comm/dsi/audio/impl/DSISoundProxy.java:939`: serializes
  connection, terminal, mode unchanged, remote method 139.

The matching MU1316 native MIBRoot is ARM LE32 ELF, LOAD0 VA=file offset+0x100000.
SHA-256: `e6e710d7467be935c3fea933ef9068a66786c392f3f7a0e78155ad9a1063acd1`.
Its named `requestSet3DSound` handler at VA `0x1EB0FC` calls `0x1E2CD0`:
look up the mode in table `0xDDEE50` = [0,1,2,4,8,16,32,64,128].
At `0x1EB1F0`, subtract one from that index and dispatch SoundSettings `0x22`.
`0x892E68` stores the setting ID at event+0x58 and the signed 16-bit value
at event+0x5C before sending the request event.

| MMI label | UI index | DSI mode | HU SoundSettings 0x22 value |
|---|---:|---:|---:|
| OFF | 0 | 1 | 0 |
| LOW | 1 | 2 | 1 |
| MID | 2 | 4 | 2 |
| HIGH | 3 | 8 | 3 |

This removes the guess about the **HU-side** OFF value. It agrees with J525's
separately recovered field-4 range 0..3. The intervening RCC/MOST serializer
to that exact J525 field has **not** been traced end-to-end here; neither has
the user's current state been read back. Therefore:

- OFF -> HU level 0 is verified statically.
- HU level 0 -> J525 field 4 remains a strongly supported cross-component link.
- J525 level 0 + effective gate + valid B01 -> gain row 1 is already verified.
- OFF does not prove that either gate is off, or that all spatial processing
  and the auxiliary mix disappear. Row 0 is a different state.

Evidence: [native listings](../../assets/soundamp/mmi_3d_enum_evidence.md),
[source-backed regression](../../tests/test_amp_control_contract.py),
[J525 state audit](SHARC_VISA_3D_OFF_AUDIT.md).

## 2. Complex SHARC block: more than a fixed matrix

Descriptor `0xC4D17`, class `0xBDFD0`, process `0x126DB2..0x127852`:
1133 instructions now decode with no UNKNOWN, and all internal direct
branch/loop targets land on instruction boundaries. Ten directly called
helper ranges also have complete instruction decoding. These are coverage
checks, **not** a sample-accurate emulator or proof of every high-level role.

Verified control/dataflow:

- descriptor+40 bit 0 gates the whole entry. Zero returns without copying or
  clearing outputs. The loader initializes it to zero, but V850 `0xAA89A`
  explicitly sends `1` to `0xC4D3F`; loader defaults are not playback state.
- descriptor+5 bit 0 is a one-time buffer-pointer setup flag. Initialization
  builds nested buffer vectors, sets the flag, and returns for that invocation.
- descriptor+39 chooses the processing-stage bitmask. V850 `0xAA9B6` writes
  `4` when gp-0x3D62 is A5/5A, otherwise `2`. This is not the MMI 3D level.
- with control bit 2 set, port-vector entry 0 (graph ID 97) bit 0 chooses
  `0x0FFF7FE5` versus `0x0DFF75E5`. Only stage bits **9, 11, 25** differ.
  This scalar sideband must not be treated as a thirtieth audio channel.
- control=0 gives mask 2; bit 0 selects mask 1; bit 1 selects 0xF4BA;
  bit 2 takes precedence. Stage 0 copies ten ports; stage 1 clears ten ports.
- `0x127B3B` interleaves two streams and `0x127A80` deinterleaves them.
  They use register swaps and stores, not sum/difference matrix arithmetic.
- `0x127852` contains five outer paths, four repeated two-pass recursive
  filter operations, and interpolated gains. Five paths are not five speakers.
- `0x1274B4..0x1275C4` accumulates squared samples into a five-group history,
  selects sum or maximum history values, scales them, and feeds nonlinear
  gain calculations. `0x12777B..0x127824` clamps gain changes relative to
  previous state and derives increments using float 1/1024.
- V850 `0xAA912` writes descriptor+15 from the conjunction of gp-0x3D62=A5
  and calibration flag gp-0x1250C=A5. `0xAAFC0` writes descriptor+16 from a
  profile byte divided by ten, and descriptor+27=1.0. These are separate from
  the level/enable state controlling the previously recovered pair mixer.

Consequences: a stateful, signal-dependent block cannot be represented by one
constant gain row or by `symphoria_3d_enabled=false`. Conversely, do not label
this entire block "Symphoria" from its complexity alone: the branded identity
and full normal/calibration semantics are still unproven.

Residual static work: complete numerical interpretation of the recursive and
nonlinear helpers, their coefficient/state initialization, and all writers
of the sideband. Helper `0x126CEC` retains ten UNKNOWN operations; its call is
behind stage bit 4, present in mask 0xF4BA and absent in both full masks above.
This isolates that remaining decoding gap, but does not validate the other
paths' numerical behavior by exclusion.

Evidence: [main listing](../../assets/soundamp/complex_visa_evidence.md),
[helper listings](../../assets/soundamp/complex_helpers_evidence.md),
[V850 writers](../../assets/soundamp/complex_v850_evidence.md).
The decoder extension follows ADI rev 2.4 tables 12-3/12-4/12-11; the selected
PDF instruction tables were visually checked during research.

## 3. What Apple Sound Check actually contributes

Apple publicly describes Sound Check as matching loudness between songs,
including stereo versus Dolby Atmos differences; that page does not specify
a universal receiver target of -16 or the private receiver tuple calculation.
Source: [Apple Support](https://support.apple.com/en-ie/109331).

Our local evidence is more specific, but applies to the captured protocol:
SETUP contains `soundCheckMediaKind {"1":-16}`; APAP type 103 carries
MainLoudnessParameters (TLV 11) plus media kind (TLV 12). The existing receiver
follows the recovered Pioneer calculation. Do not hardcode -16 as an Apple
rule or describe these fields as proved LUFS/dBTP measurements.

The current `soundcheck.c` computes desired=SETUP target-p0; positive gain is
limited by -1-p2, rounded with floor(x+0.5), then bounded to -128..+20 dB.
Observed AAC tuple [-6.5,-1,0.96875] gives -9 dB; the retained APAC tuples give
+1 dB. First usable metadata is applied immediately; subsequent gain changes
ramp over 20 ms. FLUSH resets metadata history without forcing unity gain.
Legacy TLV 7 remains uninterpreted. RMS fallback and sample-peak guard are
separate opt-in policies, not Apple's metadata or a true-peak limiter.

Current shipping order in `audio_hook.c:3251..3291`:

```text
decoded S16 -> upmix/reorder to S16 -> Sound Check gain to S16
            -> optional linked sample-peak guard -> PCM ring
```

The gain belongs after analysis/synthesis for the existing model: HSSR uses
absolute log-energy features. Moving -9 dB before analysis changes its routing
decisions; earlier project A/B evidence is retained in Memora 997. Applying the
same gain once to all rendered channels preserves the intended spatial balance
until clipping or limiting occurs. Native APAC 5.1 bypasses stereo synthesis.

### Reproduced headroom failure

[The diagnostic](../../tests/soundcheck_headroom_diagnostic.c) links the current
production Sound Check source, uses a captured +1 dB metadata tuple and a
synthetic stereo frame [30000,31000]:

```text
after +1 dB S16:          32767, 32767
after current limiter:   29204, 29204   ratio 1.00000000
float linked reference:  28261.568, 29203.619   ratio 0.96774197
```

This proves that a limiter after S16 gain clipping cannot restore channel
ratios. It does not show that a particular captured song clipped. The existing
tuple's source-peak estimate does not describe the synthetic frame: this is a
boundary test of the processing order, not a metadata/PCM-consistent recording.
With accurate metadata and sufficient rendered headroom, +1 dB need not clip.
The existing upmixer also has S16 clamps before Sound Check; later attenuation cannot recover
peaks already lost there. A float limiter reference demonstrates ordering only;
it is not the proposed final true-peak algorithm.

## 4. Contract for the next upmix

1. **Separate inference and rendering.** Preserve source-analysis semantics
   initially. Render synthesis, rear shelf, bass management and common programme
   gain in float; quantize only once at the PCM boundary. Keep explicit exact
   bypass tests. Do not feed Sound Check gain into V11 analysis as a shortcut.
2. **One Sound Check application.** Use negotiated target and applicable media
   kind; retain metadata/ramp/FLUSH behavior. Apply linked gain to all six buses
   after synthesis, before final headroom protection. Test stale/missing metadata
   and AAC/APAC transitions; do not add an independent loudness normalizer by
   default. Bound CPU/latency before choosing oversampling/lookahead protection.
3. **Use verified electronics as the evaluation domain.** Add recovered ordered
   selectors, pair mixers, packed EQ and state selection to an offline float
   evaluator. Keep unknown complex-block behavior explicitly separate. Do not
   reverse arbitrary OEM EQ or train an inverse of an unvalidated cabin model.
4. **Respect 19 physical drivers / 16 electrical circuits.** Each rear door has
   a bass and treble driver sharing one amplifier circuit. They are not two
   independently steerable upmix outputs, nor two equal full-band copies. Passive
   response, acoustic summation and driver directivity remain unknown.
5. **Improve training targets, not only rear level.** `train_hssr.py:343` derives
   classes/rearability/pan from stems; `:719` optimizes class, rear, pan, global
   and temporal losses. It has no cabin transfer in the training loss. Keep that
   semantic objective, add verified digital-path metrics and level-robustness
   tests/augmentation. Do not train against fictitious seat responses.

Acceptance matrix for implementation: mono vocals, centered bass/kick, hard
pans, decorrelated ambience, transients, silence, authentic stereo music, native
5.1, codec/track/FLUSH transitions; Sound Check -9/0/+1 dB plus legal gain bounds
and ramps. Measure pre-quantization peaks, sample/true peak as implemented,
limiter activity, front-anchor stability, rear vocal leakage, LFE phase behavior,
channel balance, exact bypass and QNX real-time cost. Keep held-out tracks/stems
separate from training. Loudness-match listening comparisons after rendering;
never use increased loudness alone as the quality score.

## Remaining boundary and next step

Static work can still close RCC/MOST field translation and more complex DSP
semantics. It cannot recover actual seat impulse responses, driver directivity,
passive crossover response or every physical output label from code alone.
Current circuit fan-out is documentary and must stay labelled as inferred.

Recommended next implementation slice: float render/gain/headroom interface and
offline state-explicit digital evaluator, with the diagnostic converted to a
no-clipping acceptance test. Only then train/compare a new model. No production
replacement or training run was started in this audit.

Validation: 26 amplifier/control/decoder tests and 6 existing cabin tests pass;
`scripts/run_tests.sh` passes, including the available APAC reference-capture
integration test. The standalone headroom diagnostic reproduces the limitation.
