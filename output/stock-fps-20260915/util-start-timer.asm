            ;-- method.util::TimerGroup.startTimer_unsigned_int_:
            ;-- util::TimerGroup::startTimer(unsigned int):
            0x000588e0      f0412de9       push {r4, r5, r6, r7, r8, lr} ; util::TimerGroup::startTimer(unsigned int)
            0x000588e4      146080e2       add r6, r0, 0x14
            0x000588e8      10d04de2       sub sp, sp, 0x10
            0x000588ec      0050a0e1       mov r5, r0
            0x000588f0      0600a0e1       mov r0, r6
            0x000588f4      0170a0e1       mov r7, r1
            0x000588f8      fb41ffeb       bl sym.imp.pthread_mutex_lock
            0x000588fc      f8409fe5       ldr r4, [0x000589fc]        ; [0x589fc:4]=0x8dc88
            0x00058900      04408fe0       add r4, pc, r4
            0x00058904      000050e3       cmp r0, 0
        ┌─< 0x00058908      1100001a       bne 0x58954
       ┌──> 0x0005890c      0510a0e1       mov r1, r5
       ╎│   0x00058910      0720a0e1       mov r2, r7
       ╎│   0x00058914      08008de2       add r0, sp, 8
       ╎│   0x00058918      0180a0e3       mov r8, 1
       ╎│   0x0005891c      ba46ffeb       bl 0x2a40c
       ╎│   0x00058920      0c30dde5       ldrb r3, [sp, 0xc]
       ╎│   0x00058924      000053e3       cmp r3, 0
       ╎│   0x00058928      0150a013       movne r5, 1
      ┌───< 0x0005892c      1f00000a       beq 0x589b0
    ┌┌────> 0x00058930      000058e3       cmp r8, 0
   ┌──────< 0x00058934      0300000a       beq 0x58948
   │╎╎│╎│   0x00058938      0600a0e1       mov r0, r6
   │╎╎│╎│   0x0005893c      7340ffeb       bl 0x28b10
   │╎╎│╎│   0x00058940      000050e3       cmp r0, 0
  ┌───────< 0x00058944      2000001a       bne 0x589cc
  ─└──────> 0x00058948      0500a0e1       mov r0, r5
  │ ╎╎│╎│   0x0005894c      10d08de2       add sp, sp, 0x10
  │ ╎╎│╎│   0x00058950      f081bde8       pop {r4, r5, r6, r7, r8, pc}
  │ ╎╎│╎└─> 0x00058954      7e47ffeb       bl method.osal::MutexOps.handleLockError_int_ ; osal::MutexOps::handleLockError(int)
  │ ╎╎│╎    0x00058958      000050e3       cmp r0, 0
  │ ╎╎│└──< 0x0005895c      eaffff0a       beq 0x5890c
  │ ╎╎│     0x00058960      98109fe5       ldr r1, [0x00058a00]        ; [0x58a00:4]=0xffff6204
  │ ╎╎│     0x00058964      0080a0e3       mov r8, 0
  │ ╎╎│     0x00058968      94209fe5       ldr r2, [0x00058a04]        ; [0x58a04:4]=0xffff6164
  │ ╎╎│     0x0005896c      4e3fa0e3       mov r3, 0x138
  │ ╎╎│     0x00058970      90c09fe5       ldr ip, [0x00058a08]        ; [0x58a08:4]=0xffff6314
  │ ╎╎│     0x00058974      0800a0e1       mov r0, r8
  │ ╎╎│     0x00058978      011084e0       add r1, r4, r1
  │ ╎╎│     0x0005897c      022084e0       add r2, r4, r2
  │ ╎╎│     0x00058980      0cc084e0       add ip, r4, ip
  │ ╎╎│     0x00058984      04808de5       str r8, [sp, 4]
  │ ╎╎│     0x00058988      00c08de5       str ip, [sp]
  │ ╎╎│     0x0005898c      3741ffeb       bl method.ipl::GlobalErrorHandler.doAssert_bool__char_const__char_const__int__char_const__char_const__..._ ; ipl::GlobalErrorHandler::doAssert(bool, char const*, char const*, int, char const*, char const*, ...)
  │ ╎╎│     0x00058990      0510a0e1       mov r1, r5
  │ ╎╎│     0x00058994      0720a0e1       mov r2, r7
  │ ╎╎│     0x00058998      08008de2       add r0, sp, 8
  │ ╎╎│     0x0005899c      9a46ffeb       bl 0x2a40c
  │ ╎╎│     0x000589a0      0c30dde5       ldrb r3, [sp, 0xc]
  │ ╎╎│     0x000589a4      000053e3       cmp r3, 0
  │ ╎╎│     0x000589a8      0150a013       movne r5, 1
  │ └─────< 0x000589ac      dfffff1a       bne 0x58930
  │  ╎└───> 0x000589b0      08309de5       ldr r3, [sp, 8]
  │  ╎      0x000589b4      1420d3e5       ldrb r2, [r3, 0x14]
  │  ╎      0x000589b8      080093e5       ldr r0, [r3, 8]
  │  ╎      0x000589bc      101093e5       ldr r1, [r3, 0x10]
  │  ╎      0x000589c0      8d41ffeb       bl method.osal::Timer.start_unsigned_int__bool_ ; osal::Timer::start(unsigned int, bool)
  │  ╎      0x000589c4      0050a0e1       mov r5, r0
  │  └────< 0x000589c8      d8ffffea       b 0x58930
  └───────> 0x000589cc      38109fe5       ldr r1, [0x00058a0c]        ; [0x58a0c:4]=0xffff2a10
            0x000589d0      00c0a0e3       mov ip, 0
            0x000589d4      34209fe5       ldr r2, [0x00058a10]        ; [0x58a10:4]=0xffff2a24
            0x000589d8      0c00a0e1       mov r0, ip
            0x000589dc      30e09fe5       ldr lr, [0x00058a14]        ; [0x58a14:4]=0xffff632c
            0x000589e0      011084e0       add r1, r4, r1
            0x000589e4      022084e0       add r2, r4, r2
            0x000589e8      8230a0e3       mov r3, 0x82
            0x000589ec      0e4084e0       add r4, r4, lr
            0x000589f0      10108de8       stm sp, {r4, ip}
            0x000589f4      1d41ffeb       bl method.ipl::GlobalErrorHandler.doAssert_bool__char_const__char_const__int__char_const__char_const__..._ ; ipl::GlobalErrorHandler::doAssert(bool, char const*, char const*, int, char const*, char const*, ...)
  ────────< 0x000589f8      d2ffffea       b 0x58948
            0x000589fc      88dc0800       andeq sp, r8, r8, lsl 25
            0x00058a00      0462ffff       invalid
            0x00058a04      6461ffff       invalid
            0x00058a08      1463ffff       invalid
            0x00058a0c      102affff       invalid
            0x00058a10      242affff       invalid
            0x00058a14      2c63ffff       invalid
