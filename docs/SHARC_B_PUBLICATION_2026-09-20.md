# First-B full target replay and explicit publication lifecycle

Static/offline only. `amp_b_publication.py` connects the existing verified
all-nine-control replay to a **whole829-word target and explicit effective
configuration state**. It also models `12FBA5` publication and `12FAF2`
coefficient advancement. It does not change the retained music factory or
amplifier defaults, and does not choose a vehicle control state.

## Why a stored bank is not the effective replay result

Successful API17 helper132346 validates and copies the selected829-word bank
into the target buffer, rebases descriptor44..58 bases, preserves their135
additive contribution slots, replays19→18→22→21→23→24→25→27→29, and publishes.
The preceding scalar helper already covers all15 descriptor targets and the
four EQ rebuilds. The new composition overlays those actual writes on the
selected retained bank; every other target word is preserved verbatim.

This is a successful bank-copy/rebase/replay slice, not a general replay after
arbitrary target-RAM changes. Current829 words, prior829 steps, all135 prior
contributions, nine effective controls, and shadow16/20 selectors are explicit
inputs. A source bank and zero controls alone do not supply those missing states.

`replay_retained_bank_publication(...)` returns `effective_config`,
`target_config`, `steps`, `duration`, `all_current_words_equal_target`, updated
contributions and metadata. Caller must explicitly assert the successful
validated-bank path. `publication_flag` is a **raw uint32 helper argument**,
not a guessed API/control value. Choosing1 requires an explicit scenario or
proof of its actual caller argument.

## Publication is not a uniform ramp

`12FBA5` reads target word2, truncates without+.5 and maps0 to1 for the duration
at B903C. It tests the **entire publication argument for equality to1** with
`BTGL R8 BY0`/SZ. It is not an odd/even bit test.

| Condition | Actual successful publication action |
|---|---|
| raw flag==1 | Copy all829 target words to effective; leave prior steps unchanged |
| flag!=1 and prior step word==float1 | Copy this target word directly; keep step1 |
| flag!=1, prior step!=1, target==current | Set this step0 |
| flag!=1, prior step!=1, target!=current | Set step=(target−current)/duration; do not advance current yet |

Both paths publish the target pointer at B903B. Reference state retains a copied
target vector instead of inventing memory aliases or later target mutation.

Initialization12FC3C first writes step1 to all829 words, then12FB40 clears31
entries:

```
35,79,122,147,198,249,293,316,317,377,420,
295..304,379..388
```

These are seven gain/pre-EQ scalars, two delays, two branch/final gains and20
EQ coefficient words (the exact offsets, not a guessed physical-speaker map,
are authoritative). Spatial coefficients and most other configuration words
therefore copy immediately on an ordinary publication; they do not all ramp.

The step array is not a separate bool mask. If a computed step happens to equal
float1, the next publication sees that value as the immediate-copy sentinel.
Tests retain this literal behavior rather than replacing it with fixed flags.

## Coefficients advance only at explicit accesses

`12FAF2` rejects addresses below the configured effective base. For an in-range
address, it returns immediately if current==target (without clearing a stale
step). Otherwise:

```
if abs(target-current) <= abs(step): current=target; step=0
else: current=current+step
```

This is not a global timer. Different coefficient consumers may call it with
different frequencies, repeated addresses, or not at all. A zero step with an
unequal current/target can remain stalled. The host reference therefore exposes
`advance_offsets(explicit_sequence)`, retaining ordering and duplicates. It
does not generate a whole-frame access schedule or claim that N audio frames
settle every coefficient. The lower-bound bypass includes object97, whose
config-relative offset is−202. Host accesses are restricted to known object
offsets0..1127 (config-relative−299..828); the firmware helper itself has no
upper bound, so unknown higher memory is deliberately rejected.

The direct-copy flag1 is the only built-in whole-vector completion route.
For ordinary publication, callers must supply actual consumer-call sequences
or separately assert a settled snapshot; the reference does neither silently.

## Evidence and verification

Source: retained hash-pinned SHARC SPI loader
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
SHARC code PCs are16-bit shortword addresses; configuration offsets are32-bit
data words. Hash ranges:

- `12FAF2..12FC78`: smoothing, initial mask, publication and initialization.
- `132168..1323DE`: complete replay and successful bank replacement.

The evidence helper also checks exact instruction pins and derives the31 mask
entries from decoded direct-store instructions, not a hand-entered offset list.
Existing scalar replay math retains its verified zero-gain EQ path and optional
separately audited nonzero mathematical designer. Publication arithmetic uses
float32 host operations, not a claim of exact SHARC40-bit/RECIPS execution.

Eight focused tests pass. They cover all829-word copy, flag3≠flag1, nonuniform
initial mask, opposite-sign steps, duration truncation, repeated/absent calls,
snap threshold, stale-step equality, stalled zero-step state, step1 sentinel
collision, republishing from current rather than prior target, whole retained
bank composition, untouched raw fields, preserved prior slots, and fail-closed
validation without partial state mutation.

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p test_amp_b_publication.py
```

## What remains outside this closure

No live bank/effective controls/pointers/history, callback timing, command FIFO,
defer behavior, partial errors or rollback, mode-pointer writes, common-header
updates, target-buffer alias writes, or audio-history transition is inferred.
The actual API17 helper stores effective selected-bank state after replay even
on a replay error; this successful-path helper does not pretend to emulate that
failure path. A whole effective numerical vector is now obtainable with explicit
publication inputs, but cannot identify the running car's state from static
images alone.
