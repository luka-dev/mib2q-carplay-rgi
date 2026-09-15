
-[APRKMediaPlayer _sendUpstreamPlaybackStateMessageWithPlaybackStateString:stoppedBecauseInterrupted:]
0x250ef284c:  7f 23 03 d5   pacibsp
0x250ef2850:  ff 83 01 d1   sub	sp, sp, #0x60
0x250ef2854:  f8 5f 02 a9   stp	x24, x23, [sp, #0x20]
0x250ef2858:  f6 57 03 a9   stp	x22, x21, [sp, #0x30]
0x250ef285c:  f4 4f 04 a9   stp	x20, x19, [sp, #0x40]
0x250ef2860:  fd 7b 05 a9   stp	fp, lr, [sp, #0x50]
0x250ef2864:  fd 43 01 91   add	fp, sp, #0x50
0x250ef2868:  88 95 0b d0   adrp	x8, 0x2681a4000
0x250ef286c:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.___stack_chk_guard
0x250ef2870:  08 01 40 f9   ldr	x8, [x8]
0x250ef2874:  e8 0f 00 f9   str	x8, [sp, #0x18]
0x250ef2878:  42 0a 00 b4   cbz	x2, loc_250ef29c0 ; ⤵ 0x148
0x250ef287c:  f5 03 03 aa   mov	x21, x3
0x250ef2880:  f4 03 00 aa   mov	x20, x0
0x250ef2884:  c8 24 0c f0   adrp	x8, 0x26938d000
0x250ef2888:  13 61 45 f9   ldr	x19, [x8, #0xac0] ; "\xf0fO\xf1"
0x250ef288c:  79 37 c8 95   bl	_objc_retain_x2_stub
0x250ef2890:  f6 03 00 aa   mov	x22, x0
0x250ef2894:  a2 5f 12 d0   adrp	x2, 0x275ae8000
0x250ef2898:  42 a0 32 91   add	x2, x2, #0xca8 ; _ptr.__NSCFConstantString
0x250ef289c:  e0 03 13 aa   mov	x0, x19
0x250ef28a0:  34 ba c4 95   bl	0x258021170
0x250ef28a4:  ff 38 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef28a8:  f3 03 00 aa   mov	x19, x0
0x250ef28ac:  a3 5f 12 f0   adrp	x3, 0x275ae9000
0x250ef28b0:  63 a0 00 91   add	x3, x3, #0x28 ; _ptr.__NSCFConstantString
0x250ef28b4:  e2 03 16 aa   mov	x2, x22
0x250ef28b8:  5e 3c c4 95   bl	0x258001a30
0x250ef28bc:  41 37 c8 95   bl	_objc_release_x22_stub
0x250ef28c0:  88 4a 40 f9   ldr	x8, [x20, #0x90]
0x250ef28c4:  1f 0d 00 f1   cmp	x8, #0x3
0x250ef28c8:  00 01 00 54   b.eq	loc_250ef28e8 ; ⤵ 0x20
0x250ef28cc:  1f 09 00 f1   cmp	x8, #0x2
0x250ef28d0:  01 07 00 54   b.ne	loc_250ef29b0 ; ⤵ 0xe0
0x250ef28d4:  88 0e 40 f9   ldr	x8, [x20, #0x18]
0x250ef28d8:  28 02 00 b4   cbz	x8, loc_250ef291c ; ⤵ 0x44
0x250ef28dc:  75 37 c8 95   bl	_objc_retain_x8_stub
0x250ef28e0:  f5 03 00 aa   mov	x21, x0
0x250ef28e4:  17 00 00 14   b	loc_250ef2940 ; ⤵ 0x5c
0x250ef28e8:  ; loc_250ef28e8
0x250ef28e8:  88 8e 0b f0   adrp	x8, 0x2680c5000
0x250ef28ec:  08 31 40 f9   ldr	x8, [x8, #0x60] ; _ptr._kFigEndpointPlaybackSessionReason_Ended
0x250ef28f0:  a9 5f 12 f0   adrp	x9, 0x275ae9000
0x250ef28f4:  29 21 09 91   add	x9, x9, #0x248 ; _ptr.__NSCFConstantString
0x250ef28f8:  08 01 40 f9   ldr	x8, [x8]
0x250ef28fc:  bf 02 00 71   cmp	w21, #0
0x250ef2900:  22 11 88 9a   csel	x2, x9, x8, ne
0x250ef2904:  88 8e 0b d0   adrp	x8, 0x2680c4000
0x250ef2908:  08 71 46 f9   ldr	x8, [x8, #0xce0] ; _ptr._kFigEndpointPlaybackSessionEventKey_Reason
0x250ef290c:  03 01 40 f9   ldr	x3, [x8]
0x250ef2910:  e0 03 13 aa   mov	x0, x19
0x250ef2914:  47 3c c4 95   bl	0x258001a30
0x250ef2918:  26 00 00 14   b	loc_250ef29b0 ; ⤵ 0x98
0x250ef291c:  ; loc_250ef291c
0x250ef291c:  80 06 40 f9   ldr	x0, [x20, #0x8]
0x250ef2920:  a8 ac c4 95   bl	0x25801dbc0
0x250ef2924:  df 38 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef2928:  f6 03 00 aa   mov	x22, x0
0x250ef292c:  09 3c c4 95   bl	0x258001950
0x250ef2930:  dc 38 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef2934:  f5 03 00 aa   mov	x21, x0
0x250ef2938:  22 37 c8 95   bl	_objc_release_x22_stub
0x250ef293c:  b5 05 00 b4   cbz	x21, loc_250ef29f0 ; ⤵ 0xb4
0x250ef2940:  ; loc_250ef2940
0x250ef2940:  a8 5f 12 d0   adrp	x8, 0x275ae8000
0x250ef2944:  08 a1 31 91   add	x8, x8, #0xc68 ; _ptr.__NSCFConstantString
0x250ef2948:  e8 d7 00 a9   stp	x8, x21, [sp, #0x8]
0x250ef294c:  88 8e 0b 90   adrp	x8, 0x2680c2000
0x250ef2950:  00 6d 47 f9   ldr	x0, [x8, #0xed8] ; _ptr.NSDictionary
0x250ef2954:  e2 43 00 91   add	x2, sp, #0x10
0x250ef2958:  e3 23 00 91   add	x3, sp, #0x8
0x250ef295c:  24 00 80 52   mov	w4, #0x1
0x250ef2960:  30 3d c4 95   bl	0x258001e20
0x250ef2964:  cf 38 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef2968:  f6 03 00 aa   mov	x22, x0
0x250ef296c:  a3 5f 12 d0   adrp	x3, 0x275ae8000
0x250ef2970:  63 20 32 91   add	x3, x3, #0xc88 ; _ptr.__NSCFConstantString
0x250ef2974:  e0 03 13 aa   mov	x0, x19
0x250ef2978:  e2 03 16 aa   mov	x2, x22
0x250ef297c:  2d 3c c4 95   bl	0x258001a30
0x250ef2980:  e0 03 14 aa   mov	x0, x20
0x250ef2984:  8b b8 c4 95   bl	0x258020bb0
0x250ef2988:  c6 38 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef298c:  f7 03 00 aa   mov	x23, x0
0x250ef2990:  a3 5f 12 f0   adrp	x3, 0x275ae9000
0x250ef2994:  63 a0 08 91   add	x3, x3, #0x228 ; _ptr.__NSCFConstantString
0x250ef2998:  e0 03 13 aa   mov	x0, x19
0x250ef299c:  e2 03 17 aa   mov	x2, x23
0x250ef29a0:  24 3c c4 95   bl	0x258001a30
0x250ef29a4:  0b 37 c8 95   bl	_objc_release_x23_stub
0x250ef29a8:  06 37 c8 95   bl	_objc_release_x22_stub
0x250ef29ac:  01 37 c8 95   bl	_objc_release_x21_stub
0x250ef29b0:  ; loc_250ef29b0
0x250ef29b0:  e0 03 14 aa   mov	x0, x20
0x250ef29b4:  e2 03 13 aa   mov	x2, x19
0x250ef29b8:  3e b9 c4 95   bl	0x258020eb0
0x250ef29bc:  ; loc_250ef29bc
0x250ef29bc:  f5 36 c8 95   bl	_objc_release_x19_stub
0x250ef29c0:  ; loc_250ef29c0
0x250ef29c0:  e8 0f 40 f9   ldr	x8, [sp, #0x18]
0x250ef29c4:  89 95 0b d0   adrp	x9, 0x2681a4000
0x250ef29c8:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.___stack_chk_guard
0x250ef29cc:  29 01 40 f9   ldr	x9, [x9]
0x250ef29d0:  3f 01 08 eb   cmp	x9, x8
0x250ef29d4:  a1 02 00 54   b.ne	loc_250ef2a28 ; ⤵ 0x54
0x250ef29d8:  fd 7b 45 a9   ldp	fp, lr, [sp, #0x50]
0x250ef29dc:  f4 4f 44 a9   ldp	x20, x19, [sp, #0x40]
0x250ef29e0:  f6 57 43 a9   ldp	x22, x21, [sp, #0x30]
0x250ef29e4:  f8 5f 42 a9   ldp	x24, x23, [sp, #0x20]
0x250ef29e8:  ff 83 01 91   add	sp, sp, #0x60
0x250ef29ec:  ff 0f 5f d6   retab
0x250ef29f0:  ; loc_250ef29f0
0x250ef29f0:  08 c3 0e b0   adrp	x8, 0x26e753000
0x250ef29f4:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250ef29f8:  1f 79 00 71   cmp	w8, #0x1e
0x250ef29fc:  0c fe ff 54   b.gt	loc_250ef29bc ; ⤴ -0x40
0x250ef2a00:  1f 05 00 31   cmn	w8, #0x1
0x250ef2a04:  e1 00 00 54   b.ne	loc_250ef2a20 ; ⤵ 0x1c
0x250ef2a08:  00 c3 0e b0   adrp	x0, 0x26e753000
0x250ef2a0c:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250ef2a10:  c1 03 80 52   mov	w1, #0x1e
0x250ef2a14:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef2a18:  ea 38 c8 95   bl	__LogCategory_Initialize_stub
0x250ef2a1c:  00 fd ff 34   cbz	w0, loc_250ef29bc ; ⤴ -0x60
0x250ef2a20:  ; loc_250ef2a20
0x250ef2a20:  0c 65 00 94   bl	-[APRKMediaPlayer _sendUpstreamPlaybackStateMessageWithPlaybackStateString:stoppedBecauseInterrupted:].cold.1
0x250ef2a24:  e6 ff ff 17   b	loc_250ef29bc ; ⤴ -0x68
0x250ef2a28:  ; loc_250ef2a28
0x250ef2a28:  a2 36 c8 95   bl	___stack_chk_fail_stub

