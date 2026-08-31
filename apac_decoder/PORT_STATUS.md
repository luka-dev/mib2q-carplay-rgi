# APAC port status

One row per stage. A stage is DONE only when its differential test passes against a real oracle on
real data — never on inspection of the decompilation alone.

Oracles: `reference/run.sh` (whole frame, Apple vs Pioneer), `reference/apac_fn_ref` (per function,
calls the library directly). Diff in **float32**, not S16: the int16 paths of the two decoders round
differently and a 1 LSB mismatch there means nothing.

| # | stage | classes | state | test |
| --- | --- | --- | --- | --- |
| 0 | build + per-function oracle | — | **DONE** | `reference/apac_fn_ref.cpp`, `run.sh fn`; `make check` runs 4 tests |
| a | bit reader and config | `TBitstreamReader`, `apac::GlobalConfig`, `BaseASCConfig`, `ProfileLevelConfig` | **DONE**, two documented gaps | `tests/test_bitreader.cpp`, `tests/test_globalconfig.cpp` |
| b | band layout | `APACICSInfo`, `APACScaleFactorData`, `apac::SFBOffsets` | **PARTIAL** — `APACScaleFactorData` not started | `tests/test_icsinfo.cpp`, `tests/test_sfb_offsets.cpp` |
| c | spectrum core | `APACSpectralData` | TODO | LRVQ is NOT here, see below |
| d | transform | `APACTNSData`, `apac::LSFTools`, `apac::LPCBase`, `apac::LPMDCTFilter`, `APACSynthesisFilterBank`, `MDCT`, `DCT_IV`, `MultiRadix*FFT` | TODO | |
| e | bandwidth extension | `APACBWE2Decoder`, `BWELPC` | TODO | |
| f | elements | `APACSingleChannelElement`, `APACChannelPairElement`, `APACLFEElement`, `APACSyntacticElement` | TODO | |
| g | top level | `APACCoreDecoder`, `APACDecoder`, `APACASPDecoder` | TODO | |
| h1 | LRVQ frame mode | `APACLRVQDecoder`, `APACChannelPairElement::DeserializeLrvqFrame` | DEFERRED | semantics established, unreachable on our content |
| h | loudness | `apac::drc`, `apac::APACDRCDecoder` | TODO | parser only; gains need not be applied, but the bit reader must advance past them |
| i | ARMv7/QNX build | — | TODO | cross-build clean, `nm | grep emutls` empty |
| j | hook integration | — | TODO | decode -> `spa_reorder_5_1_decoder_to_declared_s16` -> PCM6/DIO5 |

Out of scope for the CarPlay 5.1 target: `apac::scene`, `apac::APACAudioSceneDecoder`,
`apac::AudioSceneConfig` (Ambisonics), `APACCoreLBRDecoder`, `apac::lbr`.

## Where stages 0, a and b stand

**Stage 0.** `reference/apac_fn_ref.cpp` is a sibling of `apac_ref.cpp` in the same arm64
container. It resolves individual members by their Itanium mangling and calls them:
`run.sh fn probe | bs | ics | sfb | gc | gc-raw | cookie | scene`. Host paths handed to `fn`
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

**Stage b — `apac::SFBOffsets` and `APACICSInfo` proven, `APACScaleFactorData` not started.**

* `apac::GetSFBOffsetTable`: all 13 sample rates and both `frameLength` rejections agree with
  the library, offset for offset — 733 band offsets. 48 kHz gives 49 long bands and 14 short.
  The arrays are the uint16 view of the byte dumps in `reference/decomp/rodata/`.
* `APACICSInfo::Deserialize`: 944 cases agree on the return code, all four output fields and
  the bit count, sweeping every window sequence against every value of the 6-bit `maxSfb`
  field, both `codingType` branches and nine `(maxSfbLong, maxSfbShort)` limit pairs.
  `maxSfbLong` / `maxSfbShort` are inputs the syntactic element fills from the SFB table;
  wiring them up is stage f.
* `APACScaleFactorData::Deserialize` (0x9a140) is **not started**. It decodes through
  `apac::GetSCFHuffmanTable` and the codebook descriptors, whose field semantics the port
  brief flags as unestablished. That is exactly what the concurrent entropy-core work is
  pinning down (`reference/scratch/lrvq_probe.cpp`); this should be picked up from there
  rather than guessed at here, and it will land with stage c's tables rather than duplicate
  them.

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
* Huffman codebook byte layout is known; the field semantics are **not** — `n` is used as a shift
  (`0x80000000u >> n`), which is not an element count. Establish this dynamically before porting.
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
* `APACICSInfo`'s window-grouping word is one hex digit per group counting the windows in it,
  built from seven bits where a **zero** opens a new group: all ones gives 8 (one group of
  eight), all zeros gives `0x11111111` (eight groups of one).
