            ;-- reloc.ScreenStreamProcessData:
            ;-- ScreenStreamProcessData:
            0x00084fec      ec339fe5       ldr r3, [0x000853e0]        ; [0x853e0:4]=0x33d44; RELOC 32 ScreenStreamProcessData @ 0x00084fec
            0x00084ff0      ecc39fe5       ldr ip, [0x000853e4]        ; [0x853e4:4]=0x20b0
            0x00084ff4      03308fe0       add r3, pc, r3
            0x00084ff8      f04f2de9       push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
            0x00084ffc      028b2ded       vpush {d8}
            0x00085000      0040a0e1       mov r4, r0
            0x00085004      2cd04de2       sub sp, sp, 0x2c
            0x00085008      0c0083e0       add r0, r3, ip
            0x0008500c      080080e2       add r0, r0, 8
            0x00085010      01a0a0e1       mov sl, r1
            0x00085014      10308de5       str r3, [sp, 0x10]
            0x00085018      0270a0e1       mov r7, r2
            0x0008501c      14c08de5       str ip, [sp, 0x14]
            0x00085020      9151feeb       bl sym.imp.pthread_mutex_lock
            0x00085024      000054e3       cmp r4, 0
            0x00085028      0030a0e3       mov r3, 0
            0x0008502c      1c308de5       str r3, [sp, 0x1c]
            0x00085030      24308de5       str r3, [sp, 0x24]
            0x00085034      20308de5       str r3, [sp, 0x20]
        ┌─< 0x00085038      c300000a       beq 0x8534c
        │   0x0008503c      3c6094e5       ldr r6, [r4, 0x3c]
        │   0x00085040      03005ae1       cmp sl, r3
        │   0x00085044      03005611       cmpne r6, r3
        │   0x00085048      0060a013       movne r6, 0
        │   0x0008504c      0160a003       moveq r6, 1
       ┌──< 0x00085050      bd00000a       beq 0x8534c
       ││   0x00085054      32c0d4e5       ldrb ip, [r4, 0x32]
       ││   0x00085058      03005ce1       cmp ip, r3
      ┌───< 0x0008505c      9200000a       beq 0x852ac
      │││   0x00085060      000057e3       cmp r7, 0
     ┌────< 0x00085064      7900000a       beq 0x85250
     ││││   0x00085068      d805cde1       ldrd r0, r1, [sp, 0x58]
     ││││   0x0008506c      7251feeb       bl rsym.UpTicksToMicroseconds
     ││││   0x00085070      3130d4e5       ldrb r3, [r4, 0x31]
     ││││   0x00085074      000053e3       cmp r3, 0
     ││││   0x00085078      180b41ec       vmov d8, r0, r1
    ┌─────< 0x0008507c      6100001a       bne 0x85208
    │││││   0x00085080      3030d4e5       ldrb r3, [r4, 0x30]
    │││││   0x00085084      000053e3       cmp r3, 0
   ┌──────< 0x00085088      5100000a       beq 0x851d4
  ┌───────> 0x0008508c      54639fe5       ldr r6, [0x000853e8]        ; [0x853e8:4]=0xffff5754
  ╎││││││   0x00085090      07708ae0       add r7, sl, r7
  ╎││││││   0x00085094      10209de5       ldr r2, [sp, 0x10]
  ╎││││││   0x00085098      24308de2       add r3, sp, 0x24
  ╎││││││   0x0008509c      24a08de5       str sl, [sp, 0x24]
  ╎││││││   0x000850a0      0a00a0e1       mov r0, sl
  ╎││││││   0x000850a4      0050a0e3       mov r5, 0
  ╎││││││   0x000850a8      066082e0       add r6, r2, r6
  ╎││││││   0x000850ac      20908de2       add sb, sp, 0x20
  ╎││││││   0x000850b0      1cb08de2       add fp, sp, 0x1c
  ╎││││││   0x000850b4      0780a0e1       mov r8, r7
  ╎││││││   0x000850b8      03a0a0e1       mov sl, r3
  ────────< 0x000850bc      0a0000ea       b 0x850ec
  ────────> 0x000850c0      3c0094e5       ldr r0, [r4, 0x3c]
  ╎││││││   0x000850c4      050080e0       add r0, r0, r5
  ╎││││││   0x000850c8      634ffeeb       bl sym.imp.memcpy
  ╎││││││   0x000850cc      3c0094e5       ldr r0, [r4, 0x3c]
  ╎││││││   0x000850d0      20109de5       ldr r1, [sp, 0x20]
  ╎││││││   0x000850d4      070080e0       add r0, r0, r7
  ╎││││││   0x000850d8      1c209de5       ldr r2, [sp, 0x1c]
  ╎││││││   0x000850dc      5e4ffeeb       bl sym.imp.memcpy
  ╎││││││   0x000850e0      1c509de5       ldr r5, [sp, 0x1c]
  ╎││││││   0x000850e4      24009de5       ldr r0, [sp, 0x24]
  ╎││││││   0x000850e8      055087e0       add r5, r7, r5
  ────────> 0x000850ec      342094e5       ldr r2, [r4, 0x34]
  ╎││││││   0x000850f0      0810a0e1       mov r1, r8
  ╎││││││   0x000850f4      0930a0e1       mov r3, sb
  ╎││││││   0x000850f8      00b08de5       str fp, [sp]
  ╎││││││   0x000850fc      04a08de5       str sl, [sp, 4]
  ╎││││││   0x00085100      f359feeb       bl rsym.H264GetNextNALUnit
  ╎││││││   0x00085104      0420a0e3       mov r2, 4
  ╎││││││   0x00085108      0610a0e1       mov r1, r6
  ╎││││││   0x0008510c      027085e0       add r7, r5, r2
  ╎││││││   0x00085110      000050e3       cmp r0, 0
  ────────< 0x00085114      e9ffff0a       beq 0x850c0
  ╎││││││   0x00085118      3c1094e5       ldr r1, [r4, 0x3c]
  ╎││││││   0x0008511c      0520a0e1       mov r2, r5
  ╎││││││   0x00085120      0400a0e1       mov r0, r4
  ╎││││││   0x00085124      0030a0e3       mov r3, 0
  ╎││││││   0x00085128      008b8ded       vstr d8, [sp]
  ╎││││││   0x0008512c      08308de5       str r3, [sp, 8]
  ╎││││││   0x00085130      8afdffeb       bl 0x84760
  ╎││││││   0x00085134      0050a0e1       mov r5, r0
  ────────> 0x00085138      64309de5       ldr r3, [sp, 0x64]
  ╎││││││   0x0008513c      000053e3       cmp r3, 0
  ────────< 0x00085140      0100000a       beq 0x8514c
  ╎││││││   0x00085144      68009de5       ldr r0, [sp, 0x68]
  ╎││││││   0x00085148      33ff2fe1       blx r3
  ────────> 0x0008514c      000055e3       cmp r5, 0
  ────────< 0x00085150      1600000a       beq 0x851b0
  ╎││││││   0x00085154      90629fe5       ldr r6, [0x000853ec]        ; [0x853ec:4]=0x1170
  ╎││││││   0x00085158      10c09de5       ldr ip, [sp, 0x10]
  ╎││││││   0x0008515c      06009ce7       ldr r0, [ip, r6]
  ╎││││││   0x00085160      003090e5       ldr r3, [r0]
  ╎││││││   0x00085164      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x00085168      100000ca       bgt 0x851b0
  ╎││││││   0x0008516c      010073e3       cmn r3, 1
  ────────< 0x00085170      3100000a       beq 0x8523c
  ────────> 0x00085174      000054e3       cmp r4, 0
  ╎││││││   0x00085178      10e09de5       ldr lr, [sp, 0x10]
  ╎││││││   0x0008517c      6c129fe5       ldr r1, [0x000853f0]        ; [0x853f0:4]=0xffff5884
  ╎││││││   0x00085180      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎││││││   0x00085184      3240d415       ldrbne r4, [r4, 0x32]
  ╎││││││   0x00085188      64329fe5       ldr r3, [0x000853f4]        ; [0x853f4:4]=0xffff5bb0
  ╎││││││   0x0008518c      01108ee0       add r1, lr, r1
  ╎││││││   0x00085190      60c29fe5       ldr ip, [0x000853f8]        ; [0x853f8:4]=0xffff592c
  ╎││││││   0x00085194      06009ee7       ldr r0, [lr, r6]
  ╎││││││   0x00085198      03308ee0       add r3, lr, r3
  ╎││││││   0x0008519c      0cc08ee0       add ip, lr, ip
  ╎││││││   0x000851a0      08408de5       str r4, [sp, 8]
  ╎││││││   0x000851a4      00c08de5       str ip, [sp]
  ╎││││││   0x000851a8      04508de5       str r5, [sp, 4]
  ╎││││││   0x000851ac      3b59feeb       bl rsym.LogPrintF
  ────────> 0x000851b0      10209de5       ldr r2, [sp, 0x10]
  ╎││││││   0x000851b4      14309de5       ldr r3, [sp, 0x14]
  ╎││││││   0x000851b8      030082e0       add r0, r2, r3
  ╎││││││   0x000851bc      080080e2       add r0, r0, 8
  ╎││││││   0x000851c0      b957feeb       bl sym.imp.pthread_mutex_unlock
  ╎││││││   0x000851c4      0500a0e1       mov r0, r5
  ╎││││││   0x000851c8      2cd08de2       add sp, sp, 0x2c
  ╎││││││   0x000851cc      028bbdec       vpop {d8}
  ╎││││││   0x000851d0      f08fbde8       pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}
  ╎└──────> 0x000851d4      281094e5       ldr r1, [r4, 0x28]
  ╎ │││││   0x000851d8      000051e3       cmp r1, 0
  ╎┌──────< 0x000851dc      5d00000a       beq 0x85358
  ╎││││││   0x000851e0      2c2094e5       ldr r2, [r4, 0x2c]
  ╎││││││   0x000851e4      0400a0e1       mov r0, r4
  ╎││││││   0x000851e8      0150a0e3       mov r5, 1
  ╎││││││   0x000851ec      008b8ded       vstr d8, [sp]
  ╎││││││   0x000851f0      08508de5       str r5, [sp, 8]
  ╎││││││   0x000851f4      59fdffeb       bl 0x84760
  ╎││││││   0x000851f8      3050c4e5       strb r5, [r4, 0x30]
  ╎││││││   0x000851fc      005050e2       subs r5, r0, 0
  └───────< 0x00085200      a1ffff0a       beq 0x8508c
  ────────< 0x00085204      cbffffea       b 0x85138
   │└─────> 0x00085208      ec819fe5       ldr r8, [0x000853fc]        ; [0x853fc:4]=0x1ca8
   │ ││││   0x0008520c      008b8ded       vstr d8, [sp]
   │ ││││   0x00085210      10c09de5       ldr ip, [sp, 0x10]
   │ ││││   0x00085214      0a10a0e1       mov r1, sl
   │ ││││   0x00085218      0720a0e1       mov r2, r7
   │ ││││   0x0008521c      0400a0e1       mov r0, r4
   │ ││││   0x00085220      0830dce7       ldrb r3, [ip, r8]
   │ ││││   0x00085224      08308de5       str r3, [sp, 8]
   │ ││││   0x00085228      4cfdffeb       bl 0x84760
   │ ││││   0x0008522c      10e09de5       ldr lr, [sp, 0x10]
   │ ││││   0x00085230      0860cee7       strb r6, [lr, r8]
   │ ││││   0x00085234      0050a0e1       mov r5, r0
  ────────< 0x00085238      beffffea       b 0x85138
  ────────> 0x0008523c      5a10a0e3       mov r1, 0x5a                ; 'Z'
   │ ││││   0x00085240      d458feeb       bl rsym.__LogCategory_Initialize
   │ ││││   0x00085244      000050e3       cmp r0, 0
  ────────< 0x00085248      d8ffff0a       beq 0x851b0
  ────────< 0x0008524c      c8ffffea       b 0x85174
   │ └────> 0x00085250      94619fe5       ldr r6, [0x000853ec]        ; [0x853ec:4]=0x1170
   │  │││   0x00085254      10209de5       ldr r2, [sp, 0x10]
   │  │││   0x00085258      060092e7       ldr r0, [r2, r6]
   │  │││   0x0008525c      003090e5       ldr r3, [r0]
   │  │││   0x00085260      3c0053e3       cmp r3, 0x3c                ; '<'
   │ ┌────< 0x00085264      2d0000ca       bgt 0x85320
   │ ││││   0x00085268      010073e3       cmn r3, 1
   │┌─────< 0x0008526c      2700000a       beq 0x85310
  ┌───────> 0x00085270      10309de5       ldr r3, [sp, 0x10]
  ╎││││││   0x00085274      3c20a0e3       mov r2, 0x3c                ; '<'
  ╎││││││   0x00085278      70119fe5       ldr r1, [0x000853f0]        ; [0x853f0:4]=0xffff5884
  ╎││││││   0x0008527c      0050a0e3       mov r5, 0
  ╎││││││   0x00085280      10c09de5       ldr ip, [sp, 0x10]
  ╎││││││   0x00085284      060093e7       ldr r0, [r3, r6]
  ╎││││││   0x00085288      011083e0       add r1, r3, r1
  ╎││││││   0x0008528c      6c319fe5       ldr r3, [0x00085400]        ; [0x85400:4]=0xffff5b58
  ╎││││││   0x00085290      10e09de5       ldr lr, [sp, 0x10]
  ╎││││││   0x00085294      03308ce0       add r3, ip, r3
  ╎││││││   0x00085298      58c19fe5       ldr ip, [0x000853f8]        ; [0x853f8:4]=0xffff592c
  ╎││││││   0x0008529c      0cc08ee0       add ip, lr, ip
  ╎││││││   0x000852a0      00c08de5       str ip, [sp]
  ╎││││││   0x000852a4      fd58feeb       bl rsym.LogPrintF
  ────────< 0x000852a8      a2ffffea       b 0x85138
  ╎│││└───> 0x000852ac      38619fe5       ldr r6, [0x000853ec]        ; [0x853ec:4]=0x1170
  ╎│││ ││   0x000852b0      10e09de5       ldr lr, [sp, 0x10]
  ╎│││ ││   0x000852b4      06009ee7       ldr r0, [lr, r6]
  ╎│││ ││   0x000852b8      003090e5       ldr r3, [r0]
  ╎│││ ││   0x000852bc      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ╎│││┌───< 0x000852c0      180000ca       bgt 0x85328
  ╎││││││   0x000852c4      010073e3       cmn r3, 1
  ────────< 0x000852c8      1900000a       beq 0x85334
  ────────> 0x000852cc      10309de5       ldr r3, [sp, 0x10]
  ╎││││││   0x000852d0      a7550ee3       movw r5, 0xe5a7
  ╎││││││   0x000852d4      14119fe5       ldr r1, [0x000853f0]        ; [0x853f0:4]=0xffff5884
  ╎││││││   0x000852d8      ff5f4fe3       movt r5, 0xffff
  ╎││││││   0x000852dc      10209de5       ldr r2, [sp, 0x10]
  ╎││││││   0x000852e0      10e09de5       ldr lr, [sp, 0x10]
  ╎││││││   0x000852e4      011083e0       add r1, r3, r1
  ╎││││││   0x000852e8      14319fe5       ldr r3, [0x00085404]        ; [0x85404:4]=0xffff5b14
  ╎││││││   0x000852ec      04c08de5       str ip, [sp, 4]
  ╎││││││   0x000852f0      00c19fe5       ldr ip, [0x000853f8]        ; [0x853f8:4]=0xffff592c
  ╎││││││   0x000852f4      03308ee0       add r3, lr, r3
  ╎││││││   0x000852f8      060092e7       ldr r0, [r2, r6]
  ╎││││││   0x000852fc      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎││││││   0x00085300      0cc08ee0       add ip, lr, ip
  ╎││││││   0x00085304      00c08de5       str ip, [sp]
  ╎││││││   0x00085308      e458feeb       bl rsym.LogPrintF
  ────────< 0x0008530c      89ffffea       b 0x85138
  ╎│└─────> 0x00085310      3c10a0e3       mov r1, 0x3c                ; '<'
  ╎│ ││││   0x00085314      9f58feeb       bl rsym.__LogCategory_Initialize
  ╎│ ││││   0x00085318      000050e3       cmp r0, 0
  └───────< 0x0008531c      d3ffff1a       bne 0x85270
   │ └────> 0x00085320      0050a0e3       mov r5, 0
  ────────< 0x00085324      83ffffea       b 0x85138
   │ ┌└───> 0x00085328      a7550ee3       movw r5, 0xe5a7
   │ ╎ ││   0x0008532c      ff5f4fe3       movt r5, 0xffff
  ────────< 0x00085330      80ffffea       b 0x85138
  ────────> 0x00085334      5a10a0e3       mov r1, 0x5a                ; 'Z'
   │ ╎ ││   0x00085338      9658feeb       bl rsym.__LogCategory_Initialize
   │ ╎ ││   0x0008533c      000050e3       cmp r0, 0
   │ └────< 0x00085340      f8ffff0a       beq 0x85328
   │   ││   0x00085344      32c0d4e5       ldrb ip, [r4, 0x32]
  ────────< 0x00085348      dfffffea       b 0x852cc
   │   └└─> 0x0008534c      ac550ee3       movw r5, 0xe5ac
   │        0x00085350      ff5f4fe3       movt r5, 0xffff
  ────────< 0x00085354      77ffffea       b 0x85138
   └──────> 0x00085358      8c609fe5       ldr r6, [0x000853ec]        ; [0x853ec:4]=0x1170
            0x0008535c      10209de5       ldr r2, [sp, 0x10]
            0x00085360      060092e7       ldr r0, [r2, r6]
            0x00085364      003090e5       ldr r3, [r0]
            0x00085368      5a0053e3       cmp r3, 0x5a                ; 'Z'
        ┌─< 0x0008536c      120000ca       bgt 0x853bc
        │   0x00085370      010073e3       cmn r3, 1
        │   0x00085374      01c0a011       movne ip, r1
       ┌──< 0x00085378      1200000a       beq 0x853c8
      ┌───> 0x0008537c      10309de5       ldr r3, [sp, 0x10]
      ╎││   0x00085380      5a20a0e3       mov r2, 0x5a                ; 'Z'
      ╎││   0x00085384      64109fe5       ldr r1, [0x000853f0]        ; [0x853f0:4]=0xffff5884
      ╎││   0x00085388      b0550ee3       movw r5, 0xe5b0
      ╎││   0x0008538c      10e09de5       ldr lr, [sp, 0x10]
      ╎││   0x00085390      ff5f4fe3       movt r5, 0xffff
      ╎││   0x00085394      060093e7       ldr r0, [r3, r6]
      ╎││   0x00085398      011083e0       add r1, r3, r1
      ╎││   0x0008539c      64309fe5       ldr r3, [0x00085408]        ; [0x85408:4]=0xffff5b7c
      ╎││   0x000853a0      04c08de5       str ip, [sp, 4]
      ╎││   0x000853a4      4cc09fe5       ldr ip, [0x000853f8]        ; [0x853f8:4]=0xffff592c
      ╎││   0x000853a8      03308ee0       add r3, lr, r3
      ╎││   0x000853ac      0cc08ee0       add ip, lr, ip
      ╎││   0x000853b0      00c08de5       str ip, [sp]
      ╎││   0x000853b4      b958feeb       bl rsym.LogPrintF
  ────────< 0x000853b8      5effffea       b 0x85138
     ┌──└─> 0x000853bc      b0550ee3       movw r5, 0xe5b0
     ╎╎│    0x000853c0      ff5f4fe3       movt r5, 0xffff
  ────────< 0x000853c4      5bffffea       b 0x85138
     ╎╎└──> 0x000853c8      5a10a0e3       mov r1, 0x5a                ; 'Z'
     ╎╎     0x000853cc      7158feeb       bl rsym.__LogCategory_Initialize
     ╎╎     0x000853d0      000050e3       cmp r0, 0
     └────< 0x000853d4      f8ffff0a       beq 0x853bc
      ╎     0x000853d8      28c094e5       ldr ip, [r4, 0x28]
      └───< 0x000853dc      e6ffffea       b 0x8537c
            0x000853e0      443d0300       andeq r3, r3, r4, asr 26
            0x000853e4      b0200000       strheq r2, [r0], -r0
            0x000853e8      5457ffff       invalid
            0x000853ec      70110000       andeq r1, r0, r0, ror r1
            0x000853f0      8458ffff       invalid
            0x000853f4      b05bffff       invalid
            0x000853f8      2c59ffff       invalid
            0x000853fc      a81c0000       andeq r1, r0, r8, lsr 25
            0x00085400      585bffff       invalid
            0x00085404      145bffff       invalid
            0x00085408      7c5bffff       invalid
