# APAC spectrum entropy core — established semantics

Scope: `APACLRVQDecoder` (the LRVQ / lattice-residual VQ mode) and `APACSpectralData`
(the transform-mode spectral deserialiser), plus the Huffman codebook structure both of
them and `APACScaleFactorData` share.

**Everything below that is marked *established* was confirmed by running the original
library.** The oracle is `apac_decoder/reference/scratch/`, three scratch drivers that
`dlopen` Pioneer's `libSPF_CP_PlugInWrapper.so` in the existing `linux/arm64` container
and call the exported methods directly:

| driver | what it drives | what it proves |
| --- | --- | --- |
| `lrvq_probe.cpp` | `APACSpectralData::Deserialize`, `APACScaleFactorData::Deserialize` | the Huffman structure and all 12 codebooks |
| `lrvq_fn.cpp` | `APACLRVQDecoder::LrvqDeserialize` | the whole LRVQ frame, both channels |
| `env_probe.cpp` | `dequantizeLrvqGain`, `ltpToMdctSpectralEnvelope`, `lsfsToMdctSpectralEnvelope` | their closed forms |

Each driver contains a **model** written from the reconstruction in this document and
compares it against the library on inputs the model never saw. `dump_codebooks.py` in the
same directory prints the tables straight out of the ELF.

Build and run (from the repo root):

```sh
cd apac_decoder/reference
docker run --rm --platform linux/arm64 -v "$PWD:/src:ro" apac-ref bash -c \
  'g++ -std=c++11 -O2 -o /tmp/p /src/scratch/lrvq_probe.cpp -ldl -lm && /tmp/p 500'
```

---

## 0. Headline result for the port

**On the captured CarPlay 5.1 stream, `LrvqDeserialize` is never called.**

Evidence: `scratch/stub_shim.c` patches a chosen function's entry to `MOV W0,#1 ; RET` and
the capture is decoded with and without the patch. Stubbing `LrvqDeserialize` leaves all
48 869 376 bytes of output from all 3 977 packets **byte-identical**; stubbing
`APACSpectralData::Deserialize` or `APACScaleFactorData::Deserialize` changes the output.
So the CarPlay stream is entirely transform/SQ coded, and LRVQ is a different frame mode
(`APACChannelPairElement::DeserializeLrvqFrame`) that this content does not use.

That does not make LRVQ dead code for the port — a different iOS build or bitrate may emit
it, and the return value is propagated as a hard error — but it is not on the critical path
and should be scheduled after the transform path works.

Note for anyone planning to instrument the library: **it is linked `DT_SYMBOLIC`**, so
`LD_PRELOAD` cannot interpose its internals. Patch the entry in memory instead, as
`stub_shim.c` does.

---

## 1. The Huffman codebooks — *established*

### 1.1 What the fields are

`apac::GetHuffmanTable(n)` (`0x99dc0`) is `return off_424D98[n]`, a 12-entry pointer array.
`apac::GetSCFHuffmanTable()` returns entry 0, so **codebook 0 is the scalefactor book** and
codebooks 1..11 are the spectral books.

```c
struct HuffTable {           /* 16 bytes */
    const HuffEntry *entries;  /* +0 : n+1 records — n usable, plus one guard */
    uint64_t         n;        /* +8 : the longest run of leading 1 bits a code may have */
};
struct HuffEntry {           /* 16 bytes */
    uint64_t        a;         /* +0 : a RIGHT-SHIFT amount, 25..31, not a count       */
    const uint16_t *payload;   /* +8 : 1 << (31 - a) uint16 records                    */
};
```

**`n` is not an element count and `a` is not a length.** The code is a *unary-prefix* code:

* `n` bounds the unary prefix. Both decoders build a clamp from it — `0x80000000u >> n` in
  `APACScaleFactorData::Deserialize`, `(1u << (32 - n)) - 1` in `APACSpectralData::Deserialize`
  — and take `clz(clamp | ~w)`, which is `min(leading_ones(w), n)`. The two forms are
  different constants with the same `clz`.
* `a` is the shift that turns the bits *after* the prefix into an index:
  `idx = (uint32_t)(w << k) >> a`, giving `31 - a` index bits (the top bit of `w << k` is
  the prefix's terminating `0`, so it is always clear for `k < n`).
* Each `HuffEntry` therefore owns one prefix length, and its payload has exactly
  `1 << (31 - a)` uint16 slots. Codes shorter than the maximum appear as repeated slots.

The payload word is:

```
bits  5..0   codeword length in bits, INCLUDING the k ones and the terminating 0
bits 15..6   the symbol, laid out differently per codebook family (§1.3)
```

Decoding one symbol, exactly as the library does it:

```c
uint32_t clamp = 0x80000000u >> t->n;      /* or (1u<<(32-t->n))-1; same clz */
int      k     = clz32(clamp | ~w);        /* leading 1s, capped at n        */
const HuffEntry *e = &t->entries[k];
uint16_t r   = e->payload[(uint32_t)(w << k) >> e->a];
int      len = r & 0x3f;
w <<= len;  avail -= len;                  /* symbol = r >> 6                */
```

`entries[n]` is a **real, in-bounds guard record**, not padding: it carries the single
all-ones code `1^n`, always with `a = 31` (one payload slot) and `len == n`. Counting it,
the Kraft sum of every book is exactly 1. Counting only `entries[0..n-1]` it is `1 - 2^-n`.
Both were computed over every book.

Odd-numbered books (0,1,3,5,7,9) have a 1-bit code `0` at `entries[0]`; even books and book
11 do not. That is why the odd spectral books get a fast path (§2.3).

| book | n | prefix records | distinct codes (incl. guard) | code lengths |
| ---: | --: | --: | --: | --- |
| 0 (SCF) | 19 | 20 | 121 | 1..19 |
| 1 | 11 | 12 | 81 | 1..11 |
| 2 | 9 | 10 | 81 | 3..9 |
| 3 | 16 | 17 | 81 | 1..16 |
| 4 | 12 | 13 | 81 | 4..12 |
| 5 | 13 | 14 | 81 | 1..13 |
| 6 | 11 | 12 | 81 | 4..11 |
| 7 | 12 | 13 | 64 | 1..12 |
| 8 | 10 | 11 | 64 | 3..10 |
| 9 | 15 | 16 | 169 | 1..15 |
| 10 | 12 | 13 | 169 | 4..12 |
| 11 | 12 | 13 | 289 | 4..12 |

### 1.2 The bit window both decoders use

Neither `Deserialize` pulls single fields out of `TBitstreamReader`. Each opens a local
32-bit MSB-aligned window `w` with `avail` valid bits, tops it up with one `GetBits` when it
runs low, and gives the surplus back at the end:

```c
uint32_t w = 0; int avail = 0;
void need(int k) { if (avail < k) { if (avail < 32) w |= GetBits(bs, 32 - avail); avail = 32; } }
/* ... decode ... */
PutBack(bs, avail);
```

The `k` passed to `need()` differs per decode path (§2) but only has to be an upper bound on
one tuple's consumption; the trailing `PutBack` makes the reader's final position
independent of it.

### 1.3 Symbol layout per codebook family — *established*

Field extraction is by codebook range. `X` below is `flt_3308C0`, the `i^(4/3)` table
(§4), and `sf` is the band's scalefactor.

| books | tuple | fields in `r` | signs |
| --- | --- | --- | --- |
| 1, 2 | 4 coefficients | `sext2(r>>14)`, `sext2(r>>12)`, `sext2(r>>10)`, `sext2(r>>8)` | none — the value is signed in the table, range −1..+1 |
| 3, 4 | 4 coefficients | `(r>>14)&3`, `(r>>12)&3`, `(r>>10)&3`, `(r>>8)&3` | one bit from the stream after each **non-zero** magnitude, in order; range 0..2 |
| 5, 6 | 2 coefficients | `sext4(r>>11)`, `sext4(r>>6)` | none; range −4..+4 |
| 7, 8 | 2 coefficients | `(r>>11)&0xF`, `(r>>6)&0xF` | one bit after each non-zero; range 0..7 |
| 9, 10 | 2 coefficients | `(r>>11)&0xF`, `(r>>6)&0xF` | one bit after each non-zero; range 0..12 |
| 11 | 2 coefficients | `(r>>11)&0x1F`, `(r>>6)&0x1F` | one bit after each non-zero; range 0..16, **16 = escape** |
| 0 (SCF) | 1 delta | `(int16_t)r >> 6` | none; range −47..+60 |

Bits 7..6 of `r` are unused in books 1..4. The ranges above are the *observed* extents over
every payload word of every book, and each sits exactly inside what its decode path can
address — that is what pins the field widths.

`X` is addressed at **negative** indices by books 1..6. That is legal: the 20 bytes before
`0x3308c0` hold `X[-4..-1] = -4^(4/3), -3^(4/3), -2^(4/3), -1.0`, a mirrored negative wing
(and `X[-5] = X[-6] = 0`, then unrelated data). A port must reproduce that wing, or branch
on the sign.

### 1.4 Codebook 11's escape — *established*

Magnitude 16 means "read an Elias-gamma-like escape", inlined in `Deserialize` (it is *not*
`apac::GetEscBits`, which is a different, three-stage all-ones escape used by the config
parsers):

```c
if (avail <= 20) need(32);
uint32_t k  = clz32(~w | 0x800000);   /* leading 1s, capped at 8 */
uint32_t nb = k + 4;
uint32_t t  = w << (k + 1);           /* skip the k ones and the terminating 0 */
avail -= (k + 1) + nb;
uint32_t v  = (1u << nb) + (t >> (32 - nb));
w = t << nb;
mag = (v > 1023) ? (float)pow((double)v, 4.0/3.0) : X[v];
```

The exponent really is the `double` nearest `4/3` — the code materialises
`0x3FF5555555555555` into `D9` at `0xab448`. Using `1.33333333` instead reproduces the
library on ordinary input but differs by 1 float ULP on escapes above 1023; that was the
only mismatch in the first oracle run.

**Both sign bits are read before either escape.** Order per tuple: Huffman code, sign for
coefficient 0 if its magnitude is non-zero, sign for coefficient 1 if its magnitude is
non-zero, then escape for coefficient 0 if it was 16, then escape for coefficient 1.

### 1.5 Evidence

`scratch/lrvq_probe.cpp`. For each codebook 1..11 it builds an `APACICSInfo`,
`APACScaleFactorData` and `apac::SFBOffsets` in memory (16 bands × 8 coefficients, one
window group), feeds a **random** 2048-byte bitstream to the real
`APACSpectralData::Deserialize` and to the model, and compares all 128 output floats with
`memcmp` plus the exact number of bits consumed (`BitsRemaining` before/after).

```
cb  1 .. cb 11: 500/500 trials matched (floats bit-exact + bit count)
ALL MATCH (5500 comparisons, 0 failures)
```

For codebook 0 it drives the real `APACScaleFactorData::Deserialize` and compares all 120
scalefactor floats, all 120 `(codebook,run)` records, the per-group record counts, the
return value and the bit count, in three configurations:

```
SCF long  (1 group, 5-bit runs, random payload):   400/400 matched (209 decoded without error)
SCF short (1 group, 3-bit runs, random payload):   400/400 matched (236 decoded without error)
SCF short (8 groups, 3-bit runs, all-zero deltas): 400/400 matched (400 decoded without error)
```

The random bitstreams were generated after the hypothesis was fixed and were never
inspected while forming it.

---

## 2. `APACSpectralData::Deserialize` — *established*

`0xab3b0`. Hex-Rays produced clean output for this one; the reconstruction below is the
decompilation plus the field semantics of §1, and it is what the oracle confirms.

### 2.1 The structures it reads

Recovered from the access pattern and confirmed by driving the function with hand-built
objects:

```c
struct APACScaleFactorData {   /* 728 bytes */
    float    scf[120];         /* +0    stride 15 per window group      */
    uint8_t  nrec[8];          /* +480  (codebook,run) records per group */
    uint16_t rec[120];         /* +488  stride 15; low byte = codebook, high byte = band run */
};
struct SFBOffsets  { uint16_t numBands; /* +0 */ const uint16_t *offset; /* +8 */ };
struct APACSpectralData { float *begin; /* +0 */ float *end; /* +8 */ };
```

`APACICSInfo` fields this function touches: `+10` (byte; `== 2` selects the eight-short-window
layout), `+11` (byte, total scalefactor bands; `0` means "zero the whole spectrum"), `+12`
(byte, window-group count), `+16` (dword, 8 nibbles = the window count of each group).

### 2.2 Structure

```c
for (g = 0; g < ics[12]; ++g) {
    groupLen = (g <= 7) ? ((uint32_t *)ics)[4] >> (4*g) & 0xF : 0;
    dst = (groupLen == 1) ? spec->begin + coefPerWindow*windowIdx    /* long: straight out */
                          : scratch4096 + coefPerWindow*windowIdx;   /* short: staged     */
    windowIdx += (groupLen == 1) ? 1 : groupLen;
    band = 0; coef = 0;
    for (r = 0; r < sf->nrec[g]; ++r) {
        cb  = sf->rec[15*g + r] & 0xff;
        run = sf->rec[15*g + r] >> 8;
        band += run;
        if (cb == 0) { zero out to offset[band]*groupLen; continue; }
        for (b = band-run+1; b <= band; ++b) {
            sfv  = sf->scf[15*g + b - 1];
            stop = offset[b] * groupLen;
            decode tuples until coef == stop, per §1.3;
        }
    }
    if (numBands > band) memset(dst, 0, 4*(coefPerWindow*groupLen - coef));
}
if (ics[10] == 2) de-interleave the staged short windows into spec->begin;
PutBack(bs, avail);
```

The de-interleave at the end walks the bands and, for each window `j` of a group of `n`
windows, copies `offset[b+1]-offset[b]` floats from
`scratch[4*(offset[b]*n + j*(offset[b+1]-offset[b])) + windowBase]` into the output — i.e.
the short windows are coded band-interleaved and unpacked window-major.

### 2.3 The per-tuple zero flag

For **odd** codebooks 1..10 (`rec & 1`, so 1,3,5,7,9) the tuple is preceded by the leading
bit of the window: if it is `0` the whole tuple is zero and exactly one bit is consumed; if
it is `1` the Huffman code follows and *the `1` is part of the code*. Codebook 11 has no
such flag. This is the same `0` code that `entries[0]` carries in the odd books, hoisted
into a fast path.

Refill thresholds per family (`need(k)` from §1.2), reproduced for exactness:
books 1,2 → `n`; books 3,4 → `n + 4`; books 5,6 → `n`; books 7..10 → `n + 2`;
book 11 → `n + 2`, plus `avail <= 20 → need(32)` before each escape.

### 2.4 Where the `i^(4/3)` table enters

Every reconstructed coefficient is `X[q] * sf`, where `q` is the signed quantised value from
§1.3 and `sf` is the band's scalefactor float. There is no separate inverse-quantisation
pass — `APACSpectralData::Deserialize` dereferences `flt_3308C0` inline in all five decode
paths (`vmulq_n_f32` for the quad books, scalar for the pairs). Escapes above 1023 use
`pow(v, 4/3)` instead of the table.

The scalefactor floats themselves come out of `APACScaleFactorData::Deserialize` already in
linear form: `sf = 2^((v-100)/4)`, built without a `pow` as
`bits(twoPowQuarter[v & 3]) + ((v-100) >> 2) << 23`, where `twoPowQuarter` is
`{1.0, 2^0.25, 2^0.5, 2^0.75}` at `0x310280`. `v` is an 8-bit start value plus the running
book-0 deltas, clamped to `[-256, 255]` with the two out-of-range values precomputed
(`0x13000000` = `2^-89`, `0x52D744FD` = `2^38.75`). The run lengths use 3-bit fields for
short blocks and 5-bit for long, escape-coded on all-ones.

---

## 3. `APACLRVQDecoder::LrvqDeserialize` — *established*

`0x9b100`, 3264 bytes, 811 instructions, no usable Hex-Rays output. Reconstructed from the
disassembly and confirmed against the library.

### 3.1 Signature — Hex-Rays got it wrong

**It is a static function.** `X0` holds the `TBitstreamReader`, not a `this`; the
decompiler invented a `this` and shifted every argument by one, which is why its pseudocode
is unusable. The same mistake affects `SpectrumDeserializeSCE/CPE/LFE`,
`dequantizeLrvqGain`, `lsfsToMdctSpectralEnvelope` and `ltpToMdctSpectralEnvelope`.

```c
int32_t LrvqDeserialize(
    TBitstreamReader<uint32_t> &bs,   /* X0 */
    bool      eightShortWindows,      /* W1 : the caller passes (ics[10] == 2)          */
    bool      firstChannel,           /* W2 : 1 for an SCE and for channel 0 of a CPE   */
    APACSpectralData &spec,           /* X3 : &icsStream + 0x2F0                        */
    float     env  [1024],            /* X4 : LPC/LSF envelope, shared across a CPE     */
    float     env2 [1024],            /* X5 : LTP envelope                              */
    float     work [256],             /* X6 : VQ residual, shared across a CPE          */
    int16_t   qidx [64],              /* X7 : 32 (stage0,stage1) index pairs, shared    */
    uint16_t *prevMode);              /* [sp]: last frame's 4-bit VQ mode, shared       */
```

Returns 0 on success, 1 on a malformed VQ mode, or the sub-function's error otherwise.

The `+0x2F0`/`+0x2F8` pointer pair that `NOTES.md` §"Things in the output that are wrong"
flags as suspect on `APACICSInfo` is in fact `APACIndividualChannelStream + 0x2F0`: the
`APACSpectralData` (begin/end) of that channel's spectrum. The callers zero it before the
call. That resolves that open item.

### 3.2 Bitstream syntax

Read straight from the reader (no local window; the function does not `PutBack`).

```
gainMode         u(1)
gainIndex        u(9)
if (!firstChannel) {
    reuseEnvelope   u(1)                 // 1 = keep the previous channel's `env`
    if (!eightShortWindows)
        noLtpSection u(1)                // 1 = the LTP block below is absent
    reuseResidual   u(1)                 // 1 = keep the previous channel's `work`
}
if (firstChannel || !reuseEnvelope) {
    lsf[0..4]       u(9) each
    if (eightShortWindows) shortLsfBooks u(1)
}
if (!eightShortWindows && !noLtpSection) {
    ltpPresent      u(1)
    if (ltpPresent) { ltpGainIndex u(7);  ltpLag u(10) }
}
if (!reuseResidual) {
    vqMode          u(4)
    for (k = 0; k < 32; ++k) {
        if (sameMode) { reuse0 u(1); if (!reuse0) idx0 u(bits0) }  else idx0 u(bits0)
        if (stage1 present) {
            if (sameMode) { reuse1 u(1); if (!reuse1) idx1 u(bits1) } else idx1 u(bits1)
        }
    }
}
```

`sameMode` is `!firstChannel && *prevMode == vqMode`, evaluated before `*prevMode` is
overwritten. When it holds, each index may be replaced by the *same slot* of the previous
channel's `qidx`, one flag bit each — that is the inter-channel prediction, and it is the
only reason `qidx` and `prevMode` are passed in.

`shortLsfBooks` is read only in the eight-short-window case, and it does double duty: it is
the `bool` argument to `lsfsToMdctSpectralEnvelope` **and** it selects the short-block output
layout in §3.5. A channel that sets `reuseEnvelope` never reads it and therefore always
takes layout 0.

### 3.3 The VQ mode table at `0x42b1f8` — *established*

16 records of 32 bytes:

```c
struct LrvqMode {
    uint16_t index;      /* +0  == the row number                                  */
    uint16_t lo, hi;     /* +2,+4  a bit-budget range: 0,288,320,384,...,608,9999   */
    uint16_t totalBits;  /* +6  == bits0 + bits1                                    */
    const float *cb0;    /* +8                                                      */
    const float *cb1;    /* +16 NULL for rows 0..3                                  */
    int16_t bits0, n0;   /* +24,+26   n0 == 1 << bits0                              */
    int16_t bits1, n1;   /* +28,+30   n1 == 1 << bits1, 0 when cb1 is NULL          */
    /* bits0/bits1 are loaded with LDRSH, n0/n1 with LDRH; the bound check is then a
       signed compare of the zero-extended n against the sign-extended index.        */
};
```

| row | bits0 / n0 / cb0 | bits1 / n1 / cb1 | total |
| --: | --- | --- | --: |
| 0 | 6 / 64 / `0x317290` | — | 6 |
| 1 | 7 / 128 / `0x316290` | — | 7 |
| 2 | 8 / 256 / `0x314290` | — | 8 |
| 3 | 9 / 512 / `0x310290` | — | 9 |
| 4 | 6 / 64 / `0x317290` | 5 / 32 / `0x317a90` | 11 |
| 5 | 7 / 128 / `0x316290` | 5 / 32 / `0x317a90` | 12 |
| 6 | 7 / 128 / `0x316290` | 6 / 64 / `0x317290` | 13 |
| 7 | 8 / 256 / `0x314290` | 6 / 64 / `0x317290` | 14 |
| 8 | 8 / 256 / `0x314290` | 7 / 128 / `0x316290` | 15 |
| 9 | 9 / 512 / `0x310290` | 7 / 128 / `0x316290` | 16 |
| 10 | 9 / 512 / `0x310290` | 8 / 256 / `0x314290` | 17 |
| 11..15 | — | — | invalid, `lo=hi=999`, `totalBits=99` |

Rows 11..15 have both pointers NULL; the function returns 1 for them (two separate checks:
`cb0|cb1 == 0` and `bits0|bits1 == 0`).

The codebooks are one contiguous block `0x310290 .. 0x317e90`, **992 vectors of 8 floats**
(512 + 256 + 128 + 64 + 32), dense trained VQ, no exploitable structure — copy the bytes.
`lo`/`hi` are not read by this function; they are how the encoder picks the mode from a bit
budget.

### 3.4 The VQ loop

```c
for (k = 0; k < 32; ++k) {
    idx0 = /* per §3.2 */;
    if ((int)(uint16_t)mode.n0 < (int)idx0) return 1;   /* LDRH then signed compare */
    if (mode.cb1 == NULL) {
        for (n = 0; n < 8; ++n) work[32*n + k] = mode.cb0[8*idx0 + n];   /* + 0.0f */
        idx1 = -1;                                   /* qidx keeps the sentinel */
    } else {
        idx1 = /* per §3.2 */;
        if ((uint16_t)mode.n1 < (int)idx1) return 1;
        for (n = 0; n < 8; ++n)
            work[32*n + k] = (mode.cb0[8*idx0 + n] + mode.cb1[8*idx1 + n]) * 0.5f;
    }
    qidx[2*k] = idx0; qidx[2*k + 1] = idx1;
}
```

The two shapes are `vDSP_vasm(cb0, 1, cb1, 1, &0.5f, work + k, 32, 8)` and
`vDSP_vsadd(cb0, 1, &0.0f, work + k, 32, 8)`; the scalars are `0.5f` at `0x32fef8` and
`0.0f` at `0x32fefc`. So `work` is an 8 × 32 matrix in row-major order: row `n` is
VQ component `n`, column `k` is the VQ vector index.

The bound checks compare against `n0`/`n1`, which equal `1 << bits`, so a freshly read index
can never fail them. They only bite on the `sameMode` reuse path, where `qidx` may still hold
`-1` from a single-stage frame — see §6.

### 3.5 Reconstruction and output

```c
if (eightShortWindows) {
    for (i = 0; i < 256; ++i) work[i] /= env[i];
    if (shortLsfBooks) {                         /* layout 1 */
        for (i = 0; i < 256; ++i) work[i] *= gain;
        for (w = 0; w < 8; ++w) {
            float *src = work + 32*w, *dst = spec.begin + 128*w;
            if (w & 1) memcpy(dst, src, 128);                  /* forward  */
            else       for (i = 0; i < 32; ++i) dst[i] = src[31 - i];   /* reversed */
            for (i = 0; i < 32; ++i) dst[i] /= ramp32[i];
        }
    } else {                                     /* layout 0 */
        for (w = 0; w < 8; ++w)
            for (n = 0; n < 32; ++n) spec.begin[128*w + n] = work[w + 8*n] * gain;
    }
} else {
    if (ltpPresent) for (i = 0; i < 256; ++i) env[i] *= env2[i];
    for (i = 0; i < 256; ++i) work[i] /= env[i];
    for (i = 0; i < 256; ++i) spec.begin[i] = work[i] * gain;
}
```

LRVQ writes **256 coefficients** in the long case and 8 × 32 in the short case; the rest of
the 1024-coefficient spectrum is left to the caller (which zeroed it before the call).

`ramp32` is `float[32]` at `0x32ff00`, a linear ramp from `1.0` to `2.0`. The stored values
are `1 + i/31` rounded to four decimals (`1.0323`, `1.0645`, `1.0968`, …), not the exact
quotients — **copy the bytes**.

`env` holds the *analysis* filter magnitude, which is why the reconstruction divides:
`lsfsToMdctSpectralEnvelope` returns `|A(e^{jω})|` and `ltpToMdctSpectralEnvelope` returns
`|1 - g·e^{-jω·lag}|`, and dividing by their product applies both synthesis filters at once.

### 3.6 The `reuseResidual` flag is overloaded

The stack slot the flag lands in (`var_94`) means two different things at two points, and a
port must keep both:

* before the VQ section — "no VQ data follows; keep the previous channel's `work`". Reached
  only from the `!firstChannel` header.
* after the VQ section — "an LTP envelope was decoded; multiply it into `env`". Set from
  `ltpPresent` when the first meaning did not apply.

The transition is in the LTP block: if `reuseResidual` was already set, the function decodes
the LTP envelope (if present), multiplies it in and jumps straight to the output, skipping
the VQ entirely. Otherwise it stores `ltpPresent` in the same slot and continues.

Consequence for the port: a second channel with `reuseResidual = 1` reuses `work` **as it
was left by the previous channel — that is, already divided by that channel's `env` and, in
the short case, already multiplied by that channel's gain** — and then divides it again by
its own `env`. That is what the library does; the model reproduces it and matches
bit-exactly. Whether it is what Apple intended is not something this analysis can say.

### 3.7 Evidence

`scratch/lrvq_fn.cpp` builds a syntactically valid two-channel (CPE-shaped) bitstream with
randomised fields, then calls the real `LrvqDeserialize` twice (channel 0 then channel 1)
and the model twice on the same bits, and compares:

* the return value of each call,
* the exact bit count consumed across both calls,
* `*prevMode`,
* all 64 entries of `qidx`,
* all 256 floats of `work`,
* all 1024 floats of `env`,
* all 1024 floats of `spec` — everything with `memcmp`, so bit-exact.

```
coverage: single-stage=328 two-stage=547 error-mode=132 same-mode-reuse=501
          ltp=240 reuse-residual=236 shortRev=221 shortFwd=279
ALL MATCH: 1000/1000 comparisons matched (875 reached the second channel cleanly)
```

Every branch listed in §3.2–§3.5 is exercised by that coverage line.

---

## 4. The three other functions Hex-Rays could not decompile

All three are **static**; Hex-Rays inserted a phantom `this` in each and mis-assigned the
arguments.

### 4.1 `dequantizeLrvqGain(uint16 gainIndex, uint16 gainMode, float *out)` — *established*

`0x9a950`, 45 instructions.

```c
float t = (float)gainIndex / 511.0f;                    /* 0x43FF8000 */
float g = (gainMode == 1) ? powf(10.0f, t + t) * 10.0f  /* = 10^(1 + 2t) */
                          : 10.0f / powf(10.0f, t * 4.0f); /* = 10^(1 - 4t) */
*out = g * 65536.0f;                                    /* 0x47800000 */
return 0;
```

**Evidence:** `scratch/env_probe.cpp` calls the library for all 512 indices × both modes and
compares the returned `float` with `memcmp`: **1024/1024 bit-exact.**

### 4.2 `ltpToMdctSpectralEnvelope(uint16 gainIndex, uint16 lag, float out[1024])` — *established*

`0x9aee0`, 536 bytes. Builds a 2048-tap impulse response `1 - g·z^{-lag}`, takes its FFT
magnitude, keeps 256 bins and normalises by `1/(1+g)`, then zeroes `out[256..1023]`.

That whole thing has a closed form, which is what a port should use:

```c
float g = ((float)gainIndex / 127.0f) * 0.74f + 0.25f;   /* 0.25 .. 0.99 */
for (int k = 0; k < 256; ++k) {
    double w = 2.0 * M_PI * k * lag / 2048.0;
    out[k] = (float)(sqrt(1.0 - 2.0*g*cos(w) + (double)g*g) / (1.0 + g));
}
memset(out + 256, 0, 768 * sizeof(float));
```

**Evidence:** `scratch/env_probe.cpp` sweeps `gainIndex ∈ {0,7,…,126}` × `lag ∈ {1,38,…,999}`
and compares all 256 bins per call against the closed form: **136 192 / 136 192 bins within
0.2 %, worst relative error 4.05e-06**, and `out[256..1023]` is zero on every call. The
residual is FFT round-off, so the closed form is a drop-in replacement, not an
approximation of a different formula.

### 4.3 `lsfsToMdctSpectralEnvelope(bool shortBooks, const uint16 idx[5], float out[1024])`

`0x9aa10`, 1232 bytes. Static-analysis reconstruction is in
`scratch_envelopes.md` (same directory), produced from the disassembly:

* two-stage LSF vector quantiser →
  `lsf[0..3] = CB11[idx0][0..3] + CB21[idx2]`,
  `lsf[4..7] = CB11[idx0][4..7] + CB22[idx3]`,
  `lsf[8..15] = CB12[idx1] + CB23[idx4]`;
  `shortBooks` swaps `CB11`/`CB12` for their `*_SHORT` variants;
* `LRVQLPC` (order 16, `24000.0f` and `50.0f` in its state) converts the 16 LSFs to 17 LPC
  coefficients; a 512-tap impulse response is taken through an `LPMDCT_LONG` (FFT size 512,
  scale `2.0f/512`) and `LPImpulseToSpecEnvelope(..., 256)` produces 256 magnitude bins;
* `out[256..1023]` is zeroed.

Tables, all `float`, all reached through relocated `.data.rel.ro` slots:
`CB11 0x32be90`, `CB11_SHORT 0x323e90`, `CB12 0x327e90`, `CB12_SHORT 0x31fe90` (each
`float[512][8]`, 16 KB); `CB21 0x31de90`, `CB22 0x31be90` (`float[512][4]`, 8 KB);
`CB23 0x317e90`. The values are LSF frequencies in Hz.

**Evidence, partial.** `env_probe.cpp` confirms only the invariants: the function returns 0,
`out[256..1023]` is always zero, and the output changes with `shortBooks` (`idx = 0` gives
`out[0..3] = 1.07391 …` for `false` and `0.239421 …` for `true`). The LSF→LPC→FFT chain
itself was **not** verified numerically — see §6.

---

## 5. How the pieces fit together

```
APACSyntacticElement
├── SQ / transform frame
│     APACICSInfo::Deserialize            band layout, window sequence
│     APACScaleFactorData::Deserialize    codebook 0, one delta per band  -> rec[] + scf[]
│     APACSpectralData::Deserialize       codebooks 1..11, X[] * scf      -> 1024 floats
│     (this is the only path the CarPlay 5.1 capture uses)
└── LRVQ frame
      APACLRVQDecoder::SpectrumDeserialize{SCE,CPE,LFE}
        └── LrvqDeserialize
              dequantizeLrvqGain          1 + 9 bits          -> gain
              lsfsToMdctSpectralEnvelope  5 x 9 bits          -> env  (|A(w)|)
              ltpToMdctSpectralEnvelope   7 + 10 bits         -> env2 (|1 - g z^-lag|)
              4-bit mode + 32 VQ index pairs                  -> work (8 x 32)
              spec = (work / (env * env2)) * gain
```

The two paths share **nothing** except `TBitstreamReader` and the general idea of a
scalefactor: LRVQ has no scalefactor bands, no `SFBOffsets`, no Huffman codebooks, and does
not touch `flt_3308C0`.

`SpectrumDeserializeSCE` / `CPE` read one bit before each channel; if it is set the channel
is skipped and its `APACICSInfo` is reset. `CPE` passes `firstChannel = 1` for channel 0 and
`0` for channel 1, and reuses the same `env`, `env2`, `work`, `qidx` and `prevMode` buffers
for both — that is the whole mechanism behind §3.2's reuse flags.

---

## 6. Unresolved

1. **`lsfsToMdctSpectralEnvelope`'s numeric chain is not oracle-verified.** The codebook
   addressing, the LPC order, the FFT size and the scaling in `scratch_envelopes.md` come
   from reading the disassembly only. The LRVQ oracle in §3.7 calls the *library's* copy, so
   it does not test that reconstruction. Before porting LRVQ, drive it directly the way
   §4.2 drives the LTP envelope.
2. **`vDSP_vdist`'s stride inside `LPImpulseToSpecEnvelope` (`0xb4ce0`)** was taken from
   Hex-Rays output on a different function and differs between the N=512 and N=2048 cases.
   Re-derive it.
3. **`lsf[0..4]` are 9 bits each — confirmed by `LrvqDeserialize`** (§3.2), which resolves
   the sibling analysis's open question about their width. But nothing bounds-checks them,
   and each codebook has exactly 512 rows, so a corrupt stream indexes in range by luck, not
   by construction.
4. **`qidx` can hold `-1`.** Single-stage modes (rows 0..3) never write a stage-1 index, so
   `qidx[2k+1]` keeps the `-1` the function initialises it with. If the next channel selects
   the *same* mode and that mode were two-stage, a `reuse1` flag would feed `-1` into
   `cb1[8*(-1) + n]`, reading 32 bytes before the table. Rows 0..3 are single-stage in both
   channels so `sameMode` cannot produce this, but nothing in the code enforces it. Not
   reachable from a well-formed stream as far as this analysis can tell; not proven
   unreachable.
5. **`reuseResidual` double-divides `work`** (§3.6). Reproduced exactly, but whether it is a
   bug in Apple's decoder or the intended design is unknown — no stream that exercises it
   has been observed.
6. **The `lo`/`hi` columns of the VQ mode table** are never read by the decoder. They are
   presumed to be the encoder's bit-budget-to-mode mapping; that is an inference, not a
   finding.
7. **Whether any real APAC stream uses LRVQ at all** is unknown. The one capture available
   does not (§0). The frame-type bit that selects `DeserializeLrvqFrame` over
   `DeserializeSQFrame` in `APACChannelPairElement` was not traced here.
8. **`APACSpectralData::Deserialize`'s short-window de-interleave** (§2.2) is transcribed
   from the decompilation and is *not* covered by the oracle, which only ever configures one
   window group with `ics[10] != 2`. It should be tested before the transform path is
   declared done.
9. **Codebook 11's `pow` path for magnitudes > 1023.** The oracle exercises it (the first
   run's only failure was there), but the library calls libm `pow`; a port on a different
   libm may differ in the last bit. Worth a table or a bit-exact `cbrt`-based
   implementation if drift matters.
