            0x18005a9f0      53             push rbx
            0x18005a9f1      56             push rsi
            0x18005a9f2      57             push rdi
            0x18005a9f3      4154           push r12
            0x18005a9f5      4155           push r13
            0x18005a9f7      4156           push r14
            0x18005a9f9      4157           push r15
            0x18005a9fb      55             push rbp
            0x18005a9fc      4883ec38       sub rsp, 0x38
            0x18005aa00      4889cf         mov rdi, rcx
            0x18005aa03      4989d0         mov r8, rdx
            0x18005aa06      4533e4         xor r12d, r12d
            0x18005aa09      48897c2428     mov qword [rsp + 0x28], rdi
            0x18005aa0e      4533d2         xor r10d, r10d
            0x18005aa11      488d2de855..   lea rbp, [0x180000000]
            0x18005aa18      448b8f5033..   mov r9d, dword [rdi + 0x3350]
            0x18005aa1f      488d9f7031..   lea rbx, [rdi + 0x3170]
            0x18005aa26      498b10         mov rdx, qword [r8]
            0x18005aa29      498b7008       mov rsi, qword [r8 + 8]
            0x18005aa2d      4d8b6810       mov r13, qword [r8 + 0x10]
            0x18005aa31      428d048d00..   lea eax, [r9*4]
            0x18005aa39      498b4818       mov rcx, qword [r8 + 0x18]
            0x18005aa3d      4c8d1c87       lea r11, [rdi + rax*4]
            0x18005aa41      4d8b4020       mov r8, qword [r8 + 0x20]
            0x18005aa45      4c89d8         mov rax, r11
            0x18005aa48      4889542420     mov qword [rsp + 0x20], rdx
        ┌─> 0x18005aa4d      420fb69425..   movzx edx, byte [rbp + r12 + 0x338220]
        ╎   0x18005aa56      420fb6bc25..   movzx edi, byte [rbp + r12 + 0x338280]
        ╎   0x18005aa5f      460fb6a425..   movzx r12d, byte [rbp + r12 + 0x3382e0]
        ╎   0x18005aa68      41c1e402       shl r12d, 2
        ╎   0x18005aa6c      448d349500..   lea r14d, [rdx*4]
        ╎   0x18005aa74      f3430f1004b3   movss xmm0, dword [r11 + r14*4]
        ╎   0x18005aa7a      8d14950200..   lea edx, [rdx*4 + 2]
        ╎   0x18005aa81      f3410f100c93   movss xmm1, dword [r11 + rdx*4]
        ╎   0x18005aa87      448d3cbd01..   lea r15d, [rdi*4 + 1]
        ╎   0x18005aa8f      8d3cbd0300..   lea edi, [rdi*4 + 3]
        ╎   0x18005aa96      f3430f5804bb   addss xmm0, dword [r11 + r15*4]
        ╎   0x18005aa9c      f3410f580cbb   addss xmm1, dword [r11 + rdi*4]
        ╎   0x18005aaa2      4f8d1ca3       lea r11, [r11 + r12*4]
        ╎   0x18005aaa6      493bdb         cmp rbx, r11
        ╎   0x18005aaa9      478d3412       lea r14d, [r10 + r10]
        ╎   0x18005aaad      f3430f1144..   movss dword [r13 + r14*4], xmm0
        ╎   0x18005aab4      438d541201     lea edx, [r10 + r10 + 1]
        ╎   0x18005aab9      f3410f114c..   movss dword [r13 + rdx*4], xmm1
        ╎   0x18005aac0      4d8da390ce..   lea r12, [r11 - 0x3170]
        ╎   0x18005aac7      4d0f46dc       cmovbe r11, r12
        ╎   0x18005aacb      41ffc2         inc r10d
        ╎   0x18005aace      4589d4         mov r12d, r10d
        ╎   0x18005aad1      4183fa4d       cmp r10d, 0x4d             ; 'M' ; 77
        └─< 0x18005aad5      0f8272ffffff   jb 0x18005aa4d
            0x18005aadb      4989c4         mov r12, rax
            0x18005aade      488b542420     mov rdx, qword [rsp + 0x20]
            0x18005aae3      4533db         xor r11d, r11d
            0x18005aae6      4533d2         xor r10d, r10d
            0x18005aae9      488d3d1055..   lea rdi, [0x180000000]
        ┌─> 0x18005aaf0      420fb6ac1f..   movzx ebp, byte [rdi + r11 + 0x338340]
        ╎   0x18005aaf9      460fb6ac1f..   movzx r13d, byte [rdi + r11 + 0x338220]
        ╎   0x18005ab02      460fb69c1f..   movzx r11d, byte [rdi + r11 + 0x3382e0]
        ╎   0x18005ab0b      41c1e302       shl r11d, 2
        ╎   0x18005ab0f      448d34ad00..   lea r14d, [rbp*4]
        ╎   0x18005ab17      f3430f1004b4   movss xmm0, dword [r12 + r14*4]
        ╎   0x18005ab1d      8d2cad0200..   lea ebp, [rbp*4 + 2]
        ╎   0x18005ab24      f3410f100cac   movss xmm1, dword [r12 + rbp*4]
        ╎   0x18005ab2a      468d3cad01..   lea r15d, [r13*4 + 1]
        ╎   0x18005ab32      468d2cad03..   lea r13d, [r13*4 + 3]
        ╎   0x18005ab3a      f3430f5804bc   addss xmm0, dword [r12 + r15*4]
        ╎   0x18005ab40      f3430f580cac   addss xmm1, dword [r12 + r13*4]
        ╎   0x18005ab46      4f8d249c       lea r12, [r12 + r11*4]
        ╎   0x18005ab4a      493bdc         cmp rbx, r12
        ╎   0x18005ab4d      478d3412       lea r14d, [r10 + r10]
        ╎   0x18005ab51      f3420f1104b1   movss dword [rcx + r14*4], xmm0
        ╎   0x18005ab57      438d6c1201     lea ebp, [r10 + r10 + 1]
        ╎   0x18005ab5c      f30f110ca9     movss dword [rcx + rbp*4], xmm1
        ╎   0x18005ab61      4d8d9c2490..   lea r11, [r12 - 0x3170]
        ╎   0x18005ab69      4d0f46e3       cmovbe r12, r11
        ╎   0x18005ab6d      41ffc2         inc r10d
        ╎   0x18005ab70      4589d3         mov r11d, r10d
        ╎   0x18005ab73      4183fa4d       cmp r10d, 0x4d             ; 'M' ; 77
        └─< 0x18005ab77      0f8273ffffff   jb 0x18005aaf0
            0x18005ab7d      488b7c2428     mov rdi, qword [rsp + 0x28]
            0x18005ab82      4533d2         xor r10d, r10d
            0x18005ab85      4533ed         xor r13d, r13d
            0x18005ab88      4c8d1d7154..   lea r11, [0x180000000]
        ┌─> 0x18005ab8f      430fb68c13..   movzx ecx, byte [r11 + r10 + 0x338280]
        ╎   0x18005ab98      470fb6a413..   movzx r12d, byte [r11 + r10 + 0x338340]
        ╎   0x18005aba1      470fb69413..   movzx r10d, byte [r11 + r10 + 0x3382e0]
        ╎   0x18005abaa      41c1e202       shl r10d, 2
        ╎   0x18005abae      448d348d00..   lea r14d, [rcx*4]
        ╎   0x18005abb6      f3420f1004b0   movss xmm0, dword [rax + r14*4]
        ╎   0x18005abbc      8d0c8d0200..   lea ecx, [rcx*4 + 2]
        ╎   0x18005abc3      f30f100c88     movss xmm1, dword [rax + rcx*4]
        ╎   0x18005abc8      468d3ca501..   lea r15d, [r12*4 + 1]
        ╎   0x18005abd0      468d24a503..   lea r12d, [r12*4 + 3]
        ╎   0x18005abd8      f3420f5804b8   addss xmm0, dword [rax + r15*4]
        ╎   0x18005abde      f3420f580ca0   addss xmm1, dword [rax + r12*4]
        ╎   0x18005abe4      4a8d0490       lea rax, [rax + r10*4]
        ╎   0x18005abe8      483bd8         cmp rbx, rax
        ╎   0x18005abeb      478d742d00     lea r14d, [r13 + r13]
        ╎   0x18005abf0      f3430f1104b0   movss dword [r8 + r14*4], xmm0
        ╎   0x18005abf6      438d4c2d01     lea ecx, [r13 + r13 + 1]
        ╎   0x18005abfb      f3410f110c88   movss dword [r8 + rcx*4], xmm1
        ╎   0x18005ac01      4c8d9090ce..   lea r10, [rax - 0x3170]
        ╎   0x18005ac08      490f46c2       cmovbe rax, r10
        ╎   0x18005ac0c      41ffc5         inc r13d
        ╎   0x18005ac0f      4589ea         mov r10d, r13d
        ╎   0x18005ac12      4183fd4d       cmp r13d, 0x4d             ; 'M' ; 77
        └─< 0x18005ac16      0f8273ffffff   jb 0x18005ab8f
            0x18005ac1c      b816030000     mov eax, 0x316             ; 790
            0x18005ac21      4585c9         test r9d, r9d
            0x18005ac24      458d59ff       lea r11d, [r9 - 1]
            0x18005ac28      410f47c3       cmova eax, r11d
            0x18005ac2c      4533c0         xor r8d, r8d
            0x18005ac2f      4c8d9fe001..   lea r11, [rdi + 0x1e0]
            0x18005ac36      898750330000   mov dword [rdi + 0x3350], eax ; [0x3350:4]=-1
            0x18005ac3c      4c8d0dbd53..   lea r9, [0x180000000]
            0x18005ac43      c1e002         shl eax, 2
            0x18005ac46      4c8d1487       lea r10, [rdi + rax*4]
        ┌─> 0x18005ac4a      438d0c00       lea ecx, [r8 + r8]
        ╎   0x18005ac4e      8b048a         mov eax, dword [rdx + rcx*4]
        ╎   0x18005ac51      418902         mov dword [r10], eax
        ╎   0x18005ac54      438d440001     lea eax, [r8 + r8 + 1]
        ╎   0x18005ac59      448b248e       mov r12d, dword [rsi + rcx*4]
        ╎   0x18005ac5d      45896204       mov dword [r10 + 4], r12d
        ╎   0x18005ac61      448b2c82       mov r13d, dword [rdx + rax*4]
        ╎   0x18005ac65      45896a08       mov dword [r10 + 8], r13d
        ╎   0x18005ac69      430fb6bc01..   movzx edi, byte [r9 + r8 + 0x3382e0]
        ╎   0x18005ac72      c1e702         shl edi, 2
        ╎   0x18005ac75      448b2486       mov r12d, dword [rsi + rax*4]
        ╎   0x18005ac79      4589620c       mov dword [r10 + 0xc], r12d
        ╎   0x18005ac7d      4d3bd3         cmp r10, r11
       ┌──< 0x18005ac80      732a           jae 0x18005acac
       │╎   0x18005ac82      448b248a       mov r12d, dword [rdx + rcx*4]
       │╎   0x18005ac86      4589a27031..   mov dword [r10 + 0x3170], r12d ; [0x3170:4]=-1
       │╎   0x18005ac8d      8b0c8e         mov ecx, dword [rsi + rcx*4]
       │╎   0x18005ac90      41898a7431..   mov dword [r10 + 0x3174], ecx ; [0x3174:4]=-1
       │╎   0x18005ac97      448b2c82       mov r13d, dword [rdx + rax*4]
       │╎   0x18005ac9b      4589aa7831..   mov dword [r10 + 0x3178], r13d ; [0x3178:4]=-1
       │╎   0x18005aca2      8b0486         mov eax, dword [rsi + rax*4]
       │╎   0x18005aca5      4189827c31..   mov dword [r10 + 0x317c], eax ; [0x317c:4]=-1
       └──> 0x18005acac      41ffc0         inc r8d
        ╎   0x18005acaf      4d8d14ba       lea r10, [r10 + rdi*4]
        ╎   0x18005acb3      493bda         cmp rbx, r10
        ╎   0x18005acb6      498d8290ce..   lea rax, [r10 - 0x3170]
        ╎   0x18005acbd      4c0f46d0       cmovbe r10, rax
        ╎   0x18005acc1      4183f84d       cmp r8d, 0x4d              ; 'M' ; 77
        └─< 0x18005acc5      7283           jb 0x18005ac4a
            0x18005acc7      4883c438       add rsp, 0x38
            0x18005accb      5d             pop rbp
            0x18005accc      415f           pop r15
            0x18005acce      415e           pop r14
            0x18005acd0      415d           pop r13
            0x18005acd2      415c           pop r12
            0x18005acd4      5f             pop rdi
            0x18005acd5      5e             pop rsi
            0x18005acd6      5b             pop rbx
            0x18005acd7      c3             ret
            0x18005acd8      0f1f840000..   nop dword [rax + rax]
            0x18005b100      4883ec28       sub rsp, 0x28
            0x18005b104      33d2           xor edx, edx
            0x18005b106      41b850330000   mov r8d, 0x3350            ; 'P3'
            0x18005b10c      c781503300..   mov dword [rcx + 0x3350], 0 ; [0x3350:4]=-1
            0x18005b116      e875770200     call 0x180082890
            0x18005b11b      4883c428       add rsp, 0x28
            0x18005b11f      c3             ret
            0x18005b120      4189d0         mov r8d, edx
            0x18005b123      4889ca         mov rdx, rcx
            0x18005b126      33c0           xor eax, eax
            0x18005b128      8b4a18         mov ecx, dword [rdx + 0x18]
            0x18005b12b      85c9           test ecx, ecx
        ┌─< 0x18005b12d      7614           jbe 0x18005b143
        │   0x18005b12f      488b5210       mov rdx, qword [rdx + 0x10]
        │   0x18005b133      443b0482       cmp r8d, dword [rdx + rax*4]
       ┌──< 0x18005b137      7304           jae 0x18005b13d
       ││   0x18005b139      ffc0           inc eax
      ┌───< 0x18005b13b      eb06           jmp 0x18005b143
      │└──> 0x18005b13d      ffc0           inc eax
      │ │   0x18005b13f      3b             invalid
