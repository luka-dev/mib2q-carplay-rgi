/* ============================================================================
 * SC_LanguageSelectionSet
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 1020 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 *
 * NOTE: not required for the CarPlay 5.1 / 5.1.2 target (Ambisonics scene
 *       or low-bitrate path). Included for completeness only.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000a44b0    236  SC_LanguageSelectionSet::operator==(SC_LanguageSelectionSet const&) const */
/* 0x000a45a0    156  SC_LanguageSelectionSet::CheckConformance(unsigned short) const */
/* 0x000a6860    628  SC_LanguageSelectionSet::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x000a44b0  size 236 bytes  59 insns
 *   SC_LanguageSelectionSet::operator==(SC_LanguageSelectionSet const&) const
 *   [_ZNK23SC_LanguageSelectionSeteqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_LanguageSelectionSet::operator==(SC_LanguageSelectionSet *this, const SC_LanguageSelectionSet *a)
{
  unsigned int v2; // w22
  const void *v5; // x0
  const void *v6; // x1
  size_t v7; // x2
  __int64 v9; // [xsp+40h] [xbp+40h]
  v2 = 0;
  v5 = *(const void **)this;
  v6 = *(const void **)a;
  v7 = *((_QWORD *)this + 1) - (_QWORD)v5;
  if ( v7 != *((_QWORD *)a + 1) - (_QWORD)v6 || v7 && memcmp(v5, v6, v7) )
    return v2;
  if ( *((unsigned __int16 *)this + 12) != *((unsigned __int16 *)a + 12)
    || *((unsigned __int8 *)a + 26) != *((unsigned __int8 *)this + 26) )
  {
    return 0;
  }
  v2 = 1;
  if ( !*((_BYTE *)a + 26) )
    return v2;
  return SC_Tag::operator==(
           (SC_LanguageSelectionSet *)((char *)this + 32),
           (const SC_LanguageSelectionSet *)((char *)a + 32));
}


/* ------------------------------------------------------------------------ */
/* 0x000a45a0  size 156 bytes  38 insns
 *   SC_LanguageSelectionSet::CheckConformance(unsigned short) const
 *   [_ZNK23SC_LanguageSelectionSet16CheckConformanceEt]
 *   data refs: 0x3306d8, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_LanguageSelectionSet::CheckConformance(SC_LanguageSelectionSet *this, unsigned __int16 v)
{
  unsigned __int16 *v3; // x0
  unsigned __int16 *v4; // x3
  __int64 v6; // [xsp+30h] [xbp+30h]
  v3 = *(unsigned __int16 **)this;
  v4 = *((unsigned __int16 **)this + 1);
  if ( v4 == v3 )
    return 0;
  while ( *v3 < (unsigned int)v )
  {
    if ( v4 == ++v3 )
      return 0;
  }
  __printf_chk(1, "ERROR: Scene item index out of range\n");
  return 0xFFFFFFFFLL;
}


/* ------------------------------------------------------------------------ */
/* 0x000a6860  size 628 bytes  157 insns
 *   SC_LanguageSelectionSet::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN23SC_LanguageSelectionSet11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_LanguageSelectionSet::Deserialize(SC_LanguageSelectionSet *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x25
  unsigned int cache; // w0
  int v6; // w1
  unsigned int v7; // w22
  int v8; // w20
  unsigned int v9; // w2
  int v10; // w1
  unsigned int v11; // w25
  unsigned int v12; // w0
  int v13; // w1
  __int16 v14; // w25
  unsigned int v15; // w0
  int v16; // w1
  unsigned int v17; // w0
  unsigned int v18; // w20
  unsigned int v19; // w1
  int v20; // w0
  unsigned int v21; // w20
  __int64 result; // x0
  unsigned int v23; // w0
  int v24; // w1
  unsigned int v25; // w0
  unsigned int v26; // w0
  int v27; // w1
  unsigned int v28; // w0
  unsigned int v29; // w2
  const unsigned __int8 *cur; // x0
  int bitsLeft; // w3
  __int64 v32; // [xsp+40h] [xbp+40h]
  __int16 v33; // [xsp+56h] [xbp+56h] BYREF
  __int64 v34; // [xsp+58h] [xbp+58h]
  __int64 v35; // [xsp+60h] [xbp+60h]
  cache = bs->cache;
  v6 = bs->bitsLeft - 6;
  bs->bitsLeft = v6;
  v7 = cache >> 26;
  if ( v6 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v29 = bs->cache;
    cur = bs->cur;
    bitsLeft = bs->bitsLeft;
    v6 = bitsLeft + 32;
    bs->bitsLeft = bitsLeft + 32;
    bs->cur = cur + 4;
    v7 = (unsigned __int8)((v29 >> (bitsLeft + 32)) | v7);
    if ( bitsLeft == -32 )
      bs->cache = 0;
    else
      bs->cache = v29 << -(char)bitsLeft;
  }
  else
  {
    bs->cache = cache << 6;
  }
  if ( *(_QWORD *)this != *((_QWORD *)this + 1) )
  {
    *((_QWORD *)this + 1) = *(_QWORD *)this;
    v6 = bs->bitsLeft;
  }
  v8 = 0;
  if ( v7 )
  {
    v32 = v2;
    do
    {
      while ( 1 )
      {
        v9 = bs->cache;
        v10 = v6 - 6;
        bs->bitsLeft = v10;
        v11 = v9 >> 26;
        if ( v10 < 0 )
          break;
        bs->cache = v9 << 6;
        ++v8;
        v33 = v9 >> 26;
        std::vector<unsigned short>::emplace_back<unsigned short>(this, &v33);
        v6 = bs->bitsLeft;
        if ( v8 == v7 )
          goto LABEL_12;
      }
      ++v8;
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v12 = bs->cache;
      v13 = bs->bitsLeft;
      v14 = v11 | (v12 >> (v13 + 32));
      v15 = v12 << -(char)v13;
      if ( v13 == -32 )
        v15 = 0;
      bs->cur += 4;
      bs->cache = v15;
      bs->bitsLeft = v13 + 32;
      v33 = v14;
      std::vector<unsigned short>::emplace_back<unsigned short>(this, &v33);
      v6 = bs->bitsLeft;
    }
    while ( v8 != v7 );
LABEL_12:
    v2 = v32;
  }
  v16 = v6 - 15;
  v17 = bs->cache;
  bs->bitsLeft = v16;
  v18 = v17 >> 17;
  if ( v16 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v26 = bs->cache;
    v27 = bs->bitsLeft;
    LOWORD(v18) = v18 | (v26 >> (v27 + 32));
    v28 = v26 << -(char)v27;
    if ( v27 == -32 )
      v28 = 0;
    bs->cur += 4;
    bs->cache = v28;
    bs->bitsLeft = v27 + 32;
  }
  else
  {
    bs->cache = v17 << 15;
  }
  *((_WORD *)this + 12) = v18;
  v19 = bs->cache;
  v20 = bs->bitsLeft - 1;
  bs->bitsLeft = v20;
  v21 = v19 >> 31;
  if ( v20 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v23 = bs->cache;
    v24 = bs->bitsLeft;
    LOBYTE(v21) = v21 | (v23 >> (v24 + 32));
    v25 = v23 << -(char)v24;
    if ( v24 == -32 )
      v25 = 0;
    bs->cur += 4;
    bs->cache = v25;
    bs->bitsLeft = v24 + 32;
  }
  else
  {
    bs->cache = 2 * v19;
  }
  result = 0;
  *((_BYTE *)this + 26) = (unsigned __int8)v21 != 0;
  if ( (_BYTE)v21 )
    result = SC_Tag::Deserialize((SC_LanguageSelectionSet *)((char *)this + 32), bs);
  if ( v34 != _stack_chk_guard )
    v32 = v2;
  return result;
}


