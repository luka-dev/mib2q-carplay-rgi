// Original V850 FD51 G1952; linked VA 0x8adac; file offset 0x6adac; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008adac(int param_1)

{
  int iVar1;
  char cStack_21;
  undefined2 local_20;
  undefined2 uStack_1e;
  undefined2 uStack_1c;
  undefined1 uStack_1a;
  undefined1 uStack_19;
  undefined1 auStack_18 [16];
  
  FUN_000b5188(&local_20,0,0x18);
  cStack_21 = -1;
  if ((*(byte *)(param_1 + 2) & 1) != 0) {
    uRamfedfc26c = 1;
    iVar1 = FUN_00065b2a(9,&local_20);
    if (((iVar1 == 1) && (FUN_0008b32e(&cStack_21,local_20), cStack_21 != -1)) &&
       (FUN_0008b3b2(0xfedfc270,cStack_21,uStack_1e), iRamfedfc270 != 0)) {
      FUN_0008b486(0xfedfc26d,cStack_21,uStack_1c);
      FUN_0008b536(0xfedfc26e,0xfedfc274,cStack_21,uStack_1a,auStack_18,uStack_19,uStack_1e);
      uRamfedfc26f = FUN_0008b86e(&local_20);
    }
  }
  return;
}

