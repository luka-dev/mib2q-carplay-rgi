// Original V850 FD51 G1952; linked VA 0x8b6fa; file offset 0x6b6fa; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

uint FUN_0008b6fa(int param_1,int param_2,byte *param_3,uint param_4,short param_5)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  
  bVar1 = false;
  uVar3 = 0x13;
  uVar2 = 0;
  while ((uVar2 < 0x13 && (!bVar1))) {
    if (*(short *)(uVar2 * 2 + 0x27d76) == param_5) {
      *(undefined4 *)((uint)*param_3 * 4 + param_1) =
           *(undefined4 *)((uVar2 + (param_4 & 0xff) * 0x13) * 8 + 0x39e24);
      *(undefined1 *)((uint)*param_3 + param_2) =
           *(undefined1 *)((uVar2 + (param_4 & 0xff) * 0x13) * 8 + 0x39e28);
      *param_3 = *param_3 + 1;
      bVar1 = true;
      uVar3 = uVar2;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return uVar3;
}

