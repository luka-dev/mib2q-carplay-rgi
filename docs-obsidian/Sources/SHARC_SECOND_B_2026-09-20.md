# Second mixer B: numerical auxiliary-path reference, 2026-09-20

Static/offline only. No car access, deployment, firmware writes, preset/default
changes, invented calibration or live-state assertion.

The earlier ordered-ancestry check is now complemented by
`tools/soundamp-virtual-cabin/amp_second_b.py`: an explicit-state numerical
composition of **schedule21..50**, from three caller-supplied graph inputs
**8/9/10** to the sixteen B inputs of **C4DFB**. It never uses musical `ten` or
`split16` as a donor. Their physical meaning and actual activity are unproven.

## Source identity and reproducibility

- V850 raw CodeFlash: little-endian, 32-bit decoder, file/load base0,
  0x180000 bytes; SHA-256
  `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
- Bit-reversed SHARC loader SHA-256
  `9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
  SHARC PCs are 16-bit short-word addresses, not V850 byte offsets.
- Inventory: `assets/soundamp/second-b-20260920/inventory.json`.
  Extraction rejects other image hashes, checks instruction/writer pins, and
  compares every descriptor, class handler, scheduled pointer and port against
  the actual loader rather than trusting derived JSON alone.

```sh
python3 tools/soundamp-virtual-cabin/amp_second_b.py \
  --output assets/soundamp/second-b-20260920/inventory.json

python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p test_amp_second_b.py -v
```

The read-only r2 checks used `-a v850 -b 32 -e scr.color=false`, `iI` and bounded
`pD` ranges at A928A, A9A2C, A952E, AC59E, AB6A6. Pattern searches for literal
E067C and C073A identified the auxiliary count writer and side-branch matrix
writer; these were then checked as instructions, not accepted from hits alone.
SHARC handler listings use the existing `inspect_amp_visa` decoder over the
hash-pinned loader. No patch was made to either image.

## Recovered path

| Schedule | Descriptor(s) | Implemented math / required state |
|---|---|---|
|21|C4B70 /126559|One shared biquad, independently stateful on3 inputs; auxiliary scratch graph42 is not an extra audio source|
|22|C4B7F /12879C|3 gain states; explicit current, target, mode, rates|
|23|C4B96 /126A95|3-channel packed EQ, capacity16; explicit counts, records, histories|
|24–28|C4BA7,C4BBE,C4BD5,C4BEC,C4C03|Master gains then16-way fan-outs for auxiliary8/9, master gain for auxiliary10|
|29–31|C4C1A,C4C24,C4C2B|Three-input sum, scalar biquad, scalar gain; side branch only|
|32|C4C31 /12879C|16-way auxiliary10 fan-out|
|33–48|C4C48 through C4CDE /1269BC|16 independent three-input matrices, each using corresponding8/9/10 fan-out lanes|
|49|C4CE8 /126A95|16-channel packed EQ, capacity48|
|50|C4CF9 /124517|16 independent settled delays; total delay allocation ≤2880 samples; external ring2944 words|

The output order is exactly:

```text
27 26 25 24 23 22 21 36 37 50 51 52 53 54 55 73
```

These are **mixer slots**, not physical speaker numbers. They match schedule55
C4DFB input slots16..31. Earlier musical users of graph24/23/22 must not be
connected to this later path merely because IDs are reused.

### Side branch is not a hidden fourth summand

Nodes29–31 read graph27/60/76 and write graph60 after a sum/filter/gain. No later
node within21..50 consumes this new graph60. The path executes and its output
can be captured for auditing, but it is **not** injected into any of the16 B
outputs. A regression changes its scalar gain to−99 and proves output remains
unchanged while the captured side branch changes. This does not establish that
graph60 has no consumers elsewhere in the entire firmware.

## Stored targets that are now bound

`load_static_inventory()['stored_writer_targets']` deliberately returns a
**partial** scenario overlay, not a usable live preset:

- **Shared prefilter:** AB6A6 uses target table68A70. Entry1 is E05AC, the C4B70
  coefficient record. Source is the second fixed design record at file102EF0
  (`gp−1285C+8`): type0/bypass in this dump. The same record is applied to each
  of the three channels, with independent filter histories.
- **Matrices:** A928A writes three float1 coefficients to each C0840+4*i for
  i0..15. AC59E writes the same three unity coefficients to C073A for the side
  branch. “All three sum coefficients1” does **not** mean unit output gain:
  preceding per-source/fan-out gains remain required.
- **Post-EQ counts:** A952E writes16 counts to E067C from `gp−134CD`, file102277:
  `[0,8,0,7,0,8,0,7,0,0,0,0,1,4,0,0]`, total**35** of capacity48.
- **Post-EQ records:** A9A2C writes48 five-word coefficient records at E069C from
  design records `gp−134BC`, file102288. The numerical partial overlay packs
  the first35 used records according to those counts. These are **nontrivial**
  filters: the first record is approximately295Hz/type15 HP, followed by peak
  cuts; they are not replaced with unity.

These are global/fixed-calibration targets, not profile-selected values or a
readback of delivered/current state. Existing V850 biquad design math converts
records to `[-a1,-a2,b0,b1,b2]`; no new approximate EQ recipe is introduced.

## Explicit-state API

```python
inventory = load_static_inventory()
reference = SecondBAuxReference(scenario={
    "label": "explicit caller scenario, not live state",
    "stages": effective_parameters_for_all_30_descriptors,
}, inventory=inventory)
second_b = reference.process(auxiliary_graph_8_9_10)  # frames×3 -> frames×16
```

No default scenario is supplied. Each stage requires exactly these fields:

| Handler | Fields |
|---|---|
|12879C modes0/2/3|`current[N]`, `target[N]`, integer `mode`, `rates[N][up,down]`|
|12879C mode1|`current[N]`, `target[N]`, `mode=1`, `previous_mode`, `previous_target[N]`, `time_parameters[N]`, `scale`, `steps[N][up,down]`|
|126559|`records[sections][5]`, `history[channels][sections][2]`|
|126A95|`counts[channels]`, packed `records[][5]`, `history[sum(counts)][2]`|
|126092|`record[5]`, `history[2]`|
|1269BC|`coefficients[3]`|
|127A4D|`gain`|
|124517|`samples[16]`, one `history[delay]` per channel, `lifecycle="settled_dma"`|

Histories are mandatory even when deliberately zero. EQ histories use this
reference's transposed-direct-form-II z1/z2 representation; delay queues are
oldest-to-newest. They are not arbitrary unconverted binary memory dumps.

Gain modes0/2/3 are supported with fixed mode/target/rates; mode1 additionally
uses explicitly supplied previous-mode/target, time and step state (see below).
Gain updates occur
once per64-frame scheduler block and are constant within that block. The
reference starts at an explicit block boundary; partial calls preserve phase.
It does **not** pad a short final chunk, advance hidden state or automatically
flush tails. `process(..., capture_stages=True)` returns `out` and every node's
output. Deepcopy preserves state while giving an independent simulation.

The fixed delay reference excludes DMA allocation/clears/completions/errors and
delay-change transitions. General gain-mode switching after construction,
coefficient delivery/change ramps, packed-count change resets and graph
enable lifecycle are outside this numerical scenario. Mode1 entry from the
explicit previous mode is modeled. Unknown/missing stages
and unsupported modes fail rather than silently bypassing.

## Validation and remaining work

### AmpChain and cabin bridge integration

```python
chain = AmpChain(second_b_scenario={
    "reference": reference,
    "label": "explicit auxiliary scenario; not live state",
})
result = chain.process(music_pcm6, second_b_auxiliary=auxiliary_graph_8_9_10)
```

The reference is deep-copied at construction and again per whole-clip render:
repeated AmpChain calls start from the supplied initial state. For continuous
streaming, use `SecondBAuxReference.process` directly. Auxiliary PCM must be
finite real frames×3 and exactly as long as music PCM; it is **not derived from
the music inputs**. Missing auxiliary PCM or simultaneous direct `second_b`
injection fails. Source firmware hashes and the16 mixer ports must match the
main model. The result is inserted before the second pair mixer and its output
EQ, in recovered config order. Metadata becomes
`bank1-electronics-v9-explicit-second-B`, records the scenario/state hash and
retains all other warnings. Default v7 metadata is unchanged. A six-input
frequency response cannot represent independent auxiliary input and is rejected
for this scenario; use time rendering.

`amp_cabin_render` accepts `stage_inputs['second_b_auxiliary']` and includes its
hash in the report. Supply explicit auxiliary samples for the electronic tail
padding too; no hidden zero tail or measured acoustic evidence is invented.

Eight integration tests cover direct-injection equivalence, independent aux
source, state copying/reset, missing/conflicting inputs, source/port mismatch,
empty/short renders, bridge input hashes and unchanged historical default
metadata. Together with the11 reference tests below, these validate numerical
wiring, not current vehicle settings.

**18 core tests** pass (11 initial plus7 mode1 regressions), covering direct-loader bindings and extracted35-record
post-EQ, independent three-source routing and16-way order, dead-branch isolation,
nonzero history with silent inputs, unequal delays/history, per64-frame gain
cadence across arbitrary call partitions, shared-EQ independent histories,
packed per-lane EQ, explicit-state validation and deepcopy/empty-call behavior.
Synthetic tests use conspicuously labelled synthetic scenarios, not recovered
production settings.

This closes the *numerical composition for supplied effective state*. It does
not prove live auxiliary8/9/10 silence, their physical meaning, active gains or
delays. The initial helper left C4B96's packed EQ, gain targets, scalar/filter
coefficients and delay targets to the caller. The continuation below now binds
their static writers; **effective** gain/current/mode/rates, histories and actual
externally selected state still require explicit evidence. B=0 remains a
conditional scenario requiring both silent
auxiliary inputs **and drained internal state**, not a consequence of3D OFF.

## Continuation: V850 controls and remaining target overlays

`amp_second_b_control.py` adds `load_control_inventory()` and
`target_overlay(parameters, **explicit_raw_controls)`. **9 additional tests**
pass. This is a target-write plan, not a ready current-state/preset constructor.

```sh
python3 tools/soundamp-virtual-cabin/amp_second_b_control.py \
  --output assets/soundamp/second-b-control-20260920/inventory.json \
  --listings
python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p test_amp_second_b_control.py -v
```

The inventory pins the same firmware SHA, selected instruction bytes, raw
writer-range hashes, and optionally regenerates bounded r2 `.asm` listings with
their exact command and SHA. No broad disassembly-analysis pass or car access.

### Packed input EQ C4B96

A9938 loads target addresses from68A08 and design records from `gp−13590`
(file1021B4). Table entries12..15 and20..23 are−1 and skipped by the bulk writer.
Valid design indices **0..11 and16..19** map contiguously to the16 packed records
E05C0..E060B. They are non-unity filters, not padded identities. The loader
contains counts **[8,4,4]** at E05B8. No direct target-address write to those
counts was identified; that negative search is **not** proof that indirect
control cannot change them. The overlay supplies the recovered records, but
adds counts only when the caller explicitly sets
`assume_loader_input_eq_counts=True`. It never supplies filter histories.

### Three input/master gains and48 fan-out gains

AC1C6 (within the AC064 initializer) writes C04B8 from float table68BC8:
**[1,1,1]**. This is the input-gain *target*, not the initially zero current gain.

A7EB0..A80A8, part of A7D2C's writer, addresses the three masters and three
fan-outs through table **68954**, with targets
`C050B,C061B,C072B,C052A,C063A,C0750`.
For master i0..2, the target in tenths of a dB is:

```text
clamp(10*source_levels[i] + common_addition + stored_trim[i] - 82, -1000, 400)
```

`source_levels` are signed bytes at gp−5E2E/−5E2D/−5E2C;
`common_addition` is the unsigned byte at gp−3E2E. Stored signed trims from
gp−134D0 are **[48,−75,−75]**. Raw zero controls therefore calculate
**−3.4/−15.7/−15.7dB**, not a claim about live source loudness or a recommended
setting.

Each fan-out uses slot→row mapping at gp−12997
`[8,0,8,0,9,1,9,1,4,5,6,7,2,2,3,3]`, then sums the two19-entry calibration
tables at gp−12E5C and gp−12CE0 using the corresponding auxiliary source's
explicit signed position. Row3 additionally uses the gp−12AF2 sub table if
gp−3E30 bit0 is set, otherwise it is forced to−1000. The result is clamped to
[−1000,+400]. The implementation reads the **actual float lookup** at
`0x69398−4*tenths_db`, preserving the exact0 at−1000; it does not replace the
mute with `10**(-1000/200)` leakage.

Control positions are required and restricted to the calibrated−9..9 range.
The API does not infer MMI percentages or UI meaning from raw fields.

### Scalar side branch and delay targets

AC59E writes C4C30 from signed calibration byte gp−1285E, **−70 tenths dB**,
through the same gain lookup: **0.4466835856437683**. Its output remains a side
branch, not an additional B-output summand.

AB73C writes the C4C24 biquad at E0656. Explicit selector byte gp−3D5E chooses:

- value1: type3 LP, frequency selector5512, q7, gain0;
- value2: type3 LP, frequency selector11025, q7, gain0;
- other byte values: type0 bypass.

These are firmware **frequency selector codes**, not a promise that the lookup
resolves exactly5512/11025Hz. Existing lookup/design math is reused.

A9C94's second target-table entry at68A68 selects **E079C**. It copies16 uint16
delays directly from gp−1332C, file102418, in B-slot order:

```text
67 57 0 57 67 57 0 57 96 96 0 0 0 0 0 0
```

There is no68B30 output-config permutation on this second branch. These targets
sum to554, within the2880-sample allocation, but the overlay does not invent
drained rings, successful DMA startup, current delays or delivery completion.

### Mode/ramp controls are not silently converted into usable state

A80B8 requests **mode1** for the three master gain nodes, with multiplicative
ratio fields and a4.0 time parameter. Mode1 is now supported with its actual
step-generation/previous-state logic below; no substitution to mode0 is made.

AC248 initializes fan-outs to **mode3** from68BD4 and writes the32 interleaved
up/down ratio words at68BD8..68C57:
`[1.1659144163131714,0.8576958775520325] ×16`. These are recorded under
`requested_gain_control_writes`, separately from the stage target overlay.
Even supported mode3 is not labelled effective merely because its writer was
decoded. Current gains and histories remain explicit; entering mode1 uses the
supplied previous-mode value, not a guessed current vehicle mode.

Tests check the packed-hole mapping, loader-count acceptance, exact stored
gains/delays, master clamps, source-specific fan-out tables and sub mute,
side-filter selectors, separation of requested/effective modes, rejection of
incomplete state and out-of-range controls. A synthetic fixture explicitly
combines these targets with chosen effective mode/history solely to test the
numerical chain; it is not saved as an OEM/live preset.

## Mode1 follow-up: actual SHARC step generation, not a generic ramp

`mode1_gain_update()` and the mode1 branch of `SecondBAuxReference` now model
12879C's explicit prior-state behavior. Relevant listing:
`assets/soundamp/remaining-blocks-20260919/gain.asm`; additional decoded
instruction pins are checked against the SHA-pinned loader during inventory
extraction.

1. At1287B9..1287CD, descriptor+22's previous mode is compared with requested
   mode and updated. On change, current gain+9 is copied to previous-target+13.
2. At128802..12880D, compare **absolute target magnitude** with the absolute
   previous-target magnitude. Equal magnitudes retain the existing positive
   step at+15. A sign-only target change does not recompute its size.
3. The delayed branch at12880D still executes128810 and128813, including loading
   that old step. Skipping those instructions would wrongly reuse another
   channel's scratch register or invent a zero step.
4. For changed magnitude, compute:

   ```text
   delta = abs(abs(target) - abs(current))
   denominator = time_parameter * descriptor[11]
   if denominator <= 0: denominator = 1
   step = delta / denominator
   ```

   This is **not** `max(denominator,1)`: a positive denominator below1 is retained.
   The loader's descriptor+11 is0.75; the reference still requires `scale`
   explicitly. A writer's4.0 time and this0.75 value therefore imply a three-
   block magnitude ramp for a newly changed target, conditional on that state.
5. At12882D/128830, write `[+step,−step]`; at128833..12885A apply the additive
   step toward target magnitude with clamping, then copy the target sign. The
   new gain multiplies the current64-frame block. Previous-target becomes the
   signed target for the next call.

The helper returns updated current, previous target/mode and steps, plus which
channels recomputed their step. Full audio processing preserves this state and
64-frame cadence across arbitrary caller chunk sizes. Existing mode0/2/3
scenario schemas are unchanged. Mode1 without its additional state fails.

Seven regressions cover changed-target division, retained old step and corrected
down-step sign, mode-entry previous-target replacement, nonpositive versus
positive-subunity denominators, decreasing targets, sign-only changes and
uneven streaming partitions. Float64 division represents the mathematical
RECIPS/Newton-refinement result, **not instruction-rounding equivalence**.
This does not model a live control queue or silently certify writer delivery.
