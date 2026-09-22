# Effective first-B tail binding and composed-model check — 2026-09-20

Static/offline only. No car contact, firmware edits, production preset/default
change, deployment or commit. Coefficients are supplied effective-state
**scenarios**, not a readback of what the vehicle is running.

## What is connected

`amp_b_tail_binding.py` binds all three fixed time-domain tails and the existing
eight-channel finish to one explicit829-word effective configuration plus all18
mode-pointer offsets. Spectral generation still requires its own explicit
`BSpectralFrameReference`; object97 input gain is supplied separately because
it is outside the configuration starting at object299.

`build_effective_mode5` joins those components into the reference already
accepted by AmpChain. Binding provenance is retained in first-B metadata and
the complete parameter/history fingerprint. No default model metadata changes.

Offsets below are32-bit words, relative to effective config=object299. Program
addresses are SHARC shortwords, not V850 file offsets.

| Tail | Bound parameters |
|---|---|
| Four,12EDE6 | common delay32; pair banks33/77; final gain through1156 |
| Six,12A7F9 | pre-gain123; common delay1159 pointer; nested allpass records1167..1170 pointers; pair banks145/196/247; single allpasses189/192/240/243; pair gains1171..1173 pointers; final gain1157 pointer |
| Two,132F86 | common delay1160 pointer; pair bank316; nested records360/368; single records365/373; final gain1158 pointer |

Every pair bank is `[delayL, delayR, gain, section_count, coefficients...]`.
Each coefficient record is `[b0,b1,b2,-a1,-a2]`. Maximum inline capacity is8
sections. Zero-section lifecycle is not silently substituted with identity.
These tail gains are **additional** to the inline branch gains122/293/377 and
final gain420 applied by the output finish.

Delayed reads, old parallel operands and call arguments were checked against
the loader. The tests independently execute the three decoded outer callers
and12EB9B with successful callee stand-ins, deriving passed addresses instead
of importing the binding's offset table. This validates binding, not callee
error behavior. `tail_binding_evidence()` checks the actual loader, eight code
range fingerprints and33 decoded instruction pins.

## Conversion and zero-delay detail

Nested allpass enable rounds by float32 addition0.5 then truncation. Single
allpass enable truncates without0.5. Thus0.9 enables a nested stage but disables
a single stage. Delay requests and EQ counts round; the host binding deliberately
restricts converted controls to a nonnegative bounded domain instead of
guessing firmware unsigned overflow/error behavior.

An enabled nested delay0 is **not bypass**. With zero cursor and at least one
valid backing word,12D10E reads/writes cell0 then wraps to0. The opt-in
`NestedAllpass(..., allow_zero_delay_cells=True)` represents this as one stored
sample and preserves the original requested zero in the binding metadata.
The old constructor default remains strict-positive. This behavior is used by
the retained B01 bank4 default second nested stages. Example `[1,0,0,96,-.3]`
has97-sample feedback, not96. See the independent
[zero-delay instruction/recurrence proof](SHARC_NESTED_ZERO_DELAY_2026-09-20.md).
No conclusion is made about arbitrary initial cursors, valid real allocations,
single-allpass delay0, delay changes or live bank selection.

## Explicit use

```python
reference = build_effective_mode5(
    effective_config=effective829,
    pointers=all18_config_relative_pointers,
    library_mode=5,
    assume_successful_settled_allocations=True,
    zero_histories=True,
    spectral=explicit_spectral_reference,
    input_mix_gain=explicit_object97_gain,
)
```

The two boolean assumptions are required. The builder starts fresh zero tail
histories and copies the supplied spectral reference with its supplied history.
For AmpChain, common input binding, wrapper delay and scenario label are still
required separately. The conditional steady wrapper delay is1024 samples only
under the [on-time successful callback assumptions](SHARC_B_WRAPPER_TIMING_2026-09-20.md).

Twelve binding tests cover all8 retained calibrations, independent decoded caller
addresses, every pair field, active/disabled conversion differences, preservation
of distinct tail/finish gains, streaming partitions and fail-closed invalid
state. One additional decorrelator test checks opt-in zero requests against
independent transfer polynomials, including zero inner/outer/both requests.
An independent review caught and fixed two boundary issues: the built reference
now retains original requested records and config/pointer hashes (zero and one
must remain distinct scenarios even when their audio matches), and the existing
finish binder now uses float32 rounding for section counts too. The float32 value
immediately below0.5 rounds to1 after adding0.5; float64 addition had incorrectly
produced0. Stored integer section counts and default music renders are unchanged.

## All-stage integration is not full-fidelity evidence

`test_amp_all_stage_composition.py` adds3 tests connecting explicit common input,
first B with retained B01bank1 tails but **synthetic spectral/controls**, second B
with **synthetic independent aux state and mode1 gain entry**, full C4D17 with explicit auxiliary,
pre/post gains and active limiters. The4097-frame seeded synthetic render equals
a separate run with both computed B boundary signals injected (max error0).
Repeated renders restore the supplied state and do not modify caller histories.

The readiness report can now show all electrical **scenario-presence** gates
true for this constructed case. It still reports actual vehicle state, cabin
fidelity and subjective optimality as false. Presence is not a proof that these
parameters belong together in the car or that all firmware modes/lifecycles are
emulated. No musical bass or SPL conclusion follows from this wiring fixture.

The generated [composition report](../output/upmix-composed-static-20260920/report.json)
records input/output hashes, source hashes, stage shapes, model metadata and
the readiness result. Do not combine its all-scenarios gate with historical
default-v7 stress cells to claim that this different scenario has392-cell coverage.

Reproduce:

```sh
OPENBLAS_NUM_THREADS=1 python3 -m unittest discover \
  -s tools/soundamp-virtual-cabin -p test_amp_b_tail_binding.py
OPENBLAS_NUM_THREADS=1 python3 -m unittest discover \
  -s tools/soundamp-virtual-cabin -p test_amp_all_stage_composition.py
```

Evidence image: CodeFlash SHA256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`;
loader SHA256 `9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Numerical kernels remain host floating references, not SHARC bit equivalence.
