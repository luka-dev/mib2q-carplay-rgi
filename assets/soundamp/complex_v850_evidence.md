# J525 complex block V850 controls

2026-09-08. Ghidra decompilation, CodeFlash SHA-256 `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Unresolved `unaff_r20` is the packet ring mask, not an extra algorithm input. No commands executed on hardware.

## FUN_000aa89a

```c

void FUN_000aa89a(void)

{
  int unaff_gp;
  uint unaff_r20;

  thunk_FUN_000a8c14(1,5,1);
  *(undefined4 *)(unaff_gp + -0x4334) = 1;
  *(undefined4 *)((unaff_r20 & 4) * 4 + unaff_gp + -0x4340) = 0xc4d3f;
  *(undefined4 *)(((unaff_r20 & 4) + 1 & unaff_r20) * 4 + unaff_gp + -0x4340) = 1;
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x1c);
  return;
}

```

## FUN_000aa912

```c

void FUN_000aa912(void)

{
  int unaff_gp;
  int iVar1;
  uint unaff_r20;
  uint uVar2;

  thunk_FUN_000a8c14(1,5,1);
  iVar1 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 1;
  uVar2 = (unaff_r20 & 4) + 1 & unaff_r20;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar1) = 0xc4d26;
  if ((*(char *)(unaff_gp + -0x3d62) == -0x5b) && (*(char *)(unaff_gp + -0x1250c) == -0x5b)) {
    *(undefined4 *)(uVar2 * 4 + iVar1) = 1;
  }
  else {
    *(undefined4 *)(uVar2 * 4 + iVar1) = 0;
  }
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x1c);
  return;
}

```

## FUN_000aa9b6

```c

void FUN_000aa9b6(void)

{
  int unaff_gp;
  int iVar1;
  uint unaff_r20;
  uint uVar2;

  thunk_FUN_000a8c14(1,5,1);
  iVar1 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 1;
  uVar2 = (unaff_r20 & 4) + 1 & unaff_r20;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar1) = 0xc4d3e;
  if ((*(char *)(unaff_gp + -0x3d62) == -0x5b) || (*(char *)(unaff_gp + -0x3d62) == 'Z')) {
    *(undefined4 *)(uVar2 * 4 + iVar1) = 4;
  }
  else {
    *(undefined4 *)(uVar2 * 4 + iVar1) = 2;
  }
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x1c);
  return;
}

```

## FUN_000aafc0

```c

void FUN_000aafc0(void)

{
  int unaff_gp;
  int iVar1;
  uint unaff_r20;
  uint uVar2;
  uint uVar3;

  thunk_FUN_000a8c14(1,5,2);
  iVar1 = unaff_gp + -0x4340;
  *(undefined4 *)(unaff_gp + -0x4334) = 1;
  uVar2 = (unaff_r20 & 4) + 1 & unaff_r20;
  *(undefined4 *)((unaff_r20 & 4) * 4 + iVar1) = 0xc4d27;
  if (*(char *)(unaff_gp + -0x1250c) == -0x5b) {
    *(uint *)(uVar2 * 4 + iVar1) =
         *(byte *)(unaff_gp + (uint)*(byte *)(unaff_gp + -0x4592) * 0x13 + -0x129ec) / 10;
  }
  else {
    *(undefined4 *)(uVar2 * 4 + iVar1) = 0;
  }
  uVar2 = uVar2 + 1 & unaff_r20;
  uVar3 = uVar2 + 1 & unaff_r20;
  *(undefined4 *)(uVar2 * 4 + iVar1) = 1;
  *(undefined4 *)(uVar3 * 4 + iVar1) = 0xc4d32;
  *(undefined4 *)((uVar3 + 1 & unaff_r20) * 4 + iVar1) = 0x3f800000;
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e35c8(unaff_gp + -0x44d8,0x28);
  return;
}

```

## FUN_000ab1be

```c

void FUN_000ab1be(void)

{
  int unaff_gp;

  thunk_FUN_000a8c64(0xc4d21,1);
  thunk_FUN_000a8d54();
  thunk_FUN_000a8cfc();
  thunk_FUN_000e36d2(unaff_gp + -0x44d8,unaff_gp + -0x4340);
  if (*(int *)(unaff_gp + -0x4340) == 1) {
    *(undefined1 *)(unaff_gp + -0x3d62) = 0xa5;
  }
  if (*(char *)(unaff_gp + -0x3d5f) == '\x02') {
    thunk_FUN_000dd374(2,0xb);
  }
  else if (*(char *)(unaff_gp + -0x3d5f) == '\a') {
    thunk_FUN_000dd374(7,0xf);
  }
  return;
}

```
