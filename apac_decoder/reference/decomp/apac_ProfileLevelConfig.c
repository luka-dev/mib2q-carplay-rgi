/* ============================================================================
 * apac::ProfileLevelConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 2272 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009d0d0    524  apac::ProfileLevelConfig::GetLevelFromCodecConfigs(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&) const */
/* 0x0009d2e0   1412  apac::ProfileLevelConfig::InferProfileFromCodecConfigs(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&, apac::ConstParamArray<apac::ProfileLevel> const&) */
/* 0x0009d870    108  apac::ProfileLevelConfig::IsDecodable(apac::Profile, apac::Profile) */
/* 0x0009d8e0    228  apac::ProfileLevelConfig::ValidateProfileLevelData(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&) const */

/* ------------------------------------------------------------------------ */
/* 0x0009d0d0  size 524 bytes  131 insns
 *   apac::ProfileLevelConfig::GetLevelFromCodecConfigs(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&) const
 *   [_ZNK4apac18ProfileLevelConfig24GetLevelFromCodecConfigsERKSt6vectorISt10unique_ptrINS_10IASCConfigESt14default_deleteIS3_EESaIS6_EE]
 *   data refs: 0x42ce00, 0x434bc8, 0x434bd0, 0x434bd8, 0x43e520
 */
__int64 __fastcall apac::ProfileLevelConfig::GetLevelFromCodecConfigs(
        apac__ProfileLevelConfig *this,
        const std_vector *vec)
{
  __int64 v2; // x21
  __int64 v3; // x22
  __int64 v4; // x27
  __int64 v5; // x28
  unsigned int v7; // w24
  int v9; // w21
  __int64 v10; // x22
  _QWORD *end; // x19
  _QWORD *begin; // x26
  char *v13; // x2
  char *v14; // x3
  __int64 v15; // x0
  bool v16; // cc
  __int64 v17; // x1
  __int64 v18; // x0
  char *v19; // x0
  int v20; // w1
  unsigned int v21; // w28
  int v22; // w0
  unsigned __int8 v23; // w0
  __int64 v24; // [xsp+20h] [xbp+20h]
  __int64 v25; // [xsp+28h] [xbp+28h]
  __int64 v26; // [xsp+50h] [xbp+50h]
  __int64 v27; // [xsp+58h] [xbp+58h]
  __int64 v28; // [xsp+68h] [xbp+68h]
  __int64 v29; // [xsp+70h] [xbp+70h]
  v7 = 255;
  if ( word_434BC8 == *(_WORD *)this )
  {
    v24 = v2;
    v25 = v3;
    v27 = v5;
    v4 = qword_434BD0;
    v9 = (unsigned __int8)((unsigned int)(qword_434BD8 - qword_434BD0) >> 4);
    if ( (((_DWORD)qword_434BD8 - (_DWORD)qword_434BD0) & 0xFF0) == 0 )
    {
LABEL_27:
      v7 = 255;
      v2 = v24;
      v3 = v25;
      v5 = v27;
      goto LABEL_2;
    }
    v10 = 0;
    v7 = 0;
LABEL_6:
    begin = vec->begin;
    end = vec->end;
    v4 += v10;
    if ( vec->begin == end )
    {
LABEL_18:
      v2 = v24;
      v3 = v25;
      v5 = v27;
      goto LABEL_2;
    }
    while ( 1 )
    {
      v13 = *(char **)v4;
      v14 = *(char **)(v4 + 8);
      v15 = (__int64)&v14[-*(_QWORD *)v4];
      v16 = v15 >> 5 > 0;
      v17 = v15 >> 3;
      v18 = v15 >> 5;
      if ( v16 )
      {
        v19 = &v13[32 * v18];
        v20 = *(_DWORD *)(*begin + 8LL);
        while ( *(_DWORD *)v13 != v20 )
        {
          if ( v20 == *((_DWORD *)v13 + 2) )
          {
            v13 += 8;
            goto LABEL_15;
          }
          if ( v20 == *((_DWORD *)v13 + 4) )
          {
            v13 += 16;
            goto LABEL_15;
          }
          if ( v20 == *((_DWORD *)v13 + 6) )
          {
            v13 += 24;
            goto LABEL_15;
          }
          v13 += 32;
          if ( v13 == v19 )
          {
            v17 = (v14 - v13) >> 3;
            goto LABEL_20;
          }
        }
        goto LABEL_15;
      }
LABEL_20:
      if ( v17 == 2 )
        break;
      if ( v17 == 3 )
      {
        v22 = *(_DWORD *)(*begin + 8LL);
        if ( *(_DWORD *)v13 == v22 )
          goto LABEL_15;
        v13 += 8;
        goto LABEL_24;
      }
      if ( v17 != 1 )
        goto LABEL_27;
      v22 = *(_DWORD *)(*begin + 8LL);
LABEL_26:
      if ( v22 != *(_DWORD *)v13 )
        goto LABEL_27;
LABEL_15:
      if ( v14 == v13 )
        goto LABEL_27;
      v21 = *((_DWORD *)v13 + 1);
      if ( v21 < (*(unsigned int (__fastcall **)(_QWORD))(*(_QWORD *)*begin + 24LL))(*begin) )
      {
        v23 = v7 + 1;
        v10 += 16;
        v7 = (unsigned __int8)(v7 + 1);
        if ( v9 == v23 )
          goto LABEL_27;
        v4 = qword_434BD0;
        goto LABEL_6;
      }
      if ( end == ++begin )
        goto LABEL_18;
    }
    v22 = *(_DWORD *)(*begin + 8LL);
LABEL_24:
    if ( v22 == *(_DWORD *)v13 )
      goto LABEL_15;
    v13 += 8;
    goto LABEL_26;
  }
LABEL_2:
  if ( v28 != _stack_chk_guard )
  {
    v24 = v2;
    v25 = v3;
    v26 = v4;
    v27 = v5;
  }
  return v7;
}


/* ------------------------------------------------------------------------ */
/* 0x0009d2e0  size 1412 bytes  353 insns
 *   apac::ProfileLevelConfig::InferProfileFromCodecConfigs(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&, apac::ConstParamArray<apac::ProfileLevel> const&)
 *   [_ZN4apac18ProfileLevelConfig28InferProfileFromCodecConfigsERKSt6vectorISt10unique_ptrINS_10IASCConfigESt14default_deleteIS3_EESaIS6_EERKNS_15ConstParamArrayINS_12ProfileLevelEEE]
 *   data refs: 0x42ce00, 0x434bc8, 0x434bd0, 0x434bd8, 0x434be0, 0x434be8, 0x43e520
 */
__int64 __fastcall apac::ProfileLevelConfig::InferProfileFromCodecConfigs(
        apac__ProfileLevelConfig *this,
        const std_vector *vec,
        const std_vector *vec2)
{
  __int64 v3; // x19
  _QWORD *v4; // x20
  __int64 v5; // x25
  __int16 *v6; // x26
  __int64 v7; // x27
  unsigned __int16 *begin; // x2
  unsigned __int16 *end; // x6
  __int64 v10; // x1
  unsigned __int16 *v11; // x1
  unsigned __int64 v12; // x3
  __int64 v13; // x3
  int v14; // w1
  _QWORD *v15; // x24
  unsigned __int8 v16; // w23
  __int64 v17; // x2
  char *v18; // x1
  char *v19; // x3
  __int64 v20; // x0
  bool v21; // cc
  __int64 v22; // x2
  __int64 v23; // x0
  char *v24; // x0
  int v25; // w2
  unsigned int v26; // w27
  int v27; // w2
  int *v28; // x1
  unsigned int v29; // w4
  __int64 v30; // x3
  int v31; // w3
  bool v32; // zf
  int v33; // w3
  bool v34; // zf
  int v35; // w3
  bool v36; // zf
  int v37; // w3
  bool v38; // zf
  int v39; // w0
  bool v40; // zf
  int v41; // w0
  bool v42; // zf
  int v43; // w0
  bool v44; // zf
  unsigned __int16 v45; // w1
  __int64 result; // x0
  _BOOL4 v47; // w3
  int v48; // w0
  int v49; // w2
  _BOOL4 v51; // w0
  __int64 v52; // [xsp+10h] [xbp+10h]
  _QWORD *v53; // [xsp+18h] [xbp+18h]
  __int64 v54; // [xsp+40h] [xbp+40h]
  __int16 *v55; // [xsp+48h] [xbp+48h]
  __int64 v56; // [xsp+50h] [xbp+50h]
  __int64 v57; // [xsp+68h] [xbp+68h]
  __int64 v58; // [xsp+70h] [xbp+70h]
  begin = (unsigned __int16 *)vec->begin;
  end = (unsigned __int16 *)vec->end;
  v10 = (char *)end - (char *)vec->begin;
  if ( v10 >> 4 > 0 )
  {
    v11 = &begin[8 * (v10 >> 4)];
    v12 = (qword_434BD8 - qword_434BD0) >> 4;
    while ( 1 )
    {
      if ( *begin == (unsigned __int16)word_434BC8 )
      {
        if ( v12 > *((unsigned __int8 *)begin + 2) )
          goto LABEL_15;
        if ( begin[2] == (unsigned __int16)word_434BC8 )
        {
LABEL_91:
          if ( v12 > *((unsigned __int8 *)begin + 6) )
          {
            begin += 2;
            goto LABEL_15;
          }
        }
      }
      else if ( begin[2] == (unsigned __int16)word_434BC8 )
      {
        goto LABEL_91;
      }
      if ( begin[4] == (unsigned __int16)word_434BC8 && v12 > *((unsigned __int8 *)begin + 10) )
      {
        begin += 4;
        goto LABEL_15;
      }
      if ( begin[6] == (unsigned __int16)word_434BC8 && v12 > *((unsigned __int8 *)begin + 14) )
      {
        begin += 6;
        goto LABEL_15;
      }
      begin += 8;
      if ( begin == v11 )
      {
        v13 = ((char *)end - (char *)begin) >> 2;
        goto LABEL_9;
      }
    }
  }
  v13 = v10 >> 2;
LABEL_9:
  if ( v13 == 2 )
  {
    v14 = (unsigned __int16)word_434BC8;
LABEL_136:
    if ( *begin == v14 )
    {
      if ( *((unsigned __int8 *)begin + 2) < (unsigned __int64)((qword_434BD8 - qword_434BD0) >> 4) )
        goto LABEL_15;
      begin += 2;
    }
    else
    {
      begin += 2;
    }
    goto LABEL_13;
  }
  if ( v13 == 3 )
  {
    v14 = (unsigned __int16)word_434BC8;
    if ( *begin == (unsigned __int16)word_434BC8
      && *((unsigned __int8 *)begin + 2) < (unsigned __int64)((qword_434BD8 - qword_434BD0) >> 4) )
    {
      goto LABEL_15;
    }
    begin += 2;
    goto LABEL_136;
  }
  if ( v13 != 1 )
  {
LABEL_86:
    v16 = 0;
    v45 = -1;
    goto LABEL_87;
  }
  v14 = (unsigned __int16)word_434BC8;
LABEL_13:
  if ( *begin != v14 || *((unsigned __int8 *)begin + 2) >= (unsigned __int64)((qword_434BD8 - qword_434BD0) >> 4) )
    goto LABEL_86;
LABEL_15:
  if ( end == begin )
    goto LABEL_86;
  v15 = *(_QWORD **)this;
  v4 = *((_QWORD **)this + 1);
  v16 = *((_BYTE *)begin + 2);
  if ( *(_QWORD **)this == v4 )
    goto LABEL_98;
  v6 = &word_434BC8;
  v56 = v7;
  do
  {
    v17 = qword_434BD0 + 16LL * v16;
    v18 = *(char **)v17;
    v19 = *(char **)(v17 + 8);
    v20 = (__int64)&v19[-*(_QWORD *)v17];
    v21 = v20 >> 5 > 0;
    v22 = v20 >> 3;
    v23 = v20 >> 5;
    if ( v21 )
    {
      v24 = &v18[32 * v23];
      v25 = *(_DWORD *)(*v15 + 8LL);
      while ( *(_DWORD *)v18 != v25 )
      {
        if ( v25 == *((_DWORD *)v18 + 2) )
        {
          v18 += 8;
          goto LABEL_26;
        }
        if ( v25 == *((_DWORD *)v18 + 4) )
        {
          v18 += 16;
          goto LABEL_26;
        }
        if ( v25 == *((_DWORD *)v18 + 6) )
        {
          v18 += 24;
          goto LABEL_26;
        }
        v18 += 32;
        if ( v24 == v18 )
        {
          v22 = (v19 - v18) >> 3;
          goto LABEL_104;
        }
      }
      goto LABEL_26;
    }
LABEL_104:
    if ( v22 == 2 )
    {
      v48 = *(_DWORD *)(*v15 + 8LL);
LABEL_108:
      if ( *(_DWORD *)v18 != v48 )
      {
        v49 = *((_DWORD *)v18 + 2);
        v18 += 8;
        if ( v49 != v48 )
          goto LABEL_85;
      }
      goto LABEL_26;
    }
    if ( v22 == 3 )
    {
      v48 = *(_DWORD *)(*v15 + 8LL);
      if ( *(_DWORD *)v18 == v48 )
        goto LABEL_26;
      v18 += 8;
      goto LABEL_108;
    }
    if ( v22 != 1 || *(_DWORD *)v18 != *(_DWORD *)(*v15 + 8LL) )
    {
LABEL_85:
      v7 = v56;
      goto LABEL_86;
    }
LABEL_26:
    if ( v19 == v18 )
      goto LABEL_85;
    v26 = *((_DWORD *)v18 + 1);
    if ( v26 < (*(unsigned int (__fastcall **)(_QWORD))(*(_QWORD *)*v15 + 24LL))(*v15) )
      goto LABEL_85;
    v27 = *(_DWORD *)(*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v15 + 32LL))(*v15);
    v28 = (int *)qword_434BE0;
    v29 = v27 & 0xFFFF0000;
    v30 = (qword_434BE8 - qword_434BE0) >> 2;
    if ( (qword_434BE8 - qword_434BE0) >> 4 > 0 )
    {
      while ( 1 )
      {
        v31 = *v28;
        if ( (unsigned __int16)*v28 || ((v31 & 0xFFFEFFFF) != 0 ? (v32 = v31 == -65536) : (v32 = 1), v32) )
          v47 = v27 == v31;
        else
          v47 = v29 == v31;
        if ( v47 )
          goto LABEL_95;
        v33 = v28[1];
        if ( (_WORD)v33 || ((v33 & 0xFFFEFFFF) != 0 ? (v34 = v33 == -65536) : (v34 = 1), v34) )
        {
          if ( v27 == v33 )
            goto LABEL_100;
        }
        else if ( v29 == v33 )
        {
LABEL_100:
          ++v28;
          goto LABEL_95;
        }
        v35 = v28[2];
        if ( (_WORD)v35 || ((v35 & 0xFFFEFFFF) != 0 ? (v36 = v35 == -65536) : (v36 = 1), v36) )
        {
          if ( v27 == v35 )
            goto LABEL_102;
        }
        else if ( v29 == v35 )
        {
LABEL_102:
          v28 += 2;
          goto LABEL_95;
        }
        v37 = v28[3];
        if ( (_WORD)v37 || ((v37 & 0xFFFEFFFF) != 0 ? (v38 = v37 == -65536) : (v38 = 1), v38) )
        {
          if ( v27 == v37 )
            goto LABEL_94;
        }
        else if ( v29 == v37 )
        {
LABEL_94:
          v28 += 3;
          goto LABEL_95;
        }
        v28 += 4;
        if ( v28 == (int *)(qword_434BE0 + 16 * ((qword_434BE8 - qword_434BE0) >> 4)) )
        {
          v30 = (qword_434BE8 - (__int64)v28) >> 2;
          break;
        }
      }
    }
    if ( v30 != 2 )
    {
      if ( v30 != 3 )
      {
        if ( v30 != 1 )
          goto LABEL_85;
        v43 = *v28;
        if ( (unsigned __int16)*v28 )
          goto LABEL_84;
        if ( (v43 & 0xFFFEFFFF) == 0 || v43 == -65536 )
          goto LABEL_84;
        goto LABEL_129;
      }
      v39 = *v28;
      if ( (unsigned __int16)*v28 || ((v39 & 0xFFFEFFFF) != 0 ? (v40 = v39 == -65536) : (v40 = 1), v40) )
      {
        if ( v27 == v39 )
          goto LABEL_95;
      }
      else if ( v29 == v39 )
      {
        goto LABEL_95;
      }
      ++v28;
    }
    v41 = *v28;
    if ( (unsigned __int16)*v28 || ((v41 & 0xFFFEFFFF) != 0 ? (v42 = v41 == -65536) : (v42 = 1), v42) )
      v51 = v27 == v41;
    else
      v51 = v29 == v41;
    if ( v51 )
      goto LABEL_95;
    v43 = v28[1];
    ++v28;
    if ( (_WORD)v43 || ((v43 & 0xFFFEFFFF) != 0 ? (v44 = v43 == -65536) : (v44 = 1), v44) )
    {
LABEL_84:
      if ( v27 != v43 )
        goto LABEL_85;
      goto LABEL_95;
    }
LABEL_129:
    if ( v29 != v43 )
      goto LABEL_85;
LABEL_95:
    if ( (int *)qword_434BE8 == v28 )
      goto LABEL_85;
    ++v15;
  }
  while ( v4 != v15 );
  v7 = v56;
LABEL_98:
  v45 = word_434BC8;
LABEL_87:
  result = v45 | (v16 << 16);
  if ( v57 != _stack_chk_guard )
  {
    v52 = v3;
    v53 = v4;
    v54 = v5;
    v55 = v6;
    v56 = v7;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0009d870  size 108 bytes  27 insns
 *   apac::ProfileLevelConfig::IsDecodable(apac::Profile, apac::Profile)
 *   [_ZN4apac18ProfileLevelConfig11IsDecodableENS_7ProfileES1_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::ProfileLevelConfig::IsDecodable(apac__ProfileLevelConfig *this, int n, int n2)
{
  unsigned int v3; // w4
  __int64 result; // x0
  __int64 v5; // [xsp+20h] [xbp+20h]
  v3 = (unsigned __int16)this;
  result = 0;
  if ( v3 <= 0x1F )
    return ((unsigned __int16)n <= 0x1Fu) & (unsigned __int8)(v3 <= (unsigned __int16)n);
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x0009d8e0  size 228 bytes  57 insns
 *   apac::ProfileLevelConfig::ValidateProfileLevelData(std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > > const&) const
 *   [_ZNK4apac18ProfileLevelConfig24ValidateProfileLevelDataERKSt6vectorISt10unique_ptrINS_10IASCConfigESt14default_deleteIS3_EESaIS6_EE]
 *   data refs: 0x330080, 0x42ce00, 0x42d530, 0x43e520
 */
__int64 __fastcall apac::ProfileLevelConfig::ValidateProfileLevelData(
        apac__ProfileLevelConfig *this,
        const std_vector *vec)
{
  int v4; // w1
  __int64 result; // x0
  __int64 v6; // x0
  unsigned int v7; // w21
  unsigned __int8 LevelFromCodecConfigs; // w0
  unsigned int v9; // w1
  std_vector v11; // [xsp+38h] [xbp+38h] BYREF
  __int64 v12; // [xsp+50h] [xbp+50h]
  v4 = *(unsigned __int16 *)this;
  result = 4;
  if ( v4 == 31 && *((unsigned __int8 *)this + 2) <= 6u )
  {
    v11.begin = &apac::kSupportedProfileLevels;
    v11.end = &unk_330084;
    v6 = apac::ProfileLevelConfig::InferProfileFromCodecConfigs(
           (apac__ProfileLevelConfig *)vec,
           &v11,
           (const std_vector *)&unk_330084);
    v7 = BYTE2(v6);
    if ( (unsigned __int16)v6 == 0xFFFF
      || !(unsigned __int8)apac::ProfileLevelConfig::IsDecodable(
                             (apac__ProfileLevelConfig *)*(unsigned __int16 *)this,
                             (unsigned __int16)v6,
                             0xFFFF) )
    {
      return 1;
    }
    else
    {
      LevelFromCodecConfigs = apac::ProfileLevelConfig::GetLevelFromCodecConfigs(this, vec);
      v9 = *((unsigned __int8 *)this + 2);
      return v9 < LevelFromCodecConfigs || v9 > v7;
    }
  }
  return result;
}


