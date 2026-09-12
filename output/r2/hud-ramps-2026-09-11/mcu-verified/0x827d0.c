// Original V850 FD51 G1952; linked VA 0x827d0; file offset 0x627d0; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

uint FUN_000827d0(int param_1,uint param_2,undefined1 *param_3)

{
  uint uVar1;
  uint uVar2;
  undefined1 uVar3;
  int iVar4;
  undefined1 local_214 [4];
  undefined1 local_210 [508];
  
  FUN_000b5188(local_214,0,0x200);
  uVar1 = 0;
  if ((param_1 != 0) && (param_3 != (undefined1 *)0x0)) {
    local_214[0] = 1;
    local_214[1] = (undefined1)param_2;
    local_214[2] = (undefined1)(param_2 >> 8);
    local_214[3] = uRamfedfbf3a;
    uVar1 = 4;
    for (uVar2 = 0; (uVar2 < (param_2 & 0xffff) && (uVar2 < 0x200)); uVar2 = uVar2 + 1 & 0xffff) {
      local_210[uVar2] = *(undefined1 *)(uVar2 + param_1);
      uVar1 = uVar1 + 1 & 0xffff;
    }
    uVar3 = FUN_00082cae(local_214,4,param_2 & 0xffff,0x200);
    local_214[uVar1] = uVar3;
    iVar4 = FUN_0008277e(local_214,uVar1 + 1 & 0xffff,param_3 + 2);
    *param_3 = 0x10;
    param_3[1] = 2;
    uVar1 = iVar4 + 2U & 0xffff;
    param_3[uVar1] = 0x10;
    param_3[uVar1 + 1] = 3;
    uVar1 = uVar1 + 2 & 0xffff;
  }
  return uVar1;
}

