/* ============================================================================
 * APACSpectralData
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 3820 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000ab3b0   3820  APACSpectralData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&, APACScaleFactorData const&, apac::SFBOffsets const&) */

/* ------------------------------------------------------------------------ */
/* 0x000ab3b0  size 3820 bytes  950 insns
 *   APACSpectralData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&, APACScaleFactorData const&, apac::SFBOffsets const&)
 *   [_ZN16APACSpectralData11DeserializeER16TBitstreamReaderIjERK11APACICSInfoRK19APACScaleFactorDataRKN4apac10SFBOffsetsE]
 *   data refs: 0x3308c0, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSpectralData::Deserialize(
        APACSpectralData *this,
        TBitstreamReader_u32 *bs,
        const APACICSInfo *ics,
        const APACScaleFactorData *sfData,
        const apac__SFBOffsets *a)
{
  __int64 v5; // x19
  __int64 v6; // x20
  __int64 v7; // x21
  TBitstreamReader_u32 *v8; // x22
  __int64 v9; // x23
  __int64 v10; // x24
  __int64 v11; // x25
  __int64 v12; // x26
  __int64 v13; // x27
  __int64 v14; // x28
  __int64 v15; // x29
  __int64 v16; // x30
  __int64 v17; // d8
  __int64 v18; // d9
  __int64 v19; // d10
  __int64 *v20; // x9
  int v21; // w2
  unsigned int v22; // w0
  unsigned int v23; // w1
  int v24; // w19
  unsigned int v25; // w20
  __int64 v26; // x21
  int v27; // w0
  float *v28; // x21
  __int64 v29; // x1
  unsigned int v30; // w23
  unsigned int v31; // w26
  _UNKNOWN **HuffmanTable; // x0
  unsigned int v33; // w24
  _QWORD *v34; // x25
  int v35; // w28
  __int64 v36; // x0
  int v37; // w23
  int v38; // w26
  float v39; // s10
  int v40; // w26
  unsigned int v41; // w1
  int v42; // w0
  float32x4_t v43; // q0
  char v44; // w0
  __int64 v45; // x0
  float32x4_t v46; // q0
  int v47; // w1
  int v48; // w0
  unsigned int v50; // w0
  int v51; // w1
  int v52; // w19
  unsigned int v53; // w0
  unsigned int v54; // w24
  int v55; // w23
  int v56; // w26
  float v57; // s10
  int v58; // w26
  unsigned int v59; // w1
  int v60; // w0
  float32x4_t v61; // q0
  char v62; // w0
  unsigned __int64 v63; // x0
  unsigned int v64; // w1
  float v65; // s3
  __int64 v66; // x1
  float v67; // s2
  __int64 v68; // x1
  float v69; // s1
  __int64 v70; // x0
  unsigned int v71; // w0
  int v72; // w1
  int v73; // w19
  unsigned int v74; // w0
  unsigned int v75; // w27
  int v76; // w24
  float *v77; // x23
  int v78; // w26
  float v79; // s10
  int v80; // w26
  unsigned int cache; // w1
  int v82; // w0
  unsigned int v83; // w21
  char v84; // w0
  unsigned __int64 v85; // x0
  __int64 v86; // x1
  bool v87; // nf
  __int64 v88; // x0
  float v89; // s0
  float v90; // s0
  unsigned int v91; // w0
  int bitsLeft; // w1
  unsigned int v93; // w4
  unsigned int v94; // w0
  int v95; // w24
  float v96; // s10
  int v97; // w24
  unsigned int v98; // w1
  int v99; // w0
  unsigned int v100; // w23
  char v101; // w0
  unsigned __int64 v102; // x23
  bool v103; // zf
  __int64 v104; // x0
  __int64 v105; // x23
  unsigned int v106; // w27
  unsigned int v107; // w26
  float v108; // s0
  float v109; // s0
  unsigned int v110; // w1
  int v111; // w0
  unsigned int v112; // w23
  unsigned int v113; // w0
  unsigned int v114; // w2
  unsigned int v115; // w19
  int v116; // w1
  unsigned int v117; // w1
  int v118; // w0
  unsigned int v119; // w0
  unsigned int v120; // w2
  unsigned int v121; // w19
  int v122; // w1
  unsigned int v123; // w0
  int v124; // w1
  unsigned int v125; // w4
  unsigned int v126; // w0
  int v127; // w0
  unsigned int v128; // w1
  int v129; // w2
  unsigned int v130; // w0
  int v131; // w0
  unsigned int v132; // w1
  int v133; // w2
  const unsigned __int8 *cur; // x3
  unsigned int v135; // w0
  int v136; // w1
  int v137; // w23
  int v138; // w26
  float v139; // s10
  int v140; // w26
  unsigned int v141; // w1
  int v142; // w0
  char v143; // w0
  __int64 v144; // x0
  unsigned int v145; // w0
  int v146; // w1
  int v147; // w19
  unsigned int v148; // w0
  __int64 v149; // x1
  unsigned int v150; // w14
  unsigned __int16 *v151; // x13
  unsigned int v152; // w15
  unsigned int v153; // w11
  int v154; // w12
  __int64 v155; // x9
  const APACICSInfo *v156; // x16
  APACSpectralData *v157; // x17
  int v158; // w6
  __int64 v159; // x8
  _QWORD *v160; // x2
  int j; // w7
  unsigned __int16 *v162; // x3
  int v163; // w1
  int v164; // w0
  __int64 *v165; // x1
  __int64 v166; // x4
  __int64 v167; // x5
  _DWORD *v168; // x0
  _DWORD *i; // x1
  __int64 v171; // [xsp+0h] [xbp-1120h]
  __int64 v172; // [xsp+8h] [xbp-1118h]
  __int64 v173; // [xsp+10h] [xbp-1110h]
  __int64 v174; // [xsp+18h] [xbp-1108h]
  __int64 v175; // [xsp+20h] [xbp-1100h]
  TBitstreamReader_u32 *v176; // [xsp+28h] [xbp-10F8h]
  __int64 v177; // [xsp+30h] [xbp-10F0h]
  __int64 v178; // [xsp+38h] [xbp-10E8h]
  __int64 v179; // [xsp+40h] [xbp-10E0h]
  __int64 v180; // [xsp+48h] [xbp-10D8h]
  __int64 v181; // [xsp+50h] [xbp-10D0h]
  __int64 v182; // [xsp+58h] [xbp-10C8h]
  __int64 v183; // [xsp+60h] [xbp-10C0h]
  __int64 v184; // [xsp+68h] [xbp-10B8h]
  __int64 v185; // [xsp+70h] [xbp-10B0h]
  float *v186; // [xsp+80h] [xbp-10A0h]
  unsigned int v187; // [xsp+88h] [xbp-1098h]
  int v188; // [xsp+8Ch] [xbp-1094h]
  unsigned __int16 *v189; // [xsp+90h] [xbp-1090h]
  unsigned int v190; // [xsp+98h] [xbp-1088h]
  unsigned int v191; // [xsp+9Ch] [xbp-1084h]
  int v192; // [xsp+A0h] [xbp-1080h]
  int v193; // [xsp+A4h] [xbp-107Ch]
  const APACScaleFactorData *v194; // [xsp+A8h] [xbp-1078h]
  unsigned __int16 *v195; // [xsp+B0h] [xbp-1070h]
  _WORD *v196; // [xsp+B8h] [xbp-1068h]
  unsigned __int16 *v197; // [xsp+C0h] [xbp-1060h]
  unsigned int v198; // [xsp+C8h] [xbp-1058h]
  int v199; // [xsp+CCh] [xbp-1054h]
  __int64 v200; // [xsp+D0h] [xbp-1050h]
  unsigned __int64 v201; // [xsp+D8h] [xbp-1048h]
  int v202; // [xsp+E0h] [xbp-1040h]
  int v203; // [xsp+E4h] [xbp-103Ch]
  unsigned int v204; // [xsp+E8h] [xbp-1038h]
  unsigned int v205; // [xsp+ECh] [xbp-1034h]
  char *v206; // [xsp+F0h] [xbp-1030h]
  const APACICSInfo *v207; // [xsp+F8h] [xbp-1028h]
  _BYTE *v208; // [xsp+100h] [xbp-1020h]
  APACSpectralData *v209; // [xsp+108h] [xbp-1018h]
  _BYTE v210[4104]; // [xsp+110h] [xbp-1010h] BYREF
  __int64 v211; // [xsp+1118h] [xbp-8h]
  __int64 v212; // [xsp+1120h] [xbp+0h] BYREF

  v20 = &v212 - 2560;
  v20[512] = 0;
  v20[476] = 0;
  v171 = v15;
  v172 = v16;
  v207 = ics;
  v21 = *((unsigned __int8 *)ics + 11);
  v194 = sfData;
  v209 = this;
  if ( !v21 )
  {
    v168 = *(_DWORD **)this;
    for ( i = *((_DWORD **)this + 1); v168 != i; ++v168 )
      *v168 = 0;
    goto LABEL_36;
  }
  v173 = v5;
  v174 = v6;
  v175 = v7;
  v8 = bs;
  v22 = *(unsigned __int16 *)a;
  v197 = *((unsigned __int16 **)a + 1);
  v23 = *((unsigned __int8 *)v207 + 12);
  v204 = v22;
  v205 = v23;
  v203 = v197[(unsigned __int16)v22];
  if ( !v23 )
  {
    v25 = 0;
    goto LABEL_35;
  }
  v183 = v17;
  v184 = v18;
  v206 = (char *)v194 + 480;
  v208 = v210;
  v24 = 0;
  v25 = 0;
  v177 = v9;
  v178 = v10;
  v179 = v11;
  v180 = v12;
  v181 = v13;
  v182 = v14;
  v201 = 0;
  v202 = 0;
  v185 = v19;
  do
  {
    v26 = 4LL * (unsigned int)(v203 * v202);
    v27 = (unsigned __int8)v206[v201];
    if ( v201 <= 7 )
    {
      v191 = (unsigned __int8)v206[v201];
      v193 = (*((_DWORD *)v207 + 4) >> (4 * v201)) & 0xF;
      if ( v193 == 1 )
      {
        v149 = *(_QWORD *)v209;
        ++v202;
        v28 = (float *)(v149 + v26);
        if ( !v27 )
        {
LABEL_168:
          v188 = 0;
          goto LABEL_30;
        }
        goto LABEL_7;
      }
      v202 += v193;
    }
    else
    {
      v191 = (unsigned __int8)v206[v201];
      v193 = 0;
    }
    v28 = (float *)&v208[v26];
    if ( !v27 )
      goto LABEL_168;
LABEL_7:
    v29 = (unsigned int)(15 * v201);
    v199 = 15 * v201;
    v188 = 0;
    v190 = 0;
    v191 = 0;
    v200 = (__int64)v194 + 2 * (unsigned int)(v27 - 1) + 2 * v29 + 490;
    v196 = (_WORD *)((char *)v194 + 2 * v29 + 488);
    do
    {
      v30 = (unsigned __int16)*v196;
      v31 = (unsigned __int8)*v196;
      v190 += v30 >> 8;
      if ( !v31 )
      {
        v47 = v197[v190] * v193;
        v48 = v47 - v188;
        do
        {
          *(_OWORD *)v28 = 0u;
          v28 += 4;
          v48 -= 4;
        }
        while ( v48 );
        v188 = v47;
        v191 = v190;
        goto LABEL_29;
      }
      HuffmanTable = apac::GetHuffmanTable(v31);
      v33 = *((_DWORD *)HuffmanTable + 2);
      v34 = HuffmanTable;
      v35 = (1 << -(char)v33) - 1;
      if ( v31 > 6 )
      {
        if ( v31 <= 0xA )
        {
          v75 = v33 + 2;
          v76 = v30 & 1;
          v187 = v191 + 1;
          v189 = &v197[v191 + 1];
          v186 = flt_3308C0;
          v192 = v199 - 1;
          while ( 1 )
          {
            v77 = v28 + 2;
            v78 = v188;
            v191 = v187;
            v79 = *((float *)v194 + v192 + v187);
            v188 = *v189 * v193;
            v80 = v188 - v78;
            do
            {
              if ( v25 < v75 )
              {
                if ( v25 > 0x1F )
                  goto LABEL_81;
                cache = v8->cache;
                v82 = v8->bitsLeft - 32 + v25;
                v8->bitsLeft = v82;
                v83 = cache >> v25;
                if ( (v82 & 0x80000000) == 0 )
                {
                  v24 |= v83;
                  v8->cache = cache << -(char)v25;
LABEL_81:
                  v25 = 32;
                  goto LABEL_82;
                }
                TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
                v91 = v8->cache;
                bitsLeft = v8->bitsLeft;
                v25 = 32;
                v93 = v91 >> (bitsLeft + 32);
                v94 = v91 << -(char)bitsLeft;
                v24 |= v93 | v83;
                if ( bitsLeft == -32 )
                  v94 = 0;
                v8->cur += 4;
                v8->cache = v94;
                v8->bitsLeft = bitsLeft + 32;
              }
LABEL_82:
              if ( v76 && (v24 & 0x80000000) == 0 )
              {
                v24 *= 2;
                --v25;
                v28 = v77;
                *(v77 - 2) = 0.0;
                *(v77 - 1) = 0.0;
                goto LABEL_85;
              }
              v84 = __clz(v35 | ~v24);
              v85 = *(unsigned __int16 *)(*(_QWORD *)(*v34 + 16LL * (v84 & 0x3F) + 8)
                                        + 2LL * ((unsigned int)(v24 << v84) >> *(_WORD *)(*v34 + 16LL * (v84 & 0x3F))));
              v25 -= v85 & 0x3F;
              v86 = (v85 >> 11) & 0xF;
              v24 <<= v85 & 0x3F;
              if ( (_DWORD)v86 )
              {
                v87 = v24 < 0;
                --v25;
                v24 *= 2;
                v88 = (v85 >> 6) & 0xF;
                v89 = v186[(unsigned int)v86];
                v28 = v77;
                if ( v87 )
                  v89 = -v89;
                *(v77 - 2) = v89 * v79;
                if ( !(_DWORD)v88 )
                {
LABEL_96:
                  *(v77 - 1) = 0.0;
                  goto LABEL_85;
                }
              }
              else
              {
                *(v77 - 2) = 0.0;
                v88 = (v85 >> 6) & 0xF;
                v28 = v77;
                if ( !(_DWORD)v88 )
                  goto LABEL_96;
              }
              v87 = v24 < 0;
              --v25;
              v24 *= 2;
              v90 = v186[(unsigned int)v88];
              if ( v87 )
                v90 = -v90;
              *(v77 - 1) = v90 * v79;
LABEL_85:
              v77 += 2;
              v80 -= 2;
            }
            while ( v80 );
            ++v189;
            if ( v187 >= v190 )
              goto LABEL_29;
            ++v187;
          }
        }
        if ( v31 != 11 )
          goto LABEL_29;
        LODWORD(v189) = v33 + 2;
        v187 = v191 + 1;
        v186 = flt_3308C0;
        v195 = &v197[v191 + 1];
        v192 = v199 - 1;
        while ( 2 )
        {
          v95 = v188;
          v191 = v187;
          v96 = *((float *)v194 + v192 + v187);
          v188 = *v195 * v193;
          v97 = v188 - v95;
LABEL_103:
          if ( v25 < (unsigned int)v189 )
          {
            if ( v25 <= 0x1F )
            {
              v98 = v8->cache;
              v99 = v8->bitsLeft - 32 + v25;
              v8->bitsLeft = v99;
              v100 = v98 >> v25;
              if ( v99 < 0 )
              {
                TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
                v123 = v8->cache;
                v124 = v8->bitsLeft;
                v125 = v123 >> (v124 + 32);
                v126 = v123 << -(char)v124;
                v24 |= v125 | v100;
                if ( v124 == -32 )
                  v126 = 0;
                v8->cur += 4;
                v8->cache = v126;
                v8->bitsLeft = v124 + 32;
              }
              else
              {
                v24 |= v100;
                v8->cache = v98 << -(char)v25;
              }
            }
            v25 = 32;
          }
          v101 = __clz(v35 | ~v24);
          v102 = *(unsigned __int16 *)(*(_QWORD *)(*v34 + 16LL * (v101 & 0x3F) + 8)
                                     + 2LL * ((unsigned int)(v24 << v101) >> *(_WORD *)(*v34 + 16LL * (v101 & 0x3F))));
          v103 = (unsigned int)v102 >> 11 == 0;
          v25 -= v102 & 0x3F;
          v24 <<= v102 & 0x3F;
          v104 = (unsigned int)v102 >> 11;
          v105 = (v102 >> 6) & 0x1F;
          if ( !v103 )
          {
            --v25;
            __SET_PAIR__(v106, v24, 2LL * (unsigned int)v24);
            v107 = 0;
            if ( (_DWORD)v105 )
              goto LABEL_110;
            goto LABEL_111;
          }
          if ( (_DWORD)v105 )
          {
            v106 = 0;
LABEL_110:
            --v25;
            __SET_PAIR__(v107, v24, 2LL * (unsigned int)v24);
LABEL_111:
            if ( (_DWORD)v104 == 16 )
            {
              if ( v25 <= 0x14 )
              {
                v117 = v8->cache;
                v118 = v8->bitsLeft - 32 + v25;
                v8->bitsLeft = v118;
                v198 = v117 >> v25;
                if ( v118 < 0 )
                {
                  TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
                  v132 = v8->cache;
                  v131 = v8->bitsLeft;
                  v133 = v131 + 32;
                  v103 = v131 == -32;
                  cur = v8->cur;
                  v135 = v132 << -(char)v131;
                  v136 = v198 | (v132 >> v133);
                  if ( v103 )
                    v135 = 0;
                  v8->cache = v135;
                  v8->bitsLeft = v133;
                  v198 = v136;
                  v8->cur = cur + 4;
                }
                else
                {
                  v8->cache = v117 << -(char)v25;
                }
                v25 = 32;
                v24 |= v198;
              }
              v119 = __clz(~v24 | 0x800000);
              v120 = v119 + 1;
              v119 += 4;
              v121 = v24 << v120;
              v25 -= v120 + v119;
              v122 = (1 << v119) + (v121 >> -(char)v119);
              v24 = v121 << v119;
              if ( v122 > 1023 )
                v108 = pow((double)v122, 1.33333333);
              else
                v108 = v186[v122];
            }
            else
            {
              v108 = v186[v104];
            }
            if ( v106 )
              v108 = -v108;
            *v28 = v108 * v96;
            if ( (_DWORD)v105 == 16 )
            {
              if ( v25 <= 0x14 )
              {
                v110 = v8->cache;
                v111 = v8->bitsLeft - 32 + v25;
                v8->bitsLeft = v111;
                v112 = v110 >> v25;
                if ( v111 < 0 )
                {
                  TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
                  v128 = v8->cache;
                  v127 = v8->bitsLeft;
                  v129 = v127 + 32;
                  v103 = v127 == -32;
                  v130 = v128 << -(char)v127;
                  if ( v103 )
                    v130 = 0;
                  v112 |= v128 >> v129;
                  v8->cur += 4;
                  v8->cache = v130;
                  v8->bitsLeft = v129;
                }
                else
                {
                  v8->cache = v110 << -(char)v25;
                }
                v24 |= v112;
                v25 = 32;
              }
              v113 = __clz(~v24 | 0x800000);
              v114 = v113 + 1;
              v113 += 4;
              v115 = v24 << v114;
              v25 -= v114 + v113;
              v116 = (1 << v113) + (v115 >> -(char)v113);
              v24 = v115 << v113;
              if ( v116 > 1023 )
                v109 = pow((double)v116, 1.33333333);
              else
                v109 = v186[v116];
            }
            else
            {
              v109 = v186[(unsigned int)v105];
            }
            if ( v107 )
              v109 = -v109;
          }
          else
          {
            v109 = 0.0;
            *v28 = v96 * 0.0;
          }
          v28 += 2;
          v97 -= 2;
          *(v28 - 1) = v109 * v96;
          if ( !v97 )
          {
            ++v195;
            if ( v187 >= v190 )
              goto LABEL_29;
            ++v187;
            continue;
          }
          goto LABEL_103;
        }
      }
      v36 = v191 + 1;
      if ( v31 > 4 )
      {
        v187 = v191 + 1;
        v137 = v30 & 1;
        v189 = &v197[v36];
        v192 = v199 - 1;
        while ( 1 )
        {
          v138 = v188;
          v191 = v187;
          v139 = *((float *)v194 + v192 + v187);
          v188 = *v189 * v193;
          v140 = v188 - v138;
          do
          {
            if ( v33 > v25 )
            {
              if ( v25 <= 0x1F )
              {
                v141 = v8->cache;
                v142 = v8->bitsLeft - 32 + v25;
                v8->bitsLeft = v142;
                LODWORD(v186) = v141 >> v25;
                if ( v142 < 0 )
                {
                  TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
                  v145 = v8->cache;
                  v146 = v8->bitsLeft;
                  v147 = (v145 >> (v146 + 32)) | v24;
                  v148 = v145 << -(char)v146;
                  if ( v146 == -32 )
                    v148 = 0;
                  v24 = v147 | (unsigned int)v186;
                  v8->cur += 4;
                  v8->cache = v148;
                  v8->bitsLeft = v146 + 32;
                }
                else
                {
                  v24 |= v141 >> v25;
                  v8->cache = v141 << -(char)v25;
                }
              }
              v25 = 32;
            }
            v28 += 2;
            if ( !v137 || v24 < 0 )
            {
              v143 = __clz(v35 | ~v24);
              v144 = *(unsigned __int16 *)(*(_QWORD *)(*v34 + 16LL * (v143 & 0x3F) + 8)
                                         + 2LL
                                         * ((unsigned int)(v24 << v143) >> *(_WORD *)(*v34 + 16LL * (v143 & 0x3F))));
              v25 -= v144 & 0x3F;
              v24 <<= v144 & 0x3F;
              *(v28 - 2) = flt_3308C0[v144 << 49 >> 60] * v139;
              *(v28 - 1) = flt_3308C0[v144 << 54 >> 60] * v139;
            }
            else
            {
              v24 *= 2;
              --v25;
              *(v28 - 2) = 0.0;
              *(v28 - 1) = 0.0;
            }
            v140 -= 2;
          }
          while ( v140 );
          ++v189;
          if ( v187 >= v190 )
            break;
          ++v187;
        }
        goto LABEL_29;
      }
      if ( v31 > 2 )
      {
        v187 = v191 + 1;
        v54 = v33 + 4;
        v55 = v30 & 1;
        v189 = &v197[v36];
        v192 = v199 - 1;
        while ( 1 )
        {
          v56 = v188;
          v191 = v187;
          v57 = *((float *)v194 + v192 + v187);
          v188 = *v189 * v193;
          v58 = v188 - v56;
          do
          {
            if ( v25 < v54 )
            {
              if ( v25 > 0x1F )
                goto LABEL_47;
              v59 = v8->cache;
              v60 = v8->bitsLeft - 32 + v25;
              v8->bitsLeft = v60;
              LODWORD(v186) = v59 >> v25;
              if ( (v60 & 0x80000000) == 0 )
              {
                v24 |= v59 >> v25;
                v8->cache = v59 << -(char)v25;
LABEL_47:
                v25 = 32;
                goto LABEL_48;
              }
              TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
              v71 = v8->cache;
              v72 = v8->bitsLeft;
              v25 = 32;
              v73 = (v71 >> (v72 + 32)) | v24;
              v74 = v71 << -(char)v72;
              if ( v72 == -32 )
                v74 = 0;
              v24 = v73 | (unsigned int)v186;
              v8->cur += 4;
              v8->cache = v74;
              v8->bitsLeft = v72 + 32;
            }
LABEL_48:
            if ( !v55 || v24 < 0 )
            {
              v61.n128_u32[0] = 0;
              v62 = __clz(v35 | ~v24);
              v63 = *(unsigned __int16 *)(*(_QWORD *)(*v34 + 16LL * (v62 & 0x3F) + 8)
                                        + 2LL * ((unsigned int)(v24 << v62) >> *(_WORD *)(*v34 + 16LL * (v62 & 0x3F))));
              v25 -= v63 & 0x3F;
              v24 <<= v63 & 0x3F;
              v64 = (unsigned int)v63 >> 14;
              if ( (unsigned int)v63 >> 14 )
              {
                --v25;
                if ( v24 < 0 )
                  v64 = -v64;
                v24 *= 2;
                v61.n128_f32[0] = flt_3308C0[v64];
              }
              v65 = 0.0;
              v66 = (v63 >> 12) & 3;
              if ( (_DWORD)v66 )
              {
                --v25;
                if ( v24 < 0 )
                  LODWORD(v66) = -(int)v66;
                v24 *= 2;
                v65 = flt_3308C0[(int)v66];
              }
              v67 = 0.0;
              v68 = (v63 >> 10) & 3;
              if ( (_DWORD)v68 )
              {
                --v25;
                if ( v24 < 0 )
                  LODWORD(v68) = -(int)v68;
                v24 *= 2;
                v67 = flt_3308C0[(int)v68];
              }
              v69 = 0.0;
              v70 = (v63 >> 8) & 3;
              if ( (_DWORD)v70 )
              {
                --v25;
                if ( v24 < 0 )
                  LODWORD(v70) = -(int)v70;
                v24 *= 2;
                v69 = flt_3308C0[(int)v70];
              }
              v61.n128_f32[1] = v65;
              v61.n128_u64[1] = __PAIR64__(LODWORD(v69), LODWORD(v67));
              *(float32x4_t *)v28 = vmulq_n_f32(v61, v57);
            }
            else
            {
              v24 *= 2;
              --v25;
              *(_QWORD *)v28 = 0;
              *((_QWORD *)v28 + 1) = 0;
            }
            v28 += 4;
            v58 -= 4;
          }
          while ( v58 );
          ++v189;
          if ( v187 >= v190 )
            goto LABEL_29;
          ++v187;
        }
      }
      v187 = v191 + 1;
      v37 = v30 & 1;
      v189 = &v197[v36];
      v192 = v199 - 1;
      while ( 2 )
      {
        v38 = v188;
        v191 = v187;
        v39 = *((float *)v194 + v192 + v187);
        v188 = *v189 * v193;
        v40 = v188 - v38;
        do
        {
          if ( v33 > v25 )
          {
            if ( v25 > 0x1F )
              goto LABEL_18;
            v41 = v8->cache;
            v42 = v8->bitsLeft - 32 + v25;
            v8->bitsLeft = v42;
            LODWORD(v186) = v41 >> v25;
            if ( (v42 & 0x80000000) == 0 )
            {
              v24 |= v41 >> v25;
              v8->cache = v41 << -(char)v25;
LABEL_18:
              v25 = 32;
              goto LABEL_19;
            }
            TBitstreamReader<unsigned int>::FillCacheFrom(v8, v8->cur);
            v50 = v8->cache;
            v51 = v8->bitsLeft;
            v25 = 32;
            v52 = (v50 >> (v51 + 32)) | v24;
            v53 = v50 << -(char)v51;
            if ( v51 == -32 )
              v53 = 0;
            v24 = v52 | (unsigned int)v186;
            v8->cur += 4;
            v8->cache = v53;
            v8->bitsLeft = v51 + 32;
          }
LABEL_19:
          if ( !v37 || v24 < 0 )
          {
            v44 = __clz(v35 | ~v24);
            v45 = *(unsigned __int16 *)(*(_QWORD *)(*v34 + 16LL * (v44 & 0x3F) + 8)
                                      + 2LL * ((unsigned int)(v24 << v44) >> *(_WORD *)(*v34 + 16LL * (v44 & 0x3F))));
            v25 -= v45 & 0x3F;
            v24 <<= v45 & 0x3F;
            v46.n128_f32[0] = flt_3308C0[(__int64)(__int16)v45 >> 14];
            v46.n128_f32[1] = flt_3308C0[v45 << 50 >> 62];
            v46.n128_f32[2] = flt_3308C0[v45 << 52 >> 62];
            v46.n128_f32[3] = flt_3308C0[v45 << 54 >> 62];
            v43 = vmulq_n_f32(v46, v39);
          }
          else
          {
            v43 = 0u;
            v24 *= 2;
            --v25;
          }
          *(float32x4_t *)v28 = v43;
          v28 += 4;
          v40 -= 4;
        }
        while ( v40 );
        ++v189;
        if ( v187 < v190 )
        {
          ++v187;
          continue;
        }
        break;
      }
LABEL_29:
      ++v196;
    }
    while ( (_WORD *)v200 != v196 );
LABEL_30:
    if ( v204 > v191 )
      memset(v28, 0, 4LL * (unsigned int)(v203 * v193 - v188));
    ++v201;
  }
  while ( v205 > (unsigned int)v201 );
  if ( *((_BYTE *)v207 + 10) == 2 )
  {
    v150 = v204;
    v151 = v197;
    v152 = v205;
    v153 = 0;
    v154 = 0;
    v155 = (__int64)&v197[v204];
    v156 = v207;
    v157 = v209;
    do
    {
      if ( v153 <= 7 )
      {
        v158 = (*((_DWORD *)v156 + 4) >> (4 * v153)) & 0xF;
        if ( ((*((_DWORD *)v156 + 4) >> (4 * v153)) & 0xE) != 0 )
        {
          v159 = 4LL * (unsigned int)(v203 * v154);
          v160 = (_QWORD *)(*(_QWORD *)v157 + v159);
          if ( v158 )
          {
            for ( j = 0; j != v158; ++j )
            {
              if ( v150 )
              {
                v162 = v151;
                do
                {
                  v163 = *v162;
                  v164 = v162[1] - v163;
                  v165 = (__int64 *)&v210[4 * (unsigned int)(v163 * v158)
                                        + 4 * (unsigned __int64)(unsigned int)(j * v164)
                                        + v159];
                  do
                  {
                    v166 = *v165;
                    v167 = v165[1];
                    v165 += 2;
                    *v160 = v166;
                    v160[1] = v167;
                    v160 += 2;
                    v164 -= 4;
                  }
                  while ( v164 );
                  ++v162;
                }
                while ( (unsigned __int16 *)v155 != v162 );
              }
            }
            v154 += j;
          }
        }
        else
        {
          v154 += v158;
        }
      }
      ++v153;
    }
    while ( v152 != v153 );
  }
  v9 = v177;
  v10 = v178;
  v11 = v179;
  v12 = v180;
  v13 = v181;
  v14 = v182;
  v17 = v183;
  v18 = v184;
  v19 = v185;
LABEL_35:
  TBitstreamReader<unsigned int>::PutBack(v8, v25);
  v5 = v173;
  v6 = v174;
  v7 = v175;
LABEL_36:
  if ( v211 != _stack_chk_guard )
  {
    v173 = v5;
    v174 = v6;
    v175 = v7;
    v176 = v8;
    v177 = v9;
    v178 = v10;
    v179 = v11;
    v180 = v12;
    v181 = v13;
    v182 = v14;
    v183 = v17;
    v184 = v18;
    v185 = v19;
  }
  return 0;
}


