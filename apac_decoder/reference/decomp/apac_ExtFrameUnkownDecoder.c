/* ============================================================================
 * apac::ExtFrameUnkownDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 696 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000955a0     64  apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder() */
/* 0x00095b30     64  apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder() */
/* 0x00095e70    500  apac::ExtFrameUnkownDecoder::Decode(TBitstreamReader<unsigned int>&) */
/* 0x00096290     68  apac::ExtFrameUnkownDecoder::Initialize(apac::IExtCfgElement const&) */

/* ------------------------------------------------------------------------ */
/* 0x000955a0  size 64 bytes  16 insns
 *   apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()
 *   [_ZN4apac21ExtFrameUnkownDecoderD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder(apac__ExtFrameUnkownDecoder *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00095b30  size 64 bytes  16 insns
 *   apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()
 *   apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder()
 *   [_ZN4apac21ExtFrameUnkownDecoderD1Ev]
 *   [_ZN4apac21ExtFrameUnkownDecoderD2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN4apac21ExtFrameUnkownDecoderD2Ev'
void __fastcall apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder(apac__ExtFrameUnkownDecoder *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
}


/* ------------------------------------------------------------------------ */
/* 0x00095e70  size 500 bytes  125 insns
 *   apac::ExtFrameUnkownDecoder::Decode(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac21ExtFrameUnkownDecoder6DecodeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::ExtFrameUnkownDecoder::Decode(apac__ExtFrameUnkownDecoder *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x21
  TBitstreamReader_u32 *v3; // x0
  unsigned int cache; // w1
  int v5; // w2
  unsigned int v6; // w19
  int v8; // w2
  unsigned int v9; // w1
  unsigned int v10; // w21
  unsigned int v11; // w1
  int v12; // w2
  unsigned int v13; // w19
  const unsigned __int8 *v14; // x1
  unsigned int v15; // w3
  const unsigned __int8 *v16; // x1
  int v17; // w4
  const unsigned __int8 *cur; // x1
  unsigned int v19; // w3
  const unsigned __int8 *v20; // x1
  int bitsLeft; // w4
  const unsigned __int8 *v22; // x1
  unsigned int v23; // w1
  int v24; // w2
  unsigned int v25; // w1
  __int64 v26; // [xsp+20h] [xbp+20h]
  TBitstreamReader_u32 *v27; // [xsp+38h] [xbp+38h]
  __int64 v28; // [xsp+48h] [xbp+48h]
  __int64 v29; // [xsp+50h] [xbp+50h]
  v3 = bs;
  cache = bs->cache;
  v5 = v3->bitsLeft - 4;
  v3->bitsLeft = v5;
  v6 = cache >> 28;
  if ( (v5 & 0x80000000) == 0 )
  {
    v3->cache = 16 * cache;
LABEL_3:
    if ( v6 != 15 )
      goto LABEL_4;
    v26 = v2;
    v8 = v5 - 8;
    v3->bitsLeft = v8;
    v9 = v3->cache;
    v10 = HIBYTE(v9);
    if ( (v8 & 0x80000000) == 0 )
    {
      v6 = v10 + 15;
      v3->cache = v9 << 8;
      goto LABEL_8;
    }
    cur = v3->cur;
LABEL_18:
    v27 = v3;
    TBitstreamReader<unsigned int>::FillCacheFrom(v3, cur);
    v3 = v27;
    v19 = v27->cache;
    v20 = v27->cur;
    bitsLeft = v27->bitsLeft;
    v8 = bitsLeft + 32;
    v27->bitsLeft = bitsLeft + 32;
    v22 = v20 + 4;
    v3->cur = v22;
    v10 |= v19 >> (bitsLeft + 32);
    v6 = v10 + 15;
    if ( bitsLeft == -32 )
    {
      v3->cache = 0;
      if ( v10 != 255 )
        goto LABEL_9;
      v13 = 0;
      v3->bitsLeft = -16;
      goto LABEL_21;
    }
    v3->cache = v19 << -(char)bitsLeft;
LABEL_8:
    if ( v10 != 255 )
    {
LABEL_9:
      v2 = v26;
      goto LABEL_4;
    }
    v11 = v3->cache;
    v12 = v8 - 16;
    v3->bitsLeft = v12;
    v13 = HIWORD(v11);
    if ( (v12 & 0x80000000) == 0 )
    {
      v3->cache = v11 << 16;
LABEL_12:
      v6 = v13 + 270;
      v2 = v26;
      goto LABEL_4;
    }
    v22 = v3->cur;
LABEL_21:
    v27 = v3;
    TBitstreamReader<unsigned int>::FillCacheFrom(v3, v22);
    v3 = v27;
    v23 = v27->cache;
    v24 = v27->bitsLeft;
    v13 |= v23 >> (v24 + 32);
    v25 = v23 << -(char)v24;
    if ( v24 == -32 )
      v25 = 0;
    v27->cur += 4;
    v3->cache = v25;
    v3->bitsLeft = v24 + 32;
    goto LABEL_12;
  }
  v14 = v3->cur;
  v27 = v3;
  TBitstreamReader<unsigned int>::FillCacheFrom(v3, v14);
  v3 = v27;
  v15 = v27->cache;
  v16 = v27->cur;
  v17 = v27->bitsLeft;
  v5 = v17 + 32;
  v27->bitsLeft = v17 + 32;
  cur = v16 + 4;
  v3->cur = cur;
  v6 |= v15 >> (v17 + 32);
  if ( v17 != -32 )
  {
    v3->cache = v15 << -(char)v17;
    goto LABEL_3;
  }
  v3->cache = 0;
  if ( v6 == 15 )
  {
    v26 = v2;
    v10 = 0;
    v3->bitsLeft = -8;
    goto LABEL_18;
  }
LABEL_4:
  TBitstreamReader<unsigned int>::SkipBits(v3, 8 * (v6 + 1));
  if ( v28 != _stack_chk_guard )
    v26 = v2;
  return 0;
}


/* ------------------------------------------------------------------------ */
/* 0x00096290  size 68 bytes  17 insns
 *   apac::ExtFrameUnkownDecoder::Initialize(apac::IExtCfgElement const&)
 *   [_ZN4apac21ExtFrameUnkownDecoder10InitializeERKNS_14IExtCfgElementE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::ExtFrameUnkownDecoder::Initialize(
        apac__ExtFrameUnkownDecoder *this,
        const apac__IExtCfgElement *a)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return 0;
}


