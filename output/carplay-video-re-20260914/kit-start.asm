
_uiController_startVideoPlayback
0x250f08fd8:  7f 23 03 d5   pacibsp
0x250f08fdc:  f8 5f bc a9   stp	x24, x23, [sp, #-0x40]!
0x250f08fe0:  f6 57 01 a9   stp	x22, x21, [sp, #0x10]
0x250f08fe4:  f4 4f 02 a9   stp	x20, x19, [sp, #0x20]
0x250f08fe8:  fd 7b 03 a9   stp	fp, lr, [sp, #0x30]
0x250f08fec:  fd c3 00 91   add	fp, sp, #0x30
0x250f08ff0:  f3 03 04 aa   mov	x19, x4
0x250f08ff4:  f7 03 03 aa   mov	x23, x3
0x250f08ff8:  f5 03 02 aa   mov	x21, x2
0x250f08ffc:  f6 03 01 aa   mov	x22, x1
0x250f09000:  f4 03 00 aa   mov	x20, x0
0x250f09004:  48 c2 0e d0   adrp	x8, 0x26e753000
0x250f09008:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250f0900c:  1f c9 00 71   cmp	w8, #0x32
0x250f09010:  2c 01 00 54   b.gt	loc_250f09034 ; ⤵ 0x24
0x250f09014:  1f 05 00 31   cmn	w8, #0x1
0x250f09018:  81 06 00 54   b.ne	loc_250f090e8 ; ⤵ 0xd0
0x250f0901c:  40 c2 0e d0   adrp	x0, 0x26e753000
0x250f09020:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250f09024:  41 06 80 52   mov	w1, #0x32
0x250f09028:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f0902c:  65 df c7 95   bl	__LogCategory_Initialize_stub
0x250f09030:  c0 05 00 35   cbnz	w0, loc_250f090e8 ; ⤵ 0xb8
0x250f09034:  ; loc_250f09034
0x250f09034:  e0 03 14 aa   mov	x0, x20
0x250f09038:  62 11 c8 95   bl	_CMBaseObjectGetDerivedStorage_stub
0x250f0903c:  f4 03 00 aa   mov	x20, x0
0x250f09040:  00 00 40 f9   ldr	x0, [x0]
0x250f09044:  60 00 00 b4   cbz	x0, loc_250f09050 ; ⤵ 0xc
0x250f09048:  ca e2 c7 95   bl	_CFRelease_stub
0x250f0904c:  9f 02 00 f9   str	xzr, [x20]
0x250f09050:  ; loc_250f09050
0x250f09050:  b7 00 00 b4   cbz	x23, loc_250f09064 ; ⤵ 0x14
0x250f09054:  e0 03 17 aa   mov	x0, x23
0x250f09058:  de 12 c8 95   bl	_FigCFWeakReferenceHolderCreateWithReferencedObject_stub
0x250f0905c:  80 02 00 f9   str	x0, [x20]
0x250f09060:  80 04 00 b4   cbz	x0, loc_250f090f0 ; ⤵ 0x90
0x250f09064:  ; loc_250f09064
0x250f09064:  88 1a 40 f9   ldr	x8, [x20, #0x30]
0x250f09068:  92 dd c7 95   bl	_objc_retain_x8_stub
0x250f0906c:  f7 03 00 aa   mov	x23, x0
0x250f09070:  84 df c7 95   bl	_objc_sync_enter_stub
0x250f09074:  96 d6 01 a9   stp	x22, x21, [x20, #0x18]
0x250f09078:  e0 03 17 aa   mov	x0, x23
0x250f0907c:  85 df c7 95   bl	_objc_sync_exit_stub
0x250f09080:  54 dd c7 95   bl	_objc_release_x23_stub
0x250f09084:  01 5f 12 90   adrp	x1, 0x275ae9000
0x250f09088:  21 20 33 91   add	x1, x1, #0xcc8 ; _ptr.__NSCFConstantString
0x250f0908c:  e0 03 13 aa   mov	x0, x19
0x250f09090:  02 00 80 d2   mov	x2, #0
0x250f09094:  e7 0f c8 95   bl	_CFDictionaryGetInt64_stub
0x250f09098:  f5 03 00 aa   mov	x21, x0
0x250f0909c:  48 8f 0b f0   adrp	x8, 0x2680f4000
0x250f090a0:  08 71 40 f9   ldr	x8, [x8, #0xe0] ; _ptr._kAPReceiverUIControllerParam_NetworkClock
0x250f090a4:  01 01 40 f9   ldr	x1, [x8]
0x250f090a8:  e0 03 13 aa   mov	x0, x19
0x250f090ac:  41 e5 c7 95   bl	_CFDictionaryGetValue_stub
0x250f090b0:  80 00 00 b4   cbz	x0, loc_250f090c0 ; ⤵ 0x10
0x250f090b4:  e2 03 00 aa   mov	x2, x0
0x250f090b8:  80 1a 40 f9   ldr	x0, [x20, #0x30]
0x250f090bc:  e1 62 c4 95   bl	0x258021c40
0x250f090c0:  ; loc_250f090c0
0x250f090c0:  80 1a 40 f9   ldr	x0, [x20, #0x30]
0x250f090c4:  e2 03 15 aa   mov	x2, x21
0x250f090c8:  43 00 80 52   mov	w3, #0x2
0x250f090cc:  b1 61 c4 95   bl	0x258021790
0x250f090d0:  00 00 80 52   mov	w0, #0
0x250f090d4:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x250f090d8:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x250f090dc:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x250f090e0:  f8 5f c4 a8   ldp	x24, x23, [sp], #0x40
0x250f090e4:  ff 0f 5f d6   retab
0x250f090e8:  ; loc_250f090e8
0x250f090e8:  36 1a 00 94   bl	_uiController_startVideoPlayback.cold.1
0x250f090ec:  d2 ff ff 17   b	loc_250f09034 ; ⤴ -0xb8
0x250f090f0:  ; loc_250f090f0
0x250f090f0:  3c 1a 00 94   bl	_uiController_startVideoPlayback.cold.2
0x250f090f4:  e0 48 83 12   mov	w0, #0xffffe5b8

