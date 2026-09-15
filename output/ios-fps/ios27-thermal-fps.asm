
-[DBThermalMonitor suggestedFrameRateLimit]:
0x25e43fb44:  7f 23 03 d5   pacibsp
0x25e43fb48:  ff 03 01 d1   sub	sp, sp, #0x40
0x25e43fb4c:  f6 57 01 a9   stp	x22, x21, [sp, #0x10]
0x25e43fb50:  f4 4f 02 a9   stp	x20, x19, [sp, #0x20]
0x25e43fb54:  fd 7b 03 a9   stp	fp, lr, [sp, #0x30]
0x25e43fb58:  fd c3 00 91   add	fp, sp, #0x30
0x25e43fb5c:  f4 03 00 aa   mov	x20, x0
0x25e43fb60:  ff 3f 00 39   strb	wzr, [sp, #0xf]
0x25e43fb64:  00 1b 0c d0   adrp	x0, 0x2767a1000
0x25e43fb68:  00 20 17 91   add	x0, x0, #0x5c8 ; "FrameRateLimit"
0x25e43fb6c:  c1 1a 0c f0   adrp	x1, 0x27679a000
0x25e43fb70:  21 a0 10 91   add	x1, x1, #0x428 ; "com.apple.carplay.internal"
0x25e43fb74:  e2 3f 00 91   add	x2, sp, #0xf
0x25e43fb78:  02 12 72 94   bl	j__CFPreferencesGetAppIntegerValue
0x25e43fb7c:  e8 3f 40 39   ldrb	w8, [sp, #0xf]
0x25e43fb80:  1f 01 00 71   cmp	w8, #0
0x25e43fb84:  04 18 40 fa   ccmp	x0, #0, #0x4, ne
0x25e43fb88:  41 01 00 54   b.ne	loc_25e43fbb0 ; ⤵ 0x28
0x25e43fb8c:  e0 03 14 aa   mov	x0, x20
0x25e43fb90:  3c 83 6f 94   bl	0x260020880
0x25e43fb94:  08 04 00 d1   sub	x8, x0, #0x1
0x25e43fb98:  1f 09 00 f1   cmp	x8, #0x2
0x25e43fb9c:  28 05 00 54   b.hi	loc_25e43fc40 ; ⤵ 0xa4
0x25e43fba0:  89 93 05 b0   adrp	x9, 0x2696b0000
0x25e43fba4:  29 c1 3e 91   add	x9, x9, #0xfb0
0x25e43fba8:  20 79 68 f8   ldr	x0, [x9, x8, lsl #0x3]
0x25e43fbac:  26 00 00 14   b	loc_25e43fc44 ; ⤵ 0x98
0x25e43fbb0:  ; loc_25e43fbb0
0x25e43fbb0:  f3 03 00 aa   mov	x19, x0
0x25e43fbb4:  00 00 80 d2   mov	x0, #0
0x25e43fbb8:  e8 9d ff 97   bl	_DBLogForCategory
0x25e43fbbc:  61 14 72 94   bl	j__objc_claimAutoreleasedReturnValue
0x25e43fbc0:  f5 03 00 aa   mov	x21, x0
0x25e43fbc4:  01 00 80 52   mov	w1, #0
0x25e43fbc8:  5e 15 72 94   bl	j__os_log_type_enabled
0x25e43fbcc:  c0 01 00 34   cbz	w0, loc_25e43fc04 ; ⤵ 0x38
0x25e43fbd0:  08 18 00 90   adrp	x8, 0x25e73f000
0x25e43fbd4:  00 09 42 fd   ldr	d0, [x8, #0x410]
0x25e43fbd8:  e0 03 00 bd   str	s0, [sp]
0x25e43fbdc:  f3 43 00 f8   stur	x19, [sp, #0x4]
0x25e43fbe0:  e0 fe ff f0   adrp	x0, 0x25e41e000
0x25e43fbe4:  00 00 00 91   add	x0, x0, #0 ; "\xcf\xfa\xed\xfe\f"
0x25e43fbe8:  43 19 00 d0   adrp	x3, 0x25e769000
0x25e43fbec:  63 04 01 91   add	x3, x3, #0x41 ; "frameRateLimit preference is: %lu"
0x25e43fbf0:  e4 03 00 91   mov	x4, sp
0x25e43fbf4:  e1 03 15 aa   mov	x1, x21
0x25e43fbf8:  02 00 80 52   mov	w2, #0
0x25e43fbfc:  85 01 80 52   mov	w5, #0xc
0x25e43fc00:  c0 13 72 94   bl	j___os_log_impl
0x25e43fc04:  ; loc_25e43fc04
0x25e43fc04:  a7 14 72 94   bl	j__objc_release_x21
0x25e43fc08:  e0 03 14 aa   mov	x0, x20
0x25e43fc0c:  1d 83 6f 94   bl	0x260020880
0x25e43fc10:  1f 0c 00 f1   cmp	x0, #0x3
0x25e43fc14:  a0 00 00 54   b.eq	loc_25e43fc28 ; ⤵ 0x14
0x25e43fc18:  1f 08 00 f1   cmp	x0, #0x2
0x25e43fc1c:  60 00 00 54   b.eq	loc_25e43fc28 ; ⤵ 0xc
0x25e43fc20:  1f 04 00 f1   cmp	x0, #0x1
0x25e43fc24:  e1 00 00 54   b.ne	loc_25e43fc40 ; ⤵ 0x1c
0x25e43fc28:  ; loc_25e43fc28
0x25e43fc28:  88 e4 04 90   adrp	x8, 0x2680cf000
0x25e43fc2c:  00 45 43 f9   ldr	x0, [x8, #0x688] ; _ptr.1e6f2dd18 (Foundation/__DATA_DIRTY.__objc_data)
0x25e43fc30:  e2 03 13 aa   mov	x2, x19
0x25e43fc34:  8f 70 6f 94   bl	0x26001be70
0x25e43fc38:  42 14 72 94   bl	j__objc_claimAutoreleasedReturnValue
0x25e43fc3c:  02 00 00 14   b	loc_25e43fc44 ; ⤵ 0x8
0x25e43fc40:  ; loc_25e43fc40
0x25e43fc40:  00 00 80 d2   mov	x0, #0
0x25e43fc44:  ; loc_25e43fc44
0x25e43fc44:  fd 7b 43 a9   ldp	fp, lr, [sp, #0x30]
0x25e43fc48:  f4 4f 42 a9   ldp	x20, x19, [sp, #0x20]
0x25e43fc4c:  f6 57 41 a9   ldp	x22, x21, [sp, #0x10]
0x25e43fc50:  ff 03 01 91   add	sp, sp, #0x40
0x25e43fc54:  ff 23 03 d5   autibsp
0x25e43fc58:  d0 07 1e ca   eor	x16, lr, lr, lsl #0x1
0x25e43fc5c:  50 00 f0 b6   tbz	x16, #0x3e, loc_25e43fc64 ; ⤵ 0x8
0x25e43fc60:  20 8e 38 d4   brk	#0xc471
0x25e43fc64:  ; loc_25e43fc64
0x25e43fc64:  2f 14 72 14   b	j__objc_autoreleaseReturnValue

