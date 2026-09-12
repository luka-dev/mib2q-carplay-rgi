// Original V850 FD51 G1952; linked VA 0x8b486; file offset 0x6b486; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008b486(undefined1 *param_1,char param_2,short param_3)

{
  bool bVar1;
  uint uVar2;
  
  bVar1 = false;
  uVar2 = 0;
  while ((uVar2 < 4 && (!bVar1))) {
    if ((param_2 == *(char *)(uVar2 * 6 + 0x27978)) && (param_3 == *(short *)(uVar2 * 6 + 0x2797a)))
    {
      *param_1 = *(undefined1 *)(uVar2 * 6 + 0x2797c);
      bVar1 = true;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return;
}

