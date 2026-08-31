/* ============================================================================
 * APACCACDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 1824 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0007b5a0    792  APACCACDecoder::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&) */
/* 0x0007b8c0   1032  APACCACDecoder::ProcessCac(APACICSInfo const&, APACIndividualChannelStream&, APACIndividualChannelStream&) const */

/* ------------------------------------------------------------------------ */
/* 0x0007b5a0  size 792 bytes  198 insns
 *   APACCACDecoder::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&)
 *   [_ZN14APACCACDecoder11DeserializeER16TBitstreamReaderIjERK11APACICSInfo]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCACDecoder::Deserialize(APACCACDecoder *this, TBitstreamReader_u32 *bs, const APACICSInfo *ics)
{
  __int64 v3; // x19
  __int64 v4; // x20
  __int64 v5; // x21
  TBitstreamReader_u32 *v6; // x22
  __int64 v7; // x27
  __int64 v8; // x28
  __int64 result; // x0
  char *v11; // x28
  char *v12; // x0
  unsigned int cache; // w2
  char *v14; // x5
  int v15; // w19
  char *v16; // x23
  __int64 v17; // x26
  char *v18; // x3
  int v19; // w0
  int v20; // w19
  unsigned int v21; // w2
  int v22; // w1
  unsigned int v23; // w27
  char *v24; // x3
  int v25; // w1
  unsigned int v26; // w3
  int v27; // w2
  unsigned int v28; // w27
  const unsigned __int8 *cur; // x1
  unsigned int v30; // w0
  int bitsLeft; // w1
  unsigned int v32; // w0
  const unsigned __int8 *v33; // x1
  unsigned int v34; // w2
  int v35; // w3
  const unsigned __int8 *v36; // x0
  int v37; // w0
  const unsigned __int8 *v38; // x1
  unsigned int v39; // w2
  int v40; // w3
  const unsigned __int8 *v41; // x0
  int v42; // w0
  __int64 v43; // [xsp+10h] [xbp+10h]
  __int64 v44; // [xsp+18h] [xbp+18h]
  __int64 v45; // [xsp+20h] [xbp+20h]
  TBitstreamReader_u32 *v46; // [xsp+28h] [xbp+28h]
  __int64 v47; // [xsp+50h] [xbp+50h]
  __int64 v48; // [xsp+58h] [xbp+58h]
  char *v49; // [xsp+68h] [xbp+68h]
  __int64 v50; // [xsp+78h] [xbp+78h]
  __int64 v51; // [xsp+80h] [xbp+80h]
  result = 0;
  if ( *((_BYTE *)ics + 11) )
  {
    v43 = v3;
    v45 = v5;
    v6 = bs;
    v47 = v7;
    v48 = v8;
    v11 = (char *)CacDecoder::GainIndexCodeTableDec((APACCACDecoder *)((char *)this + 24));
    v12 = (char *)CacDecoder::RepeatCodeTableDec((APACCACDecoder *)((char *)this + 24));
    cache = v6->cache;
    v14 = v12;
    LODWORD(v12) = v6->bitsLeft - 16;
    v6->bitsLeft = (int)v12;
    v15 = HIWORD(cache);
    if ( ((unsigned int)v12 & 0x80000000) != 0 )
    {
      cur = v6->cur;
      v49 = v14;
      TBitstreamReader<unsigned int>::FillCacheFrom(v6, cur);
      v30 = v6->cache;
      bitsLeft = v6->bitsLeft;
      v15 |= v30 >> (bitsLeft + 32);
      v32 = v30 << -(char)bitsLeft;
      if ( bitsLeft == -32 )
        v32 = 0;
      v6->cur += 4;
      v6->cache = v32;
      v6->bitsLeft = bitsLeft + 32;
      v14 = v49;
    }
    else
    {
      v6->cache = cache << 16;
    }
    v15 = (unsigned __int16)v15;
    v16 = v14 + 264;
    v17 = 0;
    while ( 2 )
    {
      v18 = v11;
      while ( *((unsigned __int16 *)v18 + 1) != v15 >> (16 - v18[4]) )
      {
        v18 += 6;
        if ( v18 == v11 + 210 )
          goto LABEL_21;
      }
      *(_BYTE *)(*((_QWORD *)this + 4) + v17) = *v18;
      v19 = (unsigned __int8)v18[4];
      v20 = (unsigned __int16)(v15 << v19);
      if ( (unsigned int)(v19 - 1) <= 0x1F )
      {
        v21 = v6->cache;
        v22 = v6->bitsLeft - v19;
        v6->bitsLeft = v22;
        v23 = v21 >> (32 - v19);
        if ( v22 < 0 )
        {
          v33 = v6->cur;
          v49 = v14;
          TBitstreamReader<unsigned int>::FillCacheFrom(v6, v33);
          v34 = v6->cache;
          v35 = v6->bitsLeft;
          v36 = v6->cur;
          v6->bitsLeft = v35 + 32;
          v6->cur = v36 + 4;
          v37 = (v34 >> (v35 + 32)) | v23;
          v14 = v49;
          if ( v35 == -32 )
          {
            v6->cache = 0;
            v20 = (unsigned __int16)(v20 | v37);
          }
          else
          {
            v20 = (unsigned __int16)(v20 | v37);
            v6->cache = v34 << -(char)v35;
          }
        }
        else
        {
          v20 = (unsigned __int16)(v20 | v23);
          v6->cache = v21 << v19;
        }
      }
      v24 = v14;
      while ( *((unsigned __int16 *)v24 + 1) != v20 >> (16 - v24[4]) )
      {
        v24 += 6;
        if ( v24 == v16 )
          goto LABEL_21;
      }
      *(_BYTE *)(*((_QWORD *)this + 7) + v17) = *v24;
      v25 = (unsigned __int8)v24[4];
      v15 = (unsigned __int16)(v20 << v25);
      if ( (unsigned int)(v25 - 1) <= 0x1F )
      {
        v26 = v6->cache;
        v27 = v6->bitsLeft - v25;
        v6->bitsLeft = v27;
        v28 = v26 >> (32 - v25);
        if ( v27 < 0 )
        {
          v38 = v6->cur;
          v49 = v14;
          TBitstreamReader<unsigned int>::FillCacheFrom(v6, v38);
          v39 = v6->cache;
          v40 = v6->bitsLeft;
          v41 = v6->cur;
          v6->bitsLeft = v40 + 32;
          v6->cur = v41 + 4;
          v42 = (v39 >> (v40 + 32)) | v28;
          v14 = v49;
          if ( v40 == -32 )
          {
            v6->cache = 0;
            v15 = (unsigned __int16)(v15 | v42);
          }
          else
          {
            v15 = (unsigned __int16)(v15 | v42);
            v6->cache = v39 << -(char)v40;
          }
        }
        else
        {
          v15 = (unsigned __int16)(v15 | v28);
          v6->cache = v26 << v25;
        }
      }
      if ( *(_BYTE *)(*((_QWORD *)this + 7) + v17) != 43 )
      {
        if ( ++v17 != 120 )
          continue;
LABEL_21:
        result = 1;
        v3 = v43;
        v5 = v45;
        v7 = v47;
        v8 = v48;
        goto LABEL_2;
      }
      break;
    }
    TBitstreamReader<unsigned int>::PutBack(v6, 0x10u);
    result = 0;
    v3 = v43;
    v5 = v45;
    v7 = v47;
    v8 = v48;
  }
LABEL_2:
  if ( v50 != _stack_chk_guard )
  {
    v43 = v3;
    v44 = v4;
    v45 = v5;
    v46 = v6;
    v47 = v7;
    v48 = v8;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0007b8c0  size 1032 bytes  256 insns
 *   APACCACDecoder::ProcessCac(APACICSInfo const&, APACIndividualChannelStream&, APACIndividualChannelStream&) const
 *   [_ZNK14APACCACDecoder10ProcessCacERK11APACICSInfoR27APACIndividualChannelStreamS4_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCACDecoder::ProcessCac(
        APACCACDecoder *this,
        const APACICSInfo *ics,
        APACIndividualChannelStream *ics_stream,
        APACIndividualChannelStream *ics_stream2)
{
  __int64 v4; // x19
  __int64 v5; // x20
  __int64 v6; // x23
  __int64 v7; // x24
  __int64 v8; // x25
  __int64 v9; // x26
  __int64 v10; // x27
  __int64 v11; // x28
  __int64 v12; // d8
  __int64 v13; // d9
  __int64 v14; // d10
  unsigned int v16; // w9
  unsigned int v17; // w12
  __int64 *v19; // x4
  int v20; // w15
  float32x4_t *v21; // x28
  __int64 v22; // x0
  float32x4_t *v23; // x25
  unsigned __int16 *v24; // x14
  unsigned __int16 *v25; // x0
  __int64 v26; // x20
  int v27; // w13
  unsigned int v28; // w6
  int v29; // w11
  int v30; // w10
  char v31; // w5
  unsigned int v32; // w4
  int v33; // w3
  int v34; // w2
  int v35; // w26
  unsigned int v36; // w23
  int v37; // w3
  unsigned int v38; // w27
  int v39; // w0
  unsigned __int64 v40; // x22
  float v41; // s0
  float v42; // s3
  int v43; // s1
  double v44; // d0
  int v45; // s2
  double v46; // d1
  long double v47; // q2
  __int64 v48; // x4
  __int64 v49; // x5
  __int64 v50; // x6
  __int64 v51; // x7
  __int64 result; // x0
  __int64 v53; // x1
  int v54; // w27
  unsigned int v55; // w23
  int v56; // w22
  int v57; // w3
  unsigned __int64 v58; // x26
  __int64 v59; // x4
  __int64 v60; // x6
  __int64 v61; // x7
  double v62; // d0
  double v63; // d1
  long double v64; // q2
  int v65; // s8
  int v66; // s9
  unsigned int v67; // w9
  unsigned int v68; // w5
  __int64 v69; // x5
  int v70; // w0
  unsigned int v71; // w1
  __int64 v72; // x0
  __int64 v73; // [xsp+0h] [xbp+0h]
  __int64 v74; // [xsp+10h] [xbp+10h]
  __int64 v75; // [xsp+18h] [xbp+18h]
  __int64 v76; // [xsp+30h] [xbp+30h]
  __int64 v77; // [xsp+38h] [xbp+38h]
  __int64 v78; // [xsp+40h] [xbp+40h]
  __int64 v79; // [xsp+48h] [xbp+48h]
  __int64 v80; // [xsp+50h] [xbp+50h]
  __int64 v81; // [xsp+58h] [xbp+58h]
  __int64 v82; // [xsp+60h] [xbp+60h]
  __int64 v83; // [xsp+68h] [xbp+68h]
  __int64 v84; // [xsp+70h] [xbp+70h]
  unsigned int v85; // [xsp+80h] [xbp+80h]
  unsigned int v86; // [xsp+84h] [xbp+84h]
  int v87; // [xsp+88h] [xbp+88h]
  int v88; // [xsp+8Ch] [xbp+8Ch]
  _BOOL4 v89; // [xsp+90h] [xbp+90h]
  int v90; // [xsp+94h] [xbp+94h]
  unsigned int v91; // [xsp+98h] [xbp+98h]
  unsigned int v92; // [xsp+9Ch] [xbp+9Ch]
  float v93; // [xsp+A0h] [xbp+A0h]
  int v94; // [xsp+A4h] [xbp+A4h]
  int v95; // [xsp+A8h] [xbp+A8h]
  _BYTE v96[120]; // [xsp+B0h] [xbp+B0h] BYREF
  __int64 v97; // [xsp+128h] [xbp+128h] BYREF
  __int64 v98; // [xsp+130h] [xbp+130h]
  v16 = *((unsigned __int8 *)ics + 11);
  if ( !*((_BYTE *)ics + 11) )
    goto LABEL_24;
  v17 = *((unsigned __int8 *)ics + 11);
  v19 = (__int64 *)v96;
  v20 = *((unsigned __int8 *)ics + 10);
  v21 = *((float32x4_t **)ics_stream2 + 94);
  v22 = *(_QWORD *)(*((_QWORD *)this + 10) + 16LL);
  v23 = *((float32x4_t **)ics_stream + 94);
  v24 = (unsigned __int16 *)(v22 + 24);
  v25 = (unsigned __int16 *)(v22 + 8);
  if ( v20 != 2 )
    v24 = v25;
  v26 = *((_QWORD *)v24 + 1);
  do
  {
    *(_BYTE *)v19 = 0;
    v19 = (__int64 *)((char *)v19 + 1);
  }
  while ( &v97 != v19 );
  v27 = *((unsigned __int8 *)ics + 12);
  if ( !*((_BYTE *)ics + 12) )
  {
    if ( v20 == 2 )
    {
      result = 0;
      v4 = v74;
      v5 = v75;
      v6 = v76;
      v7 = v77;
      v8 = v78;
      v9 = v79;
      v10 = v80;
      v11 = v81;
      goto LABEL_45;
    }
    v82 = v12;
    v83 = v13;
    goto LABEL_14;
  }
  v28 = v16;
  v29 = 0;
  v30 = 0;
  v31 = 0;
  v32 = 0;
  v33 = -1;
  v82 = v12;
  v83 = v13;
  do
  {
    v34 = v29;
    do
    {
      if ( v33 == -1 )
      {
        v31 = *(_BYTE *)(*((_QWORD *)this + 4) + v32);
        v33 = *(unsigned __int8 *)(*((_QWORD *)this + 7) + v32++);
      }
      v96[v34++] = v31;
      --v33;
    }
    while ( v28 != v34 );
    ++v30;
    v28 += v16;
    v29 += v16;
  }
  while ( v30 != v27 );
  if ( v20 != 2 )
  {
LABEL_14:
    v35 = 0;
    v36 = 0;
LABEL_15:
    if ( v17 )
    {
      v37 = v35;
      v38 = v36 + v17;
      while ( 1 )
      {
        v39 = v36++;
        if ( v17 < v36 )
          break;
        v35 = *(unsigned __int16 *)(v26 + 2LL * v36);
        v40 = (unsigned int)(v35 - v37);
        if ( v96[v39] )
        {
          CacQuantizer::GainTableEntry(this, (unsigned __int8)v96[v39]);
          v42 = v41;
          LODWORD(v44) = v43;
          LODWORD(v46) = v45;
          *(float *)&v47 = fabsf(v42);
          v93 = v42;
          v94 = LODWORD(v44);
          v95 = LODWORD(v46);
          sub_7B070(*(float *)&v47 >= 0.999, v23, v21, v40, v44, v46, v47, v48, v49, v50, v51, v73);
          v17 = *((unsigned __int8 *)ics + 11);
          v23 = (float32x4_t *)((char *)v23 + 4 * v40);
          v21 = (float32x4_t *)((char *)v21 + 4 * v40);
          if ( v38 == v36 )
          {
LABEL_22:
            if ( v36 < v17 )
              goto LABEL_15;
            goto LABEL_23;
          }
        }
        else
        {
          v23 = (float32x4_t *)((char *)v23 + 4 * v40);
          v21 = (float32x4_t *)((char *)v21 + 4 * v40);
          if ( v38 == v36 )
            goto LABEL_22;
        }
        v37 = v35;
      }
LABEL_44:
      v4 = v74;
      v5 = v75;
      result = 1;
      v6 = v76;
      v7 = v77;
      v8 = v78;
      v9 = v79;
      v10 = v80;
      v11 = v81;
      v12 = v82;
      v13 = v83;
      goto LABEL_45;
    }
LABEL_23:
    v4 = v74;
    v5 = v75;
    v6 = v76;
    v7 = v77;
    v8 = v78;
    v9 = v79;
    v10 = v80;
    v11 = v81;
    v12 = v82;
    v13 = v83;
LABEL_24:
    result = 0;
    goto LABEL_45;
  }
  v84 = v14;
  v53 = *v24;
  v54 = 0;
  v92 = 0;
  v88 = *(unsigned __int16 *)(v26 + 2 * v53);
  do
  {
    v55 = 0;
    v56 = 0;
    v87 = 4 * v92;
LABEL_27:
    if ( v16 )
    {
      v57 = v56;
      v91 = v55 + v16;
      while ( 1 )
      {
        if ( v16 < ++v55 )
        {
          v14 = v84;
          goto LABEL_44;
        }
        v56 = *(unsigned __int16 *)(v26 + 2LL * v55);
        v58 = (unsigned int)(v56 - v57);
        if ( v96[v54] )
        {
          CacQuantizer::GainTableEntry(this, (unsigned __int8)v96[v54]);
          v65 = LODWORD(v63);
          v93 = *(float *)&v62;
          v94 = LODWORD(v63);
          v66 = LODWORD(v64);
          v95 = LODWORD(v64);
          if ( v92 <= 7 )
          {
            v67 = 0;
            v68 = 0;
            v89 = fabsf(*(float *)&v62) >= 0.999;
            while ( ((*((_DWORD *)ics + 4) >> v87) & 0xFu) > v68 )
            {
              LODWORD(v63) = v66;
              LODWORD(v62) = v65;
              v69 = v68 + 1;
              v85 = v67 + v88;
              v86 = v69;
              v90 = v67 + v88;
              sub_7B070(
                v89,
                (float32x4_t *)((char *)v23 + 4 * v67),
                (float32x4_t *)((char *)v21 + 4 * v67),
                v58,
                v62,
                v63,
                v64,
                v59,
                v69,
                v60,
                v61,
                v73);
              v67 = v85;
              v68 = v86;
            }
          }
          v16 = *((unsigned __int8 *)ics + 11);
          ++v54;
          v23 = (float32x4_t *)((char *)v23 + 4 * v58);
          v21 = (float32x4_t *)((char *)v21 + 4 * v58);
          if ( v91 == v55 )
          {
LABEL_38:
            if ( v55 < v16 )
              goto LABEL_27;
            break;
          }
        }
        else
        {
          ++v54;
          v23 = (float32x4_t *)((char *)v23 + 4 * v58);
          v21 = (float32x4_t *)((char *)v21 + 4 * v58);
          if ( v91 == v55 )
            goto LABEL_38;
        }
        v57 = v56;
      }
    }
    v70 = 0;
    if ( v92 <= 7 )
      v70 = ((*((_DWORD *)ics + 4) >> v87) & 0xF) * v88;
    v71 = *((unsigned __int8 *)ics + 12);
    ++v92;
    v72 = 4LL * (unsigned int)(v70 - v56);
    v23 = (float32x4_t *)((char *)v23 + v72);
    v21 = (float32x4_t *)((char *)v21 + v72);
  }
  while ( v71 > v92 );
  result = 0;
  v14 = v84;
  v4 = v74;
  v5 = v75;
  v6 = v76;
  v7 = v77;
  v8 = v78;
  v9 = v79;
  v10 = v80;
  v11 = v81;
  v12 = v82;
  v13 = v83;
LABEL_45:
  if ( v97 != _stack_chk_guard )
  {
    v74 = v4;
    v75 = v5;
    v76 = v6;
    v77 = v7;
    v78 = v8;
    v79 = v9;
    v80 = v10;
    v81 = v11;
    v82 = v12;
    v83 = v13;
    v84 = v14;
  }
  return result;
}


