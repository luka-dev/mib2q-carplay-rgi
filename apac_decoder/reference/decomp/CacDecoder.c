/* ============================================================================
 * CacDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 144 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b2070     72  CacDecoder::RepeatCodeTableDec() const */
/* 0x000b20c0     72  CacDecoder::GainIndexCodeTableDec() const */

/* ------------------------------------------------------------------------ */
/* 0x000b2070  size 72 bytes  18 insns
 *   CacDecoder::RepeatCodeTableDec() const
 *   [_ZNK10CacDecoder18RepeatCodeTableDecEv]
 *   data refs: 0x331c00, 0x42ce00, 0x42db98, 0x43e520
 */
void *__fastcall CacDecoder::RepeatCodeTableDec(CacDecoder *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return &CacDecoder::kRepeatCodeTableDec;
}


/* ------------------------------------------------------------------------ */
/* 0x000b20c0  size 72 bytes  18 insns
 *   CacDecoder::GainIndexCodeTableDec() const
 *   [_ZNK10CacDecoder21GainIndexCodeTableDecEv]
 *   data refs: 0x331b28, 0x42ce00, 0x42d7e0, 0x43e520
 */
void *__fastcall CacDecoder::GainIndexCodeTableDec(CacDecoder *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return &CacDecoder::kGainIndexCodeTableDec;
}


