// FD51 G1952 original V850; VA 0x8bf8a; file offset 0x6bf8a; GP=fedff8b4 TP=28018.

int FUN_0008bf8a(short param_1,undefined1 *param_2)

{
  int iVar1;
  uint uVar2;

  iVar1 = 0;
  uVar2 = 0;
  while ((uVar2 < 9 && (iVar1 == 0))) {
    if (*(short *)(uVar2 * 2 + 0x27dec) == param_1) {
      *param_2 = (char)uVar2;
      iVar1 = 1;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return iVar1;
}
