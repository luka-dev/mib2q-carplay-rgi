# First-B wrapper control and transition boundaries — 2026-09-20

Static/offline only. The new reference does not change `AmpChain`, the
steady double-buffer model, production defaults, firmware, or any vehicle.

## Scope closed

[amp_b_wrapper_control.py](../../tools/soundamp-virtual-cabin/amp_b_wrapper_control.py)
adds three separate, explicitly bounded references:

- callback124F56 state/status/bank-clear effects;
- alternate handler124D64 stereo fanout, which is **not mute**;
- successful library12DE3B transition behavior **up to12DF07**, before the
  remaining output-mode-dependent postprocessing/reset path.

Callback invocation is an input event, not a simulated real-time schedule.
Library init/parameter/process return words are explicit inputs, not fabricated
success. No live state or callback deadline is inferred.

## Callback state machine124F56

State, busy and error are words `B95E2`, `B95E3`, `B95E4`. Every represented
invocation writes busy1 at124F61 and returns through busy0 at124F9A.

| Initial state | Condition/result | Ordered effects | Final state/error |
|---|---|---|---|
|0 or any value except1/3|—|clear published4096-word output bank|preserve state/error|
|1|init fails|clear bank; call12DD7D|1 /2|
|1|init succeeds, parameter fails|clear bank;12DD7D;131A65(command0800AFFE)|1 /1|
|1|both calls succeed|same calls|3 /0|
|3|BC9EC equals1|clear bank; **do not call library**|3 /preserve error|
|3|BC9EC is anything else, process fails|call12DE3B; clear bank|3 /3|
|3|BC9EC is anything else, process succeeds|call12DE3B; retain library result|3 /0|

“Fails” here means the exact return word is `0x4000` or `0x8000`. The
instructions use BTGL followed by zero comparison, **not tests for error bits**.
For example `0xC000`, `0x4001`, and `0x8001` do not take those branches. This
documents the instruction predicate; it does not assign semantic success to
arbitrary undocumented statuses in other library contexts.

Similarly, `BC9EC==1` inhibits. `BC9EC==3` does not inhibit despite its low
bit being set. The successful init path writes state3 in the delayed jump slots
at125037/125039; omitting those would leave a wrong state1 model.

Evidence:124F5D..124F79 state/inhibit;124F84..124FBB process/error clearing;
124FC9..125039 initialization and parameter result handling. Input-bank audio
is not cleared by these callback-level branches. Library-level transition
clearing is a different mechanism below.

### Clearing is not instantaneous graph mute

The callback clears4096 words at the **currently published BC9EB** bank only.
It does not clear both output banks, undo the process-handler copy that already
happened, or select the alternate graph handler.

An integration regression with the existing double-buffer reference supplies
explicit synchronous inhibited callbacks and initial output banks0.3/0.7.
The graph still receives512 frames of0.3, then512 of0.7, then zero, because
each callback occurs after the corresponding bank was copied out. This is a
synthetic ordering proof, not a claim about a real vehicle's stale audio.

## Alternate graph handler124D64

Four loop iterations copy input lanes0/1 into output pairs0/1,2/3,4/5,6/7.
Input lanes2..4 are not used. With the recovered graph binding this is:

```text
graph24/23 → graph20/19,18/17,34/35,40/41
```

There is no double-bank callback delay in this copy path. Odd sample counts
use the scalar delayed-slot copy. This does not prove the handler is selected
during playback. Handler selection and callback inhibit must remain separate.

## Successful library transition, not an invented full reset

`SuccessfulLibraryTransition` requires readiness word **exactly1**, explicit
transition/counter and an explicit promise that all supplied processing calls
succeed with no intervening parameter/state writes. Caller supplies the actual
stateful pre-conversion→algorithm→post-conversion operation; the reference
does not substitute a fake algorithm or erase its history.

For an initial snapshot `B909B==1`,12DE98/12DF90 select:

1. Increment B89BF as a32-bit word.
2. Clear five input lanes using12DD16.
3. Still run conversion/algorithm/conversion on those zero inputs.
4. Clear eight output lanes at12DF6B.
5. If the incremented counter **equals10**, call1319FE to clear B909B and
   clear B89BF in the delayed jump slot12DF8C.
6. Continue at12DF07.

Other transition words, including3, do not select the exact1 transition branch.
The counter comparison is equality, not≥10: a supplied counter10 increments
to11 and does not immediately reset. A normal sequence from0 has ten cleared
boundary outputs; the eleventh successful call is no longer transition-muted.
The algorithm has nevertheless advanced on each of the ten zero-input calls.

**The returned buffer is named `audio_at_12DF07`, not final callback audio.**
After this boundary, object+1176 can select130E78 and subsequent12B694/
130D3D output-mode/reset work. These effects are not silently moved before
the clear or folded into a made-up reset. The helper reports no inferred
algorithm-state reset and does not claim to emulate that remaining tail.

If the algorithm raises or returns invalid audio, the successful-path contract
was violated. Earlier counter/input-clear effects are not rolled back. Such
failures require a separate instruction-level failure-path model.

## Evidence and tests

[Report](../../assets/soundamp/wrapper-control-20260920/report.json) includes
raw instruction-range hashes, exact instruction pins, callback branch cases,
and an11-call synthetic transition trace.

The evidence reader requires derived loader SHA256
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Original CodeFlash SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Instruction addresses are SHARC shortwords, not source-file offsets.

Eleven new tests cover literal XOR/BTGL predicates, state/error preservation,
delayed-slot initialization state3, fail-closed external statuses, stale bank
ordering, independent scalar stereo fanout, ten still-executed transition
calls, exact counter comparison, uint32 wrap and loader pinning. All20
`test_amp_b_wrapper*.py` tests passed.

```sh
python3 tools/soundamp-virtual-cabin/amp_b_wrapper_control.py \
  --output assets/soundamp/wrapper-control-20260920/report.json
python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p 'test_amp_b_wrapper*.py'
```

## Still outside the reference

Process-handler event registration/raising and descriptor+5/+8 control delivery;
late/missing/reentrant callbacks and BC9EA/BC9EB publication races; actual active
handler/readiness/inhibit/state; pending-control1326C7 effects; initialization
allocation failure details; library conversion/algorithm failure recovery;
post12DF07 output-mode/reset composition. Therefore the established1024-sample
steady wrapper delay remains **conditional**, not a universal transient model.
