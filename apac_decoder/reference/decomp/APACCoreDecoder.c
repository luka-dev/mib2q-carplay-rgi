/* ============================================================================
 * APACCoreDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 11 function(s), 3596 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0007cf50     68  APACCoreDecoder::Reset() */
/* 0x0007cfa0    120  APACCoreDecoder::Deserialize(unsigned char const*, unsigned int) */
/* 0x0007d020    352  APACCoreDecoder::~APACCoreDecoder() */
/* 0x0007d180     84  APACCoreDecoder::~APACCoreDecoder() */
/* 0x0007d1e0   1476  APACCoreDecoder::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>) */
/* 0x0007d7b0    256  APACCoreDecoder::APACCoreDecoder() */
/* 0x0007d8b0    148  APACCoreDecoder::DeserializeExtTools(TBitstreamReader<unsigned int>&) */
/* 0x0007d950    468  APACCoreDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool) */
/* 0x0007db30    468  APACCoreDecoder::DecodeAPACFrame(TBitstreamReader<unsigned int>&, apac::FrameType, std::vector<float*, std::allocator<float*> >&) */
/* 0x0007dd10     88  APACCoreDecoder::GetBlockSize() const */
/* 0x0007dd70     68  APACCoreDecoder::GetNumChannels() const */

/* ------------------------------------------------------------------------ */
/* 0x0007cf50  size 68 bytes  17 insns
 *   APACCoreDecoder::Reset()
 *   [_ZN15APACCoreDecoder5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::Reset(APACCoreDecoder *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return APACSynthesisFilterBank::Reset((APACCoreDecoder *)((char *)this + 16));
}


/* ------------------------------------------------------------------------ */
/* 0x0007cfa0  size 120 bytes  30 insns
 *   APACCoreDecoder::Deserialize(unsigned char const*, unsigned int)
 *   [_ZN15APACCoreDecoder11DeserializeEPKhj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::Deserialize(APACCoreDecoder *this, const unsigned __int8 *a, unsigned int n)
{
  TBitstreamReader_u32 v5; // [xsp+30h] [xbp+30h] BYREF
  __int64 v6; // [xsp+50h] [xbp+50h]
  TBitstreamReader<unsigned int>::TBitstreamReader(&v5, a, n);
  return (*(__int64 (__fastcall **)(APACCoreDecoder *, TBitstreamReader_u32 *, __int64))(*(_QWORD *)this + 48LL))(
           this,
           &v5,
           1);
}


/* ------------------------------------------------------------------------ */
/* 0x0007d020  size 352 bytes  88 insns
 *   APACCoreDecoder::~APACCoreDecoder()
 *   APACCoreDecoder::~APACCoreDecoder()
 *   [_ZN15APACCoreDecoderD2Ev]
 *   [_ZN15APACCoreDecoderD1Ev]
 *   data refs: 0x7be60, 0x42a8b8, 0x42a9e0, 0x42b588, 0x42ca70, 0x42ce00, 0x42d368, 0x42d478, 0x42db50, 0x43e520
 */
// Alternative name is '_ZN15APACCoreDecoderD1Ev'
void __fastcall APACCoreDecoder::~APACCoreDecoder(APACCoreDecoder *this)
{
  _QWORD *v2; // x20
  void (__fastcall *v3)(APACChannelRemapper *); // x1
  void *v4; // x0
  _QWORD *v5; // x20
  _QWORD *v6; // x21
  void *v7; // x0
  void *v8; // x0
  __int64 v9; // [xsp+40h] [xbp+40h]
  *(_QWORD *)this = off_42A9F0;
  APACSyntacticElement::DestroyList((APACCoreDecoder *)((char *)this + 208), (std_vector *)off_42A9F0);
  v2 = *((_QWORD **)this + 32);
  if ( v2 )
  {
    v3 = *(void (__fastcall **)(APACChannelRemapper *))(*v2 + 8LL);
    if ( v3 == APACChannelRemapper::~APACChannelRemapper )
    {
      v4 = (void *)v2[1];
      *v2 = off_42A8C8;
      if ( v4 )
        operator delete(v4);
      operator delete(v2);
    }
    else
    {
      v3(*((APACChannelRemapper **)this + 32));
    }
  }
  v5 = *((_QWORD **)this + 29);
  v6 = *((_QWORD **)this + 30);
  if ( v5 != v6 )
  {
    do
    {
      if ( *v5 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v5 + 8LL))(*v5);
      ++v5;
    }
    while ( v6 != v5 );
    v6 = *((_QWORD **)this + 29);
  }
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 26);
  if ( v7 )
    operator delete(v7);
  *((_QWORD *)this + 2) = off_42B598;
  MDCT::~MDCT((APACCoreDecoder *)((char *)this + 112));
  MDCT::~MDCT((APACCoreDecoder *)((char *)this + 56));
  operator delete(*((void **)this + 6));
  operator delete(*((void **)this + 4));
  v8 = *((void **)this + 1);
  if ( v8 )
    operator delete(v8);
}


/* ------------------------------------------------------------------------ */
/* 0x0007d180  size 84 bytes  21 insns
 *   APACCoreDecoder::~APACCoreDecoder()
 *   [_ZN15APACCoreDecoderD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall APACCoreDecoder::~APACCoreDecoder(APACCoreDecoder *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  APACCoreDecoder::~APACCoreDecoder(this);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x0007d1e0  size 1476 bytes  365 insns
 *   APACCoreDecoder::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>)
 *   [_ZN15APACCoreDecoder10InitializeERKN4apac9ParamListENS0_12observer_ptrINS0_10IASCConfigEEE]
 *   data refs: 0x76d20, 0x7bd80, 0x7be60, 0x42a478, 0x42a4e0, 0x42a578, 0x42a5d0, 0x42a690, 0x42a6e0, 0x42a720, 0x42a8b8, 0x42c9f8, 0x42cd00, 0x42ce00, 0x42d198, 0x42d1b0, 0x42d368, 0x42d3d0, 0x42d478, 0x42d6c0, 0x42d700, 0x42dad0, 0x42de28, 0x42de90
 */
__int64 __fastcall APACCoreDecoder::Initialize(APACCoreDecoder *this, const apac__ParamList *a, void *a2)
{
  __int64 v3; // x5
  __int64 v4; // x6
  __int64 v5; // x7
  __int64 v7; // x3
  __int64 v8; // x4
  char *v9; // x25
  APACCoreDecoder *v10; // x19
  char *v12; // x20
  __int64 v13; // x0
  const char *v14; // x0
  _QWORD *v15; // x1
  const char *v16; // x24
  _QWORD *exception; // x20
  __int64 v18; // x0
  unsigned int v19; // w23
  char *v20; // x20
  __int64 v21; // x0
  _QWORD *v22; // x0
  const char *v23; // x0
  void *v24; // x3
  void *v25; // x0
  std_vector *v26; // x1
  const std_vector *v27; // x1
  unsigned int List; // w20
  _QWORD *v29; // x24
  _QWORD *v30; // x25
  _QWORD *v31; // x20
  unsigned int v32; // w0
  unsigned int *v33; // x2
  _QWORD *v34; // x0
  _QWORD *v35; // x23
  void (__fastcall *v36)(APACChannelRemapper *); // x2
  void *v37; // x0
  __int64 (*v38)(void); // x2
  unsigned int v39; // w0
  _QWORD *v41; // x20
  __int64 v42; // [xsp+0h] [xbp+0h]
  void (__fastcall **v43)(boost::bad_any_cast *__hidden); // [xsp+50h] [xbp+50h]
  void *v44; // [xsp+58h] [xbp+58h] BYREF
  exception v45; // [xsp+60h] [xbp+60h] BYREF
  __int64 v46; // [xsp+90h] [xbp+90h]
  v7 = *((_QWORD *)a + 2);
  v8 = 0;
  if ( !v7 )
    goto LABEL_57;
  v9 = (char *)a + 8;
  v10 = this;
  v8 = *((_QWORD *)a + 2);
  v12 = (char *)a + 8;
  do
  {
    while ( 1 )
    {
      this = (APACCoreDecoder *)*(unsigned int *)(v8 + 32);
      if ( ((unsigned int)this & 0x80000000) != 0 )
        break;
      v12 = (char *)v8;
      v8 = *(_QWORD *)(v8 + 16);
      if ( !v8 )
        goto LABEL_5;
    }
    v8 = *(_QWORD *)(v8 + 24);
  }
  while ( v8 );
LABEL_5:
  if ( v9 == v12 || (this = (APACCoreDecoder *)*((unsigned int *)v12 + 8), (int)this > 0) )
LABEL_57:
    sub_70C28((__int64)this, (__int64)&_stack_chk_guard, (__int64)a2, v7, v8, v3, v4, v5, v42);
  v13 = *((_QWORD *)v12 + 5);
  if ( v13 )
  {
    v14 = *(const char **)((*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v13 + 16LL))(v13) + 8);
    v15 = `typeinfo for'unsigned int;
    v16 = (const char *)`typeinfo for'unsigned int[1];
    if ( v14 == v16 )
      goto LABEL_12;
  }
  else
  {
    v15 = `typeinfo for'unsigned int;
    v14 = (const char *)`typeinfo for'void[1];
    v16 = (const char *)`typeinfo for'unsigned int[1];
    if ( v16 == v14 )
    {
      v19 = *(_DWORD *)(v8 + 8);
      goto LABEL_13;
    }
  }
  if ( *v14 == 42 || strcmp(v14, v16) )
  {
    v43 = off_42A4F0;
    exception = __cxa_allocate_exception(0x38u);
    exception[2] = 0;
    exception[3] = 0;
    exception[4] = 0;
    *((_DWORD *)exception + 10) = -1;
    *exception = &unk_42A690;
    exception[1] = off_42A620;
    exception[6] = &off_42A6E0;
    v44 = off_42A588;
    *(_QWORD *)&v45.type = off_42A5B0;
    memset(&v45.name, 0, 24);
    LODWORD(v45.retval) = -1;
    boost::exception_detail::copy_boost_exception(
      (boost::exception_detail *)(exception + 1),
      &v45,
      (const exception *)off_42A5B0);
    *exception = off_42A738;
    exception[1] = off_42A770;
    exception[6] = &off_42A7A8;
    boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector((std::bad_cast *)&v44);
    __cxa_throw(
      exception,
      (struct type_info *)&`typeinfo for'boost::wrapexcept<boost::bad_any_cast>,
      (void (*)(void *))boost::wrapexcept<boost::bad_any_cast>::~wrapexcept);
  }
LABEL_12:
  v7 = *((_QWORD *)a + 2);
  v18 = *((_QWORD *)v12 + 5);
  v19 = *(_DWORD *)(v18 + 8);
  if ( !v7 )
    goto LABEL_58;
LABEL_13:
  v20 = v9;
  do
  {
    while ( 1 )
    {
      v18 = *(unsigned int *)(v7 + 32);
      if ( (int)v18 <= 1 )
        break;
      v20 = (char *)v7;
      v7 = *(_QWORD *)(v7 + 16);
      if ( !v7 )
        goto LABEL_16;
    }
    v7 = *(_QWORD *)(v7 + 24);
  }
  while ( v7 );
LABEL_16:
  if ( v9 == v20 || (v18 = *((unsigned int *)v20 + 8), (int)v18 > 2) )
LABEL_58:
    sub_70C28(v18, (__int64)v15, (__int64)a2, v7, v8, v3, v4, v5, v42);
  v21 = *((_QWORD *)v20 + 5);
  if ( v21 )
    v22 = (_QWORD *)(*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v21 + 16LL))(v21);
  else
    v22 = `typeinfo for'void;
  v23 = (const char *)v22[1];
  if ( v23 != v16 && (*v23 == 42 || strcmp(v23, v16)) )
  {
    v43 = off_42A4F0;
    v41 = __cxa_allocate_exception(0x38u);
    v41[2] = 0;
    v41[3] = 0;
    v41[4] = 0;
    *((_DWORD *)v41 + 10) = -1;
    *v41 = &unk_42A690;
    v41[1] = off_42A620;
    v41[6] = &off_42A6E0;
    v44 = off_42A588;
    *(_QWORD *)&v45.type = off_42A5B0;
    memset(&v45.name, 0, 24);
    LODWORD(v45.retval) = -1;
    boost::exception_detail::copy_boost_exception(
      (boost::exception_detail *)(v41 + 1),
      &v45,
      (const exception *)off_42A5B0);
    *v41 = off_42A738;
    v41[1] = off_42A770;
    v41[6] = &off_42A7A8;
    boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector((std::bad_cast *)&v44);
    __cxa_throw(
      v41,
      (struct type_info *)&`typeinfo for'boost::wrapexcept<boost::bad_any_cast>,
      (void (*)(void *))boost::wrapexcept<boost::bad_any_cast>::~wrapexcept);
  }
  if ( !a2 )
    return 1;
  APACInstanceConfig::Create((APACInstanceConfig *)v19, *(_DWORD *)(*((_QWORD *)v20 + 5) + 8LL), (unsigned int)a2);
  v25 = *((void **)v10 + 1);
  v26 = (std_vector *)v44;
  *((_QWORD *)v10 + 1) = v44;
  v44 = nullptr;
  if ( v25 )
  {
    operator delete(v25);
    if ( v44 )
      operator delete(v44);
    v26 = *((std_vector **)v10 + 1);
  }
  if ( v26 )
  {
    List = APACSyntacticElement::CreateList((APACCoreDecoder *)((char *)v10 + 208), v26, (APACInstanceConfig *)a2, v24);
    if ( !List )
    {
      v30 = *((_QWORD **)v10 + 29);
      v29 = *((_QWORD **)v10 + 30);
      if ( v30 != v29 )
      {
        v31 = *((_QWORD **)v10 + 29);
        do
        {
          if ( *v31 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v31 + 8LL))(*v31);
          ++v31;
        }
        while ( v29 != v31 );
        *((_QWORD *)v10 + 30) = v30;
      }
      v32 = APACSyntacticElement::NumberOfChannels((APACCoreDecoder *)((char *)v10 + 208), v27);
      v33 = *((unsigned int **)v10 + 1);
      *((_DWORD *)v10 + 50) = v32;
      List = APACSynthesisFilterBank::Initialize((APACCoreDecoder *)((char *)v10 + 16), v32, *v33);
      if ( !List )
      {
        if ( *((_QWORD *)a2 + 6) == *((_QWORD *)a2 + 7) )
          goto LABEL_44;
        v34 = (_QWORD *)operator new(0x20u);
        v35 = *((_QWORD **)v10 + 32);
        *((_QWORD *)v10 + 32) = v34;
        *v34 = off_42A8C8;
        v34[1] = 0;
        v34[2] = 0;
        v34[3] = 0;
        if ( v35 )
        {
          v36 = *(void (__fastcall **)(APACChannelRemapper *))(*v35 + 8LL);
          if ( v36 == APACChannelRemapper::~APACChannelRemapper )
          {
            v37 = (void *)v35[1];
            *v35 = off_42A8C8;
            if ( v37 )
              operator delete(v37);
            operator delete(v35);
            v38 = *(__int64 (**)(void))(**((_QWORD **)v10 + 32) + 16LL);
          }
          else
          {
            v36((APACChannelRemapper *)v35);
            v38 = *(__int64 (**)(void))(**((_QWORD **)v10 + 32) + 16LL);
          }
        }
        else
        {
          v38 = (__int64 (*)(void))APACChannelRemapper::Initialize;
        }
        v39 = v38();
        if ( v39 )
          return v39;
        else
LABEL_44:
          *((_BYTE *)v10 + 204) = 1;
      }
    }
  }
  else
  {
    return 1;
  }
  return List;
}


/* ------------------------------------------------------------------------ */
/* 0x0007d7b0  size 256 bytes  64 insns
 *   APACCoreDecoder::APACCoreDecoder()
 *   APACCoreDecoder::APACCoreDecoder()
 *   [_ZN15APACCoreDecoderC1Ev]
 *   [_ZN15APACCoreDecoderC2Ev]
 *   data refs: 0x42a9e0, 0x42b588, 0x42ca70, 0x42ce00, 0x42db50, 0x43e520
 */
// Alternative name is '_ZN15APACCoreDecoderC2Ev'
void __fastcall APACCoreDecoder::APACCoreDecoder(APACCoreDecoder *this)
{
  __int64 v2; // [xsp+50h] [xbp+50h]
  *((_QWORD *)this + 2) = off_42B598;
  *((_QWORD *)this + 3) = 0;
  *(_QWORD *)this = off_42A9F0;
  *((_QWORD *)this + 1) = 0;
  *((_QWORD *)this + 4) = 0;
  *((_QWORD *)this + 5) = 0;
  *((_QWORD *)this + 6) = 0;
  MDCT::MDCT((APACCoreDecoder *)((char *)this + 56));
  MDCT::MDCT((APACCoreDecoder *)((char *)this + 112));
  *((_QWORD *)this + 21) = 0;
  *((_QWORD *)this + 22) = 0;
  *((_QWORD *)this + 23) = 0;
  *((_QWORD *)this + 24) = 0;
  *((_DWORD *)this + 50) = 0;
  *((_BYTE *)this + 204) = 0;
  *((_QWORD *)this + 26) = 0;
  *((_QWORD *)this + 27) = 0;
  *((_QWORD *)this + 28) = 0;
  *((_QWORD *)this + 29) = 0;
  *((_QWORD *)this + 30) = 0;
  *((_QWORD *)this + 31) = 0;
  *((_QWORD *)this + 32) = 0;
}


/* ------------------------------------------------------------------------ */
/* 0x0007d8b0  size 148 bytes  37 insns
 *   APACCoreDecoder::DeserializeExtTools(TBitstreamReader<unsigned int>&)
 *   [_ZN15APACCoreDecoder19DeserializeExtToolsER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::DeserializeExtTools(APACCoreDecoder *this, TBitstreamReader_u32 *bs)
{
  _QWORD *v3; // x19
  _QWORD *v4; // x21
  __int64 result; // x0
  __int64 v6; // [xsp+40h] [xbp+40h]
  v3 = *((_QWORD **)this + 29);
  v4 = *((_QWORD **)this + 30);
  if ( v3 == v4 )
    return 0;
  while ( 1 )
  {
    result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *))(*(_QWORD *)*v3 + 16LL))(*v3, bs);
    if ( (_DWORD)result )
      break;
    if ( v4 == ++v3 )
      return 0;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0007d950  size 468 bytes  117 insns
 *   APACCoreDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN15APACCoreDecoder11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::Deserialize(APACCoreDecoder *this, TBitstreamReader_u32 *bs, bool flag)
{
  _BOOL4 v5; // w23
  __int64 v6; // x20
  __int64 v7; // x24
  int bitsLeft; // w1
  unsigned int v9; // w25
  unsigned int cache; // w0
  int v11; // w1
  unsigned int v12; // w25
  __int64 v13; // x1
  unsigned int v15; // w0
  int v16; // w1
  unsigned int v17; // w0
  int v18; // w0
  int v19; // w1
  int v20; // w0
  int v21; // w0
  __int64 v22; // [xsp+60h] [xbp+60h]
  v5 = flag;
  v7 = *((_QWORD *)this + 26);
  v6 = *((_QWORD *)this + 27);
  if ( v7 != v6 )
  {
    bitsLeft = bs->bitsLeft;
    do
    {
      cache = bs->cache;
      v11 = bitsLeft - 1;
      bs->bitsLeft = v11;
      v12 = cache >> 31;
      if ( v11 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v15 = bs->cache;
        v16 = bs->bitsLeft;
        LOBYTE(v12) = v12 | (v15 >> (v16 + 32));
        v17 = v15 << -(char)v16;
        if ( v16 == -32 )
          v17 = 0;
        bs->cur += 4;
        bs->cache = v17;
        bs->bitsLeft = v16 + 32;
      }
      else
      {
        bs->cache = 2 * cache;
      }
      v13 = **(_QWORD **)(v7 + 8);
      if ( (_BYTE)v12 )
      {
        v9 = (*(__int64 (**)(void))(v13 + 16))();
        if ( v9 )
          return v9;
        bitsLeft = bs->bitsLeft;
        if ( ((bitsLeft + 8 * ((unsigned int)bs->end - (unsigned int)bs->cur)) & 0x80000000) != 0 )
          return 1;
      }
      else
      {
        (*(void (**)(void))(v13 + 24))();
        bitsLeft = bs->bitsLeft;
        if ( ((bitsLeft + 8 * ((unsigned int)bs->end - (unsigned int)bs->cur)) & 0x80000000) != 0 )
          return 1;
      }
      v7 += 16;
    }
    while ( v6 != v7 );
  }
  v9 = APACCoreDecoder::DeserializeExtTools(this, bs);
  if ( !v9 && v5 )
  {
    v18 = bs->bitsLeft;
    v19 = v18 & 7;
    if ( (v18 & 7) != 0 && (unsigned int)(v19 - 1) <= 0x1F )
    {
      v20 = v18 - v19;
      bs->bitsLeft = v20;
      if ( v20 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v21 = bs->bitsLeft;
        bs->cur += 4;
        bs->bitsLeft = v21 + 32;
        if ( v21 == -32 )
          bs->cache = 0;
        else
          bs->cache <<= -(char)v21;
      }
      else
      {
        bs->cache <<= v19;
      }
    }
  }
  return v9;
}


/* ------------------------------------------------------------------------ */
/* 0x0007db30  size 468 bytes  117 insns
 *   APACCoreDecoder::DecodeAPACFrame(TBitstreamReader<unsigned int>&, apac::FrameType, std::vector<float*, std::allocator<float*> >&)
 *   [_ZN15APACCoreDecoder15DecodeAPACFrameER16TBitstreamReaderIjEN4apac9FrameTypeERSt6vectorIPfSaIS6_EE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::DecodeAPACFrame(
        APACCoreDecoder *this,
        TBitstreamReader_u32 *bs,
        int n,
        std_vector *vec)
{
  __int64 v4; // x19
  __int64 v5; // x20
  __int64 v6; // x25
  __int64 v7; // x26
  __int64 v10; // x23
  __int64 v11; // x28
  unsigned int v12; // w20
  _DWORD *v13; // x25
  float **v14; // x19
  __int64 v15; // x0
  unsigned int v16; // w22
  float **v17; // x19
  __int64 v18; // x0
  __int64 v19; // x0
  __int64 v21; // [xsp+10h] [xbp+10h]
  __int64 v22; // [xsp+18h] [xbp+18h]
  __int64 v23; // [xsp+40h] [xbp+40h]
  __int64 v24; // [xsp+48h] [xbp+48h]
  unsigned int v25; // [xsp+6Ch] [xbp+6Ch]
  _QWORD v26[2]; // [xsp+70h] [xbp+70h] BYREF
  __int64 v27; // [xsp+80h] [xbp+80h]
  v25 = (*(__int64 (__fastcall **)(APACCoreDecoder *, TBitstreamReader_u32 *, __int64))(*(_QWORD *)this + 48LL))(
          this,
          bs,
          1);
  if ( !v25 )
  {
    if ( vec->begin != vec->end )
      vec->end = vec->begin;
    v11 = *((_QWORD *)this + 26);
    v10 = *((_QWORD *)this + 27);
    if ( v11 != v10 )
    {
      v21 = v4;
      v22 = v5;
      v12 = 0;
      v23 = v6;
      while ( 1 )
      {
        v13 = *(_DWORD **)(v11 + 8);
        v14 = (float **)(*(__int64 (__fastcall **)(_DWORD *, __int64))(*(_QWORD *)v13 + 32LL))(v13, 1);
        v15 = (*(__int64 (__fastcall **)(_DWORD *, __int64))(*(_QWORD *)v13 + 40LL))(v13, 1);
        if ( v13[2] == 6 )
          goto LABEL_7;
        v16 = v12 + 1;
        APACSynthesisFilterBank::FrequencyToTimeInPlace(
          (APACCoreDecoder *)((char *)this + 16),
          v12,
          *v14,
          0,
          *(unsigned __int8 *)(v15 + 10));
        v26[0] = *v14;
        std::vector<float *>::emplace_back<float *>(vec, v26);
        if ( v13[2] == 1 )
        {
          v11 += 16;
          v12 += 2;
          v17 = (float **)(*(__int64 (__fastcall **)(_DWORD *, _QWORD))(*(_QWORD *)v13 + 32LL))(v13, 0);
          v18 = (*(__int64 (__fastcall **)(_DWORD *, _QWORD))(*(_QWORD *)v13 + 40LL))(v13, 0);
          APACSynthesisFilterBank::FrequencyToTimeInPlace(
            (APACCoreDecoder *)((char *)this + 16),
            v16,
            *v17,
            0,
            *(unsigned __int8 *)(v18 + 10));
          v26[0] = *v17;
          std::vector<float *>::emplace_back<float *>(vec, v26);
          if ( v10 == v11 )
          {
LABEL_11:
            v4 = v21;
            v5 = v22;
            v6 = v23;
            break;
          }
        }
        else
        {
          ++v12;
LABEL_7:
          v11 += 16;
          if ( v10 == v11 )
            goto LABEL_11;
        }
      }
    }
    v19 = *((_QWORD *)this + 32);
    if ( v19 )
      v25 = (*(__int64 (__fastcall **)(__int64, std_vector *))(*(_QWORD *)v19 + 24LL))(v19, vec);
  }
  if ( v26[1] != _stack_chk_guard )
  {
    v21 = v4;
    v22 = v5;
    v23 = v6;
    v24 = v7;
  }
  return v25;
}


/* ------------------------------------------------------------------------ */
/* 0x0007dd10  size 88 bytes  22 insns
 *   APACCoreDecoder::GetBlockSize() const
 *   [_ZNK15APACCoreDecoder12GetBlockSizeEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::GetBlockSize(APACCoreDecoder *this)
{
  unsigned int v1; // w3
  unsigned int *v2; // x2
  __int64 v4; // [xsp+20h] [xbp+20h]
  v1 = 0;
  v2 = *((unsigned int **)this + 1);
  if ( v2 )
    return *v2;
  return v1;
}


/* ------------------------------------------------------------------------ */
/* 0x0007dd70  size 68 bytes  17 insns
 *   APACCoreDecoder::GetNumChannels() const
 *   [_ZNK15APACCoreDecoder14GetNumChannelsEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACCoreDecoder::GetNumChannels(APACCoreDecoder *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return *((unsigned int *)this + 50);
}


