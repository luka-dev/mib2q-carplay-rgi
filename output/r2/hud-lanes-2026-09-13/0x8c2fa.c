// FD51 G1952 original V850; VA 0x8c2fa; file offset 0x6c2fa; GP=fedff8b4 TP=28018.

void FUN_0008c2fa(char param_1,uint param_2,uint param_3,int param_4)

{
  param_3 = param_3 & 0xff;
  if (param_3 < (param_2 & 0xff)) {
    if (param_1 == '\x01') {
      *(undefined1 *)(param_3 + param_4) = 1;
    }
    else {
      *(undefined1 *)(param_3 + param_4) = 2;
    }
  }
  return;
}
