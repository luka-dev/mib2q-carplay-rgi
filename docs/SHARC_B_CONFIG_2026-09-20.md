# J525 B configuration replacement and replay — static only

The B library's calibration-bank selector is **not** the input matrix's
stereo/5.1/7.1 bank. Static bindings now cover API17 bank replacement, all four
stored B calibrations in loader/B01, descriptor rebase and effective-control
replay. Neither actual vehicle state nor B=0 is established. No vehicle I/O,
deployment, firmware edits or commit; AmpChain defaults unchanged.

## Three distinct identifiers

| Identifier | Domain | Recovered use |
|---|---|---|
| Input matrix bank | 0/1/2 | stereo/5.1/7.1 matrix inside an A07 profile |
| A07 profile | signed table value | table100008 indexed by two control states |
| B calibration bank | API17=1/2/3/4 | one of four829-word parameter blocks |

V850 event46 atAA468 reads `s8(100008 + u8(gp−17770) + 21*u8(gp−17810))`,
adds1, converts to float and sends API17 throughAC648. In the table domain
state_a0..20/state_b0..1, both rows are `[-1,0,1,-1,...,-1]`:

- state_a1 → A07 profile0 → API17 candidate1;
- state_a2 → A07 profile1 → API17 candidate2;
- other entries → candidate0, **rejected** by the SHARC API17 validator.

An invalid candidate does not select a guessed default B bank. Calibration
banks3/4 exist and API17 accepts them, but this normal table writer does not
produce them. Generic/other commands remain a separate route. A matrix bank1
(5.1) does not by itself imply B calibration bank1 or2. No delivery or current
state is inferred from this formula.

## API17: copy, rebase, replay, publish

Jump tables B913D/B914B index1 bind validator131E97 and applier1324A6.
The validator accepts exactly float1/2/3/4. The applier always stores shadow
object1143; if defer object1178 is nonzero it does not invoke132346.

On successful nondeferred selection,132346:

1. Uses source `object1179 + (uint32-trunc(value)−1)*829`.
2. Calls1329AA: version word must be0x40128F5C (float2.29), and checksum of
   words0..827 must equal word828. Invalid source fails before copying.
3. Copies829 words to the target buffer referenced by object1180.
4. Calls1320CE→12AF83 for descriptors44..58. Each descriptor's base at+2 is
   refreshed from its target pointer at+1. **Its nine additive slots are not cleared.**
5. Calls132168 to replay stored effective controls in order
   19,18,22,21,23,24,25,27,29, then publishes via12FBA5.
6. Stores selected value to effective object1129. This store is after the replay
   call even if it returned an error; do not assume transactional rollback.

The replay uses effective object offsets1131,1130,1134,1133,1135,1136,1137,1139,1141.
It also appears after the mode helpers for API16 and20, at132489/13255A.
The preceding mode-helper pointer writes are now bound separately below.

Important caller difference:1322A1 passes **raw effective API25** to12CE71.
The single-update applier13262D instead passes `value−object1090` (config791).
All eight extracted calibration banks have baseline791=0, and their API25
branch-gain curves are zero. Therefore this difference is not evidence of an
audible defect in these stored tables. Synthetic tests ensure it is not erased
if a later configuration uses a nonzero baseline/curve.

## All four stored calibrations

Loader source baseC3C6A; B01 whole-source byte offset12000C. Bankk starts at
source word `245+829*(k−1)`; V850 bank1 starts1203E0. Offsets are DM32 words
for SHARC data, byte/file offsets for V850. PCs in listings are SHARC16-bit
shortword addresses.

| Source | Bank1 CRC | Bank2 CRC / changed words | Bank3 CRC / changed words | Bank4 CRC / changed words |
|---|---|---|---|---|
| Loader | 90A9 | 3EDF /2 | A888 /2 | 0250 /2 |
| B01 | 3104 | CE33 /31 | 5BD1 /53 | B8EE /271 |

Counts compare all829 words against source bank1, including bank-number and
checksum words. Loader banks differ only at those two words. B01 banks have
real calibration differences. Whole3562-word source CRCs are74A9(loader) and
4A94(B01). The1328EA algorithm processes each word's low-to-high bytes with
tableB915A, zero initial CRC and16-bit result; independent `binascii.crc_hqx`
matches every extracted bank. The entire CodeFlash hash is still validated.

Conditional examples with retained B01 tables and explicitly zero other slots:

- Bank2 API19−50/0/+100 gives separate two-tail delays140/96/42 samples;
  bank1 gives162/96/19. At−50, two-branch gain is1.45 versus1.30.
- API18=9 plus API27=180 gives six-branch gain3.2/3.2/2.5/1.0 for banks1..4.
  These are linear target gains, not loudness or physical-speaker levels.
- The four previously bound EQ design gains and stored identity sections remain
  zero/identity in all four banks of both sources. This says nothing about other
  EQ stages or nonzero prior contribution slots.

`gain_controls(..., config_bank=k)` now exports the selected bank's bases,
curves, design records and provenance addresses. Defaultk=1 preserves existing
conditional callers, **not an actual-car assumption**. New output
`assets/soundamp/remaining-blocks-20260919/b_config_control.json` contains all
bank words, hashes, CRCs, difference offsets and per-bank control bindings.
`b_config_request` models the table-domain candidate; `replay_branch_gain_targets`
models only the three-gain slice using raw effective controls and explicit prior
slots. No full config-switch/audio/queue/failure emulator is claimed.

## API16/20: effective-configuration pointer selection

Helper12B348 writes **pointers**, not coefficient values. Its18-word table is
object1156..1173; every offset below is relative to **effective** configuration
base object299, not the desired target buffer. API16 accepts2/5/6; API20 accepts1/3/6.

| Pointer words | Default config offsets | API16=5 offsets | API20=3 offsets |
|---|---|---|---|
| 1156..1165, shared | 121,292,376,124,315,31,533,536,578,581 | 488,489,490,491,492,493,494,497,500,503 | 421,422,423,424,425,426,427,430,433,436 |
| 1166..1170, API20 only | 3,125,130,135,140 | retained | 439,467,472,477,482 |
| 1171..1173, API16 only | 195,246,291 | 506,507,508 | retained |

API16=2/6 uses its default mapping; resolved API20=1 uses its default mapping.
This does not equate API16=6 audio behavior with2: algorithm bypass selection
is separate. Initialization12B48F applies20(value1), then16(value2), yielding
all18 default pointers.

Both APIs overwrite shared1156..1165, retain the other API's suffix, then replay
effective controls. **Last applied wins for the shared pointers.** Therefore
effective16=5/20=3 alone does not determine the selected parameter records.
For retained B01 calibration1, pointer1156 reads four-tail final gain0.630957
when16 was last, versus0.446684 when20 was last. These are internal linear gains,
not the three previously bound descriptor gains, total bass level or speaker SPL.

API20 request6 resolves at132577 using object786 = effective config487:
exact float1 gives resolved1; anything else gives3. This happens **before** the
shadow1146 store and defer1178 check. Deferred requests thus store resolved1/3,
not6, without changing effective1132 or pointers. API16 uses shadow1142 and
effective1128. All four loader banks have config487=3; all four B01 banks have0;
both retained-image scenarios resolve AUTO6 to3, not proof of live configuration.

Pending loop132402 processes APIs16..29 ascending: if16 and20 are pending in
the same pass,16 applies first. V850 event48 atAA3C4 sends16 then20 when
gp−15760==0; otherwise20, wait-helperDCE98(15), then16. **Sender order does not
prove SHARC apply order** without queue/pass timing. Do not infer current state
or the cause of the bass excess from this ordering alone.

Proven consumers cover three final tail gains1156..1158, common six/two-tail
delays1159/1160, post crossfeed1161, spatial record1166, four nested-allpass
records1167..1170 and three six-tail pair gains1171..1173. Consumers of1162..1165
remain unbound; do not interpret these as four missing filters without evidence.
`b_mode_control.json` exports pointer maps and values for all eight stored banks.
`amp_b_mode_control.apply_mode_control` requires all prior pointers, shadow and
effective selectors; `resolve_mode_parameters` requires an explicit effective
829-word float configuration. Neither guesses history or changes AmpChain defaults.
Replay/publication and audio composition are not emulated by this pointer helper.

## Pointer1161 crossfeed and the four unresolved fields

Caller1315AE/B0 passes object+59 to12E80E inR8. The wrapper saves it at
stack−2 (12E841), then forwards it inR8 at12EA39/3B to12E521. That function
preserves it inR3 (12E556), setsI5=R3 (12E672), and readsDM(+1102,I5) at12E683:
**59+1102=1161**. Call12E686→12E487 dereferences this pointer at12E48E.
This explains why a search for a literal read of1161 found nothing.

The scalar `c` directly mixes complex spectra in the post branch before130771:

```
Lout = (1-c)*L + c*R
Rout = (1-c)*R + c*L
```

There is no clamp on c in this kernel. The sum is preserved; the difference is
multiplied by1−2c. Identical mono L/R therefore passes unchanged through **this
stage alone**, including mono bass. This does not establish the full B path's
gain, speaker sum, LFE behavior or cabin response.

| Retained calibration | Default c, config31 | API16=5 last, config493 | API20=3 last, config426 |
|---|---|---|---|
| Loader, all four banks | .15 | 0 | .15 |
| B01 bank1 | .25 | .27 | .27 |
| B01 bank2 | .25 | .27 | .28 |
| B01 bank3 | .25 | .30 | .30 |
| B01 bank4 | .20 | 0 | 0 |

Rounded display values; JSON retains extracted float32 values. These are B
calibration banks, **not matrix banks**. `resolve_mode_parameters` now returns
`post_crossfeed` for the existing `amp_b_pipeline.crossmix` reference. It still
requires explicit prior pointer state and effective configuration.

An aligned literal census of12A7F9..1331AF covers16,093 decoded instructions,
UNKNOWN0, SHA256 `a30e9c631734d5846aa97ac2ac4c50dacc1769b5284c282960363070880552d4`.
It finds14 immediate DM-offset reads in the full-object/core pointer-table
domains, but none for1162..1165 or their core-relative aliases1103..1106.
`b_mode_control.json.pointer_literal_census` retains exact hits and scope.
**This is not complete alias analysis:** dynamic offsets, other base aliases,
external writes and code outside the span are not excluded. The four fields
remain unresolved, not proven unused. Their three-word JSON previews follow
pointer spacing only; no record type or audio function is assigned.

Search also located additional12B348 calls at1327EA and132805 inside1326C7.
Their indexed-command entry and call sequence are now bound below, separately
from the normal API16/20 reference.

## Extended FIFO1326C7: direct writes and source RAM mutation

Static entry131A65 accepts canonical extended commands below after ready1 and
nonnull payload-pointer checks.131A19 copies the **raw payload word**, not its
pointer, into a two-word record atB909C. CountB911C must be<64 before append.
The branch after getter131A10 preserves earlier arithmetic flags: it separates
ordinary/extended command classes, **not an empty/nonempty queue test**.

| Canonical command | Accepted domain | Direct effect when FIFO record applies |
|---|---|---|
| `0x14000000 + index` | index1..842; arbitrary raw32 payload | writes object299+index AND target1180+index |
| `0x12000000 + index` | index4..206; arbitrary raw32 payload | writes object92+index only; index5 reaches input gain97 |
| `0x10000000` | raw integer0/1/2 | writes defer1178; does not replay shadow controls itself |
| `0x20000003` | raw integer bank1..4, **not float bits** | refreshes effective config CRC and copies829 words into source RAM bank |

These are selected canonical families, not a complete arbitrary-bit-pattern
protocol model. The ordinary API-specific float validators are not used for
indexed payloads. The indexed writes do not consult defer1178 or update shadow
controls. Generic outer A21 forwarding is now bound in the
[outer-control audit](SHARC_OUTER_CONTROL_2026-09-20.md); no fixed normal sender
of these headers or actual playback delivery has been established.

12DE66 calls1326C7 before audio processing. If normal pending flagB911D is set,
the ascending API16..29 pass runs first. The extended FIFO then runs in stored
order only if that pass returned0 and count is1..64. Count and pending flag are
cleared at exit even if the FIFO was skipped after a pending error. Error-logger
return policy and concurrent producer timing are not fully emulated.

### The829/830 boundary and repeated mode switch

At1327C1 the first write targets object299+index. The second write at1327CB is
in a **branch delay slot** and executes on either side of `index<=829`.

- Indices1..829 then call descriptor rebase1320CE only. No replay, pointer
  switch or publication is performed by this branch.
- Indices830..842 call132168 effective-control replay, then12B348(API20),
  then12B348(API16), then132346 current-bank replacement.
- Both pointer calls take **object1132, effective API20**, read at1327E2 and
  again1327FD. The second call does not take effective API16/object1128.
  This is recorded as firmware behavior, not corrected to an assumed intent.
  Error-report paths continue to subsequent calls; no rollback is inferred.

829..842 overlap effective controls16..29 atobject1128..1141. Thus index829
can change effective API16 without the repeated mode-switch sequence, while
index833 changes effective API20 before both helpers read it. Shadows stay
unchanged. For ordinary effective20=3, inner API16(value3) chooses its default
pointer mapping even though the **public API16 validator rejects3**. Combined
with the preceding API20(value3), this leaves shared pointers default, spatial
suffix20=3, and pair-gain suffix default. The decoded helper test verifies this;
the model must not substitute the public API16 setter for the inner call.

In normal initialization target baseB89C0+829 isB8CFD, the start of the previously
bound interpolation-step array. Extended writes must therefore not be silently
clamped to an829-word target vector. The model exposes these addresses as
out-of-config writes; it does not assert a crash or audible fault.

### Source-store and read boundaries

`0x20000003` reaches132AEF with core=object+59 and destination
`object1179+(bank−1)*829`. It computes CRC over effective config words0..827,
writes effective word828, then copies all829 words into that source bank. This
is a **SHARC RAM source mutation**, not a flash write, target publication or
active-bank selector change. A later API17 replacement may consequently read
modified data rather than the original loader/B01 bytes, conditional on this
command having executed.

Immediate read families use object299+index0..857, object92+index0..206 and
object59+index0..32. The broadest ends atobject1156. Validated indexed writes
end atobject1141. These bounds do not directly reach unresolved1162..1165;
they do not rule out other indirect aliases or writers.

`b_indexed_control.json` exports hashes, domains, control/target overlaps and
scope. `amp_b_indexed_control.indexed_command_plan` requires all14 prior raw
effective control words, ready and queue count. It returns direct stores and
ordered call arguments for one accepted record. **It is not full FIFO, callee,
memory, smoothing or audio emulation.** Existing AmpChain defaults are unchanged.

## Validation and remaining work

392 tests pass:117 extraction +275 model. All166 bounded SHARC ranges decode
with UNKNOWN0;41 V850 level/control listings remain validated. Added tests cover
CRC independent implementation, image/code mutations, bank-address provenance,
all42 selector entries, profile-specific targets, retained slots and raw API25.
API16/20 adds hash/dispatch/consumer pins, all five non-AUTO pointer paths checked
against bounded execution of decoded instructions, order sensitivity, AUTO/defer,
suffix retention and effective-record dereferencing. The execution oracle bridges
the suffix helper ABI explicitly; it is not a complete firmware emulator.
Crossfeed tests add all eight banks, explicit-pointer dereferencing, mono/difference
invariants and11 coefficient cases against execution of decoded12E487 caller
instructions with host-vector callee stand-ins. No bit-exact SIMD claim.
The census test injects a missing-offset read and checks that it is detected;
production extraction rejects the modified code hash.
Extended-write tests compare all842 config/control indices,203 core indices,
three defer values and four source-store values against a bounded decoded-record
interpreter. Callees are recorded and return success without side effects.
An independent decoded validation-branch check covers3,213 boundary/count cases.
Additional checks pin caller hashes and test raw bits, delay-slot stores, the
829/830 split, selector source and API16(value3) inner-helper behavior.

The [outer-control audit](SHARC_OUTER_CONTROL_2026-09-20.md) now binds A21 tables,
dispatchers and read/write distinction. It also corrects the prior API18 gap:
AA886 is its dedicated sender, through events22/368, with a signed source clamped
to−9..9 by CA4B4. API29's dedicated sender and both UI identities remain open.
Generic extended requests do not establish actual playback state.

Coefficient side paths are now bound in the [matrix audit](SHARC_MATRIX_CONTROL_2026-09-20.md),
which also corrects AmpChain's prior transpose; current403 tests,168 SHARC ranges,43 V850 listings.
Next static work: outermost request/UI mapping;
possible indirect consumers1162..1165; other extended families/error lifecycle; then target
composition, nonzero EQ design if needed, wrapper timing and physical input map.

Reproduce: run `audit_amp_level_control.py` with `--loader`, then
`audit_amp_remaining_blocks.py` as in the [level audit](SHARC_LEVEL_CONTROL_2026-09-20.md).
Source raw V850 LE32/base0/size0x180000 SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
SHARC loader SHA256:
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
