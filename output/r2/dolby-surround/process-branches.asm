            0x18000850f      83bd7c0700..   cmp dword [rbp + 0x77c], 0
        ┌─< 0x180008516      0f8478040000   je 0x180008994
        │   0x18000851c      4885d2         test rdx, rdx
       ┌──< 0x18000851f      0f8459320000   je 0x18000b77e
       ││   0x180008525      83fb02         cmp ebx, 2                 ; reloc.OLEAUT32.dll_SysReAllocString
      ┌───< 0x180008528      0f8566040000   jne 0x180008994
      │││   0x18000852e      488b842470..   mov rax, qword [rsp + 0x970]
      │││   0x180008536      83780400       cmp dword [rax + 4], 0
     ┌────< 0x18000853a      0f8554040000   jne 0x180008994
     ││││   0x180008540      83bc24b003..   cmp dword [rsp + 0x3b0], 2
    ┌─────< 0x180008548      770e           ja 0x180008558
    │││││   0x18000854a      83bc24b803..   cmp dword [rsp + 0x3b8], 0
   ┌──────< 0x180008552      0f843c040000   je 0x180008994
   │└─────> 0x180008558      83bddc3200..   cmp dword [rbp + 0x32dc], 0
   │┌─────< 0x18000855f      0f852f040000   jne 0x180008994
   ││││││   0x180008565      448b951c01..   mov r10d, dword [rbp + 0x11c]
   ││││││   0x18000856c      33db           xor ebx, ebx
   ││││││   0x18000856e      4585ff         test r15d, r15d
   ││││││   0x180008571      8b8590070000   mov eax, dword [rbp + 0x790]
   ││││││   0x180008577      0f94c3         sete bl
   ││││││   0x18000857a      4885d2         test rdx, rdx
   ││││││   0x18000857d      899c24d800..   mov dword [rsp + 0xd8], ebx
   ││││││   0x180008584      488b9c2470..   mov rbx, qword [rsp + 0x970]
   ││││││   0x18000858c      490f44dc       cmove rbx, r12
   ││││││   0x180008590      898424e000..   mov dword [rsp + 0xe0], eax
   ││││││   0x180008597      448b7b08       mov r15d, dword [rbx + 8]
   ││││││   0x18000859b      4183fa0f       cmp r10d, 0xf              ; 15
  ┌───────< 0x18000859f      7757           ja 0x1800085f8
  │││││││   0x1800085a1      4c8d1d587a..   lea r11, [0x180000000]
  │││││││   0x1800085a8      430fb68413..   movzx eax, byte [r11 + r10 + 0x2e8930]
  │││││││   0x1800085b1      498d8403bb..   lea rax, [r11 + rax + 0x85bb]
  │││││││   0x1800085b9      ffe0           jmp rax
  │││││││   0x1800085bb      41bb0a000000   mov r11d, 0xa
  ────────< 0x1800085c1      eb38           jmp 0x1800085fb
  │││││││   0x1800085c3      41bb06000000   mov r11d, 6
  │││││││   0x1800085c9      4183ff08       cmp r15d, 8                ; 8
  │││││││   0x1800085cd      450f44df       cmove r11d, r15d
  ────────< 0x1800085d1      eb28           jmp 0x1800085fb
  │││││││   0x1800085d3      b80a000000     mov eax, 0xa
  │││││││   0x1800085d8      41bb08000000   mov r11d, 8
  │││││││   0x1800085de      4183ff08       cmp r15d, 8                ; 8
  │││││││   0x1800085e2      440f44d8       cmove r11d, eax
  ────────< 0x1800085e6      eb13           jmp 0x1800085fb
  │││││││   0x1800085e8      41bb08000000   mov r11d, 8
  ────────< 0x1800085ee      eb0b           jmp 0x1800085fb
  │││││││   0x1800085f0      41bb06000000   mov r11d, 6
  ────────< 0x1800085f6      eb03           jmp 0x1800085fb
  └───────> 0x1800085f8      4533db         xor r11d, r11d
  ────────> 0x1800085fb      83bc24b803..   cmp dword [rsp + 0x3b8], 0
  ┌───────< 0x180008603      7410           je 0x180008615
  │││││││   0x180008605      4489d0         mov eax, r10d
  │││││││   0x180008608      83e0fb         and eax, 0xfffffffb        ; 4294967291
  │││││││   0x18000860b      83f809         cmp eax, 9                 ; 9
  │││││││   0x18000860e      740e           je 0x18000861e
  │││││││   0x180008610      83f80b         cmp eax, 0xb               ; 11
  │││││││   0x180008613      7409           je 0x18000861e
  └───────> 0x180008615      418d42fa       lea eax, [r10 - 6]
   ││││││   0x180008619      83f801         cmp eax, 1                 ; 1
   ││││││   0x18000861c      770d           ja 0x18000862b
   ││││││   0x18000861e      c78424c000..   mov dword [rsp + 0xc0], 1
   ││││││   0x180008629      eb0b           jmp 0x180008636
   ││││││   0x18000862b      c78424c000..   mov dword [rsp + 0xc0], 0
   ││││││   0x180008636      4183ff02       cmp r15d, 2                ; reloc.OLEAUT32.dll_SysReAllocString
   ││││││   0x18000863a      7573           jne 0x1800086af
   ││││││   0x18000863c      4183fb06       cmp r11d, 6                ; reloc.OLEAUT32.dll_SysStringLen
   ││││││   0x180008640      7459           je 0x18000869b
   ││││││   0x180008642      4183fb08       cmp r11d, 8                ; 8
   ││││││   0x180008646      7527           jne 0x18000866f
   ││││││   0x180008648      83bc24c000..   cmp dword [rsp + 0xc0], 0
   ││││││   0x180008650      488d05c946..   lea rax, [0x1802ecd20]
   ││││││   0x180008657      4c8d159246..   lea r10, [0x1802eccf0]
   ││││││   0x18000865e      4c0f45d0       cmovne r10, rax
   ││││││   0x180008662      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x18000866a      e9d7010000     jmp 0x180008846
   ││││││   0x18000866f      4183fb0a       cmp r11d, 0xa              ; 10
   ││││││   0x180008673      0f85c1010000   jne 0x18000883a
   ││││││   0x180008679      83bc24c000..   cmp dword [rsp + 0xc0], 0
   ││││││   0x180008681      0f84b3010000   je 0x18000883a
   ││││││   0x180008687      488d05c246..   lea rax, [0x1802ecd50]
   ││││││   0x18000868e      48898424c8..   mov qword [rsp + 0xc8], rax
   ││││││   0x180008696      e9ab010000     jmp 0x180008846
   ││││││   0x18000869b      488d051e46..   lea rax, [0x1802eccc0]
   ││││││   0x1800086a2      48898424c8..   mov qword [rsp + 0xc8], rax
   ││││││   0x1800086aa      e997010000     jmp 0x180008846
   ││││││   0x1800086af      4183ff06       cmp r15d, 6                ; reloc.OLEAUT32.dll_SysStringLen
   ││││││   0x1800086b3      0f85da000000   jne 0x180008793
   ││││││   0x1800086b9      4183fb06       cmp r11d, 6                ; reloc.OLEAUT32.dll_SysStringLen
   ││││││   0x1800086bd      7527           jne 0x1800086e6
   ││││││   0x1800086bf      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x1800086c7      488d055a47..   lea rax, [0x1802ece28]
   ││││││   0x1800086ce      4c8d152347..   lea r10, [0x1802ecdf8]
   ││││││   0x1800086d5      4c0f45d0       cmovne r10, rax
   ││││││   0x1800086d9      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x1800086e1      e960010000     jmp 0x180008846
   ││││││   0x1800086e6      4183fb08       cmp r11d, 8                ; 8
   ││││││   0x1800086ea      7576           jne 0x180008762
   ││││││   0x1800086ec      83bc24c000..   cmp dword [rsp + 0xc0], 0
   ││││││   0x1800086f4      7445           je 0x18000873b
   ││││││   0x1800086f6      4183e2fb       and r10d, 0xfffffffb       ; 4294967291
   ││││││   0x1800086fa      4183fa09       cmp r10d, 9                ; 9
   ││││││   0x1800086fe      7427           je 0x180008727
   ││││││   0x180008700      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x180008708      488d05f147..   lea rax, [0x1802ecf00]
   ││││││   0x18000870f      4c8d15a247..   lea r10, [0x1802eceb8]
   ││││││   0x180008716      4c0f45d0       cmovne r10, rax
   ││││││   0x18000871a      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x180008722      e91f010000     jmp 0x180008846
   ││││││   0x180008727      488d051a48..   lea rax, [0x1802ecf48]
   ││││││   0x18000872e      48898424c8..   mov qword [rsp + 0xc8], rax
   ││││││   0x180008736      e90b010000     jmp 0x180008846
   ││││││   0x18000873b      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x180008743      488d056646..   lea rax, [0x1802ecdb0]
   ││││││   0x18000874a      4c8d152f46..   lea r10, [0x1802ecd80]
   ││││││   0x180008751      4c0f45d0       cmovne r10, rax
   ││││││   0x180008755      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x18000875d      e9e4000000     jmp 0x180008846
   ││││││   0x180008762      4183fb0a       cmp r11d, 0xa              ; 10
   ││││││   0x180008766      0f85ce000000   jne 0x18000883a
   ││││││   0x18000876c      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x180008774      488d054548..   lea rax, [0x1802ecfc0]
   ││││││   0x18000877b      4c8d15f647..   lea r10, [0x1802ecf78]
   ││││││   0x180008782      4c0f45d0       cmovne r10, rax
   ││││││   0x180008786      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x18000878e      e9b3000000     jmp 0x180008846
   ││││││   0x180008793      4183ff08       cmp r15d, 8                ; 8
   ││││││   0x180008797      0f859d000000   jne 0x18000883a
   ││││││   0x18000879d      4183fb08       cmp r11d, 8                ; 8
   ││││││   0x1800087a1      7552           jne 0x1800087f5
   ││││││   0x1800087a3      83bc24c000..   cmp dword [rsp + 0xc0], 0
   ││││││   0x1800087ab      7424           je 0x1800087d1
   ││││││   0x1800087ad      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x1800087b5      488d056448..   lea rax, [0x1802ed020]
   ││││││   0x1800087bc      4c8d154548..   lea r10, [0x1802ed008]
   ││││││   0x1800087c3      4c0f45d0       cmovne r10, rax
   ││││││   0x1800087c7      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x1800087cf      eb75           jmp 0x180008846
   ││││││   0x1800087d1      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x1800087d9      488d05a846..   lea rax, [0x1802ece88]
   ││││││   0x1800087e0      4c8d157146..   lea r10, [0x1802ece58]
   ││││││   0x1800087e7      4c0f45d0       cmovne r10, rax
   ││││││   0x1800087eb      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x1800087f3      eb51           jmp 0x180008846
   ││││││   0x1800087f5      4183fb0a       cmp r11d, 0xa              ; 10
   ││││││   0x1800087f9      753f           jne 0x18000883a
   ││││││   0x1800087fb      4183e2fb       and r10d, 0xfffffffb       ; 4294967291
   ││││││   0x1800087ff      4183fa09       cmp r10d, 9                ; 9
   ││││││   0x180008803      7424           je 0x180008829
   ││││││   0x180008805      83bc24e000..   cmp dword [rsp + 0xe0], 0
   ││││││   0x18000880d      488d053c48..   lea rax, [0x1802ed050]
   ││││││   0x180008814      4c8d151d48..   lea r10, [0x1802ed038]
   ││││││   0x18000881b      4c0f45d0       cmovne r10, rax
   ││││││   0x18000881f      4c899424c8..   mov qword [rsp + 0xc8], r10
   ││││││   0x180008827      eb1d           jmp 0x180008846
   ││││││   0x180008829      488d053848..   lea rax, [0x1802ed068]
   ││││││   0x180008830      48898424c8..   mov qword [rsp + 0xc8], rax
   ││││││   0x180008838      eb0c           jmp 0x180008846
   ││││││   0x18000883a      48c78424c8..   mov qword [rsp + 0xc8], 0
   ││││││   0x180008846      488b8424c8..   mov rax, qword [rsp + 0xc8]
   ││││││   0x18000884e      483b858807..   cmp rax, qword [rbp + 0x788]
   ││││││   0x180008855      741a           je 0x180008871
            0x180009604      f30f1095a8..   movss xmm2, dword [rbp + 0x7a8]
            0x18000960c      83bd6c0700..   cmp dword [rbp + 0x76c], 0
        ┌─< 0x180009613      7410           je 0x180009625
        │   0x180009615      410f28ca       movaps xmm1, xmm10
        │   0x180009619      f30f5c8d48..   subss xmm1, dword [rbp + 0x748]
        │   0x180009621      f30f59d1       mulss xmm2, xmm1
        └─> 0x180009625      4889e2         mov rdx, rsp
            0x180009628      488d842498..   lea rax, [rsp + 0x698]
            0x180009630      f30f108d84..   movss xmm1, dword [rbp + 0xa84]
            0x180009638      4c8d9424b0..   lea r10, [rsp + 0x6b0]
            0x180009640      488b8d1014..   mov rcx, qword [rbp + 0x1410]
            0x180009647      4c8d9c2460..   lea r11, [rsp + 0x860]
            0x18000964f      448b8db007..   mov r9d, dword [rbp + 0x7b0]
            0x180009656      48894220       mov qword [rdx + 0x20], rax
            0x18000965a      4c895228       mov qword [rdx + 0x28], r10
            0x18000965e      4c895a30       mov qword [rdx + 0x30], r11
            0x180009662      e8999c0100     call 0x180023300
            0x180009667      83bc244003..   cmp dword [rsp + 0x340], 0
        ┌─< 0x18000966f      7576           jne 0x1800096e7
        │   0x180009671      83bd900700..   cmp dword [rbp + 0x790], 0
       ┌──< 0x180009678      746d           je 0x1800096e7
       ││   0x18000967a      8b8544070000   mov eax, dword [rbp + 0x744]
       ││   0x180009680      4c8d9c24ac..   lea r11, [rsp + 0x3ac]
       ││   0x180009688      418903         mov dword [r11], eax
       ││   0x18000968b      4889e0         mov rax, rsp
       ││   0x18000968e      4533d2         xor r10d, r10d
       ││   0x180009691      83bd640700..   cmp dword [rbp + 0x764], 0
       ││   0x180009698      f30f108da0..   movss xmm1, dword [rbp + 0x7a0]
       ││   0x1800096a0      f30f109d84..   movss xmm3, dword [rbp + 0xa84]
       ││   0x1800096a8      4d0f44da       cmove r11, r10
       ││   0x1800096ac      488b8d1814..   mov rcx, qword [rbp + 0x1418]
       ││   0x1800096b3      4c8d942498..   lea r10, [rsp + 0x698]
       ││   0x1800096bb      4c895028       mov qword [rax + 0x28], r10
       ││   0x1800096bf      4c8d9424b0..   lea r10, [rsp + 0x6b0]
       ││   0x1800096c7      4c895030       mov qword [rax + 0x30], r10
       ││   0x1800096cb      4c8d942460..   lea r10, [rsp + 0x860]
       ││   0x1800096d3      448b859807..   mov r8d, dword [rbp + 0x798]
       ││   0x1800096da      4c895820       mov qword [rax + 0x20], r11
       ││   0x1800096de      4c895038       mov qword [rax + 0x38], r10
       ││   0x1800096e2      e8a9a20100     call 0x180023990
       └└─> 0x1800096e7      f30f10b500..   movss xmm6, dword [rbp + 0x700]
            0x1800096ef      8b85800a0000   mov eax, dword [rbp + 0xa80]
            0x1800096f5      898424f800..   mov dword [rsp + 0xf8], eax
            0x1800096fc      83bdb80700..   cmp dword [rbp + 0x7b8], 0
            0x180009703      f30f58b508..   addss xmm6, dword [rbp + 0x708]
            0x18000970b      7516           jne 0x180009723
            0x18000970d      83bdc80700..   cmp dword [rbp + 0x7c8], 0
            0x180009714      750d           jne 0x180009723
            0x180009716      83bde00700..   cmp dword [rbp + 0x7e0], 0
            0x18000971d      0f84b9020000   je 0x1800099dc
            0x180009723      450f28f9       movaps xmm15, xmm9
            0x180009727      660fefc9       pxor xmm1, xmm1
            0x18000972b      f30f10a530..   movss xmm4, dword [rbp + 0x730]
            0x180009733      440f28f1       movaps xmm14, xmm1
            0x180009737      f30f11a424..   movss dword [rsp + 0xd8], xmm4
            0x180009740      f3440f5df9     minss xmm15, xmm1
            0x180009745      f3450f5ff1     maxss xmm14, xmm9
            0x18000974a      f30f108d38..   movss xmm1, dword [rbp + 0x738]
            0x180009752      f30f118c24..   movss dword [rsp + 0x654], xmm1 ; [0x654:4]=-1 ; 1620
            0x18000975b      83bd340a00..   cmp dword [rbp + 0xa34], 0
            0x180009762      7542           jne 0x1800097a6
            0x180009764      486385c007..   movsxd rax, dword [rbp + 0x7c0]
            0x18000976b      4c8d150e30..   lea r10, [0x1802ec780]
            0x180009772      83bdc80700..   cmp dword [rbp + 0x7c8], 0
            0x180009779      48c78424c8..   mov qword [rsp + 0xc8], 0
            0x180009785      488d04c0       lea rax, [rax + rax*8]
            0x180009789      4d8d1cc2       lea r11, [r10 + rax*8]
            0x18000978d      4d0f45d3       cmovne r10, r11
            0x180009791      4c899424c0..   mov qword [rsp + 0xc0], r10
            0x180009799      4c8d15c02c..   lea r10, [0x1802ec460]
            0x1800097a0      498d04c2       lea rax, [r10 + rax*8]
            0x1800097a4      eb62           jmp 0x180009808
            0x1800097a6      488b4530       mov rax, qword [rbp + 0x30]
            0x1800097aa      4989e3         mov r11, rsp
            0x1800097ad      488d8d380a..   lea rcx, [rbp + 0xa38]
            0x1800097b4      4c8d842464..   lea r8, [rsp + 0x364]
            0x1800097bc      f30f108890..   movss xmm1, dword [rax + 0x90]
            0x1800097c4      4c8d8c2428..   lea r9, [rsp + 0x628]
            0x1800097cc      4c8d942454..   lea r10, [rsp + 0x654]
            0x1800097d4      4d895320       mov qword [r11 + 0x20], r10
            0x18002c7b3      4183bd7c07..   cmp dword [r13 + 0x77c], 0
        ┌─< 0x18002c7bb      7429           je 0x18002c7e6
        │   0x18002c7bd      83fa02         cmp edx, 2                 ; reloc.OLEAUT32.dll_SysReAllocString
       ┌──< 0x18002c7c0      751f           jne 0x18002c7e1
       ││   0x18002c7c2      418b859c0e..   mov eax, dword [r13 + 0xe9c]
       ││   0x18002c7c9      837d0400       cmp dword [rbp + 4], 0
      ┌───< 0x18002c7cd      7509           jne 0x18002c7d8
      │││   0x18002c7cf      41b001         mov r8b, 1
      │││   0x18002c7d2      85c0           test eax, eax
     ┌────< 0x18002c7d4      7527           jne 0x18002c7fd
    ┌─────< 0x18002c7d6      eb20           jmp 0x18002c7f8
    ││└───> 0x18002c7d8      4532c0         xor r8b, r8b
    ││ ││   0x18002c7db      85c0           test eax, eax
    ││┌───< 0x18002c7dd      751e           jne 0x18002c7fd
   ┌──────< 0x18002c7df      eb17           jmp 0x18002c7f8
   ││││└──> 0x18002c7e1      4532c0         xor r8b, r8b
   ││││┌──< 0x18002c7e4      eb12           jmp 0x18002c7f8
   │││││└─> 0x18002c7e6      4532c0         xor r8b, r8b
   │││││    0x18002c7e9      4183bd9c0e..   cmp dword [r13 + 0xe9c], 0
   │││││┌─< 0x18002c7f1      7405           je 0x18002c7f8
   ││││││   0x18002c7f3      83fa02         cmp edx, 2                 ; reloc.OLEAUT32.dll_SysReAllocString
  ┌───────< 0x18002c7f6      7405           je 0x18002c7fd
  │└└──└└─> 0x18002c7f8      4532ff         xor r15b, r15b
  │  ││ ┌─< 0x18002c7fb      eb1f           jmp 0x18002c81c
  └──└└───> 0x18002c7fd      4889e9         mov rcx, rbp
        │   0x18002c800      498d95a80e..   lea rdx, [r13 + 0xea8]
        │   0x18002c807      4488442478     mov byte [rsp + 0x78], r8b
        │   0x18002c80c      41b701         mov r15b, 1
        │   0x18002c80f      e82c07feff     call 0x18000cf40
        │   0x18002c814      448a442478     mov r8b, byte [rsp + 0x78]
        │   0x18002c819      8b5508         mov edx, dword [rbp + 8]
        └─> 0x18002c81c      4584c0         test r8b, r8b
        ┌─< 0x18002c81f      7509           jne 0x18002c82a
        │   0x18002c821      448b4504       mov r8d, dword [rbp + 4]
       ┌──< 0x18002c825      e98a010000     jmp 0x18002c9b4
       │└─> 0x18002c82a      418b859007..   mov eax, dword [r13 + 0x790]
       │    0x18002c831      4c89e9         mov rcx, r13
       │    0x18002c834      89442478       mov dword [rsp + 0x78], eax
       │    0x18002c838      8994248000..   mov dword [rsp + 0x80], edx
       │    0x18002c83f      e83cb1fdff     call 0x180007980
       │    0x18002c844      8b94248000..   mov edx, dword [rsp + 0x80]
       │    0x18002c84b      4189c3         mov r11d, eax
       │    0x18002c84e      4989e2         mov r10, rsp
       │    0x18002c851      4533c0         xor r8d, r8d
       │    0x18002c854      418b851c01..   mov eax, dword [r13 + 0x11c]
       │    0x18002c85b      89d1           mov ecx, edx
       │    0x18002c85d      83e0fb         and eax, 0xfffffffb        ; 4294967291
       │    0x18002c860      4489da         mov edx, r11d
       │    0x18002c863      83f809         cmp eax, 9                 ; 9
       │    0x18002c866      8984248000..   mov dword [rsp + 0x80], eax
       │    0x18002c86d      410f94c0       sete r8b
       │    0x18002c871      45894220       mov dword [r10 + 0x20], r8d
       │    0x18002c875      448b4c2478     mov r9d, dword [rsp + 0x78]
       │    0x18002c87a      e8c1affdff     call 0x180007840
       │    0x18002c87f      4989c1         mov r9, rax
       │    0x18002c882      4d3b8d8807..   cmp r9, qword [r13 + 0x788]
       │┌─< 0x18002c889      7424           je 0x18002c8af
       ││   0x18002c88b      488d15de12..   lea rdx, [0x1802edb70]
       ││   0x18002c892      498b8db813..   mov rcx, qword [r13 + 0x13b8]
       ││   0x18002c899      4d898d8807..   mov qword [r13 + 0x788], r9 ; [0x788:8]=-1 ; 1928
       ││   0x18002c8a0      4c894c2478     mov qword [rsp + 0x78], r9
       ││   0x18002c8a5      e8064ffeff     call 0x1800117b0
       ││   0x18002c8aa      4c8b4c2478     mov r9, qword [rsp + 0x78]
       │└─> 0x18002c8af      4d8b5d30       mov r11, qword [r13 + 0x30]
       │    0x18002c8b3      4889e0         mov rax, rsp
       │    0x18002c8b6      498b8db813..   mov rcx, qword [r13 + 0x13b8]
       │    0x18002c8bd      488d5508       lea rdx, [rbp + 8]
       │    0x18002c8c1      4d8b839800..   mov r8, qword [r11 + 0x98]
       │    0x18002c8c8      4d8b5350       mov r10, qword [r11 + 0x50]
       │    0x18002c8cc      4533db         xor r11d, r11d
       │    0x18002c8cf      4c894028       mov qword [rax + 0x28], r8
       │    0x18002c8d3      4c895020       mov qword [rax + 0x20], r10
       │    0x18002c8d7      4c8d159212..   lea r10, [0x1802edb70]
       │    0x18002c8de      4c8b4530       mov r8, qword [rbp + 0x30]
       │    0x18002c8e2      4c895030       mov qword [rax + 0x30], r10
       │    0x18002c8e6      44895838       mov dword [rax + 0x38], r11d
       │    0x18002c8ea      44895840       mov dword [rax + 0x40], r11d
       │    0x18002c8ee      48897048       mov qword [rax + 0x48], rsi
       │    0x18002c8f2      e8c94effff     call 0x1800217c0
       │    0x18002c8f7      4584ff         test r15b, r15b
       │    0x18002c8fa      0f8597000000   jne 0x18002c997
       │    0x18002c900      4533ff         xor r15d, r15d
       │    0x18002c903      837d0c00       cmp dword [rbp + 0xc], 0
       │    0x18002c907      0f8681000000   jbe 0x18002c98e
       │    0x18002c90d      c745000000..   mov dword [rbp], 0
       │    0x18002c914      4c89642478     mov qword [rsp + 0x78], r12
       │    0x18002c919      488b4518       mov rax, qword [rbp + 0x18]
       │    0x18002c91d      4589fc         mov r12d, r15d
       │    0x18002c920      488b5018       mov rdx, qword [rax + 0x18]
       │    0x18002c924      4a8b0ce2       mov rcx, qword [rdx + r12*8]
       │    0x18002c928      8b5528         mov edx, dword [rbp + 0x28]
       │    0x18002c92b      4c8d410f       lea r8, [rcx + 0xf]
       │    0x18002c92f      4983e0f0       and r8, 0xfffffffffffffff0
       │    0x18002c933      493bc8         cmp rcx, r8
       │    0x18002c936      750c           jne 0x18002c944
       │    0x18002c938      f6c203         test dl, 3                 ; 3
       │    0x18002c93b      7507           jne 0x18002c944
       │    0x18002c93d      e8cedf0b00     call 0x1800ea910
       │    0x18002c942      eb05           jmp 0x18002c949
       │    0x18002c944      e8c7df0b00     call 0x1800ea910
