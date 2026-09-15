
-[APRKMediaPlayer _serializeTimeRanges:]
0x250eec904:  7f 23 03 d5   pacibsp
0x250eec908:  ff 43 06 d1   sub	sp, sp, #0x190
0x250eec90c:  fc 6f 13 a9   stp	x28, x27, [sp, #0x130]
0x250eec910:  fa 67 14 a9   stp	x26, x25, [sp, #0x140]
0x250eec914:  f8 5f 15 a9   stp	x24, x23, [sp, #0x150]
0x250eec918:  f6 57 16 a9   stp	x22, x21, [sp, #0x160]
0x250eec91c:  f4 4f 17 a9   stp	x20, x19, [sp, #0x170]
0x250eec920:  fd 7b 18 a9   stp	fp, lr, [sp, #0x180]
0x250eec924:  fd 03 06 91   add	fp, sp, #0x180
0x250eec928:  c8 95 0b 90   adrp	x8, 0x2681a4000
0x250eec92c:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.___stack_chk_guard
0x250eec930:  08 01 40 f9   ldr	x8, [x8]
0x250eec934:  a8 03 1a f8   stur	x8, [fp, #-0x60]
0x250eec938:  4e 4f c8 95   bl	_objc_retain_x2_stub
0x250eec93c:  f3 03 00 aa   mov	x19, x0
0x250eec940:  80 0b 00 b4   cbz	x0, loc_250eecab0 ; ⤵ 0x170
0x250eec944:  a8 8e 0b d0   adrp	x8, 0x2680c2000
0x250eec948:  00 99 47 f9   ldr	x0, [x8, #0xf30] ; _ptr.NSMutableArray
0x250eec94c:  1d 52 c4 95   bl	0x2580011c0
0x250eec950:  d4 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eec954:  f4 03 00 aa   mov	x20, x0
0x250eec958:  f3 03 00 f9   str	x19, [sp]
0x250eec95c:  41 4f c8 95   bl	_objc_retain_x19_stub
0x250eec960:  00 e4 00 6f   movi	v0.2d, #0
0x250eec964:  e0 03 03 ad   stp	q0, q0, [sp, #0x60]
0x250eec968:  e0 03 04 ad   stp	q0, q0, [sp, #0x80]
0x250eec96c:  e2 83 01 91   add	x2, sp, #0x60
0x250eec970:  e3 83 02 91   add	x3, sp, #0xa0
0x250eec974:  e0 07 00 f9   str	x0, [sp, #0x8]
0x250eec978:  04 02 80 52   mov	w4, #0x10
0x250eec97c:  25 52 c4 95   bl	0x258001210
0x250eec980:  00 09 00 b4   cbz	x0, loc_250eecaa0 ; ⤵ 0x120
0x250eec984:  f7 03 00 aa   mov	x23, x0
0x250eec988:  e8 3b 40 f9   ldr	x8, [sp, #0x70]
0x250eec98c:  1b 01 40 f9   ldr	x27, [x8]
0x250eec990:  c8 8e 0b b0   adrp	x8, 0x2680c5000
0x250eec994:  08 2d 40 f9   ldr	x8, [x8, #0x58] ; _ptr._kFigEndpointPlaybackSessionRangeKey_Start
0x250eec998:  16 01 40 f9   ldr	x22, [x8]
0x250eec99c:  fc c3 00 91   add	x28, sp, #0x30
0x250eec9a0:  c8 8e 0b b0   adrp	x8, 0x2680c5000
0x250eec9a4:  08 29 40 f9   ldr	x8, [x8, #0x50] ; _ptr._kFigEndpointPlaybackSessionRangeKey_Duration
0x250eec9a8:  18 01 40 f9   ldr	x24, [x8]
0x250eec9ac:  b3 8e 0b d0   adrp	x19, 0x2680c2000
0x250eec9b0:  ; loc_250eec9b0
0x250eec9b0:  15 00 80 d2   mov	x21, #0
0x250eec9b4:  ; loc_250eec9b4
0x250eec9b4:  e8 3b 40 f9   ldr	x8, [sp, #0x70]
0x250eec9b8:  08 01 40 f9   ldr	x8, [x8]
0x250eec9bc:  1f 01 1b eb   cmp	x8, x27
0x250eec9c0:  60 00 00 54   b.eq	loc_250eec9cc ; ⤵ 0xc
0x250eec9c4:  e0 07 40 f9   ldr	x0, [sp, #0x8]
0x250eec9c8:  ba 50 c8 95   bl	_objc_enumerationMutation_stub
0x250eec9cc:  ; loc_250eec9cc
0x250eec9cc:  e8 37 40 f9   ldr	x8, [sp, #0x68]
0x250eec9d0:  00 79 75 f8   ldr	x0, [x8, x21, lsl #0x3]
0x250eec9d4:  00 e4 00 6f   movi	v0.2d, #0
0x250eec9d8:  e0 03 02 ad   stp	q0, q0, [sp, #0x40]
0x250eec9dc:  e0 0f 80 3d   str	q0, [sp, #0x30]
0x250eec9e0:  60 00 00 b4   cbz	x0, loc_250eec9ec ; ⤵ 0xc
0x250eec9e4:  e8 c3 00 91   add	x8, sp, #0x30
0x250eec9e8:  4a d0 c4 95   bl	0x258020b10
0x250eec9ec:  ; loc_250eec9ec
0x250eec9ec:  60 aa 47 f9   ldr	x0, [x19, #0xf50]
0x250eec9f0:  1c 51 c8 95   bl	_objc_alloc_init_stub
0x250eec9f4:  f9 03 00 aa   mov	x25, x0
0x250eec9f8:  e0 0f c0 3d   ldr	q0, [sp, #0x30]
0x250eec9fc:  e0 07 80 3d   str	q0, [sp, #0x10]
0x250eeca00:  e8 23 40 f9   ldr	x8, [sp, #0x40]
0x250eeca04:  e8 13 00 f9   str	x8, [sp, #0x20]
0x250eeca08:  e0 43 00 91   add	x0, sp, #0x10
0x250eeca0c:  01 00 80 d2   mov	x1, #0
0x250eeca10:  38 83 c8 95   bl	_CMTimeCopyAsDictionary_stub
0x250eeca14:  e0 00 00 b4   cbz	x0, loc_250eeca30 ; ⤵ 0x1c
0x250eeca18:  fa 03 00 aa   mov	x26, x0
0x250eeca1c:  e0 03 19 aa   mov	x0, x25
0x250eeca20:  e2 03 1a aa   mov	x2, x26
0x250eeca24:  e3 03 16 aa   mov	x3, x22
0x250eeca28:  02 54 c4 95   bl	0x258001a30
0x250eeca2c:  f5 4e c8 95   bl	_objc_release_x26_stub
0x250eeca30:  ; loc_250eeca30
0x250eeca30:  80 83 c1 3c   ldur	q0, [x28, #0x18]
0x250eeca34:  e0 07 80 3d   str	q0, [sp, #0x10]
0x250eeca38:  88 83 42 f8   ldur	x8, [x28, #0x28]
0x250eeca3c:  e8 13 00 f9   str	x8, [sp, #0x20]
0x250eeca40:  e0 43 00 91   add	x0, sp, #0x10
0x250eeca44:  01 00 80 d2   mov	x1, #0
0x250eeca48:  2a 83 c8 95   bl	_CMTimeCopyAsDictionary_stub
0x250eeca4c:  e0 00 00 b4   cbz	x0, loc_250eeca68 ; ⤵ 0x1c
0x250eeca50:  fa 03 00 aa   mov	x26, x0
0x250eeca54:  e0 03 19 aa   mov	x0, x25
0x250eeca58:  e2 03 1a aa   mov	x2, x26
0x250eeca5c:  e3 03 18 aa   mov	x3, x24
0x250eeca60:  f4 53 c4 95   bl	0x258001a30
0x250eeca64:  e7 4e c8 95   bl	_objc_release_x26_stub
0x250eeca68:  ; loc_250eeca68
0x250eeca68:  e0 03 14 aa   mov	x0, x20
0x250eeca6c:  e2 03 19 aa   mov	x2, x25
0x250eeca70:  d0 51 c4 95   bl	0x2580011b0
0x250eeca74:  df 4e c8 95   bl	_objc_release_x25_stub
0x250eeca78:  b5 06 00 91   add	x21, x21, #0x1
0x250eeca7c:  f7 06 00 f1   subs	x23, x23, #0x1
0x250eeca80:  a1 f9 ff 54   b.ne	loc_250eec9b4 ; ⤴ -0xcc
0x250eeca84:  e2 83 01 91   add	x2, sp, #0x60
0x250eeca88:  e3 83 02 91   add	x3, sp, #0xa0
0x250eeca8c:  e0 07 40 f9   ldr	x0, [sp, #0x8]
0x250eeca90:  04 02 80 52   mov	w4, #0x10
0x250eeca94:  df 51 c4 95   bl	0x258001210
0x250eeca98:  f7 03 00 aa   mov	x23, x0
0x250eeca9c:  a0 f8 ff b5   cbnz	x0, loc_250eec9b0 ; ⤴ -0xec
0x250eecaa0:  ; loc_250eecaa0
0x250eecaa0:  e8 07 40 f9   ldr	x8, [sp, #0x8]
0x250eecaa4:  e3 4e c8 95   bl	_objc_release_x8_stub
0x250eecaa8:  f3 03 40 f9   ldr	x19, [sp]
0x250eecaac:  02 00 00 14   b	loc_250eecab4 ; ⤵ 0x8
0x250eecab0:  ; loc_250eecab0
0x250eecab0:  14 00 80 d2   mov	x20, #0
0x250eecab4:  ; loc_250eecab4
0x250eecab4:  b7 4e c8 95   bl	_objc_release_x19_stub
0x250eecab8:  a8 03 5a f8   ldur	x8, [fp, #-0x60]
0x250eecabc:  c9 95 0b 90   adrp	x9, 0x2681a4000
0x250eecac0:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.___stack_chk_guard
0x250eecac4:  29 01 40 f9   ldr	x9, [x9]
0x250eecac8:  3f 01 08 eb   cmp	x9, x8
0x250eecacc:  c1 01 00 54   b.ne	loc_250eecb04 ; ⤵ 0x38
0x250eecad0:  e0 03 14 aa   mov	x0, x20
0x250eecad4:  fd 7b 58 a9   ldp	fp, lr, [sp, #0x180]
0x250eecad8:  f4 4f 57 a9   ldp	x20, x19, [sp, #0x170]
0x250eecadc:  f6 57 56 a9   ldp	x22, x21, [sp, #0x160]
0x250eecae0:  f8 5f 55 a9   ldp	x24, x23, [sp, #0x150]
0x250eecae4:  fa 67 54 a9   ldp	x26, x25, [sp, #0x140]
0x250eecae8:  fc 6f 53 a9   ldp	x28, x27, [sp, #0x130]
0x250eecaec:  ff 43 06 91   add	sp, sp, #0x190
0x250eecaf0:  ff 23 03 d5   autibsp
0x250eecaf4:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250eecaf8:  50 00 f0 b6   tbz	x16, #0x3e, loc_250eecb00 ; ⤵ 0x8
0x250eecafc:  20 8e 38 d4   brk	#0xc471
0x250eecb00:  ; loc_250eecb00
0x250eecb00:  64 50 c8 15   b	_objc_autoreleaseReturnValue_stub
0x250eecb04:  ; loc_250eecb04
0x250eecb04:  6b 4e c8 95   bl	___stack_chk_fail_stub

