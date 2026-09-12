// Original V850 FD51 G1952; linked VA 0x5ec3c; file offset 0x3ec3c; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0005ec3c(int param_1)

{
  int iVar1;
  undefined2 uStack_46;
  undefined4 uStack_44;
  byte bStack_40;
  byte local_3f;
  byte local_3e;
  byte local_3d;
  byte bStack_3c;
  byte bStack_3b;
  byte bStack_3a;
  
  uStack_46 = 0x17;
  if ((*(byte *)(param_1 + 2) & 1) == 0) {
    iVar1 = FUN_000a5ed6(0x32,0x17,&uStack_44,&uStack_46);
    if (iVar1 == 0) {
      thunk_FUN_000b507a(&bStack_40,uStack_44,0x3c);
      uRamfedfadd4 = (ushort)bStack_40;
      uRamfedfadd6 = (ushort)local_3f;
      uRamfedfadd8 = (ushort)local_3e;
      uRamfedfadda = 1;
      bRamfedfaddb = local_3d;
      FUN_000b5188(0xfedfaddc,0,0x10);
      if ((bRamfedfaddb != 0) && (bRamfedfaddb < 0x11)) {
        thunk_FUN_000b507a(0xfedfaddc,&bStack_3c,bRamfedfaddb);
      }
      if ((((uRamfedfadd4 == 0xb) && (uRamfedfadd6 == 0)) && (bRamfedfaddb == 0)) &&
         (uRamfedfadd8 == 0)) {
        uRamfedfadd4 = 0;
        uRamfedfadd6 = 0;
        uRamfedfadd8 = 0;
        uRamfedfadda = 0;
        bRamfedfaddb = 0;
        uRamfedfae36 = (ushort)bStack_3c;
        uRamfedfae38 = (ushort)bStack_3b;
        uRamfedfae3a = (ushort)bStack_3a;
      }
      else {
        uRamfedfae36 = 0;
      }
    }
    else {
      FUN_0005fa7e();
    }
  }
  return;
}

