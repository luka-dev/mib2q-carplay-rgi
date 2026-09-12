_BYTE *sub_13CD8()
{
  _BYTE *result; // r0

  result = (_BYTE *)sub_13D5C();
  if ( result == (_BYTE *)1 )
  {
    result = (_BYTE *)sub_2297C(dword_2942C + 4, *(unsigned __int16 *)(dword_2942C + 10));
    if ( !result )
    {
      byte_293B4 = 1;
      dword_29434 = 0;
      result = (_BYTE *)dword_29428;
      dword_29428 = dword_2942C;
      dword_2942C = (int)result;
      *result = 0;
      byte_293B5 = 0;
    }
  }
  return result;
}
