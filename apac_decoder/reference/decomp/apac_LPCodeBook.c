/* ============================================================================
 * apac::LPCodeBook
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 948 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b54b0    616  apac::LPCodeBook::VQSearch(float const*) */
/* 0x000b5720    332  apac::LPCodeBook::LSF_Quant_Weighting(float const*, short) */

/* ------------------------------------------------------------------------ */
/* 0x000b54b0  size 616 bytes  151 insns
 *   apac::LPCodeBook::VQSearch(float const*)
 *   [_ZN4apac10LPCodeBook8VQSearchEPKf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPCodeBook::VQSearch(apac__LPCodeBook *this, const float *a)
{
  unsigned int v2; // w4
  unsigned int v3; // w8
  __int64 v4; // x3
  int v5; // w7
  float v6; // s4
  int v7; // w6
  float v8; // s2
  __int64 v9; // x2
  float v10; // s0
  float v11; // s3
  float v12; // s1
  unsigned __int64 *v14; // x2
  unsigned __int64 v15; // x5
  unsigned __int64 v16; // x5
  unsigned __int64 v17; // x5
  unsigned __int64 v18; // x5
  float32x4_t v19; // q6
  float32x4_t v20; // q2
  float32x4_t v21; // q0
  int v22; // w1
  float32x4_t v23; // q19
  float32x4_t v24; // q18
  unsigned __int64 v25; // x5
  unsigned __int64 v26; // x5
  float32x4_t v27; // q1
  unsigned __int64 v28; // x5
  float32x4_t v29; // q23
  float32x4_t v30; // q5
  float32x4_t v31; // q24
  float32x4_t v32; // q22
  float32x4_t v33; // q23
  float32x4_t v34; // q3
  float32x4_t v35; // q2
  float v36; // s1
  float v37; // s0
  float32x4_t v38; // [xsp+10h] [xbp+10h]
  float32x4_t v39; // [xsp+20h] [xbp+20h]
  float32x4_t v40; // [xsp+30h] [xbp+30h]
  float32x4_t v41; // [xsp+40h] [xbp+40h]
  float32x4_t v42; // [xsp+50h] [xbp+50h]
  float32x4_t v43; // [xsp+60h] [xbp+60h]
  float32x4_t v44; // [xsp+70h] [xbp+70h]
  float32x4_t v45; // [xsp+80h] [xbp+80h]
  float32x4_t v46; // [xsp+98h] [xbp+98h]
  float32x4_t v47; // [xsp+A8h] [xbp+A8h]
  float32x4_t v48; // [xsp+B8h] [xbp+B8h]
  float32x4_t v49; // [xsp+C8h] [xbp+C8h]
  __int64 v50; // [xsp+D8h] [xbp+D8h]
  __int64 v51; // [xsp+E0h] [xbp+E0h]
  v2 = *((unsigned __int16 *)this + 1);
  v3 = *(unsigned __int16 *)this;
  v4 = *((_QWORD *)this + 1);
  if ( v2 != 16 )
  {
    if ( *(_WORD *)this )
    {
      v5 = 0;
      v6 = 1.0e30;
      v7 = 0;
      do
      {
        v8 = 0.0;
        if ( *((_WORD *)this + 1) )
        {
          v9 = 0;
          do
          {
            v10 = a[v9];
            v11 = *(float *)(v4 + 4 * v9);
            v12 = *(float *)(*((_QWORD *)this + 6) + 4 * v9++);
            v8 = v8 + (float)((float)((float)(v10 - v11) * v12) * (float)(v10 - v11));
          }
          while ( v2 > (unsigned __int16)v9 );
          v4 += 4LL * (unsigned __int16)(v2 - 1) + 4;
        }
        if ( v6 > v8 )
          v7 = v5;
        ++v5;
        if ( v6 > v8 )
          v6 = v8;
      }
      while ( v3 > (unsigned __int16)v5 );
      goto LABEL_13;
    }
LABEL_22:
    v7 = 0;
    v6 = 1.0e30;
    goto LABEL_13;
  }
  v14 = *((unsigned __int64 **)this + 6);
  v15 = v14[1];
  v46.n128_u64[0] = *v14;
  v46.n128_u64[1] = v15;
  v16 = v14[3];
  v47.n128_u64[0] = v14[2];
  v47.n128_u64[1] = v16;
  v17 = v14[5];
  v48.n128_u64[0] = v14[4];
  v48.n128_u64[1] = v17;
  v18 = v14[7];
  v49.n128_u64[0] = v14[6];
  v49.n128_u64[1] = v18;
  if ( !v3 )
    goto LABEL_22;
  v19 = *(float32x4_t *)(a + 4);
  v7 = 0;
  v20 = *(float32x4_t *)(a + 8);
  v6 = 1.0e30;
  v21 = *(float32x4_t *)(a + 12);
  v38 = *(float32x4_t *)a;
  v22 = 0;
  v39 = v19;
  v40 = v20;
  v23 = v20;
  v41 = v21;
  v24 = v21;
  do
  {
    v4 += 64;
    v25 = *(_QWORD *)(v4 - 56);
    v42.n128_u64[0] = *(_QWORD *)(v4 - 64);
    v42.n128_u64[1] = v25;
    v26 = *(_QWORD *)(v4 - 40);
    v43.n128_u64[0] = *(_QWORD *)(v4 - 48);
    v43.n128_u64[1] = v26;
    v27 = vsubq_f32(v38, v42);
    v28 = *(_QWORD *)(v4 - 24);
    v44.n128_u64[0] = *(_QWORD *)(v4 - 32);
    v44.n128_u64[1] = v28;
    v29 = vsubq_f32(v19, v43);
    v30 = vmulq_f32(vmulq_f32(v27, v46), v27);
    v31 = vsubq_f32(v23, v44);
    v45 = *(float32x4_t *)(v4 - 16);
    v32 = vmulq_f32(vmulq_f32(v29, v47), v29);
    v33 = vsubq_f32(v24, v45);
    v34 = vmulq_f32(vmulq_f32(v31, v48), v31);
    v35 = vmulq_f32(vmulq_f32(v33, v49), v33);
    v36 = (float)((float)((float)((float)((float)((float)((float)(v30.n128_f32[0] + v30.n128_f32[1]) + v30.n128_f32[2])
                                                + v30.n128_f32[3])
                                        + v32.n128_f32[0])
                                + v32.n128_f32[1])
                        + v32.n128_f32[2])
                + v32.n128_f32[3])
        + v34.n128_f32[0];
    v37 = (float)((float)((float)((float)((float)((float)(v36 + v34.n128_f32[1]) + v34.n128_f32[2]) + v34.n128_f32[3])
                                + v35.n128_f32[0])
                        + v35.n128_f32[1])
                + v35.n128_f32[2])
        + v35.n128_f32[3];
    if ( v37 < v6 )
      v7 = v22;
    ++v22;
    if ( v37 < v6 )
      v6 = (float)((float)((float)((float)((float)((float)(v36 + v34.n128_f32[1]) + v34.n128_f32[2]) + v34.n128_f32[3])
                                 + v35.n128_f32[0])
                         + v35.n128_f32[1])
                 + v35.n128_f32[2])
         + v35.n128_f32[3];
  }
  while ( v3 > (unsigned __int16)v22 );
LABEL_13:
  **((_DWORD **)this + 2) = v7;
  *((float *)this + 10) = v6;
  return v50 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5720  size 332 bytes  83 insns
 *   apac::LPCodeBook::LSF_Quant_Weighting(float const*, short)
 *   [_ZN4apac10LPCodeBook19LSF_Quant_WeightingEPKfs]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LPCodeBook::LSF_Quant_Weighting(apac__LPCodeBook *this, const float *a, __int16 v)
{
  float *v3; // x3
  float *v4; // x4
  float v5; // s3
  int v6; // w10
  float v7; // s0
  int v8; // w2
  __int64 v9; // x4
  int v10; // w7
  int v11; // w9
  __int64 v12; // x6
  float v13; // s1
  float v14; // s3
  float v15; // s1
  __int64 v17; // [xsp+18h] [xbp+18h]
  __int64 v18; // [xsp+20h] [xbp+20h]
  v3 = *((float **)this + 6);
  v4 = *((float **)this + 7);
  if ( v4 != v3 )
  {
    do
      *v3++ = 0.0;
    while ( v3 != v4 );
    v3 = *((float **)this + 6);
  }
  v5 = *a;
  v6 = v - 2;
  v7 = 1.0 / *a;
  if ( v6 <= 1 )
  {
    v12 = 1;
  }
  else
  {
    LOWORD(v8) = 1;
    v9 = 1;
    v10 = 1;
    while ( 1 )
    {
      v11 = v10 - 1;
      v8 = (__int16)(v8 + 2);
      v10 = v8;
      v12 = v8;
      v13 = 1.0 / (float)(a[v9] - v5);
      v3[v11] = v13 + v7;
      v7 = 1.0 / (float)(a[v9 + 1] - a[v9]);
      *(float *)(*((_QWORD *)this + 6) + 4 * v9) = v13 + v7;
      v9 = v8;
      if ( v8 >= v6 )
        break;
      v3 = *((float **)this + 6);
      v5 = a[v12 - 1];
    }
    v5 = a[v12 - 1];
    v3 = (float *)(*((_QWORD *)this + 6) + 4LL * (v8 - 1));
  }
  v14 = 1.0 / (float)(a[v12] - v5);
  *v3 = v14 + v7;
  v15 = 1.0 / (0.5 - a[v12]);
  *(float *)(*((_QWORD *)this + 6) + v12 * 4) = v15 + v14;
  return v17 ^ _stack_chk_guard;
}


