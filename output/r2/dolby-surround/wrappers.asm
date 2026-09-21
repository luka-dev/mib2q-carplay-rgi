            0x18011d560      0000           add byte [rax], al
        ┌─< 0x18011d562      7200           jb 0x18011d564
        └─> 0x18011d564      0000           add byte [rax], al
            0x18011d566      440f1085e0..   movups xmm8, xmmword [rbp + 0x7e0]
            0x18011d56e      4889bdf007..   mov qword [rbp + 0x7f0], rdi
            0x18011d575      48c785f807..   mov qword [rbp + 0x7f8], 0x2e ; '.' ; 46
            0x18011d580      0f10bdf007..   movups xmm7, xmmword [rbp + 0x7f0]
            0x18011d587      488d05128b..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011d58e      4889850008..   mov qword [rbp + 0x800], rax
            0x18011d595      48c7850808..   mov qword [rbp + 0x808], 0x42 ; 'B' ; 66
            0x18011d5a0      0f10b50008..   movups xmm6, xmmword [rbp + 0x800]
            0x18011d5a7      418b5708       mov edx, dword [r15 + 8]
            0x18011d5ab      b901000000     mov ecx, 1
            0x18011d5b0      e8bb540300     call 0x180152a70
            0x18011d5b5      440f29442460   movaps xmmword [rsp + 0x60], xmm8
            0x18011d5bb      0f297c2470     movaps xmmword [rsp + 0x70], xmm7
            0x18011d5c0      0f297580       movaps xmmword [rbp - 0x80], xmm6
            0x18011d5c4      488d4c2460     lea rcx, [rsp + 0x60]
            0x18011d5c9      48894c2428     mov qword [rsp + 0x28], rcx
            0x18011d5ce      488d4c2470     lea rcx, [rsp + 0x70]
            0x18011d5d3      48894c2420     mov qword [rsp + 0x20], rcx
            0x18011d5d8      41b9ed000000   mov r9d, 0xed              ; 237
            0x18011d5de      4c8d4580       lea r8, [rbp - 0x80]
            0x18011d5e2      b203           mov dl, 3
            0x18011d5e4      488bc8         mov rcx, rax
            0x18011d5e7      e894d3ffff     call 0x18011a980
            0x18011d5ec      48899d900f..   mov qword [rbp + 0xf90], rbx
            0x18011d5f3      488d05966b..   lea rax, [0x180004190]
            0x18011d5fa      488985980f..   mov qword [rbp + 0xf98], rax
            0x18011d601      488d8d900f..   lea rcx, [rbp + 0xf90]
            0x18011d608      48898dc80f..   mov qword [rbp + 0xfc8], rcx
            0x18011d60f      4180bdb405..   cmp byte [r13 + 0x5b4], 0
        ┌─< 0x18011d617      7436           je 0x18011d64f
        │   0x18011d619      410fb685b5..   movzx eax, byte [r13 + 0x5b5]
        │   0x18011d621      8985d01e0000   mov dword [rbp + 0x1ed0], eax
        │   0x18011d627      4c89a5401d..   mov qword [rbp + 0x1d40], r12
        │   0x18011d62e      4c8d85d01e..   lea r8, [rbp + 0x1ed0]
        │   0x18011d635      488d95401d..   lea rdx, [rbp + 0x1d40]
        │   0x18011d63c      488d8d900f..   lea rcx, [rbp + 0xf90]
        │   0x18011d643      e8a8bb0000     call 0x1801291f0
        │   0x18011d648      488b8dc80f..   mov rcx, qword [rbp + 0xfc8]
        └─> 0x18011d64f      4885c9         test rcx, rcx
        ┌─< 0x18011d652      7421           je 0x18011d675
        │   0x18011d654      488b01         mov rax, qword [rcx]
        │   0x18011d657      488d95900f..   lea rdx, [rbp + 0xf90]
        │   0x18011d65e      483bca         cmp rcx, rdx
        │   0x18011d661      0f95c2         setne dl
        │   0x18011d664      488b4020       mov rax, qword [rax + 0x20]
        │   0x18011d668      ff15622f0700   call qword [0x1801905d0]   ; [0x1801905d0:8]=0x18018d190
        │   0x18011d66e      4c89b5c80f..   mov qword [rbp + 0xfc8], r14
        └─> 0x18011d675      4180bdb405..   cmp byte [r13 + 0x5b4], 0
            0x18011d67d      0f84b0000000   je 0x18011d733
            0x18011d683      498d9db505..   lea rbx, [r13 + 0x5b5]
            0x18011d68a      488d05ff91..   lea rax, str.dap_vr_surround_decoder_enable_set:__d ; 0x1803e6890 ; "dap_vr_surround_decoder_enable_set: %d"
            0x18011d691      4889851008..   mov qword [rbp + 0x810], rax
            0x18011d698      48c7851808..   mov qword [rbp + 0x818], 0x26 ; '&' ; 38
            0x18011d6a3      440f108510..   movups xmm8, xmmword [rbp + 0x810]
            0x18011d6ab      4889bd2008..   mov qword [rbp + 0x820], rdi
            0x18011d6b2      48c7852808..   mov qword [rbp + 0x828], 0x2e ; '.' ; 46
            0x18011d6bd      0f10bd2008..   movups xmm7, xmmword [rbp + 0x820]
            0x18011d6c4      488d05d589..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011d6cb      4889853008..   mov qword [rbp + 0x830], rax
            0x18011d6d2      48c7853808..   mov qword [rbp + 0x838], 0x42 ; 'B' ; 66
            0x18011d6dd      0f10b53008..   movups xmm6, xmmword [rbp + 0x830]
            0x18011d6e4      418b5708       mov edx, dword [r15 + 8]
            0x18011d6e8      b901000000     mov ecx, 1
            0x18011d6ed      e87e530300     call 0x180152a70
            0x18011d6f2      440f29442460   movaps xmmword [rsp + 0x60], xmm8
            0x18011d6f8      0f297c2470     movaps xmmword [rsp + 0x70], xmm7
            0x18011d6fd      0f297580       movaps xmmword [rbp - 0x80], xmm6
            0x18011d701      48895c2430     mov qword [rsp + 0x30], rbx
            0x18011d706      488d4c2460     lea rcx, [rsp + 0x60]
            0x18011d70b      48894c2428     mov qword [rsp + 0x28], rcx
            0x18011d710      488d4c2470     lea rcx, [rsp + 0x70]
            0x18011d715      48894c2420     mov qword [rsp + 0x20], rcx
            0x18011d71a      41b9ef000000   mov r9d, 0xef              ; 239
            0x18011d720      4c8d4580       lea r8, [rbp - 0x80]
            0x18011d724      488bc8         mov rcx, rax
            0x18011d727      e8f4920000     call 0x180126a20
            0x18011d72c      488d1dddc0..   lea rbx, [0x1803e9810]
            0x18011d733      48899d500f..   mov qword [rbp + 0xf50], rbx
            0x18011d73a      488d057f6c..   lea rax, [0x1800043c0]
            0x18011d741      488985580f..   mov qword [rbp + 0xf58], rax
            0x18011d748      488d8d500f..   lea rcx, [rbp + 0xf50]
            0x18011d74f      48898d880f..   mov qword [rbp + 0xf88], rcx
            0x18011d756      4180bd380b..   cmp byte [r13 + 0xb38], 0
            0x18011d75e      7436           je 0x18011d796
            0x18011d760      410fb68539..   movzx eax, byte [r13 + 0xb39]
            0x18011d768      8985c81e0000   mov dword [rbp + 0x1ec8], eax
            0x18011d76e      4c89a5481d..   mov qword [rbp + 0x1d48], r12
            0x18011d775      4c8d85c81e..   lea r8, [rbp + 0x1ec8]
            0x18011d77c      488d95481d..   lea rdx, [rbp + 0x1d48]
            0x18011d783      488d8d500f..   lea rcx, [rbp + 0xf50]
            0x18011d78a      e861ba0000     call 0x1801291f0
            0x18011d78f      488b8d880f..   mov rcx, qword [rbp + 0xf88]
            0x18011d796      4885c9         test rcx, rcx
            0x18011d799      7421           je 0x18011d7bc
            0x18011d79b      488b01         mov rax, qword [rcx]
            0x18011d79e      488d95500f..   lea rdx, [rbp + 0xf50]
            0x18011d7a5      483bca         cmp rcx, rdx
            0x18011d7a8      0f95c2         setne dl
            0x18011d7ab      488b4020       mov rax, qword [rax + 0x20]
            0x18011d7af      ff151b2e0700   call qword [0x1801905d0]   ; [0x1801905d0:8]=0x18018d190
            0x18011d7b5      4c89b5880f..   mov qword [rbp + 0xf88], r14
            0x18011d7bc      4180bd380b..   cmp byte [r13 + 0xb38], 0
            0x18011d7c4      0f84b0000000   je 0x18011d87a
            0x18011d7ca      498d9d390b..   lea rbx, [r13 + 0xb39]
            0x18011d7d1      488d050891..   lea rax, str.dap_vr_bass_enhancer_enable_set:__d ; 0x1803e68e0 ; "dap_vr_bass_enhancer_enable_set: %d"
            0x18011d7d8      4889854008..   mov qword [rbp + 0x840], rax
            0x18011d7df      48c7854808..   mov qword [rbp + 0x848], 0x23 ; '#' ; 35
            0x18011d7ea      440f108540..   movups xmm8, xmmword [rbp + 0x840]
            0x18011d7f2      4889bd5008..   mov qword [rbp + 0x850], rdi
            0x18011d7f9      48c7855808..   mov qword [rbp + 0x858], 0x2e ; '.' ; 46
            0x18011d804      0f10bd5008..   movups xmm7, xmmword [rbp + 0x850]
            0x18011d80b      488d058e88..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011d812      4889856008..   mov qword [rbp + 0x860], rax
            0x18011d819      48c7856808..   mov qword [rbp + 0x868], 0x42 ; 'B' ; 66
            0x18011d824      0f10b56008..   movups xmm6, xmmword [rbp + 0x860]
            0x18011d82b      418b5708       mov edx, dword [r15 + 8]
            0x18011d82f      b901000000     mov ecx, 1
            0x18011d834      e837520300     call 0x180152a70
            0x18011d839      440f29442460   movaps xmmword [rsp + 0x60], xmm8
            0x18011d83f      0f297c2470     movaps xmmword [rsp + 0x70], xmm7
            0x18011d844      0f297580       movaps xmmword [rbp - 0x80], xmm6
            0x18011d848      48895c2430     mov qword [rsp + 0x30], rbx
            0x18011d84d      488d4c2460     lea rcx, [rsp + 0x60]
            0x18011d852      48894c2428     mov qword [rsp + 0x28], rcx
            0x18011d857      488d4c2470     lea rcx, [rsp + 0x70]
            0x18011d85c      48894c2420     mov qword [rsp + 0x20], rcx
            0x18011d861      41b9f0000000   mov r9d, 0xf0              ; 240
            0x18011d867      4c8d4580       lea r8, [rbp - 0x80]
            0x18011d86b      488bc8         mov rcx, rax
            0x18011d86e      e8ad910000     call 0x180126a20
            0x18011d873      488d1d96bf..   lea rbx, [0x1803e9810]
            0x18011d87a      48899d100f..   mov qword [rbp + 0xf10], rbx
            0x18011d881      488d051895..   lea rax, [0x180006da0]
            0x18011d888      488985180f..   mov qword [rbp + 0xf18], rax
            0x18011d88f      488d8d100f..   lea rcx, [rbp + 0xf10]
            0x18011d896      48898d480f..   mov qword [rbp + 0xf48], rcx
            0x18011d89d      4180bde005..   cmp byte [r13 + 0x5e0], 0
            0x18011d8a5      7436           je 0x18011d8dd
            0x18011d8a7      410fb685e1..   movzx eax, byte [r13 + 0x5e1]
            0x18011d8af      8985c01e0000   mov dword [rbp + 0x1ec0], eax
            0x18011d8b5      4c89a5501d..   mov qword [rbp + 0x1d50], r12
            0x18011d8bc      4c8d85c01e..   lea r8, [rbp + 0x1ec0]
            0x18011d8c3      488d95501d..   lea rdx, [rbp + 0x1d50]
            0x18011d8ca      488d8d100f..   lea rcx, [rbp + 0xf10]
            0x18011d8d1      e81ab90000     call 0x1801291f0
            0x18011d8d6      488b8d480f..   mov rcx, qword [rbp + 0xf48]
            0x18011d8dd      4885c9         test rcx, rcx
            0x18011e870      842c00         test byte [rax + rax], ch
            0x18011e873      488985b00a..   mov qword [rbp + 0xab0], rax
            0x18011e87a      48c785b80a..   mov qword [rbp + 0xab8], 0x1f ; 31
            0x18011e885      440f1085b0..   movups xmm8, xmmword [rbp + 0xab0]
            0x18011e88d      4889bdc00a..   mov qword [rbp + 0xac0], rdi
            0x18011e894      48c785c80a..   mov qword [rbp + 0xac8], 0x2e ; '.' ; 46
            0x18011e89f      0f10bdc00a..   movups xmm7, xmmword [rbp + 0xac0]
            0x18011e8a6      488d05f377..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011e8ad      488985d00a..   mov qword [rbp + 0xad0], rax
            0x18011e8b4      48c785d80a..   mov qword [rbp + 0xad8], 0x42 ; 'B' ; 66
            0x18011e8bf      0f10b5d00a..   movups xmm6, xmmword [rbp + 0xad0]
            0x18011e8c6      418b5708       mov edx, dword [r15 + 8]
            0x18011e8ca      b901000000     mov ecx, 1
            0x18011e8cf      e89c410300     call 0x180152a70
            0x18011e8d4      440f29442460   movaps xmmword [rsp + 0x60], xmm8
            0x18011e8da      0f297c2470     movaps xmmword [rsp + 0x70], xmm7
            0x18011e8df      0f297580       movaps xmmword [rbp - 0x80], xmm6
            0x18011e8e3      48895c2430     mov qword [rsp + 0x30], rbx
            0x18011e8e8      488d4c2460     lea rcx, [rsp + 0x60]
            0x18011e8ed      48894c2428     mov qword [rsp + 0x28], rcx
            0x18011e8f2      488d4c2470     lea rcx, [rsp + 0x70]
            0x18011e8f7      48894c2420     mov qword [rsp + 0x20], rcx
            0x18011e8fc      41b907010000   mov r9d, 0x107             ; 263
            0x18011e902      4c8d4580       lea r8, [rbp - 0x80]
            0x18011e906      488bc8         mov rcx, rax
            0x18011e909      e812810000     call 0x180126a20
            0x18011e90e      488d1dfbae..   lea rbx, [0x1803e9810]
            0x18011e915      48899df018..   mov qword [rbp + 0x18f0], rbx
            0x18011e91c      488d05fd57..   lea rax, [0x180004120]
            0x18011e923      488985f818..   mov qword [rbp + 0x18f8], rax
            0x18011e92a      488d8df018..   lea rcx, [rbp + 0x18f0]
            0x18011e931      48898d2819..   mov qword [rbp + 0x1928], rcx
            0x18011e938      4180bdd805..   cmp byte [r13 + 0x5d8], 0
        ┌─< 0x18011e940      7435           je 0x18011e977
        │   0x18011e942      418b85dc05..   mov eax, dword [r13 + 0x5dc]
        │   0x18011e949      8985e81f0000   mov dword [rbp + 0x1fe8], eax
        │   0x18011e94f      4c89a5b81d..   mov qword [rbp + 0x1db8], r12
        │   0x18011e956      4c8d85e81f..   lea r8, [rbp + 0x1fe8]
        │   0x18011e95d      488d95b81d..   lea rdx, [rbp + 0x1db8]
        │   0x18011e964      488d8df018..   lea rcx, [rbp + 0x18f0]
        │   0x18011e96b      e880a80000     call 0x1801291f0
        │   0x18011e970      488b8d2819..   mov rcx, qword [rbp + 0x1928]
        └─> 0x18011e977      4885c9         test rcx, rcx
            0x18011e97a      7421           je 0x18011e99d
            0x18011e97c      488b01         mov rax, qword [rcx]
            0x18011e97f      488d95f018..   lea rdx, [rbp + 0x18f0]
            0x18011e986      483bca         cmp rcx, rdx
            0x18011e989      0f95c2         setne dl
            0x18011e98c      488b4020       mov rax, qword [rax + 0x20]
            0x18011e990      ff153a1c0700   call qword [0x1801905d0]   ; [0x1801905d0:8]=0x18018d190
            0x18011e996      4c89b52819..   mov qword [rbp + 0x1928], r14
            0x18011e99d      4180bdd805..   cmp byte [r13 + 0x5d8], 0
            0x18011e9a5      0f84b2000000   je 0x18011ea5d
            0x18011e9ab      498d9ddc05..   lea rbx, [r13 + 0x5dc]
            0x18011e9b2      488d05b783..   lea rax, str.dap_vr_surround_boost_set:__d ; 0x1803e6d70 ; "dap_vr_surround_boost_set: %d"
            0x18011e9b9      488985e00a..   mov qword [rbp + 0xae0], rax
            0x18011e9c0      48c785e80a..   mov qword [rbp + 0xae8], 0x1d ; 29
            0x18011e9cb      440f1085e0..   movups xmm8, xmmword [rbp + 0xae0]
            0x18011e9d3      4889bdf00a..   mov qword [rbp + 0xaf0], rdi
            0x18011e9da      48c785f80a..   mov qword [rbp + 0xaf8], 0x2e ; '.' ; 46
            0x18011e9e5      0f10bdf00a..   movups xmm7, xmmword [rbp + 0xaf0]
            0x18011e9ec      488d05ad76..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011e9f3      488985000b..   mov qword [rbp + 0xb00], rax
            0x18011e9fa      48c785080b..   mov qword [rbp + 0xb08], 0x42 ; 'B' ; 66
            0x18011ea05      0f10b5000b..   movups xmm6, xmmword [rbp + 0xb00]
            0x18011ea0c      418b5708       mov edx, dword [r15 + 8]
            0x18011ea10      b901000000     mov ecx, 1
            0x18011ea15      e856400300     call 0x180152a70
            0x18011ea1a      440f29442460   movaps xmmword [rsp + 0x60], xmm8
            0x18011ea20      0f297c2470     movaps xmmword [rsp + 0x70], xmm7
            0x18011ea25      0f297580       movaps xmmword [rbp - 0x80], xmm6
            0x18011ea29      48895c2430     mov qword [rsp + 0x30], rbx
            0x18011ea2e      488d4c2460     lea rcx, [rsp + 0x60]
            0x18011ea33      48894c2428     mov qword [rsp + 0x28], rcx
            0x18011ea38      488d4c2470     lea rcx, [rsp + 0x70]
            0x18011ea3d      48894c2420     mov qword [rsp + 0x20], rcx
            0x18011ea42      41b90a010000   mov r9d, 0x10a             ; 266
            0x18011ea48      4c8d4580       lea r8, [rbp - 0x80]
            0x18011ea4c      b204           mov dl, 4
            0x18011ea4e      488bc8         mov rcx, rax
            0x18011ea51      e8cac0ffff     call 0x18011ab20
            0x18011ea56      488d1db3ad..   lea rbx, [0x1803e9810]
            0x18011ea5d      48899d3019..   mov qword [rbp + 0x1930], rbx
            0x18011ea64      488d05d526..   lea rax, [0x180001140]
            0x18011ea6b      4889853819..   mov qword [rbp + 0x1938], rax
            0x18011ea72      488d8d3019..   lea rcx, [rbp + 0x1930]
            0x18011ea79      48898d6819..   mov qword [rbp + 0x1968], rcx
            0x18011ea80      4180bde805..   cmp byte [r13 + 0x5e8], 0
            0x18011ea88      7435           je 0x18011eabf
            0x18011ea8a      418b85ec05..   mov eax, dword [r13 + 0x5ec]
            0x18011ea91      8985e01f0000   mov dword [rbp + 0x1fe0], eax
            0x18011ea97      4c89a5c01d..   mov qword [rbp + 0x1dc0], r12
            0x18011ea9e      4c8d85e01f..   lea r8, [rbp + 0x1fe0]
            0x18011eaa5      488d95c01d..   lea rdx, [rbp + 0x1dc0]
            0x18011eaac      488d8d3019..   lea rcx, [rbp + 0x1930]
            0x18011eab3      e838a70000     call 0x1801291f0
            0x18011eab8      488b8d6819..   mov rcx, qword [rbp + 0x1968]
            0x18011eabf      4885c9         test rcx, rcx
            0x18011eac2      7421           je 0x18011eae5
            0x18011eac4      488b01         mov rax, qword [rcx]
            0x180130550      48895c2418     mov qword [rsp + 0x18], rbx
            0x180130555      4889742420     mov qword [rsp + 0x20], rsi
            0x18013055a      55             push rbp
            0x18013055b      57             push rdi
            0x18013055c      4156           push r14
            0x18013055e      488d6c24b9     lea rbp, [rsp - 0x47]
            0x180130563      4881ecf000..   sub rsp, 0xf0
            0x18013056a      488b050f2e..   mov rax, qword [0x180413380] ; [0x180413380:8]=0x2b992ddfa232
            0x180130571      4833c4         xor rax, rsp
            0x180130574      48894537       mov qword [rbp + 0x37], rax
            0x180130578      0f57c0         xorps xmm0, xmm0
            0x18013057b      48c745af3a..   mov qword [rbp - 0x51], 0x3a ; ':' ; 58
            0x180130583      488bfa         mov rdi, rdx
            0x180130586      48c7459f47..   mov qword [rbp - 0x61], 0x47 ; 'G' ; 71
            0x18013058e      8b5120         mov edx, dword [rcx + 0x20]
            0x180130591      488d3578b5..   lea rsi, str.dolby::CDolbyAudioProcessingModule::GetStereoBypassAllowed ; 0x1803ebb10 ; "dolby::CDolbyAudioProcessingModule::GetStereoBypassAllowed"
            0x180130598      488bd9         mov rbx, rcx
            0x18013059b      488975a7       mov qword [rbp - 0x59], rsi
            0x18013059f      4c8d351a9a..   lea r14, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDefaultDAPModule.cpp ; 0x1803e9fc0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DefaultDAPModule.cpp"
            0x1801305a6      b901000000     mov ecx, 1
            0x1801305ab      0f1145c7       movups xmmword [rbp - 0x39], xmm0
            0x1801305af      4c897597       mov qword [rbp - 0x69], r14
            0x1801305b3      0f1145d7       movups xmmword [rbp - 0x29], xmm0
            0x1801305b7      0f1145e7       movups xmmword [rbp - 0x19], xmm0
            0x1801305bb      0f1145f7       movups xmmword [rbp - 9], xmm0
            0x1801305bf      0f114507       movups xmmword [rbp + 7], xmm0
            0x1801305c3      0f114517       movups xmmword [rbp + 0x17], xmm0
            0x1801305c7      0f114527       movups xmmword [rbp + 0x27], xmm0
            0x1801305cb      e8a0240200     call 0x180152a70
            0x1801305d0      0f2845a7       movaps xmm0, xmmword [rbp - 0x59]
            0x1801305d4      488d4db7       lea rcx, [rbp - 0x49]
            0x1801305d8      0f284d97       movaps xmm1, xmmword [rbp - 0x69]
            0x1801305dc      4c8d4597       lea r8, [rbp - 0x69]
            0x1801305e0      48894c2420     mov qword [rsp + 0x20], rcx
            0x1801305e5      41b978040000   mov r9d, 0x478             ; 1144
            0x1801305eb      488d4dc7       lea rcx, [rbp - 0x39]
            0x1801305ef      660f7f45b7     movdqa xmmword [rbp - 0x49], xmm0
            0x1801305f4      488bd0         mov rdx, rax
            0x1801305f7      660f7f4d97     movdqa xmmword [rbp - 0x69], xmm1
            0x1801305fc      e8ff7a0200     call 0x180158100
            0x180130601      4885ff         test rdi, rdi
        ┌─< 0x180130604      750a           jne 0x180130610
        │   0x180130606      bb03400080     mov ebx, 0x80004003
       ┌──< 0x18013060b      e91a010000     jmp 0x18013072a
       │└─> 0x180130610      80bbd90200..   cmp byte [rbx + 0x2d9], 0
       │┌─< 0x180130617      7417           je 0x180130630
       ││   0x180130619      488b833005..   mov rax, qword [rbx + 0x530]
       ││   0x180130620      4885c0         test rax, rax
      ┌───< 0x180130623      740b           je 0x180130630
      │││   0x180130625      0fb6809c00..   movzx eax, byte [rax + 0x9c]
      │││   0x18013062c      8807           mov byte [rdi], al
     ┌────< 0x18013062e      eb7c           jmp 0x1801306ac
     │└─└─> 0x180130630      c60701         mov byte [rdi], 1
     │ │    0x180130633      488d0556b5..   lea rax, str.Bypass_forced:_configuration_did_not_succeed. ; 0x1803ebb90 ; "Bypass forced: configuration did not succeed."
     │ │    0x18013063a      8b5320         mov edx, dword [rbx + 0x20]
     │ │    0x18013063d      b901000000     mov ecx, 1
     │ │    0x180130642      48894597       mov qword [rbp - 0x69], rax
     │ │    0x180130646      48c7459f2d..   mov qword [rbp - 0x61], 0x2d ; '-' ; 45
     │ │    0x18013064e      488975a7       mov qword [rbp - 0x59], rsi
     │ │    0x180130652      48c745af3a..   mov qword [rbp - 0x51], 0x3a ; ':' ; 58
     │ │    0x18013065a      4c8975b7       mov qword [rbp - 0x49], r14
     │ │    0x18013065e      48c745bf47..   mov qword [rbp - 0x41], 0x47 ; 'G' ; 71
     │ │    0x180130666      e805240200     call 0x180152a70
     │ │    0x18013066b      0f284597       movaps xmm0, xmmword [rbp - 0x69]
     │ │    0x18013066f      488d4d97       lea rcx, [rbp - 0x69]
     │ │    0x180130673      0f284da7       movaps xmm1, xmmword [rbp - 0x59]
     │ │    0x180130677      4c8d45b7       lea r8, [rbp - 0x49]
     │ │    0x18013067b      48894c2428     mov qword [rsp + 0x28], rcx
     │ │    0x180130680      41b982040000   mov r9d, 0x482             ; 1154
     │ │    0x180130686      488d4da7       lea rcx, [rbp - 0x59]
     │ │    0x18013068a      660f7f4597     movdqa xmmword [rbp - 0x69], xmm0
     │ │    0x18013068f      0f2845b7       movaps xmm0, xmmword [rbp - 0x49]
     │ │    0x180130693      b203           mov dl, 3
     │ │    0x180130695      48894c2420     mov qword [rsp + 0x20], rcx
     │ │    0x18013069a      488bc8         mov rcx, rax
     │ │    0x18013069d      660f7f4da7     movdqa xmmword [rbp - 0x59], xmm1
     │ │    0x1801306a2      660f7f45b7     movdqa xmmword [rbp - 0x49], xmm0
     │ │    0x1801306a7      e8d4a2feff     call 0x18011a980
     └────> 0x1801306ac      8b5320         mov edx, dword [rbx + 0x20]
       │    0x1801306af      488d053ab5..   lea rax, str.GetStereoBypassAllowed:__d ; 0x1803ebbf0 ; "GetStereoBypassAllowed: %d"
       │    0x1801306b6      b901000000     mov ecx, 1
       │    0x1801306bb      488945b7       mov qword [rbp - 0x49], rax
       │    0x1801306bf      48c745bf1a..   mov qword [rbp - 0x41], 0x1a ; 26
       │    0x1801306c7      48897597       mov qword [rbp - 0x69], rsi
       │    0x1801306cb      48c7459f3a..   mov qword [rbp - 0x61], 0x3a ; ':' ; 58
       │    0x1801306d3      4c8975a7       mov qword [rbp - 0x59], r14
       │    0x1801306d7      48c745af47..   mov qword [rbp - 0x51], 0x47 ; 'G' ; 71
       │    0x1801306df      e88c230200     call 0x180152a70
       │    0x1801306e4      0f2845b7       movaps xmm0, xmmword [rbp - 0x49]
       │    0x1801306e8      488d4db7       lea rcx, [rbp - 0x49]
       │    0x1801306ec      0f284d97       movaps xmm1, xmmword [rbp - 0x69]
       │    0x1801306f0      4c8d45a7       lea r8, [rbp - 0x59]
       │    0x1801306f4      48897c2430     mov qword [rsp + 0x30], rdi
       │    0x1801306f9      41b988040000   mov r9d, 0x488             ; 1160
       │    0x1801306ff      48894c2428     mov qword [rsp + 0x28], rcx
       │    0x180130704      488d4d97       lea rcx, [rbp - 0x69]
       │    0x180130708      660f7f45b7     movdqa xmmword [rbp - 0x49], xmm0
       │    0x18013070d      0f2845a7       movaps xmm0, xmmword [rbp - 0x59]
       │    0x180130711      48894c2420     mov qword [rsp + 0x20], rcx
       │    0x180130716      488bc8         mov rcx, rax
       │    0x180130719      660f7f4d97     movdqa xmmword [rbp - 0x69], xmm1
       │    0x18013071e      660f7f45a7     movdqa xmmword [rbp - 0x59], xmm0
       │    0x180130723      e8f862ffff     call 0x180126a20
       │    0x180130728      33db           xor ebx, ebx
       └──> 0x18013072a      488d4dc7       lea rcx, [rbp - 0x39]
            0x18013072e      e8ed7b0200     call 0x180158320
            0x180130733      8bc3           mov eax, ebx
            0x180130735      488b4d37       mov rcx, qword [rbp + 0x37]
            0x180130739      4833cc         xor rcx, rsp
            0x18013073c      e8df320400     call 0x180173a20
            0x180130741      4c8d9c24f0..   lea r11, [rsp + 0xf0]
            0x180130749      498b5b30       mov rbx, qword [r11 + 0x30]
            0x18013074d      498b7338       mov rsi, qword [r11 + 0x38]
            0x180130751      498be3         mov rsp, r11
            0x180130754      415e           pop r14
            0x180130756      5f             pop rdi
            0x180130757      5d             pop rbp
            0x180130758      c3             ret
            0x180130759      cc             int3
            0x18013075a      cc             int3
            0x18013075b      cc             int3
            0x18013075c      cc             int3
            0x18013075d      cc             int3
            0x18013075e      cc             int3
            0x18013075f      cc             int3
            0x180130760      4055           push rbp
            0x180130762      53             push rbx
            0x180130763      56             push rsi
            0x180130764      57             push rdi
            0x180130765      4154           push r12
            0x180130767      4156           push r14
            0x180130769      4157           push r15
            0x18013076b      488d6c24f0     lea rbp, [rsp - 0x10]
            0x180130770      4881ec1001..   sub rsp, 0x110
            0x180130777      488b05022c..   mov rax, qword [0x180413380] ; [0x180413380:8]=0x2b992ddfa232
            0x18013077e      4833c4         xor rax, rsp
            0x180130781      48894500       mov qword [rbp], rax
            0x180130785      458bf0         mov r14d, r8d
            0x180130788      488bf2         mov rsi, rdx
            0x18013078b      488bf9         mov rdi, rcx
            0x18013078e      4489442470     mov dword [rsp + 0x70], r8d
            0x180130793      0f57c0         xorps xmm0, xmm0
            0x180130796      0f114590       movups xmmword [rbp - 0x70], xmm0
            0x18013079a      0f1145a0       movups xmmword [rbp - 0x60], xmm0
            0x18013079e      0f1145b0       movups xmmword [rbp - 0x50], xmm0
            0x1801307a2      0f1145c0       movups xmmword [rbp - 0x40], xmm0
            0x1801307a6      0f1145d0       movups xmmword [rbp - 0x30], xmm0
            0x1801307aa      0f1145e0       movups xmmword [rbp - 0x20], xmm0
            0x1801307ae      0f1145f0       movups xmmword [rbp - 0x10], xmm0
            0x1801307b2      4c8d3d77b4..   lea r15, str.dolby::CDolbyAudioProcessingModule::HandleRealTimeParams ; 0x1803ebc30 ; "dolby::CDolbyAudioProcessingModule::HandleRealTimeParams"
