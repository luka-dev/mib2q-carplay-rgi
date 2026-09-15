
+[APRKUtilities baseDictionaryForResponseToRequestWithDictionary:]
0x250ee9414:  7f 23 03 d5   pacibsp
0x250ee9418:  f6 57 bd a9   stp	x22, x21, [sp, #-0x30]!
0x250ee941c:  f4 4f 01 a9   stp	x20, x19, [sp, #0x10]

_objc_msgSend$removeObjectForKey:
0x250ee9420:  fd 7b 02 a9   stp	fp, lr, [sp, #0x20]
0x250ee9424:  fd 83 00 91   add	fp, sp, #0x20
0x250ee9428:  92 5c c8 95   bl	_objc_retain_x2_stub
0x250ee942c:  f3 03 00 aa   mov	x19, x0
0x250ee9430:  00 05 00 b4   cbz	x0, loc_250ee94d0 ; ⤵ 0xa0
0x250ee9434:  c8 8e 0b b0   adrp	x8, 0x2680c2000
0x250ee9438:  00 a9 47 f9   ldr	x0, [x8, #0xf50] ; _ptr.NSMutableDictionary
0x250ee943c:  bd 5e c8 95   bl	_objc_alloc_stub

_objc_msgSend$removeObjectsForKeys:
0x250ee9440:  62 00 80 52   mov	w2, #0x3
0x250ee9444:  57 6f c4 95   bl	0x2580051a0
0x250ee9448:  f4 03 00 aa   mov	x20, x0
0x250ee944c:  e2 5f 12 f0   adrp	x2, 0x275ae8000
0x250ee9450:  42 a0 23 91   add	x2, x2, #0x8e8 ; _ptr.__NSCFConstantString
0x250ee9454:  e0 03 13 aa   mov	x0, x19
0x250ee9458:  66 61 c4 95   bl	0x2580019f0
0x250ee945c:  11 5e c8 95   bl	_objc_claimAutoreleasedReturnValue_stub

_objc_msgSend$removeObserver:
0x250ee9460:  f5 03 00 aa   mov	x21, x0
0x250ee9464:  c0 00 00 b4   cbz	x0, loc_250ee947c ; ⤵ 0x18
0x250ee9468:  e3 5f 12 f0   adrp	x3, 0x275ae8000
0x250ee946c:  63 a0 23 91   add	x3, x3, #0x8e8 ; _ptr.__NSCFConstantString
0x250ee9470:  e0 03 14 aa   mov	x0, x20
0x250ee9474:  e2 03 15 aa   mov	x2, x21
0x250ee9478:  6e 61 c4 95   bl	0x258001a30
0x250ee947c:  ; loc_250ee947c
0x250ee947c:  e2 5f 12 f0   adrp	x2, 0x275ae8000

_objc_msgSend$removeObserver:forKeyPath:context:
0x250ee9480:  42 20 24 91   add	x2, x2, #0x908 ; _ptr.__NSCFConstantString
0x250ee9484:  e3 5f 12 f0   adrp	x3, 0x275ae8000
0x250ee9488:  63 a0 24 91   add	x3, x3, #0x928 ; _ptr.__NSCFConstantString
0x250ee948c:  e0 03 14 aa   mov	x0, x20
0x250ee9490:  68 61 c4 95   bl	0x258001a30
0x250ee9494:  e2 5f 12 f0   adrp	x2, 0x275ae8000
0x250ee9498:  42 20 25 91   add	x2, x2, #0x948 ; _ptr.__NSCFConstantString
0x250ee949c:  e0 03 13 aa   mov	x0, x19

_objc_msgSend$removeObserver:name:object:
0x250ee94a0:  54 61 c4 95   bl	0x2580019f0
0x250ee94a4:  ff 5d c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ee94a8:  f6 03 00 aa   mov	x22, x0
0x250ee94ac:  41 5c c8 95   bl	_objc_release_x21_stub
0x250ee94b0:  d6 00 00 b4   cbz	x22, loc_250ee94c8 ; ⤵ 0x18
0x250ee94b4:  e3 5f 12 f0   adrp	x3, 0x275ae8000
0x250ee94b8:  63 20 25 91   add	x3, x3, #0x948 ; _ptr.__NSCFConstantString
0x250ee94bc:  e0 03 14 aa   mov	x0, x20

_objc_msgSend$resetSession
0x250ee94c0:  e2 03 16 aa   mov	x2, x22
0x250ee94c4:  5b 61 c4 95   bl	0x258001a30
0x250ee94c8:  ; loc_250ee94c8
0x250ee94c8:  3e 5c c8 95   bl	_objc_release_x22_stub
0x250ee94cc:  02 00 00 14   b	loc_250ee94d4 ; ⤵ 0x8
0x250ee94d0:  ; loc_250ee94d0
0x250ee94d0:  14 00 80 d2   mov	x20, #0
0x250ee94d4:  ; loc_250ee94d4
0x250ee94d4:  2f 5c c8 95   bl	_objc_release_x19_stub
0x250ee94d8:  e0 03 14 aa   mov	x0, x20
0x250ee94dc:  fd 7b 42 a9   ldp	fp, lr, [sp, #0x20]

_objc_msgSend$resignAsNowPlayingApplication
0x250ee94e0:  f4 4f 41 a9   ldp	x20, x19, [sp, #0x10]
0x250ee94e4:  f6 57 c3 a8   ldp	x22, x21, [sp], #0x30
0x250ee94e8:  ff 23 03 d5   autibsp
0x250ee94ec:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250ee94f0:  50 00 f0 b6   tbz	x16, #0x3e, loc_250ee94f8 ; ⤵ 0x8
0x250ee94f4:  20 8e 38 d4   brk	#0xc471
0x250ee94f8:  ; loc_250ee94f8
0x250ee94f8:  e6 5d c8 15   b	_objc_autoreleaseReturnValue_stub

