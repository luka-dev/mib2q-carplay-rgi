/* ============================================================================
 * APACTNSData
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 2188 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000aed10    168  APACTNSData::APACTNSData(APACInstanceConfig&) */
/* 0x000aedc0   1140  APACTNSData::ParseTNSData(TBitstreamReader<unsigned int>&, APACICSInfo const&) */
/* 0x000af240    880  APACTNSData::Apply(APACICSInfo const&, apac::SFBOffsets const&, APACSpectralData&) const */

/* ------------------------------------------------------------------------ */
/* 0x000aed10  size 168 bytes  41 insns
 *   APACTNSData::APACTNSData(APACInstanceConfig&)
 *   APACTNSData::APACTNSData(APACInstanceConfig&)
 *   [_ZN11APACTNSDataC1ER18APACInstanceConfig]
 *   [_ZN11APACTNSDataC2ER18APACInstanceConfig]
 *   data refs: 0x331970, 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN11APACTNSDataC2ER18APACInstanceConfig'
__int64 __fastcall APACTNSData::APACTNSData(APACTNSData *this, APACInstanceConfig *instCfg)
{
  int *v2; // x2
  int v3; // w4
  int v4; // w3
  unsigned int v5; // w1
  char v6; // w1
  char v7; // w2
  char *v9; // x1
  __int64 v10; // [xsp+18h] [xbp+18h]
  __int64 v11; // [xsp+20h] [xbp+20h]
  v2 = (int *)&unk_331970;
  v3 = *((_DWORD *)instCfg + 1);
  v4 = 96000;
  v5 = 0;
  *(_WORD *)this = 0;
  *((_WORD *)this + 93) = 1804;
  while ( v3 != v4 )
  {
    ++v5;
    v2 += 2;
    if ( v5 == 12 )
    {
      v6 = 0;
      v7 = 0;
      goto LABEL_6;
    }
    v4 = *v2;
  }
  v9 = (char *)&unk_331970 + 8 * v5;
  v7 = v9[4];
  v6 = v9[5];
LABEL_6:
  *((_BYTE *)this + 188) = v7;
  *((_BYTE *)this + 189) = v6;
  return v10 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000aedc0  size 1140 bytes  285 insns
 *   APACTNSData::ParseTNSData(TBitstreamReader<unsigned int>&, APACICSInfo const&)
 *   [_ZN11APACTNSData12ParseTNSDataER16TBitstreamReaderIjERK11APACICSInfo]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACTNSData::ParseTNSData(APACTNSData *this, TBitstreamReader_u32 *bs, const APACICSInfo *ics)
{
  APACTNSData *v6; // x25
  unsigned int v7; // w3
  APACTNSData *v8; // x21
  unsigned int cache; // w3
  int v10; // w1
  char v11; // w20
  int v12; // w0
  unsigned int v13; // w20
  unsigned int v14; // w0
  int bitsLeft; // w1
  unsigned int v16; // w0
  unsigned int v17; // w1
  int v18; // w0
  unsigned int v19; // w23
  unsigned int v20; // w1
  char v21; // w23
  bool *v22; // x20
  bool *v23; // x28
  int v24; // w0
  bool *v25; // x5
  unsigned __int64 v26; // x26
  int v27; // w27
  __int64 v28; // x0
  int v29; // w0
  unsigned __int64 v30; // x26
  unsigned int v32; // w1
  int v33; // w0
  unsigned int v34; // w9
  bool *v35; // x26
  unsigned int v36; // w3
  char *v37; // x6
  unsigned int v38; // w5
  char *v39; // x6
  unsigned int v40; // w3
  char v41; // w27
  char *v42; // x26
  char v43; // w1
  unsigned int v44; // w9
  int v45; // w0
  const unsigned __int8 *v46; // x1
  unsigned int v47; // w9
  int v48; // w11
  unsigned int v49; // w10
  const unsigned __int8 *v50; // x1
  unsigned int v51; // w0
  int v52; // w1
  unsigned int v53; // w0
  unsigned int v54; // w0
  int v55; // w1
  unsigned int v56; // w0
  const unsigned __int8 *v57; // x1
  unsigned int v58; // w0
  int v59; // w1
  unsigned int v60; // w10
  unsigned int v61; // w0
  unsigned int v62; // w1
  int v63; // w5
  const unsigned __int8 *cur; // x3
  bool *v65; // [xsp+68h] [xbp+68h]
  unsigned int v66; // [xsp+74h] [xbp+74h]
  char *v67; // [xsp+78h] [xbp+78h]
  bool *v68; // [xsp+80h] [xbp+80h]
  unsigned int v69; // [xsp+88h] [xbp+88h]
  unsigned int v70; // [xsp+8Ch] [xbp+8Ch]
  __int64 v71; // [xsp+A0h] [xbp+A0h]
  v6 = this;
  if ( *((_BYTE *)ics + 10) == 2 )
    v7 = 8;
  else
    v7 = 1;
  *((_BYTE *)this + 1) = 0;
  v8 = (APACTNSData *)((char *)this + 23 * v7);
  while ( 1 )
  {
    cache = bs->cache;
    v10 = *((_BYTE *)ics + 10) == 2 ? 1 : 2;
    v11 = *((_BYTE *)ics + 10) == 2 ? 31 : 30;
    v12 = bs->bitsLeft - v10;
    bs->bitsLeft = v12;
    v13 = cache >> v11;
    if ( v12 < 0 )
      break;
    bs->cache = cache << v10;
    *((_BYTE *)this + 1) = v13 | (2 * *((_BYTE *)this + 1));
    if ( v13 )
      goto LABEL_17;
LABEL_6:
    v6 = (APACTNSData *)((char *)v6 + 23);
    if ( v8 == v6 )
      return 0;
  }
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v14 = bs->cache;
  bitsLeft = bs->bitsLeft;
  v13 |= v14 >> (bitsLeft + 32);
  v16 = v14 << -(char)bitsLeft;
  if ( bitsLeft == -32 )
    v16 = 0;
  bs->cur += 4;
  bs->cache = v16;
  bs->bitsLeft = bitsLeft + 32;
  *((_BYTE *)this + 1) = v13 | (2 * *((_BYTE *)this + 1));
  if ( !v13 )
    goto LABEL_6;
LABEL_17:
  v17 = bs->cache;
  v18 = bs->bitsLeft - 1;
  bs->bitsLeft = v18;
  v19 = v17 >> 31;
  if ( v18 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v62 = bs->cache;
    v63 = bs->bitsLeft;
    cur = bs->cur;
    v18 = v63 + 32;
    bs->bitsLeft = v63 + 32;
    bs->cur = cur + 4;
    LOBYTE(v19) = v19 | (v62 >> (v63 + 32));
    if ( v63 == -32 )
    {
      bs->cache = 0;
      v70 = (unsigned __int8)v19;
      v20 = 0;
    }
    else
    {
      v70 = (unsigned __int8)v19;
      v20 = v62 << -(char)v63;
      bs->cache = v20;
    }
  }
  else
  {
    v20 = 2 * v17;
    bs->cache = v20;
    v70 = v19;
  }
  v68 = (bool *)v6 + 2;
  v21 = 16 * (v19 & 0xF);
  v22 = (bool *)this + 23 * v13 + 2;
  v23 = (bool *)this + 2;
  if ( *((_BYTE *)ics + 10) == 2 )
  {
    while ( 1 )
    {
      v29 = v18 - 7;
      bs->bitsLeft = v29;
      v30 = v20 >> 25;
      if ( v29 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v54 = bs->cache;
        v55 = bs->bitsLeft;
        v30 = (unsigned int)v30 | (v54 >> (v55 + 32));
        v56 = v54 << -(char)v55;
        if ( v55 == -32 )
          v56 = 0;
        bs->cur += 4;
        bs->cache = v56;
        bs->bitsLeft = v55 + 32;
      }
      else
      {
        bs->cache = v20 << 7;
      }
      LOBYTE(v27) = *((_BYTE *)this + 187);
      v28 = (v30 >> 3) & 0xF;
      v25 = v68;
      if ( (v30 & 7) <= (unsigned __int8)v27 )
        LOBYTE(v27) = v30 & 7;
      v27 = (unsigned __int8)v27;
      if ( !(_DWORD)v28 )
        break;
LABEL_25:
      v25[1] = v28;
      v25[2] = v27;
      if ( v27 )
      {
        v32 = bs->cache;
        v33 = bs->bitsLeft - 2;
        bs->bitsLeft = v33;
        v34 = v32 >> 30;
        if ( v33 < 0 )
        {
          v57 = bs->cur;
          v65 = v25;
          v66 = v34;
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, v57);
          v58 = bs->cache;
          v59 = bs->bitsLeft;
          v60 = v58 >> (v59 + 32);
          v61 = v58 << -(char)v59;
          LOBYTE(v34) = v66 | v60;
          if ( v59 == -32 )
            v61 = 0;
          bs->cur += 4;
          bs->cache = v61;
          bs->bitsLeft = v59 + 32;
          v25 = v65;
        }
        else
        {
          bs->cache = 4 * v32;
        }
        v35 = v25;
        v36 = v70 - (v34 & 1);
        v37 = (char *)(v25 + 4);
        v38 = v36 + 3;
        v39 = &v37[v27 - 1];
        v40 = v36 + 2;
        v41 = 32 - v38;
        *v35 = (v34 & 2) != 0;
        v42 = (char *)(v35 + 3);
        do
        {
          v43 = 8;
          if ( v40 <= 0x1F )
          {
            v44 = bs->cache;
            v45 = bs->bitsLeft - v38;
            bs->bitsLeft = v45;
            if ( v45 < 0 )
            {
              v46 = bs->cur;
              LODWORD(v65) = v38;
              v66 = v44 >> v41;
              v67 = v39;
              v69 = v40;
              TBitstreamReader<unsigned int>::FillCacheFrom(bs, v46);
              v47 = bs->cache;
              v48 = bs->bitsLeft;
              v49 = v66;
              bs->cur += 4;
              bs->bitsLeft = v48 + 32;
              v38 = (unsigned int)v65;
              v40 = v69;
              v39 = v67;
              v43 = ((int)(((v47 >> (v48 + 32)) | v49) << v41) >> v41) + 8;
              if ( v48 == -32 )
                bs->cache = 0;
              else
                bs->cache = v47 << -(char)v48;
            }
            else
            {
              v43 = ((int)(v44 >> v41 << v41) >> v41) + 8;
              bs->cache = v44 << v38;
            }
          }
          *v42++ = v21 | v43;
        }
        while ( v42 != v39 );
      }
      v23 += 23;
      if ( v22 == v23 )
        goto LABEL_6;
      v20 = bs->cache;
      v18 = bs->bitsLeft;
      if ( *((_BYTE *)ics + 10) != 2 )
        goto LABEL_20;
    }
  }
  else
  {
LABEL_20:
    v24 = v18 - 11;
    bs->bitsLeft = v24;
    v25 = v23;
    v26 = v20 >> 21;
    if ( v24 < 0 )
    {
      v50 = bs->cur;
      v65 = v23;
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v50);
      v51 = bs->cache;
      v52 = bs->bitsLeft;
      v26 = (unsigned int)v26 | (v51 >> (v52 + 32));
      v53 = v51 << -(char)v52;
      if ( v52 == -32 )
        v53 = 0;
      bs->cur += 4;
      bs->cache = v53;
      bs->bitsLeft = v52 + 32;
      v25 = v65;
    }
    else
    {
      bs->cache = v20 << 11;
    }
    LOBYTE(v27) = *((_BYTE *)this + 186);
    v28 = (v26 >> 5) & 0x3F;
    if ( (v26 & 0x1F) <= (unsigned __int8)v27 )
      LOBYTE(v27) = v26 & 0x1F;
    v27 = (unsigned __int8)v27;
    if ( (_DWORD)v28 )
      goto LABEL_25;
  }
  return 0xFFFFFFFFLL;
}


/* ------------------------------------------------------------------------ */
/* 0x000af240  size 880 bytes  219 insns
 *   APACTNSData::Apply(APACICSInfo const&, apac::SFBOffsets const&, APACSpectralData&) const
 *   [_ZNK11APACTNSData5ApplyERK11APACICSInfoRKN4apac10SFBOffsetsER16APACSpectralData]
 *   data refs: 0x331970, 0x3319d0, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACTNSData::Apply(
        APACTNSData *this,
        const APACICSInfo *ics,
        const apac__SFBOffsets *a,
        APACSpectralData *spectrum)
{
  __int64 v4; // x6
  __int64 v5; // x19
  __int64 v6; // x20
  __int64 v7; // x21
  __int64 v8; // x22
  __int64 v9; // x23
  __int64 v10; // x24
  __int64 v11; // x25
  __int64 v12; // x26
  __int64 v13; // x27
  __int64 v14; // x28
  unsigned __int8 *v15; // x12
  int v16; // w0
  unsigned int v17; // w25
  __int64 v18; // x11
  unsigned __int16 *v19; // x8
  unsigned int v20; // w0
  int v21; // w13
  unsigned int v22; // w25
  unsigned __int8 *v23; // x26
  unsigned __int16 *v24; // x28
  int v25; // w24
  __int64 v26; // x23
  int v27; // w10
  float *v28; // x9
  const APACICSInfo *v29; // x8
  int v30; // w0
  __int64 v31; // x1
  __int64 v32; // x22
  __int64 v33; // x0
  unsigned __int16 *v34; // x20
  unsigned __int8 *v35; // x23
  __int64 v36; // x28
  __int64 v37; // x21
  const APACICSInfo *v38; // x24
  unsigned __int8 *v39; // x15
  unsigned int v40; // w4
  int v41; // w22
  __int64 v42; // x0
  unsigned __int16 *v43; // x0
  int v44; // w0
  unsigned int v45; // w14
  float *v47; // x13
  float *v48; // x2
  __int64 v49; // x1
  __int64 v50; // x3
  __int64 v51; // x12
  int v52; // w11
  float v53; // s1
  __int64 v54; // x10
  float *v55; // x5
  __int64 v56; // x2
  __int64 v57; // x1
  __int64 v58; // x7
  float v59; // s3
  __int64 v60; // x1
  float *v61; // x5
  float *v62; // x2
  unsigned int v63; // w0
  int v64; // w1
  int v65; // w2
  __int64 v66; // [xsp+0h] [xbp+0h]
  __int64 v67; // [xsp+10h] [xbp+10h]
  __int64 v68; // [xsp+18h] [xbp+18h]
  __int64 v69; // [xsp+20h] [xbp+20h]
  __int64 v70; // [xsp+28h] [xbp+28h]
  __int64 v71; // [xsp+30h] [xbp+30h]
  __int64 v72; // [xsp+38h] [xbp+38h]
  __int64 v73; // [xsp+40h] [xbp+40h]
  __int64 v74; // [xsp+48h] [xbp+48h]
  __int64 v75; // [xsp+50h] [xbp+50h]
  __int64 v76; // [xsp+58h] [xbp+58h]
  int v77; // [xsp+60h] [xbp+60h]
  int v78; // [xsp+64h] [xbp+64h]
  unsigned __int8 *v79; // [xsp+68h] [xbp+68h]
  __int64 v80; // [xsp+70h] [xbp+70h]
  unsigned __int16 *v81; // [xsp+78h] [xbp+78h]
  unsigned __int8 *v82; // [xsp+80h] [xbp+80h]
  float *v83; // [xsp+88h] [xbp+88h]
  _BYTE v84[80]; // [xsp+98h] [xbp+98h] BYREF
  _QWORD v85[23]; // [xsp+E8h] [xbp+E8h] BYREF
  __int64 v86; // [xsp+1A0h] [xbp+1A0h]
  v15 = (unsigned __int8 *)this;
  if ( *(_BYTE *)this )
  {
    v16 = *((unsigned __int8 *)ics + 10);
    LODWORD(v80) = *(unsigned __int16 *)a;
    v17 = *((unsigned __int8 *)ics + 11);
    v18 = *(_QWORD *)spectrum;
    v19 = *((unsigned __int16 **)a + 1);
    if ( v16 == 2 )
    {
      v63 = v15[189];
      v21 = 8;
      if ( v63 > v17 )
        LOBYTE(v63) = *((_BYTE *)ics + 11);
      v22 = (unsigned __int8)v63;
    }
    else
    {
      v20 = v15[188];
      v21 = 1;
      if ( v20 > v17 )
        LOBYTE(v20) = *((_BYTE *)ics + 11);
      v22 = (unsigned __int8)v20;
    }
    v23 = v15 + 2;
    v81 = &v19[(unsigned __int16)v80];
    v82 = v15 + 2;
    v24 = v19;
    v25 = v21;
    v26 = v18;
    v27 = 0;
    v28 = (float *)v84;
    v29 = ics;
    while ( 1 )
    {
      v30 = ((int)v15[1] >> (7 - v27)) & 1;
      if ( v25 != 8 )
        v30 = v15[1];
      if ( v30 )
        break;
LABEL_27:
      ++v27;
      v23 += 23;
      v26 += 4LL * *v81;
      if ( v25 == v27 )
      {
        v5 = v67;
        v6 = v68;
        v7 = v69;
        v8 = v70;
        v9 = v71;
        v10 = v72;
        v11 = v73;
        v12 = v74;
        v13 = v75;
        v14 = v76;
        goto LABEL_29;
      }
    }
    LODWORD(v31) = v80;
    v32 = (__int64)&v15[23 * (v30 - 1) + 25];
    v33 = v26;
    v34 = v24;
    v35 = v82;
    v36 = v32;
    v37 = v33;
    v77 = v25;
    v78 = v27;
    v38 = v29;
    v79 = v15;
    while ( 1 )
    {
      v39 = *((_BYTE *)v38 + 10) == 2 ? v23 : v35;
      v40 = v39[2];
      v41 = v31 - v39[1];
      if ( v41 < 0 )
        break;
      if ( v39[2] )
      {
        if ( v41 > v22 )
          v42 = v22;
        else
          v42 = (unsigned int)v41;
        v43 = &v34[v42];
LABEL_20:
        v44 = *v43;
        if ( (unsigned int)v31 > v22 )
          v31 = v22;
        else
          v31 = (unsigned int)v31;
        v45 = v34[v31];
        if ( v44 < (int)v45 )
        {
          v47 = v28;
          v48 = v28;
          v49 = 0;
          *(_QWORD *)v28 = 0;
          *((_QWORD *)v28 + 1) = 0;
          v85[0] = 0;
          v85[1] = 0;
          *((_QWORD *)v28 + 2) = 0;
          *((_QWORD *)v28 + 3) = 0;
          v85[2] = 0;
          v85[3] = 0;
          *((_QWORD *)v28 + 4) = 0;
          *((_QWORD *)v28 + 5) = 0;
          v85[4] = 0;
          v85[5] = 0;
          *((_QWORD *)v28 + 6) = 0;
          *((_QWORD *)v28 + 7) = 0;
          v85[6] = 0;
          v85[7] = 0;
          *((_QWORD *)v28 + 8) = 0;
          *((_QWORD *)v28 + 9) = 0;
          memset(&v85[8], 0, 24);
          do
          {
            v50 = v39[v49++ + 3];
            *v48++ = flt_3319D0[v50];
          }
          while ( v40 > (unsigned int)v49 );
          v51 = 1;
          v52 = 1;
          v53 = 1.0;
          v54 = 3;
          LODWORD(v85[0]) = 1.0;
          while ( 1 )
          {
            v55 = (float *)&v85[11] + v54;
            v56 = 1;
            v57 = 4;
            *(v55 - 2) = *v47;
            while ( 1 )
            {
              ++v56;
              *(float *)&v84[v57 + 76] = v53;
              if ( v54 == v56 )
                break;
              v57 = 4 * v56;
              v55 = (float *)&v85[11] + v56;
              v53 = *(v55 - 1);
            }
            v58 = (unsigned int)(v52 + 1);
            if ( (unsigned int)v58 > v40 )
              break;
            v59 = v47[1];
            v60 = 0;
            v53 = *(float *)v85;
            do
            {
              v4 = (unsigned int)(v52 - v60);
              v61 = (float *)v85 + v60;
              v62 = (float *)&v85[11] + v60++;
              v62[1] = v61[1] + (float)(v59 * *((float *)v85 + v4));
            }
            while ( v60 != v51 );
            ++v51;
            ++v47;
            ++v54;
            ++v52;
          }
          v64 = *v39;
          v65 = v45 - v44;
          v83 = v28;
          if ( v64 )
            sub_AEC20((float *)(v37 + 4LL * v45 - 4), -1, v65, (__int64)v85, v40, (__int64)v55, v4, v58, v66);
          else
            sub_AEC20(
              (float *)(v37 + 4LL * (unsigned __int16)v44),
              1,
              v65,
              (__int64)v85,
              v40,
              (__int64)v55,
              v4,
              v58,
              v66);
          v28 = v83;
        }
      }
      LODWORD(v31) = v41;
LABEL_25:
      v35 += 23;
      if ( (unsigned __int8 *)v36 == v35 )
      {
        v29 = v38;
        v24 = v34;
        v25 = v77;
        v27 = v78;
        v26 = v37;
        v15 = v79;
        goto LABEL_27;
      }
    }
    if ( !v39[2] )
    {
      LODWORD(v31) = 0;
      goto LABEL_25;
    }
    v43 = v34;
    v41 = 0;
    goto LABEL_20;
  }
LABEL_29:
  if ( v85[22] != _stack_chk_guard )
  {
    v67 = v5;
    v68 = v6;
    v69 = v7;
    v70 = v8;
    v71 = v9;
    v72 = v10;
    v73 = v11;
    v74 = v12;
    v75 = v13;
    v76 = v14;
  }
  return 0;
}


