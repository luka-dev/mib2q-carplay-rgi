            ;-- method.MMIVdecController.SetOmxParam_void_:
            ;-- MMIVdecController::SetOmxParam(void*):
            0x00018958      003091e5       ldr r3, [r1]                ; RELOC 32 _ZN17MMIVdecController11SetOmxParamEPv @ 0x00018958 ; MMIVdecController::SetOmxParam(void*)
            0x0001895c      30239fe5       ldr r2, [0x00018c94]        ; [0x18c94:4]=0x600000a
            0x00018960      f0412de9       push {r4, r5, r6, r7, r8, lr}
            0x00018964      020053e1       cmp r3, r2
            0x00018968      28539fe5       ldr r5, [0x00018c98]        ; [0x18c98:4]=0xc82c
            0x0001896c      30d04de2       sub sp, sp, 0x30
            0x00018970      0040a0e1       mov r4, r0
            0x00018974      05508fe0       add r5, pc, r5
        ┌─< 0x00018978      090000ca       bgt 0x189a4
        │   0x0001897c      012042e2       sub r2, r2, 1
        │   0x00018980      020053e1       cmp r3, r2
       ┌──< 0x00018984      bc0000aa       bge 0x18c7c
       ││   0x00018988      0c239fe5       ldr r2, [0x00018c9c]        ; [0x18c9c:4]=0x1000017
       ││   0x0001898c      020053e1       cmp r3, r2
      ┌───< 0x00018990      8700000a       beq 0x18bb4
      │││   0x00018994      04239fe5       ldr r2, [0x00018ca0]        ; [0x18ca0:4]=0x6000001
      │││   0x00018998      020053e1       cmp r3, r2
     ┌────< 0x0001899c      ae00001a       bne 0x18c5c
    ┌─────< 0x000189a0      0e0000ea       b 0x189e0
    ││││└─> 0x000189a4      f8229fe5       ldr r2, [0x00018ca4]        ; [0x18ca4:4]=0x600000d
    ││││    0x000189a8      020053e1       cmp r3, r2
    ││││┌─< 0x000189ac      5000000a       beq 0x18af4
   ┌──────< 0x000189b0      030000ca       bgt 0x189c4
   ││││││   0x000189b4      022042e2       sub r2, r2, 2
   ││││││   0x000189b8      020053e1       cmp r3, r2
  ┌───────< 0x000189bc      a600001a       bne 0x18c5c
  ────────< 0x000189c0      740000ea       b 0x18b98
  │└──────> 0x000189c4      dc229fe5       ldr r2, [0x00018ca8]        ; [0x18ca8:4]=0x600000e
  │ │││││   0x000189c8      020053e1       cmp r3, r2
  │┌──────< 0x000189cc      aa00000a       beq 0x18c7c
  │││││││   0x000189d0      d4229fe5       ldr r2, [0x00018cac]        ; [0x18cac:4]=0x7000001
  │││││││   0x000189d4      020053e1       cmp r3, r2
  ────────< 0x000189d8      9f00001a       bne 0x18c5c
  ────────< 0x000189dc      770000ea       b 0x18bc0
  ││└─────> 0x000189e0      047091e5       ldr r7, [r1, 4]
  ││ ││││   0x000189e4      081097e5       ldr r1, [r7, 8]
  ││ ││││   0x000189e8      000051e3       cmp r1, 0
  ││┌─────< 0x000189ec      0a00001a       bne 0x18a1c
  │││││││   0x000189f0      101097e5       ldr r1, [r7, 0x10]
  │││││││   0x000189f4      8db7ffeb       bl rsym._ZN17MMIVdecController13SetCodingTypeE20OMX_VIDEO_CODINGTYPE
  │││││││   0x000189f8      000050e3       cmp r0, 0
  ────────< 0x000189fc      a000000a       beq 0x18c84
  │││││││   0x00018a00      082097e5       ldr r2, [r7, 8]
  │││││││   0x00018a04      3410a0e3       mov r1, 0x34
  │││││││   0x00018a08      103097e5       ldr r3, [r7, 0x10]
  │││││││   0x00018a0c      1262a0e3       mov r6, 0x20000001
  │││││││   0x00018a10      914224e0       mla r4, r1, r2, r4
  │││││││   0x00018a14      5c3084e5       str r3, [r4, 0x5c]
  ────────< 0x00018a18      9a0000ea       b 0x18c88
  ││└─────> 0x00018a1c      010051e3       cmp r1, 1
  ││┌─────< 0x00018a20      9500001a       bne 0x18c7c
  │││││││   0x00018a24      143802e3       movw r3, 0x2814             ; '\x14('
  │││││││   0x00018a28      033090e7       ldr r3, [r0, r3]
  │││││││   0x00018a2c      000053e3       cmp r3, 0
  ────────< 0x00018a30      1f00000a       beq 0x18ab4
  │││││││   0x00018a34      0300a0e1       mov r0, r3
  │││││││   0x00018a38      142097e5       ldr r2, [r7, 0x14]
  │││││││   0x00018a3c      bbb6ffeb       bl rsym._ZN12Deinterlacer11SetColorFmtE16VdecDeintBufType20OMX_COLOR_FORMATTYPE
  │││││││   0x00018a40      120250e3       cmp r0, 0x20000001
  │││││││   0x00018a44      0060a0e1       mov r6, r0
  ────────< 0x00018a48      0a00001a       bne 0x18a78
  │││││││   0x00018a4c      083097e5       ldr r3, [r7, 8]
  │││││││   0x00018a50      34c0a0e3       mov ip, 0x34
  │││││││   0x00018a54      142097e5       ldr r2, [r7, 0x14]
  │││││││   0x00018a58      0400a0e1       mov r0, r4
  │││││││   0x00018a5c      4c129fe5       ldr r1, [0x00018cb0]        ; [0x18cb0:4]=0x7f000004
  │││││││   0x00018a60      9c4323e0       mla r3, ip, r3, r4
  │││││││   0x00018a64      602083e5       str r2, [r3, 0x60]
  │││││││   0x00018a68      bcb6ffeb       bl rsym._ZN17MMIVdecController14SetColorFormatE20OMX_COLOR_FORMATTYPE
  │││││││   0x00018a6c      001050e2       subs r1, r0, 0
  ────────< 0x00018a70      8400000a       beq 0x18c88
  ────────< 0x00018a74      120000ea       b 0x18ac4
  ────────> 0x00018a78      34329fe5       ldr r3, [0x00018cb4]        ; [0x18cb4:4]=0xffffd6b7
  │││││││   0x00018a7c      231700e3       movw r1, 0x723
  │││││││   0x00018a80      30029fe5       ldr r0, [0x00018cb8]        ; [0x18cb8:4]=0xffffaa0c
  │││││││   0x00018a84      0620a0e3       mov r2, 6
  │││││││   0x00018a88      033085e0       add r3, r5, r3
  │││││││   0x00018a8c      00308de5       str r3, [sp]
  │││││││   0x00018a90      14e097e5       ldr lr, [r7, 0x14]
  │││││││   0x00018a94      000085e0       add r0, r5, r0
  │││││││   0x00018a98      0330a0e3       mov r3, 3
  │││││││   0x00018a9c      00c0a0e3       mov ip, 0
  │││││││   0x00018aa0      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00018aa4      04e08de5       str lr, [sp, 4]
  │││││││   0x00018aa8      08c08de5       str ip, [sp, 8]
  │││││││   0x00018aac      2fb7ffeb       bl sym.imp.MM_OutputDebugMessage
  ────────< 0x00018ab0      740000ea       b 0x18c88
  ────────> 0x00018ab4      141097e5       ldr r1, [r7, 0x14]
  │││││││   0x00018ab8      a8b6ffeb       bl rsym._ZN17MMIVdecController14SetColorFormatE20OMX_COLOR_FORMATTYPE
  │││││││   0x00018abc      001050e2       subs r1, r0, 0
  ────────< 0x00018ac0      0400000a       beq 0x18ad8
  ────────> 0x00018ac4      020051e3       cmp r1, 2
  │││││││   0x00018ac8      15005113       cmpne r1, 0x15
  │││││││   0x00018acc      0400a001       moveq r0, r4
  ────────< 0x00018ad0      6b00001a       bne 0x18c84
  ────────< 0x00018ad4      5e0000ea       b 0x18c54
  ────────> 0x00018ad8      082097e5       ldr r2, [r7, 8]
  │││││││   0x00018adc      3410a0e3       mov r1, 0x34
  │││││││   0x00018ae0      143097e5       ldr r3, [r7, 0x14]
  │││││││   0x00018ae4      1262a0e3       mov r6, 0x20000001
  │││││││   0x00018ae8      914224e0       mla r4, r1, r2, r4
  │││││││   0x00018aec      603084e5       str r3, [r4, 0x60]
  ────────< 0x00018af0      640000ea       b 0x18c88
  ││││││└─> 0x00018af4      042091e5       ldr r2, [r1, 4]
  ││││││    0x00018af8      103090e5       ldr r3, [r0, 0x10]
  ││││││    0x00018afc      38c092e5       ldr ip, [r2, 0x38]
  ││││││    0x00018b00      010053e3       cmp r3, 1
  ││││││    0x00018b04      18c080e5       str ip, [r0, 0x18]
  ││││││┌─< 0x00018b08      5b00001a       bne 0x18c7c
  │││││││   0x00018b0c      1c6090e5       ldr r6, [r0, 0x1c]
  │││││││   0x00018b10      d03602e3       movw r3, 0x26d0
  │││││││   0x00018b14      02e0a0e3       mov lr, 2
  │││││││   0x00018b18      14208de2       add r2, sp, 0x14
  │││││││   0x00018b1c      030090e7       ldr r0, [r0, r3]
  │││││││   0x00018b20      0e10a0e1       mov r1, lr
  │││││││   0x00018b24      9b3d43e2       sub r3, r3, 0x26c0
  │││││││   0x00018b28      1cc08de5       str ip, [sp, 0x1c]
  │││││││   0x00018b2c      20608de5       str r6, [sp, 0x20]
  │││││││   0x00018b30      00c0a0e3       mov ip, 0
  │││││││   0x00018b34      14e08de5       str lr, [sp, 0x14]
  │││││││   0x00018b38      08c08de5       str ip, [sp, 8]
  │││││││   0x00018b3c      18e08de5       str lr, [sp, 0x18]
  │││││││   0x00018b40      00c08de5       str ip, [sp]
  │││││││   0x00018b44      04c08de5       str ip, [sp, 4]
  │││││││   0x00018b48      f6b6ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018b4c      006050e2       subs r6, r0, 0
  ────────< 0x00018b50      4900000a       beq 0x18c7c
  │││││││   0x00018b54      1cc09de5       ldr ip, [sp, 0x1c]
  │││││││   0x00018b58      541700e3       movw r1, 0x754
  │││││││   0x00018b5c      54019fe5       ldr r0, [0x00018cb8]        ; [0x18cb8:4]=0xffffaa0c
  │││││││   0x00018b60      0620a0e3       mov r2, 6
  │││││││   0x00018b64      0330a0e3       mov r3, 3
  │││││││   0x00018b68      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00018b6c      04c08de5       str ip, [sp, 4]
  │││││││   0x00018b70      000085e0       add r0, r5, r0
  │││││││   0x00018b74      20c09de5       ldr ip, [sp, 0x20]
  │││││││   0x00018b78      08c08de5       str ip, [sp, 8]
  │││││││   0x00018b7c      38c19fe5       ldr ip, [0x00018cbc]        ; [0x18cbc:4]=0xffffd6e3
  │││││││   0x00018b80      0c5085e0       add r5, r5, ip
  │││││││   0x00018b84      00508de5       str r5, [sp]
  │││││││   0x00018b88      f8b6ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018b8c      0400a0e1       mov r0, r4
  │││││││   0x00018b90      0610a0e1       mov r1, r6
  ────────< 0x00018b94      2e0000ea       b 0x18c54
  ────────> 0x00018b98      043091e5       ldr r3, [r1, 4]
  │││││││   0x00018b9c      0c3093e5       ldr r3, [r3, 0xc]
  │││││││   0x00018ba0      080053e3       cmp r3, 8
  │││││││   0x00018ba4      143080e5       str r3, [r0, 0x14]
  │││││││   0x00018ba8      10619f15       ldrne r6, [0x00018cc0]      ; [0x18cc0:4]=0x20000011
  ────────< 0x00018bac      3500001a       bne 0x18c88
  ────────< 0x00018bb0      310000ea       b 0x18c7c
  ││││└───> 0x00018bb4      041091e5       ldr r1, [r1, 4]
  ││││ ││   0x00018bb8      e3b6ffeb       bl rsym._ZN17MMIVdecController16SetComponentRoleEPv
  ││││┌───< 0x00018bbc      2c0000ea       b 0x18c74
  ────────> 0x00018bc0      047091e5       ldr r7, [r1, 4]
  │││││││   0x00018bc4      d03602e3       movw r3, 0x26d0
  │││││││   0x00018bc8      02c0a0e3       mov ip, 2
  │││││││   0x00018bcc      24208de2       add r2, sp, 0x24
  │││││││   0x00018bd0      030090e7       ldr r0, [r0, r3]
  │││││││   0x00018bd4      0c10a0e1       mov r1, ip
  │││││││   0x00018bd8      0ce097e5       ldr lr, [r7, 0xc]
  │││││││   0x00018bdc      0c30a0e3       mov r3, 0xc
  │││││││   0x00018be0      0060a0e3       mov r6, 0
  │││││││   0x00018be4      28c08de5       str ip, [sp, 0x28]
  │││││││   0x00018be8      00608de5       str r6, [sp]
  │││││││   0x00018bec      01005ee3       cmp lr, 1
  │││││││   0x00018bf0      00c0a013       movne ip, 0
  │││││││   0x00018bf4      01c0a003       moveq ip, 1
  │││││││   0x00018bf8      04608de5       str r6, [sp, 4]
  │││││││   0x00018bfc      2cc0cde5       strb ip, [sp, 0x2c]
  │││││││   0x00018c00      0fc0a0e3       mov ip, 0xf
  │││││││   0x00018c04      08608de5       str r6, [sp, 8]
  │││││││   0x00018c08      24c08de5       str ip, [sp, 0x24]
  │││││││   0x00018c0c      c5b6ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018c10      008050e2       subs r8, r0, 0
  ────────< 0x00018c14      1800000a       beq 0x18c7c
  │││││││   0x00018c18      a4109fe5       ldr r1, [0x00018cc4]        ; [0x18cc4:4]=0xffffd718
  │││││││   0x00018c1c      0620a0e3       mov r2, 6
  │││││││   0x00018c20      90009fe5       ldr r0, [0x00018cb8]        ; [0x18cb8:4]=0xffffaa0c
  │││││││   0x00018c24      0330a0e3       mov r3, 3
  │││││││   0x00018c28      011085e0       add r1, r5, r1
  │││││││   0x00018c2c      00108de5       str r1, [sp]
  │││││││   0x00018c30      0cc097e5       ldr ip, [r7, 0xc]
  │││││││   0x00018c34      000085e0       add r0, r5, r0
  │││││││   0x00018c38      771700e3       movw r1, 0x777
  │││││││   0x00018c3c      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00018c40      08808de5       str r8, [sp, 8]
  │││││││   0x00018c44      04c08de5       str ip, [sp, 4]
  │││││││   0x00018c48      c8b6ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018c4c      0400a0e1       mov r0, r4
  │││││││   0x00018c50      0810a0e1       mov r1, r8
  ────────> 0x00018c54      03b5ffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  ────────< 0x00018c58      050000ea       b 0x18c74
  └──└────> 0x00018c5c      64209fe5       ldr r2, [0x00018cc8]        ; [0x18cc8:4]=0x6f100000
   ││ │││   0x00018c60      020053e1       cmp r3, r2
   ││ │││   0x00018c64      f262a093       movls r6, 0x2000000f
   ││┌────< 0x00018c68      0600009a       bls 0x18c88
   ││││││   0x00018c6c      0400a0e1       mov r0, r4
   ││││││   0x00018c70      9bb5ffeb       bl rsym._ZN17MMIVdecController14SetOmxExtParamEP19MMI_OmxParamCmdType
  ────└───> 0x00018c74      0060a0e1       mov r6, r0
   │││┌───< 0x00018c78      020000ea       b 0x18c88
  ─└└──└└─> 0x00018c7c      1262a0e3       mov r6, 0x20000001
     ││ ┌─< 0x00018c80      000000ea       b 0x18c88
  ────────> 0x00018c84      4262a0e3       mov r6, 0x20000004
  ───└└─└─> 0x00018c88      0600a0e1       mov r0, r6
            0x00018c8c      30d08de2       add sp, sp, 0x30
            0x00018c90      f081bde8       pop {r4, r5, r6, r7, r8, pc}
            0x00018c94      0a000006       streq r0, [r0], -sl
            0x00018c98      2cc80000       andeq ip, r0, ip, lsr 16
            0x00018c9c      17000001       invalid
            0x00018ca0      01000006       streq r0, [r0], -r1
            0x00018ca4      0d000006       streq r0, [r0], -sp
            0x00018ca8      0e000006       streq r0, [r0], -lr
            0x00018cac      01000007       streq r0, [r0, -r1]
            0x00018cb0      0400007f       svcvc 4
            0x00018cb4      b7d6ffff       invalid
            0x00018cb8      0caaffff       invalid
            0x00018cbc      e3d6ffff       invalid
            0x00018cc0      11000020       andhs r0, r0, r1, lsl r0
            0x00018cc4      18d7ffff       invalid
            0x00018cc8      0000106f       svcvs 0x100000
