/* ============================================================================
 * SC_AudioScenes
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 2844 bytes of .text.
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
/* 0x00073840   1216  SC_AudioScenes::~SC_AudioScenes() */
/* 0x000a55c0    344  SC_AudioScenes::operator==(SC_AudioScenes const&) const */
/* 0x000a5720    280  SC_AudioScenes::CheckConformance() const */
/* 0x000a8e60   1004  SC_AudioScenes::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x00073840  size 1216 bytes  304 insns
 *   SC_AudioScenes::~SC_AudioScenes()
 *   SC_AudioScenes::~SC_AudioScenes()
 *   [_ZN14SC_AudioScenesD2Ev]
 *   [_ZN14SC_AudioScenesD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN14SC_AudioScenesD1Ev'
void __fastcall SC_AudioScenes::~SC_AudioScenes(SC_AudioScenes *this)
{
  void *v2; // x0
  void *v3; // x0
  void *v4; // x0
  void *v5; // x0
  void *v6; // x0
  void *v7; // x0
  void *v8; // x0
  void *v9; // x0
  _QWORD *v10; // x19
  _QWORD *v11; // x24
  _QWORD *v12; // x21
  _QWORD *v13; // x23
  void *v14; // x0
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  char *v18; // x0
  _QWORD *v19; // x19
  _QWORD *v20; // x23
  _QWORD *v21; // x0
  _QWORD *v22; // x0
  void *v23; // x0
  void *v24; // x0
  __int64 v25; // x21
  void *v26; // x24
  void *v27; // x0
  void *v28; // x0
  void *v29; // x0
  void *v30; // x0
  void *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  void *v35; // x0
  __int64 v36; // x21
  void *v37; // x24
  void *v38; // x0
  void *v39; // x0
  void *v40; // x0
  void *v41; // x0
  void *v42; // x0
  void *v43; // x0
  _QWORD *v44; // x21
  _QWORD *v45; // x24
  void *v46; // x0
  void *v47; // x0
  void *v48; // x0
  void *v49; // x0
  void *v50; // x0
  void *v51; // x0
  void *v52; // x0
  void *v53; // x0
  void *v54; // x0
  __int64 v55; // x21
  void *v56; // x24
  void *v57; // x0
  void *v58; // x0
  void *v59; // x0
  void *v60; // x0
  __int64 v61; // x21
  void *v62; // x24
  void *v63; // x0
  __int64 v64; // x21
  void *v65; // x24
  void *v66; // x0
  void *v67; // x0
  void *v68; // x0
  void *v69; // x0
  _QWORD *v70; // x0
  __int64 v71; // [xsp+50h] [xbp+50h]
  v2 = *((void **)this + 42);
  if ( v2 )
    operator delete(v2);
  v3 = *((void **)this + 39);
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 35);
  if ( v4 )
    operator delete(v4);
  v5 = *((void **)this + 32);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 28);
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 24);
  if ( v7 )
    operator delete(v7);
  v8 = *((void **)this + 20);
  if ( v8 )
    operator delete(v8);
  v9 = *((void **)this + 17);
  if ( v9 )
    operator delete(v9);
  v10 = *((_QWORD **)this + 13);
  v11 = *((_QWORD **)this + 14);
  if ( v10 != v11 )
  {
    do
    {
      v12 = (_QWORD *)v10[8];
      v13 = (_QWORD *)v10[9];
      if ( v12 != v13 )
      {
        do
        {
          v14 = (void *)v12[7];
          if ( v14 )
            operator delete(v14);
          v15 = (void *)v12[4];
          if ( v15 )
            operator delete(v15);
          v12 += 10;
        }
        while ( v13 != v12 );
        v13 = (_QWORD *)v10[8];
      }
      if ( v13 )
        operator delete(v13);
      v16 = (void *)v10[4];
      if ( v16 )
        operator delete(v16);
      v17 = (void *)v10[1];
      if ( v17 )
        operator delete(v17);
      v10 += 11;
    }
    while ( v11 != v10 );
    v11 = *((_QWORD **)this + 13);
  }
  if ( v11 )
    operator delete(v11);
  v18 = *((char **)this + 7);
  if ( v18 != (char *)this + 72 )
    operator delete(v18);
  v19 = *((_QWORD **)this + 1);
  v20 = *((_QWORD **)this + 2);
  if ( v19 != v20 )
  {
    do
    {
      v21 = (_QWORD *)v19[56];
      if ( v21 != v19 + 58 )
        operator delete(v21);
      v22 = (_QWORD *)v19[50];
      if ( v22 != v19 + 52 )
        operator delete(v22);
      v23 = (void *)v19[46];
      if ( v23 )
        operator delete(v23);
      v24 = (void *)v19[43];
      if ( v24 )
        operator delete(v24);
      v25 = v19[39];
      v26 = (void *)v19[40];
      if ( (void *)v25 != v26 )
      {
        do
        {
          v27 = *(void **)(v25 + 152);
          if ( v27 )
            operator delete(v27);
          v28 = *(void **)(v25 + 128);
          if ( v28 )
            operator delete(v28);
          v29 = *(void **)(v25 + 96);
          if ( v29 )
            operator delete(v29);
          v30 = *(void **)(v25 + 64);
          if ( v30 )
            operator delete(v30);
          v31 = *(void **)(v25 + 32);
          if ( v31 )
            operator delete(v31);
          if ( *(_QWORD *)v25 )
            operator delete(*(void **)v25);
          v25 += 176;
        }
        while ( v26 != (void *)v25 );
        v26 = (void *)v19[39];
      }
      if ( v26 )
        operator delete(v26);
      v32 = (void *)v19[35];
      if ( v32 )
        operator delete(v32);
      v33 = (void *)v19[32];
      if ( v33 )
        operator delete(v33);
      v34 = (void *)v19[28];
      if ( v34 )
        operator delete(v34);
      v35 = (void *)v19[24];
      if ( v35 )
        operator delete(v35);
      v36 = v19[20];
      v37 = (void *)v19[21];
      if ( (void *)v36 != v37 )
      {
        do
        {
          v38 = *(void **)(v36 + 216);
          if ( v38 )
            operator delete(v38);
          v39 = *(void **)(v36 + 168);
          if ( v39 != (void *)(v36 + 184) )
            operator delete(v39);
          v40 = *(void **)(v36 + 120);
          if ( v40 )
            operator delete(v40);
          v41 = *(void **)(v36 + 88);
          if ( v41 )
            operator delete(v41);
          v42 = *(void **)(v36 + 64);
          if ( v42 )
            operator delete(v42);
          v43 = *(void **)(v36 + 32);
          if ( v43 )
            operator delete(v43);
          if ( *(_QWORD *)v36 )
            operator delete(*(void **)v36);
          v36 += 248;
        }
        while ( v37 != (void *)v36 );
        v37 = (void *)v19[20];
      }
      if ( v37 )
        operator delete(v37);
      v44 = (_QWORD *)v19[16];
      v45 = (_QWORD *)v19[17];
      if ( v44 != v45 )
      {
        do
        {
          v46 = (void *)v44[29];
          if ( v46 )
            operator delete(v46);
          v47 = (void *)v44[26];
          if ( v47 )
            operator delete(v47);
          v48 = (void *)v44[22];
          if ( v48 )
            operator delete(v48);
          v49 = (void *)v44[19];
          if ( v49 )
            operator delete(v49);
          v50 = (void *)v44[15];
          if ( v50 )
            operator delete(v50);
          v51 = (void *)v44[11];
          if ( v51 )
            operator delete(v51);
          v52 = (void *)v44[7];
          if ( v52 )
            operator delete(v52);
          v53 = (void *)v44[4];
          if ( v53 )
            operator delete(v53);
          v54 = (void *)v44[1];
          if ( v54 )
            operator delete(v54);
          v44 += 32;
        }
        while ( v45 != v44 );
        v45 = (_QWORD *)v19[16];
      }
      if ( v45 )
        operator delete(v45);
      v55 = v19[13];
      v56 = (void *)v19[14];
      if ( (void *)v55 != v56 )
      {
        do
        {
          v57 = *(void **)(v55 + 120);
          if ( v57 )
            operator delete(v57);
          v58 = *(void **)(v55 + 96);
          if ( v58 )
            operator delete(v58);
          v59 = *(void **)(v55 + 64);
          if ( v59 )
            operator delete(v59);
          v60 = *(void **)(v55 + 32);
          if ( v60 )
            operator delete(v60);
          if ( *(_QWORD *)v55 )
            operator delete(*(void **)v55);
          v55 += 144;
        }
        while ( v56 != (void *)v55 );
        v56 = (void *)v19[13];
      }
      if ( v56 )
        operator delete(v56);
      v61 = v19[10];
      v62 = (void *)v19[11];
      if ( (void *)v61 != v62 )
      {
        do
        {
          v63 = *(void **)(v61 + 32);
          if ( v63 != (void *)(v61 + 48) )
            operator delete(v63);
          if ( *(_QWORD *)v61 )
            operator delete(*(void **)v61);
          v61 += 80;
        }
        while ( v62 != (void *)v61 );
        v62 = (void *)v19[10];
      }
      if ( v62 )
        operator delete(v62);
      v64 = v19[7];
      v65 = (void *)v19[8];
      if ( (void *)v64 != v65 )
      {
        do
        {
          v66 = *(void **)(v64 + 120);
          if ( v66 )
            operator delete(v66);
          v67 = *(void **)(v64 + 96);
          if ( v67 )
            operator delete(v67);
          v68 = *(void **)(v64 + 64);
          if ( v68 )
            operator delete(v68);
          v69 = *(void **)(v64 + 32);
          if ( v69 )
            operator delete(v69);
          if ( *(_QWORD *)v64 )
            operator delete(*(void **)v64);
          v64 += 144;
        }
        while ( v65 != (void *)v64 );
        v65 = (void *)v19[7];
      }
      if ( v65 )
        operator delete(v65);
      v70 = (_QWORD *)v19[2];
      if ( v70 != v19 + 4 )
        operator delete(v70);
      v19 += 61;
    }
    while ( v20 != v19 );
    v20 = *((_QWORD **)this + 1);
  }
  if ( v20 )
    operator delete(v20);
}


/* ------------------------------------------------------------------------ */
/* 0x000a55c0  size 344 bytes  86 insns
 *   SC_AudioScenes::operator==(SC_AudioScenes const&) const
 *   [_ZNK14SC_AudioSceneseqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_AudioScenes::operator==(SC_AudioScenes *this, const SC_AudioScenes *scenes)
{
  const SC_SceneComposition *v2; // x23
  SC_SceneComposition *v3; // x24
  SC_SceneComposition *v7; // x22
  const SC_SceneComposition *v8; // [xsp+30h] [xbp+30h]
  SC_SceneComposition *v9; // [xsp+38h] [xbp+38h]
  __int64 v10; // [xsp+48h] [xbp+48h]
  __int64 v11; // [xsp+50h] [xbp+50h]
  if ( *((unsigned __int8 *)this + 1) == *((unsigned __int8 *)scenes + 1)
    && *((unsigned __int8 *)this + 2) == *((unsigned __int8 *)scenes + 2)
    && *((unsigned __int8 *)this + 3) == *((unsigned __int8 *)scenes + 3)
    && *((unsigned __int8 *)this + 4) == *((unsigned __int8 *)scenes + 4) )
  {
    v7 = *((SC_SceneComposition **)this + 1);
    v3 = *((SC_SceneComposition **)this + 2);
    v2 = *((const SC_SceneComposition **)scenes + 1);
    if ( v3 - v7 == *((_QWORD *)scenes + 2) - (_QWORD)v2 )
    {
      if ( v7 == v3 )
      {
LABEL_14:
        if ( *((unsigned __int8 *)scenes + 184) == *((unsigned __int8 *)this + 184)
          && (!*((_BYTE *)scenes + 184)
           || SC_Tag::operator==((SC_AudioScenes *)((char *)this + 192), (const SC_AudioScenes *)((char *)scenes + 192))) )
        {
          return SC_Extension::operator==(
                   (SC_AudioScenes *)((char *)this + 304),
                   (const SC_AudioScenes *)((char *)scenes + 304));
        }
      }
      else
      {
        while ( SC_SceneComposition::operator==(v7, v2) )
        {
          v7 = (SC_SceneComposition *)((char *)v7 + 488);
          v2 = (const SC_SceneComposition *)((char *)v2 + 488);
          if ( v3 == v7 )
            goto LABEL_14;
        }
      }
    }
  }
  if ( v10 != _stack_chk_guard )
  {
    v8 = v2;
    v9 = v3;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000a5720  size 280 bytes  65 insns
 *   SC_AudioScenes::CheckConformance() const
 *   [_ZNK14SC_AudioScenes16CheckConformanceEv]
 *   data refs: 0x330820, 0x330858, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_AudioScenes::CheckConformance(SC_AudioScenes *this)
{
  __int64 v1; // x19
  __int64 v2; // x20
  __int64 v3; // x25
  __int64 v5; // x2
  int v6; // w22
  __int64 v7; // x4
  int v8; // w25
  __int64 v9; // x23
  __int64 result; // x0
  __int64 v11; // [xsp+10h] [xbp+10h]
  __int64 v12; // [xsp+18h] [xbp+18h]
  __int64 v13; // [xsp+40h] [xbp+40h]
  __int64 v14; // [xsp+58h] [xbp+58h]
  __int64 v15; // [xsp+60h] [xbp+60h]
  v5 = *((_QWORD *)this + 1);
  if ( v5 == *((_QWORD *)this + 2) )
  {
    __printf_chk(1, "ERROR: AudioScenes payload has missing elements\n");
    result = 0xFFFFFFFFLL;
  }
  else
  {
    v6 = 0;
    v7 = 0;
    v13 = v3;
    v8 = 0;
    do
    {
      v9 = 488 * v7;
      result = SC_SceneComposition::CheckConformance((SC_SceneComposition *)(v5 + 488 * v7));
      if ( (_DWORD)result )
        goto LABEL_7;
      v5 = *((_QWORD *)this + 1);
      v7 = (unsigned int)++v6;
      if ( *(_BYTE *)(v5 + v9) )
        v8 = *(unsigned __int8 *)(v5 + v9);
    }
    while ( 0x4FBCDA3AC10C9715LL * ((*((_QWORD *)this + 2) - v5) >> 3) > (unsigned __int64)(unsigned int)v7 );
    if ( v8 )
    {
LABEL_7:
      v3 = v13;
      goto LABEL_8;
    }
    __printf_chk(1, "ERROR: AudioScenes has no global scene\n");
    result = 0xFFFFFFFFLL;
    v3 = v13;
  }
LABEL_8:
  if ( v14 != _stack_chk_guard )
  {
    v11 = v1;
    v12 = v2;
    v13 = v3;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a8e60  size 1004 bytes  250 insns
 *   SC_AudioScenes::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN14SC_AudioScenes11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_AudioScenes::Deserialize(SC_AudioScenes *this, TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w1
  int v5; // w0
  unsigned int v6; // w20
  unsigned int v7; // w1
  int v8; // w0
  unsigned int v9; // w20
  unsigned int v10; // w1
  int v11; // w0
  unsigned int v12; // w20
  unsigned int v13; // w1
  int v14; // w0
  unsigned int v15; // w20
  unsigned int v16; // w1
  int v17; // w0
  unsigned int v18; // w20
  unsigned int v19; // w24
  __int64 v20; // x23
  SC_SceneComposition *v21; // x25
  unsigned __int64 v22; // x0
  __int64 v23; // x20
  __int64 result; // x0
  unsigned int v25; // w1
  int v26; // w0
  unsigned int v27; // w20
  SC_SceneComposition *v28; // x23
  SC_SceneComposition *v29; // x20
  SC_SceneComposition *v30; // x0
  unsigned int v31; // w0
  int v32; // w1
  char v33; // w20
  unsigned int v34; // w0
  int v35; // w0
  unsigned int v36; // w0
  int v37; // w1
  char v38; // w20
  unsigned int v39; // w0
  int v40; // w0
  unsigned int v41; // w2
  int v42; // w3
  const unsigned __int8 *cur; // x1
  unsigned int v44; // w0
  int v45; // w1
  unsigned int v46; // w0
  unsigned int v47; // w0
  int bitsLeft; // w1
  unsigned int v49; // w0
  unsigned int v50; // w0
  int v51; // w1
  unsigned int v52; // w0
  __int64 v53; // [xsp+60h] [xbp+60h]
  cache = bs->cache;
  v5 = bs->bitsLeft - 1;
  bs->bitsLeft = v5;
  v6 = cache >> 31;
  if ( v5 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v47 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v6) = v6 | (v47 >> (bitsLeft + 32));
    v49 = v47 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v49 = 0;
    bs->cur += 4;
    bs->cache = v49;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  *((_BYTE *)this + 1) = (unsigned __int8)v6 != 0;
  v7 = bs->cache;
  v8 = bs->bitsLeft - 1;
  bs->bitsLeft = v8;
  v9 = v7 >> 31;
  if ( v8 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v44 = bs->cache;
    v45 = bs->bitsLeft;
    LOBYTE(v9) = v9 | (v44 >> (v45 + 32));
    v46 = v44 << -(char)v45;
    if ( v45 == -32 )
      v46 = 0;
    bs->cur += 4;
    bs->cache = v46;
    bs->bitsLeft = v45 + 32;
  }
  else
  {
    bs->cache = 2 * v7;
  }
  *((_BYTE *)this + 2) = (unsigned __int8)v9 != 0;
  v10 = bs->cache;
  v11 = bs->bitsLeft - 1;
  bs->bitsLeft = v11;
  v12 = v10 >> 31;
  if ( (v11 & 0x80000000) == 0 )
  {
    bs->cache = 2 * v10;
    *((_BYTE *)this + 3) = v10 >> 31 != 0;
    v13 = bs->cache;
    v14 = bs->bitsLeft - 2;
    bs->bitsLeft = v14;
    v15 = v13 >> 30;
    if ( (v14 & 0x80000000) == 0 )
      goto LABEL_7;
LABEL_31:
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v36 = bs->cache;
    v37 = bs->bitsLeft;
    v38 = v15 | (v36 >> (v37 + 32));
    v39 = v36 << -(char)v37;
    if ( v37 == -32 )
      v39 = 0;
    bs->cur += 4;
    bs->cache = v39;
    bs->bitsLeft = v37 + 32;
    *((_BYTE *)this + 4) = v38;
    v16 = bs->cache;
    v40 = bs->bitsLeft - 6;
    bs->bitsLeft = v40;
    v18 = v16 >> 26;
    if ( (v40 & 0x80000000) == 0 )
      goto LABEL_8;
    goto LABEL_34;
  }
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v31 = bs->cache;
  v32 = bs->bitsLeft;
  v33 = v12 | (v31 >> (v32 + 32));
  v34 = v31 << -(char)v32;
  if ( v32 == -32 )
    v34 = 0;
  bs->cur += 4;
  bs->cache = v34;
  bs->bitsLeft = v32 + 32;
  *((_BYTE *)this + 3) = v33 != 0;
  v13 = bs->cache;
  v35 = bs->bitsLeft - 2;
  bs->bitsLeft = v35;
  v15 = v13 >> 30;
  if ( v35 < 0 )
    goto LABEL_31;
LABEL_7:
  bs->cache = 4 * v13;
  *((_BYTE *)this + 4) = v15;
  v16 = bs->cache;
  v17 = bs->bitsLeft - 6;
  bs->bitsLeft = v17;
  v18 = v16 >> 26;
  if ( (v17 & 0x80000000) == 0 )
  {
LABEL_8:
    v19 = v18;
    bs->cache = v16 << 6;
    goto LABEL_9;
  }
LABEL_34:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v41 = bs->cache;
  v42 = bs->bitsLeft;
  cur = bs->cur;
  bs->bitsLeft = v42 + 32;
  bs->cur = cur + 4;
  LOWORD(v18) = v18 | (v41 >> (v42 + 32));
  v19 = (unsigned __int16)v18;
  if ( v42 == -32 )
    bs->cache = 0;
  else
    bs->cache = v41 << -(char)v42;
LABEL_9:
  v20 = *((_QWORD *)this + 1);
  v21 = *((SC_SceneComposition **)this + 2);
  v22 = 0x4FBCDA3AC10C9715LL * (((__int64)v21 - v20) >> 3);
  if ( (unsigned __int16)v18 > v22 )
  {
    std::vector<SC_SceneComposition>::_M_default_append((std_vector *)((char *)this + 8), (unsigned __int16)v18 - v22);
  }
  else if ( (unsigned __int16)v18 < v22 )
  {
    v28 = (SC_SceneComposition *)(v20 + 488LL * (unsigned __int16)v18);
    if ( v21 != v28 )
    {
      v29 = v28;
      do
      {
        v30 = v29;
        v29 = (SC_SceneComposition *)((char *)v29 + 488);
        SC_SceneComposition::~SC_SceneComposition(v30);
      }
      while ( v21 != v29 );
      *((_QWORD *)this + 2) = v28;
    }
  }
  if ( v19 )
  {
    v23 = 0;
    while ( 1 )
    {
      result = SC_SceneComposition::Deserialize((SC_SceneComposition *)(*((_QWORD *)this + 1) + v23), bs, 0);
      if ( (_DWORD)result )
        break;
      v23 += 488;
      if ( 488LL * (v19 - 1) + 488 == v23 )
        goto LABEL_16;
    }
  }
  else
  {
LABEL_16:
    v25 = bs->cache;
    v26 = bs->bitsLeft - 1;
    bs->bitsLeft = v26;
    v27 = v25 >> 31;
    if ( v26 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v50 = bs->cache;
      v51 = bs->bitsLeft;
      LOBYTE(v27) = v27 | (v50 >> (v51 + 32));
      v52 = v50 << -(char)v51;
      if ( v51 == -32 )
        v52 = 0;
      bs->cur += 4;
      bs->cache = v52;
      bs->bitsLeft = v51 + 32;
    }
    else
    {
      bs->cache = 2 * v25;
    }
    *((_BYTE *)this + 184) = (unsigned __int8)v27 != 0;
    if ( !(_BYTE)v27 || (result = SC_Tag::Deserialize((SC_AudioScenes *)((char *)this + 192), bs), !(_DWORD)result) )
    {
      result = SC_Extension::Deserialize((SC_AudioScenes *)((char *)this + 304), bs);
      if ( !(_DWORD)result )
      {
        *((_WORD *)this + 153) = 0;
        result = SC_AudioScenes::CheckConformance(this);
        if ( !(_DWORD)result )
          *(_BYTE *)this = 1;
      }
    }
  }
  return result;
}


