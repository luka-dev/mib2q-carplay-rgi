/* ============================================================================
 * APACSingleChannelElement
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 1192 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000ac3f0    528  APACSingleChannelElement::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x000ad520    176  APACSingleChannelElement::Reset() */
/* 0x000ad790    272  APACSingleChannelElement::~APACSingleChannelElement() */
/* 0x000adb30    216  APACSingleChannelElement::~APACSingleChannelElement() */

/* ------------------------------------------------------------------------ */
/* 0x000ac3f0  size 528 bytes  132 insns
 *   APACSingleChannelElement::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN24APACSingleChannelElement11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSingleChannelElement::Deserialize(APACSingleChannelElement *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x23
  unsigned int cache; // w1
  int v6; // w0
  unsigned int v7; // w22
  __int64 result; // x0
  APACIndividualChannelStream *v9; // x3
  unsigned int v10; // w1
  int v11; // w0
  unsigned int v12; // w22
  const APACICSInfo *v13; // x1
  unsigned int v14; // w3
  int bitsLeft; // w4
  const unsigned __int8 *cur; // x1
  __int64 v17; // x0
  const apac__SFBOffsets *v18; // x2
  const apac__SFBOffsets *v19; // x0
  unsigned int v20; // w0
  int v21; // w1
  unsigned int v22; // w0
  __int64 v23; // [xsp+30h] [xbp+30h]
  __int64 v24; // [xsp+48h] [xbp+48h]
  __int64 v25; // [xsp+50h] [xbp+50h]
  cache = bs->cache;
  v6 = bs->bitsLeft - 1;
  bs->bitsLeft = v6;
  v7 = cache >> 31;
  if ( v6 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v14 = bs->cache;
    bitsLeft = bs->bitsLeft;
    cur = bs->cur;
    bs->bitsLeft = bitsLeft + 32;
    bs->cur = cur + 4;
    v7 = (unsigned __int8)((v14 >> (bitsLeft + 32)) | v7);
    if ( bitsLeft == -32 )
      bs->cache = 0;
    else
      bs->cache = v14 << -(char)bitsLeft;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD))(**((_QWORD **)this + 2) + 16LL))(
             *((_QWORD *)this + 2),
             bs,
             v7);
  if ( !(_DWORD)result )
  {
    if ( v7 == 1 )
    {
      result = APACLRVQDecoder::SpectrumDeserializeSCE(
                 (APACLRVQDecoder *)bs,
                 *((TBitstreamReader_u32 **)this + 2),
                 *((APACICSInfo **)this + 3),
                 v9);
      if ( (_DWORD)result )
        goto LABEL_11;
    }
    else
    {
      result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD, _QWORD))(**((_QWORD **)this + 3) + 16LL))(
                 *((_QWORD *)this + 3),
                 bs,
                 *((_QWORD *)this + 2),
                 0);
      if ( (_DWORD)result )
        goto LABEL_11;
    }
    v10 = bs->cache;
    v11 = bs->bitsLeft - 1;
    bs->bitsLeft = v11;
    v12 = v10 >> 31;
    if ( v11 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v20 = bs->cache;
      v21 = bs->bitsLeft;
      LOBYTE(v12) = v12 | (v20 >> (v21 + 32));
      v22 = v20 << -(char)v21;
      if ( v21 == -32 )
        v22 = 0;
      bs->cur += 4;
      bs->cache = v22;
      bs->bitsLeft = v21 + 32;
    }
    else
    {
      bs->cache = 2 * v10;
    }
    *((_BYTE *)this + 40) = (unsigned __int8)v12 != 0;
    v13 = *((const APACICSInfo **)this + 2);
    if ( !(_BYTE)v12 )
    {
      *((_BYTE *)this + 41) = 0;
      goto LABEL_16;
    }
    result = APACTNSData::ParseTNSData(
               (APACSingleChannelElement *)((char *)this + 40),
               bs,
               *((const APACICSInfo **)this + 2));
    if ( !(_DWORD)result )
    {
      v13 = *((const APACICSInfo **)this + 2);
LABEL_16:
      v17 = *(_QWORD *)(*((_QWORD *)this + 4) + 16LL);
      v18 = (const apac__SFBOffsets *)(v17 + 24);
      v19 = (const apac__SFBOffsets *)(v17 + 8);
      if ( *((_BYTE *)v13 + 10) != 2 )
        v18 = v19;
      return APACTNSData::Apply(
               (APACSingleChannelElement *)((char *)this + 40),
               v13,
               v18,
               (APACSpectralData *)(*((_QWORD *)this + 3) + 752LL));
    }
  }
LABEL_11:
  if ( v24 != _stack_chk_guard )
    v23 = v2;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad520  size 176 bytes  44 insns
 *   APACSingleChannelElement::Reset()
 *   [_ZN24APACSingleChannelElement5ResetEv]
 *   data refs: 0x9a720, 0x42ce00, 0x42cec8, 0x43e520
 */
__int64 __fastcall APACSingleChannelElement::Reset(APACSingleChannelElement *this)
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
/* 0x000ad790  size 272 bytes  68 insns
 *   APACSingleChannelElement::~APACSingleChannelElement()
 *   APACSingleChannelElement::~APACSingleChannelElement()
 *   [_ZN24APACSingleChannelElementD2Ev]
 *   [_ZN24APACSingleChannelElementD1Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42ce00, 0x42cf58, 0x42dc38, 0x42dc80, 0x43e520
 */
// Alternative name is '_ZN24APACSingleChannelElementD1Ev'
void __fastcall APACSingleChannelElement::~APACSingleChannelElement(APACSingleChannelElement *this)
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
/* 0x000adb30  size 216 bytes  54 insns
 *   APACSingleChannelElement::~APACSingleChannelElement()
 *   [_ZN24APACSingleChannelElementD0Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42ce00, 0x42cf58, 0x42dc38, 0x42dc80, 0x43e520
 */
void __fastcall APACSingleChannelElement::~APACSingleChannelElement(APACSingleChannelElement *this)
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


