0x18004e510: push      rbx
0x18004e511: push      rsi
0x18004e512: push      rdi
0x18004e513: push      r12
0x18004e515: push      r13
0x18004e517: push      r14
0x18004e519: push      r15
0x18004e51b: push      rbp
0x18004e51c: sub       rsp, 0x168
0x18004e523: mov       rax, rdx
0x18004e526: mov       r10d, dword ptr [rsp + 0x1f8]
0x18004e52e: xor       ebp, ebp
0x18004e530: cmp       dword ptr [rsp + 0x1f0], 0
0x18004e538: mov       rdx, qword ptr [rax]
0x18004e53b: mov       rax, qword ptr [rax + 8]
0x18004e53f: jbe       0x18004e5b0
0x18004e541: mov       qword ptr [rsp + 0xc0], r9
0x18004e549: lea       r15d, [r10 - 1]
0x18004e54d: mov       r12, qword ptr [rsp + 0x200]
0x18004e555: mov       rbx, rax
0x18004e558: mov       edi, dword ptr [rsp + 0x1f0]
0x18004e55f: mov       rsi, rdx
0x18004e562: mov       r14, rcx
0x18004e565: mov       r13, r8
0x18004e568: mov       edx, dword ptr [r12 + r15*4]
0x18004e56c: mov       rcx, qword ptr [r14 + rbp*8]
0x18004e570: lea       r9, [rcx + 0xf]
0x18004e574: and       r9, 0xfffffffffffffff0
0x18004e578: cmp       rcx, r9
0x18004e57b: jne       0x18004e589
0x18004e57d: test      dl, 3
0x18004e580: jne       0x18004e589
0x18004e582: call      0x1800ea910
0x18004e587: jmp       0x18004e58e
0x18004e589: call      0x1800ea910
0x18004e58e: inc       ebp
0x18004e590: cmp       ebp, edi
0x18004e592: jb        0x18004e568
0x18004e594: mov       r10d, dword ptr [rsp + 0x1f8]
0x18004e59c: mov       rax, rbx
0x18004e59f: mov       r9, qword ptr [rsp + 0xc0]
0x18004e5a7: mov       rdx, rsi
0x18004e5aa: mov       r8, r13
0x18004e5ad: mov       rcx, r14
0x18004e5b0: xor       r15d, r15d
0x18004e5b3: xor       r11d, r11d
0x18004e5b6: test      r10d, r10d
0x18004e5b9: jbe       0x18004eb4a
0x18004e5bf: mov       qword ptr [rsp + 0x108], rax
0x18004e5c7: xor       ebp, ebp
0x18004e5c9: mov       qword ptr [rsp + 0x110], rdx
0x18004e5d1: movups    xmm3, xmmword ptr [rip + 0x2e02c8]
0x18004e5d8: movups    xmm2, xmmword ptr [rip + 0x2e02d1]
0x18004e5df: movss     xmm1, dword ptr [rip + 0x2e02d9]
0x18004e5e7: movss     xmm0, dword ptr [rip + 0x2e02d5]
0x18004e5ef: movups    xmmword ptr [rsp + 0x20], xmm6
0x18004e5f4: movups    xmmword ptr [rsp + 0x30], xmm7
0x18004e5f9: movups    xmmword ptr [rsp + 0x40], xmm8
0x18004e5ff: movups    xmmword ptr [rsp + 0x50], xmm9
0x18004e605: movups    xmmword ptr [rsp + 0x60], xmm10
0x18004e60b: movups    xmmword ptr [rsp + 0x70], xmm11
0x18004e611: movups    xmmword ptr [rsp + 0x80], xmm12
0x18004e61a: movups    xmmword ptr [rsp + 0x90], xmm13
0x18004e623: movups    xmmword ptr [rsp + 0xa0], xmm14
0x18004e62c: movups    xmmword ptr [rsp + 0xb0], xmm15
0x18004e635: mov       qword ptr [rsp + 0x118], r8
0x18004e63d: mov       qword ptr [rsp + 0xc0], r9
0x18004e645: mov       rax, qword ptr [rsp + 0x1e8]
0x18004e64d: mov       rdx, qword ptr [rsp + 0x1e0]
0x18004e655: mov       rbx, qword ptr [rsp + 0x1d8]
0x18004e65d: mov       rsi, qword ptr [rsp + 0x1d0]
0x18004e665: mov       rdi, qword ptr [rsp + 0x200]
0x18004e66d: mov       r8d, dword ptr [rdi + r11*4]
0x18004e671: lea       r9, [r11 + r11*2]
0x18004e675: movss     xmm8, dword ptr [rsi + r11*8]
0x18004e67b: movss     xmm9, dword ptr [rsi + r11*8 + 4]
0x18004e682: movss     xmm15, dword ptr [rbx + r11*8]
0x18004e688: movss     xmm14, dword ptr [rbx + r11*8 + 4]
0x18004e68f: mov       r10d, dword ptr [rax + r11*8]
0x18004e693: mov       r11d, dword ptr [rax + r11*8 + 4]
0x18004e698: movss     xmm10, dword ptr [rdx + r9*4]
0x18004e69e: movss     xmm7, dword ptr [rdx + r9*4 + 4]
0x18004e6a5: movss     xmm6, dword ptr [rdx + r9*4 + 8]
0x18004e6ac: mov       r9, qword ptr [rcx + r10*8]
0x18004e6b0: mov       r14, qword ptr [rcx + r11*8]
0x18004e6b4: cmp       r8d, r15d
0x18004e6b7: jbe       0x18004eaef
0x18004e6bd: mov       r13d, r8d
0x18004e6c0: sub       r13d, r15d
0x18004e6c3: cmp       r13d, 4
0x18004e6c7: jl        0x18004eb5e
0x18004e6cd: movaps    xmm2, xmm7
0x18004e6d0: movaps    xmm5, xmm8
0x18004e6d4: movaps    xmm4, xmm9
0x18004e6d8: movaps    xmm3, xmm10
0x18004e6dc: shufps    xmm2, xmm2, 0
0x18004e6e0: movaps    xmm13, xmm15
0x18004e6e4: shufps    xmm5, xmm5, 0
0x18004e6e8: movaps    xmm12, xmm14
0x18004e6ec: shufps    xmm4, xmm4, 0
0x18004e6f0: movaps    xmm11, xmm6
0x18004e6f4: shufps    xmm3, xmm3, 0
0x18004e6f8: mov       r12d, r13d
0x18004e6fb: movups    xmmword ptr [rsp + 0x150], xmm2
0x18004e703: lea       r11d, [r15 + r15]
0x18004e707: mov       qword ptr [rsp + 0xc8], rcx
0x18004e70f: and       r12d, 0xfffffffc
0x18004e713: shufps    xmm13, xmm13, 0
0x18004e718: xor       r10d, r10d
0x18004e71b: shufps    xmm12, xmm12, 0
0x18004e720: shufps    xmm11, xmm11, 0
0x18004e725: movups    xmmword ptr [rsp + 0x140], xmm3
0x18004e72d: movups    xmmword ptr [rsp + 0x130], xmm4
0x18004e735: movups    xmmword ptr [rsp + 0x120], xmm5
0x18004e73d: movss     dword ptr [rsp + 0xd0], xmm6
0x18004e746: movss     dword ptr [rsp + 0xd8], xmm7
0x18004e74f: movss     dword ptr [rsp + 0xe0], xmm10
0x18004e759: movss     dword ptr [rsp + 0xe8], xmm14
0x18004e763: movss     dword ptr [rsp + 0xf0], xmm15
0x18004e76d: movss     dword ptr [rsp + 0xf8], xmm9
0x18004e777: movss     dword ptr [rsp + 0x100], xmm8
0x18004e781: movups    xmm2, xmmword ptr [rip + 0x2e0128]
0x18004e788: movups    xmm1, xmmword ptr [rip + 0x2e0111]
0x18004e78f: mov       rcx, qword ptr [rsp + 0x108]
0x18004e797: mov       rsi, qword ptr [rsp + 0x110]
0x18004e79f: mov       rax, qword ptr [rsp + 0x118]
0x18004e7a7: mov       rdx, qword ptr [rsp + 0xc0]
0x18004e7af: movups    xmm14, xmmword ptr [rsp + 0x120]
0x18004e7b8: lea       edi, [r11 + r10*2]
0x18004e7bc: mov       ebx, edi
0x18004e7be: add       edi, 4
0x18004e7c1: movaps    xmm5, xmm14
0x18004e7c5: movups    xmm10, xmmword ptr [rax + rbx*4]
0x18004e7ca: movups    xmm9, xmmword ptr [rax + rdi*4]
0x18004e7cf: movups    xmm15, xmmword ptr [rdx + rbx*4]
0x18004e7d4: movups    xmm8, xmmword ptr [rdx + rdi*4]
0x18004e7d9: movups    xmm0, xmmword ptr [rsp + 0x130]
0x18004e7e1: movaps    xmm3, xmm10
0x18004e7e5: shufps    xmm10, xmm9, 0xdd
0x18004e7ea: movaps    xmm4, xmm15
0x18004e7ee: shufps    xmm15, xmm8, 0xdd
0x18004e7f3: movaps    xmm7, xmm13
0x18004e7f7: shufps    xmm3, xmm9, 0x88
0x18004e7fc: movaps    xmm6, xmm0
0x18004e7ff: mulps     xmm5, xmm10
0x18004e803: movaps    xmm9, xmm12
0x18004e807: mulps     xmm7, xmm15
0x18004e80b: mulps     xmm6, xmm10
0x18004e80f: mulps     xmm9, xmm15
0x18004e813: addps     xmm5, xmm7
0x18004e816: mulps     xmm14, xmm3
0x18004e81a: mulps     xmm0, xmm3
0x18004e81d: addps     xmm6, xmm9
0x18004e821: mulps     xmm5, xmm1
0x18004e824: mulps     xmm6, xmm1
0x18004e827: movups    xmm7, xmmword ptr [rsp + 0x140]
0x18004e82f: add       r10d, 4
0x18004e833: shufps    xmm4, xmm8, 0x88
0x18004e838: movaps    xmm9, xmm7
0x18004e83c: movaps    xmm8, xmm11
0x18004e840: mulps     xmm9, xmm10
0x18004e844: mulps     xmm8, xmm15
0x18004e848: mulps     xmm10, xmm11
0x18004e84c: mulps     xmm7, xmm3
0x18004e84f: addps     xmm9, xmm8
0x18004e853: mulps     xmm3, xmm11
0x18004e857: mulps     xmm9, xmm2
0x18004e85b: movups    xmm8, xmmword ptr [rsp + 0x150]
0x18004e864: mulps     xmm15, xmm8
0x18004e868: addps     xmm10, xmm15
0x18004e86c: movaps    xmm15, xmm13
0x18004e870: mulps     xmm15, xmm4
0x18004e874: mulps     xmm10, xmm2
0x18004e878: addps     xmm14, xmm15
0x18004e87c: movaps    xmm15, xmm12
0x18004e880: mulps     xmm15, xmm4
0x18004e884: mulps     xmm14, xmm1
0x18004e888: addps     xmm0, xmm15
0x18004e88c: mulps     xmm0, xmm1
0x18004e88f: movaps    xmm15, xmm14
0x18004e893: unpcklps  xmm15, xmm5
0x18004e897: unpckhps  xmm14, xmm5
0x18004e89b: movaps    xmm5, xmm0
0x18004e89e: unpcklps  xmm5, xmm6
0x18004e8a1: unpckhps  xmm0, xmm6
0x18004e8a4: movups    xmmword ptr [r14 + rbx*4], xmm5
0x18004e8a9: movups    xmmword ptr [r14 + rdi*4], xmm0
0x18004e8ae: movups    xmmword ptr [r9 + rbx*4], xmm15
0x18004e8b3: movups    xmmword ptr [r9 + rdi*4], xmm14
0x18004e8b8: movaps    xmm0, xmm11
0x18004e8bc: mulps     xmm0, xmm4
0x18004e8bf: mulps     xmm4, xmm8
0x18004e8c3: addps     xmm7, xmm0
0x18004e8c6: addps     xmm3, xmm4
0x18004e8c9: mulps     xmm7, xmm2
0x18004e8cc: mulps     xmm3, xmm2
0x18004e8cf: movaps    xmm4, xmm7
0x18004e8d2: movaps    xmm6, xmm3
0x18004e8d5: unpcklps  xmm4, xmm9
0x18004e8d9: unpcklps  xmm6, xmm10
0x18004e8dd: unpckhps  xmm7, xmm9
0x18004e8e1: unpckhps  xmm3, xmm10
0x18004e8e5: movups    xmmword ptr [rsi + rbx*4], xmm4
0x18004e8e9: movups    xmmword ptr [rcx + rbx*4], xmm6
0x18004e8ed: movups    xmmword ptr [rsi + rdi*4], xmm7
0x18004e8f1: movups    xmmword ptr [rcx + rdi*4], xmm3
0x18004e8f5: cmp       r10d, r12d
0x18004e8f8: jb        0x18004e7af
0x18004e8fe: movss     xmm6, dword ptr [rsp + 0xd0]
0x18004e907: movss     xmm7, dword ptr [rsp + 0xd8]
0x18004e910: movss     xmm10, dword ptr [rsp + 0xe0]
0x18004e91a: movss     xmm14, dword ptr [rsp + 0xe8]
0x18004e924: movss     xmm15, dword ptr [rsp + 0xf0]
0x18004e92e: movss     xmm9, dword ptr [rsp + 0xf8]
0x18004e938: movss     xmm8, dword ptr [rsp + 0x100]
0x18004e942: mov       rcx, qword ptr [rsp + 0xc8]
0x18004e94a: movss     xmm0, dword ptr [rip + 0x2dff72]
0x18004e952: movss     xmm1, dword ptr [rip + 0x2dff66]
0x18004e95a: mov       rax, qword ptr [rsp + 0x1e8]
0x18004e962: mov       rdx, qword ptr [rsp + 0x1e0]
0x18004e96a: mov       rbx, qword ptr [rsp + 0x1d8]
0x18004e972: mov       rsi, qword ptr [rsp + 0x1d0]
0x18004e97a: mov       rdi, qword ptr [rsp + 0x200]
0x18004e982: cmp       r12d, r13d
0x18004e985: jae       0x18004eaef
0x18004e98b: mov       qword ptr [rsp + 0xc8], rcx
0x18004e993: add       r15d, r15d
0x18004e996: mov       rdi, qword ptr [rsp + 0x108]
0x18004e99e: mov       rsi, qword ptr [rsp + 0x110]
0x18004e9a6: mov       rcx, qword ptr [rsp + 0x118]
0x18004e9ae: mov       rbx, qword ptr [rsp + 0xc0]
0x18004e9b6: movaps    xmm13, xmm8
0x18004e9ba: lea       r10d, [r15 + r12*2]
0x18004e9be: movaps    xmm11, xmm15
0x18004e9c2: lea       r11d, [r10 + 1]
0x18004e9c6: movss     xmm4, dword ptr [rcx + r11*4]
0x18004e9cc: movaps    xmm2, xmm9
0x18004e9d0: movss     xmm5, dword ptr [rbx + r11*4]
0x18004e9d6: movaps    xmm12, xmm14
0x18004e9da: mulss     xmm13, xmm4
0x18004e9df: movaps    xmm3, xmm6
0x18004e9e2: mulss     xmm11, xmm5
0x18004e9e7: mulss     xmm2, xmm4
0x18004e9eb: mulss     xmm12, xmm5
0x18004e9f0: mulss     xmm3, xmm5
0x18004e9f4: mulss     xmm5, xmm7
0x18004e9f8: addss     xmm13, xmm11
0x18004e9fd: addss     xmm2, xmm12
0x18004ea02: mulss     xmm13, xmm1
0x18004ea07: mulss     xmm2, xmm1
0x18004ea0b: movaps    xmm11, xmm10
0x18004ea0f: movaps    xmm12, xmm8
0x18004ea13: mulss     xmm11, xmm4
0x18004ea18: inc       r12d
0x18004ea1b: mulss     xmm4, xmm6
0x18004ea1f: addss     xmm11, xmm3
0x18004ea24: addss     xmm4, xmm5
0x18004ea28: mulss     xmm11, xmm0
0x18004ea2d: mulss     xmm4, xmm0
0x18004ea31: movss     dword ptr [r9 + r11*4], xmm13
0x18004ea37: movaps    xmm5, xmm14
0x18004ea3b: movss     dword ptr [r14 + r11*4], xmm2
0x18004ea41: movss     dword ptr [rsi + r11*4], xmm11
0x18004ea47: movaps    xmm11, xmm9
0x18004ea4b: movss     dword ptr [rdi + r11*4], xmm4
0x18004ea51: movaps    xmm4, xmm15
0x18004ea55: movss     xmm2, dword ptr [rcx + r10*4]
0x18004ea5b: movss     xmm3, dword ptr [rbx + r10*4]
0x18004ea61: mulss     xmm12, xmm2
0x18004ea66: mulss     xmm11, xmm2
0x18004ea6b: mulss     xmm4, xmm3
0x18004ea6f: mulss     xmm5, xmm3
0x18004ea73: addss     xmm12, xmm4
0x18004ea78: addss     xmm11, xmm5
0x18004ea7d: mulss     xmm12, xmm1
0x18004ea82: mulss     xmm11, xmm1
0x18004ea87: movaps    xmm5, xmm10
0x18004ea8b: movaps    xmm4, xmm6
0x18004ea8e: mulss     xmm5, xmm2
0x18004ea92: mulss     xmm2, xmm6
0x18004ea96: mulss     xmm4, xmm3
0x18004ea9a: mulss     xmm3, xmm7
0x18004ea9e: addss     xmm5, xmm4
0x18004eaa2: addss     xmm2, xmm3
0x18004eaa6: mulss     xmm5, xmm0
0x18004eaaa: mulss     xmm2, xmm0
0x18004eaae: movss     dword ptr [r9 + r10*4], xmm12
0x18004eab4: movss     dword ptr [r14 + r10*4], xmm11
0x18004eaba: movss     dword ptr [rsi + r10*4], xmm5
0x18004eac0: movss     dword ptr [rdi + r10*4], xmm2
0x18004eac6: cmp       r12d, r13d
0x18004eac9: jb        0x18004e9b6
0x18004eacf: mov       rbx, qword ptr [rsp + 0x1d8]
0x18004ead7: mov       rsi, qword ptr [rsp + 0x1d0]
0x18004eadf: mov       rdi, qword ptr [rsp + 0x200]
0x18004eae7: mov       rcx, qword ptr [rsp + 0xc8]
0x18004eaef: inc       ebp
0x18004eaf1: mov       r15d, r8d
0x18004eaf4: mov       r11d, ebp
0x18004eaf7: cmp       ebp, dword ptr [rsp + 0x1f8]
0x18004eafe: jb        0x18004e66d
0x18004eb04: movups    xmm6, xmmword ptr [rsp + 0x20]
0x18004eb09: movups    xmm7, xmmword ptr [rsp + 0x30]
0x18004eb0e: movups    xmm8, xmmword ptr [rsp + 0x40]
0x18004eb14: movups    xmm9, xmmword ptr [rsp + 0x50]
0x18004eb1a: movups    xmm10, xmmword ptr [rsp + 0x60]
0x18004eb20: movups    xmm11, xmmword ptr [rsp + 0x70]
0x18004eb26: movups    xmm12, xmmword ptr [rsp + 0x80]
0x18004eb2f: movups    xmm13, xmmword ptr [rsp + 0x90]
0x18004eb38: movups    xmm14, xmmword ptr [rsp + 0xa0]
0x18004eb41: movups    xmm15, xmmword ptr [rsp + 0xb0]
0x18004eb4a: add       rsp, 0x168
0x18004eb51: pop       rbp
0x18004eb52: pop       r15
0x18004eb54: pop       r14
0x18004eb56: pop       r13
0x18004eb58: pop       r12
0x18004eb5a: pop       rdi
0x18004eb5b: pop       rsi
0x18004eb5c: pop       rbx
0x18004eb5d: ret       
0x18004eb5e: xor       r12d, r12d
0x18004eb61: jmp       0x18004e982
0x18004eb66: nop       dword ptr [rax]
0x18004eb69: nop       dword ptr [rax]
