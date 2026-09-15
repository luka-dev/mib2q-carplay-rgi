            ;-- method.MMIVdecController.SetOmxExtParam_MMI_OmxParamCmdType_:
            ;-- MMIVdecController::SetOmxExtParam(MMI_OmxParamCmdType*):
            0x000175f0      003091e5       ldr r3, [r1]                ; RELOC 32 _ZN17MMIVdecController14SetOmxExtParamEP19MMI_OmxParamCmdType @ 0x000175f0 ; MMIVdecController::SetOmxExtParam(MMI_OmxParamCmdType*)
            0x000175f4      f04f2de9       push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
            0x000175f8      99df4de2       sub sp, sp, 0x264
            0x000175fc      5c5f9fe5       ldr r5, [0x00018560]        ; [0x18560:4]=0xdb90
            0x00017600      0040a0e1       mov r4, r0
            0x00017604      18308de5       str r3, [sp, 0x18]
            0x00017608      0170a0e1       mov r7, r1
            0x0001760c      503f9fe5       ldr r3, [0x00018564]        ; [0x18564:4]=0x3b4
            0x00017610      05508fe0       add r5, pc, r5
            0x00017614      c02090e5       ldr r2, [r0, 0xc0]
            0x00017618      033095e7       ldr r3, [r5, r3]
            0x0001761c      000052e3       cmp r2, 0
            0x00017620      003093e5       ldr r3, [r3]
            0x00017624      5c328de5       str r3, [sp, 0x25c]
        ┌─< 0x00017628      0d00000a       beq 0x17664
        │   0x0001762c      18a09de5       ldr sl, [sp, 0x18]
        │   0x00017630      cb1700e3       movw r1, 0x7cb
        │   0x00017634      2c0f9fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
        │   0x00017638      0620a0e3       mov r2, 6
        │   0x0001763c      28cf9fe5       ldr ip, [0x0001856c]        ; [0x1856c:4]=0xffffd168
        │   0x00017640      0330a0e3       mov r3, 3
        │   0x00017644      000085e0       add r0, r5, r0
        │   0x00017648      04a08de5       str sl, [sp, 4]
        │   0x0001764c      0cc085e0       add ip, r5, ip
        │   0x00017650      5262a0e3       mov r6, 0x20000005
        │   0x00017654      00c08de5       str ip, [sp]
        │   0x00017658      00c0a0e3       mov ip, 0
        │   0x0001765c      0cc08de5       str ip, [sp, 0xc]
       ┌──< 0x00017660      380400ea       b 0x18748
       │└─> 0x00017664      18b09de5       ldr fp, [sp, 0x18]
       │    0x00017668      5f334be2       sub r3, fp, 0x7c000001
       │    0x0001766c      313643e2       sub r3, r3, 0x3100000
       │    0x00017670      110053e3       cmp r3, 0x11
       │    0x00017674      03f18f90       addls pc, pc, r3, lsl 2     ; [0x1767c:4]=0xea000010
       │┌─< 0x00017678      990300ea       b 0x184e4
      ┌───< 0x0001767c      100000ea       b 0x176c4
     ┌────< 0x00017680      d20100ea       b 0x17dd0
    ┌─────< 0x00017684      860000ea       b 0x178a4
   ┌──────< 0x00017688      9d0000ea       b 0x17904
  ┌───────< 0x0001768c      980000ea       b 0x178f4
  ────────< 0x00017690      a20000ea       b 0x17920
  ────────< 0x00017694      c30000ea       b 0x179a8
  ────────< 0x00017698      040100ea       b 0x17ab0
  ────────< 0x0001769c      590100ea       b 0x17c08
  ────────< 0x000176a0      e40000ea       b 0x17a38
  ────────< 0x000176a4      d90100ea       b 0x17e10
  ────────< 0x000176a8      8d0300ea       b 0x184e4
  ────────< 0x000176ac      230100ea       b 0x17b40
  ────────< 0x000176b0      2f0200ea       b 0x17f74
  ────────< 0x000176b4      840200ea       b 0x180cc
  ────────< 0x000176b8      c90200ea       b 0x181e4
  ────────< 0x000176bc      880300ea       b 0x184e4
  ────────< 0x000176c0      010300ea       b 0x182cc
  ││││└───> 0x000176c4      04a091e5       ldr sl, [r1, 4]
  ││││ ││   0x000176c8      0c909ae5       ldr sb, [sl, 0xc]
  ││││ ││   0x000176cc      000059e3       cmp sb, 0
  ││││┌───< 0x000176d0      7604000a       beq 0x188b0
  │││││││   0x000176d4      ff9e89e2       add sb, sb, 0xff0
  │││││││   0x000176d8      886e9fe5       ldr r6, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x000176dc      0f9089e2       add sb, sb, 0xf
  │││││││   0x000176e0      dd2700e3       movw r2, 0x7dd
  │││││││   0x000176e4      ff9ec9e3       bic sb, sb, 0xff0
  │││││││   0x000176e8      066085e0       add r6, r5, r6
  │││││││   0x000176ec      0f90c9e3       bic sb, sb, 0xf
  │││││││   0x000176f0      0610a0e1       mov r1, r6
  │││││││   0x000176f4      0900a0e1       mov r0, sb
  │││││││   0x000176f8      7bbaffeb       bl sym.imp.MM_malloc
  │││││││   0x000176fc      008050e2       subs r8, r0, 0
  ────────< 0x00017700      0a00001a       bne 0x17730
  │││││││   0x00017704      64ce9fe5       ldr ip, [0x00018570]        ; [0x18570:4]=0xffffd188
  │││││││   0x00017708      0600a0e1       mov r0, r6
  │││││││   0x0001770c      e11700e3       movw r1, 0x7e1
  │││││││   0x00017710      0620a0e3       mov r2, 6
  │││││││   0x00017714      0cc085e0       add ip, r5, ip
  │││││││   0x00017718      0330a0e3       mov r3, 3
  │││││││   0x0001771c      0c808de5       str r8, [sp, 0xc]
  │││││││   0x00017720      6262a0e3       mov r6, 0x20000006
  │││││││   0x00017724      00c08de5       str ip, [sp]
  │││││││   0x00017728      04808de5       str r8, [sp, 4]
  ────────< 0x0001772c      aa0200ea       b 0x181dc
  ────────> 0x00017730      0c309ae5       ldr r3, [sl, 0xc]
  │││││││   0x00017734      106088e2       add r6, r8, 0x10
  │││││││   0x00017738      0600a0e1       mov r0, r6
  │││││││   0x0001773c      10108ae2       add r1, sl, 0x10
  │││││││   0x00017740      0320a0e1       mov r2, r3
  │││││││   0x00017744      50318de5       str r3, [sp, 0x150]
  │││││││   0x00017748      59bcffeb       bl sym.imp.std_memmove
  │││││││   0x0001774c      103094e5       ldr r3, [r4, 0x10]
  │││││││   0x00017750      010053e3       cmp r3, 1
  ────────< 0x00017754      2800001a       bne 0x177fc
  │││││││   0x00017758      9c2094e5       ldr r2, [r4, 0x9c]
  │││││││   0x0001775c      000052e3       cmp r2, 0
  ────────< 0x00017760      2500000a       beq 0x177fc
  │││││││   0x00017764      242702e3       movw r2, 0x2724             ; '$\''
  │││││││   0x00017768      02b094e7       ldr fp, [r4, r2]
  │││││││   0x0001776c      042042e2       sub r2, r2, 4
  │││││││   0x00017770      022094e7       ldr r2, [r4, r2]
  │││││││   0x00017774      02201be0       ands r2, fp, r2
  │││││││   0x00017778      1c208de5       str r2, [sp, 0x1c]
  ────────< 0x0001777c      1e00001a       bne 0x177fc
  │││││││   0x00017780      20308de2       add r3, sp, 0x20
  │││││││   0x00017784      109049e2       sub sb, sb, 0x10
  │││││││   0x00017788      0300a0e1       mov r0, r3
  │││││││   0x0001778c      14308de5       str r3, [sp, 0x14]
  │││││││   0x00017790      38bcffeb       bl rsym._ZN13MMIVdecBufferC1Ev
  │││││││   0x00017794      34908de5       str sb, [sp, 0x34]
  │││││││   0x00017798      0c209ae5       ldr r2, [sl, 0xc]
  │││││││   0x0001779c      26ce8de2       add ip, sp, 0x260
  │││││││   0x000177a0      9c0094e5       ldr r0, [r4, 0x9c]
  │││││││   0x000177a4      00a0a0e3       mov sl, 0
  │││││││   0x000177a8      14309de5       ldr r3, [sp, 0x14]
  │││││││   0x000177ac      00b0a0e3       mov fp, 0
  │││││││   0x000177b0      2c208de5       str r2, [sp, 0x2c]
  │││││││   0x000177b4      b82d9fe5       ldr r2, [0x00018574]        ; [0x18574:4]=0xfffffdd8
  │││││││   0x000177b8      0310a0e1       mov r1, r3
  │││││││   0x000177bc      24608de5       str r6, [sp, 0x24]
  │││││││   0x000177c0      48608de5       str r6, [sp, 0x48]
  │││││││   0x000177c4      f2a08ce1       strd sl, fp, [ip, r2]
  │││││││   0x000177c8      8e2f82e2       add r2, r2, 0x238
  │││││││   0x000177cc      40208de5       str r2, [sp, 0x40]
  │││││││   0x000177d0      1c209de5       ldr r2, [sp, 0x1c]
  │││││││   0x000177d4      30208de5       str r2, [sp, 0x30]
  │││││││   0x000177d8      4c20cde5       strb r2, [sp, 0x4c]
  │││││││   0x000177dc      28208de5       str r2, [sp, 0x28]
  │││││││   0x000177e0      e0baffeb       bl rsym._ZN14FrameAssembler25ConvertNalLengthBitStreamEmP13MMIVdecBuffer
  │││││││   0x000177e4      14309de5       ldr r3, [sp, 0x14]
  │││││││   0x000177e8      0300a0e1       mov r0, r3
  │││││││   0x000177ec      2c309de5       ldr r3, [sp, 0x2c]
  │││││││   0x000177f0      50318de5       str r3, [sp, 0x150]
  │││││││   0x000177f4      7abbffeb       bl rsym._ZN13MMIVdecBufferD1Ev
  ────────< 0x000177f8      170000ea       b 0x1785c
  ────────> 0x000177fc      100053e3       cmp r3, 0x10
  │││││││   0x00017800      40005313       cmpne r3, 0x40              ; '@'
  ────────< 0x00017804      0300000a       beq 0x17818
  │││││││   0x00017808      800053e3       cmp r3, 0x80
  ────────< 0x0001780c      0100000a       beq 0x17818
  │││││││   0x00017810      010c53e3       cmp r3, 0x100
  ────────< 0x00017814      1000001a       bne 0x1785c
  ────────> 0x00017818      563f8de2       add r3, sp, 0x158
  │││││││   0x0001781c      270c84e2       add r0, r4, 0x2700
  │││││││   0x00017820      013043e2       sub r3, r3, 1
  │││││││   0x00017824      280080e2       add r0, r0, 0x28
  │││││││   0x00017828      08308de5       str r3, [sp, 8]
  │││││││   0x0001782c      0610a0e1       mov r1, r6
  │││││││   0x00017830      523f8de2       add r3, sp, 0x148
  │││││││   0x00017834      00308de5       str r3, [sp]
  │││││││   0x00017838      153e8de2       add r3, sp, 0x150
  │││││││   0x0001783c      04308de5       str r3, [sp, 4]
  │││││││   0x00017840      533f8de2       add r3, sp, 0x14c
  │││││││   0x00017844      0c209ae5       ldr r2, [sl, 0xc]
  │││││││   0x00017848      00c0a0e3       mov ip, 0
  │││││││   0x0001784c      57c1cde5       strb ip, [sp, 0x157]
  │││││││   0x00017850      4cc18de5       str ip, [sp, 0x14c]
  │││││││   0x00017854      48c18de5       str ip, [sp, 0x148]
  │││││││   0x00017858      dfbbffeb       bl rsym._ZN8MP4Utils14ParseVolHeaderEPhmRmS1_S1_Rb
  ────────> 0x0001785c      0730a0e3       mov r3, 7
  │││││││   0x00017860      003088e5       str r3, [r8]
  │││││││   0x00017864      50319de5       ldr r3, [sp, 0x150]
  │││││││   0x00017868      0810a0e1       mov r1, r8
  │││││││   0x0001786c      086088e5       str r6, [r8, 8]
  │││││││   0x00017870      0400a0e1       mov r0, r4
  │││││││   0x00017874      0120a0e3       mov r2, 1
  │││││││   0x00017878      0c3088e5       str r3, [r8, 0xc]
  │││││││   0x0001787c      0230a0e3       mov r3, 2
  │││││││   0x00017880      043088e5       str r3, [r8, 4]
  │││││││   0x00017884      22bcffeb       bl rsym._ZN17MMIVdecController11InitDecoderER43QVDPROPERTY_VIDEO_DECODER_SEQUENCE_HEADER_Sb
  │││││││   0x00017888      d81c9fe5       ldr r1, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x0001788c      082800e3       movw r2, 0x808
  │││││││   0x00017890      011085e0       add r1, r5, r1
  │││││││   0x00017894      0060a0e1       mov r6, r0
  │││││││   0x00017898      0800a0e1       mov r0, r8
  │││││││   0x0001789c      83bbffeb       bl sym.imp.MM_free
  ────────< 0x000178a0      150300ea       b 0x184fc
  ││└─────> 0x000178a4      041091e5       ldr r1, [r1, 4]
  ││ ││││   0x000178a8      4020a0e3       mov r2, 0x40                ; '@'
  ││ ││││   0x000178ac      102080e5       str r2, [r0, 0x10]
  ││ ││││   0x000178b0      0c3091e5       ldr r3, [r1, 0xc]
  ││ ││││   0x000178b4      101091e5       ldr r1, [r1, 0x10]
  ││ ││││   0x000178b8      020053e3       cmp r3, 2
  ││ ││││   0x000178bc      143080e5       str r3, [r0, 0x14]
  ││ ││││   0x000178c0      1e308302       addeq r3, r3, 0x1e
  ││ ││││   0x000178c4      181080e5       str r1, [r0, 0x18]
  ││┌─────< 0x000178c8      1c00000a       beq 0x17940
  │││││││   0x000178cc      040053e3       cmp r3, 4
  │││││││   0x000178d0      10208005       streq r2, [r0, 0x10]
  ────────< 0x000178d4      1a00000a       beq 0x17944
  │││││││   0x000178d8      080053e3       cmp r3, 8
  │││││││   0x000178dc      78308302       addeq r3, r3, 0x78
  ────────< 0x000178e0      1600000a       beq 0x17940
  │││││││   0x000178e4      100053e3       cmp r3, 0x10
  │││││││   0x000178e8      f0308302       addeq r3, r3, 0xf0
  ────────< 0x000178ec      ef03001a       bne 0x188b0
  ────────< 0x000178f0      120000ea       b 0x17940
  └───────> 0x000178f4      043091e5       ldr r3, [r1, 4]
   ││││││   0x000178f8      1262a0e3       mov r6, 0x20000001
   ││││││   0x000178fc      0c3093e5       ldr r3, [r3, 0xc]
  ┌───────< 0x00017900      040000ea       b 0x17918
  │└──────> 0x00017904      043091e5       ldr r3, [r1, 4]
  │ │││││   0x00017908      1262a0e3       mov r6, 0x20000001
  │ │││││   0x0001790c      102093e5       ldr r2, [r3, 0x10]
  │ │││││   0x00017910      0c3093e5       ldr r3, [r3, 0xc]
  │ │││││   0x00017914      182080e5       str r2, [r0, 0x18]
  └───────> 0x00017918      143084e5       str r3, [r4, 0x14]
   ┌──────< 0x0001791c      cf0300ea       b 0x18860
  ────────> 0x00017920      042091e5       ldr r2, [r1, 4]
   ││││││   0x00017924      0c3092e5       ldr r3, [r2, 0xc]
   ││││││   0x00017928      102092e5       ldr r2, [r2, 0x10]
   ││││││   0x0001792c      040053e3       cmp r3, 4
   ││││││   0x00017930      183080e5       str r3, [r0, 0x18]
   ││││││   0x00017934      1c2080e5       str r2, [r0, 0x1c]
  ┌───────< 0x00017938      1500001a       bne 0x17994
  │││││││   0x0001793c      ff3f83e2       add r3, r3, 0x3fc
  ──└─────> 0x00017940      103084e5       str r3, [r4, 0x10]
  ────────> 0x00017944      10e094e5       ldr lr, [r4, 0x10]
  ││ ││││   0x00017948      d03602e3       movw r3, 0x26d0
  ││ ││││   0x0001794c      02c0a0e3       mov ip, 2
  ││ ││││   0x00017950      f0208de2       add r2, sp, 0xf0
  ││ ││││   0x00017954      030094e7       ldr r0, [r4, r3]
  ││ ││││   0x00017958      0c10a0e1       mov r1, ip
  ││ ││││   0x0001795c      0c30a0e3       mov r3, 0xc
  ││ ││││   0x00017960      f4c08de5       str ip, [sp, 0xf4]
  ││ ││││   0x00017964      f8e08de5       str lr, [sp, 0xf8]
  ││ ││││   0x00017968      00c0a0e3       mov ip, 0
  ││ ││││   0x0001796c      0470a0e3       mov r7, 4
  ││ ││││   0x00017970      08c08de5       str ip, [sp, 8]
  ││ ││││   0x00017974      00c08de5       str ip, [sp]
  ││ ││││   0x00017978      1262a0e3       mov r6, 0x20000001
  ││ ││││   0x0001797c      f0708de5       str r7, [sp, 0xf0]
  ││ ││││   0x00017980      04c08de5       str ip, [sp, 4]
  ││ ││││   0x00017984      67bbffeb       bl rsym.QVD_IOControl
  ││ ││││   0x00017988      001050e2       subs r1, r0, 0
  ││┌─────< 0x0001798c      a803000a       beq 0x18834
  ────────< 0x00017990      910300ea       b 0x187dc
  └───────> 0x00017994      013043e2       sub r3, r3, 1
   ││││││   0x00017998      010053e3       cmp r3, 1
   ││││││   0x0001799c      023ba093       movls r3, 0x800
  ┌───────< 0x000179a0      c203008a       bhi 0x188b0
  ────────< 0x000179a4      e5ffffea       b 0x17940
  ────────> 0x000179a8      048091e5       ldr r8, [r1, 4]
  │││││││   0x000179ac      1130a0e3       mov r3, 0x11
  │││││││   0x000179b0      02a0a0e3       mov sl, 2
  │││││││   0x000179b4      2c318de5       str r3, [sp, 0x12c]
  │││││││   0x000179b8      30a18de5       str sl, [sp, 0x130]
  │││││││   0x000179bc      4d2f8de2       add r2, sp, 0x134
  │││││││   0x000179c0      0c1098e5       ldr r1, [r8, 0xc]
  │││││││   0x000179c4      0130a0e3       mov r3, 1
  │││││││   0x000179c8      c9baffeb       bl rsym._ZN17MMIVdecController14MapPictureTypeEmPmb
  │││││││   0x000179cc      006050e2       subs r6, r0, 0
  ────────< 0x000179d0      0a00001a       bne 0x17a00
  │││││││   0x000179d4      d00602e3       movw r0, 0x26d0
  │││││││   0x000179d8      0a10a0e1       mov r1, sl
  │││││││   0x000179dc      4b2f8de2       add r2, sp, 0x12c
  │││││││   0x000179e0      0c30a0e3       mov r3, 0xc
  │││││││   0x000179e4      000094e7       ldr r0, [r4, r0]
  │││││││   0x000179e8      08608de5       str r6, [sp, 8]
  │││││││   0x000179ec      00608de5       str r6, [sp]
  │││││││   0x000179f0      04608de5       str r6, [sp, 4]
  │││││││   0x000179f4      4bbbffeb       bl rsym.QVD_IOControl
  │││││││   0x000179f8      006050e2       subs r6, r0, 0
  ────────< 0x000179fc      a903000a       beq 0x188a8
  ────────> 0x00017a00      701b9fe5       ldr r1, [0x00018578]        ; [0x18578:4]=0xffffd1b1
  │││││││   0x00017a04      00e0a0e3       mov lr, 0
  │││││││   0x00017a08      580b9fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017a0c      0620a0e3       mov r2, 6
  │││││││   0x00017a10      011085e0       add r1, r5, r1
  │││││││   0x00017a14      00108de5       str r1, [sp]
  │││││││   0x00017a18      0cc098e5       ldr ip, [r8, 0xc]
  │││││││   0x00017a1c      0330a0e3       mov r3, 3
  │││││││   0x00017a20      000085e0       add r0, r5, r0
  │││││││   0x00017a24      651800e3       movw r1, 0x865
  │││││││   0x00017a28      08608de5       str r6, [sp, 8]
  │││││││   0x00017a2c      0ce08de5       str lr, [sp, 0xc]
  │││││││   0x00017a30      04c08de5       str ip, [sp, 4]
  ────────< 0x00017a34      c50000ea       b 0x17d50
  ────────> 0x00017a38      043091e5       ldr r3, [r1, 4]
  │││││││   0x00017a3c      d01602e3       movw r1, 0x26d0
  │││││││   0x00017a40      02e0a0e3       mov lr, 2
  │││││││   0x00017a44      0060a0e3       mov r6, 0
  │││││││   0x00017a48      010090e7       ldr r0, [r0, r1]
  │││││││   0x00017a4c      452f8de2       add r2, sp, 0x114
  │││││││   0x00017a50      0cc093e5       ldr ip, [r3, 0xc]
  │││││││   0x00017a54      0e10a0e1       mov r1, lr
  │││││││   0x00017a58      0c30a0e3       mov r3, 0xc
  │││││││   0x00017a5c      18e18de5       str lr, [sp, 0x118]
  │││││││   0x00017a60      06005ce1       cmp ip, r6
  │││││││   0x00017a64      00c0a013       movne ip, 0
  │││││││   0x00017a68      01c0a003       moveq ip, 1
  │││││││   0x00017a6c      00608de5       str r6, [sp]
  │││││││   0x00017a70      1cc1cde5       strb ip, [sp, 0x11c]
  │││││││   0x00017a74      15c0a0e3       mov ip, 0x15
  │││││││   0x00017a78      04608de5       str r6, [sp, 4]
  │││││││   0x00017a7c      14c18de5       str ip, [sp, 0x114]
  │││││││   0x00017a80      08608de5       str r6, [sp, 8]
  │││││││   0x00017a84      27bbffeb       bl rsym.QVD_IOControl
  │││││││   0x00017a88      008050e2       subs r8, r0, 0
  ────────< 0x00017a8c      8503000a       beq 0x188a8
  │││││││   0x00017a90      d00a9fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017a94      0620a0e3       mov r2, 6
  │││││││   0x00017a98      0330a0e3       mov r3, 3
  │││││││   0x00017a9c      7b1800e3       movw r1, 0x87b
  │││││││   0x00017aa0      000085e0       add r0, r5, r0
  │││││││   0x00017aa4      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00017aa8      ccca9fe5       ldr ip, [0x0001857c]        ; [0x1857c:4]=0xffffd1e7
  ────────< 0x00017aac      4c0000ea       b 0x17be4
  ────────> 0x00017ab0      043091e5       ldr r3, [r1, 4]
  │││││││   0x00017ab4      0c2093e5       ldr r2, [r3, 0xc]
  │││││││   0x00017ab8      010052e3       cmp r2, 1
  ────────< 0x00017abc      0200001a       bne 0x17acc
  │││││││   0x00017ac0      102093e5       ldr r2, [r3, 0x10]
  │││││││   0x00017ac4      010052e3       cmp r2, 1
  ────────< 0x00017ac8      7803001a       bne 0x188b0
  ────────> 0x00017acc      0ce093e5       ldr lr, [r3, 0xc]
  │││││││   0x00017ad0      d03602e3       movw r3, 0x26d0
  │││││││   0x00017ad4      02c0a0e3       mov ip, 2
  │││││││   0x00017ad8      122e8de2       add r2, sp, 0x120
  │││││││   0x00017adc      030094e7       ldr r0, [r4, r3]
  │││││││   0x00017ae0      01005ee3       cmp lr, 1
  │││││││   0x00017ae4      0c10a0e1       mov r1, ip
  │││││││   0x00017ae8      0c30a0e3       mov r3, 0xc
  │││││││   0x00017aec      0ce0a001       moveq lr, ip
  │││││││   0x00017af0      01e0a013       movne lr, 1
  │││││││   0x00017af4      0060a0e3       mov r6, 0
  │││││││   0x00017af8      24c18de5       str ip, [sp, 0x124]
  │││││││   0x00017afc      28e18de5       str lr, [sp, 0x128]
  │││││││   0x00017b00      10c08ce2       add ip, ip, 0x10
  │││││││   0x00017b04      00608de5       str r6, [sp]
  │││││││   0x00017b08      20c18de5       str ip, [sp, 0x120]
  │││││││   0x00017b0c      04608de5       str r6, [sp, 4]
  │││││││   0x00017b10      08608de5       str r6, [sp, 8]
  │││││││   0x00017b14      03bbffeb       bl rsym.QVD_IOControl
  │││││││   0x00017b18      008050e2       subs r8, r0, 0
  ────────< 0x00017b1c      6103000a       beq 0x188a8
  │││││││   0x00017b20      400a9fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017b24      0620a0e3       mov r2, 6
  │││││││   0x00017b28      0330a0e3       mov r3, 3
  │││││││   0x00017b2c      9d1800e3       movw r1, 0x89d
  │││││││   0x00017b30      000085e0       add r0, r5, r0
  │││││││   0x00017b34      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00017b38      40ca9fe5       ldr ip, [0x00018580]        ; [0x18580:4]=0xffffd20f
  ────────< 0x00017b3c      280000ea       b 0x17be4
  ────────> 0x00017b40      043091e5       ldr r3, [r1, 4]
  │││││││   0x00017b44      000053e3       cmp r3, 0
  ────────< 0x00017b48      5803000a       beq 0x188b0
  │││││││   0x00017b4c      002093e5       ldr r2, [r3]
  │││││││   0x00017b50      1c0052e3       cmp r2, 0x1c
  ────────< 0x00017b54      5503001a       bne 0x188b0
  │││││││   0x00017b58      0c2093e5       ldr r2, [r3, 0xc]
  │││││││   0x00017b5c      010052e3       cmp r2, 1
  ────────< 0x00017b60      0200001a       bne 0x17b70
  │││││││   0x00017b64      142093e5       ldr r2, [r3, 0x14]
  │││││││   0x00017b68      010052e3       cmp r2, 1
  ────────< 0x00017b6c      4f03001a       bne 0x188b0
  ────────> 0x00017b70      0c1093e5       ldr r1, [r3, 0xc]
  │││││││   0x00017b74      1620a0e3       mov r2, 0x16
  │││││││   0x00017b78      e0208de5       str r2, [sp, 0xe0]
  │││││││   0x00017b7c      0220a0e3       mov r2, 2
  │││││││   0x00017b80      e4208de5       str r2, [sp, 0xe4]
  │││││││   0x00017b84      010051e3       cmp r1, 1
  │││││││   0x00017b88      18c093e5       ldr ip, [r3, 0x18]
  │││││││   0x00017b8c      d03602e3       movw r3, 0x26d0
  │││││││   0x00017b90      0120a013       movne r2, 1
  │││││││   0x00017b94      0210a0e3       mov r1, 2
  │││││││   0x00017b98      030094e7       ldr r0, [r4, r3]
  │││││││   0x00017b9c      9b3d43e2       sub r3, r3, 0x26c0
  │││││││   0x00017ba0      e8208de5       str r2, [sp, 0xe8]
  │││││││   0x00017ba4      e0208de2       add r2, sp, 0xe0
  │││││││   0x00017ba8      0060a0e3       mov r6, 0
  │││││││   0x00017bac      ecc08de5       str ip, [sp, 0xec]
  │││││││   0x00017bb0      00608de5       str r6, [sp]
  │││││││   0x00017bb4      04608de5       str r6, [sp, 4]
  │││││││   0x00017bb8      08608de5       str r6, [sp, 8]
  │││││││   0x00017bbc      d9baffeb       bl rsym.QVD_IOControl
  │││││││   0x00017bc0      008050e2       subs r8, r0, 0
  ────────< 0x00017bc4      3703000a       beq 0x188a8
  │││││││   0x00017bc8      98099fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017bcc      0620a0e3       mov r2, 6
  │││││││   0x00017bd0      acc99fe5       ldr ip, [0x00018584]        ; [0x18584:4]=0xffffd237
  │││││││   0x00017bd4      0330a0e3       mov r3, 3
  │││││││   0x00017bd8      000085e0       add r0, r5, r0
  │││││││   0x00017bdc      c91800e3       movw r1, 0x8c9
  │││││││   0x00017be0      0c608de5       str r6, [sp, 0xc]
  ────────> 0x00017be4      0cc085e0       add ip, r5, ip
  │││││││   0x00017be8      04808de5       str r8, [sp, 4]
  │││││││   0x00017bec      00c08de5       str ip, [sp]
  ────────> 0x00017bf0      08608de5       str r6, [sp, 8]
  │││││││   0x00017bf4      ddbaffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00017bf8      0400a0e1       mov r0, r4
  │││││││   0x00017bfc      0810a0e1       mov r1, r8
  ────────> 0x00017c00      18b9ffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  ────────< 0x00017c04      3b0200ea       b 0x184f8
  ────────> 0x00017c08      048091e5       ldr r8, [r1, 4]
  │││││││   0x00017c0c      74399fe5       ldr r3, [0x00018588]        ; [0x18588:4]=0x2000001
  │││││││   0x00017c10      10a098e5       ldr sl, [r8, 0x10]
  │││││││   0x00017c14      03005ae1       cmp sl, r3
  ────────< 0x00017c18      3200001a       bne 0x17ce8
  │││││││   0x00017c1c      083098e5       ldr r3, [r8, 8]
  │││││││   0x00017c20      010053e3       cmp r3, 1
  ────────< 0x00017c24      6100001a       bne 0x17db0
  │││││││   0x00017c28      0ce098e5       ldr lr, [r8, 0xc]
  │││││││   0x00017c2c      d03602e3       movw r3, 0x26d0
  │││││││   0x00017c30      02c0a0e3       mov ip, 2
  │││││││   0x00017c34      452f8de2       add r2, sp, 0x114
  │││││││   0x00017c38      030090e7       ldr r0, [r0, r3]
  │││││││   0x00017c3c      0c10a0e1       mov r1, ip
  │││││││   0x00017c40      0c30a0e3       mov r3, 0xc
  │││││││   0x00017c44      0060a0e3       mov r6, 0
  │││││││   0x00017c48      18c18de5       str ip, [sp, 0x118]
  │││││││   0x00017c4c      12c08ce2       add ip, ip, 0x12
  │││││││   0x00017c50      00608de5       str r6, [sp]
  │││││││   0x00017c54      04608de5       str r6, [sp, 4]
  │││││││   0x00017c58      08608de5       str r6, [sp, 8]
  │││││││   0x00017c5c      1ce1cde5       strb lr, [sp, 0x11c]
  │││││││   0x00017c60      14c18de5       str ip, [sp, 0x114]
  │││││││   0x00017c64      afbaffeb       bl rsym.QVD_IOControl
  │││││││   0x00017c68      00a050e2       subs sl, r0, 0
  │││││││   0x00017c6c      12628602       addeq r6, r6, 0x20000001
  ────────< 0x00017c70      0f00000a       beq 0x17cb4
  │││││││   0x00017c74      ec089fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017c78      0620a0e3       mov r2, 6
  │││││││   0x00017c7c      08c99fe5       ldr ip, [0x0001858c]        ; [0x1858c:4]=0xffffd268
  │││││││   0x00017c80      0330a0e3       mov r3, 3
  │││││││   0x00017c84      000085e0       add r0, r5, r0
  │││││││   0x00017c88      f21800e3       movw r1, 0x8f2
  │││││││   0x00017c8c      0cc085e0       add ip, r5, ip
  │││││││   0x00017c90      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00017c94      00c08de5       str ip, [sp]
  │││││││   0x00017c98      08608de5       str r6, [sp, 8]
  │││││││   0x00017c9c      04a08de5       str sl, [sp, 4]
  │││││││   0x00017ca0      b2baffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00017ca4      0400a0e1       mov r0, r4
  │││││││   0x00017ca8      0a10a0e1       mov r1, sl
  │││││││   0x00017cac      edb8ffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  │││││││   0x00017cb0      0060a0e1       mov r6, r0
  ────────> 0x00017cb4      0c2098e5       ldr r2, [r8, 0xc]
  │││││││   0x00017cb8      503702e3       movw r3, 0x2750             ; 'P\''
  │││││││   0x00017cbc      08c0a0e1       mov ip, r8
  │││││││   0x00017cc0      9dad84e2       add sl, r4, 0x2740
  │││││││   0x00017cc4      002052e2       subs r2, r2, 0
  │││││││   0x00017cc8      0120a013       movne r2, 1
  │││││││   0x00017ccc      0320c4e7       strb r2, [r4, r3]
  │││││││   0x00017cd0      0f00bce8       ldm ip!, {r0, r1, r2, r3}
  │││││││   0x00017cd4      14a08ae2       add sl, sl, 0x14
  │││││││   0x00017cd8      0f00aae8       stm sl!, {r0, r1, r2, r3}
  │││││││   0x00017cdc      00309ce5       ldr r3, [ip]
  │││││││   0x00017ce0      00308ae5       str r3, [sl]
  ────────< 0x00017ce4      040200ea       b 0x184fc
  ────────> 0x00017ce8      a0389fe5       ldr r3, [0x00018590]        ; [0x18590:4]=0x7f10000c
  │││││││   0x00017cec      03005ae1       cmp sl, r3
  ────────< 0x00017cf0      2e00001a       bne 0x17db0
  │││││││   0x00017cf4      083098e5       ldr r3, [r8, 8]
  │││││││   0x00017cf8      010053e3       cmp r3, 1
  ────────< 0x00017cfc      2b00001a       bne 0x17db0
  │││││││   0x00017d00      0c3098e5       ldr r3, [r8, 0xc]
  │││││││   0x00017d04      8010a0e3       mov r1, 0x80
  │││││││   0x00017d08      84289fe5       ldr r2, [0x00018594]        ; [0x18594:4]=0x7f100002
  │││││││   0x00017d0c      003053e2       subs r3, r3, 0
  │││││││   0x00017d10      0130a013       movne r3, 1
  │││││││   0x00017d14      07bbffeb       bl rsym._ZN17MMIVdecController15ConfigExtraDataEiib
  │││││││   0x00017d18      006050e2       subs r6, r0, 0
  ────────< 0x00017d1c      0f00000a       beq 0x17d60
  │││││││   0x00017d20      40089fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017d24      0620a0e3       mov r2, 6
  │││││││   0x00017d28      68c89fe5       ldr ip, [0x00018598]        ; [0x18598:4]=0xffffd29e
  │││││││   0x00017d2c      0330a0e3       mov r3, 3
  │││││││   0x00017d30      000085e0       add r0, r5, r0
  │││││││   0x00017d34      071900e3       movw r1, 0x907
  ────────> 0x00017d38      0cc085e0       add ip, r5, ip
  │││││││   0x00017d3c      00c08de5       str ip, [sp]
  │││││││   0x00017d40      00c0a0e3       mov ip, 0
  │││││││   0x00017d44      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00017d48      04c08de5       str ip, [sp, 4]
  │││││││   0x00017d4c      08c08de5       str ip, [sp, 8]
  ────────> 0x00017d50      86baffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00017d54      0400a0e1       mov r0, r4
  │││││││   0x00017d58      0610a0e1       mov r1, r6
  ────────< 0x00017d5c      a7ffffea       b 0x17c00
  ────────> 0x00017d60      0c3098e5       ldr r3, [r8, 0xc]
  │││││││   0x00017d64      0a20a0e1       mov r2, sl
  │││││││   0x00017d68      0400a0e1       mov r0, r4
  │││││││   0x00017d6c      011ca0e3       mov r1, 0x100
  │││││││   0x00017d70      003053e2       subs r3, r3, 0
  │││││││   0x00017d74      0130a013       movne r3, 1
  │││││││   0x00017d78      eebaffeb       bl rsym._ZN17MMIVdecController15ConfigExtraDataEiib
  │││││││   0x00017d7c      008050e2       subs r8, r0, 0
  ────────< 0x00017d80      c802000a       beq 0x188a8
  │││││││   0x00017d84      dc079fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017d88      0620a0e3       mov r2, 6
  │││││││   0x00017d8c      08c89fe5       ldr ip, [0x0001859c]        ; [0x1859c:4]=0xffffd2c0
  │││││││   0x00017d90      0330a0e3       mov r3, 3
  │││││││   0x00017d94      000085e0       add r0, r5, r0
  │││││││   0x00017d98      121900e3       movw r1, 0x912
  │││││││   0x00017d9c      0cc085e0       add ip, r5, ip
  │││││││   0x00017da0      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00017da4      00c08de5       str ip, [sp]
  │││││││   0x00017da8      04608de5       str r6, [sp, 4]
  ────────< 0x00017dac      8fffffea       b 0x17bf0
  ────────> 0x00017db0      b0079fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017db4      1a1900e3       movw r1, 0x91a
  │││││││   0x00017db8      0620a0e3       mov r2, 6
  │││││││   0x00017dbc      0330a0e3       mov r3, 3
  │││││││   0x00017dc0      000085e0       add r0, r5, r0
  │││││││   0x00017dc4      04a08de5       str sl, [sp, 4]
  │││││││   0x00017dc8      d0c79fe5       ldr ip, [0x000185a0]        ; [0x185a0:4]=0xffffd2e2
  ────────< 0x00017dcc      360000ea       b 0x17eac
  │││└────> 0x00017dd0      043091e5       ldr r3, [r1, 4]
  │││ │││   0x00017dd4      021ba0e3       mov r1, 0x800
  │││ │││   0x00017dd8      c4279fe5       ldr r2, [0x000185a4]        ; [0x185a4:4]=0x7f100001
  │││ │││   0x00017ddc      0c3093e5       ldr r3, [r3, 0xc]
  │││ │││   0x00017de0      003053e2       subs r3, r3, 0
  │││ │││   0x00017de4      0130a013       movne r3, 1
  │││ │││   0x00017de8      d2baffeb       bl rsym._ZN17MMIVdecController15ConfigExtraDataEiib
  │││ │││   0x00017dec      006050e2       subs r6, r0, 0
  │││┌────< 0x00017df0      ac02000a       beq 0x188a8
  │││││││   0x00017df4      6c079fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017df8      0620a0e3       mov r2, 6
  │││││││   0x00017dfc      0330a0e3       mov r3, 3
  │││││││   0x00017e00      2b1900e3       movw r1, 0x92b
  │││││││   0x00017e04      000085e0       add r0, r5, r0
  │││││││   0x00017e08      98c79fe5       ldr ip, [0x000185a8]        ; [0x185a8:4]=0xffffd31a
  ────────< 0x00017e0c      c9ffffea       b 0x17d38
  ────────> 0x00017e10      048091e5       ldr r8, [r1, 4]
  │││││││   0x00017e14      0c7098e5       ldr r7, [r8, 0xc]
  │││││││   0x00017e18      000057e3       cmp r7, 0
  ────────< 0x00017e1c      0800001a       bne 0x17e44
  │││││││   0x00017e20      e86702e3       movw r6, 0x27e8
  │││││││   0x00017e24      063090e7       ldr r3, [r0, r6]
  │││││││   0x00017e28      000053e3       cmp r3, 0
  ────────< 0x00017e2c      9d02000a       beq 0x188a8
  │││││││   0x00017e30      0300a0e1       mov r0, r3
  │││││││   0x00017e34      143093e5       ldr r3, [r3, 0x14]
  │││││││   0x00017e38      33ff2fe1       blx r3
  │││││││   0x00017e3c      067084e7       str r7, [r4, r6]
  ────────< 0x00017e40      980200ea       b 0x188a8
  ────────> 0x00017e44      e83702e3       movw r3, 0x27e8
  │││││││   0x00017e48      033090e7       ldr r3, [r0, r3]
  │││││││   0x00017e4c      000053e3       cmp r3, 0
  ────────< 0x00017e50      8a02001a       bne 0x18880
  │││││││   0x00017e54      50179fe5       ldr r1, [0x000185ac]        ; [0x185ac:4]=0x45524651
  │││││││   0x00017e58      eab9ffeb       bl rsym._ZN17MMIVdecController14EnablePostProcEi
  │││││││   0x00017e5c      00c050e2       subs ip, r0, 0
  ────────< 0x00017e60      8602001a       bne 0x18880
  │││││││   0x00017e64      fc069fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017e68      441900e3       movw r1, 0x944
  │││││││   0x00017e6c      3ce79fe5       ldr lr, [0x000185b0]        ; [0x185b0:4]=0xffffd345
  │││││││   0x00017e70      0620a0e3       mov r2, 6
  │││││││   0x00017e74      000085e0       add r0, r5, r0
  │││││││   0x00017e78      0330a0e3       mov r3, 3
  │││││││   0x00017e7c      0ee085e0       add lr, r5, lr
  │││││││   0x00017e80      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00017e84      00e08de5       str lr, [sp]
  │││││││   0x00017e88      2262a0e3       mov r6, 0x20000002
  ────────< 0x00017e8c      220100ea       b 0x1831c
  ────────> 0x00017e90      d0069fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017e94      4c1900e3       movw r1, 0x94c
  │││││││   0x00017e98      14c79fe5       ldr ip, [0x000185b4]        ; [0x185b4:4]=0xffffd35b
  │││││││   0x00017e9c      0620a0e3       mov r2, 6
  │││││││   0x00017ea0      000085e0       add r0, r5, r0
  │││││││   0x00017ea4      0330a0e3       mov r3, 3
  │││││││   0x00017ea8      04608de5       str r6, [sp, 4]
  ────────> 0x00017eac      0cc085e0       add ip, r5, ip
  │││││││   0x00017eb0      2262a0e3       mov r6, 0x20000002
  │││││││   0x00017eb4      00c08de5       str ip, [sp]
  │││││││   0x00017eb8      00c0a0e3       mov ip, 0
  │││││││   0x00017ebc      0cc08de5       str ip, [sp, 0xc]
  ────────< 0x00017ec0      200200ea       b 0x18748
  ────────> 0x00017ec4      0a3094e7       ldr r3, [r4, sl]
  │││││││   0x00017ec8      0410a0e3       mov r1, 4
  │││││││   0x00017ecc      142088e2       add r2, r8, 0x14
  │││││││   0x00017ed0      0300a0e1       mov r0, r3
  │││││││   0x00017ed4      003093e5       ldr r3, [r3]
  │││││││   0x00017ed8      33ff2fe1       blx r3
  │││││││   0x00017edc      007050e2       subs r7, r0, 0
  ────────< 0x00017ee0      0c00000a       beq 0x17f18
  │││││││   0x00017ee4      cc369fe5       ldr r3, [0x000185b8]        ; [0x185b8:4]=0xffffd395
  │││││││   0x00017ee8      531900e3       movw r1, 0x953
  │││││││   0x00017eec      74069fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017ef0      0620a0e3       mov r2, 6
  │││││││   0x00017ef4      033085e0       add r3, r5, r3
  │││││││   0x00017ef8      00308de5       str r3, [sp]
  │││││││   0x00017efc      14c098e5       ldr ip, [r8, 0x14]
  │││││││   0x00017f00      000085e0       add r0, r5, r0
  │││││││   0x00017f04      0330a0e3       mov r3, 3
  │││││││   0x00017f08      08708de5       str r7, [sp, 8]
  │││││││   0x00017f0c      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00017f10      04c08de5       str ip, [sp, 4]
  ────────< 0x00017f14      130000ea       b 0x17f68
  ────────> 0x00017f18      0a3094e7       ldr r3, [r4, sl]
  │││││││   0x00017f1c      0610a0e3       mov r1, 6
  │││││││   0x00017f20      182088e2       add r2, r8, 0x18
  │││││││   0x00017f24      0300a0e1       mov r0, r3
  │││││││   0x00017f28      003093e5       ldr r3, [r3]
  │││││││   0x00017f2c      33ff2fe1       blx r3
  │││││││   0x00017f30      00c050e2       subs ip, r0, 0
  ────────< 0x00017f34      5b02000a       beq 0x188a8
  │││││││   0x00017f38      7c369fe5       ldr r3, [0x000185bc]        ; [0x185bc:4]=0xffffd3d1
  │││││││   0x00017f3c      5a1900e3       movw r1, 0x95a
  │││││││   0x00017f40      20069fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017f44      0620a0e3       mov r2, 6
  │││││││   0x00017f48      033085e0       add r3, r5, r3
  │││││││   0x00017f4c      00308de5       str r3, [sp]
  │││││││   0x00017f50      18e098e5       ldr lr, [r8, 0x18]
  │││││││   0x00017f54      000085e0       add r0, r5, r0
  │││││││   0x00017f58      0330a0e3       mov r3, 3
  │││││││   0x00017f5c      08c08de5       str ip, [sp, 8]
  │││││││   0x00017f60      0c708de5       str r7, [sp, 0xc]
  │││││││   0x00017f64      04e08de5       str lr, [sp, 4]
  ────────> 0x00017f68      00baffeb       bl sym.imp.MM_OutputDebugMessage
  ────────> 0x00017f6c      b4669fe5       ldr r6, [0x00018628]        ; [0x18628:4]=0x20000010
  ────────< 0x00017f70      3a0200ea       b 0x18860
  ────────> 0x00017f74      048091e5       ldr r8, [r1, 4]
  │││││││   0x00017f78      bec0d8e1       ldrh ip, [r8, 0xe]
  │││││││   0x00017f7c      00005ce3       cmp ip, 0
  ────────< 0x00017f80      0900001a       bne 0x17fac
  │││││││   0x00017f84      dc059fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00017f88      6b1900e3       movw r1, 0x96b
  │││││││   0x00017f8c      2ce69fe5       ldr lr, [0x000185c0]        ; [0x185c0:4]=0xffffd416
  │││││││   0x00017f90      0620a0e3       mov r2, 6
  │││││││   0x00017f94      000085e0       add r0, r5, r0
  │││││││   0x00017f98      0230a0e3       mov r3, 2
  │││││││   0x00017f9c      0ee085e0       add lr, r5, lr
  │││││││   0x00017fa0      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00017fa4      00e08de5       str lr, [sp]
  ────────< 0x00017fa8      da0000ea       b 0x18318
  ────────> 0x00017fac      0230a0e3       mov r3, 2
  │││││││   0x00017fb0      d4308de5       str r3, [sp, 0xd4]
  │││││││   0x00017fb4      153083e2       add r3, r3, 0x15
  │││││││   0x00017fb8      d0308de5       str r3, [sp, 0xd0]
  │││││││   0x00017fbc      0cc098e5       ldr ip, [r8, 0xc]
  │││││││   0x00017fc0      0030a0e3       mov r3, 0
  │││││││   0x00017fc4      0c20a0e1       mov r2, ip
  ────────< 0x00017fc8      010000ea       b 0x17fd4
  ────────> 0x00017fcc      a220a0e1       lsr r2, r2, 1
  │││││││   0x00017fd0      013083e2       add r3, r3, 1
  ────────> 0x00017fd4      016022e2       eor r6, r2, 1
  │││││││   0x00017fd8      0f0053e3       cmp r3, 0xf
  │││││││   0x00017fdc      0060a083       movhi r6, 0
  │││││││   0x00017fe0      01600692       andls r6, r6, 1
  │││││││   0x00017fe4      000056e3       cmp r6, 0
  ────────< 0x00017fe8      f7ffff1a       bne 0x17fcc
  │││││││   0x00017fec      3cc3a0e1       lsr ip, ip, r3
  │││││││   0x00017ff0      102063e2       rsb r2, r3, 0x10
  │││││││   0x00017ff4      01e0a0e3       mov lr, 1
  │││││││   0x00017ff8      c4359fe5       ldr r3, [0x000185c4]        ; [0x185c4:4]=0xffffd447
  │││││││   0x00017ffc      64959fe5       ldr sb, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00018000      721900e3       movw r1, 0x972
  │││││││   0x00018004      1ee2a0e1       lsl lr, lr, r2
  │││││││   0x00018008      dcc08de5       str ip, [sp, 0xdc]
  │││││││   0x0001800c      d8e08de5       str lr, [sp, 0xd8]
  │││││││   0x00018010      033085e0       add r3, r5, r3
  │││││││   0x00018014      00308de5       str r3, [sp]
  │││││││   0x00018018      099085e0       add sb, r5, sb
  │││││││   0x0001801c      0ca098e5       ldr sl, [r8, 0xc]
  │││││││   0x00018020      0900a0e1       mov r0, sb
  │││││││   0x00018024      0620a0e3       mov r2, 6
  │││││││   0x00018028      0230a0e3       mov r3, 2
  │││││││   0x0001802c      00548de9       stmib sp, {sl, ip, lr}
  │││││││   0x00018030      ceb9ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018034      d00602e3       movw r0, 0x26d0
  │││││││   0x00018038      0210a0e3       mov r1, 2
  │││││││   0x0001803c      d0208de2       add r2, sp, 0xd0
  │││││││   0x00018040      1030a0e3       mov r3, 0x10
  │││││││   0x00018044      000094e7       ldr r0, [r4, r0]
  │││││││   0x00018048      00608de5       str r6, [sp]
  │││││││   0x0001804c      04608de5       str r6, [sp, 4]
  │││││││   0x00018050      08608de5       str r6, [sp, 8]
  │││││││   0x00018054      b3b9ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018058      00a050e2       subs sl, r0, 0
  ────────< 0x0001805c      0d00000a       beq 0x18098
  │││││││   0x00018060      60c59fe5       ldr ip, [0x000185c8]        ; [0x185c8:4]=0xffffd47f
  │││││││   0x00018064      0900a0e1       mov r0, sb
  │││││││   0x00018068      0620a0e3       mov r2, 6
  │││││││   0x0001806c      0330a0e3       mov r3, 3
  │││││││   0x00018070      791900e3       movw r1, 0x979
  │││││││   0x00018074      0cc085e0       add ip, r5, ip
  │││││││   0x00018078      0c608de5       str r6, [sp, 0xc]
  │││││││   0x0001807c      00c08de5       str ip, [sp]
  │││││││   0x00018080      04a08de5       str sl, [sp, 4]
  │││││││   0x00018084      08608de5       str r6, [sp, 8]
  ────────> 0x00018088      b8b9ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x0001808c      0400a0e1       mov r0, r4
  │││││││   0x00018090      0a10a0e1       mov r1, sl
  ────────< 0x00018094      d9feffea       b 0x17c00
  ────────> 0x00018098      2c359fe5       ldr r3, [0x000185cc]        ; [0x185cc:4]=0xffffd4aa
  │││││││   0x0001809c      0900a0e1       mov r0, sb
  │││││││   0x000180a0      7f1900e3       movw r1, 0x97f
  │││││││   0x000180a4      0620a0e3       mov r2, 6
  │││││││   0x000180a8      033085e0       add r3, r5, r3
  │││││││   0x000180ac      00308de5       str r3, [sp]
  │││││││   0x000180b0      0cc098e5       ldr ip, [r8, 0xc]
  │││││││   0x000180b4      0230a0e3       mov r3, 2
  │││││││   0x000180b8      0ca08de5       str sl, [sp, 0xc]
  │││││││   0x000180bc      1262a0e3       mov r6, 0x20000001
  │││││││   0x000180c0      08a08de5       str sl, [sp, 8]
  │││││││   0x000180c4      04c08de5       str ip, [sp, 4]
  ────────< 0x000180c8      9f0100ea       b 0x1874c
  ────────> 0x000180cc      94649fe5       ldr r6, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x000180d0      0080a0e3       mov r8, 0
  │││││││   0x000180d4      04a091e5       ldr sl, [r1, 4]
  │││││││   0x000180d8      0130a0e3       mov r3, 1
  │││││││   0x000180dc      066085e0       add r6, r5, r6
  │││││││   0x000180e0      e8c49fe5       ldr ip, [0x000185d0]        ; [0x185d0:4]=0xffffd4cd
  │││││││   0x000180e4      0600a0e1       mov r0, r6
  │││││││   0x000180e8      8b1900e3       movw r1, 0x98b
  │││││││   0x000180ec      0620a0e3       mov r2, 6
  │││││││   0x000180f0      569f8de2       add sb, sp, 0x158
  │││││││   0x000180f4      0cc085e0       add ip, r5, ip
  │││││││   0x000180f8      04808de5       str r8, [sp, 4]
  │││││││   0x000180fc      00c08de5       str ip, [sp]
  │││││││   0x00018100      10a08ae2       add sl, sl, 0x10
  │││││││   0x00018104      08808de5       str r8, [sp, 8]
  │││││││   0x00018108      0c808de5       str r8, [sp, 0xc]
  │││││││   0x0001810c      97b9ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018110      412fa0e3       mov r2, 0x104
  │││││││   0x00018114      0810a0e1       mov r1, r8
  │││││││   0x00018118      0900a0e1       mov r0, sb
  │││││││   0x0001811c      2fbaffeb       bl sym.imp.std_memset
  │││││││   0x00018120      0a00a0e1       mov r0, sl
  │││││││   0x00018124      013ca0e3       mov r3, 0x100
  │││││││   0x00018128      58318de5       str r3, [sp, 0x158]
  │││││││   0x0001812c      b4b8ffeb       bl sym.imp.std_strlen
  │││││││   0x00018130      58219de5       ldr r2, [sp, 0x158]
  │││││││   0x00018134      020050e1       cmp r0, r2
  ────────< 0x00018138      1d00002a       bhs 0x181b4
  │││││││   0x0001813c      0a10a0e1       mov r1, sl
  │││││││   0x00018140      570f8de2       add r0, sp, 0x15c
  │││││││   0x00018144      49baffeb       bl sym.imp.std_strlcpy
  │││││││   0x00018148      d03602e3       movw r3, 0x26d0
  │││││││   0x0001814c      02c0a0e3       mov ip, 2
  │││││││   0x00018150      4e2f8de2       add r2, sp, 0x138
  │││││││   0x00018154      030094e7       ldr r0, [r4, r3]
  │││││││   0x00018158      0c10a0e1       mov r1, ip
  │││││││   0x0001815c      3cc18de5       str ip, [sp, 0x13c]
  │││││││   0x00018160      0830a0e3       mov r3, 8
  │││││││   0x00018164      16c08ce2       add ip, ip, 0x16
  │││││││   0x00018168      00908de5       str sb, [sp]
  │││││││   0x0001816c      38c18de5       str ip, [sp, 0x138]
  │││││││   0x00018170      ecc08ce2       add ip, ip, 0xec
  │││││││   0x00018174      08808de5       str r8, [sp, 8]
  │││││││   0x00018178      04c08de5       str ip, [sp, 4]
  │││││││   0x0001817c      69b9ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018180      00a050e2       subs sl, r0, 0
  ────────< 0x00018184      c701000a       beq 0x188a8
  │││││││   0x00018188      44c49fe5       ldr ip, [0x000185d4]        ; [0x185d4:4]=0xffffd4f7
  │││││││   0x0001818c      0600a0e1       mov r0, r6
  │││││││   0x00018190      0620a0e3       mov r2, 6
  │││││││   0x00018194      0330a0e3       mov r3, 3
  │││││││   0x00018198      0cc085e0       add ip, r5, ip
  │││││││   0x0001819c      9e1900e3       movw r1, 0x99e
  │││││││   0x000181a0      0c808de5       str r8, [sp, 0xc]
  │││││││   0x000181a4      00c08de5       str ip, [sp]
  │││││││   0x000181a8      04808de5       str r8, [sp, 4]
  │││││││   0x000181ac      08808de5       str r8, [sp, 8]
  ────────< 0x000181b0      b4ffffea       b 0x18088
  ────────> 0x000181b4      0600a0e1       mov r0, r6
  │││││││   0x000181b8      18c49fe5       ldr ip, [0x000185d8]        ; [0x185d8:4]=0xffffd091
  │││││││   0x000181bc      a41900e3       movw r1, 0x9a4
  │││││││   0x000181c0      0620a0e3       mov r2, 6
  │││││││   0x000181c4      0330a0e3       mov r3, 3
  │││││││   0x000181c8      2262a0e3       mov r6, 0x20000002
  │││││││   0x000181cc      0cc085e0       add ip, r5, ip
  │││││││   0x000181d0      0c808de5       str r8, [sp, 0xc]
  │││││││   0x000181d4      00c08de5       str ip, [sp]
  │││││││   0x000181d8      04808de5       str r8, [sp, 4]
  ────────> 0x000181dc      08808de5       str r8, [sp, 8]
  ────────< 0x000181e0      590100ea       b 0x1874c
  ────────> 0x000181e4      046091e5       ldr r6, [r1, 4]
  │││││││   0x000181e8      c0a08de2       add sl, sp, 0xc0
  │││││││   0x000181ec      0010a0e3       mov r1, 0
  │││││││   0x000181f0      1020a0e3       mov r2, 0x10
  │││││││   0x000181f4      0a00a0e1       mov r0, sl
  │││││││   0x000181f8      0180a0e1       mov r8, r1
  │││││││   0x000181fc      f7b9ffeb       bl sym.imp.std_memset
  │││││││   0x00018200      0830a0e3       mov r3, 8
  │││││││   0x00018204      c0308de5       str r3, [sp, 0xc0]
  │││││││   0x00018208      0230a0e3       mov r3, 2
  │││││││   0x0001820c      c4308de5       str r3, [sp, 0xc4]
  │││││││   0x00018210      b71900e3       movw r1, 0x9b7
  │││││││   0x00018214      0c0096e5       ldr r0, [r6, 0xc]
  │││││││   0x00018218      0620a0e3       mov r2, 6
  │││││││   0x0001821c      08c096e5       ldr ip, [r6, 8]
  │││││││   0x00018220      0130a0e3       mov r3, 1
  │││││││   0x00018224      3c939fe5       ldr sb, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x00018228      cc008de5       str r0, [sp, 0xcc]
  │││││││   0x0001822c      08c05ce0       subs ip, ip, r8
  │││││││   0x00018230      01c0a013       movne ip, 1
  │││││││   0x00018234      a0039fe5       ldr r0, [0x000185dc]        ; [0x185dc:4]=0xffffd519
  │││││││   0x00018238      099085e0       add sb, r5, sb
  │││││││   0x0001823c      c8c08de5       str ip, [sp, 0xc8]
  │││││││   0x00018240      000085e0       add r0, r5, r0
  │││││││   0x00018244      00008de5       str r0, [sp]
  │││││││   0x00018248      080096e5       ldr r0, [r6, 8]
  │││││││   0x0001824c      04008de5       str r0, [sp, 4]
  │││││││   0x00018250      0900a0e1       mov r0, sb
  │││││││   0x00018254      0cc096e5       ldr ip, [r6, 0xc]
  │││││││   0x00018258      0c808de5       str r8, [sp, 0xc]
  │││││││   0x0001825c      08c08de5       str ip, [sp, 8]
  │││││││   0x00018260      42b9ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018264      d00602e3       movw r0, 0x26d0
  │││││││   0x00018268      0a20a0e1       mov r2, sl
  │││││││   0x0001826c      0110a0e3       mov r1, 1
  │││││││   0x00018270      1030a0e3       mov r3, 0x10
  │││││││   0x00018274      000094e7       ldr r0, [r4, r0]
  │││││││   0x00018278      00808de5       str r8, [sp]
  │││││││   0x0001827c      04808de5       str r8, [sp, 4]
  │││││││   0x00018280      08808de5       str r8, [sp, 8]
  │││││││   0x00018284      27b9ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018288      00a050e2       subs sl, r0, 0
  ────────< 0x0001828c      0b00000a       beq 0x182c0
  │││││││   0x00018290      48339fe5       ldr r3, [0x000185e0]        ; [0x185e0:4]=0xffffd557
  │││││││   0x00018294      0900a0e1       mov r0, sb
  │││││││   0x00018298      271da0e3       mov r1, 0x9c0
  │││││││   0x0001829c      0620a0e3       mov r2, 6
  │││││││   0x000182a0      033085e0       add r3, r5, r3
  │││││││   0x000182a4      00308de5       str r3, [sp]
  │││││││   0x000182a8      08c096e5       ldr ip, [r6, 8]
  │││││││   0x000182ac      0330a0e3       mov r3, 3
  │││││││   0x000182b0      0c808de5       str r8, [sp, 0xc]
  │││││││   0x000182b4      08a08de5       str sl, [sp, 8]
  │││││││   0x000182b8      04c08de5       str ip, [sp, 4]
  │││││││   0x000182bc      2bb9ffeb       bl sym.imp.MM_OutputDebugMessage
  ────────> 0x000182c0      0a10a0e1       mov r1, sl
  │││││││   0x000182c4      0400a0e1       mov r0, r4
  ────────< 0x000182c8      4cfeffea       b 0x17c00
  ────────> 0x000182cc      043091e5       ldr r3, [r1, 4]
  │││││││   0x000182d0      0c3093e5       ldr r3, [r3, 0xc]
  │││││││   0x000182d4      000053e3       cmp r3, 0
  ────────< 0x000182d8      7201000a       beq 0x188a8
  │││││││   0x000182dc      00339fe5       ldr r3, [0x000185e4]        ; [0x185e4:4]=0x3b8
  │││││││   0x000182e0      80029fe5       ldr r0, [0x00018568]        ; [0x18568:4]=0xffffaa0c
  │││││││   0x000182e4      033095e7       ldr r3, [r5, r3]
  │││││││   0x000182e8      008093e5       ldr r8, [r3]
  │││││││   0x000182ec      000058e3       cmp r8, 0
  ────────< 0x000182f0      0b00000a       beq 0x18324
  │││││││   0x000182f4      ecc29fe5       ldr ip, [0x000185e8]        ; [0x185e8:4]=0xffffd592
  │││││││   0x000182f8      000085e0       add r0, r5, r0
  │││││││   0x000182fc      cf1900e3       movw r1, 0x9cf
  │││││││   0x00018300      0620a0e3       mov r2, 6
  │││││││   0x00018304      0330a0e3       mov r3, 3
  │││││││   0x00018308      0cc085e0       add ip, r5, ip
  │││││││   0x0001830c      00c08de5       str ip, [sp]
  │││││││   0x00018310      00c0a0e3       mov ip, 0
  │││││││   0x00018314      0cc08de5       str ip, [sp, 0xc]
  ────────> 0x00018318      1262a0e3       mov r6, 0x20000001
  ────────> 0x0001831c      04c08de5       str ip, [sp, 4]
  ────────< 0x00018320      080100ea       b 0x18748
  ────────> 0x00018324      c0c29fe5       ldr ip, [0x000185ec]        ; [0x185ec:4]=0xfffe8538
  │││││││   0x00018328      00b085e0       add fp, r5, r0
  │││││││   0x0001832c      d81900e3       movw r1, 0x9d8
  │││││││   0x00018330      0620a0e3       mov r2, 6
  │││││││   0x00018334      0cc085e0       add ip, r5, ip
  │││││││   0x00018338      08c18de5       str ip, [sp, 0x108]
  │││││││   0x0001833c      acc29fe5       ldr ip, [0x000185f0]        ; [0x185f0:4]=0xfffe9e14
  │││││││   0x00018340      0230a0e3       mov r3, 2
  │││││││   0x00018344      0b00a0e1       mov r0, fp
  │││││││   0x00018348      04808de5       str r8, [sp, 4]
  │││││││   0x0001834c      0cc085e0       add ip, r5, ip
  │││││││   0x00018350      0cc18de5       str ip, [sp, 0x10c]
  │││││││   0x00018354      98c29fe5       ldr ip, [0x000185f4]        ; [0x185f4:4]=0xfffe9954
  │││││││   0x00018358      149802e3       movw sb, 0x2814             ; '\x14('
  │││││││   0x0001835c      08808de5       str r8, [sp, 8]
  │││││││   0x00018360      0cc085e0       add ip, r5, ip
  │││││││   0x00018364      10c18de5       str ip, [sp, 0x110]
  │││││││   0x00018368      88c29fe5       ldr ip, [0x000185f8]        ; [0x185f8:4]=0xffffd5dc
  │││││││   0x0001836c      0c808de5       str r8, [sp, 0xc]
  │││││││   0x00018370      0cc085e0       add ip, r5, ip
  │││││││   0x00018374      00c08de5       str ip, [sp]
  │││││││   0x00018378      fcb8ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x0001837c      180500e3       movw r0, 0x518
  │││││││   0x00018380      abb9ffeb       bl sym.imp.operator_new_unsigned_int_
  │││││││   0x00018384      421f8de2       add r1, sp, 0x108
  │││││││   0x00018388      0420a0e1       mov r2, r4
  │││││││   0x0001838c      00a0a0e1       mov sl, r0
  │││││││   0x00018390      aeb8ffeb       bl rsym._ZN12DeinterlacerC1EP21DeiniterlacerCallbackPv
  │││││││   0x00018394      00005ae3       cmp sl, 0
  │││││││   0x00018398      09a084e7       str sl, [r4, sb]
  ────────< 0x0001839c      0b00001a       bne 0x183d0
  │││││││   0x000183a0      54c29fe5       ldr ip, [0x000185fc]        ; [0x185fc:4]=0xffffd600
  │││││││   0x000183a4      0b00a0e1       mov r0, fp
  │││││││   0x000183a8      de1900e3       movw r1, 0x9de
  │││││││   0x000183ac      0620a0e3       mov r2, 6
  │││││││   0x000183b0      0cc085e0       add ip, r5, ip
  │││││││   0x000183b4      0330a0e3       mov r3, 3
  │││││││   0x000183b8      0ca08de5       str sl, [sp, 0xc]
  │││││││   0x000183bc      2262a0e3       mov r6, 0x20000002
  │││││││   0x000183c0      00c08de5       str ip, [sp]
  │││││││   0x000183c4      04a08de5       str sl, [sp, 4]
  │││││││   0x000183c8      08a08de5       str sl, [sp, 8]
  ────────< 0x000183cc      de0000ea       b 0x1874c
  ────────> 0x000183d0      306094e5       ldr r6, [r4, 0x30]
  │││││││   0x000183d4      b0c08de2       add ip, sp, 0xb0
  │││││││   0x000183d8      2c3094e5       ldr r3, [r4, 0x2c]
  │││││││   0x000183dc      0a00a0e1       mov r0, sl
  │││││││   0x000183e0      1cc08de5       str ip, [sp, 0x1c]
  │││││││   0x000183e4      1fe086e2       add lr, r6, 0x1f
  │││││││   0x000183e8      7fc083e2       add ip, r3, 0x7f
  │││││││   0x000183ec      0810a0e1       mov r1, r8
  │││││││   0x000183f0      1c209de5       ldr r2, [sp, 0x1c]
  │││││││   0x000183f4      7fc0cce3       bic ip, ip, 0x7f
  │││││││   0x000183f8      b4608de5       str r6, [sp, 0xb4]
  │││││││   0x000183fc      b8c08de5       str ip, [sp, 0xb8]
  │││││││   0x00018400      1fc0cee3       bic ip, lr, 0x1f
  │││││││   0x00018404      b0308de5       str r3, [sp, 0xb0]
  │││││││   0x00018408      bcc08de5       str ip, [sp, 0xbc]
  │││││││   0x0001840c      a1b8ffeb       bl rsym._ZN12Deinterlacer10SetPicSizeE16VdecDeintBufTypeP18VdecDeintFrameSize
  │││││││   0x00018410      120250e3       cmp r0, 0x20000001
  │││││││   0x00018414      0060a0e1       mov r6, r0
  ────────< 0x00018418      0b00001a       bne 0x1844c
  │││││││   0x0001841c      7c3094e5       ldr r3, [r4, 0x7c]
  │││││││   0x00018420      0110a0e3       mov r1, 1
  │││││││   0x00018424      80c094e5       ldr ip, [r4, 0x80]
  │││││││   0x00018428      1c209de5       ldr r2, [sp, 0x1c]
  │││││││   0x0001842c      090094e7       ldr r0, [r4, sb]
  │││││││   0x00018430      b8308de5       str r3, [sp, 0xb8]
  │││││││   0x00018434      bcc08de5       str ip, [sp, 0xbc]
  │││││││   0x00018438      96b8ffeb       bl rsym._ZN12Deinterlacer10SetPicSizeE16VdecDeintBufTypeP18VdecDeintFrameSize
  │││││││   0x0001843c      120250e3       cmp r0, 0x20000001
  │││││││   0x00018440      0060a0e1       mov r6, r0
  ────────< 0x00018444      0c00000a       beq 0x1847c
  ────────< 0x00018448      130000ea       b 0x1849c
  ────────> 0x0001844c      acc19fe5       ldr ip, [0x00018600]        ; [0x18600:4]=0xffffd61e
  │││││││   0x00018450      0b00a0e1       mov r0, fp
  │││││││   0x00018454      f51900e3       movw r1, 0x9f5
  │││││││   0x00018458      0620a0e3       mov r2, 6
  │││││││   0x0001845c      0330a0e3       mov r3, 3
  │││││││   0x00018460      0cc085e0       add ip, r5, ip
  │││││││   0x00018464      0c808de5       str r8, [sp, 0xc]
  │││││││   0x00018468      00c08de5       str ip, [sp]
  │││││││   0x0001846c      04808de5       str r8, [sp, 4]
  │││││││   0x00018470      08808de5       str r8, [sp, 8]
  │││││││   0x00018474      bdb8ffeb       bl sym.imp.MM_OutputDebugMessage
  ────────< 0x00018478      070000ea       b 0x1849c
  ────────> 0x0001847c      0810a0e1       mov r1, r8
  │││││││   0x00018480      542094e5       ldr r2, [r4, 0x54]
  │││││││   0x00018484      090094e7       ldr r0, [r4, sb]
  │││││││   0x00018488      fbb7ffeb       bl rsym._ZN12Deinterlacer12SetFrameRateE16VdecDeintBufTypem
  │││││││   0x0001848c      090094e7       ldr r0, [r4, sb]
  │││││││   0x00018490      0110a0e3       mov r1, 1
  │││││││   0x00018494      882094e5       ldr r2, [r4, 0x88]
  │││││││   0x00018498      f7b7ffeb       bl rsym._ZN12Deinterlacer12SetFrameRateE16VdecDeintBufTypem
  ────────> 0x0001849c      05ad8de2       add sl, sp, 0x140
  │││││││   0x000184a0      0400a0e1       mov r0, r4
  │││││││   0x000184a4      0a10a0e1       mov r1, sl
  │││││││   0x000184a8      0180a0e3       mov r8, 1
  │││││││   0x000184ac      5c308de2       add r3, sp, 0x5c
  │││││││   0x000184b0      5c808de5       str r8, [sp, 0x5c]
  │││││││   0x000184b4      44318de5       str r3, [sp, 0x144]
  │││││││   0x000184b8      40818de5       str r8, [sp, 0x140]
  │││││││   0x000184bc      7cb7ffeb       bl rsym._ZN17MMIVdecController14GetCustomParamEPv
  │││││││   0x000184c0      0a10a0e1       mov r1, sl
  │││││││   0x000184c4      0400a0e1       mov r0, r4
  │││││││   0x000184c8      0230a0e3       mov r3, 2
  │││││││   0x000184cc      90808de5       str r8, [sp, 0x90]
  │││││││   0x000184d0      40318de5       str r3, [sp, 0x140]
  │││││││   0x000184d4      90308de2       add r3, sp, 0x90
  │││││││   0x000184d8      44318de5       str r3, [sp, 0x144]
  │││││││   0x000184dc      74b7ffeb       bl rsym._ZN17MMIVdecController14GetCustomParamEPv
  ────────< 0x000184e0      050000ea       b 0x184fc
  ──────└─> 0x000184e4      18319fe5       ldr r3, [0x00018604]        ; [0x18604:4]=0x7f100fff
  ││││││    0x000184e8      18209de5       ldr r2, [sp, 0x18]
  ││││││    0x000184ec      030052e1       cmp r2, r3
  ││││││┌─< 0x000184f0      0300009a       bls 0x18504
  │││││││   0x000184f4      86b7ffeb       bl rsym._ZN17MMIVdecController17SetOmxDevExtParamEP19MMI_OmxParamCmdType
  ────────> 0x000184f8      0060a0e1       mov r6, r0
  ────────> 0x000184fc      f20256e3       cmp r6, 0x2000000f          ; '\x0f'
  ────────< 0x00018500      d600001a       bne 0x18860
  ││││││└─> 0x00018504      fc309fe5       ldr r3, [0x00018608]        ; [0x18608:4]=0x6f100001
  ││││││    0x00018508      18a09de5       ldr sl, [sp, 0x18]
  ││││││    0x0001850c      03005ae1       cmp sl, r3
  ││││││┌─< 0x00018510      0300000a       beq 0x18524
  │││││││   0x00018514      f0309fe5       ldr r3, [0x0001860c]        ; [0x1860c:4]=0x6f600003
  │││││││   0x00018518      03005ae1       cmp sl, r3
  ────────< 0x0001851c      e300001a       bne 0x188b0
  ────────< 0x00018520      420000ea       b 0x18630
  ││││││└─> 0x00018524      043097e5       ldr r3, [r7, 4]
  ││││││    0x00018528      000053e3       cmp r3, 0
  ││││││┌─< 0x0001852c      df00000a       beq 0x188b0
  │││││││   0x00018530      002093e5       ldr r2, [r3]
  │││││││   0x00018534      140052e3       cmp r2, 0x14
  ────────< 0x00018538      dc00001a       bne 0x188b0
  │││││││   0x0001853c      0c1093e5       ldr r1, [r3, 0xc]
  │││││││   0x00018540      c8209fe5       ldr r2, [0x00018610]        ; [0x18610:4]=0x6000010
  │││││││   0x00018544      020051e1       cmp r1, r2
  ────────< 0x00018548      87feff1a       bne 0x17f6c
  │││││││   0x0001854c      102093e5       ldr r2, [r3, 0x10]
  │││││││   0x00018550      f83702e3       movw r3, 0x27f8
  │││││││   0x00018554      f262a0e3       mov r6, 0x2000000f          ; '\x0f'
  │││││││   0x00018558      032084e7       str r2, [r4, r3]
  ────────< 0x0001855c      bf0000ea       b 0x18860
  │││││││   0x00018560      90db0000       muleq r0, r0, fp
  │││││││   0x00018564      b4030000       strheq r0, [r0], -r4
  │││││││   0x00018568      0caaffff       invalid
  │││││││   0x0001856c      68d1ffff       invalid
  │││││││   0x00018570      88d1ffff       invalid
  │││││││   0x00018574      d8fdffff       invalid
  │││││││   0x00018578      b1d1ffff       invalid
  │││││││   0x0001857c      e7d1ffff       invalid
  │││││││   0x00018580      0fd2ffff       invalid
  │││││││   0x00018584      37d2ffff       invalid
  │││││││   0x00018588      01000002       andeq r0, r0, 1
  │││││││   0x0001858c      68d2ffff       invalid
  │││││││   0x00018590      0c00107f       svcvc 0x10000c
  │││││││   0x00018594      0200107f       svcvc 0x100002
  │││││││   0x00018598      9ed2ffff       invalid
  │││││││   0x0001859c      c0d2ffff       invalid
  │││││││   0x000185a0      e2d2ffff       invalid
  │││││││   0x000185a4      0100107f       svcvc 0x100001
  │││││││   0x000185a8      1ad3ffff       invalid
  │││││││   0x000185ac      51465245       ldrbmi r4, [r2, -0x651]
  │││││││   0x000185b0      45d3ffff       invalid
  │││││││   0x000185b4      5bd3ffff       invalid
  │││││││   0x000185b8      95d3ffff       invalid
  │││││││   0x000185bc      d1d3ffff       invalid
  │││││││   0x000185c0      16d4ffff       invalid
  │││││││   0x000185c4      47d4ffff       invalid
  │││││││   0x000185c8      7fd4ffff       invalid
  │││││││   0x000185cc      aad4ffff       invalid
  │││││││   0x000185d0      cdd4ffff       invalid
  │││││││   0x000185d4      f7d4ffff       invalid
  │││││││   0x000185d8      91d0ffff       invalid
  │││││││   0x000185dc      19d5ffff       invalid
  │││││││   0x000185e0      57d5ffff       invalid
  │││││││   0x000185e4      b8030000       strheq r0, [r0], -r8
  │││││││   0x000185e8      92d5ffff       invalid
  │││││││   0x000185ec      3885feff       invalid
  │││││││   0x000185f0      149efeff       invalid
  │││││││   0x000185f4      5499feff       invalid
  │││││││   0x000185f8      dcd5ffff       invalid
  │││││││   0x000185fc      00d6ffff       invalid
  │││││││   0x00018600      1ed6ffff       invalid
  │││││││   0x00018604      ff0f107f       svcvc 0x100fff
  │││││││   0x00018608      0100106f       svcvs 0x100001
  │││││││   0x0001860c      0300606f       svcvs 0x600003
  │││││││   0x00018610      10000006       invalid
  │││││││   0x00018614      e5d0ffff       invalid
  │││││││   0x00018618      48d6ffff       invalid
  │││││││   0x0001861c      5ed6ffff       invalid
  │││││││   0x00018620      0caaffff       invalid
  │││││││   0x00018624      8fd6ffff       invalid
  │││││││   0x00018628      10000020       andhs r0, r0, r0, lsl r0
  │││││││   0x0001862c      b4030000       strheq r0, [r0], -r4
  ────────> 0x00018630      203702e3       movw r3, 0x2720             ; ' \''
  │││││││   0x00018634      036094e7       ldr r6, [r4, r3]
  │││││││   0x00018638      000056e3       cmp r6, 0
  ────────< 0x0001863c      1d00001a       bne 0x186b8
  │││││││   0x00018640      262e8de2       add r2, sp, 0x260
  │││││││   0x00018644      0e30a0e3       mov r3, 0xe
  │││││││   0x00018648      27cc84e2       add ip, r4, 0x2700
  │││││││   0x0001864c      0210a0e3       mov r1, 2
  │││││││   0x00018650      283122e5       str r3, [r2, -0x128]!
  │││││││   0x00018654      d03602e3       movw r3, 0x26d0
  │││││││   0x00018658      20c08ce2       add ip, ip, 0x20
  │││││││   0x0001865c      030094e7       ldr r0, [r4, r3]
  │││││││   0x00018660      0830a0e3       mov r3, 8
  │││││││   0x00018664      00c08de5       str ip, [sp]
  │││││││   0x00018668      01c0a0e3       mov ip, 1
  │││││││   0x0001866c      08608de5       str r6, [sp, 8]
  │││││││   0x00018670      3cc18de5       str ip, [sp, 0x13c]
  │││││││   0x00018674      03c08ce2       add ip, ip, 3
  │││││││   0x00018678      04c08de5       str ip, [sp, 4]
  │││││││   0x0001867c      29b8ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018680      008050e2       subs r8, r0, 0
  ────────< 0x00018684      0b00000a       beq 0x186b8
  │││││││   0x00018688      70001fe5       ldr r0, [0x00018620]        ; [0x18620:4]=0xffffaa0c
  │││││││   0x0001868c      0620a0e3       mov r2, 6
  │││││││   0x00018690      84c01fe5       ldr ip, [0x00018614]        ; [0x18614:4]=0xffffd0e5
  │││││││   0x00018694      0330a0e3       mov r3, 3
  │││││││   0x00018698      000085e0       add r0, r5, r0
  │││││││   0x0001869c      4f1a00e3       movw r1, 0xa4f              ; 'O\n'
  │││││││   0x000186a0      0cc085e0       add ip, r5, ip
  │││││││   0x000186a4      0c608de5       str r6, [sp, 0xc]
  │││││││   0x000186a8      00c08de5       str ip, [sp]
  │││││││   0x000186ac      04808de5       str r8, [sp, 4]
  │││││││   0x000186b0      08608de5       str r6, [sp, 8]
  ────────< 0x000186b4      440000ea       b 0x187cc
  ────────> 0x000186b8      043097e5       ldr r3, [r7, 4]
  │││││││   0x000186bc      0cc093e5       ldr ip, [r3, 0xc]
  │││││││   0x000186c0      01005ce3       cmp ip, 1
  │││││││   0x000186c4      0020a003       moveq r2, 0
  ────────< 0x000186c8      0800000a       beq 0x186f0
  │││││││   0x000186cc      04005ce3       cmp ip, 4
  │││││││   0x000186d0      0120a003       moveq r2, 1
  ────────< 0x000186d4      0500000a       beq 0x186f0
  │││││││   0x000186d8      08005ce3       cmp ip, 8
  │││││││   0x000186dc      0220a003       moveq r2, 2
  ────────< 0x000186e0      0200000a       beq 0x186f0
  │││││││   0x000186e4      10005ce3       cmp ip, 0x10
  ────────< 0x000186e8      0a00001a       bne 0x18718
  │││││││   0x000186ec      0420a0e3       mov r2, 4
  ────────> 0x000186f0      0cc093e5       ldr ip, [r3, 0xc]
  │││││││   0x000186f4      203702e3       movw r3, 0x2720             ; ' \''
  │││││││   0x000186f8      247702e3       movw r7, 0x2724             ; '$\''
  │││││││   0x000186fc      9c2084e5       str r2, [r4, 0x9c]
  │││││││   0x00018700      033094e7       ldr r3, [r4, r3]
  │││││││   0x00018704      07c084e7       str ip, [r4, r7]
  │││││││   0x00018708      03001ce1       tst ip, r3
  │││││││   0x0001870c      f262a003       moveq r6, 0x2000000f
  ────────< 0x00018710      5200000a       beq 0x18860
  ────────< 0x00018714      0e0000ea       b 0x18754
  ────────> 0x00018718      00011fe5       ldr r0, [0x00018620]        ; [0x18620:4]=0xffffaa0c
  │││││││   0x0001871c      691a00e3       movw r1, 0xa69              ; 'i\n'
  │││││││   0x00018720      04c08de5       str ip, [sp, 4]
  │││││││   0x00018724      0620a0e3       mov r2, 6
  │││││││   0x00018728      18c11fe5       ldr ip, [0x00018618]        ; [0x18618:4]=0xffffd648
  │││││││   0x0001872c      000085e0       add r0, r5, r0
  │││││││   0x00018730      0330a0e3       mov r3, 3
  │││││││   0x00018734      4262a0e3       mov r6, 0x20000004
  │││││││   0x00018738      0cc085e0       add ip, r5, ip
  │││││││   0x0001873c      00c08de5       str ip, [sp]
  │││││││   0x00018740      00c0a0e3       mov ip, 0
  │││││││   0x00018744      0cc08de5       str ip, [sp, 0xc]
  ─────└──> 0x00018748      08c08de5       str ip, [sp, 8]
  ────────> 0x0001874c      07b8ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││┌──< 0x00018750      420000ea       b 0x18860
  ────────> 0x00018754      d03602e3       movw r3, 0x26d0
  │││││││   0x00018758      02e0a0e3       mov lr, 2
  │││││││   0x0001875c      0e10a0e1       mov r1, lr
  │││││││   0x00018760      fc208de2       add r2, sp, 0xfc
  │││││││   0x00018764      030094e7       ldr r0, [r4, r3]
  │││││││   0x00018768      0c30a0e3       mov r3, 0xc
  │││││││   0x0001876c      0060a0e3       mov r6, 0
  │││││││   0x00018770      04c18de5       str ip, [sp, 0x104]
  │││││││   0x00018774      00608de5       str r6, [sp]
  │││││││   0x00018778      0dc0a0e3       mov ip, 0xd
  │││││││   0x0001877c      04608de5       str r6, [sp, 4]
  │││││││   0x00018780      08608de5       str r6, [sp, 8]
  │││││││   0x00018784      00e18de5       str lr, [sp, 0x100]
  │││││││   0x00018788      fcc08de5       str ip, [sp, 0xfc]
  │││││││   0x0001878c      e5b7ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018790      008050e2       subs r8, r0, 0
  │││││││   0x00018794      12628602       addeq r6, r6, 0x20000001
  ────────< 0x00018798      3000000a       beq 0x18860
  │││││││   0x0001879c      07c094e7       ldr ip, [r4, r7]
  │││││││   0x000187a0      0620a0e3       mov r2, 6
  │││││││   0x000187a4      8c011fe5       ldr r0, [0x00018620]        ; [0x18620:4]=0xffffaa0c
  │││││││   0x000187a8      0330a0e3       mov r3, 3
  │││││││   0x000187ac      7c1a00e3       movw r1, 0xa7c              ; '|\n'
  │││││││   0x000187b0      0c608de5       str r6, [sp, 0xc]
  │││││││   0x000187b4      04c08de5       str ip, [sp, 4]
  │││││││   0x000187b8      000085e0       add r0, r5, r0
  │││││││   0x000187bc      a8c11fe5       ldr ip, [0x0001861c]        ; [0x1861c:4]=0xffffd65e
  │││││││   0x000187c0      08808de5       str r8, [sp, 8]
  │││││││   0x000187c4      0cc085e0       add ip, r5, ip
  │││││││   0x000187c8      00c08de5       str ip, [sp]
  ────────> 0x000187cc      e7b7ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x000187d0      0400a0e1       mov r0, r4
  │││││││   0x000187d4      0810a0e1       mov r1, r8
  ────────< 0x000187d8      050000ea       b 0x187f4
  ────────> 0x000187dc      020051e3       cmp r1, 2
  │││││││   0x000187e0      15005113       cmpne r1, 0x15
  │││││││   0x000187e4      00c0a013       movne ip, 0
  │││││││   0x000187e8      01c0a003       moveq ip, 1
  ────────< 0x000187ec      0300001a       bne 0x18800
  │││││││   0x000187f0      0400a0e1       mov r0, r4
  ────────> 0x000187f4      1bb6ffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  │││││││   0x000187f8      0060a0e1       mov r6, r0
  ────────< 0x000187fc      170000ea       b 0x18860
  ────────> 0x00018800      e8011fe5       ldr r0, [0x00018620]        ; [0x18620:4]=0xffffaa0c
  │││││││   0x00018804      0730a0e1       mov r3, r7
  │││││││   0x00018808      ece11fe5       ldr lr, [0x00018624]        ; [0x18624:4]=0xffffd68f
  │││││││   0x0001880c      9d1a00e3       movw r1, 0xa9d
  │││││││   0x00018810      000085e0       add r0, r5, r0
  │││││││   0x00018814      0620a0e3       mov r2, 6
  │││││││   0x00018818      0ee085e0       add lr, r5, lr
  │││││││   0x0001881c      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00018820      00e08de5       str lr, [sp]
  │││││││   0x00018824      04c08de5       str ip, [sp, 4]
  │││││││   0x00018828      08c08de5       str ip, [sp, 8]
  │││││││   0x0001882c      cfb7ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018830      10621fe5       ldr r6, [0x00018628]        ; [0x18628:4]=0x20000010
  ││└─────> 0x00018834      103094e5       ldr r3, [r4, 0x10]
  ││ ││││   0x00018838      010053e3       cmp r3, 1
  ││ ││││   0x0001883c      010b5313       cmpne r3, 0x400
  ││┌─────< 0x00018840      0600001a       bne 0x18860
  │││││││   0x00018844      f57702e3       movw r7, 0x27f5
  │││││││   0x00018848      0400a0e1       mov r0, r4
  │││││││   0x0001884c      0710d4e7       ldrb r1, [r4, r7]
  │││││││   0x00018850      fdb6ffeb       bl rsym._ZN17MMIVdecController12SetDmxStatusEb
  │││││││   0x00018854      120250e3       cmp r0, 0x20000001
  │││││││   0x00018858      0030a013       movne r3, 0
  │││││││   0x0001885c      0730c417       strbne r3, [r4, r7]
  ─└└──└──> 0x00018860      3c321fe5       ldr r3, [0x0001862c]        ; [0x1862c:4]=0x3b4
  │  ││ │   0x00018864      0600a0e1       mov r0, r6
  │  ││ │   0x00018868      5c229de5       ldr r2, [sp, 0x25c]
  │  ││ │   0x0001886c      033095e7       ldr r3, [r5, r3]
  │  ││ │   0x00018870      003093e5       ldr r3, [r3]
  │  ││ │   0x00018874      030052e1       cmp r2, r3
  │  ││┌──< 0x00018878      0f00000a       beq 0x188bc
  │ ┌─────< 0x0001887c      0d0000ea       b 0x188b8
  ────────> 0x00018880      e8a702e3       movw sl, 0x27e8
  │ │││││   0x00018884      0510a0e3       mov r1, 5
  │ │││││   0x00018888      102088e2       add r2, r8, 0x10
  │ │││││   0x0001888c      0a3094e7       ldr r3, [r4, sl]
  │ │││││   0x00018890      0300a0e1       mov r0, r3
  │ │││││   0x00018894      003093e5       ldr r3, [r3]
  │ │││││   0x00018898      33ff2fe1       blx r3
  │ │││││   0x0001889c      006050e2       subs r6, r0, 0
  ────────< 0x000188a0      7afdff1a       bne 0x17e90
  ────────< 0x000188a4      86fdffea       b 0x17ec4
  ───└────> 0x000188a8      1262a0e3       mov r6, 0x20000001
  ────────< 0x000188ac      ebffffea       b 0x18860
  └───└─└─> 0x000188b0      4262a0e3       mov r6, 0x20000004
  ────────< 0x000188b4      e9ffffea       b 0x18860
    └─────> 0x000188b8      e0b6ffeb       bl sym.imp.__stack_chk_fail
       └──> 0x000188bc      99df8de2       add sp, sp, 0x264
            0x000188c0      f08fbde8       pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}
