/* ============================================================================
 * MP4LoudnessInfo
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 1948 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b7cd0   1948  MP4LoudnessInfo::Deserialize(TBitstreamReader<unsigned int>&, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000b7cd0  size 1948 bytes  487 insns
 *   MP4LoudnessInfo::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN15MP4LoudnessInfo11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x80280, 0x42ce00, 0x42de78, 0x43e520
 */
__int64 __fastcall MP4LoudnessInfo::Deserialize(MP4LoudnessInfo *this, TBitstreamReader_u32 *bs, bool flag)
{
  __int64 v3; // x27
  unsigned int cache; // w1
  int bitsLeft; // w0
  int v8; // w0
  unsigned int v9; // w21
  int v10; // w0
  unsigned __int64 v11; // x21
  unsigned int v12; // w1
  int v13; // w0
  unsigned __int64 v14; // x24
  void (__fastcall ***v15)(MP4LoudnessMeasure *__hidden); // x25
  void (__fastcall ***v16)(MP4LoudnessMeasure *__hidden); // x26
  void (__fastcall ***v17)(MP4LoudnessMeasure *__hidden); // x21
  void (__fastcall *v18)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v19)(MP4LoudnessMeasure *__hidden); // x0
  void (__fastcall ***v20)(MP4LoudnessMeasure *__hidden); // x0
  unsigned __int64 v21; // x1
  void (__fastcall ***v22)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v23)(MP4LoudnessMeasure *__hidden); // x21
  void (__fastcall *v24)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v25)(MP4LoudnessMeasure *__hidden); // x0
  int v26; // w0
  unsigned int v27; // w21
  int v28; // w0
  unsigned int v29; // w1
  int v30; // w0
  unsigned int v31; // w21
  unsigned int v32; // w1
  int v33; // w0
  unsigned int v34; // w21
  unsigned int v35; // w1
  int v36; // w0
  unsigned int v37; // w21
  unsigned int v38; // w1
  int v39; // w0
  unsigned int v40; // w21
  unsigned int v41; // w1
  int v42; // w0
  unsigned int v43; // w21
  unsigned int v44; // w2
  const unsigned __int8 *v45; // x1
  char v46; // w3
  const unsigned __int8 *v47; // x1
  void (__fastcall ***v48)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v49)(MP4LoudnessMeasure *__hidden); // x25
  void (__fastcall ***v50)(MP4LoudnessMeasure *__hidden); // x21
  void (__fastcall *v51)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v52)(MP4LoudnessMeasure *__hidden); // x0
  unsigned int v53; // w1
  int v54; // w0
  unsigned __int64 v55; // x21
  void (__fastcall ***v56)(MP4LoudnessMeasure *__hidden); // x0
  void (__fastcall ***v57)(MP4LoudnessMeasure *__hidden); // x25
  unsigned __int64 v58; // x2
  void (__fastcall ***v59)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v60)(MP4LoudnessMeasure *__hidden); // x21
  void (__fastcall *v61)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v62)(MP4LoudnessMeasure *__hidden); // x0
  int v63; // w21
  __int64 v64; // x2
  __int64 result; // x0
  unsigned int v66; // w2
  const unsigned __int8 *cur; // x1
  char v68; // w3
  const unsigned __int8 *v69; // x1
  unsigned int v70; // w1
  int v71; // w0
  unsigned __int64 v72; // x21
  unsigned int v73; // w0
  int v74; // w1
  unsigned int v75; // w0
  int v76; // w21
  __int64 v77; // x2
  unsigned int v78; // w0
  int v79; // w1
  unsigned int v80; // w0
  unsigned int v81; // w0
  int v82; // w1
  char v83; // w21
  unsigned int v84; // w0
  unsigned int v85; // w0
  int v86; // w1
  unsigned int v87; // w0
  unsigned int v88; // w0
  int v89; // w1
  unsigned int v90; // w0
  unsigned int v91; // w0
  int v92; // w1
  unsigned int v93; // w0
  unsigned int v94; // w0
  int v95; // w1
  unsigned int v96; // w0
  unsigned int v97; // w0
  int v98; // w1
  unsigned int v99; // w0
  unsigned int v100; // w0
  int v101; // w1
  __int16 v102; // w21
  unsigned int v103; // w0
  __int64 v104; // [xsp+50h] [xbp+50h]
  __int64 v105; // [xsp+68h] [xbp+68h]
  __int64 v106; // [xsp+70h] [xbp+70h]
  cache = bs->cache;
  bitsLeft = bs->bitsLeft;
  if ( !flag )
  {
    v26 = bitsLeft - 6;
    bs->bitsLeft = v26;
    v27 = cache >> 26;
    if ( v26 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v85 = bs->cache;
      v86 = bs->bitsLeft;
      LOBYTE(v27) = v27 | (v85 >> (v86 + 32));
      v87 = v85 << -(char)v86;
      if ( v86 == -32 )
        v87 = 0;
      bs->cur += 4;
      bs->cache = v87;
      bs->bitsLeft = v86 + 32;
    }
    else
    {
      bs->cache = cache << 6;
    }
    v28 = *(unsigned __int8 *)this;
    *((_BYTE *)this + 1) = v27;
    if ( v28 )
    {
      v29 = bs->cache;
      v30 = bs->bitsLeft - 6;
      bs->bitsLeft = v30;
      v31 = v29 >> 26;
      if ( v30 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v81 = bs->cache;
        v82 = bs->bitsLeft;
        v83 = v31 | (v81 >> (v82 + 32));
        v84 = v81 << -(char)v82;
        if ( v82 == -32 )
          v84 = 0;
        bs->cur += 4;
        bs->cache = v84;
        bs->bitsLeft = v82 + 32;
        *((_BYTE *)this + 2) = v83;
      }
      else
      {
        bs->cache = v29 << 6;
        *((_BYTE *)this + 2) = v31;
      }
    }
    v32 = bs->cache;
    v33 = bs->bitsLeft - 7;
    bs->bitsLeft = v33;
    v34 = v32 >> 25;
    if ( v33 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v94 = bs->cache;
      v95 = bs->bitsLeft;
      LOBYTE(v34) = v34 | (v94 >> (v95 + 32));
      v96 = v94 << -(char)v95;
      if ( v95 == -32 )
        v96 = 0;
      bs->cur += 4;
      bs->cache = v96;
      bs->bitsLeft = v95 + 32;
    }
    else
    {
      bs->cache = v32 << 7;
    }
    *((_BYTE *)this + 3) = v34;
    v35 = bs->cache;
    v36 = bs->bitsLeft - 1;
    bs->bitsLeft = v36;
    v37 = v35 >> 31;
    if ( v36 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v66 = bs->cache;
      cur = bs->cur;
      v68 = bs->bitsLeft;
      v36 = bs->bitsLeft + 32;
      bs->bitsLeft = v36;
      v69 = cur + 4;
      bs->cur = v69;
      LOBYTE(v37) = v37 | (v66 >> v36);
      if ( !v36 )
      {
        bs->cache = 0;
        if ( (_BYTE)v37 )
        {
          LOWORD(v40) = 0;
          bs->bitsLeft = -12;
          goto LABEL_109;
        }
LABEL_64:
        *((_WORD *)this + 2) = 0;
        v41 = bs->cache;
        v42 = bs->bitsLeft - 1;
        bs->bitsLeft = v42;
        v43 = v41 >> 31;
        if ( v42 < 0 )
          goto LABEL_36;
        goto LABEL_65;
      }
      bs->cache = v66 << -v68;
    }
    else
    {
      bs->cache = 2 * v35;
    }
    if ( (_BYTE)v37 )
    {
      v38 = bs->cache;
      v39 = v36 - 12;
      bs->bitsLeft = v39;
      v40 = v38 >> 20;
      if ( (v39 & 0x80000000) == 0 )
      {
        bs->cache = v38 << 12;
        *((_WORD *)this + 2) = v40;
        goto LABEL_35;
      }
      v69 = bs->cur;
LABEL_109:
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, v69);
      v100 = bs->cache;
      v101 = bs->bitsLeft;
      v102 = v40 | (v100 >> (v101 + 32));
      v103 = v100 << -(char)v101;
      if ( v101 == -32 )
        v103 = 0;
      bs->cur += 4;
      bs->cache = v103;
      bs->bitsLeft = v101 + 32;
      *((_WORD *)this + 2) = v102;
LABEL_35:
      v41 = bs->cache;
      v42 = bs->bitsLeft - 1;
      bs->bitsLeft = v42;
      v43 = v41 >> 31;
      if ( v42 < 0 )
      {
LABEL_36:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v44 = bs->cache;
        v45 = bs->cur;
        v46 = bs->bitsLeft;
        v42 = bs->bitsLeft + 32;
        bs->bitsLeft = v42;
        v47 = v45 + 4;
        bs->cur = v47;
        LOBYTE(v43) = v43 | (v44 >> v42);
        if ( !v42 )
        {
          bs->cache = 0;
          if ( !(_BYTE)v43 )
          {
LABEL_38:
            *((_WORD *)this + 3) = 0;
            goto LABEL_39;
          }
          LODWORD(v72) = 0;
          bs->bitsLeft = -18;
          goto LABEL_105;
        }
        bs->cache = v44 << -v46;
LABEL_66:
        if ( !(_BYTE)v43 )
          goto LABEL_38;
        v70 = bs->cache;
        v71 = v42 - 18;
        bs->bitsLeft = v71;
        v72 = v70 >> 14;
        if ( (v71 & 0x80000000) == 0 )
        {
          bs->cache = v70 << 18;
LABEL_69:
          *((_WORD *)this + 3) = (v72 >> 6) & 0xFFF;
          *((_DWORD *)this + 2) = (v72 >> 2) & 0xF;
          *((_DWORD *)this + 3) = v72 & 3;
LABEL_39:
          v49 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
          v48 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 3);
          if ( v49 != v48 )
          {
            v50 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
            do
            {
              while ( 1 )
              {
                v51 = **v50;
                if ( v51 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
                  break;
                v50 += 2;
                if ( v48 == v50 )
                  goto LABEL_44;
              }
              v52 = v50;
              v50 += 2;
              v51((MP4LoudnessMeasure *)v52);
            }
            while ( v48 != v50 );
LABEL_44:
            *((_QWORD *)this + 3) = v49;
          }
          v53 = bs->cache;
          v54 = bs->bitsLeft - 4;
          bs->bitsLeft = v54;
          v55 = v53 >> 28;
          if ( v54 < 0 )
          {
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
            v91 = bs->cache;
            v92 = bs->bitsLeft;
            v55 = (unsigned int)v55 | (v91 >> (v92 + 32));
            v93 = v91 << -(char)v92;
            if ( v92 == -32 )
              v93 = 0;
            bs->cur += 4;
            bs->cache = v93;
            bs->bitsLeft = v92 + 32;
          }
          else
          {
            bs->cache = 16 * v53;
          }
          v56 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
          v57 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 3);
          v58 = ((char *)v57 - (char *)v56) >> 4;
          if ( v55 > v58 )
          {
            std::vector<MP4LoudnessMeasure>::_M_default_append((char *)this + 16, v55 - v58);
            v56 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
            v57 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 3);
          }
          else if ( v55 < v58 )
          {
            v59 = &v56[2 * v55];
            if ( v57 == v59 )
              goto LABEL_56;
            v60 = &v56[2 * v55];
            do
            {
              while ( 1 )
              {
                v61 = **v60;
                if ( v61 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
                  break;
                v60 += 2;
                if ( v57 == v60 )
                  goto LABEL_54;
              }
              v62 = v60;
              v60 += 2;
              v61((MP4LoudnessMeasure *)v62);
            }
            while ( v57 != v60 );
LABEL_54:
            v56 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
            v57 = v59;
            *((_QWORD *)this + 3) = v59;
          }
          if ( v57 == v56 )
            goto LABEL_72;
LABEL_56:
          v63 = 0;
          v64 = 0;
          while ( !(unsigned int)MP4LoudnessMeasure::Deserialize((MP4LoudnessMeasure *)&v56[2 * v64], bs, 0) )
          {
            v56 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
            v64 = (unsigned int)(v63 + 1);
            v63 = v64;
            if ( (__int64)(*((_QWORD *)this + 3) - (_QWORD)v56) >> 4 <= (unsigned __int64)(unsigned int)v64 )
              goto LABEL_72;
          }
          goto LABEL_59;
        }
        v47 = bs->cur;
LABEL_105:
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, v47);
        v97 = bs->cache;
        v98 = bs->bitsLeft;
        v72 = (unsigned int)v72 | (v97 >> (v98 + 32));
        v99 = v97 << -(char)v98;
        if ( v98 == -32 )
          v99 = 0;
        bs->cur += 4;
        bs->cache = v99;
        bs->bitsLeft = v98 + 32;
        goto LABEL_69;
      }
LABEL_65:
      bs->cache = 2 * v41;
      goto LABEL_66;
    }
    goto LABEL_64;
  }
  if ( *(_BYTE *)this )
  {
    v8 = bitsLeft - 8;
    bs->bitsLeft = v8;
    v9 = HIBYTE(cache);
    if ( v8 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v78 = bs->cache;
      v79 = bs->bitsLeft;
      LOBYTE(v9) = v9 | (v78 >> (v79 + 32));
      v80 = v78 << -(char)v79;
      if ( v79 == -32 )
        v80 = 0;
      bs->cur += 4;
      bs->cache = v80;
      bs->bitsLeft = v79 + 32;
    }
    else
    {
      bs->cache = cache << 8;
    }
    *((_BYTE *)this + 2) = v9 & 0x3F;
    cache = bs->cache;
    bitsLeft = bs->bitsLeft;
  }
  v10 = bitsLeft - 28;
  bs->bitsLeft = v10;
  v11 = cache >> 4;
  if ( v10 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v73 = bs->cache;
    v74 = bs->bitsLeft;
    v11 = (unsigned int)v11 | (v73 >> (v74 + 32));
    v75 = v73 << -(char)v74;
    if ( v74 == -32 )
      v75 = 0;
    bs->cur += 4;
    bs->cache = v75;
    bs->bitsLeft = v74 + 32;
  }
  else
  {
    bs->cache = cache << 28;
  }
  *((_BYTE *)this + 1) = (v11 >> 12) & 0x3F;
  *((_BYTE *)this + 3) = (v11 >> 18) & 0x7F;
  *((_WORD *)this + 2) = v11 & 0xFFF;
  v12 = bs->cache;
  v13 = bs->bitsLeft - 28;
  bs->bitsLeft = v13;
  v14 = v12 >> 4;
  if ( v13 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v88 = bs->cache;
    v89 = bs->bitsLeft;
    v14 = (unsigned int)v14 | (v88 >> (v89 + 32));
    v90 = v88 << -(char)v89;
    if ( v89 == -32 )
      v90 = 0;
    bs->cur += 4;
    bs->cache = v90;
    bs->bitsLeft = v89 + 32;
  }
  else
  {
    bs->cache = v12 << 28;
  }
  v15 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
  v16 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 3);
  *((_WORD *)this + 3) = WORD1(v14) & 0xFFF;
  *((_DWORD *)this + 2) = (unsigned __int16)v14 >> 12;
  *((_DWORD *)this + 3) = (v14 >> 8) & 0xF;
  if ( v15 == v16 )
  {
    v14 = (unsigned __int8)v14;
    if ( !(_BYTE)v14 )
      goto LABEL_72;
    v21 = 0;
  }
  else
  {
    v17 = v15;
    do
    {
      while ( 1 )
      {
        v18 = **v17;
        if ( v18 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
          break;
        v17 += 2;
        if ( v16 == v17 )
          goto LABEL_15;
      }
      v19 = v17;
      v17 += 2;
      v18((MP4LoudnessMeasure *)v19);
    }
    while ( v16 != v17 );
LABEL_15:
    v20 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
    *((_QWORD *)this + 3) = v15;
    v14 = (unsigned __int8)v14;
    v21 = ((char *)v15 - (char *)v20) >> 4;
    if ( (unsigned __int8)v14 <= v21 )
    {
      if ( (unsigned __int8)v14 < v21 )
      {
        v22 = &v20[2 * (unsigned __int8)v14];
        if ( v15 == v22 )
        {
LABEL_77:
          v76 = 0;
          v77 = 0;
          while ( !(unsigned int)MP4LoudnessMeasure::Deserialize((MP4LoudnessMeasure *)&v20[2 * v77], bs, 1) )
          {
            v20 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
            v77 = (unsigned int)(v76 + 1);
            v76 = v77;
            if ( (__int64)(*((_QWORD *)this + 3) - (_QWORD)v20) >> 4 <= (unsigned __int64)(unsigned int)v77 )
              goto LABEL_72;
          }
LABEL_59:
          result = 4294967246LL;
          goto LABEL_60;
        }
        v23 = v22;
        do
        {
          while ( 1 )
          {
            v24 = **v23;
            if ( v24 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
              break;
            v23 += 2;
            if ( v15 == v23 )
              goto LABEL_22;
          }
          v25 = v23;
          v23 += 2;
          v24((MP4LoudnessMeasure *)v25);
        }
        while ( v15 != v23 );
LABEL_22:
        *((_QWORD *)this + 3) = v22;
        v15 = v22;
        v20 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
      }
      goto LABEL_76;
    }
  }
  std::vector<MP4LoudnessMeasure>::_M_default_append((char *)this + 16, v14 - v21);
  v20 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 2);
  v15 = *((void (__fastcall ****)(MP4LoudnessMeasure *__hidden))this + 3);
LABEL_76:
  if ( v20 != v15 )
    goto LABEL_77;
LABEL_72:
  result = 0;
LABEL_60:
  if ( v105 != _stack_chk_guard )
    v104 = v3;
  return result;
}


