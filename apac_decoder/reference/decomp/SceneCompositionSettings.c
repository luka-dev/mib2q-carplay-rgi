/* ============================================================================
 * SceneCompositionSettings
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 6620 bytes of .text.
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
/* 0x0009dfa0    420  SceneCompositionSettings::Initialize(SC_SceneComposition const&, unsigned int) */
/* 0x0009eb20    320  SceneCompositionSettings::SetEffectiveInteractivityGains(SC_SceneComposition const&) */
/* 0x0009f540   1984  SceneCompositionSettings::SetFromPreset(unsigned short, SC_SceneComposition const&, unsigned short) */
/* 0x000a00d0   3896  SceneCompositionSettings::ApplyUserInput(unsigned int, SC_SceneComposition const&, CD_AudioSceneControlData::SceneCompositionData const&) */

/* ------------------------------------------------------------------------ */
/* 0x0009dfa0  size 420 bytes  101 insns
 *   SceneCompositionSettings::Initialize(SC_SceneComposition const&, unsigned int)
 *   [_ZN24SceneCompositionSettings10InitializeERK19SC_SceneCompositionj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SceneCompositionSettings::Initialize(
        SceneCompositionSettings *this,
        const SC_SceneComposition *sceneComp,
        unsigned int n)
{
  __int64 v3; // x23
  __int64 v4; // x24
  __int64 v5; // x25
  void *v9; // x22
  __int64 v10; // x3
  unsigned __int16 v11; // w2
  __int64 v13; // x19
  char *v14; // x25
  char *v15; // x23
  signed __int64 v16; // x2
  __int64 v17; // [xsp+30h] [xbp+30h]
  __int64 v18; // [xsp+38h] [xbp+38h]
  __int64 v19; // [xsp+40h] [xbp+40h]
  __int64 v20; // [xsp+58h] [xbp+58h]
  __int64 v21; // [xsp+60h] [xbp+60h]
  if ( *((_BYTE *)sceneComp + 304) )
    std::vector<SceneCompositionSettings::CategorySettings>::reserve(
      (char *)this + 8,
      0x2E8BA2E8BA2E8BA3LL * ((__int64)(*((_QWORD *)sceneComp + 40) - *((_QWORD *)sceneComp + 39)) >> 4));
  std::vector<SceneCompositionSettings::SelectionGroupSettings>::reserve(
    (char *)this + 32,
    (__int64)(*((_QWORD *)sceneComp + 17) - *((_QWORD *)sceneComp + 16)) >> 8);
  std::vector<SC_ComponentModifier>::reserve((std_vector *)((char *)this + 56), n);
  v9 = *((void **)this + 10);
  v10 = *((_QWORD *)this + 12) - (_QWORD)v9;
  v11 = -13107 * ((__int64)(*((_QWORD *)sceneComp + 11) - *((_QWORD *)sceneComp + 10)) >> 4)
      - 29127 * ((__int64)(*((_QWORD *)sceneComp + 8) - *((_QWORD *)sceneComp + 7)) >> 4);
  *((_WORD *)this + 1) = v11;
  if ( v10 >> 2 < (unsigned __int64)v11 )
  {
    v17 = v3;
    v13 = 4LL * v11;
    v19 = v5;
    v4 = *((_QWORD *)this + 11) - (_QWORD)v9;
    if ( v11 )
    {
      v14 = (char *)this + 80;
      v15 = (char *)operator new(4LL * v11);
      v9 = *((void **)this + 10);
      v16 = *((_QWORD *)this + 11) - (_QWORD)v9;
    }
    else
    {
      v16 = *((_QWORD *)this + 11) - (_QWORD)v9;
      v14 = (char *)this + 80;
      v15 = nullptr;
    }
    if ( v16 > 0 )
    {
      memmove(v15, v9, v16);
    }
    else if ( !v9 )
    {
LABEL_10:
      *((_QWORD *)this + 10) = v15;
      *((_QWORD *)v14 + 1) = &v15[v4];
      *((_QWORD *)v14 + 2) = &v15[v13];
      v3 = v17;
      v5 = v19;
      goto LABEL_4;
    }
    operator delete(v9);
    goto LABEL_10;
  }
LABEL_4:
  if ( v20 != _stack_chk_guard )
  {
    v17 = v3;
    v18 = v4;
    v19 = v5;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009eb20  size 320 bytes  79 insns
 *   SceneCompositionSettings::SetEffectiveInteractivityGains(SC_SceneComposition const&)
 *   [_ZN24SceneCompositionSettings30SetEffectiveInteractivityGainsERK19SC_SceneComposition]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SceneCompositionSettings::SetEffectiveInteractivityGains(
        SceneCompositionSettings *this,
        const SC_SceneComposition *sceneComp)
{
  void *v4; // x1
  size_t v5; // x2
  __int64 v6; // x0
  __int64 v7; // x4
  unsigned int v8; // w19
  __int64 v9; // x22
  int SelectedItemIndex; // w0
  __int64 v11; // x1
  int v13; // [xsp+44h] [xbp+44h]
  __int64 v14; // [xsp+50h] [xbp+50h]
  v4 = *((void **)this + 10);
  v5 = *((unsigned __int16 *)this + 1);
  if ( v4 != *((void **)this + 11) )
  {
    v13 = -971227136;
    *((_QWORD *)this + 11) = v4;
    if ( !v5 )
      goto LABEL_4;
    goto LABEL_3;
  }
  v13 = -971227136;
  if ( v5 )
LABEL_3:
    std::vector<float>::_M_fill_insert((_DWORD)this + 80, v4, v5);
LABEL_4:
  v6 = *((_QWORD *)sceneComp + 16);
  v7 = *((_QWORD *)sceneComp + 17);
  v8 = 0;
  if ( !(unsigned int)((v7 - v6) >> 8) )
    return 0;
  while ( 1 )
  {
    while ( 1 )
    {
      v9 = *((_QWORD *)this + 4) + 88LL * v8;
      if ( *(_BYTE *)v9 )
        break;
LABEL_6:
      if ( ++v8 >= (unsigned int)((v7 - v6) >> 8) )
        return 0;
    }
    SelectedItemIndex = SC_SceneComposition::GetSelectedItemIndex(sceneComp, v8, *(unsigned __int16 *)(v9 + 36));
    if ( SelectedItemIndex < 0 )
      return 0xFFFFFFFFLL;
    v11 = *((_QWORD *)this + 10);
    if ( !*(_BYTE *)(v9 + 1) )
    {
      *(_DWORD *)(v11 + 4LL * SelectedItemIndex) = 0;
      v6 = *((_QWORD *)sceneComp + 16);
      v7 = *((_QWORD *)sceneComp + 17);
      goto LABEL_6;
    }
    ++v8;
    *(_DWORD *)(v11 + 4LL * SelectedItemIndex) = *(_DWORD *)(v9 + 4);
    v6 = *((_QWORD *)sceneComp + 16);
    v7 = *((_QWORD *)sceneComp + 17);
    if ( v8 >= (unsigned int)((v7 - v6) >> 8) )
      return 0;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x0009f540  size 1984 bytes  492 insns
 *   SceneCompositionSettings::SetFromPreset(unsigned short, SC_SceneComposition const&, unsigned short)
 *   [_ZN24SceneCompositionSettings13SetFromPresetEtRK19SC_SceneCompositiont]
 *   data refs: 0x3304e0, 0x330508, 0x42ce00, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall SceneCompositionSettings::SetFromPreset(
        SceneCompositionSettings *this,
        unsigned __int16 v,
        const SC_SceneComposition *sceneComp,
        unsigned __int16 v2)
{
  __int64 v4; // d8
  __int64 v5; // d9
  __int64 v6; // d10
  unsigned int v8; // w24
  __int64 v10; // x20
  __int64 v11; // x22
  unsigned __int64 v12; // x2
  __int64 v13; // x19
  void *v14; // x0
  __int64 v15; // x20
  __int64 v16; // x22
  __int64 v17; // x19
  void *v18; // x0
  void *v19; // x0
  __int64 v20; // x20
  __int64 v21; // x22
  __int64 v22; // x19
  void *v23; // x0
  void *v24; // x0
  __int64 v25; // x22
  __int64 v26; // x4
  unsigned __int64 v27; // x0
  __int64 v28; // x1
  __int64 v29; // x22
  __int64 v30; // x19
  void *v31; // x0
  void *v32; // x0
  unsigned int v33; // w22
  unsigned __int64 v34; // x20
  unsigned __int8 *v35; // x2
  int v36; // w0
  int v37; // w7
  int v38; // w11
  int v39; // w10
  int v40; // w9
  unsigned __int8 v41; // w24
  int v42; // w23
  unsigned int v43; // w25
  unsigned __int8 v44; // w19
  __int16 v45; // w27
  float v46; // s10
  int v47; // s0
  int v48; // s9
  int v49; // s0
  int v50; // s8
  float v51; // s0
  __int64 v52; // x2
  char v53; // w7
  char v54; // w8
  __int64 v55; // x0
  __int64 v56; // x1
  __int64 v57; // x24
  __int64 v58; // x4
  __int64 v59; // x1
  __int64 v60; // x6
  __int64 v61; // x19
  __int64 result; // x0
  unsigned __int16 *v63; // x20
  unsigned __int16 *v64; // x27
  unsigned __int16 v65; // w4
  __int64 v66; // x7
  unsigned int v67; // w2
  __int64 v68; // x22
  __int64 v69; // x23
  unsigned __int64 v70; // x0
  unsigned __int64 v71; // x1
  unsigned int v72; // w19
  __int64 v73; // x22
  __int64 v74; // x23
  __int64 v75; // x20
  __int64 v76; // x3
  __int64 v77; // x0
  __int64 v78; // x2
  __int64 v79; // x4
  void *v80; // x1
  __int64 v81; // x1
  bool v82; // nf
  __int64 v83; // x3
  __int64 v84; // x2
  __int64 v85; // x1
  __int64 v86; // x1
  __int64 v87; // x6
  __int64 v88; // x3
  unsigned __int64 v89; // x1
  __int64 v90; // x25
  __int64 v91; // x2
  __int64 v92; // x26
  __int64 v93; // x4
  __int64 v94; // x1
  __int64 v95; // x2
  __int64 v96; // x27
  int v97; // w7
  unsigned int v98; // w3
  __int64 v99; // x0
  SceneCompositionSettings::CategorySettings *v100; // x0
  int v101; // w7
  _QWORD *v102; // x4
  __int64 v103; // x23
  __int64 v104; // x20
  void *v105; // x0
  __int64 v106; // [xsp+60h] [xbp+60h]
  __int64 v107; // [xsp+68h] [xbp+68h]
  __int64 v108; // [xsp+70h] [xbp+70h]
  int v109; // [xsp+80h] [xbp+80h]
  int v110; // [xsp+84h] [xbp+84h]
  int v111; // [xsp+88h] [xbp+88h]
  int v112; // [xsp+8Ch] [xbp+8Ch]
  int v113; // [xsp+90h] [xbp+90h]
  int v114; // [xsp+94h] [xbp+94h]
  __int64 v115; // [xsp+98h] [xbp+98h]
  unsigned __int16 v116; // [xsp+A8h] [xbp+A8h] BYREF
  __int64 v117; // [xsp+B0h] [xbp+B0h]
  void *v118; // [xsp+B8h] [xbp+B8h]
  __int64 v119; // [xsp+C0h] [xbp+C0h]
  __int64 v120; // [xsp+C8h] [xbp+C8h]
  __int64 v121; // [xsp+D0h] [xbp+D0h]
  v8 = v2;
  v11 = *((_QWORD *)this + 1);
  v10 = *((_QWORD *)this + 2);
  v12 = 0;
  *(_BYTE *)this = 0;
  *((_WORD *)this + 2) = v;
  if ( v10 != v11 )
  {
    v13 = v11;
    do
    {
      v14 = *(void **)(v13 + 24);
      if ( v14 )
        operator delete(v14);
      if ( *(_QWORD *)v13 )
        operator delete(*(void **)v13);
      v13 += 64;
    }
    while ( v10 != v13 );
    *((_QWORD *)this + 2) = v11;
  }
  v16 = *((_QWORD *)this + 4);
  v15 = *((_QWORD *)this + 5);
  if ( v15 != v16 )
  {
    v17 = *((_QWORD *)this + 4);
    do
    {
      v18 = *(void **)(v17 + 64);
      if ( v18 )
        operator delete(v18);
      v19 = *(void **)(v17 + 40);
      if ( v19 )
        operator delete(v19);
      v17 += 88;
    }
    while ( v17 != v15 );
    *((_QWORD *)this + 5) = v16;
  }
  if ( !*((_BYTE *)sceneComp + 304) )
    goto LABEL_18;
  v69 = *((_QWORD *)this + 1);
  v68 = *((_QWORD *)this + 2);
  v70 = (v68 - v69) >> 6;
  v71 = (unsigned __int16)(-29789 * ((__int64)(*((_QWORD *)sceneComp + 40) - *((_QWORD *)sceneComp + 39)) >> 4));
  v72 = (unsigned __int16)(-29789 * ((__int64)(*((_QWORD *)sceneComp + 40) - *((_QWORD *)sceneComp + 39)) >> 4));
  if ( v71 > v70 )
  {
    std::vector<SceneCompositionSettings::CategorySettings>::_M_default_append((char *)this + 8, v71 - v70, v12);
  }
  else if ( v71 < v70 )
  {
    v103 = v69 + (v71 << 6);
    if ( v68 != v103 )
    {
      v104 = v103;
      do
      {
        v105 = *(void **)(v104 + 24);
        if ( v105 )
          operator delete(v105);
        if ( *(_QWORD *)v104 )
          operator delete(*(void **)v104);
        v104 += 64;
      }
      while ( v68 != v104 );
      *((_QWORD *)this + 2) = v103;
    }
  }
  if ( !v72 )
    goto LABEL_18;
  v73 = 0;
  v74 = 0;
  v75 = 176LL * (v72 - 1) + 176;
  do
  {
    while ( 1 )
    {
      v77 = *((_QWORD *)this + 1) + v74;
      v87 = *(_QWORD *)(v77 + 8);
      v88 = *(_QWORD *)(*((_QWORD *)sceneComp + 39) + v73 + 8) - *(_QWORD *)(*((_QWORD *)sceneComp + 39) + v73);
      v89 = (v87 - *(_QWORD *)v77) >> 1;
      v78 = v88 >> 1;
      if ( v88 >> 1 > v89 )
      {
        std::vector<unsigned short>::_M_default_append(v77, v78 - v89);
        goto LABEL_66;
      }
      if ( v88 >> 1 < v89 )
      {
        v76 = *(_QWORD *)v77 + v88;
        if ( v87 != v76 )
        {
          *(_QWORD *)(v77 + 8) = v76;
LABEL_66:
          v77 = *((_QWORD *)this + 1) + v74;
          v78 = (__int64)(*(_QWORD *)(*((_QWORD *)sceneComp + 39) + v73 + 8)
                        - *(_QWORD *)(*((_QWORD *)sceneComp + 39) + v73)) >> 1;
        }
      }
      v79 = *(_QWORD *)(v77 + 24);
      v80 = *(void **)(v77 + 40);
      if ( (unsigned __int64)*(unsigned int *)(v77 + 48) + 8 * ((__int64)v80 - v79) > v78 )
        break;
      LODWORD(v119) = *(_DWORD *)(v77 + 48);
      v73 += 176;
      v118 = v80;
      v74 += 64;
      std::vector<bool>::_M_fill_insert(v77 + 24, v80);
      if ( v75 == v73 )
        goto LABEL_78;
    }
    v81 = v78 + 63;
    if ( v78 >= 0 )
      v81 = v78;
    v82 = -v78 < 0;
    v83 = -v78 & 0x3F;
    v84 = v78 & 0x3F;
    v85 = v81 >> 6;
    if ( !v82 )
      v84 = -v83;
    v86 = v79 + 8 * v85;
    if ( v84 < 0 )
    {
      LODWORD(v84) = v84 + 64;
      v86 -= 8;
    }
    *(_QWORD *)(v77 + 40) = v86;
    v73 += 176;
    *(_DWORD *)(v77 + 48) = v84;
    v74 += 64;
  }
  while ( v75 != v73 );
LABEL_78:
  v90 = 0;
  v91 = *((_QWORD *)sceneComp + 39);
  v92 = 0;
  v93 = v91;
  v94 = *(_QWORD *)(v91 + 32);
  if ( (unsigned __int16)v8 >= (unsigned __int64)((*(_QWORD *)(v91 + 40) - v94) >> 1) )
  {
LABEL_86:
    __printf_chk(1, "ERROR: preset index out of range\n");
    result = 0xFFFFFFFFLL;
    goto LABEL_45;
  }
  while ( 1 )
  {
    v95 = *(_QWORD *)(v91 + v92);
    v96 = v90 << 6;
    v97 = *(unsigned __int16 *)(v94 + 2LL * (unsigned __int16)v8);
    if ( (unsigned int)((*(_QWORD *)(v93 + 8) - v95) >> 1) )
    {
      v98 = 0;
      do
      {
        v99 = 2LL * v98++;
        *(_WORD *)(*(_QWORD *)(*((_QWORD *)this + 1) + (v90 << 6)) + v99) = *(_WORD *)(v95 + v99);
        v93 = *((_QWORD *)sceneComp + 39) + v92;
        v95 = *(_QWORD *)v93;
      }
      while ( v98 < (unsigned int)((__int64)(*(_QWORD *)(v93 + 8) - *(_QWORD *)v93) >> 1) );
    }
    v100 = (SceneCompositionSettings::CategorySettings *)(*((_QWORD *)this + 1) + v96);
    if ( *(_BYTE *)(v93 + 24) )
    {
      v109 = v97;
      SceneCompositionSettings::CategorySettings::SetAllOff(v100);
      v12 = *((_QWORD *)sceneComp + 39);
      *(_QWORD *)&v2 = 0;
      v101 = v109;
      if ( (unsigned int)((__int64)(*(_QWORD *)(v12 + v92 + 8) - *(_QWORD *)(v12 + v92)) >> 1) )
      {
        do
        {
          v102 = (_QWORD *)(*((_QWORD *)this + 1) + v96);
          if ( *(unsigned __int16 *)(*v102 + 2LL * v2) == v101 )
          {
            *(_QWORD *)(v102[3] + 8 * ((unsigned __int64)v2 >> 6)) |= 1LL << v2;
            v12 = *((_QWORD *)sceneComp + 39);
          }
          *(_QWORD *)&v2 = (unsigned int)(v2 + 1);
        }
        while ( v2 < (unsigned int)((__int64)(*(_QWORD *)(v12 + v92 + 8) - *(_QWORD *)(v12 + v92)) >> 1) );
      }
    }
    else
    {
      SceneCompositionSettings::CategorySettings::SetAllOn(v100);
    }
    if ( v72 <= (unsigned int)++v90 )
      break;
    v91 = *((_QWORD *)sceneComp + 39);
    v92 = 176 * v90;
    v93 = v91 + 176 * v90;
    v94 = *(_QWORD *)(v93 + 32);
    if ( (unsigned __int16)v8 >= (unsigned __int64)((*(_QWORD *)(v93 + 40) - v94) >> 1) )
      goto LABEL_86;
  }
LABEL_18:
  v20 = *((_QWORD *)this + 4);
  v21 = *((_QWORD *)this + 5);
  v22 = v20;
  if ( v20 == v21 )
  {
    v28 = (__int64)(*((_QWORD *)sceneComp + 17) - *((_QWORD *)sceneComp + 16)) >> 8;
    if ( !v28 )
      goto LABEL_44;
    v27 = 0;
LABEL_49:
    std::vector<SceneCompositionSettings::SelectionGroupSettings>::_M_default_append(
      (char *)this + 32,
      v28 - v27,
      v12,
      v2);
    v26 = *((_QWORD *)sceneComp + 16);
    *(_QWORD *)&v2 = (*((_QWORD *)sceneComp + 17) - v26) >> 8;
    goto LABEL_34;
  }
  do
  {
    v23 = *(void **)(v22 + 64);
    if ( v23 )
      operator delete(v23);
    v24 = *(void **)(v22 + 40);
    if ( v24 )
      operator delete(v24);
    v22 += 88;
  }
  while ( v21 != v22 );
  *((_QWORD *)this + 5) = v20;
  v25 = *((_QWORD *)this + 4);
  v26 = *((_QWORD *)sceneComp + 16);
  v27 = 0x2E8BA2E8BA2E8BA3LL * ((v20 - v25) >> 3);
  v12 = (*((_QWORD *)sceneComp + 17) - v26) >> 8;
  *(_QWORD *)&v2 = v12;
  v28 = v12;
  if ( v12 > v27 )
    goto LABEL_49;
  if ( v12 < v27 )
  {
    v29 = v25 + 88 * v12;
    if ( v20 != v29 )
    {
      v30 = v29;
      do
      {
        v31 = *(void **)(v30 + 64);
        if ( v31 )
          operator delete(v31);
        v32 = *(void **)(v30 + 40);
        if ( v32 )
          operator delete(v32);
        v30 += 88;
      }
      while ( v20 != v30 );
      *((_QWORD *)this + 5) = v29;
      v26 = *((_QWORD *)sceneComp + 16);
      *(_QWORD *)&v2 = (*((_QWORD *)sceneComp + 17) - v26) >> 8;
    }
  }
LABEL_34:
  if ( !v2 )
  {
LABEL_44:
    result = SceneCompositionSettings::SetSceneComponentModifiers(this);
    if ( !(_DWORD)result )
    {
      result = SceneCompositionSettings::SetEffectiveInteractivityGains(this, sceneComp);
      if ( !(_DWORD)result )
        *(_BYTE *)this = 1;
    }
    goto LABEL_45;
  }
  v33 = 0;
  v106 = v4;
  v107 = v5;
  v115 = 48LL * v8;
  v108 = v6;
  while ( 1 )
  {
    v34 = (unsigned __int64)v33 << 8;
    v35 = (unsigned __int8 *)(*(_QWORD *)(v26 + v34 + 56) + v115);
    v36 = *v35;
    v37 = v35[34];
    v38 = *((unsigned __int16 *)v35 + 18);
    v39 = *((unsigned __int16 *)v35 + 20);
    v40 = *((unsigned __int16 *)v35 + 22);
    v113 = v35[30];
    v114 = v36;
    LOWORD(v36) = *((_WORD *)v35 + 16);
    v41 = v35[42];
    v42 = v35[27];
    v43 = *((unsigned __int16 *)v35 + 14);
    v44 = v35[38];
    v45 = *((_WORD *)v35 + 23);
    v109 = v40;
    v110 = v39;
    v111 = v38;
    v112 = v37;
    v46 = apac::scene::DecodeInteractivityGain(v36, 1);
    apac::scene::DecodeAngleOffset(v111, 6u);
    v48 = v47;
    apac::scene::DecodeAngleOffset(v110, 6u);
    v50 = v49;
    v51 = apac::scene::DecodeDistanceRatio(v109, 4u);
    v52 = 88LL * v33;
    v53 = v112;
    v54 = v113;
    v55 = *((_QWORD *)this + 4) + v52;
    *(_BYTE *)v55 = v42;
    *(_BYTE *)(v55 + 1) = v54;
    *(_BYTE *)(v55 + 8) = v53;
    *(_BYTE *)(v55 + 16) = v44;
    *(_BYTE *)(v55 + 24) = v41;
    *(_WORD *)(v55 + 32) = v45;
    *(float *)(v55 + 4) = v46;
    *(_DWORD *)(v55 + 12) = v48;
    *(_DWORD *)(v55 + 20) = v50;
    *(float *)(v55 + 28) = v51;
    *(_WORD *)(*((_QWORD *)this + 4) + v52 + 36) = v43;
    v56 = *((_QWORD *)this + 4) + v52;
    v57 = v56 + 40;
    v58 = *(_QWORD *)(v56 + 40);
    if ( v58 != *(_QWORD *)(v56 + 48) )
      *(_QWORD *)(v56 + 48) = v58;
    if ( v42 || v114 )
      break;
    v26 = *((_QWORD *)sceneComp + 16);
    v43 = 0;
    v59 = v26 + v34;
    v60 = *(_QWORD *)(v26 + v34 + 8);
    v61 = (*(_QWORD *)(v26 + v34 + 16) - v60) >> 1;
    if ( !(_DWORD)v61 )
      goto LABEL_51;
LABEL_41:
    std::vector<unsigned short>::operator=(
      *((_QWORD *)this + 4) + v52 + 64,
      *((_QWORD *)sceneComp + 7) + 144LL * *(unsigned __int16 *)(v60 + 2LL * (unsigned __int16)v43));
    v26 = *((_QWORD *)sceneComp + 16);
LABEL_42:
    if ( ++v33 >= (unsigned int)((*((_QWORD *)sceneComp + 17) - v26) >> 8) )
    {
      v4 = v106;
      v5 = v107;
      v6 = v108;
      goto LABEL_44;
    }
  }
  v26 = *((_QWORD *)sceneComp + 16);
  v59 = v26 + v34;
  v60 = *(_QWORD *)(v26 + v34 + 8);
  v61 = (*(_QWORD *)(v26 + v34 + 16) - v60) >> 1;
  if ( v43 < (unsigned int)v61 )
    goto LABEL_41;
LABEL_51:
  v63 = *(unsigned __int16 **)(v59 + 32);
  v64 = *(unsigned __int16 **)(v59 + 40);
  LODWORD(v61) = (unsigned __int16)v61;
  if ( v63 == v64 )
    goto LABEL_42;
  while ( 1 )
  {
    while ( v43 != (_DWORD)v61 )
    {
      ++v63;
      LODWORD(v61) = (unsigned __int16)(v61 + 1);
      if ( v64 == v63 )
        goto LABEL_57;
    }
    v65 = *v63;
    v66 = *((_QWORD *)sceneComp + 10);
    v67 = **(unsigned __int16 **)(*((_QWORD *)sceneComp + 13) + 144LL * *v63);
    if ( v67 >= (unsigned __int16)(-13107 * ((*((_QWORD *)sceneComp + 11) - v66) >> 4)) )
      break;
    ++v63;
    v116 = v65;
    LODWORD(v61) = (unsigned __int16)(v61 + 1);
    v117 = v66 + 80LL * v67;
    std::vector<SceneCompositionSettings::SelectionSetSettings>::emplace_back<SceneCompositionSettings::SelectionSetSettings>(
      v57,
      &v116);
    if ( v64 == v63 )
    {
LABEL_57:
      v26 = *((_QWORD *)sceneComp + 16);
      goto LABEL_42;
    }
  }
  __printf_chk(1, "ERROR: item index out of range\n");
  result = 0xFFFFFFFFLL;
  v6 = v108;
  v4 = v106;
  v5 = v107;
LABEL_45:
  if ( v120 != _stack_chk_guard )
  {
    v106 = v4;
    v107 = v5;
    v108 = v6;
  }
  return result;
}


#if 0 /* disassembly of 0x0009f540, for verification */
0009F540  e91340d1  SUB             X9, SP, #4,LSL#12
0009F544  3f9907f9  STR             XZR, [X9,#0xF30]
0009F548  fd7bb3a9  STP             X29, X30, [SP,#var_D0]!
0009F54C  fd030091  MOV             X29, SP
0009F550  f55b02a9  STP             X21, X22, [SP,#0xD0+var_B0]
0009F554  f50300aa  MOV             X21, X0
0009F558  601c00b0  ADRP            X0, #__stack_chk_guard_ptr@PAGE
0009F55C  f35301a9  STP             X19, X20, [SP,#0xD0+var_C0]
0009F560  f76303a9  STP             X23, X24, [SP,#0xD0+var_A0]
0009F564  783c0012  AND             W24, W3, #0xFFFF
0009F568  f96b04a9  STP             X25, X26, [SP,#0xD0+var_90]
0009F56C  fb7305a9  STP             X27, X28, [SP,#0xD0+var_80]
0009F570  fc0302aa  MOV             X28, X2
0009F574  000047f9  LDR             X0, [X0,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009F578  b6d240a9  LDP             X22, X20, [X21,#8]
0009F57C  020040f9  LDR             X2, [X0]
0009F580  e26700f9  STR             X2, [SP,#0xD0+var_8]
0009F584  020080d2  MOV             X2, #0
0009F588  bf020039  STRB            WZR, [X21]
0009F58C  a10a0079  STRH            W1, [X21,#4]
0009F590  9f0216eb  CMP             X20, X22
0009F594  a0010054  B.EQ            loc_9F5C8
0009F598  f30316aa  MOV             X19, X22
0009F59C  1f2003d5  NOP
0009F5A0  600e40f9  LDR             X0, [X19,#0x18]; void *
0009F5A4  400000b4  CBZ             X0, loc_9F5AC
0009F5A8  be42ff97  BL              ._ZdlPv; operator delete(void *)
0009F5AC  600240f9  LDR             X0, [X19]; void *
0009F5B0  400000b4  CBZ             X0, loc_9F5B8
0009F5B4  bb42ff97  BL              ._ZdlPv; operator delete(void *)
0009F5B8  73020191  ADD             X19, X19, #0x40 ; '@'
0009F5BC  9f0213eb  CMP             X20, X19
0009F5C0  01ffff54  B.NE            loc_9F5A0
0009F5C4  b60a00f9  STR             X22, [X21,#0x10]
0009F5C8  b65242a9  LDP             X22, X20, [X21,#0x20]
0009F5CC  9f0216eb  CMP             X20, X22
0009F5D0  80010054  B.EQ            loc_9F600
0009F5D4  f30316aa  MOV             X19, X22
0009F5D8  602240f9  LDR             X0, [X19,#0x40]; void *
0009F5DC  400000b4  CBZ             X0, loc_9F5E4
0009F5E0  b042ff97  BL              ._ZdlPv; operator delete(void *)
0009F5E4  601640f9  LDR             X0, [X19,#0x28]; void *
0009F5E8  400000b4  CBZ             X0, loc_9F5F0
0009F5EC  ad42ff97  BL              ._ZdlPv; operator delete(void *)
0009F5F0  73620191  ADD             X19, X19, #0x58 ; 'X'
0009F5F4  7f0214eb  CMP             X19, X20
0009F5F8  01ffff54  B.NE            loc_9F5D8
0009F5FC  b61600f9  STR             X22, [X21,#0x28]
0009F600  80c34439  LDRB            W0, [X28,#0x130]
0009F604  a01d0035  CBNZ            W0, loc_9F9B8
0009F608  b45a42a9  LDP             X20, X22, [X21,#0x20]
0009F60C  f30314aa  MOV             X19, X20
0009F610  9f0216eb  CMP             X20, X22
0009F614  80140054  B.EQ            loc_9F8A4
0009F618  602240f9  LDR             X0, [X19,#0x40]; void *
0009F61C  400000b4  CBZ             X0, loc_9F624
0009F620  a042ff97  BL              ._ZdlPv; operator delete(void *)
0009F624  601640f9  LDR             X0, [X19,#0x28]; void *
0009F628  400000b4  CBZ             X0, loc_9F630
0009F62C  9d42ff97  BL              ._ZdlPv; operator delete(void *)
0009F630  73620191  ADD             X19, X19, #0x58 ; 'X'
0009F634  df0213eb  CMP             X22, X19
0009F638  01ffff54  B.NE            loc_9F618
0009F63C  b41600f9  STR             X20, [X21,#0x28]
0009F640  617491d2  MOV             X1, #0xBA2E8BA3
0009F648  b61240f9  LDR             X22, [X21,#0x20]
0009F64C  015dd4f2  MOVK            X1, #0xA2E8,LSL#32
0009F650  61d1e5f2  MOVK            X1, #0x2E8B,LSL#48
0009F654  840b48a9  LDP             X4, X2, [X28,#0x80]
0009F658  800216cb  SUB             X0, X20, X22
0009F65C  00fc4393  ASR             X0, X0, #3
0009F660  420004cb  SUB             X2, X2, X4
0009F664  007c019b  MUL             X0, X0, X1
0009F668  42fc4893  ASR             X2, X2, #8
0009F66C  e30302aa  MOV             X3, X2
0009F670  e10302aa  MOV             X1, X2
0009F674  5f0000eb  CMP             X2, X0
0009F678  28120054  B.HI            loc_9F8BC
0009F67C  82020054  B.CS            loc_9F6CC
0009F680  000b80d2  MOV             X0, #0x58 ; 'X'
0009F684  5658009b  MADD            X22, X2, X0, X22
0009F688  9f0216eb  CMP             X20, X22
0009F68C  00020054  B.EQ            loc_9F6CC
0009F690  f30316aa  MOV             X19, X22
0009F694  1f2003d5  NOP
0009F698  602240f9  LDR             X0, [X19,#0x40]; void *
0009F69C  400000b4  CBZ             X0, loc_9F6A4
0009F6A0  8042ff97  BL              ._ZdlPv; operator delete(void *)
0009F6A4  601640f9  LDR             X0, [X19,#0x28]; void *
0009F6A8  400000b4  CBZ             X0, loc_9F6B0
0009F6AC  7d42ff97  BL              ._ZdlPv; operator delete(void *)
0009F6B0  73620191  ADD             X19, X19, #0x58 ; 'X'
0009F6B4  9f0213eb  CMP             X20, X19
0009F6B8  01ffff54  B.NE            loc_9F698
0009F6BC  b61600f9  STR             X22, [X21,#0x28]
0009F6C0  840f48a9  LDP             X4, X3, [X28,#0x80]
0009F6C4  630004cb  SUB             X3, X3, X4
0009F6C8  63fc4893  ASR             X3, X3, #8
0009F6CC  a30c0034  CBZ             W3, loc_9F860
0009F6D0  00068052  MOV             W0, #0x30 ; '0'
0009F6D4  faa30291  ADD             X26, SP, #0xD0+var_28
0009F6D8  16008052  MOV             W22, #0
0009F6DC  e827066d  STP             D8, D9, [SP,#0xD0+var_70]
0009F6E0  007fa09b  UMULL           X0, W24, W0
0009F6E4  e04f00f9  STR             X0, [SP,#0xD0+var_38]
0009F6E8  ea3b00fd  STR             D10, [SP,#0xD0+var_60]
0009F6EC  1f2003d5  NOP
0009F6F0  d47e78d3  UBFIZ           X20, X22, #8, #0x20 ; ' '
0009F6F4  21008052  MOV             W1, #1; unsigned __int16
0009F6F8  8400148b  ADD             X4, X4, X20
0009F6FC  e34f40f9  LDR             X3, [SP,#0xD0+var_38]
0009F700  801c40f9  LDR             X0, [X4,#0x38]
0009F704  0200038b  ADD             X2, X0, X3; bool
0009F708  00686338  LDRB            W0, [X0,X3]
0009F70C  48784039  LDRB            W8, [X2,#0x1E]
0009F710  47884039  LDRB            W7, [X2,#0x22]
0009F714  4b484079  LDRH            W11, [X2,#0x24]
0009F718  4a504079  LDRH            W10, [X2,#0x28]
0009F71C  49584079  LDRH            W9, [X2,#0x2C]
0009F720  e8031229  STP             W8, W0, [SP,#0xD0+var_40]
0009F724  40404079  LDRH            W0, [X2,#0x20]; this
0009F728  58a84039  LDRB            W24, [X2,#0x2A]
0009F72C  576c4039  LDRB            W23, [X2,#0x1B]
0009F730  59384079  LDRH            W25, [X2,#0x1C]
0009F734  53984039  LDRB            W19, [X2,#0x26]
0009F738  5b5c4079  LDRH            W27, [X2,#0x2E]
0009F73C  e92b1029  STP             W9, W10, [SP,#0xD0+var_50]
0009F740  eb1f1129  STP             W11, W7, [SP,#0x88]
0009F744  3b110094  BL              _ZN4apac5scene23DecodeInteractivityGainEtb; apac::scene::DecodeInteractivityGain(ushort,bool)
0009F748  eb8b40b9  LDR             W11, [SP,#0x88]
0009F74C  0a40201e  FMOV            S10, S0
0009F750  c1008052  MOV             W1, #6; unsigned __int16
0009F754  e0030b2a  MOV             W0, W11; this
0009F758  f2100094  BL              _ZN4apac5scene17DecodeAngleOffsetEtt; apac::scene::DecodeAngleOffset(ushort,ushort)
0009F75C  0940201e  FMOV            S9, S0
0009F760  ea8740b9  LDR             W10, [SP,#0xD0+var_50+4]
0009F764  c1008052  MOV             W1, #6; unsigned __int16
0009F768  e0030a2a  MOV             W0, W10; this
0009F76C  ed100094  BL              _ZN4apac5scene17DecodeAngleOffsetEtt; apac::scene::DecodeAngleOffset(ushort,ushort)
0009F770  0840201e  FMOV            S8, S0
0009F774  e98340b9  LDR             W9, [SP,#0xD0+var_50]
0009F778  81008052  MOV             W1, #4; unsigned __int16
0009F77C  e003092a  MOV             W0, W9; this
0009F780  08110094  BL              _ZN4apac5scene19DecodeDistanceRatioEtt; apac::scene::DecodeDistanceRatio(ushort,ushort)
0009F784  020b8052  MOV             W2, #0x58 ; 'X'
0009F788  a11240f9  LDR             X1, [X21,#0x20]
0009F78C  c27ea29b  UMULL           X2, W22, W2
0009F790  e7a35129  LDP             W7, W8, [SP,#0x8C]
0009F794  2000028b  ADD             X0, X1, X2
0009F798  37682238  STRB            W23, [X1,X2]
0009F79C  08040039  STRB            W8, [X0,#1]
0009F7A0  07200039  STRB            W7, [X0,#8]
0009F7A4  13400039  STRB            W19, [X0,#0x10]
0009F7A8  18600039  STRB            W24, [X0,#0x18]
0009F7AC  1b400079  STRH            W27, [X0,#0x20]
0009F7B0  0a0400bd  STR             S10, [X0,#4]
0009F7B4  090c00bd  STR             S9, [X0,#0xC]
0009F7B8  081400bd  STR             S8, [X0,#0x14]
0009F7BC  001c00bd  STR             S0, [X0,#0x1C]
0009F7C0  a01240f9  LDR             X0, [X21,#0x20]
0009F7C4  0000028b  ADD             X0, X0, X2
0009F7C8  19480079  STRH            W25, [X0,#0x24]
0009F7CC  a11240f9  LDR             X1, [X21,#0x20]
0009F7D0  2100028b  ADD             X1, X1, X2
0009F7D4  38a00091  ADD             X24, X1, #0x28 ; '('
0009F7D8  249842a9  LDP             X4, X6, [X1,#0x28]
0009F7DC  9f0006eb  CMP             X4, X6
0009F7E0  40000054  B.EQ            loc_9F7E8
0009F7E4  241800f9  STR             X4, [X1,#0x30]
0009F7E8  97070035  CBNZ            W23, loc_9F8D8
0009F7EC  e09740b9  LDR             W0, [SP,#0xD0+var_3C]
0009F7F0  40070035  CBNZ            W0, loc_9F8D8
0009F7F4  844340f9  LDR             X4, [X28,#0x80]
0009F7F8  07008052  MOV             W7, #0
0009F7FC  19008052  MOV             W25, #0
0009F800  8100148b  ADD             X1, X4, X20
0009F804  26cc40a9  LDP             X6, X19, [X1,#8]
0009F808  730206cb  SUB             X19, X19, X6
0009F80C  73fe4193  ASR             X19, X19, #1
0009F810  ff00136b  CMP             W7, W19
0009F814  22070054  B.CS            loc_9F8F8
0009F818  233f7fd3  UBFIZ           X3, X25, #1, #0x10
0009F81C  01128052  MOV             W1, #0x90
0009F820  841f40f9  LDR             X4, [X28,#0x38]
0009F824  c3686378  LDRH            W3, [X6,X3]
0009F828  a01240f9  LDR             X0, [X21,#0x20]
0009F82C  6110a19b  UMADDL          X1, W3, W1, X4
0009F830  0000028b  ADD             X0, X0, X2
0009F834  00000191  ADD             X0, X0, #0x40 ; '@'
0009F838  4a8eff97  BL              _ZNSt6vectorItSaItEEaSERKS1_; std::vector<ushort>::operator=(std::vector<ushort> const&)
0009F83C  844340f9  LDR             X4, [X28,#0x80]
0009F840  d6060011  ADD             W22, W22, #1
0009F844  804740f9  LDR             X0, [X28,#0x88]
0009F848  000004cb  SUB             X0, X0, X4
0009F84C  00fc4893  ASR             X0, X0, #8
0009F850  df02006b  CMP             W22, W0
0009F854  e3f4ff54  B.CC            loc_9F6F0
0009F858  e827466d  LDP             D8, D9, [SP,#0xD0+var_70]
0009F85C  ea3b40fd  LDR             D10, [SP,#0xD0+var_60]
0009F860  e00315aa  MOV             X0, X21; this
0009F864  fffcff97  BL              _ZN24SceneCompositionSettings26SetSceneComponentModifiersEv; SceneCompositionSettings::SetSceneComponentModifiers(void)
0009F868  00210034  CBZ             W0, loc_9FC88
0009F86C  1f2003d5  NOP
0009F870  611c00b0  ADRP            X1, #__stack_chk_guard_ptr@PAGE
0009F874  350047f9  LDR             X21, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009F878  e26740f9  LDR             X2, [SP,#0xD0+var_8]
0009F87C  a10240f9  LDR             X1, [X21]
0009F880  410001ca  EOR             X1, X2, X1
0009F884  812300b5  CBNZ            X1, loc_9FCF4
0009F888  f35341a9  LDP             X19, X20, [SP,#0xD0+var_C0]
0009F88C  f55b42a9  LDP             X21, X22, [SP,#0xD0+var_B0]
0009F890  f76343a9  LDP             X23, X24, [SP,#0xD0+var_A0]
0009F894  f96b44a9  LDP             X25, X26, [SP,#0xD0+var_90]
0009F898  fb7345a9  LDP             X27, X28, [SP,#0xD0+var_80]
0009F89C  fd7bcda8  LDP             X29, X30, [SP+0xD0+var_D0],#0xD0
0009F8A0  c0035fd6  RET
0009F8A4  840748a9  LDP             X4, X1, [X28,#0x80]
0009F8A8  210004cb  SUB             X1, X1, X4
0009F8AC  ff2381eb  CMP             XZR, X1,ASR#8
0009F8B0  21fc4893  ASR             X1, X1, #8
0009F8B4  60fdff54  B.EQ            loc_9F860
0009F8B8  000080d2  MOV             X0, #0
0009F8BC  210000cb  SUB             X1, X1, X0
0009F8C0  a0820091  ADD             X0, X21, #0x20 ; ' '
0009F8C4  db080094  BL              _ZNSt6vectorIN24SceneCompositionSettings22SelectionGroupSettingsESaIS1_EE17_M_default_appendEm; std::vector<SceneCompositionSettings::SelectionGroupSettings>::_M_default_append(ulong)
0009F8C8  840f48a9  LDP             X4, X3, [X28,#0x80]
0009F8CC  630004cb  SUB             X3, X3, X4
0009F8D0  63fc4893  ASR             X3, X3, #8
0009F8D4  7effff17  B               loc_9F6CC
0009F8D8  844340f9  LDR             X4, [X28,#0x80]
0009F8DC  e703192a  MOV             W7, W25
0009F8E0  8100148b  ADD             X1, X4, X20
0009F8E4  26cc40a9  LDP             X6, X19, [X1,#8]
0009F8E8  730206cb  SUB             X19, X19, X6
0009F8EC  73fe4193  ASR             X19, X19, #1
0009F8F0  ff00136b  CMP             W7, W19
0009F8F4  23f9ff54  B.CC            loc_9F818
0009F8F8  346c42a9  LDP             X20, X27, [X1,#0x20]
0009F8FC  733e0012  AND             W19, W19, #0xFFFF
0009F900  9f021beb  CMP             X20, X27
0009F904  e0f9ff54  B.EQ            loc_9F840
0009F908  17128052  MOV             W23, #0x90
0009F90C  06000014  B               loc_9F924
0009F910  73060011  ADD             W19, W19, #1
0009F914  940a0091  ADD             X20, X20, #2
0009F918  733e0012  AND             W19, W19, #0xFFFF
0009F91C  7f0314eb  CMP             X27, X20
0009F920  80030054  B.EQ            loc_9F990
0009F924  3f03136b  CMP             W25, W19
0009F928  41ffff54  B.NE            loc_9F910
0009F92C  84024079  LDRH            W4, [X20]
0009F930  e0e702b2  MOV             X0, #0xCCCCCCCCCCCCCCCC
0009F934  870745a9  LDP             X7, X1, [X28,#0x50]
0009F938  a09999f2  MOVK            X0, #0xCCCD
0009F93C  883740f9  LDR             X8, [X28,#0x68]
0009F940  827cb79b  UMULL           X2, W4, W23
0009F944  210007cb  SUB             X1, X1, X7
0009F948  026962f8  LDR             X2, [X8,X2]
0009F94C  21fc4493  ASR             X1, X1, #4
0009F950  42004079  LDRH            W2, [X2]
0009F954  217c009b  MUL             X1, X1, X0
0009F958  5f20216b  CMP             W2, W1,UXTH
0009F95C  e2010054  B.CS            loc_9F998
0009F960  000a8052  MOV             W0, #0x50 ; 'P'
0009F964  e1031aaa  MOV             X1, X26
0009F968  73060011  ADD             W19, W19, #1
0009F96C  940a0091  ADD             X20, X20, #2
0009F970  421ca09b  UMADDL          X2, W2, W0, X7
0009F974  e00318aa  MOV             X0, X24
0009F978  e4530179  STRH            W4, [SP,#0xD0+var_28]
0009F97C  733e0012  AND             W19, W19, #0xFFFF
0009F980  e25b00f9  STR             X2, [SP,#0xD0+var_20]
0009F984  d70d0094  BL              _ZNSt6vectorIN24SceneCompositionSettings20SelectionSetSettingsESaIS1_EE12emplace_backIJS1_EEEvDpOT_; std::vector<SceneCompositionSettings::SelectionSetSettings>::emplace_back<SceneCompositionSettings::SelectionSetSettings>(SceneCompositionSettings::SelectionSetSettings &&)
0009F988  7f0314eb  CMP             X27, X20
0009F98C  c1fcff54  B.NE            loc_9F924
0009F990  844340f9  LDR             X4, [X28,#0x80]
0009F994  abffff17  B               loc_9F840
0009F998  811400b0  ADRL            X1, aErrorItemIndex; "ERROR: item index out of range\n"
0009F9A0  20008052  MOV             W0, #1
0009F9A4  bf43ff97  BL              .__printf_chk
0009F9A8  00008012  MOV             W0, #0xFFFFFFFF
0009F9AC  ea3b40fd  LDR             D10, [SP,#0xD0+var_60]
0009F9B0  e827466d  LDP             D8, D9, [SP,#0xD0+var_70]
0009F9B4  afffff17  B               loc_9F870
0009F9B8  80cf53a9  LDP             X0, X19, [X28,#0x138]
0009F9BC  617491d2  MOV             X1, #0xBA2E8BA3
0009F9C4  b7da40a9  LDP             X23, X22, [X21,#8]
0009F9C8  015dd4f2  MOVK            X1, #0xA2E8,LSL#32
0009F9CC  61d1e5f2  MOVK            X1, #0x2E8B,LSL#48
0009F9D0  730200cb  SUB             X19, X19, X0
0009F9D4  73fe4493  ASR             X19, X19, #4
0009F9D8  c00217cb  SUB             X0, X22, X23
0009F9DC  737e019b  MUL             X19, X19, X1
0009F9E0  00fc4693  ASR             X0, X0, #6
0009F9E4  613e4092  AND             X1, X19, #0xFFFF
0009F9E8  733e0012  AND             W19, W19, #0xFFFF
0009F9EC  3f0000eb  CMP             X1, X0
0009F9F0  a8170054  B.HI            loc_9FCE4
0009F9F4  83150054  B.CC            loc_9FCA4
0009F9F8  93e0ff34  CBZ             W19, loc_9F608
0009F9FC  60060051  SUB             W0, W19, #1
0009FA00  141680d2  MOV             X20, #0xB0
0009FA04  160080d2  MOV             X22, #0
0009FA08  170080d2  MOV             X23, #0
0009FA0C  1450149b  MADD            X20, X0, X20, X20
0009FA10  28000014  B               loc_9FAB0
0009FA14  a2010054  B.CS            loc_9FA48
0009FA18  8300038b  ADD             X3, X4, X3
0009FA1C  df0003eb  CMP             X6, X3
0009FA20  40010054  B.EQ            loc_9FA48
0009FA24  030400f9  STR             X3, [X0,#8]
0009FA28  819f40f9  LDR             X1, [X28,#0x138]
0009FA2C  a00640f9  LDR             X0, [X21,#8]
0009FA30  2200168b  ADD             X2, X1, X22
0009FA34  216876f8  LDR             X1, [X1,X22]
0009FA38  0000178b  ADD             X0, X0, X23
0009FA3C  420440f9  LDR             X2, [X2,#8]
0009FA40  420001cb  SUB             X2, X2, X1
0009FA44  42fc4193  ASR             X2, X2, #1
0009FA48  040c40f9  LDR             X4, [X0,#0x18]
0009FA4C  011440f9  LDR             X1, [X0,#0x28]; s
0009FA50  063040b9  LDR             W6, [X0,#0x30]
0009FA54  230004cb  SUB             X3, X1, X4
0009FA58  e703062a  MOV             W7, W6
0009FA5C  e30c038b  ADD             X3, X7, X3,LSL#3
0009FA60  7f0002eb  CMP             X3, X2
0009FA64  89040054  B.LS            loc_9FAF4
0009FA68  5f0000f1  CMP             X2, #0
0009FA6C  41fc0091  ADD             X1, X2, #0x3F ; '?'
0009FA70  21b0829a  CSEL            X1, X1, X2, LT
0009FA74  e30302eb  NEGS            X3, X2
0009FA78  63144092  AND             X3, X3, #0x3F ; '?'
0009FA7C  42144092  AND             X2, X2, #0x3F ; '?'
0009FA80  21fc4693  ASR             X1, X1, #6
0009FA84  424483da  CSNEG           X2, X2, X3, MI
0009FA88  810c018b  ADD             X1, X4, X1,LSL#3
0009FA8C  6200f8b6  TBZ             X2, #0x3F, loc_9FA98 ; '?'
0009FA90  42000191  ADD             X2, X2, #0x40 ; '@'
0009FA94  212000d1  SUB             X1, X1, #8
0009FA98  011400f9  STR             X1, [X0,#0x28]
0009FA9C  d6c20291  ADD             X22, X22, #0xB0
0009FAA0  023000b9  STR             W2, [X0,#0x30]
0009FAA4  f7020191  ADD             X23, X23, #0x40 ; '@'
0009FAA8  9f0216eb  CMP             X20, X22
0009FAAC  a0030054  B.EQ            loc_9FB20
0009FAB0  a20640f9  LDR             X2, [X21,#8]
0009FAB4  819f40f9  LDR             X1, [X28,#0x138]
0009FAB8  4000178b  ADD             X0, X2, X23
0009FABC  446877f8  LDR             X4, [X2,X23]
0009FAC0  2300168b  ADD             X3, X1, X22
0009FAC4  226876f8  LDR             X2, [X1,X22]
0009FAC8  060440f9  LDR             X6, [X0,#8]
0009FACC  630440f9  LDR             X3, [X3,#8]
0009FAD0  c10004cb  SUB             X1, X6, X4
0009FAD4  630002cb  SUB             X3, X3, X2
0009FAD8  21fc4193  ASR             X1, X1, #1
0009FADC  62fc4193  ASR             X2, X3, #1
0009FAE0  5f0001eb  CMP             X2, X1
0009FAE4  89f9ff54  B.LS            loc_9FA14
0009FAE8  410001cb  SUB             X1, X2, X1
0009FAEC  fd070094  BL              _ZNSt6vectorItSaItEE17_M_default_appendEm; std::vector<ushort>::_M_default_append(ulong)
0009FAF0  ceffff17  B               loc_9FA28
0009FAF4  e6c300b9  STR             W6, [SP,#0xD0+var_10]
0009FAF8  430003cb  SUB             X3, X2, X3
0009FAFC  00600091  ADD             X0, X0, #0x18; int
0009FB00  d6c20291  ADD             X22, X22, #0xB0
0009FB04  e26340f9  LDR             X2, [SP,#0xD0+var_10]
0009FB08  04008052  MOV             W4, #0
0009FB0C  e15f00f9  STR             X1, [SP,#0xD0+var_18]
0009FB10  f7020191  ADD             X23, X23, #0x40 ; '@'
0009FB14  df6aff97  BL              _ZNSt6vectorIbSaIbEE14_M_fill_insertESt13_Bit_iteratormb; std::vector<bool>::_M_fill_insert(std::_Bit_iterator,ulong,bool)
0009FB18  9f0216eb  CMP             X20, X22
0009FB1C  a1fcff54  B.NE            loc_9FAB0
0009FB20  190080d2  MOV             X25, #0
0009FB24  141680d2  MOV             X20, #0xB0
0009FB28  829f40f9  LDR             X2, [X28,#0x138]
0009FB2C  3a7f149b  MUL             X26, X25, X20
0009FB30  163f4092  AND             X22, X24, #0xFFFF
0009FB34  173f7fd3  UBFIZ           X23, X24, #1, #0x10
0009FB38  44001a8b  ADD             X4, X2, X26
0009FB3C  810042a9  LDP             X1, X0, [X4,#0x20]
0009FB40  000001cb  SUB             X0, X0, X1
0009FB44  df0680eb  CMP             X22, X0,ASR#1
0009FB48  e2040054  B.CS            loc_9FBE4
0009FB4C  1f2003d5  NOP
0009FB50  42687af8  LDR             X2, [X2,X26]
0009FB54  3be77ad3  LSL             X27, X25, #6
0009FB58  800440f9  LDR             X0, [X4,#8]
0009FB5C  27687778  LDRH            W7, [X1,X23]
0009FB60  000002cb  SUB             X0, X0, X2
0009FB64  00fc4193  ASR             X0, X0, #1
0009FB68  00020034  CBZ             W0, loc_9FBA8
0009FB6C  03008052  MOV             W3, #0
0009FB70  a10640f9  LDR             X1, [X21,#8]
0009FB74  607c7fd3  UBFIZ           X0, X3, #1, #0x20 ; ' '
0009FB78  63040011  ADD             W3, W3, #1
0009FB7C  21687bf8  LDR             X1, [X1,X27]
0009FB80  42686078  LDRH            W2, [X2,X0]
0009FB84  22682078  STRH            W2, [X1,X0]
0009FB88  809f40f9  LDR             X0, [X28,#0x138]
0009FB8C  04001a8b  ADD             X4, X0, X26
0009FB90  02687af8  LDR             X2, [X0,X26]
0009FB94  810440f9  LDR             X1, [X4,#8]
0009FB98  210002cb  SUB             X1, X1, X2
0009FB9C  21fc4193  ASR             X1, X1, #1
0009FBA0  7f00016b  CMP             W3, W1
0009FBA4  63feff54  B.CC            loc_9FB70
0009FBA8  81604039  LDRB            W1, [X4,#0x18]
0009FBAC  a00640f9  LDR             X0, [X21,#8]
0009FBB0  00001b8b  ADD             X0, X0, X27; this
0009FBB4  41020035  CBNZ            W1, loc_9FBFC
0009FBB8  faf7ff97  BL              _ZN24SceneCompositionSettings16CategorySettings8SetAllOnEv; SceneCompositionSettings::CategorySettings::SetAllOn(void)
0009FBBC  39070091  ADD             X25, X25, #1
0009FBC0  7f02196b  CMP             W19, W25
0009FBC4  29d2ff54  B.LS            loc_9F608
0009FBC8  829f40f9  LDR             X2, [X28,#0x138]
0009FBCC  3a7f149b  MUL             X26, X25, X20
0009FBD0  44001a8b  ADD             X4, X2, X26
0009FBD4  810042a9  LDP             X1, X0, [X4,#0x20]
0009FBD8  000001cb  SUB             X0, X0, X1
0009FBDC  df0680eb  CMP             X22, X0,ASR#1
0009FBE0  83fbff54  B.CC            loc_9FB50
0009FBE4  811400b0  ADRL            X1, aErrorPresetInd; "ERROR: preset index out of range\n"
0009FBEC  20008052  MOV             W0, #1
0009FBF0  2c43ff97  BL              .__printf_chk
0009FBF4  00008012  MOV             W0, #0xFFFFFFFF
0009FBF8  1effff17  B               loc_9F870
0009FBFC  e78300b9  STR             W7, [SP,#0xD0+var_50]
0009FC00  10f8ff97  BL              _ZN24SceneCompositionSettings16CategorySettings9SetAllOffEv; SceneCompositionSettings::CategorySettings::SetAllOff(void)
0009FC04  829f40f9  LDR             X2, [X28,#0x138]
0009FC08  03008052  MOV             W3, #0
0009FC0C  e78340b9  LDR             W7, [SP,#0xD0+var_50]
0009FC10  40001a8b  ADD             X0, X2, X26
0009FC14  41687af8  LDR             X1, [X2,X26]
0009FC18  000440f9  LDR             X0, [X0,#8]
0009FC1C  000001cb  SUB             X0, X0, X1
0009FC20  00fc4193  ASR             X0, X0, #1
0009FC24  c0fcff34  CBZ             W0, loc_9FBBC
0009FC28  a00640f9  LDR             X0, [X21,#8]
0009FC2C  e103032a  MOV             W1, W3
0009FC30  04001b8b  ADD             X4, X0, X27
0009FC34  00687bf8  LDR             X0, [X0,X27]
0009FC38  00586378  LDRH            W0, [X0,W3,UXTW#1]
0009FC3C  1f00076b  CMP             W0, W7
0009FC40  21010054  B.NE            loc_9FC64
0009FC44  840c40f9  LDR             X4, [X4,#0x18]
0009FC48  21fc46d3  LSR             X1, X1, #6
0009FC4C  200080d2  MOV             X0, #1
0009FC50  0020c39a  LSL             X0, X0, X3
0009FC54  827861f8  LDR             X2, [X4,X1,LSL#3]
0009FC58  400000aa  ORR             X0, X2, X0
0009FC5C  807821f8  STR             X0, [X4,X1,LSL#3]
0009FC60  829f40f9  LDR             X2, [X28,#0x138]
0009FC64  41001a8b  ADD             X1, X2, X26
0009FC68  44687af8  LDR             X4, [X2,X26]
0009FC6C  63040011  ADD             W3, W3, #1
0009FC70  200440f9  LDR             X0, [X1,#8]
0009FC74  000004cb  SUB             X0, X0, X4
0009FC78  00fc4193  ASR             X0, X0, #1
0009FC7C  7f00006b  CMP             W3, W0
0009FC80  43fdff54  B.CC            loc_9FC28
0009FC84  ceffff17  B               loc_9FBBC
0009FC88  e1031caa  MOV             X1, X28; SC_SceneComposition *
0009FC8C  e00315aa  MOV             X0, X21; this
0009FC90  a4fbff97  BL              _ZN24SceneCompositionSettings30SetEffectiveInteractivityGainsERK19SC_SceneComposition; SceneCompositionSettings::SetEffectiveInteractivityGains(SC_SceneComposition const&)
0009FC94  e0deff35  CBNZ            W0, loc_9F870
0009FC98  21008052  MOV             W1, #1
0009FC9C  a1020039  STRB            W1, [X21]
0009FCA0  f4feff17  B               loc_9F870
0009FCA4  f71a018b  ADD             X23, X23, X1,LSL#6
0009FCA8  df0217eb  CMP             X22, X23
0009FCAC  60eaff54  B.EQ            loc_9F9F8
0009FCB0  f40317aa  MOV             X20, X23
0009FCB4  1f2003d5  NOP
0009FCB8  800e40f9  LDR             X0, [X20,#0x18]; void *
0009FCBC  400000b4  CBZ             X0, loc_9FCC4
0009FCC0  f840ff97  BL              ._ZdlPv; operator delete(void *)
0009FCC4  800240f9  LDR             X0, [X20]; void *
0009FCC8  400000b4  CBZ             X0, loc_9FCD0
0009FCCC  f540ff97  BL              ._ZdlPv; operator delete(void *)
0009FCD0  94020191  ADD             X20, X20, #0x40 ; '@'
0009FCD4  df0214eb  CMP             X22, X20
0009FCD8  01ffff54  B.NE            loc_9FCB8
0009FCDC  b70a00f9  STR             X23, [X21,#0x10]
0009FCE0  46ffff17  B               loc_9F9F8
0009FCE4  210000cb  SUB             X1, X1, X0
0009FCE8  a0220091  ADD             X0, X21, #8
0009FCEC  fd060094  BL              _ZNSt6vectorIN24SceneCompositionSettings16CategorySettingsESaIS1_EE17_M_default_appendEm; std::vector<SceneCompositionSettings::CategorySettings>::_M_default_append(ulong)
0009FCF0  42ffff17  B               loc_9F9F8
0009FCF4  e827066d  STP             D8, D9, [SP,#0xD0+var_70]
0009FCF8  ea3b00fd  STR             D10, [SP,#0xD0+var_60]
0009FCFC  853fff97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000a00d0  size 3896 bytes  967 insns
 *   SceneCompositionSettings::ApplyUserInput(unsigned int, SC_SceneComposition const&, CD_AudioSceneControlData::SceneCompositionData const&)
 *   [_ZN24SceneCompositionSettings14ApplyUserInputEjRK19SC_SceneCompositionRKN24CD_AudioSceneControlData20SceneCompositionDataE]
 *   data refs: 0x3305a8, 0x3305e0, 0x330610, 0x42ce00, 0x43e520
 */
__int64 __fastcall SceneCompositionSettings::ApplyUserInput(
        SceneCompositionSettings *this,
        unsigned int n,
        const SC_SceneComposition *sceneComp,
        const CD_AudioSceneControlData__SceneCompositionData *a)
{
  __int64 v4; // x23
  __int64 v5; // x24
  __int64 v6; // x25
  __int64 v7; // x26
  __int64 v8; // d8
  __int64 v9; // d9
  __int64 v10; // d10
  __int64 v11; // d11
  unsigned __int16 v14; // w19
  __int64 v16; // x3
  int v17; // w2
  unsigned int v18; // w20
  __int64 result; // x0
  unsigned int v20; // w3
  __int64 v21; // x19
  int v22; // w26
  __int64 v23; // x1
  unsigned int v24; // w2
  __int64 v25; // x1
  int v26; // w23
  char *v27; // x1
  __int64 v28; // x26
  __int64 v29; // x2
  unsigned __int8 *v30; // x23
  unsigned int v31; // w21
  char *v32; // x3
  __int16 *v33; // x19
  __int64 v34; // x0
  __int64 v35; // x20
  signed __int64 v36; // x20
  _BYTE *v37; // x1
  _BYTE *v38; // x2
  signed __int64 v39; // x24
  __int64 v40; // x0
  __int64 v41; // x20
  signed __int64 v42; // x20
  char *v43; // x3
  _BYTE *v44; // x1
  _BYTE *v45; // x2
  signed __int64 v46; // x24
  __int64 v47; // x0
  __int64 v48; // x20
  signed __int64 v49; // x20
  unsigned __int64 v50; // x0
  char *v51; // x3
  char *v52; // x6
  char *v53; // x1
  _QWORD *v54; // x2
  __int64 v55; // x5
  __int64 v56; // x5
  __int64 v57; // x5
  __int64 v58; // x1
  __int64 v59; // x20
  char *v60; // x3
  signed __int64 v61; // x20
  _BYTE *v62; // x1
  _BYTE *v63; // x2
  signed __int64 v64; // x24
  __int64 v65; // x1
  __int64 v66; // x20
  char v67; // w0
  signed __int64 v68; // x20
  char *v69; // x3
  _BYTE *v70; // x1
  _BYTE *v71; // x2
  signed __int64 v72; // x24
  __int64 v73; // x2
  __int64 v74; // x20
  char v75; // w1
  __int16 v76; // w0
  signed __int64 v77; // x20
  char *v78; // x3
  __int64 v79; // x24
  const void *v80; // x1
  size_t v81; // x0
  __int64 v82; // x0
  __int64 v83; // x20
  signed __int64 v84; // x20
  char *v85; // x3
  _BYTE *v86; // x1
  _BYTE *v87; // x2
  signed __int64 v88; // x24
  __int64 v89; // x2
  __int64 v90; // x20
  char v91; // w1
  __int16 v92; // w0
  char *v93; // x3
  signed __int64 v94; // x20
  __int64 v95; // x24
  const void *v96; // x1
  size_t v97; // x0
  __int64 v98; // x0
  __int64 v99; // x20
  signed __int64 v100; // x20
  char *v101; // x3
  _BYTE *v102; // x1
  _BYTE *v103; // x2
  signed __int64 v104; // x19
  __int64 v105; // x2
  __int64 v106; // x0
  char *v107; // x5
  void *v108; // x5
  unsigned __int8 v109; // w20
  int v110; // w4
  int v111; // w4
  char v112; // w25
  char v113; // w21
  unsigned __int8 v114; // w24
  int v115; // w3
  float v116; // s11
  int v117; // s10
  int v118; // s9
  float v119; // s8
  int v120; // w2
  __int64 v121; // x0
  __int64 v122; // x4
  __int64 v123; // x0
  char *v124; // x3
  void *v125; // x3
  char *v126; // x0
  char *v127; // x1
  signed __int64 v128; // x19
  char *v129; // x0
  char *v130; // x1
  size_t v131; // x21
  unsigned int v132; // w24
  SceneCompositionSettings::CategorySettings *v133; // x21
  __int64 v134; // x0
  int v135; // w25
  __int64 v136; // x0
  __int64 v137; // x20
  int v138; // w19
  __int64 v139; // x1
  __int16 v140; // w19
  __int64 v141; // x0
  __int64 v142; // x25
  int v143; // w24
  SceneCompositionSettings::CategorySettings *i; // x20
  unsigned int v145; // w2
  __int64 v146; // x3
  unsigned int v147; // w0
  unsigned int v148; // w0
  int v149; // s0
  int v150; // s0
  unsigned __int64 v151; // x2
  __int64 v152; // x0
  __int64 v153; // x4
  signed __int64 v154; // x4
  char *v155; // x0
  char *v156; // x3
  _BYTE *v157; // x1
  _BYTE *v158; // x0
  size_t v159; // x2
  __int64 v160; // x0
  char *v161; // x3
  char *v162; // x0
  __int64 v163; // x1
  char *v164; // x1
  char *v165; // x7
  size_t v166; // x2
  unsigned int v167; // w5
  char *v168; // x0
  __int64 *v169; // x3
  __int64 v170; // x4
  int v171; // w2
  __int64 v172; // x0
  char *v173; // x3
  char *v174; // x6
  _QWORD *v175; // x0
  unsigned int v176; // w1
  __int64 v177; // x1
  __int64 v178; // [xsp+30h] [xbp+30h]
  __int64 v179; // [xsp+38h] [xbp+38h]
  __int64 v180; // [xsp+40h] [xbp+40h]
  __int64 v181; // [xsp+48h] [xbp+48h]
  __int64 v182; // [xsp+60h] [xbp+60h]
  __int64 v183; // [xsp+68h] [xbp+68h]
  __int64 v184; // [xsp+70h] [xbp+70h]
  __int64 v185; // [xsp+78h] [xbp+78h]
  SceneCompositionSettings::CategorySettings *v186; // [xsp+80h] [xbp+80h]
  __int64 v187; // [xsp+88h] [xbp+88h]
  __int64 v188; // [xsp+90h] [xbp+90h]
  void *dest; // [xsp+98h] [xbp+98h]
  __int16 *v190; // [xsp+A0h] [xbp+A0h]
  __int64 v191; // [xsp+A8h] [xbp+A8h]
  int v192; // [xsp+B0h] [xbp+B0h]
  int v193; // [xsp+B4h] [xbp+B4h]
  int v194; // [xsp+B8h] [xbp+B8h]
  int v195; // [xsp+BCh] [xbp+BCh]
  __int64 v196; // [xsp+C0h] [xbp+C0h]
  const char *v197; // [xsp+C8h] [xbp+C8h]
  __int16 v198; // [xsp+D8h] [xbp+D8h] BYREF
  __int64 v199; // [xsp+E0h] [xbp+E0h]
  void *v200; // [xsp+E8h] [xbp+E8h] BYREF
  char *v201; // [xsp+F0h] [xbp+F0h]
  char *v202; // [xsp+F8h] [xbp+F8h]
  void *v203; // [xsp+100h] [xbp+100h]
  int v204; // [xsp+108h] [xbp+108h]
  char *v205; // [xsp+110h] [xbp+110h]
  int v206; // [xsp+118h] [xbp+118h]
  char *v207; // [xsp+120h] [xbp+120h]
  __int16 v208; // [xsp+128h] [xbp+128h]
  void *src; // [xsp+130h] [xbp+130h]
  char *v210; // [xsp+138h] [xbp+138h]
  char *v211; // [xsp+140h] [xbp+140h]
  void *v212; // [xsp+148h] [xbp+148h]
  char *v213; // [xsp+150h] [xbp+150h]
  char *v214; // [xsp+158h] [xbp+158h]
  void *v215; // [xsp+160h] [xbp+160h]
  unsigned __int64 v216; // [xsp+168h] [xbp+168h]
  unsigned __int64 v217; // [xsp+170h] [xbp+170h]
  char v218; // [xsp+178h] [xbp+178h]
  void *v219; // [xsp+180h] [xbp+180h]
  char *v220; // [xsp+188h] [xbp+188h]
  char *v221; // [xsp+190h] [xbp+190h]
  char v222; // [xsp+198h] [xbp+198h]
  void *v223; // [xsp+1A0h] [xbp+1A0h]
  char *v224; // [xsp+1A8h] [xbp+1A8h]
  char *v225; // [xsp+1B0h] [xbp+1B0h]
  char v226; // [xsp+1B8h] [xbp+1B8h]
  __int16 v227; // [xsp+1BAh] [xbp+1BAh]
  void *v228; // [xsp+1C0h] [xbp+1C0h]
  char *v229; // [xsp+1C8h] [xbp+1C8h]
  char *v230; // [xsp+1D0h] [xbp+1D0h]
  void *v231; // [xsp+1D8h] [xbp+1D8h]
  char *v232; // [xsp+1E0h] [xbp+1E0h]
  char *v233; // [xsp+1E8h] [xbp+1E8h]
  char v234; // [xsp+1F0h] [xbp+1F0h]
  __int16 v235; // [xsp+1F2h] [xbp+1F2h]
  void *v236; // [xsp+1F8h] [xbp+1F8h]
  char *v237; // [xsp+200h] [xbp+200h]
  char *v238; // [xsp+208h] [xbp+208h]
  void *v239; // [xsp+210h] [xbp+210h]
  char *v240; // [xsp+218h] [xbp+218h]
  char *v241; // [xsp+220h] [xbp+220h]
  __int64 v242; // [xsp+228h] [xbp+228h]
  __int64 v243; // [xsp+230h] [xbp+230h]
  v14 = n;
  if ( !*(_BYTE *)a )
  {
    v18 = 0;
    goto LABEL_7;
  }
  v16 = *((_QWORD *)sceneComp + 20);
  v17 = *((unsigned __int8 *)a + 1);
  if ( v17 >= (unsigned __int16)(31711 * ((*((_QWORD *)sceneComp + 21) - v16) >> 3)) )
  {
    __printf_chk(1, "WARNING: presetIndex out of range %d. Using preset 0\n", v17);
    v18 = 0;
LABEL_7:
    result = SceneCompositionSettings::SetFromPreset(this, v14, sceneComp, 0);
    if ( (_DWORD)result )
      goto LABEL_4;
    goto LABEL_8;
  }
  v18 = *((unsigned __int8 *)a + 1);
  result = SceneCompositionSettings::SetFromPreset(this, n, sceneComp, v17);
  if ( (_DWORD)result )
    goto LABEL_4;
LABEL_8:
  if ( *((_BYTE *)a + 2) )
  {
    v178 = v4;
    v179 = v5;
    v180 = v6;
    v181 = v7;
    if ( *((_BYTE *)a + 3) )
    {
      v20 = *((unsigned __int8 *)a + 4);
      v21 = 0;
      v22 = 0;
      if ( !*((_BYTE *)a + 4) )
        goto LABEL_148;
      v186 = (SceneCompositionSettings::CategorySettings *)&v200;
      do
      {
        if ( *((_BYTE *)sceneComp + 304) )
        {
          v23 = *((_QWORD *)sceneComp + 39);
          v24 = *(unsigned __int8 *)(*((_QWORD *)a + 1) + v21);
          if ( v24 < (unsigned __int16)(-29789 * ((*((_QWORD *)sceneComp + 40) - v23) >> 4)) )
          {
            v25 = v23 + 176LL * *(unsigned __int8 *)(*((_QWORD *)a + 1) + v21);
            v26 = *(unsigned __int8 *)(v25 + 24);
            if ( *(_BYTE *)(v25 + 24) )
            {
              v27 = *((char **)this + 1);
              if ( v24 < (unsigned int)((__int64)(*((_QWORD *)this + 2) - (_QWORD)v27) >> 6) )
              {
                v151 = (unsigned __int64)*(unsigned __int8 *)(*((_QWORD *)a + 1) + v21) << 6;
                v187 = (__int64)&v27[v151];
                v152 = *(_QWORD *)&v27[v151];
                v153 = *(_QWORD *)&v27[v151 + 8];
                v200 = nullptr;
                v201 = nullptr;
                v154 = v153 - v152;
                v202 = nullptr;
                if ( v154 >> 1 )
                {
                  if ( (unsigned __int64)(v154 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
                  {
                    v182 = v8;
                    v183 = v9;
                    v184 = v10;
                    v185 = v11;
LABEL_187:
                    std::__throw_bad_alloc();
                  }
                  v188 = v154;
                  dest = v27;
                  v190 = (__int16 *)v151;
                  v155 = (char *)operator new(v154);
                  v154 = v188;
                  v27 = (char *)dest;
                  v156 = v155;
                  v151 = (unsigned __int64)v190;
                }
                else
                {
                  v156 = nullptr;
                }
                v200 = v156;
                v201 = v156;
                v202 = &v156[v154];
                v157 = *(_BYTE **)&v27[v151];
                v158 = *(_BYTE **)(v187 + 8);
                v159 = v158 - v157;
                if ( v157 != v158 )
                {
                  v188 = v158 - v157;
                  v156 = (char *)memmove(v156, v157, v159);
                  v159 = v188;
                }
                v201 = &v156[v159];
                v203 = nullptr;
                v204 = 0;
                v205 = nullptr;
                v206 = 0;
                v207 = nullptr;
                dest = (void *)(v187 + 40);
                v160 = *(unsigned int *)(v187 + 48) + 8LL * (*(_QWORD *)(v187 + 40) - *(_QWORD *)(v187 + 24));
                v188 = v160;
                if ( v160 )
                {
                  v190 = (__int16 *)(8 * ((unsigned __int64)(v160 + 63) >> 6));
                  v191 = v160 + 63;
                  v161 = (char *)operator new((unsigned __int64)v190);
                  v203 = v161;
                  v177 = v191;
                  v204 = 0;
                  if ( v188 >= 0 )
                    v177 = v188;
                  v207 = (char *)v190 + (_QWORD)v161;
                  v162 = &v161[8 * (v177 >> 6)];
                  if ( v188 <= 0 )
                    v163 = -(-v188 & 0x3F);
                  else
                    v163 = v188 & 0x3F;
                  if ( v163 < 0 )
                  {
                    LODWORD(v163) = v163 + 64;
                    v162 -= 8;
                  }
                }
                else
                {
                  v161 = nullptr;
                  v162 = nullptr;
                  LODWORD(v163) = 0;
                }
                v205 = v162;
                v206 = v163;
                v164 = *(char **)(v187 + 24);
                v165 = *(char **)(v187 + 40);
                v166 = v165 - v164;
                v167 = *((_DWORD *)dest + 2);
                if ( v164 != v165 )
                {
                  v187 = v165 - v164;
                  LODWORD(v188) = v167;
                  dest = v165;
                  v168 = (char *)memmove(v161, v164, v166);
                  v167 = v188;
                  v161 = v168;
                  v166 = v187;
                  v165 = (char *)dest;
                }
                v169 = (__int64 *)&v161[v166];
                v170 = v167;
                v171 = 0;
                if ( v167 )
                {
                  do
                  {
                    v172 = *v169 & ~(1LL << v171);
                    if ( ((1LL << v171) & *(_QWORD *)v165) != 0 )
                      v172 = (1LL << v171) | *v169;
                    *v169 = v172;
                    if ( v171 == 63 )
                    {
                      v165 += 8;
                      ++v169;
                      v171 = 0;
                    }
                    else
                    {
                      ++v171;
                    }
                    --v170;
                  }
                  while ( v170 );
                }
                SceneCompositionSettings::CategorySettings::SetAllOff(v186);
                v173 = (char *)v200;
                v174 = v201;
                v175 = v203;
                if ( (unsigned int)((v201 - (_BYTE *)v200) >> 1) )
                {
                  v176 = 0;
                  do
                  {
                    if ( *(unsigned __int8 *)(*((_QWORD *)a + 4) + v21) == *(unsigned __int16 *)&v173[2 * v176] )
                    {
                      v22 = v26;
                      v175[(unsigned __int64)v176 >> 6] |= 1LL << v176;
                      v173 = (char *)v200;
                      v174 = v201;
                      v175 = v203;
                    }
                    ++v176;
                  }
                  while ( v176 < (unsigned int)((v174 - v173) >> 1) );
                }
                if ( v175 )
                  operator delete(v175);
                if ( v200 )
                  operator delete(v200);
                v20 = *((unsigned __int8 *)a + 4);
              }
            }
          }
        }
        ++v21;
      }
      while ( v20 > (unsigned int)v21 );
      if ( !v22 )
LABEL_148:
        __printf_chk(1, "WARNING: User setting of category failed\n");
    }
    if ( !*((_BYTE *)a + 56) )
    {
      v4 = v178;
      v5 = v179;
      v6 = v180;
      v7 = v181;
      goto LABEL_134;
    }
    v28 = 0;
    LODWORD(v188) = 0;
    v196 = 48LL * v18;
    v197 = "WARNING: User setting of comsposition parameters failed\n";
    v190 = &v198;
    v182 = v8;
    v183 = v9;
    v184 = v10;
    v185 = v11;
    while ( 1 )
    {
      while ( 1 )
      {
        v29 = *((_QWORD *)sceneComp + 16);
        v30 = (unsigned __int8 *)(*((_QWORD *)a + 8) + v28);
        v31 = *v30;
        if ( v31 < (unsigned int)((*((_QWORD *)sceneComp + 17) - v29) >> 8) )
          break;
        __printf_chk(1, v197);
LABEL_137:
        v28 += 80;
        v148 = *((unsigned __int8 *)a + 56);
        LODWORD(v188) = v188 + 1;
        if ( v148 <= (unsigned int)v188 )
        {
LABEL_133:
          v4 = v178;
          v5 = v179;
          v6 = v180;
          v7 = v181;
          v8 = v182;
          v9 = v183;
          v10 = v184;
          v11 = v185;
          goto LABEL_134;
        }
      }
      v32 = nullptr;
      v33 = (__int16 *)(v29 + ((unsigned __int64)*v30 << 8));
      v34 = *((_QWORD *)v33 + 1);
      v35 = *((_QWORD *)v33 + 2);
      v208 = *v33;
      src = nullptr;
      v210 = nullptr;
      v211 = nullptr;
      v36 = v35 - v34;
      if ( v36 >> 1 )
      {
        if ( (unsigned __int64)(v36 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          goto LABEL_187;
        v32 = (char *)operator new(v36);
      }
      src = v32;
      v210 = v32;
      v211 = &v32[v36];
      v37 = *((_BYTE **)v33 + 1);
      v38 = *((_BYTE **)v33 + 2);
      v39 = v38 - v37;
      if ( v37 != v38 )
        v32 = (char *)memmove(v32, v37, v38 - v37);
      v40 = *((_QWORD *)v33 + 4);
      v41 = *((_QWORD *)v33 + 5);
      v210 = &v32[v39];
      v212 = nullptr;
      v213 = nullptr;
      v214 = nullptr;
      v42 = v41 - v40;
      if ( v42 >> 1 )
      {
        if ( (unsigned __int64)(v42 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        v43 = (char *)operator new(v42);
      }
      else
      {
        v43 = nullptr;
      }
      v212 = v43;
      v213 = v43;
      v214 = &v43[v42];
      v44 = *((_BYTE **)v33 + 4);
      v45 = *((_BYTE **)v33 + 5);
      v46 = v45 - v44;
      if ( v44 != v45 )
        v43 = (char *)memmove(v43, v44, v45 - v44);
      v47 = *((_QWORD *)v33 + 7);
      v48 = *((_QWORD *)v33 + 8);
      v213 = &v43[v46];
      v215 = nullptr;
      v216 = 0;
      v49 = v48 - v47;
      v217 = 0;
      v50 = 0xAAAAAAAAAAAAAAABLL * (v49 >> 4);
      if ( v50 )
      {
        if ( v50 > 0x2AAAAAAAAAAAAAALL )
          std::__throw_bad_alloc();
        v50 = operator new(v49);
      }
      v215 = (void *)v50;
      v216 = v50;
      v217 = v50 + v49;
      v52 = *((char **)v33 + 7);
      v51 = *((char **)v33 + 8);
      if ( v52 != v51 )
      {
        v53 = *((char **)v33 + 7);
        v54 = (_QWORD *)v50;
        do
        {
          v55 = *((_QWORD *)v53 + 1);
          *v54 = *(_QWORD *)v53;
          v54[1] = v55;
          v53 += 48;
          v56 = *((_QWORD *)v53 - 3);
          v54[2] = *((_QWORD *)v53 - 4);
          v54[3] = v56;
          v54 += 6;
          v57 = *((_QWORD *)v53 - 1);
          *(v54 - 2) = *((_QWORD *)v53 - 2);
          *(v54 - 1) = v57;
        }
        while ( v51 != v53 );
        v50 += 48 * ((0xAAAAAAAAAAAAAABLL * ((unsigned __int64)(v51 - 48 - v52) >> 4)) & 0xFFFFFFFFFFFFFFFLL) + 48;
      }
      v58 = *((_QWORD *)v33 + 11);
      v59 = *((_QWORD *)v33 + 12);
      v216 = v50;
      v60 = nullptr;
      v218 = *((_BYTE *)v33 + 80);
      v219 = nullptr;
      v220 = nullptr;
      v61 = v59 - v58;
      v221 = nullptr;
      if ( v61 >> 1 )
      {
        if ( (unsigned __int64)(v61 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        v60 = (char *)operator new(v61);
      }
      v219 = v60;
      v220 = v60;
      v221 = &v60[v61];
      v62 = *((_BYTE **)v33 + 11);
      v63 = *((_BYTE **)v33 + 12);
      v64 = v63 - v62;
      if ( v62 != v63 )
        v60 = (char *)memmove(v60, v62, v63 - v62);
      v65 = *((_QWORD *)v33 + 15);
      v66 = *((_QWORD *)v33 + 16);
      v67 = *((_BYTE *)v33 + 112);
      v220 = &v60[v64];
      v222 = v67;
      v223 = nullptr;
      v224 = nullptr;
      v68 = v66 - v65;
      v225 = nullptr;
      if ( v68 >> 1 )
      {
        if ( (unsigned __int64)(v68 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        v69 = (char *)operator new(v68);
      }
      else
      {
        v69 = nullptr;
      }
      v223 = v69;
      v224 = v69;
      v225 = &v69[v68];
      v70 = *((_BYTE **)v33 + 15);
      v71 = *((_BYTE **)v33 + 16);
      v72 = v71 - v70;
      if ( v70 != v71 )
        v69 = (char *)memmove(v69, v70, v71 - v70);
      v73 = *((_QWORD *)v33 + 19);
      v74 = *((_QWORD *)v33 + 20);
      v75 = *((_BYTE *)v33 + 144);
      v76 = v33[73];
      v224 = &v69[v72];
      v226 = v75;
      v227 = v76;
      v77 = v74 - v73;
      v228 = nullptr;
      v229 = nullptr;
      v230 = nullptr;
      if ( v77 )
      {
        if ( v77 < 0 )
          std::__throw_bad_alloc();
        v78 = (char *)operator new(v77);
      }
      else
      {
        v78 = nullptr;
      }
      v228 = v78;
      v229 = v78;
      v79 = 0;
      v230 = &v78[v77];
      v80 = *((const void **)v33 + 19);
      v81 = *((_QWORD *)v33 + 20) - (_QWORD)v80;
      if ( v81 )
      {
        v79 = *((_QWORD *)v33 + 20) - (_QWORD)v80;
        v78 = (char *)memmove(v78, v80, v81);
      }
      v82 = *((_QWORD *)v33 + 22);
      v83 = *((_QWORD *)v33 + 23);
      v229 = &v78[v79];
      v231 = nullptr;
      v232 = nullptr;
      v233 = nullptr;
      v84 = v83 - v82;
      if ( v84 >> 1 )
      {
        if ( (unsigned __int64)(v84 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        v85 = (char *)operator new(v84);
      }
      else
      {
        v85 = nullptr;
      }
      v231 = v85;
      v232 = v85;
      v233 = &v85[v84];
      v86 = *((_BYTE **)v33 + 22);
      v87 = *((_BYTE **)v33 + 23);
      v88 = v87 - v86;
      if ( v86 != v87 )
        v85 = (char *)memmove(v85, v86, v87 - v86);
      v89 = *((_QWORD *)v33 + 26);
      v90 = *((_QWORD *)v33 + 27);
      v91 = *((_BYTE *)v33 + 200);
      v92 = v33[101];
      v232 = &v85[v88];
      v93 = nullptr;
      v234 = v91;
      v235 = v92;
      v94 = v90 - v89;
      v236 = nullptr;
      v237 = nullptr;
      v238 = nullptr;
      if ( v94 )
      {
        if ( v94 < 0 )
          std::__throw_bad_alloc();
        v93 = (char *)operator new(v94);
      }
      v236 = v93;
      v237 = v93;
      v95 = 0;
      v238 = &v93[v94];
      v96 = *((const void **)v33 + 26);
      v97 = *((_QWORD *)v33 + 27) - (_QWORD)v96;
      if ( v97 )
      {
        v95 = *((_QWORD *)v33 + 27) - (_QWORD)v96;
        v93 = (char *)memmove(v93, v96, v97);
      }
      v98 = *((_QWORD *)v33 + 29);
      v99 = *((_QWORD *)v33 + 30);
      v237 = &v93[v95];
      v239 = nullptr;
      v240 = nullptr;
      v241 = nullptr;
      v100 = v99 - v98;
      if ( v100 >> 1 )
      {
        if ( (unsigned __int64)(v100 >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        v101 = (char *)operator new(v100);
      }
      else
      {
        v101 = nullptr;
      }
      v239 = v101;
      v240 = v101;
      v241 = &v101[v100];
      v102 = *((_BYTE **)v33 + 29);
      v103 = *((_BYTE **)v33 + 30);
      v104 = v103 - v102;
      if ( v102 != v103 )
        v101 = (char *)memmove(v101, v102, v103 - v102);
      v105 = *((_QWORD *)this + 4);
      v240 = &v101[v104];
      v106 = v105 + 88LL * v31;
      v187 = 88LL * v31;
      v107 = *(char **)(v106 + 8);
      v200 = *(void **)v106;
      v201 = v107;
      v108 = *(void **)(v106 + 24);
      v202 = *(char **)(v106 + 16);
      v203 = v108;
      v204 = *(_DWORD *)(v106 + 32);
      v109 = *(_BYTE *)v106;
      v110 = *((unsigned __int8 *)v215 + v196 + 8);
      LODWORD(v191) = *(unsigned __int8 *)(v106 + 1);
      LODWORD(dest) = *(unsigned __int8 *)(v106 + 8);
      LODWORD(v105) = *(unsigned __int8 *)(v106 + 16);
      v194 = v110;
      v111 = *((unsigned __int8 *)v215 + v196 + 14);
      v112 = *(_BYTE *)(v106 + 24);
      v113 = *(_BYTE *)(v106 + 32);
      v114 = *(_BYTE *)(v106 + 33);
      v115 = *((unsigned __int8 *)v215 + v196);
      LODWORD(v186) = v105;
      v193 = v111;
      v116 = *(float *)(v106 + 4);
      v117 = *(_DWORD *)(v106 + 12);
      v118 = *(_DWORD *)(v106 + 20);
      v119 = *(float *)(v106 + 28);
      v120 = *((unsigned __int8 *)v215 + v196 + 1);
      v195 = *((unsigned __int8 *)v215 + v196 + 20);
      v192 = *((unsigned __int8 *)v215 + v196 + 26);
      if ( v115 )
        v109 = v30[1];
      if ( v120 && v30[2] )
      {
        v116 = apac::scene::DecodeInteractivityGain(*((_WORD *)v30 + 2), 1);
        LODWORD(v191) = 1;
      }
      if ( v194 && v30[6] )
      {
        apac::scene::DecodeAngleOffset(v30[7], 6u);
        v117 = v149;
        LODWORD(dest) = 1;
      }
      if ( v193 && v30[8] )
      {
        apac::scene::DecodeAngleOffset(v30[9], 6u);
        v118 = v150;
        LODWORD(v186) = 1;
      }
      if ( v195 && v30[10] )
      {
        v119 = apac::scene::DecodeDistanceRatio(v30[11], 4u);
        v112 = 1;
      }
      if ( v192 && v30[12] )
      {
        v114 = v30[13];
        v113 = 1;
      }
      v121 = *((_QWORD *)this + 4);
      BYTE1(v200) = v191;
      v122 = v187;
      LOBYTE(v200) = v109;
      LOBYTE(v201) = (_BYTE)dest;
      v123 = v121 + v187;
      LOBYTE(v202) = (_BYTE)v186;
      LOBYTE(v203) = v112;
      LOBYTE(v204) = v113;
      BYTE1(v204) = v114;
      *((float *)&v200 + 1) = v116;
      HIDWORD(v201) = v117;
      HIDWORD(v202) = v118;
      *((float *)&v203 + 1) = v119;
      v124 = v201;
      *(_QWORD *)v123 = v200;
      *(_QWORD *)(v123 + 8) = v124;
      v125 = v203;
      *(_QWORD *)(v123 + 16) = v202;
      *(_QWORD *)(v123 + 24) = v125;
      *(_WORD *)(v123 + 32) = v204;
      if ( v30[14] )
        *(_WORD *)(*((_QWORD *)this + 4) + v122 + 36) = v30[15];
      v127 = (char *)src;
      v126 = v210;
      LODWORD(v191) = v30[15];
      v128 = v210 - (_BYTE *)src;
      if ( (v210 - (_BYTE *)src) >> 1 )
      {
        if ( (unsigned __int64)((v210 - (_BYTE *)src) >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        dest = (void *)operator new(v210 - (_BYTE *)src);
        v127 = (char *)src;
        v126 = v210;
        v128 = v210 - (_BYTE *)src;
      }
      else
      {
        dest = nullptr;
      }
      if ( v127 != v126 )
        memmove(dest, v127, v128);
      v130 = (char *)v212;
      v129 = v213;
      v131 = v213 - (_BYTE *)v212;
      if ( (v213 - (_BYTE *)v212) >> 1 )
      {
        if ( (unsigned __int64)((v213 - (_BYTE *)v212) >> 1) > 0x3FFFFFFFFFFFFFFFLL )
          std::__throw_bad_alloc();
        v186 = (SceneCompositionSettings::CategorySettings *)operator new(v213 - (_BYTE *)v212);
        v130 = (char *)v212;
        v129 = v213;
        v131 = v213 - (_BYTE *)v212;
      }
      else
      {
        v186 = nullptr;
      }
      if ( v130 != v129 )
        memmove(v186, v130, v131);
      v132 = v128 >> 1;
      v133 = (SceneCompositionSettings::CategorySettings *)((char *)v186 + v131);
      v134 = *((_QWORD *)this + 4);
      if ( v132 )
      {
        v135 = (unsigned __int16)v191;
        v136 = v134 + v187;
        v137 = 0;
        do
        {
          while ( 1 )
          {
            v139 = v136;
            v140 = v137;
            if ( v135 == (unsigned __int16)v137 )
              break;
            ++v137;
            v138 = (unsigned __int16)(v140 + 1);
            if ( v132 <= (unsigned int)v137 )
              goto LABEL_102;
          }
          std::vector<unsigned short>::operator=(
            v136 + 64,
            *((_QWORD *)sceneComp + 7) + 144LL * *((unsigned __int16 *)dest + v137++));
          v138 = (unsigned __int16)(v140 + 1);
          v136 = *((_QWORD *)this + 4) + v187;
          v139 = v136;
        }
        while ( v132 > (unsigned int)v137 );
      }
      else
      {
        v139 = v134 + v187;
        v138 = 0;
      }
LABEL_102:
      v141 = *(_QWORD *)(v139 + 40);
      v142 = v139 + 40;
      if ( v141 != *(_QWORD *)(v139 + 48) )
        *(_QWORD *)(v139 + 48) = v141;
      v143 = (unsigned __int16)v191;
      for ( i = v186; v133 != i; v138 = (unsigned __int16)(v138 + 1) )
      {
        while ( v138 != v143 )
        {
          i = (SceneCompositionSettings::CategorySettings *)((char *)i + 2);
          v138 = (unsigned __int16)(v138 + 1);
          if ( v133 == i )
            goto LABEL_111;
        }
        if ( v30[16] )
          v145 = v30[17];
        else
          v145 = **(unsigned __int16 **)(*((_QWORD *)sceneComp + 13) + 144LL * *(unsigned __int16 *)i);
        v146 = *((_QWORD *)sceneComp + 10);
        v198 = v145;
        v199 = v146 + 80LL * v145;
        std::vector<SceneCompositionSettings::SelectionSetSettings>::emplace_back<SceneCompositionSettings::SelectionSetSettings>(
          v142,
          v190);
        i = (SceneCompositionSettings::CategorySettings *)((char *)i + 2);
      }
LABEL_111:
      if ( v186 )
        operator delete(v186);
      if ( dest )
        operator delete(dest);
      if ( v239 )
        operator delete(v239);
      if ( v236 )
        operator delete(v236);
      if ( v231 )
        operator delete(v231);
      if ( v228 )
        operator delete(v228);
      if ( v223 )
        operator delete(v223);
      if ( v219 )
        operator delete(v219);
      if ( v215 )
        operator delete(v215);
      if ( v212 )
        operator delete(v212);
      if ( !src )
        goto LABEL_137;
      operator delete(src);
      v28 += 80;
      v147 = *((unsigned __int8 *)a + 56);
      LODWORD(v188) = v188 + 1;
      if ( v147 <= (unsigned int)v188 )
        goto LABEL_133;
    }
  }
LABEL_134:
  result = SceneCompositionSettings::SetSceneComponentModifiers(this);
  if ( !(_DWORD)result )
    result = SceneCompositionSettings::SetEffectiveInteractivityGains(this, sceneComp);
LABEL_4:
  if ( v242 != _stack_chk_guard )
  {
    v178 = v4;
    v179 = v5;
    v180 = v6;
    v181 = v7;
    v182 = v8;
    v183 = v9;
    v184 = v10;
    v185 = v11;
  }
  return result;
}


