// FD51 G1952 original V850; VA 0x95d10; file offset 0x75d10; GP=fedff8b4 TP=28018.

undefined4 FUN_00095d10(int param_1)

{
  byte bVar1;
  bool bVar2;
  bool bVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  undefined4 uVar7;
  undefined1 auStack_1d8 [92];
  short asStack_17c [18];
  char cStack_158;
  char acStack_157 [319];

  uVar7 = 0;
  bVar2 = false;
  bVar3 = false;
  uVar6 = FUN_0008c352();
  if ((((*(byte *)(param_1 + 8) & 2) != 0) && ((*(byte *)(param_1 + 6) & 0x20) != 0)) &&
     (func_0x0008c340(auStack_1d8), cStack_158 == '\x01')) {
    uVar5 = 0;
    while (((uVar5 < uVar6 && (!bVar2)) && (!bVar3))) {
      if (asStack_17c[uVar5 * 3] != 0) {
        bVar2 = true;
      }
      iVar4 = uVar6 - 1;
      bVar1 = -(char)(iVar4 >> 0x1f);
      if (((int)(uVar5 - iVar4) < 0 != (bVar1 != 0 && bVar1 == (byte)(uVar5 - iVar4 >> 0x1f))) &&
         (acStack_157[uVar5] != '\0')) {
        bVar3 = true;
      }
      uVar5 = uVar5 + 1 & 0xff;
    }
    if ((bVar2) || (bVar3)) {
      uVar7 = 0x27;
    }
  }
  return uVar7;
}
