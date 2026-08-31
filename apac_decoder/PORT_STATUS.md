# APAC port status

One row per stage. A stage is DONE only when its differential test passes against a real oracle on
real data — never on inspection of the decompilation alone.

Oracles: `reference/run.sh` (whole frame, Apple vs Pioneer), `reference/apac_fn_ref` (per function,
calls the library directly). Diff in **float32**, not S16: the int16 paths of the two decoders round
differently and a 1 LSB mismatch there means nothing.

| # | stage | classes | state | test |
| --- | --- | --- | --- | --- |
| 0 | build + per-function oracle | — | **DONE** | `reference/apac_fn_ref.cpp`, `run.sh fn`; `make check` runs the per-stage tests |
| a | bit reader and config | `TBitstreamReader`, `apac::GlobalConfig`, `BaseASCConfig`, `ProfileLevelConfig` | **DONE**, two documented gaps | `tests/test_bitreader.cpp`, `tests/test_globalconfig.cpp` |
| b | band layout | `APACICSInfo`, `APACScaleFactorData`, `apac::SFBOffsets` | **DONE** | `tests/test_icsinfo.cpp`, `tests/test_sfb_offsets.cpp`, `tests/test_scalefactors.cpp` |
| c | spectrum core | `APACSpectralData`, `apac::GetHuffmanTable` and the 12 codebooks | **DONE** | `tests/test_spectraldata.cpp`; LRVQ is NOT here, see below |
| d | transform | `APACTNSData`, `apac::LSFTools`, `apac::LPCBase`, `apac::LPMDCTFilter`, `APACSynthesisFilterBank`, `MDCT`, `DCT_IV`, `MultiRadix*FFT` | **DONE**, one documented gap | `tests/test_transform_tables.cpp`, `test_mdct.cpp`, `test_filterbank.cpp`, `test_tns.cpp`, `test_lpc.cpp`, `test_lpmdct.cpp` |
| e | bandwidth extension | `APACBWE2Decoder`, `BWELPC` | TODO | |
| f | elements | `APACSingleChannelElement`, `APACChannelPairElement`, `APACLFEElement`, `APACSyntacticElement` | TODO | |
| g | top level | `APACCoreDecoder`, `APACDecoder`, `APACASPDecoder` | TODO | |
| h1 | LRVQ frame mode | `APACLRVQDecoder`, `APACChannelPairElement::DeserializeLrvqFrame` | DEFERRED | semantics established, unreachable on our content |
| h | loudness | `apac::drc`, `apac::APACDRCDecoder` | TODO | parser only; gains need not be applied, but the bit reader must advance past them |
| i | ARMv7/QNX build | — | TODO | cross-build clean, `nm | grep emutls` empty |
| j | hook integration | — | TODO | decode -> `spa_reorder_5_1_decoder_to_declared_s16` -> PCM6/DIO5 |

Out of scope for the CarPlay 5.1 target: `apac::scene`, `apac::APACAudioSceneDecoder`,
`apac::AudioSceneConfig` (Ambisonics), `APACCoreLBRDecoder`, `apac::lbr`.

## Where stages 0, a, b, c and d stand

**Stage 0.** `reference/apac_fn_ref.cpp` is a sibling of `apac_ref.cpp` in the same arm64
container. It resolves individual members by their Itanium mangling and calls them:
`run.sh fn probe | bs | ics | scf | spec | sfb | gc | gc-raw | cookie | scene | win | mdct | fb |
tns | lpc | lpmdct`. Host paths handed to `fn`
are bind-mounted read-only. `tools/gen_fixtures.sh` drives it and writes
`tests/fixtures/*.fixture`; those are committed, so `make check` runs on the host with no
Docker. Every expectation in them is a value the library actually produced.

**Stage a — proven.**

* `TBitstreamReader`: 5428 operations over 580 randomised cases agree bit-exactly on the
  returned value and on all three mutable state fields, at all four buffer alignments,
  including buffers of 0..7 bytes where the tail padding path runs.
* `apac::GlobalConfig::Deserialize`: on the live 33-byte CarPlay cookie every field the
  library exposes matches **and** the config consumes exactly the same 150 of its 152 bits —
  which only comes out right if the element list, the 16-bit layout tag, the per-config
  trailer and the extension skip are all read at the right widths. 48000 Hz / 1024 frames /
  6 channels, layout tag `0x007c0006` = `kAudioChannelLayoutTag_MPEG_5_1_D`, C L R Ls Rs LFE
  — the order the whole-frame harness measured. On the afconvert-generated 125-byte cookie
  every field matches and the port stops at the audio-scene flag, bit 97; `run.sh fn scene`
  shows the library's own `AudioSceneConfig::Deserialize` succeeds at bit 97 and at no other
  offset within ±4, so "it stopped in the right place" is a checked claim, not an assertion.

Two branches the port deliberately does not guess at, each with its own status code:
`kPortNeedsAudioScene` (`apac::AudioSceneConfig`, out of scope for 5.1) and
`kPortNeedsLoudnessDRC` (`apac::LoudnessDRCConfig` → `mpddrc`, stage h). The generated cookie
needs both; the live CarPlay cookie needs neither.

Also not ported, and none of it reads any bits, so none of it can move the bitstream
position:

* `ProfileLevelConfig::InferProfileFromCodecConfigs` (0x9d2e0) and `GetLevelFromCodecConfigs`
  (0x9d0d0). Only `ValidateProfileLevelData`'s first gate — profile 31, level ≤ 6 — is
  implemented. Both of the others walk pointer tables in `.data.rel.ro` that were never
  dumped, so the port may accept a profile/level pair the original would reject.
* `GlobalConfig::SetChannelLayout`'s multi-config merge and its profile-table-derived layout.
  Only the single-codec-config path is implemented; both cookies take it.
* The sort and slice-assignment the original applies to the `BaseASCConfig` list. The bits
  are read; the bookkeeping over them is not.

**Stage b — proven.**

* `apac::GetSFBOffsetTable`: all 13 sample rates and both `frameLength` rejections agree with
  the library, offset for offset — 733 band offsets. 48 kHz gives 49 long bands and 14 short.
  The arrays are the uint16 view of the byte dumps in `reference/decomp/rodata/`.
* `APACICSInfo::Deserialize`: 944 cases agree on the return code, all four output fields and
  the bit count, sweeping every window sequence against every value of the 6-bit `maxSfb`
  field, both `codingType` branches and nine `(maxSfbLong, maxSfbShort)` limit pairs.
  `maxSfbLong` / `maxSfbShort` are inputs the syntactic element fills from the SFB table;
  wiring them up is stage f.
* `APACScaleFactorData::Deserialize` (0x9a140): 459 cases agree on the return code, the
  exact bit count, all eight per-group record counts and every `(codebook, run)` record and
  scalefactor the call wrote — 18 615 of each, compared as **bit patterns**, because the
  original builds a scalefactor by adding an exponent into the float's bit pattern
  (`bits(2^(k/4)) + (((v - 100) >> 2) << 23)`) rather than with a `pow`, and only a
  bit-exact match proves the exponent arithmetic. 179 of the cases are rejections
  (codebook > 11, a run past `maxSfb`, a zero run) and 205 are eight-short frames on 3-bit
  run fields. Both precomputed clamp constants — `0x13000000` = 2^-89 and `0x52D744FD` =
  2^38.75, for a running value driven out of `[-256, 255]` — are reached, by cases that
  encode book-0 codewords explicitly to walk the value there.

**Stage c — proven, short-window de-interleave included.**

`APACSpectralData::Deserialize` (0xab3b0): 379 cases agree on the return code, the exact bit
count and all 102 432 output coefficients, compared as bit patterns. Coverage measured by
instrumenting the port and replaying the fixture: 5 365 all-zero tuples through the odd
books' 1-bit flag, 2 092 codebook-11 escapes of which **562 exceed 1023 and take the
`pow(v, 4/3)` path**, and 6 556 lookups at a NEGATIVE index into the `i^(4/3)` table. 47 of
the 51 eight-short cases use a window grouping other than eight groups of one, sweeping
every grouping from `{8}` through `{2,3,3}` to `{1,2,4,1}` — so the band-interleaved staging
and the de-interleave that unpacks it, which `LRVQ_SEMANTICS.md` §6.8 flagged as transcribed
but never tested, are now covered. The 48 kHz long (49 bands, 1024 coefficients) and short
(14 bands) layouts are both exercised with the library's own band offsets.

`apac::GetHuffmanTable` and all twelve codebooks are generated into `src/huff_tables.cpp` by
`tools/gen_huff_tables.py`, which parses the structure out of `reference/decomp/RODATA.md`,
asserts every field against the library ELF, and takes the payload bytes from the ELF after
asserting they equal the committed dumps in `reference/decomp/rodata/`. Nothing is retyped.
Every book's Kraft sum is exactly 1 counting the guard record.

`APACSpectralData::Deserialize`'s only exit sets `W0 = 0`, so it cannot fail; malformed input
is caught upstream in `APACScaleFactorData::Deserialize`.

**Stage d — proven, except the LSFTools codebook half.**

Everything in the transform path is transcribed index for index out of the disassembly except
the FFTs, and there is a reason for that: `MultiRadixFFT::CplxOutOfPlaceTransform` and
`MultiRadixRealFFT` are three-line wrappers around **`vDSP_DFT_Execute`** — Apple's Accelerate
framework, which is closed and is not on the head unit. So the port has its own radix-2 FFT and
the agreement there is float noise rather than bit-exact. Everything with no FFT under it comes
out bit-exact, and does.

| what | agreement | coverage |
| --- | --- | --- |
| the six constant tables + the size gate | **bit exact**, 3456 words | sine/KBD/DCT-IV modulation at both sizes, 27 NULL checks |
| `MDCT::InverseInPlaceTransform` | 1.4e-07 of full scale (denormals 2.1e-06) | both block sizes x 16 cases x both halves, 36 864 floats |
| `APACSynthesisFilterBank::FrequencyToTimeInPlace` | 3e-08 | 30 stateful steps, 3 channels, all 4 window sequences, both shapes |
| `APACTNSData` ctor / ParseTNSData / Apply | **bit exact** | 14 rates; 32 parses compared over the whole 190-byte object; 32 applies |
| `apac::LPCBase` + the stateless `apac::LSFTools` | 9.4e-08 | 10 entry points x 16 orders, 176 checks |
| `MultiRadixRealFFT` + `apac::LPMDCTFilter` | 2.7e-07 | 4 transform lengths, both fftSize/n ratios, 72 checks |

`reference/dsp_cases.h` generates every input and is included by BOTH the oracle and the host
tests, so the committed fixtures carry only the expectations. Regenerate with
`tools/gen_fixtures_transform.sh`.

**The one gap.** `apac::LSFTools::Initialize`, `Reset`, `DecodeLSF`, `QuantizeLSF` and the
`LSF_To_LPC` that chains the conversions are **not started**. They need the object's six
`std::vector` members and the 64 KiB codebook block at `0x333000` that `Initialize` unpacks, and
their only caller is `APACLRVQDecoder` — already established below as never entered on the
CarPlay 5.1 stream. Guessing at them was not worth it; they land with stage h1.

Four things the differential runs settled that static analysis had not:

* The complex FFT is **forward**, `exp(-2*pi*i*j*k/m)`, unscaled. `DFTHandle<true>` keeps two
  vDSP setups and the direction was not readable from the decompilation.
* The real FFT is the **plain unscaled DFT** with `X[0]` in `realp[0]` and `X[n/2]` in
  `imagp[0]`; its inverse is unnormalised, so `Inverse(Forward(x)) == n*x`. vDSP's factor of two
  and the `0.5` at `0x345db4` cancel.
* `FrequencyToTimeInPlace`'s third argument is the window **shape** and its fourth the window
  **sequence**, and the shape is remembered **per channel**: frame k's rising half is windowed
  with frame k-1's shape.
* In `LSFTools::LSP_To_LPC` it is the polynomial built from the **even-indexed** LSPs that takes
  the sum recursion. Hex-Rays gave both stack arrays the same base pointer and dropped one side
  of every merged expression, so this was not in its output at all; the wrong choice leaves
  `lpc[0]` and `lpc[1]` right and every later coefficient off by a constant.

## LRVQ is off the critical path — verified twice

`APACLRVQDecoder` is never entered on the CarPlay 5.1 stream. Patching the entry of
`LrvqDeserialize` to `MOV W0,#1 ; RET` leaves the decoded output byte-identical, and so does the
same patch on `SpectrumDeserializeCPE`; patching `APACSpectralData::Deserialize` instead produces
no output at all. Independently reproduced with `reference/scratch/stub_shim.c` over 600 packets,
after the same result over all 3977.

So this content is entirely transform/SQ coded and LRVQ is a different frame mode. Its semantics
are established anyway (`reference/decomp/LRVQ_SEMANTICS.md`) and its return value propagates as a
hard error, so it still has to be ported eventually — but after the transform path works, not
before. Reordered above.

Instrumenting the library: it is linked `DT_SYMBOLIC`, so `LD_PRELOAD` cannot interpose its
internals. Patch the function entry in memory, as `stub_shim.c` does. Build the mangled name by
reading it out of the binary (`gobjdump -T`); hand-constructing one is how the first attempt at
this experiment silently proved nothing.

## Known traps, all found the hard way

* `TBitstreamReader` refill is a **big-endian** 32-bit load; a short tail pads with **`0xFF`**, not
  zero; the constructor pre-aligns an unaligned buffer.
* Cookie metadata helpers must be called at `GetCookieOffset() + 2` — the `dapa` payload starts with
  the `0x0800` sync word and GlobalConfig is two bytes further on.
* Do not scope anything by matching "APAC" in symbol names. Apple's DSP layer is not named that way.
* Tables are copied byte-for-byte out of the binary. The `i^(4/3)` table matches a generated one only
  to within 1 ULP, and the KBD windows are not analytic at all.
* Only `n == 128` and `n == 1024` are accepted by the window lookups, so there are exactly two block
  sizes.
* `LrvqDeserialize` (0x…, 3264 bytes, 811 insns) has no usable Hex-Rays output — port it from the
  disassembly appended in the `#if 0` block.
* Huffman codebook field semantics: `n` is a cap on the unary prefix used as a shift
  (`0x80000000u >> n`), NOT an element count, and `a` is a right shift giving
  `1 << (31 - a)` uint16 payload slots. Established against the library and written up in
  `reference/decomp/LRVQ_SEMANTICS.md` §1.1; the port follows that document.
* The 16-bit config word's fields do **not** all line up with the C cookie helpers.
  `APACDecoder_SampleRate`, `_FrameLength` and `_NumberOfChannels` match
  `GlobalConfig::Deserialize` exactly, but `APACDecoder_GetProfile` reads six bits one
  position late and does not. Trust the bitstream, not that helper.
* `GlobalConfig::Deserialize` reads two fields it then throws away: one reserved bit between
  the profile level and the sample-rate index, and eight bits after the channel count (value
  2 on both cookies). Skipping them shifts everything after by nine bits.
* `chan::CoreDecoderConfig::Deserialize` escape-codes its element count by hand as
  5 / +10 / +16 with the escape steps adding 31 and 1054 — not through `apac::GetEscBits`,
  which is only ever instantiated `<16,20>`, `<8,16>` and `<6,12>`.
* For a 16-bit channel-layout tag of **1** the original writes the channel bitmap into the
  layout but never sets the layout tag to `0x10000`, so its own `GetNumChannels` then reads
  the layout as "use descriptions" with a count of zero. That looks like a bug in Apple's
  source. It is unreachable on the CarPlay streams (tag 121 or 124) and the port reproduces
  it rather than correcting it.
* The Huffman guard record `entries[n]` is the **one** record whose payload is not
  `1 << (31 - a)` slots. Having no terminating zero, its index `(w << n) >> 31` is the bit
  after the n ones — which belongs to the next codeword and can be either value — so the
  binary gives it TWO slots holding the same word. Sizing it by the formula reads two bytes
  past the table. Found by round-tripping every codeword of every book back through the
  tables.
* `APACScaleFactorData` and `APACSpectralData` are handed **different `apac::SFBOffsets`
  types than `apac::GetSFBOffsetTable` returns**. The table record is 40 bytes,
  `{u64 rate, SFBOffsets long, SFBOffsets short}`; the spectrum decoder's argument is one of
  those halves, `{u64 numBands, const u16* offset}`, and it reads `numBands` as a **u16**.
* The scalefactor struct's stride is **15 records and 15 scalefactors per window group**, but
  a single-group long block legitimately runs past 15: 49 bands and up to 49 records all live
  in group 0's slice because there is no group 1. A test vector that hands
  `APACSpectralData` `nrec[0] > 15` with only 15 records supplied makes the original read a
  zero record — codebook 0, run 0 — and its band-zeroing loop is a `do/while` that
  decrements by four and stops only on exactly zero, so it runs away and writes over
  everything. The library segfaults; a run of zero must never reach it. `APACScaleFactorData`
  rejects one, which is why this is only reachable from a hand-built object.
* For the same reason every band width must be a multiple of the codebook's tuple size (4 for
  books 1..4, 2 for the rest). The 48 kHz tables are; the original would hang if one were not.
  The port uses a bounded `for` loop instead, which differs only where the original runs away.
* Codebook 11's escape exponent is the `double` nearest `4/3`, not `1.33333333`; the
  decompiler prints the latter. The two differ by one float ULP above 1023. macOS libm and
  glibc/aarch64 agree bit-for-bit on `pow(v, 4/3)` over the 562 escapes the fixture drives
  through that path, but that is an observation about two libms, not a guarantee.
* Zeroed coefficients are **not** all written the same way. Codebooks 3 and 4 leave their
  zero lanes at `0.0f` and then multiply all four lanes by the scalefactor, so a negative
  scalefactor puts `-0.0` in the output; every other book stores a literal `+0.0`. A float
  comparison hides this. Compare bit patterns.
* `APACICSInfo`'s window-grouping word is one hex digit per group counting the windows in it,
  built from seven bits where a **zero** opens a new group: all ones gives 8 (one group of
  eight), all zeros gives `0x11111111` (eight groups of one).
* Hex-Rays **merges adjacent stack VLAs** into one array and then drops one side of every
  expression that used the other. It did this three times in stage d — `APACTNSData::Apply`'s
  parcor-to-LPC recursion, `LPCBase::ComputeLPCs` (the pseudocode appears to zero the signal it
  just windowed), and `LSFTools::LSP_To_LPC`. If a decompiled function looks like it clobbers
  its own work, count the `alloca`s before believing it.
* The LSF trig divides by the literal **`3.14159265`**, not `M_PI`, and calls the **double**
  `cos` / `acosf`. Substituting either misses by far more than float noise.
* `APACTNSData`'s byte at `+1` is a filter **count** for a long block and an 8-bit per-window
  **present mask** (window 0 in bit 7) for a short one — so a long block's filter records are
  indexed by filter and a short block's by window. Its `present` byte at `+0` is set by the
  syntactic element, not by `ParseTNSData`.
* `APACSynthesisFilterBank` only works at `frameLength == 1024`: it looks its short window up at
  `frameLength >> 3`, and the only windows that exist are 128 and 1024.
* The stage-d objects are **big and must not go on a stack**: `SynthesisFilterBank` is 90 KiB
  (its overlap buffer is channels x 1024 floats), `LPMDCTFilter` 47 KiB, `RealFFT` 30 KiB, `MDCT`
  10 KiB. The original heap-allocates those buffers; the port cannot allocate on the decode path,
  so they are inline members. `tools/build_qnx.sh` links the whole set for armle-v7 with
  `emutls=0 exceptions=0 rtti=0 stl=0` and no undefined symbol beyond libc/libm.
* The Makefile had **no header dependencies** until stage d. Changing a constant in
  `include/apac/` recompiled some translation units and not others, and the survivors disagreed
  about a struct's size — which failed as wrong numbers in a differential test, not as a link
  error. `-MMD -MP` now; if you see an inexplicable numeric regression, `make clean` first.
