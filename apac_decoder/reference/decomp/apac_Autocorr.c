/* ============================================================================
 * apac::Autocorr
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 228 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00079790    116  apac::Autocorr::~Autocorr() */
/* 0x00079810    112  apac::Autocorr::~Autocorr() */

/* ------------------------------------------------------------------------ */
/* 0x00079790  size 116 bytes  29 insns
 *   apac::Autocorr::~Autocorr()
 *   apac::Autocorr::~Autocorr()
 *   [_ZN4apac8AutocorrD1Ev]
 *   [_ZN4apac8AutocorrD2Ev]
 *   data refs: 0x42a848, 0x42ce00, 0x42d7d8, 0x43e520
 */
// Alternative name is '_ZN4apac8AutocorrD2Ev'
void __fastcall apac::Autocorr::~Autocorr(apac__Autocorr *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+20h] [xbp+20h]
  v2 = *((void **)this + 2);
  *(_QWORD *)this = off_42A858;
  if ( v2 )
    operator delete(v2);
}


/* ------------------------------------------------------------------------ */
/* 0x00079810  size 112 bytes  28 insns
 *   apac::Autocorr::~Autocorr()
 *   [_ZN4apac8AutocorrD0Ev]
 *   data refs: 0x42a848, 0x42ce00, 0x42d7d8, 0x43e520
 */
void __fastcall apac::Autocorr::~Autocorr(apac__Autocorr *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((void **)this + 2);
  *(_QWORD *)this = off_42A858;
  if ( v2 )
    operator delete(v2);
  operator delete(this);
}


