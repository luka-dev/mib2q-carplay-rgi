// Original V850 FD51 G1952; linked VA 0x8b3b2; file offset 0x6b3b2; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008b3b2(undefined4 *param_1,uint param_2,short param_3)

{
  bool bVar1;
  uint uVar2;
  
  bVar1 = false;
  *param_1 = *(undefined4 *)((param_2 & 0xff) * 0x88 + 0x38964);
  uVar2 = 0;
  while ((uVar2 < 0x11 && (!bVar1))) {
    if (param_3 == *(short *)(uVar2 * 2 + 0x27d54)) {
      *param_1 = *(undefined4 *)((uVar2 + (param_2 & 0xff) * 0x11) * 8 + 0x388e4);
      bVar1 = true;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return;
}

