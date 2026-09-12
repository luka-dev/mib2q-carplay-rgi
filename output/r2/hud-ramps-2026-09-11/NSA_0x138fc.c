int __fastcall sub_138FC(int a1, unsigned __int8 a2)
{
  int v2; // r7
  int v3; // r10
  char v6; // [sp+2h] [bp-2Eh] BYREF
  unsigned __int8 v7; // [sp+3h] [bp-2Dh]
  _BYTE v8[4]; // [sp+4h] [bp-2Ch] BYREF
  int v9; // [sp+8h] [bp-28h]

  v7 = a2;
  v9 = a1;
  if ( !a1 )
    return 1;
  if ( v7 >= 2u )
  {
    if ( v7 == 2 )
    {
      v2 = 26;
      v3 = 1;
      goto LABEL_16;
    }
    if ( (unsigned int)v7 - 2 < 2 )
    {
      v2 = 108;
      v3 = 1;
      goto LABEL_16;
    }
    switch ( v7 )
    {
      case 4u:
        v2 = 1;
        v3 = 0;
        goto LABEL_16;
      case 0xAu:
        v2 = 18;
        v3 = 1;
        goto LABEL_16;
      case 0xBu:
        v2 = 18;
        v3 = 1;
        goto LABEL_16;
      case 0xE1u:
        v2 = 6;
        v3 = 0;
        goto LABEL_16;
    }
  }
  v2 = 0;
  v3 = 0;
LABEL_16:
  if ( v2 )
  {
    *(_BYTE *)dword_2942C = 1;
    *(_WORD *)(dword_2942C + 10) = v2 + 14;
    MEMORY[0x1782D3C](dword_2942C + 5, v9 + 3, 1);
    *(_BYTE *)(dword_2942C + 12) = v7;
    MEMORY[0x1782D3C](v8, v9 + 5, 4);
    *(_BYTE *)(dword_2942C + 13) = v8[0];
    *(_BYTE *)(dword_2942C + 14) = v8[1];
    *(_BYTE *)(dword_2942C + 15) = v8[2];
    *(_BYTE *)(dword_2942C + 16) = v8[3];
    if ( v3 == 1 )
    {
      MEMORY[0x1782D3C](&v6, v9 + 9, 1);
      v6 &= 1u;
      *(_BYTE *)(dword_2942C + 17) = v6;
    }
    else
    {
      *(_BYTE *)(dword_2942C + 17) = 0;
    }
    MEMORY[0x1782D3C](dword_2942C + 6, v9 + 11, 2);
    MEMORY[0x1782D3C](dword_2942C + 8, v9 + 13, 2);
    MEMORY[0x1782D3C](dword_2942C + 18, v9 + 15, v2);
    if ( v3 == 1 )
    {
      *(_BYTE *)(dword_2942C + 18 + v2) = v6;
      ++*(_WORD *)(dword_2942C + 10);
    }
    return 0;
  }
  else
  {
    *(_BYTE *)dword_2942C = 1;
    *(_WORD *)(dword_2942C + 10) = 14;
    *(_BYTE *)(dword_2942C + 12) = v7;
    return 0;
  }
}
