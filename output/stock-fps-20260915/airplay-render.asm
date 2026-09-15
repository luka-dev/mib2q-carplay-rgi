            ;-- method.dio::CScreenRender.render_unsigned_char_:
            ;-- dio::CScreenRender::render(unsigned char*):
            0x0008b5c0      f0452de9       push {r4, r5, r6, r7, r8, sl, lr}; RELOC 32 _ZN3dio13CScreenRender6renderEPh @ 0x0008b5c0 ; dio::CScreenRender::render(unsigned char*)
            0x0008b5c4      70419fe5       ldr r4, [0x0008b73c]        ; [0x8b73c:4]=0x2d768
            0x0008b5c8      70a19fe5       ldr sl, [0x0008b740]        ; [0x8b740:4]=0x10a4
            0x0008b5cc      0050a0e1       mov r5, r0
            0x0008b5d0      04408fe0       add r4, pc, r4
            0x0008b5d4      18c090e5       ldr ip, [r0, 0x18]
            0x0008b5d8      2cd04de2       sub sp, sp, 0x2c
            0x0008b5dc      0170a0e1       mov r7, r1
            0x0008b5e0      0a0094e7       ldr r0, [r4, sl]
            0x0008b5e4      18608de2       add r6, sp, 0x18
            0x0008b5e8      043086e2       add r3, r6, 4
            0x0008b5ec      1c8095e5       ldr r8, [r5, 0x1c]
            0x0008b5f0      0020a0e3       mov r2, 0
            0x0008b5f4      18208de5       str r2, [sp, 0x18]
            0x0008b5f8      001090e5       ldr r1, [r0]
            0x0008b5fc      042083e4       str r2, [r3], 4
            0x0008b600      043083e2       add r3, r3, 4
            0x0008b604      140051e3       cmp r1, 0x14
            0x0008b608      20c08de5       str ip, [sp, 0x20]
            0x0008b60c      002083e5       str r2, [r3]
            0x0008b610      24808de5       str r8, [sp, 0x24]
        ┌─< 0x0008b614      110000ca       bgt 0x8b660
        │   0x0008b618      010071e3       cmn r1, 1
       ┌──< 0x0008b61c      2100000a       beq 0x8b6a8
      ┌───> 0x0008b620      18e09de5       ldr lr, [sp, 0x18]
      ╎││   0x0008b624      1420a0e3       mov r2, 0x14
      ╎││   0x0008b628      14119fe5       ldr r1, [0x0008b744]        ; [0x8b744:4]=0xffff7f40
      ╎││   0x0008b62c      14319fe5       ldr r3, [0x0008b748]        ; [0x8b748:4]=0xffff7cf0
      ╎││   0x0008b630      04e08de5       str lr, [sp, 4]
      ╎││   0x0008b634      011084e0       add r1, r4, r1
      ╎││   0x0008b638      1ce09de5       ldr lr, [sp, 0x1c]
      ╎││   0x0008b63c      033084e0       add r3, r4, r3
      ╎││   0x0008b640      0a0094e7       ldr r0, [r4, sl]
      ╎││   0x0008b644      0cc08de5       str ip, [sp, 0xc]
      ╎││   0x0008b648      08e08de5       str lr, [sp, 8]
      ╎││   0x0008b64c      f8e09fe5       ldr lr, [0x0008b74c]        ; [0x8b74c:4]=0xffff804c
      ╎││   0x0008b650      10808de5       str r8, [sp, 0x10]
      ╎││   0x0008b654      0ee084e0       add lr, r4, lr
      ╎││   0x0008b658      00e08de5       str lr, [sp]
      ╎││   0x0008b65c      0f40feeb       bl rsym.LogPrintF
     ┌──└─> 0x0008b660      400095e5       ldr r0, [r5, 0x40]
     ╎╎│    0x0008b664      000050e3       cmp r0, 0
     ╎╎│┌─< 0x0008b668      1500000a       beq 0x8b6c4
     ╎╎││   0x0008b66c      0710a0e1       mov r1, r7
     ╎╎││   0x0008b670      e93ffeeb       bl rsym._ZN3dio13CScreenRender14CWindowBuffers21getWindowBufferHandleEPh
     ╎╎││   0x0008b674      001050e2       subs r1, r0, 0
    ┌─────< 0x0008b678      1100000a       beq 0x8b6c4
    │╎╎││   0x0008b67c      080095e5       ldr r0, [r5, 8]
    │╎╎││   0x0008b680      0630a0e1       mov r3, r6
    │╎╎││   0x0008b684      0120a0e3       mov r2, 1
    │╎╎││   0x0008b688      00c0a0e3       mov ip, 0
    │╎╎││   0x0008b68c      00c08de5       str ip, [sp]
    │╎╎││   0x0008b690      973efeeb       bl sym.imp.screen_post_window
    │╎╎││   0x0008b694      005050e2       subs r5, r0, 0
   ┌──────< 0x0008b698      0b00001a       bne 0x8b6cc
  ┌───────> 0x0008b69c      0500a0e1       mov r0, r5
  ╎││╎╎││   0x0008b6a0      2cd08de2       add sp, sp, 0x2c
  ╎││╎╎││   0x0008b6a4      f085bde8       pop {r4, r5, r6, r7, r8, sl, pc}
  ╎││╎╎└──> 0x0008b6a8      1410a0e3       mov r1, 0x14
  ╎││╎╎ │   0x0008b6ac      b93ffeeb       bl rsym.__LogCategory_Initialize
  ╎││╎╎ │   0x0008b6b0      000050e3       cmp r0, 0
  ╎││└────< 0x0008b6b4      e9ffff0a       beq 0x8b660
  ╎││ ╎ │   0x0008b6b8      20c09de5       ldr ip, [sp, 0x20]
  ╎││ ╎ │   0x0008b6bc      24809de5       ldr r8, [sp, 0x24]
  ╎││ └───< 0x0008b6c0      d6ffffea       b 0x8b620
  ╎│└───└─> 0x0008b6c4      ac550ee3       movw r5, 0xe5ac
  ╎│        0x0008b6c8      ff5f4fe3       movt r5, 0xffff
  ╎└──────> 0x0008b6cc      0a0094e7       ldr r0, [r4, sl]
  ╎         0x0008b6d0      003090e5       ldr r3, [r0]
  ╎         0x0008b6d4      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x0008b6d8      efffffca       bgt 0x8b69c
  ╎         0x0008b6dc      010073e3       cmn r3, 1
  ╎     ┌─< 0x0008b6e0      1000000a       beq 0x8b728
  ╎    ┌──> 0x0008b6e4      6537feeb       bl sym.imp.__get_errno_ptr
  ╎    ╎│   0x0008b6e8      000090e5       ldr r0, [r0]
  ╎    ╎│   0x0008b6ec      9d3ffeeb       bl sym.imp.strerror
  ╎    ╎│   0x0008b6f0      0ac094e7       ldr ip, [r4, sl]
  ╎    ╎│   0x0008b6f4      48109fe5       ldr r1, [0x0008b744]        ; [0x8b744:4]=0xffff7f40
  ╎    ╎│   0x0008b6f8      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎    ╎│   0x0008b6fc      4c309fe5       ldr r3, [0x0008b750]        ; [0x8b750:4]=0xffff7ac4
  ╎    ╎│   0x0008b700      44e09fe5       ldr lr, [0x0008b74c]        ; [0x8b74c:4]=0xffff804c
  ╎    ╎│   0x0008b704      011084e0       add r1, r4, r1
  ╎    ╎│   0x0008b708      033084e0       add r3, r4, r3
  ╎    ╎│   0x0008b70c      04508de5       str r5, [sp, 4]
  ╎    ╎│   0x0008b710      0e4084e0       add r4, r4, lr
  ╎    ╎│   0x0008b714      00408de5       str r4, [sp]
  ╎    ╎│   0x0008b718      08008de5       str r0, [sp, 8]
  ╎    ╎│   0x0008b71c      0c00a0e1       mov r0, ip
  ╎    ╎│   0x0008b720      de3ffeeb       bl rsym.LogPrintF
  ────────< 0x0008b724      dcffffea       b 0x8b69c
  ╎    ╎└─> 0x0008b728      5a10a0e3       mov r1, 0x5a                ; 'Z'
  ╎    ╎    0x0008b72c      993ffeeb       bl rsym.__LogCategory_Initialize
  ╎    ╎    0x0008b730      000050e3       cmp r0, 0
  └───────< 0x0008b734      d8ffff0a       beq 0x8b69c
       └──< 0x0008b738      e9ffffea       b 0x8b6e4
            0x0008b73c      68d70200       andeq sp, r2, r8, ror 14
            0x0008b740      a4100000       andeq r1, r0, r4, lsr 1
            0x0008b744      407fffff       invalid
            0x0008b748      f07cffff       invalid
            0x0008b74c      4c80ffff       invalid
            0x0008b750      c47affff       invalid
