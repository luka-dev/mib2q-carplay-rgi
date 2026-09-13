// FD51 G1952 original V850; VA 0x8b21c; file offset 0x6b21c; GP=fedff8b4 TP=28018.

void FUN_0008b21c(int param_1)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  undefined4 uVar5;
  uint uVar6;
  byte bStack_1bd;
  undefined1 auStack_1bc [194];
  short sStack_fa;
  undefined1 auStack_f8 [224];

  func_0x000b5188(&sStack_fa,0,0xe2);
  func_0x000b5188(auStack_1bc,0,0xc0);
  bStack_1bd = 0;
  if ((*(byte *)(param_1 + 3) & 0x80) != 0) {
    uRamfedfc2ec = 1;
    iVar3 = func_0x00065b2a(0xb,&sStack_fa);
    if ((iVar3 == 1) && (sStack_fa == 1)) {
      uVar4 = func_0x0008b89c(auStack_1bc,auStack_f8,&bStack_1bd);
      uVar5 = func_0x0008b318(uVar4);
      uVar1 = (uint)bStack_1bd;
      for (uVar6 = 0; uVar6 < uVar4; uVar6 = uVar6 + 1 & 0xff) {
        uVar2 = uVar6 + bStack_1bd & 0xff;
        func_0x0008bbe2(auStack_1bc + uVar2 * 0x20,uVar5,uVar2 * 6 + -0x1203d38);
        func_0x0008c2fa(uVar5,(uVar4 + uVar1) - 1 & 0xff,uVar2,0xfedfc2ed);
      }
    }
  }
  return;
}
