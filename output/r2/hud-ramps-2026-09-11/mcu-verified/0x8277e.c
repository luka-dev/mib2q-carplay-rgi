// Original V850 FD51 G1952; linked VA 0x8277e; file offset 0x6277e; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

uint FUN_0008277e(int param_1,uint param_2,int param_3)

{
  uint uVar1;
  uint uVar2;
  
  uVar2 = 0;
  if ((param_1 != 0) && (param_3 != 0)) {
    uVar1 = 0;
    while ((uVar1 < (param_2 & 0xffff) && (uVar2 < 0x200))) {
      *(undefined1 *)(uVar2 + param_3) = *(undefined1 *)(uVar1 + param_1);
      uVar2 = uVar2 + 1 & 0xffff;
      if (*(char *)(uVar1 + param_1) == '\x10') {
        *(undefined1 *)(uVar2 + param_3) = 0x10;
        uVar2 = uVar2 + 1 & 0xffff;
      }
      uVar1 = uVar1 + 1 & 0xffff;
    }
  }
  return uVar2;
}

