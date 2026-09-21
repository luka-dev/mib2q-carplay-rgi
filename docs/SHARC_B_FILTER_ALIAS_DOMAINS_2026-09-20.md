# B pointer cells1162..1165: bounded filter-heap exclusion

Static/offline only. No runtime model, live-state inference, deployment or vehicle access.

## Result

Four previously unresolved indirect-read sites have an explicit **conditional heap provenance**:

| Read | Origin / address domain |
| --- | --- |
| `12D97A` | `descriptor+3` points to separately allocated channel-pointer array; index `< descriptor+1` |
| `12D97C` | each channel points to separately allocated section-pointer array; index `< descriptor+2` |
| `12DCDB` | reads filter `descriptor+3`, not a B-object parameter offset |
| `12DCE5` | channel-pointer array indexed by fixed caller channel count, equal to constructor capacity |

These reads do not read B-object pointer cells1162..1165 after successful fresh construction with preserved descriptors and normal allocator/DAG state. This is **not** a global no-consumer, dead-field or complete fixed-mode5 proof. It does not identify the consumers of the four pointers or establish that omitting them is acoustically neutral.

## Evidence and allocation ownership

`130F19/130F1B/130F1D/130F20` allocates the parent B object as `1*1181` words, kind `0x2000`. `132D56..132D5F` rounds `(requested+1)&~1`, reserving1182 words. The successful kind0x2000 allocator path loads cursor `DM(B925A)` at `132D6C`, compares the advanced cursor to exclusive upper bound `B89B8` at `132D74/132D76`, commits it at `132DEC`, zero-fills and returns the old cursor at `132DFF`. With nonwrapping arithmetic and no cursor reset/reuse, subsequent allocations are disjoint from the parent allocation.

`12D844` allocates the filter descriptor (5 requested /6 reserved words), stores channel capacity at+1, section capacity at+2, then allocates the channel-pointer array stored at+3. For every channel, it allocates a section-pointer array (`12D8B7/12D8C0`). For every section, `12FC86` allocates a2-word biquad history and writes its pointer into the section array (`12FCB8`). `12FC78` explicitly zeros both state words. Descriptor+4 is a caller-supplied lifecycle flag pointer; it is **not** claimed to be heap-owned. The B constructors pass a B-object lifecycle flag, which is distinct from the four pointer cells.

All direct callers in the **aligned B span** `12A7F9..1331AF` were re-decoded and checked against the following domain, not discovered through a byte-pattern search:

| Constructor call | Process call | Descriptor destination | Channels | Section capacity |
| --- | --- | --- | ---: | ---: |
| `12B849` | `12B8BA` | B+43 |8|4|
| `12EB3A` | `12EC4D` | pair heap object+3 |2|8|
| `131015` | `13138C` | B+34 |6|2|
| `131059` | `1313E6` | B+42 |2|2|

Reset wrapper `12D94C` has one direct caller, `12DC4E`, in this span. The processing wrapper's caller-supplied channel count comes from `12B89E`, `12EC45/12EC47`, `131385/131387`, or `1313E1/1313E3`; it equals the corresponding allocation capacity. This bounds `12DCE5` without inventing an intrinsic descriptor-capacity guard in the processing wrapper.

## Reproducibility

Source: `output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin`; SHA256 `9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.

Architecture: SHARC VISA, program addresses in16-bit shortwords, data addresses in32-bit words. The segmented loader is validated and decoded by `inspect_amp_visa.load_shortwords/decode`; these addresses are decoded program addresses, not byte offsets into the loader. Whole B span SHA256: `a30e9c631734d5846aa97ac2ac4c50dacc1769b5284c282960363070880552d4`. The artifact records separate constructor/reset/wrapper/allocator range hashes and exact instruction pins.

```sh
python3 tools/audit_amp_b_filter_aliases.py --out output/r2/soundamp/remaining-blocks-20260920-v3/b_binding_filter_aliases.json
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p test_amp_b_filter_aliases.py -v
```

Ten tests pass. The independent bounded interpreter executes fresh decoded `12D844`, nested `12FC86/12FC78`, the reset traversal `12D94C`, the channel-pointer traversal `12DCD1..12DCF6`, and separately the `12DAA5` cascade wrapper. Successful allocation/diagnostic/fill calls are stand-ins; the two audio kernels `12D9A9/12DA2F` are not executed. The channel-loop test captures its `12DAA5` call, while a separate test executes that wrapper through its real coefficient/history address loops. This is not an audio or SHARC timing emulator. Parallel operands use old register values, delayed branches execute their two delay slots, and software/hardware loops obtain their actual bounds from decoded instructions. The resulting allocation graph matches a separately constructed interval witness for all four constructor domains.

The negative control deliberately changes a channel's section-array pointer to `B+1162`: decoded reset then reads `B+1162` and `B+1163`. This demonstrates why descriptor-integrity assumptions are essential and why the result cannot be generalized to arbitrary RAM state.

## Extension: retained section counts close the cascade wrapper

The audit now extracts the entire3562-word B source again from both hash-pinned images and validates whole-source and individual-bank CRCs. It does not trust copied JSON bank data. All four banks for each source have these section counts:

| Count at B offset | Owner | Capacity | Loader count | B01 count |
| ---: | --- | ---: | ---: | ---: |
|335|pair1|8|2|8|
|379|pair2|8|1|8|
|447|pair3|8|7|8|
|498|pair4|8|5|8|
|549|pair5|8|6|8|
|618|pair6|8|3|8|
|593|six-channel filter|2|2|2|
|677|two-channel filter|2|2|2|
|698|eight-channel finish|4|2|4|

These72 bank×descriptor combinations conservatively cover all fixed tails, including successful fixed mode5, without relying on a particular branch being disabled. Actual coefficient values can differ between banks; only their address domain is at issue here. All counts are exact nonnegative integers and therefore retain their value through the wrapper's float32 `+0.5` then truncation.

`12DAA5` receives the channel's section-pointer array, coefficient-count record and count N. It reads exactly5N coefficients at record offsets1..5N (`12DB1A/1D/20/23/29`) into40 scratch words. The paired branch executes `floor(N/2)` times, accesses state-pointer indices0..2floor(N/2)-1, and the odd branch uses indexN-1. Thus all indices fit the actual constructor's section-array allocation. Each of the N states contributes exactly two history-word reads and writes. Decoded tests cover the actual retained counts1..8, including both odd/even branches, and assert precise address sets, not just numerical output agreement.

Read sites `12DB3B/12DB4F/12DBB5` are therefore conditionally bounded to owned section-pointer arrays; reads `12DB44/4B/51/57/BB/C1` and writes `12DB6A/73/79/82/D3/DC` are bounded to their allocated two-word states. Actual coefficient addresses are below B+1162 for all eight banks. This extends the heap exclusion to the cascade **wrapper**, not the unexecuted audio-kernel instruction bodies or arbitrary mutated counts. Counts exceeding capacities (including93), negatives and fractional counts are rejected by this retained-domain helper.

## Typed residual candidates

The aligned census also checks all six direct calls to nested-allpass `12D10E` (`12A8BD/12A8E3/12A909/12A92F/13303C/13308A`) and the sole direct ramp call `12AE54→12AD70`.

`12D182/12D184/12D191` are ring-history reads: bases originate from descriptor+4/+1, and indices from descriptor+5/+2, with requested-length capacity checks at `12D157/12D15E` and per-sample wrap at `12D195/12D1A0`. The remaining proof obligation is **constructor allocation provenance plus retained delay/capacity domains** for those six callers, not identifying a mysterious arithmetic block.

`12ADC6` reads the preceding sample of a spectral-ramp output vector, at `caller_output + adjusted_start - 1`. The path guards start<=end<=length-1, changes zero start to1 and executes that read only when adjusted_start<end. Its remaining obligation is output-vector allocation/capacity provenance at caller `12AE54`; the invalid-input branch is not used to infer general termination. Neither typed candidate is declared globally closed in this report.

## Exact remaining boundaries

* `1162..1165` remain **unresolved consumers**. Their extracted delay-curve pointees and target/shadow copies are not evidence of a real audio consumer.
* Previous smoothing proof for `12FAF2` remains conditional on retained bank counts and normal maps. The helper has a lower bound but **no intrinsic upper bound**; count93 can reach pointer cells through the coefficient loop. See the earlier `b_binding_pointer_aliases.json` artifact.
* This audit is not a full retained-image indirect-call graph. The aligned B census has16093 instructions; arbitrary incoming bases across control-flow joins, code outside that span and external/indirect calls remain outside the proof.
* Generic cascade wrapper reads/writes are now coupled to every retained bank and constructor capacity as above. Arbitrary mutated section counts, audio-kernel internal aliases and generic external calls remain outside that result.
* Other nominal non-affine sites, e.g. `12ADC6`, `12D182/12D184/12D191`, `12D55E/12D5F7`, still need their own typed incoming-pointer/caller proofs. They cannot be dismissed just because literal offsets1162..1165 are absent.
* No conclusion about current vehicle mode5, current B state, or acoustic effect is made. Both `dead_field_proven` and `fixed_mode5_complete_alias_proof` remain false in the artifact.
