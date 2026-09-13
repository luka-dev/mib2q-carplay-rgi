// FD51 G1952 original V850; VA 0x8bd30; file offset 0x6bd30; GP=fedff8b4 TP=28018.

undefined4 FUN_0008bd30(int param_1,char *param_2,int param_3,int param_4)

{
  int iVar1;
  undefined4 uVar2;
  byte bStack_19;

  uVar2 = 0;
  if (*(short *)(param_1 + 2) != 0xff) {
    iVar1 = func_0x0008bf8a(*(undefined2 *)(param_1 + 2),&bStack_19);
    if (iVar1 == 1) {
      *(undefined1 *)((uint)bStack_19 + param_3) = 1;
      *param_2 = *param_2 + '\x01';
      if (*(char *)(param_1 + 0x1b) == '\x02') {
        *(undefined1 *)((uint)bStack_19 + param_4) = 0;
      }
    }
    else {
      *param_2 = '\0';
      uVar2 = 1;
    }
  }
  return uVar2;
}
