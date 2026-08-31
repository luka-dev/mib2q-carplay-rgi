/* ============================================================================
 * APACASPDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 1172 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00072000    296  APACASPDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&) */
/* 0x00072130    416  APACASPDecoder::DecodeOneFrame(TBitstreamReader<unsigned int>&) */
/* 0x000722d0    228  APACASPDecoder::Initialize(apac::GlobalConfig const&) */
/* 0x000723c0    112  APACASPDecoder::~APACASPDecoder() */
/* 0x00072430    120  APACASPDecoder::~APACASPDecoder() */

/* ------------------------------------------------------------------------ */
/* 0x00072000  size 296 bytes  74 insns
 *   APACASPDecoder::DecodeFrame(unsigned char const*, unsigned int, apac::FrameOutRecord&)
 *   [_ZN14APACASPDecoder11DecodeFrameEPKhjRN4apac14FrameOutRecordE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACASPDecoder::DecodeFrame(
        APACASPDecoder *this,
        const unsigned __int8 *a,
        unsigned int n,
        apac__FrameOutRecord *a2)
{
  __int64 v4; // x23
  unsigned int v7; // w23
  __int64 v8; // x2
  __int64 result; // x0
  unsigned int v10; // w0
  __int64 v11; // [xsp+30h] [xbp+30h]
  TBitstreamReader_u32 v12; // [xsp+40h] [xbp+40h] BYREF
  __int64 v13; // [xsp+58h] [xbp+58h]
  __int64 v14; // [xsp+60h] [xbp+60h]
  TBitstreamReader<unsigned int>::TBitstreamReader(&v12, a, n);
  if ( *((_QWORD *)this + 49) == *((_QWORD *)this + 50) )
    goto LABEL_12;
  v11 = v4;
  v12.bitsLeft -= 2;
  v7 = v12.cache >> 30;
  if ( v12.bitsLeft < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(&v12, v12.cur);
    v7 |= v12.cache >> (LOBYTE(v12.bitsLeft) + 32);
    v10 = v12.cache << -LOBYTE(v12.bitsLeft);
    if ( v12.bitsLeft == -32 )
      v10 = 0;
    v12.cur += 4;
    v12.cache = v10;
    v12.bitsLeft += 32;
  }
  else
  {
    v12.cache *= 4;
  }
  if ( v7 == 2 )
  {
    v4 = v11;
LABEL_12:
    result = 1;
    goto LABEL_8;
  }
  v8 = *(_QWORD *)this;
  *((_DWORD *)this + 4) = v7;
  result = (*(__int64 (__fastcall **)(APACASPDecoder *, TBitstreamReader_u32 *))(v8 + 64))(this, &v12);
  if ( !(_DWORD)result )
    result = (*(__int64 (__fastcall **)(_QWORD, char *, apac__FrameOutRecord *))(**((_QWORD **)this + 55) + 32LL))(
               *((_QWORD *)this + 55),
               (char *)this + 392,
               a2);
  v4 = v11;
LABEL_8:
  if ( v13 != _stack_chk_guard )
    v11 = v4;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00072130  size 416 bytes  104 insns
 *   APACASPDecoder::DecodeOneFrame(TBitstreamReader<unsigned int>&)
 *   [_ZN14APACASPDecoder14DecodeOneFrameER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACASPDecoder::DecodeOneFrame(APACASPDecoder *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x25
  __int64 v3; // x26
  __int64 v4; // x27
  __int64 v5; // x28
  int *v8; // x19
  __int64 v9; // x2
  __int64 v10; // x20
  __int64 v11; // x0
  unsigned int v12; // w26
  __int64 result; // x0
  unsigned int v14; // w27
  __int64 v15; // x22
  void (__fastcall *v16)(__int64, _QWORD, _QWORD, __int64, __int64, _QWORD); // x24
  __int64 v17; // x28
  unsigned int v18; // w0
  __int64 v19; // x1
  __int64 v20; // x20
  unsigned int v21; // w0
  __int64 v22; // [xsp+40h] [xbp+40h]
  __int64 v23; // [xsp+48h] [xbp+48h]
  __int64 v24; // [xsp+50h] [xbp+50h]
  __int64 v25; // [xsp+58h] [xbp+58h]
  int *v26; // [xsp+68h] [xbp+68h]
  __int64 v27; // [xsp+78h] [xbp+78h]
  __int64 v28; // [xsp+80h] [xbp+80h]
  v8 = *((int **)this + 49);
  v26 = *((int **)this + 50);
  if ( v26 == v8 )
  {
    result = 1;
  }
  else
  {
    v23 = v3;
    v24 = v4;
    v25 = v5;
    v9 = *((unsigned int *)this + 4);
    do
    {
      v10 = *((_QWORD *)v8 + 1);
      v11 = *(_QWORD *)(v10 + 16);
      if ( v11 )
      {
        v12 = *v8;
        result = (*(__int64 (__fastcall **)(__int64, TBitstreamReader_u32 *, __int64, char *))(*(_QWORD *)v11 + 32LL))(
                   v11,
                   bs,
                   v9,
                   (char *)this + 416);
        if ( (_DWORD)result )
          goto LABEL_9;
        v14 = apac::ConvertToAudioDataType(*(_DWORD *)(*((_QWORD *)v8 + 1) + 8LL));
        v15 = *((_QWORD *)this + 55);
        v16 = *(void (__fastcall **)(__int64, _QWORD, _QWORD, __int64, __int64, _QWORD))(*(_QWORD *)v15 + 24LL);
        v17 = *((_QWORD *)this + 52);
        v18 = (*(__int64 (__fastcall **)(_QWORD))(**(_QWORD **)(v10 + 16) + 64LL))(*(_QWORD *)(v10 + 16));
        v19 = *(_QWORD *)(v10 + 16);
        v20 = v17 + 8LL * v18;
        v21 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v19 + 56LL))(v19);
        v16(v15, v14, v12, v17, v20, v21);
        v9 = *((unsigned int *)this + 4);
      }
      v8 += 4;
    }
    while ( v26 != v8 );
    result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, __int64, _QWORD))(**((_QWORD **)this + 56) + 24LL))(
               *((_QWORD *)this + 56),
               bs,
               *((_QWORD *)this + 55) + 8LL,
               (unsigned int)v9);
    if ( !(_DWORD)result )
      return APACExtFrameDecoder::DecodeFrame(*((APACExtFrameDecoder **)this + 57), bs);
LABEL_9:
    v3 = v23;
    v4 = v24;
    v5 = v25;
  }
  if ( v27 != _stack_chk_guard )
  {
    v22 = v2;
    v23 = v3;
    v24 = v4;
    v25 = v5;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000722d0  size 228 bytes  57 insns
 *   APACASPDecoder::Initialize(apac::GlobalConfig const&)
 *   [_ZN14APACASPDecoder10InitializeERKN4apac12GlobalConfigE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACASPDecoder::Initialize(APACASPDecoder *this, const apac__GlobalConfig *a)
{
  unsigned int v4; // w20
  int8x8_t v5; // d0
  _DWORD *v6; // x0
  int v7; // w1
  __int64 v8; // x0
  __int64 v9; // x2
  __int64 v10; // x3
  unsigned __int64 v11; // x1
  bool v12; // cf
  __int64 v14; // x1
  __int64 v15; // [xsp+40h] [xbp+40h]
  v4 = APACDecoder::Initialize(this, a);
  if ( v4 )
    return v4;
  v6 = *((_DWORD **)a + 32);
  if ( *v6 )
  {
    v7 = (unsigned __int16)*v6;
    if ( *v6 == 0x10000 )
    {
      v5.n64_u32[0] = v6[1];
      v8 = *((_QWORD *)this + 58);
      v9 = *((_QWORD *)this + 59);
      v10 = v9 - v8;
      v11 = (unsigned __int8)vaddv_s8(vcnt_s8(v5)) << 11;
      v12 = v11 >= v9 - v8;
      if ( v11 > v9 - v8 )
      {
LABEL_5:
        std::vector<unsigned char>::_M_default_append((_DWORD)this + 464, v11 - v10);
        return v4;
      }
      goto LABEL_9;
    }
  }
  else
  {
    v7 = v6[2];
  }
  v8 = *((_QWORD *)this + 58);
  v9 = *((_QWORD *)this + 59);
  v11 = (unsigned int)(v7 << 11);
  v10 = v9 - v8;
  v12 = v11 >= v9 - v8;
  if ( v11 > v9 - v8 )
    goto LABEL_5;
LABEL_9:
  if ( !v12 )
  {
    v14 = v8 + v11;
    if ( v9 != v14 )
      *((_QWORD *)this + 59) = v14;
  }
  return v4;
}


/* ------------------------------------------------------------------------ */
/* 0x000723c0  size 112 bytes  28 insns
 *   APACASPDecoder::~APACASPDecoder()
 *   APACASPDecoder::~APACASPDecoder()
 *   [_ZN14APACASPDecoderD2Ev]
 *   [_ZN14APACASPDecoderD1Ev]
 *   data refs: 0x42a278, 0x42ce00, 0x42ddd8, 0x43e520
 */
// Alternative name is '_ZN14APACASPDecoderD1Ev'
void __fastcall APACASPDecoder::~APACASPDecoder(APACASPDecoder *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((void **)this + 58);
  *(_QWORD *)this = off_42A288;
  if ( v2 )
    operator delete(v2);
  APACDecoder::~APACDecoder(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00072430  size 120 bytes  30 insns
 *   APACASPDecoder::~APACASPDecoder()
 *   [_ZN14APACASPDecoderD0Ev]
 *   data refs: 0x42a278, 0x42ce00, 0x42ddd8, 0x43e520
 */
void __fastcall APACASPDecoder::~APACASPDecoder(APACASPDecoder *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((void **)this + 58);
  *(_QWORD *)this = off_42A288;
  if ( v2 )
    operator delete(v2);
  APACDecoder::~APACDecoder(this);
  operator delete(this);
}


