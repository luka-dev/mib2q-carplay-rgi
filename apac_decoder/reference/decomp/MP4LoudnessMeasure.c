/* ============================================================================
 * MP4LoudnessMeasure
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 788 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b77b0    788  MP4LoudnessMeasure::Deserialize(TBitstreamReader<unsigned int>&, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000b77b0  size 788 bytes  197 insns
 *   MP4LoudnessMeasure::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN18MP4LoudnessMeasure11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall MP4LoudnessMeasure::Deserialize(MP4LoudnessMeasure *this, TBitstreamReader_u32 *bs, bool flag)
{
  unsigned int cache; // w1
  int bitsLeft; // w0
  int v7; // w0
  unsigned int v8; // w22
  int v10; // w0
  unsigned int v11; // w22
  int v12; // w0
  unsigned int v13; // w1
  int v14; // w0
  unsigned int v15; // w22
  unsigned int v16; // w1
  int v17; // w0
  unsigned int v18; // w22
  unsigned int v19; // w1
  int v20; // w0
  unsigned int v21; // w22
  unsigned int v22; // w0
  int v23; // w1
  unsigned int v24; // w0
  unsigned int v25; // w0
  int v26; // w1
  unsigned int v27; // w0
  unsigned int v28; // w1
  int v29; // w0
  int v30; // w0
  unsigned int v31; // w0
  int v32; // w1
  int v33; // w22
  unsigned int v34; // w0
  int v35; // w0
  unsigned int v36; // w0
  int v37; // w1
  unsigned int v38; // w0
  unsigned int v39; // w0
  int v40; // w1
  unsigned int v41; // w0
  int v42; // w0
  int v43; // w0
  unsigned int v44; // w1
  bool v45; // zf
  int v46; // w2
  unsigned int v47; // w0
  __int64 v48; // [xsp+40h] [xbp+40h]
  cache = bs->cache;
  bitsLeft = bs->bitsLeft;
  if ( !flag )
  {
    v10 = bitsLeft - 4;
    bs->bitsLeft = v10;
    v11 = cache >> 28;
    if ( v10 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v39 = bs->cache;
      v40 = bs->bitsLeft;
      v11 |= v39 >> (v40 + 32);
      v41 = v39 << -(char)v40;
      if ( v40 == -32 )
        v41 = 0;
      bs->cur += 4;
      bs->cache = v41;
      bs->bitsLeft = v40 + 32;
    }
    else
    {
      bs->cache = 16 * cache;
    }
    *((_DWORD *)this + 2) = v11 << 16;
    if ( v11 == 7 )
    {
      v28 = bs->cache;
      v29 = bs->bitsLeft - 5;
      bs->bitsLeft = v29;
      v15 = v28 >> 27;
      if ( (v29 & 0x80000000) == 0 )
      {
        bs->cache = 32 * v28;
        *((_DWORD *)this + 2) |= v15 << 8;
        v16 = bs->cache;
        v30 = bs->bitsLeft - 4;
        bs->bitsLeft = v30;
        v18 = v16 >> 28;
        if ( (v30 & 0x80000000) == 0 )
        {
LABEL_13:
          bs->cache = 16 * v16;
          *((_DWORD *)this + 2) |= 16 * v18;
          v19 = bs->cache;
          v20 = bs->bitsLeft - 2;
          bs->bitsLeft = v20;
          v21 = v19 >> 30;
          if ( (v20 & 0x80000000) == 0 )
          {
LABEL_14:
            bs->cache = 4 * v19;
LABEL_15:
            *((_DWORD *)this + 2) |= v21;
            return 0;
          }
LABEL_27:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v36 = bs->cache;
          v37 = bs->bitsLeft;
          v21 |= v36 >> (v37 + 32);
          v38 = v36 << -(char)v37;
          if ( v37 == -32 )
            v38 = 0;
          bs->cur += 4;
          bs->cache = v38;
          bs->bitsLeft = v37 + 32;
          goto LABEL_15;
        }
LABEL_24:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v31 = bs->cache;
        v32 = bs->bitsLeft;
        v33 = v18 | (v31 >> (v32 + 32));
        v34 = v31 << -(char)v32;
        if ( v32 == -32 )
          v34 = 0;
        bs->cur += 4;
        bs->cache = v34;
        bs->bitsLeft = v32 + 32;
        *((_DWORD *)this + 2) |= 16 * v33;
        v19 = bs->cache;
        v35 = bs->bitsLeft - 2;
        bs->bitsLeft = v35;
        v21 = v19 >> 30;
        if ( (v35 & 0x80000000) == 0 )
          goto LABEL_14;
        goto LABEL_27;
      }
    }
    else
    {
      v13 = bs->cache;
      v12 = bs->bitsLeft;
      if ( v11 != 8 )
      {
        v14 = v12 - 8;
        bs->bitsLeft = v14;
        v15 = HIBYTE(v13);
        if ( v14 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v25 = bs->cache;
          v26 = bs->bitsLeft;
          v15 |= v25 >> (v26 + 32);
          v27 = v25 << -(char)v26;
          if ( v26 == -32 )
            v27 = 0;
          bs->cur += 4;
          bs->cache = v27;
          bs->bitsLeft = v26 + 32;
        }
        else
        {
          bs->cache = v13 << 8;
        }
        goto LABEL_12;
      }
      v42 = v12 - 2;
      bs->bitsLeft = v42;
      v15 = v13 >> 30;
      if ( (v42 & 0x80000000) == 0 )
      {
        bs->cache = 4 * v13;
        goto LABEL_12;
      }
    }
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v44 = bs->cache;
    v43 = bs->bitsLeft;
    v46 = v43 + 32;
    v45 = v43 == -32;
    bs->cur += 4;
    v47 = v44 << -(char)v43;
    if ( v45 )
      v47 = 0;
    v15 |= v44 >> v46;
    bs->cache = v47;
    bs->bitsLeft = v46;
LABEL_12:
    *((_DWORD *)this + 2) |= v15 << 8;
    v16 = bs->cache;
    v17 = bs->bitsLeft - 4;
    bs->bitsLeft = v17;
    v18 = v16 >> 28;
    if ( (v17 & 0x80000000) == 0 )
      goto LABEL_13;
    goto LABEL_24;
  }
  v7 = bitsLeft - 24;
  bs->bitsLeft = v7;
  v8 = cache >> 8;
  if ( v7 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v22 = bs->cache;
    v23 = bs->bitsLeft;
    v8 |= v22 >> (v23 + 32);
    v24 = v22 << -(char)v23;
    if ( v23 == -32 )
      v24 = 0;
    bs->cur += 4;
    bs->cache = v24;
    bs->bitsLeft = v23 + 32;
  }
  else
  {
    bs->cache = cache << 24;
  }
  *((_DWORD *)this + 2) = v8;
  return 0;
}


