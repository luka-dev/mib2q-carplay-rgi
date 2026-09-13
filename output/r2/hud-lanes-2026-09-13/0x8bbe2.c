// FD51 G1952 original V850; VA 0x8bbe2; file offset 0x6bbe2; GP=fedff8b4 TP=28018.

void FUN_0008bbe2(char *param_1,char param_2,undefined2 *param_3)

{
  if (*(int *)(param_1 + 0x1c) == 1) {
    *param_3 = 0x13b;
  }
  else if (*param_1 == '\x01') {
    if (*(short *)(param_1 + 0x16) == 8) {
      if (param_2 == '\x01') {
        *param_3 = 0x139;
      }
      else {
        *param_3 = 0x13a;
      }
    }
    else {
      func_0x0008bc30();
    }
  }
  return;
}
