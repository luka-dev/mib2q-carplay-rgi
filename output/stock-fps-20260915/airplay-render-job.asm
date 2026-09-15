            ;-- method.dio::CScreenRender.onJob_renderFrame_unsigned_char__int____void__void___void__void_:
            ;-- dio::CScreenRender::onJob_renderFrame(unsigned char*, int (*)(void*, void*), void*, void*):
; dio::CScreenRender::onJob_renderFrame(unsigned char*, int (*)(void*,
; void*), void*, void*)
            0x0008af08      f0452de9       push {r4, r5, r6, r7, r8, sl, lr}; RELOC 32 _ZN3dio13CScreenRender17onJob_renderFrameEPhPFiPvS2_ES2_S2_ @ 0x0008af08
            0x0008af0c      008051e2       subs r8, r1, 0
            0x0008af10      50419fe5       ldr r4, [0x0008b068]        ; [0x8b068:4]=0x2de18
            0x0008af14      0cd04de2       sub sp, sp, 0xc
            0x0008af18      0050a0e1       mov r5, r0
            0x0008af1c      0270a0e1       mov r7, r2
            0x0008af20      04408fe0       add r4, pc, r4
            0x0008af24      0360a0e1       mov r6, r3
        ┌─< 0x0008af28      4700000a       beq 0x8b04c
        │   0x0008af2c      38a19fe5       ldr sl, [0x0008b06c]        ; [0x8b06c:4]=0x10a4
        │   0x0008af30      0a0094e7       ldr r0, [r4, sl]
        │   0x0008af34      003090e5       ldr r3, [r0]
        │   0x0008af38      140053e3       cmp r3, 0x14
       ┌──< 0x0008af3c      160000da       ble 0x8af9c
     ┌┌───> 0x0008af40      3c3095e5       ldr r3, [r5, 0x3c]
     ╎╎││   0x0008af44      000053e3       cmp r3, 0
    ┌─────< 0x0008af48      2600000a       beq 0x8afe8
    │╎╎││   0x0008af4c      28c09de5       ldr ip, [sp, 0x28]
    │╎╎││   0x0008af50      0810a0e1       mov r1, r8
    │╎╎││   0x0008af54      0720a0e1       mov r2, r7
    │╎╎││   0x0008af58      0630a0e1       mov r3, r6
    │╎╎││   0x0008af5c      0500a0e1       mov r0, r5
    │╎╎││   0x0008af60      00c08de5       str ip, [sp]
    │╎╎││   0x0008af64      163efeeb       bl rsym._ZN3dio13CScreenRender19CRenderFrameCommand6createEPS0_PhPFiPvS4_ES4_S4_
    │╎╎││   0x0008af68      001050e2       subs r1, r0, 0
   ┌──────< 0x0008af6c      1d00000a       beq 0x8afe8
   ││╎╎││   0x0008af70      3c0095e5       ldr r0, [r5, 0x3c]
   ││╎╎││   0x0008af74      6420a0e3       mov r2, 0x64                ; 'd'
   ││╎╎││   0x0008af78      b041feeb       bl rsym._ZN3dio14CCommandThread14enqueueCommandEPNS_8CCommandEi
   ││╎╎││   0x0008af7c      000050e3       cmp r0, 0
   ││╎╎││   0x0008af80      410f5013       cmpne r0, 0x104
   ││╎╎││   0x0008af84      0050a003       moveq r5, 0
   ││╎╎││   0x0008af88      0150a013       movne r5, 1
  ┌───────< 0x0008af8c      3200001a       bne 0x8b05c
  ────────> 0x0008af90      0500a0e1       mov r0, r5
  │││╎╎││   0x0008af94      0cd08de2       add sp, sp, 0xc
  │││╎╎││   0x0008af98      f085bde8       pop {r4, r5, r6, r7, r8, sl, pc}
  │││╎╎└──> 0x0008af9c      010073e3       cmn r3, 1
  │││╎╎┌──< 0x0008afa0      0b00000a       beq 0x8afd4
  ────────> 0x0008afa4      c4109fe5       ldr r1, [0x0008b070]        ; [0x8b070:4]=0xffff8054
  │││╎╎││   0x0008afa8      1420a0e3       mov r2, 0x14
  │││╎╎││   0x0008afac      c0309fe5       ldr r3, [0x0008b074]        ; [0x8b074:4]=0xffff7ca4
  │││╎╎││   0x0008afb0      c0c09fe5       ldr ip, [0x0008b078]        ; [0x8b078:4]=0xffff7830
  │││╎╎││   0x0008afb4      011084e0       add r1, r4, r1
  │││╎╎││   0x0008afb8      0a0094e7       ldr r0, [r4, sl]
  │││╎╎││   0x0008afbc      033084e0       add r3, r4, r3
  │││╎╎││   0x0008afc0      0cc084e0       add ip, r4, ip
  │││╎╎││   0x0008afc4      04808de5       str r8, [sp, 4]
  │││╎╎││   0x0008afc8      00c08de5       str ip, [sp]
  │││╎╎││   0x0008afcc      b341feeb       bl rsym.LogPrintF
  │││└────< 0x0008afd0      daffffea       b 0x8af40
  │││ ╎└──> 0x0008afd4      1410a0e3       mov r1, 0x14
  │││ ╎ │   0x0008afd8      6e41feeb       bl rsym.__LogCategory_Initialize
  │││ ╎ │   0x0008afdc      000050e3       cmp r0, 0
  │││ └───< 0x0008afe0      d6ffff0a       beq 0x8af40
  ────────< 0x0008afe4      eeffffea       b 0x8afa4
  │└└─────> 0x0008afe8      ac550ee3       movw r5, 0xe5ac
  │     │   0x0008afec      ff5f4fe3       movt r5, 0xffff
  │   ┌┌──> 0x0008aff0      0a0094e7       ldr r0, [r4, sl]
  │   ╎╎│   0x0008aff4      003090e5       ldr r3, [r0]
  │   ╎╎│   0x0008aff8      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x0008affc      e3ffffca       bgt 0x8af90
  │   ╎╎│   0x0008b000      010073e3       cmn r3, 1
  │  ┌────< 0x0008b004      0b00000a       beq 0x8b038
  │ ┌─────> 0x0008b008      60109fe5       ldr r1, [0x0008b070]        ; [0x8b070:4]=0xffff8054
  │ ╎│╎╎│   0x0008b00c      5a20a0e3       mov r2, 0x5a                ; 'Z'
  │ ╎│╎╎│   0x0008b010      64309fe5       ldr r3, [0x0008b07c]        ; [0x8b07c:4]=0xffff7aa4
  │ ╎│╎╎│   0x0008b014      5cc09fe5       ldr ip, [0x0008b078]        ; [0x8b078:4]=0xffff7830
  │ ╎│╎╎│   0x0008b018      011084e0       add r1, r4, r1
  │ ╎│╎╎│   0x0008b01c      0a0094e7       ldr r0, [r4, sl]
  │ ╎│╎╎│   0x0008b020      033084e0       add r3, r4, r3
  │ ╎│╎╎│   0x0008b024      04508de5       str r5, [sp, 4]
  │ ╎│╎╎│   0x0008b028      0c4084e0       add r4, r4, ip
  │ ╎│╎╎│   0x0008b02c      00408de5       str r4, [sp]
  │ ╎│╎╎│   0x0008b030      9a41feeb       bl rsym.LogPrintF
  ────────< 0x0008b034      d5ffffea       b 0x8af90
  │ ╎└────> 0x0008b038      5a10a0e3       mov r1, 0x5a                ; 'Z'
  │ ╎ ╎╎│   0x0008b03c      5541feeb       bl rsym.__LogCategory_Initialize
  │ ╎ ╎╎│   0x0008b040      000050e3       cmp r0, 0
  ────────< 0x0008b044      d1ffff0a       beq 0x8af90
  │ └─────< 0x0008b048      eeffffea       b 0x8b008
  │   ╎╎└─> 0x0008b04c      cf550ee3       movw r5, 0xe5cf
  │   ╎╎    0x0008b050      14a09fe5       ldr sl, [0x0008b06c]        ; [0x8b06c:4]=0x10a4
  │   ╎╎    0x0008b054      ff5f4fe3       movt r5, 0xffff
  │   └───< 0x0008b058      e4ffffea       b 0x8aff0
  └───────> 0x0008b05c      b7550ee3       movw r5, 0xe5b7
       ╎    0x0008b060      ff5f4fe3       movt r5, 0xffff
       └──< 0x0008b064      e1ffffea       b 0x8aff0
            0x0008b068      18de0200       andeq sp, r2, r8, lsl lr
            0x0008b06c      a4100000       andeq r1, r0, r4, lsr 1
            0x0008b070      5480ffff       invalid
            0x0008b074      a47cffff       invalid
            0x0008b078      3078ffff       invalid
            0x0008b07c      a47affff       invalid
