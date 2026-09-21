0x18004d0b0: push      r13
0x18004d0b2: push      r14
0x18004d0b4: push      r15
0x18004d0b6: push      rbp
0x18004d0b7: sub       rsp, 0x1e8
0x18004d0be: mov       r14, r9
0x18004d0c1: mov       r10, rsp
0x18004d0c4: mov       r15, rdx
0x18004d0c7: mov       r13, r8
0x18004d0ca: mov       rbp, rcx
0x18004d0cd: mov       rcx, r15
0x18004d0d0: mov       r8, r14
0x18004d0d3: mov       r9d, 0x14
0x18004d0d9: lea       rax, [rsp + 0xf8]
0x18004d0e1: mov       edx, dword ptr [rsp + 0x240]
0x18004d0e8: mov       qword ptr [r10 + 0x20], rax
0x18004d0ec: call      0x180022640
0x18004d0f1: mov       r10, rsp
0x18004d0f4: mov       rcx, r13
0x18004d0f7: mov       r8, r14
0x18004d0fa: mov       r9d, 0x14
0x18004d100: mov       edx, dword ptr [rsp + 0x240]
0x18004d107: lea       rax, [rsp + 0x148]
0x18004d10f: mov       qword ptr [r10 + 0x20], rax
0x18004d113: call      0x180022640
0x18004d118: nop       
0x18004d119: xor       r9d, r9d
0x18004d11c: xor       r11d, r11d
0x18004d11f: mov       qword ptr [rsp + 0xd0], rbp
0x18004d127: xor       r10d, r10d
0x18004d12a: mov       qword ptr [rsp + 0xd8], rbx
0x18004d132: pxor      xmm2, xmm2
0x18004d136: mov       qword ptr [rsp + 0xe0], rsi
0x18004d13e: mov       qword ptr [rsp + 0xe8], rdi
0x18004d146: mov       qword ptr [rsp + 0xf0], r12
0x18004d14e: mov       ebp, dword ptr [rsp + 0x240]
0x18004d155: mov       ebx, dword ptr [r14 + r11*4]
0x18004d159: cmp       ebx, r9d
0x18004d15c: ja        0x18004d16d
0x18004d15e: movss     dword ptr [rsp + r11*4 + 0x198], xmm2
0x18004d168: jmp       0x18004d2a6
0x18004d16d: xor       edx, edx
0x18004d16f: pxor      xmm0, xmm0
0x18004d173: movaps    xmm1, xmm0
0x18004d176: test      ebp, ebp
0x18004d178: jbe       0x18004d294
0x18004d17e: mov       qword ptr [rsp + 0xc0], r11
0x18004d186: lea       eax, [r9 + r9]
0x18004d18a: neg       r9d
0x18004d18d: mov       qword ptr [rsp + 0xc8], r14
0x18004d195: add       r9d, ebx
0x18004d198: xor       r12d, r12d
0x18004d19b: lea       esi, [rdx + 1]
0x18004d19e: mov       r14, qword ptr [r13 + rdx*8]
0x18004d1a3: mov       r11, qword ptr [r15 + rdx*8]
0x18004d1a7: cmp       esi, ebp
0x18004d1a9: jb        0x18004d1fa
0x18004d1ab: nop       dword ptr [rax + rax]
0x18004d1b0: pxor      xmm4, xmm4
0x18004d1b4: lea       edi, [rax + r12*2]
0x18004d1b8: pxor      xmm5, xmm5
0x18004d1bc: lea       edx, [rdi + 1]
0x18004d1bf: cvtss2sd  xmm4, dword ptr [r11 + rdx*4]
0x18004d1c5: cvtss2sd  xmm3, dword ptr [r14 + rdx*4]
0x18004d1cb: cvtss2sd  xmm5, dword ptr [r14 + rdi*4]
0x18004d1d1: mulsd     xmm4, xmm3
0x18004d1d5: pxor      xmm3, xmm3
0x18004d1d9: inc       r12d
0x18004d1dc: cvtss2sd  xmm3, dword ptr [r11 + rdi*4]
0x18004d1e2: addsd     xmm1, xmm4
0x18004d1e6: mulsd     xmm3, xmm5
0x18004d1ea: addsd     xmm0, xmm3
0x18004d1ee: cmp       r12d, r9d
0x18004d1f1: jb        0x18004d1b0
0x18004d1f3: mov       edx, esi
0x18004d1f5: jmp       0x18004d27c
0x18004d1fa: mov       r8, qword ptr [r15 + rsi*8]
0x18004d1fe: mov       rsi, qword ptr [r13 + rsi*8]
0x18004d203: nop       dword ptr [rax + rax]
0x18004d208: nop       dword ptr [rax + rax]
0x18004d210: pxor      xmm4, xmm4
0x18004d214: lea       edi, [rax + r12*2]
0x18004d218: inc       r12d
0x18004d21b: lea       ecx, [rdi + 1]
0x18004d21e: cvtss2sd  xmm4, dword ptr [r11 + rcx*4]
0x18004d224: cvtss2sd  xmm3, dword ptr [r14 + rcx*4]
0x18004d22a: cvtss2sd  xmm5, dword ptr [rsi + rcx*4]
0x18004d22f: mulsd     xmm4, xmm3
0x18004d233: cvtss2sd  xmm3, dword ptr [r8 + rcx*4]
0x18004d239: addsd     xmm1, xmm4
0x18004d23d: cvtss2sd  xmm4, dword ptr [r14 + rdi*4]
0x18004d243: mulsd     xmm3, xmm5
0x18004d247: pxor      xmm5, xmm5
0x18004d24b: addsd     xmm1, xmm3
0x18004d24f: cvtss2sd  xmm3, dword ptr [r11 + rdi*4]
0x18004d255: cvtss2sd  xmm5, dword ptr [rsi + rdi*4]
0x18004d25a: mulsd     xmm3, xmm4
0x18004d25e: addsd     xmm0, xmm3
0x18004d262: pxor      xmm3, xmm3
0x18004d266: cvtss2sd  xmm3, dword ptr [r8 + rdi*4]
0x18004d26c: mulsd     xmm3, xmm5
0x18004d270: addsd     xmm0, xmm3
0x18004d274: cmp       r12d, r9d
0x18004d277: jb        0x18004d210
0x18004d279: add       edx, 2
0x18004d27c: cmp       edx, ebp
0x18004d27e: jb        0x18004d198
0x18004d284: mov       r11, qword ptr [rsp + 0xc0]
0x18004d28c: mov       r14, qword ptr [rsp + 0xc8]
0x18004d294: addsd     xmm0, xmm1
0x18004d298: cvtsd2ss  xmm0, xmm0
0x18004d29c: movss     dword ptr [rsp + r11*4 + 0x198], xmm0
0x18004d2a6: inc       r10d
0x18004d2a9: mov       r9d, ebx
0x18004d2ac: mov       r11d, r10d
0x18004d2af: cmp       r10d, 0x14
0x18004d2b3: jb        0x18004d155
0x18004d2b9: movss     xmm4, dword ptr [rip + 0x2e157f]
0x18004d2c1: xor       r14d, r14d
0x18004d2c4: movss     xmm5, dword ptr [rsp + 0x238]
0x18004d2cd: movaps    xmm3, xmm4
0x18004d2d0: movss     xmm1, dword ptr [rip + 0x2e1558]
0x18004d2d8: xor       r13d, r13d
0x18004d2db: movss     xmm0, dword ptr [rip + 0x2e1551]
0x18004d2e3: subss     xmm3, xmm5
0x18004d2e7: movups    xmmword ptr [rsp + 0x40], xmm13
0x18004d2ed: movups    xmmword ptr [rsp + 0x30], xmm14
0x18004d2f3: mov       rbp, qword ptr [rsp + 0xd0]
0x18004d2fb: mov       rbx, qword ptr [rsp + 0xd8]
0x18004d303: mov       rsi, qword ptr [rsp + 0xe0]
0x18004d30b: mov       rdi, qword ptr [rsp + 0xe8]
0x18004d313: mov       r12, qword ptr [rsp + 0xf0]
0x18004d31b: mov       r15, qword ptr [rsp + 0x230]
0x18004d323: movups    xmmword ptr [rsp + 0xb0], xmm6
0x18004d32b: movaps    xmm6, xmm3
0x18004d32e: movups    xmmword ptr [rsp + 0xa0], xmm7
0x18004d336: movaps    xmm7, xmm4
0x18004d339: movups    xmmword ptr [rsp + 0x90], xmm8
0x18004d342: movaps    xmm8, xmm5
0x18004d346: movups    xmmword ptr [rsp + 0x80], xmm9
0x18004d34f: movaps    xmm9, xmm2
0x18004d353: movups    xmmword ptr [rsp + 0x70], xmm10
0x18004d359: movaps    xmm10, xmm1
0x18004d35d: movups    xmmword ptr [rsp + 0x60], xmm11
0x18004d363: movaps    xmm11, xmm0
0x18004d367: movups    xmmword ptr [rsp + 0x50], xmm12
0x18004d36d: movss     xmm13, dword ptr [rip + 0x2e14c6]
0x18004d376: movss     xmm14, dword ptr [rip + 0x2e14b9]
0x18004d37f: movss     xmm0, dword ptr [rsp + r14*4 + 0xf8]
0x18004d389: movaps    xmm3, xmm7
0x18004d38c: movss     xmm1, dword ptr [rsp + r14*4 + 0x148]
0x18004d396: movaps    xmm12, xmm0
0x18004d39a: movss     xmm2, dword ptr [r15 + r14*4]
0x18004d3a0: addss     xmm12, xmm1
0x18004d3a5: subss     xmm3, xmm2
0x18004d3a9: subss     xmm0, xmm1
0x18004d3ad: mulss     xmm3, dword ptr [rbp + r14*4]
0x18004d3b4: movaps    xmm1, xmm2
0x18004d3b7: mulss     xmm1, xmm12
0x18004d3bc: addss     xmm1, xmm3
0x18004d3c0: movaps    xmm4, xmm1
0x18004d3c3: xorps     xmm4, xmmword ptr [rip + 0x2e1456]
0x18004d3ca: maxss     xmm4, xmm1
0x18004d3ce: comiss    xmm10, xmm4
0x18004d3d2: jbe       0x18004d3dd
0x18004d3d4: movss     dword ptr [rbp + r14*4], xmm9
0x18004d3db: jmp       0x18004d3e4
0x18004d3dd: movss     dword ptr [rbp + r14*4], xmm1
0x18004d3e4: movss     xmm1, dword ptr [r15 + r14*4]
0x18004d3ea: movaps    xmm2, xmm7
0x18004d3ed: mulss     xmm0, xmm1
0x18004d3f1: subss     xmm2, xmm1
0x18004d3f5: mulss     xmm2, dword ptr [rbp + r14*4 + 0x50]
0x18004d3fc: addss     xmm0, xmm2
0x18004d400: movaps    xmm3, xmm0
0x18004d403: xorps     xmm3, xmmword ptr [rip + 0x2e1416]
0x18004d40a: maxss     xmm3, xmm0
0x18004d40e: comiss    xmm10, xmm3
0x18004d412: jbe       0x18004d41d
0x18004d414: movss     dword ptr [rbp + r14*4 + 0x50], xmm9
0x18004d41b: jmp       0x18004d424
0x18004d41d: movss     dword ptr [rbp + r14*4 + 0x50], xmm0
0x18004d424: movss     xmm0, dword ptr [r15 + r14*4]
0x18004d42a: movaps    xmm1, xmm7
0x18004d42d: movss     xmm3, dword ptr [rsp + r14*4 + 0x198]
0x18004d437: subss     xmm1, xmm0
0x18004d43b: mulss     xmm3, xmm0
0x18004d43f: mulss     xmm1, dword ptr [rbp + r14*4 + 0xa0]
0x18004d449: addss     xmm3, xmm1
0x18004d44d: movaps    xmm2, xmm3
0x18004d450: xorps     xmm2, xmmword ptr [rip + 0x2e13c9]
0x18004d457: maxss     xmm2, xmm3
0x18004d45b: comiss    xmm10, xmm2
0x18004d45f: jbe       0x18004d46d
0x18004d461: movss     dword ptr [rbp + r14*4 + 0xa0], xmm9
0x18004d46b: jmp       0x18004d477
0x18004d46d: movss     dword ptr [rbp + r14*4 + 0xa0], xmm3
0x18004d477: movss     xmm1, dword ptr [rbp + r14*4 + 0xf0]
0x18004d481: movaps    xmm0, xmm11
0x18004d485: addss     xmm0, xmm12
0x18004d48a: addss     xmm1, xmm11
0x18004d48f: divss     xmm0, xmm1
0x18004d493: mulss     xmm0, xmm14
0x18004d498: call      0x18003c040
0x18004d49d: mulss     xmm0, xmm13
0x18004d4a2: movaps    xmm1, xmm9
0x18004d4a6: maxss     xmm1, xmm0
0x18004d4aa: movss     xmm0, dword ptr [rbp + r14*4 + 0x140]
0x18004d4b4: minss     xmm1, xmm7
0x18004d4b8: comiss    xmm0, xmm1
0x18004d4bb: jb        0x18004d4c5
0x18004d4bd: movaps    xmm1, xmm8
0x18004d4c1: mulss     xmm1, xmm0
0x18004d4c5: movss     xmm0, dword ptr [rbp + r14*4 + 0xf0]
0x18004d4cf: mulss     xmm0, xmm8
0x18004d4d4: mulss     xmm12, xmm6
0x18004d4d9: movss     dword ptr [rbp + r14*4 + 0x140], xmm1
0x18004d4e3: addss     xmm0, xmm12
0x18004d4e8: movaps    xmm12, xmm0
0x18004d4ec: xorps     xmm12, xmmword ptr [rip + 0x2e132c]
0x18004d4f4: maxss     xmm12, xmm0
0x18004d4f9: comiss    xmm10, xmm12
0x18004d4fd: jbe       0x18004d50b
0x18004d4ff: movss     dword ptr [rbp + r14*4 + 0xf0], xmm9
0x18004d509: jmp       0x18004d515
0x18004d50b: movss     dword ptr [rbp + r14*4 + 0xf0], xmm0
0x18004d515: inc       r13d
0x18004d518: mov       r14d, r13d
0x18004d51b: cmp       r13d, 0x14
0x18004d51f: jb        0x18004d37f
0x18004d525: movups    xmm6, xmmword ptr [rsp + 0xb0]
0x18004d52d: movups    xmm7, xmmword ptr [rsp + 0xa0]
0x18004d535: movups    xmm8, xmmword ptr [rsp + 0x90]
0x18004d53e: movups    xmm9, xmmword ptr [rsp + 0x80]
0x18004d547: movups    xmm10, xmmword ptr [rsp + 0x70]
0x18004d54d: movups    xmm11, xmmword ptr [rsp + 0x60]
0x18004d553: movups    xmm12, xmmword ptr [rsp + 0x50]
0x18004d559: movups    xmm13, xmmword ptr [rsp + 0x40]
0x18004d55f: movups    xmm14, xmmword ptr [rsp + 0x30]
0x18004d565: add       rsp, 0x1e8
0x18004d56c: pop       rbp
0x18004d56d: pop       r15
0x18004d56f: pop       r14
0x18004d571: pop       r13
0x18004d573: ret       
