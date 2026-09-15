0x250e75c80:  e0 03 1b aa   mov	x0, x27
0x250e75c84:  e1 03 17 aa   mov	x1, x23
0x250e75c88:  50 26 00 94   bl	__AddResponseStream
0x250e75c8c:  e0 f3 00 b9   str	w0, [sp, #0xf0]
0x250e75c90:  80 3a 00 35   cbnz	w0, loc_250e763e0 ; ⤵ 0x750
0x250e75c94:  e0 03 13 aa   mov	x0, x19
0x250e75c98:  72 57 ca 95   bl	_CMBaseObjectGetVTable_stub
0x250e75c9c:  10 0c 41 f8   ldr	x16, [x0, #0x10]!
0x250e75ca0:  f1 03 00 aa   mov	x17, x0
0x250e75ca4:  11 49 f1 f2   movk	x17, #0x8a48, lsl #0x30
0x250e75ca8:  30 1a c1 da   autda	x16, x17
0x250e75cac:  09 8e 40 f8   ldr	x9, [x16, #0x8]!
0x250e75cb0:  09 03 00 b4   cbz	x9, loc_250e75d10 ; ⤵ 0x60
0x250e75cb4:  e8 03 10 aa   mov	x8, x16
0x250e75cb8:  28 4a e9 f2   movk	x8, #0x4a51, lsl #0x30
0x250e75cbc:  f0 03 09 aa   mov	x16, x9
0x250e75cc0:  f1 03 08 aa   mov	x17, x8
0x250e75cc4:  30 12 c1 da   autia	x16, x17
0x250e75cc8:  f1 03 10 aa   mov	x17, x16
0x250e75ccc:  f1 43 c1 da   xpaci	x17
0x250e75cd0:  1f 02 11 eb   cmp	x16, x17
0x250e75cd4:  40 00 00 54   b.eq	loc_250e75cdc ; ⤵ 0x8
0x250e75cd8:  00 8e 38 d4   brk	#0xc470
0x250e75cdc:  ; loc_250e75cdc
0x250e75cdc:  f0 23 c1 da   paciza	x16
0x250e75ce0:  90 01 00 b4   cbz	x16, loc_250e75d10 ; ⤵ 0x30
0x250e75ce4:  e0 03 13 aa   mov	x0, x19
0x250e75ce8:  28 09 3f d7   blraa	x9, x8
0x250e75cec:  e0 f3 00 b9   str	w0, [sp, #0xf0]
0x250e75cf0:  40 01 00 35   cbnz	w0, loc_250e75d18 ; ⤵ 0x28
0x250e75cf4:  13 00 00 14   b	loc_250e75d40 ; ⤵ 0x4c
0x250e75cf8:  a0 3d 86 12   mov	w0, #0xffffce12
0x250e75cfc:  e0 f3 00 b9   str	w0, [sp, #0xf0]
0x250e75d00:  61 03 00 f0   adrp	x1, 0x250ee4000
0x250e75d04:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e75d08:  e2 08 83 52   mov	w2, #0x1847
0x250e75d0c:  06 00 00 14   b	loc_250e75d24 ; ⤵ 0x18
0x250e75d10:  ; loc_250e75d10
0x250e75d10:  a0 3d 86 12   mov	w0, #0xffffce12
0x250e75d14:  e0 f3 00 b9   str	w0, [sp, #0xf0]
0x250e75d18:  ; loc_250e75d18
0x250e75d18:  61 03 00 f0   adrp	x1, 0x250ee4000
0x250e75d1c:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e75d20:  e2 09 83 52   mov	w2, #0x184f
0x250e75d24:  ; loc_250e75d24
0x250e75d24:  03 00 80 52   mov	w3, #0
0x250e75d28:  8e 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75d2c:  ; loc_250e75d2c
0x250e75d2c:  e8 f3 40 b9   ldr	w8, [sp, #0xf0]
0x250e75d30:  88 00 00 34   cbz	w8, loc_250e75d40 ; ⤵ 0x10
0x250e75d34:  ; loc_250e75d34
0x250e75d34:  e0 03 14 aa   mov	x0, x20
0x250e75d38:  e1 03 1c aa   mov	x1, x28
0x250e75d3c:  8a 15 00 94   bl	__RemoteControlTearDownSession
0x250e75d40:  ; loc_250e75d40
0x250e75d40:  e0 9b 40 f9   ldr	x0, [sp, #0x130]
0x250e75d44:  40 00 00 b4   cbz	x0, loc_250e75d4c ; ⤵ 0x8
0x250e75d48:  8a 2f ca 95   bl	_CFRelease_stub
0x250e75d4c:  ; loc_250e75d4c
0x250e75d4c:  7c 00 00 b4   cbz	x28, loc_250e75d58 ; ⤵ 0xc
0x250e75d50:  e0 03 1c aa   mov	x0, x28
0x250e75d54:  87 2f ca 95   bl	_CFRelease_stub
0x250e75d58:  ; loc_250e75d58
0x250e75d58:  77 00 00 b4   cbz	x23, loc_250e75d64 ; ⤵ 0xc
0x250e75d5c:  e0 03 17 aa   mov	x0, x23
0x250e75d60:  84 2f ca 95   bl	_CFRelease_stub
0x250e75d64:  ; loc_250e75d64
0x250e75d64:  fc 63 40 f9   ldr	x28, [sp, #0xc0]
0x250e75d68:  76 00 00 b4   cbz	x22, loc_250e75d74 ; ⤵ 0xc
0x250e75d6c:  e0 03 16 aa   mov	x0, x22
0x250e75d70:  80 2f ca 95   bl	_CFRelease_stub
0x250e75d74:  ; loc_250e75d74
0x250e75d74:  75 00 00 b4   cbz	x21, loc_250e75d80 ; ⤵ 0xc
0x250e75d78:  e0 03 15 aa   mov	x0, x21
0x250e75d7c:  7d 2f ca 95   bl	_CFRelease_stub
0x250e75d80:  ; loc_250e75d80
0x250e75d80:  e0 f3 40 b9   ldr	w0, [sp, #0xf0]
0x250e75d84:  e0 ef 00 b9   str	w0, [sp, #0xec]
0x250e75d88:  40 2d 00 34   cbz	w0, loc_250e76330 ; ⤵ 0x5a8
0x250e75d8c:  05 03 00 14   b	loc_250e769a0 ; ⤵ 0xc14
0x250e75d90:  00 00 80 52   mov	w0, #0
0x250e75d94:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75d98:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75d9c:  22 3a 82 52   mov	w2, #0x11d1
0x250e75da0:  03 00 80 52   mov	w3, #0
0x250e75da4:  6f 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75da8:  d5 47 83 12   mov	w21, #0xffffe5c1
0x250e75dac:  5a 01 00 14   b	loc_250e76314 ; ⤵ 0x568
0x250e75db0:  61 03 00 d0   adrp	x1, 0x250ee3000
0x250e75db4:  21 dc 34 91   add	x1, x1, #0xd37 ; "_PlaybackSetup"
0x250e75db8:  c2 d6 82 52   mov	w2, #0x16b6
0x250e75dbc:  03 00 80 52   mov	w3, #0
0x250e75dc0:  68 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75dc4:  16 00 80 d2   mov	x22, #0
0x250e75dc8:  1a 00 00 14   b	loc_250e75e30 ; ⤵ 0x68
0x250e75dcc:  f5 03 00 aa   mov	x21, x0
0x250e75dd0:  28 d3 43 b9   ldr	w8, [x25, #0x3d0]
0x250e75dd4:  1f 69 01 71   cmp	w8, #0x5a
0x250e75dd8:  ec 29 00 54   b.gt	loc_250e76314 ; ⤵ 0x53c
0x250e75ddc:  1f 05 00 31   cmn	w8, #0x1
0x250e75de0:  00 10 00 54   b.eq	loc_250e75fe0 ; ⤵ 0x200
0x250e75de4:  ; loc_250e75de4
0x250e75de4:  88 5a 41 f9   ldr	x8, [x20, #0x2b0]
0x250e75de8:  e8 d7 00 a9   stp	x8, x21, [sp, #0x8]
0x250e75dec:  49 06 80 52   mov	w9, #0x32
0x250e75df0:  09 40 a0 72   movk	w9, #0x200, lsl #0x10
0x250e75df4:  22 a1 00 11   add	w2, w9, #0x28
0x250e75df8:  f4 03 00 f9   str	x20, [sp]
0x250e75dfc:  20 c6 0e d0   adrp	x0, 0x26e73b000
0x250e75e00:  00 40 0f 91   add	x0, x0, #0x3d0 ; _gLogCategory_AirPlayReceiverCore
0x250e75e04:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75e08:  21 a4 2b 91   add	x1, x1, #0xae9 ; "OSStatus _ScreenSetup(AirPlayReceiverSessionRef, CFDictionaryRef, CFMutableDictionaryRef)"
0x250e75e0c:  63 03 00 b0   adrp	x3, 0x250ee2000
0x250e75e10:  63 0c 2d 91   add	x3, x3, #0xb43 ; "[%{ptr}] Screen setup failed due to async %@ clock err: %#m\n"
0x250e75e14:  3b 2c ca 95   bl	_LogPrintF_stub
0x250e75e18:  3f 01 00 14   b	loc_250e76314 ; ⤵ 0x4fc
0x250e75e1c:  61 03 00 d0   adrp	x1, 0x250ee3000
0x250e75e20:  21 dc 34 91   add	x1, x1, #0xd37 ; "_PlaybackSetup"
0x250e75e24:  22 d7 82 52   mov	w2, #0x16b9
0x250e75e28:  03 00 80 52   mov	w3, #0
0x250e75e2c:  4d 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75e30:  ; loc_250e75e30
0x250e75e30:  f7 48 83 12   mov	w23, #0xffffe5b8
0x250e75e34:  e6 f9 ff 17   b	loc_250e745cc ; ⤴ -0x1868
0x250e75e38:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75e3c:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75e40:  02 3b 82 52   mov	w2, #0x11d8
0x250e75e44:  03 00 80 52   mov	w3, #0
0x250e75e48:  46 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75e4c:  f5 48 83 12   mov	w21, #0xffffe5b8
0x250e75e50:  fc 63 40 f9   ldr	x28, [sp, #0xc0]
0x250e75e54:  2a 01 00 14   b	loc_250e762fc ; ⤵ 0x4a8
0x250e75e58:  f7 03 00 aa   mov	x23, x0
0x250e75e5c:  61 03 00 d0   adrp	x1, 0x250ee3000
0x250e75e60:  21 dc 34 91   add	x1, x1, #0xd37 ; "_PlaybackSetup"
0x250e75e64:  e2 d7 82 52   mov	w2, #0x16bf
0x250e75e68:  03 00 80 52   mov	w3, #0
0x250e75e6c:  3d 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75e70:  d7 f9 ff 17   b	loc_250e745cc ; ⤴ -0x18a4
0x250e75e74:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75e78:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75e7c:  02 3d 82 52   mov	w2, #0x11e8
0x250e75e80:  03 00 80 52   mov	w3, #0
0x250e75e84:  37 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75e88:  f5 48 83 12   mov	w21, #0xffffe5b8
0x250e75e8c:  fb 5f 40 f9   ldr	x27, [sp, #0xb8]
0x250e75e90:  24 00 00 14   b	loc_250e75f20 ; ⤵ 0x90
0x250e75e94:  f5 03 00 aa   mov	x21, x0
0x250e75e98:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75e9c:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75ea0:  22 43 82 52   mov	w2, #0x1219
0x250e75ea4:  0b 01 00 14   b	loc_250e762d0 ; ⤵ 0x42c
0x250e75ea8:  00 00 80 52   mov	w0, #0
0x250e75eac:  e1 01 00 f0   adrp	x1, 0x250eb4000
0x250e75eb0:  21 6c 10 91   add	x1, x1, #0x41b ; "APReceiverScreenSinkCreateSBufConsumer"
0x250e75eb4:  62 3a 80 52   mov	w2, #0x1d3
0x250e75eb8:  03 00 80 52   mov	w3, #0
0x250e75ebc:  29 59 ca 95   bl	_APSLogErrorAt_stub
0x250e75ec0:  b5 c7 9c 52   mov	w21, #0xe63d
0x250e75ec4:  d5 ff bf 72   movk	w21, #0xfffe, lsl #0x10
0x250e75ec8:  10 00 00 14   b	loc_250e75f08 ; ⤵ 0x40
0x250e75ecc:  e0 c6 0e b0   adrp	x0, 0x26e752000
0x250e75ed0:  00 80 15 91   add	x0, x0, #0x560 ; _APReceiverScreenSinkGetClassID.once
0x250e75ed4:  21 63 12 90   adrp	x1, 0x275ad9000
0x250e75ed8:  21 00 17 91   add	x1, x1, #0x5c0 ; ___block_literal_global.6248
0x250e75edc:  c9 2b ca 95   bl	j__dispatch_once
0x250e75ee0:  82 fa ff 17   b	loc_250e748e8 ; ⤴ -0x15f8
0x250e75ee4:  f5 03 00 aa   mov	x21, x0
0x250e75ee8:  e1 01 00 f0   adrp	x1, 0x250eb4000
0x250e75eec:  21 6c 10 91   add	x1, x1, #0x41b ; "APReceiverScreenSinkCreateSBufConsumer"
0x250e75ef0:  22 3b 80 52   mov	w2, #0x1d9
0x250e75ef4:  03 00 80 52   mov	w3, #0
0x250e75ef8:  1a 59 ca 95   bl	j__APSLogErrorAt
0x250e75efc:  a0 03 53 f8   ldur	x0, [fp, #-0xd0]
0x250e75f00:  40 00 00 b4   cbz	x0, loc_250e75f08 ; ⤵ 0x8
0x250e75f04:  1b 2f ca 95   bl	j__CFRelease
0x250e75f08:  ; loc_250e75f08
0x250e75f08:  e0 03 15 aa   mov	x0, x21
0x250e75f0c:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75f10:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75f14:  82 3b 82 52   mov	w2, #0x11dc
0x250e75f18:  03 00 80 52   mov	w3, #0
0x250e75f1c:  11 59 ca 95   bl	j__APSLogErrorAt
0x250e75f20:  ; loc_250e75f20
0x250e75f20:  e0 03 1c aa   mov	x0, x28
0x250e75f24:  13 2f ca 95   bl	j__CFRelease
0x250e75f28:  f7 7f 40 f9   ldr	x23, [sp, #0xf8]
0x250e75f2c:  f0 00 00 14   b	loc_250e762ec ; ⤵ 0x3c0
0x250e75f30:  e0 03 13 aa   mov	x0, x19
0x250e75f34:  61 03 00 d0   adrp	x1, 0x250ee3000
0x250e75f38:  21 6c 39 91   add	x1, x1, #0xe5b ; "_PlaybackV2Setup"
0x250e75f3c:  42 ef 82 52   mov	w2, #0x177a
0x250e75f40:  03 00 80 52   mov	w3, #0
0x250e75f44:  07 59 ca 95   bl	j__APSLogErrorAt
0x250e75f48:  b9 fc ff 17   b	loc_250e7522c ; ⤴ -0xd1c
0x250e75f4c:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75f50:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75f54:  82 44 82 52   mov	w2, #0x1224
0x250e75f58:  03 00 80 52   mov	w3, #0
0x250e75f5c:  01 59 ca 95   bl	j__APSLogErrorAt
0x250e75f60:  55 47 83 12   mov	w21, #0xffffe5c5
0x250e75f64:  dd 00 00 14   b	loc_250e762d8 ; ⤵ 0x374
0x250e75f68:  1c 00 80 d2   mov	x28, #0
0x250e75f6c:  17 00 80 d2   mov	x23, #0
0x250e75f70:  13 46 83 12   mov	w19, #0xffffe5cf
0x250e75f74:  42 fc 82 52   mov	w2, #0x17e2
0x250e75f78:  0c 00 00 14   b	loc_250e75fa8 ; ⤵ 0x30
0x250e75f7c:  17 00 80 d2   mov	x23, #0
0x250e75f80:  f3 48 83 12   mov	w19, #0xffffe5b8
0x250e75f84:  22 fd 82 52   mov	w2, #0x17e9
0x250e75f88:  08 00 00 14   b	loc_250e75fa8 ; ⤵ 0x20
0x250e75f8c:  17 00 80 d2   mov	x23, #0
0x250e75f90:  f3 48 83 12   mov	w19, #0xffffe5b8
0x250e75f94:  82 fd 82 52   mov	w2, #0x17ec
0x250e75f98:  04 00 00 14   b	loc_250e75fa8 ; ⤵ 0x10
0x250e75f9c:  13 46 83 12   mov	w19, #0xffffe5cf
0x250e75fa0:  82 fe 82 52   mov	w2, #0x17f4
0x250e75fa4:  f7 4b 40 f9   ldr	x23, [sp, #0x90]
0x250e75fa8:  ; loc_250e75fa8
0x250e75fa8:  00 00 80 52   mov	w0, #0
0x250e75fac:  61 03 00 f0   adrp	x1, 0x250ee4000
0x250e75fb0:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e75fb4:  03 00 80 52   mov	w3, #0
0x250e75fb8:  ea 58 ca 95   bl	j__APSLogErrorAt
0x250e75fbc:  15 00 80 d2   mov	x21, #0
0x250e75fc0:  16 00 80 d2   mov	x22, #0
0x250e75fc4:  f3 f3 00 b9   str	w19, [sp, #0xf0]
0x250e75fc8:  5b ff ff 17   b	loc_250e75d34 ; ⤴ -0x294
0x250e75fcc:  e0 03 15 aa   mov	x0, x21
0x250e75fd0:  61 03 00 b0   adrp	x1, 0x250ee2000
0x250e75fd4:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e75fd8:  82 46 82 52   mov	w2, #0x1234
0x250e75fdc:  bd 00 00 14   b	loc_250e762d0 ; ⤵ 0x2f4
0x250e75fe0:  ; loc_250e75fe0
0x250e75fe0:  48 06 80 52   mov	w8, #0x32
0x250e75fe4:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250e75fe8:  01 a1 00 11   add	w1, w8, #0x28
0x250e75fec:  20 c6 0e d0   adrp	x0, 0x26e73b000
0x250e75ff0:  00 40 0f 91   add	x0, x0, #0x3d0 ; "\xff\xff\xff\xff2"
0x250e75ff4:  73 2b ca 95   bl	j___LogCategory_Initialize
0x250e75ff8:  60 ef ff 35   cbnz	w0, loc_250e75de4 ; ⤴ -0x214
0x250e75ffc:  c6 00 00 14   b	loc_250e76314 ; ⤵ 0x318
0x250e76000:  f5 03 00 aa   mov	x21, x0
0x250e76004:  61 03 00 90   adrp	x1, 0x250ee2000
0x250e76008:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e7600c:  c2 47 82 52   mov	w2, #0x123e
0x250e76010:  b0 00 00 14   b	loc_250e762d0 ; ⤵ 0x2c0
0x250e76014:  ff 9b 00 f9   str	xzr, [sp, #0x130]
0x250e76018:  00 00 80 d2   mov	x0, #0
0x250e7601c:  01 00 80 d2   mov	x1, #0
0x250e76020:  62 92 0b b0   adrp	x2, 0x2680c3000
0x250e76024:  42 c8 41 f9   ldr	x2, [x2, #0x390] ; _ptr.1e8d91d98 (CoreFoundation/__AUTH_CONST.__const)
0x250e76028:  63 92 0b b0   adrp	x3, 0x2680c3000
0x250e7602c:  63 cc 41 f9   ldr	x3, [x3, #0x398] ; _ptr.1e8d91df8 (CoreFoundation/__AUTH_CONST.__const)
0x250e76030:  50 31 ca 95   bl	j__CFDictionaryCreateMutable
0x250e76034:  f6 03 00 aa   mov	x22, x0
0x250e76038:  e0 2c 00 b4   cbz	x0, loc_250e765d4 ; ⤵ 0x59c
0x250e7603c:  e0 9b 40 f9   ldr	x0, [sp, #0x130]
0x250e76040:  80 02 00 b4   cbz	x0, loc_250e76090 ; ⤵ 0x50
0x250e76044:  41 63 12 d0   adrp	x1, 0x275ae0000
0x250e76048:  21 00 38 91   add	x1, x1, #0xe00
0x250e7604c:  59 31 ca 95   bl	j__CFDictionaryGetValue
0x250e76050:  c0 00 00 b4   cbz	x0, loc_250e76068 ; ⤵ 0x18
0x250e76054:  e2 03 00 aa   mov	x2, x0
0x250e76058:  e0 03 16 aa   mov	x0, x22
0x250e7605c:  21 63 12 b0   adrp	x1, 0x275adb000
0x250e76060:  21 00 18 91   add	x1, x1, #0x600
0x250e76064:  57 31 ca 95   bl	j__CFDictionarySetValue
0x250e76068:  ; loc_250e76068
0x250e76068:  e0 9b 40 f9   ldr	x0, [sp, #0x130]
0x250e7606c:  41 63 12 d0   adrp	x1, 0x275ae0000
0x250e76070:  21 00 24 91   add	x1, x1, #0x900
0x250e76074:  4f 31 ca 95   bl	j__CFDictionaryGetValue
0x250e76078:  c0 00 00 b4   cbz	x0, loc_250e76090 ; ⤵ 0x18
0x250e7607c:  e2 03 00 aa   mov	x2, x0
0x250e76080:  e0 03 16 aa   mov	x0, x22
0x250e76084:  21 63 12 d0   adrp	x1, 0x275adc000
0x250e76088:  21 00 17 91   add	x1, x1, #0x5c0
0x250e7608c:  4d 31 ca 95   bl	j__CFDictionarySetValue
0x250e76090:  ; loc_250e76090
0x250e76090:  e0 03 16 aa   mov	x0, x22
0x250e76094:  21 63 12 d0   adrp	x1, 0x275adc000
0x250e76098:  21 00 16 91   add	x1, x1, #0x580
0x250e7609c:  00 1d a8 4e   mov	v0.16b, v8.16b
0x250e760a0:  21 1d a9 4e   mov	v1.16b, v9.16b
0x250e760a4:  7b 5e ca 95   bl	j__FigCFDictionarySetCGSize
0x250e760a8:  e8 6b 40 f9   ldr	x8, [sp, #0xd0]
0x250e760ac:  13 01 40 f9   ldr	x19, [x8]
0x250e760b0:  e0 03 13 aa   mov	x0, x19
0x250e760b4:  6b 56 ca 95   bl	j__CMBaseObjectGetVTable
0x250e760b8:  10 0c 41 f8   ldr	x16, [x0, #0x10]!
0x250e760bc:  f1 03 00 aa   mov	x17, x0
0x250e760c0:  71 62 f8 f2   movk	x17, #0xc313, lsl #0x30
0x250e760c4:  30 1a c1 da   autda	x16, x17
0x250e760c8:  09 8e 40 f8   ldr	x9, [x16, #0x8]!
0x250e760cc:  09 0a 00 b4   cbz	x9, loc_250e7620c ; ⤵ 0x140
0x250e760d0:  e8 03 10 aa   mov	x8, x16
0x250e760d4:  a8 07 ec f2   movk	x8, #0x603d, lsl #0x30
0x250e760d8:  f0 03 09 aa   mov	x16, x9
0x250e760dc:  f1 03 08 aa   mov	x17, x8
0x250e760e0:  30 12 c1 da   autia	x16, x17
0x250e760e4:  f1 03 10 aa   mov	x17, x16
0x250e760e8:  f1 43 c1 da   xpaci	x17
0x250e760ec:  1f 02 11 eb   cmp	x16, x17
0x250e760f0:  40 00 00 54   b.eq	loc_250e760f8 ; ⤵ 0x8
0x250e760f4:  00 8e 38 d4   brk	#0xc470
0x250e760f8:  ; loc_250e760f8
0x250e760f8:  f0 23 c1 da   paciza	x16
0x250e760fc:  90 08 00 b4   cbz	x16, loc_250e7620c ; ⤵ 0x110
0x250e76100:  e0 03 13 aa   mov	x0, x19
0x250e76104:  e1 03 16 aa   mov	x1, x22
0x250e76108:  28 09 3f d7   blraa	x9, x8
0x250e7610c:  a0 27 00 35   cbnz	w0, loc_250e76600 ; ⤵ 0x4f4
0x250e76110:  28 00 80 52   mov	w8, #0x1
0x250e76114:  88 02 0a 39   strb	w8, [x20, #0x280]
0x250e76118:  93 9e 41 f9   ldr	x19, [x20, #0x338]
0x250e7611c:  60 63 12 d0   adrp	x0, 0x275ae4000
0x250e76120:  00 80 19 91   add	x0, x0, #0x660
0x250e76124:  80 9e 01 f9   str	x0, [x20, #0x338]
0x250e76128:  96 2e ca 95   bl	j__CFRetain
0x250e7612c:  73 00 00 b4   cbz	x19, loc_250e76138 ; ⤵ 0xc
0x250e76130:  e0 03 13 aa   mov	x0, x19
0x250e76134:  8f 2e ca 95   bl	j__CFRelease
0x250e76138:  ; loc_250e76138
0x250e76138:  88 4a 41 f9   ldr	x8, [x20, #0x290]
0x250e7613c:  48 06 00 b4   cbz	x8, loc_250e76204 ; ⤵ 0xc8
0x250e76140:  88 62 4a 39   ldrb	w8, [x20, #0x298]
0x250e76144:  08 06 00 34   cbz	w8, loc_250e76204 ; ⤵ 0xc0
0x250e76148:  93 52 41 f9   ldr	x19, [x20, #0x2a0]
0x250e7614c:  53 04 00 b5   cbnz	x19, loc_250e761d4 ; ⤵ 0x88
0x250e76150:  83 0a 40 f9   ldr	x3, [x20, #0x10]
0x250e76154:  60 99 0b d0   adrp	x0, 0x2681a4000
0x250e76158:  00 e0 44 f9   ldr	x0, [x0, #0x9c0] ; _ptr.1ec141550 (libdispatch.dylib/__AUTH_CONST.__const)
0x250e7615c:  01 00 80 d2   mov	x1, #0
0x250e76160:  02 00 80 d2   mov	x2, #0
0x250e76164:  37 2b ca 95   bl	j__dispatch_source_create
0x250e76168:  80 52 01 f9   str	x0, [x20, #0x2a0]
0x250e7616c:  e8 03 04 91   add	x8, sp, #0x100
0x250e76170:  70 99 0b d0   adrp	x16, 0x2681a4000
0x250e76174:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250e76178:  f1 03 08 aa   mov	x17, x8
0x250e7617c:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250e76180:  30 0a c1 da   pacda	x16, x17
0x250e76184:  f0 83 00 f9   str	x16, [sp, #0x100]
0x250e76188:  eb 87 00 fd   str	d11, [sp, #0x108]
0x250e7618c:  e8 1f 40 f9   ldr	x8, [sp, #0x38]
0x250e76190:  50 00 00 b0   adrp	x16, 0x250e7f000
0x250e76194:  10 e2 22 91   add	x16, x16, #0x8b8
0x250e76198:  10 01 c1 da   pacia	x16, x8
0x250e7619c:  88 28 0c d0   adrp	x8, 0x269388000
0x250e761a0:  08 01 1f 91   add	x8, x8, #0x7c0 ; data_2693887c0 (AirPlayReceiver/__DATA_CONST.__const)
0x250e761a4:  f0 23 11 a9   stp	x16, x8, [sp, #0x110]
0x250e761a8:  f4 93 00 f9   str	x20, [sp, #0x120]
0x250e761ac:  e1 03 04 91   add	x1, sp, #0x100
0x250e761b0:  28 2b ca 95   bl	j__dispatch_source_set_event_handler
0x250e761b4:  80 52 41 f9   ldr	x0, [x20, #0x2a0]
0x250e761b8:  01 00 80 92   mov	x1, #0xffffffffffffffff
0x250e761bc:  02 00 80 d2   mov	x2, #0
0x250e761c0:  03 00 80 d2   mov	x3, #0
0x250e761c4:  9f 2e ca 95   bl	j___dispatch_source_set_runloop_timer_4CF
0x250e761c8:  80 52 41 f9   ldr	x0, [x20, #0x2a0]
0x250e761cc:  99 2e ca 95   bl	j__dispatch_resume
0x250e761d0:  93 52 41 f9   ldr	x19, [x20, #0x2a0]
0x250e761d4:  ; loc_250e761d4
0x250e761d4:  00 00 80 d2   mov	x0, #0
0x250e761d8:  15 80 9c d2   mov	x21, #0xe400
0x250e761dc:  75 81 aa f2   movk	x21, #0x540b, lsl #0x10
0x250e761e0:  55 00 c0 f2   movk	x21, #0x2, lsl #0x20
0x250e761e4:  e1 03 15 aa   mov	x1, x21
0x250e761e8:  be 31 ca 95   bl	j__dispatch_time
0x250e761ec:  e1 03 00 aa   mov	x1, x0
0x250e761f0:  e0 03 13 aa   mov	x0, x19
0x250e761f4:  e2 03 15 aa   mov	x2, x21
0x250e761f8:  03 40 99 52   mov	w3, #0xca00
0x250e761fc:  43 73 a7 72   movk	w3, #0x3b9a, lsl #0x10
0x250e76200:  90 2e ca 95   bl	j___dispatch_source_set_runloop_timer_4CF
0x250e76204:  ; loc_250e76204
0x250e76204:  15 00 80 52   mov	w21, #0
0x250e76208:  08 00 00 14   b	loc_250e76228 ; ⤵ 0x20
0x250e7620c:  ; loc_250e7620c
0x250e7620c:  b5 3d 86 12   mov	w21, #0xffffce12
0x250e76210:  ; loc_250e76210
0x250e76210:  e0 03 15 aa   mov	x0, x21
0x250e76214:  61 03 00 90   adrp	x1, 0x250ee2000
0x250e76218:  21 e8 33 91   add	x1, x1, #0xcfa ; "_ScreenStart"
0x250e7621c:  22 55 82 52   mov	w2, #0x12a9
0x250e76220:  03 00 80 52   mov	w3, #0
0x250e76224:  4f 58 ca 95   bl	j__APSLogErrorAt
0x250e76228:  ; loc_250e76228
0x250e76228:  e0 9b 40 f9   ldr	x0, [sp, #0x130]
0x250e7622c:  40 00 00 b4   cbz	x0, loc_250e76234 ; ⤵ 0x8
0x250e76230:  50 2e ca 95   bl	j__CFRelease
0x250e76234:  ; loc_250e76234
0x250e76234:  76 00 00 b4   cbz	x22, loc_250e76240 ; ⤵ 0xc
0x250e76238:  e0 03 16 aa   mov	x0, x22
0x250e7623c:  4d 2e ca 95   bl	j__CFRelease
0x250e76240:  ; loc_250e76240
0x250e76240:  b5 02 00 34   cbz	w21, loc_250e76294 ; ⤵ 0x54
0x250e76244:  28 d3 43 b9   ldr	w8, [x25, #0x3d0]
0x250e76248:  1f f1 00 71   cmp	w8, #0x3c
0x250e7624c:  6c 03 00 54   b.gt	loc_250e762b8 ; ⤵ 0x6c
0x250e76250:  1f 05 00 31   cmn	w8, #0x1
0x250e76254:  40 02 00 54   b.eq	loc_250e7629c ; ⤵ 0x48
0x250e76258:  ; loc_250e76258
0x250e76258:  88 3a 41 f9   ldr	x8, [x20, #0x270]
0x250e7625c:  e9 03 15 aa   mov	x9, x21
0x250e76260:  e8 a7 00 a9   stp	x8, x9, [sp, #0x8]
0x250e76264:  49 06 80 52   mov	w9, #0x32
0x250e76268:  09 40 a0 72   movk	w9, #0x200, lsl #0x10
0x250e7626c:  22 29 00 11   add	w2, w9, #0xa
0x250e76270:  f4 03 00 f9   str	x20, [sp]
0x250e76274:  20 c6 0e b0   adrp	x0, 0x26e73b000
0x250e76278:  00 40 0f 91   add	x0, x0, #0x3d0 ; "\xff\xff\xff\xff2"
0x250e7627c:  61 03 00 90   adrp	x1, 0x250ee2000
0x250e76280:  21 1c 34 91   add	x1, x1, #0xd07 ; "OSStatus _ScreenStart(AirPlayReceiverSessionRef)"
0x250e76284:  63 03 00 90   adrp	x3, 0x250ee2000
0x250e76288:  63 d4 35 91   add	x3, x3, #0xd75 ; "[%{ptr}] screen session [%{ptr}] screen start failed: %#m\n"
0x250e7628c:  1d 2b ca 95   bl	j__LogPrintF
0x250e76290:  0a 00 00 14   b	loc_250e762b8 ; ⤵ 0x28
0x250e76294:  ; loc_250e76294
0x250e76294:  15 00 80 52   mov	w21, #0
0x250e76298:  10 00 00 14   b	loc_250e762d8 ; ⤵ 0x40
0x250e7629c:  ; loc_250e7629c
0x250e7629c:  48 06 80 52   mov	w8, #0x32
0x250e762a0:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250e762a4:  01 29 00 11   add	w1, w8, #0xa
0x250e762a8:  20 c6 0e b0   adrp	x0, 0x26e73b000
0x250e762ac:  00 40 0f 91   add	x0, x0, #0x3d0 ; "\xff\xff\xff\xff2"
0x250e762b0:  c4 2a ca 95   bl	j___LogCategory_Initialize
0x250e762b4:  20 fd ff 35   cbnz	w0, loc_250e76258 ; ⤴ -0x5c
0x250e762b8:  ; loc_250e762b8
0x250e762b8:  e0 03 14 aa   mov	x0, x20
0x250e762bc:  8a 12 00 94   bl	0x250e7ace4
0x250e762c0:  e0 03 15 aa   mov	x0, x21
0x250e762c4:  61 03 00 90   adrp	x1, 0x250ee2000
0x250e762c8:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e762cc:  82 4a 82 52   mov	w2, #0x1254
0x250e762d0:  ; loc_250e762d0
0x250e762d0:  03 00 80 52   mov	w3, #0
0x250e762d4:  23 58 ca 95   bl	j__APSLogErrorAt
0x250e762d8:  ; loc_250e762d8
0x250e762d8:  e0 03 1c aa   mov	x0, x28
0x250e762dc:  25 2e ca 95   bl	j__CFRelease
0x250e762e0:  e0 03 1b aa   mov	x0, x27
0x250e762e4:  23 2e ca 95   bl	j__CFRelease
0x250e762e8:  fb 5f 40 f9   ldr	x27, [sp, #0xb8]
0x250e762ec:  ; loc_250e762ec
0x250e762ec:  fc 63 40 f9   ldr	x28, [sp, #0xc0]
0x250e762f0:  77 00 00 b4   cbz	x23, loc_250e762fc ; ⤵ 0xc
0x250e762f4:  e0 03 17 aa   mov	x0, x23
0x250e762f8:  1e 2e ca 95   bl	j__CFRelease
0x250e762fc:  ; loc_250e762fc
0x250e762fc:  e0 7b 40 f9   ldr	x0, [sp, #0xf0]
0x250e76300:  40 00 00 b4   cbz	x0, loc_250e76308 ; ⤵ 0x8
0x250e76304:  1b 2e ca 95   bl	j__CFRelease
0x250e76308:  ; loc_250e76308
0x250e76308:  75 00 00 34   cbz	w21, loc_250e76314 ; ⤵ 0xc
0x250e7630c:  e0 03 14 aa   mov	x0, x20
0x250e76310:  75 12 00 94   bl	0x250e7ace4
0x250e76314:  ; loc_250e76314
0x250e76314:  e8 63 05 91   add	x8, sp, #0x158
0x250e76318:  e9 03 80 92   mov	x9, #0xffffffffffffffe0
0x250e7631c:  ; loc_250e7631c
0x250e7631c:  1f 15 00 38   strb	wzr, [x8], #0x1
0x250e76320:  29 05 00 b1   adds	x9, x9, #0x1
0x250e76324:  c3 ff ff 54   b.lo	loc_250e7631c ; ⤴ -0x8
0x250e76328:  35 23 00 35   cbnz	w21, loc_250e7678c ; ⤵ 0x464
0x250e7632c:  ff ef 00 b9   str	wzr, [sp, #0xec]
0x250e76330:  ; loc_250e76330
0x250e76330:  5a 07 00 91   add	x26, x26, #0x1
0x250e76334:  e8 6f 40 f9   ldr	x8, [sp, #0xd8]
0x250e76338:  5f 03 08 eb   cmp	x26, x8
0x250e7633c:  e1 f0 fe 54   b.ne	loc_250e74158 ; ⤴ -0x21e4
0x250e76340:  b2 00 00 14   b	loc_250e76608 ; ⤵ 0x2c8
0x250e76344:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e76348:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e7634c:  c2 02 83 52   mov	w2, #0x1816
0x250e76350:  04 00 00 14   b	loc_250e76360 ; ⤵ 0x10
0x250e76354:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e76358:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e7635c:  22 03 83 52   mov	w2, #0x1819
0x250e76360:  ; loc_250e76360
0x250e76360:  03 00 80 52   mov	w3, #0
0x250e76364:  ff 57 ca 95   bl	j__APSLogErrorAt
0x250e76368:  16 00 80 d2   mov	x22, #0
0x250e7636c:  f7 4b 40 f9   ldr	x23, [sp, #0x90]
0x250e76370:  1a 00 00 14   b	loc_250e763d8 ; ⤵ 0x68
0x250e76374:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e76378:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e7637c:  82 03 83 52   mov	w2, #0x181c
0x250e76380:  03 00 80 52   mov	w3, #0
0x250e76384:  f7 57 ca 95   bl	j__APSLogErrorAt
0x250e76388:  16 00 80 d2   mov	x22, #0
0x250e7638c:  e8 48 83 12   mov	w8, #0xffffe5b8
0x250e76390:  e8 f3 00 b9   str	w8, [sp, #0xf0]
0x250e76394:  f7 4b 40 f9   ldr	x23, [sp, #0x90]
0x250e76398:  0e 00 00 14   b	loc_250e763d0 ; ⤵ 0x38
0x250e7639c:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e763a0:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e763a4:  42 07 83 52   mov	w2, #0x183a
0x250e763a8:  5f fe ff 17   b	loc_250e75d24 ; ⤴ -0x684
0x250e763ac:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e763b0:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e763b4:  e2 03 83 52   mov	w2, #0x181f
0x250e763b8:  03 00 80 52   mov	w3, #0
0x250e763bc:  e9 57 ca 95   bl	j__APSLogErrorAt
0x250e763c0:  e8 48 83 12   mov	w8, #0xffffe5b8
0x250e763c4:  e8 f3 00 b9   str	w8, [sp, #0xf0]
0x250e763c8:  f7 4b 40 f9   ldr	x23, [sp, #0x90]
0x250e763cc:  f6 03 18 aa   mov	x22, x24
0x250e763d0:  ; loc_250e763d0
0x250e763d0:  f8 03 1c aa   mov	x24, x28
0x250e763d4:  fc 03 15 aa   mov	x28, x21
0x250e763d8:  ; loc_250e763d8
0x250e763d8:  15 00 80 d2   mov	x21, #0
0x250e763dc:  54 fe ff 17   b	loc_250e75d2c ; ⤴ -0x6b0
0x250e763e0:  ; loc_250e763e0
0x250e763e0:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e763e4:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e763e8:  82 09 83 52   mov	w2, #0x184c
0x250e763ec:  4e fe ff 17   b	loc_250e75d24 ; ⤴ -0x6c8
0x250e763f0:  f5 03 00 aa   mov	x21, x0
0x250e763f4:  61 03 00 90   adrp	x1, 0x250ee2000
0x250e763f8:  21 70 2b 91   add	x1, x1, #0xadc ; "_ScreenSetup"
0x250e763fc:  22 45 82 52   mov	w2, #0x1229
0x250e76400:  b4 ff ff 17   b	loc_250e762d0 ; ⤴ -0x130
0x250e76404:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e76408:  00 00 80 52   mov	w0, #0
0x250e7640c:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e76410:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e76414:  c2 15 80 52   mov	w2, #0xae
0x250e76418:  03 00 80 52   mov	w3, #0
0x250e7641c:  d1 57 ca 95   bl	j__APSLogErrorAt
0x250e76420:  09 00 00 14   b	loc_250e76444 ; ⤵ 0x24
0x250e76424:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e76428:  00 00 80 52   mov	w0, #0
0x250e7642c:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e76430:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e76434:  02 16 80 52   mov	w2, #0xb0
0x250e76438:  03 00 80 52   mov	w3, #0
0x250e7643c:  c9 57 ca 95   bl	j__APSLogErrorAt
0x250e76440:  16 00 80 d2   mov	x22, #0
0x250e76444:  ; loc_250e76444
0x250e76444:  13 46 83 12   mov	w19, #0xffffe5cf
0x250e76448:  2b 00 00 14   b	loc_250e764f4 ; ⤵ 0xac
0x250e7644c:  70 fb ff d0   adrp	x16, 0x250de4000
0x250e76450:  10 a2 0f 91   add	x16, x16, #0x3e8
0x250e76454:  f0 23 c1 da   paciza	x16
0x250e76458:  e2 03 10 aa   mov	x2, x16
0x250e7645c:  e0 c6 0e 90   adrp	x0, 0x26e752000
0x250e76460:  00 e0 09 91   add	x0, x0, #0x278 ; data_26e752278 (AirPlayReceiver/__DATA.__bss)
0x250e76464:  e1 c6 0e 90   adrp	x1, 0x26e752000
0x250e76468:  21 00 0a 91   add	x1, x1, #0x280 ; data_26e752280 (AirPlayReceiver/__DATA.__bss)
0x250e7646c:  35 3e ca 95   bl	j__dispatch_once_f
0x250e76470:  f8 fc ff 17   b	loc_250e75850 ; ⤴ -0xc20
0x250e76474:  f3 03 00 aa   mov	x19, x0
0x250e76478:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e7647c:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e76480:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e76484:  62 16 80 52   mov	w2, #0xb3
0x250e76488:  03 00 80 52   mov	w3, #0
0x250e7648c:  b5 57 ca 95   bl	j__APSLogErrorAt
0x250e76490:  16 00 80 d2   mov	x22, #0
0x250e76494:  18 00 00 14   b	loc_250e764f4 ; ⤵ 0x60
0x250e76498:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e7649c:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e764a0:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e764a4:  62 17 80 52   mov	w2, #0xbb
0x250e764a8:  0f 00 00 14   b	loc_250e764e4 ; ⤵ 0x3c
0x250e764ac:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e764b0:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e764b4:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e764b8:  e2 17 80 52   mov	w2, #0xbf
0x250e764bc:  0a 00 00 14   b	loc_250e764e4 ; ⤵ 0x28
0x250e764c0:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e764c4:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e764c8:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e764cc:  62 18 80 52   mov	w2, #0xc3
0x250e764d0:  05 00 00 14   b	loc_250e764e4 ; ⤵ 0x14
0x250e764d4:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e764d8:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e764dc:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e764e0:  02 19 80 52   mov	w2, #0xc8
0x250e764e4:  ; loc_250e764e4
0x250e764e4:  03 00 80 52   mov	w3, #0
0x250e764e8:  9e 57 ca 95   bl	j__APSLogErrorAt
0x250e764ec:  16 00 80 d2   mov	x22, #0
0x250e764f0:  ; loc_250e764f0
0x250e764f0:  f3 48 83 12   mov	w19, #0xffffe5b8
0x250e764f4:  ; loc_250e764f4
0x250e764f4:  f7 4b 40 f9   ldr	x23, [sp, #0x90]
0x250e764f8:  f5 bf 40 f9   ldr	x21, [sp, #0x178]
0x250e764fc:  55 03 00 b4   cbz	x21, loc_250e76564 ; ⤵ 0x68
0x250e76500:  e0 03 15 aa   mov	x0, x21
0x250e76504:  57 55 ca 95   bl	j__CMBaseObjectGetVTable
0x250e76508:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x250e7650c:  f1 03 00 aa   mov	x17, x0
0x250e76510:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x250e76514:  30 1a c1 da   autda	x16, x17
0x250e76518:  09 8e 41 f8   ldr	x9, [x16, #0x18]!
0x250e7651c:  e9 01 00 b4   cbz	x9, loc_250e76558 ; ⤵ 0x3c
0x250e76520:  e8 03 10 aa   mov	x8, x16
0x250e76524:  08 5f e7 f2   movk	x8, #0x3af8, lsl #0x30
0x250e76528:  f0 03 09 aa   mov	x16, x9
0x250e7652c:  f1 03 08 aa   mov	x17, x8
0x250e76530:  30 12 c1 da   autia	x16, x17
0x250e76534:  f1 03 10 aa   mov	x17, x16
0x250e76538:  f1 43 c1 da   xpaci	x17
0x250e7653c:  1f 02 11 eb   cmp	x16, x17
0x250e76540:  40 00 00 54   b.eq	loc_250e76548 ; ⤵ 0x8
0x250e76544:  00 8e 38 d4   brk	#0xc470
0x250e76548:  ; loc_250e76548
0x250e76548:  f0 23 c1 da   paciza	x16
0x250e7654c:  70 00 00 b4   cbz	x16, loc_250e76558 ; ⤵ 0xc
0x250e76550:  e0 03 15 aa   mov	x0, x21
0x250e76554:  28 09 3f d7   blraa	x9, x8
0x250e76558:  ; loc_250e76558
0x250e76558:  e0 bf 40 f9   ldr	x0, [sp, #0x178]
0x250e7655c:  40 00 00 b4   cbz	x0, loc_250e76564 ; ⤵ 0x8
0x250e76560:  84 2d ca 95   bl	j__CFRelease
0x250e76564:  ; loc_250e76564
0x250e76564:  76 00 00 b4   cbz	x22, loc_250e76570 ; ⤵ 0xc
0x250e76568:  e0 03 16 aa   mov	x0, x22
0x250e7656c:  81 2d ca 95   bl	j__CFRelease
0x250e76570:  ; loc_250e76570
0x250e76570:  f3 f3 00 b9   str	w19, [sp, #0xf0]
0x250e76574:  e0 03 13 aa   mov	x0, x19
0x250e76578:  61 03 00 d0   adrp	x1, 0x250ee4000
0x250e7657c:  21 d4 03 91   add	x1, x1, #0xf5 ; "_RemoteControlSessionSetup"
0x250e76580:  e2 07 83 52   mov	w2, #0x183f
0x250e76584:  03 00 80 52   mov	w3, #0
0x250e76588:  76 57 ca 95   bl	j__APSLogErrorAt
0x250e7658c:  f6 47 40 f9   ldr	x22, [sp, #0x88]
0x250e76590:  f5 23 40 f9   ldr	x21, [sp, #0x40]
0x250e76594:  e6 fd ff 17   b	loc_250e75d2c ; ⤴ -0x868
0x250e76598:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e7659c:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e765a0:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e765a4:  82 19 80 52   mov	w2, #0xcc
0x250e765a8:  03 00 80 52   mov	w3, #0
0x250e765ac:  6d 57 ca 95   bl	j__APSLogErrorAt
0x250e765b0:  d0 ff ff 17   b	loc_250e764f0 ; ⤴ -0xc0
0x250e765b4:  f3 03 00 aa   mov	x19, x0
0x250e765b8:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250e765bc:  c1 02 00 90   adrp	x1, 0x250ece000
0x250e765c0:  21 b4 14 91   add	x1, x1, #0x52d ; "APReceiverRemoteControlSessionSenderUIEventsChannelCreate"
0x250e765c4:  02 1b 80 52   mov	w2, #0xd8
0x250e765c8:  03 00 80 52   mov	w3, #0
0x250e765cc:  65 57 ca 95   bl	j__APSLogErrorAt
0x250e765d0:  c9 ff ff 17   b	loc_250e764f4 ; ⤴ -0xdc
0x250e765d4:  ; loc_250e765d4
0x250e765d4:  61 03 00 90   adrp	x1, 0x250ee2000
0x250e765d8:  21 e8 33 91   add	x1, x1, #0xcfa ; "_ScreenStart"
0x250e765dc:  c2 53 82 52   mov	w2, #0x129e
0x250e765e0:  03 00 80 52   mov	w3, #0
0x250e765e4:  5f 57 ca 95   bl	j__APSLogErrorAt
0x250e765e8:  f5 48 83 12   mov	w21, #0xffffe5b8
0x250e765ec:  0f ff ff 17   b	loc_250e76228 ; ⤴ -0x3c4
0x250e765f0:  f5 03 00 aa   mov	x21, x0
0x250e765f4:  ab f9 ff 17   b	loc_250e74ca0 ; ⤴ -0x1954
0x250e765f8:  f5 03 00 aa   mov	x21, x0
0x250e765fc:  1b fa ff 17   b	loc_250e74e68 ; ⤴ -0x1794
0x250e76600:  ; loc_250e76600
0x250e76600:  f5 03 00 aa   mov	x21, x0
0x250e76604:  03 ff ff 17   b	loc_250e76210 ; ⤴ -0x3f4
0x250e76608:  ; loc_250e76608
0x250e76608:  e9 23 56 29   ldp	w9, w8, [sp, #0xb0]
0x250e7660c:  08 01 09 2a   orr	w8, w8, w9
0x250e76610:  1f 01 00 71   cmp	w8, #0
0x250e76614:  e8 17 9f 1a   cset	w8, eq
0x250e76618:  89 c2 42 b9   ldr	w9, [x20, #0x2c0]
0x250e7661c:  3f 09 00 71   cmp	w9, #0x2
0x250e76620:  fa 5f 45 a9   ldp	x26, x23, [sp, #0x50]
0x250e76624:  c1 02 00 54   b.ne	loc_250e7667c ; ⤵ 0x58
0x250e76628:  a8 02 00 37   tbnz	w8, #0, loc_250e7667c ; ⤵ 0x54
0x250e7662c:  80 22 41 f9   ldr	x0, [x20, #0x240]
0x250e76630:  d8 2f ca 95   bl	j__CFDictionaryGetCount
0x250e76634:  f3 03 00 aa   mov	x19, x0
0x250e76638:  80 22 41 f9   ldr	x0, [x20, #0x240]
0x250e7663c:  e2 83 03 91   add	x2, sp, #0xe0
0x250e76640:  01 00 80 d2   mov	x1, #0
0x250e76644:  14 ee ff 97   bl	0x250e71e94
0x250e76648:  e0 ef 00 b9   str	w0, [sp, #0xec]
0x250e7664c:  a0 29 00 35   cbnz	w0, loc_250e76b80 ; ⤵ 0x534
0x250e76650:  7f 06 00 f1   cmp	x19, #0x1
0x250e76654:  4b 01 00 54   b.lt	loc_250e7667c ; ⤵ 0x28
0x250e76658:  15 00 80 d2   mov	x21, #0
0x250e7665c:  f6 73 40 f9   ldr	x22, [sp, #0xe0]
0x250e76660:  ; loc_250e76660
0x250e76660:  e0 03 16 aa   mov	x0, x22
0x250e76664:  e1 03 15 aa   mov	x1, x21
0x250e76668:  aa 2f ca 95   bl	j__CFArrayGetValueAtIndex
0x250e7666c:  96 48 fe 97   bl	0x250e088c4
0x250e76670:  b5 06 00 91   add	x21, x21, #0x1
0x250e76674:  7f 02 15 eb   cmp	x19, x21
0x250e76678:  41 ff ff 54   b.ne	loc_250e76660 ; ⤴ -0x18
0x250e7667c:  ; loc_250e7667c
0x250e7667c:  5b 03 00 f9   str	x27, [x26]
0x250e76680:  ff ef 00 b9   str	wzr, [sp, #0xec]
0x250e76684:  e8 9f 40 b9   ldr	w8, [sp, #0x9c]
0x250e76688:  a8 00 00 35   cbnz	w8, loc_250e7669c ; ⤵ 0x14
0x250e7668c:  e0 03 14 aa   mov	x0, x20
0x250e76690:  21 00 80 52   mov	w1, #0x1
0x250e76694:  02 00 80 52   mov	w2, #0
0x250e76698:  46 e4 ff 97   bl	0x250e6f7b0
0x250e7669c:  ; loc_250e7669c
0x250e7669c:  1b 00 80 d2   mov	x27, #0
0x250e766a0:  e0 73 40 f9   ldr	x0, [sp, #0xe0]
0x250e766a4:  40 00 00 b4   cbz	x0, loc_250e766ac ; ⤵ 0x8

