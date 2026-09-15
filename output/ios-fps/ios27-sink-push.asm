
_vdsink_PushFrame:
0x250f523fc:  7f 23 03 d5   pacibsp
0x250f52400:  ff 43 04 d1   sub	sp, sp, #0x110
0x250f52404:  ef 3b 07 6d   stp	d15, d14, [sp, #0x70]
0x250f52408:  ed 33 08 6d   stp	d13, d12, [sp, #0x80]
0x250f5240c:  eb 2b 09 6d   stp	d11, d10, [sp, #0x90]
0x250f52410:  e9 23 0a 6d   stp	d9, d8, [sp, #0xa0]
0x250f52414:  fc 6f 0b a9   stp	x28, x27, [sp, #0xb0]
0x250f52418:  fa 67 0c a9   stp	x26, x25, [sp, #0xc0]
0x250f5241c:  f8 5f 0d a9   stp	x24, x23, [sp, #0xd0]
0x250f52420:  f6 57 0e a9   stp	x22, x21, [sp, #0xe0]
0x250f52424:  f4 4f 0f a9   stp	x20, x19, [sp, #0xf0]
0x250f52428:  fd 7b 10 a9   stp	fp, lr, [sp, #0x100]
0x250f5242c:  fd 03 04 91   add	fp, sp, #0x100
0x250f52430:  e8 1c a7 4e   mov	v8.16b, v7.16b
0x250f52434:  c9 1c a6 4e   mov	v9.16b, v6.16b
0x250f52438:  aa 1c a5 4e   mov	v10.16b, v5.16b
0x250f5243c:  8b 1c a4 4e   mov	v11.16b, v4.16b
0x250f52440:  6c 1c a3 4e   mov	v12.16b, v3.16b
0x250f52444:  4d 1c a2 4e   mov	v13.16b, v2.16b
0x250f52448:  2e 1c a1 4e   mov	v14.16b, v1.16b
0x250f5244c:  0f 1c a0 4e   mov	v15.16b, v0.16b
0x250f52450:  f7 03 01 aa   mov	x23, x1
0x250f52454:  f8 03 00 aa   mov	x24, x0
0x250f52458:  e2 37 00 b9   str	w2, [sp, #0x34]
0x250f5245c:  59 ec c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f52460:  f9 03 00 aa   mov	x25, x0
0x250f52464:  01 80 44 6d   ldp	d1, d0, [x0, #0x48]
0x250f52468:  03 88 43 6d   ldp	d3, d2, [x0, #0x38]
0x250f5246c:  20 20 63 1e   fcmp	d1, d3
0x250f52470:  00 04 62 1e   fccmp	d0, d2, #0, eq
0x250f52474:  c0 01 00 54   b.eq	loc_250f524ac ; ⤵ 0x38
0x250f52478:  64 18 61 1e   fdiv	d4, d3, d1
0x250f5247c:  45 18 60 1e   fdiv	d5, d2, d0
0x250f52480:  80 20 65 1e   fcmp	d4, d5
0x250f52484:  84 4c 65 1e   fcsel	d4, d4, d5, mi
0x250f52488:  29 09 64 1e   fmul	d9, d9, d4
0x250f5248c:  08 09 64 1e   fmul	d8, d8, d4
0x250f52490:  21 8c 44 1f   fmsub	d1, d1, d4, d3
0x250f52494:  03 10 6c 1e   fmov	d3, #0.50000000
0x250f52498:  21 08 63 1e   fmul	d1, d1, d3
0x250f5249c:  00 88 44 1f   fmsub	d0, d0, d4, d2
0x250f524a0:  00 08 63 1e   fmul	d0, d0, d3
0x250f524a4:  6b 05 44 1f   fmadd	d11, d11, d4, d1
0x250f524a8:  4a 01 44 1f   fmadd	d10, d10, d4, d0
0x250f524ac:  ; loc_250f524ac
0x250f524ac:  e0 1d af 4e   mov	v0.16b, v15.16b
0x250f524b0:  c1 1d ae 4e   mov	v1.16b, v14.16b
0x250f524b4:  a2 1d ad 4e   mov	v2.16b, v13.16b
0x250f524b8:  83 1d ac 4e   mov	v3.16b, v12.16b
0x250f524bc:  2d ec c6 95   bl	j__CGRectCreateDictionaryRepresentation
0x250f524c0:  fb 03 00 aa   mov	x27, x0
0x250f524c4:  60 1d ab 4e   mov	v0.16b, v11.16b
0x250f524c8:  41 1d aa 4e   mov	v1.16b, v10.16b
0x250f524cc:  22 1d a9 4e   mov	v2.16b, v9.16b
0x250f524d0:  03 1d a8 4e   mov	v3.16b, v8.16b
0x250f524d4:  27 ec c6 95   bl	j__CGRectCreateDictionaryRepresentation
0x250f524d8:  f4 03 00 aa   mov	x20, x0
0x250f524dc:  88 8b 0b b0   adrp	x8, 0x2680c3000
0x250f524e0:  08 d1 40 f9   ldr	x8, [x8, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x250f524e4:  16 01 40 f9   ldr	x22, [x8]
0x250f524e8:  e2 d3 00 91   add	x2, sp, #0x34
0x250f524ec:  e0 03 16 aa   mov	x0, x22
0x250f524f0:  61 00 80 52   mov	w1, #0x3
0x250f524f4:  43 c0 c6 95   bl	j__CFNumberCreate
0x250f524f8:  fc 03 00 aa   mov	x28, x0
0x250f524fc:  88 8b 0b f0   adrp	x8, 0x2680c5000
0x250f52500:  08 d9 44 f9   ldr	x8, [x8, #0x9b0] ; _ptr.1e0a0cf10 (CoreMedia/__DATA_CONST.__const)
0x250f52504:  01 01 40 f9   ldr	x1, [x8]
0x250f52508:  e0 03 17 aa   mov	x0, x23
0x250f5250c:  e2 03 1b aa   mov	x2, x27
0x250f52510:  03 00 80 52   mov	w3, #0
0x250f52514:  6f ec c6 95   bl	j__CMSetAttachment
0x250f52518:  88 8b 0b f0   adrp	x8, 0x2680c5000
0x250f5251c:  08 c9 44 f9   ldr	x8, [x8, #0x990] ; _ptr.1e0a0cf18 (CoreMedia/__DATA_CONST.__const)
0x250f52520:  01 01 40 f9   ldr	x1, [x8]
0x250f52524:  e0 03 17 aa   mov	x0, x23
0x250f52528:  e2 03 14 aa   mov	x2, x20
0x250f5252c:  03 00 80 52   mov	w3, #0
0x250f52530:  68 ec c6 95   bl	j__CMSetAttachment
0x250f52534:  88 8b 0b f0   adrp	x8, 0x2680c5000
0x250f52538:  08 dd 44 f9   ldr	x8, [x8, #0x9b8] ; _ptr.1e0a0cf28 (CoreMedia/__DATA_CONST.__const)
0x250f5253c:  01 01 40 f9   ldr	x1, [x8]
0x250f52540:  e0 03 17 aa   mov	x0, x23
0x250f52544:  e2 03 1c aa   mov	x2, x28
0x250f52548:  03 00 80 52   mov	w3, #0
0x250f5254c:  61 ec c6 95   bl	j__CMSetAttachment
0x250f52550:  22 33 40 f9   ldr	x2, [x25, #0x60]
0x250f52554:  42 01 00 b4   cbz	x2, loc_250f5257c ; ⤵ 0x28
0x250f52558:  61 5d 12 d0   adrp	x1, 0x275b00000
0x250f5255c:  21 80 2a 91   add	x1, x1, #0xaa0 ; "APTimestamps"
0x250f52560:  e0 03 17 aa   mov	x0, x23
0x250f52564:  03 00 80 52   mov	w3, #0
0x250f52568:  5a ec c6 95   bl	j__CMSetAttachment
0x250f5256c:  20 33 40 f9   ldr	x0, [x25, #0x60]
0x250f52570:  60 00 00 b4   cbz	x0, loc_250f5257c ; ⤵ 0xc
0x250f52574:  7f bd c6 95   bl	j__CFRelease
0x250f52578:  3f 33 00 f9   str	xzr, [x25, #0x60]
0x250f5257c:  ; loc_250f5257c
0x250f5257c:  22 37 40 f9   ldr	x2, [x25, #0x68]
0x250f52580:  42 01 00 b4   cbz	x2, loc_250f525a8 ; ⤵ 0x28
0x250f52584:  61 5d 12 d0   adrp	x1, 0x275b00000
0x250f52588:  21 00 2b 91   add	x1, x1, #0xac0 ; "APSubmitSurfaceTimestamp"
0x250f5258c:  e0 03 17 aa   mov	x0, x23
0x250f52590:  03 00 80 52   mov	w3, #0
0x250f52594:  4f ec c6 95   bl	j__CMSetAttachment
0x250f52598:  20 37 40 f9   ldr	x0, [x25, #0x68]
0x250f5259c:  60 00 00 b4   cbz	x0, loc_250f525a8 ; ⤵ 0xc
0x250f525a0:  74 bd c6 95   bl	j__CFRelease
0x250f525a4:  3f 37 00 f9   str	xzr, [x25, #0x68]
0x250f525a8:  ; loc_250f525a8
0x250f525a8:  22 3b 40 f9   ldr	x2, [x25, #0x70]
0x250f525ac:  42 01 00 b4   cbz	x2, loc_250f525d4 ; ⤵ 0x28
0x250f525b0:  61 5d 12 d0   adrp	x1, 0x275b00000
0x250f525b4:  21 80 2b 91   add	x1, x1, #0xae0 ; "APInterestingFrame"
0x250f525b8:  e0 03 17 aa   mov	x0, x23
0x250f525bc:  03 00 80 52   mov	w3, #0
0x250f525c0:  44 ec c6 95   bl	j__CMSetAttachment
0x250f525c4:  20 3b 40 f9   ldr	x0, [x25, #0x70]
0x250f525c8:  60 00 00 b4   cbz	x0, loc_250f525d4 ; ⤵ 0xc
0x250f525cc:  69 bd c6 95   bl	j__CFRelease
0x250f525d0:  3f 3b 00 f9   str	xzr, [x25, #0x70]
0x250f525d4:  ; loc_250f525d4
0x250f525d4:  20 0b 40 f9   ldr	x0, [x25, #0x10]
0x250f525d8:  e1 03 17 aa   mov	x1, x23
0x250f525dc:  f1 fb c6 95   bl	j__CMBufferQueueEnqueue
0x250f525e0:  15 c0 0e d0   adrp	x21, 0x26e754000
0x250f525e4:  a8 3a 4f b9   ldr	w8, [x21, #0xf38] ; _gLogCategory_APVirtualDisplaySink
0x250f525e8:  1f 29 00 71   cmp	w8, #0xa
0x250f525ec:  cc 02 00 54   b.gt	loc_250f52644 ; ⤵ 0x58
0x250f525f0:  1f 05 00 31   cmn	w8, #0x1
0x250f525f4:  c1 00 00 54   b.ne	loc_250f5260c ; ⤵ 0x18
0x250f525f8:  66 6e 00 94   bl	_OUTLINED_FUNCTION_4
0x250f525fc:  41 01 80 52   mov	w1, #0xa
0x250f52600:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f52604:  ef b9 c6 95   bl	j___LogCategory_Initialize
0x250f52608:  e0 01 00 34   cbz	w0, loc_250f52644 ; ⤵ 0x3c
0x250f5260c:  ; loc_250f5260c
0x250f5260c:  28 07 40 f9   ldr	x8, [x25, #0x8]
0x250f52610:  1f 01 00 f1   cmp	x8, #0
0x250f52614:  29 5d 12 90   adrp	x9, 0x275af6000
0x250f52618:  29 81 18 91   add	x9, x9, #0x620
0x250f5261c:  28 01 88 9a   csel	x8, x9, x8, eq
0x250f52620:  e8 5f 00 a9   stp	x8, x23, [sp]
0x250f52624:  5b 6e 00 94   bl	_OUTLINED_FUNCTION_4
0x250f52628:  81 12 00 f0   adrp	x1, 0x2511a5000
0x250f5262c:  21 b4 3f 91   add	x1, x1, #0xfed ; "OSStatus vdsink_PushFrame(FigVirtualDisplaySinkRef, CMSampleBufferRef, CGRect, CGRect, uint32_t)"
0x250f52630:  43 13 00 b0   adrp	x3, 0x2511bb000
0x250f52634:  63 84 3c 91   add	x3, x3, #0xf21 ; "%@ pushed frame %@\n"
0x250f52638:  42 01 80 52   mov	w2, #0xa
0x250f5263c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f52640:  30 ba c6 95   bl	j__LogPrintF
0x250f52644:  ; loc_250f52644
0x250f52644:  e0 03 18 aa   mov	x0, x24
0x250f52648:  de eb c6 95   bl	j__CMBaseObjectGetDerivedStorage
0x250f5264c:  08 2c 40 f9   ldr	x8, [x0, #0x58]
0x250f52650:  08 0d 00 b4   cbz	x8, loc_250f527f0 ; ⤵ 0x1a0
0x250f52654:  f8 03 00 aa   mov	x24, x0
0x250f52658:  e0 03 17 aa   mov	x0, x23
0x250f5265c:  01 00 80 52   mov	w1, #0
0x250f52660:  18 ec c6 95   bl	j__CMSampleBufferGetSampleAttachmentsArray
0x250f52664:  60 0c 00 b4   cbz	x0, loc_250f527f0 ; ⤵ 0x18c
0x250f52668:  f9 03 00 aa   mov	x25, x0
0x250f5266c:  a5 bf c6 95   bl	j__CFArrayGetCount
0x250f52670:  1f 04 00 f1   cmp	x0, #0x1
0x250f52674:  eb 0b 00 54   b.lt	loc_250f527f0 ; ⤵ 0x17c
0x250f52678:  fa 03 00 aa   mov	x26, x0
0x250f5267c:  fc ef 01 a9   stp	x28, x27, [sp, #0x18]
0x250f52680:  f4 17 00 f9   str	x20, [sp, #0x28]
0x250f52684:  17 00 80 d2   mov	x23, #0
0x250f52688:  1b 00 80 d2   mov	x27, #0
0x250f5268c:  a8 8c 0b f0   adrp	x8, 0x2680e9000
0x250f52690:  08 65 40 f9   ldr	x8, [x8, #0xc8] ; _ptr.1e0a06b60 (VideoToolbox/__DATA_CONST.__const)
0x250f52694:  1c 01 40 f9   ldr	x28, [x8]
0x250f52698:  14 c0 0e d0   adrp	x20, 0x26e754000
0x250f5269c:  94 e2 3c 91   add	x20, x20, #0xf38 ; _gLogCategory_APVirtualDisplaySink
0x250f526a0:  ; loc_250f526a0
0x250f526a0:  e0 03 19 aa   mov	x0, x25
0x250f526a4:  e1 03 1b aa   mov	x1, x27
0x250f526a8:  9a bf c6 95   bl	j__CFArrayGetValueAtIndex
0x250f526ac:  e0 04 00 b4   cbz	x0, loc_250f52748 ; ⤵ 0x9c
0x250f526b0:  e1 03 1c aa   mov	x1, x28
0x250f526b4:  bf bf c6 95   bl	j__CFDictionaryGetValue
0x250f526b8:  80 04 00 b4   cbz	x0, loc_250f52748 ; ⤵ 0x90
0x250f526bc:  f3 03 00 aa   mov	x19, x0
0x250f526c0:  f7 00 00 b5   cbnz	x23, loc_250f526dc ; ⤵ 0x1c
0x250f526c4:  e0 03 16 aa   mov	x0, x22
0x250f526c8:  01 00 80 d2   mov	x1, #0
0x250f526cc:  82 8b 0b b0   adrp	x2, 0x2680c3000
0x250f526d0:  42 c0 41 f9   ldr	x2, [x2, #0x380] ; _ptr.1e8d90d00 (CoreFoundation/__AUTH_CONST.__const)
0x250f526d4:  83 bf c6 95   bl	j__CFArrayCreateMutable
0x250f526d8:  f7 03 00 aa   mov	x23, x0
0x250f526dc:  ; loc_250f526dc
0x250f526dc:  a8 3a 4f b9   ldr	w8, [x21, #0xf38]
0x250f526e0:  1f 29 00 71   cmp	w8, #0xa
0x250f526e4:  cc 02 00 54   b.gt	loc_250f5273c ; ⤵ 0x58
0x250f526e8:  1f 05 00 31   cmn	w8, #0x1
0x250f526ec:  c1 00 00 54   b.ne	loc_250f52704 ; ⤵ 0x18
0x250f526f0:  e0 03 14 aa   mov	x0, x20
0x250f526f4:  41 01 80 52   mov	w1, #0xa
0x250f526f8:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f526fc:  b1 b9 c6 95   bl	j___LogCategory_Initialize
0x250f52700:  e0 01 00 34   cbz	w0, loc_250f5273c ; ⤵ 0x3c
0x250f52704:  ; loc_250f52704
0x250f52704:  08 07 40 f9   ldr	x8, [x24, #0x8]
0x250f52708:  1f 01 00 f1   cmp	x8, #0
0x250f5270c:  29 5d 12 90   adrp	x9, 0x275af6000
0x250f52710:  29 81 18 91   add	x9, x9, #0x620
0x250f52714:  28 01 88 9a   csel	x8, x9, x8, eq
0x250f52718:  e8 4f 00 a9   stp	x8, x19, [sp]
0x250f5271c:  e0 03 14 aa   mov	x0, x20
0x250f52720:  a1 12 00 90   adrp	x1, 0x2511a6000
0x250f52724:  21 7c 01 91   add	x1, x1, #0x5f ; "void vdsink_handleFrameAcknowledgements(FigVirtualDisplaySinkRef, CMSampleBufferRef)"
0x250f52728:  42 01 80 52   mov	w2, #0xa
0x250f5272c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f52730:  43 13 00 b0   adrp	x3, 0x2511bb000
0x250f52734:  63 d4 3c 91   add	x3, x3, #0xf35 ; "%@ acknowledging token %@\n"
0x250f52738:  f2 b9 c6 95   bl	j__LogPrintF
0x250f5273c:  ; loc_250f5273c
0x250f5273c:  e0 03 17 aa   mov	x0, x23
0x250f52740:  e1 03 13 aa   mov	x1, x19
0x250f52744:  63 bf c6 95   bl	j__CFArrayAppendValue
0x250f52748:  ; loc_250f52748
0x250f52748:  7b 07 00 91   add	x27, x27, #0x1
0x250f5274c:  5f 03 1b eb   cmp	x26, x27
0x250f52750:  81 fa ff 54   b.ne	loc_250f526a0 ; ⤴ -0xb0
0x250f52754:  fb 53 42 a9   ldp	x27, x20, [sp, #0x20]
0x250f52758:  fc 0f 40 f9   ldr	x28, [sp, #0x18]
0x250f5275c:  b7 04 00 b4   cbz	x23, loc_250f527f0 ; ⤵ 0x94
0x250f52760:  82 8b 0b b0   adrp	x2, 0x2680c3000
0x250f52764:  42 c8 41 f9   ldr	x2, [x2, #0x390] ; _ptr.1e8d91d98 (CoreFoundation/__AUTH_CONST.__const)
0x250f52768:  83 8b 0b b0   adrp	x3, 0x2680c3000
0x250f5276c:  63 cc 41 f9   ldr	x3, [x3, #0x398] ; _ptr.1e8d91df8 (CoreFoundation/__AUTH_CONST.__const)
0x250f52770:  e0 03 16 aa   mov	x0, x22
0x250f52774:  01 00 80 d2   mov	x1, #0
0x250f52778:  7e bf c6 95   bl	j__CFDictionaryCreateMutable
0x250f5277c:  f3 03 00 aa   mov	x19, x0
0x250f52780:  48 8c 0b b0   adrp	x8, 0x2680db000
0x250f52784:  08 c9 43 f9   ldr	x8, [x8, #0x790] ; _ptr.1e0c048e8 (MediaToolbox/__DATA_CONST.__const)
0x250f52788:  01 01 40 f9   ldr	x1, [x8]
0x250f5278c:  e2 03 17 aa   mov	x2, x23
0x250f52790:  8c bf c6 95   bl	j__CFDictionarySetValue
0x250f52794:  00 0f 40 f9   ldr	x0, [x24, #0x18]
0x250f52798:  e8 e3 00 91   add	x8, sp, #0x38
0x250f5279c:  90 92 0b d0   adrp	x16, 0x2681a4000
0x250f527a0:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250f527a4:  f1 03 08 aa   mov	x17, x8
0x250f527a8:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250f527ac:  30 0a c1 da   pacda	x16, x17
0x250f527b0:  f0 1f 00 f9   str	x16, [sp, #0x38]
0x250f527b4:  a9 15 00 f0   adrp	x9, 0x251209000
0x250f527b8:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x250f527bc:  e0 23 00 fd   str	d0, [sp, #0x40]
0x250f527c0:  08 41 00 91   add	x8, x8, #0x10
0x250f527c4:  09 22 0c 90   adrp	x9, 0x269392000
0x250f527c8:  29 01 1c 91   add	x9, x9, #0x700 ; ___block_descriptor_tmp.48
0x250f527cc:  10 00 00 b0   adrp	x16, 0x250f53000
0x250f527d0:  10 d2 00 91   add	x16, x16, #0x34 ; ___vdsink_handleFrameAcknowledgements_block_invoke
0x250f527d4:  10 01 c1 da   pacia	x16, x8
0x250f527d8:  f0 a7 04 a9   stp	x16, x9, [sp, #0x48]
0x250f527dc:  f8 cf 05 a9   stp	x24, x19, [sp, #0x58]
0x250f527e0:  e1 e3 00 91   add	x1, sp, #0x38
0x250f527e4:  27 b9 c6 95   bl	j__dispatch_async
0x250f527e8:  e0 03 17 aa   mov	x0, x23
0x250f527ec:  e1 bc c6 95   bl	j__CFRelease
0x250f527f0:  ; loc_250f527f0
0x250f527f0:  7b 00 00 b4   cbz	x27, loc_250f527fc ; ⤵ 0xc
0x250f527f4:  e0 03 1b aa   mov	x0, x27
0x250f527f8:  de bc c6 95   bl	j__CFRelease
0x250f527fc:  ; loc_250f527fc
0x250f527fc:  74 00 00 b4   cbz	x20, loc_250f52808 ; ⤵ 0xc
0x250f52800:  e0 03 14 aa   mov	x0, x20
0x250f52804:  db bc c6 95   bl	j__CFRelease
0x250f52808:  ; loc_250f52808
0x250f52808:  7c 00 00 b4   cbz	x28, loc_250f52814 ; ⤵ 0xc
0x250f5280c:  e0 03 1c aa   mov	x0, x28
0x250f52810:  d8 bc c6 95   bl	j__CFRelease
0x250f52814:  ; loc_250f52814
0x250f52814:  00 00 80 52   mov	w0, #0
0x250f52818:  fd 7b 50 a9   ldp	fp, lr, [sp, #0x100]
0x250f5281c:  f4 4f 4f a9   ldp	x20, x19, [sp, #0xf0]
0x250f52820:  f6 57 4e a9   ldp	x22, x21, [sp, #0xe0]
0x250f52824:  f8 5f 4d a9   ldp	x24, x23, [sp, #0xd0]
0x250f52828:  fa 67 4c a9   ldp	x26, x25, [sp, #0xc0]
0x250f5282c:  fc 6f 4b a9   ldp	x28, x27, [sp, #0xb0]
0x250f52830:  e9 23 4a 6d   ldp	d9, d8, [sp, #0xa0]
0x250f52834:  eb 2b 49 6d   ldp	d11, d10, [sp, #0x90]
0x250f52838:  ed 33 48 6d   ldp	d13, d12, [sp, #0x80]
0x250f5283c:  ef 3b 47 6d   ldp	d15, d14, [sp, #0x70]
0x250f52840:  ff 43 04 91   add	sp, sp, #0x110
0x250f52844:  ff 0f 5f d6   retab

