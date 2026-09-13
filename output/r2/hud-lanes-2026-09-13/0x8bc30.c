// FD51 G1952 original V850; VA 0x8bc30; file offset 0x6bc30; GP=fedff8b4 TP=28018.

void FUN_0008bc30(undefined4 param_1,undefined1 param_2,undefined4 param_3)

{
  uint uVar1;
  int iVar2;
  byte abStack_19 [3];
  undefined1 auStack_16 [3];
  undefined1 auStack_13 [3];

  func_0x000b5188(abStack_19,0,3);
  func_0x000b5188(auStack_16,2,3);
  func_0x000b5188(auStack_13,0x4d,3);
  uVar1 = func_0x0008bea6(param_1,abStack_19,auStack_16);
  if (uVar1 != 0) {
    if (uVar1 == 1) {
      if (abStack_19[0] < 9) {
        auStack_13[0] = *(undefined1 *)(abStack_19[0] + 0x27e1a);
        func_0x0008c28c(param_2,auStack_13,auStack_16,param_3);
      }
    }
    else if (uVar1 < 3) {
      iVar2 = func_0x0008bfec(abStack_19,auStack_13);
      if (iVar2 == 1) {
        func_0x0008c19e(auStack_13,auStack_16);
        func_0x0008c28c(param_2,auStack_13,auStack_16,param_3);
      }
    }
    else if ((uVar1 == 3) && (iVar2 = func_0x0008c078(abStack_19,auStack_13), iVar2 == 1)) {
      func_0x0008c19e(auStack_13,auStack_16);
      func_0x0008c28c(param_2,auStack_13,auStack_16,param_3);
    }
  }
  return;
}
