/* ============================================================================
 * APACChannelRemapper
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 488 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0007bcd0    176  APACChannelRemapper::Process(std::vector<float*, std::allocator<float*> >&) */
/* 0x0007bd80     84  APACChannelRemapper::Initialize(std::vector<unsigned char, std::allocator<unsigned char> > const&) */
/* 0x0007bde0    116  APACChannelRemapper::~APACChannelRemapper() */
/* 0x0007be60    112  APACChannelRemapper::~APACChannelRemapper() */

/* ------------------------------------------------------------------------ */
/* 0x0007bcd0  size 176 bytes  44 insns
 *   APACChannelRemapper::Process(std::vector<float*, std::allocator<float*> >&)
 *   [_ZN19APACChannelRemapper7ProcessERSt6vectorIPfSaIS1_EE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelRemapper::Process(APACChannelRemapper *this, std_vector *vec)
{
  __int64 v2; // x5
  __int64 v3; // x4
  __int64 v4; // x6
  unsigned int v5; // w3
  __int64 i; // x2
  _QWORD *begin; // x4
  __int64 v8; // x6
  __int64 v10; // [xsp+20h] [xbp+20h]
  v2 = 0;
  v3 = *((_QWORD *)this + 1);
  v4 = *((_QWORD *)this + 2);
  v5 = 0;
  if ( v4 != v3 )
  {
    do
    {
      for ( i = *(unsigned __int8 *)(v3 + v2); (unsigned int)i > v5; i = *(unsigned __int8 *)(v3 + (unsigned __int8)i) )
        ;
      if ( v5 != (_DWORD)i )
      {
        begin = vec->begin;
        v8 = *((_QWORD *)vec->begin + v2);
        *((_QWORD *)vec->begin + v2) = *((_QWORD *)vec->begin + i);
        begin[i] = v8;
        v3 = *((_QWORD *)this + 1);
        v4 = *((_QWORD *)this + 2);
      }
      v2 = v5 + 1;
      v5 = v2;
    }
    while ( v4 - v3 > (unsigned __int64)(unsigned int)v2 );
  }
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007bd80  size 84 bytes  21 insns
 *   APACChannelRemapper::Initialize(std::vector<unsigned char, std::allocator<unsigned char> > const&)
 *   [_ZN19APACChannelRemapper10InitializeERKSt6vectorIhSaIhEE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelRemapper::Initialize(APACChannelRemapper *this, const std_vector *vec)
{
  __int64 v3; // [xsp+30h] [xbp+30h]
  std::vector<unsigned char>::operator=((char *)this + 8, vec, 0);
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007bde0  size 116 bytes  29 insns
 *   APACChannelRemapper::~APACChannelRemapper()
 *   APACChannelRemapper::~APACChannelRemapper()
 *   [_ZN19APACChannelRemapperD2Ev]
 *   [_ZN19APACChannelRemapperD1Ev]
 *   data refs: 0x42a8b8, 0x42ce00, 0x42d368, 0x43e520
 */
// Alternative name is '_ZN19APACChannelRemapperD1Ev'
void __fastcall APACChannelRemapper::~APACChannelRemapper(APACChannelRemapper *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+20h] [xbp+20h]
  v2 = *((void **)this + 1);
  *(_QWORD *)this = off_42A8C8;
  if ( v2 )
    operator delete(v2);
}


/* ------------------------------------------------------------------------ */
/* 0x0007be60  size 112 bytes  28 insns
 *   APACChannelRemapper::~APACChannelRemapper()
 *   [_ZN19APACChannelRemapperD0Ev]
 *   data refs: 0x42a8b8, 0x42ce00, 0x42d368, 0x43e520
 */
void __fastcall APACChannelRemapper::~APACChannelRemapper(APACChannelRemapper *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((void **)this + 1);
  *(_QWORD *)this = off_42A8C8;
  if ( v2 )
    operator delete(v2);
  operator delete(this);
}


