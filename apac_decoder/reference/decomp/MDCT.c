/* ============================================================================
 * MDCT
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 560 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b74b0     64  MDCT::MDCT() */
/* 0x000b74f0     64  MDCT::~MDCT() */
/* 0x000b7530    132  MDCT::Initialize(unsigned int, float) */
/* 0x000b75c0    300  MDCT::InverseInPlaceTransform(float*, float*) */

/* ------------------------------------------------------------------------ */
/* 0x000b74b0  size 64 bytes  16 insns
 *   MDCT::MDCT()
 *   MDCT::MDCT()
 *   [_ZN4MDCTC2Ev]
 *   [_ZN4MDCTC1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4MDCTC1Ev'
void __fastcall MDCT::MDCT(MDCT *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  DCT_IV::DCT_IV(this);
}


/* ------------------------------------------------------------------------ */
/* 0x000b74f0  size 64 bytes  16 insns
 *   MDCT::~MDCT()
 *   MDCT::~MDCT()
 *   [_ZN4MDCTD1Ev]
 *   [_ZN4MDCTD2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4MDCTD2Ev'
void __fastcall MDCT::~MDCT(MDCT *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  DCT_IV::~DCT_IV(this);
}


/* ------------------------------------------------------------------------ */
/* 0x000b7530  size 132 bytes  32 insns
 *   MDCT::Initialize(unsigned int, float)
 *   [_ZN4MDCT10InitializeEjf]
 *   data refs: 0x42ce00, 0x43e520, 0x73697a65
 */
__int64 __fastcall MDCT::Initialize(MDCT *this, unsigned int n, float f)
{
  __int64 v4; // [xsp+20h] [xbp+20h]
  if ( f == 0.0 )
    f = 1.0 / (float)n;
  *((float *)this + 12) = f;
  if ( n > 0x1F )
    return DCT_IV::Initialize(this, n);
  else
    return 1936292453;
}


/* ------------------------------------------------------------------------ */
/* 0x000b75c0  size 300 bytes  74 insns
 *   MDCT::InverseInPlaceTransform(float*, float*)
 *   [_ZN4MDCT23InverseInPlaceTransformEPfS0_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall MDCT::InverseInPlaceTransform(MDCT *this, float *buf, float *buf2)
{
  _QWORD *v4; // x0
  int v7; // w3
  __int64 v8; // x5
  __int64 v9; // x6
  _QWORD *v10; // x1
  float v11; // s0
  _QWORD v13[2]; // [xsp+0h] [xbp+0h] BYREF
  DSPSplitComplex v14; // [xsp+48h] [xbp+48h] BYREF
  DSPSplitComplex v15; // [xsp+58h] [xbp+58h] BYREF
  __int64 v16; // [xsp+70h] [xbp+70h]
  if ( !*((_QWORD *)this + 5) )
    return 561214580;
  v4 = &v13[-1536];
  v7 = *(_DWORD *)this >> 1;
  v8 = (unsigned int)(v7 + 3);
  v9 = (8 * v8) & 0x3FFFFF000LL;
  v10 = (_QWORD *)((char *)&v13[-1536] - v9);
  if ( &v13[-1536] != v10 )
  {
    do
    {
      v4 -= 512;
      *v4 = 0;
    }
    while ( v4 != v10 );
  }
  *(_QWORD *)((char *)v4 - (((8 * v8) & 0x3FFFFFFE0LL) - v9)) = 0;
  v11 = *((float *)this + 12);
  v14.realp = buf2;
  v14.imagp = &buf2[v7 & 0x7FFFFFFF];
  v15.realp = (float *)v13;
  v15.imagp = (float *)v13 + (v8 & 0x7FFFFFFC);
  DCT_IV::PreModulation(this, buf, &v14, v11);
  MultiRadixFFT::CplxOutOfPlaceTransform((MDCT *)((char *)this + 8), &v14, &v15, 1);
  DCT_IV::PostModulationExtend(this, &v15, buf, buf2);
  return 0;
}


