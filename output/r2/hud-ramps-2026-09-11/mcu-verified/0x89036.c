// Original V850 FD51 G1952; linked VA 0x89036; file offset 0x69036; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_00089036(undefined1 *param_1,uint param_2)

{
  if ((param_2 & 0xff) < 0x79) {
    *param_1 = *(undefined1 *)((param_2 & 0xff) + 0x277ba);
  }
  else {
    *param_1 = 0;
  }
  return;
}

