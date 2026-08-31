/* ============================================================================
 * BWELPC
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 688 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b0d70    160  BWELPC::LPCToMDCTSpectrum(float const*, float*, unsigned int) */
/* 0x000b0e10    328  BWELPC::MDCTCoefficientsToLPC(float const*, unsigned int, unsigned int, unsigned int) */
/* 0x000b0f60    200  BWELPC::DecodeLSFsToLPCs(unsigned short const*) */

/* ------------------------------------------------------------------------ */
/* 0x000b0d70  size 160 bytes  40 insns
 *   BWELPC::LPCToMDCTSpectrum(float const*, float*, unsigned int)
 *   [_ZN6BWELPC17LPCToMDCTSpectrumEPKfPfj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall BWELPC::LPCToMDCTSpectrum(BWELPC *this, const float *a, float *buf, unsigned int n)
{
  __int64 v4; // x4
  __int64 v6; // [xsp+20h] [xbp+20h]
  if ( n == 64 )
  {
    v4 = 24;
  }
  else if ( n <= 0x40 )
  {
    v4 = 40;
    if ( n != 32 )
    {
      v4 = 32;
      if ( n != 48 )
        v4 = 0;
    }
  }
  else
  {
    v4 = 16;
    if ( n != 256 )
    {
      v4 = 8;
      if ( n != 384 )
        v4 = 0;
    }
  }
  return (*(__int64 (__fastcall **)(_QWORD, const float *, float *))(**(_QWORD **)(*((_QWORD *)this + 30) + v4) + 40LL))(
           *(_QWORD *)(*((_QWORD *)this + 30) + v4),
           a,
           buf);
}


/* ------------------------------------------------------------------------ */
/* 0x000b0e10  size 328 bytes  81 insns
 *   BWELPC::MDCTCoefficientsToLPC(float const*, unsigned int, unsigned int, unsigned int)
 *   [_ZN6BWELPC21MDCTCoefficientsToLPCEPKfjjj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall BWELPC::MDCTCoefficientsToLPC(
        BWELPC *this,
        const float *a,
        unsigned int n,
        unsigned int n2,
        unsigned int n3)
{
  __int64 v6; // x3
  _QWORD *v8; // x0
  int v9; // w6
  __int64 v10; // x2
  __int64 v11; // x6
  _QWORD *v12; // x8
  __int64 v13; // x6
  float v14; // s0
  __int64 v15; // x2
  _QWORD v17[8]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v18; // [xsp+40h] [xbp+40h]
  v6 = n;
  v8 = &v17[-1536];
  v9 = 4 * *((unsigned __int16 *)this + 52) + 19;
  v10 = *(_QWORD *)&v9 & 0x7F000LL;
  v11 = *(_QWORD *)&v9 & 0x7FFF0LL;
  v12 = (_QWORD *)((char *)&v17[-1536] - v10);
  if ( &v17[-1536] != v12 )
  {
    do
    {
      v8 -= 512;
      *v8 = 0;
    }
    while ( v8 != v12 );
  }
  *(_QWORD *)((char *)v8 - (v11 - v10)) = 0;
  if ( n2 == 64 )
  {
    v13 = 24;
  }
  else if ( n2 > 0x40 )
  {
    v13 = 16;
    if ( n2 != 256 )
    {
      v13 = 8;
      if ( n2 != 384 )
        v13 = 0;
    }
  }
  else
  {
    v13 = 40;
    if ( n2 != 32 )
    {
      v13 = 32;
      if ( n2 != 48 )
        v13 = 0;
    }
  }
  (*(void (__fastcall **)(_QWORD, const float *, _QWORD *, __int64, unsigned int))(**(_QWORD **)(*((_QWORD *)this + 30)
                                                                                               + v13)
                                                                                 + 32LL))(
    *(_QWORD *)(*((_QWORD *)this + 30) + v13),
    a,
    v17,
    v6,
    n3);
  if ( *(float *)v17 == 0.0 )
    v14 = 1.0001;
  else
    v14 = *(float *)v17 * 1.0001;
  v15 = *(_QWORD *)this;
  *(float *)v17 = v14;
  (*(void (__fastcall **)(BWELPC *, _QWORD *))(v15 + 48))(this, v17);
  return v17[7] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b0f60  size 200 bytes  48 insns
 *   BWELPC::DecodeLSFsToLPCs(unsigned short const*)
 *   [_ZN6BWELPC16DecodeLSFsToLPCsEPKt]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall BWELPC::DecodeLSFsToLPCs(BWELPC *this, const unsigned __int16 *a)
{
  __int64 v4; // [xsp+30h] [xbp+30h]
  (*(void (__fastcall **)(BWELPC *, const unsigned __int16 *))(*(_QWORD *)this + 72LL))(this, a);
  (*(void (__fastcall **)(BWELPC *, _QWORD, float, float))(*(_QWORD *)this + 128LL))(
    this,
    *((_QWORD *)this + 24),
    50.0,
    24000.0);
  (*(void (__fastcall **)(BWELPC *, _QWORD, _QWORD, float))(*(_QWORD *)this + 96LL))(
    this,
    *((_QWORD *)this + 24),
    *((_QWORD *)this + 27),
    24000.0);
  return (*(__int64 (__fastcall **)(BWELPC *, _QWORD, _QWORD, _QWORD))(*(_QWORD *)this + 112LL))(
           this,
           *((_QWORD *)this + 27),
           *((_QWORD *)this + 6),
           (unsigned __int16)((*((_QWORD *)this + 28) - *((_QWORD *)this + 27)) >> 2));
}


