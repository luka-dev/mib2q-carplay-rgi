# Native AArch64 reference harness for Apple's APAC decoder

Status 2026-08-31. **Working end to end.** Pioneer's APAC decoder runs
*natively* on this Apple Silicon Mac in a `linux/arm64` container, decodes the captured
CarPlay 5.1 stream, and its output matches macOS AudioToolbox to within one 16-bit LSB on
every one of 24 434 688 samples.

```sh
./apac_decoder/reference/run.sh              # full capture + comparison, ~2 s
./apac_decoder/reference/run.sh --count 64   # first 64 packets only
./apac_decoder/reference/run.sh --f32        # float32 out, skips the Apple compare
./apac_decoder/reference/run.sh --probe      # just load the library
./apac_decoder/reference/run.sh ours         # OUR decoder against the library, whole capture
```

## Result

3 977 packets (the 3 zero-length frames in the capture are excluded from both sides;
Apple's converter rejects an empty packet), 4 072 448 frames × 6 channels, S16LE:

| ch | Apple rms | Pioneer rms | rms(diff) | rel | max abs diff | bit-identical samples |
| -: | --------: | ----------: | --------: | --: | -----------: | --------------------: |
| 0 |  1887.12 | 1886.95 | 0.264 | 0.014 % | 1 | 93.04 % |
| 1 |  2179.84 | 2179.58 | 0.593 | 0.027 % | 2 | 64.90 % |
| 2 |  1919.79 | 1919.59 | 0.472 | 0.025 % | 2 | 77.69 % |
| 3 |  1547.58 | 1547.41 | 0.294 | 0.019 % | 1 | 91.33 % |
| 4 |  1744.13 | 1743.95 | 0.297 | 0.017 % | 1 | 91.20 % |
| 5 |  1079.13 | 1079.00 | 0.247 | 0.023 % | 1 | 93.90 % |

**Channel order is identical.** The cross-channel `rms(diff)` matrix is diagonal by three
orders of magnitude (0.25–0.59 on the diagonal, 1250–2900 off it), so Pioneer emits the same
`C L R Ls Rs LFE` order Apple does on this stream. No remapping is applied anywhere in the
harness — the S16 bytes come straight out of the library.

The residual is **purely int16 quantisation, not a decode difference.** Taking Pioneer's
*float32* output and re-quantising it in Python:

| quantisation of Pioneer float | max abs diff vs Apple S16 | identical |
| --- | --: | --: |
| `trunc(x * 32767)` | 1 | 81.7 % |
| `round(x * 32767)` | 1 | 97.1 % |
| `round(x * 32768)` | 1 | **99.989 %** |

Apple rounds to nearest and scales by 32768; the library's built-in S16 path truncates
(`fcvtzs`) and scales positives by 32767. The float paths agree to ~1e-5 full scale.
**For our decoder, compare in float32, not in S16.**

Decode speed, for our budget: 84.8 s of 48 kHz 5.1 audio in 0.79 s wall / 0.15 s user
on an M-series core.

## How the library was made to load

The library is an AArch64 Linux ELF and Docker Desktop here runs an arm64 Linux server, so
`--platform linux/arm64` executes it natively. No Unicorn, no qemu.

Base image is `ubuntu:20.04` — glibc 2.31 and `libicu66`, which is what the Pioneer binaries
were built against. `debian:bullseye` has no libicu66.

Staging (`run.sh`, `stage_libs`): only the `DT_NEEDED` transitive closure of
`libSPF_CP_PlugInWrapper.so` is copied out of the OTA rootfs — 8 files from `usr/local/lib`
(`libSPF_CP_PlugInWrapper`, `libNStandardLib`, `libNStorageLib`, `libNSystemInfoLib`,
`libNPCommonLib`, `libNDeviceLibAV`, `libNEventBaseLib`, `libGparts`) plus `libdns_sd.so`,
`libcrypto.so.3` and `libssl.so.3` from `usr/lib`. Everything else in the rootfs is either
irrelevant or a 10–12 byte broken symlink stub left by the macOS extraction (`lib/libc.so.6`,
`ld-linux-aarch64.so.1`, `libasound.so.2`, …); those are ignored and the container's own
glibc and system libraries are used instead.

Three things had to be fixed, in this order:

1. **`libMali.so`** is a `DT_NEEDED` of `libGparts.so` and is not in the OTA rootfs. An
   *empty* stub is not enough: `libGparts` then fails on `glDrawArrays`, then
   `wl_egl_window_destroy`. The stub is therefore built with real DT_NEEDED entries —
   `gcc -shared -Wl,-soname,libMali.so … -Wl,--no-as-needed -l:libGLESv2.so.2 -l:libEGL.so.1
   -l:libgbm.so.1 -l:libwayland-{egl,client,server,cursor}.so.* -l:libdrm.so.2` — so those
   symbols resolve through the container's Mesa/Wayland packages.
2. **OpenSSL 3.** `libSPF_CP_PlugInWrapper.so` imports `X509_up_ref` and friends without
   listing libcrypto in its own `DT_NEEDED`; on the head unit the *host process* provides it.
   The rootfs's own `libcrypto.so.3` / `libssl.so.3` are linked into the same stub, which puts
   them in the dlopened object's dependency scope. (Ubuntu 20.04's libcrypto is 1.1 and is the
   wrong ABI generation for these binaries.)
3. **The `apluser` group.** This was the last reported blocker and it is real: without it the
   library dies exactly as reported.

   ```
   NStandardLib: Failed to get group info: errno=0, gr_r_size=1024, gr_name=apluser
   NProcSharedMemory::Alloc : Failed to get group info: ...
   NProcSharedMemory::OpenOrAlloc Failed
   Bus error
   ```

   `groupadd -g 1010 apluser && useradd -u 1010 -g apluser -M apluser` in the image fixes it
   completely — no stubbing or `LD_PRELOAD` of `NProcSharedMemory` was needed. Verified by
   deleting the group inside a running container and reproducing the Bus error.

`dlopen` uses `RTLD_LAZY | RTLD_GLOBAL`; `RTLD_NOW` still fails (there are undefined symbols
on paths the APAC code never takes). The Pioneer libs print their own
`NStandardLib: DLL_PROCESS_ATTACHED` banners on load; that is normal and harmless.

## Instrumenting the running library

The library is linked `DT_SYMBOLIC`, so `LD_PRELOAD` cannot interpose its internals. Three
techniques, in increasing order of how much they destroy:

1. **Swap an exported vtable slot** — `reference/scratch/trace_shim.c`. `_ZTV11APACICSInfo`,
   `_ZTV24APACSingleChannelElement`, `_ZTV22APACChannelPairElement`, `_ZTV14APACLFEElement`,
   `_ZTV18APACCoreLBRDecoder` and the rest are all exported, and slot `k + 2` is virtual index `k`.
   The wrapper still calls the real function by its own symbol, so nothing is lost. This is what
   showed that the frame-type field is two bits, not one.
2. **Patch an entry to a constant return** — `reference/scratch/reach_shim.c`. `APAC_STUB` is a
   comma-separated list of mangled names; `MOV W0,#1 ; RET` by default, `MOV W0,#0 ; RET` with
   `APAC_STUB_RET0=1`. Return-1 answers "is this called at all"; return-0 answers "does it read
   bits, or change the output". Both questions are needed — `apac::TrimmingData::Deserialize` is
   called on every frame and cannot change anything, because it is the last thing that reads a bit.
3. **Patch an entry to a jump into our decoder** — `reference/transform_swap_shim.cpp` and
   `reference/spectrum_dump_shim.cpp`. Needed for a function called directly rather than through a
   vtable; `APACSynthesisFilterBank::FrequencyToTimeInPlace` is the one that matters. The library
   is gone, so the shim has to finish the job itself.

Read every mangled name out of the binary with `gobjdump -T`. A hand-built name resolves to nothing
and the experiment then proves whatever you hoped.

## The entry points actually used

`apac::GlobalConfig` and `apac::FrameOutRecord` **did not have to be reverse engineered**,
because the library also exports Apple's plain-C decoder API with its names intact. Those are
the entry points the harness calls. Their signatures were recovered from the disassembly (kept
in `decomp/apac_c_api.asm`).

```c
int32_t APACDecoder_Create(void** handleOut);                     // 0x106800
int32_t APACDecoder_Configure(void* handle,                       // 0x106490
                              const AudioStreamDescription* out,
                              const void* cookie, uint16_t cookieBytes);
int32_t APACDecoder_DecodeFrame(void* handle,                     // 0x105bb0
                                const uint8_t* in, uint32_t inBytes,
                                void* out, uint32_t outCapacity,
                                uint32_t* framesOut);
int32_t APACDecoder_Reset(void* handle);                          // 0x105de0
int32_t APACDecoder_SetParameter(void* handle, ...);              // 0x106260
int32_t APACDecoder_GetParameter(void* handle, ...);              // 0x105e70
void    APACDecoder_Dispose(void* handle);                        // 0x106c10
```

plus six pure cookie parsers, all taking `(const void* cookie, uint16_t byteOffset)`:
`APACDecoder_GetCookieOffset` (takes the cookie *size* in the second argument),
`APACDecoder_GetVersion`, `APACDecoder_GetProfile`, `APACDecoder_SampleRate`,
`APACDecoder_FrameLength`, `APACDecoder_NumberOfChannels`.

### Calling conventions, field by field

**`APACDecoder_Create`** — `calloc(1, 0x858)` (2136 bytes), `new APACDecoder` at `handle[0]`,
a `GlobalConfig` at `handle[8]`, a `std::vector<float>` output buffer at `handle[2096]`.
Returns 0, or `-6705` if `handleOut` is NULL.

**`APACDecoder_Configure`** parses the cookie into `apac::GlobalConfig` via
`TBitstreamReader<unsigned>` and `apac::GlobalConfig::Deserialize`, then validates the
requested output format. Its fourth argument is masked to 16 bits. It logs, on stdout:

```
mAPACBsVersion = 0x0800 mAPACProfile = 31 and mAPACProfileLevel = 1
## APACDecoder_Initialize ## sampleRate = 48000 packetSizePerChannel = 1024  numChannels = 6
```

Those numbers are authoritative for the captured stream. The output descriptor it reads is
32 bytes; only four fields are touched:

| offset | meaning | what Configure does with it |
| -----: | --- | --- |
| +0  | sample rate | not read |
| +4  | format id | not read |
| +8  | format flags | **`== 12` ⇒ S16 output, anything else ⇒ float32.** 12 is `IsSignedInteger\|IsPacked`; the decoder stores 9 (`IsFloat\|IsPacked`) internally as its own native form |
| +12 | bytes per frame | **must equal** bytesPerSample × channels, else `-6748` |
| +16 | frames per packet | not read |
| +20 | bytes per packet | not read |
| +24 | channels per frame | **must equal** the stream's channel count, else `-6748` |
| +28 | bits per channel | stored at `handle+2104` |

**`APACDecoder_DecodeFrame`** — `outCapacity` is compared against `channels * 1024` and must
not be smaller (`-6728` otherwise). Decoding goes through `APACDecoder::DecodeFrame` behind a
vtable, then a small converter at `0x1057f0` writes the output: `memcpy` of `n*4` bytes for
float, or clamp to ±1 and `fcvtzs` for S16. `*framesOut` is 1024 on every packet in this
capture. Passing `inBytes == 0` is *not* an error — it is the flush path, it calls the
decoder's Reset and returns 0 frames. **Allocate the output buffer at 4 bytes per sample even
in S16 mode**: the internal error path memsets `channels*frameLength*4` bytes regardless.

Decoder-object fields worth knowing (offsets from the `handle`):

| offset | contents |
| -----: | --- |
| +2064 | float 1/32768, the S16 scale |
| +2072 | frame length (1024) |
| +2076 | channel count (u16) |
| +2084 | 9 — the decoder's native output format flags (float\|packed) |
| +2096 | `std::vector<float>*` output staging buffer |
| +2104 | bits per channel, from the descriptor |
| +2108 | output format flags, from the descriptor |
| +2112 | 2048 |
| +2120 | frames still to trim (encoder delay). **Zero after Create/Configure**, so no priming is dropped and every packet yields a full 1024 frames — matching Apple's converter |
| +2124 | profile (u16), 31 here |
| +2126 | profile level, 1 here |
| +2128 | "configured" flag; `DecodeFrame` returns `-6718` while it is 0 |

Error codes seen: `-6705` bad argument / cookie shorter than 8 bytes, `-6718` not configured,
`-6728` output buffer too small, `-6748` output format mismatch, `-6762` a C++ exception was
caught inside Configure.

### Magic-cookie layout

`APACDecoder_GetCookieOffset` walks the cookie as a chain of big-endian `[u32 size][u32 type]`
atoms and understands `frma`, `apac` and `dapa`. For a `dapa` atom the config starts at
`atomStart + 12` (i.e. the 8-byte atom header plus 4 more bytes) and runs for `size - 12`
bytes. On the captured 33-byte cookie that gives **offset 12, 21 bytes**, starting with the
`0x0800` APAC sync word — matching the capture README.

**Trap: the metadata helpers must be called at `offset + 2`, not `offset`.**
`GetCookieOffset` returns the start of the `dapa` payload, and that payload *begins with the
16-bit `0x0800` sync word*. GlobalConfig starts two bytes further on. Called on the sync word
the helpers read `0x08007c41` instead of `0x7c41800c` and report 96000 Hz, 32 channels and
frame length 0 — which is how they were first misread here as broken leftovers. They are not
broken - with one real exception. `SampleRate`, `FrameLength` and `NumberOfChannels` agree with
the bitstream exactly once called at `offset + 2`; `GetProfile` reads its six bits one position
late and disagrees, so that one genuinely is wrong and should not be trusted. Their bit ranges (`[20:15]` sample-rate index, `[8:1]` channels, mask `0x7e00` for the
1024-frame test) are exactly the formulas `tools/pioneer_apac_cookie.py` implements, and
`docs/audio/BUFFERED_AUDIO_PATH.md` already documents the same `+2` correction. At the right
offset they return 48000 Hz, 1024 frames and 6 channels, matching `Configure`'s own log line.

`apac_ref` therefore takes the stream geometry from the cookie and only overrides it when
`--channels/--frame-length/--rate` are given explicitly. Do NOT hardcode 6/1024/48000: format
index 66 is an eight-channel stream and would be decoded at the wrong width.

The sample-rate table is at `0x3483a8`:
`96000 88200 64000 48000 44100 32000 24000 22050 16000 12000 11025 8000 7350`.

## What this gives our decoder

`APACDecoder_DecodeFrame` is a whole-frame oracle. For the *per-function* differential testing
our decoder plan asks for, everything needed is exported as well — in particular the bit reader,
which is what would otherwise have blocked calling any `Deserialize` directly:

```
0xb1430  TBitstreamReader<unsigned>::TBitstreamReader(const unsigned char*, unsigned)
0xb14e0  ::GetBits(unsigned)     0xb16b0 ::GetBit()      0xb1760 ::SkipBits(unsigned)
0xb1850  ::AlignToNextByte()     0xb1910 ::PutBack(unsigned)  0xb15b0 ::BitsRemaining() const
```

The object is 24 bytes: `+0` current byte pointer, `+8` end pointer, `+16` 32-bit cache,
`+20` bit count in the cache. Construct it into a zeroed 64-byte buffer and every exported
`X::Deserialize(TBitstreamReader<unsigned>&, ...)` becomes directly callable from the harness.

That is what `apac_fn_ref.cpp` does. It is a sibling of `apac_ref.cpp`, built and run in the
same image:

```sh
./apac_decoder/reference/run.sh fn probe             # resolve every symbol
./apac_decoder/reference/run.sh fn sfb               # apac::GetSFBOffsetTable, all rates
./apac_decoder/reference/run.sh fn cookie CK         # the pure C cookie parsers
./apac_decoder/reference/run.sh fn gc CK             # apac::GlobalConfig::Deserialize
./apac_decoder/reference/run.sh fn scene CK --at 97  # AudioSceneConfig at a bit offset
./apac_decoder/reference/run.sh fn bs  < cases       # TBitstreamReader, one call at a time
./apac_decoder/reference/run.sh fn ics < cases       # APACICSInfo::Deserialize, per case
```

Host paths in the argument list are bind-mounted read-only at the same path inside the
container, so nothing has to be copied by hand. `bs` and `ics` read a case file on stdin and
echo it back with a result line after each case; `apac_decoder/tools/gen_fixtures.sh` uses
that to write the committed expectation files under `apac_decoder/tests/fixtures/`, which is
why `make check` needs no Docker.

Two objects needed help to be constructible without a public constructor:

* **`apac::GlobalConfig`** — `Deserialize` starts with `Reset()`, which walks the two
  `unique_ptr` vectors at `+176` and `+200`; on zeroed memory `begin == end == NULL` and both
  loops are skipped, so a zeroed slab is a legal empty object. What zero is *not* enough for
  is the embedded `CodecConfigData` / `ASCConfigData` sub-objects at `+0` and `+16`, each
  `{vptr, GlobalConfig*}`: `Deserialize` dereferences the back pointer at `+24` near the end.
  Set `+8` and `+24` to the object itself and take the two vtable pointers from the exported
  `_ZTVN4apac12GlobalConfig15CodecConfigDataE` / `...13ASCConfigDataE`, plus 16.
* **`APACICSInfo`** — 768 bytes, but `Deserialize` reads only `+8` and `+9` (the maximum
  scalefactor-band counts, which the syntactic element fills from the SFB table) and writes
  only `+10 ... +16`. A zeroed slab with those two bytes set is enough.

`apac::AudioSceneConfig` needs nothing but eight zeroed bytes, which makes `fn scene` a cheap
way to pin a bit offset: run the library's own parser at a candidate position and see whether
it accepts. On the afconvert-generated cookie exactly one offset in +-4 bits does.

## The composition test: `run.sh swap`

The per-function oracle proves each transform member against the library on synthetic vectors.
`run.sh swap` answers the different question — what our decoder costs **in the finished audio**,
driven by the coefficients the real stream actually produces.

```sh
./apac_decoder/reference/run.sh swap
```

It decodes the capture twice. The second run preloads `transform_swap_shim.cpp`, which patches
the entries of `APACSynthesisFilterBank::FrequencyToTimeInPlace` and `::Reset` with a 16-byte
absolute jump into our filter bank:

```
LDR X16, [PC, #8]   ; 0x58000050
BR  X16             ; 0xd61f0200
.quad target
```

Same technique as `scratch/stub_shim.c`, and for the same reason — the library is `DT_SYMBOLIC`
so `LD_PRELOAD` cannot interpose its internals. Everything except the transform stage is still
the library's code, so any difference in the output is our transform and nothing else.
Our decoder keeps its own overlap state; the library's copy goes stale, which is safe because the
only two functions that read it are the two being replaced.

Result on the full capture — 3977 packets, 4 072 448 frames, 24.4 M samples:

* max difference 2.09e-07 absolute, **3.5e-07 of full scale, −129 dBFS**;
* **1 S16 LSB**, on 0.0059 % of samples;
* agreement with Apple's AudioToolbox **unchanged to four significant figures** — per-channel
  `rms(diff)` 0.2637 / 0.5925 / 0.4724 / 0.2944 / 0.2967 / 0.2471 before, 0.2638 / 0.5925 /
  0.4724 / 0.2944 / 0.2967 / 0.2471 after.

`compare_f32.py` does the float-domain half of that report (per channel, in dBFS and in S16
LSBs, plus the worst 1024-frame block so a localised divergence cannot average away);
`compare_pcm.py` does the Apple comparison as usual.


## Files

| file | contents |
| --- | --- |
| `Dockerfile` | arm64 Ubuntu 20.04 image: deps, `apluser` group/user, `libMali.so` + OpenSSL-3 stub |
| `run.sh` | one command: stage libs → build image → decode → Apple oracle → compare |
| `apac_ref.cpp` | the harness; compiled inside the container on every run so editing it needs no image rebuild |
| `apac_fn_ref.cpp` | the per-function oracle: calls individual library members by mangled name, `run.sh fn ...` |
| `apac_lrvq_ref.cpp` | stage h1's own oracle, driven by `tools/gen_fixtures_lrvq.sh` |
| `apac_bwe_ref.cpp` | stage e's own oracle - `BWELPC` and `APACBWE2Decoder::DecodeFrame`, on objects it builds field by field the way `InitInterFrameMemory` does. Driven by `tools/gen_fixtures_bwe.sh` |
| `compare_pcm.py` | per-channel RMS / max-error / cross-channel permutation report |
| `decomp/apac_c_api.asm` | disassembly of `0x105670`–`0x106e00`, the evidence for every signature above |
| `.build/` | generated: staged libs, packet lists, `apple.pcm`, `pioneer.pcm`. Gitignored |

`run.sh` regenerates `captures/.../apac_packets/` from the raw APAP capture if it is missing,
using `tools/pioneer_apap_decrypt.py` and the `trailing_hex` column of its manifest. The
session key is read straight from `shk.bin` by that tool and is never echoed.

## Still open

* `apac::GlobalConfig` and `apac::FrameOutRecord` layouts are **not** recovered — the C API
  made them unnecessary. If a future test needs `APACDecoder::Initialize` /
  `APACDecoder::DecodeFrame` (the C++ ones) directly, that work is still to do.
* `APACDecoder_SetParameter` / `GetParameter` take a `boost::any`; their parameter ids are not
  mapped. Nothing on the 5.1 path needed them.
* Only index 65 (48 kHz 5.1) has been exercised. Index 66 (5.1.2, eight channels) is untested
  here; the harness should handle it with `--channels 8` once such a capture exists.
* Zero-length packets are excluded from the comparison because Apple's converter rejects them
  while the library treats `inBytes == 0` as a flush. The three in this capture are at
  indices 3496–3498.
* The image pins `ubuntu:20.04` by tag, not by digest.
