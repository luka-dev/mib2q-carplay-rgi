
_screenstream_copyNetworkInfo:
0x250f4e9c0:  7f 23 03 d5   pacibsp
0x250f4e9c4:  ff 83 02 d1   sub	sp, sp, #0xa0
0x250f4e9c8:  e9 23 06 6d   stp	d9, d8, [sp, #0x60]
0x250f4e9cc:  f6 57 07 a9   stp	x22, x21, [sp, #0x70]
0x250f4e9d0:  f4 4f 08 a9   stp	x20, x19, [sp, #0x80]
0x250f4e9d4:  fd 7b 09 a9   stp	fp, lr, [sp, #0x90]
0x250f4e9d8:  fd 43 02 91   add	fp, sp, #0x90
0x250f4e9dc:  f4 03 00 aa   mov	x20, x0
0x250f4e9e0:  ff ff 02 a9   stp	xzr, xzr, [sp, #0x28]
0x250f4e9e4:  ff 27 00 b9   str	wzr, [sp, #0x24]
0x250f4e9e8:  a8 8b 0b b0   adrp	x8, 0x2680c3000
0x250f4e9ec:  08 d1 40 f9   ldr	x8, [x8, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x250f4e9f0:  15 01 40 f9   ldr	x21, [x8]
0x250f4e9f4:  a2 8b 0b b0   adrp	x2, 0x2680c3000
0x250f4e9f8:  42 c8 41 f9   ldr	x2, [x2, #0x390] ; _ptr.1e8d91d98 (CoreFoundation/__AUTH_CONST.__const)
0x250f4e9fc:  a3 8b 0b b0   adrp	x3, 0x2680c3000
0x250f4ea00:  63 cc 41 f9   ldr	x3, [x3, #0x398] ; _ptr.1e8d91df8 (CoreFoundation/__AUTH_CONST.__const)
0x250f4ea04:  e0 03 15 aa   mov	x0, x21
0x250f4ea08:  01 00 80 d2   mov	x1, #0
0x250f4ea0c:  d9 ce c6 95   bl	j__CFDictionaryCreateMutable
0x250f4ea10:  f3 03 00 aa   mov	x19, x0
0x250f4ea14:  80 46 40 f9   ldr	x0, [x20, #0x88]
0x250f4ea18:  80 2e 00 b4   cbz	x0, loc_250f4efe8 ; ⤵ 0x5d0
0x250f4ea1c:  11 09 c7 95   bl	j__APTransportStreamGetCMBaseObject
0x250f4ea20:  f6 03 00 aa   mov	x22, x0
0x250f4ea24:  0f f4 c6 95   bl	j__CMBaseObjectGetVTable
0x250f4ea28:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x250f4ea2c:  f1 03 00 aa   mov	x17, x0
0x250f4ea30:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x250f4ea34:  30 1a c1 da   autda	x16, x17
0x250f4ea38:  09 0e 43 f8   ldr	x9, [x16, #0x30]!
0x250f4ea3c:  89 02 00 b4   cbz	x9, loc_250f4ea8c ; ⤵ 0x50
0x250f4ea40:  e8 03 10 aa   mov	x8, x16
0x250f4ea44:  a8 e4 f2 f2   movk	x8, #0x9725, lsl #0x30
0x250f4ea48:  f0 03 09 aa   mov	x16, x9
0x250f4ea4c:  f1 03 08 aa   mov	x17, x8
0x250f4ea50:  30 12 c1 da   autia	x16, x17
0x250f4ea54:  f1 03 10 aa   mov	x17, x16
0x250f4ea58:  f1 43 c1 da   xpaci	x17
0x250f4ea5c:  1f 02 11 eb   cmp	x16, x17
0x250f4ea60:  40 00 00 54   b.eq	loc_250f4ea68 ; ⤵ 0x8
0x250f4ea64:  00 8e 38 d4   brk	#0xc470
0x250f4ea68:  ; loc_250f4ea68
0x250f4ea68:  f0 23 c1 da   paciza	x16
0x250f4ea6c:  10 01 00 b4   cbz	x16, loc_250f4ea8c ; ⤵ 0x20
0x250f4ea70:  ea 8c 0b d0   adrp	x10, 0x2680ec000
0x250f4ea74:  4a b5 43 f9   ldr	x10, [x10, #0x768] ; _ptr.269323140 (APTransport/__DATA_CONST.__const)
0x250f4ea78:  41 01 40 f9   ldr	x1, [x10]
0x250f4ea7c:  e3 c3 00 91   add	x3, sp, #0x30
0x250f4ea80:  e0 03 16 aa   mov	x0, x22
0x250f4ea84:  e2 03 15 aa   mov	x2, x21
0x250f4ea88:  28 09 3f d7   blraa	x9, x8
0x250f4ea8c:  ; loc_250f4ea8c
0x250f4ea8c:  88 e2 40 fd   ldr	d8, [x20, #0x1c0]
0x250f4ea90:  08 21 60 1e   fcmp	d8, #0.00000000
0x250f4ea94:  6c 08 00 54   b.gt	loc_250f4eba0 ; ⤵ 0x10c
0x250f4ea98:  88 0e 46 39   ldrb	w8, [x20, #0x183]
0x250f4ea9c:  c8 03 00 34   cbz	w8, loc_250f4eb14 ; ⤵ 0x78
0x250f4eaa0:  88 16 46 39   ldrb	w8, [x20, #0x185]
0x250f4eaa4:  88 03 00 34   cbz	w8, loc_250f4eb14 ; ⤵ 0x70
0x250f4eaa8:  80 b2 40 fd   ldr	d0, [x20, #0x160]
0x250f4eaac:  e8 15 00 90   adrp	x8, 0x25120a000
0x250f4eab0:  01 91 42 fd   ldr	d1, [x8, #0x520]
0x250f4eab4:  00 20 61 1e   fcmp	d0, d1
0x250f4eab8:  e8 15 00 90   adrp	x8, 0x25120a000
0x250f4eabc:  00 95 42 fd   ldr	d0, [x8, #0x528]
0x250f4eac0:  01 30 67 1e   fmov	d1, #25.00000000
0x250f4eac4:  08 9c 61 1e   fcsel	d8, d0, d1, ls
0x250f4eac8:  08 aa 0f 90   adrp	x8, 0x27048e000
0x250f4eacc:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4ead0:  1f 51 00 71   cmp	w8, #0x14
0x250f4ead4:  0c 06 00 54   b.gt	loc_250f4eb94 ; ⤵ 0xc0
0x250f4ead8:  1f 05 00 31   cmn	w8, #0x1
0x250f4eadc:  00 05 00 54   b.eq	loc_250f4eb7c ; ⤵ 0xa0
0x250f4eae0:  ; loc_250f4eae0
0x250f4eae0:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f4eae4:  e8 07 00 fd   str	d8, [sp, #0x8]
0x250f4eae8:  e8 03 00 f9   str	x8, [sp]
0x250f4eaec:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4eaf0:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4eaf4:  a1 15 00 f0   adrp	x1, 0x251205000
0x250f4eaf8:  21 ac 2d 91   add	x1, x1, #0xb6b ; "CFDictionaryRef screenstream_copyNetworkInfo(StreamScreenStorage *)"
0x250f4eafc:  a3 15 00 f0   adrp	x3, 0x251205000
0x250f4eb00:  63 bc 2e 91   add	x3, x3, #0xbaf ; "%@ using %.2f Mbps for wired car connection\n"
0x250f4eb04:  82 02 80 52   mov	w2, #0x14
0x250f4eb08:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f4eb0c:  fd c8 c6 95   bl	j__LogPrintF
0x250f4eb10:  21 00 00 14   b	loc_250f4eb94 ; ⤵ 0x84
0x250f4eb14:  ; loc_250f4eb14
0x250f4eb14:  88 4a 46 39   ldrb	w8, [x20, #0x192]
0x250f4eb18:  c8 00 00 34   cbz	w8, loc_250f4eb30 ; ⤵ 0x18
0x250f4eb1c:  88 e6 45 39   ldrb	w8, [x20, #0x179]
0x250f4eb20:  88 00 00 34   cbz	w8, loc_250f4eb30 ; ⤵ 0x10
0x250f4eb24:  e8 15 00 90   adrp	x8, 0x25120a000
0x250f4eb28:  08 99 42 fd   ldr	d8, [x8, #0x530]
0x250f4eb2c:  1d 00 00 14   b	loc_250f4eba0 ; ⤵ 0x74
0x250f4eb30:  ; loc_250f4eb30
0x250f4eb30:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x250f4eb34:  e8 8c 0b d0   adrp	x8, 0x2680ec000
0x250f4eb38:  08 6d 43 f9   ldr	x8, [x8, #0x6d8] ; _ptr.269323150 (APTransport/__DATA_CONST.__const)
0x250f4eb3c:  01 01 40 f9   ldr	x1, [x8]
0x250f4eb40:  e2 a3 00 91   add	x2, sp, #0x28
0x250f4eb44:  af fb c6 95   bl	j__FigCFDictionaryGetDoubleIfPresent
0x250f4eb48:  c0 00 00 34   cbz	w0, loc_250f4eb60 ; ⤵ 0x18
0x250f4eb4c:  e0 17 40 fd   ldr	d0, [sp, #0x28]
0x250f4eb50:  80 de 00 fd   str	d0, [x20, #0x1b8]
0x250f4eb54:  81 d2 40 fd   ldr	d1, [x20, #0x1a0]
0x250f4eb58:  00 08 61 1e   fmul	d0, d0, d1
0x250f4eb5c:  02 00 00 14   b	loc_250f4eb64 ; ⤵ 0x8
0x250f4eb60:  ; loc_250f4eb60
0x250f4eb60:  00 e4 00 6f   movi	v0.2d, #0
0x250f4eb64:  ; loc_250f4eb64
0x250f4eb64:  81 8a 5a 6d   ldp	d1, d2, [x20, #0x1a8]
0x250f4eb68:  00 20 61 1e   fcmp	d0, d1
0x250f4eb6c:  00 cc 61 1e   fcsel	d0, d0, d1, gt
0x250f4eb70:  00 20 62 1e   fcmp	d0, d2
0x250f4eb74:  08 4c 62 1e   fcsel	d8, d0, d2, mi
0x250f4eb78:  0a 00 00 14   b	loc_250f4eba0 ; ⤵ 0x28
0x250f4eb7c:  ; loc_250f4eb7c
0x250f4eb7c:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4eb80:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4eb84:  81 02 80 52   mov	w1, #0x14
0x250f4eb88:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f4eb8c:  8d c8 c6 95   bl	j___LogCategory_Initialize
0x250f4eb90:  80 fa ff 35   cbnz	w0, loc_250f4eae0 ; ⤴ -0xb0
0x250f4eb94:  ; loc_250f4eb94
0x250f4eb94:  c8 15 00 f0   adrp	x8, 0x251209000
0x250f4eb98:  00 a9 47 fd   ldr	d0, [x8, #0xf50]
0x250f4eb9c:  08 09 60 1e   fmul	d8, d8, d0
0x250f4eba0:  ; loc_250f4eba0
0x250f4eba0:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4eba4:  08 cd 43 f9   ldr	x8, [x8, #0x798] ; _ptr.1e0c04988 (MediaToolbox/__DATA_CONST.__const)
0x250f4eba8:  01 01 40 f9   ldr	x1, [x8]
0x250f4ebac:  02 01 78 1e   fcvtzs	w2, d8
0x250f4ebb0:  e0 03 13 aa   mov	x0, x19
0x250f4ebb4:  c3 fb c6 95   bl	j__FigCFDictionarySetInt32
0x250f4ebb8:  88 e6 00 fd   str	d8, [x20, #0x1c8]
0x250f4ebbc:  88 d6 41 b9   ldr	w8, [x20, #0x1d4]
0x250f4ebc0:  1f 05 00 71   cmp	w8, #0x1
0x250f4ebc4:  cb 09 00 54   b.lt	loc_250f4ecfc ; ⤵ 0x138
0x250f4ebc8:  ; loc_250f4ebc8
0x250f4ebc8:  88 da 01 b9   str	w8, [x20, #0x1d8]
0x250f4ebcc:  88 de 01 b9   str	w8, [x20, #0x1dc]
0x250f4ebd0:  ; loc_250f4ebd0
0x250f4ebd0:  80 fa 40 f9   ldr	x0, [x20, #0x1f0]
0x250f4ebd4:  e8 e3 00 91   add	x8, sp, #0x38
0x250f4ebd8:  b0 92 0b d0   adrp	x16, 0x2681a4000
0x250f4ebdc:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x250f4ebe0:  f1 03 08 aa   mov	x17, x8
0x250f4ebe4:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x250f4ebe8:  30 0a c1 da   pacda	x16, x17
0x250f4ebec:  f0 1f 00 f9   str	x16, [sp, #0x38]
0x250f4ebf0:  c9 15 00 f0   adrp	x9, 0x251209000
0x250f4ebf4:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x250f4ebf8:  e0 23 00 fd   str	d0, [sp, #0x40]
0x250f4ebfc:  08 41 00 91   add	x8, x8, #0x10
0x250f4ec00:  f0 ff ff f0   adrp	x16, 0x250f4d000
0x250f4ec04:  10 b2 3e 91   add	x16, x16, #0xfac ; ___screenstream_updateFPS_block_invoke
0x250f4ec08:  10 01 c1 da   pacia	x16, x8
0x250f4ec0c:  48 22 0c 90   adrp	x8, 0x269396000
0x250f4ec10:  08 c1 22 91   add	x8, x8, #0x8b0 ; ___block_descriptor_tmp.96
0x250f4ec14:  f0 a3 04 a9   stp	x16, x8, [sp, #0x48]
0x250f4ec18:  f4 2f 00 f9   str	x20, [sp, #0x58]
0x250f4ec1c:  e1 e3 00 91   add	x1, sp, #0x38
0x250f4ec20:  0c cc c6 95   bl	j__dispatch_sync
0x250f4ec24:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4ec28:  08 e5 43 f9   ldr	x8, [x8, #0x7c8] ; _ptr.1e0c049b8 (MediaToolbox/__DATA_CONST.__const)
0x250f4ec2c:  01 01 40 f9   ldr	x1, [x8]
0x250f4ec30:  82 da 41 b9   ldr	w2, [x20, #0x1d8]
0x250f4ec34:  e0 03 13 aa   mov	x0, x19
0x250f4ec38:  a2 fb c6 95   bl	j__FigCFDictionarySetInt32
0x250f4ec3c:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x250f4ec40:  e8 8c 0b d0   adrp	x8, 0x2680ec000
0x250f4ec44:  08 69 43 f9   ldr	x8, [x8, #0x6d0] ; _ptr.269323160 (APTransport/__DATA_CONST.__const)
0x250f4ec48:  01 01 40 f9   ldr	x1, [x8]
0x250f4ec4c:  e2 93 00 91   add	x2, sp, #0x24
0x250f4ec50:  70 fb c6 95   bl	j__FigCFDictionaryGetInt32IfPresent
0x250f4ec54:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4ec58:  08 e1 43 f9   ldr	x8, [x8, #0x7c0] ; _ptr.1e0c049c0 (MediaToolbox/__DATA_CONST.__const)
0x250f4ec5c:  01 01 40 f9   ldr	x1, [x8]
0x250f4ec60:  e2 27 40 b9   ldr	w2, [sp, #0x24]
0x250f4ec64:  e0 03 13 aa   mov	x0, x19
0x250f4ec68:  96 fb c6 95   bl	j__FigCFDictionarySetInt32
0x250f4ec6c:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4ec70:  08 d9 43 f9   ldr	x8, [x8, #0x7b0] ; _ptr.1e0c049a8 (MediaToolbox/__DATA_CONST.__const)
0x250f4ec74:  01 01 40 f9   ldr	x1, [x8]
0x250f4ec78:  e8 27 40 b9   ldr	w8, [sp, #0x24]
0x250f4ec7c:  08 7d 48 0b   add	w8, w8, w8, lsr #0x1f
0x250f4ec80:  02 7d 01 13   asr	w2, w8, #0x1
0x250f4ec84:  e0 03 13 aa   mov	x0, x19
0x250f4ec88:  8e fb c6 95   bl	j__FigCFDictionarySetInt32
0x250f4ec8c:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4ec90:  08 d1 43 f9   ldr	x8, [x8, #0x7a0] ; _ptr.1e0c04998 (MediaToolbox/__DATA_CONST.__const)
0x250f4ec94:  01 01 40 f9   ldr	x1, [x8]
0x250f4ec98:  e0 03 13 aa   mov	x0, x19
0x250f4ec9c:  02 00 80 52   mov	w2, #0
0x250f4eca0:  88 fb c6 95   bl	j__FigCFDictionarySetInt32
0x250f4eca4:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4eca8:  08 d5 43 f9   ldr	x8, [x8, #0x7a8] ; _ptr.1e0c049a0 (MediaToolbox/__DATA_CONST.__const)
0x250f4ecac:  01 01 40 f9   ldr	x1, [x8]
0x250f4ecb0:  e0 03 13 aa   mov	x0, x19
0x250f4ecb4:  02 00 80 52   mov	w2, #0
0x250f4ecb8:  82 fb c6 95   bl	j__FigCFDictionarySetInt32
0x250f4ecbc:  68 8c 0b b0   adrp	x8, 0x2680db000
0x250f4ecc0:  08 dd 43 f9   ldr	x8, [x8, #0x7b8] ; _ptr.1e0c049b0 (MediaToolbox/__DATA_CONST.__const)
0x250f4ecc4:  01 01 40 f9   ldr	x1, [x8]
0x250f4ecc8:  00 e4 00 6f   movi	v0.2d, #0
0x250f4eccc:  e0 03 13 aa   mov	x0, x19
0x250f4ecd0:  74 fb c6 95   bl	j__FigCFDictionarySetDouble
0x250f4ecd4:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x250f4ecd8:  40 00 00 b4   cbz	x0, loc_250f4ece0 ; ⤵ 0x8
0x250f4ecdc:  a5 cb c6 95   bl	j__CFRelease
0x250f4ece0:  ; loc_250f4ece0
0x250f4ece0:  e0 03 13 aa   mov	x0, x19
0x250f4ece4:  fd 7b 49 a9   ldp	fp, lr, [sp, #0x90]
0x250f4ece8:  f4 4f 48 a9   ldp	x20, x19, [sp, #0x80]
0x250f4ecec:  f6 57 47 a9   ldp	x22, x21, [sp, #0x70]
0x250f4ecf0:  e9 23 46 6d   ldp	d9, d8, [sp, #0x60]
0x250f4ecf4:  ff 83 02 91   add	sp, sp, #0xa0
0x250f4ecf8:  ff 0f 5f d6   retab
0x250f4ecfc:  ; loc_250f4ecfc
0x250f4ecfc:  88 0e 46 39   ldrb	w8, [x20, #0x183]
0x250f4ed00:  a8 00 00 34   cbz	w8, loc_250f4ed14 ; ⤵ 0x14
0x250f4ed04:  88 16 46 39   ldrb	w8, [x20, #0x185]
0x250f4ed08:  68 00 00 34   cbz	w8, loc_250f4ed14 ; ⤵ 0xc
0x250f4ed0c:  88 d2 41 b9   ldr	w8, [x20, #0x1d0]
0x250f4ed10:  ae ff ff 17   b	loc_250f4ebc8 ; ⤴ -0x148
0x250f4ed14:  ; loc_250f4ed14
0x250f4ed14:  88 da 41 b9   ldr	w8, [x20, #0x1d8]
0x250f4ed18:  89 d2 41 b9   ldr	w9, [x20, #0x1d0]
0x250f4ed1c:  1f 01 09 6b   cmp	w8, w9
0x250f4ed20:  6d 00 00 54   b.le	loc_250f4ed2c ; ⤵ 0xc
0x250f4ed24:  89 da 01 b9   str	w9, [x20, #0x1d8]
0x250f4ed28:  e8 03 09 aa   mov	x8, x9
0x250f4ed2c:  ; loc_250f4ed2c
0x250f4ed2c:  ea 15 00 90   adrp	x10, 0x25120a000
0x250f4ed30:  40 9d 42 fd   ldr	d0, [x10, #0x538]
0x250f4ed34:  00 21 60 1e   fcmp	d8, d0
0x250f4ed38:  65 00 00 54   b.pl	loc_250f4ed44 ; ⤵ 0xc
0x250f4ed3c:  4a 01 80 52   mov	w10, #0xa
0x250f4ed40:  0d 00 00 14   b	loc_250f4ed74 ; ⤵ 0x34
0x250f4ed44:  ; loc_250f4ed44
0x250f4ed44:  ca 15 00 f0   adrp	x10, 0x251209000
0x250f4ed48:  40 a9 47 fd   ldr	d0, [x10, #0xf50]
0x250f4ed4c:  00 21 60 1e   fcmp	d8, d0
0x250f4ed50:  65 00 00 54   b.pl	loc_250f4ed5c ; ⤵ 0xc
0x250f4ed54:  8a 02 80 52   mov	w10, #0x14
0x250f4ed58:  07 00 00 14   b	loc_250f4ed74 ; ⤵ 0x1c
0x250f4ed5c:  ; loc_250f4ed5c
0x250f4ed5c:  ea 15 00 90   adrp	x10, 0x25120a000
0x250f4ed60:  40 a1 42 fd   ldr	d0, [x10, #0x540]
0x250f4ed64:  00 21 60 1e   fcmp	d8, d0
0x250f4ed68:  8a 07 80 52   mov	w10, #0x3c
0x250f4ed6c:  cb 03 80 52   mov	w11, #0x1e
0x250f4ed70:  6a 41 8a 1a   csel	w10, w11, w10, mi
0x250f4ed74:  ; loc_250f4ed74
0x250f4ed74:  5f 01 09 6b   cmp	w10, w9
0x250f4ed78:  4a b1 89 1a   csel	w10, w10, w9, lt
0x250f4ed7c:  5f 01 08 6b   cmp	w10, w8
0x250f4ed80:  ca 03 00 54   b.ge	loc_250f4edf8 ; ⤵ 0x78
0x250f4ed84:  8a de 41 b9   ldr	w10, [x20, #0x1dc]
0x250f4ed88:  5f 01 08 6b   cmp	w10, w8
0x250f4ed8c:  cb 0d 00 54   b.lt	loc_250f4ef44 ; ⤵ 0x1b8
0x250f4ed90:  1f 79 00 71   cmp	w8, #0x1e
0x250f4ed94:  8b 02 80 52   mov	w11, #0x14
0x250f4ed98:  cc 03 80 52   mov	w12, #0x1e
0x250f4ed9c:  8b c1 8b 1a   csel	w11, w12, w11, gt
0x250f4eda0:  1f 55 00 71   cmp	w8, #0x15
0x250f4eda4:  48 01 80 52   mov	w8, #0xa
0x250f4eda8:  08 b1 8b 1a   csel	w8, w8, w11, lt
0x250f4edac:  1f 01 09 6b   cmp	w8, w9
0x250f4edb0:  15 b1 89 1a   csel	w21, w8, w9, lt
0x250f4edb4:  08 aa 0f 90   adrp	x8, 0x27048e000
0x250f4edb8:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4edbc:  1f 51 00 71   cmp	w8, #0x14
0x250f4edc0:  ac 0a 00 54   b.gt	loc_250f4ef14 ; ⤵ 0x154
0x250f4edc4:  1f 05 00 31   cmn	w8, #0x1
0x250f4edc8:  80 08 00 54   b.eq	loc_250f4eed8 ; ⤵ 0x110
0x250f4edcc:  ; loc_250f4edcc
0x250f4edcc:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f4edd0:  e8 0f 00 fd   str	d8, [sp, #0x18]
0x250f4edd4:  ea d7 00 a9   stp	x10, x21, [sp, #0x8]
0x250f4edd8:  e8 03 00 f9   str	x8, [sp]
0x250f4eddc:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4ede0:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4ede4:  a1 15 00 f0   adrp	x1, 0x251205000
0x250f4ede8:  21 70 2f 91   add	x1, x1, #0xbdc ; "void screenstream_updateFPS(StreamScreenStorage *)"
0x250f4edec:  a3 15 00 f0   adrp	x3, 0x251205000
0x250f4edf0:  63 3c 30 91   add	x3, x3, #0xc0f ; "%@ lowering FPS target: %d -> %d (BW %.2f)\n"
0x250f4edf4:  1e 00 00 14   b	loc_250f4ee6c ; ⤵ 0x78
0x250f4edf8:  ; loc_250f4edf8
0x250f4edf8:  8a de 41 b9   ldr	w10, [x20, #0x1dc]
0x250f4edfc:  0d 04 00 54   b.le	loc_250f4ee7c ; ⤵ 0x80
0x250f4ee00:  5f 01 08 6b   cmp	w10, w8
0x250f4ee04:  0c 0a 00 54   b.gt	loc_250f4ef44 ; ⤵ 0x140
0x250f4ee08:  1f 51 00 71   cmp	w8, #0x14
0x250f4ee0c:  cb 03 80 52   mov	w11, #0x1e
0x250f4ee10:  8c 02 80 52   mov	w12, #0x14
0x250f4ee14:  8b b1 8b 1a   csel	w11, w12, w11, lt
0x250f4ee18:  1f 75 00 71   cmp	w8, #0x1d
0x250f4ee1c:  88 07 80 52   mov	w8, #0x3c
0x250f4ee20:  08 c1 8b 1a   csel	w8, w8, w11, gt
0x250f4ee24:  1f 01 09 6b   cmp	w8, w9
0x250f4ee28:  15 b1 89 1a   csel	w21, w8, w9, lt
0x250f4ee2c:  08 aa 0f 90   adrp	x8, 0x27048e000
0x250f4ee30:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4ee34:  1f 51 00 71   cmp	w8, #0x14
0x250f4ee38:  ec 06 00 54   b.gt	loc_250f4ef14 ; ⤵ 0xdc
0x250f4ee3c:  1f 05 00 31   cmn	w8, #0x1
0x250f4ee40:  e0 05 00 54   b.eq	loc_250f4eefc ; ⤵ 0xbc
0x250f4ee44:  ; loc_250f4ee44
0x250f4ee44:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f4ee48:  e8 0f 00 fd   str	d8, [sp, #0x18]
0x250f4ee4c:  ea d7 00 a9   stp	x10, x21, [sp, #0x8]
0x250f4ee50:  e8 03 00 f9   str	x8, [sp]
0x250f4ee54:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4ee58:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4ee5c:  a1 15 00 f0   adrp	x1, 0x251205000
0x250f4ee60:  21 70 2f 91   add	x1, x1, #0xbdc ; "void screenstream_updateFPS(StreamScreenStorage *)"
0x250f4ee64:  a3 15 00 f0   adrp	x3, 0x251205000
0x250f4ee68:  63 ec 30 91   add	x3, x3, #0xc3b ; "%@ raising FPS target: %d -> %d (BW %.2f)\n"
0x250f4ee6c:  ; loc_250f4ee6c
0x250f4ee6c:  82 02 80 52   mov	w2, #0x14
0x250f4ee70:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f4ee74:  23 c8 c6 95   bl	j__LogPrintF
0x250f4ee78:  27 00 00 14   b	loc_250f4ef14 ; ⤵ 0x9c
0x250f4ee7c:  ; loc_250f4ee7c
0x250f4ee7c:  5f 01 08 6b   cmp	w10, w8
0x250f4ee80:  20 06 00 54   b.eq	loc_250f4ef44 ; ⤵ 0xc4
0x250f4ee84:  09 aa 0f 90   adrp	x9, 0x27048e000
0x250f4ee88:  29 31 4b b9   ldr	w9, [x9, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4ee8c:  3f 51 00 71   cmp	w9, #0x14
0x250f4ee90:  8c 05 00 54   b.gt	loc_250f4ef40 ; ⤵ 0xb0
0x250f4ee94:  3f 05 00 31   cmn	w9, #0x1
0x250f4ee98:  60 04 00 54   b.eq	loc_250f4ef24 ; ⤵ 0x8c
0x250f4ee9c:  ; loc_250f4ee9c
0x250f4ee9c:  89 16 40 f9   ldr	x9, [x20, #0x28]
0x250f4eea0:  e8 0f 00 fd   str	d8, [sp, #0x18]
0x250f4eea4:  ea a3 00 a9   stp	x10, x8, [sp, #0x8]
0x250f4eea8:  e9 03 00 f9   str	x9, [sp]
0x250f4eeac:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4eeb0:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4eeb4:  a1 15 00 f0   adrp	x1, 0x251205000
0x250f4eeb8:  21 70 2f 91   add	x1, x1, #0xbdc ; "void screenstream_updateFPS(StreamScreenStorage *)"
0x250f4eebc:  a3 15 00 f0   adrp	x3, 0x251205000
0x250f4eec0:  63 98 31 91   add	x3, x3, #0xc66 ; "%@ cancel FPS adjustment: %d -> %d (BW %.2f)\n"
0x250f4eec4:  82 02 80 52   mov	w2, #0x14
0x250f4eec8:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250f4eecc:  0d c8 c6 95   bl	j__LogPrintF
0x250f4eed0:  88 da 41 b9   ldr	w8, [x20, #0x1d8]
0x250f4eed4:  1b 00 00 14   b	loc_250f4ef40 ; ⤵ 0x6c
0x250f4eed8:  ; loc_250f4eed8
0x250f4eed8:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4eedc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4eee0:  81 02 80 52   mov	w1, #0x14
0x250f4eee4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f4eee8:  b6 c7 c6 95   bl	j___LogCategory_Initialize
0x250f4eeec:  40 01 00 34   cbz	w0, loc_250f4ef14 ; ⤵ 0x28
0x250f4eef0:  8a de 41 b9   ldr	w10, [x20, #0x1dc]
0x250f4eef4:  88 e6 40 fd   ldr	d8, [x20, #0x1c8]
0x250f4eef8:  b5 ff ff 17   b	loc_250f4edcc ; ⤴ -0x12c
0x250f4eefc:  ; loc_250f4eefc
0x250f4eefc:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4ef00:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4ef04:  81 02 80 52   mov	w1, #0x14
0x250f4ef08:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f4ef0c:  ad c7 c6 95   bl	j___LogCategory_Initialize
0x250f4ef10:  00 07 00 35   cbnz	w0, loc_250f4eff0 ; ⤵ 0xe0
0x250f4ef14:  ; loc_250f4ef14
0x250f4ef14:  95 de 01 b9   str	w21, [x20, #0x1dc]
0x250f4ef18:  b6 ec c6 95   bl	j__mach_absolute_time
0x250f4ef1c:  80 f2 00 f9   str	x0, [x20, #0x1e0]
0x250f4ef20:  09 00 00 14   b	loc_250f4ef44 ; ⤵ 0x24
0x250f4ef24:  ; loc_250f4ef24
0x250f4ef24:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4ef28:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4ef2c:  81 02 80 52   mov	w1, #0x14
0x250f4ef30:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250f4ef34:  a3 c7 c6 95   bl	j___LogCategory_Initialize
0x250f4ef38:  88 da 41 b9   ldr	w8, [x20, #0x1d8]
0x250f4ef3c:  00 06 00 35   cbnz	w0, loc_250f4effc ; ⤵ 0xc0
0x250f4ef40:  ; loc_250f4ef40
0x250f4ef40:  88 de 01 b9   str	w8, [x20, #0x1dc]
0x250f4ef44:  ; loc_250f4ef44
0x250f4ef44:  88 de 41 b9   ldr	w8, [x20, #0x1dc]
0x250f4ef48:  89 da 41 b9   ldr	w9, [x20, #0x1d8]
0x250f4ef4c:  1f 01 09 6b   cmp	w8, w9
0x250f4ef50:  00 e4 ff 54   b.eq	loc_250f4ebd0 ; ⤴ -0x380
0x250f4ef54:  a7 ec c6 95   bl	j__mach_absolute_time
0x250f4ef58:  88 26 5e a9   ldp	x8, x9, [x20, #0x1e0]
0x250f4ef5c:  08 00 08 cb   sub	x8, x0, x8
0x250f4ef60:  1f 01 09 eb   cmp	x8, x9
0x250f4ef64:  63 e3 ff 54   b.lo	loc_250f4ebd0 ; ⤴ -0x394
0x250f4ef68:  08 aa 0f 90   adrp	x8, 0x27048e000
0x250f4ef6c:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x250f4ef70:  1f a1 00 71   cmp	w8, #0x28
0x250f4ef74:  4c 03 00 54   b.gt	loc_250f4efdc ; ⤵ 0x68
0x250f4ef78:  95 02 80 52   mov	w21, #0x14
0x250f4ef7c:  15 40 a0 72   movk	w21, #0x200, lsl #0x10
0x250f4ef80:  1f 05 00 31   cmn	w8, #0x1
0x250f4ef84:  20 02 00 54   b.eq	loc_250f4efc8 ; ⤵ 0x44
0x250f4ef88:  ; loc_250f4ef88
0x250f4ef88:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x250f4ef8c:  80 e6 40 fd   ldr	d0, [x20, #0x1c8]
0x250f4ef90:  89 da 41 b9   ldr	w9, [x20, #0x1d8]
0x250f4ef94:  8a de 41 b9   ldr	w10, [x20, #0x1dc]
0x250f4ef98:  e0 0f 00 fd   str	d0, [sp, #0x18]
0x250f4ef9c:  e9 ab 00 a9   stp	x9, x10, [sp, #0x8]
0x250f4efa0:  e8 03 00 f9   str	x8, [sp]
0x250f4efa4:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4efa8:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4efac:  a1 15 00 f0   adrp	x1, 0x251205000
0x250f4efb0:  21 70 2f 91   add	x1, x1, #0xbdc ; "void screenstream_updateFPS(StreamScreenStorage *)"
0x250f4efb4:  a3 15 00 f0   adrp	x3, 0x251205000
0x250f4efb8:  63 50 32 91   add	x3, x3, #0xc94 ; "%@ changing encoder FPS: %d -> %d (BW %.2f)\n"
0x250f4efbc:  a2 52 00 11   add	w2, w21, #0x14
0x250f4efc0:  d0 c7 c6 95   bl	j__LogPrintF
0x250f4efc4:  06 00 00 14   b	loc_250f4efdc ; ⤵ 0x18
0x250f4efc8:  ; loc_250f4efc8
0x250f4efc8:  00 aa 0f 90   adrp	x0, 0x27048e000
0x250f4efcc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x250f4efd0:  a1 52 00 11   add	w1, w21, #0x14
0x250f4efd4:  7b c7 c6 95   bl	j___LogCategory_Initialize
0x250f4efd8:  80 fd ff 35   cbnz	w0, loc_250f4ef88 ; ⤴ -0x50
0x250f4efdc:  ; loc_250f4efdc
0x250f4efdc:  88 de 41 b9   ldr	w8, [x20, #0x1dc]
0x250f4efe0:  88 da 01 b9   str	w8, [x20, #0x1d8]
0x250f4efe4:  fb fe ff 17   b	loc_250f4ebd0 ; ⤴ -0x414
0x250f4efe8:  ; loc_250f4efe8
0x250f4efe8:  32 1c 00 94   bl	_screenstream_copyNetworkInfo.cold.1
0x250f4efec:  3d ff ff 17   b	loc_250f4ece0 ; ⤴ -0x30c
0x250f4eff0:  ; loc_250f4eff0
0x250f4eff0:  8a de 41 b9   ldr	w10, [x20, #0x1dc]
0x250f4eff4:  88 e6 40 fd   ldr	d8, [x20, #0x1c8]
0x250f4eff8:  93 ff ff 17   b	loc_250f4ee44 ; ⤴ -0x1b4
0x250f4effc:  ; loc_250f4effc
0x250f4effc:  8a de 41 b9   ldr	w10, [x20, #0x1dc]
0x250f4f000:  88 e6 40 fd   ldr	d8, [x20, #0x1c8]
0x250f4f004:  a6 ff ff 17   b	loc_250f4ee9c ; ⤴ -0x168

