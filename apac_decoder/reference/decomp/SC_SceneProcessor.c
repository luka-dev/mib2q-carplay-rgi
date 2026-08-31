/* ============================================================================
 * SC_SceneProcessor
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 3528 bytes of .text.
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
/* 0x0009e150    216  SC_SceneProcessor::Update(std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&) */
/* 0x0009e230   2280  SC_SceneProcessor::Process(float const* const*, float**, unsigned int) */
/* 0x0009f130   1032  SC_SceneProcessor::Initialize(unsigned int, unsigned int, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&) */

/* ------------------------------------------------------------------------ */
/* 0x0009e150  size 216 bytes  54 insns
 *   SC_SceneProcessor::Update(std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&)
 *   [_ZN17SC_SceneProcessor6UpdateERKSt6vectorI20SC_ComponentModifierSaIS1_EE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneProcessor::Update(SC_SceneProcessor *this, const std_vector *vec)
{
  unsigned int v4; // w0
  bool v5; // zf
  int v6; // w0
  __int64 v7; // x19
  __int64 v8; // x21
  __int64 v10; // [xsp+50h] [xbp+50h]
  v4 = *((_QWORD *)this + 11) - *((_QWORD *)this + 10);
  v5 = (v4 & 0x3FFFC) == 0;
  v6 = (unsigned __int16)(v4 >> 2);
  if ( !v5 )
  {
    v7 = 0;
    v8 = 36LL * (unsigned int)(v6 - 1) + 36;
    do
    {
      if ( !SC_ComponentModifier::operator==(
              (SC_ComponentModifier *)((char *)vec->begin + v7),
              (const SC_ComponentModifier *)(*((_QWORD *)this + 1) + v7)) )
        *(_DWORD *)this = 1;
      v7 += 36;
    }
    while ( v7 != v8 );
  }
  if ( *(_DWORD *)this == 1 )
    std::vector<SC_ComponentModifier>::operator=((std_vector *)((char *)this + 32), vec);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0009e230  size 2280 bytes  567 insns
 *   SC_SceneProcessor::Process(float const* const*, float**, unsigned int)
 *   [_ZN17SC_SceneProcessor7ProcessEPKPKfPPfj]
 *   data refs: 0x330420, 0x330440, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneProcessor::Process(SC_SceneProcessor *this, const float **a, float **buf, unsigned int n)
{
  const float **v4; // x19
  float **v5; // x20
  __int64 v6; // x21
  __int64 v7; // x22
  __int64 v8; // x25
  __int64 v9; // x26
  __int64 v10; // x27
  __int64 v11; // x28
  __int64 v12; // d8
  __int64 v13; // d9
  __int64 v15; // x4
  unsigned int v16; // w21
  __int64 v17; // x26
  __int64 v18; // x28
  __int64 v19; // x21
  __int64 v20; // x1
  __int64 v21; // x2
  float *v22; // x27
  __int64 v23; // x2
  __int64 v24; // x27
  unsigned __int64 v25; // x0
  unsigned __int64 v26; // x3
  __int64 v27; // x4
  __int64 v28; // x4
  __int64 v29; // x23
  __int64 v30; // x21
  size_t v31; // x27
  _BYTE *v32; // x1
  __int64 v33; // x10
  float *v34; // x2
  float v35; // s0
  float v36; // s1
  __int64 v37; // x0
  int v38; // w0
  unsigned int v39; // w22
  __int64 v40; // x6
  int v41; // w0
  __int64 v42; // x0
  __int64 v43; // x26
  size_t v44; // x25
  unsigned int v45; // w27
  __int64 v46; // x4
  float *v47; // x0
  __int64 v48; // x1
  __int64 v49; // x22
  __int64 v50; // t1
  __int64 result; // x0
  unsigned __int64 v52; // x0
  unsigned __int64 v53; // x3
  unsigned int v54; // w1
  __int64 v55; // x23
  size_t v56; // x27
  __int64 v57; // x2
  float v58; // s1
  unsigned int v59; // w28
  unsigned int v60; // w0
  __int64 v61; // x8
  __int64 v62; // x4
  __int64 v63; // x1
  void *v64; // x0
  const float *v65; // x1
  unsigned __int64 v66; // x27
  __int64 v67; // x26
  size_t v68; // x25
  __int64 v69; // x4
  float *v70; // x0
  __int64 v71; // x1
  __int64 v72; // x22
  __int64 v73; // t1
  unsigned int v74; // w28
  __int64 v75; // x11
  unsigned int v76; // w0
  __int64 v77; // x12
  __int64 v78; // x10
  float *v79; // x2
  __int64 v80; // x5
  __int64 v81; // x9
  __int64 v82; // x3
  const float *v83; // x0
  __int64 v84; // x5
  unsigned int v85; // w22
  __int64 v86; // x1
  __int64 v87; // x0
  unsigned int v88; // w28
  __int64 v89; // x0
  unsigned int v90; // w22
  __int64 v91; // x1
  __int64 v92; // x0
  __int64 v93; // x11
  __int64 v94; // x0
  unsigned int v95; // w22
  __int64 v96; // x11
  __int64 v97; // x0
  const float **v98; // [xsp+10h] [xbp+10h]
  float **v99; // [xsp+18h] [xbp+18h]
  __int64 v100; // [xsp+20h] [xbp+20h]
  __int64 v101; // [xsp+28h] [xbp+28h]
  __int64 v102; // [xsp+40h] [xbp+40h]
  __int64 v103; // [xsp+48h] [xbp+48h]
  __int64 v104; // [xsp+50h] [xbp+50h]
  __int64 v105; // [xsp+58h] [xbp+58h]
  __int64 v106; // [xsp+60h] [xbp+60h]
  __int64 v107; // [xsp+68h] [xbp+68h]
  __int64 v108; // [xsp+70h] [xbp+70h]
  float *v109; // [xsp+78h] [xbp+78h]
  __int64 v110; // [xsp+80h] [xbp+80h]
  unsigned int v111; // [xsp+88h] [xbp+88h]
  unsigned int v112; // [xsp+8Ch] [xbp+8Ch]
  float *v113; // [xsp+90h] [xbp+90h]
  float *v114; // [xsp+98h] [xbp+98h]
  int v115; // [xsp+ACh] [xbp+ACh] BYREF
  float v116; // [xsp+B0h] [xbp+B0h] BYREF
  float v117; // [xsp+B4h] [xbp+B4h] BYREF
  __int64 v118; // [xsp+B8h] [xbp+B8h]
  __int64 v119; // [xsp+C0h] [xbp+C0h]
  v115 = 0;
  v15 = *((_QWORD *)this + 11);
  v111 = (unsigned __int16)(((unsigned int)v15 - (unsigned int)*((_QWORD *)this + 10)) >> 2);
  v112 = n;
  if ( n > 0x400 )
  {
    __printf_chk(1, "ERROR: frame size exceeded %d\n", n);
    result = 0xFFFFFFFFLL;
    goto LABEL_39;
  }
  v4 = a;
  v5 = buf;
  v100 = v6;
  v101 = v7;
  v16 = *(_DWORD *)this;
  if ( *(_DWORD *)this )
  {
    if ( v16 != 1 )
    {
      __printf_chk(1, "ERROR: mTransitionState invalid\n", buf, n, v15, 0);
      result = 0xFFFFFFFFLL;
      v6 = v100;
      v7 = v101;
      goto LABEL_39;
    }
    v102 = v8;
    v103 = v9;
    v104 = v10;
    v105 = v11;
    v106 = v12;
    v107 = v13;
    if ( v111 )
    {
      v17 = 0;
      v18 = 36LL * (v111 - 1) + 36;
      v19 = 0;
      do
      {
        while ( 1 )
        {
          v20 = *((_QWORD *)this + 24);
          v21 = *((_QWORD *)this + 1) + v19;
          v22 = (float *)(v20 + v17);
          if ( *(_BYTE *)v21 )
            *v22 = *(_BYTE *)(v21 + 1) ? powf(10.0, *(float *)(v21 + 4) * 0.05) : 1.0;
          else
            *(_DWORD *)(v20 + v17) = 0;
          v23 = *((_QWORD *)this + 4) + v19;
          v24 = *((_QWORD *)this + 24) + v17;
          if ( *(_BYTE *)v23 )
            break;
          *(_DWORD *)(v24 + 4) = 0;
          v19 += 36;
          v17 += 8;
          if ( v18 == v19 )
            goto LABEL_14;
        }
        if ( *(_BYTE *)(v23 + 1) )
          *(float *)(v24 + 4) = powf(10.0, *(float *)(v23 + 4) * 0.05);
        else
          *(float *)(v24 + 4) = 1.0;
        v19 += 36;
        v17 += 8;
      }
      while ( v18 != v19 );
LABEL_14:
      if ( !*((_WORD *)this + 3) )
      {
        std::vector<SC_ComponentModifier>::operator=(
          (std_vector *)((char *)this + 8),
          (const std_vector *)((char *)this + 32));
LABEL_18:
        v29 = 0;
        v30 = 0;
        v31 = 4LL * v112;
        v110 = v112;
        v113 = &v117;
        v114 = &v116;
        while ( 1 )
        {
          v32 = (_BYTE *)(*((_QWORD *)this + 1) + v29);
          if ( !*v32 )
            goto LABEL_28;
          if ( v32[1] )
          {
            v33 = 8 * v30;
            v34 = (float *)(*((_QWORD *)this + 24) + 8 * v30);
            v35 = *v34;
            v36 = v34[1];
            if ( *v34 == 0.0 && v36 == 0.0 )
              goto LABEL_28;
            v37 = *((_QWORD *)this + 10);
            if ( v35 == v36 )
            {
              v38 = *(_DWORD *)(v37 + 4 * v30);
              if ( v35 == 1.0 )
              {
                if ( v38 )
                {
                  v39 = 0;
                  do
                  {
                    v40 = *((_QWORD *)this + 27);
                    v41 = *(_DWORD *)(*((_QWORD *)this + 13) + 4 * v30);
                    LODWORD(v108) = v39 + *(_DWORD *)(*((_QWORD *)this + 16) + 4 * v30);
                    v42 = v39 + v41;
                    ++v39;
                    memcpy(*(void **)(v40 + 24LL * (unsigned int)v108), v4[v42], v31);
                    *(_QWORD *)(*((_QWORD *)this + 19) + 8LL * ((unsigned int)v108 >> 6)) &= ~(1LL << v108);
                  }
                  while ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v30) > v39 );
                }
                goto LABEL_28;
              }
              if ( v38 )
              {
                v90 = 0;
                while ( 1 )
                {
                  v91 = *((_QWORD *)this + 16);
                  v92 = *((_QWORD *)this + 13);
                  v109 = (float *)v33;
                  v93 = *((_QWORD *)this + 27);
                  v94 = v90 + *(_DWORD *)(v92 + 4 * v30);
                  LODWORD(v108) = v90 + *(_DWORD *)(v91 + 4 * v30);
                  ++v90;
                  vDSP_vsmul(v4[v94], 1, v34, *(_QWORD *)(v93 + 24LL * (unsigned int)v108), 1, v110);
                  *(_QWORD *)(*((_QWORD *)this + 19) + 8LL * ((unsigned int)v108 >> 6)) &= ~(1LL << v108);
                  if ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v30) <= v90 )
                    break;
                  v33 = (__int64)v109;
                  v34 = (float *)((char *)v109 + *((_QWORD *)this + 24));
                }
                ++v30;
                v29 += 36;
                if ( v111 <= (unsigned int)v30 )
                  goto LABEL_29;
              }
              else
              {
LABEL_28:
                ++v30;
                v29 += 36;
                if ( v111 <= (unsigned int)v30 )
                  goto LABEL_29;
              }
            }
            else
            {
              v116 = *v34;
              v117 = 0.0;
              if ( v112 )
                v117 = (float)(v36 - *v34) / (float)(int)v112;
              v95 = 0;
              if ( !*(_DWORD *)(v37 + 4 * v30) )
                goto LABEL_28;
              do
              {
                v96 = *((_QWORD *)this + 27);
                v97 = v95 + *(_DWORD *)(*((_QWORD *)this + 13) + 4 * v30);
                LODWORD(v108) = v95 + *(_DWORD *)(*((_QWORD *)this + 16) + 4 * v30);
                ++v95;
                vDSP_vrampmul(v4[v97], 1, v114, v113, *(_QWORD *)(v96 + 24LL * (unsigned int)v108), 1, v110);
                *(_QWORD *)(*((_QWORD *)this + 19) + 8LL * ((unsigned int)v108 >> 6)) &= ~(1LL << v108);
              }
              while ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v30) > v95 );
              ++v30;
              v29 += 36;
              if ( v111 <= (unsigned int)v30 )
                goto LABEL_29;
            }
          }
          else
          {
            v84 = 4 * v30;
            if ( !*(_DWORD *)(*((_QWORD *)this + 10) + 4 * v30) )
              goto LABEL_28;
            v85 = 0;
            do
            {
              v86 = *((_QWORD *)this + 16);
              v87 = *((_QWORD *)this + 13);
              v108 = v84;
              v88 = v85 + *(_DWORD *)(v86 + v84);
              v89 = v85 + *(_DWORD *)(v87 + v84);
              ++v85;
              memcpy(*(void **)(*((_QWORD *)this + 27) + 24LL * v88), v4[v89], v31);
              v84 = v108;
              *(_QWORD *)(*((_QWORD *)this + 19) + 8LL * (v88 >> 6)) &= ~(1LL << v88);
            }
            while ( *(_DWORD *)(*((_QWORD *)this + 10) + v84) > v85 );
            ++v30;
            v29 += 36;
            if ( v111 <= (unsigned int)v30 )
              goto LABEL_29;
          }
        }
      }
    }
    else if ( !*((_WORD *)this + 3) )
    {
      std::vector<SC_ComponentModifier>::operator=(
        (std_vector *)((char *)this + 8),
        (const std_vector *)((char *)this + 32));
      goto LABEL_29;
    }
    v25 = 0;
    do
    {
      v26 = v25 >> 6;
      v27 = 1LL << v25++;
      *(_QWORD *)(*((_QWORD *)this + 19) + 8 * v26) |= v27;
    }
    while ( *((unsigned __int16 *)this + 3) > (unsigned int)v25 );
    std::vector<SC_ComponentModifier>::operator=(
      (std_vector *)((char *)this + 8),
      (const std_vector *)((char *)this + 32));
    if ( v111 )
      goto LABEL_18;
LABEL_29:
    if ( *((_WORD *)this + 3) )
    {
      v4 = nullptr;
      v43 = v112;
      v44 = 4LL * v112;
      do
      {
        while ( 1 )
        {
          v46 = *((_QWORD *)this + 19);
          v47 = v5[(_QWORD)v4];
          if ( ((1LL << (char)v4) & *(_QWORD *)(v46 + 8 * ((unsigned __int64)v4 >> 6))) == 0 )
            break;
          vDSP_vfill(&v115, v5[(_QWORD)v4], 1, v43, v46);
          v45 = *((unsigned __int16 *)this + 3);
          v4 = (const float **)((char *)v4 + 1);
          if ( v45 <= (unsigned int)v4 )
            goto LABEL_34;
        }
        v48 = 24LL * (_QWORD)v4;
        v4 = (const float **)((char *)v4 + 1);
        memcpy(v47, *(const void **)(*((_QWORD *)this + 27) + v48), v44);
        v45 = *((unsigned __int16 *)this + 3);
      }
      while ( v45 > (unsigned int)v4 );
    }
    else
    {
      v45 = 0;
    }
LABEL_34:
    if ( *((unsigned __int16 *)this + 2) > v45 )
    {
      v49 = v112;
      v5 += v45;
      do
      {
        v50 = (__int64)*v5++;
        ++v45;
        vDSP_vfill(&v115, v50, 1, v49, v28);
      }
      while ( *((unsigned __int16 *)this + 2) > v45 );
    }
    *(_DWORD *)this = 0;
    result = 0;
    v6 = v100;
    v7 = v101;
    v8 = v102;
    v9 = v103;
    v10 = v104;
    v11 = v105;
    v12 = v106;
    v13 = v107;
    goto LABEL_39;
  }
  v102 = v8;
  v103 = v9;
  v104 = v10;
  v105 = v11;
  if ( *((_WORD *)this + 3) )
  {
    v52 = 0;
    do
    {
      v53 = v52 >> 6;
      v15 = 1LL << v52++;
      *(_QWORD *)(*((_QWORD *)this + 19) + 8 * v53) |= v15;
      v54 = *((unsigned __int16 *)this + 3);
    }
    while ( v54 > (unsigned int)v52 );
    if ( !v111 )
      goto LABEL_56;
  }
  else if ( !v111 )
  {
    goto LABEL_61;
  }
  v55 = 0;
  v106 = v12;
  v107 = v13;
  v56 = 4LL * v112;
  v108 = v112;
  v109 = &v117;
  do
  {
    while ( 1 )
    {
      v57 = *((_QWORD *)this + 1) + 36 * v55;
      if ( *(_BYTE *)v57 )
      {
        if ( !*(_BYTE *)(v57 + 1) || (v58 = *(float *)(v57 + 4), v58 == 0.0) )
        {
          v59 = 0;
          while ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v55) > v59 )
          {
            v60 = v59 + *(_DWORD *)(*((_QWORD *)this + 16) + 4 * v55);
            v61 = v60 >> 6;
            v62 = 1LL << v60;
            v63 = v59 + *(_DWORD *)(*((_QWORD *)this + 13) + 4 * v55);
            ++v59;
            v64 = *(void **)(*((_QWORD *)this + 27) + 24LL * v60);
            v65 = v4[v63];
            *(_QWORD *)(*((_QWORD *)this + 19) + 8 * v61) &= ~v62;
            memcpy(v64, v65, v56);
          }
          goto LABEL_54;
        }
        v74 = 0;
        v117 = powf(10.0, v58 * 0.05);
        if ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v55) )
          break;
      }
LABEL_54:
      if ( v111 <= (unsigned int)++v55 )
        goto LABEL_55;
    }
    do
    {
      v75 = *((_QWORD *)this + 19);
      v76 = v74 + *(_DWORD *)(*((_QWORD *)this + 16) + 4 * v55);
      v77 = v76 >> 6;
      v78 = v74 + *(_DWORD *)(*((_QWORD *)this + 13) + 4 * v55);
      ++v74;
      v80 = v108;
      v79 = v109;
      v81 = *(_QWORD *)(v75 + 8 * v77) & ~(1LL << v76);
      v82 = *(_QWORD *)(*((_QWORD *)this + 27) + 24LL * v76);
      v83 = v4[v78];
      *(_QWORD *)(v75 + 8 * v77) = v81;
      vDSP_vsmul(v83, 1, v79, v82, 1, v80);
    }
    while ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v55) > v74 );
    ++v55;
  }
  while ( v111 > (unsigned int)v55 );
LABEL_55:
  v54 = *((unsigned __int16 *)this + 3);
  v12 = v106;
  v13 = v107;
LABEL_56:
  if ( v54 )
  {
    v66 = 0;
    v67 = v112;
    v68 = 4LL * v112;
    do
    {
      while ( 1 )
      {
        v69 = *((_QWORD *)this + 19);
        v70 = v5[v66];
        if ( ((1LL << v66) & *(_QWORD *)(v69 + 8 * (v66 >> 6))) == 0 )
          break;
        vDSP_vfill(&v115, v5[v66], 1, v67, v69);
        v16 = *((unsigned __int16 *)this + 3);
        if ( v16 <= (unsigned int)++v66 )
          goto LABEL_61;
      }
      v71 = 24 * v66++;
      memcpy(v70, *(const void **)(*((_QWORD *)this + 27) + v71), v68);
      v16 = *((unsigned __int16 *)this + 3);
    }
    while ( v16 > (unsigned int)v66 );
  }
LABEL_61:
  if ( *((unsigned __int16 *)this + 2) > v16 )
  {
    v72 = v112;
    v5 += v16;
    do
    {
      v73 = (__int64)*v5++;
      ++v16;
      vDSP_vfill(&v115, v73, 1, v72, v15);
    }
    while ( *((unsigned __int16 *)this + 2) > v16 );
  }
  result = 0;
  v6 = v100;
  v7 = v101;
  v8 = v102;
  v9 = v103;
  v10 = v104;
  v11 = v105;
LABEL_39:
  if ( v118 != _stack_chk_guard )
  {
    v98 = v4;
    v99 = v5;
    v100 = v6;
    v101 = v7;
    v102 = v8;
    v103 = v9;
    v104 = v10;
    v105 = v11;
    v106 = v12;
    v107 = v13;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0009f130  size 1032 bytes  254 insns
 *   SC_SceneProcessor::Initialize(unsigned int, unsigned int, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<unsigned int, std::allocator<unsigned int> > const&, std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&)
 *   [_ZN17SC_SceneProcessor10InitializeEjjRKSt6vectorIjSaIjEES4_S4_RKS0_I20SC_ComponentModifierSaIS5_EE]
 *   data refs: 0x330468, 0x330488, 0x3304b0, 0x42ce00, 0x43e520
 */
__int64 __fastcall SC_SceneProcessor::Initialize(
        SC_SceneProcessor *this,
        unsigned int n,
        unsigned int n2,
        const std_vector *vec,
        const std_vector *vec2,
        const std_vector *vec3,
        const std_vector *vec4)
{
  const std_vector *v7; // x21
  __int64 v8; // x22
  __int64 v9; // x25
  __int64 v10; // x26
  void *begin; // x3
  __int64 v13; // x23
  __int64 v17; // x0
  unsigned int v18; // w2
  int v19; // w1
  int v20; // w3
  unsigned int v21; // w1
  unsigned int v22; // w0
  __int64 v23; // x4
  void *v24; // x1
  __int64 v25; // x0
  __int64 v26; // x2
  unsigned __int64 v27; // x1
  __int64 v28; // x0
  __int64 v29; // x0
  __int64 v30; // x2
  unsigned __int64 v31; // x1
  __int64 v32; // x7
  __int64 v33; // x6
  unsigned int v34; // w0
  int v35; // w1
  unsigned __int64 v36; // x2
  __int64 v37; // x2
  void **v38; // x22
  unsigned int v39; // w4
  unsigned __int64 v40; // x1
  unsigned __int64 v41; // x0
  unsigned int i; // w22
  __int64 v43; // x2
  _QWORD *v44; // x0
  __int64 v45; // x3
  __int64 v46; // x1
  bool v47; // cc
  __int64 v48; // x1
  __int64 result; // x0
  __int64 v50; // x0
  void **v51; // x23
  void **v52; // x21
  const std_vector *v53; // [xsp+20h] [xbp+20h]
  __int64 v54; // [xsp+28h] [xbp+28h]
  __int64 v55; // [xsp+40h] [xbp+40h]
  __int64 v56; // [xsp+48h] [xbp+48h]
  void *v57; // [xsp+58h] [xbp+58h]
  __int64 v58; // [xsp+60h] [xbp+60h]
  __int64 v59; // [xsp+68h] [xbp+68h]
  __int64 v60; // [xsp+70h] [xbp+70h]
  begin = vec->begin;
  v13 = ((char *)vec->end - (char *)begin) >> 2;
  if ( (_DWORD)v13 )
  {
    if ( (_DWORD)v13 == (unsigned int)(((char *)vec2->end - (char *)vec2->begin) >> 2)
      && (_DWORD)v13 == (unsigned int)(((char *)vec3->end - (char *)vec3->begin) >> 2) )
    {
      v54 = v8;
      v7 = vec4;
      *((_WORD *)this + 2) = n;
      *((_WORD *)this + 3) = n2;
      std::vector<unsigned int>::operator=((char *)this + 80, vec);
      std::vector<unsigned int>::operator=((char *)this + 104, vec2);
      std::vector<unsigned int>::operator=((char *)this + 128, vec3);
      v17 = 0;
      v18 = 0;
      do
      {
        v19 = *((_DWORD *)vec3->begin + v17);
        v20 = *((_DWORD *)vec->begin + v17++);
        v21 = v19 + v20;
        if ( v18 < v21 )
          v18 = v21;
      }
      while ( (unsigned int)v13 > (unsigned int)v17 );
      v22 = *((unsigned __int16 *)this + 3);
      if ( v22 < v18 )
      {
        __printf_chk(1, "ERROR: intermediate channel count is too small\n");
        result = 0xFFFFFFFFLL;
        v8 = v54;
        v9 = v55;
        v10 = v56;
      }
      else
      {
        v23 = *((_QWORD *)this + 19);
        v24 = *((void **)this + 21);
        if ( (unsigned __int64)*((unsigned int *)this + 44) + 8 * ((__int64)v24 - v23) <= (unsigned __int16)v22 )
        {
          LODWORD(v58) = *((_DWORD *)this + 44);
          v57 = v24;
          std::vector<bool>::_M_fill_insert((_DWORD)this + 152, v24);
        }
        else
        {
          *((_DWORD *)this + 44) = v22 & 0x3F;
          *((_QWORD *)this + 21) = v23 + 8 * ((__int64)(unsigned __int16)v22 >> 6);
        }
        v25 = *((_QWORD *)this + 7);
        v26 = *((_QWORD *)this + 8);
        v27 = (v26 - v25) >> 1;
        if ( (unsigned int)v13 > v27 )
        {
          std::vector<SC_SceneProcessor::ComponentTransitionFlags>::_M_default_append(
            (std_vector *)((char *)this + 56),
            (unsigned int)v13 - v27);
        }
        else if ( (unsigned int)v13 < v27 )
        {
          v28 = v25 + 2LL * (unsigned int)v13;
          if ( v26 != v28 )
            *((_QWORD *)this + 8) = v28;
        }
        v29 = *((_QWORD *)this + 24);
        v30 = *((_QWORD *)this + 25);
        v31 = (v30 - v29) >> 3;
        if ( (unsigned int)v13 > v31 )
        {
          std::vector<SC_SceneProcessor::Ramp>::_M_default_append((std_vector *)this + 8, (unsigned int)v13 - v31);
        }
        else if ( (unsigned int)v13 < v31 )
        {
          v50 = v29 + 8LL * (unsigned int)v13;
          if ( v30 != v50 )
            *((_QWORD *)this + 25) = v50;
        }
        std::vector<SC_ComponentModifier>::reserve((std_vector *)((char *)this + 32), (unsigned int)v13);
        std::vector<SC_ComponentModifier>::reserve((std_vector *)((char *)this + 8), (unsigned int)v13);
        std::vector<SC_ComponentModifier>::operator=((std_vector *)((char *)this + 8), v7);
        v32 = 0;
        v33 = 0;
        do
        {
          while ( !*(_BYTE *)(*((_QWORD *)this + 1) + v32) || !*(_DWORD *)(*((_QWORD *)this + 10) + 4 * v33) )
          {
            ++v33;
            v32 += 36;
            if ( (unsigned int)v13 <= (unsigned int)v33 )
              goto LABEL_24;
          }
          v34 = 0;
          do
          {
            v35 = *(_DWORD *)(*((_QWORD *)this + 16) + 4 * v33);
            v36 = v34 + v35;
            LOBYTE(v35) = v34++ + v35;
            *(_QWORD *)(*((_QWORD *)this + 19) + 8 * (v36 >> 6)) &= ~(1LL << v35);
          }
          while ( *(_DWORD *)(*((_QWORD *)this + 10) + 4 * v33) > v34 );
          ++v33;
          v32 += 36;
        }
        while ( (unsigned int)v13 > (unsigned int)v33 );
LABEL_24:
        SC_SceneProcessor::Update(this, v7);
        v37 = *((_QWORD *)this + 27);
        v38 = *((void ***)this + 28);
        v39 = *((unsigned __int16 *)this + 3);
        v40 = *((unsigned __int16 *)this + 3);
        v41 = 0xAAAAAAAAAAAAAAABLL * (((__int64)v38 - v37) >> 3);
        if ( v40 > v41 )
        {
          std::vector<std::vector<float>>::_M_default_append((char *)this + 216, v40 - v41);
          v39 = *((unsigned __int16 *)this + 3);
        }
        else if ( v40 < v41 )
        {
          v51 = (void **)(v37 + 24LL * *((unsigned __int16 *)this + 3));
          if ( v38 != v51 )
          {
            v52 = (void **)(v37 + 24LL * *((unsigned __int16 *)this + 3));
            do
            {
              if ( *v52 )
                operator delete(*v52);
              v52 += 3;
            }
            while ( v38 != v52 );
            v39 = *((unsigned __int16 *)this + 3);
            *((_QWORD *)this + 28) = v51;
          }
        }
        v7 = nullptr;
        for ( i = 0; v39 > i; ++v7 )
        {
          while ( 1 )
          {
            v44 = (void **)((char *)&v7->begin + *((_QWORD *)this + 27));
            v45 = v44[1];
            v46 = v45 - *v44;
            v47 = (unsigned __int64)v46 > 0xFFF;
            v48 = v46 >> 2;
            if ( v47 )
              break;
            std::vector<float>::_M_default_append(v44, 1024 - v48);
            v39 = *((unsigned __int16 *)this + 3);
            ++i;
            ++v7;
            if ( v39 <= i )
              goto LABEL_34;
          }
          if ( v48 != 1024 )
          {
            v43 = *v44 + 4096LL;
            if ( v45 != v43 )
            {
              v44[1] = v43;
              v39 = *((unsigned __int16 *)this + 3);
            }
          }
          ++i;
        }
LABEL_34:
        v8 = v54;
        result = 0;
        v9 = v55;
        v10 = v56;
      }
    }
    else
    {
      __printf_chk(1, "ERROR: Inconsistent vector size\n", n2);
      result = 0xFFFFFFFFLL;
      v9 = v55;
      v10 = v56;
    }
  }
  else
  {
    __printf_chk(1, "ERROR: No scene component\n", n2, begin, vec2, vec3, vec4);
    result = 0xFFFFFFFFLL;
  }
  if ( v59 != _stack_chk_guard )
  {
    v53 = v7;
    v54 = v8;
    v55 = v9;
    v56 = v10;
  }
  return result;
}


