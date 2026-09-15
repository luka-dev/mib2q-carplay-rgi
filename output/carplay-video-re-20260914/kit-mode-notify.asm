
___42-[APRKStreamRenderer _updateStreamingMode]_block_invoke_2
0x250efe790:  7f 23 03 d5   pacibsp
0x250efe794:  ff 03 01 d1   sub	sp, sp, #0x40
0x250efe798:  f6 57 01 a9   stp	x22, x21, [sp, #0x10]
0x250efe79c:  f4 4f 02 a9   stp	x20, x19, [sp, #0x20]
0x250efe7a0:  fd 7b 03 a9   stp	fp, lr, [sp, #0x30]
0x250efe7a4:  fd c3 00 91   add	fp, sp, #0x30
0x250efe7a8:  f3 03 00 aa   mov	x19, x0
0x250efe7ac:  f4 03 00 aa   mov	x20, x0
0x250efe7b0:  80 0e 42 f8   ldr	x0, [x20, #0x20]!
0x250efe7b4:  7f 0c c4 95   bl	0x2580019b0
0x250efe7b8:  3a 09 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250efe7bc:  f5 03 00 aa   mov	x21, x0
0x250efe7c0:  a8 7e d3 f0   adrp	x8, 0x1f7ed5000
0x250efe7c4:  01 bd 31 91   add	x1, x8, #0xc6f ; didChangeStreamRendererModeTo:forRenderer:
0x250efe7c8:  b6 0b c8 95   bl	_objc_opt_respondsToSelector_stub
0x250efe7cc:  f6 03 00 aa   mov	x22, x0
0x250efe7d0:  78 07 c8 95   bl	_objc_release_x21_stub
0x250efe7d4:  b6 03 00 36   tbz	w22, #0, loc_250efe848 ; ⤵ 0x74
0x250efe7d8:  a8 c2 0e b0   adrp	x8, 0x26e753000
0x250efe7dc:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250efe7e0:  1f c9 00 71   cmp	w8, #0x32
0x250efe7e4:  2c 01 00 54   b.gt	loc_250efe808 ; ⤵ 0x24
0x250efe7e8:  1f 05 00 31   cmn	w8, #0x1
0x250efe7ec:  81 03 00 54   b.ne	loc_250efe85c ; ⤵ 0x70
0x250efe7f0:  a0 c2 0e b0   adrp	x0, 0x26e753000
0x250efe7f4:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250efe7f8:  41 06 80 52   mov	w1, #0x32
0x250efe7fc:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250efe800:  70 09 c8 95   bl	__LogCategory_Initialize_stub
0x250efe804:  c0 02 00 35   cbnz	w0, loc_250efe85c ; ⤵ 0x58
0x250efe808:  ; loc_250efe808
0x250efe808:  60 12 40 f9   ldr	x0, [x19, #0x20]
0x250efe80c:  69 0c c4 95   bl	0x2580019b0
0x250efe810:  24 09 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250efe814:  e0 07 00 f9   str	x0, [sp, #0x8]
0x250efe818:  63 0a 42 a9   ldp	x3, x2, [x19, #0x20]
0x250efe81c:  a9 8a c4 95   bl	0x2580212c0
0x250efe820:  e0 07 40 f9   ldr	x0, [sp, #0x8]
0x250efe824:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x250efe828:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x250efe82c:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x250efe830:  ff 03 01 91   add	sp, sp, #0x40
0x250efe834:  ff 23 03 d5   autibsp
0x250efe838:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250efe83c:  50 00 f0 b6   tbz	x16, #0x3e, loc_250efe844 ; ⤵ 0x8
0x250efe840:  20 8e 38 d4   brk	#0xc471
0x250efe844:  ; loc_250efe844
0x250efe844:  4f 07 c8 15   b	_objc_release_x0_stub
0x250efe848:  ; loc_250efe848
0x250efe848:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x250efe84c:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x250efe850:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x250efe854:  ff 03 01 91   add	sp, sp, #0x40
0x250efe858:  ff 0f 5f d6   retab
0x250efe85c:  ; loc_250efe85c
0x250efe85c:  e0 03 14 aa   mov	x0, x20
0x250efe860:  e1 03 13 aa   mov	x1, x19
0x250efe864:  e1 3d 00 94   bl	___42-[APRKStreamRenderer _updateStreamingMode]_block_invoke_2.cold.1
0x250efe868:  e8 ff ff 17   b	loc_250efe808 ; ⤴ -0x60

