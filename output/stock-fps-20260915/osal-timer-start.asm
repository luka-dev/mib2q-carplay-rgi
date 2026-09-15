            ;-- method.osal::Timer.start_unsigned_int__bool_:
            ;-- osal::Timer::start(unsigned int, bool):
            0x0003593c      70402de9       push {r4, r5, r6, lr}       ; osal::Timer::start(unsigned int, bool)
            0x00035940      000051e3       cmp r1, 0
            0x00035944      24419fe5       ldr r4, [0x00035a70]        ; [0x35a70:4]=0x48454
            0x00035948      28d04de2       sub sp, sp, 0x28
            0x0003594c      04408fe0       add r4, pc, r4
        ┌─< 0x00035950      3300000a       beq 0x35a24
        │   0x00035954      d33d04e3       movw r3, 0x4dd3
        │   0x00035958      40c204e3       movw ip, 0x4240             ; '@B'
        │   0x0003595c      623041e3       movt r3, 0x1062
        │   0x00035960      0fc040e3       movt ip, 0xf
        │   0x00035964      000052e3       cmp r2, 0
        │   0x00035968      14208005       streq r2, [r0, 0x14]
        │   0x0003596c      93e183e0       umull lr, r3, r3, r1
        │   0x00035970      23e3a0e1       lsr lr, r3, 6
        │   0x00035974      fa3fa0e3       mov r3, 0x3e8
        │   0x00035978      14e08015       strne lr, [r0, 0x14]
        │   0x0003597c      931e61e0       mls r1, r3, lr, r1
        │   0x00035980      0ce080e5       str lr, [r0, 0xc]
        │   0x00035984      9c0103e0       mul r3, ip, r1
        │   0x00035988      0010a0e3       mov r1, 0
        │   0x0003598c      103080e5       str r3, [r0, 0x10]
        │   0x00035990      0230a001       moveq r3, r2
        │   0x00035994      0c2080e2       add r2, r0, 0xc
        │   0x00035998      183080e5       str r3, [r0, 0x18]
        │   0x0003599c      080090e5       ldr r0, [r0, 8]
        │   0x000359a0      0130a0e1       mov r3, r1
        │   0x000359a4      bf95ffeb       bl sym.imp.timer_settime
        │   0x000359a8      000050e3       cmp r0, 0
       ┌──< 0x000359ac      0100001a       bne 0x359b8
     ┌┌───> 0x000359b0      28d08de2       add sp, sp, 0x28
     ╎╎││   0x000359b4      7080bde8       pop {r4, r5, r6, pc}
     ╎╎└──> 0x000359b8      b4109fe5       ldr r1, [0x00035a74]        ; [0x35a74:4]=0xffff74a8
     ╎╎ │   0x000359bc      0c508de2       add r5, sp, 0xc
     ╎╎ │   0x000359c0      0130a0e3       mov r3, 1
     ╎╎ │   0x000359c4      042400e3       movw r2, 0x404
     ╎╎ │   0x000359c8      011084e0       add r1, r4, r1
     ╎╎ │   0x000359cc      24608de2       add r6, sp, 0x24
     ╎╎ │   0x000359d0      0500a0e1       mov r0, r5
     ╎╎ │   0x000359d4      3796ffeb       bl sym.imp.ipl::ErrorOutput::ErrorOutput_char_const__int__bool_ ; ipl::ErrorOutput::ErrorOutput(char const*, int, bool)
     ╎╎ │   0x000359d8      0600a0e1       mov r0, r6
     ╎╎ │   0x000359dc      de95ffeb       bl 0x1b15c
     ╎╎ │   0x000359e0      0610a0e1       mov r1, r6
     ╎╎ │   0x000359e4      0d00a0e1       mov r0, sp
     ╎╎ │   0x000359e8      079affeb       bl 0x1c20c
     ╎╎ │   0x000359ec      08309de5       ldr r3, [sp, 8]
     ╎╎ │   0x000359f0      000053e3       cmp r3, 0
     ╎╎┌──< 0x000359f4      1a00000a       beq 0x35a64
    ┌─────> 0x000359f8      78109fe5       ldr r1, [0x00035a78]        ; [0x35a78:4]=0x9cc
    ╎╎╎││   0x000359fc      0500a0e1       mov r0, r5
    ╎╎╎││   0x00035a00      0120a0e3       mov r2, 1
    ╎╎╎││   0x00035a04      011094e7       ldr r1, [r4, r1]
    ╎╎╎││   0x00035a08      8097ffeb       bl method.ipl::ErrorOutput.operator___ipl::ErrorType_const__int__..._ ; ipl::ErrorOutput::operator()(ipl::ErrorType const&, int, ...)
    ╎╎╎││   0x00035a0c      08009de5       ldr r0, [sp, 8]
    ╎╎╎││   0x00035a10      000050e3       cmp r0, 0
   ┌──────< 0x00035a14      0000000a       beq 0x35a1c
   │╎╎╎││   0x00035a18      ee9affeb       bl sym.imp.free
   └──────> 0x00035a1c      0100a0e3       mov r0, 1
    ╎└────< 0x00035a20      e2ffffea       b 0x359b0
    ╎ ╎│└─> 0x00035a24      48109fe5       ldr r1, [0x00035a74]        ; [0x35a74:4]=0xffff74a8
    ╎ ╎│    0x00035a28      18508de2       add r5, sp, 0x18
    ╎ ╎│    0x00035a2c      e72300e3       movw r2, 0x3e7
    ╎ ╎│    0x00035a30      0130a0e3       mov r3, 1
    ╎ ╎│    0x00035a34      011084e0       add r1, r4, r1
    ╎ ╎│    0x00035a38      0500a0e1       mov r0, r5
    ╎ ╎│    0x00035a3c      1d96ffeb       bl sym.imp.ipl::ErrorOutput::ErrorOutput_char_const__int__bool_ ; ipl::ErrorOutput::ErrorOutput(char const*, int, bool)
    ╎ ╎│    0x00035a40      34109fe5       ldr r1, [0x00035a7c]        ; [0x35a7c:4]=0x9ec
    ╎ ╎│    0x00035a44      34309fe5       ldr r3, [0x00035a80]        ; [0x35a80:4]=0xffff752c
    ╎ ╎│    0x00035a48      0500a0e1       mov r0, r5
    ╎ ╎│    0x00035a4c      0120a0e3       mov r2, 1
    ╎ ╎│    0x00035a50      033084e0       add r3, r4, r3
    ╎ ╎│    0x00035a54      011094e7       ldr r1, [r4, r1]
    ╎ ╎│    0x00035a58      6c97ffeb       bl method.ipl::ErrorOutput.operator___ipl::ErrorType_const__int__..._ ; ipl::ErrorOutput::operator()(ipl::ErrorType const&, int, ...)
    ╎ ╎│    0x00035a5c      0100a0e3       mov r0, 1
    ╎ └───< 0x00035a60      d2ffffea       b 0x359b0
    ╎  └──> 0x00035a64      18309fe5       ldr r3, [0x00035a84]        ; [0x35a84:4]=0x910
    ╎       0x00035a68      033094e7       ldr r3, [r4, r3]
    └─────< 0x00035a6c      e1ffffea       b 0x359f8
            0x00035a70      54840400       andeq r8, r4, r4, asr r4
            0x00035a74      a874ffff       invalid
            0x00035a78      cc090000       andeq r0, r0, ip, asr 19
            0x00035a7c      ec090000       andeq r0, r0, ip, ror 19
            0x00035a80      2c75ffff       invalid
            0x00035a84      10090000       andeq r0, r0, r0, lsl sb
