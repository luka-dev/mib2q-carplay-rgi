/* ============================================================================
 * DRCConfiguration
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 20 function(s), 3288 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00082850    356  DRCConfiguration::~DRCConfiguration() */
/* 0x000b3420     64  DRCConfiguration::LogStatus() const */
/* 0x000b3460    104  DRCConfiguration::SetCompressionMode(unsigned int) */
/* 0x000b34d0     72  DRCConfiguration::SetTargetLevelChanged(unsigned char) */
/* 0x000b3520    148  DRCConfiguration::SetTargetLevel(float) */
/* 0x000b35c0     72  DRCConfiguration::SetCutBoostChanged(unsigned char) */
/* 0x000b3610    156  DRCConfiguration::SetCutFactor(float) */
/* 0x000b36b0    156  DRCConfiguration::SetBoostFactor(float) */
/* 0x000b3750     72  DRCConfiguration::SetSensitivityChanged(unsigned char) */
/* 0x000b37a0     72  DRCConfiguration::SetPlaybackGainChanged(unsigned char) */
/* 0x000b37f0     72  DRCConfiguration::SetAudioScenePresetParamsChanged(unsigned char) */
/* 0x000b3840     72  DRCConfiguration::SetLoudnessEqStrengthChanged(unsigned char) */
/* 0x000b3890     72  DRCConfiguration::SetUniDRCCompressionProfileChanged(unsigned char) */
/* 0x000b38e0    132  DRCConfiguration::SetUniDRCCompressionProfile(unsigned int) */
/* 0x000b3970    108  DRCConfiguration::SetUniDrcDelay(unsigned int) */
/* 0x000b39e0     72  DRCConfiguration::SetUniDrcDelayChanged(unsigned char) */
/* 0x000b3a30     72  DRCConfiguration::SetUniDrcDelayIsSetOnCodec(unsigned char) */
/* 0x000b3a80     80  DRCConfiguration::SetAlbumMode(unsigned char) */
/* 0x000b3ad0    908  DRCConfiguration::SetAudioScenePresetParams(unsigned char, std::vector<bool, std::allocator<bool> > const&, std::vector<float, std::allocator<float> > const&, std::vector<float, std::allocator<float> > const&) */
/* 0x000b3e60    428  DRCConfiguration::DRCConfiguration() */

/* ------------------------------------------------------------------------ */
/* 0x00082850  size 356 bytes  89 insns
 *   DRCConfiguration::~DRCConfiguration()
 *   DRCConfiguration::~DRCConfiguration()
 *   [_ZN16DRCConfigurationD2Ev]
 *   [_ZN16DRCConfigurationD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN16DRCConfigurationD1Ev'
void __fastcall DRCConfiguration::~DRCConfiguration(DRCConfiguration *this)
{
  void *v2; // x0
  void **v3; // x20
  void **v4; // x21
  void **v5; // x20
  void **v6; // x21
  void **v7; // x20
  void **v8; // x21
  void *v9; // x0
  void *v10; // x0
  void *v11; // x0
  void *v12; // x0
  __int64 v13; // [xsp+40h] [xbp+40h]
  v2 = *((void **)this + 42);
  if ( v2 )
    operator delete(v2);
  v3 = *((void ***)this + 39);
  v4 = *((void ***)this + 40);
  if ( v3 != v4 )
  {
    do
    {
      if ( *v3 )
        operator delete(*v3);
      v3 += 3;
    }
    while ( v4 != v3 );
    v4 = *((void ***)this + 39);
  }
  if ( v4 )
    operator delete(v4);
  v5 = *((void ***)this + 36);
  v6 = *((void ***)this + 37);
  if ( v5 != v6 )
  {
    do
    {
      if ( *v5 )
        operator delete(*v5);
      v5 += 3;
    }
    while ( v6 != v5 );
    v6 = *((void ***)this + 36);
  }
  if ( v6 )
    operator delete(v6);
  v7 = *((void ***)this + 33);
  v8 = *((void ***)this + 34);
  if ( v7 != v8 )
  {
    do
    {
      if ( *v7 )
        operator delete(*v7);
      v7 += 3;
    }
    while ( v8 != v7 );
    v8 = *((void ***)this + 33);
  }
  if ( v8 )
    operator delete(v8);
  v9 = *((void **)this + 30);
  if ( v9 )
    operator delete(v9);
  std::vector<MP4LoudnessInfo>::~vector((char *)this + 216);
  std::vector<MP4LoudnessInfo>::~vector((char *)this + 192);
  v10 = *((void **)this + 17);
  if ( v10 )
    operator delete(v10);
  v11 = *((void **)this + 14);
  if ( v11 )
    operator delete(v11);
  v12 = *((void **)this + 9);
  if ( v12 )
    operator delete(v12);
}


/* ------------------------------------------------------------------------ */
/* 0x000b3420  size 64 bytes  16 insns
 *   DRCConfiguration::LogStatus() const
 *   [_ZNK16DRCConfiguration9LogStatusEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::LogStatus(DRCConfiguration *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  return v2 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3460  size 104 bytes  26 insns
 *   DRCConfiguration::SetCompressionMode(unsigned int)
 *   [_ZN16DRCConfiguration18SetCompressionModeEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetCompressionMode(DRCConfiguration *this, unsigned int n)
{
  __int64 v4; // [xsp+20h] [xbp+20h]
  if ( n > 2 )
    return 0;
  *((_BYTE *)this + 4) = 1;
  *((_DWORD *)this + 2) = n;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b34d0  size 72 bytes  18 insns
 *   DRCConfiguration::SetTargetLevelChanged(unsigned char)
 *   [_ZN16DRCConfiguration21SetTargetLevelChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetTargetLevelChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 12) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3520  size 148 bytes  37 insns
 *   DRCConfiguration::SetTargetLevel(float)
 *   [_ZN16DRCConfiguration14SetTargetLevelEf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetTargetLevel(DRCConfiguration *this, float f)
{
  __int64 result; // x0
  __int64 v4; // [xsp+20h] [xbp+20h]
  result = 0;
  if ( f >= -31.0 && f <= -9.0 )
  {
    if ( *((float *)this + 4) != f )
    {
      *((_BYTE *)this + 12) = 1;
      *((float *)this + 4) = f;
    }
    result = 1;
    if ( !*((_BYTE *)this + 13) )
      *((_WORD *)this + 6) = 257;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b35c0  size 72 bytes  18 insns
 *   DRCConfiguration::SetCutBoostChanged(unsigned char)
 *   [_ZN16DRCConfiguration18SetCutBoostChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetCutBoostChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 32) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3610  size 156 bytes  39 insns
 *   DRCConfiguration::SetCutFactor(float)
 *   [_ZN16DRCConfiguration12SetCutFactorEf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetCutFactor(DRCConfiguration *this, float f)
{
  unsigned int v2; // w21
  float v5; // [xsp+3Ch] [xbp+3Ch]
  __int64 v6; // [xsp+50h] [xbp+50h]
  v2 = 0;
  if ( f >= 0.0 && f <= 1.0 )
  {
    v2 = 1;
    if ( *((float *)this + 6) != f )
    {
      v5 = f;
      DRCConfiguration::SetCutBoostChanged(this, 1u);
      *((float *)this + 6) = v5;
    }
  }
  return v2;
}


/* ------------------------------------------------------------------------ */
/* 0x000b36b0  size 156 bytes  39 insns
 *   DRCConfiguration::SetBoostFactor(float)
 *   [_ZN16DRCConfiguration14SetBoostFactorEf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetBoostFactor(DRCConfiguration *this, float f)
{
  unsigned int v2; // w21
  float v5; // [xsp+3Ch] [xbp+3Ch]
  __int64 v6; // [xsp+50h] [xbp+50h]
  v2 = 0;
  if ( f >= 0.0 && f <= 1.0 )
  {
    v2 = 1;
    if ( *((float *)this + 7) != f )
    {
      v5 = f;
      DRCConfiguration::SetCutBoostChanged(this, 1u);
      *((float *)this + 7) = v5;
    }
  }
  return v2;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3750  size 72 bytes  18 insns
 *   DRCConfiguration::SetSensitivityChanged(unsigned char)
 *   [_ZN16DRCConfiguration21SetSensitivityChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetSensitivityChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 40) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b37a0  size 72 bytes  18 insns
 *   DRCConfiguration::SetPlaybackGainChanged(unsigned char)
 *   [_ZN16DRCConfiguration22SetPlaybackGainChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetPlaybackGainChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 48) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b37f0  size 72 bytes  18 insns
 *   DRCConfiguration::SetAudioScenePresetParamsChanged(unsigned char)
 *   [_ZN16DRCConfiguration32SetAudioScenePresetParamsChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetAudioScenePresetParamsChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 64) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3840  size 72 bytes  18 insns
 *   DRCConfiguration::SetLoudnessEqStrengthChanged(unsigned char)
 *   [_ZN16DRCConfiguration28SetLoudnessEqStrengthChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetLoudnessEqStrengthChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 56) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3890  size 72 bytes  18 insns
 *   DRCConfiguration::SetUniDRCCompressionProfileChanged(unsigned char)
 *   [_ZN16DRCConfiguration34SetUniDRCCompressionProfileChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetUniDRCCompressionProfileChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 33) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b38e0  size 132 bytes  33 insns
 *   DRCConfiguration::SetUniDRCCompressionProfile(unsigned int)
 *   [_ZN16DRCConfiguration27SetUniDRCCompressionProfileEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetUniDRCCompressionProfile(DRCConfiguration *this, unsigned int n)
{
  __int64 result; // x0
  __int64 v4; // [xsp+20h] [xbp+20h]
  result = 0;
  if ( n <= 8 )
  {
    if ( *((_DWORD *)this + 9) != n )
    {
      *((_BYTE *)this + 33) = 1;
      *((_DWORD *)this + 9) = n;
    }
    result = 1;
    if ( !*((_BYTE *)this + 34) )
      *(_WORD *)((char *)this + 33) = 257;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3970  size 108 bytes  27 insns
 *   DRCConfiguration::SetUniDrcDelay(unsigned int)
 *   [_ZN16DRCConfiguration14SetUniDrcDelayEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetUniDrcDelay(DRCConfiguration *this, unsigned int n)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  if ( *((_DWORD *)this + 44) != n && !*((_BYTE *)this + 180) )
  {
    *((_DWORD *)this + 44) = n;
    *((_BYTE *)this + 181) = 1;
  }
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b39e0  size 72 bytes  18 insns
 *   DRCConfiguration::SetUniDrcDelayChanged(unsigned char)
 *   [_ZN16DRCConfiguration21SetUniDrcDelayChangedEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetUniDrcDelayChanged(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 181) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3a30  size 72 bytes  18 insns
 *   DRCConfiguration::SetUniDrcDelayIsSetOnCodec(unsigned char)
 *   [_ZN16DRCConfiguration26SetUniDrcDelayIsSetOnCodecEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetUniDrcDelayIsSetOnCodec(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 180) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3a80  size 80 bytes  20 insns
 *   DRCConfiguration::SetAlbumMode(unsigned char)
 *   [_ZN16DRCConfiguration12SetAlbumModeEh]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetAlbumMode(DRCConfiguration *this, unsigned __int8 a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_BYTE *)this + 168) = 1;
  *((_BYTE *)this + 169) = a;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3ad0  size 908 bytes  227 insns
 *   DRCConfiguration::SetAudioScenePresetParams(unsigned char, std::vector<bool, std::allocator<bool> > const&, std::vector<float, std::allocator<float> > const&, std::vector<float, std::allocator<float> > const&)
 *   [_ZN16DRCConfiguration25SetAudioScenePresetParamsEhRKSt6vectorIbSaIbEERKS0_IfSaIfEES8_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCConfiguration::SetAudioScenePresetParams(
        DRCConfiguration *this,
        unsigned __int8 a,
        const std_vector *vec,
        const std_vector *vec2,
        const std_vector *vec3)
{
  unsigned int v9; // w3
  void *v10; // x5
  _BYTE *v11; // x6
  char *v12; // x3
  char *begin; // x1
  char *cap; // x25
  size_t v15; // x2
  __int64 begin_low; // x21
  signed __int64 v17; // x28
  __int64 *v18; // x2
  int i; // w5
  __int64 v20; // x0
  float *v21; // x1
  float *v22; // x0
  float *v23; // x2
  float *v24; // x1
  float *v25; // x0
  float *v26; // x2
  __int64 v28; // x2
  int v29; // w0
  __int64 v31; // x21
  __int64 v32; // x1
  __int64 v33; // x1
  __int64 v34; // x0
  char *v35; // x1
  char *v36; // [xsp+68h] [xbp+68h]
  __int64 *v37; // [xsp+78h] [xbp+78h]
  int v38; // [xsp+80h] [xbp+80h]
  __int64 v39; // [xsp+90h] [xbp+90h]
  if ( *((_DWORD *)this + 17) != a )
  {
    *((_BYTE *)this + 64) = 1;
    *((_DWORD *)this + 17) = a;
  }
  v9 = *((_DWORD *)this + 24);
  v10 = *((void **)this + 9);
  v11 = *((_BYTE **)this + 11);
  if ( v9 + 8 * (v11 - (_BYTE *)v10) == LODWORD(vec[1].begin) + 8 * ((char *)vec->cap - (char *)vec->begin) )
  {
    v28 = 0;
LABEL_34:
    v29 = 0;
    while ( v11 != (char *)v10 + v28 || v9 != v29 )
    {
      if ( (((1LL << v29) & *(_QWORD *)((_BYTE *)v10 + v28)) != 0) != (((1LL << v29)
                                                                      & *(_QWORD *)((_BYTE *)vec->begin + v28)) != 0) )
        goto LABEL_4;
      if ( ++v29 == 64 )
      {
        v28 += 8;
        goto LABEL_34;
      }
    }
  }
  else
  {
LABEL_4:
    *((_BYTE *)this + 64) = 1;
    v12 = (char *)this + 72;
    if ( vec != (const std_vector *)((char *)this + 72) )
    {
      begin = (char *)vec->begin;
      cap = (char *)vec->cap;
      v15 = cap - (char *)vec->begin;
      begin_low = LODWORD(vec[1].begin);
      v17 = begin_low + 8 * v15;
      if ( v17 > (unsigned __int64)(8LL * (*((_QWORD *)this + 13) - (_QWORD)v10)) )
      {
        if ( !v10 )
          goto LABEL_54;
        v36 = (char *)this + 72;
        operator delete(v10);
        *((_QWORD *)this + 9) = 0;
        v12 = v36;
        *((_QWORD *)this + 11) = 0;
        *((_DWORD *)this + 24) = 0;
        *((_DWORD *)v12 + 2) = *((_DWORD *)this + 24);
        *((_QWORD *)v12 + 4) = 0;
        v17 = LODWORD(vec[1].begin) + 8 * ((char *)vec->cap - (char *)vec->begin);
        if ( v17 )
        {
LABEL_54:
          v31 = v17 + 63;
          v36 = v12;
          v10 = (void *)operator new(8 * ((unsigned __int64)(v17 + 63) >> 6));
          *((_QWORD *)v36 + 4) = (char *)v10 + 8 * ((unsigned __int64)(v17 + 63) >> 6);
          *((_QWORD *)this + 9) = v10;
          *((_DWORD *)this + 20) = 0;
        }
        else
        {
          v10 = nullptr;
          v31 = 63;
          *((_QWORD *)this + 9) = 0;
          *((_DWORD *)this + 20) = 0;
        }
        if ( v17 >= 0 )
          v32 = v17;
        else
          v32 = v31;
        v33 = v32 >> 6;
        if ( v17 <= 0 )
          v34 = -(-v17 & 0x3F);
        else
          v34 = v17 & 0x3F;
        v35 = (char *)v10 + 8 * v33;
        if ( v34 < 0 )
        {
          LODWORD(v34) = v34 + 64;
          v35 -= 8;
        }
        *((_QWORD *)this + 11) = v35;
        *((_DWORD *)this + 24) = v34;
        begin = (char *)vec->begin;
        cap = (char *)vec->cap;
        begin_low = LODWORD(vec[1].begin);
        v15 = cap - (char *)vec->begin;
      }
      if ( cap != begin )
      {
        v36 = (char *)v15;
        v10 = memmove(v10, begin, v15);
        v15 = (size_t)v36;
      }
      v18 = (__int64 *)((char *)v10 + v15);
      for ( i = 0; begin_low; i = 0 )
      {
        while ( 1 )
        {
          v20 = *v18 & ~(1LL << i);
          if ( ((1LL << i) & *(_QWORD *)cap) != 0 )
            v20 = (1LL << i) | *v18;
          *v18 = v20;
          if ( i == 63 )
            break;
          ++i;
          if ( !--begin_low )
            goto LABEL_15;
        }
        cap += 8;
        ++v18;
        --begin_low;
      }
LABEL_15:
      *((_QWORD *)this + 11) = v18;
      *((_DWORD *)this + 24) = i;
      v37 = v18;
      v38 = i;
    }
  }
  v21 = (float *)vec2->begin;
  v22 = *((float **)this + 14);
  v23 = *((float **)this + 15);
  if ( (char *)v23 - (char *)v22 != (char *)vec2->end - (char *)vec2->begin )
    goto LABEL_17;
  if ( v22 != v23 )
  {
    while ( *v22 == *v21 )
    {
      ++v22;
      ++v21;
      if ( v23 == v22 )
        goto LABEL_18;
    }
LABEL_17:
    *((_BYTE *)this + 64) = 1;
    std::vector<float>::operator=((char *)this + 112, vec2);
  }
LABEL_18:
  v24 = (float *)vec3->begin;
  v25 = *((float **)this + 17);
  v26 = *((float **)this + 18);
  if ( (char *)v26 - (char *)v25 != (char *)vec3->end - (char *)vec3->begin )
  {
LABEL_19:
    *((_BYTE *)this + 64) = 1;
    std::vector<float>::operator=((char *)this + 136, vec3);
    goto LABEL_20;
  }
  if ( v25 != v26 )
  {
    while ( *v25 == *v24 )
    {
      ++v25;
      ++v24;
      if ( v26 == v25 )
        goto LABEL_20;
    }
    goto LABEL_19;
  }
LABEL_20:
  if ( !*((_BYTE *)this + 65) )
    *((_WORD *)this + 32) = 257;
  return 1;
}


/* ------------------------------------------------------------------------ */
/* 0x000b3e60  size 428 bytes  107 insns
 *   DRCConfiguration::DRCConfiguration()
 *   DRCConfiguration::DRCConfiguration()
 *   [_ZN16DRCConfigurationC1Ev]
 *   [_ZN16DRCConfigurationC2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN16DRCConfigurationC2Ev'
void __fastcall DRCConfiguration::DRCConfiguration(DRCConfiguration *this)
{
  __int64 v1; // [xsp+60h] [xbp+60h]
  *(_DWORD *)this = 1;
  *((_BYTE *)this + 4) = 0;
  *((_DWORD *)this + 2) = 0;
  *((_WORD *)this + 6) = 0;
  *((_QWORD *)this + 2) = 0xFFFFFFFF00000000LL;
  *((_QWORD *)this + 3) = 0x3F8000003F800000LL;
  *((_WORD *)this + 16) = 0;
  *((_BYTE *)this + 34) = 0;
  *((_DWORD *)this + 9) = 0;
  *((_WORD *)this + 20) = 0;
  *((_DWORD *)this + 11) = 0;
  *((_WORD *)this + 24) = 0;
  *((_DWORD *)this + 13) = 0;
  *((_WORD *)this + 28) = 0;
  *((_DWORD *)this + 15) = 0;
  *((_WORD *)this + 32) = 0;
  *(_QWORD *)((char *)this + 68) = 0;
  *(_QWORD *)((char *)this + 76) = 0;
  *((_QWORD *)this + 11) = 0;
  *((_DWORD *)this + 24) = 0;
  *((_QWORD *)this + 13) = 0;
  *((_QWORD *)this + 14) = 0;
  *((_QWORD *)this + 15) = 0;
  *((_QWORD *)this + 16) = 0;
  *((_QWORD *)this + 17) = 0;
  *((_QWORD *)this + 18) = 0;
  *((_QWORD *)this + 19) = 0;
  *((_BYTE *)this + 160) = 0;
  *((_DWORD *)this + 41) = 0;
  *((_WORD *)this + 84) = 0;
  *((_BYTE *)this + 170) = 0;
  *((_DWORD *)this + 43) = 0;
  *((_QWORD *)this + 22) = 0;
  *((_QWORD *)this + 23) = 0;
  *((_QWORD *)this + 24) = 0;
  *((_QWORD *)this + 25) = 0;
  *((_QWORD *)this + 26) = 0;
  *((_QWORD *)this + 27) = 0;
  *((_QWORD *)this + 28) = 0;
  *((_QWORD *)this + 29) = 0;
  *((_QWORD *)this + 30) = 0;
  *((_QWORD *)this + 31) = 0;
  *((_QWORD *)this + 32) = 0;
  *((_QWORD *)this + 33) = 0;
  *((_QWORD *)this + 34) = 0;
  *((_QWORD *)this + 35) = 0;
  *((_QWORD *)this + 36) = 0;
  *((_QWORD *)this + 37) = 0;
  *((_QWORD *)this + 38) = 0;
  *((_QWORD *)this + 39) = 0;
  *((_QWORD *)this + 40) = 0;
  *((_QWORD *)this + 41) = 0;
  *((_QWORD *)this + 42) = 0;
  *((_QWORD *)this + 43) = 0;
  *((_QWORD *)this + 44) = 0;
  *((_BYTE *)this + 360) = 0;
  DRCLoggingRegisterScopeOnce();
}


