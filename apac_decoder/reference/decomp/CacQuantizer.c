/* ============================================================================
 * CacQuantizer
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 600 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b2110    188  CacQuantizer::GainTableEntry(unsigned int) const */
/* 0x000b21d0    412  CacQuantizer::Initialize() */

/* ------------------------------------------------------------------------ */
/* 0x000b2110  size 188 bytes  47 insns
 *   CacQuantizer::GainTableEntry(unsigned int) const
 *   [_ZNK12CacQuantizer14GainTableEntryEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall CacQuantizer::GainTableEntry(CacQuantizer *this, unsigned int n)
{
  float *v2; // x0
  __int64 v3; // x1
  __int64 v5; // x1
  float v6; // s2
  float v7; // s0
  __int64 v8; // [xsp+28h] [xbp+28h]
  float v9; // [xsp+30h] [xbp+30h]
  __int64 v10; // [xsp+38h] [xbp+38h]
  __int64 v11; // [xsp+40h] [xbp+40h]
  v2 = *(float **)this;
  if ( n > 0x11 )
  {
    if ( n > 0x22 )
    {
      v8 = *(_QWORD *)v2;
      v9 = v2[2];
    }
    else
    {
      v5 = 3LL * (n - 17);
      v6 = v2[v5 + 1];
      v7 = -v2[v5 + 2];
      *(float *)&v8 = -v2[v5];
      *((float *)&v8 + 1) = v6;
      v9 = v7;
    }
  }
  else
  {
    v3 = 3LL * n;
    v8 = *(_QWORD *)&v2[v3];
    v9 = v2[v3 + 2];
  }
  return v10 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b21d0  size 412 bytes  99 insns
 *   CacQuantizer::Initialize()
 *   [_ZN12CacQuantizer10InitializeEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall CacQuantizer::Initialize(CacQuantizer *this)
{
  _DWORD *v2; // x2
  _DWORD *v3; // x3
  unsigned __int64 v4; // x1
  int v5; // s1
  float v6; // s8
  __int64 v7; // x19
  int v8; // w21
  float v9; // s0
  float v10; // s0
  float v11; // s13
  __int64 v13; // [xsp+78h] [xbp+78h]
  __int64 v14; // [xsp+80h] [xbp+80h]
  v2 = *(_DWORD **)this;
  v3 = *((_DWORD **)this + 1);
  v4 = 0xAAAAAAAAAAAAAAABLL * (((__int64)v3 - *(_QWORD *)this) >> 2);
  if ( v4 <= 0x11 )
  {
    std::vector<CacQuantizer::CacGainTableEntry>::_M_default_append(this, 18 - v4);
    v2 = *(_DWORD **)this;
  }
  else if ( v4 != 18 && v3 != v2 + 54 )
  {
    *((_QWORD *)this + 1) = v2 + 54;
  }
  *v2 = 0;
  v5 = 1048615884;
  v6 = 0.25119;
  *(float *)(*(_QWORD *)this + 4LL) = 1.0;
  v7 = 12;
  v8 = 1;
  *(_DWORD *)(*(_QWORD *)this + 8LL) = 0;
  while ( 1 )
  {
    v10 = (float)(v6 * v6) + 1.0;
    *(_DWORD *)(*(_QWORD *)this + v7) = v5;
    v11 = sqrtf(v10);
    if ( v10 < 0.0 )
      sqrtf(v10);
    *(float *)(*(_QWORD *)this + v7 + 4) = 1.0 / v11;
    *(float *)(*(_QWORD *)this + v7 + 8) = *(float *)(*(_QWORD *)this + v7 + 4) * v6;
    if ( v7 == 204 )
      break;
    v9 = powf(10.0, (float)((float)((float)v8 * 1.5) + -12.0) * 0.05);
    v6 = v9;
    v5 = LODWORD(v9);
    if ( v9 >= 0.999 )
      v6 = 1.0 / v9;
    ++v8;
    v7 += 12;
  }
  return v13 ^ _stack_chk_guard;
}


