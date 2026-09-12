// Original V850 FD51 G1952; linked VA 0x8a984; file offset 0x6a984; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008a984(void)

{
  int iVar1;
  
  if (sRamfedfc26a != 0) {
    uRamfedfc1e9 = uRamfedfc1e0;
    uRamfedfc1ea = uRamfedfc1e1;
    iVar1 = FUN_00081b9e(0xfedfc1e4,sRamfedfc26a);
    if (iVar1 == 1) {
      uRamfedfc267 = 3;
    }
    else if (cRamfedfc266 != -1) {
      uRamfedfc267 = 2;
    }
  }
  return;
}

