// Original V850 FD51 G1952; linked VA 0x83154; file offset 0x63154; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_00083154(int param_1,ushort param_2)

{
  int iVar1;
  
  if (((param_1 != 0) && (param_2 != 0)) && (param_2 < 0x201)) {
    FUN_000b5188(0xfedfbf48,0,0x200);
    FUN_000b51e6(0xfedfbf48,param_1,param_2);
    iVar1 = FUN_00083300();
    if (iVar1 == 1) {
      _DAT_ff6e0008 = 0xc10b;
      _DAT_ffff7b40 = _DAT_ffff7b40 | 0x10;
      _DAT_ffff73d0 = 1;
      _DAT_ffff73d4 = 0xbf4a;
      _DAT_ffff73d8 = 2;
      _DAT_ffff73e4 = 0x100c;
      _DAT_ffff73e8 = 1;
      _DAT_ffff73f2 = 0xff;
      _DAT_ffff73f6 = 1;
      _DAT_ffff73f8 = 0x2020;
      _DAT_ffff7b08 = 0x8061;
      _DAT_ffff60f6 = _DAT_ffff60f6 & 0xef7f;
      DAT_ffff73fa = DAT_ffff73fa & 0x7f | 1;
      _DAT_ff6e1008 = CONCAT11(uRamfedfbf49,uRamfedfbf48) | 0x20fe0000;
    }
  }
  return;
}

