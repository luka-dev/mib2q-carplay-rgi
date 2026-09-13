// FD51 G1952 original V850; VA 0x8c19e; file offset 0x6c19e; GP=fedff8b4 TP=28018.

void FUN_0008c19e(int param_1,int param_2)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  undefined1 auStack_1a [6];

  func_0x000b5188(auStack_1a,0x4d,3);
  func_0x000b5188(auStack_1a + 3,0x4d,3);
  bVar1 = false;
  uVar4 = 0;
  uVar3 = 0;
  uVar2 = 0;
  while ((uVar2 < 3 && (!bVar1))) {
    if (*(byte *)(uVar2 + param_1) < 0x4d) {
      if (*(char *)(uVar2 + param_2) == '\0') {
        auStack_1a[uVar4] = *(undefined1 *)(uVar2 + param_1);
        uVar4 = uVar4 + 1 & 0xff;
      }
      else {
        auStack_1a[uVar3 + 3] = *(undefined1 *)(uVar2 + param_1);
        uVar3 = uVar3 + 1 & 0xff;
      }
    }
    else {
      bVar1 = true;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  func_0x000b5068(param_1,auStack_1a,uVar4);
  func_0x000b5188(param_2,0,uVar4);
  func_0x000b5068(uVar4 + param_1,auStack_1a + 3,uVar3);
  func_0x000b5188(uVar4 + param_2,2,uVar3);
  return;
}
