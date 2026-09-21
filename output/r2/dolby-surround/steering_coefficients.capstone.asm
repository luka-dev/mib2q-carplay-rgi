0x18004e2c0: push      rbx
0x18004e2c1: push      rsi
0x18004e2c2: push      rdi
0x18004e2c3: push      r12
0x18004e2c5: push      r13
0x18004e2c7: push      r14
0x18004e2c9: push      r15
0x18004e2cb: push      rbp
0x18004e2cc: sub       rsp, 0x98
0x18004e2d3: mov       r11, rcx
0x18004e2d6: movss     xmm3, dword ptr [rip + 0x2e05a2]
0x18004e2de: xor       ecx, ecx
0x18004e2e0: movss     xmm2, dword ptr [rip + 0x2e059c]
0x18004e2e8: xor       eax, eax
0x18004e2ea: movss     xmm1, dword ptr [rip + 0x2e0596]
0x18004e2f2: mov       ebx, eax
0x18004e2f4: movss     xmm0, dword ptr [rip + 0x2e0590]
0x18004e2fc: mov       r15, rdx
0x18004e2ff: mov       r10, qword ptr [rsp + 0x100]
0x18004e307: mov       rsi, rcx
0x18004e30a: mov       r14, qword ptr [rsp + 0x108]
0x18004e312: mov       rdi, r8
0x18004e315: movups    xmmword ptr [rsp + 0x80], xmm6
0x18004e31d: movaps    xmm6, xmm0
0x18004e320: movups    xmmword ptr [rsp + 0x70], xmm7
0x18004e325: movaps    xmm7, xmm2
0x18004e328: movups    xmmword ptr [rsp + 0x60], xmm8
0x18004e32e: movaps    xmm8, xmm3
0x18004e332: movups    xmmword ptr [rsp + 0x50], xmm9
0x18004e338: movaps    xmm9, xmm1
0x18004e33c: movups    xmmword ptr [rsp + 0x40], xmm10
0x18004e342: mov       rbp, r9
0x18004e345: movups    xmmword ptr [rsp + 0x30], xmm11
0x18004e34b: mov       r13, r10
0x18004e34e: movups    xmmword ptr [rsp + 0x20], xmm12
0x18004e354: mov       r12, r11
0x18004e357: mov       rcx, r15
0x18004e35a: mov       edx, ebx
0x18004e35c: mov       r8, r14
0x18004e35f: mov       dword ptr [r12 + rsi*8 + 0x230], 0
0x18004e36b: mov       dword ptr [r12 + rsi*8 + 0x234], 1
0x18004e377: call      0x18004e2b0
0x18004e37c: movss     xmm5, dword ptr [rdi + rsi*4]
0x18004e381: inc       ebx
0x18004e383: movss     xmm12, dword ptr [rip + 0x2e0504]
0x18004e38c: addss     xmm5, xmm8
0x18004e391: divss     xmm12, xmm5
0x18004e396: movss     xmm4, dword ptr [r13 + rsi*4]
0x18004e39d: movss     xmm10, dword ptr [rbp + rsi*4]
0x18004e3a4: mulss     xmm4, xmm12
0x18004e3a9: mulss     xmm12, xmm10
0x18004e3ae: mulss     xmm4, xmm7
0x18004e3b2: mulss     xmm12, xmm9
0x18004e3b7: movaps    xmm3, xmm4
0x18004e3ba: movaps    xmm11, xmm12
0x18004e3be: mulss     xmm3, xmm4
0x18004e3c2: mulss     xmm11, xmm12
0x18004e3c7: mov       ecx, eax
0x18004e3c9: addss     xmm3, xmm11
0x18004e3ce: sqrtss    xmm3, xmm3
0x18004e3d2: mov       r9, rcx
0x18004e3d5: movaps    xmm2, xmm3
0x18004e3d8: shl       r9, 4
0x18004e3dc: addss     xmm2, xmm12
0x18004e3e1: subss     xmm3, xmm12
0x18004e3e6: mov       r10, qword ptr [r15]
0x18004e3e9: mov       rdx, qword ptr [r15 + 8]
0x18004e3ed: movss     xmm0, dword ptr [r9 + r10 + 8]
0x18004e3f4: movss     xmm1, dword ptr [r9 + r10]
0x18004e3fa: movaps    xmm10, xmm0
0x18004e3fe: movaps    xmm5, xmm1
0x18004e401: mulss     xmm10, xmm4
0x18004e406: mulss     xmm5, xmm2
0x18004e40a: mulss     xmm1, xmm4
0x18004e40e: mulss     xmm0, xmm3
0x18004e412: addss     xmm10, xmm5
0x18004e417: addss     xmm1, xmm0
0x18004e41b: movss     dword ptr [r12 + rsi*8], xmm10
0x18004e421: movss     xmm10, dword ptr [r9 + r10 + 0xc]
0x18004e428: movss     xmm12, dword ptr [r9 + r10 + 4]
0x18004e42f: movaps    xmm5, xmm10
0x18004e433: movaps    xmm11, xmm12
0x18004e437: mulss     xmm5, xmm4
0x18004e43b: mulss     xmm11, xmm2
0x18004e440: addss     xmm2, xmm6
0x18004e444: mulss     xmm12, xmm4
0x18004e449: mulss     xmm10, xmm3
0x18004e44e: addss     xmm3, xmm6
0x18004e452: addss     xmm5, xmm11
0x18004e457: addss     xmm12, xmm10
0x18004e45c: mov       eax, dword ptr [rdx + rcx*8]
0x18004e45f: mov       dword ptr [r12 + rsi*8 + 0x230], eax
0x18004e467: mov       r8d, dword ptr [rdx + rcx*8 + 4]
0x18004e46c: mov       dword ptr [r12 + rsi*8 + 0x234], r8d
0x18004e474: movss     dword ptr [r12 + rsi*8 + 4], xmm5
0x18004e47b: movss     dword ptr [r12 + rsi*8 + 0xa0], xmm1
0x18004e485: movss     dword ptr [r12 + rsi*8 + 0xa4], xmm12
0x18004e48f: lea       rsi, [rsi + rsi*2]
0x18004e493: xorps     xmm2, xmmword ptr [rip + 0x2e03d6]
0x18004e49a: xorps     xmm3, xmmword ptr [rip + 0x2e03cf]
0x18004e4a1: xorps     xmm4, xmmword ptr [rip + 0x2e03c8]
0x18004e4a8: movss     dword ptr [r12 + rsi*4 + 0x140], xmm2
0x18004e4b2: movss     dword ptr [r12 + rsi*4 + 0x144], xmm3
0x18004e4bc: movss     dword ptr [r12 + rsi*4 + 0x148], xmm4
0x18004e4c6: mov       esi, ebx
0x18004e4c8: cmp       ebx, 0x14
0x18004e4cb: jb        0x18004e357
0x18004e4d1: movups    xmm6, xmmword ptr [rsp + 0x80]
0x18004e4d9: movups    xmm7, xmmword ptr [rsp + 0x70]
0x18004e4de: movups    xmm8, xmmword ptr [rsp + 0x60]
0x18004e4e4: movups    xmm9, xmmword ptr [rsp + 0x50]
0x18004e4ea: movups    xmm10, xmmword ptr [rsp + 0x40]
0x18004e4f0: movups    xmm11, xmmword ptr [rsp + 0x30]
0x18004e4f6: movups    xmm12, xmmword ptr [rsp + 0x20]
0x18004e4fc: add       rsp, 0x98
0x18004e503: pop       rbp
0x18004e504: pop       r15
0x18004e506: pop       r14
0x18004e508: pop       r13
0x18004e50a: pop       r12
0x18004e50c: pop       rdi
0x18004e50d: pop       rsi
0x18004e50e: pop       rbx
0x18004e50f: ret       
