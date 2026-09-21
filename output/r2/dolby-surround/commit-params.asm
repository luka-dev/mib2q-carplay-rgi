            0x180003590      57             push rdi
            0x180003591      4154           push r12
            0x180003593      4155           push r13
            0x180003595      4156           push r14
            0x180003597      4157           push r15
            0x180003599      55             push rbp
            0x18000359a      4881ecb800..   sub rsp, 0xb8
            0x1800035a1      4889cd         mov rbp, rcx
            0x1800035a4      83bdb01300..   cmp dword [rbp + 0x13b0], 0
        ┌─< 0x1800035ab      0f84b7060000   je 0x180003c68
        │   0x1800035b1      837d0400       cmp dword [rbp + 4], 0
       ┌──< 0x1800035b5      7409           je 0x1800035c0
       ││   0x1800035b7      83bd780700..   cmp dword [rbp + 0x778], 0
      ┌───< 0x1800035be      7507           jne 0x1800035c7
      │└──> 0x1800035c0      bf01000000     mov edi, 1
      │┌──< 0x1800035c5      eb02           jmp 0x1800035c9
      └───> 0x1800035c7      33ff           xor edi, edi
       └──> 0x1800035c9      8b4560         mov eax, dword [rbp + 0x60]
        │   0x1800035cc      3b85c8000000   cmp eax, dword [rbp + 0xc8]
       ┌──< 0x1800035d2      7406           je 0x1800035da
       ││   0x1800035d4      8985c8000000   mov dword [rbp + 0xc8], eax
       └──> 0x1800035da      448b8d2801..   mov r9d, dword [rbp + 0x128]
        │   0x1800035e1      448b951c01..   mov r10d, dword [rbp + 0x11c]
        │   0x1800035e8      8b8518010000   mov eax, dword [rbp + 0x118]
        │   0x1800035ee      44898d2c01..   mov dword [rbp + 0x12c], r9d
        │   0x1800035f5      443bd0         cmp r10d, eax
       ┌──< 0x1800035f8      7413           je 0x18000360d
       ││   0x1800035fa      c785d00600..   mov dword [rbp + 0x6d0], 1
       ││   0x180003604      4189c2         mov r10d, eax
       ││   0x180003607      89851c010000   mov dword [rbp + 0x11c], eax
       └──> 0x18000360d      448b852401..   mov r8d, dword [rbp + 0x124]
        │   0x180003614      8b8520010000   mov eax, dword [rbp + 0x120]
        │   0x18000361a      443bc0         cmp r8d, eax
       ┌──< 0x18000361d      7413           je 0x180003632
       ││   0x18000361f      c785d00600..   mov dword [rbp + 0x6d0], 1
       ││   0x180003629      4189c0         mov r8d, eax
       ││   0x18000362c      898524010000   mov dword [rbp + 0x124], eax
       └──> 0x180003632      4585c9         test r9d, r9d
       ┌──< 0x180003635      0f849d000000   je 0x1800036d8
       ││   0x18000363b      4183fa0f       cmp r10d, 0xf              ; 15
      ┌───< 0x18000363f      773d           ja 0x18000367e
      │││   0x180003641      4c8d0db8c9..   lea r9, [0x180000000]
      │││   0x180003648      430fb68411..   movzx eax, byte [r9 + r10 + 0x2e8768]
      │││   0x180003651      498d84015b..   lea rax, [r9 + rax + 0x365b]
      │││   0x180003659      ffe0           jmp rax
      │││   0x18000365b      b80c000000     mov eax, 0xc               ; 12
     ┌────< 0x180003660      eb21           jmp 0x180003683
     ││││   0x180003662      b80a000000     mov eax, 0xa
    ┌─────< 0x180003667      eb1a           jmp 0x180003683
    │││││   0x180003669      b808000000     mov eax, 8
   ┌──────< 0x18000366e      eb13           jmp 0x180003683
   ││││││   0x180003670      b806000000     mov eax, 6
  ┌───────< 0x180003675      eb0c           jmp 0x180003683
  │││││││   0x180003677      b801000000     mov eax, 1
  ────────< 0x18000367c      eb05           jmp 0x180003683
  ││││└───> 0x18000367e      b802000000     mov eax, 2
  └└└└────> 0x180003683      440fafc0       imul r8d, eax
       ││   0x180003687      41ffc8         dec r8d
       ││   0x18000368a      4489c0         mov eax, r8d
       ││   0x18000368d      4183f8ff       cmp r8d, 0xffffffff
       ││   0x180003691      7445           je 0x1800036d8
       ││   0x180003693      448b8dd006..   mov r9d, dword [rbp + 0x6d0]
       ││   0x18000369a      f30f108c85..   movss xmm1, dword [rbp + rax*4 + 0x130]
       ││   0x1800036a3      41ffc8         dec r8d
       ││   0x1800036a6      f30f108485..   movss xmm0, dword [rbp + rax*4 + 0x310]
       ││   0x1800036af      f30f118c85..   movss dword [rbp + rax*4 + 0x310], xmm1
       ││   0x1800036b8      4489c0         mov eax, r8d
       ││   0x1800036bb      f30fc2c104     cmpneqss xmm0, xmm1
       ││   0x1800036c0      66410f7ec2     movd r10d, xmm0
       ││   0x1800036c5      41f7da         neg r10d
       ││   0x1800036c8      450bca         or r9d, r10d
       ││   0x1800036cb      4183f8ff       cmp r8d, 0xffffffff
       ││   0x1800036cf      75c9           jne 0x18000369a
       ││   0x1800036d1      44898dd006..   mov dword [rbp + 0x6d0], r9d
       └──> 0x1800036d8      4c8b4530       mov r8, qword [rbp + 0x30]
        │   0x1800036dc      488d8d980e..   lea rcx, [rbp + 0xe98]
        │   0x1800036e3      8b8580070000   mov eax, dword [rbp + 0x780]
        │   0x1800036e9      89857c070000   mov dword [rbp + 0x77c], eax
        │   0x1800036ef      498b5040       mov rdx, qword [r8 + 0x40]
        │   0x1800036f3      e8b8b00100     call 0x18001e7b0
        │   0x1800036f8      8b85ac070000   mov eax, dword [rbp + 0x7ac]
        │   0x1800036fe      8985a8070000   mov dword [rbp + 0x7a8], eax
        │   0x180003704      8b8594070000   mov eax, dword [rbp + 0x794]
        │   0x18000370a      83bd900700..   cmp dword [rbp + 0x790], 0
        │   0x180003711      7416           je 0x180003729
        │   0x180003713      85c0           test eax, eax
        │   0x180003715      7512           jne 0x180003729
        │   0x180003717      488b8d1814..   mov rcx, qword [rbp + 0x1418]
        │   0x18000371e      e84d0f0100     call 0x180014670
        │   0x180003723      8b8594070000   mov eax, dword [rbp + 0x794]
        │   0x180003729      898590070000   mov dword [rbp + 0x790], eax
        │   0x18000372f      488d8d5007..   lea rcx, [rbp + 0x750]
        │   0x180003736      8b414c         mov eax, dword [rcx + 0x4c]
        │   0x180003739      89fa           mov edx, edi
        │   0x18000373b      448b4154       mov r8d, dword [rcx + 0x54]
        │   0x18000373f      894148         mov dword [rcx + 0x48], eax
        │   0x180003742      44894150       mov dword [rcx + 0x50], r8d
        │   0x180003746      e8e5370000     call 0x180006f30
        │   0x18000374b      8b450c         mov eax, dword [rbp + 0xc]
        │   0x18000374e      a9feffffff     test eax, 0xfffffffe       ; 4294967294
        │   0x180003753      750f           jne 0x180003764
        │   0x180003755      488d8d880a..   lea rcx, [rbp + 0xa88]
        │   0x18000375c      e85f3d0000     call 0x1800074c0
        │   0x180003761      8b450c         mov eax, dword [rbp + 0xc]
        │   0x180003764      85c0           test eax, eax
        │   0x180003766      753d           jne 0x1800037a5
        │   0x180003768      488d8d200b..   lea rcx, [rbp + 0xb20]
        │   0x18000376f      e85cb10100     call 0x18001e8d0
        │   0x180003774      488d8dc80d..   lea rcx, [rbp + 0xdc8]
        │   0x18000377b      e8a0b30100     call 0x18001eb20
        │   0x180003780      488d8df80e..   lea rcx, [rbp + 0xef8]
        │   0x180003787      488d950813..   lea rdx, [rbp + 0x1308]
        │   0x18000378e      e8edba0100     call 0x18001f280
        │   0x180003793      837d0c00       cmp dword [rbp + 0xc], 0
        │   0x180003797      750c           jne 0x1800037a5
        │   0x180003799      488b8df00e..   mov rcx, qword [rbp + 0xef0]
        │   0x1800037a0      e8fb300100     call 0x1800168a0
        │   0x1800037a5      8b85100b0000   mov eax, dword [rbp + 0xb10]
        │   0x1800037ab      488d8df806..   lea rcx, [rbp + 0x6f8]
        │   0x1800037b2      8985140b0000   mov dword [rbp + 0xb14], eax
        │   0x1800037b8      e8e32f0000     call 0x1800067a0
        │   0x1800037bd      837d0800       cmp dword [rbp + 8], 0
        │   0x1800037c1      7408           je 0x1800037cb
        │   0x1800037c3      4889e9         mov rcx, rbp
        │   0x1800037c6      e835c00100     call 0x18001f800
- offset -   4041 4243 4445 4647 4849 4A4B 4C4D 4E4F  0123456789ABCDEF
0x1802e8740  c10f fc39 2d15 aa43 d10b 3d3d 0bd7 233e  ...9-..C..==..#>
