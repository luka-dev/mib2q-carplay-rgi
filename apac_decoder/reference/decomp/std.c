/* ============================================================================
 * std
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 33 function(s), 15152 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0007cd40    524  void std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::_M_realloc_insert<apac::TCEConfig const&>(__gnu_cxx::__normal_iterator<apac::TCEConfig*, std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> > >, apac::TCEConfig const&) */
/* 0x000802c0     64  std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace() */
/* 0x00080340     64  std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace() */
/* 0x00080380     64  std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_destroy() */
/* 0x000803c0    164  std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_get_deleter(std::type_info const&) */
/* 0x000884d0     68  std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_dispose() */
/* 0x00095cc0    432  void std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > >::_M_realloc_insert<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > >(__gnu_cxx::__normal_iterator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >*, std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > > >, std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >&&) */
/* 0x000962e0    440  void std::vector<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >, std::allocator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > > >::_M_realloc_insert<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >*, std::vector<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >, std::allocator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > > > >, std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >&&) */
/* 0x00099150    204  std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::~vector() */
/* 0x00099ba0    400  void std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::_M_realloc_insert<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >&&) */
/* 0x00099d30    132  void std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::emplace_back<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > >(std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >&&) */
/* 0x000a15c0    316  std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::reserve(unsigned long) */
/* 0x000a1700    476  std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::operator=(std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&) */
/* 0x000a22c0    540  void std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::_M_realloc_insert<SC_ComponentModifier const&>(__gnu_cxx::__normal_iterator<SC_ComponentModifier*, std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > >, SC_ComponentModifier const&) */
/* 0x000a24e0    500  std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::_M_default_append(unsigned long) */
/* 0x000a26e0    352  std::vector<SC_SceneProcessor::ComponentTransitionFlags, std::allocator<SC_SceneProcessor::ComponentTransitionFlags> >::_M_default_append(unsigned long) */
/* 0x000a2840    360  std::vector<SC_SceneProcessor::Ramp, std::allocator<SC_SceneProcessor::Ramp> >::_M_default_append(unsigned long) */
/* 0x000a94f0    524  void std::vector<SC_CompositionSelectionGroup::Presets, std::allocator<SC_CompositionSelectionGroup::Presets> >::_M_realloc_insert<SC_CompositionSelectionGroup::Presets const&>(__gnu_cxx::__normal_iterator<SC_CompositionSelectionGroup::Presets*, std::vector<SC_CompositionSelectionGroup::Presets, std::allocator<SC_CompositionSelectionGroup::Presets> > >, SC_CompositionSelectionGroup::Presets const&) */
/* 0x000a9700    748  std::vector<SC_NonLanguageItem, std::allocator<SC_NonLanguageItem> >::_M_default_append(unsigned long) */
/* 0x000a99f0    608  std::vector<SC_LanguageSpecificItem, std::allocator<SC_LanguageSpecificItem> >::_M_default_append(unsigned long) */
/* 0x000a9c50    768  std::vector<SC_LanguageSelectionSet, std::allocator<SC_LanguageSelectionSet> >::_M_default_append(unsigned long) */
/* 0x000a9f50    960  std::vector<SC_CompositionSelectionGroup, std::allocator<SC_CompositionSelectionGroup> >::_M_default_append(unsigned long) */
/* 0x000aa310   1040  std::vector<SC_PresetDescription, std::allocator<SC_PresetDescription> >::_M_default_append(unsigned long) */
/* 0x000aa720    824  std::vector<SC_SceneComposition::SC_Category, std::allocator<SC_SceneComposition::SC_Category> >::_M_default_append(unsigned long) */
/* 0x000aae00   1456  std::vector<SC_SceneComposition, std::allocator<SC_SceneComposition> >::_M_default_append(unsigned long) */
/* 0x000ae180    380  std::vector<float, apac::AlignedAllocator<float, 16ul> >::_M_default_append(unsigned long) */
/* 0x000ae300    404  void std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >::_M_realloc_insert<apac::ElementInstance<APACSyntacticElement*> >(__gnu_cxx::__normal_iterator<apac::ElementInstance<APACSyntacticElement*>*, std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > > >, apac::ElementInstance<APACSyntacticElement*>&&) */
/* 0x000ae4a0    136  void std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >::emplace_back<apac::ElementInstance<APACSyntacticElement*> >(apac::ElementInstance<APACSyntacticElement*>&&) */
/* 0x000af7e0    408  void std::vector<std::tuple<apac::AudioDataType, apac::ASCChannelRange>, std::allocator<std::tuple<apac::AudioDataType, apac::ASCChannelRange> > >::_M_realloc_insert<std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&>(__gnu_cxx::__normal_iterator<std::tuple<apac::AudioDataType, apac::ASCChannelRange>*, std::vector<std::tuple<apac::AudioDataType, apac::ASCChannelRange>, std::allocator<std::tuple<apac::AudioDataType, apac::ASCChannelRange> > > >, std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&) */
/* 0x000b1240    496  void std::vector<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >, std::allocator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > > >::_M_realloc_insert<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >*, std::vector<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >, std::allocator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > > > >, std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >&&) */
/* 0x000b30e0    716  void std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> >::_M_realloc_insert<apac::ASCAudioOutData>(__gnu_cxx::__normal_iterator<apac::ASCAudioOutData*, std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> > >, apac::ASCAudioOutData&&) */
/* 0x000b5fb0    416  void std::vector<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >, std::allocator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > > >::_M_realloc_insert<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >*, std::vector<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >, std::allocator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > > > >, std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >&&) */
/* 0x00106f50    168  std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector() */

/* ------------------------------------------------------------------------ */
/* 0x0007cd40  size 524 bytes  130 insns
 *   void std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::_M_realloc_insert<apac::TCEConfig const&>(__gnu_cxx::__normal_iterator<apac::TCEConfig*, std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> > >, apac::TCEConfig const&)
 *   [_ZNSt6vectorIN4apac9TCEConfigESaIS1_EE17_M_realloc_insertIJRKS1_EEEvN9__gnu_cxx17__normal_iteratorIPS1_S3_EEDpOT_]
 *   data refs: 0x7c850, 0x30e8b8, 0x42a920, 0x42ce00, 0x42d1a8, 0x42d480, 0x43e520
 */
__int64 __fastcall std::vector<apac::TCEConfig>::_M_realloc_insert<apac::TCEConfig const&>(
        std_vector *this,
        void *a,
        const apac__TCEConfig *a2)
{
  apac__TCEConfig *begin; // x24
  _BYTE *end; // x25
  __int64 v6; // x2
  unsigned __int64 v8; // x0
  char *v9; // x20
  bool v10; // cc
  unsigned __int64 v11; // x26
  unsigned __int64 v12; // x0
  char *v13; // x22
  char *v14; // x0
  char *v15; // x26
  int v16; // w2
  char *v17; // x21
  apac__TCEConfig *v18; // x20
  int v19; // w0
  void (__fastcall *v20)(apac__TCEConfig *); // x2
  apac__TCEConfig *v21; // x0
  void (__fastcall ***v22)(apac__TCEConfig *); // x19
  char *v23; // x20
  void (__fastcall **v24)(apac__TCEConfig *); // x0
  void (__fastcall *v25)(apac__TCEConfig *); // x2
  apac__TCEConfig *v26; // x0
  std_vector *v27; // x0
  char *v28; // x1
  char *v30; // [xsp+60h] [xbp+60h]
  std_vector *v31; // [xsp+68h] [xbp+68h]
  __int64 v32; // [xsp+78h] [xbp+78h]
  __int64 v33; // [xsp+80h] [xbp+80h]
  v31 = this;
  begin = (apac__TCEConfig *)this->begin;
  end = this->end;
  v6 = end - (char *)this->begin;
  if ( v6 >> 4 == 0x7FFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v8 = v6 >> 4;
  v9 = (char *)((_BYTE *)a - (_BYTE *)begin);
  if ( !(v6 >> 4) )
  {
    v11 = 16;
    goto LABEL_5;
  }
  v10 = v8 > 2 * v8;
  v11 = 0x7FFFFFFFFFFFFFF0LL;
  v12 = 2 * v8;
  if ( v10 )
  {
LABEL_5:
    v13 = (char *)operator new(v11);
    v14 = &v13[v11];
    v15 = v13 + 16;
    v30 = v14;
    goto LABEL_6;
  }
  if ( v12 )
  {
    if ( v12 > 0x7FFFFFFFFFFFFFFLL )
      v12 = 0x7FFFFFFFFFFFFFFLL;
    v11 = 16 * v12;
    goto LABEL_5;
  }
  v15 = (char *)&word_10;
  v13 = nullptr;
  v30 = nullptr;
LABEL_6:
  v16 = *((_DWORD *)a2 + 2);
  *(_QWORD *)&v9[(_QWORD)v13] = off_42A930;
  *(_DWORD *)&v9[(_QWORD)v13 + 8] = v16;
  if ( a != begin )
  {
    v17 = v13;
    v18 = begin;
    do
    {
      while ( 1 )
      {
        v19 = *((_DWORD *)v18 + 2);
        *(_QWORD *)v17 = off_42A930;
        *((_DWORD *)v17 + 2) = v19;
        v20 = **(void (__fastcall ***)(apac__TCEConfig *))v18;
        if ( v20 != apac::TCEConfig::~TCEConfig )
          break;
        v18 = (apac__TCEConfig *)((char *)v18 + 16);
        v17 += 16;
        if ( a == v18 )
          goto LABEL_11;
      }
      v21 = v18;
      v18 = (apac__TCEConfig *)((char *)v18 + 16);
      v20(v21);
      v17 += 16;
    }
    while ( a != v18 );
LABEL_11:
    v15 = &v13[(_BYTE *)a - (_BYTE *)begin + 16];
  }
  if ( a != end )
  {
    v22 = (void (__fastcall ***)(apac__TCEConfig *))a;
    v23 = v15;
    do
    {
      while ( 1 )
      {
        v24 = *v22;
        *(_QWORD *)v23 = off_42A930;
        *((_DWORD *)v23 + 2) = *((_DWORD *)v22 + 2);
        v25 = *v24;
        if ( *v24 != apac::TCEConfig::~TCEConfig )
          break;
        v22 += 2;
        v23 += 16;
        if ( v22 == (void (__fastcall ***)(apac__TCEConfig *))end )
          goto LABEL_17;
      }
      v26 = (apac__TCEConfig *)v22;
      v22 += 2;
      v25(v26);
      v23 += 16;
    }
    while ( v22 != (void (__fastcall ***)(apac__TCEConfig *))end );
LABEL_17:
    v15 += (char *)v22 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  v28 = v30;
  v27 = v31;
  v31->begin = v13;
  v27->end = v15;
  v27->cap = v28;
  return v32 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000802c0  size 64 bytes  16 insns
 *   std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()
 *   std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()
 *   [_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EED1Ev]
 *   [_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EED2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EED2Ev'
__int64 __fastcall std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace(
        std___Sp_counted_ptr_inplace *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  return v2 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00080340  size 64 bytes  16 insns
 *   std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace()
 *   [_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EED0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::~_Sp_counted_ptr_inplace(
        std___Sp_counted_ptr_inplace *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00080380  size 64 bytes  16 insns
 *   std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_destroy()
 *   [_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EE10_M_destroyEv]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::_M_destroy(
        std___Sp_counted_ptr_inplace *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x000803c0  size 164 bytes  41 insns
 *   std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_get_deleter(std::type_info const&)
 *   [_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EE14_M_get_deleterERKSt9type_info]
 *   data refs: 0x30eeb8, 0x30efd0, 0x42ce00, 0x42d428, 0x42daf8, 0x43e520
 */
__int64 __fastcall std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::_M_get_deleter(
        std___Sp_counted_ptr_inplace *this,
        const void *a)
{
  char *v3; // x0
  __int64 v4; // x21
  __int64 v6; // [xsp+40h] [xbp+40h]
  if ( a == &std::_Sp_make_shared_tag::_S_ti(void)::__tag )
    return (__int64)this + 16;
  v3 = *((char **)a + 1);
  if ( v3 == "St19_Sp_make_shared_tag" )
    return (__int64)this + 16;
  v4 = 0;
  if ( *v3 != 42 && !strcmp(v3, "St19_Sp_make_shared_tag") )
    return (__int64)this + 16;
  return v4;
}


/* ------------------------------------------------------------------------ */
/* 0x000884d0  size 68 bytes  17 insns
 *   std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload, std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>, (__gnu_cxx::_Lock_policy)2>::_M_dispose()
 *   [_ZNSt23_Sp_counted_ptr_inplaceIN4apac3drc18DynRangeCompressor23APACDRCExtensionPayloadESaIS3_ELN9__gnu_cxx12_Lock_policyE2EE10_M_disposeEv]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall std::_Sp_counted_ptr_inplace<apac::drc::DynRangeCompressor::APACDRCExtensionPayload,std::allocator<apac::drc::DynRangeCompressor::APACDRCExtensionPayload>,(__gnu_cxx::_Lock_policy)2>::_M_dispose(
        std___Sp_counted_ptr_inplace *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  apac::drc::DynRangeCompressor::APACDRCExtensionPayload::~APACDRCExtensionPayload((std___Sp_counted_ptr_inplace *)((char *)this + 16));
}


/* ------------------------------------------------------------------------ */
/* 0x00095cc0  size 432 bytes  107 insns
 *   void std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > >::_M_realloc_insert<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > >(__gnu_cxx::__normal_iterator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >*, std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > > >, std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >&&)
 *   [_ZNSt6vectorISt4pairIjSt10unique_ptrIN4apac10DecElementESt14default_deleteIS3_EEESaIS7_EE17_M_realloc_insertIJS7_EEEvN9__gnu_cxx17__normal_iteratorIPS7_S9_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<std::pair<unsigned int,std::unique_ptr<apac::DecElement>>>::_M_realloc_insert<std::pair<unsigned int,std::unique_ptr<apac::DecElement>>>(
        std_vector *this,
        void *a,
        void *a2)
{
  _BYTE *begin; // x23
  _BYTE *end; // x26
  __int64 v7; // x1
  unsigned __int64 v9; // x0
  char *v10; // x28
  bool v11; // cc
  unsigned __int64 v12; // x25
  unsigned __int64 v13; // x0
  char *v14; // x21
  char *v15; // x25
  char *v16; // x19
  char *v17; // x20
  _QWORD *v18; // x19
  __int64 v19; // x0
  __int64 v20; // x0
  _DWORD *v21; // x2
  char *v22; // x3
  int v23; // w1
  __int64 v24; // x0
  __int64 v26; // [xsp+68h] [xbp+68h]
  __int64 v27; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 4 == 0x7FFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 4;
  v10 = (char *)((_BYTE *)a - begin);
  if ( !(v7 >> 4) )
  {
    v12 = 16;
    goto LABEL_5;
  }
  v11 = v9 > 2 * v9;
  v12 = 0x7FFFFFFFFFFFFFF0LL;
  v13 = 2 * v9;
  if ( v11 )
  {
LABEL_5:
    v14 = (char *)operator new(v12);
    v15 = &v14[v12];
    v16 = v14 + 16;
    goto LABEL_6;
  }
  if ( v13 )
  {
    if ( v13 > 0x7FFFFFFFFFFFFFFLL )
      v13 = 0x7FFFFFFFFFFFFFFLL;
    v12 = 16 * v13;
    goto LABEL_5;
  }
  v16 = (char *)&word_10;
  v15 = nullptr;
  v14 = nullptr;
LABEL_6:
  *(_DWORD *)&v10[(_QWORD)v14] = *(_DWORD *)a2;
  *(_QWORD *)&v10[(_QWORD)v14 + 8] = *((_QWORD *)a2 + 1);
  *((_QWORD *)a2 + 1) = 0;
  if ( a != begin )
  {
    v17 = v14;
    v18 = begin;
    do
    {
      *(_DWORD *)v17 = *(_DWORD *)v18;
      v19 = v18[1];
      v18[1] = 0;
      *((_QWORD *)v17 + 1) = v19;
      v20 = v18[1];
      if ( v20 )
        (*(void (__fastcall **)(__int64))(*(_QWORD *)v20 + 8LL))(v20);
      v18 += 2;
      v17 += 16;
    }
    while ( a != v18 );
    v16 = &v14[(_BYTE *)a - begin + 16];
  }
  if ( a != end )
  {
    v21 = a;
    v22 = v16;
    do
    {
      v23 = *v21;
      v21 += 4;
      v24 = *((_QWORD *)v21 - 1);
      *(_DWORD *)v22 = v23;
      *((_QWORD *)v22 + 1) = v24;
      v22 += 16;
    }
    while ( v21 != (_DWORD *)end );
    v16 += (char *)v21 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v14;
  this->end = v16;
  this->cap = v15;
  return v26 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000962e0  size 440 bytes  109 insns
 *   void std::vector<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >, std::allocator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > > >::_M_realloc_insert<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >*, std::vector<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >, std::allocator<std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> > > > >, std::unique_ptr<apac::IExtFrameElementDecoder, std::default_delete<apac::IExtFrameElementDecoder> >&&)
 *   [_ZNSt6vectorISt10unique_ptrIN4apac23IExtFrameElementDecoderESt14default_deleteIS2_EESaIS5_EE17_M_realloc_insertIJS5_EEEvN9__gnu_cxx17__normal_iteratorIPS5_S7_EEDpOT_]
 *   data refs: 0x955a0, 0x30e8b8, 0x42ce00, 0x42d888, 0x43e520
 */
__int64 __fastcall std::vector<std::unique_ptr<apac::IExtFrameElementDecoder>>::_M_realloc_insert<std::unique_ptr<apac::IExtFrameElementDecoder>>(
        std_vector *this,
        void *a,
        void **a2)
{
  void **begin; // x23
  _BYTE *end; // x26
  __int64 v7; // x1
  unsigned __int64 v9; // x0
  bool v10; // cc
  unsigned __int64 v11; // x25
  unsigned __int64 v12; // x0
  char *v13; // x22
  char *v14; // x25
  char *v15; // x19
  _QWORD *v16; // x20
  void **v17; // x19
  void *v18; // x0
  void *v19; // x0
  void (__fastcall *v20)(apac__ExtFrameUnkownDecoder *); // x1
  char *v21; // x2
  char *v22; // x0
  __int64 v23; // t1
  __int64 v25; // [xsp+68h] [xbp+68h]
  __int64 v26; // [xsp+70h] [xbp+70h]
  begin = (void **)this->begin;
  end = this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 3 == 0xFFFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 3;
  if ( v7 >> 3 )
  {
    v10 = v9 > 2 * v9;
    v11 = 0x7FFFFFFFFFFFFFF8LL;
    v12 = 2 * v9;
    if ( !v10 )
    {
      if ( !v12 )
      {
        v15 = byte_8;
        v14 = nullptr;
        v13 = nullptr;
        goto LABEL_6;
      }
      if ( v12 > 0xFFFFFFFFFFFFFFFLL )
        v12 = 0xFFFFFFFFFFFFFFFLL;
      v11 = 8 * v12;
    }
  }
  else
  {
    v11 = 8;
  }
  v13 = (char *)operator new(v11);
  v14 = &v13[v11];
  v15 = v13 + 8;
LABEL_6:
  *(_QWORD *)&v13[(_BYTE *)a - (_BYTE *)begin] = *a2;
  *a2 = nullptr;
  if ( a == begin )
    goto LABEL_14;
  v16 = v13;
  v17 = begin;
  do
  {
    while ( 1 )
    {
      v18 = *v17;
      *v17 = nullptr;
      *v16 = v18;
      v19 = *v17;
      if ( !*v17 )
        goto LABEL_9;
      v20 = *(void (__fastcall **)(apac__ExtFrameUnkownDecoder *))(*(_QWORD *)v19 + 8LL);
      if ( v20 != apac::ExtFrameUnkownDecoder::~ExtFrameUnkownDecoder )
        break;
      operator delete(v19);
LABEL_9:
      ++v17;
      ++v16;
      if ( a == v17 )
        goto LABEL_13;
    }
    ++v17;
    ((void (*)(void))v20)();
    ++v16;
  }
  while ( a != v17 );
LABEL_13:
  v15 = &v13[(_BYTE *)a - (_BYTE *)begin + 8];
LABEL_14:
  if ( a != end )
  {
    v21 = (char *)a;
    v22 = v15;
    do
    {
      v23 = *(_QWORD *)v21;
      v21 += 8;
      *(_QWORD *)v22 = v23;
      v22 += 8;
    }
    while ( v21 != end );
    v15 += v21 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v13;
  this->end = v15;
  this->cap = v14;
  return v25 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00099150  size 204 bytes  51 insns
 *   std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::~vector()
 *   std::vector<apac::TCEConfig, std::allocator<apac::TCEConfig> >::~vector()
 *   [_ZNSt6vectorIN4apac9TCEConfigESaIS1_EED1Ev]
 *   [_ZNSt6vectorIN4apac9TCEConfigESaIS1_EED2Ev]
 *   data refs: 0x7c850, 0x42ce00, 0x42d480, 0x43e520
 */
// Alternative name is '_ZNSt6vectorIN4apac9TCEConfigESaIS1_EED2Ev'
void __fastcall std::vector<apac::TCEConfig>::~vector(std_vector *this)
{
  void (__fastcall ***begin)(apac__TCEConfig *); // x19
  void (__fastcall ***end)(apac__TCEConfig *); // x20
  void (__fastcall *v4)(apac__TCEConfig *); // x1
  void (__fastcall ***v5)(apac__TCEConfig *); // x0
  __int64 v6; // [xsp+50h] [xbp+50h]
  begin = (void (__fastcall ***)(apac__TCEConfig *))this->begin;
  end = (void (__fastcall ***)(apac__TCEConfig *))this->end;
  if ( this->begin != end )
  {
    do
    {
      while ( 1 )
      {
        v4 = **begin;
        if ( v4 != apac::TCEConfig::~TCEConfig )
          break;
        begin += 2;
        if ( end == begin )
          goto LABEL_6;
      }
      v5 = begin;
      begin += 2;
      v4((apac__TCEConfig *)v5);
    }
    while ( end != begin );
LABEL_6:
    end = (void (__fastcall ***)(apac__TCEConfig *))this->begin;
  }
  if ( end )
    operator delete(end);
}


/* ------------------------------------------------------------------------ */
/* 0x00099ba0  size 400 bytes  99 insns
 *   void std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::_M_realloc_insert<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >&&)
 *   [_ZNSt6vectorISt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS2_EESaIS5_EE17_M_realloc_insertIJS5_EEEvN9__gnu_cxx17__normal_iteratorIPS5_S7_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<std::unique_ptr<apac::IASCConfig>>::_M_realloc_insert<std::unique_ptr<apac::IASCConfig>>(
        std_vector *this,
        void *a,
        void **a2)
{
  _BYTE *begin; // x23
  _BYTE *end; // x26
  __int64 v7; // x1
  unsigned __int64 v9; // x0
  bool v10; // cc
  unsigned __int64 v11; // x25
  unsigned __int64 v12; // x0
  char *v13; // x22
  char *v14; // x25
  char *v15; // x19
  _QWORD *v16; // x20
  _QWORD *v17; // x19
  __int64 v18; // x0
  char *v19; // x2
  char *v20; // x0
  __int64 v21; // t1
  __int64 v23; // [xsp+68h] [xbp+68h]
  __int64 v24; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 3 == 0xFFFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 3;
  if ( !(v7 >> 3) )
  {
    v11 = 8;
    goto LABEL_5;
  }
  v10 = v9 > 2 * v9;
  v11 = 0x7FFFFFFFFFFFFFF8LL;
  v12 = 2 * v9;
  if ( v10 )
  {
LABEL_5:
    v13 = (char *)operator new(v11);
    v14 = &v13[v11];
    v15 = v13 + 8;
    goto LABEL_6;
  }
  if ( v12 )
  {
    if ( v12 > 0xFFFFFFFFFFFFFFFLL )
      v12 = 0xFFFFFFFFFFFFFFFLL;
    v11 = 8 * v12;
    goto LABEL_5;
  }
  v15 = byte_8;
  v14 = nullptr;
  v13 = nullptr;
LABEL_6:
  *(_QWORD *)&v13[(_BYTE *)a - begin] = *a2;
  *a2 = nullptr;
  if ( a != begin )
  {
    v16 = v13;
    v17 = begin;
    do
    {
      v18 = *v17;
      *v17 = 0;
      *v16 = v18;
      if ( *v17 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v17 + 8LL))(*v17);
      ++v17;
      ++v16;
    }
    while ( a != v17 );
    v15 = &v13[(_BYTE *)a - begin + 8];
  }
  if ( a != end )
  {
    v19 = (char *)a;
    v20 = v15;
    do
    {
      v21 = *(_QWORD *)v19;
      v19 += 8;
      *(_QWORD *)v20 = v21;
      v20 += 8;
    }
    while ( v19 != end );
    v15 += v19 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v13;
  this->end = v15;
  this->cap = v14;
  return v23 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00099d30  size 132 bytes  33 insns
 *   void std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::emplace_back<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > >(std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >&&)
 *   [_ZNSt6vectorISt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS2_EESaIS5_EE12emplace_backIJS5_EEEvDpOT_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<std::unique_ptr<apac::IASCConfig>>::emplace_back<std::unique_ptr<apac::IASCConfig>>(
        std_vector *this,
        void **a)
{
  _QWORD *end; // x1
  void *v5; // x0
  __int64 result; // x0
  __int64 v7; // [xsp+20h] [xbp+20h]
  end = this->end;
  if ( end == this->cap )
    return std::vector<std::unique_ptr<apac::IASCConfig>>::_M_realloc_insert<std::unique_ptr<apac::IASCConfig>>(
             this,
             end,
             a);
  v5 = *a;
  *a = nullptr;
  *end = v5;
  result = (__int64)this->end + 8;
  this->end = (void *)result;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a15c0  size 316 bytes  74 insns
 *   std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::reserve(unsigned long)
 *   [_ZNSt6vectorI20SC_ComponentModifierSaIS0_EE7reserveEm]
 *   data refs: 0x330650, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_ComponentModifier>::reserve(std_vector *this, unsigned __int64 a)
{
  unsigned __int64 v2; // x23
  _BYTE *begin; // x7
  __int64 result; // x0
  _QWORD *v6; // x22
  _BYTE *end; // x6
  signed __int64 v8; // x20
  _QWORD *v9; // x0
  _QWORD *v10; // x2
  _QWORD *v11; // x1
  __int64 v12; // x5
  __int64 v13; // x5
  unsigned __int64 v14; // [xsp+30h] [xbp+30h]
  __int64 v15; // [xsp+48h] [xbp+48h]
  __int64 v16; // [xsp+50h] [xbp+50h]
  if ( a > 0x38E38E38E38E38ELL )
  {
    v14 = v2;
    std::__throw_length_error("vector::reserve");
  }
  begin = this->begin;
  if ( a > 0x8E38E38E38E38E39LL * (((char *)this->cap - (char *)this->begin) >> 2) )
  {
    v6 = nullptr;
    end = this->end;
    v2 = 36 * a;
    v8 = end - begin;
    if ( a )
    {
      v9 = (_QWORD *)operator new(36 * a);
      begin = this->begin;
      end = this->end;
      v6 = v9;
    }
    v10 = v6;
    v11 = begin;
    if ( begin != end )
    {
      do
      {
        v12 = v11[1];
        *v10 = *v11;
        v10[1] = v12;
        v13 = v11[3];
        v10[2] = v11[2];
        v10[3] = v13;
        v11 = (_QWORD *)((char *)v11 + 36);
        v10 = (_QWORD *)((char *)v10 + 36);
        *((_DWORD *)v10 - 1) = *((_DWORD *)v11 - 1);
      }
      while ( v11 != (_QWORD *)end );
    }
    if ( begin )
      operator delete(begin);
    this->begin = v6;
    this->end = (char *)v6 + v8;
    this->cap = (char *)v6 + v2;
  }
  result = v15 ^ _stack_chk_guard;
  if ( v15 != _stack_chk_guard )
    v14 = v2;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a1700  size 476 bytes  115 insns
 *   std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::operator=(std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > const&)
 *   [_ZNSt6vectorI20SC_ComponentModifierSaIS0_EEaSERKS2_]
 *   data refs: 0x42ce00, 0x43e520
 */
char **__fastcall std::vector<SC_ComponentModifier>::operator=(std_vector *this, const std_vector *vec)
{
  char *begin; // x19
  char *end; // x21
  char *v6; // x0
  signed __int64 v7; // x24
  unsigned __int64 v8; // x4
  _QWORD *v9; // x3
  size_t v10; // x2
  char *v11; // x1
  _QWORD *v13; // x22
  char *v14; // x1
  _QWORD *i; // x2
  __int64 v16; // x5
  __int64 v17; // x5
  char *v18; // x1
  __int64 v19; // x5
  __int64 v20; // x5
  __int64 v21; // [xsp+50h] [xbp+50h]
  if ( vec != this )
  {
    begin = (char *)vec->begin;
    end = (char *)vec->end;
    v6 = (char *)this->begin;
    v7 = end - (char *)vec->begin;
    v8 = 0x8E38E38E38E38E39LL * (v7 >> 2);
    if ( v8 > 0x8E38E38E38E38E39LL * (((char *)this->cap - (char *)this->begin) >> 2) )
    {
      v13 = nullptr;
      if ( v8 )
      {
        if ( v8 > 0x38E38E38E38E38ELL )
          std::__throw_bad_alloc();
        v13 = (_QWORD *)operator new(v7);
        v6 = (char *)this->begin;
      }
      v14 = begin;
      for ( i = v13; end != v14; *((_DWORD *)i - 1) = *((_DWORD *)v14 - 1) )
      {
        v16 = *((_QWORD *)v14 + 1);
        *i = *(_QWORD *)v14;
        i[1] = v16;
        v17 = *((_QWORD *)v14 + 3);
        i[2] = *((_QWORD *)v14 + 2);
        i[3] = v17;
        v14 += 36;
        i = (_QWORD *)((char *)i + 36);
      }
      if ( v6 )
        operator delete(v6);
      this->begin = v13;
      this->end = (char *)v13 + v7;
      this->cap = (char *)v13 + v7;
    }
    else
    {
      v9 = this->end;
      v10 = (char *)v9 - v6;
      if ( v8 > 0x8E38E38E38E38E39LL * (((char *)v9 - v6) >> 2) )
      {
        if ( v10 )
        {
          memmove(v6, vec->begin, v10);
          v6 = (char *)this->begin;
          v9 = this->end;
          begin = (char *)vec->begin;
          end = (char *)vec->end;
          v10 = (char *)v9 - (char *)this->begin;
        }
        v18 = &begin[v10];
        if ( &begin[v10] == end )
        {
          this->end = &v6[v7];
        }
        else
        {
          do
          {
            v19 = *((_QWORD *)v18 + 1);
            *v9 = *(_QWORD *)v18;
            v9[1] = v19;
            v20 = *((_QWORD *)v18 + 3);
            v9[2] = *((_QWORD *)v18 + 2);
            v9[3] = v20;
            v18 += 36;
            v9 = (_QWORD *)((char *)v9 + 36);
            *((_DWORD *)v9 - 1) = *((_DWORD *)v18 - 1);
          }
          while ( v18 != end );
          this->end = (char *)this->begin + v7;
        }
      }
      else
      {
        v11 = &v6[v7];
        if ( begin != end )
        {
          memmove(v6, vec->begin, end - (char *)vec->begin);
          v11 = (char *)this->begin + v7;
        }
        this->end = v11;
      }
    }
  }
  return (char **)this;
}


/* ------------------------------------------------------------------------ */
/* 0x000a22c0  size 540 bytes  130 insns
 *   void std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::_M_realloc_insert<SC_ComponentModifier const&>(__gnu_cxx::__normal_iterator<SC_ComponentModifier*, std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> > >, SC_ComponentModifier const&)
 *   [_ZNSt6vectorI20SC_ComponentModifierSaIS0_EE17_M_realloc_insertIJRKS0_EEEvN9__gnu_cxx17__normal_iteratorIPS0_S2_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_ComponentModifier>::_M_realloc_insert<SC_ComponentModifier const&>(
        std_vector *this,
        void *a,
        const SC_ComponentModifier *a2)
{
  _BYTE *begin; // x23
  char *end; // x25
  unsigned __int64 v7; // x0
  bool v9; // cc
  unsigned __int64 v10; // x24
  unsigned __int64 v11; // x0
  _QWORD *v12; // x21
  char *v13; // x24
  char *v14; // x19
  char *v15; // x2
  int v16; // w3
  __int64 v17; // x5
  __int64 v18; // x0
  __int64 v19; // x1
  _QWORD *v20; // x4
  _QWORD *v21; // x3
  __int64 v22; // x7
  __int64 v23; // x7
  char *v24; // x2
  char *v25; // x3
  __int64 v26; // x5
  int v27; // w4
  __int64 v28; // x7
  __int64 v30; // [xsp+68h] [xbp+68h]
  __int64 v31; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = (char *)this->end;
  v7 = 0x8E38E38E38E38E39LL * ((end - (char *)this->begin) >> 2);
  if ( v7 == 0x38E38E38E38E38ELL )
    std::__throw_length_error("vector::_M_realloc_insert");
  if ( !v7 )
  {
    v10 = 36;
    goto LABEL_5;
  }
  v9 = v7 > 2 * v7;
  v10 = 0x7FFFFFFFFFFFFFF8LL;
  v11 = 2 * v7;
  if ( v9 )
  {
LABEL_5:
    v12 = (_QWORD *)operator new(v10);
    v13 = (char *)v12 + v10;
    v14 = (char *)v12 + 36;
    goto LABEL_6;
  }
  if ( v11 )
  {
    if ( v11 > 0x38E38E38E38E38ELL )
      v11 = 0x38E38E38E38E38ELL;
    v10 = 36 * v11;
    goto LABEL_5;
  }
  v14 = (_BYTE *)(&qword_20 + 4);
  v13 = nullptr;
  v12 = nullptr;
LABEL_6:
  v15 = (char *)v12 + (_BYTE *)a - begin;
  v16 = *((_DWORD *)a2 + 8);
  v17 = *((_QWORD *)a2 + 1);
  v18 = *((_QWORD *)a2 + 2);
  v19 = *((_QWORD *)a2 + 3);
  *(_QWORD *)v15 = *(_QWORD *)a2;
  *((_QWORD *)v15 + 1) = v17;
  *((_QWORD *)v15 + 2) = v18;
  *((_QWORD *)v15 + 3) = v19;
  *((_DWORD *)v15 + 8) = v16;
  if ( a != begin )
  {
    v20 = v12;
    v21 = begin;
    do
    {
      v22 = v21[1];
      *v20 = *v21;
      v20[1] = v22;
      v23 = v21[3];
      v20[2] = v21[2];
      v20[3] = v23;
      v21 = (_QWORD *)((char *)v21 + 36);
      v20 = (_QWORD *)((char *)v20 + 36);
      *((_DWORD *)v20 - 1) = *((_DWORD *)v21 - 1);
    }
    while ( a != v21 );
    v14 = (char *)v12
        + 36 * ((0xE38E38E38E38E39LL * ((unsigned __int64)((char *)a - 36 - begin) >> 2)) & 0x3FFFFFFFFFFFFFFFLL)
        + 72;
  }
  if ( a != end )
  {
    v24 = (char *)a;
    v25 = v14;
    do
    {
      v26 = *((_QWORD *)v24 + 1);
      *(_QWORD *)v25 = *(_QWORD *)v24;
      *((_QWORD *)v25 + 1) = v26;
      v27 = *((_DWORD *)v24 + 8);
      v28 = *((_QWORD *)v24 + 3);
      *((_QWORD *)v25 + 2) = *((_QWORD *)v24 + 2);
      *((_QWORD *)v25 + 3) = v28;
      v24 += 36;
      *((_DWORD *)v25 + 8) = v27;
      v25 += 36;
    }
    while ( v24 != end );
    v14 += 36 * ((0xE38E38E38E38E39LL * ((unsigned __int64)(v24 - (_BYTE *)a - 36) >> 2)) & 0x3FFFFFFFFFFFFFFFLL) + 36;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v12;
  this->end = v14;
  this->cap = v13;
  return v30 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000a24e0  size 500 bytes  122 insns
 *   std::vector<SC_ComponentModifier, std::allocator<SC_ComponentModifier> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI20SC_ComponentModifierSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_ComponentModifier>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  _BYTE *end; // x4
  __int64 v8; // x19
  unsigned __int64 v9; // x21
  _BYTE *v10; // x2
  unsigned __int64 v11; // x3
  __int64 result; // x0
  unsigned __int64 v13; // x0
  unsigned __int64 v14; // x0
  __int64 v15; // x24
  _QWORD *v16; // x0
  unsigned __int64 v17; // x1
  _QWORD *v18; // x25
  _BYTE *v19; // x2
  void *begin; // x0
  _QWORD *v21; // x6
  _QWORD *v22; // x3
  _QWORD *v23; // x2
  __int64 v24; // x5
  __int64 v25; // x5
  __int64 v26; // [xsp+10h] [xbp+10h]
  __int64 v27; // [xsp+18h] [xbp+18h]
  __int64 v28; // [xsp+40h] [xbp+40h]
  __int64 v29; // [xsp+58h] [xbp+58h]
  __int64 v30; // [xsp+60h] [xbp+60h]
  if ( a )
  {
    end = this->end;
    v8 = end - (char *)this->begin;
    v9 = 0x8E38E38E38E38E39LL * (v8 >> 2);
    if ( a > 0x8E38E38E38E38E39LL * (((char *)this->cap - (char *)end) >> 2) )
    {
      v28 = v4;
      if ( 0x38E38E38E38E38ELL - v9 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v9 )
        v13 = 0x8E38E38E38E38E39LL * (v8 >> 2);
      else
        v13 = a;
      v14 = v9 + v13;
      if ( v14 > 0x38E38E38E38E38ELL )
        v14 = 0x38E38E38E38E38ELL;
      v15 = 36 * v14;
      v16 = (_QWORD *)operator new(36 * v14);
      v17 = a;
      v18 = v16;
      v19 = (char *)v16 + v8;
      do
      {
        *v19 = 0;
        --v17;
        v19[1] = 0;
        v19 += 36;
        *((_DWORD *)v19 - 8) = 0;
        *(v19 - 28) = 0;
        *((_DWORD *)v19 - 6) = 0;
        *(v19 - 20) = 0;
        *((_DWORD *)v19 - 4) = 0;
        *(v19 - 12) = 0;
        *((_DWORD *)v19 - 2) = 0;
        *(v19 - 4) = 0;
        *(v19 - 3) = 0;
      }
      while ( v17 );
      begin = this->begin;
      v21 = this->end;
      v22 = v18;
      v23 = this->begin;
      if ( this->begin != v21 )
      {
        do
        {
          v24 = v23[1];
          *v22 = *v23;
          v22[1] = v24;
          v25 = v23[3];
          v22[2] = v23[2];
          v22[3] = v25;
          v23 = (_QWORD *)((char *)v23 + 36);
          v22 = (_QWORD *)((char *)v22 + 36);
          *((_DWORD *)v22 - 1) = *((_DWORD *)v23 - 1);
        }
        while ( v21 != v23 );
      }
      if ( begin )
        operator delete(begin);
      this->cap = (char *)v18 + v15;
      this->begin = v18;
      this->end = (char *)v18 + 36 * (a - 0x71C71C71C71C71C7LL * (v8 >> 2));
      v2 = v26;
      v3 = v27;
      v4 = v28;
    }
    else
    {
      v10 = this->end;
      v11 = a;
      do
      {
        *v10 = 0;
        --v11;
        v10[1] = 0;
        v10 += 36;
        *((_DWORD *)v10 - 8) = 0;
        *(v10 - 28) = 0;
        *((_DWORD *)v10 - 6) = 0;
        *(v10 - 20) = 0;
        *((_DWORD *)v10 - 4) = 0;
        *(v10 - 12) = 0;
        *((_DWORD *)v10 - 2) = 0;
        *(v10 - 4) = 0;
        *(v10 - 3) = 0;
      }
      while ( v11 );
      this->end = &end[36 * a];
      v2 = v26;
      v3 = v27;
    }
  }
  result = v29 ^ _stack_chk_guard;
  if ( v29 != _stack_chk_guard )
  {
    v26 = v2;
    v27 = v3;
    v28 = v4;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a26e0  size 352 bytes  87 insns
 *   std::vector<SC_SceneProcessor::ComponentTransitionFlags, std::allocator<SC_SceneProcessor::ComponentTransitionFlags> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorIN17SC_SceneProcessor24ComponentTransitionFlagsESaIS1_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_SceneProcessor::ComponentTransitionFlags>::_M_default_append(
        std_vector *this,
        unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x21
  __int64 v5; // x22
  __int64 v6; // x25
  _BYTE *end; // x3
  __int64 v10; // x20
  unsigned __int64 v11; // x25
  _WORD *v12; // x1
  unsigned __int64 v13; // x2
  __int64 result; // x0
  unsigned __int64 v15; // x22
  unsigned __int64 v16; // x22
  unsigned __int64 v17; // x22
  char *v18; // x0
  unsigned __int64 v19; // x2
  char *v20; // x21
  char *v21; // x1
  char *begin; // x0
  char *v23; // x1
  unsigned __int64 v24; // x4
  bool v25; // zf
  __int64 v26; // [xsp+10h] [xbp+10h]
  __int64 v27; // [xsp+18h] [xbp+18h]
  __int64 v28; // [xsp+20h] [xbp+20h]
  __int64 v29; // [xsp+28h] [xbp+28h]
  __int64 v30; // [xsp+40h] [xbp+40h]
  __int64 v31; // [xsp+58h] [xbp+58h]
  __int64 v32; // [xsp+60h] [xbp+60h]
  if ( !a )
    goto LABEL_6;
  end = this->end;
  v10 = end - (char *)this->begin;
  v11 = v10 >> 1;
  if ( a <= ((char *)this->cap - (char *)end) >> 1 )
  {
    v12 = this->end;
    v13 = a;
    do
    {
      *v12++ = 0;
      --v13;
    }
    while ( v13 );
    this->end = &end[2 * a];
    v2 = v26;
    v3 = v27;
    v6 = v30;
    goto LABEL_6;
  }
  v28 = v4;
  v29 = v5;
  if ( 0x3FFFFFFFFFFFFFFFLL - (v10 >> 1) < a )
    std::__throw_length_error("vector::_M_default_append");
  if ( a < v11 )
    v15 = v10 >> 1;
  else
    v15 = a;
  v16 = v11 + v15;
  if ( v16 > 0x3FFFFFFFFFFFFFFFLL )
    v16 = 0x3FFFFFFFFFFFFFFFLL;
  v17 = 2 * v16;
  v18 = (char *)operator new(v17);
  v19 = a;
  v20 = v18;
  v21 = &v18[v10];
  do
  {
    *(_WORD *)v21 = 0;
    v21 += 2;
    --v19;
  }
  while ( v19 );
  begin = (char *)this->begin;
  v23 = (char *)this->end;
  v24 = v23 - 2 - (char *)this->begin;
  if ( this->begin == v23 )
  {
    if ( !begin )
      goto LABEL_20;
  }
  else
  {
    do
    {
      v25 = v19 == v24;
      *(_WORD *)&v20[v19] = *(_WORD *)&begin[v19];
      v19 += 2LL;
    }
    while ( !v25 );
  }
  operator delete(begin);
LABEL_20:
  this->cap = &v20[v17];
  this->begin = v20;
  this->end = &v20[2 * a + 2 * v11];
  v2 = v26;
  v3 = v27;
  v4 = v28;
  v5 = v29;
  v6 = v30;
LABEL_6:
  result = v31 ^ _stack_chk_guard;
  if ( v31 != _stack_chk_guard )
  {
    v26 = v2;
    v27 = v3;
    v28 = v4;
    v29 = v5;
    v30 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a2840  size 360 bytes  89 insns
 *   std::vector<SC_SceneProcessor::Ramp, std::allocator<SC_SceneProcessor::Ramp> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorIN17SC_SceneProcessor4RampESaIS1_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_SceneProcessor::Ramp>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x23
  __int64 v5; // x24
  __int64 v6; // x25
  _BYTE *end; // x1
  __int64 v10; // x19
  unsigned __int64 v11; // x24
  _DWORD *v12; // x2
  unsigned __int64 v13; // x3
  __int64 result; // x0
  unsigned __int64 v15; // x23
  unsigned __int64 v16; // x23
  unsigned __int64 v17; // x23
  char *v18; // x0
  unsigned __int64 v19; // x1
  char *v20; // x25
  char *v21; // x2
  void *begin; // x19
  __int64 v23; // [xsp+10h] [xbp+10h]
  __int64 v24; // [xsp+18h] [xbp+18h]
  __int64 v25; // [xsp+30h] [xbp+30h]
  __int64 v26; // [xsp+38h] [xbp+38h]
  __int64 v27; // [xsp+40h] [xbp+40h]
  __int64 v28; // [xsp+58h] [xbp+58h]
  __int64 v29; // [xsp+60h] [xbp+60h]
  if ( !a )
    goto LABEL_6;
  end = this->end;
  v10 = end - (char *)this->begin;
  v11 = v10 >> 3;
  if ( a <= ((char *)this->cap - (char *)end) >> 3 )
  {
    v12 = this->end;
    v13 = a;
    do
    {
      *v12 = 0;
      --v13;
      v12[1] = 0;
      v12 += 2;
    }
    while ( v13 );
    this->end = &end[8 * a];
    v2 = v23;
    v3 = v24;
    v4 = v25;
    v5 = v26;
    goto LABEL_6;
  }
  v27 = v6;
  if ( 0xFFFFFFFFFFFFFFFLL - (v10 >> 3) < a )
    std::__throw_length_error("vector::_M_default_append");
  if ( a < v11 )
    v15 = v10 >> 3;
  else
    v15 = a;
  v16 = v11 + v15;
  if ( v16 > 0xFFFFFFFFFFFFFFFLL )
    v16 = 0xFFFFFFFFFFFFFFFLL;
  v17 = 8 * v16;
  v18 = (char *)operator new(v17);
  v19 = a;
  v20 = v18;
  v21 = &v18[v10];
  do
  {
    *(_DWORD *)v21 = 0;
    --v19;
    *((_DWORD *)v21 + 1) = 0;
    v21 += 8;
  }
  while ( v19 );
  begin = this->begin;
  if ( (char *)this->end - (char *)this->begin > 0 )
  {
    memmove(v18, this->begin, (char *)this->end - (char *)this->begin);
    goto LABEL_20;
  }
  if ( begin )
LABEL_20:
    operator delete(begin);
  this->cap = &v20[v17];
  this->begin = v20;
  this->end = &v20[8 * a + 8 * v11];
  v2 = v23;
  v3 = v24;
  v4 = v25;
  v5 = v26;
  v6 = v27;
LABEL_6:
  result = v28 ^ _stack_chk_guard;
  if ( v28 != _stack_chk_guard )
  {
    v23 = v2;
    v24 = v3;
    v25 = v4;
    v26 = v5;
    v27 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a94f0  size 524 bytes  127 insns
 *   void std::vector<SC_CompositionSelectionGroup::Presets, std::allocator<SC_CompositionSelectionGroup::Presets> >::_M_realloc_insert<SC_CompositionSelectionGroup::Presets const&>(__gnu_cxx::__normal_iterator<SC_CompositionSelectionGroup::Presets*, std::vector<SC_CompositionSelectionGroup::Presets, std::allocator<SC_CompositionSelectionGroup::Presets> > >, SC_CompositionSelectionGroup::Presets const&)
 *   [_ZNSt6vectorIN28SC_CompositionSelectionGroup7PresetsESaIS1_EE17_M_realloc_insertIJRKS1_EEEvN9__gnu_cxx17__normal_iteratorIPS1_S3_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_CompositionSelectionGroup::Presets>::_M_realloc_insert<SC_CompositionSelectionGroup::Presets const&>(
        std_vector *this,
        void *a,
        const SC_CompositionSelectionGroup__Presets *a2)
{
  _BYTE *begin; // x22
  char *end; // x25
  unsigned __int64 v7; // x0
  bool v9; // cc
  unsigned __int64 v10; // x23
  unsigned __int64 v11; // x0
  _QWORD *v12; // x20
  char *v13; // x23
  char *v14; // x24
  _QWORD *v15; // x2
  __int64 v16; // x1
  __int64 v17; // x5
  __int64 v18; // x1
  _QWORD *v19; // x4
  _QWORD *v20; // x3
  __int64 v21; // x7
  __int64 v22; // x7
  __int64 v23; // x7
  char *v24; // x2
  _QWORD *v25; // x3
  __int64 v26; // x5
  __int64 v27; // x5
  __int64 v28; // x5
  __int64 v30; // [xsp+68h] [xbp+68h]
  __int64 v31; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = (char *)this->end;
  v7 = 0xAAAAAAAAAAAAAAABLL * ((end - (char *)this->begin) >> 4);
  if ( v7 == 0x2AAAAAAAAAAAAAALL )
    std::__throw_length_error("vector::_M_realloc_insert");
  if ( !v7 )
  {
    v10 = 6;
    goto LABEL_5;
  }
  v9 = v7 > 2 * v7;
  v10 = 0xFFFFFFFFFFFFFFCLL;
  v11 = 2 * v7;
  if ( v9 )
  {
LABEL_5:
    v12 = (_QWORD *)operator new(v10 * 8);
    v13 = (char *)&v12[v10];
    v14 = (char *)(v12 + 6);
    goto LABEL_6;
  }
  if ( v11 )
  {
    if ( v11 > 0x2AAAAAAAAAAAAAALL )
      v11 = 0x2AAAAAAAAAAAAAALL;
    v10 = 6 * v11;
    goto LABEL_5;
  }
  v14 = (char *)&dword_30;
  v13 = nullptr;
  v12 = nullptr;
LABEL_6:
  v15 = (_QWORD *)((char *)v12 + (_BYTE *)a - begin);
  v16 = *((_QWORD *)a2 + 1);
  *v15 = *(_QWORD *)a2;
  v15[1] = v16;
  v17 = *((_QWORD *)a2 + 3);
  v15[2] = *((_QWORD *)a2 + 2);
  v15[3] = v17;
  v18 = *((_QWORD *)a2 + 5);
  v15[4] = *((_QWORD *)a2 + 4);
  v15[5] = v18;
  if ( a != begin )
  {
    v19 = v12;
    v20 = begin;
    do
    {
      v21 = v20[1];
      *v19 = *v20;
      v19[1] = v21;
      v20 += 6;
      v22 = *(v20 - 3);
      v19[2] = *(v20 - 4);
      v19[3] = v22;
      v19 += 6;
      v23 = *(v20 - 1);
      *(v19 - 2) = *(v20 - 2);
      *(v19 - 1) = v23;
    }
    while ( a != v20 );
    v14 = (char *)&v12[6
                     * ((0xAAAAAAAAAAAAAABLL * ((unsigned __int64)((char *)a - 48 - begin) >> 4)) & 0xFFFFFFFFFFFFFFFLL)
                     + 12];
  }
  if ( a != end )
  {
    v24 = (char *)a;
    v25 = v14;
    do
    {
      v26 = *((_QWORD *)v24 + 1);
      *v25 = *(_QWORD *)v24;
      v25[1] = v26;
      v24 += 48;
      v27 = *((_QWORD *)v24 - 3);
      v25[2] = *((_QWORD *)v24 - 4);
      v25[3] = v27;
      v25 += 6;
      v28 = *((_QWORD *)v24 - 1);
      *(v25 - 2) = *((_QWORD *)v24 - 2);
      *(v25 - 1) = v28;
    }
    while ( v24 != end );
    v14 += 48 * ((0xAAAAAAAAAAAAAABLL * ((unsigned __int64)(v24 - (_BYTE *)a - 48) >> 4)) & 0xFFFFFFFFFFFFFFFLL) + 48;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v12;
  this->end = v14;
  this->cap = v13;
  return v30 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000a9700  size 748 bytes  184 insns
 *   std::vector<SC_NonLanguageItem, std::allocator<SC_NonLanguageItem> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI18SC_NonLanguageItemSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_NonLanguageItem>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  __int64 v6; // x27
  unsigned __int64 v8; // x21
  _BYTE *end; // x3
  __int64 v10; // x19
  unsigned __int64 v11; // x24
  unsigned __int64 v12; // x0
  unsigned __int64 v13; // x0
  __int64 v14; // x25
  char *v15; // x0
  unsigned __int64 v16; // x1
  char *v17; // x26
  char *v18; // x2
  _QWORD *begin; // x19
  _QWORD *v20; // x27
  char *v21; // x20
  char v22; // w0
  char v23; // w0
  char v24; // w0
  void *v25; // x0
  void *v26; // x0
  void *v27; // x0
  __int64 result; // x0
  _QWORD *v29; // x2
  __int64 v30; // [xsp+10h] [xbp+10h]
  __int64 v31; // [xsp+18h] [xbp+18h]
  __int64 v32; // [xsp+40h] [xbp+40h]
  __int64 v33; // [xsp+48h] [xbp+48h]
  __int64 v34; // [xsp+50h] [xbp+50h]
  __int64 v35; // [xsp+68h] [xbp+68h]
  __int64 v36; // [xsp+70h] [xbp+70h]
  v8 = a;
  if ( a )
  {
    end = this->end;
    v10 = end - (char *)this->begin;
    v11 = 0x8E38E38E38E38E39LL * (v10 >> 4);
    if ( a <= 0x8E38E38E38E38E39LL * (((char *)this->cap - (char *)end) >> 4) )
    {
      v29 = this->end;
      do
      {
        *v29 = 0;
        v29[1] = 0;
        --a;
        v29[2] = 0;
        *((_BYTE *)v29 + 24) = 0;
        v29[4] = 0;
        v29[5] = 0;
        v29[6] = 0;
        *((_BYTE *)v29 + 56) = 0;
        v29[8] = 0;
        v29[9] = 0;
        v29[10] = 0;
        *((_BYTE *)v29 + 88) = 0;
        *((_WORD *)v29 + 45) = 0;
        v29[12] = 0;
        v29[13] = 0;
        v29[14] = 0;
        v29[15] = 0;
        v29[16] = 0;
        v29[17] = 0;
        v29 += 18;
      }
      while ( a );
      this->end = &end[144 * v8];
      v2 = v30;
      v3 = v31;
      v4 = v32;
      v5 = v33;
    }
    else
    {
      v34 = v6;
      if ( 0xE38E38E38E38E3LL - v11 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v11 )
        v12 = 0x8E38E38E38E38E39LL * (v10 >> 4);
      else
        v12 = a;
      v13 = v11 + v12;
      if ( v13 > 0xE38E38E38E38E3LL )
        v13 = 0xE38E38E38E38E3LL;
      v14 = 144 * v13;
      v15 = (char *)operator new(144 * v13);
      v16 = v8;
      v17 = v15;
      v18 = &v15[v10];
      do
      {
        *(_QWORD *)v18 = 0;
        *((_QWORD *)v18 + 1) = 0;
        --v16;
        *((_QWORD *)v18 + 2) = 0;
        v18[24] = 0;
        *((_QWORD *)v18 + 4) = 0;
        *((_QWORD *)v18 + 5) = 0;
        *((_QWORD *)v18 + 6) = 0;
        v18[56] = 0;
        *((_QWORD *)v18 + 8) = 0;
        *((_QWORD *)v18 + 9) = 0;
        *((_QWORD *)v18 + 10) = 0;
        v18[88] = 0;
        *((_WORD *)v18 + 45) = 0;
        *((_QWORD *)v18 + 12) = 0;
        *((_QWORD *)v18 + 13) = 0;
        *((_QWORD *)v18 + 14) = 0;
        *((_QWORD *)v18 + 15) = 0;
        *((_QWORD *)v18 + 16) = 0;
        *((_QWORD *)v18 + 17) = 0;
        v18 += 144;
      }
      while ( v16 );
      begin = this->begin;
      v20 = this->end;
      if ( this->begin != v20 )
      {
        v21 = v15;
        do
        {
          *(_QWORD *)v21 = *begin;
          *((_QWORD *)v21 + 1) = begin[1];
          *((_QWORD *)v21 + 2) = begin[2];
          *begin = 0;
          begin[1] = 0;
          v22 = *((_BYTE *)begin + 24);
          begin[2] = 0;
          v21[24] = v22;
          *((_QWORD *)v21 + 4) = begin[4];
          *((_QWORD *)v21 + 5) = begin[5];
          *((_QWORD *)v21 + 6) = begin[6];
          begin[4] = 0;
          begin[5] = 0;
          v23 = *((_BYTE *)begin + 56);
          begin[6] = 0;
          v21[56] = v23;
          *((_QWORD *)v21 + 8) = begin[8];
          *((_QWORD *)v21 + 9) = begin[9];
          *((_QWORD *)v21 + 10) = begin[10];
          begin[8] = 0;
          begin[9] = 0;
          v24 = *((_BYTE *)begin + 88);
          begin[10] = 0;
          v21[88] = v24;
          *((_WORD *)v21 + 45) = *((_WORD *)begin + 45);
          *((_QWORD *)v21 + 12) = begin[12];
          *((_QWORD *)v21 + 13) = begin[13];
          *((_QWORD *)v21 + 14) = begin[14];
          begin[12] = 0;
          begin[13] = 0;
          begin[14] = 0;
          *((_QWORD *)v21 + 15) = begin[15];
          *((_QWORD *)v21 + 16) = begin[16];
          *((_QWORD *)v21 + 17) = begin[17];
          v25 = (void *)begin[12];
          if ( v25 )
            operator delete(v25);
          v26 = (void *)begin[8];
          if ( v26 )
            operator delete(v26);
          v27 = (void *)begin[4];
          if ( v27 )
            operator delete(v27);
          if ( *begin )
            operator delete((void *)*begin);
          begin += 18;
          v21 += 144;
        }
        while ( v20 != begin );
        v20 = this->begin;
      }
      if ( v20 )
        operator delete(v20);
      this->begin = v17;
      this->cap = &v17[v14];
      this->end = &v17[144 * v8 + 144 * v11];
      v2 = v30;
      v3 = v31;
      v4 = v32;
      v5 = v33;
      v6 = v34;
    }
  }
  result = v35 ^ _stack_chk_guard;
  if ( v35 != _stack_chk_guard )
  {
    v30 = v2;
    v31 = v3;
    v32 = v4;
    v33 = v5;
    v34 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a99f0  size 608 bytes  150 insns
 *   std::vector<SC_LanguageSpecificItem, std::allocator<SC_LanguageSpecificItem> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI23SC_LanguageSpecificItemSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_LanguageSpecificItem>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  _BYTE *end; // x4
  __int64 v9; // x3
  __int64 v10; // x19
  unsigned __int64 v11; // x24
  _QWORD *v12; // x2
  unsigned __int64 v13; // x3
  __int64 result; // x0
  unsigned __int64 v15; // x0
  unsigned __int64 v16; // x0
  __int64 v17; // x25
  char *v18; // x0
  unsigned __int64 v19; // x1
  char *v20; // x26
  char *v21; // x2
  _QWORD *begin; // x19
  _QWORD *v23; // x27
  char *v24; // x21
  char *v25; // x20
  char v26; // w0
  char *v27; // x0
  char *v28; // x0
  __int64 v29; // x1
  __int64 v30; // [xsp+10h] [xbp+10h]
  __int64 v31; // [xsp+18h] [xbp+18h]
  __int64 v32; // [xsp+40h] [xbp+40h]
  __int64 v33; // [xsp+48h] [xbp+48h]
  __int64 v34; // [xsp+68h] [xbp+68h]
  __int64 v35; // [xsp+70h] [xbp+70h]
  if ( a )
  {
    end = this->end;
    v9 = 0x199999999999999LL;
    v10 = end - (char *)this->begin;
    v11 = 0xCCCCCCCCCCCCCCCDLL * (v10 >> 4);
    if ( a > 0xCCCCCCCCCCCCCCCDLL * (((char *)this->cap - (char *)end) >> 4) )
    {
      v32 = v4;
      v33 = v5;
      if ( 0x199999999999999LL - v11 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v11 )
        v15 = 0xCCCCCCCCCCCCCCCDLL * (v10 >> 4);
      else
        v15 = a;
      v16 = v11 + v15;
      if ( v16 <= 0x199999999999999LL )
        v9 = v16;
      v17 = 80 * v9;
      v18 = (char *)operator new(80 * v9);
      v19 = a;
      v20 = v18;
      v21 = &v18[v10];
      do
      {
        *(_QWORD *)v21 = 0;
        *((_QWORD *)v21 + 1) = 0;
        --v19;
        *((_QWORD *)v21 + 2) = 0;
        *((_QWORD *)v21 + 4) = v21 + 48;
        *((_QWORD *)v21 + 5) = 0;
        v21 += 80;
        *(v21 - 32) = 0;
        *(v21 - 16) = 0;
        *(v21 - 8) = 0;
        *(v21 - 7) = 0;
      }
      while ( v19 );
      begin = this->begin;
      v23 = this->end;
      if ( this->begin != v23 )
      {
        v24 = (char *)(begin + 6);
        v25 = v18;
        do
        {
          *(_QWORD *)v25 = *begin;
          *((_QWORD *)v25 + 1) = begin[1];
          *((_QWORD *)v25 + 2) = begin[2];
          *begin = 0;
          begin[1] = 0;
          begin[2] = 0;
          *((_QWORD *)v25 + 4) = v25 + 48;
          v28 = (char *)begin[4];
          if ( v28 == v24 )
          {
            v29 = begin[7];
            *((_QWORD *)v25 + 6) = begin[6];
            *((_QWORD *)v25 + 7) = v29;
          }
          else
          {
            *((_QWORD *)v25 + 4) = v28;
            *((_QWORD *)v25 + 6) = begin[6];
          }
          *((_QWORD *)v25 + 5) = begin[5];
          begin[4] = v24;
          begin[5] = 0;
          v26 = *((_BYTE *)begin + 64);
          *((_BYTE *)begin + 48) = 0;
          v25[64] = v26;
          v25[72] = *((_BYTE *)begin + 72);
          v25[73] = *((_BYTE *)begin + 73);
          v27 = (char *)begin[4];
          if ( v24 != v27 )
            operator delete(v27);
          if ( *begin )
            operator delete((void *)*begin);
          begin += 10;
          v25 += 80;
          v24 += 80;
        }
        while ( v23 != begin );
        v23 = this->begin;
      }
      if ( v23 )
        operator delete(v23);
      this->begin = v20;
      this->cap = &v20[v17];
      this->end = &v20[80 * a + 80 * v11];
      v2 = v30;
      v3 = v31;
      v4 = v32;
      v5 = v33;
    }
    else
    {
      v12 = this->end;
      v13 = a;
      do
      {
        *v12 = 0;
        v12[1] = 0;
        --v13;
        v12[2] = 0;
        v12[4] = v12 + 6;
        v12[5] = 0;
        v12 += 10;
        *((_BYTE *)v12 - 32) = 0;
        *((_BYTE *)v12 - 16) = 0;
        *((_BYTE *)v12 - 8) = 0;
        *((_BYTE *)v12 - 7) = 0;
      }
      while ( v13 );
      this->end = &end[80 * a];
      v2 = v30;
      v3 = v31;
    }
  }
  result = v34 ^ _stack_chk_guard;
  if ( v34 != _stack_chk_guard )
  {
    v30 = v2;
    v31 = v3;
    v32 = v4;
    v33 = v5;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a9c50  size 768 bytes  189 insns
 *   std::vector<SC_LanguageSelectionSet, std::allocator<SC_LanguageSelectionSet> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI23SC_LanguageSelectionSetSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_LanguageSelectionSet>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  __int64 v6; // x27
  unsigned __int64 v8; // x21
  _BYTE *end; // x3
  __int64 v10; // x19
  unsigned __int64 v11; // x24
  unsigned __int64 v12; // x0
  unsigned __int64 v13; // x0
  __int64 v14; // x25
  char *v15; // x0
  unsigned __int64 v16; // x1
  char *v17; // x26
  char *v18; // x2
  _QWORD *begin; // x19
  _QWORD *v20; // x27
  char *v21; // x20
  __int16 v22; // w0
  void *v23; // x0
  void *v24; // x0
  void *v25; // x0
  __int64 result; // x0
  _QWORD *v27; // x2
  __int64 v28; // [xsp+10h] [xbp+10h]
  __int64 v29; // [xsp+18h] [xbp+18h]
  __int64 v30; // [xsp+40h] [xbp+40h]
  __int64 v31; // [xsp+48h] [xbp+48h]
  __int64 v32; // [xsp+50h] [xbp+50h]
  __int64 v33; // [xsp+68h] [xbp+68h]
  __int64 v34; // [xsp+70h] [xbp+70h]
  v8 = a;
  if ( a )
  {
    end = this->end;
    v10 = end - (char *)this->begin;
    v11 = 0x8E38E38E38E38E39LL * (v10 >> 4);
    if ( a <= 0x8E38E38E38E38E39LL * (((char *)this->cap - (char *)end) >> 4) )
    {
      v27 = this->end;
      do
      {
        *v27 = 0;
        v27[1] = 0;
        --a;
        v27[2] = 0;
        *((_WORD *)v27 + 12) = 0;
        *((_BYTE *)v27 + 26) = 0;
        v27[4] = 0;
        v27[5] = 0;
        v27[6] = 0;
        *((_BYTE *)v27 + 56) = 0;
        v27[8] = 0;
        v27[9] = 0;
        v27[10] = 0;
        *((_BYTE *)v27 + 88) = 0;
        *((_WORD *)v27 + 45) = 0;
        v27[12] = 0;
        v27[13] = 0;
        v27[14] = 0;
        v27[15] = 0;
        v27[16] = 0;
        v27[17] = 0;
        v27 += 18;
      }
      while ( a );
      this->end = &end[144 * v8];
      v2 = v28;
      v3 = v29;
      v4 = v30;
      v5 = v31;
    }
    else
    {
      v32 = v6;
      if ( 0xE38E38E38E38E3LL - v11 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v11 )
        v12 = 0x8E38E38E38E38E39LL * (v10 >> 4);
      else
        v12 = a;
      v13 = v11 + v12;
      if ( v13 > 0xE38E38E38E38E3LL )
        v13 = 0xE38E38E38E38E3LL;
      v14 = 144 * v13;
      v15 = (char *)operator new(144 * v13);
      v16 = v8;
      v17 = v15;
      v18 = &v15[v10];
      do
      {
        *(_QWORD *)v18 = 0;
        *((_QWORD *)v18 + 1) = 0;
        --v16;
        *((_QWORD *)v18 + 2) = 0;
        *((_WORD *)v18 + 12) = 0;
        v18[26] = 0;
        *((_QWORD *)v18 + 4) = 0;
        *((_QWORD *)v18 + 5) = 0;
        *((_QWORD *)v18 + 6) = 0;
        v18[56] = 0;
        *((_QWORD *)v18 + 8) = 0;
        *((_QWORD *)v18 + 9) = 0;
        *((_QWORD *)v18 + 10) = 0;
        v18[88] = 0;
        *((_WORD *)v18 + 45) = 0;
        *((_QWORD *)v18 + 12) = 0;
        *((_QWORD *)v18 + 13) = 0;
        *((_QWORD *)v18 + 14) = 0;
        *((_QWORD *)v18 + 15) = 0;
        *((_QWORD *)v18 + 16) = 0;
        *((_QWORD *)v18 + 17) = 0;
        v18 += 144;
      }
      while ( v16 );
      begin = this->begin;
      v20 = this->end;
      if ( this->begin != v20 )
      {
        v21 = v15;
        do
        {
          *(_QWORD *)v21 = *begin;
          *((_QWORD *)v21 + 1) = begin[1];
          *((_QWORD *)v21 + 2) = begin[2];
          *begin = 0;
          begin[1] = 0;
          v22 = *((_WORD *)begin + 12);
          begin[2] = 0;
          *((_WORD *)v21 + 12) = v22;
          v21[26] = *((_BYTE *)begin + 26);
          *((_QWORD *)v21 + 4) = begin[4];
          *((_QWORD *)v21 + 5) = begin[5];
          *((_QWORD *)v21 + 6) = begin[6];
          begin[4] = 0;
          begin[5] = 0;
          LOBYTE(v22) = *((_BYTE *)begin + 56);
          begin[6] = 0;
          v21[56] = v22;
          *((_QWORD *)v21 + 8) = begin[8];
          *((_QWORD *)v21 + 9) = begin[9];
          *((_QWORD *)v21 + 10) = begin[10];
          begin[9] = 0;
          begin[10] = 0;
          begin[8] = 0;
          v21[88] = *((_BYTE *)begin + 88);
          *((_WORD *)v21 + 45) = *((_WORD *)begin + 45);
          *((_QWORD *)v21 + 12) = begin[12];
          *((_QWORD *)v21 + 13) = begin[13];
          *((_QWORD *)v21 + 14) = begin[14];
          begin[12] = 0;
          begin[13] = 0;
          begin[14] = 0;
          *((_QWORD *)v21 + 15) = begin[15];
          *((_QWORD *)v21 + 16) = begin[16];
          *((_QWORD *)v21 + 17) = begin[17];
          v23 = (void *)begin[12];
          if ( v23 )
            operator delete(v23);
          v24 = (void *)begin[8];
          if ( v24 )
            operator delete(v24);
          v25 = (void *)begin[4];
          if ( v25 )
            operator delete(v25);
          if ( *begin )
            operator delete((void *)*begin);
          begin += 18;
          v21 += 144;
        }
        while ( v20 != begin );
        v20 = this->begin;
      }
      if ( v20 )
        operator delete(v20);
      this->begin = v17;
      this->cap = &v17[v14];
      this->end = &v17[144 * v8 + 144 * v11];
      v2 = v28;
      v3 = v29;
      v4 = v30;
      v5 = v31;
      v6 = v32;
    }
  }
  result = v33 ^ _stack_chk_guard;
  if ( v33 != _stack_chk_guard )
  {
    v28 = v2;
    v29 = v3;
    v30 = v4;
    v31 = v5;
    v32 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a9f50  size 960 bytes  239 insns
 *   std::vector<SC_CompositionSelectionGroup, std::allocator<SC_CompositionSelectionGroup> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI28SC_CompositionSelectionGroupSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_CompositionSelectionGroup>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  __int64 v6; // x27
  _BYTE *end; // x0
  __int64 v10; // x19
  unsigned __int64 v11; // x26
  _QWORD *v12; // x2
  unsigned __int64 v13; // x3
  __int64 result; // x0
  unsigned __int64 v15; // x24
  unsigned __int64 v16; // x24
  unsigned __int64 v17; // x24
  char *v18; // x0
  unsigned __int64 v19; // x1
  char *v20; // x25
  char *v21; // x2
  _QWORD *begin; // x19
  _QWORD *v23; // x27
  char *v24; // x20
  char v25; // w0
  char v26; // w0
  char v27; // w0
  char v28; // w0
  void *v29; // x0
  void *v30; // x0
  void *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  void *v35; // x0
  void *v36; // x0
  __int64 v37; // [xsp+10h] [xbp+10h]
  __int64 v38; // [xsp+18h] [xbp+18h]
  __int64 v39; // [xsp+40h] [xbp+40h]
  __int64 v40; // [xsp+48h] [xbp+48h]
  __int64 v41; // [xsp+50h] [xbp+50h]
  __int64 v42; // [xsp+68h] [xbp+68h]
  __int64 v43; // [xsp+70h] [xbp+70h]
  if ( a )
  {
    end = this->end;
    v10 = end - (char *)this->begin;
    v11 = v10 >> 8;
    if ( a > ((char *)this->cap - (char *)end) >> 8 )
    {
      v41 = v6;
      if ( 0x7FFFFFFFFFFFFFLL - (v10 >> 8) < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v11 )
        v15 = v10 >> 8;
      else
        v15 = a;
      v16 = v11 + v15;
      if ( v16 > 0x7FFFFFFFFFFFFFLL )
        v16 = 0x7FFFFFFFFFFFFFLL;
      v17 = v16 << 8;
      v18 = (char *)operator new(v17);
      v19 = a;
      v20 = v18;
      v21 = &v18[v10];
      do
      {
        *((_QWORD *)v21 + 1) = 0;
        *((_QWORD *)v21 + 2) = 0;
        --v19;
        *((_QWORD *)v21 + 3) = 0;
        *((_QWORD *)v21 + 4) = 0;
        *((_QWORD *)v21 + 5) = 0;
        *((_QWORD *)v21 + 6) = 0;
        *((_QWORD *)v21 + 7) = 0;
        *((_QWORD *)v21 + 8) = 0;
        *((_QWORD *)v21 + 9) = 0;
        v21[80] = 0;
        *((_QWORD *)v21 + 11) = 0;
        *((_QWORD *)v21 + 12) = 0;
        *((_QWORD *)v21 + 13) = 0;
        v21[112] = 0;
        *((_QWORD *)v21 + 15) = 0;
        *((_QWORD *)v21 + 16) = 0;
        *((_QWORD *)v21 + 17) = 0;
        v21[144] = 0;
        *((_WORD *)v21 + 73) = 0;
        *((_QWORD *)v21 + 19) = 0;
        *((_QWORD *)v21 + 20) = 0;
        *((_QWORD *)v21 + 21) = 0;
        *((_QWORD *)v21 + 22) = 0;
        *((_QWORD *)v21 + 23) = 0;
        *((_QWORD *)v21 + 24) = 0;
        v21[200] = 0;
        *((_WORD *)v21 + 101) = 0;
        *((_QWORD *)v21 + 26) = 0;
        *((_QWORD *)v21 + 27) = 0;
        *((_QWORD *)v21 + 28) = 0;
        *((_QWORD *)v21 + 29) = 0;
        *((_QWORD *)v21 + 30) = 0;
        *((_QWORD *)v21 + 31) = 0;
        v21 += 256;
      }
      while ( v19 );
      begin = this->begin;
      v23 = this->end;
      if ( this->begin != v23 )
      {
        v24 = v18;
        do
        {
          *(_WORD *)v24 = *(_WORD *)begin;
          *((_QWORD *)v24 + 1) = begin[1];
          *((_QWORD *)v24 + 2) = begin[2];
          *((_QWORD *)v24 + 3) = begin[3];
          begin[1] = 0;
          begin[2] = 0;
          begin[3] = 0;
          *((_QWORD *)v24 + 4) = begin[4];
          *((_QWORD *)v24 + 5) = begin[5];
          *((_QWORD *)v24 + 6) = begin[6];
          begin[4] = 0;
          begin[5] = 0;
          begin[6] = 0;
          *((_QWORD *)v24 + 7) = begin[7];
          *((_QWORD *)v24 + 8) = begin[8];
          *((_QWORD *)v24 + 9) = begin[9];
          begin[7] = 0;
          begin[8] = 0;
          v25 = *((_BYTE *)begin + 80);
          begin[9] = 0;
          v24[80] = v25;
          *((_QWORD *)v24 + 11) = begin[11];
          *((_QWORD *)v24 + 12) = begin[12];
          *((_QWORD *)v24 + 13) = begin[13];
          begin[11] = 0;
          begin[12] = 0;
          v26 = *((_BYTE *)begin + 112);
          begin[13] = 0;
          v24[112] = v26;
          *((_QWORD *)v24 + 15) = begin[15];
          *((_QWORD *)v24 + 16) = begin[16];
          *((_QWORD *)v24 + 17) = begin[17];
          begin[15] = 0;
          begin[16] = 0;
          v27 = *((_BYTE *)begin + 144);
          begin[17] = 0;
          v24[144] = v27;
          *((_WORD *)v24 + 73) = *((_WORD *)begin + 73);
          *((_QWORD *)v24 + 19) = begin[19];
          *((_QWORD *)v24 + 20) = begin[20];
          *((_QWORD *)v24 + 21) = begin[21];
          begin[19] = 0;
          begin[20] = 0;
          begin[21] = 0;
          *((_QWORD *)v24 + 22) = begin[22];
          *((_QWORD *)v24 + 23) = begin[23];
          *((_QWORD *)v24 + 24) = begin[24];
          begin[22] = 0;
          begin[23] = 0;
          v28 = *((_BYTE *)begin + 200);
          begin[24] = 0;
          v24[200] = v28;
          *((_WORD *)v24 + 101) = *((_WORD *)begin + 101);
          *((_QWORD *)v24 + 26) = begin[26];
          *((_QWORD *)v24 + 27) = begin[27];
          *((_QWORD *)v24 + 28) = begin[28];
          begin[26] = 0;
          begin[27] = 0;
          begin[28] = 0;
          *((_QWORD *)v24 + 29) = begin[29];
          *((_QWORD *)v24 + 30) = begin[30];
          *((_QWORD *)v24 + 31) = begin[31];
          v29 = (void *)begin[26];
          if ( v29 )
            operator delete(v29);
          v30 = (void *)begin[22];
          if ( v30 )
            operator delete(v30);
          v31 = (void *)begin[19];
          if ( v31 )
            operator delete(v31);
          v32 = (void *)begin[15];
          if ( v32 )
            operator delete(v32);
          v33 = (void *)begin[11];
          if ( v33 )
            operator delete(v33);
          v34 = (void *)begin[7];
          if ( v34 )
            operator delete(v34);
          v35 = (void *)begin[4];
          if ( v35 )
            operator delete(v35);
          v36 = (void *)begin[1];
          if ( v36 )
            operator delete(v36);
          begin += 32;
          v24 += 256;
        }
        while ( v23 != begin );
        v23 = this->begin;
      }
      if ( v23 )
        operator delete(v23);
      this->cap = &v20[v17];
      this->begin = v20;
      this->end = &v20[256 * (a + v11)];
      v2 = v37;
      v3 = v38;
      v4 = v39;
      v5 = v40;
      v6 = v41;
    }
    else
    {
      v12 = this->end;
      v13 = a;
      do
      {
        v12[1] = 0;
        v12[2] = 0;
        --v13;
        v12[3] = 0;
        v12[4] = 0;
        v12[5] = 0;
        v12[6] = 0;
        v12[7] = 0;
        v12[8] = 0;
        v12[9] = 0;
        *((_BYTE *)v12 + 80) = 0;
        v12[11] = 0;
        v12[12] = 0;
        v12[13] = 0;
        *((_BYTE *)v12 + 112) = 0;
        v12[15] = 0;
        v12[16] = 0;
        v12[17] = 0;
        *((_BYTE *)v12 + 144) = 0;
        *((_WORD *)v12 + 73) = 0;
        v12[19] = 0;
        v12[20] = 0;
        v12[21] = 0;
        v12[22] = 0;
        v12[23] = 0;
        v12[24] = 0;
        *((_BYTE *)v12 + 200) = 0;
        *((_WORD *)v12 + 101) = 0;
        v12[26] = 0;
        v12[27] = 0;
        v12[28] = 0;
        v12[29] = 0;
        v12[30] = 0;
        v12[31] = 0;
        v12 += 32;
      }
      while ( v13 );
      this->end = &end[256 * a];
      v2 = v37;
      v3 = v38;
      v4 = v39;
      v5 = v40;
    }
  }
  result = v42 ^ _stack_chk_guard;
  if ( v42 != _stack_chk_guard )
  {
    v37 = v2;
    v38 = v3;
    v39 = v4;
    v40 = v5;
    v41 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000aa310  size 1040 bytes  257 insns
 *   std::vector<SC_PresetDescription, std::allocator<SC_PresetDescription> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI20SC_PresetDescriptionSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_PresetDescription>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  __int64 v6; // x27
  __int64 v7; // x28
  _BYTE *end; // x3
  __int64 v11; // x19
  unsigned __int64 v12; // x26
  unsigned __int64 v13; // x0
  unsigned __int64 v14; // x0
  __int64 v15; // x24
  char *v16; // x25
  unsigned __int64 v17; // x1
  char *v18; // x2
  _QWORD *begin; // x19
  _QWORD *v20; // x28
  char *v21; // x27
  char *v22; // x20
  char v23; // w0
  char v24; // w0
  void *v25; // x0
  char *v26; // x0
  void *v27; // x0
  void *v28; // x0
  void *v29; // x0
  void *v30; // x0
  char v31; // w1
  char v32; // w1
  char v33; // w1
  __int64 v34; // x3
  __int64 v35; // x3
  char *v36; // x0
  __int64 v37; // x1
  __int64 result; // x0
  _QWORD *v39; // x2
  unsigned __int64 v40; // x0
  __int64 v41; // [xsp+10h] [xbp+10h]
  __int64 v42; // [xsp+18h] [xbp+18h]
  __int64 v43; // [xsp+40h] [xbp+40h]
  __int64 v44; // [xsp+48h] [xbp+48h]
  __int64 v45; // [xsp+50h] [xbp+50h]
  __int64 v46; // [xsp+58h] [xbp+58h]
  __int64 v47; // [xsp+68h] [xbp+68h]
  __int64 v48; // [xsp+70h] [xbp+70h]
  if ( a )
  {
    end = this->end;
    v11 = end - (char *)this->begin;
    v12 = 0xEF7BDEF7BDEF7BDFLL * (v11 >> 3);
    if ( a <= 0xEF7BDEF7BDEF7BDFLL * (((char *)this->cap - (char *)end) >> 3) )
    {
      v39 = this->end;
      v40 = a;
      do
      {
        *v39 = 0;
        v39[1] = 0;
        --v40;
        v39[2] = 0;
        *((_BYTE *)v39 + 24) = 0;
        v39[4] = 0;
        v39[5] = 0;
        v39[6] = 0;
        *((_BYTE *)v39 + 56) = 0;
        *((_WORD *)v39 + 29) = 0;
        v39[8] = 0;
        v39[9] = 0;
        v39[10] = 0;
        v39[11] = 0;
        v39[12] = 0;
        v39[13] = 0;
        *((_BYTE *)v39 + 112) = 0;
        v39[15] = 0;
        *((_DWORD *)v39 + 32) = 0;
        v39[17] = 0;
        *((_DWORD *)v39 + 36) = 0;
        v39[19] = 0;
        v39[21] = v39 + 23;
        v39[22] = 0;
        *((_BYTE *)v39 + 184) = 0;
        *((_BYTE *)v39 + 200) = 0;
        *((_BYTE *)v39 + 208) = 1;
        *((_BYTE *)v39 + 209) = 1;
        v39[27] = 0;
        v39[28] = 0;
        v39 += 31;
        *(v39 - 2) = 0;
      }
      while ( v40 );
      this->end = &end[248 * a];
      v2 = v41;
      v3 = v42;
      v4 = v43;
      v5 = v44;
    }
    else
    {
      v45 = v6;
      v46 = v7;
      if ( 0x84210842108421LL - v12 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v12 )
        v13 = 0xEF7BDEF7BDEF7BDFLL * (v11 >> 3);
      else
        v13 = a;
      v14 = v12 + v13;
      if ( v14 > 0x84210842108421LL )
        v14 = 0x84210842108421LL;
      v15 = 248 * v14;
      v16 = (char *)operator new(248 * v14);
      v17 = a;
      v18 = &v16[v11];
      do
      {
        *(_QWORD *)v18 = 0;
        *((_QWORD *)v18 + 1) = 0;
        --v17;
        *((_QWORD *)v18 + 2) = 0;
        v18[24] = 0;
        *((_QWORD *)v18 + 4) = 0;
        *((_QWORD *)v18 + 5) = 0;
        *((_QWORD *)v18 + 6) = 0;
        v18[56] = 0;
        *((_WORD *)v18 + 29) = 0;
        *((_QWORD *)v18 + 8) = 0;
        *((_QWORD *)v18 + 9) = 0;
        *((_QWORD *)v18 + 10) = 0;
        *((_QWORD *)v18 + 11) = 0;
        *((_QWORD *)v18 + 12) = 0;
        *((_QWORD *)v18 + 13) = 0;
        v18[112] = 0;
        *((_QWORD *)v18 + 15) = 0;
        *((_DWORD *)v18 + 32) = 0;
        *((_QWORD *)v18 + 17) = 0;
        *((_DWORD *)v18 + 36) = 0;
        *((_QWORD *)v18 + 19) = 0;
        *((_QWORD *)v18 + 21) = v18 + 184;
        *((_QWORD *)v18 + 22) = 0;
        v18[184] = 0;
        v18[200] = 0;
        v18[208] = 1;
        v18[209] = 1;
        *((_QWORD *)v18 + 27) = 0;
        *((_QWORD *)v18 + 28) = 0;
        v18 += 248;
        *((_QWORD *)v18 - 2) = 0;
      }
      while ( v17 );
      begin = this->begin;
      v20 = this->end;
      if ( this->begin != v20 )
      {
        v21 = (char *)(begin + 23);
        v22 = v16;
        do
        {
          *(_QWORD *)v22 = *begin;
          *((_QWORD *)v22 + 1) = begin[1];
          *((_QWORD *)v22 + 2) = begin[2];
          *begin = 0;
          begin[1] = 0;
          v31 = *((_BYTE *)begin + 24);
          begin[2] = 0;
          v22[24] = v31;
          *((_QWORD *)v22 + 4) = begin[4];
          *((_QWORD *)v22 + 5) = begin[5];
          *((_QWORD *)v22 + 6) = begin[6];
          begin[4] = 0;
          begin[5] = 0;
          v32 = *((_BYTE *)begin + 56);
          begin[6] = 0;
          v22[56] = v32;
          *((_WORD *)v22 + 29) = *((_WORD *)begin + 29);
          *((_QWORD *)v22 + 8) = begin[8];
          *((_QWORD *)v22 + 9) = begin[9];
          *((_QWORD *)v22 + 10) = begin[10];
          begin[9] = 0;
          begin[10] = 0;
          begin[8] = 0;
          *((_QWORD *)v22 + 11) = begin[11];
          *((_QWORD *)v22 + 12) = begin[12];
          *((_QWORD *)v22 + 13) = begin[13];
          begin[11] = 0;
          begin[12] = 0;
          v33 = *((_BYTE *)begin + 112);
          begin[13] = 0;
          v22[112] = v33;
          v34 = begin[16];
          *((_QWORD *)v22 + 15) = begin[15];
          *((_QWORD *)v22 + 16) = v34;
          v35 = begin[18];
          *((_QWORD *)v22 + 17) = begin[17];
          *((_QWORD *)v22 + 18) = v35;
          *((_QWORD *)v22 + 19) = begin[19];
          begin[15] = 0;
          *((_DWORD *)begin + 32) = 0;
          begin[17] = 0;
          *((_DWORD *)begin + 36) = 0;
          begin[19] = 0;
          *((_QWORD *)v22 + 21) = v22 + 184;
          v36 = (char *)begin[21];
          if ( v36 == v21 )
          {
            v37 = begin[24];
            *((_QWORD *)v22 + 23) = begin[23];
            *((_QWORD *)v22 + 24) = v37;
          }
          else
          {
            *((_QWORD *)v22 + 21) = v36;
            *((_QWORD *)v22 + 23) = begin[23];
          }
          *((_QWORD *)v22 + 22) = begin[22];
          begin[21] = v21;
          begin[22] = 0;
          v23 = *((_BYTE *)begin + 200);
          *((_BYTE *)begin + 184) = 0;
          v22[200] = v23;
          v22[208] = *((_BYTE *)begin + 208);
          v22[209] = *((_BYTE *)begin + 209);
          *((_QWORD *)v22 + 27) = begin[27];
          *((_QWORD *)v22 + 28) = begin[28];
          *((_QWORD *)v22 + 29) = begin[29];
          begin[27] = 0;
          begin[28] = 0;
          v24 = *((_BYTE *)begin + 240);
          begin[29] = 0;
          v22[240] = v24;
          v25 = (void *)begin[27];
          if ( v25 )
            operator delete(v25);
          v26 = (char *)begin[21];
          if ( v26 != v21 )
            operator delete(v26);
          v27 = (void *)begin[15];
          if ( v27 )
            operator delete(v27);
          v28 = (void *)begin[11];
          if ( v28 )
            operator delete(v28);
          v29 = (void *)begin[8];
          if ( v29 )
            operator delete(v29);
          v30 = (void *)begin[4];
          if ( v30 )
            operator delete(v30);
          if ( *begin )
            operator delete((void *)*begin);
          begin += 31;
          v22 += 248;
          v21 += 248;
        }
        while ( v20 != begin );
        v20 = this->begin;
      }
      if ( v20 )
        operator delete(v20);
      this->cap = &v16[v15];
      this->begin = v16;
      this->end = &v16[248 * a + 248 * v12];
      v2 = v41;
      v3 = v42;
      v4 = v43;
      v5 = v44;
      v6 = v45;
      v7 = v46;
    }
  }
  result = v47 ^ _stack_chk_guard;
  if ( v47 != _stack_chk_guard )
  {
    v41 = v2;
    v42 = v3;
    v43 = v4;
    v44 = v5;
    v45 = v6;
    v46 = v7;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000aa720  size 824 bytes  203 insns
 *   std::vector<SC_SceneComposition::SC_Category, std::allocator<SC_SceneComposition::SC_Category> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorIN19SC_SceneComposition11SC_CategoryESaIS1_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_SceneComposition::SC_Category>::_M_default_append(
        std_vector *this,
        unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  __int64 v6; // x27
  _BYTE *end; // x1
  __int64 v10; // x19
  unsigned __int64 v11; // x26
  unsigned __int64 v12; // x0
  unsigned __int64 v13; // x0
  __int64 v14; // x24
  char *v15; // x0
  unsigned __int64 v16; // x1
  char *v17; // x25
  char *v18; // x2
  _QWORD *begin; // x19
  _QWORD *v20; // x27
  char *v21; // x20
  char v22; // w0
  char v23; // w0
  char v24; // w0
  char v25; // w0
  void *v26; // x0
  void *v27; // x0
  void *v28; // x0
  void *v29; // x0
  __int64 result; // x0
  _QWORD *v31; // x2
  unsigned __int64 v32; // x0
  __int64 v33; // [xsp+10h] [xbp+10h]
  __int64 v34; // [xsp+18h] [xbp+18h]
  __int64 v35; // [xsp+40h] [xbp+40h]
  __int64 v36; // [xsp+48h] [xbp+48h]
  __int64 v37; // [xsp+50h] [xbp+50h]
  __int64 v38; // [xsp+68h] [xbp+68h]
  __int64 v39; // [xsp+70h] [xbp+70h]
  if ( a )
  {
    end = this->end;
    v10 = end - (char *)this->begin;
    v11 = 0x2E8BA2E8BA2E8BA3LL * (v10 >> 4);
    if ( a <= 0x2E8BA2E8BA2E8BA3LL * (((char *)this->cap - (char *)end) >> 4) )
    {
      v31 = this->end;
      v32 = a;
      do
      {
        *v31 = 0;
        v31[1] = 0;
        --v32;
        v31[2] = 0;
        *((_BYTE *)v31 + 24) = 0;
        v31[4] = 0;
        v31[5] = 0;
        v31[6] = 0;
        *((_BYTE *)v31 + 56) = 0;
        v31[8] = 0;
        v31[9] = 0;
        v31[10] = 0;
        *((_BYTE *)v31 + 88) = 0;
        v31[12] = 0;
        v31[13] = 0;
        v31[14] = 0;
        *((_BYTE *)v31 + 120) = 0;
        *((_WORD *)v31 + 61) = 0;
        v31[16] = 0;
        v31[17] = 0;
        v31[18] = 0;
        v31[19] = 0;
        v31[20] = 0;
        v31[21] = 0;
        v31 += 22;
      }
      while ( v32 );
      this->end = &end[176 * a];
      v2 = v33;
      v3 = v34;
      v4 = v35;
      v5 = v36;
    }
    else
    {
      v37 = v6;
      if ( 0xBA2E8BA2E8BA2ELL - v11 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v11 )
        v12 = 0x2E8BA2E8BA2E8BA3LL * (v10 >> 4);
      else
        v12 = a;
      v13 = v11 + v12;
      if ( v13 > 0xBA2E8BA2E8BA2ELL )
        v13 = 0xBA2E8BA2E8BA2ELL;
      v14 = 176 * v13;
      v15 = (char *)operator new(176 * v13);
      v16 = a;
      v17 = v15;
      v18 = &v15[v10];
      do
      {
        *(_QWORD *)v18 = 0;
        *((_QWORD *)v18 + 1) = 0;
        --v16;
        *((_QWORD *)v18 + 2) = 0;
        v18[24] = 0;
        *((_QWORD *)v18 + 4) = 0;
        *((_QWORD *)v18 + 5) = 0;
        *((_QWORD *)v18 + 6) = 0;
        v18[56] = 0;
        *((_QWORD *)v18 + 8) = 0;
        *((_QWORD *)v18 + 9) = 0;
        *((_QWORD *)v18 + 10) = 0;
        v18[88] = 0;
        *((_QWORD *)v18 + 12) = 0;
        *((_QWORD *)v18 + 13) = 0;
        *((_QWORD *)v18 + 14) = 0;
        v18[120] = 0;
        *((_WORD *)v18 + 61) = 0;
        *((_QWORD *)v18 + 16) = 0;
        *((_QWORD *)v18 + 17) = 0;
        *((_QWORD *)v18 + 18) = 0;
        *((_QWORD *)v18 + 19) = 0;
        *((_QWORD *)v18 + 20) = 0;
        *((_QWORD *)v18 + 21) = 0;
        v18 += 176;
      }
      while ( v16 );
      begin = this->begin;
      v20 = this->end;
      if ( this->begin != v20 )
      {
        v21 = v15;
        do
        {
          *(_QWORD *)v21 = *begin;
          *((_QWORD *)v21 + 1) = begin[1];
          *((_QWORD *)v21 + 2) = begin[2];
          *begin = 0;
          begin[1] = 0;
          v22 = *((_BYTE *)begin + 24);
          begin[2] = 0;
          v21[24] = v22;
          *((_QWORD *)v21 + 4) = begin[4];
          *((_QWORD *)v21 + 5) = begin[5];
          *((_QWORD *)v21 + 6) = begin[6];
          begin[4] = 0;
          begin[5] = 0;
          v23 = *((_BYTE *)begin + 56);
          begin[6] = 0;
          v21[56] = v23;
          *((_QWORD *)v21 + 8) = begin[8];
          *((_QWORD *)v21 + 9) = begin[9];
          *((_QWORD *)v21 + 10) = begin[10];
          begin[8] = 0;
          begin[9] = 0;
          v24 = *((_BYTE *)begin + 88);
          begin[10] = 0;
          v21[88] = v24;
          *((_QWORD *)v21 + 12) = begin[12];
          *((_QWORD *)v21 + 13) = begin[13];
          *((_QWORD *)v21 + 14) = begin[14];
          begin[12] = 0;
          begin[13] = 0;
          v25 = *((_BYTE *)begin + 120);
          begin[14] = 0;
          v21[120] = v25;
          *((_WORD *)v21 + 61) = *((_WORD *)begin + 61);
          *((_QWORD *)v21 + 16) = begin[16];
          *((_QWORD *)v21 + 17) = begin[17];
          *((_QWORD *)v21 + 18) = begin[18];
          begin[16] = 0;
          begin[17] = 0;
          begin[18] = 0;
          *((_QWORD *)v21 + 19) = begin[19];
          *((_QWORD *)v21 + 20) = begin[20];
          *((_QWORD *)v21 + 21) = begin[21];
          v26 = (void *)begin[16];
          if ( v26 )
            operator delete(v26);
          v27 = (void *)begin[12];
          if ( v27 )
            operator delete(v27);
          v28 = (void *)begin[8];
          if ( v28 )
            operator delete(v28);
          v29 = (void *)begin[4];
          if ( v29 )
            operator delete(v29);
          if ( *begin )
            operator delete((void *)*begin);
          begin += 22;
          v21 += 176;
        }
        while ( v20 != begin );
        v20 = this->begin;
      }
      if ( v20 )
        operator delete(v20);
      this->cap = &v17[v14];
      this->begin = v17;
      this->end = &v17[176 * a + 176 * v11];
      v2 = v33;
      v3 = v34;
      v4 = v35;
      v5 = v36;
      v6 = v37;
    }
  }
  result = v38 ^ _stack_chk_guard;
  if ( v38 != _stack_chk_guard )
  {
    v33 = v2;
    v34 = v3;
    v35 = v4;
    v36 = v5;
    v37 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000aae00  size 1456 bytes  361 insns
 *   std::vector<SC_SceneComposition, std::allocator<SC_SceneComposition> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorI19SC_SceneCompositionSaIS0_EE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<SC_SceneComposition>::_M_default_append(std_vector *this, unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  _BYTE *end; // x4
  __int64 v9; // x3
  __int64 v10; // x19
  unsigned __int64 v11; // x24
  char *v12; // x2
  unsigned __int64 v13; // x3
  char *v14; // x0
  __int64 result; // x0
  unsigned __int64 v16; // x0
  unsigned __int64 v17; // x0
  char *v18; // x0
  char *v19; // x2
  unsigned __int64 v20; // x1
  char *v21; // x0
  char *begin; // x22
  char *v23; // x28
  char *v24; // x19
  char *v25; // x26
  char *v26; // x25
  char *v27; // x20
  char v28; // w3
  char v29; // w3
  char v30; // w3
  char v31; // w3
  char v32; // w3
  char *v33; // x0
  char v34; // w3
  char *v35; // x0
  SC_SceneComposition *v36; // x0
  char v37; // w3
  char *v38; // x0
  __int64 v39; // x5
  __int64 v40; // x5
  __int64 v41; // x5
  char *v42; // x26
  char *v43; // x25
  __int64 v44; // [xsp+10h] [xbp+10h]
  __int64 v45; // [xsp+18h] [xbp+18h]
  __int64 v46; // [xsp+40h] [xbp+40h]
  __int64 v47; // [xsp+48h] [xbp+48h]
  char *v48; // [xsp+60h] [xbp+60h]
  __int64 v49; // [xsp+68h] [xbp+68h]
  __int64 v50; // [xsp+78h] [xbp+78h]
  __int64 v51; // [xsp+80h] [xbp+80h]
  if ( a )
  {
    end = this->end;
    v9 = 0x4325C53EF368EBLL;
    v10 = end - (char *)this->begin;
    v11 = 0x4FBCDA3AC10C9715LL * (v10 >> 3);
    if ( a > 0x4FBCDA3AC10C9715LL * (((char *)this->cap - (char *)end) >> 3) )
    {
      v46 = v4;
      v47 = v5;
      if ( 0x4325C53EF368EBLL - v11 < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v11 )
        v16 = 0x4FBCDA3AC10C9715LL * (v10 >> 3);
      else
        v16 = a;
      v17 = v11 + v16;
      if ( v17 <= 0x4325C53EF368EBLL )
        v9 = v17;
      v49 = 488 * v9;
      v18 = (char *)operator new(488 * v9);
      v19 = &v18[v10];
      v20 = a;
      v48 = v18;
      do
      {
        *v19 = 0;
        v19[1] = 0;
        *((_QWORD *)v19 + 2) = v19 + 32;
        *((_QWORD *)v19 + 3) = 0;
        v21 = v19 + 464;
        --v20;
        v19[32] = 0;
        v19[48] = 0;
        *((_QWORD *)v19 + 7) = 0;
        *((_QWORD *)v19 + 8) = 0;
        *((_QWORD *)v19 + 9) = 0;
        *((_QWORD *)v19 + 10) = 0;
        *((_QWORD *)v19 + 11) = 0;
        *((_QWORD *)v19 + 12) = 0;
        *((_QWORD *)v19 + 13) = 0;
        *((_QWORD *)v19 + 14) = 0;
        *((_QWORD *)v19 + 15) = 0;
        *((_QWORD *)v19 + 16) = 0;
        *((_QWORD *)v19 + 17) = 0;
        *((_QWORD *)v19 + 18) = 0;
        v19[152] = 0;
        *((_QWORD *)v19 + 20) = 0;
        *((_QWORD *)v19 + 21) = 0;
        *((_QWORD *)v19 + 22) = 0;
        v19[184] = 0;
        *((_QWORD *)v19 + 24) = 0;
        *((_QWORD *)v19 + 25) = 0;
        *((_QWORD *)v19 + 26) = 0;
        v19[216] = 0;
        *((_QWORD *)v19 + 28) = 0;
        *((_QWORD *)v19 + 29) = 0;
        *((_QWORD *)v19 + 30) = 0;
        v19[248] = 0;
        *((_WORD *)v19 + 125) = 0;
        *((_QWORD *)v19 + 32) = 0;
        *((_QWORD *)v19 + 33) = 0;
        *((_QWORD *)v19 + 34) = 0;
        *((_QWORD *)v19 + 35) = 0;
        *((_QWORD *)v19 + 36) = 0;
        *((_QWORD *)v19 + 37) = 0;
        v19[304] = 0;
        *((_QWORD *)v19 + 39) = 0;
        *((_QWORD *)v19 + 40) = 0;
        *((_QWORD *)v19 + 41) = 0;
        v19[336] = 0;
        *((_WORD *)v19 + 169) = 0;
        *((_QWORD *)v19 + 43) = 0;
        *((_QWORD *)v19 + 44) = 0;
        *((_QWORD *)v19 + 45) = 0;
        *((_QWORD *)v19 + 46) = 0;
        *((_QWORD *)v19 + 47) = 0;
        *((_QWORD *)v19 + 48) = 0;
        *((_QWORD *)v19 + 50) = v19 + 416;
        *((_QWORD *)v19 + 51) = 0;
        v19 += 488;
        *(v19 - 72) = 0;
        *(v19 - 56) = 0;
        *((_QWORD *)v19 - 5) = v21;
        *((_QWORD *)v19 - 4) = 0;
        *(v19 - 24) = 0;
        *(v19 - 8) = 0;
      }
      while ( v20 );
      begin = (char *)this->begin;
      v23 = (char *)this->end;
      if ( this->begin != v23 )
      {
        v24 = begin + 32;
        v25 = begin + 464;
        v26 = begin + 416;
        v27 = v48;
        do
        {
          *v27 = *(v24 - 32);
          v27[1] = *(v24 - 31);
          *((_QWORD *)v27 + 2) = v27 + 32;
          v38 = *((char **)v24 - 2);
          if ( v38 == v24 )
          {
            v39 = *((_QWORD *)v24 + 1);
            *((_QWORD *)v27 + 4) = *(_QWORD *)v24;
            *((_QWORD *)v27 + 5) = v39;
          }
          else
          {
            *((_QWORD *)v27 + 2) = v38;
            *((_QWORD *)v27 + 4) = *(_QWORD *)v24;
          }
          *((_QWORD *)v27 + 3) = *((_QWORD *)v24 - 1);
          *((_QWORD *)v24 - 2) = v24;
          *((_QWORD *)v24 - 1) = 0;
          v28 = v24[16];
          *v24 = 0;
          v27[48] = v28;
          *((_QWORD *)v27 + 7) = *((_QWORD *)v24 + 3);
          *((_QWORD *)v27 + 8) = *((_QWORD *)v24 + 4);
          *((_QWORD *)v27 + 9) = *((_QWORD *)v24 + 5);
          *((_QWORD *)v24 + 3) = 0;
          *((_QWORD *)v24 + 4) = 0;
          *((_QWORD *)v24 + 5) = 0;
          *((_QWORD *)v27 + 10) = *((_QWORD *)v24 + 6);
          *((_QWORD *)v27 + 11) = *((_QWORD *)v24 + 7);
          *((_QWORD *)v27 + 12) = *((_QWORD *)v24 + 8);
          *((_QWORD *)v24 + 6) = 0;
          *((_QWORD *)v24 + 7) = 0;
          *((_QWORD *)v24 + 8) = 0;
          *((_QWORD *)v27 + 13) = *((_QWORD *)v24 + 9);
          *((_QWORD *)v27 + 14) = *((_QWORD *)v24 + 10);
          *((_QWORD *)v27 + 15) = *((_QWORD *)v24 + 11);
          *((_QWORD *)v24 + 10) = 0;
          *((_QWORD *)v24 + 11) = 0;
          *((_QWORD *)v24 + 9) = 0;
          *((_QWORD *)v27 + 16) = *((_QWORD *)v24 + 12);
          *((_QWORD *)v27 + 17) = *((_QWORD *)v24 + 13);
          *((_QWORD *)v27 + 18) = *((_QWORD *)v24 + 14);
          *((_QWORD *)v24 + 12) = 0;
          *((_QWORD *)v24 + 13) = 0;
          v29 = v24[120];
          *((_QWORD *)v24 + 14) = 0;
          v27[152] = v29;
          *((_QWORD *)v27 + 20) = *((_QWORD *)v24 + 16);
          *((_QWORD *)v27 + 21) = *((_QWORD *)v24 + 17);
          *((_QWORD *)v27 + 22) = *((_QWORD *)v24 + 18);
          *((_QWORD *)v24 + 16) = 0;
          *((_QWORD *)v24 + 17) = 0;
          v30 = v24[152];
          *((_QWORD *)v24 + 18) = 0;
          v27[184] = v30;
          *((_QWORD *)v27 + 24) = *((_QWORD *)v24 + 20);
          *((_QWORD *)v27 + 25) = *((_QWORD *)v24 + 21);
          *((_QWORD *)v27 + 26) = *((_QWORD *)v24 + 22);
          *((_QWORD *)v24 + 20) = 0;
          *((_QWORD *)v24 + 21) = 0;
          *((_QWORD *)v24 + 22) = 0;
          v27[216] = v24[184];
          *((_QWORD *)v27 + 28) = *((_QWORD *)v24 + 24);
          *((_QWORD *)v27 + 29) = *((_QWORD *)v24 + 25);
          *((_QWORD *)v27 + 30) = *((_QWORD *)v24 + 26);
          *((_QWORD *)v24 + 24) = 0;
          *((_QWORD *)v24 + 25) = 0;
          v31 = v24[216];
          *((_QWORD *)v24 + 26) = 0;
          v27[248] = v31;
          *((_WORD *)v27 + 125) = *((_WORD *)v24 + 109);
          *((_QWORD *)v27 + 32) = *((_QWORD *)v24 + 28);
          *((_QWORD *)v27 + 33) = *((_QWORD *)v24 + 29);
          *((_QWORD *)v27 + 34) = *((_QWORD *)v24 + 30);
          *((_QWORD *)v24 + 28) = 0;
          *((_QWORD *)v24 + 29) = 0;
          *((_QWORD *)v24 + 30) = 0;
          *((_QWORD *)v27 + 35) = *((_QWORD *)v24 + 31);
          *((_QWORD *)v27 + 36) = *((_QWORD *)v24 + 32);
          *((_QWORD *)v27 + 37) = *((_QWORD *)v24 + 33);
          *((_QWORD *)v24 + 32) = 0;
          *((_QWORD *)v24 + 33) = 0;
          *((_QWORD *)v24 + 31) = 0;
          v27[304] = v24[272];
          *((_QWORD *)v27 + 39) = *((_QWORD *)v24 + 35);
          *((_QWORD *)v27 + 40) = *((_QWORD *)v24 + 36);
          *((_QWORD *)v27 + 41) = *((_QWORD *)v24 + 37);
          *((_QWORD *)v24 + 35) = 0;
          *((_QWORD *)v24 + 36) = 0;
          v32 = v24[304];
          *((_QWORD *)v24 + 37) = 0;
          v27[336] = v32;
          *((_WORD *)v27 + 169) = *((_WORD *)v24 + 153);
          *((_QWORD *)v27 + 43) = *((_QWORD *)v24 + 39);
          *((_QWORD *)v27 + 44) = *((_QWORD *)v24 + 40);
          *((_QWORD *)v27 + 45) = *((_QWORD *)v24 + 41);
          *((_QWORD *)v24 + 39) = 0;
          *((_QWORD *)v24 + 40) = 0;
          *((_QWORD *)v24 + 41) = 0;
          *((_QWORD *)v27 + 46) = *((_QWORD *)v24 + 42);
          *((_QWORD *)v27 + 47) = *((_QWORD *)v24 + 43);
          *((_QWORD *)v27 + 48) = *((_QWORD *)v24 + 44);
          *((_QWORD *)v24 + 43) = 0;
          *((_QWORD *)v24 + 44) = 0;
          *((_QWORD *)v24 + 42) = 0;
          *((_QWORD *)v27 + 50) = v27 + 416;
          v33 = *((char **)v24 + 46);
          if ( v33 == v26 )
          {
            v41 = *((_QWORD *)v24 + 49);
            *((_QWORD *)v27 + 52) = *((_QWORD *)v24 + 48);
            *((_QWORD *)v27 + 53) = v41;
          }
          else
          {
            *((_QWORD *)v27 + 50) = v33;
            *((_QWORD *)v27 + 52) = *((_QWORD *)v24 + 48);
          }
          *((_QWORD *)v27 + 51) = *((_QWORD *)v24 + 47);
          *((_QWORD *)v24 + 46) = v26;
          *((_QWORD *)v24 + 47) = 0;
          v34 = v24[400];
          v24[384] = 0;
          v27[432] = v34;
          *((_QWORD *)v27 + 56) = v27 + 464;
          v35 = *((char **)v24 + 52);
          if ( v35 == v25 )
          {
            v40 = *((_QWORD *)v24 + 55);
            *((_QWORD *)v27 + 58) = *((_QWORD *)v24 + 54);
            *((_QWORD *)v27 + 59) = v40;
          }
          else
          {
            *((_QWORD *)v27 + 56) = v35;
            *((_QWORD *)v27 + 58) = *((_QWORD *)v24 + 54);
          }
          *((_QWORD *)v27 + 57) = *((_QWORD *)v24 + 53);
          *((_QWORD *)v24 + 52) = v25;
          *((_QWORD *)v24 + 53) = 0;
          v36 = (SC_SceneComposition *)begin;
          v37 = v24[448];
          v24[432] = 0;
          begin += 488;
          v27[480] = v37;
          v26 += 488;
          v25 += 488;
          v27 += 488;
          SC_SceneComposition::~SC_SceneComposition(v36);
          v24 += 488;
        }
        while ( v23 != begin );
        v23 = (char *)this->begin;
      }
      if ( v23 )
        operator delete(v23);
      v42 = &v48[488 * a + 488 * v11];
      v43 = &v48[v49];
      this->begin = v48;
      this->end = v42;
      this->cap = v43;
      v2 = v44;
      v3 = v45;
      v4 = v46;
      v5 = v47;
    }
    else
    {
      v12 = (char *)this->end;
      v13 = a;
      do
      {
        *v12 = 0;
        v12[1] = 0;
        *((_QWORD *)v12 + 2) = v12 + 32;
        *((_QWORD *)v12 + 3) = 0;
        v14 = v12 + 464;
        --v13;
        v12[32] = 0;
        v12[48] = 0;
        *((_QWORD *)v12 + 7) = 0;
        *((_QWORD *)v12 + 8) = 0;
        *((_QWORD *)v12 + 9) = 0;
        *((_QWORD *)v12 + 10) = 0;
        *((_QWORD *)v12 + 11) = 0;
        *((_QWORD *)v12 + 12) = 0;
        *((_QWORD *)v12 + 13) = 0;
        *((_QWORD *)v12 + 14) = 0;
        *((_QWORD *)v12 + 15) = 0;
        *((_QWORD *)v12 + 16) = 0;
        *((_QWORD *)v12 + 17) = 0;
        *((_QWORD *)v12 + 18) = 0;
        v12[152] = 0;
        *((_QWORD *)v12 + 20) = 0;
        *((_QWORD *)v12 + 21) = 0;
        *((_QWORD *)v12 + 22) = 0;
        v12[184] = 0;
        *((_QWORD *)v12 + 24) = 0;
        *((_QWORD *)v12 + 25) = 0;
        *((_QWORD *)v12 + 26) = 0;
        v12[216] = 0;
        *((_QWORD *)v12 + 28) = 0;
        *((_QWORD *)v12 + 29) = 0;
        *((_QWORD *)v12 + 30) = 0;
        v12[248] = 0;
        *((_WORD *)v12 + 125) = 0;
        *((_QWORD *)v12 + 32) = 0;
        *((_QWORD *)v12 + 33) = 0;
        *((_QWORD *)v12 + 34) = 0;
        *((_QWORD *)v12 + 35) = 0;
        *((_QWORD *)v12 + 36) = 0;
        *((_QWORD *)v12 + 37) = 0;
        v12[304] = 0;
        *((_QWORD *)v12 + 39) = 0;
        *((_QWORD *)v12 + 40) = 0;
        *((_QWORD *)v12 + 41) = 0;
        v12[336] = 0;
        *((_WORD *)v12 + 169) = 0;
        *((_QWORD *)v12 + 43) = 0;
        *((_QWORD *)v12 + 44) = 0;
        *((_QWORD *)v12 + 45) = 0;
        *((_QWORD *)v12 + 46) = 0;
        *((_QWORD *)v12 + 47) = 0;
        *((_QWORD *)v12 + 48) = 0;
        *((_QWORD *)v12 + 50) = v12 + 416;
        *((_QWORD *)v12 + 51) = 0;
        v12 += 488;
        *(v12 - 72) = 0;
        *(v12 - 56) = 0;
        *((_QWORD *)v12 - 5) = v14;
        *((_QWORD *)v12 - 4) = 0;
        *(v12 - 24) = 0;
        *(v12 - 8) = 0;
      }
      while ( v13 );
      this->end = &end[488 * a];
      v2 = v44;
      v3 = v45;
    }
  }
  result = v50 ^ _stack_chk_guard;
  if ( v50 != _stack_chk_guard )
  {
    v44 = v2;
    v45 = v3;
    v46 = v4;
    v47 = v5;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000ae180  size 380 bytes  94 insns
 *   std::vector<float, apac::AlignedAllocator<float, 16ul> >::_M_default_append(unsigned long)
 *   [_ZNSt6vectorIfN4apac16AlignedAllocatorIfLm16EEEE17_M_default_appendEm]
 *   data refs: 0x30e7a0, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<float,apac::AlignedAllocator<float,16ul>>::_M_default_append(
        std_vector *this,
        unsigned __int64 a)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  _BYTE *end; // x1
  __int64 v8; // x20
  unsigned __int64 v9; // x25
  __int64 v10; // x2
  __int64 result; // x0
  unsigned __int64 v12; // x24
  unsigned __int64 v13; // x24
  size_t v14; // x24
  int v15; // w0
  _DWORD *v16; // x21
  __int64 v17; // x1
  _BYTE *begin; // x0
  char *v19; // x3
  __int64 v20; // x1
  unsigned __int64 v21; // x3
  bool v22; // zf
  __int64 v23; // [xsp+10h] [xbp+10h]
  __int64 v24; // [xsp+18h] [xbp+18h]
  __int64 v25; // [xsp+40h] [xbp+40h]
  void *memptr; // [xsp+50h] [xbp+50h] BYREF
  __int64 v27; // [xsp+58h] [xbp+58h]
  __int64 v28; // [xsp+60h] [xbp+60h]
  if ( a )
  {
    end = this->end;
    v8 = end - (char *)this->begin;
    v9 = v8 >> 2;
    if ( a > ((char *)this->cap - (char *)end) >> 2 )
    {
      if ( 0x1FFFFFFFFFFFFFFFLL - (v8 >> 2) < a )
        std::__throw_length_error("vector::_M_default_append");
      if ( a < v9 )
        v12 = v8 >> 2;
      else
        v12 = a;
      v13 = v9 + v12;
      if ( v13 > 0x1FFFFFFFFFFFFFFFLL )
        v13 = 0x1FFFFFFFFFFFFFFFLL;
      v14 = v13;
      v15 = posix_memalign(&memptr, 0x10u, v14 * 4);
      v16 = memptr;
      v17 = 0;
      if ( v15 )
        v16 = nullptr;
      do
        *(_DWORD *)((char *)&v16[v17++] + v8) = 0;
      while ( v17 != a );
      begin = this->begin;
      v19 = (char *)this->end;
      if ( this->begin != v19 )
      {
        v20 = 0;
        v21 = (unsigned __int64)(v19 - 4 - begin) >> 2;
        do
        {
          v22 = v21 == v20;
          v16[v20] = *(_DWORD *)&begin[4 * v20];
          ++v20;
        }
        while ( !v22 );
        v19 = (char *)this->begin;
      }
      if ( v19 )
        free(v19);
      this->cap = &v16[v14];
      this->begin = v16;
      this->end = &v16[a + v9];
      v2 = v23;
      v3 = v24;
      v4 = v25;
    }
    else
    {
      v10 = 0;
      do
        *(_DWORD *)&end[4 * v10++] = 0;
      while ( a != v10 );
      this->end = &end[4 * a];
      v2 = v23;
      v3 = v24;
      v4 = v25;
    }
  }
  result = v27 ^ _stack_chk_guard;
  if ( v27 != _stack_chk_guard )
  {
    v23 = v2;
    v24 = v3;
    v25 = v4;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000ae300  size 404 bytes  100 insns
 *   void std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >::_M_realloc_insert<apac::ElementInstance<APACSyntacticElement*> >(__gnu_cxx::__normal_iterator<apac::ElementInstance<APACSyntacticElement*>*, std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > > >, apac::ElementInstance<APACSyntacticElement*>&&)
 *   [_ZNSt6vectorIN4apac15ElementInstanceIP20APACSyntacticElementEESaIS4_EE17_M_realloc_insertIJS4_EEEvN9__gnu_cxx17__normal_iteratorIPS4_S6_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<apac::ElementInstance<APACSyntacticElement *>>::_M_realloc_insert<apac::ElementInstance<APACSyntacticElement *>>(
        std_vector *this,
        void *a,
        apac__ElementInstance *a2)
{
  char *end; // x20
  _BYTE *begin; // x24
  __int64 v7; // x1
  unsigned __int64 v9; // x0
  char *v10; // x28
  bool v11; // cc
  unsigned __int64 v12; // x27
  unsigned __int64 v13; // x0
  char *v14; // x22
  char *v15; // x27
  char *v16; // x21
  char *v17; // x3
  _DWORD *v18; // x2
  int v19; // w4
  __int64 v20; // x1
  char *v21; // x2
  char *v22; // x3
  int v23; // t1
  __int64 v25; // [xsp+68h] [xbp+68h]
  __int64 v26; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = (char *)this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 4 == 0x7FFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 4;
  v10 = (char *)((_BYTE *)a - begin);
  if ( !(v7 >> 4) )
  {
    v12 = 16;
    goto LABEL_5;
  }
  v11 = v9 > 2 * v9;
  v12 = 0x7FFFFFFFFFFFFFF0LL;
  v13 = 2 * v9;
  if ( v11 )
  {
LABEL_5:
    v14 = (char *)operator new(v12);
    v15 = &v14[v12];
    v16 = v14 + 16;
    goto LABEL_6;
  }
  if ( v13 )
  {
    if ( v13 > 0x7FFFFFFFFFFFFFFLL )
      v13 = 0x7FFFFFFFFFFFFFFLL;
    v12 = 16 * v13;
    goto LABEL_5;
  }
  v16 = (char *)&word_10;
  v15 = nullptr;
  v14 = nullptr;
LABEL_6:
  *(_DWORD *)&v10[(_QWORD)v14] = *(_DWORD *)a2;
  *(_QWORD *)&v10[(_QWORD)v14 + 8] = *((_QWORD *)a2 + 1);
  if ( a != begin )
  {
    v17 = v14;
    v18 = begin;
    do
    {
      v19 = *v18;
      v18 += 4;
      v20 = *((_QWORD *)v18 - 1);
      *(_DWORD *)v17 = v19;
      *((_QWORD *)v17 + 1) = v20;
      v17 += 16;
    }
    while ( a != v18 );
    v16 = &v14[(_BYTE *)a - begin + 16];
  }
  if ( a != end )
  {
    v21 = (char *)a;
    v22 = v16;
    do
    {
      *((_QWORD *)v22 + 1) = *((_QWORD *)v21 + 1);
      v23 = *(_DWORD *)v21;
      v21 += 16;
      v22 += 16;
      *((_DWORD *)v22 - 4) = v23;
    }
    while ( v21 != end );
    v16 += v21 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v14;
  this->end = v16;
  this->cap = v15;
  return v25 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000ae4a0  size 136 bytes  34 insns
 *   void std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >::emplace_back<apac::ElementInstance<APACSyntacticElement*> >(apac::ElementInstance<APACSyntacticElement*>&&)
 *   [_ZNSt6vectorIN4apac15ElementInstanceIP20APACSyntacticElementEESaIS4_EE12emplace_backIJS4_EEEvDpOT_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<apac::ElementInstance<APACSyntacticElement *>>::emplace_back<apac::ElementInstance<APACSyntacticElement *>>(
        std_vector *this,
        apac__ElementInstance *a)
{
  _QWORD *end; // x1
  __int64 v5; // x0
  __int64 result; // x0
  __int64 v7; // [xsp+20h] [xbp+20h]
  end = this->end;
  if ( end == this->cap )
    return std::vector<apac::ElementInstance<APACSyntacticElement *>>::_M_realloc_insert<apac::ElementInstance<APACSyntacticElement *>>(
             this,
             end,
             a);
  v5 = *((_QWORD *)a + 1);
  *(_DWORD *)end = *(_DWORD *)a;
  end[1] = v5;
  result = (__int64)this->end + 16;
  this->end = (void *)result;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000af7e0  size 408 bytes  101 insns
 *   void std::vector<std::tuple<apac::AudioDataType, apac::ASCChannelRange>, std::allocator<std::tuple<apac::AudioDataType, apac::ASCChannelRange> > >::_M_realloc_insert<std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&>(__gnu_cxx::__normal_iterator<std::tuple<apac::AudioDataType, apac::ASCChannelRange>*, std::vector<std::tuple<apac::AudioDataType, apac::ASCChannelRange>, std::allocator<std::tuple<apac::AudioDataType, apac::ASCChannelRange> > > >, std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&)
 *   [_ZNSt6vectorISt5tupleIJN4apac13AudioDataTypeENS1_15ASCChannelRangeEEESaIS4_EE17_M_realloc_insertIJRKS4_EEEvN9__gnu_cxx17__normal_iteratorIPS4_S6_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<std::tuple<apac::AudioDataType,apac::ASCChannelRange>>::_M_realloc_insert<std::tuple<apac::AudioDataType,apac::ASCChannelRange> const&>(
        std_vector *this,
        void *a,
        const void *a2)
{
  char *end; // x20
  _BYTE *begin; // x24
  __int64 v7; // x1
  unsigned __int64 v9; // x0
  bool v10; // cc
  unsigned __int64 v11; // x26
  unsigned __int64 v12; // x0
  char *v13; // x23
  char *v14; // x26
  char *v15; // x21
  char *v16; // x4
  _WORD *v17; // x3
  __int16 v18; // w1
  char *v19; // x2
  char *v20; // x3
  __int16 v21; // w4
  int v22; // w1
  __int64 v24; // [xsp+68h] [xbp+68h]
  __int64 v25; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = (char *)this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 3 == 0xFFFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 3;
  if ( !(v7 >> 3) )
  {
    v11 = 8;
    goto LABEL_5;
  }
  v10 = v9 > 2 * v9;
  v11 = 0x7FFFFFFFFFFFFFF8LL;
  v12 = 2 * v9;
  if ( v10 )
  {
LABEL_5:
    v13 = (char *)operator new(v11);
    v14 = &v13[v11];
    v15 = v13 + 8;
    goto LABEL_6;
  }
  if ( v12 )
  {
    if ( v12 > 0xFFFFFFFFFFFFFFFLL )
      v12 = 0xFFFFFFFFFFFFFFFLL;
    v11 = 8 * v12;
    goto LABEL_5;
  }
  v15 = byte_8;
  v14 = nullptr;
  v13 = nullptr;
LABEL_6:
  *(_QWORD *)&v13[(_BYTE *)a - begin] = *(_QWORD *)a2;
  if ( a != begin )
  {
    v16 = v13;
    v17 = begin;
    do
    {
      v18 = v17[1];
      v17 += 4;
      v16 += 8;
      *((_WORD *)v16 - 4) = *(v17 - 4);
      *((_WORD *)v16 - 3) = v18;
      *((_DWORD *)v16 - 1) = *((_DWORD *)v17 - 1);
    }
    while ( a != v17 );
    v15 = &v13[(_BYTE *)a - begin + 8];
  }
  if ( a != end )
  {
    v19 = (char *)a;
    v20 = v15;
    do
    {
      v21 = *((_WORD *)v19 + 1);
      v19 += 8;
      v20 += 8;
      v22 = *((_DWORD *)v19 - 1);
      *((_WORD *)v20 - 4) = *((_WORD *)v19 - 4);
      *((_WORD *)v20 - 3) = v21;
      *((_DWORD *)v20 - 1) = v22;
    }
    while ( v19 != end );
    v15 += v19 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v13;
  this->end = v15;
  this->cap = v14;
  return v24 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b1240  size 496 bytes  123 insns
 *   void std::vector<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >, std::allocator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > > >::_M_realloc_insert<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >*, std::vector<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >, std::allocator<std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> > > > >, std::unique_ptr<apac::LPMDCTFilter, std::default_delete<apac::LPMDCTFilter> >&&)
 *   [_ZNSt6vectorISt10unique_ptrIN4apac12LPMDCTFilterESt14default_deleteIS2_EESaIS5_EE17_M_realloc_insertIJS5_EEEvN9__gnu_cxx17__normal_iteratorIPS5_S7_EEDpOT_]
 *   data refs: 0x79940, 0x30e8b8, 0x42a828, 0x42b700, 0x42ce00, 0x42d2a8, 0x42d4b0, 0x42dbe8, 0x43e520
 */
__int64 __fastcall std::vector<std::unique_ptr<apac::LPMDCTFilter>>::_M_realloc_insert<std::unique_ptr<apac::LPMDCTFilter>>(
        std_vector *this,
        void *a,
        void **a2)
{
  _BYTE *end; // x21
  apac__LPMDCTFilter **begin; // x24
  __int64 v7; // x2
  unsigned __int64 v9; // x0
  char *v10; // x27
  bool v11; // cc
  unsigned __int64 v12; // x26
  unsigned __int64 v13; // x0
  void *v14; // x23
  char *v15; // x19
  apac__LPMDCTFilter **v16; // x26
  apac__LPMDCTFilter **v17; // x19
  apac__LPMDCTFilter *v18; // x0
  apac__LPMDCTFilter *v19; // x20
  void (__fastcall *v20)(apac__LPMDCTFilter *); // x2
  apac__LPMDCTFilter *v21; // x0
  char *v22; // x2
  _QWORD *v23; // x0
  __int64 v24; // t1
  char *v26; // [xsp+68h] [xbp+68h]
  __int64 v27; // [xsp+78h] [xbp+78h]
  __int64 v28; // [xsp+80h] [xbp+80h]
  begin = (apac__LPMDCTFilter **)this->begin;
  end = this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 3 == 0xFFFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 3;
  v10 = (char *)((_BYTE *)a - (_BYTE *)begin);
  if ( v7 >> 3 )
  {
    v11 = v9 > 2 * v9;
    v12 = 0x7FFFFFFFFFFFFFF8LL;
    v13 = 2 * v9;
    if ( !v11 )
    {
      if ( !v13 )
      {
        v15 = byte_8;
        v14 = nullptr;
        v26 = nullptr;
        goto LABEL_6;
      }
      if ( v13 > 0xFFFFFFFFFFFFFFFLL )
        v13 = 0xFFFFFFFFFFFFFFFLL;
      v12 = 8 * v13;
    }
  }
  else
  {
    v12 = 8;
  }
  v14 = (void *)operator new(v12);
  v15 = (char *)v14 + 8;
  v26 = (char *)v14 + v12;
LABEL_6:
  *(_QWORD *)&v10[(_QWORD)v14] = *a2;
  *a2 = nullptr;
  if ( a == begin )
    goto LABEL_14;
  v16 = (apac__LPMDCTFilter **)v14;
  v17 = begin;
  do
  {
    while ( 1 )
    {
      v18 = *v17;
      *v17 = nullptr;
      *v16 = v18;
      v19 = *v17;
      if ( !*v17 )
        goto LABEL_9;
      v20 = *(void (__fastcall **)(apac__LPMDCTFilter *))(*(_QWORD *)v19 + 8LL);
      if ( v20 != apac::LPMDCTFilter::~LPMDCTFilter )
        break;
      *(_QWORD *)v19 = off_42B710;
      *((_QWORD *)v19 + 2) = off_42A838;
      DFTHandle<false>::~DFTHandle((char *)v19 + 16);
      operator delete(v19);
LABEL_9:
      ++v17;
      ++v16;
      if ( a == v17 )
        goto LABEL_13;
    }
    v21 = *v17++;
    v20(v21);
    ++v16;
  }
  while ( a != v17 );
LABEL_13:
  v15 = (char *)v14 + (_BYTE *)a - (_BYTE *)begin + 8;
LABEL_14:
  if ( a != end )
  {
    v22 = (char *)a;
    v23 = v15;
    do
    {
      v24 = *(_QWORD *)v22;
      v22 += 8;
      *v23++ = v24;
    }
    while ( v22 != end );
    v15 += v22 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v14;
  this->end = v15;
  this->cap = v26;
  return v27 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b30e0  size 716 bytes  174 insns
 *   void std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> >::_M_realloc_insert<apac::ASCAudioOutData>(__gnu_cxx::__normal_iterator<apac::ASCAudioOutData*, std::vector<apac::ASCAudioOutData, std::allocator<apac::ASCAudioOutData> > >, apac::ASCAudioOutData&&)
 *   [_ZNSt6vectorIN4apac15ASCAudioOutDataESaIS1_EE17_M_realloc_insertIJS1_EEEvN9__gnu_cxx17__normal_iteratorIPS1_S3_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<apac::ASCAudioOutData>::_M_realloc_insert<apac::ASCAudioOutData>(
        std_vector *this,
        void *a,
        apac__ASCAudioOutData *a2)
{
  _BYTE *begin; // x22
  char *end; // x27
  unsigned __int64 v7; // x0
  char *v9; // x28
  bool v10; // cc
  unsigned __int64 v11; // x24
  unsigned __int64 v12; // x0
  char *v13; // x20
  char *v14; // x24
  char *v15; // x19
  char *v16; // x0
  int v17; // w5
  __int64 v18; // x3
  __int64 v19; // x7
  __int64 v20; // x6
  __int64 v21; // x5
  int v22; // w4
  __int64 v23; // x1
  __int64 v24; // x0
  char *v25; // x25
  _DWORD *v26; // x19
  int v27; // w0
  void *v28; // x0
  char *v29; // x2
  char *v30; // x0
  int v31; // w5
  __int64 v32; // x3
  __int64 v33; // x1
  int v34; // w5
  __int64 v35; // x3
  __int64 v36; // x4
  __int64 v37; // x1
  __int64 v39; // [xsp+68h] [xbp+68h]
  __int64 v40; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = (char *)this->end;
  v7 = 0x8E38E38E38E38E39LL * ((end - (char *)this->begin) >> 3);
  if ( v7 == 0x1C71C71C71C71C7LL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = (char *)((_BYTE *)a - begin);
  if ( !v7 )
  {
    v11 = 72;
    goto LABEL_5;
  }
  v10 = v7 > 2 * v7;
  v11 = 0x7FFFFFFFFFFFFFF8LL;
  v12 = 2 * v7;
  if ( v10 )
  {
LABEL_5:
    v13 = (char *)operator new(v11);
    v14 = &v13[v11];
    v15 = v13 + 72;
    goto LABEL_6;
  }
  if ( v12 )
  {
    if ( v12 > 0x1C71C71C71C71C7LL )
      v12 = 0x1C71C71C71C71C7LL;
    v11 = 72 * v12;
    goto LABEL_5;
  }
  v15 = (char *)&qword_48;
  v14 = nullptr;
  v13 = nullptr;
LABEL_6:
  v16 = &v9[(_QWORD)v13];
  v17 = *((_DWORD *)a2 + 2);
  v18 = (__int64)&v9[(_QWORD)v13 + 48];
  v19 = *((_QWORD *)a2 + 2);
  *(_QWORD *)&v9[(_QWORD)v13] = *(_QWORD *)a2;
  *((_DWORD *)v16 + 2) = v17;
  v20 = *((_QWORD *)a2 + 3);
  *((_QWORD *)v16 + 2) = v19;
  *((_QWORD *)v16 + 3) = v20;
  v21 = *((_QWORD *)a2 + 4);
  *((_QWORD *)a2 + 3) = 0;
  *((_QWORD *)a2 + 4) = 0;
  v22 = *((_DWORD *)a2 + 10);
  v23 = *((_QWORD *)a2 + 6);
  *((_QWORD *)v16 + 4) = v21;
  *((_DWORD *)v16 + 10) = v22;
  *((_QWORD *)v16 + 6) = v23;
  v24 = *((_QWORD *)a2 + 8);
  *(_QWORD *)(v18 + 8) = *((_QWORD *)a2 + 7);
  *(_QWORD *)(v18 + 16) = v24;
  *((_QWORD *)a2 + 2) = 0;
  *((_QWORD *)a2 + 6) = 0;
  *((_QWORD *)a2 + 7) = 0;
  *((_QWORD *)a2 + 8) = 0;
  if ( a != begin )
  {
    v25 = v13;
    v26 = begin;
    do
    {
      *(_DWORD *)v25 = *v26;
      *((_DWORD *)v25 + 1) = v26[1];
      *((_DWORD *)v25 + 2) = v26[2];
      *((_QWORD *)v25 + 2) = *((_QWORD *)v26 + 2);
      *((_QWORD *)v25 + 3) = *((_QWORD *)v26 + 3);
      *((_QWORD *)v25 + 4) = *((_QWORD *)v26 + 4);
      *((_QWORD *)v26 + 2) = 0;
      *((_QWORD *)v26 + 3) = 0;
      v27 = v26[10];
      *((_QWORD *)v26 + 4) = 0;
      *((_DWORD *)v25 + 10) = v27;
      *((_QWORD *)v25 + 6) = *((_QWORD *)v26 + 6);
      *((_QWORD *)v25 + 7) = *((_QWORD *)v26 + 7);
      *((_QWORD *)v25 + 8) = *((_QWORD *)v26 + 8);
      v28 = *((void **)v26 + 2);
      if ( v28 )
        operator delete(v28);
      v26 += 18;
      v25 += 72;
    }
    while ( a != v26 );
    v15 = &v13[72 * ((0xE38E38E38E38E39LL * ((unsigned __int64)((char *)a - 72 - begin) >> 3)) & 0x1FFFFFFFFFFFFFFFLL)
             + 144];
  }
  if ( a != end )
  {
    v29 = (char *)a;
    v30 = v15;
    do
    {
      v31 = *((_DWORD *)v29 + 1);
      *(_DWORD *)v30 = *(_DWORD *)v29;
      *((_DWORD *)v30 + 1) = v31;
      v32 = *((_QWORD *)v29 + 2);
      *((_DWORD *)v30 + 2) = *((_DWORD *)v29 + 2);
      v33 = *((_QWORD *)v29 + 3);
      *((_QWORD *)v30 + 2) = v32;
      v34 = *((_DWORD *)v29 + 10);
      *((_QWORD *)v30 + 3) = v33;
      *((_QWORD *)v30 + 4) = *((_QWORD *)v29 + 4);
      v36 = *((_QWORD *)v29 + 6);
      v35 = *((_QWORD *)v29 + 7);
      *((_DWORD *)v30 + 10) = v34;
      v37 = *((_QWORD *)v29 + 8);
      *((_QWORD *)v30 + 6) = v36;
      *((_QWORD *)v30 + 7) = v35;
      v29 += 72;
      *((_QWORD *)v30 + 8) = v37;
      v30 += 72;
    }
    while ( v29 != end );
    v15 += 72 * ((0xE38E38E38E38E39LL * ((unsigned __int64)(v29 - (_BYTE *)a - 72) >> 3)) & 0x1FFFFFFFFFFFFFFFLL) + 72;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v13;
  this->end = v15;
  this->cap = v14;
  return v39 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5fb0  size 416 bytes  103 insns
 *   void std::vector<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >, std::allocator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > > >::_M_realloc_insert<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >*, std::vector<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >, std::allocator<std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> > > > >, std::unique_ptr<apac::LPCodeBook, std::default_delete<apac::LPCodeBook> >&&)
 *   [_ZNSt6vectorISt10unique_ptrIN4apac10LPCodeBookESt14default_deleteIS2_EESaIS5_EE17_M_realloc_insertIJS5_EEEvN9__gnu_cxx17__normal_iteratorIPS5_S7_EEDpOT_]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
__int64 __fastcall std::vector<std::unique_ptr<apac::LPCodeBook>>::_M_realloc_insert<std::unique_ptr<apac::LPCodeBook>>(
        std_vector *this,
        void *a,
        void **a2)
{
  _BYTE *begin; // x24
  _BYTE *end; // x27
  __int64 v7; // x1
  unsigned __int64 v9; // x0
  bool v10; // cc
  unsigned __int64 v11; // x26
  unsigned __int64 v12; // x0
  char *v13; // x23
  char *v14; // x26
  char *v15; // x19
  _QWORD *v16; // x28
  _QWORD *v17; // x19
  _QWORD *v18; // x0
  _QWORD *v19; // x20
  void *v20; // x0
  void *v21; // x0
  char *v22; // x2
  char *v23; // x0
  __int64 v24; // t1
  __int64 v26; // [xsp+68h] [xbp+68h]
  __int64 v27; // [xsp+70h] [xbp+70h]
  begin = this->begin;
  end = this->end;
  v7 = end - (char *)this->begin;
  if ( v7 >> 3 == 0xFFFFFFFFFFFFFFFLL )
    std::__throw_length_error("vector::_M_realloc_insert");
  v9 = v7 >> 3;
  if ( !(v7 >> 3) )
  {
    v11 = 8;
    goto LABEL_5;
  }
  v10 = v9 > 2 * v9;
  v11 = 0x7FFFFFFFFFFFFFF8LL;
  v12 = 2 * v9;
  if ( v10 )
  {
LABEL_5:
    v13 = (char *)operator new(v11);
    v14 = &v13[v11];
    v15 = v13 + 8;
    goto LABEL_6;
  }
  if ( v12 )
  {
    if ( v12 > 0xFFFFFFFFFFFFFFFLL )
      v12 = 0xFFFFFFFFFFFFFFFLL;
    v11 = 8 * v12;
    goto LABEL_5;
  }
  v15 = byte_8;
  v14 = nullptr;
  v13 = nullptr;
LABEL_6:
  *(_QWORD *)&v13[(_BYTE *)a - begin] = *a2;
  *a2 = nullptr;
  if ( a != begin )
  {
    v16 = v13;
    v17 = begin;
    do
    {
      v18 = (_QWORD *)*v17;
      *v17 = 0;
      *v16 = v18;
      v19 = (_QWORD *)*v17;
      if ( *v17 )
      {
        v20 = (void *)v19[6];
        if ( v20 )
          operator delete(v20);
        v21 = (void *)v19[2];
        if ( v21 )
          operator delete(v21);
        operator delete(v19);
      }
      ++v17;
      ++v16;
    }
    while ( a != v17 );
    v15 = &v13[(_BYTE *)a - begin + 8];
  }
  if ( a != end )
  {
    v22 = (char *)a;
    v23 = v15;
    do
    {
      v24 = *(_QWORD *)v22;
      v22 += 8;
      *(_QWORD *)v23 = v24;
      v23 += 8;
    }
    while ( v22 != end );
    v15 += v22 - (_BYTE *)a;
  }
  if ( begin )
    operator delete(begin);
  this->begin = v13;
  this->end = v15;
  this->cap = v14;
  return v26 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00106f50  size 168 bytes  42 insns
 *   std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector()
 *   std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector()
 *   [_ZNSt6vectorISt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS2_EESaIS5_EED2Ev]
 *   [_ZNSt6vectorISt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS2_EESaIS5_EED1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZNSt6vectorISt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS2_EESaIS5_EED1Ev'
void __fastcall std::vector<std::unique_ptr<apac::IASCConfig>>::~vector(std_vector *this)
{
  _QWORD *begin; // x19
  _QWORD *end; // x20
  __int64 v4; // [xsp+40h] [xbp+40h]
  begin = this->begin;
  end = this->end;
  if ( this->begin != end )
  {
    do
    {
      if ( *begin )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*begin + 8LL))(*begin);
      ++begin;
    }
    while ( end != begin );
    end = this->begin;
  }
  if ( end )
    operator delete(end);
}


