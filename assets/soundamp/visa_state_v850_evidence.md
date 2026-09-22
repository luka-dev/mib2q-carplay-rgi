# V850 level-state evidence

2026-09-08. CodeFlash base/file offset 0; Ghidra V850 decompilation.
Unrecovered fixed-register conventions and soft-float calls can distort expressions.
Raw disassembly independently verifies field 4 accepts 0..3 and writes gp-0x418f.

## 0xCA4B8

```c

undefined4
FUN_000ca4b8(undefined4 param_1,int *param_2,undefined4 param_3,undefined4 param_4,
            undefined4 param_5)

{
  char cVar1;
  char cVar2;
  char cVar3;
  char cVar4;
  char cVar5;
  int unaff_gp;
  undefined4 uVar6;
  uint uVar7;
  uint uVar8;
  uint unaff_r20;
  int iVar9;
  int iVar10;
  int iVar11;
  undefined4 uStack00000004;
  int *piStack00000008;

  cVar1 = *(char *)(unaff_gp + -0x456b);
  cVar2 = *(char *)(unaff_gp + -0x456a);
  cVar3 = *(char *)(unaff_gp + -0x418f);
  uStack00000004 = param_1;
  piStack00000008 = param_2;
  thunk_FUN_000ea47e(3,0x119,0,0);
  if (*(char *)*param_2 == '\0') {
    if ((short)param_2[1] != 8) {
      thunk_FUN_00099e0e(param_1,5);
      return 0xf;
    }
  }
  else {
    if ((short)param_2[1] != 3) {
      thunk_FUN_00099e0e(param_1,5);
      return 0xf;
    }
    if ((*(char *)*param_2 == '\0') || (6 < *(byte *)*param_2)) {
      thunk_FUN_00099efc(param_1,1,*param_2,1);
      return 0xf;
    }
  }
  iVar9 = 0x5a;
  uVar7 = 2;
  iVar10 = 0x5a;
  if ((*(char *)*param_2 == '\0') || (*(char *)*param_2 == '\x01')) {
    param_5._3_1_ = *(byte *)(*param_2 + 2);
    uVar7 = unaff_r20 & 3;
    if (param_5._3_1_ != 0) {
      thunk_FUN_00099efc(param_1,uVar7 + 1 & unaff_r20,(int)&param_5 + 3,1);
      return 0xf;
    }
  }
  if ((*(char *)*param_2 == '\0') || (iVar11 = iVar9, *(char *)*param_2 == '\x02')) {
    param_5._3_1_ = *(byte *)(*param_2 + uVar7);
    uVar8 = (uint)param_5._3_1_;
    uVar7 = uVar7 + 1 & unaff_r20;
    iVar11 = iVar10;
    if ((-1 < (int)(uVar8 - 1)) &&
       (((int)(uVar8 - 8) < 0 || uVar8 == 8 &&
        ((&DAT_00100008)[uVar8 + (uint)*(byte *)(unaff_gp + -0x4592) * 0x15] != -1)))) {
      if (uVar8 != *(byte *)(unaff_gp + -0x3a1a)) {
        iVar9 = 0xa5;
      }
      *(byte *)(unaff_gp + -0x3a1a) = param_5._3_1_;
      iVar11 = iVar9;
    }
  }
  if ((*(char *)*param_2 == '\0') || (*(char *)*param_2 == '\x03')) {
    if ((*(byte *)(*param_2 + uVar7) & 1) == 0) {
      if (*(char *)(unaff_gp + -0x4568) == -0x5b) {
        iVar11 = 0xa5;
      }
      *(undefined1 *)(unaff_gp + -0x4568) = 0x5a;
    }
    else {
      if (*(char *)(unaff_gp + -0x4568) == 'Z') {
        iVar11 = 0xa5;
      }
      *(undefined1 *)(unaff_gp + -0x4568) = 0xa5;
    }
    uVar7 = uVar7 + 1 & unaff_r20;
  }
  if ((*(char *)*param_2 == '\0') || (*(char *)*param_2 == '\x04')) {
    param_5._3_1_ = *(byte *)(*param_2 + uVar7);
    uVar8 = (uint)param_5._3_1_;
    if (-1 < (int)(uVar8 - 3) && uVar8 != 3) {
      uVar6 = thunk_FUN_00099efc(param_1,uVar7 + 1 & unaff_r20,(int)&param_5 + 3,1);
      return uVar6;
    }
    if (uVar8 != *(byte *)(unaff_gp + -0xcc8)) {
      iVar11 = 0xa5;
      *(byte *)(unaff_gp + -0xcc8) = param_5._3_1_;
      *(byte *)(unaff_gp + -0x418f) = param_5._3_1_;
    }
    uVar7 = uVar7 + 1 & unaff_r20;
  }
  if ((*(char *)*param_2 == '\0') || (*(char *)*param_2 == '\x05')) {
    cVar4 = *(char *)(*param_2 + uVar7);
    iVar9 = (int)cVar4;
    cVar5 = cVar4 >> 7;
    if ((iVar9 + -0xc < 0 == (cVar5 < '\0' && -1 < iVar9 + -0xc) && iVar9 != 0xc) ||
       (iVar9 + 0xc < 0 != (-1 < cVar5 && iVar9 + 0xc < 0))) {
      uVar6 = thunk_FUN_00099efc(param_1,uVar7 + 1 & unaff_r20,*param_2 + uVar7,1);
      return uVar6;
    }
    if (iVar9 != *(char *)(unaff_gp + -0x3a7d)) {
      iVar11 = 0xa5;
    }
    *(char *)(unaff_gp + -0x3a7d) = cVar4;
    if (iVar9 + 9 < 0 == (-1 < cVar5 && iVar9 + 9 < 0)) {
      if (iVar9 + -9 < 0 == (cVar5 < '\0' && -1 < iVar9 + -9) && iVar9 != 9) {
        *(undefined1 *)(unaff_gp + -0x3de0) = 9;
      }
      else {
        *(char *)(unaff_gp + -0x3de0) = cVar4;
      }
    }
    else {
      *(undefined1 *)(unaff_gp + -0x3de0) = 0xf7;
    }
    uVar7 = uVar7 + 1 & unaff_r20;
  }
  if ((*(char *)*param_2 == '\0') || (*(char *)*param_2 == '\x06')) {
    param_5._2_1_ = *(byte *)(*param_2 + uVar7);
    uVar8 = (uint)param_5._2_1_;
    if (-1 < (int)(uVar8 - 9) && uVar8 != 9) {
      uVar6 = thunk_FUN_00099efc(param_1,uVar7 + 1 & unaff_r20,(int)&param_5 + 2,1);
      return uVar6;
    }
    if (uVar8 != *(byte *)(unaff_gp + -0x3d8f)) {
      iVar11 = 0xa5;
      cVar4 = *(char *)(unaff_gp + -0x3d8f);
      *(byte *)(unaff_gp + -0x3d8f) = param_5._2_1_;
      thunk_FUN_000c39b6(0x30);
      if ((*(char *)(unaff_gp + -0x3d8f) == '\x02') || (cVar4 == '\x02')) {
        thunk_FUN_000dd374(4,0x11);
      }
    }
  }
  if (iVar11 == 0xa5) {
    *(undefined1 *)(unaff_gp + -0xcc7) = *(undefined1 *)*param_2;
    thunk_FUN_000dbe36(unaff_gp + -0xe5a,1);
  }
  if (*(char *)(unaff_gp + -0x4573) != -0x5b) {
    *(undefined1 *)(unaff_gp + -0x456a) = *(undefined1 *)(unaff_gp + -0x3a1a);
  }
  if ((*(char *)(unaff_gp + -0x4573) == -0x5b) && (*(char *)(unaff_gp + -0x456a) == '\0')) {
    *(undefined1 *)(unaff_gp + -0x456b) = 0x5a;
  }
  else {
    *(undefined1 *)(unaff_gp + -0x456b) = *(undefined1 *)(unaff_gp + -0x4568);
  }
  if (((*(char *)(unaff_gp + -0x456b) == cVar1) && (*(char *)(unaff_gp + -0x456a) == cVar2)) &&
     (*(char *)(unaff_gp + -0x418f) == cVar3)) {
    thunk_FUN_000c39b6(0x16);
  }
  else if (((*(char *)(unaff_gp + -0x456b) == cVar1) && (*(char *)(unaff_gp + -0x456a) == cVar2)) &&
          (*(char *)(unaff_gp + -0x418f) != cVar3)) {
    if (*(char *)(unaff_gp + -0x456b) == -0x5b) {
      thunk_FUN_000c39b6(0x2f);
      thunk_FUN_000c39b6(0x18);
      thunk_FUN_000c39b6(0x16);
      thunk_FUN_000c39b6(0xf);
      thunk_FUN_000dd374(5,4);
    }
  }
  else if ((*(char *)(unaff_gp + -0x456b) == cVar1) || (*(char *)(unaff_gp + -0x456a) != cVar2)) {
    thunk_FUN_000dd374(1,0x14);
  }
  else {
    thunk_FUN_000dd374(1,0x1c);
  }
  return 0;
}


```

## 0xD3A38

```c

undefined4 FUN_000d3a38(undefined4 param_1,int *param_2)

{
  char cVar1;
  char cVar2;
  byte bVar3;
  int unaff_gp;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint unaff_r20;
  undefined4 uStack00000000;
  int *piStack00000004;

  cVar1 = *(char *)(unaff_gp + -0x456b);
  cVar2 = *(char *)(unaff_gp + -0x418f);
  if (*(char *)*param_2 == '\0') {
    *(undefined1 *)(unaff_gp + -0x4568) = 0x5a;
  }
  else {
    *(undefined1 *)(unaff_gp + -0x4568) = 0xa5;
    *(char *)(unaff_gp + -0x418f) = *(char *)*param_2 + -1;
  }
  bVar3 = *(byte *)*param_2;
  uVar6 = 1;
  uVar4 = 0;
  do {
    uVar5 = uVar6 & unaff_r20;
    uVar6 = uVar5 + 1;
    *(undefined1 *)((uint)bVar3 * 0x14 + unaff_gp + -0x135f8 + uVar4) =
         *(undefined1 *)(*param_2 + uVar5);
    uVar4 = uVar4 + 1 & unaff_r20;
  } while (uVar4 < 0x14);
  *(undefined1 *)(unaff_gp + -0x456b) = *(undefined1 *)(unaff_gp + -0x4568);
  uStack00000000 = param_1;
  piStack00000004 = param_2;
  if ((*(char *)(unaff_gp + -0x4568) == cVar1) && (*(char *)(unaff_gp + -0x418f) == cVar2)) {
    thunk_FUN_000c39b6(0x16);
  }
  else if ((*(char *)(unaff_gp + -0x456b) == cVar1) && (*(char *)(unaff_gp + -0x418f) != cVar2)) {
    if (*(char *)(unaff_gp + -0x456b) == -0x5b) {
      thunk_FUN_000c39b6(0x2f);
      thunk_FUN_000c39b6(0x18);
      thunk_FUN_000c39b6(0x16);
      thunk_FUN_000c39b6(0xf);
      thunk_FUN_000dd374(5,4);
    }
  }
  else {
    thunk_FUN_000dd374(1,0x1c);
  }
  return 0;
}


```

## 0xD3B4E

```c

undefined4 FUN_000d3b4e(int *param_1)

{
  int unaff_gp;
  uint uVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint unaff_r20;

  if (*(char *)(unaff_gp + -0x4568) == -0x5b) {
    uVar4 = (int)*(char *)(unaff_gp + -0x418f) + 1U & unaff_r20;
  }
  else {
    uVar4 = 0;
  }
  *(char *)*param_1 = (char)uVar4;
  uVar1 = 0;
  uVar2 = unaff_r20 & 1;
  do {
    uVar3 = uVar2 + 1 & unaff_r20;
    *(undefined1 *)(*param_1 + uVar2) = *(undefined1 *)(uVar4 * 0x14 + unaff_gp + -0x135f8 + uVar1);
    uVar1 = uVar1 + 1 & unaff_r20;
    uVar2 = uVar3;
  } while (uVar1 < 0x14);
  *(short *)(param_1 + 1) = (short)uVar3;
  return 0xc;
}


```

## 0xAA38C

```c

void FUN_000aa38c(uint param_1)

{
  int unaff_gp;
  int iVar1;
  uint uVar2;
  uint uStack00000000;
  uint uStack00000004;

  uStack00000004 = param_1;
  switch(param_1 & 0xffff) {
  case 0x2d:
    thunk_FUN_000ac6c4(0x10,*(undefined4 *)(&DAT_0006eaa0 + (uint)*(byte *)(unaff_gp + -0x3d90) * 4)
                      );
    break;
  case 0x2e:
    iVar1 = (char)(&DAT_00100008)
                  [(uint)*(byte *)(unaff_gp + -0x456a) + (uint)*(byte *)(unaff_gp + -0x4592) * 0x15]
            + 1;
    FUN_000ebca4();
    thunk_FUN_000ac6c4(0x11,iVar1);
    break;
  case 0x2f:
    thunk_FUN_000ac6c4(0x13,*(undefined4 *)(&DAT_0006ea68 + (uint)*(byte *)(unaff_gp + -0x418f) * 4)
                      );
    break;
  case 0x30:
    if (*(char *)(unaff_gp + -0x3d90) == '\0') {
      thunk_FUN_000ac6c4(0x10,*(undefined4 *)
                               (&DAT_0006eaa0 + (uint)*(byte *)(unaff_gp + -0x3d90) * 4));
      thunk_FUN_000ac6c4(0x14,*(undefined4 *)
                               (&LAB_0006ea78 + (uint)*(byte *)(unaff_gp + -0x3d8f) * 4));
    }
    else {
      thunk_FUN_000ac6c4(0x14,*(undefined4 *)
                               (&LAB_0006ea78 + (uint)*(byte *)(unaff_gp + -0x3d8f) * 4));
      thunk_FUN_000dced4(0xf);
      thunk_FUN_000ac6c4(0x10,*(undefined4 *)
                               (&DAT_0006eaa0 + (uint)*(byte *)(unaff_gp + -0x3d90) * 4));
    }
    break;
  case 0x31:
    iVar1 = (int)*(char *)(unaff_gp + -0x3d8e + (uint)*(byte *)(unaff_gp + -0x3d7c));
    FUN_000ebca4();
    thunk_FUN_000ac6c4(0x15,iVar1);
    iVar1 = (int)*(char *)(unaff_gp + -0x3d7b + (uint)*(byte *)(unaff_gp + -0x3d7c));
    FUN_000ebca4();
    thunk_FUN_000ac6c4(0x16,iVar1);
    break;
  case 0x32:
    iVar1 = (int)*(char *)(unaff_gp + -0x3d69);
    FUN_000ebca4();
    thunk_FUN_000ac6c4(0x17,iVar1);
    break;
  case 0x33:
    iVar1 = (int)*(char *)(unaff_gp + -0x3d67);
    FUN_000ebca4();
    thunk_FUN_000ac6c4(0x18,iVar1);
    break;
  case 0x34:
    uStack00000000 = *(char *)(unaff_gp + -0x3d66) + 100;
    if ((int)uStack00000000 < 1) {
      uStack00000000 = 0;
    }
    else {
      FUN_000ebca4();
    }
    thunk_FUN_000ac6c4(0x19,uStack00000000);
    break;
  case 0x35:
    uStack00000000 = *(uint *)(unaff_gp + -0x45a8) / 100;
    if (uStack00000000 < 400) {
      if ((int)uStack00000000 < 0) {
        FUN_000ebca4(uStack00000000);
        uVar2 = 0x4f000000;
        FUN_000eba38(0x4f000000,uStack00000000);
        uStack00000000 = uVar2;
      }
      else {
        FUN_000ebca4();
      }
    }
    else {
      uStack00000000 = 0x43c80000;
    }
    thunk_FUN_000ac6c4(0x1b,uStack00000000);
  }
  return;
}


```

## 0xA711C

```c

void FUN_000a711c(void)

{
  int unaff_gp;
  uint uVar1;
  uint uVar2;
  uint unaff_r20;
  uint unaff_r21;

  uVar2 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x44d8 + uVar2) = 0;
    uVar2 = uVar2 + 1 & unaff_r21;
  } while ((int)(uVar2 - 0x180) < 0 != ((int)uVar2 < 0 && -1 < (int)(uVar2 - 0x180)));
  uVar2 = 0;
  do {
    *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0;
    uVar2 = uVar2 + 1 & unaff_r21;
  } while ((int)(uVar2 - 0x60) < 0 != ((int)uVar2 < 0 && -1 < (int)(uVar2 - 0x60)));
  thunk_FUN_000dd2cc(5);
  *(undefined4 *)(unaff_gp + -0x41c0) = 0;
  *(undefined1 *)(unaff_gp + -0x41aa) = 0x14;
  *(undefined1 *)(unaff_gp + -0x41a9) = 0;
  *(undefined2 *)(unaff_gp + -0x41bc) = 0x14;
  *(undefined1 *)(unaff_gp + -0x41a8) = 0;
  *(undefined1 *)(unaff_gp + -0x41a6) = 0;
  *(undefined1 *)(unaff_gp + -0x41a5) = 0xa5;
  *(undefined4 *)(unaff_gp + -0x4358) = 0;
  *(undefined4 *)(unaff_gp + -0x4354) = 0;
  *(undefined4 *)(unaff_gp + -0x4350) = 0;
  *(undefined4 *)(unaff_gp + -0x434c) = 0;
  *(undefined4 *)(unaff_gp + -0x4348) = 0;
  *(undefined4 *)(unaff_gp + -0x4344) = 0;
  *(undefined1 *)(unaff_gp + -0x4190) = 0x5a;
  if (((*(char *)(unaff_gp + -0x456b) == -0x5b) && (*(char *)(unaff_gp + -0x418f) != '\0')) &&
     (*(char *)(unaff_gp + -0x455c) == -0x5b)) {
    *(undefined1 *)(unaff_gp + -0x41a7) = 1;
  }
  else {
    *(undefined1 *)(unaff_gp + -0x41a7) = 0;
  }
  if ((*(char *)(unaff_gp + -0x456b) == -0x5b) && (*(char *)(unaff_gp + -0x455c) == -0x5b)) {
    uVar2 = (int)*(char *)(unaff_gp + -0x418f) + 1U & unaff_r20;
  }
  else {
    uVar2 = 0;
  }
  uVar1 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x41a4 + uVar1) =
         *(undefined1 *)(uVar2 * 0x14 + unaff_gp + -0x135f8 + uVar1);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0x14) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0x14)));
  *(undefined1 *)(unaff_gp + -0x5ef8) = 0xa5;
  return;
}


```

## 0xAC9C4

```c

void FUN_000ac9c4(void)

{
  int unaff_gp;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;

  if ((*(char *)(unaff_gp + -0x456b) == -0x5b) && (*(char *)(unaff_gp + -0x455c) == -0x5b)) {
    uVar1 = (int)*(char *)(unaff_gp + -0x418f) + 1U & unaff_r20;
  }
  else {
    uVar1 = 0;
  }
  uVar2 = 0;
  uVar3 = 0;
  do {
    thunk_FUN_000a8c14(0x14,5,1);
    *(undefined4 *)(unaff_gp + -0x4334) = 0x14;
    uVar4 = (unaff_r20 & 4) + 1 & unaff_r20;
    *(uint *)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = uVar2 * 5 + 0xe01b4;
    uVar5 = 0;
    do {
      thunk_FUN_000a984e(unaff_gp + -0x1284c + uVar1 * 0x80 + uVar2 * 8);
      uVar4 = thunk_FUN_000ac05a(uVar4);
      uVar2 = uVar2 + 1 & unaff_r21;
      uVar5 = uVar5 + 1 & unaff_r20;
    } while ((int)(uVar5 - 4) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 4)));
    thunk_FUN_000a8d54();
    thunk_FUN_000a8cfc();
    thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x68);
    uVar3 = uVar3 + 1 & unaff_r20;
  } while ((int)(uVar3 - 4) < 0 != ((int)uVar3 < 0 && -1 < (int)(uVar3 - 4)));
  return;
}


```
