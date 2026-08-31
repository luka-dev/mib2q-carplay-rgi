/* ============================================================================
 * SC_Extension
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 1184 bytes of .text.
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
/* 0x000a3ca0     80  SC_Extension::operator==(SC_Extension const&) const */
/* 0x000a5840   1104  SC_Extension::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x000a3ca0  size 80 bytes  20 insns
 *   SC_Extension::operator==(SC_Extension const&) const
 *   [_ZNK12SC_ExtensioneqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_Extension::operator==(SC_Extension *this, const SC_Extension *a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return *(unsigned __int8 *)this == *(unsigned __int8 *)a;
}


/* ------------------------------------------------------------------------ */
/* 0x000a5840  size 1104 bytes  274 insns
 *   SC_Extension::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN12SC_Extension11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x30e8b8, 0x3306b0, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_Extension::Deserialize(SC_Extension *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x25
  __int64 v3; // x26
  __int64 v4; // x27
  __int64 v5; // x28
  unsigned int cache; // w0
  int v9; // w1
  unsigned int v10; // w19
  __int64 v11; // x0
  __int64 v12; // x1
  __int64 v13; // x0
  int v14; // w23
  unsigned int v15; // w1
  int v16; // w0
  unsigned int v17; // w19
  _BYTE *v18; // x1
  unsigned int v19; // w1
  int v20; // w0
  unsigned int v21; // w21
  int v22; // w2
  unsigned int v23; // w1
  int v24; // w0
  unsigned int v25; // w19
  unsigned int v26; // w19
  char *v27; // x0
  int v28; // w1
  unsigned int v29; // w0
  int v30; // w21
  int v31; // w1
  char v32; // w2
  __int64 result; // x0
  unsigned int v34; // w0
  int v35; // w1
  unsigned int v36; // w0
  unsigned int v37; // w1
  int v38; // w2
  bool v39; // zf
  int v40; // w3
  unsigned __int8 v41; // w2
  _BYTE *v42; // x1
  size_t v43; // x2
  unsigned __int64 v44; // x0
  unsigned __int64 v45; // x5
  char *v46; // x0
  _BYTE *v47; // x5
  char *v48; // x3
  size_t v49; // x2
  size_t v50; // x4
  bool v51; // cc
  size_t v52; // x5
  size_t v53; // x4
  unsigned int v54; // w2
  int v55; // w3
  const unsigned __int8 *cur; // x0
  __int16 v57; // w19
  unsigned int v58; // w0
  int bitsLeft; // w1
  unsigned int v60; // w0
  __int64 v61; // x5
  __int64 v62; // [xsp+40h] [xbp+40h]
  __int64 v63; // [xsp+48h] [xbp+48h]
  __int64 v64; // [xsp+50h] [xbp+50h]
  __int64 v65; // [xsp+58h] [xbp+58h]
  _BYTE *v66; // [xsp+60h] [xbp+60h]
  _BYTE *v67; // [xsp+68h] [xbp+68h]
  size_t n; // [xsp+70h] [xbp+70h]
  size_t v69; // [xsp+78h] [xbp+78h]
  char v70; // [xsp+87h] [xbp+87h]
  __int64 v71; // [xsp+88h] [xbp+88h]
  __int64 v72; // [xsp+90h] [xbp+90h]
  cache = bs->cache;
  v9 = bs->bitsLeft - 1;
  bs->bitsLeft = v9;
  v10 = cache >> 31;
  if ( v9 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v58 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v10) = v10 | (v58 >> (bitsLeft + 32));
    v60 = v58 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v60 = 0;
    bs->cur += 4;
    bs->cache = v60;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  v11 = *((_QWORD *)this + 1);
  v12 = *((_QWORD *)this + 2);
  *(_BYTE *)this = (unsigned __int8)v10 != 0;
  if ( v11 != v12 )
    *((_QWORD *)this + 2) = v11;
  v13 = *((_QWORD *)this + 4);
  if ( v13 != *((_QWORD *)this + 5) )
    *((_QWORD *)this + 5) = v13;
  if ( !(_BYTE)v10 )
  {
    result = 0;
    goto LABEL_42;
  }
  v14 = 8;
  v70 = 0;
  do
  {
    v15 = bs->cache;
    v16 = bs->bitsLeft - 3;
    bs->bitsLeft = v16;
    v17 = v15 >> 29;
    if ( v16 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v34 = bs->cache;
      v35 = bs->bitsLeft;
      LOBYTE(v17) = v17 | (v34 >> (v35 + 32));
      v36 = v34 << -(char)v35;
      if ( v35 == -32 )
        v36 = 0;
      bs->cur += 4;
      bs->cache = v36;
      bs->bitsLeft = v35 + 32;
    }
    else
    {
      bs->cache = 8 * v15;
    }
    v70 = v17;
    if ( !(_BYTE)v17 )
      goto LABEL_32;
    v18 = *((_BYTE **)this + 2);
    if ( v18 == *((_BYTE **)this + 3) )
    {
      std::vector<unsigned char>::_M_realloc_insert<unsigned char const&>((char *)this + 8);
    }
    else
    {
      *v18 = v17;
      ++*((_QWORD *)this + 2);
    }
    v19 = bs->cache;
    v20 = bs->bitsLeft - 4;
    bs->bitsLeft = v20;
    v21 = v19 >> 28;
    if ( (v20 & 0x80000000) == 0 )
    {
      v22 = v21 + 4;
      v23 = 16 * v19;
      bs->cache = v23;
      goto LABEL_16;
    }
    v26 = 1;
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v37 = bs->cache;
    v38 = bs->bitsLeft;
    v20 = v38 + 32;
    v39 = v38 == -32;
    v40 = -v38;
    v41 = (v37 >> (v38 + 32)) | v21;
    v23 = v37 << v40;
    if ( v39 )
      v23 = 0;
    bs->cur += 4;
    bs->cache = v23;
    bs->bitsLeft = v20;
    if ( (unsigned int)v41 + 3 <= 0x1F )
    {
      v22 = v41 + 4;
LABEL_16:
      v24 = v20 - v22;
      bs->bitsLeft = v24;
      v25 = v23 >> (32 - v22);
      if ( v24 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v54 = bs->cache;
        v55 = bs->bitsLeft;
        cur = bs->cur;
        bs->bitsLeft = v55 + 32;
        bs->cur = cur + 4;
        v57 = (v54 >> (v55 + 32)) | v25;
        if ( v55 == -32 )
        {
          bs->cache = 0;
          v26 = (unsigned __int16)(v57 + 1);
        }
        else
        {
          v26 = (unsigned __int16)(v57 + 1);
          bs->cache = v54 << -(char)v55;
        }
      }
      else
      {
        v26 = (unsigned __int16)(v25 + 1);
        bs->cache = v23 << v22;
      }
    }
    v27 = *((char **)this + 5);
    if ( v27 != *((char **)this + 6) )
    {
      *(_WORD *)v27 = v26;
      *((_QWORD *)this + 5) += 2LL;
      goto LABEL_20;
    }
    v42 = *((_BYTE **)this + 4);
    v43 = v27 - v42;
    v44 = (v27 - v42) >> 1;
    if ( v44 == 0x3FFFFFFFFFFFFFFFLL )
      std::__throw_length_error("vector::_M_realloc_insert");
    if ( v44 )
    {
      v45 = 0x7FFFFFFFFFFFFFFELL;
      if ( v44 <= v43 )
      {
        if ( !v43 )
        {
          v48 = nullptr;
          v53 = 2;
          v52 = 0;
          LOWORD(dword_0) = v26;
LABEL_56:
          if ( !v42 )
            goto LABEL_51;
          goto LABEL_50;
        }
        if ( v43 > 0x3FFFFFFFFFFFFFFFLL )
          v61 = 0x3FFFFFFFFFFFFFFFLL;
        else
          v61 = v43;
        v45 = 2 * v61;
      }
    }
    else
    {
      v45 = 2;
    }
    v66 = (_BYTE *)v45;
    v67 = v42;
    n = v43;
    v46 = (char *)operator new(v45);
    v47 = v66;
    v42 = v67;
    v48 = v46;
    v49 = n;
    v50 = n + 2;
    v51 = (__int64)n <= 0;
    *(_WORD *)&v46[n] = v26;
    v52 = (size_t)&v47[(_QWORD)v46];
    v53 = (size_t)&v46[v50];
    if ( v51 )
      goto LABEL_56;
    v66 = v42;
    v67 = v46;
    n = v53;
    v69 = v52;
    memmove(v46, v42, v49);
    v42 = v66;
    v48 = v67;
    v53 = n;
    v52 = v69;
LABEL_50:
    v66 = v48;
    v67 = (_BYTE *)v53;
    n = v52;
    operator delete(v42);
    v48 = v66;
    v53 = (size_t)v67;
    v52 = n;
LABEL_51:
    *((_QWORD *)this + 4) = v48;
    *((_QWORD *)this + 5) = v53;
    *((_QWORD *)this + 6) = v52;
LABEL_20:
    if ( v26 )
    {
      v28 = bs->bitsLeft;
      do
      {
        while ( 1 )
        {
          v29 = bs->cache;
          v30 = v26 > 0x20 ? 32 : v26;
          v28 -= v30;
          bs->bitsLeft = v28;
          if ( v28 < 0 )
            break;
          bs->cache = v29 << v30;
LABEL_23:
          v26 -= v30;
          if ( !v26 )
            goto LABEL_30;
        }
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v31 = bs->bitsLeft;
        bs->cur += 4;
        v32 = v31;
        v28 = v31 + 32;
        bs->bitsLeft = v28;
        if ( v28 )
        {
          bs->cache <<= -v32;
          goto LABEL_23;
        }
        bs->cache = 0;
        v26 -= v30;
      }
      while ( v26 );
    }
LABEL_30:
    --v14;
  }
  while ( v14 );
  if ( v70 )
  {
    __printf_chk(1, "ERROR: error in extension payload\n");
    result = 0xFFFFFFFFLL;
  }
  else
  {
LABEL_32:
    result = 0;
  }
LABEL_42:
  if ( v71 != _stack_chk_guard )
  {
    v62 = v2;
    v63 = v3;
    v64 = v4;
    v65 = v5;
  }
  return result;
}


