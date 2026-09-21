0x18004e1c0: push      rbx
0x18004e1c1: push      rsi
0x18004e1c2: push      rdi
0x18004e1c3: push      r12
0x18004e1c5: push      r13
0x18004e1c7: push      r14
0x18004e1c9: sub       rsp, 0x58
0x18004e1cd: mov       r10, rdx
0x18004e1d0: movss     xmm2, dword ptr [rip + 0x2e0684]
0x18004e1d8: pxor      xmm1, xmm1
0x18004e1dc: movss     xmm0, dword ptr [rip + 0x2e067c]
0x18004e1e4: mov       r9, rcx
0x18004e1e7: xor       ecx, ecx
0x18004e1e9: mov       edx, 0xffff8000
0x18004e1ee: mov       eax, 0x7fff
0x18004e1f3: mov       r13d, edx
0x18004e1f6: movups    xmmword ptr [rsp + 0x40], xmm6
0x18004e1fb: movaps    xmm6, xmm0
0x18004e1fe: movups    xmmword ptr [rsp + 0x30], xmm7
0x18004e203: movaps    xmm7, xmm2
0x18004e206: movups    xmmword ptr [rsp + 0x20], xmm8
0x18004e20c: movaps    xmm8, xmm1
0x18004e210: mov       ebx, eax
0x18004e212: mov       rdi, rcx
0x18004e215: mov       r12, r8
0x18004e218: mov       rsi, r9
0x18004e21b: mov       r14, r10
0x18004e21e: movss     xmm0, dword ptr [r14 + rdi*4]
0x18004e224: mulss     xmm0, xmm7
0x18004e228: movss     xmm1, dword ptr [rsi + rdi*4]
0x18004e22d: ucomiss   xmm0, xmm8
0x18004e231: jne       0x18004e243
0x18004e233: jp        0x18004e243
0x18004e235: ucomiss   xmm1, xmm8
0x18004e239: jne       0x18004e243
0x18004e23b: jp        0x18004e243
0x18004e23d: movaps    xmm3, xmm8
0x18004e241: jmp       0x18004e24b
0x18004e243: call      0x18005b150
0x18004e248: movaps    xmm3, xmm0
0x18004e24b: comiss    xmm8, xmm3
0x18004e24f: movss     xmm0, dword ptr [rip + 0x2e060d]
0x18004e257: addss     xmm0, xmm3
0x18004e25b: ja        0x18004e260
0x18004e25d: movaps    xmm0, xmm3
0x18004e260: mulss     xmm0, xmm6
0x18004e264: call      0x18006a860
0x18004e269: cmp       eax, 0xffff8000
0x18004e26e: cmovl     eax, r13d
0x18004e272: cmp       eax, 0x7fff
0x18004e277: cmovge    eax, ebx
0x18004e27a: movsx     r11, ax
0x18004e27e: sar       r11d, 5
0x18004e282: mov       dword ptr [r12 + rdi*4], r11d
0x18004e286: inc       rdi
0x18004e289: cmp       rdi, 0x14
0x18004e28d: jl        0x18004e21e
0x18004e28f: movups    xmm6, xmmword ptr [rsp + 0x40]
0x18004e294: movups    xmm7, xmmword ptr [rsp + 0x30]
0x18004e299: movups    xmm8, xmmword ptr [rsp + 0x20]
0x18004e29f: add       rsp, 0x58
0x18004e2a3: pop       r14
0x18004e2a5: pop       r13
0x18004e2a7: pop       r12
0x18004e2a9: pop       rdi
0x18004e2aa: pop       rsi
0x18004e2ab: pop       rbx
