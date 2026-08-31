/* ============================================================================
 * DCT_IV
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 1564 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b6e80    560  DCT_IV::PreModulation(float const*, DSPSplitComplex*, float) const */
/* 0x000b70b0    656  DCT_IV::PostModulationExtend(DSPSplitComplex const*, float*, float*) const */
/* 0x000b7340    100  DCT_IV::DCT_IV() */
/* 0x000b73b0     88  DCT_IV::~DCT_IV() */
/* 0x000b7410    160  DCT_IV::Initialize(unsigned int) */

/* ------------------------------------------------------------------------ */
/* 0x000b6e80  size 560 bytes  140 insns
 *   DCT_IV::PreModulation(float const*, DSPSplitComplex*, float) const
 *   [_ZNK6DCT_IV13PreModulationEPKfP15DSPSplitComplexf]
 *   data refs: 0x343050, 0x42ce00, 0x43e520
 */
/* !! Hex-Rays reported 'local variable allocation has failed' for this
 * !! function. Treat the pseudocode below as a sketch, not as source.
 * !! The disassembly appended after it is the ground truth. */
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall DCT_IV::PreModulation(DCT_IV *this, const float *a, DSPSplitComplex *a2, float f)
{
  int v4; // w10
  unsigned int v5; // w5
  __int64 v6; // x16
  int32x4_t v7; // q17
  __int64 v8; // x9
  __int64 v9; // x4
  unsigned int v10; // w6
  __int64 v11; // x11
  __int64 v12; // x13
  unsigned __int64 v13; // x0
  __int64 v14; // x12
  int32x4_t v15; // q16
  __int64 v16; // x7
  int32x4_t v17; // q20
  float32x4_t v18; // q4
  float32x4_t v19; // q5
  float32x4_t v20; // q1
  int32x4_t v21; // q4
  float32x4_t v22; // q5
  unsigned int v24; // w7
  int v25; // w12
  unsigned int v26; // w3
  unsigned int v27; // w0
  __int64 v28; // x5
  __int64 v29; // x6
  __int64 v30; // x7
  __int64 v31; // x11
  float v32; // s6
  __int64 v33; // x10
  float v34; // s7
  __int64 v35; // x9
  float v36; // s2
  float v37; // s4
  float v38; // s1
  float v39; // s3
  float v40; // s5
  float v41; // s4
  __int64 v42; // [xsp+18h] [xbp+18h]
  __int64 v43; // [xsp+20h] [xbp+20h]
  int8x16x2_t v44; // 0:q2.16,16:q3.16
  int8x16x2_t v45; // 0:q6.16,16:q7.16
  v4 = 0;
  v5 = *(_DWORD *)this;
  v6 = *(_DWORD *)this >> 1;
  v7 = vdupq_lane_s32(*(int32x2_t *)&f, 0);
  v8 = 0;
  v9 = *((_QWORD *)this + 5);
  v10 = 0;
  do
  {
    v11 = v10;
    v12 = v8;
    v13 = v5 - (unsigned __int64)v10;
    v8 = (unsigned int)(v4 + 4);
    v14 = v13 * 4 - 32;
    v13 -= 4LL;
    v44.val[1] = *(int8x16_t *)&a[v11];
    v15 = *(int32x4_t *)(v9 + v11 * 4);
    v16 = v6 - v8;
    v10 += 8;
    v4 += 4;
    v17 = *(int32x4_t *)(v9 + v11 * 4 + 16);
    v45.val[1] = *(int8x16_t *)((char *)a + v14);
    v18 = vmulq_f32(vuzp1q_s32(v44.val[1], *(int32x4_t *)&a[v11 + 4]), v7);
    v44.val[0] = *(int8x16_t *)&a[v11 + 4];
    v45.val[0] = *(int8x16_t *)&a[v13];
    v19 = vuzp1q_s32(v15, v17);
    v45.val[0] = vqtbl2q_s8(v45, (int8x16_t)xmmword_343050);
    v45.val[1] = vuzp2q_s32(v15, v17);
    v45.val[0] = vmulq_f32(v7, v45.val[0]);
    v44.val[0] = vmulq_f32(v7, vqtbl2q_s8(v44, (int8x16_t)xmmword_343050));
    v20 = vmulq_f32(vuzp1q_s32(*(int32x4_t *)((char *)a + v14), *(int32x4_t *)&a[v13]), v7);
    *(float32x4_t *)&a2->realp[v12] = vmlaq_f32(vmulq_f32(v18, v19), v45.val[0], v45.val[1]);
    *(float32x4_t *)&a2->imagp[v12] = vmlsq_f32(vmulq_f32(v45.val[0], v19), v18, v45.val[1]);
    v21 = *(int32x4_t *)(v9 + v13 * 4);
    v44.val[1] = *(int8x16_t *)(v9 + v14);
    v22 = vuzp2q_s32(v44.val[1], v21);
    v44.val[1] = vuzp1q_s32(v44.val[1], v21);
    *(float32x4_t *)&a2->realp[v16] = vmlaq_f32(vmulq_f32(v44.val[0], v22), v20, v44.val[1]);
    *(float32x4_t *)&a2->imagp[v16] = vmlsq_f32(vmulq_f32(v44.val[0], v44.val[1]), v20, v22);
  }
  while ( v10 <= (int)v6 - 8 );
  if ( v10 < (unsigned int)v6 )
  {
    v24 = v10 >> 1;
    if ( v5 >> 2 > v10 >> 1 )
    {
      v25 = v6 - 1 - (v5 >> 2);
      v26 = v5 - 2 - 2 * v24;
      v27 = v6 - 1 - v24;
      v28 = v10 >> 1;
      v29 = 2LL * v24;
      v30 = 2 * v24 + 1;
      do
      {
        v31 = v26 + 1;
        v32 = *(float *)(v9 + v29 * 4);
        v33 = v26;
        v34 = *(float *)(v9 + v30 * 4);
        v35 = v27;
        v36 = f * a[v29];
        --v27;
        v37 = f * a[v31];
        v29 += 2;
        v38 = f * a[v26];
        v26 -= 2;
        v39 = f * a[v30];
        v30 += 2;
        a2->realp[v28] = (float)(v36 * v32) + (float)(v37 * v34);
        a2->imagp[v28++] = -(float)((float)(v36 * v34) - (float)(v37 * v32));
        v40 = *(float *)(v9 + 4 * v31);
        v41 = *(float *)(v9 + 4 * v33);
        a2->realp[v35] = (float)(v39 * v40) + (float)(v38 * v41);
        a2->imagp[v35] = -(float)((float)(v38 * v40) - (float)(v39 * v41));
      }
      while ( v25 != v27 );
    }
  }
  return v42 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x000b6e80, for verification */
000B6E80  e91340d1  SUB             X9, SP, #4,LSL#12
000B6E84  3ff107f9  STR             XZR, [X9,#0xFE0]
000B6E88  fd7bbea9  STP             X29, X30, [SP,#var_20]!
000B6E8C  a81b00d0  ADRP            X8, #__stack_chk_guard_ptr@PAGE
000B6E90  0a008052  MOV             W10, #0
000B6E94  fd030091  MOV             X29, SP
000B6E98  050040b9  LDR             W5, [X0]
000B6E9C  631400b0  ADRP            X3, #xmmword_343050@PAGE
000B6EA0  7214c03d  LDR             Q18, [X3,#xmmword_343050@PAGEOFF]
000B6EA4  ef03052a  MOV             W15, W5
000B6EA8  060147f9  LDR             X6, [X8,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B6EAC  b07c0153  LSR             W16, W5, #1
000B6EB0  1104044e  DUP             V17.4S, V0.S[0]
000B6EB4  e30310aa  MOV             X3, X16
000B6EB8  11220051  SUB             W17, W16, #8
000B6EBC  090080d2  MOV             X9, #0
000B6EC0  041440f9  LDR             X4, [X0,#0x28]
000B6EC4  c00040f9  LDR             X0, [X6]
000B6EC8  e00f00f9  STR             X0, [SP,#0x20+var_8]
000B6ECC  000080d2  MOV             X0, #0
000B6ED0  06008052  MOV             W6, #0
000B6ED4  1f2003d5  NOP
000B6ED8  cb7c7ed3  UBFIZ           X11, X6, #2, #0x20 ; ' '
000B6EDC  e04126cb  SUB             X0, X15, W6,UXTW
000B6EE0  6e410091  ADD             X14, X11, #0x10
000B6EE4  2df57ed3  LSL             X13, X9, #2
000B6EE8  00f47ed3  LSL             X0, X0, #2
000B6EEC  49110011  ADD             W9, W10, #4
000B6EF0  2468eb3c  LDR             Q4, [X1,X11]
000B6EF4  0c8000d1  SUB             X12, X0, #0x20 ; ' '
000B6EF8  2568ee3c  LDR             Q5, [X1,X14]
000B6EFC  004000d1  SUB             X0, X0, #0x10
000B6F00  831ca44e  MOV             V3.16B, V4.16B
000B6F04  070209cb  SUB             X7, X16, X9
000B6F08  9068eb3c  LDR             Q16, [X4,X11]
000B6F0C  e7f47ed3  LSL             X7, X7, #2
000B6F10  8418854e  UZP1            V4.4S, V4.4S, V5.4S
000B6F14  c6200011  ADD             W6, W6, #8
000B6F18  3368e03c  LDR             Q19, [X1,X0]
000B6F1C  ea0309aa  MOV             X10, X9
000B6F20  2168ec3c  LDR             Q1, [X1,X12]
000B6F24  df00116b  CMP             W6, W17
000B6F28  9468ee3c  LDR             Q20, [X4,X14]
000B6F2C  271ca14e  MOV             V7.16B, V1.16B
000B6F30  84dc316e  FMUL            V4.4S, V4.4S, V17.4S
000B6F34  a21ca54e  MOV             V2.16B, V5.16B
000B6F38  661eb34e  MOV             V6.16B, V19.16B
000B6F3C  051a944e  UZP1            V5.4S, V16.4S, V20.4S
000B6F40  4b0040f9  LDR             X11, [X2]
000B6F44  c620124e  TBL             V6.16B, {V6.16B-V7.16B}, V18.16B
000B6F48  075a944e  UZP2            V7.4S, V16.4S, V20.4S
000B6F4C  90dc256e  FMUL            V16.4S, V4.4S, V5.4S
000B6F50  4220124e  TBL             V2.16B, {V2.16B-V3.16B}, V18.16B
000B6F54  26de266e  FMUL            V6.4S, V17.4S, V6.4S
000B6F58  2118934e  UZP1            V1.4S, V1.4S, V19.4S
000B6F5C  031eb04e  MOV             V3.16B, V16.16B
000B6F60  22de226e  FMUL            V2.4S, V17.4S, V2.4S
000B6F64  c5dc256e  FMUL            V5.4S, V6.4S, V5.4S
000B6F68  21dc316e  FMUL            V1.4S, V1.4S, V17.4S
000B6F6C  c3cc274e  FMLA            V3.4S, V6.4S, V7.4S
000B6F70  85cca74e  FMLS            V5.4S, V4.4S, V7.4S
000B6F74  6369ad3c  STR             Q3, [X11,X13]
000B6F78  4b0440f9  LDR             X11, [X2,#8]
000B6F7C  6569ad3c  STR             Q5, [X11,X13]
000B6F80  8468e03c  LDR             Q4, [X4,X0]
000B6F84  8368ec3c  LDR             Q3, [X4,X12]
000B6F88  400040f9  LDR             X0, [X2]
000B6F8C  6558844e  UZP2            V5.4S, V3.4S, V4.4S
000B6F90  6318844e  UZP1            V3.4S, V3.4S, V4.4S
000B6F94  44dc256e  FMUL            V4.4S, V2.4S, V5.4S
000B6F98  42dc236e  FMUL            V2.4S, V2.4S, V3.4S
000B6F9C  24cc234e  FMLA            V4.4S, V1.4S, V3.4S
000B6FA0  22cca54e  FMLS            V2.4S, V1.4S, V5.4S
000B6FA4  0468a73c  STR             Q4, [X0,X7]
000B6FA8  400440f9  LDR             X0, [X2,#8]
000B6FAC  0268a73c  STR             Q2, [X0,X7]
000B6FB0  49f9ff54  B.LS            loc_B6ED8
000B6FB4  df00036b  CMP             W6, W3
000B6FB8  03010054  B.CC            loc_B6FD8
000B6FBC  080147f9  LDR             X8, [X8,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B6FC0  e10f40f9  LDR             X1, [SP,#0x20+var_8]
000B6FC4  000140f9  LDR             X0, [X8]
000B6FC8  200000ca  EOR             X0, X1, X0
000B6FCC  000700b5  CBNZ            X0, loc_B70AC
000B6FD0  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
000B6FD4  c0035fd6  RET
000B6FD8  ac7c0253  LSR             W12, W5, #2
000B6FDC  c77c0153  LSR             W7, W6, #1
000B6FE0  9f05466b  CMP             W12, W6,LSR#1
000B6FE4  c9feff54  B.LS            loc_B6FBC
000B6FE8  e600070b  ADD             W6, W7, W7
000B6FEC  60040051  SUB             W0, W3, #1
000B6FF0  c9040011  ADD             W9, W6, #1
000B6FF4  a3080051  SUB             W3, W5, #2
000B6FF8  0c000c4b  SUB             W12, W0, W12
000B6FFC  6300064b  SUB             W3, W3, W6
000B7000  0000074b  SUB             W0, W0, W7
000B7004  e5787ed3  UBFIZ           X5, X7, #2, #0x1F
000B7008  c67c7ed3  UBFIZ           X6, X6, #2, #0x20 ; ' '
000B700C  27f57ed3  LSL             X7, X9, #2
000B7010  6b040011  ADD             W11, W3, #1
000B7014  226866bc  LDR             S2, [X1,X6]
000B7018  866866bc  LDR             S6, [X4,X6]
000B701C  ea03032a  MOV             W10, W3
000B7020  876867bc  LDR             S7, [X4,X7]
000B7024  e903002a  MOV             W9, W0
000B7028  24786bbc  LDR             S4, [X1,X11,LSL#2]
000B702C  0208221e  FMUL            S2, S0, S2
000B7030  4d0040f9  LDR             X13, [X2]
000B7034  00040051  SUB             W0, W0, #1
000B7038  0408241e  FMUL            S4, S0, S4
000B703C  21786abc  LDR             S1, [X1,X10,LSL#2]
000B7040  4508261e  FMUL            S5, S2, S6
000B7044  4208271e  FMUL            S2, S2, S7
000B7048  236867bc  LDR             S3, [X1,X7]
000B704C  c6200091  ADD             X6, X6, #8
000B7050  0108211e  FMUL            S1, S0, S1
000B7054  63080051  SUB             W3, W3, #2
000B7058  8514071f  FMADD           S5, S4, S7, S5
000B705C  8288261f  FNMSUB          S2, S4, S6, S2
000B7060  0308231e  FMUL            S3, S0, S3
000B7064  e7200091  ADD             X7, X7, #8
000B7068  9f01006b  CMP             W12, W0
000B706C  a56925bc  STR             S5, [X13,X5]
000B7070  4d0440f9  LDR             X13, [X2,#8]
000B7074  a26925bc  STR             S2, [X13,X5]
000B7078  a5100091  ADD             X5, X5, #4
000B707C  85786bbc  LDR             S5, [X4,X11,LSL#2]
000B7080  84786abc  LDR             S4, [X4,X10,LSL#2]
000B7084  4a0040f9  LDR             X10, [X2]
000B7088  6208251e  FMUL            S2, S3, S5
000B708C  2208041f  FMADD           S2, S1, S4, S2
000B7090  2108251e  FMUL            S1, S1, S5
000B7094  6184241f  FNMSUB          S1, S3, S4, S1
000B7098  427929bc  STR             S2, [X10,X9,LSL#2]
000B709C  4a0440f9  LDR             X10, [X2,#8]
000B70A0  417929bc  STR             S1, [X10,X9,LSL#2]
000B70A4  61fbff54  B.NE            loc_B7010
000B70A8  c5ffff17  B               loc_B6FBC
000B70AC  99e2fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b70b0  size 656 bytes  164 insns
 *   DCT_IV::PostModulationExtend(DSPSplitComplex const*, float*, float*) const
 *   [_ZNK6DCT_IV20PostModulationExtendEPK15DSPSplitComplexPfS3_]
 *   data refs: 0x343060, 0x42ce00, 0x43e520
 */
__int64 __fastcall DCT_IV::PostModulationExtend(DCT_IV *this, const DSPSplitComplex *a, float *buf, float *buf2)
{
  unsigned int v4; // w12
  __int64 v5; // x11
  unsigned int v6; // w8
  int v7; // w13
  __int64 v8; // x14
  int v9; // w6
  __int64 v10; // x5
  unsigned int v11; // w4
  int32x4_t *v12; // x21
  __int64 v13; // x16
  __int64 v14; // x7
  float *imagp; // x17
  int32x4_t v16; // q1
  int32x4_t v17; // q5
  __int64 v18; // x9
  int32x4_t v19; // q2
  __int64 v20; // x7
  float32x4_t v21; // q0
  float32x4_t v22; // q7
  __int64 v23; // x0
  float32x4_t v24; // q16
  __int64 v25; // x15
  float32x4_t v26; // q6
  float32x4_t v27; // q2
  float32x4_t v28; // q5
  float32x4_t v29; // q18
  __int64 v30; // x9
  float32x4_t v31; // q17
  int32x4_t v32; // q4
  int32x4_t v33; // q0
  int32x4_t v34; // q1
  int32x4_t v35; // q2
  int8x16_t v36; // q6
  int8x16_t v37; // q5
  int8x16_t v38; // q1
  int8x16_t v39; // q0
  unsigned int v41; // w7
  __int64 v42; // x18
  __int64 v43; // x14
  float *v44; // x19
  float *v45; // x30
  unsigned int v46; // w4
  int v47; // w21
  int v48; // w8
  unsigned int v49; // w9
  float *v50; // x18
  float *v51; // x14
  __int64 v52; // x20
  __int64 v53; // x15
  __int64 v54; // x7
  __int64 v55; // x0
  float *v56; // x12
  float v57; // s16
  __int64 v58; // x16
  float v59; // s0
  float v60; // s4
  float v61; // s1
  float v62; // s5
  __int64 v63; // x11
  float v64; // s7
  float v65; // s3
  float v66; // s2
  float v67; // s6
  float v68; // s0
  __int64 v69; // x12
  float v70; // s1
  float v71; // s4
  __int64 v72; // [xsp+38h] [xbp+38h]
  __int64 v73; // [xsp+40h] [xbp+40h]
  v4 = 0;
  v5 = 0;
  v6 = *(_DWORD *)this;
  v7 = *(_DWORD *)this + 1;
  v8 = *(_DWORD *)this >> 1;
  v9 = *(_DWORD *)this >> 1;
  v10 = *((_QWORD *)this + 5);
  v11 = 0;
  do
  {
    v12 = (int32x4_t *)(v10 + 4LL * v11);
    v4 += 4;
    v13 = v5;
    v14 = v10 + 4 * (v6 - (unsigned __int64)v11);
    v5 = v4;
    imagp = a->imagp;
    v17 = *(int32x4_t *)(v14 - 32);
    v16 = *(int32x4_t *)(v14 - 16);
    v18 = v8 - v4;
    v19 = v12[1];
    v20 = v11 + v8;
    v21 = vuzp1q_s32(v17, v16);
    v22 = *(float32x4_t *)&imagp[v18];
    v23 = v8 - v11;
    v24 = vuzp1q_s32(*v12, v19);
    v25 = v20 * 4 + 16;
    v26 = vuzp2q_s32(*v12, v19);
    v11 += 8;
    v27 = *(float32x4_t *)&a->realp[v13];
    v28 = vuzp2q_s32(v17, v16);
    v29 = *(float32x4_t *)&a->realp[v18];
    v30 = v23 * 4 - 32;
    v31 = *(float32x4_t *)&imagp[v13];
    v23 -= 4;
    v32 = vqtbl1q_s8(vmlsq_f32(vmulq_f32(v22, v21), v29, v28), (int8x16_t)xmmword_343060);
    v33 = vmlaq_f32(vmulq_f32(v29, v21), v22, v28);
    v34 = vnegq_f32(vmlaq_f32(vmulq_f32(v27, v24), v31, v26));
    v35 = vqtbl1q_s8(vmlsq_f32(vmulq_f32(v27, v26), v31, v24), (int8x16_t)xmmword_343060);
    v36 = vzip2q_s32(v34, v32);
    v37 = vzip2q_s32(v33, v35);
    v38 = vzip1q_s32(v34, v32);
    v39 = vzip1q_s32(v33, v35);
    *(int8x16_t *)&buf2[v20] = v38;
    *(int8x16_t *)((char *)buf2 + v25) = v36;
    *(int8x16_t *)((char *)buf2 + v30) = vqtbl1q_s8(v36, (int8x16_t)xmmword_343060);
    *(int8x16_t *)&buf2[v23] = vqtbl1q_s8(v38, (int8x16_t)xmmword_343060);
    *(int8x16_t *)((char *)buf + v30) = v39;
    *(int8x16_t *)&buf[v23] = v37;
    *(float32x4_t *)&buf[v20] = vnegq_f32(vqtbl1q_s8(v37, (int8x16_t)xmmword_343060));
    *(float32x4_t *)((char *)buf + v25) = vnegq_f32(vqtbl1q_s8(v39, (int8x16_t)xmmword_343060));
  }
  while ( v11 <= (int)v8 - 8 );
  if ( v11 < (unsigned int)v8 )
  {
    v41 = v11 >> 1;
    if ( v6 >> 2 > v11 >> 1 )
    {
      v42 = (unsigned int)v8 + 2 * v41;
      v43 = (_DWORD)v8 + 1 + 2 * v41;
      v44 = &buf2[v42];
      v45 = &buf2[v43];
      v46 = v9 - 2 - 2 * v41;
      v47 = v9 - 1 - (v6 >> 2);
      v48 = v6 - v9;
      v49 = v9 - 1 - v41;
      v50 = &buf[v42];
      v51 = &buf[v43];
      v52 = v10 + 4LL * (2 * v41 + 1);
      v53 = v10 + 8LL * v41;
      v54 = v41 & 0x7FFFFFFF;
      v55 = 0;
      do
      {
        v56 = a->imagp;
        v57 = *(float *)(v53 + v55 * 4);
        v58 = v49;
        v59 = *(float *)(v52 + v55 * 4);
        --v49;
        v60 = a->realp[v54];
        v61 = v56[v54++];
        v62 = *(float *)(v10 + 4LL * (v46 - v9 + v7));
        v63 = v46 + 1;
        v64 = *(float *)(v10 + 4LL * (v48 + v46));
        v65 = (float)(v60 * v57) + (float)(v61 * v59);
        v66 = a->realp[v58];
        v67 = v56[v58];
        v68 = -(float)((float)(v61 * v57) - (float)(v60 * v59));
        v69 = v46;
        v70 = (float)(v66 * v64) + (float)(v67 * v62);
        v71 = -(float)((float)(v66 * v62) - (float)(v67 * v64));
        v46 -= 2;
        v44[v55] = -v65;
        v45[v55] = v71;
        buf2[v69] = v71;
        buf2[v63] = -v65;
        v50[v55] = -v68;
        v51[v55] = -v70;
        v55 += 2;
        buf[v69] = v70;
        buf[v63] = v68;
      }
      while ( v47 != v49 );
    }
  }
  return v72 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b7340  size 100 bytes  25 insns
 *   DCT_IV::DCT_IV()
 *   DCT_IV::DCT_IV()
 *   [_ZN6DCT_IVC1Ev]
 *   [_ZN6DCT_IVC2Ev]
 *   data refs: 0x42b860, 0x42cdb0, 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN6DCT_IVC2Ev'
void __fastcall DCT_IV::DCT_IV(DCT_IV *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  *(_DWORD *)this = 0;
  *((_DWORD *)this + 4) = 0;
  *((_QWORD *)this + 3) = 0;
  *((_QWORD *)this + 4) = 0;
  *((_QWORD *)this + 1) = off_42B870;
  *((_QWORD *)this + 5) = 0;
}


/* ------------------------------------------------------------------------ */
/* 0x000b73b0  size 88 bytes  22 insns
 *   DCT_IV::~DCT_IV()
 *   DCT_IV::~DCT_IV()
 *   [_ZN6DCT_IVD2Ev]
 *   [_ZN6DCT_IVD1Ev]
 *   data refs: 0x42b860, 0x42cdb0, 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN6DCT_IVD1Ev'
void __fastcall DCT_IV::~DCT_IV(DCT_IV *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  *((_QWORD *)this + 1) = off_42B870;
  DFTHandle<true>::~DFTHandle((char *)this + 8);
}


/* ------------------------------------------------------------------------ */
/* 0x000b7410  size 160 bytes  39 insns
 *   DCT_IV::Initialize(unsigned int)
 *   [_ZN6DCT_IV10InitializeEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall DCT_IV::Initialize(DCT_IV *this, unsigned int n)
{
  __int64 v2; // x4
  __int64 v3; // x5
  __int64 v4; // x6
  __int64 v5; // x7
  __int64 v8; // x1
  __int64 result; // x0
  void *DCTIVModulationCoefficients; // x0
  __int64 v11; // [xsp+0h] [xbp+0h]
  __int64 v12; // [xsp+40h] [xbp+40h]
  v8 = *(unsigned int *)this;
  result = 0;
  if ( (_DWORD)v8 != n )
  {
    DCTIVModulationCoefficients = GetDCTIVModulationCoefficients(
                                    n,
                                    v8,
                                    (__int64)&_stack_chk_guard,
                                    0,
                                    v2,
                                    v3,
                                    v4,
                                    v5,
                                    v11);
    *((_QWORD *)this + 5) = DCTIVModulationCoefficients;
    if ( !DCTIVModulationCoefficients )
      return 1936292453;
    result = DFTHandle<true>::Initialize((char *)this + 8, n >> 1);
    if ( (_DWORD)result )
      return 1936292453;
    else
      *(_DWORD *)this = n;
  }
  return result;
}


