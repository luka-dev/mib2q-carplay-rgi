/* ============================================================================
 * SC_NonLanguageItem
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 748 bytes of .text.
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
/* 0x000a42e0    220  SC_NonLanguageItem::operator==(SC_NonLanguageItem const&) const */
/* 0x000a6650    528  SC_NonLanguageItem::Deserialize(TBitstreamReader<unsigned int>&, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000a42e0  size 220 bytes  55 insns
 *   SC_NonLanguageItem::operator==(SC_NonLanguageItem const&) const
 *   [_ZNK18SC_NonLanguageItemeqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_NonLanguageItem::operator==(SC_NonLanguageItem *this, const SC_NonLanguageItem *a)
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
  if ( *((unsigned __int8 *)a + 24) != *((unsigned __int8 *)this + 24) )
    return 0;
  v2 = 1;
  if ( !*((_BYTE *)a + 24) )
    return v2;
  return SC_Tag::operator==((SC_NonLanguageItem *)((char *)this + 32), (const SC_NonLanguageItem *)((char *)a + 32));
}


/* ------------------------------------------------------------------------ */
/* 0x000a6650  size 528 bytes  132 insns
 *   SC_NonLanguageItem::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN18SC_NonLanguageItem11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_NonLanguageItem::Deserialize(SC_NonLanguageItem *this, TBitstreamReader_u32 *bs, bool flag)
{
  __int64 v3; // x23
  __int64 v4; // x24
  bool v5; // zf
  int bitsLeft; // w1
  unsigned int cache; // w2
  int v10; // w1
  unsigned int v11; // w22
  int v12; // w21
  int v13; // w1
  unsigned int v14; // w23
  unsigned int v15; // w0
  int v16; // w1
  __int16 v17; // w23
  unsigned int v18; // w0
  int v19; // w1
  unsigned int v20; // w21
  __int64 result; // x0
  unsigned int v22; // w0
  int v23; // w1
  unsigned int v24; // w0
  unsigned int v25; // w2
  int v26; // w3
  const unsigned __int8 *cur; // x0
  __int64 v28; // [xsp+30h] [xbp+30h]
  __int64 v29; // [xsp+38h] [xbp+38h]
  __int16 v30; // [xsp+56h] [xbp+56h] BYREF
  __int64 v31; // [xsp+58h] [xbp+58h]
  __int64 v32; // [xsp+60h] [xbp+60h]
  v5 = !flag;
  cache = bs->cache;
  bitsLeft = bs->bitsLeft;
  if ( v5 )
  {
    v28 = v3;
    v10 = bitsLeft - 6;
    v11 = cache >> 26;
    bs->bitsLeft = v10;
    if ( v10 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v25 = bs->cache;
      v26 = bs->bitsLeft;
      cur = bs->cur;
      bs->bitsLeft = v26 + 32;
      bs->cur = cur + 4;
      v11 = (unsigned __int8)((v25 >> (v26 + 32)) | v11);
      if ( v26 == -32 )
        bs->cache = 0;
      else
        bs->cache = v25 << -(char)v26;
    }
    else
    {
      bs->cache = cache << 6;
    }
    if ( *(_QWORD *)this != *((_QWORD *)this + 1) )
      *((_QWORD *)this + 1) = *(_QWORD *)this;
    cache = bs->cache;
    bitsLeft = bs->bitsLeft;
    v12 = 0;
    if ( v11 )
    {
      do
      {
        while ( 1 )
        {
          v13 = bitsLeft - 6;
          bs->bitsLeft = v13;
          v14 = cache >> 26;
          if ( v13 < 0 )
            break;
          bs->cache = cache << 6;
          ++v12;
          v30 = cache >> 26;
          std::vector<unsigned short>::emplace_back<unsigned short>(this, &v30);
          cache = bs->cache;
          bitsLeft = bs->bitsLeft;
          if ( v12 == v11 )
            goto LABEL_14;
        }
        ++v12;
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v15 = bs->cache;
        v16 = bs->bitsLeft;
        v17 = v14 | (v15 >> (v16 + 32));
        v18 = v15 << -(char)v16;
        if ( v16 == -32 )
          v18 = 0;
        bs->cur += 4;
        bs->cache = v18;
        bs->bitsLeft = v16 + 32;
        v30 = v17;
        std::vector<unsigned short>::emplace_back<unsigned short>(this, &v30);
        cache = bs->cache;
        bitsLeft = bs->bitsLeft;
      }
      while ( v12 != v11 );
    }
LABEL_14:
    v3 = v28;
  }
  v19 = bitsLeft - 1;
  bs->bitsLeft = v19;
  v20 = cache >> 31;
  if ( v19 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v22 = bs->cache;
    v23 = bs->bitsLeft;
    LOBYTE(v20) = v20 | (v22 >> (v23 + 32));
    v24 = v22 << -(char)v23;
    if ( v23 == -32 )
      v24 = 0;
    bs->cur += 4;
    bs->cache = v24;
    bs->bitsLeft = v23 + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  result = 0;
  *((_BYTE *)this + 24) = (unsigned __int8)v20 != 0;
  if ( (_BYTE)v20 )
    result = SC_Tag::Deserialize((SC_NonLanguageItem *)((char *)this + 32), bs);
  if ( v31 != _stack_chk_guard )
  {
    v28 = v3;
    v29 = v4;
  }
  return result;
}


