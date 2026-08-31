/* ============================================================================
 * apac::APACDRCDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 1848 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00074620    856  apac::APACDRCDecoder::Initialize(apac::ParamList const&, apac::drc::DynRangeCompressor&) */
/* 0x00074980     72  apac::APACDRCDecoder::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x000749d0     92  apac::APACDRCDecoder::DecodeFrame(float**) */
/* 0x00076860    424  apac::APACDRCDecoder::~APACDRCDecoder() */
/* 0x00076a10    404  apac::APACDRCDecoder::~APACDRCDecoder() */

/* ------------------------------------------------------------------------ */
/* 0x00074620  size 856 bytes  214 insns
 *   apac::APACDRCDecoder::Initialize(apac::ParamList const&, apac::drc::DynRangeCompressor&)
 *   [_ZN4apac14APACDRCDecoder10InitializeERKNS_9ParamListERNS_3drc18DynRangeCompressorE]
 *   data refs: 0x733c0, 0x73400, 0x73590, 0x30ea60, 0x42a330, 0x42a4e0, 0x42c9f8, 0x42cc70, 0x42cd68, 0x42ce00, 0x42d2e0, 0x42d3d0, 0x42d618, 0x42d748, 0x42d7c0, 0x42de90, 0x43e3d0, 0x43e520, 0x43e7e8, 0x43ec98
 */
__int64 __fastcall apac::APACDRCDecoder::Initialize(
        apac__APACDRCDecoder *this,
        const apac__ParamList *a,
        apac__drc__DynRangeCompressor *a2)
{
  __int64 v6; // x0
  int v7; // s0
  __int64 v8; // x1
  char v9; // w7
  __int16 v10; // w6
  __int64 v11; // x4
  __int64 v12; // x5
  __int64 v13; // x3
  unsigned int *v14; // x1
  unsigned int v15; // w3
  _QWORD *v16; // x23
  char v17; // w3
  __int64 v18; // x1
  void (__fastcall *v19)(apac__drc__DynRangeCompressor *); // x1
  unsigned int *v20; // x19
  unsigned int *v21; // x1
  unsigned int v22; // w0
  __int64 (__fastcall *v23)(); // x1
  unsigned int *v24; // x1
  unsigned int v25; // w0
  int *v26; // x2
  int *v27; // x0
  int *v28; // x4
  int *v29; // x1
  int *v30; // x19
  __int64 v31; // x0
  _QWORD *v32; // x0
  char *v33; // x0
  __int64 (__fastcall *v35)(); // x2
  void (__fastcall **v36)(boost::bad_any_cast *__hidden); // [xsp+40h] [xbp+40h]
  __int64 v37; // [xsp+50h] [xbp+50h]
  v6 = operator new(0x48u);
  v7 = *((_DWORD *)a2 + 8);
  v8 = *((_QWORD *)a2 + 6);
  *(_QWORD *)(v6 + 48) = v8;
  *(_DWORD *)(v6 + 32) = v7;
  v9 = *((_BYTE *)a2 + 8);
  v10 = *((_WORD *)a2 + 5);
  *(_QWORD *)v6 = off_42A340;
  v12 = *((_QWORD *)a2 + 2);
  v11 = *((_QWORD *)a2 + 3);
  *(_BYTE *)(v6 + 8) = v9;
  v13 = *((_QWORD *)a2 + 5);
  *(_WORD *)(v6 + 10) = v10;
  *(_QWORD *)(v6 + 16) = v12;
  *(_QWORD *)(v6 + 24) = v11;
  *(_QWORD *)(v6 + 40) = v13;
  if ( v8 )
  {
    if ( &_pthread_key_create )
    {
      v14 = (unsigned int *)(v8 + 8);
      do
        v15 = __ldaxr(v14);
      while ( __stlxr(v15 + 1, v14) );
    }
    else
    {
      ++*(_DWORD *)(v8 + 8);
    }
  }
  v16 = *((_QWORD **)this + 1);
  v17 = *((_BYTE *)a2 + 56);
  v18 = *(_QWORD *)((char *)a2 + 60);
  *((_QWORD *)this + 1) = v6;
  *(_BYTE *)(v6 + 56) = v17;
  *(_QWORD *)(v6 + 60) = v18;
  if ( v16 )
  {
    v19 = *(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*v16 + 8LL);
    if ( v19 == apac::drc::DynRangeCompressor::~DynRangeCompressor )
    {
      v20 = (unsigned int *)v16[6];
      *v16 = off_42A340;
      if ( v20 )
      {
        if ( &_pthread_key_create )
        {
          v21 = v20 + 2;
          do
            v22 = __ldaxr(v21);
          while ( __stlxr(v22 - 1, v21) );
        }
        else
        {
          v22 = v20[2];
          v20[2] = v22 - 1;
        }
        if ( v22 == 1 )
        {
          v23 = *(__int64 (__fastcall **)())(*(_QWORD *)v20 + 16LL);
          if ( v23 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
            ((void (__fastcall *)(unsigned int *))v23)(v20);
          if ( &_pthread_key_create )
          {
            v24 = v20 + 3;
            do
              v25 = __ldaxr(v24);
            while ( __stlxr(v25 - 1, v24) );
          }
          else
          {
            v25 = v20[3];
            v20[3] = v25 - 1;
          }
          if ( v25 == 1 )
          {
            v35 = *(__int64 (__fastcall **)())(*(_QWORD *)v20 + 24LL);
            if ( v35 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
              (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v20 + 8LL))(v20);
            else
              ((void (__fastcall *)(unsigned int *))v35)(v20);
          }
        }
      }
      operator delete(v16);
    }
    else
    {
      v19((apac__drc__DynRangeCompressor *)v16);
    }
  }
  v26 = *((int **)a + 2);
  v27 = (int *)((char *)a + 8);
  if ( !v26 )
    return 1;
  v28 = (int *)((char *)a + 8);
  v29 = *((int **)a + 2);
  do
  {
    while ( v29[8] > 4 )
    {
      v28 = v29;
      v29 = *((int **)v29 + 2);
      if ( !v29 )
        goto LABEL_23;
    }
    v29 = *((int **)v29 + 3);
  }
  while ( v29 );
LABEL_23:
  if ( v27 == v28 || v28[8] > 5 )
    return 1;
  v30 = (int *)((char *)a + 8);
  do
  {
    while ( v26[8] > 4 )
    {
      v30 = v26;
      v26 = *((int **)v26 + 2);
      if ( !v26 )
        goto LABEL_28;
    }
    v26 = *((int **)v26 + 3);
  }
  while ( v26 );
LABEL_28:
  if ( v27 == v30 || v30[8] > 5 )
    start();
  v31 = *((_QWORD *)v30 + 5);
  if ( v31 )
    v32 = (_QWORD *)(*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v31 + 16LL))(v31);
  else
    v32 = `typeinfo for'void;
  v33 = (char *)v32[1];
  if ( v33 != "P16DRCConfiguration" && (*v33 == 42 || strcmp(v33, "P16DRCConfiguration")) )
  {
    v36 = off_42A4F0;
    sub_70A44();
  }
  *(_QWORD *)(*((_QWORD *)this + 1) + 16LL) = *(_QWORD *)(*((_QWORD *)v30 + 5) + 8LL);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x00074980  size 72 bytes  18 insns
 *   apac::APACDRCDecoder::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac14APACDRCDecoder11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::APACDRCDecoder::Deserialize(apac__APACDRCDecoder *this, TBitstreamReader_u32 *bs)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return apac::drc::DynRangeCompressor::Deserialize(*((apac__drc__DynRangeCompressor **)this + 1), bs, 0);
}


/* ------------------------------------------------------------------------ */
/* 0x000749d0  size 92 bytes  23 insns
 *   apac::APACDRCDecoder::DecodeFrame(float**)
 *   [_ZN4apac14APACDRCDecoder11DecodeFrameEPPf]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall apac::APACDRCDecoder::DecodeFrame(apac__APACDRCDecoder *this, float **buf)
{
  unsigned int v3; // [xsp+24h] [xbp+24h] BYREF
  __int64 v4; // [xsp+30h] [xbp+30h]
  v3 = 0;
  return apac::drc::DynRangeCompressor::ProcessUniDrc(
           *((apac__drc__DynRangeCompressor **)this + 1),
           (const float **)buf,
           buf,
           &v3);
}


/* ------------------------------------------------------------------------ */
/* 0x00076860  size 424 bytes  106 insns
 *   apac::APACDRCDecoder::~APACDRCDecoder()
 *   apac::APACDRCDecoder::~APACDRCDecoder()
 *   [_ZN4apac14APACDRCDecoderD2Ev]
 *   [_ZN4apac14APACDRCDecoderD1Ev]
 *   data refs: 0x733c0, 0x73400, 0x73590, 0x42a330, 0x42a528, 0x42cc70, 0x42cd68, 0x42ce00, 0x42d280, 0x42d2e0, 0x42d618, 0x42d7c0, 0x43e520, 0x43ec98
 */
// Alternative name is '_ZN4apac14APACDRCDecoderD1Ev'
void __fastcall apac::APACDRCDecoder::~APACDRCDecoder(apac__APACDRCDecoder *this)
{
  __int64 v1; // x21
  __int64 v2; // x22
  apac__drc__DynRangeCompressor *v3; // x19
  void (__fastcall *v4)(apac__drc__DynRangeCompressor *); // x1
  unsigned int *v5; // x21
  unsigned int *v6; // x1
  unsigned int v7; // w0
  __int64 (__fastcall *v8)(); // x1
  unsigned int *v9; // x1
  unsigned int v10; // w0
  __int64 (__fastcall *v11)(); // x2
  __int64 v12; // [xsp+20h] [xbp+20h]
  __int64 v13; // [xsp+28h] [xbp+28h]
  __int64 v14; // [xsp+38h] [xbp+38h]
  __int64 v15; // [xsp+40h] [xbp+40h]
  v3 = *((apac__drc__DynRangeCompressor **)this + 1);
  *(_QWORD *)this = off_42A538;
  if ( !v3 )
  {
    if ( v14 == _stack_chk_guard )
      return;
LABEL_24:
    v12 = v1;
    v13 = v2;
  }
  v4 = *(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*(_QWORD *)v3 + 8LL);
  if ( v4 != apac::drc::DynRangeCompressor::~DynRangeCompressor )
  {
    if ( v14 == _stack_chk_guard )
    {
      v4(v3);
      return;
    }
    goto LABEL_24;
  }
  v5 = *((unsigned int **)v3 + 6);
  *(_QWORD *)v3 = off_42A340;
  if ( v5 )
  {
    if ( &_pthread_key_create )
    {
      v6 = v5 + 2;
      do
        v7 = __ldaxr(v6);
      while ( __stlxr(v7 - 1, v6) );
    }
    else
    {
      v7 = v5[2];
      v5[2] = v7 - 1;
    }
    if ( v7 == 1 )
    {
      v8 = *(__int64 (__fastcall **)())(*(_QWORD *)v5 + 16LL);
      if ( v8 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
        ((void (__fastcall *)(unsigned int *))v8)(v5);
      if ( &_pthread_key_create )
      {
        v9 = v5 + 3;
        do
          v10 = __ldaxr(v9);
        while ( __stlxr(v10 - 1, v9) );
      }
      else
      {
        v10 = v5[3];
        v5[3] = v10 - 1;
      }
      if ( v10 == 1 )
      {
        v11 = *(__int64 (__fastcall **)())(*(_QWORD *)v5 + 24LL);
        if ( v11 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
          (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v5 + 8LL))(v5);
        else
          ((void (__fastcall *)(unsigned int *))v11)(v5);
      }
    }
  }
  operator delete(v3);
}


/* ------------------------------------------------------------------------ */
/* 0x00076a10  size 404 bytes  101 insns
 *   apac::APACDRCDecoder::~APACDRCDecoder()
 *   [_ZN4apac14APACDRCDecoderD0Ev]
 *   data refs: 0x733c0, 0x73400, 0x73590, 0x42a330, 0x42a528, 0x42cc70, 0x42cd68, 0x42ce00, 0x42d280, 0x42d2e0, 0x42d618, 0x42d7c0, 0x43e520, 0x43ec98
 */
void __fastcall apac::APACDRCDecoder::~APACDRCDecoder(apac__APACDRCDecoder *this)
{
  __int64 v1; // x23
  _QWORD *v3; // x21
  void (__fastcall *v4)(apac__drc__DynRangeCompressor *); // x1
  unsigned int *v5; // x22
  unsigned int *v6; // x1
  unsigned int v7; // w0
  unsigned int v8; // w0
  __int64 (__fastcall *v9)(); // x1
  unsigned int *v10; // x1
  unsigned int v11; // w0
  __int64 (__fastcall *v12)(); // x2
  __int64 v13; // [xsp+30h] [xbp+30h]
  __int64 v14; // [xsp+48h] [xbp+48h]
  __int64 v15; // [xsp+50h] [xbp+50h]
  v3 = *((_QWORD **)this + 1);
  *(_QWORD *)this = off_42A538;
  if ( v3 )
  {
    v4 = *(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*v3 + 8LL);
    if ( v4 == apac::drc::DynRangeCompressor::~DynRangeCompressor )
    {
      v5 = (unsigned int *)v3[6];
      *v3 = off_42A340;
      if ( v5 )
      {
        if ( &_pthread_key_create )
        {
          v6 = v5 + 2;
          do
            v7 = __ldaxr(v6);
          while ( __stlxr(v7 - 1, v6) );
          if ( v7 != 1 )
            goto LABEL_8;
LABEL_12:
          v9 = *(__int64 (__fastcall **)())(*(_QWORD *)v5 + 16LL);
          if ( v9 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
            ((void (__fastcall *)(unsigned int *))v9)(v5);
          if ( &_pthread_key_create )
          {
            v10 = v5 + 3;
            do
              v11 = __ldaxr(v10);
            while ( __stlxr(v11 - 1, v10) );
          }
          else
          {
            v11 = v5[3];
            v5[3] = v11 - 1;
          }
          if ( v11 == 1 )
          {
            v12 = *(__int64 (__fastcall **)())(*(_QWORD *)v5 + 24LL);
            if ( v12 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
              (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v5 + 8LL))(v5);
            else
              ((void (__fastcall *)(unsigned int *))v12)(v5);
          }
          goto LABEL_8;
        }
        v8 = v5[2];
        v5[2] = v8 - 1;
        if ( v8 == 1 )
          goto LABEL_12;
      }
LABEL_8:
      operator delete(v3);
      goto LABEL_9;
    }
    v4((apac__drc__DynRangeCompressor *)v3);
  }
LABEL_9:
  if ( v14 != _stack_chk_guard )
    v13 = v1;
  operator delete(this);
}


