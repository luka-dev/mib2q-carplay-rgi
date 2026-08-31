/* ============================================================================
 * APACDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 9 function(s), 5308 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00093cc0    128  APACDecoder::Reset() */
/* 0x00093d40     68  APACDecoder::SetParameter(int, boost::any const&) */
/* 0x00093d90    192  APACDecoder::CreateDecoderElement(apac::ASCType) */
/* 0x00093e50    564  APACDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&) */
/* 0x00094090    292  APACDecoder::GetParameter(int, boost::any&) const */
/* 0x000941c0   1016  APACDecoder::~APACDecoder() */
/* 0x000945c0     84  APACDecoder::~APACDecoder() */
/* 0x00094620    128  APACDecoder::APACDecoder() */
/* 0x000946a0   2836  APACDecoder::Initialize(apac::GlobalConfig const&) */

/* ------------------------------------------------------------------------ */
/* 0x00093cc0  size 128 bytes  32 insns
 *   APACDecoder::Reset()
 *   [_ZN11APACDecoder5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder::Reset(APACDecoder *this)
{
  __int64 v1; // x19
  __int64 v2; // x20
  __int64 v3; // x0
  __int64 v5; // [xsp+38h] [xbp+38h]
  __int64 v6; // [xsp+40h] [xbp+40h]
  v1 = *((_QWORD *)this + 49);
  v2 = *((_QWORD *)this + 50);
  while ( v2 != v1 )
  {
    v3 = *(_QWORD *)(v1 + 8);
    v1 += 16;
    (*(void (__fastcall **)(_QWORD))(**(_QWORD **)(v3 + 16) + 24LL))(*(_QWORD *)(v3 + 16));
  }
  return v5 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00093d40  size 68 bytes  17 insns
 *   APACDecoder::SetParameter(int, boost::any const&)
 *   [_ZN11APACDecoder12SetParameterEiRKN5boost3anyE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder::SetParameter(APACDecoder *this, int n, const void *a)
{
  __int64 v4; // [xsp+20h] [xbp+20h]
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x00093d90  size 192 bytes  48 insns
 *   APACDecoder::CreateDecoderElement(apac::ASCType)
 *   [_ZN11APACDecoder20CreateDecoderElementEN4apac7ASCTypeE]
 *   data refs: 0x42ac58, 0x42cce8, 0x42ce00, 0x43e520
 */
__int64 *__fastcall APACDecoder::CreateDecoderElement(APACDecoder *this, int n)
{
  __int64 *v2; // x8
  __int64 *v3; // x19
  __int64 v5; // x1
  __int64 v6; // x0
  __int64 v7; // [xsp+30h] [xbp+30h]
  v3 = v2;
  *v2 = 0;
  if ( !n )
  {
    v5 = operator new(0x18u);
    v6 = *v3;
    *v3 = v5;
    *(_DWORD *)(v5 + 8) = 0;
    *(_QWORD *)(v5 + 16) = 0;
    *(_QWORD *)v5 = off_42AC68;
    if ( v6 )
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v6 + 8LL))(v6);
  }
  return v3;
}


/* ------------------------------------------------------------------------ */
/* 0x00093e50  size 564 bytes  141 insns
 *   APACDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&)
 *   [_ZN11APACDecoder11DecodeFrameEPKhjRN4apac14FrameOutRecordE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder::DecodeFrame(
        APACDecoder *this,
        const unsigned __int8 *a,
        unsigned int n,
        apac__FrameOutRecord *a2)
{
  __int64 v4; // x27
  __int64 v5; // x28
  int *v7; // x19
  __int64 result; // x0
  unsigned int cache; // w1
  unsigned int v10; // w2
  __int64 v11; // x20
  __int64 v12; // x0
  unsigned int v13; // w22
  unsigned int v14; // w28
  __int64 v15; // x26
  void (__fastcall *v16)(__int64, _QWORD, _QWORD, __int64, __int64, _QWORD); // x27
  __int64 v17; // x23
  unsigned int v18; // w0
  __int64 v19; // x1
  __int64 v20; // x20
  unsigned int v21; // w0
  char bitsLeft; // w4
  int *v23; // x0
  __int64 v24; // [xsp+50h] [xbp+50h]
  __int64 v25; // [xsp+58h] [xbp+58h]
  int *v26; // [xsp+60h] [xbp+60h]
  apac__FrameOutRecord *v27; // [xsp+68h] [xbp+68h]
  TBitstreamReader_u32 v28; // [xsp+70h] [xbp+70h] BYREF
  __int64 v29; // [xsp+88h] [xbp+88h]
  __int64 v30; // [xsp+90h] [xbp+90h]
  v27 = a2;
  TBitstreamReader<unsigned int>::TBitstreamReader(&v28, a, n);
  v7 = *((int **)this + 49);
  v26 = *((int **)this + 50);
  result = 1;
  if ( v26 == v7 )
    goto LABEL_12;
  cache = v28.cache;
  --v28.bitsLeft;
  v10 = v28.cache >> 31;
  if ( (v28.bitsLeft & 0x80000000) == 0 )
  {
    v24 = v4;
    v25 = v5;
    *((_DWORD *)this + 4) = v10;
    v28.cache = 2 * cache;
    goto LABEL_4;
  }
  LODWORD(v26) = v28.cache >> 31;
  TBitstreamReader<unsigned int>::FillCacheFrom(&v28, v28.cur);
  bitsLeft = v28.bitsLeft;
  v28.cur += 4;
  v28.bitsLeft += 32;
  if ( v28.bitsLeft )
  {
    v10 = (unsigned __int8)((v28.cache >> SLOBYTE(v28.bitsLeft)) | (unsigned __int8)v26);
    v7 = *((int **)this + 49);
    v26 = *((int **)this + 50);
    v28.cache <<= -bitsLeft;
  }
  else
  {
    v10 = (unsigned __int8)(LOBYTE(v28.cache) | (unsigned __int8)v26);
    v28.cache = 0;
    v7 = *((int **)this + 49);
    v26 = *((int **)this + 50);
  }
  v23 = v26;
  *((_DWORD *)this + 4) = v10;
  if ( v7 != v23 )
  {
    v24 = v4;
    v25 = v5;
    while ( 1 )
    {
LABEL_4:
      v11 = *((_QWORD *)v7 + 1);
      v12 = *(_QWORD *)(v11 + 16);
      if ( v12 )
      {
        v13 = *v7;
        result = (*(__int64 (__fastcall **)(__int64, TBitstreamReader_u32 *))(*(_QWORD *)v12 + 32LL))(v12, &v28);
        if ( (_DWORD)result )
        {
          v4 = v24;
          v5 = v25;
          goto LABEL_12;
        }
        v14 = apac::ConvertToAudioDataType(*(_DWORD *)(*((_QWORD *)v7 + 1) + 8LL));
        v15 = *((_QWORD *)this + 55);
        v16 = *(void (__fastcall **)(__int64, _QWORD, _QWORD, __int64, __int64, _QWORD))(*(_QWORD *)v15 + 24LL);
        v17 = *((_QWORD *)this + 52);
        v18 = (*(__int64 (__fastcall **)(_QWORD))(**(_QWORD **)(v11 + 16) + 64LL))(*(_QWORD *)(v11 + 16));
        v19 = *(_QWORD *)(v11 + 16);
        v20 = v17 + 8LL * v18;
        v21 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v19 + 56LL))(v19);
        v16(v15, v14, v13, v17, v20, v21);
        v10 = *((_DWORD *)this + 4);
      }
      v7 += 4;
      if ( v26 == v7 )
      {
        v4 = v24;
        v5 = v25;
        break;
      }
    }
  }
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, __int64, _QWORD))(**((_QWORD **)this + 56) + 24LL))(
             *((_QWORD *)this + 56),
             &v28,
             *((_QWORD *)this + 55) + 8LL,
             v10);
  if ( !(_DWORD)result )
  {
    result = APACExtFrameDecoder::DecodeFrame(*((APACExtFrameDecoder **)this + 57), &v28);
    if ( !(_DWORD)result )
      result = (*(__int64 (__fastcall **)(_QWORD, char *, apac__FrameOutRecord *))(**((_QWORD **)this + 55) + 32LL))(
                 *((_QWORD *)this + 55),
                 (char *)this + 392,
                 v27);
  }
LABEL_12:
  if ( v29 != _stack_chk_guard )
  {
    v24 = v4;
    v25 = v5;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00094090  size 292 bytes  73 insns
 *   APACDecoder::GetParameter(int, boost::any&) const
 *   [_ZNK11APACDecoder12GetParameterEiRN5boost3anyE]
 *   data refs: 0x42aeb8, 0x42aee8, 0x42af18, 0x42cd60, 0x42ce00, 0x42d110, 0x42d318, 0x43e520
 */
__int64 __fastcall APACDecoder::GetParameter(APACDecoder *this, int n, void *a)
{
  __int64 result; // x0
  _QWORD *v6; // x1
  __int64 v7; // x0
  __int64 v8; // x0
  __int64 v9; // x1
  __int64 v10; // x21
  _QWORD *v11; // x1
  __int64 v12; // x0
  _QWORD *v13; // x1
  __int64 v14; // [xsp+40h] [xbp+40h]
  if ( n == 5 )
  {
    v13 = (_QWORD *)operator new(0x10u);
    v12 = *(_QWORD *)a;
    *(_QWORD *)a = v13;
    *v13 = off_42AEC8;
    v13[1] = (char *)this + 24;
    if ( !v12 )
      return 0;
LABEL_11:
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v12 + 8LL))(v12);
    return 0;
  }
  result = 1;
  if ( n != 6 )
    return result;
  v6 = (_QWORD *)operator new(0x10u);
  v7 = *(_QWORD *)a;
  *(_QWORD *)a = v6;
  *v6 = off_42AF28;
  v6[1] = 0;
  if ( v7 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v7 + 8LL))(v7);
  v8 = *((_QWORD *)this + 56);
  if ( v8 )
  {
    v9 = *(_QWORD *)(v8 + 16);
    if ( v9 )
    {
      v10 = *(_QWORD *)(v9 + 8);
      v11 = (_QWORD *)operator new(0x10u);
      v12 = *(_QWORD *)a;
      *(_QWORD *)a = v11;
      *v11 = off_42AEF8;
      v11[1] = v10;
      if ( v12 )
        goto LABEL_11;
    }
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000941c0  size 1016 bytes  254 insns
 *   APACDecoder::~APACDecoder()
 *   APACDecoder::~APACDecoder()
 *   [_ZN11APACDecoderD1Ev]
 *   [_ZN11APACDecoderD2Ev]
 *   data refs: 0x80280, 0x955a0, 0x95980, 0x42add0, 0x42ae08, 0x42b690, 0x42ca90, 0x42cc88, 0x42ce00, 0x42ce18, 0x42d888, 0x42dd28, 0x42de78, 0x43e520
 */
// Alternative name is '_ZN11APACDecoderD2Ev'
void __fastcall APACDecoder::~APACDecoder(APACDecoder *this)
{
  void ***v2; // x21
  void **v3; // x20
  void **v4; // x23
  void *v5; // x0
  void (__fastcall *v6)(apac__ExtFrameUnkownDecoder *); // x1
  __int64 v7; // x0
  _QWORD *v8; // x20
  void (__fastcall *v9)(ContentOutBuffer *); // x1
  void *v10; // x0
  _QWORD *v11; // x23
  _QWORD *v12; // x21
  void *v13; // x0
  void *v14; // x0
  void *v15; // x0
  _QWORD *v16; // x20
  _QWORD *v17; // x21
  __int64 v18; // x0
  void *v19; // x0
  void **v20; // x20
  void **v21; // x21
  void **v22; // x20
  void **v23; // x21
  void **v24; // x20
  void **v25; // x21
  void *v26; // x0
  void (__fastcall ****v27)(MP4LoudnessMeasure *__hidden); // x20
  void (__fastcall ****v28)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v29)(MP4LoudnessMeasure *__hidden); // x21
  void (__fastcall ***v30)(MP4LoudnessMeasure *__hidden); // x23
  void (__fastcall *v31)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v32)(MP4LoudnessMeasure *__hidden); // x0
  void (__fastcall ****v33)(MP4LoudnessMeasure *__hidden); // x20
  void (__fastcall ****v34)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v35)(MP4LoudnessMeasure *__hidden); // x21
  void (__fastcall ***v36)(MP4LoudnessMeasure *__hidden); // x23
  void (__fastcall *v37)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v38)(MP4LoudnessMeasure *__hidden); // x0
  void *v39; // x0
  void *v40; // x0
  void *v41; // x0
  __int64 v42; // [xsp+60h] [xbp+60h]
  v2 = *((void ****)this + 57);
  *(_QWORD *)this = off_42AE18;
  if ( !v2 )
    goto LABEL_13;
  v3 = *v2;
  v4 = v2[1];
  if ( *v2 == v4 )
    goto LABEL_10;
  do
  {
    while ( 1 )
    {
      v5 = *v3;
      if ( !*v3 )
        goto LABEL_5;
      v6 = *(void (__fastcall **)(apac__ExtFrameUnkownDecoder *))(*(_QWORD *)v5 + 8LL);
      if ( v6 != apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder )
        break;
      operator delete(v5);
LABEL_5:
      if ( v4 == ++v3 )
        goto LABEL_9;
    }
    ++v3;
    ((void (*)(void))v6)();
  }
  while ( v4 != v3 );
LABEL_9:
  v4 = *v2;
LABEL_10:
  if ( v4 )
    operator delete(v4);
  operator delete(v2);
LABEL_13:
  v7 = *((_QWORD *)this + 56);
  if ( v7 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v7 + 8LL))(v7);
  v8 = *((_QWORD **)this + 55);
  if ( v8 )
  {
    v9 = *(void (__fastcall **)(ContentOutBuffer *))(*v8 + 8LL);
    if ( v9 == ContentOutBuffer::~ContentOutBuffer )
    {
      v10 = (void *)v8[4];
      *v8 = off_42B6A0;
      if ( v10 )
        operator delete(v10);
      *v8 = &unk_42ADE0;
      v11 = (_QWORD *)v8[2];
      v12 = (_QWORD *)v8[1];
      if ( v12 != v11 )
      {
        do
        {
          v13 = (void *)v12[6];
          if ( v13 )
            operator delete(v13);
          v14 = (void *)v12[2];
          if ( v14 )
            operator delete(v14);
          v12 += 9;
        }
        while ( v11 != v12 );
        v11 = (_QWORD *)v8[1];
      }
      if ( v11 )
        operator delete(v11);
      operator delete(v8);
    }
    else
    {
      v9(*((ContentOutBuffer **)this + 55));
    }
  }
  v15 = *((void **)this + 52);
  if ( v15 )
    operator delete(v15);
  v16 = *((_QWORD **)this + 49);
  v17 = *((_QWORD **)this + 50);
  if ( v16 != v17 )
  {
    do
    {
      v18 = v16[1];
      if ( v18 )
        (*(void (__fastcall **)(__int64))(*(_QWORD *)v18 + 8LL))(v18);
      v16 += 2;
    }
    while ( v17 != v16 );
    v17 = *((_QWORD **)this + 49);
  }
  if ( v17 )
    operator delete(v17);
  v19 = *((void **)this + 45);
  if ( v19 )
    operator delete(v19);
  v20 = *((void ***)this + 42);
  v21 = *((void ***)this + 43);
  if ( v20 != v21 )
  {
    do
    {
      if ( *v20 )
        operator delete(*v20);
      v20 += 3;
    }
    while ( v21 != v20 );
    v21 = *((void ***)this + 42);
  }
  if ( v21 )
    operator delete(v21);
  v22 = *((void ***)this + 39);
  v23 = *((void ***)this + 40);
  if ( v22 != v23 )
  {
    do
    {
      if ( *v22 )
        operator delete(*v22);
      v22 += 3;
    }
    while ( v23 != v22 );
    v23 = *((void ***)this + 39);
  }
  if ( v23 )
    operator delete(v23);
  v24 = *((void ***)this + 36);
  v25 = *((void ***)this + 37);
  if ( v24 != v25 )
  {
    do
    {
      if ( *v24 )
        operator delete(*v24);
      v24 += 3;
    }
    while ( v25 != v24 );
    v25 = *((void ***)this + 36);
  }
  if ( v25 )
    operator delete(v25);
  v26 = *((void **)this + 33);
  if ( v26 )
    operator delete(v26);
  v27 = *((void (__fastcall *****)(MP4LoudnessMeasure *__hidden))this + 30);
  v28 = *((void (__fastcall *****)(MP4LoudnessMeasure *__hidden))this + 31);
  if ( v27 != v28 )
  {
    do
    {
      v29 = v27[2];
      v30 = v27[3];
      if ( v29 != v30 )
      {
        do
        {
          while ( 1 )
          {
            v31 = **v29;
            if ( v31 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
              break;
            v29 += 2;
            if ( v30 == v29 )
              goto LABEL_69;
          }
          v32 = v29;
          v29 += 2;
          v31((MP4LoudnessMeasure *)v32);
        }
        while ( v30 != v29 );
LABEL_69:
        v30 = v27[2];
      }
      if ( v30 )
        operator delete(v30);
      v27 += 5;
    }
    while ( v28 != v27 );
    v28 = *((void (__fastcall *****)(MP4LoudnessMeasure *__hidden))this + 30);
  }
  if ( v28 )
    operator delete(v28);
  v33 = *((void (__fastcall *****)(MP4LoudnessMeasure *__hidden))this + 27);
  v34 = *((void (__fastcall *****)(MP4LoudnessMeasure *__hidden))this + 28);
  if ( v33 != v34 )
  {
    do
    {
      v35 = v33[2];
      v36 = v33[3];
      if ( v35 != v36 )
      {
        do
        {
          while ( 1 )
          {
            v37 = **v35;
            if ( v37 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
              break;
            v35 += 2;
            if ( v36 == v35 )
              goto LABEL_82;
          }
          v38 = v35;
          v35 += 2;
          v37((MP4LoudnessMeasure *)v38);
        }
        while ( v36 != v35 );
LABEL_82:
        v36 = v33[2];
      }
      if ( v36 )
        operator delete(v36);
      v33 += 5;
    }
    while ( v34 != v33 );
    v34 = *((void (__fastcall *****)(MP4LoudnessMeasure *__hidden))this + 27);
  }
  if ( v34 )
    operator delete(v34);
  v39 = *((void **)this + 20);
  if ( v39 )
    operator delete(v39);
  v40 = *((void **)this + 17);
  if ( v40 )
    operator delete(v40);
  v41 = *((void **)this + 12);
  if ( v41 )
    operator delete(v41);
}


/* ------------------------------------------------------------------------ */
/* 0x000945c0  size 84 bytes  21 insns
 *   APACDecoder::~APACDecoder()
 *   [_ZN11APACDecoderD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall APACDecoder::~APACDecoder(APACDecoder *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  APACDecoder::~APACDecoder(this);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00094620  size 128 bytes  32 insns
 *   APACDecoder::APACDecoder()
 *   APACDecoder::APACDecoder()
 *   [_ZN11APACDecoderC1Ev]
 *   [_ZN11APACDecoderC2Ev]
 *   data refs: 0x42ae08, 0x42cc88, 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN11APACDecoderC2Ev'
void __fastcall APACDecoder::APACDecoder(APACDecoder *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  *(_QWORD *)this = off_42AE18;
  *((_BYTE *)this + 8) = 0;
  *(_QWORD *)((char *)this + 12) = 0;
  DRCConfiguration::DRCConfiguration((APACDecoder *)((char *)this + 24));
  *((_QWORD *)this + 49) = 0;
  *((_QWORD *)this + 50) = 0;
  *((_QWORD *)this + 51) = 0;
  *((_QWORD *)this + 52) = 0;
  *((_QWORD *)this + 53) = 0;
  *((_QWORD *)this + 54) = 0;
  *((_QWORD *)this + 55) = 0;
  *((_QWORD *)this + 56) = 0;
  *((_QWORD *)this + 57) = 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000946a0  size 2836 bytes  708 insns
 *   APACDecoder::Initialize(apac::GlobalConfig const&)
 *   [_ZN11APACDecoder10InitializeERKN4apac12GlobalConfigE]
 *   data refs: 0x0, 0x8, 0x10, 0x20, 0x951c0, 0x955a0, 0x95980, 0x30f198, 0x42a370, 0x42add0, 0x42ae58, 0x42ae88, 0x42aeb8, 0x42aee8, 0x42b690, 0x42ca90, 0x42ce00, 0x42ce18, 0x42d110, 0x42d300, 0x42d318, 0x42d808, 0x42d888, 0x42db68
 */
__int64 __fastcall APACDecoder::Initialize(APACDecoder *this, const apac__GlobalConfig *a)
{
  const apac__GlobalConfig *v2; // x20
  __int64 v4; // x19
  __int64 v5; // x1
  __int64 v6; // x0
  __int64 v7; // x1
  int *v8; // x19
  __int64 v9; // x1
  __int64 v10; // x0
  __int64 v11; // x1
  int *v12; // x19
  _QWORD *v13; // x1
  apac *v14; // x0
  __int64 v15; // x0
  _QWORD *v16; // x20
  void *v17; // x0
  void (__fastcall *v18)(ContentOutBuffer *); // x1
  void *v19; // x0
  _QWORD *v20; // x19
  _QWORD *v21; // x22
  void *v22; // x0
  void *v23; // x0
  void *v24; // x19
  void (__fastcall *v25)(ContentOutBuffer *); // x1
  void *v26; // x0
  _QWORD *v27; // x20
  _QWORD *v28; // x21
  void *v29; // x0
  void *v30; // x0
  unsigned int v31; // w19
  int8x8_t v32; // d0
  __int64 v33; // x19
  __int64 v34; // x20
  __int64 v35; // x21
  __int64 v36; // x0
  __int64 (__fastcall *v37)(apac__GlobalConfig__CodecConfigData *); // x1
  _QWORD *v38; // x21
  apac *v39; // x0
  __int64 v40; // x0
  __int64 v41; // x1
  int *v42; // x20
  unsigned int *v43; // x24
  unsigned __int64 v44; // x19
  char *v45; // x22
  __int64 (__fastcall **v46)(); // x19
  __int64 (__fastcall ***v47)(); // x1
  __int64 v48; // x0
  _QWORD *v49; // x1
  _QWORD *v50; // x4
  __int64 v51; // x3
  int v52; // w0
  __int64 v53; // x0
  __int64 v54; // x1
  int *v55; // x19
  _DWORD *v56; // x0
  int v57; // w20
  __int64 v58; // x2
  __int64 v59; // x0
  __int64 v60; // x1
  int *v61; // x19
  _QWORD *v62; // x1
  __int64 v63; // x0
  apac__APACAncillaryFrameDecoder *v64; // x19
  __int64 v65; // x0
  APACExtFrameDecoder *v66; // x20
  int8x8_t v67; // d0
  void ***v68; // x21
  void **v69; // x22
  void **v70; // x24
  void *v71; // x0
  void (*v72)(void); // x1
  _QWORD *v73; // x0
  apac *v75; // x20
  _QWORD **v76; // x0
  __int64 (__fastcall *v77)(apac__GlobalConfig__CodecConfigData *); // x1
  __int64 v78; // x2
  __int64 v79; // x1
  int *v80; // x20
  __int64 v81; // x21
  _QWORD *v82; // x1
  __int64 v83; // x0
  _DWORD *v84; // x2
  unsigned int v85; // w1
  _QWORD *v86; // x2
  unsigned __int64 v87; // x3
  _QWORD *v88; // x1
  apac *v89; // [xsp+68h] [xbp+68h]
  __int64 (__fastcall **v90)(); // [xsp+70h] [xbp+70h]
  _QWORD *v91; // [xsp+78h] [xbp+78h]
  int *v92; // [xsp+80h] [xbp+80h]
  int *v93; // [xsp+88h] [xbp+88h]
  int v94; // [xsp+9Ch] [xbp+9Ch] BYREF
  int v95; // [xsp+A0h] [xbp+A0h] BYREF
  int v96; // [xsp+A4h] [xbp+A4h] BYREF
  int v97; // [xsp+A8h] [xbp+A8h] BYREF
  int v98; // [xsp+ACh] [xbp+ACh] BYREF
  int v99; // [xsp+B0h] [xbp+B0h] BYREF
  __int64 v100; // [xsp+B8h] [xbp+B8h] BYREF
  void *v101; // [xsp+C0h] [xbp+C0h] BYREF
  __int64 v102; // [xsp+C8h] [xbp+C8h]
  char v103[8]; // [xsp+D0h] [xbp+D0h] BYREF
  int v104; // [xsp+D8h] [xbp+D8h] BYREF
  __int64 v105; // [xsp+E0h] [xbp+E0h]
  int *v106; // [xsp+E8h] [xbp+E8h]
  int *v107; // [xsp+F0h] [xbp+F0h]
  __int64 v108; // [xsp+F8h] [xbp+F8h]
  _QWORD v109[3]; // [xsp+100h] [xbp+100h] BYREF
  __int64 v110; // [xsp+120h] [xbp+120h]
  v2 = a;
  v89 = a;
  v94 = 0;
  v101 = &v94;
  v104 = 0;
  v105 = 0;
  v106 = &v104;
  v107 = &v104;
  v108 = 0;
  v4 = std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
         v103,
         &v104,
         &unk_30F198,
         &v101,
         &v100);
  LODWORD(v2) = *((_DWORD *)v2 + 9);
  v5 = operator new(0x10u);
  *(_DWORD *)(v5 + 8) = (_DWORD)v2;
  *(_QWORD *)v5 = off_42A380;
  v6 = *(_QWORD *)(v4 + 40);
  *(_QWORD *)(v4 + 40) = v5;
  if ( v6 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v6 + 8LL))(v6);
  v7 = v105;
  v95 = 2;
  v8 = &v104;
  if ( !v105 )
    goto LABEL_108;
  do
  {
    while ( *(int *)(v7 + 32) > 1 )
    {
      v8 = (int *)v7;
      v7 = *(_QWORD *)(v7 + 16);
      if ( !v7 )
        goto LABEL_6;
    }
    v7 = *(_QWORD *)(v7 + 24);
  }
  while ( v7 );
LABEL_6:
  if ( v8 == &v104 || v8[8] > 2 )
  {
LABEL_108:
    v101 = &v95;
    v8 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                  v103,
                  v8,
                  &unk_30F198,
                  &v101,
                  &v100);
  }
  v9 = operator new(0x10u);
  *(_DWORD *)(v9 + 8) = 1024;
  *(_QWORD *)v9 = off_42A380;
  v10 = *((_QWORD *)v8 + 5);
  *((_QWORD *)v8 + 5) = v9;
  if ( v10 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v10 + 8LL))(v10);
  v11 = v105;
  v96 = 3;
  v12 = &v104;
  if ( !v105 )
    goto LABEL_107;
  do
  {
    while ( *(int *)(v11 + 32) > 2 )
    {
      v12 = (int *)v11;
      v11 = *(_QWORD *)(v11 + 16);
      if ( !v11 )
        goto LABEL_13;
    }
    v11 = *(_QWORD *)(v11 + 24);
  }
  while ( v11 );
LABEL_13:
  if ( v12 == &v104 || v12[8] > 3 )
  {
LABEL_107:
    v101 = &v96;
    v12 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                   v103,
                   v12,
                   &unk_30F198,
                   &v101,
                   &v100);
  }
  v13 = (_QWORD *)operator new(0x10u);
  v14 = v89;
  *v13 = off_42AE68;
  v13[1] = v14;
  v15 = *((_QWORD *)v12 + 5);
  *((_QWORD *)v12 + 5) = v13;
  if ( v15 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v15 + 8LL))(v15);
  apac::CreateContentMixer(v89);
  v16 = *((_QWORD **)this + 55);
  v17 = v101;
  v101 = nullptr;
  *((_QWORD *)this + 55) = v17;
  if ( v16 )
  {
    v18 = *(void (__fastcall **)(ContentOutBuffer *))(*v16 + 8LL);
    if ( v18 == ContentOutBuffer::~ContentOutBuffer )
    {
      v19 = (void *)v16[4];
      *v16 = off_42B6A0;
      if ( v19 )
        operator delete(v19);
      *v16 = &unk_42ADE0;
      v20 = (_QWORD *)v16[2];
      v21 = (_QWORD *)v16[1];
      if ( v21 != v20 )
      {
        do
        {
          v22 = (void *)v21[6];
          if ( v22 )
            operator delete(v22);
          v23 = (void *)v21[2];
          if ( v23 )
            operator delete(v23);
          v21 += 9;
        }
        while ( v20 != v21 );
        v20 = (_QWORD *)v16[1];
      }
      if ( v20 )
        operator delete(v20);
      operator delete(v16);
    }
    else
    {
      v18((ContentOutBuffer *)v16);
    }
    v24 = v101;
    if ( v101 )
    {
      v25 = *(void (__fastcall **)(ContentOutBuffer *))(*(_QWORD *)v101 + 8LL);
      if ( v25 == ContentOutBuffer::~ContentOutBuffer )
      {
        v26 = *(void **)((char *)&qword_20 + (_QWORD)v101);
        *(_QWORD *)v101 = off_42B6A0;
        if ( v26 )
          operator delete(v26);
        *(_QWORD *)v24 = &unk_42ADE0;
        v27 = *(_QWORD **)((char *)&word_10 + (_QWORD)v24);
        v28 = *(_QWORD **)&byte_8[(_QWORD)v24];
        if ( v28 != v27 )
        {
          do
          {
            v29 = (void *)v28[6];
            if ( v29 )
              operator delete(v29);
            v30 = (void *)v28[2];
            if ( v30 )
              operator delete(v30);
            v28 += 9;
          }
          while ( v27 != v28 );
          v27 = *(_QWORD **)&byte_8[(_QWORD)v24];
        }
        if ( v27 )
          operator delete(v27);
        operator delete(v24);
        v17 = *((void **)this + 55);
        goto LABEL_45;
      }
      v25((ContentOutBuffer *)v101);
    }
    v17 = *((void **)this + 55);
  }
LABEL_45:
  if ( !v17 )
  {
    v41 = v105;
LABEL_121:
    v31 = 1;
    goto LABEL_119;
  }
  v31 = (*(__int64 (__fastcall **)(void *, apac *))(*(_QWORD *)v17 + 16LL))(v17, v89);
  if ( !v31 )
  {
    v34 = *((_QWORD *)this + 49);
    v33 = *((_QWORD *)this + 50);
    if ( v34 != v33 )
    {
      v35 = *((_QWORD *)this + 49);
      do
      {
        v36 = *(_QWORD *)(v35 + 8);
        if ( v36 )
          (*(void (__fastcall **)(__int64))(*(_QWORD *)v36 + 8LL))(v36);
        v35 += 16;
      }
      while ( v33 != v35 );
      *((_QWORD *)this + 50) = v34;
    }
    memset(v109, 0, sizeof(v109));
    v37 = **(__int64 (__fastcall ***)(apac__GlobalConfig__CodecConfigData *))v89;
    if ( v37 == apac::GlobalConfig::CodecConfigData::configs )
    {
      v38 = *(_QWORD **)(*((_QWORD *)v89 + 1) + 176LL);
      v39 = v89;
    }
    else
    {
      v75 = v89;
      v76 = (_QWORD **)v37(v89);
      v77 = **(__int64 (__fastcall ***)(apac__GlobalConfig__CodecConfigData *))v75;
      v38 = *v76;
      v39 = v89;
      if ( v77 != apac::GlobalConfig::CodecConfigData::configs )
      {
        v40 = v77(v89);
        goto LABEL_56;
      }
    }
    v40 = *((_QWORD *)v39 + 1) + 176LL;
LABEL_56:
    v91 = *(_QWORD **)(v40 + 8);
    if ( v38 != v91 )
    {
      v92 = &v97;
      v93 = &v99;
      v90 = &off_42AE98;
      do
      {
        (*(void (__fastcall **)(__int64 *__return_ptr, APACDecoder *, _QWORD))(*(_QWORD *)this + 56LL))(
          &v100,
          this,
          *(unsigned int *)(*v38 + 8LL));
        v41 = v105;
        if ( !v100 )
          goto LABEL_121;
        v97 = 1;
        v42 = &v104;
        if ( !v105 )
          goto LABEL_64;
        do
        {
          while ( *(int *)(v41 + 32) > 0 )
          {
            v42 = (int *)v41;
            v41 = *(_QWORD *)(v41 + 16);
            if ( !v41 )
              goto LABEL_62;
          }
          v41 = *(_QWORD *)(v41 + 24);
        }
        while ( v41 );
LABEL_62:
        if ( v42 == &v104 || v42[8] > 1 )
        {
LABEL_64:
          v101 = v92;
          v42 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                         v103,
                         v42,
                         &unk_30F198,
                         &v101,
                         v93);
        }
        v43 = (unsigned int *)(*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v38 + 32LL))(*v38);
        v44 = 20LL * v43[2] + 12;
        v45 = (char *)operator new(v44);
        memset(v45, 0, v44);
        memcpy(v45, v43, v44);
        v46 = (__int64 (__fastcall **)())&v45[v44];
        v47 = (__int64 (__fastcall ***)())operator new(0x20u);
        *v47 = v90;
        v47[1] = (__int64 (__fastcall **)())v45;
        v47[2] = v46;
        v47[3] = v46;
        v48 = *((_QWORD *)v42 + 5);
        *((_QWORD *)v42 + 5) = v47;
        if ( v48 )
          (*(void (__fastcall **)(__int64))(*(_QWORD *)v48 + 8LL))(v48);
        v31 = (*(__int64 (__fastcall **)(__int64, char *, _QWORD))(*(_QWORD *)v100 + 16LL))(v100, v103, *v38);
        if ( v31 )
        {
          if ( !v100 )
            goto LABEL_118;
          (*(void (**)(void))(*(_QWORD *)v100 + 8LL))();
          v41 = v105;
          goto LABEL_119;
        }
        v49 = *((_QWORD **)this + 50);
        v50 = *((_QWORD **)this + 51);
        v51 = *(unsigned int *)(*v38 + 8LL);
        v102 = v100;
        v100 = 0;
        v52 = *((_DWORD *)v109 + v51);
        LODWORD(v101) = v52;
        *((_DWORD *)v109 + v51) = v52 + 1;
        if ( v49 == v50 )
        {
          std::vector<std::pair<unsigned int,std::unique_ptr<apac::DecElement>>>::_M_realloc_insert<std::pair<unsigned int,std::unique_ptr<apac::DecElement>>>(
            (std_vector *)((char *)this + 392),
            v49,
            &v101);
        }
        else
        {
          *(_DWORD *)v49 = v52;
          v53 = v102;
          v102 = 0;
          v49[1] = v53;
          *((_QWORD *)this + 50) += 16LL;
        }
        if ( v102 )
          (*(void (__fastcall **)(__int64))(*(_QWORD *)v102 + 8LL))(v102);
        if ( v100 )
          (*(void (__fastcall **)(__int64))(*(_QWORD *)v100 + 8LL))(v100);
        ++v38;
      }
      while ( v91 != v38 );
    }
    v54 = v105;
    v98 = 4;
    v55 = &v104;
    if ( !v105 )
      goto LABEL_81;
    do
    {
      if ( *(int *)(v54 + 32) <= 3 )
      {
        v54 = *(_QWORD *)(v54 + 24);
      }
      else
      {
        v55 = (int *)v54;
        v54 = *(_QWORD *)(v54 + 16);
      }
    }
    while ( v54 );
    if ( v55 == &v104 || v55[8] > 4 )
    {
LABEL_81:
      v101 = &v98;
      v55 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                     v103,
                     v55,
                     &unk_30F198,
                     &v101,
                     &v100);
    }
    v56 = *((_DWORD **)v89 + 32);
    if ( *v56 )
    {
      v57 = (unsigned __int16)*v56;
      if ( *v56 == 0x10000 )
      {
        v32.n64_u32[0] = v56[1];
        v57 = (unsigned __int8)vaddv_s8(vcnt_s8(v32));
      }
    }
    else
    {
      v57 = v56[2];
    }
    v58 = operator new(0x10u);
    *(_DWORD *)(v58 + 8) = v57;
    *(_QWORD *)v58 = off_42A380;
    v59 = *((_QWORD *)v55 + 5);
    *((_QWORD *)v55 + 5) = v58;
    if ( v59 )
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v59 + 8LL))(v59);
    v60 = v105;
    v99 = 5;
    v61 = &v104;
    if ( !v105 )
      goto LABEL_93;
    do
    {
      if ( *(int *)(v60 + 32) <= 4 )
      {
        v60 = *(_QWORD *)(v60 + 24);
      }
      else
      {
        v61 = (int *)v60;
        v60 = *(_QWORD *)(v60 + 16);
      }
    }
    while ( v60 );
    if ( v61 == &v104 || v61[8] > 5 )
    {
LABEL_93:
      v101 = &v99;
      v61 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                     v103,
                     v61,
                     &unk_30F198,
                     &v101,
                     &v100);
    }
    v62 = (_QWORD *)operator new(0x10u);
    *v62 = off_42AEC8;
    v62[1] = (char *)this + 24;
    v63 = *((_QWORD *)v61 + 5);
    *((_QWORD *)v61 + 5) = v62;
    if ( v63 )
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v63 + 8LL))(v63);
    v64 = (apac__APACAncillaryFrameDecoder *)operator new(0x40u);
    apac::APACAncillaryFrameDecoder::APACAncillaryFrameDecoder(v64, (apac *)((char *)v89 + 48));
    v65 = *((_QWORD *)this + 56);
    *((_QWORD *)this + 56) = v64;
    if ( v65 )
    {
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v65 + 8LL))(v65);
      v64 = *((apac__APACAncillaryFrameDecoder **)this + 56);
    }
    v31 = (*(__int64 (__fastcall **)(apac__APACAncillaryFrameDecoder *, char *))(*(_QWORD *)v64 + 16LL))(v64, v103);
    if ( !v31 )
    {
      v66 = (APACExtFrameDecoder *)operator new(0x18u);
      APACExtFrameDecoder::APACExtFrameDecoder(v66, v89);
      v68 = *((void ****)this + 57);
      *((_QWORD *)this + 57) = v66;
      if ( v68 )
      {
        v69 = *v68;
        v70 = v68[1];
        if ( *v68 != v70 )
        {
          do
          {
            v71 = *v69;
            if ( *v69 )
            {
              v72 = *(void (**)(void))(*(_QWORD *)v71 + 8LL);
              if ( (char *)v72 == (char *)apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder )
                operator delete(v71);
              else
                v72();
            }
            ++v69;
          }
          while ( v70 != v69 );
          v70 = *v68;
        }
        if ( v70 )
          operator delete(v70);
        operator delete(v68);
      }
      v78 = *(_QWORD *)(*((_QWORD *)this + 56) + 16LL);
      if ( v78 )
      {
        v79 = v105;
        LODWORD(v100) = 6;
        v80 = &v104;
        if ( !v105 )
          goto LABEL_137;
        do
        {
          if ( *(int *)(v79 + 32) <= 5 )
          {
            v79 = *(_QWORD *)(v79 + 24);
          }
          else
          {
            v80 = (int *)v79;
            v79 = *(_QWORD *)(v79 + 16);
          }
        }
        while ( v79 );
        if ( v80 == &v104 || v80[8] > 6 )
        {
LABEL_137:
          v101 = &v100;
          v80 = (int *)std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_emplace_hint_unique<std::piecewise_construct_t const&,std::tuple<int const&>,std::tuple<>>(
                         v103,
                         v80,
                         &unk_30F198,
                         &v101,
                         &v99);
          v78 = *(_QWORD *)(*((_QWORD *)this + 56) + 16LL);
        }
        v81 = *(_QWORD *)(v78 + 8);
        v82 = (_QWORD *)operator new(0x10u);
        *v82 = off_42AEF8;
        v82[1] = v81;
        v83 = *((_QWORD *)v80 + 5);
        *((_QWORD *)v80 + 5) = v82;
        if ( v83 )
          (*(void (__fastcall **)(__int64))(*(_QWORD *)v83 + 8LL))(v83);
      }
      v84 = *((_DWORD **)v89 + 32);
      if ( *v84 )
      {
        v85 = (unsigned __int16)*v84;
        if ( *v84 == 0x10000 )
        {
          v67.n64_u32[0] = v84[1];
          v85 = (unsigned __int8)vaddv_s8(vcnt_s8(v67));
        }
      }
      else
      {
        v85 = v84[2];
      }
      v73 = *((_QWORD **)this + 52);
      v86 = *((_QWORD **)this + 53);
      v87 = v86 - v73;
      if ( v85 > v87 )
      {
        std::vector<float *>::_M_default_append((char *)this + 416, v85 - v87);
        v73 = *((_QWORD **)this + 52);
        v86 = *((_QWORD **)this + 53);
      }
      else if ( v85 < v87 )
      {
        v88 = &v73[v85];
        if ( v86 != v88 )
        {
          v86 = v88;
          *((_QWORD *)this + 53) = v88;
        }
      }
      while ( v73 != v86 )
        *v73++ = 0;
    }
  }
LABEL_118:
  v41 = v105;
LABEL_119:
  std::_Rb_tree<int,std::pair<int const,boost::any>,std::_Select1st<std::pair<int const,boost::any>>,std::less<int>,std::allocator<std::pair<int const,boost::any>>>::_M_erase(
    v103,
    v41);
  return v31;
}


