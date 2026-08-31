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


