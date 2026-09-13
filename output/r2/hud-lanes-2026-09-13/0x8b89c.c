// FD51 G1952 original V850; VA 0x8b89c; file offset 0x6b89c; GP=fedff8b4 TP=28018.

byte FUN_0008b89c(undefined4 param_1,int param_2,undefined1 *param_3)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  byte bStack_22;
  char cStack_21;
  undefined1 *puStack_20;
  undefined4 uStack_1c;

  cStack_21 = '\0';
  bStack_22 = 0;
  uVar6 = 0;
  uVar3 = 8;
  uVar5 = 8;
  puStack_20 = param_3;
  uStack_1c = param_1;
  while (((uVar6 < 8 && (bStack_22 < 8)) && (cStack_21 == '\0'))) {
    uVar1 = uVar3;
    if (uVar3 == 8) {
      uVar1 = 7 - uVar6 & 0xff;
      iVar2 = func_0x0008b9a6(uVar1 * 0x1c + param_2);
      if (iVar2 == 0) {
        bStack_22 = bStack_22 + 1;
        uVar1 = uVar3;
      }
    }
    uVar4 = uVar5;
    if ((uVar5 == 8) && (iVar2 = func_0x0008b9a6(uVar6 * 0x1c + param_2), uVar4 = uVar6, iVar2 == 0)
       ) {
      bStack_22 = bStack_22 + 1;
      uVar4 = uVar5;
    }
    if ((uVar1 < 8) && (uVar4 < 8)) {
      cStack_21 = '\x01';
    }
    uVar6 = uVar6 + 1 & 0xff;
    uVar3 = uVar1;
    uVar5 = uVar4;
  }
  bStack_22 = 8 - bStack_22;
  if (bStack_22 != 0) {
    *puStack_20 = (char)uVar5;
    func_0x0008b9b6(uStack_1c,param_2,&bStack_22,puStack_20);
  }
  return bStack_22;
}
