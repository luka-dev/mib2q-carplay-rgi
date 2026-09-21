            0x18004e1c0      53             push rbx
            0x18004e1c1      56             push rsi
            0x18004e1c2      57             push rdi
            0x18004e1c3      4154           push r12
            0x18004e1c5      4155           push r13
            0x18004e1c7      4156           push r14
            0x18004e1c9      4883ec58       sub rsp, 0x58
            0x18004e1cd      4989d2         mov r10, rdx
            0x18004e1d0      f30f101584..   movss xmm2, dword [0x18032e85c] ; [0x18032e85c:4]=0x40000000
            0x18004e1d8      660fefc9       pxor xmm1, xmm1
            0x18004e1dc      f30f10057c..   movss xmm0, dword [0x18032e860] ; [0x18032e860:4]=0x47000000
            0x18004e1e4      4989c9         mov r9, rcx
            0x18004e1e7      33c9           xor ecx, ecx
            0x18004e1e9      ba0080ffff     mov edx, 0xffff8000
            0x18004e1ee      b8ff7f0000     mov eax, 0x7fff
            0x18004e1f3      4189d5         mov r13d, edx
            0x18004e1f6      0f11742440     movups xmmword [rsp + 0x40], xmm6
            0x18004e1fb      0f28f0         movaps xmm6, xmm0
            0x18004e1fe      0f117c2430     movups xmmword [rsp + 0x30], xmm7
            0x18004e203      0f28fa         movaps xmm7, xmm2
            0x18004e206      440f11442420   movups xmmword [rsp + 0x20], xmm8
            0x18004e20c      440f28c1       movaps xmm8, xmm1
            0x18004e210      89c3           mov ebx, eax
            0x18004e212      4889cf         mov rdi, rcx
            0x18004e215      4d89c4         mov r12, r8
            0x18004e218      4c89ce         mov rsi, r9
            0x18004e21b      4d89d6         mov r14, r10
        ┌─> 0x18004e21e      f3410f1004be   movss xmm0, dword [r14 + rdi*4]
        ╎   0x18004e224      f30f59c7       mulss xmm0, xmm7
        ╎   0x18004e228      f30f100cbe     movss xmm1, dword [rsi + rdi*4]
        ╎   0x18004e22d      410f2ec0       ucomiss xmm0, xmm8
       ┌──< 0x18004e231      7510           jne 0x18004e243
      ┌───< 0x18004e233      7a0e           jp 0x18004e243
      ││╎   0x18004e235      410f2ec8       ucomiss xmm1, xmm8
     ┌────< 0x18004e239      7508           jne 0x18004e243
    ┌─────< 0x18004e23b      7a06           jp 0x18004e243
    ││││╎   0x18004e23d      410f28d8       movaps xmm3, xmm8
   ┌──────< 0x18004e241      eb08           jmp 0x18004e24b
   │└└└└──> 0x18004e243      e808cf0000     call 0x18005b150
   │    ╎   0x18004e248      0f28d8         movaps xmm3, xmm0
   └──────> 0x18004e24b      440f2fc3       comiss xmm8, xmm3
        ╎   0x18004e24f      f30f10050d..   movss xmm0, dword [0x18032e864] ; [0x18032e864:4]=0x3f800000
        ╎   0x18004e257      f30f58c3       addss xmm0, xmm3
       ┌──< 0x18004e25b      7703           ja 0x18004e260
       │╎   0x18004e25d      0f28c3         movaps xmm0, xmm3
       └──> 0x18004e260      f30f59c6       mulss xmm0, xmm6
        ╎   0x18004e264      e8f7c50100     call 0x18006a860
        ╎   0x18004e269      3d0080ffff     cmp eax, 0xffff8000
        ╎   0x18004e26e      410f4cc5       cmovl eax, r13d
        ╎   0x18004e272      3dff7f0000     cmp eax, 0x7fff
        ╎   0x18004e277      0f4dc3         cmovge eax, ebx
        ╎   0x18004e27a      4c0fbfd8       movsx r11, ax
        ╎   0x18004e27e      41c1fb05       sar r11d, 5
        ╎   0x18004e282      45891cbc       mov dword [r12 + rdi*4], r11d
        ╎   0x18004e286      48ffc7         inc rdi
        ╎   0x18004e289      4883ff14       cmp rdi, 0x14              ; 20
        └─< 0x18004e28d      7c8f           jl 0x18004e21e
            0x18004e28f      0f10742440     movups xmm6, xmmword [rsp + 0x40]
            0x18004e294      0f107c2430     movups xmm7, xmmword [rsp + 0x30]
            0x18004e299      440f10442420   movups xmm8, xmmword [rsp + 0x20]
            0x18004e29f      4883c458       add rsp, 0x58
            0x18004e2a3      415e           pop r14
            0x18004e2a5      415d           pop r13
            0x18004e2a7      415c           pop r12
            0x18004e2a9      5f             pop rdi
            0x18004e2aa      5e             pop rsi
            0x18004e2ab      5b             pop rbx
            0x18004e2ac      c3             ret
            0x18004e2ad      0f1f00         nop dword [rax]
            0x18004e2b0      89d2           mov edx, edx
            0x18004e2b2      418b1490       mov edx, dword [r8 + rdx*4]
        ┌─< 0x18004e2b6      e965ce0000     jmp 0x18005b120
        │   0x18004e2bb      0f1f440000     nop dword [rax + rax]
        │   0x18004e2c0      53             push rbx
        │   0x18004e2c1      56             push rsi
        │   0x18004e2c2      57             push rdi
        │   0x18004e2c3      4154           push r12
        │   0x18004e2c5      4155           push r13
        │   0x18004e2c7      4156           push r14
        │   0x18004e2c9      4157           push r15
        │   0x18004e2cb      55             push rbp
        │   0x18004e2cc      4881ec9800..   sub rsp, 0x98
        │   0x18004e2d3      4989cb         mov r11, rcx
        │   0x18004e2d6      f30f101da2..   movss xmm3, dword [0x18032e880] ; [0x18032e880:4]=0x3456bf95
        │   0x18004e2de      33c9           xor ecx, ecx
        │   0x18004e2e0      f30f10159c..   movss xmm2, dword [0x18032e884] ; [0x18032e884:4]=0x3f000000
        │   0x18004e2e8      33c0           xor eax, eax
        │   0x18004e2ea      f30f100d96..   movss xmm1, dword [0x18032e888] ; [0x18032e888:4]=0x3e800000
        │   0x18004e2f2      89c3           mov ebx, eax
        │   0x18004e2f4      f30f100590..   movss xmm0, dword [0x18032e88c] ; [0x18032e88c:4]=0xbf000000
        │   0x18004e2fc      4989d7         mov r15, rdx
        │   0x18004e2ff      4c8b942400..   mov r10, qword [rsp + 0x100]
        │   0x18004e307      4889ce         mov rsi, rcx
        │   0x18004e30a      4c8bb42408..   mov r14, qword [rsp + 0x108]
        │   0x18004e312      4c89c7         mov rdi, r8
        │   0x18004e315      0f11b42480..   movups xmmword [rsp + 0x80], xmm6
        │   0x18004e31d      0f28f0         movaps xmm6, xmm0
        │   0x18004e320      0f117c2470     movups xmmword [rsp + 0x70], xmm7
        │   0x18004e325      0f28fa         movaps xmm7, xmm2
        │   0x18004e328      440f11442460   movups xmmword [rsp + 0x60], xmm8
        │   0x18004e32e      440f28c3       movaps xmm8, xmm3
        │   0x18004e332      440f114c2450   movups xmmword [rsp + 0x50], xmm9
        │   0x18004e338      440f28c9       movaps xmm9, xmm1
        │   0x18004e33c      440f11542440   movups xmmword [rsp + 0x40], xmm10
        │   0x18004e342      4c89cd         mov rbp, r9
        │   0x18004e345      440f115c2430   movups xmmword [rsp + 0x30], xmm11
        │   0x18004e34b      4d89d5         mov r13, r10
        │   0x18004e34e      440f11642420   movups xmmword [rsp + 0x20], xmm12
        │   0x18004e354      4d89dc         mov r12, r11
        │   0x18004e357      4c89f9         mov rcx, r15
        │   0x18004e35a      89da           mov edx, ebx
        │   0x18004e35c      4d89f0         mov r8, r14
        │   0x18004e35f      41c784f430..   mov dword [r12 + rsi*8 + 0x230], 0
        │   0x18004e36b      41c784f434..   mov dword [r12 + rsi*8 + 0x234], 1
        │   0x18004e377      e834ffffff     call 0x18004e2b0
        │   0x18004e37c      f30f102cb7     movss xmm5, dword [rdi + rsi*4]
        │   0x18004e381      ffc3           inc ebx
        │   0x18004e383      f3440f1025..   movss xmm12, dword [0x18032e890] ; [0x18032e890:4]=0x3f800000
        │   0x18004e38c      f3410f58e8     addss xmm5, xmm8
        │   0x18004e391      f3440f5ee5     divss xmm12, xmm5
        │   0x18004e396      f3410f1064..   movss xmm4, dword [r13 + rsi*4]
        │   0x18004e39d      f3440f1054..   movss xmm10, dword [rbp + rsi*4]
        │   0x18004e3a4      f3410f59e4     mulss xmm4, xmm12
        │   0x18004e3a9      f3450f59e2     mulss xmm12, xmm10
        │   0x18004e3ae      f30f59e7       mulss xmm4, xmm7
        │   0x18004e3b2      f3450f59e1     mulss xmm12, xmm9
        │   0x18004e3b7      0f28dc         movaps xmm3, xmm4
        │   0x18004e3ba      450f28dc       movaps xmm11, xmm12
        │   0x18004e3be      f30f59dc       mulss xmm3, xmm4
        │   0x18004e3c2      f3450f59dc     mulss xmm11, xmm12
        │   0x18004e3c7      89c1           mov ecx, eax
        │   0x18004e3c9      f3410f58db     addss xmm3, xmm11
        │   0x18004e3ce      f30f51db       sqrtss xmm3, xmm3
        │   0x18004e3d2      4989c9         mov r9, rcx
        │   0x18004e3d5      0f28d3         movaps xmm2, xmm3
        │   0x18004e3d8      49c1e104       shl r9, 4
        │   0x18004e3dc      f3410f58d4     addss xmm2, xmm12
        │   0x18004e3e1      f3410f5cdc     subss xmm3, xmm12
        │   0x18004e3e6      4d8b17         mov r10, qword [r15]
        │   0x18004e3e9      498b5708       mov rdx, qword [r15 + 8]
        │   0x18004e3ed      f3430f1044..   movss xmm0, dword [r9 + r10 + 8]
        │   0x18004e3f4      f3430f100c11   movss xmm1, dword [r9 + r10]
        │   0x18004e3fa      440f28d0       movaps xmm10, xmm0
        │   0x18004e3fe      0f28e9         movaps xmm5, xmm1
        │   0x18004e401      f3440f59d4     mulss xmm10, xmm4
        │   0x18004e406      f30f59ea       mulss xmm5, xmm2
        │   0x18004e40a      f30f59cc       mulss xmm1, xmm4
        │   0x18004e40e      f30f59c3       mulss xmm0, xmm3
        │   0x18004e412      f3440f58d5     addss xmm10, xmm5
        │   0x18004e417      f30f58c8       addss xmm1, xmm0
        │   0x18004e41b      f3450f1114f4   movss dword [r12 + rsi*8], xmm10
        │   0x18004e421      f3470f1054..   movss xmm10, dword [r9 + r10 + 0xc]
        │   0x18004e428      f3470f1064..   movss xmm12, dword [r9 + r10 + 4]
        │   0x18004e42f      410f28ea       movaps xmm5, xmm10
        │   0x18004e433      450f28dc       movaps xmm11, xmm12
        │   0x18004e437      f30f59ec       mulss xmm5, xmm4
        │   0x18004e43b      f3440f59da     mulss xmm11, xmm2
        │   0x18004e440      f30f58d6       addss xmm2, xmm6
        │   0x18004e444      f3440f59e4     mulss xmm12, xmm4
        │   0x18004e449      f3440f59d3     mulss xmm10, xmm3
        │   0x18004e44e      f30f58de       addss xmm3, xmm6
        │   0x18004e452      f3410f58eb     addss xmm5, xmm11
        │   0x18004e457      f3450f58e2     addss xmm12, xmm10
        │   0x18004e45c      8b04ca         mov eax, dword [rdx + rcx*8]
        │   0x18004e45f      418984f430..   mov dword [r12 + rsi*8 + 0x230], eax
        │   0x18004e467      448b44ca04     mov r8d, dword [rdx + rcx*8 + 4]
        │   0x18004e46c      458984f434..   mov dword [r12 + rsi*8 + 0x234], r8d
        │   0x18004e474      f3410f116c..   movss dword [r12 + rsi*8 + 4], xmm5
        │   0x18004e47b      f3410f118c..   movss dword [r12 + rsi*8 + 0xa0], xmm1
        │   0x18004e485      f3450f11a4..   movss dword [r12 + rsi*8 + 0xa4], xmm12
        │   0x18004e48f      488d3476       lea rsi, [rsi + rsi*2]
        │   0x18004e493      0f5715d603..   xorps xmm2, xmmword [0x18032e870]
        │   0x18004e49a      0f571dcf03..   xorps xmm3, xmmword [0x18032e870]
        │   0x18004e4a1      0f5725c803..   xorps xmm4, xmmword [0x18032e870]
        │   0x18004e4a8      f3410f1194..   movss dword [r12 + rsi*4 + 0x140], xmm2
        │   0x18004e4b2      f3410f119c..   movss dword [r12 + rsi*4 + 0x144], xmm3
        │   0x18004e4bc      f3410f11a4..   movss dword [r12 + rsi*4 + 0x148], xmm4
        │   0x18004e4c6      89de           mov esi, ebx
        │   0x18004e4c8      83fb14         cmp ebx, 0x14              ; 20
        │   0x18004e4cb      0f8286feffff   jb 0x18004e357
