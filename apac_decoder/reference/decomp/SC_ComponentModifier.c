/* ============================================================================
 * SC_ComponentModifier
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 296 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 *
 * NOTE: not required for the CarPlay 5.1 / 5.1.2 target (Ambisonics scene
 *       or low-bitrate path). Included for completeness only.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009da70    296  SC_ComponentModifier::operator==(SC_ComponentModifier const&) const */

/* ------------------------------------------------------------------------ */
/* 0x0009da70  size 296 bytes  74 insns
 *   SC_ComponentModifier::operator==(SC_ComponentModifier const&) const
 *   [_ZNK20SC_ComponentModifiereqERKS_]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall SC_ComponentModifier::operator==(SC_ComponentModifier *this, const SC_ComponentModifier *a)
{
  int v3; // w5
  bool result; // w0
  __int64 v5; // [xsp+20h] [xbp+20h]
  v3 = *(unsigned __int8 *)this;
  result = 0;
  if ( v3 == *(unsigned __int8 *)a
    && *((unsigned __int8 *)a + 1) == *((unsigned __int8 *)this + 1)
    && (!*((_BYTE *)a + 1) || *((float *)this + 1) == *((float *)a + 1)) )
  {
    result = 0;
    if ( *((unsigned __int8 *)a + 8) == *((unsigned __int8 *)this + 8)
      && (!*((_BYTE *)a + 8) || *((float *)this + 3) == *((float *)a + 3)) )
    {
      result = 0;
      if ( *((unsigned __int8 *)a + 16) == *((unsigned __int8 *)this + 16)
        && (!*((_BYTE *)a + 16) || *((float *)this + 5) == *((float *)a + 5)) )
      {
        result = 0;
        if ( *((unsigned __int8 *)a + 24) == *((unsigned __int8 *)this + 24)
          && (!*((_BYTE *)a + 24) || *((float *)this + 7) == *((float *)a + 7)) )
        {
          result = 0;
          if ( *((unsigned __int8 *)a + 32) == *((unsigned __int8 *)this + 32) )
          {
            result = 1;
            if ( *((_BYTE *)a + 32) )
              return *((unsigned __int8 *)this + 33) == *((unsigned __int8 *)a + 33);
          }
        }
      }
    }
  }
  return result;
}


