
-[APRKMediaPlayer _figPlaybackStateStringFrom:]
0x250ef331c:  7f 23 03 d5   pacibsp
0x250ef3320:  ff c3 01 d1   sub	sp, sp, #0x70
0x250ef3324:  f4 4f 05 a9   stp	x20, x19, [sp, #0x50]
0x250ef3328:  fd 7b 06 a9   stp	fp, lr, [sp, #0x60]
0x250ef332c:  fd 83 01 91   add	fp, sp, #0x60
0x250ef3330:  f3 03 00 aa   mov	x19, x0
0x250ef3334:  5f f8 7e f2   tst	x2, #0xfffffffffffffffd
0x250ef3338:  61 02 00 54   b.ne	loc_250ef3384 ; ⤵ 0x4c
0x250ef333c:  68 b2 42 39   ldrb	w8, [x19, #0xac]
0x250ef3340:  28 02 00 36   tbz	w8, #0, loc_250ef3384 ; ⤵ 0x44
0x250ef3344:  e1 a3 00 91   add	x1, sp, #0x28
0x250ef3348:  90 95 0b b0   adrp	x16, 0x2681a4000
0x250ef334c:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.__NSStackBlock__
0x250ef3350:  f1 03 01 aa   mov	x17, x1
0x250ef3354:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250ef3358:  30 0a c1 da   pacda	x16, x17
0x250ef335c:  f0 17 00 f9   str	x16, [sp, #0x28]
0x250ef3360:  28 01 00 d0   adrp	x8, 0x250f19000
0x250ef3364:  00 d5 46 fd   ldr	d0, [x8, #0xda8]
0x250ef3368:  e0 1b 00 fd   str	d0, [sp, #0x30]
0x250ef336c:  28 40 00 91   add	x8, x1, #0x10
0x250ef3370:  10 00 00 90   adrp	x16, 0x250ef3000
0x250ef3374:  10 b2 11 91   add	x16, x16, #0x46c ; ___47-[APRKMediaPlayer _figPlaybackStateStringFrom:]_block_invoke
0x250ef3378:  10 01 c1 da   pacia	x16, x8
0x250ef337c:  f0 1f 00 f9   str	x16, [sp, #0x38]
0x250ef3380:  1f 00 00 14   b	loc_250ef33fc ; ⤵ 0x7c
0x250ef3384:  ; loc_250ef3384
0x250ef3384:  02 06 00 b5   cbnz	x2, loc_250ef3444 ; ⤵ 0xc0
0x250ef3388:  68 b6 42 39   ldrb	w8, [x19, #0xad]
0x250ef338c:  c8 05 00 36   tbz	w8, #0, loc_250ef3444 ; ⤵ 0xb8
0x250ef3390:  08 c3 0e 90   adrp	x8, 0x26e753000
0x250ef3394:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250ef3398:  1f 79 00 71   cmp	w8, #0x1e
0x250ef339c:  2c 01 00 54   b.gt	loc_250ef33c0 ; ⤵ 0x24
0x250ef33a0:  1f 05 00 31   cmn	w8, #0x1
0x250ef33a4:  01 06 00 54   b.ne	loc_250ef3464 ; ⤵ 0xc0
0x250ef33a8:  00 c3 0e 90   adrp	x0, 0x26e753000
0x250ef33ac:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250ef33b0:  c1 03 80 52   mov	w1, #0x1e
0x250ef33b4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef33b8:  82 36 c8 95   bl	__LogCategory_Initialize_stub
0x250ef33bc:  40 05 00 35   cbnz	w0, loc_250ef3464 ; ⤵ 0xa8
0x250ef33c0:  ; loc_250ef33c0
0x250ef33c0:  e1 03 00 91   mov	x1, sp
0x250ef33c4:  90 95 0b b0   adrp	x16, 0x2681a4000
0x250ef33c8:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.__NSStackBlock__
0x250ef33cc:  f1 03 01 aa   mov	x17, x1
0x250ef33d0:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250ef33d4:  30 0a c1 da   pacda	x16, x17
0x250ef33d8:  f0 03 00 f9   str	x16, [sp]
0x250ef33dc:  28 01 00 d0   adrp	x8, 0x250f19000
0x250ef33e0:  00 d5 46 fd   ldr	d0, [x8, #0xda8]
0x250ef33e4:  e0 07 00 fd   str	d0, [sp, #0x8]
0x250ef33e8:  28 40 00 91   add	x8, x1, #0x10
0x250ef33ec:  10 00 00 90   adrp	x16, 0x250ef3000
0x250ef33f0:  10 e2 11 91   add	x16, x16, #0x478 ; ___47-[APRKMediaPlayer _figPlaybackStateStringFrom:]_block_invoke_2
0x250ef33f4:  10 01 c1 da   pacia	x16, x8
0x250ef33f8:  f0 0b 00 f9   str	x16, [sp, #0x10]
0x250ef33fc:  ; loc_250ef33fc
0x250ef33fc:  c8 24 0c 90   adrp	x8, 0x26938b000
0x250ef3400:  08 61 34 91   add	x8, x8, #0xd18 ; data_26938bd18 (AirPlayReceiverKit/__DATA_CONST.__const)
0x250ef3404:  28 cc 01 a9   stp	x8, x19, [x1, #0x18]
0x250ef3408:  80 95 0b b0   adrp	x0, 0x2681a4000
0x250ef340c:  00 ac 44 f9   ldr	x0, [x0, #0x958] ; _ptr.__dispatch_main_q
0x250ef3410:  1c 36 c8 95   bl	j__dispatch_channel_async
0x250ef3414:  88 8e 0b d0   adrp	x8, 0x2680c5000
0x250ef3418:  08 35 40 f9   ldr	x8, [x8, #0x68] ; _ptr.1e0a0d160 (CoreMedia/__DATA_CONST.__const)
0x250ef341c:  ; loc_250ef341c
0x250ef341c:  08 01 40 f9   ldr	x8, [x8]
0x250ef3420:  a4 34 c8 95   bl	j__objc_retain_x8
0x250ef3424:  ; loc_250ef3424
0x250ef3424:  fd 7b 46 a9   ldp	fp, lr, [sp, #0x60]
0x250ef3428:  f4 4f 45 a9   ldp	x20, x19, [sp, #0x50]
0x250ef342c:  ff c3 01 91   add	sp, sp, #0x70
0x250ef3430:  ff 23 03 d5   autibsp
0x250ef3434:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250ef3438:  50 00 f0 b6   tbz	x16, #0x3e, loc_250ef3440 ; ⤵ 0x8
0x250ef343c:  20 8e 38 d4   brk	#0xc471
0x250ef3440:  ; loc_250ef3440
0x250ef3440:  14 36 c8 15   b	j__objc_autoreleaseReturnValue
0x250ef3444:  ; loc_250ef3444
0x250ef3444:  5f 10 00 f1   cmp	x2, #0x4
0x250ef3448:  a2 00 00 54   b.hs	loc_250ef345c ; ⤵ 0x14
0x250ef344c:  c8 24 0c b0   adrp	x8, 0x26938c000
0x250ef3450:  08 01 02 91   add	x8, x8, #0x80 ; "XѠ`"
0x250ef3454:  08 79 62 f8   ldr	x8, [x8, x2, lsl #0x3]
0x250ef3458:  f1 ff ff 17   b	loc_250ef341c ; ⤴ -0x3c
0x250ef345c:  ; loc_250ef345c
0x250ef345c:  00 00 80 d2   mov	x0, #0
0x250ef3460:  f1 ff ff 17   b	loc_250ef3424 ; ⤴ -0x3c
0x250ef3464:  ; loc_250ef3464
0x250ef3464:  b3 62 00 94   bl	0x250f0bf30

