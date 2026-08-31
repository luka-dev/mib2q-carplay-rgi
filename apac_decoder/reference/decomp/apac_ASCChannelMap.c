/* ============================================================================
 * apac::ASCChannelMap
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 2 function(s), 560 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000af5b0    448  apac::ASCChannelMap::InsertMap(std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&) */
/* 0x000af770    112  apac::ASCChannelMap::AddMap(apac::AudioDataType, unsigned short, unsigned short) */

/* ------------------------------------------------------------------------ */
/* 0x000af5b0  size 448 bytes  112 insns
 *   apac::ASCChannelMap::InsertMap(std::tuple<apac::AudioDataType, apac::ASCChannelRange> const&)
 *   [_ZN4apac13ASCChannelMap9InsertMapERKSt5tupleIJNS_13AudioDataTypeENS_15ASCChannelRangeEEE]
 *   data refs: 0x42ce00, 0x43e520
 */
char *__fastcall apac::ASCChannelMap::InsertMap(apac__ASCChannelMap *this, const void *a)
{
  unsigned __int16 *v3; // x0
  __int64 v4; // x8
  unsigned __int16 *v5; // x4
  __int64 i; // x3
  unsigned __int16 *v7; // x2
  unsigned int v8; // w2
  char *v10; // x20
  __int16 v11; // w2
  int v12; // w0
  __int16 v13; // w3
  int v14; // w5
  __int64 v15; // x0
  _DWORD *v16; // x1
  __int64 v17; // x2
  int v18; // w0
  __int64 v19; // [xsp+40h] [xbp+40h]
  __int64 v20; // [xsp+50h] [xbp+50h]
  v3 = *(unsigned __int16 **)this;
  v4 = *((_QWORD *)this + 1);
  v5 = v3;
  for ( i = (v4 - (__int64)v3) >> 3; i > 0; i >>= 1 )
  {
    while ( 1 )
    {
      v7 = &v5[4 * (i >> 1)];
      if ( *(unsigned __int16 *)a < (unsigned int)*v7 )
        break;
      v5 = v7 + 4;
      i = i - (i >> 1) - 1;
      if ( i <= 0 )
        goto LABEL_5;
    }
  }
LABEL_5:
  if ( v5 == (unsigned __int16 *)v4 )
  {
    v10 = (char *)((char *)v5 - (char *)v3);
    if ( v5 != *((unsigned __int16 **)this + 2) )
    {
      *(_QWORD *)v5 = *(_QWORD *)a;
      *((_QWORD *)this + 1) += 8LL;
      return &v10[*(_QWORD *)this];
    }
  }
  else
  {
    v8 = *((unsigned __int16 *)a + 1);
    if ( *v5 < v8 && v5[1] > (unsigned int)*(unsigned __int16 *)a
      || (unsigned __int16 *)v4 != v5 + 4 && v5[4] < v8 && v5[5] > (unsigned int)*(unsigned __int16 *)a )
    {
      return *((char **)this + 1);
    }
    v10 = (char *)((char *)v5 - (char *)v3);
    if ( v4 != *((_QWORD *)this + 2) )
    {
      v11 = *(_WORD *)(v4 - 6);
      v12 = *(_DWORD *)(v4 - 4);
      v13 = *(_WORD *)(v4 - 8);
      v19 = *(_QWORD *)a;
      v14 = *((_DWORD *)a + 1);
      *(_WORD *)v4 = v13;
      *(_WORD *)(v4 + 2) = v11;
      *(_DWORD *)(v4 + 4) = v12;
      v15 = *((_QWORD *)this + 1);
      v16 = (_DWORD *)(v15 - 8);
      *((_QWORD *)this + 1) = v15 + 8;
      v17 = (v15 - 8 - (__int64)v5) >> 3;
      if ( v15 - 8 - (__int64)v5 > 0 )
      {
        while ( 1 )
        {
          v18 = *(v16 - 1);
          *v16 = *(v16 - 2);
          v16[1] = v18;
          if ( !--v17 )
            break;
          v16 -= 2;
        }
      }
      *(_DWORD *)v5 = v19;
      *((_DWORD *)v5 + 1) = v14;
      return &v10[*(_QWORD *)this];
    }
  }
  std::vector<std::tuple<apac::AudioDataType,apac::ASCChannelRange>>::_M_realloc_insert<std::tuple<apac::AudioDataType,apac::ASCChannelRange> const&>(
    (std_vector *)this,
    v5,
    a);
  return &v10[*(_QWORD *)this];
}


/* ------------------------------------------------------------------------ */
/* 0x000af770  size 112 bytes  28 insns
 *   apac::ASCChannelMap::AddMap(apac::AudioDataType, unsigned short, unsigned short)
 *   [_ZN4apac13ASCChannelMap6AddMapENS_13AudioDataTypeEtt]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall apac::ASCChannelMap::AddMap(apac__ASCChannelMap *this, int n, unsigned __int16 v, unsigned __int16 v2)
{
  _WORD v5[2]; // [xsp+20h] [xbp+20h] BYREF
  int v6; // [xsp+24h] [xbp+24h]
  __int64 v7; // [xsp+30h] [xbp+30h]
  v5[0] = v;
  v5[1] = v2;
  v6 = n;
  return *((_QWORD *)this + 1) != (_QWORD)apac::ASCChannelMap::InsertMap(this, v5);
}


