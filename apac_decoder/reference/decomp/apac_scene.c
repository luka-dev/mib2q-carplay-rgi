/* ============================================================================
 * apac::scene
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 9 function(s), 2808 bytes of .text.
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
/* 0x000a3170    168  apac::scene::BCP47LanguageString::operator==(apac::scene::BCP47LanguageString const&) const */
/* 0x000a3220     76  apac::scene::BCP47LanguageString::Reset() */
/* 0x000a3270    940  apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int>&, char&, bool&) */
/* 0x000a3620    388  apac::scene::BCP47LanguageString::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x000a37b0    780  apac::scene::SC_Language::Deserialize(TBitstreamReader<unsigned int>&) */
/* 0x000a3ac0     84  apac::scene::SC_Language::Reset() */
/* 0x000a3b20    128  apac::scene::DecodeAngleOffset(unsigned short, unsigned short) */
/* 0x000a3ba0    144  apac::scene::DecodeDistanceRatio(unsigned short, unsigned short) */
/* 0x000a3c30    100  apac::scene::DecodeInteractivityGain(unsigned short, bool) */

/* ------------------------------------------------------------------------ */
/* 0x000a3170  size 168 bytes  42 insns
 *   apac::scene::BCP47LanguageString::operator==(apac::scene::BCP47LanguageString const&) const
 *   [_ZNK4apac5scene19BCP47LanguageStringeqERKS1_]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::scene::BCP47LanguageString::operator==(
        apac__scene__BCP47LanguageString *this,
        const apac__scene__BCP47LanguageString *a)
{
  unsigned __int64 v2; // x19
  unsigned __int64 v3; // x21
  size_t v4; // x2
  int v5; // w0
  unsigned int v6; // w1
  __int64 v7; // x19
  __int64 v9; // [xsp+40h] [xbp+40h]
  v2 = *((_QWORD *)this + 2);
  v3 = *((_QWORD *)a + 2);
  if ( v2 > v3 )
    v4 = *((_QWORD *)a + 2);
  else
    v4 = *((_QWORD *)this + 2);
  if ( !v4 || (v5 = memcmp(*((const void **)this + 1), *((const void **)a + 1), v4), v6 = 0, !v5) )
  {
    v7 = v2 - v3;
    v6 = 0;
    if ( v7 <= 0x7FFFFFFF && v7 >= (__int64)0xFFFFFFFF80000000LL )
      return (_DWORD)v7 == 0;
  }
  return v6;
}


/* ------------------------------------------------------------------------ */
/* 0x000a3220  size 76 bytes  19 insns
 *   apac::scene::BCP47LanguageString::Reset()
 *   [_ZN4apac5scene19BCP47LanguageString5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::scene::BCP47LanguageString::Reset(apac__scene__BCP47LanguageString *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_QWORD *)this + 2) = 0;
  **((_BYTE **)this + 1) = 0;
  return v2 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000a3270  size 940 bytes  234 insns
 *   apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(TBitstreamReader<unsigned int>&, char&, bool&)
 *   [_ZN4apac5scene19BCP47LanguageString17BCP47LanguageChar11DeserializeER16TBitstreamReaderIjERcRb]
 *   data refs: 0x330660, 0x330888, 0x330890, 0x42ce00, 0x42d218, 0x42d858, 0x42d9e8, 0x431ed0, 0x431ed2, 0x431ed6, 0x431eda, 0x431ede, 0x43e520
 */
__int64 __fastcall apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(
        apac__scene__BCP47LanguageString__BCP47LanguageChar *this,
        TBitstreamReader_u32 *bs,
        char *a,
        bool *a2)
{
  char v7; // w1
  unsigned int cache; // w2
  int v9; // w0
  unsigned int v10; // w19
  int v11; // w2
  unsigned __int16 v12; // w19
  unsigned int v13; // w3
  unsigned int v14; // w23
  __int16 v15; // w19
  __int64 v16; // x2
  int v18; // w2
  unsigned __int16 v19; // w19
  unsigned int v20; // w3
  unsigned int v21; // w23
  unsigned __int16 v22; // w19
  unsigned int v23; // w3
  int v24; // w0
  unsigned int v25; // w23
  __int16 v26; // w19
  unsigned int v27; // w1
  int v28; // w3
  unsigned int v29; // w1
  __int16 v30; // w19
  unsigned int v31; // w2
  const unsigned __int8 *cur; // x1
  char bitsLeft; // w3
  unsigned int v34; // w2
  const unsigned __int8 *v35; // x1
  char v36; // w3
  int v37; // w3
  const unsigned __int8 *v38; // x1
  __int64 v39; // [xsp+50h] [xbp+50h]
  *a2 = 0;
  v7 = byte_431ED2;
  if ( (unsigned int)(unsigned __int8)byte_431ED2 - 1 > 0x1F )
    goto LABEL_12;
  cache = bs->cache;
  v9 = bs->bitsLeft - (unsigned __int8)byte_431ED2;
  bs->bitsLeft = v9;
  v10 = cache >> -v7;
  if ( v9 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v27 = bs->cache;
    v28 = -bs->bitsLeft;
    v9 = bs->bitsLeft + 32;
    bs->cur += 4;
    LOWORD(v10) = v10 | (v27 >> v9);
    v29 = v27 << v28;
    if ( !v9 )
      v29 = 0;
    bs->cache = v29;
    bs->bitsLeft = v9;
  }
  else
  {
    bs->cache = cache << v7;
  }
  if ( !(_WORD)v10 )
  {
LABEL_12:
    *a = 0;
    *a2 = 1;
    return 0;
  }
  if ( (unsigned __int16)v10 == 1 )
  {
    *a = 45;
    return 0;
  }
  else
  {
    v11 = (unsigned __int8)(byte_431ED6 - byte_431ED2);
    v12 = (unsigned __int16)v10 << (byte_431ED6 - byte_431ED2);
    if ( (unsigned int)(v11 - 1) <= 0x1F )
    {
      v13 = bs->cache;
      v9 -= v11;
      bs->bitsLeft = v9;
      v14 = v13 >> -(char)v11;
      if ( v9 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v31 = bs->cache;
        cur = bs->cur;
        bitsLeft = bs->bitsLeft;
        v9 = bs->bitsLeft + 32;
        bs->bitsLeft = v9;
        bs->cur = cur + 4;
        v12 |= (unsigned __int16)(v31 >> v9) | (unsigned __int16)v14;
        if ( v9 )
          bs->cache = v31 << -bitsLeft;
        else
          bs->cache = 0;
      }
      else
      {
        bs->cache = v13 << v11;
        v12 |= v14;
      }
    }
    if ( v12 > 0x1Eu )
    {
      v18 = (unsigned __int8)(byte_431EDA - byte_431ED6);
      v19 = v12 << (byte_431EDA - byte_431ED6);
      if ( (unsigned int)(v18 - 1) <= 0x1F )
      {
        v20 = bs->cache;
        v9 -= v18;
        bs->bitsLeft = v9;
        v21 = v20 >> -(char)v18;
        if ( v9 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v34 = bs->cache;
          v35 = bs->cur;
          v36 = bs->bitsLeft;
          v9 = bs->bitsLeft + 32;
          bs->bitsLeft = v9;
          bs->cur = v35 + 4;
          v19 |= (unsigned __int16)(v34 >> v9) | (unsigned __int16)v21;
          if ( v9 )
            bs->cache = v34 << -v36;
          else
            bs->cache = 0;
        }
        else
        {
          bs->cache = v20 << v18;
          v19 |= v21;
        }
      }
      if ( v19 <= 0xFAu )
      {
        v30 = v19 - 248;
        v16 = (unsigned int)v30;
        if ( (v30 & 0x8000) == 0 )
        {
          *a = apac::scene::BCP47LanguageString::BCP47LanguageChar::kCharacterDecodingTable_8Bit[v30];
          return 0;
        }
      }
      else
      {
        v16 = (unsigned __int8)(byte_431EDE - byte_431EDA);
        v22 = v19 << (byte_431EDE - byte_431EDA);
        if ( (unsigned int)(v16 - 1) <= 0x1F )
        {
          v23 = bs->cache;
          v24 = v9 - v16;
          bs->bitsLeft = v24;
          v25 = v23 >> -(char)v16;
          if ( v24 < 0 )
          {
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
            v16 = bs->cache;
            v37 = bs->bitsLeft;
            v38 = bs->cur;
            bs->bitsLeft = v37 + 32;
            bs->cur = v38 + 4;
            v22 |= (unsigned __int16)((unsigned int)v16 >> (v37 + 32)) | (unsigned __int16)v25;
            if ( v37 == -32 )
            {
              bs->cache = 0;
            }
            else
            {
              v16 = (unsigned int)((_DWORD)v16 << -(char)v37);
              bs->cache = v16;
            }
          }
          else
          {
            v16 = v23 << v16;
            bs->cache = v16;
            v22 |= v25;
          }
        }
        if ( v22 <= 0x1FFu )
        {
          v26 = v22 - 502;
          if ( (v26 & 0x8000) == 0 )
          {
            *a = v26 + 48;
            return 0;
          }
        }
      }
    }
    else
    {
      v15 = v12 - 8;
      v16 = (unsigned int)v15;
      if ( (v15 & 0x8000) == 0 )
      {
        *a = apac::scene::BCP47LanguageString::BCP47LanguageChar::kCharacterDecodingTable_5Bit[v15];
        return 0;
      }
    }
    __printf_chk(1, "ERROR: decoding language code\n", v16);
    return 0xFFFFFFFFLL;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000a3620  size 388 bytes  96 insns
 *   apac::scene::BCP47LanguageString::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac5scene19BCP47LanguageString11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x330680, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::scene::BCP47LanguageString::Deserialize(
        apac__scene__BCP47LanguageString *this,
        TBitstreamReader_u32 *bs)
{
  __int64 v2; // x21
  __int64 v3; // x22
  __int64 v4; // x23
  __int64 v5; // x24
  __int64 v6; // x27
  __int64 v7; // x28
  _BYTE *v9; // x0
  int v11; // w20
  __int64 result; // x0
  apac__scene__BCP47LanguageString *v13; // x0
  __int64 v14; // x28
  char v15; // w5
  unsigned __int64 v16; // x27
  unsigned __int64 v17; // x1
  __int64 v18; // x1
  __int64 v19; // [xsp+20h] [xbp+20h]
  __int64 v20; // [xsp+28h] [xbp+28h]
  __int64 v21; // [xsp+30h] [xbp+30h]
  __int64 v22; // [xsp+38h] [xbp+38h]
  __int64 v23; // [xsp+50h] [xbp+50h]
  __int64 v24; // [xsp+58h] [xbp+58h]
  int v25; // [xsp+6Ch] [xbp+6Ch]
  char v26; // [xsp+76h] [xbp+76h] BYREF
  bool v27; // [xsp+77h] [xbp+77h] BYREF
  __int64 v28; // [xsp+78h] [xbp+78h]
  __int64 v29; // [xsp+80h] [xbp+80h]
  v9 = *((_BYTE **)this + 1);
  *((_QWORD *)this + 2) = 0;
  v26 = 0;
  v27 = 0;
  *v9 = 0;
  if ( v27 )
  {
LABEL_7:
    result = 0;
  }
  else
  {
    v11 = 0;
    while ( 1 )
    {
      ++v11;
      result = apac::scene::BCP47LanguageString::BCP47LanguageChar::Deserialize(this, bs, &v26, &v27);
      if ( (_DWORD)result )
        break;
      if ( v27 )
        goto LABEL_5;
      v13 = *((apac__scene__BCP47LanguageString **)this + 1);
      v14 = *((_QWORD *)this + 2);
      v15 = v26;
      v16 = v14 + 1;
      if ( v13 == (apac__scene__BCP47LanguageString *)((char *)this + 24) )
        v17 = 15;
      else
        v17 = *((_QWORD *)this + 3);
      if ( v16 > v17 )
      {
        v18 = *((_QWORD *)this + 2);
        v25 = v26;
        std::string::_M_mutate((char *)this + 8, v18, 0, 0, 1);
        v15 = v25;
        v13 = *((apac__scene__BCP47LanguageString **)this + 1);
      }
      *((_BYTE *)v13 + v14) = v15;
      *((_QWORD *)this + 2) = v16;
      *(_BYTE *)(*((_QWORD *)this + 1) + v16) = 0;
      if ( v27 )
      {
LABEL_5:
        if ( v11 != 200 )
        {
          v2 = v19;
          v3 = v20;
          v4 = v21;
          v5 = v22;
          v6 = v23;
          v7 = v24;
          goto LABEL_7;
        }
LABEL_16:
        __printf_chk(1, "ERROR: Language code exceeds permitted length\n");
        result = 0xFFFFFFFFLL;
        v2 = v19;
        v3 = v20;
        v4 = v21;
        v5 = v22;
        v6 = v23;
        v7 = v24;
        goto LABEL_8;
      }
      if ( v11 == 200 )
        goto LABEL_16;
    }
    v2 = v19;
    v3 = v20;
    v4 = v21;
    v5 = v22;
    v6 = v23;
    v7 = v24;
  }
LABEL_8:
  if ( v28 != _stack_chk_guard )
  {
    v19 = v2;
    v20 = v3;
    v21 = v4;
    v22 = v5;
    v23 = v6;
    v24 = v7;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000a37b0  size 780 bytes  194 insns
 *   apac::scene::SC_Language::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac5scene11SC_Language11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x3306b0, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::scene::SC_Language::Deserialize(apac__scene__SC_Language *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x23
  __int64 v3; // x24
  __int64 v4; // x25
  __int64 v5; // x26
  unsigned int v7; // w21
  unsigned int cache; // w1
  int v10; // w0
  unsigned int v11; // w20
  int v12; // w0
  int v13; // w23
  unsigned int v14; // w1
  unsigned int v15; // w20
  unsigned int v16; // w1
  int v17; // w2
  unsigned int v18; // w20
  int v19; // w3
  unsigned int v20; // w1
  unsigned int v21; // w20
  unsigned int v22; // w20
  int v23; // w0
  int v24; // w25
  int v25; // w0
  unsigned int v26; // w2
  const unsigned __int8 *v27; // x1
  int v28; // w3
  const unsigned __int8 *cur; // x1
  unsigned int v30; // w1
  int bitsLeft; // w4
  const unsigned __int8 *v32; // x0
  int v33; // w3
  int v34; // w0
  int v35; // w0
  unsigned int v36; // w1
  bool v37; // zf
  int v38; // w2
  unsigned int v39; // w0
  unsigned int v40; // w1
  int v41; // w3
  unsigned int v42; // w1
  __int64 v43; // [xsp+30h] [xbp+30h]
  __int64 v44; // [xsp+38h] [xbp+38h]
  __int64 v45; // [xsp+40h] [xbp+40h]
  __int64 v46; // [xsp+48h] [xbp+48h]
  __int64 v47; // [xsp+58h] [xbp+58h]
  __int64 v48; // [xsp+60h] [xbp+60h]
  v7 = apac::scene::BCP47LanguageString::Deserialize(this, bs);
  if ( !v7 )
  {
    cache = bs->cache;
    v10 = bs->bitsLeft - 1;
    bs->bitsLeft = v10;
    v11 = cache >> 31;
    if ( v10 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
      v40 = bs->cache;
      v41 = -bs->bitsLeft;
      v10 = bs->bitsLeft + 32;
      bs->cur += 4;
      LOBYTE(v11) = v11 | (v40 >> v10);
      v42 = v40 << v41;
      if ( !v10 )
        v42 = 0;
      bs->cache = v42;
      bs->bitsLeft = v10;
    }
    else
    {
      bs->cache = 2 * cache;
    }
    if ( (_BYTE)v11 )
    {
      v43 = v2;
      v12 = v10 - 3;
      v13 = 8;
      v45 = v4;
      v14 = bs->cache;
      bs->bitsLeft = v12;
      v15 = v14 >> 29;
      if ( v12 < 0 )
        goto LABEL_24;
LABEL_8:
      bs->cache = 8 * v14;
LABEL_9:
      if ( !(_BYTE)v15 )
      {
LABEL_43:
        v2 = v43;
        v4 = v45;
        goto LABEL_2;
      }
      v16 = bs->cache;
      v17 = v12 - 4;
      bs->bitsLeft = v12 - 4;
      v18 = v16 >> 28;
      if ( v12 - 4 < 0 )
      {
        cur = bs->cur;
        while ( 1 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, cur);
          v30 = bs->cache;
          bitsLeft = bs->bitsLeft;
          v32 = bs->cur;
          v17 = bitsLeft + 32;
          bs->bitsLeft = bitsLeft + 32;
          bs->cur = v32 + 4;
          v23 = bitsLeft + 32;
          v33 = (v30 >> (bitsLeft + 32)) | v18;
          if ( bitsLeft == -32 )
          {
            v20 = 0;
            bs->cache = 0;
          }
          else
          {
            v20 = v30 << -(char)bitsLeft;
            bs->cache = v20;
          }
          v22 = 1;
          if ( (unsigned int)(unsigned __int8)v33 + 3 <= 0x1F )
            break;
LABEL_16:
          if ( v22 > 0x20 )
            v24 = 32;
          else
            v24 = v22;
          v25 = v23 - v24;
          bs->bitsLeft = v25;
          if ( v25 < 0 )
          {
            TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
            v34 = bs->bitsLeft;
            bs->cur += 4;
            bs->bitsLeft = v34 + 32;
            if ( v34 == -32 )
              bs->cache = 0;
            else
              bs->cache <<= -(char)v34;
          }
          else
          {
            bs->cache = v20 << v24;
          }
          v22 -= v24;
          if ( v22 )
            goto LABEL_15;
LABEL_22:
          if ( !--v13 )
          {
            v7 = -1;
            __printf_chk(1, "ERROR: error in extension payload\n");
            v2 = v43;
            v4 = v45;
            goto LABEL_2;
          }
          v14 = bs->cache;
          v12 = bs->bitsLeft - 3;
          bs->bitsLeft = v12;
          v15 = v14 >> 29;
          if ( (v12 & 0x80000000) == 0 )
            goto LABEL_8;
LABEL_24:
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v26 = bs->cache;
          v27 = bs->cur;
          v28 = bs->bitsLeft;
          v12 = v28 + 32;
          bs->bitsLeft = v28 + 32;
          cur = v27 + 4;
          bs->cur = cur;
          LOBYTE(v15) = v15 | (v26 >> (v28 + 32));
          if ( v28 != -32 )
          {
            bs->cache = v26 << -(char)v28;
            goto LABEL_9;
          }
          bs->cache = 0;
          if ( !(_BYTE)v15 )
            goto LABEL_43;
          v18 = 0;
          bs->bitsLeft = -4;
        }
        v19 = (unsigned __int8)v33 + 4;
      }
      else
      {
        v19 = v18 + 4;
        v20 = 16 * v16;
        bs->cache = v20;
      }
      bs->bitsLeft = v17 - v19;
      v21 = v20 >> (32 - v19);
      if ( v17 - v19 < 0 )
      {
        TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
        v36 = bs->cache;
        v35 = bs->bitsLeft;
        v38 = v35 + 32;
        v37 = v35 == -32;
        LOWORD(v21) = v21 | (v36 >> (v35 + 32));
        v39 = v36 << -(char)v35;
        if ( v37 )
          v39 = 0;
        bs->cur += 4;
        bs->cache = v39;
        bs->bitsLeft = v38;
      }
      else
      {
        bs->cache = v20 << v19;
      }
      v22 = (unsigned __int16)(v21 + 1);
      if ( !v22 )
        goto LABEL_22;
LABEL_15:
      v20 = bs->cache;
      v23 = bs->bitsLeft;
      goto LABEL_16;
    }
  }
LABEL_2:
  if ( v47 != _stack_chk_guard )
  {
    v43 = v2;
    v44 = v3;
    v45 = v4;
    v46 = v5;
  }
  return v7;
}


/* ------------------------------------------------------------------------ */
/* 0x000a3ac0  size 84 bytes  21 insns
 *   apac::scene::SC_Language::Reset()
 *   [_ZN4apac5scene11SC_Language5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::scene::SC_Language::Reset(apac__scene__SC_Language *this)
{
  __int64 v3; // [xsp+28h] [xbp+28h]
  __int64 v4; // [xsp+30h] [xbp+30h]
  apac::scene::BCP47LanguageString::Reset(this);
  *((_BYTE *)this + 40) = 0;
  return v3 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000a3b20  size 128 bytes  32 insns
 *   apac::scene::DecodeAngleOffset(unsigned short, unsigned short)
 *   [_ZN4apac5scene17DecodeAngleOffsetEtt]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::scene::DecodeAngleOffset(unsigned __int16 v, unsigned __int16 v2)
{
  __int64 v3; // [xsp+18h] [xbp+18h]
  __int64 v4; // [xsp+20h] [xbp+20h]
  return v3 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000a3ba0  size 144 bytes  36 insns
 *   apac::scene::DecodeDistanceRatio(unsigned short, unsigned short)
 *   [_ZN4apac5scene19DecodeDistanceRatioEtt]
 *   data refs: 0x42ce00, 0x43e520
 */
float __fastcall apac::scene::DecodeDistanceRatio(unsigned __int16 v, unsigned __int16 v2)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  if ( v2 <= 0xFu )
    return powf(10.0, (float)((float)v * (float)(2.0 / (float)(1 << v2))) + -1.0);
  else
    return 0.0;
}


/* ------------------------------------------------------------------------ */
/* 0x000a3c30  size 100 bytes  25 insns
 *   apac::scene::DecodeInteractivityGain(unsigned short, bool)
 *   [_ZN4apac5scene23DecodeInteractivityGainEtb]
 *   data refs: 0x42ce00, 0x43e520
 */
float __fastcall apac::scene::DecodeInteractivityGain(unsigned __int16 v, bool flag)
{
  float v2; // s0
  __int64 v4; // [xsp+20h] [xbp+20h]
  v2 = 0.25;
  if ( !flag )
    v2 = 1.0;
  return (float)((float)v * v2) + -64.0;
}


