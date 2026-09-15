
-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:]
0x250eed7d4:  7f 23 03 d5   pacibsp
0x250eed7d8:  ff 83 05 d1   sub	sp, sp, #0x160
0x250eed7dc:  fc 6f 10 a9   stp	x28, x27, [sp, #0x100]
0x250eed7e0:  fa 67 11 a9   stp	x26, x25, [sp, #0x110]
0x250eed7e4:  f8 5f 12 a9   stp	x24, x23, [sp, #0x120]
0x250eed7e8:  f6 57 13 a9   stp	x22, x21, [sp, #0x130]
0x250eed7ec:  f4 4f 14 a9   stp	x20, x19, [sp, #0x140]
0x250eed7f0:  fd 7b 15 a9   stp	fp, lr, [sp, #0x150]
0x250eed7f4:  fd 43 05 91   add	fp, sp, #0x150
0x250eed7f8:  f6 03 00 aa   mov	x22, x0
0x250eed7fc:  a8 95 0b f0   adrp	x8, 0x2681a4000
0x250eed800:  08 39 45 f9   ldr	x8, [x8, #0xa70] ; _ptr.___stack_chk_guard
0x250eed804:  08 01 40 f9   ldr	x8, [x8]
0x250eed808:  a8 03 1a f8   stur	x8, [fp, #-0x60]
0x250eed80c:  d8 03 80 52   mov	w24, #0x1e
0x250eed810:  18 40 a0 72   movk	w24, #0x200, lsl #0x10
0x250eed814:  97 4b c8 95   bl	_objc_retain_x2_stub
0x250eed818:  fb 03 00 aa   mov	x27, x0
0x250eed81c:  b9 8e 0b b0   adrp	x25, 0x2680c2000
0x250eed820:  20 ab 47 f9   ldr	x0, [x25, #0xf50] ; _ptr.NSMutableDictionary
0x250eed824:  8f 4d c8 95   bl	_objc_alloc_init_stub
0x250eed828:  fc 03 00 aa   mov	x28, x0
0x250eed82c:  a8 8e 0b f0   adrp	x8, 0x2680c4000
0x250eed830:  08 99 43 f9   ldr	x8, [x8, #0x730] ; _ptr._kCMTimeZero
0x250eed834:  00 01 c0 3d   ldr	q0, [x8]
0x250eed838:  e0 17 80 3d   str	q0, [sp, #0x50]
0x250eed83c:  a0 03 97 3c   stur	q0, [fp, #-0x90]
0x250eed840:  1a 09 40 f9   ldr	x26, [x8, #0x10]
0x250eed844:  ba 03 18 f8   stur	x26, [fp, #-0x80]
0x250eed848:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eed84c:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eed850:  1f 79 00 71   cmp	w8, #0x1e
0x250eed854:  2c 01 00 54   b.gt	loc_250eed878 ; ⤵ 0x24
0x250eed858:  1f 05 00 31   cmn	w8, #0x1
0x250eed85c:  c1 4c 00 54   b.ne	loc_250eee1f4 ; ⤵ 0x998
0x250eed860:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eed864:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eed868:  c1 03 80 52   mov	w1, #0x1e
0x250eed86c:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eed870:  54 4d c8 95   bl	__LogCategory_Initialize_stub
0x250eed874:  00 4c 00 35   cbnz	w0, loc_250eee1f4 ; ⤵ 0x980
0x250eed878:  ; loc_250eed878
0x250eed878:  33 ab 47 f9   ldr	x19, [x25, #0xf50]
0x250eed87c:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eed880:  42 20 32 91   add	x2, x2, #0xc88 ; _ptr.__NSCFConstantString
0x250eed884:  e0 03 1b aa   mov	x0, x27
0x250eed888:  5a 50 c4 95   bl	0x2580019f0
0x250eed88c:  05 4d c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed890:  f4 03 00 aa   mov	x20, x0
0x250eed894:  e0 03 13 aa   mov	x0, x19
0x250eed898:  e2 03 14 aa   mov	x2, x20
0x250eed89c:  09 5d c4 95   bl	0x258004cc0
0x250eed8a0:  00 4d c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed8a4:  f5 03 00 aa   mov	x21, x0
0x250eed8a8:  3e 4b c8 95   bl	_objc_release_x20_stub
0x250eed8ac:  55 47 00 b4   cbz	x21, loc_250eee194 ; ⤵ 0x8e8
0x250eed8b0:  a8 8e 0b f0   adrp	x8, 0x2680c4000
0x250eed8b4:  08 19 47 f9   ldr	x8, [x8, #0xe30] ; _ptr._kFigEndpointPlaybackSessionKey_PlayerLoggingID
0x250eed8b8:  13 01 40 f9   ldr	x19, [x8]
0x250eed8bc:  e0 03 15 aa   mov	x0, x21
0x250eed8c0:  e2 03 13 aa   mov	x2, x19
0x250eed8c4:  4b 50 c4 95   bl	0x2580019f0
0x250eed8c8:  f6 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed8cc:  f4 03 00 aa   mov	x20, x0
0x250eed8d0:  34 4b c8 95   bl	_objc_release_x20_stub
0x250eed8d4:  94 02 00 b4   cbz	x20, loc_250eed924 ; ⤵ 0x50
0x250eed8d8:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eed8dc:  00 25 45 f9   ldr	x0, [x8, #0xa48] ; _ptr.AVSpecifiedLoggingIdentifier
0x250eed8e0:  94 4d c8 95   bl	_objc_alloc_stub
0x250eed8e4:  f4 03 00 aa   mov	x20, x0
0x250eed8e8:  e0 03 15 aa   mov	x0, x21
0x250eed8ec:  e2 03 13 aa   mov	x2, x19
0x250eed8f0:  40 50 c4 95   bl	0x2580019f0
0x250eed8f4:  eb 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed8f8:  f7 03 00 aa   mov	x23, x0
0x250eed8fc:  e0 03 14 aa   mov	x0, x20
0x250eed900:  e2 03 17 aa   mov	x2, x23
0x250eed904:  db ce c4 95   bl	0x258021470
0x250eed908:  f3 03 00 aa   mov	x19, x0
0x250eed90c:  31 4b c8 95   bl	_objc_release_x23_stub
0x250eed910:  93 00 00 b4   cbz	x19, loc_250eed920 ; ⤵ 0x10
0x250eed914:  c0 06 40 f9   ldr	x0, [x22, #0x8]
0x250eed918:  e2 03 13 aa   mov	x2, x19
0x250eed91c:  a9 d0 c4 95   bl	0x258021bc0
0x250eed920:  ; loc_250eed920
0x250eed920:  1c 4b c8 95   bl	_objc_release_x19_stub
0x250eed924:  ; loc_250eed924
0x250eed924:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eed928:  42 a0 36 91   add	x2, x2, #0xda8 ; _ptr.__NSCFConstantString
0x250eed92c:  e0 03 15 aa   mov	x0, x21
0x250eed930:  30 50 c4 95   bl	0x2580019f0
0x250eed934:  db 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed938:  f3 03 00 aa   mov	x19, x0
0x250eed93c:  25 4e c4 95   bl	0x2580011d0
0x250eed940:  f4 03 00 aa   mov	x20, x0
0x250eed944:  13 4b c8 95   bl	_objc_release_x19_stub
0x250eed948:  b4 08 00 34   cbz	w20, loc_250eeda5c ; ⤵ 0x114
0x250eed94c:  e0 03 16 aa   mov	x0, x22
0x250eed950:  48 cf c4 95   bl	0x258021670
0x250eed954:  d3 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed958:  f3 03 00 aa   mov	x19, x0
0x250eed95c:  e2 03 16 aa   mov	x2, x22
0x250eed960:  38 cf c4 95   bl	0x258021640
0x250eed964:  0b 4b c8 95   bl	_objc_release_x19_stub
0x250eed968:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eed96c:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eed970:  1f 29 00 71   cmp	w8, #0xa
0x250eed974:  0c 01 00 54   b.gt	loc_250eed994 ; ⤵ 0x20
0x250eed978:  1f 05 00 31   cmn	w8, #0x1
0x250eed97c:  c1 44 00 54   b.ne	loc_250eee214 ; ⤵ 0x898
0x250eed980:  01 53 00 51   sub	w1, w24, #0x14
0x250eed984:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eed988:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eed98c:  0d 4d c8 95   bl	__LogCategory_Initialize_stub
0x250eed990:  20 44 00 35   cbnz	w0, loc_250eee214 ; ⤵ 0x884
0x250eed994:  ; loc_250eed994
0x250eed994:  c2 26 40 f9   ldr	x2, [x22, #0x48]
0x250eed998:  c2 00 00 b4   cbz	x2, loc_250eed9b0 ; ⤵ 0x18
0x250eed99c:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eed9a0:  08 41 43 f9   ldr	x8, [x8, #0x680] ; _ptr._AVURLAssetSSLPropertiesKey
0x250eed9a4:  03 01 40 f9   ldr	x3, [x8]
0x250eed9a8:  e0 03 1c aa   mov	x0, x28
0x250eed9ac:  21 50 c4 95   bl	0x258001a30
0x250eed9b0:  ; loc_250eed9b0
0x250eed9b0:  c8 a2 42 39   ldrb	w8, [x22, #0xa8]
0x250eed9b4:  1f 05 00 71   cmp	w8, #0x1
0x250eed9b8:  c1 01 00 54   b.ne	loc_250eed9f0 ; ⤵ 0x38
0x250eed9bc:  08 8f 0b d0   adrp	x8, 0x2680cf000
0x250eed9c0:  00 45 43 f9   ldr	x0, [x8, #0x688] ; _ptr.NSNumber
0x250eed9c4:  22 00 80 52   mov	w2, #0x1
0x250eed9c8:  2a 4e c4 95   bl	0x258001270
0x250eed9cc:  b5 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eed9d0:  f3 03 00 aa   mov	x19, x0
0x250eed9d4:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eed9d8:  08 4d 43 f9   ldr	x8, [x8, #0x698] ; _ptr._AVURLAssetUseAppleWirelessDirectLinkKey
0x250eed9dc:  03 01 40 f9   ldr	x3, [x8]
0x250eed9e0:  e0 03 1c aa   mov	x0, x28
0x250eed9e4:  e2 03 13 aa   mov	x2, x19
0x250eed9e8:  12 50 c4 95   bl	0x258001a30
0x250eed9ec:  e9 4a c8 95   bl	_objc_release_x19_stub
0x250eed9f0:  ; loc_250eed9f0
0x250eed9f0:  a8 8e 0b f0   adrp	x8, 0x2680c4000
0x250eed9f4:  08 cd 46 f9   ldr	x8, [x8, #0xd98] ; _ptr._kFigEndpointPlaybackSessionKey_ContentLocation
0x250eed9f8:  14 01 40 f9   ldr	x20, [x8]
0x250eed9fc:  e0 03 15 aa   mov	x0, x21
0x250eeda00:  e2 03 14 aa   mov	x2, x20
0x250eeda04:  fb 4f c4 95   bl	0x2580019f0
0x250eeda08:  a6 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eeda0c:  f3 03 00 aa   mov	x19, x0
0x250eeda10:  bf 83 16 f8   stur	xzr, [fp, #-0x98]
0x250eeda14:  a3 63 02 d1   sub	x3, fp, #0x98
0x250eeda18:  e0 03 16 aa   mov	x0, x22
0x250eeda1c:  e2 03 13 aa   mov	x2, x19
0x250eeda20:  90 cc c4 95   bl	0x258020c60
0x250eeda24:  9f 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eeda28:  f8 03 00 aa   mov	x24, x0
0x250eeda2c:  a8 83 56 f8   ldur	x8, [fp, #-0x98]
0x250eeda30:  20 4b c8 95   bl	_objc_retain_x8_stub
0x250eeda34:  f7 03 00 aa   mov	x23, x0
0x250eeda38:  b8 00 00 b4   cbz	x24, loc_250eeda4c ; ⤵ 0x14
0x250eeda3c:  e0 03 15 aa   mov	x0, x21
0x250eeda40:  e2 03 18 aa   mov	x2, x24
0x250eeda44:  e3 03 14 aa   mov	x3, x20
0x250eeda48:  fa 4f c4 95   bl	0x258001a30
0x250eeda4c:  ; loc_250eeda4c
0x250eeda4c:  e5 4a c8 95   bl	_objc_release_x24_stub
0x250eeda50:  d0 4a c8 95   bl	_objc_release_x19_stub
0x250eeda54:  df 4a c8 95   bl	_objc_release_x23_stub
0x250eeda58:  04 00 00 14   b	loc_250eeda68 ; ⤵ 0x10
0x250eeda5c:  ; loc_250eeda5c
0x250eeda5c:  c8 26 40 f9   ldr	x8, [x22, #0x48]
0x250eeda60:  df 26 00 f9   str	xzr, [x22, #0x48]
0x250eeda64:  f3 4a c8 95   bl	_objc_release_x8_stub
0x250eeda68:  ; loc_250eeda68
0x250eeda68:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eeda6c:  42 20 37 91   add	x2, x2, #0xdc8 ; _ptr.__NSCFConstantString
0x250eeda70:  e0 03 15 aa   mov	x0, x21
0x250eeda74:  df 4f c4 95   bl	0x2580019f0
0x250eeda78:  8a 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eeda7c:  f8 03 00 aa   mov	x24, x0
0x250eeda80:  80 0c 00 b4   cbz	x0, loc_250eedc10 ; ⤵ 0x190
0x250eeda84:  c0 46 40 f9   ldr	x0, [x22, #0x88]
0x250eeda88:  e1 03 15 aa   mov	x1, x21
0x250eeda8c:  02 00 80 d2   mov	x2, #0
0x250eeda90:  90 84 c8 95   bl	_APReceiverFairPlayHelperProcessFPInfo_stub
0x250eeda94:  60 06 00 34   cbz	w0, loc_250eedb60 ; ⤵ 0xcc
0x250eeda98:  f3 03 00 aa   mov	x19, x0
0x250eeda9c:  f9 03 1a aa   mov	x25, x26
0x250eedaa0:  fa 03 18 aa   mov	x26, x24
0x250eedaa4:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eedaa8:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eedaac:  1f 79 00 71   cmp	w8, #0x1e
0x250eedab0:  2c 01 00 54   b.gt	loc_250eedad4 ; ⤵ 0x24
0x250eedab4:  1f 05 00 31   cmn	w8, #0x1
0x250eedab8:  41 3b 00 54   b.ne	loc_250eee220 ; ⤵ 0x768
0x250eedabc:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eedac0:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eedac4:  c1 03 80 52   mov	w1, #0x1e
0x250eedac8:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eedacc:  bd 4c c8 95   bl	__LogCategory_Initialize_stub
0x250eedad0:  80 3a 00 35   cbnz	w0, loc_250eee220 ; ⤵ 0x750
0x250eedad4:  ; loc_250eedad4
0x250eedad4:  08 8f 0b d0   adrp	x8, 0x2680cf000
0x250eedad8:  09 8f 0b d0   adrp	x9, 0x2680cf000
0x250eedadc:  29 55 40 f9   ldr	x9, [x9, #0xa8] ; _ptr._NSOSStatusErrorDomain
0x250eedae0:  14 5d 42 f9   ldr	x20, [x8, #0x4b8]
0x250eedae4:  37 01 40 f9   ldr	x23, [x9]
0x250eedae8:  73 7e 40 93   sxtw	x19, w19
0x250eedaec:  08 8f 0b b0   adrp	x8, 0x2680ce000
0x250eedaf0:  08 c1 47 f9   ldr	x8, [x8, #0xf80] ; _ptr._NSLocalizedDescriptionKey
0x250eedaf4:  09 01 40 f9   ldr	x9, [x8]
0x250eedaf8:  c8 5f 12 f0   adrp	x8, 0x275ae8000
0x250eedafc:  08 a1 37 91   add	x8, x8, #0xde8 ; _ptr.__NSCFConstantString
0x250eedb00:  a9 23 39 a9   stp	x9, x8, [fp, #-0x70]
0x250eedb04:  a8 8e 0b b0   adrp	x8, 0x2680c2000
0x250eedb08:  00 6d 47 f9   ldr	x0, [x8, #0xed8] ; _ptr.NSDictionary
0x250eedb0c:  a2 a3 01 d1   sub	x2, fp, #0x68
0x250eedb10:  a3 c3 01 d1   sub	x3, fp, #0x70
0x250eedb14:  24 00 80 52   mov	w4, #0x1
0x250eedb18:  c2 50 c4 95   bl	0x258001e20
0x250eedb1c:  61 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedb20:  f8 03 00 aa   mov	x24, x0
0x250eedb24:  e0 03 14 aa   mov	x0, x20
0x250eedb28:  e2 03 17 aa   mov	x2, x23
0x250eedb2c:  e3 03 13 aa   mov	x3, x19
0x250eedb30:  e4 03 18 aa   mov	x4, x24
0x250eedb34:  bf 5c c4 95   bl	0x258004e30
0x250eedb38:  5a 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedb3c:  f3 03 00 aa   mov	x19, x0
0x250eedb40:  e0 03 16 aa   mov	x0, x22
0x250eedb44:  e2 03 13 aa   mov	x2, x19
0x250eedb48:  d6 cc c4 95   bl	0x258020ea0
0x250eedb4c:  91 4a c8 95   bl	_objc_release_x19_stub
0x250eedb50:  a4 4a c8 95   bl	_objc_release_x24_stub
0x250eedb54:  f8 03 1a aa   mov	x24, x26
0x250eedb58:  fa 03 19 aa   mov	x26, x25
0x250eedb5c:  b9 8e 0b b0   adrp	x25, 0x2680c2000
0x250eedb60:  ; loc_250eedb60
0x250eedb60:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eedb64:  42 20 38 91   add	x2, x2, #0xe08 ; _ptr.__NSCFConstantString
0x250eedb68:  e0 03 15 aa   mov	x0, x21
0x250eedb6c:  a1 4f c4 95   bl	0x2580019f0
0x250eedb70:  4c 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedb74:  f3 03 00 aa   mov	x19, x0
0x250eedb78:  20 ab 47 f9   ldr	x0, [x25, #0xf50]
0x250eedb7c:  b9 4c c8 95   bl	_objc_alloc_init_stub
0x250eedb80:  f4 03 00 aa   mov	x20, x0
0x250eedb84:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eedb88:  08 19 41 f9   ldr	x8, [x8, #0x230] ; _ptr._AVContentAuthorizationInfoSessionIDKey
0x250eedb8c:  03 01 40 f9   ldr	x3, [x8]
0x250eedb90:  e2 03 13 aa   mov	x2, x19
0x250eedb94:  a7 4f c4 95   bl	0x258001a30
0x250eedb98:  e0 03 18 aa   mov	x0, x24
0x250eedb9c:  6d 4d c4 95   bl	0x258001150
0x250eedba0:  00 01 00 b4   cbz	x0, loc_250eedbc0 ; ⤵ 0x20
0x250eedba4:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eedba8:  08 15 41 f9   ldr	x8, [x8, #0x228] ; _ptr._AVContentAuthorizationInfoPlayInfoKey
0x250eedbac:  03 01 40 f9   ldr	x3, [x8]
0x250eedbb0:  e0 03 14 aa   mov	x0, x20
0x250eedbb4:  e2 03 18 aa   mov	x2, x24
0x250eedbb8:  9e 4f c4 95   bl	0x258001a30
0x250eedbbc:  0d 00 00 14   b	loc_250eedbf0 ; ⤵ 0x34
0x250eedbc0:  ; loc_250eedbc0
0x250eedbc0:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eedbc4:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eedbc8:  1f 79 00 71   cmp	w8, #0x1e
0x250eedbcc:  2c 01 00 54   b.gt	loc_250eedbf0 ; ⤵ 0x24
0x250eedbd0:  1f 05 00 31   cmn	w8, #0x1
0x250eedbd4:  81 33 00 54   b.ne	loc_250eee244 ; ⤵ 0x670
0x250eedbd8:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eedbdc:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eedbe0:  c1 03 80 52   mov	w1, #0x1e
0x250eedbe4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eedbe8:  76 4c c8 95   bl	__LogCategory_Initialize_stub
0x250eedbec:  c0 32 00 35   cbnz	w0, loc_250eee244 ; ⤵ 0x658
0x250eedbf0:  ; loc_250eedbf0
0x250eedbf0:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eedbf4:  08 fd 42 f9   ldr	x8, [x8, #0x5f8] ; _ptr._AVURLAssetAirPlayAuthorizationInfoKey
0x250eedbf8:  03 01 40 f9   ldr	x3, [x8]
0x250eedbfc:  e0 03 1c aa   mov	x0, x28
0x250eedc00:  e2 03 14 aa   mov	x2, x20
0x250eedc04:  8b 4f c4 95   bl	0x258001a30
0x250eedc08:  66 4a c8 95   bl	_objc_release_x20_stub
0x250eedc0c:  61 4a c8 95   bl	_objc_release_x19_stub
0x250eedc10:  ; loc_250eedc10
0x250eedc10:  33 ab 47 f9   ldr	x19, [x25, #0xf50]
0x250eedc14:  a8 8e 0b f0   adrp	x8, 0x2680c4000
0x250eedc18:  08 b5 46 f9   ldr	x8, [x8, #0xd68] ; _ptr._kFigEndpointPlaybackSessionKey_AlternativeConfigurationOptions
0x250eedc1c:  02 01 40 f9   ldr	x2, [x8]
0x250eedc20:  e0 03 15 aa   mov	x0, x21
0x250eedc24:  73 4f c4 95   bl	0x2580019f0
0x250eedc28:  1e 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedc2c:  f4 03 00 aa   mov	x20, x0
0x250eedc30:  e0 03 13 aa   mov	x0, x19
0x250eedc34:  e2 03 14 aa   mov	x2, x20
0x250eedc38:  22 5c c4 95   bl	0x258004cc0
0x250eedc3c:  19 4c c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedc40:  f3 03 00 aa   mov	x19, x0
0x250eedc44:  57 4a c8 95   bl	_objc_release_x20_stub
0x250eedc48:  f3 27 00 f9   str	x19, [sp, #0x48]
0x250eedc4c:  13 01 00 b4   cbz	x19, loc_250eedc6c ; ⤵ 0x20
0x250eedc50:  08 90 0b 90   adrp	x8, 0x2680ed000
0x250eedc54:  08 09 43 f9   ldr	x8, [x8, #0x610] ; _ptr._AVURLAssetAlternativeConfigurationOptionsKey
0x250eedc58:  03 01 40 f9   ldr	x3, [x8]
0x250eedc5c:  e0 03 1c aa   mov	x0, x28
0x250eedc60:  e2 03 13 aa   mov	x2, x19
0x250eedc64:  73 4f c4 95   bl	0x258001a30
0x250eedc68:  0d 00 00 14   b	loc_250eedc9c ; ⤵ 0x34
0x250eedc6c:  ; loc_250eedc6c
0x250eedc6c:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eedc70:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eedc74:  1f 79 00 71   cmp	w8, #0x1e
0x250eedc78:  2c 01 00 54   b.gt	loc_250eedc9c ; ⤵ 0x24
0x250eedc7c:  1f 05 00 31   cmn	w8, #0x1
0x250eedc80:  e1 2d 00 54   b.ne	loc_250eee23c ; ⤵ 0x5bc
0x250eedc84:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eedc88:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eedc8c:  c1 03 80 52   mov	w1, #0x1e
0x250eedc90:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eedc94:  4b 4c c8 95   bl	__LogCategory_Initialize_stub
0x250eedc98:  20 2d 00 35   cbnz	w0, loc_250eee23c ; ⤵ 0x5a4
0x250eedc9c:  ; loc_250eedc9c
0x250eedc9c:  08 25 0c 90   adrp	x8, 0x26938d000
0x250eedca0:  00 49 45 f9   ldr	x0, [x8, #0xa90] ; "\xa8hO\xf1"
0x250eedca4:  a3 4c c8 95   bl	_objc_alloc_stub
0x250eedca8:  c3 92 46 a9   ldp	x3, x4, [x22, #0x68]
0x250eedcac:  e2 03 15 aa   mov	x2, x21
0x250eedcb0:  e5 03 1c aa   mov	x5, x28
0x250eedcb4:  e3 cd c4 95   bl	0x258021440
0x250eedcb8:  f9 03 00 aa   mov	x25, x0
0x250eedcbc:  39 81 c8 95   bl	_IsAppleInternalBuild_stub
0x250eedcc0:  40 06 00 34   cbz	w0, loc_250eedd88 ; ⤵ 0xc8
0x250eedcc4:  e7 77 c8 95   bl	_APGetSignpostsLogHandle_stub
0x250eedcc8:  f6 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedccc:  f3 03 00 aa   mov	x19, x0
0x250eedcd0:  e4 77 c8 95   bl	_APGetSignpostsLogHandle_stub
0x250eedcd4:  f3 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedcd8:  f7 03 00 aa   mov	x23, x0
0x250eedcdc:  65 83 c8 95   bl	_objc_msgSend$zoneUniqueIdentifiers_stub
0x250eedce0:  f4 03 00 aa   mov	x20, x0
0x250eedce4:  3b 4a c8 95   bl	_objc_release_x23_stub
0x250eedce8:  88 06 00 d1   sub	x8, x20, #0x1
0x250eedcec:  1f 0d 00 b1   cmn	x8, #0x3
0x250eedcf0:  a8 04 00 54   b.hi	loc_250eedd84 ; ⤵ 0x94
0x250eedcf4:  e0 03 13 aa   mov	x0, x19
0x250eedcf8:  56 53 c8 95   bl	_os_signpost_enabled_stub
0x250eedcfc:  40 04 00 34   cbz	w0, loc_250eedd84 ; ⤵ 0x88
0x250eedd00:  c0 06 40 f9   ldr	x0, [x22, #0x8]
0x250eedd04:  27 ce c4 95   bl	0x2580215a0
0x250eedd08:  e6 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedd0c:  f7 03 00 aa   mov	x23, x0
0x250eedd10:  e0 03 19 aa   mov	x0, x25
0x250eedd14:  0f 4f c4 95   bl	0x258001950
0x250eedd18:  e2 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedd1c:  fa 1f 00 f9   str	x26, [sp, #0x38]
0x250eedd20:  fa 03 18 aa   mov	x26, x24
0x250eedd24:  f8 03 00 aa   mov	x24, x0
0x250eedd28:  68 01 00 90   adrp	x8, 0x250f19000
0x250eedd2c:  00 e1 46 fd   ldr	d0, [x8, #0xdc0]
0x250eedd30:  e0 a3 00 bd   str	s0, [sp, #0xa0]
0x250eedd34:  f7 43 0a f8   stur	x23, [sp, #0xa4]
0x250eedd38:  08 08 81 52   mov	w8, #0x840
0x250eedd3c:  e8 5b 01 79   strh	w8, [sp, #0xac]
0x250eedd40:  e0 e3 0a f8   stur	x0, [sp, #0xae]
0x250eedd44:  c0 ff ff f0   adrp	x0, 0x250ee8000
0x250eedd48:  00 00 00 91   add	x0, x0, #0 ; _objc_msgSend$activateWithCompletion:
0x250eedd4c:  64 01 00 90   adrp	x4, 0x250f19000
0x250eedd50:  84 b8 3f 91   add	x4, x4, #0xfee ; "AirPlayVideoPlayerInsertItem"
0x250eedd54:  65 01 00 90   adrp	x5, 0x250f19000
0x250eedd58:  a5 f8 3e 91   add	x5, x5, #0xfbe ; "Handling insertPlayQueueItem. player %@ UUID %@"
0x250eedd5c:  e6 83 02 91   add	x6, sp, #0xa0
0x250eedd60:  e1 03 13 aa   mov	x1, x19
0x250eedd64:  02 00 80 52   mov	w2, #0
0x250eedd68:  e3 03 14 aa   mov	x3, x20
0x250eedd6c:  c7 02 80 52   mov	w7, #0x16
0x250eedd70:  bc 52 c8 95   bl	_objc_msgSend$value_dictionary_stub
0x250eedd74:  1b 4a c8 95   bl	_objc_release_x24_stub
0x250eedd78:  f8 03 1a aa   mov	x24, x26
0x250eedd7c:  fa 1f 40 f9   ldr	x26, [sp, #0x38]
0x250eedd80:  14 4a c8 95   bl	_objc_release_x23_stub
0x250eedd84:  ; loc_250eedd84
0x250eedd84:  03 4a c8 95   bl	_objc_release_x19_stub
0x250eedd88:  ; loc_250eedd88
0x250eedd88:  a8 8e 0b f0   adrp	x8, 0x2680c4000
0x250eedd8c:  08 45 47 f9   ldr	x8, [x8, #0xe88] ; _ptr._kFigEndpointPlaybackSessionKey_SupportsIntegratedTimeline
0x250eedd90:  02 01 40 f9   ldr	x2, [x8]
0x250eedd94:  e0 03 15 aa   mov	x0, x21
0x250eedd98:  16 4f c4 95   bl	0x2580019f0
0x250eedd9c:  c1 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedda0:  f3 03 00 aa   mov	x19, x0
0x250eedda4:  c0 5f 12 f0   adrp	x0, 0x275ae8000
0x250eedda8:  00 a0 38 91   add	x0, x0, #0xe28 ; _ptr.__NSCFConstantString
0x250eeddac:  71 7a c8 95   bl	_APSSettingsIsFeatureEnabled_stub
0x250eeddb0:  80 00 00 34   cbz	w0, loc_250eeddc0 ; ⤵ 0x10
0x250eeddb4:  73 00 00 b4   cbz	x19, loc_250eeddc0 ; ⤵ 0xc
0x250eeddb8:  e0 03 13 aa   mov	x0, x19
0x250eeddbc:  05 4d c4 95   bl	0x2580011d0
0x250eeddc0:  ; loc_250eeddc0
0x250eeddc0:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eeddc4:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eeddc8:  1f 79 00 71   cmp	w8, #0x1e
0x250eeddcc:  b9 07 00 b4   cbz	x25, loc_250eedec0 ; ⤵ 0xf4
0x250eeddd0:  2c 01 00 54   b.gt	loc_250eeddf4 ; ⤵ 0x24
0x250eeddd4:  1f 05 00 31   cmn	w8, #0x1
0x250eeddd8:  41 21 00 54   b.ne	loc_250eee200 ; ⤵ 0x428
0x250eedddc:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eedde0:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eedde4:  c1 03 80 52   mov	w1, #0x1e
0x250eedde8:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eeddec:  f5 4b c8 95   bl	__LogCategory_Initialize_stub
0x250eeddf0:  80 20 00 35   cbnz	w0, loc_250eee200 ; ⤵ 0x410
0x250eeddf4:  ; loc_250eeddf4
0x250eeddf4:  e0 17 c0 3d   ldr	q0, [sp, #0x50]
0x250eeddf8:  a0 03 97 3c   stur	q0, [fp, #-0x90]
0x250eeddfc:  ba 03 18 f8   stur	x26, [fp, #-0x80]
0x250eede00:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eede04:  42 20 39 91   add	x2, x2, #0xe48 ; _ptr.__NSCFConstantString
0x250eede08:  e0 03 15 aa   mov	x0, x21
0x250eede0c:  f9 4e c4 95   bl	0x2580019f0
0x250eede10:  a4 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eede14:  f4 03 00 aa   mov	x20, x0
0x250eede18:  80 00 00 b4   cbz	x0, loc_250eede28 ; ⤵ 0x10
0x250eede1c:  e0 03 19 aa   mov	x0, x25
0x250eede20:  e2 03 14 aa   mov	x2, x20
0x250eede24:  bf 9d c4 95   bl	0x258015520
0x250eede28:  ; loc_250eede28
0x250eede28:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eede2c:  42 a0 39 91   add	x2, x2, #0xe68 ; _ptr.__NSCFConstantString
0x250eede30:  e0 03 15 aa   mov	x0, x21
0x250eede34:  ef 4e c4 95   bl	0x2580019f0
0x250eede38:  9a 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eede3c:  f7 03 00 aa   mov	x23, x0
0x250eede40:  60 01 00 b4   cbz	x0, loc_250eede6c ; ⤵ 0x2c
0x250eede44:  a8 43 02 d1   sub	x8, fp, #0x90
0x250eede48:  e0 03 17 aa   mov	x0, x23
0x250eede4c:  21 84 c8 95   bl	_CMTimeMakeFromDictionary_stub
0x250eede50:  a0 03 d7 3c   ldur	q0, [fp, #-0x90]
0x250eede54:  e0 2b 80 3d   str	q0, [sp, #0xa0]
0x250eede58:  a8 03 58 f8   ldur	x8, [fp, #-0x80]
0x250eede5c:  e8 5b 00 f9   str	x8, [sp, #0xb0]
0x250eede60:  e2 83 02 91   add	x2, sp, #0xa0
0x250eede64:  e0 03 19 aa   mov	x0, x25
0x250eede68:  a2 cf c4 95   bl	0x258021cf0
0x250eede6c:  ; loc_250eede6c
0x250eede6c:  f7 53 02 a9   stp	x23, x20, [sp, #0x20]
0x250eede70:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eede74:  42 20 3a 91   add	x2, x2, #0xe88 ; _ptr.__NSCFConstantString
0x250eede78:  e0 03 1b aa   mov	x0, x27
0x250eede7c:  dd 4e c4 95   bl	0x2580019f0
0x250eede80:  88 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eede84:  f7 03 00 aa   mov	x23, x0
0x250eede88:  80 03 00 b4   cbz	x0, loc_250eedef8 ; ⤵ 0x70
0x250eede8c:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eede90:  42 a0 31 91   add	x2, x2, #0xc68 ; _ptr.__NSCFConstantString
0x250eede94:  e0 03 17 aa   mov	x0, x23
0x250eede98:  d6 4e c4 95   bl	0x2580019f0
0x250eede9c:  81 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedea0:  f4 03 00 aa   mov	x20, x0
0x250eedea4:  e0 03 16 aa   mov	x0, x22
0x250eedea8:  e2 03 14 aa   mov	x2, x20
0x250eedeac:  a9 cb c4 95   bl	0x258020d50
0x250eedeb0:  7c 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedeb4:  e0 0f 00 f9   str	x0, [sp, #0x18]
0x250eedeb8:  34 02 00 b4   cbz	x20, loc_250eedefc ; ⤵ 0x44
0x250eedebc:  1d 00 00 14   b	loc_250eedf30 ; ⤵ 0x74
0x250eedec0:  ; loc_250eedec0
0x250eedec0:  0c 16 00 54   b.gt	loc_250eee180 ; ⤵ 0x2c0
0x250eedec4:  1f 05 00 31   cmn	w8, #0x1
0x250eedec8:  80 18 00 54   b.eq	loc_250eee1d8 ; ⤵ 0x310
0x250eedecc:  ; loc_250eedecc
0x250eedecc:  f5 73 00 a9   stp	x21, x28, [sp]
0x250eeded0:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eeded4:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eeded8:  21 01 00 90   adrp	x1, 0x250f11000
0x250eededc:  21 dc 3e 91   add	x1, x1, #0xfb7 ; "-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:]"
0x250eedee0:  23 01 00 b0   adrp	x3, 0x250f12000
0x250eedee4:  63 9c 07 91   add	x3, x3, #0x1e7 ; "insert play queue item, unable to create item with dictionary = %@ options = %@"
0x250eedee8:  c2 03 80 52   mov	w2, #0x1e
0x250eedeec:  02 40 a0 72   movk	w2, #0x200, lsl #0x10
0x250eedef0:  04 4c c8 95   bl	_LogPrintF_stub
0x250eedef4:  a3 00 00 14   b	loc_250eee180 ; ⤵ 0x28c
0x250eedef8:  ; loc_250eedef8
0x250eedef8:  ff 0f 00 f9   str	xzr, [sp, #0x18]
0x250eedefc:  ; loc_250eedefc
0x250eedefc:  28 c3 0e d0   adrp	x8, 0x26e753000
0x250eedf00:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eedf04:  1f 79 00 71   cmp	w8, #0x1e
0x250eedf08:  2c 01 00 54   b.gt	loc_250eedf2c ; ⤵ 0x24
0x250eedf0c:  1f 05 00 31   cmn	w8, #0x1
0x250eedf10:  21 19 00 54   b.ne	loc_250eee234 ; ⤵ 0x324
0x250eedf14:  20 c3 0e d0   adrp	x0, 0x26e753000
0x250eedf18:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eedf1c:  c1 03 80 52   mov	w1, #0x1e
0x250eedf20:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eedf24:  a7 4b c8 95   bl	__LogCategory_Initialize_stub
0x250eedf28:  60 18 00 35   cbnz	w0, loc_250eee234 ; ⤵ 0x30c
0x250eedf2c:  ; loc_250eedf2c
0x250eedf2c:  14 00 80 d2   mov	x20, #0
0x250eedf30:  ; loc_250eedf30
0x250eedf30:  f3 63 03 a9   stp	x19, x24, [sp, #0x30]
0x250eedf34:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eedf38:  42 a0 3a 91   add	x2, x2, #0xea8 ; _ptr.__NSCFConstantString
0x250eedf3c:  e0 03 15 aa   mov	x0, x21
0x250eedf40:  ac 4e c4 95   bl	0x2580019f0
0x250eedf44:  57 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedf48:  f8 03 00 aa   mov	x24, x0
0x250eedf4c:  ff 7f 0a a9   stp	xzr, xzr, [sp, #0xa0]
0x250eedf50:  ff 5b 00 f9   str	xzr, [sp, #0xb0]
0x250eedf54:  e8 83 02 91   add	x8, sp, #0xa0
0x250eedf58:  de 83 c8 95   bl	_CMTimeMakeFromDictionary_stub
0x250eedf5c:  e8 af 40 b9   ldr	w8, [sp, #0xac]
0x250eedf60:  ba 03 80 52   mov	w26, #0x1d
0x250eedf64:  08 01 1a 0a   and	w8, w8, w26
0x250eedf68:  1f 05 00 71   cmp	w8, #0x1
0x250eedf6c:  01 01 00 54   b.ne	loc_250eedf8c ; ⤵ 0x20
0x250eedf70:  e0 2b c0 3d   ldr	q0, [sp, #0xa0]
0x250eedf74:  e0 23 80 3d   str	q0, [sp, #0x80]
0x250eedf78:  e8 5b 40 f9   ldr	x8, [sp, #0xb0]
0x250eedf7c:  e8 4b 00 f9   str	x8, [sp, #0x90]
0x250eedf80:  e2 03 02 91   add	x2, sp, #0x80
0x250eedf84:  e0 03 19 aa   mov	x0, x25
0x250eedf88:  6a cf c4 95   bl	0x258021d30
0x250eedf8c:  ; loc_250eedf8c
0x250eedf8c:  c2 5f 12 f0   adrp	x2, 0x275ae8000
0x250eedf90:  42 20 3b 91   add	x2, x2, #0xec8 ; _ptr.__NSCFConstantString
0x250eedf94:  e0 03 15 aa   mov	x0, x21
0x250eedf98:  96 4e c4 95   bl	0x2580019f0
0x250eedf9c:  41 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eedfa0:  f3 03 00 aa   mov	x19, x0
0x250eedfa4:  ff 7f 08 a9   stp	xzr, xzr, [sp, #0x80]
0x250eedfa8:  ff 4b 00 f9   str	xzr, [sp, #0x90]
0x250eedfac:  e8 03 02 91   add	x8, sp, #0x80
0x250eedfb0:  c8 83 c8 95   bl	_CMTimeMakeFromDictionary_stub
0x250eedfb4:  e8 8f 40 b9   ldr	w8, [sp, #0x8c]
0x250eedfb8:  08 01 1a 0a   and	w8, w8, w26
0x250eedfbc:  1f 05 00 71   cmp	w8, #0x1
0x250eedfc0:  fb 23 00 f9   str	x27, [sp, #0x40]
0x250eedfc4:  81 01 00 54   b.ne	loc_250eedff4 ; ⤵ 0x30
0x250eedfc8:  e0 23 c0 3d   ldr	q0, [sp, #0x80]
0x250eedfcc:  e0 1b 80 3d   str	q0, [sp, #0x60]
0x250eedfd0:  e8 4b 40 f9   ldr	x8, [sp, #0x90]
0x250eedfd4:  e8 3b 00 f9   str	x8, [sp, #0x70]
0x250eedfd8:  e2 83 01 91   add	x2, sp, #0x60
0x250eedfdc:  e0 03 19 aa   mov	x0, x25
0x250eedfe0:  58 cf c4 95   bl	0x258021d40
0x250eedfe4:  e0 23 c0 3d   ldr	q0, [sp, #0x80]
0x250eedfe8:  c0 16 80 3d   str	q0, [x22, #0x50]
0x250eedfec:  e8 4b 40 f9   ldr	x8, [sp, #0x90]
0x250eedff0:  06 00 00 14   b	loc_250eee008 ; ⤵ 0x18
0x250eedff4:  ; loc_250eedff4
0x250eedff4:  a8 8e 0b f0   adrp	x8, 0x2680c4000
0x250eedff8:  08 81 43 f9   ldr	x8, [x8, #0x700] ; _ptr._kCMTimeInvalid
0x250eedffc:  00 01 c0 3d   ldr	q0, [x8]
0x250eee000:  c0 16 80 3d   str	q0, [x22, #0x50]
0x250eee004:  08 09 40 f9   ldr	x8, [x8, #0x10]
0x250eee008:  ; loc_250eee008
0x250eee008:  c8 32 00 f9   str	x8, [x22, #0x60]
0x250eee00c:  fc 2b 00 f9   str	x28, [sp, #0x50]
0x250eee010:  c2 5f 12 d0   adrp	x2, 0x275ae8000
0x250eee014:  42 a0 3b 91   add	x2, x2, #0xee8 ; _ptr.__NSCFConstantString
0x250eee018:  e0 03 15 aa   mov	x0, x21
0x250eee01c:  75 4e c4 95   bl	0x2580019f0
0x250eee020:  20 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eee024:  fb 03 00 aa   mov	x27, x0
0x250eee028:  c0 00 00 b4   cbz	x0, loc_250eee040 ; ⤵ 0x18
0x250eee02c:  e0 03 1b aa   mov	x0, x27
0x250eee030:  68 4c c4 95   bl	0x2580011d0
0x250eee034:  e2 03 00 aa   mov	x2, x0
0x250eee038:  e0 03 19 aa   mov	x0, x25
0x250eee03c:  29 cf c4 95   bl	0x258021ce0
0x250eee040:  ; loc_250eee040
0x250eee040:  e0 03 19 aa   mov	x0, x25
0x250eee044:  02 00 80 52   mov	w2, #0
0x250eee048:  82 ce c4 95   bl	0x258021a50
0x250eee04c:  a8 8e 0b d0   adrp	x8, 0x2680c4000
0x250eee050:  08 ad 47 f9   ldr	x8, [x8, #0xf58] ; _ptr._kFigEndpointPlaybackSessionProperty_AudioMode
0x250eee054:  02 01 40 f9   ldr	x2, [x8]
0x250eee058:  e0 03 15 aa   mov	x0, x21
0x250eee05c:  65 4e c4 95   bl	0x2580019f0
0x250eee060:  10 4b c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eee064:  fc 03 00 aa   mov	x28, x0
0x250eee068:  a0 01 00 b4   cbz	x0, loc_250eee09c ; ⤵ 0x34
0x250eee06c:  28 c3 0e b0   adrp	x8, 0x26e753000
0x250eee070:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eee074:  1f 79 00 71   cmp	w8, #0x1e
0x250eee078:  2c 01 00 54   b.gt	loc_250eee09c ; ⤵ 0x24
0x250eee07c:  1f 05 00 31   cmn	w8, #0x1
0x250eee080:  61 0d 00 54   b.ne	loc_250eee22c ; ⤵ 0x1ac
0x250eee084:  20 c3 0e b0   adrp	x0, 0x26e753000
0x250eee088:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eee08c:  c1 03 80 52   mov	w1, #0x1e
0x250eee090:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eee094:  4b 4b c8 95   bl	__LogCategory_Initialize_stub
0x250eee098:  a0 0c 00 35   cbnz	w0, loc_250eee22c ; ⤵ 0x194
0x250eee09c:  ; loc_250eee09c
0x250eee09c:  c8 82 40 39   ldrb	w8, [x22, #0x20]
0x250eee0a0:  1f 05 00 71   cmp	w8, #0x1
0x250eee0a4:  81 00 00 54   b.ne	loc_250eee0b4 ; ⤵ 0x10
0x250eee0a8:  e0 03 16 aa   mov	x0, x22
0x250eee0ac:  e2 03 19 aa   mov	x2, x25
0x250eee0b0:  d4 cb c4 95   bl	0x258021000
0x250eee0b4:  ; loc_250eee0b4
0x250eee0b4:  a8 8e 0b d0   adrp	x8, 0x2680c4000
0x250eee0b8:  08 fd 46 f9   ldr	x8, [x8, #0xdf8] ; _ptr._kFigEndpointPlaybackSessionKey_MetricEventsToSubscribe
0x250eee0bc:  02 01 40 f9   ldr	x2, [x8]
0x250eee0c0:  e0 03 15 aa   mov	x0, x21
0x250eee0c4:  4b 4e c4 95   bl	0x2580019f0
0x250eee0c8:  f6 4a c8 95   bl	_objc_claimAutoreleasedReturnValue_stub
0x250eee0cc:  fa 03 00 aa   mov	x26, x0
0x250eee0d0:  80 01 00 b4   cbz	x0, loc_250eee100 ; ⤵ 0x30
0x250eee0d4:  a8 8e 0b 90   adrp	x8, 0x2680c2000
0x250eee0d8:  00 3d 47 f9   ldr	x0, [x8, #0xe78] ; _ptr.NSArray
0x250eee0dc:  99 4b c8 95   bl	_objc_opt_class_stub
0x250eee0e0:  e1 03 00 aa   mov	x1, x0
0x250eee0e4:  e0 03 1a aa   mov	x0, x26
0x250eee0e8:  fa 4e c8 95   bl	_objc_opt_isKindOfClass_stub
0x250eee0ec:  a0 00 00 36   tbz	w0, #0, loc_250eee100 ; ⤵ 0x14
0x250eee0f0:  e0 03 16 aa   mov	x0, x22
0x250eee0f4:  e2 03 1a aa   mov	x2, x26
0x250eee0f8:  e3 03 19 aa   mov	x3, x25
0x250eee0fc:  9d cb c4 95   bl	0x258020f70
0x250eee100:  ; loc_250eee100
0x250eee100:  e0 03 16 aa   mov	x0, x22
0x250eee104:  e2 03 19 aa   mov	x2, x25
0x250eee108:  f6 0f 40 f9   ldr	x22, [sp, #0x18]
0x250eee10c:  e3 03 16 aa   mov	x3, x22
0x250eee110:  44 cb c4 95   bl	0x258020e20
0x250eee114:  28 c3 0e b0   adrp	x8, 0x26e753000
0x250eee118:  08 29 43 b9   ldr	w8, [x8, #0x328] ; _gLogCategory_AirPlayReceiverKit
0x250eee11c:  1f 79 00 71   cmp	w8, #0x1e
0x250eee120:  2c 01 00 54   b.gt	loc_250eee144 ; ⤵ 0x24
0x250eee124:  1f 05 00 31   cmn	w8, #0x1
0x250eee128:  21 07 00 54   b.ne	loc_250eee20c ; ⤵ 0xe4
0x250eee12c:  20 c3 0e b0   adrp	x0, 0x26e753000
0x250eee130:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eee134:  c1 03 80 52   mov	w1, #0x1e
0x250eee138:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eee13c:  21 4b c8 95   bl	__LogCategory_Initialize_stub
0x250eee140:  60 06 00 35   cbnz	w0, loc_250eee20c ; ⤵ 0xcc
0x250eee144:  ; loc_250eee144
0x250eee144:  2f 49 c8 95   bl	_objc_release_x26_stub
0x250eee148:  36 49 c8 95   bl	_objc_release_x28_stub
0x250eee14c:  31 49 c8 95   bl	_objc_release_x27_stub
0x250eee150:  10 49 c8 95   bl	_objc_release_x19_stub
0x250eee154:  23 49 c8 95   bl	_objc_release_x24_stub
0x250eee158:  1e 49 c8 95   bl	_objc_release_x23_stub
0x250eee15c:  e8 13 40 f9   ldr	x8, [sp, #0x20]
0x250eee160:  34 49 c8 95   bl	_objc_release_x8_stub
0x250eee164:  e8 17 40 f9   ldr	x8, [sp, #0x28]
0x250eee168:  32 49 c8 95   bl	_objc_release_x8_stub
0x250eee16c:  0d 49 c8 95   bl	_objc_release_x20_stub
0x250eee170:  14 49 c8 95   bl	_objc_release_x22_stub
0x250eee174:  f8 ef 43 a9   ldp	x24, x27, [sp, #0x38]
0x250eee178:  fc 2b 40 f9   ldr	x28, [sp, #0x50]
0x250eee17c:  f3 1b 40 f9   ldr	x19, [sp, #0x30]
0x250eee180:  ; loc_250eee180
0x250eee180:  e8 27 40 f9   ldr	x8, [sp, #0x48]
0x250eee184:  2b 49 c8 95   bl	_objc_release_x8_stub
0x250eee188:  16 49 c8 95   bl	_objc_release_x24_stub
0x250eee18c:  01 49 c8 95   bl	_objc_release_x19_stub
0x250eee190:  18 49 c8 95   bl	_objc_release_x25_stub
0x250eee194:  ; loc_250eee194
0x250eee194:  23 49 c8 95   bl	_objc_release_x28_stub
0x250eee198:  06 49 c8 95   bl	_objc_release_x21_stub
0x250eee19c:  1d 49 c8 95   bl	_objc_release_x27_stub
0x250eee1a0:  a8 03 5a f8   ldur	x8, [fp, #-0x60]
0x250eee1a4:  a9 95 0b d0   adrp	x9, 0x2681a4000
0x250eee1a8:  29 39 45 f9   ldr	x9, [x9, #0xa70] ; _ptr.___stack_chk_guard
0x250eee1ac:  29 01 40 f9   ldr	x9, [x9]
0x250eee1b0:  3f 01 08 eb   cmp	x9, x8
0x250eee1b4:  c1 04 00 54   b.ne	loc_250eee24c ; ⤵ 0x98
0x250eee1b8:  fd 7b 55 a9   ldp	fp, lr, [sp, #0x150]
0x250eee1bc:  f4 4f 54 a9   ldp	x20, x19, [sp, #0x140]
0x250eee1c0:  f6 57 53 a9   ldp	x22, x21, [sp, #0x130]
0x250eee1c4:  f8 5f 52 a9   ldp	x24, x23, [sp, #0x120]
0x250eee1c8:  fa 67 51 a9   ldp	x26, x25, [sp, #0x110]
0x250eee1cc:  fc 6f 50 a9   ldp	x28, x27, [sp, #0x100]
0x250eee1d0:  ff 83 05 91   add	sp, sp, #0x160
0x250eee1d4:  ff 0f 5f d6   retab
0x250eee1d8:  ; loc_250eee1d8
0x250eee1d8:  20 c3 0e b0   adrp	x0, 0x26e753000
0x250eee1dc:  00 a0 0c 91   add	x0, x0, #0x328 ; _gLogCategory_AirPlayReceiverKit
0x250eee1e0:  c1 03 80 52   mov	w1, #0x1e
0x250eee1e4:  01 40 a0 72   movk	w1, #0x200, lsl #0x10
0x250eee1e8:  f6 4a c8 95   bl	__LogCategory_Initialize_stub
0x250eee1ec:  00 e7 ff 35   cbnz	w0, loc_250eedecc ; ⤴ -0x320
0x250eee1f0:  e4 ff ff 17   b	loc_250eee180 ; ⤴ -0x70
0x250eee1f4:  ; loc_250eee1f4
0x250eee1f4:  e0 03 1b aa   mov	x0, x27
0x250eee1f8:  3d 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.1
0x250eee1fc:  9f fd ff 17   b	loc_250eed878 ; ⤴ -0x984
0x250eee200:  ; loc_250eee200
0x250eee200:  e0 03 19 aa   mov	x0, x25
0x250eee204:  77 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.6
0x250eee208:  fb fe ff 17   b	loc_250eeddf4 ; ⤴ -0x414
0x250eee20c:  ; loc_250eee20c
0x250eee20c:  9d 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.9
0x250eee210:  cd ff ff 17   b	loc_250eee144 ; ⤴ -0xcc
0x250eee214:  ; loc_250eee214
0x250eee214:  e0 03 16 aa   mov	x0, x22
0x250eee218:  43 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.2
0x250eee21c:  de fd ff 17   b	loc_250eed994 ; ⤴ -0x888
0x250eee220:  ; loc_250eee220
0x250eee220:  e0 03 13 aa   mov	x0, x19
0x250eee224:  4d 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.3
0x250eee228:  2b fe ff 17   b	loc_250eedad4 ; ⤴ -0x754
0x250eee22c:  ; loc_250eee22c
0x250eee22c:  8b 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.8
0x250eee230:  9b ff ff 17   b	loc_250eee09c ; ⤴ -0x194
0x250eee234:  ; loc_250eee234
0x250eee234:  7f 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.7
0x250eee238:  3d ff ff 17   b	loc_250eedf2c ; ⤴ -0x30c
0x250eee23c:  ; loc_250eee23c
0x250eee23c:  5f 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.5
0x250eee240:  97 fe ff 17   b	loc_250eedc9c ; ⤴ -0x5a4
0x250eee244:  ; loc_250eee244
0x250eee244:  53 75 00 94   bl	-[APRKMediaPlayer _insertPlayQueueItemWithDictionary:].cold.4
0x250eee248:  6a fe ff 17   b	loc_250eedbf0 ; ⤴ -0x658
0x250eee24c:  ; loc_250eee24c
0x250eee24c:  99 48 c8 95   bl	___stack_chk_fail_stub

