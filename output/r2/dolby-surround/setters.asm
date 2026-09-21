            0x180004120      53             push rbx
            0x180004121      56             push rsi
            0x180004122      55             push rbp
            0x180004123      4883ec20       sub rsp, 0x20
            0x180004127      4889ce         mov rsi, rcx
            0x18000412a      89d3           mov ebx, edx
            0x18000412c      488d6e38       lea rbp, [rsi + 0x38]
            0x180004130      4889e9         mov rcx, rbp
            0x180004133      e8e8c70000     call 0x180010920
            0x180004138      89d8           mov eax, ebx
            0x18000413a      ba60000000     mov edx, 0x60              ; '`' ; 96
            0x18000413f      c1f81f         sar eax, 0x1f
            0x180004142      660fefc9       pxor xmm1, xmm1
            0x180004146      f7d0           not eax
            0x180004148      23c3           and eax, ebx
            0x18000414a      83f860         cmp eax, 0x60              ; '`' ; 96
            0x18000414d      f30f1086ac..   movss xmm0, dword [rsi + 0x7ac]
            0x180004155      0f4cd0         cmovl edx, eax
            0x180004158      f30f2aca       cvtsi2ss xmm1, edx
            0x18000415c      f30f590ddc..   mulss xmm1, dword [0x1802e8740]
            0x180004164      0f2ec1         ucomiss xmm0, xmm1
        ┌─< 0x180004167      7a02           jp 0x18000416b
       ┌──< 0x180004169      7412           je 0x18000417d
       │└─> 0x18000416b      f30f118eac..   movss dword [rsi + 0x7ac], xmm1 ; [0x7ac:4]=-1 ; 1964
       │    0x180004173      c786b01300..   mov dword [rsi + 0x13b0], 1 ; [0x13b0:4]=-1
       └──> 0x18000417d      4889e9         mov rcx, rbp
            0x180004180      4883c420       add rsp, 0x20
            0x180004184      5d             pop rbp
            0x180004185      5e             pop rsi
            0x180004186      5b             pop rbx
        ┌─< 0x180004187      e9d4c70000     jmp 0x180010960
        │   0x18000418c      0f1f4000       nop dword [rax]
        │   0x180004190      56             push rsi
        │   0x180004191      57             push rdi
        │   0x180004192      4154           push r12
        │   0x180004194      4883ec20       sub rsp, 0x20
        │   0x180004198      4889cf         mov rdi, rcx
        │   0x18000419b      33f6           xor esi, esi
        │   0x18000419d      85d2           test edx, edx
        │   0x18000419f      400f95c6       setne sil
        │   0x1800041a3      4c8d6738       lea r12, [rdi + 0x38]
        │   0x1800041a7      4c89e1         mov rcx, r12
        │   0x1800041aa      e871c70000     call 0x180010920
        │   0x1800041af      3bb780070000   cmp esi, dword [rdi + 0x780]
       ┌──< 0x1800041b5      7410           je 0x1800041c7
       ││   0x1800041b7      89b780070000   mov dword [rdi + 0x780], esi ; [0x780:4]=-1 ; 1920
       ││   0x1800041bd      c787b01300..   mov dword [rdi + 0x13b0], 1 ; [0x13b0:4]=-1
       └──> 0x1800041c7      4c89e1         mov rcx, r12
        │   0x1800041ca      4883c420       add rsp, 0x20
        │   0x1800041ce      415c           pop r12
        │   0x1800041d0      5f             pop rdi
        │   0x1800041d1      5e             pop rsi
       ┌──< 0x1800041d2      e989c70000     jmp 0x180010960
       ││   0x1800041d7      660f1f8400..   nop word [rax + rax]
       ││   0x1800041e0      56             push rsi
            0x1801291f0      4883ec28       sub rsp, 0x28
            0x1801291f4      4c8bca         mov r9, rdx
            0x1801291f7      488b4108       mov rax, qword [rcx + 8]
            0x1801291fb      418b10         mov edx, dword [r8]
            0x1801291fe      498b09         mov rcx, qword [r9]
            0x180129201      ff15c9730600   call qword [0x1801905d0]   ; [0x1801905d0:8]=0x18018d190
            0x180129207      90             nop
            0x180129208      4883c428       add rsp, 0x28
            0x18012920c      c3             ret
            0x18012920d      cc             int3
            0x18012920e      cc             int3
            0x18012920f      cc             int3
            0x180129210      488d05c905..   lea rax, [0x1803e97e0]
            0x180129217      488902         mov qword [rdx], rax
            0x18012921a      488b4108       mov rax, qword [rcx + 8]
            0x18012921e      48894208       mov qword [rdx + 8], rax
            0x180129222      488bc2         mov rax, rdx
            0x180129225      c3             ret
            0x180129226      cc             int3
            0x180129227      cc             int3
            0x180129228      cc             int3
            0x180129229      cc             int3
            0x18012922a      cc             int3
            0x18012922b      cc             int3
            0x18012922c      cc             int3
            0x18012922d      cc             int3
            0x18012922e      cc             int3
            0x18012922f      cc             int3
            0x180129230      488d0571df..   lea rax, [0x1804171a8]
            0x180129237      c3             ret
            0x180129238      cc             int3
            0x180129239      cc             int3
            0x18012923a      cc             int3
            0x18012923b      cc             int3
            0x18012923c      cc             int3
            0x18012923d      cc             int3
            0x18012923e      cc             int3
            0x18012923f      cc             int3
            0x180129240      488d05c905..   lea rax, [0x1803e9810]
            0x180129247      488902         mov qword [rdx], rax
            0x18012924a      488b4108       mov rax, qword [rcx + 8]
            0x18012924e      48894208       mov qword [rdx + 8], rax
            0x180129252      488bc2         mov rax, rdx
            0x180129255      c3             ret
            0x180129256      cc             int3
            0x180129257      cc             int3
            0x180129258      cc             int3
            0x180129259      cc             int3
            0x18012925a      cc             int3
            0x18012925b      cc             int3
            0x18012925c      cc             int3
            0x18012925d      cc             int3
            0x18012925e      cc             int3
            0x18012925f      cc             int3
            0x180129260      4885c9         test rcx, rcx
        ┌─< 0x180129263      7413           je 0x180129278
        │   0x180129265      488b01         mov rax, qword [rcx]
        │   0x180129268      ba01000000     mov edx, 1
        │   0x18012926d      488b4010       mov rax, qword [rax + 0x10]
        │   0x180129271      48ff255873..   jmp qword [0x1801905d0]    ; [0x1801905d0:8]=0x18018d190
        └─> 0x180129278      c3             ret
            0x180129279      cc             int3
            0x18012927a      cc             int3
            0x18012927b      cc             int3
            0x18012927c      cc             int3
            0x18012927d      cc             int3
            0x18012927e      cc             int3
            0x18012927f      cc             int3
            0x180129280      488b4910       mov rcx, qword [rcx + 0x10]
        ┌─< 0x180129284      e9b7a70400     jmp 0x180173a40
            0x18011cd60      48895c2420     mov qword [rsp + 0x20], rbx
            0x18011cd65      55             push rbp
            0x18011cd66      56             push rsi
            0x18011cd67      57             push rdi
            0x18011cd68      4154           push r12
            0x18011cd6a      4155           push r13
            0x18011cd6c      4156           push r14
            0x18011cd6e      4157           push r15
            0x18011cd70      488dac2480..   lea rbp, [rsp - 0x2880]
            0x18011cd78      b880290000     mov eax, 0x2980
            0x18011cd7d      e8ae770500     call 0x180174530
            0x18011cd82      482be0         sub rsp, rax
            0x18011cd85      0f29b42470..   movaps xmmword [rsp + 0x2970], xmm6 ; [0x2970:16]=-1
            0x18011cd8d      0f29bc2460..   movaps xmmword [rsp + 0x2960], xmm7 ; [0x2960:16]=-1
            0x18011cd95      440f298424..   movaps xmmword [rsp + 0x2950], xmm8 ; [0x2950:16]=-1
            0x18011cd9e      440f298c24..   movaps xmmword [rsp + 0x2940], xmm9 ; [0x2940:16]=-1
            0x18011cda7      440f299424..   movaps xmmword [rsp + 0x2930], xmm10 ; [0x2930:16]=-1
            0x18011cdb0      488b05c965..   mov rax, qword [0x180413380] ; [0x180413380:8]=0x2b992ddfa232
            0x18011cdb7      4833c4         xor rax, rsp
            0x18011cdba      4889852028..   mov qword [rbp + 0x2820], rax
            0x18011cdc1      4d8be0         mov r12, r8
            0x18011cdc4      4c894500       mov qword [rbp], r8
            0x18011cdc8      4c8bea         mov r13, rdx
            0x18011cdcb      4c8bf9         mov r15, rcx
            0x18011cdce      48894dc0       mov qword [rbp - 0x40], rcx
            0x18011cdd2      4533f6         xor r14d, r14d
            0x18011cdd5      418bf6         mov esi, r14d
            0x18011cdd8      448975d0       mov dword [rbp - 0x30], r14d
            0x18011cddc      488d3dfd95..   lea rdi, str.dolby::dapvr::CDapVRModule::ApplyRunTimeParams ; 0x1803e63e0 ; "dolby::dapvr::CDapVRModule::ApplyRunTimeParams"
            0x18011cde3      4c8d05b692..   lea r8, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011cdea      403832         cmp byte [rdx], sil
        ┌─< 0x18011cded      0f84c5040000   je 0x18011d2b8
        │   0x18011cdf3      44897590       mov dword [rbp - 0x70], r14d
        │   0x18011cdf7      40387208       cmp byte [rdx + 8], sil
       ┌──< 0x18011cdfb      0f84fe030000   je 0x18011d1ff
       ││   0x18011ce01      4038b2c40c..   cmp byte [rdx + 0xcc4], sil
      ┌───< 0x18011ce08      0f859f000000   jne 0x18011cead
      │││   0x18011ce0e      488d054b95..   lea rax, str.channel_count_must_be_supplied_when_a_mix_matrix_is_provided ; 0x1803e6360 ; "channel_count must be supplied when a mix matrix is provided"
      │││   0x18011ce15      488985d006..   mov qword [rbp + 0x6d0], rax
      │││   0x18011ce1c      48c785d806..   mov qword [rbp + 0x6d8], 0x3c ; '<' ; 60
      │││   0x18011ce27      440f1085d0..   movups xmm8, xmmword [rbp + 0x6d0]
      │││   0x18011ce2f      4889bde006..   mov qword [rbp + 0x6e0], rdi
      │││   0x18011ce36      48c785e806..   mov qword [rbp + 0x6e8], 0x2e ; '.' ; 46
      │││   0x18011ce41      0f10bde006..   movups xmm7, xmmword [rbp + 0x6e0]
      │││   0x18011ce48      4c8985f006..   mov qword [rbp + 0x6f0], r8
            0x18011d4b0      001d00000044   add byte [0x1c411d4b6], bl ; [0x1c411d4b6:1]=255
            0x18011d4b6      0f1085b007..   movups xmm0, xmmword [rbp + 0x7b0]
            0x18011d4bd      4889bdc007..   mov qword [rbp + 0x7c0], rdi
            0x18011d4c4      48c785c807..   mov qword [rbp + 0x7c8], 0x2e ; '.' ; 46
            0x18011d4cf      0f10bdc007..   movups xmm7, xmmword [rbp + 0x7c0]
            0x18011d4d6      488d05c38b..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
            0x18011d4dd      488985d007..   mov qword [rbp + 0x7d0], rax
            0x18011d4e4      48c785d807..   mov qword [rbp + 0x7d8], 0x42 ; 'B' ; 66
            0x18011d4ef      0f10b5d007..   movups xmm6, xmmword [rbp + 0x7d0]
            0x18011d4f6      418b5708       mov edx, dword [r15 + 8]
            0x18011d4fa      b901000000     mov ecx, 1
            0x18011d4ff      e86c550300     call 0x180152a70
            0x18011d504      440f29442460   movaps xmmword [rsp + 0x60], xmm8
            0x18011d50a      0f297c2470     movaps xmmword [rsp + 0x70], xmm7
            0x18011d50f      0f297580       movaps xmmword [rbp - 0x80], xmm6
            0x18011d513      48895c2430     mov qword [rsp + 0x30], rbx
            0x18011d518      488d4c2460     lea rcx, [rsp + 0x60]
            0x18011d51d      48894c2428     mov qword [rsp + 0x28], rcx
            0x18011d522      488d4c2470     lea rcx, [rsp + 0x70]
            0x18011d527      48894c2420     mov qword [rsp + 0x20], rcx
            0x18011d52c      4c8d4580       lea r8, [rbp - 0x80]
            0x18011d530      488bc8         mov rcx, rax
            0x18011d533      e8c8920000     call 0x180126800
            0x18011d538      488d1dd1c2..   lea rbx, [0x1803e9810]
            0x18011d53f      4180bd1002..   cmp byte [r13 + 0x210], 0
        ┌─< 0x18011d547      0f849f000000   je 0x18011d5ec
        │   0x18011d54d      488d054c92..   lea rax, str.dap_vr_process_optimizer_is_not_currently_supported._Ignoring_request_to_call_dap_vr_process_optimizer_enable_set. ; 0x1803e67a0 ; "dap-vr process optimizer is not currently supported. Ignoring request to call dap_vr_process_optimizer_enable_set."
        │   0x18011d554      488985e007..   mov qword [rbp + 0x7e0], rax
        │   0x18011d55b      48c785e807..   mov qword [rbp + 0x7e8], 0x72 ; 'r' ; 114
        │   0x18011d566      440f1085e0..   movups xmm8, xmmword [rbp + 0x7e0]
        │   0x18011d56e      4889bdf007..   mov qword [rbp + 0x7f0], rdi
        │   0x18011d575      48c785f807..   mov qword [rbp + 0x7f8], 0x2e ; '.' ; 46
        │   0x18011d580      0f10bdf007..   movups xmm7, xmmword [rbp + 0x7f0]
        │   0x18011d587      488d05128b..   lea rax, str.C:_b0SampleCodecPack_v2DolbyAudioProcessing_srcDAPVRModule.cpp ; 0x1803e60a0 ; "C:\\b\\0\\SampleCodecPack_v2\\DolbyAudioProcessing\\src\\DAPVRModule.cpp"
        │   0x18011d58e      4889850008..   mov qword [rbp + 0x800], rax
