            ;-- method.dio::CScreenRender::CRenderFrameCommand.execute__:
            ;-- dio::CScreenRender::CRenderFrameCommand::execute():
            0x0008b754      70402de9       push {r4, r5, r6, lr}       ; RELOC 32 _ZN3dio13CScreenRender19CRenderFrameCommand7executeEv @ 0x0008b754 ; dio::CScreenRender::CRenderFrameCommand::execute()
            0x0008b758      0060a0e1       mov r6, r0
            0x0008b75c      100090e5       ldr r0, [r0, 0x10]
            0x0008b760      10d04de2       sub sp, sp, 0x10
            0x0008b764      c4409fe5       ldr r4, [0x0008b830]        ; [0x8b830:4]=0x2d5c8
            0x0008b768      000050e3       cmp r0, 0
            0x0008b76c      ac550e03       movweq r5, 0xe5ac
            0x0008b770      04408fe0       add r4, pc, r4
            0x0008b774      ff5f4f03       movteq r5, 0xffff
        ┌─< 0x0008b778      0e00000a       beq 0x8b7b8
        │   0x0008b77c      141096e5       ldr r1, [r6, 0x14]
        │   0x0008b780      ad3afeeb       bl rsym._ZN3dio13CScreenRender6renderEPh
        │   0x0008b784      005050e2       subs r5, r0, 0
       ┌──< 0x0008b788      0a00001a       bne 0x8b7b8
       ││   0x0008b78c      183096e5       ldr r3, [r6, 0x18]
       ││   0x0008b790      000053e3       cmp r3, 0
      ┌───< 0x0008b794      0400000a       beq 0x8b7ac
      │││   0x0008b798      201096e5       ldr r1, [r6, 0x20]
      │││   0x0008b79c      1c0096e5       ldr r0, [r6, 0x1c]
      │││   0x0008b7a0      33ff2fe1       blx r3
      │││   0x0008b7a4      005050e2       subs r5, r0, 0
     ┌────< 0x0008b7a8      0200001a       bne 0x8b7b8
  ┌┌┌─└───> 0x0008b7ac      0500a0e1       mov r0, r5
  ╎╎╎│ ││   0x0008b7b0      10d08de2       add sp, sp, 0x10
  ╎╎╎│ ││   0x0008b7b4      7080bde8       pop {r4, r5, r6, pc}
  ╎╎╎└─└└─> 0x0008b7b8      74609fe5       ldr r6, [0x0008b834]        ; [0x8b834:4]=0x10a4
  ╎╎╎       0x0008b7bc      060094e7       ldr r0, [r4, r6]
  ╎╎╎       0x0008b7c0      003090e5       ldr r3, [r0]
  ╎╎╎       0x0008b7c4      5a0053e3       cmp r3, 0x5a                ; 'Z'
  └───────< 0x0008b7c8      f7ffffca       bgt 0x8b7ac
   ╎╎       0x0008b7cc      010073e3       cmn r3, 1
   ╎╎   ┌─< 0x0008b7d0      1100000a       beq 0x8b81c
   ╎╎  ┌──> 0x0008b7d4      2937feeb       bl sym.imp.__get_errno_ptr
   ╎╎  ╎│   0x0008b7d8      000090e5       ldr r0, [r0]
   ╎╎  ╎│   0x0008b7dc      613ffeeb       bl sym.imp.strerror
   ╎╎  ╎│   0x0008b7e0      06c094e7       ldr ip, [r4, r6]
   ╎╎  ╎│   0x0008b7e4      4c109fe5       ldr r1, [0x0008b838]        ; [0x8b838:4]=0xffff79b8
   ╎╎  ╎│   0x0008b7e8      5a20a0e3       mov r2, 0x5a                ; 'Z'
   ╎╎  ╎│   0x0008b7ec      48309fe5       ldr r3, [0x0008b83c]        ; [0x8b83c:4]=0xffff7d1c
   ╎╎  ╎│   0x0008b7f0      48e09fe5       ldr lr, [0x0008b840]        ; [0x8b840:4]=0xffff7810
   ╎╎  ╎│   0x0008b7f4      011084e0       add r1, r4, r1
   ╎╎  ╎│   0x0008b7f8      033084e0       add r3, r4, r3
   ╎╎  ╎│   0x0008b7fc      04508de5       str r5, [sp, 4]
   ╎╎  ╎│   0x0008b800      0e4084e0       add r4, r4, lr
   ╎╎  ╎│   0x0008b804      08508de5       str r5, [sp, 8]
   ╎╎  ╎│   0x0008b808      00408de5       str r4, [sp]
   ╎╎  ╎│   0x0008b80c      0c008de5       str r0, [sp, 0xc]
   ╎╎  ╎│   0x0008b810      0c00a0e1       mov r0, ip
   ╎╎  ╎│   0x0008b814      a13ffeeb       bl rsym.LogPrintF
   └──────< 0x0008b818      e3ffffea       b 0x8b7ac
    ╎  ╎└─> 0x0008b81c      5a10a0e3       mov r1, 0x5a                ; 'Z'
    ╎  ╎    0x0008b820      5c3ffeeb       bl rsym.__LogCategory_Initialize
    ╎  ╎    0x0008b824      000050e3       cmp r0, 0
    └─────< 0x0008b828      dfffff0a       beq 0x8b7ac
       └──< 0x0008b82c      e8ffffea       b 0x8b7d4
            0x0008b830      c8d50200       andeq sp, r2, r8, asr 11
            0x0008b834      a4100000       andeq r1, r0, r4, lsr 1
            0x0008b838      b879ffff       invalid
            0x0008b83c      1c7dffff       invalid
            0x0008b840      1078ffff       invalid
