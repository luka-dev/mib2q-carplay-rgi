0x18004d6d0: push      r12
0x18004d6d2: push      r13
0x18004d6d4: push      r14
0x18004d6d6: push      r15
0x18004d6d8: push      rbp
0x18004d6d9: sub       rsp, 0xf0
0x18004d6e0: mov       r13, r9
0x18004d6e3: mov       r15d, dword ptr [rsp + 0x140]
0x18004d6eb: mov       r14, r8
0x18004d6ee: mov       rbp, qword ptr [rsp + 0x150]
0x18004d6f6: mov       r12, rdx
0x18004d6f9: cmp       r15d, 2
0x18004d6fd: jbe       0x18004dc8d
0x18004d703: cmp       dword ptr [rsp + 0x158], 0x4d
0x18004d70b: jne       0x18004d717
0x18004d70d: mov       rdx, r14
0x18004d710: call      0x18005a9f0
0x18004d715: jmp       0x18004d72e
0x18004d717: add       rcx, 0x3354
0x18004d71e: mov       rdx, r14
0x18004d721: mov       r8d, dword ptr [rsp + 0x158]
0x18004d729: call      0x18005ace0
0x18004d72e: cmp       r15d, 4
0x18004d732: je        0x18004da28
0x18004d738: cmp       r15d, 5
0x18004d73c: jne       0x18004dde8
0x18004d742: mov       qword ptr [rsp + 0xc0], rbx
0x18004d74a: xor       r8d, r8d
0x18004d74d: movups    xmmword ptr [rsp + 0x60], xmm6
0x18004d752: movups    xmmword ptr [rsp + 0x70], xmm7
0x18004d757: movups    xmmword ptr [rsp + 0x80], xmm8
0x18004d760: movups    xmmword ptr [rsp + 0x90], xmm9
0x18004d769: movups    xmmword ptr [rsp + 0xa0], xmm10
0x18004d772: movups    xmmword ptr [rsp + 0xb0], xmm11
0x18004d77b: movups    xmmword ptr [rsp + 0x20], xmm12
0x18004d781: movups    xmmword ptr [rsp + 0x30], xmm13
0x18004d787: movups    xmmword ptr [rsp + 0x40], xmm14
0x18004d78d: movups    xmmword ptr [rsp + 0x50], xmm15
0x18004d793: mov       qword ptr [rsp + 0xc8], rsi
0x18004d79b: mov       qword ptr [rsp + 0xd0], rdi
0x18004d7a3: mov       r9d, dword ptr [rsp + 0x158]
0x18004d7ab: mov       ebx, dword ptr [rsp + 0x148]
0x18004d7b2: test      rbp, rbp
0x18004d7b5: je        0x18004d7c0
0x18004d7b7: mov       eax, dword ptr [rbp]
0x18004d7ba: add       rbp, 4
0x18004d7be: jmp       0x18004d7c3
0x18004d7c0: mov       eax, r9d
0x18004d7c3: xor       esi, esi
0x18004d7c5: test      ebx, ebx
0x18004d7c7: jbe       0x18004d9b9
0x18004d7cd: mov       dword ptr [rsp + 0xe8], eax
0x18004d7d4: lea       edi, [rax - 1]
0x18004d7d7: mov       qword ptr [rsp + 0xd8], rbp
0x18004d7df: xor       ecx, ecx
0x18004d7e1: movss     xmm0, dword ptr [r13]
0x18004d7e7: mov       eax, r8d
0x18004d7ea: movss     xmm1, dword ptr [r13 + 4]
0x18004d7f0: movaps    xmm9, xmm0
0x18004d7f4: movss     xmm2, dword ptr [r13 + 8]
0x18004d7fa: movaps    xmm8, xmm1
0x18004d7fe: movss     xmm3, dword ptr [r13 + 0xc]
0x18004d804: movaps    xmm7, xmm2
0x18004d807: movss     xmm4, dword ptr [r13 + 0x10]
0x18004d80d: movaps    xmm6, xmm3
0x18004d810: movaps    xmm5, xmm4
0x18004d813: shufps    xmm9, xmm9, 0
0x18004d818: shufps    xmm8, xmm8, 0
0x18004d81d: shufps    xmm7, xmm7, 0
0x18004d821: shufps    xmm6, xmm6, 0
0x18004d825: shufps    xmm5, xmm5, 0
0x18004d829: cmp       r8d, edi
0x18004d82c: jae       0x18004d8c0
0x18004d832: mov       r10, qword ptr [r14]
0x18004d835: mov       r11, qword ptr [r14 + 8]
0x18004d839: mov       r15, qword ptr [r14 + 0x10]
0x18004d83d: mov       rdx, qword ptr [r14 + 0x18]
0x18004d841: mov       r9, qword ptr [r14 + 0x20]
0x18004d845: mov       rbp, qword ptr [r12 + rsi*8]
0x18004d849: mov       qword ptr [rsp + 0xe0], r13
0x18004d851: nop       dword ptr [rax + rax]
0x18004d859: nop       dword ptr [rax]
0x18004d860: lea       r13d, [rax + rax]
0x18004d864: add       eax, 2
0x18004d867: movups    xmm10, xmmword ptr [r10 + r13*4]
0x18004d86c: movups    xmm15, xmmword ptr [rbp + r13*4]
0x18004d872: mulps     xmm10, xmm9
0x18004d876: movups    xmm11, xmmword ptr [r11 + r13*4]
0x18004d87b: movups    xmm12, xmmword ptr [r15 + r13*4]
0x18004d880: mulps     xmm11, xmm8
0x18004d884: addps     xmm15, xmm10
0x18004d888: mulps     xmm12, xmm7
0x18004d88c: movups    xmm13, xmmword ptr [rdx + r13*4]
0x18004d891: movups    xmm14, xmmword ptr [r9 + r13*4]
0x18004d896: mulps     xmm13, xmm6
0x18004d89a: addps     xmm15, xmm11
0x18004d89e: mulps     xmm14, xmm5
0x18004d8a2: addps     xmm15, xmm12
0x18004d8a6: addps     xmm15, xmm13
0x18004d8aa: addps     xmm15, xmm14
0x18004d8ae: movups    xmmword ptr [rbp + r13*4], xmm15
0x18004d8b4: cmp       eax, edi
0x18004d8b6: jb        0x18004d860
0x18004d8b8: mov       r13, qword ptr [rsp + 0xe0]
0x18004d8c0: cmp       eax, dword ptr [rsp + 0xe8]
0x18004d8c7: jae       0x18004d992
0x18004d8cd: mov       r9, qword ptr [r14]
0x18004d8d0: lea       ebp, [rax + rax]
0x18004d8d3: mov       r10, qword ptr [r14 + 8]
0x18004d8d7: lea       eax, [rax + rax + 1]
0x18004d8db: mov       rsi, qword ptr [r12 + rsi*8]
0x18004d8df: movss     xmm9, dword ptr [r9 + rbp*4]
0x18004d8e5: movss     xmm10, dword ptr [r9 + rax*4]
0x18004d8eb: mulss     xmm9, xmm0
0x18004d8f0: mulss     xmm10, xmm0
0x18004d8f5: addss     xmm9, dword ptr [rsi + rbp*4]
0x18004d8fb: addss     xmm10, dword ptr [rsi + rax*4]
0x18004d901: movss     xmm0, dword ptr [r10 + rbp*4]
0x18004d907: movss     xmm5, dword ptr [r10 + rax*4]
0x18004d90d: mulss     xmm0, xmm1
0x18004d911: mulss     xmm5, xmm1
0x18004d915: addss     xmm9, xmm0
0x18004d91a: addss     xmm10, xmm5
0x18004d91f: mov       r11, qword ptr [r14 + 0x10]
0x18004d923: mov       r15, qword ptr [r14 + 0x18]
0x18004d927: mov       r9, qword ptr [r14 + 0x20]
0x18004d92b: movss     xmm1, dword ptr [r11 + rbp*4]
0x18004d931: movss     xmm6, dword ptr [r11 + rax*4]
0x18004d937: mulss     xmm1, xmm2
0x18004d93b: mulss     xmm6, xmm2
0x18004d93f: addss     xmm9, xmm1
0x18004d944: addss     xmm10, xmm6
0x18004d949: movss     xmm2, dword ptr [r15 + rbp*4]
0x18004d94f: movss     xmm7, dword ptr [r15 + rax*4]
0x18004d955: mulss     xmm2, xmm3
0x18004d959: mulss     xmm7, xmm3
0x18004d95d: addss     xmm9, xmm2
0x18004d962: addss     xmm10, xmm7
0x18004d967: movss     xmm3, dword ptr [r9 + rbp*4]
0x18004d96d: movss     xmm8, dword ptr [r9 + rax*4]
0x18004d973: mulss     xmm3, xmm4
0x18004d977: mulss     xmm8, xmm4
0x18004d97c: addss     xmm9, xmm3
0x18004d981: addss     xmm10, xmm8
0x18004d986: movss     dword ptr [rsi + rbp*4], xmm9
0x18004d98c: movss     dword ptr [rsi + rax*4], xmm10
0x18004d992: inc       ecx
0x18004d994: add       r13, 0x14
0x18004d998: mov       esi, ecx
0x18004d99a: cmp       ecx, ebx
0x18004d99c: jb        0x18004d7e1
0x18004d9a2: mov       eax, dword ptr [rsp + 0xe8]
0x18004d9a9: mov       rbp, qword ptr [rsp + 0xd8]
0x18004d9b1: mov       r9d, dword ptr [rsp + 0x158]
0x18004d9b9: mov       r8d, eax
0x18004d9bc: cmp       eax, r9d
0x18004d9bf: jb        0x18004d7b2
0x18004d9c5: movups    xmm6, xmmword ptr [rsp + 0x60]
0x18004d9ca: movups    xmm7, xmmword ptr [rsp + 0x70]
0x18004d9cf: movups    xmm8, xmmword ptr [rsp + 0x80]
0x18004d9d8: movups    xmm9, xmmword ptr [rsp + 0x90]
0x18004d9e1: movups    xmm10, xmmword ptr [rsp + 0xa0]
0x18004d9ea: movups    xmm11, xmmword ptr [rsp + 0xb0]
0x18004d9f3: movups    xmm12, xmmword ptr [rsp + 0x20]
0x18004d9f9: movups    xmm13, xmmword ptr [rsp + 0x30]
0x18004d9ff: movups    xmm14, xmmword ptr [rsp + 0x40]
0x18004da05: movups    xmm15, xmmword ptr [rsp + 0x50]
0x18004da0b: mov       rbx, qword ptr [rsp + 0xc0]
0x18004da13: mov       rsi, qword ptr [rsp + 0xc8]
0x18004da1b: mov       rdi, qword ptr [rsp + 0xd0]
0x18004da23: jmp       0x18004dde8
0x18004da28: mov       qword ptr [rsp + 0xd0], rdi
0x18004da30: xor       r9d, r9d
0x18004da33: movups    xmmword ptr [rsp + 0x90], xmm9
0x18004da3c: movups    xmmword ptr [rsp + 0xa0], xmm10
0x18004da45: movups    xmmword ptr [rsp + 0xb0], xmm11
0x18004da4e: movups    xmmword ptr [rsp + 0x20], xmm12
0x18004da54: movups    xmmword ptr [rsp + 0x30], xmm13
0x18004da5a: movups    xmmword ptr [rsp + 0x40], xmm14
0x18004da60: movups    xmmword ptr [rsp + 0x50], xmm15
0x18004da66: mov       qword ptr [rsp + 0xc0], rbx
0x18004da6e: mov       qword ptr [rsp + 0xc8], rsi
0x18004da76: mov       ecx, dword ptr [rsp + 0x158]
0x18004da7d: mov       edi, dword ptr [rsp + 0x148]
0x18004da84: test      rbp, rbp
0x18004da87: je        0x18004da92
0x18004da89: mov       esi, dword ptr [rbp]
0x18004da8c: add       rbp, 4
0x18004da90: jmp       0x18004da94
0x18004da92: mov       esi, ecx
0x18004da94: xor       eax, eax
0x18004da96: test      edi, edi
0x18004da98: jbe       0x18004dc32
0x18004da9e: mov       qword ptr [rsp + 0xd8], rbp
0x18004daa6: lea       r8d, [rsi - 1]
0x18004daaa: xor       ebx, ebx
0x18004daac: movss     xmm0, dword ptr [r13]
0x18004dab2: mov       ecx, r9d
0x18004dab5: movss     xmm2, dword ptr [r13 + 4]
0x18004dabb: movaps    xmm10, xmm0
0x18004dabf: movss     xmm3, dword ptr [r13 + 8]
0x18004dac5: movaps    xmm9, xmm2
0x18004dac9: movss     xmm1, dword ptr [r13 + 0xc]
0x18004dacf: movaps    xmm5, xmm3
0x18004dad2: movaps    xmm4, xmm1
0x18004dad5: shufps    xmm10, xmm10, 0
0x18004dada: shufps    xmm9, xmm9, 0
0x18004dadf: shufps    xmm5, xmm5, 0
0x18004dae3: shufps    xmm4, xmm4, 0
0x18004dae7: cmp       r9d, r8d
0x18004daea: jae       0x18004db64
0x18004daec: mov       r11, qword ptr [r14]
0x18004daef: mov       r15, qword ptr [r14 + 8]
0x18004daf3: mov       rdx, qword ptr [r14 + 0x10]
0x18004daf7: mov       r10, qword ptr [r14 + 0x18]
0x18004dafb: mov       rbp, qword ptr [r12 + rax*8]
0x18004daff: mov       qword ptr [rsp + 0xe0], r13
0x18004db07: nop       word ptr [rax + rax]
0x18004db10: lea       r13d, [rcx + rcx]
0x18004db14: add       ecx, 2
0x18004db17: movups    xmm11, xmmword ptr [r11 + r13*4]
0x18004db1c: movups    xmm15, xmmword ptr [rbp + r13*4]
0x18004db22: mulps     xmm11, xmm10
0x18004db26: movups    xmm12, xmmword ptr [r15 + r13*4]
0x18004db2b: movups    xmm13, xmmword ptr [rdx + r13*4]
0x18004db30: mulps     xmm12, xmm9
0x18004db34: addps     xmm15, xmm11
0x18004db38: mulps     xmm13, xmm5
0x18004db3c: movups    xmm14, xmmword ptr [r10 + r13*4]
0x18004db41: addps     xmm15, xmm12
0x18004db45: mulps     xmm14, xmm4
0x18004db49: addps     xmm15, xmm13
0x18004db4d: addps     xmm15, xmm14
0x18004db51: movups    xmmword ptr [rbp + r13*4], xmm15
0x18004db57: cmp       ecx, r8d
0x18004db5a: jb        0x18004db10
0x18004db5c: mov       r13, qword ptr [rsp + 0xe0]
0x18004db64: cmp       ecx, esi
0x18004db66: jae       0x18004dc13
0x18004db6c: mov       r10, qword ptr [r14]
0x18004db6f: mov       rbp, qword ptr [r12 + rax*8]
0x18004db73: lea       eax, [rcx + rcx]
0x18004db76: mov       r11, qword ptr [r14 + 8]
0x18004db7a: lea       ecx, [rcx + rcx + 1]
0x18004db7e: movss     xmm10, dword ptr [r10 + rax*4]
0x18004db84: movss     xmm11, dword ptr [r10 + rcx*4]
0x18004db8a: mulss     xmm10, xmm0
0x18004db8f: mulss     xmm11, xmm0
0x18004db94: addss     xmm10, dword ptr [rbp + rax*4]
0x18004db9b: addss     xmm11, dword ptr [rbp + rcx*4]
0x18004dba2: movss     xmm0, dword ptr [r11 + rax*4]
0x18004dba8: movss     xmm4, dword ptr [r11 + rcx*4]
0x18004dbae: mulss     xmm0, xmm2
0x18004dbb2: mulss     xmm4, xmm2
0x18004dbb6: addss     xmm10, xmm0
0x18004dbbb: addss     xmm11, xmm4
0x18004dbc0: mov       r15, qword ptr [r14 + 0x10]
0x18004dbc4: mov       r10, qword ptr [r14 + 0x18]
0x18004dbc8: movss     xmm2, dword ptr [r15 + rax*4]
0x18004dbce: movss     xmm5, dword ptr [r15 + rcx*4]
0x18004dbd4: mulss     xmm2, xmm3
0x18004dbd8: mulss     xmm5, xmm3
0x18004dbdc: addss     xmm10, xmm2
0x18004dbe1: addss     xmm11, xmm5
0x18004dbe6: movss     xmm3, dword ptr [r10 + rax*4]
0x18004dbec: movss     xmm9, dword ptr [r10 + rcx*4]
0x18004dbf2: mulss     xmm3, xmm1
0x18004dbf6: mulss     xmm9, xmm1
0x18004dbfb: addss     xmm10, xmm3
0x18004dc00: addss     xmm11, xmm9
0x18004dc05: movss     dword ptr [rbp + rax*4], xmm10
0x18004dc0c: movss     dword ptr [rbp + rcx*4], xmm11
0x18004dc13: inc       ebx
0x18004dc15: add       r13, 0x10
0x18004dc19: mov       eax, ebx
0x18004dc1b: cmp       ebx, edi
0x18004dc1d: jb        0x18004daac
0x18004dc23: mov       ecx, dword ptr [rsp + 0x158]
0x18004dc2a: mov       rbp, qword ptr [rsp + 0xd8]
0x18004dc32: mov       r9d, esi
0x18004dc35: cmp       esi, ecx
0x18004dc37: jb        0x18004da84
0x18004dc3d: movups    xmm9, xmmword ptr [rsp + 0x90]
0x18004dc46: movups    xmm10, xmmword ptr [rsp + 0xa0]
0x18004dc4f: movups    xmm11, xmmword ptr [rsp + 0xb0]
0x18004dc58: movups    xmm12, xmmword ptr [rsp + 0x20]
0x18004dc5e: movups    xmm13, xmmword ptr [rsp + 0x30]
0x18004dc64: movups    xmm14, xmmword ptr [rsp + 0x40]
0x18004dc6a: movups    xmm15, xmmword ptr [rsp + 0x50]
0x18004dc70: mov       rbx, qword ptr [rsp + 0xc0]
0x18004dc78: mov       rsi, qword ptr [rsp + 0xc8]
0x18004dc80: mov       rdi, qword ptr [rsp + 0xd0]
0x18004dc88: jmp       0x18004dde8
0x18004dc8d: jne       0x18004dde8
0x18004dc93: mov       qword ptr [rsp + 0xc0], rbx
0x18004dc9b: xor       r9d, r9d
0x18004dc9e: mov       qword ptr [rsp + 0xd0], rdi
0x18004dca6: movups    xmmword ptr [rsp + 0x50], xmm15
0x18004dcac: mov       qword ptr [rsp + 0xc8], rsi
0x18004dcb4: mov       edi, dword ptr [rsp + 0x158]
0x18004dcbb: mov       ebx, dword ptr [rsp + 0x148]
0x18004dcc2: test      rbp, rbp
0x18004dcc5: je        0x18004dcd0
0x18004dcc7: mov       esi, dword ptr [rbp]
0x18004dcca: add       rbp, 4
0x18004dcce: jmp       0x18004dcd2
0x18004dcd0: mov       esi, edi
0x18004dcd2: xor       r15d, r15d
0x18004dcd5: test      ebx, ebx
0x18004dcd7: jbe       0x18004ddbf
0x18004dcdd: mov       qword ptr [rsp + 0xd8], rbp
0x18004dce5: lea       r8d, [rsi - 1]
0x18004dce9: xor       ecx, ecx
0x18004dceb: movss     xmm0, dword ptr [r13]
0x18004dcf1: mov       eax, r9d
0x18004dcf4: movss     xmm1, dword ptr [r13 + 4]
0x18004dcfa: movaps    xmm3, xmm0
0x18004dcfd: movaps    xmm2, xmm1
0x18004dd00: shufps    xmm3, xmm3, 0
0x18004dd04: shufps    xmm2, xmm2, 0
0x18004dd08: cmp       r9d, r8d
0x18004dd0b: jae       0x18004dd47
0x18004dd0d: mov       r11, qword ptr [r14]
0x18004dd10: mov       r10, qword ptr [r14 + 8]
0x18004dd14: mov       rbp, qword ptr [r12 + r15*8]
0x18004dd18: lea       edx, [rax + rax]
0x18004dd1b: add       eax, 2
0x18004dd1e: movups    xmm4, xmmword ptr [r11 + rdx*4]
0x18004dd23: movups    xmm15, xmmword ptr [rbp + rdx*4]
0x18004dd29: mulps     xmm4, xmm3
0x18004dd2c: movups    xmm5, xmmword ptr [r10 + rdx*4]
0x18004dd31: addps     xmm15, xmm4
0x18004dd35: mulps     xmm5, xmm2
0x18004dd38: addps     xmm15, xmm5
0x18004dd3c: movups    xmmword ptr [rbp + rdx*4], xmm15
0x18004dd42: cmp       eax, r8d
0x18004dd45: jb        0x18004dd18
0x18004dd47: cmp       eax, esi
0x18004dd49: jae       0x18004dda5
0x18004dd4b: mov       r11, qword ptr [r14]
0x18004dd4e: lea       ebp, [rax + rax]
0x18004dd51: mov       r10, qword ptr [r12 + r15*8]
0x18004dd55: lea       eax, [rax + rax + 1]
0x18004dd59: mov       r15, qword ptr [r14 + 8]
0x18004dd5d: movss     xmm3, dword ptr [r11 + rbp*4]
0x18004dd63: movss     xmm4, dword ptr [r11 + rax*4]
0x18004dd69: mulss     xmm3, xmm0
0x18004dd6d: mulss     xmm4, xmm0
0x18004dd71: addss     xmm3, dword ptr [r10 + rbp*4]
0x18004dd77: addss     xmm4, dword ptr [r10 + rax*4]
0x18004dd7d: movss     xmm0, dword ptr [r15 + rbp*4]
0x18004dd83: movss     xmm2, dword ptr [r15 + rax*4]
0x18004dd89: mulss     xmm0, xmm1
0x18004dd8d: mulss     xmm2, xmm1
0x18004dd91: addss     xmm3, xmm0
0x18004dd95: addss     xmm4, xmm2
0x18004dd99: movss     dword ptr [r10 + rbp*4], xmm3
0x18004dd9f: movss     dword ptr [r10 + rax*4], xmm4
0x18004dda5: inc       ecx
0x18004dda7: add       r13, 8
0x18004ddab: mov       r15d, ecx
0x18004ddae: cmp       ecx, ebx
0x18004ddb0: jb        0x18004dceb
0x18004ddb6: nop       
0x18004ddb7: mov       rbp, qword ptr [rsp + 0xd8]
0x18004ddbf: mov       r9d, esi
0x18004ddc2: cmp       esi, edi
0x18004ddc4: jb        0x18004dcc2
0x18004ddca: movups    xmm15, xmmword ptr [rsp + 0x50]
0x18004ddd0: mov       rbx, qword ptr [rsp + 0xc0]
0x18004ddd8: mov       rsi, qword ptr [rsp + 0xc8]
0x18004dde0: mov       rdi, qword ptr [rsp + 0xd0]
0x18004dde8: add       rsp, 0xf0
0x18004ddef: pop       rbp
0x18004ddf0: pop       r15
0x18004ddf2: pop       r14
0x18004ddf4: pop       r13
0x18004ddf6: pop       r12
0x18004ddf8: ret       
