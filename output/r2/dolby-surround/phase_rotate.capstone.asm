0x18004eb70: push      r12
0x18004eb72: sub       rsp, 0x40
0x18004eb76: mov       r10d, edx
0x18004eb79: xor       r11d, r11d
0x18004eb7c: test      r10d, r10d
0x18004eb7f: jbe       0x18004ebc2
0x18004eb81: mov       edx, r9d
0x18004eb84: xor       eax, eax
0x18004eb86: shr       edx, 1
0x18004eb88: mov       qword ptr [rsp + 0x30], rbx
0x18004eb8d: mov       qword ptr [rsp + 0x28], rsi
0x18004eb92: mov       qword ptr [rsp + 0x20], rdi
0x18004eb97: mov       ebx, dword ptr [r8 + r11*4]
0x18004eb9b: cmp       ebx, 1
0x18004eb9e: je        0x18004ec70
0x18004eba4: cmp       ebx, -1
0x18004eba7: je        0x18004ebc9
0x18004eba9: inc       eax
0x18004ebab: mov       r11d, eax
0x18004ebae: cmp       eax, r10d
0x18004ebb1: jb        0x18004eb97
0x18004ebb3: mov       rbx, qword ptr [rsp + 0x30]
0x18004ebb8: mov       rsi, qword ptr [rsp + 0x28]
0x18004ebbd: mov       rdi, qword ptr [rsp + 0x20]
0x18004ebc2: add       rsp, 0x40
0x18004ebc6: pop       r12
0x18004ebc8: ret       
0x18004ebc9: mov       rsi, qword ptr [rcx + r11*8]
0x18004ebcd: test      r9d, r9d
0x18004ebd0: jbe       0x18004eba9
0x18004ebd2: mov       edi, 1
0x18004ebd7: xor       ebx, ebx
0x18004ebd9: test      edx, edx
0x18004ebdb: je        0x18004ec3e
0x18004ebdd: nop       dword ptr [rax]
0x18004ebe0: lea       r11d, [rbx*4]
0x18004ebe8: movss     xmm0, dword ptr [rsi + r11*4]
0x18004ebee: lea       r12d, [rbx*4 + 1]
0x18004ebf6: mov       edi, dword ptr [rsi + r12*4]
0x18004ebfa: xorps     xmm0, xmmword ptr [rip + 0x2dfccf]
0x18004ec01: mov       dword ptr [rsi + r11*4], edi
0x18004ec05: lea       edi, [rbx*4 + 2]
0x18004ec0c: movss     dword ptr [rsi + r12*4], xmm0
0x18004ec12: lea       r11d, [rbx*4 + 3]
0x18004ec1a: movss     xmm1, dword ptr [rsi + rdi*4]
0x18004ec1f: inc       ebx
0x18004ec21: mov       r12d, dword ptr [rsi + r11*4]
0x18004ec25: xorps     xmm1, xmmword ptr [rip + 0x2dfca4]
0x18004ec2c: mov       dword ptr [rsi + rdi*4], r12d
0x18004ec30: movss     dword ptr [rsi + r11*4], xmm1
0x18004ec36: cmp       ebx, edx
0x18004ec38: jb        0x18004ebe0
0x18004ec3a: lea       edi, [rbx + rbx + 1]
0x18004ec3e: lea       ebx, [rdi - 1]
0x18004ec41: cmp       ebx, r9d
0x18004ec44: jae       0x18004eba9
0x18004ec4a: lea       ebx, [rdi + rdi - 2]
0x18004ec4e: movss     xmm0, dword ptr [rsi + rbx*4]
0x18004ec53: lea       edi, [rdi + rdi - 1]
0x18004ec57: mov       r11d, dword ptr [rsi + rdi*4]
0x18004ec5b: xorps     xmm0, xmmword ptr [rip + 0x2dfc6e]
0x18004ec62: mov       dword ptr [rsi + rbx*4], r11d
0x18004ec66: movss     dword ptr [rsi + rdi*4], xmm0
0x18004ec6b: jmp       0x18004eba9
0x18004ec70: mov       rsi, qword ptr [rcx + r11*8]
0x18004ec74: test      r9d, r9d
0x18004ec77: jbe       0x18004eba9
0x18004ec7d: mov       edi, 1
0x18004ec82: xor       ebx, ebx
0x18004ec84: test      edx, edx
0x18004ec86: je        0x18004ecee
0x18004ec88: nop       dword ptr [rax + rax]
0x18004ec90: lea       r11d, [rbx*4 + 1]
0x18004ec98: movss     xmm0, dword ptr [rsi + r11*4]
0x18004ec9e: lea       r12d, [rbx*4]
0x18004eca6: mov       edi, dword ptr [rsi + r12*4]
0x18004ecaa: xorps     xmm0, xmmword ptr [rip + 0x2dfc1f]
0x18004ecb1: mov       dword ptr [rsi + r11*4], edi
0x18004ecb5: lea       edi, [rbx*4 + 3]
0x18004ecbc: movss     dword ptr [rsi + r12*4], xmm0
0x18004ecc2: lea       r11d, [rbx*4 + 2]
0x18004ecca: movss     xmm1, dword ptr [rsi + rdi*4]
0x18004eccf: inc       ebx
0x18004ecd1: mov       r12d, dword ptr [rsi + r11*4]
0x18004ecd5: xorps     xmm1, xmmword ptr [rip + 0x2dfbf4]
0x18004ecdc: mov       dword ptr [rsi + rdi*4], r12d
0x18004ece0: movss     dword ptr [rsi + r11*4], xmm1
0x18004ece6: cmp       ebx, edx
0x18004ece8: jb        0x18004ec90
0x18004ecea: lea       edi, [rbx + rbx + 1]
0x18004ecee: lea       ebx, [rdi - 1]
0x18004ecf1: cmp       ebx, r9d
0x18004ecf4: jae       0x18004eba9
0x18004ecfa: lea       ebx, [rdi + rdi - 1]
0x18004ecfe: movss     xmm0, dword ptr [rsi + rbx*4]
0x18004ed03: lea       edi, [rdi + rdi - 2]
0x18004ed07: mov       r11d, dword ptr [rsi + rdi*4]
0x18004ed0b: xorps     xmm0, xmmword ptr [rip + 0x2dfbbe]
0x18004ed12: mov       dword ptr [rsi + rbx*4], r11d
0x18004ed16: movss     dword ptr [rsi + rdi*4], xmm0
0x18004ed1b: jmp       0x18004eba9
0x18004ed20: mov       eax, 0x7f
0x18004ed25: ret       
