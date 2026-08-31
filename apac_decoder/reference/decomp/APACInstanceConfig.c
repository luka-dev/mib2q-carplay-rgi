/* ============================================================================
 * APACInstanceConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 224 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009a870    224  APACInstanceConfig::Create(unsigned int, unsigned int) */

/* ------------------------------------------------------------------------ */
/* 0x0009a870  size 224 bytes  56 insns
 *   APACInstanceConfig::Create(unsigned int, unsigned int)
 *   [_ZN18APACInstanceConfig6CreateEjj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 *__fastcall APACInstanceConfig::Create(APACInstanceConfig *this, unsigned int n, unsigned int n2)
{
  __int64 *v3; // x8
  __int64 *v5; // x19
  int v6; // w23
  __int64 SFBOffsetTable; // x20
  __int64 v8; // x2
  void *v9; // x0
  __int64 v11; // [xsp+50h] [xbp+50h]
  v5 = v3;
  v6 = (int)this;
  *v3 = 0;
  SFBOffsetTable = apac::GetSFBOffsetTable((unsigned int)this, n);
  if ( SFBOffsetTable )
  {
    v8 = operator new(0x18u);
    v9 = (void *)*v5;
    *v5 = v8;
    *(_QWORD *)(v8 + 16) = SFBOffsetTable;
    if ( !v9 || (operator delete(v9), (v8 = *v5) != 0) )
    {
      *(_DWORD *)v8 = n;
      *(_DWORD *)(*v5 + 4) = v6;
      *(_WORD *)(*v5 + 8) = 0;
      *(_WORD *)(*v5 + 10) = *(_WORD *)(SFBOffsetTable + 8);
    }
  }
  return v5;
}


