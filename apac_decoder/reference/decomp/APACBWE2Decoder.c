/* ============================================================================
 * APACBWE2Decoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 7 function(s), 6592 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00078490   1484  APACBWE2Decoder::DecodeFrame(APACICSInfo const&, APACSpectralData&, unsigned int) */
/* 0x00078a60   1896  APACBWE2Decoder::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, APACSyntacticElement&) */
/* 0x000791d0   1180  APACBWE2Decoder::InitInterFrameMemory(unsigned int) */
/* 0x00079670    192  APACBWE2Decoder::Initialize(unsigned int) */
/* 0x00079730     92  APACBWE2Decoder::GetActiveFlag(unsigned int) const */
/* 0x000799c0    888  APACBWE2Decoder::~APACBWE2Decoder() */
/* 0x00079ff0    860  APACBWE2Decoder::~APACBWE2Decoder() */

/* ------------------------------------------------------------------------ */
/* 0x00078490  size 1484 bytes  370 insns
 *   APACBWE2Decoder::DecodeFrame(APACICSInfo const&, APACSpectralData&, unsigned int)
 *   [_ZN15APACBWE2Decoder11DecodeFrameERK11APACICSInfoR16APACSpectralDataj]
 *   data refs: 0x30ec80, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACBWE2Decoder::DecodeFrame(
        APACBWE2Decoder *this,
        const APACICSInfo *ics,
        APACSpectralData *spectrum,
        unsigned int n)
{
  __int64 v4; // x29
  __int64 v5; // x30
  int v10; // w5
  int v11; // w2
  __int64 v12; // x1
  unsigned int v13; // w0
  int v14; // w24
  unsigned int v15; // w8
  unsigned int v16; // w22
  unsigned int v17; // w27
  __int64 v18; // x26
  bool v19; // zf
  __int64 v21; // x3
  __int64 v22; // x4
  __int64 v23; // x5
  __int64 v24; // x6
  __int64 v25; // x7
  float *v26; // x21
  float *v27; // x22
  int v28; // w21
  __int64 v29; // x0
  __int64 v30; // x7
  __int64 v31; // x28
  __int64 v32; // x7
  unsigned __int64 v33; // x5
  unsigned int v34; // w24
  __int64 v35; // x0
  char *v36; // x22
  __int64 v37; // x20
  float *v38; // x28
  __int64 v39; // x3
  float *v40; // x1
  __int64 v41; // x0
  __int64 v42; // x3
  __int64 v43; // x2
  float *v44; // x20
  __int64 v45; // x27
  unsigned int v46; // w19
  float *v47; // x0
  __int64 v49; // x20
  APACSpectralData *v50; // x19
  __int64 v51; // x2
  const float **v52; // x0
  __int64 v53; // x2
  __int64 v54; // x7
  int v55; // w4
  APACSpectralData *v56; // x27
  int v57; // w20
  APACSpectralData *v58; // x19
  int v59; // w26
  __int64 v60; // x0
  __int64 v61; // x20
  int v62; // w24
  int v63; // w2
  int v64; // w28
  unsigned int v65; // w21
  __int64 v66; // x20
  unsigned int v67; // w20
  __int64 v68; // x3
  unsigned int v69; // w0
  __int64 v70; // [xsp+0h] [xbp-10E0h]
  __int64 v71; // [xsp+8h] [xbp-10D8h]
  unsigned int v72[2]; // [xsp+68h] [xbp-1078h]
  __int64 v73; // [xsp+70h] [xbp-1070h]
  __int64 v74; // [xsp+78h] [xbp-1068h]
  unsigned int v75[2]; // [xsp+80h] [xbp-1060h]
  APACSpectralData *v76; // [xsp+88h] [xbp-1058h]
  float *v77; // [xsp+90h] [xbp-1050h]
  __int64 v78; // [xsp+98h] [xbp-1048h]
  _DWORD *v79; // [xsp+A0h] [xbp-1040h]
  int *v80; // [xsp+A8h] [xbp-1038h]
  __int64 v81; // [xsp+B0h] [xbp-1030h]
  float *v82; // [xsp+B8h] [xbp-1028h]
  float v83; // [xsp+C0h] [xbp-1020h] BYREF
  int v84; // [xsp+C4h] [xbp-101Ch] BYREF
  unsigned __int16 v85; // [xsp+C8h] [xbp-1018h] BYREF
  __int16 v86; // [xsp+CAh] [xbp-1016h]
  float v87[1026]; // [xsp+D0h] [xbp-1010h] BYREF
  __int64 v88; // [xsp+10E0h] [xbp+0h]
  v70 = v4;
  v71 = v5;
  v10 = *((unsigned __int8 *)ics + 11);
  v11 = *((unsigned __int8 *)ics + 10);
  v12 = *(_QWORD *)(*((_QWORD *)this + 10) + 16LL);
  if ( v11 == 2 )
    v13 = 8 * *(unsigned __int16 *)(*(_QWORD *)(v12 + 32) + 2LL * (unsigned __int8)v10);
  else
    v13 = *(unsigned __int16 *)(*(_QWORD *)(v12 + 16) + 2LL * (unsigned __int8)v10);
  if ( v13 >= 0x180 )
    v14 = 2;
  else
    v14 = 4;
  if ( v13 >= 0x180 )
    v15 = 384;
  else
    v15 = 256;
  if ( v13 >= 0x180 )
    v16 = 32;
  else
    v16 = 16;
  if ( v13 >= 0x180 )
    v17 = 256;
  else
    v17 = 128;
  v18 = n;
  v19 = ((1LL << n) & *(_QWORD *)(*((_QWORD *)this + 1) + 8LL * (n >> 6))) == 0 || v10 == 0;
  if ( !v19 )
  {
    v72[0] = v15;
    v83 = 1.0;
    vDSP_vfill(&v83, v87, 1, 1024, 0);
    *(_QWORD *)v75 = 8LL * n;
    v73 = *((_QWORD *)this + 11);
    if ( *((_BYTE *)ics + 10) == 2 )
    {
      v39 = v72[0] >> 3;
      v75[0] = v72[0] >> 3;
      v40 = *(float **)spectrum;
      v41 = v39 & 0x1FFFFFFF;
      v78 = v39;
      v42 = -4 * v39;
      v43 = 0;
      v74 = (__int64)this;
      v44 = &v40[v41];
      v45 = v42;
      v76 = spectrum;
      v77 = v40;
      v46 = 0;
      *(_QWORD *)v72 = v41 * 4;
      do
      {
        v47 = sub_78370((float *)((char *)v44 + v45), v44, v43, v42, v22, v23, v24, v25, v70);
        v43 = v46 + 1;
        v42 = (__int64)v44;
        v46 = (unsigned __int16)(v46 + 1);
        v44 += 128;
      }
      while ( v46 <= 7 && v47 == (float *)v42 );
      v49 = v74;
      v50 = v76;
      if ( v47 != (float *)v42 )
      {
        v51 = *((_QWORD *)v76 + 1);
        v74 = v75[0] - v16;
        BWELPC::MDCTCoefficientsToLPC(
          *(BWELPC **)(v73 + 8LL * n),
          v77,
          (unsigned __int64)(v51 - (_QWORD)v77) >> 5,
          v75[0],
          8u);
        LODWORD(v77) = 0;
        LODWORD(v79) = v75[0];
        v52 = *(const float ***)(v73 + 8LL * n);
        v76 = (APACSpectralData *)&v87[v74];
        BWELPC::LPCToMDCTSpectrum((BWELPC *)v52, v52[3], v87, v75[0]);
        v53 = *(_QWORD *)(v49 + 72);
        v85 = *(_DWORD *)(v53 + 8LL * n);
        v86 = *(_DWORD *)(v53 + 8LL * (n & 0x7FFFFFFF) + 4);
        BWELPC::DecodeLSFsToLPCs(*(BWELPC **)(v73 + 8LL * n), &v85);
        BWELPC::LPCToMDCTSpectrum(
          *(BWELPC **)(v73 + 8LL * n),
          *(const float **)(*(_QWORD *)(v73 + 8LL * n) + 48LL),
          (float *)((char *)v87 + *(_QWORD *)v72),
          0x40u);
        v73 = v49;
        v55 = (int)v77;
        v77 = (float *)ics;
        v56 = v50;
        v57 = (int)v79;
        v58 = v76;
        do
        {
          v59 = v55;
          vDSP_vdiv(&v87[v57], 1, v58, 1, &v87[v57], 1, v16, v54, v70, v71);
          v55 = v59 + 1;
          v57 += v16;
        }
        while ( v14 != v59 + 1 );
        v61 = v73;
        v60 = v74;
        v62 = 0;
        LODWORD(v73) = 0;
        LODWORD(v74) = 0;
        v81 = v61;
        v82 = v77;
        v63 = *((_DWORD *)v77 + 4);
        LODWORD(v77) = 8 * n;
        v78 = 4 * (v60 - v78);
        LODWORD(v76) = v63 & 0xF;
        v79 = dword_30EC80;
        v80 = &v84;
        do
        {
          v64 = 0;
          v65 = v75[0];
          v66 = v78 + *(_QWORD *)v72;
          v84 = v79[*(int *)(*(_QWORD *)(v81 + 56) + 4LL * (unsigned int)((_DWORD)v77 + v74))];
          do
          {
            vDSP_vmul(*(_QWORD *)v56 + v66, 1, &v87[v65], 1, *(_QWORD *)v56 + 4LL * (v62 + v65));
            v19 = v59 == v64;
            v65 += v16;
            ++v64;
          }
          while ( !v19 );
          v68 = *(_QWORD *)v56 + *(_QWORD *)v72;
          LODWORD(v73) = v73 + 1;
          v67 = v73;
          vDSP_vsmul(v68, 1, v80, v68, 1, 64);
          if ( v67 >= (unsigned int)v76 )
          {
            v69 = v74 + 1;
            LODWORD(v74) = v69;
            LODWORD(v73) = 0;
            if ( v69 > 7 )
              LODWORD(v76) = 0;
            else
              LODWORD(v76) = (*((_DWORD *)v82 + 4) >> (4 * v69)) & 0xF;
          }
          v62 += 128;
          *(_QWORD *)v72 += 512LL;
        }
        while ( v62 != 1024 );
      }
    }
    else
    {
      v26 = *(float **)spectrum;
      v74 = 4LL * v72[0];
      v27 = (float *)((char *)v26 + v74);
      v76 = (APACSpectralData *)v72[0];
      if ( v27 != sub_78370(v26, (float *)((char *)v26 + v74), v74, v21, v22, v72[0], v24, v25, v70) )
      {
        BWELPC::MDCTCoefficientsToLPC(
          *(BWELPC **)(v73 + 8LL * n),
          v26,
          (*((_QWORD *)spectrum + 1) - (_QWORD)v26) >> 2,
          v72[0],
          1u);
        v28 = 0;
        BWELPC::LPCToMDCTSpectrum(
          *(BWELPC **)(v73 + 8LL * n),
          *(const float **)(*(_QWORD *)(v73 + 8LL * n) + 24LL),
          v87,
          v72[0]);
        v29 = *((_QWORD *)this + 9);
        v30 = v29 + 8LL * (n & 0x7FFFFFFF);
        v85 = *(_DWORD *)(v29 + 8LL * n);
        v31 = v73;
        v86 = *(_DWORD *)(v30 + 4);
        BWELPC::DecodeLSFsToLPCs(*(BWELPC **)(v73 + 8 * v18), &v85);
        BWELPC::LPCToMDCTSpectrum(
          *(BWELPC **)(v31 + 8 * v18),
          *(const float **)(*(_QWORD *)(v31 + 8 * v18) + 48LL),
          (float *)((char *)v87 + v74),
          0x200u);
        LODWORD(v73) = v14;
        v33 = (unsigned __int64)v76;
        v34 = v72[0];
        v35 = *(int *)(*((_QWORD *)this + 7) + 4LL * v75[0]);
        *(_QWORD *)v72 = 4LL * (v72[0] - v17);
        v36 = (char *)v87 + *(_QWORD *)v72;
        v84 = dword_30EC80[v35];
        while ( 1 )
        {
          v37 = 4 * v33;
          v38 = &v87[v33];
          vDSP_vdiv(v38, 1, v36, 1, v38, 1, v17, v32, v70, v71);
          vDSP_vmul(*(_QWORD *)spectrum + *(_QWORD *)v72, 1, v38, 1, *(_QWORD *)spectrum + v37);
          ++v28;
          v34 += v17;
          if ( (_DWORD)v73 == v28 )
            break;
          v33 = v34;
        }
        vDSP_vsmul(*(_QWORD *)spectrum + v74, 1, &v84, *(_QWORD *)spectrum + v74, 1, 512);
      }
    }
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x00078a60  size 1896 bytes  474 insns
 *   APACBWE2Decoder::Deserialize(TBitstreamReader<unsigned int>&, unsigned int, APACSyntacticElement&)
 *   [_ZN15APACBWE2Decoder11DeserializeER16TBitstreamReaderIjEjR20APACSyntacticElement]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACBWE2Decoder::Deserialize(
        APACBWE2Decoder *this,
        TBitstreamReader_u32 *bs,
        unsigned int n,
        APACSyntacticElement *element)
{
  __int64 v6; // x26
  __int64 v8; // x27
  unsigned int v10; // w22
  __int64 v11; // x24
  unsigned int v12; // w23
  unsigned __int64 v13; // x22
  __int64 v14; // x21
  unsigned int v15; // w0
  int v16; // w2
  unsigned int v17; // w24
  int v18; // w2
  unsigned int v19; // w0
  unsigned int v20; // w3
  unsigned int v21; // w24
  __int64 v22; // x1
  int v23; // w0
  unsigned int v24; // w1
  unsigned int v25; // w27
  unsigned int v26; // w1
  int v27; // w0
  unsigned int v28; // w2
  unsigned int v29; // w19
  unsigned int v30; // w27
  __int64 v31; // x0
  unsigned int v32; // w1
  int v33; // w0
  unsigned int v34; // w26
  unsigned int v35; // w0
  int v36; // w1
  unsigned int v37; // w0
  unsigned int cache; // w1
  unsigned int *p_cache; // t2
  __int64 v40; // x23
  int v41; // w0
  unsigned int v42; // w21
  __int64 v43; // x0
  __int64 v44; // x24
  __int64 v45; // x0
  __int64 (__fastcall *v47)(APACSyntacticElement *, _QWORD); // x2
  unsigned int v48; // w1
  __int64 v49; // x21
  int v50; // w0
  unsigned int v51; // w22
  __int64 v52; // x19
  unsigned int v53; // w1
  int v54; // w0
  unsigned int v55; // w22
  int v56; // w23
  unsigned int v57; // w21
  __int64 v58; // x0
  unsigned int v59; // w1
  int v60; // w0
  unsigned int v61; // w19
  unsigned int v62; // w0
  int v63; // w1
  unsigned int v64; // w0
  unsigned int v65; // w1
  __int64 v66; // x23
  int v67; // w0
  unsigned int v68; // w24
  __int64 v69; // x21
  unsigned int v70; // w1
  int v71; // w0
  unsigned int v72; // w23
  unsigned int v73; // w19
  __int64 v74; // x0
  unsigned int v75; // w1
  int v76; // w0
  unsigned int v77; // w21
  unsigned int v78; // w0
  int v79; // w1
  unsigned int v80; // w0
  unsigned int v81; // w0
  int v82; // w6
  const unsigned __int8 *cur; // x1
  const unsigned __int8 *v84; // x1
  unsigned int v85; // w0
  int v86; // w1
  unsigned int v87; // w0
  unsigned int v88; // w0
  int bitsLeft; // w1
  unsigned int v90; // w0
  __int64 v91; // x25
  _DWORD *v92; // x1
  __int64 v93; // x1
  __int64 v94; // x0
  unsigned int v95; // w0
  int v96; // w1
  unsigned int v97; // w0
  unsigned int v98; // w0
  int v99; // w1
  unsigned int v100; // w0
  unsigned int v101; // w0
  int v102; // w1
  unsigned int v103; // w0
  unsigned int v104; // w0
  int v105; // w1
  unsigned int v106; // w0
  unsigned int v107; // w0
  int v108; // w1
  unsigned int v109; // w0
  const unsigned __int8 *v110; // x1
  unsigned int v111; // w0
  int v112; // w1
  unsigned int v113; // w8
  unsigned int v114; // w0
  __int64 v115; // [xsp+60h] [xbp+60h]
  __int64 v116; // [xsp+68h] [xbp+68h]
  __int64 v117; // [xsp+70h] [xbp+70h]
  unsigned int v118; // [xsp+78h] [xbp+78h]
  unsigned int v119; // [xsp+7Ch] [xbp+7Ch]
  __int64 v120; // [xsp+90h] [xbp+90h]
  v6 = n >> 6;
  v8 = 1LL << n;
  v10 = *((_DWORD *)element + 2);
  v11 = ~(1LL << n);
  *(_QWORD *)(*((_QWORD *)this + 1) + 8 * v6) &= v11;
  if ( !v10 )
  {
    p_cache = &bs->cache;
    cache = bs->cache;
    v40 = *((_QWORD *)this + 1);
    v41 = p_cache[1] - 1;
    bs->bitsLeft = v41;
    v42 = cache >> 31;
    if ( v41 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v88 = bs->cache;
      bitsLeft = bs->bitsLeft;
      LOBYTE(v42) = v42 | (v88 >> (bitsLeft + 32));
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
    v43 = *(_QWORD *)(v40 + 8 * v6);
    v44 = v11 & v43;
    v45 = v8 | v43;
    if ( !(_BYTE)v42 )
      v45 = v44;
    *(_QWORD *)(v40 + 8 * v6) = v45;
    if ( (v8 & *(_QWORD *)(*((_QWORD *)this + 1) + 8 * v6)) != 0 )
    {
      v65 = bs->cache;
      v66 = *((_QWORD *)this + 9);
      v67 = bs->bitsLeft - 9;
      bs->bitsLeft = v67;
      v68 = v65 >> 23;
      v69 = v66 + 8LL * (n & 0x7FFFFFFF);
      if ( v67 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v95 = bs->cache;
        v96 = bs->bitsLeft;
        v68 |= v95 >> (v96 + 32);
        v97 = v95 << -(char)v96;
        if ( v96 == -32 )
          v97 = 0;
        bs->cur += 4;
        bs->cache = v97;
        bs->bitsLeft = v96 + 32;
      }
      else
      {
        bs->cache = v65 << 9;
      }
      *(_DWORD *)(v66 + 8LL * n) = v68;
      v70 = bs->cache;
      v71 = bs->bitsLeft - 9;
      bs->bitsLeft = v71;
      v72 = v70 >> 23;
      if ( v71 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v98 = bs->cache;
        v99 = bs->bitsLeft;
        v72 |= v98 >> (v99 + 32);
        v100 = v98 << -(char)v99;
        if ( v99 == -32 )
          v100 = 0;
        bs->cur += 4;
        bs->cache = v100;
        bs->bitsLeft = v99 + 32;
      }
      else
      {
        bs->cache = v70 << 9;
      }
      v73 = 8 * n;
      *(_DWORD *)(v69 + 4) = v72;
      while ( *(unsigned __int8 *)((*(__int64 (__fastcall **)(APACSyntacticElement *, __int64))(*(_QWORD *)element + 40LL))(
                                     element,
                                     1)
                                 + 12) > v10 )
      {
        v75 = bs->cache;
        v76 = bs->bitsLeft - 6;
        bs->bitsLeft = v76;
        v77 = v75 >> 26;
        if ( v76 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v78 = bs->cache;
          v79 = bs->bitsLeft;
          v77 |= v78 >> (v79 + 32);
          v80 = v78 << -(char)v79;
          if ( v79 == -32 )
            v80 = 0;
          bs->cur += 4;
          bs->cache = v80;
          bs->bitsLeft = v79 + 32;
        }
        else
        {
          bs->cache = v75 << 6;
        }
        v74 = v73 + v10++;
        *(_DWORD *)(*((_QWORD *)this + 7) + 4 * v74) = v77;
      }
    }
    return 0;
  }
  if ( v10 != 1 )
    return 0;
  v12 = n + 1;
  v13 = (unsigned __int64)(n + 1) >> 6;
  v14 = 1LL << ((unsigned __int8)n + 1);
  *(_QWORD *)(*((_QWORD *)this + 1) + 8 * v6) &= v11;
  *(_QWORD *)(*((_QWORD *)this + 1) + 8 * v13) &= ~v14;
  v15 = bs->cache;
  v16 = bs->bitsLeft;
  bs->bitsLeft = v16 - 1;
  v17 = v15 >> 31;
  if ( v16 - 1 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v81 = bs->cache;
    v82 = bs->bitsLeft;
    cur = bs->cur;
    bs->bitsLeft = v82 + 32;
    v18 = v82 + 31;
    v84 = cur + 4;
    bs->cur = v84;
    v20 = (unsigned __int8)((v81 >> (v82 + 32)) | v17);
    if ( v82 == -32 )
    {
      LOBYTE(v21) = 0;
      bs->cache = 0;
      bs->bitsLeft = v18;
LABEL_56:
      LODWORD(v115) = v20;
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v84);
      v85 = bs->cache;
      v86 = bs->bitsLeft;
      v20 = v115;
      LOBYTE(v21) = v21 | (v85 >> (v86 + 32));
      v87 = v85 << -(char)v86;
      if ( v86 == -32 )
        v87 = 0;
      bs->cur += 4;
      bs->cache = v87;
      bs->bitsLeft = v86 + 32;
      goto LABEL_7;
    }
    v19 = v81 << -(char)v82;
    bs->cache = v19;
  }
  else
  {
    v18 = v16 - 2;
    __SET_PAIR__(v20, v19, 2LL * v15);
    bs->cache = v19;
  }
  bs->bitsLeft = v18;
  v21 = v19 >> 31;
  if ( v18 < 0 )
  {
    v84 = bs->cur;
    goto LABEL_56;
  }
  bs->cache = 2 * v19;
LABEL_7:
  v21 = (unsigned __int8)v21;
  if ( v20
    && *(_BYTE *)((*(__int64 (__fastcall **)(APACSyntacticElement *, __int64))(*(_QWORD *)element + 40LL))(element, 1)
                + 11) )
  {
    v22 = *((_QWORD *)this + 1);
    v116 = 2 * n;
    v118 = 2 * n;
    *(_QWORD *)(v22 + 8 * v6) |= v8;
    v117 = *((_QWORD *)this + 9);
    v24 = bs->cache;
    v23 = bs->bitsLeft;
    v115 = v117 + 8LL * n;
    v23 -= 9;
    bs->bitsLeft = v23;
    v25 = v24 >> 23;
    if ( v23 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v101 = bs->cache;
      v102 = bs->bitsLeft;
      v25 |= v101 >> (v102 + 32);
      v103 = v101 << -(char)v102;
      if ( v102 == -32 )
        v103 = 0;
      bs->cur += 4;
      bs->cache = v103;
      bs->bitsLeft = v102 + 32;
    }
    else
    {
      bs->cache = v24 << 9;
    }
    *(_DWORD *)(v117 + 4 * v116) = v25;
    v26 = bs->cache;
    v27 = bs->bitsLeft - 9;
    bs->bitsLeft = v27;
    v28 = v26 >> 23;
    if ( v27 < 0 )
    {
      v110 = bs->cur;
      v119 = v28;
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v110);
      v111 = bs->cache;
      v112 = bs->bitsLeft;
      v113 = v111 >> (v112 + 32);
      v114 = v111 << -(char)v112;
      v28 = v119 | v113;
      if ( v112 == -32 )
        v114 = 0;
      bs->cur += 4;
      bs->cache = v114;
      bs->bitsLeft = v112 + 32;
    }
    else
    {
      bs->cache = v26 << 9;
    }
    v29 = 8 * n;
    v30 = 0;
    *(_DWORD *)(v115 + 4) = v28;
    while ( *(unsigned __int8 *)((*(__int64 (__fastcall **)(APACSyntacticElement *, __int64))(*(_QWORD *)element + 40LL))(
                                   element,
                                   1)
                               + 12) > v30 )
    {
      v32 = bs->cache;
      v33 = bs->bitsLeft - 6;
      bs->bitsLeft = v33;
      v34 = v32 >> 26;
      if ( v33 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v35 = bs->cache;
        v36 = bs->bitsLeft;
        v34 |= v35 >> (v36 + 32);
        v37 = v35 << -(char)v36;
        if ( v36 == -32 )
          v37 = 0;
        bs->cur += 4;
        bs->cache = v37;
        bs->bitsLeft = v36 + 32;
      }
      else
      {
        bs->cache = v32 << 6;
      }
      v31 = v29 + v30++;
      *(_DWORD *)(*((_QWORD *)this + 7) + 4 * v31) = v34;
    }
    v47 = *(__int64 (__fastcall **)(APACSyntacticElement *, _QWORD))(*(_QWORD *)element + 40LL);
    if ( !(_BYTE)v21 )
    {
      if ( *(_BYTE *)(v47(element, 0) + 11) )
      {
        v91 = v118 + 2;
        *(_QWORD *)(*((_QWORD *)this + 1) + 8 * v13) |= v14;
        v92 = (_DWORD *)(*((_QWORD *)this + 9) + 4 * v91);
        *v92 = *(_DWORD *)(v117 + 4 * v116);
        v92[1] = *(_DWORD *)(v115 + 4);
        while ( *(unsigned __int8 *)((*(__int64 (__fastcall **)(APACSyntacticElement *, __int64))(*(_QWORD *)element
                                                                                                + 40LL))(
                                       element,
                                       1)
                                   + 12) > v21 )
        {
          v93 = v29 + v21;
          v94 = v29 + 8 + v21++;
          *(_DWORD *)(*((_QWORD *)this + 7) + 4 * v94) = *(_DWORD *)(*((_QWORD *)this + 7) + 4 * v93);
        }
      }
      return 0;
    }
  }
  else
  {
    if ( !(_BYTE)v21 )
      return 0;
    v47 = *(__int64 (__fastcall **)(APACSyntacticElement *, _QWORD))(*(_QWORD *)element + 40LL);
  }
  if ( *(_BYTE *)(v47(element, 0) + 11) )
  {
    *(_QWORD *)(*((_QWORD *)this + 1) + 8 * v13) |= v14;
    v48 = bs->cache;
    v49 = *((_QWORD *)this + 9);
    v50 = bs->bitsLeft - 9;
    bs->bitsLeft = v50;
    v51 = v48 >> 23;
    v52 = v49 + 8LL * (v12 & 0x7FFFFFFF);
    if ( v50 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v107 = bs->cache;
      v108 = bs->bitsLeft;
      v51 |= v107 >> (v108 + 32);
      v109 = v107 << -(char)v108;
      if ( v108 == -32 )
        v109 = 0;
      bs->cur += 4;
      bs->cache = v109;
      bs->bitsLeft = v108 + 32;
    }
    else
    {
      bs->cache = v48 << 9;
    }
    *(_DWORD *)(v49 + 8LL * v12) = v51;
    v53 = bs->cache;
    v54 = bs->bitsLeft - 9;
    bs->bitsLeft = v54;
    v55 = v53 >> 23;
    if ( v54 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v104 = bs->cache;
      v105 = bs->bitsLeft;
      v55 |= v104 >> (v105 + 32);
      v106 = v104 << -(char)v105;
      if ( v105 == -32 )
        v106 = 0;
      bs->cur += 4;
      bs->cache = v106;
      bs->bitsLeft = v105 + 32;
    }
    else
    {
      bs->cache = v53 << 9;
    }
    v56 = 8 * v12;
    v57 = 0;
    *(_DWORD *)(v52 + 4) = v55;
    while ( *(unsigned __int8 *)((*(__int64 (__fastcall **)(APACSyntacticElement *, __int64))(*(_QWORD *)element + 40LL))(
                                   element,
                                   1)
                               + 12) > v57 )
    {
      v59 = bs->cache;
      v60 = bs->bitsLeft - 6;
      bs->bitsLeft = v60;
      v61 = v59 >> 26;
      if ( v60 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v62 = bs->cache;
        v63 = bs->bitsLeft;
        v61 |= v62 >> (v63 + 32);
        v64 = v62 << -(char)v63;
        if ( v63 == -32 )
          v64 = 0;
        bs->cur += 4;
        bs->cache = v64;
        bs->bitsLeft = v63 + 32;
      }
      else
      {
        bs->cache = v59 << 6;
      }
      v58 = v56 + v57++;
      *(_DWORD *)(*((_QWORD *)this + 7) + 4 * v58) = v61;
    }
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000791d0  size 1180 bytes  293 insns
 *   APACBWE2Decoder::InitInterFrameMemory(unsigned int)
 *   [_ZN15APACBWE2Decoder20InitInterFrameMemoryEj]
 *   data refs: 0x79810, 0x79940, 0x79d40, 0x42a828, 0x42a848, 0x42b5d8, 0x42b700, 0x42b750, 0x42b798, 0x42cd38, 0x42ce00, 0x42d070, 0x42d2a8, 0x42d4b0, 0x42d7d8, 0x42d920, 0x42dbd8, 0x42dbe8, 0x42ddc0, 0x43e520
 */
__int64 __fastcall APACBWE2Decoder::InitInterFrameMemory(APACBWE2Decoder *this, unsigned int n)
{
  __int64 v2; // x27
  __int64 v3; // x28
  __int64 v6; // x21
  __int64 v7; // x19
  bool v8; // zf
  _DWORD *v9; // x0
  _DWORD *i; // x2
  __int64 v11; // x21
  __int64 v12; // x19
  _DWORD *v13; // x0
  _DWORD *j; // x2
  __int64 v15; // x21
  _QWORD *v16; // x0
  _QWORD *v17; // x1
  _QWORD *v18; // x3
  _QWORD *v19; // x19
  void (__fastcall *v20)(BWELPC *__hidden); // x1
  apac__Autocorr **v21; // x20
  apac__Autocorr **v22; // x23
  void (__fastcall **v23)(apac::Autocorr *__hidden); // x0
  void (__fastcall ***v24)(apac::Autocorr *__hidden); // x24
  void (__fastcall *v25)(apac__Autocorr *); // x2
  apac__Autocorr *v26; // x0
  apac__LPMDCTFilter **v27; // x24
  apac__LPMDCTFilter **v28; // x28
  apac__LPMDCTFilter *v29; // x25
  void (__fastcall *v30)(apac__LPMDCTFilter *); // x1
  apac__LPMDCTFilter *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  void *v35; // x0
  void *v36; // x0
  _QWORD **v37; // x23
  _QWORD **v38; // x20
  _QWORD *v39; // x25
  void *v40; // x0
  void *v41; // x0
  _QWORD *v42; // x20
  void (__fastcall *v43)(apac__Autocorr *); // x1
  void *v44; // x0
  void *v45; // x0
  void *v46; // x0
  __int64 v47; // x0
  __int64 result; // x0
  __int64 v49; // [xsp+50h] [xbp+50h]
  __int64 v50; // [xsp+58h] [xbp+58h]
  void (__fastcall **v51)(apac::Autocorr *__hidden); // [xsp+60h] [xbp+60h]
  __int64 v52; // [xsp+68h] [xbp+68h]
  void *v53; // [xsp+70h] [xbp+70h]
  __int64 v54; // [xsp+78h] [xbp+78h]
  __int64 v55; // [xsp+80h] [xbp+80h]
  v6 = 2 * n;
  v7 = 2LL * (n & 0x7FFFFFFF);
  v8 = v6 == *((_QWORD *)this + 8);
  v9 = *((_DWORD **)this + 9);
  if ( !v8 )
  {
    operator delete(v9);
    *((_QWORD *)this + 8) = v6;
    v9 = (_DWORD *)operator new(8LL * (n & 0x7FFFFFFF));
    *((_QWORD *)this + 9) = v9;
  }
  for ( i = &v9[v7]; i != v9; ++v9 )
    *v9 = 0;
  v11 = 8 * n;
  v12 = 8LL * (n & 0x1FFFFFFF);
  v13 = *((_DWORD **)this + 7);
  if ( v11 != *((_QWORD *)this + 6) )
  {
    operator delete(v13);
    *((_QWORD *)this + 6) = v11;
    v13 = (_DWORD *)operator new(32LL * (n & 0x1FFFFFFF));
    *((_QWORD *)this + 7) = v13;
  }
  for ( j = &v13[v12]; j != v13; ++v13 )
    *v13 = 0;
  if ( n )
  {
    v50 = v3;
    v15 = 0;
    v52 = 8 * (n - 1 + 1LL);
    v51 = off_42A858;
    while ( 1 )
    {
      v16 = (_QWORD *)operator new(0x120u);
      *v16 = off_42B5E8;
      v16[1] = 0x100400010LL;
      *((_WORD *)v16 + 8) = 0;
      v16[3] = 0;
      v16[4] = 0;
      v16[5] = 0;
      v16[6] = 0;
      v16[7] = 0;
      v16[8] = 0;
      v16[9] = 0;
      v16[10] = 0;
      v16[11] = 0;
      v16[12] = 0;
      *((_DWORD *)v16 + 26) = 16;
      *((_WORD *)v16 + 54) = 0;
      v16[14] = 0x4248000046BB8000LL;
      v16[15] = 0;
      v16[16] = 0;
      v16[17] = 0;
      v16[18] = 0;
      v16[19] = 0;
      v16[20] = 0;
      v16[21] = 0;
      v16[22] = 0;
      v16[23] = 0;
      v16[24] = 0;
      v16[25] = 0;
      v16[26] = 0;
      v16[27] = 0;
      v16[28] = 0;
      v16[29] = 0;
      v16[30] = 0;
      v16[31] = 0;
      v17 = *((_QWORD **)this + 12);
      v18 = *((_QWORD **)this + 13);
      v53 = v16;
      v16[32] = 0;
      v16[33] = 0;
      v16[34] = 0;
      v16[35] = 0;
      if ( v17 == v18 )
      {
        std::vector<std::unique_ptr<BWELPC>>::_M_realloc_insert<std::unique_ptr<BWELPC>>((char *)this + 88);
      }
      else
      {
        v53 = nullptr;
        *v17 = v16;
        *((_QWORD *)this + 12) += 8LL;
      }
      v19 = v53;
      if ( !v53 )
        goto LABEL_68;
      v20 = *(void (__fastcall **)(BWELPC *__hidden))(*(_QWORD *)v53 + 8LL);
      if ( v20 != BWELPC::~BWELPC )
      {
        v20((BWELPC *)v53);
        goto LABEL_68;
      }
      v21 = *((apac__Autocorr ***)v53 + 33);
      v22 = *((apac__Autocorr ***)v53 + 34);
      *(_QWORD *)v53 = off_42B5E8;
      if ( v21 == v22 )
        goto LABEL_25;
      do
      {
        while ( 1 )
        {
          v24 = (void (__fastcall ***)(apac::Autocorr *__hidden))*v21;
          if ( *v21 )
            break;
LABEL_20:
          if ( v22 == ++v21 )
            goto LABEL_24;
        }
        v25 = (*v24)[1];
        if ( v25 == apac::Autocorr::~Autocorr )
        {
          v23 = v24[2];
          *v24 = v51;
          if ( v23 )
            operator delete(v23);
          operator delete(v24);
          goto LABEL_20;
        }
        v26 = *v21++;
        v25(v26);
      }
      while ( v22 != v21 );
LABEL_24:
      v22 = (apac__Autocorr **)v19[33];
LABEL_25:
      if ( v22 )
        operator delete(v22);
      v28 = (apac__LPMDCTFilter **)v19[30];
      v27 = (apac__LPMDCTFilter **)v19[31];
      if ( v28 != v27 )
      {
        while ( 1 )
        {
LABEL_31:
          v29 = *v28;
          if ( !*v28 )
            goto LABEL_30;
          v30 = *(void (__fastcall **)(apac__LPMDCTFilter *))(*(_QWORD *)v29 + 8LL);
          if ( v30 == apac::LPMDCTFilter::~LPMDCTFilter )
            break;
          v31 = *v28++;
          v30(v31);
          if ( v27 == v28 )
          {
LABEL_34:
            v27 = (apac__LPMDCTFilter **)v19[30];
            goto LABEL_35;
          }
        }
        *(_QWORD *)v29 = off_42B710;
        *((_QWORD *)v29 + 2) = off_42A838;
        DFTHandle<false>::~DFTHandle((char *)v29 + 16);
        operator delete(v29);
LABEL_30:
        if ( v27 == ++v28 )
          goto LABEL_34;
        goto LABEL_31;
      }
LABEL_35:
      if ( v27 )
        operator delete(v27);
      v32 = (void *)v19[27];
      *v19 = off_42B7A8;
      if ( v32 )
        operator delete(v32);
      v33 = (void *)v19[24];
      if ( v33 )
        operator delete(v33);
      v34 = (void *)v19[21];
      if ( v34 )
        operator delete(v34);
      v35 = (void *)v19[18];
      if ( v35 )
        operator delete(v35);
      v36 = (void *)v19[15];
      if ( v36 )
        operator delete(v36);
      *v19 = &unk_42B760;
      v37 = (_QWORD **)v19[11];
      v38 = (_QWORD **)v19[10];
      if ( v38 != v37 )
      {
        do
        {
          v39 = *v38;
          if ( *v38 )
          {
            v40 = (void *)v39[6];
            if ( v40 )
              operator delete(v40);
            v41 = (void *)v39[2];
            if ( v41 )
              operator delete(v41);
            operator delete(v39);
          }
          ++v38;
        }
        while ( v37 != v38 );
        v37 = (_QWORD **)v19[10];
      }
      if ( v37 )
        operator delete(v37);
      v42 = (_QWORD *)v19[9];
      if ( v42 )
      {
        v43 = *(void (__fastcall **)(apac__Autocorr *))(*v42 + 8LL);
        if ( v43 == apac::Autocorr::~Autocorr )
        {
          v44 = (void *)v42[2];
          *v42 = off_42A858;
          if ( v44 )
            operator delete(v44);
          operator delete(v42);
        }
        else
        {
          v43((apac__Autocorr *)v19[9]);
        }
      }
      v45 = (void *)v19[6];
      if ( v45 )
        operator delete(v45);
      v46 = (void *)v19[3];
      if ( v46 )
        operator delete(v46);
      operator delete(v19);
LABEL_68:
      v47 = *(_QWORD *)(*((_QWORD *)this + 11) + v15);
      v15 += 8;
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v47 + 16LL))(v47);
      if ( v52 == v15 )
      {
        v3 = v50;
        break;
      }
    }
  }
  result = v54 ^ _stack_chk_guard;
  if ( v54 != _stack_chk_guard )
  {
    v49 = v2;
    v50 = v3;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00079670  size 192 bytes  48 insns
 *   APACBWE2Decoder::Initialize(unsigned int)
 *   [_ZN15APACBWE2Decoder10InitializeEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACBWE2Decoder::Initialize(APACBWE2Decoder *this, unsigned int n)
{
  __int64 v4; // x2
  void *v5; // x1
  __int64 v7; // [xsp+40h] [xbp+40h]
  __int64 v8; // [xsp+50h] [xbp+50h]
  v4 = *((_QWORD *)this + 1);
  v5 = *((void **)this + 3);
  if ( (unsigned __int64)*((unsigned int *)this + 8) + 8 * ((__int64)v5 - v4) <= n )
  {
    LODWORD(v7) = *((_DWORD *)this + 8);
    std::vector<bool>::_M_fill_insert((_DWORD)this + 8, v5);
  }
  else
  {
    *((_DWORD *)this + 8) = n & 0x3F;
    *((_QWORD *)this + 3) = v4 + 8 * ((__int64)n >> 6);
  }
  APACBWE2Decoder::InitInterFrameMemory(this, n);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x00079730  size 92 bytes  23 insns
 *   APACBWE2Decoder::GetActiveFlag(unsigned int) const
 *   [_ZNK15APACBWE2Decoder13GetActiveFlagEj]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall APACBWE2Decoder::GetActiveFlag(APACBWE2Decoder *this, unsigned int n)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return ((1LL << n) & *(_QWORD *)(*((_QWORD *)this + 1) + 8LL * (n >> 6))) != 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000799c0  size 888 bytes  222 insns
 *   APACBWE2Decoder::~APACBWE2Decoder()
 *   APACBWE2Decoder::~APACBWE2Decoder()
 *   [_ZN15APACBWE2DecoderD1Ev]
 *   [_ZN15APACBWE2DecoderD2Ev]
 *   data refs: 0x79810, 0x79940, 0x79d40, 0x42a828, 0x42a848, 0x42a868, 0x42b5d8, 0x42b700, 0x42b750, 0x42b798, 0x42cd38, 0x42ce00, 0x42d070, 0x42d168, 0x42d2a8, 0x42d4b0, 0x42d7d8, 0x42d920, 0x42dbd8, 0x42dbe8, 0x42ddc0, 0x43e520
 */
// Alternative name is '_ZN15APACBWE2DecoderD2Ev'
void __fastcall APACBWE2Decoder::~APACBWE2Decoder(APACBWE2Decoder *this)
{
  __int64 v1; // x25
  __int64 v2; // x26
  __int64 v3; // x27
  __int64 v4; // x28
  void *v6; // x2
  __int64 v7; // x3
  _QWORD *v8; // x23
  _QWORD *v9; // x21
  _QWORD *v10; // x19
  void (__fastcall *v11)(BWELPC *__hidden); // x1
  apac__Autocorr **v12; // x26
  apac__Autocorr **v13; // x28
  void (__fastcall **v14)(apac::Autocorr *__hidden); // x0
  void (__fastcall ***v15)(apac::Autocorr *__hidden); // x22
  void (__fastcall *v16)(apac__Autocorr *); // x3
  apac__Autocorr *v17; // x0
  void **v18; // x26
  void **v19; // x28
  _QWORD *v20; // x1
  void (__fastcall *v21)(apac__LPMDCTFilter *); // x4
  apac__LPMDCTFilter *v22; // x0
  void *v23; // x0
  void *v24; // x0
  void *v25; // x0
  void *v26; // x0
  void *v27; // x0
  _QWORD **v28; // x22
  _QWORD **v29; // x25
  _QWORD *v30; // x26
  void *v31; // x0
  void *v32; // x0
  _QWORD *v33; // x22
  void (__fastcall *v34)(apac__Autocorr *); // x1
  void *v35; // x0
  void *v36; // x0
  void *v37; // x0
  void *v38; // x0
  __int64 v39; // [xsp+40h] [xbp+40h]
  __int64 v40; // [xsp+48h] [xbp+48h]
  __int64 v41; // [xsp+50h] [xbp+50h]
  __int64 v42; // [xsp+58h] [xbp+58h]
  void *v43; // [xsp+60h] [xbp+60h]
  void (__fastcall **v44)(apac::Autocorr *__hidden); // [xsp+68h] [xbp+68h]
  __int64 v45; // [xsp+78h] [xbp+78h]
  __int64 v46; // [xsp+80h] [xbp+80h]
  v6 = &_stack_chk_guard;
  v7 = 0;
  *(_QWORD *)this = off_42A878;
  v8 = *((_QWORD **)this + 12);
  v9 = *((_QWORD **)this + 11);
  if ( v9 == v8 )
    goto LABEL_60;
  v42 = v4;
  v39 = v1;
  v40 = v2;
  v44 = off_42A858;
  do
  {
    v10 = (_QWORD *)*v9;
    if ( !*v9 )
      goto LABEL_58;
    v11 = *(void (__fastcall **)(BWELPC *__hidden))(*v10 + 8LL);
    if ( v11 != BWELPC::~BWELPC )
    {
      ((void (__fastcall *)(_QWORD, void (__fastcall *)(BWELPC *__hidden), void *, __int64))v11)(*v9, v11, v6, v7);
      goto LABEL_58;
    }
    v13 = (apac__Autocorr **)v10[33];
    v12 = (apac__Autocorr **)v10[34];
    *v10 = off_42B5E8;
    if ( v13 == v12 )
      goto LABEL_15;
    do
    {
      while ( 1 )
      {
        v15 = (void (__fastcall ***)(apac::Autocorr *__hidden))*v13;
        if ( *v13 )
          break;
LABEL_10:
        if ( v12 == ++v13 )
          goto LABEL_14;
      }
      v16 = (*v15)[1];
      if ( v16 == apac::Autocorr::~Autocorr )
      {
        v14 = v15[2];
        *v15 = v44;
        if ( v14 )
          operator delete(v14);
        operator delete(v15);
        goto LABEL_10;
      }
      v17 = *v13++;
      v16(v17);
    }
    while ( v12 != v13 );
LABEL_14:
    v12 = (apac__Autocorr **)v10[33];
LABEL_15:
    if ( v12 )
      operator delete(v12);
    v18 = (void **)v10[30];
    v19 = (void **)v10[31];
    if ( v18 != v19 )
    {
      while ( 1 )
      {
LABEL_21:
        v20 = *v18;
        if ( !*v18 )
          goto LABEL_20;
        v21 = *(void (__fastcall **)(apac__LPMDCTFilter *))(*v20 + 8LL);
        if ( v21 == apac::LPMDCTFilter::~LPMDCTFilter )
          break;
        v22 = (apac__LPMDCTFilter *)*v18++;
        v21(v22);
        if ( v19 == v18 )
        {
LABEL_24:
          v19 = (void **)v10[30];
          goto LABEL_25;
        }
      }
      v43 = *v18;
      *v20 = off_42B710;
      v20[2] = off_42A838;
      DFTHandle<false>::~DFTHandle(v20 + 2);
      operator delete(v43);
LABEL_20:
      if ( v19 == ++v18 )
        goto LABEL_24;
      goto LABEL_21;
    }
LABEL_25:
    if ( v19 )
      operator delete(v19);
    v23 = (void *)v10[27];
    *v10 = off_42B7A8;
    if ( v23 )
      operator delete(v23);
    v24 = (void *)v10[24];
    if ( v24 )
      operator delete(v24);
    v25 = (void *)v10[21];
    if ( v25 )
      operator delete(v25);
    v26 = (void *)v10[18];
    if ( v26 )
      operator delete(v26);
    v27 = (void *)v10[15];
    if ( v27 )
      operator delete(v27);
    *v10 = &unk_42B760;
    v28 = (_QWORD **)v10[11];
    v29 = (_QWORD **)v10[10];
    if ( v29 != v28 )
    {
      do
      {
        v30 = *v29;
        if ( *v29 )
        {
          v31 = (void *)v30[6];
          if ( v31 )
            operator delete(v31);
          v32 = (void *)v30[2];
          if ( v32 )
            operator delete(v32);
          operator delete(v30);
        }
        ++v29;
      }
      while ( v28 != v29 );
      v28 = (_QWORD **)v10[10];
    }
    if ( v28 )
      operator delete(v28);
    v33 = (_QWORD *)v10[9];
    if ( v33 )
    {
      v34 = *(void (__fastcall **)(apac__Autocorr *))(*v33 + 8LL);
      if ( v34 == apac::Autocorr::~Autocorr )
      {
        v35 = (void *)v33[2];
        *v33 = off_42A858;
        if ( v35 )
          operator delete(v35);
        operator delete(v33);
      }
      else
      {
        v34((apac__Autocorr *)v10[9]);
      }
    }
    v36 = (void *)v10[6];
    if ( v36 )
      operator delete(v36);
    v37 = (void *)v10[3];
    if ( v37 )
      operator delete(v37);
    operator delete(v10);
LABEL_58:
    ++v9;
  }
  while ( v8 != v9 );
  v1 = v39;
  v2 = v40;
  v4 = v42;
  v8 = *((_QWORD **)this + 11);
LABEL_60:
  if ( v8 )
    operator delete(v8);
  operator delete(*((void **)this + 9));
  operator delete(*((void **)this + 7));
  v38 = *((void **)this + 1);
  if ( v38 )
  {
    if ( v45 == _stack_chk_guard )
    {
      operator delete(v38);
      return;
    }
LABEL_68:
    v39 = v1;
    v40 = v2;
    v41 = v3;
    v42 = v4;
  }
  if ( v45 != _stack_chk_guard )
    goto LABEL_68;
}


/* ------------------------------------------------------------------------ */
/* 0x00079ff0  size 860 bytes  215 insns
 *   APACBWE2Decoder::~APACBWE2Decoder()
 *   [_ZN15APACBWE2DecoderD0Ev]
 *   data refs: 0x79810, 0x79940, 0x79d40, 0x42a828, 0x42a848, 0x42a868, 0x42b5d8, 0x42b700, 0x42b750, 0x42b798, 0x42cd38, 0x42ce00, 0x42d070, 0x42d168, 0x42d2a8, 0x42d4b0, 0x42d7d8, 0x42d920, 0x42dbd8, 0x42dbe8, 0x42ddc0, 0x43e520
 */
void __fastcall APACBWE2Decoder::~APACBWE2Decoder(APACBWE2Decoder *this)
{
  __int64 v1; // x25
  __int64 v2; // x26
  __int64 v3; // x27
  __int64 v4; // x28
  void *v6; // x2
  __int64 v7; // x3
  _QWORD *v8; // x23
  _QWORD *v9; // x21
  _QWORD *v10; // x19
  void (__fastcall *v11)(BWELPC *__hidden); // x1
  apac__Autocorr **v12; // x26
  apac__Autocorr **v13; // x28
  void (__fastcall **v14)(apac::Autocorr *__hidden); // x0
  void (__fastcall ***v15)(apac::Autocorr *__hidden); // x22
  void (__fastcall *v16)(apac__Autocorr *); // x3
  apac__Autocorr *v17; // x0
  void **v18; // x26
  void **v19; // x28
  _QWORD *v20; // x1
  void (__fastcall *v21)(apac__LPMDCTFilter *); // x4
  apac__LPMDCTFilter *v22; // x0
  void *v23; // x0
  void *v24; // x0
  void *v25; // x0
  void *v26; // x0
  void *v27; // x0
  _QWORD **v28; // x22
  _QWORD **v29; // x25
  _QWORD *v30; // x26
  void *v31; // x0
  void *v32; // x0
  _QWORD *v33; // x22
  void (__fastcall *v34)(apac__Autocorr *); // x1
  void *v35; // x0
  void *v36; // x0
  void *v37; // x0
  void *v38; // x0
  __int64 v39; // [xsp+40h] [xbp+40h]
  __int64 v40; // [xsp+48h] [xbp+48h]
  __int64 v41; // [xsp+50h] [xbp+50h]
  __int64 v42; // [xsp+58h] [xbp+58h]
  void *v43; // [xsp+60h] [xbp+60h]
  void (__fastcall **v44)(apac::Autocorr *__hidden); // [xsp+68h] [xbp+68h]
  __int64 v45; // [xsp+78h] [xbp+78h]
  __int64 v46; // [xsp+80h] [xbp+80h]
  v6 = &_stack_chk_guard;
  v7 = 0;
  *(_QWORD *)this = off_42A878;
  v8 = *((_QWORD **)this + 12);
  v9 = *((_QWORD **)this + 11);
  if ( v9 != v8 )
  {
    v42 = v4;
    v39 = v1;
    v40 = v2;
    v44 = off_42A858;
    while ( 1 )
    {
      v10 = (_QWORD *)*v9;
      if ( !*v9 )
        goto LABEL_58;
      v11 = *(void (__fastcall **)(BWELPC *__hidden))(*v10 + 8LL);
      if ( v11 != BWELPC::~BWELPC )
      {
        ((void (__fastcall *)(_QWORD, void (__fastcall *)(BWELPC *__hidden), void *, __int64))v11)(*v9, v11, v6, v7);
        goto LABEL_58;
      }
      v13 = (apac__Autocorr **)v10[33];
      v12 = (apac__Autocorr **)v10[34];
      *v10 = off_42B5E8;
      if ( v13 == v12 )
        goto LABEL_15;
      do
      {
        while ( 1 )
        {
          v15 = (void (__fastcall ***)(apac::Autocorr *__hidden))*v13;
          if ( *v13 )
            break;
LABEL_10:
          if ( v12 == ++v13 )
            goto LABEL_14;
        }
        v16 = (*v15)[1];
        if ( v16 == apac::Autocorr::~Autocorr )
        {
          v14 = v15[2];
          *v15 = v44;
          if ( v14 )
            operator delete(v14);
          operator delete(v15);
          goto LABEL_10;
        }
        v17 = *v13++;
        v16(v17);
      }
      while ( v12 != v13 );
LABEL_14:
      v12 = (apac__Autocorr **)v10[33];
LABEL_15:
      if ( v12 )
        operator delete(v12);
      v18 = (void **)v10[30];
      v19 = (void **)v10[31];
      if ( v18 != v19 )
      {
        while ( 1 )
        {
LABEL_21:
          v20 = *v18;
          if ( !*v18 )
            goto LABEL_20;
          v21 = *(void (__fastcall **)(apac__LPMDCTFilter *))(*v20 + 8LL);
          if ( v21 == apac::LPMDCTFilter::~LPMDCTFilter )
            break;
          v22 = (apac__LPMDCTFilter *)*v18++;
          v21(v22);
          if ( v19 == v18 )
          {
LABEL_24:
            v19 = (void **)v10[30];
            goto LABEL_25;
          }
        }
        v43 = *v18;
        *v20 = off_42B710;
        v20[2] = off_42A838;
        DFTHandle<false>::~DFTHandle(v20 + 2);
        operator delete(v43);
LABEL_20:
        if ( v19 == ++v18 )
          goto LABEL_24;
        goto LABEL_21;
      }
LABEL_25:
      if ( v19 )
        operator delete(v19);
      v23 = (void *)v10[27];
      *v10 = off_42B7A8;
      if ( v23 )
        operator delete(v23);
      v24 = (void *)v10[24];
      if ( v24 )
        operator delete(v24);
      v25 = (void *)v10[21];
      if ( v25 )
        operator delete(v25);
      v26 = (void *)v10[18];
      if ( v26 )
        operator delete(v26);
      v27 = (void *)v10[15];
      if ( v27 )
        operator delete(v27);
      *v10 = &unk_42B760;
      v28 = (_QWORD **)v10[11];
      v29 = (_QWORD **)v10[10];
      if ( v29 != v28 )
      {
        do
        {
          v30 = *v29;
          if ( *v29 )
          {
            v31 = (void *)v30[6];
            if ( v31 )
              operator delete(v31);
            v32 = (void *)v30[2];
            if ( v32 )
              operator delete(v32);
            operator delete(v30);
          }
          ++v29;
        }
        while ( v28 != v29 );
        v28 = (_QWORD **)v10[10];
      }
      if ( v28 )
        operator delete(v28);
      v33 = (_QWORD *)v10[9];
      if ( v33 )
      {
        v34 = *(void (__fastcall **)(apac__Autocorr *))(*v33 + 8LL);
        if ( v34 == apac::Autocorr::~Autocorr )
        {
          v35 = (void *)v33[2];
          *v33 = off_42A858;
          if ( v35 )
            operator delete(v35);
          operator delete(v33);
        }
        else
        {
          v34((apac__Autocorr *)v10[9]);
        }
      }
      v36 = (void *)v10[6];
      if ( v36 )
        operator delete(v36);
      v37 = (void *)v10[3];
      if ( v37 )
        operator delete(v37);
      operator delete(v10);
LABEL_58:
      if ( v8 == ++v9 )
      {
        v1 = v39;
        v2 = v40;
        v4 = v42;
        v8 = *((_QWORD **)this + 11);
        break;
      }
    }
  }
  if ( v8 )
    operator delete(v8);
  operator delete(*((void **)this + 9));
  operator delete(*((void **)this + 7));
  v38 = *((void **)this + 1);
  if ( v38 )
    operator delete(v38);
  if ( v45 != _stack_chk_guard )
  {
    v39 = v1;
    v40 = v2;
    v41 = v3;
    v42 = v4;
  }
  operator delete(this);
}


