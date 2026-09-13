// FD51 G1952 original V850; VA 0x8bea6; file offset 0x6bea6; GP=fedff8b4 TP=28018.

uint FUN_0008bea6(undefined4 param_1,int param_2,int param_3)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  char cStack_2c;
  char cStack_2b;
  undefined1 auStack_2a [9];
  char acStack_21 [9];

  func_0x000b5188(acStack_21,0,9);
  func_0x000b5188(auStack_2a,2,9);
  cStack_2c = '\0';
  cStack_2b = '\0';
  uVar3 = 0;
  iVar2 = func_0x0008bd30(param_1,&cStack_2c,acStack_21,auStack_2a);
  if (iVar2 == 0) {
    func_0x0008bdc6(param_1,cStack_2c,&cStack_2b,acStack_21);
  }
  if (((byte)(cStack_2b + cStack_2c) != 0) && (iVar2 == 0)) {
    uVar1 = 0;
    while ((uVar1 < 9 && (uVar3 < (byte)(cStack_2b + cStack_2c)))) {
      if (acStack_21[uVar1] == '\x01') {
        *(char *)(uVar3 + param_2) = (char)uVar1;
        *(undefined1 *)(uVar3 + param_3) = auStack_2a[uVar1];
        uVar3 = uVar3 + 1 & 0xff;
      }
      uVar1 = uVar1 + 1 & 0xff;
    }
  }
  return uVar3;
}
