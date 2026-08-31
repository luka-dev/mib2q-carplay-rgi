# Disassembly of the exported C entry points of Apple's APAC decoder
# libSPF_CP_PlugInWrapper.so, Pioneer SPHERA 1.030010, AArch64
# objdump -d -C --start-address=0x105670 --stop-address=0x106e00


/opt/apac/lib/libSPF_CP_PlugInWrapper.so:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000105670 <GetKBDWindowCoefficientPtr@@Base+0x70>:
  105670:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  105674:	f907f13f 	str	xzr, [x9, #4064]
  105678:	f000192e 	adrp	x14, 42c000 <aStateConstant2State@@Base+0x178>
  10567c:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  105680:	51002030 	sub	w16, w1, #0x8
  105684:	910003fd 	mov	x29, sp
  105688:	f94701c4 	ldr	x4, [x14, #3584]
  10568c:	528dac2c 	mov	w12, #0x6d61                	// #28001
  105690:	72acce4c 	movk	w12, #0x6672, lsl #16
  105694:	528c2c6d 	mov	w13, #0x6163                	// #24931
  105698:	72ac2e0d 	movk	w13, #0x6170, lsl #16
  10569c:	f9400085 	ldr	x5, [x4]
  1056a0:	f9000fe5 	str	x5, [sp, #24]
  1056a4:	d2800005 	mov	x5, #0x0                   	// #0
  1056a8:	528e0c2f 	mov	w15, #0x7061                	// #28769
  1056ac:	72ac8c2f 	movk	w15, #0x6461, lsl #16
  1056b0:	5280000b 	mov	w11, #0x0                   	// #0
  1056b4:	5280000a 	mov	w10, #0x0                   	// #0
  1056b8:	8b2a4008 	add	x8, x0, w10, uxtw
  1056bc:	b86a4806 	ldr	w6, [x0, w10, uxtw]
  1056c0:	5ac008c6 	rev	w6, w6
  1056c4:	b9400505 	ldr	w5, [x8, #4]
  1056c8:	53185ca4 	lsl	w4, w5, #8
  1056cc:	53087ca7 	lsr	w7, w5, #8
  1056d0:	12101c84 	and	w4, w4, #0xff0000
  1056d4:	12181ce7 	and	w7, w7, #0xff00
  1056d8:	53187ca9 	lsr	w9, w5, #24
  1056dc:	2a070084 	orr	w4, w4, w7
  1056e0:	2a056125 	orr	w5, w9, w5, lsl #24
  1056e4:	2a050084 	orr	w4, w4, w5
  1056e8:	6b0c009f 	cmp	w4, w12
  1056ec:	54000240 	b.eq	105734 <GetKBDWindowCoefficientPtr@@Base+0x134>  // b.none
  1056f0:	6b0d009f 	cmp	w4, w13
  1056f4:	540003e0 	b.eq	105770 <GetKBDWindowCoefficientPtr@@Base+0x170>  // b.none
  1056f8:	6b0f009f 	cmp	w4, w15
  1056fc:	54000520 	b.eq	1057a0 <GetKBDWindowCoefficientPtr@@Base+0x1a0>  // b.none
  105700:	34000344 	cbz	w4, 105768 <GetKBDWindowCoefficientPtr@@Base+0x168>
  105704:	350005eb 	cbnz	w11, 1057c0 <GetKBDWindowCoefficientPtr@@Base+0x1c0>
  105708:	b900004b 	str	w11, [x2]
  10570c:	52800020 	mov	w0, #0x1                   	// #1
  105710:	b9000061 	str	w1, [x3]
  105714:	d503201f 	nop
  105718:	f94701ce 	ldr	x14, [x14, #3584]
  10571c:	f9400fe2 	ldr	x2, [sp, #24]
  105720:	f94001c1 	ldr	x1, [x14]
  105724:	ca010041 	eor	x1, x2, x1
  105728:	b5000601 	cbnz	x1, 1057e8 <GetKBDWindowCoefficientPtr@@Base+0x1e8>
  10572c:	a8c27bfd 	ldp	x29, x30, [sp], #32
  105730:	d65f03c0 	ret
  105734:	71002cdf 	cmp	w6, #0xb
  105738:	5200016b 	eor	w11, w11, #0x1
  10573c:	1a9f97e4 	cset	w4, hi  // hi = pmore
  105740:	6a04017f 	tst	w11, w4
  105744:	54000120 	b.eq	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.none
  105748:	4b0a0024 	sub	w4, w1, w10
  10574c:	6b0400df 	cmp	w6, w4
  105750:	540000c8 	b.hi	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.pmore
  105754:	b9400904 	ldr	w4, [x8, #8]
  105758:	5ac00884 	rev	w4, w4
  10575c:	6b0d009f 	cmp	w4, w13
  105760:	540003a0 	b.eq	1057d4 <GetKBDWindowCoefficientPtr@@Base+0x1d4>  // b.none
  105764:	d503201f 	nop
  105768:	52800000 	mov	w0, #0x0                   	// #0
  10576c:	17ffffeb 	b	105718 <GetKBDWindowCoefficientPtr@@Base+0x118>
  105770:	710030df 	cmp	w6, #0xc
  105774:	54ffffa9 	b.ls	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.plast
  105778:	4b0a0024 	sub	w4, w1, w10
  10577c:	6b0400df 	cmp	w6, w4
  105780:	54ffff48 	b.hi	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.pmore
  105784:	b9400904 	ldr	w4, [x8, #8]
  105788:	35ffff04 	cbnz	w4, 105768 <GetKBDWindowCoefficientPtr@@Base+0x168>
  10578c:	0b06014a 	add	w10, w10, w6
  105790:	6b10015f 	cmp	w10, w16
  105794:	54fff923 	b.cc	1056b8 <GetKBDWindowCoefficientPtr@@Base+0xb8>  // b.lo, b.ul, b.last
  105798:	52800000 	mov	w0, #0x0                   	// #0
  10579c:	17ffffdf 	b	105718 <GetKBDWindowCoefficientPtr@@Base+0x118>
  1057a0:	710030df 	cmp	w6, #0xc
  1057a4:	54fffe29 	b.ls	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.plast
  1057a8:	4b0a0021 	sub	w1, w1, w10
  1057ac:	6b0100df 	cmp	w6, w1
  1057b0:	54fffdc8 	b.hi	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.pmore
  1057b4:	1100314b 	add	w11, w10, #0xc
  1057b8:	510030c1 	sub	w1, w6, #0xc
  1057bc:	17ffffd3 	b	105708 <GetKBDWindowCoefficientPtr@@Base+0x108>
  1057c0:	71001cdf 	cmp	w6, #0x7
  1057c4:	54fffd29 	b.ls	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.plast
  1057c8:	4b0a0024 	sub	w4, w1, w10
  1057cc:	6b0400df 	cmp	w6, w4
  1057d0:	54fffcc8 	b.hi	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.pmore
  1057d4:	0b06014a 	add	w10, w10, w6
  1057d8:	6b10015f 	cmp	w10, w16
  1057dc:	54fffc62 	b.cs	105768 <GetKBDWindowCoefficientPtr@@Base+0x168>  // b.hs, b.nlast
  1057e0:	5280002b 	mov	w11, #0x1                   	// #1
  1057e4:	17ffffb5 	b	1056b8 <GetKBDWindowCoefficientPtr@@Base+0xb8>
  1057e8:	97fda8ca 	bl	6fb10 <__stack_chk_fail@plt>
  1057ec:	d503201f 	nop
  1057f0:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  1057f4:	f907f13f 	str	xzr, [x9, #4064]
  1057f8:	f0001927 	adrp	x7, 42c000 <aStateConstant2State@@Base+0x178>
  1057fc:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  105800:	aa0003e5 	mov	x5, x0
  105804:	910003fd 	mov	x29, sp
  105808:	f94700e4 	ldr	x4, [x7, #3584]
  10580c:	aa0103e0 	mov	x0, x1
  105810:	7100307f 	cmp	w3, #0xc
  105814:	f9400081 	ldr	x1, [x4]
  105818:	f9000fe1 	str	x1, [sp, #24]
  10581c:	d2800001 	mov	x1, #0x0                   	// #0
  105820:	54000501 	b.ne	1058c0 <GetKBDWindowCoefficientPtr@@Base+0x2c0>  // b.any
  105824:	7100005f 	cmp	w2, #0x0
  105828:	8b224406 	add	x6, x0, w2, uxtw #1
  10582c:	fa461002 	ccmp	x0, x6, #0x2, ne  // ne = any
  105830:	52800004 	mov	w4, #0x0                   	// #0
  105834:	54000382 	b.cs	1058a4 <GetKBDWindowCoefficientPtr@@Base+0x2a4>  // b.hs, b.nlast
  105838:	529fc001 	mov	w1, #0xfe00                	// #65024
  10583c:	72a8dfe1 	movk	w1, #0x46ff, lsl #16
  105840:	1e2e1002 	fmov	s2, #1.000000000000000000e+00
  105844:	1e270023 	fmov	s3, w1
  105848:	1e3e1001 	fmov	s1, #-1.000000000000000000e+00
  10584c:	14000009 	b	105870 <GetKBDWindowCoefficientPtr@@Base+0x270>
  105850:	1e230800 	fmul	s0, s0, s3
  105854:	11000484 	add	w4, w4, #0x1
  105858:	6b04005f 	cmp	w2, w4
  10585c:	1e380003 	fcvtzs	w3, s0
  105860:	13003c63 	sxth	w3, w3
  105864:	78002403 	strh	w3, [x0], #2
  105868:	fa4080c0 	ccmp	x6, x0, #0x0, hi  // hi = pmore
  10586c:	540001c9 	b.ls	1058a4 <GetKBDWindowCoefficientPtr@@Base+0x2a4>  // b.plast
  105870:	f94000a1 	ldr	x1, [x5]
  105874:	bc645820 	ldr	s0, [x1, w4, uxtw #2]
  105878:	1e207840 	fminnm	s0, s2, s0
  10587c:	1e206820 	fmaxnm	s0, s1, s0
  105880:	1e202018 	fcmpe	s0, #0.0
  105884:	54fffe65 	b.pl	105850 <GetKBDWindowCoefficientPtr@@Base+0x250>  // b.nfrst
  105888:	1e18c403 	fcvtzs	w3, s0, #15
  10588c:	11000484 	add	w4, w4, #0x1
  105890:	6b04005f 	cmp	w2, w4
  105894:	13003c63 	sxth	w3, w3
  105898:	78002403 	strh	w3, [x0], #2
  10589c:	fa4080c0 	ccmp	x6, x0, #0x0, hi  // hi = pmore
  1058a0:	54fffe88 	b.hi	105870 <GetKBDWindowCoefficientPtr@@Base+0x270>  // b.pmore
  1058a4:	f94700e7 	ldr	x7, [x7, #3584]
  1058a8:	f9400fe1 	ldr	x1, [sp, #24]
  1058ac:	f94000e0 	ldr	x0, [x7]
  1058b0:	ca000020 	eor	x0, x1, x0
  1058b4:	b5000160 	cbnz	x0, 1058e0 <GetKBDWindowCoefficientPtr@@Base+0x2e0>
  1058b8:	a8c27bfd 	ldp	x29, x30, [sp], #32
  1058bc:	d65f03c0 	ret
  1058c0:	f9400fe1 	ldr	x1, [sp, #24]
  1058c4:	f9400083 	ldr	x3, [x4]
  1058c8:	ca030023 	eor	x3, x1, x3
  1058cc:	d37e7c42 	ubfiz	x2, x2, #2, #32
  1058d0:	f94000a1 	ldr	x1, [x5]
  1058d4:	b5000063 	cbnz	x3, 1058e0 <GetKBDWindowCoefficientPtr@@Base+0x2e0>
  1058d8:	a8c27bfd 	ldp	x29, x30, [sp], #32
  1058dc:	17fda825 	b	6f970 <memcpy@plt>
  1058e0:	97fda88c 	bl	6fb10 <__stack_chk_fail@plt>
  1058e4:	d503201f 	nop
  1058e8:	d503201f 	nop
  1058ec:	d503201f 	nop
  1058f0:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  1058f4:	f907b93f 	str	xzr, [x9, #3952]
  1058f8:	a9b77bfd 	stp	x29, x30, [sp, #-144]!
  1058fc:	528000a1 	mov	w1, #0x5                   	// #5
  105900:	910003fd 	mov	x29, sp
  105904:	a90153f3 	stp	x19, x20, [sp, #16]
  105908:	f0001933 	adrp	x19, 42c000 <aStateConstant2State@@Base+0x178>
  10590c:	910123e2 	add	x2, sp, #0x48
  105910:	f9400000 	ldr	x0, [x0]
  105914:	f90027ff 	str	xzr, [sp, #72]
  105918:	f9470264 	ldr	x4, [x19, #3584]
  10591c:	f9400003 	ldr	x3, [x0]
  105920:	f9400085 	ldr	x5, [x4]
  105924:	f90047e5 	str	x5, [sp, #136]
  105928:	d2800005 	mov	x5, #0x0                   	// #0
  10592c:	f9401463 	ldr	x3, [x3, #40]
  105930:	d63f0060 	blr	x3
  105934:	35000ac0 	cbnz	w0, 105a8c <GetKBDWindowCoefficientPtr@@Base+0x48c>
  105938:	f94027e0 	ldr	x0, [sp, #72]
  10593c:	b40009c0 	cbz	x0, 105a74 <GetKBDWindowCoefficientPtr@@Base+0x474>
  105940:	f9400001 	ldr	x1, [x0]
  105944:	f9400821 	ldr	x1, [x1, #16]
  105948:	d63f0020 	blr	x1
  10594c:	90001941 	adrp	x1, 42d000 <aStateConstant2State@@Base+0x1178>
  105950:	f9400400 	ldr	x0, [x0, #8]
  105954:	f943a421 	ldr	x1, [x1, #1864]
  105958:	eb01001f 	cmp	x0, x1
  10595c:	540006e0 	b.eq	105a38 <GetKBDWindowCoefficientPtr@@Base+0x438>  // b.none
  105960:	39c00002 	ldrsb	w2, [x0]
  105964:	7100a85f 	cmp	w2, #0x2a
  105968:	54000060 	b.eq	105974 <GetKBDWindowCoefficientPtr@@Base+0x374>  // b.none
  10596c:	97fdaad1 	bl	704b0 <strcmp@plt>
  105970:	34000640 	cbz	w0, 105a38 <GetKBDWindowCoefficientPtr@@Base+0x438>
  105974:	a9025bf5 	stp	x21, x22, [sp, #32]
  105978:	90001956 	adrp	x22, 42d000 <aStateConstant2State@@Base+0x1178>
  10597c:	d2800700 	mov	x0, #0x38                  	// #56
  105980:	f9474ac1 	ldr	x1, [x22, #3728]
  105984:	f9001bf7 	str	x23, [sp, #48]
  105988:	910163f7 	add	x23, sp, #0x58
  10598c:	91004021 	add	x1, x1, #0x10
  105990:	f9002be1 	str	x1, [sp, #80]
  105994:	97fda8ab 	bl	6fc40 <__cxa_allocate_exception@plt>
  105998:	aa0003f3 	mov	x19, x0
  10599c:	f0001923 	adrp	x3, 42c000 <aStateConstant2State@@Base+0x178>
  1059a0:	90001941 	adrp	x1, 42d000 <aStateConstant2State@@Base+0x1178>
  1059a4:	91002000 	add	x0, x0, #0x8
  1059a8:	12800006 	mov	w6, #0xffffffff            	// #-1
  1059ac:	f9436021 	ldr	x1, [x1, #1728]
  1059b0:	f9000a7f 	str	xzr, [x19, #16]
  1059b4:	f9468063 	ldr	x3, [x3, #3328]
  1059b8:	91004027 	add	x7, x1, #0x10
  1059bc:	9100e022 	add	x2, x1, #0x38
  1059c0:	b0001925 	adrp	x5, 42a000 <sfbOffsetTables@@Base+0x520>
  1059c4:	911a40a5 	add	x5, x5, #0x690
  1059c8:	91014063 	add	x3, x3, #0x50
  1059cc:	b0001924 	adrp	x4, 42a000 <sfbOffsetTables@@Base+0x520>
  1059d0:	911b8084 	add	x4, x4, #0x6e0
  1059d4:	a9017c1f 	stp	xzr, xzr, [x0, #16]
  1059d8:	910022e1 	add	x1, x23, #0x8
  1059dc:	b9002006 	str	w6, [x0, #32]
  1059e0:	a9000e65 	stp	x5, x3, [x19]
  1059e4:	f9001a64 	str	x4, [x19, #48]
  1059e8:	a9058be7 	stp	x7, x2, [sp, #88]
  1059ec:	a906ffff 	stp	xzr, xzr, [sp, #104]
  1059f0:	f9003fff 	str	xzr, [sp, #120]
  1059f4:	b90083e6 	str	w6, [sp, #128]
  1059f8:	97fde0f2 	bl	7ddc0 <boost::exception_detail::copy_boost_exception(boost::exception*, boost::exception const*)@@Base>
  1059fc:	90001941 	adrp	x1, 42d000 <aStateConstant2State@@Base+0x1178>
  105a00:	aa1703e0 	mov	x0, x23
  105a04:	f9471421 	ldr	x1, [x1, #3624]
  105a08:	91014022 	add	x2, x1, #0x50
  105a0c:	91006023 	add	x3, x1, #0x18
  105a10:	91022021 	add	x1, x1, #0x88
  105a14:	a9000a63 	stp	x3, x2, [x19]
  105a18:	f9001a61 	str	x1, [x19, #48]
  105a1c:	97fdc1ed 	bl	761d0 <boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector()@@Base>
  105a20:	90001942 	adrp	x2, 42d000 <aStateConstant2State@@Base+0x1178>
  105a24:	90001941 	adrp	x1, 42d000 <aStateConstant2State@@Base+0x1178>
  105a28:	aa1303e0 	mov	x0, x19
  105a2c:	f9438042 	ldr	x2, [x2, #1792]
  105a30:	f940cc21 	ldr	x1, [x1, #408]
  105a34:	97fdab2b 	bl	706e0 <__cxa_throw@plt>
  105a38:	f94027e0 	ldr	x0, [sp, #72]
  105a3c:	f9400414 	ldr	x20, [x0, #8]
  105a40:	b4000394 	cbz	x20, 105ab0 <GetKBDWindowCoefficientPtr@@Base+0x4b0>
  105a44:	f9400001 	ldr	x1, [x0]
  105a48:	f9400421 	ldr	x1, [x1, #8]
  105a4c:	d63f0020 	blr	x1
  105a50:	f9470273 	ldr	x19, [x19, #3584]
  105a54:	f94047e1 	ldr	x1, [sp, #136]
  105a58:	f9400260 	ldr	x0, [x19]
  105a5c:	ca000020 	eor	x0, x1, x0
  105a60:	b5000100 	cbnz	x0, 105a80 <GetKBDWindowCoefficientPtr@@Base+0x480>
  105a64:	aa1403e0 	mov	x0, x20
  105a68:	a94153f3 	ldp	x19, x20, [sp, #16]
  105a6c:	a8c97bfd 	ldp	x29, x30, [sp], #144
  105a70:	d65f03c0 	ret
  105a74:	90001940 	adrp	x0, 42d000 <aStateConstant2State@@Base+0x1178>
  105a78:	f941e800 	ldr	x0, [x0, #976]
  105a7c:	17ffffb4 	b	10594c <GetKBDWindowCoefficientPtr@@Base+0x34c>
  105a80:	a9025bf5 	stp	x21, x22, [sp, #32]
  105a84:	f9001bf7 	str	x23, [sp, #48]
  105a88:	97fda822 	bl	6fb10 <__stack_chk_fail@plt>
  105a8c:	d2800080 	mov	x0, #0x4                   	// #4
  105a90:	97fda86c 	bl	6fc40 <__cxa_allocate_exception@plt>
  105a94:	f0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  105a98:	528c8c43 	mov	w3, #0x6462                	// #25698
  105a9c:	72ac4c23 	movk	w3, #0x6261, lsl #16
  105aa0:	b9000003 	str	w3, [x0]
  105aa4:	f944fc21 	ldr	x1, [x1, #2552]
  105aa8:	d2800002 	mov	x2, #0x0                   	// #0
  105aac:	97fdab0d 	bl	706e0 <__cxa_throw@plt>
  105ab0:	d2800080 	mov	x0, #0x4                   	// #4
  105ab4:	97fda863 	bl	6fc40 <__cxa_allocate_exception@plt>
  105ab8:	f0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  105abc:	528c8c43 	mov	w3, #0x6462                	// #25698
  105ac0:	72ac4c23 	movk	w3, #0x6261, lsl #16
  105ac4:	b9000003 	str	w3, [x0]
  105ac8:	f944fc21 	ldr	x1, [x1, #2552]
  105acc:	d2800002 	mov	x2, #0x0                   	// #0
  105ad0:	97fdab04 	bl	706e0 <__cxa_throw@plt>
  105ad4:	aa0003f3 	mov	x19, x0
  105ad8:	a9025bf5 	stp	x21, x22, [sp, #32]
  105adc:	f9001bf7 	str	x23, [sp, #48]
  105ae0:	f94027e0 	ldr	x0, [sp, #72]
  105ae4:	b4000080 	cbz	x0, 105af4 <GetKBDWindowCoefficientPtr@@Base+0x4f4>
  105ae8:	f9400001 	ldr	x1, [x0]
  105aec:	f9400421 	ldr	x1, [x1, #8]
  105af0:	d63f0020 	blr	x1
  105af4:	aa1303e0 	mov	x0, x19
  105af8:	97fdab12 	bl	70740 <_Unwind_Resume@plt>
  105afc:	aa0103f5 	mov	x21, x1
  105b00:	aa0003f4 	mov	x20, x0
  105b04:	aa1303e0 	mov	x0, x19
  105b08:	97fdc1b2 	bl	761d0 <boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector()@@Base>
  105b0c:	aa1703e0 	mov	x0, x23
  105b10:	97fdc1b0 	bl	761d0 <boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector()@@Base>
  105b14:	aa1303e0 	mov	x0, x19
  105b18:	97fda8ee 	bl	6fed0 <__cxa_free_exception@plt>
  105b1c:	f9474ac1 	ldr	x1, [x22, #3728]
  105b20:	910143e0 	add	x0, sp, #0x50
  105b24:	91004021 	add	x1, x1, #0x10
  105b28:	f9002be1 	str	x1, [sp, #80]
  105b2c:	97fdaaad 	bl	705e0 <std::bad_cast::~bad_cast()@plt>
  105b30:	f10006bf 	cmp	x21, #0x1
  105b34:	540001e1 	b.ne	105b70 <GetKBDWindowCoefficientPtr@@Base+0x570>  // b.any
  105b38:	aa1403e0 	mov	x0, x20
  105b3c:	97fda81d 	bl	6fbb0 <__cxa_begin_catch@plt>
  105b40:	aa0003e1 	mov	x1, x0
  105b44:	d2800080 	mov	x0, #0x4                   	// #4
  105b48:	b9400033 	ldr	w19, [x1]
  105b4c:	97fda83d 	bl	6fc40 <__cxa_allocate_exception@plt>
  105b50:	f0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  105b54:	b9000013 	str	w19, [x0]
  105b58:	d2800002 	mov	x2, #0x0                   	// #0
  105b5c:	f944fc21 	ldr	x1, [x1, #2552]
  105b60:	97fdaae0 	bl	706e0 <__cxa_throw@plt>
  105b64:	aa0003f4 	mov	x20, x0
  105b68:	aa0103f5 	mov	x21, x1
  105b6c:	17ffffec 	b	105b1c <GetKBDWindowCoefficientPtr@@Base+0x51c>
  105b70:	aa1403e0 	mov	x0, x20
  105b74:	97fda80f 	bl	6fbb0 <__cxa_begin_catch@plt>
  105b78:	d2800080 	mov	x0, #0x4                   	// #4
  105b7c:	97fda831 	bl	6fc40 <__cxa_allocate_exception@plt>
  105b80:	f0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  105b84:	52800023 	mov	w3, #0x1                   	// #1
  105b88:	b9000003 	str	w3, [x0]
  105b8c:	d2800002 	mov	x2, #0x0                   	// #0
  105b90:	f944fc21 	ldr	x1, [x1, #2552]
  105b94:	97fdaad3 	bl	706e0 <__cxa_throw@plt>
  105b98:	aa0003f3 	mov	x19, x0
  105b9c:	97fdaaa5 	bl	70630 <__cxa_end_catch@plt>
  105ba0:	17ffffd0 	b	105ae0 <GetKBDWindowCoefficientPtr@@Base+0x4e0>
  105ba4:	aa0003f3 	mov	x19, x0
  105ba8:	97fdaaa2 	bl	70630 <__cxa_end_catch@plt>
  105bac:	17ffffcd 	b	105ae0 <GetKBDWindowCoefficientPtr@@Base+0x4e0>

0000000000105bb0 <APACDecoder_DecodeFrame@@Base>:
  105bb0:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  105bb4:	f907d13f 	str	xzr, [x9, #4000]
  105bb8:	a9ba7bfd 	stp	x29, x30, [sp, #-96]!
  105bbc:	910003fd 	mov	x29, sp
  105bc0:	a9025bf5 	stp	x21, x22, [sp, #32]
  105bc4:	f0001935 	adrp	x21, 42c000 <aStateConstant2State@@Base+0x178>
  105bc8:	a90153f3 	stp	x19, x20, [sp, #16]
  105bcc:	aa0003f3 	mov	x19, x0
  105bd0:	f94702a0 	ldr	x0, [x21, #3584]
  105bd4:	39614266 	ldrb	w6, [x19, #2128]
  105bd8:	f9400007 	ldr	x7, [x0]
  105bdc:	f9002fe7 	str	x7, [sp, #88]
  105be0:	d2800007 	mov	x7, #0x0                   	// #0
  105be4:	34000e86 	cbz	w6, 105db4 <APACDecoder_DecodeFrame@@Base+0x204>
  105be8:	a90363f7 	stp	x23, x24, [sp, #48]
  105bec:	aa0503f7 	mov	x23, x5
  105bf0:	f90023f9 	str	x25, [sp, #64]
  105bf4:	aa0303f9 	mov	x25, x3
  105bf8:	a9400e60 	ldp	x0, x3, [x19]
  105bfc:	f9408065 	ldr	x5, [x3, #256]
  105c00:	b94000a6 	ldr	w6, [x5]
  105c04:	34000746 	cbz	w6, 105cec <APACDecoder_DecodeFrame@@Base+0x13c>
  105c08:	714040df 	cmp	w6, #0x10, lsl #12
  105c0c:	12003cc6 	and	w6, w6, #0xffff
  105c10:	54000620 	b.eq	105cd4 <APACDecoder_DecodeFrame@@Base+0x124>  // b.none
  105c14:	34000502 	cbz	w2, 105cb4 <APACDecoder_DecodeFrame@@Base+0x104>
  105c18:	531654d4 	lsl	w20, w6, #10
  105c1c:	7100029f 	cmp	w20, #0x0
  105c20:	7a441282 	ccmp	w20, w4, #0x2, ne  // ne = any
  105c24:	54000cc8 	b.hi	105dbc <APACDecoder_DecodeFrame@@Base+0x20c>  // b.pmore
  105c28:	f9441a64 	ldr	x4, [x19, #2096]
  105c2c:	aa1303e3 	mov	x3, x19
  105c30:	f9400084 	ldr	x4, [x4]
  105c34:	f8010c64 	str	x4, [x3, #16]!
  105c38:	f9400004 	ldr	x4, [x0]
  105c3c:	f9401084 	ldr	x4, [x4, #32]
  105c40:	d63f0080 	blr	x4
  105c44:	2a0003f6 	mov	w22, w0
  105c48:	aa1303e0 	mov	x0, x19
  105c4c:	97ffff29 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  105c50:	3942d401 	ldrb	w1, [x0, #181]
  105c54:	aa0003f8 	mov	x24, x0
  105c58:	35000501 	cbnz	w1, 105cf8 <APACDecoder_DecodeFrame@@Base+0x148>
  105c5c:	79503a62 	ldrh	w2, [x19, #2076]
  105c60:	b9481a64 	ldr	w4, [x19, #2072]
  105c64:	1b047c42 	mul	w2, w2, w4
  105c68:	35000076 	cbnz	w22, 105c74 <APACDecoder_DecodeFrame@@Base+0xc4>
  105c6c:	6b02029f 	cmp	w20, w2
  105c70:	54000500 	b.eq	105d10 <APACDecoder_DecodeFrame@@Base+0x160>  // b.none
  105c74:	aa1903e0 	mov	x0, x25
  105c78:	d37e7c42 	ubfiz	x2, x2, #2, #32
  105c7c:	52800001 	mov	w1, #0x0                   	// #0
  105c80:	97fda828 	bl	6fd20 <memset@plt>
  105c84:	a94363f7 	ldp	x23, x24, [sp, #48]
  105c88:	f94023f9 	ldr	x25, [sp, #64]
  105c8c:	f94702b5 	ldr	x21, [x21, #3584]
  105c90:	f9402fe1 	ldr	x1, [sp, #88]
  105c94:	f94002a0 	ldr	x0, [x21]
  105c98:	ca000020 	eor	x0, x1, x0
  105c9c:	b5000980 	cbnz	x0, 105dcc <APACDecoder_DecodeFrame@@Base+0x21c>
  105ca0:	2a1603e0 	mov	w0, w22
  105ca4:	a94153f3 	ldp	x19, x20, [sp, #16]
  105ca8:	a9425bf5 	ldp	x21, x22, [sp, #32]
  105cac:	a8c67bfd 	ldp	x29, x30, [sp], #96
  105cb0:	d65f03c0 	ret
  105cb4:	f9400001 	ldr	x1, [x0]
  105cb8:	52800016 	mov	w22, #0x0                   	// #0
  105cbc:	f9400c21 	ldr	x1, [x1, #24]
  105cc0:	d63f0020 	blr	x1
  105cc4:	b9081a7f 	str	wzr, [x19, #2072]
  105cc8:	a94363f7 	ldp	x23, x24, [sp, #48]
  105ccc:	f94023f9 	ldr	x25, [sp, #64]
  105cd0:	17ffffef 	b	105c8c <APACDecoder_DecodeFrame@@Base+0xdc>
  105cd4:	bd4004a0 	ldr	s0, [x5, #4]
  105cd8:	0e205800 	cnt	v0.8b, v0.8b
  105cdc:	0e31b800 	addv	b0, v0.8b
  105ce0:	0e013c06 	umov	w6, v0.b[0]
  105ce4:	35fff9a2 	cbnz	w2, 105c18 <APACDecoder_DecodeFrame@@Base+0x68>
  105ce8:	17fffff3 	b	105cb4 <APACDecoder_DecodeFrame@@Base+0x104>
  105cec:	b94008a6 	ldr	w6, [x5, #8]
  105cf0:	35fff942 	cbnz	w2, 105c18 <APACDecoder_DecodeFrame@@Base+0x68>
  105cf4:	17fffff0 	b	105cb4 <APACDecoder_DecodeFrame@@Base+0x104>
  105cf8:	52800021 	mov	w1, #0x1                   	// #1
  105cfc:	97feb74d 	bl	b3a30 <DRCConfiguration::SetUniDrcDelayIsSetOnCodec(unsigned char)@@Base>
  105d00:	aa1803e0 	mov	x0, x24
  105d04:	52800001 	mov	w1, #0x0                   	// #0
  105d08:	97feb736 	bl	b39e0 <DRCConfiguration::SetUniDrcDelayChanged(unsigned char)@@Base>
  105d0c:	17ffffd4 	b	105c5c <APACDecoder_DecodeFrame@@Base+0xac>
  105d10:	b9484a74 	ldr	w20, [x19, #2120]
  105d14:	340003d4 	cbz	w20, 105d8c <APACDecoder_DecodeFrame@@Base+0x1dc>
  105d18:	6b04029f 	cmp	w20, w4
  105d1c:	540002c2 	b.cs	105d74 <APACDecoder_DecodeFrame@@Base+0x1c4>  // b.hs, b.nlast
  105d20:	4b140084 	sub	w4, w4, w20
  105d24:	b90002e4 	str	w4, [x23]
  105d28:	79503a63 	ldrh	w3, [x19, #2076]
  105d2c:	f9441a77 	ldr	x23, [x19, #2096]
  105d30:	1b037c94 	mul	w20, w4, w3
  105d34:	d37e7e82 	ubfiz	x2, x20, #2, #32
  105d38:	b40000c2 	cbz	x2, 105d50 <APACDecoder_DecodeFrame@@Base+0x1a0>
  105d3c:	b9484a61 	ldr	w1, [x19, #2120]
  105d40:	f94002e0 	ldr	x0, [x23]
  105d44:	1b017c61 	mul	w1, w3, w1
  105d48:	8b010801 	add	x1, x0, x1, lsl #2
  105d4c:	97fda969 	bl	702f0 <memmove@plt>
  105d50:	b9483e63 	ldr	w3, [x19, #2108]
  105d54:	aa1903e1 	mov	x1, x25
  105d58:	aa1703e0 	mov	x0, x23
  105d5c:	2a1403e2 	mov	w2, w20
  105d60:	97fffea4 	bl	1057f0 <GetKBDWindowCoefficientPtr@@Base+0x1f0>
  105d64:	b9084a7f 	str	wzr, [x19, #2120]
  105d68:	a94363f7 	ldp	x23, x24, [sp, #48]
  105d6c:	f94023f9 	ldr	x25, [sp, #64]
  105d70:	17ffffc7 	b	105c8c <APACDecoder_DecodeFrame@@Base+0xdc>
  105d74:	4b040284 	sub	w4, w20, w4
  105d78:	b9084a64 	str	w4, [x19, #2120]
  105d7c:	b90002ff 	str	wzr, [x23]
  105d80:	a94363f7 	ldp	x23, x24, [sp, #48]
  105d84:	f94023f9 	ldr	x25, [sp, #64]
  105d88:	17ffffc1 	b	105c8c <APACDecoder_DecodeFrame@@Base+0xdc>
  105d8c:	b90002e4 	str	w4, [x23]
  105d90:	aa1903e1 	mov	x1, x25
  105d94:	79503a62 	ldrh	w2, [x19, #2076]
  105d98:	b9483e63 	ldr	w3, [x19, #2108]
  105d9c:	f9441a60 	ldr	x0, [x19, #2096]
  105da0:	1b047c42 	mul	w2, w2, w4
  105da4:	97fffe93 	bl	1057f0 <GetKBDWindowCoefficientPtr@@Base+0x1f0>
  105da8:	a94363f7 	ldp	x23, x24, [sp, #48]
  105dac:	f94023f9 	ldr	x25, [sp, #64]
  105db0:	17ffffb7 	b	105c8c <APACDecoder_DecodeFrame@@Base+0xdc>
  105db4:	128347b6 	mov	w22, #0xffffe5c2            	// #-6718
  105db8:	17ffffb5 	b	105c8c <APACDecoder_DecodeFrame@@Base+0xdc>
  105dbc:	128348f6 	mov	w22, #0xffffe5b8            	// #-6728
  105dc0:	a94363f7 	ldp	x23, x24, [sp, #48]
  105dc4:	f94023f9 	ldr	x25, [sp, #64]
  105dc8:	17ffffb1 	b	105c8c <APACDecoder_DecodeFrame@@Base+0xdc>
  105dcc:	a90363f7 	stp	x23, x24, [sp, #48]
  105dd0:	f90023f9 	str	x25, [sp, #64]
  105dd4:	97fda74f 	bl	6fb10 <__stack_chk_fail@plt>
  105dd8:	d503201f 	nop
  105ddc:	d503201f 	nop

0000000000105de0 <APACDecoder_Reset@@Base>:
  105de0:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  105de4:	f907e93f 	str	xzr, [x9, #4048]
  105de8:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
  105dec:	910003fd 	mov	x29, sp
  105df0:	a90153f3 	stp	x19, x20, [sp, #16]
  105df4:	f0001933 	adrp	x19, 42c000 <aStateConstant2State@@Base+0x178>
  105df8:	f9470261 	ldr	x1, [x19, #3584]
  105dfc:	f9400022 	ldr	x2, [x1]
  105e00:	f90017e2 	str	x2, [sp, #40]
  105e04:	d2800002 	mov	x2, #0x0                   	// #0
  105e08:	b4000280 	cbz	x0, 105e58 <APACDecoder_Reset@@Base+0x78>
  105e0c:	91200001 	add	x1, x0, #0x800
  105e10:	52800014 	mov	w20, #0x0                   	// #0
  105e14:	f804403f 	stur	xzr, [x1, #68]
  105e18:	39614001 	ldrb	w1, [x0, #2128]
  105e1c:	35000141 	cbnz	w1, 105e44 <APACDecoder_Reset@@Base+0x64>
  105e20:	f9470273 	ldr	x19, [x19, #3584]
  105e24:	f94017e1 	ldr	x1, [sp, #40]
  105e28:	f9400260 	ldr	x0, [x19]
  105e2c:	ca000020 	eor	x0, x1, x0
  105e30:	b5000180 	cbnz	x0, 105e60 <APACDecoder_Reset@@Base+0x80>
  105e34:	2a1403e0 	mov	w0, w20
  105e38:	a94153f3 	ldp	x19, x20, [sp, #16]
  105e3c:	a8c37bfd 	ldp	x29, x30, [sp], #48
  105e40:	d65f03c0 	ret
  105e44:	f9400000 	ldr	x0, [x0]
  105e48:	f9400001 	ldr	x1, [x0]
  105e4c:	f9400c21 	ldr	x1, [x1, #24]
  105e50:	d63f0020 	blr	x1
  105e54:	17fffff3 	b	105e20 <APACDecoder_Reset@@Base+0x40>
  105e58:	12834614 	mov	w20, #0xffffe5cf            	// #-6705
  105e5c:	17fffff1 	b	105e20 <APACDecoder_Reset@@Base+0x40>
  105e60:	97fda72c 	bl	6fb10 <__stack_chk_fail@plt>
  105e64:	d503201f 	nop
  105e68:	d503201f 	nop
  105e6c:	d503201f 	nop

0000000000105e70 <APACDecoder_GetParameter@@Base>:
  105e70:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  105e74:	f907e13f 	str	xzr, [x9, #4032]
  105e78:	a9bc7bfd 	stp	x29, x30, [sp, #-64]!
  105e7c:	f100005f 	cmp	x2, #0x0
  105e80:	fa401864 	ccmp	x3, #0x0, #0x4, ne  // ne = any
  105e84:	910003fd 	mov	x29, sp
  105e88:	a90153f3 	stp	x19, x20, [sp, #16]
  105e8c:	f0001933 	adrp	x19, 42c000 <aStateConstant2State@@Base+0x178>
  105e90:	aa0303f4 	mov	x20, x3
  105e94:	f9470262 	ldr	x2, [x19, #3584]
  105e98:	fa401804 	ccmp	x0, #0x0, #0x4, ne  // ne = any
  105e9c:	f9400043 	ldr	x3, [x2]
  105ea0:	f9001fe3 	str	x3, [sp, #56]
  105ea4:	d2800003 	mov	x3, #0x0                   	// #0
  105ea8:	54000b40 	b.eq	106010 <APACDecoder_GetParameter@@Base+0x1a0>  // b.none
  105eac:	39614002 	ldrb	w2, [x0, #2128]
  105eb0:	34000b42 	cbz	w2, 106018 <APACDecoder_GetParameter@@Base+0x1a8>
  105eb4:	f90013f5 	str	x21, [sp, #32]
  105eb8:	528d6cc2 	mov	w2, #0x6b66                	// #27494
  105ebc:	72ae0c22 	movk	w2, #0x7061, lsl #16
  105ec0:	6b02003f 	cmp	w1, w2
  105ec4:	f9400415 	ldr	x21, [x0, #8]
  105ec8:	54000840 	b.eq	105fd0 <APACDecoder_GetParameter@@Base+0x160>  // b.none
  105ecc:	5400052c 	b.gt	105f70 <APACDecoder_GetParameter@@Base+0x100>
  105ed0:	528e6e40 	mov	w0, #0x7372                	// #29554
  105ed4:	72ac6de0 	movk	w0, #0x636f, lsl #16
  105ed8:	6b00003f 	cmp	w1, w0
  105edc:	54000700 	b.eq	105fbc <APACDecoder_GetParameter@@Base+0x14c>  // b.none
  105ee0:	528d8400 	mov	w0, #0x6c20                	// #27680
  105ee4:	72adec60 	movk	w0, #0x6f63, lsl #16
  105ee8:	6b00003f 	cmp	w1, w0
  105eec:	54000621 	b.ne	105fb0 <APACDecoder_GetParameter@@Base+0x140>  // b.any
  105ef0:	794042a0 	ldrh	w0, [x21, #32]
  105ef4:	97fe5c5f 	bl	9d070 <apac::GetProfileConfig(apac::Profile)@@Base>
  105ef8:	79400002 	ldrh	w2, [x0]
  105efc:	529fffe1 	mov	w1, #0xffff                	// #65535
  105f00:	6b01005f 	cmp	w2, w1
  105f04:	540007e0 	b.eq	106000 <APACDecoder_GetParameter@@Base+0x190>  // b.none
  105f08:	f94082a2 	ldr	x2, [x21, #256]
  105f0c:	a9419001 	ldp	x1, x4, [x0, #24]
  105f10:	b9400043 	ldr	w3, [x2]
  105f14:	34000723 	cbz	w3, 105ff8 <APACDecoder_GetParameter@@Base+0x188>
  105f18:	7140407f 	cmp	w3, #0x10, lsl #12
  105f1c:	12003c63 	and	w3, w3, #0xffff
  105f20:	540000a1 	b.ne	105f34 <APACDecoder_GetParameter@@Base+0xc4>  // b.any
  105f24:	bd400440 	ldr	s0, [x2, #4]
  105f28:	0e205800 	cnt	v0.8b, v0.8b
  105f2c:	0e31b800 	addv	b0, v0.8b
  105f30:	0e013c03 	umov	w3, v0.b[0]
  105f34:	eb04003f 	cmp	x1, x4
  105f38:	52800005 	mov	w5, #0x0                   	// #0
  105f3c:	540000e1 	b.ne	105f58 <APACDecoder_GetParameter@@Base+0xe8>  // b.any
  105f40:	1400002a 	b	105fe8 <APACDecoder_GetParameter@@Base+0x178>
  105f44:	6b20207f 	cmp	w3, w0, uxth
  105f48:	540004e0 	b.eq	105fe4 <APACDecoder_GetParameter@@Base+0x174>  // b.none
  105f4c:	91001021 	add	x1, x1, #0x4
  105f50:	eb04003f 	cmp	x1, x4
  105f54:	540004a0 	b.eq	105fe8 <APACDecoder_GetParameter@@Base+0x178>  // b.none
  105f58:	b9400020 	ldr	w0, [x1]
  105f5c:	12103c02 	and	w2, w0, #0xffff0000
  105f60:	7164c05f 	cmp	w2, #0x930, lsl #12
  105f64:	54ffff01 	b.ne	105f44 <APACDecoder_GetParameter@@Base+0xd4>  // b.any
  105f68:	2a030005 	orr	w5, w0, w3
  105f6c:	17fffff8 	b	105f4c <APACDecoder_GetParameter@@Base+0xdc>
  105f70:	528d2da0 	mov	w0, #0x696d                	// #26989
  105f74:	72ae0e40 	movk	w0, #0x7072, lsl #16
  105f78:	6b00003f 	cmp	w1, w0
  105f7c:	540001a1 	b.ne	105fb0 <APACDecoder_GetParameter@@Base+0x140>  // b.any
  105f80:	52810001 	mov	w1, #0x800                 	// #2048
  105f84:	b9000281 	str	w1, [x20]
  105f88:	52800000 	mov	w0, #0x0                   	// #0
  105f8c:	f94013f5 	ldr	x21, [sp, #32]
  105f90:	f9470273 	ldr	x19, [x19, #3584]
  105f94:	f9401fe2 	ldr	x2, [sp, #56]
  105f98:	f9400261 	ldr	x1, [x19]
  105f9c:	ca010041 	eor	x1, x2, x1
  105fa0:	b5000401 	cbnz	x1, 106020 <APACDecoder_GetParameter@@Base+0x1b0>
  105fa4:	a94153f3 	ldp	x19, x20, [sp, #16]
  105fa8:	a8c47bfd 	ldp	x29, x30, [sp], #64
  105fac:	d65f03c0 	ret
  105fb0:	12834600 	mov	w0, #0xffffe5cf            	// #-6705
  105fb4:	f94013f5 	ldr	x21, [sp, #32]
  105fb8:	17fffff6 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  105fbc:	b94026a1 	ldr	w1, [x21, #36]
  105fc0:	52800000 	mov	w0, #0x0                   	// #0
  105fc4:	b9000281 	str	w1, [x20]
  105fc8:	f94013f5 	ldr	x21, [sp, #32]
  105fcc:	17fffff1 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  105fd0:	52808001 	mov	w1, #0x400                 	// #1024
  105fd4:	b9000281 	str	w1, [x20]
  105fd8:	52800000 	mov	w0, #0x0                   	// #0
  105fdc:	f94013f5 	ldr	x21, [sp, #32]
  105fe0:	17ffffec 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  105fe4:	2a0003e5 	mov	w5, w0
  105fe8:	b9000285 	str	w5, [x20]
  105fec:	52800000 	mov	w0, #0x0                   	// #0
  105ff0:	f94013f5 	ldr	x21, [sp, #32]
  105ff4:	17ffffe7 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  105ff8:	b9400843 	ldr	w3, [x2, #8]
  105ffc:	17ffffce 	b	105f34 <APACDecoder_GetParameter@@Base+0xc4>
  106000:	528c2e80 	mov	w0, #0x6174                	// #24948
  106004:	72a42ce0 	movk	w0, #0x2167, lsl #16
  106008:	f94013f5 	ldr	x21, [sp, #32]
  10600c:	17ffffe1 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  106010:	12834600 	mov	w0, #0xffffe5cf            	// #-6705
  106014:	17ffffdf 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  106018:	128347a0 	mov	w0, #0xffffe5c2            	// #-6718
  10601c:	17ffffdd 	b	105f90 <APACDecoder_GetParameter@@Base+0x120>
  106020:	f90013f5 	str	x21, [sp, #32]
  106024:	97fda6bb 	bl	6fb10 <__stack_chk_fail@plt>
  106028:	d503201f 	nop
  10602c:	d503201f 	nop

0000000000106030 <APACDecoder_GetCookieOffset@@Base>:
  106030:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106034:	f907e93f 	str	xzr, [x9, #4048]
  106038:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
  10603c:	12003c21 	and	w1, w1, #0xffff
  106040:	7100203f 	cmp	w1, #0x8
  106044:	910003fd 	mov	x29, sp
  106048:	f9000bf3 	str	x19, [sp, #16]
  10604c:	d0001933 	adrp	x19, 42c000 <aStateConstant2State@@Base+0x178>
  106050:	29047fff 	stp	wzr, wzr, [sp, #32]
  106054:	f9470262 	ldr	x2, [x19, #3584]
  106058:	f9400043 	ldr	x3, [x2]
  10605c:	f90017e3 	str	x3, [sp, #40]
  106060:	d2800003 	mov	x3, #0x0                   	// #0
  106064:	54000148 	b.hi	10608c <APACDecoder_GetCookieOffset@@Base+0x5c>  // b.pmore
  106068:	529cb9e0 	mov	w0, #0xe5cf                	// #58831
  10606c:	f9470273 	ldr	x19, [x19, #3584]
  106070:	f94017e2 	ldr	x2, [sp, #40]
  106074:	f9400261 	ldr	x1, [x19]
  106078:	ca010041 	eor	x1, x2, x1
  10607c:	b5000161 	cbnz	x1, 1060a8 <APACDecoder_GetCookieOffset@@Base+0x78>
  106080:	f9400bf3 	ldr	x19, [sp, #16]
  106084:	a8c37bfd 	ldp	x29, x30, [sp], #48
  106088:	d65f03c0 	ret
  10608c:	910093e3 	add	x3, sp, #0x24
  106090:	910083e2 	add	x2, sp, #0x20
  106094:	97fffd77 	bl	105670 <GetKBDWindowCoefficientPtr@@Base+0x70>
  106098:	72001c1f 	tst	w0, #0xff
  10609c:	54fffe60 	b.eq	106068 <APACDecoder_GetCookieOffset@@Base+0x38>  // b.none
  1060a0:	794043e0 	ldrh	w0, [sp, #32]
  1060a4:	17fffff2 	b	10606c <APACDecoder_GetCookieOffset@@Base+0x3c>
  1060a8:	97fda69a 	bl	6fb10 <__stack_chk_fail@plt>
  1060ac:	d503201f 	nop

00000000001060b0 <APACDecoder_GetVersion@@Base>:
  1060b0:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  1060b4:	f907f13f 	str	xzr, [x9, #4064]
  1060b8:	d0001922 	adrp	x2, 42c000 <aStateConstant2State@@Base+0x178>
  1060bc:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  1060c0:	92403c21 	and	x1, x1, #0xffff
  1060c4:	910003fd 	mov	x29, sp
  1060c8:	f9470042 	ldr	x2, [x2, #3584]
  1060cc:	b8616800 	ldr	w0, [x0, x1]
  1060d0:	f9400041 	ldr	x1, [x2]
  1060d4:	f9000fe1 	str	x1, [sp, #24]
  1060d8:	d2800001 	mov	x1, #0x0                   	// #0
  1060dc:	f9400fe3 	ldr	x3, [sp, #24]
  1060e0:	f9400041 	ldr	x1, [x2]
  1060e4:	ca010061 	eor	x1, x3, x1
  1060e8:	b50000a1 	cbnz	x1, 1060fc <APACDecoder_GetVersion@@Base+0x4c>
  1060ec:	5ac00800 	rev	w0, w0
  1060f0:	a8c27bfd 	ldp	x29, x30, [sp], #32
  1060f4:	53107c00 	lsr	w0, w0, #16
  1060f8:	d65f03c0 	ret
  1060fc:	97fda685 	bl	6fb10 <__stack_chk_fail@plt>

0000000000106100 <APACDecoder_GetProfile@@Base>:
  106100:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106104:	f907f13f 	str	xzr, [x9, #4064]
  106108:	d0001922 	adrp	x2, 42c000 <aStateConstant2State@@Base+0x178>
  10610c:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  106110:	92403c21 	and	x1, x1, #0xffff
  106114:	910003fd 	mov	x29, sp
  106118:	f9470042 	ldr	x2, [x2, #3584]
  10611c:	b8616800 	ldr	w0, [x0, x1]
  106120:	f9400041 	ldr	x1, [x2]
  106124:	f9000fe1 	str	x1, [sp, #24]
  106128:	d2800001 	mov	x1, #0x0                   	// #0
  10612c:	5ac00800 	rev	w0, w0
  106130:	f9400fe3 	ldr	x3, [sp, #24]
  106134:	f9400041 	ldr	x1, [x2]
  106138:	ca010061 	eor	x1, x3, x1
  10613c:	d34a3c00 	ubfx	x0, x0, #10, #6
  106140:	b5000061 	cbnz	x1, 10614c <APACDecoder_GetProfile@@Base+0x4c>
  106144:	a8c27bfd 	ldp	x29, x30, [sp], #32
  106148:	d65f03c0 	ret
  10614c:	97fda671 	bl	6fb10 <__stack_chk_fail@plt>

0000000000106150 <APACDecoder_SampleRate@@Base>:
  106150:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106154:	f907f13f 	str	xzr, [x9, #4064]
  106158:	92403c23 	and	x3, x1, #0xffff
  10615c:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  106160:	d0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  106164:	910003fd 	mov	x29, sp
  106168:	b8636800 	ldr	w0, [x0, x3]
  10616c:	d0001202 	adrp	x2, 348000 <mpddrc::UniDrcSelection::kUNDEFINED_LOUDNESS_VALUE@@Base+0x2250>
  106170:	910ea042 	add	x2, x2, #0x3a8
  106174:	f9470021 	ldr	x1, [x1, #3584]
  106178:	5ac00800 	rev	w0, w0
  10617c:	f9400023 	ldr	x3, [x1]
  106180:	f9000fe3 	str	x3, [sp, #24]
  106184:	d2800003 	mov	x3, #0x0                   	// #0
  106188:	d34f5000 	ubfx	x0, x0, #15, #6
  10618c:	f9400fe3 	ldr	x3, [sp, #24]
  106190:	f9400021 	ldr	x1, [x1]
  106194:	ca010061 	eor	x1, x3, x1
  106198:	b8607840 	ldr	w0, [x2, x0, lsl #2]
  10619c:	b5000061 	cbnz	x1, 1061a8 <APACDecoder_SampleRate@@Base+0x58>
  1061a0:	a8c27bfd 	ldp	x29, x30, [sp], #32
  1061a4:	d65f03c0 	ret
  1061a8:	97fda65a 	bl	6fb10 <__stack_chk_fail@plt>
  1061ac:	d503201f 	nop

00000000001061b0 <APACDecoder_FrameLength@@Base>:
  1061b0:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  1061b4:	f907f13f 	str	xzr, [x9, #4064]
  1061b8:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  1061bc:	92403c22 	and	x2, x1, #0xffff
  1061c0:	d0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  1061c4:	910003fd 	mov	x29, sp
  1061c8:	f9470021 	ldr	x1, [x1, #3584]
  1061cc:	b8626800 	ldr	w0, [x0, x2]
  1061d0:	f9400022 	ldr	x2, [x1]
  1061d4:	f9000fe2 	str	x2, [sp, #24]
  1061d8:	d2800002 	mov	x2, #0x0                   	// #0
  1061dc:	5ac00800 	rev	w0, w0
  1061e0:	f9400fe2 	ldr	x2, [sp, #24]
  1061e4:	f9400021 	ldr	x1, [x1]
  1061e8:	ca010041 	eor	x1, x2, x1
  1061ec:	f277141f 	tst	x0, #0x7e00
  1061f0:	b50000a1 	cbnz	x1, 106204 <APACDecoder_FrameLength@@Base+0x54>
  1061f4:	1a9f17e0 	cset	w0, eq  // eq = none
  1061f8:	a8c27bfd 	ldp	x29, x30, [sp], #32
  1061fc:	53165400 	lsl	w0, w0, #10
  106200:	d65f03c0 	ret
  106204:	97fda643 	bl	6fb10 <__stack_chk_fail@plt>
  106208:	d503201f 	nop
  10620c:	d503201f 	nop

0000000000106210 <APACDecoder_NumberOfChannels@@Base>:
  106210:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106214:	f907f13f 	str	xzr, [x9, #4064]
  106218:	d0001922 	adrp	x2, 42c000 <aStateConstant2State@@Base+0x178>
  10621c:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
  106220:	92403c21 	and	x1, x1, #0xffff
  106224:	910003fd 	mov	x29, sp
  106228:	f9470042 	ldr	x2, [x2, #3584]
  10622c:	b8616800 	ldr	w0, [x0, x1]
  106230:	f9400041 	ldr	x1, [x2]
  106234:	f9000fe1 	str	x1, [sp, #24]
  106238:	d2800001 	mov	x1, #0x0                   	// #0
  10623c:	5ac00800 	rev	w0, w0
  106240:	f9400fe3 	ldr	x3, [sp, #24]
  106244:	f9400041 	ldr	x1, [x2]
  106248:	ca010061 	eor	x1, x3, x1
  10624c:	d3412000 	ubfx	x0, x0, #1, #8
  106250:	b5000061 	cbnz	x1, 10625c <APACDecoder_NumberOfChannels@@Base+0x4c>
  106254:	a8c27bfd 	ldp	x29, x30, [sp], #32
  106258:	d65f03c0 	ret
  10625c:	97fda62d 	bl	6fb10 <__stack_chk_fail@plt>

0000000000106260 <APACDecoder_SetParameter@@Base>:
  106260:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106264:	f907e13f 	str	xzr, [x9, #4032]
  106268:	a9bc7bfd 	stp	x29, x30, [sp, #-64]!
  10626c:	f100001f 	cmp	x0, #0x0
  106270:	910003fd 	mov	x29, sp
  106274:	a90153f3 	stp	x19, x20, [sp, #16]
  106278:	d0001933 	adrp	x19, 42c000 <aStateConstant2State@@Base+0x178>
  10627c:	aa0303f4 	mov	x20, x3
  106280:	f9470265 	ldr	x5, [x19, #3584]
  106284:	fa401a84 	ccmp	x20, #0x0, #0x4, ne  // ne = any
  106288:	f94000a3 	ldr	x3, [x5]
  10628c:	f9001fe3 	str	x3, [sp, #56]
  106290:	d2800003 	mov	x3, #0x0                   	// #0
  106294:	54000ee0 	b.eq	106470 <APACDecoder_SetParameter@@Base+0x210>  // b.none
  106298:	f90013f5 	str	x21, [sp, #32]
  10629c:	2a0203f5 	mov	w21, w2
  1062a0:	528c4da2 	mov	w2, #0x626d                	// #25197
  1062a4:	72ac2d82 	movk	w2, #0x616c, lsl #16
  1062a8:	6b02003f 	cmp	w1, w2
  1062ac:	54000be0 	b.eq	106428 <APACDecoder_SetParameter@@Base+0x1c8>  // b.none
  1062b0:	5400026d 	b.le	1062fc <APACDecoder_SetParameter@@Base+0x9c>
  1062b4:	528e8d82 	mov	w2, #0x746c                	// #29804
  1062b8:	72ae0e02 	movk	w2, #0x7070, lsl #16
  1062bc:	6b02003f 	cmp	w1, w2
  1062c0:	54000300 	b.eq	106320 <APACDecoder_SetParameter@@Base+0xc0>  // b.none
  1062c4:	5400052d 	b.le	106368 <APACDecoder_SetParameter@@Base+0x108>
  1062c8:	aa0003e4 	mov	x4, x0
  1062cc:	528d2da0 	mov	w0, #0x696d                	// #26989
  1062d0:	72ae0e40 	movk	w0, #0x7072, lsl #16
  1062d4:	6b00003f 	cmp	w1, w0
  1062d8:	54000341 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  1062dc:	710012bf 	cmp	w21, #0x4
  1062e0:	54000d21 	b.ne	106484 <APACDecoder_SetParameter@@Base+0x224>  // b.any
  1062e4:	b9400281 	ldr	w1, [x20]
  1062e8:	52800000 	mov	w0, #0x0                   	// #0
  1062ec:	b9084481 	str	w1, [x4, #2116]
  1062f0:	b9084881 	str	w1, [x4, #2120]
  1062f4:	f94013f5 	ldr	x21, [sp, #32]
  1062f8:	14000014 	b	106348 <APACDecoder_SetParameter@@Base+0xe8>
  1062fc:	528e4de2 	mov	w2, #0x726f                	// #29295
  106300:	72abce02 	movk	w2, #0x5e70, lsl #16
  106304:	6b02003f 	cmp	w1, w2
  106308:	54000800 	b.eq	106408 <APACDecoder_SetParameter@@Base+0x1a8>  // b.none
  10630c:	540005ed 	b.le	1063c8 <APACDecoder_SetParameter@@Base+0x168>
  106310:	528e8d82 	mov	w2, #0x746c                	// #29804
  106314:	72abce02 	movk	w2, #0x5e70, lsl #16
  106318:	6b02003f 	cmp	w1, w2
  10631c:	54000121 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  106320:	97fffd74 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  106324:	710012bf 	cmp	w21, #0x4
  106328:	54000ac1 	b.ne	106480 <APACDecoder_SetParameter@@Base+0x220>  // b.any
  10632c:	bd400280 	ldr	s0, [x20]
  106330:	97feb47c 	bl	b3520 <DRCConfiguration::SetTargetLevel(float)@@Base>
  106334:	72001c1f 	tst	w0, #0xff
  106338:	54000360 	b.eq	1063a4 <APACDecoder_SetParameter@@Base+0x144>  // b.none
  10633c:	d503201f 	nop
  106340:	f94013f5 	ldr	x21, [sp, #32]
  106344:	52800000 	mov	w0, #0x0                   	// #0
  106348:	f9470273 	ldr	x19, [x19, #3584]
  10634c:	f9401fe2 	ldr	x2, [sp, #56]
  106350:	f9400261 	ldr	x1, [x19]
  106354:	ca010041 	eor	x1, x2, x1
  106358:	b5000901 	cbnz	x1, 106478 <APACDecoder_SetParameter@@Base+0x218>
  10635c:	a94153f3 	ldp	x19, x20, [sp, #16]
  106360:	a8c47bfd 	ldp	x29, x30, [sp], #64
  106364:	d65f03c0 	ret
  106368:	528c6e02 	mov	w2, #0x6370                	// #25456
  10636c:	72ac8e42 	movk	w2, #0x6472, lsl #16
  106370:	6b02003f 	cmp	w1, w2
  106374:	540004a0 	b.eq	106408 <APACDecoder_SetParameter@@Base+0x1a8>  // b.none
  106378:	528e4c62 	mov	w2, #0x7263                	// #29283
  10637c:	72adac82 	movk	w2, #0x6d64, lsl #16
  106380:	6b02003f 	cmp	w1, w2
  106384:	54fffde1 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  106388:	97fffd5a 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  10638c:	710012bf 	cmp	w21, #0x4
  106390:	540007a1 	b.ne	106484 <APACDecoder_SetParameter@@Base+0x224>  // b.any
  106394:	b9400281 	ldr	w1, [x20]
  106398:	97feb432 	bl	b3460 <DRCConfiguration::SetCompressionMode(unsigned int)@@Base>
  10639c:	72001c1f 	tst	w0, #0xff
  1063a0:	54fffd01 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  1063a4:	d2800080 	mov	x0, #0x4                   	// #4
  1063a8:	97fda626 	bl	6fc40 <__cxa_allocate_exception@plt>
  1063ac:	d0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  1063b0:	528c2e83 	mov	w3, #0x6174                	// #24948
  1063b4:	72a42ce3 	movk	w3, #0x2167, lsl #16
  1063b8:	b9000003 	str	w3, [x0]
  1063bc:	f944fc21 	ldr	x1, [x1, #2552]
  1063c0:	d2800002 	mov	x2, #0x0                   	// #0
  1063c4:	97fda8c7 	bl	706e0 <__cxa_throw@plt>
  1063c8:	528e6e82 	mov	w2, #0x7374                	// #29556
  1063cc:	72abcc42 	movk	w2, #0x5e62, lsl #16
  1063d0:	6b02003f 	cmp	w1, w2
  1063d4:	540003e0 	b.eq	106450 <APACDecoder_SetParameter@@Base+0x1f0>  // b.none
  1063d8:	528eae82 	mov	w2, #0x7574                	// #30068
  1063dc:	72abcc62 	movk	w2, #0x5e63, lsl #16
  1063e0:	6b02003f 	cmp	w1, w2
  1063e4:	54fffae1 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  1063e8:	97fffd42 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  1063ec:	710012bf 	cmp	w21, #0x4
  1063f0:	54000481 	b.ne	106480 <APACDecoder_SetParameter@@Base+0x220>  // b.any
  1063f4:	bd400280 	ldr	s0, [x20]
  1063f8:	97feb486 	bl	b3610 <DRCConfiguration::SetCutFactor(float)@@Base>
  1063fc:	72001c1f 	tst	w0, #0xff
  106400:	54fffa01 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  106404:	17ffffe8 	b	1063a4 <APACDecoder_SetParameter@@Base+0x144>
  106408:	97fffd3a 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  10640c:	710012bf 	cmp	w21, #0x4
  106410:	540003a1 	b.ne	106484 <APACDecoder_SetParameter@@Base+0x224>  // b.any
  106414:	b9400281 	ldr	w1, [x20]
  106418:	97feb532 	bl	b38e0 <DRCConfiguration::SetUniDRCCompressionProfile(unsigned int)@@Base>
  10641c:	72001c1f 	tst	w0, #0xff
  106420:	54fff901 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  106424:	17ffffe0 	b	1063a4 <APACDecoder_SetParameter@@Base+0x144>
  106428:	97fffd32 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  10642c:	710012bf 	cmp	w21, #0x4
  106430:	540002a1 	b.ne	106484 <APACDecoder_SetParameter@@Base+0x224>  // b.any
  106434:	b9400281 	ldr	w1, [x20]
  106438:	7100003f 	cmp	w1, #0x0
  10643c:	1a9f07e1 	cset	w1, ne  // ne = any
  106440:	97feb590 	bl	b3a80 <DRCConfiguration::SetAlbumMode(unsigned char)@@Base>
  106444:	72001c1f 	tst	w0, #0xff
  106448:	54fff7c1 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  10644c:	17ffffd6 	b	1063a4 <APACDecoder_SetParameter@@Base+0x144>
  106450:	97fffd28 	bl	1058f0 <GetKBDWindowCoefficientPtr@@Base+0x2f0>
  106454:	710012bf 	cmp	w21, #0x4
  106458:	54000141 	b.ne	106480 <APACDecoder_SetParameter@@Base+0x220>  // b.any
  10645c:	bd400280 	ldr	s0, [x20]
  106460:	97feb494 	bl	b36b0 <DRCConfiguration::SetBoostFactor(float)@@Base>
  106464:	72001c1f 	tst	w0, #0xff
  106468:	54fff6c1 	b.ne	106340 <APACDecoder_SetParameter@@Base+0xe0>  // b.any
  10646c:	17ffffce 	b	1063a4 <APACDecoder_SetParameter@@Base+0x144>
  106470:	12834600 	mov	w0, #0xffffe5cf            	// #-6705
  106474:	17ffffb5 	b	106348 <APACDecoder_SetParameter@@Base+0xe8>
  106478:	f90013f5 	str	x21, [sp, #32]
  10647c:	97fda5a5 	bl	6fb10 <__stack_chk_fail@plt>
  106480:	97fdaa1d 	bl	70cf4 <APACLRVQDecoder::LRVQLPC::Initialize()@@Base+0x44>
  106484:	97fdaa1c 	bl	70cf4 <APACLRVQDecoder::LRVQLPC::Initialize()@@Base+0x44>
  106488:	d503201f 	nop
  10648c:	d503201f 	nop

0000000000106490 <APACDecoder_Configure@@Base>:
  106490:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106494:	f907c13f 	str	xzr, [x9, #3968]
  106498:	a9b87bfd 	stp	x29, x30, [sp, #-128]!
  10649c:	910003fd 	mov	x29, sp
  1064a0:	a9025bf5 	stp	x21, x22, [sp, #32]
  1064a4:	d0001936 	adrp	x22, 42c000 <aStateConstant2State@@Base+0x178>
  1064a8:	f94702c4 	ldr	x4, [x22, #3584]
  1064ac:	a90153f3 	stp	x19, x20, [sp, #16]
  1064b0:	aa0003f3 	mov	x19, x0
  1064b4:	f9400080 	ldr	x0, [x4]
  1064b8:	f9003fe0 	str	x0, [sp, #120]
  1064bc:	d2800000 	mov	x0, #0x0                   	// #0
  1064c0:	b4001773 	cbz	x19, 1067ac <APACDecoder_Configure@@Base+0x31c>
  1064c4:	a90363f7 	stp	x23, x24, [sp, #48]
  1064c8:	aa0103f5 	mov	x21, x1
  1064cc:	12003c61 	and	w1, w3, #0xffff
  1064d0:	290b7fff 	stp	wzr, wzr, [sp, #88]
  1064d4:	7100203f 	cmp	w1, #0x8
  1064d8:	a9405e78 	ldp	x24, x23, [x19]
  1064dc:	540001a8 	b.hi	106510 <APACDecoder_Configure@@Base+0x80>  // b.pmore
  1064e0:	a94363f7 	ldp	x23, x24, [sp, #48]
  1064e4:	12834614 	mov	w20, #0xffffe5cf            	// #-6705
  1064e8:	f94702c1 	ldr	x1, [x22, #3584]
  1064ec:	f9403fe2 	ldr	x2, [sp, #120]
  1064f0:	f9400020 	ldr	x0, [x1]
  1064f4:	ca000040 	eor	x0, x2, x0
  1064f8:	b50015e0 	cbnz	x0, 1067b4 <APACDecoder_Configure@@Base+0x324>
  1064fc:	2a1403e0 	mov	w0, w20
  106500:	a94153f3 	ldp	x19, x20, [sp, #16]
  106504:	a9425bf5 	ldp	x21, x22, [sp, #32]
  106508:	a8c87bfd 	ldp	x29, x30, [sp], #128
  10650c:	d65f03c0 	ret
  106510:	aa0203f4 	mov	x20, x2
  106514:	910173e3 	add	x3, sp, #0x5c
  106518:	910163e2 	add	x2, sp, #0x58
  10651c:	aa1403e0 	mov	x0, x20
  106520:	97fffc54 	bl	105670 <GetKBDWindowCoefficientPtr@@Base+0x70>
  106524:	72001c1f 	tst	w0, #0xff
  106528:	54fffdc0 	b.eq	1064e0 <APACDecoder_Configure@@Base+0x50>  // b.none
  10652c:	294b0be1 	ldp	w1, w2, [sp, #88]
  106530:	f90023f9 	str	x25, [sp, #64]
  106534:	910183f9 	add	x25, sp, #0x60
  106538:	8b010281 	add	x1, x20, x1
  10653c:	aa1903e0 	mov	x0, x25
  106540:	97feabbc 	bl	b1430 <TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int)@@Base>
  106544:	294e03e1 	ldp	w1, w0, [sp, #112]
  106548:	51004000 	sub	w0, w0, #0x10
  10654c:	b90077e0 	str	w0, [sp, #116]
  106550:	53107c34 	lsr	w20, w1, #16
  106554:	37f80f00 	tbnz	w0, #31, 106734 <APACDecoder_Configure@@Base+0x2a4>
  106558:	53103c21 	lsl	w1, w1, #16
  10655c:	b90073e1 	str	w1, [sp, #112]
  106560:	7120029f 	cmp	w20, #0x800
  106564:	54000140 	b.eq	10658c <APACDecoder_Configure@@Base+0xfc>  // b.none
  106568:	d2800080 	mov	x0, #0x4                   	// #4
  10656c:	97fda5b5 	bl	6fc40 <__cxa_allocate_exception@plt>
  106570:	d0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  106574:	528c8c43 	mov	w3, #0x6462                	// #25698
  106578:	72ac4c23 	movk	w3, #0x6261, lsl #16
  10657c:	b9000003 	str	w3, [x0]
  106580:	f944fc21 	ldr	x1, [x1, #2552]
  106584:	d2800002 	mov	x2, #0x0                   	// #0
  106588:	97fda856 	bl	706e0 <__cxa_throw@plt>
  10658c:	aa1903e1 	mov	x1, x25
  106590:	aa1703e0 	mov	x0, x23
  106594:	97fe45ef 	bl	97d50 <apac::GlobalConfig::Deserialize(TBitstreamReader<unsigned int>&)@@Base>
  106598:	2a0003f4 	mov	w20, w0
  10659c:	35000bc0 	cbnz	w0, 106714 <APACDecoder_Configure@@Base+0x284>
  1065a0:	52810002 	mov	w2, #0x800                 	// #2048
  1065a4:	b9084262 	str	w2, [x19, #2112]
  1065a8:	d0001201 	adrp	x1, 348000 <mpddrc::UniDrcSelection::kUNDEFINED_LOUDNESS_VALUE@@Base+0x2250>
  1065ac:	91080021 	add	x1, x1, #0x200
  1065b0:	794042e3 	ldrh	w3, [x23, #32]
  1065b4:	52800020 	mov	w0, #0x1                   	// #1
  1065b8:	79109a63 	strh	w3, [x19, #2124]
  1065bc:	39408ae4 	ldrb	w4, [x23, #34]
  1065c0:	79109e64 	strh	w4, [x19, #2126]
  1065c4:	97fda8b7 	bl	708a0 <__printf_chk@plt>
  1065c8:	79509a60 	ldrh	w0, [x19, #2124]
  1065cc:	71007c1f 	cmp	w0, #0x1f
  1065d0:	54000f81 	b.ne	1067c0 <APACDecoder_Configure@@Base+0x330>  // b.any
  1065d4:	f9400302 	ldr	x2, [x24]
  1065d8:	aa1803e0 	mov	x0, x24
  1065dc:	aa1703e1 	mov	x1, x23
  1065e0:	f9400842 	ldr	x2, [x2, #16]
  1065e4:	d63f0040 	blr	x2
  1065e8:	f94082e1 	ldr	x1, [x23, #256]
  1065ec:	2a0003f4 	mov	w20, w0
  1065f0:	b94026e2 	ldr	w2, [x23, #36]
  1065f4:	b9400024 	ldr	w4, [x1]
  1065f8:	34000bc4 	cbz	w4, 106770 <APACDecoder_Configure@@Base+0x2e0>
  1065fc:	7140409f 	cmp	w4, #0x10, lsl #12
  106600:	12003c84 	and	w4, w4, #0xffff
  106604:	540008e0 	b.eq	106720 <APACDecoder_Configure@@Base+0x290>  // b.none
  106608:	d0001201 	adrp	x1, 348000 <mpddrc::UniDrcSelection::kUNDEFINED_LOUDNESS_VALUE@@Base+0x2250>
  10660c:	52808003 	mov	w3, #0x400                 	// #1024
  106610:	91092021 	add	x1, x1, #0x248
  106614:	52800020 	mov	w0, #0x1                   	// #1
  106618:	97fda8a2 	bl	708a0 <__printf_chk@plt>
  10661c:	350007d4 	cbnz	w20, 106714 <APACDecoder_Configure@@Base+0x284>
  106620:	b9401ea0 	ldr	w0, [x21, #28]
  106624:	0f016700 	movi	v0.2s, #0x38, lsl #24
  106628:	b9083a60 	str	w0, [x19, #2104]
  10662c:	52800121 	mov	w1, #0x9                   	// #9
  106630:	b9400aa0 	ldr	w0, [x21, #8]
  106634:	b9082661 	str	w1, [x19, #2084]
  106638:	b9083e60 	str	w0, [x19, #2108]
  10663c:	bd081260 	str	s0, [x19, #2064]
  106640:	f94082e1 	ldr	x1, [x23, #256]
  106644:	b9400020 	ldr	w0, [x1]
  106648:	34000980 	cbz	w0, 106778 <APACDecoder_Configure@@Base+0x2e8>
  10664c:	12003c02 	and	w2, w0, #0xffff
  106650:	7140401f 	cmp	w0, #0x10, lsl #12
  106654:	2a0203e4 	mov	w4, w2
  106658:	540000c1 	b.ne	106670 <APACDecoder_Configure@@Base+0x1e0>  // b.any
  10665c:	bd400420 	ldr	s0, [x1, #4]
  106660:	0e205800 	cnt	v0.8b, v0.8b
  106664:	0e31b800 	addv	b0, v0.8b
  106668:	0e013c02 	umov	w2, v0.b[0]
  10666c:	2a0203e4 	mov	w4, w2
  106670:	f9441a60 	ldr	x0, [x19, #2096]
  106674:	52808001 	mov	w1, #0x400                 	// #1024
  106678:	12003c42 	and	w2, w2, #0xffff
  10667c:	b9081a61 	str	w1, [x19, #2072]
  106680:	79103a62 	strh	w2, [x19, #2076]
  106684:	d3763c83 	ubfiz	x3, x4, #10, #16
  106688:	a9401406 	ldp	x6, x5, [x0]
  10668c:	cb0600a1 	sub	x1, x5, x6
  106690:	9342fc21 	asr	x1, x1, #2
  106694:	eb01007f 	cmp	x3, x1
  106698:	54000808 	b.hi	106798 <APACDecoder_Configure@@Base+0x308>  // b.pmore
  10669c:	540000e2 	b.cs	1066b8 <APACDecoder_Configure@@Base+0x228>  // b.hs, b.nlast
  1066a0:	8b0308c3 	add	x3, x6, x3, lsl #2
  1066a4:	eb0300bf 	cmp	x5, x3
  1066a8:	54000080 	b.eq	1066b8 <APACDecoder_Configure@@Base+0x228>  // b.none
  1066ac:	f9000403 	str	x3, [x0, #8]
  1066b0:	79503a64 	ldrh	w4, [x19, #2076]
  1066b4:	2a0403e2 	mov	w2, w4
  1066b8:	52800020 	mov	w0, #0x1                   	// #1
  1066bc:	39214260 	strb	w0, [x19, #2128]
  1066c0:	52800081 	mov	w1, #0x4                   	// #4
  1066c4:	52800040 	mov	w0, #0x2                   	// #2
  1066c8:	b9400aa5 	ldr	w5, [x21, #8]
  1066cc:	b9401aa3 	ldr	w3, [x21, #24]
  1066d0:	710030bf 	cmp	w5, #0xc
  1066d4:	1a810000 	csel	w0, w0, w1, eq  // eq = none
  1066d8:	6b04007f 	cmp	w3, w4
  1066dc:	54000541 	b.ne	106784 <APACDecoder_Configure@@Base+0x2f4>  // b.any
  1066e0:	1b047c02 	mul	w2, w0, w4
  1066e4:	b9400ea3 	ldr	w3, [x21, #12]
  1066e8:	6b02007f 	cmp	w3, w2
  1066ec:	54000140 	b.eq	106714 <APACDecoder_Configure@@Base+0x284>  // b.none
  1066f0:	d0001201 	adrp	x1, 348000 <mpddrc::UniDrcSelection::kUNDEFINED_LOUDNESS_VALUE@@Base+0x2250>
  1066f4:	52800020 	mov	w0, #0x1                   	// #1
  1066f8:	910ba021 	add	x1, x1, #0x2e8
  1066fc:	97fda869 	bl	708a0 <__printf_chk@plt>
  106700:	3921427f 	strb	wzr, [x19, #2128]
  106704:	12834b74 	mov	w20, #0xffffe5a4            	// #-6748
  106708:	a94363f7 	ldp	x23, x24, [sp, #48]
  10670c:	f94023f9 	ldr	x25, [sp, #64]
  106710:	17ffff76 	b	1064e8 <APACDecoder_Configure@@Base+0x58>
  106714:	a94363f7 	ldp	x23, x24, [sp, #48]
  106718:	f94023f9 	ldr	x25, [sp, #64]
  10671c:	17ffff73 	b	1064e8 <APACDecoder_Configure@@Base+0x58>
  106720:	bd400420 	ldr	s0, [x1, #4]
  106724:	0e205800 	cnt	v0.8b, v0.8b
  106728:	0e31b800 	addv	b0, v0.8b
  10672c:	0e013c04 	umov	w4, v0.b[0]
  106730:	17ffffb6 	b	106608 <APACDecoder_Configure@@Base+0x178>
  106734:	f94033e1 	ldr	x1, [sp, #96]
  106738:	aa1903e0 	mov	x0, x25
  10673c:	97feabb1 	bl	b1600 <TBitstreamReader<unsigned int>::FillCacheFrom(unsigned char const*)@@Base>
  106740:	294e07e0 	ldp	w0, w1, [sp, #112]
  106744:	f94033e3 	ldr	x3, [sp, #96]
  106748:	31008022 	adds	w2, w1, #0x20
  10674c:	4b0103e1 	neg	w1, w1
  106750:	1ac22404 	lsr	w4, w0, w2
  106754:	2a040294 	orr	w20, w20, w4
  106758:	1ac12000 	lsl	w0, w0, w1
  10675c:	91001061 	add	x1, x3, #0x4
  106760:	1a8003e0 	csel	w0, wzr, w0, eq  // eq = none
  106764:	f90033e1 	str	x1, [sp, #96]
  106768:	290e0be0 	stp	w0, w2, [sp, #112]
  10676c:	17ffff7d 	b	106560 <APACDecoder_Configure@@Base+0xd0>
  106770:	b9400824 	ldr	w4, [x1, #8]
  106774:	17ffffa5 	b	106608 <APACDecoder_Configure@@Base+0x178>
  106778:	b9400822 	ldr	w2, [x1, #8]
  10677c:	12003c44 	and	w4, w2, #0xffff
  106780:	17ffffbc 	b	106670 <APACDecoder_Configure@@Base+0x1e0>
  106784:	d0001201 	adrp	x1, 348000 <mpddrc::UniDrcSelection::kUNDEFINED_LOUDNESS_VALUE@@Base+0x2250>
  106788:	52800020 	mov	w0, #0x1                   	// #1
  10678c:	910aa021 	add	x1, x1, #0x2a8
  106790:	97fda844 	bl	708a0 <__printf_chk@plt>
  106794:	17ffffdb 	b	106700 <APACDecoder_Configure@@Base+0x270>
  106798:	cb010061 	sub	x1, x3, x1
  10679c:	97fe70f1 	bl	a2b60 <std::vector<float, std::allocator<float> >::_M_default_append(unsigned long)@@Base>
  1067a0:	79503a62 	ldrh	w2, [x19, #2076]
  1067a4:	2a0203e4 	mov	w4, w2
  1067a8:	17ffffc4 	b	1066b8 <APACDecoder_Configure@@Base+0x228>
  1067ac:	12834614 	mov	w20, #0xffffe5cf            	// #-6705
  1067b0:	17ffff4e 	b	1064e8 <APACDecoder_Configure@@Base+0x58>
  1067b4:	a90363f7 	stp	x23, x24, [sp, #48]
  1067b8:	f90023f9 	str	x25, [sp, #64]
  1067bc:	97fda4d5 	bl	6fb10 <__stack_chk_fail@plt>
  1067c0:	d2800080 	mov	x0, #0x4                   	// #4
  1067c4:	97fda51f 	bl	6fc40 <__cxa_allocate_exception@plt>
  1067c8:	d0001921 	adrp	x1, 42c000 <aStateConstant2State@@Base+0x178>
  1067cc:	528c2e83 	mov	w3, #0x6174                	// #24948
  1067d0:	72a42ce3 	movk	w3, #0x2167, lsl #16
  1067d4:	b9000003 	str	w3, [x0]
  1067d8:	f944fc21 	ldr	x1, [x1, #2552]
  1067dc:	d2800002 	mov	x2, #0x0                   	// #0
  1067e0:	97fda7c0 	bl	706e0 <__cxa_throw@plt>
  1067e4:	97fda4f3 	bl	6fbb0 <__cxa_begin_catch@plt>
  1067e8:	12834d34 	mov	w20, #0xffffe596            	// #-6762
  1067ec:	97fda791 	bl	70630 <__cxa_end_catch@plt>
  1067f0:	a94363f7 	ldp	x23, x24, [sp, #48]
  1067f4:	f94023f9 	ldr	x25, [sp, #64]
  1067f8:	17ffff3c 	b	1064e8 <APACDecoder_Configure@@Base+0x58>
  1067fc:	d503201f 	nop

0000000000106800 <APACDecoder_Create@@Base>:
  106800:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106804:	f907c93f 	str	xzr, [x9, #3984]
  106808:	a9b97bfd 	stp	x29, x30, [sp, #-112]!
  10680c:	910003fd 	mov	x29, sp
  106810:	a9025bf5 	stp	x21, x22, [sp, #32]
  106814:	d0001935 	adrp	x21, 42c000 <aStateConstant2State@@Base+0x178>
  106818:	aa0003f6 	mov	x22, x0
  10681c:	f94702a0 	ldr	x0, [x21, #3584]
  106820:	f9400001 	ldr	x1, [x0]
  106824:	f90037e1 	str	x1, [sp, #104]
  106828:	d2800001 	mov	x1, #0x0                   	// #0
  10682c:	b4000c16 	cbz	x22, 1069ac <APACDecoder_Create@@Base+0x1ac>
  106830:	d2810b01 	mov	x1, #0x858                 	// #2136
  106834:	d2800020 	mov	x0, #0x1                   	// #1
  106838:	a90153f3 	stp	x19, x20, [sp, #16]
  10683c:	97fda491 	bl	6fa80 <calloc@plt>
  106840:	aa0003f4 	mov	x20, x0
  106844:	b4001840 	cbz	x0, 106b4c <APACDecoder_Create@@Base+0x34c>
  106848:	d2803d00 	mov	x0, #0x1e8                 	// #488
  10684c:	a90363f7 	stp	x23, x24, [sp, #48]
  106850:	a9046bf9 	stp	x25, x26, [sp, #64]
  106854:	a90573fb 	stp	x27, x28, [sp, #80]
  106858:	97fda636 	bl	70130 <operator new(unsigned long)@plt>
  10685c:	aa0003f3 	mov	x19, x0
  106860:	97fe3770 	bl	94620 <APACDecoder::APACDecoder()@@Base>
  106864:	f0001937 	adrp	x23, 42d000 <aStateConstant2State@@Base+0x1178>
  106868:	f9000293 	str	x19, [x20]
  10686c:	a91d7e7f 	stp	xzr, xzr, [x19, #464]
  106870:	d2802300 	mov	x0, #0x118                 	// #280
  106874:	f946eee1 	ldr	x1, [x23, #3544]
  106878:	f900f27f 	str	xzr, [x19, #480]
  10687c:	91004021 	add	x1, x1, #0x10
  106880:	f9000261 	str	x1, [x19]
  106884:	97fda62b 	bl	70130 <operator new(unsigned long)@plt>
  106888:	f9000400 	str	x0, [x0, #8]
  10688c:	f0001922 	adrp	x2, 42d000 <aStateConstant2State@@Base+0x1178>
  106890:	f0001921 	adrp	x1, 42d000 <aStateConstant2State@@Base+0x1178>
  106894:	aa0003f3 	mov	x19, x0
  106898:	9100c018 	add	x24, x0, #0x30
  10689c:	f9412042 	ldr	x2, [x2, #576]
  1068a0:	528003e4 	mov	w4, #0x1f                  	// #31
  1068a4:	f9448821 	ldr	x1, [x1, #2320]
  1068a8:	91004042 	add	x2, x2, #0x10
  1068ac:	f8010402 	str	x2, [x0], #16
  1068b0:	528000c3 	mov	w3, #0x6                   	// #6
  1068b4:	91004021 	add	x1, x1, #0x10
  1068b8:	d2dfffe2 	mov	x2, #0xffff00000000        	// #281470681743360
  1068bc:	f9000413 	str	x19, [x0, #8]
  1068c0:	aa1803e0 	mov	x0, x24
  1068c4:	f9000a61 	str	x1, [x19, #16]
  1068c8:	79004264 	strh	w4, [x19, #32]
  1068cc:	39008a63 	strb	w3, [x19, #34]
  1068d0:	f8024262 	stur	x2, [x19, #36]
  1068d4:	97fdb0fb 	bl	72cc0 <apac::AncillaryConfig::AncillaryConfig()@@Base>
  1068d8:	9101827a 	add	x26, x19, #0x60
  1068dc:	9102c27b 	add	x27, x19, #0xb0
  1068e0:	3901627f 	strb	wzr, [x19, #88]
  1068e4:	9103227c 	add	x28, x19, #0xc8
  1068e8:	f900327f 	str	xzr, [x19, #96]
  1068ec:	91038261 	add	x1, x19, #0xe0
  1068f0:	a900ff5f 	stp	xzr, xzr, [x26, #8]
  1068f4:	d2800180 	mov	x0, #0xc                   	// #12
  1068f8:	b9007a7f 	str	wzr, [x19, #120]
  1068fc:	a9087e7f 	stp	xzr, xzr, [x19, #128]
  106900:	a9097e7f 	stp	xzr, xzr, [x19, #144]
  106904:	a90a7e7f 	stp	xzr, xzr, [x19, #160]
  106908:	f9005a7f 	str	xzr, [x19, #176]
  10690c:	a900ff7f 	stp	xzr, xzr, [x27, #8]
  106910:	f900667f 	str	xzr, [x19, #200]
  106914:	a900ff9f 	stp	xzr, xzr, [x28, #8]
  106918:	f900727f 	str	xzr, [x19, #224]
  10691c:	a900fc3f 	stp	xzr, xzr, [x1, #8]
  106920:	7901f27f 	strh	wzr, [x19, #248]
  106924:	a9107e7f 	stp	xzr, xzr, [x19, #256]
  106928:	f9008a7f 	str	xzr, [x19, #272]
  10692c:	97fda601 	bl	70130 <operator new(unsigned long)@plt>
  106930:	aa0003e1 	mov	x1, x0
  106934:	f9000693 	str	x19, [x20, #8]
  106938:	91003022 	add	x2, x1, #0xc
  10693c:	a9100a61 	stp	x1, x2, [x19, #256]
  106940:	d2800300 	mov	x0, #0x18                  	// #24
  106944:	f900003f 	str	xzr, [x1]
  106948:	b900083f 	str	wzr, [x1, #8]
  10694c:	f9008a62 	str	x2, [x19, #272]
  106950:	97fda5f8 	bl	70130 <operator new(unsigned long)@plt>
  106954:	a9007c1f 	stp	xzr, xzr, [x0]
  106958:	aa0003f3 	mov	x19, x0
  10695c:	f9400298 	ldr	x24, [x20]
  106960:	f900081f 	str	xzr, [x0, #16]
  106964:	f9041a80 	str	x0, [x20, #2096]
  106968:	b4000dd8 	cbz	x24, 106b20 <APACDecoder_Create@@Base+0x320>
  10696c:	f9400680 	ldr	x0, [x20, #8]
  106970:	b4000220 	cbz	x0, 1069b4 <APACDecoder_Create@@Base+0x1b4>
  106974:	f90002d4 	str	x20, [x22]
  106978:	52800000 	mov	w0, #0x0                   	// #0
  10697c:	a94153f3 	ldp	x19, x20, [sp, #16]
  106980:	a94363f7 	ldp	x23, x24, [sp, #48]
  106984:	a9446bf9 	ldp	x25, x26, [sp, #64]
  106988:	a94573fb 	ldp	x27, x28, [sp, #80]
  10698c:	f94702b5 	ldr	x21, [x21, #3584]
  106990:	f94037e2 	ldr	x2, [sp, #104]
  106994:	f94002a1 	ldr	x1, [x21]
  106998:	ca010041 	eor	x1, x2, x1
  10699c:	b5000ce1 	cbnz	x1, 106b38 <APACDecoder_Create@@Base+0x338>
  1069a0:	a9425bf5 	ldp	x21, x22, [sp, #32]
  1069a4:	a8c77bfd 	ldp	x29, x30, [sp], #112
  1069a8:	d65f03c0 	ret
  1069ac:	12834600 	mov	w0, #0xffffe5cf            	// #-6705
  1069b0:	17fffff7 	b	10698c <APACDecoder_Create@@Base+0x18c>
  1069b4:	f9400301 	ldr	x1, [x24]
  1069b8:	d0001920 	adrp	x0, 42c000 <aStateConstant2State@@Base+0x178>
  1069bc:	f9452800 	ldr	x0, [x0, #2640]
  1069c0:	f9400421 	ldr	x1, [x1, #8]
  1069c4:	eb00003f 	cmp	x1, x0
  1069c8:	54000b21 	b.ne	106b2c <APACDecoder_Create@@Base+0x32c>  // b.any
  1069cc:	f946eee1 	ldr	x1, [x23, #3544]
  1069d0:	f940eb00 	ldr	x0, [x24, #464]
  1069d4:	91004021 	add	x1, x1, #0x10
  1069d8:	f9000301 	str	x1, [x24]
  1069dc:	b4000040 	cbz	x0, 1069e4 <APACDecoder_Create@@Base+0x1e4>
  1069e0:	97fda5b0 	bl	700a0 <operator delete(void*)@plt>
  1069e4:	aa1803e0 	mov	x0, x24
  1069e8:	97fe35f6 	bl	941c0 <APACDecoder::~APACDecoder()@@Base>
  1069ec:	aa1803e0 	mov	x0, x24
  1069f0:	97fda5ac 	bl	700a0 <operator delete(void*)@plt>
  1069f4:	f9400696 	ldr	x22, [x20, #8]
  1069f8:	b4000776 	cbz	x22, 106ae4 <APACDecoder_Create@@Base+0x2e4>
  1069fc:	f94082c0 	ldr	x0, [x22, #256]
  106a00:	b4000040 	cbz	x0, 106a08 <APACDecoder_Create@@Base+0x208>
  106a04:	97fda5a7 	bl	700a0 <operator delete(void*)@plt>
  106a08:	f94072c0 	ldr	x0, [x22, #224]
  106a0c:	b4000040 	cbz	x0, 106a14 <APACDecoder_Create@@Base+0x214>
  106a10:	97fda5a4 	bl	700a0 <operator delete(void*)@plt>
  106a14:	a94cded3 	ldp	x19, x23, [x22, #200]
  106a18:	eb17027f 	cmp	x19, x23
  106a1c:	54000120 	b.eq	106a40 <APACDecoder_Create@@Base+0x240>  // b.none
  106a20:	f9400260 	ldr	x0, [x19]
  106a24:	b4000080 	cbz	x0, 106a34 <APACDecoder_Create@@Base+0x234>
  106a28:	f9400001 	ldr	x1, [x0]
  106a2c:	f9400421 	ldr	x1, [x1, #8]
  106a30:	d63f0020 	blr	x1
  106a34:	91002273 	add	x19, x19, #0x8
  106a38:	eb1302ff 	cmp	x23, x19
  106a3c:	54ffff21 	b.ne	106a20 <APACDecoder_Create@@Base+0x220>  // b.any
  106a40:	f94066c0 	ldr	x0, [x22, #200]
  106a44:	b4000040 	cbz	x0, 106a4c <APACDecoder_Create@@Base+0x24c>
  106a48:	97fda596 	bl	700a0 <operator delete(void*)@plt>
  106a4c:	a94b5ed3 	ldp	x19, x23, [x22, #176]
  106a50:	eb17027f 	cmp	x19, x23
  106a54:	54000120 	b.eq	106a78 <APACDecoder_Create@@Base+0x278>  // b.none
  106a58:	f9400260 	ldr	x0, [x19]
  106a5c:	b4000080 	cbz	x0, 106a6c <APACDecoder_Create@@Base+0x26c>
  106a60:	f9400001 	ldr	x1, [x0]
  106a64:	f9400421 	ldr	x1, [x1, #8]
  106a68:	d63f0020 	blr	x1
  106a6c:	91002273 	add	x19, x19, #0x8
  106a70:	eb1302ff 	cmp	x23, x19
  106a74:	54ffff21 	b.ne	106a58 <APACDecoder_Create@@Base+0x258>  // b.any
  106a78:	f9405ac0 	ldr	x0, [x22, #176]
  106a7c:	b4000040 	cbz	x0, 106a84 <APACDecoder_Create@@Base+0x284>
  106a80:	97fda588 	bl	700a0 <operator delete(void*)@plt>
  106a84:	f9404ec0 	ldr	x0, [x22, #152]
  106a88:	b4000040 	cbz	x0, 106a90 <APACDecoder_Create@@Base+0x290>
  106a8c:	97fda585 	bl	700a0 <operator delete(void*)@plt>
  106a90:	f94042c0 	ldr	x0, [x22, #128]
  106a94:	b4000040 	cbz	x0, 106a9c <APACDecoder_Create@@Base+0x29c>
  106a98:	97fda582 	bl	700a0 <operator delete(void*)@plt>
  106a9c:	a9465ed3 	ldp	x19, x23, [x22, #96]
  106aa0:	eb17027f 	cmp	x19, x23
  106aa4:	54000120 	b.eq	106ac8 <APACDecoder_Create@@Base+0x2c8>  // b.none
  106aa8:	f9400260 	ldr	x0, [x19]
  106aac:	b4000080 	cbz	x0, 106abc <APACDecoder_Create@@Base+0x2bc>
  106ab0:	f9400001 	ldr	x1, [x0]
  106ab4:	f9400421 	ldr	x1, [x1, #8]
  106ab8:	d63f0020 	blr	x1
  106abc:	91002273 	add	x19, x19, #0x8
  106ac0:	eb1302ff 	cmp	x23, x19
  106ac4:	54ffff21 	b.ne	106aa8 <APACDecoder_Create@@Base+0x2a8>  // b.any
  106ac8:	f94032c0 	ldr	x0, [x22, #96]
  106acc:	b4000040 	cbz	x0, 106ad4 <APACDecoder_Create@@Base+0x2d4>
  106ad0:	97fda574 	bl	700a0 <operator delete(void*)@plt>
  106ad4:	9100c2c0 	add	x0, x22, #0x30
  106ad8:	97fdb092 	bl	72d20 <apac::AncillaryConfig::~AncillaryConfig()@@Base>
  106adc:	aa1603e0 	mov	x0, x22
  106ae0:	97fda570 	bl	700a0 <operator delete(void*)@plt>
  106ae4:	f9441a93 	ldr	x19, [x20, #2096]
  106ae8:	b40000d3 	cbz	x19, 106b00 <APACDecoder_Create@@Base+0x300>
  106aec:	f9400260 	ldr	x0, [x19]
  106af0:	b4000040 	cbz	x0, 106af8 <APACDecoder_Create@@Base+0x2f8>
  106af4:	97fda56b 	bl	700a0 <operator delete(void*)@plt>
  106af8:	aa1303e0 	mov	x0, x19
  106afc:	97fda569 	bl	700a0 <operator delete(void*)@plt>
  106b00:	aa1403e0 	mov	x0, x20
  106b04:	97fda47f 	bl	6fd00 <free@plt>
  106b08:	12834600 	mov	w0, #0xffffe5cf            	// #-6705
  106b0c:	a94153f3 	ldp	x19, x20, [sp, #16]
  106b10:	a94363f7 	ldp	x23, x24, [sp, #48]
  106b14:	a9446bf9 	ldp	x25, x26, [sp, #64]
  106b18:	a94573fb 	ldp	x27, x28, [sp, #80]
  106b1c:	17ffff9c 	b	10698c <APACDecoder_Create@@Base+0x18c>
  106b20:	f9400696 	ldr	x22, [x20, #8]
  106b24:	b5fff6d6 	cbnz	x22, 1069fc <APACDecoder_Create@@Base+0x1fc>
  106b28:	17fffff4 	b	106af8 <APACDecoder_Create@@Base+0x2f8>
  106b2c:	aa1803e0 	mov	x0, x24
  106b30:	d63f0020 	blr	x1
  106b34:	17ffffb0 	b	1069f4 <APACDecoder_Create@@Base+0x1f4>
  106b38:	a90153f3 	stp	x19, x20, [sp, #16]
  106b3c:	a90363f7 	stp	x23, x24, [sp, #48]
  106b40:	a9046bf9 	stp	x25, x26, [sp, #64]
  106b44:	a90573fb 	stp	x27, x28, [sp, #80]
  106b48:	97fda3f2 	bl	6fb10 <__stack_chk_fail@plt>
  106b4c:	128348e0 	mov	w0, #0xffffe5b8            	// #-6728
  106b50:	a94153f3 	ldp	x19, x20, [sp, #16]
  106b54:	17ffff8e 	b	10698c <APACDecoder_Create@@Base+0x18c>
  106b58:	aa0003e1 	mov	x1, x0
  106b5c:	aa1303e0 	mov	x0, x19
  106b60:	aa0103f3 	mov	x19, x1
  106b64:	97fda54f 	bl	700a0 <operator delete(void*)@plt>
  106b68:	aa1303e0 	mov	x0, x19
  106b6c:	97fda6f5 	bl	70740 <_Unwind_Resume@plt>
  106b70:	f9407261 	ldr	x1, [x19, #224]
  106b74:	aa0003f4 	mov	x20, x0
  106b78:	b4000061 	cbz	x1, 106b84 <APACDecoder_Create@@Base+0x384>
  106b7c:	aa0103e0 	mov	x0, x1
  106b80:	97fda548 	bl	700a0 <operator delete(void*)@plt>
  106b84:	aa1c03e0 	mov	x0, x28
  106b88:	940000f2 	bl	106f50 <std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector()@@Base>
  106b8c:	aa1b03e0 	mov	x0, x27
  106b90:	940000f0 	bl	106f50 <std::vector<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> >, std::allocator<std::unique_ptr<apac::IASCConfig, std::default_delete<apac::IASCConfig> > > >::~vector()@@Base>
  106b94:	f9404e60 	ldr	x0, [x19, #152]
  106b98:	b4000040 	cbz	x0, 106ba0 <APACDecoder_Create@@Base+0x3a0>
  106b9c:	97fda541 	bl	700a0 <operator delete(void*)@plt>
  106ba0:	f9404260 	ldr	x0, [x19, #128]
  106ba4:	b4000040 	cbz	x0, 106bac <APACDecoder_Create@@Base+0x3ac>
  106ba8:	97fda53e 	bl	700a0 <operator delete(void*)@plt>
  106bac:	f9400756 	ldr	x22, [x26, #8]
  106bb0:	f9403275 	ldr	x21, [x19, #96]
  106bb4:	eb1502df 	cmp	x22, x21
  106bb8:	54000181 	b.ne	106be8 <APACDecoder_Create@@Base+0x3e8>  // b.any
  106bbc:	f9403260 	ldr	x0, [x19, #96]
  106bc0:	b4000040 	cbz	x0, 106bc8 <APACDecoder_Create@@Base+0x3c8>
  106bc4:	97fda537 	bl	700a0 <operator delete(void*)@plt>
  106bc8:	aa1803e0 	mov	x0, x24
  106bcc:	97fdb055 	bl	72d20 <apac::AncillaryConfig::~AncillaryConfig()@@Base>
  106bd0:	aa1303e0 	mov	x0, x19
  106bd4:	97fda533 	bl	700a0 <operator delete(void*)@plt>
  106bd8:	aa1403e0 	mov	x0, x20
  106bdc:	97fda6d9 	bl	70740 <_Unwind_Resume@plt>
  106be0:	aa0003f4 	mov	x20, x0
  106be4:	17fffffb 	b	106bd0 <APACDecoder_Create@@Base+0x3d0>
  106be8:	f94002a0 	ldr	x0, [x21]
  106bec:	b4000080 	cbz	x0, 106bfc <APACDecoder_Create@@Base+0x3fc>
  106bf0:	f9400001 	ldr	x1, [x0]
  106bf4:	f9400421 	ldr	x1, [x1, #8]
  106bf8:	d63f0020 	blr	x1
  106bfc:	910022b5 	add	x21, x21, #0x8
  106c00:	17ffffed 	b	106bb4 <APACDecoder_Create@@Base+0x3b4>
  106c04:	d503201f 	nop
  106c08:	d503201f 	nop
  106c0c:	d503201f 	nop

0000000000106c10 <APACDecoder_Dispose@@Base>:
  106c10:	d14013e9 	sub	x9, sp, #0x4, lsl #12
  106c14:	f907d93f 	str	xzr, [x9, #4016]
  106c18:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
  106c1c:	910003fd 	mov	x29, sp
  106c20:	a9025bf5 	stp	x21, x22, [sp, #32]
  106c24:	d0001935 	adrp	x21, 42c000 <aStateConstant2State@@Base+0x178>
  106c28:	a90153f3 	stp	x19, x20, [sp, #16]
  106c2c:	aa0003f4 	mov	x20, x0
  106c30:	f94702a0 	ldr	x0, [x21, #3584]
  106c34:	f9400001 	ldr	x1, [x0]
  106c38:	f90027e1 	str	x1, [sp, #72]
  106c3c:	d2800001 	mov	x1, #0x0                   	// #0
  106c40:	b4000c14 	cbz	x20, 106dc0 <APACDecoder_Dispose@@Base+0x1b0>
  106c44:	f9400293 	ldr	x19, [x20]
  106c48:	b4000253 	cbz	x19, 106c90 <APACDecoder_Dispose@@Base+0x80>
  106c4c:	f9400261 	ldr	x1, [x19]
  106c50:	d0001920 	adrp	x0, 42c000 <aStateConstant2State@@Base+0x178>
  106c54:	f9452800 	ldr	x0, [x0, #2640]
  106c58:	f9400421 	ldr	x1, [x1, #8]
  106c5c:	eb00003f 	cmp	x1, x0
  106c60:	54000c41 	b.ne	106de8 <APACDecoder_Dispose@@Base+0x1d8>  // b.any
  106c64:	f0001921 	adrp	x1, 42d000 <aStateConstant2State@@Base+0x1178>
  106c68:	f940ea60 	ldr	x0, [x19, #464]
  106c6c:	f946ec21 	ldr	x1, [x1, #3544]
  106c70:	91004021 	add	x1, x1, #0x10
  106c74:	f9000261 	str	x1, [x19]
  106c78:	b4000040 	cbz	x0, 106c80 <APACDecoder_Dispose@@Base+0x70>
  106c7c:	97fda509 	bl	700a0 <operator delete(void*)@plt>
  106c80:	aa1303e0 	mov	x0, x19
  106c84:	97fe354f 	bl	941c0 <APACDecoder::~APACDecoder()@@Base>
  106c88:	aa1303e0 	mov	x0, x19
  106c8c:	97fda505 	bl	700a0 <operator delete(void*)@plt>
  106c90:	f9400693 	ldr	x19, [x20, #8]
  106c94:	b4000853 	cbz	x19, 106d9c <APACDecoder_Dispose@@Base+0x18c>
  106c98:	f9001bf7 	str	x23, [sp, #48]
  106c9c:	f9408260 	ldr	x0, [x19, #256]
  106ca0:	b4000040 	cbz	x0, 106ca8 <APACDecoder_Dispose@@Base+0x98>
  106ca4:	97fda4ff 	bl	700a0 <operator delete(void*)@plt>
  106ca8:	f9407260 	ldr	x0, [x19, #224]
  106cac:	b4000040 	cbz	x0, 106cb4 <APACDecoder_Dispose@@Base+0xa4>
  106cb0:	97fda4fc 	bl	700a0 <operator delete(void*)@plt>
  106cb4:	a94cde76 	ldp	x22, x23, [x19, #200]
  106cb8:	eb1702df 	cmp	x22, x23
  106cbc:	54000140 	b.eq	106ce4 <APACDecoder_Dispose@@Base+0xd4>  // b.none
  106cc0:	f94002c0 	ldr	x0, [x22]
  106cc4:	b4000080 	cbz	x0, 106cd4 <APACDecoder_Dispose@@Base+0xc4>
  106cc8:	f9400001 	ldr	x1, [x0]
  106ccc:	f9400421 	ldr	x1, [x1, #8]
  106cd0:	d63f0020 	blr	x1
  106cd4:	910022d6 	add	x22, x22, #0x8
  106cd8:	eb1602ff 	cmp	x23, x22
  106cdc:	54ffff21 	b.ne	106cc0 <APACDecoder_Dispose@@Base+0xb0>  // b.any
  106ce0:	f9406677 	ldr	x23, [x19, #200]
  106ce4:	b4000077 	cbz	x23, 106cf0 <APACDecoder_Dispose@@Base+0xe0>
  106ce8:	aa1703e0 	mov	x0, x23
  106cec:	97fda4ed 	bl	700a0 <operator delete(void*)@plt>
  106cf0:	a94b5e76 	ldp	x22, x23, [x19, #176]
  106cf4:	eb1702df 	cmp	x22, x23
  106cf8:	54000160 	b.eq	106d24 <APACDecoder_Dispose@@Base+0x114>  // b.none
  106cfc:	d503201f 	nop
  106d00:	f94002c0 	ldr	x0, [x22]
  106d04:	b4000080 	cbz	x0, 106d14 <APACDecoder_Dispose@@Base+0x104>
  106d08:	f9400001 	ldr	x1, [x0]
  106d0c:	f9400421 	ldr	x1, [x1, #8]
  106d10:	d63f0020 	blr	x1
  106d14:	910022d6 	add	x22, x22, #0x8
  106d18:	eb1602ff 	cmp	x23, x22
  106d1c:	54ffff21 	b.ne	106d00 <APACDecoder_Dispose@@Base+0xf0>  // b.any
  106d20:	f9405a77 	ldr	x23, [x19, #176]
  106d24:	b4000077 	cbz	x23, 106d30 <APACDecoder_Dispose@@Base+0x120>
  106d28:	aa1703e0 	mov	x0, x23
  106d2c:	97fda4dd 	bl	700a0 <operator delete(void*)@plt>
  106d30:	f9404e60 	ldr	x0, [x19, #152]
  106d34:	b4000040 	cbz	x0, 106d3c <APACDecoder_Dispose@@Base+0x12c>
  106d38:	97fda4da 	bl	700a0 <operator delete(void*)@plt>
  106d3c:	f9404260 	ldr	x0, [x19, #128]
  106d40:	b4000040 	cbz	x0, 106d48 <APACDecoder_Dispose@@Base+0x138>
  106d44:	97fda4d7 	bl	700a0 <operator delete(void*)@plt>
  106d48:	a9465e76 	ldp	x22, x23, [x19, #96]
  106d4c:	eb1702df 	cmp	x22, x23
  106d50:	54000160 	b.eq	106d7c <APACDecoder_Dispose@@Base+0x16c>  // b.none
  106d54:	d503201f 	nop
  106d58:	f94002c0 	ldr	x0, [x22]
  106d5c:	b4000080 	cbz	x0, 106d6c <APACDecoder_Dispose@@Base+0x15c>
  106d60:	f9400001 	ldr	x1, [x0]
  106d64:	f9400421 	ldr	x1, [x1, #8]
  106d68:	d63f0020 	blr	x1
  106d6c:	910022d6 	add	x22, x22, #0x8
  106d70:	eb1602ff 	cmp	x23, x22
  106d74:	54ffff21 	b.ne	106d58 <APACDecoder_Dispose@@Base+0x148>  // b.any
  106d78:	f9403277 	ldr	x23, [x19, #96]
  106d7c:	b4000077 	cbz	x23, 106d88 <APACDecoder_Dispose@@Base+0x178>
  106d80:	aa1703e0 	mov	x0, x23
  106d84:	97fda4c7 	bl	700a0 <operator delete(void*)@plt>
  106d88:	9100c260 	add	x0, x19, #0x30
  106d8c:	97fdafe5 	bl	72d20 <apac::AncillaryConfig::~AncillaryConfig()@@Base>
  106d90:	aa1303e0 	mov	x0, x19
  106d94:	97fda4c3 	bl	700a0 <operator delete(void*)@plt>
  106d98:	f9401bf7 	ldr	x23, [sp, #48]
  106d9c:	f9441a93 	ldr	x19, [x20, #2096]
  106da0:	b40000d3 	cbz	x19, 106db8 <APACDecoder_Dispose@@Base+0x1a8>
  106da4:	f9400260 	ldr	x0, [x19]
  106da8:	b4000040 	cbz	x0, 106db0 <APACDecoder_Dispose@@Base+0x1a0>
  106dac:	97fda4bd 	bl	700a0 <operator delete(void*)@plt>
  106db0:	aa1303e0 	mov	x0, x19
  106db4:	97fda4bb 	bl	700a0 <operator delete(void*)@plt>
  106db8:	aa1403e0 	mov	x0, x20
  106dbc:	97fda3d1 	bl	6fd00 <free@plt>
  106dc0:	f94702b5 	ldr	x21, [x21, #3584]
  106dc4:	f94027e1 	ldr	x1, [sp, #72]
  106dc8:	f94002a0 	ldr	x0, [x21]
  106dcc:	ca000020 	eor	x0, x1, x0
  106dd0:	b5000120 	cbnz	x0, 106df4 <APACDecoder_Dispose@@Base+0x1e4>
  106dd4:	52800000 	mov	w0, #0x0                   	// #0
  106dd8:	a94153f3 	ldp	x19, x20, [sp, #16]
  106ddc:	a9425bf5 	ldp	x21, x22, [sp, #32]
  106de0:	a8c57bfd 	ldp	x29, x30, [sp], #80
  106de4:	d65f03c0 	ret
  106de8:	aa1303e0 	mov	x0, x19
  106dec:	d63f0020 	blr	x1
  106df0:	17ffffa8 	b	106c90 <APACDecoder_Dispose@@Base+0x80>
  106df4:	f9001bf7 	str	x23, [sp, #48]
  106df8:	97fda346 	bl	6fb10 <__stack_chk_fail@plt>
  106dfc:	d503201f 	nop
