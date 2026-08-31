/* ============================================================================
 * SC_SceneComposition
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 8 function(s), 7200 bytes of .text.
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
/* 0x000a4d60    316  SC_SceneComposition::SC_Category::operator==(SC_SceneComposition::SC_Category const&) const */
/* 0x000a4ea0    288  SC_SceneComposition::SC_Category::CheckConformance(unsigned short, unsigned short) const */
/* 0x000a4fc0    224  SC_SceneComposition::GetSelectedItemIndex(unsigned int, unsigned int) const */
/* 0x000a50a0    836  SC_SceneComposition::operator==(SC_SceneComposition const&) const */
/* 0x000a53f0    456  SC_SceneComposition::CheckConformance() const */
/* 0x000a6030    916  SC_SceneComposition::SC_Category::Deserialize(TBitstreamReader<unsigned int>&, unsigned short) */
/* 0x000a81b0   3236  SC_SceneComposition::Deserialize(TBitstreamReader<unsigned int>&, bool) */
/* 0x000aaa60    928  SC_SceneComposition::~SC_SceneComposition() */

/* ------------------------------------------------------------------------ */
/* 0x000a4d60  size 316 bytes  79 insns
 *   SC_SceneComposition::SC_Category::operator==(SC_SceneComposition::SC_Category const&) const
 *   [_ZNK19SC_SceneComposition11SC_CategoryeqERKS0_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneComposition::SC_Category::operator==(
        SC_SceneComposition__SC_Category *this,
        const SC_SceneComposition__SC_Category *sceneComp)
{
  __int64 v2; // x23
  unsigned int v3; // w22
  const void *v6; // x0
  const void *v7; // x1
  size_t v8; // x2
  int v10; // w0
  int v11; // w23
  const void *v12; // x0
  const void *v13; // x1
  size_t v14; // x2
  __int64 v15; // [xsp+30h] [xbp+30h]
  __int64 v16; // [xsp+48h] [xbp+48h]
  __int64 v17; // [xsp+50h] [xbp+50h]
  v3 = 0;
  v6 = *(const void **)this;
  v7 = *(const void **)sceneComp;
  v8 = *((_QWORD *)this + 1) - (_QWORD)v6;
  if ( v8 == *((_QWORD *)sceneComp + 1) - (_QWORD)v7 && (!v8 || !memcmp(v6, v7, v8)) )
  {
    v10 = *((unsigned __int8 *)sceneComp + 24);
    if ( v10 == *((unsigned __int8 *)this + 24) )
    {
      v15 = v2;
      v11 = *((unsigned __int8 *)this + 56);
      if ( *((unsigned __int8 *)sceneComp + 56) == v11 )
      {
        if ( !v10
          || (v12 = *((const void **)this + 4),
              v3 = 0,
              v13 = *((const void **)sceneComp + 4),
              v14 = *((_QWORD *)this + 5) - (_QWORD)v12,
              v14 == *((_QWORD *)sceneComp + 5) - (_QWORD)v13)
          && (!v14 || !memcmp(v12, v13, v14)) )
        {
          v3 = 1;
          if ( v11 )
            return SC_Tag::operator==(
                     (SC_SceneComposition__SC_Category *)((char *)this + 64),
                     (const SC_SceneComposition__SC_Category *)((char *)sceneComp + 64));
        }
        v2 = v15;
      }
      else
      {
        v3 = 0;
        v2 = v15;
      }
    }
    else
    {
      v3 = 0;
    }
  }
  if ( v16 != _stack_chk_guard )
    v15 = v2;
  return v3;
}


/* ------------------------------------------------------------------------ */
/* 0x000a4ea0  size 288 bytes  69 insns
 *   SC_SceneComposition::SC_Category::CheckConformance(unsigned short, unsigned short) const
 *   [_ZNK19SC_SceneComposition11SC_Category16CheckConformanceEtt]
 *   data refs: 0x330730, 0x330758, 0x330788, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneComposition::SC_Category::CheckConformance(
        SC_SceneComposition__SC_Category *this,
        unsigned __int16 v,
        unsigned __int16 v2)
{
  unsigned __int16 *v4; // x5
  unsigned __int16 *v5; // x7
  unsigned __int16 *v6; // x3
  __int64 result; // x0
  unsigned __int16 *v8; // x2
  unsigned __int16 *v9; // x3
  unsigned __int16 *v10; // x0
  __int64 v11; // [xsp+30h] [xbp+30h]
  v5 = *(unsigned __int16 **)this;
  v4 = *((unsigned __int16 **)this + 1);
  if ( v4 == *(unsigned __int16 **)this )
  {
LABEL_7:
    result = 0;
    if ( *((_BYTE *)this + 24) )
    {
      v8 = *((unsigned __int16 **)this + 4);
      v9 = *((unsigned __int16 **)this + 5);
      if ( v9 - v8 == v )
      {
        if ( v8 != v9 )
        {
          v10 = *((unsigned __int16 **)this + 4);
          do
          {
            if ( *v10 >= (unsigned int)(unsigned __int16)((unsigned int)((_DWORD)v4 - (_DWORD)v5) >> 1) )
            {
              __printf_chk(1, "ERROR: Member index out of range\n");
              return 0xFFFFFFFFLL;
            }
            ++v10;
          }
          while ( v9 != v10 );
          return 0;
        }
      }
      else
      {
        __printf_chk(1, "ERROR: SceneComposition has inconsistent size\n");
        return 0xFFFFFFFFLL;
      }
    }
  }
  else
  {
    v6 = *(unsigned __int16 **)this;
    while ( *v6 < (unsigned int)v2 )
    {
      if ( v4 == ++v6 )
        goto LABEL_7;
    }
    __printf_chk(1, "ERROR: Selection group index out of range\n");
    return 0xFFFFFFFFLL;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a4fc0  size 224 bytes  54 insns
 *   SC_SceneComposition::GetSelectedItemIndex(unsigned int, unsigned int) const
 *   [_ZNK19SC_SceneComposition20GetSelectedItemIndexEjj]
 *   data refs: 0x3307b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneComposition::GetSelectedItemIndex(
        SC_SceneComposition *this,
        unsigned int n,
        unsigned int n2)
{
  _QWORD *v3; // x1
  __int64 v4; // x4
  __int64 v5; // x3
  __int64 v7; // x5
  unsigned int v8; // w2
  __int64 v9; // [xsp+30h] [xbp+30h]
  v3 = (_QWORD *)(*((_QWORD *)this + 16) + ((unsigned __int64)n << 8));
  v4 = v3[1];
  v5 = (v3[2] - v4) >> 1;
  if ( (unsigned int)v5 > n2 )
    return *(unsigned __int16 *)(v4 + 2LL * n2);
  v7 = *((_QWORD *)this + 13);
  v8 = *(unsigned __int16 *)(v3[4] + 2LL * (n2 - (unsigned int)v5));
  if ( v8 < 954437177 * (unsigned int)((*((_QWORD *)this + 14) - v7) >> 4) )
    return **(unsigned __int16 **)(v7 + 144LL * v8)
         + 954437177 * (unsigned int)((__int64)(*((_QWORD *)this + 8) - *((_QWORD *)this + 7)) >> 4);
  __printf_chk(1, "ERROR: selectedSelectionSetIndex out of range\n");
  return 0xFFFFFFFFLL;
}


/* ------------------------------------------------------------------------ */
/* 0x000a50a0  size 836 bytes  206 insns
 *   SC_SceneComposition::operator==(SC_SceneComposition const&) const
 *   [_ZNK19SC_SceneCompositioneqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_SceneComposition::operator==(SC_SceneComposition *this, const SC_SceneComposition *sceneComp)
{
  __int64 v2; // x23
  __int64 v3; // x24
  int v6; // w0
  SC_NonLanguageItem *v8; // x21
  SC_NonLanguageItem *v9; // x24
  const SC_NonLanguageItem *v10; // x23
  SC_LanguageSpecificItem *v11; // x21
  SC_LanguageSpecificItem *v12; // x24
  const SC_LanguageSpecificItem *v13; // x23
  SC_LanguageSelectionSet *v14; // x21
  SC_LanguageSelectionSet *v15; // x24
  const SC_LanguageSelectionSet *v16; // x23
  SC_CompositionSelectionGroup *v17; // x21
  SC_CompositionSelectionGroup *v18; // x24
  const SC_CompositionSelectionGroup *v19; // x23
  SC_PresetDescription *v20; // x21
  SC_PresetDescription *v21; // x24
  const SC_PresetDescription *v22; // x23
  __int64 v23; // x0
  __int64 v24; // x2
  int v25; // w24
  __int64 v26; // [xsp+30h] [xbp+30h]
  __int64 v27; // [xsp+38h] [xbp+38h]
  __int64 v28; // [xsp+48h] [xbp+48h]
  __int64 v29; // [xsp+50h] [xbp+50h]
  if ( *(unsigned __int8 *)this == *(unsigned __int8 *)sceneComp )
  {
    v6 = *((unsigned __int8 *)sceneComp + 1);
    if ( v6 == *((unsigned __int8 *)this + 1) )
    {
      v26 = v2;
      v27 = v3;
      if ( v6
        && (!(unsigned __int8)apac::scene::BCP47LanguageString::operator==(
                                (SC_SceneComposition *)((char *)this + 8),
                                (const SC_SceneComposition *)((char *)sceneComp + 8))
         || *((unsigned __int8 *)this + 48) != *((unsigned __int8 *)sceneComp + 48)) )
      {
        goto LABEL_8;
      }
      v8 = *((SC_NonLanguageItem **)this + 7);
      v9 = *((SC_NonLanguageItem **)this + 8);
      v10 = *((const SC_NonLanguageItem **)sceneComp + 7);
      if ( v9 - v8 != *((_QWORD *)sceneComp + 8) - (_QWORD)v10 )
        goto LABEL_8;
      if ( v8 != v9 )
      {
        while ( (unsigned __int8)SC_NonLanguageItem::operator==(v8, v10) )
        {
          v8 = (SC_NonLanguageItem *)((char *)v8 + 144);
          v10 = (const SC_NonLanguageItem *)((char *)v10 + 144);
          if ( v9 == v8 )
            goto LABEL_15;
        }
        v2 = v26;
        v3 = v27;
        goto LABEL_3;
      }
LABEL_15:
      v11 = *((SC_LanguageSpecificItem **)this + 10);
      v12 = *((SC_LanguageSpecificItem **)this + 11);
      v13 = *((const SC_LanguageSpecificItem **)sceneComp + 10);
      if ( v12 - v11 != *((_QWORD *)sceneComp + 11) - (_QWORD)v13 )
        goto LABEL_8;
      if ( v11 != v12 )
      {
        while ( SC_LanguageSpecificItem::operator==(v11, v13) )
        {
          v11 = (SC_LanguageSpecificItem *)((char *)v11 + 80);
          v13 = (const SC_LanguageSpecificItem *)((char *)v13 + 80);
          if ( v12 == v11 )
            goto LABEL_21;
        }
        v2 = v26;
        v3 = v27;
        goto LABEL_3;
      }
LABEL_21:
      v14 = *((SC_LanguageSelectionSet **)this + 13);
      v15 = *((SC_LanguageSelectionSet **)this + 14);
      v16 = *((const SC_LanguageSelectionSet **)sceneComp + 13);
      if ( v15 - v14 != *((_QWORD *)sceneComp + 14) - (_QWORD)v16 )
        goto LABEL_8;
      if ( v14 != v15 )
      {
        while ( (unsigned __int8)SC_LanguageSelectionSet::operator==(v14, v16) )
        {
          v14 = (SC_LanguageSelectionSet *)((char *)v14 + 144);
          v16 = (const SC_LanguageSelectionSet *)((char *)v16 + 144);
          if ( v15 == v14 )
            goto LABEL_27;
        }
        v2 = v26;
        v3 = v27;
        goto LABEL_3;
      }
LABEL_27:
      v17 = *((SC_CompositionSelectionGroup **)this + 16);
      v18 = *((SC_CompositionSelectionGroup **)this + 17);
      v19 = *((const SC_CompositionSelectionGroup **)sceneComp + 16);
      if ( v18 - v17 != *((_QWORD *)sceneComp + 17) - (_QWORD)v19 )
        goto LABEL_8;
      if ( v17 != v18 )
      {
        while ( (unsigned __int8)SC_CompositionSelectionGroup::operator==(v17, v19) )
        {
          v17 = (SC_CompositionSelectionGroup *)((char *)v17 + 256);
          v19 = (const SC_CompositionSelectionGroup *)((char *)v19 + 256);
          if ( v18 == v17 )
            goto LABEL_33;
        }
        v2 = v26;
        v3 = v27;
        goto LABEL_3;
      }
LABEL_33:
      if ( *((unsigned __int8 *)this + 152) != *((unsigned __int8 *)sceneComp + 152) )
        goto LABEL_8;
      v20 = *((SC_PresetDescription **)this + 20);
      v21 = *((SC_PresetDescription **)this + 21);
      v22 = *((const SC_PresetDescription **)sceneComp + 20);
      if ( v21 - v20 != *((_QWORD *)sceneComp + 21) - (_QWORD)v22 )
        goto LABEL_8;
      if ( v20 != v21 )
      {
        while ( (unsigned __int8)SC_PresetDescription::operator==(v20, v22) )
        {
          v20 = (SC_PresetDescription *)((char *)v20 + 248);
          v22 = (const SC_PresetDescription *)((char *)v22 + 248);
          if ( v21 == v20 )
            goto LABEL_40;
        }
        v2 = v26;
        v3 = v27;
        goto LABEL_3;
      }
LABEL_40:
      if ( *((unsigned __int8 *)sceneComp + 184) != *((unsigned __int8 *)this + 184) )
        goto LABEL_8;
      if ( *((_BYTE *)sceneComp + 184)
        && !SC_Tag::operator==(
              (SC_SceneComposition *)((char *)this + 192),
              (const SC_SceneComposition *)((char *)sceneComp + 192)) )
      {
        v2 = v26;
        v3 = v27;
        goto LABEL_3;
      }
      if ( *((unsigned __int8 *)sceneComp + 304) == *((unsigned __int8 *)this + 304) )
      {
        if ( !*((_BYTE *)sceneComp + 304) )
          return SC_Extension::operator==(
                   (SC_SceneComposition *)((char *)this + 336),
                   (const SC_SceneComposition *)((char *)sceneComp + 336));
        v23 = *((_QWORD *)this + 39);
        if ( v23 == *((_QWORD *)this + 40) )
          return SC_Extension::operator==(
                   (SC_SceneComposition *)((char *)this + 336),
                   (const SC_SceneComposition *)((char *)sceneComp + 336));
        v24 = 0;
        v25 = 0;
        while ( (unsigned __int8)SC_SceneComposition::SC_Category::operator==(
                                   (SC_SceneComposition__SC_Category *)(v23 + 176 * v24),
                                   (const SC_SceneComposition__SC_Category *)(*((_QWORD *)sceneComp + 39) + 176 * v24)) )
        {
          v23 = *((_QWORD *)this + 39);
          v24 = (unsigned int)(v25 + 1);
          v25 = v24;
          if ( 0x2E8BA2E8BA2E8BA3LL * ((*((_QWORD *)this + 40) - v23) >> 4) <= (unsigned __int64)(unsigned int)v24 )
            return SC_Extension::operator==(
                     (SC_SceneComposition *)((char *)this + 336),
                     (const SC_SceneComposition *)((char *)sceneComp + 336));
        }
        v2 = v26;
        v3 = v27;
      }
      else
      {
LABEL_8:
        v2 = v26;
        v3 = v27;
      }
    }
  }
LABEL_3:
  if ( v28 != _stack_chk_guard )
  {
    v26 = v2;
    v27 = v3;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000a53f0  size 456 bytes  110 insns
 *   SC_SceneComposition::CheckConformance() const
 *   [_ZNK19SC_SceneComposition16CheckConformanceEv]
 *   data refs: 0x3307e8, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneComposition::CheckConformance(SC_SceneComposition *this)
{
  __int64 v2; // x2
  SC_CompositionSelectionGroup *v3; // x25
  unsigned __int16 v4; // w22
  SC_CompositionSelectionGroup *v5; // x0
  int v6; // w21
  int v7; // w19
  bool v8; // zf
  unsigned __int16 v9; // w23
  __int64 v10; // x26
  __int64 v11; // x28
  SC_SceneComposition__SC_Category *v12; // x24
  SC_SceneComposition__SC_Category *v13; // x25
  __int64 result; // x0
  SC_CompositionSelectionGroup *v15; // x19
  SC_LanguageSelectionSet *v16; // x19
  SC_LanguageSelectionSet *v17; // x21
  __int64 v18; // [xsp+70h] [xbp+70h]
  v2 = *((_QWORD *)this + 20);
  v3 = *((SC_CompositionSelectionGroup **)this + 17);
  v4 = -29127 * ((__int64)(*((_QWORD *)this + 14) - *((_QWORD *)this + 13)) >> 4);
  v5 = *((SC_CompositionSelectionGroup **)this + 16);
  v6 = (unsigned __int16)(-29127 * ((__int64)(*((_QWORD *)this + 8) - *((_QWORD *)this + 7)) >> 4));
  v7 = (unsigned __int16)(31711 * ((*((_QWORD *)this + 21) - v2) >> 3));
  if ( v6 | v4 )
    v8 = v7 == 0;
  else
    v8 = 1;
  v9 = (unsigned int)((_DWORD)v3 - (_DWORD)v5) >> 8;
  v11 = *((_QWORD *)this + 10);
  v10 = *((_QWORD *)this + 11);
  if ( v8 )
  {
    __printf_chk(1, "ERROR: SceneComposition payload has missing elements\n");
    return 0xFFFFFFFFLL;
  }
  else
  {
    v12 = *((SC_SceneComposition__SC_Category **)this + 40);
    if ( *((SC_SceneComposition__SC_Category **)this + 39) == v12 )
    {
LABEL_11:
      v15 = v5;
      if ( v5 == v3 )
      {
LABEL_12:
        v16 = *((SC_LanguageSelectionSet **)this + 13);
        v17 = *((SC_LanguageSelectionSet **)this + 14);
        if ( v17 == v16 )
        {
          return 0;
        }
        else
        {
          while ( 1 )
          {
            result = SC_LanguageSelectionSet::CheckConformance(v16, -13107 * (unsigned __int16)((v10 - v11) >> 4));
            if ( (_DWORD)result )
              break;
            v16 = (SC_LanguageSelectionSet *)((char *)v16 + 144);
            if ( v17 == v16 )
              return 0;
          }
        }
      }
      else
      {
        while ( 1 )
        {
          result = SC_CompositionSelectionGroup::CheckConformance(v15, v6, v4);
          if ( (_DWORD)result )
            break;
          v15 = (SC_CompositionSelectionGroup *)((char *)v15 + 256);
          if ( v3 == v15 )
            goto LABEL_12;
        }
      }
    }
    else
    {
      v13 = *((SC_SceneComposition__SC_Category **)this + 39);
      while ( 1 )
      {
        result = SC_SceneComposition::SC_Category::CheckConformance(v13, v7, v9);
        if ( (_DWORD)result )
          break;
        v13 = (SC_SceneComposition__SC_Category *)((char *)v13 + 176);
        if ( v12 == v13 )
        {
          v5 = *((SC_CompositionSelectionGroup **)this + 16);
          v3 = *((SC_CompositionSelectionGroup **)this + 17);
          goto LABEL_11;
        }
      }
    }
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a6030  size 916 bytes  229 insns
 *   SC_SceneComposition::SC_Category::Deserialize(TBitstreamReader<unsigned int>&, unsigned short)
 *   [_ZN19SC_SceneComposition11SC_Category11DeserializeER16TBitstreamReaderIjEt]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneComposition::SC_Category::Deserialize(
        SC_SceneComposition__SC_Category *this,
        TBitstreamReader_u32 *bs,
        unsigned __int16 v)
{
  int v3; // w23
  unsigned int cache; // w0
  int v7; // w3
  unsigned int v8; // w22
  unsigned int v9; // w25
  __int64 v10; // x2
  unsigned __int64 v11; // x0
  __int64 v12; // x20
  __int64 v13; // x25
  unsigned int v14; // w1
  int v15; // w3
  unsigned int v16; // w22
  unsigned int v17; // w0
  int v18; // w1
  __int16 v19; // w22
  unsigned int v20; // w0
  unsigned int v21; // w0
  int v22; // w3
  unsigned int v23; // w20
  __int64 v24; // x2
  __int64 v25; // x3
  unsigned int v26; // w1
  int v27; // w0
  unsigned int v28; // w20
  __int64 v30; // x22
  unsigned __int64 v31; // x1
  __int64 v32; // x20
  __int64 v33; // x23
  unsigned int v34; // w1
  int v35; // w0
  unsigned int v36; // w22
  unsigned int v37; // w0
  int v38; // w1
  unsigned int v39; // w0
  unsigned int v40; // w0
  int v41; // w1
  unsigned int v42; // w0
  unsigned int v43; // w0
  int v44; // w1
  unsigned int v45; // w0
  unsigned int v46; // w1
  const unsigned __int8 *cur; // x0
  int bitsLeft; // w2
  __int64 v49; // x2
  __int64 v50; // [xsp+60h] [xbp+60h]
  v3 = v;
  cache = bs->cache;
  v7 = bs->bitsLeft - 6;
  bs->bitsLeft = v7;
  v8 = cache >> 26;
  if ( v7 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v46 = bs->cache;
    cur = bs->cur;
    bitsLeft = bs->bitsLeft;
    v7 = bitsLeft + 32;
    bs->bitsLeft = bitsLeft + 32;
    bs->cur = cur + 4;
    LOWORD(v8) = v8 | (v46 >> (bitsLeft + 32));
    v9 = (unsigned __int16)v8;
    if ( bitsLeft == -32 )
      bs->cache = 0;
    else
      bs->cache = v46 << -(char)bitsLeft;
  }
  else
  {
    v9 = cache >> 26;
    bs->cache = cache << 6;
  }
  v10 = *((_QWORD *)this + 1);
  v11 = (v10 - *(_QWORD *)this) >> 1;
  if ( (unsigned __int16)v8 > v11 )
  {
    std::vector<unsigned short>::_M_default_append(this, (unsigned __int16)v8 - v11);
    v7 = bs->bitsLeft;
  }
  else if ( (unsigned __int16)v8 < v11 )
  {
    v30 = *(_QWORD *)this + 2LL * (unsigned __int16)v8;
    if ( v10 != v30 )
    {
      *((_QWORD *)this + 1) = v30;
      v7 = bs->bitsLeft;
    }
  }
  if ( v9 )
  {
    v12 = 0;
    v13 = 2 * (v9 - 1 + 1LL);
    do
    {
      while ( 1 )
      {
        v14 = bs->cache;
        v15 = v7 - 6;
        bs->bitsLeft = v15;
        v16 = v14 >> 26;
        if ( v15 < 0 )
          break;
        bs->cache = v14 << 6;
        *(_WORD *)(*(_QWORD *)this + v12) = v16;
        v12 += 2;
        v7 = bs->bitsLeft;
        if ( v13 == v12 )
          goto LABEL_12;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v17 = bs->cache;
      v18 = bs->bitsLeft;
      v19 = v16 | (v17 >> (v18 + 32));
      v20 = v17 << -(char)v18;
      if ( v18 == -32 )
        v20 = 0;
      bs->cur += 4;
      bs->cache = v20;
      bs->bitsLeft = v18 + 32;
      *(_WORD *)(*(_QWORD *)this + v12) = v19;
      v12 += 2;
      v7 = bs->bitsLeft;
    }
    while ( v13 != v12 );
  }
LABEL_12:
  v21 = bs->cache;
  v22 = v7 - 1;
  bs->bitsLeft = v22;
  v23 = v21 >> 31;
  if ( v22 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v40 = bs->cache;
    v41 = bs->bitsLeft;
    LOBYTE(v23) = v23 | (v40 >> (v41 + 32));
    v42 = v40 << -(char)v41;
    if ( v41 == -32 )
      v42 = 0;
    bs->cur += 4;
    bs->cache = v42;
    bs->bitsLeft = v41 + 32;
  }
  else
  {
    bs->cache = 2 * v21;
  }
  *((_BYTE *)this + 24) = (unsigned __int8)v23 != 0;
  v24 = *((_QWORD *)this + 4);
  v25 = *((_QWORD *)this + 5);
  if ( (_BYTE)v23 )
  {
    v31 = (v25 - v24) >> 1;
    if ( (unsigned __int16)v3 > v31 )
    {
      std::vector<unsigned short>::_M_default_append((char *)this + 32, (unsigned __int16)v3 - v31);
    }
    else if ( (unsigned __int16)v3 < v31 )
    {
      v49 = v24 + 2LL * (unsigned __int16)v3;
      if ( v49 != v25 )
        *((_QWORD *)this + 5) = v49;
    }
    if ( v3 )
    {
      v32 = 0;
      v33 = 2 * ((unsigned int)(v3 - 1) + 1LL);
      do
      {
        v34 = bs->cache;
        v35 = bs->bitsLeft - 6;
        bs->bitsLeft = v35;
        v36 = v34 >> 26;
        if ( v35 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v37 = bs->cache;
          v38 = bs->bitsLeft;
          LOWORD(v36) = v36 | (v37 >> (v38 + 32));
          v39 = v37 << -(char)v38;
          if ( v38 == -32 )
            v39 = 0;
          bs->cur += 4;
          bs->cache = v39;
          bs->bitsLeft = v38 + 32;
        }
        else
        {
          bs->cache = v34 << 6;
        }
        *(_WORD *)(*((_QWORD *)this + 4) + v32) = v36;
        v32 += 2;
      }
      while ( v33 != v32 );
    }
  }
  else if ( v24 != v25 )
  {
    *((_QWORD *)this + 5) = v24;
  }
  v26 = bs->cache;
  v27 = bs->bitsLeft - 1;
  bs->bitsLeft = v27;
  v28 = v26 >> 31;
  if ( v27 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v43 = bs->cache;
    v44 = bs->bitsLeft;
    LOBYTE(v28) = v28 | (v43 >> (v44 + 32));
    v45 = v43 << -(char)v44;
    if ( v44 == -32 )
      v45 = 0;
    bs->cur += 4;
    bs->cache = v45;
    bs->bitsLeft = v44 + 32;
  }
  else
  {
    bs->cache = 2 * v26;
  }
  *((_BYTE *)this + 56) = (unsigned __int8)v28 != 0;
  if ( (_BYTE)v28 )
    return SC_Tag::Deserialize((SC_SceneComposition__SC_Category *)((char *)this + 64), bs);
  else
    return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000a81b0  size 3236 bytes  795 insns
 *   SC_SceneComposition::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN19SC_SceneComposition11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneComposition::Deserialize(SC_SceneComposition *this, TBitstreamReader_u32 *bs, bool flag)
{
  __int64 v3; // x23
  __int64 v4; // x24
  __int64 v5; // x25
  __int64 v6; // x26
  _BOOL4 v8; // w19
  unsigned int cache; // w1
  int v11; // w0
  unsigned int v12; // w21
  unsigned int v13; // w1
  int v14; // w0
  unsigned int v15; // w21
  unsigned int v16; // w0
  int v17; // w2
  unsigned int v18; // w22
  unsigned int v19; // w0
  int v20; // w2
  unsigned int v21; // w21
  unsigned int v22; // w0
  int v23; // w2
  unsigned int v24; // w23
  unsigned int v25; // w0
  int v26; // w2
  unsigned int v27; // w24
  unsigned int v28; // w0
  int v29; // w2
  __int64 v30; // x0
  __int64 v31; // x25
  unsigned __int64 v32; // x1
  __int64 v33; // x22
  __int64 v34; // x26
  void *v35; // x0
  void *v36; // x0
  void *v37; // x0
  void *v38; // x0
  int v39; // w26
  __int64 v40; // x1
  __int64 result; // x0
  __int64 v42; // x0
  __int64 v43; // x27
  unsigned __int64 v44; // x1
  __int64 v45; // x21
  __int64 v46; // x22
  void *v47; // x0
  int v48; // w23
  __int64 v49; // x3
  __int64 v50; // x25
  __int64 v51; // x1
  __int64 v52; // x0
  __int64 v53; // x22
  unsigned __int64 v54; // x1
  __int64 v55; // x23
  __int64 v56; // x21
  void *v57; // x0
  void *v58; // x0
  void *v59; // x0
  void *v60; // x0
  unsigned int v61; // w0
  int v62; // w3
  const unsigned __int8 *v63; // x1
  const unsigned __int8 *cur; // x1
  unsigned int v65; // w0
  int v66; // w3
  const unsigned __int8 *v67; // x1
  const unsigned __int8 *v68; // x1
  unsigned int v69; // w0
  int v70; // w3
  const unsigned __int8 *v71; // x1
  const unsigned __int8 *v72; // x1
  unsigned __int16 v73; // w23
  unsigned int v74; // w0
  int v75; // w3
  const unsigned __int8 *v76; // x1
  int v77; // w2
  const unsigned __int8 *v78; // x1
  unsigned __int16 v79; // w24
  unsigned int v80; // w0
  int v81; // w1
  unsigned int v82; // w6
  unsigned int v83; // w0
  int v84; // w4
  unsigned int v85; // w0
  int v86; // w1
  unsigned int v87; // w0
  unsigned int v88; // w0
  int bitsLeft; // w1
  unsigned int v90; // w0
  int v91; // w23
  __int64 v92; // x2
  char *v93; // x0
  char *v94; // x22
  unsigned int v95; // w24
  unsigned __int64 v96; // x1
  char *v97; // x23
  char *v98; // x21
  void *v99; // x0
  void *v100; // x0
  void *v101; // x0
  void *v102; // x0
  void *v103; // x0
  void *v104; // x0
  void *v105; // x0
  void *v106; // x0
  void *v107; // x0
  int v108; // w21
  __int64 v109; // x2
  __int64 v110; // x0
  __int64 v111; // x22
  unsigned __int64 v112; // x1
  __int64 v113; // x23
  __int64 v114; // x21
  void *v115; // x0
  void *v116; // x0
  void *v117; // x0
  void *v118; // x0
  void *v119; // x0
  void *v120; // x0
  int v121; // w23
  __int64 v122; // x2
  unsigned int v123; // w1
  int v124; // w0
  unsigned int v125; // w21
  unsigned int v126; // w1
  int v127; // w0
  unsigned int v128; // w19
  unsigned int v129; // w1
  int v130; // w0
  unsigned int v131; // w19
  unsigned int v132; // w0
  int v133; // w1
  unsigned int v134; // w19
  unsigned int v135; // w22
  __int64 v136; // x21
  __int64 v137; // x23
  unsigned __int64 v138; // x0
  __int64 v139; // x21
  __int64 v140; // x19
  void *v141; // x0
  void *v142; // x0
  void *v143; // x0
  void *v144; // x0
  void *v145; // x0
  __int64 v146; // x19
  SC_SceneComposition__SC_Category *v147; // x0
  unsigned int v148; // w0
  int v149; // w1
  unsigned int v150; // w0
  unsigned int v151; // w0
  int v152; // w1
  unsigned int v153; // w0
  unsigned int v154; // w0
  int v155; // w1
  unsigned int v156; // w0
  unsigned int v157; // w0
  int v158; // w1
  const unsigned __int8 *v159; // x3
  __int64 v160; // [xsp+30h] [xbp+30h]
  __int64 v161; // [xsp+38h] [xbp+38h]
  __int64 v162; // [xsp+40h] [xbp+40h]
  __int64 v163; // [xsp+48h] [xbp+48h]
  __int64 v164; // [xsp+60h] [xbp+60h]
  char *v165; // [xsp+68h] [xbp+68h]
  unsigned int v166; // [xsp+70h] [xbp+70h]
  unsigned int v167; // [xsp+74h] [xbp+74h]
  __int64 v168; // [xsp+78h] [xbp+78h]
  __int64 v169; // [xsp+88h] [xbp+88h]
  __int64 v170; // [xsp+90h] [xbp+90h]
  v8 = flag;
  cache = bs->cache;
  v11 = bs->bitsLeft - 1;
  bs->bitsLeft = v11;
  v12 = cache >> 31;
  if ( v11 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v88 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v12) = v12 | (v88 >> (bitsLeft + 32));
    v90 = v88 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v90 = 0;
    bs->cur += 4;
    bs->cache = v90;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  *(_BYTE *)this = (unsigned __int8)v12 != 0;
  v13 = bs->cache;
  v14 = bs->bitsLeft - 1;
  bs->bitsLeft = v14;
  v15 = v13 >> 31;
  if ( v14 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v85 = bs->cache;
    v86 = bs->bitsLeft;
    LOBYTE(v15) = v15 | (v85 >> (v86 + 32));
    v87 = v85 << -(char)v86;
    if ( v86 == -32 )
      v87 = 0;
    bs->cur += 4;
    bs->cache = v87;
    bs->bitsLeft = v86 + 32;
  }
  else
  {
    bs->cache = 2 * v13;
  }
  *((_BYTE *)this + 1) = (unsigned __int8)v15 != 0;
  if ( (_BYTE)v15 )
  {
    result = apac::scene::SC_Language::Deserialize((SC_SceneComposition *)((char *)this + 8), bs);
    if ( (_DWORD)result )
      goto LABEL_33;
    v160 = v3;
    v161 = v4;
    v162 = v5;
    v163 = v6;
    v16 = bs->cache;
    v17 = bs->bitsLeft;
    bs->bitsLeft = v17 - 6;
    v18 = v16 >> 26;
    if ( ((v17 - 6) & 0x80000000) == 0 )
    {
LABEL_7:
      v19 = v16 << 6;
      v20 = v17 - 12;
      bs->cache = v19;
      v18 = (unsigned __int16)v18;
      bs->bitsLeft = v20;
      v21 = v19 >> 26;
      if ( (v20 & 0x80000000) == 0 )
        goto LABEL_8;
LABEL_90:
      cur = bs->cur;
      goto LABEL_73;
    }
  }
  else
  {
    v16 = bs->cache;
    v17 = bs->bitsLeft;
    bs->bitsLeft = v17 - 6;
    v18 = v16 >> 26;
    if ( ((v17 - 6) & 0x80000000) == 0 )
      goto LABEL_7;
  }
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v61 = bs->cache;
  v62 = bs->bitsLeft;
  v63 = bs->cur;
  bs->bitsLeft = v62 + 32;
  v20 = v62 + 26;
  cur = v63 + 4;
  bs->cur = cur;
  LOWORD(v18) = v18 | (v61 >> (v62 + 32));
  if ( v62 != -32 )
  {
    bs->bitsLeft = v20;
    v18 = (unsigned __int16)v18;
    v19 = v61 << -(char)v62;
    bs->cache = v19;
    v21 = v19 >> 26;
    if ( (v20 & 0x80000000) == 0 )
    {
LABEL_8:
      v22 = v19 << 6;
      v23 = v20 - 6;
      bs->cache = v22;
      v21 = (unsigned __int16)v21;
      bs->bitsLeft = v23;
      v24 = v22 >> 26;
      if ( (v23 & 0x80000000) == 0 )
        goto LABEL_9;
      goto LABEL_132;
    }
    goto LABEL_90;
  }
  v18 = (unsigned __int16)v18;
  LOWORD(v21) = 0;
  bs->cache = 0;
  bs->bitsLeft = v20;
LABEL_73:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, cur);
  v65 = bs->cache;
  v66 = bs->bitsLeft;
  v67 = bs->cur;
  bs->bitsLeft = v66 + 32;
  v23 = v66 + 26;
  v68 = v67 + 4;
  bs->cur = v68;
  LOWORD(v21) = v21 | (v65 >> (v66 + 32));
  if ( v66 == -32 )
  {
    v21 = (unsigned __int16)v21;
    LOWORD(v24) = 0;
    bs->cache = 0;
    bs->bitsLeft = v23;
    goto LABEL_75;
  }
  bs->bitsLeft = v23;
  v21 = (unsigned __int16)v21;
  v22 = v65 << -(char)v66;
  bs->cache = v22;
  v24 = v22 >> 26;
  if ( (v23 & 0x80000000) == 0 )
  {
LABEL_9:
    v25 = v22 << 6;
    v26 = v23 - 5;
    bs->cache = v25;
    bs->bitsLeft = v26;
    v27 = v25 >> 27;
    v167 = (unsigned __int16)v24;
    if ( (v26 & 0x80000000) == 0 )
      goto LABEL_10;
    goto LABEL_130;
  }
LABEL_132:
  v68 = bs->cur;
LABEL_75:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v68);
  v69 = bs->cache;
  v70 = bs->bitsLeft;
  v71 = bs->cur;
  bs->bitsLeft = v70 + 32;
  v26 = v70 + 27;
  v72 = v71 + 4;
  bs->cur = v72;
  v73 = v24 | (v69 >> (v70 + 32));
  if ( v70 == -32 )
  {
    LOWORD(v27) = 0;
    bs->cache = 0;
    bs->bitsLeft = v26;
    v167 = v73;
LABEL_77:
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, v72);
    v74 = bs->cache;
    v75 = bs->bitsLeft;
    v76 = bs->cur;
    bs->bitsLeft = v75 + 32;
    v77 = v75 + 28;
    v78 = v76 + 4;
    bs->cur = v78;
    v79 = v27 | (v74 >> (v75 + 32));
    if ( v75 == -32 )
    {
      bs->cache = 0;
      bs->bitsLeft = v77;
      v166 = 0;
      LODWORD(v168) = v79;
LABEL_79:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v78);
      v80 = bs->cache;
      v81 = bs->bitsLeft;
      v82 = v80 >> (v81 + 32);
      v83 = v80 << -(char)v81;
      v84 = v166 | v82;
      if ( v81 == -32 )
        v83 = 0;
      bs->cur += 4;
      bs->cache = v83;
      bs->bitsLeft = v81 + 32;
      v166 = v84;
      goto LABEL_12;
    }
    bs->bitsLeft = v77;
    v28 = v74 << -(char)v75;
    bs->cache = v28;
    LODWORD(v168) = v79;
    v166 = v28 >> 28;
    if ( (v77 & 0x80000000) == 0 )
      goto LABEL_11;
LABEL_128:
    v78 = bs->cur;
    goto LABEL_79;
  }
  bs->bitsLeft = v26;
  v25 = v69 << -(char)v70;
  bs->cache = v25;
  v167 = v73;
  v27 = v25 >> 27;
  if ( v26 < 0 )
  {
LABEL_130:
    v72 = bs->cur;
    goto LABEL_77;
  }
LABEL_10:
  v28 = 32 * v25;
  v29 = v26 - 4;
  bs->cache = v28;
  bs->bitsLeft = v29;
  LODWORD(v168) = (unsigned __int16)v27;
  v166 = v28 >> 28;
  if ( v29 < 0 )
    goto LABEL_128;
LABEL_11:
  bs->cache = 16 * v28;
LABEL_12:
  v30 = *((_QWORD *)this + 7);
  v31 = *((_QWORD *)this + 8);
  v32 = 0x8E38E38E38E38E39LL * ((v31 - v30) >> 4);
  if ( (unsigned __int16)v18 > v32 )
  {
    std::vector<SC_NonLanguageItem>::_M_default_append((std_vector *)((char *)this + 56), (unsigned __int16)v18 - v32);
    v30 = *((_QWORD *)this + 7);
    v31 = *((_QWORD *)this + 8);
  }
  else if ( (unsigned __int16)v18 < v32 )
  {
    v33 = v30 + 144LL * v18;
    if ( v31 == v33 )
      goto LABEL_29;
    v34 = v33;
    do
    {
      v35 = *(void **)(v34 + 120);
      if ( v35 )
        operator delete(v35);
      v36 = *(void **)(v34 + 96);
      if ( v36 )
        operator delete(v36);
      v37 = *(void **)(v34 + 64);
      if ( v37 )
        operator delete(v37);
      v38 = *(void **)(v34 + 32);
      if ( v38 )
        operator delete(v38);
      if ( *(_QWORD *)v34 )
        operator delete(*(void **)v34);
      v34 += 144;
    }
    while ( v31 != v34 );
    v30 = *((_QWORD *)this + 7);
    v31 = v33;
    *((_QWORD *)this + 8) = v33;
  }
  if ( v31 != v30 )
  {
LABEL_29:
    v39 = 0;
    v40 = 0;
    do
    {
      result = SC_NonLanguageItem::Deserialize((SC_NonLanguageItem *)(v30 + 144 * v40), bs, v8);
      if ( (_DWORD)result )
        goto LABEL_32;
      v30 = *((_QWORD *)this + 7);
      v40 = (unsigned int)(v39 + 1);
      v39 = v40;
    }
    while ( 0x8E38E38E38E38E39LL * ((*((_QWORD *)this + 8) - v30) >> 4) > (unsigned int)v40 );
  }
  v165 = (char *)this + 392;
  apac::scene::SC_Language::Reset((SC_SceneComposition *)((char *)this + 392));
  apac::scene::SC_Language::Reset((SC_SceneComposition *)((char *)this + 440));
  v42 = *((_QWORD *)this + 10);
  v43 = *((_QWORD *)this + 11);
  v44 = 0xCCCCCCCCCCCCCCCDLL * ((v43 - v42) >> 4);
  if ( (unsigned __int16)v21 > v44 )
  {
    std::vector<SC_LanguageSpecificItem>::_M_default_append(
      (std_vector *)((char *)this + 80),
      (unsigned __int16)v21 - v44);
    v42 = *((_QWORD *)this + 10);
    v43 = *((_QWORD *)this + 11);
  }
  else if ( (unsigned __int16)v21 < v44 )
  {
    v45 = v42 + 80LL * v21;
    if ( v43 == v45 )
    {
LABEL_46:
      v48 = 0;
      v49 = 0;
      while ( 1 )
      {
        v50 = 80 * v49;
        result = SC_LanguageSpecificItem::Deserialize((SC_LanguageSpecificItem *)(v42 + 80 * v49), bs, v8);
        if ( (_DWORD)result )
          goto LABEL_32;
        v42 = *((_QWORD *)this + 10);
        v51 = v42 + v50;
        if ( *(_BYTE *)(v42 + v50 + 72) )
        {
          v164 = v51 + 24;
          std::string::_M_assign((char *)this + 400, v51 + 32);
          v165[40] = *(_BYTE *)(v164 + 40);
          v42 = *((_QWORD *)this + 10);
          v51 = v42 + v50;
          if ( *(_BYTE *)(v42 + v50 + 73) )
            goto LABEL_52;
LABEL_48:
          v49 = (unsigned int)(v48 + 1);
          v48 = v49;
          if ( 0xCCCCCCCCCCCCCCCDLL * ((*((_QWORD *)this + 11) - v42) >> 4) <= (unsigned int)v49 )
            goto LABEL_53;
        }
        else
        {
          if ( !*(_BYTE *)(v51 + 73) )
            goto LABEL_48;
LABEL_52:
          v164 = v51 + 24;
          std::string::_M_assign((char *)this + 448, v51 + 32);
          v49 = (unsigned int)(v48 + 1);
          v48 = v49;
          *((_BYTE *)this + 480) = *(_BYTE *)(v164 + 40);
          v42 = *((_QWORD *)this + 10);
          if ( 0xCCCCCCCCCCCCCCCDLL * ((*((_QWORD *)this + 11) - v42) >> 4) <= (unsigned int)v49 )
            goto LABEL_53;
        }
      }
    }
    v46 = v45;
    do
    {
      v47 = *(void **)(v46 + 32);
      if ( v47 != (void *)(v46 + 48) )
        operator delete(v47);
      if ( *(_QWORD *)v46 )
        operator delete(*(void **)v46);
      v46 += 80;
    }
    while ( v43 != v46 );
    v42 = *((_QWORD *)this + 10);
    v43 = v45;
    *((_QWORD *)this + 11) = v45;
  }
  if ( v42 != v43 )
    goto LABEL_46;
LABEL_53:
  v52 = *((_QWORD *)this + 13);
  v53 = *((_QWORD *)this + 14);
  v54 = 0x8E38E38E38E38E39LL * ((v53 - v52) >> 4);
  if ( (unsigned __int16)v167 > v54 )
  {
    std::vector<SC_LanguageSelectionSet>::_M_default_append(
      (std_vector *)((char *)this + 104),
      (unsigned __int16)v167 - v54);
    v52 = *((_QWORD *)this + 13);
    v53 = *((_QWORD *)this + 14);
  }
  else if ( (unsigned __int16)v167 < v54 )
  {
    v55 = v52 + 144LL * v167;
    if ( v53 == v55 )
      goto LABEL_93;
    v56 = v52 + 144LL * v167;
    do
    {
      v57 = *(void **)(v56 + 120);
      if ( v57 )
        operator delete(v57);
      v58 = *(void **)(v56 + 96);
      if ( v58 )
        operator delete(v58);
      v59 = *(void **)(v56 + 64);
      if ( v59 )
        operator delete(v59);
      v60 = *(void **)(v56 + 32);
      if ( v60 )
        operator delete(v60);
      if ( *(_QWORD *)v56 )
        operator delete(*(void **)v56);
      v56 += 144;
    }
    while ( v53 != v56 );
    v53 = v55;
    *((_QWORD *)this + 14) = v55;
    v52 = *((_QWORD *)this + 13);
  }
  if ( v53 != v52 )
  {
LABEL_93:
    v91 = 0;
    v92 = 0;
    do
    {
      result = SC_LanguageSelectionSet::Deserialize((SC_LanguageSelectionSet *)(v52 + 144 * v92), bs);
      if ( (_DWORD)result )
        goto LABEL_32;
      v52 = *((_QWORD *)this + 13);
      v92 = (unsigned int)(v91 + 1);
      v91 = v92;
    }
    while ( 0x8E38E38E38E38E39LL * ((*((_QWORD *)this + 14) - v52) >> 4) > (unsigned int)v92 );
  }
  v93 = *((char **)this + 16);
  v94 = *((char **)this + 17);
  v95 = (unsigned __int16)v166;
  v96 = (v94 - v93) >> 8;
  if ( (unsigned __int16)v168 > v96 )
  {
    std::vector<SC_CompositionSelectionGroup>::_M_default_append(
      (std_vector *)((char *)this + 128),
      (unsigned __int16)v168 - v96);
    v93 = *((char **)this + 16);
    v94 = *((char **)this + 17);
  }
  else if ( (unsigned __int16)v168 < v96 )
  {
    v97 = &v93[256 * (unsigned __int64)(unsigned __int16)v168];
    if ( v94 == v97 )
      goto LABEL_123;
    v98 = &v93[256 * (unsigned __int64)(unsigned __int16)v168];
    do
    {
      v99 = *((void **)v98 + 29);
      if ( v99 )
        operator delete(v99);
      v100 = *((void **)v98 + 26);
      if ( v100 )
        operator delete(v100);
      v101 = *((void **)v98 + 22);
      if ( v101 )
        operator delete(v101);
      v102 = *((void **)v98 + 19);
      if ( v102 )
        operator delete(v102);
      v103 = *((void **)v98 + 15);
      if ( v103 )
        operator delete(v103);
      v104 = *((void **)v98 + 11);
      if ( v104 )
        operator delete(v104);
      v105 = *((void **)v98 + 7);
      if ( v105 )
        operator delete(v105);
      v106 = *((void **)v98 + 4);
      if ( v106 )
        operator delete(v106);
      v107 = *((void **)v98 + 1);
      if ( v107 )
        operator delete(v107);
      v98 += 256;
    }
    while ( v94 != v98 );
    v93 = *((char **)this + 16);
    v94 = v97;
    *((_QWORD *)this + 17) = v97;
  }
  if ( v93 != v94 )
  {
LABEL_123:
    v108 = 0;
    v109 = 0;
    do
    {
      result = SC_CompositionSelectionGroup::Deserialize((SC_CompositionSelectionGroup *)&v93[256 * v109], bs, v95);
      if ( (_DWORD)result )
        goto LABEL_32;
      v93 = *((char **)this + 16);
      v109 = (unsigned int)(v108 + 1);
      v108 = v109;
    }
    while ( (__int64)(*((_QWORD *)this + 17) - (_QWORD)v93) >> 8 > (unsigned __int64)(unsigned int)v109 );
  }
  if ( !v8 )
  {
    v123 = bs->cache;
    v124 = bs->bitsLeft - 1;
    bs->bitsLeft = v124;
    v125 = v123 >> 31;
    if ( v124 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v154 = bs->cache;
      v155 = bs->bitsLeft;
      LOBYTE(v125) = v125 | (v154 >> (v155 + 32));
      v156 = v154 << -(char)v155;
      if ( v155 == -32 )
        v156 = 0;
      bs->cur += 4;
      bs->cache = v156;
      bs->bitsLeft = v155 + 32;
    }
    else
    {
      bs->cache = 2 * v123;
    }
    *((_BYTE *)this + 152) = (unsigned __int8)v125 != 0;
  }
  v110 = *((_QWORD *)this + 20);
  v111 = *((_QWORD *)this + 21);
  v112 = 0xEF7BDEF7BDEF7BDFLL * ((v111 - v110) >> 3);
  if ( (unsigned __int16)v95 > v112 )
  {
    std::vector<SC_PresetDescription>::_M_default_append(
      (std_vector *)((char *)this + 160),
      (unsigned __int16)v95 - v112);
    v110 = *((_QWORD *)this + 20);
    v111 = *((_QWORD *)this + 21);
  }
  else if ( (unsigned __int16)v95 < v112 )
  {
    v113 = v110 + 248LL * v95;
    if ( v111 == v113 )
      goto LABEL_156;
    v114 = v110 + 248LL * v95;
    do
    {
      v115 = *(void **)(v114 + 216);
      if ( v115 )
        operator delete(v115);
      v116 = *(void **)(v114 + 168);
      if ( v116 != (void *)(v114 + 184) )
        operator delete(v116);
      v117 = *(void **)(v114 + 120);
      if ( v117 )
        operator delete(v117);
      v118 = *(void **)(v114 + 88);
      if ( v118 )
        operator delete(v118);
      v119 = *(void **)(v114 + 64);
      if ( v119 )
        operator delete(v119);
      v120 = *(void **)(v114 + 32);
      if ( v120 )
        operator delete(v120);
      if ( *(_QWORD *)v114 )
        operator delete(*(void **)v114);
      v114 += 248;
    }
    while ( v111 != v114 );
    v111 = v113;
    *((_QWORD *)this + 21) = v113;
    v110 = *((_QWORD *)this + 20);
  }
  if ( v110 != v111 )
  {
LABEL_156:
    v121 = 0;
    v122 = 0;
    do
    {
      result = SC_PresetDescription::Deserialize(
                 (SC_PresetDescription *)(v110 + 248 * v122),
                 bs,
                 v8,
                 *((_BYTE *)this + 152));
      if ( (_DWORD)result )
        goto LABEL_32;
      v110 = *((_QWORD *)this + 20);
      v122 = (unsigned int)(v121 + 1);
      v121 = v122;
    }
    while ( 0xEF7BDEF7BDEF7BDFLL * ((*((_QWORD *)this + 21) - v110) >> 3) > (unsigned int)v122 );
  }
  v126 = bs->cache;
  v127 = bs->bitsLeft - 1;
  bs->bitsLeft = v127;
  v128 = v126 >> 31;
  if ( v127 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v148 = bs->cache;
    v149 = bs->bitsLeft;
    LOBYTE(v128) = v128 | (v148 >> (v149 + 32));
    v150 = v148 << -(char)v149;
    if ( v149 == -32 )
      v150 = 0;
    bs->cur += 4;
    bs->cache = v150;
    bs->bitsLeft = v149 + 32;
  }
  else
  {
    bs->cache = 2 * v126;
  }
  *((_BYTE *)this + 184) = (unsigned __int8)v128 != 0;
  if ( (_BYTE)v128 )
  {
    result = SC_Tag::Deserialize((SC_SceneComposition *)((char *)this + 192), bs);
    if ( (_DWORD)result )
      goto LABEL_32;
  }
  v129 = bs->cache;
  v130 = bs->bitsLeft - 1;
  bs->bitsLeft = v130;
  v131 = v129 >> 31;
  if ( v130 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v151 = bs->cache;
    v152 = bs->bitsLeft;
    LOBYTE(v131) = v131 | (v151 >> (v152 + 32));
    v153 = v151 << -(char)v152;
    if ( v152 == -32 )
      v153 = 0;
    bs->cur += 4;
    bs->cache = v153;
    bs->bitsLeft = v152 + 32;
  }
  else
  {
    bs->cache = 2 * v129;
  }
  *((_BYTE *)this + 304) = (unsigned __int8)v131 != 0;
  if ( (_BYTE)v131 )
  {
    v132 = bs->cache;
    v133 = bs->bitsLeft - 4;
    bs->bitsLeft = v133;
    v134 = v132 >> 28;
    if ( v133 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v157 = bs->cache;
      v158 = bs->bitsLeft;
      v159 = bs->cur;
      bs->bitsLeft = v158 + 32;
      bs->cur = v159 + 4;
      LOWORD(v134) = v134 | (v157 >> (v158 + 32));
      v135 = (unsigned __int16)v134;
      if ( v158 == -32 )
        bs->cache = 0;
      else
        bs->cache = v157 << -(char)v158;
    }
    else
    {
      v135 = v132 >> 28;
      bs->cache = 16 * v132;
    }
    v136 = *((_QWORD *)this + 39);
    v137 = *((_QWORD *)this + 40);
    v138 = 0x2E8BA2E8BA2E8BA3LL * ((v137 - v136) >> 4);
    if ( (unsigned __int16)v134 > v138 )
    {
      std::vector<SC_SceneComposition::SC_Category>::_M_default_append(
        (std_vector *)this + 13,
        (unsigned __int16)v134 - v138);
    }
    else if ( (unsigned __int16)v134 < v138 )
    {
      v139 = v136 + 176LL * (unsigned __int16)v134;
      if ( v137 != v139 )
      {
        v140 = v139;
        do
        {
          v141 = *(void **)(v140 + 152);
          if ( v141 )
            operator delete(v141);
          v142 = *(void **)(v140 + 128);
          if ( v142 )
            operator delete(v142);
          v143 = *(void **)(v140 + 96);
          if ( v143 )
            operator delete(v143);
          v144 = *(void **)(v140 + 64);
          if ( v144 )
            operator delete(v144);
          v145 = *(void **)(v140 + 32);
          if ( v145 )
            operator delete(v145);
          if ( *(_QWORD *)v140 )
            operator delete(*(void **)v140);
          v140 += 176;
        }
        while ( v137 != v140 );
        *((_QWORD *)this + 40) = v139;
      }
    }
    if ( v135 )
    {
      v146 = 0;
      do
      {
        v147 = (SC_SceneComposition__SC_Category *)(*((_QWORD *)this + 39) + v146);
        v146 += 176;
        SC_SceneComposition::SC_Category::Deserialize(v147, bs, v95);
      }
      while ( v146 != 176LL * (v135 - 1) + 176 );
    }
  }
  result = SC_Extension::Deserialize((SC_SceneComposition *)((char *)this + 336), bs);
  if ( (_DWORD)result )
  {
LABEL_32:
    v3 = v160;
    v4 = v161;
    v5 = v162;
    v6 = v163;
    goto LABEL_33;
  }
  *((_WORD *)this + 169) = 1;
  v3 = v160;
  v4 = v161;
  v5 = v162;
  v6 = v163;
LABEL_33:
  if ( v169 != _stack_chk_guard )
  {
    v160 = v3;
    v161 = v4;
    v162 = v5;
    v163 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000aaa60  size 928 bytes  232 insns
 *   SC_SceneComposition::~SC_SceneComposition()
 *   SC_SceneComposition::~SC_SceneComposition()
 *   [_ZN19SC_SceneCompositionD2Ev]
 *   [_ZN19SC_SceneCompositionD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN19SC_SceneCompositionD1Ev'
void __fastcall SC_SceneComposition::~SC_SceneComposition(SC_SceneComposition *this)
{
  char *v1; // x1
  char *v3; // x0
  char *v4; // x0
  void *v5; // x0
  void *v6; // x0
  __int64 v7; // x20
  void *v8; // x22
  void *v9; // x0
  void *v10; // x0
  void *v11; // x0
  void *v12; // x0
  void *v13; // x0
  void *v14; // x0
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  __int64 v18; // x20
  void *v19; // x22
  void *v20; // x0
  void *v21; // x0
  void *v22; // x0
  void *v23; // x0
  void *v24; // x0
  void *v25; // x0
  _QWORD *v26; // x20
  _QWORD *v27; // x22
  void *v28; // x0
  void *v29; // x0
  void *v30; // x0
  void *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  void *v35; // x0
  void *v36; // x0
  __int64 v37; // x20
  void *v38; // x22
  void *v39; // x0
  void *v40; // x0
  void *v41; // x0
  void *v42; // x0
  __int64 v43; // x20
  void *v44; // x22
  void *v45; // x0
  __int64 v46; // x20
  void *v47; // x22
  void *v48; // x0
  void *v49; // x0
  void *v50; // x0
  void *v51; // x0
  char *v52; // x0
  __int64 v53; // [xsp+40h] [xbp+40h]
  v1 = (char *)this + 464;
  v3 = *((char **)this + 56);
  if ( v3 != v1 )
    operator delete(v3);
  v4 = *((char **)this + 50);
  if ( v4 != (char *)this + 416 )
    operator delete(v4);
  v5 = *((void **)this + 46);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 43);
  if ( v6 )
    operator delete(v6);
  v7 = *((_QWORD *)this + 39);
  v8 = *((void **)this + 40);
  if ( (void *)v7 != v8 )
  {
    do
    {
      v9 = *(void **)(v7 + 152);
      if ( v9 )
        operator delete(v9);
      v10 = *(void **)(v7 + 128);
      if ( v10 )
        operator delete(v10);
      v11 = *(void **)(v7 + 96);
      if ( v11 )
        operator delete(v11);
      v12 = *(void **)(v7 + 64);
      if ( v12 )
        operator delete(v12);
      v13 = *(void **)(v7 + 32);
      if ( v13 )
        operator delete(v13);
      if ( *(_QWORD *)v7 )
        operator delete(*(void **)v7);
      v7 += 176;
    }
    while ( v8 != (void *)v7 );
    v8 = *((void **)this + 39);
  }
  if ( v8 )
    operator delete(v8);
  v14 = *((void **)this + 35);
  if ( v14 )
    operator delete(v14);
  v15 = *((void **)this + 32);
  if ( v15 )
    operator delete(v15);
  v16 = *((void **)this + 28);
  if ( v16 )
    operator delete(v16);
  v17 = *((void **)this + 24);
  if ( v17 )
    operator delete(v17);
  v18 = *((_QWORD *)this + 20);
  v19 = *((void **)this + 21);
  if ( (void *)v18 != v19 )
  {
    do
    {
      v20 = *(void **)(v18 + 216);
      if ( v20 )
        operator delete(v20);
      v21 = *(void **)(v18 + 168);
      if ( v21 != (void *)(v18 + 184) )
        operator delete(v21);
      v22 = *(void **)(v18 + 120);
      if ( v22 )
        operator delete(v22);
      v23 = *(void **)(v18 + 88);
      if ( v23 )
        operator delete(v23);
      v24 = *(void **)(v18 + 64);
      if ( v24 )
        operator delete(v24);
      v25 = *(void **)(v18 + 32);
      if ( v25 )
        operator delete(v25);
      if ( *(_QWORD *)v18 )
        operator delete(*(void **)v18);
      v18 += 248;
    }
    while ( v19 != (void *)v18 );
    v19 = *((void **)this + 20);
  }
  if ( v19 )
    operator delete(v19);
  v26 = *((_QWORD **)this + 16);
  v27 = *((_QWORD **)this + 17);
  if ( v26 != v27 )
  {
    do
    {
      v28 = (void *)v26[29];
      if ( v28 )
        operator delete(v28);
      v29 = (void *)v26[26];
      if ( v29 )
        operator delete(v29);
      v30 = (void *)v26[22];
      if ( v30 )
        operator delete(v30);
      v31 = (void *)v26[19];
      if ( v31 )
        operator delete(v31);
      v32 = (void *)v26[15];
      if ( v32 )
        operator delete(v32);
      v33 = (void *)v26[11];
      if ( v33 )
        operator delete(v33);
      v34 = (void *)v26[7];
      if ( v34 )
        operator delete(v34);
      v35 = (void *)v26[4];
      if ( v35 )
        operator delete(v35);
      v36 = (void *)v26[1];
      if ( v36 )
        operator delete(v36);
      v26 += 32;
    }
    while ( v27 != v26 );
    v27 = *((_QWORD **)this + 16);
  }
  if ( v27 )
    operator delete(v27);
  v37 = *((_QWORD *)this + 13);
  v38 = *((void **)this + 14);
  if ( (void *)v37 != v38 )
  {
    do
    {
      v39 = *(void **)(v37 + 120);
      if ( v39 )
        operator delete(v39);
      v40 = *(void **)(v37 + 96);
      if ( v40 )
        operator delete(v40);
      v41 = *(void **)(v37 + 64);
      if ( v41 )
        operator delete(v41);
      v42 = *(void **)(v37 + 32);
      if ( v42 )
        operator delete(v42);
      if ( *(_QWORD *)v37 )
        operator delete(*(void **)v37);
      v37 += 144;
    }
    while ( v38 != (void *)v37 );
    v38 = *((void **)this + 13);
  }
  if ( v38 )
    operator delete(v38);
  v43 = *((_QWORD *)this + 10);
  v44 = *((void **)this + 11);
  if ( (void *)v43 != v44 )
  {
    do
    {
      v45 = *(void **)(v43 + 32);
      if ( v45 != (void *)(v43 + 48) )
        operator delete(v45);
      if ( *(_QWORD *)v43 )
        operator delete(*(void **)v43);
      v43 += 80;
    }
    while ( v44 != (void *)v43 );
    v44 = *((void **)this + 10);
  }
  if ( v44 )
    operator delete(v44);
  v46 = *((_QWORD *)this + 7);
  v47 = *((void **)this + 8);
  if ( (void *)v46 != v47 )
  {
    do
    {
      v48 = *(void **)(v46 + 120);
      if ( v48 )
        operator delete(v48);
      v49 = *(void **)(v46 + 96);
      if ( v49 )
        operator delete(v49);
      v50 = *(void **)(v46 + 64);
      if ( v50 )
        operator delete(v50);
      v51 = *(void **)(v46 + 32);
      if ( v51 )
        operator delete(v51);
      if ( *(_QWORD *)v46 )
        operator delete(*(void **)v46);
      v46 += 144;
    }
    while ( v47 != (void *)v46 );
    v47 = *((void **)this + 7);
  }
  if ( v47 )
    operator delete(v47);
  v52 = *((char **)this + 2);
  if ( v52 != (char *)this + 32 )
    operator delete(v52);
}


