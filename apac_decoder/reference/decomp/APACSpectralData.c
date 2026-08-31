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


#if 0 /* disassembly of 0x000ab3b0, for verification */
000AB3B0  e91340d1  SUB             X9, SP, #4,LSL#12
000AB3B4  290540d1  SUB             X9, X9, #1,LSL#12
000AB3B8  3f0108f9  STR             XZR, [X9,#0x1000]
000AB3BC  3f7107f9  STR             XZR, [X9,#0xEE0]
000AB3C0  0c2482d2  MOV             X12, #0x1120
000AB3C4  ff632ccb  SUB             SP, SP, X12
000AB3C8  051c00b0  ADRP            X5, #__stack_chk_guard_ptr@PAGE
000AB3CC  fd7b00a9  STP             X29, X30, [SP,#0x1120+var_1120]
000AB3D0  fd030091  MOV             X29, SP
000AB3D4  a50047f9  LDR             X5, [X5,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000AB3D8  e27f00f9  STR             X2, [SP,#0x1120+var_1028]
000AB3DC  422c4039  LDRB            W2, [X2,#0xB]
000AB3E0  a60040f9  LDR             X6, [X5]
000AB3E4  e68f08f9  STR             X6, [SP,#0x1120+var_8]
000AB3E8  060080d2  MOV             X6, #0
000AB3EC  e35700f9  STR             X3, [SP,#0x1120+var_1078]
000AB3F0  e08700f9  STR             X0, [SP,#0x1120+var_1018]
000AB3F4  62720034  CBZ             W2, loc_AC240
000AB3F8  f35301a9  STP             X19, X20, [SP,#0x1120+var_1110]
000AB3FC  f55b02a9  STP             X21, X22, [SP,#0x1120+var_1100]
000AB400  f60301aa  MOV             X22, X1
000AB404  e17f40f9  LDR             X1, [SP,#0x1120+var_1028]
000AB408  80004079  LDRH            W0, [X4]
000AB40C  820440f9  LDR             X2, [X4,#8]
000AB410  e26300f9  STR             X2, [SP,#0x1120+var_1060]
000AB414  21304039  LDRB            W1, [X1,#0xC]
000AB418  e0071d29  STP             W0, W1, [SP,#0x1120+var_1038]
000AB41C  003c7fd3  UBFIZ           X0, X0, #1, #0x10
000AB420  40686078  LDRH            W0, [X2,X0]
000AB424  e0e700b9  STR             W0, [SP,#0x1120+var_103C]
000AB428  a1710034  CBZ             W1, loc_AC25C
000AB42C  e05740f9  LDR             X0, [SP,#0x1120+var_1078]
000AB430  e827066d  STP             D8, D9, [SP,#0x1120+var_10C0]
000AB434  0804000f  MOVI            V8.2S, #0
000AB438  00800791  ADD             X0, X0, #0x1E0
000AB43C  e07b00f9  STR             X0, [SP,#0x1120+var_1030]
000AB440  e0430491  ADD             X0, SP, #0x1120+var_1010
000AB444  e08300f9  STR             X0, [SP,#0x1120+var_1020]
000AB448  e0f300b2  MOV             X0, #0x5555555555555555
000AB44C  13008052  MOV             W19, #0
000AB450  a0fee7f2  MOVK            X0, #0x3FF5,LSL#48
000AB454  14008052  MOV             W20, #0
000AB458  0900679e  FMOV            D9, X0
000AB45C  f76303a9  STP             X23, X24, [SP,#0x1120+var_10F0]
000AB460  f96b04a9  STP             X25, X26, [SP,#0x1120+var_10E0]
000AB464  fb7305a9  STP             X27, X28, [SP,#0x1120+var_10D0]
000AB468  ff6f00f9  STR             XZR, [SP,#0x1120+var_1048]
000AB46C  ffe300b9  STR             WZR, [SP,#0x1120+var_1040]
000AB470  ea3b00fd  STR             D10, [SP,#0x1120+var_10B0]
000AB474  1f2003d5  NOP
000AB478  e1035c29  LDP             W1, W0, [SP,#0x1120+var_1040]
000AB47C  157c011b  MUL             W21, W0, W1
000AB480  e16f40f9  LDR             X1, [SP,#0x1120+var_1048]
000AB484  e07b40f9  LDR             X0, [SP,#0x1120+var_1030]
000AB488  b5f67ed3  LSL             X21, X21, #2
000AB48C  3f1c00f1  CMP             X1, #7
000AB490  e303012a  MOV             W3, W1
000AB494  00686138  LDRB            W0, [X0,X1]
000AB498  69620054  B.LS            loc_AC0E4
000AB49C  e09f00b9  STR             W0, [SP,#0x1120+var_1084]
000AB4A0  ffa700b9  STR             WZR, [SP,#0x1120+var_107C]
000AB4A4  e18340f9  LDR             X1, [SP,#0x1120+var_1020]
000AB4A8  3500158b  ADD             X21, X1, X21
000AB4AC  c0650034  CBZ             W0, loc_AC164
000AB4B0  e1018052  MOV             W1, #0xF
000AB4B4  00040051  SUB             W0, W0, #1
000AB4B8  e45740f9  LDR             X4, [SP,#0x1120+var_1078]
000AB4BC  617c011b  MUL             W1, W3, W1
000AB4C0  e1cf00b9  STR             W1, [SP,#0x1120+var_1054]
000AB4C4  0000018b  ADD             X0, X0, X1
000AB4C8  82a80791  ADD             X2, X4, #0x1EA
000AB4CC  21d00391  ADD             X1, X1, #0xF4; unsigned int
000AB4D0  ff8f00b9  STR             WZR, [SP,#0x1120+var_1094]
000AB4D4  4004008b  ADD             X0, X2, X0,LSL#1
000AB4D8  ff7f1329  STP             WZR, WZR, [SP,#0x1120+var_1088]
000AB4DC  e06b00f9  STR             X0, [SP,#0x1120+var_1050]
000AB4E0  8004018b  ADD             X0, X4, X1,LSL#1
000AB4E4  e05f00f9  STR             X0, [SP,#0x1120+var_1068]
000AB4E8  e05f40f9  LDR             X0, [SP,#0x1120+var_1068]
000AB4EC  17004079  LDRH            W23, [X0]
000AB4F0  e09b40b9  LDR             W0, [SP,#0x1120+var_1088]
000AB4F4  fa1e0072  ANDS            W26, W23, #0xFF
000AB4F8  0020570b  ADD             W0, W0, W23,LSR#8
000AB4FC  e09b00b9  STR             W0, [SP,#0x1120+var_1088]
000AB500  800c0054  B.EQ            loc_AB690
000AB504  e0031a2a  MOV             W0, W26; this
000AB508  2ebaff97  BL              _ZN4apac15GetHuffmanTableEj; apac::GetHuffmanTable(uint)
000AB50C  180840b9  LDR             W24, [X0,#8]
000AB510  f90300aa  MOV             X25, X0
000AB514  21008052  MOV             W1, #1
000AB518  5f1b0071  CMP             W26, #6
000AB51C  e003184b  NEG             W0, W24
000AB520  2120c01a  LSL             W1, W1, W0
000AB524  3c040051  SUB             W28, W1, #1
000AB528  88240054  B.HI            loc_AB9B8
000AB52C  e19f40b9  LDR             W1, [SP,#0x1120+var_1084]
000AB530  5f130071  CMP             W26, #4
000AB534  20040011  ADD             W0, W1, #1
000AB538  48510054  B.HI            loc_ABF60
000AB53C  5f0b0071  CMP             W26, #2
000AB540  a8130054  B.HI            loc_AB7B4
000AB544  21040011  ADD             W1, W1, #1
000AB548  e18b00b9  STR             W1, [SP,#0x1120+var_1098]
000AB54C  e16340f9  LDR             X1, [SP,#0x1120+var_1060]
000AB550  3b1400b0  ADRL            X27, unk_3308C0
000AB558  f7020012  AND             W23, W23, #1
000AB55C  2004008b  ADD             X0, X1, X0,LSL#1
000AB560  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000AB564  e0cf40b9  LDR             W0, [SP,#0x1120+var_1054]
000AB568  00040051  SUB             W0, W0, #1
000AB56C  e0a300b9  STR             W0, [SP,#0x1120+var_1080]
000AB570  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000AB574  e26b5129  LDP             W2, W26, [SP,#0x1120+var_1098]
000AB578  e29f00b9  STR             W2, [SP,#0x1120+var_1084]
000AB57C  01004079  LDRH            W1, [X0]
000AB580  e0a340b9  LDR             W0, [SP,#0x1120+var_1080]
000AB584  0000020b  ADD             W0, W0, W2
000AB588  e25740f9  LDR             X2, [SP,#0x1120+var_1078]
000AB58C  4a7860bc  LDR             S10, [X2,X0,LSL#2]
000AB590  e0a740b9  LDR             W0, [SP,#0x1120+var_107C]
000AB594  207c001b  MUL             W0, W1, W0
000AB598  e08f00b9  STR             W0, [SP,#0x1120+var_1094]
000AB59C  1a001a4b  SUB             W26, W0, W26
000AB5A0  1f03146b  CMP             W24, W20
000AB5A4  e9010054  B.LS            loc_AB5E0
000AB5A8  9f7e0071  CMP             W20, #0x1F
000AB5AC  88010054  B.HI            loc_AB5DC
000AB5B0  c1024229  LDP             W1, W0, [X22,#0x10]
000AB5B4  00800051  SUB             W0, W0, #0x20 ; ' '
000AB5B8  0000140b  ADD             W0, W0, W20
000AB5BC  2224d41a  LSR             W2, W1, W20
000AB5C0  c01600b9  STR             W0, [X22,#0x14]
000AB5C4  e28300b9  STR             W2, [SP,#0x1120+var_10A0]
000AB5C8  200df837  TBNZ            W0, #0x1F, loc_AB76C
000AB5CC  f403144b  NEG             W20, W20
000AB5D0  7302022a  ORR             W19, W19, W2
000AB5D4  2120d41a  LSL             W1, W1, W20
000AB5D8  c11200b9  STR             W1, [X22,#0x10]
000AB5DC  14048052  MOV             W20, #0x20 ; ' '
000AB5E0  57020034  CBZ             W23, loc_AB628
000AB5E4  3302f837  TBNZ            W19, #0x1F, loc_AB628
000AB5E8  0004004f  MOVI            V0.4S, #0
000AB5EC  7302130b  ADD             W19, W19, W19
000AB5F0  94060051  SUB             W20, W20, #1
000AB5F4  a006813c  STR             Q0, [X21],#0x10
000AB5F8  5a130071  SUBS            W26, W26, #4
000AB5FC  21fdff54  B.NE            loc_AB5A0
000AB600  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000AB604  e19b40b9  LDR             W1, [SP,#0x1120+var_1088]
000AB608  00080091  ADD             X0, X0, #2
000AB60C  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000AB610  e08b40b9  LDR             W0, [SP,#0x1120+var_1098]
000AB614  1f00016b  CMP             W0, W1
000AB618  82050054  B.CS            loc_AB6C8
000AB61C  00040011  ADD             W0, W0, #1
000AB620  e08b00b9  STR             W0, [SP,#0x1120+var_1098]
000AB624  d3ffff17  B               loc_AB570
000AB628  8003332a  ORN             W0, W28, W19
000AB62C  0010c05a  CLZ             W0, W0
000AB630  220340f9  LDR             X2, [X25]
000AB634  01147cd3  UBFIZ           X1, X0, #4, #6
000AB638  6022c01a  LSL             W0, W19, W0
000AB63C  4300018b  ADD             X3, X2, X1
000AB640  42686178  LDRH            W2, [X2,X1]
000AB644  610440f9  LDR             X1, [X3,#8]
000AB648  0024c21a  LSR             W0, W0, W2
000AB64C  20786078  LDRH            W0, [X1,X0,LSL#1]
000AB650  01140012  AND             W1, W0, #0x3F ; '?'
000AB654  043c4e93  SBFX            X4, X0, #0xE, #2
000AB658  03344c93  SBFX            X3, X0, #0xC, #2
000AB65C  022c4a93  SBFX            X2, X0, #0xA, #2
000AB660  00244893  SBFX            X0, X0, #8, #2
000AB664  9402014b  SUB             W20, W20, W1
000AB668  7322c11a  LSL             W19, W19, W1
000AB66C  607b64bc  LDR             S0, [X27,X4,LSL#2]
000AB670  637b63bc  LDR             S3, [X27,X3,LSL#2]
000AB674  627b62bc  LDR             S2, [X27,X2,LSL#2]
000AB678  617b60bc  LDR             S1, [X27,X0,LSL#2]
000AB67C  60040c6e  INS             V0.S[1], V3.S[0]
000AB680  4004146e  INS             V0.S[2], V2.S[0]
000AB684  20041c6e  INS             V0.S[3], V1.S[0]
000AB688  00908a4f  FMUL            V0.4S, V0.4S, V10.S[0]
000AB68C  daffff17  B               loc_AB5F4
000AB690  e19b40b9  LDR             W1, [SP,#0x1120+var_1088]
000AB694  e06340f9  LDR             X0, [SP,#0x1120+var_1060]
000AB698  0004004f  MOVI            V0.4S, #0
000AB69C  01586178  LDRH            W1, [X0,W1,UXTW#1]
000AB6A0  e0a740b9  LDR             W0, [SP,#0x1120+var_107C]
000AB6A4  217c001b  MUL             W1, W1, W0
000AB6A8  e08f40b9  LDR             W0, [SP,#0x1120+var_1094]
000AB6AC  2000004b  SUB             W0, W1, W0
000AB6B0  a006813c  STR             Q0, [X21],#0x10
000AB6B4  00100071  SUBS            W0, W0, #4
000AB6B8  c1ffff54  B.NE            loc_AB6B0
000AB6BC  e09b40b9  LDR             W0, [SP,#0x1120+var_1088]
000AB6C0  e18f00b9  STR             W1, [SP,#0x1120+var_1094]
000AB6C4  e09f00b9  STR             W0, [SP,#0x1120+var_1084]
000AB6C8  e05f40f9  LDR             X0, [SP,#0x1120+var_1068]
000AB6CC  e16b40f9  LDR             X1, [SP,#0x1120+var_1050]
000AB6D0  00080091  ADD             X0, X0, #2
000AB6D4  e05f00f9  STR             X0, [SP,#0x1120+var_1068]
000AB6D8  3f0000eb  CMP             X1, X0
000AB6DC  61f0ff54  B.NE            loc_AB4E8
000AB6E0  e19f40b9  LDR             W1, [SP,#0x1120+var_1084]
000AB6E4  e0eb40b9  LDR             W0, [SP,#0x1120+var_1038]
000AB6E8  1f00016b  CMP             W0, W1
000AB6EC  a8510054  B.HI            loc_AC120
000AB6F0  e06f40f9  LDR             X0, [SP,#0x1120+var_1048]
000AB6F4  e1ef40b9  LDR             W1, [SP,#0x1120+var_1034]
000AB6F8  00040091  ADD             X0, X0, #1
000AB6FC  e06f00f9  STR             X0, [SP,#0x1120+var_1048]
000AB700  3f00006b  CMP             W1, W0
000AB704  a8ebff54  B.HI            loc_AB478
000AB708  e07f40f9  LDR             X0, [SP,#0x1120+var_1028]
000AB70C  00284039  LDRB            W0, [X0,#0xA]
000AB710  1f080071  CMP             W0, #2
000AB714  c0520054  B.EQ            loc_AC16C
000AB718  f76343a9  LDP             X23, X24, [SP,#0x1120+var_10F0]
000AB71C  f96b44a9  LDP             X25, X26, [SP,#0x1120+var_10E0]
000AB720  fb7345a9  LDP             X27, X28, [SP,#0x1120+var_10D0]
000AB724  e827466d  LDP             D8, D9, [SP,#0x1120+var_10C0]
000AB728  ea3b40fd  LDR             D10, [SP,#0x1120+var_10B0]
000AB72C  e103142a  MOV             W1, W20
000AB730  e00316aa  MOV             X0, X22
000AB734  77180094  BL              _ZN16TBitstreamReaderIjE7PutBackEj; TBitstreamReader<uint>::PutBack(uint)
000AB738  f35341a9  LDP             X19, X20, [SP,#0x1120+var_1110]
000AB73C  f55b42a9  LDP             X21, X22, [SP,#0x1120+var_1100]
000AB740  001c00b0  ADRP            X0, #__stack_chk_guard_ptr@PAGE
000AB744  000047f9  LDR             X0, [X0,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000AB748  e18f48f9  LDR             X1, [SP,#0x1120+var_8]
000AB74C  000040f9  LDR             X0, [X0]
000AB750  200000ca  EOR             X0, X1, X0
000AB754  805800b5  CBNZ            X0, loc_AC264
000AB758  00008052  MOV             W0, #0
000AB75C  0c2482d2  MOV             X12, #0x1120
000AB760  fd7b40a9  LDP             X29, X30, [SP,#0x1120+var_1120]
000AB764  ff632c8b  ADD             SP, SP, X12
000AB768  c0035fd6  RET
000AB76C  c10240f9  LDR             X1, [X22]
000AB770  e00316aa  MOV             X0, X22
000AB774  a3170094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000AB778  c0064229  LDP             W0, W1, [X22,#0x10]
000AB77C  14048052  MOV             W20, #0x20 ; ' '
000AB780  c30240f9  LDR             X3, [X22]
000AB784  22800031  ADDS            W2, W1, #0x20 ; ' '
000AB788  e103014b  NEG             W1, W1
000AB78C  0424c21a  LSR             W4, W0, W2
000AB790  9300132a  ORR             W19, W4, W19
000AB794  e48340b9  LDR             W4, [SP,#0x1120+var_10A0]
000AB798  0020c11a  LSL             W0, W0, W1
000AB79C  61100091  ADD             X1, X3, #4
000AB7A0  e003801a  CSEL            W0, WZR, W0, EQ
000AB7A4  7302042a  ORR             W19, W19, W4
000AB7A8  c10200f9  STR             X1, [X22]
000AB7AC  c00a0229  STP             W0, W2, [X22,#0x10]
000AB7B0  8cffff17  B               loc_AB5E0
000AB7B4  21040011  ADD             W1, W1, #1
000AB7B8  e18b00b9  STR             W1, [SP,#0x1120+var_1098]
000AB7BC  e16340f9  LDR             X1, [SP,#0x1120+var_1060]
000AB7C0  3b1400b0  ADRL            X27, unk_3308C0
000AB7C8  18130011  ADD             W24, W24, #4
000AB7CC  f7020012  AND             W23, W23, #1
000AB7D0  2004008b  ADD             X0, X1, X0,LSL#1
000AB7D4  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000AB7D8  e0cf40b9  LDR             W0, [SP,#0x1120+var_1054]
000AB7DC  00040051  SUB             W0, W0, #1
000AB7E0  e0a300b9  STR             W0, [SP,#0x1120+var_1080]
000AB7E4  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000AB7E8  e26b5129  LDP             W2, W26, [SP,#0x1120+var_1098]
000AB7EC  e29f00b9  STR             W2, [SP,#0x1120+var_1084]
000AB7F0  01004079  LDRH            W1, [X0]
000AB7F4  e0a340b9  LDR             W0, [SP,#0x1120+var_1080]
000AB7F8  0000020b  ADD             W0, W0, W2
000AB7FC  e25740f9  LDR             X2, [SP,#0x1120+var_1078]
000AB800  4a7860bc  LDR             S10, [X2,X0,LSL#2]
000AB804  e0a740b9  LDR             W0, [SP,#0x1120+var_107C]
000AB808  207c001b  MUL             W0, W1, W0
000AB80C  e08f00b9  STR             W0, [SP,#0x1120+var_1094]
000AB810  1a001a4b  SUB             W26, W0, W26
000AB814  1f2003d5  NOP
000AB818  9f02186b  CMP             W20, W24
000AB81C  e2010054  B.CS            loc_AB858
000AB820  9f7e0071  CMP             W20, #0x1F
000AB824  88010054  B.HI            loc_AB854
000AB828  c1024229  LDP             W1, W0, [X22,#0x10]
000AB82C  00800051  SUB             W0, W0, #0x20 ; ' '
000AB830  0000140b  ADD             W0, W0, W20
000AB834  2224d41a  LSR             W2, W1, W20
000AB838  c01600b9  STR             W0, [X22,#0x14]
000AB83C  e28300b9  STR             W2, [SP,#0x1120+var_10A0]
000AB840  8009f837  TBNZ            W0, #0x1F, loc_AB970
000AB844  f403144b  NEG             W20, W20
000AB848  7302022a  ORR             W19, W19, W2
000AB84C  2120d41a  LSL             W1, W1, W20
000AB850  c11200b9  STR             W1, [X22,#0x10]
000AB854  14048052  MOV             W20, #0x20 ; ' '
000AB858  57020034  CBZ             W23, loc_AB8A0
000AB85C  3302f837  TBNZ            W19, #0x1F, loc_AB8A0
000AB860  7302130b  ADD             W19, W19, W19
000AB864  94060051  SUB             W20, W20, #1
000AB868  bf7e00a9  STP             XZR, XZR, [X21]
000AB86C  b5420091  ADD             X21, X21, #0x10
000AB870  5a130071  SUBS            W26, W26, #4
000AB874  21fdff54  B.NE            loc_AB818
000AB878  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000AB87C  e19b40b9  LDR             W1, [SP,#0x1120+var_1088]
000AB880  00080091  ADD             X0, X0, #2
000AB884  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000AB888  e08b40b9  LDR             W0, [SP,#0x1120+var_1098]
000AB88C  1f00016b  CMP             W0, W1
000AB890  c2f1ff54  B.CS            loc_AB6C8
000AB894  00040011  ADD             W0, W0, #1
000AB898  e08b00b9  STR             W0, [SP,#0x1120+var_1098]
000AB89C  d2ffff17  B               loc_AB7E4
000AB8A0  8003332a  ORN             W0, W28, W19
000AB8A4  0004000f  MOVI            V0.2S, #0
000AB8A8  0010c05a  CLZ             W0, W0
000AB8AC  220340f9  LDR             X2, [X25]
000AB8B0  01147cd3  UBFIZ           X1, X0, #4, #6
000AB8B4  6022c01a  LSL             W0, W19, W0
000AB8B8  4300018b  ADD             X3, X2, X1
000AB8BC  42686178  LDRH            W2, [X2,X1]
000AB8C0  610440f9  LDR             X1, [X3,#8]
000AB8C4  0024c21a  LSR             W0, W0, W2
000AB8C8  20786078  LDRH            W0, [X1,X0,LSL#1]
000AB8CC  01140012  AND             W1, W0, #0x3F ; '?'
000AB8D0  9402014b  SUB             W20, W20, W1
000AB8D4  ff3b406b  CMP             WZR, W0,LSR#14
000AB8D8  7322c11a  LSL             W19, W19, W1
000AB8DC  017c0e53  LSR             W1, W0, #0xE
000AB8E0  c0000054  B.EQ            loc_AB8F8
000AB8E4  7f020071  CMP             W19, #0
000AB8E8  94060051  SUB             W20, W20, #1
000AB8EC  21a4815a  CNEG            W1, W1, LT
000AB8F0  7302130b  ADD             W19, W19, W19
000AB8F4  60db61bc  LDR             S0, [X27,W1,SXTW#2]
000AB8F8  0304000f  MOVI            V3.2S, #0
000AB8FC  01344cd3  UBFX            X1, X0, #0xC, #2
000AB900  c1000034  CBZ             W1, loc_AB918
000AB904  7f020071  CMP             W19, #0
000AB908  94060051  SUB             W20, W20, #1
000AB90C  21a4815a  CNEG            W1, W1, LT
000AB910  7302130b  ADD             W19, W19, W19
000AB914  63db61bc  LDR             S3, [X27,W1,SXTW#2]
000AB918  0204000f  MOVI            V2.2S, #0
000AB91C  012c4ad3  UBFX            X1, X0, #0xA, #2
000AB920  c1000034  CBZ             W1, loc_AB938
000AB924  7f020071  CMP             W19, #0
000AB928  94060051  SUB             W20, W20, #1
000AB92C  21a4815a  CNEG            W1, W1, LT
000AB930  7302130b  ADD             W19, W19, W19
000AB934  62db61bc  LDR             S2, [X27,W1,SXTW#2]
000AB938  0104000f  MOVI            V1.2S, #0
000AB93C  002448d3  UBFX            X0, X0, #8, #2
000AB940  c0000034  CBZ             W0, loc_AB958
000AB944  7f020071  CMP             W19, #0
000AB948  94060051  SUB             W20, W20, #1
000AB94C  00a4805a  CNEG            W0, W0, LT
000AB950  7302130b  ADD             W19, W19, W19
000AB954  61db60bc  LDR             S1, [X27,W0,SXTW#2]
000AB958  60040c6e  INS             V0.S[1], V3.S[0]
000AB95C  4004146e  INS             V0.S[2], V2.S[0]
000AB960  20041c6e  INS             V0.S[3], V1.S[0]
000AB964  00908a4f  FMUL            V0.4S, V0.4S, V10.S[0]
000AB968  a002803d  STR             Q0, [X21]
000AB96C  c0ffff17  B               loc_AB86C
000AB970  c10240f9  LDR             X1, [X22]
000AB974  e00316aa  MOV             X0, X22
000AB978  22170094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000AB97C  c0064229  LDP             W0, W1, [X22,#0x10]
000AB980  14048052  MOV             W20, #0x20 ; ' '
000AB984  c30240f9  LDR             X3, [X22]
000AB988  22800031  ADDS            W2, W1, #0x20 ; ' '
000AB98C  e103014b  NEG             W1, W1
000AB990  0424c21a  LSR             W4, W0, W2
000AB994  9300132a  ORR             W19, W4, W19
000AB998  e48340b9  LDR             W4, [SP,#0x1120+var_10A0]
000AB99C  0020c11a  LSL             W0, W0, W1
000AB9A0  61100091  ADD             X1, X3, #4
000AB9A4  e003801a  CSEL            W0, WZR, W0, EQ
000AB9A8  7302042a  ORR             W19, W19, W4
000AB9AC  c10200f9  STR             X1, [X22]
000AB9B0  c00a0229  STP             W0, W2, [X22,#0x10]
000AB9B4  a9ffff17  B               loc_AB858
000AB9B8  5f2b0071  CMP             W26, #0xA
000AB9BC  e80f0054  B.HI            loc_ABBB8
000AB9C0  e19f40b9  LDR             W1, [SP,#0x1120+var_1084]
000AB9C4  1b0b0011  ADD             W27, W24, #2
000AB9C8  f8020012  AND             W24, W23, #1
000AB9CC  20040011  ADD             W0, W1, #1
000AB9D0  21040011  ADD             W1, W1, #1
000AB9D4  e18b00b9  STR             W1, [SP,#0x1120+var_1098]
000AB9D8  e16340f9  LDR             X1, [SP,#0x1120+var_1060]
000AB9DC  2004008b  ADD             X0, X1, X0,LSL#1
000AB9E0  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000AB9E4  201400b0  ADRL            X0, unk_3308C0
000AB9EC  e04300f9  STR             X0, [SP,#0x1120+var_10A0]
000AB9F0  e0cf40b9  LDR             W0, [SP,#0x1120+var_1054]
000AB9F4  00040051  SUB             W0, W0, #1
000AB9F8  e0a300b9  STR             W0, [SP,#0x1120+var_1080]
000AB9FC  1f2003d5  NOP
000ABA00  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000ABA04  b7220091  ADD             X23, X21, #8
000ABA08  e26b5129  LDP             W2, W26, [SP,#0x1120+var_1098]
000ABA0C  e29f00b9  STR             W2, [SP,#0x1120+var_1084]
000ABA10  01004079  LDRH            W1, [X0]
000ABA14  e0a340b9  LDR             W0, [SP,#0x1120+var_1080]
000ABA18  0000020b  ADD             W0, W0, W2
000ABA1C  e25740f9  LDR             X2, [SP,#0x1120+var_1078]
000ABA20  4a7860bc  LDR             S10, [X2,X0,LSL#2]
000ABA24  e0a740b9  LDR             W0, [SP,#0x1120+var_107C]
000ABA28  207c001b  MUL             W0, W1, W0
000ABA2C  e08f00b9  STR             W0, [SP,#0x1120+var_1094]
000ABA30  1a001a4b  SUB             W26, W0, W26
000ABA34  1f2003d5  NOP
000ABA38  9f021b6b  CMP             W20, W27
000ABA3C  c2010054  B.CS            loc_ABA74
000ABA40  9f7e0071  CMP             W20, #0x1F
000ABA44  68010054  B.HI            loc_ABA70
000ABA48  c1024229  LDP             W1, W0, [X22,#0x10]
000ABA4C  00800051  SUB             W0, W0, #0x20 ; ' '
000ABA50  0000140b  ADD             W0, W0, W20
000ABA54  c01600b9  STR             W0, [X22,#0x14]
000ABA58  3524d41a  LSR             W21, W1, W20
000ABA5C  c008f837  TBNZ            W0, #0x1F, loc_ABB74
000ABA60  f403144b  NEG             W20, W20
000ABA64  7302152a  ORR             W19, W19, W21
000ABA68  2120d41a  LSL             W1, W1, W20
000ABA6C  c11200b9  STR             W1, [X22,#0x10]
000ABA70  14048052  MOV             W20, #0x20 ; ' '
000ABA74  78020034  CBZ             W24, loc_ABAC0
000ABA78  5302f837  TBNZ            W19, #0x1F, loc_ABAC0
000ABA7C  7302130b  ADD             W19, W19, W19
000ABA80  94060051  SUB             W20, W20, #1
000ABA84  f50317aa  MOV             X21, X23
000ABA88  ff7e3f29  STP             WZR, WZR, [X23,#-8]
000ABA8C  f7220091  ADD             X23, X23, #8
000ABA90  5a0b0071  SUBS            W26, W26, #2
000ABA94  21fdff54  B.NE            loc_ABA38
000ABA98  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000ABA9C  e19b40b9  LDR             W1, [SP,#0x1120+var_1088]
000ABAA0  00080091  ADD             X0, X0, #2
000ABAA4  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000ABAA8  e08b40b9  LDR             W0, [SP,#0x1120+var_1098]
000ABAAC  1f00016b  CMP             W0, W1
000ABAB0  c2e0ff54  B.CS            loc_AB6C8
000ABAB4  00040011  ADD             W0, W0, #1
000ABAB8  e08b00b9  STR             W0, [SP,#0x1120+var_1098]
000ABABC  d1ffff17  B               loc_ABA00
000ABAC0  8003332a  ORN             W0, W28, W19
000ABAC4  0010c05a  CLZ             W0, W0
000ABAC8  220340f9  LDR             X2, [X25]
000ABACC  01147cd3  UBFIZ           X1, X0, #4, #6
000ABAD0  6022c01a  LSL             W0, W19, W0
000ABAD4  4300018b  ADD             X3, X2, X1
000ABAD8  42686178  LDRH            W2, [X2,X1]
000ABADC  610440f9  LDR             X1, [X3,#8]
000ABAE0  0024c21a  LSR             W0, W0, W2
000ABAE4  20786078  LDRH            W0, [X1,X0,LSL#1]
000ABAE8  02140012  AND             W2, W0, #0x3F ; '?'
000ABAEC  9402024b  SUB             W20, W20, W2
000ABAF0  01384bd3  UBFX            X1, X0, #0xB, #4
000ABAF4  7322c21a  LSL             W19, W19, W2
000ABAF8  21030034  CBZ             W1, loc_ABB5C
000ABAFC  e24340f9  LDR             X2, [SP,#0x1120+var_10A0]
000ABB00  e103012a  MOV             W1, W1
000ABB04  7f020071  CMP             W19, #0
000ABB08  94060051  SUB             W20, W20, #1
000ABB0C  7302130b  ADD             W19, W19, W19
000ABB10  002446d3  UBFX            X0, X0, #6, #4
000ABB14  407861bc  LDR             S0, [X2,X1,LSL#2]
000ABB18  f50317aa  MOV             X21, X23
000ABB1C  0140211e  FNEG            S1, S0
000ABB20  20bc201e  FCSEL           S0, S1, S0, LT
000ABB24  00082a1e  FMUL            S0, S0, S10
000ABB28  e0821fbc  STUR            S0, [X23,#-8]
000ABB2C  00020034  CBZ             W0, loc_ABB6C
000ABB30  e14340f9  LDR             X1, [SP,#0x1120+var_10A0]
000ABB34  e003002a  MOV             W0, W0
000ABB38  7f020071  CMP             W19, #0
000ABB3C  94060051  SUB             W20, W20, #1
000ABB40  7302130b  ADD             W19, W19, W19
000ABB44  207860bc  LDR             S0, [X1,X0,LSL#2]
000ABB48  0140211e  FNEG            S1, S0
000ABB4C  20bc201e  FCSEL           S0, S1, S0, LT
000ABB50  00082a1e  FMUL            S0, S0, S10
000ABB54  e0c21fbc  STUR            S0, [X23,#-4]
000ABB58  cdffff17  B               loc_ABA8C
000ABB5C  ff821fb8  STUR            WZR, [X23,#-8]
000ABB60  002446d3  UBFX            X0, X0, #6, #4
000ABB64  f50317aa  MOV             X21, X23
000ABB68  40feff35  CBNZ            W0, loc_ABB30
000ABB6C  ffc21fb8  STUR            WZR, [X23,#-4]
000ABB70  c7ffff17  B               loc_ABA8C
000ABB74  c10240f9  LDR             X1, [X22]
000ABB78  e00316aa  MOV             X0, X22
000ABB7C  a1160094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000ABB80  c0064229  LDP             W0, W1, [X22,#0x10]
000ABB84  14048052  MOV             W20, #0x20 ; ' '
000ABB88  c30240f9  LDR             X3, [X22]
000ABB8C  22800031  ADDS            W2, W1, #0x20 ; ' '
000ABB90  e103014b  NEG             W1, W1
000ABB94  0424c21a  LSR             W4, W0, W2
000ABB98  9300132a  ORR             W19, W4, W19
000ABB9C  0020c11a  LSL             W0, W0, W1
000ABBA0  7302152a  ORR             W19, W19, W21
000ABBA4  e003801a  CSEL            W0, WZR, W0, EQ
000ABBA8  61100091  ADD             X1, X3, #4
000ABBAC  c10200f9  STR             X1, [X22]
000ABBB0  c00a0229  STP             W0, W2, [X22,#0x10]
000ABBB4  b0ffff17  B               loc_ABA74
000ABBB8  5f2f0071  CMP             W26, #0xB
000ABBBC  61d8ff54  B.NE            loc_AB6C8
000ABBC0  e19f40b9  LDR             W1, [SP,#0x1120+var_1084]
000ABBC4  020b0011  ADD             W2, W24, #2
000ABBC8  e29300b9  STR             W2, [SP,#0x1120+var_1090]
000ABBCC  20040011  ADD             W0, W1, #1
000ABBD0  21040011  ADD             W1, W1, #1
000ABBD4  e18b00b9  STR             W1, [SP,#0x1120+var_1098]
000ABBD8  211400b0  ADRL            X1, unk_3308C0
000ABBE0  e14300f9  STR             X1, [SP,#0x1120+var_10A0]
000ABBE4  e16340f9  LDR             X1, [SP,#0x1120+var_1060]
000ABBE8  2004008b  ADD             X0, X1, X0,LSL#1
000ABBEC  e05b00f9  STR             X0, [SP,#0x1120+var_1070]
000ABBF0  e0cf40b9  LDR             W0, [SP,#0x1120+var_1054]
000ABBF4  00040051  SUB             W0, W0, #1
000ABBF8  e0a300b9  STR             W0, [SP,#0x1120+var_1080]
000ABBFC  1f2003d5  NOP
000ABC00  e05b40f9  LDR             X0, [SP,#0x1120+var_1070]
000ABC04  e2635129  LDP             W2, W24, [SP,#0x1120+var_1098]
000ABC08  e29f00b9  STR             W2, [SP,#0x1120+var_1084]
000ABC0C  01004079  LDRH            W1, [X0]
000ABC10  e0a340b9  LDR             W0, [SP,#0x1120+var_1080]
000ABC14  0000020b  ADD             W0, W0, W2
000ABC18  e25740f9  LDR             X2, [SP,#0x1120+var_1078]
000ABC1C  4a7860bc  LDR             S10, [X2,X0,LSL#2]
000ABC20  e0a740b9  LDR             W0, [SP,#0x1120+var_107C]
000ABC24  207c001b  MUL             W0, W1, W0
000ABC28  e08f00b9  STR             W0, [SP,#0x1120+var_1094]
000ABC2C  1800184b  SUB             W24, W0, W24
000ABC30  e09340b9  LDR             W0, [SP,#0x1120+var_1090]
000ABC34  9f02006b  CMP             W20, W0
000ABC38  c2010054  B.CS            loc_ABC70
000ABC3C  9f7e0071  CMP             W20, #0x1F
000ABC40  68010054  B.HI            loc_ABC6C
000ABC44  c1024229  LDP             W1, W0, [X22,#0x10]
000ABC48  00800051  SUB             W0, W0, #0x20 ; ' '
000ABC4C  0000140b  ADD             W0, W0, W20
000ABC50  c01600b9  STR             W0, [X22,#0x14]
000ABC54  3724d41a  LSR             W23, W1, W20
000ABC58  4012f837  TBNZ            W0, #0x1F, loc_ABEA0
000ABC5C  f403144b  NEG             W20, W20
000ABC60  7302172a  ORR             W19, W19, W23
000ABC64  2120d41a  LSL             W1, W1, W20
000ABC68  c11200b9  STR             W1, [X22,#0x10]
000ABC6C  14048052  MOV             W20, #0x20 ; ' '
000ABC70  8003332a  ORN             W0, W28, W19
000ABC74  0010c05a  CLZ             W0, W0
000ABC78  220340f9  LDR             X2, [X25]
000ABC7C  01147cd3  UBFIZ           X1, X0, #4, #6
000ABC80  6022c01a  LSL             W0, W19, W0
000ABC84  4300018b  ADD             X3, X2, X1
000ABC88  42686178  LDRH            W2, [X2,X1]
000ABC8C  610440f9  LDR             X1, [X3,#8]
000ABC90  0024c21a  LSR             W0, W0, W2
000ABC94  37786078  LDRH            W23, [X1,X0,LSL#1]
000ABC98  e0160012  AND             W0, W23, #0x3F ; '?'
000ABC9C  ff2f576b  CMP             WZR, W23,LSR#11
000ABCA0  9402004b  SUB             W20, W20, W0
000ABCA4  7322c01a  LSL             W19, W19, W0
000ABCA8  e07e0b53  LSR             W0, W23, #0xB
000ABCAC  f72a46d3  UBFX            X23, X23, #6, #5
000ABCB0  20050054  B.EQ            loc_ABD54
000ABCB4  7b7e1f53  LSR             W27, W19, #0x1F
000ABCB8  94060051  SUB             W20, W20, #1
000ABCBC  7302130b  ADD             W19, W19, W19
000ABCC0  1a008052  MOV             W26, #0
000ABCC4  97000034  CBZ             W23, loc_ABCD4
000ABCC8  7a7e1f53  LSR             W26, W19, #0x1F
000ABCCC  94060051  SUB             W20, W20, #1
000ABCD0  7302130b  ADD             W19, W19, W19
000ABCD4  1f400071  CMP             W0, #0x10
000ABCD8  80080054  B.EQ            loc_ABDE8
000ABCDC  e14340f9  LDR             X1, [SP,#0x1120+var_10A0]
000ABCE0  207860bc  LDR             S0, [X1,X0,LSL#2]
000ABCE4  0140211e  FNEG            S1, S0
000ABCE8  7f030071  CMP             W27, #0
000ABCEC  201c201e  FCSEL           S0, S1, S0, NE
000ABCF0  ff420071  CMP             W23, #0x10
000ABCF4  00082a1e  FMUL            S0, S0, S10
000ABCF8  a00200bd  STR             S0, [X21]
000ABCFC  60030054  B.EQ            loc_ABD68
000ABD00  e04340f9  LDR             X0, [SP,#0x1120+var_10A0]
000ABD04  f703172a  MOV             W23, W23
000ABD08  007877bc  LDR             S0, [X0,X23,LSL#2]
000ABD0C  0140211e  FNEG            S1, S0
000ABD10  5f030071  CMP             W26, #0
000ABD14  201c201e  FCSEL           S0, S1, S0, NE
000ABD18  00082a1e  FMUL            S0, S0, S10
000ABD1C  b5220091  ADD             X21, X21, #8
000ABD20  180b0071  SUBS            W24, W24, #2
000ABD24  a0c21fbc  STUR            S0, [X21,#-4]
000ABD28  41f8ff54  B.NE            loc_ABC30
000ABD2C  e05b40f9  LDR             X0, [SP,#0x1120+var_1070]
000ABD30  e19b40b9  LDR             W1, [SP,#0x1120+var_1088]
000ABD34  00080091  ADD             X0, X0, #2
000ABD38  e05b00f9  STR             X0, [SP,#0x1120+var_1070]
000ABD3C  e08b40b9  LDR             W0, [SP,#0x1120+var_1098]
000ABD40  1f00016b  CMP             W0, W1
000ABD44  22ccff54  B.CS            loc_AB6C8
000ABD48  00040011  ADD             W0, W0, #1
000ABD4C  e08b00b9  STR             W0, [SP,#0x1120+var_1098]
000ABD50  acffff17  B               loc_ABC00
000ABD54  f7080035  CBNZ            W23, loc_ABE70
000ABD58  4109281e  FMUL            S1, S10, S8
000ABD5C  0004000f  MOVI            V0.2S, #0
000ABD60  a10200bd  STR             S1, [X21]
000ABD64  edffff17  B               loc_ABD18
000ABD68  9f520071  CMP             W20, #0x14
000ABD6C  88010054  B.HI            loc_ABD9C
000ABD70  c1024229  LDP             W1, W0, [X22,#0x10]
000ABD74  00800051  SUB             W0, W0, #0x20 ; ' '
000ABD78  0000140b  ADD             W0, W0, W20
000ABD7C  c01600b9  STR             W0, [X22,#0x14]
000ABD80  3724d41a  LSR             W23, W1, W20
000ABD84  e00af837  TBNZ            W0, #0x1F, loc_ABEE0
000ABD88  f403144b  NEG             W20, W20
000ABD8C  2120d41a  LSL             W1, W1, W20
000ABD90  c11200b9  STR             W1, [X22,#0x10]
000ABD94  7302172a  ORR             W19, W19, W23
000ABD98  14048052  MOV             W20, #0x20 ; ' '
000ABD9C  e003332a  MVN             W0, W19
000ABDA0  21008052  MOV             W1, #1
000ABDA4  00000932  ORR             W0, W0, #0x800000
000ABDA8  0010c05a  CLZ             W0, W0
000ABDAC  0200010b  ADD             W2, W0, W1
000ABDB0  00100011  ADD             W0, W0, #4
000ABDB4  e303004b  NEG             W3, W0
000ABDB8  7322c21a  LSL             W19, W19, W2
000ABDBC  4200000b  ADD             W2, W2, W0
000ABDC0  6326c31a  LSR             W3, W19, W3
000ABDC4  9402024b  SUB             W20, W20, W2
000ABDC8  2120c01a  LSL             W1, W1, W0
000ABDCC  2100030b  ADD             W1, W1, W3
000ABDD0  3ffc0f71  CMP             W1, #0x3FF
000ABDD4  7322c01a  LSL             W19, W19, W0
000ABDD8  0c050054  B.GT            loc_ABE78
000ABDDC  e04340f9  LDR             X0, [SP,#0x1120+var_10A0]
000ABDE0  00d861bc  LDR             S0, [X0,W1,SXTW#2]
000ABDE4  caffff17  B               loc_ABD0C
000ABDE8  9f520071  CMP             W20, #0x14
000ABDEC  c8010054  B.HI            loc_ABE24
000ABDF0  c1024229  LDP             W1, W0, [X22,#0x10]
000ABDF4  00800051  SUB             W0, W0, #0x20 ; ' '
000ABDF8  0000140b  ADD             W0, W0, W20
000ABDFC  2224d41a  LSR             W2, W1, W20
000ABE00  c01600b9  STR             W0, [X22,#0x14]
000ABE04  e2cb00b9  STR             W2, [SP,#0x1120+var_1058]
000ABE08  a008f837  TBNZ            W0, #0x1F, loc_ABF1C
000ABE0C  f403144b  NEG             W20, W20
000ABE10  2120d41a  LSL             W1, W1, W20
000ABE14  c11200b9  STR             W1, [X22,#0x10]
000ABE18  e0cb40b9  LDR             W0, [SP,#0x1120+var_1058]
000ABE1C  14048052  MOV             W20, #0x20 ; ' '
000ABE20  7302002a  ORR             W19, W19, W0
000ABE24  e003332a  MVN             W0, W19
000ABE28  21008052  MOV             W1, #1
000ABE2C  00000932  ORR             W0, W0, #0x800000
000ABE30  0010c05a  CLZ             W0, W0
000ABE34  0200010b  ADD             W2, W0, W1
000ABE38  00100011  ADD             W0, W0, #4
000ABE3C  e303004b  NEG             W3, W0
000ABE40  7322c21a  LSL             W19, W19, W2
000ABE44  4200000b  ADD             W2, W2, W0
000ABE48  6326c31a  LSR             W3, W19, W3
000ABE4C  9402024b  SUB             W20, W20, W2
000ABE50  2120c01a  LSL             W1, W1, W0
000ABE54  2100030b  ADD             W1, W1, W3
000ABE58  3ffc0f71  CMP             W1, #0x3FF
000ABE5C  7322c01a  LSL             W19, W19, W0
000ABE60  6c010054  B.GT            loc_ABE8C
000ABE64  e04340f9  LDR             X0, [SP,#0x1120+var_10A0]
000ABE68  00d861bc  LDR             S0, [X0,W1,SXTW#2]
000ABE6C  9effff17  B               loc_ABCE4
000ABE70  1b008052  MOV             W27, #0
000ABE74  95ffff17  B               loc_ABCC8
000ABE78  2000621e  SCVTF           D0, W1; x
000ABE7C  2141601e  FMOV            D1, D9; y
000ABE80  2412ff97  BL              .pow
000ABE84  0040621e  FCVT            S0, D0
000ABE88  a1ffff17  B               loc_ABD0C
000ABE8C  2000621e  SCVTF           D0, W1; x
000ABE90  2141601e  FMOV            D1, D9; y
000ABE94  1f12ff97  BL              .pow
000ABE98  0040621e  FCVT            S0, D0
000ABE9C  92ffff17  B               loc_ABCE4
000ABEA0  c10240f9  LDR             X1, [X22]
000ABEA4  e00316aa  MOV             X0, X22
000ABEA8  d6150094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000ABEAC  c0064229  LDP             W0, W1, [X22,#0x10]
000ABEB0  c30240f9  LDR             X3, [X22]
000ABEB4  22800031  ADDS            W2, W1, #0x20 ; ' '
000ABEB8  e103014b  NEG             W1, W1
000ABEBC  0424c21a  LSR             W4, W0, W2
000ABEC0  9300132a  ORR             W19, W4, W19
000ABEC4  0020c11a  LSL             W0, W0, W1
000ABEC8  7302172a  ORR             W19, W19, W23
000ABECC  e003801a  CSEL            W0, WZR, W0, EQ
000ABED0  61100091  ADD             X1, X3, #4
000ABED4  c10200f9  STR             X1, [X22]
000ABED8  c00a0229  STP             W0, W2, [X22,#0x10]
000ABEDC  64ffff17  B               loc_ABC6C
000ABEE0  c10240f9  LDR             X1, [X22]
000ABEE4  e00316aa  MOV             X0, X22
000ABEE8  c6150094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000ABEEC  c1024229  LDP             W1, W0, [X22,#0x10]
000ABEF0  c30240f9  LDR             X3, [X22]
000ABEF4  02800031  ADDS            W2, W0, #0x20 ; ' '
000ABEF8  e003004b  NEG             W0, W0
000ABEFC  2020c01a  LSL             W0, W1, W0
000ABF00  e003801a  CSEL            W0, WZR, W0, EQ
000ABF04  2124c21a  LSR             W1, W1, W2
000ABF08  f702012a  ORR             W23, W23, W1
000ABF0C  61100091  ADD             X1, X3, #4
000ABF10  c10200f9  STR             X1, [X22]
000ABF14  c00a0229  STP             W0, W2, [X22,#0x10]
000ABF18  9fffff17  B               loc_ABD94
000ABF1C  c10240f9  LDR             X1, [X22]
000ABF20  e00316aa  MOV             X0, X22
000ABF24  b7150094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000ABF28  c1024229  LDP             W1, W0, [X22,#0x10]
000ABF2C  e4cb40b9  LDR             W4, [SP,#0x1120+var_1058]
000ABF30  02800031  ADDS            W2, W0, #0x20 ; ' '
000ABF34  e003004b  NEG             W0, W0
000ABF38  c30240f9  LDR             X3, [X22]
000ABF3C  2020c01a  LSL             W0, W1, W0
000ABF40  2124c21a  LSR             W1, W1, W2
000ABF44  8100012a  ORR             W1, W4, W1
000ABF48  e003801a  CSEL            W0, WZR, W0, EQ
000ABF4C  c00a0229  STP             W0, W2, [X22,#0x10]
000ABF50  e1cb00b9  STR             W1, [SP,#0x1120+var_1058]
000ABF54  61100091  ADD             X1, X3, #4
000ABF58  c10200f9  STR             X1, [X22]
000ABF5C  afffff17  B               loc_ABE18
000ABF60  21040011  ADD             W1, W1, #1
000ABF64  e18b00b9  STR             W1, [SP,#0x1120+var_1098]
000ABF68  e16340f9  LDR             X1, [SP,#0x1120+var_1060]
000ABF6C  3b1400b0  ADRL            X27, unk_3308C0
000ABF74  f7020012  AND             W23, W23, #1
000ABF78  2004008b  ADD             X0, X1, X0,LSL#1
000ABF7C  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000ABF80  e0cf40b9  LDR             W0, [SP,#0x1120+var_1054]
000ABF84  00040051  SUB             W0, W0, #1
000ABF88  e0a300b9  STR             W0, [SP,#0x1120+var_1080]
000ABF8C  1f2003d5  NOP
000ABF90  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000ABF94  e26b5129  LDP             W2, W26, [SP,#0x1120+var_1098]
000ABF98  e29f00b9  STR             W2, [SP,#0x1120+var_1084]
000ABF9C  01004079  LDRH            W1, [X0]
000ABFA0  e0a340b9  LDR             W0, [SP,#0x1120+var_1080]
000ABFA4  0000020b  ADD             W0, W0, W2
000ABFA8  e25740f9  LDR             X2, [SP,#0x1120+var_1078]
000ABFAC  4a7860bc  LDR             S10, [X2,X0,LSL#2]
000ABFB0  e0a740b9  LDR             W0, [SP,#0x1120+var_107C]
000ABFB4  207c001b  MUL             W0, W1, W0
000ABFB8  e08f00b9  STR             W0, [SP,#0x1120+var_1094]
000ABFBC  1a001a4b  SUB             W26, W0, W26
000ABFC0  1f03146b  CMP             W24, W20
000ABFC4  e9010054  B.LS            loc_AC000
000ABFC8  9f7e0071  CMP             W20, #0x1F
000ABFCC  88010054  B.HI            loc_ABFFC
000ABFD0  c1024229  LDP             W1, W0, [X22,#0x10]
000ABFD4  00800051  SUB             W0, W0, #0x20 ; ' '
000ABFD8  0000140b  ADD             W0, W0, W20
000ABFDC  2224d41a  LSR             W2, W1, W20
000ABFE0  c01600b9  STR             W0, [X22,#0x14]
000ABFE4  e28300b9  STR             W2, [SP,#0x1120+var_10A0]
000ABFE8  c005f837  TBNZ            W0, #0x1F, loc_AC0A0
000ABFEC  f403144b  NEG             W20, W20
000ABFF0  7302022a  ORR             W19, W19, W2
000ABFF4  2120d41a  LSL             W1, W1, W20
000ABFF8  c11200b9  STR             W1, [X22,#0x10]
000ABFFC  14048052  MOV             W20, #0x20 ; ' '
000AC000  b5220091  ADD             X21, X21, #8
000AC004  37020034  CBZ             W23, loc_AC048
000AC008  1302f837  TBNZ            W19, #0x1F, loc_AC048
000AC00C  7302130b  ADD             W19, W19, W19
000AC010  94060051  SUB             W20, W20, #1
000AC014  bf7e3f29  STP             WZR, WZR, [X21,#-8]
000AC018  5a0b0071  SUBS            W26, W26, #2
000AC01C  21fdff54  B.NE            loc_ABFC0
000AC020  e04b40f9  LDR             X0, [SP,#0x1120+var_1090]
000AC024  e19b40b9  LDR             W1, [SP,#0x1120+var_1088]
000AC028  00080091  ADD             X0, X0, #2
000AC02C  e04b00f9  STR             X0, [SP,#0x1120+var_1090]
000AC030  e08b40b9  LDR             W0, [SP,#0x1120+var_1098]
000AC034  1f00016b  CMP             W0, W1
000AC038  82b4ff54  B.CS            loc_AB6C8
000AC03C  00040011  ADD             W0, W0, #1
000AC040  e08b00b9  STR             W0, [SP,#0x1120+var_1098]
000AC044  d3ffff17  B               loc_ABF90
000AC048  8003332a  ORN             W0, W28, W19
000AC04C  0010c05a  CLZ             W0, W0
000AC050  220340f9  LDR             X2, [X25]
000AC054  01147cd3  UBFIZ           X1, X0, #4, #6
000AC058  6022c01a  LSL             W0, W19, W0
000AC05C  4300018b  ADD             X3, X2, X1
000AC060  42686178  LDRH            W2, [X2,X1]
000AC064  610440f9  LDR             X1, [X3,#8]
000AC068  0024c21a  LSR             W0, W0, W2
000AC06C  20786078  LDRH            W0, [X1,X0,LSL#1]
000AC070  01140012  AND             W1, W0, #0x3F ; '?'
000AC074  02384b93  SBFX            X2, X0, #0xB, #4
000AC078  00244693  SBFX            X0, X0, #6, #4
000AC07C  9402014b  SUB             W20, W20, W1
000AC080  7322c11a  LSL             W19, W19, W1
000AC084  607b62bc  LDR             S0, [X27,X2,LSL#2]
000AC088  00082a1e  FMUL            S0, S0, S10
000AC08C  a0821fbc  STUR            S0, [X21,#-8]
000AC090  607b60bc  LDR             S0, [X27,X0,LSL#2]
000AC094  00082a1e  FMUL            S0, S0, S10
000AC098  a0c21fbc  STUR            S0, [X21,#-4]
000AC09C  dfffff17  B               loc_AC018
000AC0A0  c10240f9  LDR             X1, [X22]
000AC0A4  e00316aa  MOV             X0, X22
000AC0A8  56150094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000AC0AC  c0064229  LDP             W0, W1, [X22,#0x10]
000AC0B0  c30240f9  LDR             X3, [X22]
000AC0B4  22800031  ADDS            W2, W1, #0x20 ; ' '
000AC0B8  e103014b  NEG             W1, W1
000AC0BC  0424c21a  LSR             W4, W0, W2
000AC0C0  9300132a  ORR             W19, W4, W19
000AC0C4  e48340b9  LDR             W4, [SP,#0x1120+var_10A0]
000AC0C8  0020c11a  LSL             W0, W0, W1
000AC0CC  61100091  ADD             X1, X3, #4
000AC0D0  e003801a  CSEL            W0, WZR, W0, EQ
000AC0D4  7302042a  ORR             W19, W19, W4
000AC0D8  c10200f9  STR             X1, [X22]
000AC0DC  c00a0229  STP             W0, W2, [X22,#0x10]
000AC0E0  c7ffff17  B               loc_ABFFC
000AC0E4  e17f40f9  LDR             X1, [SP,#0x1120+var_1028]
000AC0E8  e09f00b9  STR             W0, [SP,#0x1120+var_1084]
000AC0EC  e2db40b9  LDR             W2, [SP,#0x1120+var_1048]
000AC0F0  211040b9  LDR             W1, [X1,#0x10]
000AC0F4  42741e53  LSL             W2, W2, #2
000AC0F8  2124c21a  LSR             W1, W1, W2
000AC0FC  210c0012  AND             W1, W1, #0xF
000AC100  e1a700b9  STR             W1, [SP,#0x1120+var_107C]
000AC104  3f040071  CMP             W1, #1
000AC108  00020054  B.EQ            loc_AC148
000AC10C  e1e340b9  LDR             W1, [SP,#0x1120+var_1040]
000AC110  e2a740b9  LDR             W2, [SP,#0x1120+var_107C]
000AC114  2100020b  ADD             W1, W1, W2
000AC118  e1e300b9  STR             W1, [SP,#0x1120+var_1040]
000AC11C  e2fcff17  B               loc_AB4A4
000AC120  e1a740b9  LDR             W1, [SP,#0x1120+var_107C]
000AC124  e0e740b9  LDR             W0, [SP,#0x1120+var_103C]
000AC128  e38f40b9  LDR             W3, [SP,#0x1120+var_1094]
000AC12C  027c011b  MUL             W2, W0, W1
000AC130  e00315aa  MOV             X0, X21; s
000AC134  01008052  MOV             W1, #0; c
000AC138  4200034b  SUB             W2, W2, W3
000AC13C  42f47ed3  LSL             X2, X2, #2; n
000AC140  f80eff97  BL              .memset
000AC144  6bfdff17  B               loc_AB6F0
000AC148  e18740f9  LDR             X1, [SP,#0x1120+var_1018]
000AC14C  e2e340b9  LDR             W2, [SP,#0x1120+var_1040]
000AC150  210040f9  LDR             X1, [X1]
000AC154  42040011  ADD             W2, W2, #1
000AC158  e2e300b9  STR             W2, [SP,#0x1120+var_1040]
000AC15C  3500158b  ADD             X21, X1, X21
000AC160  809aff35  CBNZ            W0, loc_AB4B0
000AC164  ff8f00b9  STR             WZR, [SP,#0x1120+var_1094]
000AC168  5efdff17  B               loc_AB6E0
000AC16C  eeeb40b9  LDR             W14, [SP,#0x1120+var_1038]
000AC170  ea430491  ADD             X10, SP, #0x1120+var_1010
000AC174  ed6340f9  LDR             X13, [SP,#0x1120+var_1060]
000AC178  c9050051  SUB             W9, W14, #1
000AC17C  ef0301aa  MOV             X15, X1
000AC180  0b008052  MOV             W11, #0
000AC184  a0090091  ADD             X0, X13, #2
000AC188  0c008052  MOV             W12, #0
000AC18C  0944298b  ADD             X9, X0, W9,UXTW#1
000AC190  f07f40f9  LDR             X16, [SP,#0x1120+var_1028]
000AC194  f18740f9  LDR             X17, [SP,#0x1120+var_1018]
000AC198  05000014  B               loc_AC1AC
000AC19C  8c01060b  ADD             W12, W12, W6
000AC1A0  6b050011  ADD             W11, W11, #1
000AC1A4  ff010b6b  CMP             W15, W11
000AC1A8  80abff54  B.EQ            loc_AB718
000AC1AC  7f1d0071  CMP             W11, #7
000AC1B0  88ffff54  B.HI            loc_AC1A0
000AC1B4  061240b9  LDR             W6, [X16,#0x10]
000AC1B8  60751e53  LSL             W0, W11, #2
000AC1BC  c624c01a  LSR             W6, W6, W0
000AC1C0  df081f72  TST             W6, #0xE
000AC1C4  c60c0012  AND             W6, W6, #0xF
000AC1C8  a0feff54  B.EQ            loc_AC19C
000AC1CC  e0e740b9  LDR             W0, [SP,#0x1120+var_103C]
000AC1D0  220240f9  LDR             X2, [X17]
000AC1D4  087c0c1b  MUL             W8, W0, W12
000AC1D8  08f57ed3  LSL             X8, X8, #2
000AC1DC  4200088b  ADD             X2, X2, X8
000AC1E0  06feff34  CBZ             W6, loc_AC1A0
000AC1E4  07008052  MOV             W7, #0
000AC1E8  2e020034  CBZ             W14, loc_AC22C
000AC1EC  e3030daa  MOV             X3, X13
000AC1F0  61004079  LDRH            W1, [X3]
000AC1F4  60044079  LDRH            W0, [X3,#2]
000AC1F8  0000014b  SUB             W0, W0, W1
000AC1FC  217c061b  MUL             W1, W1, W6
000AC200  e47c001b  MUL             W4, W7, W0
000AC204  2100048b  ADD             X1, X1, X4
000AC208  0109018b  ADD             X1, X8, X1,LSL#2
000AC20C  4101018b  ADD             X1, X10, X1
000AC210  2414c1a8  LDP             X4, X5, [X1],#0x10
000AC214  441481a8  STP             X4, X5, [X2],#0x10
000AC218  00100071  SUBS            W0, W0, #4
000AC21C  a1ffff54  B.NE            loc_AC210
000AC220  63080091  ADD             X3, X3, #2
000AC224  3f0103eb  CMP             X9, X3
000AC228  41feff54  B.NE            loc_AC1F0
000AC22C  e7040011  ADD             W7, W7, #1
000AC230  ff00066b  CMP             W7, W6
000AC234  a1fdff54  B.NE            loc_AC1E8
000AC238  8c01070b  ADD             W12, W12, W7
000AC23C  d9ffff17  B               loc_AC1A0
000AC240  000440a9  LDP             X0, X1, [X0]
000AC244  3f0000eb  CMP             X1, X0
000AC248  c0a7ff54  B.EQ            loc_AB740
000AC24C  1f4400b8  STR             WZR, [X0],#4
000AC250  1f0001eb  CMP             X0, X1
000AC254  c1ffff54  B.NE            loc_AC24C
000AC258  3afdff17  B               loc_AB740
000AC25C  14008052  MOV             W20, #0
000AC260  33fdff17  B               loc_AB72C
000AC264  f35301a9  STP             X19, X20, [SP,#0x1120+var_1110]
000AC268  f55b02a9  STP             X21, X22, [SP,#0x1120+var_1100]
000AC26C  f76303a9  STP             X23, X24, [SP,#0x1120+var_10F0]
000AC270  f96b04a9  STP             X25, X26, [SP,#0x1120+var_10E0]
000AC274  fb7305a9  STP             X27, X28, [SP,#0x1120+var_10D0]
000AC278  e827066d  STP             D8, D9, [SP,#0x1120+var_10C0]
000AC27C  ea3b00fd  STR             D10, [SP,#0x1120+var_10B0]
000AC280  240eff97  BL              .__stack_chk_fail
000AC284  e103142a  MOV             W1, W20
000AC288  f30300aa  MOV             X19, X0
000AC28C  e00316aa  MOV             X0, X22
000AC290  a0150094  BL              _ZN16TBitstreamReaderIjE7PutBackEj; TBitstreamReader<uint>::PutBack(uint)
000AC294  e00313aa  MOV             X0, X19; struct _Unwind_Exception *
000AC298  2a11ff97  BL              ._Unwind_Resume
#endif

