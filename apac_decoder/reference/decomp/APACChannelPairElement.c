/* ============================================================================
 * APACChannelPairElement
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 8 function(s), 3048 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000ac600    712  APACChannelPairElement::DeserializeLrvqFrame(TBitstreamReader<unsigned int>&) */
/* 0x000ac8d0    844  APACChannelPairElement::DeserializeSQFrame(TBitstreamReader<unsigned int>&) */
/* 0x000ad460     96  APACChannelPairElement::ChannelData(bool) */
/* 0x000ad4c0     88  APACChannelPairElement::GetICSInfo(bool) */
/* 0x000ad680    272  APACChannelPairElement::Reset() */
/* 0x000ad9b0    376  APACChannelPairElement::~APACChannelPairElement() */
/* 0x000adcf0    432  APACChannelPairElement::~APACChannelPairElement() */
/* 0x000adea0    228  APACChannelPairElement::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x000ac600  size 712 bytes  178 insns
 *   APACChannelPairElement::DeserializeLrvqFrame(TBitstreamReader<unsigned int>&)
 *   [_ZN22APACChannelPairElement20DeserializeLrvqFrameER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelPairElement::DeserializeLrvqFrame(APACChannelPairElement *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x23
  __int64 v3; // x24
  __int64 v4; // x25
  __int64 result; // x0
  APACIndividualChannelStream *v8; // x5
  unsigned int cache; // w1
  int v10; // w0
  unsigned int v11; // w23
  unsigned int v12; // w1
  int v13; // w0
  unsigned int v14; // w22
  const APACICSInfo *v15; // x1
  __int64 v16; // x4
  const apac__SFBOffsets *v17; // x2
  const apac__SFBOffsets *v18; // x4
  const APACICSInfo *v19; // x1
  __int64 v20; // x0
  const apac__SFBOffsets *v21; // x2
  const apac__SFBOffsets *v22; // x0
  unsigned int v23; // w1
  int v24; // w0
  unsigned int v25; // w0
  int bitsLeft; // w1
  unsigned int v27; // w0
  unsigned int v28; // w0
  int v29; // w1
  unsigned int v30; // w0
  int v31; // w1
  __int64 v32; // [xsp+30h] [xbp+30h]
  __int64 v33; // [xsp+38h] [xbp+38h]
  __int64 v34; // [xsp+40h] [xbp+40h]
  __int64 v35; // [xsp+58h] [xbp+58h]
  __int64 v36; // [xsp+60h] [xbp+60h]
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, __int64))(**((_QWORD **)this + 2) + 16LL))(
             *((_QWORD *)this + 2),
             bs,
             1);
  if ( (_DWORD)result )
    goto LABEL_3;
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, __int64))(**((_QWORD **)this + 64) + 16LL))(
             *((_QWORD *)this + 64),
             bs,
             1);
  if ( (_DWORD)result )
    goto LABEL_3;
  result = APACLRVQDecoder::SpectrumDeserializeCPE(
             (APACLRVQDecoder *)bs,
             *((TBitstreamReader_u32 **)this + 2),
             *((APACICSInfo **)this + 64),
             *((APACICSInfo **)this + 3),
             *((APACIndividualChannelStream **)this + 65),
             v8);
  if ( (_DWORD)result )
    goto LABEL_3;
  v32 = v2;
  cache = bs->cache;
  v10 = bs->bitsLeft - 1;
  bs->bitsLeft = v10;
  v11 = cache >> 31;
  if ( v10 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v25 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v11) = v11 | (v25 >> (bitsLeft + 32));
    v27 = v25 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v27 = 0;
    bs->cur += 4;
    bs->cache = v27;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  *((_BYTE *)this + 128) = (unsigned __int8)v11 != 0;
  if ( (_BYTE)v11 )
  {
    result = APACTNSData::ParseTNSData(
               (APACChannelPairElement *)((char *)this + 128),
               bs,
               *((const APACICSInfo **)this + 2));
    if ( (_DWORD)result )
    {
      v2 = v32;
LABEL_3:
      if ( v35 != _stack_chk_guard )
      {
        v32 = v2;
        v33 = v3;
        v34 = v4;
      }
      return result;
    }
  }
  else
  {
    *((_BYTE *)this + 129) = 0;
  }
  v12 = bs->cache;
  v13 = bs->bitsLeft - 1;
  bs->bitsLeft = v13;
  v14 = v12 >> 31;
  if ( v13 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v28 = bs->cache;
    v29 = bs->bitsLeft;
    LOBYTE(v14) = v14 | (v28 >> (v29 + 32));
    v30 = v28 << -(char)v29;
    if ( v29 == -32 )
      v30 = 0;
    bs->cur += 4;
    bs->cache = v30;
    bs->bitsLeft = v29 + 32;
  }
  else
  {
    bs->cache = 2 * v12;
  }
  *((_BYTE *)this + 318) = (unsigned __int8)v14 != 0;
  if ( (_BYTE)v14 )
  {
    result = APACTNSData::ParseTNSData(
               (APACChannelPairElement *)((char *)this + 318),
               bs,
               *((const APACICSInfo **)this + 64));
    if ( (_DWORD)result )
    {
LABEL_20:
      v2 = v32;
      goto LABEL_3;
    }
  }
  else
  {
    *((_BYTE *)this + 319) = 0;
    if ( (_BYTE)v11 )
    {
      v23 = bs->cache;
      v24 = bs->bitsLeft - 1;
      bs->bitsLeft = v24;
      if ( v24 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v31 = bs->bitsLeft;
        bs->cur += 4;
        bs->bitsLeft = v31 + 32;
        if ( v31 == -32 )
          bs->cache = 0;
        else
          bs->cache <<= -(char)v31;
      }
      else
      {
        bs->cache = 2 * v23;
      }
    }
  }
  v15 = *((const APACICSInfo **)this + 2);
  v16 = *(_QWORD *)(*((_QWORD *)this + 4) + 16LL);
  v17 = (const apac__SFBOffsets *)(v16 + 24);
  v18 = (const apac__SFBOffsets *)(v16 + 8);
  if ( *((_BYTE *)v15 + 10) != 2 )
    v17 = v18;
  result = APACTNSData::Apply(
             (APACChannelPairElement *)((char *)this + 128),
             v15,
             v17,
             (APACSpectralData *)(*((_QWORD *)this + 3) + 752LL));
  if ( (_DWORD)result )
    goto LABEL_20;
  v19 = *((const APACICSInfo **)this + 64);
  v20 = *(_QWORD *)(*((_QWORD *)this + 4) + 16LL);
  v21 = (const apac__SFBOffsets *)(v20 + 24);
  v22 = (const apac__SFBOffsets *)(v20 + 8);
  if ( *((_BYTE *)v19 + 10) != 2 )
    v21 = v22;
  return APACTNSData::Apply(
           (APACChannelPairElement *)((char *)this + 318),
           v19,
           v21,
           (APACSpectralData *)(*((_QWORD *)this + 65) + 752LL));
}


/* ------------------------------------------------------------------------ */
/* 0x000ac8d0  size 844 bytes  211 insns
 *   APACChannelPairElement::DeserializeSQFrame(TBitstreamReader<unsigned int>&)
 *   [_ZN22APACChannelPairElement18DeserializeSQFrameER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelPairElement::DeserializeSQFrame(APACChannelPairElement *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x23
  __int64 v3; // x24
  __int64 result; // x0
  unsigned int cache; // w1
  int v8; // w0
  unsigned int v9; // w22
  __int64 v10; // x0
  unsigned int v11; // w1
  int v12; // w0
  unsigned int v13; // w22
  unsigned int v14; // w1
  int v15; // w0
  unsigned int v16; // w22
  const APACICSInfo *v17; // x1
  __int64 v18; // x4
  const apac__SFBOffsets *v19; // x2
  const apac__SFBOffsets *v20; // x4
  const APACICSInfo *v21; // x1
  __int64 v22; // x0
  const apac__SFBOffsets *v23; // x2
  const apac__SFBOffsets *v24; // x0
  __int64 v25; // x1
  unsigned int v26; // w0
  int bitsLeft; // w1
  unsigned int v28; // w0
  unsigned int v29; // w0
  int v30; // w1
  unsigned int v31; // w0
  unsigned int v32; // w0
  int v33; // w1
  unsigned int v34; // w0
  __int64 v35; // [xsp+30h] [xbp+30h]
  __int64 v36; // [xsp+38h] [xbp+38h]
  __int64 v37; // [xsp+48h] [xbp+48h]
  __int64 v38; // [xsp+50h] [xbp+50h]
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD))(**((_QWORD **)this + 2) + 16LL))(
             *((_QWORD *)this + 2),
             bs,
             0);
  if ( (_DWORD)result )
    goto LABEL_3;
  result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD, __int64))(**((_QWORD **)this + 3) + 16LL))(
             *((_QWORD *)this + 3),
             bs,
             *((_QWORD *)this + 2),
             1);
  if ( (_DWORD)result )
    goto LABEL_3;
  cache = bs->cache;
  v8 = bs->bitsLeft - 1;
  bs->bitsLeft = v8;
  v9 = cache >> 31;
  if ( v8 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v26 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v9) = v9 | (v26 >> (bitsLeft + 32));
    v28 = v26 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v28 = 0;
    bs->cur += 4;
    bs->cache = v28;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  v10 = *((_QWORD *)this + 64);
  if ( (_BYTE)v9 )
  {
    v25 = *((_QWORD *)this + 2);
    if ( v25 != v10 )
    {
      *(_BYTE *)(v10 + 10) = *(_BYTE *)(v25 + 10);
      *(_BYTE *)(v10 + 11) = *(_BYTE *)(v25 + 11);
      *(_BYTE *)(v10 + 12) = *(_BYTE *)(v25 + 12);
      *(_DWORD *)(v10 + 16) = *(_DWORD *)(v25 + 16);
      v10 = *((_QWORD *)this + 2);
    }
    result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, __int64, __int64))(**((_QWORD **)this + 65)
                                                                                         + 16LL))(
               *((_QWORD *)this + 65),
               bs,
               v10,
               1);
    if ( (_DWORD)result )
      goto LABEL_3;
    result = APACCACDecoder::Deserialize(
               (APACChannelPairElement *)((char *)this + 40),
               bs,
               *((const APACICSInfo **)this + 2));
    if ( (_DWORD)result )
      goto LABEL_3;
    result = APACCACDecoder::ProcessCac(
               (APACChannelPairElement *)((char *)this + 40),
               *((const APACICSInfo **)this + 2),
               *((APACIndividualChannelStream **)this + 3),
               *((APACIndividualChannelStream **)this + 65));
    if ( (_DWORD)result )
      goto LABEL_3;
  }
  else
  {
    result = (*(__int64 (__fastcall **)(__int64, TBitstreamReader_u32 *, _QWORD))(*(_QWORD *)v10 + 16LL))(v10, bs, 0);
    if ( (_DWORD)result )
      goto LABEL_3;
    result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *, _QWORD, _QWORD))(**((_QWORD **)this + 65) + 16LL))(
               *((_QWORD *)this + 65),
               bs,
               *((_QWORD *)this + 64),
               0);
    if ( (_DWORD)result )
      goto LABEL_3;
  }
  v11 = bs->cache;
  v12 = bs->bitsLeft - 1;
  bs->bitsLeft = v12;
  v13 = v11 >> 31;
  if ( v12 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v29 = bs->cache;
    v30 = bs->bitsLeft;
    LOBYTE(v13) = v13 | (v29 >> (v30 + 32));
    v31 = v29 << -(char)v30;
    if ( v30 == -32 )
      v31 = 0;
    bs->cur += 4;
    bs->cache = v31;
    bs->bitsLeft = v30 + 32;
  }
  else
  {
    bs->cache = 2 * v11;
  }
  *((_BYTE *)this + 128) = (unsigned __int8)v13 != 0;
  if ( (_BYTE)v13 )
  {
    result = APACTNSData::ParseTNSData(
               (APACChannelPairElement *)((char *)this + 128),
               bs,
               *((const APACICSInfo **)this + 2));
    if ( (_DWORD)result )
      goto LABEL_3;
  }
  else
  {
    *((_BYTE *)this + 129) = 0;
  }
  v14 = bs->cache;
  v15 = bs->bitsLeft - 1;
  bs->bitsLeft = v15;
  v16 = v14 >> 31;
  if ( v15 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v32 = bs->cache;
    v33 = bs->bitsLeft;
    LOBYTE(v16) = v16 | (v32 >> (v33 + 32));
    v34 = v32 << -(char)v33;
    if ( v33 == -32 )
      v34 = 0;
    bs->cur += 4;
    bs->cache = v34;
    bs->bitsLeft = v33 + 32;
  }
  else
  {
    bs->cache = 2 * v14;
  }
  *((_BYTE *)this + 318) = (unsigned __int8)v16 != 0;
  if ( (_BYTE)v16 )
  {
    result = APACTNSData::ParseTNSData(
               (APACChannelPairElement *)((char *)this + 318),
               bs,
               *((const APACICSInfo **)this + 64));
    if ( !(_DWORD)result )
      goto LABEL_18;
LABEL_3:
    if ( v37 != _stack_chk_guard )
    {
      v35 = v2;
      v36 = v3;
    }
    return result;
  }
  *((_BYTE *)this + 319) = 0;
LABEL_18:
  v17 = *((const APACICSInfo **)this + 2);
  v18 = *(_QWORD *)(*((_QWORD *)this + 4) + 16LL);
  v19 = (const apac__SFBOffsets *)(v18 + 24);
  v20 = (const apac__SFBOffsets *)(v18 + 8);
  if ( *((_BYTE *)v17 + 10) != 2 )
    v19 = v20;
  result = APACTNSData::Apply(
             (APACChannelPairElement *)((char *)this + 128),
             v17,
             v19,
             (APACSpectralData *)(*((_QWORD *)this + 3) + 752LL));
  if ( (_DWORD)result )
    goto LABEL_3;
  v21 = *((const APACICSInfo **)this + 64);
  v22 = *(_QWORD *)(*((_QWORD *)this + 4) + 16LL);
  v23 = (const apac__SFBOffsets *)(v22 + 24);
  v24 = (const apac__SFBOffsets *)(v22 + 8);
  if ( *((_BYTE *)v21 + 10) != 2 )
    v23 = v24;
  return APACTNSData::Apply(
           (APACChannelPairElement *)((char *)this + 318),
           v21,
           v23,
           (APACSpectralData *)(*((_QWORD *)this + 65) + 752LL));
}


/* ------------------------------------------------------------------------ */
/* 0x000ad460  size 96 bytes  24 insns
 *   APACChannelPairElement::ChannelData(bool)
 *   [_ZN22APACChannelPairElement11ChannelDataEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelPairElement::ChannelData(APACChannelPairElement *this, bool flag)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  if ( flag )
    return *((_QWORD *)this + 3) + 752LL;
  else
    return *((_QWORD *)this + 65) + 752LL;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad4c0  size 88 bytes  22 insns
 *   APACChannelPairElement::GetICSInfo(bool)
 *   [_ZN22APACChannelPairElement10GetICSInfoEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelPairElement::GetICSInfo(APACChannelPairElement *this, bool flag)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  if ( flag )
    return *((_QWORD *)this + 2);
  else
    return *((_QWORD *)this + 64);
}


/* ------------------------------------------------------------------------ */
/* 0x000ad680  size 272 bytes  68 insns
 *   APACChannelPairElement::Reset()
 *   [_ZN22APACChannelPairElement5ResetEv]
 *   data refs: 0x9a720, 0x42ce00, 0x42cec8, 0x43e520
 */
__int64 __fastcall APACChannelPairElement::Reset(APACChannelPairElement *this)
{
  __int64 v2; // x0
  void (*v3)(void); // x2
  __int64 v4; // x0
  void (*v5)(void); // x2
  __int64 v6; // x0
  _DWORD *v7; // x1
  _DWORD *i; // x2
  __int64 v9; // x1
  _DWORD *v10; // x0
  _DWORD *j; // x1
  __int64 v13; // [xsp+28h] [xbp+28h]
  __int64 v14; // [xsp+30h] [xbp+30h]
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
  v4 = *((_QWORD *)this + 64);
  if ( v4 )
  {
    v5 = *(void (**)(void))(*(_QWORD *)v4 + 24LL);
    if ( (char *)v5 == (char *)APACICSInfo::Reset )
    {
      *(_WORD *)(v4 + 10) = 0;
      *(_BYTE *)(v4 + 12) = 0;
      *(_DWORD *)(v4 + 16) = 1;
    }
    else
    {
      v5();
    }
  }
  v6 = *((_QWORD *)this + 3);
  if ( v6 )
  {
    v7 = *(_DWORD **)(v6 + 752);
    for ( i = *(_DWORD **)(v6 + 760); i != v7; ++v7 )
      *v7 = 0;
  }
  v9 = *((_QWORD *)this + 65);
  if ( v9 )
  {
    v10 = *(_DWORD **)(v9 + 752);
    for ( j = *(_DWORD **)(v9 + 760); j != v10; ++v10 )
      *v10 = 0;
  }
  return v13 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad9b0  size 376 bytes  94 insns
 *   APACChannelPairElement::~APACChannelPairElement()
 *   [_ZN22APACChannelPairElementD0Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42b528, 0x42ce00, 0x42cf58, 0x42da00, 0x42dc38, 0x42dc80, 0x43e520
 */
void __fastcall APACChannelPairElement::~APACChannelPairElement(APACChannelPairElement *this)
{
  _QWORD *v2; // x21
  void (__fastcall *v3)(APACIndividualChannelStream *); // x1
  void *v4; // x0
  void *v5; // x0
  void (*v6)(void); // x2
  void *v7; // x0
  void *v8; // x0
  void *v9; // x0
  _QWORD *v10; // x21
  void (__fastcall *v11)(APACIndividualChannelStream *); // x1
  void *v12; // x0
  void *v13; // x0
  void (*v14)(void); // x2
  __int64 v15; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD **)this + 65);
  *(_QWORD *)this = off_42B538;
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
  v5 = *((void **)this + 64);
  if ( v5 )
  {
    v6 = *(void (**)(void))(*(_QWORD *)v5 + 8LL);
    if ( (char *)v6 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v5);
    else
      v6();
  }
  v7 = *((void **)this + 12);
  if ( v7 )
    operator delete(v7);
  v8 = *((void **)this + 9);
  if ( v8 )
    operator delete(v8);
  v9 = *((void **)this + 5);
  if ( v9 )
    operator delete(v9);
  v10 = *((_QWORD **)this + 3);
  *(_QWORD *)this = &unk_42B478;
  if ( v10 )
  {
    v11 = *(void (__fastcall **)(APACIndividualChannelStream *))(*v10 + 8LL);
    if ( v11 == APACIndividualChannelStream::~APACIndividualChannelStream )
    {
      v12 = (void *)v10[94];
      if ( v12 )
        free(v12);
      operator delete(v10);
    }
    else
    {
      v11((APACIndividualChannelStream *)v10);
    }
  }
  v13 = *((void **)this + 2);
  if ( v13 )
  {
    v14 = *(void (**)(void))(*(_QWORD *)v13 + 8LL);
    if ( (char *)v14 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v13);
    else
      v14();
  }
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x000adcf0  size 432 bytes  108 insns
 *   APACChannelPairElement::~APACChannelPairElement()
 *   APACChannelPairElement::~APACChannelPairElement()
 *   [_ZN22APACChannelPairElementD2Ev]
 *   [_ZN22APACChannelPairElementD1Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42b528, 0x42ce00, 0x42cf58, 0x42da00, 0x42dc38, 0x42dc80, 0x43e520
 */
// Alternative name is '_ZN22APACChannelPairElementD1Ev'
void __fastcall APACChannelPairElement::~APACChannelPairElement(APACChannelPairElement *this)
{
  _QWORD *v2; // x21
  void (__fastcall *v3)(APACIndividualChannelStream *); // x1
  void *v4; // x0
  void *v5; // x0
  void (*v6)(void); // x2
  void *v7; // x0
  void *v8; // x0
  void *v9; // x0
  _QWORD *v10; // x21
  void (__fastcall *v11)(APACIndividualChannelStream *); // x1
  void *v12; // x0
  void *v13; // x0
  void (*v14)(void); // x2
  __int64 v15; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD **)this + 65);
  *(_QWORD *)this = off_42B538;
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
  v5 = *((void **)this + 64);
  if ( v5 )
  {
    v6 = *(void (**)(void))(*(_QWORD *)v5 + 8LL);
    if ( (char *)v6 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v5);
    else
      v6();
  }
  v7 = *((void **)this + 12);
  if ( v7 )
    operator delete(v7);
  v8 = *((void **)this + 9);
  if ( v8 )
    operator delete(v8);
  v9 = *((void **)this + 5);
  if ( v9 )
    operator delete(v9);
  v10 = *((_QWORD **)this + 3);
  *(_QWORD *)this = &unk_42B478;
  if ( v10 )
  {
    v11 = *(void (__fastcall **)(APACIndividualChannelStream *))(*v10 + 8LL);
    if ( v11 == APACIndividualChannelStream::~APACIndividualChannelStream )
    {
      v12 = (void *)v10[94];
      if ( v12 )
        free(v12);
      operator delete(v10);
    }
    else
    {
      v11((APACIndividualChannelStream *)v10);
    }
  }
  v13 = *((void **)this + 2);
  if ( v13 )
  {
    v14 = *(void (**)(void))(*(_QWORD *)v13 + 8LL);
    if ( (char *)v14 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v13);
    else
      v14();
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000adea0  size 228 bytes  57 insns
 *   APACChannelPairElement::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN22APACChannelPairElement11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACChannelPairElement::Deserialize(APACChannelPairElement *this, TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w2
  int v5; // w1
  unsigned int v6; // w22
  unsigned int v8; // w0
  int bitsLeft; // w1
  unsigned int v10; // w0
  __int64 v11; // [xsp+40h] [xbp+40h]
  cache = bs->cache;
  v5 = bs->bitsLeft - 1;
  bs->bitsLeft = v5;
  v6 = cache >> 31;
  if ( v5 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v8 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v6) = v6 | (v8 >> (bitsLeft + 32));
    v10 = v8 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v10 = 0;
    bs->cur += 4;
    bs->cache = v10;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 2 * cache;
  }
  if ( (_BYTE)v6 )
    return APACChannelPairElement::DeserializeLrvqFrame(this, bs);
  else
    return APACChannelPairElement::DeserializeSQFrame(this, bs);
}


