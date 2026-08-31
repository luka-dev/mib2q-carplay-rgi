# APAC decompilation — method, coverage, confidence

Companion to `CLASSES.md`, `RODATA.md` and the per-class `.c` files in this directory.
Read this before trusting anything in them.

## Tool and method

**IDA Pro 9.3 with Hex-Rays**, driven headless through the `idapro` idalib Python module — not
through the GUI and not through Ghidra. The Ghidra MCP was never opened on this binary.

The work was scripted rather than done function by function, because 334 functions through an
interactive tool would have been both slower and less consistent. Four passes over a private copy
of the library (`/tmp/apacwork/lib.so`, so the shared `.i64` was never contended):

1. **Enumerate.** Parse `.dynsym` directly out of the ELF, demangle with `c++filt`, and select
   every `.text` symbol whose demangled name is scoped to `apac::`, an `APAC*` class,
   `TBitstreamReader`, `SC_*`, or `ContentOutBuffer`, while excluding `mpddrc::` (the MPEG-D DRC
   reference code that `apac::drc` wraps, and which is a separate body of code).
2. **Type.** Rebuild each function's prototype from its Itanium mangling — the mangling encodes
   every parameter type exactly — map the C++ types onto IDA types, and apply. 321 of 334
   prototypes applied cleanly. The 13 that did not are the `extern "C"` entry points
   (`APACDecoder_DecodeFrame` and friends), which carry no type information in their names and
   were left with Hex-Rays' own guess.
3. **Decompile.** All 334 functions, after typing.
4. **Data.** Dump `.data.rel.ro` with relocations applied, walk the pointer graph, extract the
   tables.

Applying the prototypes matters more than it sounds. Before it, every function looked like
`__int64 f(__int64 *a1, unsigned int a2, __int64 a3 ... __int64 a9)` with seven phantom arguments,
and every bitstream operation was `*((_DWORD *)a1 + 5)`. After it the same code reads
`bs->bitsLeft -= 2; v = bs->cache >> 30;`. That is the difference between material you can port
from and material you cannot.

## Coverage

| | |
|---|---|
| functions decompiled | **334 / 334** (0 failures) |
| `.text` covered | 161 044 bytes, 40 046 instructions |
| vtables recovered | 37 |
| `_ZTI` records parsed | 45 |
| data objects extracted byte-for-byte | 223 (205 232 bytes) |
| functions with a Hex-Rays reliability warning | 6 |

### The brief's function count was low

The brief said 307 functions / 126 988 bytes, from `apac_symbols_pioneer.txt`. That file has 307
rows but only **263 unique addresses** (destructors are listed two or three times for the D0/D1/D2
aliases, which share one body). And it is missing 71 functions that are in `.dynsym` — the whole
`SC_*` audio-scene family, `ContentOutBuffer`'s destructors, and the `boost::any::holder` shims.
The true set is **334 unique addresses, 161 044 bytes**. Nothing in the priority list a–f was
missing; the additions are almost all scene code, which is out of scope for the CarPlay target.

## What is solid, stage by stage

Confidence is about whether the *decompiled C is a faithful rendering of the machine code*, not
about whether the codec is understood.

| stage | status | confidence |
|---|---|---|
| **a. `TBitstreamReader`** | done, and better than expected | **high** |
| **a. `GlobalConfig` / `BaseASCConfig` / `ProfileLevelConfig`** | done | high |
| **b. `APACICSInfo`, `APACScaleFactorData`, `apac::SFBOffsets`** | done; SFB table fully decoded | **high** |
| **c. `APACLRVQDecoder`** | done, but 4 of 15 carry a decompiler warning | **medium** |
| **c. `APACSpectralData::Deserialize`** | done, clean, no warning | high |
| **d. TNS / LSF / LPC / LPMDCT / filter bank** | done, clean | high |
| **e. `APACBWE2Decoder`** | done, clean | high |
| **f. element classes, core, `APACDecoder`, `APACASPDecoder`** | done, clean | high |
| **g. `apac::drc` / `APACDRCDecoder`** | done | high for the parser; the gain application lives in `mpddrc`, which was deliberately not decompiled |
| **skipped tier** (`apac::scene`, `SC_*`, `AudioSceneConfig`, LBR) | decompiled anyway, since it was free | high, but unreviewed |

### a. `TBitstreamReader` — it is *not* inlined-only

The brief flagged this as possibly inlined everywhere with no real class. It is both. All eight
methods have out-of-line copies at `0x000b1430`–`0x000b19a8` — constructor, `GetBits`, `GetBit`,
`SkipBits`, `AlignToNextByte`, `PutBack`, `BitsRemaining`, `FillCacheFrom` — *and* GCC inlined
them at nearly every call site. `FillCacheFrom` is the only one that survives as a real call in
the inlined paths, which is why it shows up in every `Deserialize`.

This is the single best outcome of the job: the bit reader can be transcribed rather than
inferred. Layout and exact semantics are in `CLASSES.md` §1. Three details a port will get wrong
if it does not read them: the refill is a **big-endian** 32-bit load; a short tail is padded with
**`0xFF`**, not zero; and the constructor **pre-aligns an unaligned buffer** to a 4-byte boundary,
which every later refill depends on.

### c. `APACLRVQDecoder` — the one soft spot

Four functions carry Hex-Rays' `local variable allocation has failed, the output may be wrong!`:

| addr | function | why it matters |
|---|---|---|
| `0x0009a950` | `LRVQLPC::Initialize()` | small, low risk |
| `0x0009aa10` | `lsfsToMdctSpectralEnvelope(bool, aligned_array<uint16,5> const&, aligned_array<float,1024>&)` | 1232 bytes; selects among the seven envelope tables |
| `0x0009aee0` | `ltpToMdctSpectralEnvelope(uint16, uint16, aligned_array<float,1024>&)` | 536 bytes |
| `0x0009b100` | `LrvqDeserialize(...)` | **3264 bytes, 811 instructions — the largest and most important function in the entropy core** |

All four have large stack frames full of `aligned_array` temporaries, which is exactly what
exhausts the decompiler's variable allocator. The pseudocode is still mostly right, but it is a
sketch. **Full disassembly is appended inside `#if 0` blocks** after every function in
`APACLRVQDecoder.c`, `APACSpectralData.c`, `APACScaleFactorData.c`, `APACICSInfo.c` and
`TBitstreamReader.c` (31 functions), so there is ground truth to check against. Port
`LrvqDeserialize` from the disassembly, not from the pseudocode.

`APACSpectralData::Deserialize` itself came out clean and is readable end to end: it walks the
window groups, reads a `(codebook, band-run)` pair per scalefactor band out of the
`APACScaleFactorData` array at `+488` (two bytes per record: low byte codebook, high byte run),
calls `apac::GetHuffmanTable(codebook)`, and branches into three different decode paths for
codebook ranges `<= 6`, `7..10` and `> 10`. It dereferences the `x^(4/3)` table at `0x003308c0`
directly. Bands whose codebook is 0 are memset to zero with a NEON `stp q`.

## Things in the output that are wrong, and why

1. **`*(&vN - 1544) = 0;` as the first statement of most functions.** This was stripped from the
   emitted files. It is Hex-Rays rendering the AArch64 stack-clash probe
   (`SUB X9, SP, #4,LSL#12 ; STR XZR, [X9,#0xFB0]`) that GCC emits with
   `-fstack-clash-protection`. It is prologue noise and has no semantics.
2. **Dead assignments from register liveness.** Lines like
   `cur = (const unsigned __int8 *)&qword_100;` or `v9 = BYTE2(bs->end);` are the decompiler
   naming a register that happens to still hold a stale value. Ignore any assignment whose result
   is never used on a path that matters.
3. **The member-offset tables in `CLASSES.md` §4 are a mechanical scrape**, not a declaration.
   They collect every access through a typed pointer and group by byte offset. Consequences:
   fields the library never touches are absent, padding is invisible, and a handful of entries are
   artefacts. A known example: `APACICSInfo` shows `+0x2f0`/`+0x2f8` as a `u64` pair, seen once
   each, 700 bytes above every other member — that is from
   `APACLRVQDecoder::SpectrumDeserializeSCE`, where it may be a genuine `std::vector` member of a
   large `APACICSInfo` or may be register aliasing. It was not resolved. **Treat a low-`n` offset
   far from a class's main cluster as suspect.**
4. **Four vtables have NULL slots** where the destructor pair belongs: `apac::IASCConfig`,
   `apac::DecElement`, `APACSyntacticElement`, `apac::LPCBase`. This was checked against
   `.rela.dyn` directly — there is no relocation at those addresses, so the slots really are null
   in the image. All four are abstract bases that are never instantiated; every concrete subclass
   vtable is complete. It is not a decompilation failure.
5. **All table lengths in `RODATA.md` are upper bounds** except the two that are pinned by
   identifying the generating function. See `RODATA.md` for the reasoning. Do not size a ported
   array from the extent column.

## Corrections to the brief's assumptions about the tables

The brief said "48 `.rodata` objects ... these are the codec's tables". They are not. 45 of the 48
are `typeinfo name for ...` strings; the three real data symbols total 30 bytes. The actual tables
are **unnamed**, and most are not addressed from `.text` at all — they hang off relocated pointer
arrays in `.data.rel.ro`, behind three-instruction accessors (`apac::GetHuffmanTable`,
`apac::GetSCFHuffmanTable`, `apac::GetSFBOffsetTable`). A scan that follows `ADRP` out of `.text`
finds the accessor's array and stops there. Walking the pointer graph transitively reaches 223
data objects totalling 205 232 bytes, all dumped byte-for-byte under `rodata/`.

Two tables are now known exactly:

* `0x003308c0` is `float[1024]` and entry `i` is `i^(4/3)` to the last bit of float precision,
  verified for all 1024 entries. The boundary scan reports 4272 bytes for it; the extra 176 are a
  different object. **Use 1024 entries.**
* The scalefactor-band offset table is completely decoded — 12 sample rates, both long and short
  offset arrays, in `RODATA.md`. For 48 kHz: 49 long bands, 14 short. That is `apac::SFBOffsets`
  fully solved, which was priority (b).

## What still needs dynamic verification

Ranked by how much a wrong guess would cost.

1. **`APACLRVQDecoder::LrvqDeserialize` (`0x0009b100`)** and the three other warned LRVQ
   functions. Run them against the Pioneer library in the `linux/arm64` container with recorded
   inputs and compare state, function by function. Nothing else in the port is as likely to be
   subtly wrong.
2. **The Huffman codebook structure.** `RODATA.md` gives the exact byte layout — a 12-entry
   pointer array at `0x00424d98`, each pointing at `{const Entry *entries; uint64 n;}`, each entry
   `{uint64 a; const void *payload;}`, with `a` ascending 6..31 and payload sizes halving. What
   those fields *mean* is not established. `APACScaleFactorData::Deserialize` uses `n` as a shift
   (`0x80000000u >> n`), which does not fit a plain element count. **Do not port these from an
   assumed canonical-Huffman layout.** Feed known bit patterns through
   `apac::GetHuffmanTable` + the decode loop in the real library and read the structure back out.
3. **`TBitstreamReader`'s unaligned constructor path.** The alignment pre-load loop is small but
   fiddly and its `cur` fixup (`this->cur = v11`, the pre-increment value) is the kind of
   off-by-one that a static read will not settle. Drive the real constructor with buffers at all
   four alignments and compare all 24 bytes of state.
4. **`TBitstreamReader::BitsRemaining`** truncates the pointer difference to 32 bits before the
   multiply. That is what the code does; whether it is intentional or a latent bug in Apple's
   source matters only if a port on a different pointer size reproduces it. Match the observed
   behaviour, do not "fix" it.
5. **`APACDecoder::DecodeFrame`'s element dispatch (`0x00093e50`).** The static picture: build a
   reader over the packet, read **one bit** and store it at `this+16` (frame type / independence
   flag), then iterate the vector of `{uint32, unique_ptr<apac::DecElement>}` at `this+392`, and
   for each, load the object at `DecElement+16` and call **its** vtable slot 4 (`vidx 2`) with
   `(obj, &bs)`. By signature that slot matches `apac::IASCConfig::Deserialize(BitstreamReader&)`
   — `apac::BaseASCConfig::Deserialize` is the concrete override and it returns the constant `5`.
   That is consistent but not proven. Trace it to get the real executed set for a 5.1 frame; the
   brief's open item about establishing the reachable set dynamically still stands.
6. **The seven envelope tables** at `0x00317e90 … 0x0032be90`. Which base is selected under which
   conditions is decided inside `lsfsToMdctSpectralEnvelope`, one of the four warned functions.
   Both the selection and the table lengths need confirming against the running library.
7. **`apac::drc`.** Everything below `apac::drc::DynRangeCompressor` calls into `mpddrc`, which
   was excluded from this job. The bitstream parser is here and is what the port needs first (the
   reader has to advance past mandatory DRC metadata even if no gain is applied), but the gain
   computation is not.

## Bonus finds worth knowing about

* **The public C API is present and named**: `APACDecoder_Create`, `_Configure`, `_DecodeFrame`,
  `_Reset`, `_GetParameter`, `_SetParameter`, `_GetCookieOffset`, `_GetVersion`, `_GetProfile`,
  `_SampleRate`, `_FrameLength`, `_NumberOfChannels`, `_Dispose` — decompiled in
  `_free_functions.c`. `APACDecoder_GetCookieOffset` is the magic-cookie entry point.
  `APACDecoder_Configure` carries printf strings naming its own parameters
  (`"## APACDecoder_Initialize ## sampleRate = %d packetSizePerChannel = %d numChannels = %d"`),
  which pins the configuration contract without any guessing.
* **Apple's own source paths survive** in three `__assert_fail` calls: the codec header is
  `.../Support/ApplePositionalAudioCodec/Source/APACStructs.h`, and the assert at line 198 is in
  `apac::GetEscBits`. The `__PRETTY_FUNCTION__` strings confirm `BitstreamReader` is a typedef for
  `TBitstreamReader<unsigned int>` and that the parameter is named `numBits`.
* **`apac::GetEscBits` is instantiated exactly three times** — `<16,20>`, `<8,16>`, `<6,12>` — so
  there are three escape-coded field widths in the bitstream and no others.
* **`apac::GetSFBOffsetTable` returns NULL unless `frameLength == 1024`.** The decoder has no
  other frame length.

## Ground rules that were followed

* Nothing here was taken from a pre-existing IDB comment; the database was created fresh.
* Nothing was reasoned from how AAC, USAC or FDK build their tables. Where the shape of a
  structure resembles something familiar — the Huffman descriptors most of all — it is flagged as
  a resemblance and explicitly not relied on.
* Every table came out of the binary as bytes. None was retyped from a specification.
* Nothing outside `apac_decoder/reference/decomp/` was modified.
