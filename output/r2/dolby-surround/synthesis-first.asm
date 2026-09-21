            0x180031860      53             push rbx
            0x180031861      56             push rsi
            0x180031862      57             push rdi
            0x180031863      4154           push r12
            0x180031865      4155           push r13
            0x180031867      4156           push r14
            0x180031869      4157           push r15
            0x18003186b      55             push rbp
            0x18003186c      4883ec78       sub rsp, 0x78
            0x180031870      4889cb         mov rbx, rcx
            0x180031873      488b8c24f8..   mov rcx, qword [rsp + 0xf8]
            0x18003187b      4c89cf         mov rdi, r9
            0x18003187e      488b842400..   mov rax, qword [rsp + 0x100]
            0x180031886      4c89c6         mov rsi, r8
            0x180031889      4889442460     mov qword [rsp + 0x60], rax
            0x18003188e      4889d5         mov rbp, rdx
            0x180031891      4c8b942408..   mov r10, qword [rsp + 0x108]
            0x180031899      4c8d592f       lea r11, [rcx + 0x2f]
            0x18003189d      f30f6f0deb..   movdqu xmm1, xmmword [0x1802f5290] ; [0x1802f5290:16]=-1
            0x1800318a5      488d81ce02..   lea rax, [rcx + 0x2ce]
            0x1800318ac      4c89542468     mov qword [rsp + 0x68], r10
            0x1800318b1      4c8d7107       lea r14, [rcx + 7]
            0x1800318b5      4983e6f8       and r14, 0xfffffffffffffff8
            0x1800318b9      4c8d91ca0a..   lea r10, [rcx + 0xaca]
            0x1800318c0      66490f6edb     movq xmm3, r11
            0x1800318c5      4983e2e0       and r10, 0xffffffffffffffe0
            0x1800318c9      66480f6ec0     movq xmm0, rax
            0x1800318ce      4533ff         xor r15d, r15d
            0x1800318d1      660f6cd8       punpcklqdq xmm3, xmm0
            0x1800318d5      f30f6f05a3..   movdqu xmm0, xmmword [0x1802f5280] ; [0x1802f5280:16]=-1
            0x1800318dd      660f6fd0       movdqa xmm2, xmm0
            0x1800318e1      660fd4d3       paddq xmm2, xmm3
            0x1800318e5      660fd41db3..   paddq xmm3, xmmword [0x1802f52a0]
            0x1800318ed      660fd4d8       paddq xmm3, xmm0
            0x1800318f1      660fdbd1       pand xmm2, xmm1
            0x1800318f5      660fdbd9       pand xmm3, xmm1
            0x1800318f9      4c8bac2410..   mov r13, qword [rsp + 0x110]
            0x180031901      4c8b8424e0..   mov r8, qword [rsp + 0xe0]
            0x180031909      4c8b8c24e8..   mov r9, qword [rsp + 0xe8]
            0x180031911      4c8ba424f0..   mov r12, qword [rsp + 0xf0]
            0x180031919      f3410f7f16     movdqu xmmword [r14], xmm2
            0x18003191e      f3410f7f5e10   movdqu xmmword [r14 + 0x10], xmm3
            0x180031924      4d895620       mov qword [r14 + 0x20], r10
            0x180031928      4d85ed         test r13, r13
        ┌─< 0x18003192b      0f8496000000   je 0x1800319c7
        │   0x180031931      448b9c2418..   mov r11d, dword [rsp + 0x118]
        │   0x180031939      488d91790d..   lea rdx, [rcx + 0xd79]
        │   0x180031940      4183c303       add r11d, 3
        │   0x180031944      660f6fd8       movdqa xmm3, xmm0
        │   0x180031948      41c1eb02       shr r11d, 2
        │   0x18003194c      4c8db9510d..   lea r15, [rcx + 0xd51]
        │   0x180031953      41c1e303       shl r11d, 3
        │   0x180031957      4983e7f8       and r15, 0xfffffffffffffff8
        │   0x18003195b      66480f6eea     movq xmm5, rdx
        │   0x180031960      4a8d049d1f..   lea rax, [r11*4 + 0x1f]
        │   0x180031968      488d940879..   lea rdx, [rax + rcx + 0xd79]
        │   0x180031970      4803c0         add rax, rax
        │   0x180031973      4e8d149d00..   lea r10, [r11*4]
        │   0x18003197b      49f7da         neg r10
        │   0x18003197e      66480f6ed2     movq xmm2, rdx
        │   0x180031983      66480f6ee0     movq xmm4, rax
        │   0x180031988      660f6cea       punpcklqdq xmm5, xmm2
        │   0x18003198c      4b8d049b       lea rax, [r11 + r11*4]
        │   0x180031990      660f6ce4       punpcklqdq xmm4, xmm4
        │   0x180031994      4d8d1482       lea r10, [r10 + rax*4]
        │   0x180031998      660fd4dd       paddq xmm3, xmm5
        │   0x18003199c      660fd4ec       paddq xmm5, xmm4
        │   0x1800319a0      660fd4e8       paddq xmm5, xmm0
        │   0x1800319a4      660fdbd9       pand xmm3, xmm1
        │   0x1800319a8      498d840a14..   lea rax, [r10 + rcx + 0xe14]
        │   0x1800319b0      660fdbe9       pand xmm5, xmm1
        │   0x1800319b4      4883e0e0       and rax, 0xffffffffffffffe0
        │   0x1800319b8      f3410f7f1f     movdqu xmmword [r15], xmm3
        │   0x1800319bd      f3410f7f6f10   movdqu xmmword [r15 + 0x10], xmm5
        │   0x1800319c3      49894720       mov qword [r15 + 0x20], rax
        └─> 0x1800319c7      4889e0         mov rax, rsp
            0x1800319ca      4c8d5710       lea r10, [rdi + 0x10]
            0x1800319ce      4c89e1         mov rcx, r12
            0x1800319d1      4c89f2         mov rdx, r14
            0x1800319d4      4c8d9fb000..   lea r11, [rdi + 0xb0]
            0x1800319db      4c895020       mov qword [rax + 0x20], r10
            0x1800319df      4c8d975001..   lea r10, [rdi + 0x150]
            0x1800319e6      4c895030       mov qword [rax + 0x30], r10
            0x1800319ea      448b16         mov r10d, dword [rsi]
            0x1800319ed      4c895828       mov qword [rax + 0x28], r11
            0x1800319f1      4c8d9f4002..   lea r11, [rdi + 0x240]
            0x1800319f8      4c895838       mov qword [rax + 0x38], r11
            0x1800319fc      44895040       mov dword [rax + 0x40], r10d
            0x180031a00      c740481400..   mov dword [rax + 0x48], 0x14 ; [0x14:4]=-1 ; 20
            0x180031a07      48896850       mov qword [rax + 0x50], rbp
            0x180031a0b      e800cb0100     call 0x18004e510
            0x180031a10      4d85ed         test r13, r13
        ┌─< 0x180031a13      745f           je 0x180031a74
        │   0x180031a15      4989e1         mov r9, rsp
        │   0x180031a18      4c8d974801..   lea r10, [rdi + 0x148]
        │   0x180031a1f      4c89e9         mov rcx, r13
        │   0x180031a22      488d87a800..   lea rax, [rdi + 0xa8]
        │   0x180031a29      4c89fa         mov rdx, r15
        │   0x180031a2c      4c8d9f3402..   lea r11, [rdi + 0x234]
        │   0x180031a33      4d895128       mov qword [r9 + 0x28], r10
        │   0x180031a37      448b16         mov r10d, dword [rsi]
        │   0x180031a3a      49894120       mov qword [r9 + 0x20], rax
        │   0x180031a3e      488d87d802..   lea rax, [rdi + 0x2d8]
        │   0x180031a45      4d895930       mov qword [r9 + 0x30], r11
        │   0x180031a49      4c8d9c2418..   lea r11, [rsp + 0x118]
        │   0x180031a51      49894138       mov qword [r9 + 0x38], rax
        │   0x180031a55      45895140       mov dword [r9 + 0x40], r10d
        │   0x180031a59      41c7414801..   mov dword [r9 + 0x48], 1
        │   0x180031a61      4d895950       mov qword [r9 + 0x50], r11
        │   0x180031a65      4c8b442460     mov r8, qword [rsp + 0x60]
        │   0x180031a6a      4c8b4c2468     mov r9, qword [rsp + 0x68]
        │   0x180031a6f      e89cca0100     call 0x18004e510
        └─> 0x180031a74      488d8b3868..   lea rcx, [rbx + 0x6838]
            0x180031a7b      48894c2460     mov qword [rsp + 0x60], rcx
            0x180031a80      e8fb4ffeff     call 0x180016a80
            0x180031a85      85c0           test eax, eax
        ┌─< 0x180031a87      7410           je 0x180031a99
        │   0x180031a89      4c89e2         mov rdx, r12
        │   0x180031a8c      4989e8         mov r8, rbp
        │   0x180031a8f      488b4c2460     mov rcx, qword [rsp + 0x60]
        │   0x180031a94      e8c7c40100     call 0x18004df60
        └─> 0x180031a99      4c89e1         mov rcx, r12
            0x180031a9c      41b94d000000   mov r9d, 0x4d              ; 'M' ; 77
            0x180031aa2      8b16           mov edx, dword [rsi]
            0x180031aa4      4c8b4628       mov r8, qword [rsi + 0x28]
            0x180031aa8      e8c3d00100     call 0x18004eb70
            0x180031aad      4d85ed         test r13, r13
        ┌─< 0x180031ab0      7416           je 0x180031ac8
        │   0x180031ab2      4c89e9         mov rcx, r13
        │   0x180031ab5      448b8c2418..   mov r9d, dword [rsp + 0x118]
        │   0x180031abd      8b16           mov edx, dword [rsi]
        │   0x180031abf      4c8b4628       mov r8, qword [rsi + 0x28]
        │   0x180031ac3      e8a8d00100     call 0x18004eb70
        └─> 0x180031ac8      4989e1         mov r9, rsp
            0x180031acb      4881c39001..   add rbx, 0x190             ; 400
            0x180031ad2      4533db         xor r11d, r11d
            0x180031ad5      833f00         cmp dword [rdi], 0
            0x180031ad8      8b4704         mov eax, dword [rdi + 4]
            0x180031adb      4889d9         mov rcx, rbx
            0x180031ade      448b16         mov r10d, dword [rsi]
            0x180031ae1      4c89e2         mov rdx, r12
            0x180031ae4      4d89f0         mov r8, r14
            0x180031ae7      4c0f45dd       cmovne r11, rbp
            0x180031aeb      41894120       mov dword [r9 + 0x20], eax
            0x180031aef      45895128       mov dword [r9 + 0x28], r10d
            0x180031af3      4d895930       mov qword [r9 + 0x30], r11
            0x180031af7      41c741384d..   mov dword [r9 + 0x38], 0x4d ; 'M'
                                                                       ; [0x4d:4]=-1 ; 77
            0x180031aff      4c8b4f08       mov r9, qword [rdi + 8]
            0x180031b03      e8c8bb0100     call 0x18004d6d0
            0x180031b08      4d85ed         test r13, r13
        ┌─< 0x180031b0b      7448           je 0x180031b55
        │   0x180031b0d      448b16         mov r10d, dword [rsi]
        │   0x180031b10      4989e3         mov r11, rsp
        │   0x180031b13      416bc213       imul eax, r10d, 0x13       ; 19
        │   0x180031b17      4889d9         mov rcx, rbx
        │   0x180031b1a      8b7704         mov esi, dword [rdi + 4]
        │   0x180031b1d      4c89ea         mov rdx, r13
        │   0x180031b20      0fafc6         imul eax, esi
        │   0x180031b23      4d89f8         mov r8, r15
        │   0x180031b26      4c8b4f08       mov r9, qword [rdi + 8]
        │   0x180031b2a      833f00         cmp dword [rdi], 0
        │   0x180031b2d      8bbc241801..   mov edi, dword [rsp + 0x118]
        │   0x180031b34      41897320       mov dword [r11 + 0x20], esi
        │   0x180031b38      498d2c81       lea rbp, [r9 + rax*4]
        │   0x180031b3c      4c0f45cd       cmovne r9, rbp
        │   0x180031b40      45895328       mov dword [r11 + 0x28], r10d
        │   0x180031b44      49c7433000..   mov qword [r11 + 0x30], 0
        │   0x180031b4c      41897b38       mov dword [r11 + 0x38], edi
        │   0x180031b50      e87bbb0100     call 0x18004d6d0
        └─> 0x180031b55      4883c478       add rsp, 0x78
            0x180031b59      5d             pop rbp
            0x180031b5a      415f           pop r15
            0x180031b5c      415e           pop r14
            0x180031b5e      415d           pop r13
            0x180031b60      415c           pop r12
            0x180031b62      5f             pop rdi
            0x180031b63      5e             pop rsi
            0x180031b64      5b             pop rbx
            0x180031b65      c3             ret
            0x180031b66      0f1f00         nop dword [rax]
            0x180031b69      0f1f800000..   nop dword [rax]
            0x180031b70      57             push rdi
            0x180031b71      4883ec20       sub rsp, 0x20
            0x180031b75      4889cf         mov rdi, rcx
            0x180031b78      4883c71f       add rdi, 0x1f              ; 31
            0x180031b7c      4883e7e0       and rdi, 0xffffffffffffffe0
            0x180031b80      488d8f3868..   lea rcx, [rdi + 0x6838]
            0x180031b87      e814c60100     call 0x18004e1a0
            0x180031b8c      4889f9         mov rcx, rdi
            0x180031b8f      e8ecb90100     call 0x18004d580
            0x180031b94      488d8f9001..   lea rcx, [rdi + 0x190]
            0x180031b9b      e860c20100     call 0x18004de00
            0x180031ba0      4889f8         mov rax, rdi
            0x180031ba3      4883c420       add rsp, 0x20
            0x180031ba7      5f             pop rdi
            0x180031ba8      c3             ret
            0x180031ba9      0f1f800000..   nop dword [rax]
            0x180031bb0      4889c8         mov rax, rcx
            0x180031bb3      4885c0         test rax, rax
        ┌─< 0x180031bb6      744a           je 0x180031c02
        │   0x180031bb8      8b08           mov ecx, dword [rax]
        │   0x180031bba      85c9           test ecx, ecx
       ┌──< 0x180031bbc      7644           jbe 0x180031c02
       ││   0x180031bbe      81f9a0000000   cmp ecx, 0xa0              ; 160
      ┌───< 0x180031bc4      733c           jae 0x180031c02
      │││   0x180031bc6      448b4004       mov r8d, dword [rax + 4]
      │││   0x180031bca      4585c0         test r8d, r8d
     ┌────< 0x180031bcd      7633           jbe 0x180031c02
     ││││   0x180031bcf      4183f809       cmp r8d, 9                 ; 9
    ┌─────< 0x180031bd3      732d           jae 0x180031c02
    │││││   0x180031bd5      448d0c49       lea r9d, [rcx + rcx*2]
    │││││   0x180031bd9      4c89c0         mov rax, r8
    │││││   0x180031bdc      48c1e005       shl rax, 5
    │││││   0x180031be0      492bc0         sub rax, r8
    │││││   0x180031be3      492bc0         sub rax, r8
    │││││   0x180031be6      89ca           mov edx, ecx
    │││││   0x180031be8      4883c010       add rax, 0x10              ; 16
    │││││   0x180031bec      480fafc2       imul rax, rdx
    │││││   0x180031bf0      4903d1         add rdx, r9
    │││││   0x180031bf3      c1e103         shl ecx, 3
    │││││   0x180031bf6      4c8d940a23..   lea r10, [rdx + rcx + 0x223]
    │││││   0x180031bfe      4903c2         add rax, r10
    │││││   0x180031c01      c3             ret
    └└└└└─> 0x180031c02      33c0           xor eax, eax
            0x180031c04      c3             ret
            0x180031c05      0f1f4000       nop dword [rax]
            0x180031c09      0f1f800000..   nop dword [rax]
            0x180031c10      57             push rdi
            0x180031c11      4154           push r12
            0x180031c13      4155           push r13
            0x180031c15      4156           push r14
            0x180031c17      4157           push r15
            0x180031c19      55             push rbp
            0x180031c1a      4883ec48       sub rsp, 0x48
            0x180031c1e      4989d2         mov r10, rdx
            0x180031c21      4d89c5         mov r13, r8
            0x180031c24      4989ce         mov r14, rcx
            0x180031c27      bac0000000     mov edx, 0xc0              ; 192
            0x180031c2c      498d6a1f       lea rbp, [r10 + 0x1f]
            0x180031c30      4883e5e0       and rbp, 0xffffffffffffffe0
            0x180031c34      4d8b8dc000..   mov r9, qword [r13 + 0xc0]
            0x180031c3b      4c898dc000..   mov qword [rbp + 0xc0], r9
        ┌─> 0x180031c42      410f104415f0   movups xmm0, xmmword [r13 + rdx - 0x10]
        ╎   0x180031c48      0f114415f0     movups xmmword [rbp + rdx - 0x10], xmm0
        ╎   0x180031c4d      410f104c15e0   movups xmm1, xmmword [r13 + rdx - 0x20]
        ╎   0x180031c53      0f114c15e0     movups xmmword [rbp + rdx - 0x20], xmm1
        ╎   0x180031c58      410f105415d0   movups xmm2, xmmword [r13 + rdx - 0x30]
        ╎   0x180031c5e      0f115415d0     movups xmmword [rbp + rdx - 0x30], xmm2
        ╎   0x180031c63      410f105c15c0   movups xmm3, xmmword [r13 + rdx - 0x40]
        ╎   0x180031c69      0f115c15c0     movups xmmword [rbp + rdx - 0x40], xmm3
        ╎   0x180031c6e      4883ea40       sub rdx, 0x40              ; 64
        └─< 0x180031c72      75ce           jne 0x180031c42
            0x180031c74      418b16         mov edx, dword [r14]
            0x180031c77      4189d0         mov r8d, edx
            0x180031c7a      4d89c1         mov r9, r8
            0x180031c7d      49c1e104       shl r9, 4
            0x180031c81      498d820601..   lea rax, [r10 + 0x106]
            0x180031c88      4883e0e0       and rax, 0xffffffffffffffe0
            0x180031c8c      48894560       mov qword [rbp + 0x60], rax
            0x180031c90      4b8d8c0a06..   lea rcx, [r10 + r9 + 0x106]
            0x180031c98      4533d2         xor r10d, r10d
            0x180031c9b      458b4e04       mov r9d, dword [r14 + 4]
            0x180031c9f      4c8d791f       lea r15, [rcx + 0x1f]
            0x180031ca3      4983e7e0       and r15, 0xffffffffffffffe0
            0x180031ca7      85d2           test edx, edx
        ┌─< 0x180031ca9      7623           jbe 0x180031cce
        │   0x180031cab      4d89cb         mov r11, r9
        │   0x180031cae      49c1e305       shl r11, 5
        │   0x180031cb2      4d2bd9         sub r11, r9
        │   0x180031cb5      4d2bd9         sub r11, r9
       ┌──> 0x180031cb8      4489d7         mov edi, r10d
       ╎│   0x180031cbb      41ffc2         inc r10d
       ╎│   0x180031cbe      48c1e704       shl rdi, 4
       ╎│   0x180031cc2      4c893c38       mov qword [rax + rdi], r15
       ╎│   0x180031cc6      4d03fb         add r15, r11
       ╎│   0x180031cc9      443bd2         cmp r10d, edx
       └──< 0x180031ccc      72ea           jb 0x180031cb8
        └─> 0x180031cce      4d89c2         mov r10, r8
            0x180031cd1      49c1e205       shl r10, 5
            0x180031cd5      4d2bd0         sub r10, r8
            0x180031cd8      4d2bd0         sub r10, r8
            0x180031cdb      4d0fafd1       imul r10, r9
            0x180031cdf      8995c0000000   mov dword [rbp + 0xc0], edx
            0x180031ce5      4e8d5c111f     lea r11, [rcx + r10 + 0x1f]
            0x180031cea      44898dc400..   mov dword [rbp + 0xc4], r9d
            0x180031cf1      4f8d7c0367     lea r15, [r11 + r8 + 0x67]
            0x180031cf6      498d4b1f       lea rcx, [r11 + 0x1f]
            0x180031cfa      48             invalid
            0x180031cfb      83             invalid
            0x18004e510      53             push rbx
            0x18004e511      56             push rsi
            0x18004e512      57             push rdi
            0x18004e513      4154           push r12
            0x18004e515      4155           push r13
            0x18004e517      4156           push r14
            0x18004e519      4157           push r15
            0x18004e51b      55             push rbp
            0x18004e51c      4881ec6801..   sub rsp, 0x168
            0x18004e523      4889d0         mov rax, rdx
            0x18004e526      448b9424f8..   mov r10d, dword [rsp + 0x1f8]
            0x18004e52e      33ed           xor ebp, ebp
            0x18004e530      83bc24f001..   cmp dword [rsp + 0x1f0], 0
            0x18004e538      488b10         mov rdx, qword [rax]
            0x18004e53b      488b4008       mov rax, qword [rax + 8]
        ┌─< 0x18004e53f      766f           jbe 0x18004e5b0
        │   0x18004e541      4c898c24c0..   mov qword [rsp + 0xc0], r9
        │   0x18004e549      458d7aff       lea r15d, [r10 - 1]
        │   0x18004e54d      4c8ba42400..   mov r12, qword [rsp + 0x200]
        │   0x18004e555      4889c3         mov rbx, rax
        │   0x18004e558      8bbc24f001..   mov edi, dword [rsp + 0x1f0]
        │   0x18004e55f      4889d6         mov rsi, rdx
        │   0x18004e562      4989ce         mov r14, rcx
        │   0x18004e565      4d89c5         mov r13, r8
       ┌──> 0x18004e568      438b14bc       mov edx, dword [r12 + r15*4]
       ╎│   0x18004e56c      498b0cee       mov rcx, qword [r14 + rbp*8]
       ╎│   0x18004e570      4c8d490f       lea r9, [rcx + 0xf]
       ╎│   0x18004e574      4983e1f0       and r9, 0xfffffffffffffff0
       ╎│   0x18004e578      493bc9         cmp rcx, r9
      ┌───< 0x18004e57b      750c           jne 0x18004e589
      │╎│   0x18004e57d      f6c203         test dl, 3                 ; 3
     ┌────< 0x18004e580      7507           jne 0x18004e589
     ││╎│   0x18004e582      e889c30900     call 0x1800ea910
    ┌─────< 0x18004e587      eb05           jmp 0x18004e58e
    │└└───> 0x18004e589      e882c30900     call 0x1800ea910
    └─────> 0x18004e58e      ffc5           inc ebp
       ╎│   0x18004e590      3bef           cmp ebp, edi
       └──< 0x18004e592      72d4           jb 0x18004e568
        │   0x18004e594      448b9424f8..   mov r10d, dword [rsp + 0x1f8]
        │   0x18004e59c      4889d8         mov rax, rbx
        │   0x18004e59f      4c8b8c24c0..   mov r9, qword [rsp + 0xc0]
        │   0x18004e5a7      4889f2         mov rdx, rsi
        │   0x18004e5aa      4d89e8         mov r8, r13
        │   0x18004e5ad      4c89f1         mov rcx, r14
        └─> 0x18004e5b0      4533ff         xor r15d, r15d
            0x18004e5b3      4533db         xor r11d, r11d
            0x18004e5b6      4585d2         test r10d, r10d
        ┌─< 0x18004e5b9      0f868b050000   jbe 0x18004eb4a
        │   0x18004e5bf      4889842408..   mov qword [rsp + 0x108], rax
        │   0x18004e5c7      33ed           xor ebp, ebp
        │   0x18004e5c9      4889942410..   mov qword [rsp + 0x110], rdx
        │   0x18004e5d1      0f101dc802..   movups xmm3, xmmword [0x18032e8a0] ; [0x18032e8a0:16]=-1
        │   0x18004e5d8      0f1015d102..   movups xmm2, xmmword [0x18032e8b0] ; [0x18032e8b0:16]=-1
        │   0x18004e5df      f30f100dd9..   movss xmm1, dword [0x18032e8c0] ; [0x18032e8c0:4]=0x41800000
        │   0x18004e5e7      f30f1005d5..   movss xmm0, dword [0x18032e8c4] ; [0x18032e8c4:4]=0x40000000
        │   0x18004e5ef      0f11742420     movups xmmword [rsp + 0x20], xmm6
        │   0x18004e5f4      0f117c2430     movups xmmword [rsp + 0x30], xmm7
        │   0x18004e5f9      440f11442440   movups xmmword [rsp + 0x40], xmm8
        │   0x18004e5ff      440f114c2450   movups xmmword [rsp + 0x50], xmm9
        │   0x18004e605      440f11542460   movups xmmword [rsp + 0x60], xmm10
        │   0x18004e60b      440f115c2470   movups xmmword [rsp + 0x70], xmm11
        │   0x18004e611      440f11a424..   movups xmmword [rsp + 0x80], xmm12
        │   0x18004e61a      440f11ac24..   movups xmmword [rsp + 0x90], xmm13
        │   0x18004e623      440f11b424..   movups xmmword [rsp + 0xa0], xmm14
        │   0x18004e62c      440f11bc24..   movups xmmword [rsp + 0xb0], xmm15
        │   0x18004e635      4c89842418..   mov qword [rsp + 0x118], r8
        │   0x18004e63d      4c898c24c0..   mov qword [rsp + 0xc0], r9
        │   0x18004e645      488b8424e8..   mov rax, qword [rsp + 0x1e8]
        │   0x18004e64d      488b9424e0..   mov rdx, qword [rsp + 0x1e0]
        │   0x18004e655      488b9c24d8..   mov rbx, qword [rsp + 0x1d8]
        │   0x18004e65d      488bb424d0..   mov rsi, qword [rsp + 0x1d0]
        │   0x18004e665      488bbc2400..   mov rdi, qword [rsp + 0x200]
       ┌──> 0x18004e66d      468b049f       mov r8d, dword [rdi + r11*4]
       ╎│   0x18004e671      4f8d0c5b       lea r9, [r11 + r11*2]
       ╎│   0x18004e675      f3460f1004de   movss xmm8, dword [rsi + r11*8]
       ╎│   0x18004e67b      f3460f104c..   movss xmm9, dword [rsi + r11*8 + 4]
       ╎│   0x18004e682      f3460f103cdb   movss xmm15, dword [rbx + r11*8]
       ╎│   0x18004e688      f3460f1074..   movss xmm14, dword [rbx + r11*8 + 4]
       ╎│   0x18004e68f      468b14d8       mov r10d, dword [rax + r11*8]
       ╎│   0x18004e693      468b5cd804     mov r11d, dword [rax + r11*8 + 4]
       ╎│   0x18004e698      f3460f10148a   movss xmm10, dword [rdx + r9*4]
       ╎│   0x18004e69e      f3420f107c..   movss xmm7, dword [rdx + r9*4 + 4]
       ╎│   0x18004e6a5      f3420f1074..   movss xmm6, dword [rdx + r9*4 + 8]
       ╎│   0x18004e6ac      4e8b0cd1       mov r9, qword [rcx + r10*8]
       ╎│   0x18004e6b0      4e8b34d9       mov r14, qword [rcx + r11*8]
       ╎│   0x18004e6b4      453bc7         cmp r8d, r15d
      ┌───< 0x18004e6b7      0f8632040000   jbe 0x18004eaef
      │╎│   0x18004e6bd      4589c5         mov r13d, r8d
      │╎│   0x18004e6c0      452bef         sub r13d, r15d
      │╎│   0x18004e6c3      4183fd04       cmp r13d, 4                ; 4
     ┌────< 0x18004e6c7      0f8c91040000   jl 0x18004eb5e
     ││╎│   0x18004e6cd      0f28d7         movaps xmm2, xmm7
     ││╎│   0x18004e6d0      410f28e8       movaps xmm5, xmm8
     ││╎│   0x18004e6d4      410f28e1       movaps xmm4, xmm9
     ││╎│   0x18004e6d8      410f28da       movaps xmm3, xmm10
     ││╎│   0x18004e6dc      0fc6d200       shufps xmm2, xmm2, 0
     ││╎│   0x18004e6e0      450f28ef       movaps xmm13, xmm15
     ││╎│   0x18004e6e4      0fc6ed00       shufps xmm5, xmm5, 0
     ││╎│   0x18004e6e8      450f28e6       movaps xmm12, xmm14
     ││╎│   0x18004e6ec      0fc6e400       shufps xmm4, xmm4, 0
     ││╎│   0x18004e6f0      440f28de       movaps xmm11, xmm6
     ││╎│   0x18004e6f4      0fc6db00       shufps xmm3, xmm3, 0
     ││╎│   0x18004e6f8      4589ec         mov r12d, r13d
     ││╎│   0x18004e6fb      0f11942450..   movups xmmword [rsp + 0x150], xmm2
     ││╎│   0x18004e703      478d1c3f       lea r11d, [r15 + r15]
     ││╎│   0x18004e707      48898c24c8..   mov qword [rsp + 0xc8], rcx
     ││╎│   0x18004e70f      4183e4fc       and r12d, 0xfffffffc       ; 4294967292
     ││╎│   0x18004e713      450fc6ed00     shufps xmm13, xmm13, 0
     ││╎│   0x18004e718      4533d2         xor r10d, r10d
     ││╎│   0x18004e71b      450fc6e400     shufps xmm12, xmm12, 0
     ││╎│   0x18004e720      450fc6db00     shufps xmm11, xmm11, 0
     ││╎│   0x18004e725      0f119c2440..   movups xmmword [rsp + 0x140], xmm3
     ││╎│   0x18004e72d      0f11a42430..   movups xmmword [rsp + 0x130], xmm4
     ││╎│   0x18004e735      0f11ac2420..   movups xmmword [rsp + 0x120], xmm5
     ││╎│   0x18004e73d      f30f11b424..   movss dword [rsp + 0xd0], xmm6
     ││╎│   0x18004e746      f30f11bc24..   movss dword [rsp + 0xd8], xmm7
     ││╎│   0x18004e74f      f3440f1194..   movss dword [rsp + 0xe0], xmm10
     ││╎│   0x18004e759      f3440f11b4..   movss dword [rsp + 0xe8], xmm14
     ││╎│   0x18004e763      f3440f11bc..   movss dword [rsp + 0xf0], xmm15
     ││╎│   0x18004e76d      f3440f118c..   movss dword [rsp + 0xf8], xmm9
     ││╎│   0x18004e777      f3440f1184..   movss dword [rsp + 0x100], xmm8
     ││╎│   0x18004e781      0f10152801..   movups xmm2, xmmword [0x18032e8b0] ; [0x18032e8b0:16]=-1
     ││╎│   0x18004e788      0f100d1101..   movups xmm1, xmmword [0x18032e8a0] ; [0x18032e8a0:16]=-1
     ││╎│   0x18004e78f      488b8c2408..   mov rcx, qword [rsp + 0x108]
     ││╎│   0x18004e797      488bb42410..   mov rsi, qword [rsp + 0x110]
     ││╎│   0x18004e79f      488b842418..   mov rax, qword [rsp + 0x118]
     ││╎│   0x18004e7a7      488b9424c0..   mov rdx, qword [rsp + 0xc0]
    ┌─────> 0x18004e7af      440f10b424..   movups xmm14, xmmword [rsp + 0x120]
    ╎││╎│   0x18004e7b8      438d3c53       lea edi, [r11 + r10*2]
    ╎││╎│   0x18004e7bc      89fb           mov ebx, edi
    ╎││╎│   0x18004e7be      83c704         add edi, 4
    ╎││╎│   0x18004e7c1      410f28ee       movaps xmm5, xmm14
    ╎││╎│   0x18004e7c5      440f101498     movups xmm10, xmmword [rax + rbx*4]
    ╎││╎│   0x18004e7ca      440f100cb8     movups xmm9, xmmword [rax + rdi*4]
    ╎││╎│   0x18004e7cf      440f103c9a     movups xmm15, xmmword [rdx + rbx*4]
    ╎││╎│   0x18004e7d4      440f1004ba     movups xmm8, xmmword [rdx + rdi*4]
    ╎││╎│   0x18004e7d9      0f10842430..   movups xmm0, xmmword [rsp + 0x130]
    ╎││╎│   0x18004e7e1      410f28da       movaps xmm3, xmm10
    ╎││╎│   0x18004e7e5      450fc6d1dd     shufps xmm10, xmm9, 0xdd
    ╎││╎│   0x18004e7ea      410f28e7       movaps xmm4, xmm15
    ╎││╎│   0x18004e7ee      450fc6f8dd     shufps xmm15, xmm8, 0xdd
    ╎││╎│   0x18004e7f3      410f28fd       movaps xmm7, xmm13
    ╎││╎│   0x18004e7f7      410fc6d988     shufps xmm3, xmm9, 0x88
    ╎││╎│   0x18004e7fc      0f28f0         movaps xmm6, xmm0
    ╎││╎│   0x18004e7ff      410f59ea       mulps xmm5, xmm10
    ╎││╎│   0x18004e803      450f28cc       movaps xmm9, xmm12
    ╎││╎│   0x18004e807      410f59ff       mulps xmm7, xmm15
    ╎││╎│   0x18004e80b      410f59f2       mulps xmm6, xmm10
    ╎││╎│   0x18004e80f      450f59cf       mulps xmm9, xmm15
    ╎││╎│   0x18004e813      0f58ef         addps xmm5, xmm7
    ╎││╎│   0x18004e816      440f59f3       mulps xmm14, xmm3
    ╎││╎│   0x18004e81a      0f59c3         mulps xmm0, xmm3
    ╎││╎│   0x18004e81d      410f58f1       addps xmm6, xmm9
    ╎││╎│   0x18004e821      0f59e9         mulps xmm5, xmm1
    ╎││╎│   0x18004e824      0f59f1         mulps xmm6, xmm1
    ╎││╎│   0x18004e827      0f10bc2440..   movups xmm7, xmmword [rsp + 0x140]
    ╎││╎│   0x18004e82f      4183c204       add r10d, 4
    ╎││╎│   0x18004e833      410fc6e088     shufps xmm4, xmm8, 0x88
    ╎││╎│   0x18004e838      440f28cf       movaps xmm9, xmm7
    ╎││╎│   0x18004e83c      450f28c3       movaps xmm8, xmm11
    ╎││╎│   0x18004e840      450f59ca       mulps xmm9, xmm10
    ╎││╎│   0x18004e844      450f59c7       mulps xmm8, xmm15
    ╎││╎│   0x18004e848      450f59d3       mulps xmm10, xmm11
    ╎││╎│   0x18004e84c      0f59fb         mulps xmm7, xmm3
    ╎││╎│   0x18004e84f      450f58c8       addps xmm9, xmm8
    ╎││╎│   0x18004e853      410f59db       mulps xmm3, xmm11
    ╎││╎│   0x18004e857      440f59ca       mulps xmm9, xmm2
    ╎││╎│   0x18004e85b      440f108424..   movups xmm8, xmmword [rsp + 0x150]
    ╎││╎│   0x18004e864      450f59f8       mulps xmm15, xmm8
    ╎││╎│   0x18004e868      450f58d7       addps xmm10, xmm15
    ╎││╎│   0x18004e86c      450f28fd       movaps xmm15, xmm13
    ╎││╎│   0x18004e870      440f59fc       mulps xmm15, xmm4
    ╎││╎│   0x18004e874      440f59d2       mulps xmm10, xmm2
    ╎││╎│   0x18004e878      450f58f7       addps xmm14, xmm15
    ╎││╎│   0x18004e87c      450f28fc       movaps xmm15, xmm12
    ╎││╎│   0x18004e880      440f59fc       mulps xmm15, xmm4
    ╎││╎│   0x18004e884      440f59f1       mulps xmm14, xmm1
    ╎││╎│   0x18004e888      410f58c7       addps xmm0, xmm15
    ╎││╎│   0x18004e88c      0f59c1         mulps xmm0, xmm1
    ╎││╎│   0x18004e88f      450f28fe       movaps xmm15, xmm14
    ╎││╎│   0x18004e893      440f14fd       unpcklps xmm15, xmm5
    ╎││╎│   0x18004e897      440f15f5       unpckhps xmm14, xmm5
    ╎││╎│   0x18004e89b      0f28e8         movaps xmm5, xmm0
    ╎││╎│   0x18004e89e      0f14ee         unpcklps xmm5, xmm6
    ╎││╎│   0x18004e8a1      0f15c6         unpckhps xmm0, xmm6
    ╎││╎│   0x18004e8a4      410f112c9e     movups xmmword [r14 + rbx*4], xmm5
    ╎││╎│   0x18004e8a9      410f1104be     movups xmmword [r14 + rdi*4], xmm0
    ╎││╎│   0x18004e8ae      450f113c99     movups xmmword [r9 + rbx*4], xmm15
    ╎││╎│   0x18004e8b3      450f1134b9     movups xmmword [r9 + rdi*4], xmm14
    ╎││╎│   0x18004e8b8      410f28c3       movaps xmm0, xmm11
    ╎││╎│   0x18004e8bc      0f59c4         mulps xmm0, xmm4
    ╎││╎│   0x18004e8bf      410f59e0       mulps xmm4, xmm8
    ╎││╎│   0x18004e8c3      0f58f8         addps xmm7, xmm0
    ╎││╎│   0x18004e8c6      0f58dc         addps xmm3, xmm4
    ╎││╎│   0x18004e8c9      0f59fa         mulps xmm7, xmm2
    ╎││╎│   0x18004e8cc      0f59da         mulps xmm3, xmm2
    ╎││╎│   0x18004e8cf      0f28e7         movaps xmm4, xmm7
    ╎││╎│   0x18004e8d2      0f28f3         movaps xmm6, xmm3
    ╎││╎│   0x18004e8d5      410f14e1       unpcklps xmm4, xmm9
    ╎││╎│   0x18004e8d9      410f14f2       unpcklps xmm6, xmm10
    ╎││╎│   0x18004e8dd      410f15f9       unpckhps xmm7, xmm9
    ╎││╎│   0x18004e8e1      410f15da       unpckhps xmm3, xmm10
    ╎││╎│   0x18004e8e5      0f11249e       movups xmmword [rsi + rbx*4], xmm4
    ╎││╎│   0x18004e8e9      0f113499       movups xmmword [rcx + rbx*4], xmm6
    ╎││╎│   0x18004e8ed      0f113cbe       movups xmmword [rsi + rdi*4], xmm7
    ╎││╎│   0x18004e8f1      0f111cb9       movups xmmword [rcx + rdi*4], xmm3
    ╎││╎│   0x18004e8f5      453bd4         cmp r10d, r12d
    └─────< 0x18004e8f8      0f82b1feffff   jb 0x18004e7af
     ││╎│   0x18004e8fe      f30f10b424..   movss xmm6, dword [rsp + 0xd0]
     ││╎│   0x18004e907      f30f10bc24..   movss xmm7, dword [rsp + 0xd8]
     ││╎│   0x18004e910      f3440f1094..   movss xmm10, dword [rsp + 0xe0]
     ││╎│   0x18004e91a      f3440f10b4..   movss xmm14, dword [rsp + 0xe8]
     ││╎│   0x18004e924      f3440f10bc..   movss xmm15, dword [rsp + 0xf0]
     ││╎│   0x18004e92e      f3440f108c..   movss xmm9, dword [rsp + 0xf8]
     ││╎│   0x18004e938      f3440f1084..   movss xmm8, dword [rsp + 0x100]
     ││╎│   0x18004e942      488b8c24c8..   mov rcx, qword [rsp + 0xc8]
     ││╎│   0x18004e94a      f30f100572..   movss xmm0, dword [0x18032e8c4] ; [0x18032e8c4:4]=0x40000000
     ││╎│   0x18004e952      f30f100d66..   movss xmm1, dword [0x18032e8c0] ; [0x18032e8c0:4]=0x41800000
     ││╎│   0x18004e95a      488b8424e8..   mov rax, qword [rsp + 0x1e8]
     ││╎│   0x18004e962      488b9424e0..   mov rdx, qword [rsp + 0x1e0]
     ││╎│   0x18004e96a      488b9c24d8..   mov rbx, qword [rsp + 0x1d8]
     ││╎│   0x18004e972      488bb424d0..   mov rsi, qword [rsp + 0x1d0]
     ││╎│   0x18004e97a      488bbc2400..   mov rdi, qword [rsp + 0x200]
    ┌─────> 0x18004e982      453be5         cmp r12d, r13d
   ┌──────< 0x18004e985      0f8364010000   jae 0x18004eaef
   │╎││╎│   0x18004e98b      48898c24c8..   mov qword [rsp + 0xc8], rcx
   │╎││╎│   0x18004e993      4503ff         add r15d, r15d
   │╎││╎│   0x18004e996      488bbc2408..   mov rdi, qword [rsp + 0x108]
   │╎││╎│   0x18004e99e      488bb42410..   mov rsi, qword [rsp + 0x110]
   │╎││╎│   0x18004e9a6      488b8c2418..   mov rcx, qword [rsp + 0x118]
   │╎││╎│   0x18004e9ae      488b9c24c0..   mov rbx, qword [rsp + 0xc0]
  ┌───────> 0x18004e9b6      450f28e8       movaps xmm13, xmm8
  ╎│╎││╎│   0x18004e9ba      478d1467       lea r10d, [r15 + r12*2]
  ╎│╎││╎│   0x18004e9be      450f28df       movaps xmm11, xmm15
  ╎│╎││╎│   0x18004e9c2      458d5a01       lea r11d, [r10 + 1]
  ╎│╎││╎│   0x18004e9c6      f3420f102499   movss xmm4, dword [rcx + r11*4]
  ╎│╎││╎│   0x18004e9cc      410f28d1       movaps xmm2, xmm9
  ╎│╎││╎│   0x18004e9d0      f3420f102c9b   movss xmm5, dword [rbx + r11*4]
  ╎│╎││╎│   0x18004e9d6      450f28e6       movaps xmm12, xmm14
  ╎│╎││╎│   0x18004e9da      f3440f59ec     mulss xmm13, xmm4
  ╎│╎││╎│   0x18004e9df      0f28de         movaps xmm3, xmm6
  ╎│╎││╎│   0x18004e9e2      f3440f59dd     mulss xmm11, xmm5
  ╎│╎││╎│   0x18004e9e7      f30f59d4       mulss xmm2, xmm4
  ╎│╎││╎│   0x18004e9eb      f3440f59e5     mulss xmm12, xmm5
  ╎│╎││╎│   0x18004e9f0      f30f59dd       mulss xmm3, xmm5
  ╎│╎││╎│   0x18004e9f4      f30f59ef       mulss xmm5, xmm7
  ╎│╎││╎│   0x18004e9f8      f3450f58eb     addss xmm13, xmm11
  ╎│╎││╎│   0x18004e9fd      f3410f58d4     addss xmm2, xmm12
  ╎│╎││╎│   0x18004ea02      f3440f59e9     mulss xmm13, xmm1
  ╎│╎││╎│   0x18004ea07      f30f59d1       mulss xmm2, xmm1
  ╎│╎││╎│   0x18004ea0b      450f28da       movaps xmm11, xmm10
  ╎│╎││╎│   0x18004ea0f      450f28e0       movaps xmm12, xmm8
  ╎│╎││╎│   0x18004ea13      f3440f59dc     mulss xmm11, xmm4
  ╎│╎││╎│   0x18004ea18      41ffc4         inc r12d
  ╎│╎││╎│   0x18004ea1b      f30f59e6       mulss xmm4, xmm6
  ╎│╎││╎│   0x18004ea1f      f3440f58db     addss xmm11, xmm3
  ╎│╎││╎│   0x18004ea24      f30f58e5       addss xmm4, xmm5
  ╎│╎││╎│   0x18004ea28      f3440f59d8     mulss xmm11, xmm0
  ╎│╎││╎│   0x18004ea2d      f30f59e0       mulss xmm4, xmm0
  ╎│╎││╎│   0x18004ea31      f3470f112c99   movss dword [r9 + r11*4], xmm13
  ╎│╎││╎│   0x18004ea37      410f28ee       movaps xmm5, xmm14
  ╎│╎││╎│   0x18004ea3b      f3430f11149e   movss dword [r14 + r11*4], xmm2
  ╎│╎││╎│   0x18004ea41      f3460f111c9e   movss dword [rsi + r11*4], xmm11
  ╎│╎││╎│   0x18004ea47      450f28d9       movaps xmm11, xmm9
  ╎│╎││╎│   0x18004ea4b      f3420f11249f   movss dword [rdi + r11*4], xmm4
  ╎│╎││╎│   0x18004ea51      410f28e7       movaps xmm4, xmm15
  ╎│╎││╎│   0x18004ea55      f3420f101491   movss xmm2, dword [rcx + r10*4]
  ╎│╎││╎│   0x18004ea5b      f3420f101c93   movss xmm3, dword [rbx + r10*4]
  ╎│╎││╎│   0x18004ea61      f3440f59e2     mulss xmm12, xmm2
  ╎│╎││╎│   0x18004ea66      f3440f59da     mulss xmm11, xmm2
  ╎│╎││╎│   0x18004ea6b      f30f59e3       mulss xmm4, xmm3
  ╎│╎││╎│   0x18004ea6f      f30f59eb       mulss xmm5, xmm3
  ╎│╎││╎│   0x18004ea73      f3440f58e4     addss xmm12, xmm4
  ╎│╎││╎│   0x18004ea78      f3440f58dd     addss xmm11, xmm5
  ╎│╎││╎│   0x18004ea7d      f3440f59e1     mulss xmm12, xmm1
  ╎│╎││╎│   0x18004ea82      f3440f59d9     mulss xmm11, xmm1
  ╎│╎││╎│   0x18004ea87      410f28ea       movaps xmm5, xmm10
  ╎│╎││╎│   0x18004ea8b      0f28e6         movaps xmm4, xmm6
  ╎│╎││╎│   0x18004ea8e      f30f59ea       mulss xmm5, xmm2
  ╎│╎││╎│   0x18004ea92      f30f59d6       mulss xmm2, xmm6
  ╎│╎││╎│   0x18004ea96      f30f59e3       mulss xmm4, xmm3
  ╎│╎││╎│   0x18004ea9a      f30f59df       mulss xmm3, xmm7
  ╎│╎││╎│   0x18004ea9e      f30f58ec       addss xmm5, xmm4
  ╎│╎││╎│   0x18004eaa2      f30f58d3       addss xmm2, xmm3
  ╎│╎││╎│   0x18004eaa6      f30f59e8       mulss xmm5, xmm0
  ╎│╎││╎│   0x18004eaaa      f30f59d0       mulss xmm2, xmm0
  ╎│╎││╎│   0x18004eaae      f3470f112491   movss dword [r9 + r10*4], xmm12
  ╎│╎││╎│   0x18004eab4      f3470f111c96   movss dword [r14 + r10*4], xmm11
  ╎│╎││╎│   0x18004eaba      f3420f112c96   movss dword [rsi + r10*4], xmm5
  ╎│╎││╎│   0x18004eac0      f3420f111497   movss dword [rdi + r10*4], xmm2
  ╎│╎││╎│   0x18004eac6      453be5         cmp r12d, r13d
  └───────< 0x18004eac9      0f82e7feffff   jb 0x18004e9b6
   │╎││╎│   0x18004eacf      488b9c24d8..   mov rbx, qword [rsp + 0x1d8]
   │╎││╎│   0x18004ead7      488bb424d0..   mov rsi, qword [rsp + 0x1d0]
   │╎││╎│   0x18004eadf      488bbc2400..   mov rdi, qword [rsp + 0x200]
   │╎││╎│   0x18004eae7      488b8c24c8..   mov rcx, qword [rsp + 0xc8]
   └──└───> 0x18004eaef      ffc5           inc ebp
    ╎│ ╎│   0x18004eaf1      4589c7         mov r15d, r8d
    ╎│ ╎│   0x18004eaf4      4189eb         mov r11d, ebp
    ╎│ ╎│   0x18004eaf7      3bac24f801..   cmp ebp, dword [rsp + 0x1f8]
    ╎│ └──< 0x18004eafe      0f8269fbffff   jb 0x18004e66d
    ╎│  │   0x18004eb04      0f10742420     movups xmm6, xmmword [rsp + 0x20]
    ╎│  │   0x18004eb09      0f107c2430     movups xmm7, xmmword [rsp + 0x30]
    ╎│  │   0x18004eb0e      440f10442440   movups xmm8, xmmword [rsp + 0x40]
    ╎│  │   0x18004eb14      440f104c2450   movups xmm9, xmmword [rsp + 0x50]
    ╎│  │   0x18004eb1a      440f10542460   movups xmm10, xmmword [rsp + 0x60]
    ╎│  │   0x18004eb20      440f105c2470   movups xmm11, xmmword [rsp + 0x70]
    ╎│  │   0x18004eb26      440f10a424..   movups xmm12, xmmword [rsp + 0x80]
    ╎│  │   0x18004eb2f      440f10ac24..   movups xmm13, xmmword [rsp + 0x90]
    ╎│  │   0x18004eb38      440f10b424..   movups xmm14, xmmword [rsp + 0xa0]
    ╎│  │   0x18004eb41      440f10bc24..   movups xmm15, xmmword [rsp + 0xb0]
    ╎│  └─> 0x18004eb4a      4881c46801..   add rsp, 0x168
    ╎│      0x18004eb51      5d             pop rbp
    ╎│      0x18004eb52      415f           pop r15
    ╎│      0x18004eb54      415e           pop r14
    ╎│      0x18004eb56      415d           pop r13
    ╎│      0x18004eb58      415c           pop r12
    ╎│      0x18004eb5a      5f             pop rdi
    ╎│      0x18004eb5b      5e             pop rsi
    ╎│      0x18004eb5c      5b             pop rbx
    ╎│      0x18004eb5d      c3             ret
    ╎└────> 0x18004eb5e      4533e4         xor r12d, r12d
    └─────< 0x18004eb61      e91cfeffff     jmp 0x18004e982
            0x18004eb66      0f1f00         nop dword [rax]
            0x18004eb69      0f1f800000..   nop dword [rax]
            0x18004eb70      4154           push r12
            0x18004eb72      4883ec40       sub rsp, 0x40
            0x18004eb76      4189d2         mov r10d, edx
            0x18004eb79      4533db         xor r11d, r11d
            0x18004eb7c      4585d2         test r10d, r10d
        ┌─< 0x18004eb7f      7641           jbe 0x18004ebc2
        │   0x18004eb81      4489ca         mov edx, r9d
        │   0x18004eb84      33c0           xor eax, eax
        │   0x18004eb86      d1ea           shr edx, 1
        │   0x18004eb88      48895c2430     mov qword [rsp + 0x30], rbx
        │   0x18004eb8d      4889742428     mov qword [rsp + 0x28], rsi
        │   0x18004eb92      48897c2420     mov qword [rsp + 0x20], rdi
       ┌──> 0x18004eb97      438b1c98       mov ebx, dword [r8 + r11*4]
       ╎│   0x18004eb9b      83fb01         cmp ebx, 1                 ; 1
      ┌───< 0x18004eb9e      0f84cc000000   je 0x18004ec70
      │╎│   0x18004eba4      83fbff         cmp ebx, 0xffffffff
     ┌────< 0x18004eba7      7420           je 0x18004ebc9
  ┌┌┌─────> 0x18004eba9      ffc0           inc eax
  ╎╎╎││╎│   0x18004ebab      4189c3         mov r11d, eax
  ╎╎╎││╎│   0x18004ebae      413bc2         cmp eax, r10d
  ╎╎╎││└──< 0x18004ebb1      72e4           jb 0x18004eb97
  ╎╎╎││ │   0x18004ebb3      488b5c2430     mov rbx, qword [rsp + 0x30]
  ╎╎╎││ │   0x18004ebb8      488b742428     mov rsi, qword [rsp + 0x28]
  ╎╎╎││ │   0x18004ebbd      488b7c2420     mov rdi, qword [rsp + 0x20]
  ╎╎╎││ └─> 0x18004ebc2      4883c440       add rsp, 0x40
  ╎╎╎││     0x18004ebc6      415c           pop r12
  ╎╎╎││     0x18004ebc8      c3             ret
  ╎╎╎└────> 0x18004ebc9      4a8b34d9       mov rsi, qword [rcx + r11*8]
  ╎╎╎ │     0x18004ebcd      4585c9         test r9d, r9d
  └───────< 0x18004ebd0      76d7           jbe 0x18004eba9
   ╎╎ │     0x18004ebd2      bf01000000     mov edi, 1
   ╎╎ │     0x18004ebd7      33db           xor ebx, ebx
   ╎╎ │     0x18004ebd9      85d2           test edx, edx
   ╎╎ │ ┌─< 0x18004ebdb      7461           je 0x18004ec3e
   ╎╎ │ │   0x18004ebdd      0f1f00         nop dword [rax]
   ╎╎ │┌──> 0x18004ebe0      448d1c9d00..   lea r11d, [rbx*4]
   ╎╎ │╎│   0x18004ebe8      f3420f10049e   movss xmm0, dword [rsi + r11*4]
   ╎╎ │╎│   0x18004ebee      448d249d01..   lea r12d, [rbx*4 + 1]
   ╎╎ │╎│   0x18004ebf6      428b3ca6       mov edi, dword [rsi + r12*4]
   ╎╎ │╎│   0x18004ebfa      0f5705cffc..   xorps xmm0, xmmword [0x18032e8d0]
   ╎╎ │╎│   0x18004ec01      42893c9e       mov dword [rsi + r11*4], edi
   ╎╎ │╎│   0x18004ec05      8d3c9d0200..   lea edi, [rbx*4 + 2]
   ╎╎ │╎│   0x18004ec0c      f3420f1104a6   movss dword [rsi + r12*4], xmm0
   ╎╎ │╎│   0x18004ec12      448d1c9d03..   lea r11d, [rbx*4 + 3]
   ╎╎ │╎│   0x18004ec1a      f30f100cbe     movss xmm1, dword [rsi + rdi*4]
   ╎╎ │╎│   0x18004ec1f      ffc3           inc ebx
   ╎╎ │╎│   0x18004ec21      468b249e       mov r12d, dword [rsi + r11*4]
   ╎╎ │╎│   0x18004ec25      0f570da4fc..   xorps xmm1, xmmword [0x18032e8d0]
   ╎╎ │╎│   0x18004ec2c      448924be       mov dword [rsi + rdi*4], r12d
   ╎╎ │╎│   0x18004ec30      f3420f110c9e   movss dword [rsi + r11*4], xmm1
   ╎╎ │╎│   0x18004ec36      3bda           cmp ebx, edx
   ╎╎ │└──< 0x18004ec38      72a6           jb 0x18004ebe0
   ╎╎ │ │   0x18004ec3a      8d7c1b01       lea edi, [rbx + rbx + 1]
   ╎╎ │ └─> 0x18004ec3e      8d5fff         lea ebx, [rdi - 1]
   ╎╎ │     0x18004ec41      413bd9         cmp ebx, r9d
   └──────< 0x18004ec44      0f835fffffff   jae 0x18004eba9
    ╎ │     0x18004ec4a      8d5c3ffe       lea ebx, [rdi + rdi - 2]
    ╎ │     0x18004ec4e      f30f10049e     movss xmm0, dword [rsi + rbx*4]
    ╎ │     0x18004ec53      8d7c3fff       lea edi, [rdi + rdi - 1]
    ╎ │     0x18004ec57      448b1cbe       mov r11d, dword [rsi + rdi*4]
    ╎ │     0x18004ec5b      0f57056efc..   xorps xmm0, xmmword [0x18032e8d0]
    ╎ │     0x18004ec62      44891c9e       mov dword [rsi + rbx*4], r11d
    ╎ │     0x18004ec66      f30f1104be     movss dword [rsi + rdi*4], xmm0
    └─────< 0x18004ec6b      e939ffffff     jmp 0x18004eba9
      └───> 0x18004ec70      4a8b34d9       mov rsi, qword [rcx + r11*8]
            0x18004ec74      4585c9         test r9d, r9d
            0x18004ec77      0f             invalid
            0x18004ec78      862cff         xchg byte [rdi + rdi*8], ch
            0x18004ec7b      ff             invalid
0x1802eae80  0x0000000100000005  0x00000001802eaee0   ................
0x1802eae90  0x00000001802ed210  0x00000001802ed238   ........8.......
0x1802eaea0  0x0000000000000005  0x00000001802ed24c   ........L.......
0x1802eaeb0  0x0000000000000005  0x00000001802eaf40   ........@.......
0x1802eaec0  0x00000001802ed260  0x3f5d1e0d3f557cef   `........|U?..]?
0x1802eaed0  0x00000001802eafc0  0x00000001802ed288   ................
0x1802eaee0  0x000000003dffffff  0x3e3504f3bdffffff   ...=..........5>
0x1802eaef0  0xbdffffff3e3504f3  0x3dffffff00000000   ..5>...........=
0x1802eaf00  0xbe8211453e6276eb  0x000000003e000000   .vb>E......>....
0x1802eaf10  0x3e580d093df43a69  0x3df43a6f3e580d0a   i:.=..X>..X>o:.=
0x1802ed2b0  0x00000000 0x00000002 0x00000001 0x00000004  ................
0x1802ed2c0  0x00000005 0x00000000 0x00000001 0x00000001  ................
0x1802ed2d0  0x00000002 0x00000002 0x00000004 0x00000004  ................
0x1802ed2e0  0x00000006 0x00000006 0x00000005 0x00000005  ................
0x1802ed2f0  0x00000003 0x00000003 0x00000000 0x00000100  ................
0x1802ed300  0x00000200 0x000002a7 0x000002d7 0x00000328  ............(...
0x1802ed310  0x00000358 0x00000400 0x00000000 0x00000000  X...............
0x1802ed320  0x00000000 0x00000001 0xffffffff 0x00000001  ................
0x1802ed330  0xffffffff 0x3f1304e5 0x00000000 0x3e2d4384  .......?.....C->
0x1802ed340  0x3e2d4384 0x00000000 0x3f1304e5 0x3f10adf7  .C->.......?...?
0x1802ed350  0xbdd0fd7b 0xbdd0fd90 0x3f10adf6 0x3f017fec  {..........?...?
0x1802ed360  0xbe72008f 0xbe720092 0x3f017fec 0x3f68c57f  ..r...r....?..h?
0x1802ed370  0x00000000 0x3e892974 0x3e892975 0x00000000  ....t).>u).>....
0x1802ed380  0x3f68c580 0x3e662809 0xbd263b1d 0xbd263b2d  ..h?.(f>.;&.-;&.
0x1802ed390  0x3e662809 0x3e4e021e 0xbdc07d1b 0xbdc07d1d  .(f>..N>.}...}..
0x1802ed3a0  0x3e4e021f                                   ..N>
