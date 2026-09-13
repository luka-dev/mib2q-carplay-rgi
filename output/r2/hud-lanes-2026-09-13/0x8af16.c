// FD51 G1952 original V850; VA 0x8af16; file offset 0x6af16; GP=fedff8b4 TP=28018.

void FUN_0008af16(void)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  undefined1 auStack_1b5 [3];
  byte bStack_1b2;
  undefined1 auStack_1ac [200];
  undefined1 auStack_e4 [10];
  short sStack_da;
  short sStack_d6;
  byte bStack_d2;
  undefined1 auStack_d1 [100];
  byte bStack_6d;
  undefined1 auStack_6c [100];

  uVar3 = 0;
  func_0x000b5188(auStack_1b5,0,9);
  func_0x000b5188(&bStack_d2,0,0xca);
  func_0x000b5188(0xfedfc361,0,0xca);
  func_0x000b5188(0xfedfc362,0,100);
  func_0x000b5188(0xfedfc3c7,0,100);
  func_0x000b5188(auStack_6c,0,100);
  func_0x000b5188(auStack_d1,0,100);
  bStack_6d = 0;
  bStack_d2 = 0;
  func_0x000b5188(auStack_1ac,0,200);
  func_0x00065b2a(4,auStack_1b5);
  if ((bStack_1b2 & 8) != 0) {
    func_0x00065b2a(0x13,&bStack_d2);
    iVar2 = func_0x00065b2a(8,auStack_e4);
    if (iVar2 == 1) {
      if ((sStack_da == 1) && (sStack_d6 == 1)) {
        bRamfedfc361 = bStack_d2;
        for (uVar1 = 0; uVar1 < bStack_d2; uVar1 = uVar1 + 1 & 0xff) {
          *(undefined1 *)(uVar1 + 0xfedfc362) = auStack_d1[uVar1];
        }
        bRamfedfc3c6 = bStack_6d;
        if (bStack_6d != 0) {
          for (uVar1 = 0; uVar1 < bStack_6d; uVar1 = uVar1 + 1 & 0xff) {
            *(undefined1 *)(uVar3 + 0xfedfc3c7) = auStack_6c[uVar1];
            uVar3 = uVar3 + 1 & 0xff;
          }
        }
      }
      else {
        bRamfedfc361 = 0;
        bRamfedfc3c6 = 0;
      }
    }
  }
  return;
}
