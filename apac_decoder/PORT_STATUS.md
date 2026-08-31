# APAC port status

One row per stage. A stage is DONE only when its differential test passes against a real oracle on
real data — never on inspection of the decompilation alone.

Oracles: `reference/run.sh` (whole frame, Apple vs Pioneer), `reference/apac_fn_ref` (per function,
calls the library directly). Diff in **float32**, not S16: the int16 paths of the two decoders round
differently and a 1 LSB mismatch there means nothing.

| # | stage | classes | state | test |
| --- | --- | --- | --- | --- |
| 0 | build + per-function oracle | — | TODO | `make check` runs |
| a | bit reader and config | `TBitstreamReader`, `apac::GlobalConfig`, `BaseASCConfig`, `ProfileLevelConfig` | TODO | |
| b | band layout | `APACICSInfo`, `APACScaleFactorData`, `apac::SFBOffsets` | TODO | |
| c | spectrum core | `APACLRVQDecoder`, `APACSpectralData` | TODO | |
| d | transform | `APACTNSData`, `apac::LSFTools`, `apac::LPCBase`, `apac::LPMDCTFilter`, `APACSynthesisFilterBank`, `MDCT`, `DCT_IV`, `MultiRadix*FFT` | TODO | |
| e | bandwidth extension | `APACBWE2Decoder`, `BWELPC` | TODO | |
| f | elements | `APACSingleChannelElement`, `APACChannelPairElement`, `APACLFEElement`, `APACSyntacticElement` | TODO | |
| g | top level | `APACCoreDecoder`, `APACDecoder`, `APACASPDecoder` | TODO | |
| h | loudness | `apac::drc`, `apac::APACDRCDecoder` | TODO | parser only; gains need not be applied, but the bit reader must advance past them |
| i | ARMv7/QNX build | — | TODO | cross-build clean, `nm | grep emutls` empty |
| j | hook integration | — | TODO | decode -> `spa_reorder_5_1_decoder_to_declared_s16` -> PCM6/DIO5 |

Out of scope for the CarPlay 5.1 target: `apac::scene`, `apac::APACAudioSceneDecoder`,
`apac::AudioSceneConfig` (Ambisonics), `APACCoreLBRDecoder`, `apac::lbr`.

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
