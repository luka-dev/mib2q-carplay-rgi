// Original V850 FD51 G1952; linked VA 0x8910c; file offset 0x6910c; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008910c(int param_1,int param_2,uint param_3)

{
  uint uVar1;
  
  if ((param_3 & 0xff) < 0x12) {
    for (uVar1 = 0; uVar1 < (param_3 & 0xff); uVar1 = uVar1 + 1 & 0xff) {
      if (*(int *)(uVar1 * 4 + param_2) < 0x43) {
        *(undefined1 *)(uVar1 + param_1) = *(undefined1 *)(*(int *)(uVar1 * 4 + param_2) + 0x27890);
      }
      else {
        *(undefined1 *)(uVar1 + param_1) = 0;
      }
    }
  }
  return;
}

