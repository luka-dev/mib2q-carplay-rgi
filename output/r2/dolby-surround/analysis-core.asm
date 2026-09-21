            0x18004d0b0      4155           push r13
            0x18004d0b2      4156           push r14
            0x18004d0b4      4157           push r15
            0x18004d0b6      55             push rbp
            0x18004d0b7      4881ece801..   sub rsp, 0x1e8
            0x18004d0be      4d89ce         mov r14, r9
            0x18004d0c1      4989e2         mov r10, rsp
            0x18004d0c4      4989d7         mov r15, rdx
            0x18004d0c7      4d89c5         mov r13, r8
            0x18004d0ca      4889cd         mov rbp, rcx
            0x18004d0cd      4c89f9         mov rcx, r15
            0x18004d0d0      4d89f0         mov r8, r14
            0x18004d0d3      41b914000000   mov r9d, 0x14              ; 20
            0x18004d0d9      488d8424f8..   lea rax, [rsp + 0xf8]
            0x18004d0e1      8b94244002..   mov edx, dword [rsp + 0x240]
            0x18004d0e8      49894220       mov qword [r10 + 0x20], rax
            0x18004d0ec      e84f55fdff     call 0x180022640
            0x18004d0f1      4989e2         mov r10, rsp
            0x18004d0f4      4c89e9         mov rcx, r13
            0x18004d0f7      4d89f0         mov r8, r14
            0x18004d0fa      41b914000000   mov r9d, 0x14              ; 20
            0x18004d100      8b94244002..   mov edx, dword [rsp + 0x240]
            0x18004d107      488d842448..   lea rax, [rsp + 0x148]
            0x18004d10f      49894220       mov qword [r10 + 0x20], rax
            0x18004d113      e82855fdff     call 0x180022640
            0x18004d118      90             nop
            0x18004d119      4533c9         xor r9d, r9d
            0x18004d11c      4533db         xor r11d, r11d
            0x18004d11f      4889ac24d0..   mov qword [rsp + 0xd0], rbp
            0x18004d127      4533d2         xor r10d, r10d
            0x18004d12a      48899c24d8..   mov qword [rsp + 0xd8], rbx
            0x18004d132      660fefd2       pxor xmm2, xmm2
            0x18004d136      4889b424e0..   mov qword [rsp + 0xe0], rsi
            0x18004d13e      4889bc24e8..   mov qword [rsp + 0xe8], rdi
            0x18004d146      4c89a424f0..   mov qword [rsp + 0xf0], r12
            0x18004d14e      8bac244002..   mov ebp, dword [rsp + 0x240]
            0x18004d155      438b1c9e       mov ebx, dword [r14 + r11*4]
            0x18004d159      413bd9         cmp ebx, r9d
        ┌─< 0x18004d15c      770f           ja 0x18004d16d
        │   0x18004d15e      f3420f1194..   movss dword [rsp + r11*4 + 0x198], xmm2
       ┌──< 0x18004d168      e939010000     jmp 0x18004d2a6
       │└─> 0x18004d16d      33d2           xor edx, edx
       │    0x18004d16f      660fefc0       pxor xmm0, xmm0
       │    0x18004d173      0f28c8         movaps xmm1, xmm0
       │    0x18004d176      85ed           test ebp, ebp
       │┌─< 0x18004d178      0f8616010000   jbe 0x18004d294
       ││   0x18004d17e      4c899c24c0..   mov qword [rsp + 0xc0], r11
       ││   0x18004d186      438d0409       lea eax, [r9 + r9]
       ││   0x18004d18a      41f7d9         neg r9d
       ││   0x18004d18d      4c89b424c8..   mov qword [rsp + 0xc8], r14
       ││   0x18004d195      4403cb         add r9d, ebx
       ││   0x18004d198      4533e4         xor r12d, r12d
       ││   0x18004d19b      8d7201         lea esi, [rdx + 1]
       ││   0x18004d19e      4d8b74d500     mov r14, qword [r13 + rdx*8]
       ││   0x18004d1a3      4d8b1cd7       mov r11, qword [r15 + rdx*8]
       ││   0x18004d1a7      3bf5           cmp esi, ebp
      ┌───< 0x18004d1a9      724f           jb 0x18004d1fa
      │││   0x18004d1ab      0f1f440000     nop dword [rax + rax]
      │││   0x18004d1b0      660fefe4       pxor xmm4, xmm4
      │││   0x18004d1b4      428d3c60       lea edi, [rax + r12*2]
      │││   0x18004d1b8      660fefed       pxor xmm5, xmm5
      │││   0x18004d1bc      8d5701         lea edx, [rdi + 1]
      │││   0x18004d1bf      f3410f5a2493   cvtss2sd xmm4, dword [r11 + rdx*4]
      │││   0x18004d1c5      f3410f5a1c96   cvtss2sd xmm3, dword [r14 + rdx*4]
      │││   0x18004d1cb      f3410f5a2cbe   cvtss2sd xmm5, dword [r14 + rdi*4]
      │││   0x18004d1d1      f20f59e3       mulsd xmm4, xmm3
      │││   0x18004d1d5      660fefdb       pxor xmm3, xmm3
      │││   0x18004d1d9      41ffc4         inc r12d
      │││   0x18004d1dc      f3410f5a1cbb   cvtss2sd xmm3, dword [r11 + rdi*4]
      │││   0x18004d1e2      f20f58cc       addsd xmm1, xmm4
      │││   0x18004d1e6      f20f59dd       mulsd xmm3, xmm5
      │││   0x18004d1ea      f20f58c3       addsd xmm0, xmm3
      │││   0x18004d1ee      453be1         cmp r12d, r9d
      │││   0x18004d1f1      72bd           jb 0x18004d1b0
      │││   0x18004d1f3      89f2           mov edx, esi
      │││   0x18004d1f5      e982000000     jmp 0x18004d27c
      └───> 0x18004d1fa      4d8b04f7       mov r8, qword [r15 + rsi*8]
       ││   0x18004d1fe      498b74f500     mov rsi, qword [r13 + rsi*8]
       ││   0x18004d203      0f1f440000     nop dword [rax + rax]
       ││   0x18004d208      0f1f840000..   nop dword [rax + rax]
       ││   0x18004d210      660fefe4       pxor xmm4, xmm4
       ││   0x18004d214      428d3c60       lea edi, [rax + r12*2]
       ││   0x18004d218      41ffc4         inc r12d
       ││   0x18004d21b      8d4f01         lea ecx, [rdi + 1]
       ││   0x18004d21e      f3410f5a248b   cvtss2sd xmm4, dword [r11 + rcx*4]
       ││   0x18004d224      f3410f5a1c8e   cvtss2sd xmm3, dword [r14 + rcx*4]
       ││   0x18004d22a      f30f5a2c8e     cvtss2sd xmm5, dword [rsi + rcx*4]
       ││   0x18004d22f      f20f59e3       mulsd xmm4, xmm3
       ││   0x18004d233      f3410f5a1c88   cvtss2sd xmm3, dword [r8 + rcx*4]
       ││   0x18004d239      f20f58cc       addsd xmm1, xmm4
       ││   0x18004d23d      f3410f5a24be   cvtss2sd xmm4, dword [r14 + rdi*4]
       ││   0x18004d243      f20f59dd       mulsd xmm3, xmm5
       ││   0x18004d247      660fefed       pxor xmm5, xmm5
       ││   0x18004d24b      f20f58cb       addsd xmm1, xmm3
       ││   0x18004d24f      f3410f5a1cbb   cvtss2sd xmm3, dword [r11 + rdi*4]
       ││   0x18004d255      f30f5a2cbe     cvtss2sd xmm5, dword [rsi + rdi*4]
       ││   0x18004d25a      f20f59dc       mulsd xmm3, xmm4
       ││   0x18004d25e      f20f58c3       addsd xmm0, xmm3
       ││   0x18004d262      660fefdb       pxor xmm3, xmm3
       ││   0x18004d266      f3410f5a1cb8   cvtss2sd xmm3, dword [r8 + rdi*4]
       ││   0x18004d26c      f20f59dd       mulsd xmm3, xmm5
       ││   0x18004d270      f20f58c3       addsd xmm0, xmm3
       ││   0x18004d274      453be1         cmp r12d, r9d
       ││   0x18004d277      7297           jb 0x18004d210
       ││   0x18004d279      83c202         add edx, 2
       ││   0x18004d27c      3bd5           cmp edx, ebp
       ││   0x18004d27e      0f8214ffffff   jb 0x18004d198
       ││   0x18004d284      4c8b9c24c0..   mov r11, qword [rsp + 0xc0]
       ││   0x18004d28c      4c8bb424c8..   mov r14, qword [rsp + 0xc8]
       │└─> 0x18004d294      f20f58c1       addsd xmm0, xmm1
       │    0x18004d298      f20f5ac0       cvtsd2ss xmm0, xmm0
       │    0x18004d29c      f3420f1184..   movss dword [rsp + r11*4 + 0x198], xmm0
       └──> 0x18004d2a6      41ffc2         inc r10d
            0x18004d2a9      4189d9         mov r9d, ebx
            0x18004d2ac      4589d3         mov r11d, r10d
            0x18004d2af      4183fa14       cmp r10d, 0x14             ; 20
            0x18004d2b3      0f829cfeffff   jb 0x18004d155
            0x18004d2b9      f30f10257f..   movss xmm4, dword [0x18032e840] ; [0x18032e840:4]=0x3f800000
            0x18004d2c1      4533f6         xor r14d, r14d
            0x18004d2c4      f30f10ac24..   movss xmm5, dword [rsp + 0x238]
            0x18004d2cd      0f28dc         movaps xmm3, xmm4
            0x18004d2d0      f30f100d58..   movss xmm1, dword [0x18032e830] ; [0x18032e830:4]=0x800000
            0x18004d2d8      4533ed         xor r13d, r13d
            0x18004d2db      f30f100551..   movss xmm0, dword [0x18032e834] ; [0x18032e834:4]=0x3089705f
            0x18004d2e3      f30f5cdd       subss xmm3, xmm5
            0x18004d2e7      440f116c2440   movups xmmword [rsp + 0x40], xmm13
            0x18004d2ed      440f11742430   movups xmmword [rsp + 0x30], xmm14
            0x18004d2f3      488bac24d0..   mov rbp, qword [rsp + 0xd0]
            0x18004d2fb      488b9c24d8..   mov rbx, qword [rsp + 0xd8]
            0x18004d303      488bb424e0..   mov rsi, qword [rsp + 0xe0]
            0x18004d30b      488bbc24e8..   mov rdi, qword [rsp + 0xe8]
            0x18004d313      4c8ba424f0..   mov r12, qword [rsp + 0xf0]
            0x18004d31b      4c8bbc2430..   mov r15, qword [rsp + 0x230]
            0x18004d323      0f11b424b0..   movups xmmword [rsp + 0xb0], xmm6
            0x18004d32b      0f28f3         movaps xmm6, xmm3
            0x18004d32e      0f11bc24a0..   movups xmmword [rsp + 0xa0], xmm7
            0x18004d336      0f28fc         movaps xmm7, xmm4
            0x18004d339      440f118424..   movups xmmword [rsp + 0x90], xmm8
            0x18004d342      440f28c5       movaps xmm8, xmm5
            0x18004d346      440f118c24..   movups xmmword [rsp + 0x80], xmm9
            0x18004d34f      440f28ca       movaps xmm9, xmm2
            0x18004d353      440f11542470   movups xmmword [rsp + 0x70], xmm10
            0x18004d359      440f28d1       movaps xmm10, xmm1
            0x18004d35d      440f115c2460   movups xmmword [rsp + 0x60], xmm11
            0x18004d363      440f28d8       movaps xmm11, xmm0
            0x18004d367      440f11642450   movups xmmword [rsp + 0x50], xmm12
            0x18004d36d      f3440f102d..   movss xmm13, dword [0x18032e83c] ; [0x18032e83c:4]=0x3f007081
            0x18004d376      f3440f1035..   movss xmm14, dword [0x18032e838] ; [0x18032e838:4]=0x3f004dce
            0x18004d37f      f3420f1084..   movss xmm0, dword [rsp + r14*4 + 0xf8]
            0x18004d389      0f28df         movaps xmm3, xmm7
            0x18004d38c      f3420f108c..   movss xmm1, dword [rsp + r14*4 + 0x148]
            0x18004d396      440f28e0       movaps xmm12, xmm0
            0x18004d39a      f3430f1014b7   movss xmm2, dword [r15 + r14*4]
            0x18004d3a0      f3440f58e1     addss xmm12, xmm1
            0x18004d3a5      f30f5cda       subss xmm3, xmm2
            0x18004d3a9      f30f5cc1       subss xmm0, xmm1
            0x18004d3ad      f3420f595c..   mulss xmm3, dword [rbp + r14*4]
            0x18004d3b4      0f28ca         movaps xmm1, xmm2
            0x18004d3b7      f3410f59cc     mulss xmm1, xmm12
            0x18004d3bc      f30f58cb       addss xmm1, xmm3
            0x18004d3c0      0f28e1         movaps xmm4, xmm1
            0x18004d3c3      0f57255614..   xorps xmm4, xmmword [0x18032e820]
            0x18004d3ca      f30f5fe1       maxss xmm4, xmm1
            0x18004d3ce      440f2fd4       comiss xmm10, xmm4
            0x18004d3d2      7609           jbe 0x18004d3dd
            0x18004d3d4      f3460f114c..   movss dword [rbp + r14*4], xmm9
            0x18004d3db      eb07           jmp 0x18004d3e4
            0x18004d3dd      f3420f114c..   movss dword [rbp + r14*4], xmm1
            0x18004d3e4      f3430f100cb7   movss xmm1, dword [r15 + r14*4]
            0x18004d3ea      0f28d7         movaps xmm2, xmm7
            0x18004d3ed      f30f59c1       mulss xmm0, xmm1
            0x18004d3f1      f30f5cd1       subss xmm2, xmm1
            0x18004d3f5      f3420f5954..   mulss xmm2, dword [rbp + r14*4 + 0x50]
            0x18004d3fc      f30f58c2       addss xmm0, xmm2
            0x18004d400      0f28d8         movaps xmm3, xmm0
            0x18004d403      0f571d1614..   xorps xmm3, xmmword [0x18032e820]
            0x18004d40a      f30f5fd8       maxss xmm3, xmm0
            0x18004d40e      440f2fd3       comiss xmm10, xmm3
            0x18004d412      7609           jbe 0x18004d41d
            0x18004d414      f3460f114c..   movss dword [rbp + r14*4 + 0x50], xmm9
            0x18004d41b      eb07           jmp 0x18004d424
            0x18004d41d      f3420f1144..   movss dword [rbp + r14*4 + 0x50], xmm0
            0x18004d424      f3430f1004b7   movss xmm0, dword [r15 + r14*4]
            0x18004d42a      0f28cf         movaps xmm1, xmm7
            0x18004d42d      f3420f109c..   movss xmm3, dword [rsp + r14*4 + 0x198]
            0x18004d437      f30f5cc8       subss xmm1, xmm0
            0x18004d43b      f30f59d8       mulss xmm3, xmm0
            0x18004d43f      f3420f598c..   mulss xmm1, dword [rbp + r14*4 + 0xa0]
            0x18004d449      f30f58d9       addss xmm3, xmm1
            0x18004d44d      0f28d3         movaps xmm2, xmm3
            0x18004d450      0f5715c913..   xorps xmm2, xmmword [0x18032e820]
            0x18004d457      f30f5fd3       maxss xmm2, xmm3
            0x18004d45b      440f2fd2       comiss xmm10, xmm2
            0x18004d45f      760c           jbe 0x18004d46d
            0x18004d461      f3460f118c..   movss dword [rbp + r14*4 + 0xa0], xmm9
            0x18004d46b      eb0a           jmp 0x18004d477
            0x18004d46d      f3420f119c..   movss dword [rbp + r14*4 + 0xa0], xmm3
            0x18004d477      f3420f108c..   movss xmm1, dword [rbp + r14*4 + 0xf0]
            0x18004d481      410f28c3       movaps xmm0, xmm11
            0x18004d485      f3410f58c4     addss xmm0, xmm12
            0x18004d48a      f3410f58cb     addss xmm1, xmm11
            0x18004d48f      f30f5ec1       divss xmm0, xmm1
            0x18004d493      f3410f59c6     mulss xmm0, xmm14
            0x18004d498      e8a3ebfeff     call 0x18003c040
            0x18004d49d      f3410f59c5     mulss xmm0, xmm13
            0x18004d4a2      410f28c9       movaps xmm1, xmm9
            0x18004d4a6      f30f5fc8       maxss xmm1, xmm0
            0x18004d4aa      f3420f1084..   movss xmm0, dword [rbp + r14*4 + 0x140]
            0x18004d4b4      f30f5dcf       minss xmm1, xmm7
            0x18004d4b8      0f2fc1         comiss xmm0, xmm1
            0x18004d4bb      7208           jb 0x18004d4c5
            0x18004d4bd      410f28c8       movaps xmm1, xmm8
            0x18004d4c1      f30f59c8       mulss xmm1, xmm0
            0x18004d4c5      f3420f1084..   movss xmm0, dword [rbp + r14*4 + 0xf0]
            0x18004d4cf      f3410f59c0     mulss xmm0, xmm8
            0x18004d4d4      f3440f59e6     mulss xmm12, xmm6
            0x18004d4d9      f3420f118c..   movss dword [rbp + r14*4 + 0x140], xmm1
            0x18004d4e3      f3410f58c4     addss xmm0, xmm12
            0x18004d4e8      440f28e0       movaps xmm12, xmm0
            0x18004d4ec      440f57252c..   xorps xmm12, xmmword [0x18032e820]
            0x18004d4f4      f3440f5fe0     maxss xmm12, xmm0
            0x18004d4f9      450f2fd4       comiss xmm10, xmm12
            0x18004d4fd      760c           jbe 0x18004d50b
            0x18004d4ff      f3460f118c..   movss dword [rbp + r14*4 + 0xf0], xmm9
            0x18004d509      eb0a           jmp 0x18004d515
            0x18004d50b      f3420f1184..   movss dword [rbp + r14*4 + 0xf0], xmm0
            0x18004d515      41ffc5         inc r13d
            0x18004d518      4589ee         mov r14d, r13d
            0x18004d51b      4183fd14       cmp r13d, 0x14             ; 20
            0x18004d51f      0f825afeffff   jb 0x18004d37f
            0x18004de30      4156           push r14
            0x18004de32      4157           push r15
            0x18004de34      4883ec28       sub rsp, 0x28
            0x18004de38      4989cf         mov r15, rcx
            0x18004de3b      4989d6         mov r14, rdx
            0x18004de3e      458907         mov dword [r15], r8d
            0x18004de41      4585c0         test r8d, r8d
        ┌─< 0x18004de44      0f840a010000   je 0x18004df54
        │   0x18004de4a      4c89f1         mov rcx, r14
        │   0x18004de4d      33d2           xor edx, edx
        │   0x18004de4f      e8ccd20000     call 0x18005b120
        │   0x18004de54      89c0           mov eax, eax
        │   0x18004de56      4c89f1         mov rcx, r14
        │   0x18004de59      4d8b4608       mov r8, qword [r14 + 8]
        │   0x18004de5d      4989c3         mov r11, rax
        │   0x18004de60      f30f100de8..   movss xmm1, dword [0x18032e850] ; [0x18032e850:4]=0x3f3504f3
        │   0x18004de68      49c1e304       shl r11, 4
        │   0x18004de6c      458b0cc0       mov r9d, dword [r8 + rax*8]
        │   0x18004de70      4183f908       cmp r9d, 8                 ; 8
        │   0x18004de74      45894f10       mov dword [r15 + 0x10], r9d
        │   0x18004de78      458b54c004     mov r10d, dword [r8 + rax*8 + 4]
       ┌──< 0x18004de7d      7408           je 0x18004de87
       ││   0x18004de7f      f30f100dcd..   movss xmm1, dword [0x18032e854] ; [0x18032e854:4]=0x3f800000
       └──> 0x18004de87      4183fa08       cmp r10d, 8                ; 8
        │   0x18004de8b      f30f101dbd..   movss xmm3, dword [0x18032e850] ; [0x18032e850:4]=0x3f3504f3
        │   0x18004de93      498b16         mov rdx, qword [r14]
       ┌──< 0x18004de96      7408           je 0x18004dea0
       ││   0x18004de98      f30f101db4..   movss xmm3, dword [0x18032e854] ; [0x18032e854:4]=0x3f800000
       └──> 0x18004dea0      f3410f114f18   movss dword [r15 + 0x18], xmm1
        │   0x18004dea6      f3410f115f1c   movss dword [r15 + 0x1c], xmm3
        │   0x18004deac      f3410f100413   movss xmm0, dword [r11 + rdx]
        │   0x18004deb2      f30f59c8       mulss xmm1, xmm0
        │   0x18004deb6      f3410f114f18   movss dword [r15 + 0x18], xmm1
        │   0x18004debc      f3410f1054..   movss xmm2, dword [r11 + rdx + 4]
        │   0x18004dec3      baff010000     mov edx, 0x1ff             ; 511
        │   0x18004dec8      f30f59da       mulss xmm3, xmm2
        │   0x18004decc      45895714       mov dword [r15 + 0x14], r10d
        │   0x18004ded0      f3410f115f1c   movss dword [r15 + 0x1c], xmm3
        │   0x18004ded6      e845d20000     call 0x18005b120
        │   0x18004dedb      89c0           mov eax, eax
        │   0x18004dedd      4d8b0e         mov r9, qword [r14]
        │   0x18004dee0      4989c0         mov r8, rax
        │   0x18004dee3      4d8b7608       mov r14, qword [r14 + 8]
        │   0x18004dee7      f30f100d61..   movss xmm1, dword [0x18032e850] ; [0x18032e850:4]=0x3f3504f3
        │   0x18004deef      f30f101d59..   movss xmm3, dword [0x18032e850] ; [0x18032e850:4]=0x3f3504f3
        │   0x18004def7      418b14c6       mov edx, dword [r14 + rax*8]
        │   0x18004defb      83fa08         cmp edx, 8                 ; 8
        │   0x18004defe      41895720       mov dword [r15 + 0x20], edx
        │   0x18004df02      418b4cc604     mov ecx, dword [r14 + rax*8 + 4]
       ┌──< 0x18004df07      7408           je 0x18004df11
       ││   0x18004df09      f30f100d43..   movss xmm1, dword [0x18032e854] ; [0x18032e854:4]=0x3f800000
       └──> 0x18004df11      83f908         cmp ecx, 8                 ; 8
        │   0x18004df14      f3410f114f28   movss dword [r15 + 0x28], xmm1
       ┌──< 0x18004df1a      7408           je 0x18004df24
       ││   0x18004df1c      f30f101d30..   movss xmm3, dword [0x18032e854] ; [0x18032e854:4]=0x3f800000
       └──> 0x18004df24      49c1e004       shl r8, 4
        │   0x18004df28      f3410f115f2c   movss dword [r15 + 0x2c], xmm3
        │   0x18004df2e      41894f24       mov dword [r15 + 0x24], ecx
        │   0x18004df32      f3430f1044..   movss xmm0, dword [r8 + r9 + 8]
        │   0x18004df39      f30f59c8       mulss xmm1, xmm0
        │   0x18004df3d      f3410f114f28   movss dword [r15 + 0x28], xmm1
        │   0x18004df43      f3430f1054..   movss xmm2, dword [r8 + r9 + 0xc]
        │   0x18004df4a      f30f59da       mulss xmm3, xmm2
        │   0x18004df4e      f3410f115f2c   movss dword [r15 + 0x2c], xmm3
        └─> 0x18004df54      4883c428       add rsp, 0x28
            0x18004df58      415f           pop r15
            0x18004df5a      415e           pop r14
            0x18004df5c      c3             ret
            0x18004df5d      0f1f00         nop dword [rax]
            0x18004df60      53             push rbx
            0x18004df61      56             push rsi
            0x18004df62      57             push rdi
            0x18004df63      4157           push r15
            0x18004df65      55             push rbp
            0x18004df66      4883ec30       sub rsp, 0x30
            0x18004df6a      4d89c7         mov r15, r8
            0x18004df6d      4889e0         mov rax, rsp
            0x18004df70      4533c0         xor r8d, r8d
            0x18004df73      bff304353f     mov edi, 0x3f3504f3
            0x18004df78      4889d6         mov rsi, rdx
            0x18004df7b      4889cb         mov rbx, rcx
            0x18004df7e      418b2f         mov ebp, dword [r15]
            0x18004df81      4189e9         mov r9d, ebp
            0x18004df84      c740200000..   mov dword [rax + 0x20], 0
            0x18004df8b      897828         mov dword [rax + 0x28], edi
            0x18004df8e      e82d000000     call 0x18004dfc0
            0x18004df93      4889e0         mov rax, rsp
            0x18004df96      4889d9         mov rcx, rbx
            0x18004df99      4889f2         mov rdx, rsi
            0x18004df9c      4189e8         mov r8d, ebp
