// FD51 G1952 original V850; VA 0x8c078; file offset 0x6c078; GP=fedff8b4 TP=28018.

int FUN_0008c078(byte *param_1,undefined1 *param_2)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  int iVar4;

  bVar1 = false;
  iVar4 = 0;
  uVar3 = 0;
  while ((uVar3 < 7 && (!bVar1))) {
    if (*(byte *)((uVar3 + (uint)*param_1 * 7) * 8 + 0x27fe4) == param_1[1]) {
      uVar2 = 0;
      while ((uVar2 < *(byte *)((uVar3 + (uint)*param_1 * 7) * 8 + 0x27fe5) && (iVar4 == 0))) {
        if (*(byte *)(uVar2 * 4 + *(int *)((uVar3 + (uint)*param_1 * 7) * 8 + 0x27fe8)) ==
            param_1[2]) {
          *param_2 = *(undefined1 *)
                      (uVar2 * 4 + *(int *)((uVar3 + (uint)*param_1 * 7) * 8 + 0x27fe8) + 1);
          param_2[1] = *(undefined1 *)
                        (uVar2 * 4 + *(int *)((uVar3 + (uint)*param_1 * 7) * 8 + 0x27fe8) + 2);
          param_2[2] = *(undefined1 *)
                        (uVar2 * 4 + *(int *)((uVar3 + (uint)*param_1 * 7) * 8 + 0x27fe8) + 3);
          iVar4 = 1;
        }
        uVar2 = uVar2 + 1 & 0xff;
      }
      bVar1 = true;
    }
    uVar3 = uVar3 + 1 & 0xff;
  }
  return iVar4;
}
