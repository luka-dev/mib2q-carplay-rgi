0x180031860: push      rbx
0x180031861: push      rsi
0x180031862: push      rdi
0x180031863: push      r12
0x180031865: push      r13
0x180031867: push      r14
0x180031869: push      r15
0x18003186b: push      rbp
0x18003186c: sub       rsp, 0x78
0x180031870: mov       rbx, rcx
0x180031873: mov       rcx, qword ptr [rsp + 0xf8]
0x18003187b: mov       rdi, r9
0x18003187e: mov       rax, qword ptr [rsp + 0x100]
0x180031886: mov       rsi, r8
0x180031889: mov       qword ptr [rsp + 0x60], rax
0x18003188e: mov       rbp, rdx
0x180031891: mov       r10, qword ptr [rsp + 0x108]
0x180031899: lea       r11, [rcx + 0x2f]
0x18003189d: movdqu    xmm1, xmmword ptr [rip + 0x2c39eb]
0x1800318a5: lea       rax, [rcx + 0x2ce]
0x1800318ac: mov       qword ptr [rsp + 0x68], r10
0x1800318b1: lea       r14, [rcx + 7]
0x1800318b5: and       r14, 0xfffffffffffffff8
0x1800318b9: lea       r10, [rcx + 0xaca]
0x1800318c0: movq      xmm3, r11
0x1800318c5: and       r10, 0xffffffffffffffe0
0x1800318c9: movq      xmm0, rax
0x1800318ce: xor       r15d, r15d
0x1800318d1: punpcklqdq xmm3, xmm0
0x1800318d5: movdqu    xmm0, xmmword ptr [rip + 0x2c39a3]
0x1800318dd: movdqa    xmm2, xmm0
0x1800318e1: paddq     xmm2, xmm3
0x1800318e5: paddq     xmm3, xmmword ptr [rip + 0x2c39b3]
0x1800318ed: paddq     xmm3, xmm0
0x1800318f1: pand      xmm2, xmm1
0x1800318f5: pand      xmm3, xmm1
0x1800318f9: mov       r13, qword ptr [rsp + 0x110]
0x180031901: mov       r8, qword ptr [rsp + 0xe0]
0x180031909: mov       r9, qword ptr [rsp + 0xe8]
0x180031911: mov       r12, qword ptr [rsp + 0xf0]
0x180031919: movdqu    xmmword ptr [r14], xmm2
0x18003191e: movdqu    xmmword ptr [r14 + 0x10], xmm3
0x180031924: mov       qword ptr [r14 + 0x20], r10
0x180031928: test      r13, r13
0x18003192b: je        0x1800319c7
0x180031931: mov       r11d, dword ptr [rsp + 0x118]
0x180031939: lea       rdx, [rcx + 0xd79]
0x180031940: add       r11d, 3
0x180031944: movdqa    xmm3, xmm0
0x180031948: shr       r11d, 2
0x18003194c: lea       r15, [rcx + 0xd51]
0x180031953: shl       r11d, 3
0x180031957: and       r15, 0xfffffffffffffff8
0x18003195b: movq      xmm5, rdx
0x180031960: lea       rax, [r11*4 + 0x1f]
0x180031968: lea       rdx, [rax + rcx + 0xd79]
0x180031970: add       rax, rax
0x180031973: lea       r10, [r11*4]
0x18003197b: neg       r10
0x18003197e: movq      xmm2, rdx
0x180031983: movq      xmm4, rax
0x180031988: punpcklqdq xmm5, xmm2
0x18003198c: lea       rax, [r11 + r11*4]
0x180031990: punpcklqdq xmm4, xmm4
0x180031994: lea       r10, [r10 + rax*4]
0x180031998: paddq     xmm3, xmm5
0x18003199c: paddq     xmm5, xmm4
0x1800319a0: paddq     xmm5, xmm0
0x1800319a4: pand      xmm3, xmm1
0x1800319a8: lea       rax, [r10 + rcx + 0xe14]
0x1800319b0: pand      xmm5, xmm1
0x1800319b4: and       rax, 0xffffffffffffffe0
0x1800319b8: movdqu    xmmword ptr [r15], xmm3
0x1800319bd: movdqu    xmmword ptr [r15 + 0x10], xmm5
0x1800319c3: mov       qword ptr [r15 + 0x20], rax
0x1800319c7: mov       rax, rsp
0x1800319ca: lea       r10, [rdi + 0x10]
0x1800319ce: mov       rcx, r12
0x1800319d1: mov       rdx, r14
0x1800319d4: lea       r11, [rdi + 0xb0]
0x1800319db: mov       qword ptr [rax + 0x20], r10
0x1800319df: lea       r10, [rdi + 0x150]
0x1800319e6: mov       qword ptr [rax + 0x30], r10
0x1800319ea: mov       r10d, dword ptr [rsi]
0x1800319ed: mov       qword ptr [rax + 0x28], r11
0x1800319f1: lea       r11, [rdi + 0x240]
0x1800319f8: mov       qword ptr [rax + 0x38], r11
0x1800319fc: mov       dword ptr [rax + 0x40], r10d
0x180031a00: mov       dword ptr [rax + 0x48], 0x14
0x180031a07: mov       qword ptr [rax + 0x50], rbp
0x180031a0b: call      0x18004e510
0x180031a10: test      r13, r13
0x180031a13: je        0x180031a74
0x180031a15: mov       r9, rsp
0x180031a18: lea       r10, [rdi + 0x148]
0x180031a1f: mov       rcx, r13
0x180031a22: lea       rax, [rdi + 0xa8]
0x180031a29: mov       rdx, r15
0x180031a2c: lea       r11, [rdi + 0x234]
0x180031a33: mov       qword ptr [r9 + 0x28], r10
0x180031a37: mov       r10d, dword ptr [rsi]
0x180031a3a: mov       qword ptr [r9 + 0x20], rax
0x180031a3e: lea       rax, [rdi + 0x2d8]
0x180031a45: mov       qword ptr [r9 + 0x30], r11
0x180031a49: lea       r11, [rsp + 0x118]
0x180031a51: mov       qword ptr [r9 + 0x38], rax
0x180031a55: mov       dword ptr [r9 + 0x40], r10d
0x180031a59: mov       dword ptr [r9 + 0x48], 1
0x180031a61: mov       qword ptr [r9 + 0x50], r11
0x180031a65: mov       r8, qword ptr [rsp + 0x60]
0x180031a6a: mov       r9, qword ptr [rsp + 0x68]
0x180031a6f: call      0x18004e510
0x180031a74: lea       rcx, [rbx + 0x6838]
0x180031a7b: mov       qword ptr [rsp + 0x60], rcx
0x180031a80: call      0x180016a80
0x180031a85: test      eax, eax
0x180031a87: je        0x180031a99
0x180031a89: mov       rdx, r12
0x180031a8c: mov       r8, rbp
0x180031a8f: mov       rcx, qword ptr [rsp + 0x60]
0x180031a94: call      0x18004df60
0x180031a99: mov       rcx, r12
0x180031a9c: mov       r9d, 0x4d
0x180031aa2: mov       edx, dword ptr [rsi]
0x180031aa4: mov       r8, qword ptr [rsi + 0x28]
0x180031aa8: call      0x18004eb70
0x180031aad: test      r13, r13
0x180031ab0: je        0x180031ac8
0x180031ab2: mov       rcx, r13
0x180031ab5: mov       r9d, dword ptr [rsp + 0x118]
0x180031abd: mov       edx, dword ptr [rsi]
0x180031abf: mov       r8, qword ptr [rsi + 0x28]
0x180031ac3: call      0x18004eb70
0x180031ac8: mov       r9, rsp
0x180031acb: add       rbx, 0x190
0x180031ad2: xor       r11d, r11d
0x180031ad5: cmp       dword ptr [rdi], 0
0x180031ad8: mov       eax, dword ptr [rdi + 4]
0x180031adb: mov       rcx, rbx
0x180031ade: mov       r10d, dword ptr [rsi]
0x180031ae1: mov       rdx, r12
0x180031ae4: mov       r8, r14
0x180031ae7: cmovne    r11, rbp
0x180031aeb: mov       dword ptr [r9 + 0x20], eax
0x180031aef: mov       dword ptr [r9 + 0x28], r10d
0x180031af3: mov       qword ptr [r9 + 0x30], r11
0x180031af7: mov       dword ptr [r9 + 0x38], 0x4d
0x180031aff: mov       r9, qword ptr [rdi + 8]
0x180031b03: call      0x18004d6d0
0x180031b08: test      r13, r13
0x180031b0b: je        0x180031b55
0x180031b0d: mov       r10d, dword ptr [rsi]
0x180031b10: mov       r11, rsp
0x180031b13: imul      eax, r10d, 0x13
0x180031b17: mov       rcx, rbx
0x180031b1a: mov       esi, dword ptr [rdi + 4]
0x180031b1d: mov       rdx, r13
0x180031b20: imul      eax, esi
0x180031b23: mov       r8, r15
0x180031b26: mov       r9, qword ptr [rdi + 8]
0x180031b2a: cmp       dword ptr [rdi], 0
0x180031b2d: mov       edi, dword ptr [rsp + 0x118]
0x180031b34: mov       dword ptr [r11 + 0x20], esi
0x180031b38: lea       rbp, [r9 + rax*4]
0x180031b3c: cmovne    r9, rbp
0x180031b40: mov       dword ptr [r11 + 0x28], r10d
0x180031b44: mov       qword ptr [r11 + 0x30], 0
0x180031b4c: mov       dword ptr [r11 + 0x38], edi
0x180031b50: call      0x18004d6d0
0x180031b55: add       rsp, 0x78
0x180031b59: pop       rbp
0x180031b5a: pop       r15
0x180031b5c: pop       r14
0x180031b5e: pop       r13
0x180031b60: pop       r12
0x180031b62: pop       rdi
0x180031b63: pop       rsi
0x180031b64: pop       rbx
0x180031b65: ret       
