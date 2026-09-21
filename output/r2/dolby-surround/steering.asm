            0x18004d525      0f10b424b0..   movups xmm6, xmmword [rsp + 0xb0]
            0x18004d52d      0f10bc24a0..   movups xmm7, xmmword [rsp + 0xa0]
            0x18004d535      440f108424..   movups xmm8, xmmword [rsp + 0x90]
            0x18004d53e      440f108c24..   movups xmm9, xmmword [rsp + 0x80]
            0x18004d547      440f10542470   movups xmm10, xmmword [rsp + 0x70]
            0x18004d54d      440f105c2460   movups xmm11, xmmword [rsp + 0x60]
            0x18004d553      440f10642450   movups xmm12, xmmword [rsp + 0x50]
            0x18004d559      440f106c2440   movups xmm13, xmmword [rsp + 0x40]
            0x18004d55f      440f10742430   movups xmm14, xmmword [rsp + 0x30]
            0x18004d565      4881c4e801..   add rsp, 0x1e8
            0x18004d56c      5d             pop rbp
            0x18004d56d      415f           pop r15
            0x18004d56f      415e           pop r14
            0x18004d571      415d           pop r13
            0x18004d573      c3             ret
            0x18004d574      0f1f440000     nop dword [rax + rax]
            0x18004d579      0f1f800000..   nop dword [rax]
            0x18004d580      4989ca         mov r10, rcx
            0x18004d583      498d824001..   lea rax, [r10 + 0x140]
            0x18004d58a      4883e00f       and rax, 0xf               ; 15
            0x18004d58e      89c0           mov eax, eax
            0x18004d590      85c0           test eax, eax
        ┌─< 0x18004d592      7433           je 0x18004d5c7
        │   0x18004d594      f7d8           neg eax
        │   0x18004d596      33c9           xor ecx, ecx
        │   0x18004d598      83c010         add eax, 0x10              ; 16
        │   0x18004d59b      33d2           xor edx, edx
        │   0x18004d59d      c1e802         shr eax, 2
       ┌──> 0x18004d5a0      41890c92       mov dword [r10 + rdx*4], ecx
       ╎│   0x18004d5a4      41894c9250     mov dword [r10 + rdx*4 + 0x50], ecx
       ╎│   0x18004d5a9      41898c92a0..   mov dword [r10 + rdx*4 + 0xa0], ecx
       ╎│   0x18004d5b1      41898c92f0..   mov dword [r10 + rdx*4 + 0xf0], ecx
       ╎│   0x18004d5b9      41898c9240..   mov dword [r10 + rdx*4 + 0x140], ecx
       ╎│   0x18004d5c1      ffc2           inc edx
       ╎│   0x18004d5c3      3bd0           cmp edx, eax
       └──< 0x18004d5c5      72d9           jb 0x18004d5a0
        └─> 0x18004d5c7      4189c0         mov r8d, eax
            0x18004d5ca      41f7d8         neg r8d
            0x18004d5cd      660fefc0       pxor xmm0, xmm0
            0x18004d5d1      4183c004       add r8d, 4
            0x18004d5d5      4183e007       and r8d, 7
            0x18004d5d9      41f7d8         neg r8d
            0x18004d5dc      458d4814       lea r9d, [r8 + 0x14]
            0x18004d5e0      8d5004         lea edx, [rax + 4]
            0x18004d5e3      410f110482     movups xmmword [r10 + rax*4], xmm0
            0x18004d5e8      410f11448250   movups xmmword [r10 + rax*4 + 0x50], xmm0
            0x18004d5ee      410f118482..   movups xmmword [r10 + rax*4 + 0xa0], xmm0
            0x18004d5f7      410f118482..   movups xmmword [r10 + rax*4 + 0xf0], xmm0
            0x18004d600      410f118482..   movups xmmword [r10 + rax*4 + 0x140], xmm0
            0x18004d609      83c008         add eax, 8
            0x18004d60c      410f110492     movups xmmword [r10 + rdx*4], xmm0
            0x18004d611      410f11449250   movups xmmword [r10 + rdx*4 + 0x50], xmm0
            0x18004d617      410f118492..   movups xmmword [r10 + rdx*4 + 0xa0], xmm0
            0x18004d620      410f118492..   movups xmmword [r10 + rdx*4 + 0xf0], xmm0
            0x18004d629      410f118492..   movups xmmword [r10 + rdx*4 + 0x140], xmm0
            0x18004d632      413bc1         cmp eax, r9d
            0x18004d635      72a9           jb 0x18004d5e0
            0x18004d637      418d4015       lea eax, [r8 + 0x15]
            0x18004d63b      83f814         cmp eax, 0x14              ; 20
            0x18004d63e      777b           ja 0x18004d6bb
            0x18004d640      41f7d9         neg r9d
            0x18004d643      4183c114       add r9d, 0x14              ; 20
            0x18004d647      4183f904       cmp r9d, 4                 ; 4
            0x18004d64b      726f           jb 0x18004d6bc
            0x18004d64d      4489c9         mov ecx, r9d
            0x18004d650      33d2           xor edx, edx
            0x18004d652      83e1fc         and ecx, 0xfffffffc        ; 4294967292
            0x18004d655      33c0           xor eax, eax
            0x18004d657      468d5c0214     lea r11d, [rdx + r8 + 0x14]
            0x18004d65c      83c204         add edx, 4
            0x18004d65f      430f11049a     movups xmmword [r10 + r11*4], xmm0
            0x18004d664      430f11449a50   movups xmmword [r10 + r11*4 + 0x50], xmm0
            0x18004d66a      430f11849a..   movups xmmword [r10 + r11*4 + 0xa0], xmm0
            0x18004d673      430f11849a..   movups xmmword [r10 + r11*4 + 0xf0], xmm0
            0x18004d67c      430f11849a..   movups xmmword [r10 + r11*4 + 0x140], xmm0
            0x18004d685      3bd1           cmp edx, ecx
            0x18004d687      72ce           jb 0x18004d657
            0x18004d689      413bc9         cmp ecx, r9d
            0x18004d68c      732d           jae 0x18004d6bb
            0x18004d68e      428d540114     lea edx, [rcx + r8 + 0x14]
            0x18004d693      ffc1           inc ecx
            0x18004d695      41890492       mov dword [r10 + rdx*4], eax
            0x18004d699      4189449250     mov dword [r10 + rdx*4 + 0x50], eax
            0x18004d69e      41898492a0..   mov dword [r10 + rdx*4 + 0xa0], eax
            0x18004d6a6      41898492f0..   mov dword [r10 + rdx*4 + 0xf0], eax
            0x18004d6ae      4189849240..   mov dword [r10 + rdx*4 + 0x140], eax
            0x18004d6b6      413bc9         cmp ecx, r9d
            0x18004d6b9      72d3           jb 0x18004d68e
            0x18004d6bb      c3             ret
            0x18004d6bc      33c9           xor ecx, ecx
            0x18004d6be      33c0           xor eax, eax
            0x18004d6c0      ebc7           jmp 0x18004d689
            0x18004d6c2      0f1f800000..   nop dword [rax]
            0x18004d6c9      0f1f800000..   nop dword [rax]
            0x18004d6d0      4154           push r12
            0x18004d6d2      4155           push r13
            0x18004d6d4      4156           push r14
            0x18004d6d6      4157           push r15
            0x18004d6d8      55             push rbp
            0x18004d6d9      4881ecf000..   sub rsp, 0xf0
            0x18004d6e0      4d89cd         mov r13, r9
            0x18004d6e3      448bbc2440..   mov r15d, dword [rsp + 0x140]
            0x18004d6eb      4d89c6         mov r14, r8
            0x18004d6ee      488bac2450..   mov rbp, qword [rsp + 0x150]
            0x18004d6f6      4989d4         mov r12, rdx
            0x18004d6f9      4183ff02       cmp r15d, 2                ; reloc.OLEAUT32.dll_SysReAllocString
            0x18004d6fd      0f868a050000   jbe 0x18004dc8d
            0x18004d703      83bc245801..   cmp dword [rsp + 0x158], 0x4d ; 'M'
            0x18004d70b      750a           jne 0x18004d717
            0x18004d70d      4c89f2         mov rdx, r14
            0x18004d710      e8dbd20000     call 0x18005a9f0
            0x18004d715      eb17           jmp 0x18004d72e
            0x18004d717      4881c15433..   add rcx, 0x3354
            0x18004d71e      4c89f2         mov rdx, r14
            0x18004d721      448b842458..   mov r8d, dword [rsp + 0x158]
            0x18004d729      e8b2d50000     call 0x18005ace0
            0x18004d72e      4183ff04       cmp r15d, 4                ; 4
            0x18004d732      0f84f0020000   je 0x18004da28
            0x18004d738      4183ff05       cmp r15d, 5                ; 5
            0x18004d73c      0f85a6060000   jne 0x18004dde8
            0x18004d742      48899c24c0..   mov qword [rsp + 0xc0], rbx
            0x18004d74a      4533c0         xor r8d, r8d
            0x18004d74d      0f11742460     movups xmmword [rsp + 0x60], xmm6
            0x18004d752      0f117c2470     movups xmmword [rsp + 0x70], xmm7
            0x18004d757      440f118424..   movups xmmword [rsp + 0x80], xmm8
            0x18004d760      440f118c24..   movups xmmword [rsp + 0x90], xmm9
            0x18004d769      440f119424..   movups xmmword [rsp + 0xa0], xmm10
            0x18004d772      440f119c24..   movups xmmword [rsp + 0xb0], xmm11
            0x18004d77b      440f11642420   movups xmmword [rsp + 0x20], xmm12
            0x18004d781      440f116c2430   movups xmmword [rsp + 0x30], xmm13
            0x18004d787      440f11742440   movups xmmword [rsp + 0x40], xmm14
            0x18004d78d      440f117c2450   movups xmmword [rsp + 0x50], xmm15
            0x18004d793      4889b424c8..   mov qword [rsp + 0xc8], rsi
            0x18004d79b      4889bc24d0..   mov qword [rsp + 0xd0], rdi
            0x18004d7a3      448b8c2458..   mov r9d, dword [rsp + 0x158]
            0x18004d7ab      8b9c244801..   mov ebx, dword [rsp + 0x148]
            0x18004d7b2      4885ed         test rbp, rbp
            0x18004d7b5      7409           je 0x18004d7c0
            0x18004d7b7      8b4500         mov eax, dword [rbp]
            0x18004d7ba      4883c504       add rbp, 4
            0x18004d7be      eb03           jmp 0x18004d7c3
            0x18004d7c0      4489c8         mov eax, r9d
            0x18004d7c3      33f6           xor esi, esi
            0x18004d7c5      85db           test ebx, ebx
            0x18004d7c7      0f86ec010000   jbe 0x18004d9b9
            0x18004d7cd      898424e800..   mov dword [rsp + 0xe8], eax
            0x18004d7d4      8d78ff         lea edi, [rax - 1]
            0x18004d7d7      4889ac24d8..   mov qword [rsp + 0xd8], rbp
            0x18004d7df      33c9           xor ecx, ecx
            0x18004d7e1      f3410f104500   movss xmm0, dword [r13]
            0x18004d7e7      4489c0         mov eax, r8d
            0x18004d7ea      f3410f104d04   movss xmm1, dword [r13 + 4]
            0x18004d7f0      440f28c8       movaps xmm9, xmm0
            0x18004d7f4      f3410f105508   movss xmm2, dword [r13 + 8]
            0x18004d7fa      440f28c1       movaps xmm8, xmm1
            0x18004d7fe      f3410f105d0c   movss xmm3, dword [r13 + 0xc]
            0x18004d804      0f28fa         movaps xmm7, xmm2
            0x18004d807      f3410f106510   movss xmm4, dword [r13 + 0x10]
            0x18004d80d      0f28f3         movaps xmm6, xmm3
            0x18004d810      0f28ec         movaps xmm5, xmm4
            0x18004d813      450fc6c900     shufps xmm9, xmm9, 0
            0x18004d818      450fc6c000     shufps xmm8, xmm8, 0
            0x18004d81d      0fc6ff00       shufps xmm7, xmm7, 0
            0x18004d821      0fc6f600       shufps xmm6, xmm6, 0
            0x18004d825      0fc6ed00       shufps xmm5, xmm5, 0
            0x18004d829      443bc7         cmp r8d, edi
            0x18004d82c      0f838e000000   jae 0x18004d8c0
            0x18004d832      4d8b16         mov r10, qword [r14]
            0x18004d835      4d8b5e08       mov r11, qword [r14 + 8]
            0x18004d839      4d8b7e10       mov r15, qword [r14 + 0x10]
            0x18004d83d      498b5618       mov rdx, qword [r14 + 0x18]
            0x18004d841      4d8b4e20       mov r9, qword [r14 + 0x20]
            0x18004d845      498b2cf4       mov rbp, qword [r12 + rsi*8]
            0x18004d849      4c89ac24e0..   mov qword [rsp + 0xe0], r13
            0x18004d851      0f1f840000..   nop dword [rax + rax]
            0x18004d859      0f1f800000..   nop dword [rax]
            0x18004d860      448d2c00       lea r13d, [rax + rax]
            0x18004d864      83c002         add eax, 2
            0x18004d867      470f1014aa     movups xmm10, xmmword [r10 + r13*4]
            0x18004d86c      460f107cad00   movups xmm15, xmmword [rbp + r13*4]
            0x18004d872      450f59d1       mulps xmm10, xmm9
            0x18004d876      470f101cab     movups xmm11, xmmword [r11 + r13*4]
            0x18004d87b      470f1024af     movups xmm12, xmmword [r15 + r13*4]
            0x18004d880      450f59d8       mulps xmm11, xmm8
            0x18004d884      450f58fa       addps xmm15, xmm10
            0x18004d888      440f59e7       mulps xmm12, xmm7
            0x18004d88c      460f102caa     movups xmm13, xmmword [rdx + r13*4]
            0x18004d891      470f1034a9     movups xmm14, xmmword [r9 + r13*4]
            0x18004d896      440f59ee       mulps xmm13, xmm6
            0x18004d89a      450f58fb       addps xmm15, xmm11
            0x18004d89e      440f59f5       mulps xmm14, xmm5
            0x18004d8a2      450f58fc       addps xmm15, xmm12
            0x18004d8a6      450f58fd       addps xmm15, xmm13
            0x18004d8aa      450f58fe       addps xmm15, xmm14
            0x18004d8ae      460f117cad00   movups xmmword [rbp + r13*4], xmm15
            0x18004d8b4      3bc7           cmp eax, edi
            0x18004d8b6      72a8           jb 0x18004d860
            0x18004d8b8      4c8bac24e0..   mov r13, qword [rsp + 0xe0]
            0x18004d8c0      3b8424e800..   cmp eax, dword [rsp + 0xe8]
            0x18004d8c7      0f83c5000000   jae 0x18004d992
            0x180022640      4883ec78       sub rsp, 0x78
            0x180022644      4989ca         mov r10, rcx
            0x180022647      33c9           xor ecx, ecx
            0x180022649      33c0           xor eax, eax
            0x18002264b      4585c9         test r9d, r9d
        ┌─< 0x18002264e      0f864b020000   jbe 0x18002289f
        │   0x180022654      4533db         xor r11d, r11d
        │   0x180022657      4c89642438     mov qword [rsp + 0x38], r12
        │   0x18002265c      48895c2420     mov qword [rsp + 0x20], rbx
        │   0x180022661      4889742428     mov qword [rsp + 0x28], rsi
        │   0x180022666      48897c2430     mov qword [rsp + 0x30], rdi
        │   0x18002266b      4c896c2440     mov qword [rsp + 0x40], r13
        │   0x180022670      4c89742448     mov qword [rsp + 0x48], r14
        │   0x180022675      4c897c2450     mov qword [rsp + 0x50], r15
        │   0x18002267a      48896c2458     mov qword [rsp + 0x58], rbp
        │   0x18002267f      33ed           xor ebp, ebp
        │   0x180022681      4c8ba424a0..   mov r12, qword [rsp + 0xa0]
        │   0x180022689      418b3480       mov esi, dword [r8 + rax*4]
        │   0x18002268d      3bf1           cmp esi, ecx
       ┌──< 0x18002268f      7709           ja 0x18002269a
       ││   0x180022691      41892c84       mov dword [r12 + rax*4], ebp
      ┌───< 0x180022695      e9cc010000     jmp 0x180022866
      │└──> 0x18002269a      89ef           mov edi, ebp
      │ │   0x18002269c      660fefc0       pxor xmm0, xmm0
      │ │   0x1800226a0      0f28c8         movaps xmm1, xmm0
      │ │   0x1800226a3      85d2           test edx, edx
      │┌──< 0x1800226a5      0f86ad010000   jbe 0x180022858
      │││   0x1800226ab      4889442470     mov qword [rsp + 0x70], rax
      │││   0x1800226b0      8d1c09         lea ebx, [rcx + rcx]
      │││   0x1800226b3      f7d9           neg ecx
      │││   0x1800226b5      4c89442468     mov qword [rsp + 0x68], r8
      │││   0x1800226ba      03ce           add ecx, esi
      │││   0x1800226bc      44894c2460     mov dword [rsp + 0x60], r9d
      │││   0x1800226c1      89e8           mov eax, ebp
      │││   0x1800226c3      448d4f03       lea r9d, [rdi + 3]
      │││   0x1800226c7      448d4701       lea r8d, [rdi + 1]
      │││   0x1800226cb      443bca         cmp r9d, edx
     ┌────< 0x1800226ce      0f82ba000000   jb 0x18002278e
     ││││   0x1800226d4      89ff           mov edi, edi
     ││││   0x1800226d6      4d8b0cfa       mov r9, qword [r10 + rdi*8]
     ││││   0x1800226da      443bc2         cmp r8d, edx
    ┌─────< 0x1800226dd      723b           jb 0x18002271a
   ┌──────> 0x1800226df      660fefd2       pxor xmm2, xmm2
   ╎│││││   0x1800226e3      448d2443       lea r12d, [rbx + rax*2]
   ╎│││││   0x1800226e7      660fefdb       pxor xmm3, xmm3
   ╎│││││   0x1800226eb      418d7c2401     lea edi, [r12 + 1]
   ╎│││││   0x1800226f0      f3410f5a14b9   cvtss2sd xmm2, dword [r9 + rdi*4]
   ╎│││││   0x1800226f6      f3430f5a1ca1   cvtss2sd xmm3, dword [r9 + r12*4]
   ╎│││││   0x1800226fc      f20f59d2       mulsd xmm2, xmm2
   ╎│││││   0x180022700      f20f59db       mulsd xmm3, xmm3
   ╎│││││   0x180022704      f20f58ca       addsd xmm1, xmm2
   ╎│││││   0x180022708      f20f58c3       addsd xmm0, xmm3
   ╎│││││   0x18002270c      ffc0           inc eax
   ╎│││││   0x18002270e      3bc1           cmp eax, ecx
   └──────< 0x180022710      72cd           jb 0x1800226df
    │││││   0x180022712      4489c7         mov edi, r8d
   ┌──────< 0x180022715      e91f010000     jmp 0x180022839
   │└─────> 0x18002271a      4589c0         mov r8d, r8d
   │ ││││   0x18002271d      4f8b04c2       mov r8, qword [r10 + r8*8]
   │ ││││   0x180022721      0f1f840000..   nop dword [rax + rax]
   │ ││││   0x180022729      0f1f800000..   nop dword [rax]
   │ ││││   0x180022730      660fefd2       pxor xmm2, xmm2
   │ ││││   0x180022734      448d2c43       lea r13d, [rbx + rax*2]
   │ ││││   0x180022738      660fefe4       pxor xmm4, xmm4
   │ ││││   0x18002273c      458d6501       lea r12d, [r13 + 1]
   │ ││││   0x180022740      f3430f5a14a1   cvtss2sd xmm2, dword [r9 + r12*4]
   │ ││││   0x180022746      f3430f5a24a9   cvtss2sd xmm4, dword [r9 + r13*4]
   │ ││││   0x18002274c      f20f59d2       mulsd xmm2, xmm2
   │ ││││   0x180022750      f20f59e4       mulsd xmm4, xmm4
   │ ││││   0x180022754      f20f58ca       addsd xmm1, xmm2
   │ ││││   0x180022758      f20f58c4       addsd xmm0, xmm4
   │ ││││   0x18002275c      660fefdb       pxor xmm3, xmm3
   │ ││││   0x180022760      660fefed       pxor xmm5, xmm5
   │ ││││   0x180022764      f3430f5a1ca0   cvtss2sd xmm3, dword [r8 + r12*4]
   │ ││││   0x18002276a      f3430f5a2ca8   cvtss2sd xmm5, dword [r8 + r13*4]
   │ ││││   0x180022770      f20f59db       mulsd xmm3, xmm3
   │ ││││   0x180022774      f20f59ed       mulsd xmm5, xmm5
   │ ││││   0x180022778      f20f58cb       addsd xmm1, xmm3
   │ ││││   0x18002277c      f20f58c5       addsd xmm0, xmm5
   │ ││││   0x180022780      ffc0           inc eax
   │ ││││   0x180022782      3bc1           cmp eax, ecx
   │ ││││   0x180022784      72aa           jb 0x180022730
   │ ││││   0x180022786      83c702         add edi, 2
   │ ││││   0x180022789      e9ab000000     jmp 0x180022839
   │ └────> 0x18002278e      448d7f02       lea r15d, [rdi + 2]
   │  │││   0x180022792      4f8b2cc2       mov r13, qword [r10 + r8*8]
   │  │││   0x180022796      4f8b34ca       mov r14, qword [r10 + r9*8]
   │  │││   0x18002279a      4d8b24fa       mov r12, qword [r10 + rdi*8]
   │  │││   0x18002279e      4f8b04fa       mov r8, qword [r10 + r15*8]
   │  │││   0x1800227a2      660fefed       pxor xmm5, xmm5
   │  │││   0x1800227a6      448d0c43       lea r9d, [rbx + rax*2]
   │  │││   0x1800227aa      ffc0           inc eax
   │  │││   0x1800227ac      458d7901       lea r15d, [r9 + 1]
   │  │││   0x1800227b0      f3430f5a14bc   cvtss2sd xmm2, dword [r12 + r15*4]
   │  │││   0x1800227b6      f3430f5a5c..   cvtss2sd xmm3, dword [r13 + r15*4]
   │  │││   0x1800227bd      f3430f5a24b8   cvtss2sd xmm4, dword [r8 + r15*4]
   │  │││   0x1800227c3      f3430f5a2cbe   cvtss2sd xmm5, dword [r14 + r15*4]
   │  │││   0x1800227c9      f20f59d2       mulsd xmm2, xmm2
   │  │││   0x1800227cd      f20f59db       mulsd xmm3, xmm3
   │  │││   0x1800227d1      f20f59e4       mulsd xmm4, xmm4
