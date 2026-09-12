
+[CPAccNavUpdate update:valueForParamKey:]
0x24712501c:  7f 23 03 d5   pacibsp
0x247125020:  fa 67 bb a9   stp	x26, x25, [sp, #-0x50]!
0x247125024:  f8 5f 01 a9   stp	x24, x23, [sp, #0x10]
0x247125028:  f6 57 02 a9   stp	x22, x21, [sp, #0x20]
0x24712502c:  f4 4f 03 a9   stp	x20, x19, [sp, #0x30]
0x247125030:  fd 7b 04 a9   stp	fp, lr, [sp, #0x40]
0x247125034:  fd 03 01 91   add	fp, sp, #0x40
0x247125038:  f4 03 02 aa   mov	x20, x2
0x24712503c:  f5 03 00 aa   mov	x21, x0
0x247125040:  bc 6b 3d 94   bl	_objc_retain_x3_stub
0x247125044:  f3 03 00 aa   mov	x19, x0
0x247125048:  52 63 3d 94   bl	_objc_retain_x20_stub
0x24712504c:  f6 03 00 aa   mov	x22, x0
0x247125050:  e0 03 13 aa   mov	x0, x19
0x247125054:  cf 08 3d 94   bl	0x248067390
0x247125058:  2a 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x24712505c:  f7 03 00 aa   mov	x23, x0
0x247125060:  b4 9c 3c 94   bl	0x24804c330
0x247125064:  27 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125068:  f8 03 00 aa   mov	x24, x0
0x24712506c:  e0 03 16 aa   mov	x0, x22
0x247125070:  e2 03 18 aa   mov	x2, x24
0x247125074:  03 9c 3b 94   bl	0x24800c080
0x247125078:  22 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x24712507c:  f4 03 00 aa   mov	x20, x0
0x247125080:  34 63 3d 94   bl	_objc_release_x22_stub
0x247125084:  57 6b 3d 94   bl	_objc_release_x24_stub
0x247125088:  36 63 3d 94   bl	_objc_release_x23_stub
0x24712508c:  34 0f 00 b4   cbz	x20, loc_247125270 ; ⤵ 0x1e4
0x247125090:  e0 03 13 aa   mov	x0, x19
0x247125094:  bf 08 3d 94   bl	0x248067390
0x247125098:  1a 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x24712509c:  f6 03 00 aa   mov	x22, x0
0x2471250a0:  60 08 3d 94   bl	0x248067220
0x2471250a4:  f7 03 00 aa   mov	x23, x0
0x2471250a8:  48 7d 10 d0   adrp	x8, 0x2680cf000
0x2471250ac:  00 01 43 f9   ldr	x0, [x8, #0x600] ; _ptr.NSMeasurement
0x2471250b0:  30 6b 3d 94   bl	_objc_opt_class_stub
0x2471250b4:  f8 03 00 aa   mov	x24, x0
0x2471250b8:  26 63 3d 94   bl	_objc_release_x22_stub
0x2471250bc:  ff 02 18 eb   cmp	x23, x24
0x2471250c0:  80 03 00 54   b.eq	loc_247125130 ; ⤵ 0x70
0x2471250c4:  e0 03 13 aa   mov	x0, x19
0x2471250c8:  0a 03 3d 94   bl	0x248065cf0
0x2471250cc:  80 00 00 35   cbnz	w0, loc_2471250dc ; ⤵ 0x10
0x2471250d0:  e0 03 13 aa   mov	x0, x19
0x2471250d4:  ef 05 3d 94   bl	0x248066890
0x2471250d8:  80 01 00 34   cbz	w0, loc_247125108 ; ⤵ 0x30
0x2471250dc:  ; loc_2471250dc
0x2471250dc:  e0 03 13 aa   mov	x0, x19
0x2471250e0:  c8 ff 3c 94   bl	0x248065000
0x2471250e4:  3a f9 ff 97   bl	_NotSetFromCPAccNavType
0x2471250e8:  06 63 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x2471250ec:  f6 03 00 aa   mov	x22, x0
0x2471250f0:  e0 03 14 aa   mov	x0, x20
0x2471250f4:  e2 03 16 aa   mov	x2, x22
0x2471250f8:  d6 85 3b 94   bl	0x248006850
0x2471250fc:  f7 03 00 aa   mov	x23, x0
0x247125100:  14 63 3d 94   bl	_objc_release_x22_stub
0x247125104:  37 01 00 37   tbnz	w23, #0, loc_247125128 ; ⤵ 0x24
0x247125108:  ; loc_247125108
0x247125108:  e0 03 13 aa   mov	x0, x19
0x24712510c:  e5 05 3d 94   bl	0x2480668a0
0x247125110:  c0 01 00 34   cbz	w0, loc_247125148 ; ⤵ 0x38
0x247125114:  02 04 17 90   adrp	x2, 0x2751a5000
0x247125118:  42 80 1e 91   add	x2, x2, #0x7a0 ; _ptr.NSConstantDoubleNumber
0x24712511c:  e0 03 14 aa   mov	x0, x20
0x247125120:  cc 85 3b 94   bl	0x248006850
0x247125124:  20 01 00 36   tbz	w0, #0, loc_247125148 ; ⤵ 0x24
0x247125128:  ; loc_247125128
0x247125128:  15 00 80 d2   mov	x21, #0
0x24712512c:  4f 00 00 14   b	loc_247125268 ; ⤵ 0x13c
0x247125130:  ; loc_247125130
0x247125130:  e0 03 15 aa   mov	x0, x21
0x247125134:  e2 03 14 aa   mov	x2, x20
0x247125138:  e3 03 13 aa   mov	x3, x19
0x24712513c:  89 ff 3c 94   bl	0x248064f60
0x247125140:  f0 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125144:  48 00 00 14   b	loc_247125264 ; ⤵ 0x120
0x247125148:  ; loc_247125148
0x247125148:  e0 03 13 aa   mov	x0, x19
0x24712514c:  91 08 3d 94   bl	0x248067390
0x247125150:  ec 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125154:  f6 03 00 aa   mov	x22, x0
0x247125158:  32 08 3d 94   bl	0x248067220
0x24712515c:  f7 03 00 aa   mov	x23, x0
0x247125160:  e8 7c 10 b0   adrp	x8, 0x2680c2000
0x247125164:  00 59 47 f9   ldr	x0, [x8, #0xeb0] ; _ptr.NSDate
0x247125168:  02 6b 3d 94   bl	_objc_opt_class_stub
0x24712516c:  f8 03 00 aa   mov	x24, x0
0x247125170:  f8 62 3d 94   bl	_objc_release_x22_stub
0x247125174:  e0 03 13 aa   mov	x0, x19
0x247125178:  ff 02 18 eb   cmp	x23, x24
0x24712517c:  80 05 00 54   b.eq	loc_24712522c ; ⤵ 0xb0
0x247125180:  84 08 3d 94   bl	0x248067390
0x247125184:  df 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125188:  f6 03 00 aa   mov	x22, x0
0x24712518c:  25 08 3d 94   bl	0x248067220
0x247125190:  f7 03 00 aa   mov	x23, x0
0x247125194:  e8 7c 10 b0   adrp	x8, 0x2680c2000
0x247125198:  00 3d 47 f9   ldr	x0, [x8, #0xe78] ; _ptr.NSArray
0x24712519c:  f5 6a 3d 94   bl	_objc_opt_class_stub
0x2471251a0:  ff 02 00 eb   cmp	x23, x0
0x2471251a4:  40 05 00 54   b.eq	loc_24712524c ; ⤵ 0xa8
0x2471251a8:  e0 03 13 aa   mov	x0, x19
0x2471251ac:  79 08 3d 94   bl	0x248067390
0x2471251b0:  d4 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x2471251b4:  f7 03 00 aa   mov	x23, x0
0x2471251b8:  1a 08 3d 94   bl	0x248067220
0x2471251bc:  f8 03 00 aa   mov	x24, x0
0x2471251c0:  e8 7c 10 b0   adrp	x8, 0x2680c2000
0x2471251c4:  00 d5 47 f9   ldr	x0, [x8, #0xfa8] ; _ptr.NSSet
0x2471251c8:  ea 6a 3d 94   bl	_objc_opt_class_stub
0x2471251cc:  f9 03 00 aa   mov	x25, x0
0x2471251d0:  e4 62 3d 94   bl	_objc_release_x23_stub
0x2471251d4:  df 62 3d 94   bl	_objc_release_x22_stub
0x2471251d8:  1f 03 19 eb   cmp	x24, x25
0x2471251dc:  a0 03 00 54   b.eq	loc_247125250 ; ⤵ 0x74
0x2471251e0:  e0 03 13 aa   mov	x0, x19
0x2471251e4:  6b 08 3d 94   bl	0x248067390
0x2471251e8:  c6 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x2471251ec:  f6 03 00 aa   mov	x22, x0
0x2471251f0:  0c 08 3d 94   bl	0x248067220
0x2471251f4:  f7 03 00 aa   mov	x23, x0
0x2471251f8:  48 fb 10 90   adrp	x8, 0x26908d000
0x2471251fc:  00 25 47 f9   ldr	x0, [x8, #0xe48] ; _ptr.CPElectricVehicleWaypoint
0x247125200:  dc 6a 3d 94   bl	_objc_opt_class_stub
0x247125204:  f8 03 00 aa   mov	x24, x0
0x247125208:  d2 62 3d 94   bl	_objc_release_x22_stub
0x24712520c:  ff 02 18 eb   cmp	x23, x24
0x247125210:  01 03 00 54   b.ne	loc_247125270 ; ⤵ 0x60
0x247125214:  e0 03 15 aa   mov	x0, x21
0x247125218:  e2 03 14 aa   mov	x2, x20
0x24712521c:  e3 03 13 aa   mov	x3, x19
0x247125220:  48 ff 3c 94   bl	0x248064f40
0x247125224:  b7 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125228:  0f 00 00 14   b	loc_247125264 ; ⤵ 0x3c
0x24712522c:  ; loc_24712522c
0x24712522c:  75 ff 3c 94   bl	0x248065000
0x247125230:  f5 03 00 aa   mov	x21, x0
0x247125234:  e0 03 14 aa   mov	x0, x20
0x247125238:  7e 99 3b 94   bl	0x24800b830
0x24712523c:  e0 03 15 aa   mov	x0, x21
0x247125240:  d5 fe ff 97   bl	_NSNumberForCPAccNavTypeFromDouble
0x247125244:  af 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125248:  07 00 00 14   b	loc_247125264 ; ⤵ 0x1c
0x24712524c:  ; loc_24712524c
0x24712524c:  c1 62 3d 94   bl	_objc_release_x22_stub
0x247125250:  ; loc_247125250
0x247125250:  e0 03 15 aa   mov	x0, x21
0x247125254:  e2 03 14 aa   mov	x2, x20
0x247125258:  e3 03 13 aa   mov	x3, x19
0x24712525c:  3d ff 3c 94   bl	0x248064f50
0x247125260:  a8 62 3d 94   bl	_objc_claimAutoreleasedReturnValue_stub
0x247125264:  ; loc_247125264
0x247125264:  f5 03 00 aa   mov	x21, x0
0x247125268:  ; loc_247125268
0x247125268:  b2 62 3d 94   bl	_objc_release_x20_stub
0x24712526c:  f4 03 15 aa   mov	x20, x21
0x247125270:  ; loc_247125270
0x247125270:  ac 62 3d 94   bl	_objc_release_x19_stub
0x247125274:  e0 03 14 aa   mov	x0, x20
0x247125278:  fd 7b 44 a9   ldp	fp, lr, [sp, #0x40]
0x24712527c:  f4 4f 43 a9   ldp	x20, x19, [sp, #0x30]
0x247125280:  f6 57 42 a9   ldp	x22, x21, [sp, #0x20]
0x247125284:  f8 5f 41 a9   ldp	x24, x23, [sp, #0x10]
0x247125288:  fa 67 c5 a8   ldp	x26, x25, [sp], #0x50
0x24712528c:  ff 23 03 d5   autibsp
0x247125290:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x247125294:  50 00 f0 b6   tbz	x16, #0x3e, loc_24712529c ; ⤵ 0x8
0x247125298:  20 8e 38 d4   brk	#0xc471
0x24712529c:  ; loc_24712529c
0x24712529c:  7d 6a 3d 14   b	_objc_autoreleaseReturnValue_stub

