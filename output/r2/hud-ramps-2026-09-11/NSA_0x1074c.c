unsigned int __fastcall sub_1074C(int a1)
{
  unsigned int v1; // r1
  unsigned int i; // r2

  if ( !a1 )
    return 0;
  v1 = (unsigned __int8)byte_2894C;
  if ( (unsigned __int8)byte_2894C > 1u )
    v1 = 1;
  for ( i = 0; i < v1; i = (unsigned __int8)(i + 1) )
    *(_DWORD *)(a1 + 4 * i) = dword_28948[i];
  return v1;
}
