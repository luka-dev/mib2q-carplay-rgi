// Original V850 FD51 G1952; linked VA 0x890f0; file offset 0x690f0; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_000890f0(undefined1 *param_1,uint param_2)

{
  if ((param_2 & 0xff) < 5) {
    *param_1 = *(undefined1 *)((param_2 & 0xff) + 0x2749a);
  }
  else {
    *param_1 = 0;
  }
  return;
}

