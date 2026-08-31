/* ============================================================================
 * APACLRVQDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 15 function(s), 8904 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00070c6c     68  APACLRVQDecoder::LRVQLPC::Initialize(unsigned short) */
/* 0x00070cb0     68  APACLRVQDecoder::LRVQLPC::Initialize() */
/* 0x0009a950    184  APACLRVQDecoder::dequantizeLrvqGain(unsigned short, unsigned short, float*) */
/* 0x0009aa10   1232  APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> const&, apac::aligned_array<float, 1024ul>&) */
/* 0x0009aee0    536  APACLRVQDecoder::ltpToMdctSpectralEnvelope(unsigned short, unsigned short, apac::aligned_array<float, 1024ul>&) */
/* 0x0009b100   3264  APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 256ul>&, apac::aligned_array<short, 64ul>&, unsigned short*) */
/* 0x0009bdc0    408  APACLRVQDecoder::SpectrumDeserializeSCE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACIndividualChannelStream&) */
/* 0x0009bf60    644  APACLRVQDecoder::SpectrumDeserializeCPE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACICSInfo&, APACIndividualChannelStream&, APACIndividualChannelStream&) */
/* 0x0009c1f0   1260  APACLRVQDecoder::SpectrumDeserializeLFE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACIndividualChannelStream&) */
/* 0x0009c830    100  APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG() */
/* 0x0009c8a0    116  APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG() */
/* 0x0009c920    100  APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP() */
/* 0x0009c990    116  APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP() */
/* 0x0009ca10    416  APACLRVQDecoder::LRVQLPC::~LRVQLPC() */
/* 0x0009cbb0    392  APACLRVQDecoder::LRVQLPC::~LRVQLPC() */

/* ------------------------------------------------------------------------ */
/* 0x00070c6c  size 68 bytes  17 insns
 *   APACLRVQDecoder::LRVQLPC::Initialize(unsigned short)
 *   [_ZN15APACLRVQDecoder7LRVQLPC10InitializeEt]
 *   data refs: 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
void __fastcall __noreturn APACLRVQDecoder::LRVQLPC::Initialize(APACLRVQDecoder__LRVQLPC *this, unsigned __int16 v)
{
  _DWORD *exception; // x0
  __int64 v3; // [xsp+20h] [xbp+20h]
  exception = __cxa_allocate_exception(4u);
  *exception = 4;
  __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
}


/* ------------------------------------------------------------------------ */
/* 0x00070cb0  size 68 bytes  17 insns
 *   APACLRVQDecoder::LRVQLPC::Initialize()
 *   [_ZN15APACLRVQDecoder7LRVQLPC10InitializeEv]
 *   data refs: 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
void __fastcall __noreturn APACLRVQDecoder::LRVQLPC::Initialize(APACLRVQDecoder__LRVQLPC *this)
{
  _DWORD *exception; // x0
  __int64 v2; // [xsp+20h] [xbp+20h]
  exception = __cxa_allocate_exception(4u);
  *exception = 4;
  __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
}


/* ------------------------------------------------------------------------ */
/* 0x0009a950  size 184 bytes  45 insns
 *   APACLRVQDecoder::dequantizeLrvqGain(unsigned short, unsigned short, float*)
 *   [_ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf]
 *   data refs: 0x42ce00, 0x43e520
 */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall APACLRVQDecoder::dequantizeLrvqGain(
        APACLRVQDecoder *this,
        unsigned __int16 v,
        unsigned __int16 v2,
        float *buf)
{
  float *v4; // x20
  float v5; // s1
  float v6; // s0
  __int64 v8; // [xsp+30h] [xbp+30h]
  v4 = (float *)v2;
  v5 = (float)(unsigned __int16)this / 511.0;
  if ( v == 1 )
    v6 = powf(10.0, v5 + v5) * 10.0;
  else
    v6 = 10.0 / powf(10.0, v5 * 4.0);
  *v4 = v6 * 65536.0;
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009aa10  size 1232 bytes  305 insns
 *   APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> const&, apac::aligned_array<float, 1024ul>&)
 *   [_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE]
 *   data refs: 0x79810, 0x317e90, 0x31be90, 0x31de90, 0x31fe90, 0x323e90, 0x327e90, 0x32be90, 0x42a828, 0x42a848, 0x42b0b0, 0x42b158, 0x42b700, 0x42b750, 0x42b798, 0x42cb80, 0x42ce00, 0x42d070, 0x42d128, 0x42d138, 0x42d2a8, 0x42d5b0, 0x42d7d8, 0x42d8c8
 */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall APACLRVQDecoder::lsfsToMdctSpectralEnvelope(
        APACLRVQDecoder *this,
        bool flag,
        const unsigned __int16 *a,
        float *buf)
{
  bool v4; // zf
  unsigned __int16 *v5; // x20
  unsigned __int16 v7; // w1
  unsigned __int16 v8; // w3
  char *v9; // x2
  __int64 v10; // x0
  __int64 *v11; // x0
  int v12; // s1
  const unsigned __int16 *v13; // x0
  const unsigned __int16 *v14; // x19
  _QWORD **v15; // x19
  _QWORD **v16; // x21
  _QWORD *v17; // x20
  void *v18; // x0
  void *v19; // x0
  void *v20; // x19
  void (__fastcall *v21)(apac__Autocorr *); // x1
  void *v22; // x0
  unsigned __int16 v24; // w0
  __int64 v25; // x1
  char *v26; // x4
  void (__fastcall **v27)(apac::LPMDCTFilter *__hidden); // [xsp+58h] [xbp+58h] BYREF
  int v28; // [xsp+60h] [xbp+60h]
  void (__fastcall **v29)(MultiRadixRealFFT *__hidden); // [xsp+68h] [xbp+68h] BYREF
  int v30; // [xsp+70h] [xbp+70h]
  __int64 v31; // [xsp+78h] [xbp+78h]
  __int64 v32; // [xsp+80h] [xbp+80h]
  float v33; // [xsp+88h] [xbp+88h]
  _QWORD v34[2]; // [xsp+90h] [xbp+90h] BYREF
  __int16 v35; // [xsp+A0h] [xbp+A0h]
  void *v36; // [xsp+A8h] [xbp+A8h]
  __int64 v37; // [xsp+B0h] [xbp+B0h]
  __int64 v38; // [xsp+B8h] [xbp+B8h]
  void *v39; // [xsp+C0h] [xbp+C0h]
  __int64 v40; // [xsp+C8h] [xbp+C8h]
  __int64 v41; // [xsp+D0h] [xbp+D0h]
  void *v42; // [xsp+D8h] [xbp+D8h]
  _QWORD **v43; // [xsp+E0h] [xbp+E0h]
  _QWORD **v44; // [xsp+E8h] [xbp+E8h]
  __int64 v45; // [xsp+F0h] [xbp+F0h]
  int v46; // [xsp+F8h] [xbp+F8h]
  __int16 v47; // [xsp+FCh] [xbp+FCh]
  __int64 v48; // [xsp+100h] [xbp+100h]
  void *v49; // [xsp+108h] [xbp+108h]
  __int64 v50; // [xsp+110h] [xbp+110h]
  __int64 v51; // [xsp+118h] [xbp+118h]
  void *v52; // [xsp+120h] [xbp+120h]
  __int64 v53; // [xsp+128h] [xbp+128h]
  __int64 v54; // [xsp+130h] [xbp+130h]
  void *v55; // [xsp+138h] [xbp+138h]
  __int64 v56; // [xsp+140h] [xbp+140h]
  __int64 v57; // [xsp+148h] [xbp+148h]
  void *v58; // [xsp+150h] [xbp+150h]
  __int64 v59; // [xsp+158h] [xbp+158h]
  __int64 v60; // [xsp+160h] [xbp+160h]
  void *v61; // [xsp+168h] [xbp+168h]
  __int64 v62; // [xsp+170h] [xbp+170h]
  __int64 v63; // [xsp+178h] [xbp+178h]
  _QWORD v64[2]; // [xsp+180h] [xbp+180h] BYREF
  _QWORD v65[2]; // [xsp+190h] [xbp+190h] BYREF
  _QWORD v66[4]; // [xsp+1A0h] [xbp+1A0h] BYREF
  _QWORD v67[8]; // [xsp+1C0h] [xbp+1C0h] BYREF
  int v68; // [xsp+200h] [xbp+200h]
  _QWORD v69[8]; // [xsp+210h] [xbp+210h] BYREF
  int v70; // [xsp+250h] [xbp+250h]
  int v71; // [xsp+254h] [xbp+254h] BYREF
  __int64 v72; // [xsp+A10h] [xbp+A10h] BYREF
  __int64 v73; // [xsp+A20h] [xbp+A20h]
  v4 = (unsigned __int8)this == 0;
  v5 = (unsigned __int16 *)flag;
  v64[0] = 0;
  v64[1] = 0;
  v7 = *(_WORD *)(flag + 4LL);
  v8 = *v5;
  v65[0] = 0;
  v65[1] = 0;
  memset(v66, 0, sizeof(v66));
  v9 = (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB21 + 16 * v7;
  v10 = 32LL * v8;
  if ( v4 )
  {
    vDSP_vadd((char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB11 + v10, 1, v9, 1, v64, 1, 4);
    vDSP_vadd(
      (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB11 + 32 * *v5 + 16,
      1,
      (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB22 + 16 * v5[3],
      1,
      v65,
      1,
      4);
    v24 = v5[1];
    v26 = (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB12;
    v25 = 32LL * v5[4];
  }
  else
  {
    vDSP_vadd((char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB11_SHORT + v10, 1, v9, 1, v64, 1, 4);
    vDSP_vadd(
      (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB11_SHORT + 32 * *v5 + 16,
      1,
      (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB22 + 16 * v5[3],
      1,
      v65,
      1,
      4);
    v24 = v5[1];
    v25 = 32LL * v5[4];
    v26 = (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB12_SHORT;
  }
  vDSP_vadd(&v26[32 * v24], 1, (char *)&LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB23 + v25, 1, v66, 1, 8);
  v34[0] = off_42B0C0;
  v34[1] = 0x100400010LL;
  v35 = 0;
  v36 = nullptr;
  v37 = 0;
  v38 = 0;
  v39 = nullptr;
  v40 = 0;
  v41 = 0;
  v42 = nullptr;
  v43 = nullptr;
  v44 = nullptr;
  v45 = 0;
  v46 = 16;
  v47 = 0;
  v48 = 0x4248000046BB8000LL;
  v49 = nullptr;
  v50 = 0;
  v51 = 0;
  v52 = nullptr;
  v53 = 0;
  v54 = 0;
  v55 = nullptr;
  v56 = 0;
  v57 = 0;
  v58 = nullptr;
  v59 = 0;
  v60 = 0;
  v61 = nullptr;
  v62 = 0;
  v63 = 0;
  apac::LSFTools::LSF_To_LPC((apac__LSFTools *)v34, (float *)v64, (float *)v67);
  v11 = (__int64 *)&v71;
  do
  {
    *(_DWORD *)v11 = 0;
    v11 = (__int64 *)((char *)v11 + 4);
  }
  while ( v11 != &v72 );
  v69[0] = v67[0];
  v69[1] = v67[1];
  v69[2] = v67[2];
  v69[3] = v67[3];
  v69[4] = v67[4];
  v69[5] = v67[5];
  v69[6] = v67[6];
  v69[7] = v67[7];
  v70 = v68;
  v27 = off_42B168;
  v28 = 1049088;
  v29 = off_42A838;
  v30 = 0;
  v31 = 0;
  v32 = 0;
  DFTHandle<false>::Initialize(&v29, 512);
  LOWORD(v12) = v28;
  v33 = 2.0 / (float)v12;
  apac::LPMDCTFilter::LPImpulseToSpecEnvelope((apac__LPMDCTFilter *)&v27, (const float *)v69, (float *)a, 0x100u);
  v13 = a + 512;
  v14 = a + 2048;
  do
  {
    *(_DWORD *)v13 = 0;
    v13 += 2;
  }
  while ( v14 != v13 );
  v29 = off_42A838;
  v27 = off_42B710;
  DFTHandle<false>::~DFTHandle(&v29);
  v34[0] = off_42B7A8;
  if ( v61 )
    operator delete(v61);
  if ( v58 )
    operator delete(v58);
  if ( v55 )
    operator delete(v55);
  if ( v52 )
    operator delete(v52);
  if ( v49 )
    operator delete(v49);
  v15 = v43;
  v16 = v44;
  v34[0] = &unk_42B760;
  if ( v43 != v44 )
  {
    do
    {
      v17 = *v15;
      if ( *v15 )
      {
        v18 = (void *)v17[6];
        if ( v18 )
          operator delete(v18);
        v19 = (void *)v17[2];
        if ( v19 )
          operator delete(v19);
        operator delete(v17);
      }
      ++v15;
    }
    while ( v16 != v15 );
    v16 = v43;
  }
  if ( v16 )
    operator delete(v16);
  v20 = v42;
  if ( v42 )
  {
    v21 = *(void (__fastcall **)(apac__Autocorr *))(*(_QWORD *)v42 + 8LL);
    if ( v21 == apac::Autocorr::~Autocorr )
    {
      v22 = *((void **)v42 + 2);
      *(_QWORD *)v42 = off_42A858;
      if ( v22 )
        operator delete(v22);
      operator delete(v20);
    }
    else
    {
      v21((apac__Autocorr *)v42);
    }
  }
  if ( v39 )
    operator delete(v39);
  if ( v36 )
    operator delete(v36);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009aee0  size 536 bytes  132 insns
 *   APACLRVQDecoder::ltpToMdctSpectralEnvelope(unsigned short, unsigned short, apac::aligned_array<float, 1024ul>&)
 *   [_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE]
 *   data refs: 0x42a828, 0x42b1a8, 0x42b700, 0x42ce00, 0x42d2a8, 0x42d5e8, 0x42dbe8, 0x43e520
 */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall APACLRVQDecoder::ltpToMdctSpectralEnvelope(
        APACLRVQDecoder *this,
        unsigned __int16 v,
        unsigned __int16 v2,
        float *buf)
{
  __int64 v4; // x29
  __int64 v5; // x30
  __int64 v6; // d8
  __int64 *v7; // x9
  float v8; // s0
  float *v9; // x21
  float *v10; // x0
  float v11; // s8
  int v12; // s1
  float *v13; // x0
  __int64 v15; // [xsp+0h] [xbp-2090h]
  __int64 v16; // [xsp+8h] [xbp-2088h]
  __int64 v17; // [xsp+38h] [xbp-2058h]
  float v18; // [xsp+44h] [xbp-204Ch] BYREF
  void (__fastcall **v19)(apac::LPMDCTFilter *__hidden); // [xsp+48h] [xbp-2048h] BYREF
  int v20; // [xsp+50h] [xbp-2040h]
  void (__fastcall **v21)(MultiRadixRealFFT *__hidden); // [xsp+58h] [xbp-2038h] BYREF
  int v22; // [xsp+60h] [xbp-2030h]
  __int64 v23; // [xsp+68h] [xbp-2028h]
  __int64 v24; // [xsp+70h] [xbp-2020h]
  float v25; // [xsp+78h] [xbp-2018h]
  float v26[2048]; // [xsp+80h] [xbp-2010h] BYREF
  __int64 v27; // [xsp+2080h] [xbp-10h] BYREF
  __int64 v28; // [xsp+2090h] [xbp+0h] BYREF

  v7 = &v28 - 2560;
  v7[512] = 0;
  *v7 = 0;
  v15 = v4;
  v16 = v5;
  v17 = v6;
  v8 = (float)(unsigned __int16)this / 127.0;
  v9 = (float *)v2;
  v10 = v26;
  v11 = (float)(v8 * 0.74) + 0.25;
  do
    *v10++ = 0.0;
  while ( v10 != (float *)&v27 );
  v26[0] = 1.0;
  v26[v] = -v11;
  v19 = off_42B1B8;
  v20 = 1050624;
  v21 = off_42A838;
  v22 = 0;
  v23 = 0;
  v24 = 0;
  DFTHandle<false>::Initialize(&v21, 2048);
  LOWORD(v12) = v20;
  v25 = 2.0 / (float)v12;
  apac::LPMDCTFilter::LPImpulseToSpecEnvelope((apac__LPMDCTFilter *)&v19, v26, v9, 0x100u);
  v13 = v9 + 256;
  do
    *v13++ = 0.0;
  while ( v13 != v9 + 1024 );
  v18 = 1.0 / (float)(v11 + 1.0);
  vDSP_vsmul(v9, 1, &v18, v9, 1, 256);
  v21 = off_42A838;
  v19 = off_42B710;
  DFTHandle<false>::~DFTHandle(&v21);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009b100  size 3264 bytes  811 insns
 *   APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 256ul>&, apac::aligned_array<short, 64ul>&, unsigned short*)
 *   [_ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt]
 *   data refs: 0x32fef8, 0x32fefc, 0x32ff00, 0x42b1f8, 0x42ce00, 0x43e520
 */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall APACLRVQDecoder::LrvqDeserialize(
        APACLRVQDecoder *this,
        TBitstreamReader_u32 *bs,
        bool flag,
        bool flag2,
        APACSpectralData *spectrum,
        float *buf,
        float *buf2,
        float *buf3,
        __int16 *a,
        unsigned __int16 *v)
{
  _QWORD *v12; // x23
  _BOOL4 v13; // w27
  unsigned int v14; // w9
  int v15; // w10
  unsigned int v16; // w21
  __int16 *v17; // x25
  unsigned int v18; // w9
  unsigned int v19; // w28
  float *v20; // x26
  unsigned __int64 v21; // x3
  __int64 v22; // x7
  unsigned int v23; // w21
  int v24; // w28
  unsigned int v25; // w0
  int v26; // w1
  int v27; // w1
  _BYTE *v28; // x21
  unsigned int v29; // w23
  int v30; // w1
  int v31; // w0
  unsigned int v32; // w1
  const unsigned __int8 *v33; // x6
  int v34; // w5
  int v35; // w0
  unsigned int v36; // w0
  bool v37; // zf
  int v38; // w0
  unsigned int v39; // w1
  int v40; // w0
  unsigned int v41; // w28
  _BOOL4 v42; // w3
  char *v43; // x0
  __int64 v44; // x2
  __int64 v45; // x4
  int v46; // w1
  int v47; // w2
  float *v48; // x24
  _BOOL4 v49; // w21
  int v50; // w23
  __int64 v51; // x20
  TBitstreamReader_u32 *v52; // x26
  __int16 *v53; // x19
  float *v54; // x27
  unsigned int v55; // w1
  int v56; // w0
  int v57; // w28
  int v58; // w3
  unsigned int v59; // w1
  char v60; // w2
  int v61; // w10
  int v62; // w0
  __int64 v63; // x0
  int v64; // w2
  float *v65; // x0
  unsigned int v67; // w28
  int v68; // w1
  int v69; // w2
  unsigned int v70; // w0
  unsigned int v71; // w4
  int v72; // w2
  unsigned int v73; // w9
  const unsigned __int8 *v74; // x1
  int v75; // w0
  const unsigned __int8 *v76; // x1
  unsigned int v77; // w0
  int v78; // w1
  int v79; // w0
  unsigned int v80; // w0
  int v81; // w4
  const unsigned __int8 *v82; // x1
  const unsigned __int8 *v83; // x6
  int v84; // w0
  unsigned int cache; // w1
  unsigned int v86; // w28
  unsigned int v87; // w1
  int v88; // w0
  unsigned int v89; // w27
  unsigned int v90; // w2
  const unsigned __int8 *cur; // x1
  int bitsLeft; // w3
  __int16 v93; // w9
  unsigned int v94; // w1
  int v95; // w28
  unsigned int v96; // w2
  unsigned int v97; // w1
  unsigned __int64 v98; // x3
  unsigned int v99; // w24
  int v100; // w0
  unsigned int v101; // w1
  int v102; // w2
  char v103; // w2
  unsigned int v104; // w1
  int v105; // w0
  int v106; // w10
  unsigned int v107; // w1
  int v108; // w0
  unsigned int v109; // w2
  const unsigned __int8 *v110; // x1
  int v111; // w2
  unsigned int v112; // w4
  const unsigned __int8 *v113; // x0
  __int16 v114; // w10
  unsigned int v115; // w3
  int v116; // w1
  unsigned int v117; // w1
  __int64 i; // x19
  __int64 v119; // x3
  int v120; // w1
  const unsigned __int8 *v121; // x1
  unsigned int v122; // w4
  unsigned int v123; // w1
  int v124; // w0
  int v125; // w2
  const unsigned __int8 *v126; // x1
  unsigned int v127; // w0
  int v128; // w1
  int v129; // w0
  const unsigned __int8 *v130; // x1
  int v131; // w0
  unsigned int v132; // w2
  int v133; // w2
  unsigned int v134; // w3
  const unsigned __int8 *v135; // x1
  int v136; // w4
  const unsigned __int8 *v137; // x5
  unsigned int v138; // w2
  int v139; // w4
  int v140; // w3
  const unsigned __int8 *v141; // x1
  int v142; // w0
  unsigned int v143; // w1
  int v144; // w4
  int v145; // w0
  __int64 v146; // x25
  int v147; // w19
  __int64 v148; // x7
  float *v149; // x26
  __int64 v150; // x7
  __int64 v151; // x0
  _DWORD *v152; // x2
  float *v153; // x0
  int v154; // t1
  const unsigned __int8 *v155; // x1
  unsigned int v156; // w0
  int v157; // w1
  unsigned int v158; // w6
  int v159; // w0
  const unsigned __int8 *v160; // x1
  const unsigned __int8 *v161; // x1
  unsigned int v162; // w0
  int v163; // w5
  int v164; // w4
  __int64 v165; // [xsp+0h] [xbp+0h]
  __int64 v166; // [xsp+8h] [xbp+8h]
  APACSpectralData *v167; // [xsp+60h] [xbp+60h]
  int v168; // [xsp+68h] [xbp+68h]
  _BOOL4 v169; // [xsp+6Ch] [xbp+6Ch]
  __int64 v170; // [xsp+70h] [xbp+70h]
  float *v171; // [xsp+78h] [xbp+78h]
  float *v172; // [xsp+80h] [xbp+80h]
  _QWORD *v173; // [xsp+88h] [xbp+88h]
  float *v174; // [xsp+90h] [xbp+90h]
  void *v175; // [xsp+98h] [xbp+98h]
  unsigned int v176; // [xsp+A0h] [xbp+A0h]
  int v177; // [xsp+A4h] [xbp+A4h]
  void *v178; // [xsp+A8h] [xbp+A8h]
  int v179; // [xsp+B0h] [xbp+B0h]
  unsigned int v180; // [xsp+B4h] [xbp+B4h]
  _QWORD *v181; // [xsp+B8h] [xbp+B8h]
  float *v182; // [xsp+C0h] [xbp+C0h]
  float *v183; // [xsp+C8h] [xbp+C8h]
  float v184; // [xsp+DCh] [xbp+DCh] BYREF
  _BYTE v185[10]; // [xsp+E0h] [xbp+E0h] BYREF
  _BYTE v186[14]; // [xsp+EAh] [xbp+EAh] BYREF

  *(&a - 1568) = nullptr;
  v12 = (_QWORD *)flag2;
  v13 = flag;
  v167 = spectrum;
  v14 = *((_DWORD *)this + 4);
  v15 = *((_DWORD *)this + 5);
  v168 = (unsigned __int8)bs;
  v171 = buf3;
  v172 = buf;
  *((_DWORD *)this + 5) = v15 - 1;
  v16 = v14 >> 31;
  v17 = a;
  if ( ((v15 - 1) & 0x80000000) == 0 )
  {
    v18 = 2 * v14;
    *((_DWORD *)this + 4) = v18;
    *((_DWORD *)this + 5) = v15 - 10;
    v19 = v18 >> 23;
    if ( ((v15 - 10) & 0x80000000) == 0 )
    {
LABEL_3:
      *((_DWORD *)this + 4) = v18 << 9;
      goto LABEL_4;
    }
    goto LABEL_59;
  }
  TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
  v73 = *((_DWORD *)this + 4);
  v72 = *((_DWORD *)this + 5);
  v74 = *(const unsigned __int8 **)this;
  *(_QWORD *)&flag2 = (unsigned int)(v72 + 32);
  *((_DWORD *)this + 5) = flag2;
  v75 = v72 + 23;
  v76 = v74 + 4;
  *(_QWORD *)this = v76;
  LOWORD(v16) = (v73 >> (v72 + 32)) | v16;
  if ( v72 != -32 )
  {
    *((_DWORD *)this + 5) = v75;
    v18 = v73 << -(char)v72;
    *((_DWORD *)this + 4) = v18;
    v19 = v18 >> 23;
    if ( (v75 & 0x80000000) == 0 )
      goto LABEL_3;
LABEL_59:
    v76 = *(const unsigned __int8 **)this;
    goto LABEL_55;
  }
  v19 = 0;
  *((_DWORD *)this + 4) = 0;
  *((_DWORD *)this + 5) = v75;
LABEL_55:
  TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v76);
  v77 = *((_DWORD *)this + 4);
  v78 = *((_DWORD *)this + 5);
  *(_QWORD *)&flag2 = *(_QWORD *)this;
  v19 |= v77 >> (v78 + 32);
  v79 = v77 << -(char)v78;
  if ( v78 == -32 )
    v79 = 0;
  *(_QWORD *)this += 4LL;
  *((_DWORD *)this + 4) = v79;
  *((_DWORD *)this + 5) = v78 + 32;
LABEL_4:
  v20 = &v184;
  v23 = APACLRVQDecoder::dequantizeLrvqGain((APACLRVQDecoder *)v19, v16, (unsigned __int16)&v184, (float *)flag2);
  if ( v23 )
    return v23;
  v169 = 0;
  v24 = 0;
  v25 = *((_DWORD *)this + 4);
  v26 = *((_DWORD *)this + 5);
  if ( v13 )
    goto LABEL_6;
  *((_DWORD *)this + 5) = v26 - 1;
  v67 = v25 >> 31;
  if ( v26 - 1 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
    v80 = *((_DWORD *)this + 4);
    v81 = *((_DWORD *)this + 5);
    v82 = *(const unsigned __int8 **)this;
    *((_DWORD *)this + 5) = v81 + 32;
    v69 = v81 + 31;
    v83 = v82 + 4;
    *(_QWORD *)this = v82 + 4;
    v68 = v81 + 31;
    v21 = (unsigned __int8)((v80 >> (v81 + 32)) | v67) != 0;
    if ( v81 == -32 )
    {
      v84 = v168;
      *((_DWORD *)this + 4) = 0;
      if ( v84 )
      {
        v24 = 0;
        v71 = 0;
        v70 = 0;
        goto LABEL_49;
      }
      v71 = 0;
      *((_DWORD *)this + 5) = v69;
      goto LABEL_155;
    }
    v70 = v80 << -(char)v81;
    *((_DWORD *)this + 4) = v70;
  }
  else
  {
    v68 = v26 - 2;
    v21 = v67;
    v69 = v68;
    v70 = 2 * v25;
    *((_DWORD *)this + 4) = v70;
  }
  v71 = v70 >> 31;
  v24 = 0;
  if ( !v168 )
  {
    *((_DWORD *)this + 5) = v69;
    if ( (v69 & 0x80000000) == 0 )
    {
      v70 *= 2;
      LOBYTE(v125) = v71;
      *((_DWORD *)this + 4) = v70;
      v71 = v70 >> 31;
LABEL_116:
      v37 = (unsigned __int8)v125 == 0;
      v69 = v68 - 1;
      v24 = !v37;
      goto LABEL_49;
    }
    v83 = *(const unsigned __int8 **)this;
LABEL_155:
    v169 = v21;
    LODWORD(v170) = v71;
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v83);
    v162 = *((_DWORD *)this + 4);
    v163 = *((_DWORD *)this + 5);
    v21 = v169;
    v164 = v170;
    v68 = v163 + 32;
    *(_QWORD *)this += 4LL;
    *((_DWORD *)this + 5) = v163 + 32;
    v125 = (v162 >> (v163 + 32)) | v164;
    if ( v163 == -32 )
    {
      v71 = 0;
      v70 = 0;
      *((_DWORD *)this + 4) = 0;
    }
    else
    {
      v70 = v162 << -(char)v163;
      *((_DWORD *)this + 4) = v70;
      v71 = v70 >> 31;
    }
    goto LABEL_116;
  }
LABEL_49:
  *((_DWORD *)this + 5) = v69;
  if ( v69 < 0 )
  {
    v130 = *(const unsigned __int8 **)this;
    v169 = v21;
    LODWORD(v170) = v71;
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v130);
    v132 = *((_DWORD *)this + 4);
    v131 = *((_DWORD *)this + 5);
    v21 = v169;
    LOBYTE(v71) = v170 | (v132 >> (v131 + 32));
    v133 = v132 << -(char)v131;
    if ( v131 == -32 )
      v133 = 0;
    *(_QWORD *)this += 4LL;
    *((_DWORD *)this + 4) = v133;
    *((_DWORD *)this + 5) = v131 + 32;
  }
  else
  {
    *((_DWORD *)this + 4) = 2 * v70;
  }
  LODWORD(v173) = 0;
  v169 = (unsigned __int8)v71 != 0;
  if ( (_DWORD)v21 )
    goto LABEL_14;
  v25 = *((_DWORD *)this + 4);
  v26 = *((_DWORD *)this + 5);
LABEL_6:
  v27 = v26 - 9;
  *((_DWORD *)this + 5) = v27;
  LODWORD(v170) = 0;
  v28 = v185;
  v173 = v12;
  v174 = buf2;
  v29 = v25 >> 23;
  if ( v27 < 0 )
    goto LABEL_9;
  while ( 1 )
  {
    *(_WORD *)v28 = v29;
    v28 += 2;
    *((_DWORD *)this + 4) = v25 << 9;
    if ( v186 == v28 )
      break;
    while ( 1 )
    {
      v25 = *((_DWORD *)this + 4);
      v30 = *((_DWORD *)this + 5) - 9;
      *((_DWORD *)this + 5) = v30;
      v29 = v25 >> 23;
      if ( (v30 & 0x80000000) == 0 )
        break;
LABEL_9:
      TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
      v32 = *((_DWORD *)this + 4);
      v31 = *((_DWORD *)this + 5);
      v33 = *(const unsigned __int8 **)this;
      v34 = v31 + 32;
      v37 = v31 == -32;
      v35 = v32 << -(char)v31;
      if ( v37 )
        v35 = 0;
      *(_WORD *)v28 = v29 | (v32 >> v34);
      v28 += 2;
      *(_QWORD *)this = v33 + 4;
      *((_DWORD *)this + 4) = v35;
      *((_DWORD *)this + 5) = v34;
      if ( v186 == v28 )
        goto LABEL_12;
    }
  }
LABEL_12:
  v12 = v173;
  buf2 = v174;
  LODWORD(v173) = 0;
  v23 = v170;
  if ( v168 )
  {
    v107 = *((_DWORD *)this + 4);
    v108 = *((_DWORD *)this + 5) - 1;
    *((_DWORD *)this + 5) = v108;
    v109 = v107 >> 31;
    if ( v108 < 0 )
    {
      v155 = *(const unsigned __int8 **)this;
      LODWORD(v170) = v109;
      TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v155);
      v156 = *((_DWORD *)this + 4);
      v157 = *((_DWORD *)this + 5);
      v21 = (unsigned int)(v157 + 32);
      v158 = v156 >> (v157 + 32);
      v159 = v156 << -(char)v157;
      LOBYTE(v109) = v170 | v158;
      if ( v157 == -32 )
        v159 = 0;
      *(_QWORD *)this += 4LL;
      *((_DWORD *)this + 4) = v159;
      *((_DWORD *)this + 5) = v21;
    }
    else
    {
      *((_DWORD *)this + 4) = 2 * v107;
    }
    LODWORD(v173) = (unsigned __int8)v109 != 0;
  }
  v36 = APACLRVQDecoder::lsfsToMdctSpectralEnvelope(
          (APACLRVQDecoder *)(unsigned __int8)v173,
          (bool)v185,
          (const unsigned __int16 *)v167,
          (float *)v21);
  if ( v36 )
    return v36;
LABEL_14:
  if ( v168 )
    v37 = 0;
  else
    v37 = v24 == 0;
  if ( !v37 )
  {
    if ( v169 )
    {
      if ( !v168 )
        goto LABEL_44;
      goto LABEL_103;
    }
    goto LABEL_19;
  }
  v94 = *((_DWORD *)this + 4);
  v38 = *((_DWORD *)this + 5) - 1;
  *((_DWORD *)this + 5) = v38;
  v95 = v94 >> 31;
  if ( v38 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
    v134 = *((_DWORD *)this + 4);
    v135 = *(const unsigned __int8 **)this;
    v136 = *((_DWORD *)this + 5);
    v38 = v136 + 32;
    *((_DWORD *)this + 5) = v136 + 32;
    v126 = v135 + 4;
    *(_QWORD *)this = v126;
    v96 = (unsigned __int8)((v134 >> (v136 + 32)) | v95);
    v137 = v126;
    v95 = ((unsigned __int8)(v134 >> (v136 + 32)) | (unsigned __int8)v95) != 0;
    if ( v136 == -32 )
    {
      *((_DWORD *)this + 4) = 0;
      if ( !v96 )
      {
        if ( v169 )
          goto LABEL_44;
        v41 = 0;
        *((_DWORD *)this + 5) = -4;
        goto LABEL_120;
      }
      LODWORD(v98) = 0;
      v95 = 1;
      *((_DWORD *)this + 5) = -7;
      goto LABEL_131;
    }
    *((_DWORD *)this + 4) = v134 << -(char)v136;
  }
  else
  {
    v96 = v94 >> 31;
    *((_DWORD *)this + 4) = 2 * v94;
  }
  if ( v96 )
  {
    v97 = *((_DWORD *)this + 4);
    *((_DWORD *)this + 5) = v38 - 7;
    v98 = v97 >> 25;
    if ( ((v38 - 7) & 0x80000000) == 0 )
    {
      v99 = v97 >> 25;
      v100 = v38 - 17;
      v101 = v97 << 7;
      *((_DWORD *)this + 4) = v101;
      goto LABEL_86;
    }
    v137 = *(const unsigned __int8 **)this;
LABEL_131:
    LODWORD(v170) = v98;
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v137);
    v138 = *((_DWORD *)this + 4);
    v139 = *((_DWORD *)this + 5);
    v140 = v170;
    v100 = v139 + 22;
    v141 = (const unsigned __int8 *)(*(_QWORD *)this + 4LL);
    *(_QWORD *)this = v141;
    *((_DWORD *)this + 5) = v139 + 32;
    v98 = (v138 >> (v139 + 32)) | v140;
    v99 = (unsigned __int16)v98;
    if ( v139 == -32 )
    {
      v102 = 0;
      *((_DWORD *)this + 4) = 0;
      *((_DWORD *)this + 5) = v100;
      goto LABEL_133;
    }
    v101 = v138 << -(char)v139;
    *((_DWORD *)this + 4) = v101;
LABEL_86:
    *((_DWORD *)this + 5) = v100;
    v102 = v101 >> 22;
    if ( (v100 & 0x80000000) == 0 )
    {
      *((_DWORD *)this + 4) = v101 << 10;
      goto LABEL_88;
    }
    v141 = *(const unsigned __int8 **)this;
LABEL_133:
    LODWORD(v170) = v102;
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v141);
    v143 = *((_DWORD *)this + 4);
    v142 = *((_DWORD *)this + 5);
    v144 = v142 + 32;
    v37 = v142 == -32;
    v145 = v143 << -(char)v142;
    if ( v37 )
      v145 = 0;
    v102 = v170 | (v143 >> v144);
    *(_QWORD *)this += 4LL;
    *((_DWORD *)this + 4) = v145;
    *((_DWORD *)this + 5) = v144;
LABEL_88:
    v36 = APACLRVQDecoder::ltpToMdctSpectralEnvelope((APACLRVQDecoder *)v99, v102, (unsigned __int16)v172, (float *)v98);
    if ( !v36 )
    {
      if ( v169 )
      {
LABEL_42:
        vDSP_vmul(v167, 1, v172, 1, v167);
        goto LABEL_44;
      }
      v169 = v95;
LABEL_19:
      v38 = *((_DWORD *)this + 5);
      goto LABEL_20;
    }
    return v36;
  }
  if ( v169 )
    goto LABEL_44;
  v169 = v95;
LABEL_20:
  v39 = *((_DWORD *)this + 4);
  v40 = v38 - 4;
  *((_DWORD *)this + 5) = v40;
  v41 = v39 >> 28;
  if ( v40 < 0 )
  {
    v126 = *(const unsigned __int8 **)this;
LABEL_120:
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v126);
    v127 = *((_DWORD *)this + 4);
    v128 = *((_DWORD *)this + 5);
    v41 |= v127 >> (v128 + 32);
    v129 = v127 << -(char)v128;
    if ( v128 == -32 )
      v129 = 0;
    *(_QWORD *)this += 4LL;
    *((_DWORD *)this + 4) = v129;
    *((_DWORD *)this + 5) = v128 + 32;
    goto LABEL_22;
  }
  *((_DWORD *)this + 4) = 16 * v39;
LABEL_22:
  v42 = 0;
  if ( !v13 )
    v42 = (unsigned __int16)*v17 == (unsigned __int16)v41;
  *v17 = v41;
  v43 = (char *)&unk_42B1F8 + 32 * (unsigned __int16)v41;
  v44 = *((_QWORD *)v43 + 2);
  v170 = *((_QWORD *)v43 + 1);
  v45 = v44;
  if ( !(v170 | v44) )
    return 1;
  v46 = *((__int16 *)v43 + 14);
  v47 = *((__int16 *)v43 + 12);
  v177 = v46;
  if ( !(v47 | v46) )
    return 1;
  v54 = v171;
  v175 = &unk_32FEF8;
  v178 = &unk_32FEFC;
  v48 = buf2;
  v171 = buf2 + 32;
  v180 = v23;
  v49 = v42;
  v181 = v12;
  v182 = buf2;
  v50 = v47;
  v51 = v45;
  v183 = &v184;
  v52 = (TBitstreamReader_u32 *)this;
  v53 = (__int16 *)v54;
  LOWORD(v54) = -1;
  v176 = v46 - 1;
  LODWORD(v174) = v47 - 1;
  do
  {
    if ( v49 )
    {
      cache = v52->cache;
      v56 = v52->bitsLeft - 1;
      v52->bitsLeft = v56;
      v86 = cache >> 31;
      if ( v56 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(v52, v52->cur);
        v115 = v52->cache;
        v116 = -v52->bitsLeft;
        v56 = v52->bitsLeft + 32;
        v52->cur += 4;
        v117 = v115 << v116;
        if ( !v56 )
          v117 = 0;
        LOBYTE(v86) = v86 | (v115 >> v56);
        v52->cache = v117;
        v52->bitsLeft = v56;
      }
      else
      {
        v52->cache = 2 * cache;
      }
      if ( (_BYTE)v86 )
      {
        v57 = *v53;
        v58 = v57;
        goto LABEL_31;
      }
      if ( (unsigned int)v174 > 0x1F )
      {
        v58 = 0;
        LOWORD(v57) = 0;
        if ( v51 )
          goto LABEL_68;
        goto LABEL_74;
      }
      v55 = v52->cache;
      v56 -= v50;
      v52->bitsLeft = v56;
      v57 = v55 >> -(char)v50;
      if ( (v56 & 0x80000000) == 0 )
        goto LABEL_30;
    }
    else
    {
      if ( (unsigned int)v174 > 0x1F )
      {
        v58 = 0;
        LOWORD(v57) = 0;
        if ( v51 )
          goto LABEL_34;
        v58 = 0;
        LOWORD(v57) = 0;
        goto LABEL_74;
      }
      v55 = v52->cache;
      v56 = v52->bitsLeft - v50;
      v52->bitsLeft = v56;
      v57 = v55 >> -(char)v50;
      if ( (v56 & 0x80000000) == 0 )
      {
LABEL_30:
        v57 = (__int16)v57;
        v58 = (__int16)v57;
        v52->cache = v55 << v50;
        goto LABEL_31;
      }
    }
    TBitstreamReader<unsigned int>::FillCacheFrom(v52, v52->cur);
    v90 = v52->cache;
    cur = v52->cur;
    bitsLeft = v52->bitsLeft;
    v56 = bitsLeft + 32;
    v52->bitsLeft = bitsLeft + 32;
    v52->cur = cur + 4;
    v93 = (v90 >> (bitsLeft + 32)) | v57;
    if ( bitsLeft == -32 )
    {
      v57 = v93;
      v52->cache = 0;
      v58 = v93;
    }
    else
    {
      v120 = -bitsLeft;
      v57 = v93;
      v58 = v93;
      v52->cache = v90 << v120;
    }
LABEL_31:
    if ( *((unsigned __int16 *)&unk_42B1F8 + 16 * (unsigned __int16)*v17 + 13) < v57 )
      return 1;
    if ( v51 )
    {
      if ( !v49 )
      {
LABEL_34:
        if ( v176 > 0x1F )
        {
          v64 = 0;
          LOWORD(v54) = 0;
LABEL_38:
          vDSP_vasm(v170 + 32LL * v58, 1, v51 + 32LL * v64, 1, v175, v48, 32, 8);
          goto LABEL_39;
        }
        v59 = v52->cache;
        v60 = v177;
        v61 = -v177;
        v62 = v52->bitsLeft - v177;
        v52->bitsLeft = v62;
        LODWORD(v54) = v59 >> v61;
        if ( (v62 & 0x80000000) == 0 )
        {
          v52->cache = v59 << v60;
          v63 = (unsigned __int16)*v17;
          v64 = (__int16)v54;
          goto LABEL_37;
        }
        v110 = v52->cur;
        v179 = v58;
        TBitstreamReader<unsigned int>::FillCacheFrom(v52, v110);
        v112 = v52->cache;
        v111 = v52->bitsLeft;
        v113 = v52->cur;
        v52->bitsLeft = v111 + 32;
        v52->cur = v113 + 4;
        v58 = v179;
        v114 = (v112 >> (v111 + 32)) | (unsigned __int16)v54;
        if ( v111 == -32 )
        {
          v52->cache = 0;
          LOWORD(v54) = (v112 >> (v111 + 32)) | (unsigned __int16)v54;
          v64 = v114;
          v63 = (unsigned __int16)*v17;
          goto LABEL_37;
        }
        goto LABEL_113;
      }
LABEL_68:
      v87 = v52->cache;
      v88 = v56 - 1;
      v52->bitsLeft = v88;
      v89 = v87 >> 31;
      if ( v88 < 0 )
      {
        v121 = v52->cur;
        v179 = v58;
        TBitstreamReader<unsigned int>::FillCacheFrom(v52, v121);
        v122 = v52->cache;
        v88 = v52->bitsLeft + 32;
        v58 = v179;
        v123 = v122 << -LOBYTE(v52->bitsLeft);
        if ( v52->bitsLeft == -32 )
          v123 = 0;
        LOBYTE(v89) = v89 | (v122 >> v88);
        v52->cur += 4;
        v52->cache = v123;
        v52->bitsLeft = v88;
      }
      else
      {
        v52->cache = 2 * v87;
      }
      if ( (_BYTE)v89 )
      {
        LOWORD(v54) = v53[1];
        v63 = (unsigned __int16)*v17;
        v64 = (__int16)v54;
        goto LABEL_37;
      }
      if ( v176 > 0x1F )
      {
        v63 = (unsigned __int16)*v17;
        v64 = 0;
        LOWORD(v54) = 0;
      }
      else
      {
        v103 = v177;
        v104 = v52->cache;
        v105 = v88 - v177;
        v106 = -v177;
        v52->bitsLeft = v105;
        LODWORD(v54) = v104 >> v106;
        if ( (v105 & 0x80000000) == 0 )
        {
          v52->cache = v104 << v103;
          v63 = (unsigned __int16)*v17;
          v64 = (__int16)v54;
          goto LABEL_37;
        }
        v160 = v52->cur;
        v179 = v58;
        TBitstreamReader<unsigned int>::FillCacheFrom(v52, v160);
        v112 = v52->cache;
        v111 = v52->bitsLeft;
        v161 = v52->cur;
        v52->bitsLeft = v111 + 32;
        v52->cur = v161 + 4;
        v58 = v179;
        v114 = (v112 >> (v111 + 32)) | (unsigned __int16)v54;
        if ( v111 != -32 )
        {
LABEL_113:
          v124 = -v111;
          LOWORD(v54) = v114;
          v64 = v114;
          v52->cache = v112 << v124;
          v63 = (unsigned __int16)*v17;
          goto LABEL_37;
        }
        v52->cache = 0;
        LOWORD(v54) = (v112 >> (v111 + 32)) | (unsigned __int16)v54;
        v64 = v114;
        v63 = (unsigned __int16)*v17;
      }
LABEL_37:
      if ( *((unsigned __int16 *)&unk_42B1F8 + 16 * v63 + 15) < v64 )
        return 1;
      goto LABEL_38;
    }
LABEL_74:
    vDSP_vsadd(v170 + 32LL * v58, 1, v178, v48, 32, 8);
LABEL_39:
    v65 = v171;
    *v53 = v57;
    v53[1] = (__int16)v54;
    ++v48;
    v53 += 2;
  }
  while ( v65 != v48 );
  v23 = v180;
  v12 = v181;
  buf2 = v182;
  v20 = v183;
  if ( v168 )
  {
LABEL_103:
    vDSP_vdiv(v167, 1, buf2, 1, buf2, 1, 256, v22, v165, v166);
    if ( (_DWORD)v173 )
    {
      v146 = 0;
      LOWORD(v147) = 0;
      vDSP_vsmul(buf2, 1, v20, buf2, 1, 256);
      do
      {
        v151 = *v12;
        if ( (v147 & 1) != 0 )
        {
          memmove((void *)(v151 + v146), buf2, 0x80u);
          v149 = buf2 + 32;
          vDSP_vdiv(&unk_32FF00, 1, *v12 + v146, 1, *v12 + v146, 1, 32, v150, v165, v166);
        }
        else
        {
          v149 = buf2 + 32;
          v152 = (_DWORD *)(v151 + v146);
          v153 = buf2 + 32;
          do
          {
            v154 = *((_DWORD *)v153-- - 1);
            *v152++ = v154;
          }
          while ( v153 != buf2 );
          vDSP_vdiv(&unk_32FF00, 1, *v12 + v146, 1, *v12 + v146, 1, 32, v148, v165, v166);
        }
        v146 += 512;
        v147 = (unsigned __int16)(v147 + 1);
        buf2 = v149;
      }
      while ( v147 != 8 );
    }
    else
    {
      for ( i = 0; i != 4096; i += 512 )
      {
        v119 = *v12 + i;
        vDSP_vsmul(buf2++, 8, v20, v119, 1, 32);
      }
    }
    return v23;
  }
  if ( v169 )
    goto LABEL_42;
LABEL_44:
  vDSP_vdiv(v167, 1, buf2, 1, buf2, 1, 256, v22, v165, v166);
  vDSP_vsmul(buf2, 1, v20, *v12, 1, 256);
  return v23;
}


/* ------------------------------------------------------------------------ */
/* 0x0009bdc0  size 408 bytes  102 insns
 *   APACLRVQDecoder::SpectrumDeserializeSCE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACIndividualChannelStream&)
 *   [_ZN15APACLRVQDecoder22SpectrumDeserializeSCEER16TBitstreamReaderIjER11APACICSInfoR27APACIndividualChannelStream]
 *   data refs: 0x9a720, 0x42ce00, 0x42cec8, 0x43e520
 */
__int64 __fastcall APACLRVQDecoder::SpectrumDeserializeSCE(
        APACLRVQDecoder *this,
        TBitstreamReader_u32 *bs,
        APACICSInfo *ics,
        APACIndividualChannelStream *ics_stream)
{
  __int64 v4; // x29
  __int64 v5; // x30
  __int64 *v6; // x9
  char v8; // w20
  int v9; // w23
  _DWORD *v10; // x4
  _DWORD *i; // x5
  unsigned int v12; // w5
  int v13; // w4
  unsigned int v14; // w19
  float *v15; // x1
  __int64 (__fastcall *v17)(APACICSInfo *); // x1
  const unsigned __int8 *v18; // x1
  unsigned int v19; // w1
  int v20; // w2
  int v21; // w1
  unsigned __int16 *v22; // [xsp+8h] [xbp-24F8h]
  __int64 v23; // [xsp+10h] [xbp-24F0h]
  __int64 v24; // [xsp+18h] [xbp-24E8h]
  APACLRVQDecoder *v25; // [xsp+58h] [xbp-24A8h]
  __int16 v26; // [xsp+6Eh] [xbp-2492h] BYREF
  float v27[32]; // [xsp+70h] [xbp-2490h] BYREF
  float v28[256]; // [xsp+F0h] [xbp-2410h] BYREF
  _BYTE v29[4096]; // [xsp+4F0h] [xbp-2010h] BYREF
  float v30; // [xsp+14F0h] [xbp-1010h] BYREF
  __int64 v31; // [xsp+2500h] [xbp+0h] BYREF
  v6 = &v31 - 3072;
  v6[512] = 0;
  v6[352] = 0;
  v23 = v4;
  v24 = v5;
  v8 = (char)ics;
  v9 = BYTE2(bs->end);
  v10 = *((_DWORD **)ics + 94);
  for ( i = *((_DWORD **)ics + 95); i != v10; ++v10 )
    *v10 = 0;
  v12 = *((_DWORD *)this + 4);
  v13 = *((_DWORD *)this + 5) - 1;
  *((_DWORD *)this + 5) = v13;
  v14 = v12 >> 31;
  if ( v13 < 0 )
  {
    v18 = *(const unsigned __int8 **)this;
    v25 = this;
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v18);
    this = v25;
    v19 = *((_DWORD *)v25 + 4);
    v20 = *((_DWORD *)v25 + 5);
    LOBYTE(v14) = v14 | (v19 >> (v20 + 32));
    v21 = v19 << -(char)v20;
    if ( v20 == -32 )
      v21 = 0;
    *(_QWORD *)v25 += 4LL;
    *((_DWORD *)this + 4) = v21;
    *((_DWORD *)this + 5) = v20 + 32;
  }
  else
  {
    *((_DWORD *)this + 4) = 2 * v12;
  }
  if ( (_BYTE)v14 )
  {
    v17 = *((__int64 (__fastcall **)(APACICSInfo *))bs->cur + 3);
    if ( v17 == APACICSInfo::Reset )
    {
      WORD1(bs->end) = 0;
      BYTE4(bs->end) = 0;
      bs->cache = 1;
    }
    else
    {
      v17((APACICSInfo *)bs);
    }
    return 0;
  }
  else
  {
    v15 = v27;
    do
    {
      *(_WORD *)v15 = 0;
      v15 = (float *)((char *)v15 + 2);
    }
    while ( v15 != v28 );
    v26 = 0;
    return APACLRVQDecoder::LrvqDeserialize(
             this,
             (TBitstreamReader_u32 *)(v9 == 2),
             1,
             v8 - 16,
             (APACSpectralData *)v29,
             &v30,
             v28,
             v27,
             &v26,
             v22);
  }
}


/* ------------------------------------------------------------------------ */
/* 0x0009bf60  size 644 bytes  161 insns
 *   APACLRVQDecoder::SpectrumDeserializeCPE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACICSInfo&, APACIndividualChannelStream&, APACIndividualChannelStream&)
 *   [_ZN15APACLRVQDecoder22SpectrumDeserializeCPEER16TBitstreamReaderIjER11APACICSInfoS4_R27APACIndividualChannelStreamS6_]
 *   data refs: 0x9a720, 0x42ce00, 0x42cec8, 0x43e520
 */
__int64 __fastcall APACLRVQDecoder::SpectrumDeserializeCPE(
        APACLRVQDecoder *this,
        TBitstreamReader_u32 *bs,
        APACICSInfo *ics,
        APACICSInfo *ics2,
        APACIndividualChannelStream *ics_stream,
        APACIndividualChannelStream *ics_stream2)
{
  __int64 v6; // x29
  __int64 v7; // x30
  __int64 *v8; // x9
  char v10; // w22
  int v14; // w26
  _DWORD *v15; // x5
  _DWORD *i; // x6
  unsigned int v17; // w1
  int v18; // w0
  unsigned int v19; // w25
  __int64 (__fastcall *v20)(APACICSInfo *); // x1
  _DWORD *v21; // x1
  _DWORD *v22; // x0
  int j; // w22
  unsigned int v24; // w1
  int v25; // w0
  unsigned int v26; // w20
  __int64 (__fastcall *v27)(APACICSInfo *); // x1
  __int64 result; // x0
  unsigned int v29; // w0
  int v30; // w1
  int v31; // w0
  unsigned int v32; // w0
  int v33; // w1
  int v34; // w0
  unsigned __int16 *v35; // [xsp+8h] [xbp-24F8h]
  __int64 v36; // [xsp+10h] [xbp-24F0h]
  __int64 v37; // [xsp+18h] [xbp-24E8h]
  __int16 v38; // [xsp+6Eh] [xbp-2492h] BYREF
  float v39[32]; // [xsp+70h] [xbp-2490h] BYREF
  float v40[256]; // [xsp+F0h] [xbp-2410h] BYREF
  _BYTE v41[4096]; // [xsp+4F0h] [xbp-2010h] BYREF
  float v42[1026]; // [xsp+14F0h] [xbp-1010h] BYREF
  __int64 v43; // [xsp+2500h] [xbp+0h] BYREF
  v8 = &v43 - 3072;
  v8[512] = 0;
  v8[352] = 0;
  v36 = v6;
  v37 = v7;
  v10 = (char)ics2;
  v14 = BYTE2(bs->end);
  v15 = *((_DWORD **)ics2 + 94);
  for ( i = *((_DWORD **)ics2 + 95); v15 != i; ++v15 )
    *v15 = 0;
  v17 = *((_DWORD *)this + 4);
  v18 = *((_DWORD *)this + 5) - 1;
  *((_DWORD *)this + 5) = v18;
  v19 = v17 >> 31;
  if ( v18 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
    v29 = *((_DWORD *)this + 4);
    v30 = *((_DWORD *)this + 5);
    LOBYTE(v19) = v19 | (v29 >> (v30 + 32));
    v31 = v29 << -(char)v30;
    if ( v30 == -32 )
      v31 = 0;
    *(_QWORD *)this += 4LL;
    *((_DWORD *)this + 4) = v31;
    *((_DWORD *)this + 5) = v30 + 32;
  }
  else
  {
    *((_DWORD *)this + 4) = 2 * v17;
  }
  if ( (_BYTE)v19 )
  {
    v20 = *((__int64 (__fastcall **)(APACICSInfo *))bs->cur + 3);
    if ( v20 == APACICSInfo::Reset )
    {
      WORD1(bs->end) = 0;
      BYTE4(bs->end) = 0;
      bs->cache = 1;
    }
    else
    {
      v20((APACICSInfo *)bs);
    }
  }
  else
  {
    result = APACLRVQDecoder::LrvqDeserialize(
               this,
               (TBitstreamReader_u32 *)(v14 == 2),
               1,
               v10 - 16,
               (APACSpectralData *)v41,
               v42,
               v40,
               v39,
               &v38,
               v35);
    if ( (_DWORD)result )
      return result;
  }
  v21 = *((_DWORD **)ics_stream + 94);
  v22 = *((_DWORD **)ics_stream + 95);
  for ( j = *((unsigned __int8 *)ics + 10); v22 != v21; ++v21 )
    *v21 = 0;
  v24 = *((_DWORD *)this + 4);
  v25 = *((_DWORD *)this + 5) - 1;
  *((_DWORD *)this + 5) = v25;
  v26 = v24 >> 31;
  if ( v25 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
    v32 = *((_DWORD *)this + 4);
    v33 = *((_DWORD *)this + 5);
    LOBYTE(v26) = v26 | (v32 >> (v33 + 32));
    v34 = v32 << -(char)v33;
    if ( v33 == -32 )
      v34 = 0;
    *(_QWORD *)this += 4LL;
    *((_DWORD *)this + 4) = v34;
    *((_DWORD *)this + 5) = v33 + 32;
  }
  else
  {
    *((_DWORD *)this + 4) = 2 * v24;
  }
  if ( !(_BYTE)v26 )
    return APACLRVQDecoder::LrvqDeserialize(
             this,
             (TBitstreamReader_u32 *)(j == 2),
             0,
             (unsigned __int8)ics_stream - 16,
             (APACSpectralData *)v41,
             v42,
             v40,
             v39,
             &v38,
             v35);
  v27 = *(__int64 (__fastcall **)(APACICSInfo *))(*(_QWORD *)ics + 24LL);
  if ( v27 == APACICSInfo::Reset )
  {
    *((_WORD *)ics + 5) = 0;
    *((_BYTE *)ics + 12) = 0;
    *((_DWORD *)ics + 4) = 1;
  }
  else
  {
    v27(ics);
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009c1f0  size 1260 bytes  313 insns
 *   APACLRVQDecoder::SpectrumDeserializeLFE(TBitstreamReader<unsigned int>&, APACICSInfo&, APACIndividualChannelStream&)
 *   [_ZN15APACLRVQDecoder22SpectrumDeserializeLFEER16TBitstreamReaderIjER11APACICSInfoR27APACIndividualChannelStream]
 *   data refs: 0x9a720, 0x32fef8, 0x32fefc, 0x42b1f8, 0x42ce00, 0x42cec8, 0x43e520
 */
__int64 __fastcall APACLRVQDecoder::SpectrumDeserializeLFE(
        APACLRVQDecoder *this,
        TBitstreamReader_u32 *bs,
        APACICSInfo *ics,
        APACIndividualChannelStream *ics_stream)
{
  __int64 v4; // x23
  __int64 v5; // x24
  __int64 v6; // x25
  __int64 v7; // x26
  float *v11; // x2
  unsigned __int64 i; // x3
  unsigned int v13; // w2
  int v14; // w0
  unsigned int v15; // w22
  unsigned int v16; // w2
  unsigned int v17; // w21
  int v18; // w0
  unsigned int v19; // w2
  unsigned int v20; // w22
  unsigned int v21; // w27
  unsigned int v22; // w1
  int v23; // w0
  unsigned int v24; // w21
  char *v25; // x0
  __int64 v26; // x22
  __int64 v27; // x26
  int v28; // w1
  int v29; // w21
  char *v30; // x25
  __int64 v31; // x3
  unsigned int v32; // w1
  char v33; // w2
  char v34; // w3
  int v35; // w0
  unsigned int v36; // w3
  __int64 v37; // x2
  unsigned int v38; // w4
  int v39; // w0
  __int64 (__fastcall *v41)(APACICSInfo *); // x1
  unsigned int v42; // w2
  const unsigned __int8 *v43; // x1
  int v44; // w3
  const unsigned __int8 *v45; // x1
  unsigned int v46; // w2
  int v47; // w3
  const unsigned __int8 *v48; // x1
  const unsigned __int8 *v49; // x1
  unsigned int v50; // w0
  int v51; // w1
  unsigned __int8 *v52; // x3
  unsigned int v53; // w22
  int v54; // w0
  const unsigned __int8 *v55; // x1
  unsigned int v56; // w2
  int v57; // w4
  unsigned int v58; // w3
  unsigned int v59; // w0
  int v60; // w1
  int v61; // w0
  const unsigned __int8 *v62; // x1
  unsigned int v63; // w4
  int v64; // w5
  __int16 v65; // w2
  __int64 v66; // [xsp+30h] [xbp+30h]
  __int64 v67; // [xsp+38h] [xbp+38h]
  __int64 v68; // [xsp+40h] [xbp+40h]
  __int64 v69; // [xsp+48h] [xbp+48h]
  char *v70; // [xsp+68h] [xbp+68h]
  unsigned int v71; // [xsp+74h] [xbp+74h]
  unsigned int v72; // [xsp+78h] [xbp+78h]
  int v73; // [xsp+7Ch] [xbp+7Ch]
  _BYTE *v74; // [xsp+80h] [xbp+80h]
  void *v75; // [xsp+88h] [xbp+88h]
  int v76; // [xsp+90h] [xbp+90h]
  unsigned int v77; // [xsp+94h] [xbp+94h]
  __int64 v78; // [xsp+98h] [xbp+98h]
  _BYTE v79[4]; // [xsp+ACh] [xbp+ACh] BYREF
  _BYTE v80[16]; // [xsp+B0h] [xbp+B0h] BYREF
  char v81; // [xsp+C0h] [xbp+C0h] BYREF
  __int64 v82; // [xsp+E8h] [xbp+E8h]
  __int64 v83; // [xsp+F0h] [xbp+F0h]
  v11 = *((float **)ics + 94);
  for ( i = *((_QWORD *)ics + 95); (float *)i != v11; ++v11 )
    *v11 = 0.0;
  v13 = *((_DWORD *)this + 4);
  v14 = *((_DWORD *)this + 5) - 1;
  *((_DWORD *)this + 5) = v14;
  v15 = v13 >> 31;
  if ( v14 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
    v42 = *((_DWORD *)this + 4);
    v43 = *(const unsigned __int8 **)this;
    v44 = *((_DWORD *)this + 5);
    v14 = v44 + 32;
    *((_DWORD *)this + 5) = v44 + 32;
    v45 = v43 + 4;
    *(_QWORD *)this = v45;
    LOBYTE(v15) = v15 | (v42 >> (v44 + 32));
    if ( v44 == -32 )
    {
      *((_DWORD *)this + 4) = 0;
      if ( !(_BYTE)v15 )
      {
        LOWORD(v17) = 0;
        *((_DWORD *)this + 5) = -1;
        goto LABEL_33;
      }
LABEL_28:
      v41 = *((__int64 (__fastcall **)(APACICSInfo *))bs->cur + 3);
      v21 = 0;
      if ( v41 == APACICSInfo::Reset )
      {
        WORD1(bs->end) = 0;
        BYTE4(bs->end) = 0;
        bs->cache = 1;
      }
      else
      {
        v41((APACICSInfo *)bs);
      }
      goto LABEL_26;
    }
    i = (unsigned int)-v44;
    *((_DWORD *)this + 4) = v42 << i;
  }
  else
  {
    *((_DWORD *)this + 4) = 2 * v13;
  }
  if ( (_BYTE)v15 )
    goto LABEL_28;
  v16 = *((_DWORD *)this + 4);
  *((_DWORD *)this + 5) = v14 - 1;
  v17 = v16 >> 31;
  if ( ((v14 - 1) & 0x80000000) == 0 )
  {
    v18 = v14 - 10;
    v19 = 2 * v16;
    *((_DWORD *)this + 4) = v19;
    goto LABEL_8;
  }
  v45 = *(const unsigned __int8 **)this;
LABEL_33:
  TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v45);
  v46 = *((_DWORD *)this + 4);
  v47 = *((_DWORD *)this + 5);
  v48 = *(const unsigned __int8 **)this;
  *((_DWORD *)this + 5) = v47 + 32;
  v18 = v47 + 23;
  v49 = v48 + 4;
  *(_QWORD *)this = v49;
  LOWORD(v17) = v17 | (v46 >> (v47 + 32));
  if ( v47 == -32 )
  {
    v20 = 0;
    *((_DWORD *)this + 4) = 0;
    *((_DWORD *)this + 5) = v18;
    goto LABEL_35;
  }
  i = (unsigned int)-v47;
  v19 = v46 << i;
  *((_DWORD *)this + 4) = v19;
LABEL_8:
  *((_DWORD *)this + 5) = v18;
  v20 = v19 >> 23;
  if ( (v18 & 0x80000000) == 0 )
  {
    *((_DWORD *)this + 4) = v19 << 9;
    v21 = APACLRVQDecoder::dequantizeLrvqGain((APACLRVQDecoder *)v20, v17, (unsigned __int16)v79, (float *)i);
    if ( v21 )
      goto LABEL_26;
    goto LABEL_10;
  }
  v49 = *(const unsigned __int8 **)this;
LABEL_35:
  TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v49);
  v50 = *((_DWORD *)this + 4);
  v51 = *((_DWORD *)this + 5);
  v52 = *(unsigned __int8 **)this;
  v53 = v20 | (v50 >> (v51 + 32));
  v54 = v50 << -(char)v51;
  if ( v51 == -32 )
    v54 = 0;
  *(_QWORD *)this += 4LL;
  *((_DWORD *)this + 4) = v54;
  *((_DWORD *)this + 5) = v51 + 32;
  v21 = APACLRVQDecoder::dequantizeLrvqGain((APACLRVQDecoder *)v53, v17, (unsigned __int16)v79, (float *)v52);
  if ( !v21 )
  {
LABEL_10:
    v68 = v6;
    v69 = v7;
    v22 = *((_DWORD *)this + 4);
    v23 = *((_DWORD *)this + 5) - 4;
    *((_DWORD *)this + 5) = v23;
    v24 = v22 >> 28;
    if ( v23 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, *(const unsigned __int8 **)this);
      v59 = *((_DWORD *)this + 4);
      v60 = *((_DWORD *)this + 5);
      LOWORD(v24) = v24 | (v59 >> (v60 + 32));
      v61 = v59 << -(char)v60;
      if ( v60 == -32 )
        v61 = 0;
      *(_QWORD *)this += 4LL;
      *((_DWORD *)this + 4) = v61;
      *((_DWORD *)this + 5) = v60 + 32;
    }
    else
    {
      *((_DWORD *)this + 4) = 16 * v22;
    }
    v25 = (char *)&unk_42B1F8 + 32 * (unsigned __int16)v24;
    v27 = *((_QWORD *)v25 + 1);
    v26 = *((_QWORD *)v25 + 2);
    if ( *(_OWORD *)(v25 + 8) == 0
      || (v28 = *((__int16 *)v25 + 12), v29 = *((__int16 *)v25 + 14), v73 = v28, !(v28 | v29)) )
    {
      v21 = 1;
      v6 = v68;
      v7 = v69;
    }
    else
    {
      v30 = v80;
      v74 = v80;
      v75 = &unk_32FEFC;
      v70 = &v81;
      v71 = v28 - 1;
      v76 = 32 - v28;
      v72 = v29 - 1;
      do
      {
        v31 = v27;
        if ( v71 <= 0x1F )
        {
          v32 = *((_DWORD *)this + 4);
          v33 = v73;
          v34 = v76;
          v35 = *((_DWORD *)this + 5) - v73;
          *((_DWORD *)this + 5) = v35;
          v36 = v32 >> v34;
          if ( v35 < 0 )
          {
            v55 = *(const unsigned __int8 **)this;
            v77 = v36;
            TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v55);
            v56 = *((_DWORD *)this + 4);
            v57 = *((_DWORD *)this + 5);
            v58 = v77;
            *(_QWORD *)this += 4LL;
            *((_DWORD *)this + 5) = v57 + 32;
            v31 = v27 + 4LL * ((int)(((v56 >> (v57 + 32)) | v58) << 16) >> 13);
            if ( v57 == -32 )
              *((_DWORD *)this + 4) = 0;
            else
              *((_DWORD *)this + 4) = v56 << -(char)v57;
          }
          else
          {
            *((_DWORD *)this + 4) = v32 << v33;
            v31 = v27 + 32LL * (__int16)v36;
          }
        }
        if ( v26 )
        {
          v37 = v26;
          if ( v72 <= 0x1F )
          {
            v38 = *((_DWORD *)this + 4);
            v39 = *((_DWORD *)this + 5) - v29;
            *((_DWORD *)this + 5) = v39;
            if ( v39 < 0 )
            {
              v62 = *(const unsigned __int8 **)this;
              v77 = v38 >> (32 - v29);
              v78 = v31;
              TBitstreamReader<unsigned int>::FillCacheFrom((TBitstreamReader_u32 *)this, v62);
              v63 = *((_DWORD *)this + 4);
              v64 = *((_DWORD *)this + 5);
              v65 = v77;
              *(_QWORD *)this += 4LL;
              *((_DWORD *)this + 5) = v64 + 32;
              v31 = v78;
              v37 = v26 + 32LL * (__int16)((v63 >> (v64 + 32)) | v65);
              if ( v64 == -32 )
                *((_DWORD *)this + 4) = 0;
              else
                *((_DWORD *)this + 4) = v63 << -(char)v64;
            }
            else
            {
              *((_DWORD *)this + 4) = v38 << v29;
              v37 = v26 + 32LL * (__int16)(v38 >> (32 - v29));
            }
          }
          vDSP_vasm(v31, 1, v37, 1, &unk_32FEF8, v30, 4, 3);
        }
        else
        {
          vDSP_vsadd(v31, 1, v75, v30, 4, 3);
        }
        v30 += 4;
      }
      while ( v30 != v70 );
      vDSP_vsmul(v74, 1, v79, *((_QWORD *)ics + 94), 1, 12);
      v6 = v68;
      v7 = v69;
    }
  }
LABEL_26:
  if ( v82 != _stack_chk_guard )
  {
    v66 = v4;
    v67 = v5;
    v68 = v6;
    v69 = v7;
  }
  return v21;
}


/* ------------------------------------------------------------------------ */
/* 0x0009c830  size 100 bytes  25 insns
 *   APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()
 *   APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()
 *   [_ZN15APACLRVQDecoder11LPMDCT_LONGD1Ev]
 *   [_ZN15APACLRVQDecoder11LPMDCT_LONGD2Ev]
 *   data refs: 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42dbe8, 0x43e520
 */
// Alternative name is '_ZN15APACLRVQDecoder11LPMDCT_LONGD2Ev'
void __fastcall APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG(APACLRVQDecoder__LPMDCT_LONG *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  *(_QWORD *)this = off_42B710;
  *((_QWORD *)this + 2) = off_42A838;
  DFTHandle<false>::~DFTHandle((char *)this + 16);
}


/* ------------------------------------------------------------------------ */
/* 0x0009c8a0  size 116 bytes  29 insns
 *   APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG()
 *   [_ZN15APACLRVQDecoder11LPMDCT_LONGD0Ev]
 *   data refs: 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42dbe8, 0x43e520
 */
void __fastcall APACLRVQDecoder::LPMDCT_LONG::~LPMDCT_LONG(APACLRVQDecoder__LPMDCT_LONG *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  *(_QWORD *)this = off_42B710;
  *((_QWORD *)this + 2) = off_42A838;
  DFTHandle<false>::~DFTHandle((char *)this + 16);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x0009c920  size 100 bytes  25 insns
 *   APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()
 *   APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()
 *   [_ZN15APACLRVQDecoder10LPMDCT_LTPD2Ev]
 *   [_ZN15APACLRVQDecoder10LPMDCT_LTPD1Ev]
 *   data refs: 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42dbe8, 0x43e520
 */
// Alternative name is '_ZN15APACLRVQDecoder10LPMDCT_LTPD1Ev'
void __fastcall APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP(APACLRVQDecoder__LPMDCT_LTP *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  *(_QWORD *)this = off_42B710;
  *((_QWORD *)this + 2) = off_42A838;
  DFTHandle<false>::~DFTHandle((char *)this + 16);
}


/* ------------------------------------------------------------------------ */
/* 0x0009c990  size 116 bytes  29 insns
 *   APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP()
 *   [_ZN15APACLRVQDecoder10LPMDCT_LTPD0Ev]
 *   data refs: 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42dbe8, 0x43e520
 */
void __fastcall APACLRVQDecoder::LPMDCT_LTP::~LPMDCT_LTP(APACLRVQDecoder__LPMDCT_LTP *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  *(_QWORD *)this = off_42B710;
  *((_QWORD *)this + 2) = off_42A838;
  DFTHandle<false>::~DFTHandle((char *)this + 16);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x0009ca10  size 416 bytes  104 insns
 *   APACLRVQDecoder::LRVQLPC::~LRVQLPC()
 *   APACLRVQDecoder::LRVQLPC::~LRVQLPC()
 *   [_ZN15APACLRVQDecoder7LRVQLPCD2Ev]
 *   [_ZN15APACLRVQDecoder7LRVQLPCD1Ev]
 *   data refs: 0x79810, 0x42a848, 0x42b750, 0x42b798, 0x42ce00, 0x42d070, 0x42d7d8, 0x42dbd8, 0x42ddc0, 0x43e520
 */
// Alternative name is '_ZN15APACLRVQDecoder7LRVQLPCD1Ev'
void __fastcall APACLRVQDecoder::LRVQLPC::~LRVQLPC(APACLRVQDecoder__LRVQLPC *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void *v5; // x0
  void *v6; // x0
  void *v7; // x0
  _QWORD **v8; // x21
  _QWORD **v9; // x20
  _QWORD *v10; // x23
  void *v11; // x0
  void *v12; // x0
  _QWORD *v13; // x20
  void (__fastcall *v14)(apac__Autocorr *); // x1
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  __int64 v18; // [xsp+30h] [xbp+30h]
  __int64 v19; // [xsp+48h] [xbp+48h]
  __int64 v20; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 27);
  *(_QWORD *)this = off_42B7A8;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 24);
  if ( v4 )
    operator delete(v4);
  v5 = *((void **)this + 21);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 18);
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 15);
  if ( v7 )
    operator delete(v7);
  *(_QWORD *)this = &unk_42B760;
  v8 = *((_QWORD ***)this + 11);
  v9 = *((_QWORD ***)this + 10);
  if ( v9 != v8 )
  {
    v18 = v1;
    do
    {
      v10 = *v9;
      if ( *v9 )
      {
        v11 = (void *)v10[6];
        if ( v11 )
          operator delete(v11);
        v12 = (void *)v10[2];
        if ( v12 )
          operator delete(v12);
        operator delete(v10);
      }
      ++v9;
    }
    while ( v8 != v9 );
    v1 = v18;
    v8 = *((_QWORD ***)this + 10);
  }
  if ( v8 )
    operator delete(v8);
  v13 = *((_QWORD **)this + 9);
  if ( v13 )
  {
    v14 = *(void (__fastcall **)(apac__Autocorr *))(*v13 + 8LL);
    if ( v14 == apac::Autocorr::~Autocorr )
    {
      v15 = (void *)v13[2];
      *v13 = off_42A858;
      if ( v15 )
        operator delete(v15);
      operator delete(v13);
    }
    else
    {
      v14(*((apac__Autocorr **)this + 9));
    }
  }
  v16 = *((void **)this + 6);
  if ( v16 )
    operator delete(v16);
  v17 = *((void **)this + 3);
  if ( v17 )
  {
    if ( v19 == _stack_chk_guard )
    {
      operator delete(v17);
      return;
    }
LABEL_36:
    v18 = v1;
  }
  if ( v19 != _stack_chk_guard )
    goto LABEL_36;
}


/* ------------------------------------------------------------------------ */
/* 0x0009cbb0  size 392 bytes  98 insns
 *   APACLRVQDecoder::LRVQLPC::~LRVQLPC()
 *   [_ZN15APACLRVQDecoder7LRVQLPCD0Ev]
 *   data refs: 0x79810, 0x42a848, 0x42b750, 0x42b798, 0x42ce00, 0x42d070, 0x42d7d8, 0x42dbd8, 0x42ddc0, 0x43e520
 */
void __fastcall APACLRVQDecoder::LRVQLPC::~LRVQLPC(APACLRVQDecoder__LRVQLPC *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void *v5; // x0
  void *v6; // x0
  void *v7; // x0
  _QWORD **v8; // x21
  _QWORD **v9; // x20
  _QWORD *v10; // x23
  void *v11; // x0
  void *v12; // x0
  _QWORD *v13; // x20
  void (__fastcall *v14)(apac__Autocorr *); // x1
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  __int64 v18; // [xsp+30h] [xbp+30h]
  __int64 v19; // [xsp+48h] [xbp+48h]
  __int64 v20; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 27);
  *(_QWORD *)this = off_42B7A8;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 24);
  if ( v4 )
    operator delete(v4);
  v5 = *((void **)this + 21);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 18);
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 15);
  if ( v7 )
    operator delete(v7);
  *(_QWORD *)this = &unk_42B760;
  v8 = *((_QWORD ***)this + 11);
  v9 = *((_QWORD ***)this + 10);
  if ( v9 != v8 )
  {
    v18 = v1;
    do
    {
      v10 = *v9;
      if ( *v9 )
      {
        v11 = (void *)v10[6];
        if ( v11 )
          operator delete(v11);
        v12 = (void *)v10[2];
        if ( v12 )
          operator delete(v12);
        operator delete(v10);
      }
      ++v9;
    }
    while ( v8 != v9 );
    v1 = v18;
    v8 = *((_QWORD ***)this + 10);
  }
  if ( v8 )
    operator delete(v8);
  v13 = *((_QWORD **)this + 9);
  if ( v13 )
  {
    v14 = *(void (__fastcall **)(apac__Autocorr *))(*v13 + 8LL);
    if ( v14 == apac::Autocorr::~Autocorr )
    {
      v15 = (void *)v13[2];
      *v13 = off_42A858;
      if ( v15 )
        operator delete(v15);
      operator delete(v13);
    }
    else
    {
      v14(*((apac__Autocorr **)this + 9));
    }
  }
  v16 = *((void **)this + 6);
  if ( v16 )
    operator delete(v16);
  v17 = *((void **)this + 3);
  if ( v17 )
    operator delete(v17);
  if ( v19 != _stack_chk_guard )
    v18 = v1;
  operator delete(this);
}


