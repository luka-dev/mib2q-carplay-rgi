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
| e | bandwidth extension | `APACBWE2Decoder`, `BWELPC` | **PARSER DONE**, synthesis TODO | the header is on the bit path and is ported; the synthesis is never active on this content, see below |
| f | elements | `APACSingleChannelElement`, `APACChannelPairElement`, `APACLFEElement`, `APACSyntacticElement`, `APACIndividualChannelStream`, `APACCACDecoder` | **DONE**, bit exact | `tests/test_frames.cpp`; whole capture with `reference/run.sh port` |
| g | top level | `APACCoreDecoder`, `APACCoreLBRDecoder`, `APACDecoder`, `APACASPDecoder`, `APACChannelRemapper`, `apac::TrimmingData` | **DONE**, bit exact above the transform | `tests/test_frames.cpp`; whole capture with `reference/run.sh port` |
| h1 | LRVQ frame mode | `APACLRVQDecoder`, `APACChannelPairElement::DeserializeLrvqFrame` | DEFERRED | semantics established, unreachable on our content |
| h | loudness | `apac::drc`, `apac::APACDRCDecoder`, `apac::LoudnessDRCConfig` | **DONE** for the `apac` layer; `mpddrc` is a stage of its own | `tests/test_drc.cpp`, plus the reachability run below |
| i | ARMv7/QNX build | — | **DONE** | `tools/build_qnx.sh` (`--both`, `--qemu`), `tests/test_escape_pow.cpp` |
| j | hook integration | — | TODO | decode -> `spa_reorder_5_1_decoder_to_declared_s16` -> PCM6/DIO5 |

Out of scope for the CarPlay 5.1 target: `apac::scene`, `apac::APACAudioSceneDecoder`,
`apac::AudioSceneConfig` (Ambisonics), `apac::lbr`.

`APACCoreLBRDecoder` was on that list and is **not** droppable: `apac::ChanDecElement::Initialize`
allocates the core decoder, plants the LBR vtable and calls the LBR `Initialize`, so the object the
C API drives is the subclass and the frame path runs `APACCoreLBRDecoder::Deserialize`. Same for
`APACCACDecoder`, which `docs/reference/APAC_DECODER_PORT.md` called droppable "unless tracing shows
index 65 uses it". Tracing shows it does, on every channel pair of every packet.

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
| `MDCT::InverseInPlaceTransform` | 1.4e-07 of full scale (denormals 2.1e-06) | both block sizes x 21 cases x both halves, 48 384 floats |
| `APACSynthesisFilterBank::FrequencyToTimeInPlace` | 3e-08 | 30 stateful steps, 3 channels, all 4 window sequences, both shapes |
| `APACTNSData` ctor / ParseTNSData / Apply | **bit exact** | 14 rates; 32 parses compared over the whole 190-byte object; 32 applies |
| `apac::LPCBase` + the stateless `apac::LSFTools` | 9.4e-08 | 10 entry points x 16 orders, 176 checks |
| `MultiRadixRealFFT` + `apac::LPMDCTFilter` | 2.7e-07 | 4 transform lengths, both fftSize/n ratios, 72 checks |

`reference/dsp_cases.h` generates every input and is included by BOTH the oracle and the host
tests, so the committed fixtures carry only the expectations. Regenerate with
`tools/gen_fixtures_transform.sh`.

### Why the transform tolerance is not a defect

Every other stage of this port is bit-exact, so the two rows above that are not needed a reason
rather than a shrug. Three things are **checked on every run**, not argued:

1. **A double-precision reference.** `test_mdct.cpp` runs the same algorithm — same
   pre-modulation, an O(m²) DFT, same eight-store post-modulation — entirely in double, on the
   library's own float32 modulation table promoted to double. That is the exact answer both
   sides are approximating. Measured distance from it, excluding the all-denormal case:
   **port 1.41 ULP, library 1.28 ULP.** The same size. On the worst case of all (the denormal
   one) the library is the *further* of the two, 45.7 ULP against the port's 42.7. The test
   asserts `err(port) <= 4 * err(library)`, which an off-by-one in a twiddle, a window index or
   the post-modulation index map would fail by orders of magnitude while the library sat at one
   ULP.
2. **The error tracks the number of terms summed.** Cases 16–20 hold exactly 2, 4, 16, 64 and
   256 non-zero coefficients. At n = 1024 the absolute error, in ULPs of the input's full scale,
   runs `1:0.0005  2:0.0009  4:0.0012  16:0.0034  64:0.0108  256:0.0231  1024:0.0453` — two
   orders of magnitude across the sweep. Relative to each block's *own* peak it rises from
   0.48 ULP at one active bin and then saturates at about 1.0, because the peak grows with the
   term count too. That is accumulated rounding. A wrong index would be just as wrong with two
   active bins as with a thousand.
3. **It does not compound on real data.** `run.sh swap` patches the port's filter bank into the
   middle of Pioneer's own decoder (`reference/transform_swap_shim.cpp`) and decodes the whole
   capture through it — 3977 packets, 4 072 448 frames, 24.4 M samples of real coefficients in
   the real window-sequence and window-shape order:

   | | |
   | --- | --- |
   | max abs difference | 2.09e-07, i.e. **3.5e-07 of full scale, −129 dBFS** |
   | in S16 | **1 LSB**, on **0.0059 %** of samples (1444 of 24 434 688) |
   | worst 1024-frame block | 2.1e-04 of that block's own peak, still 1 LSB |

   And the acceptance measure — does the substitution change how well the decoder matches
   Apple? Per-channel `rms(diff)` against Apple's AudioToolbox, over the whole capture:

   | | ch0 | ch1 | ch2 | ch3 | ch4 | ch5 |
   | --- | --- | --- | --- | --- | --- | --- |
   | Pioneer, unmodified | 0.2637 | 0.5925 | 0.4724 | 0.2944 | 0.2967 | 0.2471 |
   | Pioneer, port's transform | 0.2638 | 0.5925 | 0.4724 | 0.2944 | 0.2967 | 0.2471 |

   `max|d|` is 1–2 LSB in both, unchanged. Pioneer and Apple are already 1–2 LSB apart on this
   capture; the port's transform vanishes inside that gap.

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

## Stages f and g — proven, and bit exact above the transform

`reference/run.sh port` decodes the whole live capture twice, once with the original library and
once with the port, and compares them two ways.

**The frame layer is bit exact.** Everything the element and top layers do has exactly one
observable output of its own — the spectrum each channel hands the filter bank — and over the whole
capture the port's and the library's agree **byte for byte**: 23 862 records, 6 channels x 3 977
packets, 24 434 688 float32 coefficients, plus the window sequence attached to each. Compared as
bit patterns, so the signed zeros codebooks 3 and 4 leave behind are covered too. The comparison is
made by `reference/spectrum_dump_shim.cpp`, which patches
`APACSynthesisFilterBank::FrequencyToTimeInPlace` in the running library and reads the buffer before
the transform touches it.

**The decoded PCM is FFT-limited, and by exactly that much.** Against the unmodified library, in
float32:

| ch | peak &#124;A&#124; | max &#124;A-B&#124; | rel to peak | dBFS | max S16 LSB | S16 samples differing |
| -: | ---: | ---: | ---: | ---: | ---: | --- |
| 0 | 0.463123 | 1.639e-07 | 3.539e-07 | -129.0 | 1 | 209 / 4 072 448 (0.0051 %) |
| 1 | 0.827541 | 2.086e-07 | 2.521e-07 | -132.0 | 1 | 354 / 4 072 448 (0.0087 %) |
| 2 | 0.827541 | 2.086e-07 | 2.521e-07 | -132.0 | 1 | 366 / 4 072 448 (0.0090 %) |
| 3 | 0.509583 | 1.490e-07 | 2.924e-07 | -130.7 | 1 | 196 / 4 072 448 (0.0048 %) |
| 4 | 0.508970 | 1.341e-07 | 2.635e-07 | -131.6 | 1 | 238 / 4 072 448 (0.0058 %) |
| 5 | 0.343438 | 8.941e-08 | 2.603e-07 | -131.7 | 1 | 81 / 4 072 448 (0.0020 %) |

1444 of 24 434 688 S16 samples differ, all by exactly 1 LSB. And the residual is **entirely** the
FFT: decoding the capture with the LIBRARY's frame layer and the PORT's transform, and comparing
that against the port end to end, gives **0.000e+00 on every one of the 24 434 688 samples**. So
stages a..g are bit exact and stage d's `vDSP_DFT_Execute` gap is the whole of the difference.

`make check` runs the same test on six committed packets (`tests/fixtures/frames.bin`, 149 KB,
`tools/gen_frame_fixture.sh` to regenerate). They are packets 429..434 of the capture, contiguous,
decoded from a fresh decoder: instrumenting the port over all 3977 packets shows that window is the
shortest run reaching every feature this content ever uses but one — long blocks, `LONG_START`,
`EIGHT_SHORT` with one window group and with three, `LONG_STOP`, TNS present and absent, a joint
channel pair, and an element whose presence bit is clear. Ten more packets would add only a second
short-window grouping.

### The four things this stage got wrong first, and what settled each

* **The C API's decoder is an `APACASPDecoder`, not an `APACDecoder`, so the frame-type field is
  TWO bits.** `APACDecoder_Create` at 0x106800 runs `APACDecoder::APACDecoder`, zeroes the three
  extra words at +464/+472/+480 and then overwrites the vptr with `APACASPDecoder`'s. Reading one
  bit puts every later field one place early, and the first element then looks like an LRVQ frame —
  which is exactly the symptom the port showed. The field is **1 on all 3977 packets**, i.e. this
  whole stream is Audio Synchronization Packets: independently decodable, which is what Apple's
  specification says a decoder may resume on.
* **The float32 output is scaled by 1/32768.** The decode path is internally S16-scaled: the filter
  bank's output runs to +-32768 and `InterleaveConvert` (0xb1d90) multiplies by the decoder's
  +2064 on the way out — for the float path too, not only for S16. Measured: dividing the library's
  float32 output by the buffer it hands its own filter bank gives exactly 3.0517578e-05 on every
  sample. Being a power of two the multiply is exact, so it costs no accuracy.
* **`APACCACDecoder` is live**, and on this content it is live on *every* channel pair of *every*
  packet: the joint-pair bit is set 3977 times out of 3977 and the non-joint CPE branch is never
  taken. Its two variable-length code tables are copied out of `.rodata` by
  `tools/gen_cac_tables.py`; its gain table is built at Initialize by the same
  `powf`/`sqrtf` recurrence `CacQuantizer::Initialize` uses.
* **`APACBWE2Decoder::Deserialize` is on the bit path even though BWE never fires.** Patching its
  entry to consume no bits truncates the decode; patching `GetActiveFlag` to return 0, or
  `DecodeFrame` to return 0, leaves the whole capture's float32 output bit identical. So the port
  reads the header exactly and refuses with `kPortNeedsBWE` if a channel ever comes up active.

### How the frame layer was measured

Two techniques, both in `reference/`, both needed because the library is linked `DT_SYMBOLIC` and
`LD_PRELOAD` cannot interpose its internals:

* `scratch/trace_shim.c` replaces exported **vtable slots** — `APACICSInfo`, the three element
  classes, `APACCoreLBRDecoder` — with wrappers that still call the real function by its own symbol.
  That is what showed two bits gone before the element loop instead of one. Use it for anything
  reached through a vtable; it needs no entry patching and destroys nothing.
* `scratch/reach_shim.c` patches a comma-separated list of function entries to `MOV W0,#1 ; RET`,
  or to `MOV W0,#0 ; RET` with `APAC_STUB_RET0=1`. Return-1 answers "is it called at all"; return-0
  answers "does it read bits or change the output". Both answers are needed: `TrimmingData`,
  `APACAncillaryFrameDecoder` and `APACExtFrameDecoder` are all *called* every frame and none of
  them can change the output, because they sit after the last field anything else reads.

`APACSynthesisFilterBank::FrequencyToTimeInPlace` is called **directly**, not through a vtable, so
the spectrum dump has to patch its entry — which destroys it — and finish the call with the port's
own filter bank. That substitution is the one `run.sh swap` already measured at 3e-08, and it
cannot touch the dump, which is taken before the transform runs.

### What stages f and g deliberately do not implement

* **LRVQ.** A set frame-mode bit in a CPE, or a set coding-type bit in an SCE or LFE, returns
  `kPortNeedsLRVQ`. Unreachable: patching each of
  `APACLRVQDecoder::SpectrumDeserialize{SCE,CPE,LFE}` in the running library to return 0 — to
  consume no bits — leaves the whole capture's decoded float32 output bit identical, which none of
  them could do if it read a single bit. `src/element.cpp` carries the exact three-way seam for the
  stage-h1 port to be dropped into.
* **The BWE synthesis**, as above.
* **A codec config whose channel remap is non-empty.** `APACChannelRemapper::Process` is ported
  (the cycle-following swap, not a gather) but the live cookie carries no remap, and patching
  `Process` out of the library changes nothing.
* **More than one codec config.** The structure is there — one `CoreDecoder` per config, channels
  concatenated in config order — but only the single-config path is exercised, because that is all
  either cookie produces.

## Stage i — the ARMv7 / QNX build, and what running it there found

`tools/build_qnx.sh` cross-builds `src/` with the project's own toolchain images
(`Tools/qnx-65-sdp-docker`), **both** GCC 4.9.4 (`--variant 4.9`, the default and the strict
one) and GCC 8.5.0 (`--both`). It links a `.so` — the decoder ships inside
`libcarplay_hook.so`, but linking one here is what lets the assertions run against a real link
rather than against object files — and rejects the result if `nm` finds any `emutls`, any
`__cxa_throw` / `__gxx_personality` / `_ZTI` / `_ZTV`, or any `operator new`, `std::` or
`__cxa_guard`. It prints the remaining undefined set so a new runtime dependency appears in
the log instead of at load time; today that set is libc and libm only.

`--qemu` goes further and **executes the whole test suite on real QNX 6.5** on an emulated
Cortex-A15, one boot for all of it (a boot costs about a minute), each test handed its fixture
out of `/proc/boot`. `tools/qnx_qemu_run.sh` drives that: it is
`Tools/qnx-65-sdp-docker/tests/qemu-run.sh` plus three knobs it does not have — data files in
the IFS, several programs per boot, and `--libm` — and it borrows that repo's boot pieces
rather than copying them.

**Two source changes were needed, both minimal, neither behavioural.**

* `src/config.cpp`: QNX 6.5 puts every C99 function in `<math.h>` behind `#if _HAS_C9X`, which
  the headers set only for C compiled as C99. In C++ the declarations are simply absent, so
  `log2f` did not compile — though `nm -D libm.so.2` finds `log2f`, `log2`, `exp2f`, `pow`,
  `powf` and `ceilf`, so it is a missing declaration and not a missing function. Declared under
  `#if defined(__QNX__)`. Defining `_HAS_C9X` from the build instead looked tidier and is
  **wrong**: it also switches on QNX's own C++ `<math.h>` overloads, which then redefine GCC
  `<cmath>`'s in any translation unit including both — `tests/test_mdct.cpp` does. The build
  passes `-D_NO_CPP_INLINES` for that same collision, which is what makes `sin`/`cos` reach
  libm directly instead of through QNX's inline `_Sin(x, 0)`.
* `src/lrvq.cpp`: its two bit-pattern reads through a `reinterpret_cast` are
  `-Werror=strict-aliasing` under GCC 4.9.4. `memcpy` instead — the same single load everywhere.

Everything else compiled unchanged under both compilers, first try.

**On the target, every bit-exact test still passes**: bitreader, globalconfig, icsinfo,
scalefactors, spectraldata, transform_tables, escape_pow, and the tolerance-based mdct, lpmdct
and filterbank. Two miss their tolerance, and **neither is a port bug**:

| test | on ARMv7 | tolerance | cause |
| --- | --- | --- | --- |
| `test_tns` | 1.67e-6 of full scale | 1.53e-6 | FMA contraction |
| `test_lpc` | 9.3e-10 abs on a 1.9e-3 peak | 7.7e-10 | QNX libm `cos`/`acos` |

`test_tns` is *entirely* FMA contraction: arm64 fuses `a*b+c` into one rounding, the target is
VFPv3-d16 and has no FMA at all. Rebuilding the **host** test with `-ffp-contract=off`
reproduces the target's bits exactly — `3f04a9ea` at index 179, the same 1.66893005e-06 — so
nothing here is ARM- or QNX-specific. `test_lpc` is not contraction (the host passes with
`-ffp-contract=off`); it is QNX's libm `cos`/`acos` differing from glibc/arm64 by about an ULP,
and the LSF trig is where that lands.

Both are far under an S16 LSB, which is 3.05e-5 of full scale: the TNS delta is 1/18 of one and
the LPC delta 1/32000 of one. `build_qnx.sh --qemu` records them by name and fails on anything
else, so a real regression on the target still stands out. If the transform tolerances are ever
recalibrated, calibrate them with contraction off.

## Stage i — `pow(v, 4/3)` on QNX libm, settled

The old note said macOS and glibc/aarch64 agree over the 562 escapes the fixture drives, "an
observation about two libms, not a guarantee". It is now a guarantee, three ways, and none of
them is a third libm.

* **The domain is bounded.** The escape reads its unary prefix against `~w | 0x800000`, so bit
  23 is always set, `k <= 8`, `nb = k + 4 <= 12` and `v <= 8191`. Exactly **7168** values reach
  `pow`. `tests/test_escape_pow.cpp` re-derives that by sweeping the byte `k` depends on
  instead of asserting it.
* **The reference is the exact value, not a libm.** `tools/gen_escape_pow_table.py` evaluates
  `v ** y` at 80 decimal digits with mpmath — `y` being the double `0x3FF5555555555555` the
  library materialises — and rounds to float32 into `tests/escape_pow_table.h`. The 533
  distinct values the committed `spectraldata.fixture` drives through the path, which the
  library itself produced, match it exactly.
* **The margin makes every libm equivalent.** The generator also measures how far each exact
  value sits from a float32 rounding boundary. Worst case **5.7e4 double-ULPs, at v = 3222**.
  A `pow` wrong by less than that cannot change the float32 result, so any libm accurate to
  better than ~15 bits lands on this table by construction.
* **And it was run on the target.** Cross-compiled for armle-v7, executed on QNX 6.5 under
  QEMU against `libm.so.2`: all 7168 bit-exact. The head unit's own `libm.so.2`
  (`Tools/qnx-carplay-emu/byPERK/ifslibs`) is a different build from the SDP's; with 15 bits of
  slack that cannot matter.

## Stage h — loudness / DRC, and where the parser actually lives

All four of Apple's DRC entry points are dispatchers, and between them they read **no bits**:

| | |
| --- | --- |
| `apac::LoudnessDRCConfig::Deserialize` | `0x00072bb0`, 272 bytes — lazily builds a `DynRangeCompressor`, `Initialize`s it from the `ParamList`, delegates |
| `apac::drc::DynRangeCompressor::Deserialize` | `0x0007ecc0`, 100 bytes — delegates to its payload, or `return 1` immediately with nothing read |
| `...::APACDRCExtensionPayload::Deserialize` | `0x0007ea20`, 160 bytes — calls `mpddrc::UniDrc::Deserialize`, folds failure into "disable and reset", always returns 0 |
| `apac::APACDRCDecoder::Deserialize` | `0x00074980`, 72 bytes — `DynRangeCompressor::Deserialize(bs, HeaderPayloadType 0)` |

All four are ported (`include/apac/drc.h`, `src/drc.cpp`), and `GlobalConfig::Deserialize`'s DRC
gate now goes through the real `LoudnessDRCConfig` instead of a bare status code.

**Every bit of the payload is read by `mpddrc::UniDrc::Deserialize` (`0x000b9b30`)** — the
MPEG-D DRC parser (ISO/IEC 23003-4) Apple links in whole: 278 exported symbols, 62 classes and
51 `Deserialize` members in this binary, `std::vector` throughout. That is a stage of its own
and it is **not** ported. So the layer refuses with `kPortNeedsLoudnessDRC` the moment a payload
exists, having consumed nothing — the refusal names an exact bit position rather than a guess.

**Refusing is safe here, and that is measured.** All five DRC entry points (the four above plus
`mpddrc::UniDrc::Deserialize`) were patched to `MOV W0,#imm ; RET` in the running library and
the whole capture decoded again, with `reference/scratch/reach_shim.c` — the technique that
settled LRVQ. Over **all 3977 packets, 97 738 752 bytes of float32, the output is byte-identical
to the unpatched decode, and identical for both return values 0 and 1**. The DRC path reads zero
bits on this content and nothing downstream reads its result.

The structure says why: `apac::AncillaryConfig::Deserialize` only sets its flag bit 1
(`*(this+2) |= 2`) after `LoudnessDRCConfig::Deserialize` succeeds, and
`APACAncillaryFrameDecoder::Deserialize` only calls the frame side when that same bit is set. No
DRC in the cookie means no DRC in any frame — and the live CarPlay cookie does not set the flag,
which is why the port parses it end to end, 150 of its 152 bits, without reaching `drc.cpp`.

`tests/test_drc.cpp` checks the two cases that decide bit accounting: with no payload the layer
moves the reader by zero bits, over a whole capture's worth of frames; with one, it refuses and
still has not moved the reader.

**Correction to an earlier note.** The 125-byte afconvert cookie does *not* parse further with
stage h landed. Its stop at bit 97 is the **audio-scene** flag, which `GlobalConfig` reads
before the DRC flag, so `apac::AudioSceneConfig` is what that cookie is waiting on — not
`LoudnessDRCConfig`.

Applying the gains stays out of scope: `ProcessUniDrc` touches no bitstream, and the head unit
does not want loudness normalisation applied on top of its own. `APACDRCDecoder::DecodeFrame` is
the original's call with the processing left out, and `tests/test_drc.cpp` checks it leaves the
samples untouched.

## Stage j — the integration seam, located

The decoder is finished; what remains is wiring. The seam is exact:
`hook/audio/audio_hook.c:1841`, the `if (frameComplete)` block inside `spa103_thread_main`'s
receive loop. The loop already frames APAP correctly (BE32 inclusive length, one framing phase per
`read` so the drain cannot defeat TCP flow control) and raises `frameComplete` on each whole frame.

Today that block does nothing but pace: it sleeps `SPA_103_APAP_FRAME_NS` per frame after
`SPA_103_APAP_PREFILL_FRAMES`, deliberately reproducing the backpressure of Apple's 32-package
hose so the receiver does not advertise 21 MB and swallow 100 seconds of audio in 4.5. Real
decoding replaces that artificial pacing with real consumption, and the backpressure then comes
from the sink rather than from a `nanosleep`.

Four things have to be solved there, in this order:

1. **The frame body is not retained.** The loop streams bytes to the capture file and keeps only
   `apapHeader`; there is no contiguous frame buffer to hand a decoder. One is needed, bounded by
   `SPA_103_APAP_MAX_FRAME`.
2. **Decrypt.** ChaCha20-Poly1305, original 64x64 construction with an 8-byte nonce, body laid out
   `ciphertext || tag[16] || nonce[8]`, key = the SETUP `shk` copied verbatim with no HKDF. AAD is
   `header[4:12]` for queued audio and `header[0:12]` for extension/state packets — try both, as
   `tools/pioneer_apap_decrypt.py` does.
3. **Entry point.** Decoding may only start at an Audio Synchronization Packet, and so may every
   resume after a flush. `APACASPDecoder` is ported for exactly this.
4. **Sink.** `spa_reorder_5_1_decoder_to_declared_s16`, then the fold for index 66 only, then the
   existing PCM6/DIO5 rail. Index 66's native channel order is still unverified — fail it cleanly
   rather than guessing a permutation.

The type-100 PCM6 realtime fallback must keep working exactly as it does today; it is what the car
falls back to.

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
  decompiler prints the latter. The two differ by one float ULP above 1023. **The libm
  question this used to raise is closed** — see stage i below: the pow path has exactly 7168
  reachable inputs and every libm within ~15 bits of correct gives the same float32 for all
  of them, checked against the exact value and executed on QNX.
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
* The decoder the C API hands back is an **`APACASPDecoder`**, so a frame begins with a **two-bit**
  frame type, not the one bit `APACDecoder::DecodeFrame` reads. `APACDecoder_Create` builds the base
  object and then overwrites its vptr. Reading one bit decodes the first packet as an LRVQ frame and
  looks exactly like an LRVQ bug.
* The float32 output is **scaled by 1/32768**. The decode path is S16-scaled internally; the scale
  at decoder+2064 is applied on the float path too.
* `APACIndividualChannelStream::Deserialize`'s third argument, the `bool`, is **never read**. Every
  caller passes it — 1 for both halves of a joint pair, 0 otherwise — and nothing looks at it.
* `APACSyntacticElement::ChannelData(bool)` and `GetICSInfo(bool)` take **`true` for the FIRST
  channel**, and the base-class `ChannelData` ignores the flag entirely.
* `APACICSInfo::Reset` leaves the window-grouping word at **1**, not 0, which is not what a memset
  of the object gives you. The three element `Reset`s all inline it.
* In `APACCoreLBRDecoder::Deserialize` the channel counter is only advanced **inside** the
  bandwidth-extension branch, and the absent-element branch advances it by one whatever the element
  type is — so an absent channel pair skews the index the BWE header is attributed to. The port
  reproduces that rather than fixing it. It is unreachable here: only the LFE is ever absent, and it
  is the last element.
* `APACCACDecoder::Deserialize` decodes out of a 16-bit look-ahead window and gives the surplus back
  with a single `PutBack(16)`; forgetting the put-back loses 16 bits per channel pair. Its
  run-length value `r` means **r + 1 bands**, not r.
* The CAC rotation kernel (0x7b070) **swaps which output each half lands in** when `|gain| >= 0.999`
  — because the quantiser stores the reciprocal gain from there up. Getting that backwards is
  inaudible on quiet frames and wrong everywhere else.
* `APACSynthesisFilterBank::FrequencyToTimeInPlace` is called **directly**, not through a vtable, so
  it cannot be traced by swapping a vtable slot. Everything else on the frame path can be.
* The Makefile had **no header dependencies** until stage d. Changing a constant in
  `include/apac/` recompiled some translation units and not others, and the survivors disagreed
  about a struct's size — which failed as wrong numbers in a differential test, not as a link
  error. `-MMD -MP` now; if you see an inexplicable numeric regression, `make clean` first.
