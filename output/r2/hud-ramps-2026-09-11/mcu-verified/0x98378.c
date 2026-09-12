// Original V850 FD51 G1952; linked VA 0x98378; file offset 0x78378; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

undefined4 FUN_00098378(int param_1,int param_2)

{
  bool bVar1;
  undefined4 uVar2;
  uint uVar3;
  
  uVar2 = 0;
  bVar1 = true;
  if (param_2 != 0) {
    uVar3 = 0;
    while ((uVar3 < 0x1f && (bVar1))) {
      if (*(int *)(uVar3 * 0xc + 0x29030) == param_1) {
        thunk_FUN_000b507a(param_2,*(undefined4 *)(uVar3 * 0xc + 0x29034),
                           *(undefined1 *)(uVar3 * 0xc + 0x29038));
        uVar2 = 1;
        bVar1 = false;
      }
      uVar3 = uVar3 + 1 & 0xff;
    }
  }
  return uVar2;
}

