// Original V850 FD51 G1952; linked VA 0x8b86e; file offset 0x6b86e; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

undefined4 FUN_0008b86e(short *param_1)

{
  undefined4 uVar1;
  
  uVar1 = 0;
  if ((((*param_1 == 0xb) && (param_1[1] == 0)) && ((char)param_1[3] == '\x01')) &&
     (*(char *)((int)param_1 + 7) == '\0')) {
    uVar1 = 1;
  }
  return uVar1;
}

