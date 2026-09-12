// Original V850 FD51 G1952; linked VA 0x86534; file offset 0x66534; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_00086534(undefined1 *param_1,undefined4 param_2,undefined4 param_3)

{
  uint uVar1;
  int iVar2;
  undefined1 local_5c [4];
  undefined1 auStack_58 [68];
  undefined1 uStack_14;
  undefined1 uStack_13;
  
  FUN_000b5188(local_5c,0,0x4c);
  *param_1 = 0;
  param_1[1] = 0;
  uVar1 = 0;
  do {
    param_1[uVar1 + 2] = 0;
    uVar1 = uVar1 + 1 & 0xff;
  } while (uVar1 < 0x11);
  if ((iRamfedfc170 == 3) || (iRamfedfc170 == 8)) {
    FUN_00087bb0(param_2,param_3);
    iVar2 = FUN_00099a64(2,local_5c);
    if (iVar2 == 1) {
      FUN_00089036(param_1,local_5c[0]);
      FUN_000890f0(param_1 + 1,uStack_13);
      FUN_0008910c(param_1 + 2,auStack_58,uStack_14);
    }
  }
  return;
}

