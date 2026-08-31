/* ============================================================================
 * SC_LanguageSpecificItem
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 868 bytes of .text.
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
/* 0x000a43c0    240  SC_LanguageSpecificItem::operator==(SC_LanguageSpecificItem const&) const */
/* 0x000a63d0    628  SC_LanguageSpecificItem::Deserialize(TBitstreamReader<unsigned int>&, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000a43c0  size 240 bytes  60 insns
 *   SC_LanguageSpecificItem::operator==(SC_LanguageSpecificItem const&) const
 *   [_ZNK23SC_LanguageSpecificItemeqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_LanguageSpecificItem::operator==(SC_LanguageSpecificItem *this, const SC_LanguageSpecificItem *a)
{
  __int64 v2; // x23
  const void *v5; // x0
  const void *v6; // x1
  size_t v7; // x2
  bool result; // w0
  __int64 v9; // [xsp+30h] [xbp+30h]
  __int64 v10; // [xsp+48h] [xbp+48h]
  __int64 v11; // [xsp+50h] [xbp+50h]
  v5 = *(const void **)this;
  v6 = *(const void **)a;
  v7 = *((_QWORD *)this + 1) - (_QWORD)v5;
  result = v7 == *((_QWORD *)a + 1) - (_QWORD)v6
        && (!v7 || !memcmp(v5, v6, v7))
        && (unsigned __int8)apac::scene::BCP47LanguageString::operator==(
                              (SC_LanguageSpecificItem *)((char *)this + 24),
                              (const SC_LanguageSpecificItem *)((char *)a + 24))
        && *((unsigned __int8 *)this + 64) == *((unsigned __int8 *)a + 64)
        && *((unsigned __int8 *)this + 72) == *((unsigned __int8 *)a + 72)
        && *((unsigned __int8 *)this + 73) == *((unsigned __int8 *)a + 73);
  if ( v10 != _stack_chk_guard )
    v9 = v2;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a63d0  size 628 bytes  157 insns
 *   SC_LanguageSpecificItem::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN23SC_LanguageSpecificItem11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_LanguageSpecificItem::Deserialize(
        SC_LanguageSpecificItem *this,
        TBitstreamReader_u32 *bs,
        bool flag)
{
  __int64 v3; // x25
  unsigned int cache; // w1
  int v7; // w0
  unsigned int v8; // w22
  int v9; // w20
  unsigned int v10; // w2
  int v11; // w1
  unsigned int v12; // w25
  unsigned int v13; // w0
  int v14; // w1
  __int16 v15; // w25
  unsigned int v16; // w0
  unsigned int v17; // w20
  unsigned int v19; // w1
  int v20; // w0
  unsigned int v21; // w22
  unsigned int v22; // w1
  int v23; // w0
  unsigned int v24; // w22
  unsigned int v25; // w2
  int bitsLeft; // w3
  const unsigned __int8 *cur; // x0
  unsigned int v28; // w0
  int v29; // w1
  unsigned int v30; // w0
  unsigned int v31; // w0
  int v32; // w1
  unsigned int v33; // w0
  __int64 v34; // [xsp+40h] [xbp+40h]
  __int16 v35; // [xsp+56h] [xbp+56h] BYREF
  __int64 v36; // [xsp+58h] [xbp+58h]
  __int64 v37; // [xsp+60h] [xbp+60h]
  if ( !flag )
  {
    cache = bs->cache;
    v7 = bs->bitsLeft - 2;
    bs->bitsLeft = v7;
    v8 = cache >> 30;
    if ( v7 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v25 = bs->cache;
      bitsLeft = bs->bitsLeft;
      cur = bs->cur;
      bs->bitsLeft = bitsLeft + 32;
      bs->cur = cur + 4;
      v8 = (unsigned __int8)((v25 >> (bitsLeft + 32)) | v8);
      if ( bitsLeft == -32 )
        bs->cache = 0;
      else
        bs->cache = v25 << -(char)bitsLeft;
    }
    else
    {
      bs->cache = 4 * cache;
    }
    if ( *(_QWORD *)this != *((_QWORD *)this + 1) )
      *((_QWORD *)this + 1) = *(_QWORD *)this;
    if ( v8 )
    {
      v9 = 0;
      v34 = v3;
      do
      {
        while ( 1 )
        {
          v10 = bs->cache;
          v11 = bs->bitsLeft - 6;
          bs->bitsLeft = v11;
          v12 = v10 >> 26;
          if ( v11 < 0 )
            break;
          bs->cache = v10 << 6;
          ++v9;
          v35 = v10 >> 26;
          std::vector<unsigned short>::emplace_back<unsigned short>(this, &v35);
          if ( v8 == v9 )
            goto LABEL_13;
        }
        ++v9;
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v13 = bs->cache;
        v14 = bs->bitsLeft;
        v15 = v12 | (v13 >> (v14 + 32));
        v16 = v13 << -(char)v14;
        if ( v14 == -32 )
          v16 = 0;
        bs->cur += 4;
        bs->cache = v16;
        bs->bitsLeft = v14 + 32;
        v35 = v15;
        std::vector<unsigned short>::emplace_back<unsigned short>(this, &v35);
      }
      while ( v8 != v9 );
LABEL_13:
      v3 = v34;
    }
  }
  v17 = apac::scene::SC_Language::Deserialize((SC_LanguageSpecificItem *)((char *)this + 24), bs);
  if ( !v17 )
  {
    v19 = bs->cache;
    v20 = bs->bitsLeft - 1;
    bs->bitsLeft = v20;
    v21 = v19 >> 31;
    if ( v20 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v31 = bs->cache;
      v32 = bs->bitsLeft;
      LOBYTE(v21) = v21 | (v31 >> (v32 + 32));
      v33 = v31 << -(char)v32;
      if ( v32 == -32 )
        v33 = 0;
      bs->cur += 4;
      bs->cache = v33;
      bs->bitsLeft = v32 + 32;
    }
    else
    {
      bs->cache = 2 * v19;
    }
    *((_BYTE *)this + 72) = (unsigned __int8)v21 != 0;
    v22 = bs->cache;
    v23 = bs->bitsLeft - 1;
    bs->bitsLeft = v23;
    v24 = v22 >> 31;
    if ( v23 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v28 = bs->cache;
      v29 = bs->bitsLeft;
      v24 |= v28 >> (v29 + 32);
      v30 = v28 << -(char)v29;
      if ( v29 == -32 )
        v30 = 0;
      bs->cur += 4;
      bs->cache = v30;
      bs->bitsLeft = v29 + 32;
    }
    else
    {
      bs->cache = 2 * v22;
    }
    *((_BYTE *)this + 73) = (unsigned __int8)v24 != 0;
  }
  if ( v36 != _stack_chk_guard )
    v34 = v3;
  return v17;
}


