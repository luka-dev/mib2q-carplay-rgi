int __fastcall sub_15B64(int result)
{
  _BYTE *v1; // r10
  int v2; // r0
  int v3; // r6
  int v4; // r9
  int v5; // r0
  int v6; // r6
  int v7; // r9
  int v8; // r0
  int v9; // r6
  int v10; // r9
  int v11; // r0
  int v12; // r6
  int v13; // r9
  int v14; // r0
  int v15; // r6
  int v16; // r0
  int v17; // r6
  int v18; // r0
  int v19; // r1
  bool v20; // zf

  v1 = (_BYTE *)result;
  if ( result && (unsigned int)*(unsigned __int8 *)result - 1 <= 2 )
  {
    v2 = sub_1665C(4);
    v3 = v2;
    if ( v2 != 255 )
    {
      *(_WORD *)(1034 * v2 + 0x9BC7C) = 110 * (*(unsigned __int8 *)(12 * v2 + 0x9BB5E) - 1)
                                      + *(_WORD *)(12 * v2 + 0x9BB5C);
      MEMORY[0x1782D3C](1034 * MEMORY[0x9BAE1] + 665236, 1034 * v2 + 638068, 1034);
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF8) = 4;
      v4 = 4 * MEMORY[0x9BAE1] + 637664;
      *(_BYTE *)(v4 + 25) = sub_164D8(4, v3);
      *(_DWORD *)(12 * v3 + 0x9BB54) = 0;
      *(_WORD *)(12 * v3 + 0x9BB5C) = 0;
      ((void (*)(void))sub_16890)();
    }
    v5 = sub_1665C(0);
    v6 = v5;
    if ( v5 != 255 )
    {
      *(_WORD *)(1034 * v5 + 0x9BC7C) = 110 * (*(unsigned __int8 *)(12 * v5 + 0x9BB5E) - 1)
                                      + *(_WORD *)(12 * v5 + 0x9BB5C);
      MEMORY[0x1782D3C](1034 * MEMORY[0x9BAE1] + 665236, 1034 * v5 + 638068, 1034);
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF8) = 0;
      v7 = 4 * MEMORY[0x9BAE1] + 637664;
      *(_BYTE *)(v7 + 25) = sub_164D8(0, v6);
      *(_DWORD *)(12 * v6 + 0x9BB54) = 0;
      *(_WORD *)(12 * v6 + 0x9BB5C) = 0;
      ((void (*)(void))sub_16890)();
    }
    v8 = sub_1665C(5);
    v9 = v8;
    if ( v8 != 255 )
    {
      *(_WORD *)(1034 * v8 + 0x9A2AC) = 110 * (*(unsigned __int8 *)(12 * v8 + 0x9BB16) - 1)
                                      + *(_WORD *)(12 * v8 + 0x9BB14);
      MEMORY[0x1782D3C](1034 * MEMORY[0x9BAE1] + 665236, 1034 * v8 + 631460, 1034);
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF8) = 5;
      v10 = 4 * MEMORY[0x9BAE1] + 637664;
      *(_BYTE *)(v10 + 25) = sub_164D8(5, v9);
      *(_DWORD *)(12 * v9 + 0x9BB0C) = 0;
      *(_WORD *)(12 * v9 + 0x9BB14) = 0;
      ((void (*)(void))sub_16890)();
    }
    v11 = sub_1665C(1);
    v12 = v11;
    if ( v11 != 255 )
    {
      *(_WORD *)(1034 * v11 + 0x9A2AC) = 110 * (*(unsigned __int8 *)(12 * v11 + 0x9BB16) - 1)
                                       + *(_WORD *)(12 * v11 + 0x9BB14);
      MEMORY[0x1782D3C](1034 * MEMORY[0x9BAE1] + 665236, 1034 * v11 + 631460, 1034);
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF8) = 1;
      v13 = 4 * MEMORY[0x9BAE1] + 637664;
      *(_BYTE *)(v13 + 25) = sub_164D8(1, v12);
      *(_DWORD *)(12 * v12 + 0x9BB0C) = 0;
      *(_WORD *)(12 * v12 + 0x9BB14) = 0;
      ((void (*)(void))sub_16890)();
    }
    v14 = sub_1665C(7);
    v15 = v14;
    if ( v14 != 255 )
    {
      *(_WORD *)(1034 * v14 + 0xA1E70) = 110 * (*(unsigned __int8 *)(12 * v14 + 0xA2686) - 1)
                                       + *(_WORD *)(12 * v14 + 0xA2684);
      v16 = MEMORY[0x1782D3C](1034 * MEMORY[0x9BAE1] + 665236, 1034 * v14 + 663144, 1034);
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF8) = 7;
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF9) = -1;
      *(_DWORD *)(12 * v15 + 0xA267C) = 0;
      *(_WORD *)(12 * v15 + 0xA2684) = 0;
      sub_16890(v16);
    }
    result = sub_1665C(3);
    v17 = result;
    if ( result != 255 )
    {
      *(_WORD *)(1034 * result + 0xA1E70) = 110 * (*(unsigned __int8 *)(12 * result + 0xA2686) - 1)
                                          + *(_WORD *)(12 * result + 0xA2684);
      v18 = MEMORY[0x1782D3C](1034 * MEMORY[0x9BAE1] + 665236, 1034 * result + 663144, 1034);
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF8) = 3;
      *(_BYTE *)(4 * MEMORY[0x9BAE1] + 0x9BAF9) = -1;
      *(_DWORD *)(12 * v17 + 0xA267C) = 0;
      *(_WORD *)(12 * v17 + 0xA2684) = 0;
      result = sub_16890(v18);
    }
  }
  if ( v1 )
  {
    v19 = (unsigned __int8)*v1;
    v20 = v19 == 1;
    if ( v19 != 1 )
      v20 = v19 == 3;
    if ( v20 && MEMORY[0x9BAE0] != MEMORY[0x9BAE1] )
    {
      if ( !(unsigned __int8)sub_229E8(
                               1034 * MEMORY[0x9BAE0] + 665236,
                               (unsigned __int16)(*(_WORD *)(1034 * MEMORY[0x9BAE0] + 0xA269C) + 10)) )
      {
        MEMORY[0x9BAF0] = *(_BYTE *)(4 * MEMORY[0x9BAE0] + 0x9BAF8);
        MEMORY[0x9BAF1] = *(_BYTE *)(4 * MEMORY[0x9BAE0] + 0x9BAF9);
        *v1 = 2;
      }
      return sub_168CC();
    }
  }
  return result;
}
