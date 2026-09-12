// Original V850 FD51 G1952; linked VA 0x8508e; file offset 0x6508e; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008508e(undefined4 param_1)

{
  bool bVar1;
  uint uVar2;
  undefined1 auStack_8e [126];
  
  FUN_000b5188(auStack_8e,0,0x7e);
  bVar1 = true;
  uVar2 = 0;
  while ((uVar2 < 0xd && (bVar1))) {
    if (iRamfedfc164 == *(int *)(uVar2 * 8 + 0x276d0)) {
      uRamfedfc26a = (**(code **)(uVar2 * 8 + 0x276d4))(auStack_8e,param_1);
      FUN_000b51e6(0xfedfc1e4,auStack_8e,0x7e);
      bVar1 = false;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  FUN_0008512e(param_1);
  return;
}

