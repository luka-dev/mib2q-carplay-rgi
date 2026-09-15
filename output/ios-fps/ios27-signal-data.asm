
_screenstream_signalDataAvailable:
0x250f52c70:  7f 23 03 d5   pacibsp
0x250f52c74:  f4 4f be a9   stp	x20, x19, [sp, #-0x20]!
0x250f52c78:  fd 7b 01 a9   stp	fp, lr, [sp, #0x10]
0x250f52c7c:  fd 43 00 91   add	fp, sp, #0x10
0x250f52c80:  13 44 40 f9   ldr	x19, [x0, #0x88]
0x250f52c84:  d3 03 00 b4   cbz	x19, loc_250f52cfc ; ⤵ 0x78
0x250f52c88:  e0 03 13 aa   mov	x0, x19
0x250f52c8c:  75 e3 c6 95   bl	j__CMBaseObjectGetVTable
0x250f52c90:  10 0c 41 f8   ldr	x16, [x0, #0x10]!
0x250f52c94:  f1 03 00 aa   mov	x17, x0
0x250f52c98:  f1 80 e3 f2   movk	x17, #0x1c07, lsl #0x30
0x250f52c9c:  30 1a c1 da   autda	x16, x17
0x250f52ca0:  09 8e 44 f8   ldr	x9, [x16, #0x48]!
0x250f52ca4:  69 02 00 b4   cbz	x9, loc_250f52cf0 ; ⤵ 0x4c
0x250f52ca8:  e8 03 10 aa   mov	x8, x16
0x250f52cac:  68 ee f9 f2   movk	x8, #0xcf73, lsl #0x30
0x250f52cb0:  f0 03 09 aa   mov	x16, x9
0x250f52cb4:  f1 03 08 aa   mov	x17, x8
0x250f52cb8:  30 12 c1 da   autia	x16, x17
0x250f52cbc:  f1 03 10 aa   mov	x17, x16
0x250f52cc0:  f1 43 c1 da   xpaci	x17
0x250f52cc4:  1f 02 11 eb   cmp	x16, x17
0x250f52cc8:  40 00 00 54   b.eq	loc_250f52cd0 ; ⤵ 0x8
0x250f52ccc:  00 8e 38 d4   brk	#0xc470
0x250f52cd0:  ; loc_250f52cd0
0x250f52cd0:  f0 23 c1 da   paciza	x16
0x250f52cd4:  f0 00 00 b4   cbz	x16, loc_250f52cf0 ; ⤵ 0x1c
0x250f52cd8:  e0 03 13 aa   mov	x0, x19
0x250f52cdc:  28 09 3f d7   blraa	x9, x8
0x250f52ce0:  a0 00 00 35   cbnz	w0, loc_250f52cf4 ; ⤵ 0x14
0x250f52ce4:  ; loc_250f52ce4
0x250f52ce4:  fd 7b 41 a9   ldp	fp, lr, [sp, #0x10]
0x250f52ce8:  f4 4f c2 a8   ldp	x20, x19, [sp], #0x20
0x250f52cec:  ff 0f 5f d6   retab
0x250f52cf0:  ; loc_250f52cf0
0x250f52cf0:  a0 3d 86 12   mov	w0, #0xffffce12
0x250f52cf4:  ; loc_250f52cf4
0x250f52cf4:  34 95 08 94   bl	_screenstream_signalDataAvailable.cold.1
0x250f52cf8:  fb ff ff 17   b	loc_250f52ce4 ; ⤴ -0x14
0x250f52cfc:  ; loc_250f52cfc
0x250f52cfc:  36 95 08 94   bl	_screenstream_signalDataAvailable.cold.2
0x250f52d00:  f9 ff ff 17   b	loc_250f52ce4 ; ⤴ -0x1c

