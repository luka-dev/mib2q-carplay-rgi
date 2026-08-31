/* ============================================================================
 * APACLFEElement
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 988 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000ac2a0    324  APACLFEElement::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x000ad5d0    176  APACLFEElement::Reset() */
/* 0x000ad8a0    272  APACLFEElement::~APACLFEElement() */
/* 0x000adc10    216  APACLFEElement::~APACLFEElement() */

/* ------------------------------------------------------------------------ */
/* 0x000ac2a0  size 324 bytes  81 insns
 *   APACLFEElement::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN14APACLFEElement11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACLFEElement::Deserialize(APACLFEElement *this, TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w1
  int v5; // w0
  unsigned int v6; // w22
  __int64 result; // x0
  unsigned int v8; // w3
  int bitsLeft; // w4
  const unsigned __int8 *cur; // x1
  APACIndividualChannelStream *v11; // [xsp+38h] [xbp+38h]
  __int64 v12; // [xsp+40h] [xbp+40h]
  cache = bs->cache;
  v5 = bs->bitsLeft - 1;
  bs->bitsLeft = v5;
  v6 = cache >> 31;
  if ( v5 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v8 = bs->cache;
    bitsLeft = bs->bitsLeft;
    cur = bs->cur;
    bs->bitsLeft = bitsLeft + 32;
    bs->cur = cur + 4;
    v6 = (unsigned __int8)((v8 >> (bitsLeft + 32)) | v6);
    if ( bitsLeft == -32 )
      bs->cache = 0;
    else
      bs->cache = v8 << -(char)bitsLeft;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD))(**((_QWORD **)this + 2) + 16LL))(
             *((_QWORD *)this + 2),
             bs,
             v6);
  if ( !(_DWORD)result )
  {
    if ( v6 == 1 )
      return APACLRVQDecoder::SpectrumDeserializeLFE(
               (APACLRVQDecoder *)bs,
               *((TBitstreamReader_u32 **)this + 2),
               *((APACICSInfo **)this + 3),
               v11);
    else
      return (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD, _QWORD))(**((_QWORD **)this + 3) + 16LL))(
               *((_QWORD *)this + 3),
               bs,
               *((_QWORD *)this + 2),
               0);
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad5d0  size 176 bytes  44 insns
 *   APACLFEElement::Reset()
 *   [_ZN14APACLFEElement5ResetEv]
 *   data refs: 0x9a720, 0x42ce00, 0x42cec8, 0x43e520
 */
__int64 __fastcall APACLFEElement::Reset(APACLFEElement *this)
{
  __int64 v2; // x0
  void (*v3)(void); // x2
  __int64 v4; // x1
  _DWORD *v5; // x0
  _DWORD *i; // x1
  __int64 v8; // [xsp+28h] [xbp+28h]
  __int64 v9; // [xsp+30h] [xbp+30h]
  v2 = *((_QWORD *)this + 2);
  if ( v2 )
  {
    v3 = *(void (**)(void))(*(_QWORD *)v2 + 24LL);
    if ( (char *)v3 == (char *)APACICSInfo::Reset )
    {
      *(_WORD *)(v2 + 10) = 0;
      *(_BYTE *)(v2 + 12) = 0;
      *(_DWORD *)(v2 + 16) = 1;
    }
    else
    {
      v3();
    }
  }
  v4 = *((_QWORD *)this + 3);
  if ( v4 )
  {
    v5 = *(_DWORD **)(v4 + 752);
    for ( i = *(_DWORD **)(v4 + 760); i != v5; ++v5 )
      *v5 = 0;
  }
  return v8 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad8a0  size 272 bytes  68 insns
 *   APACLFEElement::~APACLFEElement()
 *   APACLFEElement::~APACLFEElement()
 *   [_ZN14APACLFEElementD1Ev]
 *   [_ZN14APACLFEElementD2Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42ce00, 0x42cf58, 0x42dc38, 0x42dc80, 0x43e520
 */
// Alternative name is '_ZN14APACLFEElementD2Ev'
void __fastcall APACLFEElement::~APACLFEElement(APACLFEElement *this)
{
  _QWORD *v2; // x21
  void (__fastcall *v3)(APACIndividualChannelStream *); // x1
  void *v4; // x0
  void *v5; // x0
  void (*v6)(void); // x2
  __int64 v7; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD **)this + 3);
  *(_QWORD *)this = &unk_42B478;
  if ( v2 )
  {
    v3 = *(void (__fastcall **)(APACIndividualChannelStream *))(*v2 + 8LL);
    if ( v3 == APACIndividualChannelStream::~APACIndividualChannelStream )
    {
      v4 = (void *)v2[94];
      if ( v4 )
        free(v4);
      operator delete(v2);
    }
    else
    {
      v3((APACIndividualChannelStream *)v2);
    }
  }
  v5 = *((void **)this + 2);
  if ( v5 )
  {
    v6 = *(void (**)(void))(*(_QWORD *)v5 + 8LL);
    if ( (char *)v6 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v5);
    else
      v6();
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000adc10  size 216 bytes  54 insns
 *   APACLFEElement::~APACLFEElement()
 *   [_ZN14APACLFEElementD0Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42ce00, 0x42cf58, 0x42dc38, 0x42dc80, 0x43e520
 */
void __fastcall APACLFEElement::~APACLFEElement(APACLFEElement *this)
{
  _QWORD *v2; // x21
  void (__fastcall *v3)(APACIndividualChannelStream *); // x1
  void *v4; // x0
  void *v5; // x0
  void (*v6)(void); // x2
  __int64 v7; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD **)this + 3);
  *(_QWORD *)this = &unk_42B478;
  if ( v2 )
  {
    v3 = *(void (__fastcall **)(APACIndividualChannelStream *))(*v2 + 8LL);
    if ( v3 == APACIndividualChannelStream::~APACIndividualChannelStream )
    {
      v4 = (void *)v2[94];
      if ( v4 )
        free(v4);
      operator delete(v2);
    }
    else
    {
      v3((APACIndividualChannelStream *)v2);
    }
  }
  v5 = *((void **)this + 2);
  if ( v5 )
  {
    v6 = *(void (**)(void))(*(_QWORD *)v5 + 8LL);
    if ( (char *)v6 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v5);
    else
      v6();
  }
  operator delete(this);
}


