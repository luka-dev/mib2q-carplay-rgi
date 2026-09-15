
+[APRKUtilities baseDictionaryForUpstreamMessageWithType:]
0x250ee94fc:  7f 23 03 d5   pacibsp

_objc_msgSend$restartAdvertisingIfNecessary
0x250ee9500:  f4 4f be a9   stp	x20, x19, [sp, #-0x20]!
0x250ee9504:  fd 7b 01 a9   stp	fp, lr, [sp, #0x10]
0x250ee9508:  fd 43 00 91   add	fp, sp, #0x10
0x250ee950c:  02 02 00 b4   cbz	x2, loc_250ee954c ; ⤵ 0x40
0x250ee9510:  c8 8e 0b b0   adrp	x8, 0x2680c2000
0x250ee9514:  13 a9 47 f9   ldr	x19, [x8, #0xf50] ; _ptr.NSMutableDictionary
0x250ee9518:  56 5c c8 95   bl	_objc_retain_x2_stub
0x250ee951c:  f4 03 00 aa   mov	x20, x0

_objc_msgSend$result
0x250ee9520:  e0 03 13 aa   mov	x0, x19
0x250ee9524:  83 5e c8 95   bl	_objc_alloc_stub
0x250ee9528:  42 00 80 52   mov	w2, #0x2
0x250ee952c:  1d 6f c4 95   bl	0x2580051a0
0x250ee9530:  f3 03 00 aa   mov	x19, x0
0x250ee9534:  e3 5f 12 f0   adrp	x3, 0x275ae8000
0x250ee9538:  63 a0 23 91   add	x3, x3, #0x8e8 ; _ptr.__NSCFConstantString
0x250ee953c:  e2 03 14 aa   mov	x2, x20

_objc_msgSend$resume
0x250ee9540:  3c 61 c4 95   bl	0x258001a30
0x250ee9544:  17 5c c8 95   bl	_objc_release_x20_stub
0x250ee9548:  02 00 00 14   b	loc_250ee9550 ; ⤵ 0x8
0x250ee954c:  ; loc_250ee954c
0x250ee954c:  13 00 80 d2   mov	x19, #0
0x250ee9550:  ; loc_250ee9550
0x250ee9550:  e0 03 13 aa   mov	x0, x19
0x250ee9554:  fd 7b 41 a9   ldp	fp, lr, [sp, #0x10]
0x250ee9558:  f4 4f c2 a8   ldp	x20, x19, [sp], #0x20
0x250ee955c:  ff 23 03 d5   autibsp

_objc_msgSend$runWithConfiguration:
0x250ee9560:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250ee9564:  50 00 f0 b6   tbz	x16, #0x3e, loc_250ee956c ; ⤵ 0x8
0x250ee9568:  20 8e 38 d4   brk	#0xc471
0x250ee956c:  ; loc_250ee956c
0x250ee956c:  c9 5d c8 15   b	_objc_autoreleaseReturnValue_stub

