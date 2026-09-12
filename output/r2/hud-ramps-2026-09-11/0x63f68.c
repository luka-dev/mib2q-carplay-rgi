
void FUN_00063f68(char *param_1)

{
  byte bVar1;
  byte bVar2;
  bool bVar3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  undefined1 uVar10;
  char cVar11;
  byte bStack_9f;
  char cStack_9d;
  char cStack_9c;
  byte bStack_9b;
  byte bStack_9a;
  byte bStack_99;
  byte bStack_98;
  undefined1 local_97;
  byte bStack_96;
  char cStack_95;
  byte local_94;
  byte local_93;
  char local_92;
  byte local_91 [56];
  char cStack_59;
  char cStack_58;
  byte bStack_57;
  byte bStack_56;
  byte bStack_55;
  char local_54;
  byte local_53;
  byte local_52;
  char local_51;
  byte local_50 [56];
  
  uVar9 = 0;
  uVar10 = 0;
  FUN_000b5188(&cStack_59,0,0x41);
  FUN_000b5188(&cStack_9d,0,0x44);
  FUN_000b5188(0xfedf28dc,0,0x44);
  cStack_9d = *param_1;
  cStack_59 = cStack_9d;
  cStack_9c = param_1[1];
  cStack_58 = cStack_9c;
  bStack_9b = param_1[2];
  bStack_57 = bStack_9b;
  bVar1 = param_1[3];
  bStack_99 = (byte)(((int)(uint)(byte)param_1[3] >> 4) << 4);
  bStack_56 = bVar1 & 0xf | bStack_99;
  bVar2 = param_1[4];
  uVar4 = ((int)(uint)(byte)param_1[4] >> 4) << 4;
  bStack_55 = bVar2 & 0xf | (byte)uVar4;
  if ((uVar4 & 0x80) == 0) {
    local_54 = param_1[5];
    bStack_9f = param_1[6];
    local_53 = bStack_9f;
    uVar4 = 7;
  }
  else {
    local_54 = param_1[5];
    local_53 = param_1[6];
    bStack_9f = param_1[7];
    local_52 = bStack_9f;
    local_51 = param_1[8];
    uVar4 = 9;
  }
  if (local_54 == '\0') {
    uRamfedf2e48 = 0;
    uRamfedf2e44 = 0;
    bRamfedf2e17 = 0xff;
    bRamfedf2e16 = 0xff;
  }
  for (uVar5 = 0; uVar5 < bStack_9f; uVar5 = uVar5 + 1 & 0xff) {
    local_50[uVar5 * 0xe] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 1] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 2] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 3] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 4] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 5] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 6] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 7] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 8] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 9] = param_1[uVar4];
    uVar4 = uVar4 + 1 & 0xff;
    local_50[uVar5 * 0xe + 10] = param_1[uVar4];
    uVar4 = local_50[uVar5 * 0xe + 6] + uVar4 & 0xff;
  }
  if ((((cStack_9d == '\x02') && (cStack_9c == '\x01')) && ((bVar2 & 0xf) == 3)) &&
     ((bStack_9b != 0 && (bStack_9b < 8)))) {
    bStack_9a = 3;
    bStack_99 = bVar1 & 0xf | bStack_99;
    FUN_0008fb28(0xfedf0294,0xfedf042c,0xfedf0360,0xfedf2e4c,bStack_9b,uVar10);
    bStack_98 = bStack_9f;
    local_97 = 0;
    bStack_96 = bStack_55;
    if ((bStack_55 & 0x80) == 0) {
      uVar4 = 10;
    }
    else {
      local_93 = local_52;
      local_92 = local_51;
      uVar4 = 0xc;
    }
    local_94 = local_53;
    cStack_95 = local_54;
    thunk_FUN_000b507a(0xfedf28dc,&cStack_9d,uVar4);
    for (uVar5 = 0; uVar5 < bRamfedf2e4f; uVar5 = uVar5 + 1 & 0xff) {
      uVar8 = 0;
      do {
        if (*(int *)(uVar8 * 8 + 0x24518) == *(int *)(uVar5 * 0xc + -0x120fbd4)) {
          *(bool *)(uVar5 * 0xc + -0x120fbcd) =
               (*(byte *)(uVar8 * 8 + 0x2451c) &
               *(byte *)(*(byte *)(uVar8 * 8 + 0x2451d) + 0xfedf2e4c)) ==
               *(byte *)(uVar8 * 8 + 0x2451c);
        }
        uVar8 = uVar8 + 1 & 0xff;
      } while (uVar8 < 10);
    }
    cVar11 = '\n';
    for (uVar5 = 0; uVar5 < bStack_9f; uVar5 = uVar5 + 1 & 0xff) {
      bVar3 = false;
      uVar8 = 0;
      while ((uVar8 < bRamfedf2e4f && (!bVar3))) {
        if (*(int *)(uVar8 * 0xc + -0x120fbd4) ==
            (uint)local_50[uVar5 * 0xe + 3] * 0x1000000 +
            (uint)local_50[uVar5 * 0xe + 2] * 0x10000 +
            (uint)local_50[uVar5 * 0xe + 1] * 0x100 + (uint)local_50[uVar5 * 0xe]) {
          local_91[uVar9 * 0xe] = local_50[uVar5 * 0xe];
          local_91[uVar9 * 0xe + 1] = local_50[uVar5 * 0xe + 1];
          local_91[uVar9 * 0xe + 2] = local_50[uVar5 * 0xe + 2];
          local_91[uVar9 * 0xe + 3] = local_50[uVar5 * 0xe + 3];
          local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 0x10;
          local_91[uVar9 * 0xe + 4] = local_50[uVar5 * 0xe + 4];
          if (local_50[uVar5 * 0xe + 4] == 0) {
            local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 0x20;
            if ((local_50[uVar5 * 0xe + 5] & 1) == 0) {
              local_91[uVar9 * 0xe + 6] = local_50[uVar5 * 0xe + 6];
              local_91[uVar9 * 0xe + 7] = 0;
              local_91[uVar9 * 0xe + 8] = 0;
              local_91[uVar9 * 0xe + 9] = 0;
              if (local_50[uVar5 * 0xe + 6] < 2) {
                local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 0x40;
                iVar6 = FUN_0006523e(*(undefined4 *)(uVar8 * 0xc + -0x120fbd4),bStack_9b,
                                     local_50[uVar5 * 0xe + 10]);
                if (iVar6 == 1) {
                  iVar6 = FUN_00064cb0(*(undefined4 *)(uVar8 * 0xc + -0x120fbd4));
                  if (iVar6 == 2) {
                    bRamfedf2e17 = local_50[uVar5 * 0xe + 10];
                  }
                  if (iVar6 == 9) {
                    bRamfedf2e16 = local_50[uVar5 * 0xe + 10];
                  }
                  if (((local_91[uVar9 * 0xe + 5] & 0x20) != 0) &&
                     ((local_91[uVar9 * 0xe + 5] & 0x40) != 0)) {
                    uVar7 = FUN_00091430();
                    if (uVar7 == bStack_57) {
                      *(byte *)(uVar8 * 0xc + -0x120fbcd) = local_50[uVar5 * 0xe + 10];
                    }
                    local_91[uVar9 * 0xe + 10] = local_50[uVar5 * 0xe + 10];
                  }
                  local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 0x80;
                }
                else {
                  FUN_0006554c(*(undefined4 *)(uVar8 * 0xc + -0x120fbd4),bStack_57);
                  local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 1;
                  local_91[uVar9 * 0xe + 6] = 0;
                  local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] & 0x7f;
                  bRamfedf28df = bStack_9a & 0xfc;
                  bStack_9a = bRamfedf28df;
                }
              }
              else {
                local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] & 0xbf;
                bRamfedf28df = bStack_9a & 0xfc;
                bStack_9a = bRamfedf28df;
                FUN_0006554c(*(undefined4 *)(uVar8 * 0xc + -0x120fbd4),bStack_57);
                local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 1;
                local_91[uVar9 * 0xe + 6] = 0;
              }
            }
            else {
              FUN_0006554c(*(undefined4 *)(uVar8 * 0xc + -0x120fbd4),bStack_57);
              local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] | 0xc1;
              local_91[uVar9 * 0xe + 6] = 0;
            }
          }
          else {
            local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] & 0xdf;
            bRamfedf28df = bStack_9a & 0xfc;
            bStack_9a = bRamfedf28df;
          }
          if (local_91[uVar9 * 0xe + 6] == 1) {
            thunk_FUN_000b507a(uVar4 + 0xfedf28dc,local_91 + uVar9 * 0xe,0xb);
            uVar4 = uVar4 + 0xb;
            cVar11 = cVar11 + '\v';
          }
          else {
            thunk_FUN_000b507a(uVar4 + 0xfedf28dc,local_91 + uVar9 * 0xe,10);
            uVar4 = uVar4 + 10;
            cVar11 = cVar11 + '\n';
          }
          uVar4 = uVar4 & 0xff;
          bVar3 = true;
          FUN_00064c48(*(undefined4 *)(uVar8 * 0xc + -0x120fbd4),bStack_9b);
          FUN_0009144c(0,1,0xff);
          uVar9 = uVar9 + 1 & 0xff;
        }
        else {
          local_91[uVar9 * 0xe + 5] = local_91[uVar9 * 0xe + 5] & 0xef;
        }
        uVar8 = uVar8 + 1 & 0xff;
      }
      if (!bVar3) {
        bRamfedf28df = bStack_9a & 0xfc;
        local_91[uVar9 * 0xe] = local_50[uVar5 * 0xe];
        local_91[uVar9 * 0xe + 1] = local_50[uVar5 * 0xe + 1];
        local_91[uVar9 * 0xe + 2] = local_50[uVar5 * 0xe + 2];
        local_91[uVar9 * 0xe + 3] = local_50[uVar5 * 0xe + 3];
        local_91[uVar9 * 0xe + 4] = 0;
        local_91[uVar9 * 0xe + 6] = 0;
        local_91[uVar9 * 0xe + 7] = 0;
        local_91[uVar9 * 0xe + 8] = 0;
        local_91[uVar9 * 0xe + 9] = 0;
        bStack_9a = bRamfedf28df;
        thunk_FUN_000b507a(uVar4 + 0xfedf28dc,local_91 + uVar9 * 0xe,10);
        uVar4 = uVar4 + 10 & 0xff;
        cVar11 = cVar11 + '\n';
        uVar9 = uVar9 + 1 & 0xff;
      }
    }
    FUN_00066134(0xfedf28dc,cVar11);
    bRamfedf2e1b = bStack_57;
    uRamfedf2e1a = 0;
  }
  return;
}


