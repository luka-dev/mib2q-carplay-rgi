int __fastcall sub_1F364(int result)
{
  int v1; // r1
  int v2; // r1

  if ( result )
  {
    switch ( *(_BYTE *)result )
    {
      case 0xD:
        MEMORY[0xA41AE] = 0;
        MEMORY[0xA41AF] = 0;
        return sub_1FD24(1);
      case 0xE:
        MEMORY[0xA4F5F] = *(_BYTE *)(result + 2);
        break;
      case 0x3F:
        result = 675944;
        if ( MEMORY[0xA5068] == 1 )
        {
          MEMORY[0xA40D6] = 0x80;
          MEMORY[0xA40D7] = 0;
          MEMORY[0xA40BE] = 0x80;
          MEMORY[0xA40BF] = 0;
          MEMORY[0xA420E] = 50;
          MEMORY[0xA420F] = 0;
          MEMORY[0xA408E] = 0x80;
          MEMORY[0xA408F] = 0;
          MEMORY[0xA4076] = 0x80;
          MEMORY[0xA4077] = 0;
          MEMORY[0xA405E] = 50;
          MEMORY[0xA405F] = 0;
          MEMORY[0xA4046] = 0x80;
          MEMORY[0xA4047] = 0;
          MEMORY[0xA402E] = 0x80;
          MEMORY[0xA402F] = 0;
          MEMORY[0xA4016] = 50;
          MEMORY[0xA4017] = 0;
          MEMORY[0xA3FFE] = 0x80;
          MEMORY[0xA3FFF] = 0;
          MEMORY[0xA3FE6] = 50;
          MEMORY[0xA3FE7] = 0;
          MEMORY[0xA3FCE] = 0x80;
          MEMORY[0xA3FCF] = 0;
          MEMORY[0xA3FB6] = 0x80;
          MEMORY[0xA3FB7] = 0;
          MEMORY[0xA3F9E] = 0x80;
          MEMORY[0xA3F9F] = 0;
          MEMORY[0xA3F86] = 0x80;
          MEMORY[0xA3F87] = 0;
          MEMORY[0xA3F6E] = 0x80;
          MEMORY[0xA3F6F] = 0;
          MEMORY[0xA3F56] = 0x80;
          MEMORY[0xA3F57] = 0;
          MEMORY[0xA3F3E] = 0x80;
          MEMORY[0xA3F3F] = 0;
          MEMORY[0xA3F26] = 0x80;
          MEMORY[0xA3F27] = 0;
          MEMORY[0xA3F0E] = 0x80;
          MEMORY[0xA3F0F] = 0;
          MEMORY[0xA3EF6] = 0x80;
          MEMORY[0xA3EF7] = 0;
          MEMORY[0xA3EDE] = 0x80;
          MEMORY[0xA3EDF] = 0;
          MEMORY[0xA3EC6] = 0x80;
          MEMORY[0xA3EC7] = 0;
          MEMORY[0xA3EAE] = 0x80;
          MEMORY[0xA3EAF] = 0;
          MEMORY[0xA3E96] = 0x80;
          MEMORY[0xA3E97] = 0;
          MEMORY[0xA40A6] = 0x80;
          MEMORY[0xA40A7] = 0;
          MEMORY[0xA4106] = 0x80;
          MEMORY[0xA4107] = 0;
          MEMORY[0xA411E] = 0x80;
          MEMORY[0xA411F] = 0;
          MEMORY[0xA4136] = 0x80;
          MEMORY[0xA4137] = 0;
          MEMORY[0xA41DE] = 0x80;
          MEMORY[0xA41DF] = 0;
          MEMORY[0xA414E] = 0x80;
          MEMORY[0xA414F] = 0;
          MEMORY[0xA4166] = 50;
          MEMORY[0xA4167] = 0;
          MEMORY[0xA41C6] = 50;
          MEMORY[0xA41C7] = 0;
          MEMORY[0xA41F6] = 50;
          MEMORY[0xA41F7] = 0;
        }
        else
        {
          result = 675944;
          if ( MEMORY[0xA5068] == 2 )
            return sub_1FD24(1);
        }
        break;
      case 0x41:
        v1 = MEMORY[0xA40EE];
        if ( !MEMORY[0xA40EE] )
          v1 = 2;
        if ( (unsigned int)(v1 - 1) <= 2 )
        {
          v2 = (unsigned __int8)(v1 - 1);
          MEMORY[0xA40D6] = *(_BYTE *)(2 * v2 + 0xA4DD2);
          MEMORY[0xA40D7] = *(_BYTE *)(2 * v2 + 0xA4DD3);
          MEMORY[0xA40BE] = *(_BYTE *)(2 * v2 + 0xA4DD8);
          MEMORY[0xA40BF] = *(_BYTE *)(2 * v2 + 0xA4DD9);
          MEMORY[0xA420E] = *(_BYTE *)(2 * v2 + 0xA4DDE);
          MEMORY[0xA420F] = *(_BYTE *)(2 * v2 + 0xA4DDF);
          MEMORY[0xA408E] = *(_BYTE *)(2 * v2 + 0xA4DE4);
          MEMORY[0xA408F] = *(_BYTE *)(2 * v2 + 0xA4DE5);
          MEMORY[0xA4076] = *(_BYTE *)(2 * v2 + 0xA4DEA);
          MEMORY[0xA4077] = *(_BYTE *)(2 * v2 + 0xA4DEB);
          MEMORY[0xA405E] = *(_BYTE *)(2 * v2 + 0xA4DF0);
          MEMORY[0xA405F] = *(_BYTE *)(2 * v2 + 0xA4DF1);
          MEMORY[0xA4046] = *(_BYTE *)(2 * v2 + 0xA4DF6);
          MEMORY[0xA4047] = *(_BYTE *)(2 * v2 + 0xA4DF7);
          MEMORY[0xA402E] = *(_BYTE *)(2 * v2 + 0xA4DFC);
          MEMORY[0xA402F] = *(_BYTE *)(2 * v2 + 0xA4DFD);
          MEMORY[0xA4016] = *(_BYTE *)(2 * v2 + 0xA4E02);
          MEMORY[0xA4017] = *(_BYTE *)(2 * v2 + 0xA4E03);
          MEMORY[0xA3FFE] = *(_BYTE *)(2 * v2 + 0xA4E08);
          MEMORY[0xA3FFF] = *(_BYTE *)(2 * v2 + 0xA4E09);
          MEMORY[0xA3FE6] = *(_BYTE *)(2 * v2 + 0xA4E0E);
          MEMORY[0xA3FE7] = *(_BYTE *)(2 * v2 + 0xA4E0F);
          MEMORY[0xA3FCE] = *(_BYTE *)(2 * v2 + 0xA4E14);
          MEMORY[0xA3FCF] = *(_BYTE *)(2 * v2 + 0xA4E15);
          MEMORY[0xA3FB6] = *(_BYTE *)(2 * v2 + 0xA4E1A);
          MEMORY[0xA3FB7] = *(_BYTE *)(2 * v2 + 0xA4E1B);
          MEMORY[0xA3F9E] = *(_BYTE *)(2 * v2 + 0xA4E20);
          MEMORY[0xA3F9F] = *(_BYTE *)(2 * v2 + 0xA4E21);
          MEMORY[0xA3F86] = *(_BYTE *)(2 * v2 + 0xA4E26);
          MEMORY[0xA3F87] = *(_BYTE *)(2 * v2 + 0xA4E27);
          MEMORY[0xA3F6E] = *(_BYTE *)(2 * v2 + 0xA4E2C);
          MEMORY[0xA3F6F] = *(_BYTE *)(2 * v2 + 0xA4E2D);
          MEMORY[0xA3F56] = *(_BYTE *)(2 * v2 + 0xA4E32);
          MEMORY[0xA3F57] = *(_BYTE *)(2 * v2 + 0xA4E33);
          MEMORY[0xA3F3E] = *(_BYTE *)(2 * v2 + 0xA4E38);
          MEMORY[0xA3F3F] = *(_BYTE *)(2 * v2 + 0xA4E39);
          MEMORY[0xA3F26] = *(_BYTE *)(2 * v2 + 0xA4E3E);
          MEMORY[0xA3F27] = *(_BYTE *)(2 * v2 + 0xA4E3F);
          MEMORY[0xA3F0E] = *(_BYTE *)(2 * v2 + 0xA4E44);
          MEMORY[0xA3F0F] = *(_BYTE *)(2 * v2 + 0xA4E45);
          MEMORY[0xA3EF6] = *(_BYTE *)(2 * v2 + 0xA4E4A);
          MEMORY[0xA3EF7] = *(_BYTE *)(2 * v2 + 0xA4E4B);
          MEMORY[0xA3EDE] = *(_BYTE *)(2 * v2 + 0xA4E50);
          MEMORY[0xA3EDF] = *(_BYTE *)(2 * v2 + 0xA4E51);
          MEMORY[0xA3EC6] = *(_BYTE *)(2 * v2 + 0xA4E56);
          MEMORY[0xA3EC7] = *(_BYTE *)(2 * v2 + 0xA4E57);
          MEMORY[0xA3EAE] = *(_BYTE *)(2 * v2 + 0xA4E5C);
          MEMORY[0xA3EAF] = *(_BYTE *)(2 * v2 + 0xA4E5D);
          MEMORY[0xA3E96] = *(_BYTE *)(2 * v2 + 0xA4E62);
          MEMORY[0xA3E97] = *(_BYTE *)(2 * v2 + 0xA4E63);
          MEMORY[0xA40A6] = *(_BYTE *)(2 * v2 + 0xA4E68);
          MEMORY[0xA40A7] = *(_BYTE *)(2 * v2 + 0xA4E69);
          MEMORY[0xA4106] = *(_BYTE *)(2 * v2 + 0xA4E6E);
          MEMORY[0xA4107] = *(_BYTE *)(2 * v2 + 0xA4E6F);
          MEMORY[0xA411E] = *(_BYTE *)(2 * v2 + 0xA4E74);
          MEMORY[0xA411F] = *(_BYTE *)(2 * v2 + 0xA4E75);
          MEMORY[0xA4136] = *(_BYTE *)(2 * v2 + 0xA4E7A);
          MEMORY[0xA4137] = *(_BYTE *)(2 * v2 + 0xA4E7B);
          MEMORY[0xA41DE] = *(_BYTE *)(2 * v2 + 0xA4E80);
          MEMORY[0xA41DF] = *(_BYTE *)(2 * v2 + 0xA4E81);
          MEMORY[0xA414E] = *(_BYTE *)(2 * v2 + 0xA4E86);
          MEMORY[0xA414F] = *(_BYTE *)(2 * v2 + 0xA4E87);
          MEMORY[0xA4166] = *(_BYTE *)(2 * v2 + 0xA4E8C);
          MEMORY[0xA4167] = *(_BYTE *)(2 * v2 + 0xA4E8D);
          MEMORY[0xA41C6] = *(_BYTE *)(2 * v2 + 0xA4E92);
          MEMORY[0xA41C7] = *(_BYTE *)(2 * v2 + 0xA4E93);
          MEMORY[0xA41F6] = *(_BYTE *)(2 * v2 + 0xA4E98);
          MEMORY[0xA41F7] = *(_BYTE *)(2 * v2 + 0xA4E99);
        }
        MEMORY[0xA4588] = 0;
        MEMORY[0xA4587] = 0;
        break;
      default:
        if ( (unsigned int)*(unsigned __int8 *)result - 67 > 1 )
        {
          if ( (unsigned int)*(unsigned __int8 *)result - 68 > 2 )
          {
            if ( (unsigned int)*(unsigned __int8 *)result - 162 <= 1 )
            {
              MEMORY[0xA441E] = MEMORY[0xA4DBC];
              MEMORY[0xA441F] = MEMORY[0xA4DBD];
            }
          }
          else
          {
            MEMORY[0xA41AE] = MEMORY[0xA4DB8];
            MEMORY[0xA41AF] = 0;
          }
        }
        else
        {
          MEMORY[0xA4196] = MEMORY[0xA4EAF];
          MEMORY[0xA4197] = MEMORY[0xA4EB0];
          MEMORY[0xA4256] = MEMORY[0xA4EB1];
          MEMORY[0xA4257] = MEMORY[0xA4EB2];
          MEMORY[0xA42B6] = MEMORY[0xA4EB3];
          MEMORY[0xA42B7] = MEMORY[0xA4EB4];
          MEMORY[0xA429E] = MEMORY[0xA4EB5];
          MEMORY[0xA429F] = MEMORY[0xA4EB6];
          MEMORY[0xA4286] = MEMORY[0xA4EB7];
          MEMORY[0xA4287] = MEMORY[0xA4EB8];
          MEMORY[0xA426E] = MEMORY[0xA4EB9];
          MEMORY[0xA426F] = MEMORY[0xA4EBA];
          MEMORY[0xA423E] = MEMORY[0xA4EBB];
          MEMORY[0xA423F] = MEMORY[0xA4EBC];
        }
        break;
    }
  }
  return result;
}
