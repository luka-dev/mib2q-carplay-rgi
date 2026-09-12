// Original V850 FD51 G1952; linked VA 0x82a8e; file offset 0x62a8e; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_00082a8e(void)

{
  int iVar1;
  undefined4 uVar2;
  undefined1 auStack_208 [512];
  
  uVar2 = 0;
  FUN_000b5188(auStack_208,0,0x200);
  if (_DAT_fedf4ca4 == 1) {
    iVar1 = FUN_00082912(0xfedfbd50,auStack_208);
    if ((int)((uint)_DAT_fedf4eaa + iVar1) < 0x201) {
      FUN_000b51e6(auStack_208 + iVar1,0xfedf4ca8,_DAT_fedf4eaa);
      uRamfedfbd52 = 0;
    }
    else {
      FUN_000b5188(auStack_208,0,0x200);
      FUN_000b51e6(auStack_208,0xfedf4ca8,_DAT_fedf4eaa);
    }
    FUN_00083154(auStack_208,0x200);
    _DAT_fedf4ca4 = 2;
    uVar2 = 1;
    uRamfedfbf3b = 2;
  }
  return uVar2;
}

