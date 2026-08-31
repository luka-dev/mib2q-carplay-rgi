/* ============================================================================
 * MultiRadixFFT
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 156 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00103330    156  MultiRadixFFT::CplxOutOfPlaceTransform(DSPSplitComplex const*, DSPSplitComplex*, int) */

/* ------------------------------------------------------------------------ */
/* 0x00103330  size 156 bytes  37 insns
 *   MultiRadixFFT::CplxOutOfPlaceTransform(DSPSplitComplex const*, DSPSplitComplex*, int)
 *   [_ZN13MultiRadixFFT23CplxOutOfPlaceTransformEPK15DSPSplitComplexPS0_i]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall MultiRadixFFT::CplxOutOfPlaceTransform(
        MultiRadixFFT *this,
        const DSPSplitComplex *a,
        DSPSplitComplex *a2,
        int n)
{
  __int64 v5; // x0
  __int64 v7; // [xsp+30h] [xbp+30h]
  v5 = *((_QWORD *)this + 2);
  if ( n != 1 )
    v5 = *((_QWORD *)this + 3);
  if ( !v5 )
    return 561214580;
  if ( *((_DWORD *)this + 2) == 180 )
    return 1936292453;
  vDSP_DFT_Execute(v5, a->realp, a->imagp);
  return 0;
}


