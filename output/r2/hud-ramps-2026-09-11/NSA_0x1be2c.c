int __fastcall sub_1BE2C(_BYTE *a1)
{
  char v2; // r0
  int result; // r0
  unsigned int i; // r6
  unsigned __int8 v5; // [sp+2h] [bp-2Ah]
  unsigned __int8 v6; // [sp+3h] [bp-29h]

  v6 = a1[5]; /*0x1be50*/
  if ( (unsigned int)v6 - 1 > 2 ) /*0x1be60*/
    v5 = -1; /*0x1be78*/
  else
    v5 = v6 - 1; /*0x1be6c*/
  if ( v5 == 255 ) /*0x1be84*/
  {
    result = v6; /*0x1cd38*/
    if ( !v6 ) /*0x1cd40*/
    {
      for ( i = 0; i < 3; i = (unsigned __int8)(i + 1) ) /*0x1cd44*/
      {
        *(_BYTE *)(2 * i + 0xA4DD2) = a1[6]; /*0x1cd60*/
        *(_BYTE *)(2 * i + 0xA4DD3) = 0; /*0x1cd78*/
        *(_BYTE *)(2 * i + 0xA4DD8) = a1[7]; /*0x1cd90*/
        *(_BYTE *)(2 * i + 0xA4DD9) = 0; /*0x1cda8*/
        *(_BYTE *)(2 * i + 0xA4DDE) = a1[8]; /*0x1cdc0*/
        *(_BYTE *)(2 * i + 0xA4DDF) = 0; /*0x1cdd8*/
        *(_BYTE *)(2 * i + 0xA4DE4) = a1[9]; /*0x1cde8*/
        *(_BYTE *)(2 * i + 0xA4DE5) = 0; /*0x1cdf8*/
        *(_BYTE *)(2 * i + 0xA4DEA) = a1[10]; /*0x1ce10*/
        *(_BYTE *)(2 * i + 0xA4DEB) = 0; /*0x1ce28*/
        *(_BYTE *)(2 * i + 0xA4DF0) = a1[11]; /*0x1ce40*/
        *(_BYTE *)(2 * i + 0xA4DF1) = 0; /*0x1ce58*/
        *(_BYTE *)(2 * i + 0xA4DF6) = a1[12]; /*0x1ce70*/
        *(_BYTE *)(2 * i + 0xA4DF7) = 0; /*0x1ce88*/
        *(_BYTE *)(2 * i + 0xA4DFC) = a1[13]; /*0x1cea0*/
        *(_BYTE *)(2 * i + 0xA4DFD) = 0; /*0x1ceb8*/
        *(_BYTE *)(2 * i + 0xA4E02) = a1[14]; /*0x1ced0*/
        *(_BYTE *)(2 * i + 0xA4E03) = 0; /*0x1cee8*/
        *(_BYTE *)(2 * i + 0xA4E08) = a1[15]; /*0x1cf00*/
        *(_BYTE *)(2 * i + 0xA4E09) = 0; /*0x1cf18*/
        *(_BYTE *)(2 * i + 0xA4E0E) = a1[16]; /*0x1cf30*/
        *(_BYTE *)(2 * i + 0xA4E0F) = 0; /*0x1cf48*/
        *(_BYTE *)(2 * i + 0xA4E14) = a1[17]; /*0x1cf58*/
        *(_BYTE *)(2 * i + 0xA4E15) = 0; /*0x1cf68*/
        *(_BYTE *)(2 * i + 0xA4E1A) = a1[18]; /*0x1cf80*/
        *(_BYTE *)(2 * i + 0xA4E1B) = 0; /*0x1cf98*/
        *(_BYTE *)(2 * i + 0xA4E20) = a1[19]; /*0x1cfb0*/
        *(_BYTE *)(2 * i + 0xA4E21) = 0; /*0x1cfc8*/
        *(_BYTE *)(2 * i + 0xA4E26) = a1[20]; /*0x1cfe0*/
        *(_BYTE *)(2 * i + 0xA4E27) = 0; /*0x1cff8*/
        *(_BYTE *)(2 * i + 0xA4E2C) = a1[21]; /*0x1d010*/
        *(_BYTE *)(2 * i + 0xA4E2D) = 0; /*0x1d028*/
        *(_BYTE *)(2 * i + 0xA4E32) = a1[22]; /*0x1d040*/
        *(_BYTE *)(2 * i + 0xA4E33) = 0; /*0x1d058*/
        *(_BYTE *)(2 * i + 0xA4E38) = a1[23]; /*0x1d070*/
        *(_BYTE *)(2 * i + 0xA4E39) = 0; /*0x1d088*/
        *(_BYTE *)(2 * i + 0xA4E3E) = a1[24]; /*0x1d0a0*/
        *(_BYTE *)(2 * i + 0xA4E3F) = 0; /*0x1d0b8*/
        *(_BYTE *)(2 * i + 0xA4E44) = a1[25]; /*0x1d0c8*/
        *(_BYTE *)(2 * i + 0xA4E45) = 0; /*0x1d0d8*/
        *(_BYTE *)(2 * i + 0xA4E4A) = a1[26]; /*0x1d0f0*/
        *(_BYTE *)(2 * i + 0xA4E4B) = 0; /*0x1d108*/
        *(_BYTE *)(2 * i + 0xA4E50) = a1[27]; /*0x1d120*/
        *(_BYTE *)(2 * i + 0xA4E51) = 0; /*0x1d138*/
        *(_BYTE *)(2 * i + 0xA4E56) = a1[28]; /*0x1d150*/
        *(_BYTE *)(2 * i + 0xA4E57) = 0; /*0x1d168*/
        *(_BYTE *)(2 * i + 0xA4E5C) = a1[29]; /*0x1d180*/
        *(_BYTE *)(2 * i + 0xA4E5D) = 0; /*0x1d198*/
        *(_BYTE *)(2 * i + 0xA4E62) = a1[30]; /*0x1d1b0*/
        *(_BYTE *)(2 * i + 0xA4E63) = 0; /*0x1d1c8*/
        *(_BYTE *)(2 * i + 0xA4E68) = a1[31]; /*0x1d1e0*/
        *(_BYTE *)(2 * i + 0xA4E69) = 0; /*0x1d1f8*/
        *(_BYTE *)(2 * i + 0xA4E6E) = a1[32]; /*0x1d210*/
        *(_BYTE *)(2 * i + 0xA4E6F) = 0; /*0x1d228*/
        *(_BYTE *)(2 * i + 0xA4E74) = a1[33]; /*0x1d238*/
        *(_BYTE *)(2 * i + 0xA4E75) = 0; /*0x1d248*/
        *(_BYTE *)(2 * i + 0xA4E7A) = a1[34]; /*0x1d260*/
        *(_BYTE *)(2 * i + 0xA4E7B) = 0; /*0x1d278*/
        *(_BYTE *)(2 * i + 0xA4E80) = a1[35]; /*0x1d290*/
        *(_BYTE *)(2 * i + 0xA4E81) = 0; /*0x1d2a8*/
        *(_BYTE *)(2 * i + 0xA4E86) = a1[36]; /*0x1d2c0*/
        *(_BYTE *)(2 * i + 0xA4E87) = 0; /*0x1d2d8*/
        *(_BYTE *)(2 * i + 0xA4E8C) = a1[37]; /*0x1d2f0*/
        *(_BYTE *)(2 * i + 0xA4E8D) = 0; /*0x1d308*/
        *(_BYTE *)(2 * i + 0xA4E92) = a1[38]; /*0x1d320*/
        *(_BYTE *)(2 * i + 0xA4E93) = 0; /*0x1d338*/
        *(_BYTE *)(2 * i + 0xA4E98) = a1[39]; /*0x1d34c*/
        *(_BYTE *)(2 * i + 0xA4E99) = 0; /*0x1d360*/
        *(_BYTE *)(4 * i + 0xA4EA0) = a1[41]; /*0x1d374*/
        *(_BYTE *)(4 * i + 0xA4EA1) = a1[42]; /*0x1d38c*/
        *(_BYTE *)(4 * i + 0xA4EA2) = a1[43]; /*0x1d3a4*/
        *(_BYTE *)(4 * i + 0xA4EA3) = a1[44]; /*0x1d3bc*/
        *(_BYTE *)(i + 675500) = a1[40]; /*0x1d3cc*/
        sub_20474(i); /*0x1d3d4*/
      }
      result = MEMORY[0xA40EE]; /*0x1d3e8*/
      if ( !MEMORY[0xA40EE] ) /*0x1d3f0*/
      {
        MEMORY[0xA40D6] = MEMORY[0xA4DD4]; /*0x1d410*/
        MEMORY[0xA40D7] = MEMORY[0xA4DD5]; /*0x1d42c*/
        MEMORY[0xA40BE] = MEMORY[0xA4DDA]; /*0x1d444*/
        MEMORY[0xA40BF] = MEMORY[0xA4DDB]; /*0x1d460*/
        MEMORY[0xA420E] = MEMORY[0xA4DE0]; /*0x1d478*/
        MEMORY[0xA420F] = MEMORY[0xA4DE1]; /*0x1d494*/
        MEMORY[0xA408E] = MEMORY[0xA4DE6]; /*0x1d4a4*/
        MEMORY[0xA408F] = MEMORY[0xA4DE7]; /*0x1d4b8*/
        MEMORY[0xA4076] = MEMORY[0xA4DEC]; /*0x1d4d0*/
        MEMORY[0xA4077] = MEMORY[0xA4DED]; /*0x1d4ec*/
        MEMORY[0xA405E] = MEMORY[0xA4DF2]; /*0x1d504*/
        MEMORY[0xA405F] = MEMORY[0xA4DF3]; /*0x1d520*/
        MEMORY[0xA4046] = MEMORY[0xA4DF8]; /*0x1d538*/
        MEMORY[0xA4047] = MEMORY[0xA4DF9]; /*0x1d554*/
        MEMORY[0xA402E] = MEMORY[0xA4DFE]; /*0x1d56c*/
        MEMORY[0xA402F] = MEMORY[0xA4DFF]; /*0x1d588*/
        MEMORY[0xA4016] = MEMORY[0xA4E04]; /*0x1d5a0*/
        MEMORY[0xA4017] = MEMORY[0xA4E05]; /*0x1d5bc*/
        MEMORY[0xA3FFE] = MEMORY[0xA4E0A]; /*0x1d5d4*/
        MEMORY[0xA3FFF] = MEMORY[0xA4E0B]; /*0x1d5f0*/
        MEMORY[0xA3FE6] = MEMORY[0xA4E10]; /*0x1d608*/
        MEMORY[0xA3FE7] = MEMORY[0xA4E11]; /*0x1d624*/
        MEMORY[0xA3FCE] = MEMORY[0xA4E16]; /*0x1d634*/
        MEMORY[0xA3FCF] = MEMORY[0xA4E17]; /*0x1d648*/
        MEMORY[0xA3FB6] = MEMORY[0xA4E1C]; /*0x1d660*/
        MEMORY[0xA3FB7] = MEMORY[0xA4E1D]; /*0x1d67c*/
        MEMORY[0xA3F9E] = MEMORY[0xA4E22]; /*0x1d694*/
        MEMORY[0xA3F9F] = MEMORY[0xA4E23]; /*0x1d6b0*/
        MEMORY[0xA3F86] = MEMORY[0xA4E28]; /*0x1d6c8*/
        MEMORY[0xA3F87] = MEMORY[0xA4E29]; /*0x1d6e4*/
        MEMORY[0xA3F6E] = MEMORY[0xA4E2E]; /*0x1d6fc*/
        MEMORY[0xA3F6F] = MEMORY[0xA4E2F]; /*0x1d718*/
        MEMORY[0xA3F56] = MEMORY[0xA4E34]; /*0x1d730*/
        MEMORY[0xA3F57] = MEMORY[0xA4E35]; /*0x1d74c*/
        MEMORY[0xA3F3E] = MEMORY[0xA4E3A]; /*0x1d764*/
        MEMORY[0xA3F3F] = MEMORY[0xA4E3B]; /*0x1d780*/
        MEMORY[0xA3F26] = MEMORY[0xA4E40]; /*0x1d798*/
        MEMORY[0xA3F27] = MEMORY[0xA4E41]; /*0x1d7b4*/
        MEMORY[0xA3F0E] = MEMORY[0xA4E46]; /*0x1d7c4*/
        MEMORY[0xA3F0F] = MEMORY[0xA4E47]; /*0x1d7d8*/
        MEMORY[0xA3EF6] = MEMORY[0xA4E4C]; /*0x1d7f0*/
        MEMORY[0xA3EF7] = MEMORY[0xA4E4D]; /*0x1d80c*/
        MEMORY[0xA3EDE] = MEMORY[0xA4E52]; /*0x1d824*/
        MEMORY[0xA3EDF] = MEMORY[0xA4E53]; /*0x1d840*/
        MEMORY[0xA3EC6] = MEMORY[0xA4E58]; /*0x1d858*/
        MEMORY[0xA3EC7] = MEMORY[0xA4E59]; /*0x1d874*/
        MEMORY[0xA3EAE] = MEMORY[0xA4E5E]; /*0x1d88c*/
        MEMORY[0xA3EAF] = MEMORY[0xA4E5F]; /*0x1d8a8*/
        MEMORY[0xA3E96] = MEMORY[0xA4E64]; /*0x1d8c0*/
        MEMORY[0xA3E97] = MEMORY[0xA4E65]; /*0x1d8dc*/
        MEMORY[0xA40A6] = MEMORY[0xA4E6A]; /*0x1d8f4*/
        MEMORY[0xA40A7] = MEMORY[0xA4E6B]; /*0x1d910*/
        MEMORY[0xA4106] = MEMORY[0xA4E70]; /*0x1d928*/
        MEMORY[0xA4107] = MEMORY[0xA4E71]; /*0x1d944*/
        MEMORY[0xA411E] = MEMORY[0xA4E76]; /*0x1d954*/
        MEMORY[0xA411F] = MEMORY[0xA4E77]; /*0x1d968*/
        MEMORY[0xA4136] = MEMORY[0xA4E7C]; /*0x1d980*/
        MEMORY[0xA4137] = MEMORY[0xA4E7D]; /*0x1d99c*/
        MEMORY[0xA41DE] = MEMORY[0xA4E82]; /*0x1d9b4*/
        MEMORY[0xA41DF] = MEMORY[0xA4E83]; /*0x1d9d0*/
        MEMORY[0xA414E] = MEMORY[0xA4E88]; /*0x1d9e8*/
        MEMORY[0xA414F] = MEMORY[0xA4E89]; /*0x1da04*/
        MEMORY[0xA4166] = MEMORY[0xA4E8E]; /*0x1da1c*/
        MEMORY[0xA4167] = MEMORY[0xA4E8F]; /*0x1da38*/
        MEMORY[0xA41C6] = MEMORY[0xA4E94]; /*0x1da50*/
        MEMORY[0xA41C7] = MEMORY[0xA4E95]; /*0x1da6c*/
        MEMORY[0xA41F6] = MEMORY[0xA4E9A]; /*0x1da80*/
        MEMORY[0xA41F7] = MEMORY[0xA4E9B]; /*0x1da98*/
        return MEMORY[0xA4E9B]; /*0x1da94*/
      }
    }
  }
  else
  {
    *(_BYTE *)(2 * v5 + 0xA4DCC) = a1[5]; /*0x1bea0*/
    *(_BYTE *)(2 * v5 + 0xA4DCD) = 0; /*0x1bebc*/
    *(_BYTE *)(2 * v5 + 0xA4DD2) = a1[6]; /*0x1bed8*/
    *(_BYTE *)(2 * v5 + 0xA4DD3) = 0; /*0x1bef4*/
    *(_BYTE *)(2 * v5 + 0xA4DD8) = a1[7]; /*0x1bf10*/
    *(_BYTE *)(2 * v5 + 0xA4DD9) = 0; /*0x1bf2c*/
    *(_BYTE *)(2 * v5 + 0xA4DDE) = a1[8]; /*0x1bf48*/
    *(_BYTE *)(2 * v5 + 0xA4DDF) = 0; /*0x1bf64*/
    *(_BYTE *)(2 * v5 + 0xA4DE4) = a1[9]; /*0x1bf78*/
    *(_BYTE *)(2 * v5 + 0xA4DE5) = 0; /*0x1bf8c*/
    *(_BYTE *)(2 * v5 + 0xA4DEA) = a1[10]; /*0x1bfa8*/
    *(_BYTE *)(2 * v5 + 0xA4DEB) = 0; /*0x1bfc4*/
    *(_BYTE *)(2 * v5 + 0xA4DF0) = a1[11]; /*0x1bfe0*/
    *(_BYTE *)(2 * v5 + 0xA4DF1) = 0; /*0x1bffc*/
    *(_BYTE *)(2 * v5 + 0xA4DF6) = a1[12]; /*0x1c018*/
    *(_BYTE *)(2 * v5 + 0xA4DF7) = 0; /*0x1c034*/
    *(_BYTE *)(2 * v5 + 0xA4DFC) = a1[13]; /*0x1c050*/
    *(_BYTE *)(2 * v5 + 0xA4DFD) = 0; /*0x1c06c*/
    *(_BYTE *)(2 * v5 + 0xA4E02) = a1[14]; /*0x1c088*/
    *(_BYTE *)(2 * v5 + 0xA4E03) = 0; /*0x1c0a4*/
    *(_BYTE *)(2 * v5 + 0xA4E08) = a1[15]; /*0x1c0c0*/
    *(_BYTE *)(2 * v5 + 0xA4E09) = 0; /*0x1c0dc*/
    *(_BYTE *)(2 * v5 + 0xA4E0E) = a1[16]; /*0x1c0f8*/
    *(_BYTE *)(2 * v5 + 0xA4E0F) = 0; /*0x1c114*/
    *(_BYTE *)(2 * v5 + 0xA4E14) = a1[17]; /*0x1c128*/
    *(_BYTE *)(2 * v5 + 0xA4E15) = 0; /*0x1c13c*/
    *(_BYTE *)(2 * v5 + 0xA4E1A) = a1[18]; /*0x1c158*/
    *(_BYTE *)(2 * v5 + 0xA4E1B) = 0; /*0x1c174*/
    *(_BYTE *)(2 * v5 + 0xA4E20) = a1[19]; /*0x1c190*/
    *(_BYTE *)(2 * v5 + 0xA4E21) = 0; /*0x1c1ac*/
    *(_BYTE *)(2 * v5 + 0xA4E26) = a1[20]; /*0x1c1c8*/
    *(_BYTE *)(2 * v5 + 0xA4E27) = 0; /*0x1c1e4*/
    *(_BYTE *)(2 * v5 + 0xA4E2C) = a1[21]; /*0x1c200*/
    *(_BYTE *)(2 * v5 + 0xA4E2D) = 0; /*0x1c21c*/
    *(_BYTE *)(2 * v5 + 0xA4E32) = a1[22]; /*0x1c238*/
    *(_BYTE *)(2 * v5 + 0xA4E33) = 0; /*0x1c254*/
    *(_BYTE *)(2 * v5 + 0xA4E38) = a1[23]; /*0x1c270*/
    *(_BYTE *)(2 * v5 + 0xA4E39) = 0; /*0x1c28c*/
    *(_BYTE *)(2 * v5 + 0xA4E3E) = a1[24]; /*0x1c2a8*/
    *(_BYTE *)(2 * v5 + 0xA4E3F) = 0; /*0x1c2c4*/
    *(_BYTE *)(2 * v5 + 0xA4E44) = a1[25]; /*0x1c2d8*/
    *(_BYTE *)(2 * v5 + 0xA4E45) = 0; /*0x1c2ec*/
    *(_BYTE *)(2 * v5 + 0xA4E4A) = a1[26]; /*0x1c308*/
    *(_BYTE *)(2 * v5 + 0xA4E4B) = 0; /*0x1c324*/
    *(_BYTE *)(2 * v5 + 0xA4E50) = a1[27]; /*0x1c340*/
    *(_BYTE *)(2 * v5 + 0xA4E51) = 0; /*0x1c35c*/
    *(_BYTE *)(2 * v5 + 0xA4E56) = a1[28]; /*0x1c378*/
    *(_BYTE *)(2 * v5 + 0xA4E57) = 0; /*0x1c394*/
    *(_BYTE *)(2 * v5 + 0xA4E5C) = a1[29]; /*0x1c3b0*/
    *(_BYTE *)(2 * v5 + 0xA4E5D) = 0; /*0x1c3cc*/
    *(_BYTE *)(2 * v5 + 0xA4E62) = a1[30]; /*0x1c3e8*/
    *(_BYTE *)(2 * v5 + 0xA4E63) = 0; /*0x1c404*/
    *(_BYTE *)(2 * v5 + 0xA4E68) = a1[31]; /*0x1c420*/
    *(_BYTE *)(2 * v5 + 0xA4E69) = 0; /*0x1c43c*/
    *(_BYTE *)(2 * v5 + 0xA4E6E) = a1[32]; /*0x1c458*/
    *(_BYTE *)(2 * v5 + 0xA4E6F) = 0; /*0x1c474*/
    *(_BYTE *)(2 * v5 + 0xA4E74) = a1[33]; /*0x1c488*/
    *(_BYTE *)(2 * v5 + 0xA4E75) = 0; /*0x1c49c*/
    *(_BYTE *)(2 * v5 + 0xA4E7A) = a1[34]; /*0x1c4b8*/
    *(_BYTE *)(2 * v5 + 0xA4E7B) = 0; /*0x1c4d4*/
    *(_BYTE *)(2 * v5 + 0xA4E80) = a1[35]; /*0x1c4f0*/
    *(_BYTE *)(2 * v5 + 0xA4E81) = 0; /*0x1c50c*/
    *(_BYTE *)(2 * v5 + 0xA4E86) = a1[36]; /*0x1c528*/
    *(_BYTE *)(2 * v5 + 0xA4E87) = 0; /*0x1c544*/
    *(_BYTE *)(2 * v5 + 0xA4E8C) = a1[37]; /*0x1c560*/
    *(_BYTE *)(2 * v5 + 0xA4E8D) = 0; /*0x1c57c*/
    *(_BYTE *)(2 * v5 + 0xA4E92) = a1[38]; /*0x1c598*/
    *(_BYTE *)(2 * v5 + 0xA4E93) = 0; /*0x1c5b4*/
    *(_BYTE *)(2 * v5 + 0xA4E98) = a1[39]; /*0x1c5cc*/
    *(_BYTE *)(2 * v5 + 0xA4E99) = 0; /*0x1c5e4*/
    *(_BYTE *)(4 * v5 + 0xA4EA0) = a1[41]; /*0x1c5fc*/
    *(_BYTE *)(4 * v5 + 0xA4EA1) = a1[42]; /*0x1c618*/
    *(_BYTE *)(4 * v5 + 0xA4EA2) = a1[43]; /*0x1c634*/
    *(_BYTE *)(4 * v5 + 0xA4EA3) = a1[44]; /*0x1c650*/
    *(_BYTE *)(v5 + 0xA4EAC) = a1[40]; /*0x1c664*/
    sub_20474(v5); /*0x1c66c*/
    v2 = MEMORY[0xA40EE]; /*0x1c670*/
    if ( !MEMORY[0xA40EE] ) /*0x1c678*/
      v2 = 2; /*0x1c67c*/
    result = (unsigned __int8)(v2 - 1); /*0x1c684*/
    if ( result == v5 ) /*0x1c690*/
    {
      MEMORY[0xA40D6] = *(_BYTE *)(2 * v5 + 0xA4DD2); /*0x1c6a8*/
      MEMORY[0xA40D7] = *(_BYTE *)(2 * v5 + 0xA4DD3); /*0x1c6c4*/
      MEMORY[0xA40BE] = *(_BYTE *)(2 * v5 + 0xA4DD8); /*0x1c6dc*/
      MEMORY[0xA40BF] = *(_BYTE *)(2 * v5 + 0xA4DD9); /*0x1c6f8*/
      MEMORY[0xA420E] = *(_BYTE *)(2 * v5 + 0xA4DDE); /*0x1c710*/
      MEMORY[0xA420F] = *(_BYTE *)(2 * v5 + 0xA4DDF); /*0x1c72c*/
      MEMORY[0xA408E] = *(_BYTE *)(2 * v5 + 0xA4DE4); /*0x1c73c*/
      MEMORY[0xA408F] = *(_BYTE *)(2 * v5 + 0xA4DE5); /*0x1c750*/
      MEMORY[0xA4076] = *(_BYTE *)(2 * v5 + 0xA4DEA); /*0x1c768*/
      MEMORY[0xA4077] = *(_BYTE *)(2 * v5 + 0xA4DEB); /*0x1c784*/
      MEMORY[0xA405E] = *(_BYTE *)(2 * v5 + 0xA4DF0); /*0x1c79c*/
      MEMORY[0xA405F] = *(_BYTE *)(2 * v5 + 0xA4DF1); /*0x1c7b8*/
      MEMORY[0xA4046] = *(_BYTE *)(2 * v5 + 0xA4DF6); /*0x1c7d0*/
      MEMORY[0xA4047] = *(_BYTE *)(2 * v5 + 0xA4DF7); /*0x1c7ec*/
      MEMORY[0xA402E] = *(_BYTE *)(2 * v5 + 0xA4DFC); /*0x1c804*/
      MEMORY[0xA402F] = *(_BYTE *)(2 * v5 + 0xA4DFD); /*0x1c820*/
      MEMORY[0xA4016] = *(_BYTE *)(2 * v5 + 0xA4E02); /*0x1c838*/
      MEMORY[0xA4017] = *(_BYTE *)(2 * v5 + 0xA4E03); /*0x1c854*/
      MEMORY[0xA3FFE] = *(_BYTE *)(2 * v5 + 0xA4E08); /*0x1c86c*/
      MEMORY[0xA3FFF] = *(_BYTE *)(2 * v5 + 0xA4E09); /*0x1c888*/
      MEMORY[0xA3FE6] = *(_BYTE *)(2 * v5 + 0xA4E0E); /*0x1c8a0*/
      MEMORY[0xA3FE7] = *(_BYTE *)(2 * v5 + 0xA4E0F); /*0x1c8bc*/
      MEMORY[0xA3FCE] = *(_BYTE *)(2 * v5 + 0xA4E14); /*0x1c8cc*/
      MEMORY[0xA3FCF] = *(_BYTE *)(2 * v5 + 0xA4E15); /*0x1c8e0*/
      MEMORY[0xA3FB6] = *(_BYTE *)(2 * v5 + 0xA4E1A); /*0x1c8f8*/
      MEMORY[0xA3FB7] = *(_BYTE *)(2 * v5 + 0xA4E1B); /*0x1c914*/
      MEMORY[0xA3F9E] = *(_BYTE *)(2 * v5 + 0xA4E20); /*0x1c92c*/
      MEMORY[0xA3F9F] = *(_BYTE *)(2 * v5 + 0xA4E21); /*0x1c948*/
      MEMORY[0xA3F86] = *(_BYTE *)(2 * v5 + 0xA4E26); /*0x1c960*/
      MEMORY[0xA3F87] = *(_BYTE *)(2 * v5 + 0xA4E27); /*0x1c97c*/
      MEMORY[0xA3F6E] = *(_BYTE *)(2 * v5 + 0xA4E2C); /*0x1c994*/
      MEMORY[0xA3F6F] = *(_BYTE *)(2 * v5 + 0xA4E2D); /*0x1c9b0*/
      MEMORY[0xA3F56] = *(_BYTE *)(2 * v5 + 0xA4E32); /*0x1c9c8*/
      MEMORY[0xA3F57] = *(_BYTE *)(2 * v5 + 0xA4E33); /*0x1c9e4*/
      MEMORY[0xA3F3E] = *(_BYTE *)(2 * v5 + 0xA4E38); /*0x1c9fc*/
      MEMORY[0xA3F3F] = *(_BYTE *)(2 * v5 + 0xA4E39); /*0x1ca18*/
      MEMORY[0xA3F26] = *(_BYTE *)(2 * v5 + 0xA4E3E); /*0x1ca30*/
      MEMORY[0xA3F27] = *(_BYTE *)(2 * v5 + 0xA4E3F); /*0x1ca4c*/
      MEMORY[0xA3F0E] = *(_BYTE *)(2 * v5 + 0xA4E44); /*0x1ca5c*/
      MEMORY[0xA3F0F] = *(_BYTE *)(2 * v5 + 0xA4E45); /*0x1ca70*/
      MEMORY[0xA3EF6] = *(_BYTE *)(2 * v5 + 0xA4E4A); /*0x1ca88*/
      MEMORY[0xA3EF7] = *(_BYTE *)(2 * v5 + 0xA4E4B); /*0x1caa4*/
      MEMORY[0xA3EDE] = *(_BYTE *)(2 * v5 + 0xA4E50); /*0x1cabc*/
      MEMORY[0xA3EDF] = *(_BYTE *)(2 * v5 + 0xA4E51); /*0x1cad8*/
      MEMORY[0xA3EC6] = *(_BYTE *)(2 * v5 + 0xA4E56); /*0x1caf0*/
      MEMORY[0xA3EC7] = *(_BYTE *)(2 * v5 + 0xA4E57); /*0x1cb0c*/
      MEMORY[0xA3EAE] = *(_BYTE *)(2 * v5 + 0xA4E5C); /*0x1cb24*/
      MEMORY[0xA3EAF] = *(_BYTE *)(2 * v5 + 0xA4E5D); /*0x1cb40*/
      MEMORY[0xA3E96] = *(_BYTE *)(2 * v5 + 0xA4E62); /*0x1cb58*/
      MEMORY[0xA3E97] = *(_BYTE *)(2 * v5 + 0xA4E63); /*0x1cb74*/
      MEMORY[0xA40A6] = *(_BYTE *)(2 * v5 + 0xA4E68); /*0x1cb8c*/
      MEMORY[0xA40A7] = *(_BYTE *)(2 * v5 + 0xA4E69); /*0x1cba8*/
      MEMORY[0xA4106] = *(_BYTE *)(2 * v5 + 0xA4E6E); /*0x1cbc0*/
      MEMORY[0xA4107] = *(_BYTE *)(2 * v5 + 0xA4E6F); /*0x1cbdc*/
      MEMORY[0xA411E] = *(_BYTE *)(2 * v5 + 0xA4E74); /*0x1cbec*/
      MEMORY[0xA411F] = *(_BYTE *)(2 * v5 + 0xA4E75); /*0x1cc00*/
      MEMORY[0xA4136] = *(_BYTE *)(2 * v5 + 0xA4E7A); /*0x1cc18*/
      MEMORY[0xA4137] = *(_BYTE *)(2 * v5 + 0xA4E7B); /*0x1cc34*/
      MEMORY[0xA41DE] = *(_BYTE *)(2 * v5 + 0xA4E80); /*0x1cc4c*/
      MEMORY[0xA41DF] = *(_BYTE *)(2 * v5 + 0xA4E81); /*0x1cc68*/
      MEMORY[0xA414E] = *(_BYTE *)(2 * v5 + 0xA4E86); /*0x1cc80*/
      MEMORY[0xA414F] = *(_BYTE *)(2 * v5 + 0xA4E87); /*0x1cc9c*/
      MEMORY[0xA4166] = *(_BYTE *)(2 * v5 + 0xA4E8C); /*0x1ccb4*/
      MEMORY[0xA4167] = *(_BYTE *)(2 * v5 + 0xA4E8D); /*0x1ccd0*/
      MEMORY[0xA41C6] = *(_BYTE *)(2 * v5 + 0xA4E92); /*0x1cce8*/
      MEMORY[0xA41C7] = *(_BYTE *)(2 * v5 + 0xA4E93); /*0x1cd04*/
      MEMORY[0xA41F6] = *(_BYTE *)(2 * v5 + 0xA4E98); /*0x1cd18*/
      MEMORY[0xA41F7] = *(_BYTE *)(2 * v5 + 0xA4E99); /*0x1cd30*/
    }
  }
  return result; /*0x1daa4*/
}
