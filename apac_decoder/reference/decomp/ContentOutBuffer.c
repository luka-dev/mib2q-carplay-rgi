/* ============================================================================
 * ContentOutBuffer
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 2044 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00095980    208  ContentOutBuffer::~ContentOutBuffer() */
/* 0x00095a50    220  ContentOutBuffer::~ContentOutBuffer() */
/* 0x000b2500    272  ContentOutBuffer::SetAudioData(apac::AudioDataType, unsigned int, __gnu_cxx::__normal_iterator<float* const*, std::vector<float*, std::allocator<float*> > >, __gnu_cxx::__normal_iterator<float* const*, std::vector<float*, std::allocator<float*> > >, unsigned int) */
/* 0x000b2610    312  ContentOutBuffer::GetAudioData(std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > > const&, apac::FrameOutRecord&) */
/* 0x000b28f0   1032  ContentOutBuffer::Initialize(apac::GlobalConfig const&) */

/* ------------------------------------------------------------------------ */
/* 0x00095980  size 208 bytes  52 insns
 *   ContentOutBuffer::~ContentOutBuffer()
 *   [_ZN16ContentOutBufferD0Ev]
 *   data refs: 0x42add0, 0x42b690, 0x42ca90, 0x42ce00, 0x42dd28, 0x43e520
 */
void __fastcall ContentOutBuffer::~ContentOutBuffer(ContentOutBuffer *this)
{
  void *v2; // x0
  _QWORD *v3; // x22
  _QWORD *v4; // x20
  void *v5; // x0
  void *v6; // x0
  __int64 v7; // [xsp+40h] [xbp+40h]
  v2 = *((void **)this + 4);
  *(_QWORD *)this = off_42B6A0;
  if ( v2 )
    operator delete(v2);
  *(_QWORD *)this = &unk_42ADE0;
  v3 = *((_QWORD **)this + 2);
  v4 = *((_QWORD **)this + 1);
  if ( v4 != v3 )
  {
    do
    {
      v5 = (void *)v4[6];
      if ( v5 )
        operator delete(v5);
      v6 = (void *)v4[2];
      if ( v6 )
        operator delete(v6);
      v4 += 9;
    }
    while ( v3 != v4 );
    v3 = *((_QWORD **)this + 1);
  }
  if ( v3 )
    operator delete(v3);
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00095a50  size 220 bytes  55 insns
 *   ContentOutBuffer::~ContentOutBuffer()
 *   ContentOutBuffer::~ContentOutBuffer()
 *   [_ZN16ContentOutBufferD2Ev]
 *   [_ZN16ContentOutBufferD1Ev]
 *   data refs: 0x42add0, 0x42b690, 0x42ca90, 0x42ce00, 0x42dd28, 0x43e520
 */
// Alternative name is '_ZN16ContentOutBufferD1Ev'
void __fastcall ContentOutBuffer::~ContentOutBuffer(ContentOutBuffer *this)
{
  void *v2; // x0
  _QWORD *v3; // x22
  _QWORD *v4; // x20
  void *v5; // x0
  void *v6; // x0
  __int64 v7; // [xsp+40h] [xbp+40h]
  v2 = *((void **)this + 4);
  *(_QWORD *)this = off_42B6A0;
  if ( v2 )
    operator delete(v2);
  *(_QWORD *)this = &unk_42ADE0;
  v3 = *((_QWORD **)this + 2);
  v4 = *((_QWORD **)this + 1);
  if ( v4 != v3 )
  {
    do
    {
      v5 = (void *)v4[6];
      if ( v5 )
        operator delete(v5);
      v6 = (void *)v4[2];
      if ( v6 )
        operator delete(v6);
      v4 += 9;
    }
    while ( v3 != v4 );
    v3 = *((_QWORD **)this + 1);
  }
  if ( v3 )
    operator delete(v3);
}


/* ------------------------------------------------------------------------ */
/* 0x000b2500  size 272 bytes  68 insns
 *   ContentOutBuffer::SetAudioData(apac::AudioDataType, unsigned int, __gnu_cxx::__normal_iterator<float* const*, std::vector<float*, std::allocator<float*> > >, __gnu_cxx::__normal_iterator<float* const*, std::vector<float*, std::allocator<float*> > >, unsigned int)
 *   [_ZN16ContentOutBuffer12SetAudioDataEN4apac13AudioDataTypeEjN9__gnu_cxx17__normal_iteratorIPKPfSt6vectorIS4_SaIS4_EEEESA_j]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall ContentOutBuffer::SetAudioData(
        ContentOutBuffer *this,
        int n,
        unsigned int n2,
        void *a,
        void *a2,
        unsigned int n3)
{
  signed __int64 v6; // x21
  __int64 v7; // x22
  __int64 v8; // x19
  void *v9; // x0
  __int64 v10; // x1
  __int64 v12; // x2
  unsigned __int64 v13; // x4
  bool v14; // zf
  __int64 v15; // x1
  __int64 v16; // x6
  __int64 result; // x0
  signed __int64 v18; // [xsp+20h] [xbp+20h]
  __int64 v19; // [xsp+28h] [xbp+28h]
  __int64 v20; // [xsp+38h] [xbp+38h]
  __int64 v21; // [xsp+40h] [xbp+40h]
  v8 = *((_QWORD *)this + 1);
  if ( __PAIR64__(n2, n) == *(_QWORD *)v8 )
  {
    v19 = v7;
    v9 = *(void **)(v8 + 16);
    v6 = ((_BYTE *)a2 - (_BYTE *)a) >> 3;
    if ( (__int64)(*(_QWORD *)(v8 + 24) - (_QWORD)v9) >> 3 < (unsigned __int64)(unsigned int)v6 )
    {
      result = 0xFFFFFFFFLL;
      v7 = v19;
    }
    else
    {
      v10 = *(_QWORD *)(v8 + 48);
      if ( v10 == *(_QWORD *)(v8 + 56) )
      {
        if ( a2 != a )
          memmove(v9, a, (_BYTE *)a2 - (_BYTE *)a);
      }
      else if ( a2 != a )
      {
        v12 = 0;
        v13 = (unsigned __int64)((char *)a2 - 8 - (_BYTE *)a) >> 3;
        while ( 1 )
        {
          v14 = v13 == v12;
          v15 = *(unsigned int *)(v10 + 4LL * (unsigned int)v12);
          v16 = *((_QWORD *)a + v12++);
          *((_QWORD *)v9 + v15) = v16;
          if ( v14 )
            break;
          v9 = *(void **)(v8 + 16);
          v10 = *(_QWORD *)(v8 + 48);
        }
      }
      *(_DWORD *)(v8 + 8) = v6;
      result = 0;
      *(_DWORD *)(v8 + 40) = n3;
      v7 = v19;
    }
  }
  else
  {
    result = 0xFFFFFFFFLL;
  }
  if ( v20 != _stack_chk_guard )
  {
    v18 = v6;
    v19 = v7;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b2610  size 312 bytes  78 insns
 *   ContentOutBuffer::GetAudioData(std::vector<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > >, std::allocator<std::pair<unsigned int, std::unique_ptr<apac::DecElement, std::default_delete<apac::DecElement> > > > > const&, apac::FrameOutRecord&)
 *   [_ZN16ContentOutBuffer12GetAudioDataERKSt6vectorISt4pairIjSt10unique_ptrIN4apac10DecElementESt14default_deleteIS4_EEESaIS8_EERNS3_14FrameOutRecordE]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall ContentOutBuffer::GetAudioData(
        ContentOutBuffer *this,
        const std_vector *vec,
        apac__FrameOutRecord *a)
{
  apac__FrameOutRecord *v3; // x23
  const float **v4; // x24
  __int64 v5; // x27
  __int64 v7; // x20
  __int64 v8; // x26
  unsigned int v9; // w21
  unsigned int v10; // w27
  const float **v11; // x0
  char *v12; // x4
  size_t v13; // x19
  __int64 result; // x0
  apac__FrameOutRecord *v15; // [xsp+30h] [xbp+30h]
  const float **v16; // [xsp+38h] [xbp+38h]
  __int64 v17; // [xsp+50h] [xbp+50h]
  __int64 v18; // [xsp+68h] [xbp+68h]
  __int64 v19; // [xsp+70h] [xbp+70h]
  v7 = *((_QWORD *)this + 1);
  v8 = *((_QWORD *)this + 2);
  if ( v8 != v7 )
  {
    v9 = 0;
    v3 = a;
    v17 = v5;
    v4 = *((const float ***)this + 4);
    v10 = *(_DWORD *)(v7 + 40);
    v11 = v4;
    v12 = *((char **)this + 4);
    while ( 1 )
    {
      v9 += *(_DWORD *)(v7 + 8);
      if ( (__int64)(*((_QWORD *)this + 5) - (_QWORD)v11) >> 3 < (unsigned __int64)v9 || v10 != *(_DWORD *)(v7 + 40) )
        break;
      v13 = 8LL * *(unsigned int *)(v7 + 8);
      if ( v13 )
        v12 = (char *)memmove(v12, *(const void **)(v7 + 16), v13);
      v7 += 72;
      v12 += v13;
      if ( v8 == v7 )
      {
        InterleaveConvert(
          v4,
          (void **)v3,
          v9,
          v10,
          *((_DWORD *)v3 + 517) & 1,
          (*((_DWORD *)v3 + 517) & 0x20LL) != 0,
          *((float *)v3 + 512));
        *((_DWORD *)v3 + 514) = v10;
        *((_WORD *)v3 + 1030) = v9;
        result = 0;
        v5 = v17;
        goto LABEL_12;
      }
      v11 = *((const float ***)this + 4);
    }
    v5 = v17;
  }
  result = 1;
LABEL_12:
  if ( v18 != _stack_chk_guard )
  {
    v15 = v3;
    v16 = v4;
    v17 = v5;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b28f0  size 1032 bytes  256 insns
 *   ContentOutBuffer::Initialize(apac::GlobalConfig const&)
 *   [_ZN16ContentOutBuffer10InitializeERKN4apac12GlobalConfigE]
 *   data refs: 0x951c0, 0x42ce00, 0x42db68, 0x43e520
 */
__int64 __fastcall ContentOutBuffer::Initialize(ContentOutBuffer *this, const apac__GlobalConfig *a)
{
  unsigned int *v2; // x25
  __int64 v3; // x26
  __int64 (__fastcall *v6)(apac__GlobalConfig__CodecConfigData *); // x1
  _QWORD *v7; // x0
  __int64 v8; // x21
  __int64 v9; // x25
  __int64 v10; // x19
  void *v11; // x0
  void *v12; // x0
  __int64 (__fastcall *v13)(apac__GlobalConfig__CodecConfigData *); // x1
  __int64 v14; // x0
  _QWORD *v15; // x22
  int v16; // w19
  unsigned __int64 v17; // x21
  _DWORD *v18; // x0
  _DWORD *v19; // x1
  __int64 v20; // x19
  unsigned __int64 v21; // x22
  _DWORD *v22; // x24
  int8x8_t v23; // d0
  unsigned int v24; // w22
  __int64 v25; // x0
  void *v26; // x1
  unsigned __int64 v27; // x2
  __int64 v28; // x0
  void *v29; // x1
  unsigned __int64 v30; // x3
  bool v31; // cf
  __int64 result; // x0
  void *v33; // x0
  char *v34; // x0
  char *v35; // x3
  unsigned __int64 v36; // x1
  char *v37; // x22
  void *v38; // x21
  __int64 v39; // x1
  unsigned __int64 v40; // x0
  unsigned int *v42; // [xsp+40h] [xbp+40h]
  __int64 v43; // [xsp+48h] [xbp+48h]
  _QWORD v44[2]; // [xsp+50h] [xbp+50h] BYREF
  void *v45; // [xsp+60h] [xbp+60h]
  __int64 v46; // [xsp+68h] [xbp+68h]
  __int64 v47; // [xsp+70h] [xbp+70h]
  int v48; // [xsp+78h] [xbp+78h]
  void *v49; // [xsp+80h] [xbp+80h]
  __int64 v50; // [xsp+88h] [xbp+88h]
  __int64 v51; // [xsp+90h] [xbp+90h]
  __int64 v52; // [xsp+98h] [xbp+98h]
  __int64 v53; // [xsp+A0h] [xbp+A0h]
  v6 = **(__int64 (__fastcall ***)(apac__GlobalConfig__CodecConfigData *))a;
  if ( v6 == apac::GlobalConfig::CodecConfigData::configs )
    v7 = (_QWORD *)(*((_QWORD *)a + 1) + 176LL);
  else
    v7 = (_QWORD *)v6(a);
  if ( *v7 != v7[1] )
  {
    v9 = *((_QWORD *)this + 1);
    v8 = *((_QWORD *)this + 2);
    if ( v9 != v8 )
    {
      v10 = *((_QWORD *)this + 1);
      do
      {
        v11 = *(void **)(v10 + 48);
        if ( v11 )
          operator delete(v11);
        v12 = *(void **)(v10 + 16);
        if ( v12 )
          operator delete(v12);
        v10 += 72;
      }
      while ( v8 != v10 );
      *((_QWORD *)this + 2) = v9;
    }
    v13 = **(__int64 (__fastcall ***)(apac__GlobalConfig__CodecConfigData *))a;
    if ( v13 == apac::GlobalConfig::CodecConfigData::configs )
      v14 = *((_QWORD *)a + 1) + 176LL;
    else
      v14 = v13(a);
    v15 = *(_QWORD **)v14;
    v16 = apac::ConvertToAudioDataType(*(_DWORD *)(**(_QWORD **)v14 + 8LL));
    v17 = (*(unsigned int (__fastcall **)(_QWORD))(*(_QWORD *)*v15 + 24LL))(*v15);
    v2 = (unsigned int *)(*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v15 + 32LL))(*v15);
    LODWORD(v44[0]) = v16;
    *(_QWORD *)((char *)v44 + 4) = 0;
    v45 = nullptr;
    v46 = 0;
    v19 = *((_DWORD **)this + 2);
    v18 = *((_DWORD **)this + 3);
    v47 = 0;
    v48 = 0;
    v49 = nullptr;
    v50 = 0;
    v51 = 0;
    if ( v19 == v18 )
    {
      std::vector<apac::ASCAudioOutData>::_M_realloc_insert<apac::ASCAudioOutData>(
        (std_vector *)((char *)this + 8),
        v19,
        (apac__ASCAudioOutData *)v44);
      if ( v49 )
        operator delete(v49);
      if ( v45 )
        operator delete(v45);
    }
    else
    {
      *v19 = v16;
      v19[1] = HIDWORD(v44[0]);
      v19[2] = v44[1];
      *((_QWORD *)v19 + 2) = v45;
      *((_QWORD *)v19 + 3) = v46;
      *((_QWORD *)v19 + 4) = v47;
      v19[10] = v48;
      *((_QWORD *)v19 + 6) = v49;
      *((_QWORD *)v19 + 7) = v50;
      *((_QWORD *)v19 + 8) = v51;
      *((_QWORD *)this + 2) += 72LL;
    }
    v20 = *((_QWORD *)this + 2);
    v21 = 20LL * v2[2] + 12;
    v22 = (_DWORD *)operator new(v21);
    memset(v22, 0, v21);
    memcpy(v22, v2, v21);
    LODWORD(v2) = *v22;
    if ( *v22 )
    {
      v24 = (unsigned __int16)*v22;
      if ( (_DWORD)v2 == 0x10000 )
      {
        v23.n64_u32[0] = v22[1];
        v24 = (unsigned __int8)vaddv_s8(vcnt_s8(v23));
      }
    }
    else
    {
      v24 = v22[2];
    }
    *(_DWORD *)(v20 - 64) = 0;
    *(_DWORD *)(v20 - 32) = 0;
    v44[0] = 0;
    v25 = *(_QWORD *)(v20 - 56);
    v26 = *(void **)(v20 - 48);
    v27 = ((__int64)v26 - v25) >> 3;
    if ( v24 > v27 )
    {
      std::vector<float *>::_M_fill_insert(v20 - 56, v26, v24 - v27);
    }
    else if ( v24 < v27 )
    {
      v33 = (void *)(v25 + 8LL * v24);
      if ( v26 != v33 )
      {
        *(_QWORD *)(v20 - 48) = v33;
        if ( (_DWORD)v2 == 8060934 )
        {
LABEL_30:
          v35 = *(char **)(v20 - 24);
          v34 = *(char **)(v20 - 16);
          v36 = (v34 - v35) >> 2;
          if ( v24 > v36 )
          {
            std::vector<unsigned int>::_M_default_append(v20 - 24, v24 - v36);
            v35 = *(char **)(v20 - 24);
            v34 = *(char **)(v20 - 16);
          }
          else if ( v24 < v36 )
          {
            v37 = &v35[4 * v24];
            if ( v34 != v37 )
            {
              v34 = v37;
              *(_QWORD *)(v20 - 16) = v37;
            }
          }
          if ( v35 != v34 )
          {
            v39 = 0;
            v40 = (unsigned __int64)(v34 - 4 - v35) >> 2;
            do
              *(_DWORD *)&v35[4 * v39] = v39;
            while ( v40 != v39++ );
            v34 = *(char **)(v20 - 24);
          }
          *(_DWORD *)v34 = 1;
          *(_DWORD *)(*(_QWORD *)(v20 - 24) + 4LL) = 0;
          operator delete(v22);
          v44[0] = 0;
          v28 = *((_QWORD *)this + 4);
          v29 = *((void **)this + 5);
          v30 = ((__int64)v29 - v28) >> 3;
          v31 = v17 >= v30;
          if ( v17 > v30 )
            goto LABEL_46;
          goto LABEL_23;
        }
LABEL_22:
        operator delete(v22);
        v28 = *((_QWORD *)this + 4);
        v29 = *((void **)this + 5);
        v44[0] = 0;
        v30 = ((__int64)v29 - v28) >> 3;
        v31 = v17 >= v30;
        if ( v17 > v30 )
        {
LABEL_46:
          std::vector<float *>::_M_fill_insert((_DWORD)this + 32, v29, v17 - v30);
LABEL_25:
          result = 0;
          goto LABEL_26;
        }
LABEL_23:
        if ( !v31 )
        {
          v38 = (void *)(v28 + 8 * v17);
          if ( v29 != v38 )
            *((_QWORD *)this + 5) = v38;
        }
        goto LABEL_25;
      }
    }
    if ( (_DWORD)v2 == 8060934 )
      goto LABEL_30;
    goto LABEL_22;
  }
  result = 0xFFFFFFFFLL;
LABEL_26:
  if ( v52 != _stack_chk_guard )
  {
    v42 = v2;
    v43 = v3;
  }
  return result;
}


