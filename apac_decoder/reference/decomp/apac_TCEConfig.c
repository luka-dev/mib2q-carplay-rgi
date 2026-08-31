/* ============================================================================
 * apac::TCEConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 196 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0007c800     68  apac::TCEConfig::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x0007c850     64  apac::TCEConfig::~TCEConfig() */
/* 0x0007c890     64  apac::TCEConfig::~TCEConfig() */

/* ------------------------------------------------------------------------ */
/* 0x0007c800  size 68 bytes  17 insns
 *   apac::TCEConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac9TCEConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::TCEConfig::Deserialize(apac__TCEConfig *this, TBitstreamReader_u32 *bs)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007c850  size 64 bytes  16 insns
 *   apac::TCEConfig::~TCEConfig()
 *   apac::TCEConfig::~TCEConfig()
 *   [_ZN4apac9TCEConfigD2Ev]
 *   [_ZN4apac9TCEConfigD1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4apac9TCEConfigD1Ev'
void __fastcall apac::TCEConfig::~TCEConfig(apac__TCEConfig *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
}


/* ------------------------------------------------------------------------ */
/* 0x0007c890  size 64 bytes  16 insns
 *   apac::TCEConfig::~TCEConfig()
 *   [_ZN4apac9TCEConfigD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall apac::TCEConfig::~TCEConfig(apac__TCEConfig *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


