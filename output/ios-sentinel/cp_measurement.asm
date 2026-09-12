
+[CPAccNavUpdate _valueFromMeasurement:forParamKey:]
0x247124ed0:  7f 23 03 d5   pacibsp
0x247124ed4:  e9 23 bc 6d   stp	d9, d8, [sp, #-0x40]!
0x247124ed8:  f6 57 01 a9   stp	x22, x21, [sp, #0x10]
0x247124edc:  f4 4f 02 a9   stp	x20, x19, [sp, #0x20]
0x247124ee0:  fd 7b 03 a9   stp	fp, lr, [sp, #0x30]
0x247124ee4:  fd c3 00 91   add	fp, sp, #0x30
0x247124ee8:  f4 03 03 aa   mov	x20, x3
0x247124eec:  f5 03 00 aa   mov	x21, x0
0x247124ef0:  ec 6b 3d 94   bl	_objc_retain_x2_stub
0x247124ef4:  f3 03 00 aa   mov	x19, x0
0x247124ef8:  a6 63 3d 94   bl	_objc_retain_x20_stub
0x247124efc:  f4 03 00 aa   mov	x20, x0
0x247124f00:  c4 7b 3b 94   bl	0x248003e10
0x247124f04:  7f 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124f08:  f6 03 00 aa   mov	x22, x0
0x247124f0c:  91 63 3d 94   bl	_objc_release_x22_stub
0x247124f10:  e0 03 14 aa   mov	x0, x20
0x247124f14:  b6 03 00 b4   cbz	x22, loc_247124f88 ; ⤵ 0x74
0x247124f18:  be 7b 3b 94   bl	0x248003e10
0x247124f1c:  79 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124f20:  f5 03 00 aa   mov	x21, x0
0x247124f24:  e0 03 13 aa   mov	x0, x19
0x247124f28:  e2 03 15 aa   mov	x2, x21
0x247124f2c:  39 00 3d 94   bl	0x248065010
0x247124f30:  f6 03 00 aa   mov	x22, x0
0x247124f34:  83 63 3d 94   bl	_objc_release_x21_stub
0x247124f38:  b6 04 00 34   cbz	w22, loc_247124fcc ; ⤵ 0x94
0x247124f3c:  e0 03 14 aa   mov	x0, x20
0x247124f40:  b4 7b 3b 94   bl	0x248003e10
0x247124f44:  6f 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124f48:  f5 03 00 aa   mov	x21, x0
0x247124f4c:  e0 03 13 aa   mov	x0, x19
0x247124f50:  e2 03 15 aa   mov	x2, x21
0x247124f54:  37 08 3d 94   bl	0x248067030
0x247124f58:  6a 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124f5c:  f6 03 00 aa   mov	x22, x0
0x247124f60:  48 7c 3b 94   bl	0x248004080
0x247124f64:  08 1c a0 4e   mov	v8.16b, v0.16b
0x247124f68:  7a 63 3d 94   bl	_objc_release_x22_stub
0x247124f6c:  75 63 3d 94   bl	_objc_release_x21_stub
0x247124f70:  e0 03 14 aa   mov	x0, x20
0x247124f74:  23 00 3d 94   bl	0x248065000
0x247124f78:  00 1d a8 4e   mov	v0.16b, v8.16b
0x247124f7c:  86 ff ff 97   bl	_NSNumberForCPAccNavTypeFromDouble
0x247124f80:  60 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124f84:  19 00 00 14   b	loc_247124fe8 ; ⤵ 0x64
0x247124f88:  ; loc_247124f88
0x247124f88:  5a 03 3d 94   bl	0x248065cf0
0x247124f8c:  1f 14 00 71   cmp	w0, #0x5
0x247124f90:  21 02 00 54   b.ne	loc_247124fd4 ; ⤵ 0x44
0x247124f94:  48 7d 10 f0   adrp	x8, 0x2680cf000
0x247124f98:  15 45 43 f9   ldr	x21, [x8, #0x688] ; _ptr.NSNumber
0x247124f9c:  e0 03 13 aa   mov	x0, x19
0x247124fa0:  fc e0 3c 94   bl	0x24805d390
0x247124fa4:  57 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124fa8:  f6 03 00 aa   mov	x22, x0
0x247124fac:  23 04 00 94   bl	_CPDistanceUnitsFromNSUnitLength
0x247124fb0:  e2 03 00 aa   mov	x2, x0
0x247124fb4:  e0 03 15 aa   mov	x0, x21
0x247124fb8:  96 8b 3b 94   bl	0x248007e10
0x247124fbc:  51 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124fc0:  f5 03 00 aa   mov	x21, x0
0x247124fc4:  63 63 3d 94   bl	_objc_release_x22_stub
0x247124fc8:  09 00 00 14   b	loc_247124fec ; ⤵ 0x24
0x247124fcc:  ; loc_247124fcc
0x247124fcc:  15 00 80 d2   mov	x21, #0
0x247124fd0:  07 00 00 14   b	loc_247124fec ; ⤵ 0x1c
0x247124fd4:  ; loc_247124fd4
0x247124fd4:  e0 03 13 aa   mov	x0, x19
0x247124fd8:  2a 7c 3b 94   bl	0x248004080
0x247124fdc:  e0 03 15 aa   mov	x0, x21
0x247124fe0:  40 fe 3c 94   bl	0x2480648e0
0x247124fe4:  47 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247124fe8:  ; loc_247124fe8
0x247124fe8:  f5 03 00 aa   mov	x21, x0
0x247124fec:  ; loc_247124fec
0x247124fec:  51 63 3d 94   bl	_objc_release_x20_stub
0x247124ff0:  4c 63 3d 94   bl	_objc_release_x19_stub
0x247124ff4:  e0 03 15 aa   mov	x0, x21
0x247124ff8:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x247124ffc:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x247125000:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x247125004:  e9 23 c4 6c   ldp	d9, d8, [sp], #0x40
0x247125008:  ff 23 03 d5   autibsp
0x24712500c:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x247125010:  50 00 f0 b6   tbz	x16, #0x3e, loc_247125018 ; ⤵ 0x8
0x247125014:  20 8e 38 d4   brk	#0xc471
0x247125018:  ; loc_247125018
0x247125018:  1e 6b 3d 14   b	_objc_autoreleaseReturnValue_stub

