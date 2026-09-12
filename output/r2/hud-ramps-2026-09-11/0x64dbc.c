
void FUN_00064dbc(void)

{
  uint uVar1;
  int iVar2;
  undefined4 local_34;
  undefined2 uStack_30;
  undefined4 local_2c;
  undefined2 uStack_28;
  undefined4 local_24;
  undefined2 uStack_20;
  undefined4 local_1c;
  undefined2 uStack_18;
  undefined1 local_12;
  undefined1 uStack_11;
  undefined1 uStack_10;
  undefined1 uStack_f;
  undefined1 uStack_e;
  undefined1 uStack_d;
  
  uVar1 = FUN_00091430();
  if (((cRamfedf2e17 != -1) && (cRamfedf2e16 != -1)) && (cRamfedf2e16 != cRamfedf2e17)) {
    FUN_0008c72a(2,0,bRamfedf2e1b);
  }
  if ((((uRamfedf2e48 & 0x4000) != 0) || ((uRamfedf2e44 & 0x4000) != 0)) &&
     (((uRamfedf2e48 & 0x400) == 0 && ((uRamfedf2e44 & 0x400) == 0)))) {
    iVar2 = FUN_0008e20c(bRamfedf2e1b);
    if (iVar2 == 0) {
      if (uVar1 == bRamfedf2e1b) {
        FUN_000914f0(bRamfedf2e1b,10,0xfedf0343);
      }
      FUN_0008c72a(0xe,0,bRamfedf2e1b);
    }
    else {
      uStack_10 = uRamfedf2e19;
      local_1c = CONCAT13(uStack_f,CONCAT12(uRamfedf2e19,CONCAT11(uStack_11,local_12)));
      uStack_18 = CONCAT11(uStack_d,uStack_e);
      FUN_00091464(1,bRamfedf2e1b,local_1c,uStack_18,0);
      FUN_0008c72a(10,0,bRamfedf2e1b);
    }
  }
  if ((((uRamfedf2e48 & 0x4000) != 0) || ((uRamfedf2e44 & 0x4000) != 0)) &&
     (((uRamfedf2e48 & 0x400) != 0 || ((uRamfedf2e44 & 0x400) != 0)))) {
    iVar2 = FUN_0008e20c(bRamfedf2e1b);
    if ((iVar2 != 0) && ((uRamfedf2e44 & 0x4000) == 0)) {
      uStack_10 = uRamfedf2e19;
      local_24 = CONCAT13(uStack_f,CONCAT12(uRamfedf2e19,CONCAT11(uStack_11,local_12)));
      uStack_20 = CONCAT11(uStack_d,uStack_e);
      FUN_00091464(1,bRamfedf2e1b,local_24,uStack_20,0);
      FUN_0008c72a(10,0,bRamfedf2e1b);
    }
    if (iVar2 == 0) {
      if (uVar1 == bRamfedf2e1b) {
        FUN_000914f0(bRamfedf2e1b,10,0xfedf0343);
      }
      FUN_0008c72a(0xe,0,bRamfedf2e1b);
    }
  }
  if (((((uRamfedf2e48 & 0x400) != 0) || ((uRamfedf2e44 & 0x400) != 0)) &&
      ((uRamfedf2e48 & 0x4000) == 0)) &&
     (((uRamfedf2e44 & 0x4000) == 0 && (iVar2 = FUN_0008e20c(bRamfedf2e1b), iVar2 == 0)))) {
    if (uVar1 == bRamfedf2e1b) {
      FUN_000914f0(bRamfedf2e1b,10,0xfedf0343);
    }
    FUN_0008c72a(0xe,0,bRamfedf2e1b);
  }
  if ((((uRamfedf2e48 & 0x10000) != 0) || ((uRamfedf2e44 & 0x10000) != 0)) &&
     (((uRamfedf2e48 & 0x1000) == 0 && ((uRamfedf2e44 & 0x1000) == 0)))) {
    iVar2 = FUN_0008e34a(bRamfedf2e1b);
    if (iVar2 == 0) {
      if (uVar1 == bRamfedf2e1b) {
        FUN_000914f0(bRamfedf2e1b,0xc,0xfedf035b);
      }
      FUN_0008c72a(0x10,0,bRamfedf2e1b);
    }
    else {
      local_2c = CONCAT13(uStack_f,CONCAT12(uStack_10,CONCAT11(uStack_11,local_12)));
      uStack_28 = CONCAT11(uStack_d,uRamfedf2e18);
      FUN_00091464(3,bRamfedf2e1b,local_2c,uStack_28,0);
      FUN_0008c72a(0xc,0,bRamfedf2e1b);
    }
  }
  if ((((uRamfedf2e48 & 0x10000) != 0) || ((uRamfedf2e44 & 0x10000) != 0)) &&
     (((uRamfedf2e48 & 0x1000) != 0 || ((uRamfedf2e44 & 0x1000) != 0)))) {
    iVar2 = FUN_0008e34a(bRamfedf2e1b);
    if ((iVar2 != 0) && ((uRamfedf2e44 & 0x10000) == 0)) {
      local_34 = CONCAT13(uStack_f,CONCAT12(uStack_10,CONCAT11(uStack_11,local_12)));
      uStack_30 = CONCAT11(uStack_d,uRamfedf2e18);
      FUN_00091464(3,bRamfedf2e1b,local_34,uStack_30,0);
      FUN_0008c72a(0xc,0,bRamfedf2e1b);
    }
    if (iVar2 == 0) {
      if (uVar1 == bRamfedf2e1b) {
        FUN_000914f0(bRamfedf2e1b,0xc,0xfedf035b);
      }
      FUN_0008c72a(0x10,0,bRamfedf2e1b);
    }
  }
  if (((((uRamfedf2e48 & 0x1000) != 0) || ((uRamfedf2e44 & 0x1000) != 0)) &&
      ((uRamfedf2e48 & 0x10000) == 0)) &&
     (((uRamfedf2e44 & 0x10000) == 0 && (iVar2 = FUN_0008e34a(bRamfedf2e1b), iVar2 == 0)))) {
    if (uVar1 == bRamfedf2e1b) {
      FUN_000914f0(bRamfedf2e1b,0xc,0xfedf035b);
    }
    FUN_0008c72a(0x10,0,bRamfedf2e1b);
  }
  iVar2 = FUN_00091436(bRamfedf2e1b);
  if ((uVar1 == bRamfedf2e1b) && (iVar2 != 0)) {
    FUN_00065dce(uVar1,10,1,0);
  }
  uRamfedf2e48 = 0;
  uRamfedf2e44 = 0;
  bRamfedf2e1b = 0;
  return;
}


