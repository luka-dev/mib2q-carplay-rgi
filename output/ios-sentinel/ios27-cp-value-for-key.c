void __fastcall +[CPAccNavUpdate update:valueForParamKey:](__int64 a1)
{
  __int64 v2; // x19
  __int64 v3; // x22
  __int64 v4; // x0
  __int64 v5; // x0
  __int64 v6; // x0
  __int64 v7; // x20
  __int64 v8; // x0
  __int64 v9; // x0
  __int64 v10; // x0
  __int64 v11; // x0
  __int64 v12; // x23
  __int64 v13; // x24
  __int64 v14; // x0
  __int64 v15; // x0
  char v16; // w23
  __int64 v17; // x0
  __int64 v18; // x0
  __int64 v19; // x0
  __int64 v20; // x23
  __int64 v21; // x24
  __int64 v22; // x0
  __int64 v23; // x23
  __int64 v24; // x0
  __int64 v25; // x24
  __int64 v26; // x25
  __int64 v27; // x0
  __int64 v28; // x0
  __int64 v29; // x23
  __int64 v30; // x24
  __int64 v31; // x0
  __int64 v32; // x21
  double v33; // d0
  __int64 v34; // x1
  __int64 v35; // x0
  __int64 _48; // [xsp+48h] [xbp+8h]

  v2 = MEMORY[0x24807FF30](); /*0x247125044*/
  v3 = MEMORY[0x24807DD90](); /*0x24712504c*/
  v4 = MEMORY[0x248067390](v2); /*0x247125054*/
  MEMORY[0x24807DD00](v4); /*0x247125058*/
  v5 = MEMORY[0x24804C330](); /*0x247125060*/
  MEMORY[0x24807DD00](v5); /*0x247125064*/
  v6 = MEMORY[0x24800C080](v3); /*0x247125074*/
  v7 = MEMORY[0x24807DD00](v6); /*0x24712507c*/
  v8 = MEMORY[0x24807DD50](); /*0x247125080*/
  v9 = MEMORY[0x24807FDE0](v8); /*0x247125084*/
  v10 = MEMORY[0x24807DD60](v9); /*0x247125088*/
  if ( !v7 ) /*0x24712508c*/
    goto LABEL_19; /*0x24712508c*/
  v11 = MEMORY[0x248067390](v2); /*0x247125094*/
  MEMORY[0x24807DD00](v11); /*0x247125098*/
  v12 = MEMORY[0x248067220](); /*0x2471250a4*/
  v13 = MEMORY[0x24807FD70](MEMORY[0x2680CF600]); /*0x2471250b4*/
  MEMORY[0x24807DD50](); /*0x2471250b8*/
  if ( v12 != v13 ) /*0x2471250c0*/
  {
    if ( (unsigned int)MEMORY[0x248065CF0](v2) || (unsigned int)MEMORY[0x248066890](v2) ) /*0x2471250d4*/
    {
      v14 = MEMORY[0x248065000](v2); /*0x2471250e0*/
      v15 = NotSetFromCPAccNavType(v14); /*0x2471250e4*/
      MEMORY[0x24807DD00](v15); /*0x2471250e8*/
      v16 = MEMORY[0x248006850](v7); /*0x2471250fc*/
      v17 = MEMORY[0x24807DD50](); /*0x247125100*/
      if ( (v16 & 1) != 0 ) /*0x247125104*/
        goto LABEL_18; /*0x247125104*/
    }
    if ( (unsigned int)MEMORY[0x2480668A0](v2) ) /*0x24712510c*/
    {
      v17 = MEMORY[0x248006850](v7); /*0x247125120*/
      if ( (v17 & 1) != 0 ) /*0x247125124*/
        goto LABEL_18; /*0x247125124*/
    }
    v19 = MEMORY[0x248067390](v2); /*0x24712514c*/
    MEMORY[0x24807DD00](v19); /*0x247125150*/
    v20 = MEMORY[0x248067220](); /*0x24712515c*/
    v21 = MEMORY[0x24807FD70](MEMORY[0x2680C2EB0]); /*0x24712516c*/
    MEMORY[0x24807DD50](); /*0x247125170*/
    if ( v20 == v21 ) /*0x24712517c*/
    {
      v32 = MEMORY[0x248065000](v2); /*0x247125230*/
      v33 = MEMORY[0x24800B830](v7); /*0x247125238*/
      NSNumberForCPAccNavTypeFromDouble(v32, v34, v33); /*0x247125240*/
      v17 = MEMORY[0x24807DD00](); /*0x247125244*/
      goto LABEL_18; /*0x247125248*/
    }
    v22 = MEMORY[0x248067390](v2); /*0x247125180*/
    MEMORY[0x24807DD00](v22); /*0x247125184*/
    v23 = MEMORY[0x248067220](); /*0x247125190*/
    if ( v23 == MEMORY[0x24807FD70](MEMORY[0x2680C2E78]) ) /*0x2471251a4*/
    {
      MEMORY[0x24807DD50](); /*0x24712524c*/
    }
    else
    {
      v24 = MEMORY[0x248067390](v2); /*0x2471251ac*/
      MEMORY[0x24807DD00](v24); /*0x2471251b0*/
      v25 = MEMORY[0x248067220](); /*0x2471251bc*/
      v26 = MEMORY[0x24807FD70](MEMORY[0x2680C2FA8]); /*0x2471251cc*/
      v27 = MEMORY[0x24807DD60](); /*0x2471251d0*/
      MEMORY[0x24807DD50](v27); /*0x2471251d4*/
      if ( v25 != v26 ) /*0x2471251dc*/
      {
        v28 = MEMORY[0x248067390](v2); /*0x2471251e4*/
        MEMORY[0x24807DD00](v28); /*0x2471251e8*/
        v29 = MEMORY[0x248067220](); /*0x2471251f4*/
        v30 = MEMORY[0x24807FD70](off_26908DE48); /*0x247125204*/
        v10 = MEMORY[0x24807DD50](); /*0x247125208*/
        if ( v29 != v30 ) /*0x247125210*/
          goto LABEL_19; /*0x247125210*/
        v31 = MEMORY[0x248064F40](a1); /*0x247125220*/
        v17 = MEMORY[0x24807DD00](v31); /*0x247125224*/
LABEL_18:
        v10 = MEMORY[0x24807DD30](v17); /*0x247125268*/
LABEL_19:
        MEMORY[0x24807DD20](v10); /*0x247125270*/
        if ( ((_48 ^ (2 * _48)) & 0x4000000000000000LL) != 0 ) /*0x247125294*/
          __break(0xC471u); /*0x247125298*/
        JUMPOUT(0x24807FC90LL); /*0x24807fc90*/
      }
    }
    v35 = MEMORY[0x248064F50](a1); /*0x24712525c*/
    v17 = MEMORY[0x24807DD00](v35); /*0x247125260*/
    goto LABEL_18; /*0x247125260*/
  }
  v18 = MEMORY[0x248064F60](a1); /*0x24712513c*/
  v17 = MEMORY[0x24807DD00](v18); /*0x247125140*/
  goto LABEL_18; /*0x247125144*/
}
