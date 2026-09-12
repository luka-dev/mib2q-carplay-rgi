void __fastcall +[CPAccNavUpdate _valueFromMeasurement:forParamKey:](__int64 a1)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x0
  __int64 v5; // x22
  __int64 v6; // x0
  int v7; // w22
  __int64 v8; // x0
  __int64 v9; // x0
  __int64 v10; // x0
  __n128 v11; // q0
  __n128 v12; // q8
  __int64 v13; // x0
  __int64 v14; // x0
  __int64 v15; // x0
  __int64 v16; // x21
  __int64 v17; // x0
  __int64 v18; // x0
  __int64 v19; // x0
  __int64 v20; // x0
  __int64 _38; // [xsp+38h] [xbp+8h]

  v2 = MEMORY[0x24807FEA0](); /*0x247124ef4*/
  v3 = MEMORY[0x24807DD90](); /*0x247124efc*/
  v4 = MEMORY[0x248003E10](); /*0x247124f00*/
  v5 = MEMORY[0x24807DD00](v4); /*0x247124f08*/
  MEMORY[0x24807DD50](); /*0x247124f0c*/
  if ( v5 ) /*0x247124f14*/
  {
    v6 = MEMORY[0x248003E10](v3); /*0x247124f18*/
    MEMORY[0x24807DD00](v6); /*0x247124f1c*/
    v7 = MEMORY[0x248065010](v2); /*0x247124f30*/
    v8 = MEMORY[0x24807DD40](); /*0x247124f34*/
    if ( v7 ) /*0x247124f38*/
    {
      v9 = MEMORY[0x248003E10](v3); /*0x247124f40*/
      MEMORY[0x24807DD00](v9); /*0x247124f44*/
      v10 = MEMORY[0x248067030](v2); /*0x247124f54*/
      MEMORY[0x24807DD00](v10); /*0x247124f58*/
      v11.n128_f64[0] = MEMORY[0x248004080](); /*0x247124f60*/
      v12 = v11; /*0x247124f64*/
      v13 = MEMORY[0x24807DD50](); /*0x247124f68*/
      MEMORY[0x24807DD40](v13); /*0x247124f6c*/
      v14 = MEMORY[0x248065000](v3); /*0x247124f74*/
      v15 = NSNumberForCPAccNavTypeFromDouble(v14, v12); /*0x247124f7c*/
      v8 = MEMORY[0x24807DD00](v15); /*0x247124f80*/
    }
  }
  else if ( (unsigned int)MEMORY[0x248065CF0](v3) == 5 ) /*0x247124f90*/
  {
    v16 = MEMORY[0x2680CF688]; /*0x247124f98*/
    v17 = MEMORY[0x24805D390](v2); /*0x247124fa0*/
    MEMORY[0x24807DD00](v17); /*0x247124fa4*/
    CPDistanceUnitsFromNSUnitLength(); /*0x247124fac*/
    v18 = MEMORY[0x248007E10](v16); /*0x247124fb8*/
    MEMORY[0x24807DD00](v18); /*0x247124fbc*/
    v8 = MEMORY[0x24807DD50](); /*0x247124fc4*/
  }
  else
  {
    MEMORY[0x248004080](v2); /*0x247124fd8*/
    v19 = MEMORY[0x2480648E0](a1); /*0x247124fe0*/
    v8 = MEMORY[0x24807DD00](v19); /*0x247124fe4*/
  }
  v20 = MEMORY[0x24807DD30](v8); /*0x247124fec*/
  MEMORY[0x24807DD20](v20); /*0x247124ff0*/
  if ( ((_38 ^ (2 * _38)) & 0x4000000000000000LL) != 0 ) /*0x247125010*/
    __break(0xC471u); /*0x247125014*/
  JUMPOUT(0x24807FC90LL); /*0x24807fc90*/
}
