
-[APRKMediaPlayer _processMessageWithDictionaryInternal:]
0x250eec620:  7f 23 03 d5   pacibsp
0x250eec624:  ff 03 01 d1   sub	sp, sp, #0x40
0x250eec628:  f6 57 01 a9   stp	x22, x21, [sp, #0x10]
0x250eec62c:  f4 4f 02 a9   stp	x20, x19, [sp, #0x20]
0x250eec630:  fd 7b 03 a9   stp	fp, lr, [sp, #0x30]
0x250eec634:  fd c3 00 91   add	fp, sp, #0x30
0x250eec638:  f5 03 00 aa   mov	x21, x0
0x250eec63c:  0d 50 c8 95   bl	_objc_retain_x2_stub
0x250eec640:  f3 03 00 aa   mov	x19, x0
0x250eec644:  80 06 00 b4   cbz	x0, loc_250eec714 ; ⤵ 0xd0
0x250eec648:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec64c:  42 a0 23 91   add	x2, x2, #0x8e8 ; _ptr.__NSCFConstantString
0x250eec650:  e0 03 13 aa   mov	x0, x19
0x250eec654:  e7 54 c4 95   bl	0x2580019f0
0x250eec658:  92 51 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eec65c:  f4 03 00 aa   mov	x20, x0
0x250eec660:  36 c3 0e f0   adrp	x22, 0x26e753000
0x250eec664:  c8 2a 43 b9   ldr	w8, [x22, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eec668:  1f 79 00 71   cmp	w8, #0x1e
0x250eec66c:  8c 02 00 54   b.gt	loc_250eec6bc ; ⤵ 0x50
0x250eec670:  1f 05 00 31   cmn	w8, #0x1
0x250eec674:  80 01 00 54   b.eq	loc_250eec6a4 ; ⤵ 0x30
0x250eec678:  ; loc_250eec678
0x250eec678:  f4 4f 00 a9   stp	x20, x19, [sp]
0x250eec67c:  20 c3 0e f0   adrp	x0, 0x26e753000
0x250eec680:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eec684:  21 01 00 b0   adrp	x1, 0x250f11000
0x250eec688:  21 fc 2e 91   add	x1, x1, #0xbbf ; "-[APRKMediaPlayer _processMessageWithDictionaryInternal:]"
0x250eec68c:  23 01 00 b0   adrp	x3, 0x250f11000
0x250eec690:  63 e4 2f 91   add	x3, x3, #0xbf9 ; "[VideoV2: Sender -> Receiver (%@)] %@"
0x250eec694:  c2 03 80 52   mov	w2, #0x1e
0x250eec698:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250eec69c:  19 52 c8 95   bl	_LogPrintF_stub
0x250eec6a0:  07 00 00 14   b	loc_250eec6bc ; ⤵ 0x1c
0x250eec6a4:  ; loc_250eec6a4
0x250eec6a4:  20 c3 0e f0   adrp	x0, 0x26e753000
0x250eec6a8:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eec6ac:  c1 03 80 52   mov	w1, #0x1e
0x250eec6b0:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eec6b4:  c3 51 c8 95   bl	__LogCategory_Initialize_stub
0x250eec6b8:  00 fe ff 35   cbnz	w0, loc_250eec678 ; ⤴ -0x40
0x250eec6bc:  ; loc_250eec6bc
0x250eec6bc:  74 01 00 b4   cbz	x20, loc_250eec6e8 ; ⤵ 0x2c
0x250eec6c0:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec6c4:  42 a0 2b 91   add	x2, x2, #0xae8 ; _ptr.__NSCFConstantString
0x250eec6c8:  e0 03 14 aa   mov	x0, x20
0x250eec6cc:  89 53 c4 95   bl	0x2580014f0
0x250eec6d0:  e0 03 00 34   cbz	w0, loc_250eec74c ; ⤵ 0x7c
0x250eec6d4:  e0 03 15 aa   mov	x0, x21
0x250eec6d8:  e2 03 13 aa   mov	x2, x19
0x250eec6dc:  b9 d1 c4 95   bl	0x258020dc0
0x250eec6e0:  ; loc_250eec6e0
0x250eec6e0:  15 00 80 d2   mov	x21, #0
0x250eec6e4:  0e 00 00 14   b	loc_250eec71c ; ⤵ 0x38
0x250eec6e8:  ; loc_250eec6e8
0x250eec6e8:  c8 2a 43 b9   ldr	w8, [x22, #0x328]
0x250eec6ec:  1f 79 00 71   cmp	w8, #0x1e
0x250eec6f0:  2c 01 00 54   b.gt	loc_250eec714 ; ⤵ 0x24
0x250eec6f4:  1f 05 00 31   cmn	w8, #0x1
0x250eec6f8:  a1 08 00 54   b.ne	loc_250eec80c ; ⤵ 0x114
0x250eec6fc:  20 c3 0e f0   adrp	x0, 0x26e753000
0x250eec700:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eec704:  c1 03 80 52   mov	w1, #0x1e
0x250eec708:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eec70c:  ad 51 c8 95   bl	__LogCategory_Initialize_stub
0x250eec710:  e0 07 00 35   cbnz	w0, loc_250eec80c ; ⤵ 0xfc
0x250eec714:  ; loc_250eec714
0x250eec714:  15 00 80 d2   mov	x21, #0
0x250eec718:  14 00 80 d2   mov	x20, #0
0x250eec71c:  ; loc_250eec71c
0x250eec71c:  a1 4f c8 95   bl	_objc_release_x20_stub
0x250eec720:  9c 4f c8 95   bl	_objc_release_x19_stub
0x250eec724:  e0 03 15 aa   mov	x0, x21
0x250eec728:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x250eec72c:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x250eec730:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x250eec734:  ff 03 01 91   add	sp, sp, #0x40
0x250eec738:  ff 23 03 d5   autibsp
0x250eec73c:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250eec740:  50 00 f0 b6   tbz	x16, #0x3e, loc_250eec748 ; ⤵ 0x8
0x250eec744:  20 8e 38 d4   brk	#0xc471
0x250eec748:  ; loc_250eec748
0x250eec748:  52 51 c8 15   b	_objc_autoreleaseReturnValue_stub
0x250eec74c:  ; loc_250eec74c
0x250eec74c:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec750:  42 20 2c 91   add	x2, x2, #0xb08 ; _ptr.__NSCFConstantString
0x250eec754:  e0 03 14 aa   mov	x0, x20
0x250eec758:  66 53 c4 95   bl	0x2580014f0
0x250eec75c:  e0 00 00 34   cbz	w0, loc_250eec778 ; ⤵ 0x1c
0x250eec760:  e0 03 15 aa   mov	x0, x21
0x250eec764:  e2 03 13 aa   mov	x2, x19
0x250eec768:  8e d1 c4 95   bl	0x258020da0
0x250eec76c:  4d 51 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eec770:  ; loc_250eec770
0x250eec770:  f5 03 00 aa   mov	x21, x0
0x250eec774:  ea ff ff 17   b	loc_250eec71c ; ⤴ -0x58
0x250eec778:  ; loc_250eec778
0x250eec778:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec77c:  42 a0 2c 91   add	x2, x2, #0xb28 ; _ptr.__NSCFConstantString
0x250eec780:  e0 03 14 aa   mov	x0, x20
0x250eec784:  5b 53 c4 95   bl	0x2580014f0
0x250eec788:  a0 00 00 34   cbz	w0, loc_250eec79c ; ⤵ 0x14
0x250eec78c:  e0 03 15 aa   mov	x0, x21
0x250eec790:  e2 03 13 aa   mov	x2, x19
0x250eec794:  db d1 c4 95   bl	0x258020f00
0x250eec798:  d2 ff ff 17   b	loc_250eec6e0 ; ⤴ -0xb8
0x250eec79c:  ; loc_250eec79c
0x250eec79c:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec7a0:  42 20 2d 91   add	x2, x2, #0xb48 ; _ptr.__NSCFConstantString
0x250eec7a4:  e0 03 14 aa   mov	x0, x20
0x250eec7a8:  52 53 c4 95   bl	0x2580014f0
0x250eec7ac:  c0 00 00 34   cbz	w0, loc_250eec7c4 ; ⤵ 0x18
0x250eec7b0:  e0 03 15 aa   mov	x0, x21
0x250eec7b4:  e2 03 13 aa   mov	x2, x19
0x250eec7b8:  36 d1 c4 95   bl	0x258020c90
0x250eec7bc:  39 51 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eec7c0:  ec ff ff 17   b	loc_250eec770 ; ⤴ -0x50
0x250eec7c4:  ; loc_250eec7c4
0x250eec7c4:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec7c8:  42 a0 2d 91   add	x2, x2, #0xb68 ; _ptr.__NSCFConstantString
0x250eec7cc:  e0 03 14 aa   mov	x0, x20
0x250eec7d0:  48 53 c4 95   bl	0x2580014f0
0x250eec7d4:  a0 00 00 34   cbz	w0, loc_250eec7e8 ; ⤵ 0x14
0x250eec7d8:  e0 03 15 aa   mov	x0, x21
0x250eec7dc:  e2 03 13 aa   mov	x2, x19
0x250eec7e0:  34 d1 c4 95   bl	0x258020cb0
0x250eec7e4:  bf ff ff 17   b	loc_250eec6e0 ; ⤴ -0x104
0x250eec7e8:  ; loc_250eec7e8
0x250eec7e8:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec7ec:  42 20 2e 91   add	x2, x2, #0xb88 ; _ptr.__NSCFConstantString
0x250eec7f0:  e0 03 14 aa   mov	x0, x20
0x250eec7f4:  3f 53 c4 95   bl	0x2580014f0
0x250eec7f8:  00 01 00 34   cbz	w0, loc_250eec818 ; ⤵ 0x20
0x250eec7fc:  e0 03 15 aa   mov	x0, x21
0x250eec800:  e2 03 13 aa   mov	x2, x19
0x250eec804:  93 d1 c4 95   bl	0x258020e50
0x250eec808:  b6 ff ff 17   b	loc_250eec6e0 ; ⤴ -0x128
0x250eec80c:  ; loc_250eec80c
0x250eec80c:  e0 03 13 aa   mov	x0, x19
0x250eec810:  7e 7b 00 94   bl	-[APRKMediaPlayer _processMessageWithDictionaryInternal:].cold.1
0x250eec814:  c0 ff ff 17   b	loc_250eec714 ; ⤴ -0x100
0x250eec818:  ; loc_250eec818
0x250eec818:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec81c:  42 a0 2e 91   add	x2, x2, #0xba8 ; _ptr.__NSCFConstantString
0x250eec820:  e0 03 14 aa   mov	x0, x20
0x250eec824:  33 53 c4 95   bl	0x2580014f0
0x250eec828:  a0 00 00 34   cbz	w0, loc_250eec83c ; ⤵ 0x14
0x250eec82c:  e0 03 15 aa   mov	x0, x21
0x250eec830:  e2 03 13 aa   mov	x2, x19
0x250eec834:  bb d1 c4 95   bl	0x258020f20
0x250eec838:  aa ff ff 17   b	loc_250eec6e0 ; ⤴ -0x158
0x250eec83c:  ; loc_250eec83c
0x250eec83c:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec840:  42 20 2f 91   add	x2, x2, #0xbc8 ; _ptr.__NSCFConstantString
0x250eec844:  e0 03 14 aa   mov	x0, x20
0x250eec848:  2a 53 c4 95   bl	0x2580014f0
0x250eec84c:  a0 00 00 34   cbz	w0, loc_250eec860 ; ⤵ 0x14
0x250eec850:  e0 03 15 aa   mov	x0, x21
0x250eec854:  e2 03 13 aa   mov	x2, x19
0x250eec858:  8e d1 c4 95   bl	0x258020e90
0x250eec85c:  a1 ff ff 17   b	loc_250eec6e0 ; ⤴ -0x17c
0x250eec860:  ; loc_250eec860
0x250eec860:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec864:  42 a0 2f 91   add	x2, x2, #0xbe8 ; _ptr.__NSCFConstantString
0x250eec868:  e0 03 14 aa   mov	x0, x20
0x250eec86c:  21 53 c4 95   bl	0x2580014f0
0x250eec870:  a0 00 00 34   cbz	w0, loc_250eec884 ; ⤵ 0x14
0x250eec874:  e0 03 15 aa   mov	x0, x21
0x250eec878:  e2 03 13 aa   mov	x2, x19
0x250eec87c:  4d d1 c4 95   bl	0x258020db0
0x250eec880:  98 ff ff 17   b	loc_250eec6e0 ; ⤴ -0x1a0
0x250eec884:  ; loc_250eec884
0x250eec884:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec888:  42 20 30 91   add	x2, x2, #0xc08 ; _ptr.__NSCFConstantString
0x250eec88c:  e0 03 14 aa   mov	x0, x20
0x250eec890:  18 53 c4 95   bl	0x2580014f0
0x250eec894:  a0 00 00 34   cbz	w0, loc_250eec8a8 ; ⤵ 0x14
0x250eec898:  e0 03 15 aa   mov	x0, x21
0x250eec89c:  e2 03 13 aa   mov	x2, x19
0x250eec8a0:  b0 d1 c4 95   bl	0x258020f60
0x250eec8a4:  8f ff ff 17   b	loc_250eec6e0 ; ⤴ -0x1c4
0x250eec8a8:  ; loc_250eec8a8
0x250eec8a8:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec8ac:  42 a0 30 91   add	x2, x2, #0xc28 ; _ptr.__NSCFConstantString
0x250eec8b0:  e0 03 14 aa   mov	x0, x20
0x250eec8b4:  0f 53 c4 95   bl	0x2580014f0
0x250eec8b8:  c0 00 00 34   cbz	w0, loc_250eec8d0 ; ⤵ 0x18
0x250eec8bc:  e0 03 15 aa   mov	x0, x21
0x250eec8c0:  e2 03 13 aa   mov	x2, x19
0x250eec8c4:  2b d1 c4 95   bl	0x258020d70
0x250eec8c8:  f6 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eec8cc:  a9 ff ff 17   b	loc_250eec770 ; ⤴ -0x15c
0x250eec8d0:  ; loc_250eec8d0
0x250eec8d0:  e2 5f 12 90   adrp	x2, 0x275ae8000
0x250eec8d4:  42 20 31 91   add	x2, x2, #0xc48 ; _ptr.__NSCFConstantString
0x250eec8d8:  e0 03 14 aa   mov	x0, x20
0x250eec8dc:  05 53 c4 95   bl	0x2580014f0
0x250eec8e0:  00 f0 ff 34   cbz	w0, loc_250eec6e0 ; ⤴ -0x200
0x250eec8e4:  e0 5f 12 90   adrp	x0, 0x275ae8000
0x250eec8e8:  00 20 2b 91   add	x0, x0, #0xac8 ; _ptr.__NSCFConstantString
0x250eec8ec:  a1 7f c8 95   bl	_APSSettingsIsFeatureEnabled_stub
0x250eec8f0:  80 ef ff 34   cbz	w0, loc_250eec6e0 ; ⤴ -0x210
0x250eec8f4:  e0 03 15 aa   mov	x0, x21
0x250eec8f8:  e2 03 13 aa   mov	x2, x19
0x250eec8fc:  25 d1 c4 95   bl	0x258020d90
0x250eec900:  78 ff ff 17   b	loc_250eec6e0 ; ⤴ -0x220

