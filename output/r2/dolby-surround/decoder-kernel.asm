            0x180008846      488b8424c8..   mov rax, qword [rsp + 0xc8]
            0x18000884e      483b858807..   cmp rax, qword [rbp + 0x788]
        ┌─< 0x180008855      741a           je 0x180008871
        │   0x180008857      488d151253..   lea rdx, [0x1802edb70]
        │   0x18000885e      488b8db813..   mov rcx, qword [rbp + 0x13b8]
        │   0x180008865      4889858807..   mov qword [rbp + 0x788], rax
        │   0x18000886c      e83f8f0000     call 0x1800117b0
        └─> 0x180008871      4c8b5530       mov r10, qword [rbp + 0x30]
            0x180008875      4889e1         mov rcx, rsp
            0x180008878      4c8b4330       mov r8, qword [rbx + 0x30]
            0x18000887c      488d5308       lea rdx, [rbx + 8]
            0x180008880      498b4250       mov rax, qword [r10 + 0x50]
            0x180008884      4d8b9a9800..   mov r11, qword [r10 + 0x98]
            0x18000888b      48894120       mov qword [rcx + 0x20], rax
            0x18000888f      488d05da52..   lea rax, [0x1802edb70]
            0x180008896      4c895928       mov qword [rcx + 0x28], r11
            0x18000889a      48894130       mov qword [rcx + 0x30], rax
            0x18000889e      448b9424d8..   mov r10d, dword [rsp + 0xd8]
            0x1800088a6      44895138       mov dword [rcx + 0x38], r10d
            0x1800088aa      44895140       mov dword [rcx + 0x40], r10d
            0x1800088ae      4c896948       mov qword [rcx + 0x48], r13
            0x1800088b2      488b8db813..   mov rcx, qword [rbp + 0x13b8]
            0x1800088b9      4c8b8c24c8..   mov r9, qword [rsp + 0xc8]
            0x1800088c1      e8fa8e0100     call 0x1800217c0
            0x1800088c6      4183ff02       cmp r15d, 2                ; reloc.OLEAUT32.dll_SysReAllocString
        ┌─< 0x1800088ca      0f85ab000000   jne 0x18000897b
        │   0x1800088d0      83bc24f000..   cmp dword [rsp + 0xf0], 0
       ┌──< 0x1800088d8      0f859d000000   jne 0x18000897b
       ││   0x1800088de      4533ff         xor r15d, r15d
       ││   0x1800088e1      837b0c00       cmp dword [rbx + 0xc], 0
      ┌───< 0x1800088e5      0f8688000000   jbe 0x180008973
      │││   0x1800088eb      c70300000000   mov dword [rbx], 0
      │││   0x1800088f1      4c89a42470..   mov qword [rsp + 0x670], r12 ; [0x670:8]=-1 ; 1648
      │││   0x1800088f9      488b4318       mov rax, qword [rbx + 0x18]
      │││   0x1800088fd      4589fc         mov r12d, r15d
      │││   0x180008900      4c8b4018       mov r8, qword [rax + 0x18]
      │││   0x180008904      4b8b0ce0       mov rcx, qword [r8 + r12*8]
      │││   0x180008908      4c8d510f       lea r10, [rcx + 0xf]
      │││   0x18000890c      4983e2f0       and r10, 0xfffffffffffffff0
      │││   0x180008910      493bca         cmp rcx, r10
     ┌────< 0x180008913      750c           jne 0x180008921
     ││││   0x180008915      ba50000000     mov edx, 0x50              ; 'P' ; 80
     ││││   0x18000891a      e8f11f0e00     call 0x1800ea910
    ┌─────< 0x18000891f      eb0a           jmp 0x18000892b
    │└────> 0x180008921      ba50000000     mov edx, 0x50              ; 'P' ; 80
    │ │││   0x180008926      e8e51f0e00     call 0x1800ea910
    └─────> 0x18000892b      4c8b5330       mov r10, qword [rbx + 0x30]
      │││   0x18000892f      4d85d2         test r10, r10
     ┌────< 0x180008932      742e           je 0x180008962
     ││││   0x180008934      498b4210       mov rax, qword [r10 + 0x10]
     ││││   0x180008938      418b5220       mov edx, dword [r10 + 0x20]
     ││││   0x18000893c      4c8b4018       mov r8, qword [rax + 0x18]
     ││││   0x180008940      4b8b0ce0       mov rcx, qword [r8 + r12*8]
     ││││   0x180008944      4c8d510f       lea r10, [rcx + 0xf]
     ││││   0x180008948      4983e2f0       and r10, 0xfffffffffffffff0
     ││││   0x18000894c      493bca         cmp rcx, r10
     ││││   0x18000894f      750c           jne 0x18000895d
     ││││   0x180008951      f6c203         test dl, 3                 ; 3
     ││││   0x180008954      7507           jne 0x18000895d
     ││││   0x180008956      e8b51f0e00     call 0x1800ea910
     ││││   0x18000895b      eb05           jmp 0x180008962
     ││││   0x18000895d      e8ae1f0e00     call 0x1800ea910
     └────> 0x180008962      41ffc7         inc r15d
      │││   0x180008965      443b7b0c       cmp r15d, dword [rbx + 0xc]
      │││   0x180008969      728e           jb 0x1800088f9
      │││   0x18000896b      4c8ba42470..   mov r12, qword [rsp + 0x670]
      └───> 0x180008973      c70308000000   mov dword [rbx], 8
       ││   0x180008979      eb06           jmp 0x180008981
       └└─> 0x18000897b      c70300000000   mov dword [rbx], 0
            0x180008981      8b8424c000..   mov eax, dword [rsp + 0xc0]
            0x180008988      894304         mov dword [rbx + 4], eax
            0x18000898b      488b95d000..   mov rdx, qword [rbp + 0xd0]
            0x180008992      eb0b           jmp 0x18000899f
            0x180008994      48c7858807..   mov qword [rbp + 0x788], 0
            0x18000899f      4885d2         test rdx, rdx
            0x1800089a2      0f8433070000   je 0x1800090db
            0x1800089a8      488b842470..   mov rax, qword [rsp + 0x970]
            0x1800089b0      448b5808       mov r11d, dword [rax + 8]
            0x1800089b4      4585db         test r11d, r11d
            0x1800089b7      0f84bf020000   je 0x180008c7c
            0x1800089bd      83bddc3200..   cmp dword [rbp + 0x32dc], 0
            0x1800089c4      0f85b2020000   jne 0x180008c7c
            0x1800089ca      83bc244003..   cmp dword [rsp + 0x340], 0
            0x1800089d2      0f85a4020000   jne 0x180008c7c
            0x1800089d8      488b9c2470..   mov rbx, qword [rsp + 0x970]
            0x1800089e0      498b442418     mov rax, qword [r12 + 0x18]
            0x1800089e5      48898424e0..   mov qword [rsp + 0xe0], rax
            0x1800089ed      4c8b5318       mov r10, qword [rbx + 0x18]
            0x1800089f1      4c899424d8..   mov qword [rsp + 0xd8], r10
            0x1800089f9      4183fb02       cmp r11d, 2                ; reloc.OLEAUT32.dll_SysReAllocString
            0x1800217c0      53             push rbx
            0x1800217c1      56             push rsi
            0x1800217c2      57             push rdi
            0x1800217c3      4154           push r12
            0x1800217c5      4155           push r13
            0x1800217c7      4156           push r14
            0x1800217c9      4157           push r15
            0x1800217cb      55             push rbp
            0x1800217cc      4881ec2801..   sub rsp, 0x128
            0x1800217d3      4c89ce         mov rsi, r9
            0x1800217d6      448bbc24b0..   mov r15d, dword [rsp + 0x1b0]
            0x1800217de      33ff           xor edi, edi
            0x1800217e0      440f117c2470   movups xmmword [rsp + 0x70], xmm15
            0x1800217e6      4c89c3         mov rbx, r8
            0x1800217e9      48898c24c0..   mov qword [rsp + 0xc0], rcx
            0x1800217f1      4989d6         mov r14, rdx
            0x1800217f4      4c8bac24a0..   mov r13, qword [rsp + 0x1a0]
            0x1800217fc      33ed           xor ebp, ebp
            0x1800217fe      f3440f107e08   movss xmm15, dword [rsi + 8]
            0x180021804      443b7908       cmp r15d, dword [rcx + 8]
        ┌─< 0x180021808      740c           je 0x180021816
        │   0x18002180a      4c89ea         mov rdx, r13
        │   0x18002180d      44897908       mov dword [rcx + 8], r15d
        │   0x180021811      e89afffeff     call 0x1800117b0
        └─> 0x180021816      488b8c24b8..   mov rcx, qword [rsp + 0x1b8]
            0x18002181e      458b4500       mov r8d, dword [r13]
            0x180021822      488d4107       lea rax, [rcx + 7]
            0x180021826      4883e0f8       and rax, 0xfffffffffffffff8
            0x18002182a      4889842498..   mov qword [rsp + 0x98], rax
            0x180021832      4a8d44c107     lea rax, [rcx + r8*8 + 7]
            0x180021837      33c9           xor ecx, ecx
            0x180021839      837e0c00       cmp dword [rsi + 0xc], 0
        ┌─< 0x18002183d      0f86ee000000   jbe 0x180021931
        │   0x180021843      8b9424a801..   mov edx, dword [rsp + 0x1a8]
        │   0x18002184a      4533e4         xor r12d, r12d
        │   0x18002184d      4889ac24d0..   mov qword [rsp + 0xd0], rbp
        │   0x180021855      4489fd         mov ebp, r15d
        │   0x180021858      4889bc24e8..   mov qword [rsp + 0xe8], rdi
        │   0x180021860      4189d7         mov r15d, edx
        │   0x180021863      48899c2490..   mov qword [rsp + 0x90], rbx
        │   0x18002186b      4c89eb         mov rbx, r13
        │   0x18002186e      4989c5         mov r13, rax
        │   0x180021871      4889cf         mov rdi, rcx
        │   0x180021874      4c8b5610       mov r10, qword [rsi + 0x10]
        │   0x180021878      488d047f       lea rax, [rdi + rdi*2]
        │   0x18002187c      498b4e10       mov rcx, qword [r14 + 0x10]
        │   0x180021880      4889e2         mov rdx, rsp
        │   0x180021883      4c8b9c24c0..   mov r11, qword [rsp + 0xc0]
        │   0x18002188b      4d8b44c210     mov r8, qword [r10 + rax*8 + 0x10]
        │   0x180021890      458b4cc208     mov r9d, dword [r10 + rax*8 + 8]
        │   0x180021895      4d8b1b         mov r11, qword [r11]
        │   0x180021898      44897a20       mov dword [rdx + 0x20], r15d
        │   0x18002189c      478b0c88       mov r9d, dword [r8 + r9*4]
        │   0x1800218a0      896a28         mov dword [rdx + 0x28], ebp
        │   0x1800218a3      c742300100..   mov dword [rdx + 0x30], 1
        │   0x1800218aa      4e8b0cc9       mov r9, qword [rcx + r9*8]
        │   0x1800218ae      4c894a38       mov qword [rdx + 0x38], r9
        │   0x1800218b2      458b4cc20c     mov r9d, dword [r10 + rax*8 + 0xc]
        │   0x1800218b7      4c896a50       mov qword [rdx + 0x50], r13
        │   0x1800218bb      478b0488       mov r8d, dword [r8 + r9*4]
        │   0x1800218bf      4d8b0cc2       mov r9, qword [r10 + rax*8]
        │   0x1800218c3      4e8b04c1       mov r8, qword [rcx + r8*8]
        │   0x1800218c7      4c894240       mov qword [rdx + 0x40], r8
        │   0x1800218cb      458b4604       mov r8d, dword [r14 + 4]
        │   0x1800218cf      44894248       mov dword [rdx + 0x48], r8d
        │   0x1800218d3      498b0cfb       mov rcx, qword [r11 + rdi*8]
        │   0x1800218d7      488b942498..   mov rdx, qword [rsp + 0x198]
        │   0x1800218df      4c8b842490..   mov r8, qword [rsp + 0x190]
        │   0x1800218e7      e824fb0000     call 0x180031410
        │   0x1800218ec      4c8b842498..   mov r8, qword [rsp + 0x98]
        │   0x1800218f4      8b4b04         mov ecx, dword [rbx + 4]
        │   0x1800218f7      498904f8       mov qword [r8 + rdi*8], rax
        │   0x1800218fb      e8d0fa0000     call 0x1800313d0
        │   0x180021900      41ffc4         inc r12d
        │   0x180021903      4c03e8         add r13, rax
        │   0x180021906      4489e7         mov edi, r12d
        │   0x180021909      443b660c       cmp r12d, dword [rsi + 0xc]
        │   0x18002190d      0f8261ffffff   jb 0x180021874
        │   0x180021913      488bac24d0..   mov rbp, qword [rsp + 0xd0]
        │   0x18002191b      4c89e8         mov rax, r13
        │   0x18002191e      488bbc24e8..   mov rdi, qword [rsp + 0xe8]
        │   0x180021926      4989dd         mov r13, rbx
        │   0x180021929      488b9c2490..   mov rbx, qword [rsp + 0x90]
        └─> 0x180021931      488d4807       lea rcx, [rax + 7]
            0x180021935      4883e1f8       and rcx, 0xfffffffffffffff8
            0x180021939      48898c24f0..   mov qword [rsp + 0xf0], rcx
            0x180021941      418b4d04       mov ecx, dword [r13 + 4]
            0x180021945      4c8d44c807     lea r8, [rax + rcx*8 + 7]
            0x18002194a      4885db         test rbx, rbx
            0x18002194d      740d           je 0x18002195c
            0x18002194f      498d7807       lea rdi, [r8 + 7]
            0x180021953      4883e7f8       and rdi, 0xfffffffffffffff8
            0x180021957      4d8d44c807     lea r8, [r8 + rcx*8 + 7]
            0x18002195c      418b550c       mov edx, dword [r13 + 0xc]
            0x180021960      4189d3         mov r11d, edx
            0x180021963      4c89d8         mov rax, r11
            0x180021966      48c1e004       shl rax, 4
            0x18002196a      4d8d501f       lea r10, [r8 + 0x1f]
            0x18002196e      4983e2e0       and r10, 0xffffffffffffffe0
            0x180021972      4a8d4c001f     lea rcx, [rax + r8 + 0x1f]
            0x180021977      4989c9         mov r9, rcx
            0x18002197a      85d2           test edx, edx
            0x18002197c      0f8647010000   jbe 0x180021ac9
            0x180021982      83fa08         cmp edx, 8                 ; 8
            0x180021985      0f828d0b0000   jb 0x180022518
            0x18002198b      4189d0         mov r8d, edx
            0x18002198e      488d819f02..   lea rax, [rcx + 0x29f]
            0x180021995      66480f6ed1     movq xmm2, rcx
            0x18002199a      4183e0f8       and r8d, 0xfffffff8        ; 4294967288
            0x18002199e      f30f6f1d1a..   movdqu xmm3, xmmword [0x1802ee9c0] ; [0x1802ee9c0:16]=-1
            0x1800219a6      f30f6f0d32..   movdqu xmm1, xmmword [0x1802ee9e0] ; [0x1802ee9e0:16]=-1
            0x1800219ae      66480f6ec0     movq xmm0, rax
            0x1800219b3      33c0           xor eax, eax
            0x1800219b5      660f6cd0       punpcklqdq xmm2, xmm0
            0x1800219b9      f30f6f050f..   movdqu xmm0, xmmword [0x1802ee9d0] ; [0x1802ee9d0:16]=-1
            0x1800219c1      4189c4         mov r12d, eax
            0x1800219c4      660f6fe0       movdqa xmm4, xmm0
            0x1800219c8      49c1e404       shl r12, 4
            0x1800219cc      448d6802       lea r13d, [rax + 2]
            0x1800219d0      660fd4e2       paddq xmm4, xmm2
            0x1800219d4      660fd4d3       paddq xmm2, xmm3
            0x1800219d8      660f6fe8       movdqa xmm5, xmm0
            0x1800219dc      660fdbe1       pand xmm4, xmm1
            0x1800219e0      49c1e504       shl r13, 4
            0x1800219e4      448d7804       lea r15d, [rax + 4]
            0x1800219e8      660fd4ea       paddq xmm5, xmm2
            0x1800219ec      660fd4d3       paddq xmm2, xmm3
            0x1800219f0      66430fd62414   movq qword [r12 + r10], xmm4
            0x1800219f6      660fdbe9       pand xmm5, xmm1
            0x1800219fa      660f73dc08     psrldq xmm4, 8
            0x1800219ff      66430fd664..   movq qword [r12 + r10 + 0x10], xmm4
            0x180021a06      660f6fe0       movdqa xmm4, xmm0
            0x180021a0a      66430fd66c..   movq qword [r13 + r10], xmm5
            0x180021a11      448d6006       lea r12d, [rax + 6]
            0x180021a15      660f73dd08     psrldq xmm5, 8
            0x180021a1a      83c008         add eax, 8
            0x180021a1d      660fd4e2       paddq xmm4, xmm2
            0x180021a21      660fd4d3       paddq xmm2, xmm3
            0x180021a25      49c1e704       shl r15, 4
            0x180021a29      660fdbe1       pand xmm4, xmm1
            0x1800117b0      4883ec38       sub rsp, 0x38
            0x1800117b4      33c0           xor eax, eax
            0x1800117b6      833a00         cmp dword [rdx], 0
        ┌─< 0x1800117b9      7638           jbe 0x1800117f3
        │   0x1800117bb      48895c2430     mov qword [rsp + 0x30], rbx
        │   0x1800117c0      4889d3         mov rbx, rdx
        │   0x1800117c3      4889742428     mov qword [rsp + 0x28], rsi
        │   0x1800117c8      89c6           mov esi, eax
        │   0x1800117ca      48897c2420     mov qword [rsp + 0x20], rdi
        │   0x1800117cf      4889cf         mov rdi, rcx
       ┌──> 0x1800117d2      4c8b07         mov r8, qword [rdi]
       ╎│   0x1800117d5      498b0cf0       mov rcx, qword [r8 + rsi*8]
       ╎│   0x1800117d9      e8c2fb0100     call 0x1800313a0
       ╎│   0x1800117de      ffc6           inc esi
       ╎│   0x1800117e0      3b33           cmp esi, dword [rbx]
       └──< 0x1800117e2      72ee           jb 0x1800117d2
        │   0x1800117e4      488b5c2430     mov rbx, qword [rsp + 0x30]
        │   0x1800117e9      488b742428     mov rsi, qword [rsp + 0x28]
        │   0x1800117ee      488b7c2420     mov rdi, qword [rsp + 0x20]
        └─> 0x1800117f3      4883c438       add rsp, 0x38
            0x1800117f7      c3             ret
            0x1800117f8      0f1f840000..   nop dword [rax + rax]
            0x180011800      4883ec48       sub rsp, 0x48
            0x180011804      4d89c2         mov r10, r8
            0x180011807      4989d0         mov r8, rdx
            0x18001180a      33d2           xor edx, edx
            0x18001180c      498d421f       lea rax, [r10 + 0x1f]
            0x180011810      4883e0e0       and rax, 0xffffffffffffffe0
            0x180011814      4d8d4a36       lea r9, [r10 + 0x36]
            0x180011818      458b18         mov r11d, dword [r8]
            0x18001181b      4983e1f8       and r9, 0xfffffffffffffff8
            0x18001181f      4c8908         mov qword [rax], r9
            0x180011822      4b8d4cda36     lea rcx, [r10 + r11*8 + 0x36]
            0x180011827      4d85db         test r11, r11
        ┌─< 0x18001182a      7653           jbe 0x18001187f
        │   0x18001182c      48895c2438     mov qword [rsp + 0x38], rbx
        │   0x180011831      4889c3         mov rbx, rax
        │   0x180011834      4889742430     mov qword [rsp + 0x30], rsi
        │   0x180011839      4c89c6         mov rsi, r8
        │   0x18001183c      48897c2428     mov qword [rsp + 0x28], rdi
        │   0x180011841      89d7           mov edi, edx
        │   0x180011843      4c89642420     mov qword [rsp + 0x20], r12
        │   0x180011848      4989cc         mov r12, rcx
       ┌──> 0x18001184b      4c89e1         mov rcx, r12
       ╎│   0x18001184e      e81d030200     call 0x180031b70
       ╎│   0x180011853      4c8b0b         mov r9, qword [rbx]
       ╎│   0x180011856      498904f9       mov qword [r9 + rdi*8], rax
       ╎│   0x18001185a      e831fb0100     call 0x180031390
       ╎│   0x18001185f      ffc7           inc edi
       ╎│   0x180011861      4c03e0         add r12, rax
       ╎│   0x180011864      3b3e           cmp edi, dword [rsi]
       └──< 0x180011866      72e3           jb 0x18001184b
        │   0x180011868      488b742430     mov rsi, qword [rsp + 0x30]
        │   0x18001186d      4889d8         mov rax, rbx
        │   0x180011870      488b5c2438     mov rbx, qword [rsp + 0x38]
        │   0x180011875      488b7c2428     mov rdi, qword [rsp + 0x28]
        │   0x18001187a      4c8b642420     mov r12, qword [rsp + 0x20]
        └─> 0x18001187f      c740080000..   mov dword [rax + 8], 0
            0x180011886      4883c448       add rsp, 0x48
            0x18001188a      c3             ret
            0x18001188b      0f1f440000     nop dword [rax + rax]
            0x180011890      4589c0         mov r8d, r8d
            0x180011893      89c9           mov ecx, ecx
            0x180011895      49c1e002       shl r8, 2
            0x180011899      4b8d44c02b     lea rax, [r8 + r8*8 + 0x2b]
            0x18001189e      480fafc1       imul rax, rcx
            0x1800118a2      4883c075       add rax, 0x75              ; 117
            0x1800118a6      c3             ret
            0x1800118a7      660f1f8400..   nop word [rax + rax]
            0x1800118b0      4589c0         mov r8d, r8d
            0x1800118b3      89c9           mov ecx, ecx
            0x1800118b5      49c1e002       shl r8, 2
            0x1800118b9      4b8d44c027     lea rax, [r8 + r8*8 + 0x27]
            0x1800118be      480fafc1       imul rax, rcx
            0x1800118c2      4883c04e       add rax, 0x4e              ; 78
            0x1800118c6      c3             ret
            0x1800118c7      660f1f8400..   nop word [rax + rax]
            0x1800118d0      4589c0         mov r8d, r8d
            0x1800118d3      4b8d0480       lea rax, [r8 + r8*4]
            0x1800118d7      488d04c51f..   lea rax, [rax*8 + 0x1f]
            0x1800118df      c3             ret
            0x1800118e0      4589c0         mov r8d, r8d
            0x1800118e3      49c1e004       shl r8, 4
            0x1800118e7      4983c01f       add r8, 0x1f               ; 31
            0x1800118eb      4c89c0         mov rax, r8
            0x1800118ee      c3             ret
            0x1800118ef      90             nop
            0x1800118f0      4156           push r14
            0x1800118f2      4157           push r15
            0x1800118f4      4883ec28       sub rsp, 0x28
