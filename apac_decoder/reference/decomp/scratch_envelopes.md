# `APACLRVQDecoder` spectral-envelope builders — reconstructed from disassembly

Both functions carry Hex-Rays' *"local variable allocation has failed"* warning.
Everything below is derived from the AArch64 disassembly in `APACLRVQDecoder.c`
(the `#if 0 /* disassembly of ... */` blocks), cross-checked against
`libSPF_CP_PlugInWrapper.so` (ELF, image base 0) via `reference/scratch/lib.py`.

**Both are static member functions. Neither has a `this`.** Hex-Rays inserted a
phantom `this` in both, exactly as it did for `dequantizeLrvqGain` at `0x9a950`,
and every argument in its pseudocode is therefore shifted by one.

---

## 1. `lsfsToMdctSpectralEnvelope` @ `0x0009aa10` (1232 bytes, 305 insns)

`_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE`

### 1.1 Signature / register map — **STATIC**

| reg | meaning |
|---|---|
| `W0` | `bool useShortCodebooks` — first *declared* parameter |
| `X1` | `const apac::aligned_array<uint16,5>&` — 5 codebook indices |
| `X2` | `apac::aligned_array<float,1024>&` — output envelope |
| `W0` (ret) | `int` error code, **always 0** (`MOV W0,#0` at `0x9adac`) |

Proof that `W0` is the `bool`, not `this`:

```
0009AA20  TST  W0, #0xFF          ; tests only the low byte -> a bool
0009AA84  B.NE loc_9ADCC          ; nonzero -> *_SHORT codebook variant
0009AA3C  MOV  X20, X1            ; X1 is the uint16[5] array (LDRH [X20],[X20,#2].. )
0009AA40  MOV  X19, X2            ; X2 is the float output buffer
```

and the call site inside `LrvqDeserialize`:

```
0009B268  LDRB W0, [SP,#var_78]   ; a 1-bit bitstream flag, zero-extended byte
0009B26C  MOV  X1, X24            ; &lsfIdx[0]
0009B270  LDR  X2, [SP,#var_A0]   ; &out[0]
0009B274  BL   ...lsfsToMdctSpectralEnvelope...
```
Three arguments, no fourth. Confirmed static.

Corrected C signature:

```c
static int APACLRVQDecoder::lsfsToMdctSpectralEnvelope(
        bool           useShortCodebooks,   /* W0 */
        const uint16_t idx[5],              /* X1 */
        float          out[1024]);          /* X2 */
```

`useShortCodebooks != 0` selects `*_SHORT` for stage-1 only (CB11/CB12);
stage-2 (CB21/CB22/CB23) is shared by both paths. Note the "short" branch at
`0x9ADCC` re-joins the common path by `B loc_9AB10`.

### 1.2 Stack frame (`SUB SP,SP,#0xA20`, offsets relative to SP after the sub)

| offset | size | contents |
|---|---|---|
| `+0x00..0x48` | | saved `X29,X30,X19..X25` |
| `+0x58..0x90` | 0x38 | `APACLRVQDecoder::LPMDCT_LONG` object (an `apac::LPMDCTFilter`) |
| `+0x88` | 4 | `float` = `2.0f / fftSize` (member `+0x30` of the filter) |
| `+0x90..0x180` | 0xF0 | `APACLRVQDecoder::LRVQLPC` object (an `apac::LSFTools`) |
| `+0x180..0x1C0` | 0x40 | `float lsf[16]` — the dequantized LSFs, in **Hz** |
| `+0x1C0..0x204` | 0x44 | `float lpc[17]` — `LSF_To_LPC` output |
| `+0x210..0xA10` | 0x800 | `float impulse[512]` — LP impulse/coefficient buffer |
| `+0xA18` | 8 | `__stack_chk_guard` canary |

The two instructions before the `SUB SP` (`SUB X9,SP,#4,LSL#12` / `STR XZR,[X9,#0x5E0]`)
are stack-clash probing, not data.

### 1.3 Reconstructed body

```c
static int lsfsToMdctSpectralEnvelope(bool useShort,
                                      const uint16_t idx[5],
                                      float out[1024])
{
    float lsf[16] = {0};            /* SP+0x180, 4x STP XZR,XZR */
    float lpc[17];                  /* SP+0x1C0 */
    float impulse[512];             /* SP+0x210 */

    /* ---- stage-1 / stage-2 VQ, three vDSP_vadd calls -------------------- */
    const float *CB11 = useShort ? kCodebook_Lrvq_Lsf_CB11_SHORT   /* 0x323e90 */
                                 : kCodebook_Lrvq_Lsf_CB11;        /* 0x32be90 */
    const float *CB12 = useShort ? kCodebook_Lrvq_Lsf_CB12_SHORT   /* 0x31fe90 */
                                 : kCodebook_Lrvq_Lsf_CB12;        /* 0x327e90 */

    /* lsf[0..3]  = CB11[idx0][0..3] + CB21[idx2][0..3] */
    vDSP_vadd(CB11 + 8*idx[0],      1, CB21 + 4*idx[2], 1, &lsf[0], 1, 4);
    /* lsf[4..7]  = CB11[idx0][4..7] + CB22[idx3][0..3] */
    vDSP_vadd(CB11 + 8*idx[0] + 4,  1, CB22 + 4*idx[3], 1, &lsf[4], 1, 4);
    /* lsf[8..15] = CB12[idx1][0..7] + CB23[idx4][0..7] */
    vDSP_vadd(CB12 + 8*idx[1],      1, CB23 + 8*idx[4], 1, &lsf[8], 1, 8);

    /* ---- stack-construct an LRVQLPC (LSFTools) --------------------------- */
    /* whole 0xF0-byte object is zeroed except: */
    obj.vptr        = vtable_LRVQLPC + 0x10;   /* 0x42b0b0 + 0x10 */
    *(u16*)(obj+0x08) = 0x0010;                /* 16   : LPCBase order          */
    *(u16*)(obj+0x0A) = 0x0040;                /* 64   : UNRESOLVED (see below) */
    *(u16*)(obj+0x0C) = 0x0001;                /* 1    : LSFTools "frames"      */
    *(u16*)(obj+0x68) = 0x0010;                /* 16   : LSF order, read by LSF_To_LPC */
    *(float*)(obj+0x70) = 24000.0f;            /* sample rate, read by LSF_To_LPC */
    *(float*)(obj+0x74) = 50.0f;               /* min LSF spacing (not read here) */
    /* every std::vector inside stays {null,null,null} -> dtor is a no-op */

    apac::LSFTools::LSF_To_LPC(&obj, lsf, lpc);   /* 0xb5370 */

    /* ---- build the LP impulse buffer ------------------------------------ */
    memset(&impulse[17], 0, (512-17)*4);   /* STR WZR,[X0],#4 from +0x254 to +0xA10 */
    memcpy(impulse, lpc, 17*4);            /* 4x LDP/STP (0x40 B) + 1 LDR/STR (4 B) */

    /* ---- stack-construct an LPMDCT_LONG (LPMDCTFilter) ------------------- */
    filt.vptr        = vtable_LPMDCT_LONG + 0x10;      /* 0x42b158 + 0x10 */
    *(u32*)(filt+0x08) = 0x00100200;   /* u16 @+8 = 512 (fftSize N), u16 @+10 = 16 */
    filt.dft.vptr    = vtable_MultiRadixRealFFT + 0x10;/* 0x42a828 + 0x10, at +0x10 */
    *(u32*)(filt+0x18) = 0;  *(u64*)(filt+0x20) = 0;  *(u64*)(filt+0x28) = 0;

    DFTHandle<false>::Initialize(&filt.dft /* filt+0x10 */, 512);
    *(float*)(filt+0x30) = 2.0f / (float)(u16)*(u16*)(filt+0x08);   /* = 2.0f/512 */

    apac::LPMDCTFilter::LPImpulseToSpecEnvelope(&filt, impulse, out, 256);

    memset(&out[256], 0, (1024-256)*4);   /* STR WZR,[X0],#4, X0: out+0x400 -> out+0x1000 */

    /* ---- destruction: vptrs rewritten to the base-class vtables, then the
       null-check/delete chain runs over the (all-null) LSFTools vectors, so
       nothing is actually freed. ------------------------------------------ */
    DFTHandle<false>::~DFTHandle(&filt.dft);
    /* ...inlined ~LSFTools: 5x `if (p) operator delete(p)` for the vectors at
       obj+0xD8,0xC0,0xA8,0x90,0x78; then the vector<T*> at obj+0x50 (element
       loop deleting p[6] and p[2] then p); then the Autocorr* at obj+0x48
       (devirtualised: compares vptr[1] against apac::Autocorr::~Autocorr
       @0x79810 and inlines it if equal, else calls it); then obj+0x30, obj+0x18 */
    return 0;
}
```

Control flow, exactly: one two-way branch (`TST W0,#0xFF` / `B.NE`) that only
picks the two stage-1 table bases, three straight-line `vDSP_vadd` calls, two
`STR WZR,[Xn],#4` clear loops, one `if (p1!=p2) do {...} while` vector-element
loop in the destructor, seven `CBZ`-guarded `operator delete` calls, one
`CMP/B.NE` devirtualisation branch, and the canary check. No early exits, no
error returns — the only `ret` value is 0. Landing pads at `0x9ae68`, `0x9aeac`,
`0x9aeb4` are C++ exception cleanup (`_Unwind_Resume`), not normal flow.

### 1.4 Constants materialised

| where | encoding | value |
|---|---|---|
| `0x9aa8c`, etc. | `MOV X5,#1` | vDSP stride `1` |
| `0x9aaa0`,`0x9aae0` | `MOV X6,#4` | vDSP length `4` |
| `0x9ab28` | `MOV X6,#8` | vDSP length `8` |
| `0x9ab34`+`0x9ab48` | `MOV X6,#0x400010` + `MOVK X6,#1,LSL#32` | `0x0000000100400010` → `u16 16`, `u16 64`, `u16 1`, `u16 0` |
| `0x9ab3c`+`0x9ab50` | `MOV X4,#0x46BB8000` + `MOVK X4,#0x4248,LSL#48` | `0x4248000046BB8000` → low word `0x46BB8000` = **24000.0f**, high word `0x42480000` = **50.0f** |
| `0x9ab4c` | `MOV W5,#0x10` | `16` (LSF/LPC order) |
| `0x9abd0` | `MOV W13,#0x100200` | `u16 512` (fft size) + `u16 16` |
| `0x9ac0c` | `MOV W1,#0x200` | DFT size `512` |
| `0x9ac34` | `FMOV S0,#2.0` | `2.0f` |
| `0x9ac48` | `MOV W3,#0x100` | `256` output bins |
| all `ADD Xn,Xn,#0x10` | | the `+16` that skips the RTTI/offset-to-top header of an Itanium-ABI vtable |

`0x46BB8000 = 24000.0f` and `0x42480000 = 50.0f` are corroborated:
`apac::LSFTools::ConditionLSFs` @`0x9c6e0` calls `Condition_LSFs(buf, 50.0f, 24000.0f)`
with those same literals, and `LSF_To_LPC` reads `*((float*)this+28)` (= `+0x70`)
and `*((uint16*)this+52)` (= `+0x68`).

### 1.5 Data references, classified

**Real run-time-indexed tables** (all `float`, all `.rodata`, exact `st_size`
from `.dynsym`; reached indirectly — `ADRP`+`LDR` of a `.data.rel.ro` slot that
holds an `R_AARCH64_RELATIVE` pointer to the payload):

| GOT-ish slot | payload | symbol | bytes | shape | index expr | stride |
|---|---|---|---|---|---|---|
| `0x42d138` | `0x32be90` | `kCodebook_Lrvq_Lsf_CB11` | 16384 | `float[512][8]` | `idx[0]` | 32 B (`UBFIZ #5`; second use `LSL#3;ADD#4;LSL#2` = +16 B) |
| `0x42d128` | `0x323e90` | `kCodebook_Lrvq_Lsf_CB11_SHORT` | 16384 | `float[512][8]` | `idx[0]` | 32 B |
| `0x42dfa0` | `0x327e90` | `kCodebook_Lrvq_Lsf_CB12` | 16384 | `float[512][8]` | `idx[1]` | 32 B (`UBFIZ #5`) |
| `0x42d8c8` | `0x31fe90` | `kCodebook_Lrvq_Lsf_CB12_SHORT` | 16384 | `float[512][8]` | `idx[1]` | 32 B |
| `0x42dc68` | `0x31de90` | `kCodebook_Lrvq_Lsf_CB21` | 8192 | `float[512][4]` | `idx[2]` | 16 B (`UBFIZ #4`) |
| `0x42d9a0` | `0x31be90` | `kCodebook_Lrvq_Lsf_CB22` | 8192 | `float[512][4]` | `idx[3]` | 16 B (`UBFIZ #4`) |
| `0x42d5b0` | `0x317e90` | `kCodebook_Lrvq_Lsf_CB23` | 16384 | `float[512][8]` | `idx[4]` | 32 B (`UBFIZ #5`) |

All five indices are masked to 16 bits by the `UBFIZ`/`LSL W` forms; **no bounds
check is emitted**, and 512 rows implies 9-bit indices. First rows (decoded as
`float`, values are LSF frequencies in Hz for a 24000 Hz rate → 0..12000):

```
CB11 [0]        654.57  1219.46  2382.69  3332.50  3712.34  4172.18  5423.02  6004.66
CB12 [0]       6037.50  7012.14  7782.81  8670.03  9458.90  9751.94 10207.63 11089.74
CB11_SHORT[0]   548.70   993.91  1725.14  2561.99  3261.60  4001.34  4711.97  5372.10
CB12_SHORT[0]  6373.88  7339.92  7521.83  8097.03  9014.13 10290.22 10471.19 10789.86
CB21 [0]        -94.80    74.23    -7.31  -102.29
CB22 [0]        223.80  -134.19   -56.14   -45.90
CB23 [0]        -20.86  -121.16   169.08   -67.03   151.12    37.28   105.32   -53.04
```
Stage 1 (CB11/CB12) is the coarse LSF vector, stage 2 (CB21/CB22/CB23) the
signed refinement — consistent with a split multi-stage VQ.

**One-off pointer loads, not tables** (each is loaded once and used as a vtable
pointer / function address / GOT entry):

| address | what it is | how used |
|---|---|---|
| `0x42ce00` | `.got` slot, `R_AARCH64_GLOB_DAT __stack_chk_guard` | canary load at entry and exit |
| `0x42dbe8` → `0x42a828` | `vtable for MultiRadixRealFFT` | `+0x10` stored into `filt+0x10`, twice |
| `0x42cb80` → `0x42b158` | `vtable for APACLRVQDecoder::LPMDCT_LONG` | `+0x10` stored into `filt+0x00` |
| `0x42da70` → `0x42b0b0` | `vtable for APACLRVQDecoder::LRVQLPC` | `+0x10` stored into `obj+0x00` |
| `0x42d2a8` → `0x42b700` | `vtable for apac::LPMDCTFilter` | `+0x10` written back during teardown |
| `0x42d070` → `0x42b798` | `vtable for apac::LSFTools` | `+0x10` written back during teardown |
| `0x42dbd8` → `0x42b750` | `vtable for apac::LPCBase` | `+0x10` written back during teardown |
| `0x42d7d8` → `0x42a848` | `vtable for apac::Autocorr` | `+0x10` written back in the inlined `~Autocorr` |
| `0x42ddc0` → `0x79810` | `apac::Autocorr::~Autocorr()` (`D0Ev`) | compared against `vptr[1]` to devirtualise |

So of the addresses IDA lists in the header comment: `0x79810` is a **function**
(not data); `0x42a828, 0x42a848, 0x42b0b0, 0x42b158, 0x42b700, 0x42b750, 0x42b798`
are **vtables** (targets of the slots, not the slots); `0x42cb80, 0x42ce00,
0x42d070, 0x42d128, 0x42d138, 0x42d2a8, 0x42d5b0, 0x42d7d8, 0x42d8c8` are the
`.data.rel.ro`/`.got` **slots**. The list omits the slots `0x42d9a0` (CB22),
`0x42dfa0` (CB12), `0x42dc68` (CB21), `0x42dbd8` (LPCBase), `0x42dbe8`
(MultiRadixRealFFT) and `0x42ddc0` (Autocorr dtor), which the disassembly does
reference. Only the seven `0x31xxxx/0x32xxxx` codebooks are indexed at run time.

### 1.6 Relevant vtable slots (verified, `libSPF_CP_PlugInWrapper.so`)

`vtable for APACLRVQDecoder::LRVQLPC` @ `0x42b0b0` (entries at `+0x10 + 8*n`):

```
n=10 (+0x50, offset 80)  apac::LSFTools::LSF_To_LPC(float*,float*)      0xb5370
n=12 (+0x60, offset 96)  apac::LSFTools::LSF_To_LSP(const float*,float*,float) 0xb50f0
n=14 (+0x70, offset112)  apac::LSFTools::LSP_To_LPC(const float*,float*,u16)   0xb4460
n=16 (+0x80, offset128)  apac::LSFTools::Condition_LSFs(float*,float,float)    0xb4390
n=17 (+0x88, offset136)  apac::LSFTools::ConditionLSFs(float*)                 0x9c6e0
```

`LSF_To_LPC` tests `vtable[136] == apac::LSFTools::ConditionLSFs` — for `LRVQLPC`
that is **true**, so the concrete chain is deterministic:

```
Condition_LSFs(lsf, 50.0f, 24000.0f);
LSF_To_LSP(lsf, tmp, 24000.0f);          /* obj+0x70 */
LSP_To_LPC(tmp, lpc, 16);                /* obj+0x68 */
```

`vtable for APACLRVQDecoder::LPMDCT_LONG` @ `0x42b158` — every filter slot is the
un-overridden `apac::LPMDCTFilter` implementation; only the destructors differ.
So `LPImpulseToSpecEnvelope` is literally `apac::LPMDCTFilter`'s @ `0xb4ce0`.

---

## 2. `ltpToMdctSpectralEnvelope` @ `0x0009aee0` (536 bytes, 132 insns)

`_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE`

### 2.1 Signature / register map — **STATIC**

| reg | meaning |
|---|---|
| `W0` | `uint16 ltpGainIndex` — first declared parameter (`AND W0,W0,#0xFFFF; UCVTF S0,W0`) |
| `W1` | `uint16 ltpLag` (`AND W1,W1,#0xFFFF`, later `AND X2,X1,#0xFFFF` used as an array index) |
| `X2` | `apac::aligned_array<float,1024>&` out (`MOV X21,X2`) |
| `W0` (ret) | `int`, **always 0** |

Call site inside `LrvqDeserialize` (`0x9b7b8..0x9b7fc`) reads a **7-bit** field
(`LSR W3,W1,#0x19` → bits 31..25, range 0..127) into `W0` and a **10-bit** field
(`LSL W1,W1,#7` then `LSR W2,W1,#0x16`, range 0..1023) into `W1`, then `LDR X2`
= the buffer. Three arguments, no `this`. Confirmed static.

```c
static int APACLRVQDecoder::ltpToMdctSpectralEnvelope(
        uint16_t ltpGainIndex,   /* W0, 7-bit  0..127 */
        uint16_t ltpLag,         /* W1, 10-bit 0..1023 */
        float    out[1024]);     /* X2 */
```

### 2.2 Stack frame (`SUB SP,SP,#0x2090`)

| offset | size | contents |
|---|---|---|
| `+0x00..0x38` | | `X29,X30,X19..X23` |
| `+0x38` | 8 | saved `D8` (holds the gain across the calls) |
| `+0x44` | 4 | `float` scalar for `vDSP_vsmul` |
| `+0x48..0x80` | 0x38 | `APACLRVQDecoder::LPMDCT_LTP` object (`apac::LPMDCTFilter`) |
| `+0x78` | 4 | `float` = `2.0f/2048` (filter member `+0x30`) |
| `+0x80..0x2080` | 0x2000 | `float impulse[2048]` |
| `+0x2088` | 8 | canary |

### 2.3 Reconstructed body

```c
static int ltpToMdctSpectralEnvelope(uint16_t gainIdx, uint16_t lag, float out[1024])
{
    float impulse[2048];                              /* SP+0x80 */

    /* gain = (gainIdx / 127.0f) * 0.74f + 0.25f   -> range [0.25, 0.99] */
    float gain = ((float)gainIdx / 127.0f) * 0.74f + 0.25f;   /* FMADD S8,S0,S1,S2 */

    for (i = 0; i < 2048; i++) impulse[i] = 0.0f;     /* STR WZR,[X0],#4 loop */
    impulse[0]   =  1.0f;                             /* STR S1, [SP+0x80]      */
    impulse[lag] = -gain;                             /* STR S0, [X23,X2,LSL#2] */
                                                      /* NB: lag==0 overwrites the 1.0f */

    /* stack-construct an LPMDCT_LTP */
    filt.vptr        = vtable_LPMDCT_LTP + 0x10;          /* 0x42b1a8 + 0x10 */
    *(u32*)(filt+0x08) = 0x00100800;   /* u16 @+8 = 2048 (fftSize), u16 @+10 = 16 */
    filt.dft.vptr    = vtable_MultiRadixRealFFT + 0x10;   /* 0x42a828 + 0x10 */
    *(u32*)(filt+0x18) = 0; *(u64*)(filt+0x20) = 0; *(u64*)(filt+0x28) = 0;

    DFTHandle<false>::Initialize(&filt.dft, 2048);
    *(float*)(filt+0x30) = 2.0f / (float)(u16)*(u16*)(filt+0x08);   /* 2.0f/2048 */

    apac::LPMDCTFilter::LPImpulseToSpecEnvelope(&filt, impulse, out, 256);

    for (i = 256; i < 1024; i++) out[i] = 0.0f;

    float s = 1.0f / (gain + 1.0f);                   /* FADD S8,S8,#1.0; FDIV */
    vDSP_vsmul(out, 1, &s, out, 1, 256);              /* X0,X1,X2,X3,X4,X5 */

    DFTHandle<false>::~DFTHandle(&filt.dft);          /* vptrs restored first */
    return 0;
}
```

Straight-line except for the two `STR WZR,[X0],#4` clear loops and the canary
check; the `0x9b098`/`0x9b0c8` blocks are `_Unwind_Resume` landing pads. No
early exits, no bounds check on `lag`.

### 2.4 Constants materialised

| where | encoding | value |
|---|---|---|
| `0x9af00` | `MOV W3,#0x42FE0000` → `FMOV S8,W3` | **127.0f** |
| `0x9af04` | `FMOV S2,#0.25` | `0.25f` |
| `0x9af20` | `MOV W3,#0x3F3D70A4` → `FMOV S1,W3` | **0.74f** (nearest float to 0.74) |
| `0x9af88`, `0x9b00c` | `FMOV S1/S0,#1.0` | `1.0f` |
| `0x9af94` | `MOV W5,#0x100800` | `u16 2048` (fft size) + `u16 16` |
| `0x9afac` | `MOV W1,#0x800` | DFT size `2048` |
| `0x9afd0` | `FMOV S0,#2.0` | `2.0f` |
| `0x9afe4` | `MOV W3,#0x100` | `256` output bins |
| `0x9b010`,`0x9b028` | `MOV X4,#1`, `MOV X5,#0x100` | vDSP stride 1, length 256 |

`FNEG S0,S8` at `0x9af7c` produces the `-gain` written at `impulse[lag]`.

### 2.5 Data references, classified

**No run-time-indexed table.** Every listed address is a one-off pointer load:

| address | what | use |
|---|---|---|
| `0x42ce00` | `.got` slot, `__stack_chk_guard` | canary |
| `0x42dbe8` → `0x42a828` | `vtable for MultiRadixRealFFT` | `+0x10` into `filt+0x10` |
| `0x42d5e8` → `0x42b1a8` | `vtable for APACLRVQDecoder::LPMDCT_LTP` | `+0x10` into `filt+0x00` |
| `0x42d2a8` → `0x42b700` | `vtable for apac::LPMDCTFilter` | `+0x10` on teardown |

The header comment lists `0x42a828, 0x42b1a8, 0x42b700` (the vtables) and
`0x42d2a8, 0x42d5e8, 0x42dbe8` (their slots) — i.e. the same three objects
counted twice — plus `0x42ce00` and `0x43e520`.

`vtable for APACLRVQDecoder::LPMDCT_LTP` @ `0x42b1a8` again inherits every
`apac::LPMDCTFilter` method unchanged (`LPImpulseToSpecEnvelope` = `0xb4ce0`);
only the two destructor slots are the derived ones.

### 2.6 What it computes

`impulse[]` is the impulse response of the LTP analysis filter
`A_ltp(z) = 1 - gain * z^-lag` laid out as a 2048-tap sequence;
`LPImpulseToSpecEnvelope` (`0xb4ce0`) runs a 2048-point real forward FFT of it
and takes the per-bin magnitude (`vDSP_vdist(re, ., im)`), yielding 256 envelope
values. Those are then normalised by `1/(1+gain)`, which is the maximum of
`|1 - gain·e^{-jωL}|`, so the envelope is scaled to a peak of 1.0. The remaining
768 slots of the 1024-float output are zeroed.

`lsfsToMdctSpectralEnvelope` is the same machinery one level up: the two-stage
VQ produces 16 LSFs in Hz, `LSF_To_LPC` turns them into 17 LPC coefficients
`a[0..16]`, those become a 512-tap impulse buffer, and a 512-point FFT magnitude
gives the same 256-bin envelope (no post-normalisation there).

---

## 3. Unresolved / not determined

1. **`u16` at `LSFTools+0x0A` = `0x0040` (64).** No reader found in
   `apac_LSFTools.c`, `apac_LPCBase.c` or the two functions here. Purpose unknown.
2. **`u16` at `LPMDCTFilter+0x0A` = `0x0010` (16)** in *both* functions.
   `apac::LPMDCTFilter::Initialize(u16,u16)` @`0x9c7b0` writes it as its second
   argument, and `LPCToSpecEnvelope`/`AutocorrFromPSD` read it as an LPC order
   (`4*(order+1)` allocations). Reading it as "LPC order 16" is an inference from
   those siblings, **not proven** from these two functions — `LPImpulseToSpecEnvelope`
   never touches it.
3. **`u16` at `LSFTools+0x0C` = `1`.** `LSFTools::Initialize()` uses it as
   `n + order*n`, i.e. a subframe/frame count; but `Initialize()` is never called
   on these stack objects, so the value is inert here. Meaning inferred only.
4. **The exact semantics of the `bool` in `lsfsToMdctSpectralEnvelope`.** All that
   is provable is *nonzero → `CB11_SHORT`/`CB12_SHORT`*. Its bitstream name
   (short-block? low-rate? "LSF codebook set") is not determined; at the
   fall-through call path in `LrvqDeserialize` it is a byte that was just zeroed,
   and the non-zero value arrives via `B loc_9B268` from `0x9b880`.
5. **Row counts are exact (512 each, from `.dynsym` `st_size`), but the legal
   index range is not** — no bounds check is emitted and I did not find the
   bit-widths of `idx[0..4]` in the bitstream reader.
6. **`0x43e520`** (listed by IDA for `0x9aee0`) is referenced by no instruction in
   the function; it is not inside any `SHT_PROGBITS` section I could match. Most
   likely the `.bss`/copy-relocation object behind `__stack_chk_guard`, but this
   is **unverified**.
7. **`DFTHandle<false>::Initialize`** (called with 512 / 2048) and
   `MultiRadixRealFFT::RealOutOfPlaceForwardTransform` were not analysed; the
   twiddle tables they allocate are out of scope here.
8. **`vDSP_vdist`'s stride argument** inside `LPImpulseToSpecEnvelope`
   (`(N/n == 4) + 1`) comes from Hex-Rays on a *different* function and was not
   re-derived from disassembly; for `N=512,n=256` and `N=2048,n=256` it is not
   the same value, so the port must re-check that function.
9. **Whether `out` may alias `impulse`/`lsf`** — irrelevant here (all stack), but
   note `LPImpulseToSpecEnvelope` writes `out[0..255]` and the caller then zeroes
   `out[256..1023]`; nothing proves `out` is 1024 floats other than the mangled
   type `aligned_array<float,1024>`.
