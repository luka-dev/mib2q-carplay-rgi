
_screenstream_createVirtualDisplayActivationOptions:
0x251176a08:  7f 23 03 d5   pacibsp
0x251176a0c:  ff 83 03 d1   sub	sp, sp, #0xe0
0x251176a10:  fc 6f 08 a9   stp	x28, x27, [sp, #0x80]
0x251176a14:  fa 67 09 a9   stp	x26, x25, [sp, #0x90]
0x251176a18:  f8 5f 0a a9   stp	x24, x23, [sp, #0xa0]
0x251176a1c:  f6 57 0b a9   stp	x22, x21, [sp, #0xb0]
0x251176a20:  f4 4f 0c a9   stp	x20, x19, [sp, #0xc0]
0x251176a24:  fd 7b 0d a9   stp	fp, lr, [sp, #0xd0]
0x251176a28:  fd 43 03 91   add	fp, sp, #0xd0
0x251176a2c:  f4 03 00 aa   mov	x20, x0
0x251176a30:  e4 5a be 95   bl	j__CMBaseObjectGetDerivedStorage
0x251176a34:  f3 03 00 aa   mov	x19, x0
0x251176a38:  bf c3 1a b8   stur	wzr, [fp, #-0x54]
0x251176a3c:  e0 03 14 aa   mov	x0, x20
0x251176a40:  e0 5a be 95   bl	j__CMBaseObjectGetDerivedStorage
0x251176a44:  f4 03 00 aa   mov	x20, x0
0x251176a48:  18 54 40 f9   ldr	x24, [x0, #0xa8]
0x251176a4c:  ff 33 00 f9   str	xzr, [sp, #0x60]
0x251176a50:  2a 7b 0b b0   adrp	x10, 0x2680db000
0x251176a54:  4a 2d 44 f9   ldr	x10, [x10, #0x858] ; _ptr.1e0bfd4a8 (MediaToolbox/__DATA_CONST.__const)
0x251176a58:  35 7b 0b b0   adrp	x21, 0x2680db000
0x251176a5c:  b5 32 44 f9   ldr	x21, [x21, #0x860] ; _ptr.1e0bfd4a0 (MediaToolbox/__DATA_CONST.__const)
0x251176a60:  bf 03 1a f8   stur	xzr, [fp, #-0x60]
0x251176a64:  77 7a 0b b0   adrp	x23, 0x2680c3000
0x251176a68:  f7 d2 40 f9   ldr	x23, [x23, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x251176a6c:  da 98 0f 90   adrp	x26, 0x27048e000
0x251176a70:  39 7b 0b b0   adrp	x25, 0x2680db000
0x251176a74:  39 43 44 f9   ldr	x25, [x25, #0x880] ; _ptr.1e0bfd490 (MediaToolbox/__DATA_CONST.__const)
0x251176a78:  78 16 00 b5   cbnz	x24, loc_251176d44 ; ⤵ 0x2cc
0x251176a7c:  88 0e 46 39   ldrb	w8, [x20, #0x183]
0x251176a80:  68 01 00 34   cbz	w8, loc_251176aac ; ⤵ 0x2c
0x251176a84:  88 12 46 39   ldrb	w8, [x20, #0x184]
0x251176a88:  e8 13 00 34   cbz	w8, loc_251176d04 ; ⤵ 0x27c
0x251176a8c:  ed 6b be 95   bl	j__FVDUtilsHEVCEncoderSupports4K60
0x251176a90:  a0 13 00 34   cbz	w0, loc_251176d04 ; ⤵ 0x274
0x251176a94:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176a98:  08 39 44 f9   ldr	x8, [x8, #0x870] ; _ptr.1e0bfd480 (MediaToolbox/__DATA_CONST.__const)
0x251176a9c:  18 01 40 f9   ldr	x24, [x8]
0x251176aa0:  28 00 80 52   mov	w8, #0x1
0x251176aa4:  88 1e 0f 39   strb	w8, [x20, #0x3c7]
0x251176aa8:  98 00 00 14   b	loc_251176d08 ; ⤵ 0x260
0x251176aac:  ; loc_251176aac
0x251176aac:  88 4a 46 39   ldrb	w8, [x20, #0x192]
0x251176ab0:  08 01 00 34   cbz	w8, loc_251176ad0 ; ⤵ 0x20
0x251176ab4:  88 e6 45 39   ldrb	w8, [x20, #0x179]
0x251176ab8:  29 7b 0b b0   adrp	x9, 0x2680db000
0x251176abc:  29 3d 44 f9   ldr	x9, [x9, #0x878] ; _ptr.1e0bfd488 (MediaToolbox/__DATA_CONST.__const)
0x251176ac0:  1f 01 00 71   cmp	w8, #0
0x251176ac4:  28 01 8a 9a   csel	x8, x9, x10, eq
0x251176ac8:  18 01 40 f9   ldr	x24, [x8]
0x251176acc:  8f 00 00 14   b	loc_251176d08 ; ⤵ 0x23c
0x251176ad0:  ; loc_251176ad0
0x251176ad0:  88 de 45 39   ldrb	w8, [x20, #0x177]
0x251176ad4:  a8 01 00 34   cbz	w8, loc_251176b08 ; ⤵ 0x34
0x251176ad8:  88 e6 45 39   ldrb	w8, [x20, #0x179]
0x251176adc:  e8 00 00 34   cbz	w8, loc_251176af8 ; ⤵ 0x1c
0x251176ae0:  88 b6 40 f9   ldr	x8, [x20, #0x168]
0x251176ae4:  1f 05 00 f1   cmp	x8, #0x1
0x251176ae8:  48 01 95 9a   csel	x8, x10, x21, eq
0x251176aec:  18 01 40 f9   ldr	x24, [x8]
0x251176af0:  d8 00 00 b4   cbz	x24, loc_251176b08 ; ⤵ 0x18
0x251176af4:  85 00 00 14   b	loc_251176d08 ; ⤵ 0x214
0x251176af8:  ; loc_251176af8
0x251176af8:  88 ea 45 39   ldrb	w8, [x20, #0x17a]
0x251176afc:  68 00 00 34   cbz	w8, loc_251176b08 ; ⤵ 0xc
0x251176b00:  b8 02 40 f9   ldr	x24, [x21]
0x251176b04:  38 10 00 b5   cbnz	x24, loc_251176d08 ; ⤵ 0x204
0x251176b08:  ; loc_251176b08
0x251176b08:  88 ea 45 39   ldrb	w8, [x20, #0x17a]
0x251176b0c:  e8 00 00 34   cbz	w8, loc_251176b28 ; ⤵ 0x1c
0x251176b10:  ; loc_251176b10
0x251176b10:  88 a6 44 b9   ldr	w8, [x20, #0x4a4]
0x251176b14:  1f 01 00 71   cmp	w8, #0
0x251176b18:  0c 01 00 54   b.gt	loc_251176b38 ; ⤵ 0x20
0x251176b1c:  b8 02 40 f9   ldr	x24, [x21]
0x251176b20:  d8 00 00 b4   cbz	x24, loc_251176b38 ; ⤵ 0x18
0x251176b24:  79 00 00 14   b	loc_251176d08 ; ⤵ 0x1e4
0x251176b28:  ; loc_251176b28
0x251176b28:  88 42 45 39   ldrb	w8, [x20, #0x150]
0x251176b2c:  68 00 00 34   cbz	w8, loc_251176b38 ; ⤵ 0xc
0x251176b30:  1c 65 be 95   bl	j__APSHas4K60444SenderSupport
0x251176b34:  e0 fe ff 35   cbnz	w0, loc_251176b10 ; ⤴ -0x24
0x251176b38:  ; loc_251176b38
0x251176b38:  88 ce 45 39   ldrb	w8, [x20, #0x173]
0x251176b3c:  48 0e 00 34   cbz	w8, loc_251176d04 ; ⤵ 0x1c8
0x251176b40:  80 1a 40 f9   ldr	x0, [x20, #0x30]
0x251176b44:  00 0e 00 b4   cbz	x0, loc_251176d04 ; ⤵ 0x1c0
0x251176b48:  b1 85 f7 97   bl	_APSenderSessionGetCMBaseObject
0x251176b4c:  f6 03 00 aa   mov	x22, x0
0x251176b50:  c4 53 be 95   bl	j__CMBaseObjectGetVTable
0x251176b54:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x251176b58:  f1 03 00 aa   mov	x17, x0
0x251176b5c:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x251176b60:  30 1a c1 da   autda	x16, x17
0x251176b64:  09 0e 43 f8   ldr	x9, [x16, #0x30]!
0x251176b68:  49 0b 00 b4   cbz	x9, loc_251176cd0 ; ⤵ 0x168
0x251176b6c:  e8 03 10 aa   mov	x8, x16
0x251176b70:  a8 e4 f2 f2   movk	x8, #0x9725, lsl #0x30
0x251176b74:  f0 03 09 aa   mov	x16, x9
0x251176b78:  f1 03 08 aa   mov	x17, x8
0x251176b7c:  30 12 c1 da   autia	x16, x17
0x251176b80:  f1 03 10 aa   mov	x17, x16
0x251176b84:  f1 43 c1 da   xpaci	x17
0x251176b88:  1f 02 11 eb   cmp	x16, x17
0x251176b8c:  40 00 00 54   b.eq	loc_251176b94 ; ⤵ 0x8
0x251176b90:  00 8e 38 d4   brk	#0xc470
0x251176b94:  ; loc_251176b94
0x251176b94:  f0 23 c1 da   paciza	x16
0x251176b98:  d0 09 00 b4   cbz	x16, loc_251176cd0 ; ⤵ 0x138
0x251176b9c:  ea 10 0c 90   adrp	x10, 0x269392000
0x251176ba0:  4a 41 22 91   add	x10, x10, #0x890 ; _kAPSenderSessionProperty_TransportControlStream
0x251176ba4:  41 01 40 f9   ldr	x1, [x10]
0x251176ba8:  f5 02 40 f9   ldr	x21, [x23]
0x251176bac:  e3 83 01 91   add	x3, sp, #0x60
0x251176bb0:  e0 03 16 aa   mov	x0, x22
0x251176bb4:  e2 03 15 aa   mov	x2, x21
0x251176bb8:  28 09 3f d7   blraa	x9, x8
0x251176bbc:  a0 c3 1a b8   stur	w0, [fp, #-0x54]
0x251176bc0:  c0 08 00 35   cbnz	w0, loc_251176cd8 ; ⤵ 0x118
0x251176bc4:  e0 33 40 f9   ldr	x0, [sp, #0x60]
0x251176bc8:  a8 7b 0b d0   adrp	x8, 0x2680ec000
0x251176bcc:  08 b1 43 f9   ldr	x8, [x8, #0x760] ; _ptr.2693230d8 (APTransport/__DATA_CONST.__const)
0x251176bd0:  01 01 40 f9   ldr	x1, [x8]
0x251176bd4:  a2 53 01 d1   sub	x2, fp, #0x54
0x251176bd8:  6e 55 be 95   bl	j__APSGetFBOPropertyInt64
0x251176bdc:  a8 c3 5a b8   ldur	w8, [fp, #-0x54]
0x251176be0:  08 4f 00 35   cbnz	w8, loc_2511775c0 ; ⤵ 0x9e0
0x251176be4:  18 00 80 d2   mov	x24, #0
0x251176be8:  1f 40 00 71   cmp	w0, #0x10
0x251176bec:  e8 08 00 54   b.hi	loc_251176d08 ; ⤵ 0x11c
0x251176bf0:  28 00 80 52   mov	w8, #0x1
0x251176bf4:  08 21 c0 1a   lsl	w8, w8, w0
0x251176bf8:  49 20 80 52   mov	w9, #0x102
0x251176bfc:  29 00 a0 72   movk	w9, #0x1, lsl #0x10
0x251176c00:  1f 01 09 6a   tst	w8, w9
0x251176c04:  20 08 00 54   b.eq	loc_251176d08 ; ⤵ 0x104
0x251176c08:  e0 33 40 f9   ldr	x0, [sp, #0x60]
0x251176c0c:  95 68 be 95   bl	j__APTransportStreamGetCMBaseObject
0x251176c10:  f6 03 00 aa   mov	x22, x0
0x251176c14:  93 53 be 95   bl	j__CMBaseObjectGetVTable
0x251176c18:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x251176c1c:  f1 03 00 aa   mov	x17, x0
0x251176c20:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x251176c24:  30 1a c1 da   autda	x16, x17
0x251176c28:  09 0e 43 f8   ldr	x9, [x16, #0x30]!
0x251176c2c:  e9 05 00 b4   cbz	x9, loc_251176ce8 ; ⤵ 0xbc
0x251176c30:  e8 03 10 aa   mov	x8, x16
0x251176c34:  a8 e4 f2 f2   movk	x8, #0x9725, lsl #0x30
0x251176c38:  f0 03 09 aa   mov	x16, x9
0x251176c3c:  f1 03 08 aa   mov	x17, x8
0x251176c40:  30 12 c1 da   autia	x16, x17
0x251176c44:  f1 03 10 aa   mov	x17, x16
0x251176c48:  f1 43 c1 da   xpaci	x17
0x251176c4c:  1f 02 11 eb   cmp	x16, x17
0x251176c50:  40 00 00 54   b.eq	loc_251176c58 ; ⤵ 0x8
0x251176c54:  00 8e 38 d4   brk	#0xc470
0x251176c58:  ; loc_251176c58
0x251176c58:  f0 23 c1 da   paciza	x16
0x251176c5c:  70 04 00 b4   cbz	x16, loc_251176ce8 ; ⤵ 0x8c
0x251176c60:  aa 7b 0b d0   adrp	x10, 0x2680ec000
0x251176c64:  4a 91 43 f9   ldr	x10, [x10, #0x720] ; _ptr.269323078 (APTransport/__DATA_CONST.__const)
0x251176c68:  41 01 40 f9   ldr	x1, [x10]
0x251176c6c:  a3 83 01 d1   sub	x3, fp, #0x60
0x251176c70:  e0 03 16 aa   mov	x0, x22
0x251176c74:  e2 03 15 aa   mov	x2, x21
0x251176c78:  28 09 3f d7   blraa	x9, x8
0x251176c7c:  a0 c3 1a b8   stur	w0, [fp, #-0x54]
0x251176c80:  80 03 00 35   cbnz	w0, loc_251176cf0 ; ⤵ 0x70
0x251176c84:  a0 03 5a f8   ldur	x0, [fp, #-0x60]
0x251176c88:  7e 55 be 95   bl	j__APSIsDirectLinkInterface
0x251176c8c:  c0 03 00 34   cbz	w0, loc_251176d04 ; ⤵ 0x78
0x251176c90:  b6 59 f7 97   bl	_OUTLINED_FUNCTION_29
0x251176c94:  ac 01 00 54   b.gt	loc_251176cc8 ; ⤵ 0x34
0x251176c98:  1f 05 00 31   cmn	w8, #0x1
0x251176c9c:  61 00 00 54   b.ne	loc_251176ca8 ; ⤵ 0xc
0x251176ca0:  33 dd f7 97   bl	_OUTLINED_FUNCTION_4
0x251176ca4:  20 01 00 34   cbz	w0, loc_251176cc8 ; ⤵ 0x24
0x251176ca8:  ; loc_251176ca8
0x251176ca8:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x251176cac:  a9 03 5a f8   ldur	x9, [fp, #-0x60]
0x251176cb0:  be 46 f7 97   bl	_OUTLINED_FUNCTION_6
0x251176cb4:  81 04 00 90   adrp	x1, 0x251206000
0x251176cb8:  21 38 2d 91   add	x1, x1, #0xb4e ; "CFStringRef screenstream_getVirtualDisplayUsageMode(FigEndpointStreamRef)"
0x251176cbc:  83 04 00 90   adrp	x3, 0x251206000
0x251176cc0:  63 60 2e 91   add	x3, x3, #0xb98 ; "%@ direct wired link on %@\n"
0x251176cc4:  81 ff f6 97   bl	_OUTLINED_FUNCTION_9
0x251176cc8:  ; loc_251176cc8
0x251176cc8:  38 03 40 f9   ldr	x24, [x25]
0x251176ccc:  0f 00 00 14   b	loc_251176d08 ; ⤵ 0x3c
0x251176cd0:  ; loc_251176cd0
0x251176cd0:  a0 3d 86 12   mov	w0, #0xffffce12
0x251176cd4:  a0 c3 1a b8   stur	w0, [fp, #-0x54]
0x251176cd8:  ; loc_251176cd8
0x251176cd8:  81 04 00 90   adrp	x1, 0x251206000
0x251176cdc:  21 98 2c 91   add	x1, x1, #0xb26 ; "screenstream_getVirtualDisplayUsageMode"
0x251176ce0:  82 60 81 52   mov	w2, #0xb04
0x251176ce4:  06 00 00 14   b	loc_251176cfc ; ⤵ 0x18
0x251176ce8:  ; loc_251176ce8
0x251176ce8:  a0 3d 86 12   mov	w0, #0xffffce12
0x251176cec:  a0 c3 1a b8   stur	w0, [fp, #-0x54]
0x251176cf0:  ; loc_251176cf0
0x251176cf0:  81 04 00 90   adrp	x1, 0x251206000
0x251176cf4:  21 98 2c 91   add	x1, x1, #0xb26 ; "screenstream_getVirtualDisplayUsageMode"
0x251176cf8:  62 61 81 52   mov	w2, #0xb0b
0x251176cfc:  ; loc_251176cfc
0x251176cfc:  03 00 80 52   mov	w3, #0
0x251176d00:  98 55 be 95   bl	j__APSLogErrorAt
0x251176d04:  ; loc_251176d04
0x251176d04:  18 00 80 d2   mov	x24, #0
0x251176d08:  ; loc_251176d08
0x251176d08:  e0 33 40 f9   ldr	x0, [sp, #0x60]
0x251176d0c:  40 00 00 b4   cbz	x0, loc_251176d14 ; ⤵ 0x8
0x251176d10:  98 2b be 95   bl	j__CFRelease
0x251176d14:  ; loc_251176d14
0x251176d14:  a0 03 5a f8   ldur	x0, [fp, #-0x60]
0x251176d18:  40 00 00 b4   cbz	x0, loc_251176d20 ; ⤵ 0x8
0x251176d1c:  95 2b be 95   bl	j__CFRelease
0x251176d20:  ; loc_251176d20
0x251176d20:  38 01 00 b5   cbnz	x24, loc_251176d44 ; ⤵ 0x24
0x251176d24:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176d28:  08 35 44 f9   ldr	x8, [x8, #0x868] ; _ptr.1e0bfd478 (MediaToolbox/__DATA_CONST.__const)
0x251176d2c:  89 0e 46 39   ldrb	w9, [x20, #0x183]
0x251176d30:  2a 7b 0b b0   adrp	x10, 0x2680db000
0x251176d34:  4a 29 44 f9   ldr	x10, [x10, #0x850] ; _ptr.1e0bfd470 (MediaToolbox/__DATA_CONST.__const)
0x251176d38:  3f 01 00 71   cmp	w9, #0
0x251176d3c:  48 01 88 9a   csel	x8, x10, x8, eq
0x251176d40:  18 01 40 f9   ldr	x24, [x8]
0x251176d44:  ; loc_251176d44
0x251176d44:  89 59 f7 97   bl	_OUTLINED_FUNCTION_29
0x251176d48:  ac 01 00 54   b.gt	loc_251176d7c ; ⤵ 0x34
0x251176d4c:  1f 05 00 31   cmn	w8, #0x1
0x251176d50:  61 00 00 54   b.ne	loc_251176d5c ; ⤵ 0xc
0x251176d54:  06 dd f7 97   bl	_OUTLINED_FUNCTION_4
0x251176d58:  20 01 00 34   cbz	w0, loc_251176d7c ; ⤵ 0x24
0x251176d5c:  ; loc_251176d5c
0x251176d5c:  88 16 40 f9   ldr	x8, [x20, #0x28]
0x251176d60:  e8 63 00 a9   stp	x8, x24, [sp]
0x251176d64:  e4 03 f7 97   bl	_OUTLINED_FUNCTION_11
0x251176d68:  81 04 00 90   adrp	x1, 0x251206000
0x251176d6c:  21 38 2d 91   add	x1, x1, #0xb4e ; "CFStringRef screenstream_getVirtualDisplayUsageMode(FigEndpointStreamRef)"
0x251176d70:  83 04 00 90   adrp	x3, 0x251206000
0x251176d74:  63 d0 2e 91   add	x3, x3, #0xbb4 ; "%@ usage mode %@\n"
0x251176d78:  54 ff f6 97   bl	_OUTLINED_FUNCTION_9
0x251176d7c:  ; loc_251176d7c
0x251176d7c:  98 5e 00 f9   str	x24, [x20, #0xb8]
0x251176d80:  fa 02 40 f9   ldr	x26, [x23]
0x251176d84:  4c ff f6 97   bl	_OUTLINED_FUNCTION_5
0x251176d88:  90 b3 fd 97   bl	_OUTLINED_FUNCTION_31
0x251176d8c:  f9 2d be 95   bl	j__CFDictionaryCreateMutable
0x251176d90:  f4 03 00 aa   mov	x20, x0
0x251176d94:  68 b6 40 f9   ldr	x8, [x19, #0x168]
0x251176d98:  1f 05 00 f1   cmp	x8, #0x1
0x251176d9c:  88 93 80 52   mov	w8, #0x49c
0x251176da0:  09 94 80 52   mov	w9, #0x4a0
0x251176da4:  28 01 88 9a   csel	x8, x9, x8, eq
0x251176da8:  69 6a a8 b8   ldrsw	x9, [x19, x8]
0x251176dac:  68 9e 44 b9   ldr	w8, [x19, #0x49c]
0x251176db0:  e9 1b 00 f9   str	x9, [sp, #0x30]
0x251176db4:  3f 01 08 6b   cmp	w9, w8
0x251176db8:  08 71 82 52   mov	w8, #0x1388
0x251176dbc:  69 08 80 52   mov	w9, #0x43
0x251176dc0:  28 01 88 1a   csel	w8, w9, w8, eq
0x251176dc4:  e8 2f 00 b9   str	w8, [sp, #0x2c]
0x251176dc8:  68 7a 0b b0   adrp	x8, 0x2680c3000
0x251176dcc:  08 a9 43 f9   ldr	x8, [x8, #0x750] ; _ptr.1842e4d50 (CoreGraphics/__TEXT.__const)
0x251176dd0:  00 01 c0 3d   ldr	q0, [x8]
0x251176dd4:  e0 1b 80 3d   str	q0, [sp, #0x60]
0x251176dd8:  bf 03 1a b8   stur	wzr, [fp, #-0x60]
0x251176ddc:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176de0:  08 81 43 f9   ldr	x8, [x8, #0x700] ; _ptr.1e0bfd468 (MediaToolbox/__DATA_CONST.__const)
0x251176de4:  01 01 40 f9   ldr	x1, [x8]
0x251176de8:  e2 03 18 aa   mov	x2, x24
0x251176dec:  f5 2d be 95   bl	j__CFDictionarySetValue
0x251176df0:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176df4:  08 7d 43 f9   ldr	x8, [x8, #0x6f8] ; _ptr.1e0bfd448 (MediaToolbox/__DATA_CONST.__const)
0x251176df8:  01 01 40 f9   ldr	x1, [x8]
0x251176dfc:  68 6a 40 39   ldrb	w8, [x19, #0x1a]
0x251176e00:  69 7a 0b b0   adrp	x9, 0x2680c3000
0x251176e04:  29 ed 40 f9   ldr	x9, [x9, #0x1d8] ; _ptr.1e0549de8 (CoreFoundation/__DATA_CONST.__const)
0x251176e08:  3c 01 40 f9   ldr	x28, [x9]
0x251176e0c:  69 7a 0b b0   adrp	x9, 0x2680c3000
0x251176e10:  29 f5 40 f9   ldr	x9, [x9, #0x1e8] ; _ptr.1e0549de0 (CoreFoundation/__DATA_CONST.__const)
0x251176e14:  3b 01 40 f9   ldr	x27, [x9]
0x251176e18:  1f 01 00 71   cmp	w8, #0
0x251176e1c:  62 03 9c 9a   csel	x2, x27, x28, eq
0x251176e20:  e0 03 14 aa   mov	x0, x20
0x251176e24:  e7 2d be 95   bl	j__CFDictionarySetValue
0x251176e28:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176e2c:  08 41 43 f9   ldr	x8, [x8, #0x680] ; _ptr.1e0bfd550 (MediaToolbox/__DATA_CONST.__const)
0x251176e30:  01 01 40 f9   ldr	x1, [x8]
0x251176e34:  62 16 40 f9   ldr	x2, [x19, #0x28]
0x251176e38:  e0 03 14 aa   mov	x0, x20
0x251176e3c:  e1 2d be 95   bl	j__CFDictionarySetValue
0x251176e40:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176e44:  08 79 43 f9   ldr	x8, [x8, #0x6f0] ; _ptr.1e0bfd510 (MediaToolbox/__DATA_CONST.__const)
0x251176e48:  54 18 f7 97   bl	_OUTLINED_FUNCTION_40
0x251176e4c:  e2 03 1b aa   mov	x2, x27
0x251176e50:  dc 2d be 95   bl	j__CFDictionarySetValue
0x251176e54:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176e58:  08 35 43 f9   ldr	x8, [x8, #0x668] ; _ptr.1e0bfd590 (MediaToolbox/__DATA_CONST.__const)
0x251176e5c:  15 01 40 f9   ldr	x21, [x8]
0x251176e60:  02 4c 12 d0   adrp	x2, 0x275af8000
0x251176e64:  42 00 2a 91   add	x2, x2, #0xa80 ; "AirPlay"
0x251176e68:  92 b3 fd 97   bl	_OUTLINED_FUNCTION_58
0x251176e6c:  f5 54 be 95   bl	j__APSIsAPValeriaScreenSender
0x251176e70:  40 01 00 34   cbz	w0, loc_251176e98 ; ⤵ 0x28
0x251176e74:  21 03 40 f9   ldr	x1, [x25]
0x251176e78:  e0 03 18 aa   mov	x0, x24
0x251176e7c:  31 2b be 95   bl	j__CFEqual
0x251176e80:  c0 00 00 34   cbz	w0, loc_251176e98 ; ⤵ 0x18
0x251176e84:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176e88:  08 75 43 f9   ldr	x8, [x8, #0x6e8] ; _ptr.1e0c01578 (MediaToolbox/__DATA_CONST.__const)
0x251176e8c:  43 18 f7 97   bl	_OUTLINED_FUNCTION_40
0x251176e90:  e2 03 1b aa   mov	x2, x27
0x251176e94:  cb 2d be 95   bl	j__CFDictionarySetValue
0x251176e98:  ; loc_251176e98
0x251176e98:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176e9c:  08 ed 43 f9   ldr	x8, [x8, #0x7d8] ; _ptr.1e0c04a78 (MediaToolbox/__DATA_CONST.__const)
0x251176ea0:  19 01 40 f9   ldr	x25, [x8]
0x251176ea4:  62 d2 41 b9   ldr	w2, [x19, #0x1d0]
0x251176ea8:  e0 03 14 aa   mov	x0, x20
0x251176eac:  e1 03 19 aa   mov	x1, x25
0x251176eb0:  04 5b be 95   bl	j__FigCFDictionarySetInt32
0x251176eb4:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176eb8:  08 2d 43 f9   ldr	x8, [x8, #0x658] ; _ptr.1e0bfd4c0 (MediaToolbox/__DATA_CONST.__const)
0x251176ebc:  01 01 40 f9   ldr	x1, [x8]
0x251176ec0:  62 d2 41 b9   ldr	w2, [x19, #0x1d0]
0x251176ec4:  e0 03 14 aa   mov	x0, x20
0x251176ec8:  fe 5a be 95   bl	j__FigCFDictionarySetInt32
0x251176ecc:  68 0e 46 39   ldrb	w8, [x19, #0x183]
0x251176ed0:  36 7b 0b b0   adrp	x22, 0x2680db000
0x251176ed4:  d6 1e 43 f9   ldr	x22, [x22, #0x638] ; _ptr.1e0bfd520 (MediaToolbox/__DATA_CONST.__const)
0x251176ed8:  37 7b 0b b0   adrp	x23, 0x2680db000
0x251176edc:  f7 46 43 f9   ldr	x23, [x23, #0x688] ; _ptr.1e0bfd4f0 (MediaToolbox/__DATA_CONST.__const)
0x251176ee0:  88 06 00 34   cbz	w8, loc_251176fb0 ; ⤵ 0xd0
0x251176ee4:  e1 02 40 f9   ldr	x1, [x23]
0x251176ee8:  62 8e 41 b9   ldr	w2, [x19, #0x18c]
0x251176eec:  e0 03 14 aa   mov	x0, x20
0x251176ef0:  f4 5a be 95   bl	j__FigCFDictionarySetInt32
0x251176ef4:  68 16 46 39   ldrb	w8, [x19, #0x185]
0x251176ef8:  c8 00 00 34   cbz	w8, loc_251176f10 ; ⤵ 0x18
0x251176efc:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176f00:  08 49 43 f9   ldr	x8, [x8, #0x690] ; _ptr.1e0bfd4f8 (MediaToolbox/__DATA_CONST.__const)
0x251176f04:  25 18 f7 97   bl	_OUTLINED_FUNCTION_40
0x251176f08:  c2 03 80 52   mov	w2, #0x1e
0x251176f0c:  ed 5a be 95   bl	j__FigCFDictionarySetInt32
0x251176f10:  ; loc_251176f10
0x251176f10:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176f14:  08 71 43 f9   ldr	x8, [x8, #0x6e0] ; _ptr.1e0c01580 (MediaToolbox/__DATA_CONST.__const)
0x251176f18:  20 18 f7 97   bl	_OUTLINED_FUNCTION_40
0x251176f1c:  e2 03 1c aa   mov	x2, x28
0x251176f20:  a8 2d be 95   bl	j__CFDictionarySetValue
0x251176f24:  c1 02 40 f9   ldr	x1, [x22]
0x251176f28:  a2 4c 12 90   adrp	x2, 0x275b0a000
0x251176f2c:  42 00 12 91   add	x2, x2, #0x480 ; "CarPlayScreen"
0x251176f30:  e0 03 14 aa   mov	x0, x20
0x251176f34:  a3 2d be 95   bl	j__CFDictionarySetValue
0x251176f38:  60 76 40 fd   ldr	d0, [x19, #0xe8]
0x251176f3c:  08 00 78 1e   fcvtzs	w8, d0
0x251176f40:  1f 05 00 71   cmp	w8, #0x1
0x251176f44:  da 98 0f 90   adrp	x26, 0x27048e000
0x251176f48:  8b 1b 00 54   b.lt	loc_2511772b8 ; ⤵ 0x370
0x251176f4c:  61 7a 40 fd   ldr	d1, [x19, #0xf0]
0x251176f50:  29 00 78 1e   fcvtzs	w9, d1
0x251176f54:  3f 05 00 71   cmp	w9, #0x1
0x251176f58:  0b 1b 00 54   b.lt	loc_2511772b8 ; ⤵ 0x360
0x251176f5c:  4a 33 4b b9   ldr	w10, [x26, #0xb30]
0x251176f60:  38 7b 0b b0   adrp	x24, 0x2680db000
0x251176f64:  18 5f 43 f9   ldr	x24, [x24, #0x6b8] ; _ptr.1e0bfd5a0 (MediaToolbox/__DATA_CONST.__const)
0x251176f68:  37 7b 0b b0   adrp	x23, 0x2680db000
0x251176f6c:  f7 5a 43 f9   ldr	x23, [x23, #0x6b0] ; _ptr.1e0bfd5a8 (MediaToolbox/__DATA_CONST.__const)
0x251176f70:  5f c9 00 71   cmp	w10, #0x32
0x251176f74:  0c 19 00 54   b.gt	loc_251177294 ; ⤵ 0x320
0x251176f78:  5f 05 00 31   cmn	w10, #0x1
0x251176f7c:  60 18 00 54   b.eq	loc_251177288 ; ⤵ 0x30c
0x251176f80:  ; loc_251176f80
0x251176f80:  6a 16 40 f9   ldr	x10, [x19, #0x28]
0x251176f84:  0b 03 40 f9   ldr	x11, [x24]
0x251176f88:  ec 02 40 f9   ldr	x12, [x23]
0x251176f8c:  ec a7 01 a9   stp	x12, x9, [sp, #0x18]
0x251176f90:  eb a3 00 a9   stp	x11, x8, [sp, #0x8]
0x251176f94:  c9 18 f7 97   bl	_OUTLINED_FUNCTION_9
0x251176f98:  ea 03 00 f9   str	x10, [sp]
0x251176f9c:  83 04 00 90   adrp	x3, 0x251206000
0x251176fa0:  63 a8 28 91   add	x3, x3, #0xa2a ; "%@ Setting %@: %d, %@: %d\n"
0x251176fa4:  c9 fe f6 97   bl	_OUTLINED_FUNCTION_9
0x251176fa8:  60 76 40 fd   ldr	d0, [x19, #0xe8]
0x251176fac:  ba 00 00 14   b	loc_251177294 ; ⤵ 0x2e8
0x251176fb0:  ; loc_251176fb0
0x251176fb0:  68 4a 46 39   ldrb	w8, [x19, #0x192]
0x251176fb4:  c1 02 40 f9   ldr	x1, [x22]
0x251176fb8:  3c 7b 0b b0   adrp	x28, 0x2680db000
0x251176fbc:  9c 33 43 f9   ldr	x28, [x28, #0x660] ; _ptr.1e0bfd4c8 (MediaToolbox/__DATA_CONST.__const)
0x251176fc0:  36 7b 0b b0   adrp	x22, 0x2680db000
0x251176fc4:  d6 1a 43 f9   ldr	x22, [x22, #0x630] ; _ptr.1e0bfd548 (MediaToolbox/__DATA_CONST.__const)
0x251176fc8:  08 07 00 34   cbz	w8, loc_2511770a8 ; ⤵ 0xe0
0x251176fcc:  a2 4c 12 90   adrp	x2, 0x275b0a000
0x251176fd0:  42 80 12 91   add	x2, x2, #0x4a0 ; "StevenoteAP"
0x251176fd4:  e0 03 14 aa   mov	x0, x20
0x251176fd8:  7a 2d be 95   bl	j__CFDictionarySetValue
0x251176fdc:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176fe0:  08 65 43 f9   ldr	x8, [x8, #0x6c8] ; _ptr.1e0bfd4d0 (MediaToolbox/__DATA_CONST.__const)
0x251176fe4:  ed 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x251176fe8:  e2 03 1b aa   mov	x2, x27
0x251176fec:  75 2d be 95   bl	j__CFDictionarySetValue
0x251176ff0:  28 7b 0b b0   adrp	x8, 0x2680db000
0x251176ff4:  08 69 43 f9   ldr	x8, [x8, #0x6d0] ; _ptr.1e0bfd4d8 (MediaToolbox/__DATA_CONST.__const)
0x251176ff8:  e8 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x251176ffc:  e2 03 1b aa   mov	x2, x27
0x251177000:  70 2d be 95   bl	j__CFDictionarySetValue
0x251177004:  ac fe f6 97   bl	_OUTLINED_FUNCTION_5
0x251177008:  f0 b2 fd 97   bl	_OUTLINED_FUNCTION_31
0x25117700c:  59 2d be 95   bl	j__CFDictionaryCreateMutable
0x251177010:  f9 03 00 aa   mov	x25, x0
0x251177014:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177018:  08 05 43 f9   ldr	x8, [x8, #0x608] ; _ptr.1e0bfd5d8 (MediaToolbox/__DATA_CONST.__const)
0x25117701c:  01 01 40 f9   ldr	x1, [x8]
0x251177020:  82 4c 12 f0   adrp	x2, 0x275b0a000
0x251177024:  42 00 13 91   add	x2, x2, #0x4c0 ; "nero"
0x251177028:  66 2d be 95   bl	j__CFDictionarySetValue
0x25117702c:  62 a6 40 f9   ldr	x2, [x19, #0x148]
0x251177030:  02 01 00 b4   cbz	x2, loc_251177050 ; ⤵ 0x20
0x251177034:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177038:  08 61 43 f9   ldr	x8, [x8, #0x6c0] ; _ptr.1e0bfd5e0 (MediaToolbox/__DATA_CONST.__const)
0x25117703c:  d7 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x251177040:  60 2d be 95   bl	j__CFDictionarySetValue
0x251177044:  c1 02 40 f9   ldr	x1, [x22]
0x251177048:  f9 b2 fd 97   bl	_OUTLINED_FUNCTION_41
0x25117704c:  8d 5a be 95   bl	j__FigCFDictionarySetBoolean
0x251177050:  ; loc_251177050
0x251177050:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177054:  08 2d 44 f9   ldr	x8, [x8, #0x858] ; _ptr.1e0bfd4a8 (MediaToolbox/__DATA_CONST.__const)
0x251177058:  01 01 40 f9   ldr	x1, [x8]
0x25117705c:  e0 03 18 aa   mov	x0, x24
0x251177060:  b8 2a be 95   bl	j__CFEqual
0x251177064:  ba 98 0f f0   adrp	x26, 0x27048e000
0x251177068:  60 10 00 34   cbz	w0, loc_251177274 ; ⤵ 0x20c
0x25117706c:  00 b3 fd 97   bl	_OUTLINED_FUNCTION_49
0x251177070:  94 5a be 95   bl	j__FigCFDictionarySetInt32
0x251177074:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177078:  08 29 43 f9   ldr	x8, [x8, #0x650] ; _ptr.1e0bfd4b8 (MediaToolbox/__DATA_CONST.__const)
0x25117707c:  c7 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x251177080:  02 80 96 52   mov	w2, #0xb400
0x251177084:  82 98 a0 72   movk	w2, #0x4c4, lsl #0x10
0x251177088:  8e 5a be 95   bl	j__FigCFDictionarySetInt32
0x25117708c:  e2 b2 fd 97   bl	_OUTLINED_FUNCTION_39
0x251177090:  9c 5a be 95   bl	j__FigCFDictionarySetValue
0x251177094:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177098:  08 3d 43 f9   ldr	x8, [x8, #0x678] ; _ptr.1e0bfd5c0 (MediaToolbox/__DATA_CONST.__const)
0x25117709c:  bf 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x2511770a0:  42 00 80 52   mov	w2, #0x2
0x2511770a4:  77 00 00 14   b	loc_251177280 ; ⤵ 0x1dc
0x2511770a8:  ; loc_2511770a8
0x2511770a8:  82 4c 12 f0   adrp	x2, 0x275b0a000
0x2511770ac:  42 80 13 91   add	x2, x2, #0x4e0 ; "AirPlayScreen"
0x2511770b0:  e0 03 14 aa   mov	x0, x20
0x2511770b4:  43 2d be 95   bl	j__CFDictionarySetValue
0x2511770b8:  28 7b 0b 90   adrp	x8, 0x2680db000
0x2511770bc:  08 31 44 f9   ldr	x8, [x8, #0x860] ; _ptr.1e0bfd4a0 (MediaToolbox/__DATA_CONST.__const)
0x2511770c0:  01 01 40 f9   ldr	x1, [x8]
0x2511770c4:  e0 03 18 aa   mov	x0, x24
0x2511770c8:  9e 2a be 95   bl	j__CFEqual
0x2511770cc:  ba 98 0f f0   adrp	x26, 0x27048e000
0x2511770d0:  a0 03 00 34   cbz	w0, loc_251177144 ; ⤵ 0x74
0x2511770d4:  68 42 45 39   ldrb	w8, [x19, #0x150]
0x2511770d8:  68 03 00 34   cbz	w8, loc_251177144 ; ⤵ 0x6c
0x2511770dc:  b1 63 be 95   bl	j__APSHas4K60444SenderSupport
0x2511770e0:  20 03 00 34   cbz	w0, loc_251177144 ; ⤵ 0x64
0x2511770e4:  68 a6 44 b9   ldr	w8, [x19, #0x4a4]
0x2511770e8:  1f 01 00 71   cmp	w8, #0
0x2511770ec:  cc 02 00 54   b.gt	loc_251177144 ; ⤵ 0x58
0x2511770f0:  f8 03 16 aa   mov	x24, x22
0x2511770f4:  d6 8c 86 52   mov	w22, #0x3466
0x2511770f8:  96 86 a6 72   movk	w22, #0x3434, lsl #0x10
0x2511770fc:  9b 58 f7 97   bl	_OUTLINED_FUNCTION_29
0x251177100:  6c 01 00 54   b.gt	loc_25117712c ; ⤵ 0x2c
0x251177104:  1f 05 00 31   cmn	w8, #0x1
0x251177108:  61 00 00 54   b.ne	loc_251177114 ; ⤵ 0xc
0x25117710c:  18 dc f7 97   bl	_OUTLINED_FUNCTION_4
0x251177110:  e0 00 00 34   cbz	w0, loc_25117712c ; ⤵ 0x1c
0x251177114:  ; loc_251177114
0x251177114:  68 16 40 f9   ldr	x8, [x19, #0x28]
0x251177118:  e8 5b 00 a9   stp	x8, x22, [sp]
0x25117711c:  67 18 f7 97   bl	_OUTLINED_FUNCTION_9
0x251177120:  63 04 00 f0   adrp	x3, 0x251206000
0x251177124:  63 90 29 91   add	x3, x3, #0xa64 ; "%@ overwriting pixel format to '%C'\n"
0x251177128:  68 fe f6 97   bl	_OUTLINED_FUNCTION_9
0x25117712c:  ; loc_25117712c
0x25117712c:  81 03 40 f9   ldr	x1, [x28]
0x251177130:  e0 03 14 aa   mov	x0, x20
0x251177134:  c2 8c 86 52   mov	w2, #0x3466
0x251177138:  82 86 a6 72   movk	w2, #0x3434, lsl #0x10
0x25117713c:  61 5a be 95   bl	j__FigCFDictionarySetInt32
0x251177140:  f6 03 18 aa   mov	x22, x24
0x251177144:  ; loc_251177144
0x251177144:  68 b6 40 f9   ldr	x8, [x19, #0x168]
0x251177148:  1f 05 00 f1   cmp	x8, #0x1
0x25117714c:  21 07 00 54   b.ne	loc_251177230 ; ⤵ 0xe4
0x251177150:  60 96 40 f9   ldr	x0, [x19, #0x128]
0x251177154:  61 d2 41 b9   ldr	w1, [x19, #0x1d0]
0x251177158:  62 e6 45 39   ldrb	w2, [x19, #0x179]
0x25117715c:  63 a6 44 b9   ldr	w3, [x19, #0x4a4]
0x251177160:  60 86 4f 6d   ldp	d0, d1, [x19, #0xf8]
0x251177164:  e4 83 01 91   add	x4, sp, #0x60
0x251177168:  a5 83 01 d1   sub	x5, fp, #0x60
0x25117716c:  51 65 be 95   bl	j__APSScreenGetMediaPresentationParams
0x251177170:  7e 58 f7 97   bl	_OUTLINED_FUNCTION_29
0x251177174:  0c 02 00 54   b.gt	loc_2511771b4 ; ⤵ 0x40
0x251177178:  1f 05 00 31   cmn	w8, #0x1
0x25117717c:  61 00 00 54   b.ne	loc_251177188 ; ⤵ 0xc
0x251177180:  fb db f7 97   bl	_OUTLINED_FUNCTION_4
0x251177184:  80 01 00 34   cbz	w0, loc_2511771b4 ; ⤵ 0x30
0x251177188:  ; loc_251177188
0x251177188:  68 16 40 f9   ldr	x8, [x19, #0x28]
0x25117718c:  e0 07 46 6d   ldp	d0, d1, [sp, #0x60]
0x251177190:  a9 03 5a b8   ldur	w9, [fp, #-0x60]
0x251177194:  e9 0f 00 f9   str	x9, [sp, #0x18]
0x251177198:  e0 87 00 6d   stp	d0, d1, [sp, #0x8]
0x25117719c:  8d 73 f7 97   bl	_OUTLINED_FUNCTION_2
0x2511771a0:  61 04 00 f0   adrp	x1, 0x251206000
0x2511771a4:  21 44 27 91   add	x1, x1, #0x9d1 ; "CFDictionaryRef screenstream_createVirtualDisplayActivationOptions(FigEndpointStreamRef)"
0x2511771a8:  63 04 00 d0   adrp	x3, 0x251205000
0x2511771ac:  63 10 2b 91   add	x3, x3, #0xac4 ; "%@ display parameters for media presentation mode: size: %.1f x %.1f, fps: %d\n"
0x2511771b0:  46 fe f6 97   bl	_OUTLINED_FUNCTION_9
0x2511771b4:  ; loc_2511771b4
0x2511771b4:  68 e6 45 39   ldrb	w8, [x19, #0x179]
0x2511771b8:  68 00 00 34   cbz	w8, loc_2511771c4 ; ⤵ 0xc
0x2511771bc:  96 b2 fd 97   bl	_OUTLINED_FUNCTION_39
0x2511771c0:  00 2d be 95   bl	j__CFDictionarySetValue
0x2511771c4:  ; loc_2511771c4
0x2511771c4:  28 7b 0b 90   adrp	x8, 0x2680db000
0x2511771c8:  08 45 44 f9   ldr	x8, [x8, #0x888] ; _ptr.1e0bfd530 (MediaToolbox/__DATA_CONST.__const)
0x2511771cc:  73 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x2511771d0:  a2 00 80 52   mov	w2, #0x5
0x2511771d4:  3b 5a be 95   bl	j__FigCFDictionarySetInt32
0x2511771d8:  e1 02 40 f9   ldr	x1, [x23]
0x2511771dc:  e0 03 14 aa   mov	x0, x20
0x2511771e0:  82 25 80 52   mov	w2, #0x12c
0x2511771e4:  37 5a be 95   bl	j__FigCFDictionarySetInt32
0x2511771e8:  68 e2 45 39   ldrb	w8, [x19, #0x178]
0x2511771ec:  c8 00 00 34   cbz	w8, loc_251177204 ; ⤵ 0x18
0x2511771f0:  28 7b 0b 90   adrp	x8, 0x2680db000
0x2511771f4:  08 55 43 f9   ldr	x8, [x8, #0x6a8] ; _ptr.1e0bfd538 (MediaToolbox/__DATA_CONST.__const)
0x2511771f8:  68 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x2511771fc:  42 00 80 52   mov	w2, #0x2
0x251177200:  30 5a be 95   bl	j__FigCFDictionarySetInt32
0x251177204:  ; loc_251177204
0x251177204:  a2 03 5a b8   ldur	w2, [fp, #-0x60]
0x251177208:  e0 03 14 aa   mov	x0, x20
0x25117720c:  e1 03 19 aa   mov	x1, x25
0x251177210:  2c 5a be 95   bl	j__FigCFDictionarySetInt32
0x251177214:  62 7e 41 b9   ldr	w2, [x19, #0x17c]
0x251177218:  5f 04 00 71   cmp	w2, #0x1
0x25117721c:  ab 00 00 54   b.lt	loc_251177230 ; ⤵ 0x14
0x251177220:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177224:  08 6d 43 f9   ldr	x8, [x8, #0x6d8] ; _ptr.1e0bfd540 (MediaToolbox/__DATA_CONST.__const)
0x251177228:  5c 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x25117722c:  25 5a be 95   bl	j__FigCFDictionarySetInt32
0x251177230:  ; loc_251177230
0x251177230:  62 a6 40 f9   ldr	x2, [x19, #0x148]
0x251177234:  22 04 00 b4   cbz	x2, loc_2511772b8 ; ⤵ 0x84
0x251177238:  28 7b 0b 90   adrp	x8, 0x2680db000
0x25117723c:  08 61 43 f9   ldr	x8, [x8, #0x6c0] ; _ptr.1e0bfd5e0 (MediaToolbox/__DATA_CONST.__const)
0x251177240:  56 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x251177244:  df 2c be 95   bl	j__CFDictionarySetValue
0x251177248:  68 02 46 39   ldrb	w8, [x19, #0x180]
0x25117724c:  c8 00 00 34   cbz	w8, loc_251177264 ; ⤵ 0x18
0x251177250:  80 4c 12 f0   adrp	x0, 0x275b0a000
0x251177254:  00 00 14 91   add	x0, x0, #0x500 ; "enable_dynamic_protection_for_3p"
0x251177258:  01 00 80 52   mov	w1, #0
0x25117725c:  3d 55 be 95   bl	j__APSSettingsGetIntWithDefault
0x251177260:  c0 02 00 34   cbz	w0, loc_2511772b8 ; ⤵ 0x58
0x251177264:  ; loc_251177264
0x251177264:  c1 02 40 f9   ldr	x1, [x22]
0x251177268:  71 b2 fd 97   bl	_OUTLINED_FUNCTION_41
0x25117726c:  05 5a be 95   bl	j__FigCFDictionarySetBoolean
0x251177270:  12 00 00 14   b	loc_2511772b8 ; ⤵ 0x48
0x251177274:  ; loc_251177274
0x251177274:  ef 69 be 95   bl	j__FVDUtilsHEVCEncoderSupports44410
0x251177278:  20 02 00 34   cbz	w0, loc_2511772bc ; ⤵ 0x44
0x25117727c:  7c b2 fd 97   bl	_OUTLINED_FUNCTION_49
0x251177280:  ; loc_251177280
0x251177280:  10 5a be 95   bl	j__FigCFDictionarySetInt32
0x251177284:  0e 00 00 14   b	loc_2511772bc ; ⤵ 0x38
0x251177288:  ; loc_251177288
0x251177288:  b9 db f7 97   bl	_OUTLINED_FUNCTION_4
0x25117728c:  60 76 40 fd   ldr	d0, [x19, #0xe8]
0x251177290:  20 1a 00 35   cbnz	w0, loc_2511775d4 ; ⤵ 0x344
0x251177294:  ; loc_251177294
0x251177294:  01 03 40 f9   ldr	x1, [x24]
0x251177298:  02 00 78 1e   fcvtzs	w2, d0
0x25117729c:  e0 03 14 aa   mov	x0, x20
0x2511772a0:  08 5a be 95   bl	j__FigCFDictionarySetInt32
0x2511772a4:  e1 02 40 f9   ldr	x1, [x23]
0x2511772a8:  60 7a 40 fd   ldr	d0, [x19, #0xf0]
0x2511772ac:  02 00 78 1e   fcvtzs	w2, d0
0x2511772b0:  e0 03 14 aa   mov	x0, x20
0x2511772b4:  03 5a be 95   bl	j__FigCFDictionarySetInt32
0x2511772b8:  ; loc_2511772b8
0x2511772b8:  19 00 80 d2   mov	x25, #0
0x2511772bc:  ; loc_2511772bc
0x2511772bc:  62 4e 40 f9   ldr	x2, [x19, #0x98]
0x2511772c0:  a2 00 00 b4   cbz	x2, loc_2511772d4 ; ⤵ 0x14
0x2511772c4:  28 7b 0b 90   adrp	x8, 0x2680db000
0x2511772c8:  08 25 43 f9   ldr	x8, [x8, #0x648] ; _ptr.1e0c01568 (MediaToolbox/__DATA_CONST.__const)
0x2511772cc:  33 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x2511772d0:  bc 2c be 95   bl	j__CFDictionarySetValue
0x2511772d4:  ; loc_2511772d4
0x2511772d4:  62 62 40 f9   ldr	x2, [x19, #0xc0]
0x2511772d8:  38 7b 0b 90   adrp	x24, 0x2680db000
0x2511772dc:  18 53 43 f9   ldr	x24, [x24, #0x6a0] ; _ptr.1e0bfd578 (MediaToolbox/__DATA_CONST.__const)
0x2511772e0:  02 03 00 b4   cbz	x2, loc_251177340 ; ⤵ 0x60
0x2511772e4:  21 58 f7 97   bl	_OUTLINED_FUNCTION_29
0x2511772e8:  ac 01 00 54   b.gt	loc_25117731c ; ⤵ 0x34
0x2511772ec:  1f 05 00 31   cmn	w8, #0x1
0x2511772f0:  81 00 00 54   b.ne	loc_251177300 ; ⤵ 0x10
0x2511772f4:  9e db f7 97   bl	_OUTLINED_FUNCTION_4
0x2511772f8:  62 62 40 f9   ldr	x2, [x19, #0xc0]
0x2511772fc:  00 01 00 34   cbz	w0, loc_25117731c ; ⤵ 0x20
0x251177300:  ; loc_251177300
0x251177300:  68 16 40 f9   ldr	x8, [x19, #0x28]
0x251177304:  e8 0b 00 a9   stp	x8, x2, [sp]
0x251177308:  ec 17 f7 97   bl	_OUTLINED_FUNCTION_9
0x25117730c:  63 04 00 f0   adrp	x3, 0x251206000
0x251177310:  63 a8 2a 91   add	x3, x3, #0xaaa ; "%@ using client PID: %@\n"
0x251177314:  ed fd f6 97   bl	_OUTLINED_FUNCTION_9
0x251177318:  62 62 40 f9   ldr	x2, [x19, #0xc0]
0x25117731c:  ; loc_25117731c
0x25117731c:  28 7b 0b 90   adrp	x8, 0x2680db000
0x251177320:  08 21 43 f9   ldr	x8, [x8, #0x640] ; _ptr.1e0bfd518 (MediaToolbox/__DATA_CONST.__const)
0x251177324:  1d 17 f7 97   bl	_OUTLINED_FUNCTION_40
0x251177328:  a6 2c be 95   bl	j__CFDictionarySetValue
0x25117732c:  a0 18 f7 97   bl	_OUTLINED_FUNCTION_23
0x251177330:  38 6a be 95   bl	j__FigCFDictionarySetInt
0x251177334:  82 4c 12 f0   adrp	x2, 0x275b0a000
0x251177338:  42 80 14 91   add	x2, x2, #0x520 ; "PerAppAirPlayDisplay"
0x25117733c:  5d b2 fd 97   bl	_OUTLINED_FUNCTION_58
0x251177340:  ; loc_251177340
0x251177340:  68 0e 46 39   ldrb	w8, [x19, #0x183]
0x251177344:  37 7b 0b 90   adrp	x23, 0x2680db000
0x251177348:  f7 4e 43 f9   ldr	x23, [x23, #0x698] ; _ptr.1e0bfd580 (MediaToolbox/__DATA_CONST.__const)
0x25117734c:  48 01 00 34   cbz	w8, loc_251177374 ; ⤵ 0x28
0x251177350:  97 18 f7 97   bl	_OUTLINED_FUNCTION_23
0x251177354:  2f 6a be 95   bl	j__FigCFDictionarySetInt
0x251177358:  68 5a 40 b9   ldr	w8, [x19, #0x58]
0x25117735c:  89 4c 12 f0   adrp	x9, 0x275b0a000
0x251177360:  29 01 15 91   add	x9, x9, #0x540 ; "CarPlayAlternate"
0x251177364:  6a 4c 12 b0   adrp	x10, 0x275b04000
0x251177368:  4a 01 1e 91   add	x10, x10, #0x780 ; "CarPlay"
0x25117736c:  1f b9 01 71   cmp	w8, #0x6e
0x251177370:  0f 00 00 14   b	loc_2511773ac ; ⤵ 0x3c
0x251177374:  ; loc_251177374
0x251177374:  68 42 46 39   ldrb	w8, [x19, #0x190]
0x251177378:  e8 01 00 34   cbz	w8, loc_2511773b4 ; ⤵ 0x3c
0x25117737c:  01 03 40 f9   ldr	x1, [x24]
0x251177380:  2b b2 fd 97   bl	_OUTLINED_FUNCTION_41
0x251177384:  23 6a be 95   bl	j__FigCFDictionarySetInt
0x251177388:  e1 02 40 f9   ldr	x1, [x23]
0x25117738c:  28 b2 fd 97   bl	_OUTLINED_FUNCTION_41
0x251177390:  20 6a be 95   bl	j__FigCFDictionarySetInt
0x251177394:  68 4a 46 39   ldrb	w8, [x19, #0x192]
0x251177398:  89 4c 12 f0   adrp	x9, 0x275b0a000
0x25117739c:  29 81 15 91   add	x9, x9, #0x560 ; "Stevenote"
0x2511773a0:  8a 4c 12 f0   adrp	x10, 0x275b0a000
0x2511773a4:  4a 01 16 91   add	x10, x10, #0x580 ; "AirPlayDemo"
0x2511773a8:  1f 01 00 71   cmp	w8, #0
0x2511773ac:  ; loc_2511773ac
0x2511773ac:  42 01 89 9a   csel	x2, x10, x9, eq
0x2511773b0:  40 b2 fd 97   bl	_OUTLINED_FUNCTION_58
0x2511773b4:  ; loc_2511773b4
0x2511773b4:  c8 10 0c f0   adrp	x8, 0x269392000
0x2511773b8:  01 35 43 f9   ldr	x1, [x8, #0x668] ; _kAPVirtualDisplaySinkOption_MaxFrameQueueDurationMs
0x2511773bc:  e0 03 14 aa   mov	x0, x20
0x2511773c0:  e2 2f 40 b9   ldr	w2, [sp, #0x2c]
0x2511773c4:  bf 59 be 95   bl	j__FigCFDictionarySetInt32
0x2511773c8:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2511773cc:  91 5b be 95   bl	j__MillisecondsToUpTicks
0x2511773d0:  60 de 01 f9   str	x0, [x19, #0x3b8]
0x2511773d4:  19 01 00 b4   cbz	x25, loc_2511773f4 ; ⤵ 0x20
0x2511773d8:  28 7b 0b 90   adrp	x8, 0x2680db000
0x2511773dc:  08 39 43 f9   ldr	x8, [x8, #0x670] ; _ptr.1e0bfd5d0 (MediaToolbox/__DATA_CONST.__const)
0x2511773e0:  ee 16 f7 97   bl	_OUTLINED_FUNCTION_40
0x2511773e4:  e2 03 19 aa   mov	x2, x25
0x2511773e8:  76 2c be 95   bl	j__CFDictionarySetValue
0x2511773ec:  e0 03 19 aa   mov	x0, x25
0x2511773f0:  e0 29 be 95   bl	j__CFRelease
0x2511773f4:  ; loc_2511773f4
0x2511773f4:  60 5a 40 f9   ldr	x0, [x19, #0xb0]
0x2511773f8:  80 04 00 b4   cbz	x0, loc_251177488 ; ⤵ 0x90
0x2511773fc:  db 57 f7 97   bl	_OUTLINED_FUNCTION_29
0x251177400:  cc 01 00 54   b.gt	loc_251177438 ; ⤵ 0x38
0x251177404:  1f 05 00 31   cmn	w8, #0x1
0x251177408:  a1 00 00 54   b.ne	loc_25117741c ; ⤵ 0x14
0x25117740c:  58 db f7 97   bl	_OUTLINED_FUNCTION_4
0x251177410:  e8 03 00 aa   mov	x8, x0
0x251177414:  60 5a 40 f9   ldr	x0, [x19, #0xb0]
0x251177418:  08 01 00 34   cbz	w8, loc_251177438 ; ⤵ 0x20
0x25117741c:  ; loc_25117741c
0x25117741c:  68 16 40 f9   ldr	x8, [x19, #0x28]
0x251177420:  e8 03 00 a9   stp	x8, x0, [sp]
0x251177424:  a5 17 f7 97   bl	_OUTLINED_FUNCTION_9
0x251177428:  63 04 00 f0   adrp	x3, 0x251206000
0x25117742c:  63 fc 2b 91   add	x3, x3, #0xaff ; "%@ applying FVD options overrides: %@\n"
0x251177430:  a6 fd f6 97   bl	_OUTLINED_FUNCTION_9
0x251177434:  60 5a 40 f9   ldr	x0, [x19, #0xb0]
0x251177438:  ; loc_251177438
0x251177438:  e8 e3 00 91   add	x8, sp, #0x38
0x25117743c:  70 81 0b b0   adrp	x16, 0x2681a4000
0x251177440:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x251177444:  f1 03 08 aa   mov	x17, x8
0x251177448:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x25117744c:  30 0a c1 da   pacda	x16, x17
0x251177450:  f0 1f 00 f9   str	x16, [sp, #0x38]
0x251177454:  89 04 00 d0   adrp	x9, 0x251209000
0x251177458:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x25117745c:  e0 23 00 fd   str	d0, [sp, #0x40]
0x251177460:  08 41 00 91   add	x8, x8, #0x10
0x251177464:  70 fb ff 90   adrp	x16, 0x2510e3000
0x251177468:  10 22 02 91   add	x16, x16, #0x88 ; ___screenstream_createVirtualDisplayActivationOptions_block_invoke
0x25117746c:  10 01 c1 da   pacia	x16, x8
0x251177470:  e8 10 0c f0   adrp	x8, 0x269396000
0x251177474:  08 81 2c 91   add	x8, x8, #0xb20 ; ___block_descriptor_tmp.253
0x251177478:  f0 a3 04 a9   stp	x16, x8, [sp, #0x48]
0x25117747c:  f4 2f 00 f9   str	x20, [sp, #0x58]
0x251177480:  e1 e3 00 91   add	x1, sp, #0x38
0x251177484:  bf 56 be 95   bl	j__CFDictionaryApplyBlock
0x251177488:  ; loc_251177488
0x251177488:  73 ce 40 f9   ldr	x19, [x19, #0x198]
0x25117748c:  73 02 00 b4   cbz	x19, loc_2511774d8 ; ⤵ 0x4c
0x251177490:  81 4c 12 f0   adrp	x1, 0x275b0a000
0x251177494:  21 80 16 91   add	x1, x1, #0x5a0 ; "tvout"
0x251177498:  e0 03 13 aa   mov	x0, x19
0x25117749c:  a9 29 be 95   bl	j__CFEqual
0x2511774a0:  c0 00 00 34   cbz	w0, loc_2511774b8 ; ⤵ 0x18
0x2511774a4:  e1 02 40 f9   ldr	x1, [x23]
0x2511774a8:  e0 03 14 aa   mov	x0, x20
0x2511774ac:  42 00 80 52   mov	w2, #0x2
0x2511774b0:  d8 69 be 95   bl	j__FigCFDictionarySetInt
0x2511774b4:  06 00 00 14   b	loc_2511774cc ; ⤵ 0x18
0x2511774b8:  ; loc_2511774b8
0x2511774b8:  81 4c 12 f0   adrp	x1, 0x275b0a000
0x2511774bc:  21 00 17 91   add	x1, x1, #0x5c0 ; "mirroringonly"
0x2511774c0:  e0 03 13 aa   mov	x0, x19
0x2511774c4:  9f 29 be 95   bl	j__CFEqual
0x2511774c8:  a0 01 00 34   cbz	w0, loc_2511774fc ; ⤵ 0x34
0x2511774cc:  ; loc_2511774cc
0x2511774cc:  01 03 40 f9   ldr	x1, [x24]
0x2511774d0:  d7 b1 fd 97   bl	_OUTLINED_FUNCTION_41
0x2511774d4:  ; loc_2511774d4
0x2511774d4:  cf 69 be 95   bl	j__FigCFDictionarySetInt
0x2511774d8:  ; loc_2511774d8
0x2511774d8:  e0 03 14 aa   mov	x0, x20
0x2511774dc:  fd 7b 4d a9   ldp	fp, lr, [sp, #0xd0]
0x2511774e0:  f4 4f 4c a9   ldp	x20, x19, [sp, #0xc0]
0x2511774e4:  f6 57 4b a9   ldp	x22, x21, [sp, #0xb0]
0x2511774e8:  f8 5f 4a a9   ldp	x24, x23, [sp, #0xa0]
0x2511774ec:  fa 67 49 a9   ldp	x26, x25, [sp, #0x90]
0x2511774f0:  fc 6f 48 a9   ldp	x28, x27, [sp, #0x80]
0x2511774f4:  ff 83 03 91   add	sp, sp, #0xe0
0x2511774f8:  ff 0f 5f d6   retab
0x2511774fc:  ; loc_2511774fc
0x2511774fc:  81 4c 12 f0   adrp	x1, 0x275b0a000
0x251177500:  21 80 17 91   add	x1, x1, #0x5e0 ; "seconddisplay"
0x251177504:  e0 03 13 aa   mov	x0, x19
0x251177508:  8e 29 be 95   bl	j__CFEqual
0x25117750c:  60 00 00 34   cbz	w0, loc_251177518 ; ⤵ 0xc
0x251177510:  27 18 f7 97   bl	_OUTLINED_FUNCTION_23
0x251177514:  f0 ff ff 17   b	loc_2511774d4 ; ⤴ -0x40
0x251177518:  ; loc_251177518
0x251177518:  81 4c 12 f0   adrp	x1, 0x275b0a000
0x25117751c:  21 00 18 91   add	x1, x1, #0x600 ; "continuity"
0x251177520:  e0 03 13 aa   mov	x0, x19
0x251177524:  87 29 be 95   bl	j__CFEqual
0x251177528:  e0 00 00 34   cbz	w0, loc_251177544 ; ⤵ 0x1c
0x25117752c:  20 18 f7 97   bl	_OUTLINED_FUNCTION_23
0x251177530:  b8 69 be 95   bl	j__FigCFDictionarySetInt
0x251177534:  82 4c 12 f0   adrp	x2, 0x275b0a000
0x251177538:  42 80 18 91   add	x2, x2, #0x620 ; "Continuity"
0x25117753c:  dd b1 fd 97   bl	_OUTLINED_FUNCTION_58
0x251177540:  e6 ff ff 17   b	loc_2511774d8 ; ⤴ -0x68
0x251177544:  ; loc_251177544
0x251177544:  81 4c 12 d0   adrp	x1, 0x275b09000
0x251177548:  21 80 2f 91   add	x1, x1, #0xbe0 ; "airplay"
0x25117754c:  e0 03 13 aa   mov	x0, x19
0x251177550:  7c 29 be 95   bl	j__CFEqual
0x251177554:  a0 00 00 34   cbz	w0, loc_251177568 ; ⤵ 0x14
0x251177558:  01 03 40 f9   ldr	x1, [x24]
0x25117755c:  e0 03 14 aa   mov	x0, x20
0x251177560:  02 00 80 52   mov	w2, #0
0x251177564:  dc ff ff 17   b	loc_2511774d4 ; ⤴ -0x90
0x251177568:  ; loc_251177568
0x251177568:  48 33 4b b9   ldr	w8, [x26, #0xb30]
0x25117756c:  1f a1 00 71   cmp	w8, #0x28
0x251177570:  4c fb ff 54   b.gt	loc_2511774d8 ; ⤴ -0x98
0x251177574:  1f 05 00 31   cmn	w8, #0x1
0x251177578:  e1 00 00 54   b.ne	loc_251177594 ; ⤵ 0x1c
0x25117757c:  48 06 80 52   mov	w8, #0x32
0x251177580:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x251177584:  01 29 00 51   sub	w1, w8, #0xa
0x251177588:  db 01 f7 97   bl	_OUTLINED_FUNCTION_11
0x25117758c:  0d 26 be 95   bl	j___LogCategory_Initialize
0x251177590:  40 fa ff 34   cbz	w0, loc_2511774d8 ; ⤴ -0xb8
0x251177594:  ; loc_251177594
0x251177594:  48 06 80 52   mov	w8, #0x32
0x251177598:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x25117759c:  02 29 00 51   sub	w2, w8, #0xa
0x2511775a0:  f3 03 00 f9   str	x19, [sp]
0x2511775a4:  d4 01 f7 97   bl	_OUTLINED_FUNCTION_11
0x2511775a8:  61 04 00 f0   adrp	x1, 0x251206000
0x2511775ac:  21 f8 2f 91   add	x1, x1, #0xbfe ; "void applyMirroringModeOverride(CFMutableDictionaryRef, CFStringRef)"
0x2511775b0:  63 04 00 f0   adrp	x3, 0x251206000
0x2511775b4:  63 0c 31 91   add	x3, x3, #0xc43 ; "ignoring unsupported mirroring mode override '%@'\n"
0x2511775b8:  52 26 be 95   bl	j__LogPrintF
0x2511775bc:  c7 ff ff 17   b	loc_2511774d8 ; ⤴ -0xe4
0x2511775c0:  ; loc_2511775c0
0x2511775c0:  61 04 00 f0   adrp	x1, 0x251206000
0x2511775c4:  21 98 2c 91   add	x1, x1, #0xb26 ; "screenstream_getVirtualDisplayUsageMode"
0x2511775c8:  e0 03 08 aa   mov	x0, x8
0x2511775cc:  e2 60 81 52   mov	w2, #0xb07
0x2511775d0:  cb fd ff 17   b	loc_251176cfc ; ⤴ -0x8d4
0x2511775d4:  ; loc_2511775d4
0x2511775d4:  61 7a 40 fd   ldr	d1, [x19, #0xf0]
0x2511775d8:  08 00 78 1e   fcvtzs	w8, d0
0x2511775dc:  29 00 78 1e   fcvtzs	w9, d1
0x2511775e0:  68 fe ff 17   b	loc_251176f80 ; ⤴ -0x660

