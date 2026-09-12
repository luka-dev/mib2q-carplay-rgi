// Original V850 FD51 G1952; linked VA 0x97b96; file offset 0x77b96; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_00097b96(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined1 uStack_f;
  undefined1 uStack_e;
  undefined4 uStack_8;
  undefined4 uStack_4;
  
  uStack_f = (undefined1)((uint)param_1 >> 8);
  uRamfedfc649 = uStack_f;
  uStack_e = (undefined1)((uint)param_1 >> 0x10);
  uRamfedfc648 = uStack_e;
  uRamfedfc600 = param_2;
  uStack_8 = param_3;
  uStack_4 = param_4;
  thunk_FUN_000b507a(0xfedfc604,&uStack_8,0x44);
  return;
}

