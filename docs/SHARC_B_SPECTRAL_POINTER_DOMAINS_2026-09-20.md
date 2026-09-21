# Spectral pointer domains: three bounded exclusions

Read-only static review, 2026-09-20. No source/runtime/artifact changes were made during the hash-pinned musical run. This document adds a bounded provenance argument for `12ADC6`, `12D55E`, and `12D5F7`; it is not a whole-image alias analysis or a dead-field claim for B+1162..1165.

## Result

All three reads can be excluded from B+1162..1165 under successful fresh construction, retained unmutated source parameters, fixed mode5 call flow, intact descriptors/workspace pointers, and normal nonwrapping allocator/DAG state.

| Read | Proven domain | Why disjoint from parent B object |
| --- | --- | --- |
| `12D5F7` | freshly allocated two-word history-pointer array, indices0 and1 | separate kind0x2000 allocation after B object |
| `12D55E` | same two-word pointer array during constructor reset, indices0 and1 | constructor passes the descriptor it just allocated |
| `12ADC6` | workspace+4111 for loader or workspace+4149 for B01 | workspace is a separate6666-word reserved allocation, held at B+1174 |

The two `12D5xx` sites are **analysis-history pointers**, not window-table coefficients. No additional DSP block or delay coefficient is introduced into the model by this result.

## Analysis-history allocation and reset

1. Parent object constructor `130EF3` allocates1181 requested /1182 reserved words at `130F19..130F20`.
2. `130FB3/130FB5` calls `12DFA8` with that parent. `12DFAD` saves it as R14.
3. `12DFE8..12DFF0` calls `12D577` with destination **B+7**, R8=512 and R12=1024. This is the sole direct call to `12D577` in the aligned B span.
4. `12D577` checks frame>=hop (`12D59A/12D59D`), allocates a5-word descriptor at `12D5A6`, stores hop512 at+1, frame1024 at+2 and history length512 at+3 (`12D5B5..12D5BC`).
5. `12D5BE..12D5C4` allocates **two words**, stores the returned pointer in descriptor+4 at `12D5CB`, then loops over indices0 and1. `12D5DA` shifts the index right by1; the nonzero result exits at `12D5E0`. `12D5EA` allocates512 history words for each index and `12D5F3` publishes the pointer. **`12D5F7` reads exactly the cell just written.**
6. `12D61E/12D621` calls reset `12D546` on the new descriptor, then `12D62A` publishes it through the B+7 destination. `12D621` is the sole direct caller of this reset in the aligned B span.
7. Reset reads descriptor+4 at `12D556`. Its `R15/M4` loop generates indices0 and1 (`12D554/12D558/12D567/12D56A`). **`12D55E` reads those two allocated pointer cells**, then fills512 words at each pointee via `12D560`.

Later processing obtains the descriptor from B+7 at `12E89A`, calls analysis-history processing `12D49F` at `12E89E`, and supplies hop512 plus frame1024. This is consistent with the constructor dimensions but is not needed to prove the constructor/reset pointer reads themselves.

Disjointness uses the already-reviewed successful kind0x2000 bump allocator: `132D56..132D5F` rounds `(requested+1)&~1`; `132D6C` reads its cursor, `132D74/132D76` check the advanced cursor against the exclusive region bound, `132DEC` commits the new cursor, and `132DFF` returns the old one. Parent allocation precedes these allocations. Cursor reuse/reset, corrupted descriptors, integer wraparound and external replacement are excluded assumptions, not inferred impossible events.

## Ramp: caller-to-workspace chain

`12ADC6` is a preceding-sample read inside the ramp generator `12AD70`. Its destination/output-vector pointer is supplied on the caller stack, not fetched through mode-pointer cells1162..1165.

The retained fixed path is:

| Address | Binding |
| --- | --- |
| `130F9A..130FA5` | set workspace capacity to0x1A09=6665 and allocate6665 requested /6666 reserved words |
| `130FAD` | store allocation pointer at B+1174 |
| `13126F` | R7=DM(B+1174), called **workspace** below |
| `131277/13127A` | save workspace+2048 in dispatch stack slot-23 |
| `1315A7/1315AB` | pass that pointer as spectral-wrapper stack argument3 |
| `1315B3/1315B5` | call `12E80E` with N=513 |
| `12E839` | R9=stack argument3=workspace+2048 |
| `12E83C/12E844` | R12=4N; R8=R9+4N |
| `12E84A` | save R8=workspace+4100 in wrapper stack slot-3 |
| `12E994/12E997/12E998` | pass that pointer as pre-wrapper stack argument1 to `12E1BB` |
| `12E1FD/12E201` | pass pre stack argument1 in R12 to subtraction `12ADFF` |
| `12AE18/12AE1E` | preserve incoming R12 as R11/R13 |
| `12AE46/12AE54` | pass R13 as ramp stack argument3/output vector |
| `12AD8B` | R11=that output vector |
| `12ADBD..12ADC6` | read output[adjusted_start-1] if adjusted_start<end |

The pre controls are the common-header source words44..49, copied to B+98..103; they do not depend on selected calibration bank1..4:

| Source | Enable | Start | End | Vector length | `12ADC6` read |
| --- | ---: | ---: | ---: | ---: | --- |
|loader|1|12|512|513|workspace+4100+11 = workspace+4111|
|B01|1|50|512|513|workspace+4100+49 = workspace+4149|

Both starts/end satisfy the successful ramp guard at `12AD97..12ADA6`: start<=end<=length-1. They are exactly representable integers, so the caller's truncation does not introduce ambiguity. The full513-word output slice `[workspace+4100, workspace+4613)` lies within the workspace's reserved interval `[workspace, workspace+6666)`. This allocation is disjoint from the parent object, regardless of its absolute address.

The singular previous-sample read is initialized by the earlier prefix fill: `12ADAE/12ADB1` fills the first `start` output cells before `12ADC6` reads cell `start-1`.

### Mode5 control-flow qualification

An initial branch alone is misleading: `131591` sends selector5 to preparation at `1315D3`, **but `131624` subsequently jumps back to `131594`**, then `1315B5` invokes spectral processing. Thus successful mode5 does call the spectral wrapper after preparation. Its changed input-buffer arguments do not change stack slot-23/workspace provenance above. No assertion about the current vehicle selector is made.

## Fingerprints and validation

Loader: `output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin`, SHA256:
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.

B01 CodeFlash SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.

SHARC program addresses are16-bit shortword addresses; DM/object offsets are32-bit words. Loader segmentation is interpreted by the existing hash-validating `inspect_amp_visa.load_shortwords` and `extract_amp_sharc_app.parse_blocks`; addresses above are not raw byte offsets into the loader.

Fresh decoded range SHA256 values (exclusive end):

| Range | SHA256 |
| --- | --- |
|`12AD70..12AF36`|`9948ae3e43ee23bd71a606a52c61d029423467be33add8e94b0aa5ca36fef5b4`|
|`12D546..12D6A3`|`144ef63ab83a676439eaf976539e7685f817e470f248cfc221256762b10c0dd6`|
|`12DFA8..12E1BB`|`eb8287a7d38a3a50966cb2730e94d4d43effe9e52f7d722ec71d177ad9d59f3a`|
|`12E1BB..12E30A`|`2f47dd726085f0868ff9d52775de4c0054dec53974a04292bcebad3cdb82d615`|
|`12E80E..12EA98`|`46635edd24b797ace2a2cde87285be17e7be4373bad663e58a6e9a8dfe4cd4da`|
|`130EF3..131208`|`b637fbdfaeabfc20b9b04715b7e38bb0685a4c75ac521b82b61e5c40f0a3fd7b`|
|`131208..13163D`|`1d05ce72048326c025150ab4be28a3965a0c33f64ed55bab82a997617b20ff75`|

Validation performed without files being written: fresh aligned disassembly/caller lookup over `12A7F9..1331AF`,18 exact ramp-chain instruction assertions, fresh common-header float extraction for loader/B01, and arithmetic verification of the two-index constructor/reset loops and ramp read domains. No new persisted executable tests were added during the frozen run. This is a bounded static argument, not an independent full SHARC emulator.

## What this does not close

* B+1162..1165 consumer identities remain unresolved; these three sites are excluded only in the stated domain.
* Generic external/indirect callers, code outside the aligned B span, source/header mutation, damaged pointers, allocator reuse and live state are not covered.
* Nested-allpass ring provenance at `12D182/12D184/12D191` is a separate remaining obligation; this document does not silently absorb it.
* Whole fixed-mode5 alias completeness and a global dead-field conclusion remain unproved.
