/* ============================================================================
 * APACCoreLBRDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 1496 bytes of .text.
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
/* 0x0007e440    944  APACCoreLBRDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool) */
/* 0x0007e7f0    304  APACCoreLBRDecoder::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>) */
/* 0x0007e920    120  APACCoreLBRDecoder::~APACCoreLBRDecoder() */
/* 0x0007e9a0    128  APACCoreLBRDecoder::~APACCoreLBRDecoder() */

/* ------------------------------------------------------------------------ */
/* 0x0007e440  size 944 bytes  236 insns
 *   APACCoreLBRDecoder::Deserialize(TBitstreamReader<unsigned int>&, bool)
 *   [_ZN18APACCoreLBRDecoder11DeserializeER16TBitstreamReaderIjEb]
 *   data refs: 0x79730, 0x42ce00, 0x42cf78, 0x43e520
 */
__int64 __fastcall APACCoreLBRDecoder::Deserialize(APACCoreLBRDecoder *this, TBitstreamReader_u32 *bs, bool flag)
{
  __int64 v3; // x23
  __int64 v4; // x24
  __int64 v7; // x22
  __int64 v8; // x28
  unsigned int v9; // w20
  unsigned int cache; // w1
  int v11; // w0
  unsigned int v12; // w25
  __int64 v13; // x1
  unsigned int v14; // w2
  __int64 v15; // x0
  _QWORD *v16; // x0
  unsigned __int8 (*v17)(void); // x2
  int v18; // w1
  _DWORD *v19; // x0
  __int64 v20; // x26
  __int64 (__fastcall *v21)(__int64, __int64, __int64, _QWORD); // x27
  __int64 v22; // x25
  __int64 v23; // x0
  unsigned int v24; // w27
  _QWORD *v26; // x0
  unsigned __int8 (*v27)(void); // x2
  int v28; // w0
  __int64 v29; // x0
  __int64 v30; // x5
  __int64 (__fastcall *v31)(__int64, _QWORD); // x2
  __int64 v32; // x0
  __int64 v33; // x2
  __int64 v34; // x0
  unsigned int v35; // w0
  int bitsLeft; // w1
  unsigned int v37; // w0
  int v38; // w0
  int v39; // w1
  int v40; // w0
  const unsigned __int8 *cur; // x1
  int v42; // w0
  __int64 v43; // [xsp+30h] [xbp+30h]
  __int64 v44; // [xsp+38h] [xbp+38h]
  bool (__fastcall *v45)(APACBWE2Decoder *, unsigned int); // [xsp+68h] [xbp+68h]
  __int64 v46; // [xsp+70h] [xbp+70h]
  __int64 (__fastcall *v47)(__int64, __int64, __int64, _QWORD); // [xsp+78h] [xbp+78h]
  __int64 v48; // [xsp+80h] [xbp+80h]
  _BOOL4 v49; // [xsp+8Ch] [xbp+8Ch]
  __int64 v50; // [xsp+98h] [xbp+98h]
  __int64 v51; // [xsp+A0h] [xbp+A0h]
  v8 = *((_QWORD *)this + 26);
  v7 = *((_QWORD *)this + 27);
  v49 = flag;
  if ( v8 != v7 )
  {
    v9 = 0;
    v45 = APACBWE2Decoder::GetActiveFlag;
    do
    {
      cache = bs->cache;
      v11 = bs->bitsLeft - 1;
      bs->bitsLeft = v11;
      v12 = cache >> 31;
      if ( v11 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v35 = bs->cache;
        bitsLeft = bs->bitsLeft;
        LOBYTE(v12) = v12 | (v35 >> (bitsLeft + 32));
        v37 = v35 << -(char)bitsLeft;
        if ( bitsLeft == -32 )
          v37 = 0;
        bs->cur += 4;
        bs->cache = v37;
        bs->bitsLeft = bitsLeft + 32;
      }
      else
      {
        bs->cache = 2 * cache;
      }
      v13 = **(_QWORD **)(v8 + 8);
      if ( (_BYTE)v12 )
      {
        v14 = (*(__int64 (**)(void))(v13 + 16))();
        if ( v14 )
          goto LABEL_21;
        v15 = *((_QWORD *)this + 33);
        if ( v15 )
        {
          if ( *(_DWORD *)(*(_QWORD *)(v8 + 8) + 8LL) == 6 )
            goto LABEL_18;
          v14 = (*(__int64 (__fastcall **)(__int64, TBitstreamReader_u32 *, _QWORD))(*(_QWORD *)v15 + 24LL))(
                  v15,
                  bs,
                  v9);
          if ( v14 )
            goto LABEL_21;
          v16 = *((_QWORD **)this + 33);
          v17 = *(unsigned __int8 (**)(void))(*v16 + 40LL);
          if ( (char *)v17 == (char *)APACBWE2Decoder::GetActiveFlag )
            v18 = ((1LL << v9) & *(_QWORD *)(v16[1] + 8LL * (v9 >> 6))) != 0;
          else
            v18 = v17();
          v19 = *(_DWORD **)(v8 + 8);
          if ( v18 )
          {
            v20 = *((_QWORD *)this + 33);
            v21 = *(__int64 (__fastcall **)(__int64, __int64, __int64, _QWORD))(*(_QWORD *)v20 + 32LL);
            v22 = (*(__int64 (__fastcall **)(_DWORD *, __int64))(*(_QWORD *)v19 + 40LL))(v19, 1);
            v23 = (*(__int64 (__fastcall **)(_QWORD, __int64))(**(_QWORD **)(v8 + 8) + 32LL))(*(_QWORD *)(v8 + 8), 1);
            v14 = v21(v20, v22, v23, v9);
            if ( v14 )
              goto LABEL_21;
            v19 = *(_DWORD **)(v8 + 8);
          }
          v24 = v9 + 1;
          if ( v19[2] == 1 )
          {
            v26 = *((_QWORD **)this + 33);
            v27 = *(unsigned __int8 (**)(void))(*v26 + 40LL);
            if ( (char *)v27 == (char *)v45 )
              v28 = ((1LL << v24) & *(_QWORD *)(v26[1] + 8LL * (v24 >> 6))) != 0;
            else
              v28 = v27();
            if ( v28 )
            {
              v29 = *(_QWORD *)(v8 + 8);
              v30 = *((_QWORD *)this + 33);
              v31 = *(__int64 (__fastcall **)(__int64, _QWORD))(*(_QWORD *)v29 + 40LL);
              v47 = *(__int64 (__fastcall **)(__int64, __int64, __int64, _QWORD))(*(_QWORD *)v30 + 32LL);
              v48 = v30;
              v32 = v31(v29, 0);
              v33 = *(_QWORD *)(v8 + 8);
              v46 = v32;
              v34 = (*(__int64 (__fastcall **)(__int64, _QWORD))(*(_QWORD *)v33 + 32LL))(v33, 0);
              v14 = v47(v48, v46, v34, v24);
              if ( v14 )
                goto LABEL_21;
            }
            v24 = v9 + 2;
          }
          v9 = v24;
        }
        if ( ((bs->bitsLeft + 8 * ((unsigned int)bs->end - (unsigned int)bs->cur)) & 0x80000000) != 0 )
          goto LABEL_29;
      }
      else
      {
        ++v9;
        (*(void (**)(void))(v13 + 24))();
        if ( ((bs->bitsLeft + 8 * ((unsigned int)bs->end - (unsigned int)bs->cur)) & 0x80000000) != 0 )
        {
LABEL_29:
          v14 = 1;
          goto LABEL_21;
        }
      }
LABEL_18:
      v8 += 16;
    }
    while ( v7 != v8 );
  }
  v14 = APACCoreDecoder::DeserializeExtTools(this, bs);
  if ( !v14 && v49 )
  {
    v38 = bs->bitsLeft;
    v39 = v38 & 7;
    if ( (v38 & 7) != 0 && (unsigned int)(v39 - 1) <= 0x1F )
    {
      v40 = v38 - v39;
      bs->bitsLeft = v40;
      if ( v40 < 0 )
      {
        cur = bs->cur;
        LODWORD(v45) = 0;
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, cur);
        v42 = bs->bitsLeft;
        bs->cur += 4;
        bs->bitsLeft = v42 + 32;
        v14 = (unsigned int)v45;
        if ( v42 == -32 )
          bs->cache = 0;
        else
          bs->cache <<= -(char)v42;
      }
      else
      {
        bs->cache <<= v39;
      }
    }
  }
LABEL_21:
  if ( v50 != _stack_chk_guard )
  {
    v43 = v3;
    v44 = v4;
  }
  return v14;
}


/* ------------------------------------------------------------------------ */
/* 0x0007e7f0  size 304 bytes  76 insns
 *   APACCoreLBRDecoder::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>)
 *   [_ZN18APACCoreLBRDecoder10InitializeERKN4apac9ParamListENS0_12observer_ptrINS0_10IASCConfigEEE]
 *   data refs: 0x79670, 0x42a868, 0x42ce00, 0x42d168, 0x42d750, 0x43e520
 */
__int64 __fastcall APACCoreLBRDecoder::Initialize(APACCoreLBRDecoder *this, const apac__ParamList *a, void *a2)
{
  __int64 v3; // x23
  __int64 result; // x0
  void *v7; // x3
  std_vector *v8; // x22
  __int64 v9; // x0
  __int64 v10; // x3
  __int64 (*v11)(void); // x2
  const std_vector *v12; // x1
  __int64 v13; // [xsp+30h] [xbp+30h]
  __int64 v14; // [xsp+48h] [xbp+48h]
  __int64 v15; // [xsp+50h] [xbp+50h]
  result = APACCoreDecoder::Initialize(this, a, a2);
  if ( (_DWORD)result )
    goto LABEL_7;
  v8 = *((std_vector **)this + 1);
  if ( *((_BYTE *)a2 + 96) )
  {
    result = APACSyntacticElement::CreateList(
               (APACCoreLBRDecoder *)((char *)this + 208),
               v8,
               (APACInstanceConfig *)a2,
               v7);
    if ( (_DWORD)result )
      goto LABEL_7;
    *((_DWORD *)this + 50) = APACSyntacticElement::NumberOfChannels((APACCoreLBRDecoder *)((char *)this + 208), v12);
    v8 = *((std_vector **)this + 1);
  }
  v9 = operator new(0x70u);
  *(_DWORD *)(v9 + 16) = 0;
  v10 = *((_QWORD *)this + 33);
  *(_QWORD *)(v9 + 24) = 0;
  *(_DWORD *)(v9 + 32) = 0;
  *(_QWORD *)(v9 + 40) = 0;
  *(_QWORD *)(v9 + 48) = 0;
  *(_QWORD *)v9 = off_42A878;
  *(_QWORD *)(v9 + 8) = 0;
  *(_QWORD *)(v9 + 56) = 0;
  *(_QWORD *)(v9 + 64) = 0;
  *(_QWORD *)(v9 + 72) = 0;
  *(_QWORD *)(v9 + 80) = v8;
  *(_QWORD *)(v9 + 88) = 0;
  *(_QWORD *)(v9 + 96) = 0;
  *(_QWORD *)(v9 + 104) = 0;
  *((_QWORD *)this + 33) = v9;
  if ( v10 )
  {
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v10 + 8LL))(v10);
    v11 = *(__int64 (**)(void))(**((_QWORD **)this + 33) + 16LL);
  }
  else
  {
    v11 = (__int64 (*)(void))APACBWE2Decoder::Initialize;
  }
  result = v11();
  if ( (_DWORD)result )
    *((_BYTE *)this + 204) = 0;
LABEL_7:
  if ( v14 != _stack_chk_guard )
    v13 = v3;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0007e920  size 120 bytes  30 insns
 *   APACCoreLBRDecoder::~APACCoreLBRDecoder()
 *   APACCoreLBRDecoder::~APACCoreLBRDecoder()
 *   [_ZN18APACCoreLBRDecoderD1Ev]
 *   [_ZN18APACCoreLBRDecoderD2Ev]
 *   data refs: 0x42aaa0, 0x42cd10, 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN18APACCoreLBRDecoderD2Ev'
void __fastcall APACCoreLBRDecoder::~APACCoreLBRDecoder(APACCoreLBRDecoder *this)
{
  __int64 v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((_QWORD *)this + 33);
  *(_QWORD *)this = off_42AAB0;
  if ( v2 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v2 + 8LL))(v2);
  APACCoreDecoder::~APACCoreDecoder(this);
}


/* ------------------------------------------------------------------------ */
/* 0x0007e9a0  size 128 bytes  32 insns
 *   APACCoreLBRDecoder::~APACCoreLBRDecoder()
 *   [_ZN18APACCoreLBRDecoderD0Ev]
 *   data refs: 0x42aaa0, 0x42cd10, 0x42ce00, 0x43e520
 */
void __fastcall APACCoreLBRDecoder::~APACCoreLBRDecoder(APACCoreLBRDecoder *this)
{
  __int64 v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((_QWORD *)this + 33);
  *(_QWORD *)this = off_42AAB0;
  if ( v2 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v2 + 8LL))(v2);
  APACCoreDecoder::~APACCoreDecoder(this);
  operator delete(this);
}


