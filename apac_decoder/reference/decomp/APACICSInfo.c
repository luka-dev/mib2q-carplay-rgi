/* ============================================================================
 * APACICSInfo
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 944 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00099e60    736  APACICSInfo::Deserialize(TBitstreamReader<unsigned int>&, apac::CodingType) */
/* 0x0009a6e0     64  APACICSInfo::~APACICSInfo() */
/* 0x0009a720     80  APACICSInfo::Reset() */
/* 0x0009a770     64  APACICSInfo::~APACICSInfo() */

/* ------------------------------------------------------------------------ */
/* 0x00099e60  size 736 bytes  184 insns
 *   APACICSInfo::Deserialize(TBitstreamReader<unsigned int>&, apac::CodingType)
 *   [_ZN11APACICSInfo11DeserializeER16TBitstreamReaderIjEN4apac10CodingTypeE]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall APACICSInfo::Deserialize(APACICSInfo *this, TBitstreamReader_u32 *bs, int n)
{
  unsigned int cache; // w3
  int v7; // w1
  unsigned int v8; // w21
  unsigned int v9; // w0
  int v10; // w1
  unsigned int v11; // w20
  int v12; // w3
  unsigned int v13; // w0
  bool result; // w0
  unsigned int v15; // w0
  unsigned int v16; // w0
  unsigned int v17; // w1
  int v18; // w0
  unsigned int v19; // w20
  unsigned int v20; // w1
  unsigned int v21; // w1
  int v22; // w0
  unsigned int v23; // w20
  int v24; // w1
  int v25; // w2
  int v26; // w5
  unsigned int v27; // w0
  int bitsLeft; // w1
  unsigned int v29; // w0
  unsigned int v30; // w0
  int v31; // w1
  unsigned int v32; // w0
  unsigned int v33; // w0
  int v34; // w1
  unsigned int v35; // w0
  unsigned int v36; // w2
  int v37; // w3
  const unsigned __int8 *cur; // x1
  int v39; // w1
  __int64 v40; // [xsp+50h] [xbp+50h]
  cache = bs->cache;
  v7 = bs->bitsLeft - 2;
  bs->bitsLeft = v7;
  v8 = cache >> 30;
  if ( v7 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v27 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v8) = v8 | (v27 >> (bitsLeft + 32));
    v29 = v27 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v29 = 0;
    bs->cur += 4;
    bs->cache = v29;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 4 * cache;
  }
  *((_BYTE *)this + 10) = v8;
  *(_WORD *)((char *)this + 11) = 256;
  *((_DWORD *)this + 4) = 1;
  if ( (unsigned __int8)v8 == 2 )
  {
    v16 = *((unsigned __int8 *)this + 9);
    if ( *((_BYTE *)this + 9) )
    {
      if ( n == 1 )
      {
        *((_BYTE *)this + 11) = 7;
        if ( v16 <= 6 )
          return 1;
        v12 = 7;
        v24 = 0;
      }
      else
      {
        v17 = bs->cache;
        v18 = bs->bitsLeft - 4;
        bs->bitsLeft = v18;
        v19 = v17 >> 28;
        if ( v18 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v33 = bs->cache;
          v34 = bs->bitsLeft;
          LOBYTE(v19) = v19 | (v33 >> (v34 + 32));
          v35 = v33 << -(char)v34;
          if ( v34 == -32 )
            v35 = 0;
          bs->cur += 4;
          bs->cache = v35;
          bs->bitsLeft = v34 + 32;
        }
        else
        {
          bs->cache = 16 * v17;
        }
        v20 = *((unsigned __int8 *)this + 9);
        *((_BYTE *)this + 11) = v19;
        if ( v20 < (unsigned __int8)v19 )
          return 1;
        v21 = bs->cache;
        v22 = bs->bitsLeft - 7;
        bs->bitsLeft = v22;
        v23 = v21 >> 25;
        if ( v22 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v36 = bs->cache;
          v37 = bs->bitsLeft;
          cur = bs->cur;
          bs->bitsLeft = v37 + 32;
          bs->cur = cur + 4;
          v39 = (v36 >> (v37 + 32)) | v23;
          if ( v37 == -32 )
          {
            bs->cache = 0;
            v24 = v39 << 25;
          }
          else
          {
            v24 = v39 << 25;
            bs->cache = v36 << -(char)v37;
          }
          v12 = *((unsigned __int8 *)this + 11);
          n = *((_DWORD *)this + 4);
        }
        else
        {
          bs->cache = v21 << 7;
          v24 = v23 << 25;
          v12 = *((unsigned __int8 *)this + 11);
          n = *((_DWORD *)this + 4);
        }
      }
      v25 = 7;
      v26 = 1;
      do
      {
        if ( (v24 & 0x80000000) == 0 )
        {
          v26 *= 16;
          ++*((_BYTE *)this + 12);
        }
        n += v26;
        v24 *= 2;
        --v25;
      }
      while ( v25 );
      *((_DWORD *)this + 4) = n;
      goto LABEL_8;
    }
    return 1;
  }
  if ( n == 1 )
  {
    v15 = *((unsigned __int8 *)this + 8);
    *((_BYTE *)this + 11) = 27;
    return v15 <= 0x1A;
  }
  v9 = bs->cache;
  v10 = bs->bitsLeft - 6;
  bs->bitsLeft = v10;
  v11 = v9 >> 26;
  if ( v10 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v30 = bs->cache;
    v31 = bs->bitsLeft;
    v11 |= v30 >> (v31 + 32);
    v32 = v30 << -(char)v31;
    if ( v31 == -32 )
      v32 = 0;
    bs->cur += 4;
    bs->cache = v32;
    bs->bitsLeft = v31 + 32;
  }
  else
  {
    bs->cache = v9 << 6;
  }
  v12 = (unsigned __int8)v11;
  v13 = *((unsigned __int8 *)this + 8);
  *((_BYTE *)this + 11) = v11;
  if ( v13 < (unsigned __int8)v11 )
    return 1;
LABEL_8:
  result = 0;
  if ( !v12 )
    *((_BYTE *)this + 12) = 0;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0009a6e0  size 64 bytes  16 insns
 *   APACICSInfo::~APACICSInfo()
 *   APACICSInfo::~APACICSInfo()
 *   [_ZN11APACICSInfoD1Ev]
 *   [_ZN11APACICSInfoD2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN11APACICSInfoD2Ev'
void __fastcall APACICSInfo::~APACICSInfo(APACICSInfo *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
}


/* ------------------------------------------------------------------------ */
/* 0x0009a720  size 80 bytes  20 insns
 *   APACICSInfo::Reset()
 *   [_ZN11APACICSInfo5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACICSInfo::Reset(APACICSInfo *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_WORD *)this + 5) = 0;
  *((_BYTE *)this + 12) = 0;
  *((_DWORD *)this + 4) = 1;
  return v2 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x0009a770  size 64 bytes  16 insns
 *   APACICSInfo::~APACICSInfo()
 *   [_ZN11APACICSInfoD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall APACICSInfo::~APACICSInfo(APACICSInfo *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


