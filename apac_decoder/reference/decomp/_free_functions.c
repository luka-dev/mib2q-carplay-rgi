/* ============================================================================
 * _free_functions
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 33 function(s), 14380 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00070c28     68  sub_70C28 */
/* 0x00070cf4     72  sub_70CF4 */
/* 0x00071fc0     64  DRCLoggingRegisterScopeOnce() */
/* 0x00078370    288  sub_78370 */
/* 0x0007b070   1316  sub_7B070 */
/* 0x00096850   2736  sub_96850 */
/* 0x00097350    348  sub_97350 */
/* 0x000974b0    308  sub_974B0 */
/* 0x00097670    592  sub_97670 */
/* 0x00099650    836  int GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, __gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::vector<apac::observer_ptr<apac::IASCConfig const>, std::allocator<apac::observer_ptr<apac::IASCConfig const> > >&) */
/* 0x000aec20    232  sub_AEC20 */
/* 0x000b19b0    316  sub_B19B0 */
/* 0x000b1d90    724  InterleaveConvert(float const* const*, void* const*, unsigned int, unsigned int, bool, bool, float) */
/* 0x000b33b0    104  GetDCTIVModulationCoefficients */
/* 0x000b40b0    220  sub_B40B0 */
/* 0x00105590    104  GetSineWindowCoefficientPtr */
/* 0x00105600    108  GetKBDWindowCoefficientPtr */
/* 0x00105670    380  sub_105670 */
/* 0x001057f0    244  sub_1057F0 */
/* 0x001058f0    704  sub_1058F0 */
/* 0x00105bb0    552  APACDecoder_DecodeFrame */
/* 0x00105de0    132  APACDecoder_Reset */
/* 0x00105e70    440  APACDecoder_GetParameter */
/* 0x00106030    124  APACDecoder_GetCookieOffset */
/* 0x001060b0     80  APACDecoder_GetVersion */
/* 0x00106100     80  APACDecoder_GetProfile */
/* 0x00106150     92  APACDecoder_SampleRate */
/* 0x001061b0     88  APACDecoder_FrameLength */
/* 0x00106210     80  APACDecoder_NumberOfChannels */
/* 0x00106260    552  APACDecoder_SetParameter */
/* 0x00106490    876  APACDecoder_Configure */
/* 0x00106800   1028  APACDecoder_Create */
/* 0x00106c10    492  APACDecoder_Dispose */

/* ------------------------------------------------------------------------ */
/* 0x00070c28  size 68 bytes  17 insns
 *   sub_70C28
 *   [sub_70C28]
 *   data refs: 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
void __fastcall __noreturn sub_70C28(
        __int64 a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  _DWORD *exception; // x0
  exception = __cxa_allocate_exception(4u);
  *exception = 3;
  __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
}


/* ------------------------------------------------------------------------ */
/* 0x00070cf4  size 72 bytes  17 insns
 *   sub_70CF4
 *   [sub_70CF4]
 *   data refs: 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
void __fastcall __noreturn sub_70CF4(
        __int64 a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  _DWORD *exception; // x0
  exception = __cxa_allocate_exception(4u);
  *exception = 561211770;
  __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
}


/* ------------------------------------------------------------------------ */
/* 0x00071fc0  size 64 bytes  16 insns
 *   DRCLoggingRegisterScopeOnce()
 *   [_Z27DRCLoggingRegisterScopeOncev]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DRCLoggingRegisterScopeOnce()
{
  __int64 v1; // [xsp+18h] [xbp+18h]
  __int64 v2; // [xsp+20h] [xbp+20h]
  return v1 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00078370  size 288 bytes  72 insns
 *   sub_78370
 *   [sub_78370]
 *   data refs: 0x42ce00, 0x43e520
 */
float *__fastcall sub_78370(
        float *a1,
        float *a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  float *v9; // x2
  __int64 v10; // x0
  float *v11; // x0
  float *result; // x0
  __int64 v13; // x4
  v9 = a1;
  v10 = (char *)a2 - (char *)a1;
  if ( v10 >> 4 <= 0 )
  {
    v13 = v10 >> 2;
  }
  else
  {
    v11 = &v9[4 * (v10 >> 4)];
    do
    {
      if ( *v9 != 0.0 )
        return v9;
      if ( v9[1] != 0.0 )
        return v9 + 1;
      if ( v9[2] != 0.0 )
        return v9 + 2;
      if ( v9[3] != 0.0 )
        return v9 + 3;
      v9 += 4;
    }
    while ( v9 != v11 );
    v13 = a2 - v9;
  }
  if ( v13 != 2 )
  {
    if ( v13 != 3 )
    {
      result = a2;
      if ( v13 != 1 )
        return result;
      goto LABEL_17;
    }
    result = v9;
    if ( *v9 != 0.0 )
      return result;
    ++v9;
  }
  result = v9;
  if ( *v9 != 0.0 )
    return result;
  ++v9;
LABEL_17:
  if ( *v9 == 0.0 )
    return a2;
  else
    return v9;
}


/* ------------------------------------------------------------------------ */
/* 0x0007b070  size 1316 bytes  329 insns
 *   sub_7B070
 *   [sub_7B070]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_7B070(
        char a1,
        float32x4_t *a2,
        float32x4_t *a3,
        unsigned __int64 a4,
        double a5,
        double a6,
        long double a7,
        __int64 a8,
        __int64 a9,
        __int64 a10,
        __int64 a11,
        __int64 a12)
{
  __int64 v12; // x19
  unsigned __int64 v13; // x20
  __int64 v14; // x22
  __int64 v15; // x23
  int32x2_t v16; // d17
  int32x4_t v17; // q3
  int32x4_t v18; // q17
  unsigned __int64 v19; // x6
  float32x4_t *v20; // x5
  float32x4_t *v21; // x4
  unsigned __int64 v22; // x9
  unsigned __int64 v23; // x9
  unsigned __int64 v24; // x9
  unsigned __int64 v25; // x9
  float32x4_t v26; // q16
  unsigned __int64 v27; // x9
  unsigned __int64 v28; // x9
  float32x4_t v29; // q21
  float32x4_t v30; // q20
  float32x4_t v31; // q18
  float32x4_t v32; // q19
  float32x4_t v33; // q7
  unsigned __int64 v34; // x23
  float32x4_t v35; // kr00_16
  float32x4_t v36; // q22
  float32x4_t v37; // q20
  float32x4_t v38; // q2
  float32x4_t v39; // q3
  float32x4_t v40; // q16
  float32x4_t v41; // q5
  float32x4_t v42; // q2
  float32x4_t v43; // q6
  float32x4_t *v44; // x4
  float32x4_t *v45; // x0
  float32x4_t v46; // q4
  float32x4_t v47; // q3
  float32x4_t *v48; // x2
  float32x4_t *v49; // x1
  __int64 v50; // x0
  float v51; // s2
  float v52; // s4
  __int64 result; // x0
  int32x4_t v54; // q3
  int32x4_t v55; // q17
  unsigned __int64 v56; // x30
  unsigned __int64 v57; // x10
  float32x4_t *v58; // x4
  float32x4_t *v59; // x5
  unsigned __int64 v60; // x7
  unsigned __int64 v61; // x7
  unsigned __int64 v62; // x7
  unsigned __int64 v63; // x7
  float32x4_t v64; // q16
  unsigned __int64 v65; // x7
  unsigned __int64 v66; // x7
  float32x4_t v67; // kr10_16
  float32x4_t v68; // kr20_16
  float32x4_t v69; // kr30_16
  float32x4_t v70; // kr40_16
  unsigned __int64 v71; // x23
  float32x4_t v72; // kr50_16
  unsigned __int64 v73; // x23
  float32x4_t v74; // kr60_16
  float32x4_t v75; // q22
  float32x4_t v76; // q20
  float32x4_t v77; // q2
  float32x4_t v78; // q3
  float32x4_t v79; // q16
  float32x4_t v80; // q5
  float32x4_t v81; // q2
  float32x4_t v82; // q6
  float32x4_t *v83; // x4
  float32x4_t *v84; // x0
  float32x4_t v85; // q4
  float32x4_t v86; // q3
  float32x4_t *v87; // x1
  float32x4_t *v88; // x2
  __int64 i; // x0
  float v90; // s2
  float v91; // s4
  __int64 v92; // [xsp+10h] [xbp+10h]
  unsigned __int64 v93; // [xsp+18h] [xbp+18h]
  __int64 v94; // [xsp+20h] [xbp+20h]
  __int64 v95; // [xsp+28h] [xbp+28h]
  float32x4_t v96; // [xsp+30h] [xbp+30h]
  float32x4_t v97; // [xsp+40h] [xbp+40h]
  float32x4_t v98; // [xsp+50h] [xbp+50h]
  float32x4_t v99; // [xsp+60h] [xbp+60h]
  float32x4_t v100; // [xsp+70h] [xbp+70h]
  float32x4_t v101; // [xsp+80h] [xbp+80h]
  float32x4_t v102; // [xsp+90h] [xbp+90h]
  float32x4_t v103; // [xsp+A0h] [xbp+A0h]
  float32x4_t v104; // [xsp+B0h] [xbp+B0h]
  float32x4_t v105; // [xsp+C0h] [xbp+C0h]
  float32x4_t v106; // [xsp+110h] [xbp+110h]
  float32x4_t v107; // [xsp+120h] [xbp+120h]
  float32x4_t v108; // [xsp+130h] [xbp+130h]
  float32x4_t v109; // [xsp+140h] [xbp+140h]
  float32x4_t v110; // [xsp+190h] [xbp+190h]
  float32x4_t v111; // [xsp+1A0h] [xbp+1A0h]
  float32x4_t v112; // [xsp+1B0h] [xbp+1B0h]
  float32x4_t v113; // [xsp+1C0h] [xbp+1C0h]
  float32x4_t v114; // [xsp+1D0h] [xbp+1D0h]
  float32x4_t v115; // [xsp+1E0h] [xbp+1E0h]
  float32x4_t v116; // [xsp+1F0h] [xbp+1F0h]
  float32x4_t v117; // [xsp+200h] [xbp+200h]
  float32x4_t v118; // [xsp+210h] [xbp+210h]
  float32x4_t v119; // [xsp+220h] [xbp+220h]
  float32x4_t v120; // [xsp+230h] [xbp+230h]
  float32x4_t v121; // [xsp+240h] [xbp+240h]
  float32x4_t v122; // [xsp+290h] [xbp+290h]
  float32x4_t v123; // [xsp+2A0h] [xbp+2A0h]
  float32x4_t v124; // [xsp+2B0h] [xbp+2B0h]
  float32x4_t v125; // [xsp+2C0h] [xbp+2C0h]
  float32x4_t v126; // [xsp+2D0h] [xbp+2D0h]
  float32x4_t v127; // [xsp+2E0h] [xbp+2E0h]
  float32x4_t v128; // [xsp+2F0h] [xbp+2F0h]
  float32x4_t v129; // [xsp+300h] [xbp+300h]
  float32x4_t v130; // [xsp+310h] [xbp+310h]
  float32x4_t v131; // [xsp+320h] [xbp+320h]
  float32x4_t v132; // [xsp+330h] [xbp+330h]
  float32x4_t v133; // [xsp+340h] [xbp+340h]
  __int64 v134; // [xsp+398h] [xbp+398h]
  if ( a1 )
  {
    if ( a4 > 0xF )
    {
      v16.n64_f32[0] = -*(float *)&a5;
      v17 = vdupq_lane_s32(*(int32x2_t *)&a5, 0);
      v18 = vdupq_lane_s32(v16, 0);
      a7 = COERCE_LONG_DOUBLE(vdupq_lane_s32(*(int32x2_t *)&a6, 0));
      v13 = (((a4 - 16) >> 4) + 1) << 6;
      v19 = 0;
      v94 = v14;
      v95 = v15;
      do
      {
        v20 = &a2[v19 / 0x10];
        v21 = &a3[v19 / 0x10];
        v19 += 64LL;
        v22 = v20->n128_u64[1];
        v110.n128_u64[0] = v20->n128_u64[0];
        v110.n128_u64[1] = v22;
        v23 = v20[1].n128_u64[1];
        v111.n128_u64[0] = v20[1].n128_u64[0];
        v111.n128_u64[1] = v23;
        v24 = v20[2].n128_u64[1];
        v112.n128_u64[0] = v20[2].n128_u64[0];
        v112.n128_u64[1] = v24;
        v25 = v21->n128_u64[1];
        v26 = v20[3];
        v114.n128_u64[0] = v21->n128_u64[0];
        v114.n128_u64[1] = v25;
        v113 = v26;
        v27 = v21[1].n128_u64[1];
        v115.n128_u64[0] = v21[1].n128_u64[0];
        v115.n128_u64[1] = v27;
        v28 = v21[2].n128_u64[1];
        v116.n128_u64[0] = v21[2].n128_u64[0];
        v116.n128_u64[1] = v28;
        v117 = v21[3];
        v29 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v117), v26, v17);
        v30 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v115), v17, v111);
        v31 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v116), v17, v112);
        v32 = vmlaq_f32(vmulq_f32(v114, *(float32x4_t *)&a7), v110, v17);
        v118 = v32;
        v119 = v30;
        v120 = v31;
        v33 = vmlaq_f32(vmulq_f32(v18, v114), *(float32x4_t *)&a7, v110);
        v121 = v29;
        v96 = v33;
        v97 = vmlaq_f32(vmulq_f32(v18, v115), *(float32x4_t *)&a7, v111);
        v98 = vmlaq_f32(vmulq_f32(v18, v116), *(float32x4_t *)&a7, v112);
        v99 = vmlaq_f32(vmulq_f32(v18, v117), v26, *(float32x4_t *)&a7);
        v20[1] = v97;
        v34 = v98.n128_u64[1];
        v20[2].n128_u64[0] = v98.n128_u64[0];
        v20[2].n128_u64[1] = v34;
        v35 = v99;
        *v20 = v33;
        v20[3] = v35;
        *v21 = v32;
        v21[1] = v30;
        v21[2] = v31;
        v21[3] = v29;
      }
      while ( v13 != v19 );
      a2 = (float32x4_t *)((char *)a2 + v13);
      a3 = (float32x4_t *)((char *)a3 + v13);
      a4 &= 0xFu;
      v14 = v94;
      v15 = v95;
    }
    if ( a4 <= 7 )
    {
      v45 = a3;
      v44 = a2;
    }
    else
    {
      *(float *)&a7 = -*(float *)&a5;
      v36 = *a3;
      v37 = a3[1];
      a4 -= 8LL;
      v38 = vdupq_lane_s32(*(int32x2_t *)&a7, 0);
      v39 = vdupq_lane_s32(*(int32x2_t *)&a6, 0);
      v40 = a2[1];
      v102 = *a2;
      v103 = v40;
      v104 = v36;
      v105 = v37;
      v41 = vmlaq_f32(vmulq_f32(v38, v36), v39, v102);
      v42 = vmlaq_f32(vmulq_f32(v38, v37), v39, v40);
      v43 = vmlaq_n_f32(vmulq_f32(v39, v36), v102, *(float *)&a5);
      *a2 = v41;
      v44 = a2 + 2;
      a2[1] = v42;
      *a3 = v43;
      v45 = a3 + 2;
      v100 = v41;
      v101 = v42;
      a3[1] = vmlaq_n_f32(vmulq_f32(v39, v37), v40, *(float *)&a5);
    }
    if ( a4 <= 3 )
    {
      v49 = v45;
      v48 = v44;
    }
    else
    {
      v46 = vdupq_lane_s32(*(int32x2_t *)&a6, 0);
      a4 -= 4LL;
      v47 = vmlaq_n_f32(vmulq_f32(v46, *v45), *v44, *(float *)&a5);
      *v44 = vmlaq_f32(vmulq_n_f32(*v45, -*(float *)&a5), *v44, v46);
      v48 = v44 + 1;
      *v45 = v47;
      v49 = v45 + 1;
    }
    v50 = 0;
    if ( a4 )
    {
      do
      {
        v51 = v49->n128_f32[v50];
        v52 = v48->n128_f32[v50];
        v48->n128_f32[v50] = (float)(v51 * (float)-*(float *)&a5) + (float)(*(float *)&a6 * v52);
        v49->n128_f32[v50++] = (float)(*(float *)&a6 * v51) + (float)(*(float *)&a5 * v52);
      }
      while ( v50 != a4 );
    }
  }
  else
  {
    if ( a4 > 0xF )
    {
      v16.n64_f32[0] = -*(float *)&a5;
      v54 = vdupq_lane_s32(*(int32x2_t *)&a5, 0);
      v55 = vdupq_lane_s32(v16, 0);
      a7 = COERCE_LONG_DOUBLE(vdupq_lane_s32(*(int32x2_t *)&a6, 0));
      v56 = (((a4 - 16) >> 4) + 1) << 6;
      v57 = 0;
      v94 = v14;
      v95 = v15;
      do
      {
        v58 = &a2[v57 / 0x10];
        v59 = &a3[v57 / 0x10];
        v57 += 64LL;
        v60 = v58->n128_u64[1];
        v122.n128_u64[0] = v58->n128_u64[0];
        v122.n128_u64[1] = v60;
        v61 = v58[1].n128_u64[1];
        v123.n128_u64[0] = v58[1].n128_u64[0];
        v123.n128_u64[1] = v61;
        v62 = v58[2].n128_u64[1];
        v124.n128_u64[0] = v58[2].n128_u64[0];
        v124.n128_u64[1] = v62;
        v63 = v59->n128_u64[1];
        v64 = v58[3];
        v126.n128_u64[0] = v59->n128_u64[0];
        v126.n128_u64[1] = v63;
        v125 = v64;
        v65 = v59[1].n128_u64[1];
        v127.n128_u64[0] = v59[1].n128_u64[0];
        v127.n128_u64[1] = v65;
        v66 = v59[2].n128_u64[1];
        v128.n128_u64[0] = v59[2].n128_u64[0];
        v128.n128_u64[1] = v66;
        v129 = v59[3];
        v98 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v128), v54, v124);
        v99 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v129), v54, v64);
        v96 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v126), v54, v122);
        v97 = vmlaq_f32(vmulq_f32(*(float32x4_t *)&a7, v127), v54, v123);
        v67 = v96;
        v130 = v96;
        v68 = v97;
        v131 = v97;
        v69 = v98;
        v132 = v98;
        v70 = v99;
        v133 = v99;
        v96 = vmlaq_f32(vmulq_f32(v55, v126), v122, *(float32x4_t *)&a7);
        v97 = vmlaq_f32(vmulq_f32(v55, v127), v123, *(float32x4_t *)&a7);
        v98 = vmlaq_f32(vmulq_f32(v55, v128), v124, *(float32x4_t *)&a7);
        v99 = vmlaq_f32(vmulq_f32(v55, v129), v64, *(float32x4_t *)&a7);
        v71 = v97.n128_u64[1];
        v72 = v96;
        v59[1].n128_u64[0] = v97.n128_u64[0];
        v59[1].n128_u64[1] = v71;
        v73 = v98.n128_u64[1];
        v59[2].n128_u64[0] = v98.n128_u64[0];
        v59[2].n128_u64[1] = v73;
        v74 = v99;
        *v59 = v72;
        v59[3] = v74;
        *v58 = v67;
        v58[1] = v68;
        v58[2] = v69;
        v58[3] = v70;
      }
      while ( v56 != v57 );
      a3 = (float32x4_t *)((char *)a3 + v56);
      v14 = v94;
      v15 = v95;
      a2 = (float32x4_t *)((char *)a2 + v56);
      a4 &= 0xFu;
    }
    if ( a4 <= 7 )
    {
      v83 = a3;
      v84 = a2;
    }
    else
    {
      *(float *)&a7 = -*(float *)&a5;
      v75 = *a3;
      v76 = a3[1];
      a4 -= 8LL;
      v77 = vdupq_lane_s32(*(int32x2_t *)&a7, 0);
      v78 = vdupq_lane_s32(*(int32x2_t *)&a6, 0);
      v79 = a2[1];
      v106 = *a2;
      v107 = v79;
      v108 = v75;
      v109 = v76;
      v80 = vmlaq_f32(vmulq_f32(v77, v75), v106, v78);
      v81 = vmlaq_f32(vmulq_f32(v77, v76), v79, v78);
      v82 = vmlaq_n_f32(vmulq_f32(v78, v75), v106, *(float *)&a5);
      *a3 = v80;
      v83 = a3 + 2;
      a3[1] = v81;
      *a2 = v82;
      v84 = a2 + 2;
      v96 = v80;
      v97 = v81;
      a2[1] = vmlaq_n_f32(vmulq_f32(v78, v76), v79, *(float *)&a5);
    }
    if ( a4 <= 3 )
    {
      v87 = v83;
      v88 = v84;
    }
    else
    {
      v85 = vdupq_lane_s32(*(int32x2_t *)&a6, 0);
      a4 -= 4LL;
      v86 = vmlaq_n_f32(vmulq_f32(v85, *v83), *v84, *(float *)&a5);
      *v83 = vmlaq_f32(vmulq_n_f32(*v83, -*(float *)&a5), *v84, v85);
      v87 = v83 + 1;
      *v84 = v86;
      v88 = v84 + 1;
    }
    if ( a4 )
    {
      for ( i = 0; i != a4; ++i )
      {
        v90 = v87->n128_f32[i];
        v91 = v88->n128_f32[i];
        v87->n128_f32[i] = (float)(v90 * (float)-*(float *)&a5) + (float)(*(float *)&a6 * v91);
        v88->n128_f32[i] = (float)(*(float *)&a6 * v90) + (float)(*(float *)&a5 * v91);
      }
    }
  }
  result = v134 ^ _stack_chk_guard;
  if ( v134 != _stack_chk_guard )
  {
    v92 = v12;
    v93 = v13;
    v94 = v14;
    v95 = v15;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00096850  size 2736 bytes  657 insns
 *   sub_96850
 *   [sub_96850]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_96850(
        unsigned int **a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  unsigned int *v9; // x3
  unsigned int v10; // w2
  int v11; // w6
  int v12; // w5
  int v13; // w3
  int v14; // w2
  int v16; // w7
  int v17; // w6
  int v18; // w5
  int v19; // w12
  int v20; // w11
  int v21; // w10
  int v22; // w9
  int v23; // w6
  int v24; // w5
  int v25; // w3
  int v26; // w2
  int v27; // w6
  int v28; // w5
  int v29; // w3
  int v30; // w2
  int v31; // w10
  int v32; // w9
  int v33; // w8
  int v34; // w7
  int v35; // w6
  int v36; // w5
  int v37; // w3
  int v38; // w2
  int v39; // w9
  int v40; // w8
  int v41; // w7
  int v42; // w6
  int v43; // w5
  int v44; // w3
  int v45; // w2
  int v46; // w8
  int v47; // w7
  int v48; // w6
  int v49; // w5
  int v50; // w3
  int v51; // w2
  __int64 v52; // x5
  int v53; // w11
  int v54; // w10
  int v55; // w9
  int v56; // w8
  int v57; // w7
  int v58; // w6
  int v59; // w5
  int v60; // w3
  int v61; // w2
  float *v62; // x0
  if ( !a2 )
    return 1;
  v9 = *a1;
  v10 = **a1;
  if ( v10 == 9240582 )
  {
    v46 = 3;
    v47 = 1;
    v48 = 2;
    v49 = 5;
    v50 = 6;
    v51 = 9;
    goto LABEL_71;
  }
  if ( v10 > 0x8D0006 )
  {
    if ( v10 == 12845066 )
    {
      v27 = 33;
      v28 = 34;
      v29 = 49;
      v30 = 51;
      goto LABEL_31;
    }
    if ( v10 <= 0xC4000A )
    {
      if ( v10 == 12582924 )
      {
        v53 = 4;
        v54 = 5;
        v55 = 6;
        v56 = 33;
        v57 = 34;
LABEL_82:
        v58 = 13;
        v59 = 15;
        v60 = 52;
        v61 = 54;
LABEL_83:
        *(_DWORD *)a2 = 1;
        *(_DWORD *)(a2 + 20) = 2;
        *(_DWORD *)(a2 + 40) = 3;
        *(_DWORD *)(a2 + 60) = v53;
        *(_DWORD *)(a2 + 80) = v54;
        *(_DWORD *)(a2 + 100) = v55;
        *(_DWORD *)(a2 + 120) = v56;
        *(_DWORD *)(a2 + 140) = v57;
        *(_DWORD *)(a2 + 160) = v58;
        *(_DWORD *)(a2 + 180) = v59;
        *(_DWORD *)(a2 + 200) = v60;
        *(_DWORD *)(a2 + 220) = v61;
        return 0;
      }
      if ( v10 > 0xC0000C )
      {
        if ( v10 == 12713992 )
        {
          v31 = 1;
          v32 = 2;
          v33 = 3;
          v34 = 4;
          v35 = 5;
          v36 = 6;
          v37 = 49;
          v38 = 51;
          goto LABEL_38;
        }
        if ( v10 != 12779530 )
        {
          if ( v10 == 12648464 )
          {
            *(_DWORD *)a2 = 1;
            *(_DWORD *)(a2 + 20) = 2;
            *(_DWORD *)(a2 + 40) = 3;
            *(_DWORD *)(a2 + 60) = 4;
            *(_DWORD *)(a2 + 80) = 5;
            *(_DWORD *)(a2 + 100) = 6;
            *(_DWORD *)(a2 + 120) = 33;
            *(_DWORD *)(a2 + 140) = 34;
            *(_DWORD *)(a2 + 160) = 35;
            *(_DWORD *)(a2 + 180) = 36;
            *(_DWORD *)(a2 + 200) = 13;
            *(_DWORD *)(a2 + 220) = 15;
            *(_DWORD *)(a2 + 240) = 49;
            *(_DWORD *)(a2 + 260) = 51;
            *(_DWORD *)(a2 + 280) = 52;
            *(_DWORD *)(a2 + 300) = 54;
            return 0;
          }
          return 1;
        }
        v27 = 13;
        v28 = 15;
        v29 = 52;
        v30 = 54;
        goto LABEL_31;
      }
      if ( v10 == 9437192 )
      {
        v62 = (float *)(a2 + 4);
        do
        {
          *(_DWORD *)v62 = 2;
          v62 += 5;
          *(v62 - 2) = 1.0;
        }
        while ( (float *)(a2 + 164) != v62 );
        *(_DWORD *)a2 = 1;
        *(_DWORD *)(a2 + 20) = 2;
        *(_DWORD *)(a2 + 40) = 5;
        *(_DWORD *)(a2 + 60) = 6;
        *(_DWORD *)(a2 + 80) = 3;
        *(_DWORD *)(a2 + 100) = 9;
        *(_DWORD *)(a2 + 120) = 35;
        *(_DWORD *)(a2 + 140) = 36;
        *(_DWORD *)(a2 + 8) = -1036779520;
        *(_DWORD *)(a2 + 28) = 1110704128;
        *(_DWORD *)(a2 + 48) = -1022951424;
        *(_DWORD *)(a2 + 68) = 1124532224;
        *(_DWORD *)(a2 + 88) = 0;
        *(_DWORD *)(a2 + 108) = 1127481344;
        *(_DWORD *)(a2 + 128) = -1028390912;
        *(_DWORD *)(a2 + 148) = 1119092736;
        return 0;
      }
      if ( v10 <= 0x900008 )
      {
        if ( v10 == 9306119 )
        {
          v39 = 3;
          v40 = 1;
          v41 = 2;
          v42 = 5;
          v43 = 6;
          v44 = 9;
          v45 = 4;
          goto LABEL_48;
        }
        if ( v10 == 9371655 )
        {
          v39 = 3;
          v40 = 1;
          v41 = 2;
          v42 = 5;
          v43 = 6;
          v44 = 33;
          v45 = 34;
LABEL_48:
          *(_DWORD *)a2 = v39;
          *(_DWORD *)(a2 + 20) = v40;
          *(_DWORD *)(a2 + 40) = v41;
          *(_DWORD *)(a2 + 60) = v42;
          *(_DWORD *)(a2 + 80) = v43;
          *(_DWORD *)(a2 + 100) = v44;
          *(_DWORD *)(a2 + 120) = v45;
          return 0;
        }
        return 1;
      }
      if ( v10 != 11993096 )
        return 1;
      v31 = 3;
      v32 = 1;
      v33 = 2;
      v34 = 5;
      v35 = 6;
      v36 = 33;
      v37 = 34;
      v38 = 4;
    }
    else
    {
      if ( v10 == 13631500 )
      {
        v53 = 4;
        v54 = 5;
        v55 = 6;
        v56 = 13;
        v57 = 15;
        v58 = 14;
        v59 = 60;
        v60 = 61;
        v61 = 12;
        goto LABEL_83;
      }
      if ( v10 > 0xD0000C )
      {
        if ( v10 != 13762572 )
        {
          if ( v10 == 13828110 )
          {
            v19 = 33;
            v20 = 34;
            v21 = 55;
            v22 = 56;
            v23 = 52;
            v24 = 54;
            v25 = 65;
            v26 = 66;
          }
          else
          {
            if ( v10 != 13697038 )
              return 1;
            v19 = 5;
            v20 = 6;
            v21 = 63;
            v22 = 64;
            v23 = 14;
            v24 = 60;
            v25 = 61;
            v26 = 12;
          }
          *(_DWORD *)a2 = 1;
          *(_DWORD *)(a2 + 20) = 2;
          *(_DWORD *)(a2 + 40) = 3;
          *(_DWORD *)(a2 + 60) = 4;
          *(_DWORD *)(a2 + 80) = v19;
          *(_DWORD *)(a2 + 100) = v20;
          *(_DWORD *)(a2 + 120) = v21;
          *(_DWORD *)(a2 + 140) = v22;
          *(_DWORD *)(a2 + 160) = 13;
          *(_DWORD *)(a2 + 180) = 15;
          *(_DWORD *)(a2 + 200) = v23;
          *(_DWORD *)(a2 + 220) = v24;
          *(_DWORD *)(a2 + 240) = v25;
          *(_DWORD *)(a2 + 260) = v26;
          return 0;
        }
        v53 = 4;
        v54 = 33;
        v55 = 34;
        v56 = 55;
        v57 = 56;
        goto LABEL_82;
      }
      if ( v10 == 13500428 )
      {
        v53 = 37;
        v54 = 33;
        v55 = 34;
        v56 = 62;
        v57 = 55;
        v58 = 56;
        v59 = 13;
        v60 = 15;
        v61 = 53;
        goto LABEL_83;
      }
      if ( v10 > 0xCE000C )
      {
        if ( v10 != 13565962 )
          return 1;
        v27 = 13;
        v28 = 15;
        v29 = 60;
        v30 = 61;
LABEL_31:
        *(_DWORD *)a2 = 1;
        *(_DWORD *)(a2 + 20) = 2;
        *(_DWORD *)(a2 + 40) = 3;
        *(_DWORD *)(a2 + 60) = 4;
        *(_DWORD *)(a2 + 80) = 5;
        *(_DWORD *)(a2 + 100) = 6;
        *(_DWORD *)(a2 + 120) = v27;
        *(_DWORD *)(a2 + 140) = v28;
        *(_DWORD *)(a2 + 160) = v29;
        *(_DWORD *)(a2 + 180) = v30;
        return 0;
      }
      if ( v10 == 13369368 )
      {
        *(_DWORD *)a2 = 7;
        *(_DWORD *)(a2 + 20) = 8;
        *(_DWORD *)(a2 + 40) = 3;
        *(_DWORD *)(a2 + 60) = 37;
        *(_DWORD *)(a2 + 80) = 33;
        *(_DWORD *)(a2 + 100) = 34;
        *(_DWORD *)(a2 + 120) = 1;
        *(_DWORD *)(a2 + 140) = 2;
        *(_DWORD *)(a2 + 160) = 9;
        *(_DWORD *)(a2 + 180) = 62;
        *(_DWORD *)(a2 + 200) = 55;
        *(_DWORD *)(a2 + 220) = 56;
        *(_DWORD *)(a2 + 240) = 13;
        *(_DWORD *)(a2 + 260) = 15;
        *(_DWORD *)(a2 + 280) = 14;
        *(_DWORD *)(a2 + 300) = 12;
        *(_DWORD *)(a2 + 320) = 52;
        *(_DWORD *)(a2 + 340) = 54;
        *(_DWORD *)(a2 + 360) = 49;
        *(_DWORD *)(a2 + 380) = 51;
        *(_DWORD *)(a2 + 400) = 53;
        *(_DWORD *)(a2 + 420) = 59;
        *(_DWORD *)(a2 + 440) = 57;
        *(_DWORD *)(a2 + 460) = 58;
        return 0;
      }
      if ( v10 != 13434888 )
        return 1;
      v31 = 1;
      v32 = 2;
      v33 = 3;
      v34 = 4;
      v35 = 5;
      v36 = 6;
      v37 = 13;
      v38 = 15;
    }
LABEL_38:
    *(_DWORD *)a2 = v31;
    *(_DWORD *)(a2 + 20) = v32;
    *(_DWORD *)(a2 + 40) = v33;
    *(_DWORD *)(a2 + 60) = v34;
    *(_DWORD *)(a2 + 80) = v35;
    *(_DWORD *)(a2 + 100) = v36;
    *(_DWORD *)(a2 + 120) = v37;
    *(_DWORD *)(a2 + 140) = v38;
    return 0;
  }
  if ( v10 == 7864325 )
  {
    v16 = 3;
    v17 = 1;
    v18 = 2;
    goto LABEL_17;
  }
  if ( v10 <= 0x780005 )
  {
    if ( v10 == 7405571 )
    {
      *(_DWORD *)a2 = 1;
      *(_DWORD *)(a2 + 20) = 2;
      *(_DWORD *)(a2 + 40) = 3;
      return 0;
    }
    if ( v10 <= 0x710003 )
    {
      if ( v10 == 6619138 )
      {
        *(_DWORD *)a2 = 1;
        *(_DWORD *)(a2 + 20) = 2;
        return 0;
      }
      else
      {
        if ( v10 > 0x650002 )
        {
          if ( v10 == 7077892 )
          {
            *(_QWORD *)a2 = 0x200000001LL;
            *(_DWORD *)(a2 + 8) = -1036779520;
            *(_QWORD *)(a2 + 16) = 0x23F800000LL;
            *(_QWORD *)(a2 + 24) = 0x4234000000000002LL;
            *(_QWORD *)(a2 + 36) = 0x53F800000LL;
            *(_QWORD *)(a2 + 44) = 0xC307000000000002LL;
            *(_QWORD *)(a2 + 56) = 0x63F800000LL;
            *(_QWORD *)(a2 + 64) = 0x4307000000000002LL;
            *(float *)(a2 + 76) = 1.0;
            return 0;
          }
          return 1;
        }
        if ( v10 )
        {
          if ( v10 == 6553601 )
          {
            *(_DWORD *)a2 = 42;
            return 0;
          }
          return 1;
        }
        if ( v9[2] )
        {
          do
          {
            v52 = 5LL * v10++;
            *(_DWORD *)(a2 + v52 * 4) = v9[v52 + 3];
            v9 = *a1;
          }
          while ( v10 < (*a1)[2] );
        }
        return 0;
      }
    }
    if ( v10 == 7602180 )
    {
      v11 = 3;
      v12 = 1;
      v13 = 2;
      v14 = 9;
      goto LABEL_11;
    }
    if ( v10 <= 0x740004 )
    {
      if ( v10 == 7471107 )
      {
        *(_DWORD *)a2 = 3;
        *(_DWORD *)(a2 + 20) = 1;
        *(_DWORD *)(a2 + 40) = 2;
        return 0;
      }
      if ( v10 == 7536644 )
      {
        v11 = 1;
        v12 = 2;
        v13 = 3;
        v14 = 9;
        goto LABEL_11;
      }
      return 1;
    }
    v16 = 1;
    v17 = 2;
    v18 = 3;
    if ( v10 != 7667717 )
      return 1;
LABEL_17:
    *(_DWORD *)a2 = v16;
    *(_DWORD *)(a2 + 20) = v17;
    *(_DWORD *)(a2 + 40) = v18;
    *(_DWORD *)(a2 + 60) = 5;
    *(_DWORD *)(a2 + 80) = 6;
    return 0;
  }
  if ( v10 == 8323080 )
  {
    v31 = 3;
    v32 = 7;
    v33 = 8;
    v34 = 1;
    v35 = 2;
    v36 = 5;
    v37 = 6;
    v38 = 4;
    goto LABEL_38;
  }
  if ( v10 <= 0x7F0008 )
  {
    if ( v10 == 8126470 )
    {
      v46 = 3;
      v47 = 1;
      v48 = 2;
      v49 = 5;
      v50 = 6;
      v51 = 4;
    }
    else
    {
      if ( v10 > 0x7C0006 )
      {
        if ( v10 == 8192007 )
        {
          v39 = 1;
          v40 = 2;
          v41 = 3;
          v42 = 4;
          v43 = 5;
          v44 = 6;
          v45 = 9;
          goto LABEL_48;
        }
        return 1;
      }
      if ( v10 == 7929862 )
      {
        v46 = 1;
        v47 = 2;
        v48 = 3;
        v49 = 4;
        v50 = 5;
        v51 = 6;
      }
      else
      {
        if ( v10 != 8060934 )
          return 1;
        v46 = 1;
        v47 = 3;
        v48 = 2;
        v49 = 5;
        v50 = 6;
        v51 = 4;
      }
    }
LABEL_71:
    *(_DWORD *)a2 = v46;
    *(_DWORD *)(a2 + 20) = v47;
    *(_DWORD *)(a2 + 40) = v48;
    *(_DWORD *)(a2 + 60) = v49;
    *(_DWORD *)(a2 + 80) = v50;
    *(_DWORD *)(a2 + 100) = v51;
    return 0;
  }
  if ( v10 != 8585219 )
  {
    if ( v10 == 8650756 )
    {
      v11 = 1;
      v12 = 2;
      v13 = 5;
      v14 = 6;
LABEL_11:
      *(_DWORD *)a2 = v11;
      *(_DWORD *)(a2 + 20) = v12;
      *(_DWORD *)(a2 + 40) = v13;
      *(_DWORD *)(a2 + 60) = v14;
      return 0;
    }
    if ( v10 == 8388616 )
    {
      v31 = 7;
      v32 = 8;
      v33 = 3;
      v34 = 4;
      v35 = 1;
      v36 = 2;
      v37 = 5;
      v38 = 6;
      goto LABEL_38;
    }
    return 1;
  }
  *(_DWORD *)a2 = 1;
  *(_DWORD *)(a2 + 20) = 2;
  *(_DWORD *)(a2 + 40) = 9;
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x00097350  size 348 bytes  87 insns
 *   sub_97350
 *   [sub_97350]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_97350(
        __int64 a1,
        __int64 a2,
        __int64 a3,
        unsigned int a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 i; // x13
  __int64 v10; // x5
  __int64 v11; // x7
  unsigned int *v12; // x6
  __int64 v13; // x10
  int v14; // w8
  __int64 v15; // x11
  unsigned int v16; // w10
  unsigned int v17; // w11
  __int64 v18; // x5
  __int64 v19; // x2
  bool v20; // cc
  unsigned int v21; // w8
  unsigned int *v22; // x11
  __int64 v24; // [xsp+18h] [xbp+18h]
  if ( a2 >= (a3 - 1) / 2 )
  {
    v12 = (unsigned int *)(a1 + 4 * a2);
    v18 = a2;
    if ( (a3 & 1) != 0 )
      goto LABEL_16;
    goto LABEL_18;
  }
  for ( i = a2; ; i = v11 )
  {
    v10 = 2 * (i + 1);
    v11 = v10 - 1;
    v12 = (unsigned int *)(a1 + 8 * (i + 1));
    v13 = *(_QWORD *)(a5 + 200);
    v14 = *v12;
    v15 = *(_QWORD *)(v13 + 8LL * *v12);
    v16 = *(unsigned __int16 *)(*(_QWORD *)(v13 + 8LL * *(unsigned int *)(a1 + 4 * (v10 - 1))) + 42LL);
    v17 = *(unsigned __int16 *)(v15 + 42);
    if ( v17 < v16 )
      v14 = *(_DWORD *)(a1 + 4 * (v10 - 1));
    *(_DWORD *)(a1 + 4 * i) = v14;
    if ( v17 < v16 )
      v12 = (unsigned int *)(a1 + 4 * (v10 - 1));
    else
      v11 = 2 * (i + 1);
    v18 = v11;
    if ( v11 >= (a3 - 1) / 2 )
      break;
  }
  if ( (a3 & 1) == 0 )
  {
LABEL_18:
    if ( v18 == (a3 - 2) / 2 )
    {
      v18 = 2 * v18 + 1;
      *v12 = *(_DWORD *)(a1 + 4 * v18);
      v12 = (unsigned int *)(a1 + 4 * v18);
    }
  }
  v19 = (v18 - 1) / 2;
  if ( v18 > a2 )
  {
    while ( 1 )
    {
      v21 = *(_DWORD *)(a1 + 4 * v19);
      v22 = (unsigned int *)(a1 + 4 * v19);
      v12 = (unsigned int *)(a1 + 4 * v18);
      if ( *(unsigned __int16 *)(*(_QWORD *)(*(_QWORD *)(a5 + 200) + 8LL * v21) + 42LL) >= (unsigned int)*(unsigned __int16 *)(*(_QWORD *)(*(_QWORD *)(a5 + 200) + 8LL * a4) + 42LL) )
        break;
      v20 = a2 < v19;
      *(_DWORD *)(a1 + 4 * v18) = v21;
      v18 = v19;
      v19 = (v19 - 1) / 2;
      if ( !v20 )
      {
        v12 = v22;
        break;
      }
    }
  }
LABEL_16:
  *v12 = a4;
  return v24 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000974b0  size 308 bytes  77 insns
 *   sub_974B0
 *   [sub_974B0]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_974B0(
        unsigned int *src,
        char *a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x23
  __int64 v10; // x24
  char *v13; // x19
  unsigned int v14; // w23
  __int64 v15; // x0
  __int64 v16; // x2
  unsigned int v17; // w5
  __int64 result; // x0
  unsigned int v19; // w4
  unsigned int *v20; // x3
  unsigned int *v21; // x0
  unsigned int v22; // t1
  __int64 v23; // [xsp+30h] [xbp+30h]
  __int64 v24; // [xsp+38h] [xbp+38h]
  __int64 v25; // [xsp+58h] [xbp+58h]
  if ( a2 != (char *)src )
  {
    v13 = (char *)(src + 1);
    if ( src + 1 != (unsigned int *)a2 )
    {
      v23 = v9;
      v10 = a3;
      do
      {
        v14 = *(_DWORD *)v13;
        v15 = *(_QWORD *)(v10 + 200);
        v16 = 8LL * *(unsigned int *)v13;
        v17 = *(unsigned __int16 *)(*(_QWORD *)(v15 + v16) + 42LL);
        if ( *(unsigned __int16 *)(*(_QWORD *)(v15 + 8LL * *src) + 42LL) <= v17 )
        {
          v19 = *((_DWORD *)v13 - 1);
          v20 = (unsigned int *)(v13 - 4);
          if ( *(unsigned __int16 *)(*(_QWORD *)(v15 + 8LL * v19) + 42LL) <= v17 )
          {
            *(_DWORD *)v13 = v14;
          }
          else
          {
            do
            {
              v20[1] = v19;
              v21 = v20;
              v22 = *--v20;
              v19 = v22;
            }
            while ( *(unsigned __int16 *)(*(_QWORD *)(*(_QWORD *)(v10 + 200) + v16) + 42LL) < (unsigned int)*(unsigned __int16 *)(*(_QWORD *)(*(_QWORD *)(v10 + 200) + 8LL * v22) + 42LL) );
            *v21 = v14;
          }
        }
        else
        {
          if ( src != (unsigned int *)v13 )
            memmove(src + 1, src, v13 - (char *)src);
          *src = v14;
        }
        v13 += 4;
      }
      while ( a2 != v13 );
      v9 = v23;
    }
  }
  result = v25 ^ _stack_chk_guard;
  if ( v25 != _stack_chk_guard )
  {
    v23 = v9;
    v24 = v10;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00097670  size 592 bytes  148 insns
 *   sub_97670
 *   [sub_97670]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_97670(
        unsigned int *a1,
        unsigned int *a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x23
  unsigned int *v10; // x24
  __int64 v11; // x25
  __int64 v12; // x26
  __int64 v14; // x0
  unsigned int v15; // w9
  unsigned int v16; // w4
  unsigned int v17; // w7
  __int64 v18; // x2
  __int64 v19; // x0
  __int64 v20; // x3
  unsigned int v21; // w8
  unsigned int v22; // w5
  unsigned int v23; // w6
  unsigned int v24; // w2
  unsigned int v25; // w6
  __int64 v26; // x5
  unsigned int *v27; // x4
  unsigned int *v28; // x20
  unsigned int *v29; // x0
  unsigned int v30; // w3
  unsigned int v31; // w2
  unsigned int v32; // t1
  unsigned int v33; // t1
  __int64 v34; // x20
  __int64 i; // x22
  unsigned int v36; // w3
  __int64 v37; // x20
  __int64 result; // x0
  __int64 v39; // [xsp+0h] [xbp+0h]
  __int64 v40; // [xsp+30h] [xbp+30h]
  unsigned int *v41; // [xsp+38h] [xbp+38h]
  __int64 v42; // [xsp+40h] [xbp+40h]
  __int64 v43; // [xsp+48h] [xbp+48h]
  __int64 v44; // [xsp+58h] [xbp+58h]
  v14 = (char *)a2 - (char *)a1;
  if ( v14 <= 64 )
    goto LABEL_26;
  v10 = a2;
  v12 = a3;
  v11 = a4;
  if ( !a3 )
    goto LABEL_21;
  while ( 2 )
  {
    v15 = *(a2 - 1);
    v17 = *a1;
    v16 = a1[1];
    --v12;
    v18 = *(_QWORD *)(v11 + 200);
    v19 = (__int64)(((unsigned __int64)((char *)a2 - (char *)a1) >> 63) + a2 - a1) >> 1;
    v20 = 8LL * v16;
    v21 = a1[v19];
    v22 = *(unsigned __int16 *)(*(_QWORD *)(v18 + v20) + 42LL);
    v23 = *(unsigned __int16 *)(*(_QWORD *)(v18 + 8LL * v15) + 42LL);
    v24 = *(unsigned __int16 *)(*(_QWORD *)(v18 + 8LL * v21) + 42LL);
    if ( v22 >= v24 )
    {
      if ( v22 < v23 )
        goto LABEL_6;
      if ( v24 < v23 )
      {
LABEL_16:
        *a1 = v15;
        v25 = v17;
        *(a2 - 1) = v17;
        v17 = a1[1];
        v20 = 8LL * *a1;
        goto LABEL_7;
      }
LABEL_20:
      *a1 = v21;
      a1[v19] = v17;
      v17 = a1[1];
      v25 = *(a2 - 1);
      v20 = 8LL * *a1;
      goto LABEL_7;
    }
    if ( v24 < v23 )
      goto LABEL_20;
    if ( v22 < v23 )
      goto LABEL_16;
LABEL_6:
    *a1 = v16;
    a1[1] = v17;
    v25 = *(a2 - 1);
LABEL_7:
    v26 = *(_QWORD *)(v11 + 200);
    v27 = a1 + 2;
    v28 = a1 + 1;
    v29 = a2;
    v30 = *(unsigned __int16 *)(*(_QWORD *)(v26 + v20) + 42LL);
    v31 = *(unsigned __int16 *)(*(_QWORD *)(v26 + 8LL * v25) + 42LL);
    while ( 1 )
    {
      v10 = v28;
      if ( *(unsigned __int16 *)(*(_QWORD *)(v26 + 8LL * v17) + 42LL) < v30 )
        goto LABEL_13;
      --v29;
      if ( v30 < v31 )
      {
        do
        {
          v32 = *--v29;
          v25 = v32;
        }
        while ( *(unsigned __int16 *)(*(_QWORD *)(v26 + 8LL * v32) + 42LL) > v30 );
      }
      if ( v28 >= v29 )
        break;
      *v28 = v25;
      *v29 = v17;
      v25 = *(v29 - 1);
      v26 = *(_QWORD *)(v11 + 200);
      v31 = *(unsigned __int16 *)(*(_QWORD *)(v26 + 8LL * v25) + 42LL);
      v30 = *(unsigned __int16 *)(*(_QWORD *)(v26 + 8LL * *a1) + 42LL);
LABEL_13:
      v33 = *v27++;
      v17 = v33;
      ++v28;
    }
    sub_97670(v28, a2, v12, v11);
    v14 = (char *)v28 - (char *)a1;
    if ( (char *)v28 - (char *)a1 > 64 )
    {
      if ( v12 )
      {
        a2 = v28;
        continue;
      }
LABEL_21:
      v34 = v14 >> 2;
      for ( i = ((v14 >> 2) - 2) >> 1; ; --i )
      {
        sub_97350((__int64)a1, i, v34, a1[i], v11, a6, a7, a8, v39);
        if ( !i )
          break;
      }
      --v10;
      do
      {
        v36 = *v10;
        v37 = (char *)v10 - (char *)a1;
        *v10 = *a1;
        sub_97350((__int64)a1, 0, v10 - a1, v36, v11, a6, a7, a8, v39);
        --v10;
      }
      while ( v37 > 4 );
    }
    break;
  }
LABEL_26:
  result = v44 ^ _stack_chk_guard;
  if ( v44 != _stack_chk_guard )
  {
    v40 = v9;
    v41 = v10;
    v42 = v11;
    v43 = v12;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00099650  size 836 bytes  208 insns
 *   int GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, __gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::vector<apac::observer_ptr<apac::IASCConfig const>, std::allocator<apac::observer_ptr<apac::IASCConfig const> > >&)
 *   [_Z28GetEffectiveDecoderOutConfigIN9__gnu_cxx17__normal_iteratorIPSt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS4_EESt6vectorIS7_SaIS7_EEEEEiT_SD_RS9_INS3_12observer_ptrIKS4_EESaISG_EE]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
int __fastcall GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig> *,std::vector<std::unique_ptr<apac::IASCConfig>>>>(
        void *a,
        void *a2,
        std_vector *vec)
{
  __int64 v3; // x23
  __int64 v4; // x24
  char *v6; // x19
  void *begin; // x2
  int v9; // w23
  int v10; // w0
  unsigned __int16 *v11; // x22
  int v12; // w24
  __int64 v13; // x0
  unsigned int (*v14)(void); // x1
  int v15; // w23
  void *v16; // x0
  _BOOL4 v17; // w19
  __int16 v19; // w0
  char *end; // x22
  __int64 v21; // x24
  char *v22; // x23
  __int64 v23; // x28
  unsigned __int64 v24; // x0
  bool v25; // cc
  unsigned __int64 v26; // x27
  unsigned __int64 v27; // x0
  __int64 v28; // x0
  void *v29; // x4
  _QWORD *v30; // x3
  char *v31; // x27
  __int64 v32; // x1
  unsigned __int64 v33; // x22
  __int64 v35; // [xsp+30h] [xbp+30h]
  __int64 v36; // [xsp+38h] [xbp+38h]
  _QWORD *v37; // [xsp+60h] [xbp+60h]
  void *v38; // [xsp+68h] [xbp+68h]
  void *v39; // [xsp+70h] [xbp+70h] BYREF
  unsigned __int16 *v40; // [xsp+78h] [xbp+78h]
  __int64 v41; // [xsp+80h] [xbp+80h]
  __int64 v42; // [xsp+88h] [xbp+88h]
  __int64 v43; // [xsp+90h] [xbp+90h]
  v6 = (char *)a;
  v39 = nullptr;
  v40 = nullptr;
  begin = vec->begin;
  v41 = 0;
  if ( begin != vec->end )
    vec->end = begin;
  if ( a != a2 )
  {
    v35 = v3;
    v36 = v4;
    while ( 1 )
    {
      v9 = *(unsigned __int16 *)(*(_QWORD *)v6 + 42LL);
      v10 = apac::ConvertToAudioDataType(*(_DWORD *)(*(_QWORD *)v6 + 8LL));
      v11 = (unsigned __int16 *)v39;
      v12 = v10;
      v13 = ((char *)v40 - (_BYTE *)v39) >> 3;
      if ( ((char *)v40 - (_BYTE *)v39) >> 5 > 0 )
      {
        while ( *v11 != v9 )
        {
          if ( v11[4] == v9 )
          {
            v11 += 4;
            goto LABEL_12;
          }
          if ( v11[8] == v9 )
          {
            v11 += 8;
            goto LABEL_12;
          }
          if ( v11[12] == v9 )
          {
            v11 += 12;
            goto LABEL_12;
          }
          v11 += 16;
          if ( (char *)v39 + 32 * (((char *)v40 - (_BYTE *)v39) >> 5) == (char *)v11 )
          {
            v13 = ((char *)v40 - (char *)v11) >> 3;
            goto LABEL_21;
          }
        }
        goto LABEL_12;
      }
LABEL_21:
      if ( v13 == 2 )
        goto LABEL_32;
      if ( v13 == 3 )
        break;
      if ( v13 != 1 )
        goto LABEL_24;
LABEL_34:
      if ( *v11 != v9 )
      {
LABEL_24:
        v14 = *(unsigned int (**)(void))(**(_QWORD **)v6 + 24LL);
        goto LABEL_25;
      }
LABEL_12:
      v14 = *(unsigned int (**)(void))(**(_QWORD **)v6 + 24LL);
      if ( v40 != v11 )
      {
        v15 = v11[1] - *v11;
        if ( v14() != v15 || *((_DWORD *)v11 + 1) != v12 )
        {
          v16 = v39;
          v17 = 1;
          if ( !v39 )
          {
LABEL_49:
            v3 = v35;
            v4 = v36;
            goto LABEL_18;
          }
LABEL_17:
          operator delete(v16);
          v3 = v35;
          v4 = v36;
          goto LABEL_18;
        }
        goto LABEL_15;
      }
LABEL_25:
      v19 = v14();
      apac::ASCChannelMap::AddMap((apac__ASCChannelMap *)&v39, v12, v9, v9 + v19);
      end = (char *)vec->end;
      v21 = *(_QWORD *)v6;
      if ( end == vec->cap )
      {
        v22 = (char *)vec->begin;
        v23 = end - (char *)vec->begin;
        v24 = v23 >> 3;
        if ( v23 >> 3 == 0xFFFFFFFFFFFFFFFLL )
          std::__throw_length_error("vector::_M_realloc_insert");
        if ( v24 )
        {
          v25 = v24 > 2 * v24;
          v26 = 0x7FFFFFFFFFFFFFF8LL;
          v27 = 2 * v24;
          if ( !v25 )
          {
            if ( !v27 )
            {
              v31 = byte_8;
              v29 = nullptr;
              v30 = nullptr;
LABEL_41:
              *(_QWORD *)((char *)v30 + v23) = v21;
              if ( end == v22 )
              {
                if ( v22 )
                  goto LABEL_46;
              }
              else
              {
                v32 = 0;
                v33 = end - 8 - v22;
                do
                  v30[v32] = *(_QWORD *)&v22[8 * v32];
                while ( v33 >> 3 != v32++ );
                v31 = (char *)v30 + v33 + 16;
LABEL_46:
                v37 = v30;
                v38 = v29;
                operator delete(v22);
                v30 = v37;
                v29 = v38;
              }
              vec->begin = v30;
              vec->end = v31;
              vec->cap = v29;
              goto LABEL_15;
            }
            if ( v27 > 0xFFFFFFFFFFFFFFFLL )
              v27 = 0xFFFFFFFFFFFFFFFLL;
            v26 = 8 * v27;
          }
        }
        else
        {
          v26 = 8;
        }
        v28 = operator new(v26);
        v29 = (void *)(v28 + v26);
        v30 = (_QWORD *)v28;
        v31 = (char *)(v28 + 8);
        goto LABEL_41;
      }
      *(_QWORD *)end = v21;
      vec->end = (char *)vec->end + 8;
LABEL_15:
      v6 += 8;
      if ( a2 == v6 )
      {
        v16 = v39;
        v17 = vec->end == vec->begin;
        if ( !v39 )
          goto LABEL_49;
        goto LABEL_17;
      }
    }
    if ( *v11 == v9 )
      goto LABEL_12;
    v11 += 4;
LABEL_32:
    if ( *v11 == v9 )
      goto LABEL_12;
    v11 += 4;
    goto LABEL_34;
  }
  v17 = vec->end == begin;
LABEL_18:
  if ( v42 != _stack_chk_guard )
  {
    v35 = v3;
    v36 = v4;
  }
  return v17;
}


/* ------------------------------------------------------------------------ */
/* 0x000aec20  size 232 bytes  58 insns
 *   sub_AEC20
 *   [sub_AEC20]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_AEC20(
        float *a1,
        int a2,
        int a3,
        __int64 a4,
        unsigned int a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  _QWORD *v9; // x5
  unsigned int v10; // w10
  __int64 v11; // x8
  float *v12; // x9
  __int64 v13; // x5
  int i; // w6
  float *v15; // x4
  float *v16; // x1
  float v17; // s2
  float v18; // t1
  _QWORD v20[11]; // [xsp+18h] [xbp+18h] BYREF
  memset(v20, 0, 80);
  v9 = v20;
  do
  {
    *(_DWORD *)v9 = 0;
    v9 = (_QWORD *)((char *)v9 + 4);
  }
  while ( v9 != (_QWORD *)((char *)v20 + 4 * a5) );
  if ( a3 )
  {
    v10 = a5 - 1;
    v11 = 4LL * a2;
    v12 = (float *)(a4 + 4LL * a5);
    v13 = a4 + 8 + 4LL * (a5 - 2);
    for ( i = 0; i != a3; ++i )
    {
      v15 = (float *)(a4 + 4);
      v16 = (float *)v20;
      v17 = *a1 - *(float *)v20;
      *a1 = v17;
      a1 = (float *)((char *)a1 + v11);
      if ( v10 )
      {
        do
        {
          v18 = *v15++;
          *v16 = v16[1] + (float)(v18 * v17);
          ++v16;
        }
        while ( (float *)v13 != v15 );
      }
      *((float *)v20 + v10) = *v12 * v17;
    }
  }
  return v20[10] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b19b0  size 316 bytes  79 insns
 *   sub_B19B0
 *   [sub_B19B0]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_B19B0(
        const void **a1,
        char *dest,
        unsigned int a3,
        unsigned int a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x27
  __int64 v10; // x28
  __int64 v14; // x24
  const void *v15; // x5
  unsigned int v16; // w19
  __int64 v17; // x2
  __int64 v18; // x0
  const void *v19; // x6
  const void **v20; // x20
  __int64 v21; // x2
  __int64 v22; // x0
  int v23; // s0
  __int64 result; // x0
  __int64 v25; // [xsp+50h] [xbp+50h]
  __int64 v26; // [xsp+58h] [xbp+58h]
  _QWORD v27[2]; // [xsp+68h] [xbp+68h] BYREF
  __int64 v28; // [xsp+78h] [xbp+78h]
  v14 = a4;
  v15 = *a1;
  if ( a3 == 1 )
  {
    memcpy(dest, *a1, 4LL * a4);
  }
  else
  {
    v16 = 0;
    v17 = 0;
    v25 = v9;
    while ( 1 )
    {
      v18 = v16 + 1;
      v16 += 2;
      v19 = a1[v18];
      v27[0] = v15;
      v27[1] = v19;
      vDSP_ztoc(v27, 1, &dest[4 * v17], a3, v14);
      if ( v16 >= a3 - 1 )
        break;
      v17 = v16;
      v15 = a1[v16];
    }
    if ( a3 > v16 && (_DWORD)v14 )
    {
      v20 = &a1[v16];
      v21 = v16;
      v22 = 0;
      while ( 1 )
      {
        v16 += a3;
        v23 = *(_DWORD *)((char *)*v20 + v22);
        v22 += 4;
        *(_DWORD *)&dest[4 * v21] = v23;
        if ( 4 * ((unsigned int)(v14 - 1) + 1LL) == v22 )
          break;
        v21 = v16;
      }
    }
    v9 = v25;
  }
  result = v28 ^ _stack_chk_guard;
  if ( v28 != _stack_chk_guard )
  {
    v25 = v9;
    v26 = v10;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b1d90  size 724 bytes  181 insns
 *   InterleaveConvert(float const* const*, void* const*, unsigned int, unsigned int, bool, bool, float)
 *   [_Z17InterleaveConvertPKPKfPKPvjjbbf]
 *   data refs: 0x42ce00, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall InterleaveConvert(
        const float **a,
        void **a2,
        unsigned int n,
        unsigned int n2,
        bool flag,
        bool flag2,
        float f)
{
  __int64 v7; // x29
  __int64 v8; // x30
  __int64 *v9; // x9
  _BOOL8 v10; // x6
  __int64 v11; // x5
  __int64 v13; // x19
  __int64 v16; // x20
  __int64 v17; // x22
  __int64 v20; // x1
  __int64 v21; // x4
  float v22; // s1
  const float *v23; // x3
  _WORD *i; // x2
  const float *v25; // x4
  const float *v26; // x5
  _WORD *v27; // x3
  int v28; // w1
  __int64 v29; // x0
  __int64 v30; // x2
  float v31; // s1
  size_t v32; // x2
  const void **v33; // x0
  __int64 v34; // x3
  const void **v35; // x4
  char *v36; // x4
  char *v37; // x5
  __int64 v38; // x6
  __int64 v39; // x7
  unsigned int v40; // w25
  unsigned int v41; // w3
  float v42; // s1
  char *v43; // x0
  float *v44; // x2
  float v45; // t1
  __int64 j; // x0
  __int64 v47; // [xsp+0h] [xbp-2080h] BYREF
  __int64 v48; // [xsp+8h] [xbp-2078h]
  float v49; // [xsp+6Ch] [xbp-2014h] BYREF
  char v50[8200]; // [xsp+70h] [xbp-2010h] BYREF
  __int64 v51; // [xsp+2078h] [xbp-8h]
  __int64 v52; // [xsp+2080h] [xbp+0h] BYREF

  v9 = &v52 - 2560;
  v9[512] = 0;
  *v9 = 0;
  v10 = n == 1;
  v47 = v7;
  v48 = v8;
  v11 = flag2 | (unsigned int)v10;
  v13 = n2;
  v49 = f;
  if ( flag )
  {
    if ( (_DWORD)v11 )
    {
      if ( n )
      {
        v16 = n - 1;
        v17 = 0;
        do
          vDSP_vsmul(a[v17], 1, &v49, a2[v17], 1, v13);
        while ( v17++ != v16 );
      }
    }
    else
    {
      sub_B19B0((const void **)a, (char *)*a2, n, n2, flag, v11, v10, (__int64)&_stack_chk_guard, v47);
      if ( v49 != 1.0 )
        vDSP_vsmul(*a2, 1, &v49, *a2, 1, (unsigned int)v13 * n);
    }
  }
  else if ( (_DWORD)v11 )
  {
    if ( n )
    {
      v20 = 0;
      do
      {
        v21 = 0;
        v22 = v49;
        v23 = a[v20];
        for ( i = a2[v20]; (unsigned int)v13 > (unsigned int)v21; ++v21 )
          i[v21] = (int)(float)(v22 * v23[v21]);
        ++v20;
      }
      while ( n > (unsigned int)v20 );
    }
  }
  else if ( n == 2 )
  {
    v25 = *a;
    v26 = a[1];
    v27 = *a2;
    if ( (_DWORD)v13 )
    {
      v28 = 0;
      v29 = 0;
      do
      {
        v30 = (unsigned int)(v28 + 1);
        v27[v28] = (int)(float)(f * v25[v29]);
        v28 += 2;
        v31 = v26[v29++];
        v27[v30] = (int)(float)(f * v31);
      }
      while ( (unsigned int)v13 > (unsigned int)v29 );
    }
  }
  else
  {
    v32 = 8LL * n;
    v33 = (const void **)(&v47 - 1536);
    v34 = (v32 + 15) & 0xFFFFFF000LL;
    v35 = (const void **)((char *)&v47 - v34 - 12288);
    if ( &v47 - 1536 != (__int64 *)v35 )
    {
      do
      {
        v33 -= 512;
        *v33 = nullptr;
      }
      while ( v33 != v35 );
    }
    *(const void **)((char *)v33 - (((v32 + 15) & 0xFFFFFFFF0LL) - v34)) = nullptr;
    memcpy(&v47, a, v32);
    for ( ; (_DWORD)v13; LODWORD(v13) = v13 - v40 )
    {
      if ( ((0x800 / n) & 0xFFFFFFFC) > (unsigned int)v13 )
        v40 = v13;
      else
        v40 = (0x800 / n) & 0xFFFFFFFC;
      sub_B19B0((const void **)&v47, v50, n, v40, (__int64)v36, (__int64)v37, v38, v39, v47);
      v41 = n * v40;
      v42 = v49;
      v37 = (char *)*a2;
      if ( n * v40 )
      {
        v43 = (char *)*a2;
        v44 = (float *)v50;
        v36 = &v37[2 * v41];
        do
        {
          v45 = *v44++;
          *(_WORD *)v43 = (int)(float)(v42 * v45);
          v43 += 2;
        }
        while ( v36 != v43 );
        v37 = (char *)*a2;
      }
      for ( j = 0; n > (unsigned int)j; ++j )
      {
        v36 = (char *)(*(&v47 + j) + 4LL * v40);
        *(&v47 + j) = (__int64)v36;
      }
      *a2 = &v37[2 * v41];
    }
  }
  return v51 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x000b1d90, for verification */
000B1D90  e91340d1  SUB             X9, SP, #4,LSL#12
000B1D94  290540d1  SUB             X9, X9, #1,LSL#12
000B1D98  3f0108f9  STR             XZR, [X9,#0x1000]
000B1D9C  3f0100f9  STR             XZR, [X9]
000B1DA0  3f0118f8  STUR            XZR, [X9,#-0x80]
000B1DA4  0c1084d2  MOV             X12, #0x2080
000B1DA8  ff632ccb  SUB             SP, SP, X12
000B1DAC  5f040071  CMP             W2, #1
000B1DB0  a51c0012  AND             W5, W5, #0xFF
000B1DB4  e6179f1a  CSET            W6, EQ
000B1DB8  9f1c0072  TST             W4, #0xFF
000B1DBC  fd7b00a9  STP             X29, X30, [SP,#0x2080+var_2080]
000B1DC0  fd030091  MOV             X29, SP
000B1DC4  a500062a  ORR             W5, W5, W6
000B1DC8  f55b02a9  STP             X21, X22, [SP,#0x2080+var_2060]
000B1DCC  d51b00f0  ADRP            X21, #__stack_chk_guard_ptr@PAGE
000B1DD0  a70247f9  LDR             X7, [X21,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1DD4  f35301a9  STP             X19, X20, [SP,#0x2080+var_2070]
000B1DD8  f403022a  MOV             W20, W2
000B1DDC  f76303a9  STP             X23, X24, [SP,#0x2080+var_2050]
000B1DE0  f303032a  MOV             W19, W3
000B1DE4  f80301aa  MOV             X24, X1
000B1DE8  f96b04a9  STP             X25, X26, [SP,#0x2080+var_2040]
000B1DEC  f90300aa  MOV             X25, X0
000B1DF0  fb2b00f9  STR             X27, [SP,#0x2080+var_2030]
000B1DF4  e80040f9  LDR             X8, [X7]
000B1DF8  a83f10f9  STR             X8, [X29,#0x2080+var_8]
000B1DFC  080080d2  MOV             X8, #0
000B1E00  a06f00bd  STR             S0, [X29,#0x2080+var_2014]
000B1E04  00040054  B.EQ            loc_B1E84
000B1E08  05090034  CBZ             W5, loc_B1F28
000B1E0C  e2010034  CBZ             W2, loc_B1E48
000B1E10  54040051  SUB             W20, W2, #1
000B1E14  b7b30191  ADD             X23, X29, #0x6C ; 'l'
000B1E18  160080d2  MOV             X22, #0
000B1E1C  1f2003d5  NOP
000B1E20  037b76f8  LDR             X3, [X24,X22,LSL#3]
000B1E24  240080d2  MOV             X4, #1
000B1E28  207b76f8  LDR             X0, [X25,X22,LSL#3]
000B1E2C  e10304aa  MOV             X1, X4
000B1E30  e50313aa  MOV             X5, X19
000B1E34  e20317aa  MOV             X2, X23
000B1E38  aaf7ff97  BL              vDSP_vsmul
000B1E3C  df0214eb  CMP             X22, X20
000B1E40  d6060091  ADD             X22, X22, #1
000B1E44  e1feff54  B.NE            loc_B1E20
000B1E48  b50247f9  LDR             X21, [X21,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1E4C  a13f50f9  LDR             X1, [X29,#0x2078]
000B1E50  a00240f9  LDR             X0, [X21]
000B1E54  200000ca  EOR             X0, X1, X0
000B1E58  401000b5  CBNZ            X0, loc_B2060
000B1E5C  bf030091  MOV             SP, X29
000B1E60  0c1084d2  MOV             X12, #0x2080
000B1E64  fd7b40a9  LDP             X29, X30, [SP,#0x2080+var_2080]
000B1E68  f35341a9  LDP             X19, X20, [SP,#0x2080+var_2070]
000B1E6C  f55b42a9  LDP             X21, X22, [SP,#0x2080+var_2060]
000B1E70  f76343a9  LDP             X23, X24, [SP,#0x2080+var_2050]
000B1E74  f96b44a9  LDP             X25, X26, [SP,#0x2080+var_2040]
000B1E78  fb2b40f9  LDR             X27, [SP,#0x2080+var_2030]
000B1E7C  ff632c8b  ADD             SP, SP, X12
000B1E80  c0035fd6  RET
000B1E84  85020034  CBZ             W5, loc_B1ED4
000B1E88  02feff34  CBZ             W2, loc_B1E48
000B1E8C  010080d2  MOV             X1, #0
000B1E90  040080d2  MOV             X4, #0
000B1E94  a16f40bd  LDR             S1, [X29,#0x6C]
000B1E98  237b61f8  LDR             X3, [X25,X1,LSL#3]
000B1E9C  027b61f8  LDR             X2, [X24,X1,LSL#3]
000B1EA0  33010034  CBZ             W19, loc_B1EC4
000B1EA4  1f2003d5  NOP
000B1EA8  607864bc  LDR             S0, [X3,X4,LSL#2]
000B1EAC  2008201e  FMUL            S0, S1, S0
000B1EB0  00b8a15e  FCVTZS          S0, S0
000B1EB4  4078247c  STR             H0, [X2,X4,LSL#1]
000B1EB8  84040091  ADD             X4, X4, #1
000B1EBC  7f02046b  CMP             W19, W4
000B1EC0  48ffff54  B.HI            loc_B1EA8
000B1EC4  21040091  ADD             X1, X1, #1
000B1EC8  9f02016b  CMP             W20, W1
000B1ECC  28feff54  B.HI            loc_B1E90
000B1ED0  deffff17  B               loc_B1E48
000B1ED4  5f080071  CMP             W2, #2
000B1ED8  41040054  B.NE            loc_B1F60
000B1EDC  041440a9  LDP             X4, X5, [X0]
000B1EE0  230040f9  LDR             X3, [X1]
000B1EE4  33fbff34  CBZ             W19, loc_B1E48
000B1EE8  01008052  MOV             W1, #0
000B1EEC  000080d2  MOV             X0, #0
000B1EF0  817860bc  LDR             S1, [X4,X0,LSL#2]
000B1EF4  22040011  ADD             W2, W1, #1
000B1EF8  0108211e  FMUL            S1, S0, S1
000B1EFC  21b8a15e  FCVTZS          S1, S1
000B1F00  6158217c  STR             H1, [X3,W1,UXTW#1]
000B1F04  21080011  ADD             W1, W1, #2
000B1F08  a17860bc  LDR             S1, [X5,X0,LSL#2]
000B1F0C  00040091  ADD             X0, X0, #1; int
000B1F10  7f02006b  CMP             W19, W0
000B1F14  0108211e  FMUL            S1, S0, S1
000B1F18  21b8a15e  FCVTZS          S1, S1
000B1F1C  6178227c  STR             H1, [X3,X2,LSL#1]
000B1F20  88feff54  B.HI            loc_B1EF0
000B1F24  c9ffff17  B               loc_B1E48
000B1F28  210040f9  LDR             X1, [X1]; dest
000B1F2C  a1feff97  BL              sub_B19B0
000B1F30  a16f40bd  LDR             S1, [X29,#0x6C]
000B1F34  00102e1e  FMOV            S0, #1.0
000B1F38  2020201e  FCMP            S1, S0
000B1F3C  60f8ff54  B.EQ            loc_B1E48
000B1F40  657e141b  MUL             W5, W19, W20
000B1F44  240080d2  MOV             X4, #1
000B1F48  030340f9  LDR             X3, [X24]
000B1F4C  e10304aa  MOV             X1, X4
000B1F50  a2b30191  ADD             X2, X29, #0x6C ; 'l'
000B1F54  e00303aa  MOV             X0, X3
000B1F58  62f7ff97  BL              vDSP_vsmul
000B1F5C  bbffff17  B               loc_B1E48
000B1F60  1a008152  MOV             W26, #0x800
000B1F64  827e7dd3  UBFIZ           X2, X20, #3, #0x20 ; ' '; n
000B1F68  413c0091  ADD             X1, X2, #0xF
000B1F6C  e00f40d1  SUB             X0, SP, #3,LSL#12
000B1F70  5a0bd41a  UDIV            W26, W26, W20
000B1F74  235c7492  AND             X3, X1, #0xFFFFFF000
000B1F78  040003cb  SUB             X4, X0, X3
000B1F7C  217c7c92  AND             X1, X1, #0xFFFFFFFF0
000B1F80  f7030091  MOV             X23, SP
000B1F84  1f0004eb  CMP             X0, X4
000B1F88  5a771e12  AND             W26, W26, #0xFFFFFFFC
000B1F8C  a0000054  B.EQ            loc_B1FA0
000B1F90  000440d1  SUB             X0, X0, #1,LSL#12
000B1F94  1f0004eb  CMP             X0, X4
000B1F98  1f0000f9  STR             XZR, [X0]
000B1F9C  a1ffff54  B.NE            loc_B1F90
000B1FA0  230003cb  SUB             X3, X1, X3
000B1FA4  000003cb  SUB             X0, X0, X3
000B1FA8  1f0000f9  STR             XZR, [X0]
000B1FAC  ff6321cb  SUB             SP, SP, X1
000B1FB0  bbc30191  ADD             X27, X29, #0x70 ; 'p'
000B1FB4  f6030091  MOV             X22, SP
000B1FB8  e10319aa  MOV             X1, X25; src
000B1FBC  e00316aa  MOV             X0, X22; dest
000B1FC0  6cf6fe97  BL              .memcpy
000B1FC4  b3040034  CBZ             W19, loc_B2058
000B1FC8  5f03136b  CMP             W26, W19
000B1FCC  e203142a  MOV             W2, W20
000B1FD0  5993931a  CSEL            W25, W26, W19, LS
000B1FD4  e1031baa  MOV             X1, X27; dest
000B1FD8  e303192a  MOV             W3, W25
000B1FDC  e00316aa  MOV             X0, X22; int
000B1FE0  74feff97  BL              sub_B19B0
000B1FE4  837e191b  MUL             W3, W20, W25
000B1FE8  a16f40bd  LDR             S1, [X29,#0x6C]
000B1FEC  050340f9  LDR             X5, [X24]
000B1FF0  a3010034  CBZ             W3, loc_B2024
000B1FF4  64040051  SUB             W4, W3, #1
000B1FF8  a1080091  ADD             X1, X5, #2
000B1FFC  e00305aa  MOV             X0, X5
000B2000  e2031baa  MOV             X2, X27
000B2004  2444248b  ADD             X4, X1, W4,UXTW#1
000B2008  404440bc  LDR             S0, [X2],#4
000B200C  2008201e  FMUL            S0, S1, S0
000B2010  00b8a15e  FCVTZS          S0, S0
000B2014  0024007c  STR             H0, [X0],#2
000B2018  9f0000eb  CMP             X4, X0
000B201C  61ffff54  B.NE            loc_B2008
000B2020  050340f9  LDR             X5, [X24]
000B2024  217f7ed3  UBFIZ           X1, X25, #2, #0x20 ; ' '
000B2028  000080d2  MOV             X0, #0
000B202C  f4000034  CBZ             W20, loc_B2048
000B2030  c47a60f8  LDR             X4, [X22,X0,LSL#3]
000B2034  8400018b  ADD             X4, X4, X1
000B2038  c47a20f8  STR             X4, [X22,X0,LSL#3]
000B203C  00040091  ADD             X0, X0, #1
000B2040  9f02006b  CMP             W20, W0
000B2044  68ffff54  B.HI            loc_B2030
000B2048  a044238b  ADD             X0, X5, W3,UXTW#1
000B204C  000300f9  STR             X0, [X24]
000B2050  7302196b  SUBS            W19, W19, W25
000B2054  a1fbff54  B.NE            loc_B1FC8
000B2058  ff020091  MOV             SP, X23
000B205C  7bffff17  B               loc_B1E48
000B2060  acf6fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b33b0  size 104 bytes  24 insns
 *   GetDCTIVModulationCoefficients
 *   [GetDCTIVModulationCoefficients]
 *   data refs: 0x331d20, 0x331f20, 0x42ce00, 0x43e520
 */
void *__fastcall GetDCTIVModulationCoefficients(
        int a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  bool v9; // zf
  void *result; // x0
  if ( a1 == 1024 )
    return &unk_331F20;
  v9 = a1 == 128;
  result = &unk_331D20;
  if ( !v9 )
    return nullptr;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b40b0  size 220 bytes  55 insns
 *   sub_B40B0
 *   [sub_B40B0]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_B40B0(
        float *a1,
        float *a2,
        int a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  float v9; // s0
  float *v10; // x5
  int v11; // w6
  float v12; // t1
  float v13; // s3
  float *v14; // x3
  float v15; // t1
  __int64 v17; // [xsp+18h] [xbp+18h]
  v9 = 1.0;
  *a2 = 1.0;
  a2[1] = *a1 * -2.0;
  if ( a3 > 1 )
  {
    v10 = a2 + 1;
    v11 = 1;
    while ( 1 )
    {
      v12 = a1[2];
      a1 += 2;
      v13 = v12 * -2.0;
      v10[1] = (float)(v9 + v9) + (float)(*v10 * (float)(v12 * -2.0));
      if ( v11 != 1 )
      {
        v14 = v10;
        do
        {
          *v14 = *v14 + (float)(*(v14 - 2) + (float)(*(v14 - 1) * v13));
          --v14;
        }
        while ( v14 != &v10[-(v11 - 2) - 1] );
      }
      ++v11;
      a2[1] = a2[1] + v13;
      if ( a3 == v11 )
        break;
      v15 = *v10++;
      v9 = v15;
    }
  }
  return v17 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00105590  size 104 bytes  24 insns
 *   GetSineWindowCoefficientPtr
 *   [GetSineWindowCoefficientPtr]
 *   data refs: 0x345e00, 0x347200, 0x42ce00, 0x43e520
 */
void *__fastcall GetSineWindowCoefficientPtr(
        int a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  bool v9; // zf
  void *result; // x0
  if ( a1 == 128 )
    return &unk_345E00;
  v9 = a1 == 1024;
  result = &unk_347200;
  if ( !v9 )
    return nullptr;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00105600  size 108 bytes  25 insns
 *   GetKBDWindowCoefficientPtr
 *   [GetKBDWindowCoefficientPtr]
 *   data refs: 0x345e00, 0x346000, 0x346200, 0x42ce00, 0x43e520
 */
void *__fastcall GetKBDWindowCoefficientPtr(
        int a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  bool v9; // zf
  void *result; // x0
  if ( a1 == 128 )
    return &unk_346000;
  v9 = a1 == 1024;
  result = &unk_346200;
  if ( !v9 )
    return nullptr;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00105670  size 380 bytes  92 insns
 *   sub_105670
 *   [sub_105670]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall sub_105670(
        __int64 a1,
        unsigned int a2,
        int *a3,
        unsigned int *a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  unsigned int v9; // w16
  int v10; // w11
  unsigned int v11; // w10
  unsigned int *v12; // x8
  unsigned int v13; // w6
  int v14; // w4
  v9 = a2 - 8;
  v10 = 0;
  v11 = 0;
  while ( 1 )
  {
    while ( 1 )
    {
      v12 = (unsigned int *)(a1 + v11);
      v13 = bswap32(*v12);
      v14 = (v12[1] << 8) & 0xFF0000 | (v12[1] >> 8) & 0xFF00 | HIBYTE(v12[1]) | (v12[1] << 24);
      if ( v14 == 1718775137 )
      {
        if ( ((v10 ^ 1) & (v13 > 0xB)) == 0 || v13 > a2 - v11 || bswap32(v12[2]) != 1634754915 )
          return 0;
        goto LABEL_22;
      }
      if ( v14 != 1634754915 )
        break;
      if ( v13 <= 0xC || v13 > a2 - v11 || v12[2] )
        return 0;
      v11 += v13;
      if ( v11 >= v9 )
        return 0;
    }
    if ( v14 == 1684107361 )
      break;
    if ( !v14 )
      return 0;
    if ( !v10 )
      goto LABEL_7;
    if ( v13 <= 7 || v13 > a2 - v11 )
      return 0;
LABEL_22:
    v11 += v13;
    if ( v11 >= v9 )
      return 0;
    v10 = 1;
  }
  if ( v13 > 0xC && v13 <= a2 - v11 )
  {
    v10 = v11 + 12;
    a2 = v13 - 12;
LABEL_7:
    *a3 = v10;
    *a4 = a2;
    return 1;
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x001057f0  size 244 bytes  60 insns
 *   sub_1057F0
 *   [sub_1057F0]
 *   data refs: 0x42ce00, 0x43e520
 */
void *__fastcall sub_1057F0(
        const void **a1,
        char *dest,
        unsigned int a3,
        int a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  char *v10; // x0
  char *v11; // x6
  bool v12; // cf
  unsigned int v13; // w4
  float v15; // s0
  __int64 v18; // [xsp+18h] [xbp+18h]
  v10 = dest;
  if ( a4 != 12 )
    return memcpy(dest, *a1, 4LL * a3);
  v11 = &dest[2 * a3];
  if ( a3 )
    v12 = dest >= v11;
  else
    v12 = 1;
  v13 = 0;
  if ( !v12 )
  {
    do
    {
      while ( 1 )
      {
        v15 = fmaxf(-1.0, fminf(1.0, *((float *)*a1 + v13)));
        if ( v15 >= 0.0 )
          break;
        ++v13;
        *(_WORD *)v10 = vcvts_n_s32_f32(v15, 0xFu);
        v10 += 2;
        if ( a3 <= v13 || v11 <= v10 )
          return (void *)(v18 ^ _stack_chk_guard);
      }
      ++v13;
      *(_WORD *)v10 = (int)(float)(v15 * 32767.0);
      v10 += 2;
    }
    while ( a3 > v13 && v11 > v10 );
  }
  return (void *)(v18 ^ _stack_chk_guard);
}


/* ------------------------------------------------------------------------ */
/* 0x001058f0  size 704 bytes  172 insns
 *   sub_1058F0
 *   [sub_1058F0]
 *   data refs: 0x0, 0x8, 0x76d20, 0x30ea60, 0x42a478, 0x42a4e0, 0x42a578, 0x42a5d0, 0x42a690, 0x42a6e0, 0x42a720, 0x42c9f8, 0x42cd00, 0x42ce00, 0x42d198, 0x42d3d0, 0x42d6c0, 0x42d700, 0x42d748, 0x42de28, 0x42de90, 0x43e3d0, 0x43e520, 0x43e7e8
 */
__int64 __fastcall sub_1058F0(
        __int64 *a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x21
  __int64 v10; // x22
  __int64 v11; // x23
  __int64 v12; // x0
  _QWORD *v13; // x0
  char *v14; // x0
  _QWORD *v15; // x19
  __int64 v16; // x20
  _DWORD *exception; // x0
  _DWORD *v19; // x0
  __int64 v20; // [xsp+20h] [xbp+20h]
  __int64 v21; // [xsp+28h] [xbp+28h]
  __int64 v22; // [xsp+30h] [xbp+30h]
  _QWORD v23[2]; // [xsp+48h] [xbp+48h] BYREF
  __int64 (__fastcall **v24)(std::bad_cast *); // [xsp+58h] [xbp+58h] BYREF
  exception v25; // [xsp+60h] [xbp+60h] BYREF
  __int64 v26; // [xsp+88h] [xbp+88h]
  v12 = *a1;
  v23[0] = 0;
  if ( (*(unsigned int (__fastcall **)(__int64, __int64, _QWORD *, _QWORD, void *, _QWORD))(*(_QWORD *)v12 + 40LL))(
         v12,
         5,
         v23,
         *(_QWORD *)(*(_QWORD *)v12 + 40LL),
         &_stack_chk_guard,
         0) )
  {
    exception = __cxa_allocate_exception(4u);
    *exception = 1650549858;
    __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
  }
  if ( v23[0] )
    v13 = (_QWORD *)(*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)v23[0] + 16LL))(v23[0]);
  else
    v13 = `typeinfo for'void;
  v14 = (char *)v13[1];
  if ( v14 != "P16DRCConfiguration" && (*v14 == 42 || strcmp(v14, "P16DRCConfiguration")) )
  {
    v20 = v9;
    v21 = v10;
    v22 = v11;
    v23[1] = off_42A4F0;
    v15 = __cxa_allocate_exception(0x38u);
    v15[2] = 0;
    v15[3] = 0;
    v15[4] = 0;
    *((_DWORD *)v15 + 10) = -1;
    *v15 = &unk_42A690;
    v15[1] = off_42A620;
    v15[6] = &off_42A6E0;
    v24 = off_42A588;
    *(_QWORD *)&v25.type = off_42A5B0;
    memset(&v25.name, 0, 24);
    LODWORD(v25.retval) = -1;
    boost::exception_detail::copy_boost_exception(
      (boost::exception_detail *)(v15 + 1),
      &v25,
      (const exception *)off_42A5B0);
    *v15 = off_42A738;
    v15[1] = off_42A770;
    v15[6] = &off_42A7A8;
    boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector((std::bad_cast *)&v24);
    __cxa_throw(
      v15,
      (struct type_info *)&`typeinfo for'boost::wrapexcept<boost::bad_any_cast>,
      (void (*)(void *))boost::wrapexcept<boost::bad_any_cast>::~wrapexcept);
  }
  v16 = *(_QWORD *)&byte_8[v23[0]];
  if ( !v16 )
  {
    v19 = __cxa_allocate_exception(4u);
    *v19 = 1650549858;
    __cxa_throw(v19, (struct type_info *)&`typeinfo for'int, nullptr);
  }
  (*(void (__fastcall **)(_QWORD))(*(_QWORD *)v23[0] + 8LL))(v23[0]);
  if ( v26 != _stack_chk_guard )
  {
    v20 = v9;
    v21 = v10;
    v22 = v11;
  }
  return v16;
}


/* ------------------------------------------------------------------------ */
/* 0x00105bb0  size 552 bytes  138 insns
 *   APACDecoder_DecodeFrame
 *   [APACDecoder_DecodeFrame]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_DecodeFrame(
        __int64 a1,
        int8x8_t a2,
        __int64 a3,
        int a4,
        char *a5,
        unsigned int a6,
        _DWORD *a7,
        __int64 a8,
        __int64 a9,
        __int64 a10)
{
  __int64 v10; // x23
  __int64 v11; // x24
  __int64 v12; // x25
  __int64 v16; // x0
  _DWORD *v17; // x5
  int v18; // w6
  unsigned int v19; // w20
  bool v20; // cc
  unsigned int v21; // w22
  __int64 v22; // x1
  __int64 v23; // x2
  __int64 v24; // x3
  __int64 v25; // x4
  __int64 v26; // x5
  __int64 v27; // x6
  __int64 v28; // x7
  DRCConfiguration *v29; // x0
  __int64 v30; // x5
  __int64 v31; // x6
  __int64 v32; // x7
  DRCConfiguration *v33; // x24
  __int64 v34; // x4
  unsigned int v36; // w20
  __int64 v37; // x4
  int v38; // w3
  void **v39; // x23
  unsigned int v40; // w20
  size_t v41; // x2
  __int64 v42; // [xsp+0h] [xbp+0h]
  __int64 v43; // [xsp+30h] [xbp+30h]
  __int64 v44; // [xsp+38h] [xbp+38h]
  __int64 v45; // [xsp+40h] [xbp+40h]
  __int64 v46; // [xsp+58h] [xbp+58h]
  if ( !*(_BYTE *)(a1 + 2128) )
  {
    v21 = -6718;
    goto LABEL_14;
  }
  v16 = *(_QWORD *)a1;
  v17 = *(_DWORD **)(*(_QWORD *)(a1 + 8) + 256LL);
  if ( *v17 )
  {
    v18 = (unsigned __int16)*v17;
    if ( *v17 != 0x10000 )
    {
      if ( a4 )
        goto LABEL_5;
LABEL_16:
      v21 = 0;
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v16 + 24LL))(v16);
      *(_DWORD *)(a1 + 2072) = 0;
      v10 = v43;
      v11 = v44;
      v12 = v45;
      goto LABEL_14;
    }
    a2.n64_u32[0] = v17[1];
    v18 = (unsigned __int8)vaddv_s8(vcnt_s8(a2));
    if ( !a4 )
      goto LABEL_16;
  }
  else
  {
    v18 = v17[2];
    if ( !a4 )
      goto LABEL_16;
  }
LABEL_5:
  v19 = v18 << 10;
  if ( v18 << 10 )
    v20 = v19 > a6;
  else
    v20 = 1;
  if ( v20 )
  {
    v21 = -6728;
    v10 = v43;
    v11 = v44;
    v12 = v45;
  }
  else
  {
    *(_QWORD *)(a1 + 16) = **(_QWORD **)(a1 + 2096);
    v21 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v16 + 32LL))(v16);
    v29 = (DRCConfiguration *)sub_1058F0((__int64 *)a1, v22, v23, v24, v25, v26, v27, v28, v42);
    v33 = v29;
    if ( *((_BYTE *)v29 + 181) )
    {
      DRCConfiguration::SetUniDrcDelayIsSetOnCodec(v29, 1u);
      DRCConfiguration::SetUniDrcDelayChanged(v33, 0);
    }
    v34 = *(unsigned int *)(a1 + 2072);
    if ( !v21 && v19 == *(unsigned __int16 *)(a1 + 2076) * (_DWORD)v34 )
    {
      v36 = *(_DWORD *)(a1 + 2120);
      if ( v36 )
      {
        if ( v36 >= (unsigned int)v34 )
        {
          *(_DWORD *)(a1 + 2120) = v36 - v34;
          *a7 = 0;
          v10 = v43;
          v11 = v44;
          v12 = v45;
        }
        else
        {
          v37 = (unsigned int)v34 - v36;
          *a7 = v37;
          v38 = *(unsigned __int16 *)(a1 + 2076);
          v39 = *(void ***)(a1 + 2096);
          v40 = v37 * v38;
          v41 = 4LL * (unsigned int)(v37 * v38);
          if ( v41 )
            memmove(*v39, (char *)*v39 + 4 * (unsigned int)(v38 * *(_DWORD *)(a1 + 2120)), v41);
          sub_1057F0((const void **)v39, a5, v40, *(_DWORD *)(a1 + 2108), v37, v30, v31, v32, v42);
          *(_DWORD *)(a1 + 2120) = 0;
          v10 = v43;
          v11 = v44;
          v12 = v45;
        }
      }
      else
      {
        *a7 = v34;
        sub_1057F0(
          *(const void ***)(a1 + 2096),
          a5,
          *(unsigned __int16 *)(a1 + 2076) * (_DWORD)v34,
          *(_DWORD *)(a1 + 2108),
          v34,
          v30,
          v31,
          v32,
          v42);
        v10 = v43;
        v11 = v44;
        v12 = v45;
      }
    }
    else
    {
      memset(a5, 0, 4LL * *(unsigned __int16 *)(a1 + 2076) * (unsigned int)v34);
      v10 = v43;
      v11 = v44;
      v12 = v45;
    }
  }
LABEL_14:
  if ( v46 != _stack_chk_guard )
  {
    v43 = v10;
    v44 = v11;
    v45 = v12;
  }
  return v21;
}


/* ------------------------------------------------------------------------ */
/* 0x00105de0  size 132 bytes  33 insns
 *   APACDecoder_Reset
 *   [APACDecoder_Reset]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_Reset(
        _QWORD *a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  unsigned int v9; // w20
  if ( a1 )
  {
    v9 = 0;
    *(_QWORD *)((char *)a1 + 2116) = 0;
    if ( *((_BYTE *)a1 + 2128) )
      (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*a1 + 24LL))(*a1);
  }
  else
  {
    return (unsigned int)-6705;
  }
  return v9;
}


/* ------------------------------------------------------------------------ */
/* 0x00105e70  size 440 bytes  105 insns
 *   APACDecoder_GetParameter
 *   [APACDecoder_GetParameter]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetParameter(
        __int64 a1,
        int a2,
        __int64 a3,
        int *a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x21
  bool v10; // zf
  __int16 *ProfileConfig; // x0
  int8x8_t v14; // d0
  _DWORD *v15; // x2
  int *v16; // x1
  int *v17; // x4
  int v18; // w3
  int i; // w5
  int v20; // w0
  __int64 result; // x0
  __int64 v22; // [xsp+20h] [xbp+20h]
  __int64 v23; // [xsp+38h] [xbp+38h]
  if ( a3 )
    v10 = a4 == nullptr;
  else
    v10 = 1;
  if ( v10 || a1 == 0 )
  {
    result = 4294960591LL;
    goto LABEL_25;
  }
  if ( !*(_BYTE *)(a1 + 2128) )
  {
    result = 4294960578LL;
    goto LABEL_25;
  }
  v9 = *(_QWORD *)(a1 + 8);
  if ( a2 == 1885432678 )
  {
    *a4 = 1024;
    result = 0;
    goto LABEL_25;
  }
  if ( a2 > 1885432678 )
  {
    if ( a2 == 1886546285 )
    {
      *a4 = 2048;
      result = 0;
      goto LABEL_25;
    }
    goto LABEL_27;
  }
  if ( a2 == 1668248434 )
  {
    result = 0;
    *a4 = *(_DWORD *)(v9 + 36);
    goto LABEL_25;
  }
  if ( a2 != 1868786720 )
  {
LABEL_27:
    result = 4294960591LL;
    goto LABEL_25;
  }
  ProfileConfig = apac::GetProfileConfig(*(unsigned __int16 *)(v9 + 32));
  if ( (unsigned __int16)*ProfileConfig == 0xFFFF )
  {
    result = 560423284;
  }
  else
  {
    v15 = *(_DWORD **)(v9 + 256);
    v16 = *((int **)ProfileConfig + 3);
    v17 = *((int **)ProfileConfig + 4);
    if ( *v15 )
    {
      v18 = (unsigned __int16)*v15;
      if ( *v15 == 0x10000 )
      {
        v14.n64_u32[0] = v15[1];
        v18 = (unsigned __int8)vaddv_s8(vcnt_s8(v14));
      }
    }
    else
    {
      v18 = v15[2];
    }
    for ( i = 0; v16 != v17; ++v16 )
    {
      v20 = *v16;
      if ( (*v16 & 0xFFFF0000) == 0x930000 )
      {
        i = v20 | v18;
      }
      else if ( v18 == (unsigned __int16)v20 )
      {
        i = *v16;
        break;
      }
    }
    *a4 = i;
    result = 0;
  }
LABEL_25:
  if ( v23 != _stack_chk_guard )
    v22 = v9;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00106030  size 124 bytes  31 insns
 *   APACDecoder_GetCookieOffset
 *   [APACDecoder_GetCookieOffset]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetCookieOffset(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v10; // [xsp+0h] [xbp+0h]
  int v11; // [xsp+20h] [xbp+20h] BYREF
  unsigned int v12; // [xsp+24h] [xbp+24h] BYREF
  v11 = 0;
  v12 = 0;
  if ( a2 > 8u && (unsigned __int8)sub_105670(a1, a2, &v11, &v12, a5, a6, a7, a8, v10) )
    return (unsigned __int16)v11;
  else
    return 58831;
}


/* ------------------------------------------------------------------------ */
/* 0x001060b0  size 80 bytes  20 insns
 *   APACDecoder_GetVersion
 *   [APACDecoder_GetVersion]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetVersion(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return bswap32(*(_DWORD *)(a1 + a2)) >> 16;
}


/* ------------------------------------------------------------------------ */
/* 0x00106100  size 80 bytes  20 insns
 *   APACDecoder_GetProfile
 *   [APACDecoder_GetProfile]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetProfile(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return (unsigned __int16)bswap32(*(_DWORD *)(a1 + a2)) >> 10;
}


/* ------------------------------------------------------------------------ */
/* 0x00106150  size 92 bytes  22 insns
 *   APACDecoder_SampleRate
 *   [APACDecoder_SampleRate]
 *   data refs: 0x3483a8, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_SampleRate(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return dword_3483A8[((unsigned __int64)bswap32(*(_DWORD *)(a1 + a2)) >> 15) & 0x3F];
}


/* ------------------------------------------------------------------------ */
/* 0x001061b0  size 88 bytes  22 insns
 *   APACDecoder_FrameLength
 *   [APACDecoder_FrameLength]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_FrameLength(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return (unsigned __int8)((bswap32(*(_DWORD *)(a1 + a2)) & 0x7E00) == 0) << 10;
}


/* ------------------------------------------------------------------------ */
/* 0x00106210  size 80 bytes  20 insns
 *   APACDecoder_NumberOfChannels
 *   [APACDecoder_NumberOfChannels]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_NumberOfChannels(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return (unsigned __int8)((unsigned __int64)bswap32(*(_DWORD *)(a1 + a2)) >> 1);
}


/* ------------------------------------------------------------------------ */
/* 0x00106260  size 552 bytes  128 insns
 *   APACDecoder_SetParameter
 *   [APACDecoder_SetParameter]
 *   data refs: 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
__int64 __fastcall APACDecoder_SetParameter(
        __int64 *a1,
        __int64 a2,
        int a3,
        unsigned int *a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x21
  void *v11; // x5
  bool v12; // zf
  __int64 v13; // x3
  __int64 v15; // x2
  __int64 v16; // x4
  __int64 v17; // x0
  unsigned int v18; // w1
  __int64 result; // x0
  __int64 v20; // x2
  DRCConfiguration *v21; // x0
  __int64 v22; // x1
  __int64 v23; // x2
  __int64 v24; // x3
  __int64 v25; // x4
  __int64 v26; // x5
  __int64 v27; // x6
  __int64 v28; // x7
  _DWORD *exception; // x0
  __int64 v30; // [xsp+0h] [xbp+0h]
  __int64 v31; // [xsp+20h] [xbp+20h]
  __int64 v32; // [xsp+38h] [xbp+38h]
  v11 = &_stack_chk_guard;
  if ( a1 )
    v12 = a4 == nullptr;
  else
    v12 = 1;
  v13 = 0;
  if ( !v12 )
  {
    v31 = v9;
    if ( (_DWORD)a2 == 1634493037 )
    {
      v17 = sub_1058F0(a1, a2, 1634493037, 0, a5, (__int64)&_stack_chk_guard, a7, a8, v30);
      if ( a3 != 4 )
        goto LABEL_42;
      if ( (unsigned __int8)DRCConfiguration::SetAlbumMode((DRCConfiguration *)v17, *a4 != 0) )
        goto LABEL_17;
      goto LABEL_24;
    }
    if ( (int)a2 <= 1634493037 )
    {
      v20 = 1584427631;
      if ( (_DWORD)a2 == 1584427631 )
      {
LABEL_30:
        v17 = sub_1058F0(a1, a2, v20, 0, a5, (__int64)&_stack_chk_guard, a7, a8, v30);
        if ( a3 != 4 )
          goto LABEL_42;
        if ( (unsigned __int8)DRCConfiguration::SetUniDRCCompressionProfile((DRCConfiguration *)v17, *a4) )
          goto LABEL_17;
LABEL_24:
        exception = __cxa_allocate_exception(4u);
        *exception = 560423284;
        __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
      }
      if ( (int)a2 <= 1584427631 )
      {
        if ( (_DWORD)a2 == 1583510388 )
        {
          v21 = (DRCConfiguration *)sub_1058F0(a1, a2, 1583510388, 0, a5, (__int64)&_stack_chk_guard, a7, a8, v30);
          if ( a3 == 4 )
          {
            if ( !(unsigned __int8)DRCConfiguration::SetBoostFactor(v21, *(float *)a4) )
              goto LABEL_24;
            goto LABEL_17;
          }
        }
        else
        {
          if ( (_DWORD)a2 != 1583576436 )
            goto LABEL_17;
          v21 = (DRCConfiguration *)sub_1058F0(a1, a2, 1583576436, 0, a5, (__int64)&_stack_chk_guard, a7, a8, v30);
          if ( a3 == 4 )
          {
            if ( !(unsigned __int8)DRCConfiguration::SetCutFactor(v21, *(float *)a4) )
              goto LABEL_24;
            goto LABEL_17;
          }
        }
LABEL_41:
        sub_70CF4((__int64)v21, v22, v23, v24, v25, v26, v27, v28, v30);
      }
      v15 = 1584428140;
      if ( (_DWORD)a2 != 1584428140 )
        goto LABEL_17;
    }
    else
    {
      v15 = 1886418028;
      if ( (_DWORD)a2 != 1886418028 )
      {
        if ( (int)a2 > 1886418028 )
        {
          v16 = (__int64)a1;
          v17 = 1886546285;
          if ( (_DWORD)a2 == 1886546285 )
          {
            if ( a3 == 4 )
            {
              v18 = *a4;
              result = 0;
              *(_DWORD *)(v16 + 2116) = *a4;
              *(_DWORD *)(v16 + 2120) = v18;
              v9 = v31;
              goto LABEL_18;
            }
LABEL_42:
            sub_70CF4(v17, a2, v15, v13, v16, (__int64)v11, a7, a8, v30);
          }
          goto LABEL_17;
        }
        v20 = 1685218160;
        if ( (_DWORD)a2 != 1685218160 )
        {
          if ( (_DWORD)a2 == 1835299427 )
          {
            v17 = sub_1058F0(a1, a2, 1835299427, 0, a5, (__int64)&_stack_chk_guard, a7, a8, v30);
            if ( a3 != 4 )
              goto LABEL_42;
            if ( !(unsigned __int8)DRCConfiguration::SetCompressionMode((DRCConfiguration *)v17, *a4) )
              goto LABEL_24;
          }
LABEL_17:
          v9 = v31;
          result = 0;
          goto LABEL_18;
        }
        goto LABEL_30;
      }
    }
    v21 = (DRCConfiguration *)sub_1058F0(a1, a2, v15, 0, a5, (__int64)&_stack_chk_guard, a7, a8, v30);
    if ( a3 == 4 )
    {
      if ( !(unsigned __int8)DRCConfiguration::SetTargetLevel(v21, *(float *)a4) )
        goto LABEL_24;
      goto LABEL_17;
    }
    goto LABEL_41;
  }
  result = 4294960591LL;
LABEL_18:
  if ( v32 != _stack_chk_guard )
    v31 = v9;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00106490  size 876 bytes  216 insns
 *   APACDecoder_Configure
 *   [APACDecoder_Configure]
 *   data refs: 0x348200, 0x348248, 0x3482a8, 0x3482e8, 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
__int64 __fastcall APACDecoder_Configure(
        __int64 *a1,
        _DWORD *a2,
        __int64 a3,
        unsigned __int16 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x23
  __int64 v10; // x24
  __int64 v11; // x25
  __int64 v14; // x23
  __int64 v15; // x24
  unsigned int v16; // w20
  unsigned int cache_high; // w20
  _DWORD *exception; // x0
  int v21; // w3
  int v22; // w4
  unsigned int v23; // w0
  int8x8_t v24; // d0
  _DWORD *v25; // x1
  int v26; // w4
  int8x8_t v27; // d0
  int v28; // w0
  _DWORD *v29; // x1
  int v30; // w2
  int v31; // w4
  unsigned __int8 v32; // b0
  _QWORD *v33; // x0
  unsigned __int64 v34; // x3
  __int64 v35; // x5
  unsigned __int64 v36; // x1
  unsigned __int64 v37; // x3
  int v38; // w3
  int v39; // w0
  int v40; // w3
  unsigned int v41; // w0
  _DWORD *v42; // x0
  __int64 v43; // [xsp+0h] [xbp+0h]
  __int64 v44; // [xsp+30h] [xbp+30h]
  __int64 v45; // [xsp+38h] [xbp+38h]
  __int64 v46; // [xsp+40h] [xbp+40h]
  unsigned int v47; // [xsp+58h] [xbp+58h] BYREF
  unsigned int v48; // [xsp+5Ch] [xbp+5Ch] BYREF
  TBitstreamReader_u32 v49; // [xsp+60h] [xbp+60h] BYREF
  __int64 v50; // [xsp+78h] [xbp+78h]
  if ( a1 )
  {
    v47 = 0;
    v48 = 0;
    v15 = *a1;
    v14 = a1[1];
    if ( a4 <= 8u
      || !(unsigned __int8)sub_105670(a3, a4, (int *)&v47, &v48, (__int64)&_stack_chk_guard, a6, a7, a8, v43) )
    {
      v9 = v44;
      v10 = v45;
      v16 = -6705;
      goto LABEL_4;
    }
    TBitstreamReader<unsigned int>::TBitstreamReader(&v49, (const unsigned __int8 *)(a3 + v47), v48);
    v49.bitsLeft -= 16;
    cache_high = HIWORD(v49.cache);
    if ( v49.bitsLeft < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(&v49, v49.cur);
      cache_high |= v49.cache >> (LOBYTE(v49.bitsLeft) + 32);
      v41 = v49.cache << -LOBYTE(v49.bitsLeft);
      if ( v49.bitsLeft == -32 )
        v41 = 0;
      v49.cur += 4;
      v49.cache = v41;
      v49.bitsLeft += 32;
    }
    else
    {
      v49.cache <<= 16;
    }
    if ( cache_high != 2048 )
    {
      exception = __cxa_allocate_exception(4u);
      *exception = 1650549858;
      __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
    }
    v16 = apac::GlobalConfig::Deserialize((apac__GlobalConfig *)v14, &v49);
    if ( !v16 )
    {
      *((_DWORD *)a1 + 528) = 2048;
      v21 = *(unsigned __int16 *)(v14 + 32);
      *((_WORD *)a1 + 1062) = v21;
      v22 = *(unsigned __int8 *)(v14 + 34);
      *((_WORD *)a1 + 1063) = v22;
      __printf_chk(1, "mAPACBsVersion = 0x%04x mAPACProfile = %d and mAPACProfileLevel = %d  \n", 2048, v21, v22);
      if ( *((_WORD *)a1 + 1062) != 31 )
      {
        v42 = __cxa_allocate_exception(4u);
        *v42 = 560423284;
        __cxa_throw(v42, (struct type_info *)&`typeinfo for'int, nullptr);
      }
      v23 = (*(__int64 (__fastcall **)(__int64, __int64))(*(_QWORD *)v15 + 16LL))(v15, v14);
      v25 = *(_DWORD **)(v14 + 256);
      v16 = v23;
      if ( *v25 )
      {
        v26 = (unsigned __int16)*v25;
        if ( *v25 == 0x10000 )
        {
          v24.n64_u32[0] = v25[1];
          v26 = (unsigned __int8)vaddv_s8(vcnt_s8(v24));
        }
      }
      else
      {
        v26 = v25[2];
      }
      __printf_chk(
        1,
        "## APACDecoder_Initialize ## sampleRate = %d packetSizePerChannel = %d  numChannels = %d \n",
        *(_DWORD *)(v14 + 36),
        1024,
        v26);
      if ( !v16 )
      {
        v27.n64_u32[1] = 939524096;
        *((_DWORD *)a1 + 526) = a2[7];
        v28 = a2[2];
        *((_DWORD *)a1 + 521) = 9;
        *((_DWORD *)a1 + 527) = v28;
        *((_DWORD *)a1 + 516) = 939524096;
        v29 = *(_DWORD **)(v14 + 256);
        if ( *v29 )
        {
          LOWORD(v30) = *v29;
          v31 = (unsigned __int16)v30;
          if ( *v29 == 0x10000 )
          {
            v27.n64_u32[0] = v29[1];
            v32 = vaddv_s8(vcnt_s8(v27));
            LOWORD(v30) = v32;
            v31 = v32;
          }
        }
        else
        {
          v30 = v29[2];
          v31 = (unsigned __int16)v30;
        }
        v33 = (_QWORD *)a1[262];
        v30 = (unsigned __int16)v30;
        *((_DWORD *)a1 + 518) = 1024;
        *((_WORD *)a1 + 1038) = v30;
        v34 = (unsigned __int64)(unsigned __int16)v31 << 10;
        v35 = v33[1];
        v36 = (v35 - *v33) >> 2;
        if ( v34 > v36 )
        {
          std::vector<float>::_M_default_append(v33, v34 - v36);
          v30 = *((unsigned __int16 *)a1 + 1038);
          v31 = v30;
        }
        else if ( v34 < v36 )
        {
          v37 = *v33 + ((unsigned __int64)(unsigned __int16)v31 << 12);
          if ( v35 != v37 )
          {
            v33[1] = v37;
            v31 = *((unsigned __int16 *)a1 + 1038);
            v30 = v31;
          }
        }
        *((_BYTE *)a1 + 2128) = 1;
        v38 = a2[6];
        if ( a2[2] == 12 )
          v39 = 2;
        else
          v39 = 4;
        if ( v38 != v31 )
        {
          __printf_chk(1, "## APACDecoder_Initialize Mismtch channels config:%d format:%d\n", v30, v38);
          goto LABEL_30;
        }
        v40 = a2[3];
        if ( v40 != v39 * v31 )
        {
          __printf_chk(1, "## APACDecoder_Initialize Mismtch bytes per packet config:%d format:%d\n", v39 * v31, v40);
LABEL_30:
          *((_BYTE *)a1 + 2128) = 0;
          v16 = -6748;
          v9 = v44;
          v10 = v45;
          goto LABEL_4;
        }
      }
    }
    v9 = v44;
    v10 = v45;
  }
  else
  {
    v16 = -6705;
  }
LABEL_4:
  if ( v50 != _stack_chk_guard )
  {
    v44 = v9;
    v45 = v10;
    v46 = v11;
  }
  return v16;
}


/* ------------------------------------------------------------------------ */
/* 0x00106800  size 1028 bytes  257 insns
 *   APACDecoder_Create
 *   [APACDecoder_Create]
 *   data refs: 0x72430, 0x42a278, 0x42b900, 0x42b930, 0x42ca50, 0x42ce00, 0x42d240, 0x42d910, 0x42ddd8, 0x43e520
 */
__int64 __fastcall APACDecoder_Create(
        APACDecoder ***a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x19
  __int64 v10; // x20
  __int64 v11; // x23
  __int64 v12; // x24
  __int64 v13; // x25
  __int64 v14; // x26
  __int64 v15; // x27
  __int64 v16; // x28
  APACDecoder **v18; // x20
  APACDecoder *v19; // x19
  __int64 v20; // x0
  __int64 v21; // x19
  __int64 v22; // x0
  void **v23; // x0
  void **v24; // x19
  APACDecoder *v25; // x24
  __int64 result; // x0
  void (__fastcall *v27)(APACASPDecoder *); // x1
  void *v28; // x0
  APACDecoder *v29; // x22
  void *v30; // x0
  void *v31; // x0
  _QWORD *v32; // x19
  _QWORD *i; // x23
  void *v34; // x0
  _QWORD *v35; // x19
  _QWORD *j; // x23
  void *v37; // x0
  void *v38; // x0
  void *v39; // x0
  _QWORD *v40; // x19
  _QWORD *k; // x23
  void *v42; // x0
  __int64 v43; // [xsp+10h] [xbp+10h]
  __int64 v44; // [xsp+18h] [xbp+18h]
  __int64 v45; // [xsp+30h] [xbp+30h]
  __int64 v46; // [xsp+38h] [xbp+38h]
  __int64 v47; // [xsp+40h] [xbp+40h]
  __int64 v48; // [xsp+48h] [xbp+48h]
  __int64 v49; // [xsp+50h] [xbp+50h]
  __int64 v50; // [xsp+58h] [xbp+58h]
  __int64 v51; // [xsp+68h] [xbp+68h]
  if ( !a1 )
  {
    result = 4294960591LL;
    goto LABEL_6;
  }
  v18 = (APACDecoder **)calloc(1u, 0x858u);
  if ( v18 )
  {
    v45 = v11;
    v46 = v12;
    v19 = (APACDecoder *)operator new(0x1E8u);
    APACDecoder::APACDecoder(v19);
    *v18 = v19;
    *((_QWORD *)v19 + 58) = 0;
    *((_QWORD *)v19 + 59) = 0;
    *((_QWORD *)v19 + 60) = 0;
    *(_QWORD *)v19 = off_42A288;
    v20 = operator new(0x118u);
    *(_QWORD *)(v20 + 8) = v20;
    v21 = v20;
    *(_QWORD *)v20 = off_42B910;
    *(_QWORD *)(v20 + 24) = v20;
    *(_QWORD *)(v20 + 16) = off_42B940;
    *(_WORD *)(v20 + 32) = 31;
    *(_BYTE *)(v20 + 34) = 6;
    *(_QWORD *)(v20 + 36) = 0xFFFF00000000LL;
    apac::AncillaryConfig::AncillaryConfig((apac__AncillaryConfig *)(v20 + 48));
    *(_BYTE *)(v21 + 88) = 0;
    *(_QWORD *)(v21 + 96) = 0;
    *(_QWORD *)(v21 + 104) = 0;
    *(_QWORD *)(v21 + 112) = 0;
    *(_DWORD *)(v21 + 120) = 0;
    *(_QWORD *)(v21 + 128) = 0;
    *(_QWORD *)(v21 + 136) = 0;
    *(_QWORD *)(v21 + 144) = 0;
    *(_QWORD *)(v21 + 152) = 0;
    *(_QWORD *)(v21 + 160) = 0;
    *(_QWORD *)(v21 + 168) = 0;
    *(_QWORD *)(v21 + 176) = 0;
    *(_QWORD *)(v21 + 184) = 0;
    *(_QWORD *)(v21 + 192) = 0;
    *(_QWORD *)(v21 + 200) = 0;
    *(_QWORD *)(v21 + 208) = 0;
    *(_QWORD *)(v21 + 216) = 0;
    *(_QWORD *)(v21 + 224) = 0;
    *(_QWORD *)(v21 + 232) = 0;
    *(_QWORD *)(v21 + 240) = 0;
    *(_WORD *)(v21 + 248) = 0;
    *(_QWORD *)(v21 + 256) = 0;
    *(_QWORD *)(v21 + 264) = 0;
    *(_QWORD *)(v21 + 272) = 0;
    v22 = operator new(0xCu);
    v18[1] = (APACDecoder *)v21;
    *(_QWORD *)(v21 + 256) = v22;
    *(_QWORD *)(v21 + 264) = v22 + 12;
    *(_QWORD *)v22 = 0;
    *(_DWORD *)(v22 + 8) = 0;
    *(_QWORD *)(v21 + 272) = v22 + 12;
    v23 = (void **)operator new(0x18u);
    *v23 = nullptr;
    v23[1] = nullptr;
    v24 = v23;
    v25 = *v18;
    v23[2] = nullptr;
    v18[262] = (APACDecoder *)v23;
    if ( v25 )
    {
      if ( v18[1] )
      {
        *a1 = v18;
        result = 0;
        v9 = v43;
        v10 = v44;
        v11 = v45;
        v12 = v46;
        goto LABEL_6;
      }
      v27 = *(void (__fastcall **)(APACASPDecoder *))(*(_QWORD *)v25 + 8LL);
      if ( v27 == APACASPDecoder::~APACASPDecoder )
      {
        v28 = *((void **)v25 + 58);
        *(_QWORD *)v25 = off_42A288;
        if ( v28 )
          operator delete(v28);
        APACDecoder::~APACDecoder(v25);
        operator delete(v25);
      }
      else
      {
        v27(v25);
      }
      v29 = v18[1];
      if ( !v29 )
        goto LABEL_41;
    }
    else
    {
      v29 = v18[1];
      if ( !v29 )
        goto LABEL_44;
    }
    v30 = *((void **)v29 + 32);
    if ( v30 )
      operator delete(v30);
    v31 = *((void **)v29 + 28);
    if ( v31 )
      operator delete(v31);
    v32 = *((_QWORD **)v29 + 25);
    for ( i = *((_QWORD **)v29 + 26); i != v32; ++v32 )
    {
      if ( *v32 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v32 + 8LL))(*v32);
    }
    v34 = *((void **)v29 + 25);
    if ( v34 )
      operator delete(v34);
    v35 = *((_QWORD **)v29 + 22);
    for ( j = *((_QWORD **)v29 + 23); j != v35; ++v35 )
    {
      if ( *v35 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v35 + 8LL))(*v35);
    }
    v37 = *((void **)v29 + 22);
    if ( v37 )
      operator delete(v37);
    v38 = *((void **)v29 + 19);
    if ( v38 )
      operator delete(v38);
    v39 = *((void **)v29 + 16);
    if ( v39 )
      operator delete(v39);
    v40 = *((_QWORD **)v29 + 12);
    for ( k = *((_QWORD **)v29 + 13); k != v40; ++v40 )
    {
      if ( *v40 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v40 + 8LL))(*v40);
    }
    v42 = *((void **)v29 + 12);
    if ( v42 )
      operator delete(v42);
    apac::AncillaryConfig::~AncillaryConfig((APACDecoder *)((char *)v29 + 48));
    operator delete(v29);
LABEL_41:
    v24 = (void **)v18[262];
    if ( !v24 )
    {
LABEL_45:
      free(v18);
      result = 4294960591LL;
      v9 = v43;
      v10 = v44;
      v11 = v45;
      v12 = v46;
      goto LABEL_6;
    }
    if ( *v24 )
      operator delete(*v24);
LABEL_44:
    operator delete(v24);
    goto LABEL_45;
  }
  result = 4294960568LL;
  v9 = v43;
  v10 = v44;
LABEL_6:
  if ( v51 != _stack_chk_guard )
  {
    v43 = v9;
    v44 = v10;
    v45 = v11;
    v46 = v12;
    v47 = v13;
    v48 = v14;
    v49 = v15;
    v50 = v16;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00106c10  size 492 bytes  123 insns
 *   APACDecoder_Dispose
 *   [APACDecoder_Dispose]
 *   data refs: 0x72430, 0x42a278, 0x42ca50, 0x42ce00, 0x42ddd8, 0x43e520
 */
__int64 __fastcall APACDecoder_Dispose(
        APACDecoder **ptr,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x23
  APACDecoder *v11; // x19
  void (__fastcall *v12)(APACASPDecoder *); // x1
  void *v13; // x0
  APACDecoder *v14; // x19
  void *v15; // x0
  void *v16; // x0
  _QWORD *v17; // x22
  _QWORD *v18; // x23
  _QWORD *v19; // x22
  _QWORD *v20; // x23
  void *v21; // x0
  void *v22; // x0
  _QWORD *v23; // x22
  _QWORD *v24; // x23
  void **v25; // x19
  __int64 v27; // [xsp+30h] [xbp+30h]
  __int64 v28; // [xsp+48h] [xbp+48h]
  if ( ptr )
  {
    v11 = *ptr;
    if ( *ptr )
    {
      v12 = *(void (__fastcall **)(APACASPDecoder *))(*(_QWORD *)v11 + 8LL);
      if ( v12 == APACASPDecoder::~APACASPDecoder )
      {
        v13 = *((void **)v11 + 58);
        *(_QWORD *)v11 = off_42A288;
        if ( v13 )
          operator delete(v13);
        APACDecoder::~APACDecoder(v11);
        operator delete(v11);
      }
      else
      {
        v12(*ptr);
      }
    }
    v14 = ptr[1];
    if ( v14 )
    {
      v27 = v9;
      v15 = *((void **)v14 + 32);
      if ( v15 )
        operator delete(v15);
      v16 = *((void **)v14 + 28);
      if ( v16 )
        operator delete(v16);
      v17 = *((_QWORD **)v14 + 25);
      v18 = *((_QWORD **)v14 + 26);
      if ( v17 != v18 )
      {
        do
        {
          if ( *v17 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v17 + 8LL))(*v17);
          ++v17;
        }
        while ( v18 != v17 );
        v18 = *((_QWORD **)v14 + 25);
      }
      if ( v18 )
        operator delete(v18);
      v19 = *((_QWORD **)v14 + 22);
      v20 = *((_QWORD **)v14 + 23);
      if ( v19 != v20 )
      {
        do
        {
          if ( *v19 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v19 + 8LL))(*v19);
          ++v19;
        }
        while ( v20 != v19 );
        v20 = *((_QWORD **)v14 + 22);
      }
      if ( v20 )
        operator delete(v20);
      v21 = *((void **)v14 + 19);
      if ( v21 )
        operator delete(v21);
      v22 = *((void **)v14 + 16);
      if ( v22 )
        operator delete(v22);
      v23 = *((_QWORD **)v14 + 12);
      v24 = *((_QWORD **)v14 + 13);
      if ( v23 != v24 )
      {
        do
        {
          if ( *v23 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v23 + 8LL))(*v23);
          ++v23;
        }
        while ( v24 != v23 );
        v24 = *((_QWORD **)v14 + 12);
      }
      if ( v24 )
        operator delete(v24);
      apac::AncillaryConfig::~AncillaryConfig((APACDecoder *)((char *)v14 + 48));
      operator delete(v14);
      v9 = v27;
    }
    v25 = (void **)ptr[262];
    if ( v25 )
    {
      if ( *v25 )
        operator delete(*v25);
      operator delete(v25);
    }
    free(ptr);
  }
  if ( v28 != _stack_chk_guard )
    v27 = v9;
  return 0;
}


