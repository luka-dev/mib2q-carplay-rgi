            ;-- method.util::TimerGroup.modifyTimer_unsigned_int__unsigned_int__bool_:
            ;-- util::TimerGroup::modifyTimer(unsigned int, unsigned int, bool):
            0x000577ac      f0472de9       push {r4, r5, r6, r7, r8, sb, sl, lr} ; util::TimerGroup::modifyTimer(unsigned int, unsigned int, bool)
            0x000577b0      146080e2       add r6, r0, 0x14
            0x000577b4      10d04de2       sub sp, sp, 0x10
            0x000577b8      0050a0e1       mov r5, r0
            0x000577bc      0600a0e1       mov r0, r6
            0x000577c0      0170a0e1       mov r7, r1
            0x000577c4      0290a0e1       mov sb, r2
            0x000577c8      03a0a0e1       mov sl, r3
            0x000577cc      4646ffeb       bl sym.imp.pthread_mutex_lock
            0x000577d0      c8409fe5       ldr r4, [0x000578a0]        ; [0x578a0:4]=0x8edb4
            0x000577d4      04408fe0       add r4, pc, r4
            0x000577d8      000050e3       cmp r0, 0
        ┌─< 0x000577dc      1300001a       bne 0x57830
       ┌──> 0x000577e0      0180a0e3       mov r8, 1
      ┌───> 0x000577e4      0510a0e1       mov r1, r5
      ╎╎│   0x000577e8      0720a0e1       mov r2, r7
      ╎╎│   0x000577ec      08008de2       add r0, sp, 8
      ╎╎│   0x000577f0      054bffeb       bl 0x2a40c
      ╎╎│   0x000577f4      0c50dde5       ldrb r5, [sp, 0xc]
      ╎╎│   0x000577f8      000055e3       cmp r5, 0
      ╎╎│   0x000577fc      0150a013       movne r5, 1
      ╎╎│   0x00057800      08309d05       ldreq r3, [sp, 8]
      ╎╎│   0x00057804      14a0c305       strbeq sl, [r3, 0x14]
      ╎╎│   0x00057808      10908305       streq sb, [r3, 0x10]
      ╎╎│   0x0005780c      000058e3       cmp r8, 0
     ┌────< 0x00057810      0300000a       beq 0x57824
     │╎╎│   0x00057814      0600a0e1       mov r0, r6
     │╎╎│   0x00057818      bc44ffeb       bl 0x28b10
     │╎╎│   0x0005781c      000050e3       cmp r0, 0
    ┌─────< 0x00057820      1200001a       bne 0x57870
   ┌─└────> 0x00057824      0500a0e1       mov r0, r5
   ╎│ ╎╎│   0x00057828      10d08de2       add sp, sp, 0x10
   ╎│ ╎╎│   0x0005782c      f087bde8       pop {r4, r5, r6, r7, r8, sb, sl, pc}
   ╎│ ╎╎└─> 0x00057830      c74bffeb       bl method.osal::MutexOps.handleLockError_int_ ; osal::MutexOps::handleLockError(int)
   ╎│ ╎╎    0x00057834      000050e3       cmp r0, 0
   ╎│ ╎└──< 0x00057838      e8ffff0a       beq 0x577e0
   ╎│ ╎     0x0005783c      60109fe5       ldr r1, [0x000578a4]        ; [0x578a4:4]=0xffff6204
   ╎│ ╎     0x00057840      0080a0e3       mov r8, 0
   ╎│ ╎     0x00057844      5c209fe5       ldr r2, [0x000578a8]        ; [0x578a8:4]=0xffff6164
   ╎│ ╎     0x00057848      0800a0e1       mov r0, r8
   ╎│ ╎     0x0005784c      58c09fe5       ldr ip, [0x000578ac]        ; [0x578ac:4]=0xffff612c
   ╎│ ╎     0x00057850      011084e0       add r1, r4, r1
   ╎│ ╎     0x00057854      022084e0       add r2, r4, r2
   ╎│ ╎     0x00057858      5d3fa0e3       mov r3, 0x174
   ╎│ ╎     0x0005785c      0cc084e0       add ip, r4, ip
   ╎│ ╎     0x00057860      04808de5       str r8, [sp, 4]
   ╎│ ╎     0x00057864      00c08de5       str ip, [sp]
   ╎│ ╎     0x00057868      8045ffeb       bl method.ipl::GlobalErrorHandler.doAssert_bool__char_const__char_const__int__char_const__char_const__..._ ; ipl::GlobalErrorHandler::doAssert(bool, char const*, char const*, int, char const*, char const*, ...)
   ╎│ └───< 0x0005786c      dcffffea       b 0x577e4
   ╎└─────> 0x00057870      38109fe5       ldr r1, [0x000578b0]        ; [0x578b0:4]=0xffff2a10
   ╎        0x00057874      00c0a0e3       mov ip, 0
   ╎        0x00057878      34209fe5       ldr r2, [0x000578b4]        ; [0x578b4:4]=0xffff2a24
   ╎        0x0005787c      0c00a0e1       mov r0, ip
   ╎        0x00057880      30e09fe5       ldr lr, [0x000578b8]        ; [0x578b8:4]=0xffff632c
   ╎        0x00057884      011084e0       add r1, r4, r1
   ╎        0x00057888      022084e0       add r2, r4, r2
   ╎        0x0005788c      8230a0e3       mov r3, 0x82
   ╎        0x00057890      0e4084e0       add r4, r4, lr
   ╎        0x00057894      10108de8       stm sp, {r4, ip}
   ╎        0x00057898      7445ffeb       bl method.ipl::GlobalErrorHandler.doAssert_bool__char_const__char_const__int__char_const__char_const__..._ ; ipl::GlobalErrorHandler::doAssert(bool, char const*, char const*, int, char const*, char const*, ...)
   └──────< 0x0005789c      e0ffffea       b 0x57824
            0x000578a0      b4ed0800       strheq lr, [r8], -r4
            0x000578a4      0462ffff       invalid
            0x000578a8      6461ffff       invalid
            0x000578ac      2c61ffff       invalid
            0x000578b0      102affff       invalid
            0x000578b4      242affff       invalid
            0x000578b8      2c63ffff       invalid
