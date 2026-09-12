int __fastcall sub_109FC(int a1, unsigned __int8 a2, int a3, int a4, _BYTE *a5)
{
  bool v5; // zf
  char v6; // r10
  char v7; // r7
  int v8; // r5
  unsigned __int8 v10; // [sp+1h] [bp-3Bh]
  char v11; // [sp+2h] [bp-3Ah]

  v5 = a3 == 0;
  if ( a3 )
    v5 = a4 == 0;
  if ( !v5 && a5 )
  {
    v10 = 0;
    v11 = byte_28968[256 * a2 + 3];
    v6 = *(_BYTE *)(a4 + 4);
    v7 = a5[2];
    if ( (unsigned __int16)(((unsigned __int8)byte_28968[256 * a2 + 2] << 8) + (unsigned __int8)byte_28968[256 * a2 + 1]) == *(_DWORD *)(a3 + 4 * a1) - 5 )
      v8 = (unsigned __int8)sub_106B8(
                              &byte_28968[256 * a2 + 4],
                              (unsigned __int16)(((unsigned __int8)byte_28968[256 * a2 + 2] << 8)
                                               + (unsigned __int8)byte_28968[256 * a2 + 1]));
    else
      v8 = 1;
    if ( !v8 )
    {
      *(_DWORD *)a4 = &byte_28968[256 * a2];
      *(_BYTE *)(a4 + 4) = v6 + 1;
    }
    *a5 = v11;
    a5[1] = v8;
    a5[2] = v7 + 1;
  }
  else
  {
    return 1;
  }
  return v10;
}
