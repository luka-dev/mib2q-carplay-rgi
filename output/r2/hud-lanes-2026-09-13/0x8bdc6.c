// FD51 G1952 original V850; VA 0x8bdc6; file offset 0x6bdc6; GP=fedff8b4 TP=28018.

void FUN_0008bdc6(int param_1,byte param_2,byte *param_3,undefined4 param_4)

{
  bool bVar1;
  int iVar2;
  uint uVar3;
  byte bStack_23;
  byte bStack_22;
  undefined1 auStack_21 [9];

  bVar1 = false;
  bStack_22 = param_2;
  func_0x000b5068(auStack_21,param_4,9);
  if (*(char *)(param_1 + 4) == '\x01') {
    uVar3 = 0;
    while ((((int)((uint)bStack_22 + (uint)*param_3 + -3) < 0 && (uVar3 < *(byte *)(param_1 + 5)))
           && (!bVar1))) {
      iVar2 = func_0x0008bfbe(*(undefined1 *)(uVar3 + param_1 + 6),&bStack_23);
      if (iVar2 == 1) {
        auStack_21[bStack_23] = 1;
        *param_3 = *param_3 + 1;
      }
      else {
        *param_3 = 0;
        bVar1 = true;
      }
      uVar3 = uVar3 + 1 & 0xff;
    }
  }
  else {
    *param_3 = 0;
  }
  if (*param_3 != 0) {
    func_0x000b5068(param_4,auStack_21,9);
  }
  return;
}
