      ╎╎╎   0x1800317ea      498d7550       lea rsi, [r13 + 0x50]
      ╎╎╎   0x1800317ee      4889f1         mov rcx, rsi
      ╎╎╎   0x1800317f1      498dada000..   lea rbp, [r13 + 0xa0]
      ╎╎╎   0x1800317f8      4889ea         mov rdx, rbp
      ╎╎╎   0x1800317fb      4c8d442440     lea r8, [rsp + 0x40]
      ╎╎╎   0x180031800      e8bbc90100     call 0x18004e1c0
      ╎╎╎   0x180031805      4989e2         mov r10, rsp
      ╎╎╎   0x180031808      488d4f10       lea rcx, [rdi + 0x10]
      ╎╎╎   0x18003180c      4c89e2         mov rdx, r12
      ╎╎╎   0x18003180f      4d89e8         mov r8, r13
      ╎╎╎   0x180031812      4989f1         mov r9, rsi
      ╎╎╎   0x180031815      49896a20       mov qword [r10 + 0x20], rbp
      ╎╎╎   0x180031819      488d6c2440     lea rbp, [rsp + 0x40]
      ╎╎╎   0x18003181e      49896a28       mov qword [r10 + 0x28], rbp
      ╎╎╎   0x180031822      e899ca0100     call 0x18004e2c0
      ╎╎╎   0x180031827      4889f8         mov rax, rdi
      ╎╎╎   0x18003182a      4881c4d000..   add rsp, 0xd0
      ╎╎╎   0x180031831      5d             pop rbp
      ╎╎╎   0x180031832      415f           pop r15
      ╎╎╎   0x180031834      415e           pop r14
      ╎╎╎   0x180031836      415d           pop r13
      ╎╎╎   0x180031838      415c           pop r12
      ╎╎╎   0x18003183a      5f             pop rdi
      ╎╎╎   0x18003183b      5e             pop rsi
      ╎╎╎   0x18003183c      c3             ret
     ┌────> 0x18003183d      33db           xor ebx, ebx
     ╎╎╎╎   0x18003183f      4489d1         mov ecx, r10d
     ╎╎╎╎   0x180031842      33d2           xor edx, edx
     ╎╎└──< 0x180031844      e919ffffff     jmp 0x180031762
     ╎╎ ╎   0x180031849      33d2           xor edx, edx
     ╎╎ └─< 0x18003184b      e912ffffff     jmp 0x180031762
     ╎╎     0x180031850      4183fa01       cmp r10d, 1                ; 1
     └────< 0x180031854      73e7           jae 0x18003183d
      └───< 0x180031856      e94cffffff     jmp 0x1800317a7
            0x18003185b      0f1f440000     nop dword [rax + rax]
       ╎╎   0x180031850      4183fa01       cmp r10d, 1                ; 1
       └──< 0x180031854      73e7           jae 0x18003183d
        └─< 0x180031856      e94cffffff     jmp 0x1800317a7
            0x18003185b      0f1f440000     nop dword [rax + rax]
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
            0x180031a13      745f           je 0x180031a74
            0x180031a15      4989e1         mov r9, rsp
            0x180031a18      4c8d974801..   lea r10, [rdi + 0x148]
            0x180031a1f      4c89e9         mov rcx, r13
            0x180031a22      488d87a800..   lea rax, [rdi + 0xa8]
            0x180031a29      4c89fa         mov rdx, r15
            0x180031a2c      4c8d9f3402..   lea r11, [rdi + 0x234]
            0x180031a33      4d895128       mov qword [r9 + 0x28], r10
            0x180031a37      448b16         mov r10d, dword [rsi]
            0x180031a3a      49894120       mov qword [r9 + 0x20], rax
            0x180031a3e      488d87d802..   lea rax, [rdi + 0x2d8]
            0x180031a45      4d895930       mov qword [r9 + 0x30], r11
            0x180031a49      4c8d9c2418..   lea r11, [rsp + 0x118]
            0x180031a51      49894138       mov qword [r9 + 0x38], rax
            0x180031a55      45895140       mov dword [r9 + 0x40], r10d
            0x180031a59      41c7414801..   mov dword [r9 + 0x48], 1
            0x180031a61      4d895950       mov qword [r9 + 0x50], r11
            0x180031a65      4c8b442460     mov r8, qword [rsp + 0x60]
            0x180031a6a      4c8b4c2468     mov r9, qword [rsp + 0x68]
            0x180031a6f      e89cca0100     call 0x18004e510
            0x180031a74      488d8b3868..   lea rcx, [rbx + 0x6838]
            0x180031a7b      48894c2460     mov qword [rsp + 0x60], rcx
            0x180031a80      e8fb4ffeff     call 0x180016a80
            0x180031a85      85c0           test eax, eax
            0x180031a87      7410           je 0x180031a99
            0x180031a89      4c89e2         mov rdx, r12
            0x180031a8c      4989e8         mov r8, rbp
            0x180031a8f      488b4c2460     mov rcx, qword [rsp + 0x60]
            0x180031a94      e8c7c40100     call 0x18004df60
            0x180031a99      4c89e1         mov rcx, r12
            0x180031a9c      41b94d000000   mov r9d, 0x4d              ; 'M' ; 77
            0x180031aa2      8b16           mov edx, dword [rsi]
            0x180031aa4      4c8b4628       mov r8, qword [rsi + 0x28]
            0x180031aa8      e8c3d00100     call 0x18004eb70
