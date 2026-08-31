# APAC decoder — constant tables

Source: `libSPF_CP_PlugInWrapper.so`, Pioneer SPHERA 1.030010 (AArch64 Linux ELF, image base 0).

## Two corrections to the brief

**1. The 48 `.rodata` symbols are not the codec's tables.** 45 of the 48 entries in
`apac_symbols_pioneer.txt` marked `.rodata` are `typeinfo name for ...` strings (`_ZTS`, RTTI
type names). Only three are data, and they total 30 bytes:

| symbol | addr | size | bytes |
|---|---|---:|---|
| `apac::kSupportedProfileLevels` | `0x00330080` | 4 | `1f 00 06 00` |
| `apac::scene::BCP47LanguageString::BCP47LanguageChar::kCharacterDecodingTable_8Bit` | `0x00330888` | 3 | `6a 71 78` |
| `apac::scene::BCP47LanguageString::BCP47LanguageChar::kCharacterDecodingTable_5Bit` | `0x00330890` | 23 | `61 62 63 64 65 66 67 68 69 6b 6c 6d 6e 6f 70 72 73 74 75 76 77 79 7a` |
| `apac::kChannelBedProfileConfigs` | `0x00431e70` | 40 | `1f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00` |
| `apac::scene::kBCP47CharacterCodeTableEntry` | `0x00431e98` | 52 | `08 05 09 05 0a 05 0b 05 0c 05 0d 05 0e 05 0f 05 10 05 f8 08 11 05 12 05 13 05 14 05 15 05 16 05 f9 08 17 05 18 05 19 05 1a 05 1b 05 1c 05 fa 08 1d 05 1e 05` |
| `apac::scene::kBCP47CharacterCodeSection` | `0x00431ed0` | 16 | `00 00 03 02 08 00 05 17 f8 00 08 03 f6 01 09 0a` |
| `g_APAC_ch` | `0x00433b68` | 1 | `04` |

**2. The real tables are unnamed, and most are reached through pointer arrays in
`.data.rel.ro`, not by a direct `ADRP` from code.** `apac::GetHuffmanTable`,
`apac::GetSCFHuffmanTable` and `apac::GetSFBOffsetTable` are three-instruction accessors that
index a relocated pointer array; the arrays in turn point at the `.rodata` payloads. A scan
that only follows `ADRP` out of `.text` finds the accessor's array and stops.

So the table set here was built by: taking every `.rodata`/`.data`/`.data.rel.ro` address
referenced by any of the 334 APAC functions (143 addresses, 61 of them in `.data.rel.ro`),
then walking the `.data.rel.ro` pointer graph transitively. That reaches **333 distinct
objects, 223 of them data (205232 bytes) and 110 C strings**.

### Extents are upper bounds

Only a table's base is ever referenced — the code indexes off it — so the start address is
exact and the length is the distance to the next address referenced from anywhere in the image.
Where several tables sit back to back and only the first is addressed, they merge into one
extent. Two lengths *are* pinned exactly, by identifying the generating function (see below).

Each data object is written byte-for-byte to `rodata/apac_rodata_<addr>.h` (a C array plus a
typed view) and `rodata/apac_rodata_<addr>.bin`. **The byte array is authoritative.** The
element type in the typed view is inferred from the byte pattern and is not proof.

---

## The scalefactor-band offset table — fully recovered

`apac::GetSFBOffsetTable(uint32 sampleRate, uint32 frameLength)` at `0x0009d9d0`:
returns `NULL` unless `frameLength == 1024`, then linearly searches a 12-entry table at
`0x004258f8` for a matching sample rate and returns the record. 40-byte records:

```c
struct SFBOffsetRecord {          /* 40 bytes, 12 records at 0x004258f8 */
    uint64_t        sampleRate;   /* +0                                   */
    uint64_t        numSfbLong;   /* +8                                   */
    const uint16_t *sfbOffsetLong;/* +16  numSfbLong + 1 entries          */
    uint64_t        numSfbShort;  /* +24                                  */
    const uint16_t *sfbOffsetShort;/*+32  numSfbShort + 1 entries         */
};
```
The `+1` is confirmed by arithmetic: for 48 kHz, `sfbOffsetLong = 0x330280` with
`numSfbLong = 49`, and `0x330280 + (49+1)*2 = 0x3302e4`, immediately before the next
referenced address `0x3302e8`. Same for every other record.

| # | sample rate | numSfbLong | sfbOffsetLong | numSfbShort | sfbOffsetShort |
|---:|---:|---:|---|---:|---|
| 0 | 96000 | 41 | `0x00330388` | 12 | `0x00330368` |
| 1 | 88200 | 41 | `0x00330388` | 12 | `0x00330368` |
| 2 | 64000 | 47 | `0x00330308` | 12 | `0x003302e8` |
| 3 | 48000 | 49 | `0x00330280` | 14 | `0x00330260` |
| 4 | 44100 | 49 | `0x00330280` | 14 | `0x00330260` |
| 5 | 32000 | 51 | `0x003301f8` | 14 | `0x00330260` |
| 6 | 24000 | 47 | `0x00330198` | 15 | `0x00330178` |
| 7 | 22050 | 47 | `0x00330198` | 15 | `0x00330178` |
| 8 | 16000 | 43 | `0x00330120` | 15 | `0x00330100` |
| 9 | 12000 | 43 | `0x00330120` | 15 | `0x00330100` |
| 10 | 11025 | 43 | `0x00330120` | 15 | `0x00330100` |
| 11 | 8000 | 40 | `0x003300a8` | 15 | `0x00330088` |

Records 3 (48000) and 5.1/5.1.2 CarPlay use `numSfbLong = 49`, `numSfbShort = 14`.
The actual offset arrays, decoded:

* `0x00330388` — long, 41+1 entries (rate 96000): `0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96, 108, 120, 132, 144, 156, 172, 188, 212, 240, 276, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024`
* `0x00330368` — short, 12+1 entries (rate 96000): `0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128`
* `0x00330308` — long, 47+1 entries (rate 64000): `0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 100, 112, 124, 140, 156, 172, 192, 216, 240, 268, 304, 344, 384, 424, 464, 504, 544, 584, 624, 664, 704, 744, 784, 824, 864, 904, 944, 984, 1024`
* `0x003302e8` — short, 12+1 entries (rate 64000): `0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128`
* `0x00330280` — long, 49+1 entries (rate 48000): `0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 88, 96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896, 928, 1024`
* `0x00330260` — short, 14+1 entries (rate 48000): `0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80, 96, 112, 128`
* `0x003301f8` — long, 51+1 entries (rate 32000): `0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 88, 96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896, 928, 960, 992, 1024`
* `0x00330198` — long, 47+1 entries (rate 24000): `0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124, 136, 148, 160, 172, 188, 204, 220, 240, 260, 284, 308, 336, 364, 396, 432, 468, 508, 552, 600, 652, 704, 768, 832, 896, 960, 1024`
* `0x00330178` — short, 15+1 entries (rate 24000): `0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64, 76, 92, 108, 128`
* `0x00330120` — long, 43+1 entries (rate 16000): `0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 100, 112, 124, 136, 148, 160, 172, 184, 196, 212, 228, 244, 260, 280, 300, 320, 344, 368, 396, 424, 456, 492, 532, 572, 616, 664, 716, 772, 832, 896, 960, 1024`
* `0x00330100` — short, 15+1 entries (rate 16000): `0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60, 72, 88, 108, 128`
* `0x003300a8` — long, 40+1 entries (rate 8000): `0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 172, 188, 204, 220, 236, 252, 268, 288, 308, 328, 348, 372, 396, 420, 448, 476, 508, 544, 580, 620, 664, 712, 764, 820, 880, 944, 1024`
* `0x00330088` — short, 15+1 entries (rate 8000): `0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 128`

---

## The Huffman codebooks

`apac::GetHuffmanTable(uint32 n)` at `0x00099dc0` is `return off_424D98[n];` — a 12-entry
pointer array at `0x00424d98`. `apac::GetSCFHuffmanTable()` at `0x00099e10` returns
`&off_424DF8`, which is entry 0 of the same array, so **codebook 0 is the scalefactor book**.

Each entry points at a two-word descriptor followed by a run of 16-byte records:

```c
struct HuffTable {                /* 16 bytes */
    const HuffEntry *entries;     /* +0  */
    uint64_t         n;           /* +8  see caveat below */
};
struct HuffEntry {                /* 16 bytes */
    uint64_t    a;                /* +0  a small number, 6..31, ascending down the array */
    const void *payload;          /* +8  -> .rodata; payload sizes halve as `a` grows    */
};
```
**Caveat — the field semantics are not established.** `APACScaleFactorData::Deserialize`
computes `0x80000000u >> tbl->n`, so `n` is used as a *bit count*, not only as a length; and
`a` ascends while the payload sizes descend by powers of two, which is the shape of a canonical
Huffman "symbols per code length" table but is not proof of it. Do not port these from a
guessed structure — decode them against the running library.

| idx | descriptor | entries | n | first entry `a` | last entry `a` |
|---:|---|---|---:|---:|---:|
| 0 (SCF) | `0x00424df8` | `0x004257b8` | 19 | 31 | 31 |
| 1 | `0x004256e8` | `0x004256f8` | 11 | 31 | 31 |
| 2 | `0x00425638` | `0x00425648` | 9 | 26 | 31 |
| 3 | `0x00425518` | `0x00425528` | 16 | 31 | 31 |
| 4 | `0x00425438` | `0x00425448` | 12 | 28 | 31 |
| 5 | `0x00425348` | `0x00425358` | 13 | 31 | 31 |
| 6 | `0x00425278` | `0x00425288` | 11 | 28 | 31 |
| 7 | `0x00425198` | `0x004251a8` | 12 | 31 | 31 |
| 8 | `0x004250d8` | `0x004250e8` | 10 | 27 | 31 |
| 9 | `0x00424fc8` | `0x00424fd8` | 15 | 31 | 31 |
| 10 | `0x00424ee8` | `0x00424ef8` | 12 | 26 | 31 |
| 11 | `0x00424e08` | `0x00424e18` | 12 | 25 | 31 |

Full entry lists:

* codebook 0 — descriptor `0x00424df8`, 19 entries at `0x004257b8`:
  `(31, 0x00310248) (29, 0x00310240) (29, 0x00310238) (29, 0x00310230) (28, 0x00310220) (28, 0x00310210) (28, 0x00310200) (28, 0x003101f0) (28, 0x003101e0) (28, 0x003101d0) (28, 0x003101c0) (28, 0x003101b0) (27, 0x00310190) (26, 0x00310150) (27, 0x00310130) (28, 0x00310120) (29, 0x00310118) (30, 0x00310110) (31, 0x00310108)`
* codebook 1 — descriptor `0x004256e8`, 11 entries at `0x004256f8`:
  `(31, 0x003100f8) (28, 0x003100e8) (27, 0x003100c8) (28, 0x003100b8) (27, 0x00310098) (28, 0x00310088) (28, 0x00310078) (28, 0x00310068) (29, 0x00310060) (30, 0x00310058) (31, 0x00310050)`
* codebook 2 — descriptor `0x00425638`, 9 entries at `0x00425648`:
  `(26, 0x00310008) (27, 0x0030ffe8) (27, 0x0030ffc8) (27, 0x0030ffa8) (28, 0x0030ff98) (28, 0x0030ff88) (29, 0x0030ff80) (30, 0x0030ff78) (31, 0x0030ff70)`
* codebook 3 — descriptor `0x00425518`, 16 entries at `0x00425528`:
  `(31, 0x0030ff60) (29, 0x0030ff58) (28, 0x0030ff48) (27, 0x0030ff28) (27, 0x0030ff08) (27, 0x0030fee8) (28, 0x0030fed8) (28, 0x0030fec8) (28, 0x0030feb8) (29, 0x0030feb0) (29, 0x0030fea8) (29, 0x0030fea0) (29, 0x0030fe98) (30, 0x0030fe90) (31, 0x0030fe88) (31, 0x0030fe80)`
* codebook 4 — descriptor `0x00425438`, 12 entries at `0x00425448`:
  `(28, 0x0030fe68) (28, 0x0030fe58) (27, 0x0030fe38) (27, 0x0030fe18) (27, 0x0030fdf8) (27, 0x0030fdd8) (28, 0x0030fdc8) (28, 0x0030fdb8) (29, 0x0030fdb0) (30, 0x0030fda8) (31, 0x0030fda0) (31, 0x0030fd98)`
* codebook 5 — descriptor `0x00425348`, 13 entries at `0x00425358`:
  `(31, 0x0030fd88) (29, 0x0030fd80) (29, 0x0030fd78) (27, 0x0030fd58) (27, 0x0030fd38) (27, 0x0030fd18) (27, 0x0030fcf8) (28, 0x0030fce8) (28, 0x0030fcd8) (29, 0x0030fcd0) (30, 0x0030fcc8) (30, 0x0030fcc0) (31, 0x0030fcb8)`
* codebook 6 — descriptor `0x00425278`, 11 entries at `0x00425288`:
  `(28, 0x0030fca0) (27, 0x0030fc80) (27, 0x0030fc60) (27, 0x0030fc40) (27, 0x0030fc20) (28, 0x0030fc10) (28, 0x0030fc00) (29, 0x0030fbf8) (30, 0x0030fbf0) (30, 0x0030fbe8) (31, 0x0030fbe0)`
* codebook 7 — descriptor `0x00425198`, 12 entries at `0x004251a8`:
  `(31, 0x0030fbd0) (30, 0x0030fbc8) (28, 0x0030fbb8) (27, 0x0030fb98) (27, 0x0030fb78) (27, 0x0030fb58) (28, 0x0030fb48) (28, 0x0030fb38) (29, 0x0030fb30) (30, 0x0030fb28) (30, 0x0030fb20) (31, 0x0030fb18)`
* codebook 8 — descriptor `0x004250d8`, 10 entries at `0x004250e8`:
  `(27, 0x0030faf0) (27, 0x0030fad0) (27, 0x0030fab0) (27, 0x0030fa90) (28, 0x0030fa80) (28, 0x0030fa70) (29, 0x0030fa68) (30, 0x0030fa60) (30, 0x0030fa58) (31, 0x0030fa50)`
* codebook 9 — descriptor `0x00424fc8`, 15 entries at `0x00424fd8`:
  `(31, 0x0030fa40) (30, 0x0030fa38) (28, 0x0030fa28) (26, 0x0030f9e8) (26, 0x0030f9a8) (26, 0x0030f968) (26, 0x0030f928) (26, 0x0030f8e8) (27, 0x0030f8c8) (28, 0x0030f8b8) (28, 0x0030f8a8) (29, 0x0030f8a0) (30, 0x0030f898) (30, 0x0030f890) (31, 0x0030f888)`
* codebook 10 — descriptor `0x00424ee8`, 12 entries at `0x00424ef8`:
  `(26, 0x0030f840) (26, 0x0030f800) (26, 0x0030f7c0) (26, 0x0030f780) (26, 0x0030f740) (27, 0x0030f720) (27, 0x0030f700) (28, 0x0030f6f0) (29, 0x0030f6e8) (29, 0x0030f6e0) (30, 0x0030f6d8) (31, 0x0030f6d0)`
* codebook 11 — descriptor `0x00424e08`, 12 entries at `0x00424e18`:
  `(25, 0x0030f648) (25, 0x0030f5c8) (25, 0x0030f548) (25, 0x0030f4c8) (26, 0x0030f488) (26, 0x0030f448) (27, 0x0030f428) (28, 0x0030f418) (29, 0x0030f410) (29, 0x0030f408) (30, 0x0030f400) (31, 0x0030f3f8)`

---

## Tables whose contents are identified exactly

* **`0x003308c0`, 4096 bytes, `float[1024]`** — read by `APACSpectralData::Deserialize`.
  Entry `i` equals `i^(4/3)` to the last bit of float precision for all 1024 entries
  (checked exhaustively). This is the inverse-quantisation power table. The extent reported
  by the boundary scan is 4272 bytes; the last 176 bytes are a different object, because
  entries past index 1023 do not fit `i^(4/3)`. **Use 1024 entries.**
* **`0x0030f3b8`, uint32** — sample-rate table read by `apac::GlobalConfig::Deserialize`:
  `96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, ...` — the same 12 rates, in the
  same order, as the SFB record table above.
* **`0x00330280` / `0x00330260`** — the 48 kHz long / short scalefactor band offsets, above.

## Tables whose purpose is placed but whose contents are not decoded

* **`0x00332ff8`, ~65536 bytes** — the single largest object, addressed from
  `apac::LSFTools::Reset()`. LSF/LPC codebook data.
* **`0x00317e90`, `0x0031be90`, `0x0031de90`, `0x0031fe90`, `0x00323e90`, `0x00327e90`,
  `0x0032be90`** — seven bases, all addressed from
  `APACLRVQDecoder::lsfsToMdctSpectralEnvelope`, 0x2000/0x4000 apart, forming one contiguous
  ~0x16000-byte block of float32. Some hold ascending Hz-like values (654.57, 1219.46,
  2382.69, ...), others signed coefficient-like values. Which base is selected by what is in
  that function's index arithmetic — read it there, do not assume.
* **`0x0030ec80`, 280 bytes, float32** — 70 ascending values from 0.00012 upward, addressed
  from `APACBWE2Decoder::DecodeFrame`.
* **`0x00330080`, `apac::kSupportedProfileLevels`** — the symbol claims 4 bytes; the
  referenced extent is 864. Read by `apac::ProfileLevelConfig::ValidateProfileLevelData`.

---

## Complete data-object index

| addr | bytes | inferred | reached by | file |
|---|---:|---|---|---|
| `0x00333000` | 32768 | float32 | code 0x000b5a10 apac::LSFTools::Initialize() (+1 more paths) | `rodata/apac_rodata_00333000.h` |
| `0x0033b000` | 32768 | float32 | code 0x000b5a10 apac::LSFTools::Initialize() (+1 more paths) | `rodata/apac_rodata_0033b000.h` |
| `0x00310290` | 16384 | float32 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_00310290.h` |
| `0x0032be90` | 16384 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_0032be90.h` |
| `0x00327e90` | 16384 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_00327e90.h` |
| `0x00323e90` | 16384 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_00323e90.h` |
| `0x0031fe90` | 16384 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_0031fe90.h` |
| `0x00317e90` | 16384 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_00317e90.h` |
| `0x00314290` | 8192 | float32 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_00314290.h` |
| `0x0031de90` | 8192 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_0031de90.h` |
| `0x0031be90` | 8192 | float32 | code 0x0009aa10 APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> con | `rodata/apac_rodata_0031be90.h` |
| `0x003308c0` | 4096 | float32 | code 0x000ab3b0 APACSpectralData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&, APACScaleFa | `rodata/apac_rodata_003308c0.h` |
| `0x00316290` | 4096 | float32 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_00316290.h` |
| `0x00317290` | 2048 | float32 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_00317290.h` |
| `0x00317a90` | 1024 | float32 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_00317a90.h` |
| `0x0030ec80` | 256 | float32 | code 0x00078490 APACBWE2Decoder::DecodeFrame(APACICSInfo const&, APACSpectralData&, unsigned int) | `rodata/apac_rodata_0030ec80.h` |
| `0x003319d0` | 128 | float32 | code 0x000af240 APACTNSData::Apply(APACICSInfo const&, apac::SFBOffsets const&, APACSpectralData&) const | `rodata/apac_rodata_003319d0.h` |
| `0x0032ff00` | 128 | float32 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_0032ff00.h` |
| `0x0030f4c8` | 128 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f4c8.h` |
| `0x0030f548` | 128 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f548.h` |
| `0x0030f5c8` | 128 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f5c8.h` |
| `0x0030f648` | 128 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f648.h` |
| `0x003483a8` | 112 | bytes | code 0x00106150 APACDecoder_SampleRate | `rodata/apac_rodata_003483a8.h` |
| `0x003301f8` | 104 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_003301f8.h` |
| `0x00330280` | 104 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330280.h` |
| `0x00331970` | 96 | uint32 | code 0x000aed10 APACTNSData::APACTNSData(APACInstanceConfig&) (+1 more paths) | `rodata/apac_rodata_00331970.h` |
| `0x00330198` | 96 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330198.h` |
| `0x00330308` | 96 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330308.h` |
| `0x003300a8` | 88 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_003300a8.h` |
| `0x00330120` | 88 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330120.h` |
| `0x00330388` | 88 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330388.h` |
| `0x00332f80` | 64 | float32 | code 0x000b4930 apac::LPCBase::ComputeLPCs(float const*, bool) (+1 more paths) | `rodata/apac_rodata_00332f80.h` |
| `0x00310150` | 64 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310150.h` |
| `0x0030f448` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f448.h` |
| `0x0030f488` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f488.h` |
| `0x0030f740` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f740.h` |
| `0x0030f780` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f780.h` |
| `0x0030f7c0` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f7c0.h` |
| `0x0030f800` | 64 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f800.h` |
| `0x0030f840` | 64 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f840.h` |
| `0x0030f8e8` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f8e8.h` |
| `0x0030f928` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f928.h` |
| `0x0030f968` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f968.h` |
| `0x0030f9a8` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f9a8.h` |
| `0x0030f9e8` | 64 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f9e8.h` |
| `0x00310008` | 64 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310008.h` |
| `0x0030f3b8` | 56 | uint32 | code 0x00097d50 apac::GlobalConfig::Deserialize(TBitstreamReader<unsigned int>&) | `rodata/apac_rodata_0030f3b8.h` |
| `0x00330088` | 32 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330088.h` |
| `0x00330100` | 32 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330100.h` |
| `0x00330178` | 32 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330178.h` |
| `0x00330260` | 32 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330260.h` |
| `0x003302e8` | 32 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_003302e8.h` |
| `0x00330368` | 32 | uint16 | code 0x0009d9d0 apac::GetSFBOffsetTable(unsigned int, unsigned int) (+1 more paths) | `rodata/apac_rodata_00330368.h` |
| `0x00310130` | 32 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310130.h` |
| `0x00310190` | 32 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310190.h` |
| `0x0030f428` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f428.h` |
| `0x0030f700` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f700.h` |
| `0x0030f720` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f720.h` |
| `0x0030f8c8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f8c8.h` |
| `0x0030fa90` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa90.h` |
| `0x0030fab0` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fab0.h` |
| `0x0030fad0` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fad0.h` |
| `0x0030faf0` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030faf0.h` |
| `0x0030fb58` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb58.h` |
| `0x0030fb78` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb78.h` |
| `0x0030fb98` | 32 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb98.h` |
| `0x0030fc20` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fc20.h` |
| `0x0030fc40` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fc40.h` |
| `0x0030fc60` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fc60.h` |
| `0x0030fc80` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fc80.h` |
| `0x0030fcf8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcf8.h` |
| `0x0030fd18` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd18.h` |
| `0x0030fd38` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd38.h` |
| `0x0030fd58` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd58.h` |
| `0x0030fdd8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fdd8.h` |
| `0x0030fdf8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fdf8.h` |
| `0x0030fe18` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe18.h` |
| `0x0030fe38` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe38.h` |
| `0x0030fee8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fee8.h` |
| `0x0030ff08` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff08.h` |
| `0x0030ff28` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff28.h` |
| `0x0030ffa8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ffa8.h` |
| `0x0030ffc8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ffc8.h` |
| `0x0030ffe8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ffe8.h` |
| `0x00310098` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310098.h` |
| `0x003100c8` | 32 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_003100c8.h` |
| `0x00310280` | 16 | float32 | code 0x0009a140 APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&) | `rodata/apac_rodata_00310280.h` |
| `0x00310120` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310120.h` |
| `0x003101b0` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_003101b0.h` |
| `0x003101c0` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_003101c0.h` |
| `0x003101d0` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_003101d0.h` |
| `0x003101e0` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_003101e0.h` |
| `0x003101f0` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_003101f0.h` |
| `0x00310200` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310200.h` |
| `0x00310210` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310210.h` |
| `0x00310220` | 16 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310220.h` |
| `0x0030f418` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f418.h` |
| `0x0030f6f0` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f6f0.h` |
| `0x0030f8a8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f8a8.h` |
| `0x0030f8b8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f8b8.h` |
| `0x0030fa28` | 16 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa28.h` |
| `0x0030fa70` | 16 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa70.h` |
| `0x0030fa80` | 16 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa80.h` |
| `0x0030fb38` | 16 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb38.h` |
| `0x0030fb48` | 16 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb48.h` |
| `0x0030fbb8` | 16 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbb8.h` |
| `0x0030fc00` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fc00.h` |
| `0x0030fc10` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fc10.h` |
| `0x0030fca0` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fca0.h` |
| `0x0030fcd8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcd8.h` |
| `0x0030fce8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fce8.h` |
| `0x0030fdb8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fdb8.h` |
| `0x0030fdc8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fdc8.h` |
| `0x0030fe58` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe58.h` |
| `0x0030fe68` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe68.h` |
| `0x0030feb8` | 16 | float32 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030feb8.h` |
| `0x0030fec8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fec8.h` |
| `0x0030fed8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fed8.h` |
| `0x0030ff48` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff48.h` |
| `0x0030ff88` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff88.h` |
| `0x0030ff98` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff98.h` |
| `0x00310068` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310068.h` |
| `0x00310078` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310078.h` |
| `0x00310088` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310088.h` |
| `0x003100b8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_003100b8.h` |
| `0x003100e8` | 16 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_003100e8.h` |
| `0x0030efd0` | 16 | float32 | code 0x000803c0 std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allo | `rodata/apac_rodata_0030efd0.h` |
| `0x00332ff8` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332ff8.h` |
| `0x00332ff0` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332ff0.h` |
| `0x00332fe8` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332fe8.h` |
| `0x00332fe0` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332fe0.h` |
| `0x00332fd8` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332fd8.h` |
| `0x00332fd0` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332fd0.h` |
| `0x00332fc8` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332fc8.h` |
| `0x00332fc0` | 8 | bytes | code 0x000b5290 apac::LSFTools::Reset() | `rodata/apac_rodata_00332fc0.h` |
| `0x00332f78` | 8 | bytes | code 0x000b5010 apac::LSFTools::LSP_To_LSF(float const*, float*, unsigned short, float) (+2 more paths) | `rodata/apac_rodata_00332f78.h` |
| `0x00330880` | 8 | uint16 | code 0x000a7d50 SC_PresetDescription::SC_PresetCharacteristics::Deserialize(TBitstreamReader<unsigned int>&) | `rodata/apac_rodata_00330880.h` |
| `0x00330888` | 8 | bytes | code 0x000a3270 apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int | `rodata/apac_rodata_00330888.h` |
| `0x00330080` | 8 | uint16 | code 0x0009d8e0 apac::ProfileLevelConfig::ValidateProfileLevelData(std::vector<std::unique_ptr<apac::IASCConfi | `rodata/apac_rodata_00330080.h` |
| `0x00431e48` | 8 | bytes | code 0x0009d070 apac::GetProfileConfig(apac::Profile) | `rodata/apac_rodata_00431e48.h` |
| `0x00310100` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310100.h` |
| `0x00310108` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310108.h` |
| `0x00310110` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310110.h` |
| `0x00310118` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310118.h` |
| `0x00310230` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310230.h` |
| `0x00310238` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310238.h` |
| `0x00310240` | 8 | bytes | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310240.h` |
| `0x00310248` | 8 | uint16 | code 0x00099e10 apac::GetSCFHuffmanTable() (+2 more paths) | `rodata/apac_rodata_00310248.h` |
| `0x0030f3f0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f3f0.h` |
| `0x0030f3f8` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f3f8.h` |
| `0x0030f400` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f400.h` |
| `0x0030f408` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f408.h` |
| `0x0030f410` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f410.h` |
| `0x0030f6c8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f6c8.h` |
| `0x0030f6d0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f6d0.h` |
| `0x0030f6d8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f6d8.h` |
| `0x0030f6e0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f6e0.h` |
| `0x0030f6e8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f6e8.h` |
| `0x0030f880` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f880.h` |
| `0x0030f888` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f888.h` |
| `0x0030f890` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f890.h` |
| `0x0030f898` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f898.h` |
| `0x0030f8a0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030f8a0.h` |
| `0x0030fa38` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa38.h` |
| `0x0030fa40` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa40.h` |
| `0x0030fa48` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa48.h` |
| `0x0030fa50` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa50.h` |
| `0x0030fa58` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa58.h` |
| `0x0030fa60` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa60.h` |
| `0x0030fa68` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fa68.h` |
| `0x0030fb10` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb10.h` |
| `0x0030fb18` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb18.h` |
| `0x0030fb20` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb20.h` |
| `0x0030fb28` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb28.h` |
| `0x0030fb30` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fb30.h` |
| `0x0030fbc8` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbc8.h` |
| `0x0030fbd0` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbd0.h` |
| `0x0030fbd8` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbd8.h` |
| `0x0030fbe0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbe0.h` |
| `0x0030fbe8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbe8.h` |
| `0x0030fbf0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbf0.h` |
| `0x0030fbf8` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fbf8.h` |
| `0x0030fcb0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcb0.h` |
| `0x0030fcb8` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcb8.h` |
| `0x0030fcc0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcc0.h` |
| `0x0030fcc8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcc8.h` |
| `0x0030fcd0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fcd0.h` |
| `0x0030fd78` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd78.h` |
| `0x0030fd80` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd80.h` |
| `0x0030fd88` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd88.h` |
| `0x0030fd90` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd90.h` |
| `0x0030fd98` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fd98.h` |
| `0x0030fda0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fda0.h` |
| `0x0030fda8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fda8.h` |
| `0x0030fdb0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fdb0.h` |
| `0x0030fe78` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe78.h` |
| `0x0030fe80` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe80.h` |
| `0x0030fe88` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe88.h` |
| `0x0030fe90` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe90.h` |
| `0x0030fe98` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fe98.h` |
| `0x0030fea0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fea0.h` |
| `0x0030fea8` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030fea8.h` |
| `0x0030feb0` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030feb0.h` |
| `0x0030ff58` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff58.h` |
| `0x0030ff60` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff60.h` |
| `0x0030ff70` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff70.h` |
| `0x0030ff78` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff78.h` |
| `0x0030ff80` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_0030ff80.h` |
| `0x00310048` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310048.h` |
| `0x00310050` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310050.h` |
| `0x00310058` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310058.h` |
| `0x00310060` | 8 | bytes | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_00310060.h` |
| `0x003100f8` | 8 | uint16 | code 0x00099dc0 apac::GetHuffmanTable(unsigned int) (+4 more paths) | `rodata/apac_rodata_003100f8.h` |
| `0x0030f198` | 8 | uint16 | code 0x000946a0 APACDecoder::Initialize(apac::GlobalConfig const&) | `rodata/apac_rodata_0030f198.h` |
| `0x0030ee00` | 8 | bytes | code 0x0007c100 apac::chan::CoreDecoderConfig::Deserialize(TBitstreamReader<unsigned int>&) | `rodata/apac_rodata_0030ee00.h` |
| `0x0030e870` | 8 | uint16 | code 0x00072e60 apac::AncillaryConfig::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, unsigned int | `rodata/apac_rodata_0030e870.h` |
| `0x00431eda` | 4 | uint16 | code 0x000a3270 apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int | `rodata/apac_rodata_00431eda.h` |
| `0x00431ed6` | 4 | uint16 | code 0x000a3270 apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int | `rodata/apac_rodata_00431ed6.h` |
| `0x00431ed2` | 4 | uint16 | code 0x000a3270 apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int | `rodata/apac_rodata_00431ed2.h` |
| `0x0032fefc` | 4 | uint16 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_0032fefc.h` |
| `0x0032fef8` | 4 | uint16 | code 0x0009b100 APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData | `rodata/apac_rodata_0032fef8.h` |
| `0x00431ede` | 2 | bytes | code 0x000a3270 apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int | `rodata/apac_rodata_00431ede.h` |
| `0x00431ed0` | 2 | bytes | code 0x000a3270 apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int | `rodata/apac_rodata_00431ed0.h` |

## Strings referenced by APAC code

| addr | text |
|---|---|
| `0x0030e7a0` | `vector::_M_default_append` |
| `0x0030e7c0` | `14APACASPDecoder` |
| `0x0030e7d8` | `N5boost3any11placeholderE` |
| `0x0030e7f8` | `N4apac3drc17APACDRCConfigBaseE` |
| `0x0030e818` | `N4apac3drc18DynRangeCompressorE` |
| `0x0030e838` | `N4apac15AncillaryConfigE` |
| `0x0030e858` | `N5boost3any6holderIjEE` |
| `0x0030e8b8` | `vector::_M_realloc_insert` |
| `0x0030e8d8` | `N5boost9exceptionE` |
| `0x0030e8f0` | `N5boost16exception_detail10clone_baseE` |
| `0x0030e918` | `N5boost12bad_any_castE` |
| `0x0030e930` | `N4apac21APACAudioSceneDecoderE` |
| `0x0030e950` | `N4apac14APACDRCDecoderE` |
| `0x0030e968` | `N4apac25APACAncillaryFrameDecoderE` |
| `0x0030e990` | `N5boost16exception_detail19error_info_injectorINS_12bad_any_castEEE` |
| `0x0030e9d8` | `N5boost16exception_detail10clone_implINS0_19error_info_injectorINS_12bad_any_castEEEEE` |
| `0x0030ea30` | `N5boost10wrapexceptINS_12bad_any_castEEE` |
| `0x0030ea60` | `P16DRCConfiguration` |
| `0x0030ea78` | `PKN4apac12GlobalConfigE` |
| `0x0030ea90` | `uint32_t apac::GetEscBits(BitstreamReader&, uint32_t) [with unsigned int ESCBits = 16; unsigned int ESC2Bits = 20; uint32_t = unsigned int; ` |
| `0x0030eb50` | `/home/kawagoe/eclipse_work/workspace/SPF_CP_PlugInWrapper/NPPlugInWrapper/src/Support/ApplePositionalAudioCodec/Source/APACStructs.h` |
| `0x0030ebd8` | `(numBits <= 32)` |
| `0x0030ec08` | `9DFTHandleILb0EE` |
| `0x0030ec20` | `17MultiRadixRealFFT` |
| `0x0030ec38` | `N4apac8AutocorrE` |
| `0x0030ec50` | `15IAPACBWEDecoder` |
| `0x0030ec68` | `15APACBWE2Decoder` |
| `0x0030ed80` | `19APACChannelRemapper` |
| `0x0030edb0` | `N4apac9TCEConfigE` |
| `0x0030edc8` | `N4apac10IASCConfigE` |
| `0x0030ede0` | `N4apac4chan17CoreDecoderConfigE` |
| `0x0030ee08` | `16IAPACBaseDecoder` |
| `0x0030ee20` | `15APACCoreDecoder` |
| `0x0030ee38` | `N4apac3lbr13DecoderConfigE` |
| `0x0030ee58` | `18APACCoreLBRDecoder` |
| `0x0030ee88` | `St11_Mutex_baseILN9__gnu_cxx12_Lock_policyE2EE` |
| `0x0030eeb8` | `St19_Sp_make_shared_tag` |
| `0x0030eed0` | `St16_Sp_counted_baseILN9__gnu_cxx12_Lock_policyE2EE` |
| `0x0030ef08` | `18MP4LoudnessMeasure` |
| `0x0030ef50` | `St23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EE` |
| `0x0030efe0` | `N4apac10DecElementE` |
| `0x0030eff8` | `N4apac14ChanDecElementE` |
| `0x0030f010` | `N4apac12GlobalConfigE` |
| `0x0030f028` | `18SC_SceneController` |
| `0x0030f058` | `P18SC_SceneController` |
| `0x0030f088` | `20APACDecoderInterface` |
| `0x0030f0a0` | `13IContentMixer` |
| `0x0030f0b0` | `11APACDecoder` |
| `0x0030f0c0` | `N5boost3any6holderIPKN4apac12GlobalConfigEEE` |
| `0x0030f0f0` | `N5boost3any6holderIN2CA13ChannelLayoutEEE` |
| `0x0030f120` | `N5boost3any6holderIP16DRCConfigurationEE` |
| `0x0030f150` | `N5boost3any6holderIP18SC_SceneControllerEE` |
| `0x0030f180` | `N5boost3any6holderIDnEE` |
| `0x0030f1a0` | `N4apac23IExtFrameElementDecoderE` |
| `0x0030f1c8` | `N4apac21ExtFrameUnkownDecoderE` |
| `0x0030f1e8` | `uint32_t apac::GetEscBits(BitstreamReader&, uint32_t) [with unsigned int ESCBits = 8; unsigned int ESC2Bits = 16; uint32_t = unsigned int; B` |
| `0x0030f2a8` | `uint32_t apac::GetEscBits(BitstreamReader&, uint32_t) [with unsigned int ESCBits = 6; unsigned int ESC2Bits = 12; uint32_t = unsigned int; B` |
| `0x0030f3a0` | `N4apac13BaseASCConfigE` |
| `0x0030ff68` | `	W	W` |
| `0x00310250` | `11APACICSInfo` |
| `0x00310260` | `27APACIndividualChannelStream` |
| `0x0032fe90` | `N15APACLRVQDecoder7LRVQLPCE` |
| `0x0032feb0` | `N15APACLRVQDecoder11LPMDCT_LONGE` |
| `0x0032fed8` | `N15APACLRVQDecoder10LPMDCT_LTPE` |
| `0x003303e0` | `ERROR: unexpected size of mInteractivityGainsPerASC %d %d
` |
| `0x00330420` | `ERROR: frame size exceeded %d
` |
| `0x00330440` | `ERROR: mTransitionState invalid
` |
| `0x00330468` | `ERROR: No scene component
` |
| `0x00330488` | `ERROR: Inconsistent vector size
` |
| `0x003304b0` | `ERROR: intermediate channel count is too small
` |
| `0x003304e0` | `ERROR: preset index out of range
` |
| `0x00330508` | `ERROR: item index out of range
` |
| `0x00330528` | `ERROR: scene data not found
` |
| `0x00330548` | `ERROR: scene composition data not found
` |
| `0x00330578` | `ERROR: sceneComponentModifiers size mismatch
` |
| `0x003305a8` | `WARNING: presetIndex out of range %d. Using preset 0
` |
| `0x003305e0` | `WARNING: User setting of category failed
` |
| `0x00330610` | `WARNING: User setting of comsposition parameters failed
` |
| `0x00330650` | `vector::reserve` |
| `0x00330660` | `ERROR: decoding language code
` |
| `0x00330680` | `ERROR: Language code exceeds permitted length
` |
| `0x003306b0` | `ERROR: error in extension payload
` |
| `0x003306d8` | `ERROR: Scene item index out of range
` |
| `0x00330700` | `ERROR: Selection set index out of range
` |
| `0x00330730` | `ERROR: Member index out of range
` |
| `0x00330758` | `ERROR: Selection group index out of range
` |
| `0x00330788` | `ERROR: SceneComposition has inconsistent size
` |
| `0x003307b8` | `ERROR: selectedSelectionSetIndex out of range
` |
| `0x003307e8` | `ERROR: SceneComposition payload has missing elements
` |
| `0x00330820` | `ERROR: AudioScenes payload has missing elements
` |
| `0x00330858` | `ERROR: AudioScenes has no global scene
` |
| `0x00330890` | `abcdefghiklmnoprstuvwyz` |
| `0x003318c0` | `16AlignedAllocatorILj16EE` |
| `0x003318e0` | `20APACSyntacticElement` |
| `0x003318f8` | `24APACSingleChannelElement` |
| `0x00331918` | `14APACLFEElement` |
| `0x00331930` | `22APACChannelPairElement` |
| `0x00331950` | `23APACSynthesisFilterBank` |
| `0x00331a50` | `6BWELPC` |
| `0x00331d08` | `16ContentOutBuffer` |
| `0x00332f38` | `N4apac12LPMDCTFilterE` |
| `0x00332f50` | `N4apac7LPCBaseE` |
| `0x00332f60` | `N4apac8LSFToolsE` |
| `0x00348200` | `mAPACBsVersion = 0x%04x mAPACProfile = %d and mAPACProfileLevel = %d  
` |
| `0x00348248` | `## APACDecoder_Initialize ## sampleRate = %d packetSizePerChannel = %d  numChannels = %d 
` |
| `0x003482a8` | `## APACDecoder_Initialize Mismtch channels config:%d format:%d
` |
| `0x003482e8` | `## APACDecoder_Initialize Mismtch bytes per packet config:%d format:%d
` |
| `0x00348330` | `N4apac12GlobalConfig16ComponentConfigsE` |
| `0x00348358` | `N4apac12GlobalConfig15CodecConfigDataE` |
| `0x00348380` | `N4apac12GlobalConfig13ASCConfigDataE` |

---

## The 48 `.rodata` / 4 `.data` symbol-table entries, verbatim

| symbol | addr | size | contents |
|---|---|---:|---|
| typeinfo name for APACASPDecoder | `0x0030e7c0` | 17 | `14APACASPDecoder` |
| typeinfo name for apac::drc::APACDRCConfigBase | `0x0030e7f8` | 31 | `N4apac3drc17APACDRCConfigBaseE` |
| typeinfo name for apac::drc::DynRangeCompressor | `0x0030e818` | 32 | `N4apac3drc18DynRangeCompressorE` |
| typeinfo name for apac::AncillaryConfig | `0x0030e838` | 25 | `N4apac15AncillaryConfigE` |
| typeinfo name for apac::APACAudioSceneDecoder | `0x0030e930` | 31 | `N4apac21APACAudioSceneDecoderE` |
| typeinfo name for apac::APACDRCDecoder | `0x0030e950` | 24 | `N4apac14APACDRCDecoderE` |
| typeinfo name for apac::APACAncillaryFrameDecoder | `0x0030e968` | 35 | `N4apac25APACAncillaryFrameDecoderE` |
| typeinfo name for apac::GlobalConfig const* | `0x0030ea78` | 24 | `PKN4apac12GlobalConfigE` |
| typeinfo name for apac::Autocorr | `0x0030ec38` | 17 | `N4apac8AutocorrE` |
| typeinfo name for IAPACBWEDecoder | `0x0030ec50` | 18 | `15IAPACBWEDecoder` |
| typeinfo name for APACBWE2Decoder | `0x0030ec68` | 18 | `15APACBWE2Decoder` |
| typeinfo name for APACChannelRemapper | `0x0030ed80` | 22 | `19APACChannelRemapper` |
| typeinfo name for apac::TCEConfig | `0x0030edb0` | 18 | `N4apac9TCEConfigE` |
| typeinfo name for apac::IASCConfig | `0x0030edc8` | 20 | `N4apac10IASCConfigE` |
| typeinfo name for apac::chan::CoreDecoderConfig | `0x0030ede0` | 32 | `N4apac4chan17CoreDecoderConfigE` |
| typeinfo name for IAPACBaseDecoder | `0x0030ee08` | 19 | `16IAPACBaseDecoder` |
| typeinfo name for APACCoreDecoder | `0x0030ee20` | 18 | `15APACCoreDecoder` |
| typeinfo name for apac::lbr::DecoderConfig | `0x0030ee38` | 27 | `N4apac3lbr13DecoderConfigE` |
| typeinfo name for APACCoreLBRDecoder | `0x0030ee58` | 21 | `18APACCoreLBRDecoder` |
| typeinfo name for std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2> | `0x0030ef50` | 122 | `St23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EE` |
| typeinfo name for apac::DecElement | `0x0030efe0` | 20 | `N4apac10DecElementE` |
| typeinfo name for apac::ChanDecElement | `0x0030eff8` | 24 | `N4apac14ChanDecElementE` |
| typeinfo name for apac::GlobalConfig | `0x0030f010` | 22 | `N4apac12GlobalConfigE` |
| typeinfo name for APACDecoderInterface | `0x0030f088` | 23 | `20APACDecoderInterface` |
| typeinfo name for APACDecoder | `0x0030f0b0` | 14 | `11APACDecoder` |
| typeinfo name for boost::any::holder<apac::GlobalConfig const*> | `0x0030f0c0` | 45 | `N5boost3any6holderIPKN4apac12GlobalConfigEEE` |
| typeinfo name for apac::IExtFrameElementDecoder | `0x0030f1a0` | 33 | `N4apac23IExtFrameElementDecoderE` |
| typeinfo name for apac::ExtFrameUnkownDecoder | `0x0030f1c8` | 31 | `N4apac21ExtFrameUnkownDecoderE` |
| typeinfo name for apac::BaseASCConfig | `0x0030f3a0` | 23 | `N4apac13BaseASCConfigE` |
| typeinfo name for APACICSInfo | `0x00310250` | 14 | `11APACICSInfo` |
| typeinfo name for APACIndividualChannelStream | `0x00310260` | 30 | `27APACIndividualChannelStream` |
| typeinfo name for APACLRVQDecoder::LRVQLPC | `0x0032fe90` | 28 | `N15APACLRVQDecoder7LRVQLPCE` |
| typeinfo name for APACLRVQDecoder::LPMDCT_LONG | `0x0032feb0` | 33 | `N15APACLRVQDecoder11LPMDCT_LONGE` |
| typeinfo name for APACLRVQDecoder::LPMDCT_LTP | `0x0032fed8` | 32 | `N15APACLRVQDecoder10LPMDCT_LTPE` |
| apac::kSupportedProfileLevels | `0x00330080` | 4 | `1f 00 06 00` |
| apac::scene::BCP47LanguageString::BCP47LanguageChar::kCharacterDecodingTable_8Bit | `0x00330888` | 3 | `6a 71 78` |
| apac::scene::BCP47LanguageString::BCP47LanguageChar::kCharacterDecodingTable_5Bit | `0x00330890` | 23 | `61 62 63 64 65 66 67 68 69 6b 6c 6d 6e 6f 70 72 73 74 75 76 77 79 7a` |
| typeinfo name for APACSyntacticElement | `0x003318e0` | 23 | `20APACSyntacticElement` |
| typeinfo name for APACSingleChannelElement | `0x003318f8` | 27 | `24APACSingleChannelElement` |
| typeinfo name for APACLFEElement | `0x00331918` | 17 | `14APACLFEElement` |
| typeinfo name for APACChannelPairElement | `0x00331930` | 25 | `22APACChannelPairElement` |
| typeinfo name for APACSynthesisFilterBank | `0x00331950` | 26 | `23APACSynthesisFilterBank` |
| typeinfo name for apac::LPMDCTFilter | `0x00332f38` | 22 | `N4apac12LPMDCTFilterE` |
| typeinfo name for apac::LPCBase | `0x00332f50` | 16 | `N4apac7LPCBaseE` |
| typeinfo name for apac::LSFTools | `0x00332f60` | 17 | `N4apac8LSFToolsE` |
| typeinfo name for apac::GlobalConfig::ComponentConfigs | `0x00348330` | 40 | `N4apac12GlobalConfig16ComponentConfigsE` |
| typeinfo name for apac::GlobalConfig::CodecConfigData | `0x00348358` | 39 | `N4apac12GlobalConfig15CodecConfigDataE` |
| typeinfo name for apac::GlobalConfig::ASCConfigData | `0x00348380` | 37 | `N4apac12GlobalConfig13ASCConfigDataE` |
| apac::kChannelBedProfileConfigs | `0x00431e70` | 40 | `1f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00` |
| apac::scene::kBCP47CharacterCodeTableEntry | `0x00431e98` | 52 | `08 05 09 05 0a 05 0b 05 0c 05 0d 05 0e 05 0f 05 10 05 f8 08 11 05 12 05 13 05 14 05 15 05 16 05 f9 08 17 05 18 05 19 05 1a 05 1b 05 1c 05 fa 08 1d 05 1e 05` |
| apac::scene::kBCP47CharacterCodeSection | `0x00431ed0` | 16 | `00 00 03 02 08 00 05 17 f8 00 08 03 f6 01 09 0a` |
| g_APAC_ch | `0x00433b68` | 1 | `04` |

