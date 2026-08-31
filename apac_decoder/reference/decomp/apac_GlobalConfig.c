/* ============================================================================
 * apac::GlobalConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 10 function(s), 6168 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000760b0     88  apac::GlobalConfig::ASCConfigData::configs() const */
/* 0x000951c0     72  apac::GlobalConfig::CodecConfigData::configs() const */
/* 0x000978c0    220  apac::GlobalConfig::Reset() */
/* 0x000979a0    940  apac::GlobalConfig::SetChannelLayout() */
/* 0x00097d50   4516  apac::GlobalConfig::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x00106e00     76  apac::GlobalConfig::ComponentConfigs::configs() */
/* 0x00106e50     64  apac::GlobalConfig::ASCConfigData::~ASCConfigData() */
/* 0x00106e90     64  apac::GlobalConfig::CodecConfigData::~CodecConfigData() */
/* 0x00106ed0     64  apac::GlobalConfig::CodecConfigData::~CodecConfigData() */
/* 0x00106f10     64  apac::GlobalConfig::ASCConfigData::~ASCConfigData() */

/* ------------------------------------------------------------------------ */
/* 0x000760b0  size 88 bytes  22 insns
 *   apac::GlobalConfig::ASCConfigData::configs() const
 *   [_ZNK4apac12GlobalConfig13ASCConfigData7configsEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::GlobalConfig::ASCConfigData::configs(apac__GlobalConfig__ASCConfigData *this)
{
  __int64 v1; // x2
  __int64 result; // x0
  __int64 v3; // [xsp+20h] [xbp+20h]
  v1 = *((_QWORD *)this + 1);
  result = v1 + 176;
  if ( *(_BYTE *)(v1 + 249) )
    return v1 + 200;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000951c0  size 72 bytes  18 insns
 *   apac::GlobalConfig::CodecConfigData::configs() const
 *   [_ZNK4apac12GlobalConfig15CodecConfigData7configsEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::GlobalConfig::CodecConfigData::configs(apac__GlobalConfig__CodecConfigData *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return *((_QWORD *)this + 1) + 176LL;
}


/* ------------------------------------------------------------------------ */
/* 0x000978c0  size 220 bytes  55 insns
 *   apac::GlobalConfig::Reset()
 *   [_ZN4apac12GlobalConfig5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::GlobalConfig::Reset(apac__GlobalConfig *this)
{
  _QWORD *v2; // x22
  _QWORD *v3; // x23
  _QWORD *v4; // x19
  _QWORD *v5; // x22
  _QWORD *v6; // x23
  _QWORD *v7; // x19
  __int64 v9; // [xsp+48h] [xbp+48h]
  __int64 v10; // [xsp+50h] [xbp+50h]
  v3 = *((_QWORD **)this + 22);
  v2 = *((_QWORD **)this + 23);
  if ( v3 != v2 )
  {
    v4 = *((_QWORD **)this + 22);
    do
    {
      if ( *v4 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v4 + 8LL))(*v4);
      ++v4;
    }
    while ( v2 != v4 );
    *((_QWORD *)this + 23) = v3;
  }
  v6 = *((_QWORD **)this + 25);
  v5 = *((_QWORD **)this + 26);
  if ( v6 != v5 )
  {
    v7 = *((_QWORD **)this + 25);
    do
    {
      if ( *v7 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v7 + 8LL))(*v7);
      ++v7;
    }
    while ( v5 != v7 );
    *((_QWORD *)this + 26) = v6;
  }
  *((_BYTE *)this + 56) = 0;
  *((_BYTE *)this + 88) = 0;
  *((_BYTE *)this + 248) = 0;
  return v9 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000979a0  size 940 bytes  235 insns
 *   apac::GlobalConfig::SetChannelLayout()
 *   [_ZN4apac12GlobalConfig16SetChannelLayoutEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::GlobalConfig::SetChannelLayout(apac__GlobalConfig *this)
{
  __int64 v1; // x25
  __int64 v2; // x26
  __int64 v3; // x27
  __int64 v4; // x28
  unsigned int v6; // w20
  __int64 *v7; // x19
  __int64 *v8; // x23
  unsigned int v9; // w22
  __int64 v10; // t1
  int v11; // w25
  __int64 v12; // x0
  unsigned int v13; // w1
  const void *v14; // x19
  char *v15; // x0
  char *v16; // x3
  unsigned __int64 v17; // x1
  size_t v18; // x2
  int8x8_t v19; // d0
  int *i; // x2
  int v21; // w19
  char *v23; // x1
  __int64 v24; // x3
  unsigned __int64 v25; // x0
  char *v26; // x0
  _QWORD *v27; // x26
  __int64 v28; // x19
  unsigned int v29; // w27
  _DWORD *v30; // x23
  unsigned int v31; // w28
  unsigned __int64 v32; // x28
  char *v33; // x0
  char *v34; // x24
  void *v35; // x21
  __int64 v36; // x2
  __int64 v37; // x3
  __int64 v38; // x4
  __int64 v39; // x5
  __int64 v40; // x6
  __int64 v41; // x7
  unsigned int v42; // w28
  _QWORD *v43; // x2
  unsigned int v44; // w1
  __int16 *ProfileConfig; // x0
  int *v46; // x1
  int *v47; // x4
  int v48; // w0
  __int64 v49; // [xsp+0h] [xbp+0h]
  __int64 v50; // [xsp+40h] [xbp+40h]
  __int64 v51; // [xsp+48h] [xbp+48h]
  __int64 v52; // [xsp+50h] [xbp+50h]
  __int64 v53; // [xsp+58h] [xbp+58h]
  _QWORD *v54; // [xsp+68h] [xbp+68h]
  void *v55; // [xsp+70h] [xbp+70h] BYREF
  char *v56; // [xsp+78h] [xbp+78h]
  char *v57; // [xsp+80h] [xbp+80h]
  __int64 v58; // [xsp+88h] [xbp+88h]
  __int64 v59; // [xsp+90h] [xbp+90h]
  v6 = GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig> *,std::vector<std::unique_ptr<apac::IASCConfig>>>>(
         *((void **)this + 22),
         *((void **)this + 23),
         (std_vector *)((char *)this + 224));
  if ( v6 )
    goto LABEL_16;
  v50 = v1;
  v51 = v2;
  v7 = *((__int64 **)this + 28);
  v8 = *((__int64 **)this + 29);
  if ( v7 == v8 )
  {
    v11 = (_DWORD)this + 256;
    v9 = 0;
  }
  else
  {
    v9 = 0;
    do
    {
      v10 = *v7++;
      v9 += (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v10 + 24LL))(v10);
    }
    while ( v8 != v7 );
    v8 = *((__int64 **)this + 28);
    v11 = (_DWORD)this + 256;
    if ( *((_QWORD *)this + 29) - (_QWORD)v8 > 0xFu )
    {
      v24 = *((_QWORD *)this + 32);
      v23 = *((char **)this + 33);
      LOBYTE(v55) = 0;
      v25 = 20LL * v9 + 12;
      if ( v25 > (unsigned __int64)&v23[-v24] )
      {
        std::vector<char>::_M_fill_insert((_DWORD)this + 256, v23, v25 - (_QWORD)&v23[-v24]);
        v24 = *((_QWORD *)this + 32);
      }
      else if ( v25 < (unsigned __int64)&v23[-v24] )
      {
        v26 = (char *)(v24 + v25);
        if ( v23 != v26 )
          *((_QWORD *)this + 33) = v26;
      }
      *(_DWORD *)(v24 + 8) = v9;
      **((_DWORD **)this + 32) = 0;
      v27 = *((_QWORD **)this + 28);
      v54 = *((_QWORD **)this + 29);
      v28 = *((_QWORD *)this + 32) + 12LL;
      if ( v27 != v54 )
      {
        v52 = v3;
        v53 = v4;
        while ( 1 )
        {
          v29 = (*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v27 + 24LL))(*v27);
          if ( *(_DWORD *)(*v27 + 8LL) )
          {
            if ( v29 )
            {
              v43 = (_QWORD *)v28;
              v44 = 0x10000;
              do
              {
                *v43 = v44++;
                v43 = (_QWORD *)((char *)v43 + 20);
              }
              while ( v44 != v29 + 0x10000 );
              v28 += 20LL * (v29 - 1) + 20;
            }
          }
          else
          {
            v30 = (_DWORD *)(*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v27 + 32LL))(*v27);
            v31 = v30[2];
            v55 = nullptr;
            v56 = nullptr;
            v57 = nullptr;
            v32 = 20LL * v31 + 12;
            v33 = (char *)operator new(v32);
            v34 = &v33[v32];
            v35 = v33;
            v55 = v33;
            v57 = &v33[v32];
            memset(v33, 0, v32);
            v56 = v34;
            memcpy(v35, v30, v32);
            v42 = sub_96850((unsigned int **)&v55, v28, v36, v37, v38, v39, v40, v41, v49);
            if ( v55 )
              operator delete(v55);
            if ( v42 )
            {
              v6 = v42;
              v1 = v50;
              v2 = v51;
              v3 = v52;
              v4 = v53;
              goto LABEL_16;
            }
            v28 += 20LL * v29;
          }
          if ( v54 == ++v27 )
          {
            v1 = v50;
            v2 = v51;
            v3 = v52;
            v4 = v53;
            goto LABEL_16;
          }
        }
      }
      goto LABEL_31;
    }
  }
  v12 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)*v8 + 32LL))(*v8);
  v13 = *(_DWORD *)(v12 + 8);
  v14 = (const void *)v12;
  v15 = *((char **)this + 32);
  v16 = *((char **)this + 33);
  v17 = 20LL * v13 + 12;
  v18 = v16 - v15;
  if ( v17 > v16 - v15 )
  {
    std::vector<char>::_M_default_append(v11, v17 - v18);
    v15 = *((char **)this + 32);
    v18 = *((_QWORD *)this + 33) - (_QWORD)v15;
  }
  else if ( v17 < v16 - v15 && v16 != &v15[v17] )
  {
    v18 = v17;
    *((_QWORD *)this + 33) = &v15[v17];
  }
  memcpy(v15, v14, v18);
  i = *((int **)this + 32);
  if ( *i )
  {
    if ( *i != 0x10000 )
    {
      if ( !(unsigned __int16)*i )
        goto LABEL_13;
LABEL_31:
      v1 = v50;
      v2 = v51;
      goto LABEL_16;
    }
    v19.n64_u32[0] = i[1];
    if ( vaddv_s8(vcnt_s8(v19)) )
      goto LABEL_31;
  }
  else if ( i[2] )
  {
    goto LABEL_31;
  }
LABEL_13:
  v21 = *(_DWORD *)(*v8 + 8);
  if ( v21 )
  {
    v21 = v9 | 0x930000;
    goto LABEL_15;
  }
  ProfileConfig = apac::GetProfileConfig(*((unsigned __int16 *)this + 16));
  if ( (unsigned __int16)*ProfileConfig == 0xFFFF )
  {
    v6 = 4;
    v1 = v50;
    v2 = v51;
  }
  else
  {
    v46 = *((int **)ProfileConfig + 3);
    v47 = *((int **)ProfileConfig + 4);
    for ( i = *((int **)this + 32); v47 != v46; ++v46 )
    {
      v48 = *v46;
      if ( (*v46 & 0xFFFF0000) == 0x930000 )
      {
        v21 = v9 | v48;
      }
      else if ( v9 == (unsigned __int16)v48 )
      {
        v21 = *v46;
        break;
      }
    }
LABEL_15:
    *i = v21;
    v1 = v50;
    v2 = v51;
  }
LABEL_16:
  if ( v58 != _stack_chk_guard )
  {
    v50 = v1;
    v51 = v2;
    v52 = v3;
    v53 = v4;
  }
  return v6;
}


/* ------------------------------------------------------------------------ */
/* 0x00097d50  size 4516 bytes  1128 insns
 *   apac::GlobalConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac12GlobalConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x0, 0x7e160, 0x30f3b8, 0x42a948, 0x42a980, 0x42aa50, 0x42afb8, 0x42cc30, 0x42cca0, 0x42ce00, 0x42d268, 0x42d2b8, 0x42d3f0, 0x43e520
 */
__int64 __fastcall apac::GlobalConfig::Deserialize(apac__GlobalConfig *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x27
  __int64 v3; // x28
  unsigned int cache; // w1
  int v7; // w0
  unsigned int v8; // w20
  unsigned int v9; // w1
  int v10; // w0
  unsigned int v11; // w20
  unsigned int v12; // w0
  int v13; // w2
  unsigned int v14; // w0
  int v15; // w2
  unsigned int v16; // w20
  unsigned int v17; // w1
  int v18; // w0
  unsigned int v19; // w20
  unsigned int v20; // w1
  int v21; // w0
  unsigned int v22; // w20
  unsigned int v23; // w1
  int v24; // w0
  unsigned int v25; // w1
  int v26; // w0
  unsigned int v27; // w24
  unsigned int Esc; // w28
  int v29; // w27
  unsigned int v30; // w26
  unsigned int v31; // w0
  int v32; // w2
  unsigned int v33; // w21
  int v34; // w2
  unsigned int v35; // w0
  unsigned int v36; // w20
  _DWORD *v37; // x20
  void (__fastcall **v38)(apac::chan::CoreDecoderConfig *__hidden); // x3
  __int64 v39; // x0
  __int64 v40; // x3
  __int64 *v41; // x2
  __int64 v42; // x1
  void *v43; // x0
  __int64 (__fastcall *v44)(apac__lbr__DecoderConfig *, TBitstreamReader_u32 *); // x2
  void *v45; // x0
  int v46; // w0
  __int16 v47; // w20
  int v48; // w0
  bool v49; // zf
  unsigned int v50; // w0
  int v51; // w1
  unsigned int v52; // w21
  int8x8_t v53; // d0
  unsigned int v54; // w1
  unsigned int v55; // w1
  int v56; // w0
  _WORD *v57; // x21
  __int64 v58; // x1
  void *v59; // x0
  int v60; // w0
  unsigned int v61; // w1
  int v62; // w0
  unsigned int v63; // w27
  _DWORD *v64; // x2
  unsigned int v65; // w1
  unsigned int v66; // w21
  int v67; // w0
  __int64 v68; // x28
  unsigned int v70; // w0
  int bitsLeft; // w1
  __int16 v72; // w20
  unsigned int v73; // w0
  int v74; // w0
  unsigned int v75; // w0
  int v76; // w1
  char v77; // w20
  unsigned int v78; // w0
  int v79; // w3
  const unsigned __int8 *cur; // x1
  int v81; // w2
  unsigned int v82; // w0
  int v83; // w1
  unsigned int v84; // w0
  unsigned int v85; // w0
  int v86; // w1
  char v87; // w20
  unsigned int v88; // w0
  unsigned int v89; // w0
  unsigned int v90; // w0
  int v91; // w3
  const unsigned __int8 *v92; // x1
  const unsigned __int8 *v93; // x1
  unsigned int v94; // w0
  int v95; // w1
  unsigned int v96; // w0
  unsigned int v97; // w0
  int v98; // w1
  char v99; // w20
  unsigned int v100; // w0
  int v101; // w2
  const unsigned __int8 *v102; // x1
  unsigned int v103; // w0
  int v104; // w1
  unsigned int v105; // w0
  void *v106; // x0
  unsigned int v107; // w1
  int v108; // w3
  unsigned int v109; // w1
  unsigned int v110; // w0
  int v111; // w1
  unsigned int v112; // w4
  unsigned int v113; // w0
  unsigned int v114; // w4
  int *v115; // x2
  int v116; // w1
  unsigned int v117; // w21
  _DWORD *v118; // x1
  __int64 v119; // x2
  __int64 v120; // x3
  __int64 v121; // x4
  __int64 v122; // x5
  __int64 v123; // x6
  __int64 v124; // x7
  char *i; // x0
  unsigned __int64 v126; // x24
  __int64 v127; // x1
  unsigned int v128; // w27
  void (__fastcall *v129)(apac::chan::CoreDecoderConfig *__hidden); // x20
  unsigned __int64 v130; // x0
  char *v131; // x1
  __int64 v132; // x3
  char *v133; // x0
  unsigned int v134; // w1
  __int64 v135; // x2
  unsigned int v136; // w0
  __int64 v137; // x0
  __int64 v138; // x1
  __int64 v139; // x20
  unsigned int v140; // w1
  int v141; // w0
  unsigned int v142; // w21
  unsigned int v143; // w21
  unsigned int v144; // w1
  int v145; // w0
  unsigned int v146; // w21
  int v147; // w0
  unsigned int v148; // w2
  int v149; // w5
  const unsigned __int8 *v150; // x1
  const unsigned __int8 *v151; // x1
  unsigned int v152; // w3
  int v153; // w5
  const unsigned __int8 *v154; // x1
  const unsigned __int8 *v155; // x1
  unsigned int v156; // w5
  const unsigned __int8 *v157; // x1
  int v158; // w3
  int v159; // w0
  unsigned int v160; // w2
  int v161; // w5
  const unsigned __int8 *v162; // x1
  int v163; // w2
  unsigned int v164; // w0
  int v165; // w1
  unsigned int v166; // w0
  __int64 v167; // x21
  __int64 v168; // x5
  __int64 v169; // x6
  __int64 v170; // x7
  int8x8_t v171; // d0
  _BYTE *v172; // x0
  _BYTE *v173; // x1
  signed __int64 v174; // x21
  char *v175; // x3
  char *v176; // x2
  size_t v177; // x21
  unsigned int *v178; // x23
  unsigned int v179; // w0
  char *v180; // x21
  unsigned int *v181; // x24
  signed __int64 v182; // x27
  __int64 v183; // x4
  __int64 v184; // x3
  __int64 v185; // x4
  __int64 v186; // x5
  __int64 v187; // x6
  __int64 v188; // x7
  char *v189; // x23
  unsigned int v190; // w2
  unsigned int *v191; // x0
  int v192; // w1
  __int64 v193; // x6
  _DWORD *v194; // x5
  unsigned int v195; // t1
  unsigned int v196; // w1
  __int64 v197; // x4
  char *v198; // x3
  _DWORD *v199; // x2
  __int64 v200; // x0
  __int64 v201; // x5
  _DWORD *v202; // x7
  int v203; // w6
  _DWORD *v204; // x2
  int v205; // w0
  int v206; // w21
  _DWORD *v207; // x0
  int *v208; // x2
  int v209; // w0
  __int64 v210; // [xsp+0h] [xbp+0h]
  __int64 v211; // [xsp+50h] [xbp+50h]
  __int64 v212; // [xsp+58h] [xbp+58h]
  void (__fastcall **v213)(apac::chan::CoreDecoderConfig *__hidden); // [xsp+60h] [xbp+60h]
  __int64 *v214; // [xsp+68h] [xbp+68h]
  unsigned int v215; // [xsp+74h] [xbp+74h]
  void (__fastcall **v216)(apac::BaseASCConfig *__hidden); // [xsp+78h] [xbp+78h]
  char v217; // [xsp+8Fh] [xbp+8Fh] BYREF
  void *v218; // [xsp+90h] [xbp+90h] BYREF
  char *v219; // [xsp+98h] [xbp+98h]
  void *v220[3]; // [xsp+A8h] [xbp+A8h] BYREF
  void *v221; // [xsp+C0h] [xbp+C0h] BYREF
  char *v222; // [xsp+C8h] [xbp+C8h]
  char *v223; // [xsp+D0h] [xbp+D0h]
  __int64 v224; // [xsp+D8h] [xbp+D8h]
  __int64 v225; // [xsp+E0h] [xbp+E0h]
  apac::GlobalConfig::Reset(this);
  cache = bs->cache;
  v7 = bs->bitsLeft - 6;
  bs->bitsLeft = v7;
  v8 = cache >> 26;
  if ( v7 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v70 = bs->cache;
    bitsLeft = bs->bitsLeft;
    v72 = v8 | (v70 >> (bitsLeft + 32));
    v73 = v70 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v73 = 0;
    bs->cur += 4;
    bs->cache = v73;
    bs->bitsLeft = bitsLeft + 32;
    *((_WORD *)this + 16) = v72;
    v9 = bs->cache;
    v74 = bs->bitsLeft - 4;
    bs->bitsLeft = v74;
    v11 = v9 >> 28;
    if ( (v74 & 0x80000000) == 0 )
    {
LABEL_3:
      bs->cache = 16 * v9;
      *((_BYTE *)this + 34) = v11;
      v12 = bs->cache;
      v13 = bs->bitsLeft;
      bs->bitsLeft = v13 - 1;
      if ( ((v13 - 1) & 0x80000000) == 0 )
        goto LABEL_4;
      goto LABEL_74;
    }
  }
  else
  {
    bs->cache = cache << 6;
    *((_WORD *)this + 16) = v8;
    v9 = bs->cache;
    v10 = bs->bitsLeft - 4;
    bs->bitsLeft = v10;
    v11 = v9 >> 28;
    if ( (v10 & 0x80000000) == 0 )
      goto LABEL_3;
  }
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v75 = bs->cache;
  v76 = bs->bitsLeft;
  v77 = v11 | (v75 >> (v76 + 32));
  v78 = v75 << -(char)v76;
  if ( v76 == -32 )
    v78 = 0;
  bs->cur += 4;
  bs->cache = v78;
  bs->bitsLeft = v76 + 32;
  *((_BYTE *)this + 34) = v77;
  v12 = bs->cache;
  v13 = bs->bitsLeft;
  bs->bitsLeft = v13 - 1;
  if ( ((v13 - 1) & 0x80000000) == 0 )
  {
LABEL_4:
    v14 = 2 * v12;
    v15 = v13 - 7;
    bs->cache = v14;
    bs->bitsLeft = v15;
    v16 = v14 >> 26;
    if ( (v15 & 0x80000000) == 0 )
    {
LABEL_5:
      bs->cache = v14 << 6;
      goto LABEL_6;
    }
    goto LABEL_84;
  }
LABEL_74:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v79 = bs->bitsLeft;
  cur = bs->cur + 4;
  bs->cur = cur;
  bs->bitsLeft = v79 + 32;
  v81 = v79 + 26;
  if ( v79 != -32 )
  {
    v89 = bs->cache;
    bs->bitsLeft = v81;
    v14 = v89 << -(char)v79;
    bs->cache = v14;
    v16 = v14 >> 26;
    if ( (v81 & 0x80000000) == 0 )
      goto LABEL_5;
LABEL_84:
    cur = bs->cur;
    goto LABEL_76;
  }
  v16 = 0;
  bs->cache = 0;
  bs->bitsLeft = v81;
LABEL_76:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, cur);
  v82 = bs->cache;
  v83 = bs->bitsLeft;
  v16 |= v82 >> (v83 + 32);
  v84 = v82 << -(char)v83;
  if ( v83 == -32 )
    v84 = 0;
  bs->cur += 4;
  bs->cache = v84;
  bs->bitsLeft = v83 + 32;
LABEL_6:
  if ( v16 > 0xC )
  {
LABEL_67:
    v27 = 1;
    goto LABEL_64;
  }
  *((_DWORD *)this + 9) = dword_30F3B8[v16];
  v17 = bs->cache;
  v18 = bs->bitsLeft - 6;
  bs->bitsLeft = v18;
  v19 = v17 >> 26;
  if ( (v18 & 0x80000000) == 0 )
  {
    bs->cache = v17 << 6;
    *((_BYTE *)this + 41) = v19;
    if ( !(v17 >> 26) )
      goto LABEL_9;
LABEL_82:
    v27 = 4;
    goto LABEL_64;
  }
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v85 = bs->cache;
  v86 = bs->bitsLeft;
  v87 = v19 | (v85 >> (v86 + 32));
  v88 = v85 << -(char)v86;
  if ( v86 == -32 )
    v88 = 0;
  bs->cur += 4;
  bs->cache = v88;
  bs->bitsLeft = v86 + 32;
  *((_BYTE *)this + 41) = v87;
  if ( v87 )
    goto LABEL_82;
LABEL_9:
  v20 = bs->cache;
  v21 = bs->bitsLeft - 8;
  bs->bitsLeft = v21;
  v22 = HIBYTE(v20);
  if ( v21 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v97 = bs->cache;
    v98 = bs->bitsLeft;
    v99 = v22 | (v97 >> (v98 + 32));
    v100 = v97 << -(char)v98;
    if ( v98 == -32 )
      v100 = 0;
    bs->cur += 4;
    bs->cache = v100;
    bs->bitsLeft = v98 + 32;
    *((_BYTE *)this + 43) = v99;
    v23 = bs->cache;
    v24 = bs->bitsLeft - 8;
    bs->bitsLeft = v24;
    if ( (v24 & 0x80000000) == 0 )
      goto LABEL_11;
  }
  else
  {
    bs->cache = v20 << 8;
    *((_BYTE *)this + 43) = HIBYTE(v20);
    v23 = bs->cache;
    v24 = bs->bitsLeft - 8;
    bs->bitsLeft = v24;
    if ( (v24 & 0x80000000) == 0 )
    {
LABEL_11:
      v25 = v23 << 8;
      bs->cache = v25;
      goto LABEL_12;
    }
  }
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v101 = bs->bitsLeft;
  v102 = bs->cur + 4;
  bs->cur = v102;
  v24 = v101 + 32;
  bs->bitsLeft = v101 + 32;
  if ( v101 == -32 )
  {
    v27 = 0;
    *(_QWORD *)&bs->cache = 0xFFFFFFFF00000000LL;
LABEL_100:
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, v102);
    v103 = bs->cache;
    v104 = bs->bitsLeft;
    v27 |= v103 >> (v104 + 32);
    v105 = v103 << -(char)v104;
    if ( v104 == -32 )
      v105 = 0;
    bs->cur += 4;
    bs->cache = v105;
    bs->bitsLeft = v104 + 32;
    goto LABEL_14;
  }
  v25 = bs->cache << -(char)v101;
  bs->cache = v25;
LABEL_12:
  v26 = v24 - 1;
  bs->bitsLeft = v26;
  v27 = v25 >> 31;
  if ( v26 < 0 )
  {
    v102 = bs->cur;
    goto LABEL_100;
  }
  bs->cache = 2 * v25;
LABEL_14:
  if ( v27 )
    goto LABEL_64;
  v211 = v2;
  v212 = v3;
  Esc = apac::GetEscBits<6u,12u>(bs, 3u);
  v221 = nullptr;
  v222 = nullptr;
  v223 = nullptr;
  if ( !Esc )
    goto LABEL_87;
  v29 = 0;
  v30 = 0;
  v213 = off_42A990;
  v214 = &`vtable for'apac::lbr::DecoderConfig;
  do
  {
    v31 = bs->cache;
    v32 = bs->bitsLeft;
    v220[0] = nullptr;
    bs->bitsLeft = v32 - 8;
    v33 = HIBYTE(v31);
    if ( v32 - 8 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v90 = bs->cache;
      v91 = bs->bitsLeft;
      v92 = bs->cur;
      bs->bitsLeft = v91 + 32;
      v34 = v91 + 29;
      v93 = v92 + 4;
      bs->cur = v93;
      v33 |= v90 >> (v91 + 32);
      if ( v91 == -32 )
      {
        v36 = 0;
        bs->cache = 0;
        bs->bitsLeft = v34;
        goto LABEL_90;
      }
      v35 = v90 << -(char)v91;
      bs->cache = v35;
    }
    else
    {
      v34 = v32 - 11;
      v35 = v31 << 8;
      bs->cache = v35;
    }
    bs->bitsLeft = v34;
    v36 = v35 >> 29;
    if ( (v34 & 0x80000000) == 0 )
    {
      bs->cache = 8 * v35;
      goto LABEL_21;
    }
    v93 = bs->cur;
LABEL_90:
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, v93);
    v94 = bs->cache;
    v95 = bs->bitsLeft;
    v36 |= v94 >> (v95 + 32);
    v96 = v94 << -(char)v95;
    if ( v95 == -32 )
      v96 = 0;
    bs->cur += 4;
    bs->cache = v96;
    bs->bitsLeft = v95 + 32;
LABEL_21:
    if ( v36 )
    {
      if ( v220[0] )
        (*(void (__fastcall **)(void *))(*(_QWORD *)v220[0] + 8LL))(v220[0]);
      if ( v221 )
        operator delete(v221);
      v27 = 4;
      v2 = v211;
      v3 = v212;
      goto LABEL_64;
    }
    v37 = (_DWORD *)operator new(0x68u);
    v38 = v213;
    v37[2] = 0;
    *(_QWORD *)v37 = v38;
    *((_QWORD *)v37 + 2) = 0;
    *((_QWORD *)v37 + 3) = 0;
    *((_QWORD *)v37 + 4) = 0;
    *((_QWORD *)v37 + 5) = 0xFFFFFFFFLL;
    *((_QWORD *)v37 + 6) = 0;
    *((_QWORD *)v37 + 7) = 0;
    *((_QWORD *)v37 + 8) = 0;
    *((_QWORD *)v37 + 9) = 0;
    *((_QWORD *)v37 + 10) = 0;
    *((_QWORD *)v37 + 11) = 0;
    v39 = operator new(0xCu);
    v40 = v39 + 12;
    v41 = v214 + 2;
    v42 = v39;
    *((_QWORD *)v37 + 9) = v39;
    v43 = v220[0];
    *((_QWORD *)v37 + 11) = v40;
    *(_QWORD *)v37 = v41;
    *(_QWORD *)v42 = 0;
    *(_DWORD *)(v42 + 8) = 0;
    *((_QWORD *)v37 + 10) = v40;
    *((_BYTE *)v37 + 96) = 0;
    v220[0] = v37;
    v44 = apac::lbr::DecoderConfig::Deserialize;
    if ( v43 )
    {
      (*(void (__fastcall **)(void *))(*(_QWORD *)v43 + 8LL))(v43);
      v37 = v220[0];
      v44 = *(__int64 (__fastcall **)(apac__lbr__DecoderConfig *, TBitstreamReader_u32 *))(*(_QWORD *)v220[0] + 16LL);
    }
    v27 = v44((apac__lbr__DecoderConfig *)v37, bs);
    v45 = v220[0];
    if ( v27 )
      goto LABEL_104;
    v46 = (*(__int64 (__fastcall **)(void *))(*(_QWORD *)v220[0] + 24LL))(v220[0]);
    v47 = v46;
    if ( !v46 || (v30 += v46, v30 <= v33) )
    {
      v27 = 1;
      v45 = v220[0];
LABEL_104:
      if ( v45 )
        (*(void (__fastcall **)(void *))(*(_QWORD *)v45 + 8LL))(v45);
      v106 = v221;
      if ( !v221 )
        goto LABEL_117;
      goto LABEL_107;
    }
    v48 = apac::ConvertToAudioDataType(0);
    v49 = !apac::ASCChannelMap::AddMap((apac__ASCChannelMap *)&v221, v48, v33, v47 + v33);
    v45 = v220[0];
    if ( v49 )
    {
      v27 = 1;
      goto LABEL_104;
    }
    *((_WORD *)v220[0] + 21) = v33;
    std::vector<std::unique_ptr<apac::IASCConfig>>::emplace_back<std::unique_ptr<apac::IASCConfig>>(
      (std_vector *)((char *)this + 176),
      v220);
    if ( v220[0] )
      (*(void (__fastcall **)(void *))(*(_QWORD *)v220[0] + 8LL))(v220[0]);
    ++v29;
  }
  while ( Esc != v29 );
  if ( v221 )
    operator delete(v221);
  v27 = apac::ProfileLevelConfig::ValidateProfileLevelData(
          (apac__GlobalConfig *)((char *)this + 32),
          (const std_vector *)((char *)this + 176));
  if ( v27 )
    goto LABEL_117;
  v27 = apac::GlobalConfig::SetChannelLayout(this);
  if ( v27 )
    goto LABEL_117;
  v50 = bs->cache;
  v51 = bs->bitsLeft - 1;
  bs->bitsLeft = v51;
  v52 = v50 >> 31;
  if ( v51 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v164 = bs->cache;
    v165 = bs->bitsLeft;
    LOBYTE(v52) = v52 | (v164 >> (v165 + 32));
    v166 = v164 << -(char)v165;
    if ( v165 == -32 )
      v166 = 0;
    bs->cur += 4;
    bs->cache = v166;
    bs->bitsLeft = v165 + 32;
  }
  else
  {
    bs->cache = 2 * v50;
  }
  *((_BYTE *)this + 249) = (unsigned __int8)v52 != 0;
  if ( !(_BYTE)v52 )
    goto LABEL_150;
  v215 = apac::GetEscBits<6u,12u>(bs, 3u);
  if ( !v215 )
  {
    v2 = v211;
    v3 = v212;
    goto LABEL_67;
  }
  LODWORD(v213) = 0;
  v216 = off_42AFC8;
  while ( 1 )
  {
    v61 = bs->cache;
    v60 = bs->bitsLeft;
    v221 = nullptr;
    v62 = v60 - 8;
    bs->bitsLeft = v62;
    v63 = HIBYTE(v61);
    if ( v62 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v107 = bs->cache;
      v108 = -bs->bitsLeft;
      v62 = bs->bitsLeft + 32;
      bs->cur += 4;
      v63 |= v107 >> v62;
      v109 = v107 << v108;
      if ( !v62 )
        v109 = 0;
      bs->cache = v109;
      bs->bitsLeft = v62;
    }
    else
    {
      bs->cache = v61 << 8;
    }
    v64 = *((_DWORD **)this + 32);
    if ( *v64 )
    {
      v54 = (unsigned __int16)*v64;
      if ( *v64 == 0x10000 )
      {
        v53.n64_u32[0] = v64[1];
        v54 = (unsigned __int8)vaddv_s8(vcnt_s8(v53));
      }
    }
    else
    {
      v54 = v64[2];
    }
    if ( v63 >= v54 )
    {
      if ( v221 )
        (*(void (__fastcall **)(void *))(*(_QWORD *)v221 + 8LL))(v221);
LABEL_87:
      v27 = 1;
      v2 = v211;
      v3 = v212;
      goto LABEL_64;
    }
    v55 = bs->cache;
    v56 = v62 - 3;
    bs->bitsLeft = v56;
    LODWORD(v214) = v55 >> 29;
    if ( v56 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v110 = bs->cache;
      v111 = bs->bitsLeft;
      v112 = v110 >> (v111 + 32);
      v113 = v110 << -(char)v111;
      v114 = (unsigned int)v214 | v112;
      if ( v111 == -32 )
        v113 = 0;
      bs->cur += 4;
      bs->cache = v113;
      bs->bitsLeft = v111 + 32;
      LODWORD(v214) = v114;
    }
    else
    {
      bs->cache = 8 * v55;
    }
    v57 = (_WORD *)operator new(0x48u);
    *((_DWORD *)v57 + 2) = (_DWORD)v214;
    *((_QWORD *)v57 + 2) = 0;
    *((_QWORD *)v57 + 3) = 0;
    *((_QWORD *)v57 + 4) = 0;
    *((_DWORD *)v57 + 10) = -1;
    *(_QWORD *)v57 = v216;
    *((_QWORD *)v57 + 6) = 0;
    *((_QWORD *)v57 + 7) = 0;
    *((_QWORD *)v57 + 8) = 0;
    v58 = operator new(0xCu);
    *((_QWORD *)v57 + 6) = v58;
    v59 = v221;
    *((_QWORD *)v57 + 8) = v58 + 12;
    *(_QWORD *)v58 = 0;
    *(_DWORD *)(v58 + 8) = 0;
    *((_QWORD *)v57 + 7) = v58 + 12;
    v221 = v57;
    if ( v59 )
    {
      (*(void (__fastcall **)(void *))(*(_QWORD *)v59 + 8LL))(v59);
      v57 = v221;
    }
    v57[21] = v63;
    std::vector<std::unique_ptr<apac::IASCConfig>>::emplace_back<std::unique_ptr<apac::IASCConfig>>(
      (std_vector *)((char *)this + 200),
      &v221);
    if ( v221 )
      (*(void (__fastcall **)(void *))(*(_QWORD *)v221 + 8LL))(v221);
    if ( v215 == (_DWORD)v213 + 1 )
      break;
    LODWORD(v213) = (_DWORD)v213 + 1;
  }
  if ( v215 == 1 )
  {
    std::vector<char>::operator=(**((_QWORD **)this + 25) + 48LL, (char *)this + 256);
  }
  else
  {
    v167 = v215;
    std::vector<unsigned int>::vector(&v218, v215, &v221);
    std::vector<unsigned int>::vector(v220, v167, &v221);
    v173 = *((_BYTE **)this + 32);
    v172 = *((_BYTE **)this + 33);
    v221 = nullptr;
    v222 = nullptr;
    v223 = nullptr;
    v174 = v172 - v173;
    if ( v172 == v173 )
    {
      v175 = nullptr;
    }
    else
    {
      if ( v172 - v173 < 0 )
        std::__throw_bad_alloc();
      v175 = (char *)operator new(v172 - v173);
      v173 = *((_BYTE **)this + 32);
      v172 = *((_BYTE **)this + 33);
    }
    v176 = &v175[v174];
    v221 = v175;
    v222 = v175;
    v177 = v172 - v173;
    v223 = v176;
    if ( v172 != v173 )
      v175 = (char *)memmove(v175, v173, v177);
    v178 = (unsigned int *)v218;
    v222 = &v175[v177];
    v179 = 0;
    if ( v218 != v219 )
    {
      do
      {
        *v178 = v179++;
        v180 = v219;
      }
      while ( v178 != (unsigned int *)v219 );
      v181 = (unsigned int *)v218;
      if ( v219 != v218 )
      {
        v182 = v219 - (_BYTE *)v218;
        v183 = __clz((v219 - (_BYTE *)v218) >> 2);
        sub_97670(
          (unsigned int *)v218,
          (unsigned int *)v219,
          2LL * (63 - (int)v183),
          (__int64)this,
          v183,
          v168,
          v169,
          v170,
          v210);
        if ( v182 <= 64 )
        {
          sub_974B0(v181, (char *)v178, (__int64)this, v184, v185, v186, v187, v188, v210);
        }
        else
        {
          v189 = (char *)(v181 + 16);
          sub_974B0(v181, (char *)v181 + 64, (__int64)this, v184, v185, v186, v187, v188, v210);
          if ( v180 != (char *)(v181 + 16) )
          {
            do
            {
              v190 = *((_DWORD *)v189 - 1);
              v191 = (unsigned int *)(v189 - 4);
              v192 = *(_DWORD *)v189;
              v193 = 8LL * *(unsigned int *)v189;
              if ( *(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + v193) + 42LL) >= (unsigned int)*(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + 8LL * v190) + 42LL) )
              {
                v194 = v189;
              }
              else
              {
                do
                {
                  v191[1] = v190;
                  v194 = v191;
                  v195 = *--v191;
                  v190 = v195;
                }
                while ( *(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + v193) + 42LL) < (unsigned int)*(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + 8LL * v195) + 42LL) );
              }
              *v194 = v192;
              v189 += 4;
            }
            while ( v180 != v189 );
          }
        }
      }
    }
    v196 = 0;
    v197 = 0;
    v198 = (char *)v218;
    v199 = v220[0];
    do
    {
      v200 = *(unsigned int *)&v198[v197];
      v201 = *((_QWORD *)this + 25);
      v202 = &v199[v200];
      v203 = *(unsigned __int16 *)(*(_QWORD *)(v201 + 8 * v200) + 42LL);
      if ( v196 < (unsigned int)v213 )
      {
        v199[v200] = *(unsigned __int16 *)(*(_QWORD *)(v201 + 8LL * *(unsigned int *)&v198[v197 + 4]) + 42LL) - v203;
      }
      else
      {
        v204 = *((_DWORD **)this + 32);
        if ( *v204 )
        {
          v205 = (unsigned __int16)*v204;
          if ( *v204 == 0x10000 )
          {
            v171.n64_u32[0] = v204[1];
            v171.n64_u64[0] = vcnt_s8(v171).n64_u64[0];
            v205 = (unsigned __int8)vaddv_s8(v171);
          }
        }
        else
        {
          v205 = v204[2];
        }
        *v202 = v205 - v203;
      }
      v198 = (char *)v218;
      v199 = v220[0];
      i = (char *)v220[0];
      if ( !*((_DWORD *)v220[0] + *(unsigned int *)((char *)v218 + v197)) )
      {
        v27 = 1;
        goto LABEL_112;
      }
      v197 += 4;
      v49 = v196++ == (_DWORD)v213;
    }
    while ( !v49 );
    v115 = *((int **)this + 32);
    v116 = *v115;
    if ( (*v115 & 0xFFFF0000) != 0 )
    {
      if ( (*v115 & 0xFFFF0000) != 0x930000 )
      {
        if ( v116 == 0x10000 )
        {
          v171.n64_u32[0] = v115[1];
          v117 = (unsigned __int8)vaddv_s8(vcnt_s8(v171));
        }
        else
        {
          v117 = (unsigned __int16)*v115;
        }
        v217 = 0;
        std::vector<char>::resize(&v221, 20LL * v117 + 12, &v217);
        v118 = v221;
        *(_DWORD *)v221 = 0;
        v118[2] = v117;
        v27 = sub_96850((unsigned int **)this + 32, (__int64)v221 + 12, v119, v120, v121, v122, v123, v124, v210);
        if ( !v27 )
          goto LABEL_134;
LABEL_112:
        if ( v221 )
          operator delete(v221);
        if ( v220[0] )
          operator delete(v220[0]);
        v106 = v218;
        if ( !v218 )
        {
LABEL_117:
          v2 = v211;
          v3 = v212;
          goto LABEL_64;
        }
LABEL_107:
        operator delete(v106);
        v2 = v211;
        v3 = v212;
        goto LABEL_64;
      }
      v206 = (unsigned __int16)*v115;
      v217 = 0;
      std::vector<char>::resize(&v221, 20LL * (unsigned __int16)v116 + 12, &v217);
      v207 = v221;
      *(_DWORD *)v221 = 0;
      v207[2] = v206;
      v208 = (int *)((char *)v221 + 12);
      if ( v206 )
      {
        v209 = 0x40000;
        do
        {
          *v208 = v209;
          v208[1] = 0;
          ++v209;
          v208 += 5;
        }
        while ( v209 != v206 + 0x40000 );
      }
LABEL_134:
      i = (char *)v220[0];
    }
    v126 = 0;
    v127 = 4LL * (unsigned int)v213 + 4;
    v213 = *((void (__fastcall ***)(apac::chan::CoreDecoderConfig *__hidden))this + 25);
    v214 = (__int64 *)v127;
    do
    {
      v128 = *(_DWORD *)&i[v126];
      v129 = v213[v126 / 4];
      v217 = 0;
      v130 = 20LL * v128 + 12;
      v132 = *((_QWORD *)v129 + 6);
      v131 = *((char **)v129 + 7);
      if ( v130 > (unsigned __int64)&v131[-v132] )
      {
        std::vector<char>::_M_fill_insert((_DWORD)v129 + 48, v131, v130 - (_QWORD)&v131[-v132]);
        v132 = *((_QWORD *)v129 + 6);
      }
      else if ( v130 < (unsigned __int64)&v131[-v132] )
      {
        v133 = (char *)(v132 + v130);
        if ( v131 != v133 )
          *((_QWORD *)v129 + 7) = v133;
      }
      *(_DWORD *)(v132 + 8) = v128;
      v134 = 0;
      **((_DWORD **)v129 + 6) = 0;
      for ( i = (char *)v220[0]; *(_DWORD *)((char *)v220[0] + v126) > v134; i = (char *)v220[0] )
      {
        v135 = 20LL * v134 + 12;
        v136 = *((unsigned __int16 *)v129 + 21) + v134++;
        *(_DWORD *)(*((_QWORD *)v129 + 6) + v135) = *((_DWORD *)v221 + 5 * v136 + 3);
      }
      v126 += 4LL;
    }
    while ( v214 != (__int64 *)v126 );
    if ( v221 )
      operator delete(v221);
    if ( v220[0] )
      operator delete(v220[0]);
    if ( v218 )
      operator delete(v218);
  }
  v27 = GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig> *,std::vector<std::unique_ptr<apac::IASCConfig>>>>(
          *((void **)this + 25),
          *((void **)this + 26),
          (std_vector *)((char *)this + 224));
  if ( v27 )
    goto LABEL_117;
LABEL_150:
  v137 = *((_QWORD *)this + 3);
  if ( *(_BYTE *)(v137 + 249) )
  {
    v68 = *(_QWORD *)(v137 + 200);
    v138 = v137 + 200;
  }
  else
  {
    v138 = v137 + 176;
    v68 = *(_QWORD *)(v137 + 176);
  }
  v139 = *(_QWORD *)(v138 + 8);
  if ( v139 != v68 )
  {
    while ( 2 )
    {
      v140 = bs->cache;
      v141 = bs->bitsLeft - 3;
      bs->bitsLeft = v141;
      v142 = v140 >> 29;
      if ( (v141 & 0x80000000) == 0 )
      {
        v65 = 8 * v140;
        bs->cache = v65;
        goto LABEL_155;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v148 = bs->cache;
      v149 = bs->bitsLeft;
      v150 = bs->cur;
      v141 = v149 + 32;
      bs->bitsLeft = v149 + 32;
      v151 = v150 + 4;
      bs->cur = v151;
      v142 |= v148 >> (v149 + 32);
      if ( v149 == -32 )
      {
        bs->cache = 0;
        if ( v142 != 7 )
          goto LABEL_168;
        v143 = 0;
        bs->bitsLeft = -6;
LABEL_183:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v151);
        v160 = bs->cache;
        v161 = bs->bitsLeft;
        v162 = bs->cur;
        v141 = v161 + 32;
        bs->bitsLeft = v161 + 32;
        v151 = v162 + 4;
        bs->cur = v151;
        v143 |= v160 >> (v161 + 32);
        if ( v161 == -32 )
        {
          bs->cache = 0;
          if ( v143 != 63 )
          {
LABEL_168:
            v66 = 0;
            bs->bitsLeft = -2;
            goto LABEL_169;
          }
          bs->bitsLeft = -9;
LABEL_186:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v163 = bs->bitsLeft;
          v151 = bs->cur + 4;
          bs->cur = v151;
          v141 = v163 + 32;
          bs->bitsLeft = v163 + 32;
          if ( v163 == -32 )
          {
            v66 = 0;
            *(_QWORD *)&bs->cache = 0xFFFFFFFE00000000LL;
LABEL_169:
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, v151);
            v152 = bs->cache;
            v153 = bs->bitsLeft;
            v154 = bs->cur;
            bs->bitsLeft = v153 + 32;
            v155 = v154 + 4;
            bs->cur = v155;
            v66 |= v152 >> (v153 + 32);
            if ( v153 == -32 )
            {
              bs->cache = 0;
              if ( v66 == 3 )
              {
                v146 = 0;
                bs->bitsLeft = -8;
                goto LABEL_172;
              }
LABEL_60:
              v68 += 8;
              if ( v139 == v68 )
                goto LABEL_61;
              continue;
            }
            bs->cache = v152 << -(char)v153;
LABEL_59:
            if ( v66 != 3 )
              goto LABEL_60;
            v144 = bs->cache;
            v145 = bs->bitsLeft - 8;
            bs->bitsLeft = v145;
            v146 = HIBYTE(v144);
            if ( (v145 & 0x80000000) == 0 )
            {
              bs->cache = v144 << 8;
              goto LABEL_163;
            }
            v155 = bs->cur;
LABEL_172:
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, v155);
            v156 = bs->cache;
            v157 = bs->cur;
            v158 = bs->bitsLeft;
            v145 = v158 + 32;
            bs->bitsLeft = v158 + 32;
            bs->cur = v157 + 4;
            v146 |= v156 >> (v158 + 32);
            if ( v158 == -32 )
            {
              bs->cache = 0;
              if ( v146 != 255 )
                goto LABEL_60;
              bs->bitsLeft = -32;
              goto LABEL_175;
            }
            bs->cache = v156 << -(char)v158;
LABEL_163:
            if ( v146 != 255 )
              goto LABEL_60;
            v147 = v145 - 32;
            bs->bitsLeft = v147;
            if ( v147 < 0 )
            {
LABEL_175:
              TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
              v159 = bs->bitsLeft;
              bs->cur += 4;
              bs->bitsLeft = v159 + 32;
              if ( v159 != -32 )
              {
                bs->cache <<= -(char)v159;
                goto LABEL_60;
              }
            }
            bs->cache = 0;
            goto LABEL_60;
          }
          v65 = bs->cache << -(char)v163;
          bs->cache = v65;
LABEL_56:
          v66 = v65 >> 30;
LABEL_57:
          v67 = v141 - 2;
          bs->bitsLeft = v67;
          if ( v67 < 0 )
          {
            v151 = bs->cur;
            goto LABEL_169;
          }
          bs->cache = 4 * v65;
          goto LABEL_59;
        }
        v65 = v160 << -(char)v161;
        bs->cache = v65;
      }
      else
      {
        v65 = v148 << -(char)v149;
        bs->cache = v65;
LABEL_155:
        if ( v142 != 7 )
          goto LABEL_56;
        v141 -= 6;
        bs->bitsLeft = v141;
        v143 = v65 >> 26;
        if ( v141 < 0 )
        {
          v151 = bs->cur;
          goto LABEL_183;
        }
        v65 <<= 6;
        bs->cache = v65;
      }
      break;
    }
    if ( v143 != 63 )
      goto LABEL_56;
    v141 -= 9;
    bs->bitsLeft = v141;
    if ( v141 < 0 )
      goto LABEL_186;
    v65 <<= 9;
    bs->cache = v65;
    v66 = v65 >> 30;
    goto LABEL_57;
  }
LABEL_61:
  v27 = apac::AncillaryConfig::Deserialize(
          (apac__GlobalConfig *)((char *)this + 48),
          bs,
          *((_DWORD *)this + 9),
          0x400u,
          v30);
  if ( v27 )
    goto LABEL_117;
  v27 = apac::ExtensionConfig::Deserialize((apac__GlobalConfig *)((char *)this + 88), bs);
  if ( v27 )
    goto LABEL_117;
  *((_BYTE *)this + 248) = 1;
  v2 = v211;
  v3 = v212;
LABEL_64:
  if ( v224 != _stack_chk_guard )
  {
    v211 = v2;
    v212 = v3;
  }
  return v27;
}


/* ------------------------------------------------------------------------ */
/* 0x00106e00  size 76 bytes  19 insns
 *   apac::GlobalConfig::ComponentConfigs::configs()
 *   [_ZN4apac12GlobalConfig16ComponentConfigs7configsEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::GlobalConfig::ComponentConfigs::configs(apac__GlobalConfig__ComponentConfigs *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return (**(__int64 (__fastcall ***)(apac__GlobalConfig__ComponentConfigs *))this)(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00106e50  size 64 bytes  16 insns
 *   apac::GlobalConfig::ASCConfigData::~ASCConfigData()
 *   apac::GlobalConfig::ASCConfigData::~ASCConfigData()
 *   [_ZN4apac12GlobalConfig13ASCConfigDataD2Ev]
 *   [_ZN4apac12GlobalConfig13ASCConfigDataD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4apac12GlobalConfig13ASCConfigDataD1Ev'
void __fastcall apac::GlobalConfig::ASCConfigData::~ASCConfigData(apac__GlobalConfig__ASCConfigData *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
}


/* ------------------------------------------------------------------------ */
/* 0x00106e90  size 64 bytes  16 insns
 *   apac::GlobalConfig::CodecConfigData::~CodecConfigData()
 *   apac::GlobalConfig::CodecConfigData::~CodecConfigData()
 *   [_ZN4apac12GlobalConfig15CodecConfigDataD1Ev]
 *   [_ZN4apac12GlobalConfig15CodecConfigDataD2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4apac12GlobalConfig15CodecConfigDataD2Ev'
void __fastcall apac::GlobalConfig::CodecConfigData::~CodecConfigData(apac__GlobalConfig__CodecConfigData *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
}


/* ------------------------------------------------------------------------ */
/* 0x00106ed0  size 64 bytes  16 insns
 *   apac::GlobalConfig::CodecConfigData::~CodecConfigData()
 *   [_ZN4apac12GlobalConfig15CodecConfigDataD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall apac::GlobalConfig::CodecConfigData::~CodecConfigData(apac__GlobalConfig__CodecConfigData *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00106f10  size 64 bytes  16 insns
 *   apac::GlobalConfig::ASCConfigData::~ASCConfigData()
 *   [_ZN4apac12GlobalConfig13ASCConfigDataD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall apac::GlobalConfig::ASCConfigData::~ASCConfigData(apac__GlobalConfig__ASCConfigData *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


