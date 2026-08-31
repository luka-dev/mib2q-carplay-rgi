/* ============================================================================
 * SC_Tag
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 1176 bytes of .text.
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
/* 0x000a41d0    260  SC_Tag::operator==(SC_Tag const&) const */
/* 0x000a5c90    916  SC_Tag::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x000a41d0  size 260 bytes  65 insns
 *   SC_Tag::operator==(SC_Tag const&) const
 *   [_ZNK6SC_TageqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_Tag::operator==(SC_Tag *this, const SC_Tag *a)
{
  const void *v4; // x0
  const void *v5; // x1
  size_t v6; // x2
  bool result; // w0
  const void *v8; // x0
  const void *v9; // x1
  size_t v10; // x2
  __int64 v11; // [xsp+40h] [xbp+40h]
  v4 = *(const void **)this;
  v5 = *(const void **)a;
  v6 = *((_QWORD *)this + 1) - (_QWORD)v4;
  result = v6 == *((_QWORD *)a + 1) - (_QWORD)v5
        && (!v6 || !memcmp(v4, v5, v6))
        && *((unsigned __int8 *)a + 24) == *((unsigned __int8 *)this + 24)
        && (!*((_BYTE *)a + 24)
         || (v8 = *((const void **)this + 4),
             v9 = *((const void **)a + 4),
             v10 = *((_QWORD *)this + 5) - (_QWORD)v8,
             v10 == *((_QWORD *)a + 5) - (_QWORD)v9)
         && (!v10 || !memcmp(v8, v9, v10)))
        && SC_Extension::operator==((SC_Tag *)((char *)this + 56), (const SC_Tag *)((char *)a + 56));
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a5c90  size 916 bytes  229 insns
 *   SC_Tag::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN6SC_Tag11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_Tag::Deserialize(SC_Tag *this, TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w0
  int bitsLeft; // w2
  unsigned int v6; // w20
  __int64 v7; // x4
  unsigned __int8 v8; // w20
  unsigned __int64 v9; // x0
  __int64 v10; // x22
  __int64 v11; // x20
  __int64 v12; // x22
  unsigned int v13; // w1
  int v14; // w2
  unsigned int v15; // w24
  unsigned int v16; // w0
  int v17; // w1
  __int16 v18; // w24
  unsigned int v19; // w0
  unsigned int v20; // w0
  int v21; // w2
  unsigned int v22; // w20
  unsigned int v23; // w1
  int v24; // w0
  unsigned int v25; // w20
  __int64 v26; // x2
  __int64 v27; // x3
  __int64 v28; // x1
  int v29; // w20
  unsigned __int64 v30; // x0
  bool v31; // cf
  __int64 v32; // x22
  __int64 v33; // x20
  __int64 v34; // x22
  unsigned int v35; // w1
  int v36; // w0
  unsigned int v37; // w24
  unsigned int v38; // w0
  int v39; // w1
  __int16 v40; // w24
  unsigned int v41; // w0
  __int64 result; // x0
  __int64 v43; // x0
  __int64 v44; // x3
  unsigned int v45; // w0
  int v46; // w1
  unsigned int v47; // w0
  unsigned int v48; // w0
  int v49; // w3
  unsigned int v50; // w0
  __int64 v51; // x2
  unsigned int v52; // w0
  int v53; // w1
  char v54; // w20
  unsigned int v55; // w0
  __int64 v56; // [xsp+50h] [xbp+50h]
  cache = bs->cache;
  bitsLeft = bs->bitsLeft - 2;
  bs->bitsLeft = bitsLeft;
  v6 = cache >> 30;
  if ( bitsLeft < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v48 = bs->cache;
    v49 = -bs->bitsLeft;
    bitsLeft = bs->bitsLeft + 32;
    bs->cur += 4;
    LOBYTE(v6) = v6 | (v48 >> bitsLeft);
    v50 = v48 << v49;
    if ( !bitsLeft )
      v50 = 0;
    bs->cache = v50;
    bs->bitsLeft = bitsLeft;
  }
  else
  {
    bs->cache = 4 * cache;
  }
  v7 = *((_QWORD *)this + 1);
  v8 = v6 + 1;
  v9 = (v7 - *(_QWORD *)this) >> 1;
  if ( v8 > v9 )
  {
    std::vector<unsigned short>::_M_default_append(this, v8 - v9);
    bitsLeft = bs->bitsLeft;
  }
  else if ( v8 < v9 )
  {
    v44 = *(_QWORD *)this + 2LL * v8;
    if ( v7 != v44 )
    {
      *((_QWORD *)this + 1) = v44;
      bitsLeft = bs->bitsLeft;
    }
  }
  if ( v8 )
  {
    v10 = (unsigned int)v8 - 1;
    v11 = 0;
    v12 = 2 * (v10 + 1);
    do
    {
      while ( 1 )
      {
        v13 = bs->cache;
        v14 = bitsLeft - 10;
        bs->bitsLeft = v14;
        v15 = v13 >> 22;
        if ( v14 < 0 )
          break;
        bs->cache = v13 << 10;
        *(_WORD *)(*(_QWORD *)this + v11) = v15;
        v11 += 2;
        bitsLeft = bs->bitsLeft;
        if ( v12 == v11 )
          goto LABEL_12;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v16 = bs->cache;
      v17 = bs->bitsLeft;
      v18 = v15 | (v16 >> (v17 + 32));
      v19 = v16 << -(char)v17;
      if ( v17 == -32 )
        v19 = 0;
      bs->cur += 4;
      bs->cache = v19;
      bs->bitsLeft = v17 + 32;
      *(_WORD *)(*(_QWORD *)this + v11) = v18;
      v11 += 2;
      bitsLeft = bs->bitsLeft;
    }
    while ( v12 != v11 );
  }
LABEL_12:
  v20 = bs->cache;
  v21 = bitsLeft - 1;
  bs->bitsLeft = v21;
  v22 = v20 >> 31;
  if ( v21 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v45 = bs->cache;
    v46 = bs->bitsLeft;
    LOBYTE(v22) = v22 | (v45 >> (v46 + 32));
    v47 = v45 << -(char)v46;
    if ( v46 == -32 )
      v47 = 0;
    bs->cur += 4;
    bs->cache = v47;
    bs->bitsLeft = v46 + 32;
  }
  else
  {
    bs->cache = 2 * v20;
  }
  *((_BYTE *)this + 24) = (unsigned __int8)v22 != 0;
  if ( !(_BYTE)v22 )
  {
    v43 = *((_QWORD *)this + 4);
    if ( v43 != *((_QWORD *)this + 5) )
      *((_QWORD *)this + 5) = v43;
    goto LABEL_26;
  }
  v23 = bs->cache;
  v24 = bs->bitsLeft - 2;
  bs->bitsLeft = v24;
  v25 = v23 >> 30;
  if ( v24 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v52 = bs->cache;
    v53 = bs->bitsLeft;
    v54 = v25 | (v52 >> (v53 + 32));
    v55 = v52 << -(char)v53;
    if ( v53 == -32 )
      v55 = 0;
    bs->cur += 4;
    bs->cache = v55;
    bs->bitsLeft = v53 + 32;
    LOBYTE(v29) = v54 + 1;
    v28 = (unsigned __int8)v29;
    v29 = (unsigned __int8)v29;
    v26 = *((_QWORD *)this + 4);
    v27 = *((_QWORD *)this + 5);
    v30 = (v27 - v26) >> 1;
    v31 = (unsigned __int8)v29 >= v30;
    if ( (unsigned __int8)v29 <= v30 )
      goto LABEL_17;
LABEL_45:
    std::vector<unsigned short>::_M_default_append((char *)this + 32, v28 - v30);
    goto LABEL_19;
  }
  bs->cache = 4 * v23;
  LOBYTE(v29) = v25 + 1;
  v26 = *((_QWORD *)this + 4);
  v27 = *((_QWORD *)this + 5);
  v28 = (unsigned __int8)v29;
  v29 = (unsigned __int8)v29;
  v30 = (v27 - v26) >> 1;
  v31 = (unsigned __int8)v29 >= v30;
  if ( (unsigned __int8)v29 > v30 )
    goto LABEL_45;
LABEL_17:
  if ( !v31 )
  {
    v51 = v26 + 2LL * (unsigned __int8)v29;
    if ( v27 != v51 )
      *((_QWORD *)this + 5) = v51;
  }
LABEL_19:
  if ( v29 )
  {
    v32 = (unsigned int)(v29 - 1);
    v33 = 0;
    v34 = 2 * (v32 + 1);
    do
    {
      while ( 1 )
      {
        v35 = bs->cache;
        v36 = bs->bitsLeft - 10;
        bs->bitsLeft = v36;
        v37 = v35 >> 22;
        if ( v36 < 0 )
          break;
        bs->cache = v35 << 10;
        *(_WORD *)(*((_QWORD *)this + 4) + v33) = v37;
        v33 += 2;
        if ( v33 == v34 )
          goto LABEL_26;
      }
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v38 = bs->cache;
      v39 = bs->bitsLeft;
      v40 = v37 | (v38 >> (v39 + 32));
      v41 = v38 << -(char)v39;
      if ( v39 == -32 )
        v41 = 0;
      bs->cur += 4;
      bs->cache = v41;
      bs->bitsLeft = v39 + 32;
      *(_WORD *)(*((_QWORD *)this + 4) + v33) = v40;
      v33 += 2;
    }
    while ( v33 != v34 );
  }
LABEL_26:
  result = SC_Extension::Deserialize((SC_Tag *)((char *)this + 56), bs);
  if ( !(_DWORD)result )
    *((_WORD *)this + 29) = 3;
  return result;
}


