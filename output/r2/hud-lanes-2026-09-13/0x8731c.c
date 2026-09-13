// FD51 G1952 original V850; VA 0x8731c; file offset 0x6731c; GP=fedff8b4 TP=28018.

void FUN_0008731c(undefined1 *param_1,undefined4 param_2,undefined4 param_3)

{
  byte bVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  undefined1 uVar5;
  undefined2 uVar6;
  int iVar7;
  uint uVar8;
  ushort auStack_48 [21];
  byte abStack_1e [6];

  func_0x000b5188(auStack_48 + 3,0,0x2a);
  uVar8 = 0;
  iVar7 = func_0x0008c352();
  *param_1 = 0;
  param_1[1] = 0;
  for (uVar2 = 0; iVar3 = iVar7 + -1, bVar1 = -(char)(iVar3 >> 0x1f),
      (int)(uVar2 - iVar3) < 0 != (bVar1 != 0 && bVar1 == (byte)(uVar2 - iVar3 >> 0x1f));
      uVar2 = uVar2 + 1 & 0xff) {
    uVar4 = 0;
    do {
      param_1[(uVar4 + uVar2 * 3) * 2 + 2] = 0;
      param_1[(uVar4 + uVar2 * 3) * 2 + 3] = 0;
      uVar4 = uVar4 + 1 & 0xff;
    } while (uVar4 < 3);
  }
  uVar2 = 0;
  do {
    param_1[uVar2 + 0x20] = 0;
    uVar2 = uVar2 + 1 & 0xff;
  } while (uVar2 < 3);
  if (iRamfedfc180 == 0x27) {
    func_0x00087bb0(param_2,param_3);
    iVar3 = func_0x00099a64(7,auStack_48 + 3);
    if (iVar3 == 1) {
      for (uVar2 = 0; iVar3 = iVar7 + -1, bVar1 = -(char)(iVar3 >> 0x1f),
          (int)(uVar2 - iVar3) < 0 != (bVar1 != 0 && bVar1 == (byte)(uVar2 - iVar3 >> 0x1f));
          uVar2 = uVar2 + 1 & 0xff) {
        if (abStack_1e[uVar2] < 3) {
          uVar8 = uVar8 | (uint)*(ushort *)((uint)abStack_1e[uVar2] * 2 + 0x2747c) <<
                          ((uVar2 & 0xf) << 1) & 0xffff;
        }
      }
      *param_1 = (char)uVar8;
      param_1[1] = (char)(uVar8 >> 8);
      for (uVar2 = 0; iVar3 = iVar7 + -1, bVar1 = -(char)(iVar3 >> 0x1f),
          (int)(uVar2 - iVar3) < 0 != (bVar1 != 0 && bVar1 == (byte)(uVar2 - iVar3 >> 0x1f));
          uVar2 = uVar2 + 1 & 0xff) {
        uVar8 = 0;
        do {
          uVar4 = (uint)auStack_48[uVar8 + uVar2 * 3 + 3];
          uVar6 = 0;
          if ((uVar2 == 0) && (uVar4 == 0x13b)) {
            uVar6 = 0x13b;
          }
          else if (uVar4 < 0x13b) {
            uVar6 = *(undefined2 *)(uVar4 * 2 + 0x383f6);
          }
          param_1[(uVar8 + uVar2 * 3) * 2 + 2] = (char)uVar6;
          param_1[(uVar8 + uVar2 * 3) * 2 + 3] = (char)((ushort)uVar6 >> 8);
          uVar8 = uVar8 + 1 & 0xff;
        } while (uVar8 < 3);
      }
      uVar2 = 0;
      do {
        uVar8 = (uint)auStack_48[uVar2 + iVar7 * 3];
        if (uVar8 == 0x13b) {
          uVar5 = 0x9e;
        }
        else if (uVar8 < 0x13b) {
          uVar5 = (undefined1)*(undefined2 *)(uVar8 * 2 + 0x3866c);
        }
        else {
          uVar5 = 0;
        }
        param_1[uVar2 + 0x20] = uVar5;
        uVar2 = uVar2 + 1 & 0xff;
      } while (uVar2 < 3);
    }
  }
  return;
}
