int __fastcall sub_1EC84(int result)
{
  int v1; // r5
  int v2; // r1
  bool v3; // zf
  int v4; // r1
  int v5; // r1

  v1 = result;
  if ( result )
  {
    if ( *(_BYTE *)(MEMORY[0xA4DB0] + 5) == 2 )
    {
      *(_BYTE *)(MEMORY[0xA4DB0] + 5) = 1;
      result = *(unsigned __int16 *)(result + 2);
      if ( result == 256 )
      {
        *(_BYTE *)(v1 + 2) = MEMORY[0xA3ADC];
        *(_BYTE *)(v1 + 3) = MEMORY[0xA3ADD];
      }
      else
      {
        v2 = *(unsigned __int8 *)(v1 + 2);
        v3 = v2 == MEMORY[0xA3ADC];
        if ( v2 == MEMORY[0xA3ADC] )
          v3 = *(unsigned __int8 *)(v1 + 3) == MEMORY[0xA3ADD];
        if ( !v3 )
        {
          v4 = *(unsigned __int8 *)(v1 + 2);
          if ( !*(_BYTE *)(v1 + 2) )
            v4 = 2;
          if ( (unsigned int)(v4 - 1) <= 2 )
          {
            v5 = (unsigned __int8)(v4 - 1);
            MEMORY[0xA40D6] = *(_BYTE *)(2 * v5 + 0xA4DD2);
            MEMORY[0xA40D7] = *(_BYTE *)(2 * v5 + 0xA4DD3);
            MEMORY[0xA40BE] = *(_BYTE *)(2 * v5 + 0xA4DD8);
            MEMORY[0xA40BF] = *(_BYTE *)(2 * v5 + 0xA4DD9);
            MEMORY[0xA420E] = *(_BYTE *)(2 * v5 + 0xA4DDE);
            MEMORY[0xA420F] = *(_BYTE *)(2 * v5 + 0xA4DDF);
            MEMORY[0xA408E] = *(_BYTE *)(2 * v5 + 0xA4DE4);
            MEMORY[0xA408F] = *(_BYTE *)(2 * v5 + 0xA4DE5);
            MEMORY[0xA4076] = *(_BYTE *)(2 * v5 + 0xA4DEA);
            MEMORY[0xA4077] = *(_BYTE *)(2 * v5 + 0xA4DEB);
            MEMORY[0xA405E] = *(_BYTE *)(2 * v5 + 0xA4DF0);
            MEMORY[0xA405F] = *(_BYTE *)(2 * v5 + 0xA4DF1);
            MEMORY[0xA4046] = *(_BYTE *)(2 * v5 + 0xA4DF6);
            MEMORY[0xA4047] = *(_BYTE *)(2 * v5 + 0xA4DF7);
            MEMORY[0xA402E] = *(_BYTE *)(2 * v5 + 0xA4DFC);
            MEMORY[0xA402F] = *(_BYTE *)(2 * v5 + 0xA4DFD);
            MEMORY[0xA4016] = *(_BYTE *)(2 * v5 + 0xA4E02);
            MEMORY[0xA4017] = *(_BYTE *)(2 * v5 + 0xA4E03);
            MEMORY[0xA3FFE] = *(_BYTE *)(2 * v5 + 0xA4E08);
            MEMORY[0xA3FFF] = *(_BYTE *)(2 * v5 + 0xA4E09);
            MEMORY[0xA3FE6] = *(_BYTE *)(2 * v5 + 0xA4E0E);
            MEMORY[0xA3FE7] = *(_BYTE *)(2 * v5 + 0xA4E0F);
            MEMORY[0xA3FCE] = *(_BYTE *)(2 * v5 + 0xA4E14);
            MEMORY[0xA3FCF] = *(_BYTE *)(2 * v5 + 0xA4E15);
            MEMORY[0xA3FB6] = *(_BYTE *)(2 * v5 + 0xA4E1A);
            MEMORY[0xA3FB7] = *(_BYTE *)(2 * v5 + 0xA4E1B);
            MEMORY[0xA3F9E] = *(_BYTE *)(2 * v5 + 0xA4E20);
            MEMORY[0xA3F9F] = *(_BYTE *)(2 * v5 + 0xA4E21);
            MEMORY[0xA3F86] = *(_BYTE *)(2 * v5 + 0xA4E26);
            MEMORY[0xA3F87] = *(_BYTE *)(2 * v5 + 0xA4E27);
            MEMORY[0xA3F6E] = *(_BYTE *)(2 * v5 + 0xA4E2C);
            MEMORY[0xA3F6F] = *(_BYTE *)(2 * v5 + 0xA4E2D);
            MEMORY[0xA3F56] = *(_BYTE *)(2 * v5 + 0xA4E32);
            MEMORY[0xA3F57] = *(_BYTE *)(2 * v5 + 0xA4E33);
            MEMORY[0xA3F3E] = *(_BYTE *)(2 * v5 + 0xA4E38);
            MEMORY[0xA3F3F] = *(_BYTE *)(2 * v5 + 0xA4E39);
            MEMORY[0xA3F26] = *(_BYTE *)(2 * v5 + 0xA4E3E);
            MEMORY[0xA3F27] = *(_BYTE *)(2 * v5 + 0xA4E3F);
            MEMORY[0xA3F0E] = *(_BYTE *)(2 * v5 + 0xA4E44);
            MEMORY[0xA3F0F] = *(_BYTE *)(2 * v5 + 0xA4E45);
            MEMORY[0xA3EF6] = *(_BYTE *)(2 * v5 + 0xA4E4A);
            MEMORY[0xA3EF7] = *(_BYTE *)(2 * v5 + 0xA4E4B);
            MEMORY[0xA3EDE] = *(_BYTE *)(2 * v5 + 0xA4E50);
            MEMORY[0xA3EDF] = *(_BYTE *)(2 * v5 + 0xA4E51);
            MEMORY[0xA3EC6] = *(_BYTE *)(2 * v5 + 0xA4E56);
            MEMORY[0xA3EC7] = *(_BYTE *)(2 * v5 + 0xA4E57);
            MEMORY[0xA3EAE] = *(_BYTE *)(2 * v5 + 0xA4E5C);
            MEMORY[0xA3EAF] = *(_BYTE *)(2 * v5 + 0xA4E5D);
            MEMORY[0xA3E96] = *(_BYTE *)(2 * v5 + 0xA4E62);
            MEMORY[0xA3E97] = *(_BYTE *)(2 * v5 + 0xA4E63);
            MEMORY[0xA40A6] = *(_BYTE *)(2 * v5 + 0xA4E68);
            MEMORY[0xA40A7] = *(_BYTE *)(2 * v5 + 0xA4E69);
            MEMORY[0xA4106] = *(_BYTE *)(2 * v5 + 0xA4E6E);
            MEMORY[0xA4107] = *(_BYTE *)(2 * v5 + 0xA4E6F);
            MEMORY[0xA411E] = *(_BYTE *)(2 * v5 + 0xA4E74);
            MEMORY[0xA411F] = *(_BYTE *)(2 * v5 + 0xA4E75);
            MEMORY[0xA4136] = *(_BYTE *)(2 * v5 + 0xA4E7A);
            MEMORY[0xA4137] = *(_BYTE *)(2 * v5 + 0xA4E7B);
            MEMORY[0xA41DE] = *(_BYTE *)(2 * v5 + 0xA4E80);
            MEMORY[0xA41DF] = *(_BYTE *)(2 * v5 + 0xA4E81);
            MEMORY[0xA414E] = *(_BYTE *)(2 * v5 + 0xA4E86);
            MEMORY[0xA414F] = *(_BYTE *)(2 * v5 + 0xA4E87);
            MEMORY[0xA4166] = *(_BYTE *)(2 * v5 + 0xA4E8C);
            MEMORY[0xA4167] = *(_BYTE *)(2 * v5 + 0xA4E8D);
            MEMORY[0xA41C6] = *(_BYTE *)(2 * v5 + 0xA4E92);
            MEMORY[0xA41C7] = *(_BYTE *)(2 * v5 + 0xA4E93);
            MEMORY[0xA41F6] = *(_BYTE *)(2 * v5 + 0xA4E98);
            MEMORY[0xA41F7] = *(_BYTE *)(2 * v5 + 0xA4E99);
          }
        }
      }
    }
    else
    {
      *(_BYTE *)(MEMORY[0xA4DB0] + 5) = 2;
      MEMORY[0xA3ADC] = *(_BYTE *)(result + 2);
      result = *(unsigned __int8 *)(result + 3);
      MEMORY[0xA3ADD] = *(_BYTE *)(v1 + 3);
    }
  }
  return result;
}
