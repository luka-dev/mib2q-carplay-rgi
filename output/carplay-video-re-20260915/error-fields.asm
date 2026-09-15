
+[APRKUtilities dictionaryForError:]
0x250ee9570:  7f 23 03 d5   pacibsp
0x250ee9574:  f6 57 bd a9   stp	x22, x21, [sp, #-0x30]!
0x250ee9578:  f4 4f 01 a9   stp	x20, x19, [sp, #0x10]
0x250ee957c:  fd 7b 02 a9   stp	fp, lr, [sp, #0x20]

_objc_msgSend$sampleRate
0x250ee9580:  fd 83 00 91   add	fp, sp, #0x20
0x250ee9584:  3b 5c c8 95   bl	_objc_retain_x2_stub
0x250ee9588:  f3 03 00 aa   mov	x19, x0
0x250ee958c:  a0 04 00 b4   cbz	_objc_msgSend$setActive:error:
0x250ee9590:  c8 8e 0b b0   adrp	x8, 0x2680c2000
0x250ee9594:  00 a9 47 f9   ldr	x0, [x8, #0xf50] ; _ptr.NSMutableDictionary
0x250ee9598:  32 5e c8 95   bl	_objc_alloc_init_stub
0x250ee959c:  f4 03 00 aa   mov	x20, x0

_objc_msgSend$seed
0x250ee95a0:  e0 03 13 aa   mov	x0, x19
0x250ee95a4:  f3 6d c4 95   bl	0x258004d70
0x250ee95a8:  be 5d c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ee95ac:  f5 03 00 aa   mov	x21, x0
0x250ee95b0:  00 5c c8 95   bl	_objc_release_x21_stub
0x250ee95b4:  75 01 00 b4   cbz	_objc_msgSend$setAPNowPlayingInfo:
0x250ee95b8:  e0 03 13 aa   mov	x0, x19
0x250ee95bc:  ed 6d c4 95   bl	0x258004d70

_objc_msgSend$session
0x250ee95c0:  b8 5d c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ee95c4:  f5 03 00 aa   mov	x21, x0
0x250ee95c8:  e3 5f 12 f0   adrp	x3, 0x275ae8000
0x250ee95cc:  63 a0 25 91   add	x3, x3, #0x968 ; _ptr.__NSCFConstantString
0x250ee95d0:  e0 03 14 aa   mov	x0, x20
0x250ee95d4:  e2 03 15 aa   mov	x2, x21
0x250ee95d8:  16 61 c4 95   bl	0x258001a30
0x250ee95dc:  f5 5b c8 95   bl	_objc_release_x21_stub

_objc_msgSend$setAPNowPlayingInfo:
0x250ee95e0:  ; loc_250ee95e0
0x250ee95e0:  28 8f 0b d0   adrp	x8, 0x2680cf000
0x250ee95e4:  15 45 43 f9   ldr	x21, [x8, #0x688] ; _ptr.NSNumber
0x250ee95e8:  e0 03 13 aa   mov	x0, x19
0x250ee95ec:  a5 6c c4 95   bl	0x258004880
0x250ee95f0:  e2 03 00 aa   mov	x2, x0
0x250ee95f4:  e0 03 15 aa   mov	x0, x21
0x250ee95f8:  ba 63 c4 95   bl	0x2580024e0
0x250ee95fc:  a9 5d c8 95   bl	_objc_claimAutoreleasedReturnValue_stub

_objc_msgSend$setAccessControlType:
0x250ee9600:  f5 03 00 aa   mov	x21, x0
0x250ee9604:  e3 5f 12 f0   adrp	x3, 0x275ae8000
0x250ee9608:  63 20 26 91   add	x3, x3, #0x988 ; _ptr.__NSCFConstantString
0x250ee960c:  e0 03 14 aa   mov	x0, x20
0x250ee9610:  e2 03 15 aa   mov	x2, x21
0x250ee9614:  07 61 c4 95   bl	0x258001a30
0x250ee9618:  e6 5b c8 95   bl	_objc_release_x21_stub
0x250ee961c:  02 00 00 14   b	loc_250ee9624 ; ⤵ 0x8

_objc_msgSend$setActive:error:
0x250ee9620:  ; loc_250ee9620
0x250ee9620:  14 00 80 d2   mov	x20, #0
0x250ee9624:  ; loc_250ee9624
0x250ee9624:  db 5b c8 95   bl	_objc_release_x19_stub
0x250ee9628:  e0 03 14 aa   mov	x0, x20
0x250ee962c:  fd 7b 42 a9   ldp	fp, lr, [sp, #0x20]
0x250ee9630:  f4 4f 41 a9   ldp	x20, x19, [sp, #0x10]
0x250ee9634:  f6 57 c3 a8   ldp	x22, x21, [sp], #0x30
0x250ee9638:  ff 23 03 d5   autibsp
0x250ee963c:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1

_objc_msgSend$setAirplayTargetConfigSeed:
0x250ee9640:  50 00 f0 b6   tbz	x16, #0x3e, loc_250ee9648 ; ⤵ 0x8
0x250ee9644:  20 8e 38 d4   brk	#0xc471
0x250ee9648:  ; loc_250ee9648
0x250ee9648:  92 5d c8 15   b	_objc_autoreleaseReturnValue_stub

