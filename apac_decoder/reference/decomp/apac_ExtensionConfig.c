/* ============================================================================
 * apac::ExtensionConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 388 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000964a0    388  apac::ExtensionConfig::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x000964a0  size 388 bytes  97 insns
 *   apac::ExtensionConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac15ExtensionConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::ExtensionConfig::Deserialize(apac__ExtensionConfig *this, TBitstreamReader_u32 *bs)
{
  _QWORD *v4; // x21
  _QWORD *v5; // x24
  _QWORD *v6; // x20
  unsigned int cache; // w1
  int v8; // w0
  unsigned int v9; // w20
  signed int v10; // w21
  signed int v11; // w20
  signed int v12; // w1
  int v13; // w0
  unsigned int v14; // w0
  int bitsLeft; // w1
  char v16; // w20
  unsigned int v17; // w0
  __int64 v19; // [xsp+50h] [xbp+50h]
  v5 = *((_QWORD **)this + 1);
  v4 = *((_QWORD **)this + 2);
  if ( v4 == v5 )
    goto LABEL_11;
  v6 = *((_QWORD **)this + 1);
  do
  {
    if ( *v6 )
      (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v6 + 8LL))(*v6);
    ++v6;
  }
  while ( v4 != v6 );
  *((_QWORD *)this + 2) = v5;
  cache = bs->cache;
  v8 = bs->bitsLeft - 1;
  bs->bitsLeft = v8;
  v9 = cache >> 31;
  if ( v8 < 0 )
    goto LABEL_12;
LABEL_7:
  bs->cache = 2 * cache;
  if ( !(_BYTE)v9 )
    return 0;
  while ( 1 )
  {
    apac::GetEscBits<8u,16u>(bs, 4u);
    v10 = 8 * (apac::GetEscBits<8u,16u>(bs, 4u) + 1);
    v11 = bs->bitsLeft + 8 * (bs->end - bs->cur);
    if ( v11 < v10 )
      return 1;
    TBitstreamReader<unsigned int>::SkipBits(bs, v10);
    v12 = v10 - (v11 - (bs->bitsLeft + 8 * (bs->end - bs->cur)));
    if ( v12 < 0 )
      return 1;
    TBitstreamReader<unsigned int>::SkipBits(bs, v12);
LABEL_11:
    cache = bs->cache;
    v13 = bs->bitsLeft - 1;
    bs->bitsLeft = v13;
    v9 = cache >> 31;
    if ( (v13 & 0x80000000) == 0 )
      goto LABEL_7;
LABEL_12:
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v14 = bs->cache;
    bitsLeft = bs->bitsLeft;
    v16 = v9 | (v14 >> (bitsLeft + 32));
    v17 = v14 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v17 = 0;
    bs->cur += 4;
    bs->cache = v17;
    bs->bitsLeft = bitsLeft + 32;
    if ( !v16 )
      return 0;
  }
}


