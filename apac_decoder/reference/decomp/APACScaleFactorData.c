/* ============================================================================
 * APACScaleFactorData
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 1224 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009a140   1224  APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&) */

/* ------------------------------------------------------------------------ */
/* 0x0009a140  size 1224 bytes  304 insns
 *   APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&)
 *   [_ZN19APACScaleFactorData11DeserializeER16TBitstreamReaderIjERK11APACICSInfo]
 *   data refs: 0x310280, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACScaleFactorData::Deserialize(
        APACScaleFactorData *this,
        TBitstreamReader_u32 *bs,
        const APACICSInfo *ics)
{
  __int64 v3; // d8
  __int64 v4; // d9
  _UNKNOWN **SCFHuffmanTable; // x28
  int bitsLeft; // w0
  unsigned int cache; // w1
  int v11; // w21
  int v12; // w0
  int v13; // w26
  int v14; // w0
  int v15; // w20
  unsigned int v16; // w1
  int v17; // w0
  int v18; // w22
  int v19; // w4
  int v20; // w1
  unsigned int v21; // w1
  unsigned int v22; // w23
  const unsigned __int8 *cur; // x1
  unsigned int v24; // w1
  int v25; // w5
  unsigned int v26; // w1
  __int64 result; // x0
  int v28; // w2
  unsigned int v29; // w22
  int v30; // w23
  unsigned int v31; // w1
  int v32; // w0
  int v33; // w0
  __int64 v34; // x1
  bool v35; // cc
  int v36; // w23
  unsigned int v37; // w0
  _BYTE *v38; // x1
  int v39; // w2
  char v40; // w0
  __int16 v41; // w0
  unsigned int v42; // w1
  int v43; // w3
  unsigned int v44; // w4
  unsigned int v45; // w1
  unsigned int v46; // w0
  int v47; // w1
  int v48; // w23
  unsigned int v49; // w0
  unsigned int v50; // w20
  int v51; // w2
  unsigned int v52; // w0
  int v53; // w1
  unsigned int v54; // w0
  __int64 v55; // [xsp+60h] [xbp+60h]
  __int64 v56; // [xsp+68h] [xbp+68h]
  unsigned int v57; // [xsp+70h] [xbp+70h]
  int v58; // [xsp+74h] [xbp+74h]
  _DWORD *v59; // [xsp+78h] [xbp+78h]
  int v60; // [xsp+80h] [xbp+80h]
  int v61; // [xsp+84h] [xbp+84h]
  unsigned int v62; // [xsp+88h] [xbp+88h]
  unsigned int v63; // [xsp+8Ch] [xbp+8Ch]
  _BYTE *v64; // [xsp+90h] [xbp+90h]
  char *v65; // [xsp+98h] [xbp+98h]
  __int64 v66; // [xsp+A8h] [xbp+A8h]
  __int64 v67; // [xsp+B0h] [xbp+B0h]
  SCFHuffmanTable = apac::GetSCFHuffmanTable();
  cache = bs->cache;
  bitsLeft = bs->bitsLeft;
  if ( *((_BYTE *)ics + 10) == 2 )
    v11 = 3;
  else
    v11 = 5;
  v62 = 0x80000000 >> *((_DWORD *)SCFHuffmanTable + 2);
  v12 = bitsLeft - 8;
  bs->bitsLeft = v12;
  v13 = HIBYTE(cache);
  if ( v12 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v52 = bs->cache;
    v53 = bs->bitsLeft;
    v13 |= v52 >> (v53 + 32);
    v54 = v52 << -(char)v53;
    if ( v53 == -32 )
      v54 = 0;
    bs->cur += 4;
    bs->cache = v54;
    bs->bitsLeft = v53 + 32;
  }
  else
  {
    bs->cache = cache << 8;
  }
  if ( *((_BYTE *)ics + 12) )
  {
    v55 = v3;
    v56 = v4;
    v60 = 0;
    v64 = (char *)this + 480;
    v65 = (char *)this + 480;
    while ( 1 )
    {
      v14 = *((unsigned __int8 *)ics + 11);
      v15 = 0;
      v57 = 0;
      v61 = 0;
      if ( !v14 )
        goto LABEL_36;
      do
      {
        v16 = bs->cache;
        v17 = bs->bitsLeft - 4;
        bs->bitsLeft = v17;
        LODWORD(v59) = v16 >> 28;
        if ( v17 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v42 = bs->cache;
          v43 = bs->bitsLeft;
          v17 = v43 + 32;
          v44 = (unsigned int)v59 | (v42 >> (v43 + 32));
          v45 = v42 << -(char)v43;
          if ( v43 == -32 )
            v45 = 0;
          bs->cur += 4;
          bs->cache = v45;
          bs->bitsLeft = v17;
          LODWORD(v59) = v44;
        }
        else
        {
          bs->cache = 16 * v16;
        }
        if ( (unsigned int)v59 > 0xB )
          goto LABEL_19;
        v18 = 0;
        v19 = 32 - v11;
        do
        {
          v21 = bs->cache;
          v17 -= v11;
          bs->bitsLeft = v17;
          v22 = v21 >> v19;
          if ( v17 < 0 )
          {
            cur = bs->cur;
            v58 = v19;
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, cur);
            v24 = bs->cache;
            v25 = -bs->bitsLeft;
            v17 = bs->bitsLeft + 32;
            bs->cur += 4;
            v22 |= v24 >> v17;
            v26 = v24 << v25;
            if ( !v17 )
              v26 = 0;
            bs->cache = v26;
            bs->bitsLeft = v17;
            v18 += v22;
            v20 = v18 + v15;
            v19 = v58;
            if ( v18 + v15 > (unsigned int)*((unsigned __int8 *)ics + 11) )
              goto LABEL_19;
          }
          else
          {
            bs->cache = v21 << v11;
            v18 += v22;
            v20 = v18 + v15;
            if ( v18 + v15 > (unsigned int)*((unsigned __int8 *)ics + 11) )
              goto LABEL_19;
          }
        }
        while ( v22 == (1 << v11) - 1 );
        if ( !v18 )
        {
LABEL_19:
          v3 = v55;
          v4 = v56;
          result = 1;
          goto LABEL_20;
        }
        v28 = (int)v59;
        v57 += v18;
        *((_WORD *)this + (unsigned int)(v60 + v61) + 244) = (unsigned __int16)v59 | ((_WORD)v18 << 8);
        if ( v28 )
        {
          v29 = 0;
          v30 = 0;
          v59 = &unk_310280;
          v58 = v13 - 100;
          while ( 1 )
          {
            if ( *((_DWORD *)SCFHuffmanTable + 2) > v29 )
            {
              if ( v29 <= 0x1F )
              {
                v31 = bs->cache;
                v32 = bs->bitsLeft - 32 + v29;
                bs->bitsLeft = v32;
                v63 = v31 >> v29;
                if ( v32 < 0 )
                {
                  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
                  v46 = bs->cache;
                  v47 = bs->bitsLeft;
                  v29 = 32;
                  v48 = (v46 >> (v47 + 32)) | v30;
                  v49 = v46 << -(char)v47;
                  if ( v47 == -32 )
                    v49 = 0;
                  v30 = v48 | v63;
                  bs->cur += 4;
                  bs->cache = v49;
                  bs->bitsLeft = v47 + 32;
                  goto LABEL_30;
                }
                v30 |= v31 >> v29;
                bs->cache = v31 << -(char)v29;
              }
              v29 = 32;
            }
LABEL_30:
            if ( v30 < 0 )
            {
              v40 = __clz(v62 | ~v30);
              v41 = *(_WORD *)(*((_QWORD *)*SCFHuffmanTable + 2 * (v40 & 0x3F) + 1)
                             + 2LL * ((unsigned int)(v30 << v40) >> *((_WORD *)*SCFHuffmanTable + 8 * (v40 & 0x3F))));
              v29 -= v41 & 0x3F;
              v13 += (__int64)v41 >> 6;
              v30 <<= v41 & 0x3F;
              if ( (v13 ^ (v13 >> 31)) <= 255 )
              {
                v58 = v13 - 100;
                v33 = v59[v13 & 3] + ((v13 - 100) >> 2 << 23);
              }
              else if ( v13 <= 0 )
              {
                v33 = 318767104;
                v13 = -256;
                v58 = -356;
              }
              else
              {
                v33 = 1389839613;
                v13 = 255;
                v58 = 155;
              }
            }
            else
            {
              v30 *= 2;
              --v29;
              v33 = v59[v58 & 3] + (v58 >> 2 << 23);
            }
            v34 = (unsigned int)(v15 + v60);
            v35 = v57 > ++v15;
            *((_DWORD *)this + v34) = v33;
            if ( !v35 )
            {
              v36 = v61 + 1;
              TBitstreamReader<unsigned int>::PutBack(bs, v29);
              goto LABEL_34;
            }
          }
        }
        v50 = v15 + v60;
        v51 = v18 + v50;
        do
          *((_DWORD *)this + v50++) = 0;
        while ( v51 != v50 );
        v15 = v20;
        v36 = v61 + 1;
LABEL_34:
        v37 = *((unsigned __int8 *)ics + 11);
        v61 = v36;
      }
      while ( v37 > v57 );
      LOBYTE(v14) = v36;
LABEL_36:
      v38 = v64;
      v39 = (int)v65;
      *v64 = v14;
      v64 = v38 + 1;
      v60 += 15;
      if ( *((unsigned __int8 *)ics + 12) <= (unsigned int)((_DWORD)v38 + 1 - v39) )
      {
        v3 = v55;
        v4 = v56;
        break;
      }
    }
  }
  result = 0;
LABEL_20:
  if ( v66 != _stack_chk_guard )
  {
    v55 = v3;
    v56 = v4;
  }
  return result;
}


#if 0 /* disassembly of 0x0009a140, for verification */
0009A140  e91340d1  SUB             X9, SP, #4,LSL#12
0009A144  3fa907f9  STR             XZR, [X9,#0xF50]
0009A148  fd7bb5a9  STP             X29, X30, [SP,#var_B0]!
0009A14C  fd030091  MOV             X29, SP
0009A150  f76303a9  STP             X23, X24, [SP,#0xB0+var_80]
0009A154  f80302aa  MOV             X24, X2
0009A158  821c00d0  ADRP            X2, #__stack_chk_guard_ptr@PAGE
0009A15C  f35301a9  STP             X19, X20, [SP,#0xB0+var_A0]
0009A160  f30301aa  MOV             X19, X1
0009A164  420047f9  LDR             X2, [X2,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A168  f55b02a9  STP             X21, X22, [SP,#0xB0+var_90]
0009A16C  75008052  MOV             W21, #3
0009A170  f96b04a9  STP             X25, X26, [SP,#0xB0+var_70]
0009A174  fb7305a9  STP             X27, X28, [SP,#0xB0+var_60]
0009A178  fb0300aa  MOV             X27, X0
0009A17C  400040f9  LDR             X0, [X2]
0009A180  e05700f9  STR             X0, [SP,#0xB0+var_8]
0009A184  000080d2  MOV             X0, #0; this
0009A188  22ffff97  BL              _ZN4apac18GetSCFHuffmanTableEv; apac::GetSCFHuffmanTable(void)
0009A18C  fc0300aa  MOV             X28, X0
0009A190  012b4039  LDRB            W1, [X24,#0xA]
0009A194  0300b052  MOV             W3, #0x80000000
0009A198  a4008052  MOV             W4, #5
0009A19C  820b40b9  LDR             W2, [X28,#8]
0009A1A0  3f080071  CMP             W1, #2
0009A1A4  61024229  LDP             W1, W0, [X19,#0x10]
0009A1A8  b502841a  CSEL            W21, W21, W4, EQ
0009A1AC  6224c21a  LSR             W2, W3, W2
0009A1B0  e28b00b9  STR             W2, [SP,#0xB0+var_28]
0009A1B4  00200051  SUB             W0, W0, #8
0009A1B8  601600b9  STR             W0, [X19,#0x14]
0009A1BC  3a7c1853  LSR             W26, W1, #0x18
0009A1C0  c01ef837  TBNZ            W0, #0x1F, loc_9A598
0009A1C4  215c1853  LSL             W1, W1, #8
0009A1C8  611200b9  STR             W1, [X19,#0x10]
0009A1CC  00334039  LDRB            W0, [X24,#0xC]
0009A1D0  e0130034  CBZ             W0, loc_9A44C
0009A1D4  39008052  MOV             W25, #1
0009A1D8  e827066d  STP             D8, D9, [SP,#0xB0+var_50]
0009A1DC  6966000f  MOVI            V9.2S, #0x13,LSL#24
0009A1E0  60830791  ADD             X0, X27, #0x1E0
0009A1E4  3923d51a  LSL             W25, W25, W21
0009A1E8  39070051  SUB             W25, W25, #1
0009A1EC  ff8300b9  STR             WZR, [SP,#0xB0+var_30]
0009A1F0  e00309a9  STP             X0, X0, [SP,#0xB0+var_20]
0009A1F4  a09f8852  MOV             W0, #0x52D744FD
0009A1FC  0800271e  FMOV            S8, W0
0009A200  002f4039  LDRB            W0, [X24,#0xB]
0009A204  14008052  MOV             W20, #0
0009A208  ff7300b9  STR             WZR, [SP,#0xB0+var_40]
0009A20C  ff8700b9  STR             WZR, [SP,#0xB0+var_2C]
0009A210  40100034  CBZ             W0, loc_9A418
0009A214  61024229  LDP             W1, W0, [X19,#0x10]
0009A218  00100051  SUB             W0, W0, #4
0009A21C  601600b9  STR             W0, [X19,#0x14]
0009A220  227c1c53  LSR             W2, W1, #0x1C
0009A224  e27b00b9  STR             W2, [SP,#0xB0+var_38]
0009A228  a015f837  TBNZ            W0, #0x1F, loc_9A4DC
0009A22C  216c1c53  LSL             W1, W1, #4
0009A230  611200b9  STR             W1, [X19,#0x10]
0009A234  e17b40b9  LDR             W1, [SP,#0xB0+var_38]
0009A238  3f2c0071  CMP             W1, #0xB
0009A23C  28050054  B.HI            loc_9A2E0
0009A240  04048052  MOV             W4, #0x20 ; ' '
0009A244  16008052  MOV             W22, #0
0009A248  8400154b  SUB             W4, W4, W21
0009A24C  0a000014  B               loc_9A274
0009A250  2120d51a  LSL             W1, W1, W21
0009A254  611200b9  STR             W1, [X19,#0x10]
0009A258  d602170b  ADD             W22, W22, W23
0009A25C  032f4039  LDRB            W3, [X24,#0xB]
0009A260  c102140b  ADD             W1, W22, W20
0009A264  3f00036b  CMP             W1, W3
0009A268  c8030054  B.HI            loc_9A2E0
0009A26C  ff02196b  CMP             W23, W25
0009A270  61050054  B.NE            loc_9A31C
0009A274  611240b9  LDR             W1, [X19,#0x10]
0009A278  0000154b  SUB             W0, W0, W21
0009A27C  601600b9  STR             W0, [X19,#0x14]
0009A280  3724c41a  LSR             W23, W1, W4
0009A284  60feff36  TBZ             W0, #0x1F, loc_9A250
0009A288  610240f9  LDR             X1, [X19]
0009A28C  e00313aa  MOV             X0, X19
0009A290  e47700b9  STR             W4, [SP,#0xB0+var_3C]
0009A294  db5c0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A298  61024229  LDP             W1, W0, [X19,#0x10]
0009A29C  630240f9  LDR             X3, [X19]
0009A2A0  e503002a  MOV             W5, W0
0009A2A4  e503054b  NEG             W5, W5
0009A2A8  00800031  ADDS            W0, W0, #0x20 ; ' '
0009A2AC  63100091  ADD             X3, X3, #4
0009A2B0  630200f9  STR             X3, [X19]
0009A2B4  2624c01a  LSR             W6, W1, W0
0009A2B8  f702062a  ORR             W23, W23, W6
0009A2BC  2120c51a  LSL             W1, W1, W5
0009A2C0  e103811a  CSEL            W1, WZR, W1, EQ
0009A2C4  61020229  STP             W1, W0, [X19,#0x10]
0009A2C8  d602170b  ADD             W22, W22, W23
0009A2CC  c102140b  ADD             W1, W22, W20
0009A2D0  e47740b9  LDR             W4, [SP,#0xB0+var_3C]
0009A2D4  032f4039  LDRB            W3, [X24,#0xB]
0009A2D8  3f00036b  CMP             W1, W3
0009A2DC  89fcff54  B.LS            loc_9A26C
0009A2E0  e827466d  LDP             D8, D9, [SP,#0xB0+var_50]
0009A2E4  20008052  MOV             W0, #1
0009A2E8  811c00d0  ADRP            X1, #__stack_chk_guard_ptr@PAGE
0009A2EC  210047f9  LDR             X1, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A2F0  e25740f9  LDR             X2, [SP,#0xB0+var_8]
0009A2F4  210040f9  LDR             X1, [X1]
0009A2F8  410001ca  EOR             X1, X2, X1
0009A2FC  611700b5  CBNZ            X1, loc_9A5E8
0009A300  f35341a9  LDP             X19, X20, [SP,#0xB0+var_A0]
0009A304  f55b42a9  LDP             X21, X22, [SP,#0xB0+var_90]
0009A308  f76343a9  LDP             X23, X24, [SP,#0xB0+var_80]
0009A30C  f96b44a9  LDP             X25, X26, [SP,#0xB0+var_70]
0009A310  fb7345a9  LDP             X27, X28, [SP,#0xB0+var_60]
0009A314  fd7bcba8  LDP             X29, X30, [SP+0xB0+var_B0],#0xB0
0009A318  c0035fd6  RET
0009A31C  36feff34  CBZ             W22, loc_9A2E0
0009A320  e20f5029  LDP             W2, W3, [SP,#0xB0+var_30]
0009A324  60a30791  ADD             X0, X27, #0x1E8
0009A328  e57340b9  LDR             W5, [SP,#0xB0+var_40]
0009A32C  4300030b  ADD             W3, W2, W3
0009A330  e27b40b9  LDR             W2, [SP,#0xB0+var_38]
0009A334  a500160b  ADD             W5, W5, W22
0009A338  e57300b9  STR             W5, [SP,#0xB0+var_40]
0009A33C  4420162a  ORR             W4, W2, W22,LSL#8
0009A340  04782378  STRH            W4, [X0,X3,LSL#1]
0009A344  42110034  CBZ             W2, loc_9A56C
0009A348  a01300d0  ADRL            X0, unk_310280
0009A350  16008052  MOV             W22, #0
0009A354  17008052  MOV             W23, #0
0009A358  e03f00f9  STR             X0, [SP,#0xB0+var_38]
0009A35C  40930151  SUB             W0, W26, #0x64 ; 'd'
0009A360  e07700b9  STR             W0, [SP,#0xB0+var_3C]
0009A364  1f2003d5  NOP
0009A368  800b40b9  LDR             W0, [X28,#8]
0009A36C  1f00166b  CMP             W0, W22
0009A370  e9010054  B.LS            loc_9A3AC
0009A374  df7e0071  CMP             W22, #0x1F
0009A378  88010054  B.HI            loc_9A3A8
0009A37C  61024229  LDP             W1, W0, [X19,#0x10]
0009A380  00800051  SUB             W0, W0, #0x20 ; ' '
0009A384  0000160b  ADD             W0, W0, W22
0009A388  2224d61a  LSR             W2, W1, W22
0009A38C  601600b9  STR             W0, [X19,#0x14]
0009A390  e28f00b9  STR             W2, [SP,#0xB0+var_24]
0009A394  800cf837  TBNZ            W0, #0x1F, loc_9A524
0009A398  f603164b  NEG             W22, W22
0009A39C  f702022a  ORR             W23, W23, W2
0009A3A0  2120d61a  LSL             W1, W1, W22
0009A3A4  611200b9  STR             W1, [X19,#0x10]
0009A3A8  16048052  MOV             W22, #0x20 ; ' '
0009A3AC  5705f837  TBNZ            W23, #0x1F, loc_9A454
0009A3B0  e07740b9  LDR             W0, [SP,#0xB0+var_3C]
0009A3B4  f702170b  ADD             W23, W23, W23
0009A3B8  e23f40f9  LDR             X2, [SP,#0xB0+var_38]
0009A3BC  01044092  AND             X1, X0, #3
0009A3C0  007c0213  ASR             W0, W0, #2
0009A3C4  d6060051  SUB             W22, W22, #1
0009A3C8  417861b8  LDR             W1, [X2,X1,LSL#2]
0009A3CC  205c000b  ADD             W0, W1, W0,LSL#23
0009A3D0  e18340b9  LDR             W1, [SP,#0xB0+var_30]
0009A3D4  e27340b9  LDR             W2, [SP,#0xB0+var_40]
0009A3D8  8102010b  ADD             W1, W20, W1
0009A3DC  94060011  ADD             W20, W20, #1
0009A3E0  5f00146b  CMP             W2, W20
0009A3E4  607b21b8  STR             W0, [X27,X1,LSL#2]
0009A3E8  08fcff54  B.HI            loc_9A368
0009A3EC  e08740b9  LDR             W0, [SP,#0xB0+var_2C]
0009A3F0  e103162a  MOV             W1, W22
0009A3F4  17040011  ADD             W23, W0, #1
0009A3F8  e00313aa  MOV             X0, X19
0009A3FC  455d0094  BL              _ZN16TBitstreamReaderIjE7PutBackEj; TBitstreamReader<uint>::PutBack(uint)
0009A400  002f4039  LDRB            W0, [X24,#0xB]
0009A404  e17340b9  LDR             W1, [SP,#0xB0+var_40]
0009A408  f78700b9  STR             W23, [SP,#0xB0+var_2C]
0009A40C  1f00016b  CMP             W0, W1
0009A410  28f0ff54  B.HI            loc_9A214
0009A414  e01e0012  AND             W0, W23, #0xFF
0009A418  e14b40f9  LDR             X1, [SP,#0xB0+var_20]
0009A41C  e29b40b9  LDR             W2, [SP,#0xB0+var_18]
0009A420  20140038  STRB            W0, [X1],#1
0009A424  e00301aa  MOV             X0, X1
0009A428  e18340b9  LDR             W1, [SP,#0xB0+var_30]
0009A42C  e04b00f9  STR             X0, [SP,#0xB0+var_20]
0009A430  0000024b  SUB             W0, W0, W2
0009A434  213c0011  ADD             W1, W1, #0xF
0009A438  e18300b9  STR             W1, [SP,#0xB0+var_30]
0009A43C  01334039  LDRB            W1, [X24,#0xC]
0009A440  3f00006b  CMP             W1, W0
0009A444  e8edff54  B.HI            loc_9A200
0009A448  e827466d  LDP             D8, D9, [SP,#0xB0+var_50]
0009A44C  00008052  MOV             W0, #0
0009A450  a6ffff17  B               loc_9A2E8
0009A454  e08b40b9  LDR             W0, [SP,#0xB0+var_28]
0009A458  820340f9  LDR             X2, [X28]
0009A45C  0000372a  ORN             W0, W0, W23
0009A460  0010c05a  CLZ             W0, W0
0009A464  01147cd3  UBFIZ           X1, X0, #4, #6
0009A468  e022c01a  LSL             W0, W23, W0
0009A46C  4300018b  ADD             X3, X2, X1
0009A470  42686178  LDRH            W2, [X2,X1]
0009A474  610440f9  LDR             X1, [X3,#8]
0009A478  0024c21a  LSR             W0, W0, W2
0009A47C  20786078  LDRH            W0, [X1,X0,LSL#1]
0009A480  01140012  AND             W1, W0, #0x3F ; '?'
0009A484  003c4693  SBFX            X0, X0, #6, #0xA
0009A488  d602014b  SUB             W22, W22, W1
0009A48C  5a03000b  ADD             W26, W26, W0
0009A490  f722c11a  LSL             W23, W23, W1
0009A494  407f9a4a  EOR             W0, W26, W26,ASR#31
0009A498  1ffc0371  CMP             W0, #0xFF
0009A49C  0d010054  B.LE            loc_9A4BC
0009A4A0  5f030071  CMP             W26, #0
0009A4A4  8d090054  B.LE            loc_9A5D4
0009A4A8  61138052  MOV             W1, #0x9B
0009A4AC  0001261e  FMOV            W0, S8
0009A4B0  fa1f8052  MOV             W26, #0xFF
0009A4B4  e17700b9  STR             W1, [SP,#0xB0+var_3C]
0009A4B8  c6ffff17  B               loc_9A3D0
0009A4BC  e23f40f9  LDR             X2, [SP,#0xB0+var_38]
0009A4C0  41074092  AND             X1, X26, #3
0009A4C4  40930151  SUB             W0, W26, #0x64 ; 'd'
0009A4C8  e07700b9  STR             W0, [SP,#0xB0+var_3C]
0009A4CC  417861b8  LDR             W1, [X2,X1,LSL#2]
0009A4D0  007c0213  ASR             W0, W0, #2
0009A4D4  205c000b  ADD             W0, W1, W0,LSL#23
0009A4D8  beffff17  B               loc_9A3D0
0009A4DC  610240f9  LDR             X1, [X19]
0009A4E0  e00313aa  MOV             X0, X19
0009A4E4  475c0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A4E8  61024229  LDP             W1, W0, [X19,#0x10]
0009A4EC  620240f9  LDR             X2, [X19]
0009A4F0  e303002a  MOV             W3, W0
0009A4F4  e57b40b9  LDR             W5, [SP,#0xB0+var_38]
0009A4F8  00800031  ADDS            W0, W0, #0x20 ; ' '
0009A4FC  e303034b  NEG             W3, W3
0009A500  42100091  ADD             X2, X2, #4
0009A504  2424c01a  LSR             W4, W1, W0
0009A508  a400042a  ORR             W4, W5, W4
0009A50C  2120c31a  LSL             W1, W1, W3
0009A510  e103811a  CSEL            W1, WZR, W1, EQ
0009A514  620200f9  STR             X2, [X19]
0009A518  61020229  STP             W1, W0, [X19,#0x10]
0009A51C  e47b00b9  STR             W4, [SP,#0xB0+var_38]
0009A520  45ffff17  B               loc_9A234
0009A524  610240f9  LDR             X1, [X19]
0009A528  e00313aa  MOV             X0, X19
0009A52C  355c0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A530  60064229  LDP             W0, W1, [X19,#0x10]
0009A534  16048052  MOV             W22, #0x20 ; ' '
0009A538  630240f9  LDR             X3, [X19]
0009A53C  22800031  ADDS            W2, W1, #0x20 ; ' '
0009A540  e103014b  NEG             W1, W1
0009A544  0424c21a  LSR             W4, W0, W2
0009A548  9700172a  ORR             W23, W4, W23
0009A54C  e48f40b9  LDR             W4, [SP,#0xB0+var_24]
0009A550  0020c11a  LSL             W0, W0, W1
0009A554  61100091  ADD             X1, X3, #4
0009A558  e003801a  CSEL            W0, WZR, W0, EQ
0009A55C  f702042a  ORR             W23, W23, W4
0009A560  610200f9  STR             X1, [X19]
0009A564  600a0229  STP             W0, W2, [X19,#0x10]
0009A568  91ffff17  B               loc_9A3AC
0009A56C  e08340b9  LDR             W0, [SP,#0xB0+var_30]
0009A570  9402000b  ADD             W20, W20, W0
0009A574  c202140b  ADD             W2, W22, W20
0009A578  7f5b34b8  STR             WZR, [X27,W20,UXTW#2]
0009A57C  94060011  ADD             W20, W20, #1
0009A580  5f00146b  CMP             W2, W20
0009A584  a1ffff54  B.NE            loc_9A578
0009A588  e08740b9  LDR             W0, [SP,#0xB0+var_2C]
0009A58C  f403012a  MOV             W20, W1
0009A590  17040011  ADD             W23, W0, #1
0009A594  9bffff17  B               loc_9A400
0009A598  610240f9  LDR             X1, [X19]
0009A59C  e00313aa  MOV             X0, X19
0009A5A0  185c0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A5A4  60064229  LDP             W0, W1, [X19,#0x10]
0009A5A8  630240f9  LDR             X3, [X19]
0009A5AC  22800031  ADDS            W2, W1, #0x20 ; ' '
0009A5B0  e103014b  NEG             W1, W1
0009A5B4  0424c21a  LSR             W4, W0, W2
0009A5B8  5a03042a  ORR             W26, W26, W4
0009A5BC  0020c11a  LSL             W0, W0, W1
0009A5C0  61100091  ADD             X1, X3, #4
0009A5C4  e003801a  CSEL            W0, WZR, W0, EQ
0009A5C8  610200f9  STR             X1, [X19]
0009A5CC  600a0229  STP             W0, W2, [X19,#0x10]
0009A5D0  fffeff17  B               loc_9A1CC
0009A5D4  612c8012  MOV             W1, #0xFFFFFE9C
0009A5D8  2001261e  FMOV            W0, S9
0009A5DC  fa1f8012  MOV             W26, #0xFFFFFF00
0009A5E0  e17700b9  STR             W1, [SP,#0xB0+var_3C]
0009A5E4  7bffff17  B               loc_9A3D0
0009A5E8  e827066d  STP             D8, D9, [SP,#0xB0+var_50]
0009A5EC  4955ff97  BL              .__stack_chk_fail
0009A5F0  e103162a  MOV             W1, W22
0009A5F4  f40300aa  MOV             X20, X0
0009A5F8  e00313aa  MOV             X0, X19
0009A5FC  c55c0094  BL              _ZN16TBitstreamReaderIjE7PutBackEj; TBitstreamReader<uint>::PutBack(uint)
0009A600  e00314aa  MOV             X0, X20; struct _Unwind_Exception *
0009A604  4f58ff97  BL              ._Unwind_Resume
#endif

