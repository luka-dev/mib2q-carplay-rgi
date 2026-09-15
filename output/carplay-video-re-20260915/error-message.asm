
-[APRKMediaPlayer _sendUpstreamErrorMessageWithError:]
0x250ef275c:  7f 23 03 d5   pacibsp
0x250ef2760:  ff 03 01 d1   sub	sp, sp, #0x40
0x250ef2764:  f6 57 01 a9   stp	x22, x21, [sp, #0x10]
0x250ef2768:  f4 4f 02 a9   stp	x20, x19, [sp, #0x20]
0x250ef276c:  fd 7b 03 a9   stp	fp, lr, [sp, #0x30]
0x250ef2770:  fd c3 00 91   add	fp, sp, #0x30
0x250ef2774:  f3 03 00 aa   mov	x19, x0
0x250ef2778:  be 37 c8 95   bl	_objc_retain_x2_stub
0x250ef277c:  e2 03 00 aa   mov	x2, x0
0x250ef2780:  40 03 00 b4   cbz	x0, loc_250ef27e8 ; ⤵ 0x68
0x250ef2784:  d6 24 0c f0   adrp	x22, 0x26938d000
0x250ef2788:  c0 62 45 f9   ldr	x0, [x22, #0xac0] ; "\xf0fO\xf1"
0x250ef278c:  b4 5f 12 d0   adrp	x20, 0x275ae8000
0x250ef2790:  94 22 34 91   add	x20, x20, #0xd08 ; _ptr.__NSCFConstantString
0x250ef2794:  e2 07 00 f9   str	x2, [sp, #0x8]
0x250ef2798:  e2 03 14 aa   mov	x2, x20
0x250ef279c:  75 ba c4 95   bl	0x258021170
0x250ef27a0:  40 39 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef27a4:  f5 03 00 aa   mov	x21, x0
0x250ef27a8:  c0 62 45 f9   ldr	x0, [x22, #0xac0]
0x250ef27ac:  e2 07 40 f9   ldr	x2, [sp, #0x8]
0x250ef27b0:  c0 ba c4 95   bl	0x2580212b0
0x250ef27b4:  3b 39 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef27b8:  f6 03 00 aa   mov	x22, x0
0x250ef27bc:  e0 03 15 aa   mov	x0, x21
0x250ef27c0:  e2 03 16 aa   mov	x2, x22
0x250ef27c4:  e3 03 14 aa   mov	x3, x20
0x250ef27c8:  9a 3c c4 95   bl	0x258001a30
0x250ef27cc:  7d 37 c8 95   bl	_objc_release_x22_stub
0x250ef27d0:  e0 03 13 aa   mov	x0, x19
0x250ef27d4:  e2 03 15 aa   mov	x2, x21
0x250ef27d8:  b6 b9 c4 95   bl	0x258020eb0
0x250ef27dc:  75 37 c8 95   bl	_objc_release_x21_stub
0x250ef27e0:  e2 07 40 f9   ldr	x2, [sp, #0x8]
0x250ef27e4:  0f 00 00 14   b	loc_250ef2820 ; ⤵ 0x3c
0x250ef27e8:  ; loc_250ef27e8
0x250ef27e8:  08 c3 0e b0   adrp	x8, 0x26e753000
0x250ef27ec:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250ef27f0:  1f c9 00 71   cmp	w8, #0x32
0x250ef27f4:  6c 01 00 54   b.gt	loc_250ef2820 ; ⤵ 0x2c
0x250ef27f8:  1f 05 00 31   cmn	w8, #0x1
0x250ef27fc:  e2 07 00 f9   str	x2, [sp, #0x8]
0x250ef2800:  21 02 00 54   b.ne	loc_250ef2844 ; ⤵ 0x44
0x250ef2804:  00 c3 0e b0   adrp	x0, 0x26e753000
0x250ef2808:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250ef280c:  41 06 80 52   mov	w1, #0x32
0x250ef2810:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef2814:  6b 39 c8 95   bl	__LogCategory_Initialize_stub
0x250ef2818:  e2 07 40 f9   ldr	x2, [sp, #0x8]
0x250ef281c:  40 01 00 35   cbnz	w0, loc_250ef2844 ; ⤵ 0x28
0x250ef2820:  ; loc_250ef2820
0x250ef2820:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x250ef2824:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x250ef2828:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x250ef282c:  ff 03 01 91   add	sp, sp, #0x40
0x250ef2830:  ff 23 03 d5   autibsp
0x250ef2834:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250ef2838:  50 00 f0 b6   tbz	x16, #0x3e, loc_250ef2840 ; ⤵ 0x8
0x250ef283c:  20 8e 38 d4   brk	#0xc471
0x250ef2840:  ; loc_250ef2840
0x250ef2840:  18 51 c8 15   b	_objc_release_x2_stub
0x250ef2844:  ; loc_250ef2844
0x250ef2844:  7b 65 00 94   bl	-[APRKMediaPlayer _sendUpstreamErrorMessageWithError:].cold.1

