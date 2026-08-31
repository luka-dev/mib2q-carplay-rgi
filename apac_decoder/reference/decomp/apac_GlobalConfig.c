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
  unsigned int v36; // w28
  _QWORD *v37; // x2
  unsigned int v38; // w1
  __int16 *ProfileConfig; // x0
  int *v40; // x1
  int *v41; // x4
  int v42; // w0
  __int64 v43; // [xsp+40h] [xbp+40h]
  __int64 v44; // [xsp+48h] [xbp+48h]
  __int64 v45; // [xsp+50h] [xbp+50h]
  __int64 v46; // [xsp+58h] [xbp+58h]
  _QWORD *v47; // [xsp+68h] [xbp+68h]
  void *v48; // [xsp+70h] [xbp+70h] BYREF
  char *v49; // [xsp+78h] [xbp+78h]
  char *v50; // [xsp+80h] [xbp+80h]
  __int64 v51; // [xsp+88h] [xbp+88h]
  __int64 v52; // [xsp+90h] [xbp+90h]
  v6 = GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig> *,std::vector<std::unique_ptr<apac::IASCConfig>>>>(
         *((void **)this + 22),
         *((void **)this + 23),
         (std_vector *)((char *)this + 224));
  if ( v6 )
    goto LABEL_16;
  v43 = v1;
  v44 = v2;
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
      LOBYTE(v48) = 0;
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
      v47 = *((_QWORD **)this + 29);
      v28 = *((_QWORD *)this + 32) + 12LL;
      if ( v27 != v47 )
      {
        v45 = v3;
        v46 = v4;
        while ( 1 )
        {
          v29 = (*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v27 + 24LL))(*v27);
          if ( *(_DWORD *)(*v27 + 8LL) )
          {
            if ( v29 )
            {
              v37 = (_QWORD *)v28;
              v38 = 0x10000;
              do
              {
                *v37 = v38++;
                v37 = (_QWORD *)((char *)v37 + 20);
              }
              while ( v38 != v29 + 0x10000 );
              v28 += 20LL * (v29 - 1) + 20;
            }
          }
          else
          {
            v30 = (_DWORD *)(*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v27 + 32LL))(*v27);
            v31 = v30[2];
            v48 = nullptr;
            v49 = nullptr;
            v50 = nullptr;
            v32 = 20LL * v31 + 12;
            v33 = (char *)operator new(v32);
            v34 = &v33[v32];
            v35 = v33;
            v48 = v33;
            v50 = &v33[v32];
            memset(v33, 0, v32);
            v49 = v34;
            memcpy(v35, v30, v32);
            v36 = sub_96850(&v48, v28);
            if ( v48 )
              operator delete(v48);
            if ( v36 )
            {
              v6 = v36;
              v1 = v43;
              v2 = v44;
              v3 = v45;
              v4 = v46;
              goto LABEL_16;
            }
            v28 += 20LL * v29;
          }
          if ( v47 == ++v27 )
          {
            v1 = v43;
            v2 = v44;
            v3 = v45;
            v4 = v46;
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
      v1 = v43;
      v2 = v44;
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
    v1 = v43;
    v2 = v44;
  }
  else
  {
    v40 = *((int **)ProfileConfig + 3);
    v41 = *((int **)ProfileConfig + 4);
    for ( i = *((int **)this + 32); v41 != v40; ++v40 )
    {
      v42 = *v40;
      if ( (*v40 & 0xFFFF0000) == 0x930000 )
      {
        v21 = v9 | v42;
      }
      else if ( v9 == (unsigned __int16)v42 )
      {
        v21 = *v40;
        break;
      }
    }
LABEL_15:
    *i = v21;
    v1 = v43;
    v2 = v44;
  }
LABEL_16:
  if ( v51 != _stack_chk_guard )
  {
    v43 = v1;
    v44 = v2;
    v45 = v3;
    v46 = v4;
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
  char *i; // x0
  unsigned __int64 v120; // x24
  __int64 v121; // x1
  unsigned int v122; // w27
  void (__fastcall *v123)(apac::chan::CoreDecoderConfig *__hidden); // x20
  unsigned __int64 v124; // x0
  char *v125; // x1
  __int64 v126; // x3
  char *v127; // x0
  unsigned int v128; // w1
  __int64 v129; // x2
  unsigned int v130; // w0
  __int64 v131; // x0
  __int64 v132; // x1
  __int64 v133; // x20
  unsigned int v134; // w1
  int v135; // w0
  unsigned int v136; // w21
  unsigned int v137; // w21
  unsigned int v138; // w1
  int v139; // w0
  unsigned int v140; // w21
  int v141; // w0
  unsigned int v142; // w2
  int v143; // w5
  const unsigned __int8 *v144; // x1
  const unsigned __int8 *v145; // x1
  unsigned int v146; // w3
  int v147; // w5
  const unsigned __int8 *v148; // x1
  const unsigned __int8 *v149; // x1
  unsigned int v150; // w5
  const unsigned __int8 *v151; // x1
  int v152; // w3
  int v153; // w0
  unsigned int v154; // w2
  int v155; // w5
  const unsigned __int8 *v156; // x1
  int v157; // w2
  unsigned int v158; // w0
  int v159; // w1
  unsigned int v160; // w0
  __int64 v161; // x21
  int8x8_t v162; // d0
  _BYTE *v163; // x0
  _BYTE *v164; // x1
  signed __int64 v165; // x21
  char *v166; // x3
  char *v167; // x2
  size_t v168; // x21
  unsigned int *v169; // x23
  unsigned int v170; // w0
  unsigned int *v171; // x21
  unsigned int *v172; // x24
  __int64 v173; // x27
  unsigned int *v174; // x23
  unsigned int v175; // w2
  unsigned int *v176; // x0
  unsigned int v177; // w1
  __int64 v178; // x6
  unsigned int *v179; // x5
  unsigned int v180; // t1
  unsigned int v181; // w1
  __int64 v182; // x4
  char *v183; // x3
  _DWORD *v184; // x2
  __int64 v185; // x0
  __int64 v186; // x5
  _DWORD *v187; // x7
  int v188; // w6
  _DWORD *v189; // x2
  int v190; // w0
  int v191; // w21
  _DWORD *v192; // x0
  int *v193; // x2
  int v194; // w0
  __int64 v195; // [xsp+50h] [xbp+50h]
  __int64 v196; // [xsp+58h] [xbp+58h]
  void (__fastcall **v197)(apac::chan::CoreDecoderConfig *__hidden); // [xsp+60h] [xbp+60h]
  __int64 *v198; // [xsp+68h] [xbp+68h]
  unsigned int v199; // [xsp+74h] [xbp+74h]
  void (__fastcall **v200)(apac::BaseASCConfig *__hidden); // [xsp+78h] [xbp+78h]
  char v201; // [xsp+8Fh] [xbp+8Fh] BYREF
  void *v202; // [xsp+90h] [xbp+90h] BYREF
  unsigned int *v203; // [xsp+98h] [xbp+98h]
  void *v204[3]; // [xsp+A8h] [xbp+A8h] BYREF
  void *v205; // [xsp+C0h] [xbp+C0h] BYREF
  char *v206; // [xsp+C8h] [xbp+C8h]
  char *v207; // [xsp+D0h] [xbp+D0h]
  __int64 v208; // [xsp+D8h] [xbp+D8h]
  __int64 v209; // [xsp+E0h] [xbp+E0h]
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
  v195 = v2;
  v196 = v3;
  Esc = apac::GetEscBits<6u,12u>(bs, 3u);
  v205 = nullptr;
  v206 = nullptr;
  v207 = nullptr;
  if ( !Esc )
    goto LABEL_87;
  v29 = 0;
  v30 = 0;
  v197 = off_42A990;
  v198 = &`vtable for'apac::lbr::DecoderConfig;
  do
  {
    v31 = bs->cache;
    v32 = bs->bitsLeft;
    v204[0] = nullptr;
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
      if ( v204[0] )
        (*(void (__fastcall **)(void *))(*(_QWORD *)v204[0] + 8LL))(v204[0]);
      if ( v205 )
        operator delete(v205);
      v27 = 4;
      v2 = v195;
      v3 = v196;
      goto LABEL_64;
    }
    v37 = (_DWORD *)operator new(0x68u);
    v38 = v197;
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
    v41 = v198 + 2;
    v42 = v39;
    *((_QWORD *)v37 + 9) = v39;
    v43 = v204[0];
    *((_QWORD *)v37 + 11) = v40;
    *(_QWORD *)v37 = v41;
    *(_QWORD *)v42 = 0;
    *(_DWORD *)(v42 + 8) = 0;
    *((_QWORD *)v37 + 10) = v40;
    *((_BYTE *)v37 + 96) = 0;
    v204[0] = v37;
    v44 = apac::lbr::DecoderConfig::Deserialize;
    if ( v43 )
    {
      (*(void (__fastcall **)(void *))(*(_QWORD *)v43 + 8LL))(v43);
      v37 = v204[0];
      v44 = *(__int64 (__fastcall **)(apac__lbr__DecoderConfig *, TBitstreamReader_u32 *))(*(_QWORD *)v204[0] + 16LL);
    }
    v27 = v44((apac__lbr__DecoderConfig *)v37, bs);
    v45 = v204[0];
    if ( v27 )
      goto LABEL_104;
    v46 = (*(__int64 (__fastcall **)(void *))(*(_QWORD *)v204[0] + 24LL))(v204[0]);
    v47 = v46;
    if ( !v46 || (v30 += v46, v30 <= v33) )
    {
      v27 = 1;
      v45 = v204[0];
LABEL_104:
      if ( v45 )
        (*(void (__fastcall **)(void *))(*(_QWORD *)v45 + 8LL))(v45);
      v106 = v205;
      if ( !v205 )
        goto LABEL_117;
      goto LABEL_107;
    }
    v48 = apac::ConvertToAudioDataType(0);
    v49 = !apac::ASCChannelMap::AddMap((apac__ASCChannelMap *)&v205, v48, v33, v47 + v33);
    v45 = v204[0];
    if ( v49 )
    {
      v27 = 1;
      goto LABEL_104;
    }
    *((_WORD *)v204[0] + 21) = v33;
    std::vector<std::unique_ptr<apac::IASCConfig>>::emplace_back<std::unique_ptr<apac::IASCConfig>>(
      (std_vector *)((char *)this + 176),
      v204);
    if ( v204[0] )
      (*(void (__fastcall **)(void *))(*(_QWORD *)v204[0] + 8LL))(v204[0]);
    ++v29;
  }
  while ( Esc != v29 );
  if ( v205 )
    operator delete(v205);
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
    v158 = bs->cache;
    v159 = bs->bitsLeft;
    LOBYTE(v52) = v52 | (v158 >> (v159 + 32));
    v160 = v158 << -(char)v159;
    if ( v159 == -32 )
      v160 = 0;
    bs->cur += 4;
    bs->cache = v160;
    bs->bitsLeft = v159 + 32;
  }
  else
  {
    bs->cache = 2 * v50;
  }
  *((_BYTE *)this + 249) = (unsigned __int8)v52 != 0;
  if ( !(_BYTE)v52 )
    goto LABEL_150;
  v199 = apac::GetEscBits<6u,12u>(bs, 3u);
  if ( !v199 )
  {
    v2 = v195;
    v3 = v196;
    goto LABEL_67;
  }
  LODWORD(v197) = 0;
  v200 = off_42AFC8;
  while ( 1 )
  {
    v61 = bs->cache;
    v60 = bs->bitsLeft;
    v205 = nullptr;
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
      if ( v205 )
        (*(void (__fastcall **)(void *))(*(_QWORD *)v205 + 8LL))(v205);
LABEL_87:
      v27 = 1;
      v2 = v195;
      v3 = v196;
      goto LABEL_64;
    }
    v55 = bs->cache;
    v56 = v62 - 3;
    bs->bitsLeft = v56;
    LODWORD(v198) = v55 >> 29;
    if ( v56 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v110 = bs->cache;
      v111 = bs->bitsLeft;
      v112 = v110 >> (v111 + 32);
      v113 = v110 << -(char)v111;
      v114 = (unsigned int)v198 | v112;
      if ( v111 == -32 )
        v113 = 0;
      bs->cur += 4;
      bs->cache = v113;
      bs->bitsLeft = v111 + 32;
      LODWORD(v198) = v114;
    }
    else
    {
      bs->cache = 8 * v55;
    }
    v57 = (_WORD *)operator new(0x48u);
    *((_DWORD *)v57 + 2) = (_DWORD)v198;
    *((_QWORD *)v57 + 2) = 0;
    *((_QWORD *)v57 + 3) = 0;
    *((_QWORD *)v57 + 4) = 0;
    *((_DWORD *)v57 + 10) = -1;
    *(_QWORD *)v57 = v200;
    *((_QWORD *)v57 + 6) = 0;
    *((_QWORD *)v57 + 7) = 0;
    *((_QWORD *)v57 + 8) = 0;
    v58 = operator new(0xCu);
    *((_QWORD *)v57 + 6) = v58;
    v59 = v205;
    *((_QWORD *)v57 + 8) = v58 + 12;
    *(_QWORD *)v58 = 0;
    *(_DWORD *)(v58 + 8) = 0;
    *((_QWORD *)v57 + 7) = v58 + 12;
    v205 = v57;
    if ( v59 )
    {
      (*(void (__fastcall **)(void *))(*(_QWORD *)v59 + 8LL))(v59);
      v57 = v205;
    }
    v57[21] = v63;
    std::vector<std::unique_ptr<apac::IASCConfig>>::emplace_back<std::unique_ptr<apac::IASCConfig>>(
      (std_vector *)((char *)this + 200),
      &v205);
    if ( v205 )
      (*(void (__fastcall **)(void *))(*(_QWORD *)v205 + 8LL))(v205);
    if ( v199 == (_DWORD)v197 + 1 )
      break;
    LODWORD(v197) = (_DWORD)v197 + 1;
  }
  if ( v199 == 1 )
  {
    std::vector<char>::operator=(**((_QWORD **)this + 25) + 48LL, (char *)this + 256);
  }
  else
  {
    v161 = v199;
    std::vector<unsigned int>::vector(&v202, v199, &v205);
    std::vector<unsigned int>::vector(v204, v161, &v205);
    v164 = *((_BYTE **)this + 32);
    v163 = *((_BYTE **)this + 33);
    v205 = nullptr;
    v206 = nullptr;
    v207 = nullptr;
    v165 = v163 - v164;
    if ( v163 == v164 )
    {
      v166 = nullptr;
    }
    else
    {
      if ( v163 - v164 < 0 )
        std::__throw_bad_alloc();
      v166 = (char *)operator new(v163 - v164);
      v164 = *((_BYTE **)this + 32);
      v163 = *((_BYTE **)this + 33);
    }
    v167 = &v166[v165];
    v205 = v166;
    v206 = v166;
    v168 = v163 - v164;
    v207 = v167;
    if ( v163 != v164 )
      v166 = (char *)memmove(v166, v164, v168);
    v169 = (unsigned int *)v202;
    v206 = &v166[v168];
    v170 = 0;
    if ( v202 != v203 )
    {
      do
      {
        *v169 = v170++;
        v171 = v203;
      }
      while ( v169 != v203 );
      v172 = (unsigned int *)v202;
      if ( v203 != v202 )
      {
        v173 = (char *)v203 - (_BYTE *)v202;
        sub_97670(v202, v203, 2LL * (int)(63 - __clz(((char *)v203 - (_BYTE *)v202) >> 2)), this);
        if ( v173 <= 64 )
        {
          sub_974B0(v172);
        }
        else
        {
          v174 = v172 + 16;
          sub_974B0(v172);
          if ( v171 != v172 + 16 )
          {
            do
            {
              v175 = *(v174 - 1);
              v176 = v174 - 1;
              v177 = *v174;
              v178 = 8LL * *v174;
              if ( *(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + v178) + 42LL) >= (unsigned int)*(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + 8LL * v175) + 42LL) )
              {
                v179 = v174;
              }
              else
              {
                do
                {
                  v176[1] = v175;
                  v179 = v176;
                  v180 = *--v176;
                  v175 = v180;
                }
                while ( *(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + v178) + 42LL) < (unsigned int)*(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 25) + 8LL * v180) + 42LL) );
              }
              *v179 = v177;
              ++v174;
            }
            while ( v171 != v174 );
          }
        }
      }
    }
    v181 = 0;
    v182 = 0;
    v183 = (char *)v202;
    v184 = v204[0];
    do
    {
      v185 = *(unsigned int *)&v183[v182];
      v186 = *((_QWORD *)this + 25);
      v187 = &v184[v185];
      v188 = *(unsigned __int16 *)(*(_QWORD *)(v186 + 8 * v185) + 42LL);
      if ( v181 < (unsigned int)v197 )
      {
        v184[v185] = *(unsigned __int16 *)(*(_QWORD *)(v186 + 8LL * *(unsigned int *)&v183[v182 + 4]) + 42LL) - v188;
      }
      else
      {
        v189 = *((_DWORD **)this + 32);
        if ( *v189 )
        {
          v190 = (unsigned __int16)*v189;
          if ( *v189 == 0x10000 )
          {
            v162.n64_u32[0] = v189[1];
            v162.n64_u64[0] = vcnt_s8(v162).n64_u64[0];
            v190 = (unsigned __int8)vaddv_s8(v162);
          }
        }
        else
        {
          v190 = v189[2];
        }
        *v187 = v190 - v188;
      }
      v183 = (char *)v202;
      v184 = v204[0];
      i = (char *)v204[0];
      if ( !*((_DWORD *)v204[0] + *(unsigned int *)((char *)v202 + v182)) )
      {
        v27 = 1;
        goto LABEL_112;
      }
      v182 += 4;
      v49 = v181++ == (_DWORD)v197;
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
          v162.n64_u32[0] = v115[1];
          v117 = (unsigned __int8)vaddv_s8(vcnt_s8(v162));
        }
        else
        {
          v117 = (unsigned __int16)*v115;
        }
        v201 = 0;
        std::vector<char>::resize(&v205, 20LL * v117 + 12, &v201);
        v118 = v205;
        *(_DWORD *)v205 = 0;
        v118[2] = v117;
        v27 = sub_96850((char *)this + 256, (char *)v205 + 12);
        if ( !v27 )
          goto LABEL_134;
LABEL_112:
        if ( v205 )
          operator delete(v205);
        if ( v204[0] )
          operator delete(v204[0]);
        v106 = v202;
        if ( !v202 )
        {
LABEL_117:
          v2 = v195;
          v3 = v196;
          goto LABEL_64;
        }
LABEL_107:
        operator delete(v106);
        v2 = v195;
        v3 = v196;
        goto LABEL_64;
      }
      v191 = (unsigned __int16)*v115;
      v201 = 0;
      std::vector<char>::resize(&v205, 20LL * (unsigned __int16)v116 + 12, &v201);
      v192 = v205;
      *(_DWORD *)v205 = 0;
      v192[2] = v191;
      v193 = (int *)((char *)v205 + 12);
      if ( v191 )
      {
        v194 = 0x40000;
        do
        {
          *v193 = v194;
          v193[1] = 0;
          ++v194;
          v193 += 5;
        }
        while ( v194 != v191 + 0x40000 );
      }
LABEL_134:
      i = (char *)v204[0];
    }
    v120 = 0;
    v121 = 4LL * (unsigned int)v197 + 4;
    v197 = *((void (__fastcall ***)(apac::chan::CoreDecoderConfig *__hidden))this + 25);
    v198 = (__int64 *)v121;
    do
    {
      v122 = *(_DWORD *)&i[v120];
      v123 = v197[v120 / 4];
      v201 = 0;
      v124 = 20LL * v122 + 12;
      v126 = *((_QWORD *)v123 + 6);
      v125 = *((char **)v123 + 7);
      if ( v124 > (unsigned __int64)&v125[-v126] )
      {
        std::vector<char>::_M_fill_insert((_DWORD)v123 + 48, v125, v124 - (_QWORD)&v125[-v126]);
        v126 = *((_QWORD *)v123 + 6);
      }
      else if ( v124 < (unsigned __int64)&v125[-v126] )
      {
        v127 = (char *)(v126 + v124);
        if ( v125 != v127 )
          *((_QWORD *)v123 + 7) = v127;
      }
      *(_DWORD *)(v126 + 8) = v122;
      v128 = 0;
      **((_DWORD **)v123 + 6) = 0;
      for ( i = (char *)v204[0]; *(_DWORD *)((char *)v204[0] + v120) > v128; i = (char *)v204[0] )
      {
        v129 = 20LL * v128 + 12;
        v130 = *((unsigned __int16 *)v123 + 21) + v128++;
        *(_DWORD *)(*((_QWORD *)v123 + 6) + v129) = *((_DWORD *)v205 + 5 * v130 + 3);
      }
      v120 += 4LL;
    }
    while ( v198 != (__int64 *)v120 );
    if ( v205 )
      operator delete(v205);
    if ( v204[0] )
      operator delete(v204[0]);
    if ( v202 )
      operator delete(v202);
  }
  v27 = GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig> *,std::vector<std::unique_ptr<apac::IASCConfig>>>>(
          *((void **)this + 25),
          *((void **)this + 26),
          (std_vector *)((char *)this + 224));
  if ( v27 )
    goto LABEL_117;
LABEL_150:
  v131 = *((_QWORD *)this + 3);
  if ( *(_BYTE *)(v131 + 249) )
  {
    v68 = *(_QWORD *)(v131 + 200);
    v132 = v131 + 200;
  }
  else
  {
    v132 = v131 + 176;
    v68 = *(_QWORD *)(v131 + 176);
  }
  v133 = *(_QWORD *)(v132 + 8);
  if ( v133 != v68 )
  {
    while ( 2 )
    {
      v134 = bs->cache;
      v135 = bs->bitsLeft - 3;
      bs->bitsLeft = v135;
      v136 = v134 >> 29;
      if ( (v135 & 0x80000000) == 0 )
      {
        v65 = 8 * v134;
        bs->cache = v65;
        goto LABEL_155;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v142 = bs->cache;
      v143 = bs->bitsLeft;
      v144 = bs->cur;
      v135 = v143 + 32;
      bs->bitsLeft = v143 + 32;
      v145 = v144 + 4;
      bs->cur = v145;
      v136 |= v142 >> (v143 + 32);
      if ( v143 == -32 )
      {
        bs->cache = 0;
        if ( v136 != 7 )
          goto LABEL_168;
        v137 = 0;
        bs->bitsLeft = -6;
LABEL_183:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v145);
        v154 = bs->cache;
        v155 = bs->bitsLeft;
        v156 = bs->cur;
        v135 = v155 + 32;
        bs->bitsLeft = v155 + 32;
        v145 = v156 + 4;
        bs->cur = v145;
        v137 |= v154 >> (v155 + 32);
        if ( v155 == -32 )
        {
          bs->cache = 0;
          if ( v137 != 63 )
          {
LABEL_168:
            v66 = 0;
            bs->bitsLeft = -2;
            goto LABEL_169;
          }
          bs->bitsLeft = -9;
LABEL_186:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v157 = bs->bitsLeft;
          v145 = bs->cur + 4;
          bs->cur = v145;
          v135 = v157 + 32;
          bs->bitsLeft = v157 + 32;
          if ( v157 == -32 )
          {
            v66 = 0;
            *(_QWORD *)&bs->cache = 0xFFFFFFFE00000000LL;
LABEL_169:
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, v145);
            v146 = bs->cache;
            v147 = bs->bitsLeft;
            v148 = bs->cur;
            bs->bitsLeft = v147 + 32;
            v149 = v148 + 4;
            bs->cur = v149;
            v66 |= v146 >> (v147 + 32);
            if ( v147 == -32 )
            {
              bs->cache = 0;
              if ( v66 == 3 )
              {
                v140 = 0;
                bs->bitsLeft = -8;
                goto LABEL_172;
              }
LABEL_60:
              v68 += 8;
              if ( v133 == v68 )
                goto LABEL_61;
              continue;
            }
            bs->cache = v146 << -(char)v147;
LABEL_59:
            if ( v66 != 3 )
              goto LABEL_60;
            v138 = bs->cache;
            v139 = bs->bitsLeft - 8;
            bs->bitsLeft = v139;
            v140 = HIBYTE(v138);
            if ( (v139 & 0x80000000) == 0 )
            {
              bs->cache = v138 << 8;
              goto LABEL_163;
            }
            v149 = bs->cur;
LABEL_172:
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, v149);
            v150 = bs->cache;
            v151 = bs->cur;
            v152 = bs->bitsLeft;
            v139 = v152 + 32;
            bs->bitsLeft = v152 + 32;
            bs->cur = v151 + 4;
            v140 |= v150 >> (v152 + 32);
            if ( v152 == -32 )
            {
              bs->cache = 0;
              if ( v140 != 255 )
                goto LABEL_60;
              bs->bitsLeft = -32;
              goto LABEL_175;
            }
            bs->cache = v150 << -(char)v152;
LABEL_163:
            if ( v140 != 255 )
              goto LABEL_60;
            v141 = v139 - 32;
            bs->bitsLeft = v141;
            if ( v141 < 0 )
            {
LABEL_175:
              TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
              v153 = bs->bitsLeft;
              bs->cur += 4;
              bs->bitsLeft = v153 + 32;
              if ( v153 != -32 )
              {
                bs->cache <<= -(char)v153;
                goto LABEL_60;
              }
            }
            bs->cache = 0;
            goto LABEL_60;
          }
          v65 = bs->cache << -(char)v157;
          bs->cache = v65;
LABEL_56:
          v66 = v65 >> 30;
LABEL_57:
          v67 = v135 - 2;
          bs->bitsLeft = v67;
          if ( v67 < 0 )
          {
            v145 = bs->cur;
            goto LABEL_169;
          }
          bs->cache = 4 * v65;
          goto LABEL_59;
        }
        v65 = v154 << -(char)v155;
        bs->cache = v65;
      }
      else
      {
        v65 = v142 << -(char)v143;
        bs->cache = v65;
LABEL_155:
        if ( v136 != 7 )
          goto LABEL_56;
        v135 -= 6;
        bs->bitsLeft = v135;
        v137 = v65 >> 26;
        if ( v135 < 0 )
        {
          v145 = bs->cur;
          goto LABEL_183;
        }
        v65 <<= 6;
        bs->cache = v65;
      }
      break;
    }
    if ( v137 != 63 )
      goto LABEL_56;
    v135 -= 9;
    bs->bitsLeft = v135;
    if ( v135 < 0 )
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
  v2 = v195;
  v3 = v196;
LABEL_64:
  if ( v208 != _stack_chk_guard )
  {
    v195 = v2;
    v196 = v3;
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


