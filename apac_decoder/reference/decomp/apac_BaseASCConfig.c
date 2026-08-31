/* ============================================================================
 * apac::BaseASCConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 756 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00097300     68  apac::BaseASCConfig::GetChannelLayout() const */
/* 0x000975f0    120  apac::BaseASCConfig::GetNumChannels() const */
/* 0x00098f00     68  apac::BaseASCConfig::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x00098f50    244  apac::BaseASCConfig::~BaseASCConfig() */
/* 0x00099050    256  apac::BaseASCConfig::~BaseASCConfig() */

/* ------------------------------------------------------------------------ */
/* 0x00097300  size 68 bytes  17 insns
 *   apac::BaseASCConfig::GetChannelLayout() const
 *   [_ZNK4apac13BaseASCConfig16GetChannelLayoutEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::BaseASCConfig::GetChannelLayout(apac__BaseASCConfig *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return *((_QWORD *)this + 6);
}


/* ------------------------------------------------------------------------ */
/* 0x000975f0  size 120 bytes  30 insns
 *   apac::BaseASCConfig::GetNumChannels() const
 *   [_ZNK4apac13BaseASCConfig14GetNumChannelsEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::BaseASCConfig::GetNumChannels(apac__BaseASCConfig *this)
{
  int8x8_t v1; // d0
  _DWORD *v2; // x2
  __int64 result; // x0
  __int64 v4; // [xsp+20h] [xbp+20h]
  v2 = *((_DWORD **)this + 6);
  if ( !*v2 )
    return (unsigned int)v2[2];
  result = (unsigned __int16)*v2;
  if ( *v2 == 0x10000 )
  {
    v1.n64_u32[0] = v2[1];
    return (unsigned __int8)vaddv_s8(vcnt_s8(v1));
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00098f00  size 68 bytes  17 insns
 *   apac::BaseASCConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac13BaseASCConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::BaseASCConfig::Deserialize(apac__BaseASCConfig *this, TBitstreamReader_u32 *bs)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return 5;
}


/* ------------------------------------------------------------------------ */
/* 0x00098f50  size 244 bytes  61 insns
 *   apac::BaseASCConfig::~BaseASCConfig()
 *   [_ZN4apac13BaseASCConfigD0Ev]
 *   data refs: 0x7c850, 0x42a948, 0x42afb8, 0x42cca0, 0x42ce00, 0x42d3f0, 0x42d480, 0x43e520
 */
void __fastcall apac::BaseASCConfig::~BaseASCConfig(apac__BaseASCConfig *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void (__fastcall ***v4)(apac__TCEConfig *); // x22
  void (__fastcall ***v5)(apac__TCEConfig *); // x20
  void (__fastcall *v6)(apac__TCEConfig *); // x1
  void (__fastcall ***v7)(apac__TCEConfig *); // x0
  __int64 v8; // [xsp+30h] [xbp+30h]
  __int64 v9; // [xsp+48h] [xbp+48h]
  __int64 v10; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 6);
  *(_QWORD *)this = off_42AFC8;
  if ( v3 )
    operator delete(v3);
  *(_QWORD *)this = &unk_42A958;
  v4 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  if ( v5 != v4 )
  {
    do
    {
      while ( 1 )
      {
        v6 = **v5;
        if ( v6 != apac::TCEConfig::~TCEConfig )
          break;
        v5 += 2;
        if ( v4 == v5 )
          goto LABEL_8;
      }
      v7 = v5;
      v5 += 2;
      v6((apac__TCEConfig *)v7);
    }
    while ( v4 != v5 );
LABEL_8:
    v4 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  }
  if ( v4 )
    operator delete(v4);
  if ( v9 != _stack_chk_guard )
    v8 = v1;
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00099050  size 256 bytes  64 insns
 *   apac::BaseASCConfig::~BaseASCConfig()
 *   apac::BaseASCConfig::~BaseASCConfig()
 *   [_ZN4apac13BaseASCConfigD2Ev]
 *   [_ZN4apac13BaseASCConfigD1Ev]
 *   data refs: 0x7c850, 0x42a948, 0x42afb8, 0x42cca0, 0x42ce00, 0x42d3f0, 0x42d480, 0x43e520
 */
// Alternative name is '_ZN4apac13BaseASCConfigD1Ev'
void __fastcall apac::BaseASCConfig::~BaseASCConfig(apac__BaseASCConfig *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void (__fastcall ***v4)(apac__TCEConfig *); // x22
  void (__fastcall ***v5)(apac__TCEConfig *); // x20
  void (__fastcall *v6)(apac__TCEConfig *); // x1
  void (__fastcall ***v7)(apac__TCEConfig *); // x0
  __int64 v8; // x0
  __int64 v9; // [xsp+30h] [xbp+30h]
  __int64 v10; // [xsp+48h] [xbp+48h]
  __int64 v11; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 6);
  *(_QWORD *)this = off_42AFC8;
  if ( v3 )
    operator delete(v3);
  *(_QWORD *)this = &unk_42A958;
  v4 = *((void (__fastcall ****)(apac__TCEConfig *))this + 3);
  v5 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  if ( v5 != v4 )
  {
    do
    {
      while ( 1 )
      {
        v6 = **v5;
        if ( v6 != apac::TCEConfig::~TCEConfig )
          break;
        v5 += 2;
        if ( v4 == v5 )
          goto LABEL_8;
      }
      v7 = v5;
      v5 += 2;
      v6((apac__TCEConfig *)v7);
    }
    while ( v4 != v5 );
LABEL_8:
    v4 = *((void (__fastcall ****)(apac__TCEConfig *))this + 2);
  }
  v8 = v10 ^ _stack_chk_guard;
  if ( v4 )
  {
    if ( !v8 )
    {
      operator delete(v4);
      return;
    }
LABEL_13:
    v9 = v1;
  }
  if ( v8 )
    goto LABEL_13;
}


