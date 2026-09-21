            0x18005b150      0f28e9         movaps xmm5, xmm1
            0x18005b153      0f572d46d2..   xorps xmm5, xmmword [0x1803383a0]
            0x18005b15a      0f28e0         movaps xmm4, xmm0
            0x18005b15d      0f57053cd2..   xorps xmm0, xmmword [0x1803383a0]
            0x18005b164      f30f5fe9       maxss xmm5, xmm1
            0x18005b168      f30f5fc4       maxss xmm0, xmm4
            0x18005b16c      660fefdb       pxor xmm3, xmm3
            0x18005b170      f30f101538..   movss xmm2, dword [0x1803383b0] ; [0x1803383b0:4]=0x3e2cb000
            0x18005b178      0f2fe8         comiss xmm5, xmm0
        ┌─< 0x18005b17b      7621           jbe 0x18005b19e
        │   0x18005b17d      f30f5ec5       divss xmm0, xmm5
        │   0x18005b181      0f28e8         movaps xmm5, xmm0
        │   0x18005b184      f30f59e8       mulss xmm5, xmm0
        │   0x18005b188      f30f59c2       mulss xmm0, xmm2
        │   0x18005b18c      f30f101520..   movss xmm2, dword [0x1803383b4] ; [0x1803383b4:4]=0xbd30e000
        │   0x18005b194      f30f59ea       mulss xmm5, xmm2
        │   0x18005b198      f30f58c5       addss xmm0, xmm5
       ┌──< 0x18005b19c      eb2b           jmp 0x18005b1c9
       │└─> 0x18005b19e      f30f5ee8       divss xmm5, xmm0
       │    0x18005b1a2      0f28c5         movaps xmm0, xmm5
       │    0x18005b1a5      f30f59c5       mulss xmm0, xmm5
       │    0x18005b1a9      f30f59ea       mulss xmm5, xmm2
       │    0x18005b1ad      f30f1015ff..   movss xmm2, dword [0x1803383b4] ; [0x1803383b4:4]=0xbd30e000
       │    0x18005b1b5      f30f59d0       mulss xmm2, xmm0
       │    0x18005b1b9      f30f1005f7..   movss xmm0, dword [0x1803383b8] ; [0x1803383b8:4]=0x3e800000
       │    0x18005b1c1      f30f58ea       addss xmm5, xmm2
       │    0x18005b1c5      f30f5cc5       subss xmm0, xmm5
       └──> 0x18005b1c9      0f2fd9         comiss xmm3, xmm1
            0x18005b1cc      f30f1015e8..   movss xmm2, dword [0x1803383bc] ; [0x1803383bc:4]=0x3f000000
            0x18005b1d4      f30f5cd0       subss xmm2, xmm0
        ┌─< 0x18005b1d8      7703           ja 0x18005b1dd
        │   0x18005b1da      0f28d0         movaps xmm2, xmm0
        └─> 0x18005b1dd      0f2fdc         comiss xmm3, xmm4
            0x18005b1e0      0f28c2         movaps xmm0, xmm2
            0x18005b1e3      0f5705b6d1..   xorps xmm0, xmmword [0x1803383a0]
        ┌─< 0x18005b1ea      7703           ja 0x18005b1ef
        │   0x18005b1ec      0f28c2         movaps xmm0, xmm2
        └─> 0x18005b1ef      c3             ret
            0x18005b1f0      57             push rdi
            0x18005b1f1      4154           push r12
            0x18005b1f3      4155           push r13
            0x18005b1f5      4156           push r14
            0x18005b1f7      4157           push r15
            0x18005b1f9      55             push rbp
            0x18005b1fa      4883ec58       sub rsp, 0x58
            0x18005b1fe      4189ca         mov r10d, ecx
            0x18005b201      4d63d2         movsxd r10, r10d
            0x18005b204      488b8424b0..   mov rax, qword [rsp + 0xb0]
            0x18005b20c      660fefc9       pxor xmm1, xmm1
            0x18005b210      8b8c24b800..   mov ecx, dword [rsp + 0xb8]
            0x18005b217      f3420f100490   movss xmm0, dword [rax + r10*4]
            0x18005b21d      4533d2         xor r10d, r10d
            0x18005b220      85c9           test ecx, ecx
        ┌─< 0x18005b222      0f8e03020000   jle 0x18005b42b
        │   0x18005b228      f30f101590..   movss xmm2, dword [0x1803383c0] ; [0x1803383c0:4]=0x3f800000
        │   0x18005b230      4989c5         mov r13, rax
        │   0x18005b233      440f116c2440   movups xmmword [rsp + 0x40], xmm13
        │   0x18005b239      440f28e8       movaps xmm13, xmm0
        │   0x18005b23d      440f11742420   movups xmmword [rsp + 0x20], xmm14
        │   0x18005b243      440f28f2       movaps xmm14, xmm2
        │   0x18005b247      440f117c2430   movups xmmword [rsp + 0x30], xmm15
        │   0x18005b24d      440f28f9       movaps xmm15, xmm1
        │   0x18005b251      89d7           mov edi, edx
        │   0x18005b253      4189cc         mov r12d, ecx
        │   0x18005b256      4589c7         mov r15d, r8d
        │   0x18005b259      4d89ce         mov r14, r9
        │   0x18005b25c      4489d5         mov ebp, r10d
        │   0x18005b25f      4c89f1         mov rcx, r14
        │   0x18005b262      89ea           mov edx, ebp
        │   0x18005b264      e8f770fdff     call 0x180032360
        │   0x18005b269      89f9           mov ecx, edi
        │   0x18005b26b      4489fa         mov edx, r15d
        │   0x18005b26e      4189c0         mov r8d, eax
        │   0x18005b271      e8da010000     call 0x18005b450
        │   0x18005b276      410f2ec6       ucomiss xmm0, xmm14
        │   0x18005b27a      7a02           jp 0x18005b27e
        │   0x18005b27c      7408           je 0x18005b286
        │   0x18005b27e      410f2ec7       ucomiss xmm0, xmm15
        │   0x18005b282      7520           jne 0x18005b2a4
        │   0x18005b284      7a1e           jp 0x18005b2a4
        │   0x18005b286      ffc5           inc ebp
        │   0x18005b288      413bec         cmp ebp, r12d
        │   0x18005b28b      7cd2           jl 0x18005b25f
        │   0x18005b28d      440f106c2440   movups xmm13, xmmword [rsp + 0x40]
        │   0x18005b293      440f10742420   movups xmm14, xmmword [rsp + 0x20]
        │   0x18005b299      440f107c2430   movups xmm15, xmmword [rsp + 0x30]
        │   0x18005b29f      e987010000     jmp 0x18005b42b
        │   0x18005b2a4      89fa           mov edx, edi
        │   0x18005b2a6      4489e1         mov ecx, r12d
        │   0x18005b2a9      4589f8         mov r8d, r15d
        │   0x18005b2ac      4533db         xor r11d, r11d
        │   0x18005b2af      410f28c5       movaps xmm0, xmm13
        │   0x18005b2b3      410f28cf       movaps xmm1, xmm15
        │   0x18005b2b7      4533d2         xor r10d, r10d
        │   0x18005b2ba      4c89e8         mov rax, r13
        │   0x18005b2bd      4d89f1         mov r9, r14
        │   0x18005b2c0      440f28f0       movaps xmm14, xmm0
        │   0x18005b2c4      440f106c2440   movups xmm13, xmmword [rsp + 0x40]
        │   0x18005b2ca      440f28f9       movaps xmm15, xmm1
        │   0x18005b2ce      48895c2450     mov qword [rsp + 0x50], rbx
        │   0x18005b2d3      89d7           mov edi, edx
        │   0x18005b2d5      4189cc         mov r12d, ecx
        │   0x18005b2d8      4589c7         mov r15d, r8d
        │   0x18005b2db      4c89d3         mov rbx, r10
        │   0x18005b2de      4489dd         mov ebp, r11d
        │   0x18005b2e1      4c89f1         mov rcx, r14
        │   0x18005b2e4      89ea           mov edx, ebp
        │   0x18005b2e6      e87570fdff     call 0x180032360
        │   0x18005b2eb      89f9           mov ecx, edi
        │   0x18005b2ed      4489fa         mov edx, r15d
        │   0x18005b2f0      4189c0         mov r8d, eax
        │   0x18005b2f3      e858010000     call 0x18005b450
        │   0x18005b2f8      0f28d0         movaps xmm2, xmm0
        │   0x18005b2fb      ffc5           inc ebp
        │   0x18005b2fd      f30f59c0       mulss xmm0, xmm0
            0x18005b120      4189d0         mov r8d, edx
            0x18005b123      4889ca         mov rdx, rcx
            0x18005b126      33c0           xor eax, eax
            0x18005b128      8b4a18         mov ecx, dword [rdx + 0x18]
            0x18005b12b      85c9           test ecx, ecx
        ┌─< 0x18005b12d      7614           jbe 0x18005b143
        │   0x18005b12f      488b5210       mov rdx, qword [rdx + 0x10]
       ┌──> 0x18005b133      443b0482       cmp r8d, dword [rdx + rax*4]
      ┌───< 0x18005b137      7304           jae 0x18005b13d
      │╎│   0x18005b139      ffc0           inc eax
     ┌────< 0x18005b13b      eb06           jmp 0x18005b143
     │└───> 0x18005b13d      ffc0           inc eax
     │ ╎│   0x18005b13f      3bc1           cmp eax, ecx
     │ └──< 0x18005b141      72f0           jb 0x18005b133
     └──└─> 0x18005b143      ffc8           dec eax
            0x18005b145      c3             ret
            0x18005b146      0f1f00         nop dword [rax]
            0x18005b149      0f1f800000..   nop dword [rax]
            0x18005b150      0f28e9         movaps xmm5, xmm1
            0x18005b153      0f572d46d2..   xorps xmm5, xmmword [0x1803383a0]
            0x18005b15a      0f28e0         movaps xmm4, xmm0
            0x18005b15d      0f57053cd2..   xorps xmm0, xmmword [0x1803383a0]
            0x18005b164      f30f5fe9       maxss xmm5, xmm1
            0x18005b168      f30f5fc4       maxss xmm0, xmm4
            0x18005b16c      660fefdb       pxor xmm3, xmm3
            0x18005b170      f30f101538..   movss xmm2, dword [0x1803383b0] ; [0x1803383b0:4]=0x3e2cb000
            0x18005b178      0f2fe8         comiss xmm5, xmm0
        ┌─< 0x18005b17b      7621           jbe 0x18005b19e
        │   0x18005b17d      f30f5ec5       divss xmm0, xmm5
        │   0x18005b181      0f28e8         movaps xmm5, xmm0
        │   0x18005b184      f30f59e8       mulss xmm5, xmm0
        │   0x18005b188      f30f59c2       mulss xmm0, xmm2
        │   0x18005b18c      f30f101520..   movss xmm2, dword [0x1803383b4] ; [0x1803383b4:4]=0xbd30e000
        │   0x18005b194      f30f59ea       mulss xmm5, xmm2
        │   0x18005b198      f30f58c5       addss xmm0, xmm5
       ┌──< 0x18005b19c      eb2b           jmp 0x18005b1c9
       │└─> 0x18005b19e      f30f5ee8       divss xmm5, xmm0
       │    0x18005b1a2      0f28c5         movaps xmm0, xmm5
       │    0x18005b1a5      f30f59c5       mulss xmm0, xmm5
       │    0x18005b1a9      f30f59ea       mulss xmm5, xmm2
       │    0x18005b1ad      f30f1015ff..   movss xmm2, dword [0x1803383b4] ; [0x1803383b4:4]=0xbd30e000
       │    0x18005b1b5      f30f59d0       mulss xmm2, xmm0
       │    0x18005b1b9      f30f1005f7..   movss xmm0, dword [0x1803383b8] ; [0x1803383b8:4]=0x3e800000
       │    0x18005b1c1      f30f58ea       addss xmm5, xmm2
       │    0x18005b1c5      f30f5cc5       subss xmm0, xmm5
