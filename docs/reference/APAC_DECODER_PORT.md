# APAC decoder: porting Apple's implementation to ARMv7/QNX

Status 2026-08-31. This document replaces the working assumption that an APAC decoder for the head
unit would have to be reverse-engineered from stripped binaries.

## The finding

`libSPF_CP_PlugInWrapper.so` from Pioneer SPHERA 1.030010 (AArch64, AirPlay 480.7.4) links Apple's
APAC decoder **with its C++ symbols intact**. `file` reports the object as stripped, and the debug
sections are gone, but the dynamic symbol table still carries 441 APAC entries — mangled Itanium
names that decode to full class names, method names and parameter types, plus vtables (`_ZTV`),
typeinfo (`_ZTI`) and type strings (`_ZTS`).

Measured footprint of the decoder, after closing over the call graph (see the correction below):

* **396 functions, 179 120 bytes (175 KB)** — all decompiled, 0 failures;
* 234 data objects, dumped byte-for-byte;
* separate from the 623-function / 464 KB Fraunhofer FDK-AAC build (SBR, MPEG Surround, uniDrc,
  USAC/ACELP/TCX) that also lives in the same library. The FDK code belongs to Pioneer's own media
  path. APAC has its own MDCT, its own filter bank and its own LP tools, so it is **not** built on
  FDK and there is no open-source decoder to borrow.

### Two counting corrections

The first pass at this document said 307 functions and 124 KB from a name filter over the symbol
list. Both numbers were wrong and the method was wrong:

* `apac_symbols_pioneer.txt` has 307 `.text` rows but only **263 unique addresses** — destructors
  appear two or three times each for their D0/D1/D2 aliases.
* **Apple's DSP layer carries no "APAC" in its symbol names at all.** Closing over the call graph
  instead of filtering on a substring pulls in `MDCT` (the actual inverse transform), `DCT_IV`,
  `MultiRadix*FFT`, `BWELPC`, `CacDecoder`/`Quantizer`, `DRCConfiguration`, and
  `GetSineWindowCoefficientPtr` / `GetKBDWindowCoefficientPtr`, which are the only route to the
  window tables. Never scope this port by name matching.

The same applies to the tables. Of the 48 APAC-named `.rodata` objects, 45 are `_ZTS` typeinfo
strings and the three real ones total 30 bytes. The actual coefficient tables are **unnamed**, and
most of them hang off relocated pointer arrays in `.data.rel.ro` behind three-instruction
accessors, so following `ADRP` out of `.text` finds the accessor and stops. Walking the pointer
graph is what reaches all 234.

The full demangled list with addresses and sizes is in
[`apac_symbols_pioneer.txt`](apac_symbols_pioneer.txt).

## Architecture recovered from the symbols

APAC is an AAC-family transform codec: it has ICS info, scalefactor bands, TNS, SCE/CPE/LFE
syntactic elements and an MDCT synthesis filter bank, plus a bandwidth-extension layer, a lattice /
residual vector quantiser for the spectrum, LSF/LPC tools, and the mandatory loudness-DRC layer that
Apple's public specification requires.

| class | fns | bytes |
| --- | ---: | ---: |
| `(free functions)` | 30 | 12228 |
| `apac::drc` | 13 | 11080 |
| `APACLRVQDecoder` | 18 | 9520 |
| `APACBWE2Decoder` | 8 | 7480 |
| `apac::APACAncillaryFrameDecoder` | 7 | 6472 |
| `APACDecoder` | 11 | 6452 |
| `apac::GlobalConfig` | 12 | 6296 |
| `apac::LSFTools` | 16 | 5192 |
| `apac::APACAudioSceneDecoder` | 5 | 4256 |
| `APACCoreDecoder` | 13 | 4204 |
| `APACSpectralData` | 1 | 3820 |
| `APACChannelPairElement` | 9 | 3480 |
| `APACSyntacticElement` | 8 | 2828 |
| `apac::scene` | 9 | 2808 |
| `apac::chan` | 6 | 2768 |
| `APACTNSData` | 4 | 2356 |
| `apac::ProfileLevelConfig` | 4 | 2272 |
| `apac::APACDRCDecoder` | 6 | 2272 |
| `apac::AncillaryConfig` | 6 | 2056 |
| `APACSynthesisFilterBank` | 6 | 1852 |
| `APACCACDecoder` | 2 | 1824 |
| `apac::LPMDCTFilter` | 9 | 1736 |
| `ContentOutBuffer` | 3 | 1616 |
| `APACCoreLBRDecoder` | 5 | 1616 |
| `std` | 11 | 1612 |
| `APACSingleChannelElement` | 5 | 1464 |
| `apac::AudioSceneConfig` | 1 | 1412 |
| `APACASPDecoder` | 6 | 1284 |
| `APACLFEElement` | 5 | 1260 |
| `APACScaleFactorData` | 1 | 1224 |
| `apac::LPCBase` | 3 | 1208 |
| `apac::BaseASCConfig` | 6 | 1012 |
| `APACICSInfo` | 5 | 1008 |
| `apac::lbr` | 4 | 996 |
| `apac::LPCodeBook` | 2 | 948 |
| `APACExtFrameDecoder` | 3 | 900 |
| `apac` | 6 | 884 |
| `apac::ExtFrameUnkownDecoder` | 5 | 760 |
| `APACChannelRemapper` | 5 | 604 |
| `apac::ChanDecElement` | 4 | 604 |
| `apac::ASCChannelMap` | 2 | 560 |
| `APACIndividualChannelStream` | 4 | 488 |
| `apac::ExtensionConfig` | 1 | 388 |
| `boost` | 5 | 368 |
| `apac::Autocorr` | 3 | 344 |
| `apac::LoudnessDRCConfig` | 1 | 272 |
| `apac::TrimmingData` | 1 | 260 |
| `apac::TCEConfig` | 4 | 260 |
| `APACInstanceConfig` | 1 | 224 |
| `CommunicationPluginProxy` | 1 | 84 |
| `CarPlay` | 1 | 76 |

Total: 307 functions, 126988 bytes.

Entry points worth naming:

```
0x00093e50  APACDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&)
0x000946a0  APACDecoder::Initialize(apac::GlobalConfig const&)
0x00093d90  APACDecoder::CreateDecoderElement(apac::ASCType)
0x0007db30  APACCoreDecoder::DecodeAPACFrame(TBitstreamReader<unsigned int>&, apac::FrameType,
                                             std::vector<float*>&)
0x0007d950  APACCoreDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool)
0x000ab3b0  APACSpectralData::Deserialize(TBitstreamReader<unsigned>&, APACICSInfo const&,
                                          APACScaleFactorData const&, apac::SFBOffsets const&)
0x000ae980  APACSynthesisFilterBank::Initialize(unsigned, unsigned)
```

Note the decoder is float-based (`std::vector<float*>`, `apac::LPMDCTFilter::MDCTToAutoCorr(float
const*, float*, ...)`), not fixed-point.

`APACASPDecoder` corresponds to the Audio Synchronization Packet in Apple's published specification:
an independently decodable packet, recommended every 75 packets (1.6 s at 48 kHz). That is the only
legal entry point into a running stream, so it matters for flush and resume.

## Why the static call graph is not enough

`APACDecoder::DecodeFrame` dispatches entirely through virtuals: its direct `bl` closure is one
function, 564 bytes, and the work happens behind `blr` on vtables (`apac::DecElement`,
`APACSyntacticElement` and its SCE/CPE/LFE subclasses). The reachable set for a channel-based 5.1
decode therefore has to be established dynamically, not by reading branches.

## Test harness — all three legs already exist

1. **Encoder.** `afconvert` produces APAC test vectors for any signal. Apple's specification gives
   the recipe, and the previously recorded `!dat` failure was a missing mandatory parameter, not a
   codec limitation:

   ```sh
   afconvert pcm.caf out.mp4 -d apac -l MPEG_5_1_A -u csrc 36 -u cdrc 1 -u aspf 75
   ```

   `csrc` (content source), `cdrc` (DRC compressor configuration) and `aspf` (ASP interval) are all
   required; loudness/DRC metadata is mandatory in the bitstream.
2. **Reference decoder.** `tools/apple_apac_oracle.c` decodes raw packets with the real cookie and
   no container, producing deterministic S16LE PCM.
3. **Per-function oracle.** Pioneer's AArch64 functions run under Unicorn. The technique is already
   proven on this exact binary: `chacha20_poly1305_encrypt_all_64x64` reproduced byte for byte, and
   `AirPlayAudioFormatIndexToASBD` was executed to recover the format table.

So every ported function can be differentially tested against the original on real inputs, and the
whole decoder against Apple's, on vectors we generate ourselves.

## Port order

Bottom-up, each stage tested against Unicorn before the next is started:

1. `TBitstreamReader` and `apac::GlobalConfig` / `BaseASCConfig` / `ProfileLevelConfig`.
2. `APACICSInfo`, `APACScaleFactorData`, `apac::SFBOffsets`.
3. `APACLRVQDecoder` and `APACSpectralData` — the entropy/spectrum core, and the largest single risk.
4. `APACTNSData`, `apac::LSFTools`, `apac::LPCBase`, `apac::LPMDCTFilter`,
   `APACSynthesisFilterBank`.
5. `APACBWE2Decoder`.
6. `APACSingleChannelElement`, `APACChannelPairElement`, `APACLFEElement`, `APACSyntacticElement`.
7. `APACCoreDecoder`, then `APACDecoder`, then `APACASPDecoder` for stream entry.
8. `apac::drc` / `apac::APACDRCDecoder` — at minimum the parser, because the metadata is mandatory
   and the bit reader has to advance past it even if the gains are not applied.

Droppable for the CarPlay 5.1 target: `apac::scene`, `apac::APACAudioSceneDecoder`,
`apac::AudioSceneConfig` (Ambisonics), `APACCoreLBRDecoder` and `apac::lbr` (low bitrate),
`APACCACDecoder` unless tracing shows index 65 uses it.

## Open items

* Trace `APACDecoder::DecodeFrame` under Unicorn on the captured index-65 frames to get the executed
  function set and the true port scope. Everything above is a ceiling, not an estimate.
* Count instructions per frame in the same trace. The stream is 46.9 packets/s at 48 kHz; the Krait
  ARMv7 budget should be comfortable, but it should be measured before any code is written.
* Measure the index-66 (eight-channel) native channel order the way index 65 was measured. See
  `hook/audio/spatial_downmix.h`.
* Lifting Apple's decoder out of a licensee's binary is derived work. Fine locally; it is not
  publishable.
