// FD51 G1952 original V850; VA 0x8c28c; file offset 0x6c28c; GP=fedff8b4 TP=28018.

void FUN_0008c28c(char param_1,int param_2,int param_3,int param_4)

{
  bool bVar1;
  uint uVar2;

  bVar1 = false;
  uVar2 = 0;
  while ((uVar2 < 3 && (!bVar1))) {
    if (*(byte *)(uVar2 + param_2) < 0x4d) {
      *(undefined2 *)(uVar2 * 2 + param_4) =
           *(undefined2 *)
            (((uint)(byte)(param_1 + *(char *)(uVar2 + param_3)) +
             (uint)*(byte *)(uVar2 + param_2) * 4) * 2 + 0x3a2e4);
    }
    else {
      bVar1 = true;
    }
    uVar2 = uVar2 + 1 & 0xff;
  }
  return;
}
