/* ============================================================================
 * apac::LPMDCTFilter
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 8 function(s), 1636 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00079940    116  apac::LPMDCTFilter::~LPMDCTFilter() */
/* 0x0007a350    100  apac::LPMDCTFilter::~LPMDCTFilter() */
/* 0x0009c740    108  apac::LPMDCTFilter::Initialize() */
/* 0x0009c7b0    116  apac::LPMDCTFilter::Initialize(unsigned short, unsigned short) */
/* 0x000b47b0    376  apac::LPMDCTFilter::AutocorrFromPSD(float const*, float*, unsigned int) */
/* 0x000b4be0    244  apac::LPMDCTFilter::LPCToSpecEnvelope(float const*, float*, unsigned int) */
/* 0x000b4ce0    248  apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*, float*, unsigned int) */
/* 0x000b4de0    328  apac::LPMDCTFilter::MDCTToAutoCorr(float const*, float*, unsigned int, unsigned int, unsigned int) */

/* ------------------------------------------------------------------------ */
/* 0x00079940  size 116 bytes  29 insns
 *   apac::LPMDCTFilter::~LPMDCTFilter()
 *   [_ZN4apac12LPMDCTFilterD0Ev]
 *   data refs: 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42dbe8, 0x43e520
 */
void __fastcall apac::LPMDCTFilter::~LPMDCTFilter(apac__LPMDCTFilter *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  *(_QWORD *)this = off_42B710;
  *((_QWORD *)this + 2) = off_42A838;
  DFTHandle<false>::~DFTHandle((char *)this + 16);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x0007a350  size 100 bytes  25 insns
 *   apac::LPMDCTFilter::~LPMDCTFilter()
 *   apac::LPMDCTFilter::~LPMDCTFilter()
 *   [_ZN4apac12LPMDCTFilterD1Ev]
 *   [_ZN4apac12LPMDCTFilterD2Ev]
 *   data refs: 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42dbe8, 0x43e520
 */
// Alternative name is '_ZN4apac12LPMDCTFilterD2Ev'
void __fastcall apac::LPMDCTFilter::~LPMDCTFilter(apac__LPMDCTFilter *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  *(_QWORD *)this = off_42B710;
  *((_QWORD *)this + 2) = off_42A838;
  DFTHandle<false>::~DFTHandle((char *)this + 16);
}


/* ------------------------------------------------------------------------ */
/* 0x0009c740  size 108 bytes  27 insns
 *   apac::LPMDCTFilter::Initialize()
 *   [_ZN4apac12LPMDCTFilter10InitializeEv]
 *   data refs: 0x42ce00, 0x43e520
 */
float __fastcall apac::LPMDCTFilter::Initialize(apac__LPMDCTFilter *this)
{
  int v2; // s1
  float result; // s0
  __int64 v4; // [xsp+30h] [xbp+30h]
  DFTHandle<false>::Initialize((char *)this + 16, *((unsigned __int16 *)this + 4));
  LOWORD(v2) = *((_WORD *)this + 4);
  result = 2.0 / (float)v2;
  *((float *)this + 12) = result;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0009c7b0  size 116 bytes  29 insns
 *   apac::LPMDCTFilter::Initialize(unsigned short, unsigned short)
 *   [_ZN4apac12LPMDCTFilter10InitializeEtt]
 *   data refs: 0x42ce00, 0x43e520
 */
float __fastcall apac::LPMDCTFilter::Initialize(apac__LPMDCTFilter *this, unsigned __int16 v, unsigned __int16 v2)
{
  int v4; // s1
  float result; // s0
  __int64 v6; // [xsp+30h] [xbp+30h]
  *((_WORD *)this + 4) = v;
  *((_WORD *)this + 5) = v2;
  DFTHandle<false>::Initialize((char *)this + 16, v);
  LOWORD(v4) = *((_WORD *)this + 4);
  result = 2.0 / (float)v4;
  *((float *)this + 12) = result;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b47b0  size 376 bytes  94 insns
 *   apac::LPMDCTFilter::AutocorrFromPSD(float const*, float*, unsigned int)
 *   [_ZN4apac12LPMDCTFilter15AutocorrFromPSDEPKfPfj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPMDCTFilter::AutocorrFromPSD(
        apac__LPMDCTFilter *this,
        const float *a,
        float *buf,
        unsigned int n)
{
  float *v6; // x4
  size_t v9; // x3
  int v10; // w0
  float *v11; // x6
  __int64 v12; // x2
  float *v13; // x1
  __int64 v14; // x0
  float *v15; // x4
  __int64 v16; // x3
  __int64 v17; // x1
  __int64 v18; // x4
  float v20[4]; // [xsp+0h] [xbp+0h] BYREF
  DSPSplitComplex v21; // [xsp+58h] [xbp+58h] BYREF
  __int64 v22; // [xsp+68h] [xbp+68h]
  __int64 v23; // [xsp+70h] [xbp+70h]
  v6 = &v20[-3072];
  v9 = 4LL * *((unsigned __int16 *)this + 4);
  v10 = v9 + 15;
  v11 = (float *)((char *)&v20[-3072] - ((v9 + 15) & 0x7F000));
  if ( &v20[-3072] != v11 )
  {
    do
    {
      v6 -= 1024;
      *(_QWORD *)v6 = 0;
    }
    while ( v6 != v11 );
  }
  *(_QWORD *)((char *)v6 - (((v9 + 15) & 0x7FFF0) - ((v9 + 15) & 0x7F000))) = 0;
  v12 = *(_QWORD *)&v10 & 0x7F000LL;
  v13 = &v20[-3072];
  v14 = *(_QWORD *)&v10 & 0x7FFF0LL;
  v15 = (float *)((char *)&v20[-3072] - v12);
  if ( &v20[-3072] != v15 )
  {
    do
    {
      v13 -= 1024;
      *(_QWORD *)v13 = 0;
    }
    while ( v13 != v15 );
  }
  *(_QWORD *)((char *)v13 - (v14 - v12)) = 0;
  memset(v20, 0, v9);
  vDSP_vsq(a, 1, v20, 1, n);
  v16 = *((_WORD *)this + 4) >> 1;
  v21.realp = v20;
  v21.imagp = &v20[v16];
  *v21.imagp = 0.0;
  MultiRadixRealFFT::RealOutOfPlaceInverseTransform((apac__LPMDCTFilter *)((char *)this + 16), &v21, v20);
  v17 = 0;
  do
  {
    LODWORD(v18) = 2 * v17;
    if ( *((unsigned __int16 *)this + 4) / n == 4 )
      v18 = (unsigned int)v18;
    else
      v18 = (unsigned int)v17;
    buf[v17++] = v20[v18];
  }
  while ( *((unsigned __int16 *)this + 5) >= (unsigned int)v17 );
  return v22 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4be0  size 244 bytes  61 insns
 *   apac::LPMDCTFilter::LPCToSpecEnvelope(float const*, float*, unsigned int)
 *   [_ZN4apac12LPMDCTFilter17LPCToSpecEnvelopeEPKfPfj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPMDCTFilter::LPCToSpecEnvelope(
        apac__LPMDCTFilter *this,
        const float *a,
        float *buf,
        unsigned int n)
{
  _QWORD *v6; // x4
  size_t v9; // x2
  __int64 v10; // x1
  _QWORD *v11; // x3
  void *v12; // x0
  void *v13; // x0
  _QWORD v15[10]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v16; // [xsp+50h] [xbp+50h]
  v6 = &v15[-1536];
  v9 = 4LL * *((unsigned __int16 *)this + 4);
  v10 = ((_DWORD)v9 + 15) & 0x7F000;
  v11 = (_QWORD *)((char *)&v15[-1536] - v10);
  if ( &v15[-1536] != v11 )
  {
    do
    {
      v6 -= 512;
      *v6 = 0;
    }
    while ( v6 != v11 );
  }
  *(_QWORD *)((char *)v6 - ((((_DWORD)v9 + 15) & 0x7FFF0) - v10)) = 0;
  v12 = memset(v15, 0, v9);
  v13 = memcpy(v12, a, 4 * (*((unsigned __int16 *)this + 5) + 1LL));
  (*(void (__fastcall **)(apac__LPMDCTFilter *, void *, float *, _QWORD))(*(_QWORD *)this + 48LL))(this, v13, buf, n);
  return v15[9] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4ce0  size 248 bytes  62 insns
 *   apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*, float*, unsigned int)
 *   [_ZN4apac12LPMDCTFilter23LPImpulseToSpecEnvelopeEPKfPfj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPMDCTFilter::LPImpulseToSpecEnvelope(
        apac__LPMDCTFilter *this,
        const float *a,
        float *buf,
        unsigned int n)
{
  _QWORD *v5; // x6
  unsigned __int16 v6; // w0
  int v8; // w5
  __int64 v9; // x2
  __int64 v10; // x5
  _QWORD *v11; // x3
  _QWORD v13[2]; // [xsp+0h] [xbp+0h] BYREF
  DSPSplitComplex v14; // [xsp+38h] [xbp+38h] BYREF
  __int64 v15; // [xsp+48h] [xbp+48h]
  __int64 v16; // [xsp+50h] [xbp+50h]
  v5 = &v13[-1536];
  v6 = *((_WORD *)this + 4);
  v8 = 4 * v6 + 15;
  v9 = *(_QWORD *)&v8 & 0x7F000LL;
  v10 = *(_QWORD *)&v8 & 0x7FFF0LL;
  v11 = (_QWORD *)((char *)&v13[-1536] - v9);
  if ( &v13[-1536] != v11 )
  {
    do
    {
      v5 -= 512;
      *v5 = 0;
    }
    while ( v5 != v11 );
  }
  *(_QWORD *)((char *)v5 - (v10 - v9)) = 0;
  v14.realp = (float *)v13;
  v14.imagp = (float *)v13 + (v6 >> 1);
  MultiRadixRealFFT::RealOutOfPlaceForwardTransform((apac__LPMDCTFilter *)((char *)this + 16), a, &v14);
  *v14.imagp = 0.0;
  vDSP_vdist(v14.realp, (*((unsigned __int16 *)this + 4) / n == 4) + 1LL, v14.imagp);
  return v15 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4de0  size 328 bytes  82 insns
 *   apac::LPMDCTFilter::MDCTToAutoCorr(float const*, float*, unsigned int, unsigned int, unsigned int)
 *   [_ZN4apac12LPMDCTFilter14MDCTToAutoCorrEPKfPfjjj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPMDCTFilter::MDCTToAutoCorr(
        apac__LPMDCTFilter *this,
        const float *a,
        float *buf,
        unsigned int n,
        unsigned int n2,
        unsigned int n3)
{
  _QWORD *v7; // x6
  int v13; // w0
  __int64 v14; // x1
  __int64 v15; // x0
  _QWORD *v16; // x2
  int v17; // w21
  int v18; // w20
  const float *v19; // x1
  _QWORD v21[14]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v22; // [xsp+70h] [xbp+70h]
  v7 = &v21[-1536];
  v13 = 4 * *((unsigned __int16 *)this + 4) + 15;
  v14 = *(_QWORD *)&v13 & 0x7F000LL;
  v15 = *(_QWORD *)&v13 & 0x7FFF0LL;
  v16 = (_QWORD *)((char *)&v21[-1536] - v14);
  if ( &v21[-1536] != v16 )
  {
    do
    {
      v7 -= 512;
      *v7 = 0;
    }
    while ( v7 != v16 );
  }
  *(_QWORD *)((char *)v7 - (v15 - v14)) = 0;
  memset(buf, 0, 4 * (*((unsigned __int16 *)this + 5) + 1LL));
  if ( n2 )
  {
    v17 = 0;
    v18 = 0;
    do
    {
      v19 = &a[v17];
      ++v18;
      v17 += n;
      (*(void (__fastcall **)(apac__LPMDCTFilter *, const float *, _QWORD *, _QWORD))(*(_QWORD *)this + 56LL))(
        this,
        v19,
        v21,
        n3);
      vDSP_vadd(v21, 1, buf, 1, buf, 1, *((unsigned __int16 *)this + 5) + 1LL);
    }
    while ( n2 != v18 );
  }
  return v21[13] ^ _stack_chk_guard;
}


