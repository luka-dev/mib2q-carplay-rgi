/* ============================================================================
 * MultiRadixRealFFT
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 612 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x001033d0    244  MultiRadixRealFFT::RealInPlaceTransform(DSPSplitComplex*, int) */
/* 0x001034d0    124  MultiRadixRealFFT::RealOutOfPlaceForwardTransform(float const*, DSPSplitComplex*) */
/* 0x00103550    244  MultiRadixRealFFT::RealOutOfPlaceInverseTransform(DSPSplitComplex const*, float*) */

/* ------------------------------------------------------------------------ */
/* 0x001033d0  size 244 bytes  59 insns
 *   MultiRadixRealFFT::RealInPlaceTransform(DSPSplitComplex*, int)
 *   [_ZN17MultiRadixRealFFT20RealInPlaceTransformEP15DSPSplitComplexi]
 *   data refs: 0x345db4, 0x42ce00, 0x43e520
 */
__int64 __fastcall MultiRadixRealFFT::RealInPlaceTransform(MultiRadixRealFFT *this, DSPSplitComplex *a, int n)
{
  __int64 v5; // x0
  __int64 v7; // x0
  __int64 v8; // [xsp+40h] [xbp+40h]
  if ( n != 1 )
  {
    v5 = *((_QWORD *)this + 3);
    if ( v5 )
    {
      vDSP_DFT_Execute(v5, a->realp, a->imagp);
      return 0;
    }
    return 561214580;
  }
  v7 = *((_QWORD *)this + 2);
  if ( !v7 )
    return 561214580;
  vDSP_DFT_Execute(v7, a->realp, a->imagp);
  vDSP_vsmul(a->realp, 1, &unk_345DB4, a->realp, 1, *((_DWORD *)this + 2) >> 1);
  vDSP_vsmul(a->imagp, 1, &unk_345DB4, a->imagp, 1, *((_DWORD *)this + 2) >> 1);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x001034d0  size 124 bytes  31 insns
 *   MultiRadixRealFFT::RealOutOfPlaceForwardTransform(float const*, DSPSplitComplex*)
 *   [_ZN17MultiRadixRealFFT30RealOutOfPlaceForwardTransformEPKfP15DSPSplitComplex]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall MultiRadixRealFFT::RealOutOfPlaceForwardTransform(
        MultiRadixRealFFT *this,
        const float *a,
        DSPSplitComplex *a2)
{
  __int64 v6; // [xsp+40h] [xbp+40h]
  vDSP_ctoz(a, 2, a2, 1, *((_DWORD *)this + 2) >> 1);
  return MultiRadixRealFFT::RealInPlaceTransform(this, a2, 1);
}


/* ------------------------------------------------------------------------ */
/* 0x00103550  size 244 bytes  60 insns
 *   MultiRadixRealFFT::RealOutOfPlaceInverseTransform(DSPSplitComplex const*, float*)
 *   [_ZN17MultiRadixRealFFT30RealOutOfPlaceInverseTransformEPK15DSPSplitComplexPf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall MultiRadixRealFFT::RealOutOfPlaceInverseTransform(
        MultiRadixRealFFT *this,
        const DSPSplitComplex *a,
        float *buf)
{
  _QWORD *v5; // x4
  unsigned int v7; // w0
  __int64 v8; // x3
  __int64 v9; // x1
  __int64 v10; // x3
  _QWORD *v11; // x2
  __int64 v12; // x4
  __int64 v13; // x0
  _QWORD v15[2]; // [xsp+0h] [xbp+0h] BYREF
  _QWORD v16[2]; // [xsp+38h] [xbp+38h] BYREF
  __int64 v17; // [xsp+50h] [xbp+50h]
  v5 = &v15[-1536];
  v7 = *((_DWORD *)this + 2);
  v8 = 4LL * v7 + 15;
  v9 = v8 & 0x7FFFFF000LL;
  v10 = v8 & 0x7FFFFFFF0LL;
  v11 = (_QWORD *)((char *)&v15[-1536] - v9);
  if ( &v15[-1536] != v11 )
  {
    do
    {
      v5 -= 512;
      *v5 = 0;
    }
    while ( v5 != v11 );
  }
  *(_QWORD *)((char *)v5 - (v10 - v9)) = 0;
  v12 = v7 >> 1;
  v13 = *((_QWORD *)this + 3);
  v16[0] = v15;
  v16[1] = (char *)v15 + 4 * v12;
  if ( !v13 )
    return 561214580;
  vDSP_DFT_Execute(v13, a->realp, a->imagp);
  vDSP_ztoc(v16, 1, buf, 2, *((_DWORD *)this + 2) >> 1);
  return 0;
}


