
-[APRKMediaPlayer _SSLPropertiesforTLSInfo:]
0x250eed348:  7f 23 03 d5   pacibsp
0x250eed34c:  ff c3 01 d1   sub	sp, sp, #0x70
0x250eed350:  f6 57 04 a9   stp	x22, x21, [sp, #0x40]
0x250eed354:  f4 4f 05 a9   stp	x20, x19, [sp, #0x50]
0x250eed358:  fd 7b 06 a9   stp	fp, lr, [sp, #0x60]
0x250eed35c:  fd 83 01 91   add	fp, sp, #0x60
0x250eed360:  a8 95 0b f0   adrp	x8, 0x2681a4000
0x250eed364:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.___stack_chk_guard
0x250eed368:  08 01 40 f9   ldr	x8, [x8]
0x250eed36c:  a8 83 1d f8   stur	x8, [fp, #-0x28]
0x250eed370:  82 06 00 b4   cbz	x2, loc_250eed440 ; ⤵ 0xd0
0x250eed374:  bf 4c c8 95   bl	_objc_retain_x2_stub
0x250eed378:  f5 03 00 aa   mov	x21, x0
0x250eed37c:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eed380:  42 a0 34 91   add	x2, x2, #0xd28 ; _ptr.__NSCFConstantString
0x250eed384:  9b 51 c4 95   bl	0x2580019f0
0x250eed388:  46 4e c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed38c:  f3 03 00 aa   mov	x19, x0
0x250eed390:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eed394:  42 20 35 91   add	x2, x2, #0xd48 ; _ptr.__NSCFConstantString
0x250eed398:  e0 03 15 aa   mov	x0, x21
0x250eed39c:  95 51 c4 95   bl	0x2580019f0
0x250eed3a0:  40 4e c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed3a4:  f4 03 00 aa   mov	x20, x0
0x250eed3a8:  82 4c c8 95   bl	_objc_release_x21_stub
0x250eed3ac:  15 00 80 d2   mov	x21, #0
0x250eed3b0:  33 04 00 b4   cbz	x19, loc_250eed434 ; ⤵ 0x84
0x250eed3b4:  14 04 00 b4   cbz	x20, loc_250eed434 ; ⤵ 0x80
0x250eed3b8:  c8 15 80 52   mov	w8, #0xae
0x250eed3bc:  e8 0f 00 79   strh	w8, [sp, #0x6]
0x250eed3c0:  a8 8e 0b b0   adrp	x8, 0x2680c2000
0x250eed3c4:  00 51 47 f9   ldr	x0, [x8, #0xea0] ; _ptr.NSData
0x250eed3c8:  e2 1b 00 91   add	x2, sp, #0x6
0x250eed3cc:  43 00 80 52   mov	w3, #0x2
0x250eed3d0:  74 52 c4 95   bl	0x258001da0
0x250eed3d4:  33 4e c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed3d8:  f6 03 00 aa   mov	x22, x0
0x250eed3dc:  88 8e 0b b0   adrp	x8, 0x2680be000
0x250eed3e0:  08 09 47 f9   ldr	x8, [x8, #0xe10] ; _ptr._kCFStreamSSLPSKIdentity
0x250eed3e4:  08 01 40 f9   ldr	x8, [x8]
0x250eed3e8:  89 8e 0b b0   adrp	x9, 0x2680be000
0x250eed3ec:  29 0d 47 f9   ldr	x9, [x9, #0xe18] ; _ptr._kCFStreamSSLPSKSharedSecret
0x250eed3f0:  29 01 40 f9   ldr	x9, [x9]
0x250eed3f4:  e8 a7 00 a9   stp	x8, x9, [sp, #0x8]
0x250eed3f8:  88 8e 0b b0   adrp	x8, 0x2680be000
0x250eed3fc:  08 fd 46 f9   ldr	x8, [x8, #0xdf8] ; _ptr._kCFStreamSSLEnabledCiphers
0x250eed400:  f3 53 02 a9   stp	x19, x20, [sp, #0x20]
0x250eed404:  08 01 40 f9   ldr	x8, [x8]
0x250eed408:  e8 0f 00 f9   str	x8, [sp, #0x18]
0x250eed40c:  e0 1b 00 f9   str	x0, [sp, #0x30]
0x250eed410:  a8 8e 0b b0   adrp	x8, 0x2680c2000
0x250eed414:  00 6d 47 f9   ldr	x0, [x8, #0xed8] ; _ptr.NSDictionary
0x250eed418:  e2 83 00 91   add	x2, sp, #0x20
0x250eed41c:  e3 23 00 91   add	x3, sp, #0x8
0x250eed420:  64 00 80 52   mov	w4, #0x3
0x250eed424:  7f 52 c4 95   bl	0x258001e20
0x250eed428:  1e 4e c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed42c:  f5 03 00 aa   mov	x21, x0
0x250eed430:  64 4c c8 95   bl	_objc_release_x22_stub
0x250eed434:  ; loc_250eed434
0x250eed434:  5b 4c c8 95   bl	_objc_release_x20_stub
0x250eed438:  56 4c c8 95   bl	_objc_release_x19_stub
0x250eed43c:  02 00 00 14   b	loc_250eed444 ; ⤵ 0x8
0x250eed440:  ; loc_250eed440
0x250eed440:  15 00 80 d2   mov	x21, #0
0x250eed444:  ; loc_250eed444
0x250eed444:  a8 83 5d f8   ldur	x8, [fp, #-0x28]
0x250eed448:  a9 95 0b f0   adrp	x9, 0x2681a4000
0x250eed44c:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.___stack_chk_guard
0x250eed450:  29 01 40 f9   ldr	x9, [x9]
0x250eed454:  3f 01 08 eb   cmp	x9, x8
0x250eed458:  61 01 00 54   b.ne	loc_250eed484 ; ⤵ 0x2c
0x250eed45c:  e0 03 15 aa   mov	x0, x21
0x250eed460:  fd 7b 46 a9   ldp	fp, lr, [sp, #0x60]
0x250eed464:  f4 4f 45 a9   ldp	x20, x19, [sp, #0x50]
0x250eed468:  f6 57 44 a9   ldp	x22, x21, [sp, #0x40]
0x250eed46c:  ff c3 01 91   add	sp, sp, #0x70
0x250eed470:  ff 23 03 d5   autibsp
0x250eed474:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250eed478:  50 00 f0 b6   tbz	x16, #0x3e, loc_250eed480 ; ⤵ 0x8
0x250eed47c:  20 8e 38 d4   brk	#0xc471
0x250eed480:  ; loc_250eed480
0x250eed480:  04 4e c8 15   b	_objc_autoreleaseReturnValue_stub
0x250eed484:  ; loc_250eed484
0x250eed484:  0b 4c c8 95   bl	___stack_chk_fail_stub

