            ;-- method.dio::COMXVideoDecoder.decode_unsigned_char__unsigned_int__unsigned_long_long__bool_:
            ;-- dio::COMXVideoDecoder::decode(unsigned char*, unsigned int, unsigned long long, bool):
; dio::COMXVideoDecoder::decode(unsigned char*, unsigned int, unsigned
; long long, bool)
            0x000869e0      f04f2de9       push {r4, r5, r6, r7, r8, sb, sl, fp, lr}; RELOC 32 _ZN3dio16COMXVideoDecoder6decodeEPhjyb @ 0x000869e0
            0x000869e4      0cd04de2       sub sp, sp, 0xc
            0x000869e8      00a051e2       subs sl, r1, 0
            0x000869ec      a4419fe5       ldr r4, [0x00086b98]        ; [0x86b98:4]=0x32340
            0x000869f0      3850dde5       ldrb r5, [sp, 0x38]
            0x000869f4      cf550e03       movweq r5, 0xe5cf
            0x000869f8      04408fe0       add r4, pc, r4
            0x000869fc      0070a0e1       mov r7, r0
            0x00086a00      0290a0e1       mov sb, r2
            0x00086a04      0a60a001       moveq r6, sl
            0x00086a08      ff5f4f03       movteq r5, 0xffff
        ┌─< 0x00086a0c      3400000a       beq 0x86ae4
        │   0x00086a10      000052e3       cmp r2, 0
        │   0x00086a14      cf550e03       movweq r5, 0xe5cf
        │   0x00086a18      0260a001       moveq r6, r2
        │   0x00086a1c      ff5f4f03       movteq r5, 0xffff
       ┌──< 0x00086a20      2f00000a       beq 0x86ae4
       ││   0x00086a24      386090e5       ldr r6, [r0, 0x38]
       ││   0x00086a28      000056e3       cmp r6, 0
       ││   0x00086a2c      ac550e03       movweq r5, 0xe5ac
       ││   0x00086a30      ff5f4f03       movteq r5, 0xffff
      ┌───< 0x00086a34      2a00000a       beq 0x86ae4
      │││   0x00086a38      5c319fe5       ldr r3, [0x00086b9c]        ; [0x86b9c:4]=0x1158
      │││   0x00086a3c      3000a0e3       mov r0, 0x30                ; '0'
      │││   0x00086a40      0180a0e3       mov r8, 1
      │││   0x00086a44      031094e7       ldr r1, [r4, r3]
      │││   0x00086a48      b44cfeeb       bl sym.imp.operator_new_unsigned_int__std::nothrow_t_const_
      │││   0x00086a4c      4c319fe5       ldr r3, [0x00086ba0]        ; [0x86ba0:4]=0x11b4
      │││   0x00086a50      0020a0e3       mov r2, 0
      │││   0x00086a54      0c0bdded       vldr d16, [sp, 0x30]
      │││   0x00086a58      033094e7       ldr r3, [r4, r3]
      │││   0x00086a5c      083083e2       add r3, r3, 8
      │││   0x00086a60      0060a0e1       mov r6, r0
      │││   0x00086a64      24b080e2       add fp, r0, 0x24
      │││   0x00086a68      2050c0e5       strb r5, [r0, 0x20]
      │││   0x00086a6c      060bc0ed       vstr d16, [r0, 0x18]
      │││   0x00086a70      0c2086e5       str r2, [r6, 0xc]
      │││   0x00086a74      0b00a0e1       mov r0, fp
      │││   0x00086a78      080186e8       stm r6, {r3, r8}
      │││   0x00086a7c      082086e5       str r2, [r6, 8]
      │││   0x00086a80      107086e5       str r7, [r6, 0x10]
      │││   0x00086a84      a351feeb       bl rsym._ZN3dio5CBlobC1Ev
      │││   0x00086a88      000056e3       cmp r6, 0
      │││   0x00086a8c      b8550e03       movweq r5, 0xe5b8
      │││   0x00086a90      ff5f4f03       movteq r5, 0xffff
     ┌────< 0x00086a94      1200000a       beq 0x86ae4
     ││││   0x00086a98      0b00a0e1       mov r0, fp
     ││││   0x00086a9c      0a10a0e1       mov r1, sl
     ││││   0x00086aa0      0920a0e1       mov r2, sb
     ││││   0x00086aa4      884cfeeb       bl rsym._ZN3dio5CBlob7setDataEPKhj
     ││││   0x00086aa8      380097e5       ldr r0, [r7, 0x38]
     ││││   0x00086aac      0610a0e1       mov r1, r6
     ││││   0x00086ab0      6420a0e3       mov r2, 0x64                ; 'd'
     ││││   0x00086ab4      e152feeb       bl rsym._ZN3dio14CCommandThread14enqueueCommandEPNS_8CCommandEi
     ││││   0x00086ab8      410f50e3       cmp r0, 0x104
     ││││   0x00086abc      0050a0e1       mov r5, r0
     ││││   0x00086ac0      be550e03       movweq r5, 0xe5be
     ││││   0x00086ac4      ff5f4f03       movteq r5, 0xffff
    ┌─────< 0x00086ac8      0500000a       beq 0x86ae4
    │││││   0x00086acc      000055e3       cmp r5, 0
   ┌──────< 0x00086ad0      0300001a       bne 0x86ae4
  ┌───────> 0x00086ad4      055088e1       orr r5, r8, r5
  ╎││││││   0x00086ad8      7500efe6       uxtb r0, r5
  ╎││││││   0x00086adc      0cd08de2       add sp, sp, 0xc
  ╎││││││   0x00086ae0      f08fbde8       pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}
  ╎└└└└└└─> 0x00086ae4      b8709fe5       ldr r7, [0x00086ba4]        ; [0x86ba4:4]=0x1230
  ╎         0x00086ae8      070094e7       ldr r0, [r4, r7]
  ╎         0x00086aec      003090e5       ldr r3, [r0]
  ╎         0x00086af0      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ╎     ┌─< 0x00086af4      0c0000ca       bgt 0x86b2c
  ╎     │   0x00086af8      010073e3       cmn r3, 1
  ╎    ┌──< 0x00086afc      2000000a       beq 0x86b84
  ╎   ┌───> 0x00086b00      a0109fe5       ldr r1, [0x00086ba8]        ; [0x86ba8:4]=0xffff6840
  ╎   ╎││   0x00086b04      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎   ╎││   0x00086b08      9c309fe5       ldr r3, [0x00086bac]        ; [0x86bac:4]=0xffff7048
  ╎   ╎││   0x00086b0c      9cc09fe5       ldr ip, [0x00086bb0]        ; [0x86bb0:4]=0xffff6758
  ╎   ╎││   0x00086b10      011084e0       add r1, r4, r1
  ╎   ╎││   0x00086b14      070094e7       ldr r0, [r4, r7]
  ╎   ╎││   0x00086b18      033084e0       add r3, r4, r3
  ╎   ╎││   0x00086b1c      04508de5       str r5, [sp, 4]
  ╎   ╎││   0x00086b20      0c4084e0       add r4, r4, ip
  ╎   ╎││   0x00086b24      00408de5       str r4, [sp]
  ╎   ╎││   0x00086b28      dc52feeb       bl rsym.LogPrintF
  ╎  ┌──└─> 0x00086b2c      000056e3       cmp r6, 0
  ╎  ╎╎│┌─< 0x00086b30      0b00000a       beq 0x86b64
  ╎  ╎╎││   0x00086b34      003096e5       ldr r3, [r6]
  ╎  ╎╎││   0x00086b38      0600a0e1       mov r0, r6
  ╎  ╎╎││   0x00086b3c      018075e2       rsbs r8, r5, 1
  ╎  ╎╎││   0x00086b40      0080a033       movlo r8, 0
  ╎  ╎╎││   0x00086b44      043093e5       ldr r3, [r3, 4]
  ╎  ╎╎││   0x00086b48      33ff2fe1       blx r3
  ╎  ╎╎││   0x00086b4c      be350ee3       movw r3, 0xe5be
  ╎  ╎╎││   0x00086b50      ff3f4fe3       movt r3, 0xffff
  ╎  ╎╎││   0x00086b54      030055e1       cmp r5, r3
  ╎  ╎╎││   0x00086b58      0050a013       movne r5, 0
  ╎  ╎╎││   0x00086b5c      0150a003       moveq r5, 1
  ────────< 0x00086b60      dbffffea       b 0x86ad4
  ╎  ╎╎│└─> 0x00086b64      be350ee3       movw r3, 0xe5be
  ╎  ╎╎│    0x00086b68      018075e2       rsbs r8, r5, 1
  ╎  ╎╎│    0x00086b6c      0080a033       movlo r8, 0
  ╎  ╎╎│    0x00086b70      ff3f4fe3       movt r3, 0xffff
  ╎  ╎╎│    0x00086b74      030055e1       cmp r5, r3
  ╎  ╎╎│    0x00086b78      0050a013       movne r5, 0
  ╎  ╎╎│    0x00086b7c      0150a003       moveq r5, 1
  └───────< 0x00086b80      d3ffffea       b 0x86ad4
     ╎╎└──> 0x00086b84      5a10a0e3       mov r1, 0x5a                ; 'Z'
     ╎╎     0x00086b88      8252feeb       bl rsym.__LogCategory_Initialize
     ╎╎     0x00086b8c      000050e3       cmp r0, 0
     └────< 0x00086b90      e5ffff0a       beq 0x86b2c
      └───< 0x00086b94      d9ffffea       b 0x86b00
            0x00086b98      40230300       andeq r2, r3, r0, asr 6
            0x00086b9c      58110000       andeq r1, r0, r8, asr r1
            0x00086ba0      b4110000       strheq r1, [r0], -r4
            0x00086ba4      30120000       andeq r1, r0, r0, lsr r2
            0x00086ba8      4068ffff       invalid
            0x00086bac      4870ffff       invalid
            0x00086bb0      5867ffff       invalid
