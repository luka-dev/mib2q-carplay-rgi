
_APEndpointStreamScreenCreate:
0x2510df8ac:  7f 23 03 d5   pacibsp
0x2510df8b0:  ff 43 05 d1   sub	sp, sp, #0x150
0x2510df8b4:  fc 6f 0f a9   stp	x28, x27, [sp, #0xf0]
0x2510df8b8:  fa 67 10 a9   stp	x26, x25, [sp, #0x100]
0x2510df8bc:  f8 5f 11 a9   stp	x24, x23, [sp, #0x110]
0x2510df8c0:  f6 57 12 a9   stp	x22, x21, [sp, #0x120]
0x2510df8c4:  f4 4f 13 a9   stp	x20, x19, [sp, #0x130]
0x2510df8c8:  fd 7b 14 a9   stp	fp, lr, [sp, #0x140]
0x2510df8cc:  fd 03 05 91   add	fp, sp, #0x140
0x2510df8d0:  28 86 0b b0   adrp	x8, 0x2681a4000
0x2510df8d4:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.1e6ef16f0 (libsystem_c.dylib/__DATA_DIRTY.__common)
0x2510df8d8:  08 01 40 f9   ldr	x8, [x8]
0x2510df8dc:  a8 03 1a f8   stur	x8, [fp, #-0x60]
0x2510df8e0:  ff 7f 08 a9   stp	xzr, xzr, [sp, #0x80]
0x2510df8e4:  ff 7f 07 a9   stp	xzr, xzr, [sp, #0x70]
0x2510df8e8:  08 fe e7 d2   mov	x8, #0x3ff0000000000000
0x2510df8ec:  e8 37 00 f9   str	x8, [sp, #0x68]
0x2510df8f0:  ff 67 00 b9   str	wzr, [sp, #0x64]
0x2510df8f4:  c2 d5 00 b4   cbz	x2, loc_2510e13ac ; ⤵ 0x1ab8
0x2510df8f8:  fc 03 07 aa   mov	x28, x7
0x2510df8fc:  f8 03 06 aa   mov	x24, x6
0x2510df900:  fa 03 05 aa   mov	x26, x5
0x2510df904:  f5 03 04 aa   mov	x21, x4
0x2510df908:  f9 03 03 aa   mov	x25, x3
0x2510df90c:  fb 03 02 aa   mov	x27, x2
0x2510df910:  f6 03 01 aa   mov	x22, x1
0x2510df914:  f7 03 00 aa   mov	x23, x0
0x2510df918:  e0 03 02 aa   mov	x0, x2
0x2510df91c:  35 88 c0 95   bl	j__CFGetTypeID
0x2510df920:  f3 03 00 aa   mov	x19, x0
0x2510df924:  9f 88 c0 95   bl	j__CFStringGetTypeID
0x2510df928:  7f 02 00 eb   cmp	x19, x0
0x2510df92c:  01 d4 00 54   b.ne	loc_2510e13ac ; ⤵ 0x1a80
0x2510df930:  35 d4 00 b4   cbz	x21, loc_2510e13b4 ; ⤵ 0x1a84
0x2510df934:  e0 03 15 aa   mov	x0, x21
0x2510df938:  2e 88 c0 95   bl	j__CFGetTypeID
0x2510df93c:  f3 03 00 aa   mov	x19, x0
0x2510df940:  5c ac fd 97   bl	_APEndpointDescriptionGetTypeID
0x2510df944:  7f 02 00 eb   cmp	x19, x0
0x2510df948:  61 d3 00 54   b.ne	loc_2510e13b4 ; ⤵ 0x1a6c
0x2510df94c:  9c d3 00 b4   cbz	x28, loc_2510e13bc ; ⤵ 0x1a70
0x2510df950:  b8 d3 00 b4   cbz	x24, loc_2510e13c4 ; ⤵ 0x1a74
0x2510df954:  f6 1b 00 f9   str	x22, [sp, #0x30]
0x2510df958:  b6 01 00 b4   cbz	x22, loc_2510df98c ; ⤵ 0x34
0x2510df95c:  c1 50 12 90   adrp	x1, 0x275af7000
0x2510df960:  21 00 0f 91   add	x1, x1, #0x3c0 ; "TransportStream"
0x2510df964:  e0 03 16 aa   mov	x0, x22
0x2510df968:  12 8b c0 95   bl	j__CFDictionaryGetValue
0x2510df96c:  40 01 00 b4   cbz	x0, loc_2510df994 ; ⤵ 0x28
0x2510df970:  f3 03 00 aa   mov	x19, x0
0x2510df974:  1f 88 c0 95   bl	j__CFGetTypeID
0x2510df978:  f4 03 00 aa   mov	x20, x0
0x2510df97c:  3d c5 c0 95   bl	j__APTransportStreamGetTypeID
0x2510df980:  9f 02 00 eb   cmp	x20, x0
0x2510df984:  74 02 9f 9a   csel	x20, x19, xzr, eq
0x2510df988:  04 00 00 14   b	loc_2510df998 ; ⤵ 0x10
0x2510df98c:  ; loc_2510df98c
0x2510df98c:  14 00 80 d2   mov	x20, #0
0x2510df990:  14 00 00 14   b	loc_2510df9e0 ; ⤵ 0x50
0x2510df994:  ; loc_2510df994
0x2510df994:  14 00 80 d2   mov	x20, #0
0x2510df998:  ; loc_2510df998
0x2510df998:  e8 7f 0b 90   adrp	x8, 0x2680db000
0x2510df99c:  08 c1 43 f9   ldr	x8, [x8, #0x780] ; _ptr.1e0c04918 (MediaToolbox/__DATA_CONST.__const)
0x2510df9a0:  01 01 40 f9   ldr	x1, [x8]
0x2510df9a4:  e2 a3 01 91   add	x2, sp, #0x68
0x2510df9a8:  e0 03 16 aa   mov	x0, x22
0x2510df9ac:  15 b8 c0 95   bl	j__FigCFDictionaryGetDoubleIfPresent
0x2510df9b0:  e8 7f 0b 90   adrp	x8, 0x2680db000
0x2510df9b4:  08 b1 43 f9   ldr	x8, [x8, #0x760] ; _ptr.1e0c04960 (MediaToolbox/__DATA_CONST.__const)
0x2510df9b8:  01 01 40 f9   ldr	x1, [x8]
0x2510df9bc:  e2 93 01 91   add	x2, sp, #0x64
0x2510df9c0:  e0 03 16 aa   mov	x0, x22
0x2510df9c4:  13 b8 c0 95   bl	j__FigCFDictionaryGetInt32IfPresent
0x2510df9c8:  88 15 0c 90   adrp	x8, 0x26938f000
0x2510df9cc:  08 21 28 91   add	x8, x8, #0xa08 ; _kAPEndpointStreamCreationOptionKey_LogPrefix
0x2510df9d0:  01 01 40 f9   ldr	x1, [x8]
0x2510df9d4:  e0 03 16 aa   mov	x0, x22
0x2510df9d8:  f6 8a c0 95   bl	j__CFDictionaryGetValue
0x2510df9dc:  f6 03 00 aa   mov	x22, x0
0x2510df9e0:  ; loc_2510df9e0
0x2510df9e0:  28 03 14 aa   orr	x8, x25, x20
0x2510df9e4:  48 cf 00 b4   cbz	x8, loc_2510e13cc ; ⤵ 0x19e8
0x2510df9e8:  59 00 00 b4   cbz	x25, loc_2510df9f0 ; ⤵ 0x8
0x2510df9ec:  54 cf 00 b5   cbnz	x20, loc_2510e13d4 ; ⤵ 0x19e8
0x2510df9f0:  ; loc_2510df9f0
0x2510df9f0:  fc 17 00 f9   str	x28, [sp, #0x28]
0x2510df9f4:  77 c9 c0 95   bl	j__FigEndpointStreamGetClassID
0x2510df9f8:  e2 03 00 aa   mov	x2, x0
0x2510df9fc:  a1 50 12 90   adrp	x1, 0x275af3000
0x2510dfa00:  21 60 1d 91   add	x1, x1, #0x758 ; _kAPEndpointStreamScreenVTable
0x2510dfa04:  e3 23 02 91   add	x3, sp, #0x88
0x2510dfa08:  e0 03 17 aa   mov	x0, x23
0x2510dfa0c:  09 b7 c0 95   bl	j__CMDerivedObjectCreate
0x2510dfa10:  80 ce 00 35   cbnz	w0, loc_2510e13e0 ; ⤵ 0x19d0
0x2510dfa14:  e0 47 40 f9   ldr	x0, [sp, #0x88]
0x2510dfa18:  ea b6 c0 95   bl	j__CMBaseObjectGetDerivedStorage
0x2510dfa1c:  f3 03 00 aa   mov	x19, x0
0x2510dfa20:  e8 47 40 f9   ldr	x8, [sp, #0x88]
0x2510dfa24:  e8 03 00 f9   str	x8, [sp]
0x2510dfa28:  22 09 00 d0   adrp	x2, 0x251205000
0x2510dfa2c:  42 28 0c 91   add	x2, x2, #0x30a ; "APEndpointStreamScreen.%{ptr}"
0x2510dfa30:  e0 83 02 91   add	x0, sp, #0xa0
0x2510dfa34:  01 08 80 52   mov	w1, #0x40
0x2510dfa38:  9a ba c0 95   bl	j__SNPrintF
0x2510dfa3c:  e0 83 02 91   add	x0, sp, #0xa0
0x2510dfa40:  01 00 80 d2   mov	x1, #0
0x2510dfa44:  f7 84 c0 95   bl	j__dispatch_queue_create
0x2510dfa48:  60 02 00 f9   str	x0, [x19]
0x2510dfa4c:  e0 83 02 91   add	x0, sp, #0xa0
0x2510dfa50:  04 8c c0 95   bl	j__os_transaction_create
0x2510dfa54:  60 0a 00 f9   str	x0, [x19, #0x10]
0x2510dfa58:  20 09 00 d0   adrp	x0, 0x251205000
0x2510dfa5c:  00 a0 0c 91   add	x0, x0, #0x328 ; "APEndpointStreamScreen.properties"
0x2510dfa60:  01 00 80 d2   mov	x1, #0
0x2510dfa64:  ef 84 c0 95   bl	j__dispatch_queue_create
0x2510dfa68:  60 fa 00 f9   str	x0, [x19, #0x1f0]
0x2510dfa6c:  e8 47 40 f9   ldr	x8, [sp, #0x88]
0x2510dfa70:  e8 03 00 f9   str	x8, [sp]
0x2510dfa74:  22 09 00 d0   adrp	x2, 0x251205000
0x2510dfa78:  42 28 0d 91   add	x2, x2, #0x34a ; "APEndpointStreamScreen.%{ptr}.notification"
0x2510dfa7c:  e0 83 02 91   add	x0, sp, #0xa0
0x2510dfa80:  01 08 80 52   mov	w1, #0x40
0x2510dfa84:  87 ba c0 95   bl	j__SNPrintF
0x2510dfa88:  e0 83 02 91   add	x0, sp, #0xa0
0x2510dfa8c:  01 00 80 d2   mov	x1, #0
0x2510dfa90:  e4 84 c0 95   bl	j__dispatch_queue_create
0x2510dfa94:  60 32 00 f9   str	x0, [x19, #0x60]
0x2510dfa98:  e0 37 40 fd   ldr	d0, [sp, #0x68]
0x2510dfa9c:  60 8e 00 fd   str	d0, [x19, #0x118]
0x2510dfaa0:  e8 67 40 b9   ldr	w8, [sp, #0x64]
0x2510dfaa4:  68 22 01 b9   str	w8, [x19, #0x120]
0x2510dfaa8:  e0 03 1b aa   mov	x0, x27
0x2510dfaac:  35 88 c0 95   bl	j__CFRetain
0x2510dfab0:  60 06 00 f9   str	x0, [x19, #0x8]
0x2510dfab4:  e0 03 15 aa   mov	x0, x21
0x2510dfab8:  32 88 c0 95   bl	j__CFRetain
0x2510dfabc:  60 1e 00 f9   str	x0, [x19, #0x38]
0x2510dfac0:  a8 50 12 f0   adrp	x8, 0x275af6000
0x2510dfac4:  08 81 18 91   add	x8, x8, #0x620
0x2510dfac8:  df 02 00 f1   cmp	x22, #0
0x2510dfacc:  08 01 96 9a   csel	x8, x8, x22, eq
0x2510dfad0:  29 05 00 90   adrp	x9, 0x251183000
0x2510dfad4:  29 41 36 91   add	x9, x9, #0xd90
0x2510dfad8:  ea 04 00 90   adrp	x10, 0x25117b000
0x2510dfadc:  4a d9 13 91   add	x10, x10, #0x4f6 ; data_25117b4f6 (AirPlaySender/__TEXT.__cstring)
0x2510dfae0:  49 01 89 9a   csel	x9, x10, x9, eq
0x2510dfae4:  ea 47 40 f9   ldr	x10, [sp, #0x88]
0x2510dfae8:  e9 ab 00 a9   stp	x9, x10, [sp, #0x8]
0x2510dfaec:  e8 03 00 f9   str	x8, [sp]
0x2510dfaf0:  21 09 00 d0   adrp	x1, 0x251205000
0x2510dfaf4:  21 d4 0d 91   add	x1, x1, #0x375 ; "%@%s[%{ptr}]"
0x2510dfaf8:  00 00 80 d2   mov	x0, #0
0x2510dfafc:  35 b6 c0 95   bl	j__CFStringCreateF
0x2510dfb00:  f5 03 13 aa   mov	x21, x19
0x2510dfb04:  a0 8e 02 f8   str	x0, [x21, #0x28]!
0x2510dfb08:  60 1e 40 f9   ldr	x0, [x19, #0x38]
0x2510dfb0c:  70 86 f9 97   bl	_APEndpointDescriptionGetCMBaseObject
0x2510dfb10:  f6 03 00 aa   mov	x22, x0
0x2510dfb14:  d3 af c0 95   bl	j__CMBaseObjectGetVTable
0x2510dfb18:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x2510dfb1c:  f1 03 00 aa   mov	x17, x0
0x2510dfb20:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x2510dfb24:  30 1a c1 da   autda	x16, x17
0x2510dfb28:  09 0e 43 f8   ldr	x9, [x16, #0x30]!
0x2510dfb2c:  69 07 00 b4   cbz	x9, loc_2510dfc18 ; ⤵ 0xec
0x2510dfb30:  e8 03 10 aa   mov	x8, x16
0x2510dfb34:  a8 e4 f2 f2   movk	x8, #0x9725, lsl #0x30
0x2510dfb38:  f0 03 09 aa   mov	x16, x9
0x2510dfb3c:  f1 03 08 aa   mov	x17, x8
0x2510dfb40:  30 12 c1 da   autia	x16, x17
0x2510dfb44:  f1 03 10 aa   mov	x17, x16
0x2510dfb48:  f1 43 c1 da   xpaci	x17
0x2510dfb4c:  1f 02 11 eb   cmp	x16, x17
0x2510dfb50:  40 00 00 54   b.eq	loc_2510dfb58 ; ⤵ 0x8
0x2510dfb54:  00 8e 38 d4   brk	#0xc470
0x2510dfb58:  ; loc_2510dfb58
0x2510dfb58:  f0 23 c1 da   paciza	x16
0x2510dfb5c:  f0 05 00 b4   cbz	x16, loc_2510dfc18 ; ⤵ 0xbc
0x2510dfb60:  8a 15 0c d0   adrp	x10, 0x269391000
0x2510dfb64:  4a 41 3a 91   add	x10, x10, #0xe90 ; _kAPEndpointDescriptionProperty_DisplayDescriptionArray
0x2510dfb68:  41 01 40 f9   ldr	x1, [x10]
0x2510dfb6c:  2a 7f 0b 90   adrp	x10, 0x2680c3000
0x2510dfb70:  4a d1 40 f9   ldr	x10, [x10, #0x1a0] ; _ptr.1807dd1c8 (CoreFoundation/__TEXT.__const)
0x2510dfb74:  5b 01 40 f9   ldr	x27, [x10]
0x2510dfb78:  e3 03 02 91   add	x3, sp, #0x80
0x2510dfb7c:  e0 03 16 aa   mov	x0, x22
0x2510dfb80:  e2 03 1b aa   mov	x2, x27
0x2510dfb84:  28 09 3f d7   blraa	x9, x8
0x2510dfb88:  00 d2 00 35   cbnz	w0, loc_2510e15c8 ; ⤵ 0x1a40
0x2510dfb8c:  e0 43 40 f9   ldr	x0, [sp, #0x80]
0x2510dfb90:  80 9f 00 b4   cbz	x0, loc_2510e0f80 ; ⤵ 0x13f0
0x2510dfb94:  5b 8a c0 95   bl	j__CFArrayGetCount
0x2510dfb98:  1f 00 00 f1   cmp	x0, #0
0x2510dfb9c:  2d 9f 00 54   b.le	loc_2510e0f80 ; ⤵ 0x13e4
0x2510dfba0:  e0 43 40 f9   ldr	x0, [sp, #0x80]
0x2510dfba4:  01 00 80 d2   mov	x1, #0
0x2510dfba8:  5a 8a c0 95   bl	j__CFArrayGetValueAtIndex
0x2510dfbac:  20 c2 00 b4   cbz	x0, loc_2510e13f0 ; ⤵ 0x1844
0x2510dfbb0:  fc 03 00 aa   mov	x28, x0
0x2510dfbb4:  3a 06 00 b4   cbz	x26, loc_2510dfc78 ; ⤵ 0xc4
0x2510dfbb8:  e0 03 1a aa   mov	x0, x26
0x2510dfbbc:  f1 87 c0 95   bl	j__CFRetain
0x2510dfbc0:  60 26 00 f9   str	x0, [x19, #0x48]
0x2510dfbc4:  88 15 0c d0   adrp	x8, 0x269391000
0x2510dfbc8:  08 c1 23 91   add	x8, x8, #0x8f0 ; _kAPEndpointDisplayDescriptionProperty_UUID
0x2510dfbcc:  16 01 40 f9   ldr	x22, [x8]
0x2510dfbd0:  e3 e3 01 91   add	x3, sp, #0x78
0x2510dfbd4:  e0 03 1c aa   mov	x0, x28
0x2510dfbd8:  e1 03 16 aa   mov	x1, x22
0x2510dfbdc:  e2 03 1b aa   mov	x2, x27
0x2510dfbe0:  41 6c fd 97   bl	_APEndpointDisplayDescriptionCopyProperty
0x2510dfbe4:  e8 3f 40 f9   ldr	x8, [sp, #0x78]
0x2510dfbe8:  88 c4 00 b4   cbz	x8, loc_2510e1478 ; ⤵ 0x1890
0x2510dfbec:  60 26 40 f9   ldr	x0, [x19, #0x48]
0x2510dfbf0:  e3 c3 01 91   add	x3, sp, #0x70
0x2510dfbf4:  e1 03 16 aa   mov	x1, x22
0x2510dfbf8:  e2 03 1b aa   mov	x2, x27
0x2510dfbfc:  3a 6c fd 97   bl	_APEndpointDisplayDescriptionCopyProperty
0x2510dfc00:  e0 07 47 a9   ldp	x0, x1, [sp, #0x70]
0x2510dfc04:  c7 b7 c0 95   bl	j__FigCFEqual
0x2510dfc08:  1f 00 00 71   cmp	w0, #0
0x2510dfc0c:  c8 0d 80 52   mov	w8, #0x6e
0x2510dfc10:  08 15 88 1a   cinc	w8, w8, eq
0x2510dfc14:  1d 00 00 14   b	loc_2510dfc88 ; ⤵ 0x74
0x2510dfc18:  ; loc_2510dfc18
0x2510dfc18:  bc 3d 86 12   mov	w28, #0xffffce12
0x2510dfc1c:  ; loc_2510dfc1c
0x2510dfc1c:  21 09 00 d0   adrp	x1, 0x251205000
0x2510dfc20:  21 b4 0b 91   add	x1, x1, #0x2ed ; "APEndpointStreamScreenCreate"
0x2510dfc24:  e0 03 1c aa   mov	x0, x28
0x2510dfc28:  82 c0 81 52   mov	w2, #0xe04
0x2510dfc2c:  ; loc_2510dfc2c
0x2510dfc2c:  03 00 80 52   mov	w3, #0
0x2510dfc30:  cc b1 c0 95   bl	j__APSLogErrorAt
0x2510dfc34:  ; loc_2510dfc34
0x2510dfc34:  68 9d 0f f0   adrp	x8, 0x27048e000
0x2510dfc38:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x2510dfc3c:  1f 69 01 71   cmp	w8, #0x5a
0x2510dfc40:  4c b4 00 54   b.gt	loc_2510e12c8 ; ⤵ 0x1688
0x2510dfc44:  1f 05 00 31   cmn	w8, #0x1
0x2510dfc48:  01 01 00 54   b.ne	loc_2510dfc68 ; ⤵ 0x20
0x2510dfc4c:  08 05 80 52   mov	w8, #0x28
0x2510dfc50:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510dfc54:  60 9d 0f f0   adrp	x0, 0x27048e000
0x2510dfc58:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510dfc5c:  01 c9 00 11   add	w1, w8, #0x32
0x2510dfc60:  58 84 c0 95   bl	j___LogCategory_Initialize
0x2510dfc64:  20 b3 00 34   cbz	w0, loc_2510e12c8 ; ⤵ 0x1664
0x2510dfc68:  ; loc_2510dfc68
0x2510dfc68:  e0 03 13 aa   mov	x0, x19
0x2510dfc6c:  e1 03 1c aa   mov	x1, x28
0x2510dfc70:  e6 5f 02 94   bl	_APEndpointStreamScreenCreate.cold.29
0x2510dfc74:  95 05 00 14   b	loc_2510e12c8 ; ⤵ 0x1654
0x2510dfc78:  ; loc_2510dfc78
0x2510dfc78:  e0 03 1c aa   mov	x0, x28
0x2510dfc7c:  c1 87 c0 95   bl	j__CFRetain
0x2510dfc80:  60 26 00 f9   str	x0, [x19, #0x48]
0x2510dfc84:  c8 0d 80 52   mov	w8, #0x6e
0x2510dfc88:  ; loc_2510dfc88
0x2510dfc88:  68 5a 00 b9   str	w8, [x19, #0x58]
0x2510dfc8c:  99 00 00 b4   cbz	x25, loc_2510dfc9c ; ⤵ 0x10
0x2510dfc90:  e0 03 19 aa   mov	x0, x25
0x2510dfc94:  bb 87 c0 95   bl	j__CFRetain
0x2510dfc98:  02 00 00 14   b	loc_2510dfca0 ; ⤵ 0x8
0x2510dfc9c:  ; loc_2510dfc9c
0x2510dfc9c:  00 00 80 d2   mov	x0, #0
0x2510dfca0:  ; loc_2510dfca0
0x2510dfca0:  60 1a 00 f9   str	x0, [x19, #0x30]
0x2510dfca4:  94 00 00 b4   cbz	x20, loc_2510dfcb4 ; ⤵ 0x10
0x2510dfca8:  e0 03 14 aa   mov	x0, x20
0x2510dfcac:  b5 87 c0 95   bl	j__CFRetain
0x2510dfcb0:  02 00 00 14   b	loc_2510dfcb8 ; ⤵ 0x8
0x2510dfcb4:  ; loc_2510dfcb4
0x2510dfcb4:  00 00 80 d2   mov	x0, #0
0x2510dfcb8:  ; loc_2510dfcb8
0x2510dfcb8:  fa 03 13 aa   mov	x26, x19
0x2510dfcbc:  40 0f 08 f8   str	x0, [x26, #0x80]!
0x2510dfcc0:  63 02 40 f9   ldr	x3, [x19]
0x2510dfcc4:  20 86 0b b0   adrp	x0, 0x2681a4000
0x2510dfcc8:  00 e0 44 f9   ldr	x0, [x0, #0x9c0] ; _ptr.1ec141550 (libdispatch.dylib/__AUTH_CONST.__const)
0x2510dfccc:  01 00 80 d2   mov	x1, #0
0x2510dfcd0:  02 00 80 d2   mov	x2, #0
0x2510dfcd4:  5b 84 c0 95   bl	j__dispatch_source_create
0x2510dfcd8:  60 66 00 f9   str	x0, [x19, #0xc8]
0x2510dfcdc:  e8 e3 00 91   add	x8, sp, #0x38
0x2510dfce0:  30 86 0b b0   adrp	x16, 0x2681a4000
0x2510dfce4:  10 2e 45 f9   ldr	x16, [x16, #0xa58] ; _ptr.2ce3d0230 (libsystem_blocks.dylib/__DATA_DIRTY.__objc_data)
0x2510dfce8:  f1 03 08 aa   mov	x17, x8
0x2510dfcec:  31 5c ed f2   movk	x17, #0x6ae1, lsl #0x30
0x2510dfcf0:  30 0a c1 da   pacda	x16, x17
0x2510dfcf4:  f0 1f 00 f9   str	x16, [sp, #0x38]
0x2510dfcf8:  49 09 00 d0   adrp	x9, 0x251209000
0x2510dfcfc:  20 45 45 fd   ldr	d0, [x9, #0xa88]
0x2510dfd00:  e0 23 00 fd   str	d0, [sp, #0x40]
0x2510dfd04:  08 41 00 91   add	x8, x8, #0x10
0x2510dfd08:  a9 15 0c f0   adrp	x9, 0x269396000
0x2510dfd0c:  29 c1 1d 91   add	x9, x9, #0x770 ; ___block_descriptor_tmp
0x2510dfd10:  90 f3 ff f0   adrp	x16, 0x250f52000
0x2510dfd14:  10 22 21 91   add	x16, x16, #0x848 ; ___APEndpointStreamScreenCreate_block_invoke
0x2510dfd18:  10 01 c1 da   pacia	x16, x8
0x2510dfd1c:  f0 a7 04 a9   stp	x16, x9, [sp, #0x48]
0x2510dfd20:  e8 47 40 f9   ldr	x8, [sp, #0x88]
0x2510dfd24:  e8 2f 00 f9   str	x8, [sp, #0x58]
0x2510dfd28:  e1 e3 00 91   add	x1, sp, #0x38
0x2510dfd2c:  49 84 c0 95   bl	j__dispatch_source_set_event_handler
0x2510dfd30:  60 66 40 f9   ldr	x0, [x19, #0xc8]
0x2510dfd34:  01 00 80 92   mov	x1, #0xffffffffffffffff
0x2510dfd38:  02 00 80 d2   mov	x2, #0
0x2510dfd3c:  03 00 80 d2   mov	x3, #0
0x2510dfd40:  c0 87 c0 95   bl	j___dispatch_source_set_runloop_timer_4CF
0x2510dfd44:  60 66 40 f9   ldr	x0, [x19, #0xc8]
0x2510dfd48:  ba 87 c0 95   bl	j__dispatch_resume
0x2510dfd4c:  e0 03 18 aa   mov	x0, x24
0x2510dfd50:  8c 87 c0 95   bl	j__CFRetain
0x2510dfd54:  60 22 00 f9   str	x0, [x19, #0x40]
0x2510dfd58:  e0 47 40 f9   ldr	x0, [sp, #0x88]
0x2510dfd5c:  19 b6 c0 95   bl	j__CMBaseObjectGetDerivedStorage
0x2510dfd60:  f6 03 00 aa   mov	x22, x0
0x2510dfd64:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2510dfd68:  c0 00 00 b4   cbz	x0, loc_2510dfd80 ; ⤵ 0x18
0x2510dfd6c:  c1 50 12 b0   adrp	x1, 0x275af8000
0x2510dfd70:  21 80 2e 91   add	x1, x1, #0xba0 ; "EventRecorder"
0x2510dfd74:  0f 8a c0 95   bl	j__CFDictionaryGetValue
0x2510dfd78:  f8 03 00 aa   mov	x24, x0
0x2510dfd7c:  02 00 00 14   b	loc_2510dfd84 ; ⤵ 0x8
0x2510dfd80:  ; loc_2510dfd80
0x2510dfd80:  18 00 80 d2   mov	x24, #0
0x2510dfd84:  ; loc_2510dfd84
0x2510dfd84:  d4 e2 11 91   add	x20, x22, #0x478
0x2510dfd88:  c0 3e 42 f9   ldr	x0, [x22, #0x478]
0x2510dfd8c:  60 00 00 b4   cbz	x0, loc_2510dfd98 ; ⤵ 0xc
0x2510dfd90:  78 87 c0 95   bl	j__CFRelease
0x2510dfd94:  9f 02 00 f9   str	xzr, [x20]
0x2510dfd98:  ; loc_2510dfd98
0x2510dfd98:  c1 06 40 f9   ldr	x1, [x22, #0x8]
0x2510dfd9c:  00 fe a1 52   mov	w0, #0xff00000
0x2510dfda0:  82 00 80 52   mov	w2, #0x4
0x2510dfda4:  03 00 80 d2   mov	x3, #0
0x2510dfda8:  e4 03 18 aa   mov	x4, x24
0x2510dfdac:  e5 03 14 aa   mov	x5, x20
0x2510dfdb0:  f0 bf c0 95   bl	j__APSEventRecorderCreate
0x2510dfdb4:  20 b2 00 35   cbnz	w0, loc_2510e13f8 ; ⤵ 0x1644
0x2510dfdb8:  80 02 40 f9   ldr	x0, [x20]
0x2510dfdbc:  16 4b 80 52   mov	w22, #0x258
0x2510dfdc0:  d6 62 a5 72   movk	w22, #0x2b16, lsl #0x10
0x2510dfdc4:  01 fe a1 52   mov	w1, #0xff00000
0x2510dfdc8:  02 4b 80 52   mov	w2, #0x258
0x2510dfdcc:  c2 62 a5 72   movk	w2, #0x2b16, lsl #0x10
0x2510dfdd0:  e4 bf c0 95   bl	j__APSEventRecorderAddSignPostForEvent
0x2510dfdd4:  80 02 40 f9   ldr	x0, [x20]
0x2510dfdd8:  c2 02 1e 32   orr	w2, w22, #0x4
0x2510dfddc:  41 00 80 52   mov	w1, #0x2
0x2510dfde0:  01 fe a1 72   movk	w1, #0xff0, lsl #0x10
0x2510dfde4:  df bf c0 95   bl	j__APSEventRecorderAddSignPostForEvent
0x2510dfde8:  20 09 00 d0   adrp	x0, 0x251205000
0x2510dfdec:  00 08 0e 91   add	x0, x0, #0x382 ; "APEndpointStreamScreen.%{ptr}.RTC"
0x2510dfdf0:  01 00 80 d2   mov	x1, #0
0x2510dfdf4:  0b 84 c0 95   bl	j__dispatch_queue_create
0x2510dfdf8:  60 42 02 f9   str	x0, [x19, #0x480]
0x2510dfdfc:  22 7f 0b 90   adrp	x2, 0x2680c3000
0x2510dfe00:  42 c0 41 f9   ldr	x2, [x2, #0x380] ; _ptr.1e8d90d00 (CoreFoundation/__AUTH_CONST.__const)
0x2510dfe04:  e0 03 17 aa   mov	x0, x23
0x2510dfe08:  01 00 80 d2   mov	x1, #0
0x2510dfe0c:  b5 89 c0 95   bl	j__CFArrayCreateMutable
0x2510dfe10:  60 46 02 f9   str	x0, [x19, #0x488]
0x2510dfe14:  f8 47 40 f9   ldr	x24, [sp, #0x88]
0x2510dfe18:  e0 03 18 aa   mov	x0, x24
0x2510dfe1c:  e9 b5 c0 95   bl	j__CMBaseObjectGetDerivedStorage
0x2510dfe20:  f9 03 00 aa   mov	x25, x0
0x2510dfe24:  ff 5f 02 39   strb	wzr, [sp, #0x97]
0x2510dfe28:  08 60 40 39   ldrb	w8, [x0, #0x18]
0x2510dfe2c:  c8 ae 00 35   cbnz	w8, loc_2510e1404 ; ⤵ 0x15d8
0x2510dfe30:  fa 0f 00 f9   str	x26, [sp, #0x18]
0x2510dfe34:  28 67 40 39   ldrb	w8, [x25, #0x19]
0x2510dfe38:  88 a9 00 35   cbnz	w8, loc_2510e1368 ; ⤵ 0x1530
0x2510dfe3c:  ff 9b 00 b9   str	wzr, [sp, #0x98]
0x2510dfe40:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510dfe44:  88 15 0c d0   adrp	x8, 0x269391000
0x2510dfe48:  08 61 3d 91   add	x8, x8, #0xf58 ; _kAPEndpointDescriptionProperty_IsConnectedOverUSB
0x2510dfe4c:  01 01 40 f9   ldr	x1, [x8]
0x2510dfe50:  e2 63 02 91   add	x2, sp, #0x98
0x2510dfe54:  cf b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dfe58:  1f 00 00 f1   cmp	x0, #0
0x2510dfe5c:  e8 07 9f 1a   cset	w8, ne
0x2510dfe60:  28 17 06 39   strb	w8, [x25, #0x185]
0x2510dfe64:  20 1b 40 f9   ldr	x0, [x25, #0x30]
0x2510dfe68:  c0 00 00 b4   cbz	x0, loc_2510dfe80 ; ⤵ 0x18
0x2510dfe6c:  88 15 0c f0   adrp	x8, 0x269392000
0x2510dfe70:  08 a1 21 91   add	x8, x8, #0x868 ; _kAPSenderSessionProperty_ControlConnectionInterfaceLatencyHint
0x2510dfe74:  01 01 40 f9   ldr	x1, [x8]
0x2510dfe78:  e2 63 02 91   add	x2, sp, #0x98
0x2510dfe7c:  c5 b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dfe80:  ; loc_2510dfe80
0x2510dfe80:  7c 9d 0f f0   adrp	x28, 0x27048e000
0x2510dfe84:  20 a7 04 b9   str	w0, [x25, #0x4a4]
0x2510dfe88:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510dfe8c:  88 15 0c d0   adrp	x8, 0x269391000
0x2510dfe90:  08 41 3d 91   add	x8, x8, #0xf50 ; _kAPEndpointDescriptionProperty_IsCarPlay
0x2510dfe94:  01 01 40 f9   ldr	x1, [x8]
0x2510dfe98:  e2 63 02 91   add	x2, sp, #0x98
0x2510dfe9c:  bd b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dfea0:  1f 00 00 f1   cmp	x0, #0
0x2510dfea4:  e8 07 9f 1a   cset	w8, ne
0x2510dfea8:  28 0f 06 39   strb	w8, [x25, #0x183]
0x2510dfeac:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510dfeb0:  88 15 0c d0   adrp	x8, 0x269391000
0x2510dfeb4:  08 81 3d 91   add	x8, x8, #0xf60 ; _kAPEndpointDescriptionProperty_IsScreenDemoMode
0x2510dfeb8:  01 01 40 f9   ldr	x1, [x8]
0x2510dfebc:  02 00 80 d2   mov	x2, #0
0x2510dfec0:  b4 b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dfec4:  1f 00 00 f1   cmp	x0, #0
0x2510dfec8:  e8 07 9f 1a   cset	w8, ne
0x2510dfecc:  28 43 06 39   strb	w8, [x25, #0x190]
0x2510dfed0:  21 03 43 a9   ldp	x1, x0, [x25, #0x30]
0x2510dfed4:  08 b1 00 94   bl	_APDemoManagerIsDemoSession
0x2510dfed8:  20 47 06 39   strb	w0, [x25, #0x191]
0x2510dfedc:  3f 4b 06 39   strb	wzr, [x25, #0x192]
0x2510dfee0:  28 0f 46 39   ldrb	w8, [x25, #0x183]
0x2510dfee4:  88 00 00 34   cbz	w8, loc_2510dfef4 ; ⤵ 0x10
0x2510dfee8:  20 17 46 39   ldrb	w0, [x25, #0x185]
0x2510dfeec:  f9 be c0 95   bl	j__APSCarPlayScreenLatencyMs
0x2510dfef0:  03 00 00 14   b	loc_2510dfefc ; ⤵ 0xc
0x2510dfef4:  ; loc_2510dfef4
0x2510dfef4:  20 a7 44 b9   ldr	w0, [x25, #0x4a4]
0x2510dfef8:  fa b1 c0 95   bl	j__APSScreenLatencyMs
0x2510dfefc:  ; loc_2510dfefc
0x2510dfefc:  20 9f 04 b9   str	w0, [x25, #0x49c]
0x2510dff00:  f0 c1 c0 95   bl	j__APSScreenMediaPresentationLatencyMs
0x2510dff04:  28 9f 44 b9   ldr	w8, [x25, #0x49c]
0x2510dff08:  1f 00 08 6b   cmp	w0, w8
0x2510dff0c:  09 c0 88 1a   csel	w9, w0, w8, gt
0x2510dff10:  29 a3 04 b9   str	w9, [x25, #0x4a0]
0x2510dff14:  8a 33 4b b9   ldr	w10, [x28, #0xb30]
0x2510dff18:  5f a1 00 71   cmp	w10, #0x28
0x2510dff1c:  0c 03 00 54   b.gt	loc_2510dff7c ; ⤵ 0x60
0x2510dff20:  5f 05 00 31   cmn	w10, #0x1
0x2510dff24:  e0 01 00 54   b.eq	loc_2510dff60 ; ⤵ 0x3c
0x2510dff28:  ; loc_2510dff28
0x2510dff28:  2a 17 40 f9   ldr	x10, [x25, #0x28]
0x2510dff2c:  e8 a7 00 a9   stp	x8, x9, [sp, #0x8]
0x2510dff30:  ea 03 00 f9   str	x10, [sp]
0x2510dff34:  60 9d 0f f0   adrp	x0, 0x27048e000
0x2510dff38:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510dff3c:  41 09 00 90   adrp	x1, 0x251207000
0x2510dff40:  21 14 0e 91   add	x1, x1, #0x385 ; "void screenstream_init(StreamScreenStorage *)"
0x2510dff44:  43 09 00 90   adrp	x3, 0x251207000
0x2510dff48:  63 cc 0e 91   add	x3, x3, #0x3b3 ; "%@ initial latency settings are screen latency: %d, media presentation mode latency: %d\n"
0x2510dff4c:  02 05 80 52   mov	w2, #0x28
0x2510dff50:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510dff54:  eb 83 c0 95   bl	j__LogPrintF
0x2510dff58:  28 9f 44 b9   ldr	w8, [x25, #0x49c]
0x2510dff5c:  08 00 00 14   b	loc_2510dff7c ; ⤵ 0x20
0x2510dff60:  ; loc_2510dff60
0x2510dff60:  60 9d 0f f0   adrp	x0, 0x27048e000
0x2510dff64:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510dff68:  01 05 80 52   mov	w1, #0x28
0x2510dff6c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510dff70:  94 83 c0 95   bl	j___LogCategory_Initialize
0x2510dff74:  28 9f 44 b9   ldr	w8, [x25, #0x49c]
0x2510dff78:  a0 ad 00 35   cbnz	w0, loc_2510e152c ; ⤵ 0x15b4
0x2510dff7c:  ; loc_2510dff7c
0x2510dff7c:  00 7d 40 93   sxtw	x0, w8
0x2510dff80:  a4 b8 c0 95   bl	j__MillisecondsToUpTicks
0x2510dff84:  20 df 01 f9   str	x0, [x25, #0x3b8]
0x2510dff88:  3f c3 03 b9   str	wzr, [x25, #0x3c0]
0x2510dff8c:  3f 13 0f 39   strb	wzr, [x25, #0x3c4]
0x2510dff90:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510dff94:  88 15 0c f0   adrp	x8, 0x269392000
0x2510dff98:  08 41 03 91   add	x8, x8, #0xd0 ; _kAPEndpointDescriptionProperty_SupportsStatsAsBody
0x2510dff9c:  01 01 40 f9   ldr	x1, [x8]
0x2510dffa0:  02 00 80 d2   mov	x2, #0
0x2510dffa4:  7b b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dffa8:  1f 00 00 f1   cmp	x0, #0
0x2510dffac:  e8 07 9f 1a   cset	w8, ne
0x2510dffb0:  28 17 0f 39   strb	w8, [x25, #0x3c5]
0x2510dffb4:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510dffb8:  88 15 0c f0   adrp	x8, 0x269392000
0x2510dffbc:  08 c1 02 91   add	x8, x8, #0xb0 ; _kAPEndpointDescriptionProperty_SupportsScreenMultiCodec
0x2510dffc0:  01 01 40 f9   ldr	x1, [x8]
0x2510dffc4:  02 00 80 d2   mov	x2, #0
0x2510dffc8:  72 b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dffcc:  1f 00 00 f1   cmp	x0, #0
0x2510dffd0:  e8 07 9f 1a   cset	w8, ne
0x2510dffd4:  28 1f 0f 39   strb	w8, [x25, #0x3c7]
0x2510dffd8:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510dffdc:  88 15 0c d0   adrp	x8, 0x269391000
0x2510dffe0:  08 e1 39 91   add	x8, x8, #0xe78 ; _kAPEndpointDescriptionProperty_ForwardFrameUserData
0x2510dffe4:  01 01 40 f9   ldr	x1, [x8]
0x2510dffe8:  02 00 80 d2   mov	x2, #0
0x2510dffec:  69 b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510dfff0:  1f 00 00 f1   cmp	x0, #0
0x2510dfff4:  e8 07 9f 1a   cset	w8, ne
0x2510dfff8:  28 2b 0f 39   strb	w8, [x25, #0x3ca]
0x2510dfffc:  89 33 4b b9   ldr	w9, [x28, #0xb30]
0x2510e0000:  3f a1 00 71   cmp	w9, #0x28
0x2510e0004:  6c 03 00 54   b.gt	loc_2510e0070 ; ⤵ 0x6c
0x2510e0008:  3f 05 00 31   cmn	w9, #0x1
0x2510e000c:  60 02 00 54   b.eq	loc_2510e0058 ; ⤵ 0x4c
0x2510e0010:  ; loc_2510e0010
0x2510e0010:  29 17 40 f9   ldr	x9, [x25, #0x28]
0x2510e0014:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e0018:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e001c:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e0020:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e0024:  1f 01 00 71   cmp	w8, #0
0x2510e0028:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e002c:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e0030:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0034:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0038:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e003c:  21 14 0e 91   add	x1, x1, #0x385 ; "void screenstream_init(StreamScreenStorage *)"
0x2510e0040:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0044:  63 30 10 91   add	x3, x3, #0x40c ; "%@ forward frame user data: %s\n"
0x2510e0048:  02 05 80 52   mov	w2, #0x28
0x2510e004c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0050:  ac 83 c0 95   bl	j__LogPrintF
0x2510e0054:  07 00 00 14   b	loc_2510e0070 ; ⤵ 0x1c
0x2510e0058:  ; loc_2510e0058
0x2510e0058:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e005c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0060:  01 05 80 52   mov	w1, #0x28
0x2510e0064:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0068:  56 83 c0 95   bl	j___LogCategory_Initialize
0x2510e006c:  40 a6 00 35   cbnz	w0, loc_2510e1534 ; ⤵ 0x14c8
0x2510e0070:  ; loc_2510e0070
0x2510e0070:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510e0074:  88 15 0c b0   adrp	x8, 0x269391000
0x2510e0078:  08 01 3a 91   add	x8, x8, #0xe80 ; _kAPEndpointDescriptionProperty_ForwardCryptorSubsampleAuxData
0x2510e007c:  01 01 40 f9   ldr	x1, [x8]
0x2510e0080:  02 00 80 d2   mov	x2, #0
0x2510e0084:  43 b0 c0 95   bl	j__APSGetFBOPropertyInt64
0x2510e0088:  1f 00 00 f1   cmp	x0, #0
0x2510e008c:  e8 07 9f 1a   cset	w8, ne
0x2510e0090:  28 2f 0f 39   strb	w8, [x25, #0x3cb]
0x2510e0094:  89 33 4b b9   ldr	w9, [x28, #0xb30]
0x2510e0098:  3f a1 00 71   cmp	w9, #0x28
0x2510e009c:  6c 03 00 54   b.gt	loc_2510e0108 ; ⤵ 0x6c
0x2510e00a0:  3f 05 00 31   cmn	w9, #0x1
0x2510e00a4:  60 02 00 54   b.eq	loc_2510e00f0 ; ⤵ 0x4c
0x2510e00a8:  ; loc_2510e00a8
0x2510e00a8:  29 17 40 f9   ldr	x9, [x25, #0x28]
0x2510e00ac:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e00b0:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e00b4:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e00b8:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e00bc:  1f 01 00 71   cmp	w8, #0
0x2510e00c0:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e00c4:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e00c8:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e00cc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e00d0:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e00d4:  21 14 0e 91   add	x1, x1, #0x385 ; "void screenstream_init(StreamScreenStorage *)"
0x2510e00d8:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e00dc:  63 b0 10 91   add	x3, x3, #0x42c ; "%@ forward cryptor aux data: %s\n"
0x2510e00e0:  02 05 80 52   mov	w2, #0x28
0x2510e00e4:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e00e8:  86 83 c0 95   bl	j__LogPrintF
0x2510e00ec:  07 00 00 14   b	loc_2510e0108 ; ⤵ 0x1c
0x2510e00f0:  ; loc_2510e00f0
0x2510e00f0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e00f4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e00f8:  01 05 80 52   mov	w1, #0x28
0x2510e00fc:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0100:  30 83 c0 95   bl	j___LogCategory_Initialize
0x2510e0104:  c0 a1 00 35   cbnz	w0, loc_2510e153c ; ⤵ 0x1438
0x2510e0108:  ; loc_2510e0108
0x2510e0108:  fa 03 19 aa   mov	x26, x25
0x2510e010c:  48 8f 42 f8   ldr	x8, [x26, #0x28]!
0x2510e0110:  5f 83 0e 39   strb	wzr, [x26, #0x3a0]
0x2510e0114:  48 c7 01 f9   str	x8, [x26, #0x388]
0x2510e0118:  48 09 00 d0   adrp	x8, 0x25120a000
0x2510e011c:  00 59 c1 3d   ldr	q0, [x8, #0x560]
0x2510e0120:  08 7f 0b f0   adrp	x8, 0x2680c3000
0x2510e0124:  08 a9 43 f9   ldr	x8, [x8, #0x750] ; _ptr.1842e4d50 (CoreGraphics/__TEXT.__const)
0x2510e0128:  01 01 c0 3d   ldr	q1, [x8]
0x2510e012c:  41 87 06 ad   stp	q1, q1, [x26, #0xd0]
0x2510e0130:  40 87 05 ad   stp	q0, q1, [x26, #0xb0]
0x2510e0134:  40 0b 40 f9   ldr	x0, [x26, #0x10]
0x2510e0138:  e5 84 f9 97   bl	_APEndpointDescriptionGetCMBaseObject
0x2510e013c:  f4 03 00 aa   mov	x20, x0
0x2510e0140:  48 ae c0 95   bl	j__CMBaseObjectGetVTable
0x2510e0144:  10 8c 40 f8   ldr	x16, [x0, #0x8]!
0x2510e0148:  f1 03 00 aa   mov	x17, x0
0x2510e014c:  31 22 f7 f2   movk	x17, #0xb911, lsl #0x30
0x2510e0150:  30 1a c1 da   autda	x16, x17
0x2510e0154:  09 0e 43 f8   ldr	x9, [x16, #0x30]!
0x2510e0158:  a9 02 00 b4   cbz	x9, loc_2510e01ac ; ⤵ 0x54
0x2510e015c:  e8 03 10 aa   mov	x8, x16
0x2510e0160:  a8 e4 f2 f2   movk	x8, #0x9725, lsl #0x30
0x2510e0164:  f0 03 09 aa   mov	x16, x9
0x2510e0168:  f1 03 08 aa   mov	x17, x8
0x2510e016c:  30 12 c1 da   autia	x16, x17
0x2510e0170:  f1 03 10 aa   mov	x17, x16
0x2510e0174:  f1 43 c1 da   xpaci	x17
0x2510e0178:  1f 02 11 eb   cmp	x16, x17
0x2510e017c:  40 00 00 54   b.eq	loc_2510e0184 ; ⤵ 0x8
0x2510e0180:  00 8e 38 d4   brk	#0xc470
0x2510e0184:  ; loc_2510e0184
0x2510e0184:  f0 23 c1 da   paciza	x16
0x2510e0188:  30 01 00 b4   cbz	x16, loc_2510e01ac ; ⤵ 0x24
0x2510e018c:  8a 15 0c b0   adrp	x10, 0x269391000
0x2510e0190:  4a 01 3c 91   add	x10, x10, #0xf00 ; _kAPEndpointDescriptionProperty_ReceiverHDRCapability
0x2510e0194:  41 01 40 f9   ldr	x1, [x10]
0x2510e0198:  43 03 04 91   add	x3, x26, #0x100
0x2510e019c:  e0 03 14 aa   mov	x0, x20
0x2510e01a0:  e2 03 1b aa   mov	x2, x27
0x2510e01a4:  28 09 3f d7   blraa	x9, x8
0x2510e01a8:  02 00 00 14   b	loc_2510e01b0 ; ⤵ 0x8
0x2510e01ac:  ; loc_2510e01ac
0x2510e01ac:  a0 3d 86 12   mov	w0, #0xffffce12
0x2510e01b0:  ; loc_2510e01b0
0x2510e01b0:  e0 9b 00 b9   str	w0, [sp, #0x98]
0x2510e01b4:  3f cb 05 39   strb	wzr, [x25, #0x172]
0x2510e01b8:  3f e3 02 79   strh	wzr, [x25, #0x170]
0x2510e01bc:  34 00 80 52   mov	w20, #0x1
0x2510e01c0:  34 d7 05 39   strb	w20, [x25, #0x175]
0x2510e01c4:  c0 50 12 f0   adrp	x0, 0x275afb000
0x2510e01c8:  00 00 29 91   add	x0, x0, #0xa40 ; "disableOption1ForMediaPresentation"
0x2510e01cc:  01 00 80 d2   mov	x1, #0
0x2510e01d0:  58 b1 c0 95   bl	j__APSSettingsGetInt64
0x2510e01d4:  1f 00 00 f1   cmp	x0, #0
0x2510e01d8:  e8 17 9f 1a   cset	w8, eq
0x2510e01dc:  28 db 05 39   strb	w8, [x25, #0x176]
0x2510e01e0:  3f 43 05 39   strb	wzr, [x25, #0x150]
0x2510e01e4:  34 d3 05 39   strb	w20, [x25, #0x174]
0x2510e01e8:  34 df 05 91   add	x20, x25, #0x177
0x2510e01ec:  20 24 00 0f   movi	v0.2s, #0x1, lsl #0x8
0x2510e01f0:  80 02 00 bd   str	s0, [x20]
0x2510e01f4:  28 a3 44 b9   ldr	w8, [x25, #0x4a0]
0x2510e01f8:  28 7f 01 b9   str	w8, [x25, #0x17c]
0x2510e01fc:  20 1f 40 f9   ldr	x0, [x25, #0x38]
0x2510e0200:  88 15 0c b0   adrp	x8, 0x269391000
0x2510e0204:  08 c1 39 91   add	x8, x8, #0xe70 ; _kAPEndpointDescriptionProperty_CanRecordScreenStream
0x2510e0208:  01 01 40 f9   ldr	x1, [x8]
0x2510e020c:  02 00 80 d2   mov	x2, #0
0x2510e0210:  e0 af c0 95   bl	j__APSGetFBOPropertyInt64
0x2510e0214:  1f 00 00 f1   cmp	x0, #0
0x2510e0218:  e8 07 9f 1a   cset	w8, ne
0x2510e021c:  28 cf 05 39   strb	w8, [x25, #0x173]
0x2510e0220:  3f b7 00 f9   str	xzr, [x25, #0x168]
0x2510e0224:  3f 13 06 39   strb	wzr, [x25, #0x184]
0x2510e0228:  28 0f 46 39   ldrb	w8, [x25, #0x183]
0x2510e022c:  1f 01 00 71   cmp	w8, #0
0x2510e0230:  e8 17 9f 1a   cset	w8, eq
0x2510e0234:  28 1b 0f 39   strb	w8, [x25, #0x3c6]
0x2510e0238:  48 09 00 d0   adrp	x8, 0x25120a000
0x2510e023c:  00 8d 42 fd   ldr	d0, [x8, #0x518]
0x2510e0240:  20 c7 00 fd   str	d0, [x25, #0x188]
0x2510e0244:  00 51 12 b0   adrp	x0, 0x275b01000
0x2510e0248:  00 00 30 91   add	x0, x0, #0xc00 ; "senderMessageTimeoutSecs"
0x2510e024c:  01 00 80 d2   mov	x1, #0
0x2510e0250:  38 b1 c0 95   bl	j__APSSettingsGetInt64
0x2510e0254:  20 9b 04 b9   str	w0, [x25, #0x498]
0x2510e0258:  3f 0b 06 39   strb	wzr, [x25, #0x182]
0x2510e025c:  48 09 00 d0   adrp	x8, 0x25120a000
0x2510e0260:  00 5d c1 3d   ldr	q0, [x8, #0x570]
0x2510e0264:  20 6b 80 3d   str	q0, [x25, #0x1a0]
0x2510e0268:  08 5a c2 d2   mov	x8, #0x12d000000000
0x2510e026c:  68 30 e8 f2   movk	x8, #0x4183, lsl #0x30
0x2510e0270:  28 db 00 f9   str	x8, [x25, #0x1b0]
0x2510e0274:  48 09 00 d0   adrp	x8, 0x25120a000
0x2510e0278:  00 61 c1 3d   ldr	q0, [x8, #0x580]
0x2510e027c:  48 09 00 d0   adrp	x8, 0x25120a000
0x2510e0280:  01 65 c1 3d   ldr	q1, [x8, #0x590]
0x2510e0284:  20 07 0e ad   stp	q0, q1, [x25, #0x1c0]
0x2510e0288:  3f f3 00 f9   str	xzr, [x25, #0x1e0]
0x2510e028c:  1d b9 c0 95   bl	j__UpTicksPerSecond
0x2510e0290:  08 08 00 8b   add	x8, x0, x0, lsl #0x2
0x2510e0294:  28 f7 00 f9   str	x8, [x25, #0x1e8]
0x2510e0298:  ff 9b 00 b9   str	wzr, [sp, #0x98]
0x2510e029c:  37 a7 44 b9   ldr	w23, [x25, #0x4a4]
0x2510e02a0:  22 c3 43 b9   ldr	w2, [x25, #0x3c0]
0x2510e02a4:  c0 50 12 f0   adrp	x0, 0x275afb000
0x2510e02a8:  00 80 2b 91   add	x0, x0, #0xae0 ; "remoteLog"
0x2510e02ac:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e02b0:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e02b4:  6f b6 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e02b8:  20 c3 03 b9   str	w0, [x25, #0x3c0]
0x2510e02bc:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e02c0:  1f a1 00 71   cmp	w8, #0x28
0x2510e02c4:  ac 02 00 54   b.gt	loc_2510e0318 ; ⤵ 0x54
0x2510e02c8:  1f 05 00 31   cmn	w8, #0x1
0x2510e02cc:  a0 01 00 54   b.eq	loc_2510e0300 ; ⤵ 0x34
0x2510e02d0:  ; loc_2510e02d0
0x2510e02d0:  48 03 40 f9   ldr	x8, [x26]
0x2510e02d4:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e02d8:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e02dc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e02e0:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e02e4:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e02e8:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e02ec:  63 10 12 91   add	x3, x3, #0x484 ; "%@ remote log level: %d\n"
0x2510e02f0:  02 05 80 52   mov	w2, #0x28
0x2510e02f4:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e02f8:  02 83 c0 95   bl	j__LogPrintF
0x2510e02fc:  07 00 00 14   b	loc_2510e0318 ; ⤵ 0x1c
0x2510e0300:  ; loc_2510e0300
0x2510e0300:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0304:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0308:  01 05 80 52   mov	w1, #0x28
0x2510e030c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0310:  ac 82 c0 95   bl	j___LogCategory_Initialize
0x2510e0314:  80 91 00 35   cbnz	w0, loc_2510e1544 ; ⤵ 0x1230
0x2510e0318:  ; loc_2510e0318
0x2510e0318:  22 13 4f 39   ldrb	w2, [x25, #0x3c4]
0x2510e031c:  c0 50 12 f0   adrp	x0, 0x275afb000
0x2510e0320:  00 00 2c 91   add	x0, x0, #0xb00 ; "showHUD"
0x2510e0324:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0328:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e032c:  51 b6 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0330:  20 13 0f 39   strb	w0, [x25, #0x3c4]
0x2510e0334:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0338:  1f a1 00 71   cmp	w8, #0x28
0x2510e033c:  8c 03 00 54   b.gt	loc_2510e03ac ; ⤵ 0x70
0x2510e0340:  1f 05 00 31   cmn	w8, #0x1
0x2510e0344:  80 02 00 54   b.eq	loc_2510e0394 ; ⤵ 0x50
0x2510e0348:  08 1c 00 12   and	w8, w0, #0xff
0x2510e034c:  ; loc_2510e034c
0x2510e034c:  49 03 40 f9   ldr	x9, [x26]
0x2510e0350:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e0354:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e0358:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e035c:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e0360:  1f 01 00 71   cmp	w8, #0
0x2510e0364:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e0368:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e036c:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0370:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0374:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0378:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e037c:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0380:  63 74 12 91   add	x3, x3, #0x49d ; "%@ show HUD: %s\n"
0x2510e0384:  02 05 80 52   mov	w2, #0x28
0x2510e0388:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e038c:  dd 82 c0 95   bl	j__LogPrintF
0x2510e0390:  07 00 00 14   b	loc_2510e03ac ; ⤵ 0x1c
0x2510e0394:  ; loc_2510e0394
0x2510e0394:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0398:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e039c:  01 05 80 52   mov	w1, #0x28
0x2510e03a0:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e03a4:  87 82 c0 95   bl	j___LogCategory_Initialize
0x2510e03a8:  20 8d 00 35   cbnz	w0, loc_2510e154c ; ⤵ 0x11a4
0x2510e03ac:  ; loc_2510e03ac
0x2510e03ac:  22 23 4f 39   ldrb	w2, [x25, #0x3c8]
0x2510e03b0:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e03b4:  00 80 1d 91   add	x0, x0, #0x760 ; "forceMultiCodec"
0x2510e03b8:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e03bc:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e03c0:  2c b6 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e03c4:  20 23 0f 39   strb	w0, [x25, #0x3c8]
0x2510e03c8:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e03cc:  1f a1 00 71   cmp	w8, #0x28
0x2510e03d0:  8c 03 00 54   b.gt	loc_2510e0440 ; ⤵ 0x70
0x2510e03d4:  1f 05 00 31   cmn	w8, #0x1
0x2510e03d8:  80 02 00 54   b.eq	loc_2510e0428 ; ⤵ 0x50
0x2510e03dc:  08 1c 00 12   and	w8, w0, #0xff
0x2510e03e0:  ; loc_2510e03e0
0x2510e03e0:  49 03 40 f9   ldr	x9, [x26]
0x2510e03e4:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e03e8:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e03ec:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e03f0:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e03f4:  1f 01 00 71   cmp	w8, #0
0x2510e03f8:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e03fc:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e0400:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0404:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0408:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e040c:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0410:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0414:  63 f8 12 91   add	x3, x3, #0x4be ; "%@ force multi codec: %s\n"
0x2510e0418:  02 05 80 52   mov	w2, #0x28
0x2510e041c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0420:  b8 82 c0 95   bl	j__LogPrintF
0x2510e0424:  07 00 00 14   b	loc_2510e0440 ; ⤵ 0x1c
0x2510e0428:  ; loc_2510e0428
0x2510e0428:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e042c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0430:  01 05 80 52   mov	w1, #0x28
0x2510e0434:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0438:  62 82 c0 95   bl	j___LogCategory_Initialize
0x2510e043c:  c0 88 00 35   cbnz	w0, loc_2510e1554 ; ⤵ 0x1118
0x2510e0440:  ; loc_2510e0440
0x2510e0440:  58 b7 c0 95   bl	j__IsAppleInternalBuild
0x2510e0444:  e2 03 00 aa   mov	x2, x0
0x2510e0448:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e044c:  00 00 1e 91   add	x0, x0, #0x780 ; "logScreenHistograms"
0x2510e0450:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0454:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0458:  06 b6 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e045c:  20 27 0f 39   strb	w0, [x25, #0x3c9]
0x2510e0460:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0464:  1f a1 00 71   cmp	w8, #0x28
0x2510e0468:  8c 03 00 54   b.gt	loc_2510e04d8 ; ⤵ 0x70
0x2510e046c:  1f 05 00 31   cmn	w8, #0x1
0x2510e0470:  80 02 00 54   b.eq	loc_2510e04c0 ; ⤵ 0x50
0x2510e0474:  08 1c 00 12   and	w8, w0, #0xff
0x2510e0478:  ; loc_2510e0478
0x2510e0478:  49 03 40 f9   ldr	x9, [x26]
0x2510e047c:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e0480:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e0484:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e0488:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e048c:  1f 01 00 71   cmp	w8, #0
0x2510e0490:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e0494:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e0498:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e049c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e04a0:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e04a4:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e04a8:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e04ac:  63 b0 13 91   add	x3, x3, #0x4ec ; "%@ log histograms: %s\n"
0x2510e04b0:  02 05 80 52   mov	w2, #0x28
0x2510e04b4:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e04b8:  92 82 c0 95   bl	j__LogPrintF
0x2510e04bc:  07 00 00 14   b	loc_2510e04d8 ; ⤵ 0x1c
0x2510e04c0:  ; loc_2510e04c0
0x2510e04c0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e04c4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e04c8:  01 05 80 52   mov	w1, #0x28
0x2510e04cc:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e04d0:  3c 82 c0 95   bl	j___LogCategory_Initialize
0x2510e04d4:  40 84 00 35   cbnz	w0, loc_2510e155c ; ⤵ 0x1088
0x2510e04d8:  ; loc_2510e04d8
0x2510e04d8:  22 d3 45 39   ldrb	w2, [x25, #0x174]
0x2510e04dc:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e04e0:  00 80 1e 91   add	x0, x0, #0x7a0 ; "screenEnableMaxDisplaySize"
0x2510e04e4:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e04e8:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e04ec:  e1 b5 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e04f0:  20 d3 05 39   strb	w0, [x25, #0x174]
0x2510e04f4:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e04f8:  1f a1 00 71   cmp	w8, #0x28
0x2510e04fc:  8c 03 00 54   b.gt	loc_2510e056c ; ⤵ 0x70
0x2510e0500:  1f 05 00 31   cmn	w8, #0x1
0x2510e0504:  80 02 00 54   b.eq	loc_2510e0554 ; ⤵ 0x50
0x2510e0508:  08 1c 00 12   and	w8, w0, #0xff
0x2510e050c:  ; loc_2510e050c
0x2510e050c:  49 03 40 f9   ldr	x9, [x26]
0x2510e0510:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e0514:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e0518:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e051c:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e0520:  1f 01 00 71   cmp	w8, #0
0x2510e0524:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e0528:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e052c:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0530:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0534:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0538:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e053c:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0540:  63 78 14 91   add	x3, x3, #0x51e ; "%@ max display size enabled: %s\n"
0x2510e0544:  02 05 80 52   mov	w2, #0x28
0x2510e0548:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e054c:  6d 82 c0 95   bl	j__LogPrintF
0x2510e0550:  07 00 00 14   b	loc_2510e056c ; ⤵ 0x1c
0x2510e0554:  ; loc_2510e0554
0x2510e0554:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0558:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e055c:  01 05 80 52   mov	w1, #0x28
0x2510e0560:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0564:  17 82 c0 95   bl	j___LogCategory_Initialize
0x2510e0568:  e0 7f 00 35   cbnz	w0, loc_2510e1564 ; ⤵ 0xffc
0x2510e056c:  ; loc_2510e056c
0x2510e056c:  22 d7 45 39   ldrb	w2, [x25, #0x175]
0x2510e0570:  c0 50 12 f0   adrp	x0, 0x275afb000
0x2510e0574:  00 80 2a 91   add	x0, x0, #0xaa0 ; "screenEnableHDR"
0x2510e0578:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e057c:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0580:  bc b5 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0584:  20 d7 05 39   strb	w0, [x25, #0x175]
0x2510e0588:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e058c:  1f a1 00 71   cmp	w8, #0x28
0x2510e0590:  8c 03 00 54   b.gt	loc_2510e0600 ; ⤵ 0x70
0x2510e0594:  1f 05 00 31   cmn	w8, #0x1
0x2510e0598:  80 02 00 54   b.eq	loc_2510e05e8 ; ⤵ 0x50
0x2510e059c:  08 1c 00 12   and	w8, w0, #0xff
0x2510e05a0:  ; loc_2510e05a0
0x2510e05a0:  49 03 40 f9   ldr	x9, [x26]
0x2510e05a4:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e05a8:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e05ac:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e05b0:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e05b4:  1f 01 00 71   cmp	w8, #0
0x2510e05b8:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e05bc:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e05c0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e05c4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e05c8:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e05cc:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e05d0:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e05d4:  63 fc 14 91   add	x3, x3, #0x53f ; "%@ HDR enabled: %s\n"
0x2510e05d8:  02 05 80 52   mov	w2, #0x28
0x2510e05dc:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e05e0:  48 82 c0 95   bl	j__LogPrintF
0x2510e05e4:  07 00 00 14   b	loc_2510e0600 ; ⤵ 0x1c
0x2510e05e8:  ; loc_2510e05e8
0x2510e05e8:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e05ec:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e05f0:  01 05 80 52   mov	w1, #0x28
0x2510e05f4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e05f8:  f2 81 c0 95   bl	j___LogCategory_Initialize
0x2510e05fc:  80 7b 00 35   cbnz	w0, loc_2510e156c ; ⤵ 0xf70
0x2510e0600:  ; loc_2510e0600
0x2510e0600:  22 db 45 39   ldrb	w2, [x25, #0x176]
0x2510e0604:  c0 50 12 f0   adrp	x0, 0x275afb000
0x2510e0608:  00 00 2b 91   add	x0, x0, #0xac0 ; "screenEnableDoVi"
0x2510e060c:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0610:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0614:  97 b5 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0618:  20 db 05 39   strb	w0, [x25, #0x176]
0x2510e061c:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0620:  1f a1 00 71   cmp	w8, #0x28
0x2510e0624:  8c 03 00 54   b.gt	loc_2510e0694 ; ⤵ 0x70
0x2510e0628:  1f 05 00 31   cmn	w8, #0x1
0x2510e062c:  80 02 00 54   b.eq	loc_2510e067c ; ⤵ 0x50
0x2510e0630:  08 1c 00 12   and	w8, w0, #0xff
0x2510e0634:  ; loc_2510e0634
0x2510e0634:  49 03 40 f9   ldr	x9, [x26]
0x2510e0638:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e063c:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e0640:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e0644:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e0648:  1f 01 00 71   cmp	w8, #0
0x2510e064c:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e0650:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e0654:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0658:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e065c:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0660:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0664:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0668:  63 4c 15 91   add	x3, x3, #0x553 ; "%@ DoVi enabled: %s\n"
0x2510e066c:  02 05 80 52   mov	w2, #0x28
0x2510e0670:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0674:  23 82 c0 95   bl	j__LogPrintF
0x2510e0678:  07 00 00 14   b	loc_2510e0694 ; ⤵ 0x1c
0x2510e067c:  ; loc_2510e067c
0x2510e067c:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0680:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0684:  01 05 80 52   mov	w1, #0x28
0x2510e0688:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e068c:  cd 81 c0 95   bl	j___LogCategory_Initialize
0x2510e0690:  20 77 00 35   cbnz	w0, loc_2510e1574 ; ⤵ 0xee4
0x2510e0694:  ; loc_2510e0694
0x2510e0694:  20 e3 40 fd   ldr	d0, [x25, #0x1c0]
0x2510e0698:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e069c:  00 00 1f 91   add	x0, x0, #0x7c0 ; "encoderBWFixed"
0x2510e06a0:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e06a4:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e06a8:  6e b5 c0 95   bl	j__FigGetCFPreferenceDoubleWithDefault
0x2510e06ac:  20 e3 00 fd   str	d0, [x25, #0x1c0]
0x2510e06b0:  08 20 60 1e   fcmp	d0, #0.00000000
0x2510e06b4:  2d 03 00 54   b.le	loc_2510e0718 ; ⤵ 0x64
0x2510e06b8:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e06bc:  1f a1 00 71   cmp	w8, #0x28
0x2510e06c0:  cc 02 00 54   b.gt	loc_2510e0718 ; ⤵ 0x58
0x2510e06c4:  1f 05 00 31   cmn	w8, #0x1
0x2510e06c8:  c0 01 00 54   b.eq	loc_2510e0700 ; ⤵ 0x38
0x2510e06cc:  ; loc_2510e06cc
0x2510e06cc:  48 03 40 f9   ldr	x8, [x26]
0x2510e06d0:  e0 07 00 fd   str	d0, [sp, #0x8]
0x2510e06d4:  e8 03 00 f9   str	x8, [sp]
0x2510e06d8:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e06dc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e06e0:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e06e4:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e06e8:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e06ec:  63 dc 15 91   add	x3, x3, #0x577 ; "%@ fixed encoder bandwidth: %f bps\n"
0x2510e06f0:  02 05 80 52   mov	w2, #0x28
0x2510e06f4:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e06f8:  02 82 c0 95   bl	j__LogPrintF
0x2510e06fc:  07 00 00 14   b	loc_2510e0718 ; ⤵ 0x1c
0x2510e0700:  ; loc_2510e0700
0x2510e0700:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0704:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0708:  01 05 80 52   mov	w1, #0x28
0x2510e070c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0710:  ac 81 c0 95   bl	j___LogCategory_Initialize
0x2510e0714:  c0 74 00 35   cbnz	w0, loc_2510e15ac ; ⤵ 0xe98
0x2510e0718:  ; loc_2510e0718
0x2510e0718:  22 d7 41 b9   ldr	w2, [x25, #0x1d4]
0x2510e071c:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e0720:  00 80 1f 91   add	x0, x0, #0x7e0 ; "encoderFPSFixed"
0x2510e0724:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0728:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e072c:  51 b5 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0730:  20 d7 01 b9   str	w0, [x25, #0x1d4]
0x2510e0734:  1f 04 00 71   cmp	w0, #0x1
0x2510e0738:  0b 03 00 54   b.lt	loc_2510e0798 ; ⤵ 0x60
0x2510e073c:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0740:  1f a1 00 71   cmp	w8, #0x28
0x2510e0744:  ac 02 00 54   b.gt	loc_2510e0798 ; ⤵ 0x54
0x2510e0748:  1f 05 00 31   cmn	w8, #0x1
0x2510e074c:  a0 01 00 54   b.eq	loc_2510e0780 ; ⤵ 0x34
0x2510e0750:  ; loc_2510e0750
0x2510e0750:  48 03 40 f9   ldr	x8, [x26]
0x2510e0754:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e0758:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e075c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0760:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0764:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0768:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e076c:  63 ac 16 91   add	x3, x3, #0x5ab ; "%@ fixed encoder FPS: %d\n"
0x2510e0770:  02 05 80 52   mov	w2, #0x28
0x2510e0774:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0778:  e2 81 c0 95   bl	j__LogPrintF
0x2510e077c:  07 00 00 14   b	loc_2510e0798 ; ⤵ 0x1c
0x2510e0780:  ; loc_2510e0780
0x2510e0780:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0784:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0788:  01 05 80 52   mov	w1, #0x28
0x2510e078c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0790:  8c 81 c0 95   bl	j___LogCategory_Initialize
0x2510e0794:  00 70 00 35   cbnz	w0, loc_2510e1594 ; ⤵ 0xe00
0x2510e0798:  ; loc_2510e0798
0x2510e0798:  22 d3 41 b9   ldr	w2, [x25, #0x1d0]
0x2510e079c:  e0 50 12 d0   adrp	x0, 0x275afe000
0x2510e07a0:  00 00 12 91   add	x0, x0, #0x480 ; "maxFPS"
0x2510e07a4:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e07a8:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e07ac:  31 b5 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e07b0:  20 d3 01 b9   str	w0, [x25, #0x1d0]
0x2510e07b4:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e07b8:  1f a1 00 71   cmp	w8, #0x28
0x2510e07bc:  0c 03 00 54   b.gt	loc_2510e081c ; ⤵ 0x60
0x2510e07c0:  1f 05 00 31   cmn	w8, #0x1
0x2510e07c4:  c0 01 00 54   b.eq	loc_2510e07fc ; ⤵ 0x38
0x2510e07c8:  ; loc_2510e07c8
0x2510e07c8:  28 17 40 f9   ldr	x8, [x25, #0x28]
0x2510e07cc:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e07d0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e07d4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e07d8:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e07dc:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e07e0:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e07e4:  63 14 17 91   add	x3, x3, #0x5c5 ; "%@ max FPS: %d\n"
0x2510e07e8:  02 05 80 52   mov	w2, #0x28
0x2510e07ec:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e07f0:  c4 81 c0 95   bl	j__LogPrintF
0x2510e07f4:  20 d3 41 b9   ldr	w0, [x25, #0x1d0]
0x2510e07f8:  09 00 00 14   b	loc_2510e081c ; ⤵ 0x24
0x2510e07fc:  ; loc_2510e07fc
0x2510e07fc:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0800:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0804:  01 05 80 52   mov	w1, #0x28
0x2510e0808:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e080c:  6d 81 c0 95   bl	j___LogCategory_Initialize
0x2510e0810:  e8 03 00 aa   mov	x8, x0
0x2510e0814:  20 d3 41 b9   ldr	w0, [x25, #0x1d0]
0x2510e0818:  88 fd ff 35   cbnz	w8, loc_2510e07c8 ; ⤴ -0x50
0x2510e081c:  ; loc_2510e081c
0x2510e081c:  20 db 01 b9   str	w0, [x25, #0x1d8]
0x2510e0820:  20 df 01 b9   str	w0, [x25, #0x1dc]
0x2510e0824:  20 d7 40 fd   ldr	d0, [x25, #0x1a8]
0x2510e0828:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e082c:  00 00 20 91   add	x0, x0, #0x800 ; "bwMin"
0x2510e0830:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0834:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0838:  0a b5 c0 95   bl	j__FigGetCFPreferenceDoubleWithDefault
0x2510e083c:  20 d7 00 fd   str	d0, [x25, #0x1a8]
0x2510e0840:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0844:  1f a1 00 71   cmp	w8, #0x28
0x2510e0848:  cc 02 00 54   b.gt	loc_2510e08a0 ; ⤵ 0x58
0x2510e084c:  1f 05 00 31   cmn	w8, #0x1
0x2510e0850:  c0 01 00 54   b.eq	loc_2510e0888 ; ⤵ 0x38
0x2510e0854:  ; loc_2510e0854
0x2510e0854:  48 03 40 f9   ldr	x8, [x26]
0x2510e0858:  e0 07 00 fd   str	d0, [sp, #0x8]
0x2510e085c:  e8 03 00 f9   str	x8, [sp]
0x2510e0860:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0864:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0868:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e086c:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0870:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0874:  63 6c 17 91   add	x3, x3, #0x5db ; "%@ bandwidth min: %f bps\n"
0x2510e0878:  02 05 80 52   mov	w2, #0x28
0x2510e087c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0880:  a0 81 c0 95   bl	j__LogPrintF
0x2510e0884:  07 00 00 14   b	loc_2510e08a0 ; ⤵ 0x1c
0x2510e0888:  ; loc_2510e0888
0x2510e0888:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e088c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0890:  01 05 80 52   mov	w1, #0x28
0x2510e0894:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0898:  4a 81 c0 95   bl	j___LogCategory_Initialize
0x2510e089c:  00 67 00 35   cbnz	w0, loc_2510e157c ; ⤵ 0xce0
0x2510e08a0:  ; loc_2510e08a0
0x2510e08a0:  20 db 40 fd   ldr	d0, [x25, #0x1b0]
0x2510e08a4:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e08a8:  00 80 20 91   add	x0, x0, #0x820 ; "bwMax"
0x2510e08ac:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e08b0:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e08b4:  eb b4 c0 95   bl	j__FigGetCFPreferenceDoubleWithDefault
0x2510e08b8:  20 db 00 fd   str	d0, [x25, #0x1b0]
0x2510e08bc:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e08c0:  1f a1 00 71   cmp	w8, #0x28
0x2510e08c4:  cc 02 00 54   b.gt	loc_2510e091c ; ⤵ 0x58
0x2510e08c8:  1f 05 00 31   cmn	w8, #0x1
0x2510e08cc:  c0 01 00 54   b.eq	loc_2510e0904 ; ⤵ 0x38
0x2510e08d0:  ; loc_2510e08d0
0x2510e08d0:  48 03 40 f9   ldr	x8, [x26]
0x2510e08d4:  e0 07 00 fd   str	d0, [sp, #0x8]
0x2510e08d8:  e8 03 00 f9   str	x8, [sp]
0x2510e08dc:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e08e0:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e08e4:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e08e8:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e08ec:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e08f0:  63 ec 17 91   add	x3, x3, #0x5fb ; "%@ bandwidth max: %f bps\n"
0x2510e08f4:  02 05 80 52   mov	w2, #0x28
0x2510e08f8:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e08fc:  81 81 c0 95   bl	j__LogPrintF
0x2510e0900:  07 00 00 14   b	loc_2510e091c ; ⤵ 0x1c
0x2510e0904:  ; loc_2510e0904
0x2510e0904:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0908:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e090c:  01 05 80 52   mov	w1, #0x28
0x2510e0910:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0914:  2b 81 c0 95   bl	j___LogCategory_Initialize
0x2510e0918:  60 63 00 35   cbnz	w0, loc_2510e1584 ; ⤵ 0xc6c
0x2510e091c:  ; loc_2510e091c
0x2510e091c:  22 e3 45 39   ldrb	w2, [x25, #0x178]
0x2510e0920:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e0924:  00 00 21 91   add	x0, x0, #0x840 ; "enableMirroringRestrictions"
0x2510e0928:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e092c:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0930:  d0 b4 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0934:  20 e3 05 39   strb	w0, [x25, #0x178]
0x2510e0938:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e093c:  1f a1 00 71   cmp	w8, #0x28
0x2510e0940:  8c 03 00 54   b.gt	loc_2510e09b0 ; ⤵ 0x70
0x2510e0944:  1f 05 00 31   cmn	w8, #0x1
0x2510e0948:  80 02 00 54   b.eq	loc_2510e0998 ; ⤵ 0x50
0x2510e094c:  08 1c 00 12   and	w8, w0, #0xff
0x2510e0950:  ; loc_2510e0950
0x2510e0950:  49 03 40 f9   ldr	x9, [x26]
0x2510e0954:  ca 04 00 f0   adrp	x10, 0x25117b000
0x2510e0958:  4a 75 09 91   add	x10, x10, #0x25d ; "yes"
0x2510e095c:  cb 04 00 f0   adrp	x11, 0x25117b000
0x2510e0960:  6b 85 09 91   add	x11, x11, #0x261 ; "no"
0x2510e0964:  1f 01 00 71   cmp	w8, #0
0x2510e0968:  68 01 8a 9a   csel	x8, x11, x10, eq
0x2510e096c:  e9 23 00 a9   stp	x9, x8, [sp]
0x2510e0970:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0974:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0978:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e097c:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0980:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0984:  63 c4 18 91   add	x3, x3, #0x631 ; "%@ enable mirroring restrictions: %s\n"
0x2510e0988:  02 05 80 52   mov	w2, #0x28
0x2510e098c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0990:  5c 81 c0 95   bl	j__LogPrintF
0x2510e0994:  07 00 00 14   b	loc_2510e09b0 ; ⤵ 0x1c
0x2510e0998:  ; loc_2510e0998
0x2510e0998:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e099c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e09a0:  01 05 80 52   mov	w1, #0x28
0x2510e09a4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e09a8:  06 81 c0 95   bl	j___LogCategory_Initialize
0x2510e09ac:  00 5f 00 35   cbnz	w0, loc_2510e158c ; ⤵ 0xbe0
0x2510e09b0:  ; loc_2510e09b0
0x2510e09b0:  22 7f 41 b9   ldr	w2, [x25, #0x17c]
0x2510e09b4:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e09b8:  00 80 21 91   add	x0, x0, #0x860 ; "requestedMaxEncoderLatency"
0x2510e09bc:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e09c0:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e09c4:  ab b4 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e09c8:  20 7f 01 b9   str	w0, [x25, #0x17c]
0x2510e09cc:  1f 04 00 71   cmp	w0, #0x1
0x2510e09d0:  0b 03 00 54   b.lt	loc_2510e0a30 ; ⤵ 0x60
0x2510e09d4:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e09d8:  1f a1 00 71   cmp	w8, #0x28
0x2510e09dc:  ac 02 00 54   b.gt	loc_2510e0a30 ; ⤵ 0x54
0x2510e09e0:  1f 05 00 31   cmn	w8, #0x1
0x2510e09e4:  a0 01 00 54   b.eq	loc_2510e0a18 ; ⤵ 0x34
0x2510e09e8:  ; loc_2510e09e8
0x2510e09e8:  48 03 40 f9   ldr	x8, [x26]
0x2510e09ec:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e09f0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e09f4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e09f8:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e09fc:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0a00:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0a04:  63 c8 19 91   add	x3, x3, #0x672 ; "%@ requested max encoder latency: %d\n"
0x2510e0a08:  02 05 80 52   mov	w2, #0x28
0x2510e0a0c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0a10:  3c 81 c0 95   bl	j__LogPrintF
0x2510e0a14:  07 00 00 14   b	loc_2510e0a30 ; ⤵ 0x1c
0x2510e0a18:  ; loc_2510e0a18
0x2510e0a18:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0a1c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0a20:  01 05 80 52   mov	w1, #0x28
0x2510e0a24:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0a28:  e6 80 c0 95   bl	j___LogCategory_Initialize
0x2510e0a2c:  80 5b 00 35   cbnz	w0, loc_2510e159c ; ⤵ 0xb70
0x2510e0a30:  ; loc_2510e0a30
0x2510e0a30:  28 0f 46 39   ldrb	w8, [x25, #0x183]
0x2510e0a34:  28 08 00 34   cbz	w8, loc_2510e0b38 ; ⤵ 0x104
0x2510e0a38:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e0a3c:  00 00 22 91   add	x0, x0, #0x880 ; "virtualDisplayTestSinkMode"
0x2510e0a40:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0a44:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0a48:  02 00 80 52   mov	w2, #0
0x2510e0a4c:  89 b4 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0a50:  20 8b 01 b9   str	w0, [x25, #0x188]
0x2510e0a54:  1f 04 00 71   cmp	w0, #0x1
0x2510e0a58:  0b 03 00 54   b.lt	loc_2510e0ab8 ; ⤵ 0x60
0x2510e0a5c:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0a60:  1f a1 00 71   cmp	w8, #0x28
0x2510e0a64:  ac 02 00 54   b.gt	loc_2510e0ab8 ; ⤵ 0x54
0x2510e0a68:  1f 05 00 31   cmn	w8, #0x1
0x2510e0a6c:  a0 01 00 54   b.eq	loc_2510e0aa0 ; ⤵ 0x34
0x2510e0a70:  ; loc_2510e0a70
0x2510e0a70:  48 03 40 f9   ldr	x8, [x26]
0x2510e0a74:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e0a78:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0a7c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0a80:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0a84:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0a88:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0a8c:  63 cc 1a 91   add	x3, x3, #0x6b3 ; "%@ will use APVirtualDisplayTestSink in mode %d\n"
0x2510e0a90:  02 05 80 52   mov	w2, #0x28
0x2510e0a94:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0a98:  1a 81 c0 95   bl	j__LogPrintF
0x2510e0a9c:  07 00 00 14   b	loc_2510e0ab8 ; ⤵ 0x1c
0x2510e0aa0:  ; loc_2510e0aa0
0x2510e0aa0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0aa4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0aa8:  01 05 80 52   mov	w1, #0x28
0x2510e0aac:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0ab0:  c4 80 c0 95   bl	j___LogCategory_Initialize
0x2510e0ab4:  40 58 00 35   cbnz	w0, loc_2510e15bc ; ⤵ 0xb08
0x2510e0ab8:  ; loc_2510e0ab8
0x2510e0ab8:  22 8f 41 b9   ldr	w2, [x25, #0x18c]
0x2510e0abc:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e0ac0:  00 80 22 91   add	x0, x0, #0x8a0 ; "wirelessdisplay_maxtimesreencodeidle"
0x2510e0ac4:  e1 50 12 90   adrp	x1, 0x275afc000
0x2510e0ac8:  21 00 24 91   add	x1, x1, #0x900 ; "com.apple.coremedia"
0x2510e0acc:  69 b4 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0ad0:  20 8f 01 b9   str	w0, [x25, #0x18c]
0x2510e0ad4:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0ad8:  1f c9 00 71   cmp	w8, #0x32
0x2510e0adc:  ec 02 00 54   b.gt	loc_2510e0b38 ; ⤵ 0x5c
0x2510e0ae0:  1f 05 00 31   cmn	w8, #0x1
0x2510e0ae4:  c0 01 00 54   b.eq	loc_2510e0b1c ; ⤵ 0x38
0x2510e0ae8:  ; loc_2510e0ae8
0x2510e0ae8:  48 03 40 f9   ldr	x8, [x26]
0x2510e0aec:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e0af0:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0af4:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0af8:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0afc:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0b00:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0b04:  63 24 1c 91   add	x3, x3, #0x709 ; "%@ max times to reencode idle frame: %d\n"
0x2510e0b08:  08 05 80 52   mov	w8, #0x28
0x2510e0b0c:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e0b10:  02 29 00 11   add	w2, w8, #0xa
0x2510e0b14:  fb 80 c0 95   bl	j__LogPrintF
0x2510e0b18:  08 00 00 14   b	loc_2510e0b38 ; ⤵ 0x20
0x2510e0b1c:  ; loc_2510e0b1c
0x2510e0b1c:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0b20:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0b24:  08 05 80 52   mov	w8, #0x28
0x2510e0b28:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e0b2c:  01 29 00 11   add	w1, w8, #0xa
0x2510e0b30:  a4 80 c0 95   bl	j___LogCategory_Initialize
0x2510e0b34:  80 53 00 35   cbnz	w0, loc_2510e15a4 ; ⤵ 0xa70
0x2510e0b38:  ; loc_2510e0b38
0x2510e0b38:  f4 13 00 f9   str	x20, [sp, #0x20]
0x2510e0b3c:  c0 50 12 f0   adrp	x0, 0x275afb000
0x2510e0b40:  00 80 38 91   add	x0, x0, #0xe20 ; "receiverHDRCapability"
0x2510e0b44:  e1 63 02 91   add	x1, sp, #0x98
0x2510e0b48:  ee ae c0 95   bl	j__APSSettingsCopyValue
0x2510e0b4c:  f4 03 00 aa   mov	x20, x0
0x2510e0b50:  e8 9b 40 b9   ldr	w8, [sp, #0x98]
0x2510e0b54:  28 04 00 35   cbnz	w8, loc_2510e0bd8 ; ⤵ 0x84
0x2510e0b58:  14 04 00 b4   cbz	x20, loc_2510e0bd8 ; ⤵ 0x80
0x2510e0b5c:  e0 03 14 aa   mov	x0, x20
0x2510e0b60:  a4 83 c0 95   bl	j__CFGetTypeID
0x2510e0b64:  f6 03 00 aa   mov	x22, x0
0x2510e0b68:  0e 84 c0 95   bl	j__CFStringGetTypeID
0x2510e0b6c:  df 02 00 eb   cmp	x22, x0
0x2510e0b70:  f6 1b 40 f9   ldr	x22, [sp, #0x30]
0x2510e0b74:  61 03 00 54   b.ne	loc_2510e0be0 ; ⤵ 0x6c
0x2510e0b78:  56 83 40 f9   ldr	x22, [x26, #0x100]
0x2510e0b7c:  54 83 00 f9   str	x20, [x26, #0x100]
0x2510e0b80:  e0 03 14 aa   mov	x0, x20
0x2510e0b84:  ff 83 c0 95   bl	j__CFRetain
0x2510e0b88:  76 00 00 b4   cbz	x22, loc_2510e0b94 ; ⤵ 0xc
0x2510e0b8c:  e0 03 16 aa   mov	x0, x22
0x2510e0b90:  f8 83 c0 95   bl	j__CFRelease
0x2510e0b94:  ; loc_2510e0b94
0x2510e0b94:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0b98:  1f c9 00 71   cmp	w8, #0x32
0x2510e0b9c:  f6 1b 40 f9   ldr	x22, [sp, #0x30]
0x2510e0ba0:  0c 02 00 54   b.gt	loc_2510e0be0 ; ⤵ 0x40
0x2510e0ba4:  1f 05 00 31   cmn	w8, #0x1
0x2510e0ba8:  01 01 00 54   b.ne	loc_2510e0bc8 ; ⤵ 0x20
0x2510e0bac:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0bb0:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0bb4:  08 05 80 52   mov	w8, #0x28
0x2510e0bb8:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e0bbc:  01 29 00 11   add	w1, w8, #0xa
0x2510e0bc0:  80 80 c0 95   bl	j___LogCategory_Initialize
0x2510e0bc4:  e0 00 00 34   cbz	w0, loc_2510e0be0 ; ⤵ 0x1c
0x2510e0bc8:  ; loc_2510e0bc8
0x2510e0bc8:  41 03 04 91   add	x1, x26, #0x100
0x2510e0bcc:  e0 03 1a aa   mov	x0, x26
0x2510e0bd0:  f5 5a 02 94   bl	_APEndpointStreamScreenCreate.cold.6
0x2510e0bd4:  03 00 00 14   b	loc_2510e0be0 ; ⤵ 0xc
0x2510e0bd8:  ; loc_2510e0bd8
0x2510e0bd8:  f6 1b 40 f9   ldr	x22, [sp, #0x30]
0x2510e0bdc:  74 00 00 b4   cbz	x20, loc_2510e0be8 ; ⤵ 0xc
0x2510e0be0:  ; loc_2510e0be0
0x2510e0be0:  e0 03 14 aa   mov	x0, x20
0x2510e0be4:  e3 83 c0 95   bl	j__CFRelease
0x2510e0be8:  ; loc_2510e0be8
0x2510e0be8:  ff 9b 00 b9   str	wzr, [sp, #0x98]
0x2510e0bec:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e0bf0:  00 00 23 91   add	x0, x0, #0x8c0 ; "latencyHint"
0x2510e0bf4:  c1 50 12 90   adrp	x1, 0x275af8000
0x2510e0bf8:  21 80 0a 91   add	x1, x1, #0x2a0 ; "com.apple.airplay"
0x2510e0bfc:  e2 03 17 aa   mov	x2, x23
0x2510e0c00:  1c b4 c0 95   bl	j__FigGetCFPreferenceNumberWithDefault
0x2510e0c04:  28 a7 44 b9   ldr	w8, [x25, #0x4a4]
0x2510e0c08:  1f 00 08 6b   cmp	w0, w8
0x2510e0c0c:  c0 03 00 54   b.eq	loc_2510e0c84 ; ⤵ 0x78
0x2510e0c10:  20 a7 04 b9   str	w0, [x25, #0x4a4]
0x2510e0c14:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0c18:  1f a1 00 71   cmp	w8, #0x28
0x2510e0c1c:  ac 02 00 54   b.gt	loc_2510e0c70 ; ⤵ 0x54
0x2510e0c20:  1f 05 00 31   cmn	w8, #0x1
0x2510e0c24:  a0 01 00 54   b.eq	loc_2510e0c58 ; ⤵ 0x34
0x2510e0c28:  ; loc_2510e0c28
0x2510e0c28:  48 03 40 f9   ldr	x8, [x26]
0x2510e0c2c:  e8 03 00 a9   stp	x8, x0, [sp]
0x2510e0c30:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0c34:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0c38:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0c3c:  21 34 11 91   add	x1, x1, #0x44d ; "void screenstream_initFromPrefs(StreamScreenStorage *)"
0x2510e0c40:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0c44:  63 a0 1d 91   add	x3, x3, #0x768 ; "%@ connection latency hint: %d\n"
0x2510e0c48:  02 05 80 52   mov	w2, #0x28
0x2510e0c4c:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0c50:  ac 80 c0 95   bl	j__LogPrintF
0x2510e0c54:  07 00 00 14   b	loc_2510e0c70 ; ⤵ 0x1c
0x2510e0c58:  ; loc_2510e0c58
0x2510e0c58:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0c5c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0c60:  01 05 80 52   mov	w1, #0x28
0x2510e0c64:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0c68:  56 80 c0 95   bl	j___LogCategory_Initialize
0x2510e0c6c:  40 4a 00 35   cbnz	w0, loc_2510e15b4 ; ⤵ 0x948
0x2510e0c70:  ; loc_2510e0c70
0x2510e0c70:  28 0f 46 39   ldrb	w8, [x25, #0x183]
0x2510e0c74:  88 00 00 35   cbnz	w8, loc_2510e0c84 ; ⤵ 0x10
0x2510e0c78:  20 a7 44 b9   ldr	w0, [x25, #0x4a4]
0x2510e0c7c:  99 ae c0 95   bl	j__APSScreenLatencyMs
0x2510e0c80:  20 9f 04 b9   str	w0, [x25, #0x49c]
0x2510e0c84:  ; loc_2510e0c84
0x2510e0c84:  47 b5 c0 95   bl	j__IsAppleInternalBuild
0x2510e0c88:  20 04 00 34   cbz	w0, loc_2510e0d0c ; ⤵ 0x84
0x2510e0c8c:  40 51 12 d0   adrp	x0, 0x275b0a000
0x2510e0c90:  00 80 23 91   add	x0, x0, #0x8e0 ; "mirroringModeOverride"
0x2510e0c94:  01 00 80 d2   mov	x1, #0
0x2510e0c98:  9a ae c0 95   bl	j__APSSettingsCopyValue
0x2510e0c9c:  80 03 00 b4   cbz	x0, loc_2510e0d0c ; ⤵ 0x70
0x2510e0ca0:  f4 03 00 aa   mov	x20, x0
0x2510e0ca4:  53 83 c0 95   bl	j__CFGetTypeID
0x2510e0ca8:  f6 03 00 aa   mov	x22, x0
0x2510e0cac:  bd 83 c0 95   bl	j__CFStringGetTypeID
0x2510e0cb0:  df 02 00 eb   cmp	x22, x0
0x2510e0cb4:  61 02 00 54   b.ne	loc_2510e0d00 ; ⤵ 0x4c
0x2510e0cb8:  36 cf 40 f9   ldr	x22, [x25, #0x198]
0x2510e0cbc:  34 cf 00 f9   str	x20, [x25, #0x198]
0x2510e0cc0:  e0 03 14 aa   mov	x0, x20
0x2510e0cc4:  af 83 c0 95   bl	j__CFRetain
0x2510e0cc8:  76 00 00 b4   cbz	x22, loc_2510e0cd4 ; ⤵ 0xc
0x2510e0ccc:  e0 03 16 aa   mov	x0, x22
0x2510e0cd0:  a8 83 c0 95   bl	j__CFRelease
0x2510e0cd4:  ; loc_2510e0cd4
0x2510e0cd4:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0cd8:  1f a1 00 71   cmp	w8, #0x28
0x2510e0cdc:  2c 01 00 54   b.gt	loc_2510e0d00 ; ⤵ 0x24
0x2510e0ce0:  1f 05 00 31   cmn	w8, #0x1
0x2510e0ce4:  c1 35 00 54   b.ne	loc_2510e139c ; ⤵ 0x6b8
0x2510e0ce8:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0cec:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0cf0:  01 05 80 52   mov	w1, #0x28
0x2510e0cf4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0cf8:  32 80 c0 95   bl	j___LogCategory_Initialize
0x2510e0cfc:  00 35 00 35   cbnz	w0, loc_2510e139c ; ⤵ 0x6a0
0x2510e0d00:  ; loc_2510e0d00
0x2510e0d00:  e0 03 14 aa   mov	x0, x20
0x2510e0d04:  9b 83 c0 95   bl	j__CFRelease
0x2510e0d08:  f6 1b 40 f9   ldr	x22, [sp, #0x30]
0x2510e0d0c:  ; loc_2510e0d0c
0x2510e0d0c:  2d c2 c0 95   bl	j__CMBufferQueueGetCallbacksForUnsortedSampleBuffers
0x2510e0d10:  e2 03 00 aa   mov	x2, x0
0x2510e0d14:  23 a3 01 91   add	x3, x25, #0x68
0x2510e0d18:  e0 03 1b aa   mov	x0, x27
0x2510e0d1c:  01 00 80 d2   mov	x1, #0
0x2510e0d20:  18 c2 c0 95   bl	j__CMBufferQueueCreate
0x2510e0d24:  60 37 00 35   cbnz	w0, loc_2510e1410 ; ⤵ 0x6ec
0x2510e0d28:  20 37 40 f9   ldr	x0, [x25, #0x68]
0x2510e0d2c:  90 f3 ff d0   adrp	x16, 0x250f52000
0x2510e0d30:  10 c2 31 91   add	x16, x16, #0xc70 ; _screenstream_signalDataAvailable
0x2510e0d34:  f0 23 c1 da   paciza	x16
0x2510e0d38:  e1 03 10 aa   mov	x1, x16
0x2510e0d3c:  25 c3 01 91   add	x5, x25, #0x70
0x2510e0d40:  e2 03 19 aa   mov	x2, x25
0x2510e0d44:  63 01 80 52   mov	w3, #0xb
0x2510e0d48:  04 00 80 d2   mov	x4, #0
0x2510e0d4c:  25 c2 c0 95   bl	j__CMBufferQueueInstallTriggerWithIntegerThreshold
0x2510e0d50:  60 36 00 35   cbnz	w0, loc_2510e141c ; ⤵ 0x6cc
0x2510e0d54:  b6 1c 00 b4   cbz	x22, loc_2510e10e8 ; ⤵ 0x394
0x2510e0d58:  68 15 0c f0   adrp	x8, 0x26938f000
0x2510e0d5c:  08 21 27 91   add	x8, x8, #0x9c8 ; _kAPEndpointStreamCreationOptionKey_ClientPID
0x2510e0d60:  01 01 40 f9   ldr	x1, [x8]
0x2510e0d64:  e0 03 16 aa   mov	x0, x22
0x2510e0d68:  12 86 c0 95   bl	j__CFDictionaryGetValue
0x2510e0d6c:  20 0e 00 b4   cbz	x0, loc_2510e0f30 ; ⤵ 0x1c4
0x2510e0d70:  f4 03 00 aa   mov	x20, x0
0x2510e0d74:  1f 83 c0 95   bl	j__CFGetTypeID
0x2510e0d78:  f6 03 00 aa   mov	x22, x0
0x2510e0d7c:  25 9d c0 95   bl	j__CFNumberGetTypeID
0x2510e0d80:  df 02 00 eb   cmp	x22, x0
0x2510e0d84:  e1 37 00 54   b.ne	loc_2510e1480 ; ⤵ 0x6fc
0x2510e0d88:  36 63 40 f9   ldr	x22, [x25, #0xc0]
0x2510e0d8c:  34 63 00 f9   str	x20, [x25, #0xc0]
0x2510e0d90:  e0 03 14 aa   mov	x0, x20
0x2510e0d94:  7b 83 c0 95   bl	j__CFRetain
0x2510e0d98:  76 00 00 b4   cbz	x22, loc_2510e0da4 ; ⤵ 0xc
0x2510e0d9c:  ; loc_2510e0d9c
0x2510e0d9c:  e0 03 16 aa   mov	x0, x22
0x2510e0da0:  74 83 c0 95   bl	j__CFRelease
0x2510e0da4:  ; loc_2510e0da4
0x2510e0da4:  68 15 0c f0   adrp	x8, 0x26938f000
0x2510e0da8:  08 a1 26 91   add	x8, x8, #0x9a8 ; _kAPEndpointStreamCreationOptionKey_ScreenSource
0x2510e0dac:  01 01 40 f9   ldr	x1, [x8]
0x2510e0db0:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2510e0db4:  ff 85 c0 95   bl	j__CFDictionaryGetValue
0x2510e0db8:  f4 03 00 aa   mov	x20, x0
0x2510e0dbc:  20 0c 00 b4   cbz	x0, loc_2510e0f40 ; ⤵ 0x184
0x2510e0dc0:  e0 03 14 aa   mov	x0, x20
0x2510e0dc4:  0b 83 c0 95   bl	j__CFGetTypeID
0x2510e0dc8:  f6 03 00 aa   mov	x22, x0
0x2510e0dcc:  31 c5 c0 95   bl	j__FigVirtualDisplaySourceGetTypeID
0x2510e0dd0:  df 02 00 eb   cmp	x22, x0
0x2510e0dd4:  a1 35 00 54   b.ne	loc_2510e1488 ; ⤵ 0x6b4
0x2510e0dd8:  36 4b 40 f9   ldr	x22, [x25, #0x90]
0x2510e0ddc:  34 4b 00 f9   str	x20, [x25, #0x90]
0x2510e0de0:  e0 03 14 aa   mov	x0, x20
0x2510e0de4:  67 83 c0 95   bl	j__CFRetain
0x2510e0de8:  76 00 00 b4   cbz	x22, loc_2510e0df4 ; ⤵ 0xc
0x2510e0dec:  ; loc_2510e0dec
0x2510e0dec:  e0 03 16 aa   mov	x0, x22
0x2510e0df0:  60 83 c0 95   bl	j__CFRelease
0x2510e0df4:  ; loc_2510e0df4
0x2510e0df4:  68 15 0c f0   adrp	x8, 0x26938f000
0x2510e0df8:  08 c1 26 91   add	x8, x8, #0x9b0 ; _kAPEndpointStreamCreationOptionKey_ScreenProcessor
0x2510e0dfc:  01 01 40 f9   ldr	x1, [x8]
0x2510e0e00:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2510e0e04:  eb 85 c0 95   bl	j__CFDictionaryGetValue
0x2510e0e08:  f6 03 00 aa   mov	x22, x0
0x2510e0e0c:  20 0a 00 b4   cbz	x0, loc_2510e0f50 ; ⤵ 0x144
0x2510e0e10:  e0 03 16 aa   mov	x0, x22
0x2510e0e14:  f7 82 c0 95   bl	j__CFGetTypeID
0x2510e0e18:  f7 03 00 aa   mov	x23, x0
0x2510e0e1c:  09 c5 c0 95   bl	j__FigVirtualDisplayProcessorGetTypeID
0x2510e0e20:  ff 02 00 eb   cmp	x23, x0
0x2510e0e24:  61 33 00 54   b.ne	loc_2510e1490 ; ⤵ 0x66c
0x2510e0e28:  37 4f 40 f9   ldr	x23, [x25, #0x98]
0x2510e0e2c:  36 4f 00 f9   str	x22, [x25, #0x98]
0x2510e0e30:  e0 03 16 aa   mov	x0, x22
0x2510e0e34:  53 83 c0 95   bl	j__CFRetain
0x2510e0e38:  77 00 00 b4   cbz	x23, loc_2510e0e44 ; ⤵ 0xc
0x2510e0e3c:  ; loc_2510e0e3c
0x2510e0e3c:  e0 03 17 aa   mov	x0, x23
0x2510e0e40:  4c 83 c0 95   bl	j__CFRelease
0x2510e0e44:  ; loc_2510e0e44
0x2510e0e44:  68 15 0c f0   adrp	x8, 0x26938f000
0x2510e0e48:  08 e1 26 91   add	x8, x8, #0x9b8 ; _kAPEndpointStreamCreationOptionKey_ScreenUsageMode
0x2510e0e4c:  01 01 40 f9   ldr	x1, [x8]
0x2510e0e50:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2510e0e54:  d7 85 c0 95   bl	j__CFDictionaryGetValue
0x2510e0e58:  40 08 00 b4   cbz	x0, loc_2510e0f60 ; ⤵ 0x108
0x2510e0e5c:  f7 03 00 aa   mov	x23, x0
0x2510e0e60:  e4 82 c0 95   bl	j__CFGetTypeID
0x2510e0e64:  fb 03 00 aa   mov	x27, x0
0x2510e0e68:  4e 83 c0 95   bl	j__CFStringGetTypeID
0x2510e0e6c:  7f 03 00 eb   cmp	x27, x0
0x2510e0e70:  41 31 00 54   b.ne	loc_2510e1498 ; ⤵ 0x628
0x2510e0e74:  3b 57 40 f9   ldr	x27, [x25, #0xa8]
0x2510e0e78:  37 57 00 f9   str	x23, [x25, #0xa8]
0x2510e0e7c:  e0 03 17 aa   mov	x0, x23
0x2510e0e80:  40 83 c0 95   bl	j__CFRetain
0x2510e0e84:  7b 00 00 b4   cbz	x27, loc_2510e0e90 ; ⤵ 0xc
0x2510e0e88:  ; loc_2510e0e88
0x2510e0e88:  e0 03 1b aa   mov	x0, x27
0x2510e0e8c:  39 83 c0 95   bl	j__CFRelease
0x2510e0e90:  ; loc_2510e0e90
0x2510e0e90:  88 02 16 aa   orr	x8, x20, x22
0x2510e0e94:  88 08 00 b5   cbnz	x8, loc_2510e0fa4 ; ⤵ 0x110
0x2510e0e98:  68 15 0c f0   adrp	x8, 0x26938f000
0x2510e0e9c:  08 61 28 91   add	x8, x8, #0xa18 ; _kAPEndpointStreamCreationOptionKey_ProtectionOptions
0x2510e0ea0:  01 01 40 f9   ldr	x1, [x8]
0x2510e0ea4:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2510e0ea8:  c2 85 c0 95   bl	j__CFDictionaryGetValue
0x2510e0eac:  f4 03 00 aa   mov	x20, x0
0x2510e0eb0:  00 06 00 b4   cbz	x0, loc_2510e0f70 ; ⤵ 0xc0
0x2510e0eb4:  e0 03 14 aa   mov	x0, x20
0x2510e0eb8:  ce 82 c0 95   bl	j__CFGetTypeID
0x2510e0ebc:  f6 03 00 aa   mov	x22, x0
0x2510e0ec0:  b8 85 c0 95   bl	j__CFDictionaryGetTypeID
0x2510e0ec4:  df 02 00 eb   cmp	x22, x0
0x2510e0ec8:  c1 2e 00 54   b.ne	loc_2510e14a0 ; ⤵ 0x5d8
0x2510e0ecc:  36 a7 40 f9   ldr	x22, [x25, #0x148]
0x2510e0ed0:  34 a7 00 f9   str	x20, [x25, #0x148]
0x2510e0ed4:  e0 03 14 aa   mov	x0, x20
0x2510e0ed8:  2a 83 c0 95   bl	j__CFRetain
0x2510e0edc:  76 00 00 b4   cbz	x22, loc_2510e0ee8 ; ⤵ 0xc
0x2510e0ee0:  ; loc_2510e0ee0
0x2510e0ee0:  e0 03 16 aa   mov	x0, x22
0x2510e0ee4:  23 83 c0 95   bl	j__CFRelease
0x2510e0ee8:  ; loc_2510e0ee8
0x2510e0ee8:  f4 05 00 b4   cbz	x20, loc_2510e0fa4 ; ⤵ 0xbc
0x2510e0eec:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e0ef0:  1f a1 00 71   cmp	w8, #0x28
0x2510e0ef4:  8c 05 00 54   b.gt	loc_2510e0fa4 ; ⤵ 0xb0
0x2510e0ef8:  1f 05 00 31   cmn	w8, #0x1
0x2510e0efc:  80 04 00 54   b.eq	loc_2510e0f8c ; ⤵ 0x90
0x2510e0f00:  ; loc_2510e0f00
0x2510e0f00:  48 03 40 f9   ldr	x8, [x26]
0x2510e0f04:  e8 53 00 a9   stp	x8, x20, [sp]
0x2510e0f08:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0f0c:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0f10:  21 09 00 f0   adrp	x1, 0x251207000
0x2510e0f14:  21 d0 09 91   add	x1, x1, #0x274 ; "OSStatus screenstream_activate(FigEndpointStreamRef, CFDictionaryRef)"
0x2510e0f18:  23 09 00 f0   adrp	x3, 0x251207000
0x2510e0f1c:  63 e8 0a 91   add	x3, x3, #0x2ba ; "%@ PWD activation options: %@\n"
0x2510e0f20:  02 05 80 52   mov	w2, #0x28
0x2510e0f24:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x2510e0f28:  f6 7f c0 95   bl	j__LogPrintF
0x2510e0f2c:  1e 00 00 14   b	loc_2510e0fa4 ; ⤵ 0x78
0x2510e0f30:  ; loc_2510e0f30
0x2510e0f30:  36 63 40 f9   ldr	x22, [x25, #0xc0]
0x2510e0f34:  3f 63 00 f9   str	xzr, [x25, #0xc0]
0x2510e0f38:  36 f3 ff b5   cbnz	x22, loc_2510e0d9c ; ⤴ -0x19c
0x2510e0f3c:  9a ff ff 17   b	loc_2510e0da4 ; ⤴ -0x198
0x2510e0f40:  ; loc_2510e0f40
0x2510e0f40:  36 4b 40 f9   ldr	x22, [x25, #0x90]
0x2510e0f44:  3f 4b 00 f9   str	xzr, [x25, #0x90]
0x2510e0f48:  36 f5 ff b5   cbnz	x22, loc_2510e0dec ; ⤴ -0x15c
0x2510e0f4c:  aa ff ff 17   b	loc_2510e0df4 ; ⤴ -0x158
0x2510e0f50:  ; loc_2510e0f50
0x2510e0f50:  37 4f 40 f9   ldr	x23, [x25, #0x98]
0x2510e0f54:  3f 4f 00 f9   str	xzr, [x25, #0x98]
0x2510e0f58:  37 f7 ff b5   cbnz	x23, loc_2510e0e3c ; ⤴ -0x11c
0x2510e0f5c:  ba ff ff 17   b	loc_2510e0e44 ; ⤴ -0x118
0x2510e0f60:  ; loc_2510e0f60
0x2510e0f60:  3b 57 40 f9   ldr	x27, [x25, #0xa8]
0x2510e0f64:  3f 57 00 f9   str	xzr, [x25, #0xa8]
0x2510e0f68:  1b f9 ff b5   cbnz	x27, loc_2510e0e88 ; ⤴ -0xe0
0x2510e0f6c:  c9 ff ff 17   b	loc_2510e0e90 ; ⤴ -0xdc
0x2510e0f70:  ; loc_2510e0f70
0x2510e0f70:  36 a7 40 f9   ldr	x22, [x25, #0x148]
0x2510e0f74:  3f a7 00 f9   str	xzr, [x25, #0x148]
0x2510e0f78:  56 fb ff b5   cbnz	x22, loc_2510e0ee0 ; ⤴ -0x98
0x2510e0f7c:  db ff ff 17   b	loc_2510e0ee8 ; ⤴ -0x94
0x2510e0f80:  ; loc_2510e0f80
0x2510e0f80:  f2 5a 02 94   bl	_APEndpointStreamScreenCreate.cold.23
0x2510e0f84:  ; loc_2510e0f84
0x2510e0f84:  fc 2e 88 12   mov	w28, #0xffffbe88
0x2510e0f88:  2b fb ff 17   b	loc_2510dfc34 ; ⤴ -0x1354
0x2510e0f8c:  ; loc_2510e0f8c
0x2510e0f8c:  60 9d 0f d0   adrp	x0, 0x27048e000
0x2510e0f90:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e0f94:  01 05 80 52   mov	w1, #0x28
0x2510e0f98:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x2510e0f9c:  89 7f c0 95   bl	j___LogCategory_Initialize
0x2510e0fa0:  00 fb ff 35   cbnz	w0, loc_2510e0f00 ; ⤴ -0xa0
0x2510e0fa4:  ; loc_2510e0fa4
0x2510e0fa4:  68 15 0c f0   adrp	x8, 0x26938f000
0x2510e0fa8:  08 01 27 91   add	x8, x8, #0x9c0 ; _kAPEndpointStreamCreationOptionKey_ScreenOverrides
0x2510e0fac:  01 01 40 f9   ldr	x1, [x8]
0x2510e0fb0:  e0 1b 40 f9   ldr	x0, [sp, #0x30]
0x2510e0fb4:  7f 85 c0 95   bl	j__CFDictionaryGetValue
0x2510e0fb8:  c0 01 00 b4   cbz	x0, loc_2510e0ff0 ; ⤵ 0x38
0x2510e0fbc:  f4 03 00 aa   mov	x20, x0
0x2510e0fc0:  8c 82 c0 95   bl	j__CFGetTypeID
0x2510e0fc4:  f6 03 00 aa   mov	x22, x0
0x2510e0fc8:  76 85 c0 95   bl	j__CFDictionaryGetTypeID
0x2510e0fcc:  df 02 00 eb   cmp	x22, x0
0x2510e0fd0:  01 01 00 54   b.ne	loc_2510e0ff0 ; ⤵ 0x20
0x2510e0fd4:  73 b4 c0 95   bl	j__IsAppleInternalBuild
0x2510e0fd8:  c0 00 00 34   cbz	w0, loc_2510e0ff0 ; ⤵ 0x18
0x2510e0fdc:  36 5b 40 f9   ldr	x22, [x25, #0xb0]
0x2510e0fe0:  34 5b 00 f9   str	x20, [x25, #0xb0]
0x2510e0fe4:  e0 03 14 aa   mov	x0, x20
0x2510e0fe8:  e6 82 c0 95   bl	j__CFRetain
0x2510e0fec:  03 00 00 14   b	loc_2510e0ff8 ; ⤵ 0xc
0x2510e0ff0:  ; loc_2510e0ff0
0x2510e0ff0:  36 5b 40 f9   ldr	x22, [x25, #0xb0]
0x2510e0ff4:  3f 5b 00 f9   str	xzr, [x25, #0xb0]
0x2510e0ff8:  ; loc_2510e0ff8
0x2510e0ff8:  76 00 00 b4   cbz	x22, loc_2510e1004 ; ⤵ 0xc
0x2510e0ffc:  e0 03 16 aa   mov	x0, x22
0x2510e1000:  dc 82 c0 95   bl	j__CFRelease
0x2510e1004:  ; loc_2510e1004
0x2510e1004:  68 15 0c d0   adrp	x8, 0x26938f000
0x2510e1008:  08 61 27 91   add	x8, x8, #0x9d8 ; _kAPEndpointStreamCreationOptionKey_IsMediaPresentation
0x2510e100c:  01 01 40 f9   ldr	x1, [x8]
0x2510e1010:  e2 5f 02 91   add	x2, sp, #0x97
0x2510e1014:  f4 1b 40 f9   ldr	x20, [sp, #0x30]
0x2510e1018:  e0 03 14 aa   mov	x0, x20
0x2510e101c:  69 b2 c0 95   bl	j__FigCFDictionaryGetBooleanIfPresent
0x2510e1020:  68 15 0c d0   adrp	x8, 0x26938f000
0x2510e1024:  08 a1 27 91   add	x8, x8, #0x9e8 ; _kAPEndpointStreamCreationOptionKey_IsMediaPresentationModeAvailable
0x2510e1028:  01 01 40 f9   ldr	x1, [x8]
0x2510e102c:  e0 03 14 aa   mov	x0, x20
0x2510e1030:  f6 13 40 f9   ldr	x22, [sp, #0x20]
0x2510e1034:  e2 03 16 aa   mov	x2, x22
0x2510e1038:  62 b2 c0 95   bl	j__FigCFDictionaryGetBooleanIfPresent
0x2510e103c:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e1040:  1f c9 00 71   cmp	w8, #0x32
0x2510e1044:  4c 01 00 54   b.gt	loc_2510e106c ; ⤵ 0x28
0x2510e1048:  1f 05 00 31   cmn	w8, #0x1
0x2510e104c:  a1 19 00 54   b.ne	loc_2510e1380 ; ⤵ 0x334
0x2510e1050:  60 9d 0f b0   adrp	x0, 0x27048e000
0x2510e1054:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e1058:  08 05 80 52   mov	w8, #0x28
0x2510e105c:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e1060:  01 29 00 11   add	w1, w8, #0xa
0x2510e1064:  57 7f c0 95   bl	j___LogCategory_Initialize
0x2510e1068:  c0 18 00 35   cbnz	w0, loc_2510e1380 ; ⤵ 0x318
0x2510e106c:  ; loc_2510e106c
0x2510e106c:  68 15 0c d0   adrp	x8, 0x26938f000
0x2510e1070:  08 01 28 91   add	x8, x8, #0xa00 ; _kAPEndpointStreamCreationOptionKey_UseCarPlayHEVC
0x2510e1074:  01 01 40 f9   ldr	x1, [x8]
0x2510e1078:  22 13 06 91   add	x2, x25, #0x184
0x2510e107c:  e0 03 14 aa   mov	x0, x20
0x2510e1080:  50 b2 c0 95   bl	j__FigCFDictionaryGetBooleanIfPresent
0x2510e1084:  68 15 0c d0   adrp	x8, 0x26938f000
0x2510e1088:  08 81 27 91   add	x8, x8, #0x9e0 ; _kAPEndpointStreamCreationOptionKey_IsAuxScreenStream
0x2510e108c:  01 01 40 f9   ldr	x1, [x8]
0x2510e1090:  22 0b 06 91   add	x2, x25, #0x182
0x2510e1094:  e0 03 14 aa   mov	x0, x20
0x2510e1098:  4a b2 c0 95   bl	j__FigCFDictionaryGetBooleanIfPresent
0x2510e109c:  68 15 0c d0   adrp	x8, 0x26938f000
0x2510e10a0:  08 e1 27 91   add	x8, x8, #0x9f8 ; _kAPEndpointStreamCreationOptionKey_OverrideCanvasSize
0x2510e10a4:  01 01 40 f9   ldr	x1, [x8]
0x2510e10a8:  42 03 03 91   add	x2, x26, #0xc0
0x2510e10ac:  e0 03 14 aa   mov	x0, x20
0x2510e10b0:  4c b2 c0 95   bl	j__FigCFDictionaryGetCGSizeIfPresent
0x2510e10b4:  a0 01 00 34   cbz	w0, loc_2510e10e8 ; ⤵ 0x34
0x2510e10b8:  88 33 4b b9   ldr	w8, [x28, #0xb30]
0x2510e10bc:  1f c9 00 71   cmp	w8, #0x32
0x2510e10c0:  4c 01 00 54   b.gt	loc_2510e10e8 ; ⤵ 0x28
0x2510e10c4:  1f 05 00 31   cmn	w8, #0x1
0x2510e10c8:  41 16 00 54   b.ne	loc_2510e1390 ; ⤵ 0x2c8
0x2510e10cc:  60 9d 0f b0   adrp	x0, 0x27048e000
0x2510e10d0:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e10d4:  08 05 80 52   mov	w8, #0x28
0x2510e10d8:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e10dc:  01 29 00 11   add	w1, w8, #0xa
0x2510e10e0:  38 7f c0 95   bl	j___LogCategory_Initialize
0x2510e10e4:  60 15 00 35   cbnz	w0, loc_2510e1390 ; ⤵ 0x2ac
0x2510e10e8:  ; loc_2510e10e8
0x2510e10e8:  f6 5f 42 39   ldrb	w22, [sp, #0x97]
0x2510e10ec:  e0 03 18 aa   mov	x0, x24
0x2510e10f0:  34 b1 c0 95   bl	j__CMBaseObjectGetDerivedStorage
0x2510e10f4:  f4 03 00 aa   mov	x20, x0
0x2510e10f8:  ff 4f 00 f9   str	xzr, [sp, #0x98]
0x2510e10fc:  e5 07 00 94   bl	_screenstream_initDisplayInfo
0x2510e1100:  40 19 00 35   cbnz	w0, loc_2510e1428 ; ⤵ 0x328
0x2510e1104:  80 8a 41 b9   ldr	w0, [x20, #0x188]
0x2510e1108:  88 36 40 f9   ldr	x8, [x20, #0x68]
0x2510e110c:  1f 04 00 71   cmp	w0, #0x1
0x2510e1110:  cb 00 00 54   b.lt	loc_2510e1128 ; ⤵ 0x18
0x2510e1114:  e3 63 02 91   add	x3, sp, #0x98
0x2510e1118:  e1 03 18 aa   mov	x1, x24
0x2510e111c:  e2 03 08 aa   mov	x2, x8
0x2510e1120:  b8 f7 fc 97   bl	_APVirtualDisplayTestSinkCreate
0x2510e1124:  05 00 00 14   b	loc_2510e1138 ; ⤵ 0x14
0x2510e1128:  ; loc_2510e1128
0x2510e1128:  e2 63 02 91   add	x2, sp, #0x98
0x2510e112c:  e0 03 18 aa   mov	x0, x24
0x2510e1130:  e1 03 08 aa   mov	x1, x8
0x2510e1134:  9d b7 fd 97   bl	_APVirtualDisplaySinkCreate
0x2510e1138:  ; loc_2510e1138
0x2510e1138:  fc 03 00 aa   mov	x28, x0
0x2510e113c:  00 19 00 35   cbnz	w0, loc_2510e145c ; ⤵ 0x320
0x2510e1140:  97 4a 40 f9   ldr	x23, [x20, #0x90]
0x2510e1144:  77 00 00 b5   cbnz	x23, loc_2510e1150 ; ⤵ 0xc
0x2510e1148:  56 c4 c0 95   bl	j__FigVirtualDisplaySourceScreenGetCurrent
0x2510e114c:  f7 03 00 aa   mov	x23, x0
0x2510e1150:  ; loc_2510e1150
0x2510e1150:  69 9d 0f b0   adrp	x9, 0x27048e000
0x2510e1154:  76 02 00 34   cbz	w22, loc_2510e11a0 ; ⤵ 0x4c
0x2510e1158:  88 de 45 39   ldrb	w8, [x20, #0x177]
0x2510e115c:  28 02 00 34   cbz	w8, loc_2510e11a0 ; ⤵ 0x44
0x2510e1160:  28 31 4b b9   ldr	w8, [x9, #0xb30]
0x2510e1164:  1f c9 00 71   cmp	w8, #0x32
0x2510e1168:  8c 01 00 54   b.gt	loc_2510e1198 ; ⤵ 0x30
0x2510e116c:  1f 05 00 31   cmn	w8, #0x1
0x2510e1170:  01 01 00 54   b.ne	loc_2510e1190 ; ⤵ 0x20
0x2510e1174:  60 9d 0f b0   adrp	x0, 0x27048e000
0x2510e1178:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e117c:  08 05 80 52   mov	w8, #0x28
0x2510e1180:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e1184:  01 29 00 11   add	w1, w8, #0xa
0x2510e1188:  0e 7f c0 95   bl	j___LogCategory_Initialize
0x2510e118c:  60 00 00 34   cbz	w0, loc_2510e1198 ; ⤵ 0xc
0x2510e1190:  ; loc_2510e1190
0x2510e1190:  e0 03 14 aa   mov	x0, x20
0x2510e1194:  10 5a 02 94   bl	_APEndpointStreamScreenCreate.cold.18
0x2510e1198:  ; loc_2510e1198
0x2510e1198:  28 00 80 52   mov	w8, #0x1
0x2510e119c:  88 b6 00 f9   str	x8, [x20, #0x168]
0x2510e11a0:  ; loc_2510e11a0
0x2510e11a0:  e0 03 18 aa   mov	x0, x24
0x2510e11a4:  19 56 02 94   bl	_screenstream_createVirtualDisplayActivationOptions
0x2510e11a8:  f6 03 00 aa   mov	x22, x0
0x2510e11ac:  e1 4f 40 f9   ldr	x1, [sp, #0x98]
0x2510e11b0:  e0 03 17 aa   mov	x0, x23
0x2510e11b4:  e2 03 16 aa   mov	x2, x22
0x2510e11b8:  26 c4 c0 95   bl	j__FigVirtualDisplaySessionActivateWithSourceAndSink
0x2510e11bc:  80 52 00 f9   str	x0, [x20, #0xa0]
0x2510e11c0:  40 15 00 b4   cbz	x0, loc_2510e1468 ; ⤵ 0x2a8
0x2510e11c4:  6f 82 c0 95   bl	j__CFRetain
0x2510e11c8:  2e b1 c0 95   bl	j__CMNotificationCenterGetDefaultLocalCenter
0x2510e11cc:  c8 7f 0b d0   adrp	x8, 0x2680db000
0x2510e11d0:  08 85 43 f9   ldr	x8, [x8, #0x708] ; _ptr.1e0c01538 (MediaToolbox/__DATA_CONST.__const)
0x2510e11d4:  03 01 40 f9   ldr	x3, [x8]
0x2510e11d8:  84 52 40 f9   ldr	x4, [x20, #0xa0]
0x2510e11dc:  10 00 00 90   adrp	x16, 0x2510e1000
0x2510e11e0:  10 b2 34 91   add	x16, x16, #0xd2c ; _screenstream_virtualDisplaySourceTerminated
0x2510e11e4:  f0 23 c1 da   paciza	x16
0x2510e11e8:  e2 03 10 aa   mov	x2, x16
0x2510e11ec:  e1 03 18 aa   mov	x1, x24
0x2510e11f0:  05 00 80 52   mov	w5, #0
0x2510e11f4:  1f b1 c0 95   bl	j__CMNotificationCenterAddListener
0x2510e11f8:  94 52 40 f9   ldr	x20, [x20, #0xa0]
0x2510e11fc:  e0 03 14 aa   mov	x0, x20
0x2510e1200:  18 aa c0 95   bl	j__CMBaseObjectGetVTable
0x2510e1204:  10 0c 41 f8   ldr	x16, [x0, #0x10]!
0x2510e1208:  f1 03 00 aa   mov	x17, x0
0x2510e120c:  31 ab e0 f2   movk	x17, #0x559, lsl #0x30
0x2510e1210:  30 1a c1 da   autda	x16, x17
0x2510e1214:  09 8e 42 f8   ldr	x9, [x16, #0x28]!
0x2510e1218:  69 02 00 b4   cbz	x9, loc_2510e1264 ; ⤵ 0x4c
0x2510e121c:  e8 03 10 aa   mov	x8, x16
0x2510e1220:  68 b6 ff f2   movk	x8, #0xfdb3, lsl #0x30
0x2510e1224:  f0 03 09 aa   mov	x16, x9
0x2510e1228:  f1 03 08 aa   mov	x17, x8
0x2510e122c:  30 12 c1 da   autia	x16, x17
0x2510e1230:  f1 03 10 aa   mov	x17, x16
0x2510e1234:  f1 43 c1 da   xpaci	x17
0x2510e1238:  1f 02 11 eb   cmp	x16, x17
0x2510e123c:  40 00 00 54   b.eq	loc_2510e1244 ; ⤵ 0x8
0x2510e1240:  00 8e 38 d4   brk	#0xc470
0x2510e1244:  ; loc_2510e1244
0x2510e1244:  f0 23 c1 da   paciza	x16
0x2510e1248:  f0 00 00 b4   cbz	x16, loc_2510e1264 ; ⤵ 0x1c
0x2510e124c:  e0 03 14 aa   mov	x0, x20
0x2510e1250:  28 09 3f d7   blraa	x9, x8
0x2510e1254:  fc 03 00 aa   mov	x28, x0
0x2510e1258:  80 00 00 35   cbnz	w0, loc_2510e1268 ; ⤵ 0x10
0x2510e125c:  96 01 00 b4   cbz	x22, loc_2510e128c ; ⤵ 0x30
0x2510e1260:  09 00 00 14   b	loc_2510e1284 ; ⤵ 0x24
0x2510e1264:  ; loc_2510e1264
0x2510e1264:  bc 3d 86 12   mov	w28, #0xffffce12
0x2510e1268:  ; loc_2510e1268
0x2510e1268:  21 09 00 d0   adrp	x1, 0x251207000
0x2510e126c:  21 a8 1f 91   add	x1, x1, #0x7ea ; "screenstream_activateVirtualDisplaySession"
0x2510e1270:  e0 03 1c aa   mov	x0, x28
0x2510e1274:  42 80 81 52   mov	w2, #0xc02
0x2510e1278:  03 00 80 52   mov	w3, #0
0x2510e127c:  39 ac c0 95   bl	j__APSLogErrorAt
0x2510e1280:  76 00 00 b4   cbz	x22, loc_2510e128c ; ⤵ 0xc
0x2510e1284:  ; loc_2510e1284
0x2510e1284:  e0 03 16 aa   mov	x0, x22
0x2510e1288:  3a 82 c0 95   bl	j__CFRelease
0x2510e128c:  ; loc_2510e128c
0x2510e128c:  e0 4f 40 f9   ldr	x0, [sp, #0x98]
0x2510e1290:  40 00 00 b4   cbz	x0, loc_2510e1298 ; ⤵ 0x8
0x2510e1294:  37 82 c0 95   bl	j__CFRelease
0x2510e1298:  ; loc_2510e1298
0x2510e1298:  5c 0d 00 35   cbnz	w28, loc_2510e1440 ; ⤵ 0x1a8
0x2510e129c:  28 00 80 52   mov	w8, #0x1
0x2510e12a0:  28 67 00 39   strb	w8, [x25, #0x19]
0x2510e12a4:  68 9d 0f b0   adrp	x8, 0x27048e000
0x2510e12a8:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x2510e12ac:  1f cd 00 71   cmp	w8, #0x33
0x2510e12b0:  2b 04 00 54   b.lt	loc_2510e1334 ; ⤵ 0x84
0x2510e12b4:  ; loc_2510e12b4
0x2510e12b4:  1c 00 80 52   mov	w28, #0
0x2510e12b8:  e8 47 40 f9   ldr	x8, [sp, #0x88]
0x2510e12bc:  e9 17 40 f9   ldr	x9, [sp, #0x28]
0x2510e12c0:  28 01 00 f9   str	x8, [x9]
0x2510e12c4:  ff 47 00 f9   str	xzr, [sp, #0x88]
0x2510e12c8:  ; loc_2510e12c8
0x2510e12c8:  e0 43 40 f9   ldr	x0, [sp, #0x80]
0x2510e12cc:  40 00 00 b4   cbz	x0, loc_2510e12d4 ; ⤵ 0x8
0x2510e12d0:  28 82 c0 95   bl	j__CFRelease
0x2510e12d4:  ; loc_2510e12d4
0x2510e12d4:  e0 3b 40 f9   ldr	x0, [sp, #0x70]
0x2510e12d8:  40 00 00 b4   cbz	x0, loc_2510e12e0 ; ⤵ 0x8
0x2510e12dc:  25 82 c0 95   bl	j__CFRelease
0x2510e12e0:  ; loc_2510e12e0
0x2510e12e0:  e0 3f 40 f9   ldr	x0, [sp, #0x78]
0x2510e12e4:  40 00 00 b4   cbz	x0, loc_2510e12ec ; ⤵ 0x8
0x2510e12e8:  22 82 c0 95   bl	j__CFRelease
0x2510e12ec:  ; loc_2510e12ec
0x2510e12ec:  e0 47 40 f9   ldr	x0, [sp, #0x88]
0x2510e12f0:  40 00 00 b4   cbz	x0, loc_2510e12f8 ; ⤵ 0x8
0x2510e12f4:  1f 82 c0 95   bl	j__CFRelease
0x2510e12f8:  ; loc_2510e12f8
0x2510e12f8:  a8 03 5a f8   ldur	x8, [fp, #-0x60]
0x2510e12fc:  09 86 0b f0   adrp	x9, 0x2681a4000
0x2510e1300:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.1e6ef16f0 (libsystem_c.dylib/__DATA_DIRTY.__common)
0x2510e1304:  29 01 40 f9   ldr	x9, [x9]
0x2510e1308:  3f 01 08 eb   cmp	x9, x8
0x2510e130c:  c1 15 00 54   b.ne	loc_2510e15c4 ; ⤵ 0x2b8
0x2510e1310:  e0 03 1c aa   mov	x0, x28
0x2510e1314:  fd 7b 54 a9   ldp	fp, lr, [sp, #0x140]
0x2510e1318:  f4 4f 53 a9   ldp	x20, x19, [sp, #0x130]
0x2510e131c:  f6 57 52 a9   ldp	x22, x21, [sp, #0x120]
0x2510e1320:  f8 5f 51 a9   ldp	x24, x23, [sp, #0x110]
0x2510e1324:  fa 67 50 a9   ldp	x26, x25, [sp, #0x100]
0x2510e1328:  fc 6f 4f a9   ldp	x28, x27, [sp, #0xf0]
0x2510e132c:  ff 43 05 91   add	sp, sp, #0x150
0x2510e1330:  ff 0f 5f d6   retab
0x2510e1334:  ; loc_2510e1334
0x2510e1334:  1f 05 00 31   cmn	w8, #0x1
0x2510e1338:  01 01 00 54   b.ne	loc_2510e1358 ; ⤵ 0x20
0x2510e133c:  60 9d 0f b0   adrp	x0, 0x27048e000
0x2510e1340:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e1344:  08 05 80 52   mov	w8, #0x28
0x2510e1348:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e134c:  01 29 00 11   add	w1, w8, #0xa
0x2510e1350:  9c 7e c0 95   bl	j___LogCategory_Initialize
0x2510e1354:  a0 00 00 34   cbz	w0, loc_2510e1368 ; ⤵ 0x14
0x2510e1358:  ; loc_2510e1358
0x2510e1358:  e0 03 1a aa   mov	x0, x26
0x2510e135c:  e1 03 19 aa   mov	x1, x25
0x2510e1360:  e2 03 18 aa   mov	x2, x24
0x2510e1364:  b5 59 02 94   bl	_APEndpointStreamScreenCreate.cold.20
0x2510e1368:  ; loc_2510e1368
0x2510e1368:  e0 5f 02 91   add	x0, sp, #0x97
0x2510e136c:  e2 23 02 91   add	x2, sp, #0x88
0x2510e1370:  e1 03 15 aa   mov	x1, x21
0x2510e1374:  e3 0f 40 f9   ldr	x3, [sp, #0x18]
0x2510e1378:  c6 59 02 94   bl	_APEndpointStreamScreenCreate.cold.21
0x2510e137c:  ce ff ff 17   b	loc_2510e12b4 ; ⤴ -0xc8
0x2510e1380:  ; loc_2510e1380
0x2510e1380:  e0 03 1a aa   mov	x0, x26
0x2510e1384:  e1 03 16 aa   mov	x1, x22
0x2510e1388:  66 59 02 94   bl	_APEndpointStreamScreenCreate.cold.15
0x2510e138c:  38 ff ff 17   b	loc_2510e106c ; ⤴ -0x320
0x2510e1390:  ; loc_2510e1390
0x2510e1390:  e0 03 1a aa   mov	x0, x26
0x2510e1394:  78 59 02 94   bl	_APEndpointStreamScreenCreate.cold.16
0x2510e1398:  54 ff ff 17   b	loc_2510e10e8 ; ⤴ -0x2b0
0x2510e139c:  ; loc_2510e139c
0x2510e139c:  21 63 06 91   add	x1, x25, #0x198
0x2510e13a0:  e0 03 1a aa   mov	x0, x26
0x2510e13a4:  0f 59 02 94   bl	_APEndpointStreamScreenCreate.cold.7
0x2510e13a8:  56 fe ff 17   b	loc_2510e0d00 ; ⤴ -0x6a8
0x2510e13ac:  ; loc_2510e13ac
0x2510e13ac:  10 5a 02 94   bl	_APEndpointStreamScreenCreate.cold.28
0x2510e13b0:  0a 00 00 14   b	loc_2510e13d8 ; ⤵ 0x28
0x2510e13b4:  ; loc_2510e13b4
0x2510e13b4:  07 5a 02 94   bl	_APEndpointStreamScreenCreate.cold.27
0x2510e13b8:  08 00 00 14   b	loc_2510e13d8 ; ⤵ 0x20
0x2510e13bc:  ; loc_2510e13bc
0x2510e13bc:  fe 59 02 94   bl	_APEndpointStreamScreenCreate.cold.26
0x2510e13c0:  06 00 00 14   b	loc_2510e13d8 ; ⤵ 0x18
0x2510e13c4:  ; loc_2510e13c4
0x2510e13c4:  f5 59 02 94   bl	_APEndpointStreamScreenCreate.cold.25
0x2510e13c8:  04 00 00 14   b	loc_2510e13d8 ; ⤵ 0x10
0x2510e13cc:  ; loc_2510e13cc
0x2510e13cc:  ea 59 02 94   bl	_APEndpointStreamScreenCreate.cold.24
0x2510e13d0:  02 00 00 14   b	loc_2510e13d8 ; ⤵ 0x8
0x2510e13d4:  ; loc_2510e13d4
0x2510e13d4:  bc 58 02 94   bl	_APEndpointStreamScreenCreate.cold.1
0x2510e13d8:  ; loc_2510e13d8
0x2510e13d8:  13 00 80 d2   mov	x19, #0
0x2510e13dc:  ea fe ff 17   b	loc_2510e0f84 ; ⤴ -0x458
0x2510e13e0:  ; loc_2510e13e0
0x2510e13e0:  fc 03 00 aa   mov	x28, x0
0x2510e13e4:  c1 58 02 94   bl	_APEndpointStreamScreenCreate.cold.2
0x2510e13e8:  13 00 80 d2   mov	x19, #0
0x2510e13ec:  12 fa ff 17   b	loc_2510dfc34 ; ⤴ -0x17b8
0x2510e13f0:  ; loc_2510e13f0
0x2510e13f0:  cd 59 02 94   bl	_APEndpointStreamScreenCreate.cold.22
0x2510e13f4:  e4 fe ff 17   b	loc_2510e0f84 ; ⤴ -0x470
0x2510e13f8:  ; loc_2510e13f8
0x2510e13f8:  fc 03 00 aa   mov	x28, x0
0x2510e13fc:  ce 58 02 94   bl	_APEndpointStreamScreenCreate.cold.4
0x2510e1400:  0d fa ff 17   b	loc_2510dfc34 ; ⤴ -0x17cc
0x2510e1404:  ; loc_2510e1404
0x2510e1404:  df 58 02 94   bl	_APEndpointStreamScreenCreate.cold.5
0x2510e1408:  3c 2f 88 12   mov	w28, #0xffffbe86
0x2510e140c:  27 00 00 14   b	loc_2510e14a8 ; ⤵ 0x9c
0x2510e1410:  ; loc_2510e1410
0x2510e1410:  fc 03 00 aa   mov	x28, x0
0x2510e1414:  02 59 02 94   bl	_APEndpointStreamScreenCreate.cold.8
0x2510e1418:  24 00 00 14   b	loc_2510e14a8 ; ⤵ 0x90
0x2510e141c:  ; loc_2510e141c
0x2510e141c:  fc 03 00 aa   mov	x28, x0
0x2510e1420:  09 59 02 94   bl	_APEndpointStreamScreenCreate.cold.9
0x2510e1424:  21 00 00 14   b	loc_2510e14a8 ; ⤵ 0x84
0x2510e1428:  ; loc_2510e1428
0x2510e1428:  fc 03 00 aa   mov	x28, x0
0x2510e142c:  21 09 00 d0   adrp	x1, 0x251207000
0x2510e1430:  21 a8 1f 91   add	x1, x1, #0x7ea ; "screenstream_activateVirtualDisplaySession"
0x2510e1434:  62 7c 81 52   mov	w2, #0xbe3
0x2510e1438:  03 00 80 52   mov	w3, #0
0x2510e143c:  c9 ab c0 95   bl	j__APSLogErrorAt
0x2510e1440:  ; loc_2510e1440
0x2510e1440:  21 09 00 d0   adrp	x1, 0x251207000
0x2510e1444:  21 78 09 91   add	x1, x1, #0x25e ; "screenstream_activate"
0x2510e1448:  e0 03 1c aa   mov	x0, x28
0x2510e144c:  62 8a 81 52   mov	w2, #0xc53
0x2510e1450:  03 00 80 52   mov	w3, #0
0x2510e1454:  c3 ab c0 95   bl	j__APSLogErrorAt
0x2510e1458:  14 00 00 14   b	loc_2510e14a8 ; ⤵ 0x50
0x2510e145c:  ; loc_2510e145c
0x2510e145c:  e0 03 1c aa   mov	x0, x28
0x2510e1460:  53 59 02 94   bl	_APEndpointStreamScreenCreate.cold.17
0x2510e1464:  8a ff ff 17   b	loc_2510e128c ; ⤴ -0x1d8
0x2510e1468:  ; loc_2510e1468
0x2510e1468:  69 59 02 94   bl	_APEndpointStreamScreenCreate.cold.19
0x2510e146c:  1c 2f 88 12   mov	w28, #0xffffbe87
0x2510e1470:  b6 f0 ff b5   cbnz	x22, loc_2510e1284 ; ⤴ -0x1ec
0x2510e1474:  86 ff ff 17   b	loc_2510e128c ; ⤴ -0x1e8
0x2510e1478:  ; loc_2510e1478
0x2510e1478:  a6 58 02 94   bl	_APEndpointStreamScreenCreate.cold.3
0x2510e147c:  c2 fe ff 17   b	loc_2510e0f84 ; ⤴ -0x4f8
0x2510e1480:  ; loc_2510e1480
0x2510e1480:  fb 58 02 94   bl	_APEndpointStreamScreenCreate.cold.10
0x2510e1484:  08 00 00 14   b	loc_2510e14a4 ; ⤵ 0x20
0x2510e1488:  ; loc_2510e1488
0x2510e1488:  02 59 02 94   bl	_APEndpointStreamScreenCreate.cold.11
0x2510e148c:  06 00 00 14   b	loc_2510e14a4 ; ⤵ 0x18
0x2510e1490:  ; loc_2510e1490
0x2510e1490:  09 59 02 94   bl	_APEndpointStreamScreenCreate.cold.12
0x2510e1494:  04 00 00 14   b	loc_2510e14a4 ; ⤵ 0x10
0x2510e1498:  ; loc_2510e1498
0x2510e1498:  10 59 02 94   bl	_APEndpointStreamScreenCreate.cold.13
0x2510e149c:  02 00 00 14   b	loc_2510e14a4 ; ⤵ 0x8
0x2510e14a0:  ; loc_2510e14a0
0x2510e14a0:  17 59 02 94   bl	_APEndpointStreamScreenCreate.cold.14
0x2510e14a4:  ; loc_2510e14a4
0x2510e14a4:  fc 2e 88 12   mov	w28, #0xffffbe88
0x2510e14a8:  ; loc_2510e14a8
0x2510e14a8:  68 9d 0f b0   adrp	x8, 0x27048e000
0x2510e14ac:  08 31 4b b9   ldr	w8, [x8, #0xb30] ; _gLogCategory_APEndpointStreamScreen
0x2510e14b0:  1f 69 01 71   cmp	w8, #0x5a
0x2510e14b4:  ec 02 00 54   b.gt	loc_2510e1510 ; ⤵ 0x5c
0x2510e14b8:  1f 05 00 31   cmn	w8, #0x1
0x2510e14bc:  c0 01 00 54   b.eq	loc_2510e14f4 ; ⤵ 0x38
0x2510e14c0:  ; loc_2510e14c0
0x2510e14c0:  28 17 40 f9   ldr	x8, [x25, #0x28]
0x2510e14c4:  e8 73 00 a9   stp	x8, x28, [sp]
0x2510e14c8:  60 9d 0f b0   adrp	x0, 0x27048e000
0x2510e14cc:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e14d0:  21 09 00 d0   adrp	x1, 0x251207000
0x2510e14d4:  21 d0 09 91   add	x1, x1, #0x274 ; "OSStatus screenstream_activate(FigEndpointStreamRef, CFDictionaryRef)"
0x2510e14d8:  23 09 00 d0   adrp	x3, 0x251207000
0x2510e14dc:  63 98 0d 91   add	x3, x3, #0x366 ; "%@ ### activation failed: %#m\n"
0x2510e14e0:  08 05 80 52   mov	w8, #0x28
0x2510e14e4:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e14e8:  02 c9 00 11   add	w2, w8, #0x32
0x2510e14ec:  85 7e c0 95   bl	j__LogPrintF
0x2510e14f0:  08 00 00 14   b	loc_2510e1510 ; ⤵ 0x20
0x2510e14f4:  ; loc_2510e14f4
0x2510e14f4:  60 9d 0f b0   adrp	x0, 0x27048e000
0x2510e14f8:  00 c0 2c 91   add	x0, x0, #0xb30 ; _gLogCategory_APEndpointStreamScreen
0x2510e14fc:  08 05 80 52   mov	w8, #0x28
0x2510e1500:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x2510e1504:  01 c9 00 11   add	w1, w8, #0x32
0x2510e1508:  2e 7e c0 95   bl	j___LogCategory_Initialize
0x2510e150c:  a0 fd ff 35   cbnz	w0, loc_2510e14c0 ; ⤴ -0x4c
0x2510e1510:  ; loc_2510e1510
0x2510e1510:  e0 03 18 aa   mov	x0, x24
0x2510e1514:  79 01 00 94   bl	_screenstream_cleanup
0x2510e1518:  21 09 00 90   adrp	x1, 0x251205000
0x2510e151c:  21 b4 0b 91   add	x1, x1, #0x2ed ; "APEndpointStreamScreenCreate"
0x2510e1520:  e0 03 1c aa   mov	x0, x28
0x2510e1524:  e2 c5 81 52   mov	w2, #0xe2f
0x2510e1528:  c1 f9 ff 17   b	loc_2510dfc2c ; ⤴ -0x18fc
0x2510e152c:  ; loc_2510e152c
0x2510e152c:  29 a3 44 b9   ldr	w9, [x25, #0x4a0]
0x2510e1530:  7e fa ff 17   b	loc_2510dff28 ; ⤴ -0x1608
0x2510e1534:  ; loc_2510e1534
0x2510e1534:  28 2b 4f 39   ldrb	w8, [x25, #0x3ca]
0x2510e1538:  b6 fa ff 17   b	loc_2510e0010 ; ⤴ -0x1528
0x2510e153c:  ; loc_2510e153c
0x2510e153c:  28 2f 4f 39   ldrb	w8, [x25, #0x3cb]
0x2510e1540:  da fa ff 17   b	loc_2510e00a8 ; ⤴ -0x1498
0x2510e1544:  ; loc_2510e1544
0x2510e1544:  20 c3 43 b9   ldr	w0, [x25, #0x3c0]
0x2510e1548:  62 fb ff 17   b	loc_2510e02d0 ; ⤴ -0x1278
0x2510e154c:  ; loc_2510e154c
0x2510e154c:  28 13 4f 39   ldrb	w8, [x25, #0x3c4]
0x2510e1550:  7f fb ff 17   b	loc_2510e034c ; ⤴ -0x1204
0x2510e1554:  ; loc_2510e1554
0x2510e1554:  28 23 4f 39   ldrb	w8, [x25, #0x3c8]
0x2510e1558:  a2 fb ff 17   b	loc_2510e03e0 ; ⤴ -0x1178
0x2510e155c:  ; loc_2510e155c
0x2510e155c:  28 27 4f 39   ldrb	w8, [x25, #0x3c9]
0x2510e1560:  c6 fb ff 17   b	loc_2510e0478 ; ⤴ -0x10e8
0x2510e1564:  ; loc_2510e1564
0x2510e1564:  28 d3 45 39   ldrb	w8, [x25, #0x174]
0x2510e1568:  e9 fb ff 17   b	loc_2510e050c ; ⤴ -0x105c
0x2510e156c:  ; loc_2510e156c
0x2510e156c:  28 d7 45 39   ldrb	w8, [x25, #0x175]
0x2510e1570:  0c fc ff 17   b	loc_2510e05a0 ; ⤴ -0xfd0
0x2510e1574:  ; loc_2510e1574
0x2510e1574:  28 db 45 39   ldrb	w8, [x25, #0x176]
0x2510e1578:  2f fc ff 17   b	loc_2510e0634 ; ⤴ -0xf44
0x2510e157c:  ; loc_2510e157c
0x2510e157c:  20 d7 40 fd   ldr	d0, [x25, #0x1a8]
0x2510e1580:  b5 fc ff 17   b	loc_2510e0854 ; ⤴ -0xd2c
0x2510e1584:  ; loc_2510e1584
0x2510e1584:  20 db 40 fd   ldr	d0, [x25, #0x1b0]
0x2510e1588:  d2 fc ff 17   b	loc_2510e08d0 ; ⤴ -0xcb8
0x2510e158c:  ; loc_2510e158c
0x2510e158c:  28 e3 45 39   ldrb	w8, [x25, #0x178]
0x2510e1590:  f0 fc ff 17   b	loc_2510e0950 ; ⤴ -0xc40
0x2510e1594:  ; loc_2510e1594
0x2510e1594:  20 d7 41 b9   ldr	w0, [x25, #0x1d4]
0x2510e1598:  6e fc ff 17   b	loc_2510e0750 ; ⤴ -0xe48
0x2510e159c:  ; loc_2510e159c
0x2510e159c:  20 7f 41 b9   ldr	w0, [x25, #0x17c]
0x2510e15a0:  12 fd ff 17   b	loc_2510e09e8 ; ⤴ -0xbb8
0x2510e15a4:  ; loc_2510e15a4
0x2510e15a4:  20 8f 41 b9   ldr	w0, [x25, #0x18c]
0x2510e15a8:  50 fd ff 17   b	loc_2510e0ae8 ; ⤴ -0xac0
0x2510e15ac:  ; loc_2510e15ac
0x2510e15ac:  20 e3 40 fd   ldr	d0, [x25, #0x1c0]
0x2510e15b0:  47 fc ff 17   b	loc_2510e06cc ; ⤴ -0xee4
0x2510e15b4:  ; loc_2510e15b4
0x2510e15b4:  20 a7 44 b9   ldr	w0, [x25, #0x4a4]
0x2510e15b8:  9c fd ff 17   b	loc_2510e0c28 ; ⤴ -0x990
0x2510e15bc:  ; loc_2510e15bc
0x2510e15bc:  20 8b 41 b9   ldr	w0, [x25, #0x188]
0x2510e15c0:  2c fd ff 17   b	loc_2510e0a70 ; ⤴ -0xb50
0x2510e15c4:  ; loc_2510e15c4
0x2510e15c4:  bb 7b c0 95   bl	j____stack_chk_fail
0x2510e15c8:  ; loc_2510e15c8
0x2510e15c8:  fc 03 00 aa   mov	x28, x0
0x2510e15cc:  94 f9 ff 17   b	loc_2510dfc1c ; ⤴ -0x19b0

