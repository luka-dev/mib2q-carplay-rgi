/* ============================================================================
 * apac::chan
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 2500 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0007c030     68  apac::chan::CoreDecoderConfig::GetChannelLayout() const */
/* 0x0007c080    120  apac::chan::CoreDecoderConfig::GetNumChannels() const */
/* 0x0007c100   1788  apac::chan::CoreDecoderConfig::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x0007c8d0    268  apac::chan::CoreDecoderConfig::~CoreDecoderConfig() */
/* 0x0007c9e0    256  apac::chan::CoreDecoderConfig::~CoreDecoderConfig() */

/* ------------------------------------------------------------------------ */
/* 0x0007c030  size 68 bytes  17 insns
 *   apac::chan::CoreDecoderConfig::GetChannelLayout() const
 *   [_ZNK4apac4chan17CoreDecoderConfig16GetChannelLayoutEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::chan::CoreDecoderConfig::GetChannelLayout(apac__chan__CoreDecoderConfig *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return *((_QWORD *)this + 9);
}


/* ------------------------------------------------------------------------ */
/* 0x0007c080  size 120 bytes  30 insns
 *   apac::chan::CoreDecoderConfig::GetNumChannels() const
 *   [_ZNK4apac4chan17CoreDecoderConfig14GetNumChannelsEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::chan::CoreDecoderConfig::GetNumChannels(apac__chan__CoreDecoderConfig *this)
{
  int8x8_t v1; // d0
  _DWORD *v2; // x2
  __int64 result; // x0
  __int64 v4; // [xsp+20h] [xbp+20h]
  v2 = *((_DWORD **)this + 9);
  if ( !*v2 )
    return (unsigned int)v2[2];
  result = (unsigned __int16)*v2;
  if ( *v2 == 0x10000 )
  {
    v1.n64_u32[0] = v2[1];
    return (unsigned __int8)vaddv_s8(vcnt_s8(v1));
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0007c100  size 1788 bytes  447 insns
 *   apac::chan::CoreDecoderConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac4chan17CoreDecoderConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x7c850, 0x30ee00, 0x42a920, 0x42ce00, 0x42d1a8, 0x42d480, 0x43e520
 */
__int64 __fastcall apac::chan::CoreDecoderConfig::Deserialize(
        apac__chan__CoreDecoderConfig *this,
        TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w0
  int v5; // w2
  int v6; // w28
  void (__fastcall ***v7)(apac__TCEConfig *); // x25
  void (__fastcall ***v8)(apac__TCEConfig *); // x26
  void (__fastcall ***v9)(apac__TCEConfig *); // x21
  void (__fastcall *v10)(apac__TCEConfig *); // x1
  void (__fastcall ***v11)(apac__TCEConfig *); // x0
  int v12; // w25
  unsigned int v13; // w27
  unsigned int v14; // w1
  int v15; // w2
  unsigned int v16; // w22
  _DWORD *v17; // x0
  _DWORD *v18; // x1
  unsigned int v19; // w0
  int v20; // w2
  unsigned int v21; // w21
  unsigned int v22; // w1
  int v23; // w0
  unsigned int v24; // w21
  unsigned int v25; // w0
  float v26; // s0
  __int64 v27; // x2
  float v28; // s0
  unsigned int v29; // w0
  __int64 v30; // x2
  __int64 v31; // x3
  unsigned int i; // w22
  int v33; // w1
  unsigned int v34; // w21
  int v35; // w0
  unsigned int v36; // w3
  int v37; // w2
  unsigned int v38; // w21
  unsigned int v39; // w2
  int v40; // w3
  const unsigned __int8 *v41; // x1
  unsigned int v42; // w0
  int v43; // w1
  unsigned int v44; // w0
  unsigned int v45; // w0
  int v46; // w2
  unsigned int v47; // w21
  int v48; // w0
  unsigned int v49; // w0
  int v50; // w1
  char v51; // w21
  unsigned int v52; // w0
  char *v54; // x1
  __int64 v55; // x3
  unsigned __int64 v56; // x0
  __int64 v57; // x21
  __int64 v58; // x27
  __int64 v59; // x0
  int v60; // w1
  __int64 v61; // x0
  __int64 v62; // x3
  int v63; // w0
  unsigned int v64; // w1
  unsigned int v65; // w22
  unsigned int v66; // w0
  int v67; // w1
  unsigned int v68; // w0
  unsigned int v69; // w0
  unsigned int v70; // w28
  unsigned int v71; // w0
  int v72; // w3
  unsigned int v73; // w0
  __int64 v74; // x0
  char *v75; // x0
  unsigned int v76; // w0
  unsigned int v77; // w28
  unsigned int v78; // w0
  int v79; // w1
  unsigned int v80; // w0
  unsigned int v81; // w0
  const unsigned __int8 *cur; // x1
  char bitsLeft; // w3
  const unsigned __int8 *v84; // x1
  unsigned int v85; // w0
  const unsigned __int8 *v86; // x1
  char v87; // w3
  const unsigned __int8 *v88; // x1
  unsigned int v89; // w0
  int v90; // w3
  unsigned int v91; // w4
  unsigned int v92; // w0
  void (__fastcall **v93)(apac::TCEConfig *__hidden); // [xsp+60h] [xbp+60h] BYREF
  unsigned int v94; // [xsp+68h] [xbp+68h]
  __int64 v95; // [xsp+70h] [xbp+70h]
  __int64 v96; // [xsp+78h] [xbp+78h]
  int v97; // [xsp+80h] [xbp+80h]
  __int64 v98; // [xsp+90h] [xbp+90h]
  cache = bs->cache;
  v5 = bs->bitsLeft - 5;
  bs->bitsLeft = v5;
  v6 = cache >> 27;
  if ( v5 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v81 = bs->cache;
    cur = bs->cur;
    bitsLeft = bs->bitsLeft;
    v5 = bs->bitsLeft + 32;
    bs->bitsLeft = v5;
    v84 = cur + 4;
    bs->cur = v84;
    v6 |= v81 >> v5;
    if ( !v5 )
    {
      bs->cache = 0;
      if ( v6 != 31 )
        goto LABEL_4;
      v70 = 0;
      bs->bitsLeft = -10;
      goto LABEL_86;
    }
    bs->cache = v81 << -bitsLeft;
  }
  else
  {
    bs->cache = 32 * cache;
  }
  if ( v6 != 31 )
    goto LABEL_4;
  v69 = bs->cache;
  v5 -= 10;
  bs->bitsLeft = v5;
  v70 = v69 >> 22;
  if ( (v5 & 0x80000000) == 0 )
  {
    bs->cache = v69 << 10;
LABEL_63:
    if ( v70 != 1023 )
    {
LABEL_64:
      v6 = v70 + 31;
      goto LABEL_4;
    }
    v76 = bs->cache;
    v5 -= 16;
    bs->bitsLeft = v5;
    v77 = HIWORD(v76);
    if ( (v5 & 0x80000000) == 0 )
    {
      v6 = v77 + 1054;
      bs->cache = v76 << 16;
      goto LABEL_4;
    }
    v88 = bs->cur;
    goto LABEL_89;
  }
  v84 = bs->cur;
LABEL_86:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v84);
  v85 = bs->cache;
  v86 = bs->cur;
  v87 = bs->bitsLeft;
  v5 = bs->bitsLeft + 32;
  bs->bitsLeft = v5;
  v88 = v86 + 4;
  bs->cur = v88;
  v70 |= v85 >> v5;
  if ( v5 )
  {
    bs->cache = v85 << -v87;
    goto LABEL_63;
  }
  bs->cache = 0;
  if ( v70 != 1023 )
    goto LABEL_64;
  v77 = 0;
  bs->bitsLeft = -16;
LABEL_89:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, v88);
  v89 = bs->cache;
  v90 = bs->bitsLeft;
  v5 = v90 + 32;
  bs->cur += 4;
  v91 = v89 >> (v90 + 32);
  v92 = v89 << -(char)v90;
  v6 = (v77 | v91) + 1054;
  if ( v90 == -32 )
    v92 = 0;
  bs->cache = v92;
  bs->bitsLeft = v5;
LABEL_4:
  v8 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  v7 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  if ( v8 != v7 )
  {
    v9 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
    do
    {
      while ( 1 )
      {
        v10 = **v9;
        if ( v10 != apac::TCEConfig::~TCEConfig )
          break;
        v9 += 2;
        if ( v7 == v9 )
          goto LABEL_9;
      }
      v11 = v9;
      v9 += 2;
      v10((apac__TCEConfig *)v11);
    }
    while ( v7 != v9 );
LABEL_9:
    *((_QWORD *)this + 3) = v8;
    v5 = bs->bitsLeft;
  }
  if ( v6 )
  {
    v12 = 0;
    v13 = 0;
    while ( 1 )
    {
      v14 = bs->cache;
      v15 = v5 - 3;
      bs->bitsLeft = v15;
      v16 = v14 >> 29;
      if ( v15 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v93 = off_42A930;
        v42 = bs->cache;
        v43 = bs->bitsLeft;
        v16 |= v42 >> (v43 + 32);
        v44 = v42 << -(char)v43;
        if ( v43 == -32 )
          v44 = 0;
        bs->cur += 4;
        bs->cache = v44;
        bs->bitsLeft = v43 + 32;
        v94 = v16;
        v18 = *((_DWORD **)this + 3);
        if ( v18 == *((_DWORD **)this + 4) )
        {
LABEL_42:
          std::vector<apac::TCEConfig>::_M_realloc_insert<apac::TCEConfig const&>(
            (std_vector *)((char *)this + 16),
            v18,
            (const apac__TCEConfig *)&v93);
          goto LABEL_18;
        }
      }
      else
      {
        bs->cache = 8 * v14;
        v93 = off_42A930;
        v18 = *((_DWORD **)this + 3);
        v17 = *((_DWORD **)this + 4);
        v94 = v16;
        if ( v18 == v17 )
          goto LABEL_42;
      }
      *(_QWORD *)v18 = off_42A930;
      v18[2] = v16;
      *((_QWORD *)this + 3) += 16LL;
LABEL_18:
      if ( v16 == 1 )
      {
        ++v12;
        v5 = bs->bitsLeft;
        v13 += 2;
        if ( v12 == v6 )
          goto LABEL_20;
      }
      else
      {
        if ( v16 == 3 || !v16 )
          ++v13;
        ++v12;
        v5 = bs->bitsLeft;
        if ( v12 == v6 )
          goto LABEL_20;
      }
    }
  }
  v13 = 0;
LABEL_20:
  v19 = bs->cache;
  v20 = v5 - 16;
  bs->bitsLeft = v20;
  v21 = HIWORD(v19);
  if ( v20 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v71 = bs->cache;
    v72 = -bs->bitsLeft;
    v20 = bs->bitsLeft + 32;
    bs->cur += 4;
    v21 |= v71 >> v20;
    v73 = v71 << v72;
    if ( !v20 )
      v73 = 0;
    bs->cache = v73;
    bs->bitsLeft = v20;
  }
  else
  {
    bs->cache = v19 << 16;
  }
  if ( !v21 )
  {
    v55 = *((_QWORD *)this + 9);
    v54 = *((char **)this + 10);
    LOBYTE(v93) = 0;
    v56 = 20LL * v13 + 12;
    if ( v56 > (unsigned __int64)&v54[-v55] )
    {
      std::vector<char>::_M_fill_insert((_DWORD)this + 72, v54, v56 - (_QWORD)&v54[-v55]);
      v55 = *((_QWORD *)this + 9);
    }
    else if ( v56 < (unsigned __int64)&v54[-v55] )
    {
      v75 = (char *)(v55 + v56);
      if ( v54 != v75 )
        *((_QWORD *)this + 10) = v75;
    }
    *(_DWORD *)(v55 + 8) = v13;
    if ( v13 )
    {
      v57 = 12;
      v58 = 20LL * (v13 - 1) + 32;
      do
      {
        v64 = bs->cache;
        v63 = bs->bitsLeft;
        v95 = 0;
        v96 = 0;
        v63 -= 7;
        bs->bitsLeft = v63;
        v97 = 0;
        v65 = v64 >> 25;
        if ( v63 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v66 = bs->cache;
          v67 = bs->bitsLeft;
          v65 |= v66 >> (v67 + 32);
          v68 = v66 << -(char)v67;
          if ( v67 == -32 )
            v68 = 0;
          bs->cur += 4;
          bs->cache = v68;
          bs->bitsLeft = v67 + 32;
        }
        else
        {
          bs->cache = v64 << 7;
        }
        v59 = *((_QWORD *)this + 9);
        LODWORD(v95) = v65;
        v60 = v97;
        v61 = v59 + v57;
        v57 += 20;
        v62 = v96;
        *(_QWORD *)v61 = v95;
        *(_QWORD *)(v61 + 8) = v62;
        *(_DWORD *)(v61 + 16) = v60;
      }
      while ( v57 != v58 );
    }
LABEL_25:
    v22 = bs->cache;
    v23 = bs->bitsLeft - 1;
    bs->bitsLeft = v23;
    v24 = v22 >> 31;
    if ( (v23 & 0x80000000) == 0 )
      goto LABEL_26;
    goto LABEL_46;
  }
  if ( v21 != 1 )
  {
    **((_DWORD **)this + 9) = v13 | (v21 << 16);
    goto LABEL_25;
  }
  v45 = bs->cache;
  v46 = v20 - 27;
  bs->bitsLeft = v46;
  v47 = v45 >> 5;
  if ( v46 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v78 = bs->cache;
    v79 = bs->bitsLeft;
    v47 |= v78 >> (v79 + 32);
    v80 = v78 << -(char)v79;
    if ( v79 == -32 )
      v80 = 0;
    bs->cur += 4;
    bs->cache = v80;
    bs->bitsLeft = v79 + 32;
  }
  else
  {
    bs->cache = v45 << 27;
  }
  *(_DWORD *)(*((_QWORD *)this + 9) + 4LL) = v47;
  v22 = bs->cache;
  v48 = bs->bitsLeft - 1;
  bs->bitsLeft = v48;
  v24 = v22 >> 31;
  if ( (v48 & 0x80000000) == 0 )
  {
LABEL_26:
    bs->cache = 2 * v22;
    if ( !(_BYTE)v24 )
      return 0;
    goto LABEL_27;
  }
LABEL_46:
  TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
  v49 = bs->cache;
  v50 = bs->bitsLeft;
  v51 = v24 | (v49 >> (v50 + 32));
  v52 = v49 << -(char)v50;
  if ( v50 == -32 )
    v52 = 0;
  bs->cur += 4;
  bs->cache = v52;
  bs->bitsLeft = v50 + 32;
  if ( !v51 )
    return 0;
LABEL_27:
  v25 = (*(__int64 (__fastcall **)(apac__chan__CoreDecoderConfig *))(*(_QWORD *)this + 24LL))(this);
  v26 = log2f((float)v25);
  v27 = *(_QWORD *)this;
  v28 = v26 - 0.0001;
  *((_DWORD *)this + 11) = vcvtps_u32_f32(v28);
  v29 = (*(__int64 (__fastcall **)(apac__chan__CoreDecoderConfig *))(v27 + 24))(this);
  v30 = *((_QWORD *)this + 6);
  v31 = *((_QWORD *)this + 7);
  if ( v29 > (unsigned __int64)(v31 - v30) )
  {
    std::vector<unsigned char>::_M_default_append((_DWORD)this + 48, v29 - (v31 - v30));
  }
  else if ( v29 < (unsigned __int64)(v31 - v30) )
  {
    v74 = v30 + v29;
    if ( v31 != v74 )
      *((_QWORD *)this + 7) = v74;
  }
  for ( i = 0; i < (*(unsigned int (__fastcall **)(apac__chan__CoreDecoderConfig *))(*(_QWORD *)this + 24LL))(this); ++i )
  {
    v35 = *((_DWORD *)this + 11);
    LOBYTE(v33) = 0;
    if ( (unsigned int)(v35 - 1) <= 0x1F )
    {
      v36 = bs->cache;
      v37 = bs->bitsLeft - v35;
      bs->bitsLeft = v37;
      v38 = v36 >> (32 - v35);
      if ( v37 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v39 = bs->cache;
        v40 = bs->bitsLeft;
        v41 = bs->cur;
        bs->bitsLeft = v40 + 32;
        bs->cur = v41 + 4;
        v33 = (v39 >> (v40 + 32)) | v38;
        if ( v40 == -32 )
          bs->cache = 0;
        else
          bs->cache = v39 << -(char)v40;
      }
      else
      {
        LOBYTE(v33) = v36 >> (32 - v35);
        bs->cache = v36 << v35;
      }
    }
    *(_BYTE *)(*((_QWORD *)this + 6) + i) = v33;
    v34 = *(unsigned __int8 *)(*((_QWORD *)this + 6) + i);
    if ( v34 >= (*(unsigned int (__fastcall **)(apac__chan__CoreDecoderConfig *))(*(_QWORD *)this + 24LL))(this) )
      return 1;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007c8d0  size 268 bytes  67 insns
 *   apac::chan::CoreDecoderConfig::~CoreDecoderConfig()
 *   apac::chan::CoreDecoderConfig::~CoreDecoderConfig()
 *   [_ZN4apac4chan17CoreDecoderConfigD1Ev]
 *   [_ZN4apac4chan17CoreDecoderConfigD2Ev]
 *   data refs: 0x7c850, 0x42a948, 0x42a980, 0x42ce00, 0x42d268, 0x42d3f0, 0x42d480, 0x43e520
 */
// Alternative name is '_ZN4apac4chan17CoreDecoderConfigD2Ev'
void __fastcall apac::chan::CoreDecoderConfig::~CoreDecoderConfig(apac__chan__CoreDecoderConfig *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void (__fastcall ***v5)(apac__TCEConfig *); // x22
  void (__fastcall ***v6)(apac__TCEConfig *); // x20
  void (__fastcall *v7)(apac__TCEConfig *); // x1
  void (__fastcall ***v8)(apac__TCEConfig *); // x0
  __int64 v9; // x0
  __int64 v10; // [xsp+30h] [xbp+30h]
  __int64 v11; // [xsp+48h] [xbp+48h]
  __int64 v12; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 9);
  *(_QWORD *)this = off_42A990;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 6);
  if ( v4 )
    operator delete(v4);
  *(_QWORD *)this = &unk_42A958;
  v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  v6 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  if ( v6 != v5 )
  {
    do
    {
      while ( 1 )
      {
        v7 = **v6;
        if ( v7 != apac::TCEConfig::~TCEConfig )
          break;
        v6 += 2;
        if ( v5 == v6 )
          goto LABEL_10;
      }
      v8 = v6;
      v6 += 2;
      v7((apac__TCEConfig *)v8);
    }
    while ( v5 != v6 );
LABEL_10:
    v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  }
  v9 = v11 ^ _stack_chk_guard;
  if ( v5 )
  {
    if ( !v9 )
    {
      operator delete(v5);
      return;
    }
LABEL_15:
    v10 = v1;
  }
  if ( v9 )
    goto LABEL_15;
}


/* ------------------------------------------------------------------------ */
/* 0x0007c9e0  size 256 bytes  64 insns
 *   apac::chan::CoreDecoderConfig::~CoreDecoderConfig()
 *   [_ZN4apac4chan17CoreDecoderConfigD0Ev]
 *   data refs: 0x7c850, 0x42a948, 0x42a980, 0x42ce00, 0x42d268, 0x42d3f0, 0x42d480, 0x43e520
 */
void __fastcall apac::chan::CoreDecoderConfig::~CoreDecoderConfig(apac__chan__CoreDecoderConfig *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void (__fastcall ***v5)(apac__TCEConfig *); // x22
  void (__fastcall ***v6)(apac__TCEConfig *); // x20
  void (__fastcall *v7)(apac__TCEConfig *); // x1
  void (__fastcall ***v8)(apac__TCEConfig *); // x0
  __int64 v9; // [xsp+30h] [xbp+30h]
  __int64 v10; // [xsp+48h] [xbp+48h]
  __int64 v11; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 9);
  *(_QWORD *)this = off_42A990;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 6);
  if ( v4 )
    operator delete(v4);
  *(_QWORD *)this = &unk_42A958;
  v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  v6 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  if ( v6 != v5 )
  {
    do
    {
      while ( 1 )
      {
        v7 = **v6;
        if ( v7 != apac::TCEConfig::~TCEConfig )
          break;
        v6 += 2;
        if ( v5 == v6 )
          goto LABEL_10;
      }
      v8 = v6;
      v6 += 2;
      v7((apac__TCEConfig *)v8);
    }
    while ( v5 != v6 );
LABEL_10:
    v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  }
  if ( v5 )
    operator delete(v5);
  if ( v10 != _stack_chk_guard )
    v9 = v1;
  operator delete(this);
}


