// FD51 G1952 original V850; VA 0x8bb1c; file offset 0x6bb1c; GP=fedff8b4 TP=28018.

void FUN_0008bb1c(int param_1,int param_2,uint param_3,char param_4)

{
  uint uVar1;
  uint uVar2;

  if (param_4 == '\x01') {
    for (uVar2 = 0; uVar2 < bRamfedfc42c; uVar2 = uVar2 + 1 & 0xff) {
      func_0x000b5068(uVar2 * 0x20 + param_1,uVar2 * 0x1c + param_2,0x1c);
    }
    *(undefined4 *)((bRamfedfc42c - 1) * 0x20 + param_1 + 0x1c) = 1;
  }
  else {
    uVar1 = (param_3 & 0xff) - (uint)bRamfedfc42c & 0xff;
    for (uVar2 = uVar1; uVar2 < (param_3 & 0xff); uVar2 = uVar2 + 1 & 0xff) {
      func_0x000b5068((uVar2 - uVar1) * 0x20 + param_1,uVar2 * 0x1c + param_2,0x1c);
    }
    *(undefined4 *)(param_1 + 0x1c) = 1;
  }
  return;
}
