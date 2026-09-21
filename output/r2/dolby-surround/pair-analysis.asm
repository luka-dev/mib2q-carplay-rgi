            0x180031410      56             push rsi
            0x180031411      57             push rdi
            0x180031412      4154           push r12
            0x180031414      4155           push r13
            0x180031416      4156           push r14
            0x180031418      4157           push r15
            0x18003141a      55             push rbp
            0x18003141b      4881ecd000..   sub rsp, 0xd0
            0x180031422      4c89ce         mov rsi, r9
            0x180031425      488bac2460..   mov rbp, qword [rsp + 0x160]
            0x18003142d      4d89c7         mov r15, r8
            0x180031430      4989d6         mov r14, rdx
            0x180031433      4989cd         mov r13, rcx
            0x180031436      488d7d1f       lea rdi, [rbp + 0x1f]
            0x18003143a      4883e7e0       and rdi, 0xffffffffffffffe0
            0x18003143e      83bc243001..   cmp dword [rsp + 0x130], 0
        ┌─< 0x180031446      740d           je 0x180031455
        │   0x180031448      4533c0         xor r8d, r8d
        │   0x18003144b      837e0400       cmp dword [rsi + 4], 0
        │   0x18003144f      410f9dc0       setge r8b
       ┌──< 0x180031453      eb03           jmp 0x180031458
       │└─> 0x180031455      4533c0         xor r8d, r8d
       └──> 0x180031458      4c8d6608       lea r12, [rsi + 8]
            0x18003145c      4c89e2         mov rdx, r12
            0x18003145f      498d8d3868..   lea rcx, [r13 + 0x6838]
            0x180031466      e8c5c90100     call 0x18004de30
            0x18003146b      4889e0         mov rax, rsp
            0x18003146e      4c89e9         mov rcx, r13
            0x180031471      4d8b16         mov r10, qword [r14]
            0x180031474      4d89f9         mov r9, r15
            0x180031477      458b7608       mov r14d, dword [r14 + 8]
            0x18003147b      448b9c2458..   mov r11d, dword [rsp + 0x158]
            0x180031483      488b942448..   mov rdx, qword [rsp + 0x148]
            0x18003148b      4c8b842450..   mov r8, qword [rsp + 0x150]
            0x180031493      4c895020       mov qword [rax + 0x20], r10
            0x180031497      44897028       mov dword [rax + 0x28], r14d
            0x18003149b      44895830       mov dword [rax + 0x30], r11d
            0x18003149f      e80cbc0100     call 0x18004d0b0
            0x1800314a4      448b8c2440..   mov r9d, dword [rsp + 0x140]
            0x1800314ac      83bc243801..   cmp dword [rsp + 0x138], 0
        ┌─< 0x1800314b4      7420           je 0x1800314d6
        │   0x1800314b6      448b5630       mov r10d, dword [rsi + 0x30]
        │   0x1800314ba      4c8b7638       mov r14, qword [rsi + 0x38]
        │   0x1800314be      44895704       mov dword [rdi + 4], r10d
        │   0x1800314c2      4585c9         test r9d, r9d
       ┌──< 0x1800314c5      0f8415030000   je 0x1800317e0
       ││   0x1800314cb      4c8b5e50       mov r11, qword [rsi + 0x50]
       ││   0x1800314cf      f30f104e48     movss xmm1, dword [rsi + 0x48]
      ┌───< 0x1800314d4      eb23           jmp 0x1800314f9
      ││└─> 0x1800314d6      4c8b7640       mov r14, qword [rsi + 0x40]
      ││    0x1800314da      41ba02000000   mov r10d, 2
      ││    0x1800314e0      c747040200..   mov dword [rdi + 4], 2
      ││    0x1800314e7      4585c9         test r9d, r9d
      ││┌─< 0x1800314ea      0f84f0020000   je 0x1800317e0
      │││   0x1800314f0      4c8b5e58       mov r11, qword [rsi + 0x58]
      │││   0x1800314f4      f30f104e4c     movss xmm1, dword [rsi + 0x4c]
      └───> 0x1800314f9      c70701000000   mov dword [rdi], 1
       ││   0x1800314ff      488dad1e03..   lea rbp, [rbp + 0x31e]
       ││   0x180031506      440faf16       imul r10d, dword [rsi]
       ││   0x18003150a      4883e5e0       and rbp, 0xffffffffffffffe0
       ││   0x18003150e      f30f10159a..   movss xmm2, dword [0x1802f52b0] ; [0x1802f52b0:4]=0x3f000000
       ││   0x180031516      f30f100596..   movss xmm0, dword [0x1802f52b4] ; [0x1802f52b4:4]=0x3e800000
       ││   0x18003151e      0f11b42490..   movups xmmword [rsp + 0x90], xmm6
       ││   0x180031526      440f11b424..   movups xmmword [rsp + 0xa0], xmm14
       ││   0x18003152f      4489d6         mov esi, r10d
       ││   0x180031532      4533ff         xor r15d, r15d
       ││   0x180031535      83e607         and esi, 7
       ││   0x180031538      4533c9         xor r9d, r9d
       ││   0x18003153b      f7de           neg esi
       ││   0x18003153d      48896f08       mov qword [rdi + 8], rbp
       ││   0x180031541      4103f2         add esi, r10d
       ││   0x180031544      440f11bc24..   movups xmmword [rsp + 0xb0], xmm15
       ││   0x18003154d      48897c2440     mov qword [rsp + 0x40], rdi
       ││   0x180031552      48899c24c0..   mov qword [rsp + 0xc0], rbx
       ││   0x18003155a      f3430f10b4..   movss xmm6, dword [r13 + r15*4 + 0x140]
       ││   0x180031564      4889e8         mov rax, rbp
       ││   0x180031567      0f28ee         movaps xmm5, xmm6
       ││   0x18003156a      0f28e6         movaps xmm4, xmm6
       ││   0x18003156d      f30f59ee       mulss xmm5, xmm6
       ││   0x180031571      f30f101d3f..   movss xmm3, dword [0x1802f52b8] ; [0x1802f52b8:4]=0x3f800000
       ││   0x180031579      f30f5cdd       subss xmm3, xmm5
       ││   0x18003157d      0f28ea         movaps xmm5, xmm2
       ││   0x180031580      f30f51db       sqrtss xmm3, xmm3
       ││   0x180031584      f30f59e3       mulss xmm4, xmm3
       ││   0x180031588      f30f59e1       mulss xmm4, xmm1
       ││   0x18003158c      f30f59e2       mulss xmm4, xmm2
       ││   0x180031590      f30f58e0       addss xmm4, xmm0
       ││   0x180031594      f30f51e4       sqrtss xmm4, xmm4
       ││   0x180031598      f30f5eec       divss xmm5, xmm4
       ││   0x18003159c      f30f59f5       mulss xmm6, xmm5
       ││   0x1800315a0      f30f59eb       mulss xmm5, xmm3
       ││   0x1800315a4      4585d2         test r10d, r10d
       ││   0x1800315a7      0f86fa010000   jbe 0x1800317a7
       ││   0x1800315ad      4183fa08       cmp r10d, 8                ; 8
       ││   0x1800315b1      0f8299020000   jb 0x180031850
       ││   0x1800315b7      4889ef         mov rdi, rbp
       ││   0x1800315ba      4883e70f       and rdi, 0xf               ; 15
       ││   0x1800315be      89ff           mov edi, edi
       ││   0x1800315c0      85ff           test edi, edi
       ││   0x1800315c2      7454           je 0x180031618
       ││   0x1800315c4      f7df           neg edi
       ││   0x1800315c6      83c710         add edi, 0x10              ; 16
       ││   0x1800315c9      c1ef02         shr edi, 2
       ││   0x1800315cc      8d5f08         lea ebx, [rdi + 8]
       ││   0x1800315cf      443bd3         cmp r10d, ebx
       ││   0x1800315d2      0f8265020000   jb 0x18003183d
       ││   0x1800315d8      4489d3         mov ebx, r10d
       ││   0x1800315db      33d2           xor edx, edx
       ││   0x1800315dd      2bdf           sub ebx, edi
       ││   0x1800315df      4533ff         xor r15d, r15d
       ││   0x1800315e2      83e307         and ebx, 7
       ││   0x1800315e5      f7db           neg ebx
       ││   0x1800315e7      4103da         add ebx, r10d
       ││   0x1800315ea      85ff           test edi, edi
       ││   0x1800315ec      742c           je 0x18003161a
       ││   0x1800315ee      f3410f102493   movss xmm4, dword [r11 + rdx*4]
       ││   0x1800315f4      f3410f101c96   movss xmm3, dword [r14 + rdx*4]
       ││   0x1800315fa      ffc2           inc edx
       ││   0x1800315fc      f30f59e6       mulss xmm4, xmm6
       ││   0x180031600      f30f59dd       mulss xmm3, xmm5
       ││   0x180031604      f30f58e3       addss xmm4, xmm3
       ││   0x180031608      f3420f1164..   movss dword [rbp + r15*4], xmm4
       ││   0x18003160f      49ffc7         inc r15
       ││   0x180031612      3bd7           cmp edx, edi
       ││   0x180031614      72d8           jb 0x1800315ee
       ││   0x180031616      eb02           jmp 0x18003161a
       ││   0x180031618      89f3           mov ebx, esi
       ││   0x18003161a      4d8d3cbe       lea r15, [r14 + rdi*4]
       ││   0x18003161e      49f7c70f00..   test r15, 0xf              ; 15
       ││   0x180031625      7463           je 0x18003168a
       ││   0x180031627      0f28e6         movaps xmm4, xmm6
       ││   0x18003162a      0f28dd         movaps xmm3, xmm5
       ││   0x18003162d      0fc6e400       shufps xmm4, xmm4, 0
       ││   0x180031631      0fc6db00       shufps xmm3, xmm3, 0
       ││   0x180031635      0f1f4000       nop dword [rax]
       ││   0x180031639      0f1f800000..   nop dword [rax]
       ││   0x180031640      89ff           mov edi, edi
       ││   0x180031642      450f103cbb     movups xmm15, xmmword [r11 + rdi*4]
       ││   0x180031647      450f1034be     movups xmm14, xmmword [r14 + rdi*4]
       ││   0x18003164c      440f59fc       mulps xmm15, xmm4
       ││   0x180031650      440f59f3       mulps xmm14, xmm3
       ││   0x180031654      4863ff         movsxd rdi, edi
       ││   0x180031657      450f58fe       addps xmm15, xmm14
       ││   0x18003165b      440f117cbd00   movups xmmword [rbp + rdi*4], xmm15
       ││   0x180031661      448d7f04       lea r15d, [rdi + 4]
       ││   0x180031665      470f103cbb     movups xmm15, xmmword [r11 + r15*4]
       ││   0x18003166a      470f1034be     movups xmm14, xmmword [r14 + r15*4]
       ││   0x18003166f      440f59fc       mulps xmm15, xmm4
       ││   0x180031673      440f59f3       mulps xmm14, xmm3
       ││   0x180031677      450f58fe       addps xmm15, xmm14
       ││   0x18003167b      440f117cbd10   movups xmmword [rbp + rdi*4 + 0x10], xmm15
       ││   0x180031681      83c708         add edi, 8
       ││   0x180031684      3bfb           cmp edi, ebx
       ││   0x180031686      72b8           jb 0x180031640
       ││   0x180031688      eb5e           jmp 0x1800316e8
       ││   0x18003168a      0f28e6         movaps xmm4, xmm6
       ││   0x18003168d      0f28dd         movaps xmm3, xmm5
       ││   0x180031690      0fc6e400       shufps xmm4, xmm4, 0
       ││   0x180031694      0fc6db00       shufps xmm3, xmm3, 0
       ││   0x180031698      0f1f840000..   nop dword [rax + rax]
       ││   0x1800316a0      89ff           mov edi, edi
       ││   0x1800316a2      450f103cbb     movups xmm15, xmmword [r11 + rdi*4]
       ││   0x1800316a7      450f1034be     movups xmm14, xmmword [r14 + rdi*4]
       ││   0x1800316ac      440f59fc       mulps xmm15, xmm4
       ││   0x1800316b0      440f59f3       mulps xmm14, xmm3
       ││   0x1800316b4      4863ff         movsxd rdi, edi
       ││   0x1800316b7      450f58fe       addps xmm15, xmm14
       ││   0x1800316bb      440f117cbd00   movups xmmword [rbp + rdi*4], xmm15
       ││   0x1800316c1      448d7f04       lea r15d, [rdi + 4]
       ││   0x1800316c5      470f103cbb     movups xmm15, xmmword [r11 + r15*4]
       ││   0x1800316ca      470f1034be     movups xmm14, xmmword [r14 + r15*4]
       ││   0x1800316cf      440f59fc       mulps xmm15, xmm4
       ││   0x1800316d3      440f59f3       mulps xmm14, xmm3
       ││   0x1800316d7      450f58fe       addps xmm15, xmm14
       ││   0x1800316db      440f117cbd10   movups xmmword [rbp + rdi*4 + 0x10], xmm15
       ││   0x1800316e1      83c708         add edi, 8
       ││   0x1800316e4      3bfb           cmp edi, ebx
       ││   0x1800316e6      72b8           jb 0x1800316a0
       ││   0x1800316e8      4c63ff         movsxd r15, edi
       ││   0x1800316eb      2bfb           sub edi, ebx
       ││   0x1800316ed      4c2bff         sub r15, rdi
       ││   0x1800316f0      4a8d6cbd00     lea rbp, [rbp + r15*4]
       ││   0x1800316f5      8d7b01         lea edi, [rbx + 1]
       ││   0x1800316f8      413bfa         cmp edi, r10d
       ││   0x1800316fb      0f87a6000000   ja 0x1800317a7
       ││   0x180031701      4489d1         mov ecx, r10d
       ││   0x180031704      2bcb           sub ecx, ebx
       ││   0x180031706      83f904         cmp ecx, 4                 ; 4
       ││   0x180031709      0f823a010000   jb 0x180031849
       ││   0x18003170f      89ca           mov edx, ecx
       ││   0x180031711      8d2c9d0000..   lea ebp, [rbx*4]
       ││   0x180031718      4863fd         movsxd rdi, ebp
       ││   0x18003171b      83e2fc         and edx, 0xfffffffc        ; 4294967292
       ││   0x18003171e      4533ff         xor r15d, r15d
       ││   0x180031721      468d043b       lea r8d, [rbx + r15]
       ││   0x180031725      4183c704       add r15d, 4
       ││   0x180031729      470f103c83     movups xmm15, xmmword [r11 + r8*4]
       ││   0x18003172e      470f103486     movups xmm14, xmmword [r14 + r8*4]
       ││   0x180031733      440f59fc       mulps xmm15, xmm4
       ││   0x180031737      440f59f3       mulps xmm14, xmm3
       ││   0x18003173b      450f58fe       addps xmm15, xmm14
       ││   0x18003173f      440f113c07     movups xmmword [rdi + rax], xmm15
       ││   0x180031744      4883c710       add rdi, 0x10              ; 16
       ││   0x180031748      443bfa         cmp r15d, edx
       ││   0x18003174b      72d4           jb 0x180031721
       ││   0x18003174d      428d6cbd00     lea ebp, [rbp + r15*4]
       ││   0x180031752      442bfa         sub r15d, edx
       ││   0x180031755      4863ed         movsxd rbp, ebp
       ││   0x180031758      49c1e702       shl r15, 2
       ││   0x18003175c      4803e8         add rbp, rax
       ││   0x18003175f      492bef         sub rbp, r15
       ││   0x180031762      3bd1           cmp edx, ecx
       ││   0x180031764      7341           jae 0x1800317a7
       ││   0x180031766      8d2c1a         lea ebp, [rdx + rbx]
       ││   0x180031769      f3410f1024ab   movss xmm4, dword [r11 + rbp*4]
       ││   0x18003176f      8d3c950000..   lea edi, [rdx*4]
       ││   0x180031776      f3410f101cae   movss xmm3, dword [r14 + rbp*4]
       ││   0x18003177c      448d3c9f       lea r15d, [rdi + rbx*4]
       ││   0x180031780      f30f59e6       mulss xmm4, xmm6
       ││   0x180031784      ffc2           inc edx
       ││   0x180031786      f30f59dd       mulss xmm3, xmm5
       ││   0x18003178a      4d63ff         movsxd r15, r15d
       ││   0x18003178d      f30f58e3       addss xmm4, xmm3
       ││   0x180031791      f3410f112407   movss dword [r15 + rax], xmm4
       ││   0x180031797      3bd1           cmp edx, ecx
       ││   0x180031799      72cb           jb 0x180031766
       ││   0x18003179b      c1e202         shl edx, 2
       ││   0x18003179e      8d2c9a         lea ebp, [rdx + rbx*4]
       ││   0x1800317a1      4863ed         movsxd rbp, ebp
       ││   0x1800317a4      4803e8         add rbp, rax
       ││   0x1800317a7      41ffc1         inc r9d
       ││   0x1800317aa      4589cf         mov r15d, r9d
       ││   0x1800317ad      4183f914       cmp r9d, 0x14              ; 20
       ││   0x1800317b1      0f82a3fdffff   jb 0x18003155a
       ││   0x1800317b7      0f10b42490..   movups xmm6, xmmword [rsp + 0x90]
       ││   0x1800317bf      440f10b424..   movups xmm14, xmmword [rsp + 0xa0]
       ││   0x1800317c8      440f10bc24..   movups xmm15, xmmword [rsp + 0xb0]
       ││   0x1800317d1      488b7c2440     mov rdi, qword [rsp + 0x40]
       ││   0x1800317d6      488b9c24c0..   mov rbx, qword [rsp + 0xc0]
       ││   0x1800317de      eb0a           jmp 0x1800317ea
0x1802eccc0  0x0000000600000002  0x000000013f8f9e4d   ........M..?....
0x1802eccd0  0x00000001802eccd8  0x00000001802eae80   ................
0x1802ecce0  0x0000000200000000  0x00000001802ed2b0   ................
0x1802eccf0  0x0000000800000002  0x000000013f8f9e4d   ........M..?....
0x1802ecd00  0x00000001802ecd08  0x00000001802eb040   ........@.......
0x1802ecd10  0x0000000200000000  0x00000001802ed3a4   ................
0x1802ecd20  0x0000000800000002  0x000000013f8f9e4d   ........M..?....
0x1802ecd30  0x00000001802ecd38  0x00000001802eb260   8.......`.......
0x1802ecd40  0x0000000200000000  0x00000001802ed488   ................
0x1802ecd50  0x0000000a00000002  0x000000013f8f9e4d   ........M..?....
0x1802ecd60  0x00000001802ecd68  0x00000001802eb460   h.......`.......
0x1802ecd70  0x0000000200000000  0x00000001802ed51c   ................
0x1802edb70  0x00000003 0x00000009 0x0000000a 0x00000008  ................
0x1802edb80  0x00000000 0x00000001 0x00000002 0x00000003  ................
0x1802edb90  0x00000004 0x00000005 0x00000006 0x00000007  ................
0x1802edba0  0x00000008 0x00000000 0x00000001 0x00000002  ................
