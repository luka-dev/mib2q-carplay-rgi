# V850 decompiler evidence — output-binding audit

Generated from existing Ghidra V850 project, base 0, 2026-09-08.
Raw pseudocode retains decompiler artifacts (notably fixed-register and soft-float calling conventions).
Use original instructions to validate ambiguous expressions. No firmware edits.

## 000a76b4

```c

void FUN_000a76b4(uint param_1)

{
  int unaff_gp;
  uint unaff_r21;
  uint uVar1;
  uint uStack00000004;

  uVar1 = param_1 & 0xffff;
  uStack00000004 = param_1;
  if ((int)(uVar1 - 0x177) < 0 || uVar1 == 0x177) {
    if (uVar1 == 0x1f) {
      thunk_FUN_000a9afa();
      return;
    }
    if ((int)(uVar1 - 0x1f) < 0 || uVar1 == 0x1f) {
      if (uVar1 == 0xf) {
        thunk_FUN_000a9036();
        return;
      }
      if ((int)(uVar1 - 0xf) < 0 || uVar1 == 0xf) {
        if (uVar1 == 7) {
          thunk_FUN_000a992c();
          return;
        }
        if ((int)(uVar1 - 7) < 0 || uVar1 == 7) {
          if (uVar1 == 3) {
LAB_000a76c2:
            if (*(char *)(unaff_gp + -0x3e42) == -0x5b) {
              thunk_FUN_000a901e(uVar1);
              return;
            }
            thunk_FUN_000a9c88(uVar1);
            return;
          }
          if ((int)(uVar1 - 3) < 0 || uVar1 == 3) {
            if (uVar1 == 1) goto LAB_000a76c2;
            if ((int)(uVar1 - 1) < 0 || uVar1 == 1) {
              if (uVar1 == 0) goto LAB_000a76c2;
            }
            else if (uVar1 == 2) goto LAB_000a76c2;
          }
          else {
            if (uVar1 == 5) {
              thunk_FUN_000ac8a2(5);
              return;
            }
            if ((int)(uVar1 - 5) < 0 || uVar1 == 5) {
              if (uVar1 == 4) goto LAB_000a76c2;
            }
            else if (uVar1 == 6) {
              thunk_FUN_000a9524();
              return;
            }
          }
        }
        else {
          if (uVar1 == 0xb) {
            thunk_FUN_000a9d74();
            return;
          }
          if ((int)(uVar1 - 0xb) < 0 || uVar1 == 0xb) {
            if (uVar1 == 9) {
              thunk_FUN_000a96be();
              return;
            }
            if ((int)(uVar1 - 9) < 0 || uVar1 == 9) {
              if (uVar1 == 8) {
                thunk_FUN_000ac936(8);
                return;
              }
            }
            else if (uVar1 == 10) {
              thunk_FUN_000a9bd4();
              return;
            }
          }
          else {
            if (uVar1 == 0xd) {
              thunk_FUN_000a9fea();
              return;
            }
            if ((int)(uVar1 - 0xd) < 0 || uVar1 == 0xd) {
              if (uVar1 == 0xc) {
                thunk_FUN_000a9f56();
                return;
              }
            }
            else if (uVar1 == 0xe) {
              thunk_FUN_000aa136();
              return;
            }
          }
        }
      }
      else {
        if (uVar1 == 0x17) {
          thunk_FUN_000ac9ba();
          return;
        }
        if ((int)(uVar1 - 0x17) < 0 || uVar1 == 0x17) {
          if (uVar1 == 0x13) {
            thunk_FUN_000a9ebe();
            return;
          }
          if ((int)(uVar1 - 0x13) < 0 || uVar1 == 0x13) {
            if (uVar1 == 0x11) {
              thunk_FUN_000a8bae();
              return;
            }
            if ((int)(uVar1 - 0x11) < 0 || uVar1 == 0x11) {
              if (uVar1 == 0x10) {
                thunk_FUN_000a8704();
                thunk_FUN_000aa26e();
                return;
              }
            }
            else if (uVar1 == 0x12) {
              thunk_FUN_000ab732();
              return;
            }
          }
          else {
            if (uVar1 == 0x15) {
              thunk_FUN_000aa63e();
              return;
            }
            if ((int)(uVar1 - 0x15) < 0 || uVar1 == 0x15) {
              if (uVar1 == 0x14) {
                thunk_FUN_000ac594();
                return;
              }
            }
            else if (uVar1 == 0x16) {
              thunk_FUN_000aa88e();
              thunk_FUN_000a91bc();
              return;
            }
          }
        }
        else {
          if (uVar1 == 0x1b) {
            thunk_FUN_000ace9c();
            return;
          }
          if ((int)(uVar1 - 0x1b) < 0 || uVar1 == 0x1b) {
            if (uVar1 == 0x19) {
              thunk_FUN_000acc76();
              return;
            }
            if ((int)(uVar1 - 0x19) < 0 || uVar1 == 0x19) {
              if (uVar1 == 0x18) {
                thunk_FUN_000aca94();
                return;
              }
            }
            else if (uVar1 == 0x1a) {
              thunk_FUN_000acd90();
              return;
            }
          }
          else {
            if (uVar1 == 0x1d) {
              thunk_FUN_000ad0a6();
              return;
            }
            if ((int)(uVar1 - 0x1d) < 0 || uVar1 == 0x1d) {
              if (uVar1 == 0x1c) {
                thunk_FUN_000acfa8();
                return;
              }
            }
            else if (uVar1 == 0x1e) {
              thunk_FUN_000a95bc();
              return;
            }
          }
        }
      }
    }
    else {
      if (uVar1 == 0x2f) goto LAB_000a7754;
      if ((int)(uVar1 - 0x2f) < 0 || uVar1 == 0x2f) {
        if (uVar1 == 0x27) {
          thunk_FUN_000aa9ae();
          return;
        }
        if ((int)(uVar1 - 0x27) < 0 || uVar1 == 0x27) {
          if (uVar1 == 0x23) {
            thunk_FUN_000aac7a();
            return;
          }
          if ((int)(uVar1 - 0x23) < 0 || uVar1 == 0x23) {
            if (uVar1 == 0x21) {
              thunk_FUN_000aad66();
              return;
            }
            if ((int)(uVar1 - 0x21) < 0 || uVar1 == 0x21) {
              if (uVar1 == 0x20) {
                thunk_FUN_000a9a20();
                return;
              }
            }
            else if (uVar1 == 0x22) {
              thunk_FUN_000aab38();
              return;
            }
          }
          else {
            if (uVar1 == 0x25) {
              thunk_FUN_000ab0b6();
              return;
            }
            if ((int)(uVar1 - 0x25) < 0 || uVar1 == 0x25) {
              if (uVar1 == 0x24) {
                thunk_FUN_000aafb4();
                return;
              }
            }
            else if (uVar1 == 0x26) {
              thunk_FUN_000ab1b4();
              return;
            }
          }
        }
        else {
          if (uVar1 == 0x2b) {
            thunk_FUN_000ab7ee();
            return;
          }
          if ((int)(uVar1 - 0x2b) < 0 || uVar1 == 0x2b) {
            if (uVar1 == 0x29) {
              thunk_FUN_000aa908();
              return;
            }
            if ((int)(uVar1 - 0x29) < 0 || uVar1 == 0x29) {
              if (uVar1 == 0x28) {
                thunk_FUN_000aaa54();
                return;
              }
            }
            else if (uVar1 == 0x2a) {
              thunk_FUN_000ab24c();
              return;
            }
          }
          else {
            if (uVar1 == 0x2d) {
LAB_000a7754:
              thunk_FUN_000aa624(uVar1);
              return;
            }
            if ((int)(uVar1 - 0x2d) < 0 || uVar1 == 0x2d) {
              if (uVar1 == 0x2c) {
                thunk_FUN_000ac8a2(0x2c);
                return;
              }
            }
            else if (uVar1 == 0x2e) goto LAB_000a7754;
          }
        }
      }
      else {
        if (uVar1 == 0x170) {
          thunk_FUN_000aa88e();
          return;
        }
        if ((int)(uVar1 - 0x170) < 0 || uVar1 == 0x170) {
          if (uVar1 != 0x33) {
            if ((int)(uVar1 - 0x33) < 0 || uVar1 == 0x33) {
              if (uVar1 != 0x31) {
                if ((int)(uVar1 - 0x31) < 0 || uVar1 == 0x31) {
                  if (uVar1 == 0x30) goto LAB_000a7754;
                }
                else if (uVar1 == 0x32) goto LAB_000a7754;
                goto LAB_000a78a8;
              }
            }
            else if (uVar1 != 0x35) {
              if ((int)(uVar1 - 0x35) < 0 || uVar1 == 0x35) {
                if (uVar1 == 0x34) goto LAB_000a7754;
              }
              else if (uVar1 == 0x15f) {
                thunk_FUN_000ab21e();
                return;
              }
              goto LAB_000a78a8;
            }
          }
          goto LAB_000a7754;
        }
        if (uVar1 == 0x174) {
          thunk_FUN_000a8b1c();
          return;
        }
        if ((int)(uVar1 - 0x174) < 0 || uVar1 == 0x174) {
          if (uVar1 == 0x172) {
            thunk_FUN_000ac738();
            return;
          }
          if ((int)(uVar1 - 0x172) < 0 || uVar1 == 0x172) {
            if (uVar1 == 0x171) {
              thunk_FUN_000ac6c4(*(undefined4 *)(unaff_gp + -0x3e40),
                                 *(undefined4 *)(unaff_gp + -0x3e3c));
              thunk_FUN_000dd846(3);
              return;
            }
          }
          else if (uVar1 == 0x173) {
            thunk_FUN_000ac810();
            return;
          }
        }
        else {
          if (uVar1 == 0x176) {
LAB_000a7734:
            thunk_FUN_000a88f8(uVar1);
            thunk_FUN_000aa380(uVar1);
            return;
          }
          if ((int)(uVar1 - 0x176) < 0 || uVar1 == 0x176) {
            if (uVar1 == 0x175) goto LAB_000a7734;
          }
          else if (uVar1 == 0x177) {
            thunk_FUN_000ac936(0x177);
            return;
          }
        }
      }
    }
  }
LAB_000a78a8:
  if (((int)(uVar1 - 0x37) < 0) || (-1 < (int)(uVar1 - 0x87))) {
    if (((int)(uVar1 - 0x87) < 0) || (-1 < (int)(uVar1 - 0x117))) {
      if (((int)(uVar1 - 0x147) < 0) || (-1 < (int)(uVar1 - 0x15f))) {
        if (((int)(uVar1 - 0x117) < 0) || (-1 < (int)(uVar1 - 0x147))) {
          if (((int)(uVar1 - 0x160) < 0) || (-1 < (int)(uVar1 - 0x168))) {
            if ((-1 < (int)(uVar1 - 0x168)) && ((int)(uVar1 - 0x170) < 0)) {
              thunk_FUN_000acfa8(uVar1 - 0x168 & unaff_r21);
            }
          }
          else {
            thunk_FUN_000ace9c(uVar1 - 0x160 & unaff_r21);
          }
        }
        else {
          thunk_FUN_000a9afa(uVar1 - 0x117 & unaff_r21);
        }
      }
      else {
        thunk_FUN_000a9a20(uVar1 - 0x147 & unaff_r21);
      }
    }
    else {
      thunk_FUN_000a9bd4(uVar1 - 0x87 & unaff_r21);
    }
  }
  else {
    thunk_FUN_000a992c(uVar1 - 0x37 & unaff_r21);
  }
  return;
}


```

## 0xa985c

```c

void FUN_000a985c(uint param_1)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uStack00000000;

  uVar1 = param_1 & 0xffff;
  if (uVar1 == unaff_r21) {
    uVar2 = 0x14;
    iVar4 = 0x14;
    uVar5 = 4;
    uVar1 = 0;
  }
  else {
    uVar2 = 1;
    iVar4 = 5;
    uVar5 = 1;
  }
  uVar3 = 0;
  uStack00000000 = param_1;
  if (uVar2 != 0) {
    do {
      thunk_FUN_000a8c14(iVar4,5,1);
      *(int *)(unaff_gp + -0x4334) = iVar4;
      uVar7 = (unaff_r20 & 4) + 1 & unaff_r20;
      *(uint *)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = uVar1 * 5 + 0xe0264;
      uVar6 = 0;
      if (uVar5 != 0) {
        do {
          thunk_FUN_000a984e(unaff_gp + -0x138f8 + uVar1 * 8);
          uVar7 = thunk_FUN_000ac05a(uVar7);
          uVar1 = uVar1 + 1 & unaff_r21;
          uVar6 = uVar6 + 1 & unaff_r20;
        } while (uVar6 < uVar5);
      }
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e35c8(unaff_gp + -0x44d8,iVar4 * 4 + 0x18U & unaff_r21);
      uVar3 = uVar3 + 1 & unaff_r20;
    } while (uVar3 < uVar2);
  }
  return;
}


```

## 0xa9b04

```c

void FUN_000a9b04(uint param_1)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uStack00000000;

  uVar1 = param_1 & 0xffff;
  if (uVar1 == unaff_r21) {
    uVar2 = 0x24;
    iVar4 = 0x14;
    uVar5 = 4;
    uVar1 = 0;
  }
  else {
    uVar2 = 1;
    iVar4 = 5;
    uVar5 = 1;
  }
  uVar3 = 0;
  uStack00000000 = param_1;
  if (uVar2 != 0) {
    do {
      thunk_FUN_000a8c14(iVar4,5,1);
      *(int *)(unaff_gp + -0x4334) = iVar4;
      uVar7 = (unaff_r20 & 4) + 1 & unaff_r20;
      *(uint *)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = uVar1 * 5 + 0xc2ab0;
      uVar6 = 0;
      if (uVar5 != 0) {
        do {
          thunk_FUN_000a984e(unaff_gp + -0x132fc + uVar1 * 8);
          uVar7 = thunk_FUN_000ac05a(uVar7);
          uVar1 = uVar1 + 1 & unaff_r21;
          uVar6 = uVar6 + 1 & unaff_r20;
        } while (uVar6 < uVar5);
      }
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e35c8(unaff_gp + -0x44d8,iVar4 * 4 + 0x18U & unaff_r21);
      uVar3 = uVar3 + 1 & unaff_r20;
    } while (uVar3 < uVar2);
  }
  return;
}


```

## 0xa9a2c

```c

void FUN_000a9a2c(uint param_1)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uStack00000000;

  uVar1 = param_1 & 0xffff;
  if (uVar1 == unaff_r21) {
    uVar2 = 0xc;
    iVar4 = 0x14;
    uVar5 = 4;
    uVar1 = 0;
  }
  else {
    uVar2 = 1;
    iVar4 = 5;
    uVar5 = 1;
  }
  uVar3 = 0;
  uStack00000000 = param_1;
  if (uVar2 != 0) {
    do {
      thunk_FUN_000a8c14(iVar4,5,1);
      *(int *)(unaff_gp + -0x4334) = iVar4;
      uVar7 = (unaff_r20 & 4) + 1 & unaff_r20;
      *(uint *)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = uVar1 * 5 + 0xe069c;
      uVar6 = 0;
      if (uVar5 != 0) {
        do {
          thunk_FUN_000a984e(unaff_gp + -0x134bc + uVar1 * 8);
          uVar7 = thunk_FUN_000ac05a(uVar7);
          uVar1 = uVar1 + 1 & unaff_r21;
          uVar6 = uVar6 + 1 & unaff_r20;
        } while (uVar6 < uVar5);
      }
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e35c8(unaff_gp + -0x44d8,iVar4 * 4 + 0x18U & unaff_r21);
      uVar3 = uVar3 + 1 & unaff_r20;
    } while (uVar3 < uVar2);
  }
  return;
}


```

## 0xa9938

```c

void FUN_000a9938(uint param_1)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uStack00000000;

  uVar1 = param_1 & 0xffff;
  if (uVar1 == unaff_r21) {
    uVar2 = 6;
    iVar3 = 0x14;
    uVar5 = 4;
    uVar1 = 0;
  }
  else {
    uVar2 = 1;
    iVar3 = 5;
    uVar5 = 1;
  }
  uVar4 = 0;
  uStack00000000 = param_1;
  if (uVar2 != 0) {
    do {
      if ((&DAT_00068a08)[uVar1] == -1) {
        uVar1 = uVar1 + uVar5 & unaff_r21;
      }
      else {
        thunk_FUN_000a8c14(iVar3,5,1);
        *(int *)(unaff_gp + -0x4334) = iVar3;
        uVar7 = (unaff_r20 & 4) + 1 & unaff_r20;
        *(undefined4 *)(unaff_gp + -0x4340 + (unaff_r20 & 4) * 4) = (&DAT_00068a08)[uVar1];
        uVar6 = 0;
        if (uVar5 != 0) {
          do {
            thunk_FUN_000a984e(unaff_gp + -0x13590 + uVar1 * 8);
            uVar7 = thunk_FUN_000ac05a(uVar7);
            uVar1 = uVar1 + 1 & unaff_r21;
            uVar6 = uVar6 + 1 & unaff_r20;
          } while (uVar6 < uVar5);
        }
        thunk_FUN_000a8d54();
        thunk_FUN_000a8cfc();
        thunk_FUN_000e35c8(unaff_gp + -0x44d8,iVar3 * 4 + 0x18U & unaff_r21);
      }
      uVar4 = uVar4 + 1 & unaff_r20;
    } while (uVar4 < uVar2);
  }
  return;
}


```

## 0xacd9c

```c

void FUN_000acd9c(uint param_1)

{
  int unaff_gp;
  uint unaff_r20;
  uint unaff_r21;
  int iVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uStack00000000;

  uVar4 = param_1 & 0xffff;
  if (uVar4 == unaff_r21) {
    iVar1 = 0x28;
    uVar2 = 0;
  }
  else {
    iVar1 = 5;
    uVar2 = uVar4;
  }
  uStack00000000 = param_1;
  thunk_FUN_000a8c14(iVar1,5,1);
  uVar3 = unaff_r20 & 4;
  *(int *)(unaff_gp + -0x4334) = iVar1;
  if (uVar4 == unaff_r21) {
    uVar4 = uVar3 + 1 & unaff_r20;
    *(undefined4 *)(uVar3 * 4 + unaff_gp + -0x4340) = 0xe4594;
    uVar3 = 0;
    do {
      thunk_FUN_000a984e(unaff_gp + -0x1258c + (uint)(byte)(&DAT_0006bd2c)[uVar2] * 8);
      uVar4 = thunk_FUN_000ac05a(uVar4);
      uVar2 = uVar2 + 1 & unaff_r21;
      uVar3 = uVar3 + 1 & unaff_r20;
    } while ((int)(uVar3 - 8) < 0 != ((int)uVar3 < 0 && -1 < (int)(uVar3 - 8)));
  }
  else {
    *(uint *)(uVar3 * 4 + unaff_gp + -0x4340) = (uint)(byte)(&DAT_0006bd2c)[uVar2] * 5 + 0xe4594;
    thunk_FUN_000a984e(unaff_gp + -0x1258c + uVar2 * 8);
    thunk_FUN_000ac05a(uVar3 + 1 & unaff_r20);
  }
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,iVar1 * 4 + 0x18U & unaff_r21);
  return;
}


```

## 0xacea8

```c

void FUN_000acea8(uint param_1)

{
  int unaff_gp;
  uint unaff_r20;
  uint unaff_r21;
  int iVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uStack00000000;

  uVar4 = param_1 & 0xffff;
  if (uVar4 == unaff_r21) {
    iVar1 = 0x28;
    uVar2 = 0;
  }
  else {
    iVar1 = 5;
    uVar2 = uVar4;
  }
  uStack00000000 = param_1;
  thunk_FUN_000a8c14(iVar1,5,1);
  uVar3 = unaff_r20 & 4;
  *(int *)(unaff_gp + -0x4334) = iVar1;
  if (uVar4 == unaff_r21) {
    uVar4 = uVar3 + 1 & unaff_r20;
    *(undefined4 *)(uVar3 * 4 + unaff_gp + -0x4340) = 0xe463c;
    uVar3 = 0;
    do {
      thunk_FUN_000a984e(unaff_gp + -0x1254c + (uint)(byte)(&DAT_0006bd2c)[uVar2] * 8);
      uVar4 = thunk_FUN_000ac05a(uVar4);
      uVar2 = uVar2 + 1 & unaff_r21;
      uVar3 = uVar3 + 1 & unaff_r20;
    } while ((int)(uVar3 - 8) < 0 != ((int)uVar3 < 0 && -1 < (int)(uVar3 - 8)));
  }
  else {
    *(uint *)(uVar3 * 4 + unaff_gp + -0x4340) = (uint)(byte)(&DAT_0006bd2c)[uVar2] * 5 + 0xe463c;
    thunk_FUN_000a984e(unaff_gp + -0x1254c + uVar2 * 8);
    thunk_FUN_000ac05a(uVar3 + 1 & unaff_r20);
  }
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,iVar1 * 4 + 0x18U & unaff_r21);
  return;
}


```

## 0xa8548

```c

void FUN_000a8548(void)

{
  byte bVar1;
  int unaff_gp;
  undefined1 uVar2;
  int iVar3;
  uint uVar4;
  int extraout_r11;
  uint extraout_r11_00;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  undefined4 *puVar8;
  undefined4 uVar9;
  uint unaff_r20;
  uint uVar10;
  uint uVar11;
  uint in_PSW;
  uint uVar12;
  undefined8 uVar13;

  uVar2 = thunk_FUN_0009afdc();
  *(undefined1 *)(unaff_gp + -0x3e2b) = uVar2;
  if (*(byte *)(unaff_gp + -0x3e2b) < 0x46) {
    uVar10 = 0;
  }
  else {
    uVar10 = (int)(*(byte *)(unaff_gp + -0x3e2b) - 0x46) / 5 & unaff_r20;
  }
  in_PSW = in_PSW & 0xfffffff0;
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)(unaff_gp + -0x4330) = 0xe46c8;
  iVar3 = thunk_FUN_0009b08e();
  uVar6 = (uint)(iVar3 == 0x5a);
  uVar4 = 0;
  iVar3 = unaff_gp + -0x13678;
  do {
    uVar5 = (uint)(byte)(&DAT_00068b40)[uVar4];
    if (*(char *)(uVar4 * 4 + iVar3) == -1) {
      in_PSW = in_PSW & 0xfffffff0;
      *(undefined4 *)(unaff_gp + -0x4340 + (uVar5 + 5) * 4) = 0x42c80000;
    }
    else {
      uVar11 = (int)*(char *)(uVar4 * 4 + iVar3) / 5;
      uVar7 = (uint)*(byte *)(unaff_gp + -0x41aa);
      bVar1 = -(char)((int)uVar11 >> 0x1f);
      uVar12 = in_PSW & 0xfffffff0 | (uint)(uVar7 == uVar11);
      if ((int)(uVar7 - uVar11) < 0 != (bVar1 != 0 && bVar1 == (byte)(uVar7 - uVar11 >> 0x1f)) ||
          (bool)((byte)uVar12 & 1)) {
        uVar11 = (uint)*(byte *)(unaff_gp + -0x41aa);
      }
      else {
        uVar11 = (int)*(char *)(uVar4 * 4 + iVar3) / 5 & unaff_r20;
        uVar12 = in_PSW & 0xfffffff0;
      }
      uVar4 = (uint)*(byte *)(unaff_gp + -0x12957 + uVar4) * 100 + 0x6a3fc;
      uVar7 = uVar10 * 4;
      in_PSW = *(uint *)(uVar4 + uVar7);
      FUN_000ebaec(in_PSW,*(undefined4 *)(&DAT_0006a528 + uVar11 * 4),
                   uVar12 & 0xfffffff0 | (uint)CARRY4(uVar7,uVar4) << 3 |
                   (uint)SCARRY4(uVar7,uVar4) << 2 | (uint)((int)(uVar7 + uVar4) < 0) << 1 |
                   (uint)(uVar7 + uVar4 == 0));
      if ((in_PSW >> 3 & 1) == 0) {
        in_PSW = in_PSW & 0xfffffff0;
        uVar9 = *(undefined4 *)(uVar6 * 0x40 + 0x6a37c + extraout_r11 * 4);
        uVar13 = FUN_000eb984(uVar9,*(undefined4 *)
                                     ((uint)*(byte *)(unaff_gp + -0x12957 + extraout_r11) * 100 +
                                      0x6a3fc + uVar10 * 4));
        uVar4 = (uint)uVar13;
        *(undefined4 *)((ulonglong)uVar13 >> 0x20) = uVar9;
      }
      else {
        puVar8 = (undefined4 *)(unaff_gp + -0x4340 + (uVar5 + 5) * 4);
        in_PSW = in_PSW & 0xfffffff0;
        uVar9 = *(undefined4 *)(uVar6 * 0x40 + 0x6a37c + extraout_r11 * 4);
        FUN_000eb984(uVar9,*(undefined4 *)(&DAT_0006a528 + uVar11 * 4));
        *puVar8 = uVar9;
        uVar4 = extraout_r11_00;
      }
    }
    uVar4 = uVar4 + 1 & unaff_r20;
  } while ((int)(uVar4 - 0xf) < 0 != ((int)uVar4 < 0 && -1 < (int)(uVar4 - 0xf)) || uVar4 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  return;
}


```

## 0xab6a6

```c

void FUN_000ab6a6(void)

{
  int unaff_gp;
  uint unaff_r20;
  int iVar1;
  uint uVar2;
  uint uVar3;

  iVar1 = 3;
  thunk_FUN_000a8c14(5,5,2);
  uVar3 = 0;
  do {
    uVar2 = iVar1 + 1U & unaff_r20;
    *(undefined4 *)(iVar1 * 4 + unaff_gp + -0x4340) = 5;
    *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = (&DAT_00068a70)[uVar3];
    thunk_FUN_000a984e(unaff_gp + -0x1285c + uVar3 * 8);
    iVar1 = thunk_FUN_000ac05a(uVar2 + 1 & unaff_r20);
    uVar3 = uVar3 + 1 & unaff_r20;
  } while ((int)(uVar3 - 2) < 0 != ((int)uVar3 < 0 && -1 < (int)(uVar3 - 2)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x48);
  return;
}


```

## 0xa8b26

```c

void FUN_000a8b26(void)

{
  int unaff_gp;
  uint uVar1;
  uint uVar2;
  uint unaff_r20;

  thunk_FUN_000a8c14(8,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 8;
  uVar2 = 5;
  *(undefined4 *)(unaff_gp + -0x4330) = 0xe0082;
  uVar1 = 0;
  do {
    if (*(char *)(unaff_gp + -0x3e2a) == -0x5b) {
      *(uint *)(unaff_gp + -0x4340 + uVar2 * 4) = (uint)*(byte *)(unaff_gp + -0x3e29);
    }
    else {
      *(undefined4 *)(unaff_gp + -0x4340 + uVar2 * 4) = 0;
    }
    uVar2 = uVar2 + 1 & unaff_r20;
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 8) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 8)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x38);
  return;
}


```

## 0xac464

```c

void FUN_000ac464(void)

{
  char cVar1;
  int unaff_gp;
  int iVar2;
  uint uVar3;
  uint uVar4;
  uint unaff_r20;
  uint unaff_r21;

  thunk_FUN_000a8c14(0x20,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x20;
  uVar4 = 5;
  *(undefined4 *)(unaff_gp + -0x4330) = 0xc39aa;
  uVar3 = 0;
  do {
    *(undefined4 *)(uVar4 * 4 + unaff_gp + -0x4340) = (&DAT_00068c98)[uVar3];
    uVar4 = uVar4 + 1 & unaff_r20;
    uVar3 = uVar3 + 1 & unaff_r21;
  } while (uVar3 < 0x20);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,unaff_r21 & 0x98);
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  uVar4 = 5;
  *(undefined4 *)(unaff_gp + -0x4330) = 0xc393a;
  uVar3 = 0;
  do {
    cVar1 = *(char *)((uint)(byte)(&DAT_00068b30)[uVar3] + unaff_gp + -0x12967);
    iVar2 = (int)cVar1;
    if (iVar2 + -0x28 < 0 == (cVar1 >> 7 < '\0' && -1 < iVar2 + -0x28) && iVar2 != 0x28) {
      iVar2 = 0x28;
    }
    if (iVar2 + 100 < 0 != (-1 < iVar2 && iVar2 + 100 < 0)) {
      iVar2 = -100;
    }
    *(undefined4 *)(unaff_gp + -0x4340 + uVar4 * 4) =
         *(undefined4 *)(&DAT_00069399 + ~(iVar2 * 0x28));
    uVar4 = uVar4 + 1 & unaff_r20;
    uVar3 = uVar3 + 1 & unaff_r20;
  } while ((int)(uVar3 - 0x10) < 0 != ((int)uVar3 < 0 && -1 < (int)(uVar3 - 0x10)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,unaff_r21 & 0x58);
  return;
}


```

## 0xaa648

```c

void FUN_000aa648(void)

{
  byte bVar1;
  char cVar2;
  char cVar3;
  int unaff_gp;
  short sVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  uint unaff_r20;
  uint uVar10;
  uint uVar11;

  if ((*(char *)(unaff_gp + -0x456b) == -0x5b) && (*(char *)(unaff_gp + -0x455c) == -0x5b)) {
    uVar10 = (int)*(char *)(unaff_gp + -0x418f) + 1U & unaff_r20;
  }
  else {
    uVar10 = 0;
  }
  iVar6 = 0xa5;
  uVar5 = 0;
  do {
    if (*(char *)(unaff_gp + -0x4190) == 'Z') {
      *(undefined1 *)(unaff_gp + -0x41a4 + uVar5) =
           *(undefined1 *)(unaff_gp + -0x135f8 + uVar10 * 0x14 + uVar5);
    }
    else if (*(char *)(unaff_gp + -0x4190) == -0x56) {
      iVar7 = unaff_gp + -0x41a4;
      iVar9 = (int)*(char *)(iVar7 + uVar5);
      cVar2 = *(char *)(uVar10 * 0x14 + unaff_gp + -0x135f8 + uVar5);
      iVar8 = (int)cVar2;
      bVar1 = (byte)(cVar2 >> 7) >> 7;
      if (iVar9 - iVar8 < 0 ==
          ((byte)(*(char *)(iVar7 + uVar5) >> 7) >> 7 != bVar1 &&
          bVar1 == (byte)((uint)(iVar9 - iVar8) >> 0x1f)) && iVar9 != iVar8) {
        *(char *)(iVar7 + uVar5) = *(char *)(iVar7 + uVar5) + -1;
        iVar6 = 0x5a;
      }
      else {
        cVar2 = *(char *)(iVar7 + uVar5);
        cVar3 = *(char *)(uVar10 * 0x14 + unaff_gp + -0x135f8 + uVar5);
        bVar1 = (byte)(cVar3 >> 7) >> 7;
        if ((int)cVar2 - (int)cVar3 < 0 !=
            ((byte)(cVar2 >> 7) >> 7 != bVar1 &&
            bVar1 == (byte)((uint)((int)cVar2 - (int)cVar3) >> 0x1f))) {
          *(char *)(iVar7 + uVar5) = *(char *)(iVar7 + uVar5) + '\x01';
          iVar6 = 0x5a;
        }
      }
    }
    else if (*(byte *)(unaff_gp + -0x4190) == unaff_r20) {
      if ((uVar5 & 1) == 1) {
        *(undefined1 *)(unaff_gp + -0x41a4 + uVar5) = 0x9c;
      }
      else {
        *(undefined1 *)(unaff_gp + -0x41a4 + uVar5) =
             *(undefined1 *)(unaff_gp + -0x135f8 + uVar10 * 0x14 + uVar5);
      }
    }
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x14) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x14)));
  if ((*(char *)(unaff_gp + -0x4190) == -0x56) && (iVar6 == 0xa5)) {
    *(undefined1 *)(unaff_gp + -0x4190) = 0x5a;
  }
  thunk_FUN_000a8c14(0x14,5,1);
  uVar11 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x14;
  *(undefined4 *)(uVar11 * 4 + unaff_gp + -0x4340) = 0xe0584;
  uVar5 = 0;
  do {
    uVar11 = uVar11 + 1 & unaff_r20;
    iVar6 = *(char *)(unaff_gp + -0x41a4 + uVar5) * 10;
    if (((0 < (int)uVar10) && (*(char *)(unaff_gp + -0x3e21) == -0x40)) && ((uVar5 & 1) == 1)) {
      sVar4 = (short)iVar6 + *(short *)(unaff_gp + *(char *)(unaff_gp + -0x3de0) * 2 + -0x12aba);
      iVar6 = (int)sVar4;
      if (iVar6 + 0x3de < 0 != (-1 < (char)(sVar4 >> 0xf) && iVar6 + 0x3de < 0)) {
        iVar6 = -0x3de;
      }
      if (iVar6 + -200 < 0 == (iVar6 < 0 && -1 < iVar6 + -200) && iVar6 != 200) {
        iVar6 = 200;
      }
    }
    *(undefined4 *)(uVar11 * 4 + unaff_gp + -0x4340) =
         *(undefined4 *)(&DAT_00069399 + ~(iVar6 << 2));
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x14) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x14)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x68);
  iVar6 = (int)*(char *)(unaff_gp + -0x3de0);
  FUN_000ebca4();
  thunk_FUN_000ac6c4(0x12,iVar6);
  return;
}


```

## 0xac93e

```c

void FUN_000ac93e(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;

  thunk_FUN_000a8c14(8,5,1);
  uVar2 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 8;
  *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0xe01a4;
  uVar1 = 0;
  do {
    uVar2 = uVar2 + 1 & unaff_r20;
    *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 2;
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 8) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 8)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x38);
  return;
}


```

## 0xac9c4

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

## 0xacaa0

```c

void FUN_000acaa0(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  int iVar2;
  uint uVar3;
  int iVar4;
  int iVar5;

  thunk_FUN_000a8c14(0x10,5,1);
  iVar5 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar5) = 0xe45cc;
  uVar3 = (unaff_r20 & 4) + 1 & unaff_r20;
  uVar1 = 0;
  iVar4 = unaff_gp + -0x125cc;
  do {
    if (*(char *)(uVar1 * 4 + iVar4) == -1) {
      *(undefined4 *)((uVar3 + (byte)(&DAT_00068b40)[uVar1]) * 4 + iVar5) = 0x42c80000;
    }
    else {
      *(undefined4 *)((uVar3 + (byte)(&DAT_00068b40)[uVar1]) * 4 + iVar5) =
           *(undefined4 *)(&DAT_0006a528 + ((int)*(char *)(uVar1 * 4 + iVar4) / 5 & unaff_r20) * 4);
    }
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  iVar2 = unaff_gp + -0x44d8;
  thunk_FUN_000e35c8(iVar2,0x58);
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar5) = 0xe45dc;
  uVar1 = 0;
  do {
    *(undefined4 *)
     (((uint)(byte)(&DAT_00068b40)[uVar1] + ((unaff_r20 & 4) + 1 & unaff_r20)) * 4 + iVar5) =
         *(undefined4 *)(&DAT_0006a980 + (uint)*(byte *)(uVar1 * 4 + iVar4 + 1) * 4);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(iVar2,0x58);
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar5) = 0xe45ec;
  uVar1 = 0;
  do {
    *(undefined4 *)
     (((uint)(byte)(&DAT_00068b40)[uVar1] + ((unaff_r20 & 4) + 1 & unaff_r20)) * 4 + iVar5) =
         *(undefined4 *)(&DAT_0006ad7c + (uint)*(ushort *)(uVar1 * 4 + iVar4 + 2) * 4);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(iVar2,0x58);
  return;
}


```

## 0xacc84

```c

void FUN_000acc84(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  int iVar2;

  thunk_FUN_000a8c14(0x10,5,1);
  iVar2 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xe4574;
  uVar1 = 0;
  do {
    *(uint *)(((uint)(byte)(&DAT_00068b40)[uVar1] + ((unaff_r20 & 4) + 1 & unaff_r20)) * 4 + iVar2)
         = (uint)(byte)(&DAT_0006bd1c)[uVar1];
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xe461c;
  uVar1 = 0;
  do {
    *(uint *)(((uint)(byte)(&DAT_00068b40)[uVar1] + ((unaff_r20 & 4) + 1 & unaff_r20)) * 4 + iVar2)
         = (uint)(byte)(&DAT_0006bd1c)[uVar1];
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  return;
}


```

## 0xacfb4

```c

void FUN_000acfb4(void)

{
  byte bVar1;
  int unaff_gp;
  uint uVar2;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar3;
  uint uVar4;
  uint uVar5;

  if (((*(char *)(unaff_gp + -0x455c) == -0x5b) && (DAT_00120008 != 0)) && (DAT_00120008 < 0x1f41))
  {
    uVar3 = 0;
    do {
      if (DAT_00120008 < uVar3 + 0x50) {
        uVar4 = DAT_00120008 - uVar3 & unaff_r21;
      }
      else {
        uVar4 = 0x50;
      }
      thunk_FUN_000a8c14(uVar4,5,1);
      uVar5 = unaff_r20 & 4;
      *(uint *)(unaff_gp + -0x4334) = uVar4;
      *(uint *)(uVar5 * 4 + unaff_gp + -0x4340) = uVar3 + 0xc3c6a;
      uVar2 = 0;
      if (0 < (int)uVar4) {
        do {
          uVar5 = uVar5 + 1 & unaff_r20;
          *(undefined4 *)(uVar5 * 4 + unaff_gp + -0x4340) = (&DAT_0012000c)[uVar3];
          uVar3 = uVar3 + 1;
          uVar2 = uVar2 + 1 & unaff_r20;
          bVar1 = (byte)(uVar4 >> 0x1f);
        } while ((int)(uVar2 - uVar4) < 0 !=
                 ((byte)(uVar2 >> 0x1f) != bVar1 && bVar1 == (byte)(uVar2 - uVar4 >> 0x1f)));
      }
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e35c8(unaff_gp + -0x44d8,uVar4 * 4 + 0x18 & unaff_r21);
    } while (uVar3 < DAT_00120008);
  }
  return;
}


```

## 0xa9c94

```c

void FUN_000a9c94(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;
  uint uVar3;
  int iVar4;

  uVar2 = 0;
  do {
    thunk_FUN_000a8c14(0x10,5,1);
    iVar4 = unaff_gp + -0x4340;
    uVar3 = unaff_r20 & 4;
    *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
    *(undefined4 *)(uVar3 * 4 + iVar4) = (&DAT_00068a68)[uVar2];
    uVar1 = 0;
    do {
      uVar3 = uVar3 + 1 & unaff_r20;
      if (uVar2 == 0) {
        *(uint *)(uVar3 * 4 + iVar4) =
             (uint)*(ushort *)(unaff_gp + -0x12e7c + (uint)(byte)(&DAT_00068b30)[uVar1] * 2);
      }
      else {
        *(uint *)(uVar3 * 4 + iVar4) = (uint)*(ushort *)(unaff_gp + -0x1332c + uVar1 * 2);
      }
      uVar1 = uVar1 + 1 & unaff_r20;
    } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
    thunk_FUN_000a8d54();
    thunk_FUN_000a8cfc();
    thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
    uVar2 = uVar2 + 1 & unaff_r20;
  } while ((int)(uVar2 - 2) < 0 != ((int)uVar2 < 0 && -1 < (int)(uVar2 - 2)));
  return;
}


```

## 0xa9eca

```c

void FUN_000a9eca(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;

  thunk_FUN_000a8c14(8,5,1);
  uVar2 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 8;
  *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0xe0144;
  uVar1 = 0;
  do {
    uVar2 = uVar2 + 1 & unaff_r20;
    *(uint *)(uVar2 * 4 + unaff_gp + -0x4340) = (uint)*(ushort *)(uVar1 * 2 + unaff_gp + -0x13638);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 8) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 8)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x38);
  return;
}


```

## 0xa9f5e

```c

void FUN_000a9f5e(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;

  thunk_FUN_000a8c14(8,5,1);
  uVar2 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 8;
  *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0xe0178;
  uVar1 = 0;
  do {
    uVar2 = uVar2 + 1 & unaff_r20;
    *(uint *)(uVar2 * 4 + unaff_gp + -0x4340) = (uint)*(ushort *)(uVar1 * 2 + unaff_gp + -0x13628);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 8) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 8)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x38);
  return;
}


```

## 0xa9ff4

```c

void FUN_000a9ff4(void)

{
  byte bVar1;
  int unaff_gp;
  int iVar2;
  undefined4 uVar3;
  uint uVar4;
  uint uVar5;
  uint unaff_r20;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  int iVar9;

  uVar6 = 0;
  do {
    thunk_FUN_000a8c14(0x10,5,1);
    iVar9 = unaff_gp + -0x4340;
    uVar7 = unaff_r20 & 4;
    *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
    *(code **)(uVar7 * 4 + iVar9) = FUN_000e453a + uVar6 * 0x10;
    uVar4 = uVar6 << 3 & unaff_r20;
    uVar5 = uVar4;
    do {
      uVar8 = uVar7 + 1 & unaff_r20;
      if (*(char *)(unaff_gp + -0x13608 + uVar4) == -0x80) {
        uVar3 = 0;
      }
      else {
        uVar3 = *(undefined4 *)(&DAT_00069399 + ~((int)*(char *)(unaff_gp + -0x13608 + uVar4) << 3))
        ;
        if (*(char *)(unaff_gp + -0x13618 + uVar4) == 'Z') {
          FUN_000ebb70(uVar3,0);
        }
      }
      uVar7 = uVar8 + 1 & unaff_r20;
      *(undefined4 *)(uVar8 * 4 + iVar9) = uVar3;
      if (*(char *)(unaff_gp + -0x1333c + uVar4) == -0x80) {
        uVar3 = 0;
      }
      else {
        uVar3 = *(undefined4 *)(&DAT_00069399 + ~((int)*(char *)(unaff_gp + -0x1333c + uVar4) << 3))
        ;
      }
      *(undefined4 *)(uVar7 * 4 + iVar9) = uVar3;
      uVar4 = uVar4 + 1 & unaff_r20;
      iVar2 = uVar5 + 8;
      bVar1 = (byte)((uint)iVar2 >> 0x1f);
    } while ((int)(uVar4 - iVar2) < 0 !=
             ((byte)(uVar4 >> 0x1f) != bVar1 && bVar1 == (byte)(uVar4 - iVar2 >> 0x1f)));
    thunk_FUN_000a8d54();
    thunk_FUN_000a8cfc();
    thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
    uVar6 = uVar6 + 1 & unaff_r20;
  } while ((int)(uVar6 - 2) < 0 != ((int)uVar6 < 0 && -1 < (int)(uVar6 - 2)));
  return;
}


```

## 0xa8238

```c

void FUN_000a8238(void)

{
  uint uVar1;
  int unaff_gp;
  undefined1 uVar2;
  undefined4 uVar3;
  uint uVar4;
  int extraout_r11;
  int extraout_r11_00;
  int iVar5;
  undefined4 *puVar6;
  undefined4 *extraout_r11_01;
  undefined4 *extraout_r11_02;
  uint uVar7;
  uint uVar8;
  undefined1 *puVar9;
  uint uVar10;
  uint unaff_r20;
  uint uVar11;
  uint in_PSW;

  *(char *)(unaff_gp + -0x41a8) = *(char *)(unaff_gp + -0x41a8) + '\x01';
  if ((*(char *)(unaff_gp + -0x41a8) == '\x02') || (*(char *)(unaff_gp + -0x41a8) == '\a')) {
    uVar4 = in_PSW & 0xfffffff0 | (uint)(*(char *)(unaff_gp + -0x4571) == '\0');
    if (((byte)uVar4 & 1) == 1) {
      thunk_FUN_000a8c64(0xc3a2a,0x10);
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e36d2(unaff_gp + -0x44d8,unaff_gp + -0x4340);
      uVar8 = 0;
      puVar9 = &DAT_00068b40;
      do {
        uVar11 = uVar8 * 4;
        uVar7 = uVar4 & 0xfffffff0 | (uint)(0xfff95cc3 < uVar11) << 3 |
                (uint)SCARRY4(uVar11,0x6a33c) << 2 | (uint)((int)(&DAT_0006a33c + uVar8) < 0) << 1 |
                (uint)(uVar11 == 0xfff95cc4);
        uVar3 = (&DAT_0006a33c)[uVar8];
        FUN_000eb984(uVar3,*(undefined4 *)(unaff_gp + -0x4340 + (uint)(byte)puVar9[uVar8] * 4));
        uVar8 = 0x3f000000;
        FUN_000eba38(0x3f000000,uVar3);
        uVar4 = 0x4f000000;
        uVar11 = uVar4;
        FUN_000ebaec(0x4f000000,uVar8,uVar7);
        if (((byte)(uVar4 >> 1) & 1) == ((byte)(uVar4 >> 2) & 1)) {
          FUN_000ebb70(uVar11,uVar8);
          FUN_000ebb82();
          uVar8 = uVar11 | 0x80000000;
          uVar4 = uVar4 & 0xfffffff8;
          iVar5 = extraout_r11;
        }
        else {
          FUN_000ebb82();
          iVar5 = extraout_r11_00;
        }
        if (uVar8 < 0x100) {
          *(char *)(unaff_gp + -0x41ba + iVar5) = (char)uVar8;
        }
        else {
          *(char *)(unaff_gp + -0x41ba + iVar5) = (char)unaff_r20;
        }
        uVar8 = iVar5 + 1U & unaff_r20;
        uVar7 = uVar4 & 0xfffffff0 | (uint)((int)uVar8 < 0 && -1 < (int)(uVar8 - 0xf)) << 2;
        uVar11 = uVar7 | (uint)((int)(uVar8 - 0xf) < 0) << 1;
        uVar4 = uVar11 | uVar8 == 0xf;
      } while (((byte)(uVar11 >> 1) & 1) != ((byte)(uVar7 >> 2) & 1) || (bool)((byte)uVar4 & 1));
      *(undefined2 *)(unaff_gp + -0x41bc) = 0x14;
      uVar4 = 0;
      do {
        *(ushort *)(unaff_gp + -0x41bc) =
             (ushort)*(byte *)(unaff_gp + -0x41ba + uVar4) + *(short *)(unaff_gp + -0x41bc);
        uVar4 = uVar4 + 1 & unaff_r20;
      } while ((int)(uVar4 - 0xf) < 0 != ((int)uVar4 < 0 && -1 < (int)(uVar4 - 0xf)) || uVar4 == 0xf
              );
      *(char *)(unaff_gp + -0x5e07) =
           (char)(((uint)*(byte *)(unaff_gp + -0x5e07) * 0x13 +
                  ((uint)(int)*(short *)(unaff_gp + -0x41bc) >> 1)) / 0x14);
      if (*(char *)(unaff_gp + -0x4571) == '\0') {
        thunk_FUN_000a853c();
        thunk_FUN_000a8704();
      }
    }
  }
  else {
    uVar4 = in_PSW & 0xfffffff0;
    if (9 < *(byte *)(unaff_gp + -0x41a8)) {
      *(undefined1 *)(unaff_gp + -0x41a8) = 0;
      thunk_FUN_000a8c64(0xc0a06,5);
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      uVar11 = unaff_gp - 0x4340;
      thunk_FUN_000e36d2(unaff_gp + -0x44d8,uVar11);
      *(undefined1 *)(unaff_gp + -0x3e2d) = 0;
      uVar8 = 0;
      do {
        uVar7 = (int)*(char *)(unaff_gp + -0x45c1) + 10U & unaff_r20;
        uVar4 = uVar4 & 0xfffffff0;
        puVar6 = &DAT_00068d58;
        do {
          uVar7 = uVar7 - 10 & unaff_r20;
          uVar10 = uVar8 * 4;
          uVar1 = uVar4 & 0xfffffff0;
          uVar4 = *(uint *)(uVar10 + uVar11);
          FUN_000ebaec(uVar4,*(undefined4 *)((int)puVar6 + ~(uVar7 << 2) + 0x641),
                       uVar1 | (uint)CARRY4(uVar11,uVar10) << 3 | (uint)SCARRY4(uVar11,uVar10) << 2
                       | (uint)((int)(uVar11 + uVar10) < 0) << 1 | (uint)(uVar11 + uVar10 == 0));
          if ((bool)((byte)(uVar4 >> 3) & 1) || (bool)((byte)uVar4 & 1)) break;
          uVar1 = uVar4 & 0xfffffff0 | (uint)((int)uVar7 < 0 && -1 < (int)(uVar7 - 10)) << 2;
          uVar4 = uVar1 | (uint)((int)(uVar7 - 10) < 0) << 1;
          puVar6 = extraout_r11_01;
        } while (((byte)(uVar4 >> 1) & 1) == ((byte)(uVar1 >> 2) & 1));
        uVar7 = uVar7 + 10 & unaff_r20;
        uVar4 = uVar4 & 0xfffffff0;
        puVar6 = extraout_r11_01;
        do {
          uVar7 = uVar7 - 1 & unaff_r20;
          uVar10 = uVar8 * 4;
          uVar1 = uVar4 & 0xfffffff0;
          uVar4 = *(uint *)(uVar10 + uVar11);
          FUN_000ebaec(uVar4,*(undefined4 *)((int)puVar6 + ~(uVar7 << 2) + 0x641),
                       uVar1 | (uint)CARRY4(uVar11,uVar10) << 3 | (uint)SCARRY4(uVar11,uVar10) << 2
                       | (uint)((int)(uVar11 + uVar10) < 0) << 1 | (uint)(uVar11 + uVar10 == 0));
          if ((bool)((byte)(uVar4 >> 3) & 1) || (bool)((byte)uVar4 & 1)) break;
          uVar4 = uVar4 & 0xfffffff0 | (uint)(uVar7 == 0);
          puVar6 = extraout_r11_02;
        } while (-1 < (int)uVar7 && !(bool)((byte)uVar4 & 1));
        if (*(byte *)(unaff_gp + -0x3e2d) < uVar7) {
          *(char *)(unaff_gp + -0x3e2d) = (char)uVar7;
        }
        uVar8 = uVar8 + 1 & unaff_r20;
        uVar7 = uVar4 & 0xfffffff0 | (uint)((int)uVar8 < 0 && -1 < (int)(uVar8 - 5)) << 2;
        uVar4 = uVar7 | (uint)((int)(uVar8 - 5) < 0) << 1;
      } while (((byte)(uVar4 >> 1) & 1) != ((byte)(uVar7 >> 2) & 1));
      thunk_FUN_000c39b6(0x24);
      thunk_FUN_000a8c64(0xc20e0,1);
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e36d2(unaff_gp + -0x44d8,uVar11);
      uVar3 = 0x3fb33333;
      FUN_000eb984(0x3fb33333,*(undefined4 *)(unaff_gp + -0x4340));
      uVar2 = 0;
      FUN_000eb984(0x42c80000,uVar3);
      FUN_000ebb82();
      *(undefined1 *)(unaff_gp + -0x3e2c) = uVar2;
    }
  }
  return;
}


```

## 0xaa144

```c

void FUN_000aa144(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  int iVar2;

  thunk_FUN_000a8c14(0x10,5,1);
  iVar2 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xe46d8;
  uVar1 = 0;
  do {
    *(undefined4 *)
     ((((unaff_r20 & 4) + 1 & unaff_r20) + (uint)(byte)(&DAT_00068b40)[uVar1]) * 4 + iVar2) =
         *(undefined4 *)(&DAT_0006a980 + (uint)*(byte *)(uVar1 * 4 + unaff_gp + -0x13678 + 1) * 4);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xe46e8;
  uVar1 = 0;
  do {
    *(undefined4 *)
     ((((unaff_r20 & 4) + 1 & unaff_r20) + (uint)(byte)(&DAT_00068b40)[uVar1]) * 4 + iVar2) =
         *(undefined4 *)(&DAT_0006ad7c + (uint)*(ushort *)(uVar1 * 4 + unaff_gp + -0x13678 + 2) * 4)
    ;
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  return;
}


```

## 0xaa27c

```c

void FUN_000aa27c(undefined4 param_1)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  int iVar2;
  undefined4 uStack00000000;

  uStack00000000 = param_1;
  thunk_FUN_000a8c14(0x10,5,1);
  iVar2 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xe46d8;
  uVar1 = 0;
  do {
    *(undefined4 *)
     ((((unaff_r20 & 4) + 1 & unaff_r20) + (uint)(byte)(&DAT_00068b40)[uVar1]) * 4 + iVar2) =
         DAT_0006aa48;
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xe46e8;
  uVar1 = 0;
  do {
    *(undefined4 *)
     ((((unaff_r20 & 4) + 1 & unaff_r20) + (uint)(byte)(&DAT_00068b40)[uVar1]) * 4 + iVar2) =
         DAT_0006b86c;
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  return;
}


```

## 0xa8710

```c

void FUN_000a8710(undefined4 param_1)

{
  byte bVar1;
  short sVar2;
  int unaff_gp;
  undefined1 uVar3;
  int iVar4;
  uint uVar5;
  int extraout_r11;
  uint extraout_r11_00;
  uint uVar6;
  uint uVar7;
  undefined1 *puVar8;
  undefined4 *puVar9;
  uint uVar10;
  undefined4 uVar11;
  uint unaff_r20;
  uint uVar12;
  uint uVar13;
  uint in_PSW;
  uint uVar14;
  undefined8 uVar15;
  short sStack00000000;

  sStack00000000 = (short)param_1;
  sVar2 = sStack00000000;
  _sStack00000000 = param_1;
  uVar3 = thunk_FUN_0009afdc();
  *(undefined1 *)(unaff_gp + -0x3e2b) = uVar3;
  if (*(byte *)(unaff_gp + -0x3e2b) < 0x46) {
    uVar13 = 0;
  }
  else {
    uVar13 = (int)(*(byte *)(unaff_gp + -0x3e2b) - 0x46) / 5 & unaff_r20;
  }
  in_PSW = in_PSW & 0xfffffff0;
  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)(unaff_gp + -0x4330) = 0xe46c8;
  iVar4 = thunk_FUN_0009b08e();
  uVar7 = (uint)(iVar4 == 0x5a);
  uVar5 = 0;
  puVar8 = &DAT_00068b40;
  do {
    uVar6 = (uint)(byte)puVar8[uVar5];
    if ((sVar2 == 0x176) || (*(char *)(unaff_gp + -0x3e53 + uVar5) != '\x01')) {
      in_PSW = in_PSW & 0xfffffff0;
      *(undefined4 *)(unaff_gp + -0x4340 + (uVar6 + 5) * 4) = 0x1fec1e4a;
    }
    else {
      uVar12 = (int)*(char *)(unaff_gp + -0x13678 + uVar5 * 4) / 5;
      uVar10 = (uint)*(byte *)(unaff_gp + -0x41aa);
      bVar1 = -(char)((int)uVar12 >> 0x1f);
      uVar14 = in_PSW & 0xfffffff0 | (uint)(uVar10 == uVar12);
      if ((int)(uVar10 - uVar12) < 0 != (bVar1 != 0 && bVar1 == (byte)(uVar10 - uVar12 >> 0x1f)) ||
          (bool)((byte)uVar14 & 1)) {
        uVar12 = (uint)*(byte *)(unaff_gp + -0x41aa);
      }
      else {
        uVar12 = (int)*(char *)(unaff_gp + -0x13678 + uVar5 * 4) / 5 & unaff_r20;
        uVar14 = in_PSW & 0xfffffff0;
      }
      uVar10 = (uint)*(byte *)(unaff_gp + -0x12957 + uVar5) * 100 + 0x6a3fc;
      uVar5 = uVar13 * 4;
      in_PSW = *(uint *)(uVar10 + uVar5);
      FUN_000ebaec(in_PSW,*(undefined4 *)(&DAT_0006a528 + uVar12 * 4),
                   uVar14 & 0xfffffff0 | (uint)CARRY4(uVar5,uVar10) << 3 |
                   (uint)SCARRY4(uVar5,uVar10) << 2 | (uint)((int)(uVar5 + uVar10) < 0) << 1 |
                   (uint)(uVar5 + uVar10 == 0));
      if ((in_PSW >> 3 & 1) == 0) {
        puVar9 = (undefined4 *)(unaff_gp + -0x4340 + (uVar6 + 5) * 4);
        in_PSW = in_PSW & 0xfffffff0;
        uVar11 = *(undefined4 *)(uVar7 * 0x40 + 0x6a37c + extraout_r11 * 4);
        FUN_000eb984(uVar11,*(undefined4 *)
                             ((uint)*(byte *)(unaff_gp + -0x12957 + extraout_r11) * 100 + 0x6a3fc +
                             uVar13 * 4));
        *puVar9 = uVar11;
        uVar5 = extraout_r11_00;
      }
      else {
        in_PSW = in_PSW & 0xfffffff0;
        uVar11 = *(undefined4 *)(uVar7 * 0x40 + 0x6a37c + extraout_r11 * 4);
        uVar15 = FUN_000eb984(uVar11,*(undefined4 *)(&DAT_0006a528 + uVar12 * 4));
        uVar5 = (uint)uVar15;
        *(undefined4 *)((ulonglong)uVar15 >> 0x20) = uVar11;
      }
    }
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0xf) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0xf)) || uVar5 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  return;
}


```

## 0xc23c0

```c

void SelectA07ProfileAndMatrix(uint param_1)

{
  int iVar1;
  byte bVar2;
  byte bVar3;
  undefined4 *puVar4;
  int unaff_gp;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  undefined4 *puVar8;
  undefined1 *puVar9;
  undefined4 *puVar10;
  undefined4 uVar11;
  uint unaff_r20;
  uint uVar12;
  uint uStack00000000;

  uVar12 = param_1 & 0xff;
  uStack00000000 = param_1;
  thunk_FUN_000ea47e(2,3,*(undefined1 *)(unaff_gp + -0x4592),*(undefined1 *)(unaff_gp + -0x456a));
  if ((*(char *)(unaff_gp + -0x4579) == -0x5b) && (uVar12 == 8)) {
    thunk_FUN_000dd374(1,0x18);
  }
  else {
    if (uVar12 == 1) {
      *(byte *)(unaff_gp + -0x4571) = *(byte *)(unaff_gp + -0x4571) | 2;
    }
    if (uVar12 == 8) {
      *(byte *)(unaff_gp + -0x4571) = *(byte *)(unaff_gp + -0x4571) | 8;
    }
    if ((uVar12 == 1) || (uVar12 == 8)) {
      if (*(char *)(unaff_gp + -0x1250c) == -0x5b) {
        if ((&DAT_00100008)
            [(uint)*(byte *)(unaff_gp + -0x456a) + (uint)*(byte *)(unaff_gp + -0x4592) * 0x15] == -1
           ) {
          puVar10 = (undefined4 *)(unaff_gp + -0x13900);
          puVar8 = &DAT_0008df48;
          do {
            uVar11 = puVar8[1];
            *puVar10 = *puVar8;
            puVar10[1] = uVar11;
            puVar4 = puVar8 + 2;
            uVar11 = puVar8[3];
            puVar8 = puVar8 + 4;
            puVar10[2] = *puVar4;
            puVar10[3] = uVar11;
            puVar10 = puVar10 + 4;
          } while (puVar8 != &DAT_0008e2b8);
          puVar9 = &DAT_0008f33c;
        }
        else {
          bVar2 = *(byte *)(unaff_gp + -0x4592);
          bVar3 = *(byte *)(unaff_gp + -0x456a);
          iVar1 = (char)(&DAT_00100008)[(uint)bVar3 + (uint)bVar2 * 0x15] * 0x370;
          puVar8 = (undefined4 *)(&DAT_00100034 + iVar1);
          puVar10 = (undefined4 *)(unaff_gp + -0x13900);
          do {
            uVar11 = puVar8[1];
            *puVar10 = *puVar8;
            puVar10[1] = uVar11;
            puVar4 = puVar8 + 2;
            uVar11 = puVar8[3];
            puVar8 = puVar8 + 4;
            puVar10[2] = *puVar4;
            puVar10[3] = uVar11;
            puVar10 = puVar10 + 4;
          } while ((undefined4 *)(iVar1 + 0x1003a4) != puVar8);
          puVar9 = &DAT_00101bb4 + (char)(&DAT_00100008)[(uint)bVar3 + (uint)bVar2 * 0x15] * 0xc0;
        }
      }
      else {
        puVar10 = (undefined4 *)(unaff_gp + -0x13900);
        puVar8 = &DAT_0008df48;
        do {
          uVar11 = puVar8[1];
          *puVar10 = *puVar8;
          puVar10[1] = uVar11;
          puVar4 = puVar8 + 2;
          uVar11 = puVar8[3];
          puVar8 = puVar8 + 4;
          puVar10[2] = *puVar4;
          puVar10[3] = uVar11;
          puVar10 = puVar10 + 4;
        } while (puVar8 != &DAT_0008e2b8);
        puVar9 = &DAT_0008f33c;
      }
      uVar7 = (uint)(byte)(&DAT_00095314)[(uint)*(byte *)(unaff_gp + -0x3d7c) * 4];
      if (((uVar7 == 2) && (*(char *)(unaff_gp + -0x3e21) == -0x40)) || (-1 < (int)(uVar7 - 3))) {
        uVar7 = 0;
      }
      if ((uVar7 == 2) && ((*(byte *)(unaff_gp + -0x3e21) & 3) == 0)) {
        uVar7 = 1;
      }
      uVar6 = 0;
      do {
        uVar5 = 0;
        do {
          *(undefined1 *)(unaff_gp + -0x3e20 + uVar6 * 8 + uVar5) =
               puVar9[uVar5 + uVar6 * 8 + uVar7 * 0x40];
          uVar5 = uVar5 + 1 & unaff_r20;
        } while ((int)(uVar5 - 8) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 8)));
        uVar6 = uVar6 + 1 & unaff_r20;
      } while ((int)(uVar6 - 8) < 0 != ((int)uVar6 < 0 && -1 < (int)(uVar6 - 8)));
    }
    if (uVar12 == 1) {
      if (*(char *)(unaff_gp + -0x1250c) == -0x5b) {
        puVar8 = (undefined4 *)(unaff_gp + -0x13590);
        puVar10 = &DAT_001021b4;
        do {
          uVar11 = puVar10[1];
          *puVar8 = *puVar10;
          puVar8[1] = uVar11;
          puVar4 = puVar10 + 2;
          uVar11 = puVar10[3];
          puVar10 = puVar10 + 4;
          puVar8[2] = *puVar4;
          puVar8[3] = uVar11;
          puVar8 = puVar8 + 4;
        } while (puVar10 != (undefined4 *)0x102434);
        *puVar8 = uRam00102434;
      }
      else {
        puVar8 = (undefined4 *)(unaff_gp + -0x13590);
        puVar10 = &DAT_0008e2b8;
        do {
          uVar11 = puVar10[1];
          *puVar8 = *puVar10;
          puVar8[1] = uVar11;
          puVar4 = puVar10 + 2;
          uVar11 = puVar10[3];
          puVar10 = puVar10 + 4;
          puVar8[2] = *puVar4;
          puVar8[3] = uVar11;
          puVar8 = puVar8 + 4;
        } while (puVar10 != (undefined4 *)0x8e538);
        *puVar8 = uRam0008e538;
      }
    }
    if (uVar12 == 1) {
      thunk_FUN_000dd2cc(9,2);
      thunk_FUN_000c2964(1);
      thunk_FUN_000dd374(5,4);
      thunk_FUN_000dcfb2(2,2000);
      thunk_FUN_000dd374(3,6);
      *(byte *)(unaff_gp + -0x4571) = *(byte *)(unaff_gp + -0x4571) & 0xfd;
      *(byte *)(unaff_gp + -0x4571) = *(byte *)(unaff_gp + -0x4571) & 0xf7;
    }
    if (uVar12 == 8) {
      thunk_FUN_000dcddc(300);
      thunk_FUN_000dd374(3,9);
      thunk_FUN_000dd2cc(9,2);
      thunk_FUN_000c39b6(0xd);
      thunk_FUN_000c39b6(0xc);
      thunk_FUN_000c39b6(0x13);
      thunk_FUN_000dd374(5,4);
      thunk_FUN_000dcfb2(2,2000);
      thunk_FUN_000dd2cc(9,2);
      thunk_FUN_000c39b6(0x16);
      thunk_FUN_000c39b6(0xf);
      thunk_FUN_000c39b6(0x18);
      thunk_FUN_000c39b6(0xe);
      thunk_FUN_000c39b6(0x22);
      thunk_FUN_000c39b6(0x10);
      thunk_FUN_000c39b6(6);
      thunk_FUN_000c39b6(7);
      thunk_FUN_000dd374(5,4);
      thunk_FUN_000dcfb2(2,2000);
      thunk_FUN_000dd2cc(9,2);
      thunk_FUN_000c39b6(0x2e);
      thunk_FUN_000c39b6(0x30);
      thunk_FUN_000c39b6(0x2f);
      thunk_FUN_000dd374(5,4);
      thunk_FUN_000dcfb2(2,2000);
      thunk_FUN_000dd374(3,6);
      *(byte *)(unaff_gp + -0x4571) = *(byte *)(unaff_gp + -0x4571) & 0xfd;
      *(byte *)(unaff_gp + -0x4571) = *(byte *)(unaff_gp + -0x4571) & 0xf7;
    }
    thunk_FUN_000dd374(1,0x18);
  }
  return;
}


```

## 0xa96c8

```c

void FUN_000a96c8(undefined2 *param_1)

{
  uint uVar1;
  undefined4 uVar2;
  uint uVar3;
  undefined4 in_stack_00000000;
  undefined2 *puStack00000004;

  puStack00000004 = param_1;
  thunk_FUN_000ab844(&stack0x00000000,*param_1);
  uVar2 = *(undefined4 *)(&DAT_00069399 + ~((int)(short)param_1[1] << 2));
  if (*(char *)(param_1 + 2) == '\0') {
    uVar1 = 0x41200000;
  }
  else if ((*(char *)((int)param_1 + 5) == '\x03') || (*(char *)((int)param_1 + 5) == '\x0f')) {
    uVar3 = (uint)*(byte *)(param_1 + 2);
    FUN_000ebca4(uVar3);
    uVar1 = 0x41200000;
    FUN_000ebbcc(0x41200000,uVar3);
  }
  else {
    uVar1 = (uint)*(byte *)(param_1 + 2);
    FUN_000ebca4(uVar1);
    FUN_000ebbcc(uVar1,0x41200000);
  }
  switch(*(undefined1 *)((int)param_1 + 5)) {
  case 0:
    thunk_FUN_000abf70();
    break;
  case 1:
    thunk_FUN_000abc46(in_stack_00000000,uVar1,uVar2,(int)(short)param_1[1]);
    break;
  case 2:
  case 4:
  case 6:
    thunk_FUN_000abca0(in_stack_00000000);
    break;
  case 3:
    thunk_FUN_000abdb8(in_stack_00000000,uVar1);
    break;
  case 5:
  case 7:
  case 8:
  case 9:
  case 10:
  case 0xb:
  case 0xc:
  case 0xd:
    thunk_FUN_000abdb8(in_stack_00000000,
                       *(undefined4 *)(&DAT_0006ea30 + (uint)*(byte *)((int)param_1 + 5) * 4));
    break;
  case 0xe:
  case 0x10:
  case 0x12:
    thunk_FUN_000abd04(in_stack_00000000);
    break;
  case 0xf:
    thunk_FUN_000abe84(in_stack_00000000,uVar1);
    break;
  case 0x11:
  case 0x13:
  case 0x14:
  case 0x15:
  case 0x16:
  case 0x17:
  case 0x18:
  case 0x19:
    thunk_FUN_000abe84(in_stack_00000000,
                       *(undefined4 *)(&DAT_0006ea00 + (uint)*(byte *)((int)param_1 + 5) * 4));
    break;
  case 0x1a:
    thunk_FUN_000ab936(in_stack_00000000,uVar2);
    break;
  case 0x1b:
    thunk_FUN_000aba3e(in_stack_00000000,uVar2);
    break;
  case 0x1c:
    thunk_FUN_000abf46(in_stack_00000000,uVar1);
    break;
  default:
    thunk_FUN_000abf70();
  }
  return;
}


```

## 0xabf7c

```c

uint FUN_000abf7c(uint param_1)

{
  int unaff_gp;
  int extraout_r11;
  int extraout_r11_00;
  int extraout_r11_01;
  int extraout_r11_02;
  uint uVar1;
  undefined4 *puVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  uint unaff_r20;
  uint uStack00000000;

  uVar1 = param_1 & 0xff;
  puVar2 = (undefined4 *)(uVar1 * 4 + unaff_gp + -0x4340);
  uVar3 = *(undefined4 *)(unaff_gp + -0x4348);
  uStack00000000 = param_1;
  FUN_000ebb70(uVar3,0);
  uVar4 = *(undefined4 *)(unaff_gp + -0x434c);
  FUN_000eb984(uVar4,uVar3);
  *puVar2 = uVar4;
  uVar1 = uVar1 + 1 & unaff_r20;
  puVar2 = (undefined4 *)(uVar1 * 4 + extraout_r11);
  uVar3 = *(undefined4 *)(unaff_gp + -0x4344);
  FUN_000ebb70(uVar3,0);
  FUN_000eb984(uVar3,*(undefined4 *)(unaff_gp + -0x434c));
  *puVar2 = uVar3;
  uVar1 = uVar1 + 1 & unaff_r20;
  puVar2 = (undefined4 *)(uVar1 * 4 + extraout_r11_00);
  uVar3 = *(undefined4 *)(unaff_gp + -0x4358);
  FUN_000eb984(uVar3,*(undefined4 *)(unaff_gp + -0x434c));
  *puVar2 = uVar3;
  uVar1 = uVar1 + 1 & unaff_r20;
  puVar2 = (undefined4 *)(uVar1 * 4 + extraout_r11_01);
  uVar3 = *(undefined4 *)(unaff_gp + -0x4354);
  FUN_000eb984(uVar3,*(undefined4 *)(unaff_gp + -0x434c));
  *puVar2 = uVar3;
  uVar1 = uVar1 + 1 & unaff_r20;
  puVar2 = (undefined4 *)(uVar1 * 4 + extraout_r11_02);
  uVar3 = *(undefined4 *)(unaff_gp + -0x4350);
  FUN_000eb984(uVar3,*(undefined4 *)(unaff_gp + -0x434c));
  *puVar2 = uVar3;
  return uVar1 + 1 & unaff_r20;
}


```

## 0xb64e0

```c

void FUN_000b64e0(void)

{
  undefined **ppuVar1;
  undefined4 *puVar2;
  undefined1 uVar3;
  undefined2 uVar4;
  int unaff_gp;
  uint uVar5;
  int iVar6;
  uint uVar7;
  byte bVar8;
  undefined *puVar9;
  undefined2 *puVar10;
  undefined4 *puVar11;
  undefined **ppuVar12;
  undefined1 *puVar13;
  undefined4 *puVar14;
  undefined4 uVar15;
  uint unaff_r20;
  uint unaff_r21;

  bVar8 = 0;
  uVar5 = 0;
  do {
    bVar8 = bVar8 | *(byte *)(uVar5 + 0x100000) ^ (&DAT_00094c54)[uVar5] |
            s_AD301A07_0010fff0[uVar5] ^ (&DAT_00094c54)[uVar5];
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 8);
  if (bVar8 == 0) {
    puVar11 = (undefined4 *)(unaff_gp + -0x1330c);
    ppuVar12 = &PTR_DAT_00102438;
    do {
      puVar9 = ppuVar12[1];
      *puVar11 = *ppuVar12;
      puVar11[1] = puVar9;
      ppuVar1 = ppuVar12 + 2;
      puVar9 = ppuVar12[3];
      ppuVar12 = ppuVar12 + 4;
      puVar11[2] = *ppuVar1;
      puVar11[3] = puVar9;
      puVar11 = puVar11 + 4;
    } while (ppuVar12 != (undefined **)&DAT_00103238);
    *(undefined1 *)(unaff_gp + -0x1250c) = 0xa5;
  }
  else {
    puVar11 = (undefined4 *)(unaff_gp + -0x1330c);
    puVar14 = &DAT_0008e53c;
    do {
      uVar15 = puVar14[1];
      *puVar11 = *puVar14;
      puVar11[1] = uVar15;
      puVar2 = puVar14 + 2;
      uVar15 = puVar14[3];
      puVar14 = puVar14 + 4;
      puVar11[2] = *puVar2;
      puVar11[3] = uVar15;
      puVar11 = puVar11 + 4;
    } while (puVar14 != (undefined4 *)&DAT_0008f33c);
    *(undefined1 *)(unaff_gp + -0x1250c) = 0x5a;
  }
  bVar8 = 0;
  uVar5 = 0;
  do {
    bVar8 = bVar8 | (&DAT_00120000)[uVar5] ^ s_AD301B01_00094c5c[uVar5] |
            s_AD301B01_00127ff0[uVar5] ^ s_AD301B01_00094c5c[uVar5];
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 8);
  *(char *)(unaff_gp + -0x455c) = (bVar8 == 0) * 'K' + 'Z';
  uVar5 = 0;
  do {
    *(undefined2 *)(uVar5 * 2 + unaff_gp + -0x4184) = 0;
    uVar5 = uVar5 + 1 & unaff_r21;
  } while ((int)(uVar5 - 0x178) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x178)));
  *(undefined2 *)(unaff_gp + -0x4188) = 0;
  *(undefined2 *)(unaff_gp + -0x4186) = 0;
  uVar5 = 0;
  do {
    *(char *)(unaff_gp + -0x3d94 + uVar5) = (char)uVar5;
    *(undefined1 *)(unaff_gp + -0x3d66 + uVar5) = 0x9c;
    *(undefined1 *)(unaff_gp + -0x3a77 + uVar5) = 0x9c;
    *(undefined1 *)(unaff_gp + -0x5e2f + uVar5) = 0x9c;
    *(undefined1 *)(unaff_gp + -0x3e38 + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3e34 + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 4) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 4)));
  *(undefined1 *)(unaff_gp + -0x3c43) = 0x9c;
  *(undefined1 *)(unaff_gp + -0x3e2f) = 0;
  *(undefined1 *)(unaff_gp + -0x4571) = 2;
  *(undefined1 *)(unaff_gp + -0x5e3a) = 0;
  *(undefined1 *)(unaff_gp + -0x3a1b) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x3ab7) = 0x5a;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x3c69 + uVar5) = 0x5a;
    *(undefined2 *)(uVar5 * 2 + unaff_gp + -0x3ab6) = 0;
    *(undefined1 *)(unaff_gp + -0x3ad0 + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3d7b + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3a73 + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3d8e + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3a61 + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3c41 + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x12) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x12)));
  *(undefined1 *)(unaff_gp + -0x3c52) = 6;
  *(undefined2 *)(unaff_gp + -0x3c54) = 300;
  uVar5 = 1;
  do {
    puVar10 = (undefined2 *)(uVar5 * 4 + unaff_gp + -0x3c54);
    *(undefined1 *)(puVar10 + 1) = 2;
    *puVar10 = 0x32;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 4) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 4)));
  *(undefined1 *)(unaff_gp + -0x3d7c) = 0x11;
  *(undefined4 *)(unaff_gp + -0x3abc) = 300;
  *(undefined1 *)(unaff_gp + -0x4577) = 0;
  *(undefined1 *)(unaff_gp + -0x3e42) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x3c2f) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x3d69) = 0;
  *(undefined1 *)(unaff_gp + -0x3d68) = 0;
  *(undefined1 *)(unaff_gp + -0x3d67) = 0;
  *(undefined1 *)(unaff_gp + -0x3e28) = 0;
  *(undefined1 *)(unaff_gp + -0x3e27) = 0;
  *(undefined1 *)(unaff_gp + -0x3e26) = 0;
  *(undefined1 *)(unaff_gp + -0x3e25) = 0;
  *(undefined1 *)(unaff_gp + -0x3e24) = 0;
  *(undefined1 *)(unaff_gp + -0x3e23) = 0;
  *(undefined1 *)(unaff_gp + -0x3a4f) = 0;
  *(undefined1 *)(unaff_gp + -0x3a4d) = 0;
  *(undefined1 *)(unaff_gp + -0x3a4e) = 0;
  *(undefined1 *)(unaff_gp + -0x458e) = 0;
  *(undefined1 *)(unaff_gp + -0x3e2e) = 0;
  *(undefined1 *)(unaff_gp + -0x3e22) = 0;
  *(undefined2 *)(unaff_gp + -0x3a92) = 0;
  *(undefined2 *)(unaff_gp + -0x3a7a) = 0;
  *(undefined4 *)(unaff_gp + -0x5ecc) = 0;
  *(undefined4 *)(unaff_gp + -0x44dc) = 0;
  *(undefined4 *)(unaff_gp + -0x3a18) = 0;
  *(undefined1 *)(unaff_gp + -0x3a14) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x3a7e) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x5ddc) = 0;
  *(undefined1 *)(unaff_gp + -0x418e) = 0;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x418d + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 5);
  *(undefined1 *)(unaff_gp + -0x5dd8) = 0;
  *(undefined1 *)(unaff_gp + -0x5e08) = 0;
  *(undefined1 *)(unaff_gp + -0x5e07) = 0;
  *(undefined2 *)(unaff_gp + -0x459c) = 0;
  *(undefined4 *)(unaff_gp + -0x4598) = 0;
  *(undefined1 *)(unaff_gp + -0x4594) = 0;
  *(undefined1 *)(unaff_gp + -0x4593) = 0;
  *(undefined1 *)(unaff_gp + -0x4592) = 0;
  uVar4 = (undefined2)unaff_r21;
  *(undefined2 *)(unaff_gp + -0x45b8) = uVar4;
  *(undefined4 *)(unaff_gp + -0x45b4) = 0xffffffff;
  uVar3 = (undefined1)unaff_r20;
  *(undefined1 *)(unaff_gp + -0x45b0) = uVar3;
  *(undefined1 *)(unaff_gp + -0x45af) = uVar3;
  *(undefined1 *)(unaff_gp + -0x45ae) = uVar3;
  *(undefined1 *)(unaff_gp + -0x45ad) = uVar3;
  *(undefined1 *)(unaff_gp + -0x45ac) = uVar3;
  *(undefined1 *)(unaff_gp + -0x45ab) = uVar3;
  *(undefined1 *)(unaff_gp + -0x4586) = 0;
  *(undefined1 *)(unaff_gp + -0x4585) = 1;
  *(undefined1 *)(unaff_gp + -0x4584) = 0;
  *(undefined1 *)(unaff_gp + -0x4588) = 0;
  *(undefined1 *)(unaff_gp + -0x5ef5) = 0;
  *(undefined4 *)(unaff_gp + -0x5ed0) = 0;
  *(undefined4 *)(unaff_gp + -0x5ed4) = 0;
  *(undefined4 *)(unaff_gp + -0x5ec8) = 0;
  *(undefined4 *)(unaff_gp + -0x5ec4) = 0;
  *(undefined4 *)(unaff_gp + -0x5ec0) = 0;
  *(undefined4 *)(unaff_gp + -0x5ebc) = 0;
  *(undefined4 *)(unaff_gp + -0x5eb8) = 0;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x5eec + uVar5) = uVar3;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 0xf);
  *(undefined1 *)(unaff_gp + -0x5edd) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5edc) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5edb) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5eda) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ed9) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ed8) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ed7) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a78) = 0;
  *(undefined1 *)(unaff_gp + -0x5ddb) = 100;
  *(undefined1 *)(unaff_gp + -0x5dda) = 1;
  *(undefined2 *)(unaff_gp + -0x45a0) = 0;
  *(undefined2 *)(unaff_gp + -0x4590) = 0;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x3a90 + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x12) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x12)));
  uVar5 = 0;
  do {
    iVar6 = uVar5 * 2;
    *(undefined2 *)(unaff_gp + -0x3e7c + iVar6) = 0;
    *(undefined2 *)(unaff_gp + -0x3e70 + iVar6) = 0;
    *(undefined2 *)(unaff_gp + -0x3e88 + iVar6) = 0;
    *(undefined2 *)(unaff_gp + -0x3e94 + iVar6) = 0xfc18;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 4) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 4)));
  *(undefined1 *)(unaff_gp + -0x3e64) = 0;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x3e53 + uVar5) = 0;
    *(undefined1 *)(unaff_gp + -0x3e63 + uVar5) = 0x5a;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0xf) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0xf)) || uVar5 == 0xf);
  *(undefined1 *)(unaff_gp + -0x3e43) = 0;
  *(undefined1 *)(unaff_gp + -0x3c2e) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x3e2a) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x5e30) = 0xaa;
  *(undefined1 *)(unaff_gp + -0x452d) = 0;
  *(undefined1 *)(unaff_gp + -0x3a48) = 0;
  *(undefined1 *)(unaff_gp + -0x3a47) = 0;
  *(undefined1 *)(unaff_gp + -0x3a46) = 0;
  *(undefined1 *)(unaff_gp + -0x3a45) = 0;
  *(undefined1 *)(unaff_gp + -0x3a44) = 0;
  *(undefined1 *)(unaff_gp + -0x457b) = 0x5a;
  *(undefined4 *)(unaff_gp + -0x45a8) = 0;
  *(undefined1 *)(unaff_gp + -0x45a4) = 0;
  *(undefined1 *)(unaff_gp + -0x45a3) = 0;
  *(undefined1 *)(unaff_gp + -0x45a2) = 0;
  *(undefined1 *)(unaff_gp + -0x45a1) = 0;
  *(undefined1 *)(unaff_gp + -0x5ef4) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ef3) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ef2) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ef1) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5ef0) = 0;
  *(undefined1 *)(unaff_gp + -0x5eef) = 0xe;
  *(undefined1 *)(unaff_gp + -0x5eee) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a4c) = 0;
  *(undefined1 *)(unaff_gp + -0x3c44) = 3;
  *(undefined1 *)(unaff_gp + -0x5e34) = 0;
  *(undefined2 *)(unaff_gp + -0x5e38) = 0;
  *(undefined1 *)(unaff_gp + -0x5e33) = 0;
  *(undefined2 *)(unaff_gp + -0x5e36) = 0;
  *(undefined1 *)(unaff_gp + -0x3d5e) = 3;
  uVar5 = 0;
  do {
    puVar13 = (undefined1 *)(uVar5 * 4 + unaff_gp + -0x3ddc);
    *puVar13 = 0;
    *(undefined2 *)(puVar13 + 2) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x12) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x12)));
  *(undefined1 *)(unaff_gp + -0x3d62) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x45c1) = 0;
  *(undefined1 *)(unaff_gp + -0x3a7c) = 0;
  *(undefined1 *)(unaff_gp + -0x45cb) = 0;
  *(undefined1 *)(unaff_gp + -0x4575) = uVar3;
  *(undefined1 *)(unaff_gp + -0x4574) = uVar3;
  *(undefined1 *)(unaff_gp + -0x4573) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x4572) = 0xe2;
  *(undefined1 *)(unaff_gp + -0x4567) = 0;
  *(undefined1 *)(unaff_gp + -0x456d) = 0;
  *(undefined1 *)(unaff_gp + -0x4570) = 1;
  *(undefined1 *)(unaff_gp + -0x456f) = 1;
  *(undefined1 *)(unaff_gp + -0x456e) = 1;
  *(undefined1 *)(unaff_gp + -0x456c) = 0x5a;
  *(undefined2 *)(unaff_gp + -0x5e1c) = 0;
  *(undefined1 *)(unaff_gp + -0x5e1a) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x5e01) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x5e02) = 0xd;
  *(undefined1 *)(unaff_gp + -0x5dea) = 0;
  *(undefined1 *)(unaff_gp + -0x5de9) = 0;
  *(undefined1 *)(unaff_gp + -0x5dec) = 1;
  *(undefined1 *)(unaff_gp + -0x5deb) = 1;
  *(undefined1 *)(unaff_gp + -0x5de8) = 1;
  *(undefined4 *)(unaff_gp + -0x5df8) = 0;
  *(undefined4 *)(unaff_gp + -0x5df0) = 0;
  *(undefined4 *)(unaff_gp + -0x5dfc) = 0;
  *(undefined4 *)(unaff_gp + -0x5df4) = 0;
  *(undefined4 *)(unaff_gp + -0x5de4) = 0;
  *(undefined4 *)(unaff_gp + -0x5e00) = 0;
  *(undefined1 *)(unaff_gp + -0x5dd9) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x3d61) = 0;
  *(undefined1 *)(unaff_gp + -0x3e2c) = 0;
  *(undefined1 *)(unaff_gp + -0x3e2d) = 0;
  *(undefined4 *)(unaff_gp + -0x452c) = 0;
  *(undefined1 *)(unaff_gp + -0x4528) = 0;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x4527 + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r21;
  } while (uVar5 < 0x40);
  *(undefined1 *)(unaff_gp + -0x3e29) = 0;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x3af9 + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 0xb);
  *(undefined1 *)(unaff_gp + -0x3afa) = 0;
  *(undefined1 *)(unaff_gp + -0x5e04) = 0;
  *(undefined1 *)(unaff_gp + -0x5dde) = 0;
  *(undefined1 *)(unaff_gp + -0x5e03) = 0;
  *(undefined1 *)(unaff_gp + -0x5ddd) = 0;
  *(undefined1 *)(unaff_gp + -0x457c) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x458d) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x458c) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x457a) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x5e06) = 0x5a;
  uVar5 = 0;
  do {
    puVar13 = (undefined1 *)(uVar5 * 0x14 + unaff_gp + -0x5eb4);
    *puVar13 = uVar3;
    *(undefined2 *)(puVar13 + 2) = 0;
    *(undefined2 *)(puVar13 + 4) = 0;
    *(undefined2 *)(puVar13 + 6) = 0;
    *(undefined2 *)(puVar13 + 8) = 0;
    puVar13[10] = 0;
    puVar13[0xb] = uVar3;
    *(undefined4 *)(puVar13 + 0xc) = 0;
    *(undefined4 *)(puVar13 + 0x10) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 6) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 6)));
  *(undefined1 *)(unaff_gp + -0x5e3c) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x3a1c) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x4576) = 0;
  *(undefined1 *)(unaff_gp + -0x5de0) = 0;
  *(undefined1 *)(unaff_gp + -0x5ddf) = 0;
  *(undefined4 *)(unaff_gp + -0x4580) = 0;
  *(undefined1 *)(unaff_gp + -0x5e05) = 0;
  uVar5 = 0;
  do {
    uVar7 = 0;
    do {
      *(undefined1 *)(unaff_gp + -0x3e20 + uVar5 * 8 + uVar7) = (&DAT_0008f33c)[uVar7 + uVar5 * 8];
      uVar7 = uVar7 + 1 & unaff_r20;
    } while ((int)(uVar7 - 8) < 0 != ((int)uVar7 < 0 && -1 < (int)(uVar7 - 8)));
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 8) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 8)));
  *(undefined1 *)(unaff_gp + -0x456b) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x4568) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x3e21) = 0xc0;
  *(undefined1 *)(unaff_gp + -0x3d90) = 0;
  *(undefined1 *)(unaff_gp + -0x456a) = 1;
  *(undefined1 *)(unaff_gp + -0x4569) = 1;
  *(undefined1 *)(unaff_gp + -0x3a1a) = 1;
  *(undefined1 *)(unaff_gp + -0x3a19) = 0;
  *(undefined1 *)(unaff_gp + -0x418f) = 0;
  *(undefined1 *)(unaff_gp + -0x3d8f) = 0;
  *(undefined1 *)(unaff_gp + -0x3ddf) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x45cc) = 1;
  *(undefined1 *)(unaff_gp + -0x45c9) = 0;
  *(undefined1 *)(unaff_gp + -0x4579) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x4578) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x4587) = 2;
  *(undefined1 *)(unaff_gp + -0x44e0) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x3d60) = 0xe2;
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x45c8 + uVar5) = (&DAT_00094b17)[uVar5];
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 7);
  *(undefined1 *)(unaff_gp + -0x5e2b) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x3a0c) = 0;
  *(undefined1 *)(unaff_gp + -0x3a0b) = 0x5a;
  uVar5 = 0;
  do {
    uVar7 = 0;
    do {
      *(undefined1 *)(uVar5 * 0x1e + unaff_gp + -0x3a0a + uVar7) = 0;
      uVar7 = uVar7 + 1 & unaff_r20;
    } while ((int)(uVar7 - 0x1e) < 0 != ((int)uVar7 < 0 && -1 < (int)(uVar7 - 0x1e)));
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x20) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x20)));
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x5eff + uVar5) = 0x5a;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 10) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 10)));
  *(undefined4 *)(unaff_gp + -0x5e28) = 0;
  *(undefined4 *)(unaff_gp + -0x3bec) = 0;
  *(undefined1 *)(unaff_gp + -0x3d5f) = 2;
  *(undefined2 *)(unaff_gp + -0x3a40) = uVar4;
  *(undefined2 *)(unaff_gp + -0x3a3e) = uVar4;
  *(undefined2 *)(unaff_gp + -0x3a3c) = uVar4;
  *(undefined2 *)(unaff_gp + -0x3a3a) = uVar4;
  *(undefined1 *)(unaff_gp + -0x3a38) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a37) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a36) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a35) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a34) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a33) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a32) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a31) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a30) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a2f) = uVar3;
  *(undefined4 *)(unaff_gp + -0x3a2c) = 0xffffffff;
  *(undefined4 *)(unaff_gp + -0x3a28) = 0xffffffff;
  *(undefined1 *)(unaff_gp + -0x3a24) = uVar3;
  *(undefined1 *)(unaff_gp + -0x3a23) = uVar3;
  *(undefined1 *)(unaff_gp + -0x5e1e) = 0xa5;
  *(undefined1 *)(unaff_gp + -0x5e20) = 1;
  *(undefined1 *)(unaff_gp + -0x5e2a) = 1;
  *(undefined1 *)(unaff_gp + -0x5e1f) = 0;
  *(undefined4 *)(unaff_gp + -0x5e24) = 0;
  *(undefined1 *)(unaff_gp + -0x364a) = 0xa5;
  *(undefined4 *)(unaff_gp + -0x3648) = 0;
  *(undefined1 *)(unaff_gp + -0x3e2b) = 0;
  *(undefined1 *)(unaff_gp + -0x4190) = 0x5a;
  *(undefined1 *)(unaff_gp + -0x3bef) = 0xa5;
  uVar5 = 0;
  do {
    *(undefined4 *)(uVar5 * 4 + unaff_gp + -0x3e40) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 2) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 2)));
  uVar5 = 0;
  do {
    *(undefined4 *)(uVar5 * 4 + unaff_gp + -0x3d5c) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while ((int)(uVar5 - 0x3c) < 0 != ((int)uVar5 < 0 && -1 < (int)(uVar5 - 0x3c)));
  uVar5 = 0;
  do {
    *(undefined1 *)(unaff_gp + -0x45c0 + uVar5) = 0;
    uVar5 = uVar5 + 1 & unaff_r20;
  } while (uVar5 < 8);
  *(undefined1 *)(unaff_gp + -0x5e1d) = uVar3;
  *(undefined2 *)(unaff_gp + -0x458a) = 0;
  *(undefined1 *)(unaff_gp + -0x3643) = 0;
  *(undefined1 *)(unaff_gp + -0x5e3b) = 0;
  return;
}


```

## 0xab844

```c

void FUN_000ab844(void)

{
  FUN_000ab7f6();
  return;
}


```

## 0xa9d80

```c

void FUN_000a9d80(void)

{
  int unaff_gp;
  uint uVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  uint unaff_r20;
  uint unaff_r21;
  uint uVar5;

  if (*(char *)((uint)*(byte *)(unaff_gp + -0x3d94) * 4 + unaff_gp + -0x3ddc) == '\0') {
    uVar5 = 0;
  }
  else {
    uVar5 = (uint)*(ushort *)((uint)*(byte *)(unaff_gp + -0x3d94) * 4 + unaff_gp + -0x3ddc + 2) *
            0x30;
  }
  uVar1 = 0xb40;
  uVar2 = 0;
  do {
    if ((*(byte *)(unaff_gp + -0x12977 + uVar2) < 0x10) &&
       (iVar3 = (uint)*(byte *)(unaff_gp + -0x12997 + (uint)*(byte *)(unaff_gp + -0x12977 + uVar2))
                * 2,
       uVar4 = (int)*(short *)(unaff_gp + -0x12e7c + uVar2 * 2) +
               (int)*(short *)(unaff_gp + -0x13628 + iVar3) +
               (int)*(short *)(unaff_gp + -0x13638 + iVar3) & unaff_r21, uVar4 < uVar1)) {
      uVar1 = uVar4;
    }
    uVar2 = uVar2 + 1 & unaff_r20;
  } while ((int)(uVar2 - 0xf) < 0 != ((int)uVar2 < 0 && -1 < (int)(uVar2 - 0xf)) || uVar2 == 0xf);
  uVar1 = uVar1 + 0x4e0 & unaff_r21;
  if (uVar1 < uVar5) {
    uVar5 = uVar5 - uVar1;
  }
  else {
    uVar5 = 0;
  }
  if (0x2580 < uVar5) {
    uVar5 = 0x2580;
  }
  thunk_FUN_000a8c14(8,5,1);
  uVar2 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 8;
  *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0xe000c;
  uVar1 = 0;
  do {
    uVar2 = uVar2 + 1 & unaff_r20;
    *(uint *)(uVar2 * 4 + unaff_gp + -0x4340) = uVar5;
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 8) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 8)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x38);
  return;
}


```

## 0xa9328

```c

void FUN_000a9328(void)

{
  byte bVar1;
  int unaff_gp;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint unaff_r20;
  byte bStack00000002;

  thunk_FUN_000a8c14(0x10,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined **)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = &DAT_000c2114;
  bVar1 = (byte)unaff_r20;
  bStack00000002 = (char)(unaff_r20 & 4) + 1U & bVar1;
  uVar2 = 0;
  do {
    uVar3 = (uint)(byte)(&DAT_00068b30)[uVar2];
    uVar4 = (uint)*(byte *)(unaff_gp + -0x3e53 + uVar3);
    if (uVar4 == 0) {
LAB_000a93f6:
      if (*(byte *)(unaff_gp + -0x12977 + uVar3) == unaff_r20) {
        *(undefined4 *)(unaff_gp + -0x4340 + (uint)bStack00000002 * 4) = 0xffffffff;
      }
      else {
        *(uint *)(unaff_gp + -0x4340 + (uint)bStack00000002 * 4) =
             (uint)*(byte *)(unaff_gp + -0x12977 + uVar3);
      }
      bStack00000002 = bStack00000002 + 1 & bVar1;
    }
    else if (uVar4 == 1) {
      *(undefined4 *)(unaff_gp + -0x4340 + (uint)bStack00000002 * 4) = 0x10;
      bStack00000002 = bStack00000002 + 1 & bVar1;
    }
    else if (uVar4 == 5) {
      *(undefined4 *)(unaff_gp + -0x4340 + (uint)bStack00000002 * 4) = 0x11;
      bStack00000002 = bStack00000002 + 1 & bVar1;
    }
    else {
      if (uVar4 != unaff_r20) goto LAB_000a93f6;
      *(undefined4 *)(unaff_gp + -0x4340 + (uint)bStack00000002 * 4) = 0xffffffff;
      bStack00000002 = bStack00000002 + 1 & bVar1;
    }
    uVar2 = uVar2 + 1 & unaff_r20;
    if ((int)(uVar2 - 0xf) < 0 == ((int)uVar2 < 0 && -1 < (int)(uVar2 - 0xf)) && uVar2 != 0xf) {
      thunk_FUN_000a8d54();
      thunk_FUN_000a8cfc();
      thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
      return;
    }
  } while( true );
}


```

## 0xa949a

```c

void FUN_000a949a(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;

  thunk_FUN_000a8c14(8,5,1);
  uVar2 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 8;
  *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0xe0254;
  uVar1 = 0;
  do {
    uVar2 = uVar2 + 1 & unaff_r20;
    *(uint *)(uVar2 * 4 + unaff_gp + -0x4340) = (uint)*(byte *)(unaff_gp + -0x13900 + uVar1);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 8) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 8)));
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x38);
  return;
}


```

## 0xa95c8

```c

void FUN_000a95c8(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  int iVar2;
  uint uVar3;

  thunk_FUN_000a8c14(0x10,5,1);
  iVar2 = unaff_gp + -0x4340;
  uVar3 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)(uVar3 * 4 + iVar2) = 0xc2124;
  uVar1 = 0;
  do {
    uVar3 = uVar3 + 1 & unaff_r20;
    *(uint *)(uVar3 * 4 + iVar2) = (uint)*(byte *)(unaff_gp + -0x1330c + uVar1);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  thunk_FUN_000a8c14(1,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 1;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar2) = 0xc4e7d;
  *(undefined4 *)(((unaff_r20 & 4) + 1 & unaff_r20) * 4 + iVar2) = 1;
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x1c);
  return;
}


```

## 0xa8f36

```c

void FUN_000a8f36(uint param_1)

{
  int unaff_gp;
  undefined4 *puVar1;
  uint uVar2;
  undefined2 uStack00000000;
  byte bStack00000004;
  undefined2 uStack00000006;
  uint uStack00000008;

  uVar2 = param_1 & 0xffff;
  uStack00000008 = param_1;
  thunk_FUN_000a8c14(5,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 5;
  *(uint *)(unaff_gp + -0x4330) = uVar2 * 5 + 0xe0068;
  if ((int)(uVar2 - 2) < 0 || uVar2 == 2) {
    puVar1 = (undefined4 *)(unaff_gp + -0x129b0 + uVar2 * 8);
    _bStack00000004 = puVar1[1];
    uStack00000000 = (undefined2)*puVar1;
    _uStack00000000 =
         CONCAT22(*(short *)(unaff_gp + uVar2 * 0x26 + *(char *)(unaff_gp + -0x3e28 + uVar2) * 2 +
                            -0x12b52) + (short)*(char *)(unaff_gp + -0x3e25 + uVar2),uStack00000000)
    ;
  }
  else if (uVar2 == 3) {
    puVar1 = (undefined4 *)(unaff_gp + -0x12a34 + (uint)*(byte *)(unaff_gp + -0x4592) * 8);
    _bStack00000004 = puVar1[1];
    uStack00000000 = (undefined2)*puVar1;
    _uStack00000000 = CONCAT22((short)*(char *)(unaff_gp + -0x3e22),uStack00000000);
  }
  else {
    _bStack00000004 = (ushort)bStack00000004;
  }
  thunk_FUN_000a984e(&stack0x00000000);
  thunk_FUN_000ac05a(5);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x2c);
  return;
}


```

## 0xa9bde

```c

void FUN_000a9bde(uint param_1)

{
  int unaff_gp;
  uint *puVar1;
  uint unaff_r20;
  uint in_stack_00000000;
  uint in_stack_00000004;
  uint uStack00000008;

  uStack00000008 = param_1;
  thunk_FUN_000a8c14(5,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 5;
  *(uint *)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = (param_1 & 0xffff) * 5 + 0xe0068;
  puVar1 = (uint *)(unaff_gp + -0x12944 + (param_1 & 0xffff) * 8);
  in_stack_00000000 = *puVar1;
  in_stack_00000004 = puVar1[1];
  if (*(char *)(unaff_gp + -0x3ddf) == 'Z') {
    in_stack_00000000 = in_stack_00000000 & 0xffff;
  }
  thunk_FUN_000a984e(&stack0x00000000);
  thunk_FUN_000ac05a((unaff_r20 & 4) + 1 & unaff_r20);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x2c);
  return;
}


```

## 0xa91c6

```c

void FUN_000a91c6(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;

  if (((*(char *)(unaff_gp + -0x456b) == -0x5b) && (*(char *)(unaff_gp + -0x418f) != '\0')) &&
     (*(char *)(unaff_gp + -0x455c) == -0x5b)) {
    *(undefined1 *)(unaff_gp + -0x41a7) = 1;
  }
  else {
    *(undefined1 *)(unaff_gp + -0x41a7) = 0;
  }
  thunk_FUN_000a8c14(0x10,5,1);
  uVar2 = unaff_r20 & 4;
  *(undefined4 *)(unaff_gp + -0x4334) = 0x10;
  *(undefined4 *)(uVar2 * 4 + unaff_gp + -0x4340) = 0xc20d0;
  uVar1 = 0;
  do {
    uVar2 = uVar2 + 1 & unaff_r20;
    *(uint *)(uVar2 * 4 + unaff_gp + -0x4340) =
         (uint)*(byte *)((uint)*(byte *)(unaff_gp + -0x41a7) * 0x10 + unaff_gp + -0x12997 + uVar1);
    uVar1 = uVar1 + 1 & unaff_r20;
  } while ((int)(uVar1 - 0xf) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 0xf)) || uVar1 == 0xf);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x58);
  return;
}


```

## 0xa928a

```c

void FUN_000a928a(void)

{
  int unaff_gp;
  uint uVar1;
  uint unaff_r20;
  uint uVar2;
  uint uVar3;

  uVar2 = 0;
  do {
    thunk_FUN_000a8c14(3,5,1);
    uVar3 = unaff_r20 & 4;
    *(undefined4 *)(unaff_gp + -0x4334) = 3;
    *(uint *)(uVar3 * 4 + unaff_gp + -0x4340) = uVar2 * 4 + 0xc0840;
    uVar1 = 0;
    do {
      uVar3 = uVar3 + 1 & unaff_r20;
      *(undefined4 *)(uVar3 * 4 + unaff_gp + -0x4340) = 0x3f800000;
      uVar1 = uVar1 + 1 & unaff_r20;
    } while ((int)(uVar1 - 3) < 0 != ((int)uVar1 < 0 && -1 < (int)(uVar1 - 3)));
    thunk_FUN_000a8d54();
    thunk_FUN_000a8cfc();
    thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x24);
    uVar2 = uVar2 + 1 & unaff_r20;
  } while ((int)(uVar2 - 0xf) < 0 != ((int)uVar2 < 0 && -1 < (int)(uVar2 - 0xf)) || uVar2 == 0xf);
  return;
}


```

## 0xabdb8

```c

void FUN_000abdb8(void)

{
  FUN_000abd0e();
  return;
}


```

## 0xabe84

```c

void FUN_000abe84(void)

{
  FUN_000abdc4();
  return;
}


```

## 0xabca0

```c

void FUN_000abca0(void)

{
  FUN_000abc52();
  return;
}


```

## 0xabd04

```c

void FUN_000abd04(void)

{
  FUN_000abcaa();
  return;
}


```

## 0xabc46

```c

void FUN_000abc46(void)

{
  FUN_000aba4c();
  return;
}


```

## 0xabd0e

```c

void FUN_000abd0e(undefined4 param_1,undefined4 param_2)

{
  int unaff_gp;
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 extraout_r11;
  undefined4 extraout_r11_00;
  undefined4 uStack00000000;
  undefined4 uStack00000004;

  *(undefined4 *)(unaff_gp + -0x4358) = 0x3f800000;
  *(undefined4 *)(unaff_gp + -0x4354) = 0x40000000;
  *(undefined4 *)(unaff_gp + -0x4350) = 0x3f800000;
  uStack00000000 = param_1;
  uStack00000004 = param_2;
  FUN_000eb984(param_1,param_1);
  uVar4 = extraout_r11;
  FUN_000eb984(extraout_r11,0x40000000);
  uVar3 = uStack00000004;
  FUN_000eb984(uStack00000004,uVar4);
  uVar1 = uVar3;
  uVar2 = FUN_000eba38(uVar3,param_1);
  uVar3 = FUN_000eba38(uVar2,uVar3);
  FUN_000ebbcc(uVar2,uVar3);
  *(undefined4 *)(unaff_gp + -0x434c) = uVar2;
  FUN_000eb984(uVar4,extraout_r11_00);
  FUN_000ebb70(uVar4,0x40000000);
  *(undefined4 *)(unaff_gp + -0x4348) = uVar4;
  uVar4 = FUN_000ebb70(uVar1,param_1);
  FUN_000eba38(uVar4,uVar1);
  *(undefined4 *)(unaff_gp + -0x4344) = uVar4;
  return;
}


```

## 0xabdc4

```c

void FUN_000abdc4(undefined4 param_1,undefined4 param_2)

{
  int unaff_gp;
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 extraout_r11;
  undefined4 uVar3;
  undefined4 uVar4;
  undefined4 uStack00000000;
  undefined4 uStack00000004;

  uVar3 = 0x3f800000;
  uStack00000000 = param_1;
  uStack00000004 = param_2;
  uVar2 = FUN_000eb984(param_1,param_1);
  *(undefined4 *)(unaff_gp + -0x4358) = param_1;
  uVar4 = 0x40000000;
  uVar2 = FUN_000eb984(0x40000000,uVar2);
  FUN_000eb984(uVar2,uVar4);
  uVar1 = uVar2;
  FUN_000ebb70(uVar2,0);
  *(undefined4 *)(unaff_gp + -0x4354) = uVar2;
  *(undefined4 *)(unaff_gp + -0x4350) = *(undefined4 *)(unaff_gp + -0x4358);
  uVar2 = uStack00000004;
  FUN_000eb984(uStack00000004,uVar4);
  FUN_000eba38(uVar2,*(undefined4 *)(unaff_gp + -0x4358));
  uVar4 = uVar3;
  FUN_000eba38(uVar3,uVar2);
  FUN_000ebbcc(uVar3,uVar4);
  *(undefined4 *)(unaff_gp + -0x434c) = uVar3;
  FUN_000ebb70(uVar1,0x40000000);
  *(undefined4 *)(unaff_gp + -0x4348) = uVar1;
  uVar2 = extraout_r11;
  FUN_000ebb70(extraout_r11,*(undefined4 *)(unaff_gp + -0x4358));
  FUN_000eba38(uVar4,uVar2);
  *(undefined4 *)(unaff_gp + -0x4344) = uVar4;
  return;
}


```

## 0xabc52

```c

void FUN_000abc52(undefined4 param_1)

{
  int unaff_gp;
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 extraout_r11;
  undefined4 extraout_r11_00;
  undefined4 uStack00000000;

  *(undefined4 *)(unaff_gp + -0x4358) = 0x3f800000;
  *(undefined4 *)(unaff_gp + -0x4354) = 0x3f800000;
  *(undefined4 *)(unaff_gp + -0x4350) = 0;
  uVar1 = param_1;
  uStack00000000 = param_1;
  FUN_000eba38(param_1,0x3f800000);
  FUN_000ebbcc(param_1,extraout_r11);
  *(undefined4 *)(unaff_gp + -0x434c) = param_1;
  uVar2 = FUN_000ebb70(uVar1,extraout_r11_00);
  *(undefined4 *)(unaff_gp + -0x4348) = uVar1;
  *(undefined4 *)(unaff_gp + -0x4344) = uVar2;
  return;
}


```

## 0xabcaa

```c

void FUN_000abcaa(undefined4 param_1)

{
  int unaff_gp;
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  undefined4 extraout_r11;
  undefined8 uVar4;
  undefined4 uStack00000000;

  *(undefined4 *)(unaff_gp + -0x4358) = param_1;
  uVar2 = param_1;
  uStack00000000 = param_1;
  uVar4 = FUN_000ebb70(param_1,0);
  uVar1 = (undefined4)((ulonglong)uVar4 >> 0x20);
  *(undefined4 *)(unaff_gp + -0x4354) = param_1;
  *(int *)(unaff_gp + -0x4350) = (int)uVar4;
  uVar3 = FUN_000eba38(uVar1,uVar2);
  uVar3 = FUN_000ebbcc(uVar1,uVar3);
  *(undefined4 *)(unaff_gp + -0x434c) = uVar1;
  FUN_000ebb70(uVar2,uVar3);
  *(undefined4 *)(unaff_gp + -0x4348) = uVar2;
  *(undefined4 *)(unaff_gp + -0x4344) = extraout_r11;
  return;
}


```
