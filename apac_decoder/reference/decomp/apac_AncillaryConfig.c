/* ============================================================================
 * apac::AncillaryConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 1752 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00072cc0     92  apac::AncillaryConfig::AncillaryConfig() */
/* 0x00072d20    212  apac::AncillaryConfig::~AncillaryConfig() */
/* 0x00072e00     84  apac::AncillaryConfig::~AncillaryConfig() */
/* 0x00072e60   1364  apac::AncillaryConfig::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, unsigned int, unsigned int) */

/* ------------------------------------------------------------------------ */
/* 0x00072cc0  size 92 bytes  23 insns
 *   apac::AncillaryConfig::AncillaryConfig()
 *   apac::AncillaryConfig::AncillaryConfig()
 *   [_ZN4apac15AncillaryConfigC2Ev]
 *   [_ZN4apac15AncillaryConfigC1Ev]
 *   data refs: 0x42a350, 0x42ce00, 0x42df38, 0x43e520
 */
// Alternative name is '_ZN4apac15AncillaryConfigC1Ev'
__int64 __fastcall apac::AncillaryConfig::AncillaryConfig(apac__AncillaryConfig *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 8) = 0;
  *((_QWORD *)this + 2) = 0;
  *((_QWORD *)this + 3) = 0;
  *(_QWORD *)this = off_42A360;
  *((_QWORD *)this + 4) = 0;
  return (__int64)this;
}


/* ------------------------------------------------------------------------ */
/* 0x00072d20  size 212 bytes  53 insns
 *   apac::AncillaryConfig::~AncillaryConfig()
 *   apac::AncillaryConfig::~AncillaryConfig()
 *   [_ZN4apac15AncillaryConfigD2Ev]
 *   [_ZN4apac15AncillaryConfigD1Ev]
 *   data refs: 0x42a350, 0x42ce00, 0x42df38, 0x43e520
 */
// Alternative name is '_ZN4apac15AncillaryConfigD1Ev'
void __fastcall apac::AncillaryConfig::~AncillaryConfig(apac__AncillaryConfig *this)
{
  SC_AudioScenes **v2; // x21
  SC_AudioScenes *v3; // x22
  _QWORD *v4; // x19
  __int64 v5; // [xsp+40h] [xbp+40h]
  v2 = *((SC_AudioScenes ***)this + 4);
  *(_QWORD *)this = off_42A360;
  if ( v2 )
  {
    v3 = *v2;
    if ( *v2 )
    {
      SC_AudioScenes::~SC_AudioScenes(*v2);
      operator delete(v3);
    }
    operator delete(v2);
  }
  v4 = *((_QWORD **)this + 3);
  if ( v4 )
  {
    if ( *v4 )
      (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v4 + 8LL))(*v4);
    operator delete(v4);
  }
}


/* ------------------------------------------------------------------------ */
/* 0x00072e00  size 84 bytes  21 insns
 *   apac::AncillaryConfig::~AncillaryConfig()
 *   [_ZN4apac15AncillaryConfigD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall apac::AncillaryConfig::~AncillaryConfig(apac__AncillaryConfig *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  apac::AncillaryConfig::~AncillaryConfig(this);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00072e60  size 1364 bytes  341 insns
 *   apac::AncillaryConfig::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, unsigned int, unsigned int)
 *   [_ZN4apac15AncillaryConfig11DeserializeER16TBitstreamReaderIjEjjj]
 *   data refs: 0x30e870, 0x42a370, 0x42ce00, 0x42d808, 0x43e520
 */
__int64 __fastcall apac::AncillaryConfig::Deserialize(
        apac__AncillaryConfig *this,
        TBitstreamReader_u32 *bs,
        unsigned int n,
        unsigned int n2,
        unsigned int n3)
{
  __int64 v8; // x24
  __int64 v9; // x1
  __int64 v10; // x0
  __int64 v11; // x0
  int *v12; // x24
  __int64 v13; // x1
  __int64 v14; // x0
  __int64 v15; // x0
  int *v16; // x24
  __int64 v17; // x1
  __int64 v18; // x0
  signed int cache; // w1
  int v20; // w0
  unsigned int v21; // w21
  _BOOL4 v22; // w21
  unsigned int v24; // w1
  int v25; // w0
  unsigned int v26; // w21
  unsigned int v27; // w2
  apac__AudioSceneConfig *v28; // x0
  SC_AudioScenes **v29; // x21
  SC_AudioScenes *v30; // x24
  unsigned int v31; // w1
  int v32; // w0
  unsigned int v33; // w21
  unsigned int v34; // w2
  unsigned int v35; // w1
  int v36; // w0
  unsigned int v37; // w21
  unsigned int v38; // w3
  const unsigned __int8 *cur; // x1
  int bitsLeft; // w4
  const unsigned __int8 *v41; // x1
  int v42; // w21
  unsigned int v43; // w3
  const unsigned __int8 *v44; // x1
  int v45; // w4
  const unsigned __int8 *v46; // x1
  unsigned int v47; // w3
  const unsigned __int8 *v48; // x1
  int v49; // w4
  const unsigned __int8 *v50; // x1
  apac__LoudnessDRCConfig *v51; // x0
  _QWORD *v52; // x21
  unsigned int v53; // w0
  int v54; // w1
  unsigned int v55; // w0
  unsigned int v56; // [xsp+68h] [xbp+68h]
  unsigned int v57; // [xsp+6Ch] [xbp+6Ch]
  int v58; // [xsp+7Ch] [xbp+7Ch] BYREF
  int v59; // [xsp+80h] [xbp+80h] BYREF
  int v60; // [xsp+88h] [xbp+88h] BYREF
  int *v61; // [xsp+90h] [xbp+90h] BYREF
  char v62[8]; // [xsp+98h] [xbp+98h] BYREF
  int v63; // [xsp+A0h] [xbp+A0h] BYREF
  __int64 v64; // [xsp+A8h] [xbp+A8h]
  int *v65; // [xsp+B0h] [xbp+B0h]
  int *v66; // [xsp+B8h] [xbp+B8h]
  __int64 v67; // [xsp+C0h] [xbp+C0h]
  __int64 v68; // [xsp+D0h] [xbp+D0h]
  v56 = n2;
  v57 = n3;
  v58 = 0;
  v61 = &v58;
  v63 = 0;
  v64 = 0;
  v65 = &v63;
  v66 = &v63;
  v67 = 0;
  v8 = std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
         v62,
         &v63,
         &unk_30E870,
         &v61,
         &v60);
  v9 = operator new(0x10u);
  *(_DWORD *)(v9 + 8) = n;
  *(_QWORD *)v9 = off_42A380;
  v10 = *(_QWORD *)(v8 + 40);
  *(_QWORD *)(v8 + 40) = v9;
  if ( v10 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v10 + 8LL))(v10);
  v11 = v64;
  v59 = 2;
  v12 = &v63;
  if ( !v64 )
    goto LABEL_22;
  do
  {
    while ( *(int *)(v11 + 32) > 1 )
    {
      v12 = (int *)v11;
      v11 = *(_QWORD *)(v11 + 16);
      if ( !v11 )
        goto LABEL_6;
    }
    v11 = *(_QWORD *)(v11 + 24);
  }
  while ( v11 );
LABEL_6:
  if ( v12 == &v63 || v12[8] > 2 )
  {
LABEL_22:
    v61 = &v59;
    v12 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                   v62,
                   v12,
                   &unk_30E870,
                   &v61,
                   &v60);
  }
  v13 = operator new(0x10u);
  *(_DWORD *)(v13 + 8) = v56;
  *(_QWORD *)v13 = off_42A380;
  v14 = *((_QWORD *)v12 + 5);
  *((_QWORD *)v12 + 5) = v13;
  if ( v14 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v14 + 8LL))(v14);
  v15 = v64;
  v60 = 4;
  v16 = &v63;
  if ( !v64 )
    goto LABEL_21;
  do
  {
    while ( *(int *)(v15 + 32) > 3 )
    {
      v16 = (int *)v15;
      v15 = *(_QWORD *)(v15 + 16);
      if ( !v15 )
        goto LABEL_13;
    }
    v15 = *(_QWORD *)(v15 + 24);
  }
  while ( v15 );
LABEL_13:
  if ( v16 == &v63 || v16[8] > 4 )
  {
LABEL_21:
    v61 = &v60;
    v16 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                   v62,
                   v16,
                   &unk_30E870,
                   &v61,
                   &v59);
  }
  v17 = operator new(0x10u);
  *(_DWORD *)(v17 + 8) = v57;
  *(_QWORD *)v17 = off_42A380;
  v18 = *((_QWORD *)v16 + 5);
  *((_QWORD *)v16 + 5) = v17;
  if ( v18 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v18 + 8LL))(v18);
  *((_QWORD *)this + 2) = 0;
  cache = bs->cache;
  v20 = bs->bitsLeft - 1;
  bs->bitsLeft = v20;
  v21 = (unsigned int)cache >> 31;
  if ( v20 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v38 = bs->cache;
    cur = bs->cur;
    bitsLeft = bs->bitsLeft;
    v20 = bitsLeft + 32;
    bs->bitsLeft = bitsLeft + 32;
    v41 = cur + 4;
    bs->cur = v41;
    v42 = (unsigned __int8)((v38 >> (bitsLeft + 32)) | v21);
    if ( bitsLeft == -32 )
    {
      bs->cache = 0;
      if ( v42 )
        goto LABEL_19;
      LOBYTE(v26) = 0;
      bs->bitsLeft = -1;
      goto LABEL_46;
    }
    bs->cache = v38 << -(char)bitsLeft;
    if ( v42 )
      goto LABEL_19;
  }
  else
  {
    bs->cache = 2 * cache;
    if ( cache < 0 )
    {
LABEL_19:
      v22 = 0;
      goto LABEL_20;
    }
  }
  v24 = bs->cache;
  v25 = v20 - 1;
  bs->bitsLeft = v25;
  v26 = v24 >> 31;
  if ( (v25 & 0x80000000) == 0 )
  {
    v27 = v24 >> 31;
    bs->cache = 2 * v24;
    goto LABEL_29;
  }
  v41 = bs->cur;
LABEL_46:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v41);
  v43 = bs->cache;
  v44 = bs->cur;
  v45 = bs->bitsLeft;
  v25 = v45 + 32;
  bs->bitsLeft = v45 + 32;
  v46 = v44 + 4;
  bs->cur = v46;
  v27 = (unsigned __int8)((v43 >> (v45 + 32)) | v26);
  if ( v45 == -32 )
  {
    bs->cache = 0;
    if ( !((unsigned __int8)v43 | (unsigned __int8)v26) )
    {
      LOBYTE(v33) = 0;
      bs->bitsLeft = -1;
      goto LABEL_52;
    }
    goto LABEL_30;
  }
  bs->cache = v43 << -(char)v45;
LABEL_29:
  if ( v27 )
  {
LABEL_30:
    v28 = (apac__AudioSceneConfig *)operator new(8u);
    v29 = *((SC_AudioScenes ***)this + 4);
    *(_QWORD *)v28 = 0;
    *((_QWORD *)this + 4) = v28;
    if ( v29 )
    {
      v30 = *v29;
      if ( *v29 )
      {
        SC_AudioScenes::~SC_AudioScenes(*v29);
        operator delete(v30);
      }
      operator delete(v29);
      v28 = *((apac__AudioSceneConfig **)this + 4);
    }
    v22 = apac::AudioSceneConfig::Deserialize(v28, bs);
    if ( v22 )
      goto LABEL_20;
    *((_QWORD *)this + 2) |= 4uLL;
    v25 = bs->bitsLeft;
  }
  v31 = bs->cache;
  v32 = v25 - 1;
  bs->bitsLeft = v32;
  v33 = v31 >> 31;
  if ( (v32 & 0x80000000) == 0 )
  {
    v34 = v31 >> 31;
    bs->cache = 2 * v31;
    goto LABEL_38;
  }
  v46 = bs->cur;
LABEL_52:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v46);
  v47 = bs->cache;
  v48 = bs->cur;
  v49 = bs->bitsLeft;
  v32 = v49 + 32;
  bs->bitsLeft = v49 + 32;
  v50 = v48 + 4;
  bs->cur = v50;
  v34 = (unsigned __int8)((v47 >> (v49 + 32)) | v33);
  if ( v49 != -32 )
  {
    bs->cache = v47 << -(char)v49;
LABEL_38:
    if ( !v34 )
    {
LABEL_39:
      v35 = bs->cache;
      v36 = v32 - 2;
      bs->bitsLeft = v36;
      v37 = v35 >> 30;
      if ( (v36 & 0x80000000) == 0 )
      {
        bs->cache = 4 * v35;
        goto LABEL_41;
      }
      v50 = bs->cur;
LABEL_63:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v50);
      v53 = bs->cache;
      v54 = bs->bitsLeft;
      LOBYTE(v37) = v37 | (v53 >> (v54 + 32));
      v55 = v53 << -(char)v54;
      if ( v54 == -32 )
        v55 = 0;
      bs->cur += 4;
      bs->cache = v55;
      bs->bitsLeft = v54 + 32;
LABEL_41:
      if ( !(_BYTE)v37 )
      {
        v22 = 0;
        *((_BYTE *)this + 8) = 1;
        goto LABEL_20;
      }
      goto LABEL_19;
    }
    goto LABEL_54;
  }
  bs->cache = 0;
  if ( !((unsigned __int8)v47 | (unsigned __int8)v33) )
  {
    LOBYTE(v37) = 0;
    bs->bitsLeft = -2;
    goto LABEL_63;
  }
LABEL_54:
  v51 = (apac__LoudnessDRCConfig *)operator new(8u);
  v52 = *((_QWORD **)this + 3);
  *(_QWORD *)v51 = 0;
  *((_QWORD *)this + 3) = v51;
  if ( v52 )
  {
    if ( *v52 )
      (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v52 + 8LL))(*v52);
    operator delete(v52);
    v51 = *((apac__LoudnessDRCConfig **)this + 3);
  }
  v22 = apac::LoudnessDRCConfig::Deserialize(v51, bs, (const apac__ParamList *)v62, 4);
  if ( !v22 )
  {
    *((_QWORD *)this + 2) |= 2uLL;
    v32 = bs->bitsLeft;
    goto LABEL_39;
  }
LABEL_20:
  std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_erase(
    v62,
    v64);
  return v22;
}


