0x1800217c0: push      rbx
0x1800217c1: push      rsi
0x1800217c2: push      rdi
0x1800217c3: push      r12
0x1800217c5: push      r13
0x1800217c7: push      r14
0x1800217c9: push      r15
0x1800217cb: push      rbp
0x1800217cc: sub       rsp, 0x128
0x1800217d3: mov       rsi, r9
0x1800217d6: mov       r15d, dword ptr [rsp + 0x1b0]
0x1800217de: xor       edi, edi
0x1800217e0: movups    xmmword ptr [rsp + 0x70], xmm15
0x1800217e6: mov       rbx, r8
0x1800217e9: mov       qword ptr [rsp + 0xc0], rcx
0x1800217f1: mov       r14, rdx
0x1800217f4: mov       r13, qword ptr [rsp + 0x1a0]
0x1800217fc: xor       ebp, ebp
0x1800217fe: movss     xmm15, dword ptr [rsi + 8]
0x180021804: cmp       r15d, dword ptr [rcx + 8]
0x180021808: je        0x180021816
0x18002180a: mov       rdx, r13
0x18002180d: mov       dword ptr [rcx + 8], r15d
0x180021811: call      0x1800117b0
0x180021816: mov       rcx, qword ptr [rsp + 0x1b8]
0x18002181e: mov       r8d, dword ptr [r13]
0x180021822: lea       rax, [rcx + 7]
0x180021826: and       rax, 0xfffffffffffffff8
0x18002182a: mov       qword ptr [rsp + 0x98], rax
0x180021832: lea       rax, [rcx + r8*8 + 7]
0x180021837: xor       ecx, ecx
0x180021839: cmp       dword ptr [rsi + 0xc], 0
0x18002183d: jbe       0x180021931
0x180021843: mov       edx, dword ptr [rsp + 0x1a8]
0x18002184a: xor       r12d, r12d
0x18002184d: mov       qword ptr [rsp + 0xd0], rbp
0x180021855: mov       ebp, r15d
0x180021858: mov       qword ptr [rsp + 0xe8], rdi
0x180021860: mov       r15d, edx
0x180021863: mov       qword ptr [rsp + 0x90], rbx
0x18002186b: mov       rbx, r13
0x18002186e: mov       r13, rax
0x180021871: mov       rdi, rcx
0x180021874: mov       r10, qword ptr [rsi + 0x10]
0x180021878: lea       rax, [rdi + rdi*2]
0x18002187c: mov       rcx, qword ptr [r14 + 0x10]
0x180021880: mov       rdx, rsp
0x180021883: mov       r11, qword ptr [rsp + 0xc0]
0x18002188b: mov       r8, qword ptr [r10 + rax*8 + 0x10]
0x180021890: mov       r9d, dword ptr [r10 + rax*8 + 8]
0x180021895: mov       r11, qword ptr [r11]
0x180021898: mov       dword ptr [rdx + 0x20], r15d
0x18002189c: mov       r9d, dword ptr [r8 + r9*4]
0x1800218a0: mov       dword ptr [rdx + 0x28], ebp
0x1800218a3: mov       dword ptr [rdx + 0x30], 1
0x1800218aa: mov       r9, qword ptr [rcx + r9*8]
0x1800218ae: mov       qword ptr [rdx + 0x38], r9
0x1800218b2: mov       r9d, dword ptr [r10 + rax*8 + 0xc]
0x1800218b7: mov       qword ptr [rdx + 0x50], r13
0x1800218bb: mov       r8d, dword ptr [r8 + r9*4]
0x1800218bf: mov       r9, qword ptr [r10 + rax*8]
0x1800218c3: mov       r8, qword ptr [rcx + r8*8]
0x1800218c7: mov       qword ptr [rdx + 0x40], r8
0x1800218cb: mov       r8d, dword ptr [r14 + 4]
0x1800218cf: mov       dword ptr [rdx + 0x48], r8d
0x1800218d3: mov       rcx, qword ptr [r11 + rdi*8]
0x1800218d7: mov       rdx, qword ptr [rsp + 0x198]
0x1800218df: mov       r8, qword ptr [rsp + 0x190]
0x1800218e7: call      0x180031410
0x1800218ec: mov       r8, qword ptr [rsp + 0x98]
0x1800218f4: mov       ecx, dword ptr [rbx + 4]
0x1800218f7: mov       qword ptr [r8 + rdi*8], rax
0x1800218fb: call      0x1800313d0
0x180021900: inc       r12d
0x180021903: add       r13, rax
0x180021906: mov       edi, r12d
0x180021909: cmp       r12d, dword ptr [rsi + 0xc]
0x18002190d: jb        0x180021874
0x180021913: mov       rbp, qword ptr [rsp + 0xd0]
0x18002191b: mov       rax, r13
0x18002191e: mov       rdi, qword ptr [rsp + 0xe8]
0x180021926: mov       r13, rbx
0x180021929: mov       rbx, qword ptr [rsp + 0x90]
0x180021931: lea       rcx, [rax + 7]
0x180021935: and       rcx, 0xfffffffffffffff8
0x180021939: mov       qword ptr [rsp + 0xf0], rcx
0x180021941: mov       ecx, dword ptr [r13 + 4]
0x180021945: lea       r8, [rax + rcx*8 + 7]
0x18002194a: test      rbx, rbx
0x18002194d: je        0x18002195c
0x18002194f: lea       rdi, [r8 + 7]
0x180021953: and       rdi, 0xfffffffffffffff8
0x180021957: lea       r8, [r8 + rcx*8 + 7]
0x18002195c: mov       edx, dword ptr [r13 + 0xc]
0x180021960: mov       r11d, edx
0x180021963: mov       rax, r11
0x180021966: shl       rax, 4
0x18002196a: lea       r10, [r8 + 0x1f]
0x18002196e: and       r10, 0xffffffffffffffe0
0x180021972: lea       rcx, [rax + r8 + 0x1f]
0x180021977: mov       r9, rcx
0x18002197a: test      edx, edx
0x18002197c: jbe       0x180021ac9
0x180021982: cmp       edx, 8
0x180021985: jb        0x180022518
0x18002198b: mov       r8d, edx
0x18002198e: lea       rax, [rcx + 0x29f]
0x180021995: movq      xmm2, rcx
0x18002199a: and       r8d, 0xfffffff8
0x18002199e: movdqu    xmm3, xmmword ptr [rip + 0x2cd01a]
0x1800219a6: movdqu    xmm1, xmmword ptr [rip + 0x2cd032]
0x1800219ae: movq      xmm0, rax
0x1800219b3: xor       eax, eax
0x1800219b5: punpcklqdq xmm2, xmm0
0x1800219b9: movdqu    xmm0, xmmword ptr [rip + 0x2cd00f]
0x1800219c1: mov       r12d, eax
0x1800219c4: movdqa    xmm4, xmm0
0x1800219c8: shl       r12, 4
0x1800219cc: lea       r13d, [rax + 2]
0x1800219d0: paddq     xmm4, xmm2
0x1800219d4: paddq     xmm2, xmm3
0x1800219d8: movdqa    xmm5, xmm0
0x1800219dc: pand      xmm4, xmm1
0x1800219e0: shl       r13, 4
0x1800219e4: lea       r15d, [rax + 4]
0x1800219e8: paddq     xmm5, xmm2
0x1800219ec: paddq     xmm2, xmm3
0x1800219f0: movq      qword ptr [r12 + r10], xmm4
0x1800219f6: pand      xmm5, xmm1
0x1800219fa: psrldq    xmm4, 8
0x1800219ff: movq      qword ptr [r12 + r10 + 0x10], xmm4
0x180021a06: movdqa    xmm4, xmm0
0x180021a0a: movq      qword ptr [r13 + r10], xmm5
0x180021a11: lea       r12d, [rax + 6]
0x180021a15: psrldq    xmm5, 8
0x180021a1a: add       eax, 8
0x180021a1d: paddq     xmm4, xmm2
0x180021a21: paddq     xmm2, xmm3
0x180021a25: shl       r15, 4
0x180021a29: pand      xmm4, xmm1
0x180021a2d: movq      qword ptr [r13 + r10 + 0x10], xmm5
0x180021a34: movdqa    xmm5, xmm0
0x180021a38: shl       r12, 4
0x180021a3c: paddq     xmm5, xmm2
0x180021a40: paddq     xmm2, xmm3
0x180021a44: movq      qword ptr [r15 + r10], xmm4
0x180021a4a: pand      xmm5, xmm1
0x180021a4e: psrldq    xmm4, 8
0x180021a53: movq      qword ptr [r15 + r10 + 0x10], xmm4
0x180021a5a: movq      qword ptr [r12 + r10], xmm5
0x180021a60: psrldq    xmm5, 8
0x180021a65: movq      qword ptr [r12 + r10 + 0x10], xmm5
0x180021a6c: cmp       eax, r8d
0x180021a6f: jb        0x1800219c1
0x180021a75: movsxd    r12, eax
0x180021a78: sub       eax, r8d
0x180021a7b: sub       r12, rax
0x180021a7e: imul      r13, r12, 0x29f
0x180021a85: add       rcx, r13
0x180021a88: movsxd    r8, r8d
0x180021a8b: imul      rax, r8, 0x29f
0x180021a92: lea       rax, [r9 + rax + 0x1f]
0x180021a97: cmp       r8d, edx
0x180021a9a: jae       0x180021ac9
0x180021a9c: mov       r12d, r8d
0x180021a9f: mov       rcx, rax
0x180021aa2: shl       r12, 4
0x180021aa6: inc       r8d
0x180021aa9: and       rcx, 0xffffffffffffffe0
0x180021aad: add       rax, 0x29f
0x180021ab3: mov       qword ptr [r10 + r12], rcx
0x180021ab7: cmp       r8d, edx
0x180021aba: jb        0x180021a9c
0x180021abc: movsxd    r8, r8d
0x180021abf: imul      rcx, r8, 0x29f
0x180021ac6: add       rcx, r9
0x180021ac9: test      rbx, rbx
0x180021acc: je        0x180021d35
0x180021ad2: lea       rbp, [rcx + 0x1f]
0x180021ad6: and       rbp, 0xffffffffffffffe0
0x180021ada: lea       rcx, [rcx + r11*8 + 7]
0x180021adf: mov       r11, rcx
0x180021ae2: test      edx, edx
0x180021ae4: jbe       0x180021d35
0x180021aea: mov       r13d, dword ptr [rbx + 8]
0x180021aee: cmp       edx, 8
0x180021af1: jb        0x18002258f
0x180021af7: mov       r15d, edx
0x180021afa: lea       r9d, [r13 + 3]
0x180021afe: shr       r9d, 2
0x180021b02: movq      xmm2, rcx
0x180021b07: shl       r9d, 3
0x180021b0b: and       r15d, 0xfffffff8
0x180021b0f: movdqu    xmm1, xmmword ptr [rip + 0x2cceb9]
0x180021b17: lea       r12, [r9*4]
0x180021b1f: mov       qword ptr [rsp + 0x80], r12
0x180021b27: lea       r12, [r9*4 + 0x1f]
0x180021b2f: lea       rax, [r12 + r12]
0x180021b33: lea       r8, [rcx + r12]
0x180021b37: movq      xmm3, rax
0x180021b3c: movq      xmm0, r8
0x180021b41: xor       r8d, r8d
0x180021b44: punpcklqdq xmm3, xmm3
0x180021b48: punpcklqdq xmm2, xmm0
0x180021b4c: movdqu    xmm0, xmmword ptr [rip + 0x2cce8c]
0x180021b54: nop       dword ptr [rax + rax]
0x180021b59: nop       dword ptr [rax]
0x180021b60: movdqa    xmm4, xmm1
0x180021b64: movdqa    xmm5, xmm1
0x180021b68: paddq     xmm4, xmm2
0x180021b6c: paddq     xmm2, xmm3
0x180021b70: paddq     xmm5, xmm2
0x180021b74: paddq     xmm2, xmm3
0x180021b78: pand      xmm4, xmm0
0x180021b7c: lea       eax, [r8 + 2]
0x180021b80: movdqu    xmmword ptr [rbp + r8*8], xmm4
0x180021b87: movdqa    xmm4, xmm1
0x180021b8b: paddq     xmm4, xmm2
0x180021b8f: paddq     xmm2, xmm3
0x180021b93: pand      xmm5, xmm0
0x180021b97: pand      xmm4, xmm0
0x180021b9b: movdqu    xmmword ptr [rbp + rax*8], xmm5
0x180021ba1: movdqa    xmm5, xmm1
0x180021ba5: paddq     xmm5, xmm2
0x180021ba9: paddq     xmm2, xmm3
0x180021bad: pand      xmm5, xmm0
0x180021bb1: lea       eax, [r8 + 4]
0x180021bb5: movdqu    xmmword ptr [rbp + rax*8], xmm4
0x180021bbb: lea       eax, [r8 + 6]
0x180021bbf: add       r8d, 8
0x180021bc3: movdqu    xmmword ptr [rbp + rax*8], xmm5
0x180021bc9: cmp       r8d, r15d
0x180021bcc: jb        0x180021b60
0x180021bce: mov       eax, r8d
0x180021bd1: sub       r8d, r15d
0x180021bd4: sub       rax, r8
0x180021bd7: lea       rcx, [rcx + r9*4]
0x180021bdb: imul      rax, r12
0x180021bdf: mov       r9, qword ptr [rsp + 0x80]
0x180021be7: neg       r9
0x180021bea: add       rcx, r9
0x180021bed: add       rcx, rax
0x180021bf0: lea       eax, [r15 + 1]
0x180021bf4: cmp       eax, edx
0x180021bf6: ja        0x180021d35
0x180021bfc: sub       edx, r15d
0x180021bff: cmp       edx, 2
0x180021c02: jb        0x180022587
0x180021c08: mov       ecx, r15d
0x180021c0b: lea       r8d, [r13 + 3]
0x180021c0f: shr       r8d, 2
0x180021c13: mov       r12d, edx
0x180021c16: shl       r8d, 3
0x180021c1a: and       r12d, 0xfffffffe
0x180021c1e: movdqu    xmm1, xmmword ptr [rip + 0x2ccdaa]
0x180021c26: lea       rax, [r8*4]
0x180021c2e: mov       qword ptr [rsp + 0x88], rax
0x180021c36: lea       rax, [r8*4 + 0x1f]
0x180021c3e: imul      rcx, rax
0x180021c42: mov       qword ptr [rsp + 0x80], rcx
0x180021c4a: add       rcx, r11
0x180021c4d: lea       r9, [rax + rax]
0x180021c51: movq      xmm2, rcx
0x180021c56: add       rcx, rax
0x180021c59: movq      xmm3, r9
0x180021c5e: xor       r9d, r9d
0x180021c61: punpcklqdq xmm3, xmm3
0x180021c65: movq      xmm0, rcx
0x180021c6a: mov       ecx, r15d
0x180021c6d: punpcklqdq xmm2, xmm0
0x180021c71: movdqu    xmm0, xmmword ptr [rip + 0x2ccd67]
0x180021c79: movdqa    xmm4, xmm1
0x180021c7d: paddq     xmm4, xmm2
0x180021c81: paddq     xmm2, xmm3
0x180021c85: add       r9d, 2
0x180021c89: pand      xmm4, xmm0
0x180021c8d: movdqu    xmmword ptr [rbp + rcx*8], xmm4
0x180021c93: add       ecx, 2
0x180021c96: cmp       r9d, r12d
0x180021c99: jb        0x180021c79
0x180021c9b: mov       ecx, r9d
0x180021c9e: sub       r9d, r12d
0x180021ca1: sub       rcx, r9
0x180021ca4: lea       r8, [r11 + r8*4]
0x180021ca8: imul      rcx, rax
0x180021cac: mov       rax, qword ptr [rsp + 0x88]
0x180021cb4: neg       rax
0x180021cb7: add       r8, rax
0x180021cba: add       r8, qword ptr [rsp + 0x80]
0x180021cc2: add       rcx, r8
0x180021cc5: lea       eax, [r15 + r12]
0x180021cc9: cmp       r12d, edx
0x180021ccc: jae       0x180021d35
0x180021cce: add       r13d, 3
0x180021cd2: shr       r13d, 2
0x180021cd6: shl       r13d, 3
0x180021cda: lea       r9, [r13*4 + 0x1f]
0x180021ce2: imul      r15, r9
0x180021ce6: mov       qword ptr [rsp + 0x88], rsi
0x180021cee: lea       r8, [r13*4]
0x180021cf6: lea       rcx, [r11 + r15]
0x180021cfa: mov       esi, r12d
0x180021cfd: inc       r12d
0x180021d00: imul      rsi, r9
0x180021d04: lea       rsi, [rcx + rsi + 0x1f]
0x180021d09: and       rsi, 0xffffffffffffffe0
0x180021d0d: mov       qword ptr [rbp + rax*8], rsi
0x180021d12: inc       eax
0x180021d14: cmp       r12d, edx
0x180021d17: jb        0x180021cfa
0x180021d19: lea       rcx, [r11 + r13*4]
0x180021d1d: imul      r9, r12
0x180021d21: neg       r8
0x180021d24: add       rcx, r8
0x180021d27: add       rcx, r15
0x180021d2a: mov       rsi, qword ptr [rsp + 0x88]
0x180021d32: add       rcx, r9
0x180021d35: mov       eax, dword ptr [rsi + 4]
0x180021d38: lea       rdx, [rcx + 3]
0x180021d3c: and       rdx, 0xfffffffffffffffc
0x180021d40: lea       rcx, [rcx + rax*4 + 3]
0x180021d45: xor       r12d, r12d
0x180021d48: lea       r8, [rcx + 3]
0x180021d4c: and       r8, 0xfffffffffffffffc
0x180021d50: lea       r11, [rcx + rax*4 + 3]
0x180021d55: xor       ecx, ecx
0x180021d57: cmp       dword ptr [r14 + 4], 0
0x180021d5c: jbe       0x1800224f4
0x180021d62: movaps    xmm0, xmm15
0x180021d66: xor       r9d, r9d
0x180021d69: shufps    xmm0, xmm0, 0
0x180021d6d: mov       qword ptr [rsp + 0xe8], rdi
0x180021d75: movups    xmmword ptr [rsp + 0x60], xmm14
0x180021d7b: movaps    xmm14, xmm0
0x180021d7f: mov       qword ptr [rsp + 0xc8], r11
0x180021d87: mov       qword ptr [rsp + 0xd0], rbp
0x180021d8f: mov       qword ptr [rsp + 0xf8], r10
0x180021d97: mov       rdi, qword ptr [rsp + 0xf0]
0x180021d9f: mov       r13, qword ptr [rsp + 0x98]
0x180021da7: xor       r10d, r10d
0x180021daa: test      eax, eax
0x180021dac: jbe       0x180021dd9
0x180021dae: mov       eax, r10d
0x180021db1: cmp       r10d, dword ptr [rsi]
0x180021db4: jae       0x180021dbf
0x180021db6: mov       dword ptr [rdx + rax*4], 1
0x180021dbd: jmp       0x180021dc6
0x180021dbf: mov       dword ptr [rdx + rax*4], 0
0x180021dc6: mov       dword ptr [r8 + rax*4], 0
0x180021dce: inc       r10d
0x180021dd1: mov       eax, dword ptr [rsi + 4]
0x180021dd4: cmp       r10d, eax
0x180021dd7: jb        0x180021dae
0x180021dd9: mov       r11d, dword ptr [rsi + 0xc]
0x180021ddd: xor       r15d, r15d
0x180021de0: test      r11d, r11d
0x180021de3: jbe       0x1800223e3
0x180021de9: mov       r10, qword ptr [rsi + 0x10]
0x180021ded: lea       r11, [r15 + r15*2]
0x180021df1: inc       r15d
0x180021df4: mov       rax, qword ptr [r10 + r11*8 + 0x10]
0x180021df9: mov       ebp, dword ptr [r10 + r11*8 + 8]
0x180021dfe: mov       ebp, dword ptr [rax + rbp*4]
0x180021e01: inc       dword ptr [r8 + rbp*4]
0x180021e05: mov       r11d, dword ptr [r10 + r11*8 + 0xc]
0x180021e0a: mov       eax, dword ptr [rax + r11*4]
0x180021e0e: inc       dword ptr [r8 + rax*4]
0x180021e12: mov       r11d, dword ptr [rsi + 0xc]
0x180021e16: cmp       r15d, r11d
0x180021e19: jb        0x180021ded
0x180021e1b: xor       eax, eax
0x180021e1d: test      r11d, r11d
0x180021e20: jbe       0x18002257f
0x180021e26: mov       qword ptr [rsp + 0x118], rcx
0x180021e2e: xor       ebp, ebp
0x180021e30: mov       dword ptr [rsp + 0x80], r9d
0x180021e38: mov       qword ptr [rsp + 0x108], r8
0x180021e40: mov       qword ptr [rsp + 0x100], rdx
0x180021e48: mov       qword ptr [rsp + 0x110], r14
0x180021e50: xor       r11d, r11d
0x180021e53: lea       r14, [rax + rax*2]
0x180021e57: mov       r8, qword ptr [r10 + r14*8]
0x180021e5b: xor       r9d, r9d
0x180021e5e: xor       ecx, ecx
0x180021e60: xor       r15d, r15d
0x180021e63: xor       edx, edx
0x180021e65: cmp       dword ptr [r8], 0
0x180021e69: jbe       0x18002217a
0x180021e6f: mov       qword ptr [rsp + 0xb0], rdx
0x180021e77: mov       qword ptr [rsp + 0xe0], r9
0x180021e7f: mov       qword ptr [rsp + 0xb8], rcx
0x180021e87: mov       qword ptr [rsp + 0xd8], r11
0x180021e8f: mov       dword ptr [rsp + 0xa0], ebp
0x180021e96: mov       rbp, r10
0x180021e99: mov       qword ptr [rsp + 0xa8], rax
0x180021ea1: mov       qword ptr [rsp + 0x88], rsi
0x180021ea9: mov       esi, r15d
0x180021eac: xor       edx, edx
0x180021eae: mov       rax, qword ptr [rbp + r14*8 + 0x10]
0x180021eb3: cmp       r15d, dword ptr [rbp + r14*8 + 8]
0x180021eb8: mov       edi, dword ptr [rax + rsi*4]
0x180021ebb: mov       eax, 0
0x180021ec0: sete      al
0x180021ec3: cmp       r15d, dword ptr [rbp + r14*8 + 0xc]
0x180021ec8: sete      dl
0x180021ecb: or        edx, eax
0x180021ecd: je        0x18002206d
0x180021ed3: mov       rdx, qword ptr [rsp + 0x110]
0x180021edb: mov       r9, qword ptr [rsp + 0x118]
0x180021ee3: mov       rcx, qword ptr [rdx + 0x10]
0x180021ee7: mov       rdx, qword ptr [rsp + 0xf0]
0x180021eef: mov       r8, qword ptr [rcx + rdi*8]
0x180021ef3: mov       r11, qword ptr [r8 + r9*8]
0x180021ef7: mov       qword ptr [rdx + rsi*8], r11
0x180021efb: test      rbx, rbx
0x180021efe: je        0x180021f1b
0x180021f00: mov       rdx, qword ptr [rbx + 0x10]
0x180021f04: mov       r8, r9
0x180021f07: mov       r11, qword ptr [rsp + 0xe8]
0x180021f0f: mov       rcx, qword ptr [rdx + rdi*8]
0x180021f13: mov       r9, qword ptr [rcx + r8*8]
0x180021f17: mov       qword ptr [r11 + rsi*8], r9
0x180021f1b: mov       r13, r12
0x180021f1e: shl       r13, 4
0x180021f22: test      eax, eax
0x180021f24: je        0x180021fc2
0x180021f2a: mov       rax, qword ptr [rsp + 0xf8]
0x180021f32: mov       r8d, 0x4d
0x180021f38: mov       r9, qword ptr [rsp + 0xf0]
0x180021f40: mov       rcx, qword ptr [r13 + rax]
0x180021f45: mov       rdx, qword ptr [r9 + rsi*8]
0x180021f49: mov       qword ptr [rsp + 0xd8], rcx
0x180021f51: call      0x1800eab40
0x180021f56: test      rbx, rbx
0x180021f59: je        0x180022049
0x180021f5f: mov       rax, qword ptr [rsp + 0xd0]
0x180021f67: mov       rdx, r12
0x180021f6a: mov       rcx, qword ptr [rsp + 0xe8]
0x180021f72: mov       r8d, dword ptr [rbx + 8]
0x180021f76: mov       r9, qword ptr [rax + rdx*8]
0x180021f7a: mov       rdx, qword ptr [rcx + rsi*8]
0x180021f7e: mov       qword ptr [rsp + 0xb8], r9
0x180021f86: lea       rsi, [r9 + 0xf]
0x180021f8a: and       rsi, 0xfffffffffffffff0
0x180021f8e: cmp       r9, rsi
0x180021f91: jne       0x180022041
0x180021f97: lea       rax, [rdx + 0xf]
0x180021f9b: and       rax, 0xfffffffffffffff0
0x180021f9f: cmp       rdx, rax
0x180021fa2: jne       0x180022041
0x180021fa8: test      r8d, 3
0x180021faf: jne       0x180022041
0x180021fb5: mov       rcx, r9
0x180021fb8: call      0x1800eab40
0x180021fbd: jmp       0x180022049
0x180021fc2: mov       rax, qword ptr [rsp + 0xf8]
0x180021fca: mov       r8d, 0x4d
0x180021fd0: mov       r9, qword ptr [rsp + 0xf0]
0x180021fd8: mov       rcx, qword ptr [r13 + rax]
0x180021fdd: mov       rdx, qword ptr [r9 + rsi*8]
0x180021fe1: mov       qword ptr [rsp + 0xe0], rcx
0x180021fe9: call      0x1800eab40
0x180021fee: test      rbx, rbx
0x180021ff1: je        0x180022049
0x180021ff3: mov       rax, qword ptr [rsp + 0xd0]
0x180021ffb: mov       rdx, r12
0x180021ffe: mov       rcx, qword ptr [rsp + 0xe8]
0x180022006: mov       r8d, dword ptr [rbx + 8]
0x18002200a: mov       r9, qword ptr [rax + rdx*8]
0x18002200e: mov       rdx, qword ptr [rcx + rsi*8]
0x180022012: mov       qword ptr [rsp + 0xb0], r9
0x18002201a: lea       rsi, [r9 + 0xf]
0x18002201e: and       rsi, 0xfffffffffffffff0
0x180022022: cmp       r9, rsi
0x180022025: jne       0x180022041
0x180022027: lea       rax, [rdx + 0xf]
0x18002202b: and       rax, 0xfffffffffffffff0
0x18002202f: cmp       rdx, rax
0x180022032: jne       0x180022041
0x180022034: test      r8d, 3
0x18002203b: je        0x180021fb5
0x180022041: mov       rcx, r9
0x180022044: call      0x1800eab40
0x180022049: mov       rax, qword ptr [rsp + 0xf8]
0x180022051: inc       r12d
0x180022054: mov       rdx, qword ptr [rsp + 0x108]
0x18002205c: mov       dword ptr [r13 + rax + 8], 0xffffffff
0x180022065: dec       dword ptr [rdx + rdi*4]
0x180022068: jmp       0x180022122
0x18002206d: mov       rax, qword ptr [rsp + 0x100]
0x180022075: cmp       dword ptr [rax + rdi*4], 0
0x180022079: jne       0x1800220d4
0x18002207b: mov       rax, qword ptr [rsp + 0x110]
0x180022083: mov       r8, qword ptr [rsp + 0x118]
0x18002208b: mov       r11, qword ptr [rsp + 0xf0]
0x180022093: mov       rdx, qword ptr [rax + 0x10]
0x180022097: mov       rcx, qword ptr [rdx + rdi*8]
0x18002209b: mov       r9, qword ptr [rcx + r8*8]
0x18002209f: mov       qword ptr [r11 + rsi*8], r9
0x1800220a3: test      rbx, rbx
0x1800220a6: je        0x1800220c3
0x1800220a8: mov       rax, qword ptr [rbx + 0x10]
0x1800220ac: mov       rcx, r8
0x1800220af: mov       r9, qword ptr [rsp + 0xe8]
0x1800220b7: mov       rdx, qword ptr [rax + rdi*8]
0x1800220bb: mov       r8, qword ptr [rdx + rcx*8]
0x1800220bf: mov       qword ptr [r9 + rsi*8], r8
0x1800220c3: mov       rax, qword ptr [rsp + 0x100]
0x1800220cb: mov       dword ptr [rax + rdi*4], 1
0x1800220d2: jmp       0x180022122
0x1800220d4: mov       r9d, r12d
0x1800220d7: mov       rax, r9
0x1800220da: shl       rax, 4
0x1800220de: mov       rdx, qword ptr [rsp + 0xf8]
0x1800220e6: mov       r8, qword ptr [rsp + 0xf0]
0x1800220ee: mov       rcx, qword ptr [rax + rdx]
0x1800220f2: mov       qword ptr [r8 + rsi*8], rcx
0x1800220f6: test      rbx, rbx
0x1800220f9: je        0x180022113
0x1800220fb: mov       rdx, qword ptr [rsp + 0xd0]
0x180022103: mov       r8, qword ptr [rsp + 0xe8]
0x18002210b: mov       rcx, qword ptr [rdx + r9*8]
0x18002210f: mov       qword ptr [r8 + rsi*8], rcx
0x180022113: mov       rdx, qword ptr [rsp + 0xf8]
0x18002211b: inc       r12d
0x18002211e: mov       dword ptr [rax + rdx + 8], edi
0x180022122: mov       r8, qword ptr [rbp + r14*8]
0x180022127: inc       r15d
0x18002212a: cmp       r15d, dword ptr [r8]
0x18002212d: jb        0x180021ea9
0x180022133: mov       rdx, qword ptr [rsp + 0xb0]
0x18002213b: mov       r9, qword ptr [rsp + 0xe0]
0x180022143: mov       rcx, qword ptr [rsp + 0xb8]
0x18002214b: mov       r11, qword ptr [rsp + 0xd8]
0x180022153: mov       ebp, dword ptr [rsp + 0xa0]
0x18002215a: mov       rax, qword ptr [rsp + 0xa8]
0x180022162: mov       rdi, qword ptr [rsp + 0xf0]
0x18002216a: mov       r13, qword ptr [rsp + 0x98]
0x180022172: mov       rsi, qword ptr [rsp + 0x88]
0x18002217a: test      rbx, rbx
0x18002217d: je        0x180022185
0x18002217f: mov       r15d, dword ptr [rbx + 8]
0x180022183: jmp       0x180022188
0x180022185: xor       r15d, r15d
0x180022188: mov       r14, rsp
0x18002218b: mov       r10, qword ptr [rsp + 0xc0]
0x180022193: mov       qword ptr [r14 + 0x20], r11
0x180022197: mov       qword ptr [r14 + 0x28], r9
0x18002219b: mov       qword ptr [r14 + 0x30], rdi
0x18002219f: mov       r11, qword ptr [rsp + 0xc8]
0x1800221a7: mov       r10, qword ptr [r10]
0x1800221aa: mov       qword ptr [r14 + 0x38], r11
0x1800221ae: mov       qword ptr [r14 + 0x40], rcx
0x1800221b2: mov       qword ptr [r14 + 0x48], rdx
0x1800221b6: mov       rdx, qword ptr [rsp + 0xe8]
0x1800221be: mov       qword ptr [r14 + 0x50], rdx
0x1800221c2: mov       rcx, qword ptr [r10 + rax*8]
0x1800221c6: mov       rdx, qword ptr [rsp + 0x190]
0x1800221ce: mov       r9, qword ptr [r13 + rax*8]
0x1800221d3: mov       dword ptr [r14 + 0x58], r15d
0x1800221d7: call      0x180031860
0x1800221dc: lea       eax, [r12 - 1]
0x1800221e1: cmp       eax, -1
0x1800221e4: je        0x1800223a2
0x1800221ea: mov       dword ptr [rsp + 0xa0], ebp
0x1800221f1: mov       qword ptr [rsp + 0x88], rsi
0x1800221f9: mov       rdi, qword ptr [rsp + 0x118]
0x180022201: mov       r8, qword ptr [rsp + 0x108]
0x180022209: mov       r13, qword ptr [rsp + 0xd0]
0x180022211: mov       rsi, qword ptr [rsp + 0xf8]
0x180022219: mov       rbp, qword ptr [rsp + 0x110]
0x180022221: mov       edx, eax
0x180022223: mov       r9, rdx
0x180022226: shl       r9, 4
0x18002222a: movsxd    r15, dword ptr [r9 + rsi + 8]
0x18002222f: test      r15, r15
0x180022232: jl        0x180022532
0x180022238: cmp       dword ptr [r8 + r15*4], 0
0x18002223d: jne       0x180022378
0x180022243: mov       rcx, qword ptr [rbp + 0x10]
0x180022247: mov       r11, qword ptr [rsi + r9]
0x18002224b: mov       r10, qword ptr [rcx + r15*8]
0x18002224f: mov       r14, qword ptr [r10 + rdi*8]
0x180022253: xor       r10d, r10d
0x180022256: lea       ecx, [r10 + r10]
0x18002225a: add       r10d, 2
0x18002225e: movups    xmm2, xmmword ptr [r14 + rcx*4]
0x180022263: movups    xmm1, xmmword ptr [r11 + rcx*4]
0x180022268: addps     xmm2, xmm1
0x18002226b: movups    xmmword ptr [r14 + rcx*4], xmm2
0x180022270: cmp       r10d, 0x4c
0x180022274: jb        0x180022256
0x180022276: lea       ecx, [r10 + r10]
0x18002227a: movss     xmm1, dword ptr [r14 + rcx*4]
0x180022280: lea       r10d, [r10 + r10 + 1]
0x180022285: movss     xmm2, dword ptr [r14 + r10*4]
0x18002228b: addss     xmm1, dword ptr [r11 + rcx*4]
0x180022291: addss     xmm2, dword ptr [r11 + r10*4]
0x180022297: movss     dword ptr [r14 + rcx*4], xmm1
0x18002229d: movss     dword ptr [r14 + r10*4], xmm2
0x1800222a3: test      rbx, rbx
0x1800222a6: je        0x180022333
0x1800222ac: mov       rcx, qword ptr [rbx + 0x10]
0x1800222b0: mov       r11, qword ptr [r13 + rdx*8]
0x1800222b5: mov       r10, qword ptr [rcx + r15*8]
0x1800222b9: xor       ecx, ecx
0x1800222bb: mov       r15d, dword ptr [rbx + 8]
0x1800222bf: mov       r14d, r15d
0x1800222c2: mov       r10, qword ptr [r10 + rdi*8]
0x1800222c6: dec       r14d
0x1800222c9: je        0x1800222f8
0x1800222cb: mov       qword ptr [rsp + 0x90], rbx
0x1800222d3: lea       ebx, [rcx + rcx]
0x1800222d6: add       ecx, 2
0x1800222d9: movups    xmm2, xmmword ptr [r10 + rbx*4]
0x1800222de: movups    xmm1, xmmword ptr [r11 + rbx*4]
0x1800222e3: addps     xmm2, xmm1
0x1800222e6: movups    xmmword ptr [r10 + rbx*4], xmm2
0x1800222eb: cmp       ecx, r14d
0x1800222ee: jb        0x1800222d3
0x1800222f0: mov       rbx, qword ptr [rsp + 0x90]
0x1800222f8: cmp       ecx, r15d
0x1800222fb: jae       0x180022329
0x1800222fd: lea       r14d, [rcx + rcx]
0x180022301: movss     xmm1, dword ptr [r10 + r14*4]
0x180022307: lea       ecx, [rcx + rcx + 1]
0x18002230b: movss     xmm2, dword ptr [r10 + rcx*4]
0x180022311: addss     xmm1, dword ptr [r11 + r14*4]
0x180022317: addss     xmm2, dword ptr [r11 + rcx*4]
0x18002231d: movss     dword ptr [r10 + r14*4], xmm1
0x180022323: movss     dword ptr [r10 + rcx*4], xmm2
0x180022329: lea       ecx, [rax + 1]
0x18002232c: cmp       ecx, r12d
0x18002232f: jb        0x18002233b
0x180022331: jmp       0x180022375
0x180022333: lea       ecx, [rax + 1]
0x180022336: cmp       ecx, r12d
0x180022339: jae       0x180022375
0x18002233b: dec       r12d
0x18002233e: mov       r10d, r12d
0x180022341: mov       rcx, r10
0x180022344: shl       rcx, 4
0x180022348: movups    xmm2, xmmword ptr [rsi + r9]
0x18002234d: movups    xmm1, xmmword ptr [rcx + rsi]
0x180022351: movups    xmmword ptr [rsi + r9], xmm1
0x180022356: movups    xmmword ptr [rcx + rsi], xmm2
0x18002235a: test      rbx, rbx
0x18002235d: je        0x180022378
0x18002235f: mov       rcx, qword ptr [r13 + r10*8]
0x180022364: mov       r9, qword ptr [r13 + rdx*8]
0x180022369: mov       qword ptr [r13 + rdx*8], rcx
0x18002236e: mov       qword ptr [r13 + r10*8], r9
0x180022373: jmp       0x180022378
0x180022375: dec       r12d
0x180022378: dec       eax
0x18002237a: cmp       eax, -1
0x18002237d: jne       0x180022221
0x180022383: mov       ebp, dword ptr [rsp + 0xa0]
0x18002238a: mov       rdi, qword ptr [rsp + 0xf0]
0x180022392: mov       r13, qword ptr [rsp + 0x98]
0x18002239a: mov       rsi, qword ptr [rsp + 0x88]
0x1800223a2: inc       ebp
0x1800223a4: mov       r11d, dword ptr [rsi + 0xc]
0x1800223a8: mov       eax, ebp
0x1800223aa: cmp       ebp, r11d
0x1800223ad: jae       0x1800223b8
0x1800223af: mov       r10, qword ptr [rsi + 0x10]
0x1800223b3: jmp       0x180021e50
0x1800223b8: mov       rcx, qword ptr [rsp + 0x118]
0x1800223c0: mov       r9d, dword ptr [rsp + 0x80]
0x1800223c8: mov       r8, qword ptr [rsp + 0x108]
0x1800223d0: mov       rdx, qword ptr [rsp + 0x100]
0x1800223d8: mov       r14, qword ptr [rsp + 0x110]
0x1800223e0: mov       eax, dword ptr [rsi + 4]
0x1800223e3: xor       ebp, ebp
0x1800223e5: test      eax, eax
0x1800223e7: jbe       0x180022520
0x1800223ed: mov       r10d, ebp
0x1800223f0: cmp       dword ptr [rdx + r10*4], 0
0x1800223f5: je        0x1800224d0
0x1800223fb: mov       rax, qword ptr [r14 + 0x10]
0x1800223ff: mov       r11, qword ptr [rax + r10*8]
0x180022403: mov       r15, qword ptr [r11 + rcx*8]
0x180022407: xor       r11d, r11d
0x18002240a: lea       eax, [r11 + r11]
0x18002240e: add       r11d, 2
0x180022412: movups    xmm1, xmmword ptr [r15 + rax*4]
0x180022417: mulps     xmm1, xmm14
0x18002241b: movups    xmmword ptr [r15 + rax*4], xmm1
0x180022420: cmp       r11d, 0x4c
0x180022424: jb        0x18002240a
0x180022426: lea       eax, [r11 + r11]
0x18002242a: movss     xmm1, dword ptr [r15 + rax*4]
0x180022430: lea       r11d, [r11 + r11 + 1]
0x180022435: movss     xmm2, dword ptr [r15 + r11*4]
0x18002243b: mulss     xmm1, xmm15
0x180022440: mulss     xmm2, xmm15
0x180022445: movss     dword ptr [r15 + rax*4], xmm1
0x18002244b: movss     dword ptr [r15 + r11*4], xmm2
0x180022451: test      rbx, rbx
0x180022454: je        0x1800224cd
0x180022456: mov       rax, qword ptr [rbx + 0x10]
0x18002245a: mov       r11d, dword ptr [rbx + 8]
0x18002245e: mov       r15d, r11d
0x180022461: mov       r10, qword ptr [rax + r10*8]
0x180022465: xor       eax, eax
0x180022467: mov       r10, qword ptr [r10 + rcx*8]
0x18002246b: dec       r15d
0x18002246e: je        0x180022499
0x180022470: mov       qword ptr [rsp + 0x88], rsi
0x180022478: lea       esi, [rax + rax]
0x18002247b: add       eax, 2
0x18002247e: movups    xmm1, xmmword ptr [r10 + rsi*4]
0x180022483: mulps     xmm1, xmm14
0x180022487: movups    xmmword ptr [r10 + rsi*4], xmm1
0x18002248c: cmp       eax, r15d
0x18002248f: jb        0x180022478
0x180022491: mov       rsi, qword ptr [rsp + 0x88]
0x180022499: cmp       eax, r11d
0x18002249c: jae       0x1800224cd
0x18002249e: lea       r11d, [rax + rax]
0x1800224a2: movss     xmm1, dword ptr [r10 + r11*4]
0x1800224a8: lea       eax, [rax + rax + 1]
0x1800224ac: movss     xmm2, dword ptr [r10 + rax*4]
0x1800224b2: mulss     xmm1, xmm15
0x1800224b7: mulss     xmm2, xmm15
0x1800224bc: movss     dword ptr [r10 + r11*4], xmm1
0x1800224c2: movss     dword ptr [r10 + rax*4], xmm2
0x1800224c8: mov       eax, dword ptr [rsi + 4]
0x1800224cb: jmp       0x1800224d0
0x1800224cd: mov       eax, dword ptr [rsi + 4]
0x1800224d0: inc       ebp
0x1800224d2: cmp       ebp, eax
0x1800224d4: jb        0x1800223ed
0x1800224da: nop       dword ptr [rax]
0x1800224de: inc       r9d
0x1800224e1: mov       ecx, r9d
0x1800224e4: cmp       r9d, dword ptr [r14 + 4]
0x1800224e8: jb        0x180021da7
0x1800224ee: movups    xmm14, xmmword ptr [rsp + 0x60]
0x1800224f4: mov       dword ptr [r14], eax
0x1800224f7: test      rbx, rbx
0x1800224fa: je        0x1800224fe
0x1800224fc: mov       dword ptr [rbx], eax
0x1800224fe: movups    xmm15, xmmword ptr [rsp + 0x70]
0x180022504: add       rsp, 0x128
0x18002250b: pop       rbp
0x18002250c: pop       r15
0x18002250e: pop       r14
0x180022510: pop       r13
0x180022512: pop       r12
0x180022514: pop       rdi
0x180022515: pop       rsi
0x180022516: pop       rbx
0x180022517: ret       
0x180022518: xor       r8d, r8d
0x18002251b: jmp       0x180021a88
0x180022520: inc       r9d
0x180022523: mov       ecx, r9d
0x180022526: cmp       r9d, dword ptr [r14 + 4]
0x18002252a: jb        0x180021ddd
0x180022530: jmp       0x1800224ee
0x180022532: lea       ecx, [rax + 1]
0x180022535: cmp       ecx, r12d
0x180022538: jae       0x180022375
0x18002253e: dec       r12d
0x180022541: mov       r10d, r12d
0x180022544: mov       rcx, r10
0x180022547: shl       rcx, 4
0x18002254b: movups    xmm2, xmmword ptr [rsi + r9]
0x180022550: movups    xmm1, xmmword ptr [rcx + rsi]
0x180022554: movups    xmmword ptr [rsi + r9], xmm1
0x180022559: movups    xmmword ptr [rcx + rsi], xmm2
0x18002255d: test      rbx, rbx
0x180022560: je        0x180022378
0x180022566: mov       rcx, qword ptr [r13 + r10*8]
0x18002256b: mov       r9, qword ptr [r13 + rdx*8]
0x180022570: mov       qword ptr [r13 + rdx*8], rcx
0x180022575: mov       qword ptr [r13 + r10*8], r9
0x18002257a: jmp       0x180022378
0x18002257f: mov       eax, dword ptr [rsi + 4]
0x180022582: jmp       0x1800223e3
0x180022587: xor       r12d, r12d
0x18002258a: jmp       0x180021cc5
0x18002258f: xor       r15d, r15d
0x180022592: jmp       0x180021bf0
0x180022597: nop       word ptr [rax + rax]
0x1800225a0: sub       rsp, 0x68
0x1800225a4: mov       r8, rcx
0x1800225a7: xor       eax, eax
0x1800225a9: cmp       dword ptr [r8 + 4], 0
0x1800225ae: jbe       0x18002262d
0x1800225b0: movss     xmm0, dword ptr [rip + 0x2cc438]
0x1800225b8: mov       ecx, 0xbf34fe00
0x1800225bd: xor       edx, edx
0x1800225bf: movups    xmmword ptr [rsp + 0x40], xmm6
0x1800225c4: movaps    xmm6, xmm0
0x1800225c7: mov       qword ptr [rsp + 0x60], rbx
0x1800225cc: mov       ebx, edx
0x1800225ce: mov       qword ptr [rsp + 0x58], rsi
0x1800225d3: mov       esi, ecx
0x1800225d5: mov       qword ptr [rsp + 0x50], rdi
0x1800225da: mov       rdi, r8
0x1800225dd: mov       r9, qword ptr [rdi + 0x10]
0x1800225e1: mov       r11, rsp
0x1800225e4: mov       r10d, dword ptr [rdi + 8]
0x1800225e8: movaps    xmm2, xmm6
0x1800225eb: movaps    xmm3, xmm6
0x1800225ee: mov       r8, qword ptr [r9]
0x1800225f1: mov       r9, qword ptr [r9 + 8]
0x1800225f5: movss     dword ptr [r11 + 0x20], xmm6
0x1800225fb: mov       rcx, qword ptr [r8 + rax*8]
0x1800225ff: mov       rdx, qword ptr [r9 + rax*8]
0x180022603: mov       dword ptr [r11 + 0x28], esi
0x180022607: mov       dword ptr [r11 + 0x30], r10d
0x18002260b: call      0x180045f80
0x180022610: inc       ebx
0x180022612: mov       eax, ebx
0x180022614: cmp       ebx, dword ptr [rdi + 4]
0x180022617: jb        0x1800225dd
0x180022619: movups    xmm6, xmmword ptr [rsp + 0x40]
0x18002261e: mov       rbx, qword ptr [rsp + 0x60]
0x180022623: mov       rsi, qword ptr [rsp + 0x58]
0x180022628: mov       rdi, qword ptr [rsp + 0x50]
0x18002262d: add       rsp, 0x68
0x180022631: ret       
0x180022632: nop       dword ptr [rax]
0x180022639: nop       dword ptr [rax]
