/* ============================================================================
 * apac
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 9 function(s), 2500 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00078150    544  unsigned int apac::GetEscBits<16u, 20u>(TBitstreamReader<unsigned int>&, unsigned int) */
/* 0x00096630    536  unsigned int apac::GetEscBits<8u, 16u>(TBitstreamReader<unsigned int>&, unsigned int) */
/* 0x00099220    536  unsigned int apac::GetEscBits<6u, 12u>(TBitstreamReader<unsigned int>&, unsigned int) */
/* 0x00099dc0     76  apac::GetHuffmanTable(unsigned int) */
/* 0x00099e10     76  apac::GetSCFHuffmanTable() */
/* 0x0009d070     92  apac::GetProfileConfig(apac::Profile) */
/* 0x0009d9d0    152  apac::GetSFBOffsetTable(unsigned int, unsigned int) */
/* 0x000b0d20     76  apac::ConvertToAudioDataType(apac::ASCType) */
/* 0x000b2750    412  apac::CreateContentMixer(apac::GlobalConfig const&) */

/* ------------------------------------------------------------------------ */
/* 0x00078150  size 544 bytes  136 insns
 *   unsigned int apac::GetEscBits<16u, 20u>(TBitstreamReader<unsigned int>&, unsigned int)
 *   [_ZN4apac10GetEscBitsILj16ELj20EEEjR16TBitstreamReaderIjEj]
 *   data refs: 0x30ea90, 0x30eb50, 0x30ebd8, 0x42ce00, 0x43e520
 */
unsigned int __fastcall apac::GetEscBits<16u,20u>(TBitstreamReader_u32 *bs, unsigned int n)
{
  int v3; // w21
  unsigned int cache; // w3
  int v5; // w2
  unsigned int v6; // w20
  unsigned int v8; // w1
  int v9; // w0
  unsigned int v10; // w21
  unsigned int v11; // w1
  int v12; // w0
  unsigned int v13; // w21
  unsigned int v14; // w2
  int bitsLeft; // w3
  const unsigned __int8 *cur; // x1
  const unsigned __int8 *v17; // x1
  unsigned int v18; // w2
  const unsigned __int8 *v19; // x1
  int v20; // w3
  const unsigned __int8 *v21; // x1
  unsigned int v22; // w0
  int v23; // w1
  int v24; // w21
  unsigned int v25; // w0
  __int64 v26; // [xsp+40h] [xbp+40h]
  if ( n > 0x20 )
    __assert_fail(
      "(numBits <= 32)",
      "/home/kawagoe/eclipse_work/workspace/SPF_CP_PlugInWrapper/NPPlugInWrapper/src/Support/ApplePositionalAudioCodec/So"
      "urce/APACStructs.h",
      0xC6u,
      "uint32_t apac::GetEscBits(BitstreamReader&, uint32_t) [with unsigned int ESCBits = 16; unsigned int ESC2Bits = 20;"
      " uint32_t = unsigned int; BitstreamReader = TBitstreamReader<unsigned int>]");
  v3 = (1LL << n) - 1;
  if ( n - 1 > 0x1F )
  {
    v6 = 0;
    if ( (unsigned int)(1LL << n) != 1 )
      return v6;
  }
  else
  {
    cache = bs->cache;
    v5 = bs->bitsLeft - n;
    bs->bitsLeft = v5;
    v6 = cache >> -(char)n;
    if ( v5 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v14 = bs->cache;
      bitsLeft = bs->bitsLeft;
      cur = bs->cur;
      bs->bitsLeft = bitsLeft + 32;
      v17 = cur + 4;
      bs->cur = v17;
      v6 |= v14 >> (bitsLeft + 32);
      if ( bitsLeft == -32 )
      {
        bs->cache = 0;
        if ( v3 != v6 )
          return v6;
        v10 = 0;
        bs->bitsLeft = -16;
        goto LABEL_18;
      }
      bs->cache = v14 << -(char)bitsLeft;
    }
    else
    {
      bs->cache = cache << n;
    }
    if ( v3 != v6 )
      return v6;
  }
  v8 = bs->cache;
  v9 = bs->bitsLeft - 16;
  bs->bitsLeft = v9;
  v10 = HIWORD(v8);
  if ( (v9 & 0x80000000) == 0 )
  {
    bs->cache = v8 << 16;
LABEL_10:
    v6 += v10;
    if ( v10 != 0xFFFF )
      return v6;
    v11 = bs->cache;
    v12 = v9 - 20;
    bs->bitsLeft = v12;
    v13 = v11 >> 12;
    if ( (v12 & 0x80000000) == 0 )
    {
      v6 += v13;
      bs->cache = v11 << 20;
      return v6;
    }
    v21 = bs->cur;
    goto LABEL_21;
  }
  v17 = bs->cur;
LABEL_18:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v17);
  v18 = bs->cache;
  v19 = bs->cur;
  v20 = bs->bitsLeft;
  v9 = v20 + 32;
  bs->bitsLeft = v20 + 32;
  v21 = v19 + 4;
  bs->cur = v21;
  v10 |= v18 >> (v20 + 32);
  if ( v20 != -32 )
  {
    bs->cache = v18 << -(char)v20;
    goto LABEL_10;
  }
  bs->cache = 0;
  v6 += v10;
  if ( v10 != 0xFFFF )
    return v6;
  v13 = 0;
  bs->bitsLeft = -20;
LABEL_21:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v21);
  v22 = bs->cache;
  v23 = bs->bitsLeft;
  v24 = v13 | (v22 >> (v23 + 32));
  v25 = v22 << -(char)v23;
  v6 += v24;
  if ( v23 == -32 )
    v25 = 0;
  bs->cur += 4;
  bs->cache = v25;
  bs->bitsLeft = v23 + 32;
  return v6;
}


/* ------------------------------------------------------------------------ */
/* 0x00096630  size 536 bytes  134 insns
 *   unsigned int apac::GetEscBits<8u, 16u>(TBitstreamReader<unsigned int>&, unsigned int)
 *   [_ZN4apac10GetEscBitsILj8ELj16EEEjR16TBitstreamReaderIjEj]
 *   data refs: 0x30eb50, 0x30ebd8, 0x30f1e8, 0x42ce00, 0x43e520
 */
unsigned int __fastcall apac::GetEscBits<8u,16u>(TBitstreamReader_u32 *bs, unsigned int n)
{
  int v3; // w21
  unsigned int cache; // w3
  int v5; // w2
  unsigned int v6; // w20
  unsigned int v8; // w1
  int v9; // w0
  unsigned int v10; // w21
  unsigned int v11; // w1
  int v12; // w0
  unsigned int v13; // w21
  unsigned int v14; // w2
  int bitsLeft; // w3
  const unsigned __int8 *cur; // x1
  const unsigned __int8 *v17; // x1
  unsigned int v18; // w2
  const unsigned __int8 *v19; // x1
  int v20; // w3
  const unsigned __int8 *v21; // x1
  unsigned int v22; // w0
  int v23; // w1
  int v24; // w21
  unsigned int v25; // w0
  __int64 v26; // [xsp+40h] [xbp+40h]
  if ( n > 0x20 )
    __assert_fail(
      "(numBits <= 32)",
      "/home/kawagoe/eclipse_work/workspace/SPF_CP_PlugInWrapper/NPPlugInWrapper/src/Support/ApplePositionalAudioCodec/So"
      "urce/APACStructs.h",
      0xC6u,
      "uint32_t apac::GetEscBits(BitstreamReader&, uint32_t) [with unsigned int ESCBits = 8; unsigned int ESC2Bits = 16; "
      "uint32_t = unsigned int; BitstreamReader = TBitstreamReader<unsigned int>]");
  v3 = (1LL << n) - 1;
  if ( n - 1 > 0x1F )
  {
    v6 = 0;
    if ( (unsigned int)(1LL << n) != 1 )
      return v6;
  }
  else
  {
    cache = bs->cache;
    v5 = bs->bitsLeft - n;
    bs->bitsLeft = v5;
    v6 = cache >> -(char)n;
    if ( v5 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v14 = bs->cache;
      bitsLeft = bs->bitsLeft;
      cur = bs->cur;
      bs->bitsLeft = bitsLeft + 32;
      v17 = cur + 4;
      bs->cur = v17;
      v6 |= v14 >> (bitsLeft + 32);
      if ( bitsLeft == -32 )
      {
        bs->cache = 0;
        if ( v3 != v6 )
          return v6;
        v10 = 0;
        bs->bitsLeft = -8;
        goto LABEL_18;
      }
      bs->cache = v14 << -(char)bitsLeft;
    }
    else
    {
      bs->cache = cache << n;
    }
    if ( v3 != v6 )
      return v6;
  }
  v8 = bs->cache;
  v9 = bs->bitsLeft - 8;
  bs->bitsLeft = v9;
  v10 = HIBYTE(v8);
  if ( (v9 & 0x80000000) == 0 )
  {
    bs->cache = v8 << 8;
LABEL_10:
    v6 += v10;
    if ( v10 != 255 )
      return v6;
    v11 = bs->cache;
    v12 = v9 - 16;
    bs->bitsLeft = v12;
    v13 = HIWORD(v11);
    if ( (v12 & 0x80000000) == 0 )
    {
      v6 += v13;
      bs->cache = v11 << 16;
      return v6;
    }
    v21 = bs->cur;
    goto LABEL_21;
  }
  v17 = bs->cur;
LABEL_18:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v17);
  v18 = bs->cache;
  v19 = bs->cur;
  v20 = bs->bitsLeft;
  v9 = v20 + 32;
  bs->bitsLeft = v20 + 32;
  v21 = v19 + 4;
  bs->cur = v21;
  v10 |= v18 >> (v20 + 32);
  if ( v20 != -32 )
  {
    bs->cache = v18 << -(char)v20;
    goto LABEL_10;
  }
  bs->cache = 0;
  v6 += v10;
  if ( v10 != 255 )
    return v6;
  v13 = 0;
  bs->bitsLeft = -16;
LABEL_21:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v21);
  v22 = bs->cache;
  v23 = bs->bitsLeft;
  v24 = v13 | (v22 >> (v23 + 32));
  v25 = v22 << -(char)v23;
  v6 += v24;
  if ( v23 == -32 )
    v25 = 0;
  bs->cur += 4;
  bs->cache = v25;
  bs->bitsLeft = v23 + 32;
  return v6;
}


/* ------------------------------------------------------------------------ */
/* 0x00099220  size 536 bytes  134 insns
 *   unsigned int apac::GetEscBits<6u, 12u>(TBitstreamReader<unsigned int>&, unsigned int)
 *   [_ZN4apac10GetEscBitsILj6ELj12EEEjR16TBitstreamReaderIjEj]
 *   data refs: 0x30eb50, 0x30ebd8, 0x30f2a8, 0x42ce00, 0x43e520
 */
unsigned int __fastcall apac::GetEscBits<6u,12u>(TBitstreamReader_u32 *bs, unsigned int n)
{
  int v3; // w21
  unsigned int cache; // w3
  int v5; // w2
  unsigned int v6; // w20
  unsigned int v8; // w1
  int v9; // w0
  unsigned int v10; // w21
  unsigned int v11; // w1
  int v12; // w0
  unsigned int v13; // w21
  unsigned int v14; // w2
  int bitsLeft; // w3
  const unsigned __int8 *cur; // x1
  const unsigned __int8 *v17; // x1
  unsigned int v18; // w2
  const unsigned __int8 *v19; // x1
  int v20; // w3
  const unsigned __int8 *v21; // x1
  unsigned int v22; // w0
  int v23; // w1
  int v24; // w21
  unsigned int v25; // w0
  __int64 v26; // [xsp+40h] [xbp+40h]
  if ( n > 0x20 )
    __assert_fail(
      "(numBits <= 32)",
      "/home/kawagoe/eclipse_work/workspace/SPF_CP_PlugInWrapper/NPPlugInWrapper/src/Support/ApplePositionalAudioCodec/So"
      "urce/APACStructs.h",
      0xC6u,
      "uint32_t apac::GetEscBits(BitstreamReader&, uint32_t) [with unsigned int ESCBits = 6; unsigned int ESC2Bits = 12; "
      "uint32_t = unsigned int; BitstreamReader = TBitstreamReader<unsigned int>]");
  v3 = (1LL << n) - 1;
  if ( n - 1 > 0x1F )
  {
    v6 = 0;
    if ( (unsigned int)(1LL << n) != 1 )
      return v6;
  }
  else
  {
    cache = bs->cache;
    v5 = bs->bitsLeft - n;
    bs->bitsLeft = v5;
    v6 = cache >> -(char)n;
    if ( v5 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v14 = bs->cache;
      bitsLeft = bs->bitsLeft;
      cur = bs->cur;
      bs->bitsLeft = bitsLeft + 32;
      v17 = cur + 4;
      bs->cur = v17;
      v6 |= v14 >> (bitsLeft + 32);
      if ( bitsLeft == -32 )
      {
        bs->cache = 0;
        if ( v3 != v6 )
          return v6;
        v10 = 0;
        bs->bitsLeft = -6;
        goto LABEL_18;
      }
      bs->cache = v14 << -(char)bitsLeft;
    }
    else
    {
      bs->cache = cache << n;
    }
    if ( v3 != v6 )
      return v6;
  }
  v8 = bs->cache;
  v9 = bs->bitsLeft - 6;
  bs->bitsLeft = v9;
  v10 = v8 >> 26;
  if ( (v9 & 0x80000000) == 0 )
  {
    bs->cache = v8 << 6;
LABEL_10:
    v6 += v10;
    if ( v10 != 63 )
      return v6;
    v11 = bs->cache;
    v12 = v9 - 12;
    bs->bitsLeft = v12;
    v13 = v11 >> 20;
    if ( (v12 & 0x80000000) == 0 )
    {
      v6 += v13;
      bs->cache = v11 << 12;
      return v6;
    }
    v21 = bs->cur;
    goto LABEL_21;
  }
  v17 = bs->cur;
LABEL_18:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v17);
  v18 = bs->cache;
  v19 = bs->cur;
  v20 = bs->bitsLeft;
  v9 = v20 + 32;
  bs->bitsLeft = v20 + 32;
  v21 = v19 + 4;
  bs->cur = v21;
  v10 |= v18 >> (v20 + 32);
  if ( v20 != -32 )
  {
    bs->cache = v18 << -(char)v20;
    goto LABEL_10;
  }
  bs->cache = 0;
  v6 += v10;
  if ( v10 != 63 )
    return v6;
  v13 = 0;
  bs->bitsLeft = -12;
LABEL_21:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v21);
  v22 = bs->cache;
  v23 = bs->bitsLeft;
  v24 = v13 | (v22 >> (v23 + 32));
  v25 = v22 << -(char)v23;
  v6 += v24;
  if ( v23 == -32 )
    v25 = 0;
  bs->cur += 4;
  bs->cache = v25;
  bs->bitsLeft = v23 + 32;
  return v6;
}


/* ------------------------------------------------------------------------ */
/* 0x00099dc0  size 76 bytes  18 insns
 *   apac::GetHuffmanTable(unsigned int)
 *   [_ZN4apac15GetHuffmanTableEj]
 *   data refs: 0x424d98, 0x42ce00, 0x43e520
 */
_UNKNOWN **__fastcall apac::GetHuffmanTable(unsigned int n)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return off_424D98[n];
}


/* ------------------------------------------------------------------------ */
/* 0x00099e10  size 76 bytes  18 insns
 *   apac::GetSCFHuffmanTable()
 *   [_ZN4apac18GetSCFHuffmanTableEv]
 *   data refs: 0x424d98, 0x424df8, 0x42ce00, 0x43e520
 */
_UNKNOWN **__fastcall apac::GetSCFHuffmanTable()
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  return &off_424DF8;
}


/* ------------------------------------------------------------------------ */
/* 0x0009d070  size 92 bytes  22 insns
 *   apac::GetProfileConfig(apac::Profile)
 *   [_ZN4apac16GetProfileConfigENS_7ProfileE]
 *   data refs: 0x42ce00, 0x431e48, 0x434bc8, 0x43e520
 */
__int16 *__fastcall apac::GetProfileConfig(int n)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  if ( (unsigned __int16)word_434BC8 == (unsigned __int16)n )
    return &word_434BC8;
  else
    return (__int16 *)&unk_431E48;
}


/* ------------------------------------------------------------------------ */
/* 0x0009d9d0  size 152 bytes  37 insns
 *   apac::GetSFBOffsetTable(unsigned int, unsigned int)
 *   [_ZN4apac17GetSFBOffsetTableEjj]
 *   data refs: 0x4258f8, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::GetSFBOffsetTable(unsigned int n, unsigned int n2)
{
  bool v2; // zf
  __int64 result; // x0
  unsigned int v5; // w1
  int *v6; // x2
  int i; // w3
  __int64 v8; // [xsp+20h] [xbp+20h]
  v2 = n2 == 1024;
  result = 0;
  v5 = 0;
  if ( v2 )
  {
    v6 = (int *)&unk_4258F8;
    for ( i = 96000; n != i; i = *v6 )
    {
      ++v5;
      v6 += 10;
      if ( v5 == 12 )
        return 0;
    }
    return (__int64)&unk_4258F8 + 40 * v5;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b0d20  size 76 bytes  19 insns
 *   apac::ConvertToAudioDataType(apac::ASCType)
 *   [_ZN4apac22ConvertToAudioDataTypeENS_7ASCTypeE]
 *   data refs: 0x42ce00, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall apac::ConvertToAudioDataType(int n)
{
  __int64 v1; // x0
  __int64 result; // x0
  __int64 v3; // [xsp+20h] [xbp+20h]
  if ( n == 6 )
    *(_QWORD *)&n = 8;
  else
    v1 = (unsigned int)n;
  LODWORD(result) = n;
  return result;
}


#if 0 /* disassembly of 0x000b0d20, for verification */
000B0D20  e91340d1  SUB             X9, SP, #4,LSL#12
000B0D24  3ff107f9  STR             XZR, [X9,#0xFE0]
000B0D28  e11b0090  ADRP            X1, #__stack_chk_guard_ptr@PAGE
000B0D2C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
000B0D30  02018052  MOV             W2, #8
000B0D34  fd030091  MOV             X29, SP
000B0D38  210047f9  LDR             X1, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B0D3C  1f180071  CMP             W0, #6
000B0D40  0010821a  CSEL            W0, W0, W2, NE
000B0D44  220040f9  LDR             X2, [X1]
000B0D48  e20f00f9  STR             X2, [SP,#0x20+var_8]
000B0D4C  020080d2  MOV             X2, #0
000B0D50  e20f40f9  LDR             X2, [SP,#0x20+var_8]
000B0D54  210040f9  LDR             X1, [X1]
000B0D58  410001ca  EOR             X1, X2, X1
000B0D5C  610000b5  CBNZ            X1, loc_B0D68
000B0D60  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
000B0D64  c0035fd6  RET
000B0D68  6afbfe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b2750  size 412 bytes  103 insns
 *   apac::CreateContentMixer(apac::GlobalConfig const&)
 *   [_ZN4apac18CreateContentMixerERKNS_12GlobalConfigE]
 *   data refs: 0x951c0, 0x95980, 0x42add0, 0x42b690, 0x42ca90, 0x42ce00, 0x42ce18, 0x42db68, 0x42dd28, 0x43e520
 */
__int64 *__fastcall apac::CreateContentMixer(const apac__GlobalConfig *a)
{
  __int64 *v1; // x8
  __int64 *v2; // x19
  __int64 (*v3)(void); // x3
  _QWORD *v4; // x0
  _QWORD *v6; // x0
  _QWORD *v7; // x23
  void (__fastcall *v8)(ContentOutBuffer *); // x2
  void *v9; // x0
  _QWORD *v10; // x22
  _QWORD *v11; // x21
  void *v12; // x0
  void *v13; // x0
  __int64 v14; // [xsp+50h] [xbp+50h]
  v2 = v1;
  *v1 = 0;
  v3 = **(__int64 (***)(void))a;
  if ( (char *)v3 == (char *)apac::GlobalConfig::CodecConfigData::configs )
    v4 = (_QWORD *)(*((_QWORD *)a + 1) + 176LL);
  else
    v4 = (_QWORD *)v3();
  if ( (unsigned int)((__int64)(v4[1] - *v4) >> 3) == 1 )
  {
    v6 = (_QWORD *)operator new(0x38u);
    v6[1] = 0;
    v6[2] = 0;
    v6[3] = 0;
    v7 = (_QWORD *)*v2;
    *v6 = off_42B6A0;
    v6[4] = 0;
    *v2 = (__int64)v6;
    v6[5] = 0;
    v6[6] = 0;
    if ( v7 )
    {
      v8 = *(void (__fastcall **)(ContentOutBuffer *))(*v7 + 8LL);
      if ( v8 == ContentOutBuffer::~ContentOutBuffer )
      {
        v9 = (void *)v7[4];
        *v7 = off_42B6A0;
        if ( v9 )
          operator delete(v9);
        *v7 = &unk_42ADE0;
        v10 = (_QWORD *)v7[2];
        v11 = (_QWORD *)v7[1];
        if ( v11 != v10 )
        {
          do
          {
            v12 = (void *)v11[6];
            if ( v12 )
              operator delete(v12);
            v13 = (void *)v11[2];
            if ( v13 )
              operator delete(v13);
            v11 += 9;
          }
          while ( v10 != v11 );
          v10 = (_QWORD *)v7[1];
        }
        if ( v10 )
          operator delete(v10);
        operator delete(v7);
      }
      else
      {
        v8((ContentOutBuffer *)v7);
      }
    }
  }
  return v2;
}


