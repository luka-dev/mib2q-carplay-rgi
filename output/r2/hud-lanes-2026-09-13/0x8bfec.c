// FD51 G1952 original V850; VA 0x8bfec; file offset 0x6bfec; GP=fedff8b4 TP=28018.

int FUN_0008bfec(byte *param_1,undefined1 *param_2)

{
  int iVar1;
  ushort uVar2;

  iVar1 = 0;
  uVar2 = 0;
  while ((uVar2 < 8 && (iVar1 == 0))) {
    if (*(byte *)((short)(uVar2 + (ushort)*param_1 * 8) * 3 + 0x27f0c) == param_1[1]) {
      *param_2 = *(undefined1 *)((short)(uVar2 + (ushort)*param_1 * 8) * 3 + 0x27f0d);
      param_2[1] = *(undefined1 *)((short)(uVar2 + (ushort)*param_1 * 8) * 3 + 0x27f0e);
      iVar1 = 1;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return iVar1;
}
