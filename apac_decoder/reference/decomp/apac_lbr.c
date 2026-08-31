/* ============================================================================
 * apac::lbr
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 728 bytes of .text.
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
/* 0x0007e160    204  apac::lbr::DecoderConfig::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x0007e230    268  apac::lbr::DecoderConfig::~DecoderConfig() */
/* 0x0007e340    256  apac::lbr::DecoderConfig::~DecoderConfig() */

/* ------------------------------------------------------------------------ */
/* 0x0007e160  size 204 bytes  51 insns
 *   apac::lbr::DecoderConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac3lbr13DecoderConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::lbr::DecoderConfig::Deserialize(apac__lbr__DecoderConfig *this, TBitstreamReader_u32 *bs)
{
  unsigned int cache; // w0
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
    v6 |= v8 >> (bitsLeft + 32);
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
  *((_BYTE *)this + 96) = v6 != 0;
  return apac::chan::CoreDecoderConfig::Deserialize(this, bs);
}


/* ------------------------------------------------------------------------ */
/* 0x0007e230  size 268 bytes  67 insns
 *   apac::lbr::DecoderConfig::~DecoderConfig()
 *   apac::lbr::DecoderConfig::~DecoderConfig()
 *   [_ZN4apac3lbr13DecoderConfigD1Ev]
 *   [_ZN4apac3lbr13DecoderConfigD2Ev]
 *   data refs: 0x7c850, 0x42a948, 0x42a980, 0x42ce00, 0x42d268, 0x42d3f0, 0x42d480, 0x43e520
 */
// Alternative name is '_ZN4apac3lbr13DecoderConfigD2Ev'
void __fastcall apac::lbr::DecoderConfig::~DecoderConfig(apac__lbr__DecoderConfig *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void (__fastcall ***v5)(apac__TCEConfig *); // x22
  void (__fastcall ***v6)(apac__TCEConfig *); // x20
  void (__fastcall *v7)(apac__TCEConfig *); // x1
  void (__fastcall ***v8)(apac__TCEConfig *); // x0
  __int64 v9; // x0
  __int64 v10; // [xsp+30h] [xbp+30h]
  __int64 v11; // [xsp+48h] [xbp+48h]
  __int64 v12; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 9);
  *(_QWORD *)this = off_42A990;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 6);
  if ( v4 )
    operator delete(v4);
  *(_QWORD *)this = &unk_42A958;
  v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  v6 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  if ( v6 != v5 )
  {
    do
    {
      while ( 1 )
      {
        v7 = **v6;
        if ( v7 != apac::TCEConfig::~TCEConfig )
          break;
        v6 += 2;
        if ( v5 == v6 )
          goto LABEL_10;
      }
      v8 = v6;
      v6 += 2;
      v7((apac__TCEConfig *)v8);
    }
    while ( v5 != v6 );
LABEL_10:
    v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  }
  v9 = v11 ^ _stack_chk_guard;
  if ( v5 )
  {
    if ( !v9 )
    {
      operator delete(v5);
      return;
    }
LABEL_15:
    v10 = v1;
  }
  if ( v9 )
    goto LABEL_15;
}


/* ------------------------------------------------------------------------ */
/* 0x0007e340  size 256 bytes  64 insns
 *   apac::lbr::DecoderConfig::~DecoderConfig()
 *   [_ZN4apac3lbr13DecoderConfigD0Ev]
 *   data refs: 0x7c850, 0x42a948, 0x42a980, 0x42ce00, 0x42d268, 0x42d3f0, 0x42d480, 0x43e520
 */
void __fastcall apac::lbr::DecoderConfig::~DecoderConfig(apac__lbr__DecoderConfig *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void (__fastcall ***v5)(apac__TCEConfig *); // x22
  void (__fastcall ***v6)(apac__TCEConfig *); // x20
  void (__fastcall *v7)(apac__TCEConfig *); // x1
  void (__fastcall ***v8)(apac__TCEConfig *); // x0
  __int64 v9; // [xsp+30h] [xbp+30h]
  __int64 v10; // [xsp+48h] [xbp+48h]
  __int64 v11; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 9);
  *(_QWORD *)this = off_42A990;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 6);
  if ( v4 )
    operator delete(v4);
  *(_QWORD *)this = &unk_42A958;
  v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  v6 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  if ( v6 != v5 )
  {
    do
    {
      while ( 1 )
      {
        v7 = **v6;
        if ( v7 != apac::TCEConfig::~TCEConfig )
          break;
        v6 += 2;
        if ( v5 == v6 )
          goto LABEL_10;
      }
      v8 = v6;
      v6 += 2;
      v7((apac__TCEConfig *)v8);
    }
    while ( v5 != v6 );
LABEL_10:
    v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  }
  if ( v5 )
    operator delete(v5);
  if ( v10 != _stack_chk_guard )
    v9 = v1;
  operator delete(this);
}


