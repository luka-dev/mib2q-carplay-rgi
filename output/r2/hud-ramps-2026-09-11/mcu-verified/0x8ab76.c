// Original V850 FD51 G1952; linked VA 0x8ab76; file offset 0x6ab76; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008ab76(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  char cStack_19;
  short sStack_18;
  char cStack_15;
  undefined1 uStack_14;
  byte local_13;
  byte local_12;
  
  FUN_0008ada4();
  FUN_000b5188(&cStack_15,0,9);
  FUN_000b5188(&sStack_18,0,2);
  FUN_000b5188(&cStack_19,0,1);
  iVar1 = FUN_00065b2a(4,&cStack_15);
  iVar2 = FUN_00065b2a(7,&sStack_18);
  iVar3 = FUN_00065b2a(6,&cStack_19);
  if (((((iVar1 == 1) && (iVar2 == 1)) && (iVar3 == 1)) &&
      ((cStack_15 == '\x01' && ((local_12 & 0x40) != 0)))) &&
     (((local_13 & 1) != 0 && ((sStack_18 == 1 && (cStack_19 == '\0')))))) {
    uVar4 = 0;
    do {
      (**(code **)(uVar4 * 8 + 0x27940))();
      (**(code **)(uVar4 * 8 + 0x27944))(&uStack_14);
      uVar4 = uVar4 + 1 & 0xff;
    } while (uVar4 < 7);
  }
  else {
    uVar4 = 0;
    do {
      (**(code **)(uVar4 * 8 + 0x27940))();
      uVar4 = uVar4 + 1 & 0xff;
    } while (uVar4 < 7);
  }
  return;
}

