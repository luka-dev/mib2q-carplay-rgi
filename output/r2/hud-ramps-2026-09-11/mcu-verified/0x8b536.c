// Original V850 FD51 G1952; linked VA 0x8b536; file offset 0x6b536; GP=fedff8b4, TP=28018.
// Analysis pseudocode: inferred prototypes/stack variables are not source code.

void FUN_0008b536(byte *param_1,undefined4 param_2,uint param_3,char param_4,int param_5,
                 byte param_6,undefined2 param_7)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  byte bStack_6e;
  undefined1 auStack_6d [17];
  undefined4 auStack_5c [17];
  
  param_3 = param_3 & 0xff;
  FUN_000b5188(auStack_5c,0,0x44);
  uVar4 = 0x11;
  FUN_000b5188(auStack_6d,0);
  uVar5 = (uint)*(byte *)(param_3 * 2 + 0x27d9c);
  uVar2 = (uint)*(byte *)(param_3 * 2 + 0x27d9d);
  bStack_6e = uVar2 != 0;
  if ((bool)bStack_6e) {
    auStack_5c[0] = *(undefined4 *)(uVar2 * 8 + 0x27990);
    auStack_6d[0] = *(undefined1 *)(uVar2 * 8 + 0x27994);
  }
  if (param_3 != 4) {
    uVar4 = FUN_0008b6fa(auStack_5c,auStack_6d,&bStack_6e,uVar5,param_7);
  }
  if (param_4 == '\x01') {
    uVar2 = 0;
    while ((uVar2 < param_6 && (bStack_6e < 0x11))) {
      bVar1 = false;
      uVar3 = 0;
      while ((uVar3 < 0x13 && (!bVar1))) {
        if ((*(char *)(uVar2 + param_5) == *(char *)(uVar3 + 0x27dfe)) && (uVar3 != uVar4)) {
          auStack_5c[bStack_6e] = *(undefined4 *)((uVar3 + uVar5 * 0x13) * 8 + 0x39e24);
          auStack_6d[bStack_6e] = *(undefined1 *)((uVar3 + uVar5 * 0x13) * 8 + 0x39e28);
          bStack_6e = bStack_6e + 1;
          bVar1 = true;
        }
        uVar3 = uVar3 + 1 & 0xff;
      }
      uVar2 = uVar2 + 1 & 0xff;
    }
  }
  FUN_0008b798(auStack_5c,auStack_6d,bStack_6e);
  *param_1 = bStack_6e;
  thunk_FUN_000b507a(param_2,auStack_5c,(uint)bStack_6e << 2);
  return;
}

