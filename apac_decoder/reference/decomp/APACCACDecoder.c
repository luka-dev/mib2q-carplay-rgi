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
  __int64 v11; // x28
  __int64 v12; // x0
  unsigned int cache; // w2
  __int64 v14; // x5
  int v15; // w19
  __int64 v16; // x23
  __int64 v17; // x26
  __int64 v18; // x3
  int v19; // w0
  int v20; // w19
  unsigned int v21; // w2
  int v22; // w1
  unsigned int v23; // w27
  __int64 v24; // x3
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
  __int64 v49; // [xsp+68h] [xbp+68h]
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
    v11 = CacDecoder::GainIndexCodeTableDec((APACCACDecoder *)((char *)this + 24));
    v12 = CacDecoder::RepeatCodeTableDec((APACCACDecoder *)((char *)this + 24));
    cache = v6->cache;
    v14 = v12;
    LODWORD(v12) = v6->bitsLeft - 16;
    v6->bitsLeft = v12;
    v15 = HIWORD(cache);
    if ( (v12 & 0x80000000) != 0 )
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
      while ( *(unsigned __int16 *)(v18 + 2) != v15 >> (16 - *(_BYTE *)(v18 + 4)) )
      {
        v18 += 6;
        if ( v18 == v11 + 210 )
          goto LABEL_21;
      }
      *(_BYTE *)(*((_QWORD *)this + 4) + v17) = *(_BYTE *)v18;
      v19 = *(unsigned __int8 *)(v18 + 4);
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
      while ( *(unsigned __int16 *)(v24 + 2) != v20 >> (16 - *(_BYTE *)(v24 + 4)) )
      {
        v24 += 6;
        if ( v24 == v16 )
          goto LABEL_21;
      }
      *(_BYTE *)(*((_QWORD *)this + 7) + v17) = *(_BYTE *)v24;
      v25 = *(unsigned __int8 *)(v24 + 4);
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
  __int64 v21; // x28
  __int64 v22; // x0
  __int64 v23; // x25
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
  __int64 v40; // x22
  float v41; // s0
  float v42; // s1
  float v43; // s2
  __int64 result; // x0
  __int64 v45; // x1
  int v46; // w27
  unsigned int v47; // w23
  int v48; // w22
  int v49; // w3
  __int64 v50; // x26
  float v51; // s0
  float v52; // s1
  float v53; // s8
  float v54; // s2
  float v55; // s9
  unsigned int v56; // w9
  unsigned int v57; // w5
  int v58; // w0
  unsigned int v59; // w1
  __int64 v60; // x0
  __int64 v61; // [xsp+10h] [xbp+10h]
  __int64 v62; // [xsp+18h] [xbp+18h]
  __int64 v63; // [xsp+30h] [xbp+30h]
  __int64 v64; // [xsp+38h] [xbp+38h]
  __int64 v65; // [xsp+40h] [xbp+40h]
  __int64 v66; // [xsp+48h] [xbp+48h]
  __int64 v67; // [xsp+50h] [xbp+50h]
  __int64 v68; // [xsp+58h] [xbp+58h]
  __int64 v69; // [xsp+60h] [xbp+60h]
  __int64 v70; // [xsp+68h] [xbp+68h]
  __int64 v71; // [xsp+70h] [xbp+70h]
  unsigned int v72; // [xsp+80h] [xbp+80h]
  unsigned int v73; // [xsp+84h] [xbp+84h]
  int v74; // [xsp+88h] [xbp+88h]
  int v75; // [xsp+8Ch] [xbp+8Ch]
  _BOOL4 v76; // [xsp+90h] [xbp+90h]
  int v77; // [xsp+94h] [xbp+94h]
  unsigned int v78; // [xsp+98h] [xbp+98h]
  unsigned int v79; // [xsp+9Ch] [xbp+9Ch]
  float v80; // [xsp+A0h] [xbp+A0h]
  float v81; // [xsp+A4h] [xbp+A4h]
  float v82; // [xsp+A8h] [xbp+A8h]
  _BYTE v83[120]; // [xsp+B0h] [xbp+B0h] BYREF
  __int64 v84; // [xsp+128h] [xbp+128h] BYREF
  __int64 v85; // [xsp+130h] [xbp+130h]
  v16 = *((unsigned __int8 *)ics + 11);
  if ( !*((_BYTE *)ics + 11) )
    goto LABEL_24;
  v17 = *((unsigned __int8 *)ics + 11);
  v19 = (__int64 *)v83;
  v20 = *((unsigned __int8 *)ics + 10);
  v21 = *((_QWORD *)ics_stream2 + 94);
  v22 = *(_QWORD *)(*((_QWORD *)this + 10) + 16LL);
  v23 = *((_QWORD *)ics_stream + 94);
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
  while ( &v84 != v19 );
  v27 = *((unsigned __int8 *)ics + 12);
  if ( !*((_BYTE *)ics + 12) )
  {
    if ( v20 == 2 )
    {
      result = 0;
      v4 = v61;
      v5 = v62;
      v6 = v63;
      v7 = v64;
      v8 = v65;
      v9 = v66;
      v10 = v67;
      v11 = v68;
      goto LABEL_45;
    }
    v69 = v12;
    v70 = v13;
    goto LABEL_14;
  }
  v28 = v16;
  v29 = 0;
  v30 = 0;
  v31 = 0;
  v32 = 0;
  v33 = -1;
  v69 = v12;
  v70 = v13;
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
      v83[v34++] = v31;
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
        if ( v83[v39] )
        {
          CacQuantizer::GainTableEntry(this, (unsigned __int8)v83[v39]);
          v80 = v41;
          v81 = v42;
          v82 = v43;
          sub_7B070(fabsf(v41) >= 0.999, v23, v21, v40, v42, v43);
          v17 = *((unsigned __int8 *)ics + 11);
          v23 += 4 * v40;
          v21 += 4 * v40;
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
          v23 += 4 * v40;
          v21 += 4 * v40;
          if ( v38 == v36 )
            goto LABEL_22;
        }
        v37 = v35;
      }
LABEL_44:
      v4 = v61;
      v5 = v62;
      result = 1;
      v6 = v63;
      v7 = v64;
      v8 = v65;
      v9 = v66;
      v10 = v67;
      v11 = v68;
      v12 = v69;
      v13 = v70;
      goto LABEL_45;
    }
LABEL_23:
    v4 = v61;
    v5 = v62;
    v6 = v63;
    v7 = v64;
    v8 = v65;
    v9 = v66;
    v10 = v67;
    v11 = v68;
    v12 = v69;
    v13 = v70;
LABEL_24:
    result = 0;
    goto LABEL_45;
  }
  v71 = v14;
  v45 = *v24;
  v46 = 0;
  v79 = 0;
  v75 = *(unsigned __int16 *)(v26 + 2 * v45);
  do
  {
    v47 = 0;
    v48 = 0;
    v74 = 4 * v79;
LABEL_27:
    if ( v16 )
    {
      v49 = v48;
      v78 = v47 + v16;
      while ( 1 )
      {
        if ( v16 < ++v47 )
        {
          v14 = v71;
          goto LABEL_44;
        }
        v48 = *(unsigned __int16 *)(v26 + 2LL * v47);
        v50 = (unsigned int)(v48 - v49);
        if ( v83[v46] )
        {
          CacQuantizer::GainTableEntry(this, (unsigned __int8)v83[v46]);
          v53 = v52;
          v80 = v51;
          v81 = v52;
          v55 = v54;
          v82 = v54;
          if ( v79 <= 7 )
          {
            v56 = 0;
            v57 = 0;
            v76 = fabsf(v51) >= 0.999;
            while ( ((*((_DWORD *)ics + 4) >> v74) & 0xFu) > v57 )
            {
              v72 = v56 + v75;
              v73 = v57 + 1;
              v77 = v56 + v75;
              sub_7B070(v76, v23 + 4LL * v56, v21 + 4LL * v56, v50, v53, v55);
              v56 = v72;
              v57 = v73;
            }
          }
          v16 = *((unsigned __int8 *)ics + 11);
          ++v46;
          v23 += 4 * v50;
          v21 += 4 * v50;
          if ( v78 == v47 )
          {
LABEL_38:
            if ( v47 < v16 )
              goto LABEL_27;
            break;
          }
        }
        else
        {
          ++v46;
          v23 += 4 * v50;
          v21 += 4 * v50;
          if ( v78 == v47 )
            goto LABEL_38;
        }
        v49 = v48;
      }
    }
    v58 = 0;
    if ( v79 <= 7 )
      v58 = ((*((_DWORD *)ics + 4) >> v74) & 0xF) * v75;
    v59 = *((unsigned __int8 *)ics + 12);
    ++v79;
    v60 = 4LL * (unsigned int)(v58 - v48);
    v23 += v60;
    v21 += v60;
  }
  while ( v59 > v79 );
  result = 0;
  v14 = v71;
  v4 = v61;
  v5 = v62;
  v6 = v63;
  v7 = v64;
  v8 = v65;
  v9 = v66;
  v10 = v67;
  v11 = v68;
  v12 = v69;
  v13 = v70;
LABEL_45:
  if ( v84 != _stack_chk_guard )
  {
    v61 = v4;
    v62 = v5;
    v63 = v6;
    v64 = v7;
    v65 = v8;
    v66 = v9;
    v67 = v10;
    v68 = v11;
    v69 = v12;
    v70 = v13;
    v71 = v14;
  }
  return result;
}


