// Original V850 FD51 G1952; linked VA 0x8b32e; file offset 0x6b32e; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008b32e(undefined1 *param_1,short param_2)

{
  bool bVar1;
  uint uVar2;
  
  bVar1 = false;
  uVar2 = 0;
  while ((uVar2 < 0x22 && (!bVar1))) {
    if (param_2 == *(short *)(uVar2 * 4 + 0x27e24)) {
      *param_1 = *(undefined1 *)(uVar2 * 4 + 0x27e26);
      bVar1 = true;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return;
}

