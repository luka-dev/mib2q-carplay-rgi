/* ============================================================================
 * APACSynthesisFilterBank
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 5 function(s), 1736 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000ae530    124  APACSynthesisFilterBank::Reset() */
/* 0x000ae5b0    964  APACSynthesisFilterBank::FrequencyToTimeInPlace(unsigned int, float*, unsigned int, unsigned int) */
/* 0x000ae980    408  APACSynthesisFilterBank::Initialize(unsigned int, unsigned int) */
/* 0x000aeb20    116  APACSynthesisFilterBank::~APACSynthesisFilterBank() */
/* 0x000aeba0    124  APACSynthesisFilterBank::~APACSynthesisFilterBank() */

/* ------------------------------------------------------------------------ */
/* 0x000ae530  size 124 bytes  31 insns
 *   APACSynthesisFilterBank::Reset()
 *   [_ZN23APACSynthesisFilterBank5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSynthesisFilterBank::Reset(APACSynthesisFilterBank *this)
{
  __int64 v1; // x2
  __int64 v2; // x3
  __int64 i; // x1
  _BYTE *v4; // x0
  __int64 v5; // x1
  _BYTE *v6; // x1
  __int64 v8; // [xsp+18h] [xbp+18h]
  __int64 v9; // [xsp+20h] [xbp+20h]
  v1 = *((_QWORD *)this + 1);
  v2 = *((_QWORD *)this + 2);
  if ( v1 )
  {
    for ( i = 0; i != v1; ++i )
      *(_DWORD *)(v2 + 4 * i) = 0;
  }
  v5 = *((_QWORD *)this + 3);
  v4 = *((_BYTE **)this + 4);
  if ( v5 )
  {
    v6 = &v4[v5];
    do
      *v4++ = 0;
    while ( v4 != v6 );
  }
  return v8 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000ae5b0  size 964 bytes  241 insns
 *   APACSynthesisFilterBank::FrequencyToTimeInPlace(unsigned int, float*, unsigned int, unsigned int)
 *   [_ZN23APACSynthesisFilterBank22FrequencyToTimeInPlaceEjPfjj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSynthesisFilterBank::FrequencyToTimeInPlace(
        APACSynthesisFilterBank *this,
        unsigned int n,
        float *buf,
        unsigned int n2,
        unsigned int n3)
{
  float *v5; // x23
  __int64 v6; // x24
  char *v7; // x25
  float *v8; // x26
  __int64 v9; // x27
  char *i; // x28
  __int64 v11; // x29
  __int64 v12; // x30
  __int64 *v13; // x9
  __int64 result; // x0
  bool v17; // cc
  int v19; // w24
  unsigned int v20; // w2
  __int64 v21; // x1
  unsigned int v22; // w24
  size_t v23; // x22
  __int64 v24; // x24
  size_t v25; // x22
  __int64 v26; // x2
  float *v27; // x24
  __int64 v28; // x0
  float *v29; // x22
  int v30; // w8
  __int64 v31; // x6
  unsigned int v32; // w24
  size_t v33; // x22
  __int64 v34; // [xsp+8h] [xbp-1298h]
  __int64 v35; // [xsp+10h] [xbp-1290h]
  __int64 v36; // [xsp+18h] [xbp-1288h]
  float *v37; // [xsp+40h] [xbp-1260h]
  __int64 v38; // [xsp+48h] [xbp-1258h]
  char *v39; // [xsp+50h] [xbp-1250h]
  float *v40; // [xsp+58h] [xbp-1248h]
  __int64 v41; // [xsp+60h] [xbp-1240h]
  char *v42; // [xsp+68h] [xbp-1238h]
  char *v43; // [xsp+70h] [xbp-1230h]
  int v44; // [xsp+7Ch] [xbp-1224h]
  MDCT *v45; // [xsp+80h] [xbp-1220h]
  unsigned int v46; // [xsp+88h] [xbp-1218h]
  unsigned int v47; // [xsp+8Ch] [xbp-1214h]
  float v48[1154]; // [xsp+90h] [xbp-1210h] BYREF
  __int64 v49; // [xsp+1298h] [xbp-8h]
  __int64 v50; // [xsp+12A0h] [xbp+0h] BYREF

  v13 = &v50 - 2560;
  v13[512] = 0;
  v13[428] = 0;
  v35 = v11;
  v36 = v12;
  v46 = n2;
  v17 = *((_QWORD *)this + 3) > (unsigned __int64)n;
  result = 1;
  v17 = !v17 || n3 > 3;
  if ( v17 )
    goto LABEL_6;
  v38 = v6;
  v5 = buf;
  v8 = (float *)(16LL * n2);
  i = (char *)this + 152;
  v19 = *((_DWORD *)this + 10);
  v20 = *((_DWORD *)this + 24);
  v43 = (char *)v8 + (_QWORD)this + 152;
  v21 = *((_QWORD *)this + 2);
  v47 = (v19 - v20) >> 1;
  v7 = (char *)(v21 + 4LL * n * v19);
  if ( n3 == 2 )
  {
    v43 = *(char **)&i[16 * n2];
    __memset_chk(v48, 0, 4LL * v20, 4608);
    v27 = &v48[v19];
    v28 = *((_QWORD *)this + 4);
    v8 = v48;
    v29 = v5;
    v30 = 8;
    v45 = (APACSynthesisFilterBank *)((char *)this + 96);
    for ( i = *(char **)&i[16 * *(unsigned __int8 *)(v28 + n)]; ; i = v43 )
    {
      v44 = v30;
      MDCT::InverseInPlaceTransform(v45, v29, v27);
      vDSP_vma(v29, 1, i, 1, v8, 1, v8, 1, *((unsigned int *)this + 24), v34, v35, v36);
      v31 = *((unsigned int *)this + 24);
      v8 += v31;
      v29 += v31;
      vDSP_vmul(v27, 1, &v43[4 * v31 - 4], -1, v8);
      v30 = v44 - 1;
      if ( v44 == 1 )
        break;
    }
    v32 = v47;
    v33 = 4LL * (v47 & 0x7FFFFFFF);
    memcpy(v5, v7, v33);
    vDSP_vadd(v48, 1, &v7[v33], 1, &v5[v33 / 4], 1, *((_DWORD *)this + 10) - v32);
    memcpy(v7, &v48[*((unsigned int *)this + 10) - (unsigned __int64)v32], 4LL * (*((_DWORD *)this + 10) - v32));
    memset(&v7[4 * (*((unsigned int *)this + 10) - (unsigned __int64)v32)], 0, v33);
    goto LABEL_12;
  }
  MDCT::InverseInPlaceTransform((APACSynthesisFilterBank *)((char *)this + 40), v5, v48);
  if ( n3 == 3 )
  {
    v24 = v47;
    v25 = v47 & 0x7FFFFFFF;
    i = *(char **)&i[16 * *(unsigned __int8 *)(*((_QWORD *)this + 4) + n)];
    memcpy(v5, v7, v25 * 4);
    vDSP_vma(&v5[v25], 1, i, 1, &v7[v25 * 4], 1, &v5[v25], 1, *((unsigned int *)this + 24), v34, v35, v36);
    v26 = *((unsigned int *)this + 24) + v24;
    vDSP_vadd(&v5[v26], 1, &v7[v26 * 4], 1, &v5[v26], 1, v24);
    goto LABEL_14;
  }
  vDSP_vma(
    v5,
    1,
    *(_QWORD *)&i[16 * *(unsigned __int8 *)(*((_QWORD *)this + 4) + n) + 8],
    1,
    v7,
    1,
    v5,
    1,
    *((unsigned int *)this + 10),
    v34,
    v35,
    v36);
  if ( n3 != 1 )
  {
LABEL_14:
    vDSP_vmul(v48, 1, *((_QWORD *)v43 + 1) + 4LL * *((unsigned int *)this + 10) - 4, -1, v7);
    goto LABEL_12;
  }
  v22 = v47;
  v5 = *(float **)((char *)v8 + (_QWORD)i);
  v23 = 4LL * (v47 & 0x7FFFFFFF);
  memcpy(v7, v48, v23);
  vDSP_vmul(&v48[v23 / 4], 1, &v5[*((unsigned int *)this + 24) - 1], -1, &v7[v23]);
  vDSP_vclr(&v7[4 * *((unsigned int *)this + 24) + 4 * (unsigned __int64)v22], 1);
LABEL_12:
  result = 0;
  *(_BYTE *)(*((_QWORD *)this + 4) + n) = v46;
  v6 = v38;
LABEL_6:
  if ( v49 != _stack_chk_guard )
  {
    v37 = v5;
    v38 = v6;
    v39 = v7;
    v40 = v8;
    v41 = v9;
    v42 = i;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000ae980  size 408 bytes  102 insns
 *   APACSynthesisFilterBank::Initialize(unsigned int, unsigned int)
 *   [_ZN23APACSynthesisFilterBank10InitializeEjj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACSynthesisFilterBank::Initialize(APACSynthesisFilterBank *this, unsigned int n, unsigned int n2)
{
  __int64 v3; // x23
  __int64 v4; // x24
  __int64 result; // x0
  _DWORD *v9; // x0
  _DWORD *i; // x1
  _BYTE *v11; // x0
  _BYTE *j; // x1
  unsigned int v13; // w22
  __int64 KBDWindowCoefficientPtr; // x0
  __int64 v15; // [xsp+30h] [xbp+30h]
  __int64 v16; // [xsp+38h] [xbp+38h]
  __int64 v17; // [xsp+48h] [xbp+48h]
  __int64 v18; // [xsp+50h] [xbp+50h]
  result = MDCT::Initialize((APACSynthesisFilterBank *)((char *)this + 40), n2, 0.0);
  if ( !(_DWORD)result )
  {
    v4 = n2 * n;
    v3 = v4;
    v9 = *((_DWORD **)this + 2);
    if ( v4 != *((_QWORD *)this + 1) )
    {
      operator delete(v9);
      *((_QWORD *)this + 1) = v4;
      v9 = (_DWORD *)operator new(4 * v4);
      *((_QWORD *)this + 2) = v9;
    }
    for ( i = &v9[v3]; i != v9; ++v9 )
      *v9 = 0;
    v11 = *((_BYTE **)this + 4);
    if ( *((_QWORD *)this + 3) != n )
    {
      operator delete(v11);
      *((_QWORD *)this + 3) = n;
      v11 = (_BYTE *)operator new(n);
      *((_QWORD *)this + 4) = v11;
    }
    for ( j = &v11[n]; j != v11; ++v11 )
      *v11 = 0;
    v13 = n2 >> 3;
    (*(void (__fastcall **)(APACSynthesisFilterBank *))(*(_QWORD *)this + 24LL))(this);
    result = MDCT::Initialize((APACSynthesisFilterBank *)((char *)this + 96), n2 >> 3, 0.0);
    if ( !(_DWORD)result )
    {
      *((_QWORD *)this + 20) = GetSineWindowCoefficientPtr(n2);
      *((_QWORD *)this + 19) = GetSineWindowCoefficientPtr(v13);
      *((_QWORD *)this + 22) = GetKBDWindowCoefficientPtr(n2);
      KBDWindowCoefficientPtr = GetKBDWindowCoefficientPtr(v13);
      *((_QWORD *)this + 21) = KBDWindowCoefficientPtr;
      result = !*((_QWORD *)this + 19)
            || !*((_QWORD *)this + 20)
            || !KBDWindowCoefficientPtr
            || *((_QWORD *)this + 22) == 0;
    }
  }
  if ( v17 != _stack_chk_guard )
  {
    v15 = v3 * 4;
    v16 = v4;
  }
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x000aeb20  size 116 bytes  29 insns
 *   APACSynthesisFilterBank::~APACSynthesisFilterBank()
 *   APACSynthesisFilterBank::~APACSynthesisFilterBank()
 *   [_ZN23APACSynthesisFilterBankD1Ev]
 *   [_ZN23APACSynthesisFilterBankD2Ev]
 *   data refs: 0x42b588, 0x42ca70, 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN23APACSynthesisFilterBankD2Ev'
void __fastcall APACSynthesisFilterBank::~APACSynthesisFilterBank(APACSynthesisFilterBank *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  *(_QWORD *)this = off_42B598;
  MDCT::~MDCT((APACSynthesisFilterBank *)((char *)this + 96));
  MDCT::~MDCT((APACSynthesisFilterBank *)((char *)this + 40));
  operator delete(*((void **)this + 4));
  operator delete(*((void **)this + 2));
}


/* ------------------------------------------------------------------------ */
/* 0x000aeba0  size 124 bytes  31 insns
 *   APACSynthesisFilterBank::~APACSynthesisFilterBank()
 *   [_ZN23APACSynthesisFilterBankD0Ev]
 *   data refs: 0x42b588, 0x42ca70, 0x42ce00, 0x43e520
 */
void __fastcall APACSynthesisFilterBank::~APACSynthesisFilterBank(APACSynthesisFilterBank *this)
{
  __int64 v2; // [xsp+30h] [xbp+30h]
  *(_QWORD *)this = off_42B598;
  MDCT::~MDCT((APACSynthesisFilterBank *)((char *)this + 96));
  MDCT::~MDCT((APACSynthesisFilterBank *)((char *)this + 40));
  operator delete(*((void **)this + 4));
  operator delete(*((void **)this + 2));
  operator delete(this);
}


