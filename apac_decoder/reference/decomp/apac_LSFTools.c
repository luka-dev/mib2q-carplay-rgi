/* ============================================================================
 * apac::LSFTools
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 15 function(s), 4776 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x0009c6e0     96  apac::LSFTools::ConditionLSFs(float*) */
/* 0x0009cd40    416  apac::LSFTools::~LSFTools() */
/* 0x0009cee0    392  apac::LSFTools::~LSFTools() */
/* 0x000b4220    268  apac::LSFTools::get_LSFWeightingFactors(float const*, float*, unsigned short, float) */
/* 0x000b4330     96  apac::LSFTools::LSP_To_LPC() */
/* 0x000b4390    200  apac::LSFTools::Condition_LSFs(float*, float, float) */
/* 0x000b4460    440  apac::LSFTools::LSP_To_LPC(float const*, float*, unsigned short) */
/* 0x000b4f30    224  apac::LSFTools::DecodeLSF(unsigned short const*) */
/* 0x000b5010    216  apac::LSFTools::LSP_To_LSF(float const*, float*, unsigned short, float) */
/* 0x000b50f0    204  apac::LSFTools::LSF_To_LSP(float const*, float*, float) */
/* 0x000b51c0    204  apac::LSFTools::LSF_To_LSP(float*, float) */
/* 0x000b5290    212  apac::LSFTools::Reset() */
/* 0x000b5370    312  apac::LSFTools::LSF_To_LPC(float*, float*) */
/* 0x000b5870    404  apac::LSFTools::QuantizeLSF(unsigned short*) */
/* 0x000b5a10   1092  apac::LSFTools::Initialize() */

/* ------------------------------------------------------------------------ */
/* 0x0009c6e0  size 96 bytes  23 insns
 *   apac::LSFTools::ConditionLSFs(float*)
 *   [_ZN4apac8LSFTools13ConditionLSFsEPf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::ConditionLSFs(apac__LSFTools *this, float *buf)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return (*(__int64 (__fastcall **)(apac__LSFTools *, float *, float, float))(*(_QWORD *)this + 128LL))(
           this,
           buf,
           50.0,
           24000.0);
}


/* ------------------------------------------------------------------------ */
/* 0x0009cd40  size 416 bytes  104 insns
 *   apac::LSFTools::~LSFTools()
 *   apac::LSFTools::~LSFTools()
 *   [_ZN4apac8LSFToolsD2Ev]
 *   [_ZN4apac8LSFToolsD1Ev]
 *   data refs: 0x79810, 0x42a848, 0x42b750, 0x42b798, 0x42ce00, 0x42d070, 0x42d7d8, 0x42dbd8, 0x42ddc0, 0x43e520
 */
// Alternative name is '_ZN4apac8LSFToolsD1Ev'
void __fastcall apac::LSFTools::~LSFTools(apac__LSFTools *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void *v5; // x0
  void *v6; // x0
  void *v7; // x0
  _QWORD **v8; // x21
  _QWORD **v9; // x20
  _QWORD *v10; // x23
  void *v11; // x0
  void *v12; // x0
  _QWORD *v13; // x20
  void (__fastcall *v14)(apac__Autocorr *); // x1
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  __int64 v18; // [xsp+30h] [xbp+30h]
  __int64 v19; // [xsp+48h] [xbp+48h]
  __int64 v20; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 27);
  *(_QWORD *)this = off_42B7A8;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 24);
  if ( v4 )
    operator delete(v4);
  v5 = *((void **)this + 21);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 18);
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 15);
  if ( v7 )
    operator delete(v7);
  *(_QWORD *)this = &unk_42B760;
  v8 = *((_QWORD ***)this + 11);
  v9 = *((_QWORD ***)this + 10);
  if ( v9 != v8 )
  {
    v18 = v1;
    do
    {
      v10 = *v9;
      if ( *v9 )
      {
        v11 = (void *)v10[6];
        if ( v11 )
          operator delete(v11);
        v12 = (void *)v10[2];
        if ( v12 )
          operator delete(v12);
        operator delete(v10);
      }
      ++v9;
    }
    while ( v8 != v9 );
    v1 = v18;
    v8 = *((_QWORD ***)this + 10);
  }
  if ( v8 )
    operator delete(v8);
  v13 = *((_QWORD **)this + 9);
  if ( v13 )
  {
    v14 = *(void (__fastcall **)(apac__Autocorr *))(*v13 + 8LL);
    if ( v14 == apac::Autocorr::~Autocorr )
    {
      v15 = (void *)v13[2];
      *v13 = off_42A858;
      if ( v15 )
        operator delete(v15);
      operator delete(v13);
    }
    else
    {
      v14(*((apac__Autocorr **)this + 9));
    }
  }
  v16 = *((void **)this + 6);
  if ( v16 )
    operator delete(v16);
  v17 = *((void **)this + 3);
  if ( v17 )
  {
    if ( v19 == _stack_chk_guard )
    {
      operator delete(v17);
      return;
    }
LABEL_36:
    v18 = v1;
  }
  if ( v19 != _stack_chk_guard )
    goto LABEL_36;
}


/* ------------------------------------------------------------------------ */
/* 0x0009cee0  size 392 bytes  98 insns
 *   apac::LSFTools::~LSFTools()
 *   [_ZN4apac8LSFToolsD0Ev]
 *   data refs: 0x79810, 0x42a848, 0x42b750, 0x42b798, 0x42ce00, 0x42d070, 0x42d7d8, 0x42dbd8, 0x42ddc0, 0x43e520
 */
void __fastcall apac::LSFTools::~LSFTools(apac__LSFTools *this)
{
  __int64 v1; // x23
  void *v3; // x0
  void *v4; // x0
  void *v5; // x0
  void *v6; // x0
  void *v7; // x0
  _QWORD **v8; // x21
  _QWORD **v9; // x20
  _QWORD *v10; // x23
  void *v11; // x0
  void *v12; // x0
  _QWORD *v13; // x20
  void (__fastcall *v14)(apac__Autocorr *); // x1
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  __int64 v18; // [xsp+30h] [xbp+30h]
  __int64 v19; // [xsp+48h] [xbp+48h]
  __int64 v20; // [xsp+50h] [xbp+50h]
  v3 = *((void **)this + 27);
  *(_QWORD *)this = off_42B7A8;
  if ( v3 )
    operator delete(v3);
  v4 = *((void **)this + 24);
  if ( v4 )
    operator delete(v4);
  v5 = *((void **)this + 21);
  if ( v5 )
    operator delete(v5);
  v6 = *((void **)this + 18);
  if ( v6 )
    operator delete(v6);
  v7 = *((void **)this + 15);
  if ( v7 )
    operator delete(v7);
  *(_QWORD *)this = &unk_42B760;
  v8 = *((_QWORD ***)this + 11);
  v9 = *((_QWORD ***)this + 10);
  if ( v9 != v8 )
  {
    v18 = v1;
    do
    {
      v10 = *v9;
      if ( *v9 )
      {
        v11 = (void *)v10[6];
        if ( v11 )
          operator delete(v11);
        v12 = (void *)v10[2];
        if ( v12 )
          operator delete(v12);
        operator delete(v10);
      }
      ++v9;
    }
    while ( v8 != v9 );
    v1 = v18;
    v8 = *((_QWORD ***)this + 10);
  }
  if ( v8 )
    operator delete(v8);
  v13 = *((_QWORD **)this + 9);
  if ( v13 )
  {
    v14 = *(void (__fastcall **)(apac__Autocorr *))(*v13 + 8LL);
    if ( v14 == apac::Autocorr::~Autocorr )
    {
      v15 = (void *)v13[2];
      *v13 = off_42A858;
      if ( v15 )
        operator delete(v15);
      operator delete(v13);
    }
    else
    {
      v14(*((apac__Autocorr **)this + 9));
    }
  }
  v16 = *((void **)this + 6);
  if ( v16 )
    operator delete(v16);
  v17 = *((void **)this + 3);
  if ( v17 )
    operator delete(v17);
  if ( v19 != _stack_chk_guard )
    v18 = v1;
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x000b4220  size 268 bytes  67 insns
 *   apac::LSFTools::get_LSFWeightingFactors(float const*, float*, unsigned short, float)
 *   [_ZN4apac8LSFTools23get_LSFWeightingFactorsEPKfPftf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::get_LSFWeightingFactors(
        apac__LSFTools *this,
        const float *a,
        float *buf,
        unsigned __int16 v,
        float f)
{
  float v5; // s4
  int v6; // w9
  float v7; // s1
  unsigned __int16 v8; // w7
  __int64 v9; // x0
  __int64 v10; // x6
  __int64 v11; // x5
  float v12; // s2
  float *v13; // x5
  float v14; // s2
  __int64 v16; // [xsp+18h] [xbp+18h]
  __int64 v17; // [xsp+20h] [xbp+20h]
  v5 = *a;
  v6 = v - 2;
  v7 = 1.0 / *a;
  if ( v6 <= 1 )
  {
    v13 = buf;
    v10 = 1;
  }
  else
  {
    v8 = 1;
    v9 = 1;
    while ( 1 )
    {
      v8 += 2;
      v10 = v8;
      v11 = v10 * 4 - 4;
      v12 = 1.0 / (float)(a[v9] - v5);
      buf[v9 - 1] = v12 + v7;
      v7 = 1.0 / (float)(a[v9 + 1] - a[v9]);
      buf[v9] = v12 + v7;
      v9 = v8;
      if ( v6 <= v8 )
        break;
      v5 = *(const float *)((char *)a + v11);
    }
    v13 = (float *)((char *)buf + v11);
    v5 = a[v10 - 1];
  }
  v14 = 1.0 / (float)(a[v10] - v5);
  *v13 = v14 + v7;
  buf[v10] = (float)(1.0 / (float)-(float)(a[v10] - (float)(f * 0.5))) + v14;
  return v16 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4330  size 96 bytes  24 insns
 *   apac::LSFTools::LSP_To_LPC()
 *   [_ZN4apac8LSFTools10LSP_To_LPCEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::LSP_To_LPC(apac__LSFTools *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return (*(__int64 (__fastcall **)(apac__LSFTools *, _QWORD, _QWORD, _QWORD))(*(_QWORD *)this + 112LL))(
           this,
           *((_QWORD *)this + 18),
           *((_QWORD *)this + 3),
           (unsigned __int16)((*((_QWORD *)this + 19) - *((_QWORD *)this + 18)) >> 2));
}


/* ------------------------------------------------------------------------ */
/* 0x000b4390  size 200 bytes  50 insns
 *   apac::LSFTools::Condition_LSFs(float*, float, float)
 *   [_ZN4apac8LSFTools14Condition_LSFsEPfff]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::Condition_LSFs(apac__LSFTools *this, float *buf, float f, float f2)
{
  float v4; // s1
  unsigned int v5; // w3
  float v6; // s3
  __int64 v7; // x2
  float v8; // s2
  __int16 v9; // w3
  __int64 v10; // x0
  float v11; // s2
  __int64 v13; // [xsp+18h] [xbp+18h]
  __int64 v14; // [xsp+20h] [xbp+20h]
  v4 = -(float)(f - (float)(f2 * 0.5));
  v5 = *((unsigned __int16 *)this + 52);
  if ( *((_WORD *)this + 52) )
  {
    v6 = f;
    v7 = 0;
    do
    {
      v8 = buf[v7];
      if ( v8 < v6 )
      {
        v8 = v6;
        buf[v7] = v6;
        v5 = *((unsigned __int16 *)this + 52);
      }
      ++v7;
      v6 = f + v8;
    }
    while ( v5 > (unsigned int)v7 );
    if ( buf[(unsigned __int16)v5 - 1] > v4 )
    {
      v9 = v5 - 1;
      if ( (v9 & 0x8000) == 0 )
      {
        v10 = v9;
        do
        {
          v11 = buf[v10];
          if ( v11 > v4 )
          {
            v11 = v4;
            buf[v10] = v4;
          }
          v4 = v11 - f;
          --v10;
        }
        while ( (v10 & 0x8000) == 0 );
      }
    }
  }
  return v13 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4460  size 440 bytes  110 insns
 *   apac::LSFTools::LSP_To_LPC(float const*, float*, unsigned short)
 *   [_ZN4apac8LSFTools10LSP_To_LPCEPKfPft]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::LSP_To_LPC(apac__LSFTools *this, const float *a, float *buf, unsigned __int16 v)
{
  _QWORD *v4; // x5
  unsigned int v5; // w20
  int v6; // w4
  _QWORD *v9; // x1
  __int64 v11; // x1
  _QWORD *v12; // x0
  __int64 v13; // x4
  _QWORD *v14; // x2
  __int64 v15; // x3
  float *v16; // x1
  float *v17; // x0
  unsigned __int16 v18; // w5
  float v19; // s0
  float *v20; // x3
  float *v21; // x1
  __int64 v22; // x0
  float v23; // s0
  _QWORD v25[12]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v26; // [xsp+60h] [xbp+60h]
  v4 = &v25[-1536];
  v5 = v >> 1;
  v6 = 4 * (unsigned __int16)(v5 + 1) + 15;
  v9 = (_QWORD *)((char *)&v25[-1536] - (*(_QWORD *)&v6 & 0x7F000LL));
  if ( &v25[-1536] != v9 )
  {
    do
    {
      v4 -= 512;
      *v4 = 0;
    }
    while ( v4 != v9 );
  }
  *(_QWORD *)((char *)v4 - ((*(_QWORD *)&v6 & 0x7FFF0LL) - (*(_QWORD *)&v6 & 0x7F000LL))) = 0;
  v11 = *(_QWORD *)&v6 & 0x7F000LL;
  v12 = &v25[-1536];
  v13 = *(_QWORD *)&v6 & 0x7FFF0LL;
  v14 = (_QWORD *)((char *)&v25[-1536] - v11);
  if ( &v25[-1536] != v14 )
  {
    do
    {
      v12 -= 512;
      *v12 = 0;
    }
    while ( v12 != v14 );
  }
  *(_QWORD *)((char *)v12 - (v13 - v11)) = 0;
  sub_B40B0(a, v25, v >> 1, 0);
  sub_B40B0(a + 1, v25, v5, v15);
  v16 = (float *)v25 + (v5 & 0x7FFF) - 1;
  v17 = v16;
  v18 = 0;
  do
  {
    --v17;
    --v16;
    ++v18;
    v19 = v16[2] - v16[1];
    v17[2] = v17[2] + v17[1];
    v16[2] = v19;
  }
  while ( (unsigned __int16)(v5 - 1) >= (unsigned int)v18 );
  v20 = &buf[v];
  v21 = (float *)((char *)v20 + ((4LL * (unsigned __int16)(v5 - 1)) ^ 0xFFFFFFFFFFFFFFFCLL));
  v22 = 1;
  *buf = 1.0;
  do
  {
    v23 = *(float *)((char *)v25 + v22 * 4);
    buf[v22++] = (float)(v23 + v23) * 0.5;
    *v20-- = (float)(v23 - v23) * 0.5;
  }
  while ( v21 != v20 );
  return v25[11] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b4f30  size 224 bytes  56 insns
 *   apac::LSFTools::DecodeLSF(unsigned short const*)
 *   [_ZN4apac8LSFTools9DecodeLSFEPKt]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::DecodeLSF(apac__LSFTools *this, const unsigned __int16 *a)
{
  _DWORD *i; // x0
  _DWORD *v5; // x2
  __int64 v6; // x0
  __int64 v7; // x24
  __int64 v8; // x19
  unsigned __int64 v9; // x23
  __int64 v12; // [xsp+48h] [xbp+48h]
  __int64 v13; // [xsp+50h] [xbp+50h]
  v5 = *((_DWORD **)this + 24);
  for ( i = *((_DWORD **)this + 25); v5 != i; ++v5 )
    *v5 = 0;
  v7 = *((_QWORD *)this + 10);
  v6 = *((_QWORD *)this + 11);
  if ( v7 != v6 )
  {
    v8 = 0;
    v9 = (unsigned __int64)(v6 - 8 - v7) >> 3;
    do
      vDSP_vadd(
        *(_QWORD *)(*(_QWORD *)(v7 + 8 * v8) + 8LL)
      + 4LL * a[(unsigned __int16)v8] * *(unsigned __int16 *)(*(_QWORD *)(v7 + 8 * v8) + 2LL),
        1,
        *((_QWORD *)this + 24),
        1,
        *((_QWORD *)this + 24),
        1,
        *((unsigned __int16 *)this + 52));
    while ( v8++ != v9 );
  }
  return v12 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5010  size 216 bytes  54 insns
 *   apac::LSFTools::LSP_To_LSF(float const*, float*, unsigned short, float)
 *   [_ZN4apac8LSFTools10LSP_To_LSFEPKfPftf]
 *   data refs: 0x332f78, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::LSP_To_LSF(
        apac__LSFTools *this,
        const float *a,
        float *buf,
        unsigned __int16 v,
        float f)
{
  __int64 v5; // x19
  __int64 v6; // x20
  __int64 v7; // d8
  __int64 v8; // d9
  __int64 v9; // d10
  float v13; // s0
  bool v14; // zf
  float v15; // s1
  __int64 result; // x0
  __int64 v17; // [xsp+10h] [xbp+10h]
  __int64 v18; // [xsp+18h] [xbp+18h]
  __int64 v19; // [xsp+38h] [xbp+38h]
  __int64 v20; // [xsp+40h] [xbp+40h]
  __int64 v21; // [xsp+48h] [xbp+48h]
  __int64 v22; // [xsp+58h] [xbp+58h]
  __int64 v23; // [xsp+60h] [xbp+60h]
  if ( v )
  {
    v20 = v7;
    v21 = v8;
    v6 = (unsigned __int16)(v - 1);
    v5 = 0;
    v19 = v9;
    do
    {
      v13 = acosf(a[v5]);
      v14 = v6 == v5;
      v15 = (float)(f * 0.5) / 3.14159265 * v13;
      buf[v5++] = v15;
    }
    while ( !v14 );
    v7 = v20;
    v8 = v21;
    v9 = v19;
  }
  result = v22 ^ _stack_chk_guard;
  if ( v22 != _stack_chk_guard )
  {
    v17 = v5;
    v18 = v6;
    v19 = v9;
    v20 = v7;
    v21 = v8;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b50f0  size 204 bytes  51 insns
 *   apac::LSFTools::LSF_To_LSP(float const*, float*, float)
 *   [_ZN4apac8LSFTools10LSF_To_LSPEPKfPff]
 *   data refs: 0x332f78, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::LSF_To_LSP(apac__LSFTools *this, const float *a, float *buf, float f)
{
  __int64 v4; // x19
  __int64 v5; // x20
  __int64 v6; // d8
  __int64 v7; // d9
  unsigned int v11; // w19
  double v12; // d8
  __int64 v13; // x20
  float v14; // s0
  unsigned __int16 v15; // w3
  __int64 result; // x0
  __int64 v17; // [xsp+10h] [xbp+10h]
  __int64 v18; // [xsp+18h] [xbp+18h]
  __int64 v19; // [xsp+40h] [xbp+40h]
  __int64 v20; // [xsp+48h] [xbp+48h]
  __int64 v21; // [xsp+58h] [xbp+58h]
  __int64 v22; // [xsp+60h] [xbp+60h]
  if ( *((_WORD *)this + 52) )
  {
    v11 = 0;
    v12 = (float)(f * 0.5);
    do
    {
      v13 = v11;
      v14 = cos(a[v11] * 3.14159265 / v12);
      v15 = v11 + 1;
      v11 = (unsigned __int16)(v11 + 1);
      buf[v13] = v14;
    }
    while ( *((unsigned __int16 *)this + 52) > (unsigned int)v15 );
    v4 = v17;
    v5 = v18;
    v6 = v19;
    v7 = v20;
  }
  result = v21 ^ _stack_chk_guard;
  if ( v21 != _stack_chk_guard )
  {
    v17 = v4;
    v18 = v5;
    v19 = v6;
    v20 = v7;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b51c0  size 204 bytes  51 insns
 *   apac::LSFTools::LSF_To_LSP(float*, float)
 *   [_ZN4apac8LSFTools10LSF_To_LSPEPff]
 *   data refs: 0x332f78, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::LSF_To_LSP(apac__LSFTools *this, float *buf, float f)
{
  __int64 v3; // x19
  __int64 v4; // x20
  __int64 v5; // d8
  __int64 v6; // d9
  unsigned int v9; // w19
  double v10; // d8
  __int64 v11; // x20
  float v12; // s0
  unsigned __int16 v13; // w2
  __int64 result; // x0
  __int64 v15; // [xsp+10h] [xbp+10h]
  __int64 v16; // [xsp+18h] [xbp+18h]
  __int64 v17; // [xsp+40h] [xbp+40h]
  __int64 v18; // [xsp+48h] [xbp+48h]
  __int64 v19; // [xsp+58h] [xbp+58h]
  __int64 v20; // [xsp+60h] [xbp+60h]
  if ( *((_WORD *)this + 52) )
  {
    v9 = 0;
    v10 = (float)(f * 0.5);
    do
    {
      v11 = v9;
      v12 = cos(buf[v9] * 3.14159265 / v10);
      v13 = v9 + 1;
      v9 = (unsigned __int16)(v9 + 1);
      buf[v11] = v12;
    }
    while ( *((unsigned __int16 *)this + 52) > (unsigned int)v13 );
    v3 = v15;
    v4 = v16;
    v5 = v17;
    v6 = v18;
  }
  result = v19 ^ _stack_chk_guard;
  if ( v19 != _stack_chk_guard )
  {
    v15 = v3;
    v16 = v4;
    v17 = v5;
    v18 = v6;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5290  size 212 bytes  52 insns
 *   apac::LSFTools::Reset()
 *   [_ZN4apac8LSFTools5ResetEv]
 *   data refs: 0x332f80, 0x332fc0, 0x332fc8, 0x332fd0, 0x332fd8, 0x332fe0, 0x332fe8, 0x332ff0, 0x332ff8, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::Reset(apac__LSFTools *this)
{
  _QWORD *v1; // x1
  _DWORD *v2; // x1
  _DWORD *i; // x2
  _DWORD *v4; // x1
  _DWORD *j; // x2
  _DWORD *v6; // x1
  _DWORD *k; // x2
  _DWORD *m; // x0
  _DWORD *v9; // x1
  __int64 v11; // [xsp+18h] [xbp+18h]
  __int64 v12; // [xsp+20h] [xbp+20h]
  v1 = *((_QWORD **)this + 15);
  v1[4] = 0xBE483127BCB78034LL;
  v1[5] = 0xBF051EB8BEBB4A23LL;
  *v1 = 0x3F7779A73F7D6A16LL;
  v1[1] = 0x3F48A0903F66594BLL;
  v1[2] = 0x3F003AFB3F28793ELL;
  v1[3] = 0x3E283E423EA95183LL;
  v1[6] = 0xBF4DD2F2BF2C49BALL;
  v1[7] = 0xBF7851ECBF67DBF5LL;
  v2 = *((_DWORD **)this + 21);
  for ( i = *((_DWORD **)this + 22); v2 != i; ++v2 )
    *v2 = 0;
  v4 = *((_DWORD **)this + 24);
  for ( j = *((_DWORD **)this + 25); v4 != j; ++v4 )
    *v4 = 0;
  v6 = *((_DWORD **)this + 18);
  for ( k = *((_DWORD **)this + 19); v6 != k; ++v6 )
    *v6 = 0;
  v9 = *((_DWORD **)this + 27);
  for ( m = *((_DWORD **)this + 28); v9 != m; ++v9 )
    *v9 = 0;
  return v11 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5370  size 312 bytes  77 insns
 *   apac::LSFTools::LSF_To_LPC(float*, float*)
 *   [_ZN4apac8LSFTools10LSF_To_LPCEPfS1_]
 *   data refs: 0x9c6e0, 0x42ce00, 0x42d740, 0x43e520
 */
__int64 __fastcall apac::LSFTools::LSF_To_LPC(apac__LSFTools *this, float *buf, float *buf2)
{
  _QWORD *v5; // x3
  int v7; // w0
  __int64 v8; // x1
  __int64 v9; // x0
  _QWORD *v10; // x2
  __int64 (__fastcall *v11)(apac__LSFTools *, float *); // x2
  _QWORD v13[10]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v14; // [xsp+50h] [xbp+50h]
  v5 = &v13[-1536];
  v7 = 4 * *((unsigned __int16 *)this + 52) + 15;
  v8 = *(_QWORD *)&v7 & 0x7F000LL;
  v9 = *(_QWORD *)&v7 & 0x7FFF0LL;
  v10 = (_QWORD *)((char *)&v13[-1536] - v8);
  if ( &v13[-1536] != v10 )
  {
    do
    {
      v5 -= 512;
      *v5 = 0;
    }
    while ( v5 != v10 );
  }
  *(_QWORD *)((char *)v5 - (v9 - v8)) = 0;
  v11 = *(__int64 (__fastcall **)(apac__LSFTools *, float *))(*(_QWORD *)this + 136LL);
  if ( v11 == apac::LSFTools::ConditionLSFs )
    (*(void (__fastcall **)(apac__LSFTools *, float *, float, float))(*(_QWORD *)this + 128LL))(
      this,
      buf,
      50.0,
      24000.0);
  else
    v11(this, buf);
  (*(void (__fastcall **)(apac__LSFTools *, float *, _QWORD *, float))(*(_QWORD *)this + 96LL))(
    this,
    buf,
    v13,
    *((float *)this + 28));
  (*(void (__fastcall **)(apac__LSFTools *, _QWORD *, float *, _QWORD))(*(_QWORD *)this + 112LL))(
    this,
    v13,
    buf2,
    *((unsigned __int16 *)this + 52));
  return v13[9] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5870  size 404 bytes  101 insns
 *   apac::LSFTools::QuantizeLSF(unsigned short*)
 *   [_ZN4apac8LSFTools11QuantizeLSFEPt]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::QuantizeLSF(apac__LSFTools *this, unsigned __int16 *v)
{
  _QWORD *v3; // x3
  unsigned __int16 v5; // w2
  int v6; // w0
  __int64 v7; // x4
  __int64 v8; // x0
  _QWORD *v9; // x5
  __int64 v10; // x20
  _QWORD v12[10]; // [xsp+0h] [xbp+0h] BYREF
  __int64 v13; // [xsp+50h] [xbp+50h]
  v3 = &v12[-1536];
  v5 = *((_WORD *)this + 52);
  v6 = 4 * v5 + 15;
  v7 = *(_QWORD *)&v6 & 0x7F000LL;
  v8 = *(_QWORD *)&v6 & 0x7FFF0LL;
  v9 = (_QWORD *)((char *)&v12[-1536] - v7);
  if ( &v12[-1536] != v9 )
  {
    do
    {
      v3 -= 512;
      *v3 = 0;
    }
    while ( v3 != v9 );
  }
  *(_QWORD *)((char *)v3 - (v8 - v7)) = 0;
  apac::LPCodeBook::LSF_Quant_Weighting(**((apac__LPCodeBook ***)this + 10), *((const float **)this + 21), v5);
  apac::LPCodeBook::VQSearch(**((apac__LPCodeBook ***)this + 10), *((const float **)this + 21));
  *v = **(_DWORD **)(**((_QWORD **)this + 10) + 16LL);
  v10 = *(_QWORD *)(**((_QWORD **)this + 10) + 8LL)
      + 4LL * *(unsigned __int16 *)(**((_QWORD **)this + 10) + 2LL) * **(_DWORD **)(**((_QWORD **)this + 10) + 16LL);
  vDSP_vsub(v10, 1, *((_QWORD *)this + 21), 1, v12, 1, *((unsigned __int16 *)this + 52));
  std::vector<float>::operator=(*(_QWORD *)(*((_QWORD *)this + 10) + 8LL) + 48LL, **((_QWORD **)this + 10) + 48LL);
  apac::LPCodeBook::VQSearch(*(apac__LPCodeBook **)(*((_QWORD *)this + 10) + 8LL), (const float *)v12);
  v[1] = **(_DWORD **)(*(_QWORD *)(*((_QWORD *)this + 10) + 8LL) + 16LL);
  vDSP_vadd(
    v10,
    1,
    *(_QWORD *)(*(_QWORD *)(*((_QWORD *)this + 10) + 8LL) + 8LL)
  + 4LL
  * *(unsigned __int16 *)(*(_QWORD *)(*((_QWORD *)this + 10) + 8LL) + 2LL)
  * **(_DWORD **)(*(_QWORD *)(*((_QWORD *)this + 10) + 8LL) + 16LL),
    1,
    *((_QWORD *)this + 24),
    1,
    *((unsigned __int16 *)this + 52));
  return v12[9] ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000b5a10  size 1092 bytes  272 insns
 *   apac::LSFTools::Initialize()
 *   [_ZN4apac8LSFTools10InitializeEv]
 *   data refs: 0x425ad8, 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::LSFTools::Initialize(apac__LSFTools *this)
{
  int v2; // w2
  __int64 v3; // x3
  __int64 v4; // x4
  unsigned __int64 v5; // x1
  unsigned __int64 v6; // x0
  __int64 v7; // x3
  __int64 v8; // x3
  __int64 v9; // x4
  unsigned __int64 v10; // x1
  __int64 v11; // x0
  __int64 v12; // x3
  __int64 v13; // x4
  unsigned __int64 v14; // x0
  unsigned __int64 v15; // x1
  bool v16; // cf
  __int64 v17; // x2
  __int64 v18; // x2
  __int64 v19; // x3
  unsigned __int64 v20; // x1
  bool v21; // cc
  __int64 v22; // x2
  __int64 v23; // x2
  __int64 v24; // x3
  unsigned __int64 v25; // x1
  bool v26; // cc
  __int64 v27; // x2
  __int64 v28; // x2
  __int64 v29; // x3
  unsigned __int64 v30; // x1
  bool v31; // cc
  __int64 v32; // x2
  __int64 v33; // x2
  __int64 v34; // x3
  unsigned __int64 v35; // x1
  bool v36; // cc
  _QWORD *v37; // x20
  __int64 v38; // x22
  int i; // w24
  _QWORD *v40; // x0
  __int16 v41; // w4
  _QWORD *v42; // x1
  _QWORD *v43; // x3
  _QWORD *v44; // x27
  void *v45; // x0
  void *v46; // x0
  __int64 v47; // x27
  float *v48; // x0
  float *v49; // x2
  unsigned __int64 v50; // x4
  unsigned __int64 v51; // x1
  float *v52; // x3
  __int64 v53; // x3
  __int64 v54; // x4
  unsigned __int64 v55; // x1
  unsigned __int64 v56; // x0
  __int64 v57; // x2
  __int64 v58; // x1
  __int64 v60; // x0
  void *v61[2]; // [xsp+60h] [xbp+60h] BYREF
  __int64 v62; // [xsp+70h] [xbp+70h]
  v2 = *((unsigned __int16 *)this + 52);
  v3 = *((_QWORD *)this + 3);
  v4 = *((_QWORD *)this + 4);
  v5 = (v4 - v3) >> 2;
  v6 = *((unsigned __int16 *)this + 6) + v2 * *((unsigned __int16 *)this + 6);
  if ( v6 > v5 )
  {
    std::vector<float>::_M_default_append((char *)this + 24, v6 - v5);
    v2 = *((unsigned __int16 *)this + 52);
    v6 = *((unsigned __int16 *)this + 6) + v2 * *((unsigned __int16 *)this + 6);
  }
  else if ( v6 < v5 )
  {
    v7 = v3 + 4 * v6;
    if ( v4 != v7 )
      *((_QWORD *)this + 4) = v7;
  }
  v9 = *((_QWORD *)this + 6);
  v8 = *((_QWORD *)this + 7);
  v10 = (v8 - v9) >> 2;
  if ( v6 > v10 )
  {
    std::vector<float>::_M_default_append((char *)this + 48, v6 - v10);
    v13 = *((_QWORD *)this + 15);
    v12 = *((_QWORD *)this + 16);
    LOWORD(v2) = *((_WORD *)this + 52);
    v14 = (unsigned __int16)v2;
    v15 = (v12 - v13) >> 2;
    v16 = (unsigned __int16)v2 >= v15;
    if ( (unsigned __int16)v2 <= v15 )
      goto LABEL_10;
LABEL_58:
    std::vector<float>::_M_default_append((char *)this + 120, v14 - v15);
    v18 = *((_QWORD *)this + 18);
    v19 = *((_QWORD *)this + 19);
    v14 = *((unsigned __int16 *)this + 52);
    v20 = (v19 - v18) >> 2;
    v21 = v20 > v14;
    if ( v20 >= v14 )
      goto LABEL_14;
LABEL_59:
    std::vector<float>::_M_default_append((char *)this + 144, v14 - v20);
    v23 = *((_QWORD *)this + 21);
    v24 = *((_QWORD *)this + 22);
    v14 = *((unsigned __int16 *)this + 52);
    v25 = (v24 - v23) >> 2;
    v26 = v25 > v14;
    if ( v25 >= v14 )
      goto LABEL_19;
LABEL_60:
    std::vector<float>::_M_default_append((char *)this + 168, v14 - v25);
    v28 = *((_QWORD *)this + 24);
    v29 = *((_QWORD *)this + 25);
    v14 = *((unsigned __int16 *)this + 52);
    v30 = (v29 - v28) >> 2;
    v31 = v30 > v14;
    if ( v30 >= v14 )
      goto LABEL_24;
    goto LABEL_61;
  }
  if ( v6 < v10 )
  {
    v11 = v9 + 4 * v6;
    if ( v8 != v11 )
      *((_QWORD *)this + 7) = v11;
  }
  v13 = *((_QWORD *)this + 15);
  v12 = *((_QWORD *)this + 16);
  v14 = (unsigned __int16)v2;
  v15 = (v12 - v13) >> 2;
  v16 = (unsigned __int16)v2 >= v15;
  if ( (unsigned __int16)v2 > v15 )
    goto LABEL_58;
LABEL_10:
  if ( !v16 )
  {
    v17 = v13 + 4LL * (unsigned __int16)v2;
    if ( v12 != v17 )
      *((_QWORD *)this + 16) = v17;
  }
  v18 = *((_QWORD *)this + 18);
  v19 = *((_QWORD *)this + 19);
  v20 = (v19 - v18) >> 2;
  v21 = v20 > v14;
  if ( v20 < v14 )
    goto LABEL_59;
LABEL_14:
  if ( v21 )
  {
    v22 = v18 + 4 * v14;
    if ( v19 != v22 )
      *((_QWORD *)this + 19) = v22;
  }
  v23 = *((_QWORD *)this + 21);
  v24 = *((_QWORD *)this + 22);
  v25 = (v24 - v23) >> 2;
  v26 = v25 > v14;
  if ( v25 < v14 )
    goto LABEL_60;
LABEL_19:
  if ( v26 )
  {
    v27 = v23 + 4 * v14;
    if ( v24 != v27 )
      *((_QWORD *)this + 22) = v27;
  }
  v28 = *((_QWORD *)this + 24);
  v29 = *((_QWORD *)this + 25);
  v30 = (v29 - v28) >> 2;
  v31 = v30 > v14;
  if ( v30 >= v14 )
  {
LABEL_24:
    if ( v31 )
    {
      v32 = v28 + 4 * v14;
      if ( v29 != v32 )
        *((_QWORD *)this + 25) = v32;
    }
    v34 = *((_QWORD *)this + 27);
    v33 = *((_QWORD *)this + 28);
    v35 = (v33 - v34) >> 2;
    v36 = v35 > v14;
    if ( v35 >= v14 )
      goto LABEL_29;
LABEL_62:
    std::vector<float>::_M_default_append((char *)this + 216, v14 - v35);
    goto LABEL_30;
  }
LABEL_61:
  std::vector<float>::_M_default_append((char *)this + 192, v14 - v30);
  v34 = *((_QWORD *)this + 27);
  v33 = *((_QWORD *)this + 28);
  v14 = *((unsigned __int16 *)this + 52);
  v35 = (v33 - v34) >> 2;
  v36 = v35 > v14;
  if ( v35 < v14 )
    goto LABEL_62;
LABEL_29:
  if ( v36 )
  {
    v60 = v34 + 4 * v14;
    if ( v33 != v60 )
      *((_QWORD *)this + 28) = v60;
  }
LABEL_30:
  v37 = &unk_425AD8;
  v38 = 0;
  for ( i = 0; ; i = 1 )
  {
    v40 = (_QWORD *)operator new(0x48u);
    v40[1] = v37[1];
    v40[2] = 0;
    v41 = *((_WORD *)this + 52);
    v42 = *((_QWORD **)this + 11);
    *(_WORD *)v40 = *(_WORD *)v37;
    v43 = *((_QWORD **)this + 12);
    *((_WORD *)v40 + 1) = v41;
    *((_DWORD *)v40 + 1) = 1;
    v40[3] = 0;
    v40[4] = 0;
    v40[6] = 0;
    v40[7] = 0;
    v40[8] = 0;
    v61[0] = v40;
    if ( v42 == v43 )
    {
      std::vector<std::unique_ptr<apac::LPCodeBook>>::_M_realloc_insert<std::unique_ptr<apac::LPCodeBook>>(
        (std_vector *)((char *)this + 80),
        v42,
        v61);
    }
    else
    {
      v61[0] = nullptr;
      *v42 = v40;
      *((_QWORD *)this + 11) += 8LL;
    }
    v44 = v61[0];
    if ( v61[0] )
    {
      v45 = *((void **)v61[0] + 6);
      if ( v45 )
        operator delete(v45);
      v46 = (void *)v44[2];
      if ( v46 )
        operator delete(v46);
      operator delete(v44);
    }
    v47 = *(_QWORD *)(*((_QWORD *)this + 10) + v38);
    v48 = *(float **)(v47 + 48);
    v49 = *(float **)(v47 + 56);
    v50 = *(unsigned __int16 *)(v47 + 2);
    v51 = v49 - v48;
    if ( v50 > v51 )
    {
      std::vector<float>::_M_default_append(v47 + 48, v50 - v51);
      v48 = *(float **)(v47 + 48);
      v49 = *(float **)(v47 + 56);
    }
    else if ( v50 < v51 )
    {
      v52 = &v48[*(unsigned __int16 *)(v47 + 2)];
      if ( v49 != v52 )
      {
        v49 = &v48[*(unsigned __int16 *)(v47 + 2)];
        *(_QWORD *)(v47 + 56) = v52;
      }
    }
    for ( ; v48 != v49; ++v48 )
      *v48 = 1.0;
    v54 = *(_QWORD *)(v47 + 16);
    v53 = *(_QWORD *)(v47 + 24);
    v55 = *(unsigned __int16 *)(v47 + 4);
    v56 = (v53 - v54) >> 2;
    if ( v55 > v56 )
    {
      std::vector<int>::_M_default_append(v47 + 16, v55 - v56);
    }
    else if ( v55 < v56 )
    {
      v57 = v54 + 4LL * *(unsigned __int16 *)(v47 + 4);
      if ( v53 != v57 )
        *(_QWORD *)(v47 + 24) = v57;
    }
    v37 += 2;
    v38 += 8;
    if ( i )
      break;
  }
  v58 = *(_QWORD *)this;
  *((_WORD *)this + 53) = 2;
  (*(void (__fastcall **)(apac__LSFTools *))(v58 + 24))(this);
  return (__int64)v61[1] ^ _stack_chk_guard;
}


