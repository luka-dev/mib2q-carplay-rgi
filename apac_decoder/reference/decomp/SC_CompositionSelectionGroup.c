/* ============================================================================
 * SC_CompositionSelectionGroup
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 6196 bytes of .text.
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
/* 0x000a11c0    212  SC_CompositionSelectionGroup::~SC_CompositionSelectionGroup() */
/* 0x000a4640    592  SC_CompositionSelectionGroup::Presets::operator==(SC_CompositionSelectionGroup::Presets const&) const */
/* 0x000a4890    352  SC_CompositionSelectionGroup::operator==(SC_CompositionSelectionGroup const&) const */
/* 0x000a49f0    332  SC_CompositionSelectionGroup::CheckConformance(unsigned short, unsigned short) const */
/* 0x000a6ae0   4708  SC_CompositionSelectionGroup::Deserialize(TBitstreamReader<unsigned int>&, unsigned short) */

/* ------------------------------------------------------------------------ */
/* 0x000a11c0  size 212 bytes  53 insns
 *   SC_CompositionSelectionGroup::~SC_CompositionSelectionGroup()
 *   SC_CompositionSelectionGroup::~SC_CompositionSelectionGroup()
 *   [_ZN28SC_CompositionSelectionGroupD2Ev]
 *   [_ZN28SC_CompositionSelectionGroupD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN28SC_CompositionSelectionGroupD1Ev'
void __fastcall SC_CompositionSelectionGroup::~SC_CompositionSelectionGroup(SC_CompositionSelectionGroup *this)
{
  void *v2; // x0
  void *v3; // x0
  void *v4; // x0
  void *v5; // x0
  void *v6; // x0
  void *v7; // x0
  void *v8; // x0
  void *v9; // x0
  void *v10; // x0
  __int64 v11; // [xsp+30h] [xbp+30h]
  v2 = *((void **)this + 29);
  if ( v2 )
    operator delete(v2);
  v3 = *((void **)this + 26);
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 22);
  if ( v4 )
    operator delete(v4);
  v5 = *((void **)this + 19);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 15);
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 11);
  if ( v7 )
    operator delete(v7);
  v8 = *((void **)this + 7);
  if ( v8 )
    operator delete(v8);
  v9 = *((void **)this + 4);
  if ( v9 )
    operator delete(v9);
  v10 = *((void **)this + 1);
  if ( v10 )
    operator delete(v10);
}


/* ------------------------------------------------------------------------ */
/* 0x000a4640  size 592 bytes  148 insns
 *   SC_CompositionSelectionGroup::Presets::operator==(SC_CompositionSelectionGroup::Presets const&) const
 *   [_ZNK28SC_CompositionSelectionGroup7PresetseqERKS0_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_CompositionSelectionGroup::Presets::operator==(
        SC_CompositionSelectionGroup__Presets *this,
        const SC_CompositionSelectionGroup__Presets *a)
{
  int v3; // w4
  __int64 result; // x0
  unsigned int v5; // w2
  __int64 v6; // [xsp+20h] [xbp+20h]
  v3 = *(unsigned __int8 *)this;
  result = 0;
  if ( *(unsigned __int8 *)a == v3
    && *((unsigned __int8 *)a + 1) == *((unsigned __int8 *)this + 1)
    && (!*((_BYTE *)a + 1)
     || *((unsigned __int8 *)a + 2) == *((unsigned __int8 *)this + 2)
     && (!*((_BYTE *)a + 2)
      || *((unsigned __int16 *)this + 2) == *((unsigned __int16 *)a + 2)
      && *((unsigned __int16 *)this + 3) == *((unsigned __int16 *)a + 3))) )
  {
    result = 0;
    if ( *((unsigned __int8 *)a + 8) == *((unsigned __int8 *)this + 8)
      && (!*((_BYTE *)a + 8)
       || *((unsigned __int16 *)this + 5) == *((unsigned __int16 *)a + 5)
       && *((unsigned __int16 *)this + 6) == *((unsigned __int16 *)a + 6)) )
    {
      result = 0;
      if ( *((unsigned __int8 *)a + 14) == *((unsigned __int8 *)this + 14)
        && (!*((_BYTE *)a + 14)
         || *((unsigned __int16 *)this + 8) == *((unsigned __int16 *)a + 8)
         && *((unsigned __int16 *)this + 9) == *((unsigned __int16 *)a + 9)) )
      {
        result = 0;
        if ( *((unsigned __int8 *)a + 20) == *((unsigned __int8 *)this + 20)
          && (!*((_BYTE *)a + 20)
           || *((unsigned __int16 *)this + 11) == *((unsigned __int16 *)a + 11)
           && *((unsigned __int16 *)this + 12) == *((unsigned __int16 *)a + 12)) )
        {
          result = 0;
          if ( *((unsigned __int8 *)this + 26) == *((unsigned __int8 *)a + 26) )
          {
            v5 = *((unsigned __int8 *)a + 27);
            if ( v5 == *((unsigned __int8 *)this + 27) )
            {
              result = v3 | v5;
              if ( (_DWORD)result )
              {
                if ( *((unsigned __int16 *)this + 14) == *((unsigned __int16 *)a + 14)
                  && *((unsigned __int8 *)a + 30) == *((unsigned __int8 *)this + 30)
                  && (!*((_BYTE *)a + 30) || *((unsigned __int16 *)this + 16) == *((unsigned __int16 *)a + 16))
                  && *((unsigned __int8 *)a + 34) == *((unsigned __int8 *)this + 34)
                  && (!*((_BYTE *)a + 34) || *((unsigned __int16 *)this + 18) == *((unsigned __int16 *)a + 18))
                  && *((unsigned __int8 *)a + 38) == *((unsigned __int8 *)this + 38)
                  && (!*((_BYTE *)a + 38) || *((unsigned __int16 *)this + 20) == *((unsigned __int16 *)a + 20))
                  && *((unsigned __int8 *)a + 42) == *((unsigned __int8 *)this + 42)
                  && (!*((_BYTE *)a + 42) || *((unsigned __int16 *)this + 22) == *((unsigned __int16 *)a + 22))
                  && *((unsigned __int8 *)a + 46) == *((unsigned __int8 *)this + 46) )
                {
                  if ( *((_BYTE *)a + 46) )
                    return *((unsigned __int8 *)this + 47) == *((unsigned __int8 *)a + 47);
                }
                else
                {
                  return 0;
                }
              }
              else
              {
                return 1;
              }
            }
          }
        }
      }
    }
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a4890  size 352 bytes  88 insns
 *   SC_CompositionSelectionGroup::operator==(SC_CompositionSelectionGroup const&) const
 *   [_ZNK28SC_CompositionSelectionGroupeqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_CompositionSelectionGroup::operator==(
        SC_CompositionSelectionGroup *this,
        const SC_CompositionSelectionGroup *a)
{
  int v4; // w20
  const void *v5; // x0
  const void *v6; // x1
  size_t v7; // x2
  const void *v9; // x0
  const void *v10; // x1
  size_t v11; // x2
  __int64 v12; // x20
  unsigned int v13; // w22
  __int64 v14; // [xsp+50h] [xbp+50h]
  v4 = *(unsigned __int16 *)this;
  if ( *(unsigned __int16 *)a != v4 )
    return 0;
  v5 = *((const void **)this + 1);
  v6 = *((const void **)a + 1);
  v7 = *((_QWORD *)this + 2) - (_QWORD)v5;
  if ( v7 != *((_QWORD *)a + 2) - (_QWORD)v6 || v7 && memcmp(v5, v6, v7) )
    return 0;
  v9 = *((const void **)this + 4);
  v10 = *((const void **)a + 4);
  v11 = *((_QWORD *)this + 5) - (_QWORD)v9;
  if ( v11 != *((_QWORD *)a + 5) - (_QWORD)v10 || v11 && memcmp(v9, v10, v11) )
    return 0;
  if ( v4 )
  {
    v12 = 0;
    v13 = 0;
    while ( (unsigned __int8)SC_CompositionSelectionGroup::Presets::operator==(
                               (SC_CompositionSelectionGroup__Presets *)(*((_QWORD *)this + 7) + v12),
                               (const SC_CompositionSelectionGroup__Presets *)(*((_QWORD *)a + 7) + v12)) )
    {
      ++v13;
      v12 += 48;
      if ( *(unsigned __int16 *)this <= v13 )
        goto LABEL_11;
    }
    return 0;
  }
LABEL_11:
  if ( *((unsigned __int8 *)a + 80) != *((unsigned __int8 *)this + 80)
    || *((_BYTE *)a + 80)
    && !SC_Tag::operator==(
          (SC_CompositionSelectionGroup *)((char *)this + 88),
          (const SC_CompositionSelectionGroup *)((char *)a + 88)) )
  {
    return 0;
  }
  return SC_Extension::operator==(
           (SC_CompositionSelectionGroup *)((char *)this + 200),
           (const SC_CompositionSelectionGroup *)((char *)a + 200));
}


/* ------------------------------------------------------------------------ */
/* 0x000a49f0  size 332 bytes  80 insns
 *   SC_CompositionSelectionGroup::CheckConformance(unsigned short, unsigned short) const
 *   [_ZNK28SC_CompositionSelectionGroup16CheckConformanceEtt]
 *   data refs: 0x3306d8, 0x330700, 0x330730, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_CompositionSelectionGroup::CheckConformance(
        SC_CompositionSelectionGroup *this,
        unsigned __int16 v,
        unsigned __int16 v2)
{
  unsigned __int16 *v3; // x5
  unsigned __int16 *v4; // x6
  unsigned __int16 *v5; // x3
  unsigned __int16 *v7; // x3
  unsigned __int16 *v8; // x7
  unsigned __int16 *v9; // x1
  __int64 v10; // x4
  __int64 v11; // x0
  __int64 v12; // [xsp+30h] [xbp+30h]
  v4 = *((unsigned __int16 **)this + 1);
  v3 = *((unsigned __int16 **)this + 2);
  if ( v3 == v4 )
  {
LABEL_6:
    v8 = *((unsigned __int16 **)this + 4);
    v7 = *((unsigned __int16 **)this + 5);
    if ( v7 == v8 )
    {
LABEL_11:
      v10 = *((_QWORD *)this + 8);
      if ( *((_QWORD *)this + 7) == v10 )
      {
        return 0;
      }
      else
      {
        v11 = *((_QWORD *)this + 7);
        while ( !*(_BYTE *)(v11 + 27) && !*(_BYTE *)v11
             || *(unsigned __int16 *)(v11 + 28) < (unsigned int)(unsigned __int16)(v7 - v8 + v3 - v4) )
        {
          v11 += 48;
          if ( v10 == v11 )
            return 0;
        }
        __printf_chk(1, "ERROR: Member index out of range\n");
        return 0xFFFFFFFFLL;
      }
    }
    else
    {
      v9 = *((unsigned __int16 **)this + 4);
      while ( *v9 < (unsigned int)v2 )
      {
        if ( v7 == ++v9 )
          goto LABEL_11;
      }
      __printf_chk(1, "ERROR: Selection set index out of range\n");
      return 0xFFFFFFFFLL;
    }
  }
  else
  {
    v5 = *((unsigned __int16 **)this + 1);
    while ( *v5 < (unsigned int)v )
    {
      if ( v3 == ++v5 )
        goto LABEL_6;
    }
    __printf_chk(1, "ERROR: Scene item index out of range\n", v2);
    return 0xFFFFFFFFLL;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000a6ae0  size 4708 bytes  1176 insns
 *   SC_CompositionSelectionGroup::Deserialize(TBitstreamReader<unsigned int>&, unsigned short)
 *   [_ZN28SC_CompositionSelectionGroup11DeserializeER16TBitstreamReaderIjEt]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_CompositionSelectionGroup::Deserialize(
        SC_CompositionSelectionGroup *this,
        TBitstreamReader_u32 *bs,
        unsigned __int16 v)
{
  __int64 v3; // x25
  __int64 v4; // x26
  unsigned int cache; // w1
  int v8; // w0
  unsigned int v9; // w21
  unsigned int v10; // w22
  __int64 v11; // x2
  __int64 v12; // x4
  unsigned __int64 v13; // x1
  __int64 v14; // x20
  __int64 v15; // x22
  unsigned int v16; // w1
  int v17; // w0
  unsigned int v18; // w21
  unsigned int v19; // w0
  int v20; // w1
  __int16 v21; // w21
  unsigned int v22; // w0
  unsigned int v23; // w1
  int v24; // w0
  unsigned int v25; // w21
  unsigned int v26; // w22
  __int64 v27; // x2
  __int64 v28; // x4
  unsigned __int64 v29; // x0
  __int64 v30; // x20
  __int64 v31; // x22
  unsigned int v32; // w1
  int v33; // w0
  unsigned int v34; // w21
  unsigned int v35; // w0
  int v36; // w1
  __int16 v37; // w21
  unsigned int v38; // w0
  __int64 v39; // x0
  int v40; // w21
  int v41; // w0
  unsigned int v42; // w1
  int v43; // w0
  unsigned int v44; // w26
  unsigned int v45; // w1
  unsigned int v46; // w25
  int v47; // w0
  unsigned int v48; // w1
  unsigned int v49; // w27
  int v50; // w0
  unsigned int v51; // w1
  int v52; // w0
  unsigned int v53; // w27
  unsigned int v54; // w1
  unsigned int v55; // w26
  unsigned int v56; // w25
  int v57; // w0
  unsigned int v58; // w1
  int v59; // w0
  unsigned int v60; // w27
  unsigned int v61; // w1
  unsigned int v62; // w26
  unsigned int v63; // w25
  int v64; // w0
  unsigned int v65; // w1
  int v66; // w0
  unsigned int v67; // w27
  unsigned int v68; // w1
  unsigned int v69; // w25
  int v70; // w0
  unsigned int v71; // w27
  int v72; // w0
  unsigned int v73; // w1
  int v74; // w0
  unsigned int v75; // w25
  unsigned int v76; // w1
  int v77; // w0
  unsigned int v78; // w27
  unsigned int v79; // w1
  int v80; // w0
  unsigned int v81; // w25
  unsigned int v82; // w1
  unsigned int v83; // w27
  unsigned int v84; // w25
  int v85; // w0
  unsigned int v86; // w1
  int v87; // w0
  unsigned int v88; // w27
  unsigned int v89; // w25
  int v90; // w0
  unsigned int v91; // w1
  int v92; // w0
  unsigned int v93; // w27
  unsigned int v94; // w25
  int v95; // w0
  unsigned int v96; // w1
  int v97; // w0
  unsigned int v98; // w27
  unsigned int v99; // w25
  int v100; // w0
  _QWORD *v101; // x1
  __int64 v102; // x5
  __int64 v103; // x5
  __int64 v104; // x5
  unsigned int v105; // w1
  int v106; // w0
  unsigned int v107; // w20
  __int64 result; // x0
  unsigned int v109; // w2
  int v110; // w5
  const unsigned __int8 *v111; // x1
  const unsigned __int8 *v112; // x1
  unsigned int v113; // w2
  int v114; // w5
  const unsigned __int8 *v115; // x1
  const unsigned __int8 *v116; // x1
  char v117; // w27
  unsigned int v118; // w0
  int v119; // w1
  unsigned int v120; // w6
  unsigned int v121; // w0
  unsigned int v122; // w2
  int v123; // w5
  const unsigned __int8 *v124; // x1
  const unsigned __int8 *v125; // x1
  unsigned int v126; // w2
  int v127; // w4
  const unsigned __int8 *v128; // x1
  const unsigned __int8 *v129; // x1
  unsigned int v130; // w2
  int v131; // w5
  const unsigned __int8 *v132; // x1
  const unsigned __int8 *v133; // x1
  unsigned int v134; // w2
  int v135; // w4
  const unsigned __int8 *v136; // x1
  const unsigned __int8 *v137; // x1
  unsigned int v138; // w2
  int v139; // w5
  const unsigned __int8 *v140; // x1
  const unsigned __int8 *v141; // x1
  unsigned int v142; // w2
  int v143; // w5
  const unsigned __int8 *v144; // x1
  const unsigned __int8 *v145; // x1
  unsigned int v146; // w2
  int v147; // w5
  const unsigned __int8 *v148; // x1
  const unsigned __int8 *v149; // x1
  int v150; // w0
  unsigned int v151; // w0
  int v152; // w1
  const unsigned __int8 *v153; // x5
  char v154; // w25
  unsigned int v155; // w0
  unsigned int v156; // w1
  int v157; // w0
  unsigned int v158; // w27
  unsigned int v159; // w2
  int v160; // w4
  const unsigned __int8 *v161; // x1
  const unsigned __int8 *v162; // x1
  __int16 v163; // w27
  unsigned int v164; // w2
  int v165; // w5
  const unsigned __int8 *v166; // x1
  const unsigned __int8 *v167; // x1
  int v168; // w0
  unsigned int v169; // w27
  unsigned int v170; // w1
  unsigned int v171; // w25
  int v172; // w0
  unsigned int v173; // w2
  int v174; // w4
  const unsigned __int8 *v175; // x1
  const unsigned __int8 *v176; // x1
  const unsigned __int8 *v177; // x1
  unsigned int v178; // w2
  int v179; // w4
  const unsigned __int8 *v180; // x1
  unsigned int v181; // w2
  int v182; // w5
  const unsigned __int8 *v183; // x1
  unsigned int v184; // w2
  int v185; // w5
  const unsigned __int8 *v186; // x1
  const unsigned __int8 *v187; // x1
  __int16 v188; // w27
  unsigned int v189; // w2
  int v190; // w4
  const unsigned __int8 *v191; // x1
  const unsigned __int8 *v192; // x1
  unsigned int v193; // w2
  int v194; // w4
  const unsigned __int8 *v195; // x1
  unsigned int v196; // w2
  int v197; // w5
  const unsigned __int8 *v198; // x1
  unsigned int v199; // w0
  int v200; // w1
  unsigned int v201; // w0
  unsigned int v202; // w2
  int v203; // w4
  const unsigned __int8 *v204; // x1
  unsigned int v205; // w2
  int v206; // w4
  const unsigned __int8 *v207; // x1
  unsigned int v208; // w2
  int v209; // w4
  const unsigned __int8 *v210; // x1
  unsigned int v211; // w1
  int v212; // w5
  const unsigned __int8 *v213; // x2
  __int64 v214; // x21
  __int64 v215; // x21
  unsigned int v216; // w2
  int v217; // w4
  const unsigned __int8 *v218; // x1
  unsigned int v219; // w2
  const unsigned __int8 *cur; // x1
  int bitsLeft; // w4
  unsigned int v222; // w0
  int v223; // w1
  unsigned int v224; // w0
  unsigned int v225; // w2
  int v226; // w5
  const unsigned __int8 *v227; // x1
  const unsigned __int8 *v228; // x1
  unsigned int v229; // w2
  int v230; // w4
  const unsigned __int8 *v231; // x1
  __int64 v232; // [xsp+40h] [xbp+40h]
  __int64 v233; // [xsp+48h] [xbp+48h]
  __int64 v234; // [xsp+68h] [xbp+68h] BYREF
  __int64 v235; // [xsp+70h] [xbp+70h]
  __int128 v236; // [xsp+78h] [xbp+78h]
  __int64 v237; // [xsp+88h] [xbp+88h]
  __int64 v238; // [xsp+90h] [xbp+90h]
  __int64 v239; // [xsp+98h] [xbp+98h]
  __int64 v240; // [xsp+A0h] [xbp+A0h]
  *(_WORD *)this = v;
  cache = bs->cache;
  v8 = bs->bitsLeft - 6;
  bs->bitsLeft = v8;
  v9 = cache >> 26;
  if ( v8 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v219 = bs->cache;
    cur = bs->cur;
    bitsLeft = bs->bitsLeft;
    v8 = bitsLeft + 32;
    bs->bitsLeft = bitsLeft + 32;
    bs->cur = cur + 4;
    LOWORD(v9) = v9 | (v219 >> (bitsLeft + 32));
    v10 = (unsigned __int16)v9;
    if ( bitsLeft == -32 )
      bs->cache = 0;
    else
      bs->cache = v219 << -(char)bitsLeft;
  }
  else
  {
    v10 = cache >> 26;
    bs->cache = cache << 6;
  }
  v12 = *((_QWORD *)this + 1);
  v11 = *((_QWORD *)this + 2);
  v13 = (v11 - v12) >> 1;
  if ( (unsigned __int16)v9 > v13 )
  {
    std::vector<unsigned short>::_M_default_append((char *)this + 8, (unsigned __int16)v9 - v13);
    v8 = bs->bitsLeft;
  }
  else if ( (unsigned __int16)v9 < v13 )
  {
    v215 = v12 + 2LL * (unsigned __int16)v9;
    if ( v11 != v215 )
    {
      *((_QWORD *)this + 2) = v215;
      v8 = bs->bitsLeft;
    }
  }
  if ( v10 )
  {
    v14 = 0;
    v15 = 2 * (v10 - 1 + 1LL);
    do
    {
      while ( 1 )
      {
        v16 = bs->cache;
        v17 = v8 - 6;
        bs->bitsLeft = v17;
        v18 = v16 >> 26;
        if ( v17 < 0 )
          break;
        bs->cache = v16 << 6;
        *(_WORD *)(*((_QWORD *)this + 1) + v14) = v18;
        v14 += 2;
        v8 = bs->bitsLeft;
        if ( v15 == v14 )
          goto LABEL_12;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v19 = bs->cache;
      v20 = bs->bitsLeft;
      v21 = v18 | (v19 >> (v20 + 32));
      v22 = v19 << -(char)v20;
      if ( v20 == -32 )
        v22 = 0;
      bs->cur += 4;
      bs->cache = v22;
      bs->bitsLeft = v20 + 32;
      *(_WORD *)(*((_QWORD *)this + 1) + v14) = v21;
      v14 += 2;
      v8 = bs->bitsLeft;
    }
    while ( v15 != v14 );
  }
LABEL_12:
  v23 = bs->cache;
  v24 = v8 - 6;
  bs->bitsLeft = v24;
  v25 = v23 >> 26;
  if ( v24 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v216 = bs->cache;
    v217 = bs->bitsLeft;
    v218 = bs->cur;
    bs->bitsLeft = v217 + 32;
    bs->cur = v218 + 4;
    LOWORD(v25) = v25 | (v216 >> (v217 + 32));
    v26 = (unsigned __int16)v25;
    if ( v217 == -32 )
      bs->cache = 0;
    else
      bs->cache = v216 << -(char)v217;
  }
  else
  {
    v26 = v23 >> 26;
    bs->cache = v23 << 6;
  }
  v28 = *((_QWORD *)this + 4);
  v27 = *((_QWORD *)this + 5);
  v29 = (v27 - v28) >> 1;
  if ( (unsigned __int16)v25 > v29 )
  {
    std::vector<unsigned short>::_M_default_append((char *)this + 32, (unsigned __int16)v25 - v29);
  }
  else if ( (unsigned __int16)v25 < v29 )
  {
    v214 = v28 + 2LL * (unsigned __int16)v25;
    if ( v27 != v214 )
      *((_QWORD *)this + 5) = v214;
  }
  if ( v26 )
  {
    v30 = 0;
    v31 = 2 * (v26 - 1 + 1LL);
    do
    {
      while ( 1 )
      {
        v32 = bs->cache;
        v33 = bs->bitsLeft - 6;
        bs->bitsLeft = v33;
        v34 = v32 >> 26;
        if ( v33 < 0 )
          break;
        bs->cache = v32 << 6;
        *(_WORD *)(*((_QWORD *)this + 4) + v30) = v34;
        v30 += 2;
        if ( v30 == v31 )
          goto LABEL_23;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v35 = bs->cache;
      v36 = bs->bitsLeft;
      v37 = v34 | (v35 >> (v36 + 32));
      v38 = v35 << -(char)v36;
      if ( v36 == -32 )
        v38 = 0;
      bs->cur += 4;
      bs->cache = v38;
      bs->bitsLeft = v36 + 32;
      *(_WORD *)(*((_QWORD *)this + 4) + v30) = v37;
      v30 += 2;
    }
    while ( v30 != v31 );
  }
LABEL_23:
  v39 = *((_QWORD *)this + 7);
  if ( v39 != *((_QWORD *)this + 8) )
    *((_QWORD *)this + 8) = v39;
  v40 = 0;
  if ( *(_WORD *)this )
  {
    v232 = v3;
    v233 = v4;
    while ( 1 )
    {
      v42 = bs->cache;
      v41 = bs->bitsLeft;
      LOWORD(v234) = 0;
      BYTE2(v234) = 0;
      v43 = v41 - 1;
      bs->bitsLeft = v43;
      HIDWORD(v234) = 16777392;
      v44 = v42 >> 31;
      LOBYTE(v235) = 0;
      *(_DWORD *)((char *)&v235 + 2) = 0;
      BYTE6(v235) = 0;
      LODWORD(v236) = 0;
      BYTE4(v236) = 0;
      *(_QWORD *)((char *)&v236 + 6) = 0;
      BYTE14(v236) = 0;
      LOWORD(v237) = 0;
      BYTE2(v237) = 0;
      WORD2(v237) = 0;
      BYTE6(v237) = 0;
      LOWORD(v238) = 0;
      BYTE2(v238) = 0;
      HIDWORD(v238) = 0;
      if ( v43 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v126 = bs->cache;
        v127 = bs->bitsLeft;
        v128 = bs->cur;
        v43 = v127 + 32;
        bs->bitsLeft = v127 + 32;
        v129 = v128 + 4;
        bs->cur = v129;
        LOBYTE(v44) = v44 | (v126 >> (v127 + 32));
        if ( v127 == -32 )
        {
          LOBYTE(v46) = 0;
          *(_QWORD *)&bs->cache = 0xFFFFFFFF00000000LL;
          LOBYTE(v234) = (unsigned __int8)v44 != 0;
          goto LABEL_110;
        }
        v45 = v126 << -(char)v127;
        bs->cache = v45;
        v46 = v45 >> 31;
      }
      else
      {
        v45 = 2 * v42;
        bs->cache = v45;
        v46 = v45 >> 31;
      }
      v47 = v43 - 1;
      bs->bitsLeft = v47;
      LOBYTE(v234) = (unsigned __int8)v44 != 0;
      if ( (v47 & 0x80000000) == 0 )
      {
        v48 = 2 * v45;
        bs->cache = v48;
        v49 = v48 >> 31;
        goto LABEL_31;
      }
      v129 = bs->cur;
LABEL_110:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v129);
      v130 = bs->cache;
      v131 = bs->bitsLeft;
      v132 = bs->cur;
      v47 = v131 + 32;
      bs->bitsLeft = v131 + 32;
      v133 = v132 + 4;
      bs->cur = v133;
      LOBYTE(v46) = v46 | (v130 >> (v131 + 32));
      if ( v131 == -32 )
      {
        bs->cache = 0;
        BYTE1(v234) = (unsigned __int8)v46 != 0;
        if ( !(_BYTE)v46 )
        {
          LOBYTE(v49) = 0;
          bs->bitsLeft = -1;
          goto LABEL_113;
        }
        LOBYTE(v49) = 0;
        bs->bitsLeft = -1;
        goto LABEL_152;
      }
      v48 = v130 << -(char)v131;
      bs->cache = v48;
      v49 = v48 >> 31;
LABEL_31:
      v50 = v47 - 1;
      BYTE1(v234) = (unsigned __int8)v46 != 0;
      if ( !(_BYTE)v46 )
        goto LABEL_32;
      bs->bitsLeft = v50;
      if ( (v50 & 0x80000000) == 0 )
      {
        v48 *= 2;
        bs->cache = v48;
        goto LABEL_144;
      }
      v133 = bs->cur;
LABEL_152:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v133);
      v173 = bs->cache;
      v174 = bs->bitsLeft;
      v175 = bs->cur;
      v50 = v174 + 32;
      bs->bitsLeft = v174 + 32;
      v176 = v175 + 4;
      bs->cur = v176;
      LOBYTE(v49) = v49 | (v173 >> (v174 + 32));
      if ( v174 != -32 )
      {
        v48 = v173 << -(char)v174;
        bs->cache = v48;
LABEL_144:
        BYTE2(v234) = (unsigned __int8)v49 != 0;
        if ( (_BYTE)v49 )
        {
          v168 = v50 - 7;
          bs->bitsLeft = v168;
          v169 = v48 >> 25;
          if ( (v168 & 0x80000000) == 0 )
          {
            v170 = v48 << 7;
            bs->cache = v170;
            v171 = v170 >> 25;
            goto LABEL_147;
          }
          v176 = bs->cur;
LABEL_236:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, v176);
          v225 = bs->cache;
          v226 = bs->bitsLeft;
          v227 = bs->cur;
          v168 = v226 + 32;
          bs->bitsLeft = v226 + 32;
          v228 = v227 + 4;
          bs->cur = v228;
          LOWORD(v169) = v169 | (v225 >> (v226 + 32));
          if ( v226 == -32 )
          {
            LOWORD(v171) = 0;
            *(_QWORD *)&bs->cache = 0xFFFFFFF900000000LL;
            WORD2(v234) = v169;
LABEL_238:
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, v228);
            v229 = bs->cache;
            v230 = bs->bitsLeft;
            v231 = bs->cur;
            v172 = v230 + 32;
            bs->bitsLeft = v230 + 32;
            bs->cur = v231 + 4;
            LOWORD(v171) = v171 | (v229 >> (v230 + 32));
            if ( v230 == -32 )
            {
              LOBYTE(v49) = 0;
              v48 = 0;
              bs->cache = 0;
            }
            else
            {
              v48 = v229 << -(char)v230;
              bs->cache = v48;
              v49 = v48 >> 31;
            }
          }
          else
          {
            v170 = v225 << -(char)v226;
            bs->cache = v170;
            v171 = v170 >> 25;
LABEL_147:
            v172 = v168 - 7;
            bs->bitsLeft = v172;
            WORD2(v234) = v169;
            if ( v172 < 0 )
            {
              v228 = bs->cur;
              goto LABEL_238;
            }
            v48 = v170 << 7;
            bs->cache = v48;
            v49 = v48 >> 31;
          }
          v50 = v172 - 1;
          bs->bitsLeft = v50;
          HIWORD(v234) = v171;
          if ( (v50 & 0x80000000) == 0 )
          {
LABEL_33:
            v51 = 2 * v48;
            bs->cache = v51;
            goto LABEL_34;
          }
          goto LABEL_150;
        }
        goto LABEL_155;
      }
      bs->cache = 0;
      BYTE2(v234) = (unsigned __int8)v49 != 0;
      if ( (_BYTE)v49 )
      {
        LOWORD(v169) = 0;
        bs->bitsLeft = -7;
        goto LABEL_236;
      }
      v48 = 0;
LABEL_155:
      --v50;
      v49 = v48 >> 31;
      HIDWORD(v234) = 16777392;
LABEL_32:
      bs->bitsLeft = v50;
      if ( (v50 & 0x80000000) == 0 )
        goto LABEL_33;
LABEL_150:
      v133 = bs->cur;
LABEL_113:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v133);
      v134 = bs->cache;
      v135 = bs->bitsLeft;
      v136 = bs->cur;
      v50 = v135 + 32;
      bs->bitsLeft = v135 + 32;
      v137 = v136 + 4;
      bs->cur = v137;
      LOBYTE(v49) = v49 | (v134 >> (v135 + 32));
      if ( v135 != -32 )
      {
        v51 = v134 << -(char)v135;
        bs->cache = v51;
LABEL_34:
        LOBYTE(v235) = (unsigned __int8)v49 != 0;
        if ( (_BYTE)v49 )
        {
          v52 = v50 - 5;
          bs->bitsLeft = v52;
          v53 = v51 >> 27;
          if ( v52 < 0 )
          {
            v137 = bs->cur;
            goto LABEL_174;
          }
          v54 = 32 * v51;
          bs->cache = v54;
          v55 = v54 >> 27;
          goto LABEL_37;
        }
        goto LABEL_116;
      }
      bs->cache = 0;
      LOBYTE(v235) = (unsigned __int8)v49 != 0;
      if ( (_BYTE)v49 )
      {
        LOWORD(v53) = 0;
        bs->bitsLeft = -5;
LABEL_174:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v137);
        v181 = bs->cache;
        v182 = bs->bitsLeft;
        v183 = bs->cur;
        v52 = v182 + 32;
        bs->bitsLeft = v182 + 32;
        v177 = v183 + 4;
        bs->cur = v177;
        LOWORD(v53) = v53 | (v181 >> (v182 + 32));
        if ( v182 == -32 )
        {
          LOWORD(v55) = 0;
          *(_QWORD *)&bs->cache = 0xFFFFFFFB00000000LL;
          WORD1(v235) = v53;
          goto LABEL_171;
        }
        v54 = v181 << -(char)v182;
        bs->cache = v54;
        v55 = v54 >> 27;
LABEL_37:
        v50 = v52 - 5;
        bs->bitsLeft = v50;
        WORD1(v235) = v53;
        if ( v50 < 0 )
        {
          v177 = bs->cur;
LABEL_171:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, v177);
          v178 = bs->cache;
          v179 = bs->bitsLeft;
          v180 = bs->cur;
          v50 = v179 + 32;
          bs->bitsLeft = v179 + 32;
          bs->cur = v180 + 4;
          LOWORD(v55) = v55 | (v178 >> (v179 + 32));
          if ( v179 == -32 )
          {
            LOBYTE(v56) = 0;
            v51 = 0;
            bs->cache = 0;
          }
          else
          {
            v51 = v178 << -(char)v179;
            bs->cache = v51;
            v56 = v51 >> 31;
          }
        }
        else
        {
          v51 = 32 * v54;
          bs->cache = v51;
          v56 = v51 >> 31;
        }
        WORD2(v235) = v55;
        goto LABEL_40;
      }
      v51 = 0;
LABEL_116:
      v56 = v51 >> 31;
LABEL_40:
      v57 = v50 - 1;
      bs->bitsLeft = v57;
      if ( v57 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v122 = bs->cache;
        v123 = bs->bitsLeft;
        v124 = bs->cur;
        v57 = v123 + 32;
        bs->bitsLeft = v123 + 32;
        v125 = v124 + 4;
        bs->cur = v125;
        LOBYTE(v56) = v56 | (v122 >> (v123 + 32));
        if ( v123 == -32 )
        {
          bs->cache = 0;
          BYTE6(v235) = (unsigned __int8)v56 != 0;
          if ( (_BYTE)v56 )
          {
            LOWORD(v60) = 0;
            bs->bitsLeft = -5;
            goto LABEL_180;
          }
          v58 = 0;
          goto LABEL_107;
        }
        v58 = v122 << -(char)v123;
        bs->cache = v58;
      }
      else
      {
        v58 = 2 * v51;
        bs->cache = v58;
      }
      BYTE6(v235) = (unsigned __int8)v56 != 0;
      if ( (_BYTE)v56 )
      {
        v59 = v57 - 5;
        bs->bitsLeft = v59;
        v60 = v58 >> 27;
        if ( v59 < 0 )
        {
          v125 = bs->cur;
LABEL_180:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, v125);
          v184 = bs->cache;
          v185 = bs->bitsLeft;
          v186 = bs->cur;
          bs->bitsLeft = v185 + 32;
          v187 = v186 + 4;
          bs->cur = v187;
          v188 = v60 | (v184 >> (v185 + 32));
          if ( v185 == -32 )
          {
            LOWORD(v62) = 0;
            *(_QWORD *)&bs->cache = 0xFFFFFFFB00000000LL;
            LOWORD(v236) = v188;
            goto LABEL_182;
          }
          v57 = v185 + 27;
          bs->bitsLeft = v185 + 27;
          v61 = v184 << -(char)v185;
          bs->cache = v61;
          LOWORD(v236) = v188;
          v62 = v61 >> 27;
          if ( v185 + 27 < 0 )
            goto LABEL_194;
LABEL_45:
          v58 = 32 * v61;
          bs->cache = v58;
          v63 = v58 >> 31;
        }
        else
        {
          v61 = 32 * v58;
          v57 = v59 - 5;
          bs->cache = v61;
          bs->bitsLeft = v57;
          v62 = v61 >> 27;
          LOWORD(v236) = v60;
          if ( (v57 & 0x80000000) == 0 )
            goto LABEL_45;
LABEL_194:
          v187 = bs->cur;
LABEL_182:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, v187);
          v189 = bs->cache;
          v190 = bs->bitsLeft;
          v191 = bs->cur;
          v57 = v190 + 32;
          bs->bitsLeft = v190 + 32;
          bs->cur = v191 + 4;
          LOWORD(v62) = v62 | (v189 >> (v190 + 32));
          if ( v190 == -32 )
          {
            LOBYTE(v63) = 0;
            v58 = 0;
            bs->cache = 0;
          }
          else
          {
            v58 = v189 << -(char)v190;
            bs->cache = v58;
            v63 = v58 >> 31;
          }
        }
        WORD1(v236) = v62;
        goto LABEL_47;
      }
LABEL_107:
      v63 = v58 >> 31;
LABEL_47:
      v64 = v57 - 1;
      bs->bitsLeft = v64;
      if ( (v64 & 0x80000000) == 0 )
      {
        v65 = 2 * v58;
        bs->cache = v65;
        goto LABEL_49;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v109 = bs->cache;
      v110 = bs->bitsLeft;
      v111 = bs->cur;
      v64 = v110 + 32;
      bs->bitsLeft = v110 + 32;
      v112 = v111 + 4;
      bs->cur = v112;
      LOBYTE(v63) = v63 | (v109 >> (v110 + 32));
      if ( v110 != -32 )
      {
        v65 = v109 << -(char)v110;
        bs->cache = v65;
LABEL_49:
        BYTE4(v236) = (unsigned __int8)v63 != 0;
        if ( (_BYTE)v63 )
        {
          v66 = v64 - 3;
          bs->bitsLeft = v66;
          v67 = v65 >> 29;
          if ( v66 < 0 )
          {
            v112 = bs->cur;
            goto LABEL_188;
          }
          v68 = 8 * v65;
          bs->cache = v68;
          v69 = v68 >> 29;
          goto LABEL_52;
        }
        goto LABEL_98;
      }
      bs->cache = 0;
      BYTE4(v236) = (unsigned __int8)v63 != 0;
      if ( (_BYTE)v63 )
      {
        LOWORD(v67) = 0;
        bs->bitsLeft = -3;
LABEL_188:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v112);
        v196 = bs->cache;
        v197 = bs->bitsLeft;
        v198 = bs->cur;
        v66 = v197 + 32;
        bs->bitsLeft = v197 + 32;
        v192 = v198 + 4;
        bs->cur = v192;
        LOWORD(v67) = v67 | (v196 >> (v197 + 32));
        if ( v197 == -32 )
        {
          LOWORD(v69) = 0;
          *(_QWORD *)&bs->cache = 0xFFFFFFFD00000000LL;
          WORD3(v236) = v67;
          goto LABEL_185;
        }
        v68 = v196 << -(char)v197;
        bs->cache = v68;
        v69 = v68 >> 29;
LABEL_52:
        v70 = v66 - 3;
        bs->bitsLeft = v70;
        WORD3(v236) = v67;
        if ( v70 < 0 )
        {
          v192 = bs->cur;
LABEL_185:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, v192);
          v193 = bs->cache;
          v194 = bs->bitsLeft;
          v195 = bs->cur;
          v70 = v194 + 32;
          bs->bitsLeft = v194 + 32;
          bs->cur = v195 + 4;
          LOWORD(v69) = v69 | (v193 >> (v194 + 32));
          if ( v194 == -32 )
          {
            LOBYTE(v71) = 0;
            v65 = 0;
            bs->cache = 0;
          }
          else
          {
            v65 = v193 << -(char)v194;
            bs->cache = v65;
            v71 = v65 >> 31;
          }
        }
        else
        {
          v65 = 8 * v68;
          bs->cache = v65;
          v71 = v65 >> 31;
        }
        v72 = v70 - 1;
        bs->bitsLeft = v72;
        WORD4(v236) = v69;
        if ( (v72 & 0x80000000) == 0 )
          goto LABEL_55;
        goto LABEL_99;
      }
      v65 = 0;
LABEL_98:
      v72 = v64 - 1;
      bs->bitsLeft = v72;
      v71 = v65 >> 31;
      if ( (v72 & 0x80000000) == 0 )
      {
LABEL_55:
        v73 = 2 * v65;
        v74 = v72 - 1;
        bs->cache = v73;
        v75 = v73 >> 31;
        bs->bitsLeft = v74;
        BYTE10(v236) = (unsigned __int8)v71 != 0;
        if ( (v74 & 0x80000000) == 0 )
          goto LABEL_56;
LABEL_159:
        v116 = bs->cur;
        goto LABEL_101;
      }
LABEL_99:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v113 = bs->cache;
      v114 = bs->bitsLeft;
      v115 = bs->cur;
      bs->bitsLeft = v114 + 32;
      v116 = v115 + 4;
      bs->cur = v116;
      v117 = v71 | (v113 >> (v114 + 32));
      if ( v114 != -32 )
      {
        bs->bitsLeft = v114 + 31;
        v73 = v113 << -(char)v114;
        bs->cache = v73;
        v75 = v73 >> 31;
        BYTE10(v236) = v117 != 0;
        if ( ((v114 + 31) & 0x80000000) == 0 )
        {
LABEL_56:
          bs->cache = 2 * v73;
          goto LABEL_57;
        }
        goto LABEL_159;
      }
      LOBYTE(v75) = 0;
      *(_QWORD *)&bs->cache = 0xFFFFFFFF00000000LL;
      BYTE10(v236) = v117 != 0;
LABEL_101:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v116);
      v118 = bs->cache;
      v119 = bs->bitsLeft;
      v120 = v118 >> (v119 + 32);
      v121 = v118 << -(char)v119;
      LOBYTE(v75) = v75 | v120;
      if ( v119 == -32 )
        v121 = 0;
      bs->cur += 4;
      bs->cache = v121;
      bs->bitsLeft = v119 + 32;
LABEL_57:
      BYTE11(v236) = (unsigned __int8)v75 != 0;
      if ( !(_BYTE)v75 && !(_BYTE)v234 )
        goto LABEL_83;
      v76 = bs->cache;
      v77 = bs->bitsLeft - 6;
      bs->bitsLeft = v77;
      v78 = v76 >> 26;
      if ( v77 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v159 = bs->cache;
        v160 = bs->bitsLeft;
        v161 = bs->cur;
        bs->bitsLeft = v160 + 32;
        v162 = v161 + 4;
        bs->cur = v162;
        v163 = v78 | (v159 >> (v160 + 32));
        if ( v160 == -32 )
        {
          LOBYTE(v81) = 0;
          *(_QWORD *)&bs->cache = 0xFFFFFFFF00000000LL;
          WORD6(v236) = v163;
          goto LABEL_138;
        }
        v80 = v160 + 31;
        bs->bitsLeft = v160 + 31;
        v79 = v159 << -(char)v160;
        bs->cache = v79;
        WORD6(v236) = v163;
        v81 = v79 >> 31;
        if ( ((v160 + 31) & 0x80000000) == 0 )
        {
LABEL_61:
          v82 = 2 * v79;
          bs->cache = v82;
          goto LABEL_62;
        }
      }
      else
      {
        v79 = v76 << 6;
        v80 = v77 - 1;
        bs->cache = v79;
        bs->bitsLeft = v80;
        v81 = v79 >> 31;
        WORD6(v236) = v78;
        if ( (v80 & 0x80000000) == 0 )
          goto LABEL_61;
      }
      v162 = bs->cur;
LABEL_138:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v162);
      v164 = bs->cache;
      v165 = bs->bitsLeft;
      v166 = bs->cur;
      v80 = v165 + 32;
      bs->bitsLeft = v165 + 32;
      v167 = v166 + 4;
      bs->cur = v167;
      LOBYTE(v81) = v81 | (v164 >> (v165 + 32));
      if ( v165 != -32 )
      {
        v82 = v164 << -(char)v165;
        bs->cache = v82;
LABEL_62:
        BYTE14(v236) = (unsigned __int8)v81 != 0;
        if ( (_BYTE)v81 )
        {
          v80 -= 9;
          bs->bitsLeft = v80;
          v83 = v82 >> 23;
          if ( v80 < 0 )
          {
            v167 = bs->cur;
            goto LABEL_201;
          }
          v82 <<= 9;
          bs->cache = v82;
          v84 = v82 >> 31;
LABEL_65:
          LOWORD(v237) = v83;
          goto LABEL_66;
        }
        goto LABEL_141;
      }
      bs->cache = 0;
      BYTE14(v236) = (unsigned __int8)v81 != 0;
      if ( (_BYTE)v81 )
      {
        LOWORD(v83) = 0;
        bs->bitsLeft = -9;
LABEL_201:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v167);
        v202 = bs->cache;
        v203 = bs->bitsLeft;
        v204 = bs->cur;
        v80 = v203 + 32;
        bs->bitsLeft = v203 + 32;
        bs->cur = v204 + 4;
        LOWORD(v83) = v83 | (v202 >> (v203 + 32));
        if ( v203 == -32 )
        {
          LOBYTE(v84) = 0;
          v82 = 0;
          bs->cache = 0;
        }
        else
        {
          v82 = v202 << -(char)v203;
          bs->cache = v82;
          v84 = v82 >> 31;
        }
        goto LABEL_65;
      }
      v82 = 0;
LABEL_141:
      v84 = v82 >> 31;
LABEL_66:
      v85 = v80 - 1;
      bs->bitsLeft = v85;
      if ( (v85 & 0x80000000) == 0 )
      {
        v86 = 2 * v82;
        bs->cache = v86;
        goto LABEL_68;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v138 = bs->cache;
      v139 = bs->bitsLeft;
      v140 = bs->cur;
      v85 = v139 + 32;
      bs->bitsLeft = v139 + 32;
      v141 = v140 + 4;
      bs->cur = v141;
      LOBYTE(v84) = v84 | (v138 >> (v139 + 32));
      if ( v139 != -32 )
      {
        v86 = v138 << -(char)v139;
        bs->cache = v86;
LABEL_68:
        BYTE2(v237) = (unsigned __int8)v84 != 0;
        if ( (_BYTE)v84 )
        {
          v87 = v85 - 6;
          bs->bitsLeft = v87;
          v88 = v86 >> 26;
          if ( v87 < 0 )
          {
            v141 = bs->cur;
            goto LABEL_204;
          }
          v86 <<= 6;
          bs->cache = v86;
          v89 = v86 >> 31;
LABEL_71:
          v90 = v87 - 1;
          bs->bitsLeft = v90;
          WORD2(v237) = v88;
          if ( (v90 & 0x80000000) == 0 )
            goto LABEL_72;
          goto LABEL_121;
        }
        goto LABEL_120;
      }
      bs->cache = 0;
      BYTE2(v237) = (unsigned __int8)v84 != 0;
      if ( (_BYTE)v84 )
      {
        LOWORD(v88) = 0;
        bs->bitsLeft = -6;
LABEL_204:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v141);
        v205 = bs->cache;
        v206 = bs->bitsLeft;
        v207 = bs->cur;
        v87 = v206 + 32;
        bs->bitsLeft = v206 + 32;
        bs->cur = v207 + 4;
        LOWORD(v88) = v88 | (v205 >> (v206 + 32));
        if ( v206 == -32 )
        {
          LOBYTE(v89) = 0;
          v86 = 0;
          bs->cache = 0;
        }
        else
        {
          v86 = v205 << -(char)v206;
          bs->cache = v86;
          v89 = v86 >> 31;
        }
        goto LABEL_71;
      }
      v86 = 0;
LABEL_120:
      v90 = v85 - 1;
      bs->bitsLeft = v90;
      v89 = v86 >> 31;
      if ( (v90 & 0x80000000) == 0 )
      {
LABEL_72:
        v91 = 2 * v86;
        bs->cache = v91;
        goto LABEL_73;
      }
LABEL_121:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v142 = bs->cache;
      v143 = bs->bitsLeft;
      v144 = bs->cur;
      v90 = v143 + 32;
      bs->bitsLeft = v143 + 32;
      v145 = v144 + 4;
      bs->cur = v145;
      LOBYTE(v89) = v89 | (v142 >> (v143 + 32));
      if ( v143 != -32 )
      {
        v91 = v142 << -(char)v143;
        bs->cache = v91;
LABEL_73:
        BYTE6(v237) = (unsigned __int8)v89 != 0;
        if ( (_BYTE)v89 )
        {
          v92 = v90 - 6;
          bs->bitsLeft = v92;
          v93 = v91 >> 26;
          if ( v92 < 0 )
          {
            v145 = bs->cur;
            goto LABEL_207;
          }
          v91 <<= 6;
          bs->cache = v91;
          v94 = v91 >> 31;
LABEL_76:
          v95 = v92 - 1;
          bs->bitsLeft = v95;
          LOWORD(v238) = v93;
          if ( (v95 & 0x80000000) == 0 )
            goto LABEL_77;
          goto LABEL_125;
        }
        goto LABEL_124;
      }
      bs->cache = 0;
      BYTE6(v237) = (unsigned __int8)v89 != 0;
      if ( (_BYTE)v89 )
      {
        LOWORD(v93) = 0;
        bs->bitsLeft = -6;
LABEL_207:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v145);
        v208 = bs->cache;
        v209 = bs->bitsLeft;
        v210 = bs->cur;
        v92 = v209 + 32;
        bs->bitsLeft = v209 + 32;
        bs->cur = v210 + 4;
        LOWORD(v93) = v93 | (v208 >> (v209 + 32));
        if ( v209 == -32 )
        {
          LOBYTE(v94) = 0;
          v91 = 0;
          bs->cache = 0;
        }
        else
        {
          v91 = v208 << -(char)v209;
          bs->cache = v91;
          v94 = v91 >> 31;
        }
        goto LABEL_76;
      }
      v91 = 0;
LABEL_124:
      v95 = v90 - 1;
      bs->bitsLeft = v95;
      v94 = v91 >> 31;
      if ( (v95 & 0x80000000) == 0 )
      {
LABEL_77:
        v96 = 2 * v91;
        bs->cache = v96;
        goto LABEL_78;
      }
LABEL_125:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v146 = bs->cache;
      v147 = bs->bitsLeft;
      v148 = bs->cur;
      v95 = v147 + 32;
      bs->bitsLeft = v147 + 32;
      v149 = v148 + 4;
      bs->cur = v149;
      LOBYTE(v94) = v94 | (v146 >> (v147 + 32));
      if ( v147 != -32 )
      {
        v96 = v146 << -(char)v147;
        bs->cache = v96;
LABEL_78:
        BYTE2(v238) = (unsigned __int8)v94 != 0;
        if ( (_BYTE)v94 )
        {
          v97 = v95 - 4;
          bs->bitsLeft = v97;
          v98 = v96 >> 28;
          if ( v97 < 0 )
          {
            v149 = bs->cur;
            goto LABEL_210;
          }
          v96 *= 16;
          bs->cache = v96;
          v99 = v96 >> 31;
LABEL_81:
          v100 = v97 - 1;
          bs->bitsLeft = v100;
          WORD2(v238) = v98;
          if ( (v100 & 0x80000000) == 0 )
            goto LABEL_82;
          goto LABEL_129;
        }
        goto LABEL_128;
      }
      bs->cache = 0;
      BYTE2(v238) = (unsigned __int8)v94 != 0;
      if ( (_BYTE)v94 )
      {
        LOWORD(v98) = 0;
        bs->bitsLeft = -4;
LABEL_210:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v149);
        v211 = bs->cache;
        v212 = bs->bitsLeft;
        v213 = bs->cur;
        v97 = v212 + 32;
        bs->bitsLeft = v212 + 32;
        bs->cur = v213 + 4;
        LOWORD(v98) = v98 | (v211 >> (v212 + 32));
        if ( v212 == -32 )
        {
          LOBYTE(v99) = 0;
          v96 = 0;
          bs->cache = 0;
        }
        else
        {
          v96 = v211 << -(char)v212;
          bs->cache = v96;
          v99 = v96 >> 31;
        }
        goto LABEL_81;
      }
      v96 = 0;
LABEL_128:
      v150 = v95 - 1;
      bs->bitsLeft = v150;
      v99 = v96 >> 31;
      if ( (v150 & 0x80000000) == 0 )
      {
LABEL_82:
        bs->cache = 2 * v96;
        BYTE6(v238) = (unsigned __int8)v99 != 0;
        if ( !(_BYTE)v99 )
          goto LABEL_83;
        goto LABEL_132;
      }
LABEL_129:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v151 = bs->cache;
      v152 = bs->bitsLeft;
      v153 = bs->cur;
      v154 = v99 | (v151 >> (v152 + 32));
      v155 = v151 << -(char)v152;
      if ( v152 == -32 )
        v155 = 0;
      bs->cache = v155;
      bs->bitsLeft = v152 + 32;
      bs->cur = v153 + 4;
      BYTE6(v238) = v154 != 0;
      if ( !v154 )
      {
LABEL_83:
        v101 = *((_QWORD **)this + 8);
        if ( v101 == *((_QWORD **)this + 9) )
          goto LABEL_135;
        goto LABEL_84;
      }
LABEL_132:
      v156 = bs->cache;
      v157 = bs->bitsLeft - 1;
      bs->bitsLeft = v157;
      v158 = v156 >> 31;
      if ( v157 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v199 = bs->cache;
        v200 = bs->bitsLeft;
        LOBYTE(v158) = v158 | (v199 >> (v200 + 32));
        v201 = v199 << -(char)v200;
        if ( v200 == -32 )
          v201 = 0;
        bs->cur += 4;
        bs->cache = v201;
        bs->bitsLeft = v200 + 32;
      }
      else
      {
        bs->cache = 2 * v156;
      }
      HIBYTE(v238) = (unsigned __int8)v158 != 0;
      v101 = *((_QWORD **)this + 8);
      if ( v101 == *((_QWORD **)this + 9) )
      {
LABEL_135:
        std::vector<SC_CompositionSelectionGroup::Presets>::_M_realloc_insert<SC_CompositionSelectionGroup::Presets const&>(
          (std_vector *)((char *)this + 56),
          v101,
          (const SC_CompositionSelectionGroup__Presets *)&v234);
        goto LABEL_85;
      }
LABEL_84:
      v102 = v235;
      *v101 = v234;
      v101[1] = v102;
      v103 = *((_QWORD *)&v236 + 1);
      v101[2] = v236;
      v101[3] = v103;
      v104 = v238;
      v101[4] = v237;
      v101[5] = v104;
      *((_QWORD *)this + 8) += 48LL;
LABEL_85:
      if ( *(unsigned __int16 *)this <= (unsigned int)++v40 )
      {
        v3 = v232;
        v4 = v233;
        break;
      }
    }
  }
  v105 = bs->cache;
  v106 = bs->bitsLeft - 1;
  bs->bitsLeft = v106;
  v107 = v105 >> 31;
  if ( v106 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v222 = bs->cache;
    v223 = bs->bitsLeft;
    LOBYTE(v107) = v107 | (v222 >> (v223 + 32));
    v224 = v222 << -(char)v223;
    if ( v223 == -32 )
      v224 = 0;
    bs->cur += 4;
    bs->cache = v224;
    bs->bitsLeft = v223 + 32;
  }
  else
  {
    bs->cache = 2 * v105;
  }
  *((_BYTE *)this + 80) = (unsigned __int8)v107 != 0;
  if ( !(_BYTE)v107
    || (result = SC_Tag::Deserialize((SC_CompositionSelectionGroup *)((char *)this + 88), bs), !(_DWORD)result) )
  {
    result = SC_Extension::Deserialize((SC_CompositionSelectionGroup *)((char *)this + 200), bs);
    if ( !(_DWORD)result )
      *((_WORD *)this + 101) = 2;
  }
  if ( v239 != _stack_chk_guard )
  {
    v232 = v3;
    v233 = v4;
  }
  return result;
}


