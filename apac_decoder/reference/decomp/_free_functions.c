/* ============================================================================
 * _free_functions
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 14 function(s), 5452 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00099650    836  int GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, __gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::vector<apac::observer_ptr<apac::IASCConfig const>, std::allocator<apac::observer_ptr<apac::IASCConfig const> > >&) */
/* 0x00105bb0    552  APACDecoder_DecodeFrame */
/* 0x00105de0    132  APACDecoder_Reset */
/* 0x00105e70    440  APACDecoder_GetParameter */
/* 0x00106030    124  APACDecoder_GetCookieOffset */
/* 0x001060b0     80  APACDecoder_GetVersion */
/* 0x00106100     80  APACDecoder_GetProfile */
/* 0x00106150     92  APACDecoder_SampleRate */
/* 0x001061b0     88  APACDecoder_FrameLength */
/* 0x00106210     80  APACDecoder_NumberOfChannels */
/* 0x00106260    552  APACDecoder_SetParameter */
/* 0x00106490    876  APACDecoder_Configure */
/* 0x00106800   1028  APACDecoder_Create */
/* 0x00106c10    492  APACDecoder_Dispose */

/* ------------------------------------------------------------------------ */
/* 0x00099650  size 836 bytes  208 insns
 *   int GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > > >(__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, __gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >*, std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > >, std::vector<apac::observer_ptr<apac::IASCConfig const>, std::allocator<apac::observer_ptr<apac::IASCConfig const> > >&)
 *   [_Z28GetEffectiveDecoderOutConfigIN9__gnu_cxx17__normal_iteratorIPSt10unique_ptrIN4apac10IASCConfigESt14default_deleteIS4_EESt6vectorIS7_SaIS7_EEEEEiT_SD_RS9_INS3_12observer_ptrIKS4_EESaISG_EE]
 *   data refs: 0x30e8b8, 0x42ce00, 0x43e520
 */
int __fastcall GetEffectiveDecoderOutConfig<__gnu_cxx::__normal_iterator<std::unique_ptr<apac::IASCConfig> *,std::vector<std::unique_ptr<apac::IASCConfig>>>>(
        void *a,
        void *a2,
        std_vector *vec)
{
  __int64 v3; // x23
  __int64 v4; // x24
  char *v6; // x19
  void *begin; // x2
  int v9; // w23
  int v10; // w0
  unsigned __int16 *v11; // x22
  int v12; // w24
  __int64 v13; // x0
  unsigned int (*v14)(void); // x1
  int v15; // w23
  void *v16; // x0
  _BOOL4 v17; // w19
  __int16 v19; // w0
  char *end; // x22
  __int64 v21; // x24
  char *v22; // x23
  __int64 v23; // x28
  unsigned __int64 v24; // x0
  bool v25; // cc
  unsigned __int64 v26; // x27
  unsigned __int64 v27; // x0
  __int64 v28; // x0
  void *v29; // x4
  _QWORD *v30; // x3
  char *v31; // x27
  __int64 v32; // x1
  unsigned __int64 v33; // x22
  __int64 v35; // [xsp+30h] [xbp+30h]
  __int64 v36; // [xsp+38h] [xbp+38h]
  _QWORD *v37; // [xsp+60h] [xbp+60h]
  void *v38; // [xsp+68h] [xbp+68h]
  void *v39; // [xsp+70h] [xbp+70h] BYREF
  unsigned __int16 *v40; // [xsp+78h] [xbp+78h]
  __int64 v41; // [xsp+80h] [xbp+80h]
  __int64 v42; // [xsp+88h] [xbp+88h]
  __int64 v43; // [xsp+90h] [xbp+90h]
  v6 = (char *)a;
  v39 = nullptr;
  v40 = nullptr;
  begin = vec->begin;
  v41 = 0;
  if ( begin != vec->end )
    vec->end = begin;
  if ( a != a2 )
  {
    v35 = v3;
    v36 = v4;
    while ( 1 )
    {
      v9 = *(unsigned __int16 *)(*(_QWORD *)v6 + 42LL);
      v10 = apac::ConvertToAudioDataType(*(_DWORD *)(*(_QWORD *)v6 + 8LL));
      v11 = (unsigned __int16 *)v39;
      v12 = v10;
      v13 = ((char *)v40 - (_BYTE *)v39) >> 3;
      if ( ((char *)v40 - (_BYTE *)v39) >> 5 > 0 )
      {
        while ( *v11 != v9 )
        {
          if ( v11[4] == v9 )
          {
            v11 += 4;
            goto LABEL_12;
          }
          if ( v11[8] == v9 )
          {
            v11 += 8;
            goto LABEL_12;
          }
          if ( v11[12] == v9 )
          {
            v11 += 12;
            goto LABEL_12;
          }
          v11 += 16;
          if ( (char *)v39 + 32 * (((char *)v40 - (_BYTE *)v39) >> 5) == (char *)v11 )
          {
            v13 = ((char *)v40 - (char *)v11) >> 3;
            goto LABEL_21;
          }
        }
        goto LABEL_12;
      }
LABEL_21:
      if ( v13 == 2 )
        goto LABEL_32;
      if ( v13 == 3 )
        break;
      if ( v13 != 1 )
        goto LABEL_24;
LABEL_34:
      if ( *v11 != v9 )
      {
LABEL_24:
        v14 = *(unsigned int (**)(void))(**(_QWORD **)v6 + 24LL);
        goto LABEL_25;
      }
LABEL_12:
      v14 = *(unsigned int (**)(void))(**(_QWORD **)v6 + 24LL);
      if ( v40 != v11 )
      {
        v15 = v11[1] - *v11;
        if ( v14() != v15 || *((_DWORD *)v11 + 1) != v12 )
        {
          v16 = v39;
          v17 = 1;
          if ( !v39 )
          {
LABEL_49:
            v3 = v35;
            v4 = v36;
            goto LABEL_18;
          }
LABEL_17:
          operator delete(v16);
          v3 = v35;
          v4 = v36;
          goto LABEL_18;
        }
        goto LABEL_15;
      }
LABEL_25:
      v19 = v14();
      apac::ASCChannelMap::AddMap((apac__ASCChannelMap *)&v39, v12, v9, v9 + v19);
      end = (char *)vec->end;
      v21 = *(_QWORD *)v6;
      if ( end == vec->cap )
      {
        v22 = (char *)vec->begin;
        v23 = end - (char *)vec->begin;
        v24 = v23 >> 3;
        if ( v23 >> 3 == 0xFFFFFFFFFFFFFFFLL )
          std::__throw_length_error("vector::_M_realloc_insert");
        if ( v24 )
        {
          v25 = v24 > 2 * v24;
          v26 = 0x7FFFFFFFFFFFFFF8LL;
          v27 = 2 * v24;
          if ( !v25 )
          {
            if ( !v27 )
            {
              v31 = byte_8;
              v29 = nullptr;
              v30 = nullptr;
LABEL_41:
              *(_QWORD *)((char *)v30 + v23) = v21;
              if ( end == v22 )
              {
                if ( v22 )
                  goto LABEL_46;
              }
              else
              {
                v32 = 0;
                v33 = end - 8 - v22;
                do
                  v30[v32] = *(_QWORD *)&v22[8 * v32];
                while ( v33 >> 3 != v32++ );
                v31 = (char *)v30 + v33 + 16;
LABEL_46:
                v37 = v30;
                v38 = v29;
                operator delete(v22);
                v30 = v37;
                v29 = v38;
              }
              vec->begin = v30;
              vec->end = v31;
              vec->cap = v29;
              goto LABEL_15;
            }
            if ( v27 > 0xFFFFFFFFFFFFFFFLL )
              v27 = 0xFFFFFFFFFFFFFFFLL;
            v26 = 8 * v27;
          }
        }
        else
        {
          v26 = 8;
        }
        v28 = operator new(v26);
        v29 = (void *)(v28 + v26);
        v30 = (_QWORD *)v28;
        v31 = (char *)(v28 + 8);
        goto LABEL_41;
      }
      *(_QWORD *)end = v21;
      vec->end = (char *)vec->end + 8;
LABEL_15:
      v6 += 8;
      if ( a2 == v6 )
      {
        v16 = v39;
        v17 = vec->end == vec->begin;
        if ( !v39 )
          goto LABEL_49;
        goto LABEL_17;
      }
    }
    if ( *v11 == v9 )
      goto LABEL_12;
    v11 += 4;
LABEL_32:
    if ( *v11 == v9 )
      goto LABEL_12;
    v11 += 4;
    goto LABEL_34;
  }
  v17 = vec->end == begin;
LABEL_18:
  if ( v42 != _stack_chk_guard )
  {
    v35 = v3;
    v36 = v4;
  }
  return v17;
}


/* ------------------------------------------------------------------------ */
/* 0x00105bb0  size 552 bytes  138 insns
 *   APACDecoder_DecodeFrame
 *   [APACDecoder_DecodeFrame]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_DecodeFrame(
        __int64 *a1,
        int8x8_t a2,
        __int64 a3,
        int a4,
        void *a5,
        unsigned int a6,
        unsigned int *a7,
        __int64 a8,
        __int64 a9,
        __int64 a10)
{
  __int64 v10; // x23
  __int64 v11; // x24
  __int64 v12; // x25
  __int64 v16; // x0
  _DWORD *v17; // x5
  int v18; // w6
  unsigned int v19; // w20
  bool v20; // cc
  unsigned int v21; // w22
  DRCConfiguration *v22; // x0
  DRCConfiguration *v23; // x24
  unsigned int v24; // w4
  unsigned int v26; // w20
  unsigned int v27; // w4
  int v28; // w3
  void **v29; // x23
  size_t v30; // x2
  __int64 v31; // [xsp+30h] [xbp+30h]
  __int64 v32; // [xsp+38h] [xbp+38h]
  __int64 v33; // [xsp+40h] [xbp+40h]
  __int64 v34; // [xsp+58h] [xbp+58h]
  if ( !*((_BYTE *)a1 + 2128) )
  {
    v21 = -6718;
    goto LABEL_14;
  }
  v16 = *a1;
  v17 = *(_DWORD **)(a1[1] + 256);
  if ( *v17 )
  {
    v18 = (unsigned __int16)*v17;
    if ( *v17 != 0x10000 )
    {
      if ( a4 )
        goto LABEL_5;
LABEL_16:
      v21 = 0;
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v16 + 24LL))(v16);
      *((_DWORD *)a1 + 518) = 0;
      v10 = v31;
      v11 = v32;
      v12 = v33;
      goto LABEL_14;
    }
    a2.n64_u32[0] = v17[1];
    v18 = (unsigned __int8)vaddv_s8(vcnt_s8(a2));
    if ( !a4 )
      goto LABEL_16;
  }
  else
  {
    v18 = v17[2];
    if ( !a4 )
      goto LABEL_16;
  }
LABEL_5:
  v19 = v18 << 10;
  if ( v18 << 10 )
    v20 = v19 > a6;
  else
    v20 = 1;
  if ( v20 )
  {
    v21 = -6728;
    v10 = v31;
    v11 = v32;
    v12 = v33;
  }
  else
  {
    a1[2] = *(_QWORD *)a1[262];
    v21 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v16 + 32LL))(v16);
    v22 = (DRCConfiguration *)sub_1058F0(a1);
    v23 = v22;
    if ( *((_BYTE *)v22 + 181) )
    {
      DRCConfiguration::SetUniDrcDelayIsSetOnCodec(v22, 1u);
      DRCConfiguration::SetUniDrcDelayChanged(v23, 0);
    }
    v24 = *((_DWORD *)a1 + 518);
    if ( !v21 && v19 == *((unsigned __int16 *)a1 + 1038) * v24 )
    {
      v26 = *((_DWORD *)a1 + 530);
      if ( v26 )
      {
        if ( v26 >= v24 )
        {
          *((_DWORD *)a1 + 530) = v26 - v24;
          *a7 = 0;
          v10 = v31;
          v11 = v32;
          v12 = v33;
        }
        else
        {
          v27 = v24 - v26;
          *a7 = v27;
          v28 = *((unsigned __int16 *)a1 + 1038);
          v29 = (void **)a1[262];
          v30 = 4LL * v27 * v28;
          if ( v30 )
            memmove(*v29, (char *)*v29 + 4 * (unsigned int)(v28 * *((_DWORD *)a1 + 530)), v30);
          sub_1057F0((int)v29, a5);
          *((_DWORD *)a1 + 530) = 0;
          v10 = v31;
          v11 = v32;
          v12 = v33;
        }
      }
      else
      {
        *a7 = v24;
        sub_1057F0(a1[262], a5);
        v10 = v31;
        v11 = v32;
        v12 = v33;
      }
    }
    else
    {
      memset(a5, 0, 4LL * *((unsigned __int16 *)a1 + 1038) * v24);
      v10 = v31;
      v11 = v32;
      v12 = v33;
    }
  }
LABEL_14:
  if ( v34 != _stack_chk_guard )
  {
    v31 = v10;
    v32 = v11;
    v33 = v12;
  }
  return v21;
}


/* ------------------------------------------------------------------------ */
/* 0x00105de0  size 132 bytes  33 insns
 *   APACDecoder_Reset
 *   [APACDecoder_Reset]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_Reset(
        _QWORD *a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  unsigned int v9; // w20
  if ( a1 )
  {
    v9 = 0;
    *(_QWORD *)((char *)a1 + 2116) = 0;
    if ( *((_BYTE *)a1 + 2128) )
      (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*a1 + 24LL))(*a1);
  }
  else
  {
    return (unsigned int)-6705;
  }
  return v9;
}


/* ------------------------------------------------------------------------ */
/* 0x00105e70  size 440 bytes  105 insns
 *   APACDecoder_GetParameter
 *   [APACDecoder_GetParameter]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetParameter(
        __int64 a1,
        int a2,
        __int64 a3,
        int *a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x21
  bool v10; // zf
  __int16 *ProfileConfig; // x0
  int8x8_t v14; // d0
  _DWORD *v15; // x2
  int *v16; // x1
  int *v17; // x4
  int v18; // w3
  int i; // w5
  int v20; // w0
  __int64 result; // x0
  __int64 v22; // [xsp+20h] [xbp+20h]
  __int64 v23; // [xsp+38h] [xbp+38h]
  if ( a3 )
    v10 = a4 == nullptr;
  else
    v10 = 1;
  if ( v10 || a1 == 0 )
  {
    result = 4294960591LL;
    goto LABEL_25;
  }
  if ( !*(_BYTE *)(a1 + 2128) )
  {
    result = 4294960578LL;
    goto LABEL_25;
  }
  v9 = *(_QWORD *)(a1 + 8);
  if ( a2 == 1885432678 )
  {
    *a4 = 1024;
    result = 0;
    goto LABEL_25;
  }
  if ( a2 > 1885432678 )
  {
    if ( a2 == 1886546285 )
    {
      *a4 = 2048;
      result = 0;
      goto LABEL_25;
    }
    goto LABEL_27;
  }
  if ( a2 == 1668248434 )
  {
    result = 0;
    *a4 = *(_DWORD *)(v9 + 36);
    goto LABEL_25;
  }
  if ( a2 != 1868786720 )
  {
LABEL_27:
    result = 4294960591LL;
    goto LABEL_25;
  }
  ProfileConfig = apac::GetProfileConfig(*(unsigned __int16 *)(v9 + 32));
  if ( (unsigned __int16)*ProfileConfig == 0xFFFF )
  {
    result = 560423284;
  }
  else
  {
    v15 = *(_DWORD **)(v9 + 256);
    v16 = *((int **)ProfileConfig + 3);
    v17 = *((int **)ProfileConfig + 4);
    if ( *v15 )
    {
      v18 = (unsigned __int16)*v15;
      if ( *v15 == 0x10000 )
      {
        v14.n64_u32[0] = v15[1];
        v18 = (unsigned __int8)vaddv_s8(vcnt_s8(v14));
      }
    }
    else
    {
      v18 = v15[2];
    }
    for ( i = 0; v16 != v17; ++v16 )
    {
      v20 = *v16;
      if ( (*v16 & 0xFFFF0000) == 0x930000 )
      {
        i = v20 | v18;
      }
      else if ( v18 == (unsigned __int16)v20 )
      {
        i = *v16;
        break;
      }
    }
    *a4 = i;
    result = 0;
  }
LABEL_25:
  if ( v23 != _stack_chk_guard )
    v22 = v9;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00106030  size 124 bytes  31 insns
 *   APACDecoder_GetCookieOffset
 *   [APACDecoder_GetCookieOffset]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetCookieOffset(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  int v10; // [xsp+20h] [xbp+20h] BYREF
  int v11; // [xsp+24h] [xbp+24h] BYREF
  v10 = 0;
  v11 = 0;
  if ( a2 > 8u && (unsigned __int8)sub_105670(a1, a2, &v10, &v11) )
    return (unsigned __int16)v10;
  else
    return 58831;
}


/* ------------------------------------------------------------------------ */
/* 0x001060b0  size 80 bytes  20 insns
 *   APACDecoder_GetVersion
 *   [APACDecoder_GetVersion]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetVersion(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return bswap32(*(_DWORD *)(a1 + a2)) >> 16;
}


/* ------------------------------------------------------------------------ */
/* 0x00106100  size 80 bytes  20 insns
 *   APACDecoder_GetProfile
 *   [APACDecoder_GetProfile]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_GetProfile(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return (unsigned __int16)bswap32(*(_DWORD *)(a1 + a2)) >> 10;
}


/* ------------------------------------------------------------------------ */
/* 0x00106150  size 92 bytes  22 insns
 *   APACDecoder_SampleRate
 *   [APACDecoder_SampleRate]
 *   data refs: 0x3483a8, 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_SampleRate(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return dword_3483A8[((unsigned __int64)bswap32(*(_DWORD *)(a1 + a2)) >> 15) & 0x3F];
}


/* ------------------------------------------------------------------------ */
/* 0x001061b0  size 88 bytes  22 insns
 *   APACDecoder_FrameLength
 *   [APACDecoder_FrameLength]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_FrameLength(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return (unsigned __int8)((bswap32(*(_DWORD *)(a1 + a2)) & 0x7E00) == 0) << 10;
}


/* ------------------------------------------------------------------------ */
/* 0x00106210  size 80 bytes  20 insns
 *   APACDecoder_NumberOfChannels
 *   [APACDecoder_NumberOfChannels]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACDecoder_NumberOfChannels(
        __int64 a1,
        unsigned __int16 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  return (unsigned __int8)((unsigned __int64)bswap32(*(_DWORD *)(a1 + a2)) >> 1);
}


/* ------------------------------------------------------------------------ */
/* 0x00106260  size 552 bytes  128 insns
 *   APACDecoder_SetParameter
 *   [APACDecoder_SetParameter]
 *   data refs: 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
__int64 __fastcall APACDecoder_SetParameter(
        __int64 a1,
        int a2,
        int a3,
        unsigned int *a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x21
  bool v11; // zf
  __int64 v14; // x0
  unsigned int v15; // w1
  __int64 result; // x0
  DRCConfiguration *v17; // x0
  _DWORD *exception; // x0
  __int64 v19; // [xsp+20h] [xbp+20h]
  __int64 v20; // [xsp+38h] [xbp+38h]
  if ( a1 )
    v11 = a4 == nullptr;
  else
    v11 = 1;
  if ( !v11 )
  {
    v19 = v9;
    if ( a2 == 1634493037 )
    {
      v14 = sub_1058F0(a1);
      if ( a3 != 4 )
        goto LABEL_42;
      if ( (unsigned __int8)DRCConfiguration::SetAlbumMode((DRCConfiguration *)v14, *a4 != 0) )
        goto LABEL_17;
      goto LABEL_24;
    }
    if ( a2 <= 1634493037 )
    {
      if ( a2 == 1584427631 )
      {
LABEL_30:
        v14 = sub_1058F0(a1);
        if ( a3 != 4 )
          goto LABEL_42;
        if ( (unsigned __int8)DRCConfiguration::SetUniDRCCompressionProfile((DRCConfiguration *)v14, *a4) )
          goto LABEL_17;
LABEL_24:
        exception = __cxa_allocate_exception(4u);
        *exception = 560423284;
        __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
      }
      if ( a2 <= 1584427631 )
      {
        if ( a2 == 1583510388 )
        {
          v17 = (DRCConfiguration *)sub_1058F0(a1);
          if ( a3 == 4 )
          {
            if ( !(unsigned __int8)DRCConfiguration::SetBoostFactor(v17, *(float *)a4) )
              goto LABEL_24;
            goto LABEL_17;
          }
        }
        else
        {
          if ( a2 != 1583576436 )
            goto LABEL_17;
          v17 = (DRCConfiguration *)sub_1058F0(a1);
          if ( a3 == 4 )
          {
            if ( !(unsigned __int8)DRCConfiguration::SetCutFactor(v17, *(float *)a4) )
              goto LABEL_24;
            goto LABEL_17;
          }
        }
LABEL_41:
        sub_70CF4(v17);
      }
      if ( a2 != 1584428140 )
        goto LABEL_17;
    }
    else if ( a2 != 1886418028 )
    {
      if ( a2 > 1886418028 )
      {
        v14 = 1886546285;
        if ( a2 == 1886546285 )
        {
          if ( a3 == 4 )
          {
            v15 = *a4;
            result = 0;
            *(_DWORD *)(a1 + 2116) = *a4;
            *(_DWORD *)(a1 + 2120) = v15;
            v9 = v19;
            goto LABEL_18;
          }
LABEL_42:
          sub_70CF4(v14);
        }
        goto LABEL_17;
      }
      if ( a2 != 1685218160 )
      {
        if ( a2 == 1835299427 )
        {
          v14 = sub_1058F0(a1);
          if ( a3 != 4 )
            goto LABEL_42;
          if ( !(unsigned __int8)DRCConfiguration::SetCompressionMode((DRCConfiguration *)v14, *a4) )
            goto LABEL_24;
        }
LABEL_17:
        v9 = v19;
        result = 0;
        goto LABEL_18;
      }
      goto LABEL_30;
    }
    v17 = (DRCConfiguration *)sub_1058F0(a1);
    if ( a3 == 4 )
    {
      if ( !(unsigned __int8)DRCConfiguration::SetTargetLevel(v17, *(float *)a4) )
        goto LABEL_24;
      goto LABEL_17;
    }
    goto LABEL_41;
  }
  result = 4294960591LL;
LABEL_18:
  if ( v20 != _stack_chk_guard )
    v19 = v9;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00106490  size 876 bytes  216 insns
 *   APACDecoder_Configure
 *   [APACDecoder_Configure]
 *   data refs: 0x348200, 0x348248, 0x3482a8, 0x3482e8, 0x42c9f8, 0x42ce00, 0x43e3d0, 0x43e520
 */
__int64 __fastcall APACDecoder_Configure(
        __int64 *a1,
        _DWORD *a2,
        __int64 a3,
        unsigned __int16 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x23
  __int64 v10; // x24
  __int64 v11; // x25
  __int64 v14; // x23
  __int64 v15; // x24
  unsigned int v16; // w20
  unsigned int cache_high; // w20
  _DWORD *exception; // x0
  int v21; // w3
  int v22; // w4
  unsigned int v23; // w0
  int8x8_t v24; // d0
  _DWORD *v25; // x1
  int v26; // w4
  int8x8_t v27; // d0
  int v28; // w0
  _DWORD *v29; // x1
  int v30; // w2
  int v31; // w4
  unsigned __int8 v32; // b0
  _QWORD *v33; // x0
  unsigned __int64 v34; // x3
  __int64 v35; // x5
  unsigned __int64 v36; // x1
  unsigned __int64 v37; // x3
  int v38; // w3
  int v39; // w0
  int v40; // w3
  unsigned int v41; // w0
  _DWORD *v42; // x0
  __int64 v43; // [xsp+30h] [xbp+30h]
  __int64 v44; // [xsp+38h] [xbp+38h]
  __int64 v45; // [xsp+40h] [xbp+40h]
  unsigned int v46; // [xsp+58h] [xbp+58h] BYREF
  unsigned int v47; // [xsp+5Ch] [xbp+5Ch] BYREF
  TBitstreamReader_u32 v48; // [xsp+60h] [xbp+60h] BYREF
  __int64 v49; // [xsp+78h] [xbp+78h]
  if ( a1 )
  {
    v46 = 0;
    v47 = 0;
    v15 = *a1;
    v14 = a1[1];
    if ( a4 <= 8u || !(unsigned __int8)sub_105670(a3, a4, &v46, &v47) )
    {
      v9 = v43;
      v10 = v44;
      v16 = -6705;
      goto LABEL_4;
    }
    TBitstreamReader<unsigned int>::TBitstreamReader(&v48, (const unsigned __int8 *)(a3 + v46), v47);
    v48.bitsLeft -= 16;
    cache_high = HIWORD(v48.cache);
    if ( v48.bitsLeft < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(&v48, v48.cur);
      cache_high |= v48.cache >> (LOBYTE(v48.bitsLeft) + 32);
      v41 = v48.cache << -LOBYTE(v48.bitsLeft);
      if ( v48.bitsLeft == -32 )
        v41 = 0;
      v48.cur += 4;
      v48.cache = v41;
      v48.bitsLeft += 32;
    }
    else
    {
      v48.cache <<= 16;
    }
    if ( cache_high != 2048 )
    {
      exception = __cxa_allocate_exception(4u);
      *exception = 1650549858;
      __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
    }
    v16 = apac::GlobalConfig::Deserialize((apac__GlobalConfig *)v14, &v48);
    if ( !v16 )
    {
      *((_DWORD *)a1 + 528) = 2048;
      v21 = *(unsigned __int16 *)(v14 + 32);
      *((_WORD *)a1 + 1062) = v21;
      v22 = *(unsigned __int8 *)(v14 + 34);
      *((_WORD *)a1 + 1063) = v22;
      __printf_chk(1, "mAPACBsVersion = 0x%04x mAPACProfile = %d and mAPACProfileLevel = %d  \n", 2048, v21, v22);
      if ( *((_WORD *)a1 + 1062) != 31 )
      {
        v42 = __cxa_allocate_exception(4u);
        *v42 = 560423284;
        __cxa_throw(v42, (struct type_info *)&`typeinfo for'int, nullptr);
      }
      v23 = (*(__int64 (__fastcall **)(__int64, __int64))(*(_QWORD *)v15 + 16LL))(v15, v14);
      v25 = *(_DWORD **)(v14 + 256);
      v16 = v23;
      if ( *v25 )
      {
        v26 = (unsigned __int16)*v25;
        if ( *v25 == 0x10000 )
        {
          v24.n64_u32[0] = v25[1];
          v26 = (unsigned __int8)vaddv_s8(vcnt_s8(v24));
        }
      }
      else
      {
        v26 = v25[2];
      }
      __printf_chk(
        1,
        "## APACDecoder_Initialize ## sampleRate = %d packetSizePerChannel = %d  numChannels = %d \n",
        *(_DWORD *)(v14 + 36),
        1024,
        v26);
      if ( !v16 )
      {
        v27.n64_u32[1] = 939524096;
        *((_DWORD *)a1 + 526) = a2[7];
        v28 = a2[2];
        *((_DWORD *)a1 + 521) = 9;
        *((_DWORD *)a1 + 527) = v28;
        *((_DWORD *)a1 + 516) = 939524096;
        v29 = *(_DWORD **)(v14 + 256);
        if ( *v29 )
        {
          LOWORD(v30) = *v29;
          v31 = (unsigned __int16)v30;
          if ( *v29 == 0x10000 )
          {
            v27.n64_u32[0] = v29[1];
            v32 = vaddv_s8(vcnt_s8(v27));
            LOWORD(v30) = v32;
            v31 = v32;
          }
        }
        else
        {
          v30 = v29[2];
          v31 = (unsigned __int16)v30;
        }
        v33 = (_QWORD *)a1[262];
        v30 = (unsigned __int16)v30;
        *((_DWORD *)a1 + 518) = 1024;
        *((_WORD *)a1 + 1038) = v30;
        v34 = (unsigned __int64)(unsigned __int16)v31 << 10;
        v35 = v33[1];
        v36 = (v35 - *v33) >> 2;
        if ( v34 > v36 )
        {
          std::vector<float>::_M_default_append(v33, v34 - v36);
          v30 = *((unsigned __int16 *)a1 + 1038);
          v31 = v30;
        }
        else if ( v34 < v36 )
        {
          v37 = *v33 + ((unsigned __int64)(unsigned __int16)v31 << 12);
          if ( v35 != v37 )
          {
            v33[1] = v37;
            v31 = *((unsigned __int16 *)a1 + 1038);
            v30 = v31;
          }
        }
        *((_BYTE *)a1 + 2128) = 1;
        v38 = a2[6];
        if ( a2[2] == 12 )
          v39 = 2;
        else
          v39 = 4;
        if ( v38 != v31 )
        {
          __printf_chk(1, "## APACDecoder_Initialize Mismtch channels config:%d format:%d\n", v30, v38);
          goto LABEL_30;
        }
        v40 = a2[3];
        if ( v40 != v39 * v31 )
        {
          __printf_chk(1, "## APACDecoder_Initialize Mismtch bytes per packet config:%d format:%d\n", v39 * v31, v40);
LABEL_30:
          *((_BYTE *)a1 + 2128) = 0;
          v16 = -6748;
          v9 = v43;
          v10 = v44;
          goto LABEL_4;
        }
      }
    }
    v9 = v43;
    v10 = v44;
  }
  else
  {
    v16 = -6705;
  }
LABEL_4:
  if ( v49 != _stack_chk_guard )
  {
    v43 = v9;
    v44 = v10;
    v45 = v11;
  }
  return v16;
}


/* ------------------------------------------------------------------------ */
/* 0x00106800  size 1028 bytes  257 insns
 *   APACDecoder_Create
 *   [APACDecoder_Create]
 *   data refs: 0x72430, 0x42a278, 0x42b900, 0x42b930, 0x42ca50, 0x42ce00, 0x42d240, 0x42d910, 0x42ddd8, 0x43e520
 */
__int64 __fastcall APACDecoder_Create(
        APACDecoder ***a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x19
  __int64 v10; // x20
  __int64 v11; // x23
  __int64 v12; // x24
  __int64 v13; // x25
  __int64 v14; // x26
  __int64 v15; // x27
  __int64 v16; // x28
  APACDecoder **v18; // x20
  APACDecoder *v19; // x19
  __int64 v20; // x0
  __int64 v21; // x19
  __int64 v22; // x0
  void **v23; // x0
  void **v24; // x19
  APACDecoder *v25; // x24
  __int64 result; // x0
  void (__fastcall *v27)(APACASPDecoder *); // x1
  void *v28; // x0
  APACDecoder *v29; // x22
  void *v30; // x0
  void *v31; // x0
  _QWORD *v32; // x19
  _QWORD *i; // x23
  void *v34; // x0
  _QWORD *v35; // x19
  _QWORD *j; // x23
  void *v37; // x0
  void *v38; // x0
  void *v39; // x0
  _QWORD *v40; // x19
  _QWORD *k; // x23
  void *v42; // x0
  __int64 v43; // [xsp+10h] [xbp+10h]
  __int64 v44; // [xsp+18h] [xbp+18h]
  __int64 v45; // [xsp+30h] [xbp+30h]
  __int64 v46; // [xsp+38h] [xbp+38h]
  __int64 v47; // [xsp+40h] [xbp+40h]
  __int64 v48; // [xsp+48h] [xbp+48h]
  __int64 v49; // [xsp+50h] [xbp+50h]
  __int64 v50; // [xsp+58h] [xbp+58h]
  __int64 v51; // [xsp+68h] [xbp+68h]
  if ( !a1 )
  {
    result = 4294960591LL;
    goto LABEL_6;
  }
  v18 = (APACDecoder **)calloc(1u, 0x858u);
  if ( v18 )
  {
    v45 = v11;
    v46 = v12;
    v19 = (APACDecoder *)operator new(0x1E8u);
    APACDecoder::APACDecoder(v19);
    *v18 = v19;
    *((_QWORD *)v19 + 58) = 0;
    *((_QWORD *)v19 + 59) = 0;
    *((_QWORD *)v19 + 60) = 0;
    *(_QWORD *)v19 = off_42A288;
    v20 = operator new(0x118u);
    *(_QWORD *)(v20 + 8) = v20;
    v21 = v20;
    *(_QWORD *)v20 = off_42B910;
    *(_QWORD *)(v20 + 24) = v20;
    *(_QWORD *)(v20 + 16) = off_42B940;
    *(_WORD *)(v20 + 32) = 31;
    *(_BYTE *)(v20 + 34) = 6;
    *(_QWORD *)(v20 + 36) = 0xFFFF00000000LL;
    apac::AncillaryConfig::AncillaryConfig((apac__AncillaryConfig *)(v20 + 48));
    *(_BYTE *)(v21 + 88) = 0;
    *(_QWORD *)(v21 + 96) = 0;
    *(_QWORD *)(v21 + 104) = 0;
    *(_QWORD *)(v21 + 112) = 0;
    *(_DWORD *)(v21 + 120) = 0;
    *(_QWORD *)(v21 + 128) = 0;
    *(_QWORD *)(v21 + 136) = 0;
    *(_QWORD *)(v21 + 144) = 0;
    *(_QWORD *)(v21 + 152) = 0;
    *(_QWORD *)(v21 + 160) = 0;
    *(_QWORD *)(v21 + 168) = 0;
    *(_QWORD *)(v21 + 176) = 0;
    *(_QWORD *)(v21 + 184) = 0;
    *(_QWORD *)(v21 + 192) = 0;
    *(_QWORD *)(v21 + 200) = 0;
    *(_QWORD *)(v21 + 208) = 0;
    *(_QWORD *)(v21 + 216) = 0;
    *(_QWORD *)(v21 + 224) = 0;
    *(_QWORD *)(v21 + 232) = 0;
    *(_QWORD *)(v21 + 240) = 0;
    *(_WORD *)(v21 + 248) = 0;
    *(_QWORD *)(v21 + 256) = 0;
    *(_QWORD *)(v21 + 264) = 0;
    *(_QWORD *)(v21 + 272) = 0;
    v22 = operator new(0xCu);
    v18[1] = (APACDecoder *)v21;
    *(_QWORD *)(v21 + 256) = v22;
    *(_QWORD *)(v21 + 264) = v22 + 12;
    *(_QWORD *)v22 = 0;
    *(_DWORD *)(v22 + 8) = 0;
    *(_QWORD *)(v21 + 272) = v22 + 12;
    v23 = (void **)operator new(0x18u);
    *v23 = nullptr;
    v23[1] = nullptr;
    v24 = v23;
    v25 = *v18;
    v23[2] = nullptr;
    v18[262] = (APACDecoder *)v23;
    if ( v25 )
    {
      if ( v18[1] )
      {
        *a1 = v18;
        result = 0;
        v9 = v43;
        v10 = v44;
        v11 = v45;
        v12 = v46;
        goto LABEL_6;
      }
      v27 = *(void (__fastcall **)(APACASPDecoder *))(*(_QWORD *)v25 + 8LL);
      if ( v27 == APACASPDecoder::~APACASPDecoder )
      {
        v28 = *((void **)v25 + 58);
        *(_QWORD *)v25 = off_42A288;
        if ( v28 )
          operator delete(v28);
        APACDecoder::~APACDecoder(v25);
        operator delete(v25);
      }
      else
      {
        v27(v25);
      }
      v29 = v18[1];
      if ( !v29 )
        goto LABEL_41;
    }
    else
    {
      v29 = v18[1];
      if ( !v29 )
        goto LABEL_44;
    }
    v30 = *((void **)v29 + 32);
    if ( v30 )
      operator delete(v30);
    v31 = *((void **)v29 + 28);
    if ( v31 )
      operator delete(v31);
    v32 = *((_QWORD **)v29 + 25);
    for ( i = *((_QWORD **)v29 + 26); i != v32; ++v32 )
    {
      if ( *v32 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v32 + 8LL))(*v32);
    }
    v34 = *((void **)v29 + 25);
    if ( v34 )
      operator delete(v34);
    v35 = *((_QWORD **)v29 + 22);
    for ( j = *((_QWORD **)v29 + 23); j != v35; ++v35 )
    {
      if ( *v35 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v35 + 8LL))(*v35);
    }
    v37 = *((void **)v29 + 22);
    if ( v37 )
      operator delete(v37);
    v38 = *((void **)v29 + 19);
    if ( v38 )
      operator delete(v38);
    v39 = *((void **)v29 + 16);
    if ( v39 )
      operator delete(v39);
    v40 = *((_QWORD **)v29 + 12);
    for ( k = *((_QWORD **)v29 + 13); k != v40; ++v40 )
    {
      if ( *v40 )
        (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v40 + 8LL))(*v40);
    }
    v42 = *((void **)v29 + 12);
    if ( v42 )
      operator delete(v42);
    apac::AncillaryConfig::~AncillaryConfig((APACDecoder *)((char *)v29 + 48));
    operator delete(v29);
LABEL_41:
    v24 = (void **)v18[262];
    if ( !v24 )
    {
LABEL_45:
      free(v18);
      result = 4294960591LL;
      v9 = v43;
      v10 = v44;
      v11 = v45;
      v12 = v46;
      goto LABEL_6;
    }
    if ( *v24 )
      operator delete(*v24);
LABEL_44:
    operator delete(v24);
    goto LABEL_45;
  }
  result = 4294960568LL;
  v9 = v43;
  v10 = v44;
LABEL_6:
  if ( v51 != _stack_chk_guard )
  {
    v43 = v9;
    v44 = v10;
    v45 = v11;
    v46 = v12;
    v47 = v13;
    v48 = v14;
    v49 = v15;
    v50 = v16;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00106c10  size 492 bytes  123 insns
 *   APACDecoder_Dispose
 *   [APACDecoder_Dispose]
 *   data refs: 0x72430, 0x42a278, 0x42ca50, 0x42ce00, 0x42ddd8, 0x43e520
 */
__int64 __fastcall APACDecoder_Dispose(
        APACDecoder **ptr,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        __int64 a6,
        __int64 a7,
        __int64 a8,
        __int64 a9)
{
  __int64 v9; // x23
  APACDecoder *v11; // x19
  void (__fastcall *v12)(APACASPDecoder *); // x1
  void *v13; // x0
  APACDecoder *v14; // x19
  void *v15; // x0
  void *v16; // x0
  _QWORD *v17; // x22
  _QWORD *v18; // x23
  _QWORD *v19; // x22
  _QWORD *v20; // x23
  void *v21; // x0
  void *v22; // x0
  _QWORD *v23; // x22
  _QWORD *v24; // x23
  void **v25; // x19
  __int64 v27; // [xsp+30h] [xbp+30h]
  __int64 v28; // [xsp+48h] [xbp+48h]
  if ( ptr )
  {
    v11 = *ptr;
    if ( *ptr )
    {
      v12 = *(void (__fastcall **)(APACASPDecoder *))(*(_QWORD *)v11 + 8LL);
      if ( v12 == APACASPDecoder::~APACASPDecoder )
      {
        v13 = *((void **)v11 + 58);
        *(_QWORD *)v11 = off_42A288;
        if ( v13 )
          operator delete(v13);
        APACDecoder::~APACDecoder(v11);
        operator delete(v11);
      }
      else
      {
        v12(*ptr);
      }
    }
    v14 = ptr[1];
    if ( v14 )
    {
      v27 = v9;
      v15 = *((void **)v14 + 32);
      if ( v15 )
        operator delete(v15);
      v16 = *((void **)v14 + 28);
      if ( v16 )
        operator delete(v16);
      v17 = *((_QWORD **)v14 + 25);
      v18 = *((_QWORD **)v14 + 26);
      if ( v17 != v18 )
      {
        do
        {
          if ( *v17 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v17 + 8LL))(*v17);
          ++v17;
        }
        while ( v18 != v17 );
        v18 = *((_QWORD **)v14 + 25);
      }
      if ( v18 )
        operator delete(v18);
      v19 = *((_QWORD **)v14 + 22);
      v20 = *((_QWORD **)v14 + 23);
      if ( v19 != v20 )
      {
        do
        {
          if ( *v19 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v19 + 8LL))(*v19);
          ++v19;
        }
        while ( v20 != v19 );
        v20 = *((_QWORD **)v14 + 22);
      }
      if ( v20 )
        operator delete(v20);
      v21 = *((void **)v14 + 19);
      if ( v21 )
        operator delete(v21);
      v22 = *((void **)v14 + 16);
      if ( v22 )
        operator delete(v22);
      v23 = *((_QWORD **)v14 + 12);
      v24 = *((_QWORD **)v14 + 13);
      if ( v23 != v24 )
      {
        do
        {
          if ( *v23 )
            (*(void (__fastcall **)(_QWORD))(*(_QWORD *)*v23 + 8LL))(*v23);
          ++v23;
        }
        while ( v24 != v23 );
        v24 = *((_QWORD **)v14 + 12);
      }
      if ( v24 )
        operator delete(v24);
      apac::AncillaryConfig::~AncillaryConfig((APACDecoder *)((char *)v14 + 48));
      operator delete(v14);
      v9 = v27;
    }
    v25 = (void **)ptr[262];
    if ( v25 )
    {
      if ( *v25 )
        operator delete(*v25);
      operator delete(v25);
    }
    free(ptr);
  }
  if ( v28 != _stack_chk_guard )
    v27 = v9;
  return 0;
}


