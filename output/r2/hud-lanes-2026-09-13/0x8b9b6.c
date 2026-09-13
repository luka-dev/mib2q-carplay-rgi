// FD51 G1952 original V850; VA 0x8b9b6; file offset 0x6b9b6; GP=fedff8b4 TP=28018.

void FUN_0008b9b6(int param_1,int param_2,byte *param_3,byte *param_4)

{
  byte bVar1;
  undefined4 uVar2;
  uint uVar3;
  uint uVar4;

  uVar4 = (uint)*param_4;
  if (bRamfedfc42c < *param_3) {
    uVar2 = func_0x0008baa4(uVar4 * 0x1c + param_2,*param_3);
    func_0x0008bb1c(param_1,uVar4 * 0x1c + param_2,*param_3,uVar2);
    *param_3 = bRamfedfc42c;
    *param_4 = 0;
  }
  else {
    FUN_0008b318(*param_3);
    *param_4 = (byte)((int)((uint)bRamfedfc42c - (uint)*param_3) / 2);
    uVar3 = (uint)*param_4;
    for (bVar1 = 0; bVar1 < *param_3; bVar1 = bVar1 + 1) {
      func_0x000b5068(uVar3 * 0x20 + param_1,uVar4 * 0x1c + param_2,0x1c);
      uVar4 = uVar4 + 1 & 0xff;
      uVar3 = uVar3 + 1 & 0xff;
    }
  }
  return;
}
