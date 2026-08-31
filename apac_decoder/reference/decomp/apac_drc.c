/* ============================================================================
 * apac::drc
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 10 function(s), 6856 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00073590    316  apac::drc::DynRangeCompressor::~DynRangeCompressor() */
/* 0x000736d0    360  apac::drc::DynRangeCompressor::~DynRangeCompressor() */
/* 0x0007ea20    160  apac::drc::DynRangeCompressor::APACDRCExtensionPayload::Deserialize(TBitstreamReader<unsigned int>&, mpddrc::HeaderPayloadType) */
/* 0x0007eac0    500  apac::drc::DynRangeCompressor::AdjustForHostInput() */
/* 0x0007ecc0    100  apac::drc::DynRangeCompressor::Deserialize(TBitstreamReader<unsigned int>&, mpddrc::HeaderPayloadType) */
/* 0x0007ed30    240  apac::drc::DynRangeCompressor::ProcessUniDrc(float const**, float**, unsigned int&) */
/* 0x0007ee20     68  apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload() */
/* 0x0007ee70   3796  apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(apac::drc::DynRangeCompressor&, DRCConfiguration const&, unsigned int, unsigned int, unsigned int, unsigned int, mpddrc::UniDrcDomain) */
/* 0x0007fd50    604  apac::drc::DynRangeCompressor::Initialize(DRCConfiguration*, float, unsigned int, unsigned int, unsigned int) */
/* 0x0007ffb0    712  apac::drc::DynRangeCompressor::Initialize(apac::ParamList const&) */

/* ------------------------------------------------------------------------ */
/* 0x00073590  size 316 bytes  79 insns
 *   apac::drc::DynRangeCompressor::~DynRangeCompressor()
 *   [_ZN4apac3drc18DynRangeCompressorD0Ev]
 *   data refs: 0x733c0, 0x73400, 0x42a330, 0x42cc70, 0x42cd68, 0x42ce00, 0x42d2e0, 0x42d618, 0x43e520, 0x43ec98
 */
void __fastcall apac::drc::DynRangeCompressor::~DynRangeCompressor(apac__drc__DynRangeCompressor *this)
{
  unsigned int *v2; // x21
  unsigned int *v3; // x1
  unsigned int v4; // w0
  unsigned int v5; // w0
  __int64 (__fastcall *v6)(); // x1
  unsigned int *v7; // x1
  unsigned int v8; // w0
  __int64 (__fastcall *v9)(); // x2
  __int64 v10; // [xsp+40h] [xbp+40h]
  v2 = *((unsigned int **)this + 6);
  *(_QWORD *)this = off_42A340;
  if ( v2 )
  {
    if ( &_pthread_key_create )
    {
      v3 = v2 + 2;
      do
        v4 = __ldaxr(v3);
      while ( __stlxr(v4 - 1, v3) );
      if ( v4 != 1 )
        goto LABEL_6;
    }
    else
    {
      v5 = v2[2];
      v2[2] = v5 - 1;
      if ( v5 != 1 )
        goto LABEL_6;
    }
    v6 = *(__int64 (__fastcall **)())(*(_QWORD *)v2 + 16LL);
    if ( v6 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
      ((void (__fastcall *)(unsigned int *))v6)(v2);
    if ( &_pthread_key_create )
    {
      v7 = v2 + 3;
      do
        v8 = __ldaxr(v7);
      while ( __stlxr(v8 - 1, v7) );
    }
    else
    {
      v8 = v2[3];
      v2[3] = v8 - 1;
    }
    if ( v8 == 1 )
    {
      v9 = *(__int64 (__fastcall **)())(*(_QWORD *)v2 + 24LL);
      if ( v9 == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
        (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v2 + 8LL))(v2);
      else
        ((void (__fastcall *)(unsigned int *))v9)(v2);
    }
  }
LABEL_6:
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x000736d0  size 360 bytes  90 insns
 *   apac::drc::DynRangeCompressor::~DynRangeCompressor()
 *   apac::drc::DynRangeCompressor::~DynRangeCompressor()
 *   [_ZN4apac3drc18DynRangeCompressorD1Ev]
 *   [_ZN4apac3drc18DynRangeCompressorD2Ev]
 *   data refs: 0x733c0, 0x73400, 0x42a330, 0x42cc70, 0x42cd68, 0x42ce00, 0x42d2e0, 0x42d618, 0x43e520, 0x43ec98
 */
// Alternative name is '_ZN4apac3drc18DynRangeCompressorD2Ev'
void __fastcall apac::drc::DynRangeCompressor::~DynRangeCompressor(apac__drc__DynRangeCompressor *this)
{
  __int64 v1; // x21
  unsigned int *v2; // x20
  unsigned int *v3; // x1
  unsigned int v4; // w0
  unsigned int v5; // w0
  __int64 (__fastcall *v6)(); // x1
  unsigned int *v7; // x1
  unsigned int v8; // w0
  __int64 v9; // [xsp+20h] [xbp+20h]
  __int64 v10; // [xsp+38h] [xbp+38h]
  __int64 v11; // [xsp+40h] [xbp+40h]
  v2 = *((unsigned int **)this + 6);
  *(_QWORD *)this = off_42A340;
  if ( !v2 )
    goto LABEL_7;
  if ( &_pthread_key_create )
  {
    v3 = v2 + 2;
    do
      v4 = __ldaxr(v3);
    while ( __stlxr(v4 - 1, v3) );
    if ( v4 != 1 )
      goto LABEL_6;
  }
  else
  {
    v5 = v2[2];
    v2[2] = v5 - 1;
    if ( v5 != 1 )
    {
LABEL_6:
      v1 = v9;
LABEL_7:
      if ( v10 != _stack_chk_guard )
        v9 = v1;
      return;
    }
  }
  v6 = *(__int64 (__fastcall **)())(*(_QWORD *)v2 + 16LL);
  if ( v6 != std::_Sp_counted_ptr<decltype(nullptr),(__gnu_cxx::_Lock_policy)2>::_M_dispose )
    ((void (__fastcall *)(unsigned int *))v6)(v2);
  if ( &_pthread_key_create )
  {
    v7 = v2 + 3;
    do
      v8 = __ldaxr(v7);
    while ( __stlxr(v8 - 1, v7) );
  }
  else
  {
    v8 = v2[3];
    v2[3] = v8 - 1;
  }
  if ( v8 != 1 )
    goto LABEL_6;
  if ( *(__int64 (__fastcall **)())(*(_QWORD *)v2 + 24LL) == std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_destroy )
    (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v2 + 8LL))(v2);
  else
    (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v2 + 24LL))(v2);
}


/* ------------------------------------------------------------------------ */
/* 0x0007ea20  size 160 bytes  40 insns
 *   apac::drc::DynRangeCompressor::APACDRCExtensionPayload::Deserialize(TBitstreamReader<unsigned int>&, mpddrc::HeaderPayloadType)
 *   [_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayload11DeserializeER16TBitstreamReaderIjEN6mpddrc17HeaderPayloadTypeE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::drc::DynRangeCompressor::APACDRCExtensionPayload::Deserialize(
        apac__drc__DynRangeCompressor__APACDRCExtensionPayload *this,
        TBitstreamReader_u32 *bs,
        int n)
{
  mpddrc::UniDrc *v4; // x21
  __int64 v6; // [xsp+40h] [xbp+40h]
  v4 = (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 8);
  if ( (unsigned int)mpddrc::UniDrc::Deserialize((int)this + 8, (int)bs) )
  {
    *(_BYTE *)(*(_QWORD *)this + 8LL) = 0;
    mpddrc::UniDrc::Reset(v4);
  }
  else if ( !*(_BYTE *)(*(_QWORD *)this + 8LL) && *((_BYTE *)this + 744) )
  {
    *(_BYTE *)(*(_QWORD *)this + 8LL) = 1;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007eac0  size 500 bytes  125 insns
 *   apac::drc::DynRangeCompressor::AdjustForHostInput()
 *   [_ZN4apac3drc18DynRangeCompressor18AdjustForHostInputEv]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall apac::drc::DynRangeCompressor::AdjustForHostInput(apac__drc__DynRangeCompressor *this)
{
  __int64 v2; // x0
  __int64 v4; // x0
  __int64 v5; // [xsp+30h] [xbp+30h]
  if ( !*((_QWORD *)this + 5) )
    return 0;
  v2 = *((_QWORD *)this + 2);
  if ( !*(_BYTE *)(v2 + 12) )
  {
    if ( !*(_BYTE *)(v2 + 33) )
      goto LABEL_4;
    goto LABEL_13;
  }
  DRCConfiguration::SetTargetLevelChanged((DRCConfiguration *)v2, 0);
  v4 = *((_QWORD *)this + 2);
  if ( *(_BYTE *)(v4 + 13) )
  {
    if ( (unsigned int)mpddrc::UniDrc::SetRequestTargetLoudness(
                         (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                         *(float *)(v4 + 16)) )
      return 1;
  }
  else if ( (unsigned int)mpddrc::UniDrc::SetRequestLoudnessNormalizationOff((mpddrc::UniDrc *)(*((_QWORD *)this + 5)
                                                                                              + 8LL)) )
  {
    return 1;
  }
  v2 = *((_QWORD *)this + 2);
  if ( *(_BYTE *)(v2 + 33) )
  {
LABEL_13:
    DRCConfiguration::SetUniDRCCompressionProfileChanged((DRCConfiguration *)v2, 0);
    v2 = *((_QWORD *)this + 2);
    if ( *(_BYTE *)(v2 + 34) )
    {
      if ( (unsigned int)mpddrc::UniDrc::SetRequestFromProfile(
                           (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                           *(_DWORD *)(v2 + 36)) )
        return 1;
      v2 = *((_QWORD *)this + 2);
    }
  }
LABEL_4:
  if ( *(_BYTE *)(v2 + 32) )
  {
    DRCConfiguration::SetCutBoostChanged((DRCConfiguration *)v2, 0);
    if ( (unsigned int)mpddrc::UniDrc::SetRequestCompressBoost(
                         (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                         *(float *)(*((_QWORD *)this + 2) + 24LL),
                         *(float *)(*((_QWORD *)this + 2) + 28LL)) )
      return 1;
    v2 = *((_QWORD *)this + 2);
    if ( !*(_BYTE *)(v2 + 40) )
    {
LABEL_6:
      if ( !*(_BYTE *)(v2 + 48) )
        goto LABEL_7;
LABEL_20:
      DRCConfiguration::SetPlaybackGainChanged((DRCConfiguration *)v2, 0);
      if ( (unsigned int)mpddrc::UniDrc::SetRequestLoudnessEqPlaybackGain(
                           (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                           *(float *)(*((_QWORD *)this + 2) + 52LL)) )
        return 1;
      v2 = *((_QWORD *)this + 2);
      if ( !*(_BYTE *)(v2 + 56) )
        goto LABEL_8;
      goto LABEL_22;
    }
  }
  else if ( !*(_BYTE *)(v2 + 40) )
  {
    goto LABEL_6;
  }
  DRCConfiguration::SetSensitivityChanged((DRCConfiguration *)v2, 0);
  if ( (unsigned int)mpddrc::UniDrc::SetRequestLoudnessEqSensitivity(
                       (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                       *(float *)(*((_QWORD *)this + 2) + 44LL)) )
    return 1;
  v2 = *((_QWORD *)this + 2);
  if ( *(_BYTE *)(v2 + 48) )
    goto LABEL_20;
LABEL_7:
  if ( *(_BYTE *)(v2 + 56) )
  {
LABEL_22:
    DRCConfiguration::SetLoudnessEqStrengthChanged((DRCConfiguration *)v2, 0);
    if ( !(unsigned int)mpddrc::UniDrc::SetRequestLoudnessEqStrength(
                          (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                          *(_DWORD *)(*((_QWORD *)this + 2) + 60LL)) )
    {
      v2 = *((_QWORD *)this + 2);
      goto LABEL_8;
    }
    return 1;
  }
LABEL_8:
  if ( !*(_BYTE *)(v2 + 64) )
    return 0;
  DRCConfiguration::SetAudioScenePresetParamsChanged((DRCConfiguration *)v2, 0);
  return (unsigned int)mpddrc::UniDrc::SetAudioScenePresetParams(
                         *((_QWORD *)this + 5) + 8LL,
                         *(unsigned __int8 *)(*((_QWORD *)this + 2) + 68LL),
                         *((_QWORD *)this + 2) + 72LL,
                         *((_QWORD *)this + 2) + 112LL,
                         *((_QWORD *)this + 2) + 136LL) != 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007ecc0  size 100 bytes  25 insns
 *   apac::drc::DynRangeCompressor::Deserialize(TBitstreamReader<unsigned int>&, mpddrc::HeaderPayloadType)
 *   [_ZN4apac3drc18DynRangeCompressor11DeserializeER16TBitstreamReaderIjEN6mpddrc17HeaderPayloadTypeE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::drc::DynRangeCompressor::Deserialize(
        apac__drc__DynRangeCompressor *this,
        TBitstreamReader_u32 *bs,
        int n)
{
  apac__drc__DynRangeCompressor__APACDRCExtensionPayload *v3; // x0
  __int64 v5; // [xsp+20h] [xbp+20h]
  v3 = *((apac__drc__DynRangeCompressor__APACDRCExtensionPayload **)this + 5);
  if ( v3 )
    return apac::drc::DynRangeCompressor::APACDRCExtensionPayload::Deserialize(v3, bs, n);
  else
    return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x0007ed30  size 240 bytes  60 insns
 *   apac::drc::DynRangeCompressor::ProcessUniDrc(float const**, float**, unsigned int&)
 *   [_ZN4apac3drc18DynRangeCompressor13ProcessUniDrcEPPKfPPfRj]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall apac::drc::DynRangeCompressor::ProcessUniDrc(
        apac__drc__DynRangeCompressor *this,
        const float **a,
        float **buf,
        unsigned int *a2)
{
  __int64 v4; // x23
  __int64 v5; // x24
  _BOOL4 v6; // w21
  __int64 v12; // x0
  __int64 v13; // [xsp+30h] [xbp+30h]
  __int64 v14; // [xsp+38h] [xbp+38h]
  __int64 v15; // [xsp+48h] [xbp+48h]
  __int64 v16; // [xsp+50h] [xbp+50h]
  LOBYTE(v6) = 0;
  if ( *((_QWORD *)this + 5) )
  {
    v6 = apac::drc::DynRangeCompressor::AdjustForHostInput(this);
    if ( v6 )
    {
      v4 = v13;
      v5 = v14;
    }
    else
    {
      if ( (unsigned int)mpddrc::UniDrc::Process(
                           (mpddrc::UniDrc *)(*((_QWORD *)this + 5) + 8LL),
                           a,
                           *((_DWORD *)this + 6),
                           buf,
                           *((_DWORD *)this + 6),
                           a2) )
      {
        v12 = *((_QWORD *)this + 5);
        *((_BYTE *)this + 8) = 0;
        mpddrc::UniDrc::Reset((mpddrc::UniDrc *)(v12 + 8));
        *((_WORD *)this + 5) = 0;
      }
      else
      {
        DRCConfiguration::SetUniDrcDelay(*((DRCConfiguration **)this + 2), *(_DWORD *)(*((_QWORD *)this + 5) + 2464LL));
        DRCConfiguration::SetUniDrcDelayChanged(*((DRCConfiguration **)this + 2), 1u);
      }
      v4 = v13;
      v5 = v14;
    }
  }
  if ( v15 != _stack_chk_guard )
  {
    v13 = v4;
    v14 = v5;
  }
  return v6;
}


/* ------------------------------------------------------------------------ */
/* 0x0007ee20  size 68 bytes  17 insns
 *   apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload()
 *   apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload()
 *   [_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadD2Ev]
 *   [_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadD1Ev'
void __fastcall apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload(
        apac__drc__DynRangeCompressor__APACDRCExtensionPayload *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  mpddrc::UniDrc::~UniDrc((apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 8));
}


/* ------------------------------------------------------------------------ */
/* 0x0007ee70  size 3796 bytes  942 insns
 *   apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(apac::drc::DynRangeCompressor&, DRCConfiguration const&, unsigned int, unsigned int, unsigned int, unsigned int, mpddrc::UniDrcDomain)
 *   apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(apac::drc::DynRangeCompressor&, DRCConfiguration const&, unsigned int, unsigned int, unsigned int, unsigned int, mpddrc::UniDrcDomain)
 *   [_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadC2ERS1_RK16DRCConfigurationjjjjN6mpddrc12UniDrcDomainE]
 *   [_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadC1ERS1_RK16DRCConfigurationjjjjN6mpddrc12UniDrcDomainE]
 *   data refs: 0x42ab70, 0x42c9f8, 0x42ce00, 0x42dc90, 0x43e3d0, 0x43e520
 */
// Alternative name is '_ZN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadC1ERS1_RK16DRCConfigurationjjjjN6mpddrc12UniDrcDomainE'
__int64 __fastcall apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(
        apac__drc__DynRangeCompressor__APACDRCExtensionPayload *this,
        apac__drc__DynRangeCompressor *a,
        const DRCConfiguration *drcCfg,
        unsigned int n,
        unsigned int n2,
        unsigned int n3,
        unsigned int n4,
        int n5)
{
  mpddrc::UniDrc *v14; // x26
  int v15; // w0
  __int64 v16; // x20
  _BYTE *v17; // x23
  __int64 v18; // x0
  __int64 v19; // x24
  signed __int64 v20; // x24
  __int64 v21; // x0
  __int64 v22; // x3
  __int64 v23; // x5
  __int64 v24; // x2
  __int64 v25; // x1
  unsigned __int64 v26; // x24
  char v27; // w25
  __int64 v28; // x2
  char v29; // w2
  __int16 v30; // w2
  __int64 v31; // x20
  _BYTE *v32; // x23
  __int64 v33; // x0
  __int64 v34; // x24
  signed __int64 v35; // x24
  __int64 v36; // x0
  __int64 v37; // x3
  __int64 v38; // x5
  __int64 v39; // x2
  __int64 v40; // x1
  unsigned __int64 v41; // x24
  char *v42; // x23
  char v43; // w2
  __int64 v44; // x2
  char v45; // w2
  __int16 v46; // w2
  __int64 v47; // x0
  __int64 v48; // x1
  __int64 v49; // x23
  __int64 v50; // x25
  __int64 v51; // x23
  __int64 i; // x25
  __int64 v53; // x23
  __int64 j; // x25
  const unsigned __int8 *v55; // x1
  const unsigned __int8 *v56; // x2
  const unsigned __int8 *v57; // x1
  const unsigned __int8 *v58; // x2
  float v59; // s0
  int v60; // w1
  __int16 v61; // w2
  int v62; // w3
  char v63; // w3
  char v64; // w4
  __int64 v65; // x3
  __int64 v66; // x2
  char v67; // w3
  int v68; // w2
  char v69; // w3
  __int64 v70; // x2
  int v71; // w3
  __int16 v72; // w2
  __int16 v73; // w2
  int v74; // w2
  char v75; // w4
  char v76; // w3
  char v77; // w4
  int v78; // w5
  int v79; // w3
  __int16 v80; // w3
  char v81; // w2
  char v82; // w2
  const unsigned __int8 *end; // x23
  __int64 v85; // x0
  __int64 v86; // x24
  signed __int64 v87; // x24
  __int64 v88; // x0
  __int64 v89; // x3
  __int64 v90; // x5
  __int64 v91; // x2
  __int64 v92; // x1
  const unsigned __int8 *v93; // x23
  __int64 v94; // x0
  __int64 v95; // x24
  signed __int64 v96; // x24
  __int64 v97; // x0
  __int64 v98; // x3
  __int64 v99; // x5
  __int64 v100; // x2
  __int64 v101; // x1
  _DWORD *v102; // x0
  _DWORD *v103; // x0
  _DWORD *exception; // x0
  _DWORD *v105; // x0
  _DWORD *v106; // x0
  _DWORD *v107; // x0
  __int64 v108; // [xsp+0h] [xbp-20h]
  char *v109; // [xsp+8h] [xbp-18h]
  char *v110; // [xsp+10h] [xbp-10h]
  void (__fastcall **v111)(MP4LoudnessMeasure *__hidden); // [xsp+80h] [xbp+60h]
  __int64 v112; // [xsp+88h] [xbp+68h]
  __int64 *v113; // [xsp+90h] [xbp+70h]
  unsigned int v114; // [xsp+9Ch] [xbp+7Ch]
  __int64 v115; // [xsp+A0h] [xbp+80h] BYREF
  _BYTE *v116; // [xsp+A8h] [xbp+88h]
  _BYTE *v117; // [xsp+B0h] [xbp+90h]
  TBitstreamReader_u32 v118; // [xsp+B8h] [xbp+98h] BYREF
  __int64 v119; // [xsp+D0h] [xbp+B0h] BYREF
  int v120; // [xsp+D8h] [xbp+B8h]
  __int64 v121; // [xsp+E0h] [xbp+C0h]
  int v122; // [xsp+E8h] [xbp+C8h]
  __int64 v123; // [xsp+F0h] [xbp+D0h]
  int v124; // [xsp+F8h] [xbp+D8h] BYREF
  __int64 v125; // [xsp+FCh] [xbp+DCh]
  __int16 v126; // [xsp+104h] [xbp+E4h]
  int v127; // [xsp+108h] [xbp+E8h] BYREF
  char v128; // [xsp+10Ch] [xbp+ECh]
  __int64 v129; // [xsp+110h] [xbp+F0h]
  int v130; // [xsp+118h] [xbp+F8h]
  _BYTE v131[24]; // [xsp+120h] [xbp+100h] BYREF
  int v132; // [xsp+138h] [xbp+118h]
  _QWORD v133[3]; // [xsp+140h] [xbp+120h] BYREF
  _BYTE v134[24]; // [xsp+158h] [xbp+138h] BYREF
  char v135; // [xsp+170h] [xbp+150h]
  char v136; // [xsp+178h] [xbp+158h]
  __int64 v137; // [xsp+17Ch] [xbp+15Ch] BYREF
  char v138; // [xsp+184h] [xbp+164h]
  __int64 v139; // [xsp+188h] [xbp+168h]
  int v140; // [xsp+190h] [xbp+170h]
  char v141; // [xsp+194h] [xbp+174h]
  _QWORD v142[2]; // [xsp+198h] [xbp+178h] BYREF
  __int64 v143; // [xsp+1A8h] [xbp+188h] BYREF
  char v144; // [xsp+1B0h] [xbp+190h]
  __int16 v145; // [xsp+1B2h] [xbp+192h]
  __int64 v146; // [xsp+1B8h] [xbp+198h] BYREF
  __int64 v147; // [xsp+1C0h] [xbp+1A0h] BYREF
  __int64 v148; // [xsp+1D0h] [xbp+1B0h] BYREF
  _QWORD v149[4]; // [xsp+1D8h] [xbp+1B8h] BYREF
  char v150; // [xsp+1F8h] [xbp+1D8h]
  int v151; // [xsp+1FCh] [xbp+1DCh]
  __int64 v152; // [xsp+200h] [xbp+1E0h] BYREF
  __int64 v153; // [xsp+218h] [xbp+1F8h] BYREF
  _BYTE v154[28]; // [xsp+230h] [xbp+210h] BYREF
  int v155; // [xsp+24Ch] [xbp+22Ch]
  __int64 v156; // [xsp+250h] [xbp+230h]
  _DWORD v157[8]; // [xsp+258h] [xbp+238h] BYREF
  _DWORD v158[7]; // [xsp+278h] [xbp+258h] BYREF
  char v159; // [xsp+294h] [xbp+274h]
  __int16 v160; // [xsp+298h] [xbp+278h]
  _BYTE v161[32]; // [xsp+2A0h] [xbp+280h] BYREF
  __int16 v162; // [xsp+2C0h] [xbp+2A0h]
  __int64 v163; // [xsp+2C8h] [xbp+2A8h] BYREF
  _WORD v164[14]; // [xsp+2E0h] [xbp+2C0h] BYREF
  int v165; // [xsp+2FCh] [xbp+2DCh]
  __int64 v166; // [xsp+300h] [xbp+2E0h] BYREF
  _BYTE v167[44]; // [xsp+318h] [xbp+2F8h] BYREF
  int v168; // [xsp+344h] [xbp+324h]
  __int64 v169; // [xsp+348h] [xbp+328h]
  __int64 v170; // [xsp+350h] [xbp+330h]
  char v171; // [xsp+358h] [xbp+338h]
  int v172; // [xsp+35Ch] [xbp+33Ch]
  char v173; // [xsp+360h] [xbp+340h]
  int v174; // [xsp+364h] [xbp+344h]
  __int64 v175; // [xsp+368h] [xbp+348h] BYREF
  _BYTE v176[26]; // [xsp+380h] [xbp+360h] BYREF
  __int16 v177; // [xsp+39Ah] [xbp+37Ah]
  char v178; // [xsp+3A0h] [xbp+380h]
  _DWORD v179[7]; // [xsp+3A8h] [xbp+388h] BYREF
  char v180; // [xsp+3C4h] [xbp+3A4h]
  __int64 v181; // [xsp+3C8h] [xbp+3A8h]
  __int64 v182; // [xsp+3D0h] [xbp+3B0h]
  *(_QWORD *)this = a;
  v14 = (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 8);
  v114 = n;
  mpddrc::UniDrc::UniDrc((apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 8));
  v119 = 0;
  LODWORD(v108) = 1;
  v109 = (char *)drcCfg + 192;
  v110 = (char *)drcCfg + 216;
  v113 = &v119;
  v120 = 0;
  v121 = 0;
  v122 = 0;
  v123 = 0;
  mpddrc::UniDrc::Initialize(v14, n, n2, n3, n4, 0);
  mpddrc::UniDrc::SetRequestLoudnessParameterProfile(v14, 1u);
  if ( *((_BYTE *)drcCfg + 13) )
    mpddrc::UniDrc::SetRequestTargetLoudness(v14, *((float *)drcCfg + 4));
  if ( *((_BYTE *)drcCfg + 34) )
    mpddrc::UniDrc::SetRequestFromProfile(v14, *((_DWORD *)drcCfg + 9));
  if ( n3 != n4 )
    mpddrc::UniDrc::SetRequestTargetChannelCount(v14, n4);
  v15 = *((_DWORD *)drcCfg + 2);
  if ( v15 )
  {
    if ( v15 == 1 )
    {
      mpddrc::UniDrc::SetRequestRangeNoOverride(v14, 8.0);
    }
    else if ( v15 == 2 )
    {
      mpddrc::UniDrc::SetRequestRangeNoOverride(v14, 5.0);
    }
  }
  if ( *((_BYTE *)drcCfg + 160) )
    mpddrc::UniDrc::SetRequestRangeWithOverride(v14, *((float *)drcCfg + 41));
  mpddrc::UniDrc::SetRequestCompressBoost(v14, *((float *)drcCfg + 6), *((float *)drcCfg + 7));
  if ( *((_BYTE *)drcCfg + 57) )
    mpddrc::UniDrc::SetRequestLoudnessEqStrength(v14, *((_DWORD *)drcCfg + 15));
  if ( *((_BYTE *)drcCfg + 41) )
    mpddrc::UniDrc::SetRequestLoudnessEqSensitivity(v14, *((float *)drcCfg + 11));
  if ( !*((_BYTE *)drcCfg + 49) )
  {
    if ( !*((_BYTE *)drcCfg + 65) )
      goto LABEL_19;
LABEL_113:
    mpddrc::UniDrc::SetAudioScenePresetParams(
      v14,
      *((unsigned __int8 *)drcCfg + 68),
      (char *)drcCfg + 72,
      (char *)drcCfg + 112,
      (char *)drcCfg + 136);
    if ( !*((_BYTE *)drcCfg + 168) )
      goto LABEL_20;
    goto LABEL_114;
  }
  mpddrc::UniDrc::SetRequestLoudnessEqPlaybackGain(v14, *((float *)drcCfg + 13));
  if ( *((_BYTE *)drcCfg + 65) )
    goto LABEL_113;
LABEL_19:
  if ( !*((_BYTE *)drcCfg + 168) )
    goto LABEL_20;
LABEL_114:
  mpddrc::UniDrc::SetAlbumMode(v14, *((_BYTE *)drcCfg + 169) != 0);
LABEL_20:
  v16 = *((_QWORD *)drcCfg + 24);
  v112 = *((_QWORD *)drcCfg + 25);
  if ( v112 != v16 )
  {
    v115 = 0;
    v116 = nullptr;
    v117 = nullptr;
    memset(&v118, 0, sizeof(v118));
    v111 = off_42AB80;
    do
    {
      if ( *(_BYTE *)v16 )
      {
        end = v118.end;
        if ( v118.end == *(const unsigned __int8 **)&v118.cache )
        {
          std::vector<MP4LoudnessInfo>::_M_realloc_insert<MP4LoudnessInfo const&>(&v118, v118.end, v16);
        }
        else
        {
          *v118.end = *(_BYTE *)v16;
          *((_BYTE *)end + 1) = *(_BYTE *)(v16 + 1);
          *((_BYTE *)end + 2) = *(_BYTE *)(v16 + 2);
          *((_BYTE *)end + 3) = *(_BYTE *)(v16 + 3);
          *((_WORD *)end + 2) = *(_WORD *)(v16 + 4);
          *((_WORD *)end + 3) = *(_WORD *)(v16 + 6);
          *((_DWORD *)end + 2) = *(_DWORD *)(v16 + 8);
          *((_DWORD *)end + 3) = *(_DWORD *)(v16 + 12);
          v85 = *(_QWORD *)(v16 + 16);
          v86 = *(_QWORD *)(v16 + 24);
          *((_QWORD *)end + 2) = 0;
          *((_QWORD *)end + 3) = 0;
          *((_QWORD *)end + 4) = 0;
          v87 = v86 - v85;
          if ( v87 >> 4 )
          {
            if ( (unsigned __int64)(v87 >> 4) > 0x7FFFFFFFFFFFFFFLL )
              std::__throw_bad_alloc();
            v88 = operator new(v87);
          }
          else
          {
            v88 = 0;
          }
          *((_QWORD *)end + 2) = v88;
          *((_QWORD *)end + 3) = v88;
          *((_QWORD *)end + 4) = v88 + v87;
          v90 = *(_QWORD *)(v16 + 16);
          v89 = *(_QWORD *)(v16 + 24);
          if ( v90 != v89 )
          {
            v91 = *(_QWORD *)(v16 + 16);
            v92 = v88;
            do
            {
              *(_QWORD *)v92 = off_42AB80;
              if ( v92 != v91 )
                *(_DWORD *)(v92 + 8) = *(_DWORD *)(v91 + 8);
              v91 += 16;
              v92 += 16;
            }
            while ( v89 != v91 );
            v88 += v89 - v90;
          }
          *((_QWORD *)end + 3) = v88;
          v118.end += 40;
        }
      }
      else
      {
        v17 = v116;
        if ( v116 == v117 )
        {
          std::vector<MP4LoudnessInfo>::_M_realloc_insert<MP4LoudnessInfo const&>(&v115, v116, v16);
        }
        else
        {
          *v116 = 0;
          v17[1] = *(_BYTE *)(v16 + 1);
          v17[2] = *(_BYTE *)(v16 + 2);
          v17[3] = *(_BYTE *)(v16 + 3);
          *((_WORD *)v17 + 2) = *(_WORD *)(v16 + 4);
          *((_WORD *)v17 + 3) = *(_WORD *)(v16 + 6);
          *((_DWORD *)v17 + 2) = *(_DWORD *)(v16 + 8);
          *((_DWORD *)v17 + 3) = *(_DWORD *)(v16 + 12);
          v18 = *(_QWORD *)(v16 + 16);
          v19 = *(_QWORD *)(v16 + 24);
          *((_QWORD *)v17 + 2) = 0;
          *((_QWORD *)v17 + 3) = 0;
          *((_QWORD *)v17 + 4) = 0;
          v20 = v19 - v18;
          if ( v20 >> 4 )
          {
            if ( (unsigned __int64)(v20 >> 4) > 0x7FFFFFFFFFFFFFFLL )
              std::__throw_bad_alloc();
            v21 = operator new(v20);
          }
          else
          {
            v21 = 0;
          }
          *((_QWORD *)v17 + 2) = v21;
          *((_QWORD *)v17 + 3) = v21;
          *((_QWORD *)v17 + 4) = v21 + v20;
          v23 = *(_QWORD *)(v16 + 16);
          v22 = *(_QWORD *)(v16 + 24);
          if ( v23 != v22 )
          {
            v24 = *(_QWORD *)(v16 + 16);
            v25 = v21;
            do
            {
              *(_QWORD *)v25 = v111;
              if ( v24 != v25 )
                *(_DWORD *)(v25 + 8) = *(_DWORD *)(v24 + 8);
              v24 += 16;
              v25 += 16;
            }
            while ( v22 != v24 );
            v21 += v22 - v23;
          }
          *((_QWORD *)v17 + 3) = v21;
          v116 += 40;
        }
      }
      v16 += 40;
    }
    while ( v112 != v16 );
    v26 = 0xCCCCCCCCCCCCCCCDLL * ((__int64)&v116[-v115] >> 3);
    if ( v118.end == v118.cur )
    {
      v27 = 0;
    }
    else
    {
      std::vector<MP4LoudnessInfo>::_M_range_insert<__gnu_cxx::__normal_iterator<MP4LoudnessInfo*,std::vector<MP4LoudnessInfo>>>(&v115);
      v27 = 1;
    }
    mpddrc::LoudnessInfoSet::LoudnessInfoSet(
      (mpddrc::LoudnessInfoSet *)&v124,
      (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 1472));
    LOBYTE(v124) = 8;
    HIDWORD(v125) = v26;
    LOBYTE(v132) = v27;
    std::vector<MP4LoudnessInfo>::operator=(v131, &v115);
    v28 = v125;
    *((_BYTE *)this + 1472) = v124;
    *(_QWORD *)((char *)this + 1476) = v28;
    std::vector<MP4LoudnessInfo>::operator=((char *)this + 1488, &v127);
    std::vector<MP4LoudnessInfo>::operator=((char *)this + 1512, v131);
    *((_DWORD *)this + 384) = v132;
    std::vector<mpddrc::LoudnessInfoV8>::operator=((char *)this + 1544, v133);
    std::vector<mpddrc::LoudnessInfoV8>::operator=((char *)this + 1568, v134);
    v29 = v136;
    *((_BYTE *)this + 1592) = v135;
    *((_BYTE *)this + 1600) = v29;
    std::vector<mpddrc::LoudnessInfoSource>::operator=((char *)this + 1608, (char *)&v137 + 4);
    std::vector<unsigned short>::operator=((char *)this + 1632, v142);
    v30 = v145;
    *((_BYTE *)this + 1656) = v144;
    *((_WORD *)this + 829) = v30;
    std::vector<float>::operator=((char *)this + 1664, &v146);
    std::vector<float>::operator=((char *)this + 1688, &v148);
    *((_BYTE *)this + 1712) = 1;
    *((_BYTE *)this + 2449) = 1;
    mpddrc::LoudnessInfoSet::~LoudnessInfoSet((mpddrc::LoudnessInfoSet *)&v124);
    std::vector<MP4LoudnessInfo>::~vector(&v118);
    std::vector<MP4LoudnessInfo>::~vector(&v115);
  }
  v31 = *((_QWORD *)drcCfg + 27);
  v112 = *((_QWORD *)drcCfg + 28);
  if ( v112 != v31 )
  {
    v115 = 0;
    v116 = nullptr;
    v117 = nullptr;
    memset(&v118, 0, sizeof(v118));
    v111 = off_42AB80;
    do
    {
      if ( *(_BYTE *)v31 )
      {
        v93 = v118.end;
        if ( v118.end == *(const unsigned __int8 **)&v118.cache )
        {
          std::vector<MP4LoudnessInfo>::_M_realloc_insert<MP4LoudnessInfo const&>(&v118, v118.end, v31);
        }
        else
        {
          *v118.end = *(_BYTE *)v31;
          *((_BYTE *)v93 + 1) = *(_BYTE *)(v31 + 1);
          *((_BYTE *)v93 + 2) = *(_BYTE *)(v31 + 2);
          *((_BYTE *)v93 + 3) = *(_BYTE *)(v31 + 3);
          *((_WORD *)v93 + 2) = *(_WORD *)(v31 + 4);
          *((_WORD *)v93 + 3) = *(_WORD *)(v31 + 6);
          *((_DWORD *)v93 + 2) = *(_DWORD *)(v31 + 8);
          *((_DWORD *)v93 + 3) = *(_DWORD *)(v31 + 12);
          v94 = *(_QWORD *)(v31 + 16);
          v95 = *(_QWORD *)(v31 + 24);
          *((_QWORD *)v93 + 2) = 0;
          *((_QWORD *)v93 + 3) = 0;
          *((_QWORD *)v93 + 4) = 0;
          v96 = v95 - v94;
          if ( v96 >> 4 )
          {
            if ( (unsigned __int64)(v96 >> 4) > 0x7FFFFFFFFFFFFFFLL )
              std::__throw_bad_alloc();
            v97 = operator new(v96);
          }
          else
          {
            v97 = 0;
          }
          *((_QWORD *)v93 + 2) = v97;
          *((_QWORD *)v93 + 3) = v97;
          *((_QWORD *)v93 + 4) = v97 + v96;
          v99 = *(_QWORD *)(v31 + 16);
          v98 = *(_QWORD *)(v31 + 24);
          if ( v99 != v98 )
          {
            v100 = *(_QWORD *)(v31 + 16);
            v101 = v97;
            do
            {
              *(_QWORD *)v101 = off_42AB80;
              if ( v100 != v101 )
                *(_DWORD *)(v101 + 8) = *(_DWORD *)(v100 + 8);
              v100 += 16;
              v101 += 16;
            }
            while ( v98 != v100 );
            v97 += v98 - v99;
          }
          *((_QWORD *)v93 + 3) = v97;
          v118.end += 40;
        }
      }
      else
      {
        v32 = v116;
        if ( v116 == v117 )
        {
          std::vector<MP4LoudnessInfo>::_M_realloc_insert<MP4LoudnessInfo const&>(&v115, v116, v31);
        }
        else
        {
          *v116 = 0;
          v32[1] = *(_BYTE *)(v31 + 1);
          v32[2] = *(_BYTE *)(v31 + 2);
          v32[3] = *(_BYTE *)(v31 + 3);
          *((_WORD *)v32 + 2) = *(_WORD *)(v31 + 4);
          *((_WORD *)v32 + 3) = *(_WORD *)(v31 + 6);
          *((_DWORD *)v32 + 2) = *(_DWORD *)(v31 + 8);
          *((_DWORD *)v32 + 3) = *(_DWORD *)(v31 + 12);
          v33 = *(_QWORD *)(v31 + 16);
          v34 = *(_QWORD *)(v31 + 24);
          *((_QWORD *)v32 + 2) = 0;
          *((_QWORD *)v32 + 3) = 0;
          *((_QWORD *)v32 + 4) = 0;
          v35 = v34 - v33;
          if ( v35 >> 4 )
          {
            if ( (unsigned __int64)(v35 >> 4) > 0x7FFFFFFFFFFFFFFLL )
              std::__throw_bad_alloc();
            v36 = operator new(v35);
          }
          else
          {
            v36 = 0;
          }
          *((_QWORD *)v32 + 2) = v36;
          *((_QWORD *)v32 + 3) = v36;
          *((_QWORD *)v32 + 4) = v36 + v35;
          v38 = *(_QWORD *)(v31 + 16);
          v37 = *(_QWORD *)(v31 + 24);
          if ( v38 != v37 )
          {
            v39 = *(_QWORD *)(v31 + 16);
            v40 = v36;
            do
            {
              *(_QWORD *)v40 = v111;
              if ( v39 != v40 )
                *(_DWORD *)(v40 + 8) = *(_DWORD *)(v39 + 8);
              v39 += 16;
              v40 += 16;
            }
            while ( v37 != v39 );
            v36 += v37 - v38;
          }
          *((_QWORD *)v32 + 3) = v36;
          v116 += 40;
        }
      }
      v31 += 40;
    }
    while ( v112 != v31 );
    v41 = 0xCCCCCCCCCCCCCCCDLL * ((__int64)&v116[-v115] >> 3);
    if ( v118.end == v118.cur )
    {
      v42 = (char *)this + 1472;
      mpddrc::LoudnessInfoSet::LoudnessInfoSet(
        (mpddrc::LoudnessInfoSet *)&v124,
        (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 1472));
      v43 = v132;
    }
    else
    {
      std::vector<MP4LoudnessInfo>::_M_range_insert<__gnu_cxx::__normal_iterator<MP4LoudnessInfo*,std::vector<MP4LoudnessInfo>>>(&v115);
      v42 = (char *)this + 1472;
      mpddrc::LoudnessInfoSet::LoudnessInfoSet(
        (mpddrc::LoudnessInfoSet *)&v124,
        (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 1472));
      v43 = 1;
    }
    LOBYTE(v124) = 8;
    LODWORD(v125) = v41;
    LOBYTE(v132) = v43;
    std::vector<MP4LoudnessInfo>::operator=(&v127, &v115);
    v44 = v125;
    *((_BYTE *)this + 1472) = v124;
    *(_QWORD *)((char *)this + 1476) = v44;
    std::vector<MP4LoudnessInfo>::operator=((char *)this + 1488, &v127);
    std::vector<MP4LoudnessInfo>::operator=((char *)this + 1512, v131);
    *((_DWORD *)this + 384) = v132;
    std::vector<mpddrc::LoudnessInfoV8>::operator=((char *)this + 1544, v133);
    std::vector<mpddrc::LoudnessInfoV8>::operator=((char *)this + 1568, v134);
    v45 = v136;
    v42[120] = v135;
    *((_BYTE *)this + 1600) = v45;
    std::vector<mpddrc::LoudnessInfoSource>::operator=((char *)this + 1608, (char *)&v137 + 4);
    std::vector<unsigned short>::operator=((char *)this + 1632, v142);
    v46 = v145;
    v42[184] = v144;
    *((_WORD *)v42 + 93) = v46;
    std::vector<float>::operator=((char *)this + 1664, &v146);
    std::vector<float>::operator=((char *)this + 1688, &v148);
    *((_BYTE *)this + 1712) = 1;
    *((_BYTE *)this + 2449) = 1;
    mpddrc::LoudnessInfoSet::~LoudnessInfoSet((mpddrc::LoudnessInfoSet *)&v124);
    std::vector<MP4LoudnessInfo>::~vector(&v118);
    std::vector<MP4LoudnessInfo>::~vector(&v115);
  }
  v48 = *((_QWORD *)drcCfg + 36);
  v47 = *((_QWORD *)drcCfg + 37);
  *((_BYTE *)this + 1713) = *((_BYTE *)drcCfg + 191) != 0;
  if ( v48 != v47
    || *((_QWORD *)drcCfg + 24) != *((_QWORD *)drcCfg + 25)
    || *((_QWORD *)drcCfg + 27) != *((_QWORD *)drcCfg + 28)
    || *((_QWORD *)drcCfg + 30) != *((_QWORD *)drcCfg + 31)
    || *((_QWORD *)drcCfg + 42) != *((_QWORD *)drcCfg + 43) )
  {
    mpddrc::UniDrcConfig::UniDrcConfig(
      (mpddrc::UniDrcConfig *)&v124,
      (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)((char *)this + 24));
    v49 = *((_QWORD *)drcCfg + 33);
    v50 = *((_QWORD *)drcCfg + 34);
    for ( LOBYTE(v125) = 8; v50 != v49; v49 += 24 )
    {
      TBitstreamReader<unsigned int>::TBitstreamReader(
        &v118,
        *(const unsigned __int8 **)v49,
        *(_QWORD *)(v49 + 8) - *(_QWORD *)v49);
      if ( (unsigned int)mpddrc::UniDrcConfig::DeserializeDownmixInstructionsBox(&v124, &v118) )
      {
        exception = __cxa_allocate_exception(4u);
        *exception = 560423284;
        __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
      }
    }
    v51 = *((_QWORD *)drcCfg + 39);
    for ( i = *((_QWORD *)drcCfg + 40); i != v51; v51 += 24 )
    {
      TBitstreamReader<unsigned int>::TBitstreamReader(
        &v118,
        *(const unsigned __int8 **)v51,
        *(_QWORD *)(v51 + 8) - *(_QWORD *)v51);
      if ( (unsigned int)mpddrc::UniDrcConfig::DeserializeDrcCoefficientsUniDrcBox(&v124, &v118) )
      {
        v103 = __cxa_allocate_exception(4u);
        *v103 = 560423284;
        __cxa_throw(v103, (struct type_info *)&`typeinfo for'int, nullptr);
      }
    }
    v53 = *((_QWORD *)drcCfg + 36);
    for ( j = *((_QWORD *)drcCfg + 37); j != v53; v53 += 24 )
    {
      TBitstreamReader<unsigned int>::TBitstreamReader(
        &v118,
        *(const unsigned __int8 **)v53,
        *(_QWORD *)(v53 + 8) - *(_QWORD *)v53);
      if ( (unsigned int)mpddrc::UniDrcConfig::DeserializeDrcInstructionsUniDrcBox(&v124, &v118) )
      {
        v106 = __cxa_allocate_exception(4u);
        *v106 = 560423284;
        __cxa_throw(v106, (struct type_info *)&`typeinfo for'int, nullptr);
      }
    }
    v55 = *((const unsigned __int8 **)drcCfg + 30);
    v56 = *((const unsigned __int8 **)drcCfg + 31);
    if ( v55 != v56 )
    {
      TBitstreamReader<unsigned int>::TBitstreamReader(&v118, v55, (_DWORD)v56 - (_DWORD)v55);
      if ( (unsigned int)mpddrc::UniDrcConfig::DeserializeChannelLayoutBox(&v124, &v118) )
      {
        v105 = __cxa_allocate_exception(4u);
        *v105 = 560423284;
        __cxa_throw(v105, (struct type_info *)&`typeinfo for'int, nullptr);
      }
    }
    v57 = *((const unsigned __int8 **)drcCfg + 42);
    v58 = *((const unsigned __int8 **)drcCfg + 43);
    if ( v57 != v58 )
    {
      TBitstreamReader<unsigned int>::TBitstreamReader(&v118, v57, (_DWORD)v58 - (_DWORD)v57);
      if ( (unsigned int)mpddrc::UniDrcConfig::DeserializeUniDrcConfigExtensionBox(&v124, &v118) )
      {
        v107 = __cxa_allocate_exception(4u);
        *v107 = 560423284;
        __cxa_throw(v107, (struct type_info *)&`typeinfo for'int, nullptr);
      }
    }
    v59 = (float)((float)v114 * 0.0005) + 0.5;
    v60 = 1 << -(char)__clz((unsigned int)v59);
    if ( !(unsigned int)v59 )
      v60 = 2;
    v127 = v60;
    if ( (unsigned int)mpddrc::UniDrcConfig::DeriveExtraInfo((mpddrc::UniDrcConfig *)&v124) )
    {
      v102 = __cxa_allocate_exception(4u);
      *v102 = 560423284;
      __cxa_throw(v102, (struct type_info *)&`typeinfo for'int, nullptr);
    }
    v61 = v125;
    *((_DWORD *)this + 6) = v124;
    *((_WORD *)this + 14) = v61;
    *((_DWORD *)this + 8) = HIDWORD(v125);
    v62 = v127;
    *((_WORD *)this + 18) = v126;
    LOBYTE(v61) = v128;
    *((_DWORD *)this + 10) = v62;
    *((_BYTE *)this + 44) = v61;
    *((_QWORD *)this + 6) = v129;
    *((_DWORD *)this + 14) = v130;
    std::vector<bool>::operator=((char *)this + 64, v131);
    *((_QWORD *)this + 13) = v133[1];
    *((_QWORD *)this + 14) = v133[2];
    std::vector<unsigned int>::operator=((char *)this + 120, v134);
    v63 = v136;
    *((_BYTE *)this + 144) = v135;
    *((_BYTE *)this + 152) = v63;
    *((_BYTE *)this + 164) = v138;
    *(_QWORD *)((char *)this + 156) = v137;
    *((_QWORD *)this + 21) = v139;
    v64 = v141;
    v65 = v142[0];
    *((_DWORD *)this + 44) = v140;
    v66 = v142[1];
    *((_BYTE *)this + 180) = v64;
    *((_QWORD *)this + 23) = v65;
    *((_QWORD *)this + 24) = v66;
    std::vector<unsigned int>::operator=((char *)this + 200, &v143);
    std::vector<unsigned int>::operator=((char *)this + 224, &v147);
    std::vector<unsigned int>::operator=((char *)this + 248, v149);
    v67 = v150;
    v68 = v151;
    *((_QWORD *)this + 34) = v149[3];
    *((_BYTE *)this + 280) = v67;
    *((_DWORD *)this + 71) = v68;
    std::vector<mpddrc::DownmixInstructions>::operator=((char *)this + 288, &v152);
    std::vector<mpddrc::DrcCoefficientsBasic>::operator=((char *)this + 312, &v153);
    std::vector<mpddrc::DrcInstructionsBasic>::operator=((char *)this + 336, v154);
    v69 = v154[24];
    *((_DWORD *)this + 91) = v155;
    v70 = v156;
    *((_BYTE *)this + 360) = v69;
    *((_QWORD *)this + 46) = v70;
    std::vector<mpddrc::DrcCoefficientsUniDrc>::operator=((char *)this + 376, v157);
    *((_DWORD *)this + 100) = v157[6];
    std::vector<mpddrc::DrcInstructionsUniDrc>::operator=((char *)this + 408, v158);
    v71 = v158[6];
    *((_BYTE *)this + 436) = v159;
    v72 = v160;
    *((_DWORD *)this + 108) = v71;
    *((_WORD *)this + 220) = v72;
    std::vector<mpddrc::LoudEqInstructions>::operator=((char *)this + 448, v161);
    v73 = v162;
    *((_BYTE *)this + 472) = v161[24];
    *((_WORD *)this + 240) = v73;
    std::vector<mpddrc::EqCoefficients::FilterBlock>::operator=((char *)this + 488, &v163);
    std::vector<mpddrc::EqCoefficients::TdFilterElement>::operator=((char *)this + 512, v164);
    v74 = v165;
    *((_WORD *)this + 268) = v164[12];
    *((_DWORD *)this + 135) = v74;
    std::vector<mpddrc::EqCoefficients::EqSubbandGains>::operator=((char *)this + 544, &v166);
    std::vector<mpddrc::EqInstructions>::operator=((char *)this + 568, v167);
    v75 = v167[32];
    v76 = v167[40];
    *((_BYTE *)this + 592) = v167[24];
    *((_BYTE *)this + 600) = v75;
    *((_BYTE *)this + 608) = v76;
    *((_DWORD *)this + 153) = v168;
    *((_QWORD *)this + 77) = v169;
    *((_QWORD *)this + 78) = v170;
    v77 = v173;
    v78 = v172;
    v79 = v174;
    *((_BYTE *)this + 632) = v171;
    *((_DWORD *)this + 159) = v78;
    *((_BYTE *)this + 640) = v77;
    *((_DWORD *)this + 161) = v79;
    std::vector<mpddrc::ParametricDrcGainSet>::operator=((char *)this + 648, &v175);
    std::vector<mpddrc::ParametricDrcInstructions>::operator=((char *)this + 672, v176);
    v80 = v177;
    v81 = v178;
    *((_BYTE *)this + 696) = v176[24];
    *((_WORD *)this + 349) = v80;
    *((_BYTE *)this + 704) = v81;
    std::vector<mpddrc::OverridePerLocation>::operator=((char *)this + 712, v179);
    v82 = v180;
    *((_DWORD *)this + 184) = v179[6];
    *((_BYTE *)this + 740) = v82;
    *((_BYTE *)this + 18) = 1;
    *((_BYTE *)this + 744) = 1;
    *((_BYTE *)this + 1714) = 1;
    *((_BYTE *)this + 2449) = 1;
    mpddrc::UniDrcConfig::~UniDrcConfig((mpddrc::UniDrcConfig *)&v124);
  }
  std::_Bvector_base<std::allocator<bool>>::_M_deallocate(v113);
  return v181 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x0007fd50  size 604 bytes  151 insns
 *   apac::drc::DynRangeCompressor::Initialize(DRCConfiguration*, float, unsigned int, unsigned int, unsigned int)
 *   [_ZN4apac3drc18DynRangeCompressor10InitializeEP16DRCConfigurationfjjj]
 *   data refs: 0x42abd0, 0x42ce00, 0x42d2e0, 0x42d338, 0x43e520, 0x43ec98
 */
__int64 __fastcall apac::drc::DynRangeCompressor::Initialize(
        apac__drc__DynRangeCompressor *this,
        DRCConfiguration *drcCfg,
        float f,
        unsigned int n,
        unsigned int n2,
        unsigned int n3)
{
  unsigned int *v12; // x22
  unsigned int *v13; // x1
  unsigned int v14; // w0
  _QWORD *v16; // x22
  unsigned int v17; // w5
  unsigned int v18; // w6
  unsigned int *v19; // x20
  unsigned int *v20; // x1
  unsigned int v21; // w0
  unsigned int v22; // w0
  unsigned int *v23; // x1
  unsigned int v24; // w0
  unsigned int *v25; // x1
  unsigned int v26; // w0
  __int64 v27; // [xsp+70h] [xbp+70h]
  DRCConfiguration::LogStatus(drcCfg);
  v12 = *((unsigned int **)this + 6);
  *((_QWORD *)this + 5) = 0;
  *((_QWORD *)this + 6) = 0;
  if ( !v12 )
    goto LABEL_6;
  if ( &_pthread_key_create )
  {
    v13 = v12 + 2;
    do
      v14 = __ldaxr(v13);
    while ( __stlxr(v14 - 1, v13) );
    if ( v14 != 1 )
      goto LABEL_6;
  }
  else
  {
    v22 = v12[2];
    v12[2] = v22 - 1;
    if ( v22 != 1 )
      goto LABEL_6;
  }
  (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v12 + 16LL))(v12);
  if ( &_pthread_key_create )
  {
    v23 = v12 + 3;
    do
      v24 = __ldaxr(v23);
    while ( __stlxr(v24 - 1, v23) );
  }
  else
  {
    v24 = v12[3];
    v12[3] = v24 - 1;
  }
  if ( v24 == 1 )
    (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v12 + 24LL))(v12);
LABEL_6:
  *((_BYTE *)this + 8) = 0;
  *((_QWORD *)this + 2) = drcCfg;
  *((_DWORD *)this + 6) = n2;
  *((_DWORD *)this + 7) = n3;
  if ( n != 1024 )
    return 1;
  v16 = (_QWORD *)operator new(0x19F8u);
  v17 = *((_DWORD *)this + 6);
  v18 = *((_DWORD *)this + 7);
  v16[1] = 0x100000001LL;
  *v16 = off_42ABE0;
  apac::drc::DynRangeCompressor::APACDRCExtensionPayload::APACDRCExtensionPayload(
    (apac__drc__DynRangeCompressor__APACDRCExtensionPayload *)(v16 + 2),
    this,
    drcCfg,
    (unsigned int)f,
    0x400u,
    v17,
    v18,
    1);
  v19 = *((unsigned int **)this + 6);
  *((_QWORD *)this + 5) = v16 + 2;
  *((_QWORD *)this + 6) = v16;
  if ( v19 )
  {
    if ( &_pthread_key_create )
    {
      v20 = v19 + 2;
      do
        v21 = __ldaxr(v20);
      while ( __stlxr(v21 - 1, v20) );
    }
    else
    {
      v21 = v19[2];
      v19[2] = v21 - 1;
    }
    if ( v21 == 1 )
    {
      (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v19 + 16LL))(v19);
      if ( &_pthread_key_create )
      {
        v25 = v19 + 3;
        do
          v26 = __ldaxr(v25);
        while ( __stlxr(v26 - 1, v25) );
      }
      else
      {
        v26 = v19[3];
        v19[3] = v26 - 1;
      }
      if ( v26 == 1 )
        (*(void (__fastcall **)(unsigned int *))(*(_QWORD *)v19 + 24LL))(v19);
    }
    if ( !*((_QWORD *)this + 5) )
      return 1;
  }
  *((_WORD *)this + 5) = 1;
  *((float *)this + 8) = f;
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007ffb0  size 712 bytes  178 insns
 *   apac::drc::DynRangeCompressor::Initialize(apac::ParamList const&)
 *   [_ZN4apac3drc18DynRangeCompressor10InitializeERKNS_9ParamListE]
 *   data refs: 0x80280, 0x42ce00, 0x42de78, 0x43e520
 */
__int64 __fastcall apac::drc::DynRangeCompressor::Initialize(
        apac__drc__DynRangeCompressor *this,
        const apac__ParamList *a)
{
  unsigned int v4; // w22
  unsigned int v5; // w23
  unsigned int v6; // w0
  unsigned int v7; // w0
  void *v8; // x1
  unsigned int v9; // w23
  void **v10; // x19
  void **v11; // x20
  void **v12; // x19
  void **v13; // x20
  void **v14; // x19
  void **v15; // x20
  void (__fastcall ****v16)(MP4LoudnessMeasure *__hidden); // x19
  void (__fastcall ****v17)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v18)(MP4LoudnessMeasure *__hidden); // x20
  void (__fastcall ***v19)(MP4LoudnessMeasure *__hidden); // x22
  void (__fastcall *v20)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v21)(MP4LoudnessMeasure *__hidden); // x0
  void (__fastcall ****v22)(MP4LoudnessMeasure *__hidden); // x19
  void (__fastcall ****v23)(MP4LoudnessMeasure *__hidden); // x24
  void (__fastcall ***v24)(MP4LoudnessMeasure *__hidden); // x20
  void (__fastcall ***v25)(MP4LoudnessMeasure *__hidden); // x22
  void (__fastcall *v26)(MP4LoudnessMeasure *__hidden); // x1
  void (__fastcall ***v27)(MP4LoudnessMeasure *__hidden); // x0
  _BYTE v29[72]; // [xsp+58h] [xbp+58h] BYREF
  void *v30; // [xsp+A0h] [xbp+A0h]
  void *v31; // [xsp+C8h] [xbp+C8h]
  void *v32; // [xsp+E0h] [xbp+E0h]
  void (__fastcall ****v33)(MP4LoudnessMeasure *__hidden); // [xsp+118h] [xbp+118h]
  void (__fastcall ****v34)(MP4LoudnessMeasure *__hidden); // [xsp+120h] [xbp+120h]
  void (__fastcall ****v35)(MP4LoudnessMeasure *__hidden); // [xsp+130h] [xbp+130h]
  void (__fastcall ****v36)(MP4LoudnessMeasure *__hidden); // [xsp+138h] [xbp+138h]
  void *v37; // [xsp+148h] [xbp+148h]
  void **v38; // [xsp+160h] [xbp+160h]
  void **v39; // [xsp+168h] [xbp+168h]
  void **v40; // [xsp+178h] [xbp+178h]
  void **v41; // [xsp+180h] [xbp+180h]
  void **v42; // [xsp+190h] [xbp+190h]
  void **v43; // [xsp+198h] [xbp+198h]
  void *v44; // [xsp+1A8h] [xbp+1A8h]
  __int64 v45; // [xsp+1D0h] [xbp+1D0h]
  DRCConfiguration::DRCConfiguration((DRCConfiguration *)v29);
  v4 = apac::ParamList::get<unsigned int>(a, 2);
  v5 = apac::ParamList::get<unsigned int>(a, 4);
  v6 = apac::ParamList::get<unsigned int>(a, 0);
  v7 = apac::drc::DynRangeCompressor::Initialize(this, (DRCConfiguration *)v29, (float)v6, v4, v5, v5);
  v8 = v44;
  *((_QWORD *)this + 2) = 0;
  v9 = v7;
  if ( v8 )
    operator delete(v8);
  v10 = v42;
  v11 = v43;
  if ( v42 != v43 )
  {
    do
    {
      if ( *v10 )
        operator delete(*v10);
      v10 += 3;
    }
    while ( v11 != v10 );
    v11 = v42;
  }
  if ( v11 )
    operator delete(v11);
  v12 = v40;
  v13 = v41;
  if ( v40 != v41 )
  {
    do
    {
      if ( *v12 )
        operator delete(*v12);
      v12 += 3;
    }
    while ( v13 != v12 );
    v13 = v40;
  }
  if ( v13 )
    operator delete(v13);
  v14 = v38;
  v15 = v39;
  if ( v38 != v39 )
  {
    do
    {
      if ( *v14 )
        operator delete(*v14);
      v14 += 3;
    }
    while ( v15 != v14 );
    v15 = v38;
  }
  if ( v15 )
    operator delete(v15);
  if ( v37 )
    operator delete(v37);
  v16 = v35;
  v17 = v36;
  if ( v35 != v36 )
  {
    do
    {
      v18 = v16[2];
      v19 = v16[3];
      if ( v18 != v19 )
      {
        do
        {
          while ( 1 )
          {
            v20 = **v18;
            if ( v20 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
              break;
            v18 += 2;
            if ( v19 == v18 )
              goto LABEL_32;
          }
          v21 = v18;
          v18 += 2;
          v20((MP4LoudnessMeasure *)v21);
        }
        while ( v19 != v18 );
LABEL_32:
        v19 = v16[2];
      }
      if ( v19 )
        operator delete(v19);
      v16 += 5;
    }
    while ( v17 != v16 );
    v17 = v35;
  }
  if ( v17 )
    operator delete(v17);
  v22 = v33;
  v23 = v34;
  if ( v33 != v34 )
  {
    do
    {
      v24 = v22[2];
      v25 = v22[3];
      if ( v24 != v25 )
      {
        do
        {
          while ( 1 )
          {
            v26 = **v24;
            if ( v26 != MP4LoudnessMeasure::~MP4LoudnessMeasure )
              break;
            v24 += 2;
            if ( v25 == v24 )
              goto LABEL_45;
          }
          v27 = v24;
          v24 += 2;
          v26((MP4LoudnessMeasure *)v27);
        }
        while ( v25 != v24 );
LABEL_45:
        v25 = v22[2];
      }
      if ( v25 )
        operator delete(v25);
      v22 += 5;
    }
    while ( v23 != v22 );
    v23 = v33;
  }
  if ( v23 )
    operator delete(v23);
  if ( v32 )
    operator delete(v32);
  if ( v31 )
    operator delete(v31);
  if ( v30 )
    operator delete(v30);
  return v9;
}


