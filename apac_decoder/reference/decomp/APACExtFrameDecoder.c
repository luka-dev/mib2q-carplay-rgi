/* ============================================================================
 * APACExtFrameDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 524 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00096070    148  APACExtFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int>&) */
/* 0x00096110    376  APACExtFrameDecoder::APACExtFrameDecoder(apac::GlobalConfig const&) */

/* ------------------------------------------------------------------------ */
/* 0x00096070  size 148 bytes  37 insns
 *   APACExtFrameDecoder::DecodeFrame(TBitstreamReader<unsigned int>&)
 *   [_ZN19APACExtFrameDecoder11DecodeFrameER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACExtFrameDecoder::DecodeFrame(APACExtFrameDecoder *this, TBitstreamReader_u32 *bs)
{
  _QWORD *v3; // x19
  _QWORD *v4; // x21
  __int64 result; // x0
  __int64 v6; // [xsp+40h] [xbp+40h]
  v3 = *(_QWORD **)this;
  v4 = *((_QWORD **)this + 1);
  if ( *(_QWORD **)this == v4 )
    return 0;
  while ( 1 )
  {
    result = (*(__int64 (__fastcall **)(_QWORD, TBitstreamReader_u32 *))(*(_QWORD *)*v3 + 24LL))(*v3, bs);
    if ( (_DWORD)result )
      break;
    if ( v4 == ++v3 )
      return 0;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00096110  size 376 bytes  94 insns
 *   APACExtFrameDecoder::APACExtFrameDecoder(apac::GlobalConfig const&)
 *   APACExtFrameDecoder::APACExtFrameDecoder(apac::GlobalConfig const&)
 *   [_ZN19APACExtFrameDecoderC1ERKN4apac12GlobalConfigE]
 *   [_ZN19APACExtFrameDecoderC2ERKN4apac12GlobalConfigE]
 *   data refs: 0x955a0, 0x42af70, 0x42ce00, 0x42d888, 0x42da30, 0x43e520
 */
// Alternative name is '_ZN19APACExtFrameDecoderC2ERKN4apac12GlobalConfigE'
void __fastcall APACExtFrameDecoder::APACExtFrameDecoder(APACExtFrameDecoder *this, const apac__GlobalConfig *a)
{
  __int64 v3; // x20
  __int64 v4; // x22
  void (*v5)(void); // x2
  _DWORD *v6; // x0
  _QWORD *v7; // x1
  _QWORD *v8; // x2
  void *v9; // [xsp+40h] [xbp+40h] BYREF
  __int64 v10; // [xsp+50h] [xbp+50h]
  *(_QWORD *)this = 0;
  *((_QWORD *)this + 1) = 0;
  *((_QWORD *)this + 2) = 0;
  v3 = *((_QWORD *)a + 12);
  v4 = *((_QWORD *)a + 13);
  if ( v3 != v4 )
  {
    while ( 1 )
    {
      v6 = (_DWORD *)operator new(0x10u);
      v7 = *((_QWORD **)this + 1);
      v8 = *((_QWORD **)this + 2);
      *(_QWORD *)v6 = off_42AF80;
      v6[2] = 65520;
      v9 = v6;
      if ( v7 == v8 )
      {
        std::vector<std::unique_ptr<apac::IExtFrameElementDecoder>>::_M_realloc_insert<std::unique_ptr<apac::IExtFrameElementDecoder>>(
          (std_vector *)this,
          v7,
          &v9);
      }
      else
      {
        v9 = nullptr;
        *v7 = v6;
        *((_QWORD *)this + 1) += 8LL;
      }
      if ( !v9 )
        goto LABEL_7;
      v5 = *(void (**)(void))(*(_QWORD *)v9 + 8LL);
      if ( (char *)v5 == (char *)apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder )
      {
        operator delete(v9);
LABEL_7:
        v3 += 8;
        if ( v4 == v3 )
          return;
      }
      else
      {
        v3 += 8;
        v5();
        if ( v4 == v3 )
          return;
      }
    }
  }
}


