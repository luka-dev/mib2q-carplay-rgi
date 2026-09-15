0x250f07df0:  c8 8d 0b f0   adrp	x8, 0x2680c2000
0x250f07df4:  00 a9 47 f9   ldr	x0, [x8, #0xf50] ; _ptr.NSMutableDictionary
0x250f07df8:  1a e4 c7 95   bl	_objc_alloc_init_stub
0x250f07dfc:  f4 03 00 aa   mov	x20, x0
0x250f07e00:  48 8e 0b 90   adrp	x8, 0x2680cf000
0x250f07e04:  00 9d 43 f9   ldr	x0, [x8, #0x738] ; _ptr.NSPropertyListSerialization
0x250f07e08:  ff 0f 00 f9   str	xzr, [sp, #0x18]
0x250f07e0c:  e5 63 00 91   add	x5, sp, #0x18
0x250f07e10:  e2 03 16 aa   mov	x2, x22
0x250f07e14:  03 19 80 52   mov	w3, #0xc8
0x250f07e18:  04 00 80 d2   mov	x4, #0
0x250f07e1c:  e5 e7 c3 95   bl	0x258001db0
0x250f07e20:  a0 e3 c7 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250f07e24:  f6 03 00 aa   mov	x22, x0
0x250f07e28:  e8 0f 40 f9   ldr	x8, [sp, #0x18]
0x250f07e2c:  21 e2 c7 95   bl	_objc_retain_x8_stub
0x250f07e30:  f7 03 00 aa   mov	x23, x0
0x250f07e34:  f6 00 00 b4   cbz	x22, loc_250f07e50 ; ⤵ 0x1c
0x250f07e38:  68 8f 0b b0   adrp	x8, 0x2680f4000
0x250f07e3c:  08 65 40 f9   ldr	x8, [x8, #0xc8] ; _ptr._kAPReceiverUIControllerParam_Data
0x250f07e40:  03 01 40 f9   ldr	x3, [x8]
0x250f07e44:  e0 03 14 aa   mov	x0, x20
0x250f07e48:  e2 03 16 aa   mov	x2, x22
0x250f07e4c:  f9 e6 c3 95   bl	0x258001a30
0x250f07e50:  ; loc_250f07e50
0x250f07e50:  97 09 00 b4   cbz	x23, loc_250f07f80 ; ⤵ 0x130
0x250f07e54:  68 c2 0e 90   adrp	x8, 0x26e753000
0x250f07e58:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250f07e5c:  1f 69 01 71   cmp	w8, #0x5a
0x250f07e60:  8c 0a 00 54   b.gt	loc_250f07fb0 ; ⤵ 0x150
0x250f07e64:  1f 05 00 31   cmn	w8, #0x1
0x250f07e68:  01 01 00 54   b.ne	loc_250f07e88 ; ⤵ 0x20
0x250f07e6c:  c8 03 80 52   mov	w8, #0x1e
0x250f07e70:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250f07e74:  60 c2 0e 90   adrp	x0, 0x26e753000
0x250f07e78:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250f07e7c:  01 f1 00 11   add	w1, w8, #0x3c
0x250f07e80:  d0 e3 c7 95   bl	__LogCategory_Initialize_stub
0x250f07e84:  60 09 00 34   cbz	w0, loc_250f07fb0 ; ⤵ 0x12c
0x250f07e88:  ; loc_250f07e88
0x250f07e88:  17 1e 00 94   bl	_UIControllerSendUpstreamMessage.cold.1
0x250f07e8c:  49 00 00 14   b	loc_250f07fb0 ; ⤵ 0x124
0x250f07e90:  68 c2 0e 90   adrp	x8, 0x26e753000
0x250f07e94:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250f07e98:  1f 79 00 71   cmp	w8, #0x1e
0x250f07e9c:  0c 0a 00 54   b.gt	loc_250f07fdc ; ⤵ 0x140
0x250f07ea0:  1f 05 00 31   cmn	w8, #0x1

