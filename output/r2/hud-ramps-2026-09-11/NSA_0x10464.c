int __fastcall sub_10464(unsigned int a1, unsigned int a2, int a3)
{
  int v3; // r3
  bool v4; // cc
  unsigned int v5; // r7
  unsigned int v6; // r5
  unsigned int i; // lr
  int v8; // r10
  unsigned __int8 v10; // [sp+2h] [bp-36h]
  unsigned int v11; // [sp+8h] [bp-30h]

  v3 = (unsigned __int8)byte_28940;
  v4 = a1 > (unsigned int)&unk_28740;
  if ( a1 <= (unsigned int)&unk_28740 )
    v4 = (unsigned int)&unk_28540 > a1;
  if ( v4 )
  {
    v10 = 1;
  }
  else
  {
    v10 = 0;
    *(_BYTE *)(a3 + 19) = 0;
    v5 = 0;
    v6 = *(_DWORD *)(a3 + 12);
    for ( i = 0; i < a2; ++i )
    {
      v8 = *(unsigned __int8 *)(a1 + i);
      v11 = v3 - 1;
      if ( v3 )
      {
        if ( v3 == 1 )
        {
          if ( v8 == 2 )
          {
            v6 = 0;
            v3 = 2;
          }
          else
          {
            v3 = 0;
          }
        }
        else if ( v11 < 2 )
        {
          if ( v8 == 16 )
          {
            v3 = 3;
          }
          else if ( v6 < 0x100 )
          {
            byte_28968[256 * (unsigned __int8)byte_28941 + v6++] = v8;
            v3 = 2;
          }
          else
          {
            v6 = 0;
            v3 = 0;
          }
        }
        else if ( v11 > 2 )
        {
          v3 = v8 == 16;
        }
        else if ( v8 == 3 )
        {
          v3 = 0;
          *(_DWORD *)(a3 + 4 * v5) = v6;
          *(_BYTE *)(a3 + 16 + v5) = byte_28941;
          v5 = (unsigned __int8)(v5 + 1);
          *(_BYTE *)(a3 + 19) = v5;
          if ( v5 >= 3 )
            v5 = 0;
          v6 = 0;
          if ( (unsigned __int8)++byte_28941 >= 3u )
            byte_28941 = 0;
        }
        else if ( v8 == 16 )
        {
          if ( v6 < 0x100 )
          {
            byte_28968[256 * (unsigned __int8)byte_28941 + v6++] = v8;
            v3 = 2;
          }
          else
          {
            v6 = 0;
            v3 = 0;
          }
        }
        else
        {
          v6 = 0;
          v3 = 0;
        }
      }
      else
      {
        v3 = v8 == 16;
      }
    }
    *(_DWORD *)(a3 + 12) = v6;
  }
  byte_28940 = v3;
  return v10;
}
