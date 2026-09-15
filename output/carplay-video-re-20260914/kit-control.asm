
_uiController_controlVideoPlayback
0x250f091ac:  7f 23 03 d5   pacibsp
0x250f091b0:  ff 83 01 d1   sub	sp, sp, #0x60
0x250f091b4:  fa 67 01 a9   stp	x26, x25, [sp, #0x10]
0x250f091b8:  f8 5f 02 a9   stp	x24, x23, [sp, #0x20]
0x250f091bc:  f6 57 03 a9   stp	x22, x21, [sp, #0x30]
0x250f091c0:  f4 4f 04 a9   stp	x20, x19, [sp, #0x40]
0x250f091c4:  fd 7b 05 a9   stp	fp, lr, [sp, #0x50]
0x250f091c8:  fd 43 01 91   add	fp, sp, #0x50
0x250f091cc:  f5 03 02 aa   mov	x21, x2
0x250f091d0:  f7 03 01 aa   mov	x23, x1
0x250f091d4:  f4 03 00 aa   mov	x20, x0
0x250f091d8:  48 8f 0b f0   adrp	x8, 0x2680f4000
0x250f091dc:  08 65 40 f9   ldr	x8, [x8, #0xc8] ; _ptr._kAPReceiverUIControllerParam_Data
0x250f091e0:  01 01 40 f9   ldr	x1, [x8]
0x250f091e4:  e0 03 17 aa   mov	x0, x23
0x250f091e8:  f2 e4 c7 95   bl	_CFDictionaryGetValue_stub
0x250f091ec:  f3 03 00 aa   mov	x19, x0
0x250f091f0:  48 8f 0b f0   adrp	x8, 0x2680f4000
0x250f091f4:  08 75 40 f9   ldr	x8, [x8, #0xe8] ; _ptr._kAPReceiverUIControllerParam_PerformSync
0x250f091f8:  01 01 40 f9   ldr	x1, [x8]
0x250f091fc:  e0 03 17 aa   mov	x0, x23
0x250f09200:  ec e4 c7 95   bl	_CFDictionaryGetValue_stub
0x250f09204:  53 0a 00 b4   cbz	x19, loc_250f0934c ; ⤵ 0x148
0x250f09208:  f6 03 00 aa   mov	x22, x0
0x250f0920c:  e0 03 14 aa   mov	x0, x20
0x250f09210:  ec 10 c8 95   bl	_CMBaseObjectGetDerivedStorage_stub
0x250f09214:  08 18 40 f9   ldr	x8, [x0, #0x30]
0x250f09218:  c8 04 00 b4   cbz	x8, loc_250f092b0 ; ⤵ 0x98
0x250f0921c:  f8 03 00 aa   mov	x24, x0
0x250f09220:  28 8e 0b d0   adrp	x8, 0x2680cf000
0x250f09224:  00 9d 43 f9   ldr	x0, [x8, #0x738] ; _ptr.NSPropertyListSerialization
0x250f09228:  ff 07 00 f9   str	xzr, [sp, #0x8]
0x250f0922c:  e5 23 00 91   add	x5, sp, #0x8
0x250f09230:  e2 03 13 aa   mov	x2, x19
0x250f09234:  03 00 80 d2   mov	x3, #0
0x250f09238:  04 00 80 d2   mov	x4, #0
0x250f0923c:  9d e3 c3 95   bl	0x2580020b0
0x250f09240:  98 de c7 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250f09244:  f4 03 00 aa   mov	x20, x0
0x250f09248:  e8 07 40 f9   ldr	x8, [sp, #0x8]
0x250f0924c:  19 dd c7 95   bl	_objc_retain_x8_stub
0x250f09250:  f3 03 00 aa   mov	x19, x0
0x250f09254:  20 03 00 b5   cbnz	x0, loc_250f092b8 ; ⤵ 0x64
0x250f09258:  14 03 00 b4   cbz	x20, loc_250f092b8 ; ⤵ 0x60
0x250f0925c:  ff 07 00 b9   str	wzr, [sp, #0x4]
0x250f09260:  01 5f 12 90   adrp	x1, 0x275ae9000
0x250f09264:  21 20 36 91   add	x1, x1, #0xd88 ; _ptr.__NSCFConstantString
0x250f09268:  e2 13 00 91   add	x2, sp, #0x4
0x250f0926c:  e0 03 17 aa   mov	x0, x23
0x250f09270:  e8 11 c8 95   bl	_FigCFDictionaryGetInt32IfPresent_stub
0x250f09274:  e0 03 00 34   cbz	w0, loc_250f092f0 ; ⤵ 0x7c
0x250f09278:  c8 8d 0b d0   adrp	x8, 0x2680c3000
0x250f0927c:  08 f5 40 f9   ldr	x8, [x8, #0x1e8] ; _ptr._kCFBooleanTrue
0x250f09280:  19 01 40 f9   ldr	x25, [x8]
0x250f09284:  00 1b 40 f9   ldr	x0, [x24, #0x30]
0x250f09288:  b2 5f c4 95   bl	0x258021150
0x250f0928c:  85 de c7 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250f09290:  f7 03 00 aa   mov	x23, x0
0x250f09294:  e3 07 40 b9   ldr	w3, [sp, #0x4]
0x250f09298:  e2 03 14 aa   mov	x2, x20
0x250f0929c:  3f 03 16 eb   cmp	x25, x22
0x250f092a0:  40 04 00 54   b.eq	loc_250f09328 ; ⤵ 0x88
0x250f092a4:  23 61 c4 95   bl	0x258021730
0x250f092a8:  ca dc c7 95   bl	_objc_release_x23_stub
0x250f092ac:  27 00 00 14   b	loc_250f09348 ; ⤵ 0x9c
0x250f092b0:  ; loc_250f092b0
0x250f092b0:  13 00 80 d2   mov	x19, #0
0x250f092b4:  26 00 00 14   b	loc_250f0934c ; ⤵ 0x98
0x250f092b8:  ; loc_250f092b8
0x250f092b8:  48 c2 0e d0   adrp	x8, 0x26e753000
0x250f092bc:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250f092c0:  1f 69 01 71   cmp	w8, #0x5a
0x250f092c4:  2c 04 00 54   b.gt	loc_250f09348 ; ⤵ 0x84
0x250f092c8:  1f 05 00 31   cmn	w8, #0x1
0x250f092cc:  e1 00 00 54   b.ne	loc_250f092e8 ; ⤵ 0x1c
0x250f092d0:  40 c2 0e d0   adrp	x0, 0x26e753000
0x250f092d4:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250f092d8:  41 0b 80 52   mov	w1, #0x5a
0x250f092dc:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f092e0:  b8 de c7 95   bl	__LogCategory_Initialize_stub
0x250f092e4:  20 03 00 34   cbz	w0, loc_250f09348 ; ⤵ 0x64
0x250f092e8:  ; loc_250f092e8
0x250f092e8:  cb 19 00 94   bl	_uiController_controlVideoPlayback.cold.1
0x250f092ec:  17 00 00 14   b	loc_250f09348 ; ⤵ 0x5c
0x250f092f0:  ; loc_250f092f0
0x250f092f0:  48 c2 0e d0   adrp	x8, 0x26e753000
0x250f092f4:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250f092f8:  1f 69 01 71   cmp	w8, #0x5a
0x250f092fc:  6c 02 00 54   b.gt	loc_250f09348 ; ⤵ 0x4c
0x250f09300:  1f 05 00 31   cmn	w8, #0x1
0x250f09304:  e1 00 00 54   b.ne	loc_250f09320 ; ⤵ 0x1c
0x250f09308:  40 c2 0e d0   adrp	x0, 0x26e753000
0x250f0930c:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250f09310:  41 0b 80 52   mov	w1, #0x5a
0x250f09314:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f09318:  aa de c7 95   bl	__LogCategory_Initialize_stub
0x250f0931c:  60 01 00 34   cbz	w0, loc_250f09348 ; ⤵ 0x2c
0x250f09320:  ; loc_250f09320
0x250f09320:  c6 19 00 94   bl	_uiController_controlVideoPlayback.cold.2
0x250f09324:  09 00 00 14   b	loc_250f09348 ; ⤵ 0x24
0x250f09328:  ; loc_250f09328
0x250f09328:  06 61 c4 95   bl	0x258021740
0x250f0932c:  5d de c7 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250f09330:  f6 03 00 aa   mov	x22, x0
0x250f09334:  a7 dc c7 95   bl	_objc_release_x23_stub
0x250f09338:  75 00 00 b4   cbz	x21, loc_250f09344 ; ⤵ 0xc
0x250f0933c:  d5 dc c7 95   bl	_objc_retain_x22_stub
0x250f09340:  a0 02 00 f9   str	x0, [x21]
0x250f09344:  ; loc_250f09344
0x250f09344:  9f dc c7 95   bl	_objc_release_x22_stub
0x250f09348:  ; loc_250f09348
0x250f09348:  96 dc c7 95   bl	_objc_release_x20_stub
0x250f0934c:  ; loc_250f0934c
0x250f0934c:  e0 03 13 aa   mov	x0, x19
0x250f09350:  4c ed c3 95   bl	0x258004880
0x250f09354:  f4 03 00 aa   mov	x20, x0
0x250f09358:  8e dc c7 95   bl	_objc_release_x19_stub
0x250f0935c:  e0 03 14 aa   mov	x0, x20
0x250f09360:  fd 7b 45 a9   ldp	fp, lr, [sp, #0x50]
0x250f09364:  f4 4f 44 a9   ldp	x20, x19, [sp, #0x40]
0x250f09368:  f6 57 43 a9   ldp	x22, x21, [sp, #0x30]
0x250f0936c:  f8 5f 42 a9   ldp	x24, x23, [sp, #0x20]
0x250f09370:  fa 67 41 a9   ldp	x26, x25, [sp, #0x10]
0x250f09374:  ff 83 01 91   add	sp, sp, #0x60
0x250f09378:  ff 0f 5f d6   retab

