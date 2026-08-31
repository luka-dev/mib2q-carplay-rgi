/* ============================================================================
 * SC_PresetDescription
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 1636 bytes of .text.
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
/* 0x000a4b40    236  SC_PresetDescription::SC_PresetCharacteristics::operator==(SC_PresetDescription::SC_PresetCharacteristics const&) const */
/* 0x000a4c30    292  SC_PresetDescription::operator==(SC_PresetDescription const&) const */
/* 0x000a7d50    548  SC_PresetDescription::SC_PresetCharacteristics::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x000a7f80    560  SC_PresetDescription::Deserialize(TBitstreamReader<unsigned int>&, bool, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000a4b40  size 236 bytes  59 insns
 *   SC_PresetDescription::SC_PresetCharacteristics::operator==(SC_PresetDescription::SC_PresetCharacteristics const&) const
 *   [_ZNK20SC_PresetDescription24SC_PresetCharacteristicseqERKS0_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_PresetDescription::SC_PresetCharacteristics::operator==(
        SC_PresetDescription__SC_PresetCharacteristics *this,
        const SC_PresetDescription__SC_PresetCharacteristics *a)
{
  SC_PresetDescription__SC_PresetCharacteristics *v2; // x3
  int v3; // w5
  __int64 v4; // x6
  __int64 v5; // x8
  __int64 v6; // x9
  unsigned int v7; // w7
  __int64 v8; // x4
  int v9; // w1
  __int64 v11; // [xsp+20h] [xbp+20h]
  v2 = this;
  v3 = *(unsigned __int8 *)this;
  LOBYTE(this) = 0;
  if ( v3 == *(unsigned __int8 *)a )
  {
    v4 = *((_QWORD *)v2 + 1);
    v5 = *((_QWORD *)a + 1);
    v6 = *((_QWORD *)v2 + 3);
    v7 = *((_DWORD *)v2 + 8);
    if ( v7 + 8 * (v6 - v4) == *((unsigned int *)a + 8) + 8 * (*((_QWORD *)a + 3) - v5) )
    {
      v8 = 0;
LABEL_5:
      v9 = 0;
      while ( 1 )
      {
        LODWORD(this) = v7 == v9 && v6 == v4 + v8;
        if ( (_DWORD)this || (((1LL << v9) & *(_QWORD *)(v4 + v8)) != 0) != (((1LL << v9) & *(_QWORD *)(v5 + v8)) != 0) )
          break;
        if ( ++v9 == 64 )
        {
          v8 += 8;
          goto LABEL_5;
        }
      }
    }
  }
  return (char)this;
}


/* ------------------------------------------------------------------------ */
/* 0x000a4c30  size 292 bytes  73 insns
 *   SC_PresetDescription::operator==(SC_PresetDescription const&) const
 *   [_ZNK20SC_PresetDescriptioneqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_PresetDescription::operator==(SC_PresetDescription *this, const SC_PresetDescription *a)
{
  __int64 v2; // x23
  __int64 result; // x0
  const void *v6; // x1
  size_t v7; // x2
  __int64 v8; // [xsp+30h] [xbp+30h]
  __int64 v9; // [xsp+48h] [xbp+48h]
  __int64 v10; // [xsp+50h] [xbp+50h]
  if ( !SC_Tag::operator==(this, a) )
    goto LABEL_2;
  if ( !SC_PresetDescription::SC_PresetCharacteristics::operator==(
          (SC_PresetDescription *)((char *)this + 112),
          (const SC_PresetDescription *)((char *)a + 112)) )
    goto LABEL_2;
  result = (unsigned __int8)apac::scene::BCP47LanguageString::operator==(
                              (SC_PresetDescription *)((char *)this + 160),
                              (const SC_PresetDescription *)((char *)a + 160));
  if ( !(_DWORD)result || *((unsigned __int8 *)this + 200) != *((unsigned __int8 *)a + 200) )
    goto LABEL_2;
  if ( !*((_BYTE *)this + 240) )
    goto LABEL_3;
  if ( *((unsigned __int8 *)this + 208) != *((unsigned __int8 *)a + 208)
    || *((unsigned __int8 *)this + 209) != *((unsigned __int8 *)a + 209)
    || (v6 = *((const void **)a + 27),
        v7 = *((_QWORD *)this + 28) - *((_QWORD *)this + 27),
        v7 != *((_QWORD *)a + 28) - (_QWORD)v6) )
  {
LABEL_2:
    result = 0;
    goto LABEL_3;
  }
  result = *((unsigned __int8 *)this + 240);
  if ( v7 )
    result = memcmp(*((const void **)this + 27), v6, v7) == 0;
LABEL_3:
  if ( v9 != _stack_chk_guard )
    v8 = v2;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a7d50  size 548 bytes  136 insns
 *   SC_PresetDescription::SC_PresetCharacteristics::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN20SC_PresetDescription24SC_PresetCharacteristics11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x330880, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_PresetDescription::SC_PresetCharacteristics::Deserialize(
        SC_PresetDescription__SC_PresetCharacteristics *this,
        TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w1
  int v5; // w0
  unsigned int v6; // w19
  unsigned int v7; // w0
  unsigned __int16 v8; // w2
  __int64 v9; // x4
  __int64 v10; // x0
  void *v11; // x1
  unsigned int v12; // w2
  __int64 v13; // x0
  unsigned int v14; // w23
  unsigned __int64 v15; // x19
  __int64 v16; // x0
  __int64 v17; // x20
  unsigned int v18; // w2
  __int64 v19; // x20
  int v20; // w1
  __int64 *v21; // x19
  unsigned int v22; // w25
  unsigned int v23; // w0
  int v24; // w1
  unsigned int v25; // w0
  unsigned int v27; // w2
  int bitsLeft; // w3
  const unsigned __int8 *cur; // x1
  void *v30; // [xsp+68h] [xbp+68h]
  __int64 v31; // [xsp+70h] [xbp+70h]
  __int64 v32; // [xsp+80h] [xbp+80h]
  cache = bs->cache;
  v5 = bs->bitsLeft - 2;
  bs->bitsLeft = v5;
  v6 = cache >> 30;
  if ( v5 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v27 = bs->cache;
    bitsLeft = bs->bitsLeft;
    cur = bs->cur;
    bs->bitsLeft = bitsLeft + 32;
    bs->cur = cur + 4;
    LOBYTE(v6) = v6 | (v27 >> (bitsLeft + 32));
    v7 = (unsigned __int8)v6;
    if ( bitsLeft == -32 )
      bs->cache = 0;
    else
      bs->cache = v27 << -(char)bitsLeft;
  }
  else
  {
    v7 = cache >> 30;
    bs->cache = 4 * cache;
  }
  *(_BYTE *)this = v6;
  v8 = word_330880[v7];
  v9 = *((_QWORD *)this + 1);
  v10 = v8;
  v11 = *((void **)this + 3);
  if ( (unsigned __int64)*((unsigned int *)this + 8) + 8 * ((__int64)v11 - v9) <= v8 )
  {
    LODWORD(v31) = *((_DWORD *)this + 8);
    v30 = v11;
    std::vector<bool>::_M_fill_insert((_DWORD)this + 8, v11);
    v12 = *((_DWORD *)this + 8);
    v9 = *((_QWORD *)this + 1);
    v13 = *((_QWORD *)this + 3);
  }
  else
  {
    v12 = v8 & 0x3F;
    *((_DWORD *)this + 8) = v12;
    v13 = v9 + 8 * (v10 >> 6);
    *((_QWORD *)this + 3) = v13;
  }
  v14 = 0;
  v15 = 0;
  if ( v12 + 8 * (v13 - v9) )
  {
    do
    {
      v18 = bs->cache;
      v19 = 1LL << v14;
      v20 = bs->bitsLeft - 1;
      bs->bitsLeft = v20;
      v21 = (__int64 *)(v9 + 8 * (v15 >> 6));
      v22 = v18 >> 31;
      if ( v20 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v23 = bs->cache;
        v24 = bs->bitsLeft;
        v22 |= v23 >> (v24 + 32);
        v25 = v23 << -(char)v24;
        if ( v24 == -32 )
          v25 = 0;
        bs->cur += 4;
        bs->cache = v25;
        bs->bitsLeft = v24 + 32;
      }
      else
      {
        bs->cache = 2 * v18;
      }
      ++v14;
      v16 = v19 | *v21;
      v17 = *v21 & ~v19;
      if ( (_BYTE)v22 )
        v17 = v16;
      *v21 = v17;
      v15 = v14;
      v9 = *((_QWORD *)this + 1);
    }
    while ( (unsigned __int64)*((unsigned int *)this + 8) + 8 * (*((_QWORD *)this + 3) - v9) > v14 );
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000a7f80  size 560 bytes  140 insns
 *   SC_PresetDescription::Deserialize(TBitstreamReader<unsigned int>&, bool, bool)
 *   [_ZN20SC_PresetDescription11DeserializeER16TBitstreamReaderIjEbb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_PresetDescription::Deserialize(
        SC_PresetDescription *this,
        TBitstreamReader_u32 *bs,
        bool flag,
        bool flag2)
{
  _BOOL4 v4; // w23
  _BOOL4 v6; // w22
  unsigned int v8; // w19
  unsigned int cache; // w1
  int v11; // w0
  unsigned int v12; // w22
  unsigned int v13; // w1
  int v14; // w0
  unsigned int v15; // w22
  __int64 v16; // x2
  __int64 v17; // x3
  int v18; // w0
  __int64 v19; // x2
  __int64 v20; // x22
  unsigned int v21; // w1
  int v22; // w0
  unsigned int v23; // w23
  unsigned int v24; // w0
  int v25; // w1
  unsigned int v26; // w0
  unsigned int v27; // w0
  int v28; // w1
  unsigned int v29; // w0
  unsigned int v30; // w0
  int bitsLeft; // w1
  unsigned int v32; // w0
  __int64 v33; // [xsp+50h] [xbp+50h]
  v4 = flag;
  v6 = flag2;
  v8 = SC_Tag::Deserialize(this, bs);
  if ( !v8 )
  {
    v8 = SC_PresetDescription::SC_PresetCharacteristics::Deserialize((SC_PresetDescription *)((char *)this + 112), bs);
    if ( !v8 )
    {
      v8 = apac::scene::SC_Language::Deserialize((SC_PresetDescription *)((char *)this + 160), bs);
      if ( !v8 )
      {
        *((_BYTE *)this + 240) = v6;
        if ( v6 && !v4 )
        {
          cache = bs->cache;
          v11 = bs->bitsLeft - 1;
          bs->bitsLeft = v11;
          v12 = cache >> 31;
          if ( v11 < 0 )
          {
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
            v30 = bs->cache;
            bitsLeft = bs->bitsLeft;
            LOBYTE(v12) = v12 | (v30 >> (bitsLeft + 32));
            v32 = v30 << -(char)bitsLeft;
            if ( bitsLeft == -32 )
              v32 = 0;
            bs->cur += 4;
            bs->cache = v32;
            bs->bitsLeft = bitsLeft + 32;
          }
          else
          {
            bs->cache = 2 * cache;
          }
          *((_BYTE *)this + 208) = (unsigned __int8)v12 != 0;
          v13 = bs->cache;
          v14 = bs->bitsLeft - 6;
          bs->bitsLeft = v14;
          v15 = v13 >> 26;
          if ( v14 < 0 )
          {
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
            v27 = bs->cache;
            v28 = bs->bitsLeft;
            v15 |= v27 >> (v28 + 32);
            v29 = v27 << -(char)v28;
            if ( v28 == -32 )
              v29 = 0;
            bs->cur += 4;
            bs->cache = v29;
            bs->bitsLeft = v28 + 32;
          }
          else
          {
            bs->cache = v13 << 6;
          }
          v16 = *((_QWORD *)this + 27);
          v17 = *((_QWORD *)this + 28);
          v18 = (unsigned __int8)v15;
          *((_BYTE *)this + 209) = v15;
          if ( (unsigned __int8)v15 > (unsigned __int64)(v17 - v16) )
          {
            std::vector<unsigned char>::_M_default_append((_DWORD)this + 216, (unsigned __int8)v15 - (v17 - v16));
            v18 = *((unsigned __int8 *)this + 209);
          }
          else if ( (unsigned __int8)v15 < (unsigned __int64)(v17 - v16) )
          {
            v19 = v16 + (unsigned __int8)v15;
            if ( v17 != v19 )
              *((_QWORD *)this + 28) = v19;
          }
          if ( v18 )
          {
            v20 = 0;
            do
            {
              v21 = bs->cache;
              v22 = bs->bitsLeft - 6;
              bs->bitsLeft = v22;
              v23 = v21 >> 26;
              if ( v22 < 0 )
              {
                TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
                v24 = bs->cache;
                v25 = bs->bitsLeft;
                LOBYTE(v23) = v23 | (v24 >> (v25 + 32));
                v26 = v24 << -(char)v25;
                if ( v25 == -32 )
                  v26 = 0;
                bs->cur += 4;
                bs->cache = v26;
                bs->bitsLeft = v25 + 32;
              }
              else
              {
                bs->cache = v21 << 6;
              }
              *(_BYTE *)(*((_QWORD *)this + 27) + v20++) = v23;
            }
            while ( *((unsigned __int8 *)this + 209) > (unsigned int)v20 );
          }
        }
      }
    }
  }
  return v8;
}


