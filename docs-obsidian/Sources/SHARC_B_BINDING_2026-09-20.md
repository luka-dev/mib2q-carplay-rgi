# B effective-parameter binding to output audio — static/offline only

Follow-up: [nonzero12AB1F EQ design](SHARC_B_EQ_DESIGN_2026-09-20.md) now has
an explicit mathematical-reference opt-in; the default replay behavior below
remains zero-gain-only. This is not bit-exact firmware or actual-car evidence.

## What is now connected

`amp_b_bound_scenario.py` connects an **explicit effective 829-word configuration**
to the existing successful mode2/5 `BOutputFinishReference`. It no longer requires
manually copying the three branch gains, each filter section, and the final gain
into an unrelated synthetic fixture. The eight retained loader/B01 banks can be
decoded as named scenarios; their 829-word SHA256 is checked before use.

This is the output finish **after the six/four/two tails**, not an entire B
generator. Inputs are three explicit frame arrays with 6, 4 and 2 channels.
Those channel numbers are library boundaries, not physical speakers.
No AmpChain defaults, vehicle files, firmware or deployment were changed.

### Inline audio bindings

Effective configuration base is object+299. All offsets below are 32-bit words;
PCs are SHARC 16-bit shortword addresses, not V850 byte offsets.

| Stage | Object offset | Config offset | Evidence PC |
|---|---:|---:|---|
| Four-branch scalar | 421 | 122 | 1314D3 →132D0B at1314DA |
| Six-branch scalar | 592 | 293 | 131487 →132D0B at131490 |
| Six-channel EQ count, then records | 593 | 294,295…304 | 131383 →12DBDE at13138C |
| Two-branch scalar | 676 | 377 | 131565 →132D0B at13156D |
| Two-channel EQ count, then records | 677 | 378,379…388 | 1313DF →12DBDE at1313E6 |
| Eight-channel EQ count, then records | 698 | 399,400…419 | 12B88B →12DBDE at12B8BA |
| Final eight-channel scalar | 719 | 420 | 12B8E9 →132D0B at12B8F0 |

These are **inline** banks, not the pointer-selected tail gains at1156..1158.
The distinction matters: applying both as if they were alternatives would
silently remove one gain stage. `bind_effective_mode` separately dereferences
only the 14 proven pointer consumers, keeping all18 pointer offsets explicit.
Unresolved1162..1165 are listed, never turned into zero/identity stages.

### Further classification of1162..1165 (not consumer closure)

Cross-binding the pointer maps to the separately decoded API19 parameter calls
classifies their **pointee records**:

| Mode-map variant |1162|1163|1164|1165|
|---|---:|---:|---:|---:|
| Default |533|536|578|581|
| API16=5 last |494|497|500|503|
| API20=3 last |427|430|433|436|

Columns are respectively negative-side descriptor52/53 curve records, then
positive-side descriptor52/53 curve records. Each is an x/y/shape triple for
the two individual tail delays, not an unidentified audio biquad. A new test
cross-checks all four calibration-bank bindings and all four map variants.

The direct API19 path reads target buffer object1180 at12BF3A, selects by
**shadow**16 at12BF91 or shadow20 at12C4B1, constructs the first addresses at
12BFA1/12C4C1/12C4D8, and passes the records to52/53 calls12C1E9/12C227.
It does **not** establish an indirect read of pointer words1162..1165. Those
words point into the **effective** config and follow applied-command order;
their domains cannot be interchanged during transitions/deferred updates.
The pointer consumers remain unresolved and no dead-field claim is made.

### Affine propagation audit and bounded smoothing closure

`tools/audit_amp_b_pointer_aliases.py` now audits the complete **hash-validated
12A7F9..1331AF span**, beyond the previous literal census: integer register
copies/add/subtract/MODIFY, postincrements, old operands in parallel clauses,
and exact local stack spills are propagated inside linear blocks. Branch,
call and loop joins are explicit provenance cuts. This is a nominal linear-
address audit, not DAG circular/SIMD or whole-program alias emulation.

Artifact `assets/soundamp/remaining-blocks-20260919/b_binding_pointer_aliases.json`
contains16,093 instructions,2,433 blocks,3,942 nominal DM reads,4,398 stores,
1,002 direct-call argument records,66 non-affine reads and2,277 explicit cut
sites. Synthetic tests hide1162 behind arithmetic, postincrements and a stack
spill and require detection. Actual candidate results remain the14 previously
known pointer reads. **This is not another claim that a missing literal proves
unused fields.** Every unresolved/cut address stays visible in the artifact.

One significant dynamic path can now be closed conditionally: coefficient
smoothing12FAF2. The helper itself checks **only a lower bound**, at12FAFC/12FAFE:

```
if unsigned(R4) < unsigned(DM[B903A]): skip
index = R4 - DM[B903A]
current = DM[R4]
target  = DM[DM[B903B] + index]      #12FB0B
step    = DM[B8CFD + index]         #12FB16, if current != target
```

There is **no upper829-word range check** in this helper. Normal registration
of effective config base object299 does not, by itself, prove that a caller
cannot request an address at object1162. A decoded prefix test exercises offsets
298,299,1127,1128,1162,1165: only298 is excluded by that lower bound.

Instead the new domain proof enumerates all **seven direct callers** in this
span and all their direct wrapper callers:

| Direct calls to12FAF2 | Wrapper | Recovered address domain |
|---|---|---|
|12DC8A/12DC92/12DC9D/12DCA8/12DCB2 |12DBDE | Count-bank base+1..5*N; five calls per section in descending coefficient order |
|12F454 |12F411 | Exact delay parameter pointer supplied inR8 |
|132D23 |132D0B | Exact scalar parameter pointer supplied inR4 |

The enclosing calls are also enumerated and checked: four calls to12DBDE,
five to12F411, thirteen to132D0B, and six fixed pair-bank calls to12EB9B.
Every callsite and its reason/domain appears under `smoothing_caller_domains`.

For the **eight retained loader/B01 calibrations and normal decoded mode pointer
maps**, including mixed apply order:

- Pair banks start at object332/376/444/495/546/615. Their delay addresses are
  base/base+1, scalar is base+2, EQ count is base+3. B01 counts are8 for these
  six banks; loader counts are2/1/7/5/6/3. Thus these loops are explicitly
  enumerated, not guessed from allocated capacity.
- Other count banks are object593/677/698 with retained counts2/2/2(loader)
  and2/2/4(B01). The independent decoded12DC79..12DCD1 loop verifies every
  coefficient address for1..8 sections.
- Mode-pointer scalar/delay consumers dereference1156..1160 and1171..1173.
  Their **pointee** offsets, not the pointer-cell addresses, enter smoothing.
- Across all these callers, nominal addresses passed to12FAF2 range from
  object97 to object807. Object97 is skipped by the config-base lower check.
  **None reaches pointer cells1162..1165 through this smoothing route.**

First four-tail pair base332 requires one extra call-boundary check:12EE80
loadsR14=332; the copies at12EE85/12EE91 invoke132E08, whose complete decoded
body does not writeR14;12EE9D therefore passes object+332. This is a specific
callee preservation proof, not a blanket assumption about all call effects.

The conclusion is conditional on unchanged retained counts, normal pointer
rebinding, fixed caller bases and no indexed/source-RAM mutation or external
caller. A synthetic count93 at final bank698 makes the loop's nominal domain
include1162/1163; the audit correctly withdraws its exclusion. This does not
claim that such state occurs, survives earlier errors or produces audible output.

This closes **12FAF2 as a hidden consumer path in the stated retained scenarios**,
not the global consumer question. Remaining concrete alias work includes
reset-instance traversal12D97A/12D97C, filter-instance traversal12DCDB/12DCE5,
and caller-base propagation across the recorded cuts. Generic read sites
131AFF/131B57/131BAD have separately recovered request domains; their normal
upper ranges do not directly reach1162, but that is not a general alias proof.

Reproduce the artifact, including the conditional caller-domain table:

```sh
python3 tools/audit_amp_b_pointer_aliases.py \
  --loader assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin \
  --config-report assets/soundamp/remaining-blocks-20260919/b_config_control.json \
  --mode-report assets/soundamp/remaining-blocks-20260919/b_mode_control.json \
  --out assets/soundamp/remaining-blocks-20260919/b_binding_pointer_aliases.json
```

All retained banks have two six-channel and two two-channel EQ sections.
Loader banks have two final sections, whereas B01 banks have four. B01 banks1–3
have final scalar1.12201845646; B01 bank4 and loader banks have1. These are
internal coefficients, not claims about actual-car loudness or bass.

The output constructor requires explicit library mode2/5 and explicit successful
active filter path. It rejects mode6 (different early-return topology), zero-
section lifecycle, out-of-bank counts and unstable sections rather than inventing
behavior. Fixed coefficients and fresh zero filter histories are assumptions;
streaming thereafter preserves state. No smoothing, ready/reset/mute/error or
mode-transition emulator is claimed.

## Replay composition: deliberately partial

`replay_output_target_slice` connects the existing API19/18 parameter helpers
and all-nine-control branch-gain helper. It takes all15 prior descriptor slot
banks, all9 effective control values, both shadow selectors, and the chosen
extracted calibration explicitly. It exports:

- branch targets46/50/51 at config122/293/377;
- API19-selected individual two-tail delays at316/317, rounded to samples;
- four rebuilt zero-gain EQ sections at295/300/379/384;
- retained contribution state with the limited helper coverage named explicitly.

Replay132168 uses **raw effective API25**, unlike single-update13262D's
baseline-adjusted coordinate. API18 does not overwrite delay descriptors52/53,
so API19's selector-sensitive result must survive it. Prior descriptor51 slot1
is also preserved, not silently zeroed. Nonzero EQ designs fail closed by
default; `allow_nonzero_eq=True` selects the separately audited mathematical
designer and labels that assumption in result metadata.

**Do not overlay this slice and call the resulting vector a full settled replay.**
The extracted call census shows API22 at12C9B7 touches46/47/48/50/51/54, and
API21 at12CB59 touches46/48/50/51/54. The previous branch-gain helper covers only
46/50/51. Thus later writes to47/48/54 (including final gain420) are not yet
composed **by this older partial entry point**. That helper explicitly omits
final gain and returns no complete effective config or finished audio object.
The bounded follow-up below closes the missing scalar calls separately.

Checked examples, conditional on zero other slots and the stated controls:

- B01 API18=9/API27=180 gives six-branch target3.2/3.2/2.5/1 across banks1–4.
- B01 API19=−50 gives individual two-tail delays162/162 samples in bank1 and
  140/140 in bank2. These are not total wrapper/acoustic latency.
- Retaining descriptor51 slot1=.25 gives target1.85 at otherwise zero controls;
  dropping the slot would incorrectly give1.6.

## Follow-up: API21/22 and all15 scalar replay targets

`api21_22_bindings` and `api21_22_parameter_targets` now bind all11 successful
scalar calls, including the previously omitted47/48/54:

| API / slot | Descriptor → two curve-record offsets | Clamp |
|---|---|---|
| 22 /2 | 46→671/689;47→674/692;48→677/695;50→680/698;51→683/701;54→686/704 | 0..5 |
| 21 /3 | 46→707/722;48→710/725;50→713/728;51→716/731;54→719/734 | 0..5 |

Each record is x/y/shape; target buffer1180 supplies records. Bases come from
the chosen retained calibration's descriptor binding, not guessed current
smoothed values. Source/target-RAM mutations still require explicit new data.
API22's newly covered call PCs are12CA50/12CA88/12CB30; API21's are12CBE8/12CC90.
All targets preserve the other eight additive slots and clamp only after summing.

`replay_bound_scalar_targets` composes19→18→22→21 and the remaining five gain
helpers, returning **all15 scalar descriptor targets**, their target config
updates and the four supported EQ rebuilds. It includes final gain420 and no
longer has the47/48/54 omission. The old partial helper remains explicitly
partial; callers must opt into the new, stronger entry point. Neither function
returns a whole effective configuration or emulates publication/smoothing.

Material retained-calibration finding: API21 reaches final eight-channel gain,
not only the three branch gains. With zero other slots and API19/18=0:

| Retained source/bank | API21−9 | API21=0 | API21+9 |
|---|---:|---:|---:|
| Loader1–4 | 1 | 1 | 1 |
| B01 1/2 | .72201845 | 1.12201846 | .72201845 |
| B01 3 | .12201846 | 1.12201846 | 1.12201846 |
| B01 4 | 1 | 1 | 1 |

These are linear final-stage targets, not total B gains, UI Bass percentages,
actual current controls, physical SPL or proof of the cause of reported bass.
API22's newly covered47/48/54 curves are zero in all8 retained calibrations;
API21's48 curves are also zero. Synthetic nonzero curves verify the code does
not incorrectly hardcode these zeros.

## Verification and usage

30 binding/alias tests pass, including all eight retained banks against independent
`scipy.signal.sosfilt` output calculations, streaming state continuity, decoded
consumer immediates, calibrated replay examples, slot retention, mutated source
word rejection, and fail-closed handling of unsupported modes/counts/EQ design.
API21/22 checks independently execute both decoded callers to collect all11
descriptor/slot/pointer/count/clamp arguments (callee success stubs, explicit
ABI bridge), verify caller machine-word hashes, and compare440 combinations
of retained-bank/control/call against the decoded float32 curve-kernel oracle.
The filter comparison covers ordinary stable fixed coefficients, not SHARC
bit-exact rounding or firmware lifecycle. Existing extraction suites validate
the image/code fingerprints underlying the reused JSON/listings.

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p test_amp_b_bound_scenario.py
```

Python entry points (module path `tools/soundamp-virtual-cabin`):

```python
config = retained_config(config_report, source='b01', bank=1)
# The retained image is explicitly a scenario; not a measured effective snapshot.
finish = build_effective_finish(effective_config=config, library_mode=5,
                                successful_filter_path=True)
out8 = finish.process(six_tail_audio, four_tail_audio, two_tail_audio)
```

Artifacts reused: `assets/soundamp/remaining-blocks-20260919/`:
`b_config_control.json`, `b_mode_control.json`, `first_b_algorithm_dispatch.asm`,
`first_b_eight_channel_finish.asm`, and the API control listings. Source raw
V850 LE32/base0/size0x180000 SHA256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`;
SHARC loader SHA256
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.

Still required for calibrated whole-B music: bit-exact math if needed and lifecycle,
spectral/tail parameter and switch binding, effective pointer/control history,
wrapper scheduling, physical input/output mapping, and actual active-state
evidence. None of these helpers proves B=0, B active, or the user's current EQ.
