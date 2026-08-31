/* ============================================================================
 * apac::LoudnessDRCConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 272 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00072bb0    272  apac::LoudnessDRCConfig::Deserialize(TBitstreamReader<unsigned int>&, apac::ParamList const&, mpddrc::HeaderPayloadType) */

/* ------------------------------------------------------------------------ */
/* 0x00072bb0  size 272 bytes  68 insns
 *   apac::LoudnessDRCConfig::Deserialize(TBitstreamReader<unsigned int>&, apac::ParamList const&, mpddrc::HeaderPayloadType)
 *   [_ZN4apac17LoudnessDRCConfig11DeserializeER16TBitstreamReaderIjERKNS_9ParamListEN6mpddrc17HeaderPayloadTypeE]
 *   data refs: 0x42a330, 0x42ce00, 0x42d618, 0x43e520
 */
bool __fastcall apac::LoudnessDRCConfig::Deserialize(
        apac__LoudnessDRCConfig *this,
        TBitstreamReader_u32 *bs,
        const apac__ParamList *a,
        int n)
{
  const apac__ParamList *v4; // x23
  apac__drc__DynRangeCompressor *v7; // x0
  bool result; // w0
  apac__drc__DynRangeCompressor *v10; // x0
  apac__drc__DynRangeCompressor *v11; // x2
  int v12; // w1
  const apac__ParamList *v13; // [xsp+30h] [xbp+30h]
  __int64 v14; // [xsp+48h] [xbp+48h]
  __int64 v15; // [xsp+50h] [xbp+50h]
  v7 = *(apac__drc__DynRangeCompressor **)this;
  if ( v7 )
    goto LABEL_2;
  v4 = a;
  v10 = (apac__drc__DynRangeCompressor *)operator new(0x48u);
  v11 = *(apac__drc__DynRangeCompressor **)this;
  *(_QWORD *)this = v10;
  *(_QWORD *)v10 = off_42A340;
  *((_BYTE *)v10 + 8) = 0;
  *((_WORD *)v10 + 5) = 0;
  *((_QWORD *)v10 + 2) = 0;
  *((_QWORD *)v10 + 3) = 0;
  *((_DWORD *)v10 + 8) = 0;
  *((_QWORD *)v10 + 5) = 0;
  *((_QWORD *)v10 + 6) = 0;
  *((_BYTE *)v10 + 56) = 1;
  *(_QWORD *)((char *)v10 + 60) = 0x3F800000C1F80000LL;
  if ( v11 )
  {
    (*(void (__fastcall **)(apac__drc__DynRangeCompressor *))(*(_QWORD *)v11 + 8LL))(v11);
    v10 = *(apac__drc__DynRangeCompressor **)this;
  }
  v12 = apac::drc::DynRangeCompressor::Initialize(v10, v4);
  result = 1;
  if ( !v12 )
  {
    v7 = *(apac__drc__DynRangeCompressor **)this;
LABEL_2:
    result = (unsigned int)apac::drc::DynRangeCompressor::Deserialize(v7, bs, n) != 0;
  }
  if ( v14 != _stack_chk_guard )
    v13 = v4;
  return result;
}


