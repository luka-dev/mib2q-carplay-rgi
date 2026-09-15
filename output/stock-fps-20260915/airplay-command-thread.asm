            ;-- method.dio::CCommandThread.thread__:
            ;-- dio::CCommandThread::thread():
            0x00072698      f0472de9       push {r4, r5, r6, r7, r8, sb, sl, lr}; RELOC 32 _ZN3dio14CCommandThread6threadEv @ 0x00072698 ; dio::CCommandThread::thread()
            0x0007269c      d4619fe5       ldr r6, [0x00072878]        ; [0x72878:4]=0x46690
            0x000726a0      d4719fe5       ldr r7, [0x0007287c]        ; [0x7287c:4]=0x11a4
            0x000726a4      0040a0e1       mov r4, r0
            0x000726a8      06608fe0       add r6, pc, r6
            0x000726ac      38d04de2       sub sp, sp, 0x38
            0x000726b0      14808de2       add r8, sp, 0x14
            0x000726b4      0020a0e3       mov r2, 0
            0x000726b8      070096e7       ldr r0, [r6, r7]
            0x000726bc      043088e2       add r3, r8, 4
            0x000726c0      14208de5       str r2, [sp, 0x14]
            0x000726c4      042083e4       str r2, [r3], 4
            0x000726c8      001090e5       ldr r1, [r0]
            0x000726cc      042083e4       str r2, [r3], 4
            0x000726d0      320051e3       cmp r1, 0x32                ; '2'
            0x000726d4      042083e4       str r2, [r3], 4
            0x000726d8      042083e4       str r2, [r3], 4
            0x000726dc      042083e4       str r2, [r3], 4
            0x000726e0      042083e4       str r2, [r3], 4
            0x000726e4      002083e5       str r2, [r3]
        ┌─< 0x000726e8      0b0000ca       bgt 0x7271c
        │   0x000726ec      010071e3       cmn r1, 1
       ┌──< 0x000726f0      5800000a       beq 0x72858
      ┌───> 0x000726f4      84119fe5       ldr r1, [0x00072880]        ; [0x72880:4]=0xffff43b0
      ╎││   0x000726f8      3220a0e3       mov r2, 0x32                ; '2'
      ╎││   0x000726fc      80319fe5       ldr r3, [0x00072884]        ; [0x72884:4]=0xffff44fc
      ╎││   0x00072700      80c19fe5       ldr ip, [0x00072888]        ; [0x72888:4]=0xffff4414
      ╎││   0x00072704      011086e0       add r1, r6, r1
      ╎││   0x00072708      070096e7       ldr r0, [r6, r7]
      ╎││   0x0007270c      033086e0       add r3, r6, r3
      ╎││   0x00072710      0cc086e0       add ip, r6, ip
      ╎││   0x00072714      00c08de5       str ip, [sp]
      ╎││   0x00072718      e0a3feeb       bl rsym.LogPrintF
     ┌──└─> 0x0007271c      5830d4e5       ldrb r3, [r4, 0x58]
     ╎╎│    0x00072720      000053e3       cmp r3, 0
     ╎╎│    0x00072724      0050a013       movne r5, 0
     ╎╎│┌─< 0x00072728      2300001a       bne 0x727bc
     ╎╎││   0x0007272c      03a0a0e1       mov sl, r3
     ╎╎││   0x00072730      34908de2       add sb, sp, 0x34
    ┌─────> 0x00072734      540094e5       ldr r0, [r4, 0x54]
    ╎╎╎││   0x00072738      34a08de5       str sl, [sp, 0x34]
    ╎╎╎││   0x0007273c      000050e3       cmp r0, 0
   ┌──────< 0x00072740      4900000a       beq 0x7286c
   │╎╎╎││   0x00072744      0910a0e1       mov r1, sb
   │╎╎╎││   0x00072748      5efeffeb       bl 0x720c8
   │╎╎╎││   0x0007274c      005050e2       subs r5, r0, 0
  ┌───────< 0x00072750      0f00001a       bne 0x72794
  ││╎╎╎││   0x00072754      34009de5       ldr r0, [sp, 0x34]
  ││╎╎╎││   0x00072758      000050e3       cmp r0, 0
  ││╎╎╎││   0x0007275c      5830d405       ldrbeq r3, [r4, 0x58]
  ────────< 0x00072760      4100000a       beq 0x7286c
  ││╎╎╎││   0x00072764      003090e5       ldr r3, [r0]
  ││╎╎╎││   0x00072768      083093e5       ldr r3, [r3, 8]
  ││╎╎╎││   0x0007276c      33ff2fe1       blx r3
  ││╎╎╎││   0x00072770      005050e2       subs r5, r0, 0
  ────────< 0x00072774      2b00001a       bne 0x72828
  ────────> 0x00072778      34009de5       ldr r0, [sp, 0x34]
  ││╎╎╎││   0x0007277c      083090e5       ldr r3, [r0, 8]
  ││╎╎╎││   0x00072780      000053e3       cmp r3, 0
  ────────< 0x00072784      0300000a       beq 0x72798
  ││╎╎╎││   0x00072788      0300a0e1       mov r0, r3
  ││╎╎╎││   0x0007278c      0010a0e3       mov r1, 0
  ││╎╎╎││   0x00072790      f0a2feeb       bl rsym._ZN3dio10CSemaphore7releaseEb
  └───────> 0x00072794      34009de5       ldr r0, [sp, 0x34]
  ────────> 0x00072798      000050e3       cmp r0, 0
  ┌───────< 0x0007279c      0200000a       beq 0x727ac
  ││╎╎╎││   0x000727a0      003090e5       ldr r3, [r0]
  ││╎╎╎││   0x000727a4      043093e5       ldr r3, [r3, 4]
  ││╎╎╎││   0x000727a8      33ff2fe1       blx r3
  └───────> 0x000727ac      5830d4e5       ldrb r3, [r4, 0x58]
  ┌───────> 0x000727b0      000053e3       cmp r3, 0
  ╎│╎╎╎││   0x000727b4      34a08de5       str sl, [sp, 0x34]
  ╎│└─────< 0x000727b8      ddffff0a       beq 0x72734
  ╎│ ╎╎│└─> 0x000727bc      0400a0e1       mov r0, r4
  ╎│ ╎╎│    0x000727c0      ae99feeb       bl rsym._ZN3dio14CCommandThread17emptyCommandQueueEv
  ╎│ ╎╎│    0x000727c4      040094e5       ldr r0, [r4, 4]
  ╎│ ╎╎│    0x000727c8      0810a0e1       mov r1, r8
  ╎│ ╎╎│    0x000727cc      2020a0e3       mov r2, 0x20
  ╎│ ╎╎│    0x000727d0      38a2feeb       bl sym.imp.pthread_getname_np
  ╎│ ╎╎│    0x000727d4      070096e7       ldr r0, [r6, r7]
  ╎│ ╎╎│    0x000727d8      003090e5       ldr r3, [r0]
  ╎│ ╎╎│    0x000727dc      320053e3       cmp r3, 0x32                ; '2'
  ╎│ ╎╎│┌─< 0x000727e0      0d0000ca       bgt 0x7281c
  ╎│ ╎╎││   0x000727e4      010073e3       cmn r3, 1
  ╎│┌─────< 0x000727e8      1500000a       beq 0x72844
  ────────> 0x000727ec      8c109fe5       ldr r1, [0x00072880]        ; [0x72880:4]=0xffff43b0
  ╎││╎╎││   0x000727f0      3220a0e3       mov r2, 0x32                ; '2'
  ╎││╎╎││   0x000727f4      90309fe5       ldr r3, [0x0007288c]        ; [0x7288c:4]=0xffff4528
  ╎││╎╎││   0x000727f8      88c09fe5       ldr ip, [0x00072888]        ; [0x72888:4]=0xffff4414
  ╎││╎╎││   0x000727fc      011086e0       add r1, r6, r1
  ╎││╎╎││   0x00072800      070096e7       ldr r0, [r6, r7]
  ╎││╎╎││   0x00072804      033086e0       add r3, r6, r3
  ╎││╎╎││   0x00072808      04808de5       str r8, [sp, 4]
  ╎││╎╎││   0x0007280c      0c6086e0       add r6, r6, ip
  ╎││╎╎││   0x00072810      08508de5       str r5, [sp, 8]
  ╎││╎╎││   0x00072814      00608de5       str r6, [sp]
  ╎││╎╎││   0x00072818      a0a3feeb       bl rsym.LogPrintF
  ──────└─> 0x0007281c      0000a0e3       mov r0, 0
  ╎││╎╎│    0x00072820      38d08de2       add sp, sp, 0x38
  ╎││╎╎│    0x00072824      f087bde8       pop {r4, r5, r6, r7, r8, sb, sl, pc}
  ────────> 0x00072828      34309de5       ldr r3, [sp, 0x34]
  ╎││╎╎│    0x0007282c      0510a0e1       mov r1, r5
  ╎││╎╎│    0x00072830      0300a0e1       mov r0, r3
  ╎││╎╎│    0x00072834      003093e5       ldr r3, [r3]
  ╎││╎╎│    0x00072838      0c3093e5       ldr r3, [r3, 0xc]
  ╎││╎╎│    0x0007283c      33ff2fe1       blx r3
  ────────< 0x00072840      ccffffea       b 0x72778
  ╎│└─────> 0x00072844      3210a0e3       mov r1, 0x32                ; '2'
  ╎│ ╎╎│    0x00072848      52a3feeb       bl rsym.__LogCategory_Initialize
  ╎│ ╎╎│    0x0007284c      000050e3       cmp r0, 0
  ────────< 0x00072850      f1ffff0a       beq 0x7281c
  ────────< 0x00072854      e4ffffea       b 0x727ec
  ╎│ ╎╎└──> 0x00072858      3210a0e3       mov r1, 0x32                ; '2'
  ╎│ ╎╎     0x0007285c      4da3feeb       bl rsym.__LogCategory_Initialize
  ╎│ ╎╎     0x00072860      000050e3       cmp r0, 0
  ╎│ └────< 0x00072864      acffff0a       beq 0x7271c
  ╎│  └───< 0x00072868      a1ffffea       b 0x726f4
  ─└──────> 0x0007286c      ac550ee3       movw r5, 0xe5ac
  ╎         0x00072870      ff5f4fe3       movt r5, 0xffff
  └───────< 0x00072874      cdffffea       b 0x727b0
            0x00072878      90660400       muleq r4, r0, r6
            0x0007287c      a4110000       andeq r1, r0, r4, lsr 3
            0x00072880      b043ffff       invalid
            0x00072884      fc44ffff       invalid
            0x00072888      1444ffff       invalid
            0x0007288c      2845ffff       invalid
