
-[APRKPlayerItem initWithDictionary:resourceLoaderHelper:contentKeyHelper:options:]
0x250effa04:  7f 23 03 d5   pacibsp
0x250effa08:  ff 83 03 d1   sub	sp, sp, #0xe0
0x250effa0c:  fc 6f 08 a9   stp	x28, x27, [sp, #0x80]
0x250effa10:  fa 67 09 a9   stp	x26, x25, [sp, #0x90]
0x250effa14:  f8 5f 0a a9   stp	x24, x23, [sp, #0xa0]
0x250effa18:  f6 57 0b a9   stp	x22, x21, [sp, #0xb0]
0x250effa1c:  f4 4f 0c a9   stp	x20, x19, [sp, #0xc0]
0x250effa20:  fd 7b 0d a9   stp	fp, lr, [sp, #0xd0]
0x250effa24:  fd 43 03 91   add	fp, sp, #0xd0
0x250effa28:  f6 03 05 aa   mov	x22, x5
0x250effa2c:  f5 03 04 aa   mov	x21, x4
0x250effa30:  f4 03 03 aa   mov	x20, x3
0x250effa34:  fb 03 00 aa   mov	x27, x0
0x250effa38:  0e 03 c8 95   bl	_objc_retain_x2_stub
0x250effa3c:  f3 03 00 aa   mov	x19, x0
0x250effa40:  b4 04 c8 95   bl	_objc_retain_x20_stub
0x250effa44:  f4 03 00 aa   mov	x20, x0
0x250effa48:  0e 03 c8 95   bl	_objc_retain_x21_stub
0x250effa4c:  f5 03 00 aa   mov	x21, x0
0x250effa50:  10 03 c8 95   bl	_objc_retain_x22_stub
0x250effa54:  f6 03 00 aa   mov	x22, x0
0x250effa58:  53 08 00 b4   cbz	x19, loc_250effb60 ; ⤵ 0x108
0x250effa5c:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effa60:  08 5d 47 f9   ldr	x8, [x8, #0xeb8] ; _ptr._kFigEndpointPlaybackSessionKey_UUID
0x250effa64:  02 01 40 f9   ldr	x2, [x8]
0x250effa68:  e0 03 13 aa   mov	x0, x19
0x250effa6c:  e1 07 c4 95   bl	0x2580019f0
0x250effa70:  8c 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effa74:  f7 03 00 aa   mov	x23, x0
0x250effa78:  80 07 00 b4   cbz	x0, loc_250effb68 ; ⤵ 0xf0
0x250effa7c:  ff 7f 06 a9   stp	xzr, xzr, [sp, #0x60]
0x250effa80:  ff 3b 00 f9   str	xzr, [sp, #0x70]
0x250effa84:  08 8e 0b f0   adrp	x8, 0x2680c2000
0x250effa88:  00 a9 47 f9   ldr	x0, [x8, #0xf50] ; _ptr.NSMutableDictionary
0x250effa8c:  85 14 c4 95   bl	0x258004ca0
0x250effa90:  84 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effa94:  f8 03 00 aa   mov	x24, x0
0x250effa98:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effa9c:  08 25 47 f9   ldr	x8, [x8, #0xe48] ; _ptr._kFigEndpointPlaybackSessionKey_ReferenceRestrictions
0x250effaa0:  02 01 40 f9   ldr	x2, [x8]
0x250effaa4:  e0 03 13 aa   mov	x0, x19
0x250effaa8:  d2 07 c4 95   bl	0x2580019f0
0x250effaac:  7d 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effab0:  f9 03 00 aa   mov	x25, x0
0x250effab4:  1f 09 c4 95   bl	0x258001f30
0x250effab8:  fa 03 00 aa   mov	x26, x0
0x250effabc:  cd 02 c8 95   bl	_objc_release_x25_stub
0x250effac0:  9c 8e 0b 90   adrp	x28, 0x2680cf000
0x250effac4:  80 47 43 f9   ldr	x0, [x28, #0x688] ; _ptr.NSNumber
0x250effac8:  e2 03 1a aa   mov	x2, x26
0x250effacc:  59 09 c4 95   bl	0x258002030
0x250effad0:  74 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effad4:  f9 03 00 aa   mov	x25, x0
0x250effad8:  68 8f 0b d0   adrp	x8, 0x2680ed000
0x250effadc:  08 3d 43 f9   ldr	x8, [x8, #0x678] ; _ptr._AVURLAssetReferenceRestrictionsKey
0x250effae0:  03 01 40 f9   ldr	x3, [x8]
0x250effae4:  e0 03 18 aa   mov	x0, x24
0x250effae8:  e2 03 19 aa   mov	x2, x25
0x250effaec:  d1 07 c4 95   bl	0x258001a30
0x250effaf0:  c0 02 c8 95   bl	_objc_release_x25_stub
0x250effaf4:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effaf8:  08 0d 47 f9   ldr	x8, [x8, #0xe18] ; _ptr._kFigEndpointPlaybackSessionKey_OutOfBandAlternateTracks
0x250effafc:  02 01 40 f9   ldr	x2, [x8]
0x250effb00:  e0 03 13 aa   mov	x0, x19
0x250effb04:  bb 07 c4 95   bl	0x2580019f0
0x250effb08:  66 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effb0c:  f9 03 00 aa   mov	x25, x0
0x250effb10:  e0 00 00 b4   cbz	x0, loc_250effb2c ; ⤵ 0x1c
0x250effb14:  68 8f 0b d0   adrp	x8, 0x2680ed000
0x250effb18:  08 2d 43 f9   ldr	x8, [x8, #0x658] ; _ptr._AVURLAssetOutOfBandAlternateTracksKey
0x250effb1c:  03 01 40 f9   ldr	x3, [x8]
0x250effb20:  e0 03 18 aa   mov	x0, x24
0x250effb24:  e2 03 19 aa   mov	x2, x25
0x250effb28:  c2 07 c4 95   bl	0x258001a30
0x250effb2c:  ; loc_250effb2c
0x250effb2c:  f9 17 00 f9   str	x25, [sp, #0x28]
0x250effb30:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effb34:  08 ed 46 f9   ldr	x8, [x8, #0xdd8] ; _ptr._kFigEndpointPlaybackSessionKey_InheritURIQueryComponentFromReferencingURI
0x250effb38:  02 01 40 f9   ldr	x2, [x8]
0x250effb3c:  e0 03 13 aa   mov	x0, x19
0x250effb40:  ac 07 c4 95   bl	0x2580019f0
0x250effb44:  57 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effb48:  99 47 43 f9   ldr	x25, [x28, #0x688]
0x250effb4c:  e0 13 00 f9   str	x0, [sp, #0x20]
0x250effb50:  00 01 00 b4   cbz	x0, loc_250effb70 ; ⤵ 0x20
0x250effb54:  9f 05 c4 95   bl	0x2580011d0
0x250effb58:  e2 03 00 aa   mov	x2, x0
0x250effb5c:  06 00 00 14   b	loc_250effb74 ; ⤵ 0x18
0x250effb60:  ; loc_250effb60
0x250effb60:  19 00 80 d2   mov	x25, #0
0x250effb64:  2c 01 00 14   b	loc_250f00014 ; ⤵ 0x4b0
0x250effb68:  ; loc_250effb68
0x250effb68:  19 00 80 d2   mov	x25, #0
0x250effb6c:  29 01 00 14   b	loc_250f00010 ; ⤵ 0x4a4
0x250effb70:  ; loc_250effb70
0x250effb70:  02 00 80 52   mov	w2, #0
0x250effb74:  ; loc_250effb74
0x250effb74:  e0 03 19 aa   mov	x0, x25
0x250effb78:  be 05 c4 95   bl	0x258001270
0x250effb7c:  49 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effb80:  f9 03 00 aa   mov	x25, x0
0x250effb84:  68 8f 0b d0   adrp	x8, 0x2680ed000
0x250effb88:  08 25 43 f9   ldr	x8, [x8, #0x648] ; _ptr._AVURLAssetInheritURIQueryComponentFromReferencingURIKey
0x250effb8c:  03 01 40 f9   ldr	x3, [x8]
0x250effb90:  e0 03 18 aa   mov	x0, x24
0x250effb94:  e2 03 19 aa   mov	x2, x25
0x250effb98:  a6 07 c4 95   bl	0x258001a30
0x250effb9c:  95 02 c8 95   bl	_objc_release_x25_stub
0x250effba0:  96 00 00 b4   cbz	x22, loc_250effbb0 ; ⤵ 0x10
0x250effba4:  e0 03 18 aa   mov	x0, x24
0x250effba8:  e2 03 16 aa   mov	x2, x22
0x250effbac:  a5 25 c4 95   bl	0x258009240
0x250effbb0:  ; loc_250effbb0
0x250effbb0:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effbb4:  08 e5 46 f9   ldr	x8, [x8, #0xdc8] ; _ptr._kFigEndpointPlaybackSessionKey_HLSContentLocation
0x250effbb8:  02 01 40 f9   ldr	x2, [x8]
0x250effbbc:  e0 03 13 aa   mov	x0, x19
0x250effbc0:  8c 07 c4 95   bl	0x2580019f0
0x250effbc4:  37 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effbc8:  fa 03 00 aa   mov	x26, x0
0x250effbcc:  68 24 0c d0   adrp	x8, 0x26938d000
0x250effbd0:  00 4d 45 f9   ldr	x0, [x8, #0xa98] ; "XhO\xf1"
0x250effbd4:  13 87 c4 95   bl	0x258021820
0x250effbd8:  32 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effbdc:  fc 03 00 aa   mov	x28, x0
0x250effbe0:  a0 85 c4 95   bl	0x258021260
0x250effbe4:  f9 03 00 aa   mov	x25, x0
0x250effbe8:  8e 02 c8 95   bl	_objc_release_x28_stub
0x250effbec:  5a 00 00 b4   cbz	x26, loc_250effbf4 ; ⤵ 0x8
0x250effbf0:  79 01 00 b5   cbnz	x25, loc_250effc1c ; ⤵ 0x2c
0x250effbf4:  ; loc_250effbf4
0x250effbf4:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effbf8:  08 cd 46 f9   ldr	x8, [x8, #0xd98] ; _ptr._kFigEndpointPlaybackSessionKey_ContentLocation
0x250effbfc:  02 01 40 f9   ldr	x2, [x8]
0x250effc00:  e0 03 13 aa   mov	x0, x19
0x250effc04:  7b 07 c4 95   bl	0x2580019f0
0x250effc08:  26 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effc0c:  f9 03 00 aa   mov	x25, x0
0x250effc10:  7c 02 c8 95   bl	_objc_release_x26_stub
0x250effc14:  fa 03 19 aa   mov	x26, x25
0x250effc18:  39 07 00 b4   cbz	x25, loc_250effcfc ; ⤵ 0xe4
0x250effc1c:  ; loc_250effc1c
0x250effc1c:  08 8e 0b f0   adrp	x8, 0x2680c2000
0x250effc20:  00 ed 47 f9   ldr	x0, [x8, #0xfd8] ; _ptr.NSURL
0x250effc24:  e2 03 1a aa   mov	x2, x26
0x250effc28:  a2 09 c4 95   bl	0x2580022b0
0x250effc2c:  1d 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effc30:  60 06 00 b4   cbz	x0, loc_250effcfc ; ⤵ 0xcc
0x250effc34:  f9 03 00 aa   mov	x25, x0
0x250effc38:  68 8f 0b d0   adrp	x8, 0x2680ed000
0x250effc3c:  00 35 45 f9   ldr	x0, [x8, #0xa68] ; _ptr.AVURLAsset
0x250effc40:  bc 04 c8 95   bl	_objc_alloc_stub
0x250effc44:  e2 03 19 aa   mov	x2, x25
0x250effc48:  e3 03 18 aa   mov	x3, x24
0x250effc4c:  0d 86 c4 95   bl	0x258021480
0x250effc50:  f9 03 01 a9   stp	x25, x0, [sp, #0x10]
0x250effc54:  f4 07 00 f9   str	x20, [sp, #0x8]
0x250effc58:  20 07 00 b4   cbz	x0, loc_250effd3c ; ⤵ 0xe4
0x250effc5c:  fc 03 00 aa   mov	x28, x0
0x250effc60:  e0 03 14 aa   mov	x0, x20
0x250effc64:  e2 03 1c aa   mov	x2, x28
0x250effc68:  02 87 c4 95   bl	0x258021870
0x250effc6c:  e0 03 15 aa   mov	x0, x21
0x250effc70:  e2 03 1c aa   mov	x2, x28
0x250effc74:  ff 86 c4 95   bl	0x258021870
0x250effc78:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effc7c:  08 b1 46 f9   ldr	x8, [x8, #0xd60] ; _ptr._kFigEndpointPlaybackSessionKey_AllowedCommonMediaClientDataKeys
0x250effc80:  02 01 40 f9   ldr	x2, [x8]
0x250effc84:  e0 03 13 aa   mov	x0, x19
0x250effc88:  5a 07 c4 95   bl	0x2580019f0
0x250effc8c:  05 04 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effc90:  f4 03 00 aa   mov	x20, x0
0x250effc94:  5b 02 c8 95   bl	_objc_release_x26_stub
0x250effc98:  b4 05 00 b4   cbz	x20, loc_250effd4c ; ⤵ 0xb4
0x250effc9c:  e0 03 14 aa   mov	x0, x20
0x250effca0:  b0 1d c4 95   bl	0x258007360
0x250effca4:  f9 03 00 aa   mov	x25, x0
0x250effca8:  e0 03 1c aa   mov	x0, x28
0x250effcac:  11 87 c4 95   bl	0x2580218f0
0x250effcb0:  fc 03 c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250effcb4:  fa 03 00 aa   mov	x26, x0
0x250effcb8:  e2 03 19 aa   mov	x2, x25
0x250effcbc:  5d 87 c4 95   bl	0x258021a30
0x250effcc0:  50 02 c8 95   bl	_objc_release_x26_stub
0x250effcc4:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250effcc8:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250effccc:  1f 79 00 71   cmp	w8, #0x1e
0x250effcd0:  2c 01 00 54   b.gt	loc_250effcf4 ; ⤵ 0x24
0x250effcd4:  1f 05 00 31   cmn	w8, #0x1
0x250effcd8:  e1 1b 00 54   b.ne	loc_250f00054 ; ⤵ 0x37c
0x250effcdc:  a0 c2 0e 90   adrp	x0, 0x26e753000
0x250effce0:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250effce4:  c1 03 80 52   mov	w1, #0x1e
0x250effce8:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250effcec:  35 04 c8 95   bl	__LogCategory_Initialize_stub
0x250effcf0:  20 1b 00 35   cbnz	w0, loc_250f00054 ; ⤵ 0x364
0x250effcf4:  ; loc_250effcf4
0x250effcf4:  f9 0b 40 f9   ldr	x25, [sp, #0x10]
0x250effcf8:  15 00 00 14   b	loc_250effd4c ; ⤵ 0x54
0x250effcfc:  ; loc_250effcfc
0x250effcfc:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250effd00:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250effd04:  1f 69 01 71   cmp	w8, #0x5a
0x250effd08:  4c 01 00 54   b.gt	loc_250effd30 ; ⤵ 0x28
0x250effd0c:  1f 05 00 31   cmn	w8, #0x1
0x250effd10:  e1 19 00 54   b.ne	loc_250f0004c ; ⤵ 0x33c
0x250effd14:  c8 03 80 52   mov	w8, #0x1e
0x250effd18:  08 40 a0 72   movk	w8, #0x200, lsl #0x10
0x250effd1c:  a0 c2 0e 90   adrp	x0, 0x26e753000
0x250effd20:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250effd24:  01 f1 00 11   add	w1, w8, #0x3c
0x250effd28:  26 04 c8 95   bl	__LogCategory_Initialize_stub
0x250effd2c:  00 19 00 35   cbnz	w0, loc_250f0004c ; ⤵ 0x320
0x250effd30:  ; loc_250effd30
0x250effd30:  19 00 80 d2   mov	x25, #0
0x250effd34:  e8 13 40 f9   ldr	x8, [sp, #0x20]
0x250effd38:  b1 00 00 14   b	loc_250effffc ; ⤵ 0x2c4
0x250effd3c:  ; loc_250effd3c
0x250effd3c:  e0 03 15 aa   mov	x0, x21
0x250effd40:  02 00 80 d2   mov	x2, #0
0x250effd44:  cb 86 c4 95   bl	0x258021870
0x250effd48:  f4 03 1a aa   mov	x20, x26
0x250effd4c:  ; loc_250effd4c
0x250effd4c:  68 24 0c d0   adrp	x8, 0x26938d000
0x250effd50:  08 31 45 f9   ldr	x8, [x8, #0xa60] ; "\xa8hO\xf1"
0x250effd54:  fb 23 05 a9   stp	x27, x8, [sp, #0x50]
0x250effd58:  08 8d d2 b0   adrp	x8, 0x1f60a0000
0x250effd5c:  01 49 3f 91   add	x1, x8, #0xfd2 ; initWithAsset:
0x250effd60:  e0 43 01 91   add	x0, sp, #0x50
0x250effd64:  e2 0f 40 f9   ldr	x2, [sp, #0x18]
0x250effd68:  fe 01 c8 95   bl	_objc_msgSendSuper2_stub
0x250effd6c:  fb 03 00 aa   mov	x27, x0
0x250effd70:  40 13 00 b4   cbz	x0, loc_250efffd8 ; ⤵ 0x268
0x250effd74:  e0 03 1b aa   mov	x0, x27
0x250effd78:  e2 03 17 aa   mov	x2, x23
0x250effd7c:  f5 87 c4 95   bl	0x258021d50
0x250effd80:  e0 03 1b aa   mov	x0, x27
0x250effd84:  22 00 80 52   mov	w2, #0x1
0x250effd88:  56 84 c4 95   bl	0x258020ee0
0x250effd8c:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250effd90:  08 55 82 b9   ldrsw	x8, [x8, #0x254] ; _OBJC_IVAR_$_APRKPlayerItem._contentLocation
0x250effd94:  60 03 08 8b   add	x0, x27, x8
0x250effd98:  e1 03 19 aa   mov	x1, x25
0x250effd9c:  e1 03 c8 95   bl	j__objc_storeStrong
0x250effda0:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effda4:  08 f5 47 f9   ldr	x8, [x8, #0xfe8] ; _ptr.1e0a0d590 (CoreMedia/__DATA_CONST.__const)
0x250effda8:  02 01 40 f9   ldr	x2, [x8]
0x250effdac:  e0 03 13 aa   mov	x0, x19
0x250effdb0:  10 07 c4 95   bl	0x2580019f0
0x250effdb4:  bb 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250effdb8:  f9 03 00 aa   mov	x25, x0
0x250effdbc:  f9 01 c8 95   bl	j__objc_release_x20
0x250effdc0:  79 01 00 b4   cbz	x25, loc_250effdec ; ⤵ 0x2c
0x250effdc4:  e8 83 01 91   add	x8, sp, #0x60
0x250effdc8:  e0 03 19 aa   mov	x0, x25
0x250effdcc:  41 3c c8 95   bl	j__CMTimeMakeFromDictionary
0x250effdd0:  e0 1b c0 3d   ldr	q0, [sp, #0x60]
0x250effdd4:  e0 0f 80 3d   str	q0, [sp, #0x30]
0x250effdd8:  e8 3b 40 f9   ldr	x8, [sp, #0x70]
0x250effddc:  e8 23 00 f9   str	x8, [sp, #0x40]
0x250effde0:  e2 c3 00 91   add	x2, sp, #0x30
0x250effde4:  e0 03 1b aa   mov	x0, x27
0x250effde8:  56 87 c4 95   bl	0x258021b40
0x250effdec:  ; loc_250effdec
0x250effdec:  28 8e 0b d0   adrp	x8, 0x2680c5000
0x250effdf0:  08 1d 40 f9   ldr	x8, [x8, #0x38] ; _ptr.1e0a0d598 (CoreMedia/__DATA_CONST.__const)
0x250effdf4:  02 01 40 f9   ldr	x2, [x8]
0x250effdf8:  e0 03 13 aa   mov	x0, x19
0x250effdfc:  fd 06 c4 95   bl	0x2580019f0
0x250effe00:  a8 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250effe04:  fa 03 00 aa   mov	x26, x0
0x250effe08:  fa 01 c8 95   bl	j__objc_release_x25
0x250effe0c:  ba 01 00 b4   cbz	x26, loc_250effe40 ; ⤵ 0x34
0x250effe10:  e8 c3 00 91   add	x8, sp, #0x30
0x250effe14:  e0 03 1a aa   mov	x0, x26
0x250effe18:  2e 3c c8 95   bl	j__CMTimeMakeFromDictionary
0x250effe1c:  e0 0f c0 3d   ldr	q0, [sp, #0x30]
0x250effe20:  e0 1b 80 3d   str	q0, [sp, #0x60]
0x250effe24:  e8 23 40 f9   ldr	x8, [sp, #0x40]
0x250effe28:  e8 3b 00 f9   str	x8, [sp, #0x70]
0x250effe2c:  e0 0f 80 3d   str	q0, [sp, #0x30]
0x250effe30:  e8 23 00 f9   str	x8, [sp, #0x40]
0x250effe34:  e2 c3 00 91   add	x2, sp, #0x30
0x250effe38:  e0 03 1b aa   mov	x0, x27
0x250effe3c:  95 87 c4 95   bl	0x258021c90
0x250effe40:  ; loc_250effe40
0x250effe40:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effe44:  08 3d 47 f9   ldr	x8, [x8, #0xe78] ; _ptr.1e0a0d3a8 (CoreMedia/__DATA_CONST.__const)
0x250effe48:  02 01 40 f9   ldr	x2, [x8]
0x250effe4c:  e0 03 13 aa   mov	x0, x19
0x250effe50:  e8 06 c4 95   bl	0x2580019f0
0x250effe54:  93 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250effe58:  fc 03 00 aa   mov	x28, x0
0x250effe5c:  e9 01 c8 95   bl	j__objc_release_x26
0x250effe60:  dc 00 00 b4   cbz	x28, loc_250effe78 ; ⤵ 0x18
0x250effe64:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250effe68:  08 59 82 b9   ldrsw	x8, [x8, #0x258]
0x250effe6c:  60 03 08 8b   add	x0, x27, x8
0x250effe70:  e1 03 1c aa   mov	x1, x28
0x250effe74:  ab 03 c8 95   bl	j__objc_storeStrong
0x250effe78:  ; loc_250effe78
0x250effe78:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effe7c:  08 41 47 f9   ldr	x8, [x8, #0xe80] ; _ptr.1e0a0d3a0 (CoreMedia/__DATA_CONST.__const)
0x250effe80:  02 01 40 f9   ldr	x2, [x8]
0x250effe84:  e0 03 13 aa   mov	x0, x19
0x250effe88:  da 06 c4 95   bl	0x2580019f0
0x250effe8c:  85 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250effe90:  f9 03 00 aa   mov	x25, x0
0x250effe94:  e3 01 c8 95   bl	j__objc_release_x28
0x250effe98:  79 01 00 b4   cbz	x25, loc_250effec4 ; ⤵ 0x2c
0x250effe9c:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250effea0:  08 5d 82 b9   ldrsw	x8, [x8, #0x25c]
0x250effea4:  7a 03 08 8b   add	x26, x27, x8
0x250effea8:  e8 c3 00 91   add	x8, sp, #0x30
0x250effeac:  e0 03 19 aa   mov	x0, x25
0x250effeb0:  08 3c c8 95   bl	j__CMTimeMakeFromDictionary
0x250effeb4:  e0 0f c0 3d   ldr	q0, [sp, #0x30]
0x250effeb8:  e8 23 40 f9   ldr	x8, [sp, #0x40]
0x250effebc:  48 0b 00 f9   str	x8, [x26, #0x10]
0x250effec0:  40 03 80 3d   str	q0, [x26]
0x250effec4:  ; loc_250effec4
0x250effec4:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250effec8:  08 f9 43 f9   ldr	x8, [x8, #0x7f0] ; _ptr.1e0a0d3e8 (CoreMedia/__DATA_CONST.__const)
0x250effecc:  02 01 40 f9   ldr	x2, [x8]
0x250effed0:  e0 03 13 aa   mov	x0, x19
0x250effed4:  c7 06 c4 95   bl	0x2580019f0
0x250effed8:  72 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250effedc:  fa 03 00 aa   mov	x26, x0
0x250effee0:  c4 01 c8 95   bl	j__objc_release_x25
0x250effee4:  fa 00 00 b4   cbz	x26, loc_250efff00 ; ⤵ 0x1c
0x250effee8:  e0 03 1a aa   mov	x0, x26
0x250effeec:  b9 04 c4 95   bl	0x2580011d0
0x250effef0:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250effef4:  08 61 82 b9   ldrsw	x8, [x8, #0x260]
0x250effef8:  68 03 08 8b   add	x8, x27, x8
0x250effefc:  00 01 00 39   strb	w0, [x8]
0x250efff00:  ; loc_250efff00
0x250efff00:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250efff04:  08 e1 47 f9   ldr	x8, [x8, #0xfc0] ; _ptr.1e0a0d578 (CoreMedia/__DATA_CONST.__const)
0x250efff08:  02 01 40 f9   ldr	x2, [x8]
0x250efff0c:  e0 03 13 aa   mov	x0, x19
0x250efff10:  b8 06 c4 95   bl	0x2580019f0
0x250efff14:  63 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250efff18:  f9 03 00 aa   mov	x25, x0
0x250efff1c:  b9 01 c8 95   bl	j__objc_release_x26
0x250efff20:  f9 00 00 b4   cbz	x25, loc_250efff3c ; ⤵ 0x1c
0x250efff24:  e0 03 19 aa   mov	x0, x25
0x250efff28:  aa 04 c4 95   bl	0x2580011d0
0x250efff2c:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250efff30:  08 65 82 b9   ldrsw	x8, [x8, #0x264]
0x250efff34:  68 03 08 8b   add	x8, x27, x8
0x250efff38:  00 01 00 39   strb	w0, [x8]
0x250efff3c:  ; loc_250efff3c
0x250efff3c:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250efff40:  08 dd 47 f9   ldr	x8, [x8, #0xfb8] ; _ptr.1e0a0d570 (CoreMedia/__DATA_CONST.__const)
0x250efff44:  02 01 40 f9   ldr	x2, [x8]
0x250efff48:  e0 03 13 aa   mov	x0, x19
0x250efff4c:  a9 06 c4 95   bl	0x2580019f0
0x250efff50:  54 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250efff54:  fa 03 00 aa   mov	x26, x0
0x250efff58:  a6 01 c8 95   bl	j__objc_release_x25
0x250efff5c:  a8 c2 0e 90   adrp	x8, 0x26e753000
0x250efff60:  08 69 82 b9   ldrsw	x8, [x8, #0x268]
0x250efff64:  68 03 08 8b   add	x8, x27, x8
0x250efff68:  1a 01 00 f9   str	x26, [x8]
0x250efff6c:  28 8e 0b d0   adrp	x8, 0x2680c5000
0x250efff70:  08 25 40 f9   ldr	x8, [x8, #0x48] ; _ptr.1e0a0d5c8 (CoreMedia/__DATA_CONST.__const)
0x250efff74:  02 01 40 f9   ldr	x2, [x8]
0x250efff78:  e0 03 13 aa   mov	x0, x19
0x250efff7c:  9d 06 c4 95   bl	0x2580019f0
0x250efff80:  48 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250efff84:  f9 03 00 aa   mov	x25, x0
0x250efff88:  9e 01 c8 95   bl	j__objc_release_x26
0x250efff8c:  99 00 00 b4   cbz	x25, loc_250efff9c ; ⤵ 0x10
0x250efff90:  e0 03 1b aa   mov	x0, x27
0x250efff94:  e2 03 19 aa   mov	x2, x25
0x250efff98:  62 87 c4 95   bl	0x258021d20
0x250efff9c:  ; loc_250efff9c
0x250efff9c:  28 8e 0b b0   adrp	x8, 0x2680c4000
0x250efffa0:  08 15 47 f9   ldr	x8, [x8, #0xe28] ; _ptr.1e0a0d360 (CoreMedia/__DATA_CONST.__const)
0x250efffa4:  02 01 40 f9   ldr	x2, [x8]
0x250efffa8:  e0 03 13 aa   mov	x0, x19
0x250efffac:  91 06 c4 95   bl	0x2580019f0
0x250efffb0:  3c 03 c8 95   bl	j__objc_claimAutoreleasedReturnValue
0x250efffb4:  f4 03 00 aa   mov	x20, x0
0x250efffb8:  8e 01 c8 95   bl	j__objc_release_x25
0x250efffbc:  d4 00 00 b4   cbz	x20, loc_250efffd4 ; ⤵ 0x18
0x250efffc0:  e0 03 14 aa   mov	x0, x20
0x250efffc4:  db 07 c4 95   bl	0x258001f30
0x250efffc8:  e2 03 00 aa   mov	x2, x0
0x250efffcc:  e0 03 1b aa   mov	x0, x27
0x250efffd0:  2c 87 c4 95   bl	0x258021c80
0x250efffd4:  ; loc_250efffd4
0x250efffd4:  f9 0b 40 f9   ldr	x25, [sp, #0x10]
0x250efffd8:  ; loc_250efffd8
0x250efffd8:  8e 04 c8 95   bl	j__objc_retain_x27
0x250efffdc:  fb 03 00 aa   mov	x27, x0
0x250efffe0:  e8 0f 40 f9   ldr	x8, [sp, #0x18]
0x250efffe4:  93 01 c8 95   bl	j__objc_release_x8
0x250efffe8:  82 01 c8 95   bl	j__objc_release_x25
0x250efffec:  fa 03 14 aa   mov	x26, x20
0x250effff0:  f9 03 1b aa   mov	x25, x27
0x250effff4:  e8 13 40 f9   ldr	x8, [sp, #0x20]
0x250effff8:  f4 07 40 f9   ldr	x20, [sp, #0x8]
0x250effffc:  ; loc_250effffc
0x250effffc:  8d 01 c8 95   bl	j__objc_release_x8
0x250f00000:  e8 17 40 f9   ldr	x8, [sp, #0x28]
0x250f00004:  8b 01 c8 95   bl	j__objc_release_x8
0x250f00008:  76 01 c8 95   bl	j__objc_release_x24
0x250f0000c:  7d 01 c8 95   bl	j__objc_release_x26
0x250f00010:  ; loc_250f00010
0x250f00010:  70 01 c8 95   bl	j__objc_release_x23
0x250f00014:  ; loc_250f00014
0x250f00014:  6b 01 c8 95   bl	j__objc_release_x22
0x250f00018:  66 01 c8 95   bl	j__objc_release_x21
0x250f0001c:  61 01 c8 95   bl	j__objc_release_x20
0x250f00020:  5c 01 c8 95   bl	j__objc_release_x19
0x250f00024:  7b 01 c8 95   bl	j__objc_release_x27
0x250f00028:  e0 03 19 aa   mov	x0, x25
0x250f0002c:  fd 7b 4d a9   ldp	fp, lr, [sp, #0xd0]
0x250f00030:  f4 4f 4c a9   ldp	x20, x19, [sp, #0xc0]
0x250f00034:  f6 57 4b a9   ldp	x22, x21, [sp, #0xb0]
0x250f00038:  f8 5f 4a a9   ldp	x24, x23, [sp, #0xa0]
0x250f0003c:  fa 67 49 a9   ldp	x26, x25, [sp, #0x90]
0x250f00040:  fc 6f 48 a9   ldp	x28, x27, [sp, #0x80]
0x250f00044:  ff 83 03 91   add	sp, sp, #0xe0
0x250f00048:  ff 0f 5f d6   retab
0x250f0004c:  ; loc_250f0004c
0x250f0004c:  83 38 00 94   bl	0x250f0e258
0x250f00050:  38 ff ff 17   b	loc_250effd30 ; ⤴ -0x320
0x250f00054:  ; loc_250f00054
0x250f00054:  e0 03 14 aa   mov	x0, x20
0x250f00058:  70 38 00 94   bl	0x250f0e218
0x250f0005c:  26 ff ff 17   b	loc_250effcf4 ; ⤴ -0x368

