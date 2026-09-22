# Nested allpass active zero-delay record — 2026-09-20

Bounded static evidence for12D10E. No vehicle state, allocation success or
changing-delay behavior is inferred. No production settings changed.

The retained B01 bank4 record at the second-left nested parameter pointer1168
is `[enable1,delay1=0,gain1=0,delay2=96,gain2≈−0.3]`. This is a legitimate
stored-record case, not a reason to silently disable its active filter.

After the enable check12D13F, the routine has **no delay-zero bypass**:

- 12D157/159 compares requested first delay with allocated capacity. Zero passes
  this numeric comparison, but that alone does not prove the pointer is valid.
- 12D163..168 resets an out-of-range current cursor only if requestedN<cursor.
  A settled initial cursor0 is retained for requestedN0.
- 12D182..18B reads and writes the selected ring cell.12D17C increments its
  cursor to1;12D191/195 resets it when requestedN≤incremented cursor.
- The outer ring has the corresponding increment/reset at12D19A..1A0.

Thus with **valid storage containing at least one word and zero initial
cursors**, requestedN0 repeatedly reads the previous sample in cell0 and
writes the current sample there. Its effective ring length is1, not an
instantaneous zero-delay bypass. A fixed zero-state reference can use
`max(1,requestedN)` for this nested kernel under that explicit capacity
assumption. It must retain the original requested value in scenario metadata.

Implemented with explicit `allow_zero_delay_cells=True` in `NestedAllpass`;
the default constructor still rejects zero. The effective-tail binder enables
this only under its required valid-storage/zero-history assumptions and keeps
the original requested records and config/pointer hashes in metadata. An
additional test checks the opt-in against independent transfer polynomials.

This does not authorize the same change to an unrelated single-allpass helper
without inspecting its own instructions. It also does not cover arbitrary
current cursors, delay changes, allocation failures or parameter errors. For
example cursor==newN can survive the initial strict comparison; that transient
is not equivalent to constructing a new settled ring of lengthN.

For the retained example, gain1=0 is still not inner bypass: ring1 storesu,
while ring2 receives the previousu. Combined feedback delay is **97 samples**.
For exact synthetic gain2=−0.3, an impulse produces−0.3 at0,0.91 at97,
0.273 at194 and0.0819 at291, with zeros between. A96-sample interpretation
would therefore be wrong.

Two new independent tests execute the requested-N cursor comparisons directly
and compare with an explicit one-word numerical reference, covering either
or both zero delay requests and the retained-record-style impulse:

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p test_amp_b_nested_zero_evidence.py
```

Evidence source: SHA-pinned loader
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`,
existing listing `remaining-blocks-20260919/first_b_nested_allpass.asm`.
Addresses above are SHARC shortwords. Tests validate reconstructed numerical
semantics, not hardware execution or SHARC floating-point rounding.
