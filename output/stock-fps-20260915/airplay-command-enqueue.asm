            ;-- method.dio::CCommandThread.enqueueCommand_dio::CCommand__int_:
            ;-- dio::CCommandThread::enqueueCommand(dio::CCommand*, int):
            0x00072214      f0402de9       push {r4, r5, r6, r7, lr}   ; RELOC 32 _ZN3dio14CCommandThread14enqueueCommandEPNS_8CCommandEi @ 0x00072214 ; dio::CCommandThread::enqueueCommand(dio::CCommand*, int)
            0x00072218      24419fe5       ldr r4, [0x00072344]        ; [0x72344:4]=0x46b14
            0x0007221c      24619fe5       ldr r6, [0x00072348]        ; [0x72348:4]=0x11a4
            0x00072220      0050a0e1       mov r5, r0
            0x00072224      04408fe0       add r4, pc, r4
            0x00072228      1cd04de2       sub sp, sp, 0x1c
            0x0007222c      0270a0e1       mov r7, r2
            0x00072230      060094e7       ldr r0, [r4, r6]
            0x00072234      14108de5       str r1, [sp, 0x14]
            0x00072238      003090e5       ldr r3, [r0]
            0x0007223c      140053e3       cmp r3, 0x14
        ┌─< 0x00072240      110000ca       bgt 0x7228c
        │   0x00072244      010073e3       cmn r3, 1
       ┌──< 0x00072248      1c00000a       beq 0x722c0
      ┌───> 0x0007224c      14c09de5       ldr ip, [sp, 0x14]
      ╎││   0x00072250      1420a0e3       mov r2, 0x14
      ╎││   0x00072254      f0109fe5       ldr r1, [0x0007234c]        ; [0x7234c:4]=0xffff4370
      ╎││   0x00072258      00005ce3       cmp ip, 0
      ╎││   0x0007225c      ec309fe5       ldr r3, [0x00072350]        ; [0x72350:4]=0xffff4448
      ╎││   0x00072260      060094e7       ldr r0, [r4, r6]
      ╎││   0x00072264      011084e0       add r1, r4, r1
      ╎││   0x00072268      04e09c15       ldrne lr, [ip, 4]
      ╎││   0x0007226c      033084e0       add r3, r4, r3
      ╎││   0x00072270      04c08de5       str ip, [sp, 4]
      ╎││   0x00072274      00e0e003       mvneq lr, 0
      ╎││   0x00072278      d4c09fe5       ldr ip, [0x00072354]        ; [0x72354:4]=0xffff4580
      ╎││   0x0007227c      08e08de5       str lr, [sp, 8]
      ╎││   0x00072280      0cc084e0       add ip, r4, ip
      ╎││   0x00072284      00c08de5       str ip, [sp]
      ╎││   0x00072288      04a5feeb       bl rsym.LogPrintF
     ┌──└─> 0x0007228c      540095e5       ldr r0, [r5, 0x54]
     ╎╎│    0x00072290      000050e3       cmp r0, 0
     ╎╎│    0x00072294      ac550e03       movweq r5, 0xe5ac
     ╎╎│    0x00072298      ff5f4f03       movteq r5, 0xffff
     ╎╎│┌─< 0x0007229c      0c00000a       beq 0x722d4
     ╎╎││   0x000722a0      0720a0e1       mov r2, r7
     ╎╎││   0x000722a4      14108de2       add r1, sp, 0x14
     ╎╎││   0x000722a8      4e9bfeeb       bl rsym._ZN3dio6CQueueIPNS_8CCommandEE7enqueueERKS2_i
     ╎╎││   0x000722ac      005050e2       subs r5, r0, 0
    ┌─────< 0x000722b0      0700001a       bne 0x722d4
  ┌┌──────> 0x000722b4      0500a0e1       mov r0, r5
  ╎╎│╎╎││   0x000722b8      1cd08de2       add sp, sp, 0x1c
  ╎╎│╎╎││   0x000722bc      f080bde8       pop {r4, r5, r6, r7, pc}
  ╎╎│╎╎└──> 0x000722c0      1410a0e3       mov r1, 0x14
  ╎╎│╎╎ │   0x000722c4      b3a4feeb       bl rsym.__LogCategory_Initialize
  ╎╎│╎╎ │   0x000722c8      000050e3       cmp r0, 0
  ╎╎│└────< 0x000722cc      eeffff0a       beq 0x7228c
  ╎╎│ └───< 0x000722d0      ddffffea       b 0x7224c
  ╎╎└───└─> 0x000722d4      060094e7       ldr r0, [r4, r6]
  ╎╎        0x000722d8      003090e5       ldr r3, [r0]
  ╎╎        0x000722dc      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x000722e0      f3ffffca       bgt 0x722b4
  ╎╎        0x000722e4      010073e3       cmn r3, 1
  ╎╎    ┌─< 0x000722e8      1000000a       beq 0x72330
  ╎╎   ┌──> 0x000722ec      639cfeeb       bl sym.imp.__get_errno_ptr
  ╎╎   ╎│   0x000722f0      000090e5       ldr r0, [r0]
  ╎╎   ╎│   0x000722f4      9ba4feeb       bl sym.imp.strerror
  ╎╎   ╎│   0x000722f8      06c094e7       ldr ip, [r4, r6]
  ╎╎   ╎│   0x000722fc      48109fe5       ldr r1, [0x0007234c]        ; [0x7234c:4]=0xffff4370
  ╎╎   ╎│   0x00072300      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎╎   ╎│   0x00072304      4c309fe5       ldr r3, [0x00072358]        ; [0x72358:4]=0xffff441c
  ╎╎   ╎│   0x00072308      44e09fe5       ldr lr, [0x00072354]        ; [0x72354:4]=0xffff4580
  ╎╎   ╎│   0x0007230c      011084e0       add r1, r4, r1
  ╎╎   ╎│   0x00072310      033084e0       add r3, r4, r3
  ╎╎   ╎│   0x00072314      04508de5       str r5, [sp, 4]
  ╎╎   ╎│   0x00072318      0e4084e0       add r4, r4, lr
  ╎╎   ╎│   0x0007231c      00408de5       str r4, [sp]
  ╎╎   ╎│   0x00072320      08008de5       str r0, [sp, 8]
  ╎╎   ╎│   0x00072324      0c00a0e1       mov r0, ip
  ╎╎   ╎│   0x00072328      dca4feeb       bl rsym.LogPrintF
  └───────< 0x0007232c      e0ffffea       b 0x722b4
   ╎   ╎└─> 0x00072330      5a10a0e3       mov r1, 0x5a                ; 'Z'
   ╎   ╎    0x00072334      97a4feeb       bl rsym.__LogCategory_Initialize
   ╎   ╎    0x00072338      000050e3       cmp r0, 0
   └──────< 0x0007233c      dcffff0a       beq 0x722b4
       └──< 0x00072340      e9ffffea       b 0x722ec
            0x00072344      146b0400       andeq r6, r4, r4, lsl fp
            0x00072348      a4110000       andeq r1, r0, r4, lsr 3
            0x0007234c      7043ffff       invalid
            0x00072350      4844ffff       invalid
            0x00072354      8045ffff       invalid
            0x00072358      1c44ffff       invalid
