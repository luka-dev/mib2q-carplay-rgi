# Successful mode5 coefficient-consumer schedule

Static/offline only. `amp_b_mode5_schedule.py` closes the consumer scheduling
gap for **one successful131208 invocation with mode5 and512 samples**. It does
not equate this invocation with an entire1024-sample wrapper callback, infer
when callbacks occur, or read the vehicle.

## Result

For retained B01 bank1 with API20 default followed by API16=5, the invocation
contains313 calls to coefficient updater12FAF2:312 addresses inside the829-word
configuration plus the proven lower-bound bypass for object97 (offset−202).
All31 initially interpolated fields are visited **once each**, not once per
channel or once per sample. All tested retained B01 banks1..4 and both API20
pointer variants have the same once-per-invocation coverage of those31 fields.

The full ordered list, caller addresses and parameter offsets are returned by
`mode5_frame_schedule(...)`; no generic all-parameter tick is substituted.

| Order | Consumer | Config offsets / pointers |
|---|---|---|
| 1 | Mode5 third-input gain | −202, skipped below configured base |
| 2 | Two-tail common delay, pair delay/gain/EQ, tail gain | pointer1160; pair316; pointer1158 |
| 3 | Two branch inline gain | 377 |
| 4 | Six-tail pre-gain/common delay, three pair banks and gains |123; pointer1159; pairs145/196/247; pointers1171/1172/1173 |
| 5 | Six-tail final and inline gains | pointer1157;293 |
| 6 | Four-tail common delay, two pair banks, tail and inline gains |32; pairs33/77; pointer1156;122 |
| 7 | Six shared EQ, two shared EQ | count294; count378 |
| 8 | Final eight-channel EQ then gain | count399;420 |

A pair at config baseN calls left delayN, right delayN+1, pre-EQ gainN+2,
then the EQ starting at countN+3. Each section's five coefficient visits are
**count+5,+4,+3,+2,+1**, then the next section. They are not ascending in
storage order. The independently decoded register-loop oracle verifies this
ordering for every pair and finish EQ bank.

Delay12F411 calls12FAF2 at12F454 **before** the zero-delay test and the channel
loops; even a zero-delay bypass visits the coefficient once. Gain132D0B calls
12FAF2 at132D23 before its channel loop. Filter12DBDE updates all coefficients
once per section before processing channels, not six or eight times for shared
EQs. Allpass kernels have no12FAF2 call in the closed caller census.

## API and explicit domain

```python
schedule = mode5_frame_schedule(
    effective_config=current_829_words,
    pointers=all_18_explicit_config_relative_pointers,
    assume_successful_active_path=True,
)
result = advance_mode5_frame(
    publication_state,
    pointers=all_18_explicit_config_relative_pointers,
    assume_successful_active_path=True,
)
```

`advance_mode5_frame` advances a private trial state in the recovered order,
returns per-access before/after values, checks the post-update fixed-filter
domain, then commits coefficient state. This is **not a dynamic audio render**.
Applying the final state to the entire frame would in general be incorrect:
each consumer sees its own update boundary, and delay transitions read both old
and new delay settings. A future dynamic audio implementation must interleave
the returned events with those audio consumers.

Required premises are a complete successful mode5 invocation, positive retained
EQ counts, active/nonreset filter state, sufficient allocations, stable filters,
and no reentrant mode/pointer/count/publication writes during the invocation.
The host validates active numerical tail/filter domains but does not claim to
have observed allocation or lifecycle state. Zero-count filters, shared reset
state2 and any callee failure/early-return prefix are excluded, not silently
treated as a full schedule.

The schedule is built from supplied pointers, so repeated aliases remain repeated
events. The once-per-frame statement for31 entries is conditional on the actual
retained tested pointer maps; arbitrary supplied pointer aliases are not deduced
to have the same count. No live control or history is inferred.

## Independent publication-threshold check

`test_amp_b_publication_oracle.py` executes the decoded12FAF2 leaf path with a
small fail-closed ISA subset. It resolves branch instructions, float32 register
aliases, memory addresses, old operands of parallel clauses and stores; it
does not call the Python publication formula as its oracle.

176 cases cover exact snap equality, both adjacent float32 values around the
threshold, positive/negative and wrong-sign steps, target==current with stale
step, zero-step stalling and the below-base bypass. The float32 reference matches
the decoded path in all cases. This does not claim SHARC40-bit/RECIPS bit-exact
behavior; that existing precision boundary remains explicit.

Six schedule tests and nine publication tests pass:

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p test_amp_b_mode5_schedule.py
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p 'test_amp_b_publication*.py'
```

Source is the previously hash-pinned SHARC loader. The new helper fingerprints
nine bounded code ranges:131208 dispatcher, all three tail handlers, pair
delay/gain/EQ wrapper, EQ wrapper, delay wrapper, scalar gain and eight-channel
finish. Exact instruction pins cover branch-back order, per-section coefficient
address generation and the sole gain/delay update sites. The existing whole
caller census in `audit_amp_b_pointer_aliases.py` bounds the helper families.

No shared spectral model, scenario factory, amplifier default, deployment,
vehicle state or commit is changed by this closure.
