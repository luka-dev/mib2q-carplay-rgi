
/* WARNING: Heritage AFTER dead removal. Example location: r0xfedf035b : 0x00063734 */
/* WARNING: Restarted to delay deadcode elimination for space: ram */

void FUN_0006273c(int param_1)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  byte bVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  byte bStack_66;
  char cStack_65;
  char cStack_64;
  undefined1 uStack_63;
  undefined1 uStack_62;
  undefined1 uStack_61;
  byte bStack_60;
  undefined1 local_5f;
  byte bStack_5e;
  byte bStack_5d;
  byte local_5c;
  byte local_5b;
  undefined1 local_5a;
  byte local_59 [10];
  byte abStack_4f [46];
  char cStack_21;
  char cStack_20;
  undefined1 uStack_1f;
  undefined1 uStack_1e;
  byte bStack_1d;
  byte bStack_1c;
  byte local_1b;
  byte local_1a;
  undefined1 local_19;
  
  uVar8 = 0;
  uVar6 = 0;
  FUN_000b5188(&cStack_21,0,9);
  FUN_000b5188(&cStack_65,0,0x44);
  FUN_000b5188(0xfedf28dc,0,0x44);
  thunk_FUN_000b507a(&cStack_21,param_1,9);
  if (cStack_21 == '\x01') {
    cStack_65 = cStack_21;
    cStack_64 = cStack_20;
    uStack_63 = uStack_1f;
    uStack_62 = 1;
    uStack_61 = uStack_1e;
    if (cRamfedf04fc == '\0') {
      FUN_0008fb28(0xfedf0294,0xfedf042c,0xfedf0360,0xfedf2e4c,uStack_1f);
      bRamfedf04ff = bRamfedf2e4f;
      bRamfedf04fd = 0;
      if (bRamfedf2e4f < 5) {
        bRamfedf04fe = bRamfedf2e4f;
      }
      else {
        bRamfedf04fe = 4;
      }
      cRamfedf04fc = '\x01';
    }
    else {
      bStack_66 = local_1b;
      if ((bStack_1d & 0x80) != 0) {
        bStack_66 = local_1a;
      }
      if ((bStack_1c != bRamfedf04fd) || (bStack_66 != bRamfedf04fe)) {
        bRamfedf04fd = 0;
        if (bRamfedf04ff < 5) {
          bRamfedf04fe = bRamfedf04ff;
        }
        else {
          bRamfedf04fe = 4;
        }
      }
    }
    bStack_60 = bRamfedf04ff;
    local_5f = 0;
    if ((bStack_1d & 0xf) == 0) {
      bStack_1d = bStack_1d & 0xf0 | 1;
    }
    bStack_5e = bStack_1d;
    if ((bStack_1d & 0x80) == 0) {
      local_5c = bRamfedf04fe;
      uVar8 = 10;
    }
    else {
      local_5c = 0;
      local_5b = bRamfedf04fe;
      local_5a = 0;
      uVar8 = 0xc;
    }
    bStack_5d = bRamfedf04fd;
    thunk_FUN_000b507a(0xfedf28dc,&cStack_65,uVar8);
    uVar7 = 0;
    for (uVar2 = (uint)bRamfedf04fd; (int)(uVar2 - ((uint)bRamfedf04fe + (uint)bRamfedf04fd)) < 0;
        uVar2 = uVar2 + 1 & 0xff) {
      local_59[uVar7 * 0xe + 5] = 0xf0;
      if ((bStack_1d & 0xf) == 1) {
        local_59[uVar7 * 0xe] = *(byte *)(uVar2 * 0xc + -0x120fbd4);
        local_59[uVar7 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 8);
        local_59[uVar7 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 0x10);
        local_59[uVar7 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 0x18);
        local_59[uVar7 * 0xe + 4] = *(byte *)(uVar2 * 0xc + -0x120fbd0);
        thunk_FUN_000b507a(uVar8 + 0xfedf28dc,local_59 + uVar7 * 0xe,5);
        uVar8 = uVar8 + 5 & 0xff;
        uVar6 = uVar6 + 5 & 0xff;
      }
      else if ((bStack_1d & 0xf) == 2) {
        local_59[uVar7 * 0xe] = *(byte *)(uVar2 * 0xc + -0x120fbd4);
        local_59[uVar7 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 8);
        local_59[uVar7 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 0x10);
        local_59[uVar7 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 0x18);
        local_59[uVar7 * 0xe + 4] = *(byte *)(uVar2 * 0xc + -0x120fbd0);
        thunk_FUN_000b507a(uVar8 + 0xfedf28dc,local_59 + uVar7 * 0xe,6);
        uVar8 = uVar8 + 6 & 0xff;
        uVar6 = uVar6 + 6 & 0xff;
      }
      else if ((bStack_1d & 0xf) == 3) {
        local_59[uVar7 * 0xe] = *(byte *)(uVar2 * 0xc + -0x120fbd4);
        local_59[uVar7 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 8);
        local_59[uVar7 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 0x10);
        local_59[uVar7 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fbd4) >> 0x18);
        local_59[uVar7 * 0xe + 4] = *(byte *)(uVar2 * 0xc + -0x120fbd0);
        local_59[uVar7 * 0xe + 6] = *(byte *)(uVar2 * 0xc + -0x120fbce);
        bVar5 = FUN_00064cb0((uint)local_59[uVar7 * 0xe + 3] * 0x1000000 +
                             (uint)local_59[uVar7 * 0xe + 2] * 0x10000 +
                             (uint)local_59[uVar7 * 0xe + 1] * 0x100 + (uint)local_59[uVar7 * 0xe]);
        if (bVar5 < 0xe) {
          if (bVar5 < 10) {
LAB_00062ea2:
            FUN_000914f0(uStack_63,bVar5,abStack_4f + uVar7 * 0xe);
            uVar1 = FUN_000914d8(uStack_63,bVar5);
            if (uVar1 == abStack_4f[uVar7 * 0xe]) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
          else if (bVar5 == 10) {
            FUN_000914f0(uStack_63,10,abStack_4f + uVar7 * 0xe);
            iVar3 = FUN_0008e20c(uStack_63);
            if (iVar3 != 0) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
          else if (bVar5 < 0xc) {
            uVar1 = FUN_000914d8(uStack_63,0xb);
            abStack_4f[uVar7 * 0xe] = (byte)uVar1;
            if (uVar1 == abStack_4f[uVar7 * 0xe]) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
          else {
            if (bVar5 != 0xc) goto LAB_00062ea2;
            FUN_000914f0(uStack_63,0xc,abStack_4f + uVar7 * 0xe);
            iVar3 = FUN_0008e34a(uStack_63);
            if (iVar3 != 0) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
        }
        else if (bVar5 < 0xf) {
          abStack_4f[uVar7 * 0xe] = bRamfedf0343;
          uVar1 = FUN_000914d8(uStack_63,10);
          if (uVar1 == abStack_4f[uVar7 * 0xe]) {
            local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
          }
        }
        else if (bVar5 == 0xf) {
          uVar1 = FUN_000914d8(uStack_63,0xb);
          abStack_4f[uVar7 * 0xe] = (byte)uVar1;
          if (uVar1 == abStack_4f[uVar7 * 0xe]) {
            local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
          }
        }
        else {
          if (bVar5 != 0x10) goto LAB_00062ea2;
          abStack_4f[uVar7 * 0xe] = bRamfedf035b;
          uVar1 = FUN_000914d8(uStack_63,0xc);
          if (uVar1 == abStack_4f[uVar7 * 0xe]) {
            local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
          }
        }
        thunk_FUN_000b507a(uVar8 + 0xfedf28dc,local_59 + uVar7 * 0xe,0xb);
        uVar8 = uVar8 + 0xb & 0xff;
        uVar6 = uVar6 + 0xb & 0xff;
        FUN_00064c48(*(undefined4 *)(uVar2 * 0xc + -0x120fbd4),uStack_63);
        FUN_0009144c(0,1,0xff);
      }
      uVar7 = uVar7 + 1 & 0xff;
    }
    FUN_00066134(0xfedf28dc,uVar6 + 10 & 0xff);
    if ((int)(((uint)bRamfedf04fe + (uint)bRamfedf04fd) - (uint)bRamfedf04ff) < 0) {
      bRamfedf04fd = bRamfedf04fe + bRamfedf04fd;
      iVar3 = (uint)bRamfedf04ff - (uint)bRamfedf04fd;
      if (iVar3 + -4 < 0 == (iVar3 < 0 && -1 < iVar3 + -4) && iVar3 != 4) {
        bRamfedf04fe = 4;
      }
      else {
        bRamfedf04fe = bRamfedf04ff - bRamfedf04fd;
      }
    }
    else {
      bRamfedf04fd = 0;
      bRamfedf04fe = 0;
      bRamfedf04ff = 0;
      cRamfedf04fc = '\0';
    }
  }
  else if (cStack_21 == '\0') {
    cStack_65 = cStack_21;
    cStack_64 = cStack_20;
    uStack_63 = uStack_1f;
    uStack_62 = 1;
    uStack_61 = uStack_1e;
    if (cRamfedf04f8 == '\0') {
      FUN_0008fb28(0xfedf0294,0xfedf042c,0xfedf0360,0xfedf2e4c,uStack_1f);
      bRamfedf04fb = bRamfedf2e4e;
      bRamfedf04f9 = 0;
      if (bRamfedf2e4e < 5) {
        bRamfedf04fa = bRamfedf2e4e;
      }
      else {
        bRamfedf04fa = 4;
      }
      cRamfedf04f8 = '\x01';
    }
    else {
      bStack_66 = local_1b;
      if ((bStack_1d & 0x80) != 0) {
        bStack_66 = local_1a;
      }
      if ((bStack_1c != bRamfedf04f9) || (bStack_66 != bRamfedf04fa)) {
        bRamfedf04f9 = 0;
        if (bRamfedf04fb < 5) {
          bRamfedf04fa = bRamfedf04fb;
        }
        else {
          bRamfedf04fa = 4;
        }
      }
    }
    bStack_60 = bRamfedf04fb;
    local_5f = 0;
    if ((bStack_1d & 0xf) == 0) {
      bStack_1d = bStack_1d & 0xf0 | 1;
    }
    bStack_5e = bStack_1d;
    if ((bStack_1d & 0x80) == 0) {
      local_5c = bRamfedf04fa;
      uVar8 = 10;
    }
    else {
      local_5c = 0;
      local_5b = bRamfedf04fa;
      local_5a = 0;
      uVar8 = 0xc;
    }
    bStack_5d = bRamfedf04f9;
    thunk_FUN_000b507a(0xfedf28dc,&cStack_65,uVar8);
    for (uVar2 = 0; uVar2 < bRamfedf2e4e; uVar2 = uVar2 + 1 & 0xff) {
      uVar7 = 0;
      do {
        if (*(int *)(uVar7 * 8 + 0x24518) == *(int *)(uVar2 * 0xc + -0x120fca0)) {
          *(bool *)(uVar2 * 0xc + -0x120fc99) =
               (*(byte *)(uVar7 * 8 + 0x2451c) &
               *(byte *)(*(byte *)(uVar7 * 8 + 0x2451d) + 0xfedf2e4c)) ==
               *(byte *)(uVar7 * 8 + 0x2451c);
        }
        uVar7 = uVar7 + 1 & 0xff;
      } while (uVar7 < 10);
    }
    uVar7 = 0;
    for (uVar2 = (uint)bRamfedf04f9; (int)(uVar2 - ((uint)bRamfedf04fa + (uint)bRamfedf04f9)) < 0;
        uVar2 = uVar2 + 1 & 0xff) {
      local_59[uVar7 * 0xe + 5] = 0xf0;
      if ((bStack_1d & 0xf) == 1) {
        local_59[uVar7 * 0xe] = *(byte *)(uVar2 * 0xc + -0x120fca0);
        local_59[uVar7 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 8);
        local_59[uVar7 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 0x10);
        local_59[uVar7 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 0x18);
        local_59[uVar7 * 0xe + 4] = *(byte *)(uVar2 * 0xc + -0x120fc9c);
        thunk_FUN_000b507a(uVar8 + 0xfedf28dc,local_59 + uVar7 * 0xe,5);
        uVar8 = uVar8 + 5 & 0xff;
        uVar6 = uVar6 + 5 & 0xff;
      }
      else if ((bStack_1d & 0xf) == 2) {
        local_59[uVar7 * 0xe] = *(byte *)(uVar2 * 0xc + -0x120fca0);
        local_59[uVar7 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 8);
        local_59[uVar7 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 0x10);
        local_59[uVar7 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 0x18);
        local_59[uVar7 * 0xe + 4] = *(byte *)(uVar2 * 0xc + -0x120fc9c);
        thunk_FUN_000b507a(uVar8 + 0xfedf28dc,local_59 + uVar7 * 0xe,6);
        uVar8 = uVar8 + 6 & 0xff;
        uVar6 = uVar6 + 6 & 0xff;
      }
      else if ((bStack_1d & 0xf) == 3) {
        local_59[uVar7 * 0xe] = *(byte *)(uVar2 * 0xc + -0x120fca0);
        local_59[uVar7 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 8);
        local_59[uVar7 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 0x10);
        local_59[uVar7 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar2 * 0xc + -0x120fca0) >> 0x18);
        local_59[uVar7 * 0xe + 4] = *(byte *)(uVar2 * 0xc + -0x120fc9c);
        local_59[uVar7 * 0xe + 6] = *(byte *)(uVar2 * 0xc + -0x120fc9a);
        bVar5 = FUN_00064cb0((uint)local_59[uVar7 * 0xe + 3] * 0x1000000 +
                             (uint)local_59[uVar7 * 0xe + 2] * 0x10000 +
                             (uint)local_59[uVar7 * 0xe + 1] * 0x100 + (uint)local_59[uVar7 * 0xe]);
        if (bVar5 < 0xe) {
          if (bVar5 < 10) {
LAB_00063768:
            FUN_000914f0(uStack_63,bVar5,abStack_4f + uVar7 * 0xe);
            uVar1 = FUN_000914d8(uStack_63,bVar5);
            if (uVar1 == abStack_4f[uVar7 * 0xe]) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
          else if (bVar5 == 10) {
            FUN_000914f0(uStack_63,10,abStack_4f + uVar7 * 0xe);
            iVar3 = FUN_0008e20c(uStack_63);
            if (iVar3 != 0) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
          else if (bVar5 < 0xc) {
            uVar1 = FUN_000914d8(uStack_63,0xb);
            abStack_4f[uVar7 * 0xe] = (byte)uVar1;
            if (uVar1 == abStack_4f[uVar7 * 0xe]) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
          else {
            if (bVar5 != 0xc) goto LAB_00063768;
            FUN_000914f0(uStack_63,0xc,abStack_4f + uVar7 * 0xe);
            iVar3 = FUN_0008e34a(uStack_63);
            if (iVar3 != 0) {
              local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
            }
          }
        }
        else if (bVar5 < 0xf) {
          abStack_4f[uVar7 * 0xe] = *(byte *)((uint)bVar5 * 0xc + -0x120fd65);
          uVar1 = FUN_000914d8(uStack_63,10);
          if (uVar1 == abStack_4f[uVar7 * 0xe]) {
            local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
          }
        }
        else if (bVar5 == 0xf) {
          uVar1 = FUN_000914d8(uStack_63,0xb);
          abStack_4f[uVar7 * 0xe] = (byte)uVar1;
          if (uVar1 == abStack_4f[uVar7 * 0xe]) {
            local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
          }
        }
        else {
          if (bVar5 != 0x10) goto LAB_00063768;
          abStack_4f[uVar7 * 0xe] = bRamfedf035b;
          uVar1 = FUN_000914d8(uStack_63,0xc);
          if (uVar1 == abStack_4f[uVar7 * 0xe]) {
            local_59[uVar7 * 0xe + 5] = local_59[uVar7 * 0xe + 5] | 1;
          }
        }
        thunk_FUN_000b507a(uVar8 + 0xfedf28dc,local_59 + uVar7 * 0xe,0xb);
        uVar8 = uVar8 + 0xb & 0xff;
        uVar6 = uVar6 + 0xb & 0xff;
        FUN_00064c48(*(undefined4 *)(uVar2 * 0xc + -0x120fca0),uStack_63);
        FUN_0009144c(0,1,0xff);
      }
      uVar7 = uVar7 + 1 & 0xff;
    }
    FUN_00066134(0xfedf28dc,uVar6 + 10 & 0xff);
    if ((int)(((uint)bRamfedf04fa + (uint)bRamfedf04f9) - (uint)bRamfedf04fb) < 0) {
      bRamfedf04f9 = bRamfedf04fa + bRamfedf04f9;
      iVar3 = (uint)bRamfedf04fb - (uint)bRamfedf04f9;
      if (iVar3 + -4 < 0 == (iVar3 < 0 && -1 < iVar3 + -4) && iVar3 != 4) {
        bRamfedf04fa = 4;
      }
      else {
        bRamfedf04fa = bRamfedf04fb - bRamfedf04f9;
      }
    }
    else {
      bRamfedf04f9 = 0;
      bRamfedf04fa = 0;
      bRamfedf04fb = 0;
      cRamfedf04f8 = '\0';
    }
  }
  else if (((cStack_21 == '\x01') || (cStack_21 == '\0')) && (cStack_20 == '\x01')) {
    cStack_65 = cStack_21;
    cStack_64 = cStack_20;
    uStack_63 = uStack_1f;
    uStack_62 = 1;
    FUN_0008fb28(0xfedf0294,0xfedf042c,0xfedf0360,0xfedf2e4c,uStack_1f);
    if (cStack_21 == '\x01') {
      bStack_60 = bRamfedf2e4f;
    }
    else if (cStack_21 == '\0') {
      bStack_60 = bRamfedf2e4e;
    }
    local_5f = 0;
    bStack_5e = bStack_1d;
    if (cStack_21 == '\x01') {
      if ((bStack_1d & 0x80) == 0) {
        bStack_5d = bStack_1c;
        local_5c = local_1b;
        uVar2 = 10;
        uVar7 = (uint)bStack_1c;
        bStack_66 = local_1b + bStack_1c;
      }
      else {
        bStack_5d = bStack_1c;
        local_5c = local_1b;
        local_5b = local_1a;
        local_5a = local_19;
        uVar2 = 0xc;
        uVar7 = (uint)bStack_1c;
        bStack_66 = local_1a + bStack_1c;
      }
    }
    else if ((bStack_1d & 0x80) == 0) {
      bStack_5d = 0;
      local_5c = 4;
      uVar2 = 10;
      uVar7 = 0;
      bStack_66 = 4;
    }
    else {
      bStack_5d = 0;
      local_5c = 0;
      local_5b = 4;
      local_5a = 0;
      uVar2 = 0xc;
      uVar7 = 0;
      bStack_66 = 4;
    }
    thunk_FUN_000b507a(0xfedf28dc,&cStack_65,uVar2);
    if (cStack_21 == '\0') {
      param_1 = -0x120fbd4;
    }
    else if (cStack_21 == '\x01') {
      param_1 = -0x120fbd4;
    }
    iVar3 = bStack_66 - uVar7;
    if (iVar3 + -4 < 0 == (iVar3 < 0 && -1 < iVar3 + -4) && iVar3 != 4) {
      bStack_66 = (char)uVar7 + 4;
    }
    for (uVar1 = 0; uVar1 < bRamfedf2e4e; uVar1 = uVar1 + 1 & 0xff) {
      uVar4 = 0;
      do {
        if (*(int *)(uVar4 * 8 + 0x24518) == *(int *)(uVar1 * 0xc + param_1)) {
          *(bool *)(uVar1 * 0xc + param_1 + 7) =
               (*(byte *)(uVar4 * 8 + 0x2451c) &
               *(byte *)(*(byte *)(uVar4 * 8 + 0x2451d) + 0xfedf2e4c)) ==
               *(byte *)(uVar4 * 8 + 0x2451c);
        }
        uVar4 = uVar4 + 1 & 0xff;
      } while (uVar4 < 10);
    }
    if (bStack_66 < 0x11) {
      uVar8 = 0;
      for (; uVar7 < bStack_66; uVar7 = uVar7 + 1 & 0xff) {
        if ((bStack_1d & 0xf) == 1) {
          local_59[uVar8 * 0xe] = *(byte *)(uVar7 * 0xc + param_1);
          local_59[uVar8 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 8);
          local_59[uVar8 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 0x10);
          local_59[uVar8 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 0x18);
          local_59[uVar8 * 0xe + 4] = *(byte *)(uVar7 * 0xc + param_1 + 4);
          thunk_FUN_000b507a(uVar2 + 0xfedf28dc,local_59 + uVar8 * 0xe,5);
          uVar2 = uVar2 + 5 & 0xff;
        }
        else if ((bStack_1d & 0xf) == 2) {
          local_59[uVar8 * 0xe] = *(byte *)(uVar7 * 0xc + param_1);
          local_59[uVar8 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 8);
          local_59[uVar8 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 0x10);
          local_59[uVar8 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 0x18);
          local_59[uVar8 * 0xe + 4] = *(byte *)(uVar7 * 0xc + param_1 + 4);
          local_59[uVar8 * 0xe + 5] = *(byte *)(uVar7 * 0xc + param_1 + 5);
          thunk_FUN_000b507a(uVar2 + 0xfedf28dc,local_59 + uVar8 * 0xe,6);
          uVar2 = uVar2 + 6 & 0xff;
        }
        else if ((bStack_1d & 0xf) == 3) {
          local_59[uVar8 * 0xe] = *(byte *)(uVar7 * 0xc + param_1);
          local_59[uVar8 * 0xe + 1] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 8);
          local_59[uVar8 * 0xe + 2] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 0x10);
          local_59[uVar8 * 0xe + 3] = (byte)((uint)*(undefined4 *)(uVar7 * 0xc + param_1) >> 0x18);
          local_59[uVar8 * 0xe + 4] = *(byte *)(uVar7 * 0xc + param_1 + 4);
          local_59[uVar8 * 0xe + 5] = *(byte *)(uVar7 * 0xc + param_1 + 5);
          local_59[uVar8 * 0xe + 6] = *(byte *)(uVar7 * 0xc + param_1 + 6);
          abStack_4f[uVar8 * 0xe] = *(byte *)(uVar7 * 0xc + param_1 + 7);
          thunk_FUN_000b507a(uVar2 + 0xfedf28dc,local_59 + uVar8 * 0xe,0xb);
          uVar2 = uVar2 + 0xb & 0xff;
          FUN_00064c48(*(undefined4 *)(uVar7 * 0xc + param_1),uStack_63);
          FUN_0009144c(0,1,0xff);
        }
        uVar8 = uVar8 + 1 & 0xff;
      }
    }
    if ((bStack_1d & 0xf) == 1) {
      uVar6 = uVar8 * 5 + 10 & 0xff;
    }
    else if ((bStack_1d & 0xf) == 2) {
      uVar6 = uVar8 * 6 + 10 & 0xff;
    }
    else if ((bStack_1d & 0xf) == 3) {
      uVar6 = uVar8 * 0xb + 10 & 0xff;
    }
    FUN_00066134(0xfedf28dc,uVar6);
  }
  return;
}


