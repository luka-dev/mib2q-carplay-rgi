# C4D17 calibration excitation and conditional timing — 2026-09-20

Static/offline only. No vehicle connection, deployment, firmware edits, playback
defaults, or changes to `AmpChain`.

## Newly closed reference gap

`amp_complex_modes.CalibrationNoiseCore` already reconstructed the modular
PRNG/shuffle, but explicitly omitted the generator's optional filter stages.
The separate [reference](../tools/soundamp-virtual-cabin/amp_complex_timing.py)
now composes the two scalar biquads and mode2 excitation gating. This completes
**the calibration excitation subpath**, not the whole calibration/LMS processor.

Evidence from the SHA-pinned ADSP-214xx VISA image:

| Operation | Instruction evidence | Reference behavior |
|---|---|---|
| Generator type1 | `126D5A` branch, delayed loads `126D5D/60` | Select records `B94F4/B94F9` |
| Generator type9 | `126D66` branch, delayed loads `126D69/6B` | Select explicit descriptor+14/+15 records |
| Other types | `126D6D` jumps to return path | Raw noise, no shaping |
| Two filters | Calls `126D7E`, `126D8F` to `126092` | Two **cascaded**, not parallel, sections |
| Excitation gate/gain | `12719D..1271A9` | Same generated wave × descriptor+30 × each channel gate |

The scalar biquad `126092..12610B` reads coefficient words as
`[-a1,-a2,b0,b1,b2]`. Its independent scalar reference is:

```text
y = b0*x + z1
z1' = b1*x + (-a1)*y + z2
z2' = b2*x + (-a2)*y
```

Generator type, PRNG seed/shuffle/previous value, ALUSAT choice, both filter
histories, excitation gain, and calibration sequence state must be explicit.
No host random-number generator, default microphone signal, live mode, or
implicit zero history is invented. Type1 constants are available through a
loader-hash-checked reader. Type9 records must be supplied by the caller.

The new excitation wrapper uses the already recovered sequence. Configure and
wait blocks clear music outputs and do not advance the generator. Measurement
blocks generate immediately. The completion fall-through produces one last
block using the previous gates; the following call clears outputs. A zero
excitation gain still advances the generator in a generation block.

## Timing distinctions that matter for model use

| Condition | What is established | What must not be inferred |
|---|---|---|
| C4A54 settled state | Uniform integer delay0..9600; audited target request subtracts2967 samples and clamps | Startup/DMA transition behavior is not the settled renderer; request≠current delay |
| C4D17 master disabled / first binding call | No output-buffer write | Not automatically silence or bypass |
| C4D17 mode0 after binding | Clears outputs | Not identity |
| C4D17 mode1 after binding | Copies inputs | No full-mode1023 delay |
| C4D17 mode2 calibration | Mask excludes stage8; excitation has no1023-sample stage8 delay | This is not a music listening scenario |
| C4D17 full-mode, either sideband | Stage8 gives1023 samples =21.3125ms at48kHz | Sideband skipping band gains does not remove delay |
| Full-mode detector/control |64-frame audio calls;4 detector-prefix frames/call;16-call target window =1024 audio samples |1024 is update cadence, **not**1024-sample audio delay |

Known integer scheduling delays are not complete acoustic or frequency-dependent
filter group delays. Existing `AmpChain` identity scenarios omit the conditional
C4D17 delay intentionally and still require explicit upstream-delay scenarios.

## Provenance and validation

Original CodeFlash SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Derived bit-reversed loader SHA256:
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
SHARC program addresses above are shortword addresses; coefficient addresses
are32-bit words. Builtin record offsets in the **derived loader** are
`0x7618` and `0x762C`; these are not original CodeFlash offsets.

[Reproducible report](../assets/soundamp/complex-timing-closure-20260920/report.json)
records hashes, constants, addresses, file offsets, and96+65+28 decoded
instructions with no UNKNOWN. The added ten tests cover independent scalar
recurrences with nonzero history, streaming partition invariance, raw/type1/type9
selection, loader rejection, lane gating, zero/negative gain, sequence timing,
the distinct1023/1024 quantities and second-B schedule binding.
All50 `test_amp_complex*.py` tests passed.

```sh
python3 tools/soundamp-virtual-cabin/amp_complex_timing.py \
  --output assets/soundamp/complex-timing-closure-20260920/report.json
python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p 'test_amp_complex*.py'
```

## Boundaries not closed by this work

- Calibration's auxiliary filtering, scratch/reference routing and LMS learning
  are not combined with this new excitation wrapper. Existing full-mode LMS
  tests are not a substitute for that calibration composition.
- PRNG FIX/FLOAT corner rounding, SIMD/extended precision and instruction-level
  numerical identity remain unproven. The new filters are mathematical float64.
- No active profile, current mode, FIR history, microphone identity/level,
  first/second B live contribution, MOST lane mapping, or cabin acoustics is
  established by these static additions.
- There is no justification to apply the calibration generator to normal music
  tuning. Its closure improves reconstruction completeness, not bass diagnosis.

Prior evidence: [remaining blocks](SHARC_REMAINING_BLOCKS_2026-09-19.md),
[C4A54/input path](SHARC_INPUT_PATH_2026-09-20.md).

## Second pair-mixer B: bounded source-binding check

The immediate producer is **schedule50 descriptorC4CF9**, a16-lane delay using
handler124517, fed by **schedule49 C4CE8**, the packed post-EQ handler126A95.
Its16 output graph IDs, in order, are:

```text
27 26 25 24 23 22 21 36 37 50 51 52 53 54 55 73
```

They match **exactly** schedule55 C4DFB input slots16..31, the B half of its
32-input/16-output pair mixer. Nodes51..54 do not overwrite these audio lanes.
The stored nonzero B coefficient slots are1,3,5,7,12,13; other stored slots are0.
These are mixer-slot numbers before the subsequent normal route, not physical
speaker numbers and not a readback of present gain settings.

The new schedule-ordered dependency regression starts at schedule21 C4B70
(`126559`), which reads graph8/9/10. It follows input filtering, gain fanouts,
three-input per-output matrices, post-EQ and delay through schedule50. All16
second-B output lanes have possible structural ancestors8/9/10, not0..7.
Matrix edges conservatively include all declared inputs even if a particular
current coefficient might be zero. Reused graph IDs24/23/22 after schedule21
must not be joined to earlier music writers with the same IDs.

This source binding was already described at a coarse level in the previous
audit; the addition is a reproducible ordered-lane/dependency regression. It
**does not close numerical second-B synthesis**: the actual auxiliary PCM,
physical meanings of8/9/10, per-stage target/current gains and EQ/matrix state,
delay values/history, and control/initialization timing are not supplied.
There is no statically justified shortcut connecting `ten` or `split16` music
buffers to this B half. The prior artificial B-donor sweep remains a deliberately
synthetic injection sensitivity check, not an actual second-B reconstruction.
