
_screenstream_CopyProperty:
0x250f50598:  7f 23 03 d5   pacibsp
0x250f5059c:  ff 43 03 d1   sub	sp, sp, #0xd0
0x250f505a0:  f8 5f 09 a9   stp	x24, x23, [sp, #0x90]
0x250f505a4:  f6 57 0a a9   stp	x22, x21, [sp, #0xa0]
0x250f505a8:  f4 4f 0b a9   stp	x20, x19, [sp, #0xb0]
0x250f505ac:  fd 7b 0c a9   stp	fp, lr, [sp, #0xc0]
0x250f505b0:  fd 03 03 91   add	fp, sp, #0xc0
0x250f505b4:  f3 03 03 aa   mov	x19, x3
0x250f505b8:  f6 03 02 aa   mov	x22, x2
0x250f505bc:  f5 03 01 aa   mov	x21, x1
0x250f505c0:  00 f4 c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f505c4:  f4 03 00 aa   mov	x20, x0
0x250f505c8:  f7 a9 0f d0   adrp	x23, 0x27048e000
0x250f505cc:  e8 32 4b b9   ldr	w8, [x23, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f505d0:  1f 79 00 71   cmp	w8, #0x1e
0x250f505d4:  ac 02 00 54   b.gt	loc_250f50628 ; ⤵ 0x54
0x250f505d8:  1f 05 00 31   cmn	w8, #0x1
0x250f505dc:  a0 01 00 54   b.eq	loc_250f50610 ; ⤵ 0x34
0x250f505e0:  ; loc_250f505e0
0x250f505e0:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f505e4:  e8 57 00 a9   stp	x8, x21, [sp]
0x250f505e8:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f505ec:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f505f0:  a1 15 00 b0   adrp	x1, 0x251205000
0x250f505f4:  21 b0 26 91   add	x1, x1, #0x9ac ; "OSStatus screenstream_CopyProperty(CMBaseObjectRef, CFStringRef, CFAllocatorRef, void *)"
0x250f505f8:  a3 15 00 b0   adrp	x3, 0x251205000
0x250f505fc:  63 14 28 91   add	x3, x3, #0xa05 ; "%@ copy propertyKey: '%@'\n"
0x250f50600:  c2 03 80 52   mov	w2, #0x1e
0x250f50604:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f50608:  3e c2 c6 95   bl	j__LogPrintF
0x250f5060c:  07 00 00 14   b	loc_250f50628 ; ⤵ 0x1c
0x250f50610:  ; loc_250f50610
0x250f50610:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f50614:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f50618:  c1 03 80 52   mov	w1, #0x1e
0x250f5061c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f50620:  e8 c1 c6 95   bl	j___LogCategory_Initialize
0x250f50624:  e0 fd ff 35   cbnz	w0, loc_250f505e0 ; ⤴ -0x44
0x250f50628:  ; loc_250f50628
0x250f50628:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f5062c:  08 e5 43 f9   ldr	x8, [x8, #0x7c8] ; _ptr.1e0a147d0 (CoreMedia/__DATA_CONST.__const)
0x250f50630:  01 01 40 f9   ldr	x1, [x8]
0x250f50634:  e0 03 15 aa   mov	x0, x21
0x250f50638:  42 c5 c6 95   bl	j__CFEqual
0x250f5063c:  80 00 00 34   cbz	w0, loc_250f5064c ; ⤵ 0x10
0x250f50640:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f50644:  08 25 44 f9   ldr	x8, [x8, #0x848] ; _ptr.1e0a14848 (CoreMedia/__DATA_CONST.__const)
0x250f50648:  0e 00 00 14   b	loc_250f50680 ; ⤵ 0x38
0x250f5064c:  ; loc_250f5064c
0x250f5064c:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f50650:  08 bd 43 f9   ldr	x8, [x8, #0x778] ; _ptr.1e0a147d8 (CoreMedia/__DATA_CONST.__const)
0x250f50654:  01 01 40 f9   ldr	x1, [x8]
0x250f50658:  e0 03 15 aa   mov	x0, x21
0x250f5065c:  39 c5 c6 95   bl	j__CFEqual
0x250f50660:  60 02 00 34   cbz	w0, loc_250f506ac ; ⤵ 0x4c
0x250f50664:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f50668:  08 0d 44 f9   ldr	x8, [x8, #0x818] ; _ptr.1e0a14878 (CoreMedia/__DATA_CONST.__const)
0x250f5066c:  89 5a 40 b9   ldr	w9, [x20, #0x58]
0x250f50670:  aa 8b 0b b0   adrp	x10, 0x2680c5000
0x250f50674:  4a 09 44 f9   ldr	x10, [x10, #0x810] ; _ptr.1e0a14880 (CoreMedia/__DATA_CONST.__const)
0x250f50678:  3f b9 01 71   cmp	w9, #0x6e
0x250f5067c:  08 01 8a 9a   csel	x8, x8, x10, eq
0x250f50680:  ; loc_250f50680
0x250f50680:  00 01 40 f9   ldr	x0, [x8]
0x250f50684:  ; loc_250f50684
0x250f50684:  3f c5 c6 95   bl	j__CFRetain
0x250f50688:  ; loc_250f50688
0x250f50688:  e8 03 00 aa   mov	x8, x0
0x250f5068c:  ; loc_250f5068c
0x250f5068c:  00 00 80 52   mov	w0, #0
0x250f50690:  68 02 00 f9   str	x8, [x19]
0x250f50694:  ; loc_250f50694
0x250f50694:  fd 7b 4c a9   ldp	fp, lr, [sp, #0xc0]
0x250f50698:  f4 4f 4b a9   ldp	x20, x19, [sp, #0xb0]
0x250f5069c:  f6 57 4a a9   ldp	x22, x21, [sp, #0xa0]
0x250f506a0:  f8 5f 49 a9   ldp	x24, x23, [sp, #0x90]
0x250f506a4:  ff 43 03 91   add	sp, sp, #0xd0
0x250f506a8:  ff 0f 5f d6   retab
0x250f506ac:  ; loc_250f506ac
0x250f506ac:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f506b0:  08 a1 43 f9   ldr	x8, [x8, #0x740] ; _ptr.1e0a147a0 (CoreMedia/__DATA_CONST.__const)
0x250f506b4:  01 01 40 f9   ldr	x1, [x8]
0x250f506b8:  e0 03 15 aa   mov	x0, x21
0x250f506bc:  21 c5 c6 95   bl	j__CFEqual
0x250f506c0:  60 00 00 34   cbz	w0, loc_250f506cc ; ⤵ 0xc
0x250f506c4:  80 2a 40 f9   ldr	x0, [x20, #0x50]
0x250f506c8:  ef ff ff 17   b	loc_250f50684 ; ⤴ -0x44
0x250f506cc:  ; loc_250f506cc
0x250f506cc:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f506d0:  08 f9 43 f9   ldr	x8, [x8, #0x7f0] ; _ptr.1e0c048f0 (MediaToolbox/__DATA_CONST.__const)
0x250f506d4:  01 01 40 f9   ldr	x1, [x8]
0x250f506d8:  e0 03 15 aa   mov	x0, x21
0x250f506dc:  19 c5 c6 95   bl	j__CFEqual
0x250f506e0:  40 07 00 34   cbz	w0, loc_250f507c8 ; ⤵ 0xe8
0x250f506e4:  88 8b 0b f0   adrp	x8, 0x2680c3000
0x250f506e8:  08 d1 40 f9   ldr	x8, [x8, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x250f506ec:  00 01 40 f9   ldr	x0, [x8]
0x250f506f0:  82 8b 0b f0   adrp	x2, 0x2680c3000
0x250f506f4:  42 c8 41 f9   ldr	x2, [x2, #0x390] ; _ptr.1e8d91d98 (CoreFoundation/__AUTH_CONST.__const)
0x250f506f8:  83 8b 0b f0   adrp	x3, 0x2680c3000
0x250f506fc:  63 cc 41 f9   ldr	x3, [x3, #0x398] ; _ptr.1e8d91df8 (CoreFoundation/__AUTH_CONST.__const)
0x250f50700:  01 00 80 d2   mov	x1, #0
0x250f50704:  9b c7 c6 95   bl	j__CFDictionaryCreateMutable
0x250f50708:  f5 03 00 aa   mov	x21, x0
0x250f5070c:  88 b6 40 f9   ldr	x8, [x20, #0x168]
0x250f50710:  96 8b 0b f0   adrp	x22, 0x2680c3000
0x250f50714:  d6 aa 43 f9   ldr	x22, [x22, #0x750] ; _ptr.1842e4d50 (CoreGraphics/__TEXT.__const)
0x250f50718:  1f 05 00 f1   cmp	x8, #0x1
0x250f5071c:  81 06 00 54   b.ne	loc_250f507ec ; ⤵ 0xd0
0x250f50720:  c0 02 c0 3d   ldr	q0, [x22]
0x250f50724:  a0 03 9c 3c   stur	q0, [fp, #-0x40]
0x250f50728:  bf c3 1b b8   stur	wzr, [fp, #-0x44]
0x250f5072c:  80 96 40 f9   ldr	x0, [x20, #0x128]
0x250f50730:  81 d2 41 b9   ldr	w1, [x20, #0x1d0]
0x250f50734:  82 e6 45 39   ldrb	w2, [x20, #0x179]
0x250f50738:  83 a6 44 b9   ldr	w3, [x20, #0x4a4]
0x250f5073c:  80 86 4f 6d   ldp	d0, d1, [x20, #0xf8]
0x250f50740:  a4 03 01 d1   sub	x4, fp, #0x40
0x250f50744:  a5 13 01 d1   sub	x5, fp, #0x44
0x250f50748:  da ff c6 95   bl	j__APSScreenGetMediaPresentationParams
0x250f5074c:  e8 32 4b b9   ldr	w8, [x23, #0xb30]
0x250f50750:  1f c9 00 71   cmp	w8, #0x32
0x250f50754:  4c 01 00 54   b.gt	loc_250f5077c ; ⤵ 0x28
0x250f50758:  1f 05 00 31   cmn	w8, #0x1
0x250f5075c:  61 19 00 54   b.ne	loc_250f50a88 ; ⤵ 0x32c
0x250f50760:  c8 03 80 52   mov	w8, #0x1e
0x250f50764:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250f50768:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f5076c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f50770:  01 51 00 11   add	w1, w8, #0x14
0x250f50774:  93 c1 c6 95   bl	j___LogCategory_Initialize
0x250f50778:  80 18 00 35   cbnz	w0, loc_250f50a88 ; ⤵ 0x310
0x250f5077c:  ; loc_250f5077c
0x250f5077c:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50780:  08 a1 43 f9   ldr	x8, [x8, #0x740] ; _ptr.1e0c04908 (MediaToolbox/__DATA_CONST.__const)
0x250f50784:  01 01 40 f9   ldr	x1, [x8]
0x250f50788:  a0 07 7c 6d   ldp	d0, d1, [fp, #-0x40]
0x250f5078c:  e0 03 15 aa   mov	x0, x21
0x250f50790:  c0 f4 c6 95   bl	j__FigCFDictionarySetCGSize
0x250f50794:  88 e6 45 39   ldrb	w8, [x20, #0x179]
0x250f50798:  e8 00 00 34   cbz	w8, loc_250f507b4 ; ⤵ 0x1c
0x250f5079c:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f507a0:  08 a9 43 f9   ldr	x8, [x8, #0x750] ; _ptr.1e0c04950 (MediaToolbox/__DATA_CONST.__const)
0x250f507a4:  01 01 40 f9   ldr	x1, [x8]
0x250f507a8:  82 a2 40 f9   ldr	x2, [x20, #0x140]
0x250f507ac:  e0 03 15 aa   mov	x0, x21
0x250f507b0:  84 c7 c6 95   bl	j__CFDictionarySetValue
0x250f507b4:  ; loc_250f507b4
0x250f507b4:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f507b8:  08 bd 43 f9   ldr	x8, [x8, #0x778] ; _ptr.1e0c04958 (MediaToolbox/__DATA_CONST.__const)
0x250f507bc:  01 01 40 f9   ldr	x1, [x8]
0x250f507c0:  a2 c3 5b b8   ldur	w2, [fp, #-0x44]
0x250f507c4:  5c 00 00 14   b	loc_250f50934 ; ⤵ 0x170
0x250f507c8:  ; loc_250f507c8
0x250f507c8:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f507cc:  08 11 44 f9   ldr	x8, [x8, #0x820] ; _ptr.1e0c04980 (MediaToolbox/__DATA_CONST.__const)
0x250f507d0:  01 01 40 f9   ldr	x1, [x8]
0x250f507d4:  e0 03 15 aa   mov	x0, x21
0x250f507d8:  da c4 c6 95   bl	j__CFEqual
0x250f507dc:  c0 03 00 34   cbz	w0, loc_250f50854 ; ⤵ 0x78
0x250f507e0:  e0 03 14 aa   mov	x0, x20
0x250f507e4:  77 f8 ff 97   bl	_screenstream_copyNetworkInfo
0x250f507e8:  a8 ff ff 17   b	loc_250f50688 ; ⤴ -0x160
0x250f507ec:  ; loc_250f507ec
0x250f507ec:  88 ea 45 39   ldrb	w8, [x20, #0x17a]
0x250f507f0:  88 00 00 34   cbz	w8, loc_250f50800 ; ⤵ 0x10
0x250f507f4:  88 a6 44 b9   ldr	w8, [x20, #0x4a4]
0x250f507f8:  1f 01 00 71   cmp	w8, #0
0x250f507fc:  4d 05 00 54   b.le	loc_250f508a4 ; ⤵ 0xa8
0x250f50800:  ; loc_250f50800
0x250f50800:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50804:  08 a1 43 f9   ldr	x8, [x8, #0x740] ; _ptr.1e0c04908 (MediaToolbox/__DATA_CONST.__const)
0x250f50808:  01 01 40 f9   ldr	x1, [x8]
0x250f5080c:  80 86 4d 6d   ldp	d0, d1, [x20, #0xd8]
0x250f50810:  e0 03 15 aa   mov	x0, x21
0x250f50814:  9f f4 c6 95   bl	j__FigCFDictionarySetCGSize
0x250f50818:  e8 32 4b b9   ldr	w8, [x23, #0xb30]
0x250f5081c:  1f a1 00 71   cmp	w8, #0x28
0x250f50820:  ec 06 00 54   b.gt	loc_250f508fc ; ⤵ 0xdc
0x250f50824:  1f 05 00 31   cmn	w8, #0x1
0x250f50828:  01 01 00 54   b.ne	loc_250f50848 ; ⤵ 0x20
0x250f5082c:  c8 03 80 52   mov	w8, #0x1e
0x250f50830:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250f50834:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f50838:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f5083c:  01 29 00 11   add	w1, w8, #0xa
0x250f50840:  60 c1 c6 95   bl	j___LogCategory_Initialize
0x250f50844:  c0 05 00 34   cbz	w0, loc_250f508fc ; ⤵ 0xb8
0x250f50848:  ; loc_250f50848
0x250f50848:  a1 15 00 b0   adrp	x1, 0x251205000
0x250f5084c:  21 c8 2c 91   add	x1, x1, #0xb32 ; "%@ using displaySizePixels\n"
0x250f50850:  29 00 00 14   b	loc_250f508f4 ; ⤵ 0xa4
0x250f50854:  ; loc_250f50854
0x250f50854:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f50858:  08 a5 43 f9   ldr	x8, [x8, #0x748] ; _ptr.1e0a14928 (CoreMedia/__DATA_CONST.__const)
0x250f5085c:  01 01 40 f9   ldr	x1, [x8]
0x250f50860:  e0 03 15 aa   mov	x0, x21
0x250f50864:  b7 c4 c6 95   bl	j__CFEqual
0x250f50868:  e0 00 00 35   cbnz	w0, loc_250f50884 ; ⤵ 0x1c
0x250f5086c:  e8 21 0c f0   adrp	x8, 0x26938f000
0x250f50870:  08 a1 1f 91   add	x8, x8, #0x7e8 ; _kAPEndpointStreamProperty_IsPlaying
0x250f50874:  01 01 40 f9   ldr	x1, [x8]
0x250f50878:  e0 03 15 aa   mov	x0, x21
0x250f5087c:  b1 c4 c6 95   bl	j__CFEqual
0x250f50880:  20 0f 00 34   cbz	w0, loc_250f50a64 ; ⤵ 0x1e4
0x250f50884:  ; loc_250f50884
0x250f50884:  88 8b 0b f0   adrp	x8, 0x2680c3000
0x250f50888:  08 f5 40 f9   ldr	x8, [x8, #0x1e8] ; _ptr.1e0549de0 (CoreFoundation/__DATA_CONST.__const)
0x250f5088c:  89 6a 40 39   ldrb	w9, [x20, #0x1a]
0x250f50890:  8a 8b 0b f0   adrp	x10, 0x2680c3000
0x250f50894:  4a ed 40 f9   ldr	x10, [x10, #0x1d8] ; _ptr.1e0549de8 (CoreFoundation/__DATA_CONST.__const)
0x250f50898:  3f 01 00 71   cmp	w9, #0
0x250f5089c:  48 01 88 9a   csel	x8, x10, x8, eq
0x250f508a0:  78 ff ff 17   b	loc_250f50680 ; ⤴ -0x220
0x250f508a4:  ; loc_250f508a4
0x250f508a4:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f508a8:  08 a1 43 f9   ldr	x8, [x8, #0x740] ; _ptr.1e0c04908 (MediaToolbox/__DATA_CONST.__const)
0x250f508ac:  01 01 40 f9   ldr	x1, [x8]
0x250f508b0:  80 86 4f 6d   ldp	d0, d1, [x20, #0xf8]
0x250f508b4:  e0 03 15 aa   mov	x0, x21
0x250f508b8:  76 f4 c6 95   bl	j__FigCFDictionarySetCGSize
0x250f508bc:  e8 32 4b b9   ldr	w8, [x23, #0xb30]
0x250f508c0:  1f a1 00 71   cmp	w8, #0x28
0x250f508c4:  cc 01 00 54   b.gt	loc_250f508fc ; ⤵ 0x38
0x250f508c8:  1f 05 00 31   cmn	w8, #0x1
0x250f508cc:  01 01 00 54   b.ne	loc_250f508ec ; ⤵ 0x20
0x250f508d0:  c8 03 80 52   mov	w8, #0x1e
0x250f508d4:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250f508d8:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f508dc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f508e0:  01 29 00 11   add	w1, w8, #0xa
0x250f508e4:  37 c1 c6 95   bl	j___LogCategory_Initialize
0x250f508e8:  a0 00 00 34   cbz	w0, loc_250f508fc ; ⤵ 0x14
0x250f508ec:  ; loc_250f508ec
0x250f508ec:  a1 15 00 b0   adrp	x1, 0x251205000
0x250f508f0:  21 4c 2c 91   add	x1, x1, #0xb13 ; "%@ using displaySizePixelsMax\n"
0x250f508f4:  ; loc_250f508f4
0x250f508f4:  e0 03 14 aa   mov	x0, x20
0x250f508f8:  fe 9c 08 94   bl	_screenstream_CopyProperty.cold.1
0x250f508fc:  ; loc_250f508fc
0x250f508fc:  88 4a 46 39   ldrb	w8, [x20, #0x192]
0x250f50900:  28 01 00 34   cbz	w8, loc_250f50924 ; ⤵ 0x24
0x250f50904:  88 e6 45 39   ldrb	w8, [x20, #0x179]
0x250f50908:  e8 00 00 34   cbz	w8, loc_250f50924 ; ⤵ 0x1c
0x250f5090c:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50910:  08 a9 43 f9   ldr	x8, [x8, #0x750] ; _ptr.1e0c04950 (MediaToolbox/__DATA_CONST.__const)
0x250f50914:  01 01 40 f9   ldr	x1, [x8]
0x250f50918:  82 a2 40 f9   ldr	x2, [x20, #0x140]
0x250f5091c:  e0 03 15 aa   mov	x0, x21
0x250f50920:  28 c7 c6 95   bl	j__CFDictionarySetValue
0x250f50924:  ; loc_250f50924
0x250f50924:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50928:  08 bd 43 f9   ldr	x8, [x8, #0x778] ; _ptr.1e0c04958 (MediaToolbox/__DATA_CONST.__const)
0x250f5092c:  01 01 40 f9   ldr	x1, [x8]
0x250f50930:  82 d2 41 b9   ldr	w2, [x20, #0x1d0]
0x250f50934:  ; loc_250f50934
0x250f50934:  e0 03 15 aa   mov	x0, x21
0x250f50938:  62 f4 c6 95   bl	j__FigCFDictionarySetInt32
0x250f5093c:  80 86 50 6d   ldp	d0, d1, [x20, #0x108]
0x250f50940:  c2 02 40 fd   ldr	d2, [x22]
0x250f50944:  00 20 62 1e   fcmp	d0, d2
0x250f50948:  81 00 00 54   b.ne	loc_250f50958 ; ⤵ 0x10
0x250f5094c:  c2 06 40 fd   ldr	d2, [x22, #0x8]
0x250f50950:  20 20 62 1e   fcmp	d1, d2
0x250f50954:  c0 00 00 54   b.eq	loc_250f5096c ; ⤵ 0x18
0x250f50958:  ; loc_250f50958
0x250f50958:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f5095c:  08 9d 43 f9   ldr	x8, [x8, #0x738] ; _ptr.1e0c04910 (MediaToolbox/__DATA_CONST.__const)
0x250f50960:  01 01 40 f9   ldr	x1, [x8]
0x250f50964:  e0 03 15 aa   mov	x0, x21
0x250f50968:  4a f4 c6 95   bl	j__FigCFDictionarySetCGSize
0x250f5096c:  ; loc_250f5096c
0x250f5096c:  88 0e 46 39   ldrb	w8, [x20, #0x183]
0x250f50970:  e8 01 00 34   cbz	w8, loc_250f509ac ; ⤵ 0x3c
0x250f50974:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50978:  08 c1 43 f9   ldr	x8, [x8, #0x780] ; _ptr.1e0c04918 (MediaToolbox/__DATA_CONST.__const)
0x250f5097c:  01 01 40 f9   ldr	x1, [x8]
0x250f50980:  80 8e 40 fd   ldr	d0, [x20, #0x118]
0x250f50984:  e0 03 15 aa   mov	x0, x21
0x250f50988:  46 f4 c6 95   bl	j__FigCFDictionarySetDouble
0x250f5098c:  82 22 41 b9   ldr	w2, [x20, #0x120]
0x250f50990:  5f 04 00 71   cmp	w2, #0x1
0x250f50994:  cb 00 00 54   b.lt	loc_250f509ac ; ⤵ 0x18
0x250f50998:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f5099c:  08 b1 43 f9   ldr	x8, [x8, #0x760] ; _ptr.1e0c04960 (MediaToolbox/__DATA_CONST.__const)
0x250f509a0:  01 01 40 f9   ldr	x1, [x8]
0x250f509a4:  e0 03 15 aa   mov	x0, x21
0x250f509a8:  46 f4 c6 95   bl	j__FigCFDictionarySetInt32
0x250f509ac:  ; loc_250f509ac
0x250f509ac:  82 9a 40 f9   ldr	x2, [x20, #0x130]
0x250f509b0:  c2 00 00 b4   cbz	x2, loc_250f509c8 ; ⤵ 0x18
0x250f509b4:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f509b8:  08 99 43 f9   ldr	x8, [x8, #0x730] ; _ptr.1e0c04930 (MediaToolbox/__DATA_CONST.__const)
0x250f509bc:  01 01 40 f9   ldr	x1, [x8]
0x250f509c0:  e0 03 15 aa   mov	x0, x21
0x250f509c4:  ff c6 c6 95   bl	j__CFDictionarySetValue
0x250f509c8:  ; loc_250f509c8
0x250f509c8:  82 6a 40 f9   ldr	x2, [x20, #0xd0]
0x250f509cc:  c2 00 00 b4   cbz	x2, loc_250f509e4 ; ⤵ 0x18
0x250f509d0:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f509d4:  08 95 43 f9   ldr	x8, [x8, #0x728] ; _ptr.1e0c04948 (MediaToolbox/__DATA_CONST.__const)
0x250f509d8:  01 01 40 f9   ldr	x1, [x8]
0x250f509dc:  e0 03 15 aa   mov	x0, x21
0x250f509e0:  f8 c6 c6 95   bl	j__CFDictionarySetValue
0x250f509e4:  ; loc_250f509e4
0x250f509e4:  82 9e 40 f9   ldr	x2, [x20, #0x138]
0x250f509e8:  c2 00 00 b4   cbz	x2, loc_250f50a00 ; ⤵ 0x18
0x250f509ec:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f509f0:  08 a5 43 f9   ldr	x8, [x8, #0x748] ; _ptr.1e0c04938 (MediaToolbox/__DATA_CONST.__const)
0x250f509f4:  01 01 40 f9   ldr	x1, [x8]
0x250f509f8:  e0 03 15 aa   mov	x0, x21
0x250f509fc:  f1 c6 c6 95   bl	j__CFDictionarySetValue
0x250f50a00:  ; loc_250f50a00
0x250f50a00:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50a04:  08 ad 43 f9   ldr	x8, [x8, #0x758] ; _ptr.1e0c04920 (MediaToolbox/__DATA_CONST.__const)
0x250f50a08:  01 01 40 f9   ldr	x1, [x8]
0x250f50a0c:  88 c6 45 39   ldrb	w8, [x20, #0x171]
0x250f50a10:  89 8b 0b f0   adrp	x9, 0x2680c3000
0x250f50a14:  29 f5 40 f9   ldr	x9, [x9, #0x1e8] ; _ptr.1e0549de0 (CoreFoundation/__DATA_CONST.__const)
0x250f50a18:  36 01 40 f9   ldr	x22, [x9]
0x250f50a1c:  89 8b 0b f0   adrp	x9, 0x2680c3000
0x250f50a20:  29 ed 40 f9   ldr	x9, [x9, #0x1d8] ; _ptr.1e0549de8 (CoreFoundation/__DATA_CONST.__const)
0x250f50a24:  37 01 40 f9   ldr	x23, [x9]
0x250f50a28:  1f 01 00 71   cmp	w8, #0
0x250f50a2c:  e2 02 96 9a   csel	x2, x23, x22, eq
0x250f50a30:  e0 03 15 aa   mov	x0, x21
0x250f50a34:  e3 c6 c6 95   bl	j__CFDictionarySetValue
0x250f50a38:  48 8c 0b f0   adrp	x8, 0x2680db000
0x250f50a3c:  08 b5 43 f9   ldr	x8, [x8, #0x768] ; _ptr.1e0c04940 (MediaToolbox/__DATA_CONST.__const)
0x250f50a40:  01 01 40 f9   ldr	x1, [x8]
0x250f50a44:  88 ca 45 39   ldrb	w8, [x20, #0x172]
0x250f50a48:  1f 01 00 71   cmp	w8, #0
0x250f50a4c:  e2 02 96 9a   csel	x2, x23, x22, eq
0x250f50a50:  e0 03 15 aa   mov	x0, x21
0x250f50a54:  db c6 c6 95   bl	j__CFDictionarySetValue
0x250f50a58:  00 00 80 52   mov	w0, #0
0x250f50a5c:  75 02 00 f9   str	x21, [x19]
0x250f50a60:  0d ff ff 17   b	loc_250f50694 ; ⤴ -0x3cc
0x250f50a64:  ; loc_250f50a64
0x250f50a64:  e8 21 0c f0   adrp	x8, 0x26938f000
0x250f50a68:  08 61 24 91   add	x8, x8, #0x918 ; _kAPEndpointStreamProperty_ShowInfo
0x250f50a6c:  01 01 40 f9   ldr	x1, [x8]
0x250f50a70:  e0 03 15 aa   mov	x0, x21
0x250f50a74:  33 c4 c6 95   bl	j__CFEqual
0x250f50a78:  20 01 00 34   cbz	w0, loc_250f50a9c ; ⤵ 0x24
0x250f50a7c:  e0 03 14 aa   mov	x0, x20
0x250f50a80:  47 1b 03 94   bl	_screenstreamudp_copyShowInfo
0x250f50a84:  01 ff ff 17   b	loc_250f50688 ; ⤴ -0x3fc
0x250f50a88:  ; loc_250f50a88
0x250f50a88:  a1 03 01 d1   sub	x1, fp, #0x40
0x250f50a8c:  a2 13 01 d1   sub	x2, fp, #0x44
0x250f50a90:  e0 03 14 aa   mov	x0, x20
0x250f50a94:  a4 9c 08 94   bl	_screenstream_CopyProperty.cold.2
0x250f50a98:  39 ff ff 17   b	loc_250f5077c ; ⤴ -0x31c
0x250f50a9c:  ; loc_250f50a9c
0x250f50a9c:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f50aa0:  08 b1 43 f9   ldr	x8, [x8, #0x760] ; _ptr.1e0a147e0 (CoreMedia/__DATA_CONST.__const)
0x250f50aa4:  01 01 40 f9   ldr	x1, [x8]
0x250f50aa8:  e0 03 15 aa   mov	x0, x21
0x250f50aac:  25 c4 c6 95   bl	j__CFEqual
0x250f50ab0:  80 00 00 34   cbz	w0, loc_250f50ac0 ; ⤵ 0x10
0x250f50ab4:  e0 03 14 aa   mov	x0, x20
0x250f50ab8:  bc 45 06 94   bl	_screenstream_copyOutputLatency
0x250f50abc:  f3 fe ff 17   b	loc_250f50688 ; ⤴ -0x434
0x250f50ac0:  ; loc_250f50ac0
0x250f50ac0:  a8 8b 0b b0   adrp	x8, 0x2680c5000
0x250f50ac4:  08 e9 43 f9   ldr	x8, [x8, #0x7d0] ; _ptr.1e0a147b0 (CoreMedia/__DATA_CONST.__const)
0x250f50ac8:  01 01 40 f9   ldr	x1, [x8]
0x250f50acc:  e0 03 15 aa   mov	x0, x21
0x250f50ad0:  1c c4 c6 95   bl	j__CFEqual
0x250f50ad4:  00 03 00 34   cbz	w0, loc_250f50b34 ; ⤵ 0x60
0x250f50ad8:  80 fa 40 f9   ldr	x0, [x20, #0x1f0]
0x250f50adc:  e8 23 01 91   add	x8, sp, #0x48
0x250f50ae0:  b0 92 0b 90   adrp	x16, 0x2681a4000
0x250f50ae4:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250f50ae8:  f1 03 08 aa   mov	x17, x8
0x250f50aec:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250f50af0:  30 0a c1 da   pacda	x16, x17
0x250f50af4:  f0 27 00 f9   str	x16, [sp, #0x48]
0x250f50af8:  c9 15 00 b0   adrp	x9, 0x251209000
0x250f50afc:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x250f50b00:  e0 2b 00 fd   str	d0, [sp, #0x50]
0x250f50b04:  08 41 00 91   add	x8, x8, #0x10
0x250f50b08:  29 22 0c d0   adrp	x9, 0x269396000
0x250f50b0c:  29 c1 21 91   add	x9, x9, #0x870 ; ___block_descriptor_tmp.73
0x250f50b10:  90 0c 00 d0   adrp	x16, 0x2510e2000
0x250f50b14:  10 92 08 91   add	x16, x16, #0x224 ; ___screenstream_CopyProperty_block_invoke
0x250f50b18:  10 01 c1 da   pacia	x16, x8
0x250f50b1c:  f0 a7 05 a9   stp	x16, x9, [sp, #0x58]
0x250f50b20:  f3 d3 06 a9   stp	x19, x20, [sp, #0x68]
0x250f50b24:  e1 23 01 91   add	x1, sp, #0x48
0x250f50b28:  4a c4 c6 95   bl	j__dispatch_sync
0x250f50b2c:  ; loc_250f50b2c
0x250f50b2c:  00 00 80 52   mov	w0, #0
0x250f50b30:  d9 fe ff 17   b	loc_250f50694 ; ⤴ -0x49c
0x250f50b34:  ; loc_250f50b34
0x250f50b34:  41 5d 12 f0   adrp	x1, 0x275afb000
0x250f50b38:  21 80 31 91   add	x1, x1, #0xc60 ; "screenStreamType"
0x250f50b3c:  e0 03 15 aa   mov	x0, x21
0x250f50b40:  00 c4 c6 95   bl	j__CFEqual
0x250f50b44:  80 00 00 34   cbz	w0, loc_250f50b54 ; ⤵ 0x10
0x250f50b48:  c0 5d 12 d0   adrp	x0, 0x275b0a000
0x250f50b4c:  00 80 06 91   add	x0, x0, #0x1a0 ; "screenStreamTCP"
0x250f50b50:  cd fe ff 17   b	loc_250f50684 ; ⤴ -0x4cc
0x250f50b54:  ; loc_250f50b54
0x250f50b54:  c1 5d 12 d0   adrp	x1, 0x275b0a000
0x250f50b58:  21 00 07 91   add	x1, x1, #0x1c0 ; "transportGlitchCount"
0x250f50b5c:  e0 03 15 aa   mov	x0, x21
0x250f50b60:  f8 c3 c6 95   bl	j__CFEqual
0x250f50b64:  a0 00 00 34   cbz	w0, loc_250f50b78 ; ⤵ 0x14
0x250f50b68:  88 a2 12 91   add	x8, x20, #0x4a8
0x250f50b6c:  00 fd df 88   ldar	w0, [x8]
0x250f50b70:  78 f1 c6 95   bl	j__CFNumberCreateInt64
0x250f50b74:  c5 fe ff 17   b	loc_250f50688 ; ⤴ -0x4ec
0x250f50b78:  ; loc_250f50b78
0x250f50b78:  c1 5d 12 d0   adrp	x1, 0x275b0a000
0x250f50b7c:  21 80 07 91   add	x1, x1, #0x1e0 ; "usageMode"
0x250f50b80:  e0 03 15 aa   mov	x0, x21
0x250f50b84:  ef c3 c6 95   bl	j__CFEqual
0x250f50b88:  a0 00 00 34   cbz	w0, loc_250f50b9c ; ⤵ 0x14
0x250f50b8c:  80 5e 40 f9   ldr	x0, [x20, #0xb8]
0x250f50b90:  a0 d7 ff b5   cbnz	x0, loc_250f50684 ; ⤴ -0x50c
0x250f50b94:  08 00 80 d2   mov	x8, #0
0x250f50b98:  bd fe ff 17   b	loc_250f5068c ; ⤴ -0x50c
0x250f50b9c:  ; loc_250f50b9c
0x250f50b9c:  e8 21 0c f0   adrp	x8, 0x26938f000
0x250f50ba0:  08 41 1f 91   add	x8, x8, #0x7d0 ; _kAPEndpointStreamProperty_RTCStats
0x250f50ba4:  01 01 40 f9   ldr	x1, [x8]
0x250f50ba8:  e0 03 15 aa   mov	x0, x21
0x250f50bac:  e5 c3 c6 95   bl	j__CFEqual
0x250f50bb0:  80 03 00 34   cbz	w0, loc_250f50c20 ; ⤵ 0x70
0x250f50bb4:  80 42 42 f9   ldr	x0, [x20, #0x480]
0x250f50bb8:  e8 43 00 91   add	x8, sp, #0x10
0x250f50bbc:  b0 92 0b 90   adrp	x16, 0x2681a4000
0x250f50bc0:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250f50bc4:  f1 03 08 aa   mov	x17, x8
0x250f50bc8:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250f50bcc:  30 0a c1 da   pacda	x16, x17
0x250f50bd0:  f0 0b 00 f9   str	x16, [sp, #0x10]
0x250f50bd4:  c9 15 00 b0   adrp	x9, 0x251209000
0x250f50bd8:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x250f50bdc:  e0 0f 00 fd   str	d0, [sp, #0x18]
0x250f50be0:  08 41 00 91   add	x8, x8, #0x10
0x250f50be4:  90 0c 00 d0   adrp	x16, 0x2510e2000
0x250f50be8:  10 72 09 91   add	x16, x16, #0x25c ; ___screenstream_CopyProperty_block_invoke_2
0x250f50bec:  10 01 c1 da   pacia	x16, x8
0x250f50bf0:  28 22 0c d0   adrp	x8, 0x269396000
0x250f50bf4:  08 41 22 91   add	x8, x8, #0x890 ; ___block_descriptor_tmp.86
0x250f50bf8:  f0 23 02 a9   stp	x16, x8, [sp, #0x20]
0x250f50bfc:  f3 5b 03 a9   stp	x19, x22, [sp, #0x30]
0x250f50c00:  f4 23 00 f9   str	x20, [sp, #0x40]
0x250f50c04:  e1 43 00 91   add	x1, sp, #0x10
0x250f50c08:  12 c4 c6 95   bl	j__dispatch_sync
0x250f50c0c:  68 02 40 f9   ldr	x8, [x19]
0x250f50c10:  e8 f8 ff b5   cbnz	x8, loc_250f50b2c ; ⤴ -0xe4
0x250f50c14:  56 9c 08 94   bl	_screenstream_CopyProperty.cold.3
0x250f50c18:  00 2f 88 12   mov	w0, #0xffffbe87
0x250f50c1c:  9e fe ff 17   b	loc_250f50694 ; ⤴ -0x588
0x250f50c20:  ; loc_250f50c20
0x250f50c20:  e8 21 0c f0   adrp	x8, 0x26938f000
0x250f50c24:  08 c1 21 91   add	x8, x8, #0x870 ; _kAPEndpointStreamProperty_StreamDescriptor
0x250f50c28:  01 01 40 f9   ldr	x1, [x8]
0x250f50c2c:  e0 03 15 aa   mov	x0, x21
0x250f50c30:  c4 c3 c6 95   bl	j__CFEqual
0x250f50c34:  e0 01 00 34   cbz	w0, loc_250f50c70 ; ⤵ 0x3c
0x250f50c38:  80 5a 40 b9   ldr	w0, [x20, #0x58]
0x250f50c3c:  01 00 80 52   mov	w1, #0
0x250f50c40:  02 00 80 d2   mov	x2, #0
0x250f50c44:  e3 03 13 aa   mov	x3, x19
0x250f50c48:  fd 7b 4c a9   ldp	fp, lr, [sp, #0xc0]
0x250f50c4c:  f4 4f 4b a9   ldp	x20, x19, [sp, #0xb0]
0x250f50c50:  f6 57 4a a9   ldp	x22, x21, [sp, #0xa0]
0x250f50c54:  f8 5f 49 a9   ldp	x24, x23, [sp, #0x90]
0x250f50c58:  ff 43 03 91   add	sp, sp, #0xd0
0x250f50c5c:  ff 23 03 d5   autibsp
0x250f50c60:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250f50c64:  50 00 f0 b6   tbz	x16, #0x3e, loc_250f50c6c ; ⤵ 0x8
0x250f50c68:  20 8e 38 d4   brk	#0xc471
0x250f50c6c:  ; loc_250f50c6c
0x250f50c6c:  f7 fd 03 14   b	_APSenderSessionCreateStreamDescriptor
0x250f50c70:  ; loc_250f50c70
0x250f50c70:  e8 32 4b b9   ldr	w8, [x23, #0xb30]
0x250f50c74:  1f 79 00 71   cmp	w8, #0x1e
0x250f50c78:  ac 02 00 54   b.gt	loc_250f50ccc ; ⤵ 0x54
0x250f50c7c:  1f 05 00 31   cmn	w8, #0x1
0x250f50c80:  a0 01 00 54   b.eq	loc_250f50cb4 ; ⤵ 0x34
0x250f50c84:  ; loc_250f50c84
0x250f50c84:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f50c88:  e8 57 00 a9   stp	x8, x21, [sp]
0x250f50c8c:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f50c90:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f50c94:  a1 15 00 b0   adrp	x1, 0x251205000
0x250f50c98:  21 b0 26 91   add	x1, x1, #0x9ac ; "OSStatus screenstream_CopyProperty(CMBaseObjectRef, CFStringRef, CFAllocatorRef, void *)"
0x250f50c9c:  a3 15 00 b0   adrp	x3, 0x251205000
0x250f50ca0:  63 a4 29 91   add	x3, x3, #0xa69 ; "%@ unknown property %@\n"
0x250f50ca4:  c2 03 80 52   mov	w2, #0x1e
0x250f50ca8:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f50cac:  95 c0 c6 95   bl	j__LogPrintF
0x250f50cb0:  07 00 00 14   b	loc_250f50ccc ; ⤵ 0x1c
0x250f50cb4:  ; loc_250f50cb4
0x250f50cb4:  e0 a9 0f d0   adrp	x0, 0x27048e000
0x250f50cb8:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f50cbc:  c1 03 80 52   mov	w1, #0x1e
0x250f50cc0:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f50cc4:  3f c0 c6 95   bl	j___LogCategory_Initialize
0x250f50cc8:  e0 fd ff 35   cbnz	w0, loc_250f50c84 ; ⤴ -0x44
0x250f50ccc:  ; loc_250f50ccc
0x250f50ccc:  e0 3d 86 12   mov	w0, #0xffffce10
0x250f50cd0:  71 fe ff 17   b	loc_250f50694 ; ⤴ -0x63c

