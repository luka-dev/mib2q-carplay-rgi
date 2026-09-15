
_screenstream_dequeueAndProcessSampleBuffer:
0x250f4f008:  7f 23 03 d5   pacibsp
0x250f4f00c:  ff 83 05 d1   sub	sp, sp, #0x160
0x250f4f010:  e9 23 0f 6d   stp	d9, d8, [sp, #0xf0]
0x250f4f014:  fc 6f 10 a9   stp	x28, x27, [sp, #0x100]
0x250f4f018:  fa 67 11 a9   stp	x26, x25, [sp, #0x110]
0x250f4f01c:  f8 5f 12 a9   stp	x24, x23, [sp, #0x120]
0x250f4f020:  f6 57 13 a9   stp	x22, x21, [sp, #0x130]
0x250f4f024:  f4 4f 14 a9   stp	x20, x19, [sp, #0x140]
0x250f4f028:  fd 7b 15 a9   stp	fp, lr, [sp, #0x150]
0x250f4f02c:  fd 43 05 91   add	fp, sp, #0x150
0x250f4f030:  f8 03 02 aa   mov	x24, x2
0x250f4f034:  f3 03 01 aa   mov	x19, x1
0x250f4f038:  ff 3f 00 f9   str	xzr, [sp, #0x78]
0x250f4f03c:  e0 17 00 f9   str	x0, [sp, #0x28]
0x250f4f040:  60 f9 c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f4f044:  08 d0 51 39   ldrb	w8, [x0, #0x474]
0x250f4f048:  28 93 00 35   cbnz	w8, loc_250f502ac ; ⤵ 0x1264
0x250f4f04c:  f4 03 00 aa   mov	x20, x0
0x250f4f050:  33 97 00 35   cbnz	w19, loc_250f50334 ; ⤵ 0x12e4
0x250f4f054:  80 36 40 f9   ldr	x0, [x20, #0x68]
0x250f4f058:  4e 09 c7 95   bl	j__CMBufferQueueDequeueAndRetain
0x250f4f05c:  80 92 00 b4   cbz	x0, loc_250f502ac ; ⤵ 0x1250
0x250f4f060:  f6 03 00 aa   mov	x22, x0
0x250f4f064:  88 22 08 91   add	x8, x20, #0x208
0x250f4f068:  e8 3b 00 f9   str	x8, [sp, #0x70]
0x250f4f06c:  5c 7e 00 94   bl	_OUTLINED_FUNCTION_27
0x250f4f070:  84 09 c7 95   bl	j__CMSampleBufferGetDataBuffer
0x250f4f074:  b7 8b 0b 90   adrp	x23, 0x2680c3000
0x250f4f078:  f7 d6 40 f9   ldr	x23, [x23, #0x1a8] ; _ptr.1e0184310 (CoreFoundation/__DATA_CONST.__const)
0x250f4f07c:  13 22 0c f0   adrp	x19, 0x269392000
0x250f4f080:  bb 8b 0b 90   adrp	x27, 0x2680c3000
0x250f4f084:  7b f7 40 f9   ldr	x27, [x27, #0x1e8] ; _ptr.1e0549de0 (CoreFoundation/__DATA_CONST.__const)
0x250f4f088:  60 04 00 b4   cbz	x0, loc_250f4f114 ; ⤵ 0x8c
0x250f4f08c:  bf 7f 36 a9   stp	xzr, xzr, [fp, #-0xa0]
0x250f4f090:  ff 47 00 f9   str	xzr, [sp, #0x88]
0x250f4f094:  e0 03 16 aa   mov	x0, x22
0x250f4f098:  7a 09 c7 95   bl	j__CMSampleBufferGetDataBuffer
0x250f4f09c:  f9 03 00 aa   mov	x25, x0
0x250f4f0a0:  5c f9 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f4f0a4:  f5 03 00 aa   mov	x21, x0
0x250f4f0a8:  88 d6 41 f9   ldr	x8, [x20, #0x3a8]
0x250f4f0ac:  e8 1f 00 f9   str	x8, [sp, #0x38]
0x250f4f0b0:  a8 8b 0b 90   adrp	x8, 0x2680c3000
0x250f4f0b4:  08 a5 43 f9   ldr	x8, [x8, #0x748] ; _ptr.1842e4d68 (CoreGraphics/__TEXT.__const)
0x250f4f0b8:  00 05 40 ad   ldp	q0, q1, [x8]
0x250f4f0bc:  e0 1b 80 3d   str	q0, [sp, #0x60]
0x250f4f0c0:  a0 87 3b ad   stp	q0, q1, [fp, #-0x90]
0x250f4f0c4:  e1 13 80 3d   str	q1, [sp, #0x40]
0x250f4f0c8:  ff 87 00 b9   str	wzr, [sp, #0x84]
0x250f4f0cc:  97 da 41 f9   ldr	x23, [x20, #0x3b0]
0x250f4f0d0:  a8 8b 0b d0   adrp	x8, 0x2680c5000
0x250f4f0d4:  08 d9 44 f9   ldr	x8, [x8, #0x9b0] ; _ptr.1e0a0cf10 (CoreMedia/__DATA_CONST.__const)
0x250f4f0d8:  01 01 40 f9   ldr	x1, [x8]
0x250f4f0dc:  a2 43 02 d1   sub	x2, fp, #0x90
0x250f4f0e0:  e0 03 16 aa   mov	x0, x22
0x250f4f0e4:  cf 13 00 94   bl	_sbufGetAttachmentCGRect
0x250f4f0e8:  c0 97 00 35   cbnz	w0, loc_250f503e0 ; ⤵ 0x12f8
0x250f4f0ec:  a0 07 77 6d   ldp	d0, d1, [fp, #-0x90]
0x250f4f0f0:  a2 0f 78 6d   ldp	d2, d3, [fp, #-0x80]
0x250f4f0f4:  84 16 41 fd   ldr	d4, [x20, #0x228]
0x250f4f0f8:  85 1a 41 fd   ldr	d5, [x20, #0x230]
0x250f4f0fc:  86 1e 41 fd   ldr	d6, [x20, #0x238]
0x250f4f100:  87 22 41 fd   ldr	d7, [x20, #0x240]
0x250f4f104:  27 d7 c6 95   bl	j__CGRectEqualToRect
0x250f4f108:  00 11 00 36   tbz	w0, #0, loc_250f4f328 ; ⤵ 0x220
0x250f4f10c:  1c 00 80 52   mov	w28, #0
0x250f4f110:  9b 00 00 14   b	loc_250f4f37c ; ⤵ 0x26c
0x250f4f114:  ; loc_250f4f114
0x250f4f114:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f118:  21 80 0c 91   add	x1, x1, #0x320 ; "APStats"
0x250f4f11c:  d9 7d 00 94   bl	_OUTLINED_FUNCTION_54
0x250f4f120:  50 f9 c6 95   bl	j__CMGetAttachment
0x250f4f124:  80 25 00 b4   cbz	x0, loc_250f4f5d4 ; ⤵ 0x4b0
0x250f4f128:  fc 03 00 aa   mov	x28, x0
0x250f4f12c:  ff 4b 00 f9   str	xzr, [sp, #0x90]
0x250f4f130:  bf 7f 36 a9   stp	xzr, xzr, [fp, #-0xa0]
0x250f4f134:  2f ec c6 95   bl	j__mach_absolute_time
0x250f4f138:  7a fd c6 95   bl	j__UpTicksToNanoseconds
0x250f4f13c:  f3 03 00 aa   mov	x19, x0
0x250f4f140:  e0 03 1c aa   mov	x0, x28
0x250f4f144:  2b ca c6 95   bl	j__CFGetTypeID
0x250f4f148:  f5 03 00 aa   mov	x21, x0
0x250f4f14c:  15 cd c6 95   bl	j__CFDictionaryGetTypeID
0x250f4f150:  bf 02 00 eb   cmp	x21, x0
0x250f4f154:  f8 2f 00 f9   str	x24, [sp, #0x58]
0x250f4f158:  e1 9b 00 54   b.ne	loc_250f504d4 ; ⤵ 0x137c
0x250f4f15c:  88 16 4f 39   ldrb	w8, [x20, #0x3c5]
0x250f4f160:  c8 27 00 34   cbz	w8, loc_250f4f658 ; ⤵ 0x4f8
0x250f4f164:  88 46 41 f9   ldr	x8, [x20, #0x288]
0x250f4f168:  f8 03 13 aa   mov	x24, x19
0x250f4f16c:  68 02 08 cb   sub	x8, x19, x8
0x250f4f170:  00 01 63 9e   ucvtf	d0, x8
0x250f4f174:  c8 15 00 d0   adrp	x8, 0x251209000
0x250f4f178:  01 d9 45 fd   ldr	d1, [x8, #0xbb0]
0x250f4f17c:  20 18 60 1e   fdiv	d0, d1, d0
0x250f4f180:  88 36 42 f9   ldr	x8, [x20, #0x468]
0x250f4f184:  08 f1 7d d3   lsl	x8, x8, #0x3
0x250f4f188:  01 01 63 9e   ucvtf	d1, x8
0x250f4f18c:  08 08 61 1e   fmul	d8, d0, d1
0x250f4f190:  81 72 44 bd   ldr	s1, [x20, #0x470]
0x250f4f194:  21 d8 61 7e   ucvtf	d1, d1
0x250f4f198:  02 10 6c 1e   fmov	d2, #0.50000000
0x250f4f19c:  20 08 40 1f   fmadd	d0, d1, d0, d2
0x250f4f1a0:  15 00 78 1e   fcvtzs	w21, d0
0x250f4f1a4:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f1a8:  21 00 0f 91   add	x1, x1, #0x3c0 ; "txUsageAvg"
0x250f4f1ac:  e0 03 1c aa   mov	x0, x28
0x250f4f1b0:  00 1d a8 4e   mov	v0.16b, v8.16b
0x250f4f1b4:  3b fa c6 95   bl	j__FigCFDictionarySetDouble
0x250f4f1b8:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f1bc:  21 80 0f 91   add	x1, x1, #0x3e0 ; "queuedFramesAvg"
0x250f4f1c0:  e0 03 1c aa   mov	x0, x28
0x250f4f1c4:  e2 03 15 aa   mov	x2, x21
0x250f4f1c8:  3e fa c6 95   bl	j__FigCFDictionarySetInt32
0x250f4f1cc:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f1d0:  21 00 10 91   add	x1, x1, #0x400 ; "sentFramesAvg"
0x250f4f1d4:  e0 03 1c aa   mov	x0, x28
0x250f4f1d8:  e2 03 15 aa   mov	x2, x21
0x250f4f1dc:  39 fa c6 95   bl	j__FigCFDictionarySetInt32
0x250f4f1e0:  16 a3 ff 97   bl	_OUTLINED_FUNCTION_19
0x250f4f1e4:  1f 79 00 71   cmp	w8, #0x1e
0x250f4f1e8:  0c 02 00 54   b.gt	loc_250f4f228 ; ⤵ 0x40
0x250f4f1ec:  7a 52 06 94   bl	_OUTLINED_FUNCTION_32
0x250f4f1f0:  a1 00 00 54   b.ne	loc_250f4f204 ; ⤵ 0x14
0x250f4f1f4:  61 2a 00 51   sub	w1, w19, #0xa
0x250f4f1f8:  bf a2 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f1fc:  f1 c6 c6 95   bl	j___LogCategory_Initialize
0x250f4f200:  40 01 00 34   cbz	w0, loc_250f4f228 ; ⤵ 0x28
0x250f4f204:  ; loc_250f4f204
0x250f4f204:  88 da 41 f9   ldr	x8, [x20, #0x3b0]
0x250f4f208:  62 2a 00 51   sub	w2, w19, #0xa
0x250f4f20c:  e8 73 00 a9   stp	x8, x28, [sp]
0x250f4f210:  b9 a2 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f214:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4f218:  21 0c 1b 91   add	x1, x1, #0x6c3 ; "OSStatus sbpd_createStatsBBuf(SBufProcessingData *, CFTypeRef, CMBlockBufferRef *)"
0x250f4f21c:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f220:  63 58 1c 91   add	x3, x3, #0x716 ; "%@ sending stats dictionary: %@\n"
0x250f4f224:  37 c7 c6 95   bl	j__LogPrintF
0x250f4f228:  ; loc_250f4f228
0x250f4f228:  56 b7 ff 97   bl	_OUTLINED_FUNCTION_28
0x250f4f22c:  1a 01 40 f9   ldr	x26, [x8]
0x250f4f230:  e0 03 1a aa   mov	x0, x26
0x250f4f234:  e1 03 1c aa   mov	x1, x28
0x250f4f238:  02 19 80 52   mov	w2, #0xc8
0x250f4f23c:  03 00 80 d2   mov	x3, #0
0x250f4f240:  04 00 80 d2   mov	x4, #0
0x250f4f244:  1f f8 c6 95   bl	j__CFPropertyListCreateData
0x250f4f248:  20 96 00 b4   cbz	x0, loc_250f5050c ; ⤵ 0x12c4
0x250f4f24c:  f9 03 00 aa   mov	x25, x0
0x250f4f250:  a4 63 02 d1   sub	x4, fp, #0x98
0x250f4f254:  e0 03 1a aa   mov	x0, x26
0x250f4f258:  e1 03 19 aa   mov	x1, x25
0x250f4f25c:  2d b4 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f4f260:  a8 0a c7 95   bl	j__FigCreateBlockBufferWithCFDataNoCopy
0x250f4f264:  40 96 00 35   cbnz	w0, loc_250f5052c ; ⤵ 0x12c8
0x250f4f268:  a0 83 56 f8   ldur	x0, [fp, #-0x98]
0x250f4f26c:  e9 f8 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f4f270:  fb 03 00 aa   mov	x27, x0
0x250f4f274:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f278:  21 80 10 91   add	x1, x1, #0x420 ; "submitSurfaceFPS"
0x250f4f27c:  e0 03 1c aa   mov	x0, x28
0x250f4f280:  02 00 80 d2   mov	x2, #0
0x250f4f284:  6b f7 c6 95   bl	j__CFDictionaryGetInt64
0x250f4f288:  00 00 62 9e   scvtf	d0, x0
0x250f4f28c:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f290:  21 80 09 91   add	x1, x1, #0x260 ; "Source Frames"
0x250f4f294:  f3 3b 40 f9   ldr	x19, [sp, #0x70]
0x250f4f298:  e0 03 13 aa   mov	x0, x19
0x250f4f29c:  bf 11 00 94   bl	_sbpd_addHistogramValue
0x250f4f2a0:  a0 02 62 1e   scvtf	d0, w21
0x250f4f2a4:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f2a8:  21 00 0a 91   add	x1, x1, #0x280 ; "Sent Frames"
0x250f4f2ac:  e0 03 13 aa   mov	x0, x19
0x250f4f2b0:  ba 11 00 94   bl	_sbpd_addHistogramValue
0x250f4f2b4:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f2b8:  21 00 11 91   add	x1, x1, #0x440 ; "sinkOverflowDropFPS"
0x250f4f2bc:  e0 03 1c aa   mov	x0, x28
0x250f4f2c0:  02 00 80 d2   mov	x2, #0
0x250f4f2c4:  5b f7 c6 95   bl	j__CFDictionaryGetInt64
0x250f4f2c8:  00 00 62 9e   scvtf	d0, x0
0x250f4f2cc:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f2d0:  21 80 0a 91   add	x1, x1, #0x2a0 ; "Dropped Overflow Frames"
0x250f4f2d4:  e0 03 13 aa   mov	x0, x19
0x250f4f2d8:  b0 11 00 94   bl	_sbpd_addHistogramValue
0x250f4f2dc:  c8 15 00 d0   adrp	x8, 0x251209000
0x250f4f2e0:  09 a9 47 fd   ldr	d9, [x8, #0xf50]
0x250f4f2e4:  00 19 69 1e   fdiv	d0, d8, d9
0x250f4f2e8:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f2ec:  21 80 0b 91   add	x1, x1, #0x2e0 ; "Used Bandwidth"
0x250f4f2f0:  e0 03 13 aa   mov	x0, x19
0x250f4f2f4:  a9 11 00 94   bl	_sbpd_addHistogramValue
0x250f4f2f8:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f2fc:  21 80 11 91   add	x1, x1, #0x460 ; "txCapacityAvg"
0x250f4f300:  e0 03 1c aa   mov	x0, x28
0x250f4f304:  02 00 80 d2   mov	x2, #0
0x250f4f308:  3e f7 c6 95   bl	j__CFDictionaryGetDouble
0x250f4f30c:  00 18 69 1e   fdiv	d0, d0, d9
0x250f4f310:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f314:  21 00 0c 91   add	x1, x1, #0x300 ; "Available Bandwidth"
0x250f4f318:  e0 03 13 aa   mov	x0, x19
0x250f4f31c:  9f 11 00 94   bl	_sbpd_addHistogramValue
0x250f4f320:  f3 03 18 aa   mov	x19, x24
0x250f4f324:  cf 00 00 14   b	loc_250f4f660 ; ⤵ 0x33c
0x250f4f328:  ; loc_250f4f328
0x250f4f328:  93 a2 08 91   add	x19, x20, #0x228
0x250f4f32c:  23 52 06 94   bl	_OUTLINED_FUNCTION_13
0x250f4f330:  0c 02 00 54   b.gt	loc_250f4f370 ; ⤵ 0x40
0x250f4f334:  1f 05 00 31   cmn	w8, #0x1
0x250f4f338:  61 00 00 54   b.ne	loc_250f4f344 ; ⤵ 0xc
0x250f4f33c:  58 c8 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4f340:  80 01 00 34   cbz	w0, loc_250f4f370 ; ⤵ 0x30
0x250f4f344:  ; loc_250f4f344
0x250f4f344:  a0 03 d7 3c   ldur	q0, [fp, #-0x90]
0x250f4f348:  a1 0b 78 6d   ldp	d1, d2, [fp, #-0x80]
0x250f4f34c:  e1 8b 01 6d   stp	d1, d2, [sp, #0x18]
0x250f4f350:  e0 83 80 3c   stur	q0, [sp, #0x8]
0x250f4f354:  f7 03 00 f9   str	x23, [sp]
0x250f4f358:  67 a2 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f35c:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4f360:  21 bc 07 91   add	x1, x1, #0x1ef ; "OSStatus sbpd_createFrameBBuf(SBufProcessingData *, CMSampleBufferRef, CMBlockBufferRef *)"
0x250f4f364:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f368:  63 28 09 91   add	x3, x3, #0x24a ; "%@ sourceRect changed to {{%.1f, %.1f}, {%.1f, %.1f}}\n"
0x250f4f36c:  1a 9f ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f370:  ; loc_250f4f370
0x250f4f370:  a0 87 7b ad   ldp	q0, q1, [fp, #-0x90]
0x250f4f374:  60 06 00 ad   stp	q0, q1, [x19]
0x250f4f378:  3c 00 80 52   mov	w28, #0x1
0x250f4f37c:  ; loc_250f4f37c
0x250f4f37c:  e1 1b c0 3d   ldr	q1, [sp, #0x60]
0x250f4f380:  e0 13 c0 3d   ldr	q0, [sp, #0x40]
0x250f4f384:  a1 83 3b ad   stp	q1, q0, [fp, #-0x90]
0x250f4f388:  a8 8b 0b d0   adrp	x8, 0x2680c5000
0x250f4f38c:  08 c9 44 f9   ldr	x8, [x8, #0x990] ; _ptr.1e0a0cf18 (CoreMedia/__DATA_CONST.__const)
0x250f4f390:  01 01 40 f9   ldr	x1, [x8]
0x250f4f394:  a2 43 02 d1   sub	x2, fp, #0x90
0x250f4f398:  e0 03 16 aa   mov	x0, x22
0x250f4f39c:  21 13 00 94   bl	_sbufGetAttachmentCGRect
0x250f4f3a0:  60 82 00 35   cbnz	w0, loc_250f503ec ; ⤵ 0x104c
0x250f4f3a4:  a0 07 77 6d   ldp	d0, d1, [fp, #-0x90]
0x250f4f3a8:  a2 0f 78 6d   ldp	d2, d3, [fp, #-0x80]
0x250f4f3ac:  84 26 41 fd   ldr	d4, [x20, #0x248]
0x250f4f3b0:  85 2a 41 fd   ldr	d5, [x20, #0x250]
0x250f4f3b4:  86 2e 41 fd   ldr	d6, [x20, #0x258]
0x250f4f3b8:  87 32 41 fd   ldr	d7, [x20, #0x260]
0x250f4f3bc:  79 d6 c6 95   bl	j__CGRectEqualToRect
0x250f4f3c0:  c0 02 00 37   tbnz	w0, #0, loc_250f4f418 ; ⤵ 0x58
0x250f4f3c4:  93 22 09 91   add	x19, x20, #0x248
0x250f4f3c8:  fc 51 06 94   bl	_OUTLINED_FUNCTION_13
0x250f4f3cc:  0c 02 00 54   b.gt	loc_250f4f40c ; ⤵ 0x40
0x250f4f3d0:  1f 05 00 31   cmn	w8, #0x1
0x250f4f3d4:  61 00 00 54   b.ne	loc_250f4f3e0 ; ⤵ 0xc
0x250f4f3d8:  31 c8 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4f3dc:  80 01 00 34   cbz	w0, loc_250f4f40c ; ⤵ 0x30
0x250f4f3e0:  ; loc_250f4f3e0
0x250f4f3e0:  a0 03 d7 3c   ldur	q0, [fp, #-0x90]
0x250f4f3e4:  a1 0b 78 6d   ldp	d1, d2, [fp, #-0x80]
0x250f4f3e8:  e1 8b 01 6d   stp	d1, d2, [sp, #0x18]
0x250f4f3ec:  e0 83 80 3c   stur	q0, [sp, #0x8]
0x250f4f3f0:  f7 03 00 f9   str	x23, [sp]
0x250f4f3f4:  40 a2 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f3f8:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4f3fc:  21 bc 07 91   add	x1, x1, #0x1ef ; "OSStatus sbpd_createFrameBBuf(SBufProcessingData *, CMSampleBufferRef, CMBlockBufferRef *)"
0x250f4f400:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f404:  63 04 0a 91   add	x3, x3, #0x281 ; "%@ destRect changed to {{%.1f, %.1f}, {%.1f, %.1f}}\n"
0x250f4f408:  f3 9e ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f40c:  ; loc_250f4f40c
0x250f4f40c:  a0 87 7b ad   ldp	q0, q1, [fp, #-0x90]
0x250f4f410:  60 06 00 ad   stp	q0, q1, [x19]
0x250f4f414:  3c 00 80 52   mov	w28, #0x1
0x250f4f418:  ; loc_250f4f418
0x250f4f418:  f5 23 00 f9   str	x21, [sp, #0x40]
0x250f4f41c:  a8 8b 0b d0   adrp	x8, 0x2680c5000
0x250f4f420:  08 dd 44 f9   ldr	x8, [x8, #0x9b8] ; _ptr.1e0a0cf28 (CoreMedia/__DATA_CONST.__const)
0x250f4f424:  01 01 40 f9   ldr	x1, [x8]
0x250f4f428:  16 7d 00 94   bl	_OUTLINED_FUNCTION_54
0x250f4f42c:  8d f8 c6 95   bl	j__CMGetAttachment
0x250f4f430:  40 77 00 b4   cbz	x0, loc_250f50318 ; ⤵ 0xee8
0x250f4f434:  f5 03 00 aa   mov	x21, x0
0x250f4f438:  6e c9 c6 95   bl	j__CFGetTypeID
0x250f4f43c:  fa 03 00 aa   mov	x26, x0
0x250f4f440:  74 e3 c6 95   bl	j__CFNumberGetTypeID
0x250f4f444:  5f 03 00 eb   cmp	x26, x0
0x250f4f448:  81 76 00 54   b.ne	loc_250f50318 ; ⤵ 0xed0
0x250f4f44c:  e2 13 02 91   add	x2, sp, #0x84
0x250f4f450:  e0 03 15 aa   mov	x0, x21
0x250f4f454:  61 00 80 52   mov	w1, #0x3
0x250f4f458:  6e cc c6 95   bl	j__CFNumberGetValue
0x250f4f45c:  ; loc_250f4f45c
0x250f4f45c:  e8 87 40 b9   ldr	w8, [sp, #0x84]
0x250f4f460:  89 6a 42 b9   ldr	w9, [x20, #0x268]
0x250f4f464:  1f 01 09 6b   cmp	w8, w9
0x250f4f468:  80 02 00 54   b.eq	loc_250f4f4b8 ; ⤵ 0x50
0x250f4f46c:  e9 a9 0f f0   adrp	x9, 0x27048e000
0x250f4f470:  29 31 4b b9   ldr	w9, [x9, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4f474:  3f a1 00 71   cmp	w9, #0x28
0x250f4f478:  cc 01 00 54   b.gt	loc_250f4f4b0 ; ⤵ 0x38
0x250f4f47c:  3f 05 00 31   cmn	w9, #0x1
0x250f4f480:  81 00 00 54   b.ne	loc_250f4f490 ; ⤵ 0x10
0x250f4f484:  06 c8 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4f488:  e8 87 40 b9   ldr	w8, [sp, #0x84]
0x250f4f48c:  20 01 00 34   cbz	w0, loc_250f4f4b0 ; ⤵ 0x24
0x250f4f490:  ; loc_250f4f490
0x250f4f490:  f7 23 00 a9   stp	x23, x8, [sp]
0x250f4f494:  18 a2 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f498:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4f49c:  21 bc 07 91   add	x1, x1, #0x1ef ; "OSStatus sbpd_createFrameBBuf(SBufProcessingData *, CMSampleBufferRef, CMBlockBufferRef *)"
0x250f4f4a0:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f4a4:  63 d8 0a 91   add	x3, x3, #0x2b6 ; "%@ transform changed to 0x%x\n"
0x250f4f4a8:  cb 9e ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f4ac:  e8 87 40 b9   ldr	w8, [sp, #0x84]
0x250f4f4b0:  ; loc_250f4f4b0
0x250f4f4b0:  88 6a 02 b9   str	w8, [x20, #0x268]
0x250f4f4b4:  3c 00 80 52   mov	w28, #0x1
0x250f4f4b8:  ; loc_250f4f4b8
0x250f4f4b8:  f9 1b 00 f9   str	x25, [sp, #0x30]
0x250f4f4bc:  f8 db 05 a9   stp	x24, x22, [sp, #0x58]
0x250f4f4c0:  e0 03 16 aa   mov	x0, x22
0x250f4f4c4:  73 fe c6 95   bl	j__CMSampleBufferGetFormatDescription
0x250f4f4c8:  fa 03 00 aa   mov	x26, x0
0x250f4f4cc:  61 f8 c6 95   bl	j__CMFormatDescriptionGetMediaSubType
0x250f4f4d0:  9a 29 00 b4   cbz	x26, loc_250f4fa00 ; ⤵ 0x530
0x250f4f4d4:  f5 03 00 aa   mov	x21, x0
0x250f4f4d8:  99 da 41 f9   ldr	x25, [x20, #0x3b0]
0x250f4f4dc:  28 66 8c 52   mov	w8, #0x6331
0x250f4f4e0:  c8 2e ac 72   movk	w8, #0x6176, lsl #0x10
0x250f4f4e4:  1f 00 08 6b   cmp	w0, w8
0x250f4f4e8:  61 00 00 54   b.ne	loc_250f4f4f4 ; ⤵ 0xc
0x250f4f4ec:  88 22 4f 39   ldrb	w8, [x20, #0x3c8]
0x250f4f4f0:  08 09 00 34   cbz	w8, loc_250f4f610 ; ⤵ 0x120
0x250f4f4f4:  ; loc_250f4f4f4
0x250f4f4f4:  88 1e 4f 39   ldrb	w8, [x20, #0x3c7]
0x250f4f4f8:  c8 04 00 34   cbz	w8, loc_250f4f590 ; ⤵ 0x98
0x250f4f4fc:  81 0a 41 f9   ldr	x1, [x20, #0x210]
0x250f4f500:  e0 03 1a aa   mov	x0, x26
0x250f4f504:  47 08 c7 95   bl	j__CMFormatDescriptionEqual
0x250f4f508:  c0 27 00 35   cbnz	w0, loc_250f4fa00 ; ⤵ 0x4f8
0x250f4f50c:  f6 03 1b aa   mov	x22, x27
0x250f4f510:  e0 03 1a aa   mov	x0, x26
0x250f4f514:  87 fe c6 95   bl	j__CMVideoFormatDescriptionGetDimensions
0x250f4f518:  fb 03 00 aa   mov	x27, x0
0x250f4f51c:  13 fc 60 d3   lsr	x19, x0, #0x20
0x250f4f520:  a6 51 06 94   bl	_OUTLINED_FUNCTION_13
0x250f4f524:  8c 01 00 54   b.gt	loc_250f4f554 ; ⤵ 0x30
0x250f4f528:  1f 05 00 31   cmn	w8, #0x1
0x250f4f52c:  61 00 00 54   b.ne	loc_250f4f538 ; ⤵ 0xc
0x250f4f530:  db c7 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4f534:  00 01 00 34   cbz	w0, loc_250f4f554 ; ⤵ 0x20
0x250f4f538:  ; loc_250f4f538
0x250f4f538:  e8 03 1b aa   mov	x8, x27
0x250f4f53c:  e8 4f 01 a9   stp	x8, x19, [sp, #0x10]
0x250f4f540:  f9 57 00 a9   stp	x25, x21, [sp]
0x250f4f544:  19 f8 ff 97   bl	_OUTLINED_FUNCTION_8
0x250f4f548:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f54c:  63 e4 0e 91   add	x3, x3, #0x3b9 ; "%@ format description changed (%'C, %d x %d)\n"
0x250f4f550:  a1 9e ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f554:  ; loc_250f4f554
0x250f4f554:  95 0a 41 f9   ldr	x21, [x20, #0x210]
0x250f4f558:  9a 0a 01 f9   str	x26, [x20, #0x210]
0x250f4f55c:  e0 03 1a aa   mov	x0, x26
0x250f4f560:  88 c9 c6 95   bl	j__CFRetain
0x250f4f564:  75 00 00 b4   cbz	x21, loc_250f4f570 ; ⤵ 0xc
0x250f4f568:  e0 03 15 aa   mov	x0, x21
0x250f4f56c:  81 c9 c6 95   bl	j__CFRelease
0x250f4f570:  ; loc_250f4f570
0x250f4f570:  60 03 62 1e   scvtf	d0, w27
0x250f4f574:  80 0e 01 fd   str	d0, [x20, #0x218]
0x250f4f578:  60 02 62 1e   scvtf	d0, w19
0x250f4f57c:  80 12 01 fd   str	d0, [x20, #0x220]
0x250f4f580:  3c 00 80 52   mov	w28, #0x1
0x250f4f584:  f9 33 40 f9   ldr	x25, [sp, #0x60]
0x250f4f588:  fb 03 16 aa   mov	x27, x22
0x250f4f58c:  1e 01 00 14   b	loc_250f4fa04 ; ⤵ 0x478
0x250f4f590:  ; loc_250f4f590
0x250f4f590:  f6 9e ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f594:  6c 23 00 54   b.gt	loc_250f4fa00 ; ⤵ 0x46c
0x250f4f598:  1f 05 00 31   cmn	w8, #0x1
0x250f4f59c:  c1 00 00 54   b.ne	loc_250f4f5b4 ; ⤵ 0x18
0x250f4f5a0:  d5 a1 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f5a4:  64 f9 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4f5a8:  01 c9 00 11   add	w1, w8, #0x32
0x250f4f5ac:  05 c6 c6 95   bl	j___LogCategory_Initialize
0x250f4f5b0:  80 22 00 34   cbz	w0, loc_250f4fa00 ; ⤵ 0x450
0x250f4f5b4:  ; loc_250f4f5b4
0x250f4f5b4:  f9 57 00 a9   stp	x25, x21, [sp]
0x250f4f5b8:  fc f7 ff 97   bl	_OUTLINED_FUNCTION_8
0x250f4f5bc:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f5c0:  63 9c 0f 91   add	x3, x3, #0x3e7 ; "%@ ### unsupported codec: %'C\n"
0x250f4f5c4:  5c f9 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4f5c8:  02 c9 00 11   add	w2, w8, #0x32
0x250f4f5cc:  4d c6 c6 95   bl	j__LogPrintF
0x250f4f5d0:  0c 01 00 14   b	loc_250f4fa00 ; ⤵ 0x430
0x250f4f5d4:  ; loc_250f4f5d4
0x250f4f5d4:  61 46 43 f9   ldr	x1, [x19, #0x688]
0x250f4f5d8:  aa 7c 00 94   bl	_OUTLINED_FUNCTION_54
0x250f4f5dc:  21 f8 c6 95   bl	j__CMGetAttachment
0x250f4f5e0:  68 03 40 f9   ldr	x8, [x27]
0x250f4f5e4:  1f 00 08 eb   cmp	x0, x8
0x250f4f5e8:  e8 17 9f 1a   cset	w8, eq
0x250f4f5ec:  88 b2 09 39   strb	w8, [x20, #0x26c]
0x250f4f5f0:  00 eb c6 95   bl	j__mach_absolute_time
0x250f4f5f4:  80 ee 01 f9   str	x0, [x20, #0x3d8]
0x250f4f5f8:  e1 e3 01 91   add	x1, sp, #0x78
0x250f4f5fc:  e0 3b 40 f9   ldr	x0, [sp, #0x70]
0x250f4f600:  a4 4d 06 94   bl	_sbpd_createConfigBBuf
0x250f4f604:  fa 03 00 aa   mov	x26, x0
0x250f4f608:  f5 7c 00 94   bl	_OUTLINED_FUNCTION_27
0x250f4f60c:  02 03 00 14   b	loc_250f50214 ; ⤵ 0xc08
0x250f4f610:  ; loc_250f4f610
0x250f4f610:  a8 8b 0b b0   adrp	x8, 0x2680c4000
0x250f4f614:  08 8d 42 f9   ldr	x8, [x8, #0x518] ; _ptr.1e0a0c5d8 (CoreMedia/__DATA_CONST.__const)
0x250f4f618:  01 01 40 f9   ldr	x1, [x8]
0x250f4f61c:  e0 03 1a aa   mov	x0, x26
0x250f4f620:  08 f8 c6 95   bl	j__CMFormatDescriptionGetExtension
0x250f4f624:  40 18 00 b4   cbz	x0, loc_250f4f92c ; ⤵ 0x308
0x250f4f628:  f5 03 00 aa   mov	x21, x0
0x250f4f62c:  f6 03 1b aa   mov	x22, x27
0x250f4f630:  f0 c8 c6 95   bl	j__CFGetTypeID
0x250f4f634:  fb 03 00 aa   mov	x27, x0
0x250f4f638:  da cb c6 95   bl	j__CFDictionaryGetTypeID
0x250f4f63c:  7f 03 00 eb   cmp	x27, x0
0x250f4f640:  e1 11 00 54   b.ne	loc_250f4f87c ; ⤵ 0x23c
0x250f4f644:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4f648:  21 80 0d 91   add	x1, x1, #0x360 ; "avcC"
0x250f4f64c:  e0 03 15 aa   mov	x0, x21
0x250f4f650:  d8 cb c6 95   bl	j__CFDictionaryGetValue
0x250f4f654:  93 00 00 14   b	loc_250f4f8a0 ; ⤵ 0x24c
0x250f4f658:  ; loc_250f4f658
0x250f4f658:  19 00 80 d2   mov	x25, #0
0x250f4f65c:  1b 00 80 d2   mov	x27, #0
0x250f4f660:  ; loc_250f4f660
0x250f4f660:  88 4a 41 f9   ldr	x8, [x20, #0x290]
0x250f4f664:  89 46 41 f9   ldr	x9, [x20, #0x288]
0x250f4f668:  dd 7c 00 94   bl	_OUTLINED_FUNCTION_27
0x250f4f66c:  89 03 09 8b   add	x9, x28, x9
0x250f4f670:  29 05 00 91   add	x9, x9, #0x1
0x250f4f674:  1f 01 09 eb   cmp	x8, x9
0x250f4f678:  42 08 00 54   b.hs	loc_250f4f780 ; ⤵ 0x108
0x250f4f67c:  88 1a 4f 39   ldrb	w8, [x20, #0x3c6]
0x250f4f680:  08 08 00 34   cbz	w8, loc_250f4f780 ; ⤵ 0x100
0x250f4f684:  f8 03 13 aa   mov	x24, x19
0x250f4f688:  ec a1 ff 97   bl	_OUTLINED_FUNCTION_19
0x250f4f68c:  1f 79 00 71   cmp	w8, #0x1e
0x250f4f690:  0c 02 00 54   b.gt	loc_250f4f6d0 ; ⤵ 0x40
0x250f4f694:  50 51 06 94   bl	_OUTLINED_FUNCTION_32
0x250f4f698:  a1 00 00 54   b.ne	loc_250f4f6ac ; ⤵ 0x14
0x250f4f69c:  61 2a 00 51   sub	w1, w19, #0xa
0x250f4f6a0:  95 a1 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f6a4:  c7 c5 c6 95   bl	j___LogCategory_Initialize
0x250f4f6a8:  40 01 00 34   cbz	w0, loc_250f4f6d0 ; ⤵ 0x28
0x250f4f6ac:  ; loc_250f4f6ac
0x250f4f6ac:  88 da 41 f9   ldr	x8, [x20, #0x3b0]
0x250f4f6b0:  62 2a 00 51   sub	w2, w19, #0xa
0x250f4f6b4:  09 35 8c 52   mov	w9, #0x61a8
0x250f4f6b8:  3c e4 ff 97   bl	_OUTLINED_FUNCTION_6
0x250f4f6bc:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4f6c0:  21 0c 1b 91   add	x1, x1, #0x6c3 ; "OSStatus sbpd_createStatsBBuf(SBufProcessingData *, CFTypeRef, CMBlockBufferRef *)"
0x250f4f6c4:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f6c8:  63 a8 1d 91   add	x3, x3, #0x76a ; "%@ sending data burst %d bytes\n"
0x250f4f6cc:  0d c6 c6 95   bl	j__LogPrintF
0x250f4f6d0:  ; loc_250f4f6d0
0x250f4f6d0:  2c b6 ff 97   bl	_OUTLINED_FUNCTION_28
0x250f4f6d4:  15 01 40 f9   ldr	x21, [x8]
0x250f4f6d8:  a8 43 02 d1   sub	x8, fp, #0x90
0x250f4f6dc:  b0 92 0b b0   adrp	x16, 0x2681a4000
0x250f4f6e0:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250f4f6e4:  f1 03 08 aa   mov	x17, x8
0x250f4f6e8:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250f4f6ec:  30 0a c1 da   pacda	x16, x17
0x250f4f6f0:  b0 03 17 f8   stur	x16, [fp, #-0x90]
0x250f4f6f4:  c9 15 00 d0   adrp	x9, 0x251209000
0x250f4f6f8:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x250f4f6fc:  a0 83 17 fc   stur	d0, [fp, #-0x88]
0x250f4f700:  08 41 00 91   add	x8, x8, #0x10
0x250f4f704:  b0 0c 00 90   adrp	x16, 0x2510e3000
0x250f4f708:  10 02 00 91   add	x16, x16, #0 ; ___getRandomDataBlob_block_invoke
0x250f4f70c:  10 01 c1 da   pacia	x16, x8
0x250f4f710:  28 22 0c f0   adrp	x8, 0x269396000
0x250f4f714:  08 c1 27 91   add	x8, x8, #0x9f0 ; ___block_descriptor_tmp.201
0x250f4f718:  b0 23 38 a9   stp	x16, x8, [fp, #-0x80]
0x250f4f71c:  08 35 8c 52   mov	w8, #0x61a8
0x250f4f720:  a8 03 19 f8   stur	x8, [fp, #-0x70]
0x250f4f724:  08 aa 0f 90   adrp	x8, 0x27048f000
0x250f4f728:  08 c5 46 f9   ldr	x8, [x8, #0xd88] ; data_27048fd88 (AirPlaySender/__DATA_DIRTY.__bss)
0x250f4f72c:  1f 05 00 b1   cmn	x8, #0x1
0x250f4f730:  a1 71 00 54   b.ne	loc_250f50564 ; ⤵ 0xe34
0x250f4f734:  ; loc_250f4f734
0x250f4f734:  08 aa 0f 90   adrp	x8, 0x27048f000
0x250f4f738:  01 c9 46 f9   ldr	x1, [x8, #0xd90] ; data_27048fd90 (AirPlaySender/__DATA_DIRTY.__bss)
0x250f4f73c:  a8 8b 0b 90   adrp	x8, 0x2680c3000
0x250f4f740:  08 dd 40 f9   ldr	x8, [x8, #0x1b8] ; _ptr.1e0184320 (CoreFoundation/__DATA_CONST.__const)
0x250f4f744:  03 01 40 f9   ldr	x3, [x8]
0x250f4f748:  a8 83 02 d1   sub	x8, fp, #0xa0
0x250f4f74c:  e8 03 00 f9   str	x8, [sp]
0x250f4f750:  e0 03 15 aa   mov	x0, x21
0x250f4f754:  02 35 8c 52   mov	w2, #0x61a8
0x250f4f758:  04 00 80 d2   mov	x4, #0
0x250f4f75c:  05 00 80 d2   mov	x5, #0
0x250f4f760:  06 35 8c 52   mov	w6, #0x61a8
0x250f4f764:  07 00 80 52   mov	w7, #0
0x250f4f768:  a6 f7 c6 95   bl	j__CMBlockBufferCreateWithMemoryBlock
0x250f4f76c:  60 70 00 35   cbnz	w0, loc_250f50578 ; ⤵ 0xe0c
0x250f4f770:  f3 03 18 aa   mov	x19, x24
0x250f4f774:  a0 03 56 f8   ldur	x0, [fp, #-0xa0]
0x250f4f778:  a6 f7 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f4f77c:  1b 00 1b 8b   add	x27, x0, x27
0x250f4f780:  ; loc_250f4f780
0x250f4f780:  20 00 80 52   mov	w0, #0x1
0x250f4f784:  01 10 80 52   mov	w1, #0x80
0x250f4f788:  c2 bd 89 52   mov	w2, #0x4dee
0x250f4f78c:  e2 20 af 72   movk	w2, #0x7907, lsl #0x10
0x250f4f790:  ec e1 c6 95   bl	j__malloc_type_calloc
0x250f4f794:  f5 03 00 aa   mov	x21, x0
0x250f4f798:  1b 00 00 b9   str	w27, [x0]
0x250f4f79c:  88 16 4f 39   ldrb	w8, [x20, #0x3c5]
0x250f4f7a0:  1f 01 00 71   cmp	w8, #0
0x250f4f7a4:  a8 00 80 52   mov	w8, #0x5
0x250f4f7a8:  49 00 80 52   mov	w9, #0x2
0x250f4f7ac:  28 01 88 1a   csel	w8, w9, w8, eq
0x250f4f7b0:  08 10 00 39   strb	w8, [x0, #0x4]
0x250f4f7b4:  88 c2 43 b9   ldr	w8, [x20, #0x3c0]
0x250f4f7b8:  08 14 00 39   strb	w8, [x0, #0x5]
0x250f4f7bc:  a0 83 56 f8   ldur	x0, [fp, #-0x98]
0x250f4f7c0:  80 00 00 b4   cbz	x0, loc_250f4f7d0 ; ⤵ 0x10
0x250f4f7c4:  93 f7 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f4f7c8:  00 00 23 9e   ucvtf	s0, x0
0x250f4f7cc:  02 00 00 14   b	loc_250f4f7d4 ; ⤵ 0x8
0x250f4f7d0:  ; loc_250f4f7d0
0x250f4f7d0:  00 e4 00 6f   movi	v0.2d, #0
0x250f4f7d4:  ; loc_250f4f7d4
0x250f4f7d4:  a0 7e 00 bd   str	s0, [x21, #0x7c]
0x250f4f7d8:  b1 9f ff 97   bl	_OUTLINED_FUNCTION_13
0x250f4f7dc:  e3 02 40 f9   ldr	x3, [x23]
0x250f4f7e0:  e8 43 02 91   add	x8, sp, #0x90
0x250f4f7e4:  e8 03 00 f9   str	x8, [sp]
0x250f4f7e8:  e1 03 15 aa   mov	x1, x21
0x250f4f7ec:  ee b2 ff 97   bl	_OUTLINED_FUNCTION_42
0x250f4f7f0:  00 68 00 35   cbnz	w0, loc_250f504f0 ; ⤵ 0xd00
0x250f4f7f4:  a1 83 56 f8   ldur	x1, [fp, #-0x98]
0x250f4f7f8:  c1 00 00 b4   cbz	x1, loc_250f4f810 ; ⤵ 0x18
0x250f4f7fc:  e0 4b 40 f9   ldr	x0, [sp, #0x90]
0x250f4f800:  c4 b2 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f4f804:  04 00 80 52   mov	w4, #0
0x250f4f808:  46 07 c7 95   bl	j__CMBlockBufferAppendBufferReference
0x250f4f80c:  c0 69 00 35   cbnz	w0, loc_250f50544 ; ⤵ 0xd38
0x250f4f810:  ; loc_250f4f810
0x250f4f810:  a1 03 56 f8   ldur	x1, [fp, #-0xa0]
0x250f4f814:  c1 00 00 b4   cbz	x1, loc_250f4f82c ; ⤵ 0x18
0x250f4f818:  e0 4b 40 f9   ldr	x0, [sp, #0x90]
0x250f4f81c:  bd b2 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f4f820:  04 00 80 52   mov	w4, #0
0x250f4f824:  3f 07 c7 95   bl	j__CMBlockBufferAppendBufferReference
0x250f4f828:  40 69 00 35   cbnz	w0, loc_250f50550 ; ⤵ 0xd28
0x250f4f82c:  ; loc_250f4f82c
0x250f4f82c:  1a 00 80 52   mov	w26, #0
0x250f4f830:  93 46 01 f9   str	x19, [x20, #0x288]
0x250f4f834:  9f 36 02 f9   str	xzr, [x20, #0x468]
0x250f4f838:  9f 72 04 b9   str	wzr, [x20, #0x470]
0x250f4f83c:  e8 4b 40 f9   ldr	x8, [sp, #0x90]
0x250f4f840:  e8 3f 00 f9   str	x8, [sp, #0x78]
0x250f4f844:  ff 4b 00 f9   str	xzr, [sp, #0x90]
0x250f4f848:  ; loc_250f4f848
0x250f4f848:  79 00 00 b4   cbz	x25, loc_250f4f854 ; ⤵ 0xc
0x250f4f84c:  ; loc_250f4f84c
0x250f4f84c:  e0 03 19 aa   mov	x0, x25
0x250f4f850:  c8 c8 c6 95   bl	j__CFRelease
0x250f4f854:  ; loc_250f4f854
0x250f4f854:  a0 83 56 f8   ldur	x0, [fp, #-0x98]
0x250f4f858:  40 00 00 b4   cbz	x0, loc_250f4f860 ; ⤵ 0x8
0x250f4f85c:  c5 c8 c6 95   bl	j__CFRelease
0x250f4f860:  ; loc_250f4f860
0x250f4f860:  a0 03 56 f8   ldur	x0, [fp, #-0xa0]
0x250f4f864:  f8 2f 40 f9   ldr	x24, [sp, #0x58]
0x250f4f868:  40 00 00 b4   cbz	x0, loc_250f4f870 ; ⤵ 0x8
0x250f4f86c:  c1 c8 c6 95   bl	j__CFRelease
0x250f4f870:  ; loc_250f4f870
0x250f4f870:  e0 4b 40 f9   ldr	x0, [sp, #0x90]
0x250f4f874:  e0 4c 00 b5   cbnz	x0, loc_250f50210 ; ⤵ 0x99c
0x250f4f878:  67 02 00 14   b	loc_250f50214 ; ⤵ 0x99c
0x250f4f87c:  ; loc_250f4f87c
0x250f4f87c:  e0 03 15 aa   mov	x0, x21
0x250f4f880:  5c c8 c6 95   bl	j__CFGetTypeID
0x250f4f884:  fb 03 00 aa   mov	x27, x0
0x250f4f888:  32 e2 c6 95   bl	j__CFArrayGetTypeID
0x250f4f88c:  7f 03 00 eb   cmp	x27, x0
0x250f4f890:  81 02 00 54   b.ne	loc_250f4f8e0 ; ⤵ 0x50
0x250f4f894:  e0 03 15 aa   mov	x0, x21
0x250f4f898:  01 00 80 d2   mov	x1, #0
0x250f4f89c:  1d cb c6 95   bl	j__CFArrayGetValueAtIndex
0x250f4f8a0:  ; loc_250f4f8a0
0x250f4f8a0:  f5 03 00 aa   mov	x21, x0
0x250f4f8a4:  fb 03 16 aa   mov	x27, x22
0x250f4f8a8:  20 04 00 b4   cbz	x0, loc_250f4f92c ; ⤵ 0x84
0x250f4f8ac:  e0 03 15 aa   mov	x0, x21
0x250f4f8b0:  50 c8 c6 95   bl	j__CFGetTypeID
0x250f4f8b4:  fb 03 00 aa   mov	x27, x0
0x250f4f8b8:  3e e2 c6 95   bl	j__CFDataGetTypeID
0x250f4f8bc:  7f 03 00 eb   cmp	x27, x0
0x250f4f8c0:  41 01 00 54   b.ne	loc_250f4f8e8 ; ⤵ 0x28
0x250f4f8c4:  e8 3b 40 f9   ldr	x8, [sp, #0x70]
0x250f4f8c8:  01 01 40 f9   ldr	x1, [x8]
0x250f4f8cc:  e0 03 15 aa   mov	x0, x21
0x250f4f8d0:  94 f8 c6 95   bl	j__FigCFEqual
0x250f4f8d4:  00 03 00 34   cbz	w0, loc_250f4f934 ; ⤵ 0x60
0x250f4f8d8:  fb 03 16 aa   mov	x27, x22
0x250f4f8dc:  14 00 00 14   b	loc_250f4f92c ; ⤵ 0x50
0x250f4f8e0:  ; loc_250f4f8e0
0x250f4f8e0:  33 00 80 52   mov	w19, #0x1
0x250f4f8e4:  2a 00 00 14   b	loc_250f4f98c ; ⤵ 0xa8
0x250f4f8e8:  ; loc_250f4f8e8
0x250f4f8e8:  20 9e ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f8ec:  fb 03 16 aa   mov	x27, x22
0x250f4f8f0:  ec 01 00 54   b.gt	loc_250f4f92c ; ⤵ 0x3c
0x250f4f8f4:  1f 05 00 31   cmn	w8, #0x1
0x250f4f8f8:  c1 00 00 54   b.ne	loc_250f4f910 ; ⤵ 0x18
0x250f4f8fc:  fe a0 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4f900:  8d f8 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4f904:  01 c9 00 11   add	w1, w8, #0x32
0x250f4f908:  2e c5 c6 95   bl	j___LogCategory_Initialize
0x250f4f90c:  00 01 00 34   cbz	w0, loc_250f4f92c ; ⤵ 0x20
0x250f4f910:  ; loc_250f4f910
0x250f4f910:  f9 57 00 a9   stp	x25, x21, [sp]
0x250f4f914:  25 f7 ff 97   bl	_OUTLINED_FUNCTION_8
0x250f4f918:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f91c:  63 40 0d 91   add	x3, x3, #0x350 ; "%@ ### bad avcC: %@\n"
0x250f4f920:  85 f8 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4f924:  02 c9 00 11   add	w2, w8, #0x32
0x250f4f928:  76 c5 c6 95   bl	j__LogPrintF
0x250f4f92c:  ; loc_250f4f92c
0x250f4f92c:  33 00 80 52   mov	w19, #0x1
0x250f4f930:  18 00 00 14   b	loc_250f4f990 ; ⤵ 0x60
0x250f4f934:  ; loc_250f4f934
0x250f4f934:  a1 50 06 94   bl	_OUTLINED_FUNCTION_13
0x250f4f938:  8c 01 00 54   b.gt	loc_250f4f968 ; ⤵ 0x30
0x250f4f93c:  1f 05 00 31   cmn	w8, #0x1
0x250f4f940:  61 00 00 54   b.ne	loc_250f4f94c ; ⤵ 0xc
0x250f4f944:  d6 c6 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4f948:  00 01 00 34   cbz	w0, loc_250f4f968 ; ⤵ 0x20
0x250f4f94c:  ; loc_250f4f94c
0x250f4f94c:  e0 03 15 aa   mov	x0, x21
0x250f4f950:  78 c8 c6 95   bl	j__CFDataGetLength
0x250f4f954:  f9 03 00 a9   stp	x25, x0, [sp]
0x250f4f958:  14 f7 ff 97   bl	_OUTLINED_FUNCTION_8
0x250f4f95c:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f960:  63 94 0d 91   add	x3, x3, #0x365 ; "%@ codec config (avcC) changed to %d bytes\n"
0x250f4f964:  9c 9d ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f968:  ; loc_250f4f968
0x250f4f968:  e8 3b 40 f9   ldr	x8, [sp, #0x70]
0x250f4f96c:  1b 01 40 f9   ldr	x27, [x8]
0x250f4f970:  15 01 00 f9   str	x21, [x8]
0x250f4f974:  e0 03 15 aa   mov	x0, x21
0x250f4f978:  82 c8 c6 95   bl	j__CFRetain
0x250f4f97c:  7b 00 00 b4   cbz	x27, loc_250f4f988 ; ⤵ 0xc
0x250f4f980:  e0 03 1b aa   mov	x0, x27
0x250f4f984:  7b c8 c6 95   bl	j__CFRelease
0x250f4f988:  ; loc_250f4f988
0x250f4f988:  13 00 80 52   mov	w19, #0
0x250f4f98c:  ; loc_250f4f98c
0x250f4f98c:  fb 03 16 aa   mov	x27, x22
0x250f4f990:  ; loc_250f4f990
0x250f4f990:  e0 03 1a aa   mov	x0, x26
0x250f4f994:  67 fd c6 95   bl	j__CMVideoFormatDescriptionGetDimensions
0x250f4f998:  f5 03 00 aa   mov	x21, x0
0x250f4f99c:  16 fc 60 d3   lsr	x22, x0, #0x20
0x250f4f9a0:  a8 02 62 1e   scvtf	d8, w21
0x250f4f9a4:  80 0e 41 fd   ldr	d0, [x20, #0x218]
0x250f4f9a8:  00 20 68 1e   fcmp	d0, d8
0x250f4f9ac:  a1 00 00 54   b.ne	loc_250f4f9c0 ; ⤵ 0x14
0x250f4f9b0:  c0 02 62 1e   scvtf	d0, w22
0x250f4f9b4:  81 12 41 fd   ldr	d1, [x20, #0x220]
0x250f4f9b8:  20 20 60 1e   fcmp	d1, d0
0x250f4f9bc:  a0 4a 00 54   b.eq	loc_250f50310 ; ⤵ 0x954
0x250f4f9c0:  ; loc_250f4f9c0
0x250f4f9c0:  7e 50 06 94   bl	_OUTLINED_FUNCTION_13
0x250f4f9c4:  6c 01 00 54   b.gt	loc_250f4f9f0 ; ⤵ 0x2c
0x250f4f9c8:  1f 05 00 31   cmn	w8, #0x1
0x250f4f9cc:  61 00 00 54   b.ne	loc_250f4f9d8 ; ⤵ 0xc
0x250f4f9d0:  b3 c6 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4f9d4:  e0 00 00 34   cbz	w0, loc_250f4f9f0 ; ⤵ 0x1c
0x250f4f9d8:  ; loc_250f4f9d8
0x250f4f9d8:  f5 db 00 a9   stp	x21, x22, [sp, #0x8]
0x250f4f9dc:  f9 03 00 f9   str	x25, [sp]
0x250f4f9e0:  f2 f6 ff 97   bl	_OUTLINED_FUNCTION_8
0x250f4f9e4:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4f9e8:  63 44 0e 91   add	x3, x3, #0x391 ; "%@ video dimensions changed to %d x %d\n"
0x250f4f9ec:  7a 9d ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4f9f0:  ; loc_250f4f9f0
0x250f4f9f0:  88 0e 01 fd   str	d8, [x20, #0x218]
0x250f4f9f4:  c0 02 62 1e   scvtf	d0, w22
0x250f4f9f8:  80 12 01 fd   str	d0, [x20, #0x220]
0x250f4f9fc:  ; loc_250f4f9fc
0x250f4f9fc:  3c 00 80 52   mov	w28, #0x1
0x250f4fa00:  ; loc_250f4fa00
0x250f4fa00:  f9 33 40 f9   ldr	x25, [sp, #0x60]
0x250f4fa04:  ; loc_250f4fa04
0x250f4fa04:  e0 03 19 aa   mov	x0, x25
0x250f4fa08:  01 00 80 52   mov	w1, #0
0x250f4fa0c:  2d f7 c6 95   bl	j__CMSampleBufferGetSampleAttachmentsArray
0x250f4fa10:  40 03 00 b4   cbz	x0, loc_250f4fa78 ; ⤵ 0x68
0x250f4fa14:  f5 03 00 aa   mov	x21, x0
0x250f4fa18:  ba ca c6 95   bl	j__CFArrayGetCount
0x250f4fa1c:  ff 43 02 39   strb	wzr, [sp, #0x90]
0x250f4fa20:  13 04 00 f1   subs	x19, x0, #0x1
0x250f4fa24:  cb 02 00 54   b.lt	loc_250f4fa7c ; ⤵ 0x58
0x250f4fa28:  f6 03 1b aa   mov	x22, x27
0x250f4fa2c:  1a 00 80 d2   mov	x26, #0
0x250f4fa30:  a8 8b 0b b0   adrp	x8, 0x2680c4000
0x250f4fa34:  08 25 43 f9   ldr	x8, [x8, #0x648] ; _ptr.1e0a0cc68 (CoreMedia/__DATA_CONST.__const)
0x250f4fa38:  1b 01 40 f9   ldr	x27, [x8]
0x250f4fa3c:  ; loc_250f4fa3c
0x250f4fa3c:  e0 03 15 aa   mov	x0, x21
0x250f4fa40:  e1 03 1a aa   mov	x1, x26
0x250f4fa44:  b3 ca c6 95   bl	j__CFArrayGetValueAtIndex
0x250f4fa48:  e2 43 02 91   add	x2, sp, #0x90
0x250f4fa4c:  e1 03 1b aa   mov	x1, x27
0x250f4fa50:  dc f7 c6 95   bl	j__FigCFDictionaryGetBooleanIfPresent
0x250f4fa54:  80 00 00 35   cbnz	w0, loc_250f4fa64 ; ⤵ 0x10
0x250f4fa58:  7f 02 1a eb   cmp	x19, x26
0x250f4fa5c:  5a 07 00 91   add	x26, x26, #0x1
0x250f4fa60:  e1 fe ff 54   b.ne	loc_250f4fa3c ; ⤴ -0x24
0x250f4fa64:  ; loc_250f4fa64
0x250f4fa64:  e8 43 42 39   ldrb	w8, [sp, #0x90]
0x250f4fa68:  1f 01 00 71   cmp	w8, #0
0x250f4fa6c:  e8 17 9f 1a   cset	w8, eq
0x250f4fa70:  fb 03 16 aa   mov	x27, x22
0x250f4fa74:  03 00 00 14   b	loc_250f4fa80 ; ⤵ 0xc
0x250f4fa78:  ; loc_250f4fa78
0x250f4fa78:  ff 43 02 39   strb	wzr, [sp, #0x90]
0x250f4fa7c:  ; loc_250f4fa7c
0x250f4fa7c:  28 00 80 52   mov	w8, #0x1
0x250f4fa80:  ; loc_250f4fa80
0x250f4fa80:  88 76 11 39   strb	w8, [x20, #0x45d]
0x250f4fa84:  e9 a9 0f f0   adrp	x9, 0x27048e000
0x250f4fa88:  29 31 4b b9   ldr	w9, [x9, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4fa8c:  3f 51 00 71   cmp	w9, #0x14
0x250f4fa90:  13 22 0c f0   adrp	x19, 0x269392000
0x250f4fa94:  4c 03 00 54   b.gt	loc_250f4fafc ; ⤵ 0x68
0x250f4fa98:  3f 05 00 31   cmn	w9, #0x1
0x250f4fa9c:  60 02 00 54   b.eq	loc_250f4fae8 ; ⤵ 0x4c
0x250f4faa0:  ; loc_250f4faa0
0x250f4faa0:  a9 15 00 f0   adrp	x9, 0x251206000
0x250f4faa4:  29 9d 11 91   add	x9, x9, #0x467 ; "Full"
0x250f4faa8:  8a da 41 f9   ldr	x10, [x20, #0x3b0]
0x250f4faac:  ab 15 00 f0   adrp	x11, 0x251206000
0x250f4fab0:  6b b1 11 91   add	x11, x11, #0x46c ; "Not"
0x250f4fab4:  1f 01 00 71   cmp	w8, #0
0x250f4fab8:  68 01 89 9a   csel	x8, x11, x9, eq
0x250f4fabc:  09 05 80 52   mov	w9, #0x28
0x250f4fac0:  09 40 a0 72   movk	w9, #0x200, lsl #0x10
0x250f4fac4:  22 51 00 51   sub	w2, w9, #0x14
0x250f4fac8:  ea 23 00 a9   stp	x10, x8, [sp]
0x250f4facc:  8a a0 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4fad0:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4fad4:  21 18 10 91   add	x1, x1, #0x406 ; "void sbpd_updateKeyFrameStatus(SBufProcessingData *, CMSampleBufferRef)"
0x250f4fad8:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4fadc:  63 38 11 91   add	x3, x3, #0x44e ; "%@ sending %sSync frame\n"
0x250f4fae0:  08 c5 c6 95   bl	j__LogPrintF
0x250f4fae4:  06 00 00 14   b	loc_250f4fafc ; ⤵ 0x18
0x250f4fae8:  ; loc_250f4fae8
0x250f4fae8:  13 f8 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4faec:  01 51 00 51   sub	w1, w8, #0x14
0x250f4faf0:  81 a0 ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4faf4:  b3 c4 c6 95   bl	j___LogCategory_Initialize
0x250f4faf8:  20 53 00 35   cbnz	w0, loc_250f5055c ; ⤵ 0xa64
0x250f4fafc:  ; loc_250f4fafc
0x250f4fafc:  61 46 43 f9   ldr	x1, [x19, #0x688]
0x250f4fb00:  55 50 06 94   bl	_OUTLINED_FUNCTION_47
0x250f4fb04:  73 03 40 f9   ldr	x19, [x27]
0x250f4fb08:  1f 00 13 eb   cmp	x0, x19
0x250f4fb0c:  e8 17 9f 1a   cset	w8, eq
0x250f4fb10:  89 b2 49 39   ldrb	w9, [x20, #0x26c]
0x250f4fb14:  3f 01 08 6b   cmp	w9, w8
0x250f4fb18:  60 00 00 54   b.eq	loc_250f4fb24 ; ⤵ 0xc
0x250f4fb1c:  88 b2 09 39   strb	w8, [x20, #0x26c]
0x250f4fb20:  3c 00 80 52   mov	w28, #0x1
0x250f4fb24:  ; loc_250f4fb24
0x250f4fb24:  08 22 0c f0   adrp	x8, 0x269392000
0x250f4fb28:  01 41 43 f9   ldr	x1, [x8, #0x680] ; _kAPSampleBufferAttachmentKey_InterestingFrame
0x250f4fb2c:  4a 50 06 94   bl	_OUTLINED_FUNCTION_47
0x250f4fb30:  1f 00 13 eb   cmp	x0, x19
0x250f4fb34:  e8 17 9f 1a   cset	w8, eq
0x250f4fb38:  88 72 11 39   strb	w8, [x20, #0x45c]
0x250f4fb3c:  08 22 0c f0   adrp	x8, 0x269392000
0x250f4fb40:  01 3d 43 f9   ldr	x1, [x8, #0x678] ; _kAPSampleBufferAttachmentKey_SubmitSurfaceTimestamp
0x250f4fb44:  44 50 06 94   bl	_OUTLINED_FUNCTION_47
0x250f4fb48:  f5 03 00 aa   mov	x21, x0
0x250f4fb4c:  08 22 0c f0   adrp	x8, 0x269392000
0x250f4fb50:  01 39 43 f9   ldr	x1, [x8, #0x670] ; _kAPSampleBufferAttachmentKey_Timestamps
0x250f4fb54:  40 50 06 94   bl	_OUTLINED_FUNCTION_47
0x250f4fb58:  fa 03 00 aa   mov	x26, x0
0x250f4fb5c:  9f 5a 04 b9   str	wzr, [x20, #0x458]
0x250f4fb60:  9f f2 01 f9   str	xzr, [x20, #0x3e0]
0x250f4fb64:  75 01 00 b4   cbz	x21, loc_250f4fb90 ; ⤵ 0x2c
0x250f4fb68:  e0 03 15 aa   mov	x0, x21
0x250f4fb6c:  a1 c7 c6 95   bl	j__CFGetTypeID
0x250f4fb70:  fb 03 00 aa   mov	x27, x0
0x250f4fb74:  a7 e1 c6 95   bl	j__CFNumberGetTypeID
0x250f4fb78:  7f 03 00 eb   cmp	x27, x0
0x250f4fb7c:  a1 00 00 54   b.ne	loc_250f4fb90 ; ⤵ 0x14
0x250f4fb80:  82 82 0f 91   add	x2, x20, #0x3e0
0x250f4fb84:  e0 03 15 aa   mov	x0, x21
0x250f4fb88:  81 00 80 52   mov	w1, #0x4
0x250f4fb8c:  a1 ca c6 95   bl	j__CFNumberGetValue
0x250f4fb90:  ; loc_250f4fb90
0x250f4fb90:  7a 07 00 b4   cbz	x26, loc_250f4fc7c ; ⤵ 0xec
0x250f4fb94:  e0 03 1a aa   mov	x0, x26
0x250f4fb98:  96 c7 c6 95   bl	j__CFGetTypeID
0x250f4fb9c:  f5 03 00 aa   mov	x21, x0
0x250f4fba0:  6c e1 c6 95   bl	j__CFArrayGetTypeID
0x250f4fba4:  bf 02 00 eb   cmp	x21, x0
0x250f4fba8:  a1 06 00 54   b.ne	loc_250f4fc7c ; ⤵ 0xd4
0x250f4fbac:  e0 03 1a aa   mov	x0, x26
0x250f4fbb0:  54 ca c6 95   bl	j__CFArrayGetCount
0x250f4fbb4:  f5 03 00 aa   mov	x21, x0
0x250f4fbb8:  97 a2 0f 91   add	x23, x20, #0x3e8
0x250f4fbbc:  bf 06 00 71   cmp	w21, #0x1
0x250f4fbc0:  eb 04 00 54   b.lt	loc_250f4fc5c ; ⤵ 0x9c
0x250f4fbc4:  1b 00 80 d2   mov	x27, #0
0x250f4fbc8:  bf 3a 00 71   cmp	w21, #0xe
0x250f4fbcc:  c8 01 80 52   mov	w8, #0xe
0x250f4fbd0:  b8 32 88 1a   csel	w24, w21, w8, cc
0x250f4fbd4:  ; loc_250f4fbd4
0x250f4fbd4:  e0 03 1a aa   mov	x0, x26
0x250f4fbd8:  e1 03 1b aa   mov	x1, x27
0x250f4fbdc:  4d ca c6 95   bl	j__CFArrayGetValueAtIndex
0x250f4fbe0:  01 00 80 d2   mov	x1, #0
0x250f4fbe4:  4b f5 c6 95   bl	j__CFGetInt64
0x250f4fbe8:  e0 7a 3b f8   str	x0, [x23, x27, lsl #0x3]
0x250f4fbec:  7b 07 00 91   add	x27, x27, #0x1
0x250f4fbf0:  1f 03 1b eb   cmp	x24, x27
0x250f4fbf4:  01 ff ff 54   b.ne	loc_250f4fbd4 ; ⤴ -0x20
0x250f4fbf8:  09 00 80 d2   mov	x9, #0
0x250f4fbfc:  0a 07 00 d1   sub	x10, x24, #0x1
0x250f4fc00:  ; loc_250f4fc00
0x250f4fc00:  e8 7a 69 f8   ldr	x8, [x23, x9, lsl #0x3]
0x250f4fc04:  a8 00 00 b5   cbnz	x8, loc_250f4fc18 ; ⤵ 0x14
0x250f4fc08:  eb 03 0a aa   mov	x11, x10
0x250f4fc0c:  29 05 00 91   add	x9, x9, #0x1
0x250f4fc10:  4a 05 00 d1   sub	x10, x10, #0x1
0x250f4fc14:  6b ff ff b5   cbnz	x11, loc_250f4fc00 ; ⤴ -0x14
0x250f4fc18:  ; loc_250f4fc18
0x250f4fc18:  09 00 80 d2   mov	x9, #0
0x250f4fc1c:  ea 03 18 aa   mov	x10, x24
0x250f4fc20:  ; loc_250f4fc20
0x250f4fc20:  eb 7a 69 f8   ldr	x11, [x23, x9, lsl #0x3]
0x250f4fc24:  7f 01 00 f1   cmp	x11, #0
0x250f4fc28:  08 01 8b 9a   csel	x8, x8, x11, eq
0x250f4fc2c:  e8 7a 29 f8   str	x8, [x23, x9, lsl #0x3]
0x250f4fc30:  29 05 00 91   add	x9, x9, #0x1
0x250f4fc34:  4a 05 00 f1   subs	x10, x10, #0x1
0x250f4fc38:  41 ff ff 54   b.ne	loc_250f4fc20 ; ⤴ -0x18
0x250f4fc3c:  6d e9 c6 95   bl	j__mach_absolute_time
0x250f4fc40:  bf 36 00 71   cmp	w21, #0xd
0x250f4fc44:  a8 01 00 54   b.hi	loc_250f4fc78 ; ⤵ 0x34
0x250f4fc48:  e0 7a 38 f8   str	x0, [x23, x24, lsl #0x3]
0x250f4fc4c:  bf 36 00 71   cmp	w21, #0xd
0x250f4fc50:  e1 00 00 54   b.ne	loc_250f4fc6c ; ⤵ 0x1c
0x250f4fc54:  d8 01 80 52   mov	w24, #0xe
0x250f4fc58:  08 00 00 14   b	loc_250f4fc78 ; ⤵ 0x20
0x250f4fc5c:  ; loc_250f4fc5c
0x250f4fc5c:  65 e9 c6 95   bl	j__mach_absolute_time
0x250f4fc60:  e0 02 00 f9   str	x0, [x23]
0x250f4fc64:  28 00 80 52   mov	w8, #0x1
0x250f4fc68:  02 00 00 14   b	loc_250f4fc70 ; ⤵ 0x8
0x250f4fc6c:  ; loc_250f4fc6c
0x250f4fc6c:  08 07 00 11   add	w8, w24, #0x1
0x250f4fc70:  ; loc_250f4fc70
0x250f4fc70:  18 05 00 11   add	w24, w8, #0x1
0x250f4fc74:  e0 5a 28 f8   str	x0, [x23, w8, uxtw #0x3]
0x250f4fc78:  ; loc_250f4fc78
0x250f4fc78:  98 5a 04 b9   str	w24, [x20, #0x458]
0x250f4fc7c:  ; loc_250f4fc7c
0x250f4fc7c:  e8 43 02 91   add	x8, sp, #0x90
0x250f4fc80:  e0 03 19 aa   mov	x0, x25
0x250f4fc84:  87 fc c6 95   bl	j__CMSampleBufferGetOutputPresentationTimeStamp
0x250f4fc88:  e0 43 02 91   add	x0, sp, #0x90
0x250f4fc8c:  61 06 c7 95   bl	j__CMClockConvertHostTimeToSystemUnits
0x250f4fc90:  80 ee 01 f9   str	x0, [x20, #0x3d8]
0x250f4fc94:  88 2a 4f 39   ldrb	w8, [x20, #0x3ca]
0x250f4fc98:  c8 00 00 34   cbz	w8, loc_250f4fcb0 ; ⤵ 0x18
0x250f4fc9c:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4fca0:  21 00 0e 91   add	x1, x1, #0x380 ; "FVDFrameUserData"
0x250f4fca4:  ec 4f 06 94   bl	_OUTLINED_FUNCTION_47
0x250f4fca8:  fa 03 00 aa   mov	x26, x0
0x250f4fcac:  02 00 00 14   b	loc_250f4fcb4 ; ⤵ 0x8
0x250f4fcb0:  ; loc_250f4fcb0
0x250f4fcb0:  1a 00 80 d2   mov	x26, #0
0x250f4fcb4:  ; loc_250f4fcb4
0x250f4fcb4:  e9 a3 43 a9   ldp	x9, x8, [sp, #0x38]
0x250f4fcb8:  37 01 08 8b   add	x23, x9, x8
0x250f4fcbc:  88 2e 4f 39   ldrb	w8, [x20, #0x3cb]
0x250f4fcc0:  b8 8b 0b b0   adrp	x24, 0x2680c4000
0x250f4fcc4:  18 0f 43 f9   ldr	x24, [x24, #0x618] ; _ptr.1e0a0cf50 (CoreMedia/__DATA_CONST.__const)
0x250f4fcc8:  16 aa 0f 90   adrp	x22, 0x27048f000
0x250f4fccc:  13 aa 0f 90   adrp	x19, 0x27048f000
0x250f4fcd0:  c8 00 00 35   cbnz	w8, loc_250f4fce8 ; ⤵ 0x18
0x250f4fcd4:  c8 c2 46 f9   ldr	x8, [x22, #0xd80]
0x250f4fcd8:  1f 05 00 b1   cmn	x8, #0x1
0x250f4fcdc:  61 40 00 54   b.ne	loc_250f504e8 ; ⤵ 0x80c
0x250f4fce0:  ; loc_250f4fce0
0x250f4fce0:  68 e2 75 39   ldrb	w8, [x19, #0xd78]
0x250f4fce4:  88 2f 00 34   cbz	w8, loc_250f502d4 ; ⤵ 0x5f0
0x250f4fce8:  ; loc_250f4fce8
0x250f4fce8:  e0 03 19 aa   mov	x0, x25
0x250f4fcec:  01 00 80 52   mov	w1, #0
0x250f4fcf0:  74 f6 c6 95   bl	j__CMSampleBufferGetSampleAttachmentsArray
0x250f4fcf4:  f5 03 00 aa   mov	x21, x0
0x250f4fcf8:  a0 01 00 b4   cbz	x0, loc_250f4fd2c ; ⤵ 0x34
0x250f4fcfc:  e0 03 15 aa   mov	x0, x21
0x250f4fd00:  00 ca c6 95   bl	j__CFArrayGetCount
0x250f4fd04:  1f 04 00 f1   cmp	x0, #0x1
0x250f4fd08:  0b 01 00 54   b.lt	loc_250f4fd28 ; ⤵ 0x20
0x250f4fd0c:  e0 03 15 aa   mov	x0, x21
0x250f4fd10:  01 00 80 d2   mov	x1, #0
0x250f4fd14:  ff c9 c6 95   bl	j__CFArrayGetValueAtIndex
0x250f4fd18:  01 03 40 f9   ldr	x1, [x24]
0x250f4fd1c:  49 f7 c6 95   bl	j__FigCFDictionaryGetValue
0x250f4fd20:  f5 03 00 aa   mov	x21, x0
0x250f4fd24:  02 00 00 14   b	loc_250f4fd2c ; ⤵ 0x8
0x250f4fd28:  ; loc_250f4fd28
0x250f4fd28:  15 00 80 d2   mov	x21, #0
0x250f4fd2c:  ; loc_250f4fd2c
0x250f4fd2c:  c8 c2 46 f9   ldr	x8, [x22, #0xd80]
0x250f4fd30:  1f 05 00 b1   cmn	x8, #0x1
0x250f4fd34:  e1 3b 00 54   b.ne	loc_250f504b0 ; ⤵ 0x77c
0x250f4fd38:  ; loc_250f4fd38
0x250f4fd38:  68 e2 75 39   ldrb	w8, [x19, #0xd78]
0x250f4fd3c:  48 02 00 34   cbz	w8, loc_250f4fd84 ; ⤵ 0x48
0x250f4fd40:  9e 4f 06 94   bl	_OUTLINED_FUNCTION_13
0x250f4fd44:  0c 02 00 54   b.gt	loc_250f4fd84 ; ⤵ 0x40
0x250f4fd48:  1f 05 00 31   cmn	w8, #0x1
0x250f4fd4c:  61 00 00 54   b.ne	loc_250f4fd58 ; ⤵ 0xc
0x250f4fd50:  d3 c5 ff 97   bl	_OUTLINED_FUNCTION_10
0x250f4fd54:  80 01 00 34   cbz	w0, loc_250f4fd84 ; ⤵ 0x30
0x250f4fd58:  ; loc_250f4fd58
0x250f4fd58:  88 da 41 f9   ldr	x8, [x20, #0x3b0]
0x250f4fd5c:  49 5d 12 90   adrp	x9, 0x275af7000
0x250f4fd60:  29 81 38 91   add	x9, x9, #0xe20 ; "none"
0x250f4fd64:  bf 02 00 f1   cmp	x21, #0
0x250f4fd68:  29 01 95 9a   csel	x9, x9, x21, eq
0x250f4fd6c:  8f e2 ff 97   bl	_OUTLINED_FUNCTION_6
0x250f4fd70:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4fd74:  21 04 12 91   add	x1, x1, #0x481 ; "OSStatus sbpd_createUserDataBBuf(SBufProcessingData *, CMSampleBufferRef, CMBlockBufferRef *)"
0x250f4fd78:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4fd7c:  63 7c 13 91   add	x3, x3, #0x4df ; "%@ cryptor subsample aux data: %@\n"
0x250f4fd80:  95 9c ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4fd84:  ; loc_250f4fd84
0x250f4fd84:  88 2e 4f 39   ldrb	w8, [x20, #0x3cb]
0x250f4fd88:  1f 01 00 71   cmp	w8, #0
0x250f4fd8c:  f9 03 95 9a   csel	x25, xzr, x21, eq
0x250f4fd90:  ; loc_250f4fd90
0x250f4fd90:  48 03 19 aa   orr	x8, x26, x25
0x250f4fd94:  68 33 00 b4   cbz	x8, loc_250f50400 ; ⤵ 0x66c
0x250f4fd98:  7a b4 ff 97   bl	_OUTLINED_FUNCTION_28
0x250f4fd9c:  1b 01 40 f9   ldr	x27, [x8]
0x250f4fda0:  45 9b ff 97   bl	_OUTLINED_FUNCTION_5
0x250f4fda4:  e0 03 1b aa   mov	x0, x27
0x250f4fda8:  01 00 80 d2   mov	x1, #0
0x250f4fdac:  f1 c9 c6 95   bl	j__CFDictionaryCreateMutable
0x250f4fdb0:  f5 03 00 aa   mov	x21, x0
0x250f4fdb4:  c1 5d 12 f0   adrp	x1, 0x275b0a000
0x250f4fdb8:  21 00 0e 91   add	x1, x1, #0x380 ; "FVDFrameUserData"
0x250f4fdbc:  e2 03 1a aa   mov	x2, x26
0x250f4fdc0:  50 f7 c6 95   bl	j__FigCFDictionarySetValue
0x250f4fdc4:  01 03 40 f9   ldr	x1, [x24]
0x250f4fdc8:  e0 03 15 aa   mov	x0, x21
0x250f4fdcc:  e2 03 19 aa   mov	x2, x25
0x250f4fdd0:  4c f7 c6 95   bl	j__FigCFDictionarySetValue
0x250f4fdd4:  e0 03 1b aa   mov	x0, x27
0x250f4fdd8:  e1 03 15 aa   mov	x1, x21
0x250f4fddc:  02 19 80 52   mov	w2, #0xc8
0x250f4fde0:  03 00 80 d2   mov	x3, #0
0x250f4fde4:  04 00 80 d2   mov	x4, #0
0x250f4fde8:  36 f5 c6 95   bl	j__CFPropertyListCreateData
0x250f4fdec:  f9 03 00 aa   mov	x25, x0
0x250f4fdf0:  60 31 00 b4   cbz	x0, loc_250f5041c ; ⤵ 0x62c
0x250f4fdf4:  e4 23 02 91   add	x4, sp, #0x88
0x250f4fdf8:  e0 03 1b aa   mov	x0, x27
0x250f4fdfc:  e1 03 19 aa   mov	x1, x25
0x250f4fe00:  44 b1 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f4fe04:  bf 07 c7 95   bl	j__FigCreateBlockBufferWithCFDataNoCopy
0x250f4fe08:  fa 03 00 aa   mov	x26, x0
0x250f4fe0c:  e0 32 00 35   cbnz	w0, loc_250f50468 ; ⤵ 0x65c
0x250f4fe10:  ; loc_250f4fe10
0x250f4fe10:  75 00 00 b4   cbz	x21, loc_250f4fe1c ; ⤵ 0xc
0x250f4fe14:  e0 03 15 aa   mov	x0, x21
0x250f4fe18:  56 c7 c6 95   bl	j__CFRelease
0x250f4fe1c:  ; loc_250f4fe1c
0x250f4fe1c:  79 00 00 b4   cbz	x25, loc_250f4fe28 ; ⤵ 0xc
0x250f4fe20:  e0 03 19 aa   mov	x0, x25
0x250f4fe24:  53 c7 c6 95   bl	j__CFRelease
0x250f4fe28:  ; loc_250f4fe28
0x250f4fe28:  5a 2f 00 35   cbnz	w26, loc_250f50410 ; ⤵ 0x5e8
0x250f4fe2c:  e0 47 40 f9   ldr	x0, [sp, #0x88]
0x250f4fe30:  e0 00 00 b4   cbz	x0, loc_250f4fe4c ; ⤵ 0x1c
0x250f4fe34:  f7 f5 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f4fe38:  88 d6 41 f9   ldr	x8, [x20, #0x3a8]
0x250f4fe3c:  1b 01 00 8b   add	x27, x8, x0
0x250f4fe40:  77 03 17 8b   add	x23, x27, x23
0x250f4fe44:  9c 00 00 35   cbnz	w28, loc_250f4fe54 ; ⤵ 0x10
0x250f4fe48:  07 00 00 14   b	loc_250f4fe64 ; ⤵ 0x1c
0x250f4fe4c:  ; loc_250f4fe4c
0x250f4fe4c:  1b 00 80 52   mov	w27, #0
0x250f4fe50:  bc 00 00 34   cbz	w28, loc_250f4fe64 ; ⤵ 0x14
0x250f4fe54:  ; loc_250f4fe54
0x250f4fe54:  a1 63 02 d1   sub	x1, fp, #0x98
0x250f4fe58:  e0 3b 40 f9   ldr	x0, [sp, #0x70]
0x250f4fe5c:  8d 4b 06 94   bl	_sbpd_createConfigBBuf
0x250f4fe60:  c0 32 00 35   cbnz	w0, loc_250f504b8 ; ⤵ 0x658
0x250f4fe64:  ; loc_250f4fe64
0x250f4fe64:  20 00 80 52   mov	w0, #0x1
0x250f4fe68:  01 10 80 52   mov	w1, #0x80
0x250f4fe6c:  42 90 8e 52   mov	w2, #0x7482
0x250f4fe70:  e2 79 ae 72   movk	w2, #0x73cf, lsl #0x10
0x250f4fe74:  33 e0 c6 95   bl	j__malloc_type_calloc
0x250f4fe78:  f9 03 00 aa   mov	x25, x0
0x250f4fe7c:  17 00 00 b9   str	w23, [x0]
0x250f4fe80:  1f 10 00 39   strb	wzr, [x0, #0x4]
0x250f4fe84:  93 ee 41 f9   ldr	x19, [x20, #0x3d8]
0x250f4fe88:  96 de 41 f9   ldr	x22, [x20, #0x3b8]
0x250f4fe8c:  ff ff 09 a9   stp	xzr, xzr, [sp, #0x98]
0x250f4fe90:  ff 4b 00 f9   str	xzr, [sp, #0x90]
0x250f4fe94:  95 ea 41 f9   ldr	x21, [x20, #0x3d0]
0x250f4fe98:  e0 03 15 aa   mov	x0, x21
0x250f4fe9c:  f1 ee c6 95   bl	j__CMBaseObjectGetVTable
0x250f4fea0:  10 0c 41 f8   ldr	x16, [x0, #0x10]!
0x250f4fea4:  f1 03 00 aa   mov	x17, x0
0x250f4fea8:  31 a3 f1 f2   movk	x17, #0x8d19, lsl #0x30
0x250f4feac:  30 1a c1 da   autda	x16, x17
0x250f4feb0:  09 0e 41 f8   ldr	x9, [x16, #0x10]!
0x250f4feb4:  09 03 00 b4   cbz	x9, loc_250f4ff14 ; ⤵ 0x60
0x250f4feb8:  e8 03 10 aa   mov	x8, x16
0x250f4febc:  c8 45 fd f2   movk	x8, #0xea2e, lsl #0x30
0x250f4fec0:  f0 03 09 aa   mov	x16, x9
0x250f4fec4:  f1 03 08 aa   mov	x17, x8
0x250f4fec8:  30 12 c1 da   autia	x16, x17
0x250f4fecc:  f1 03 10 aa   mov	x17, x16
0x250f4fed0:  f1 43 c1 da   xpaci	x17
0x250f4fed4:  1f 02 11 eb   cmp	x16, x17
0x250f4fed8:  40 00 00 54   b.eq	loc_250f4fee0 ; ⤵ 0x8
0x250f4fedc:  00 8e 38 d4   brk	#0xc470
0x250f4fee0:  ; loc_250f4fee0
0x250f4fee0:  f0 23 c1 da   paciza	x16
0x250f4fee4:  90 01 00 b4   cbz	x16, loc_250f4ff14 ; ⤵ 0x30
0x250f4fee8:  c1 02 13 8b   add	x1, x22, x19
0x250f4feec:  e2 43 02 91   add	x2, sp, #0x90
0x250f4fef0:  e0 03 15 aa   mov	x0, x21
0x250f4fef4:  28 09 3f d7   blraa	x9, x8
0x250f4fef8:  f5 03 00 aa   mov	x21, x0
0x250f4fefc:  e0 00 00 35   cbnz	w0, loc_250f4ff18 ; ⤵ 0x1c
0x250f4ff00:  e0 27 c0 3d   ldr	q0, [sp, #0x90]
0x250f4ff04:  80 9e 80 3d   str	q0, [x20, #0x270]
0x250f4ff08:  e8 53 40 f9   ldr	x8, [sp, #0xa0]
0x250f4ff0c:  88 42 01 f9   str	x8, [x20, #0x280]
0x250f4ff10:  19 00 00 14   b	loc_250f4ff74 ; ⤵ 0x64
0x250f4ff14:  ; loc_250f4ff14
0x250f4ff14:  b5 3d 86 12   mov	w21, #0xffffce12
0x250f4ff18:  ; loc_250f4ff18
0x250f4ff18:  94 9c ff 97   bl	_OUTLINED_FUNCTION_14
0x250f4ff1c:  4c 02 00 54   b.gt	loc_250f4ff64 ; ⤵ 0x48
0x250f4ff20:  1f 05 00 31   cmn	w8, #0x1
0x250f4ff24:  c1 00 00 54   b.ne	loc_250f4ff3c ; ⤵ 0x18
0x250f4ff28:  73 9f ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4ff2c:  02 f7 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4ff30:  01 c9 00 11   add	w1, w8, #0x32
0x250f4ff34:  a3 c3 c6 95   bl	j___LogCategory_Initialize
0x250f4ff38:  60 01 00 34   cbz	w0, loc_250f4ff64 ; ⤵ 0x2c
0x250f4ff3c:  ; loc_250f4ff3c
0x250f4ff3c:  88 da 41 f9   ldr	x8, [x20, #0x3b0]
0x250f4ff40:  e8 57 00 a9   stp	x8, x21, [sp]
0x250f4ff44:  6c 9f ff 97   bl	_OUTLINED_FUNCTION_11
0x250f4ff48:  a1 15 00 f0   adrp	x1, 0x251206000
0x250f4ff4c:  21 4c 15 91   add	x1, x1, #0x553 ; "APSNetworkTime sbpd_convertFrameTimestampToNetworkTime(SBufProcessingData *, uint64_t)"
0x250f4ff50:  a3 15 00 f0   adrp	x3, 0x251206000
0x250f4ff54:  63 a8 16 91   add	x3, x3, #0x5aa ; "%@ ### frame timestamp conversion to network time failed: %#m\n"
0x250f4ff58:  f7 f6 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f4ff5c:  02 c9 00 11   add	w2, w8, #0x32
0x250f4ff60:  e8 c3 c6 95   bl	j__LogPrintF
0x250f4ff64:  ; loc_250f4ff64
0x250f4ff64:  80 9e c0 3d   ldr	q0, [x20, #0x270]
0x250f4ff68:  e0 27 80 3d   str	q0, [sp, #0x90]
0x250f4ff6c:  88 42 41 f9   ldr	x8, [x20, #0x280]
0x250f4ff70:  e8 53 00 f9   str	x8, [sp, #0xa0]
0x250f4ff74:  ; loc_250f4ff74
0x250f4ff74:  e8 27 49 a9   ldp	x8, x9, [sp, #0x90]
0x250f4ff78:  ea a3 40 b9   ldr	w10, [sp, #0xa0]
0x250f4ff7c:  49 81 c9 93   extr	x9, x10, x9, #0x20
0x250f4ff80:  29 07 00 f9   str	x9, [x25, #0x8]
0x250f4ff84:  28 17 00 f9   str	x8, [x25, #0x28]
0x250f4ff88:  88 f2 41 f9   ldr	x8, [x20, #0x3e0]
0x250f4ff8c:  28 0b 00 f9   str	x8, [x25, #0x10]
0x250f4ff90:  3f 6f 03 29   stp	wzr, w27, [x25, #0x18]
0x250f4ff94:  95 f6 41 f9   ldr	x21, [x20, #0x3e8]
0x250f4ff98:  ff ff 09 a9   stp	xzr, xzr, [sp, #0x98]
0x250f4ff9c:  ff 4b 00 f9   str	xzr, [sp, #0x90]
0x250f4ffa0:  f5 03 00 b4   cbz	x21, loc_250f5001c ; ⤵ 0x7c
0x250f4ffa4:  9a ea 41 f9   ldr	x26, [x20, #0x3d0]
0x250f4ffa8:  e0 03 1a aa   mov	x0, x26
0x250f4ffac:  ad ee c6 95   bl	j__CMBaseObjectGetVTable
0x250f4ffb0:  10 0c 41 f8   ldr	x16, [x0, #0x10]!
0x250f4ffb4:  f1 03 00 aa   mov	x17, x0
0x250f4ffb8:  31 a3 f1 f2   movk	x17, #0x8d19, lsl #0x30
0x250f4ffbc:  30 1a c1 da   autda	x16, x17
0x250f4ffc0:  09 0e 41 f8   ldr	x9, [x16, #0x10]!
0x250f4ffc4:  89 02 00 b4   cbz	x9, loc_250f50014 ; ⤵ 0x50
0x250f4ffc8:  e8 03 10 aa   mov	x8, x16
0x250f4ffcc:  c8 45 fd f2   movk	x8, #0xea2e, lsl #0x30
0x250f4ffd0:  f0 03 09 aa   mov	x16, x9
0x250f4ffd4:  f1 03 08 aa   mov	x17, x8
0x250f4ffd8:  30 12 c1 da   autia	x16, x17
0x250f4ffdc:  f1 03 10 aa   mov	x17, x16
0x250f4ffe0:  f1 43 c1 da   xpaci	x17
0x250f4ffe4:  1f 02 11 eb   cmp	x16, x17
0x250f4ffe8:  40 00 00 54   b.eq	loc_250f4fff0 ; ⤵ 0x8
0x250f4ffec:  00 8e 38 d4   brk	#0xc470
0x250f4fff0:  ; loc_250f4fff0
0x250f4fff0:  f0 23 c1 da   paciza	x16
0x250f4fff4:  50 01 00 b4   cbz	x16, loc_250f5001c ; ⤵ 0x28
0x250f4fff8:  e2 43 02 91   add	x2, sp, #0x90
0x250f4fffc:  e0 03 1a aa   mov	x0, x26
0x250f50000:  e1 03 15 aa   mov	x1, x21
0x250f50004:  28 09 3f d7   blraa	x9, x8
0x250f50008:  e9 4f 40 f9   ldr	x9, [sp, #0x98]
0x250f5000c:  e8 a3 40 b9   ldr	w8, [sp, #0xa0]
0x250f50010:  05 00 00 14   b	loc_250f50024 ; ⤵ 0x14
0x250f50014:  ; loc_250f50014
0x250f50014:  08 00 80 d2   mov	x8, #0
0x250f50018:  03 00 00 14   b	loc_250f50024 ; ⤵ 0xc
0x250f5001c:  ; loc_250f5001c
0x250f5001c:  08 00 80 d2   mov	x8, #0
0x250f50020:  09 00 80 d2   mov	x9, #0
0x250f50024:  ; loc_250f50024
0x250f50024:  08 81 c9 93   extr	x8, x8, x9, #0x20
0x250f50028:  28 13 00 f9   str	x8, [x25, #0x20]
0x250f5002c:  89 72 51 39   ldrb	w9, [x20, #0x45c]
0x250f50030:  3f 01 00 71   cmp	w9, #0
0x250f50034:  e9 0f 80 12   mov	w9, #0xffffff80
0x250f50038:  e9 03 89 1a   csel	w9, wzr, w9, eq
0x250f5003c:  29 17 00 39   strb	w9, [x25, #0x5]
0x250f50040:  8a 76 51 39   ldrb	w10, [x20, #0x45d]
0x250f50044:  6a 00 00 34   cbz	w10, loc_250f50050 ; ⤵ 0xc
0x250f50048:  29 01 1c 32   orr	w9, w9, #0x10
0x250f5004c:  29 17 00 39   strb	w9, [x25, #0x5]
0x250f50050:  ; loc_250f50050
0x250f50050:  28 2f 00 f9   str	x8, [x25, #0x58]
0x250f50054:  88 5a 44 b9   ldr	w8, [x20, #0x458]
0x250f50058:  48 06 00 34   cbz	w8, loc_250f50120 ; ⤵ 0xc8
0x250f5005c:  97 a2 0f 91   add	x23, x20, #0x3e8
0x250f50060:  36 83 01 91   add	x22, x25, #0x60
0x250f50064:  18 0c 80 52   mov	w24, #0x60
0x250f50068:  33 00 80 52   mov	w19, #0x1
0x250f5006c:  f5 a9 0f d0   adrp	x21, 0x27048e000
0x250f50070:  b5 c2 2c 91   add	x21, x21, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f50074:  ba 15 00 d0   adrp	x26, 0x251206000
0x250f50078:  5a a7 17 91   add	x26, x26, #0x5e9 ; "void sbpd_setFrameHeaderTimestamps(const SBufProcessingData *, APScreenProtocolHeader *)"
0x250f5007c:  bb 15 00 d0   adrp	x27, 0x251206000
0x250f50080:  7b 0b 19 91   add	x27, x27, #0x642 ; "%@ ### bad timestamp[%d] (> 65 seconds): %llu ms\n"
0x250f50084:  ; loc_250f50084
0x250f50084:  88 5a 44 b9   ldr	w8, [x20, #0x458]
0x250f50088:  7f 02 08 eb   cmp	x19, x8
0x250f5008c:  a2 04 00 54   b.hs	loc_250f50120 ; ⤵ 0x94
0x250f50090:  68 f2 7d d3   lsl	x8, x19, #0x3
0x250f50094:  e9 7a 73 f8   ldr	x9, [x23, x19, lsl #0x3]
0x250f50098:  08 21 00 d1   sub	x8, x8, #0x8
0x250f5009c:  e8 6a 68 f8   ldr	x8, [x23, x8]
0x250f500a0:  20 01 08 cb   sub	x0, x9, x8
0x250f500a4:  9b f9 c6 95   bl	j__UpTicksToMilliseconds
0x250f500a8:  fc 03 00 aa   mov	x28, x0
0x250f500ac:  1f 40 40 f1   cmp	x0, #0x10, lsl #0xc
0x250f500b0:  c3 02 00 54   b.lo	loc_250f50108 ; ⤵ 0x58
0x250f500b4:  61 9f ff 97   bl	_OUTLINED_FUNCTION_19
0x250f500b8:  1f f1 00 71   cmp	w8, #0x3c
0x250f500bc:  4c 02 00 54   b.gt	loc_250f50104 ; ⤵ 0x48
0x250f500c0:  1f 05 00 31   cmn	w8, #0x1
0x250f500c4:  c1 00 00 54   b.ne	loc_250f500dc ; ⤵ 0x18
0x250f500c8:  9b f6 ff 97   bl	_OUTLINED_FUNCTION_24
0x250f500cc:  01 51 00 11   add	w1, w8, #0x14
0x250f500d0:  e0 03 15 aa   mov	x0, x21
0x250f500d4:  3b c3 c6 95   bl	j___LogCategory_Initialize
0x250f500d8:  60 01 00 34   cbz	w0, loc_250f50104 ; ⤵ 0x2c
0x250f500dc:  ; loc_250f500dc
0x250f500dc:  88 da 41 f9   ldr	x8, [x20, #0x3b0]
0x250f500e0:  f3 f3 00 a9   stp	x19, x28, [sp, #0x8]
0x250f500e4:  09 05 80 52   mov	w9, #0x28
0x250f500e8:  09 40 a0 72   movk	w9, #0x200, lsl #0x10
0x250f500ec:  22 51 00 11   add	w2, w9, #0x14
0x250f500f0:  e8 03 00 f9   str	x8, [sp]
0x250f500f4:  e0 03 15 aa   mov	x0, x21
0x250f500f8:  e1 03 1a aa   mov	x1, x26
0x250f500fc:  e3 03 1b aa   mov	x3, x27
0x250f50100:  80 c3 c6 95   bl	j__LogPrintF
0x250f50104:  ; loc_250f50104
0x250f50104:  fc ff 9f 52   mov	w28, #0xffff
0x250f50108:  ; loc_250f50108
0x250f50108:  18 0b 00 91   add	x24, x24, #0x2
0x250f5010c:  dc 02 00 79   strh	w28, [x22]
0x250f50110:  73 06 00 91   add	x19, x19, #0x1
0x250f50114:  36 03 18 8b   add	x22, x25, x24
0x250f50118:  7f 46 00 f1   cmp	x19, #0x11
0x250f5011c:  41 fb ff 54   b.ne	loc_250f50084 ; ⤴ -0x98
0x250f50120:  ; loc_250f50120
0x250f50120:  5f 9d ff 97   bl	_OUTLINED_FUNCTION_13
0x250f50124:  88 8b 0b f0   adrp	x8, 0x2680c3000
0x250f50128:  08 d5 40 f9   ldr	x8, [x8, #0x1a8] ; _ptr.1e0184310 (CoreFoundation/__DATA_CONST.__const)
0x250f5012c:  03 01 40 f9   ldr	x3, [x8]
0x250f50130:  a8 83 02 d1   sub	x8, fp, #0xa0
0x250f50134:  e8 03 00 f9   str	x8, [sp]
0x250f50138:  e1 03 19 aa   mov	x1, x25
0x250f5013c:  9a b0 ff 97   bl	_OUTLINED_FUNCTION_42
0x250f50140:  e0 17 00 35   cbnz	w0, loc_250f5043c ; ⤵ 0x2fc
0x250f50144:  a1 03 56 f8   ldur	x1, [fp, #-0xa0]
0x250f50148:  e0 3b 40 f9   ldr	x0, [sp, #0x70]
0x250f5014c:  f3 1b 40 f9   ldr	x19, [sp, #0x30]
0x250f50150:  e2 03 13 aa   mov	x2, x19
0x250f50154:  d1 0f 00 94   bl	_sbpd_encryptBBuf
0x250f50158:  21 7a 00 94   bl	_OUTLINED_FUNCTION_27
0x250f5015c:  40 19 00 35   cbnz	w0, loc_250f50484 ; ⤵ 0x328
0x250f50160:  e2 47 40 f9   ldr	x2, [sp, #0x88]
0x250f50164:  f8 db 45 a9   ldp	x24, x22, [sp, #0x58]
0x250f50168:  a2 0b 00 b5   cbnz	x2, loc_250f502dc ; ⤵ 0x174
0x250f5016c:  ; loc_250f5016c
0x250f5016c:  b5 5f 76 a9   ldp	x21, x23, [fp, #-0xa0]
0x250f50170:  77 01 00 b4   cbz	x23, loc_250f5019c ; ⤵ 0x2c
0x250f50174:  e0 03 17 aa   mov	x0, x23
0x250f50178:  e1 03 15 aa   mov	x1, x21
0x250f5017c:  65 b0 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f50180:  04 00 80 52   mov	w4, #0
0x250f50184:  e7 04 c7 95   bl	j__CMBlockBufferAppendBufferReference
0x250f50188:  f5 03 17 aa   mov	x21, x23
0x250f5018c:  a0 00 00 34   cbz	w0, loc_250f501a0 ; ⤵ 0x14
0x250f50190:  e8 f3 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f50194:  62 94 80 52   mov	w2, #0x4a3
0x250f50198:  c3 00 00 14   b	loc_250f504a4 ; ⤵ 0x30c
0x250f5019c:  ; loc_250f5019c
0x250f5019c:  bf 57 36 a9   stp	xzr, x21, [fp, #-0xa0]
0x250f501a0:  ; loc_250f501a0
0x250f501a0:  e0 03 15 aa   mov	x0, x21
0x250f501a4:  e1 03 13 aa   mov	x1, x19
0x250f501a8:  5a b0 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f501ac:  04 00 80 52   mov	w4, #0
0x250f501b0:  dc 04 c7 95   bl	j__CMBlockBufferAppendBufferReference
0x250f501b4:  fa 03 00 aa   mov	x26, x0
0x250f501b8:  20 17 00 35   cbnz	w0, loc_250f5049c ; ⤵ 0x2e4
0x250f501bc:  88 72 44 b9   ldr	w8, [x20, #0x470]
0x250f501c0:  08 05 00 11   add	w8, w8, #0x1
0x250f501c4:  88 72 04 b9   str	w8, [x20, #0x470]
0x250f501c8:  e0 03 15 aa   mov	x0, x21
0x250f501cc:  11 f5 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f501d0:  e1 03 00 aa   mov	x1, x0
0x250f501d4:  00 96 80 52   mov	w0, #0x4b0
0x250f501d8:  c0 62 a5 72   movk	w0, #0x2b16, lsl #0x10
0x250f501dc:  4d b0 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f501e0:  04 00 80 d2   mov	x4, #0
0x250f501e4:  eb f9 c6 95   bl	j__kdebug_trace
0x250f501e8:  f5 3f 00 f9   str	x21, [sp, #0x78]
0x250f501ec:  bf 83 16 f8   stur	xzr, [fp, #-0x98]
0x250f501f0:  ; loc_250f501f0
0x250f501f0:  e0 47 40 f9   ldr	x0, [sp, #0x88]
0x250f501f4:  40 00 00 b4   cbz	x0, loc_250f501fc ; ⤵ 0x8
0x250f501f8:  5e c6 c6 95   bl	j__CFRelease
0x250f501fc:  ; loc_250f501fc
0x250f501fc:  a0 03 56 f8   ldur	x0, [fp, #-0xa0]
0x250f50200:  40 00 00 b4   cbz	x0, loc_250f50208 ; ⤵ 0x8
0x250f50204:  5b c6 c6 95   bl	j__CFRelease
0x250f50208:  ; loc_250f50208
0x250f50208:  a0 83 56 f8   ldur	x0, [fp, #-0x98]
0x250f5020c:  40 00 00 b4   cbz	x0, loc_250f50214 ; ⤵ 0x8
0x250f50210:  ; loc_250f50210
0x250f50210:  58 c6 c6 95   bl	j__CFRelease
0x250f50214:  ; loc_250f50214
0x250f50214:  7a 0a 00 35   cbnz	w26, loc_250f50360 ; ⤵ 0x14c
0x250f50218:  e0 3f 40 f9   ldr	x0, [sp, #0x78]
0x250f5021c:  fd f4 c6 95   bl	j__CMBlockBufferGetDataLength
0x250f50220:  f5 03 00 aa   mov	x21, x0
0x250f50224:  f3 e7 c6 95   bl	j__mach_absolute_time
0x250f50228:  3e f9 c6 95   bl	j__UpTicksToNanoseconds
0x250f5022c:  09 00 80 d2   mov	x9, #0
0x250f50230:  88 9a 43 b9   ldr	w8, [x20, #0x398]
0x250f50234:  0b 0d 40 92   and	x11, x8, #0xf
0x250f50238:  8a 36 42 f9   ldr	x10, [x20, #0x468]
0x250f5023c:  4a 01 15 8b   add	x10, x10, x21
0x250f50240:  8a 36 02 f9   str	x10, [x20, #0x468]
0x250f50244:  8a 62 0a 91   add	x10, x20, #0x298
0x250f50248:  4b 11 0b 8b   add	x11, x10, x11, lsl #0x4
0x250f5024c:  60 55 00 a9   stp	x0, x21, [x11]
0x250f50250:  eb 01 80 12   mov	w11, #0xfffffff0
0x250f50254:  ec 03 08 aa   mov	x12, x8
0x250f50258:  ; loc_250f50258
0x250f50258:  8d 0d 00 12   and	w13, w12, #0xf
0x250f5025c:  4d 51 2d 8b   add	x13, x10, w13, uxtw #0x4
0x250f50260:  ae 01 40 f9   ldr	x14, [x13]
0x250f50264:  0e 00 0e cb   sub	x14, x0, x14
0x250f50268:  df 01 1c eb   cmp	x14, x28
0x250f5026c:  c8 00 00 54   b.hi	loc_250f50284 ; ⤵ 0x18
0x250f50270:  ad 05 40 f9   ldr	x13, [x13, #0x8]
0x250f50274:  a9 01 09 8b   add	x9, x13, x9
0x250f50278:  8c 05 00 51   sub	w12, w12, #0x1
0x250f5027c:  6b 05 00 31   adds	w11, w11, #0x1
0x250f50280:  c3 fe ff 54   b.lo	loc_250f50258 ; ⤴ -0x28
0x250f50284:  ; loc_250f50284
0x250f50284:  29 fd 43 d3   lsr	x9, x9, #0x3
0x250f50288:  3f d5 30 f1   cmp	x9, #0xc35
0x250f5028c:  43 00 00 54   b.lo	loc_250f50294 ; ⤵ 0x8
0x250f50290:  80 4a 01 f9   str	x0, [x20, #0x290]
0x250f50294:  ; loc_250f50294
0x250f50294:  08 05 00 11   add	w8, w8, #0x1
0x250f50298:  88 9a 03 b9   str	w8, [x20, #0x398]
0x250f5029c:  1f 03 00 b9   str	wzr, [x24]
0x250f502a0:  76 00 00 b4   cbz	x22, loc_250f502ac ; ⤵ 0xc
0x250f502a4:  ; loc_250f502a4
0x250f502a4:  e0 03 16 aa   mov	x0, x22
0x250f502a8:  32 c6 c6 95   bl	j__CFRelease
0x250f502ac:  ; loc_250f502ac
0x250f502ac:  e0 3f 40 f9   ldr	x0, [sp, #0x78]
0x250f502b0:  fd 7b 55 a9   ldp	fp, lr, [sp, #0x150]
0x250f502b4:  f4 4f 54 a9   ldp	x20, x19, [sp, #0x140]
0x250f502b8:  f6 57 53 a9   ldp	x22, x21, [sp, #0x130]
0x250f502bc:  f8 5f 52 a9   ldp	x24, x23, [sp, #0x120]
0x250f502c0:  fa 67 51 a9   ldp	x26, x25, [sp, #0x110]
0x250f502c4:  fc 6f 50 a9   ldp	x28, x27, [sp, #0x100]
0x250f502c8:  e9 23 4f 6d   ldp	d9, d8, [sp, #0xf0]
0x250f502cc:  ff 83 05 91   add	sp, sp, #0x160
0x250f502d0:  ff 0f 5f d6   retab
0x250f502d4:  ; loc_250f502d4
0x250f502d4:  19 00 80 d2   mov	x25, #0
0x250f502d8:  ae fe ff 17   b	loc_250f4fd90 ; ⤴ -0x548
0x250f502dc:  ; loc_250f502dc
0x250f502dc:  a1 03 56 f8   ldur	x1, [fp, #-0xa0]
0x250f502e0:  e0 3b 40 f9   ldr	x0, [sp, #0x70]
0x250f502e4:  6d 0f 00 94   bl	_sbpd_encryptBBuf
0x250f502e8:  20 15 00 35   cbnz	w0, loc_250f5058c ; ⤵ 0x2a4
0x250f502ec:  e1 47 40 f9   ldr	x1, [sp, #0x88]
0x250f502f0:  e0 03 13 aa   mov	x0, x19
0x250f502f4:  07 b0 ff 97   bl	_OUTLINED_FUNCTION_100
0x250f502f8:  04 00 80 52   mov	w4, #0
0x250f502fc:  89 04 c7 95   bl	j__CMBlockBufferAppendBufferReference
0x250f50300:  60 f3 ff 34   cbz	w0, loc_250f5016c ; ⤴ -0x194
0x250f50304:  8b f3 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f50308:  c2 93 80 52   mov	w2, #0x49e
0x250f5030c:  66 00 00 14   b	loc_250f504a4 ; ⤵ 0x198
0x250f50310:  ; loc_250f50310
0x250f50310:  73 b7 ff 34   cbz	w19, loc_250f4f9fc ; ⤴ -0x914
0x250f50314:  bb fd ff 17   b	loc_250f4fa00 ; ⤴ -0x914
0x250f50318:  ; loc_250f50318
0x250f50318:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f5031c:  21 b0 0b 91   add	x1, x1, #0x2ec ; "sbufGetAttachmentInt32"
0x250f50320:  00 00 80 52   mov	w0, #0
0x250f50324:  02 3b 80 52   mov	w2, #0x1d8
0x250f50328:  03 00 80 52   mov	w3, #0
0x250f5032c:  0d f0 c6 95   bl	j__APSLogErrorAt
0x250f50330:  4b fc ff 17   b	loc_250f4f45c ; ⤴ -0xed4
0x250f50334:  ; loc_250f50334
0x250f50334:  a1 11 00 d0   adrp	x1, 0x251186000
0x250f50338:  21 00 38 91   add	x1, x1, #0xe00 ; "send data failed, stream disconnected"
0x250f5033c:  a2 15 00 d0   adrp	x2, 0x251206000
0x250f50340:  42 14 04 91   add	x2, x2, #0x105 ; "screenstream_dequeueAndProcessSampleBuffer"
0x250f50344:  20 94 9c 52   mov	w0, #0xe4a1
0x250f50348:  c0 ff bf 72   movk	w0, #0xfffe, lsl #0x10
0x250f5034c:  1d f1 c6 95   bl	j__APSSignalErrorAt
0x250f50350:  e0 fa ff 34   cbz	w0, loc_250f502ac ; ⤴ -0xa4
0x250f50354:  fa 03 00 aa   mov	x26, x0
0x250f50358:  16 00 80 d2   mov	x22, #0
0x250f5035c:  07 00 00 14   b	loc_250f50378 ; ⤵ 0x1c
0x250f50360:  ; loc_250f50360
0x250f50360:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f50364:  21 14 04 91   add	x1, x1, #0x105 ; "screenstream_dequeueAndProcessSampleBuffer"
0x250f50368:  e0 03 1a aa   mov	x0, x26
0x250f5036c:  c2 9e 80 52   mov	w2, #0x4f6
0x250f50370:  03 00 80 52   mov	w3, #0
0x250f50374:  fb ef c6 95   bl	j__APSLogErrorAt
0x250f50378:  ; loc_250f50378
0x250f50378:  28 00 80 52   mov	w8, #0x1
0x250f5037c:  88 d2 11 39   strb	w8, [x20, #0x474]
0x250f50380:  7a 9b ff 97   bl	_OUTLINED_FUNCTION_14
0x250f50384:  0c 02 00 54   b.gt	loc_250f503c4 ; ⤵ 0x40
0x250f50388:  13 4e 06 94   bl	_OUTLINED_FUNCTION_32
0x250f5038c:  a1 00 00 54   b.ne	loc_250f503a0 ; ⤵ 0x14
0x250f50390:  59 9e ff 97   bl	_OUTLINED_FUNCTION_11
0x250f50394:  61 ca 00 11   add	w1, w19, #0x32
0x250f50398:  8a c2 c6 95   bl	j___LogCategory_Initialize
0x250f5039c:  40 01 00 34   cbz	w0, loc_250f503c4 ; ⤵ 0x28
0x250f503a0:  ; loc_250f503a0
0x250f503a0:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f503a4:  e8 6b 00 a9   stp	x8, x26, [sp]
0x250f503a8:  53 9e ff 97   bl	_OUTLINED_FUNCTION_11
0x250f503ac:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f503b0:  21 e0 04 91   add	x1, x1, #0x138 ; "CMBlockBufferRef screenstream_dequeueAndProcessSampleBuffer(void *, OSStatus, OSType *)"
0x250f503b4:  a3 15 00 d0   adrp	x3, 0x251206000
0x250f503b8:  63 40 06 91   add	x3, x3, #0x190 ; "%@ ### sample buffer processing failed, error: %d\n"
0x250f503bc:  62 ca 00 11   add	w2, w19, #0x32
0x250f503c0:  d0 c2 c6 95   bl	j__LogPrintF
0x250f503c4:  ; loc_250f503c4
0x250f503c4:  c2 5d 12 d0   adrp	x2, 0x275b0a000
0x250f503c8:  42 00 0d 91   add	x2, x2, #0x340 ; "sbuf processing failed"
0x250f503cc:  e0 17 40 f9   ldr	x0, [sp, #0x28]
0x250f503d0:  e1 03 1a aa   mov	x1, x26
0x250f503d4:  8c 46 06 94   bl	_screenstream_handleFatalError
0x250f503d8:  76 f6 ff b5   cbnz	x22, loc_250f502a4 ; ⤴ -0x134
0x250f503dc:  b4 ff ff 17   b	loc_250f502ac ; ⤴ -0x130
0x250f503e0:  ; loc_250f503e0
0x250f503e0:  54 f3 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f503e4:  c2 8a 80 52   mov	w2, #0x456
0x250f503e8:  2f 00 00 14   b	loc_250f504a4 ; ⤵ 0xbc
0x250f503ec:  ; loc_250f503ec
0x250f503ec:  51 f3 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f503f0:  02 8c 80 52   mov	w2, #0x460
0x250f503f4:  03 00 80 52   mov	w3, #0
0x250f503f8:  da ef c6 95   bl	j__APSLogErrorAt
0x250f503fc:  34 00 00 14   b	loc_250f504cc ; ⤵ 0xd0
0x250f50400:  ; loc_250f50400
0x250f50400:  1b 00 80 52   mov	w27, #0
0x250f50404:  ff 47 00 f9   str	xzr, [sp, #0x88]
0x250f50408:  7c d2 ff 35   cbnz	w28, loc_250f4fe54 ; ⤴ -0x5b4
0x250f5040c:  96 fe ff 17   b	loc_250f4fe64 ; ⤴ -0x5a8
0x250f50410:  ; loc_250f50410
0x250f50410:  f5 4d 06 94   bl	_OUTLINED_FUNCTION_34
0x250f50414:  62 90 80 52   mov	w2, #0x483
0x250f50418:  2a 00 00 14   b	loc_250f504c0 ; ⤵ 0xa8
0x250f5041c:  ; loc_250f5041c
0x250f5041c:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f50420:  21 08 14 91   add	x1, x1, #0x502 ; "sbpd_createUserDataBBuf"
0x250f50424:  62 87 80 52   mov	w2, #0x43b
0x250f50428:  03 00 80 52   mov	w3, #0
0x250f5042c:  cd ef c6 95   bl	j__APSLogErrorAt
0x250f50430:  1a 00 80 52   mov	w26, #0
0x250f50434:  ff 47 00 f9   str	xzr, [sp, #0x88]
0x250f50438:  76 fe ff 17   b	loc_250f4fe10 ; ⤴ -0x628
0x250f5043c:  ; loc_250f5043c
0x250f5043c:  fa 03 00 aa   mov	x26, x0
0x250f50440:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f50444:  21 e0 14 91   add	x1, x1, #0x538 ; "sbpd_createFrameHeaderBBuf"
0x250f50448:  22 73 80 52   mov	w2, #0x399
0x250f5044c:  03 00 80 52   mov	w3, #0
0x250f50450:  c4 ef c6 95   bl	j__APSLogErrorAt
0x250f50454:  e0 03 19 aa   mov	x0, x25
0x250f50458:  2a c0 c6 95   bl	j__free
0x250f5045c:  e2 4d 06 94   bl	_OUTLINED_FUNCTION_34
0x250f50460:  22 92 80 52   mov	w2, #0x491
0x250f50464:  17 00 00 14   b	loc_250f504c0 ; ⤵ 0x5c
0x250f50468:  ; loc_250f50468
0x250f50468:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f5046c:  21 08 14 91   add	x1, x1, #0x502 ; "sbpd_createUserDataBBuf"
0x250f50470:  e0 03 1a aa   mov	x0, x26
0x250f50474:  c2 87 80 52   mov	w2, #0x43e
0x250f50478:  03 00 80 52   mov	w3, #0
0x250f5047c:  b9 ef c6 95   bl	j__APSLogErrorAt
0x250f50480:  64 fe ff 17   b	loc_250f4fe10 ; ⤴ -0x670
0x250f50484:  ; loc_250f50484
0x250f50484:  2b f3 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f50488:  c2 92 80 52   mov	w2, #0x496
0x250f5048c:  03 00 80 52   mov	w3, #0
0x250f50490:  b4 ef c6 95   bl	j__APSLogErrorAt
0x250f50494:  f8 db 45 a9   ldp	x24, x22, [sp, #0x58]
0x250f50498:  56 ff ff 17   b	loc_250f501f0 ; ⤴ -0x2a8
0x250f5049c:  ; loc_250f5049c
0x250f5049c:  d2 4d 06 94   bl	_OUTLINED_FUNCTION_34
0x250f504a0:  62 95 80 52   mov	w2, #0x4ab
0x250f504a4:  ; loc_250f504a4
0x250f504a4:  03 00 80 52   mov	w3, #0
0x250f504a8:  ae ef c6 95   bl	j__APSLogErrorAt
0x250f504ac:  51 ff ff 17   b	loc_250f501f0 ; ⤴ -0x2bc
0x250f504b0:  ; loc_250f504b0
0x250f504b0:  17 d3 ff 97   bl	_OUTLINED_FUNCTION_56
0x250f504b4:  21 fe ff 17   b	loc_250f4fd38 ; ⤴ -0x77c
0x250f504b8:  ; loc_250f504b8
0x250f504b8:  1e f3 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f504bc:  a2 91 80 52   mov	w2, #0x48d
0x250f504c0:  ; loc_250f504c0
0x250f504c0:  03 00 80 52   mov	w3, #0
0x250f504c4:  a7 ef c6 95   bl	j__APSLogErrorAt
0x250f504c8:  f8 db 45 a9   ldp	x24, x22, [sp, #0x58]
0x250f504cc:  ; loc_250f504cc
0x250f504cc:  44 79 00 94   bl	_OUTLINED_FUNCTION_27
0x250f504d0:  48 ff ff 17   b	loc_250f501f0 ; ⤴ -0x2e0
0x250f504d4:  ; loc_250f504d4
0x250f504d4:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f504d8:  21 14 1a 91   add	x1, x1, #0x685 ; "sbpd_createStatsBBuf"
0x250f504dc:  00 00 80 52   mov	w0, #0
0x250f504e0:  02 58 80 52   mov	w2, #0x2c0
0x250f504e4:  0d 00 00 14   b	loc_250f50518 ; ⤵ 0x34
0x250f504e8:  ; loc_250f504e8
0x250f504e8:  09 d3 ff 97   bl	_OUTLINED_FUNCTION_56
0x250f504ec:  fd fd ff 17   b	loc_250f4fce0 ; ⤴ -0x80c
0x250f504f0:  ; loc_250f504f0
0x250f504f0:  af c4 ff 97   bl	_OUTLINED_FUNCTION_17
0x250f504f4:  62 5e 80 52   mov	w2, #0x2f3
0x250f504f8:  03 00 80 52   mov	w3, #0
0x250f504fc:  99 ef c6 95   bl	j__APSLogErrorAt
0x250f50500:  e0 03 15 aa   mov	x0, x21
0x250f50504:  ff bf c6 95   bl	j__free
0x250f50508:  d0 fc ff 17   b	loc_250f4f848 ; ⤴ -0xcc0
0x250f5050c:  ; loc_250f5050c
0x250f5050c:  a1 15 00 d0   adrp	x1, 0x251206000
0x250f50510:  21 14 1a 91   add	x1, x1, #0x685 ; "sbpd_createStatsBBuf"
0x250f50514:  c2 59 80 52   mov	w2, #0x2ce
0x250f50518:  ; loc_250f50518
0x250f50518:  03 00 80 52   mov	w3, #0
0x250f5051c:  91 ef c6 95   bl	j__APSLogErrorAt
0x250f50520:  fa 2e 88 12   mov	w26, #0xffffbe88
0x250f50524:  2e 79 00 94   bl	_OUTLINED_FUNCTION_27
0x250f50528:  cb fc ff 17   b	loc_250f4f854 ; ⤴ -0xcd4
0x250f5052c:  ; loc_250f5052c
0x250f5052c:  a0 c4 ff 97   bl	_OUTLINED_FUNCTION_17
0x250f50530:  02 5a 80 52   mov	w2, #0x2d0
0x250f50534:  03 00 80 52   mov	w3, #0
0x250f50538:  8a ef c6 95   bl	j__APSLogErrorAt
0x250f5053c:  28 79 00 94   bl	_OUTLINED_FUNCTION_27
0x250f50540:  c3 fc ff 17   b	loc_250f4f84c ; ⤴ -0xcf4
0x250f50544:  ; loc_250f50544
0x250f50544:  9a c4 ff 97   bl	_OUTLINED_FUNCTION_17
0x250f50548:  02 5f 80 52   mov	w2, #0x2f8
0x250f5054c:  0d 00 00 14   b	loc_250f50580 ; ⤵ 0x34
0x250f50550:  ; loc_250f50550
0x250f50550:  97 c4 ff 97   bl	_OUTLINED_FUNCTION_17
0x250f50554:  a2 5f 80 52   mov	w2, #0x2fd
0x250f50558:  0a 00 00 14   b	loc_250f50580 ; ⤵ 0x28
0x250f5055c:  ; loc_250f5055c
0x250f5055c:  88 76 51 39   ldrb	w8, [x20, #0x45d]
0x250f50560:  50 fd ff 17   b	loc_250f4faa0 ; ⤴ -0xac0
0x250f50564:  ; loc_250f50564
0x250f50564:  e0 a9 0f f0   adrp	x0, 0x27048f000
0x250f50568:  00 20 36 91   add	x0, x0, #0xd88 ; data_27048fd88 (AirPlaySender/__DATA_DIRTY.__bss)
0x250f5056c:  a1 43 02 d1   sub	x1, fp, #0x90
0x250f50570:  24 c2 c6 95   bl	j__dispatch_once
0x250f50574:  70 fc ff 17   b	loc_250f4f734 ; ⤴ -0xe40
0x250f50578:  ; loc_250f50578
0x250f50578:  8d c4 ff 97   bl	_OUTLINED_FUNCTION_17
0x250f5057c:  82 5c 80 52   mov	w2, #0x2e4
0x250f50580:  ; loc_250f50580
0x250f50580:  03 00 80 52   mov	w3, #0
0x250f50584:  77 ef c6 95   bl	j__APSLogErrorAt
0x250f50588:  b0 fc ff 17   b	loc_250f4f848 ; ⤴ -0xd40
0x250f5058c:  ; loc_250f5058c
0x250f5058c:  e9 f2 ff 97   bl	_OUTLINED_FUNCTION_12
0x250f50590:  82 93 80 52   mov	w2, #0x49c
0x250f50594:  c4 ff ff 17   b	loc_250f504a4 ; ⤴ -0xf0

