
-[APRKMediaPlayer initWithP2PWiFiSupport:isInterstitialPlayer:playerSessionID:]
0x250eeb464:  7f 23 03 d5   pacibsp
0x250eeb468:  ff 83 02 d1   sub	sp, sp, #0xa0
0x250eeb46c:  fa 67 05 a9   stp	x26, x25, [sp, #0x50]
0x250eeb470:  f8 5f 06 a9   stp	x24, x23, [sp, #0x60]
0x250eeb474:  f6 57 07 a9   stp	x22, x21, [sp, #0x70]
0x250eeb478:  f4 4f 08 a9   stp	x20, x19, [sp, #0x80]
0x250eeb47c:  fd 7b 09 a9   stp	fp, lr, [sp, #0x90]
0x250eeb480:  fd 43 02 91   add	fp, sp, #0x90
0x250eeb484:  f4 03 04 aa   mov	x20, x4
0x250eeb488:  f6 03 03 aa   mov	x22, x3
0x250eeb48c:  f5 03 02 aa   mov	x21, x2
0x250eeb490:  08 25 0c d0   adrp	x8, 0x26938d000
0x250eeb494:  08 25 45 f9   ldr	x8, [x8, #0xa48] ; "\xb8gO\xf1"
0x250eeb498:  e0 23 04 a9   stp	x0, x8, [sp, #0x40]
0x250eeb49c:  68 62 d2 d0   adrp	x8, 0x1f5b39000
0x250eeb4a0:  01 a1 34 91   add	x1, x8, #0xd28 ; init
0x250eeb4a4:  e0 03 01 91   add	x0, sp, #0x40
0x250eeb4a8:  2e 54 c8 95   bl	_objc_msgSendSuper2_stub
0x250eeb4ac:  f3 03 00 aa   mov	x19, x0
0x250eeb4b0:  e0 18 00 b4   cbz	x0, loc_250eeb7cc ; ⤵ 0x31c
0x250eeb4b4:  7f 16 00 f9   str	xzr, [x19, #0x28]
0x250eeb4b8:  20 01 00 d0   adrp	x0, 0x250f11000
0x250eeb4bc:  00 ac 28 91   add	x0, x0, #0xa2b ; "com.apple.APRKMediaPlayer.messageQueue"
0x250eeb4c0:  01 00 80 d2   mov	x1, #0
0x250eeb4c4:  57 56 c8 95   bl	j__dispatch_queue_create
0x250eeb4c8:  68 1e 40 f9   ldr	x8, [x19, #0x38]
0x250eeb4cc:  60 1e 00 f9   str	x0, [x19, #0x38]
0x250eeb4d0:  58 54 c8 95   bl	j__objc_release_x8
0x250eeb4d4:  08 90 0b d0   adrp	x8, 0x2680ed000
0x250eeb4d8:  00 0d 45 f9   ldr	x0, [x8, #0xa18] ; _ptr.1e70a5d20 (AVFCore/__DATA_DIRTY.__objc_data)
0x250eeb4dc:  61 56 c8 95   bl	j__objc_alloc_init
0x250eeb4e0:  68 06 40 f9   ldr	x8, [x19, #0x8]
0x250eeb4e4:  60 06 00 f9   str	x0, [x19, #0x8]
0x250eeb4e8:  52 54 c8 95   bl	j__objc_release_x8
0x250eeb4ec:  60 06 40 f9   ldr	x0, [x19, #0x8]
0x250eeb4f0:  02 00 80 52   mov	w2, #0
0x250eeb4f4:  53 d9 c4 95   bl	0x258021a40
0x250eeb4f8:  76 22 03 39   strb	w22, [x19, #0xc8]
0x250eeb4fc:  68 82 40 f9   ldr	x8, [x19, #0x100]
0x250eeb500:  7f 82 00 f9   str	xzr, [x19, #0x100]
0x250eeb504:  4b 54 c8 95   bl	j__objc_release_x8
0x250eeb508:  60 22 04 91   add	x0, x19, #0x108
0x250eeb50c:  01 00 80 d2   mov	x1, #0
0x250eeb510:  68 58 c8 95   bl	j__objc_storeWeak
0x250eeb514:  7f f2 00 b9   str	wzr, [x19, #0xf0]
0x250eeb518:  b9 8e 0b f0   adrp	x25, 0x2680c2000
0x250eeb51c:  20 ab 47 f9   ldr	x0, [x25, #0xf50] ; _ptr.1e6f3da48 (CoreFoundation/__DATA_DIRTY.__objc_data)
0x250eeb520:  50 56 c8 95   bl	j__objc_alloc_init
0x250eeb524:  68 7e 40 f9   ldr	x8, [x19, #0xf8]
0x250eeb528:  60 7e 00 f9   str	x0, [x19, #0xf8]
0x250eeb52c:  41 54 c8 95   bl	j__objc_release_x8
0x250eeb530:  68 22 43 39   ldrb	w8, [x19, #0xc8]
0x250eeb534:  88 04 00 36   tbz	w8, #0, loc_250eeb5c4 ; ⤵ 0x90
0x250eeb538:  68 8a 40 f9   ldr	x8, [x19, #0x110]
0x250eeb53c:  7f 8a 00 f9   str	xzr, [x19, #0x110]
0x250eeb540:  3c 54 c8 95   bl	j__objc_release_x8
0x250eeb544:  17 8b c8 95   bl	j__IsAppleInternalBuild
0x250eeb548:  c0 0b 00 34   cbz	w0, loc_250eeb6c0 ; ⤵ 0x178
0x250eeb54c:  c5 81 c8 95   bl	j__APGetSignpostsLogHandle
0x250eeb550:  d4 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb554:  f6 03 00 aa   mov	x22, x0
0x250eeb558:  c2 81 c8 95   bl	j__APGetSignpostsLogHandle
0x250eeb55c:  d1 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb560:  f8 03 00 aa   mov	x24, x0
0x250eeb564:  43 8d c8 95   bl	j__os_signpost_id_generate
0x250eeb568:  f7 03 00 aa   mov	x23, x0
0x250eeb56c:  1d 54 c8 95   bl	j__objc_release_x24
0x250eeb570:  e8 06 00 d1   sub	x8, x23, #0x1
0x250eeb574:  1f 0d 00 b1   cmn	x8, #0x3
0x250eeb578:  28 02 00 54   b.hi	loc_250eeb5bc ; ⤵ 0x44
0x250eeb57c:  e0 03 16 aa   mov	x0, x22
0x250eeb580:  34 5d c8 95   bl	j__os_signpost_enabled
0x250eeb584:  c0 01 00 34   cbz	w0, loc_250eeb5bc ; ⤵ 0x38
0x250eeb588:  ff 63 00 79   strh	wzr, [sp, #0x30]
0x250eeb58c:  e0 ff ff b0   adrp	x0, 0x250ee8000
0x250eeb590:  00 00 00 91   add	x0, x0, #0 ; "\xcf\xfa\xed\xfe\f"
0x250eeb594:  64 01 00 d0   adrp	x4, 0x250f19000
0x250eeb598:  84 54 3d 91   add	x4, x4, #0xf55 ; "AirPlayVideoCreatePlayer"
0x250eeb59c:  65 01 00 d0   adrp	x5, 0x250f19000
0x250eeb5a0:  a5 b8 3d 91   add	x5, x5, #0xf6e ; "Created interstitial player"
0x250eeb5a4:  e6 c3 00 91   add	x6, sp, #0x30
0x250eeb5a8:  e1 03 16 aa   mov	x1, x22
0x250eeb5ac:  02 00 80 52   mov	w2, #0
0x250eeb5b0:  e3 03 17 aa   mov	x3, x23
0x250eeb5b4:  47 00 80 52   mov	w7, #0x2
0x250eeb5b8:  aa 5c c8 95   bl	j___os_signpost_emit_with_name_impl
0x250eeb5bc:  ; loc_250eeb5bc
0x250eeb5bc:  01 54 c8 95   bl	j__objc_release_x22
0x250eeb5c0:  40 00 00 14   b	loc_250eeb6c0 ; ⤵ 0x100
0x250eeb5c4:  ; loc_250eeb5c4
0x250eeb5c4:  e0 c3 00 91   add	x0, sp, #0x30
0x250eeb5c8:  e1 03 13 aa   mov	x1, x19
0x250eeb5cc:  2d 58 c8 95   bl	j__objc_initWeak
0x250eeb5d0:  fa 23 00 91   add	x26, sp, #0x8
0x250eeb5d4:  d0 95 0b b0   adrp	x16, 0x2681a4000
0x250eeb5d8:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250eeb5dc:  f1 03 1a aa   mov	x17, x26
0x250eeb5e0:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250eeb5e4:  30 0a c1 da   pacda	x16, x17
0x250eeb5e8:  f0 07 00 f9   str	x16, [sp, #0x8]
0x250eeb5ec:  68 01 00 d0   adrp	x8, 0x250f19000
0x250eeb5f0:  00 d5 46 fd   ldr	d0, [x8, #0xda8]
0x250eeb5f4:  e0 0b 00 fd   str	d0, [sp, #0x10]
0x250eeb5f8:  48 43 00 91   add	x8, x26, #0x10
0x250eeb5fc:  10 00 00 90   adrp	x16, 0x250eeb000
0x250eeb600:  10 42 20 91   add	x16, x16, #0x810
0x250eeb604:  10 01 c1 da   pacia	x16, x8
0x250eeb608:  08 25 0c 90   adrp	x8, 0x26938b000
0x250eeb60c:  08 c1 33 91   add	x8, x8, #0xcf0 ; data_26938bcf0 (AirPlayReceiverKit/__DATA_CONST.__const)
0x250eeb610:  f0 a3 01 a9   stp	x16, x8, [sp, #0x18]
0x250eeb614:  40 83 00 91   add	x0, x26, #0x20
0x250eeb618:  e1 c3 00 91   add	x1, sp, #0x30
0x250eeb61c:  11 58 c8 95   bl	j__objc_copyWeak
0x250eeb620:  e0 23 00 91   add	x0, sp, #0x8
0x250eeb624:  b3 55 c8 95   bl	j__objc_retainBlock
0x250eeb628:  68 8a 40 f9   ldr	x8, [x19, #0x110]
0x250eeb62c:  60 8a 00 f9   str	x0, [x19, #0x110]
0x250eeb630:  00 54 c8 95   bl	j__objc_release_x8
0x250eeb634:  db 8a c8 95   bl	j__IsAppleInternalBuild
0x250eeb638:  c0 03 00 34   cbz	w0, loc_250eeb6b0 ; ⤵ 0x78
0x250eeb63c:  89 81 c8 95   bl	j__APGetSignpostsLogHandle
0x250eeb640:  98 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb644:  f6 03 00 aa   mov	x22, x0
0x250eeb648:  86 81 c8 95   bl	j__APGetSignpostsLogHandle
0x250eeb64c:  95 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb650:  f8 03 00 aa   mov	x24, x0
0x250eeb654:  07 8d c8 95   bl	j__os_signpost_id_generate
0x250eeb658:  f7 03 00 aa   mov	x23, x0
0x250eeb65c:  e1 53 c8 95   bl	j__objc_release_x24
0x250eeb660:  e8 06 00 d1   sub	x8, x23, #0x1
0x250eeb664:  1f 0d 00 b1   cmn	x8, #0x3
0x250eeb668:  28 02 00 54   b.hi	loc_250eeb6ac ; ⤵ 0x44
0x250eeb66c:  e0 03 16 aa   mov	x0, x22
0x250eeb670:  f8 5c c8 95   bl	j__os_signpost_enabled
0x250eeb674:  c0 01 00 34   cbz	w0, loc_250eeb6ac ; ⤵ 0x38
0x250eeb678:  ff 03 00 79   strh	wzr, [sp]
0x250eeb67c:  e0 ff ff b0   adrp	x0, 0x250ee8000
0x250eeb680:  00 00 00 91   add	x0, x0, #0 ; "\xcf\xfa\xed\xfe\f"
0x250eeb684:  64 01 00 d0   adrp	x4, 0x250f19000
0x250eeb688:  84 54 3d 91   add	x4, x4, #0xf55 ; "AirPlayVideoCreatePlayer"
0x250eeb68c:  65 01 00 d0   adrp	x5, 0x250f19000
0x250eeb690:  a5 18 3d 91   add	x5, x5, #0xf46 ; "Created player"
0x250eeb694:  e6 03 00 91   mov	x6, sp
0x250eeb698:  e1 03 16 aa   mov	x1, x22
0x250eeb69c:  02 00 80 52   mov	w2, #0
0x250eeb6a0:  e3 03 17 aa   mov	x3, x23
0x250eeb6a4:  47 00 80 52   mov	w7, #0x2
0x250eeb6a8:  6e 5c c8 95   bl	j___os_signpost_emit_with_name_impl
0x250eeb6ac:  ; loc_250eeb6ac
0x250eeb6ac:  c5 53 c8 95   bl	j__objc_release_x22
0x250eeb6b0:  ; loc_250eeb6b0
0x250eeb6b0:  40 83 00 91   add	x0, x26, #0x20
0x250eeb6b4:  ef 57 c8 95   bl	j__objc_destroyWeak
0x250eeb6b8:  e0 c3 00 91   add	x0, sp, #0x30
0x250eeb6bc:  ed 57 c8 95   bl	j__objc_destroyWeak
0x250eeb6c0:  ; loc_250eeb6c0
0x250eeb6c0:  e0 03 13 aa   mov	x0, x19
0x250eeb6c4:  db d5 c4 95   bl	0x258020e30
0x250eeb6c8:  68 0a 40 f9   ldr	x8, [x19, #0x10]
0x250eeb6cc:  7f 0a 00 f9   str	xzr, [x19, #0x10]
0x250eeb6d0:  d8 53 c8 95   bl	j__objc_release_x8
0x250eeb6d4:  68 0e 40 f9   ldr	x8, [x19, #0x18]
0x250eeb6d8:  7f 0e 00 f9   str	xzr, [x19, #0x18]
0x250eeb6dc:  d5 53 c8 95   bl	j__objc_release_x8
0x250eeb6e0:  08 25 0c d0   adrp	x8, 0x26938d000
0x250eeb6e4:  00 51 45 f9   ldr	x0, [x8, #0xaa0] ; "hgO\xf1"
0x250eeb6e8:  de 55 c8 95   bl	j__objc_alloc_init
0x250eeb6ec:  68 36 40 f9   ldr	x8, [x19, #0x68]
0x250eeb6f0:  60 36 00 f9   str	x0, [x19, #0x68]
0x250eeb6f4:  cf 53 c8 95   bl	j__objc_release_x8
0x250eeb6f8:  60 36 40 f9   ldr	x0, [x19, #0x68]
0x250eeb6fc:  e2 03 13 aa   mov	x2, x19
0x250eeb700:  a4 5b c4 95   bl	0x258002590
0x250eeb704:  08 25 0c d0   adrp	x8, 0x26938d000
0x250eeb708:  00 3d 45 f9   ldr	x0, [x8, #0xa78]
0x250eeb70c:  d5 55 c8 95   bl	j__objc_alloc_init
0x250eeb710:  68 3a 40 f9   ldr	x8, [x19, #0x70]
0x250eeb714:  60 3a 00 f9   str	x0, [x19, #0x70]
0x250eeb718:  c6 53 c8 95   bl	j__objc_release_x8
0x250eeb71c:  60 3a 40 f9   ldr	x0, [x19, #0x70]
0x250eeb720:  e2 03 13 aa   mov	x2, x19
0x250eeb724:  9b 5b c4 95   bl	0x258002590
0x250eeb728:  75 a2 02 39   strb	w21, [x19, #0xa8]
0x250eeb72c:  74 c6 00 b9   str	w20, [x19, #0xc4]
0x250eeb730:  7f 52 00 f9   str	xzr, [x19, #0xa0]
0x250eeb734:  7f 92 0a b8   stur	wzr, [x19, #0xa9]
0x250eeb738:  7f b6 02 39   strb	wzr, [x19, #0xad]
0x250eeb73c:  20 ab 47 f9   ldr	x0, [x25, #0xf50]
0x250eeb740:  58 65 c4 95   bl	0x258004ca0
0x250eeb744:  57 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb748:  68 5a 40 f9   ldr	x8, [x19, #0xb0]
0x250eeb74c:  60 5a 00 f9   str	x0, [x19, #0xb0]
0x250eeb750:  b8 53 c8 95   bl	j__objc_release_x8
0x250eeb754:  c8 8e 0b b0   adrp	x8, 0x2680c4000
0x250eeb758:  08 81 43 f9   ldr	x8, [x8, #0x700] ; _ptr.18ef929c8 (CoreMedia/__TEXT.__const)
0x250eeb75c:  00 01 c0 3d   ldr	q0, [x8]
0x250eeb760:  60 16 80 3d   str	q0, [x19, #0x50]
0x250eeb764:  08 09 40 f9   ldr	x8, [x8, #0x10]
0x250eeb768:  68 32 00 f9   str	x8, [x19, #0x60]
0x250eeb76c:  7f 46 00 f9   str	xzr, [x19, #0x88]
0x250eeb770:  e0 5f 12 b0   adrp	x0, 0x275ae8000
0x250eeb774:  00 20 2b 91   add	x0, x0, #0xac8
0x250eeb778:  fe 83 c8 95   bl	j__APSSettingsIsFeatureEnabled
0x250eeb77c:  80 02 00 34   cbz	w0, loc_250eeb7cc ; ⤵ 0x50
0x250eeb780:  28 8f 0b 90   adrp	x8, 0x2680cf000
0x250eeb784:  00 25 44 f9   ldr	x0, [x8, #0x848] ; _ptr.1e6f2c8a0 (Foundation/__DATA_DIRTY.__objc_data)
0x250eeb788:  6e 58 c4 95   bl	0x258001940
0x250eeb78c:  45 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb790:  68 42 40 f9   ldr	x8, [x19, #0x80]
0x250eeb794:  60 42 00 f9   str	x0, [x19, #0x80]
0x250eeb798:  a6 53 c8 95   bl	j__objc_release_x8
0x250eeb79c:  60 06 40 f9   ldr	x0, [x19, #0x8]
0x250eeb7a0:  d0 d7 c4 95   bl	0x2580216e0
0x250eeb7a4:  3f 55 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250eeb7a8:  68 3e 40 f9   ldr	x8, [x19, #0x78]
0x250eeb7ac:  60 3e 00 f9   str	x0, [x19, #0x78]
0x250eeb7b0:  a0 53 c8 95   bl	j__objc_release_x8
0x250eeb7b4:  60 3e 40 f9   ldr	x0, [x19, #0x78]
0x250eeb7b8:  e2 03 13 aa   mov	x2, x19
0x250eeb7bc:  bd d8 c4 95   bl	0x258021ab0
0x250eeb7c0:  60 3e 40 f9   ldr	x0, [x19, #0x78]
0x250eeb7c4:  e2 03 13 aa   mov	x2, x19
0x250eeb7c8:  72 5b c4 95   bl	0x258002590
0x250eeb7cc:  ; loc_250eeb7cc
0x250eeb7cc:  e0 03 13 aa   mov	x0, x19
0x250eeb7d0:  fd 7b 49 a9   ldp	fp, lr, [sp, #0x90]
0x250eeb7d4:  f4 4f 48 a9   ldp	x20, x19, [sp, #0x80]
0x250eeb7d8:  f6 57 47 a9   ldp	x22, x21, [sp, #0x70]
0x250eeb7dc:  f8 5f 46 a9   ldp	x24, x23, [sp, #0x60]
0x250eeb7e0:  fa 67 45 a9   ldp	x26, x25, [sp, #0x50]
0x250eeb7e4:  ff 83 02 91   add	sp, sp, #0xa0
0x250eeb7e8:  ff 0f 5f d6   retab
0x250eeb7ec:  02 00 00 14   b	loc_250eeb7f4 ; ⤵ 0x8

