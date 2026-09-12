// Original V850 FD51 G1952; linked VA 0x81dea; file offset 0x61dea; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_00081dea(int param_1,ushort param_2)

{
  undefined4 uVar1;
  
  uVar1 = 0;
  if ((((param_1 != 0) && (param_2 != 0)) && (param_2 < 0xfc)) && (_DAT_fedf4ca4 == 0)) {
    _DAT_fedf4eaa = FUN_000827d0(param_1,param_2,0xfedf4ca8);
    _DAT_fedf4ca4 = 1;
    DAT_fedf4ea8 = uRamfedfbf3a;
    FUN_00082b8e();
    uVar1 = 1;
  }
  return uVar1;
}

