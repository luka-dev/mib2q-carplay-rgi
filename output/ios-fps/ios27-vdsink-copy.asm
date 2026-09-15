
_vdsink_CopyProperty:
0x250f4e428:  7f 23 03 d5   pacibsp
0x250f4e42c:  ff 43 03 d1   sub	sp, sp, #0xd0
0x250f4e430:  eb 2b 06 6d   stp	d11, d10, [sp, #0x60]
0x250f4e434:  e9 23 07 6d   stp	d9, d8, [sp, #0x70]
0x250f4e438:  fa 67 08 a9   stp	x26, x25, [sp, #0x80]
0x250f4e43c:  f8 5f 09 a9   stp	x24, x23, [sp, #0x90]
0x250f4e440:  f6 57 0a a9   stp	x22, x21, [sp, #0xa0]
0x250f4e444:  f4 4f 0b a9   stp	x20, x19, [sp, #0xb0]
0x250f4e448:  fd 7b 0c a9   stp	fp, lr, [sp, #0xc0]
0x250f4e44c:  fd 03 03 91   add	fp, sp, #0xc0
0x250f4e450:  f3 03 03 aa   mov	x19, x3
0x250f4e454:  f4 03 01 aa   mov	x20, x1
0x250f4e458:  f6 03 00 aa   mov	x22, x0
0x250f4e45c:  59 fc c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f4e460:  f5 03 00 aa   mov	x21, x0
0x250f4e464:  37 c0 0e d0   adrp	x23, 0x26e754000
0x250f4e468:  e8 3a 4f b9   ldr	w8, [x23, #0xf38] ; _gLogCategory_APVirtualDisplaySink
0x250f4e46c:  1f 79 00 71   cmp	w8, #0x1e
0x250f4e470:  4c 03 00 54   b.gt	loc_250f4e4d8 ; ⤵ 0x68
0x250f4e474:  1f 05 00 31   cmn	w8, #0x1
0x250f4e478:  40 02 00 54   b.eq	loc_250f4e4c0 ; ⤵ 0x48
0x250f4e47c:  ; loc_250f4e47c
0x250f4e47c:  a8 06 40 f9   ldr	x8, [x21, #0x8]
0x250f4e480:  49 5d 12 90   adrp	x9, 0x275af6000
0x250f4e484:  29 81 18 91   add	x9, x9, #0x620
0x250f4e488:  1f 01 00 f1   cmp	x8, #0
0x250f4e48c:  28 01 88 9a   csel	x8, x9, x8, eq
0x250f4e490:  f6 d3 00 a9   stp	x22, x20, [sp, #0x8]
0x250f4e494:  e8 03 00 f9   str	x8, [sp]
0x250f4e498:  20 c0 0e d0   adrp	x0, 0x26e754000
0x250f4e49c:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f4e4a0:  a1 12 00 f0   adrp	x1, 0x2511a5000
0x250f4e4a4:  21 74 33 91   add	x1, x1, #0xcdd ; "OSStatus vdsink_CopyProperty(CMBaseObjectRef, CFStringRef, CFAllocatorRef, void *)"
0x250f4e4a8:  63 13 00 b0   adrp	x3, 0x2511bb000
0x250f4e4ac:  63 78 36 91   add	x3, x3, #0xd9e ; "%@ virtual display sink %{ptr} copy property %@\n"
0x250f4e4b0:  c2 03 80 52   mov	w2, #0x1e
0x250f4e4b4:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f4e4b8:  92 ca c6 95   bl	j__LogPrintF
0x250f4e4bc:  07 00 00 14   b	loc_250f4e4d8 ; ⤵ 0x1c
0x250f4e4c0:  ; loc_250f4e4c0
0x250f4e4c0:  20 c0 0e d0   adrp	x0, 0x26e754000
0x250f4e4c4:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f4e4c8:  c1 03 80 52   mov	w1, #0x1e
0x250f4e4cc:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f4e4d0:  3c ca c6 95   bl	j___LogCategory_Initialize
0x250f4e4d4:  40 fd ff 35   cbnz	w0, loc_250f4e47c ; ⤴ -0x58
0x250f4e4d8:  ; loc_250f4e4d8
0x250f4e4d8:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e4dc:  08 21 44 f9   ldr	x8, [x8, #0x840] ; _ptr.1e0c048a0 (MediaToolbox/__DATA_CONST.__const)
0x250f4e4e0:  01 01 40 f9   ldr	x1, [x8]
0x250f4e4e4:  e0 03 14 aa   mov	x0, x20
0x250f4e4e8:  96 cd c6 95   bl	j__CFEqual
0x250f4e4ec:  00 02 00 34   cbz	w0, loc_250f4e52c ; ⤵ 0x40
0x250f4e4f0:  40 5d 12 d0   adrp	x0, 0x275af8000
0x250f4e4f4:  00 00 2a 91   add	x0, x0, #0xa80 ; "AirPlay"
0x250f4e4f8:  a2 cd c6 95   bl	j__CFRetain
0x250f4e4fc:  15 00 80 52   mov	w21, #0
0x250f4e500:  60 02 00 f9   str	x0, [x19]
0x250f4e504:  ; loc_250f4e504
0x250f4e504:  e0 03 15 aa   mov	x0, x21
0x250f4e508:  fd 7b 4c a9   ldp	fp, lr, [sp, #0xc0]
0x250f4e50c:  f4 4f 4b a9   ldp	x20, x19, [sp, #0xb0]
0x250f4e510:  f6 57 4a a9   ldp	x22, x21, [sp, #0xa0]
0x250f4e514:  f8 5f 49 a9   ldp	x24, x23, [sp, #0x90]
0x250f4e518:  fa 67 48 a9   ldp	x26, x25, [sp, #0x80]
0x250f4e51c:  e9 23 47 6d   ldp	d9, d8, [sp, #0x70]
0x250f4e520:  eb 2b 46 6d   ldp	d11, d10, [sp, #0x60]
0x250f4e524:  ff 43 03 91   add	sp, sp, #0xd0
0x250f4e528:  ff 0f 5f d6   retab
0x250f4e52c:  ; loc_250f4e52c
0x250f4e52c:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e530:  08 fd 43 f9   ldr	x8, [x8, #0x7f8] ; _ptr.1e0c048f8 (MediaToolbox/__DATA_CONST.__const)
0x250f4e534:  01 01 40 f9   ldr	x1, [x8]
0x250f4e538:  e0 03 14 aa   mov	x0, x20
0x250f4e53c:  81 cd c6 95   bl	j__CFEqual
0x250f4e540:  b8 8b 0b b0   adrp	x24, 0x2680c3000
0x250f4e544:  18 d3 40 f9   ldr	x24, [x24, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x250f4e548:  a0 0a 00 34   cbz	w0, loc_250f4e69c ; ⤵ 0x154
0x250f4e54c:  ff 13 00 f9   str	xzr, [sp, #0x20]
0x250f4e550:  a0 02 40 f9   ldr	x0, [x21]
0x250f4e554:  9b 0e c7 95   bl	j__FigEndpointStreamGetCMBaseObject
0x250f4e558:  f5 03 00 aa   mov	x21, x0
0x250f4e55c:  41 f5 c6 95   bl	j__CMBaseObjectGetVTable
0x250f4e560:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x250f4e564:  f1 03 00 aa   mov	x17, x0
0x250f4e568:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x250f4e56c:  30 1a c1 da   autda	x16, x17
0x250f4e570:  09 0e 43 f8   ldr	x9, [x16, #0x30]!
0x250f4e574:  49 08 00 b4   cbz	x9, loc_250f4e67c ; ⤵ 0x108
0x250f4e578:  e8 03 10 aa   mov	x8, x16
0x250f4e57c:  a8 e4 f2 f2   movk	x8, #0x9725, lsl #0x30
0x250f4e580:  f0 03 09 aa   mov	x16, x9
0x250f4e584:  f1 03 08 aa   mov	x17, x8
0x250f4e588:  30 12 c1 da   autia	x16, x17
0x250f4e58c:  f1 03 10 aa   mov	x17, x16
0x250f4e590:  f1 43 c1 da   xpaci	x17
0x250f4e594:  1f 02 11 eb   cmp	x16, x17
0x250f4e598:  40 00 00 54   b.eq	loc_250f4e5a0 ; ⤵ 0x8
0x250f4e59c:  00 8e 38 d4   brk	#0xc470
0x250f4e5a0:  ; loc_250f4e5a0
0x250f4e5a0:  f0 23 c1 da   paciza	x16
0x250f4e5a4:  d0 06 00 b4   cbz	x16, loc_250f4e67c ; ⤵ 0xd8
0x250f4e5a8:  6a 8c 0b b0   adrp	x10, 0x2680db000
0x250f4e5ac:  4a f9 43 f9   ldr	x10, [x10, #0x7f0] ; _ptr.1e0c048f0 (MediaToolbox/__DATA_CONST.__const)
0x250f4e5b0:  41 01 40 f9   ldr	x1, [x10]
0x250f4e5b4:  14 03 40 f9   ldr	x20, [x24]
0x250f4e5b8:  e3 83 00 91   add	x3, sp, #0x20
0x250f4e5bc:  e0 03 15 aa   mov	x0, x21
0x250f4e5c0:  e2 03 14 aa   mov	x2, x20
0x250f4e5c4:  28 09 3f d7   blraa	x9, x8
0x250f4e5c8:  80 1f 00 35   cbnz	w0, loc_250f4e9b8 ; ⤵ 0x3f0
0x250f4e5cc:  f5 13 40 f9   ldr	x21, [sp, #0x20]
0x250f4e5d0:  e0 03 16 aa   mov	x0, x22
0x250f4e5d4:  fb fb c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f4e5d8:  f6 03 00 aa   mov	x22, x0
0x250f4e5dc:  ff 7f 05 a9   stp	xzr, xzr, [sp, #0x50]
0x250f4e5e0:  ff 7f 04 a9   stp	xzr, xzr, [sp, #0x40]
0x250f4e5e4:  ff 1f 00 f9   str	xzr, [sp, #0x38]
0x250f4e5e8:  88 07 80 52   mov	w8, #0x3c
0x250f4e5ec:  e8 7f 06 29   stp	w8, wzr, [sp, #0x30]
0x250f4e5f0:  ff bf 00 39   strb	wzr, [sp, #0x2f]
0x250f4e5f4:  e8 c0 0e d0   adrp	x8, 0x26e76c000
0x250f4e5f8:  08 c1 42 f9   ldr	x8, [x8, #0x580] ; _vdsink_getVirtualDisplaySizeOverrideFromPrefs.sCheckPrefsOnce
0x250f4e5fc:  1f 05 00 b1   cmn	x8, #0x1
0x250f4e600:  21 1c 00 54   b.ne	loc_250f4e984 ; ⤵ 0x384
0x250f4e604:  ; loc_250f4e604
0x250f4e604:  f8 c0 0e d0   adrp	x24, 0x26e76c000
0x250f4e608:  0a bb 42 fd   ldr	d10, [x24, #0x570] ; _vdsink_getVirtualDisplaySizeOverrideFromPrefs.prefScreenSize.0
0x250f4e60c:  f9 c0 0e d0   adrp	x25, 0x26e76c000
0x250f4e610:  2b bf 42 fd   ldr	d11, [x25, #0x578] ; _vdsink_getVirtualDisplaySizeOverrideFromPrefs.prefScreenSize.1
0x250f4e614:  a8 8b 0b b0   adrp	x8, 0x2680c3000
0x250f4e618:  08 a9 43 f9   ldr	x8, [x8, #0x750] ; _ptr.1842e4d50 (CoreGraphics/__TEXT.__const)
0x250f4e61c:  09 21 40 6d   ldp	d9, d8, [x8]
0x250f4e620:  40 21 69 1e   fcmp	d10, d9
0x250f4e624:  61 00 00 54   b.ne	loc_250f4e630 ; ⤵ 0xc
0x250f4e628:  60 21 68 1e   fcmp	d11, d8
0x250f4e62c:  80 09 00 54   b.eq	loc_250f4e75c ; ⤵ 0x130
0x250f4e630:  ; loc_250f4e630
0x250f4e630:  e8 3a 4f b9   ldr	w8, [x23, #0xf38]
0x250f4e634:  1f c9 00 71   cmp	w8, #0x32
0x250f4e638:  2c 09 00 54   b.gt	loc_250f4e75c ; ⤵ 0x124
0x250f4e63c:  d7 03 80 52   mov	w23, #0x1e
0x250f4e640:  17 40 a0 72   movk	w23, #0x200, lsl #0x10
0x250f4e644:  1f 05 00 31   cmn	w8, #0x1
0x250f4e648:  c0 07 00 54   b.eq	loc_250f4e740 ; ⤵ 0xf8
0x250f4e64c:  ; loc_250f4e64c
0x250f4e64c:  ea 2f 00 6d   stp	d10, d11, [sp]
0x250f4e650:  20 c0 0e d0   adrp	x0, 0x26e754000
0x250f4e654:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f4e658:  61 13 00 b0   adrp	x1, 0x2511bb000
0x250f4e65c:  21 d8 37 91   add	x1, x1, #0xdf6 ; "CGSize vdsink_getVirtualDisplaySizeOverrideFromPrefs(void)"
0x250f4e660:  a3 12 00 f0   adrp	x3, 0x2511a5000
0x250f4e664:  63 94 36 91   add	x3, x3, #0xda5 ; "virtual screen override to %.0fx%.0f\n"
0x250f4e668:  e2 52 00 11   add	w2, w23, #0x14
0x250f4e66c:  25 ca c6 95   bl	j__LogPrintF
0x250f4e670:  0a bb 42 fd   ldr	d10, [x24, #0x570]
0x250f4e674:  2b bf 42 fd   ldr	d11, [x25, #0x578]
0x250f4e678:  39 00 00 14   b	loc_250f4e75c ; ⤵ 0xe4
0x250f4e67c:  ; loc_250f4e67c
0x250f4e67c:  b5 3d 86 12   mov	w21, #0xffffce12
0x250f4e680:  ; loc_250f4e680
0x250f4e680:  a1 12 00 f0   adrp	x1, 0x2511a5000
0x250f4e684:  21 68 35 91   add	x1, x1, #0xd5a ; "vdsink_CopyProperty"
0x250f4e688:  e0 03 15 aa   mov	x0, x21
0x250f4e68c:  c2 20 80 52   mov	w2, #0x106
0x250f4e690:  03 00 80 52   mov	w3, #0
0x250f4e694:  33 f7 c6 95   bl	j__APSLogErrorAt
0x250f4e698:  9b ff ff 17   b	loc_250f4e504 ; ⤴ -0x194
0x250f4e69c:  ; loc_250f4e69c
0x250f4e69c:  a0 02 40 f9   ldr	x0, [x21]
0x250f4e6a0:  48 0e c7 95   bl	j__FigEndpointStreamGetCMBaseObject
0x250f4e6a4:  f5 03 00 aa   mov	x21, x0
0x250f4e6a8:  ee f4 c6 95   bl	j__CMBaseObjectGetVTable
0x250f4e6ac:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x250f4e6b0:  f1 03 00 aa   mov	x17, x0
0x250f4e6b4:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x250f4e6b8:  30 1a c1 da   autda	x16, x17
0x250f4e6bc:  05 0e 43 f8   ldr	x5, [x16, #0x30]!
0x250f4e6c0:  c5 03 00 b4   cbz	x5, loc_250f4e738 ; ⤵ 0x78
0x250f4e6c4:  e4 03 10 aa   mov	x4, x16
0x250f4e6c8:  a4 e4 f2 f2   movk	x4, #0x9725, lsl #0x30
0x250f4e6cc:  f0 03 05 aa   mov	x16, x5
0x250f4e6d0:  f1 03 04 aa   mov	x17, x4
0x250f4e6d4:  30 12 c1 da   autia	x16, x17
0x250f4e6d8:  f1 03 10 aa   mov	x17, x16
0x250f4e6dc:  f1 43 c1 da   xpaci	x17
0x250f4e6e0:  1f 02 11 eb   cmp	x16, x17
0x250f4e6e4:  40 00 00 54   b.eq	loc_250f4e6ec ; ⤵ 0x8
0x250f4e6e8:  00 8e 38 d4   brk	#0xc470
0x250f4e6ec:  ; loc_250f4e6ec
0x250f4e6ec:  f0 23 c1 da   paciza	x16
0x250f4e6f0:  50 02 00 b4   cbz	x16, loc_250f4e738 ; ⤵ 0x48
0x250f4e6f4:  02 03 40 f9   ldr	x2, [x24]
0x250f4e6f8:  e0 03 15 aa   mov	x0, x21
0x250f4e6fc:  e1 03 14 aa   mov	x1, x20
0x250f4e700:  e3 03 13 aa   mov	x3, x19
0x250f4e704:  fd 7b 4c a9   ldp	fp, lr, [sp, #0xc0]
0x250f4e708:  f4 4f 4b a9   ldp	x20, x19, [sp, #0xb0]
0x250f4e70c:  f6 57 4a a9   ldp	x22, x21, [sp, #0xa0]
0x250f4e710:  f8 5f 49 a9   ldp	x24, x23, [sp, #0x90]
0x250f4e714:  fa 67 48 a9   ldp	x26, x25, [sp, #0x80]
0x250f4e718:  e9 23 47 6d   ldp	d9, d8, [sp, #0x70]
0x250f4e71c:  eb 2b 46 6d   ldp	d11, d10, [sp, #0x60]
0x250f4e720:  ff 43 03 91   add	sp, sp, #0xd0
0x250f4e724:  ff 23 03 d5   autibsp
0x250f4e728:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250f4e72c:  50 00 f0 b6   tbz	x16, #0x3e, loc_250f4e734 ; ⤵ 0x8
0x250f4e730:  20 8e 38 d4   brk	#0xc471
0x250f4e734:  ; loc_250f4e734
0x250f4e734:  a4 08 1f d7   braa	x5, x4
0x250f4e738:  ; loc_250f4e738
0x250f4e738:  b5 3d 86 12   mov	w21, #0xffffce12
0x250f4e73c:  72 ff ff 17   b	loc_250f4e504 ; ⤴ -0x238
0x250f4e740:  ; loc_250f4e740
0x250f4e740:  20 c0 0e d0   adrp	x0, 0x26e754000
0x250f4e744:  00 e0 3c 91   add	x0, x0, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f4e748:  e1 52 00 11   add	w1, w23, #0x14
0x250f4e74c:  9d c9 c6 95   bl	j___LogCategory_Initialize
0x250f4e750:  0a bb 42 fd   ldr	d10, [x24, #0x570]
0x250f4e754:  2b bf 42 fd   ldr	d11, [x25, #0x578]
0x250f4e758:  a0 f7 ff 35   cbnz	w0, loc_250f4e64c ; ⤴ -0x10c
0x250f4e75c:  ; loc_250f4e75c
0x250f4e75c:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e760:  08 ad 43 f9   ldr	x8, [x8, #0x758] ; _ptr.1e0c04920 (MediaToolbox/__DATA_CONST.__const)
0x250f4e764:  01 01 40 f9   ldr	x1, [x8]
0x250f4e768:  e2 bf 00 91   add	x2, sp, #0x2f
0x250f4e76c:  e0 03 15 aa   mov	x0, x21
0x250f4e770:  94 fc c6 95   bl	j__FigCFDictionaryGetBooleanIfPresent
0x250f4e774:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e778:  08 a1 43 f9   ldr	x8, [x8, #0x740] ; _ptr.1e0c04908 (MediaToolbox/__DATA_CONST.__const)
0x250f4e77c:  01 01 40 f9   ldr	x1, [x8]
0x250f4e780:  c2 e2 00 91   add	x2, x22, #0x38
0x250f4e784:  e0 03 15 aa   mov	x0, x21
0x250f4e788:  96 fc c6 95   bl	j__FigCFDictionaryGetCGSizeIfPresent
0x250f4e78c:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e790:  08 a9 43 f9   ldr	x8, [x8, #0x750] ; _ptr.1e0c04950 (MediaToolbox/__DATA_CONST.__const)
0x250f4e794:  01 01 40 f9   ldr	x1, [x8]
0x250f4e798:  e0 03 15 aa   mov	x0, x21
0x250f4e79c:  85 cf c6 95   bl	j__CFDictionaryGetValue
0x250f4e7a0:  f7 03 00 aa   mov	x23, x0
0x250f4e7a4:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e7a8:  08 bd 43 f9   ldr	x8, [x8, #0x778] ; _ptr.1e0c04958 (MediaToolbox/__DATA_CONST.__const)
0x250f4e7ac:  01 01 40 f9   ldr	x1, [x8]
0x250f4e7b0:  e2 c3 00 91   add	x2, sp, #0x30
0x250f4e7b4:  e0 03 15 aa   mov	x0, x21
0x250f4e7b8:  96 fc c6 95   bl	j__FigCFDictionaryGetInt32IfPresent
0x250f4e7bc:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e7c0:  08 b1 43 f9   ldr	x8, [x8, #0x760] ; _ptr.1e0c04960 (MediaToolbox/__DATA_CONST.__const)
0x250f4e7c4:  01 01 40 f9   ldr	x1, [x8]
0x250f4e7c8:  e2 d3 00 91   add	x2, sp, #0x34
0x250f4e7cc:  e0 03 15 aa   mov	x0, x21
0x250f4e7d0:  90 fc c6 95   bl	j__FigCFDictionaryGetInt32IfPresent
0x250f4e7d4:  40 21 69 1e   fcmp	d10, d9
0x250f4e7d8:  61 02 00 54   b.ne	loc_250f4e824 ; ⤵ 0x4c
0x250f4e7dc:  60 21 68 1e   fcmp	d11, d8
0x250f4e7e0:  21 02 00 54   b.ne	loc_250f4e824 ; ⤵ 0x44
0x250f4e7e4:  c0 82 c3 3c   ldur	q0, [x22, #0x38]
0x250f4e7e8:  c0 82 84 3c   stur	q0, [x22, #0x48]
0x250f4e7ec:  17 02 00 b5   cbnz	x23, loc_250f4e82c ; ⤵ 0x40
0x250f4e7f0:  ; loc_250f4e7f0
0x250f4e7f0:  c0 86 44 6d   ldp	d0, d1, [x22, #0x48]
0x250f4e7f4:  00 00 78 1e   fcvtzs	w0, d0
0x250f4e7f8:  21 00 78 1e   fcvtzs	w1, d1
0x250f4e7fc:  e2 bf 40 39   ldrb	w2, [sp, #0x2f]
0x250f4e800:  e3 33 40 b9   ldr	w3, [sp, #0x30]
0x250f4e804:  e8 03 01 91   add	x8, sp, #0x40
0x250f4e808:  e8 03 00 f9   str	x8, [sp]
0x250f4e80c:  e7 23 01 91   add	x7, sp, #0x48
0x250f4e810:  84 0c 80 52   mov	w4, #0x64
0x250f4e814:  05 00 80 52   mov	w5, #0
0x250f4e818:  06 00 80 52   mov	w6, #0
0x250f4e81c:  51 0d c7 95   bl	j__FigDisplayModes_BuildColorAndTimingModes
0x250f4e820:  1b 00 00 14   b	loc_250f4e88c ; ⤵ 0x6c
0x250f4e824:  ; loc_250f4e824
0x250f4e824:  ca ae 04 6d   stp	d10, d11, [x22, #0x48]
0x250f4e828:  57 fe ff b4   cbz	x23, loc_250f4e7f0 ; ⤴ -0x38
0x250f4e82c:  ; loc_250f4e82c
0x250f4e82c:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e830:  08 15 43 f9   ldr	x8, [x8, #0x628] ; _ptr.1e0c03ff8 (MediaToolbox/__DATA_CONST.__const)
0x250f4e834:  01 01 40 f9   ldr	x1, [x8]
0x250f4e838:  e0 03 17 aa   mov	x0, x23
0x250f4e83c:  5d cf c6 95   bl	j__CFDictionaryGetValue
0x250f4e840:  f8 03 00 aa   mov	x24, x0
0x250f4e844:  c0 86 44 6d   ldp	d0, d1, [x22, #0x48]
0x250f4e848:  00 00 78 1e   fcvtzs	w0, d0
0x250f4e84c:  21 00 78 1e   fcvtzs	w1, d1
0x250f4e850:  e2 bf 40 39   ldrb	w2, [sp, #0x2f]
0x250f4e854:  e3 33 40 b9   ldr	w3, [sp, #0x30]
0x250f4e858:  e8 03 01 91   add	x8, sp, #0x40
0x250f4e85c:  e8 03 00 f9   str	x8, [sp]
0x250f4e860:  84 0c 80 52   mov	w4, #0x64
0x250f4e864:  05 00 80 52   mov	w5, #0
0x250f4e868:  06 00 80 52   mov	w6, #0
0x250f4e86c:  07 00 80 d2   mov	x7, #0
0x250f4e870:  3c 0d c7 95   bl	j__FigDisplayModes_BuildColorAndTimingModes
0x250f4e874:  e4 23 01 91   add	x4, sp, #0x48
0x250f4e878:  e0 03 18 aa   mov	x0, x24
0x250f4e87c:  e1 03 17 aa   mov	x1, x23
0x250f4e880:  02 00 80 52   mov	w2, #0
0x250f4e884:  83 0c 80 52   mov	w3, #0x64
0x250f4e888:  3a 0d c7 95   bl	j__FigDisplayModes_BuildColorModesFromHDRMode
0x250f4e88c:  ; loc_250f4e88c
0x250f4e88c:  e8 27 40 f9   ldr	x8, [sp, #0x48]
0x250f4e890:  e8 07 00 b4   cbz	x8, loc_250f4e98c ; ⤵ 0xfc
0x250f4e894:  e0 23 40 f9   ldr	x0, [sp, #0x40]
0x250f4e898:  e0 07 00 b4   cbz	x0, loc_250f4e994 ; ⤵ 0xfc
0x250f4e89c:  e1 37 40 b9   ldr	w1, [sp, #0x34]
0x250f4e8a0:  3f 04 00 71   cmp	w1, #0x1
0x250f4e8a4:  6b 01 00 54   b.lt	loc_250f4e8d0 ; ⤵ 0x2c
0x250f4e8a8:  e2 e3 00 91   add	x2, sp, #0x38
0x250f4e8ac:  39 0d c7 95   bl	j__FigDisplayModes_CopyTimingModeAppendingPreferredUIScale
0x250f4e8b0:  e0 1f 40 f9   ldr	x0, [sp, #0x38]
0x250f4e8b4:  c0 07 00 b4   cbz	x0, loc_250f4e9ac ; ⤵ 0xf8
0x250f4e8b8:  f6 23 40 f9   ldr	x22, [sp, #0x40]
0x250f4e8bc:  e0 23 00 f9   str	x0, [sp, #0x40]
0x250f4e8c0:  b0 cc c6 95   bl	j__CFRetain
0x250f4e8c4:  76 00 00 b4   cbz	x22, loc_250f4e8d0 ; ⤵ 0xc
0x250f4e8c8:  e0 03 16 aa   mov	x0, x22
0x250f4e8cc:  a9 cc c6 95   bl	j__CFRelease
0x250f4e8d0:  ; loc_250f4e8d0
0x250f4e8d0:  e1 03 44 a9   ldp	x1, x0, [sp, #0x40]
0x250f4e8d4:  e2 63 01 91   add	x2, sp, #0x58
0x250f4e8d8:  e3 43 01 91   add	x3, sp, #0x50
0x250f4e8dc:  31 0d c7 95   bl	j__FigDisplayModes_CreateColorAndTimingModesArraysWithPreferredMode
0x250f4e8e0:  e8 2f 40 f9   ldr	x8, [sp, #0x58]
0x250f4e8e4:  c8 05 00 b4   cbz	x8, loc_250f4e99c ; ⤵ 0xb8
0x250f4e8e8:  e8 2b 40 f9   ldr	x8, [sp, #0x50]
0x250f4e8ec:  c8 05 00 b4   cbz	x8, loc_250f4e9a4 ; ⤵ 0xb8
0x250f4e8f0:  e0 03 14 aa   mov	x0, x20
0x250f4e8f4:  01 00 80 d2   mov	x1, #0
0x250f4e8f8:  e2 03 15 aa   mov	x2, x21
0x250f4e8fc:  21 cf c6 95   bl	j__CFDictionaryCreateMutableCopy
0x250f4e900:  f4 03 00 aa   mov	x20, x0
0x250f4e904:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e908:  08 91 43 f9   ldr	x8, [x8, #0x720] ; _ptr.1e0c04900 (MediaToolbox/__DATA_CONST.__const)
0x250f4e90c:  01 01 40 f9   ldr	x1, [x8]
0x250f4e910:  e2 2f 40 f9   ldr	x2, [sp, #0x58]
0x250f4e914:  2b cf c6 95   bl	j__CFDictionarySetValue
0x250f4e918:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4e91c:  08 b9 43 f9   ldr	x8, [x8, #0x770] ; _ptr.1e0c04928 (MediaToolbox/__DATA_CONST.__const)
0x250f4e920:  01 01 40 f9   ldr	x1, [x8]
0x250f4e924:  e2 2b 40 f9   ldr	x2, [sp, #0x50]
0x250f4e928:  e0 03 14 aa   mov	x0, x20
0x250f4e92c:  25 cf c6 95   bl	j__CFDictionarySetValue
0x250f4e930:  ; loc_250f4e930
0x250f4e930:  e0 27 40 f9   ldr	x0, [sp, #0x48]
0x250f4e934:  40 00 00 b4   cbz	x0, loc_250f4e93c ; ⤵ 0x8
0x250f4e938:  8e cc c6 95   bl	j__CFRelease
0x250f4e93c:  ; loc_250f4e93c
0x250f4e93c:  e0 23 40 f9   ldr	x0, [sp, #0x40]
0x250f4e940:  40 00 00 b4   cbz	x0, loc_250f4e948 ; ⤵ 0x8
0x250f4e944:  8b cc c6 95   bl	j__CFRelease
0x250f4e948:  ; loc_250f4e948
0x250f4e948:  e0 1f 40 f9   ldr	x0, [sp, #0x38]
0x250f4e94c:  40 00 00 b4   cbz	x0, loc_250f4e954 ; ⤵ 0x8
0x250f4e950:  88 cc c6 95   bl	j__CFRelease
0x250f4e954:  ; loc_250f4e954
0x250f4e954:  e0 2f 40 f9   ldr	x0, [sp, #0x58]
0x250f4e958:  40 00 00 b4   cbz	x0, loc_250f4e960 ; ⤵ 0x8
0x250f4e95c:  85 cc c6 95   bl	j__CFRelease
0x250f4e960:  ; loc_250f4e960
0x250f4e960:  e0 2b 40 f9   ldr	x0, [sp, #0x50]
0x250f4e964:  40 00 00 b4   cbz	x0, loc_250f4e96c ; ⤵ 0x8
0x250f4e968:  82 cc c6 95   bl	j__CFRelease
0x250f4e96c:  ; loc_250f4e96c
0x250f4e96c:  74 02 00 f9   str	x20, [x19]
0x250f4e970:  e0 13 40 f9   ldr	x0, [sp, #0x20]
0x250f4e974:  40 00 00 b4   cbz	x0, loc_250f4e97c ; ⤵ 0x8
0x250f4e978:  7e cc c6 95   bl	j__CFRelease
0x250f4e97c:  ; loc_250f4e97c
0x250f4e97c:  15 00 80 52   mov	w21, #0
0x250f4e980:  e1 fe ff 17   b	loc_250f4e504 ; ⤴ -0x47c
0x250f4e984:  ; loc_250f4e984
0x250f4e984:  e9 67 07 94   bl	_vdsink_CopyProperty.cold.1
0x250f4e988:  1f ff ff 17   b	loc_250f4e604 ; ⤴ -0x384
0x250f4e98c:  ; loc_250f4e98c
0x250f4e98c:  15 68 07 94   bl	_vdsink_CopyProperty.cold.6
0x250f4e990:  08 00 00 14   b	loc_250f4e9b0 ; ⤵ 0x20
0x250f4e994:  ; loc_250f4e994
0x250f4e994:  0a 68 07 94   bl	_vdsink_CopyProperty.cold.5
0x250f4e998:  06 00 00 14   b	loc_250f4e9b0 ; ⤵ 0x18
0x250f4e99c:  ; loc_250f4e99c
0x250f4e99c:  f6 67 07 94   bl	_vdsink_CopyProperty.cold.3
0x250f4e9a0:  04 00 00 14   b	loc_250f4e9b0 ; ⤵ 0x10
0x250f4e9a4:  ; loc_250f4e9a4
0x250f4e9a4:  eb 67 07 94   bl	_vdsink_CopyProperty.cold.2
0x250f4e9a8:  02 00 00 14   b	loc_250f4e9b0 ; ⤵ 0x8
0x250f4e9ac:  ; loc_250f4e9ac
0x250f4e9ac:  fb 67 07 94   bl	_vdsink_CopyProperty.cold.4
0x250f4e9b0:  ; loc_250f4e9b0
0x250f4e9b0:  14 00 80 d2   mov	x20, #0
0x250f4e9b4:  df ff ff 17   b	loc_250f4e930 ; ⤴ -0x84
0x250f4e9b8:  ; loc_250f4e9b8
0x250f4e9b8:  f5 03 00 aa   mov	x21, x0
0x250f4e9bc:  31 ff ff 17   b	loc_250f4e680 ; ⤴ -0x33c

