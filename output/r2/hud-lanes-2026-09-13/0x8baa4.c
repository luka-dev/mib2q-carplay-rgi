// FD51 G1952 original V850; VA 0x8baa4; file offset 0x6baa4; GP=fedff8b4 TP=28018.

undefined4 FUN_0008baa4(int param_1,uint param_2)

{
  bool bVar1;
  uint uVar2;
  undefined4 uVar3;

  param_2 = param_2 & 0xff;
  uVar3 = 0;
  if (*(char *)(param_1 + 0x1a) == '\x05') {
    uVar3 = 0;
  }
  else if (*(char *)((param_2 - 1) * 0x1c + param_1 + 0x1a) == '\x06') {
    uVar3 = 1;
  }
  else {
    bVar1 = false;
    uVar2 = 0;
    while ((uVar2 < param_2 && (!bVar1))) {
      if (*(char *)(((param_2 - 1) - uVar2) * 0x1c + param_1 + 0x1b) == '\x02') {
        uVar3 = 0;
        bVar1 = true;
      }
      else if (*(char *)(uVar2 * 0x1c + param_1 + 0x1b) == '\x02') {
        uVar3 = 1;
        bVar1 = true;
      }
      else {
        uVar3 = 0;
      }
      uVar2 = uVar2 + 1 & 0xff;
    }
  }
  return uVar3;
}
