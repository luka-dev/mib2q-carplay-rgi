/* ============================================================================
 * apac::TrimmingData
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 260 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00078040    260  apac::TrimmingData::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x00078040  size 260 bytes  65 insns
 *   apac::TrimmingData::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac12TrimmingData11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::TrimmingData::Deserialize(apac__TrimmingData *this, TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w2
  int v5; // w1
  unsigned int v6; // w22
  __int64 result; // x0
  unsigned int Esc; // w22
  unsigned int v9; // w1
  unsigned int v10; // w0
  int bitsLeft; // w1
  unsigned int v12; // w0
  __int64 v13; // [xsp+40h] [xbp+40h]
  cache = bs->cache;
  v5 = bs->bitsLeft - 1;
  bs->bitsLeft = v5;
  v6 = cache >> 31;
  if ( v5 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v10 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v6) = v6 | (v10 >> (bitsLeft + 32));
    v12 = v10 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v12 = 0;
    bs->cur += 4;
    bs->cache = v12;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  result = 0;
  if ( (_BYTE)v6 )
  {
    Esc = apac::GetEscBits<16u,20u>(bs, 0xBu);
    v9 = apac::GetEscBits<16u,20u>(bs, 0xBu);
    result = 1;
    if ( Esc + v9 <= *((_DWORD *)this + 2) )
    {
      *(_DWORD *)this = Esc;
      *((_DWORD *)this + 1) = v9;
      return 0;
    }
  }
  return result;
}


