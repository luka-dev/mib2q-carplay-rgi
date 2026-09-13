// FD51 G1952 original V850; VA 0x95bd0; file offset 0x75bd0; GP=fedff8b4 TP=28018.

undefined4 FUN_00095bd0(void)

{
  undefined4 uVar1;

  uVar1 = 0;
  if ((((cRamfedfc6d1 == '\x01') || (cRamfedfc6d1 == '\x03')) || (cRamfedfc6d1 == '\x05')) &&
     (bRamfedfc6d0 < 3)) {
    uVar1 = 1;
  }
  return uVar1;
}
