/* ============================================================================
 * APACSyntacticElement
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 7 function(s), 2556 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000acc20    176  APACSyntacticElement::DestroyList(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >&) */
/* 0x000accd0    132  APACSyntacticElement::NumberOfChannels(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > > const&) */
/* 0x000acd60   1620  APACSyntacticElement::CreateList(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >&, APACInstanceConfig&, apac::observer_ptr<apac::IASCConfig>) */
/* 0x000ad3c0     72  APACSyntacticElement::ChannelData(bool) */
/* 0x000ad410     68  APACSyntacticElement::GetICSInfo(bool) */
/* 0x000adf90    272  APACSyntacticElement::~APACSyntacticElement() */
/* 0x000ae0a0    216  APACSyntacticElement::~APACSyntacticElement() */

/* ------------------------------------------------------------------------ */
/* 0x000acc20  size 176 bytes  44 insns
 *   APACSyntacticElement::DestroyList(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >&)
 *   [_ZN20APACSyntacticElement11DestroyListERSt6vectorIN4apac15ElementInstanceIPS_EESaIS4_EE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSyntacticElement::DestroyList(APACSyntacticElement *this, std_vector *vec)
{
  __int64 v3; // x3
  __int64 v4; // x4
  __int64 v5; // x1
  int v6; // w19
  __int64 v7; // x0
  __int64 v9; // [xsp+38h] [xbp+38h]
  __int64 v10; // [xsp+40h] [xbp+40h]
  v3 = *(_QWORD *)this;
  v4 = *((_QWORD *)this + 1);
  if ( *(_QWORD *)this != v4 )
  {
    v5 = 0;
    v6 = 0;
    do
    {
      v7 = *(_QWORD *)(v3 + 16 * v5 + 8);
      if ( v7 )
      {
        (*(void (__fastcall **)(__int64))(*(_QWORD *)v7 + 8LL))(v7);
        v3 = *(_QWORD *)this;
        v4 = *((_QWORD *)this + 1);
      }
      v5 = (unsigned int)(v6 + 1);
      v6 = v5;
    }
    while ( (v4 - v3) >> 4 > (unsigned __int64)(unsigned int)v5 );
    if ( v3 != v4 )
      *((_QWORD *)this + 1) = v3;
  }
  return v9 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000accd0  size 132 bytes  33 insns
 *   APACSyntacticElement::NumberOfChannels(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > > const&)
 *   [_ZN20APACSyntacticElement16NumberOfChannelsERKSt6vectorIN4apac15ElementInstanceIPS_EESaIS4_EE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSyntacticElement::NumberOfChannels(APACSyntacticElement *this, const std_vector *vec)
{
  __int64 v2; // x1
  __int64 v3; // x3
  __int64 result; // x0
  int v5; // w2
  __int64 v6; // [xsp+20h] [xbp+20h]
  v2 = *(_QWORD *)this;
  v3 = *((_QWORD *)this + 1);
  for ( result = 0; v3 != v2; v2 += 16 )
  {
    v5 = *(_DWORD *)(*(_QWORD *)(v2 + 8) + 8LL);
    if ( v5 != 6 )
      result = (unsigned int)result + (v5 == 1) + 1;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000acd60  size 1620 bytes  405 insns
 *   APACSyntacticElement::CreateList(std::vector<apac::ElementInstance<APACSyntacticElement*>, std::allocator<apac::ElementInstance<APACSyntacticElement*> > >&, APACInstanceConfig&, apac::observer_ptr<apac::IASCConfig>)
 *   [_ZN20APACSyntacticElement10CreateListERSt6vectorIN4apac15ElementInstanceIPS_EESaIS4_EER18APACInstanceConfigNS1_12observer_ptrINS1_10IASCConfigEEE]
 *   data refs: 0x42b010, 0x42b040, 0x42b468, 0x42b4a8, 0x42b4e8, 0x42b528, 0x42ce00, 0x42cf58, 0x42d0b8, 0x42d180, 0x42da00, 0x42de08, 0x42de98, 0x43e520
 */
__int64 __fastcall APACSyntacticElement::CreateList(
        APACSyntacticElement *this,
        std_vector *vec,
        APACInstanceConfig *instCfg,
        void *a)
{
  __int64 v4; // x25
  __int64 v5; // x26
  __int64 v6; // x27
  __int64 v7; // x28
  __int64 v11; // x21
  __int64 v12; // x23
  int v13; // w26
  int v14; // w27
  int v15; // w1
  __int64 result; // x0
  __int64 v17; // x20
  _WORD *cap; // x2
  __int16 v19; // w28
  int v20; // w1
  __int64 v21; // x0
  __int64 *v22; // x2
  __int64 v23; // x28
  _QWORD *v24; // x3
  int v25; // w2
  __int64 v26; // x0
  __int64 v27; // x2
  __int64 v28; // x1
  __int64 v29; // x0
  __int64 v30; // x2
  __int64 v31; // x1
  _WORD *v32; // x1
  __int16 v33; // w28
  __int64 v34; // x0
  char v35; // w2
  __int64 v36; // x28
  _QWORD *v37; // x3
  int v38; // w2
  int v39; // w2
  int v40; // w2
  __int64 v41; // x0
  _WORD *v42; // x2
  __int64 v43; // x20
  __int16 v44; // w28
  __int64 v45; // x0
  char v46; // w2
  __int64 v47; // x28
  _QWORD *v48; // x3
  int v49; // w2
  int v50; // w4
  int v51; // w2
  __int64 v52; // x20
  _WORD *v53; // x3
  int v54; // w1
  __int16 v55; // w28
  __int64 v56; // x0
  char v57; // w2
  __int64 v58; // x28
  _QWORD *v59; // x3
  int v60; // w2
  int v61; // w3
  int v62; // w3
  __int64 v63; // x0
  __int64 v64; // x0
  __int64 v65; // [xsp+40h] [xbp+40h]
  __int64 v66; // [xsp+48h] [xbp+48h]
  __int64 v67; // [xsp+50h] [xbp+50h]
  __int64 v68; // [xsp+58h] [xbp+58h]
  void *v69; // [xsp+60h] [xbp+60h]
  __int64 *v70; // [xsp+68h] [xbp+68h]
  int v71[2]; // [xsp+70h] [xbp+70h]
  int v72; // [xsp+78h] [xbp+78h]
  int v73; // [xsp+7Ch] [xbp+7Ch]
  int v74; // [xsp+88h] [xbp+88h] BYREF
  __int64 v75; // [xsp+90h] [xbp+90h]
  __int64 v76; // [xsp+98h] [xbp+98h]
  __int64 v77; // [xsp+A0h] [xbp+A0h]
  APACSyntacticElement::DestroyList(this, nullptr);
  v11 = *((_QWORD *)instCfg + 2);
  v12 = *((_QWORD *)instCfg + 3);
  if ( v11 == v12 )
    goto LABEL_20;
  v66 = v5;
  v13 = 0;
  v67 = v6;
  v68 = v7;
  v14 = 0;
  v69 = &unk_42B478;
  v70 = &`vtable for'APACICSInfo;
  v72 = 0;
  do
  {
    v15 = *(_DWORD *)(v11 + 8);
    if ( v15 == 1 )
    {
      v73 = 1;
      v17 = operator new(0x210u);
      cap = vec->cap;
      v19 = cap[4];
      v71[0] = (unsigned __int16)cap[12];
      v20 = v73;
      *(_QWORD *)v17 = v69;
      *(_DWORD *)(v17 + 8) = v20;
      v21 = operator new(0x18u);
      v22 = v70;
      *(_QWORD *)(v17 + 16) = v21;
      *(_QWORD *)v21 = v22 + 2;
      LOBYTE(v22) = v71[0];
      *(_BYTE *)(v21 + 8) = v19;
      *(_BYTE *)(v21 + 9) = (_BYTE)v22;
      *(_WORD *)(v21 + 10) = 0;
      *(_BYTE *)(v21 + 12) = 0;
      *(_DWORD *)(v21 + 16) = 0;
      v23 = operator new(0x308u);
      v24 = vec->cap;
      *(_BYTE *)(v23 + 8) = 1;
      *(_QWORD *)(v23 + 16) = vec;
      *(_QWORD *)v23 = off_42B050;
      v25 = *(unsigned __int16 *)(v24[2] + 2LL * WORD1(vec->end));
      *(_QWORD *)(v23 + 752) = 0;
      *(_QWORD *)(v23 + 760) = 0;
      *(_QWORD *)(v23 + 768) = 0;
      if ( v25 )
        std::vector<float,apac::AlignedAllocator<float,16ul>>::_M_default_append(
          (std_vector *)(v23 + 752),
          (unsigned __int16)v25);
      *(_QWORD *)(v17 + 24) = v23;
      *(_QWORD *)(v17 + 32) = vec;
      *(_QWORD *)(v17 + 40) = 0;
      *(_QWORD *)v71 = v17 + 72;
      *(_QWORD *)v17 = off_42B538;
      *(_QWORD *)(v17 + 48) = 0;
      *(_QWORD *)(v17 + 56) = 0;
      *(_QWORD *)(v17 + 72) = 0;
      *(_QWORD *)(v17 + 80) = 0;
      *(_QWORD *)(v17 + 88) = 0;
      *(_QWORD *)(v17 + 96) = 0;
      *(_QWORD *)(v17 + 104) = 0;
      *(_QWORD *)(v17 + 112) = 0;
      *(_QWORD *)(v17 + 120) = vec;
      CacQuantizer::Initialize((CacQuantizer *)(v17 + 40));
      v26 = *(_QWORD *)(v17 + 72);
      v27 = *(_QWORD *)(v17 + 80);
      v28 = v27 - v26;
      if ( (unsigned __int64)(v27 - v26) <= 0x77 )
      {
        std::vector<unsigned char>::_M_default_append(v71[0], 120 - v28);
      }
      else if ( v28 != 120 )
      {
        v64 = v26 + 120;
        if ( v27 != v64 )
        {
          *(_QWORD *)(v17 + 80) = v64;
          v29 = *(_QWORD *)(v17 + 96);
          v30 = *(_QWORD *)(v17 + 104);
          v31 = v30 - v29;
          if ( (unsigned __int64)(v30 - v29) <= 0x77 )
            goto LABEL_31;
          goto LABEL_14;
        }
      }
      v29 = *(_QWORD *)(v17 + 96);
      v30 = *(_QWORD *)(v17 + 104);
      v31 = v30 - v29;
      if ( (unsigned __int64)(v30 - v29) <= 0x77 )
      {
LABEL_31:
        std::vector<unsigned char>::_M_default_append(v17 + 96, 120 - v31);
LABEL_15:
        APACTNSData::APACTNSData((APACTNSData *)(v17 + 128), (APACInstanceConfig *)vec);
        APACTNSData::APACTNSData((APACTNSData *)(v17 + 318), (APACInstanceConfig *)vec);
        v32 = vec->cap;
        v33 = v32[4];
        v71[0] = (unsigned __int16)v32[12];
        v34 = operator new(0x18u);
        *(_QWORD *)(v17 + 512) = v34;
        *(_QWORD *)v34 = off_42B020;
        v35 = v71[0];
        *(_BYTE *)(v34 + 8) = v33;
        *(_BYTE *)(v34 + 9) = v35;
        *(_WORD *)(v34 + 10) = 0;
        *(_BYTE *)(v34 + 12) = 0;
        *(_DWORD *)(v34 + 16) = 0;
        v36 = operator new(0x308u);
        v37 = vec->cap;
        *(_BYTE *)(v36 + 8) = 0;
        *(_QWORD *)(v36 + 16) = vec;
        *(_QWORD *)v36 = off_42B050;
        v38 = *(unsigned __int16 *)(v37[2] + 2LL * WORD1(vec->end));
        *(_QWORD *)(v36 + 752) = 0;
        *(_QWORD *)(v36 + 760) = 0;
        *(_QWORD *)(v36 + 768) = 0;
        if ( v38 )
          std::vector<float,apac::AlignedAllocator<float,16ul>>::_M_default_append(
            (std_vector *)(v36 + 752),
            (unsigned __int16)v38);
        v39 = *(_DWORD *)(v17 + 8);
        *(_QWORD *)(v17 + 520) = v36;
        v75 = v17;
        v40 = v39 | (v14++ << 8);
        v74 = v40;
        std::vector<apac::ElementInstance<APACSyntacticElement *>>::emplace_back<apac::ElementInstance<APACSyntacticElement *>>(
          (std_vector *)this,
          (apac__ElementInstance *)&v74);
        goto LABEL_18;
      }
LABEL_14:
      if ( v31 != 120 )
      {
        v63 = v29 + 120;
        if ( v30 != v63 )
          *(_QWORD *)(v17 + 104) = v63;
      }
      goto LABEL_15;
    }
    if ( v15 == 3 )
    {
      v73 = 3;
      v52 = operator new(0x20u);
      v53 = vec->cap;
      v71[0] = (unsigned __int16)v53[12];
      v54 = v73;
      *(_QWORD *)v52 = &unk_42B478;
      *(_DWORD *)(v52 + 8) = v54;
      v55 = v53[4];
      v56 = operator new(0x18u);
      *(_QWORD *)(v52 + 16) = v56;
      *(_QWORD *)v56 = off_42B020;
      v57 = v71[0];
      *(_BYTE *)(v56 + 8) = v55;
      *(_BYTE *)(v56 + 9) = v57;
      *(_WORD *)(v56 + 10) = 0;
      *(_BYTE *)(v56 + 12) = 0;
      *(_DWORD *)(v56 + 16) = 0;
      v58 = operator new(0x308u);
      v59 = vec->cap;
      *(_BYTE *)(v58 + 8) = 1;
      *(_QWORD *)(v58 + 16) = vec;
      *(_QWORD *)v58 = off_42B050;
      v60 = *(unsigned __int16 *)(v59[2] + 2LL * WORD1(vec->end));
      *(_QWORD *)(v58 + 752) = 0;
      *(_QWORD *)(v58 + 760) = 0;
      *(_QWORD *)(v58 + 768) = 0;
      if ( v60 )
        std::vector<float,apac::AlignedAllocator<float,16ul>>::_M_default_append(
          (std_vector *)(v58 + 752),
          (unsigned __int16)v60);
      v61 = *(_DWORD *)(v52 + 8);
      *(_QWORD *)(v52 + 24) = v58;
      v62 = v61 | (v13 << 8);
      *(_QWORD *)v52 = off_42B4F8;
      ++v13;
      v74 = v62;
      v75 = v52;
      std::vector<apac::ElementInstance<APACSyntacticElement *>>::emplace_back<apac::ElementInstance<APACSyntacticElement *>>(
        (std_vector *)this,
        (apac__ElementInstance *)&v74);
    }
    else
    {
      if ( v15 )
      {
        v5 = v66;
        result = 0xFFFFFFFFLL;
        v6 = v67;
        v7 = v68;
        goto LABEL_7;
      }
      v41 = operator new(0xE8u);
      v42 = vec->cap;
      v43 = v41;
      v44 = v42[4];
      LODWORD(v42) = (unsigned __int16)v42[12];
      *(_QWORD *)v41 = &unk_42B478;
      *(_DWORD *)(v41 + 8) = 0;
      v71[0] = (int)v42;
      v45 = operator new(0x18u);
      *(_QWORD *)(v43 + 16) = v45;
      *(_QWORD *)v45 = off_42B020;
      v46 = v71[0];
      *(_BYTE *)(v45 + 8) = v44;
      *(_BYTE *)(v45 + 9) = v46;
      *(_WORD *)(v45 + 10) = 0;
      *(_BYTE *)(v45 + 12) = 0;
      *(_DWORD *)(v45 + 16) = 0;
      v47 = operator new(0x308u);
      v48 = vec->cap;
      *(_BYTE *)(v47 + 8) = 1;
      *(_QWORD *)(v47 + 16) = vec;
      *(_QWORD *)v47 = off_42B050;
      v49 = *(unsigned __int16 *)(v48[2] + 2LL * WORD1(vec->end));
      *(_QWORD *)(v47 + 752) = 0;
      *(_QWORD *)(v47 + 760) = 0;
      *(_QWORD *)(v47 + 768) = 0;
      if ( v49 )
        std::vector<float,apac::AlignedAllocator<float,16ul>>::_M_default_append(
          (std_vector *)(v47 + 752),
          (unsigned __int16)v49);
      *(_QWORD *)(v43 + 24) = v47;
      *(_QWORD *)(v43 + 32) = vec;
      *(_QWORD *)v43 = off_42B4B8;
      APACTNSData::APACTNSData((APACTNSData *)(v43 + 40), (APACInstanceConfig *)vec);
      v50 = v72;
      v51 = *(_DWORD *)(v43 + 8);
      ++v72;
      v75 = v43;
      v74 = v51 | (v50 << 8);
      std::vector<apac::ElementInstance<APACSyntacticElement *>>::emplace_back<apac::ElementInstance<APACSyntacticElement *>>(
        (std_vector *)this,
        (apac__ElementInstance *)&v74);
    }
LABEL_18:
    v11 += 16;
  }
  while ( v12 != v11 );
  v5 = v66;
  v6 = v67;
  v7 = v68;
LABEL_20:
  result = 0;
LABEL_7:
  if ( v76 != _stack_chk_guard )
  {
    v65 = v4;
    v66 = v5;
    v67 = v6;
    v68 = v7;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad3c0  size 72 bytes  18 insns
 *   APACSyntacticElement::ChannelData(bool)
 *   [_ZN20APACSyntacticElement11ChannelDataEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSyntacticElement::ChannelData(APACSyntacticElement *this, bool flag)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return *((_QWORD *)this + 3) + 752LL;
}


/* ------------------------------------------------------------------------ */
/* 0x000ad410  size 68 bytes  17 insns
 *   APACSyntacticElement::GetICSInfo(bool)
 *   [_ZN20APACSyntacticElement10GetICSInfoEb]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSyntacticElement::GetICSInfo(APACSyntacticElement *this, bool flag)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return *((_QWORD *)this + 2);
}


/* ------------------------------------------------------------------------ */
/* 0x000adf90  size 272 bytes  68 insns
 *   APACSyntacticElement::~APACSyntacticElement()
 *   APACSyntacticElement::~APACSyntacticElement()
 *   [_ZN20APACSyntacticElementD2Ev]
 *   [_ZN20APACSyntacticElementD1Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42ce00, 0x42cf58, 0x42dc38, 0x42dc80, 0x43e520
 */
// Alternative name is '_ZN20APACSyntacticElementD1Ev'
void __fastcall APACSyntacticElement::~APACSyntacticElement(APACSyntacticElement *this)
{
  _QWORD *v2; // x21
  void (__fastcall *v3)(APACIndividualChannelStream *); // x1
  void *v4; // x0
  void *v5; // x0
  void (*v6)(void); // x2
  __int64 v7; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD **)this + 3);
  *(_QWORD *)this = &unk_42B478;
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
  v5 = *((void **)this + 2);
  if ( v5 )
  {
    v6 = *(void (**)(void))(*(_QWORD *)v5 + 8LL);
    if ( (char *)v6 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v5);
    else
      v6();
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000ae0a0  size 216 bytes  54 insns
 *   APACSyntacticElement::~APACSyntacticElement()
 *   [_ZN20APACSyntacticElementD0Ev]
 *   data refs: 0x9a770, 0x9a810, 0x42b468, 0x42ce00, 0x42cf58, 0x42dc38, 0x42dc80, 0x43e520
 */
void __fastcall APACSyntacticElement::~APACSyntacticElement(APACSyntacticElement *this)
{
  _QWORD *v2; // x21
  void (__fastcall *v3)(APACIndividualChannelStream *); // x1
  void *v4; // x0
  void *v5; // x0
  void (*v6)(void); // x2
  __int64 v7; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD **)this + 3);
  *(_QWORD *)this = &unk_42B478;
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
  v5 = *((void **)this + 2);
  if ( v5 )
  {
    v6 = *(void (**)(void))(*(_QWORD *)v5 + 8LL);
    if ( (char *)v6 == (char *)APACICSInfo::~APACICSInfo )
      operator delete(v5);
    else
      v6();
  }
  operator delete(this);
}


