void sub_10E78()
{
  char v0; // r7
  unsigned __int8 v1; // r0
  int v2; // r6
  int v3; // r6
  int v4; // [sp+4h] [bp-28h] BYREF
  int v5; // [sp+8h] [bp-24h] BYREF

  v0 = byte_28950;
  if ( byte_28950 != 1 )
  {
    v0 = 2;
    if ( byte_28940 == 2 && byte_28967 )
    {
      byte_28964 = *((_BYTE *)&dword_28520[273] + (unsigned __int8)byte_28967);
      dword_28954 = dword_28520[(unsigned __int8)byte_28967 + 269];
    }
    byte_28967 = 0;
    byte_28946 = 0;
    byte_28C71 = 0;
    byte_2894C = 0;
    byte_28C6C = 0;
    v5 = 0;
    v1 = MEMORY[0x1781C88](dword_28520[0], 1, &v5, 4);
    v2 = v1;
    if ( v1 )
      v4 = 0;
    else
      v2 = sub_108F8(v5, (char *)&dword_28520[264] + 2, &v4);
    if ( v4 )
    {
      if ( !v2 && !sub_10464((char *)&unk_28540 + 512 * (unsigned __int8)byte_28942, v4, &dword_28520[269]) )
      {
        v3 = sub_10D58(&dword_28520[269]);
        if ( byte_28946 )
        {
          if ( !v3 )
            v0 = 3;
        }
      }
    }
  }
  byte_28950 = v0;
}
