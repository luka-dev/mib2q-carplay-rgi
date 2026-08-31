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


#if 0 /* disassembly of 0x00070c6c, for verification */
00070C6C  e91340d1  SUB             X9, SP, #4,LSL#12
00070C70  3ff107f9  STR             XZR, [X9,#0xFE0]
00070C74  e11d0090  ADRP            X1, #__stack_chk_guard_ptr@PAGE
00070C78  fd7bbea9  STP             X29, X30, [SP,#var_20]!
00070C7C  800080d2  MOV             X0, #4; thrown_size
00070C80  fd030091  MOV             X29, SP
00070C84  210047f9  LDR             X1, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
00070C88  220040f9  LDR             X2, [X1]
00070C8C  e20f00f9  STR             X2, [SP,#0x20+var_8]
00070C90  020080d2  MOV             X2, #0
00070C94  ebfbff97  BL              .__cxa_allocate_exception
00070C98  e11d0090  ADRP            X1, #_ZTIi_ptr@PAGE
00070C9C  83008052  MOV             W3, #4
00070CA0  030000b9  STR             W3, [X0]
00070CA4  020080d2  MOV             X2, #0; void (*)(void *)
00070CA8  21fc44f9  LDR             X1, [X1,#_ZTIi_ptr@PAGEOFF]; lptinfo
00070CAC  8dfeff97  BL              .__cxa_throw
#endif

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


#if 0 /* disassembly of 0x00070cb0, for verification */
00070CB0  e91340d1  SUB             X9, SP, #4,LSL#12
00070CB4  3ff107f9  STR             XZR, [X9,#0xFE0]
00070CB8  e11d0090  ADRP            X1, #__stack_chk_guard_ptr@PAGE
00070CBC  fd7bbea9  STP             X29, X30, [SP,#var_20]!
00070CC0  800080d2  MOV             X0, #4; thrown_size
00070CC4  fd030091  MOV             X29, SP
00070CC8  210047f9  LDR             X1, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
00070CCC  220040f9  LDR             X2, [X1]
00070CD0  e20f00f9  STR             X2, [SP,#0x20+var_8]
00070CD4  020080d2  MOV             X2, #0
00070CD8  dafbff97  BL              .__cxa_allocate_exception
00070CDC  e11d0090  ADRP            X1, #_ZTIi_ptr@PAGE
00070CE0  83008052  MOV             W3, #4
00070CE4  030000b9  STR             W3, [X0]
00070CE8  020080d2  MOV             X2, #0; void (*)(void *)
00070CEC  21fc44f9  LDR             X1, [X1,#_ZTIi_ptr@PAGEOFF]; lptinfo
00070CF0  7cfeff97  BL              .__cxa_throw
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009a950  size 184 bytes  45 insns
 *   APACLRVQDecoder::dequantizeLrvqGain(unsigned short, unsigned short, float*)
 *   [_ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf]
 *   data refs: 0x42ce00, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
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


#if 0 /* disassembly of 0x0009a950, for verification */
0009A950  e91340d1  SUB             X9, SP, #4,LSL#12
0009A954  3fe907f9  STR             XZR, [X9,#0xFD0]
0009A958  fd7bbda9  STP             X29, X30, [SP,#var_30]!
0009A95C  03009052  MOV             W3, #0x43FF8000
0009A964  fd030091  MOV             X29, SP
0009A968  f35301a9  STP             X19, X20, [SP,#0x30+var_20]
0009A96C  931c00d0  ADRP            X19, #__stack_chk_guard_ptr@PAGE
0009A970  003c0012  AND             W0, W0, #0xFFFF
0009A974  6000271e  FMOV            S0, W3
0009A978  213c0012  AND             W1, W1, #0xFFFF
0009A97C  630247f9  LDR             X3, [X19,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A980  0100231e  UCVTF           S1, W0
0009A984  3f040071  CMP             W1, #1
0009A988  f40302aa  MOV             X20, X2
0009A98C  600040f9  LDR             X0, [X3]
0009A990  e01700f9  STR             X0, [SP,#0x30+var_8]
0009A994  000080d2  MOV             X0, #0
0009A998  2118201e  FDIV            S1, S1, S0
0009A99C  80020054  B.EQ            loc_9A9EC
0009A9A0  0210221e  FMOV            S2, #4.0
0009A9A4  0090241e  FMOV            S0, #10.0; x
0009A9A8  2108221e  FMUL            S1, S1, S2; y
0009A9AC  f956ff97  BL              .powf
0009A9B0  0190241e  FMOV            S1, #10.0
0009A9B4  2018201e  FDIV            S0, S1, S0
0009A9B8  00f0a852  MOV             W0, #0x47800000
0009A9BC  0100271e  FMOV            S1, W0
0009A9C0  730247f9  LDR             X19, [X19,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A9C4  0008211e  FMUL            S0, S0, S1
0009A9C8  800200bd  STR             S0, [X20]
0009A9CC  e11740f9  LDR             X1, [SP,#0x30+var_8]
0009A9D0  600240f9  LDR             X0, [X19]
0009A9D4  200000ca  EOR             X0, X1, X0
0009A9D8  600100b5  CBNZ            X0, loc_9AA04
0009A9DC  00008052  MOV             W0, #0
0009A9E0  f35341a9  LDP             X19, X20, [SP,#0x30+var_20]
0009A9E4  fd7bc3a8  LDP             X29, X30, [SP+0x30+var_30],#0x30
0009A9E8  c0035fd6  RET
0009A9EC  2128211e  FADD            S1, S1, S1; y
0009A9F0  0090241e  FMOV            S0, #10.0; x
0009A9F4  e756ff97  BL              .powf
0009A9F8  0190241e  FMOV            S1, #10.0
0009A9FC  0008211e  FMUL            S0, S0, S1
0009AA00  eeffff17  B               loc_9A9B8
0009AA04  4354ff97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009aa10  size 1232 bytes  305 insns
 *   APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool, apac::aligned_array<unsigned short, 5ul> const&, apac::aligned_array<float, 1024ul>&)
 *   [_ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE]
 *   data refs: 0x79810, 0x317e90, 0x31be90, 0x31de90, 0x31fe90, 0x323e90, 0x327e90, 0x32be90, 0x42a828, 0x42a848, 0x42b0b0, 0x42b158, 0x42b700, 0x42b750, 0x42b798, 0x42cb80, 0x42ce00, 0x42d070, 0x42d128, 0x42d138, 0x42d2a8, 0x42d5b0, 0x42d7d8, 0x42d8c8
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
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


#if 0 /* disassembly of 0x0009aa10, for verification */
0009AA10  e91340d1  SUB             X9, SP, #4,LSL#12
0009AA14  3ff102f9  STR             XZR, [X9,#0x5E0]
0009AA18  ff8328d1  SUB             SP, SP, #0xA20
0009AA1C  841c00f0  ADRP            X4, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB21E_ptr@PAGE
0009AA20  1f1c0072  TST             W0, #0xFF
0009AA24  fd7b00a9  STP             X29, X30, [SP,#0xA20+var_A20]
0009AA28  fd030091  MOV             X29, SP
0009AA2C  843446f9  LDR             X4, [X4,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB21E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB21 ...
0009AA30  f55b02a9  STP             X21, X22, [SP,#0xA20+var_A00]
0009AA34  961c00d0  ADRP            X22, #__stack_chk_guard_ptr@PAGE
0009AA38  f35301a9  STP             X19, X20, [SP,#0xA20+var_A10]
0009AA3C  f40301aa  MOV             X20, X1
0009AA40  f30302aa  MOV             X19, X2
0009AA44  c30247f9  LDR             X3, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009AA48  f76303a9  STP             X23, X24, [SP,#0xA20+var_9F0]
0009AA4C  f5030691  ADD             X21, SP, #0xA20+var_8A0
0009AA50  f92300f9  STR             X25, [SP,#0xA20+var_9E0]
0009AA54  ff7f18a9  STP             XZR, XZR, [SP,#0xA20+var_8A0]
0009AA58  21084079  LDRH            W1, [X1,#4]
0009AA5C  650040f9  LDR             X5, [X3]
0009AA60  e50f05f9  STR             X5, [SP,#0xA20+var_8]
0009AA64  050080d2  MOV             X5, #0
0009AA68  83024079  LDRH            W3, [X20]
0009AA6C  ff7f19a9  STP             XZR, XZR, [SP,#0xA20+var_890]
0009AA70  213c7cd3  UBFIZ           X1, X1, #4, #0x10
0009AA74  ff7f1aa9  STP             XZR, XZR, [SP,#0xA20+var_880]
0009AA78  2200048b  ADD             X2, X1, X4
0009AA7C  603c7bd3  UBFIZ           X0, X3, #5, #0x10
0009AA80  ff7f1ba9  STP             XZR, XZR, [SP,#0xA20+var_870]
0009AA84  411a0054  B.NE            loc_9ADCC
0009AA88  971c00f0  ADRP            X23, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB11E_ptr@PAGE
0009AA8C  250080d2  MOV             X5, #1
0009AA90  e40315aa  MOV             X4, X21
0009AA94  e30305aa  MOV             X3, X5
0009AA98  f79e40f9  LDR             X23, [X23,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB11E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB11 ...
0009AA9C  e10305aa  MOV             X1, X5
0009AAA0  860080d2  MOV             X6, #4
0009AAA4  e002008b  ADD             X0, X23, X0
0009AAA8  ee530094  BL              vDSP_vadd
0009AAAC  80024079  LDRH            W0, [X20]
0009AAB0  821c00f0  ADRP            X2, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB22E_ptr@PAGE
0009AAB4  810e4079  LDRH            W1, [X20,#6]
0009AAB8  250080d2  MOV             X5, #1
0009AABC  42d044f9  LDR             X2, [X2,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB22E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB22 ...
0009AAC0  00701d53  LSL             W0, W0, #3
0009AAC4  00100011  ADD             W0, W0, #4
0009AAC8  213c7cd3  UBFIZ           X1, X1, #4, #0x10
0009AACC  4200018b  ADD             X2, X2, X1
0009AAD0  e30305aa  MOV             X3, X5
0009AAD4  007c4093  SXTW            X0, W0
0009AAD8  e10305aa  MOV             X1, X5
0009AADC  a4420091  ADD             X4, X21, #0x10
0009AAE0  860080d2  MOV             X6, #4
0009AAE4  e00a008b  ADD             X0, X23, X0,LSL#2
0009AAE8  de530094  BL              vDSP_vadd
0009AAEC  81124079  LDRH            W1, [X20,#8]
0009AAF0  821c00f0  ADRP            X2, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB23E_ptr@PAGE
0009AAF4  841c00f0  ADRP            X4, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB12E_ptr@PAGE
0009AAF8  80064079  LDRH            W0, [X20,#2]
0009AAFC  42d842f9  LDR             X2, [X2,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB23E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB23 ...
0009AB00  250080d2  MOV             X5, #1
0009AB04  84d047f9  LDR             X4, [X4,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB12E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB12 ...
0009AB08  e30305aa  MOV             X3, X5
0009AB0C  213c7bd3  UBFIZ           X1, X1, #5, #0x10
0009AB10  003c7bd3  UBFIZ           X0, X0, #5, #0x10
0009AB14  981c00f0  ADRP            X24, #_ZTVN15APACLRVQDecoder7LRVQLPCE_ptr@PAGE
0009AB18  4200018b  ADD             X2, X2, X1
0009AB1C  8000008b  ADD             X0, X4, X0
0009AB20  e10305aa  MOV             X1, X5
0009AB24  a4820091  ADD             X4, X21, #0x20 ; ' '
0009AB28  060180d2  MOV             X6, #8
0009AB2C  cd530094  BL              vDSP_vadd
0009AB30  033b45f9  LDR             X3, [X24,#_ZTVN15APACLRVQDecoder7LRVQLPCE_ptr@PAGEOFF]; `vtable for'APACLRVQDecoder::LRVQLPC ...
0009AB34  060280d2  MOV             X6, #0x400010
0009AB3C  040090d2  MOV             X4, #0x46BB8000
0009AB44  63400091  ADD             X3, X3, #0x10
0009AB48  2600c0f2  MOVK            X6, #1,LSL#32
0009AB4C  05028052  MOV             W5, #0x10
0009AB50  0449e8f2  MOVK            X4, #0x4248,LSL#48
0009AB54  f9430291  ADD             X25, SP, #0xA20+var_990
0009AB58  e10315aa  MOV             X1, X21; float *
0009AB5C  e00319aa  MOV             X0, X25; this
0009AB60  e2030791  ADD             X2, SP, #0xA20+var_860; float *
0009AB64  e31b09a9  STP             X3, X6, [SP,#0xA20+var_990]
0009AB68  ff430179  STRH            WZR, [SP,#0xA20+var_980]
0009AB6C  ffff0aa9  STP             XZR, XZR, [SP,#0xA20+var_978]
0009AB70  ffff0ba9  STP             XZR, XZR, [SP,#0xA20+var_968]
0009AB74  ffff0ca9  STP             XZR, XZR, [SP,#0xA20+var_958]
0009AB78  ffff0da9  STP             XZR, XZR, [SP,#0xA20+var_948]
0009AB7C  ffff0ea9  STP             XZR, XZR, [SP,#0xA20+var_938]
0009AB80  e5fb00b9  STR             W5, [SP,#0xA20+var_928]
0009AB84  fffb0179  STRH            WZR, [SP,#0xA20+var_924]
0009AB88  e47f10a9  STP             X4, XZR, [SP,#0xA20+var_920]
0009AB8C  ff7f11a9  STP             XZR, XZR, [SP,#0xA20+var_910]
0009AB90  ff7f12a9  STP             XZR, XZR, [SP,#0xA20+var_900]
0009AB94  ff7f13a9  STP             XZR, XZR, [SP,#0xA20+var_8F0]
0009AB98  ff7f14a9  STP             XZR, XZR, [SP,#0xA20+var_8E0]
0009AB9C  ff7f15a9  STP             XZR, XZR, [SP,#0xA20+var_8D0]
0009ABA0  ff7f16a9  STP             XZR, XZR, [SP,#0xA20+var_8C0]
0009ABA4  ff7f17a9  STP             XZR, XZR, [SP,#0xA20+var_8B0]
0009ABA8  f2690094  BL              _ZN4apac8LSFTools10LSF_To_LPCEPfS1_; apac::LSFTools::LSF_To_LPC(float *,float *)
0009ABAC  f4430891  ADD             X20, SP, #0xA20+var_810
0009ABB0  80120191  ADD             X0, X20, #0x44 ; 'D'
0009ABB4  81022091  ADD             X1, X20, #0x800
0009ABB8  1f4400b8  STR             WZR, [X0],#4
0009ABBC  1f0001eb  CMP             X0, X1
0009ABC0  c1ffff54  B.NE            loc_9ABB8
0009ABC4  951c00f0  ADRP            X21, #_ZTV17MultiRadixRealFFT_ptr@PAGE
0009ABC8  8a1c00d0  ADRP            X10, #_ZTVN15APACLRVQDecoder11LPMDCT_LONGE_ptr@PAGE
0009ABCC  e61f5ca9  LDP             X6, X7, [SP,#0xA20+var_860]
0009ABD0  0d408052  MOV             W13, #0x100200
0009ABD8  ec0342b9  LDR             W12, [SP,#0xA20+var_820]
0009ABDC  abf645f9  LDR             X11, [X21,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009ABE0  861e00a9  STP             X6, X7, [X20]
0009ABE4  f7630191  ADD             X23, SP, #0xA20+var_9C8
0009ABE8  4ac145f9  LDR             X10, [X10,#_ZTVN15APACLRVQDecoder11LPMDCT_LONGE_ptr@PAGEOFF]; `vtable for'APACLRVQDecoder::LPMDCT_LONG ...
0009ABEC  6b410091  ADD             X11, X11, #0x10
0009ABF0  e4175da9  LDP             X4, X5, [SP,#0xA20+var_850]
0009ABF4  4a410091  ADD             X10, X10, #0x10
0009ABF8  e20f5ea9  LDP             X2, X3, [SP,#0xA20+var_840]
0009ABFC  841601a9  STP             X4, X5, [X20,#0x10]
0009AC00  e0420091  ADD             X0, X23, #0x10
0009AC04  e8275fa9  LDP             X8, X9, [SP,#0xA20+var_830]
0009AC08  820e02a9  STP             X2, X3, [X20,#0x20]
0009AC0C  01408052  MOV             W1, #0x200
0009AC10  882603a9  STP             X8, X9, [X20,#0x30]
0009AC14  8c4200b9  STR             W12, [X20,#0x40]
0009AC18  ea2f00f9  STR             X10, [SP,#0xA20+var_9C8]
0009AC1C  ed6300b9  STR             W13, [SP,#0xA20+var_9C0]
0009AC20  eb3700f9  STR             X11, [SP,#0xA20+var_9B8]
0009AC24  ff7300b9  STR             WZR, [SP,#0xA20+var_9B0]
0009AC28  ffff07a9  STP             XZR, XZR, [SP,#0xA20+var_9A8]
0009AC2C  e1a40194  BL              _ZN9DFTHandleILb0EE10InitializeEj; DFTHandle<false>::Initialize(uint)
0009AC30  e1c3407d  LDR             H1, [SP,#0xA20+var_9C0]
0009AC34  0010201e  FMOV            S0, #2.0
0009AC38  e10314aa  MOV             X1, X20; float *
0009AC3C  e20313aa  MOV             X2, X19; float *
0009AC40  e00317aa  MOV             X0, X23; this
0009AC44  21d8215e  SCVTF           S1, S1
0009AC48  03208052  MOV             W3, #0x100; unsigned int
0009AC4C  0018211e  FDIV            S0, S0, S1
0009AC50  e08b00bd  STR             S0, [SP,#0xA20+var_998]
0009AC54  23680094  BL              _ZN4apac12LPMDCTFilter23LPImpulseToSpecEnvelopeEPKfPfj; apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*,float *,uint)
0009AC58  60021091  ADD             X0, X19, #0x400
0009AC5C  73064091  ADD             X19, X19, #1,LSL#12
0009AC60  1f4400b8  STR             WZR, [X0],#4
0009AC64  7f0200eb  CMP             X19, X0
0009AC68  c1ffff54  B.NE            loc_9AC60
0009AC6C  811c00f0  ADRP            X1, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009AC70  e0420091  ADD             X0, X23, #0x10
0009AC74  b5f645f9  LDR             X21, [X21,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009AC78  215441f9  LDR             X1, [X1,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009AC7C  b5420091  ADD             X21, X21, #0x10
0009AC80  f53700f9  STR             X21, [SP,#0xA20+var_9B8]
0009AC84  21400091  ADD             X1, X1, #0x10
0009AC88  e12f00f9  STR             X1, [SP,#0xA20+var_9C8]
0009AC8C  cda20194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009AC90  811c00f0  ADRP            X1, #_ZTVN4apac8LSFToolsE_ptr@PAGE
0009AC94  e0b740f9  LDR             X0, [SP,#0xA20+var_8B8]; void *
0009AC98  213840f9  LDR             X1, [X1,#_ZTVN4apac8LSFToolsE_ptr@PAGEOFF]; `vtable for'apac::LSFTools ...
0009AC9C  21400091  ADD             X1, X1, #0x10
0009ACA0  e14b00f9  STR             X1, [SP,#0xA20+var_990]
0009ACA4  400000b4  CBZ             X0, loc_9ACAC
0009ACA8  fe54ff97  BL              ._ZdlPv; operator delete(void *)
0009ACAC  e0ab40f9  LDR             X0, [SP,#0xA20+var_8D0]; void *
0009ACB0  400000b4  CBZ             X0, loc_9ACB8
0009ACB4  fb54ff97  BL              ._ZdlPv; operator delete(void *)
0009ACB8  e09f40f9  LDR             X0, [SP,#0xA20+var_8E8]; void *
0009ACBC  400000b4  CBZ             X0, loc_9ACC4
0009ACC0  f854ff97  BL              ._ZdlPv; operator delete(void *)
0009ACC4  e09340f9  LDR             X0, [SP,#0xA20+var_900]; void *
0009ACC8  400000b4  CBZ             X0, loc_9ACD0
0009ACCC  f554ff97  BL              ._ZdlPv; operator delete(void *)
0009ACD0  e08740f9  LDR             X0, [SP,#0xA20+var_918]; void *
0009ACD4  400000b4  CBZ             X0, loc_9ACDC
0009ACD8  f254ff97  BL              ._ZdlPv; operator delete(void *)
0009ACDC  801c00f0  ADRP            X0, #_ZTVN4apac7LPCBaseE_ptr@PAGE
0009ACE0  f3574ea9  LDP             X19, X21, [SP,#0xA20+var_940]
0009ACE4  00ec45f9  LDR             X0, [X0,#_ZTVN4apac7LPCBaseE_ptr@PAGEOFF]; `vtable for'apac::LPCBase ...
0009ACE8  00400091  ADD             X0, X0, #0x10
0009ACEC  e04b00f9  STR             X0, [SP,#0xA20+var_990]
0009ACF0  7f0215eb  CMP             X19, X21
0009ACF4  e0010054  B.EQ            loc_9AD30
0009ACF8  740240f9  LDR             X20, [X19]
0009ACFC  340100b4  CBZ             X20, loc_9AD20
0009AD00  801a40f9  LDR             X0, [X20,#0x30]; void *
0009AD04  400000b4  CBZ             X0, loc_9AD0C
0009AD08  e654ff97  BL              ._ZdlPv; operator delete(void *)
0009AD0C  800a40f9  LDR             X0, [X20,#0x10]; void *
0009AD10  400000b4  CBZ             X0, loc_9AD18
0009AD14  e354ff97  BL              ._ZdlPv; operator delete(void *)
0009AD18  e00314aa  MOV             X0, X20; void *
0009AD1C  e154ff97  BL              ._ZdlPv; operator delete(void *)
0009AD20  73220091  ADD             X19, X19, #8
0009AD24  bf0213eb  CMP             X21, X19
0009AD28  81feff54  B.NE            loc_9ACF8
0009AD2C  f57340f9  LDR             X21, [SP,#0xA20+var_940]
0009AD30  750000b4  CBZ             X21, loc_9AD3C
0009AD34  e00315aa  MOV             X0, X21; void *
0009AD38  da54ff97  BL              ._ZdlPv; operator delete(void *)
0009AD3C  f36f40f9  LDR             X19, [SP,#0xA20+var_948]
0009AD40  130200b4  CBZ             X19, loc_9AD80
0009AD44  610240f9  LDR             X1, [X19]
0009AD48  801c00f0  ADRP            X0, #_ZN4apac8AutocorrD0Ev_ptr@PAGE
0009AD4C  00e046f9  LDR             X0, [X0,#_ZN4apac8AutocorrD0Ev_ptr@PAGEOFF]; apac::Autocorr::~Autocorr() ...
0009AD50  210440f9  LDR             X1, [X1,#8]
0009AD54  3f0000eb  CMP             X1, X0
0009AD58  01080054  B.NE            loc_9AE58
0009AD5C  811c00f0  ADRP            X1, #_ZTVN4apac8AutocorrE_ptr@PAGE
0009AD60  600a40f9  LDR             X0, [X19,#0x10]; void *
0009AD64  21ec43f9  LDR             X1, [X1,#_ZTVN4apac8AutocorrE_ptr@PAGEOFF]; `vtable for'apac::Autocorr ...
0009AD68  21400091  ADD             X1, X1, #0x10
0009AD6C  610200f9  STR             X1, [X19]
0009AD70  400000b4  CBZ             X0, loc_9AD78
0009AD74  cb54ff97  BL              ._ZdlPv; operator delete(void *)
0009AD78  e00313aa  MOV             X0, X19; void *
0009AD7C  c954ff97  BL              ._ZdlPv; operator delete(void *)
0009AD80  e06340f9  LDR             X0, [SP,#0xA20+var_960]; void *
0009AD84  400000b4  CBZ             X0, loc_9AD8C
0009AD88  c654ff97  BL              ._ZdlPv; operator delete(void *)
0009AD8C  e05740f9  LDR             X0, [SP,#0xA20+var_978]; void *
0009AD90  400000b4  CBZ             X0, loc_9AD98
0009AD94  c354ff97  BL              ._ZdlPv; operator delete(void *)
0009AD98  d60247f9  LDR             X22, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009AD9C  e10f45f9  LDR             X1, [SP,#0xA20+var_8]
0009ADA0  c00240f9  LDR             X0, [X22]
0009ADA4  200000ca  EOR             X0, X1, X0
0009ADA8  e00500b5  CBNZ            X0, loc_9AE64
0009ADAC  00008052  MOV             W0, #0
0009ADB0  fd7b40a9  LDP             X29, X30, [SP,#0xA20+var_A20]
0009ADB4  f35341a9  LDP             X19, X20, [SP,#0xA20+var_A10]
0009ADB8  f55b42a9  LDP             X21, X22, [SP,#0xA20+var_A00]
0009ADBC  f76343a9  LDP             X23, X24, [SP,#0xA20+var_9F0]
0009ADC0  f92340f9  LDR             X25, [SP,#0xA20+var_9E0]
0009ADC4  ff832891  ADD             SP, SP, #0xA20
0009ADC8  c0035fd6  RET
0009ADCC  971c00f0  ADRP            X23, #_ZN16LrvqLSFCodebooks29kCodebook_Lrvq_Lsf_CB11_SHORTE_ptr@PAGE
0009ADD0  250080d2  MOV             X5, #1
0009ADD4  e40315aa  MOV             X4, X21
0009ADD8  e30305aa  MOV             X3, X5
0009ADDC  f79640f9  LDR             X23, [X23,#_ZN16LrvqLSFCodebooks29kCodebook_Lrvq_Lsf_CB11_SHORTE_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB11_SHORT ...
0009ADE0  e10305aa  MOV             X1, X5
0009ADE4  860080d2  MOV             X6, #4
0009ADE8  e002008b  ADD             X0, X23, X0
0009ADEC  1d530094  BL              vDSP_vadd
0009ADF0  80024079  LDRH            W0, [X20]
0009ADF4  821c00f0  ADRP            X2, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB22E_ptr@PAGE
0009ADF8  810e4079  LDRH            W1, [X20,#6]
0009ADFC  250080d2  MOV             X5, #1
0009AE00  42d044f9  LDR             X2, [X2,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB22E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB22 ...
0009AE04  00701d53  LSL             W0, W0, #3
0009AE08  00100011  ADD             W0, W0, #4
0009AE0C  213c7cd3  UBFIZ           X1, X1, #4, #0x10
0009AE10  4200018b  ADD             X2, X2, X1
0009AE14  e30305aa  MOV             X3, X5
0009AE18  007c4093  SXTW            X0, W0
0009AE1C  e10305aa  MOV             X1, X5
0009AE20  a4420091  ADD             X4, X21, #0x10
0009AE24  860080d2  MOV             X6, #4
0009AE28  e00a008b  ADD             X0, X23, X0,LSL#2
0009AE2C  0d530094  BL              vDSP_vadd
0009AE30  81124079  LDRH            W1, [X20,#8]
0009AE34  821c00f0  ADRP            X2, #_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB23E_ptr@PAGE
0009AE38  841c00f0  ADRP            X4, #_ZN16LrvqLSFCodebooks29kCodebook_Lrvq_Lsf_CB12_SHORTE_ptr@PAGE
0009AE3C  250080d2  MOV             X5, #1
0009AE40  80064079  LDRH            W0, [X20,#2]
0009AE44  e30305aa  MOV             X3, X5
0009AE48  42d842f9  LDR             X2, [X2,#_ZN16LrvqLSFCodebooks23kCodebook_Lrvq_Lsf_CB23E_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB23 ...
0009AE4C  213c7bd3  UBFIZ           X1, X1, #5, #0x10
0009AE50  846444f9  LDR             X4, [X4,#_ZN16LrvqLSFCodebooks29kCodebook_Lrvq_Lsf_CB12_SHORTE_ptr@PAGEOFF]; LrvqLSFCodebooks::kCodebook_Lrvq_Lsf_CB12_SHORT ...
0009AE54  2fffff17  B               loc_9AB10
0009AE58  e00313aa  MOV             X0, X19
0009AE5C  20003fd6  BLR             X1
0009AE60  c8ffff17  B               loc_9AD80
0009AE64  2b53ff97  BL              .__stack_chk_fail
0009AE68  811c00f0  ADRP            X1, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009AE6C  f30300aa  MOV             X19, X0
0009AE70  a2f645f9  LDR             X2, [X21,#0xBE8]
0009AE74  e0420091  ADD             X0, X23, #0x10
0009AE78  215441f9  LDR             X1, [X1,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009AE7C  42400091  ADD             X2, X2, #0x10
0009AE80  e23700f9  STR             X2, [SP,#0xA20+var_9B8]
0009AE84  21400091  ADD             X1, X1, #0x10
0009AE88  e12f00f9  STR             X1, [SP,#0xA20+var_9C8]
0009AE8C  4da20194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009AE90  013b45f9  LDR             X1, [X24,#0xA70]
0009AE94  e00319aa  MOV             X0, X25; this
0009AE98  21400091  ADD             X1, X1, #0x10
0009AE9C  e14b00f9  STR             X1, [SP,#0xA20+var_990]
0009AEA0  a8070094  BL              _ZN4apac8LSFToolsD2Ev; apac::LSFTools::~LSFTools()
0009AEA4  e00313aa  MOV             X0, X19; struct _Unwind_Exception *
0009AEA8  2656ff97  BL              ._Unwind_Resume
0009AEAC  f30300aa  MOV             X19, X0
0009AEB0  f8ffff17  B               loc_9AE90
0009AEB4  821c00f0  ADRP            X2, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009AEB8  f30300aa  MOV             X19, X0
0009AEBC  a1f645f9  LDR             X1, [X21,#0xBE8]
0009AEC0  e0420091  ADD             X0, X23, #0x10
0009AEC4  425441f9  LDR             X2, [X2,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009AEC8  21400091  ADD             X1, X1, #0x10
0009AECC  e13700f9  STR             X1, [SP,#0xA20+var_9B8]
0009AED0  42400091  ADD             X2, X2, #0x10
0009AED4  e22f00f9  STR             X2, [SP,#0xA20+var_9C8]
0009AED8  3aa20194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009AEDC  edffff17  B               loc_9AE90
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009aee0  size 536 bytes  132 insns
 *   APACLRVQDecoder::ltpToMdctSpectralEnvelope(unsigned short, unsigned short, apac::aligned_array<float, 1024ul>&)
 *   [_ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE]
 *   data refs: 0x42a828, 0x42b1a8, 0x42b700, 0x42ce00, 0x42d2a8, 0x42d5e8, 0x42dbe8, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
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


#if 0 /* disassembly of 0x0009aee0, for verification */
0009AEE0  e91340d1  SUB             X9, SP, #4,LSL#12
0009AEE4  290540d1  SUB             X9, X9, #1,LSL#12
0009AEE8  3f0108f9  STR             XZR, [X9,#0x1000]
0009AEEC  3f0100f9  STR             XZR, [X9]
0009AEF0  3f0117f8  STUR            XZR, [X9,#-0x90]
0009AEF4  0c1284d2  MOV             X12, #0x2090
0009AEF8  ff632ccb  SUB             SP, SP, X12
0009AEFC  003c0012  AND             W0, W0, #0xFFFF
0009AF00  c35fa852  MOV             W3, #0x42FE0000
0009AF04  02102a1e  FMOV            S2, #0.25
0009AF08  213c0012  AND             W1, W1, #0xFFFF
0009AF0C  fd7b00a9  STP             X29, X30, [SP,#0x2090+var_2090]
0009AF10  0000231e  UCVTF           S0, W0
0009AF14  fd030091  MOV             X29, SP
0009AF18  e81f00fd  STR             D8, [SP,#0x2090+var_2058]
0009AF1C  6800271e  FMOV            S8, W3
0009AF20  83148e52  MOV             W3, #0x3F3D70A4
0009AF28  0018281e  FDIV            S0, S0, S8
0009AF2C  6100271e  FMOV            S1, W3
0009AF30  f35301a9  STP             X19, X20, [SP,#0x2090+var_2080]
0009AF34  941c00d0  ADRP            X20, #__stack_chk_guard_ptr@PAGE
0009AF38  800247f9  LDR             X0, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009AF3C  f55b02a9  STP             X21, X22, [SP,#0x2090+var_2070]
0009AF40  f50302aa  MOV             X21, X2
0009AF44  f71b00f9  STR             X23, [SP,#0x2090+var_2060]
0009AF48  f7030291  ADD             X23, SP, #0x2090+var_2010
0009AF4C  e30a4091  ADD             X3, X23, #2,LSL#12
0009AF50  020040f9  LDR             X2, [X0]
0009AF54  e24710f9  STR             X2, [SP,#0x2090+var_8]
0009AF58  020080d2  MOV             X2, #0
0009AF5C  e00317aa  MOV             X0, X23
0009AF60  0808011f  FMADD           S8, S0, S1, S2
0009AF64  1f2003d5  NOP
0009AF68  1f4400b8  STR             WZR, [X0],#4
0009AF6C  1f0003eb  CMP             X0, X3
0009AF70  c1ffff54  B.NE            loc_9AF68
0009AF74  931c00f0  ADRP            X19, #_ZTV17MultiRadixRealFFT_ptr@PAGE
0009AF78  831c00f0  ADRP            X3, #_ZTVN15APACLRVQDecoder10LPMDCT_LTPE_ptr@PAGE
0009AF7C  0041211e  FNEG            S0, S8
0009AF80  223c4092  AND             X2, X1, #0xFFFF
0009AF84  64f645f9  LDR             X4, [X19,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009AF88  01102e1e  FMOV            S1, #1.0
0009AF8C  63f442f9  LDR             X3, [X3,#_ZTVN15APACLRVQDecoder10LPMDCT_LTPE_ptr@PAGEOFF]; `vtable for'APACLRVQDecoder::LPMDCT_LTP ...
0009AF90  84400091  ADD             X4, X4, #0x10
0009AF94  05008152  MOV             W5, #0x100800
0009AF9C  63400091  ADD             X3, X3, #0x10
0009AFA0  f6230191  ADD             X22, SP, #0x2090+var_2048
0009AFA4  e18300bd  STR             S1, [SP,#0x2090+var_2010]
0009AFA8  c0420091  ADD             X0, X22, #0x10
0009AFAC  01008152  MOV             W1, #0x800
0009AFB0  e07a22bc  STR             S0, [X23,X2,LSL#2]
0009AFB4  e32700f9  STR             X3, [SP,#0x2090+var_2048]
0009AFB8  e55300b9  STR             W5, [SP,#0x2090+var_2040]
0009AFBC  e42f00f9  STR             X4, [SP,#0x2090+var_2038]
0009AFC0  ff6300b9  STR             WZR, [SP,#0x2090+var_2030]
0009AFC4  ffff06a9  STP             XZR, XZR, [SP,#0x2090+var_2028]
0009AFC8  faa30194  BL              _ZN9DFTHandleILb0EE10InitializeEj; DFTHandle<false>::Initialize(uint)
0009AFCC  e1a3407d  LDR             H1, [SP,#0x2090+var_2040]
0009AFD0  0010201e  FMOV            S0, #2.0
0009AFD4  e10317aa  MOV             X1, X23; float *
0009AFD8  e20315aa  MOV             X2, X21; float *
0009AFDC  e00316aa  MOV             X0, X22; this
0009AFE0  21d8215e  SCVTF           S1, S1
0009AFE4  03208052  MOV             W3, #0x100; unsigned int
0009AFE8  0018211e  FDIV            S0, S0, S1
0009AFEC  e07b00bd  STR             S0, [SP,#0x2090+var_2018]
0009AFF0  3c670094  BL              _ZN4apac12LPMDCTFilter23LPImpulseToSpecEnvelopeEPKfPfj; apac::LPMDCTFilter::LPImpulseToSpecEnvelope(float const*,float *,uint)
0009AFF4  a0021091  ADD             X0, X21, #0x400
0009AFF8  a1064091  ADD             X1, X21, #1,LSL#12
0009AFFC  1f2003d5  NOP
0009B000  1f4400b8  STR             WZR, [X0],#4
0009B004  1f0001eb  CMP             X0, X1
0009B008  c1ffff54  B.NE            loc_9B000
0009B00C  00102e1e  FMOV            S0, #1.0
0009B010  240080d2  MOV             X4, #1
0009B014  e30315aa  MOV             X3, X21
0009B018  e10304aa  MOV             X1, X4
0009B01C  0829201e  FADD            S8, S8, S0
0009B020  e00315aa  MOV             X0, X21
0009B024  e2130191  ADD             X2, SP, #0x2090+var_204C
0009B028  052080d2  MOV             X5, #0x100
0009B02C  0018281e  FDIV            S0, S0, S8
0009B030  e04700bd  STR             S0, [SP,#0x2090+var_204C]
0009B034  2b530094  BL              vDSP_vsmul
0009B038  811c00d0  ADRP            X1, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009B03C  c0420091  ADD             X0, X22, #0x10
0009B040  73f645f9  LDR             X19, [X19,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009B044  215441f9  LDR             X1, [X1,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009B048  73420091  ADD             X19, X19, #0x10
0009B04C  f32f00f9  STR             X19, [SP,#0x2090+var_2038]
0009B050  21400091  ADD             X1, X1, #0x10
0009B054  e12700f9  STR             X1, [SP,#0x2090+var_2048]
0009B058  daa10194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009B05C  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009B060  e14750f9  LDR             X1, [SP,#0x2090+var_8]
0009B064  800240f9  LDR             X0, [X20]
0009B068  200000ca  EOR             X0, X1, X0
0009B06C  400100b5  CBNZ            X0, loc_9B094
0009B070  00008052  MOV             W0, #0
0009B074  0c1284d2  MOV             X12, #0x2090
0009B078  fd7b40a9  LDP             X29, X30, [SP,#0x2090+var_2090]
0009B07C  f35341a9  LDP             X19, X20, [SP,#0x2090+var_2080]
0009B080  f55b42a9  LDP             X21, X22, [SP,#0x2090+var_2070]
0009B084  f71b40f9  LDR             X23, [SP,#0x2090+var_2060]
0009B088  e81f40fd  LDR             D8, [SP,#0x2090+var_2058]
0009B08C  ff632c8b  ADD             SP, SP, X12
0009B090  c0035fd6  RET
0009B094  9f52ff97  BL              .__stack_chk_fail
0009B098  821c00d0  ADRP            X2, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009B09C  61f645f9  LDR             X1, [X19,#0xBE8]
0009B0A0  f30300aa  MOV             X19, X0
0009B0A4  425441f9  LDR             X2, [X2,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009B0A8  21400091  ADD             X1, X1, #0x10
0009B0AC  e12f00f9  STR             X1, [SP,#0x2090+var_2038]
0009B0B0  c0420091  ADD             X0, X22, #0x10
0009B0B4  41400091  ADD             X1, X2, #0x10
0009B0B8  e12700f9  STR             X1, [SP,#0x2090+var_2048]
0009B0BC  c1a10194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009B0C0  e00313aa  MOV             X0, X19; struct _Unwind_Exception *
0009B0C4  9f55ff97  BL              ._Unwind_Resume
0009B0C8  811c00d0  ADRP            X1, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009B0CC  f40300aa  MOV             X20, X0
0009B0D0  62f645f9  LDR             X2, [X19,#0xBE8]
0009B0D4  c0420091  ADD             X0, X22, #0x10
0009B0D8  215441f9  LDR             X1, [X1,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009B0DC  42400091  ADD             X2, X2, #0x10
0009B0E0  e22f00f9  STR             X2, [SP,#0x2090+var_2038]
0009B0E4  21400091  ADD             X1, X1, #0x10
0009B0E8  e12700f9  STR             X1, [SP,#0x2090+var_2048]
0009B0EC  b5a10194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009B0F0  e00314aa  MOV             X0, X20; struct _Unwind_Exception *
0009B0F4  9355ff97  BL              ._Unwind_Resume
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009b100  size 3264 bytes  811 insns
 *   APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<unsigned int>&, bool, bool, APACSpectralData&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 1024ul>&, apac::aligned_array<float, 256ul>&, apac::aligned_array<short, 64ul>&, unsigned short*)
 *   [_ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt]
 *   data refs: 0x32fef8, 0x32fefc, 0x32ff00, 0x42b1f8, 0x42ce00, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
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


#if 0 /* disassembly of 0x0009b100, for verification */
0009B100  e91340d1  SUB             X9, SP, #4,LSL#12
0009B104  3f8107f9  STR             XZR, [X9,#0xF00]
0009B108  fd7bb0a9  STP             X29, X30, [SP,#var_100]!
0009B10C  211c0012  AND             W1, W1, #0xFF
0009B110  fd030091  MOV             X29, SP
0009B114  f55b02a9  STP             X21, X22, [SP,#0x100+var_E0]
0009B118  961c00b0  ADRP            X22, #__stack_chk_guard_ptr@PAGE
0009B11C  c90247f9  LDR             X9, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009B120  f35301a9  STP             X19, X20, [SP,#0x100+var_F0]
0009B124  f30300aa  MOV             X19, X0
0009B128  f76303a9  STP             X23, X24, [SP,#0x100+var_D0]
0009B12C  f40306aa  MOV             X20, X6
0009B130  f70303aa  MOV             X23, X3
0009B134  f96b04a9  STP             X25, X26, [SP,#0x100+var_C0]
0009B138  fb7305a9  STP             X27, X28, [SP,#0x100+var_B0]
0009B13C  5b1c0012  AND             W27, W2, #0xFF
0009B140  280140f9  LDR             X8, [X9]
0009B144  e87f00f9  STR             X8, [SP,#0x100+var_8]
0009B148  080080d2  MOV             X8, #0
0009B14C  e43300f9  STR             X4, [SP,#0x100+var_A0]
0009B150  09284229  LDP             W9, W10, [X0,#0x10]
0009B154  e16b00b9  STR             W1, [SP,#0x100+var_98]
0009B158  e79707a9  STP             X7, X5, [SP,#0x100+var_88]
0009B15C  4b050051  SUB             W11, W10, #1
0009B160  0b1400b9  STR             W11, [X0,#0x14]
0009B164  357d1f53  LSR             W21, W9, #0x1F
0009B168  f98340f9  LDR             X25, [SP,#0x100+arg_0]
0009B16C  8b20f837  TBNZ            W11, #0x1F, loc_9B57C
0009B170  2901090b  ADD             W9, W9, W9
0009B174  40290051  SUB             W0, W10, #0xA
0009B178  691200b9  STR             W9, [X19,#0x10]
0009B17C  b53e0012  AND             W21, W21, #0xFFFF
0009B180  601600b9  STR             W0, [X19,#0x14]
0009B184  3c7d1753  LSR             W28, W9, #0x17
0009B188  0024f837  TBNZ            W0, #0x1F, loc_9B608
0009B18C  29591753  LSL             W9, W9, #9
0009B190  691200b9  STR             W9, [X19,#0x10]
0009B194  fa730391  ADD             X26, SP, #0x100+var_24
0009B198  e103152a  MOV             W1, W21; unsigned __int16
0009B19C  e0031c2a  MOV             W0, W28; this
0009B1A0  e2031aaa  MOV             X2, X26; unsigned __int16
0009B1A4  ebfdff97  BL              _ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf; APACLRVQDecoder::dequantizeLrvqGain(ushort,ushort,float *)
0009B1A8  f503002a  MOV             W21, W0
0009B1AC  e0190035  CBNZ            W0, loc_9B4E8
0009B1B0  ff6f00b9  STR             WZR, [SP,#0x100+var_94]
0009B1B4  1c008052  MOV             W28, #0
0009B1B8  60064229  LDP             W0, W1, [X19,#0x10]
0009B1BC  1b1b0034  CBZ             W27, loc_9B51C
0009B1C0  f8830391  ADD             X24, SP, #0x100+var_20
0009B1C4  21240051  SUB             W1, W1, #9
0009B1C8  611600b9  STR             W1, [X19,#0x14]
0009B1CC  042b0091  ADD             X4, X24, #0xA
0009B1D0  f57300b9  STR             W21, [SP,#0x100+var_90]
0009B1D4  f50318aa  MOV             X21, X24
0009B1D8  f7d308a9  STP             X23, X20, [SP,#0x100+var_78]
0009B1DC  f40304aa  MOV             X20, X4
0009B1E0  177c1753  LSR             W23, W0, #0x17
0009B1E4  6101f837  TBNZ            W1, #0x1F, loc_9B210
0009B1E8  00581753  LSL             W0, W0, #9
0009B1EC  b7260078  STRH            W23, [X21],#2
0009B1F0  601200b9  STR             W0, [X19,#0x10]
0009B1F4  9f0215eb  CMP             X20, X21
0009B1F8  e0020054  B.EQ            loc_9B254
0009B1FC  60064229  LDP             W0, W1, [X19,#0x10]
0009B200  21240051  SUB             W1, W1, #9
0009B204  611600b9  STR             W1, [X19,#0x14]
0009B208  177c1753  LSR             W23, W0, #0x17
0009B20C  e1feff36  TBZ             W1, #0x1F, loc_9B1E8
0009B210  610240f9  LDR             X1, [X19]
0009B214  e00313aa  MOV             X0, X19
0009B218  fa580094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B21C  61024229  LDP             W1, W0, [X19,#0x10]
0009B220  660240f9  LDR             X6, [X19]
0009B224  05800031  ADDS            W5, W0, #0x20 ; ' '
0009B228  e003004b  NEG             W0, W0
0009B22C  2020c01a  LSL             W0, W1, W0
0009B230  e003801a  CSEL            W0, WZR, W0, EQ
0009B234  2124c51a  LSR             W1, W1, W5
0009B238  f702012a  ORR             W23, W23, W1
0009B23C  c1100091  ADD             X1, X6, #4
0009B240  b7260078  STRH            W23, [X21],#2
0009B244  610200f9  STR             X1, [X19]
0009B248  60160229  STP             W0, W5, [X19,#0x10]
0009B24C  9f0215eb  CMP             X20, X21
0009B250  61fdff54  B.NE            loc_9B1FC
0009B254  e06b40b9  LDR             W0, [SP,#0x100+var_98]
0009B258  f7d348a9  LDP             X23, X20, [SP,#0x100+var_78]
0009B25C  ff8b00b9  STR             WZR, [SP,#0x100+var_78]
0009B260  f57340b9  LDR             W21, [SP,#0x100+var_90]
0009B264  c02f0035  CBNZ            W0, loc_9B85C
0009B268  e0234239  LDRB            W0, [SP,#0x100+var_78]
0009B26C  e10318aa  MOV             X1, X24
0009B270  e23340f9  LDR             X2, [SP,#0x100+var_A0]
0009B274  e7fdff97  BL              _ZN15APACLRVQDecoder26lsfsToMdctSpectralEnvelopeEbRKN4apac13aligned_arrayItLm5EEERNS1_IfLm1024EEE; APACLRVQDecoder::lsfsToMdctSpectralEnvelope(bool,apac::aligned_array<ushort,5ul> const&,apac::aligned_array<float,1024ul> &)
0009B278  e02e0035  CBNZ            W0, loc_9B854
0009B27C  e06b40b9  LDR             W0, [SP,#0x100+var_98]
0009B280  1f000071  CMP             W0, #0
0009B284  800b407a  CCMP            W28, #0, #0, EQ
0009B288  60280054  B.EQ            loc_9B794
0009B28C  e06f40b9  LDR             W0, [SP,#0x100+var_94]
0009B290  a0100035  CBNZ            W0, loc_9B4A4
0009B294  601640b9  LDR             W0, [X19,#0x14]
0009B298  611240b9  LDR             W1, [X19,#0x10]
0009B29C  00100051  SUB             W0, W0, #4
0009B2A0  601600b9  STR             W0, [X19,#0x14]
0009B2A4  3c7c1c53  LSR             W28, W1, #0x1C
0009B2A8  003cf837  TBNZ            W0, #0x1F, loc_9BA28
0009B2AC  216c1c53  LSL             W1, W1, #4
0009B2B0  611200b9  STR             W1, [X19,#0x10]
0009B2B4  813f0012  AND             W1, W28, #0xFFFF
0009B2B8  03008052  MOV             W3, #0
0009B2BC  bb000035  CBNZ            W27, loc_9B2D0
0009B2C0  20034079  LDRH            W0, [X25]
0009B2C4  1f00016b  CMP             W0, W1
0009B2C8  e0179f1a  CSET            W0, EQ
0009B2CC  e303002a  MOV             W3, W0
0009B2D0  223c7bd3  UBFIZ           X2, X1, #5, #0x10
0009B2D4  801c0090  ADRL            X0, unk_42B1F8
0009B2DC  21030079  STRH            W1, [X25]
0009B2E0  0000028b  ADD             X0, X0, X2
0009B2E4  018840a9  LDP             X1, X2, [X0,#8]
0009B2E8  e13b00f9  STR             X1, [SP,#0x100+var_90]
0009B2EC  e40302aa  MOV             X4, X2
0009B2F0  210002aa  ORR             X1, X1, X2
0009B2F4  212100b4  CBZ             X1, loc_9B718
0009B2F8  0138c079  LDRSH           W1, [X0,#0x1C]
0009B2FC  0230c079  LDRSH           W2, [X0,#0x18]
0009B300  e1a700b9  STR             W1, [SP,#0x100+var_5C]
0009B304  4000012a  ORR             W0, W2, W1
0009B308  80200034  CBZ             W0, loc_9B718
0009B30C  a0140090  ADRL            X0, unk_32FEF8
0009B314  fb3f40f9  LDR             X27, [SP,#0x100+var_88]
0009B318  e04f00f9  STR             X0, [SP,#0x100+var_68]
0009B31C  00100091  ADD             X0, X0, #(unk_32FEFC - 0x32FEF8)
0009B320  e05700f9  STR             X0, [SP,#0x100+var_58]
0009B324  80020291  ADD             X0, X20, #0x80
0009B328  f80314aa  MOV             X24, X20
0009B32C  1c008012  MOV             W28, #0xFFFFFFFF
0009B330  e03f00f9  STR             X0, [SP,#0x100+var_88]
0009B334  20040051  SUB             W0, W1, #1
0009B338  f5b700b9  STR             W21, [SP,#0x100+var_4C]
0009B33C  f503032a  MOV             W21, W3
0009B340  f7d30ba9  STP             X23, X20, [SP,#0x100+var_48]
0009B344  f703022a  MOV             W23, W2
0009B348  f40304aa  MOV             X20, X4
0009B34C  fa6700f9  STR             X26, [SP,#0x100+var_38]
0009B350  fa0313aa  MOV             X26, X19
0009B354  f3031baa  MOV             X19, X27
0009B358  fb031c2a  MOV             W27, W28
0009B35C  e0a300b9  STR             W0, [SP,#0x100+var_60]
0009B360  40040051  SUB             W0, W2, #1
0009B364  e09300b9  STR             W0, [SP,#0x100+var_70]
0009B368  35180035  CBNZ            W21, loc_9B66C
0009B36C  e09340b9  LDR             W0, [SP,#0x100+var_70]
0009B370  1f7c0071  CMP             W0, #0x1F
0009B374  481b0054  B.HI            loc_9B6DC
0009B378  41034229  LDP             W1, W0, [X26,#0x10]
0009B37C  e903174b  NEG             W9, W23
0009B380  0000174b  SUB             W0, W0, W23
0009B384  401700b9  STR             W0, [X26,#0x14]
0009B388  3c24c91a  LSR             W28, W1, W9
0009B38C  201ef837  TBNZ            W0, #0x1F, loc_9B750
0009B390  9c3f0013  SXTH            W28, W28
0009B394  2120d71a  LSL             W1, W1, W23
0009B398  e3031c2a  MOV             W3, W28
0009B39C  411300b9  STR             W1, [X26,#0x10]
0009B3A0  21034079  LDRH            W1, [X25]
0009B3A4  821c0090  ADRL            X2, unk_42B1F8
0009B3AC  4114018b  ADD             X1, X2, X1,LSL#5
0009B3B0  21344079  LDRH            W1, [X1,#0x1A]
0009B3B4  3f001c6b  CMP             W1, W28
0009B3B8  0b1b0054  B.LT            loc_9B718
0009B3BC  b41900b4  CBZ             X20, loc_9B6F0
0009B3C0  55170035  CBNZ            W21, loc_9B6A8
0009B3C4  e0a340b9  LDR             W0, [SP,#0x100+var_60]
0009B3C8  1f7c0071  CMP             W0, #0x1F
0009B3CC  a81a0054  B.HI            loc_9B720
0009B3D0  41034229  LDP             W1, W0, [X26,#0x10]
0009B3D4  e2a740b9  LDR             W2, [SP,#0x100+var_5C]
0009B3D8  ea03024b  NEG             W10, W2
0009B3DC  0000024b  SUB             W0, W0, W2
0009B3E0  401700b9  STR             W0, [X26,#0x14]
0009B3E4  3b24ca1a  LSR             W27, W1, W10
0009B3E8  0025f837  TBNZ            W0, #0x1F, loc_9B888
0009B3EC  2120c21a  LSL             W1, W1, W2
0009B3F0  411300b9  STR             W1, [X26,#0x10]
0009B3F4  7b3f0013  SXTH            W27, W27
0009B3F8  20034079  LDRH            W0, [X25]
0009B3FC  e2031b2a  MOV             W2, W27
0009B400  811c0090  ADRL            X1, unk_42B1F8
0009B408  2014008b  ADD             X0, X1, X0,LSL#5
0009B40C  003c4079  LDRH            W0, [X0,#0x1E]
0009B410  1f00026b  CMP             W0, W2
0009B414  2b180054  B.LT            loc_9B718
0009B418  e13b40f9  LDR             X1, [SP,#0x100+var_90]
0009B41C  60701d53  LSL             W0, W3, #3
0009B420  e44f40f9  LDR             X4, [SP,#0x100+var_68]
0009B424  42701d53  LSL             W2, W2, #3
0009B428  20c8208b  ADD             X0, X1, W0,SXTW#2
0009B42C  230080d2  MOV             X3, #1
0009B430  82ca228b  ADD             X2, X20, W2,SXTW#2
0009B434  e50318aa  MOV             X5, X24
0009B438  e10303aa  MOV             X1, X3
0009B43C  070180d2  MOV             X7, #8
0009B440  060480d2  MOV             X6, #0x20 ; ' '
0009B444  8b520094  BL              vDSP_vasm
0009B448  e03f40f9  LDR             X0, [SP,#0x100+var_88]
0009B44C  7c020079  STRH            W28, [X19]
0009B450  7b060079  STRH            W27, [X19,#2]
0009B454  18130091  ADD             X24, X24, #4
0009B458  73120091  ADD             X19, X19, #4
0009B45C  1f0018eb  CMP             X0, X24
0009B460  41f8ff54  B.NE            loc_9B368
0009B464  e06b40b9  LDR             W0, [SP,#0x100+var_98]
0009B468  f5b740b9  LDR             W21, [SP,#0x100+var_4C]
0009B46C  f7d34ba9  LDP             X23, X20, [SP,#0x100+var_48]
0009B470  fa6740f9  LDR             X26, [SP,#0x100+var_38]
0009B474  00250035  CBNZ            W0, loc_9B914
0009B478  e06f40b9  LDR             W0, [SP,#0x100+var_94]
0009B47C  80010034  CBZ             W0, loc_9B4AC
0009B480  e03340f9  LDR             X0, [SP,#0x100+var_A0]
0009B484  250080d2  MOV             X5, #1
0009B488  e24340f9  LDR             X2, [SP,#0x100+var_80]
0009B48C  e30305aa  MOV             X3, X5
0009B490  e10305aa  MOV             X1, X5
0009B494  e40300aa  MOV             X4, X0
0009B498  062080d2  MOV             X6, #0x100
0009B49C  b1510094  BL              vDSP_vmul
0009B4A0  03000014  B               loc_9B4AC
0009B4A4  e06b40b9  LDR             W0, [SP,#0x100+var_98]
0009B4A8  60230035  CBNZ            W0, loc_9B914
0009B4AC  e03340f9  LDR             X0, [SP,#0x100+var_A0]
0009B4B0  250080d2  MOV             X5, #1
0009B4B4  e40314aa  MOV             X4, X20
0009B4B8  e30305aa  MOV             X3, X5
0009B4BC  e10305aa  MOV             X1, X5
0009B4C0  e20314aa  MOV             X2, X20
0009B4C4  062080d2  MOV             X6, #0x100
0009B4C8  c6510094  BL              vDSP_vdiv
0009B4CC  e30240f9  LDR             X3, [X23]
0009B4D0  240080d2  MOV             X4, #1
0009B4D4  e2031aaa  MOV             X2, X26
0009B4D8  e00314aa  MOV             X0, X20
0009B4DC  e10304aa  MOV             X1, X4
0009B4E0  052080d2  MOV             X5, #0x100
0009B4E4  ff510094  BL              vDSP_vsmul
0009B4E8  d60247f9  LDR             X22, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009B4EC  e17f40f9  LDR             X1, [SP,#0x100+var_8]
0009B4F0  c00240f9  LDR             X0, [X22]
0009B4F4  200000ca  EOR             X0, X1, X0
0009B4F8  204600b5  CBNZ            X0, loc_9BDBC
0009B4FC  e003152a  MOV             W0, W21
0009B500  f35341a9  LDP             X19, X20, [SP,#0x100+var_F0]
0009B504  f55b42a9  LDP             X21, X22, [SP,#0x100+var_E0]
0009B508  f76343a9  LDP             X23, X24, [SP,#0x100+var_D0]
0009B50C  f96b44a9  LDP             X25, X26, [SP,#0x100+var_C0]
0009B510  fb7345a9  LDP             X27, X28, [SP,#0x100+var_B0]
0009B514  fd7bd0a8  LDP             X29, X30, [SP+0x100+var_100],#0x100
0009B518  c0035fd6  RET
0009B51C  22040051  SUB             W2, W1, #1
0009B520  621600b9  STR             W2, [X19,#0x14]
0009B524  1c7c1f53  LSR             W28, W0, #0x1F
0009B528  4207f837  TBNZ            W2, #0x1F, loc_9B610
0009B52C  21080051  SUB             W1, W1, #2
0009B530  e3031c2a  MOV             W3, W28
0009B534  e203012a  MOV             W2, W1
0009B538  0000000b  ADD             W0, W0, W0
0009B53C  601200b9  STR             W0, [X19,#0x10]
0009B540  e56b40b9  LDR             W5, [SP,#0x100+var_98]
0009B544  047c1f53  LSR             W4, W0, #0x1F
0009B548  1c008052  MOV             W28, #0
0009B54C  a5250034  CBZ             W5, loc_9BA00
0009B550  621600b9  STR             W2, [X19,#0x14]
0009B554  8228f837  TBNZ            W2, #0x1F, loc_9BA64
0009B558  0000000b  ADD             W0, W0, W0
0009B55C  601200b9  STR             W0, [X19,#0x10]
0009B560  9f1c0072  TST             W4, #0xFF
0009B564  ff8b00b9  STR             WZR, [SP,#0x100+var_78]
0009B568  e0079f1a  CSET            W0, NE
0009B56C  e06f00b9  STR             W0, [SP,#0x100+var_94]
0009B570  63e8ff35  CBNZ            W3, loc_9B27C
0009B574  60064229  LDP             W0, W1, [X19,#0x10]
0009B578  12ffff17  B               loc_9B1C0
0009B57C  010040f9  LDR             X1, [X0]
0009B580  20580094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B584  690a4229  LDP             W9, W2, [X19,#0x10]
0009B588  610240f9  LDR             X1, [X19]
0009B58C  43800011  ADD             W3, W2, #0x20 ; ' '
0009B590  631600b9  STR             W3, [X19,#0x14]
0009B594  405c0011  ADD             W0, W2, #0x17
0009B598  21100091  ADD             X1, X1, #4
0009B59C  610200f9  STR             X1, [X19]
0009B5A0  2425c31a  LSR             W4, W9, W3
0009B5A4  9500152a  ORR             W21, W4, W21
0009B5A8  b53e0012  AND             W21, W21, #0xFFFF
0009B5AC  23020035  CBNZ            W3, loc_9B5F0
0009B5B0  1c008052  MOV             W28, #0
0009B5B4  7f020229  STP             WZR, W0, [X19,#0x10]
0009B5B8  e00313aa  MOV             X0, X19
0009B5BC  11580094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B5C0  60064229  LDP             W0, W1, [X19,#0x10]
0009B5C4  630240f9  LDR             X3, [X19]
0009B5C8  22800031  ADDS            W2, W1, #0x20 ; ' '
0009B5CC  e103014b  NEG             W1, W1
0009B5D0  0424c21a  LSR             W4, W0, W2
0009B5D4  9c03042a  ORR             W28, W28, W4
0009B5D8  0020c11a  LSL             W0, W0, W1
0009B5DC  61100091  ADD             X1, X3, #4
0009B5E0  e003801a  CSEL            W0, WZR, W0, EQ
0009B5E4  610200f9  STR             X1, [X19]
0009B5E8  600a0229  STP             W0, W2, [X19,#0x10]
0009B5EC  eafeff17  B               loc_9B194
0009B5F0  e203024b  NEG             W2, W2
0009B5F4  601600b9  STR             W0, [X19,#0x14]
0009B5F8  2921c21a  LSL             W9, W9, W2
0009B5FC  691200b9  STR             W9, [X19,#0x10]
0009B600  3c7d1753  LSR             W28, W9, #0x17
0009B604  40dcff36  TBZ             W0, #0x1F, loc_9B18C
0009B608  610240f9  LDR             X1, [X19]
0009B60C  ebffff17  B               loc_9B5B8
0009B610  610240f9  LDR             X1, [X19]
0009B614  e00313aa  MOV             X0, X19
0009B618  fa570094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B61C  60124229  LDP             W0, W4, [X19,#0x10]
0009B620  610240f9  LDR             X1, [X19]
0009B624  85800011  ADD             W5, W4, #0x20 ; ' '
0009B628  651600b9  STR             W5, [X19,#0x14]
0009B62C  827c0011  ADD             W2, W4, #0x1F
0009B630  26100091  ADD             X6, X1, #4
0009B634  0324c51a  LSR             W3, W0, W5
0009B638  7c001c2a  ORR             W28, W3, W28
0009B63C  660200f9  STR             X6, [X19]
0009B640  9f1f0072  TST             W28, #0xFF
0009B644  e103022a  MOV             W1, W2
0009B648  e3079f1a  CSET            W3, NE
0009B64C  65230035  CBNZ            W5, loc_9BAB8
0009B650  e06b40b9  LDR             W0, [SP,#0x100+var_98]
0009B654  7f1200b9  STR             WZR, [X19,#0x10]
0009B658  c0370034  CBZ             W0, loc_9BD50
0009B65C  fc031b2a  MOV             W28, W27
0009B660  04008052  MOV             W4, #0
0009B664  00008052  MOV             W0, #0
0009B668  baffff17  B               loc_9B550
0009B66C  41034229  LDP             W1, W0, [X26,#0x10]
0009B670  00040051  SUB             W0, W0, #1
0009B674  401700b9  STR             W0, [X26,#0x14]
0009B678  3c7c1f53  LSR             W28, W1, #0x1F
0009B67C  c012f837  TBNZ            W0, #0x1F, loc_9B8D4
0009B680  2100010b  ADD             W1, W1, W1
0009B684  411300b9  STR             W1, [X26,#0x10]
0009B688  9f1f0072  TST             W28, #0xFF
0009B68C  00050054  B.EQ            loc_9B72C
0009B690  7c02c079  LDRSH           W28, [X19]
0009B694  e3031c2a  MOV             W3, W28
0009B698  42ffff17  B               loc_9B3A0
0009B69C  03008052  MOV             W3, #0
0009B6A0  1c008052  MOV             W28, #0
0009B6A4  740200b4  CBZ             X20, loc_9B6F0
0009B6A8  411340b9  LDR             W1, [X26,#0x10]
0009B6AC  00040051  SUB             W0, W0, #1
0009B6B0  401700b9  STR             W0, [X26,#0x14]
0009B6B4  3b7c1f53  LSR             W27, W1, #0x1F
0009B6B8  2017f837  TBNZ            W0, #0x1F, loc_9B99C
0009B6BC  2100010b  ADD             W1, W1, W1
0009B6C0  411300b9  STR             W1, [X26,#0x10]
0009B6C4  7f1f0072  TST             W27, #0xFF
0009B6C8  600a0054  B.EQ            loc_9B814
0009B6CC  7b06c079  LDRSH           W27, [X19,#2]
0009B6D0  20034079  LDRH            W0, [X25]
0009B6D4  e2031b2a  MOV             W2, W27
0009B6D8  4affff17  B               loc_9B400
0009B6DC  03008052  MOV             W3, #0
0009B6E0  1c008052  MOV             W28, #0
0009B6E4  14e7ffb5  CBNZ            X20, loc_9B3C4
0009B6E8  e303152a  MOV             W3, W21
0009B6EC  fc03152a  MOV             W28, W21
0009B6F0  e13b40f9  LDR             X1, [SP,#0x100+var_90]
0009B6F4  60701d53  LSL             W0, W3, #3
0009B6F8  e25740f9  LDR             X2, [SP,#0x100+var_58]
0009B6FC  e30318aa  MOV             X3, X24
0009B700  20c8208b  ADD             X0, X1, W0,SXTW#2
0009B704  050180d2  MOV             X5, #8
0009B708  040480d2  MOV             X4, #0x20 ; ' '
0009B70C  210080d2  MOV             X1, #1
0009B710  54510094  BL              vDSP_vsadd
0009B714  4dffff17  B               loc_9B448
0009B718  35008052  MOV             W21, #1
0009B71C  73ffff17  B               loc_9B4E8
0009B720  02008052  MOV             W2, #0
0009B724  1b008052  MOV             W27, #0
0009B728  3cffff17  B               loc_9B418
0009B72C  e19340b9  LDR             W1, [SP,#0x100+var_70]
0009B730  3f7c0071  CMP             W1, #0x1F
0009B734  48fbff54  B.HI            loc_9B69C
0009B738  411340b9  LDR             W1, [X26,#0x10]
0009B73C  0000174b  SUB             W0, W0, W23
0009B740  e903174b  NEG             W9, W23
0009B744  401700b9  STR             W0, [X26,#0x14]
0009B748  3c24c91a  LSR             W28, W1, W9
0009B74C  20e2ff36  TBZ             W0, #0x1F, loc_9B390
0009B750  410340f9  LDR             X1, [X26]
0009B754  e0031aaa  MOV             X0, X26
0009B758  aa570094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B75C  42034229  LDP             W2, W0, [X26,#0x10]
0009B760  410340f9  LDR             X1, [X26]
0009B764  e303002a  MOV             W3, W0
0009B768  00800011  ADD             W0, W0, #0x20 ; ' '
0009B76C  401700b9  STR             W0, [X26,#0x14]
0009B770  21100091  ADD             X1, X1, #4
0009B774  410300f9  STR             X1, [X26]
0009B778  4124c01a  LSR             W1, W2, W0
0009B77C  29001c2a  ORR             W9, W1, W28
0009B780  a00f0035  CBNZ            W0, loc_9B974
0009B784  3c3d0013  SXTH            W28, W9
0009B788  5f1300b9  STR             WZR, [X26,#0x10]
0009B78C  e3031c2a  MOV             W3, W28
0009B790  04ffff17  B               loc_9B3A0
0009B794  61024229  LDP             W1, W0, [X19,#0x10]
0009B798  00040051  SUB             W0, W0, #1
0009B79C  601600b9  STR             W0, [X19,#0x14]
0009B7A0  3c7c1f53  LSR             W28, W1, #0x1F
0009B7A4  2019f837  TBNZ            W0, #0x1F, loc_9BAC8
0009B7A8  821f0012  AND             W2, W28, #0xFF
0009B7AC  2100010b  ADD             W1, W1, W1
0009B7B0  611200b9  STR             W1, [X19,#0x10]
0009B7B4  c20e0034  CBZ             W2, loc_9B98C
0009B7B8  611240b9  LDR             W1, [X19,#0x10]
0009B7BC  021c0051  SUB             W2, W0, #7
0009B7C0  621600b9  STR             W2, [X19,#0x14]
0009B7C4  237c1953  LSR             W3, W1, #0x19
0009B7C8  c22bf837  TBNZ            W2, #0x1F, loc_9BD40
0009B7CC  783c0012  AND             W24, W3, #0xFFFF
0009B7D0  00440051  SUB             W0, W0, #0x11
0009B7D4  21601953  LSL             W1, W1, #7
0009B7D8  611200b9  STR             W1, [X19,#0x10]
0009B7DC  601600b9  STR             W0, [X19,#0x14]
0009B7E0  227c1653  LSR             W2, W1, #0x16
0009B7E4  202bf837  TBNZ            W0, #0x1F, loc_9BD48
0009B7E8  21541653  LSL             W1, W1, #0xA
0009B7EC  611200b9  STR             W1, [X19,#0x10]
0009B7F0  e103022a  MOV             W1, W2
0009B7F4  e003182a  MOV             W0, W24
0009B7F8  e24340f9  LDR             X2, [SP,#0x100+var_80]
0009B7FC  b9fdff97  BL              _ZN15APACLRVQDecoder25ltpToMdctSpectralEnvelopeEttRN4apac13aligned_arrayIfLm1024EEE; APACLRVQDecoder::ltpToMdctSpectralEnvelope(ushort,ushort,apac::aligned_array<float,1024ul> &)
0009B800  a0020035  CBNZ            W0, loc_9B854
0009B804  e06f40b9  LDR             W0, [SP,#0x100+var_94]
0009B808  c0e3ff35  CBNZ            W0, loc_9B480
0009B80C  fc6f00b9  STR             W28, [SP,#0x100+var_94]
0009B810  a1feff17  B               loc_9B294
0009B814  e1a340b9  LDR             W1, [SP,#0x100+var_60]
0009B818  3f7c0071  CMP             W1, #0x1F
0009B81C  68140054  B.HI            loc_9BAA8
0009B820  e2a740b9  LDR             W2, [SP,#0x100+var_5C]
0009B824  411340b9  LDR             W1, [X26,#0x10]
0009B828  0000024b  SUB             W0, W0, W2
0009B82C  ea03024b  NEG             W10, W2
0009B830  401700b9  STR             W0, [X26,#0x14]
0009B834  3b24ca1a  LSR             W27, W1, W10
0009B838  2024f837  TBNZ            W0, #0x1F, loc_9BCBC
0009B83C  2120c21a  LSL             W1, W1, W2
0009B840  411300b9  STR             W1, [X26,#0x10]
0009B844  7b3f0013  SXTH            W27, W27
0009B848  20034079  LDRH            W0, [X25]
0009B84C  e2031b2a  MOV             W2, W27
0009B850  ecfeff17  B               loc_9B400
0009B854  f503002a  MOV             W21, W0
0009B858  24ffff17  B               loc_9B4E8
0009B85C  61024229  LDP             W1, W0, [X19,#0x10]
0009B860  00040051  SUB             W0, W0, #1
0009B864  601600b9  STR             W0, [X19,#0x14]
0009B868  227c1f53  LSR             W2, W1, #0x1F
0009B86C  6020f837  TBNZ            W0, #0x1F, loc_9BC78
0009B870  2100010b  ADD             W1, W1, W1
0009B874  611200b9  STR             W1, [X19,#0x10]
0009B878  5f1c0072  TST             W2, #0xFF
0009B87C  e0079f1a  CSET            W0, NE
0009B880  e08b00b9  STR             W0, [SP,#0x100+var_78]
0009B884  79feff17  B               loc_9B268
0009B888  410340f9  LDR             X1, [X26]
0009B88C  e0031aaa  MOV             X0, X26
0009B890  e3b300b9  STR             W3, [SP,#0x100+var_50]
0009B894  5b570094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B898  440b4229  LDP             W4, W2, [X26,#0x10]
0009B89C  400340f9  LDR             X0, [X26]
0009B8A0  41800011  ADD             W1, W2, #0x20 ; ' '
0009B8A4  411700b9  STR             W1, [X26,#0x14]
0009B8A8  00100091  ADD             X0, X0, #4
0009B8AC  400300f9  STR             X0, [X26]
0009B8B0  e3b340b9  LDR             W3, [SP,#0x100+var_50]
0009B8B4  8024c11a  LSR             W0, W4, W1
0009B8B8  0a001b2a  ORR             W10, W0, W27
0009B8BC  41090035  CBNZ            W1, loc_9B9E4
0009B8C0  5f1300b9  STR             WZR, [X26,#0x10]
0009B8C4  5b3d0013  SXTH            W27, W10
0009B8C8  e2031b2a  MOV             W2, W27
0009B8CC  20034079  LDRH            W0, [X25]
0009B8D0  ccfeff17  B               loc_9B400
0009B8D4  410340f9  LDR             X1, [X26]
0009B8D8  e0031aaa  MOV             X0, X26
0009B8DC  49570094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B8E0  43034229  LDP             W3, W0, [X26,#0x10]
0009B8E4  440340f9  LDR             X4, [X26]
0009B8E8  e103002a  MOV             W1, W0
0009B8EC  e103014b  NEG             W1, W1
0009B8F0  00800031  ADDS            W0, W0, #0x20 ; ' '
0009B8F4  84100091  ADD             X4, X4, #4
0009B8F8  440300f9  STR             X4, [X26]
0009B8FC  6120c11a  LSL             W1, W3, W1
0009B900  e103811a  CSEL            W1, WZR, W1, EQ
0009B904  6324c01a  LSR             W3, W3, W0
0009B908  9c03032a  ORR             W28, W28, W3
0009B90C  41030229  STP             W1, W0, [X26,#0x10]
0009B910  5effff17  B               loc_9B688
0009B914  e03340f9  LDR             X0, [SP,#0x100+var_A0]
0009B918  250080d2  MOV             X5, #1
0009B91C  e40314aa  MOV             X4, X20
0009B920  e30305aa  MOV             X3, X5
0009B924  e10305aa  MOV             X1, X5
0009B928  e20314aa  MOV             X2, X20
0009B92C  062080d2  MOV             X6, #0x100
0009B930  ac500094  BL              vDSP_vdiv
0009B934  e08b40b9  LDR             W0, [SP,#0x100+var_78]
0009B938  80130035  CBNZ            W0, loc_9BBA8
0009B93C  130080d2  MOV             X19, #0
0009B940  e30240f9  LDR             X3, [X23]
0009B944  e00314aa  MOV             X0, X20
0009B948  e2031aaa  MOV             X2, X26
0009B94C  050480d2  MOV             X5, #0x20 ; ' '
0009B950  6300138b  ADD             X3, X3, X19
0009B954  240080d2  MOV             X4, #1
0009B958  73020891  ADD             X19, X19, #0x200
0009B95C  010180d2  MOV             X1, #8
0009B960  e0500094  BL              vDSP_vsmul
0009B964  94120091  ADD             X20, X20, #4
0009B968  7f0640f1  CMP             X19, #1,LSL#12
0009B96C  a1feff54  B.NE            loc_9B940
0009B970  defeff17  B               loc_9B4E8
0009B974  e103034b  NEG             W1, W3
0009B978  3c3d0013  SXTH            W28, W9
0009B97C  e3031c2a  MOV             W3, W28
0009B980  4220c11a  LSL             W2, W2, W1
0009B984  421300b9  STR             W2, [X26,#0x10]
0009B988  86feff17  B               loc_9B3A0
0009B98C  e16f40b9  LDR             W1, [SP,#0x100+var_94]
0009B990  e1d8ff35  CBNZ            W1, loc_9B4AC
0009B994  fc6f00b9  STR             W28, [SP,#0x100+var_94]
0009B998  40feff17  B               loc_9B298
0009B99C  410340f9  LDR             X1, [X26]
0009B9A0  e0031aaa  MOV             X0, X26
0009B9A4  e3b300b9  STR             W3, [SP,#0x100+var_50]
0009B9A8  16570094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009B9AC  44034229  LDP             W4, W0, [X26,#0x10]
0009B9B0  450340f9  LDR             X5, [X26]
0009B9B4  e103002a  MOV             W1, W0
0009B9B8  e103014b  NEG             W1, W1
0009B9BC  00800031  ADDS            W0, W0, #0x20 ; ' '
0009B9C0  a5100091  ADD             X5, X5, #4
0009B9C4  e3b340b9  LDR             W3, [SP,#0x100+var_50]
0009B9C8  8120c11a  LSL             W1, W4, W1
0009B9CC  e103811a  CSEL            W1, WZR, W1, EQ
0009B9D0  8424c01a  LSR             W4, W4, W0
0009B9D4  7b03042a  ORR             W27, W27, W4
0009B9D8  450300f9  STR             X5, [X26]
0009B9DC  41030229  STP             W1, W0, [X26,#0x10]
0009B9E0  39ffff17  B               loc_9B6C4
0009B9E4  e003024b  NEG             W0, W2
0009B9E8  5b3d0013  SXTH            W27, W10
0009B9EC  e2031b2a  MOV             W2, W27
0009B9F0  8420c01a  LSL             W4, W4, W0
0009B9F4  441300b9  STR             W4, [X26,#0x10]
0009B9F8  20034079  LDRH            W0, [X25]
0009B9FC  81feff17  B               loc_9B400
0009BA00  621600b9  STR             W2, [X19,#0x14]
0009BA04  e21cf837  TBNZ            W2, #0x1F, loc_9BDA0
0009BA08  0000000b  ADD             W0, W0, W0
0009BA0C  e203042a  MOV             W2, W4
0009BA10  601200b9  STR             W0, [X19,#0x10]
0009BA14  047c1f53  LSR             W4, W0, #0x1F
0009BA18  5f1c0072  TST             W2, #0xFF
0009BA1C  22040051  SUB             W2, W1, #1
0009BA20  fc079f1a  CSET            W28, NE
0009BA24  cbfeff17  B               loc_9B550
0009BA28  610240f9  LDR             X1, [X19]
0009BA2C  e00313aa  MOV             X0, X19
0009BA30  f4560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BA34  60064229  LDP             W0, W1, [X19,#0x10]
0009BA38  630240f9  LDR             X3, [X19]
0009BA3C  22800031  ADDS            W2, W1, #0x20 ; ' '
0009BA40  e103014b  NEG             W1, W1
0009BA44  0424c21a  LSR             W4, W0, W2
0009BA48  9c03042a  ORR             W28, W28, W4
0009BA4C  0020c11a  LSL             W0, W0, W1
0009BA50  61100091  ADD             X1, X3, #4
0009BA54  e003801a  CSEL            W0, WZR, W0, EQ
0009BA58  610200f9  STR             X1, [X19]
0009BA5C  600a0229  STP             W0, W2, [X19,#0x10]
0009BA60  15feff17  B               loc_9B2B4
0009BA64  610240f9  LDR             X1, [X19]
0009BA68  e00313aa  MOV             X0, X19
0009BA6C  e3930d29  STP             W3, W4, [SP,#0x100+var_94]
0009BA70  e4560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BA74  62024229  LDP             W2, W0, [X19,#0x10]
0009BA78  e3934d29  LDP             W3, W4, [SP,#0x100+var_94]
0009BA7C  05800031  ADDS            W5, W0, #0x20 ; ' '
0009BA80  610240f9  LDR             X1, [X19]
0009BA84  e003004b  NEG             W0, W0
0009BA88  4624c51a  LSR             W6, W2, W5
0009BA8C  8400062a  ORR             W4, W4, W6
0009BA90  21100091  ADD             X1, X1, #4
0009BA94  4220c01a  LSL             W2, W2, W0
0009BA98  e203821a  CSEL            W2, WZR, W2, EQ
0009BA9C  610200f9  STR             X1, [X19]
0009BAA0  62160229  STP             W2, W5, [X19,#0x10]
0009BAA4  affeff17  B               loc_9B560
0009BAA8  20034079  LDRH            W0, [X25]
0009BAAC  02008052  MOV             W2, #0
0009BAB0  1b008052  MOV             W27, #0
0009BAB4  53feff17  B               loc_9B400
0009BAB8  e403044b  NEG             W4, W4
0009BABC  0020c41a  LSL             W0, W0, W4
0009BAC0  601200b9  STR             W0, [X19,#0x10]
0009BAC4  9ffeff17  B               loc_9B540
0009BAC8  610240f9  LDR             X1, [X19]
0009BACC  e00313aa  MOV             X0, X19
0009BAD0  cc560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BAD4  63024229  LDP             W3, W0, [X19,#0x10]
0009BAD8  610240f9  LDR             X1, [X19]
0009BADC  e403002a  MOV             W4, W0
0009BAE0  00800011  ADD             W0, W0, #0x20 ; ' '
0009BAE4  601600b9  STR             W0, [X19,#0x14]
0009BAE8  21100091  ADD             X1, X1, #4
0009BAEC  610200f9  STR             X1, [X19]
0009BAF0  6224c01a  LSR             W2, W3, W0
0009BAF4  42001c2a  ORR             W2, W2, W28
0009BAF8  421c0072  ANDS            W2, W2, #0xFF
0009BAFC  e50301aa  MOV             X5, X1
0009BB00  fc079f1a  CSET            W28, NE
0009BB04  20100035  CBNZ            W0, loc_9BD08
0009BB08  7f1200b9  STR             WZR, [X19,#0x10]
0009BB0C  62100034  CBZ             W2, loc_9BD18
0009BB10  c0008012  MOV             W0, #0xFFFFFFF9
0009BB14  03008052  MOV             W3, #0
0009BB18  3c008052  MOV             W28, #1
0009BB1C  601600b9  STR             W0, [X19,#0x14]
0009BB20  e10305aa  MOV             X1, X5
0009BB24  e00313aa  MOV             X0, X19
0009BB28  e37300b9  STR             W3, [SP,#0x100+var_90]
0009BB2C  b5560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BB30  62124229  LDP             W2, W4, [X19,#0x10]
0009BB34  610240f9  LDR             X1, [X19]
0009BB38  85800011  ADD             W5, W4, #0x20 ; ' '
0009BB3C  e37340b9  LDR             W3, [SP,#0x100+var_90]
0009BB40  80580011  ADD             W0, W4, #0x16
0009BB44  21100091  ADD             X1, X1, #4
0009BB48  610200f9  STR             X1, [X19]
0009BB4C  651600b9  STR             W5, [X19,#0x14]
0009BB50  4624c51a  LSR             W6, W2, W5
0009BB54  c300032a  ORR             W3, W6, W3
0009BB58  783c0012  AND             W24, W3, #0xFFFF
0009BB5C  a50e0035  CBNZ            W5, loc_9BD30
0009BB60  02008052  MOV             W2, #0
0009BB64  7f020229  STP             WZR, W0, [X19,#0x10]
0009BB68  e00313aa  MOV             X0, X19
0009BB6C  e27300b9  STR             W2, [SP,#0x100+var_90]
0009BB70  a4560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BB74  61024229  LDP             W1, W0, [X19,#0x10]
0009BB78  650240f9  LDR             X5, [X19]
0009BB7C  04800031  ADDS            W4, W0, #0x20 ; ' '
0009BB80  e003004b  NEG             W0, W0
0009BB84  e27340b9  LDR             W2, [SP,#0x100+var_90]
0009BB88  2020c01a  LSL             W0, W1, W0
0009BB8C  e003801a  CSEL            W0, WZR, W0, EQ
0009BB90  2124c41a  LSR             W1, W1, W4
0009BB94  4200012a  ORR             W2, W2, W1
0009BB98  a1100091  ADD             X1, X5, #4
0009BB9C  610200f9  STR             X1, [X19]
0009BBA0  60120229  STP             W0, W4, [X19,#0x10]
0009BBA4  13ffff17  B               loc_9B7F0
0009BBA8  b8140090  ADRL            X24, unk_32FEF8
0009BBB0  240080d2  MOV             X4, #1
0009BBB4  e2031aaa  MOV             X2, X26
0009BBB8  e10304aa  MOV             X1, X4
0009BBBC  e30314aa  MOV             X3, X20
0009BBC0  e00314aa  MOV             X0, X20
0009BBC4  052080d2  MOV             X5, #0x100
0009BBC8  18230091  ADD             X24, X24, #(unk_32FF00 - 0x32FEF8)
0009BBCC  190080d2  MOV             X25, #0
0009BBD0  13008052  MOV             W19, #0
0009BBD4  43500094  BL              vDSP_vsmul
0009BBD8  15000014  B               loc_9BC2C
0009BBDC  e10314aa  MOV             X1, X20; src
0009BBE0  021080d2  MOV             X2, #0x80; n
0009BBE4  0000198b  ADD             X0, X0, X25; dest
0009BBE8  c251ff97  BL              .memmove
0009BBEC  e40240f9  LDR             X4, [X23]
0009BBF0  250080d2  MOV             X5, #1
0009BBF4  9a020291  ADD             X26, X20, #0x80
0009BBF8  e30305aa  MOV             X3, X5
0009BBFC  8400198b  ADD             X4, X4, X25
0009BC00  e10305aa  MOV             X1, X5
0009BC04  e00318aa  MOV             X0, X24
0009BC08  e20304aa  MOV             X2, X4
0009BC0C  060480d2  MOV             X6, #0x20 ; ' '
0009BC10  f44f0094  BL              vDSP_vdiv
0009BC14  73060011  ADD             W19, W19, #1
0009BC18  39030891  ADD             X25, X25, #0x200
0009BC1C  733e0012  AND             W19, W19, #0xFFFF
0009BC20  f4031aaa  MOV             X20, X26
0009BC24  7f220071  CMP             W19, #8
0009BC28  00c6ff54  B.EQ            loc_9B4E8
0009BC2C  e00240f9  LDR             X0, [X23]
0009BC30  73fd0737  TBNZ            W19, #0, loc_9BBDC
0009BC34  9a020291  ADD             X26, X20, #0x80
0009BC38  0200198b  ADD             X2, X0, X25
0009BC3C  e0031aaa  MOV             X0, X26
0009BC40  00cc5fbc  LDR             S0, [X0,#-4]!
0009BC44  404400bc  STR             S0, [X2],#4
0009BC48  1f0014eb  CMP             X0, X20
0009BC4C  a1ffff54  B.NE            loc_9BC40
0009BC50  e40240f9  LDR             X4, [X23]
0009BC54  250080d2  MOV             X5, #1
0009BC58  e00318aa  MOV             X0, X24
0009BC5C  e30305aa  MOV             X3, X5
0009BC60  8400198b  ADD             X4, X4, X25
0009BC64  e10305aa  MOV             X1, X5
0009BC68  e20304aa  MOV             X2, X4
0009BC6C  060480d2  MOV             X6, #0x20 ; ' '
0009BC70  dc4f0094  BL              vDSP_vdiv
0009BC74  e8ffff17  B               loc_9BC14
0009BC78  610240f9  LDR             X1, [X19]
0009BC7C  e00313aa  MOV             X0, X19
0009BC80  e27300b9  STR             W2, [SP,#0x100+var_90]
0009BC84  5f560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BC88  60064229  LDP             W0, W1, [X19,#0x10]
0009BC8C  640240f9  LDR             X4, [X19]
0009BC90  23800031  ADDS            W3, W1, #0x20 ; ' '
0009BC94  e27340b9  LDR             W2, [SP,#0x100+var_90]
0009BC98  e103014b  NEG             W1, W1
0009BC9C  0624c31a  LSR             W6, W0, W3
0009BCA0  0020c11a  LSL             W0, W0, W1
0009BCA4  4200062a  ORR             W2, W2, W6
0009BCA8  e003801a  CSEL            W0, WZR, W0, EQ
0009BCAC  81100091  ADD             X1, X4, #4
0009BCB0  610200f9  STR             X1, [X19]
0009BCB4  600e0229  STP             W0, W3, [X19,#0x10]
0009BCB8  f0feff17  B               loc_9B878
0009BCBC  410340f9  LDR             X1, [X26]
0009BCC0  e0031aaa  MOV             X0, X26
0009BCC4  e3b300b9  STR             W3, [SP,#0x100+var_50]
0009BCC8  4e560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BCCC  440b4229  LDP             W4, W2, [X26,#0x10]
0009BCD0  410340f9  LDR             X1, [X26]
0009BCD4  40800011  ADD             W0, W2, #0x20 ; ' '
0009BCD8  401700b9  STR             W0, [X26,#0x14]
0009BCDC  21100091  ADD             X1, X1, #4
0009BCE0  410300f9  STR             X1, [X26]
0009BCE4  e3b340b9  LDR             W3, [SP,#0x100+var_50]
0009BCE8  8124c01a  LSR             W1, W4, W0
0009BCEC  2a001b2a  ORR             W10, W1, W27
0009BCF0  a0e7ff35  CBNZ            W0, loc_9B9E4
0009BCF4  5f1300b9  STR             WZR, [X26,#0x10]
0009BCF8  5b3d0013  SXTH            W27, W10
0009BCFC  e2031b2a  MOV             W2, W27
0009BD00  20034079  LDRH            W0, [X25]
0009BD04  bffdff17  B               loc_9B400
0009BD08  e403044b  NEG             W4, W4
0009BD0C  6320c41a  LSL             W3, W3, W4
0009BD10  631200b9  STR             W3, [X19,#0x10]
0009BD14  a8feff17  B               loc_9B7B4
0009BD18  e06f40b9  LDR             W0, [SP,#0x100+var_94]
0009BD1C  80bcff35  CBNZ            W0, loc_9B4AC
0009BD20  84700011  ADD             W4, W4, #0x1C
0009BD24  1c008052  MOV             W28, #0
0009BD28  641600b9  STR             W4, [X19,#0x14]
0009BD2C  40ffff17  B               loc_9BA2C
0009BD30  e103044b  NEG             W1, W4
0009BD34  4120c11a  LSL             W1, W2, W1
0009BD38  611200b9  STR             W1, [X19,#0x10]
0009BD3C  a8feff17  B               loc_9B7DC
0009BD40  650240f9  LDR             X5, [X19]
0009BD44  77ffff17  B               loc_9BB20
0009BD48  610240f9  LDR             X1, [X19]
0009BD4C  87ffff17  B               loc_9BB68
0009BD50  04008052  MOV             W4, #0
0009BD54  621600b9  STR             W2, [X19,#0x14]
0009BD58  e10306aa  MOV             X1, X6
0009BD5C  e00313aa  MOV             X0, X19
0009BD60  e3930d29  STP             W3, W4, [SP,#0x100+var_94]
0009BD64  27560094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BD68  60164229  LDP             W0, W5, [X19,#0x10]
0009BD6C  610240f9  LDR             X1, [X19]
0009BD70  e3934d29  LDP             W3, W4, [SP,#0x100+var_94]
0009BD74  22100091  ADD             X2, X1, #4
0009BD78  a1800011  ADD             W1, W5, #0x20 ; ' '
0009BD7C  620200f9  STR             X2, [X19]
0009BD80  611600b9  STR             W1, [X19,#0x14]
0009BD84  0224c11a  LSR             W2, W0, W1
0009BD88  4200042a  ORR             W2, W2, W4
0009BD8C  e1000035  CBNZ            W1, loc_9BDA8
0009BD90  04008052  MOV             W4, #0
0009BD94  00008052  MOV             W0, #0
0009BD98  7f1200b9  STR             WZR, [X19,#0x10]
0009BD9C  1fffff17  B               loc_9BA18
0009BDA0  660240f9  LDR             X6, [X19]
0009BDA4  edffff17  B               loc_9BD58
0009BDA8  e503054b  NEG             W5, W5
0009BDAC  0020c51a  LSL             W0, W0, W5
0009BDB0  601200b9  STR             W0, [X19,#0x10]
0009BDB4  047c1f53  LSR             W4, W0, #0x1F
0009BDB8  18ffff17  B               loc_9BA18
0009BDBC  554fff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009bdc0, for verification */
0009BDC0  e91340d1  SUB             X9, SP, #4,LSL#12
0009BDC4  290540d1  SUB             X9, X9, #1,LSL#12
0009BDC8  3f0108f9  STR             XZR, [X9,#0x1000]
0009BDCC  290540d1  SUB             X9, X9, #1,LSL#12
0009BDD0  3f0108f9  STR             XZR, [X9,#0x1000]
0009BDD4  3f8105f9  STR             XZR, [X9,#0xB00]
0009BDD8  0ca084d2  MOV             X12, #0x2500
0009BDDC  ff632ccb  SUB             SP, SP, X12
0009BDE0  fd7b01a9  STP             X29, X30, [SP,#0x2500+var_24F0]
0009BDE4  fd430091  ADD             X29, SP, #0x2500+var_24F0
0009BDE8  f55b03a9  STP             X21, X22, [SP,#0x2500+var_24D0]
0009BDEC  961c00b0  ADRP            X22, #__stack_chk_guard_ptr@PAGE
0009BDF0  f50301aa  MOV             X21, X1
0009BDF4  f35302a9  STP             X19, X20, [SP,#0x2500+var_24E0]
0009BDF8  f40302aa  MOV             X20, X2
0009BDFC  c20247f9  LDR             X2, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009BE00  f72300f9  STR             X23, [SP,#0x2500+var_24C0]
0009BE04  410040f9  LDR             X1, [X2]
0009BE08  e17f12f9  STR             X1, [SP,#0x2500+var_8]
0009BE0C  010080d2  MOV             X1, #0
0009BE10  b72a4039  LDRB            W23, [X21,#0xA]
0009BE14  847a41f9  LDR             X4, [X20,#0x2F0]
0009BE18  857e41f9  LDR             X5, [X20,#0x2F8]
0009BE1C  bf0004eb  CMP             X5, X4
0009BE20  a0000054  B.EQ            loc_9BE34
0009BE24  1f2003d5  NOP
0009BE28  9f4400b8  STR             WZR, [X4],#4
0009BE2C  bf0004eb  CMP             X5, X4
0009BE30  c1ffff54  B.NE            loc_9BE28
0009BE34  05104229  LDP             W5, W4, [X0,#0x10]
0009BE38  84040051  SUB             W4, W4, #1
0009BE3C  041400b9  STR             W4, [X0,#0x14]
0009BE40  b37c1f53  LSR             W19, W5, #0x1F
0009BE44  0406f837  TBNZ            W4, #0x1F, loc_9BF04
0009BE48  a500050b  ADD             W5, W5, W5
0009BE4C  051000b9  STR             W5, [X0,#0x10]
0009BE50  7f1e0072  TST             W19, #0xFF
0009BE54  01040054  B.NE            loc_9BED4
0009BE58  e7c30191  ADD             X7, SP, #0x2500+var_2490
0009BE5C  e10307aa  MOV             X1, X7
0009BE60  e4000291  ADD             X4, X7, #0x80
0009BE64  1f2003d5  NOP
0009BE68  3f240078  STRH            WZR, [X1],#2
0009BE6C  3f0004eb  CMP             X1, X4
0009BE70  c1ffff54  B.NE            loc_9BE68
0009BE74  ff0a0071  CMP             W23, #2
0009BE78  e1bb0191  ADD             X1, SP, #0x2500+var_2492
0009BE7C  e10300f9  STR             X1, [SP,#0x2500+var_2500]
0009BE80  029e82d2  MOV             X2, #0x14F0
0009BE84  83c20b91  ADD             X3, X20, #0x2F0
0009BE88  e563228b  ADD             X5, SP, X2
0009BE8C  e6c30391  ADD             X6, SP, #0x2500+var_2410
0009BE90  e4c31391  ADD             X4, SP, #0x2500+var_2010
0009BE94  e1179f1a  CSET            W1, EQ
0009BE98  22008052  MOV             W2, #1
0009BE9C  ffdf0079  STRH            WZR, [SP,#0x2500+var_2492]
0009BEA0  98fcff97  BL              _ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt; APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<uint> &,bool,bool,APACSpectralData &,apac::aligned_array<float,1024ul> &,apac::aligned_array<float,1024ul> &,apac::aligned_array<float,256ul> &,apac::aligned_array<short,64ul> &,ushort *)
0009BEA4  c10247f9  LDR             X1, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009BEA8  e27f52f9  LDR             X2, [SP,#0x2500+var_8]
0009BEAC  210040f9  LDR             X1, [X1]
0009BEB0  410001ca  EOR             X1, X2, X1
0009BEB4  010500b5  CBNZ            X1, loc_9BF54
0009BEB8  0ca084d2  MOV             X12, #0x2500
0009BEBC  fd7b41a9  LDP             X29, X30, [SP,#0x2500+var_24F0]
0009BEC0  f35342a9  LDP             X19, X20, [SP,#0x2500+var_24E0]
0009BEC4  f55b43a9  LDP             X21, X22, [SP,#0x2500+var_24D0]
0009BEC8  f72340f9  LDR             X23, [SP,#0x2500+var_24C0]
0009BECC  ff632c8b  ADD             SP, SP, X12
0009BED0  c0035fd6  RET
0009BED4  a10240f9  LDR             X1, [X21]
0009BED8  801c00b0  ADRP            X0, #_ZN11APACICSInfo5ResetEv_ptr@PAGE
0009BEDC  006447f9  LDR             X0, [X0,#_ZN11APACICSInfo5ResetEv_ptr@PAGEOFF]; APACICSInfo::Reset(void) ...
0009BEE0  210c40f9  LDR             X1, [X1,#0x18]
0009BEE4  3f0000eb  CMP             X1, X0
0009BEE8  e1020054  B.NE            loc_9BF44
0009BEEC  21008052  MOV             W1, #1
0009BEF0  00008052  MOV             W0, #0
0009BEF4  bf160079  STRH            WZR, [X21,#0xA]
0009BEF8  bf320039  STRB            WZR, [X21,#0xC]
0009BEFC  a11200b9  STR             W1, [X21,#0x10]
0009BF00  e9ffff17  B               loc_9BEA4
0009BF04  010040f9  LDR             X1, [X0]
0009BF08  e02f00f9  STR             X0, [SP,#0x2500+var_24A8]
0009BF0C  bd550094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009BF10  e02f40f9  LDR             X0, [SP,#0x2500+var_24A8]
0009BF14  01084229  LDP             W1, W2, [X0,#0x10]
0009BF18  040040f9  LDR             X4, [X0]
0009BF1C  43800031  ADDS            W3, W2, #0x20 ; ' '
0009BF20  e203024b  NEG             W2, W2
0009BF24  2524c31a  LSR             W5, W1, W3
0009BF28  7302052a  ORR             W19, W19, W5
0009BF2C  2120c21a  LSL             W1, W1, W2
0009BF30  82100091  ADD             X2, X4, #4
0009BF34  e103811a  CSEL            W1, WZR, W1, EQ
0009BF38  020000f9  STR             X2, [X0]
0009BF3C  010c0229  STP             W1, W3, [X0,#0x10]
0009BF40  c4ffff17  B               loc_9BE50
0009BF44  e00315aa  MOV             X0, X21
0009BF48  20003fd6  BLR             X1
0009BF4C  00008052  MOV             W0, #0
0009BF50  d5ffff17  B               loc_9BEA4
0009BF54  ef4eff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009bf60, for verification */
0009BF60  e91340d1  SUB             X9, SP, #4,LSL#12
0009BF64  290540d1  SUB             X9, X9, #1,LSL#12
0009BF68  3f0108f9  STR             XZR, [X9,#0x1000]
0009BF6C  290540d1  SUB             X9, X9, #1,LSL#12
0009BF70  3f0108f9  STR             XZR, [X9,#0x1000]
0009BF74  3f8105f9  STR             XZR, [X9,#0xB00]
0009BF78  0ca084d2  MOV             X12, #0x2500
0009BF7C  ff632ccb  SUB             SP, SP, X12
0009BF80  fd7b01a9  STP             X29, X30, [SP,#0x2500+var_24F0]
0009BF84  fd430091  ADD             X29, SP, #0x2500+var_24F0
0009BF88  f76304a9  STP             X23, X24, [SP,#0x2500+var_24C0]
0009BF8C  981c00b0  ADRP            X24, #__stack_chk_guard_ptr@PAGE
0009BF90  f70304aa  MOV             X23, X4
0009BF94  f55b03a9  STP             X21, X22, [SP,#0x2500+var_24D0]
0009BF98  f60303aa  MOV             X22, X3
0009BF9C  f50302aa  MOV             X21, X2
0009BFA0  030347f9  LDR             X3, [X24,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009BFA4  f35302a9  STP             X19, X20, [SP,#0x2500+var_24E0]
0009BFA8  f40301aa  MOV             X20, X1
0009BFAC  f96b05a9  STP             X25, X26, [SP,#0x2500+var_24B0]
0009BFB0  f30300aa  MOV             X19, X0
0009BFB4  610040f9  LDR             X1, [X3]
0009BFB8  e17f12f9  STR             X1, [SP,#0x2500+var_8]
0009BFBC  010080d2  MOV             X1, #0
0009BFC0  9a2a4039  LDRB            W26, [X20,#0xA]
0009BFC4  c57a41f9  LDR             X5, [X22,#0x2F0]
0009BFC8  c67e41f9  LDR             X6, [X22,#0x2F8]
0009BFCC  df0005eb  CMP             X6, X5
0009BFD0  a0000054  B.EQ            loc_9BFE4
0009BFD4  1f2003d5  NOP
0009BFD8  bf4400b8  STR             WZR, [X5],#4
0009BFDC  bf0006eb  CMP             X5, X6
0009BFE0  c1ffff54  B.NE            loc_9BFD8
0009BFE4  61024229  LDP             W1, W0, [X19,#0x10]
0009BFE8  00040051  SUB             W0, W0, #1
0009BFEC  601600b9  STR             W0, [X19,#0x14]
0009BFF0  397c1f53  LSR             W25, W1, #0x1F
0009BFF4  c00af837  TBNZ            W0, #0x1F, loc_9C14C
0009BFF8  2100010b  ADD             W1, W1, W1
0009BFFC  611200b9  STR             W1, [X19,#0x10]
0009C000  3f1f0072  TST             W25, #0xFF
0009C004  a0060054  B.EQ            loc_9C0D8
0009C008  810240f9  LDR             X1, [X20]
0009C00C  801c0090  ADRP            X0, #_ZN11APACICSInfo5ResetEv_ptr@PAGE
0009C010  006447f9  LDR             X0, [X0,#_ZN11APACICSInfo5ResetEv_ptr@PAGEOFF]; APACICSInfo::Reset(void) ...
0009C014  210c40f9  LDR             X1, [X1,#0x18]
0009C018  3f0000eb  CMP             X1, X0
0009C01C  c10d0054  B.NE            loc_9C1D4
0009C020  20008052  MOV             W0, #1
0009C024  9f160079  STRH            WZR, [X20,#0xA]
0009C028  9f320039  STRB            WZR, [X20,#0xC]
0009C02C  801200b9  STR             W0, [X20,#0x10]
0009C030  e17a41f9  LDR             X1, [X23,#0x2F0]
0009C034  e07e41f9  LDR             X0, [X23,#0x2F8]
0009C038  b62a4039  LDRB            W22, [X21,#0xA]
0009C03C  3f0000eb  CMP             X1, X0
0009C040  a0000054  B.EQ            loc_9C054
0009C044  1f2003d5  NOP
0009C048  3f4400b8  STR             WZR, [X1],#4
0009C04C  1f0001eb  CMP             X0, X1
0009C050  c1ffff54  B.NE            loc_9C048
0009C054  61024229  LDP             W1, W0, [X19,#0x10]
0009C058  00040051  SUB             W0, W0, #1
0009C05C  601600b9  STR             W0, [X19,#0x14]
0009C060  347c1f53  LSR             W20, W1, #0x1F
0009C064  2009f837  TBNZ            W0, #0x1F, loc_9C188
0009C068  2100010b  ADD             W1, W1, W1
0009C06C  611200b9  STR             W1, [X19,#0x10]
0009C070  9f1e0072  TST             W20, #0xFF
0009C074  00050054  B.EQ            loc_9C114
0009C078  a10240f9  LDR             X1, [X21]
0009C07C  801c0090  ADRP            X0, #_ZN11APACICSInfo5ResetEv_ptr@PAGE
0009C080  006447f9  LDR             X0, [X0,#_ZN11APACICSInfo5ResetEv_ptr@PAGEOFF]; APACICSInfo::Reset(void) ...
0009C084  210c40f9  LDR             X1, [X1,#0x18]
0009C088  3f0000eb  CMP             X1, X0
0009C08C  c1090054  B.NE            loc_9C1C4
0009C090  21008052  MOV             W1, #1
0009C094  00008052  MOV             W0, #0
0009C098  bf160079  STRH            WZR, [X21,#0xA]
0009C09C  bf320039  STRB            WZR, [X21,#0xC]
0009C0A0  a11200b9  STR             W1, [X21,#0x10]
0009C0A4  180347f9  LDR             X24, [X24,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C0A8  e27f52f9  LDR             X2, [SP,#0x2500+var_8]
0009C0AC  010340f9  LDR             X1, [X24]
0009C0B0  410001ca  EOR             X1, X2, X1
0009C0B4  610900b5  CBNZ            X1, loc_9C1E0
0009C0B8  0ca084d2  MOV             X12, #0x2500
0009C0BC  fd7b41a9  LDP             X29, X30, [SP,#0x2500+var_24F0]
0009C0C0  f35342a9  LDP             X19, X20, [SP,#0x2500+var_24E0]
0009C0C4  f55b43a9  LDP             X21, X22, [SP,#0x2500+var_24D0]
0009C0C8  f76344a9  LDP             X23, X24, [SP,#0x2500+var_24C0]
0009C0CC  f96b45a9  LDP             X25, X26, [SP,#0x2500+var_24B0]
0009C0D0  ff632c8b  ADD             SP, SP, X12
0009C0D4  c0035fd6  RET
0009C0D8  5f0b0071  CMP             W26, #2
0009C0DC  e0bb0191  ADD             X0, SP, #0x2500+var_2492
0009C0E0  e00300f9  STR             X0, [SP,#0x2500+var_2500]
0009C0E4  049e82d2  MOV             X4, #0x14F0
0009C0E8  c3c20b91  ADD             X3, X22, #0x2F0
0009C0EC  e563248b  ADD             X5, SP, X4
0009C0F0  e1179f1a  CSET            W1, EQ
0009C0F4  e7c30191  ADD             X7, SP, #0x2500+var_2490
0009C0F8  e6c30391  ADD             X6, SP, #0x2500+var_2410
0009C0FC  e4c31391  ADD             X4, SP, #0x2500+var_2010
0009C100  e00313aa  MOV             X0, X19
0009C104  22008052  MOV             W2, #1
0009C108  fefbff97  BL              _ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt; APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<uint> &,bool,bool,APACSpectralData &,apac::aligned_array<float,1024ul> &,apac::aligned_array<float,1024ul> &,apac::aligned_array<float,256ul> &,apac::aligned_array<short,64ul> &,ushort *)
0009C10C  20f9ff34  CBZ             W0, loc_9C030
0009C110  e5ffff17  B               loc_9C0A4
0009C114  df0a0071  CMP             W22, #2
0009C118  e0bb0191  ADD             X0, SP, #0x2500+var_2492
0009C11C  e00300f9  STR             X0, [SP,#0x2500+var_2500]
0009C120  029e82d2  MOV             X2, #0x14F0
0009C124  e3c20b91  ADD             X3, X23, #0x2F0
0009C128  e563228b  ADD             X5, SP, X2
0009C12C  e1179f1a  CSET            W1, EQ
0009C130  e7c30191  ADD             X7, SP, #0x2500+var_2490
0009C134  e6c30391  ADD             X6, SP, #0x2500+var_2410
0009C138  e4c31391  ADD             X4, SP, #0x2500+var_2010
0009C13C  e00313aa  MOV             X0, X19
0009C140  02008052  MOV             W2, #0
0009C144  effbff97  BL              _ZN15APACLRVQDecoder15LrvqDeserializeER16TBitstreamReaderIjEbbR16APACSpectralDataRN4apac13aligned_arrayIfLm1024EEES8_RNS6_IfLm256EEERNS6_IsLm64EEEPt; APACLRVQDecoder::LrvqDeserialize(TBitstreamReader<uint> &,bool,bool,APACSpectralData &,apac::aligned_array<float,1024ul> &,apac::aligned_array<float,1024ul> &,apac::aligned_array<float,256ul> &,apac::aligned_array<short,64ul> &,ushort *)
0009C148  d7ffff17  B               loc_9C0A4
0009C14C  610240f9  LDR             X1, [X19]
0009C150  e00313aa  MOV             X0, X19
0009C154  2b550094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C158  60064229  LDP             W0, W1, [X19,#0x10]
0009C15C  630240f9  LDR             X3, [X19]
0009C160  22800031  ADDS            W2, W1, #0x20 ; ' '
0009C164  e103014b  NEG             W1, W1
0009C168  0424c21a  LSR             W4, W0, W2
0009C16C  3903042a  ORR             W25, W25, W4
0009C170  0020c11a  LSL             W0, W0, W1
0009C174  61100091  ADD             X1, X3, #4
0009C178  e003801a  CSEL            W0, WZR, W0, EQ
0009C17C  610200f9  STR             X1, [X19]
0009C180  600a0229  STP             W0, W2, [X19,#0x10]
0009C184  9fffff17  B               loc_9C000
0009C188  610240f9  LDR             X1, [X19]
0009C18C  e00313aa  MOV             X0, X19
0009C190  1c550094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C194  60064229  LDP             W0, W1, [X19,#0x10]
0009C198  630240f9  LDR             X3, [X19]
0009C19C  22800031  ADDS            W2, W1, #0x20 ; ' '
0009C1A0  e103014b  NEG             W1, W1
0009C1A4  0424c21a  LSR             W4, W0, W2
0009C1A8  9402042a  ORR             W20, W20, W4
0009C1AC  0020c11a  LSL             W0, W0, W1
0009C1B0  61100091  ADD             X1, X3, #4
0009C1B4  e003801a  CSEL            W0, WZR, W0, EQ
0009C1B8  610200f9  STR             X1, [X19]
0009C1BC  600a0229  STP             W0, W2, [X19,#0x10]
0009C1C0  acffff17  B               loc_9C070
0009C1C4  e00315aa  MOV             X0, X21
0009C1C8  20003fd6  BLR             X1
0009C1CC  00008052  MOV             W0, #0
0009C1D0  b5ffff17  B               loc_9C0A4
0009C1D4  e00314aa  MOV             X0, X20
0009C1D8  20003fd6  BLR             X1
0009C1DC  95ffff17  B               loc_9C030
0009C1E0  4c4eff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009c1f0, for verification */
0009C1F0  e91340d1  SUB             X9, SP, #4,LSL#12
0009C1F4  3f8907f9  STR             XZR, [X9,#0xF10]
0009C1F8  fd7bb1a9  STP             X29, X30, [SP,#var_F0]!
0009C1FC  fd030091  MOV             X29, SP
0009C200  f35301a9  STP             X19, X20, [SP,#0xF0+var_E0]
0009C204  941c0090  ADRP            X20, #__stack_chk_guard_ptr@PAGE
0009C208  f30302aa  MOV             X19, X2
0009C20C  830247f9  LDR             X3, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C210  f55b02a9  STP             X21, X22, [SP,#0xF0+var_D0]
0009C214  f50301aa  MOV             X21, X1
0009C218  fb7305a9  STP             X27, X28, [SP,#0xF0+var_A0]
0009C21C  fc0300aa  MOV             X28, X0
0009C220  640040f9  LDR             X4, [X3]
0009C224  e47700f9  STR             X4, [SP,#0xF0+var_8]
0009C228  040080d2  MOV             X4, #0
0009C22C  427841f9  LDR             X2, [X2,#0x2F0]
0009C230  637e41f9  LDR             X3, [X19,#0x2F8]; float *
0009C234  5f0003eb  CMP             X2, X3
0009C238  a0000054  B.EQ            loc_9C24C
0009C23C  1f2003d5  NOP
0009C240  5f4400b8  STR             WZR, [X2],#4
0009C244  7f0002eb  CMP             X3, X2
0009C248  c1ffff54  B.NE            loc_9C240
0009C24C  82034229  LDP             W2, W0, [X28,#0x10]
0009C250  00040051  SUB             W0, W0, #1
0009C254  801700b9  STR             W0, [X28,#0x14]
0009C258  567c1f53  LSR             W22, W2, #0x1F
0009C25C  a011f837  TBNZ            W0, #0x1F, loc_9C490
0009C260  4200020b  ADD             W2, W2, W2
0009C264  821300b9  STR             W2, [X28,#0x10]
0009C268  df1e0072  TST             W22, #0xFF
0009C26C  a10f0054  B.NE            loc_9C460
0009C270  f76303a9  STP             X23, X24, [SP,#0xF0+var_C0]
0009C274  01040051  SUB             W1, W0, #1
0009C278  821340b9  LDR             W2, [X28,#0x10]
0009C27C  811700b9  STR             W1, [X28,#0x14]
0009C280  557c1f53  LSR             W21, W2, #0x1F
0009C284  0118f837  TBNZ            W1, #0x1F, loc_9C584
0009C288  00280051  SUB             W0, W0, #0xA
0009C28C  4200020b  ADD             W2, W2, W2
0009C290  821300b9  STR             W2, [X28,#0x10]
0009C294  801700b9  STR             W0, [X28,#0x14]
0009C298  b53e0012  AND             W21, W21, #0xFFFF
0009C29C  567c1753  LSR             W22, W2, #0x17
0009C2A0  001ff837  TBNZ            W0, #0x1F, loc_9C680
0009C2A4  42581753  LSL             W2, W2, #9
0009C2A8  821300b9  STR             W2, [X28,#0x10]
0009C2AC  f7b30291  ADD             X23, SP, #0xF0+var_44
0009C2B0  e103152a  MOV             W1, W21; unsigned __int16
0009C2B4  e003162a  MOV             W0, W22; this
0009C2B8  e20317aa  MOV             X2, X23; unsigned __int16
0009C2BC  a5f9ff97  BL              _ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf; APACLRVQDecoder::dequantizeLrvqGain(ushort,ushort,float *)
0009C2C0  fb03002a  MOV             W27, W0
0009C2C4  40150035  CBNZ            W0, loc_9C56C
0009C2C8  f96b04a9  STP             X25, X26, [SP,#0xF0+var_B0]
0009C2CC  81034229  LDP             W1, W0, [X28,#0x10]
0009C2D0  00100051  SUB             W0, W0, #4
0009C2D4  801700b9  STR             W0, [X28,#0x14]
0009C2D8  357c1c53  LSR             W21, W1, #0x1C
0009C2DC  c018f837  TBNZ            W0, #0x1F, loc_9C5F4
0009C2E0  216c1c53  LSL             W1, W1, #4
0009C2E4  811300b9  STR             W1, [X28,#0x10]
0009C2E8  b53e7bd3  UBFIZ           X21, X21, #5, #0x10
0009C2EC  601c00f0  ADRL            X0, unk_42B1F8
0009C2F4  0000158b  ADD             X0, X0, X21
0009C2F8  1ad840a9  LDP             X26, X22, [X0,#8]
0009C2FC  410316aa  ORR             X1, X26, X22
0009C300  410900b4  CBZ             X1, loc_9C428
0009C304  0130c079  LDRSH           W1, [X0,#0x18]
0009C308  1538c079  LDRSH           W21, [X0,#0x1C]
0009C30C  e17f00b9  STR             W1, [SP,#0xF0+var_74]
0009C310  2000152a  ORR             W0, W1, W21
0009C314  a0080034  CBZ             W0, loc_9C428
0009C318  e0c30291  ADD             X0, SP, #0xF0+var_40
0009C31C  981400f0  ADRL            X24, unk_32FEF8
0009C324  f90300aa  MOV             X25, X0
0009C328  02130091  ADD             X2, X24, #(unk_32FEFC - 0x32FEF8)
0009C32C  e00b08a9  STP             X0, X2, [SP,#0xF0+var_70]
0009C330  e0030391  ADD             X0, SP, #0xF0+var_30
0009C334  e03700f9  STR             X0, [SP,#0xF0+var_88]
0009C338  20040051  SUB             W0, W1, #1
0009C33C  e07700b9  STR             W0, [SP,#0xF0+var_7C]
0009C340  00048052  MOV             W0, #0x20 ; ' '
0009C344  0000014b  SUB             W0, W0, W1
0009C348  e09300b9  STR             W0, [SP,#0xF0+var_60]
0009C34C  a0060051  SUB             W0, W21, #1
0009C350  e07b00b9  STR             W0, [SP,#0xF0+var_78]
0009C354  e07740b9  LDR             W0, [SP,#0xF0+var_7C]
0009C358  e3031aaa  MOV             X3, X26
0009C35C  1f7c0071  CMP             W0, #0x1F
0009C360  88010054  B.HI            loc_9C390
0009C364  81034229  LDP             W1, W0, [X28,#0x10]
0009C368  e27f40b9  LDR             W2, [SP,#0xF0+var_74]
0009C36C  e3434239  LDRB            W3, [SP,#0xF0+var_60]
0009C370  0000024b  SUB             W0, W0, W2
0009C374  801700b9  STR             W0, [X28,#0x14]
0009C378  2324c31a  LSR             W3, W1, W3
0009C37C  8011f837  TBNZ            W0, #0x1F, loc_9C5AC
0009C380  633c1d13  SBFIZ           W3, W3, #3, #0x10
0009C384  2120c21a  LSL             W1, W1, W2
0009C388  811300b9  STR             W1, [X28,#0x10]
0009C38C  43cb238b  ADD             X3, X26, W3,SXTW#2
0009C390  f60f00b4  CBZ             X22, loc_9C58C
0009C394  e07b40b9  LDR             W0, [SP,#0xF0+var_78]
0009C398  e20316aa  MOV             X2, X22
0009C39C  1f7c0071  CMP             W0, #0x1F
0009C3A0  88010054  B.HI            loc_9C3D0
0009C3A4  84034229  LDP             W4, W0, [X28,#0x10]
0009C3A8  01048052  MOV             W1, #0x20 ; ' '
0009C3AC  2100154b  SUB             W1, W1, W21
0009C3B0  0000154b  SUB             W0, W0, W21
0009C3B4  801700b9  STR             W0, [X28,#0x14]
0009C3B8  8224c11a  LSR             W2, W4, W1
0009C3BC  a013f837  TBNZ            W0, #0x1F, loc_9C630
0009C3C0  423c1d13  SBFIZ           W2, W2, #3, #0x10
0009C3C4  8420d51a  LSL             W4, W4, W21
0009C3C8  841300b9  STR             W4, [X28,#0x10]
0009C3CC  c2ca228b  ADD             X2, X22, W2,SXTW#2
0009C3D0  e00303aa  MOV             X0, X3
0009C3D4  e50319aa  MOV             X5, X25
0009C3D8  230080d2  MOV             X3, #1
0009C3DC  e40318aa  MOV             X4, X24
0009C3E0  e10303aa  MOV             X1, X3
0009C3E4  670080d2  MOV             X7, #3
0009C3E8  860080d2  MOV             X6, #4
0009C3EC  a14e0094  BL              vDSP_vasm
0009C3F0  e03740f9  LDR             X0, [SP,#0xF0+var_88]
0009C3F4  39130091  ADD             X25, X25, #4
0009C3F8  3f0300eb  CMP             X25, X0
0009C3FC  c1faff54  B.NE            loc_9C354
0009C400  e04340f9  LDR             X0, [SP,#0xF0+var_70]
0009C404  e20317aa  MOV             X2, X23
0009C408  637a41f9  LDR             X3, [X19,#0x2F0]
0009C40C  240080d2  MOV             X4, #1
0009C410  850180d2  MOV             X5, #0xC
0009C414  e10304aa  MOV             X1, X4
0009C418  324e0094  BL              vDSP_vsmul
0009C41C  f76343a9  LDP             X23, X24, [SP,#0xF0+var_C0]
0009C420  f96b44a9  LDP             X25, X26, [SP,#0xF0+var_B0]
0009C424  04000014  B               loc_9C434
0009C428  f76343a9  LDP             X23, X24, [SP,#0xF0+var_C0]
0009C42C  3b008052  MOV             W27, #1
0009C430  f96b44a9  LDP             X25, X26, [SP,#0xF0+var_B0]
0009C434  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C438  e17740f9  LDR             X1, [SP,#0xF0+var_8]
0009C43C  800240f9  LDR             X0, [X20]
0009C440  200000ca  EOR             X0, X1, X0
0009C444  601400b5  CBNZ            X0, loc_9C6D0
0009C448  e0031b2a  MOV             W0, W27
0009C44C  f35341a9  LDP             X19, X20, [SP,#0xF0+var_E0]
0009C450  f55b42a9  LDP             X21, X22, [SP,#0xF0+var_D0]
0009C454  fb7345a9  LDP             X27, X28, [SP,#0xF0+var_A0]
0009C458  fd7bcfa8  LDP             X29, X30, [SP+0xF0+var_F0],#0xF0
0009C45C  c0035fd6  RET
0009C460  a10240f9  LDR             X1, [X21]
0009C464  801c0090  ADRP            X0, #_ZN11APACICSInfo5ResetEv_ptr@PAGE
0009C468  006447f9  LDR             X0, [X0,#_ZN11APACICSInfo5ResetEv_ptr@PAGEOFF]; APACICSInfo::Reset(void) ...
0009C46C  210c40f9  LDR             X1, [X1,#0x18]
0009C470  3f0000eb  CMP             X1, X0
0009C474  21110054  B.NE            loc_9C698
0009C478  20008052  MOV             W0, #1
0009C47C  1b008052  MOV             W27, #0
0009C480  bf160079  STRH            WZR, [X21,#0xA]
0009C484  bf320039  STRB            WZR, [X21,#0xC]
0009C488  a01200b9  STR             W0, [X21,#0x10]
0009C48C  eaffff17  B               loc_9C434
0009C490  810340f9  LDR             X1, [X28]
0009C494  e0031caa  MOV             X0, X28
0009C498  5a540094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C49C  82034229  LDP             W2, W0, [X28,#0x10]
0009C4A0  810340f9  LDR             X1, [X28]
0009C4A4  e303002a  MOV             W3, W0
0009C4A8  00800011  ADD             W0, W0, #0x20 ; ' '
0009C4AC  801700b9  STR             W0, [X28,#0x14]
0009C4B0  21100091  ADD             X1, X1, #4
0009C4B4  810300f9  STR             X1, [X28]
0009C4B8  4424c01a  LSR             W4, W2, W0
0009C4BC  d602042a  ORR             W22, W22, W4
0009C4C0  a0050035  CBNZ            W0, loc_9C574
0009C4C4  9f1300b9  STR             WZR, [X28,#0x10]
0009C4C8  df1e0072  TST             W22, #0xFF
0009C4CC  a1fcff54  B.NE            loc_9C460
0009C4D0  f76303a9  STP             X23, X24, [SP,#0xF0+var_C0]
0009C4D4  00008012  MOV             W0, #0xFFFFFFFF
0009C4D8  15008052  MOV             W21, #0
0009C4DC  801700b9  STR             W0, [X28,#0x14]
0009C4E0  e0031caa  MOV             X0, X28
0009C4E4  47540094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C4E8  820f4229  LDP             W2, W3, [X28,#0x10]
0009C4EC  810340f9  LDR             X1, [X28]
0009C4F0  64800011  ADD             W4, W3, #0x20 ; ' '
0009C4F4  841700b9  STR             W4, [X28,#0x14]
0009C4F8  605c0011  ADD             W0, W3, #0x17
0009C4FC  21100091  ADD             X1, X1, #4
0009C500  810300f9  STR             X1, [X28]
0009C504  4524c41a  LSR             W5, W2, W4
0009C508  b502052a  ORR             W21, W21, W5
0009C50C  e40b0035  CBNZ            W4, loc_9C688
0009C510  b53e0012  AND             W21, W21, #0xFFFF
0009C514  16008052  MOV             W22, #0
0009C518  9f030229  STP             WZR, W0, [X28,#0x10]
0009C51C  e0031caa  MOV             X0, X28
0009C520  38540094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C524  80074229  LDP             W0, W1, [X28,#0x10]
0009C528  f7b30291  ADD             X23, SP, #0xF0+var_44
0009C52C  830340f9  LDR             X3, [X28]; float *
0009C530  22800031  ADDS            W2, W1, #0x20 ; ' '
0009C534  e103014b  NEG             W1, W1
0009C538  0424c21a  LSR             W4, W0, W2
0009C53C  d602042a  ORR             W22, W22, W4
0009C540  0020c11a  LSL             W0, W0, W1
0009C544  61100091  ADD             X1, X3, #4
0009C548  e003801a  CSEL            W0, WZR, W0, EQ
0009C54C  810300f9  STR             X1, [X28]
0009C550  800b0229  STP             W0, W2, [X28,#0x10]
0009C554  e103152a  MOV             W1, W21; unsigned __int16
0009C558  e003162a  MOV             W0, W22; this
0009C55C  e20317aa  MOV             X2, X23; unsigned __int16
0009C560  fcf8ff97  BL              _ZN15APACLRVQDecoder18dequantizeLrvqGainEttPf; APACLRVQDecoder::dequantizeLrvqGain(ushort,ushort,float *)
0009C564  fb03002a  MOV             W27, W0
0009C568  00ebff34  CBZ             W0, loc_9C2C8
0009C56C  f76343a9  LDP             X23, X24, [SP,#0xF0+var_C0]
0009C570  b1ffff17  B               loc_9C434
0009C574  e303034b  NEG             W3, W3
0009C578  4220c31a  LSL             W2, W2, W3
0009C57C  821300b9  STR             W2, [X28,#0x10]
0009C580  3affff17  B               loc_9C268
0009C584  810340f9  LDR             X1, [X28]
0009C588  d6ffff17  B               loc_9C4E0
0009C58C  e24740f9  LDR             X2, [SP,#0xF0+var_68]
0009C590  e00303aa  MOV             X0, X3
0009C594  650080d2  MOV             X5, #3
0009C598  e30319aa  MOV             X3, X25
0009C59C  840080d2  MOV             X4, #4
0009C5A0  210080d2  MOV             X1, #1
0009C5A4  af4d0094  BL              vDSP_vsadd
0009C5A8  92ffff17  B               loc_9C3F0
0009C5AC  810340f9  LDR             X1, [X28]
0009C5B0  e0031caa  MOV             X0, X28
0009C5B4  e39700b9  STR             W3, [SP,#0xF0+var_5C]
0009C5B8  12540094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C5BC  82134229  LDP             W2, W4, [X28,#0x10]
0009C5C0  800340f9  LDR             X0, [X28]
0009C5C4  81800011  ADD             W1, W4, #0x20 ; ' '
0009C5C8  e39740b9  LDR             W3, [SP,#0xF0+var_5C]
0009C5CC  00100091  ADD             X0, X0, #4
0009C5D0  800300f9  STR             X0, [X28]
0009C5D4  4024c11a  LSR             W0, W2, W1
0009C5D8  0300032a  ORR             W3, W0, W3
0009C5DC  811700b9  STR             W1, [X28,#0x14]
0009C5E0  633c1d13  SBFIZ           W3, W3, #3, #0x10
0009C5E4  21060035  CBNZ            W1, loc_9C6A8
0009C5E8  43cb238b  ADD             X3, X26, W3,SXTW#2
0009C5EC  9f1300b9  STR             WZR, [X28,#0x10]
0009C5F0  68ffff17  B               loc_9C390
0009C5F4  810340f9  LDR             X1, [X28]
0009C5F8  e0031caa  MOV             X0, X28
0009C5FC  01540094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C600  80074229  LDP             W0, W1, [X28,#0x10]
0009C604  830340f9  LDR             X3, [X28]
0009C608  22800031  ADDS            W2, W1, #0x20 ; ' '
0009C60C  e103014b  NEG             W1, W1
0009C610  0424c21a  LSR             W4, W0, W2
0009C614  b502042a  ORR             W21, W21, W4
0009C618  0020c11a  LSL             W0, W0, W1
0009C61C  61100091  ADD             X1, X3, #4
0009C620  e003801a  CSEL            W0, WZR, W0, EQ
0009C624  810300f9  STR             X1, [X28]
0009C628  800b0229  STP             W0, W2, [X28,#0x10]
0009C62C  2fffff17  B               loc_9C2E8
0009C630  810340f9  LDR             X1, [X28]
0009C634  e0031caa  MOV             X0, X28
0009C638  e29700b9  STR             W2, [SP,#0xF0+var_5C]
0009C63C  e34f00f9  STR             X3, [SP,#0xF0+var_58]
0009C640  f0530094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009C644  84174229  LDP             W4, W5, [X28,#0x10]
0009C648  810340f9  LDR             X1, [X28]
0009C64C  a0800011  ADD             W0, W5, #0x20 ; ' '
0009C650  e29740b9  LDR             W2, [SP,#0xF0+var_5C]
0009C654  21100091  ADD             X1, X1, #4
0009C658  810300f9  STR             X1, [X28]
0009C65C  8124c01a  LSR             W1, W4, W0
0009C660  2100022a  ORR             W1, W1, W2
0009C664  801700b9  STR             W0, [X28,#0x14]
0009C668  223c1d13  SBFIZ           W2, W1, #3, #0x10
0009C66C  e34f40f9  LDR             X3, [SP,#0xF0+var_58]
0009C670  60020035  CBNZ            W0, loc_9C6BC
0009C674  c2ca228b  ADD             X2, X22, W2,SXTW#2
0009C678  9f1300b9  STR             WZR, [X28,#0x10]
0009C67C  55ffff17  B               loc_9C3D0
0009C680  810340f9  LDR             X1, [X28]
0009C684  a6ffff17  B               loc_9C51C
0009C688  e303034b  NEG             W3, W3
0009C68C  4220c31a  LSL             W2, W2, W3
0009C690  821300b9  STR             W2, [X28,#0x10]
0009C694  00ffff17  B               loc_9C294
0009C698  e00315aa  MOV             X0, X21
0009C69C  1b008052  MOV             W27, #0
0009C6A0  20003fd6  BLR             X1
0009C6A4  64ffff17  B               loc_9C434
0009C6A8  e403044b  NEG             W4, W4
0009C6AC  43cb238b  ADD             X3, X26, W3,SXTW#2
0009C6B0  4220c41a  LSL             W2, W2, W4
0009C6B4  821300b9  STR             W2, [X28,#0x10]
0009C6B8  36ffff17  B               loc_9C390
0009C6BC  e503054b  NEG             W5, W5
0009C6C0  c2ca228b  ADD             X2, X22, W2,SXTW#2
0009C6C4  8420c51a  LSL             W4, W4, W5
0009C6C8  841300b9  STR             W4, [X28,#0x10]
0009C6CC  41ffff17  B               loc_9C3D0
0009C6D0  f76303a9  STP             X23, X24, [SP,#0xF0+var_C0]
0009C6D4  f96b04a9  STP             X25, X26, [SP,#0xF0+var_B0]
0009C6D8  0e4dff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009c830, for verification */
0009C830  e91340d1  SUB             X9, SP, #4,LSL#12
0009C834  3ff107f9  STR             XZR, [X9,#0xFE0]
0009C838  831c0090  ADRP            X3, #__stack_chk_guard_ptr@PAGE
0009C83C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
0009C840  821c00b0  ADRP            X2, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009C844  811c00b0  ADRP            X1, #_ZTV17MultiRadixRealFFT_ptr@PAGE
0009C848  fd030091  MOV             X29, SP
0009C84C  630047f9  LDR             X3, [X3,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C850  e40300aa  MOV             X4, X0
0009C854  425441f9  LDR             X2, [X2,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009C858  650040f9  LDR             X5, [X3]
0009C85C  e50f00f9  STR             X5, [SP,#0x20+var_8]
0009C860  050080d2  MOV             X5, #0
0009C864  21f445f9  LDR             X1, [X1,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009C868  42400091  ADD             X2, X2, #0x10
0009C86C  020401f8  STR             X2, [X0],#0x10
0009C870  21400091  ADD             X1, X1, #0x10
0009C874  810800f9  STR             X1, [X4,#0x10]
0009C878  e20f40f9  LDR             X2, [SP,#0x20+var_8]
0009C87C  610040f9  LDR             X1, [X3]
0009C880  410001ca  EOR             X1, X2, X1
0009C884  610000b5  CBNZ            X1, loc_9C890
0009C888  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
0009C88C  cd9b0114  B               _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009C890  a04cff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009c8a0, for verification */
0009C8A0  e91340d1  SUB             X9, SP, #4,LSL#12
0009C8A4  3fe907f9  STR             XZR, [X9,#0xFD0]
0009C8A8  fd7bbda9  STP             X29, X30, [SP,#var_30]!
0009C8AC  821c00b0  ADRP            X2, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009C8B0  811c00b0  ADRP            X1, #_ZTV17MultiRadixRealFFT_ptr@PAGE
0009C8B4  fd030091  MOV             X29, SP
0009C8B8  f35301a9  STP             X19, X20, [SP,#0x30+var_20]
0009C8BC  941c0090  ADRP            X20, #__stack_chk_guard_ptr@PAGE
0009C8C0  f30300aa  MOV             X19, X0
0009C8C4  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C8C8  425441f9  LDR             X2, [X2,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009C8CC  830240f9  LDR             X3, [X20]
0009C8D0  e31700f9  STR             X3, [SP,#0x30+var_8]
0009C8D4  030080d2  MOV             X3, #0
0009C8D8  21f445f9  LDR             X1, [X1,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009C8DC  42400091  ADD             X2, X2, #0x10
0009C8E0  020401f8  STR             X2, [X0],#0x10
0009C8E4  21400091  ADD             X1, X1, #0x10
0009C8E8  610a00f9  STR             X1, [X19,#0x10]
0009C8EC  b59b0194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009C8F0  e11740f9  LDR             X1, [SP,#0x30+var_8]
0009C8F4  800240f9  LDR             X0, [X20]
0009C8F8  200000ca  EOR             X0, X1, X0
0009C8FC  a00000b5  CBNZ            X0, loc_9C910
0009C900  e00313aa  MOV             X0, X19; void *
0009C904  f35341a9  LDP             X19, X20, [SP,#0x30+var_20]
0009C908  fd7bc3a8  LDP             X29, X30, [SP+0x30+var_30],#0x30
0009C90C  e54dff17  B               ._ZdlPv; operator delete(void *)
0009C910  804cff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009c920, for verification */
0009C920  e91340d1  SUB             X9, SP, #4,LSL#12
0009C924  3ff107f9  STR             XZR, [X9,#0xFE0]
0009C928  831c0090  ADRP            X3, #__stack_chk_guard_ptr@PAGE
0009C92C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
0009C930  821c00b0  ADRP            X2, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009C934  811c00b0  ADRP            X1, #_ZTV17MultiRadixRealFFT_ptr@PAGE
0009C938  fd030091  MOV             X29, SP
0009C93C  630047f9  LDR             X3, [X3,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C940  e40300aa  MOV             X4, X0
0009C944  425441f9  LDR             X2, [X2,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009C948  650040f9  LDR             X5, [X3]
0009C94C  e50f00f9  STR             X5, [SP,#0x20+var_8]
0009C950  050080d2  MOV             X5, #0
0009C954  21f445f9  LDR             X1, [X1,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009C958  42400091  ADD             X2, X2, #0x10
0009C95C  020401f8  STR             X2, [X0],#0x10
0009C960  21400091  ADD             X1, X1, #0x10
0009C964  810800f9  STR             X1, [X4,#0x10]
0009C968  e20f40f9  LDR             X2, [SP,#0x20+var_8]
0009C96C  610040f9  LDR             X1, [X3]
0009C970  410001ca  EOR             X1, X2, X1
0009C974  610000b5  CBNZ            X1, loc_9C980
0009C978  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
0009C97C  919b0114  B               _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009C980  644cff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009c990, for verification */
0009C990  e91340d1  SUB             X9, SP, #4,LSL#12
0009C994  3fe907f9  STR             XZR, [X9,#0xFD0]
0009C998  fd7bbda9  STP             X29, X30, [SP,#var_30]!
0009C99C  821c00b0  ADRP            X2, #_ZTVN4apac12LPMDCTFilterE_ptr@PAGE
0009C9A0  811c00b0  ADRP            X1, #_ZTV17MultiRadixRealFFT_ptr@PAGE
0009C9A4  fd030091  MOV             X29, SP
0009C9A8  f35301a9  STP             X19, X20, [SP,#0x30+var_20]
0009C9AC  941c0090  ADRP            X20, #__stack_chk_guard_ptr@PAGE
0009C9B0  f30300aa  MOV             X19, X0
0009C9B4  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009C9B8  425441f9  LDR             X2, [X2,#_ZTVN4apac12LPMDCTFilterE_ptr@PAGEOFF]; `vtable for'apac::LPMDCTFilter ...
0009C9BC  830240f9  LDR             X3, [X20]
0009C9C0  e31700f9  STR             X3, [SP,#0x30+var_8]
0009C9C4  030080d2  MOV             X3, #0
0009C9C8  21f445f9  LDR             X1, [X1,#_ZTV17MultiRadixRealFFT_ptr@PAGEOFF]; `vtable for'MultiRadixRealFFT ...
0009C9CC  42400091  ADD             X2, X2, #0x10
0009C9D0  020401f8  STR             X2, [X0],#0x10
0009C9D4  21400091  ADD             X1, X1, #0x10
0009C9D8  610a00f9  STR             X1, [X19,#0x10]
0009C9DC  799b0194  BL              _ZN9DFTHandleILb0EED1Ev; DFTHandle<false>::~DFTHandle()
0009C9E0  e11740f9  LDR             X1, [SP,#0x30+var_8]
0009C9E4  800240f9  LDR             X0, [X20]
0009C9E8  200000ca  EOR             X0, X1, X0
0009C9EC  a00000b5  CBNZ            X0, loc_9CA00
0009C9F0  e00313aa  MOV             X0, X19; void *
0009C9F4  f35341a9  LDP             X19, X20, [SP,#0x30+var_20]
0009C9F8  fd7bc3a8  LDP             X29, X30, [SP+0x30+var_30],#0x30
0009C9FC  a94dff17  B               ._ZdlPv; operator delete(void *)
0009CA00  444cff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009ca10, for verification */
0009CA10  e91340d1  SUB             X9, SP, #4,LSL#12
0009CA14  3fd907f9  STR             XZR, [X9,#0xFB0]
0009CA18  fd7bbba9  STP             X29, X30, [SP,#var_50]!
0009CA1C  811c00b0  ADRP            X1, #_ZTVN4apac8LSFToolsE_ptr@PAGE
0009CA20  fd030091  MOV             X29, SP
0009CA24  f55b02a9  STP             X21, X22, [SP,#0x50+var_30]
0009CA28  961c0090  ADRP            X22, #__stack_chk_guard_ptr@PAGE
0009CA2C  f35301a9  STP             X19, X20, [SP,#0x50+var_40]
0009CA30  f30300aa  MOV             X19, X0
0009CA34  c00247f9  LDR             X0, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009CA38  213840f9  LDR             X1, [X1,#_ZTVN4apac8LSFToolsE_ptr@PAGEOFF]; `vtable for'apac::LSFTools ...
0009CA3C  020040f9  LDR             X2, [X0]
0009CA40  e22700f9  STR             X2, [SP,#0x50+var_8]
0009CA44  020080d2  MOV             X2, #0
0009CA48  606e40f9  LDR             X0, [X19,#0xD8]; void *
0009CA4C  21400091  ADD             X1, X1, #0x10
0009CA50  610200f9  STR             X1, [X19]
0009CA54  400000b4  CBZ             X0, loc_9CA5C
0009CA58  924dff97  BL              ._ZdlPv; operator delete(void *)
0009CA5C  606240f9  LDR             X0, [X19,#0xC0]; void *
0009CA60  400000b4  CBZ             X0, loc_9CA68
0009CA64  8f4dff97  BL              ._ZdlPv; operator delete(void *)
0009CA68  605640f9  LDR             X0, [X19,#0xA8]; void *
0009CA6C  400000b4  CBZ             X0, loc_9CA74
0009CA70  8c4dff97  BL              ._ZdlPv; operator delete(void *)
0009CA74  604a40f9  LDR             X0, [X19,#0x90]; void *
0009CA78  400000b4  CBZ             X0, loc_9CA80
0009CA7C  894dff97  BL              ._ZdlPv; operator delete(void *)
0009CA80  603e40f9  LDR             X0, [X19,#0x78]; void *
0009CA84  400000b4  CBZ             X0, loc_9CA8C
0009CA88  864dff97  BL              ._ZdlPv; operator delete(void *)
0009CA8C  801c00b0  ADRP            X0, #_ZTVN4apac7LPCBaseE_ptr@PAGE
0009CA90  e10313aa  MOV             X1, X19
0009CA94  00ec45f9  LDR             X0, [X0,#_ZTVN4apac7LPCBaseE_ptr@PAGEOFF]; `vtable for'apac::LPCBase ...
0009CA98  00400091  ADD             X0, X0, #0x10
0009CA9C  200405f8  STR             X0, [X1],#0x50
0009CAA0  350440f9  LDR             X21, [X1,#8]
0009CAA4  742a40f9  LDR             X20, [X19,#0x50]
0009CAA8  9f0215eb  CMP             X20, X21
0009CAAC  40020054  B.EQ            loc_9CAF4
0009CAB0  f71b00f9  STR             X23, [SP,#0x50+var_20]
0009CAB4  1f2003d5  NOP
0009CAB8  970240f9  LDR             X23, [X20]
0009CABC  370100b4  CBZ             X23, loc_9CAE0
0009CAC0  e01a40f9  LDR             X0, [X23,#0x30]; void *
0009CAC4  400000b4  CBZ             X0, loc_9CACC
0009CAC8  764dff97  BL              ._ZdlPv; operator delete(void *)
0009CACC  e00a40f9  LDR             X0, [X23,#0x10]; void *
0009CAD0  400000b4  CBZ             X0, loc_9CAD8
0009CAD4  734dff97  BL              ._ZdlPv; operator delete(void *)
0009CAD8  e00317aa  MOV             X0, X23; void *
0009CADC  714dff97  BL              ._ZdlPv; operator delete(void *)
0009CAE0  94220091  ADD             X20, X20, #8
0009CAE4  bf0214eb  CMP             X21, X20
0009CAE8  81feff54  B.NE            loc_9CAB8
0009CAEC  f71b40f9  LDR             X23, [SP,#0x50+var_20]
0009CAF0  752a40f9  LDR             X21, [X19,#0x50]
0009CAF4  750000b4  CBZ             X21, loc_9CB00
0009CAF8  e00315aa  MOV             X0, X21; void *
0009CAFC  694dff97  BL              ._ZdlPv; operator delete(void *)
0009CB00  742640f9  LDR             X20, [X19,#0x48]
0009CB04  140200b4  CBZ             X20, loc_9CB44
0009CB08  810240f9  LDR             X1, [X20]
0009CB0C  801c00b0  ADRP            X0, #_ZN4apac8AutocorrD0Ev_ptr@PAGE
0009CB10  00e046f9  LDR             X0, [X0,#_ZN4apac8AutocorrD0Ev_ptr@PAGEOFF]; apac::Autocorr::~Autocorr() ...
0009CB14  210440f9  LDR             X1, [X1,#8]
0009CB18  3f0000eb  CMP             X1, X0
0009CB1C  01040054  B.NE            loc_9CB9C
0009CB20  811c00b0  ADRP            X1, #_ZTVN4apac8AutocorrE_ptr@PAGE
0009CB24  800a40f9  LDR             X0, [X20,#0x10]; void *
0009CB28  21ec43f9  LDR             X1, [X1,#_ZTVN4apac8AutocorrE_ptr@PAGEOFF]; `vtable for'apac::Autocorr ...
0009CB2C  21400091  ADD             X1, X1, #0x10
0009CB30  810200f9  STR             X1, [X20]
0009CB34  400000b4  CBZ             X0, loc_9CB3C
0009CB38  5a4dff97  BL              ._ZdlPv; operator delete(void *)
0009CB3C  e00314aa  MOV             X0, X20; void *
0009CB40  584dff97  BL              ._ZdlPv; operator delete(void *)
0009CB44  601a40f9  LDR             X0, [X19,#0x30]; void *
0009CB48  400000b4  CBZ             X0, loc_9CB50
0009CB4C  554dff97  BL              ._ZdlPv; operator delete(void *)
0009CB50  600e40f9  LDR             X0, [X19,#0x18]; void *
0009CB54  d60247f9  LDR             X22, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009CB58  200100b4  CBZ             X0, loc_9CB7C
0009CB5C  e22740f9  LDR             X2, [SP,#0x50+var_8]
0009CB60  c10240f9  LDR             X1, [X22]
0009CB64  410001ca  EOR             X1, X2, X1
0009CB68  010200b5  CBNZ            X1, loc_9CBA8
0009CB6C  f35341a9  LDP             X19, X20, [SP,#0x50+var_40]
0009CB70  f55b42a9  LDP             X21, X22, [SP,#0x50+var_30]
0009CB74  fd7bc5a8  LDP             X29, X30, [SP+0x50+var_50],#0x50
0009CB78  4a4dff17  B               ._ZdlPv; operator delete(void *)
0009CB7C  e12740f9  LDR             X1, [SP,#0x50+var_8]
0009CB80  c00240f9  LDR             X0, [X22]
0009CB84  200000ca  EOR             X0, X1, X0
0009CB88  000100b5  CBNZ            X0, loc_9CBA8
0009CB8C  f35341a9  LDP             X19, X20, [SP,#0x50+var_40]
0009CB90  f55b42a9  LDP             X21, X22, [SP,#0x50+var_30]
0009CB94  fd7bc5a8  LDP             X29, X30, [SP+0x50+var_50],#0x50
0009CB98  c0035fd6  RET
0009CB9C  e00314aa  MOV             X0, X20
0009CBA0  20003fd6  BLR             X1
0009CBA4  e8ffff17  B               loc_9CB44
0009CBA8  f71b00f9  STR             X23, [SP,#0x50+var_20]
0009CBAC  d94bff97  BL              .__stack_chk_fail
#endif

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


#if 0 /* disassembly of 0x0009cbb0, for verification */
0009CBB0  e91340d1  SUB             X9, SP, #4,LSL#12
0009CBB4  3fd907f9  STR             XZR, [X9,#0xFB0]
0009CBB8  fd7bbba9  STP             X29, X30, [SP,#var_50]!
0009CBBC  811c00b0  ADRP            X1, #_ZTVN4apac8LSFToolsE_ptr@PAGE
0009CBC0  fd030091  MOV             X29, SP
0009CBC4  f55b02a9  STP             X21, X22, [SP,#0x50+var_30]
0009CBC8  961c0090  ADRP            X22, #__stack_chk_guard_ptr@PAGE
0009CBCC  f35301a9  STP             X19, X20, [SP,#0x50+var_40]
0009CBD0  f30300aa  MOV             X19, X0
0009CBD4  c00247f9  LDR             X0, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009CBD8  213840f9  LDR             X1, [X1,#_ZTVN4apac8LSFToolsE_ptr@PAGEOFF]; `vtable for'apac::LSFTools ...
0009CBDC  020040f9  LDR             X2, [X0]
0009CBE0  e22700f9  STR             X2, [SP,#0x50+var_8]
0009CBE4  020080d2  MOV             X2, #0
0009CBE8  606e40f9  LDR             X0, [X19,#0xD8]; void *
0009CBEC  21400091  ADD             X1, X1, #0x10
0009CBF0  610200f9  STR             X1, [X19]
0009CBF4  400000b4  CBZ             X0, loc_9CBFC
0009CBF8  2a4dff97  BL              ._ZdlPv; operator delete(void *)
0009CBFC  606240f9  LDR             X0, [X19,#0xC0]; void *
0009CC00  400000b4  CBZ             X0, loc_9CC08
0009CC04  274dff97  BL              ._ZdlPv; operator delete(void *)
0009CC08  605640f9  LDR             X0, [X19,#0xA8]; void *
0009CC0C  400000b4  CBZ             X0, loc_9CC14
0009CC10  244dff97  BL              ._ZdlPv; operator delete(void *)
0009CC14  604a40f9  LDR             X0, [X19,#0x90]; void *
0009CC18  400000b4  CBZ             X0, loc_9CC20
0009CC1C  214dff97  BL              ._ZdlPv; operator delete(void *)
0009CC20  603e40f9  LDR             X0, [X19,#0x78]; void *
0009CC24  400000b4  CBZ             X0, loc_9CC2C
0009CC28  1e4dff97  BL              ._ZdlPv; operator delete(void *)
0009CC2C  801c00b0  ADRP            X0, #_ZTVN4apac7LPCBaseE_ptr@PAGE
0009CC30  e10313aa  MOV             X1, X19
0009CC34  00ec45f9  LDR             X0, [X0,#_ZTVN4apac7LPCBaseE_ptr@PAGEOFF]; `vtable for'apac::LPCBase ...
0009CC38  00400091  ADD             X0, X0, #0x10
0009CC3C  200405f8  STR             X0, [X1],#0x50
0009CC40  350440f9  LDR             X21, [X1,#8]
0009CC44  742a40f9  LDR             X20, [X19,#0x50]
0009CC48  9f0215eb  CMP             X20, X21
0009CC4C  40020054  B.EQ            loc_9CC94
0009CC50  f71b00f9  STR             X23, [SP,#0x50+var_20]
0009CC54  1f2003d5  NOP
0009CC58  970240f9  LDR             X23, [X20]
0009CC5C  370100b4  CBZ             X23, loc_9CC80
0009CC60  e01a40f9  LDR             X0, [X23,#0x30]; void *
0009CC64  400000b4  CBZ             X0, loc_9CC6C
0009CC68  0e4dff97  BL              ._ZdlPv; operator delete(void *)
0009CC6C  e00a40f9  LDR             X0, [X23,#0x10]; void *
0009CC70  400000b4  CBZ             X0, loc_9CC78
0009CC74  0b4dff97  BL              ._ZdlPv; operator delete(void *)
0009CC78  e00317aa  MOV             X0, X23; void *
0009CC7C  094dff97  BL              ._ZdlPv; operator delete(void *)
0009CC80  94220091  ADD             X20, X20, #8
0009CC84  bf0214eb  CMP             X21, X20
0009CC88  81feff54  B.NE            loc_9CC58
0009CC8C  f71b40f9  LDR             X23, [SP,#0x50+var_20]
0009CC90  752a40f9  LDR             X21, [X19,#0x50]
0009CC94  750000b4  CBZ             X21, loc_9CCA0
0009CC98  e00315aa  MOV             X0, X21; void *
0009CC9C  014dff97  BL              ._ZdlPv; operator delete(void *)
0009CCA0  742640f9  LDR             X20, [X19,#0x48]
0009CCA4  140200b4  CBZ             X20, loc_9CCE4
0009CCA8  810240f9  LDR             X1, [X20]
0009CCAC  801c00b0  ADRP            X0, #_ZN4apac8AutocorrD0Ev_ptr@PAGE
0009CCB0  00e046f9  LDR             X0, [X0,#_ZN4apac8AutocorrD0Ev_ptr@PAGEOFF]; apac::Autocorr::~Autocorr() ...
0009CCB4  210440f9  LDR             X1, [X1,#8]
0009CCB8  3f0000eb  CMP             X1, X0
0009CCBC  41030054  B.NE            loc_9CD24
0009CCC0  811c00b0  ADRP            X1, #_ZTVN4apac8AutocorrE_ptr@PAGE
0009CCC4  800a40f9  LDR             X0, [X20,#0x10]; void *
0009CCC8  21ec43f9  LDR             X1, [X1,#_ZTVN4apac8AutocorrE_ptr@PAGEOFF]; `vtable for'apac::Autocorr ...
0009CCCC  21400091  ADD             X1, X1, #0x10
0009CCD0  810200f9  STR             X1, [X20]
0009CCD4  400000b4  CBZ             X0, loc_9CCDC
0009CCD8  f24cff97  BL              ._ZdlPv; operator delete(void *)
0009CCDC  e00314aa  MOV             X0, X20; void *
0009CCE0  f04cff97  BL              ._ZdlPv; operator delete(void *)
0009CCE4  601a40f9  LDR             X0, [X19,#0x30]; void *
0009CCE8  400000b4  CBZ             X0, loc_9CCF0
0009CCEC  ed4cff97  BL              ._ZdlPv; operator delete(void *)
0009CCF0  600e40f9  LDR             X0, [X19,#0x18]; void *
0009CCF4  400000b4  CBZ             X0, loc_9CCFC
0009CCF8  ea4cff97  BL              ._ZdlPv; operator delete(void *)
0009CCFC  d60247f9  LDR             X22, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009CD00  e12740f9  LDR             X1, [SP,#0x50+var_8]
0009CD04  c00240f9  LDR             X0, [X22]
0009CD08  200000ca  EOR             X0, X1, X0
0009CD0C  200100b5  CBNZ            X0, loc_9CD30
0009CD10  e00313aa  MOV             X0, X19; void *
0009CD14  f35341a9  LDP             X19, X20, [SP,#0x50+var_40]
0009CD18  f55b42a9  LDP             X21, X22, [SP,#0x50+var_30]
0009CD1C  fd7bc5a8  LDP             X29, X30, [SP+0x50+var_50],#0x50
0009CD20  e04cff17  B               ._ZdlPv; operator delete(void *)
0009CD24  e00314aa  MOV             X0, X20
0009CD28  20003fd6  BLR             X1
0009CD2C  eeffff17  B               loc_9CCE4
0009CD30  f71b00f9  STR             X23, [SP,#0x50+var_20]
0009CD34  774bff97  BL              .__stack_chk_fail
#endif

