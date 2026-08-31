/* ============================================================================
 * apac::LPCBase
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 1208 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b4190    132  apac::LPCBase::LPC_Weighting(float const*, float*, float, unsigned short) */
/* 0x000b4620    400  apac::LPCBase::LevinsonDurbin(float const*) */
/* 0x000b4930    676  apac::LPCBase::ComputeLPCs(float const*, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000b4190  size 132 bytes  33 insns
 *   apac::LPCBase::LPC_Weighting(float const*, float*, float, unsigned short)
 *   [_ZN4apac7LPCBase13LPC_WeightingEPKfPfft]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPCBase::LPC_Weighting(
        apac__LPCBase *this,
        const float *a,
        float *buf,
        float f,
        unsigned __int16 v)
{
  float v5; // s2
  unsigned __int64 v6; // x0
  __int64 v7; // x3
  float v8; // s1
  __int64 v10; // [xsp+18h] [xbp+18h]
  __int64 v11; // [xsp+20h] [xbp+20h]
  *buf = *a;
  if ( v )
  {
    v5 = f;
    v6 = 4;
    v7 = 4 * ((unsigned int)v - 1 + 2LL);
    do
    {
      v8 = a[v6 / 4] * v5;
      v5 = v5 * f;
      buf[v6 / 4] = v8;
      v6 += 4LL;
    }
    while ( v7 != v6 );
  }
  return v10 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4620  size 400 bytes  99 insns
 *   apac::LPCBase::LevinsonDurbin(float const*)
 *   [_ZN4apac7LPCBase14LevinsonDurbinEPKf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPCBase::LevinsonDurbin(apac__LPCBase *this, const float *a)
{
  _QWORD *v2; // x3
  int v3; // w2
  __int64 v4; // x4
  __int64 v5; // x2
  _QWORD *v6; // x5
  float v7; // s5
  float v8; // s0
  unsigned int v9; // w8
  float v10; // s0
  __int64 v11; // x7
  __int64 v12; // x9
  float v13; // s1
  float v14; // s5
  float v15; // s5
  float v16; // s3
  __int64 v17; // x3
  __int64 v18; // x2
  __int64 v19; // x5
  float v20; // s0
  __int64 v21; // x2
  unsigned int v22; // w4
  float v23; // s2
  __int64 v24; // x5
  float v25; // s0
  float v26; // s1
  _QWORD v28[4]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v29; // [xsp+20h] [xbp+20h]
  v2 = &v28[-1536];
  v3 = 4 * *((unsigned __int16 *)this + 4) + 15;
  v4 = *(_QWORD *)&v3 & 0x7F000LL;
  v5 = *(_QWORD *)&v3 & 0x7FFF0LL;
  v6 = (_QWORD *)((char *)&v28[-1536] - v4);
  if ( &v28[-1536] != v6 )
  {
    do
    {
      v2 -= 512;
      *v2 = 0;
    }
    while ( v2 != v6 );
  }
  *(_QWORD *)((char *)v2 - (v5 - v4)) = 0;
  v7 = *a;
  v8 = -a[1];
  **((float **)this + 3) = 1.0;
  v9 = 1;
  v10 = v8 / v7;
  v11 = 2;
  v12 = 8;
  *(float *)(*((_QWORD *)this + 3) + 4LL) = v10;
  v14 = *a;
  v13 = a[1];
  *(float *)v28 = v10;
  v15 = v14 + (float)(v10 * v13);
  if ( *((unsigned __int16 *)this + 4) > 1u )
  {
    do
    {
      v16 = 0.0;
      v17 = *((_QWORD *)this + 3);
      v18 = 0;
      do
      {
        v19 = (unsigned int)(v11 - v18);
        v20 = *(float *)(v17 + 4 * v18++);
        v16 = v16 + (float)(a[v19] * v20);
      }
      while ( v11 != v18 );
      v21 = 4;
      v22 = v9;
      v23 = (float)-v16 / v15;
      *((float *)v28 + v9) = v23;
      while ( 1 )
      {
        v24 = v22;
        v25 = *(float *)(v17 + v21);
        --v22;
        v26 = *(float *)(v17 + 4 * v24);
        *(float *)(v17 + 4 * v24) = v26 + (float)(v23 * v25);
        *(float *)(*((_QWORD *)this + 3) + v21) = v25 + (float)(v23 * v26);
        v21 += 4;
        if ( (_DWORD)v11 - 1 - ((unsigned int)v11 >> 1) == v22 )
          break;
        v17 = *((_QWORD *)this + 3);
      }
      v15 = v15 + (float)(v23 * v16);
      ++v11;
      ++v9;
      *(float *)(*((_QWORD *)this + 3) + v12) = v23;
      v12 += 4;
      if ( v15 <= 0.0 )
        v15 = 0.01;
    }
    while ( *((unsigned __int16 *)this + 4) >= (unsigned int)v11 );
  }
  return v28[3] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4930  size 676 bytes  165 insns
 *   apac::LPCBase::ComputeLPCs(float const*, bool)
 *   [_ZN4apac7LPCBase11ComputeLPCsEPKfb]
 *   data refs: 0x332f80, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPCBase::ComputeLPCs(apac__LPCBase *this, const float *a, bool flag)
{
  _QWORD *v3; // x3
  _BOOL4 v5; // w24
  int v6; // w0
  __int64 v7; // x2
  __int64 v8; // x0
  _QWORD *v9; // x4
  __int64 v10; // x22
  _QWORD *v11; // x4
  __int64 v12; // x3
  int v13; // w26
  __int64 v14; // x0
  unsigned __int64 v15; // x9
  __int64 v16; // x2
  unsigned __int64 v17; // x5
  unsigned __int64 v18; // x2
  _QWORD *v19; // x6
  __int64 v20; // x8
  unsigned int v21; // w2
  __int64 i; // x4
  __int64 v23; // x7
  __int64 v24; // x6
  __int64 v25; // x5
  float v26; // s2
  float v27; // s0
  float v28; // s4
  float v29; // s1
  int v30; // w2
  __int64 v31; // x3
  float v32; // s2
  __int64 v33; // x1
  float j; // s1
  float v35; // s0
  unsigned int v36; // w0
  __int64 v37; // x1
  float k; // s1
  _QWORD v40[12]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v41; // [xsp+60h] [xbp+60h]
  v3 = &v40[-1536];
  v5 = flag;
  v6 = 4 * *((unsigned __int16 *)this + 4) + 19;
  v7 = *(_QWORD *)&v6 & 0x7F000LL;
  v8 = *(_QWORD *)&v6 & 0x7FFF0LL;
  v9 = (_QWORD *)((char *)&v40[-1536] - v7);
  if ( &v40[-1536] != v9 )
  {
    do
    {
      v3 -= 512;
      *v3 = 0;
    }
    while ( v3 != v9 );
  }
  *(_QWORD *)((char *)v3 - (v8 - v7)) = 0;
  v10 = *((_QWORD *)this + 9);
  v11 = &v40[-1536];
  v12 = *(_QWORD *)(v10 + 16);
  v13 = *(unsigned __int16 *)(v10 + 8);
  v14 = *(_QWORD *)(v10 + 24) - v12;
  v15 = v14 >> 2;
  v16 = 4 * ((v14 >> 2) + (unsigned __int16)v13) + 15;
  v17 = v16 & 0xFFFFFFFFFFFFF000LL;
  v18 = v16 & 0xFFFFFFFFFFFFFFF0LL;
  v19 = (_QWORD *)((char *)&v40[-1536] - v17);
  if ( &v40[-1536] != v19 )
  {
    do
    {
      v11 -= 512;
      *v11 = 0;
    }
    while ( v11 != v19 );
  }
  *(_QWORD *)((char *)v11 - (v18 - v17)) = 0;
  v20 = 0;
  v21 = 0;
  for ( i = 0; v15 > v21; *((float *)v40 + v25) = v27 * v28 )
  {
    v23 = v21 + 1;
    v24 = v21 + 2;
    v25 = v21 + 3;
    v21 += 4;
    v26 = a[v23] * *(float *)(v12 + 4 * v23);
    v27 = a[v25];
    v28 = *(float *)(v12 + 4 * v25);
    v29 = a[v24] * *(float *)(v12 + 4 * v24);
    *((float *)v40 + i) = a[v20] * *(float *)(v12 + 4 * v20);
    i = v21;
    *((float *)v40 + v23) = v26;
    v20 = v21;
    *((float *)v40 + v24) = v29;
  }
  memset((char *)v40 + v14, 0, 4LL * (unsigned __int16)v13);
  memset(v40, 0, 4LL * ((v13 + 1) & 0x1FFFF));
  v30 = 0;
  v31 = 0;
  if ( *(_QWORD *)(v10 + 16) != *(_QWORD *)(v10 + 24) )
  {
    do
    {
      v32 = *((float *)v40 + v31);
      v33 = 0;
      for ( j = v32; ; j = *((float *)v40 + (unsigned int)(v30 + v33)) )
      {
        *((float *)v40 + v33) = *((float *)v40 + v33) + (float)(v32 * j);
        if ( *(unsigned __int16 *)(v10 + 8) < (unsigned int)++v33 )
          break;
      }
      v31 = (unsigned int)(v30 + 1);
      v30 = v31;
    }
    while ( (__int64)(*(_QWORD *)(v10 + 24) - *(_QWORD *)(v10 + 16)) >> 2 > (unsigned __int64)(unsigned int)v31 );
  }
  if ( *(float *)v40 < 1.0 )
    v35 = 1.0001;
  else
    v35 = *(float *)v40 * 1.0001;
  *(float *)v40 = v35;
  if ( v5 )
  {
    v36 = *((unsigned __int16 *)this + 4);
    if ( *((_WORD *)this + 4) )
    {
      v37 = 1;
      for ( k = 0.99941; ; k = flt_332F80[v37++] )
      {
        *((float *)v40 + v37) = *((float *)v40 + v37) * k;
        if ( v36 <= (unsigned int)v37 )
          break;
      }
    }
  }
  (*(void (__fastcall **)(apac__LPCBase *, _QWORD *))(*(_QWORD *)this + 48LL))(this, v40);
  return v40[11] ^ _stack_chk_guard;
}


