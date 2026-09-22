# J525 common-EQ level source — static-only follow-up

The source of `gp−77F8` is now traced to a **smoothed source-level control**,
not an audio-level measurement or the Bass/Subwoofer setting. The shape is
consistent with loudness compensation; that descriptive interpretation is not
a recovered UI/property name. This result does not identify the car's actual
bank or establish the cause of its excessive bass.

Only local firmware/disassembly and offline tests were used. No vehicle I/O,
diagnostics, RAM readback, firmware patch, deployment or commit.

## Recovered chain

```text
normal source request + source trim       alternate override request
    C68E8: normal target                      C3790: lane0 override
                  \                         /
                   gp−15718 (4 signed bytes)
                              |
                  AEFC0: copy and mute/active gates
                              |
                   gp−30720 (4 target bytes)
                              |
                  AF130 setup → AF824 ramp step
                              |
                   gp−30712 (4 signed Q24 words)
                              |
                     lane0 = gp−77F8
                              |
           B0066: truncate /2^24 → threshold bank candidate
                              |
            commit gate + enable source → event17 → A8B26
                              |
              C4AB7 common EQ selector for all8 lanes
```

The arrows describe recovered control/data dependencies. Event delivery and
external state are not inferred. Q24 here means signed level multiplied by
2^24, not a normalized audio sample.

## Normal request and trim

`C5B64` accepts a two-byte payload: selector and level0..126. The selector
lookup at CodeFlash `96E2C` contains `21,22,23,24,26,2A,32` hex. Subtracting
33 maps these to source indices `0,1,2,3,5,9,17`. Selector0 updates all seven
listed sources, **not all18**. `C5CDA` stores the raw level in gp−15425+source;
`C5D20` calls `C68E4`. No external service identifier or MMI-percent mapping is
asserted by this inner-handler binding.

`C68E8` chooses the source for control lane0. If active[0]==A5, source0 wins.
Otherwise it scans17 down to4 and keeps the last active source (the lowest
active index4..17). If none is active, the existing mapping is retained.
Lanes1..3 use their existing mapping; this is **not** physical PCM pin mapping.

For each lane, `C6928..C69C2` resolves the mapped source and computes:

```text
if active[source] == A5 and inhibit[source] == 0
   and raw_level[source] > 0 and signed_trim[source] > -13:
    normal_target = min(26, raw_level[source] - 100 + signed_trim[source])
else:
    normal_target = -100
```

Memory bases, relative to gp:

| Value | Offset |
|---|---:|
| Lane→source mapping | −15764 |
| Source active flags | −15465 |
| Source inhibit | −15056 |
| Raw source levels | −15425 |
| Signed source trims | −14992 |
| Cached normal targets | −14967 |
| Selected targets | −15718 |

There is no lower clamp at this calculation: raw1/trim−12 yields−111.
The later gate replaces targets below−100 by−100. Raw0 is a mute-domain
value here; it is not the user's neutral Bass0 control.

## Override and gates

When gp−17779==A5, `C37B2/C37B6` takes lane0 from signed gp−17778 and sets
lanes1..3 to−100 (`C387A..C38AE`). In this mode `C69C6` still updates the
cached normal target but skips publishing it to gp−15718. Mode exit copies the
normal targets (`C368E..C36B2`) and clears mute bit0.

The alternate level handler `A21D4` accepts−100..26 for operation3; operation0
sets−30. These are code-path labels, not proven user-visible mode names.

`AEFC0` copies selected targets to gp−30720, then applies:

- Both gp−30750 and gp−30751 must be5A, and gp−17777 bits0/1 must be clear;
  otherwise all four targets become−100.
- Any gp−17777 bit2..5 forces lane0 to−100.
- Each target below−100, or whose mapped active flag is notA5, becomes−100.

The periodic caller `AEF20` orders target gating, ramp setup, ramp update,
post-level offset, mute lifecycle, control publication, then EQ selection.
The ramp therefore updates **before** the threshold lookup in this caller.
Later mute-lifecycle writes can affect the next iteration; the pure helper
does not claim to emulate that complete scheduler.

## Ramp and selector enable/commit

`AEE20/AEE24/AEE5C` initializes four words to−100×2^24. `AF9CE`, `AFA0C`,
`AFA92`, `AFAB6` update them through a quantized ramp, linear ramp, target
clamp or stationary target copy respectively. Only stationary semantics and
supplied current-word selection are modelled here; no complete timing/ramp
emulator was added.

`B0066` reads lane0 **before** the separate gain-publication offset:
`AFB14` converts these words to integer levels, adds gp−24028 and clamps the
separate result to−100..26 at gp−30765. That addition does not directly change
the word used to select C4AB7.

Selector enable is gp−17772 in override modeA5, otherwise gp−15406. Startup
sets normal enable and cached enable toA5 (`B675C`, `B6946`, `B694A`), override
mode and its enable to5A (`B6692`, `B69FE`, `B6A1C`), cached index to0 (`B6A96`).
This establishes initialization intent, not a delivered steady playback bank.

Normal enable handler `D455A` accepts onlyA5/5A. Other payload bytes leave it
unchanged. Override handler `A254C` operation0 setsA5; operation3 selectsA5/5A
from payload byte23 bit0.

The commit gate gp−30788 (gp−7844) is initializedA5 at `AEEEC`. `AF16A` can
set5A during specific gp−17777 transitions. `AFAD8` restoresA5 when lane0 is
stationary and gp−17777==0. `B00C0` updates cached index/enable and queues17
only when the gate isA5 and either cached value changed. Otherwise the old
cached state remains. `A8B26` sends that index when cached enable==A5, else0.
Disabled is **bank0, not flat bank7**; enqueue is not proof of DSP application.

## Conditional settled examples

Assume valid normal request, active source, trim0, no inhibit/mute, enabled
EQ, open commit gate, settled ramp and successful event delivery:

| Raw protocol level (not %) | Control level | EQ bank | 55Hz design gain |
|---:|---:|---:|---:|
| 50 | −50 | 13 | +14dB |
| 70 | −30 | 10 | +6dB |
| 80 | −20 | 8 | +2dB |
| 85 | −15 | 7 | 0dB |
| 100 | 0 | 4 | 0dB |

These are EQ gains, **not total amplifier gains**: volume attenuation and
other stages must also be included for absolute-level comparisons. A muted
raw0 can yield candidate bank15 (+20.5dB EQ), which does not imply loud playback.
The earlier fixed-PCM bank10-versus7 music report remains a shape-sensitivity
experiment, not a consistent simulation of turning the vehicle volume knob.

## Source-trim packet writers

Two inner handlers write signed trims at gp−14992+source (initialized to0
at B68B8..B68CE). Both use the seven source IDs above; these are not PCM pins.

| Handler | Record | Selector0 length | Single/FF length |
|---|---|---:|---:|
| C7870..C7A8C | signed trim−13..13 | 9 | 3 |
| C53A0..C571C | trim−32..32, two other signed fields−9..9 | 23 | 5 |

Byte0 is the selector, byte1 is opaque in these handlers, records start at
byte2. Selector0 carries seven independent records; FF repeats one record
for all seven sources. A listed source ID updates only that source. Unknown
IDs with valid length return without updating. Every requested field is
validated before stores. Raw byte **0x80 means leave unchanged**, independently
for each field; it is not a request for−128 attenuation.

The composite fields write gp−14992, gp−14963 and gp−14945 plus source index;
their UI names are not established here. In normal mode the latter two also
copy to gp−15739/gp−15758. C7A70/C56F8 call C68E4, then schedule1/11;
the composite handler also queues49. A stored trim≤−13 is subsequently
treated as **mute** by C698C, not merely a negative gain.

## Override mode entry and exit

Inner dispatcher A1FB4..A21D0 decodes function ID from big-endian message
bytes19/20 and operation from byte21. ID375 (0x177) controls gp−17779:

- The guard `gp−18912 & 0xEF` must be zero; otherwise error36.
- Operation3 requires length1 (otherwise error19): session byte gp−17867=C0,
  mode=A5, guard bit4 set. Schedule9/28 only if the mode changed.
- Operation0 acts only when session byte=C0: session=40, mode=5A, clear
  bit4, set deadline gp−18284 to tick gp−24268+5000 (32-bit wrap).
  Schedule9/28 only on a mode change. No local length check in this branch.
- Other operations yield error49. Related inner IDs379 (level) and1063
  (EQ enable) require modeA5.

These are **inner dispatcher IDs, not ready-to-send diagnostic requests**.
Outer transport, authorization, UI naming and indirect writers remain open.
Operation0 setting the alternate level to−30 is a handler action, not a
claim that the vehicle starts or plays at−30.

## First-B level parameter: target versus smoothed level

Event52 dispatches to V850 AA568, which calls AC648 with:

```text
API25 = float(max(0, signed_byte(gp−15718) + 100))
```

This reads the **selected unsmoothed target**, not the gated target at
gp−30720 and not the current ramp word at gp−77F8 used by common EQ.
At a level transition, these two consumers must not be forced to share one
instantaneous value. Event52 is queued at C69EC; this does not prove delivery
or that every alternate-mode change queues it.

AC648 writes ID/value to C4AD9+8/+9 through descriptor C4AE1; callback124F35
calls SHARC131A65. API25's validator is131F9A, pending word B9127, applier
13262D (jump tables B913D/B914B, index9). The finite accepted range is−200..200;
normal valid target−100..26 generates0..126. No extra silent upper clamp is
introduced by the reference.

The pending applier always writes shadow `object+1151`. If `object+1178` is
nonzero it skips the effective write and helper. Otherwise it writes
`object+1137` and calls12CE71 with `value−object[1090]`.

12CE71 calls curve kernel12B00F three times, using descriptors from
`object+46/+50/+51`, slot6, limits0..5, and paired records at offsets
`[773,782]`, `[776,785]`, `[779,788]` from `DM(object+1180)`.
The success path subsequently calls12FBA5. This is a **curve coordinate,
not audio gain in dB**. Parameter identities, initial curve loading and baseline
provenance are now bound below; full lifecycle/timing remains open.
The explicit helper projects finite accepted values only: it does not emulate
API readiness, queue lifecycle, or bit-exact SHARC floating-point arithmetic.

## API25 curves bound to linear branch gains

Normal init130EF3 copies829 words from `source+245` both to `object+299`
(effective configuration) and B89C0 (target configuration). Source is the
pointer at B95E5, loader value C3C6A. 132A9B checks the configuration checksum
word828 before copying. Object1179 retains source+245, object1180 the target
pointer. Therefore the baseline read at object1090 is configuration word791.
These are image/init-path bindings, not a claim of current RAM contents.

12BC52/12AF9A allocate12-word descriptors: word1 points to a target field,
word2 saves its initial base value, words3..11 initialize nine additive slots
to zero. 12CE71 changes **slot6**, not the other eight contributions.

| Descriptor in object | Target config offset | Effective object offset | Audio consumer | Base in loader and B01 |
|---:|---:|---:|---|---:|
| 46 | 122 | 421 | four-channel branch gain, call1314DA | 1.0 |
| 50 | 293 | 592 | six-channel branch gain, call131490 | 1.0 |
| 51 | 377 | 676 | two-channel branch gain, call13156D | 1.600000024 |

All three call132D0B, which updates that scalar through12FAF2 and multiplies
the branch's samples via12FCE1. These are **linear amplitude factors**, not
dB or physical speaker labels. They correspond to `four_gain`, `six_gain`,
`two_gain` in BOutputFinishReference, not its final eight-channel gain.
The base values are not final gains: init invokes other control-slot setters
too, and later requests can alter their contributions.

Both the loader and V850 B01 contain **zero for all18 words at config773..790**
(six x/y/shape triples), and zero baseline791. B01 byte offsets are
120FF4..121038 for the curves,12103C for baseline; loader DM starts C4064
for curve773. The hash-bound export records every word/address separately.
For these tables, API25 contributes zero for any finite accepted−200..200.
Thus this specific mechanism does not supply level-dependent extra gain under
these table assumptions. It does **not** prove B=0, unity gain, inactive B,
or unchanged parameter RAM in the car, and does not settle the bass complaint.

### Curve and publication math

12B00F selects bracketing records on the input's side of zero, with default
endpoints−200/0 or0/200. Records are processed in order, including ties.
Input0 forces contribution0. The endpoint away from zero supplies the shape:
`trunc(shape+0.5)` selects linear0, squared1 or square-root2 interpolation;
other shapes take an error path. A missing outer bracket inherits the near
endpoint's y, not its x. Distance below float32(0x3727C5AC) uses near y;
division includes float32(0x0DA24260) in its denominator.

The descriptor update is `target=clamp(base+sum(nine slots),0,5)` after
replacing slot6. Zero curves clear that slot, not the entire gain. The new
`amp_b_control.py` implements this finite-domain reference with explicit state.
Float64 math stands in for SHARC arithmetic/reciprocal/sqrt rounding.

12FBA5 copies all829 target words immediately when its selector is **exactly1**;
other values prepare per-field increments, except fields whose mask equals1
are copied immediately. Transition count is configuration word2, converted to
unsigned integer and changed from0 to1. Here loader word2 is200, B01 word2 is1:
the images differ in transition configuration even though these six curves match.
132D0B calls the scalar updater once per branch invocation, before sample
multiplication. This is not proof of a duration in milliseconds or samples;
wrapper scheduling still needs binding.

## All nine branch-gain slots

The remaining eight contributions are now bound for descriptors46/50/51.
This closes their **curve-call routing**, not every effect of the enclosing
helpers or all possible later/indirect writers. Each helper also has an API
validator and a pending applier; all shadow writes precede the object1178 defer
gate. API25 subtracts baseline1090; the others pass their value unchanged.
Finite accepted range is−200..200 except API27, which accepts−200..400.

| Slot | API | Helper | Four-channel curve offsets | Six-channel | Two-channel |
|---:|---:|---|---|---|---|
| 0 | 19 | 12BF21 | 515,560 | 527,572 | 530,575 |
| 1 | 18 | 12C4EF | 605,641 | 617,653 | untouched |
| 2 | 22 | 12C9B7 | 671,689 | 680,698 | 683,701 |
| 3 | 21 | 12CB59 | 707,722 | 713,728 | 716,731 |
| 4 | 23 | 12CCB9 | 737,746 | 740,749 | 743,752 |
| 5 | 24 | 12CD95 | 755,764 | 758,767 | 761,770 |
| 6 | 25 | 12CE71 | 773,782 | 776,785 | 779,788 |
| 7 | 27 | 12CF4D | 792,801 | 795,804 | 798,807 |
| 8 | 29 | 12D029 | 810,819 | 813,822 | 816,825 |

Offsets are in the829-word target configuration, each pointing to an x/y/shape
triple. Slot1 of descriptor51 is not written by12C4EF: retain its prior value,
or zero only with an explicitly assumed fresh descriptor initialization.
Slot0 branches on object1142/1146 for **other** descriptors' record selection;
the three rows above are invariant across those branches.

All these branch curves are zero in the loader, but **not in B01**:

| API | Branch | B01 points (x,y), all shape0 |
|---:|---|---|
| 19 | two | (−50,−0.3), (100,+0.2) |
| 18 | four | (−9,−1), (0,0) |
| 18 | six | (−9,−1), (9,+1.5) |
| 22 | six | (−9,−0.5), (9,−0.5) |
| 22 | two | (−9,−0.4), (9,−0.4) |
| 27 | six | (120,0), (180,+0.7) |

Other pairs in this table are zero. These y values are **additive linear-gain
contributions, not dB**. With all other helper coordinates and prior slots
explicitly zero, API18=+9 produces six-gain2.5; API27=180 produces1.7;
both together produce3.2. API19 values−100/−50/0/100 produce two-gain
approximately1.3/1.3/1.6/1.8. These are conditional branch-target examples,
not measured amplifier/speaker gains, and not a proven explanation of bass.

### Bound V850 writers and remaining naming boundary

- Event47, AA39A: API19 uses table6EA68 indexed by u8(gp−16783), with the
  four table values−100,−50,0,100. Index meaning and upstream writer remain open.
- Event49, AA4CE: API21 takes signed gp−15758+u8(gp−15740); API22 takes
  signed gp−15739+that index. These link to the composite source-adjustment
  fields recovered earlier; user-visible labels are not asserted.
- Events50/51, AA522/AA546: API23/24 take signed gp−15721/gp−15719.
- Event52 retains API25's max(0,selected target+100).
- Event53, AA59A: API27 is float(min(400,u32(gp−17832)//100)); the code divides
  twice by10. Units/source of that word are not yet established here.
- Follow-up [outer audit](SHARC_OUTER_CONTROL_2026-09-20.md) identifies API18's
  dedicated writer AA886(events22/368), float(s8(gp−15840)); CA4B4 accepts
  signed−12..12 and clamps−9..9. Zero maps0, UI name remains unbound.
  API29 still has no dedicated writer bound. Generic proxy
  A782E reads arbitrary ID from gp−15936 and value from gp−15932, then calls
  AC648. D40C8/D4130 stage two request words there and queue369. This is an
  route now bound to outer A21 tables/dispatchers, not complete ingress/auth
  or a ready-to-send command. Same API18 source byte also affects coefficient
  blocks in AA644/A903C, still pending audit. Total392 tests,41 V850 listings.

`branch_gain_targets` now applies all nine supplied helper coordinates to
explicit prior slot banks using either extracted loader or B01 tables. It
returns gain targets only: no queue/defer/smoothing/audio or actual preset is
silently inferred. In particular API25 input is an already adjusted helper
coordinate, not an unadjusted API packet value. The curve reference domain
extends to400 for API27, verified against decoded execution at201/250/399/400.

The enclosing API18/19 helpers also touch other descriptors. Their full DSP
effect is **not** represented by changing only these three branch gains.
The following extension binds all their scalar destinations and EQ rebuilds;
it still does not compose a live B preset or emulate changing-delay transitions.

## API18/19 full scalar destinations and EQ rebuilds

The numeric examples below refer to **B calibration bank1**, not the matrix
5.1 bank1. All four B calibration banks and API17 replacement/replay are now
bound in the [B configuration audit](SHARC_B_CONFIG_2026-09-20.md).

`audit_amp_b_parameter_controls.py` pins nine instruction spans, extracts both
image tables, and exports `gain_controls.parameter_controls`. The descriptor
constructor `12BC52` binds the following target config words (effective object
offset = config offset +299). API19 updates slot0 of all15 descriptors; API18
updates slot1 of12, leaving51/52/53 untouched:

| Descriptors | Target config words | Clamp |
|---|---|---|
| 44..51 | 35,79,122,147,198,249,293,377 | 0..5 |
| 52,53 | 316,317 | 0..288 |
| 54 | 420 | 0..5 |
| 55..58 | 307,312,391,396 | −100..20 |

Additional nonzero B01 curves, beyond previously covered46/50/51:

- API19:44 and47 have (−50,−0.3),(100,+0.2);48 has
  (−50,−0.9),(100,+0.2). Their bases are1,0.8912509084,1.7782794237.
- API18:47 has (0,0),(9,−0.5).
- API19:52/53 each have base96 and curve (−50,+66),(100,−77).
  All listed curve shapes are0 (linear). Values are additive linear parameter
  contributions, **not dB or MMI percentages**. At coordinate0 the curve
  contribution is forcibly0, not a fitted intercept.

For52/53, `12BF91..12BFB5` first tests unsigned-truncated **shadow API16**
(object1142)==5 and selects pairs494/500,497/503. Otherwise `12C4B1` tests
**shadow API20** (object1146)==3 and selects427/433,430/436; default pairs are
533/578,536/581. The delayed jumps at12C4D0/12C4E7 include the final pointer
stores. All three record variants match each other in the supplied B01 image;
all are zero in loader. Matching image values do not erase the selector branch.

These two parameters are separate channel **delays**: config316/317 becomes
object615/616, passed at133015 to12EB9B, then at12EBC9/12EC04 to12F411.
That consumer rounds `value+0.5` to a sample count, reading the old value before
12FAF2 smoothing and the new value after it. With other slots explicitly0,
B01 API19=−50/0/+100 gives settled individual delays162/96/19 samples.
These are inside the two-channel tail, not total graph/acoustic latency.

The four EQ rebuilds call12AB1F with sample-rate literal48000 and five-word
design records. The third design word is the updated gain; the fourth is the
rounded type selector. The successful callers copy these records by value:

| Descriptor | Design config | Output coefficients | API19 / API18 call | Audio consumer |
|---|---:|---:|---|---|
| 55 | 305..309 | 295..299 | 12C2EF /12C7E8 | six-channel EQ,13138C |
| 56 | 310..314 | 300..304 | 12C376 /12C873 | six-channel EQ,13138C |
| 57 | 389..393 | 379..383 | 12C3FE /12C8FF | two-channel EQ,1313E6 |
| 58 | 394..398 | 384..388 | 12C481 /12C986 | two-channel EQ,1313E6 |

Consumer parameter banks start at object593/677 (config294/378); their stage
count is2 and coefficients start one word later. `12AB1F` first calls12AB0A
to write identity `[1,0,0,0,0]`. Gain0 in the supported type0/1/2 branches exits
without replacing it. Both images contain design `[20,100,0,0,1]`, identity
coefficients, zero bases55..58, and zero API18/19 curves for all four sections.
Thus **with the other contribution slots zero and these image tables retained**,
these rebuilds add no EQ boost at any accepted API18/19 coordinate. This does
not establish the current car's EQ state or rule out the upstream common EQ.

`api18_19_parameter_targets` requires all15 prior nine-slot banks and explicit
already-converted shadow selectors. It returns scalar targets, config updates
and four design records; untouched banks are preserved. `zero_gain_eq_coefficients`
implements only the verified identity exit and rejects nonzero gain, rather
than pretending to implement the rest of12AB1F. Prior nonzero contributions,
later calibration replacement, nonzero designer math, publication/defer and
changing-delay processing remain explicit boundaries. AmpChain defaults unchanged.

## Artifacts and validation

- `tools/audit_amp_level_control.py`: hash-validating extraction,25 bounded
  r2 V850 listings with no invalid/truncated instructions; `level_control.json`
  and, with `--loader`, `b_level_control.json`.
- `audit_amp_input_path.py`: same provenance embedded in both input profiles.
- `amp_input_control.py`: explicit normal/override targets, gates and cached
  selector-step, trim packet, override mode and B-level helpers. No automatic
  replacement of AmpChain's fixed bank or first-B coefficients.
- A bounded interpreter executes decoded `C6928..C69DE` on196 cases and agrees
  with the control reference, including alternate mode and signed trim limits.
- `amp_b_control.py` adds signed-side curves and nine-slot aggregation. A second
  bounded interpreter executes decoded SHARC moves/arithmetic/branches/loops
  on123 synthetic/edge cases and260 actual-B01 curve cases, respecting old
  parallel operands and branch delay slots.
  Divide/sqrt callees use host math, not bit-exact library emulation.
- `audit_amp_b_gain_controls.py` recovers stack-built curve argument pointers,
  verifies slot/API/validator/applier links, and exports both image tables.
- API18/19 add434 extracted-curve comparisons against the decoded kernel,
  selector-priority/untouched-state/clamp tests and zero-gain EQ checks.
- 383 tests pass:110 extraction +273 model. All166 bounded SHARC ranges UNKNOWN0.
  Tests pin API25 jump tables/instructions and cover sentinel, atomic validation,
  override gates, target/ramp separation and deferred B-level application.

Reproduce the new audit:

```sh
python3 tools/audit_amp_level_control.py \
  /Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin \
  --output-dir assets/soundamp/remaining-blocks-20260919 \
  --loader assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin
```

Image: raw V850,32-bit,little-endian,base0,size0x180000. Byte addresses above
are file offsets. SHA256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Every new bounded range has its own byte hash in `level_control.json`.

API17 replacement and all four calibration banks are now bound separately;
replay uses raw API25, unlike single-update baseline subtraction. API16/20 pointer
writes into effective config are now bound too: shared pointers depend on apply
order and AUTO20 resolves before defer. See the [configuration audit](SHARC_B_CONFIG_2026-09-20.md).
Pointer1161 now binds post spectral crossfeed, preserving identical mono at that
stage. Extended FIFO1326C7 canonical indexed writes/source-store and mode-call
arguments are now bound separately as effect plans. Next, still **static only**:
extended-command senders, consumers1162..1165,
and upstream/API18/29 request semantics; extend12AB1F beyond its zero-gain exit only if
needed for nonzero scenario inputs. Then wrapper scheduling/latency and physical
input graph, and composition of the newly bound targets with existing B stages.
External values and request order that are not
fixed by firmware remain conditional branches, not a readback prerequisite.
