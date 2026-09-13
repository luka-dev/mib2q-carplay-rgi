// FD51 G1952 original V850; VA 0x8b0e0; file offset 0x6b0e0; GP=fedff8b4 TP=28018.

void FUN_0008b0e0(int param_1)

{
  uint uVar1;
  int iVar2;
  short asStack_30 [12];
  char acStack_18 [4];
  undefined4 uStack_14;
  ushort uStack_10;
  short sStack_e;
  byte bStack_c;
  ushort uStack_a;

  func_0x000b5188(acStack_18,0,0x10);
  func_0x00065b2a(9,asStack_30);
  if ((*(byte *)(param_1 + 2) & 0x20) != 0) {
    uRamfedfc2c0 = 1;
    iVar2 = func_0x00065b2a(8,acStack_18);
    if ((iVar2 == 1) && (acStack_18[0] == '\x01')) {
      if (((uStack_a & 1) != 0) && ((sStack_e != 1 && (asStack_30[0] == 0)))) {
        uRamfedfc2c1 = 1;
        uRamfedfc2c4 = uStack_14;
        if (uStack_10 < 3) {
          if (uStack_10 == 0) {
            uRamfedfc2c2 = 0;
          }
          else if (uStack_10 == 1) {
            uRamfedfc2c2 = 1;
          }
          else {
            uRamfedfc2c2 = 2;
          }
        }
        else if (uStack_10 < 4) {
          uRamfedfc2c2 = 3;
        }
        else if (uStack_10 == 4) {
          uRamfedfc2c2 = 4;
        }
        else if (uStack_10 == 5) {
          uRamfedfc2c2 = 5;
        }
        else {
          uRamfedfc2c2 = 6;
        }
      }
      if ((((uStack_a & 1) != 0) && (sStack_e == 1)) && (bStack_c < 0x65)) {
        uVar1 = 0;
        while ((uVar1 < 0x11 && (cRamfedfc2c3 == -1))) {
          if (bStack_c <= *(byte *)(uVar1 * 2 + 0x27dca)) {
            cRamfedfc2c3 = *(char *)(uVar1 * 2 + 0x27dcb);
          }
          uVar1 = uVar1 + 1 & 0xff;
        }
      }
    }
  }
  return;
}
