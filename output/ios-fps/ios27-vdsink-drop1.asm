
_vdsink_ShouldDropFrame:
0x250f52e80:  7f 23 03 d5   pacibsp
0x250f52e84:  ff 03 02 d1   sub	sp, sp, #0x80
0x250f52e88:  f6 57 05 a9   stp	x22, x21, [sp, #0x50]
0x250f52e8c:  f4 4f 06 a9   stp	x20, x19, [sp, #0x60]
0x250f52e90:  fd 7b 07 a9   stp	fp, lr, [sp, #0x70]
0x250f52e94:  fd c3 01 91   add	fp, sp, #0x70
0x250f52e98:  ca e9 c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f52e9c:  f3 03 00 aa   mov	x19, x0
0x250f52ea0:  ff 7f 04 a9   stp	xzr, xzr, [sp, #0x40]
0x250f52ea4:  ff 1f 00 f9   str	xzr, [sp, #0x38]
0x250f52ea8:  00 08 40 f9   ldr	x0, [x0, #0x10]
0x250f52eac:  e8 e3 00 91   add	x8, sp, #0x38
0x250f52eb0:  c8 f9 c6 95   bl	j__CMBufferQueueGetDuration
0x250f52eb4:  e8 47 40 b9   ldr	w8, [sp, #0x44]
0x250f52eb8:  a9 03 80 52   mov	w9, #0x1d
0x250f52ebc:  08 01 09 0a   and	w8, w8, w9
0x250f52ec0:  1f 05 00 71   cmp	w8, #0x1
0x250f52ec4:  c1 03 00 54   b.ne	loc_250f52f3c ; ⤵ 0x78
0x250f52ec8:  e0 83 c3 3c   ldur	q0, [sp, #0x38]
0x250f52ecc:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250f52ed0:  e8 27 40 f9   ldr	x8, [sp, #0x48]
0x250f52ed4:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250f52ed8:  60 0a c0 3d   ldr	q0, [x19, #0x20]
0x250f52edc:  e0 03 80 3d   str	q0, [sp]
0x250f52ee0:  68 1a 40 f9   ldr	x8, [x19, #0x30]
0x250f52ee4:  e8 0b 00 f9   str	x8, [sp, #0x10]
0x250f52ee8:  e0 83 00 91   add	x0, sp, #0x20
0x250f52eec:  e1 03 00 91   mov	x1, sp
0x250f52ef0:  fc e9 c6 95   bl	j__CMTimeCompare
0x250f52ef4:  40 02 f8 37   tbnz	w0, #0x1f, loc_250f52f3c ; ⤵ 0x48
0x250f52ef8:  08 c0 0e d0   adrp	x8, 0x26e754000
0x250f52efc:  08 39 4f b9   ldr	w8, [x8, #0xf38] ; _gLogCategory_APVirtualDisplaySink
0x250f52f00:  1f a1 00 71   cmp	w8, #0x28
0x250f52f04:  2c 01 00 54   b.gt	loc_250f52f28 ; ⤵ 0x24
0x250f52f08:  1f 05 00 31   cmn	w8, #0x1
0x250f52f0c:  a1 05 00 54   b.ne	loc_250f52fc0 ; ⤵ 0xb4
0x250f52f10:  00 c0 0e d0   adrp	x0, 0x26e754000
0x250f52f14:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f52f18:  01 05 80 52   mov	w1, #0x28
0x250f52f1c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f52f20:  a8 b7 c6 95   bl	j___LogCategory_Initialize
0x250f52f24:  e0 04 00 35   cbnz	w0, loc_250f52fc0 ; ⤵ 0x9c
0x250f52f28:  ; loc_250f52f28
0x250f52f28:  68 8a 40 b9   ldr	w8, [x19, #0x88]
0x250f52f2c:  08 05 00 11   add	w8, w8, #0x1
0x250f52f30:  68 8a 00 b9   str	w8, [x19, #0x88]
0x250f52f34:  34 00 80 52   mov	w20, #0x1
0x250f52f38:  02 00 00 14   b	loc_250f52f40 ; ⤵ 0x8
0x250f52f3c:  ; loc_250f52f3c
0x250f52f3c:  14 00 80 52   mov	w20, #0
0x250f52f40:  ; loc_250f52f40
0x250f52f40:  f5 03 13 aa   mov	x21, x19
0x250f52f44:  a8 8e 48 b8   ldr	w8, [x21, #0x88]!
0x250f52f48:  1f 05 00 71   cmp	w8, #0x1
0x250f52f4c:  eb 02 00 54   b.lt	loc_250f52fa8 ; ⤵ 0x5c
0x250f52f50:  a8 dc c6 95   bl	j__mach_absolute_time
0x250f52f54:  69 a2 47 a9   ldp	x9, x8, [x19, #0x78]
0x250f52f58:  08 00 08 cb   sub	x8, x0, x8
0x250f52f5c:  1f 01 09 eb   cmp	x8, x9
0x250f52f60:  43 02 00 54   b.lo	loc_250f52fa8 ; ⤵ 0x48
0x250f52f64:  f6 03 00 aa   mov	x22, x0
0x250f52f68:  08 c0 0e d0   adrp	x8, 0x26e754000
0x250f52f6c:  08 39 4f b9   ldr	w8, [x8, #0xf38] ; _gLogCategory_APVirtualDisplaySink
0x250f52f70:  1f f1 00 71   cmp	w8, #0x3c
0x250f52f74:  6c 01 00 54   b.gt	loc_250f52fa0 ; ⤵ 0x2c
0x250f52f78:  1f 05 00 31   cmn	w8, #0x1
0x250f52f7c:  a1 02 00 54   b.ne	loc_250f52fd0 ; ⤵ 0x54
0x250f52f80:  88 02 80 52   mov	w8, #0x14
0x250f52f84:  09 05 80 52   mov	w9, #0x28
0x250f52f88:  09 40 a0 72   movk	w9, #0x200, lsl #0x10
0x250f52f8c:  00 c0 0e d0   adrp	x0, 0x26e754000
0x250f52f90:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f52f94:  21 01 08 2a   orr	w1, w9, w8
0x250f52f98:  8a b7 c6 95   bl	j___LogCategory_Initialize
0x250f52f9c:  a0 01 00 35   cbnz	w0, loc_250f52fd0 ; ⤵ 0x34
0x250f52fa0:  ; loc_250f52fa0
0x250f52fa0:  7f 8a 00 b9   str	wzr, [x19, #0x88]
0x250f52fa4:  76 42 00 f9   str	x22, [x19, #0x80]
0x250f52fa8:  ; loc_250f52fa8
0x250f52fa8:  e0 03 14 aa   mov	x0, x20
0x250f52fac:  fd 7b 47 a9   ldp	fp, lr, [sp, #0x70]
0x250f52fb0:  f4 4f 46 a9   ldp	x20, x19, [sp, #0x60]
0x250f52fb4:  f6 57 45 a9   ldp	x22, x21, [sp, #0x50]
0x250f52fb8:  ff 03 02 91   add	sp, sp, #0x80
0x250f52fbc:  ff 0f 5f d6   retab
0x250f52fc0:  ; loc_250f52fc0
0x250f52fc0:  e1 e3 00 91   add	x1, sp, #0x38
0x250f52fc4:  e0 03 13 aa   mov	x0, x19
0x250f52fc8:  fc 56 07 94   bl	_vdsink_ShouldDropFrame.cold.1
0x250f52fcc:  d7 ff ff 17   b	loc_250f52f28 ; ⤴ -0xa4
0x250f52fd0:  ; loc_250f52fd0
0x250f52fd0:  e0 03 13 aa   mov	x0, x19
0x250f52fd4:  e1 03 15 aa   mov	x1, x21
0x250f52fd8:  16 57 07 94   bl	_vdsink_ShouldDropFrame.cold.2
0x250f52fdc:  f1 ff ff 17   b	loc_250f52fa0 ; ⤴ -0x3c

