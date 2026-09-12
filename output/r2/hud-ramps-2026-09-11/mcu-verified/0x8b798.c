// Original V850 FD51 G1952; linked VA 0x8b798; file offset 0x6b798; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008b798(int param_1,int param_2,uint param_3)

{
  byte bVar1;
  bool bVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  undefined4 uVar6;
  
  param_3 = param_3 & 0xff;
  if (param_3 == 0) {
    param_3 = 1;
  }
  for (uVar5 = 0; uVar4 = param_3 - 1,
      (int)(uVar4 - uVar5) < 0 == ((int)uVar4 < 0 && -1 < (int)(uVar4 - uVar5)) && uVar4 != uVar5;
      uVar5 = uVar5 + 1 & 0xff) {
    if (*(byte *)(uVar5 + 1 + param_2) < *(byte *)(uVar5 + param_2)) {
      uVar6 = *(undefined4 *)((uVar5 + 1) * 4 + param_1);
      bVar1 = *(byte *)(uVar5 + 1 + param_2);
      bVar2 = false;
      uVar4 = 0;
      while (((int)(uVar4 - (uVar5 + 1)) < 0 && (!bVar2))) {
        uVar3 = (uVar5 - uVar4) + 1 & 0xff;
        *(undefined4 *)(uVar3 * 4 + param_1) = *(undefined4 *)((uVar3 - 1) * 4 + param_1);
        *(undefined1 *)(uVar3 + param_2) = *(undefined1 *)((uVar3 - 1) + param_2);
        if ((uVar3 < 2) || (*(byte *)((uVar3 - 2) + param_2) <= bVar1)) {
          *(undefined4 *)((uVar3 - 1) * 4 + param_1) = uVar6;
          *(byte *)((uVar3 - 1) + param_2) = bVar1;
          bVar2 = true;
        }
        uVar4 = uVar4 + 1 & 0xff;
      }
    }
  }
  return;
}

