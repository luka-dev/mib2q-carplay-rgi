
_vdsink_Start:
0x25104f33c:  7f 23 03 d5   pacibsp
0x25104f340:  ff 83 02 d1   sub	sp, sp, #0xa0
0x25104f344:  f8 5f 06 a9   stp	x24, x23, [sp, #0x60]
0x25104f348:  f6 57 07 a9   stp	x22, x21, [sp, #0x70]
0x25104f34c:  f4 4f 08 a9   stp	x20, x19, [sp, #0x80]
0x25104f350:  fd 7b 09 a9   stp	fp, lr, [sp, #0x90]
0x25104f354:  fd 43 02 91   add	fp, sp, #0x90
0x25104f358:  f3 03 02 aa   mov	x19, x2
0x25104f35c:  f6 03 01 aa   mov	x22, x1
0x25104f360:  f5 03 00 aa   mov	x21, x0
0x25104f364:  97 f8 c2 95   bl	j__CMBaseObjectGetDerivedStorage
0x25104f368:  f4 03 00 aa   mov	x20, x0
0x25104f36c:  bf c3 1c b8   stur	wzr, [fp, #-0x34]
0x25104f370:  37 b8 0e b0   adrp	x23, 0x26e754000
0x25104f374:  e8 3a 4f b9   ldr	w8, [x23, #0xf38] ; _gLogCategory_APVirtualDisplaySink
0x25104f378:  1f c9 00 71   cmp	w8, #0x32
0x25104f37c:  2c 01 00 54   b.gt	loc_25104f3a0 ; ⤵ 0x24
0x25104f380:  1f 05 00 31   cmn	w8, #0x1
0x25104f384:  01 0c 00 54   b.ne	loc_25104f504 ; ⤵ 0x180
0x25104f388:  20 b8 0e b0   adrp	x0, 0x26e754000
0x25104f38c:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x25104f390:  41 06 80 52   mov	w1, #0x32
0x25104f394:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x25104f398:  8a c6 c2 95   bl	j___LogCategory_Initialize
0x25104f39c:  40 0b 00 35   cbnz	w0, loc_25104f504 ; ⤵ 0x168
0x25104f3a0:  ; loc_25104f3a0
0x25104f3a0:  e8 13 01 91   add	x8, sp, #0x44
0x25104f3a4:  60 08 80 52   mov	w0, #0x43
0x25104f3a8:  01 7d 80 52   mov	w1, #0x3e8
0x25104f3ac:  d9 f8 c2 95   bl	j__CMTimeMake
0x25104f3b0:  e0 43 c4 3c   ldur	q0, [sp, #0x44]
0x25104f3b4:  80 0a 80 3d   str	q0, [x20, #0x20]
0x25104f3b8:  e8 43 45 f8   ldur	x8, [sp, #0x54]
0x25104f3bc:  88 1a 00 f9   str	x8, [x20, #0x30]
0x25104f3c0:  81 55 12 b0   adrp	x1, 0x275b00000
0x25104f3c4:  21 00 2a 91   add	x1, x1, #0xa80 ; "SinkMaxFrameQueueDuration"
0x25104f3c8:  a2 d3 00 d1   sub	x2, fp, #0x34
0x25104f3cc:  e0 03 16 aa   mov	x0, x22
0x25104f3d0:  90 f9 c2 95   bl	j__FigCFDictionaryGetInt32IfPresent
0x25104f3d4:  a1 c3 5c b8   ldur	w1, [fp, #-0x34]
0x25104f3d8:  80 55 12 b0   adrp	x0, 0x275b00000
0x25104f3dc:  00 00 2d 91   add	x0, x0, #0xb40 ; "screenMaxQueueDurationMs"
0x25104f3e0:  dc f4 c2 95   bl	j__APSSettingsGetIntWithDefault
0x25104f3e4:  a0 c3 1c b8   stur	w0, [fp, #-0x34]
0x25104f3e8:  1f 04 00 71   cmp	w0, #0x1
0x25104f3ec:  0b 05 00 54   b.lt	loc_25104f48c ; ⤵ 0xa0
0x25104f3f0:  e8 3a 4f b9   ldr	w8, [x23, #0xf38]
0x25104f3f4:  1f c9 00 71   cmp	w8, #0x32
0x25104f3f8:  ac 03 00 54   b.gt	loc_25104f46c ; ⤵ 0x74
0x25104f3fc:  1f 05 00 31   cmn	w8, #0x1
0x25104f400:  60 02 00 54   b.eq	loc_25104f44c ; ⤵ 0x4c
0x25104f404:  ; loc_25104f404
0x25104f404:  88 06 40 f9   ldr	x8, [x20, #0x8]
0x25104f408:  29 55 12 f0   adrp	x9, 0x275af6000
0x25104f40c:  29 81 18 91   add	x9, x9, #0x620
0x25104f410:  1f 01 00 f1   cmp	x8, #0
0x25104f414:  28 01 88 9a   csel	x8, x9, x8, eq
0x25104f418:  f5 83 00 a9   stp	x21, x0, [sp, #0x8]
0x25104f41c:  e8 03 00 f9   str	x8, [sp]
0x25104f420:  20 b8 0e b0   adrp	x0, 0x26e754000
0x25104f424:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x25104f428:  a1 0a 00 d0   adrp	x1, 0x2511a5000
0x25104f42c:  21 84 3b 91   add	x1, x1, #0xee1 ; "OSStatus vdsink_Start(FigVirtualDisplaySinkRef, CFDictionaryRef, FigVirtualDisplaySinkStartBlock)"
0x25104f430:  63 0b 00 90   adrp	x3, 0x2511bb000
0x25104f434:  63 d0 3a 91   add	x3, x3, #0xeb4 ; "%@ virtual display sink %{ptr}: setting max queue duration to %d ms\n"
0x25104f438:  42 06 80 52   mov	w2, #0x32
0x25104f43c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x25104f440:  b0 c6 c2 95   bl	j__LogPrintF
0x25104f444:  a0 c3 5c b8   ldur	w0, [fp, #-0x34]
0x25104f448:  09 00 00 14   b	loc_25104f46c ; ⤵ 0x24
0x25104f44c:  ; loc_25104f44c
0x25104f44c:  20 b8 0e b0   adrp	x0, 0x26e754000
0x25104f450:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x25104f454:  41 06 80 52   mov	w1, #0x32
0x25104f458:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x25104f45c:  59 c6 c2 95   bl	j___LogCategory_Initialize
0x25104f460:  e8 03 00 aa   mov	x8, x0
0x25104f464:  a0 c3 5c b8   ldur	w0, [fp, #-0x34]
0x25104f468:  e8 fc ff 35   cbnz	w8, loc_25104f404 ; ⤴ -0x64
0x25104f46c:  ; loc_25104f46c
0x25104f46c:  00 7c 40 93   sxtw	x0, w0
0x25104f470:  e8 13 01 91   add	x8, sp, #0x44
0x25104f474:  01 7d 80 52   mov	w1, #0x3e8
0x25104f478:  a6 f8 c2 95   bl	j__CMTimeMake
0x25104f47c:  e0 43 c4 3c   ldur	q0, [sp, #0x44]
0x25104f480:  80 02 82 3c   stur	q0, [x20, #0x20]
0x25104f484:  e8 43 45 f8   ldur	x8, [sp, #0x54]
0x25104f488:  88 02 03 f8   stur	x8, [x20, #0x30]
0x25104f48c:  ; loc_25104f48c
0x25104f48c:  f3 02 00 b4   cbz	x19, loc_25104f4e8 ; ⤵ 0x5c
0x25104f490:  80 0e 40 f9   ldr	x0, [x20, #0x18]
0x25104f494:  e8 63 00 91   add	x8, sp, #0x18
0x25104f498:  b0 8a 0b b0   adrp	x16, 0x2681a4000
0x25104f49c:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x25104f4a0:  f1 03 08 aa   mov	x17, x8
0x25104f4a4:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x25104f4a8:  30 0a c1 da   pacda	x16, x17
0x25104f4ac:  f0 0f 00 f9   str	x16, [sp, #0x18]
0x25104f4b0:  c9 0d 00 d0   adrp	x9, 0x251209000
0x25104f4b4:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x25104f4b8:  e0 13 00 fd   str	d0, [sp, #0x20]
0x25104f4bc:  08 41 00 91   add	x8, x8, #0x10
0x25104f4c0:  09 1a 0c f0   adrp	x9, 0x269392000
0x25104f4c4:  29 e1 1a 91   add	x9, x9, #0x6b8 ; data_2693926b8 (AirPlaySender/__DATA_CONST.__const)
0x25104f4c8:  10 00 00 90   adrp	x16, 0x25104f000
0x25104f4cc:  10 f2 2b 91   add	x16, x16, #0xafc ; ___vdsink_Start_block_invoke
0x25104f4d0:  10 01 c1 da   pacia	x16, x8
0x25104f4d4:  f0 a7 02 a9   stp	x16, x9, [sp, #0x28]
0x25104f4d8:  f3 1f 00 f9   str	x19, [sp, #0x38]
0x25104f4dc:  ff 43 00 b9   str	wzr, [sp, #0x40]
0x25104f4e0:  e1 63 00 91   add	x1, sp, #0x18
0x25104f4e4:  e7 c5 c2 95   bl	j__dispatch_async
0x25104f4e8:  ; loc_25104f4e8
0x25104f4e8:  00 00 80 52   mov	w0, #0
0x25104f4ec:  fd 7b 49 a9   ldp	fp, lr, [sp, #0x90]
0x25104f4f0:  f4 4f 48 a9   ldp	x20, x19, [sp, #0x80]
0x25104f4f4:  f6 57 47 a9   ldp	x22, x21, [sp, #0x70]
0x25104f4f8:  f8 5f 46 a9   ldp	x24, x23, [sp, #0x60]
0x25104f4fc:  ff 83 02 91   add	sp, sp, #0xa0
0x25104f500:  ff 0f 5f d6   retab
0x25104f504:  ; loc_25104f504
0x25104f504:  e0 03 14 aa   mov	x0, x20
0x25104f508:  e1 03 15 aa   mov	x1, x21
0x25104f50c:  4c 65 03 94   bl	_vdsink_Start.cold.1
0x25104f510:  a4 ff ff 17   b	loc_25104f3a0 ; ⤴ -0x170

