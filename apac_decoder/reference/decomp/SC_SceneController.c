/* ============================================================================
 * SC_SceneController
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 2088 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 *
 * NOTE: not required for the CarPlay 5.1 / 5.1.2 target (Ambisonics scene
 *       or low-bitrate path). Included for completeness only.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009dce0    420  SC_SceneController::UpdateChannelActivity() */
/* 0x0009de90    268  SC_SceneController::UpdateInteractivityGains() */
/* 0x0009fd00    972  SC_SceneController::Initialize(unsigned int, unsigned int, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, SC_AudioScenes*, DRCConfiguration*) */
/* 0x000a1010    428  SC_SceneController::ProcessAudio(float const* const*, float**, unsigned int) */

/* ------------------------------------------------------------------------ */
/* 0x0009dce0  size 420 bytes  104 insns
 *   SC_SceneController::UpdateChannelActivity()
 *   [_ZN18SC_SceneController21UpdateChannelActivityEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneController::UpdateChannelActivity(SC_SceneController *this)
{
  int v2; // w22
  char *v3; // x0
  char *v4; // x21
  _BYTE *v5; // x5
  _BYTE *v6; // x9
  __int64 v7; // x3
  unsigned __int64 v8; // x0
  unsigned int i; // w7
  __int64 v10; // x0
  int v11; // w1
  int v12; // w4
  unsigned int v13; // w2
  unsigned __int64 v14; // x3
  char v15; // w1
  __int64 v17; // [xsp+40h] [xbp+40h]
  v2 = *((_DWORD *)this + 144);
  v3 = *((char **)this + 69);
  v4 = *((char **)this + 71);
  if ( v4 == v3 )
  {
    if ( !v2 )
      goto LABEL_3;
LABEL_17:
    *(_QWORD *)v4 &= ~(0xFFFFFFFFFFFFFFFFLL >> -(char)v2);
    goto LABEL_3;
  }
  memset(v3, 0, v4 - v3);
  if ( v2 )
    goto LABEL_17;
LABEL_3:
  v5 = *((_BYTE **)this + 32);
  v6 = *((_BYTE **)this + 33);
  v7 = *((_QWORD *)this + 48);
  v8 = 0x8E38E38E38E38E39LL * ((v6 - v5) >> 2);
  if ( v8 > (*((_QWORD *)this + 49) - v7) >> 2 || v8 > (__int64)(*((_QWORD *)this + 55) - *((_QWORD *)this + 54)) >> 2 )
    return 1;
  for ( i = 0; v6 != v5; ++i )
  {
    while ( 1 )
    {
      if ( *v5 )
      {
        v10 = *((_QWORD *)this + 69);
        v11 = *(_DWORD *)(*((_QWORD *)this + 54) + 4LL * i);
        v12 = *(_DWORD *)(v7 + 4LL * i);
        if ( (unsigned int)(v11 + v12) > (unsigned __int64)*((unsigned int *)this + 144)
                                       + 8 * (*((_QWORD *)this + 71) - v10) )
          return 1;
        if ( v12 )
          break;
      }
      v5 += 36;
      ++i;
      if ( v6 == v5 )
        return 0;
    }
    v13 = 0;
    while ( 1 )
    {
      v14 = v13 + v11;
      v15 = v13++ + v11;
      *(_QWORD *)(v10 + 8 * (v14 >> 6)) |= 1LL << v15;
      v7 = *((_QWORD *)this + 48);
      if ( *(_DWORD *)(v7 + 4LL * i) <= v13 )
        break;
      v10 = *((_QWORD *)this + 69);
      v11 = *(_DWORD *)(*((_QWORD *)this + 54) + 4LL * i);
    }
    v5 += 36;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009de90  size 268 bytes  65 insns
 *   SC_SceneController::UpdateInteractivityGains()
 *   [_ZN18SC_SceneController24UpdateInteractivityGainsEv]
 *   data refs: 0x3303e0, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneController::UpdateInteractivityGains(SC_SceneController *this)
{
  __int64 v1; // x1
  __int64 v2; // x7
  __int64 v3; // x4
  __int64 v4; // x5
  unsigned __int64 v5; // x3
  unsigned int v6; // w2
  _DWORD *v7; // x6
  __int64 v9; // [xsp+30h] [xbp+30h]
  v1 = *((_QWORD *)this + 32);
  v2 = *((_QWORD *)this + 33);
  v3 = *((_QWORD *)this + 74);
  v4 = *((_QWORD *)this + 75) - v3;
  v5 = 0x8E38E38E38E38E39LL * ((v2 - v1) >> 2);
  if ( v5 == v4 >> 2 )
  {
    if ( v1 != v2 )
    {
      v6 = 0;
      while ( 1 )
      {
        v7 = (_DWORD *)(v3 + 4LL * v6);
        if ( *(_BYTE *)v1 )
        {
          if ( *(_BYTE *)(v1 + 1) )
            *v7 = *(_DWORD *)(v1 + 4);
          else
            *v7 = 0;
          v1 += 36;
          ++v6;
          if ( v2 == v1 )
            return 0;
        }
        else
        {
          v1 += 36;
          *(_DWORD *)(v3 + 4LL * v6++) = -971227136;
          if ( v2 == v1 )
            return 0;
        }
        v3 = *((_QWORD *)this + 74);
      }
    }
    return 0;
  }
  else
  {
    __printf_chk(1, "ERROR: unexpected size of mInteractivityGainsPerASC %d %d\n", v4 >> 2, v5);
    return 1;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x0009fd00  size 972 bytes  232 insns
 *   SC_SceneController::Initialize(unsigned int, unsigned int, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, SC_AudioScenes*, DRCConfiguration*)
 *   [_ZN18SC_SceneController10InitializeEjjRKSt6vectorIjSaIjEES4_S4_P14SC_AudioScenesP16DRCConfiguration]
 *   data refs: 0x330528, 0x330548, 0x330578, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneController::Initialize(
        SC_SceneController *this,
        unsigned int n,
        unsigned int n2,
        const std_vector *vec,
        const std_vector *vec2,
        const std_vector *vec3,
        SC_AudioScenes *scenes,
        DRCConfiguration *drcCfg)
{
  const std_vector *v8; // x25
  const std_vector *v9; // x26
  __int64 v10; // x27
  __int64 v11; // x28
  signed __int64 v15; // x2
  const SC_SceneComposition *v16; // x27
  unsigned int updated; // w20
  void *v20; // x1
  __int64 v21; // x2
  void *v22; // x2
  char *v23; // x3
  char *v24; // x6
  signed __int64 v25; // x20
  unsigned __int64 v26; // x0
  char *v27; // x1
  _QWORD *v28; // x2
  __int64 v29; // x5
  __int64 v30; // x5
  signed __int64 v31; // x1
  int v32; // w1
  __int64 v33; // x2
  void *v34; // x1
  _BYTE *end; // x0
  _BYTE *begin; // x1
  __int64 v37; // x3
  unsigned __int64 v38; // x0
  void *v39; // x1
  unsigned __int64 v40; // x0
  unsigned __int64 v41; // x2
  void *v42; // x0
  const std_vector *v43; // [xsp+40h] [xbp+40h]
  const std_vector *v44; // [xsp+48h] [xbp+48h]
  __int64 v45; // [xsp+50h] [xbp+50h]
  __int64 v46; // [xsp+58h] [xbp+58h]
  int v47; // [xsp+6Ch] [xbp+6Ch]
  __int64 v48; // [xsp+78h] [xbp+78h]
  std_vector v49; // [xsp+80h] [xbp+80h] BYREF
  __int64 v50; // [xsp+98h] [xbp+98h]
  __int64 v51; // [xsp+A0h] [xbp+A0h]
  *((_QWORD *)this + 5) = scenes;
  *((_QWORD *)this + 6) = scenes;
  *((_QWORD *)this + 68) = drcCfg;
  v15 = ((char *)vec->end - (char *)vec->begin) >> 2;
  *((_DWORD *)this + 1) = v15;
  if ( scenes )
  {
    v16 = *((const SC_SceneComposition **)scenes + 1);
    if ( v16 == *((const SC_SceneComposition **)scenes + 2) )
    {
      updated = -1;
      __printf_chk(1, "ERROR: scene composition data not found\n");
      v10 = v45;
      v11 = v46;
    }
    else
    {
      v8 = vec2;
      v9 = vec3;
      updated = SceneCompositionSettings::Initialize(
                  (SC_SceneController *)((char *)this + 200),
                  *((const SC_SceneComposition **)scenes + 1),
                  v15);
      if ( updated )
        goto LABEL_4;
      updated = SceneCompositionSettings::SetFromPreset(
                  (SC_SceneController *)((char *)this + 200),
                  *((_WORD *)this + 2),
                  v16,
                  0);
      if ( updated )
        goto LABEL_4;
      v21 = *((_QWORD *)this + 2);
      v20 = *((void **)this + 3);
      LOWORD(v49.begin) = 0;
      if ( ((__int64)v20 - v21) >> 1 )
      {
        if ( (unsigned __int64)(((__int64)v20 - v21) >> 1) > 1 )
        {
          v22 = (void *)(v21 + 2);
          if ( v20 != v22 )
            *((_QWORD *)this + 3) = v22;
        }
      }
      else
      {
        std::vector<unsigned short>::_M_fill_insert((_DWORD)this + 16, v20);
      }
      v24 = *((char **)this + 32);
      v23 = *((char **)this + 33);
      *(_WORD *)this = 1;
      memset(&v49, 0, sizeof(v49));
      v25 = v23 - v24;
      v26 = 0x8E38E38E38E38E39LL * ((v23 - v24) >> 2);
      if ( v26 )
      {
        if ( v26 > 0x38E38E38E38E38ELL )
          std::__throw_bad_alloc();
        v26 = operator new(v23 - v24);
        v24 = *((char **)this + 32);
        v23 = *((char **)this + 33);
      }
      v49.begin = (void *)v26;
      v49.end = (void *)v26;
      v49.cap = (void *)(v26 + v25);
      if ( v23 == v24 )
      {
        v32 = 0;
      }
      else
      {
        v27 = v24;
        v28 = (_QWORD *)v26;
        do
        {
          v29 = *((_QWORD *)v27 + 1);
          *v28 = *(_QWORD *)v27;
          v28[1] = v29;
          v30 = *((_QWORD *)v27 + 3);
          v28[2] = *((_QWORD *)v27 + 2);
          v28[3] = v30;
          v27 += 36;
          v28 = (_QWORD *)((char *)v28 + 36);
          *((_DWORD *)v28 - 1) = *((_DWORD *)v27 - 1);
        }
        while ( v23 != v27 );
        v31 = 36 * ((0xE38E38E38E38E39LL * ((unsigned __int64)(v23 - 36 - v24) >> 2)) & 0x3FFFFFFFFFFFFFFFLL) + 36;
        v26 += v31;
        v32 = 954437177 * (v31 >> 2);
      }
      v49.end = (void *)v26;
      if ( *((_DWORD *)this + 1) == v32 )
      {
        updated = SC_SceneProcessor::Initialize((SC_SceneController *)((char *)this + 304), n, n2, vec, v8, v9, &v49);
        if ( !updated )
        {
          v33 = *((_QWORD *)this + 69);
          v34 = *((void **)this + 71);
          if ( (unsigned __int64)*((unsigned int *)this + 144) + 8 * ((__int64)v34 - v33) <= n )
          {
            LODWORD(v48) = *((_DWORD *)this + 144);
            std::vector<bool>::_M_fill_insert((_DWORD)this + 552, v34);
          }
          else
          {
            *((_DWORD *)this + 144) = n & 0x3F;
            *((_QWORD *)this + 71) = v33 + 8 * ((__int64)n >> 6);
          }
          updated = SC_SceneController::UpdateChannelActivity(this);
          if ( !updated )
          {
            begin = vec->begin;
            end = vec->end;
            v47 = 0;
            v37 = *((_QWORD *)this + 74);
            v38 = end - begin;
            v39 = *((void **)this + 75);
            v40 = (unsigned int)(v38 >> 2);
            v41 = ((__int64)v39 - v37) >> 2;
            if ( v40 > v41 )
            {
              std::vector<float>::_M_fill_insert((_DWORD)this + 592, v39, v40 - v41);
            }
            else if ( v40 < v41 )
            {
              v42 = (void *)(v37 + 4 * v40);
              if ( v39 != v42 )
                *((_QWORD *)this + 75) = v42;
            }
            updated = SC_SceneController::UpdateInteractivityGains(this);
            if ( !updated )
            {
              DRCConfiguration::SetAudioScenePresetParams(
                *((_QWORD *)this + 68),
                0,
                (char *)this + 552,
                (char *)this + 592,
                (char *)this + 280);
              DRCConfiguration::SetAudioScenePresetParamsChanged(*((DRCConfiguration **)this + 68), 1u);
            }
          }
        }
      }
      else
      {
        __printf_chk(1, "ERROR: sceneComponentModifiers size mismatch\n");
        updated = -1;
      }
      if ( !v49.begin )
      {
LABEL_4:
        v10 = v45;
        v11 = v46;
      }
      else
      {
        operator delete(v49.begin);
        v10 = v45;
        v11 = v46;
      }
    }
  }
  else
  {
    updated = -1;
    __printf_chk(1, "ERROR: scene data not found\n");
  }
  if ( v50 != _stack_chk_guard )
  {
    v43 = v8;
    v44 = v9;
    v45 = v10;
    v46 = v11;
  }
  return updated;
}


/* ------------------------------------------------------------------------ */
/* 0x000a1010  size 428 bytes  107 insns
 *   SC_SceneController::ProcessAudio(float const* const*, float**, unsigned int)
 *   [_ZN18SC_SceneController12ProcessAudioEPKPKfPPfj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneController::ProcessAudio(
        SC_SceneController *this,
        const float **a,
        float **buf,
        unsigned int n)
{
  SC_SceneProcessor *v4; // x25
  __int64 v9; // x2
  unsigned int updated; // w20
  __int64 v12; // x0
  SC_SceneProcessor *v13; // [xsp+40h] [xbp+40h]
  __int64 v14; // [xsp+58h] [xbp+58h]
  __int64 v15; // [xsp+60h] [xbp+60h]
  if ( *((_BYTE *)this + 12) )
  {
    v9 = *((_QWORD *)this + 5);
    *((_BYTE *)this + 12) = 0;
    updated = SceneCompositionSettings::SetFromPreset(
                (SC_SceneController *)((char *)this + 200),
                *((_WORD *)this + 2),
                *(const SC_SceneComposition **)(v9 + 8),
                0);
    if ( updated )
      goto LABEL_4;
    v4 = (SC_SceneController *)((char *)this + 304);
    updated = SC_SceneProcessor::Update(
                (SC_SceneController *)((char *)this + 304),
                (const std_vector *)((char *)this + 256));
    if ( updated )
      goto LABEL_4;
LABEL_9:
    updated = SC_SceneController::UpdateChannelActivity(this);
    if ( !updated )
    {
      updated = SC_SceneController::UpdateInteractivityGains(this);
      if ( !updated )
      {
        updated = SC_SceneProcessor::Process(v4, a, buf, n);
        if ( !updated )
        {
          DRCConfiguration::SetAudioScenePresetParams(
            *((_QWORD *)this + 68),
            0,
            (char *)this + 552,
            (char *)this + 592,
            (char *)this + 280);
          DRCConfiguration::SetAudioScenePresetParamsChanged(*((DRCConfiguration **)this + 68), 1u);
        }
      }
    }
LABEL_4:
    if ( v14 == _stack_chk_guard )
      return updated;
LABEL_15:
    v13 = v4;
  }
  if ( *((_BYTE *)this + 13) )
  {
    v12 = *((_QWORD *)this + 5);
    *((_BYTE *)this + 13) = 0;
    updated = SceneCompositionSettings::ApplyUserInput(
                (SC_SceneController *)((char *)this + 200),
                *((_DWORD *)this + 1),
                (const SC_SceneComposition *)(*(_QWORD *)(v12 + 8) + 488LL * *((unsigned int *)this + 2)),
                (const CD_AudioSceneControlData__SceneCompositionData *)(*((_QWORD *)this + 15)
                                                                       + 88LL * *((unsigned int *)this + 2)));
    if ( updated )
      goto LABEL_4;
    v4 = (SC_SceneController *)((char *)this + 304);
    updated = SC_SceneProcessor::Update(
                (SC_SceneController *)((char *)this + 304),
                (const std_vector *)((char *)this + 256));
    if ( updated )
      goto LABEL_4;
    goto LABEL_9;
  }
  if ( v14 != _stack_chk_guard )
    goto LABEL_15;
  return SC_SceneProcessor::Process((SC_SceneController *)((char *)this + 304), a, buf, n);
}


