// FD51 G1952 original V850; VA 0x8ae78; file offset 0x6ae78; GP=fedff8b4 TP=28018.

void FUN_0008ae78(int param_1)

{
  int iVar1;
  char cStack_f;
  undefined2 uStack_e;
  undefined2 uStack_c;
  undefined2 uStack_a;

  func_0x000b5188(&uStack_e,0,6);
  cStack_f = -1;
  if ((*(byte *)(param_1 + 2) & 1) != 0) {
    uRamfedfc2f4 = 1;
    iVar1 = func_0x00065b2a(10,&uStack_e);
    if ((iVar1 == 1) && (func_0x0008b370(&cStack_f,uStack_e), cStack_f != -1)) {
      func_0x0008b41c(0xfedfc2f8,cStack_f,uStack_c);
      uRamfedfc2f7 = func_0x0008b896(&uStack_e);
      if (iRamfedfc2f8 != 0) {
        func_0x0008b4de(0xfedfc2f5,cStack_f,uStack_a);
      }
    }
  }
  return;
}
