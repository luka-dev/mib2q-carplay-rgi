
-[APRKMediaPlayer _currentPlaybackInfoDictionary]
0x250eecb08:  7f 23 03 d5   pacibsp
0x250eecb0c:  ff c3 03 d1   sub	sp, sp, #0xf0
0x250eecb10:  e9 23 08 6d   stp	d9, d8, [sp, #0x80]
0x250eecb14:  fc 6f 09 a9   stp	x28, x27, [sp, #0x90]
0x250eecb18:  fa 67 0a a9   stp	x26, x25, [sp, #0xa0]
0x250eecb1c:  f8 5f 0b a9   stp	x24, x23, [sp, #0xb0]
0x250eecb20:  f6 57 0c a9   stp	x22, x21, [sp, #0xc0]
0x250eecb24:  f4 4f 0d a9   stp	x20, x19, [sp, #0xd0]
0x250eecb28:  fd 7b 0e a9   stp	fp, lr, [sp, #0xe0]
0x250eecb2c:  fd 83 03 91   add	fp, sp, #0xe0
0x250eecb30:  f6 03 00 aa   mov	x22, x0
0x250eecb34:  c8 95 0b 90   adrp	x8, 0x2681a4000
0x250eecb38:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.___stack_chk_guard
0x250eecb3c:  08 01 40 f9   ldr	x8, [x8]
0x250eecb40:  a8 83 19 f8   stur	x8, [fp, #-0x68]
0x250eecb44:  a8 8e 0b d0   adrp	x8, 0x2680c2000
0x250eecb48:  00 a9 47 f9   ldr	x0, [x8, #0xf50] ; _ptr.NSMutableDictionary
0x250eecb4c:  c5 50 c8 95   bl	_objc_alloc_init_stub
0x250eecb50:  f3 03 00 aa   mov	x19, x0
0x250eecb54:  c0 06 40 f9   ldr	x0, [x22, #0x8]
0x250eecb58:  1a c4 c4 95   bl	0x25801dbc0
0x250eecb5c:  51 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecb60:  f4 03 00 aa   mov	x20, x0
0x250eecb64:  a0 26 00 b4   cbz	x0, loc_250eed038 ; ⤵ 0x4d4
0x250eecb68:  e8 5f 12 90   adrp	x8, 0x275ae8000
0x250eecb6c:  08 a1 31 91   add	x8, x8, #0xc68 ; _ptr.__NSCFConstantString
0x250eecb70:  e8 37 00 f9   str	x8, [sp, #0x68]
0x250eecb74:  e0 03 14 aa   mov	x0, x20
0x250eecb78:  76 53 c4 95   bl	0x258001950
0x250eecb7c:  49 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecb80:  f7 03 00 aa   mov	x23, x0
0x250eecb84:  e0 3b 00 f9   str	x0, [sp, #0x70]
0x250eecb88:  a8 8e 0b d0   adrp	x8, 0x2680c2000
0x250eecb8c:  00 6d 47 f9   ldr	x0, [x8, #0xed8] ; _ptr.NSDictionary
0x250eecb90:  e2 c3 01 91   add	x2, sp, #0x70
0x250eecb94:  e3 a3 01 91   add	x3, sp, #0x68
0x250eecb98:  24 00 80 52   mov	w4, #0x1
0x250eecb9c:  a1 54 c4 95   bl	0x258001e20
0x250eecba0:  40 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecba4:  f5 03 00 aa   mov	x21, x0
0x250eecba8:  8a 4e c8 95   bl	_objc_release_x23_stub
0x250eecbac:  e3 5f 12 90   adrp	x3, 0x275ae8000
0x250eecbb0:  63 20 32 91   add	x3, x3, #0xc88 ; _ptr.__NSCFConstantString
0x250eecbb4:  e0 03 13 aa   mov	x0, x19
0x250eecbb8:  e2 03 15 aa   mov	x2, x21
0x250eecbbc:  9d 53 c4 95   bl	0x258001a30
0x250eecbc0:  e0 03 14 aa   mov	x0, x20
0x250eecbc4:  db 88 c4 95   bl	0x25800ef30
0x250eecbc8:  f7 03 00 aa   mov	x23, x0
0x250eecbcc:  1b 8f 0b f0   adrp	x27, 0x2680cf000
0x250eecbd0:  60 47 43 f9   ldr	x0, [x27, #0x688] ; _ptr.NSNumber
0x250eecbd4:  ff 06 00 f1   cmp	x23, #0x1
0x250eecbd8:  e2 17 9f 1a   cset	w2, eq
0x250eecbdc:  a5 51 c4 95   bl	0x258001270
0x250eecbe0:  30 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecbe4:  f8 03 00 aa   mov	x24, x0
0x250eecbe8:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecbec:  08 a1 47 f9   ldr	x8, [x8, #0xf40] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_ReadyToPlay
0x250eecbf0:  03 01 40 f9   ldr	x3, [x8]
0x250eecbf4:  e0 03 13 aa   mov	x0, x19
0x250eecbf8:  e2 03 18 aa   mov	x2, x24
0x250eecbfc:  8d 53 c4 95   bl	0x258001a30
0x250eecc00:  78 4e c8 95   bl	_objc_release_x24_stub
0x250eecc04:  ff 06 00 f1   cmp	x23, #0x1
0x250eecc08:  01 28 00 54   b.ne	loc_250eed108 ; ⤵ 0x500
0x250eecc0c:  e0 03 14 aa   mov	x0, x20
0x250eecc10:  ec 82 c4 95   bl	0x25800d7c0
0x250eecc14:  23 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecc18:  f7 03 00 aa   mov	x23, x0
0x250eecc1c:  a0 02 00 b4   cbz	x0, loc_250eecc70 ; ⤵ 0x54
0x250eecc20:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecc24:  08 79 47 f9   ldr	x8, [x8, #0xef0] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_Date
0x250eecc28:  03 01 40 f9   ldr	x3, [x8]
0x250eecc2c:  e0 03 13 aa   mov	x0, x19
0x250eecc30:  e2 03 17 aa   mov	x2, x23
0x250eecc34:  7f 53 c4 95   bl	0x258001a30
0x250eecc38:  78 47 43 f9   ldr	x24, [x27, #0x688]
0x250eecc3c:  e0 03 17 aa   mov	x0, x23
0x250eecc40:  e0 70 c4 95   bl	0x258008fc0
0x250eecc44:  e0 03 18 aa   mov	x0, x24
0x250eecc48:  12 63 c4 95   bl	0x258005890
0x250eecc4c:  15 50 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecc50:  f8 03 00 aa   mov	x24, x0
0x250eecc54:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecc58:  08 75 47 f9   ldr	x8, [x8, #0xee8] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_AbsoluteTime
0x250eecc5c:  03 01 40 f9   ldr	x3, [x8]
0x250eecc60:  e0 03 13 aa   mov	x0, x19
0x250eecc64:  e2 03 18 aa   mov	x2, x24
0x250eecc68:  72 53 c4 95   bl	0x258001a30
0x250eecc6c:  5d 4e c8 95   bl	_objc_release_x24_stub
0x250eecc70:  ; loc_250eecc70
0x250eecc70:  e8 03 01 91   add	x8, sp, #0x40
0x250eecc74:  e0 03 14 aa   mov	x0, x20
0x250eecc78:  76 54 c4 95   bl	0x258001e50
0x250eecc7c:  a8 8e 0b f0   adrp	x8, 0x2680c3000
0x250eecc80:  08 d1 40 f9   ldr	x8, [x8, #0x1a0] ; _ptr._kCFAllocatorDefault
0x250eecc84:  18 01 40 f9   ldr	x24, [x8]
0x250eecc88:  e0 03 01 91   add	x0, sp, #0x40
0x250eecc8c:  e1 03 18 aa   mov	x1, x24
0x250eecc90:  98 82 c8 95   bl	_CMTimeCopyAsDictionary_stub
0x250eecc94:  f9 03 00 aa   mov	x25, x0
0x250eecc98:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecc9c:  08 7d 47 f9   ldr	x8, [x8, #0xef8] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_Duration
0x250eecca0:  03 01 40 f9   ldr	x3, [x8]
0x250eecca4:  e0 03 13 aa   mov	x0, x19
0x250eecca8:  e2 03 19 aa   mov	x2, x25
0x250eeccac:  61 53 c4 95   bl	0x258001a30
0x250eeccb0:  50 4e c8 95   bl	_objc_release_x25_stub
0x250eeccb4:  c8 f2 42 39   ldrb	w8, [x22, #0xbc]
0x250eeccb8:  e8 01 00 37   tbnz	w8, #0, loc_250eeccf4 ; ⤵ 0x3c
0x250eeccbc:  e8 03 01 91   add	x8, sp, #0x40
0x250eeccc0:  e0 03 14 aa   mov	x0, x20
0x250eeccc4:  c7 c3 c4 95   bl	0x25801dbe0
0x250eeccc8:  e0 03 01 91   add	x0, sp, #0x40
0x250eecccc:  e1 03 18 aa   mov	x1, x24
0x250eeccd0:  88 82 c8 95   bl	_CMTimeCopyAsDictionary_stub
0x250eeccd4:  f8 03 00 aa   mov	x24, x0
0x250eeccd8:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eeccdc:  08 99 47 f9   ldr	x8, [x8, #0xf30] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_Position
0x250eecce0:  03 01 40 f9   ldr	x3, [x8]
0x250eecce4:  e0 03 13 aa   mov	x0, x19
0x250eecce8:  e2 03 18 aa   mov	x2, x24
0x250eeccec:  51 53 c4 95   bl	0x258001a30
0x250eeccf0:  3c 4e c8 95   bl	_objc_release_x24_stub
0x250eeccf4:  ; loc_250eeccf4
0x250eeccf4:  c0 06 40 f9   ldr	x0, [x22, #0x8]
0x250eeccf8:  4a cb c4 95   bl	0x25801fa20
0x250eeccfc:  00 e4 00 6f   movi	v0.2d, #0
0x250eecd00:  1f 08 00 f1   cmp	x0, #0x2
0x250eecd04:  61 00 00 54   b.ne	loc_250eecd10 ; ⤵ 0xc
0x250eecd08:  c0 06 40 f9   ldr	x0, [x22, #0x8]
0x250eecd0c:  f5 c3 c4 95   bl	0x25801dce0
0x250eecd10:  ; loc_250eecd10
0x250eecd10:  c8 f2 42 39   ldrb	w8, [x22, #0xbc]
0x250eecd14:  88 01 00 37   tbnz	w8, #0, loc_250eecd44 ; ⤵ 0x30
0x250eecd18:  60 47 43 f9   ldr	x0, [x27, #0x688]
0x250eecd1c:  a5 69 c4 95   bl	0x2580073b0
0x250eecd20:  e0 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecd24:  f8 03 00 aa   mov	x24, x0
0x250eecd28:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecd2c:  08 9d 47 f9   ldr	x8, [x8, #0xf38] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_Rate
0x250eecd30:  03 01 40 f9   ldr	x3, [x8]
0x250eecd34:  e0 03 13 aa   mov	x0, x19
0x250eecd38:  e2 03 18 aa   mov	x2, x24
0x250eecd3c:  3d 53 c4 95   bl	0x258001a30
0x250eecd40:  28 4e c8 95   bl	_objc_release_x24_stub
0x250eecd44:  ; loc_250eecd44
0x250eecd44:  c2 4a 40 f9   ldr	x2, [x22, #0x90]
0x250eecd48:  e0 03 16 aa   mov	x0, x22
0x250eecd4c:  ed cf c4 95   bl	0x258020d00
0x250eecd50:  c2 4a 40 f9   ldr	x2, [x22, #0x90]
0x250eecd54:  e0 03 16 aa   mov	x0, x22
0x250eecd58:  be cf c4 95   bl	0x258020c50
0x250eecd5c:  d1 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecd60:  f8 03 00 aa   mov	x24, x0
0x250eecd64:  c0 00 00 b4   cbz	x0, loc_250eecd7c ; ⤵ 0x18
0x250eecd68:  e3 5f 12 90   adrp	x3, 0x275ae8000
0x250eecd6c:  63 a0 32 91   add	x3, x3, #0xca8 ; _ptr.__NSCFConstantString
0x250eecd70:  e0 03 13 aa   mov	x0, x19
0x250eecd74:  e2 03 18 aa   mov	x2, x24
0x250eecd78:  2e 53 c4 95   bl	0x258001a30
0x250eecd7c:  ; loc_250eecd7c
0x250eecd7c:  c8 b2 42 39   ldrb	w8, [x22, #0xac]
0x250eecd80:  1f 05 00 71   cmp	w8, #0x1
0x250eecd84:  e1 00 00 54   b.ne	loc_250eecda0 ; ⤵ 0x1c
0x250eecd88:  a2 8e 0b f0   adrp	x2, 0x2680c3000
0x250eecd8c:  42 20 40 f9   ldr	x2, [x2, #0x40] ; _ptr.___kCFBooleanTrue
0x250eecd90:  e3 5f 12 90   adrp	x3, 0x275ae8000
0x250eecd94:  63 20 33 91   add	x3, x3, #0xcc8 ; _ptr.__NSCFConstantString
0x250eecd98:  e0 03 13 aa   mov	x0, x19
0x250eecd9c:  25 53 c4 95   bl	0x258001a30
0x250eecda0:  ; loc_250eecda0
0x250eecda0:  79 47 43 f9   ldr	x25, [x27, #0x688]
0x250eecda4:  e0 03 14 aa   mov	x0, x20
0x250eecda8:  da d1 c4 95   bl	0x258021510
0x250eecdac:  e2 03 00 aa   mov	x2, x0
0x250eecdb0:  e0 03 19 aa   mov	x0, x25
0x250eecdb4:  2f 51 c4 95   bl	0x258001270
0x250eecdb8:  ba 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecdbc:  f9 03 00 aa   mov	x25, x0
0x250eecdc0:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecdc4:  08 95 47 f9   ldr	x8, [x8, #0xf28] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_PlaybackLikelyToKeepUp
0x250eecdc8:  03 01 40 f9   ldr	x3, [x8]
0x250eecdcc:  e0 03 13 aa   mov	x0, x19
0x250eecdd0:  e2 03 19 aa   mov	x2, x25
0x250eecdd4:  17 53 c4 95   bl	0x258001a30
0x250eecdd8:  06 4e c8 95   bl	_objc_release_x25_stub
0x250eecddc:  e0 03 14 aa   mov	x0, x20
0x250eecde0:  10 d1 c4 95   bl	0x258021220
0x250eecde4:  af 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecde8:  f9 03 00 aa   mov	x25, x0
0x250eecdec:  e0 00 00 b4   cbz	x0, loc_250eece08 ; ⤵ 0x1c
0x250eecdf0:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecdf4:  08 81 47 f9   ldr	x8, [x8, #0xf00] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_EstimatedDate
0x250eecdf8:  03 01 40 f9   ldr	x3, [x8]
0x250eecdfc:  e0 03 13 aa   mov	x0, x19
0x250eece00:  e2 03 19 aa   mov	x2, x25
0x250eece04:  0b 53 c4 95   bl	0x258001a30
0x250eece08:  ; loc_250eece08
0x250eece08:  7a 47 43 f9   ldr	x26, [x27, #0x688]
0x250eece0c:  e0 03 14 aa   mov	x0, x20
0x250eece10:  bc d1 c4 95   bl	0x258021500
0x250eece14:  e2 03 00 aa   mov	x2, x0
0x250eece18:  e0 03 1a aa   mov	x0, x26
0x250eece1c:  15 51 c4 95   bl	0x258001270
0x250eece20:  a0 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eece24:  fa 03 00 aa   mov	x26, x0
0x250eece28:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eece2c:  08 91 47 f9   ldr	x8, [x8, #0xf20] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_PlaybackBufferFull
0x250eece30:  03 01 40 f9   ldr	x3, [x8]
0x250eece34:  e0 03 13 aa   mov	x0, x19
0x250eece38:  e2 03 1a aa   mov	x2, x26
0x250eece3c:  fd 52 c4 95   bl	0x258001a30
0x250eece40:  f0 4d c8 95   bl	_objc_release_x26_stub
0x250eece44:  7a 47 43 f9   ldr	x26, [x27, #0x688]
0x250eece48:  e0 03 14 aa   mov	x0, x20
0x250eece4c:  a9 d1 c4 95   bl	0x2580214f0
0x250eece50:  e2 03 00 aa   mov	x2, x0
0x250eece54:  e0 03 1a aa   mov	x0, x26
0x250eece58:  06 51 c4 95   bl	0x258001270
0x250eece5c:  91 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eece60:  fa 03 00 aa   mov	x26, x0
0x250eece64:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eece68:  08 8d 47 f9   ldr	x8, [x8, #0xf18] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_PlaybackBufferEmpty
0x250eece6c:  03 01 40 f9   ldr	x3, [x8]
0x250eece70:  e0 03 13 aa   mov	x0, x19
0x250eece74:  e2 03 1a aa   mov	x2, x26
0x250eece78:  ee 52 c4 95   bl	0x258001a30
0x250eece7c:  e1 4d c8 95   bl	_objc_release_x26_stub
0x250eece80:  e0 03 14 aa   mov	x0, x20
0x250eece84:  b7 d2 c4 95   bl	0x258021960
0x250eece88:  86 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eece8c:  fb 03 00 aa   mov	x27, x0
0x250eece90:  e0 03 16 aa   mov	x0, x22
0x250eece94:  e2 03 1b aa   mov	x2, x27
0x250eece98:  0e d0 c4 95   bl	0x258020ed0
0x250eece9c:  81 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecea0:  fa 03 00 aa   mov	x26, x0
0x250eecea4:  db 4d c8 95   bl	_objc_release_x27_stub
0x250eecea8:  fa 00 00 b4   cbz	x26, loc_250eecec4 ; ⤵ 0x1c
0x250eeceac:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eeceb0:  08 a5 47 f9   ldr	x8, [x8, #0xf48] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_SeekableTimeRanges
0x250eeceb4:  03 01 40 f9   ldr	x3, [x8]
0x250eeceb8:  e0 03 13 aa   mov	x0, x19
0x250eecebc:  e2 03 1a aa   mov	x2, x26
0x250eecec0:  dc 52 c4 95   bl	0x258001a30
0x250eecec4:  ; loc_250eecec4
0x250eecec4:  e0 03 14 aa   mov	x0, x20
0x250eecec8:  ae d1 c4 95   bl	0x258021580
0x250eececc:  75 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eeced0:  fc 03 00 aa   mov	x28, x0
0x250eeced4:  e0 03 16 aa   mov	x0, x22
0x250eeced8:  e2 03 1c aa   mov	x2, x28
0x250eecedc:  fd cf c4 95   bl	0x258020ed0
0x250eecee0:  70 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecee4:  fb 03 00 aa   mov	x27, x0
0x250eecee8:  ce 4d c8 95   bl	_objc_release_x28_stub
0x250eeceec:  fb 00 00 b4   cbz	x27, loc_250eecf08 ; ⤵ 0x1c
0x250eecef0:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecef4:  08 89 47 f9   ldr	x8, [x8, #0xf10] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_LoadedTimeRanges
0x250eecef8:  03 01 40 f9   ldr	x3, [x8]
0x250eecefc:  e0 03 13 aa   mov	x0, x19
0x250eecf00:  e2 03 1b aa   mov	x2, x27
0x250eecf04:  cb 52 c4 95   bl	0x258001a30
0x250eecf08:  ; loc_250eecf08
0x250eecf08:  08 8f 0b f0   adrp	x8, 0x2680cf000
0x250eecf0c:  00 45 43 f9   ldr	x0, [x8, #0x688] ; _ptr.NSNumber
0x250eecf10:  c2 16 40 f9   ldr	x2, [x22, #0x28]
0x250eecf14:  73 55 c4 95   bl	0x2580024e0
0x250eecf18:  62 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecf1c:  fc 03 00 aa   mov	x28, x0
0x250eecf20:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecf24:  08 a9 47 f9   ldr	x8, [x8, #0xf50] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_StallCount
0x250eecf28:  03 01 40 f9   ldr	x3, [x8]
0x250eecf2c:  e0 03 13 aa   mov	x0, x19
0x250eecf30:  e2 03 1c aa   mov	x2, x28
0x250eecf34:  bf 52 c4 95   bl	0x258001a30
0x250eecf38:  ba 4d c8 95   bl	_objc_release_x28_stub
0x250eecf3c:  c0 6a 40 f9   ldr	x0, [x22, #0xd0]
0x250eecf40:  60 01 00 b4   cbz	x0, loc_250eecf6c ; ⤵ 0x2c
0x250eecf44:  1b cf c4 95   bl	0x258020bb0
0x250eecf48:  56 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecf4c:  fc 03 00 aa   mov	x28, x0
0x250eecf50:  c8 8e 0b 90   adrp	x8, 0x2680c4000
0x250eecf54:  08 85 47 f9   ldr	x8, [x8, #0xf08] ; _ptr._kFigEndpointPlaybackSessionPlaybackInfoKey_InterstitialInfo
0x250eecf58:  03 01 40 f9   ldr	x3, [x8]
0x250eecf5c:  e0 03 13 aa   mov	x0, x19
0x250eecf60:  e2 03 1c aa   mov	x2, x28
0x250eecf64:  b3 52 c4 95   bl	0x258001a30
0x250eecf68:  ae 4d c8 95   bl	_objc_release_x28_stub
0x250eecf6c:  ; loc_250eecf6c
0x250eecf6c:  28 c3 0e f0   adrp	x8, 0x26e753000
0x250eecf70:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eecf74:  1f 79 00 71   cmp	w8, #0x1e
0x250eecf78:  ec 0b 00 54   b.gt	loc_250eed0f4 ; ⤵ 0x17c
0x250eecf7c:  1f 05 00 31   cmn	w8, #0x1
0x250eecf80:  e0 0a 00 54   b.eq	loc_250eed0dc ; ⤵ 0x15c
0x250eecf84:  ; loc_250eecf84
0x250eecf84:  c8 22 43 39   ldrb	w8, [x22, #0xc8]
0x250eecf88:  29 01 00 b0   adrp	x9, 0x250f11000
0x250eecf8c:  29 e9 37 91   add	x9, x9, #0xdfa ; "primary"
0x250eecf90:  2a 01 00 b0   adrp	x10, 0x250f11000
0x250eecf94:  4a b5 37 91   add	x10, x10, #0xded ; "interstitial"
0x250eecf98:  1f 01 00 71   cmp	w8, #0
0x250eecf9c:  48 11 89 9a   csel	x8, x10, x9, ne
0x250eecfa0:  e8 1f 00 f9   str	x8, [sp, #0x38]
0x250eecfa4:  e0 03 14 aa   mov	x0, x20
0x250eecfa8:  6a 52 c4 95   bl	0x258001950
0x250eecfac:  3d 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eecfb0:  fc 03 00 aa   mov	x28, x0
0x250eecfb4:  e8 03 01 91   add	x8, sp, #0x40
0x250eecfb8:  e0 03 14 aa   mov	x0, x20
0x250eecfbc:  a5 53 c4 95   bl	0x258001e50
0x250eecfc0:  e0 03 01 91   add	x0, sp, #0x40
0x250eecfc4:  cf 81 c8 95   bl	_CMTimeGetSeconds_stub
0x250eecfc8:  08 1c a0 4e   mov	v8.16b, v0.16b
0x250eecfcc:  e8 03 01 91   add	x8, sp, #0x40
0x250eecfd0:  e0 03 14 aa   mov	x0, x20
0x250eecfd4:  03 c3 c4 95   bl	0x25801dbe0
0x250eecfd8:  e0 03 01 91   add	x0, sp, #0x40
0x250eecfdc:  c9 81 c8 95   bl	_CMTimeGetSeconds_stub
0x250eecfe0:  c8 b2 42 39   ldrb	w8, [x22, #0xac]
0x250eecfe4:  29 01 00 b0   adrp	x9, 0x250f11000
0x250eecfe8:  29 1d 38 91   add	x9, x9, #0xe07 ; "false"
0x250eecfec:  2a 01 00 b0   adrp	x10, 0x250f11000
0x250eecff0:  4a 09 38 91   add	x10, x10, #0xe02 ; "true"
0x250eecff4:  1f 01 00 71   cmp	w8, #0
0x250eecff8:  48 11 89 9a   csel	x8, x10, x9, ne
0x250eecffc:  f8 23 02 a9   stp	x24, x8, [sp, #0x20]
0x250eed000:  e8 03 01 6d   stp	d8, d0, [sp, #0x10]
0x250eed004:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eed008:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eed00c:  21 01 00 90   adrp	x1, 0x250f11000
0x250eed010:  21 08 35 91   add	x1, x1, #0xd42 ; "-[APRKMediaPlayer _currentPlaybackInfoDictionary]"
0x250eed014:  e8 1f 40 f9   ldr	x8, [sp, #0x38]
0x250eed018:  e8 73 00 a9   stp	x8, x28, [sp]
0x250eed01c:  23 01 00 90   adrp	x3, 0x250f11000
0x250eed020:  63 d0 35 91   add	x3, x3, #0xd74 ; "GetPlaybackInfo for %s item %@, Duration: %f, Position: %f, PlaybackState: %@, isWaitingToSetRateFromSenderAfterSeek: %s"
0x250eed024:  c2 03 80 52   mov	w2, #0x1e
0x250eed028:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250eed02c:  b5 4f c8 95   bl	_LogPrintF_stub
0x250eed030:  7c 4d c8 95   bl	_objc_release_x28_stub
0x250eed034:  30 00 00 14   b	loc_250eed0f4 ; ⤵ 0xc0
0x250eed038:  ; loc_250eed038
0x250eed038:  c8 22 43 39   ldrb	w8, [x22, #0xc8]
0x250eed03c:  88 06 00 37   tbnz	w8, #0, loc_250eed10c ; ⤵ 0xd0
0x250eed040:  08 8f 0b d0   adrp	x8, 0x2680cf000
0x250eed044:  15 5d 42 f9   ldr	x21, [x8, #0x4b8] ; _ptr.NSError
0x250eed048:  08 8f 0b d0   adrp	x8, 0x2680cf000
0x250eed04c:  08 55 40 f9   ldr	x8, [x8, #0xa8] ; _ptr._NSOSStatusErrorDomain
0x250eed050:  16 01 40 f9   ldr	x22, [x8]
0x250eed054:  08 8f 0b b0   adrp	x8, 0x2680ce000
0x250eed058:  08 c1 47 f9   ldr	x8, [x8, #0xf80] ; _ptr._NSLocalizedDescriptionKey
0x250eed05c:  09 01 40 f9   ldr	x9, [x8]
0x250eed060:  c8 5f 12 f0   adrp	x8, 0x275ae8000
0x250eed064:  08 a1 33 91   add	x8, x8, #0xce8 ; _ptr.__NSCFConstantString
0x250eed068:  e9 a3 05 a9   stp	x9, x8, [sp, #0x58]
0x250eed06c:  a8 8e 0b b0   adrp	x8, 0x2680c2000
0x250eed070:  00 6d 47 f9   ldr	x0, [x8, #0xed8] ; _ptr.NSDictionary
0x250eed074:  e2 83 01 91   add	x2, sp, #0x60
0x250eed078:  e3 63 01 91   add	x3, sp, #0x58
0x250eed07c:  24 00 80 52   mov	w4, #0x1
0x250eed080:  68 53 c4 95   bl	0x258001e20
0x250eed084:  07 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed088:  f7 03 00 aa   mov	x23, x0
0x250eed08c:  e0 03 15 aa   mov	x0, x21
0x250eed090:  e2 03 16 aa   mov	x2, x22
0x250eed094:  43 46 83 92   mov	x3, #0xffffffffffffe5cd
0x250eed098:  e4 03 17 aa   mov	x4, x23
0x250eed09c:  65 5f c4 95   bl	0x258004e30
0x250eed0a0:  00 4f c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed0a4:  f5 03 00 aa   mov	x21, x0
0x250eed0a8:  4a 4d c8 95   bl	_objc_release_x23_stub
0x250eed0ac:  08 25 0c 90   adrp	x8, 0x26938d000
0x250eed0b0:  00 61 45 f9   ldr	x0, [x8, #0xac0] ; "\xf0fO\xf1"
0x250eed0b4:  e2 03 15 aa   mov	x2, x21
0x250eed0b8:  7e d0 c4 95   bl	0x2580212b0
0x250eed0bc:  f9 4e c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed0c0:  f7 03 00 aa   mov	x23, x0
0x250eed0c4:  c3 5f 12 f0   adrp	x3, 0x275ae8000
0x250eed0c8:  63 20 34 91   add	x3, x3, #0xd08 ; _ptr.__NSCFConstantString
0x250eed0cc:  e0 03 13 aa   mov	x0, x19
0x250eed0d0:  e2 03 17 aa   mov	x2, x23
0x250eed0d4:  57 52 c4 95   bl	0x258001a30
0x250eed0d8:  0b 00 00 14   b	loc_250eed104 ; ⤵ 0x2c
0x250eed0dc:  ; loc_250eed0dc
0x250eed0dc:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eed0e0:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eed0e4:  c1 03 80 52   mov	w1, #0x1e
0x250eed0e8:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eed0ec:  35 4f c8 95   bl	__LogCategory_Initialize_stub
0x250eed0f0:  a0 f4 ff 35   cbnz	w0, loc_250eecf84 ; ⤴ -0x16c
0x250eed0f4:  ; loc_250eed0f4
0x250eed0f4:  47 4d c8 95   bl	_objc_release_x27_stub
0x250eed0f8:  42 4d c8 95   bl	_objc_release_x26_stub
0x250eed0fc:  3d 4d c8 95   bl	_objc_release_x25_stub
0x250eed100:  38 4d c8 95   bl	_objc_release_x24_stub
0x250eed104:  ; loc_250eed104
0x250eed104:  33 4d c8 95   bl	_objc_release_x23_stub
0x250eed108:  ; loc_250eed108
0x250eed108:  2a 4d c8 95   bl	_objc_release_x21_stub
0x250eed10c:  ; loc_250eed10c
0x250eed10c:  25 4d c8 95   bl	_objc_release_x20_stub
0x250eed110:  a8 83 59 f8   ldur	x8, [fp, #-0x68]
0x250eed114:  a9 95 0b f0   adrp	x9, 0x2681a4000
0x250eed118:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.___stack_chk_guard
0x250eed11c:  29 01 40 f9   ldr	x9, [x9]
0x250eed120:  3f 01 08 eb   cmp	x9, x8
0x250eed124:  e1 01 00 54   b.ne	loc_250eed160 ; ⤵ 0x3c
0x250eed128:  e0 03 13 aa   mov	x0, x19
0x250eed12c:  fd 7b 4e a9   ldp	fp, lr, [sp, #0xe0]
0x250eed130:  f4 4f 4d a9   ldp	x20, x19, [sp, #0xd0]
0x250eed134:  f6 57 4c a9   ldp	x22, x21, [sp, #0xc0]
0x250eed138:  f8 5f 4b a9   ldp	x24, x23, [sp, #0xb0]
0x250eed13c:  fa 67 4a a9   ldp	x26, x25, [sp, #0xa0]
0x250eed140:  fc 6f 49 a9   ldp	x28, x27, [sp, #0x90]
0x250eed144:  e9 23 48 6d   ldp	d9, d8, [sp, #0x80]
0x250eed148:  ff c3 03 91   add	sp, sp, #0xf0
0x250eed14c:  ff 23 03 d5   autibsp
0x250eed150:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x250eed154:  50 00 f0 b6   tbz	x16, #0x3e, loc_250eed15c ; ⤵ 0x8
0x250eed158:  20 8e 38 d4   brk	#0xc471
0x250eed15c:  ; loc_250eed15c
0x250eed15c:  cd 4e c8 15   b	_objc_autoreleaseReturnValue_stub
0x250eed160:  ; loc_250eed160
0x250eed160:  d4 4c c8 95   bl	___stack_chk_fail_stub

