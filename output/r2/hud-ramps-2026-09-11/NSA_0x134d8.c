int sub_134D8()
{
  unsigned int v0; // r10
  int v1; // r7
  int v2; // r0
  int result; // r0
  int v4; // r0
  bool v5; // cf
  unsigned __int8 v6; // [sp+3h] [bp-25h] BYREF
  int v7; // [sp+4h] [bp-24h] BYREF

  v0 = sub_1074C(&v7);
  if ( v0 > 1 )
    v0 = 1;
  if ( v0 )
  {
    v1 = v7;
    if ( !v7 )
      goto LABEL_42;
    sub_138D0(v7, &v6);
    if ( (unsigned __int8)byte_293C3 == 227 && v6 != 227 )
    {
      v2 = sub_147DC(4);
      sub_14930(v2);
    }
    sub_14188(v1);
    if ( v6 < 0xE2u )
    {
      if ( v6 == 225 )
      {
LABEL_19:
        sub_138FC(v1, v6);
        byte_293B7 = 2;
        dword_29418 = v1;
        byte_293C4 = 0;
        dword_29424 = 0;
        dword_2941C = 0;
        sub_21D0C();
        byte_293C3 = v6;
        return v6;
      }
      if ( v6 == 224 )
      {
        byte_293B7 = 1;
        dword_29418 = v1;
        byte_293C4 = 0;
        dword_29424 = 0;
        dword_2941C = 0;
        sub_21D0C();
        byte_293C3 = v6;
        return v6;
      }
      if ( (unsigned int)v6 - 12 >= 0xFFFFFFFE || (unsigned int)v6 - 5 >= 0xFFFFFFFD )
        goto LABEL_19;
LABEL_37:
      sub_138FC(v1, v6);
      byte_293B7 = 2;
      dword_29418 = 0;
      byte_293C4 = 0;
      dword_29424 = 0;
      dword_2941C = 0;
      sub_21D0C();
      byte_293C3 = v6;
      return v6;
    }
    if ( v6 != 226 )
    {
      if ( v6 != 227 )
      {
        if ( (unsigned int)v6 - 240 <= 1 || v6 == 243 )
        {
          byte_293C4 = v6;
          dword_29424 = v1;
          byte_293B7 = 0;
          dword_29418 = v1;
          dword_2941C = 0;
          sub_21D0C();
          byte_293C3 = v6;
          return v6;
        }
        goto LABEL_37;
      }
      if ( (unsigned __int8)byte_293C3 != 227 )
        sub_15248(&byte_293BD);
      if ( byte_293BD )
      {
        sub_145F4(v1, v6);
        if ( (unsigned __int8)byte_293C3 != 227 )
          sub_147DC(1);
        v4 = (unsigned __int8)byte_293C0;
        v5 = (unsigned __int8)byte_293C0 >= (unsigned int)(unsigned __int8)byte_293BD;
        if ( (unsigned __int8)byte_293C0 >= (unsigned int)(unsigned __int8)byte_293BD )
        {
          v4 = (unsigned __int8)byte_293BE;
          v5 = (unsigned __int8)byte_293BE >= (unsigned int)(unsigned __int8)byte_293C2;
        }
        if ( v5 )
          sub_1495C(v4);
        byte_293B7 = 2;
        dword_29418 = v1;
        byte_293C4 = 0;
        dword_29424 = 0;
        dword_2941C = 0;
        sub_21D0C();
        byte_293C3 = v6;
        return v6;
      }
LABEL_42:
      byte_293C4 = 0;
      dword_29424 = 0;
      byte_293B7 = 0;
      dword_29418 = 0;
      dword_2941C = 0;
      return 0;
    }
    if ( (unsigned __int8)byte_293C3 != 226 )
      sub_21CD4();
    byte_293C4 = 0;
    dword_2941C = v1;
    byte_293B7 = 0;
    dword_29418 = 0;
    dword_29424 = 0;
    byte_293C3 = v6;
    return v6;
  }
  else
  {
    if ( byte_293BC != 1 )
      goto LABEL_42;
    result = sub_14890();
    if ( !result )
      return sub_1477C();
  }
  return result;
}
