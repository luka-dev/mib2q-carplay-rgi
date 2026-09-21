0x18005a9f0: push      rbx
0x18005a9f1: push      rsi
0x18005a9f2: push      rdi
0x18005a9f3: push      r12
0x18005a9f5: push      r13
0x18005a9f7: push      r14
0x18005a9f9: push      r15
0x18005a9fb: push      rbp
0x18005a9fc: sub       rsp, 0x38
0x18005aa00: mov       rdi, rcx
0x18005aa03: mov       r8, rdx
0x18005aa06: xor       r12d, r12d
0x18005aa09: mov       qword ptr [rsp + 0x28], rdi
0x18005aa0e: xor       r10d, r10d
0x18005aa11: lea       rbp, [rip - 0x5aa18]
0x18005aa18: mov       r9d, dword ptr [rdi + 0x3350]
0x18005aa1f: lea       rbx, [rdi + 0x3170]
0x18005aa26: mov       rdx, qword ptr [r8]
0x18005aa29: mov       rsi, qword ptr [r8 + 8]
0x18005aa2d: mov       r13, qword ptr [r8 + 0x10]
0x18005aa31: lea       eax, [r9*4]
0x18005aa39: mov       rcx, qword ptr [r8 + 0x18]
0x18005aa3d: lea       r11, [rdi + rax*4]
0x18005aa41: mov       r8, qword ptr [r8 + 0x20]
0x18005aa45: mov       rax, r11
0x18005aa48: mov       qword ptr [rsp + 0x20], rdx
0x18005aa4d: movzx     edx, byte ptr [rbp + r12 + 0x338220]
0x18005aa56: movzx     edi, byte ptr [rbp + r12 + 0x338280]
0x18005aa5f: movzx     r12d, byte ptr [rbp + r12 + 0x3382e0]
0x18005aa68: shl       r12d, 2
0x18005aa6c: lea       r14d, [rdx*4]
0x18005aa74: movss     xmm0, dword ptr [r11 + r14*4]
0x18005aa7a: lea       edx, [rdx*4 + 2]
0x18005aa81: movss     xmm1, dword ptr [r11 + rdx*4]
0x18005aa87: lea       r15d, [rdi*4 + 1]
0x18005aa8f: lea       edi, [rdi*4 + 3]
0x18005aa96: addss     xmm0, dword ptr [r11 + r15*4]
0x18005aa9c: addss     xmm1, dword ptr [r11 + rdi*4]
0x18005aaa2: lea       r11, [r11 + r12*4]
0x18005aaa6: cmp       rbx, r11
0x18005aaa9: lea       r14d, [r10 + r10]
0x18005aaad: movss     dword ptr [r13 + r14*4], xmm0
0x18005aab4: lea       edx, [r10 + r10 + 1]
0x18005aab9: movss     dword ptr [r13 + rdx*4], xmm1
0x18005aac0: lea       r12, [r11 - 0x3170]
0x18005aac7: cmovbe    r11, r12
0x18005aacb: inc       r10d
0x18005aace: mov       r12d, r10d
0x18005aad1: cmp       r10d, 0x4d
0x18005aad5: jb        0x18005aa4d
0x18005aadb: mov       r12, rax
0x18005aade: mov       rdx, qword ptr [rsp + 0x20]
0x18005aae3: xor       r11d, r11d
0x18005aae6: xor       r10d, r10d
0x18005aae9: lea       rdi, [rip - 0x5aaf0]
0x18005aaf0: movzx     ebp, byte ptr [rdi + r11 + 0x338340]
0x18005aaf9: movzx     r13d, byte ptr [rdi + r11 + 0x338220]
0x18005ab02: movzx     r11d, byte ptr [rdi + r11 + 0x3382e0]
0x18005ab0b: shl       r11d, 2
0x18005ab0f: lea       r14d, [rbp*4]
0x18005ab17: movss     xmm0, dword ptr [r12 + r14*4]
0x18005ab1d: lea       ebp, [rbp*4 + 2]
0x18005ab24: movss     xmm1, dword ptr [r12 + rbp*4]
0x18005ab2a: lea       r15d, [r13*4 + 1]
0x18005ab32: lea       r13d, [r13*4 + 3]
0x18005ab3a: addss     xmm0, dword ptr [r12 + r15*4]
0x18005ab40: addss     xmm1, dword ptr [r12 + r13*4]
0x18005ab46: lea       r12, [r12 + r11*4]
0x18005ab4a: cmp       rbx, r12
0x18005ab4d: lea       r14d, [r10 + r10]
0x18005ab51: movss     dword ptr [rcx + r14*4], xmm0
0x18005ab57: lea       ebp, [r10 + r10 + 1]
0x18005ab5c: movss     dword ptr [rcx + rbp*4], xmm1
0x18005ab61: lea       r11, [r12 - 0x3170]
0x18005ab69: cmovbe    r12, r11
0x18005ab6d: inc       r10d
0x18005ab70: mov       r11d, r10d
0x18005ab73: cmp       r10d, 0x4d
0x18005ab77: jb        0x18005aaf0
0x18005ab7d: mov       rdi, qword ptr [rsp + 0x28]
0x18005ab82: xor       r10d, r10d
0x18005ab85: xor       r13d, r13d
0x18005ab88: lea       r11, [rip - 0x5ab8f]
0x18005ab8f: movzx     ecx, byte ptr [r11 + r10 + 0x338280]
0x18005ab98: movzx     r12d, byte ptr [r11 + r10 + 0x338340]
0x18005aba1: movzx     r10d, byte ptr [r11 + r10 + 0x3382e0]
0x18005abaa: shl       r10d, 2
0x18005abae: lea       r14d, [rcx*4]
0x18005abb6: movss     xmm0, dword ptr [rax + r14*4]
0x18005abbc: lea       ecx, [rcx*4 + 2]
0x18005abc3: movss     xmm1, dword ptr [rax + rcx*4]
0x18005abc8: lea       r15d, [r12*4 + 1]
0x18005abd0: lea       r12d, [r12*4 + 3]
0x18005abd8: addss     xmm0, dword ptr [rax + r15*4]
0x18005abde: addss     xmm1, dword ptr [rax + r12*4]
0x18005abe4: lea       rax, [rax + r10*4]
0x18005abe8: cmp       rbx, rax
0x18005abeb: lea       r14d, [r13 + r13]
0x18005abf0: movss     dword ptr [r8 + r14*4], xmm0
0x18005abf6: lea       ecx, [r13 + r13 + 1]
0x18005abfb: movss     dword ptr [r8 + rcx*4], xmm1
0x18005ac01: lea       r10, [rax - 0x3170]
0x18005ac08: cmovbe    rax, r10
0x18005ac0c: inc       r13d
0x18005ac0f: mov       r10d, r13d
0x18005ac12: cmp       r13d, 0x4d
0x18005ac16: jb        0x18005ab8f
0x18005ac1c: mov       eax, 0x316
0x18005ac21: test      r9d, r9d
0x18005ac24: lea       r11d, [r9 - 1]
0x18005ac28: cmova     eax, r11d
0x18005ac2c: xor       r8d, r8d
0x18005ac2f: lea       r11, [rdi + 0x1e0]
0x18005ac36: mov       dword ptr [rdi + 0x3350], eax
0x18005ac3c: lea       r9, [rip - 0x5ac43]
0x18005ac43: shl       eax, 2
0x18005ac46: lea       r10, [rdi + rax*4]
0x18005ac4a: lea       ecx, [r8 + r8]
0x18005ac4e: mov       eax, dword ptr [rdx + rcx*4]
0x18005ac51: mov       dword ptr [r10], eax
0x18005ac54: lea       eax, [r8 + r8 + 1]
0x18005ac59: mov       r12d, dword ptr [rsi + rcx*4]
0x18005ac5d: mov       dword ptr [r10 + 4], r12d
0x18005ac61: mov       r13d, dword ptr [rdx + rax*4]
0x18005ac65: mov       dword ptr [r10 + 8], r13d
0x18005ac69: movzx     edi, byte ptr [r9 + r8 + 0x3382e0]
0x18005ac72: shl       edi, 2
0x18005ac75: mov       r12d, dword ptr [rsi + rax*4]
0x18005ac79: mov       dword ptr [r10 + 0xc], r12d
0x18005ac7d: cmp       r10, r11
0x18005ac80: jae       0x18005acac
0x18005ac82: mov       r12d, dword ptr [rdx + rcx*4]
0x18005ac86: mov       dword ptr [r10 + 0x3170], r12d
0x18005ac8d: mov       ecx, dword ptr [rsi + rcx*4]
0x18005ac90: mov       dword ptr [r10 + 0x3174], ecx
0x18005ac97: mov       r13d, dword ptr [rdx + rax*4]
0x18005ac9b: mov       dword ptr [r10 + 0x3178], r13d
0x18005aca2: mov       eax, dword ptr [rsi + rax*4]
0x18005aca5: mov       dword ptr [r10 + 0x317c], eax
0x18005acac: inc       r8d
0x18005acaf: lea       r10, [r10 + rdi*4]
0x18005acb3: cmp       rbx, r10
0x18005acb6: lea       rax, [r10 - 0x3170]
0x18005acbd: cmovbe    r10, rax
0x18005acc1: cmp       r8d, 0x4d
0x18005acc5: jb        0x18005ac4a
0x18005acc7: add       rsp, 0x38
0x18005accb: pop       rbp
0x18005accc: pop       r15
0x18005acce: pop       r14
0x18005acd0: pop       r13
0x18005acd2: pop       r12
0x18005acd4: pop       rdi
0x18005acd5: pop       rsi
0x18005acd6: pop       rbx
0x18005acd7: ret       
