
-[APRKMediaPlayer _setPropertyWithDictionary:]
0x250ef0058:  7f 23 03 d5   pacibsp
0x250ef005c:  fc 6f ba a9   stp	x28, x27, [sp, #-0x60]!
0x250ef0060:  fa 67 01 a9   stp	x26, x25, [sp, #0x10]
0x250ef0064:  f8 5f 02 a9   stp	x24, x23, [sp, #0x20]
0x250ef0068:  f6 57 03 a9   stp	x22, x21, [sp, #0x30]
0x250ef006c:  f4 4f 04 a9   stp	x20, x19, [sp, #0x40]
0x250ef0070:  fd 7b 05 a9   stp	fp, lr, [sp, #0x50]
0x250ef0074:  fd 43 01 91   add	fp, sp, #0x50
0x250ef0078:  ff c3 07 d1   sub	sp, sp, #0x1f0
0x250ef007c:  f5 03 00 aa   mov	x21, x0
0x250ef0080:  a8 95 0b 90   adrp	x8, 0x2681a4000
0x250ef0084:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.___stack_chk_guard
0x250ef0088:  08 01 40 f9   ldr	x8, [x8]
0x250ef008c:  a8 03 1a f8   stur	x8, [fp, #-0x60]
0x250ef0090:  78 41 c8 95   bl	_objc_retain_x2_stub
0x250ef0094:  f3 03 00 aa   mov	x19, x0
0x250ef0098:  e8 c3 04 91   add	x8, sp, #0x130
0x250ef009c:  ff 23 13 a9   stp	xzr, x8, [sp, #0x130]
0x250ef00a0:  49 01 00 b0   adrp	x9, 0x250f19000
0x250ef00a4:  20 dd 46 fd   ldr	d0, [x9, #0xdb8]
0x250ef00a8:  e0 a3 00 fd   str	d0, [sp, #0x140]
0x250ef00ac:  09 61 00 91   add	x9, x8, #0x18
0x250ef00b0:  f0 ff ff 90   adrp	x16, 0x250eec000
0x250ef00b4:  10 62 15 91   add	x16, x16, #0x558 ; ___Block_byref_object_copy_
0x250ef00b8:  30 01 c1 da   pacia	x16, x9
0x250ef00bc:  f0 a7 00 f9   str	x16, [sp, #0x148]
0x250ef00c0:  08 81 00 91   add	x8, x8, #0x20
0x250ef00c4:  f0 ff ff 90   adrp	x16, 0x250eec000
0x250ef00c8:  10 a2 15 91   add	x16, x16, #0x568 ; ___Block_byref_object_dispose_
0x250ef00cc:  10 01 c1 da   pacia	x16, x8
0x250ef00d0:  f0 7f 15 a9   stp	x16, xzr, [sp, #0x150]
0x250ef00d4:  20 03 00 b4   cbz	x0, loc_250ef0138 ; ⤵ 0x64
0x250ef00d8:  c2 5f 12 90   adrp	x2, 0x275ae8000
0x250ef00dc:  42 20 2d 91   add	x2, x2, #0xb48 ; _ptr.__NSCFConstantString
0x250ef00e0:  e0 03 13 aa   mov	x0, x19
0x250ef00e4:  43 46 c4 95   bl	0x2580019f0
0x250ef00e8:  ee 42 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef00ec:  f4 03 00 aa   mov	x20, x0
0x250ef00f0:  1c c3 0e f0   adrp	x28, 0x26e753000
0x250ef00f4:  88 2b 43 b9   ldr	w8, [x28, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250ef00f8:  1f 79 00 71   cmp	w8, #0x1e
0x250ef00fc:  00 03 00 b4   cbz	x0, loc_250ef015c ; ⤵ 0x60
0x250ef0100:  4c 05 00 54   b.gt	loc_250ef01a8 ; ⤵ 0xa8
0x250ef0104:  1f 05 00 31   cmn	w8, #0x1
0x250ef0108:  40 04 00 54   b.eq	loc_250ef0190 ; ⤵ 0x88
0x250ef010c:  ; loc_250ef010c
0x250ef010c:  f4 03 00 f9   str	x20, [sp]
0x250ef0110:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0114:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250ef0118:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef011c:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0120:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0124:  63 94 24 91   add	x3, x3, #0x925 ; "Handling set for property: %@"
0x250ef0128:  c2 03 80 52   mov	w2, #0x1e
0x250ef012c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0130:  74 43 c8 95   bl	_LogPrintF_stub
0x250ef0134:  1d 00 00 14   b	loc_250ef01a8 ; ⤵ 0x74
0x250ef0138:  ; loc_250ef0138
0x250ef0138:  08 c3 0e f0   adrp	x8, 0x26e753000
0x250ef013c:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250ef0140:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0144:  4c 4b 00 54   b.gt	loc_250ef0aac ; ⤵ 0x968
0x250ef0148:  1f 05 00 31   cmn	w8, #0x1
0x250ef014c:  60 14 00 54   b.eq	loc_250ef03d8 ; ⤵ 0x28c
0x250ef0150:  ; loc_250ef0150
0x250ef0150:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0154:  63 5c 23 91   add	x3, x3, #0x8d7 ; "requestDictionary is nil, doing nothing."
0x250ef0158:  06 00 00 14   b	loc_250ef0170 ; ⤵ 0x18
0x250ef015c:  ; loc_250ef015c
0x250ef015c:  8c 4a 00 54   b.gt	loc_250ef0aac ; ⤵ 0x950
0x250ef0160:  1f 05 00 31   cmn	w8, #0x1
0x250ef0164:  80 18 00 54   b.eq	loc_250ef0474 ; ⤵ 0x310
0x250ef0168:  ; loc_250ef0168
0x250ef0168:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef016c:  63 00 24 91   add	x3, x3, #0x900 ; "propertyToSet is nil, doing nothing."
0x250ef0170:  ; loc_250ef0170
0x250ef0170:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0174:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250ef0178:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef017c:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0180:  c2 03 80 52   mov	w2, #0x1e
0x250ef0184:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0188:  5e 43 c8 95   bl	_LogPrintF_stub
0x250ef018c:  48 02 00 14   b	loc_250ef0aac ; ⤵ 0x920
0x250ef0190:  ; loc_250ef0190
0x250ef0190:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0194:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250ef0198:  c1 03 80 52   mov	w1, #0x1e
0x250ef019c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef01a0:  08 43 c8 95   bl	__LogCategory_Initialize_stub
0x250ef01a4:  40 fb ff 35   cbnz	w0, loc_250ef010c ; ⤴ -0x98
0x250ef01a8:  ; loc_250ef01a8
0x250ef01a8:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef01ac:  08 11 40 f9   ldr	x8, [x8, #0x20] ; _ptr._kFigEndpointPlaybackSessionProxiedProperty_Muted
0x250ef01b0:  02 01 40 f9   ldr	x2, [x8]
0x250ef01b4:  e0 03 14 aa   mov	x0, x20
0x250ef01b8:  ce 44 c4 95   bl	0x2580014f0
0x250ef01bc:  59 01 00 b0   adrp	x25, 0x250f19000
0x250ef01c0:  40 05 00 34   cbz	w0, loc_250ef0268 ; ⤵ 0xa8
0x250ef01c4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef01c8:  42 20 01 91   add	x2, x2, #0x48 ; _ptr.__NSCFConstantString
0x250ef01cc:  e0 03 13 aa   mov	x0, x19
0x250ef01d0:  08 46 c4 95   bl	0x2580019f0
0x250ef01d4:  b3 42 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250ef01d8:  f7 03 00 aa   mov	x23, x0
0x250ef01dc:  fd 43 c4 95   bl	0x2580011d0
0x250ef01e0:  f6 03 00 aa   mov	x22, x0
0x250ef01e4:  fb 40 c8 95   bl	_objc_release_x23_stub
0x250ef01e8:  e0 43 01 91   add	x0, sp, #0x50
0x250ef01ec:  e1 03 15 aa   mov	x1, x21
0x250ef01f0:  24 45 c8 95   bl	_objc_initWeak_stub
0x250ef01f4:  f7 03 04 91   add	x23, sp, #0x100
0x250ef01f8:  b0 95 0b 90   adrp	x16, 0x2681a4000
0x250ef01fc:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.__NSStackBlock__
0x250ef0200:  f1 03 17 aa   mov	x17, x23
0x250ef0204:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250ef0208:  30 0a c1 da   pacda	x16, x17
0x250ef020c:  f0 83 00 f9   str	x16, [sp, #0x100]
0x250ef0210:  20 d7 46 fd   ldr	d0, [x25, #0xda8]
0x250ef0214:  e0 87 00 fd   str	d0, [sp, #0x108]
0x250ef0218:  e8 42 00 91   add	x8, x23, #0x10
0x250ef021c:  10 00 00 b0   adrp	x16, 0x250ef1000
0x250ef0220:  10 b2 02 91   add	x16, x16, #0xac ; ___46-[APRKMediaPlayer _setPropertyWithDictionary:]_block_invoke
0x250ef0224:  10 01 c1 da   pacia	x16, x8
0x250ef0228:  c8 24 0c f0   adrp	x8, 0x26938b000
0x250ef022c:  08 41 3b 91   add	x8, x8, #0xed0 ; data_26938bed0 (AirPlayReceiverKit/__DATA_CONST.__const)
0x250ef0230:  f0 23 11 a9   stp	x16, x8, [sp, #0x110]
0x250ef0234:  e0 82 00 91   add	x0, x23, #0x20
0x250ef0238:  e1 43 01 91   add	x1, sp, #0x50
0x250ef023c:  09 45 c8 95   bl	_objc_copyWeak_stub
0x250ef0240:  f6 a3 04 39   strb	w22, [sp, #0x128]
0x250ef0244:  a0 95 0b 90   adrp	x0, 0x2681a4000
0x250ef0248:  00 ac 44 f9   ldr	x0, [x0, #0x958] ; _ptr.__dispatch_main_q
0x250ef024c:  e1 03 04 91   add	x1, sp, #0x100
0x250ef0250:  8c 42 c8 95   bl	j__dispatch_channel_async
0x250ef0254:  e0 82 00 91   add	x0, x23, #0x20
0x250ef0258:  06 45 c8 95   bl	j__objc_destroyWeak
0x250ef025c:  e0 43 01 91   add	x0, sp, #0x50
0x250ef0260:  04 45 c8 95   bl	j__objc_destroyWeak
0x250ef0264:  f8 00 00 14   b	loc_250ef0644 ; ⤵ 0x3e0
0x250ef0268:  ; loc_250ef0268
0x250ef0268:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef026c:  08 01 40 f9   ldr	x8, [x8] ; _ptr.1e0a0d5d8 (CoreMedia/__DATA_CONST.__const)
0x250ef0270:  02 01 40 f9   ldr	x2, [x8]
0x250ef0274:  e0 03 14 aa   mov	x0, x20
0x250ef0278:  9e 44 c4 95   bl	0x2580014f0
0x250ef027c:  80 06 00 34   cbz	w0, loc_250ef034c ; ⤵ 0xd0
0x250ef0280:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0284:  42 20 01 91   add	x2, x2, #0x48
0x250ef0288:  e0 03 13 aa   mov	x0, x19
0x250ef028c:  d9 45 c4 95   bl	0x2580019f0
0x250ef0290:  84 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0294:  f6 03 00 aa   mov	x22, x0
0x250ef0298:  ce 43 c4 95   bl	0x2580011d0
0x250ef029c:  f7 03 15 aa   mov	x23, x21
0x250ef02a0:  e0 0e 02 38   strb	w0, [x23, #0x20]!
0x250ef02a4:  c7 40 c8 95   bl	j__objc_release_x22
0x250ef02a8:  e8 02 40 39   ldrb	w8, [x23]
0x250ef02ac:  1f 05 00 71   cmp	w8, #0x1
0x250ef02b0:  a1 1c 00 54   b.ne	loc_250ef0644 ; ⤵ 0x394
0x250ef02b4:  00 e4 00 6f   movi	v0.2d, #0
0x250ef02b8:  e0 03 07 ad   stp	q0, q0, [sp, #0xe0]
0x250ef02bc:  e0 03 06 ad   stp	q0, q0, [sp, #0xc0]
0x250ef02c0:  a0 06 40 f9   ldr	x0, [x21, #0x8]
0x250ef02c4:  cb 69 c4 95   bl	0x25800a9f0
0x250ef02c8:  76 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef02cc:  f6 03 00 aa   mov	x22, x0
0x250ef02d0:  e2 03 03 91   add	x2, sp, #0xc0
0x250ef02d4:  a3 83 03 d1   sub	x3, fp, #0xe0
0x250ef02d8:  04 02 80 52   mov	w4, #0x10
0x250ef02dc:  cd 43 c4 95   bl	0x258001210
0x250ef02e0:  f7 03 00 aa   mov	x23, x0
0x250ef02e4:  00 03 00 b4   cbz	x0, loc_250ef0344 ; ⤵ 0x60
0x250ef02e8:  e8 6b 40 f9   ldr	x8, [sp, #0xd0]
0x250ef02ec:  18 01 40 f9   ldr	x24, [x8]
0x250ef02f0:  ; loc_250ef02f0
0x250ef02f0:  1a 00 80 d2   mov	x26, #0
0x250ef02f4:  ; loc_250ef02f4
0x250ef02f4:  e8 6b 40 f9   ldr	x8, [sp, #0xd0]
0x250ef02f8:  08 01 40 f9   ldr	x8, [x8]
0x250ef02fc:  1f 01 18 eb   cmp	x8, x24
0x250ef0300:  60 00 00 54   b.eq	loc_250ef030c ; ⤵ 0xc
0x250ef0304:  e0 03 16 aa   mov	x0, x22
0x250ef0308:  6a 42 c8 95   bl	j__objc_enumerationMutation
0x250ef030c:  ; loc_250ef030c
0x250ef030c:  e8 67 40 f9   ldr	x8, [sp, #0xc8]
0x250ef0310:  02 79 7a f8   ldr	x2, [x8, x26, lsl #0x3]
0x250ef0314:  e0 03 15 aa   mov	x0, x21
0x250ef0318:  3a c3 c4 95   bl	0x258021000
0x250ef031c:  5a 07 00 91   add	x26, x26, #0x1
0x250ef0320:  f7 06 00 f1   subs	x23, x23, #0x1
0x250ef0324:  81 fe ff 54   b.ne	loc_250ef02f4 ; ⤴ -0x30
0x250ef0328:  e2 03 03 91   add	x2, sp, #0xc0
0x250ef032c:  a3 83 03 d1   sub	x3, fp, #0xe0
0x250ef0330:  e0 03 16 aa   mov	x0, x22
0x250ef0334:  04 02 80 52   mov	w4, #0x10
0x250ef0338:  b6 43 c4 95   bl	0x258001210
0x250ef033c:  f7 03 00 aa   mov	x23, x0
0x250ef0340:  80 fd ff b5   cbnz	x0, loc_250ef02f0 ; ⤴ -0x50
0x250ef0344:  ; loc_250ef0344
0x250ef0344:  9f 40 c8 95   bl	j__objc_release_x22
0x250ef0348:  bf 00 00 14   b	loc_250ef0644 ; ⤵ 0x2fc
0x250ef034c:  ; loc_250ef034c
0x250ef034c:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0350:  42 a0 01 91   add	x2, x2, #0x68
0x250ef0354:  e0 03 14 aa   mov	x0, x20
0x250ef0358:  66 44 c4 95   bl	0x2580014f0
0x250ef035c:  c0 04 00 34   cbz	w0, loc_250ef03f4 ; ⤵ 0x98
0x250ef0360:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0364:  42 20 01 91   add	x2, x2, #0x48
0x250ef0368:  e0 03 13 aa   mov	x0, x19
0x250ef036c:  a1 45 c4 95   bl	0x2580019f0
0x250ef0370:  4c 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0374:  f6 03 00 aa   mov	x22, x0
0x250ef0378:  40 10 00 b4   cbz	x0, loc_250ef0580 ; ⤵ 0x208
0x250ef037c:  e0 03 15 aa   mov	x0, x21
0x250ef0380:  b0 c3 c4 95   bl	0x258021240
0x250ef0384:  47 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0388:  1f 00 00 f1   cmp	x0, #0
0x250ef038c:  f7 17 9f 1a   cset	w23, eq
0x250ef0390:  7c 40 c8 95   bl	j__objc_release
0x250ef0394:  37 0e 00 35   cbnz	w23, loc_250ef0558 ; ⤵ 0x1c4
0x250ef0398:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef039c:  42 20 02 91   add	x2, x2, #0x88
0x250ef03a0:  e0 03 15 aa   mov	x0, x21
0x250ef03a4:  6f 95 c4 95   bl	0x258015960
0x250ef03a8:  e0 03 15 aa   mov	x0, x21
0x250ef03ac:  a5 c3 c4 95   bl	0x258021240
0x250ef03b0:  3c 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef03b4:  f7 03 00 aa   mov	x23, x0
0x250ef03b8:  e2 03 16 aa   mov	x2, x22
0x250ef03bc:  d1 c6 c4 95   bl	0x258021f00
0x250ef03c0:  84 40 c8 95   bl	j__objc_release_x23
0x250ef03c4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef03c8:  42 20 02 91   add	x2, x2, #0x88
0x250ef03cc:  e0 03 15 aa   mov	x0, x21
0x250ef03d0:  8c 8f c4 95   bl	0x258014200
0x250ef03d4:  6b 00 00 14   b	loc_250ef0580 ; ⤵ 0x1ac
0x250ef03d8:  ; loc_250ef03d8
0x250ef03d8:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef03dc:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef03e0:  c1 03 80 52   mov	w1, #0x1e
0x250ef03e4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef03e8:  76 42 c8 95   bl	j___LogCategory_Initialize
0x250ef03ec:  20 eb ff 35   cbnz	w0, loc_250ef0150 ; ⤴ -0x29c
0x250ef03f0:  af 01 00 14   b	loc_250ef0aac ; ⤵ 0x6bc
0x250ef03f4:  ; loc_250ef03f4
0x250ef03f4:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef03f8:  08 d5 46 f9   ldr	x8, [x8, #0xda8] ; _ptr.1e0a0d460 (CoreMedia/__DATA_CONST.__const)
0x250ef03fc:  17 01 40 f9   ldr	x23, [x8]
0x250ef0400:  e0 03 14 aa   mov	x0, x20
0x250ef0404:  e2 03 17 aa   mov	x2, x23
0x250ef0408:  3a 44 c4 95   bl	0x2580014f0
0x250ef040c:  c0 11 00 34   cbz	w0, loc_250ef0644 ; ⤵ 0x238
0x250ef0410:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0414:  42 20 01 91   add	x2, x2, #0x48
0x250ef0418:  e0 03 13 aa   mov	x0, x19
0x250ef041c:  75 45 c4 95   bl	0x2580019f0
0x250ef0420:  20 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0424:  f6 03 00 aa   mov	x22, x0
0x250ef0428:  e2 03 17 aa   mov	x2, x23
0x250ef042c:  71 45 c4 95   bl	0x2580019f0
0x250ef0430:  1c 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0434:  f7 03 00 aa   mov	x23, x0
0x250ef0438:  22 44 c4 95   bl	0x2580014c0
0x250ef043c:  e0 0f 00 f9   str	x0, [sp, #0x18]
0x250ef0440:  64 40 c8 95   bl	j__objc_release_x23
0x250ef0444:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0448:  08 d1 46 f9   ldr	x8, [x8, #0xda0] ; _ptr.1e0a0d468 (CoreMedia/__DATA_CONST.__const)
0x250ef044c:  02 01 40 f9   ldr	x2, [x8]
0x250ef0450:  e0 03 16 aa   mov	x0, x22
0x250ef0454:  67 45 c4 95   bl	0x2580019f0
0x250ef0458:  12 42 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef045c:  f7 03 00 aa   mov	x23, x0
0x250ef0460:  a8 22 43 39   ldrb	w8, [x21, #0xc8]
0x250ef0464:  68 01 00 36   tbz	w8, #0, loc_250ef0490 ; ⤵ 0x2c
0x250ef0468:  a0 22 04 91   add	x0, x21, #0x108
0x250ef046c:  89 44 c8 95   bl	j__objc_loadWeakRetained
0x250ef0470:  0a 00 00 14   b	loc_250ef0498 ; ⤵ 0x28
0x250ef0474:  ; loc_250ef0474
0x250ef0474:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0478:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef047c:  c1 03 80 52   mov	w1, #0x1e
0x250ef0480:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0484:  4f 42 c8 95   bl	j___LogCategory_Initialize
0x250ef0488:  00 e7 ff 35   cbnz	w0, loc_250ef0168 ; ⤴ -0x320
0x250ef048c:  88 01 00 14   b	loc_250ef0aac ; ⤵ 0x620
0x250ef0490:  ; loc_250ef0490
0x250ef0490:  a8 82 40 f9   ldr	x8, [x21, #0x100]
0x250ef0494:  87 40 c8 95   bl	j__objc_retain_x8
0x250ef0498:  ; loc_250ef0498
0x250ef0498:  f8 03 00 aa   mov	x24, x0
0x250ef049c:  60 07 00 b4   cbz	x0, loc_250ef0588 ; ⤵ 0xec
0x250ef04a0:  e0 03 18 aa   mov	x0, x24
0x250ef04a4:  63 c3 c4 95   bl	0x258021230
0x250ef04a8:  fe 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef04ac:  fb 03 00 aa   mov	x27, x0
0x250ef04b0:  b4 44 c4 95   bl	0x258001780
0x250ef04b4:  fb 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef04b8:  fa 03 00 aa   mov	x26, x0
0x250ef04bc:  55 40 c8 95   bl	j__objc_release_x27
0x250ef04c0:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef04c4:  08 f1 46 f9   ldr	x8, [x8, #0xde0] ; _ptr.1e0a0d470 (CoreMedia/__DATA_CONST.__const)
0x250ef04c8:  02 01 40 f9   ldr	x2, [x8]
0x250ef04cc:  e0 03 16 aa   mov	x0, x22
0x250ef04d0:  48 45 c4 95   bl	0x2580019f0
0x250ef04d4:  f3 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef04d8:  fb 03 00 aa   mov	x27, x0
0x250ef04dc:  1f 00 1a eb   cmp	x0, x26
0x250ef04e0:  a0 00 00 54   b.eq	loc_250ef04f4 ; ⤵ 0x14
0x250ef04e4:  e0 03 1b aa   mov	x0, x27
0x250ef04e8:  e2 03 1a aa   mov	x2, x26
0x250ef04ec:  01 44 c4 95   bl	0x2580014f0
0x250ef04f0:  a0 06 00 34   cbz	w0, loc_250ef05c4 ; ⤵ 0xd4
0x250ef04f4:  ; loc_250ef04f4
0x250ef04f4:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef04f8:  1f 79 00 71   cmp	w8, #0x1e
0x250ef04fc:  0c 09 00 54   b.gt	loc_250ef061c ; ⤵ 0x120
0x250ef0500:  1f 05 00 31   cmn	w8, #0x1
0x250ef0504:  00 08 00 54   b.eq	loc_250ef0604 ; ⤵ 0x100
0x250ef0508:  ; loc_250ef0508
0x250ef0508:  a8 22 43 39   ldrb	w8, [x21, #0xc8]
0x250ef050c:  09 01 00 b0   adrp	x9, 0x250f11000
0x250ef0510:  29 e9 37 91   add	x9, x9, #0xdfa ; "primary"
0x250ef0514:  0a 01 00 b0   adrp	x10, 0x250f11000
0x250ef0518:  4a b5 37 91   add	x10, x10, #0xded ; "interstitial"
0x250ef051c:  1f 01 00 71   cmp	w8, #0
0x250ef0520:  48 11 89 9a   csel	x8, x10, x9, ne
0x250ef0524:  e9 0f 40 f9   ldr	x9, [sp, #0x18]
0x250ef0528:  e9 df 00 a9   stp	x9, x23, [sp, #0x8]
0x250ef052c:  e8 03 00 f9   str	x8, [sp]
0x250ef0530:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0534:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0538:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef053c:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0540:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0544:  63 d4 26 91   add	x3, x3, #0x9b5 ; "Setting %s player skippableState: %d skippableLabel: %@"
0x250ef0548:  c2 03 80 52   mov	w2, #0x1e
0x250ef054c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0550:  6c 42 c8 95   bl	j__LogPrintF
0x250ef0554:  32 00 00 14   b	loc_250ef061c ; ⤵ 0xc8
0x250ef0558:  ; loc_250ef0558
0x250ef0558:  e8 24 0c b0   adrp	x8, 0x26938d000
0x250ef055c:  00 45 45 f9   ldr	x0, [x8, #0xa88] ; "xfO\xf1"
0x250ef0560:  74 42 c8 95   bl	j__objc_alloc
0x250ef0564:  e2 03 16 aa   mov	x2, x22
0x250ef0568:  72 48 c4 95   bl	0x258002730
0x250ef056c:  f7 03 00 aa   mov	x23, x0
0x250ef0570:  e0 03 15 aa   mov	x0, x21
0x250ef0574:  e2 03 17 aa   mov	x2, x23
0x250ef0578:  56 c5 c4 95   bl	0x258021ad0
0x250ef057c:  15 40 c8 95   bl	j__objc_release_x23
0x250ef0580:  ; loc_250ef0580
0x250ef0580:  10 40 c8 95   bl	j__objc_release_x22
0x250ef0584:  30 00 00 14   b	loc_250ef0644 ; ⤵ 0xc0
0x250ef0588:  ; loc_250ef0588
0x250ef0588:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef058c:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0590:  4c 05 00 54   b.gt	loc_250ef0638 ; ⤵ 0xa8
0x250ef0594:  1f 05 00 31   cmn	w8, #0x1
0x250ef0598:  20 2b 00 54   b.eq	loc_250ef0afc ; ⤵ 0x564
0x250ef059c:  ; loc_250ef059c
0x250ef059c:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef05a0:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef05a4:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef05a8:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef05ac:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef05b0:  63 98 25 91   add	x3, x3, #0x966 ; "Ignoring skippableState set property - no relevant interstitialEventController"
0x250ef05b4:  c2 03 80 52   mov	w2, #0x1e
0x250ef05b8:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef05bc:  51 42 c8 95   bl	j__LogPrintF
0x250ef05c0:  1e 00 00 14   b	loc_250ef0638 ; ⤵ 0x78
0x250ef05c4:  ; loc_250ef05c4
0x250ef05c4:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef05c8:  1f 79 00 71   cmp	w8, #0x1e
0x250ef05cc:  2c 03 00 54   b.gt	loc_250ef0630 ; ⤵ 0x64
0x250ef05d0:  1f 05 00 31   cmn	w8, #0x1
0x250ef05d4:  e0 2e 00 54   b.eq	loc_250ef0bb0 ; ⤵ 0x5dc
0x250ef05d8:  ; loc_250ef05d8
0x250ef05d8:  fb 6b 00 a9   stp	x27, x26, [sp]
0x250ef05dc:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef05e0:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef05e4:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef05e8:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef05ec:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef05f0:  63 b4 27 91   add	x3, x3, #0x9ed ; "Ignoring skippableState set property - payload eventID doesn't match the current eventID { %@ != %@ }"
0x250ef05f4:  c2 03 80 52   mov	w2, #0x1e
0x250ef05f8:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef05fc:  41 42 c8 95   bl	j__LogPrintF
0x250ef0600:  0c 00 00 14   b	loc_250ef0630 ; ⤵ 0x30
0x250ef0604:  ; loc_250ef0604
0x250ef0604:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0608:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef060c:  c1 03 80 52   mov	w1, #0x1e
0x250ef0610:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0614:  eb 41 c8 95   bl	j___LogCategory_Initialize
0x250ef0618:  80 f7 ff 35   cbnz	w0, loc_250ef0508 ; ⤴ -0x110
0x250ef061c:  ; loc_250ef061c
0x250ef061c:  e8 0f 40 f9   ldr	x8, [sp, #0x18]
0x250ef0620:  02 7d 40 93   sxtw	x2, w8
0x250ef0624:  e0 03 18 aa   mov	x0, x24
0x250ef0628:  e3 03 17 aa   mov	x3, x23
0x250ef062c:  2d c5 c4 95   bl	0x258021ae0
0x250ef0630:  ; loc_250ef0630
0x250ef0630:  f8 3f c8 95   bl	j__objc_release_x27
0x250ef0634:  f3 3f c8 95   bl	j__objc_release_x26
0x250ef0638:  ; loc_250ef0638
0x250ef0638:  ea 3f c8 95   bl	j__objc_release_x24
0x250ef063c:  e5 3f c8 95   bl	j__objc_release_x23
0x250ef0640:  e0 3f c8 95   bl	j__objc_release_x22
0x250ef0644:  ; loc_250ef0644
0x250ef0644:  e8 23 02 91   add	x8, sp, #0x88
0x250ef0648:  b0 95 0b 90   adrp	x16, 0x2681a4000
0x250ef064c:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250ef0650:  f1 03 08 aa   mov	x17, x8
0x250ef0654:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250ef0658:  30 0a c1 da   pacda	x16, x17
0x250ef065c:  f0 47 00 f9   str	x16, [sp, #0x88]
0x250ef0660:  20 d7 46 fd   ldr	d0, [x25, #0xda8]
0x250ef0664:  e0 4b 00 fd   str	d0, [sp, #0x90]
0x250ef0668:  08 41 00 91   add	x8, x8, #0x10
0x250ef066c:  10 00 00 b0   adrp	x16, 0x250ef1000
0x250ef0670:  10 12 04 91   add	x16, x16, #0x104
0x250ef0674:  10 01 c1 da   pacia	x16, x8
0x250ef0678:  c8 24 0c f0   adrp	x8, 0x26938b000
0x250ef067c:  08 21 38 91   add	x8, x8, #0xe08 ; data_26938be08 (AirPlayReceiverKit/__DATA_CONST.__const)
0x250ef0680:  f0 a3 09 a9   stp	x16, x8, [sp, #0x98]
0x250ef0684:  e8 c3 04 91   add	x8, sp, #0x130
0x250ef0688:  e8 5f 00 f9   str	x8, [sp, #0xb8]
0x250ef068c:  f5 57 00 f9   str	x21, [sp, #0xa8]
0x250ef0690:  f4 3f c8 95   bl	j__objc_retain_x19
0x250ef0694:  f6 03 00 aa   mov	x22, x0
0x250ef0698:  e0 5b 00 f9   str	x0, [sp, #0xb0]
0x250ef069c:  a0 95 0b 90   adrp	x0, 0x2681a4000
0x250ef06a0:  00 ac 44 f9   ldr	x0, [x0, #0x958] ; _ptr.1e6ef4e80 (libdispatch.dylib/__DATA_DIRTY.__data)
0x250ef06a4:  e1 23 02 91   add	x1, sp, #0x88
0x250ef06a8:  6a 45 c8 95   bl	j__dispatch_sync
0x250ef06ac:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef06b0:  08 dd 47 f9   ldr	x8, [x8, #0xfb8] ; _ptr.1e0a0d570 (CoreMedia/__DATA_CONST.__const)
0x250ef06b4:  02 01 40 f9   ldr	x2, [x8]
0x250ef06b8:  e0 03 14 aa   mov	x0, x20
0x250ef06bc:  8d 43 c4 95   bl	0x2580014f0
0x250ef06c0:  c0 01 00 34   cbz	w0, loc_250ef06f8 ; ⤵ 0x38
0x250ef06c4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef06c8:  42 20 01 91   add	x2, x2, #0x48
0x250ef06cc:  e0 03 16 aa   mov	x0, x22
0x250ef06d0:  c8 44 c4 95   bl	0x2580019f0
0x250ef06d4:  73 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef06d8:  f7 03 00 aa   mov	x23, x0
0x250ef06dc:  15 46 c4 95   bl	0x258001f30
0x250ef06e0:  f6 03 00 aa   mov	x22, x0
0x250ef06e4:  bb 3f c8 95   bl	j__objc_release_x23
0x250ef06e8:  a0 06 40 f9   ldr	x0, [x21, #0x8]
0x250ef06ec:  e2 03 16 aa   mov	x2, x22
0x250ef06f0:  cc c4 c4 95   bl	0x258021a20
0x250ef06f4:  eb 00 00 14   b	loc_250ef0aa0 ; ⤵ 0x3ac
0x250ef06f8:  ; loc_250ef06f8
0x250ef06f8:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef06fc:  08 f1 47 f9   ldr	x8, [x8, #0xfe0] ; _ptr.1e0a0d588 (CoreMedia/__DATA_CONST.__const)
0x250ef0700:  02 01 40 f9   ldr	x2, [x8]
0x250ef0704:  e0 03 14 aa   mov	x0, x20
0x250ef0708:  7a 43 c4 95   bl	0x2580014f0
0x250ef070c:  00 05 00 34   cbz	w0, loc_250ef07ac ; ⤵ 0xa0
0x250ef0710:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0714:  42 a0 02 91   add	x2, x2, #0xa8
0x250ef0718:  e0 03 16 aa   mov	x0, x22
0x250ef071c:  f1 42 c4 95   bl	0x2580012e0
0x250ef0720:  60 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0724:  f5 03 00 aa   mov	x21, x0
0x250ef0728:  e8 8e 0b f0   adrp	x8, 0x2680cf000
0x250ef072c:  00 45 43 f9   ldr	x0, [x8, #0x688] ; _ptr.1e6f2dd18 (Foundation/__DATA_DIRTY.__objc_data)
0x250ef0730:  04 42 c8 95   bl	j__objc_opt_class
0x250ef0734:  e1 03 00 aa   mov	x1, x0
0x250ef0738:  e0 03 15 aa   mov	x0, x21
0x250ef073c:  65 45 c8 95   bl	j__objc_opt_isKindOfClass
0x250ef0740:  f7 03 00 aa   mov	x23, x0
0x250ef0744:  9b 3f c8 95   bl	j__objc_release_x21
0x250ef0748:  d7 09 00 36   tbz	w23, #0, loc_250ef0880 ; ⤵ 0x138
0x250ef074c:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0750:  42 a0 02 91   add	x2, x2, #0xa8
0x250ef0754:  e0 03 16 aa   mov	x0, x22
0x250ef0758:  a6 44 c4 95   bl	0x2580019f0
0x250ef075c:  51 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0760:  f7 03 00 aa   mov	x23, x0
0x250ef0764:  57 43 c4 95   bl	0x2580014c0
0x250ef0768:  f5 03 00 aa   mov	x21, x0
0x250ef076c:  99 3f c8 95   bl	j__objc_release_x23
0x250ef0770:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0774:  42 20 01 91   add	x2, x2, #0x48
0x250ef0778:  e0 03 16 aa   mov	x0, x22
0x250ef077c:  9d 44 c4 95   bl	0x2580019f0
0x250ef0780:  48 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0784:  f7 03 00 aa   mov	x23, x0
0x250ef0788:  92 42 c4 95   bl	0x2580011d0
0x250ef078c:  f6 03 00 aa   mov	x22, x0
0x250ef0790:  90 3f c8 95   bl	j__objc_release_x23
0x250ef0794:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0798:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef079c:  e2 03 16 aa   mov	x2, x22
0x250ef07a0:  e3 03 15 aa   mov	x3, x21
0x250ef07a4:  df c4 c4 95   bl	0x258021b20
0x250ef07a8:  be 00 00 14   b	loc_250ef0aa0 ; ⤵ 0x2f8
0x250ef07ac:  ; loc_250ef07ac
0x250ef07ac:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef07b0:  08 f5 47 f9   ldr	x8, [x8, #0xfe8] ; _ptr.1e0a0d590 (CoreMedia/__DATA_CONST.__const)
0x250ef07b4:  02 01 40 f9   ldr	x2, [x8]
0x250ef07b8:  e0 03 14 aa   mov	x0, x20
0x250ef07bc:  4d 43 c4 95   bl	0x2580014f0
0x250ef07c0:  a0 02 00 34   cbz	w0, loc_250ef0814 ; ⤵ 0x54
0x250ef07c4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef07c8:  42 20 01 91   add	x2, x2, #0x48
0x250ef07cc:  e0 03 16 aa   mov	x0, x22
0x250ef07d0:  88 44 c4 95   bl	0x2580019f0
0x250ef07d4:  33 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef07d8:  f6 03 00 aa   mov	x22, x0
0x250ef07dc:  ff 7f 05 a9   stp	xzr, xzr, [sp, #0x50]
0x250ef07e0:  ff 33 00 f9   str	xzr, [sp, #0x60]
0x250ef07e4:  e8 43 01 91   add	x8, sp, #0x50
0x250ef07e8:  ba 79 c8 95   bl	j__CMTimeMakeFromDictionary
0x250ef07ec:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef07f0:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef07f4:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef07f8:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250ef07fc:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0800:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250ef0804:  e2 83 00 91   add	x2, sp, #0x20
0x250ef0808:  ce c4 c4 95   bl	0x258021b40
0x250ef080c:  ; loc_250ef080c
0x250ef080c:  6d 3f c8 95   bl	j__objc_release_x22
0x250ef0810:  a4 00 00 14   b	loc_250ef0aa0 ; ⤵ 0x290
0x250ef0814:  ; loc_250ef0814
0x250ef0814:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef0818:  08 1d 40 f9   ldr	x8, [x8, #0x38] ; _ptr.1e0a0d598 (CoreMedia/__DATA_CONST.__const)
0x250ef081c:  02 01 40 f9   ldr	x2, [x8]
0x250ef0820:  e0 03 14 aa   mov	x0, x20
0x250ef0824:  33 43 c4 95   bl	0x2580014f0
0x250ef0828:  a0 04 00 34   cbz	w0, loc_250ef08bc ; ⤵ 0x94
0x250ef082c:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0830:  42 20 01 91   add	x2, x2, #0x48
0x250ef0834:  e0 03 16 aa   mov	x0, x22
0x250ef0838:  6e 44 c4 95   bl	0x2580019f0
0x250ef083c:  19 41 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0840:  f6 03 00 aa   mov	x22, x0
0x250ef0844:  ff 7f 05 a9   stp	xzr, xzr, [sp, #0x50]
0x250ef0848:  ff 33 00 f9   str	xzr, [sp, #0x60]
0x250ef084c:  e8 43 01 91   add	x8, sp, #0x50
0x250ef0850:  a0 79 c8 95   bl	j__CMTimeMakeFromDictionary
0x250ef0854:  e8 73 41 39   ldrb	w8, [sp, #0x5c]
0x250ef0858:  a8 fd 07 36   tbz	w8, #0, loc_250ef080c ; ⤴ -0x4c
0x250ef085c:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0860:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0864:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef0868:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250ef086c:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0870:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250ef0874:  e2 83 00 91   add	x2, sp, #0x20
0x250ef0878:  06 c5 c4 95   bl	0x258021c90
0x250ef087c:  e4 ff ff 17   b	loc_250ef080c ; ⤴ -0x70
0x250ef0880:  ; loc_250ef0880
0x250ef0880:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0884:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0888:  cc 10 00 54   b.gt	loc_250ef0aa0 ; ⤵ 0x218
0x250ef088c:  1f 05 00 31   cmn	w8, #0x1
0x250ef0890:  80 08 00 54   b.eq	loc_250ef09a0 ; ⤵ 0x110
0x250ef0894:  ; loc_250ef0894
0x250ef0894:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0898:  63 6c 29 91   add	x3, x3, #0xa5b ; "Ignoring EnableTrack set property."
0x250ef089c:  ; loc_250ef089c
0x250ef089c:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef08a0:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef08a4:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef08a8:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef08ac:  c2 03 80 52   mov	w2, #0x1e
0x250ef08b0:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef08b4:  93 41 c8 95   bl	j__LogPrintF
0x250ef08b8:  7a 00 00 14   b	loc_250ef0aa0 ; ⤵ 0x1e8
0x250ef08bc:  ; loc_250ef08bc
0x250ef08bc:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef08c0:  08 21 40 f9   ldr	x8, [x8, #0x40] ; _ptr.1e0a0d5b8 (CoreMedia/__DATA_CONST.__const)
0x250ef08c4:  02 01 40 f9   ldr	x2, [x8]
0x250ef08c8:  e0 03 14 aa   mov	x0, x20
0x250ef08cc:  09 43 c4 95   bl	0x2580014f0
0x250ef08d0:  60 04 00 34   cbz	w0, loc_250ef095c ; ⤵ 0x8c
0x250ef08d4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef08d8:  42 20 01 91   add	x2, x2, #0x48
0x250ef08dc:  e0 03 16 aa   mov	x0, x22
0x250ef08e0:  44 44 c4 95   bl	0x2580019f0
0x250ef08e4:  ef 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef08e8:  f6 03 00 aa   mov	x22, x0
0x250ef08ec:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef08f0:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef08f4:  e2 03 16 aa   mov	x2, x22
0x250ef08f8:  62 c4 c4 95   bl	0x258021a80
0x250ef08fc:  31 3f c8 95   bl	j__objc_release_x22
0x250ef0900:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0904:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0908:  ac 0b 00 54   b.gt	loc_250ef0a7c ; ⤵ 0x174
0x250ef090c:  1f 05 00 31   cmn	w8, #0x1
0x250ef0910:  a0 0a 00 54   b.eq	loc_250ef0a64 ; ⤵ 0x154
0x250ef0914:  ; loc_250ef0914
0x250ef0914:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0918:  17 15 40 f9   ldr	x23, [x8, #0x28]
0x250ef091c:  e0 03 17 aa   mov	x0, x23
0x250ef0920:  20 c2 c4 95   bl	0x2580211a0
0x250ef0924:  df 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0928:  f6 03 00 aa   mov	x22, x0
0x250ef092c:  f7 03 00 a9   stp	x23, x0, [sp]
0x250ef0930:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0934:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0938:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef093c:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0940:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0944:  63 f8 29 91   add	x3, x3, #0xa7e ; "Item = %@. SelectedMediaArray property set to %@."
0x250ef0948:  c2 03 80 52   mov	w2, #0x1e
0x250ef094c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0950:  6c 41 c8 95   bl	j__LogPrintF
0x250ef0954:  1b 3f c8 95   bl	j__objc_release_x22
0x250ef0958:  49 00 00 14   b	loc_250ef0a7c ; ⤵ 0x124
0x250ef095c:  ; loc_250ef095c
0x250ef095c:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef0960:  08 25 40 f9   ldr	x8, [x8, #0x48] ; _ptr.1e0a0d5c8 (CoreMedia/__DATA_CONST.__const)
0x250ef0964:  02 01 40 f9   ldr	x2, [x8]
0x250ef0968:  e0 03 14 aa   mov	x0, x20
0x250ef096c:  e1 42 c4 95   bl	0x2580014f0
0x250ef0970:  60 02 00 34   cbz	w0, loc_250ef09bc ; ⤵ 0x4c
0x250ef0974:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0978:  42 20 01 91   add	x2, x2, #0x48
0x250ef097c:  e0 03 16 aa   mov	x0, x22
0x250ef0980:  1c 44 c4 95   bl	0x2580019f0
0x250ef0984:  c7 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0988:  f6 03 00 aa   mov	x22, x0
0x250ef098c:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0990:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0994:  e2 03 16 aa   mov	x2, x22
0x250ef0998:  e2 c4 c4 95   bl	0x258021d20
0x250ef099c:  9c ff ff 17   b	loc_250ef080c ; ⤴ -0x190
0x250ef09a0:  ; loc_250ef09a0
0x250ef09a0:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef09a4:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef09a8:  c1 03 80 52   mov	w1, #0x1e
0x250ef09ac:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef09b0:  04 41 c8 95   bl	j___LogCategory_Initialize
0x250ef09b4:  00 f7 ff 35   cbnz	w0, loc_250ef0894 ; ⤴ -0x120
0x250ef09b8:  3a 00 00 14   b	loc_250ef0aa0 ; ⤵ 0xe8
0x250ef09bc:  ; loc_250ef09bc
0x250ef09bc:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef09c0:  08 e5 47 f9   ldr	x8, [x8, #0xfc8] ; _ptr.1e0a0d630 (CoreMedia/__DATA_CONST.__const)
0x250ef09c4:  02 01 40 f9   ldr	x2, [x8]
0x250ef09c8:  e0 03 14 aa   mov	x0, x20
0x250ef09cc:  c9 42 c4 95   bl	0x2580014f0
0x250ef09d0:  e0 01 00 34   cbz	w0, loc_250ef0a0c ; ⤵ 0x3c
0x250ef09d4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef09d8:  42 20 01 91   add	x2, x2, #0x48
0x250ef09dc:  e0 03 16 aa   mov	x0, x22
0x250ef09e0:  04 44 c4 95   bl	0x2580019f0
0x250ef09e4:  af 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef09e8:  f6 03 00 aa   mov	x22, x0
0x250ef09ec:  e0 03 15 aa   mov	x0, x21
0x250ef09f0:  e2 03 16 aa   mov	x2, x22
0x250ef09f4:  33 c4 c4 95   bl	0x258021ac0
0x250ef09f8:  a2 72 40 f9   ldr	x2, [x21, #0xe0]
0x250ef09fc:  82 f0 ff b4   cbz	x2, loc_250ef080c ; ⤴ -0x1f0
0x250ef0a00:  a0 06 40 f9   ldr	x0, [x21, #0x8]
0x250ef0a04:  b7 c0 c4 95   bl	0x258020ce0
0x250ef0a08:  81 ff ff 17   b	loc_250ef080c ; ⤴ -0x1fc
0x250ef0a0c:  ; loc_250ef0a0c
0x250ef0a0c:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0a10:  08 fd 47 f9   ldr	x8, [x8, #0xff8] ; _ptr.1e0a0d620 (CoreMedia/__DATA_CONST.__const)
0x250ef0a14:  02 01 40 f9   ldr	x2, [x8]
0x250ef0a18:  e0 03 14 aa   mov	x0, x20
0x250ef0a1c:  b5 42 c4 95   bl	0x2580014f0
0x250ef0a20:  c0 07 00 34   cbz	w0, loc_250ef0b18 ; ⤵ 0xf8
0x250ef0a24:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0a28:  42 20 01 91   add	x2, x2, #0x48
0x250ef0a2c:  e0 03 16 aa   mov	x0, x22
0x250ef0a30:  f0 43 c4 95   bl	0x2580019f0
0x250ef0a34:  9b 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0a38:  f6 03 15 aa   mov	x22, x21
0x250ef0a3c:  c8 8e 4d f8   ldr	x8, [x22, #0xd8]!
0x250ef0a40:  c0 02 00 f9   str	x0, [x22]
0x250ef0a44:  fb 3e c8 95   bl	j__objc_release_x8
0x250ef0a48:  c2 02 40 f9   ldr	x2, [x22]
0x250ef0a4c:  a2 02 00 b4   cbz	x2, loc_250ef0aa0 ; ⤵ 0x54
0x250ef0a50:  a0 06 40 f9   ldr	x0, [x21, #0x8]
0x250ef0a54:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0a58:  03 15 40 f9   ldr	x3, [x8, #0x28]
0x250ef0a5c:  a5 c0 c4 95   bl	0x258020cf0
0x250ef0a60:  10 00 00 14   b	loc_250ef0aa0 ; ⤵ 0x40
0x250ef0a64:  ; loc_250ef0a64
0x250ef0a64:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0a68:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0a6c:  c1 03 80 52   mov	w1, #0x1e
0x250ef0a70:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0a74:  d3 40 c8 95   bl	j___LogCategory_Initialize
0x250ef0a78:  e0 f4 ff 35   cbnz	w0, loc_250ef0914 ; ⤴ -0x164
0x250ef0a7c:  ; loc_250ef0a7c
0x250ef0a7c:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0a80:  02 15 40 f9   ldr	x2, [x8, #0x28]
0x250ef0a84:  e0 03 15 aa   mov	x0, x21
0x250ef0a88:  3e c0 c4 95   bl	0x258020b80
0x250ef0a8c:  a0 00 00 34   cbz	w0, loc_250ef0aa0 ; ⤵ 0x14
0x250ef0a90:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0a94:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0a98:  02 00 80 d2   mov	x2, #0
0x250ef0a9c:  f9 c3 c4 95   bl	0x258021a80
0x250ef0aa0:  ; loc_250ef0aa0
0x250ef0aa0:  e8 5b 40 f9   ldr	x8, [sp, #0xb0]
0x250ef0aa4:  e3 3e c8 95   bl	j__objc_release_x8
0x250ef0aa8:  be 3e c8 95   bl	j__objc_release_x20
0x250ef0aac:  ; loc_250ef0aac
0x250ef0aac:  e0 c3 04 91   add	x0, sp, #0x130
0x250ef0ab0:  01 01 80 52   mov	w1, #0x8
0x250ef0ab4:  53 44 c8 95   bl	j___Block_object_dispose
0x250ef0ab8:  e8 af 40 f9   ldr	x8, [sp, #0x158]
0x250ef0abc:  dd 3e c8 95   bl	j__objc_release_x8
0x250ef0ac0:  b4 3e c8 95   bl	j__objc_release_x19
0x250ef0ac4:  a8 03 5a f8   ldur	x8, [fp, #-0x60]
0x250ef0ac8:  a9 95 0b 90   adrp	x9, 0x2681a4000
0x250ef0acc:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.1e6ef16f0 (libsystem_c.dylib/__DATA_DIRTY.__common)
0x250ef0ad0:  29 01 40 f9   ldr	x9, [x9]
0x250ef0ad4:  3f 01 08 eb   cmp	x9, x8
0x250ef0ad8:  e1 27 00 54   b.ne	loc_250ef0fd4 ; ⤵ 0x4fc
0x250ef0adc:  ff c3 07 91   add	sp, sp, #0x1f0
0x250ef0ae0:  fd 7b 45 a9   ldp	fp, lr, [sp, #0x50]
0x250ef0ae4:  f4 4f 44 a9   ldp	x20, x19, [sp, #0x40]
0x250ef0ae8:  f6 57 43 a9   ldp	x22, x21, [sp, #0x30]
0x250ef0aec:  f8 5f 42 a9   ldp	x24, x23, [sp, #0x20]
0x250ef0af0:  fa 67 41 a9   ldp	x26, x25, [sp, #0x10]
0x250ef0af4:  fc 6f c6 a8   ldp	x28, x27, [sp], #0x60
0x250ef0af8:  ff 0f 5f d6   retab
0x250ef0afc:  ; loc_250ef0afc
0x250ef0afc:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0b00:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0b04:  c1 03 80 52   mov	w1, #0x1e
0x250ef0b08:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0b0c:  ad 40 c8 95   bl	j___LogCategory_Initialize
0x250ef0b10:  60 d4 ff 35   cbnz	w0, loc_250ef059c ; ⤴ -0x574
0x250ef0b14:  c9 fe ff 17   b	loc_250ef0638 ; ⤴ -0x4dc
0x250ef0b18:  ; loc_250ef0b18
0x250ef0b18:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0b1c:  08 49 47 f9   ldr	x8, [x8, #0xe90] ; _ptr.1e0a0d428 (CoreMedia/__DATA_CONST.__const)
0x250ef0b20:  02 01 40 f9   ldr	x2, [x8]
0x250ef0b24:  e0 03 14 aa   mov	x0, x20
0x250ef0b28:  72 42 c4 95   bl	0x2580014f0
0x250ef0b2c:  00 05 00 34   cbz	w0, loc_250ef0bcc ; ⤵ 0xa0
0x250ef0b30:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0b34:  42 20 01 91   add	x2, x2, #0x48
0x250ef0b38:  e0 03 16 aa   mov	x0, x22
0x250ef0b3c:  ad 43 c4 95   bl	0x2580019f0
0x250ef0b40:  58 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0b44:  f6 03 00 aa   mov	x22, x0
0x250ef0b48:  ff 7f 05 a9   stp	xzr, xzr, [sp, #0x50]
0x250ef0b4c:  ff 33 00 f9   str	xzr, [sp, #0x60]
0x250ef0b50:  e8 43 01 91   add	x8, sp, #0x50
0x250ef0b54:  df 78 c8 95   bl	j__CMTimeMakeFromDictionary
0x250ef0b58:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0b5c:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0b60:  0c 0e 00 54   b.gt	loc_250ef0d20 ; ⤵ 0x1c0
0x250ef0b64:  1f 05 00 31   cmn	w8, #0x1
0x250ef0b68:  00 0d 00 54   b.eq	loc_250ef0d08 ; ⤵ 0x1a0
0x250ef0b6c:  ; loc_250ef0b6c
0x250ef0b6c:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef0b70:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250ef0b74:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0b78:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250ef0b7c:  e0 83 00 91   add	x0, sp, #0x20
0x250ef0b80:  e0 72 c8 95   bl	j__CMTimeGetSeconds
0x250ef0b84:  e0 03 00 fd   str	d0, [sp]
0x250ef0b88:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0b8c:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0b90:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef0b94:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0b98:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0b9c:  63 c0 2a 91   add	x3, x3, #0xab0 ; "TimeToPauseBuffering property set to %f."
0x250ef0ba0:  c2 03 80 52   mov	w2, #0x1e
0x250ef0ba4:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0ba8:  d6 40 c8 95   bl	j__LogPrintF
0x250ef0bac:  5d 00 00 14   b	loc_250ef0d20 ; ⤵ 0x174
0x250ef0bb0:  ; loc_250ef0bb0
0x250ef0bb0:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0bb4:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0bb8:  c1 03 80 52   mov	w1, #0x1e
0x250ef0bbc:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0bc0:  80 40 c8 95   bl	j___LogCategory_Initialize
0x250ef0bc4:  a0 d0 ff 35   cbnz	w0, loc_250ef05d8 ; ⤴ -0x5ec
0x250ef0bc8:  9a fe ff 17   b	loc_250ef0630 ; ⤴ -0x598
0x250ef0bcc:  ; loc_250ef0bcc
0x250ef0bcc:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0bd0:  08 4d 47 f9   ldr	x8, [x8, #0xe98] ; _ptr.1e0a0d430 (CoreMedia/__DATA_CONST.__const)
0x250ef0bd4:  02 01 40 f9   ldr	x2, [x8]
0x250ef0bd8:  e0 03 14 aa   mov	x0, x20
0x250ef0bdc:  45 42 c4 95   bl	0x2580014f0
0x250ef0be0:  20 04 00 34   cbz	w0, loc_250ef0c64 ; ⤵ 0x84
0x250ef0be4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0be8:  42 20 01 91   add	x2, x2, #0x48
0x250ef0bec:  e0 03 16 aa   mov	x0, x22
0x250ef0bf0:  80 43 c4 95   bl	0x2580019f0
0x250ef0bf4:  2b 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0bf8:  f6 03 00 aa   mov	x22, x0
0x250ef0bfc:  ff 7f 05 a9   stp	xzr, xzr, [sp, #0x50]
0x250ef0c00:  ff 33 00 f9   str	xzr, [sp, #0x60]
0x250ef0c04:  e8 43 01 91   add	x8, sp, #0x50
0x250ef0c08:  b2 78 c8 95   bl	j__CMTimeMakeFromDictionary
0x250ef0c0c:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0c10:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0c14:  ac 0c 00 54   b.gt	loc_250ef0da8 ; ⤵ 0x194
0x250ef0c18:  1f 05 00 31   cmn	w8, #0x1
0x250ef0c1c:  a0 0b 00 54   b.eq	loc_250ef0d90 ; ⤵ 0x174
0x250ef0c20:  ; loc_250ef0c20
0x250ef0c20:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef0c24:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250ef0c28:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0c2c:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250ef0c30:  e0 83 00 91   add	x0, sp, #0x20
0x250ef0c34:  b3 72 c8 95   bl	j__CMTimeGetSeconds
0x250ef0c38:  e0 03 00 fd   str	d0, [sp]
0x250ef0c3c:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0c40:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0c44:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef0c48:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0c4c:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0c50:  63 64 2b 91   add	x3, x3, #0xad9 ; "TimeToPausePlayback property set to %f."
0x250ef0c54:  c2 03 80 52   mov	w2, #0x1e
0x250ef0c58:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0c5c:  a9 40 c8 95   bl	j__LogPrintF
0x250ef0c60:  52 00 00 14   b	loc_250ef0da8 ; ⤵ 0x148
0x250ef0c64:  ; loc_250ef0c64
0x250ef0c64:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0c68:  08 39 47 f9   ldr	x8, [x8, #0xe70] ; _ptr.1e0a0d440 (CoreMedia/__DATA_CONST.__const)
0x250ef0c6c:  02 01 40 f9   ldr	x2, [x8]
0x250ef0c70:  e0 03 14 aa   mov	x0, x20
0x250ef0c74:  1f 42 c4 95   bl	0x2580014f0
0x250ef0c78:  e0 01 00 34   cbz	w0, loc_250ef0cb4 ; ⤵ 0x3c
0x250ef0c7c:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0c80:  42 20 01 91   add	x2, x2, #0x48
0x250ef0c84:  e0 03 16 aa   mov	x0, x22
0x250ef0c88:  5a 43 c4 95   bl	0x2580019f0
0x250ef0c8c:  05 40 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0c90:  f6 03 00 aa   mov	x22, x0
0x250ef0c94:  4f 41 c4 95   bl	0x2580011d0
0x250ef0c98:  f5 03 00 aa   mov	x21, x0
0x250ef0c9c:  49 3e c8 95   bl	j__objc_release_x22
0x250ef0ca0:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0ca4:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0ca8:  e2 03 15 aa   mov	x2, x21
0x250ef0cac:  0d c4 c4 95   bl	0x258021ce0
0x250ef0cb0:  7c ff ff 17   b	loc_250ef0aa0 ; ⤴ -0x210
0x250ef0cb4:  ; loc_250ef0cb4
0x250ef0cb4:  e8 8e 0b f0   adrp	x8, 0x2680cf000
0x250ef0cb8:  00 c5 43 f9   ldr	x0, [x8, #0x788] ; _ptr.1e6f2dca0 (Foundation/__DATA_DIRTY.__objc_data)
0x250ef0cbc:  02 01 00 b0   adrp	x2, 0x250f11000
0x250ef0cc0:  42 18 3d 91   add	x2, x2, #0xf46 ; "audioMode"
0x250ef0cc4:  5f 45 c4 95   bl	0x258002240
0x250ef0cc8:  f6 3f c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0ccc:  f7 03 00 aa   mov	x23, x0
0x250ef0cd0:  e0 03 14 aa   mov	x0, x20
0x250ef0cd4:  e2 03 17 aa   mov	x2, x23
0x250ef0cd8:  06 42 c4 95   bl	0x2580014f0
0x250ef0cdc:  f8 03 00 aa   mov	x24, x0
0x250ef0ce0:  3c 3e c8 95   bl	j__objc_release_x23
0x250ef0ce4:  98 03 00 34   cbz	w24, loc_250ef0d54 ; ⤵ 0x70
0x250ef0ce8:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0cec:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0cf0:  8c ed ff 54   b.gt	loc_250ef0aa0 ; ⤴ -0x250
0x250ef0cf4:  1f 05 00 31   cmn	w8, #0x1
0x250ef0cf8:  80 11 00 54   b.eq	loc_250ef0f28 ; ⤵ 0x230
0x250ef0cfc:  ; loc_250ef0cfc
0x250ef0cfc:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0d00:  63 04 2c 91   add	x3, x3, #0xb01 ; "audioMode property setter not supported on this platform."
0x250ef0d04:  e6 fe ff 17   b	loc_250ef089c ; ⤴ -0x468
0x250ef0d08:  ; loc_250ef0d08
0x250ef0d08:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0d0c:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0d10:  c1 03 80 52   mov	w1, #0x1e
0x250ef0d14:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0d18:  2a 40 c8 95   bl	j___LogCategory_Initialize
0x250ef0d1c:  80 f2 ff 35   cbnz	w0, loc_250ef0b6c ; ⤴ -0x1b0
0x250ef0d20:  ; loc_250ef0d20
0x250ef0d20:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0d24:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0d28:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef0d2c:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250ef0d30:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0d34:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250ef0d38:  e2 83 00 91   add	x2, sp, #0x20
0x250ef0d3c:  fd c3 c4 95   bl	0x258021d30
0x250ef0d40:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0d44:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0d48:  02 00 80 52   mov	w2, #0
0x250ef0d4c:  d5 c3 c4 95   bl	0x258021ca0
0x250ef0d50:  af fe ff 17   b	loc_250ef080c ; ⤴ -0x544
0x250ef0d54:  ; loc_250ef0d54
0x250ef0d54:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0d58:  08 ed 47 f9   ldr	x8, [x8, #0xfd8] ; _ptr.1e0a0d5e0 (CoreMedia/__DATA_CONST.__const)
0x250ef0d5c:  02 01 40 f9   ldr	x2, [x8]
0x250ef0d60:  e0 03 14 aa   mov	x0, x20
0x250ef0d64:  e3 41 c4 95   bl	0x2580014f0
0x250ef0d68:  a0 03 00 34   cbz	w0, loc_250ef0ddc ; ⤵ 0x74
0x250ef0d6c:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0d70:  42 20 01 91   add	x2, x2, #0x48
0x250ef0d74:  e0 03 16 aa   mov	x0, x22
0x250ef0d78:  1e 43 c4 95   bl	0x2580019f0
0x250ef0d7c:  c9 3f c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0d80:  f6 03 00 aa   mov	x22, x0
0x250ef0d84:  13 41 c4 95   bl	0x2580011d0
0x250ef0d88:  a0 ae 02 39   strb	w0, [x21, #0xab]
0x250ef0d8c:  a0 fe ff 17   b	loc_250ef080c ; ⤴ -0x580
0x250ef0d90:  ; loc_250ef0d90
0x250ef0d90:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0d94:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0d98:  c1 03 80 52   mov	w1, #0x1e
0x250ef0d9c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0da0:  08 40 c8 95   bl	j___LogCategory_Initialize
0x250ef0da4:  e0 f3 ff 35   cbnz	w0, loc_250ef0c20 ; ⤴ -0x184
0x250ef0da8:  ; loc_250ef0da8
0x250ef0da8:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0dac:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0db0:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef0db4:  e0 0b 80 3d   str	q0, [sp, #0x20]
0x250ef0db8:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0dbc:  e8 1b 00 f9   str	x8, [sp, #0x30]
0x250ef0dc0:  e2 83 00 91   add	x2, sp, #0x20
0x250ef0dc4:  df c3 c4 95   bl	0x258021d40
0x250ef0dc8:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250ef0dcc:  a0 16 80 3d   str	q0, [x21, #0x50]
0x250ef0dd0:  e8 33 40 f9   ldr	x8, [sp, #0x60]
0x250ef0dd4:  a8 32 00 f9   str	x8, [x21, #0x60]
0x250ef0dd8:  8d fe ff 17   b	loc_250ef080c ; ⤴ -0x5cc
0x250ef0ddc:  ; loc_250ef0ddc
0x250ef0ddc:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0de0:  08 15 47 f9   ldr	x8, [x8, #0xe28] ; _ptr.1e0a0d360 (CoreMedia/__DATA_CONST.__const)
0x250ef0de4:  02 01 40 f9   ldr	x2, [x8]
0x250ef0de8:  e0 03 14 aa   mov	x0, x20
0x250ef0dec:  c1 41 c4 95   bl	0x2580014f0
0x250ef0df0:  40 03 00 34   cbz	w0, loc_250ef0e58 ; ⤵ 0x68
0x250ef0df4:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0df8:  42 20 01 91   add	x2, x2, #0x48
0x250ef0dfc:  e0 03 16 aa   mov	x0, x22
0x250ef0e00:  fc 42 c4 95   bl	0x2580019f0
0x250ef0e04:  a7 3f c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0e08:  f6 03 00 aa   mov	x22, x0
0x250ef0e0c:  49 44 c4 95   bl	0x258001f30
0x250ef0e10:  f5 03 00 aa   mov	x21, x0
0x250ef0e14:  eb 3d c8 95   bl	j__objc_release_x22
0x250ef0e18:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0e1c:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0e20:  ec 09 00 54   b.gt	loc_250ef0f5c ; ⤵ 0x13c
0x250ef0e24:  1f 05 00 31   cmn	w8, #0x1
0x250ef0e28:  e0 08 00 54   b.eq	loc_250ef0f44 ; ⤵ 0x11c
0x250ef0e2c:  ; loc_250ef0e2c
0x250ef0e2c:  f5 03 00 f9   str	x21, [sp]
0x250ef0e30:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0e34:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0e38:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef0e3c:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0e40:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0e44:  63 ec 2c 91   add	x3, x3, #0xb3b ; "PlaybackRestrictions property set to %ld"
0x250ef0e48:  c2 03 80 52   mov	w2, #0x1e
0x250ef0e4c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0e50:  2c 40 c8 95   bl	j__LogPrintF
0x250ef0e54:  42 00 00 14   b	loc_250ef0f5c ; ⤵ 0x108
0x250ef0e58:  ; loc_250ef0e58
0x250ef0e58:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef0e5c:  08 09 40 f9   ldr	x8, [x8, #0x10] ; _ptr.1e0a0d5f0 (CoreMedia/__DATA_CONST.__const)
0x250ef0e60:  02 01 40 f9   ldr	x2, [x8]
0x250ef0e64:  e0 03 14 aa   mov	x0, x20
0x250ef0e68:  a2 41 c4 95   bl	0x2580014f0
0x250ef0e6c:  a0 e1 07 37   tbnz	w0, #0, loc_250ef0aa0 ; ⤴ -0x3cc
0x250ef0e70:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0e74:  42 a0 01 91   add	x2, x2, #0x68
0x250ef0e78:  e0 03 14 aa   mov	x0, x20
0x250ef0e7c:  9d 41 c4 95   bl	0x2580014f0
0x250ef0e80:  00 e1 07 37   tbnz	w0, #0, loc_250ef0aa0 ; ⤴ -0x3e0
0x250ef0e84:  a8 8e 0b 90   adrp	x8, 0x2680c4000
0x250ef0e88:  08 f9 47 f9   ldr	x8, [x8, #0xff0] ; _ptr.1e0a0d628 (CoreMedia/__DATA_CONST.__const)
0x250ef0e8c:  02 01 40 f9   ldr	x2, [x8]
0x250ef0e90:  e0 03 14 aa   mov	x0, x20
0x250ef0e94:  97 41 c4 95   bl	0x2580014f0
0x250ef0e98:  c0 06 00 34   cbz	w0, loc_250ef0f70 ; ⤵ 0xd8
0x250ef0e9c:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0ea0:  42 20 01 91   add	x2, x2, #0x48
0x250ef0ea4:  e0 03 16 aa   mov	x0, x22
0x250ef0ea8:  d2 42 c4 95   bl	0x2580019f0
0x250ef0eac:  7d 3f c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0eb0:  f6 03 00 aa   mov	x22, x0
0x250ef0eb4:  00 e4 00 6f   movi	v0.2d, #0
0x250ef0eb8:  e0 03 03 ad   stp	q0, q0, [sp, #0x60]
0x250ef0ebc:  e0 17 80 3d   str	q0, [sp, #0x50]
0x250ef0ec0:  e8 43 01 91   add	x8, sp, #0x50
0x250ef0ec4:  07 78 c8 95   bl	j__CMTimeRangeMakeFromDictionary
0x250ef0ec8:  e8 73 41 39   ldrb	w8, [sp, #0x5c]
0x250ef0ecc:  08 ca 07 36   tbz	w8, #0, loc_250ef080c ; ⤴ -0x6c0
0x250ef0ed0:  e8 d3 41 39   ldrb	w8, [sp, #0x74]
0x250ef0ed4:  c8 c9 07 36   tbz	w8, #0, loc_250ef080c ; ⤴ -0x6c8
0x250ef0ed8:  e8 3f 40 f9   ldr	x8, [sp, #0x78]
0x250ef0edc:  88 c9 ff b5   cbnz	x8, loc_250ef080c ; ⤴ -0x6d0
0x250ef0ee0:  e8 37 40 f9   ldr	x8, [sp, #0x68]
0x250ef0ee4:  48 c9 ff b7   tbnz	x8, #0x3f, loc_250ef080c ; ⤴ -0x6d8
0x250ef0ee8:  88 2b 43 b9   ldr	w8, [x28, #0x328]
0x250ef0eec:  1f 79 00 71   cmp	w8, #0x1e
0x250ef0ef0:  0c 08 00 54   b.gt	loc_250ef0ff0 ; ⤵ 0x100
0x250ef0ef4:  1f 05 00 31   cmn	w8, #0x1
0x250ef0ef8:  00 07 00 54   b.eq	loc_250ef0fd8 ; ⤵ 0xe0
0x250ef0efc:  ; loc_250ef0efc
0x250ef0efc:  f6 03 00 f9   str	x22, [sp]
0x250ef0f00:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0f04:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0f08:  01 01 00 d0   adrp	x1, 0x250f12000
0x250ef0f0c:  21 a0 22 91   add	x1, x1, #0x8a8 ; "-[APRKMediaPlayer _setPropertyWithDictionary:]"
0x250ef0f10:  03 01 00 d0   adrp	x3, 0x250f12000
0x250ef0f14:  63 90 2d 91   add	x3, x3, #0xb64 ; "InterstitialEventItemTimeOffset set to %@"
0x250ef0f18:  c2 03 80 52   mov	w2, #0x1e
0x250ef0f1c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250ef0f20:  f8 3f c8 95   bl	j__LogPrintF
0x250ef0f24:  33 00 00 14   b	loc_250ef0ff0 ; ⤵ 0xcc
0x250ef0f28:  ; loc_250ef0f28
0x250ef0f28:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0f2c:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0f30:  c1 03 80 52   mov	w1, #0x1e
0x250ef0f34:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0f38:  a2 3f c8 95   bl	j___LogCategory_Initialize
0x250ef0f3c:  20 db ff 34   cbz	w0, loc_250ef0aa0 ; ⤴ -0x49c
0x250ef0f40:  6f ff ff 17   b	loc_250ef0cfc ; ⤴ -0x244
0x250ef0f44:  ; loc_250ef0f44
0x250ef0f44:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0f48:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0f4c:  c1 03 80 52   mov	w1, #0x1e
0x250ef0f50:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0f54:  9b 3f c8 95   bl	j___LogCategory_Initialize
0x250ef0f58:  a0 f6 ff 35   cbnz	w0, loc_250ef0e2c ; ⤴ -0x12c
0x250ef0f5c:  ; loc_250ef0f5c
0x250ef0f5c:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0f60:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0f64:  e2 03 15 aa   mov	x2, x21
0x250ef0f68:  46 c3 c4 95   bl	0x258021c80
0x250ef0f6c:  cd fe ff 17   b	loc_250ef0aa0 ; ⤴ -0x4cc
0x250ef0f70:  ; loc_250ef0f70
0x250ef0f70:  a8 8e 0b b0   adrp	x8, 0x2680c5000
0x250ef0f74:  08 0d 40 f9   ldr	x8, [x8, #0x18] ; _ptr.1e0a0d5f8 (CoreMedia/__DATA_CONST.__const)
0x250ef0f78:  02 01 40 f9   ldr	x2, [x8]
0x250ef0f7c:  e0 03 14 aa   mov	x0, x20
0x250ef0f80:  5c 41 c4 95   bl	0x2580014f0
0x250ef0f84:  e0 d8 ff 34   cbz	w0, loc_250ef0aa0 ; ⤴ -0x4e4
0x250ef0f88:  c2 5f 12 b0   adrp	x2, 0x275ae9000
0x250ef0f8c:  42 20 01 91   add	x2, x2, #0x48
0x250ef0f90:  e0 03 16 aa   mov	x0, x22
0x250ef0f94:  97 42 c4 95   bl	0x2580019f0
0x250ef0f98:  42 3f c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250ef0f9c:  f6 03 00 aa   mov	x22, x0
0x250ef0fa0:  88 8e 0b d0   adrp	x8, 0x2680c2000
0x250ef0fa4:  00 3d 47 f9   ldr	x0, [x8, #0xe78] ; _ptr.1e6f3db38 (CoreFoundation/__DATA_DIRTY.__objc_data)
0x250ef0fa8:  e6 3f c8 95   bl	j__objc_opt_class
0x250ef0fac:  e1 03 00 aa   mov	x1, x0
0x250ef0fb0:  e0 03 16 aa   mov	x0, x22
0x250ef0fb4:  47 43 c8 95   bl	j__objc_opt_isKindOfClass
0x250ef0fb8:  a0 c2 07 36   tbz	w0, #0, loc_250ef080c ; ⤴ -0x7ac
0x250ef0fbc:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0fc0:  03 15 40 f9   ldr	x3, [x8, #0x28]
0x250ef0fc4:  e0 03 15 aa   mov	x0, x21
0x250ef0fc8:  e2 03 16 aa   mov	x2, x22
0x250ef0fcc:  e9 bf c4 95   bl	0x258020f70
0x250ef0fd0:  0f fe ff 17   b	loc_250ef080c ; ⤴ -0x7c4
0x250ef0fd4:  ; loc_250ef0fd4
0x250ef0fd4:  37 3d c8 95   bl	j____stack_chk_fail
0x250ef0fd8:  ; loc_250ef0fd8
0x250ef0fd8:  00 c3 0e f0   adrp	x0, 0x26e753000
0x250ef0fdc:  00 a0 0c 91   add	x0, x0, #0x328 ; "\xff\xff\xff\xff2"
0x250ef0fe0:  c1 03 80 52   mov	w1, #0x1e
0x250ef0fe4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250ef0fe8:  76 3f c8 95   bl	j___LogCategory_Initialize
0x250ef0fec:  80 f8 ff 35   cbnz	w0, loc_250ef0efc ; ⤴ -0xf0
0x250ef0ff0:  ; loc_250ef0ff0
0x250ef0ff0:  e8 9f 40 f9   ldr	x8, [sp, #0x138]
0x250ef0ff4:  00 15 40 f9   ldr	x0, [x8, #0x28]
0x250ef0ff8:  e0 87 42 ad   ldp	q0, q1, [sp, #0x50]
0x250ef0ffc:  e0 07 01 ad   stp	q0, q1, [sp, #0x20]
0x250ef1000:  e0 1f c0 3d   ldr	q0, [sp, #0x70]
0x250ef1004:  e0 13 80 3d   str	q0, [sp, #0x40]
0x250ef1008:  e2 83 00 91   add	x2, sp, #0x20
0x250ef100c:  dd c2 c4 95   bl	0x258021b80
0x250ef1010:  ff fd ff 17   b	loc_250ef080c ; ⤴ -0x804

