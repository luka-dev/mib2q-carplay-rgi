# apac_decoder

Port of Apple's APAC decoder to ARMv7/QNX 6.5 for the MHI2Q CarPlay type-103 path.

Target: CarPlay format index 65 (APAC 48 kHz 5.1) first, index 66 (5.1.2) after. Output feeds
`hook/audio/spatial_downmix.c` and then the existing PCM6/DIO5 rail.

Language is C++ (the original is C++ and the class structure is recovered; GCC 4.9.4 on the QNX
image supports C++11). Keep the QNX traps in mind: no `__thread` in an LD_PRELOAD'd `.so`, and the
build must assert `nm | grep emutls` is empty.

Background and the recovered architecture: [`../docs/reference/APAC_DECODER_PORT.md`](../docs/reference/APAC_DECODER_PORT.md).
Symbol list: [`../docs/reference/apac_symbols_pioneer.txt`](../docs/reference/apac_symbols_pioneer.txt).

## Layout

| path | contents |
| --- | --- |
| `include/`, `src/` | the port itself |
| `tests/` | differential and unit tests |
| `reference/` | native AArch64 harness that runs Pioneer's original decoder, whole frame (`apac_ref`), per function (`apac_fn_ref`), and one file per late stage (`apac_lrvq_ref`, `apac_bwe_ref`) |
| `reference/decomp/` | IDA/Ghidra decompiler output, one file per class |
| `tools/` | vector generation, packet extraction, comparison |
| `vectors/` | generated APAC test vectors and their expected PCM |

## The three oracles

1. **Apple's decoder**, end to end: `build/apple_apac_oracle` (source `tools/apple_apac_oracle.c`)
   decodes raw APAC packets with the real magic cookie into S16LE PCM.
2. **Pioneer's original**, per function: the library is AArch64 Linux and this Mac is arm64, so it
   runs *natively* in a `linux/arm64` container — no emulation, no Unicorn. See `reference/`.
   `reference/run.sh fn ...` calls individual library members by their mangled name;
   `tools/gen_fixtures.sh` turns their answers into the committed fixtures under
   `tests/fixtures/`, so `make check` runs on the host without Docker.
3. **Generated vectors**: `afconvert` encodes any signal to APAC.

   ```sh
   afconvert in.caf out.mp4 -d apac -l MPEG_5_1_A -u csrc 36 -u cdrc 1 -u aspf 75
   ```

   `csrc`, `cdrc` and `aspf` are mandatory; omitting them is what produced the old `!dat` failures.

## Rules for this port

* Nothing lands without a differential test against oracle 1 or 2 on real data.
* Port bottom-up in the order given in `APAC_DECODER_PORT.md`; do not start a stage whose
  dependencies are not passing.
* Tables come out of the binary byte-for-byte. Do not retype them by hand.
* The decoder is float-based. Keep it float; do not invent a fixed-point variant.
