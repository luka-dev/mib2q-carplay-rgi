// Original V850 FD51 G1952; linked VA 0x974e8; file offset 0x774e8; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_000974e8(int param_1)

{
  undefined4 *puVar1;
  undefined4 *puVar2;
  undefined4 local_378 [108];
  undefined4 local_1c8;
  undefined4 uStack_1c4;
  undefined4 uStack_1c0;
  undefined4 uStack_1bc;
  undefined4 local_1b8 [17];
  undefined1 uStack_171;
  
  puVar1 = local_378;
  puVar2 = local_378;
  FUN_0008c340(&local_1c8);
  if (param_1 == 3) {
    puVar1 = local_1b8;
    do {
      *puVar2 = *puVar1;
      puVar1 = puVar1 + 1;
      puVar2 = puVar2 + 1;
    } while ((undefined4 *)&stack0xfffffff8 != puVar1);
    FUN_00097b96(local_1c8,uStack_1c4,uStack_1c0,uStack_1bc);
  }
  else if ((param_1 == 5) || (param_1 == 7)) {
    uRamfedfc64a = uStack_171;
  }
  else if (param_1 == 8) {
    uRamfedfc64a = uStack_171;
    puVar2 = local_1b8;
    do {
      *puVar1 = *puVar2;
      puVar2 = puVar2 + 1;
      puVar1 = puVar1 + 1;
    } while ((undefined4 *)&stack0xfffffff8 != puVar2);
    FUN_00097b96(local_1c8,uStack_1c4,uStack_1c0,uStack_1bc);
  }
  return;
}

