// FD51 G1952 original V850; VA 0x8b06a; file offset 0x6b06a; GP=fedff8b4 TP=28018.

void FUN_0008b06a(int param_1)

{
  int iVar1;
  ushort auStack_10 [2];
  uint uStack_c;

  func_0x000b5188(auStack_10,0,8);
  if ((*(byte *)(param_1 + 6) & 0x40) != 0) {
    uRamfedfc2b8 = 1;
    iVar1 = func_0x00065b2a(0xc,auStack_10);
    if (((iVar1 == 1) && (uStack_c < 0x10000)) && (auStack_10[0] < 0x100)) {
      uRamfedfc2b9 = 1;
      uRamfedfc2bc = (undefined2)uStack_c;
      uRamfedfc2ba = (undefined1)auStack_10[0];
    }
  }
  return;
}
