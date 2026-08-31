/* ============================================================================
 * APACIndividualChannelStream
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 392 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009a610    200  APACIndividualChannelStream::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo&, bool) */
/* 0x0009a7b0     96  APACIndividualChannelStream::~APACIndividualChannelStream() */
/* 0x0009a810     96  APACIndividualChannelStream::~APACIndividualChannelStream() */

/* ------------------------------------------------------------------------ */
/* 0x0009a610  size 200 bytes  50 insns
 *   APACIndividualChannelStream::Deserialize(TBitstreamReader<unsigned int>&, APACICSInfo&, bool)
 *   [_ZN27APACIndividualChannelStream11DeserializeER16TBitstreamReaderIjER11APACICSInfob]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACIndividualChannelStream::Deserialize(
        APACIndividualChannelStream *this,
        TBitstreamReader_u32 *bs,
        APACICSInfo *ics,
        bool flag)
{
  const APACScaleFactorData *v6; // x24
  APACScaleFactorData *v7; // x0
  __int64 v9; // x5
  const apac__SFBOffsets *v10; // x19
  const apac__SFBOffsets *v11; // x5
  __int64 result; // x0
  __int64 v13; // [xsp+50h] [xbp+50h]
  v6 = (APACIndividualChannelStream *)((char *)this + 24);
  v7 = (APACIndividualChannelStream *)((char *)this + 24);
  v9 = *(_QWORD *)(*((_QWORD *)this + 2) + 16LL);
  v10 = (const apac__SFBOffsets *)(v9 + 24);
  v11 = (const apac__SFBOffsets *)(v9 + 8);
  if ( *((_BYTE *)ics + 10) != 2 )
    v10 = v11;
  result = APACScaleFactorData::Deserialize(v7, bs, ics);
  if ( !(_DWORD)result )
    return APACSpectralData::Deserialize((APACIndividualChannelStream *)((char *)this + 752), bs, ics, v6, v10);
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0009a7b0  size 96 bytes  24 insns
 *   APACIndividualChannelStream::~APACIndividualChannelStream()
 *   APACIndividualChannelStream::~APACIndividualChannelStream()
 *   [_ZN27APACIndividualChannelStreamD1Ev]
 *   [_ZN27APACIndividualChannelStreamD2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN27APACIndividualChannelStreamD2Ev'
void __fastcall APACIndividualChannelStream::~APACIndividualChannelStream(APACIndividualChannelStream *this)
{
  void *v1; // x0
  __int64 v2; // [xsp+20h] [xbp+20h]
  v1 = *((void **)this + 94);
  if ( v1 )
    free(v1);
}


/* ------------------------------------------------------------------------ */
/* 0x0009a810  size 96 bytes  24 insns
 *   APACIndividualChannelStream::~APACIndividualChannelStream()
 *   [_ZN27APACIndividualChannelStreamD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall APACIndividualChannelStream::~APACIndividualChannelStream(APACIndividualChannelStream *this)
{
  void *v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((void **)this + 94);
  if ( v2 )
    free(v2);
  operator delete(this);
}


