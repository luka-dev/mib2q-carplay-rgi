
_APVirtualDisplaySinkCreate:
0x25104efa8:  7f 23 03 d5   pacibsp
0x25104efac:  ff 43 01 d1   sub	sp, sp, #0x50
0x25104efb0:  f8 5f 01 a9   stp	x24, x23, [sp, #0x10]
0x25104efb4:  f6 57 02 a9   stp	x22, x21, [sp, #0x20]
0x25104efb8:  f4 4f 03 a9   stp	x20, x19, [sp, #0x30]
0x25104efbc:  fd 7b 04 a9   stp	fp, lr, [sp, #0x40]
0x25104efc0:  fd 03 01 91   add	fp, sp, #0x40
0x25104efc4:  ff 07 00 f9   str	xzr, [sp, #0x8]
0x25104efc8:  e0 04 00 b4   cbz	x0, loc_25104f064 ; ⤵ 0x9c
0x25104efcc:  f4 03 01 aa   mov	x20, x1
0x25104efd0:  e1 04 00 b4   cbz	x1, loc_25104f06c ; ⤵ 0x9c
0x25104efd4:  f3 03 02 aa   mov	x19, x2
0x25104efd8:  e2 04 00 b4   cbz	x2, loc_25104f074 ; ⤵ 0x9c
0x25104efdc:  f6 03 00 aa   mov	x22, x0
0x25104efe0:  a8 83 0b b0   adrp	x8, 0x2680c3000
0x25104efe4:  08 d1 40 f9   ldr	x8, [x8, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x25104efe8:  15 01 40 f9   ldr	x21, [x8]
0x25104efec:  a1 0c c3 95   bl	j__FigVirtualDisplaySinkGetClassID
0x25104eff0:  e2 03 00 aa   mov	x2, x0
0x25104eff4:  01 55 12 f0   adrp	x1, 0x275af1000
0x25104eff8:  21 80 11 91   add	x1, x1, #0x460 ; _kAPVirtualDisplaySinkVTable
0x25104effc:  e3 23 00 91   add	x3, sp, #0x8
0x25104f000:  e0 03 15 aa   mov	x0, x21
0x25104f004:  8b f9 c2 95   bl	j__CMDerivedObjectCreate
0x25104f008:  f5 03 00 aa   mov	x21, x0
0x25104f00c:  a0 03 00 35   cbnz	w0, loc_25104f080 ; ⤵ 0x74
0x25104f010:  e0 07 40 f9   ldr	x0, [sp, #0x8]
0x25104f014:  6b f9 c2 95   bl	j__CMBaseObjectGetDerivedStorage
0x25104f018:  f7 03 00 aa   mov	x23, x0
0x25104f01c:  e0 03 16 aa   mov	x0, x22
0x25104f020:  d8 ca c2 95   bl	j__CFRetain
0x25104f024:  e0 02 00 f9   str	x0, [x23]
0x25104f028:  e0 03 14 aa   mov	x0, x20
0x25104f02c:  d5 ca c2 95   bl	j__CFRetain
0x25104f030:  e0 0a 00 f9   str	x0, [x23, #0x10]
0x25104f034:  20 00 80 52   mov	w0, #0x1
0x25104f038:  22 fd c2 95   bl	j__SecondsToUpTicks
0x25104f03c:  e0 3e 00 f9   str	x0, [x23, #0x78]
0x25104f040:  e8 07 40 f9   ldr	x8, [sp, #0x8]
0x25104f044:  68 02 00 f9   str	x8, [x19]
0x25104f048:  ; loc_25104f048
0x25104f048:  e0 03 15 aa   mov	x0, x21
0x25104f04c:  fd 7b 44 a9   ldp	fp, lr, [sp, #0x40]
0x25104f050:  f4 4f 43 a9   ldp	x20, x19, [sp, #0x30]
0x25104f054:  f6 57 42 a9   ldp	x22, x21, [sp, #0x20]
0x25104f058:  f8 5f 41 a9   ldp	x24, x23, [sp, #0x10]
0x25104f05c:  ff 43 01 91   add	sp, sp, #0x50
0x25104f060:  ff 0f 5f d6   retab
0x25104f064:  ; loc_25104f064
0x25104f064:  1f 66 03 94   bl	_APVirtualDisplaySinkCreate.cold.3
0x25104f068:  04 00 00 14   b	loc_25104f078 ; ⤵ 0x10
0x25104f06c:  ; loc_25104f06c
0x25104f06c:  18 66 03 94   bl	_APVirtualDisplaySinkCreate.cold.2
0x25104f070:  02 00 00 14   b	loc_25104f078 ; ⤵ 0x8
0x25104f074:  ; loc_25104f074
0x25104f074:  11 66 03 94   bl	_APVirtualDisplaySinkCreate.cold.1
0x25104f078:  ; loc_25104f078
0x25104f078:  75 3d 86 12   mov	w21, #0xffffce14
0x25104f07c:  f3 ff ff 17   b	loc_25104f048 ; ⤴ -0x34
0x25104f080:  ; loc_25104f080
0x25104f080:  61 0b 00 90   adrp	x1, 0x2511bb000
0x25104f084:  21 f8 34 91   add	x1, x1, #0xd3e ; "APVirtualDisplaySinkCreate"
0x25104f088:  e0 03 15 aa   mov	x0, x21
0x25104f08c:  22 19 80 52   mov	w2, #0xc9
0x25104f090:  03 00 80 52   mov	w3, #0
0x25104f094:  b3 f4 c2 95   bl	j__APSLogErrorAt
0x25104f098:  e0 07 40 f9   ldr	x0, [sp, #0x8]
0x25104f09c:  60 fd ff b4   cbz	x0, loc_25104f048 ; ⤴ -0x54
0x25104f0a0:  b4 ca c2 95   bl	j__CFRelease
0x25104f0a4:  e9 ff ff 17   b	loc_25104f048 ; ⤴ -0x5c

