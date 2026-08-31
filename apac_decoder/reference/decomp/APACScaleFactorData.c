/* ============================================================================
 * APACScaleFactorData
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 1224 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009a140   1224  APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&) */

/* ------------------------------------------------------------------------ */
/* 0x0009a140  size 1224 bytes  304 insns
 *   APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo const&)
 *   [_ZN19APACScaleFactorData11DeserializeER16TBitstreamReaderIjERK11APACICSInfo]
 *   data refs: 0x310280, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACScaleFactorData::Deserialize(
        APACScaleFactorData *this,
        TBitstreamReader_u32 *bs,
        const APACICSInfo *ics)
{
  __int64 v3; // d8
  __int64 v4; // d9
  _UNKNOWN **SCFHuffmanTable; // x28
  int bitsLeft; // w0
  unsigned int cache; // w1
  int v11; // w21
  int v12; // w0
  int v13; // w26
  int v14; // w0
  int v15; // w20
  unsigned int v16; // w1
  int v17; // w0
  int v18; // w22
  int v19; // w4
  int v20; // w1
  unsigned int v21; // w1
  unsigned int v22; // w23
  const unsigned __int8 *cur; // x1
  unsigned int v24; // w1
  int v25; // w5
  unsigned int v26; // w1
  __int64 result; // x0
  int v28; // w2
  unsigned int v29; // w22
  int v30; // w23
  unsigned int v31; // w1
  int v32; // w0
  int v33; // w0
  __int64 v34; // x1
  bool v35; // cc
  int v36; // w23
  unsigned int v37; // w0
  _BYTE *v38; // x1
  int v39; // w2
  char v40; // w0
  __int16 v41; // w0
  unsigned int v42; // w1
  int v43; // w3
  unsigned int v44; // w4
  unsigned int v45; // w1
  unsigned int v46; // w0
  int v47; // w1
  int v48; // w23
  unsigned int v49; // w0
  unsigned int v50; // w20
  int v51; // w2
  unsigned int v52; // w0
  int v53; // w1
  unsigned int v54; // w0
  __int64 v55; // [xsp+60h] [xbp+60h]
  __int64 v56; // [xsp+68h] [xbp+68h]
  unsigned int v57; // [xsp+70h] [xbp+70h]
  int v58; // [xsp+74h] [xbp+74h]
  _DWORD *v59; // [xsp+78h] [xbp+78h]
  int v60; // [xsp+80h] [xbp+80h]
  int v61; // [xsp+84h] [xbp+84h]
  unsigned int v62; // [xsp+88h] [xbp+88h]
  unsigned int v63; // [xsp+8Ch] [xbp+8Ch]
  _BYTE *v64; // [xsp+90h] [xbp+90h]
  char *v65; // [xsp+98h] [xbp+98h]
  __int64 v66; // [xsp+A8h] [xbp+A8h]
  __int64 v67; // [xsp+B0h] [xbp+B0h]
  SCFHuffmanTable = apac::GetSCFHuffmanTable();
  cache = bs->cache;
  bitsLeft = bs->bitsLeft;
  if ( *((_BYTE *)ics + 10) == 2 )
    v11 = 3;
  else
    v11 = 5;
  v62 = 0x80000000 >> *((_DWORD *)SCFHuffmanTable + 2);
  v12 = bitsLeft - 8;
  bs->bitsLeft = v12;
  v13 = HIBYTE(cache);
  if ( v12 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v52 = bs->cache;
    v53 = bs->bitsLeft;
    v13 |= v52 >> (v53 + 32);
    v54 = v52 << -(char)v53;
    if ( v53 == -32 )
      v54 = 0;
    bs->cur += 4;
    bs->cache = v54;
    bs->bitsLeft = v53 + 32;
  }
  else
  {
    bs->cache = cache << 8;
  }
  if ( *((_BYTE *)ics + 12) )
  {
    v55 = v3;
    v56 = v4;
    v60 = 0;
    v64 = (char *)this + 480;
    v65 = (char *)this + 480;
    while ( 1 )
    {
      v14 = *((unsigned __int8 *)ics + 11);
      v15 = 0;
      v57 = 0;
      v61 = 0;
      if ( !v14 )
        goto LABEL_36;
      do
      {
        v16 = bs->cache;
        v17 = bs->bitsLeft - 4;
        bs->bitsLeft = v17;
        LODWORD(v59) = v16 >> 28;
        if ( v17 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v42 = bs->cache;
          v43 = bs->bitsLeft;
          v17 = v43 + 32;
          v44 = (unsigned int)v59 | (v42 >> (v43 + 32));
          v45 = v42 << -(char)v43;
          if ( v43 == -32 )
            v45 = 0;
          bs->cur += 4;
          bs->cache = v45;
          bs->bitsLeft = v17;
          LODWORD(v59) = v44;
        }
        else
        {
          bs->cache = 16 * v16;
        }
        if ( (unsigned int)v59 > 0xB )
          goto LABEL_19;
        v18 = 0;
        v19 = 32 - v11;
        do
        {
          v21 = bs->cache;
          v17 -= v11;
          bs->bitsLeft = v17;
          v22 = v21 >> v19;
          if ( v17 < 0 )
          {
            cur = bs->cur;
            v58 = v19;
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, cur);
            v24 = bs->cache;
            v25 = -bs->bitsLeft;
            v17 = bs->bitsLeft + 32;
            bs->cur += 4;
            v22 |= v24 >> v17;
            v26 = v24 << v25;
            if ( !v17 )
              v26 = 0;
            bs->cache = v26;
            bs->bitsLeft = v17;
            v18 += v22;
            v20 = v18 + v15;
            v19 = v58;
            if ( v18 + v15 > (unsigned int)*((unsigned __int8 *)ics + 11) )
              goto LABEL_19;
          }
          else
          {
            bs->cache = v21 << v11;
            v18 += v22;
            v20 = v18 + v15;
            if ( v18 + v15 > (unsigned int)*((unsigned __int8 *)ics + 11) )
              goto LABEL_19;
          }
        }
        while ( v22 == (1 << v11) - 1 );
        if ( !v18 )
        {
LABEL_19:
          v3 = v55;
          v4 = v56;
          result = 1;
          goto LABEL_20;
        }
        v28 = (int)v59;
        v57 += v18;
        *((_WORD *)this + (unsigned int)(v60 + v61) + 244) = (unsigned __int16)v59 | ((_WORD)v18 << 8);
        if ( v28 )
        {
          v29 = 0;
          v30 = 0;
          v59 = &unk_310280;
          v58 = v13 - 100;
          while ( 1 )
          {
            if ( *((_DWORD *)SCFHuffmanTable + 2) > v29 )
            {
              if ( v29 <= 0x1F )
              {
                v31 = bs->cache;
                v32 = bs->bitsLeft - 32 + v29;
                bs->bitsLeft = v32;
                v63 = v31 >> v29;
                if ( v32 < 0 )
                {
                  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
                  v46 = bs->cache;
                  v47 = bs->bitsLeft;
                  v29 = 32;
                  v48 = (v46 >> (v47 + 32)) | v30;
                  v49 = v46 << -(char)v47;
                  if ( v47 == -32 )
                    v49 = 0;
                  v30 = v48 | v63;
                  bs->cur += 4;
                  bs->cache = v49;
                  bs->bitsLeft = v47 + 32;
                  goto LABEL_30;
                }
                v30 |= v31 >> v29;
                bs->cache = v31 << -(char)v29;
              }
              v29 = 32;
            }
LABEL_30:
            if ( v30 < 0 )
            {
              v40 = __clz(v62 | ~v30);
              v41 = *(_WORD *)(*((_QWORD *)*SCFHuffmanTable + 2 * (v40 & 0x3F) + 1)
                             + 2LL * ((unsigned int)(v30 << v40) >> *((_WORD *)*SCFHuffmanTable + 8 * (v40 & 0x3F))));
              v29 -= v41 & 0x3F;
              v13 += (__int64)v41 >> 6;
              v30 <<= v41 & 0x3F;
              if ( (v13 ^ (v13 >> 31)) <= 255 )
              {
                v58 = v13 - 100;
                v33 = v59[v13 & 3] + ((v13 - 100) >> 2 << 23);
              }
              else if ( v13 <= 0 )
              {
                v33 = 318767104;
                v13 = -256;
                v58 = -356;
              }
              else
              {
                v33 = 1389839613;
                v13 = 255;
                v58 = 155;
              }
            }
            else
            {
              v30 *= 2;
              --v29;
              v33 = v59[v58 & 3] + (v58 >> 2 << 23);
            }
            v34 = (unsigned int)(v15 + v60);
            v35 = v57 > ++v15;
            *((_DWORD *)this + v34) = v33;
            if ( !v35 )
            {
              v36 = v61 + 1;
              TBitstreamReader<unsigned int>::PutBack(bs, v29);
              goto LABEL_34;
            }
          }
        }
        v50 = v15 + v60;
        v51 = v18 + v50;
        do
          *((_DWORD *)this + v50++) = 0;
        while ( v51 != v50 );
        v15 = v20;
        v36 = v61 + 1;
LABEL_34:
        v37 = *((unsigned __int8 *)ics + 11);
        v61 = v36;
      }
      while ( v37 > v57 );
      LOBYTE(v14) = v36;
LABEL_36:
      v38 = v64;
      v39 = (int)v65;
      *v64 = v14;
      v64 = v38 + 1;
      v60 += 15;
      if ( *((unsigned __int8 *)ics + 12) <= (unsigned int)((_DWORD)v38 + 1 - v39) )
      {
        v3 = v55;
        v4 = v56;
        break;
      }
    }
  }
  result = 0;
LABEL_20:
  if ( v66 != _stack_chk_guard )
  {
    v55 = v3;
    v56 = v4;
  }
  return result;
}


