/* ============================================================================
 * apac::APACAncillaryFrameDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 4656 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00074120   1172  apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder() */
/* 0x000745c0     84  apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder() */
/* 0x00074a80    644  apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(apac::AncillaryConfig const&) */
/* 0x00074d10    580  apac::APACAncillaryFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int>&, std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> >&, apac::FrameType) */
/* 0x000757e0   2176  apac::APACAncillaryFrameDecoder::Initialize(apac::ParamList const&) */

/* ------------------------------------------------------------------------ */
/* 0x00074120  size 1172 bytes  293 insns
 *   apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()
 *   apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()
 *   [_ZN4apac25APACAncillaryFrameDecoderD1Ev]
 *   [_ZN4apac25APACAncillaryFrameDecoderD2Ev]
 *   data refs: 0x733c0, 0x73400, 0x73590, 0x765c0, 0x76a10, 0x42a330, 0x42a508, 0x42a528, 0x42a548, 0x42cc70, 0x42cd68, 0x42ce00, 0x42cf80, 0x42d208, 0x42d280, 0x42d2e0, 0x42d570, 0x42d618, 0x42d7c0, 0x42de18, 0x43e520, 0x43ec98
 */
// Alternative name is '_ZN4apac25APACAncillaryFrameDecoderD2Ev'
void __fastcall apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder(apac__APACAncillaryFrameDecoder *this)
{
  __int64 v1; // x21
  __int64 v2; // x22
  __int64 v3; // x25
  void *v5; // x0
  void *v6; // x0
  _QWORD *v7; // x20
  void (__fastcall *v8)(apac__APACDRCDecoder *); // x1
  _QWORD *v9; // x21
  void (__fastcall *v10)(apac__drc__DynRangeCompressor *); // x1
  unsigned int *v11; // x22
  unsigned int *v12; // x1
  unsigned int v13; // w0
  __int64 (__fastcall *v14)(); // x1
  unsigned int *v15; // x1
  unsigned int v16; // w0
  __int64 (__fastcall *v17)(); // x2
  _QWORD *v18; // x24
  void (__fastcall *v19)(apac__APACAudioSceneDecoder *); // x1
  _QWORD *v20; // x19
  void *v21; // x0
  void *v22; // x0
  void **v23; // x20
  void **v24; // x21
  void *v25; // x0
  void *v26; // x0
  void *v27; // x0
  void *v28; // x0
  void *v29; // x0
  void *v30; // x0
  void *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  _QWORD *v35; // x20
  _QWORD *v36; // x21
  void *v37; // x0
  void *v38; // x0
  __int64 v39; // x20
  void *v40; // x21
  void *v41; // x0
  void *v42; // x0
  void *v43; // x0
  _QWORD *v44; // x20
  _QWORD *v45; // x25
  _QWORD *v46; // x21
  _QWORD *v47; // x22
  void *v48; // x0
  void *v49; // x0
  void *v50; // x0
  void *v51; // x0
  _QWORD *v52; // x0
  void *v53; // x0
  __int64 v54; // [xsp+20h] [xbp+20h]
  __int64 v55; // [xsp+28h] [xbp+28h]
  __int64 v56; // [xsp+40h] [xbp+40h]
  __int64 v57; // [xsp+58h] [xbp+58h]
  __int64 v58; // [xsp+60h] [xbp+60h]
  v5 = *((void **)this + 7);
  *(_QWORD *)this = off_42A558;
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 4);
  if ( v6 )
    operator delete(v6);
  v7 = *((_QWORD **)this + 3);
  if ( v7 )
  {
    v8 = *(void (__fastcall **)(apac__APACDRCDecoder *))(*v7 + 8LL);
    if ( v8 == apac::APACDRCDecoder::~APACDRCDecoder )
    {
      v54 = v1;
      v55 = v2;
      v9 = (_QWORD *)v7[1];
      *v7 = off_42A538;
      if ( v9 )
      {
        v10 = *(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*v9 + 8LL);
        if ( v10 == apac::drc::DynRangeCompressor::~DynRangeCompressor )
        {
          v11 = (unsigned int *)v9[6];
          *v9 = off_42A340;
          if ( v11 )
          {
            if ( &_pthread_key_create )
            {
              v12 = v11 + 2;
              do
                v13 = __ldaxr(v12);
              while ( __stlxr(v13 - 1, v12) );
            }
            else
            {
              v13 = v11[2];
              v11[2] = v13 - 1;
            }
            if ( v13 == 1 )
            {
              v14 = *(__int64 (__fastcall **)())(*(_QWORD *)v11 + 16LL);
              if ( v14 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
                ((void (__fastcall *)(unsigned int *))v14)(v11);
              if ( &_pthread_key_create )
              {
                v15 = v11 + 3;
                do
                  v16 = __ldaxr(v15);
                while ( __stlxr(v16 - 1, v15) );
              }
              else
              {
                v16 = v11[3];
                v11[3] = v16 - 1;
              }
              if ( v16 == 1 )
              {
                v17 = *(__int64 (__fastcall **)())(*(_QWORD *)v11 + 24LL);
                if ( v17 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
                  (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v11 + 8LL))(v11);
                else
                  ((void (__fastcall *)(unsigned int *))v17)(v11);
              }
            }
          }
          operator delete(v9);
        }
        else
        {
          v10((apac__drc__DynRangeCompressor *)v9);
        }
      }
      operator delete(v7);
      v1 = v54;
      v2 = v55;
    }
    else
    {
      v8(*((apac__APACDRCDecoder **)this + 3));
    }
  }
  v18 = *((_QWORD **)this + 2);
  if ( v18 )
  {
    v19 = *(void (__fastcall **)(apac__APACAudioSceneDecoder *))(*v18 + 8LL);
    if ( v19 == apac::APACAudioSceneDecoder::~APACAudioSceneDecoder )
    {
      v20 = (_QWORD *)v18[1];
      *v18 = off_42A518;
      if ( v20 )
      {
        v54 = v1;
        v55 = v2;
        v56 = v3;
        v21 = (void *)v20[74];
        if ( v21 )
          operator delete(v21);
        v22 = (void *)v20[69];
        if ( v22 )
          operator delete(v22);
        v23 = (void **)v20[65];
        v24 = (void **)v20[66];
        if ( v23 != v24 )
        {
          do
          {
            if ( *v23 )
              operator delete(*v23);
            v23 += 3;
          }
          while ( v24 != v23 );
          v24 = (void **)v20[65];
        }
        if ( v24 )
          operator delete(v24);
        v25 = (void *)v20[62];
        if ( v25 )
          operator delete(v25);
        v26 = (void *)v20[57];
        if ( v26 )
          operator delete(v26);
        v27 = (void *)v20[54];
        if ( v27 )
          operator delete(v27);
        v28 = (void *)v20[51];
        if ( v28 )
          operator delete(v28);
        v29 = (void *)v20[48];
        if ( v29 )
          operator delete(v29);
        v30 = (void *)v20[45];
        if ( v30 )
          operator delete(v30);
        v31 = (void *)v20[42];
        if ( v31 )
          operator delete(v31);
        v32 = (void *)v20[39];
        if ( v32 )
          operator delete(v32);
        v33 = (void *)v20[35];
        if ( v33 )
          operator delete(v33);
        v34 = (void *)v20[32];
        if ( v34 )
          operator delete(v34);
        v35 = (_QWORD *)v20[29];
        v36 = (_QWORD *)v20[30];
        if ( v35 != v36 )
        {
          do
          {
            v37 = (void *)v35[8];
            if ( v37 )
              operator delete(v37);
            v38 = (void *)v35[5];
            if ( v38 )
              operator delete(v38);
            v35 += 11;
          }
          while ( v36 != v35 );
          v36 = (_QWORD *)v20[29];
        }
        if ( v36 )
          operator delete(v36);
        v39 = v20[26];
        v40 = (void *)v20[27];
        if ( (void *)v39 != v40 )
        {
          do
          {
            v41 = *(void **)(v39 + 24);
            if ( v41 )
              operator delete(v41);
            if ( *(_QWORD *)v39 )
              operator delete(*(void **)v39);
            v39 += 64;
          }
          while ( v40 != (void *)v39 );
          v40 = (void *)v20[26];
        }
        if ( v40 )
          operator delete(v40);
        v42 = (void *)v20[22];
        if ( v42 )
          operator delete(v42);
        v43 = (void *)v20[19];
        if ( v43 )
          operator delete(v43);
        v44 = (_QWORD *)v20[15];
        v45 = (_QWORD *)v20[16];
        if ( v44 != v45 )
        {
          do
          {
            v46 = (_QWORD *)v44[8];
            v47 = (_QWORD *)v44[9];
            if ( v46 != v47 )
            {
              do
              {
                v48 = (void *)v46[7];
                if ( v48 )
                  operator delete(v48);
                v49 = (void *)v46[4];
                if ( v49 )
                  operator delete(v49);
                v46 += 10;
              }
              while ( v47 != v46 );
              v47 = (_QWORD *)v44[8];
            }
            if ( v47 )
              operator delete(v47);
            v50 = (void *)v44[4];
            if ( v50 )
              operator delete(v50);
            v51 = (void *)v44[1];
            if ( v51 )
              operator delete(v51);
            v44 += 11;
          }
          while ( v45 != v44 );
          v45 = (_QWORD *)v20[15];
        }
        if ( v45 )
          operator delete(v45);
        v52 = (_QWORD *)v20[9];
        if ( v52 != v20 + 11 )
          operator delete(v52);
        v53 = (void *)v20[2];
        if ( v53 )
          operator delete(v53);
        operator delete(v20);
        v1 = v54;
        v2 = v55;
        v3 = v56;
      }
      if ( v57 == _stack_chk_guard )
      {
        operator delete(v18);
        return;
      }
LABEL_114:
      v54 = v1;
      v55 = v2;
      v56 = v3;
    }
    if ( v57 != _stack_chk_guard )
      goto LABEL_114;
    v19(*((apac__APACAudioSceneDecoder **)this + 2));
  }
  else if ( v57 != _stack_chk_guard )
  {
    goto LABEL_114;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000745c0  size 84 bytes  21 insns
 *   apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder()
 *   [_ZN4apac25APACAncillaryFrameDecoderD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder(apac__APACAncillaryFrameDecoder *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  apac::APACAncillaryFrameDecoder::~APACAncillaryFrameDecoder(this);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00074a80  size 644 bytes  161 insns
 *   apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(apac::AncillaryConfig const&)
 *   apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(apac::AncillaryConfig const&)
 *   [_ZN4apac25APACAncillaryFrameDecoderC1ERKNS_15AncillaryConfigE]
 *   [_ZN4apac25APACAncillaryFrameDecoderC2ERKNS_15AncillaryConfigE]
 *   data refs: 0x733c0, 0x73400, 0x73590, 0x76a10, 0x42a330, 0x42a528, 0x42a548, 0x42cc70, 0x42cd68, 0x42ce00, 0x42d208, 0x42d280, 0x42d2e0, 0x42d570, 0x42d618, 0x42d7c0, 0x43e520, 0x43ec98
 */
// Alternative name is '_ZN4apac25APACAncillaryFrameDecoderC2ERKNS_15AncillaryConfigE'
void __fastcall apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(
        apac__APACAncillaryFrameDecoder *this,
        const apac__AncillaryConfig *a)
{
  __int64 v2; // x23
  _QWORD *v4; // x0
  _QWORD *v5; // x21
  void (__fastcall *v6)(apac__APACDRCDecoder *); // x2
  _QWORD *v7; // x19
  void (__fastcall *v8)(apac__drc__DynRangeCompressor *); // x1
  unsigned int *v9; // x22
  unsigned int *v10; // x1
  unsigned int v11; // w0
  __int64 (__fastcall *v12)(); // x1
  unsigned int *v13; // x1
  unsigned int v14; // w0
  __int64 (__fastcall *v15)(); // x2
  __int64 v16; // [xsp+30h] [xbp+30h]
  __int64 v17; // [xsp+48h] [xbp+48h]
  __int64 v18; // [xsp+50h] [xbp+50h]
  *(_QWORD *)this = off_42A558;
  *((_QWORD *)this + 1) = a;
  *((_QWORD *)this + 2) = 0;
  *((_QWORD *)this + 3) = 0;
  *((_QWORD *)this + 4) = 0;
  *((_QWORD *)this + 5) = 0;
  *((_QWORD *)this + 6) = 0;
  *((_QWORD *)this + 7) = 0;
  v4 = (_QWORD *)operator new(0x10u);
  v5 = *((_QWORD **)this + 3);
  *v4 = off_42A538;
  v4[1] = 0;
  *((_QWORD *)this + 3) = v4;
  if ( v5 )
  {
    v6 = *(void (__fastcall **)(apac__APACDRCDecoder *))(*v5 + 8LL);
    if ( v6 == apac::APACDRCDecoder::~APACDRCDecoder )
    {
      v7 = (_QWORD *)v5[1];
      *v5 = off_42A538;
      if ( v7 )
      {
        v8 = *(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*v7 + 8LL);
        if ( v8 == apac::drc::DynRangeCompressor::~DynRangeCompressor )
        {
          v9 = (unsigned int *)v7[6];
          *v7 = off_42A340;
          if ( v9 )
          {
            if ( &_pthread_key_create )
            {
              v10 = v9 + 2;
              do
                v11 = __ldaxr(v10);
              while ( __stlxr(v11 - 1, v10) );
            }
            else
            {
              v11 = v9[2];
              v9[2] = v11 - 1;
            }
            if ( v11 == 1 )
            {
              v12 = *(__int64 (__fastcall **)())(*(_QWORD *)v9 + 16LL);
              if ( v12 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
                ((void (__fastcall *)(unsigned int *))v12)(v9);
              if ( &_pthread_key_create )
              {
                v13 = v9 + 3;
                do
                  v14 = __ldaxr(v13);
                while ( __stlxr(v14 - 1, v13) );
              }
              else
              {
                v14 = v9[3];
                v9[3] = v14 - 1;
              }
              if ( v14 == 1 )
              {
                v15 = *(__int64 (__fastcall **)())(*(_QWORD *)v9 + 24LL);
                if ( v15 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
                  (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v9 + 8LL))(v9);
                else
                  ((void (__fastcall *)(unsigned int *))v15)(v9);
              }
            }
          }
          operator delete(v7);
        }
        else
        {
          v8((apac__drc__DynRangeCompressor *)v7);
        }
      }
      if ( v17 == _stack_chk_guard )
      {
        operator delete(v5);
        return;
      }
LABEL_29:
      v16 = v2;
    }
    if ( v17 != _stack_chk_guard )
      goto LABEL_29;
    v6((apac__APACDRCDecoder *)v5);
  }
  else if ( v17 != _stack_chk_guard )
  {
    goto LABEL_29;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x00074d10  size 580 bytes  145 insns
 *   apac::APACAncillaryFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int>&, std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> >&, apac::FrameType)
 *   [_ZN4apac25APACAncillaryFrameDecoder11DecodeFrameER16TBitstreamReaderIjERSt6vectorINS_15ASCAudioOutDataESaIS5_EENS_9FrameTypeE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::APACAncillaryFrameDecoder::DecodeFrame(
        apac__APACAncillaryFrameDecoder *this,
        TBitstreamReader_u32 *bs,
        std_vector *vec,
        int n)
{
  __int64 v4; // x25
  _DWORD *v5; // x26
  __int64 v9; // x0
  apac__APACDRCDecoder *v10; // x0
  unsigned int v11; // w22
  float **v12; // x19
  _DWORD *begin; // x0
  _DWORD *end; // x24
  int v15; // w25
  unsigned int i; // w19
  _QWORD *v17; // x1
  int v18; // w0
  unsigned int cache; // w0
  int v21; // w1
  unsigned int v22; // w22
  __int64 v23; // x0
  apac__APACDRCDecoder *v24; // x0
  apac__APACAudioSceneDecoder *v25; // x0
  unsigned int v26; // w0
  int bitsLeft; // w1
  unsigned int v28; // w0
  __int64 v29; // [xsp+40h] [xbp+40h]
  _DWORD *v30; // [xsp+48h] [xbp+48h]
  __int64 v31; // [xsp+58h] [xbp+58h]
  __int64 v32; // [xsp+60h] [xbp+60h]
  v9 = *(_QWORD *)(*((_QWORD *)this + 1) + 16LL);
  if ( (v9 & 4) != 0 )
  {
    cache = bs->cache;
    v21 = bs->bitsLeft - 1;
    bs->bitsLeft = v21;
    v22 = cache >> 31;
    if ( v21 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v26 = bs->cache;
      bitsLeft = bs->bitsLeft;
      LOBYTE(v22) = v22 | (v26 >> (bitsLeft + 32));
      v28 = v26 << -(char)bitsLeft;
      if ( bitsLeft == -32 )
        v28 = 0;
      bs->cur += 4;
      bs->cache = v28;
      bs->bitsLeft = bitsLeft + 32;
    }
    else
    {
      bs->cache = 2 * cache;
    }
    if ( (_BYTE)v22 )
    {
LABEL_18:
      v11 = 1;
      goto LABEL_19;
    }
    v9 = *(_QWORD *)(*((_QWORD *)this + 1) + 16LL);
  }
  if ( (v9 & 2) != 0 )
  {
    v10 = *((apac__APACDRCDecoder **)this + 3);
    if ( !v10 )
      goto LABEL_18;
    v11 = apac::APACDRCDecoder::Deserialize(v10, bs);
    if ( v11 )
      goto LABEL_19;
  }
  v11 = apac::TrimmingData::Deserialize(*((apac__TrimmingData **)this + 7), bs);
  if ( v11 )
    goto LABEL_18;
  v12 = *((float ***)this + 4);
  if ( v12 != *((float ***)this + 5) )
    *((_QWORD *)this + 5) = v12;
  begin = vec->begin;
  end = vec->end;
  if ( end == vec->begin )
  {
    v11 = 1;
  }
  else
  {
    v29 = v4;
    v5 = begin + 18;
    v15 = begin[10];
    while ( 1 )
    {
      for ( i = 0; i < *(v5 - 16); ++i )
      {
        while ( 1 )
        {
          v17 = *((_QWORD **)this + 5);
          if ( v17 == *((_QWORD **)this + 6) )
            break;
          *v17 = *(_QWORD *)(*((_QWORD *)v5 - 7) + 8LL * i++);
          *((_QWORD *)this + 5) += 8LL;
          if ( i >= *(v5 - 16) )
            goto LABEL_15;
        }
        std::vector<float *>::_M_realloc_insert<float * const&>((char *)this + 32);
      }
LABEL_15:
      if ( end == v5 )
        break;
      v18 = v5[10];
      v5 += 18;
      if ( v15 != v18 )
      {
        v4 = v29;
        goto LABEL_18;
      }
    }
    v4 = v29;
    v12 = *((float ***)this + 4);
  }
  v23 = *(_QWORD *)(*((_QWORD *)this + 1) + 16LL);
  if ( (v23 & 2) != 0 )
  {
    v24 = *((apac__APACDRCDecoder **)this + 3);
    if ( v24 )
    {
      v11 = apac::APACDRCDecoder::DecodeFrame(v24, v12);
      if ( !v11 )
      {
        v23 = *(_QWORD *)(*((_QWORD *)this + 1) + 16LL);
        goto LABEL_30;
      }
    }
LABEL_19:
    if ( v31 == _stack_chk_guard )
      return v11;
LABEL_38:
    v29 = v4;
    v30 = v5;
  }
LABEL_30:
  if ( (v23 & 4) == 0 )
    goto LABEL_19;
  v25 = *((apac__APACAudioSceneDecoder **)this + 2);
  if ( !v25 )
    goto LABEL_19;
  if ( v31 != _stack_chk_guard )
    goto LABEL_38;
  return apac::APACAudioSceneDecoder::DecodeFrame(v25, v12);
}


/* ------------------------------------------------------------------------ */
/* 0x000757e0  size 2176 bytes  544 insns
 *   apac::APACAncillaryFrameDecoder::Initialize(apac::ParamList const&)
 *   [_ZN4apac25APACAncillaryFrameDecoder10InitializeERKNS_9ParamListE]
 *   data refs: 0x733c0, 0x73400, 0x73590, 0x765c0, 0x76a10, 0x42a330, 0x42a4e0, 0x42a508, 0x42a528, 0x42c9f8, 0x42cc70, 0x42cd68, 0x42ce00, 0x42cf80, 0x42d208, 0x42d280, 0x42d2e0, 0x42d3d0, 0x42d618, 0x42d7c0, 0x42dad0, 0x42de18, 0x42de90, 0x43e3d0
 */
__int64 __fastcall apac::APACAncillaryFrameDecoder::Initialize(
        apac__APACAncillaryFrameDecoder *this,
        const apac__ParamList *a)
{
  __int64 v2; // x25
  __int64 v3; // x26
  __int64 v4; // x27
  apac__drc__DynRangeCompressor **v7; // x1
  apac__APACDRCDecoder *v8; // x0
  __int64 result; // x0
  _QWORD *v10; // x26
  void (__fastcall *v11)(apac__APACAudioSceneDecoder *); // x1
  _QWORD *v12; // x21
  void *v13; // x0
  void *v14; // x0
  void **v15; // x20
  void **v16; // x23
  void *v17; // x0
  void *v18; // x0
  void *v19; // x0
  void *v20; // x0
  void *v21; // x0
  void *v22; // x0
  void *v23; // x0
  void *v24; // x0
  void *v25; // x0
  void *v26; // x0
  _QWORD *v27; // x20
  _QWORD *v28; // x23
  void *v29; // x0
  void *v30; // x0
  __int64 v31; // x20
  void *v32; // x23
  void *v33; // x0
  void *v34; // x0
  void *v35; // x0
  _QWORD *v36; // x20
  _QWORD *v37; // x27
  _QWORD *v38; // x23
  _QWORD *v39; // x25
  void *v40; // x0
  void *v41; // x0
  void *v42; // x0
  void *v43; // x0
  _QWORD *v44; // x0
  void *v45; // x0
  apac__APACAudioSceneDecoder *v46; // x0
  _QWORD *v47; // x26
  void (__fastcall *v48)(apac__APACAudioSceneDecoder *); // x2
  _QWORD *v49; // x25
  void *v50; // x0
  void *v51; // x0
  void **v52; // x20
  void **v53; // x21
  void *v54; // x0
  void *v55; // x0
  void *v56; // x0
  void *v57; // x0
  void *v58; // x0
  void *v59; // x0
  void *v60; // x0
  void *v61; // x0
  void *v62; // x0
  void *v63; // x0
  _QWORD *v64; // x20
  _QWORD *v65; // x21
  void *v66; // x0
  void *v67; // x0
  __int64 v68; // x20
  void *v69; // x21
  void *v70; // x0
  void *v71; // x0
  void *v72; // x0
  _QWORD *v73; // x20
  _QWORD *v74; // x27
  _QWORD *v75; // x21
  _QWORD *v76; // x23
  void *v77; // x0
  void *v78; // x0
  void *v79; // x0
  void *v80; // x0
  _QWORD *v81; // x0
  void *v82; // x0
  char *v83; // x0
  char *v84; // x22
  char *v85; // x20
  __int64 v86; // x0
  _QWORD *v87; // x0
  const char *v88; // x0
  const char *v89; // x1
  int v90; // w20
  __int64 v91; // x1
  void *v92; // x0
  _QWORD *v93; // x20
  void (__fastcall *v94)(apac__APACDRCDecoder *); // x2
  _QWORD *v95; // x21
  void (__fastcall *v96)(apac__drc__DynRangeCompressor *); // x1
  unsigned int *v97; // x23
  unsigned int *v98; // x1
  unsigned int v99; // w0
  __int64 (__fastcall *v100)(); // x1
  unsigned int *v101; // x1
  unsigned int v102; // w0
  __int64 (__fastcall *v103)(); // x1
  __int64 v104; // [xsp+40h] [xbp+40h]
  __int64 v105; // [xsp+48h] [xbp+48h]
  __int64 v106; // [xsp+50h] [xbp+50h]
  void (__fastcall **v107)(boost::bad_any_cast *__hidden); // [xsp+60h] [xbp+60h]
  __int64 v108; // [xsp+68h] [xbp+68h]
  __int64 v109; // [xsp+70h] [xbp+70h]
  v7 = *(apac__drc__DynRangeCompressor ***)(*((_QWORD *)this + 1) + 24LL);
  if ( !v7 )
    goto LABEL_4;
  v8 = *((apac__APACDRCDecoder **)this + 3);
  if ( !v8 )
  {
    v8 = (apac__APACDRCDecoder *)operator new(0x10u);
    v93 = *((_QWORD **)this + 3);
    *((_QWORD *)this + 3) = v8;
    *(_QWORD *)v8 = off_42A538;
    *((_QWORD *)v8 + 1) = 0;
    if ( v93 )
    {
      v94 = *(void (__fastcall **)(apac__APACDRCDecoder *))(*v93 + 8LL);
      if ( v94 == apac::APACDRCDecoder::~APACDRCDecoder )
      {
        v95 = (_QWORD *)v93[1];
        *v93 = off_42A538;
        if ( v95 )
        {
          v96 = *(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*v95 + 8LL);
          if ( v96 == apac::drc::DynRangeCompressor::~DynRangeCompressor )
          {
            v97 = (unsigned int *)v95[6];
            *v95 = off_42A340;
            if ( v97 )
            {
              v104 = v2;
              v105 = v3;
              if ( &_pthread_key_create )
              {
                v98 = v97 + 2;
                do
                  v99 = __ldaxr(v98);
                while ( __stlxr(v99 - 1, v98) );
              }
              else
              {
                v99 = v97[2];
                v97[2] = v99 - 1;
              }
              if ( v99 != 1 )
                goto LABEL_198;
              v100 = *(__int64 (__fastcall **)())(*(_QWORD *)v97 + 16LL);
              if ( v100 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
                ((void (__fastcall *)(unsigned int *))v100)(v97);
              if ( &_pthread_key_create )
              {
                v101 = v97 + 3;
                do
                  v102 = __ldaxr(v101);
                while ( __stlxr(v102 - 1, v101) );
              }
              else
              {
                v102 = v97[3];
                v97[3] = v102 - 1;
              }
              if ( v102 == 1 )
              {
                v103 = *(__int64 (__fastcall **)())(*(_QWORD *)v97 + 24LL);
                if ( v103 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
                  (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v97 + 8LL))(v97);
                else
                  ((void (__fastcall *)(unsigned int *))v103)(v97);
                v2 = v104;
                v3 = v105;
              }
              else
              {
LABEL_198:
                v2 = v104;
                v3 = v105;
              }
            }
            operator delete(v95);
          }
          else
          {
            v96((apac__drc__DynRangeCompressor *)v95);
          }
        }
        operator delete(v93);
        v8 = *((apac__APACDRCDecoder **)this + 3);
        v7 = *(apac__drc__DynRangeCompressor ***)(*((_QWORD *)this + 1) + 24LL);
      }
      else
      {
        v94((apac__APACDRCDecoder *)v93);
        v8 = *((apac__APACDRCDecoder **)this + 3);
        v7 = *(apac__drc__DynRangeCompressor ***)(*((_QWORD *)this + 1) + 24LL);
      }
    }
    else
    {
      v7 = *(apac__drc__DynRangeCompressor ***)(*((_QWORD *)this + 1) + 24LL);
    }
  }
  result = apac::APACDRCDecoder::Initialize(v8, a, *v7);
  if ( !(_DWORD)result )
  {
LABEL_4:
    v104 = v2;
    v105 = v3;
    v10 = *((_QWORD **)this + 2);
    *((_QWORD *)this + 2) = 0;
    if ( v10 )
    {
      v11 = *(void (__fastcall **)(apac__APACAudioSceneDecoder *))(*v10 + 8LL);
      if ( v11 == apac::APACAudioSceneDecoder::~APACAudioSceneDecoder )
      {
        v12 = (_QWORD *)v10[1];
        *v10 = off_42A518;
        if ( v12 )
        {
          v106 = v4;
          v13 = (void *)v12[74];
          if ( v13 )
            operator delete(v13);
          v14 = (void *)v12[69];
          if ( v14 )
            operator delete(v14);
          v15 = (void **)v12[65];
          v16 = (void **)v12[66];
          if ( v15 != v16 )
          {
            do
            {
              if ( *v15 )
                operator delete(*v15);
              v15 += 3;
            }
            while ( v16 != v15 );
            v16 = (void **)v12[65];
          }
          if ( v16 )
            operator delete(v16);
          v17 = (void *)v12[62];
          if ( v17 )
            operator delete(v17);
          v18 = (void *)v12[57];
          if ( v18 )
            operator delete(v18);
          v19 = (void *)v12[54];
          if ( v19 )
            operator delete(v19);
          v20 = (void *)v12[51];
          if ( v20 )
            operator delete(v20);
          v21 = (void *)v12[48];
          if ( v21 )
            operator delete(v21);
          v22 = (void *)v12[45];
          if ( v22 )
            operator delete(v22);
          v23 = (void *)v12[42];
          if ( v23 )
            operator delete(v23);
          v24 = (void *)v12[39];
          if ( v24 )
            operator delete(v24);
          v25 = (void *)v12[35];
          if ( v25 )
            operator delete(v25);
          v26 = (void *)v12[32];
          if ( v26 )
            operator delete(v26);
          v27 = (_QWORD *)v12[29];
          v28 = (_QWORD *)v12[30];
          if ( v27 != v28 )
          {
            do
            {
              v29 = (void *)v27[8];
              if ( v29 )
                operator delete(v29);
              v30 = (void *)v27[5];
              if ( v30 )
                operator delete(v30);
              v27 += 11;
            }
            while ( v28 != v27 );
            v28 = (_QWORD *)v12[29];
          }
          if ( v28 )
            operator delete(v28);
          v31 = v12[26];
          v32 = (void *)v12[27];
          if ( (void *)v31 != v32 )
          {
            do
            {
              v33 = *(void **)(v31 + 24);
              if ( v33 )
                operator delete(v33);
              if ( *(_QWORD *)v31 )
                operator delete(*(void **)v31);
              v31 += 64;
            }
            while ( v32 != (void *)v31 );
            v32 = (void *)v12[26];
          }
          if ( v32 )
            operator delete(v32);
          v34 = (void *)v12[22];
          if ( v34 )
            operator delete(v34);
          v35 = (void *)v12[19];
          if ( v35 )
            operator delete(v35);
          v36 = (_QWORD *)v12[15];
          v37 = (_QWORD *)v12[16];
          if ( v36 != v37 )
          {
            do
            {
              v38 = (_QWORD *)v36[8];
              v39 = (_QWORD *)v36[9];
              if ( v38 != v39 )
              {
                do
                {
                  v40 = (void *)v38[7];
                  if ( v40 )
                    operator delete(v40);
                  v41 = (void *)v38[4];
                  if ( v41 )
                    operator delete(v41);
                  v38 += 10;
                }
                while ( v39 != v38 );
                v39 = (_QWORD *)v36[8];
              }
              if ( v39 )
                operator delete(v39);
              v42 = (void *)v36[4];
              if ( v42 )
                operator delete(v42);
              v43 = (void *)v36[1];
              if ( v43 )
                operator delete(v43);
              v36 += 11;
            }
            while ( v37 != v36 );
            v37 = (_QWORD *)v12[15];
          }
          if ( v37 )
            operator delete(v37);
          v44 = (_QWORD *)v12[9];
          if ( v44 != v12 + 11 )
            operator delete(v44);
          v45 = (void *)v12[2];
          if ( v45 )
            operator delete(v45);
          operator delete(v12);
          v4 = v106;
        }
        operator delete(v10);
      }
      else
      {
        v11((apac__APACAudioSceneDecoder *)v10);
      }
    }
    if ( *(_QWORD *)(*((_QWORD *)this + 1) + 32LL) )
    {
      v46 = (apac__APACAudioSceneDecoder *)operator new(0x10u);
      v47 = *((_QWORD **)this + 2);
      *((_QWORD *)this + 2) = v46;
      *(_QWORD *)v46 = off_42A518;
      *((_QWORD *)v46 + 1) = 0;
      if ( v47 )
      {
        v48 = *(void (__fastcall **)(apac__APACAudioSceneDecoder *))(*v47 + 8LL);
        if ( v48 == apac::APACAudioSceneDecoder::~APACAudioSceneDecoder )
        {
          v49 = (_QWORD *)v47[1];
          *v47 = off_42A518;
          if ( v49 )
          {
            v106 = v4;
            v50 = (void *)v49[74];
            if ( v50 )
              operator delete(v50);
            v51 = (void *)v49[69];
            if ( v51 )
              operator delete(v51);
            v52 = (void **)v49[65];
            v53 = (void **)v49[66];
            if ( v52 != v53 )
            {
              do
              {
                if ( *v52 )
                  operator delete(*v52);
                v52 += 3;
              }
              while ( v53 != v52 );
              v53 = (void **)v49[65];
            }
            if ( v53 )
              operator delete(v53);
            v54 = (void *)v49[62];
            if ( v54 )
              operator delete(v54);
            v55 = (void *)v49[57];
            if ( v55 )
              operator delete(v55);
            v56 = (void *)v49[54];
            if ( v56 )
              operator delete(v56);
            v57 = (void *)v49[51];
            if ( v57 )
              operator delete(v57);
            v58 = (void *)v49[48];
            if ( v58 )
              operator delete(v58);
            v59 = (void *)v49[45];
            if ( v59 )
              operator delete(v59);
            v60 = (void *)v49[42];
            if ( v60 )
              operator delete(v60);
            v61 = (void *)v49[39];
            if ( v61 )
              operator delete(v61);
            v62 = (void *)v49[35];
            if ( v62 )
              operator delete(v62);
            v63 = (void *)v49[32];
            if ( v63 )
              operator delete(v63);
            v64 = (_QWORD *)v49[29];
            v65 = (_QWORD *)v49[30];
            if ( v64 != v65 )
            {
              do
              {
                v66 = (void *)v64[8];
                if ( v66 )
                  operator delete(v66);
                v67 = (void *)v64[5];
                if ( v67 )
                  operator delete(v67);
                v64 += 11;
              }
              while ( v65 != v64 );
              v65 = (_QWORD *)v49[29];
            }
            if ( v65 )
              operator delete(v65);
            v68 = v49[26];
            v69 = (void *)v49[27];
            if ( (void *)v68 != v69 )
            {
              do
              {
                v70 = *(void **)(v68 + 24);
                if ( v70 )
                  operator delete(v70);
                if ( *(_QWORD *)v68 )
                  operator delete(*(void **)v68);
                v68 += 64;
              }
              while ( v69 != (void *)v68 );
              v69 = (void *)v49[26];
            }
            if ( v69 )
              operator delete(v69);
            v71 = (void *)v49[22];
            if ( v71 )
              operator delete(v71);
            v72 = (void *)v49[19];
            if ( v72 )
              operator delete(v72);
            v73 = (_QWORD *)v49[15];
            v74 = (_QWORD *)v49[16];
            if ( v73 != v74 )
            {
              do
              {
                v75 = (_QWORD *)v73[8];
                v76 = (_QWORD *)v73[9];
                if ( v75 != v76 )
                {
                  do
                  {
                    v77 = (void *)v75[7];
                    if ( v77 )
                      operator delete(v77);
                    v78 = (void *)v75[4];
                    if ( v78 )
                      operator delete(v78);
                    v75 += 10;
                  }
                  while ( v76 != v75 );
                  v76 = (_QWORD *)v73[8];
                }
                if ( v76 )
                  operator delete(v76);
                v79 = (void *)v73[4];
                if ( v79 )
                  operator delete(v79);
                v80 = (void *)v73[1];
                if ( v80 )
                  operator delete(v80);
                v73 += 11;
              }
              while ( v74 != v73 );
              v74 = (_QWORD *)v49[15];
            }
            if ( v74 )
              operator delete(v74);
            v81 = (_QWORD *)v49[9];
            if ( v81 != v49 + 11 )
              operator delete(v81);
            v82 = (void *)v49[2];
            if ( v82 )
              operator delete(v82);
            operator delete(v49);
            v4 = v106;
          }
          operator delete(v47);
          v46 = *((apac__APACAudioSceneDecoder **)this + 2);
        }
        else
        {
          v48((apac__APACAudioSceneDecoder *)v47);
          v46 = *((apac__APACAudioSceneDecoder **)this + 2);
        }
      }
      result = apac::APACAudioSceneDecoder::Initialize(v46, a, **(SC_AudioScenes ***)(*((_QWORD *)this + 1) + 32LL));
      if ( (_DWORD)result )
        goto LABEL_181;
    }
    v83 = *((char **)a + 2);
    v84 = (char *)a + 8;
    if ( !v83 )
      goto LABEL_172;
    v85 = v84;
    do
    {
      while ( *((int *)v83 + 8) > 1 )
      {
        v85 = v83;
        v83 = *((char **)v83 + 2);
        if ( !v83 )
          goto LABEL_170;
      }
      v83 = *((char **)v83 + 3);
    }
    while ( v83 );
LABEL_170:
    if ( v84 == v85 || *((int *)v85 + 8) > 2 )
    {
LABEL_172:
      v106 = v4;
      start();
    }
    v86 = *((_QWORD *)v85 + 5);
    if ( v86 )
      v87 = (_QWORD *)(*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v86 + 16LL))(v86);
    else
      v87 = `typeinfo for'void;
    v88 = (const char *)v87[1];
    v89 = (const char *)`typeinfo for'unsigned int[1];
    if ( v88 != v89 && (*v88 == 42 || strcmp(v88, v89)) )
    {
      v106 = v4;
      v107 = off_42A4F0;
      sub_70A44();
    }
    v90 = *(_DWORD *)(*((_QWORD *)v85 + 5) + 8LL);
    v91 = operator new(0xCu);
    v92 = *((void **)this + 7);
    *((_QWORD *)this + 7) = v91;
    *(_QWORD *)v91 = 0;
    *(_DWORD *)(v91 + 8) = v90;
    if ( !v92 || (operator delete(v92), *((_QWORD *)this + 7)) )
    {
      result = 0;
LABEL_181:
      v2 = v104;
      v3 = v105;
      goto LABEL_182;
    }
    result = 6;
    v2 = v104;
    v3 = v105;
  }
LABEL_182:
  if ( v108 != _stack_chk_guard )
  {
    v104 = v2;
    v105 = v3;
    v106 = v4;
  }
  return result;
}


