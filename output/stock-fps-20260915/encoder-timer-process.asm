            0x0010b478      f0472de9       push {r4, r5, r6, r7, r8, sb, sl, lr}
            0x0010b47c      48449fe5       ldr r4, [0x0010b8cc]        ; [0x10b8cc:4]=0x1a150
            0x0010b480      48649fe5       ldr r6, [0x0010b8d0]        ; [0x10b8d0:4]=0x510
            0x0010b484      0050a0e1       mov r5, r0
            0x0010b488      04408fe0       add r4, pc, r4
            0x0010b48c      18d04de2       sub sp, sp, 0x18
            0x0010b490      060084e0       add r0, r4, r6
            0x0010b494      0180a0e1       mov r8, r1
            0x0010b498      f833d0e1       ldrsh r3, [r0, 0x38]
            0x0010b49c      000053e3       cmp r3, 0
        ┌─< 0x0010b4a0      7b0000da       ble 0x10b694
       ┌──> 0x0010b4a4      1c7085e2       add r7, r5, 0x1c
       ╎│   0x0010b4a8      0700a0e1       mov r0, r7
       ╎│   0x0010b4ac      1ee1ffeb       bl sym.imp.pthread_mutex_trylock
       ╎│   0x0010b4b0      000050e3       cmp r0, 0
      ┌───< 0x0010b4b4      4e00001a       bne 0x10b5f4
     ┌────> 0x0010b4b8      0500a0e1       mov r0, r5
     ╎│╎│   0x0010b4bc      c5feffeb       bl 0x10afd8
     ╎│╎│   0x0010b4c0      00a0a0e1       mov sl, r0
     ╎│╎│   0x0010b4c4      0500a0e1       mov r0, r5
     ╎│╎│   0x0010b4c8      a7e1ffeb       bl sym.imp.pthread_mutex_lock
     ╎│╎│   0x0010b4cc      000050e3       cmp r0, 0
    ┌─────< 0x0010b4d0      5400001a       bne 0x10b628
   ┌──────> 0x0010b4d4      0190a0e3       mov sb, 1
  ┌───────> 0x0010b4d8      0500a0e1       mov r0, r5
  ╎╎│╎│╎│   0x0010b4dc      0810a0e1       mov r1, r8
  ╎╎│╎│╎│   0x0010b4e0      14208de2       add r2, sp, 0x14
  ╎╎│╎│╎│   0x0010b4e4      f9feffeb       bl 0x10b0d0
  ╎╎│╎│╎│   0x0010b4e8      000050e3       cmp r0, 0
  ────────< 0x0010b4ec      5e00000a       beq 0x10b66c
  ╎╎│╎│╎│   0x0010b4f0      060084e0       add r0, r4, r6
  ╎╎│╎│╎│   0x0010b4f4      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎╎│╎│╎│   0x0010b4f8      000053e3       cmp r3, 0
  ────────< 0x0010b4fc      bf0000da       ble 0x10b800
  ────────> 0x0010b500      140095e5       ldr r0, [r5, 0x14]
  ╎╎│╎│╎│   0x0010b504      000050e3       cmp r0, 0
  ────────< 0x0010b508      0500000a       beq 0x10b524
  ╎╎│╎│╎│   0x0010b50c      2c3095e5       ldr r3, [r5, 0x2c]
  ╎╎│╎│╎│   0x0010b510      010053e3       cmp r3, 1
  ────────< 0x0010b514      6f00000a       beq 0x10b6d8
  ╎╎│╎│╎│   0x0010b518      1830d5e5       ldrb r3, [r5, 0x18]
  ╎╎│╎│╎│   0x0010b51c      000053e3       cmp r3, 0
  ────────< 0x0010b520      a200001a       bne 0x10b7b0
  ────────> 0x0010b524      100095e5       ldr r0, [r5, 0x10]
  ╎╎│╎│╎│   0x0010b528      000050e3       cmp r0, 0
  ────────< 0x0010b52c      6000000a       beq 0x10b6b4
  ╎╎│╎│╎│   0x0010b530      14109de5       ldr r1, [sp, 0x14]
  ╎╎│╎│╎│   0x0010b534      f60500eb       bl 0x10cd14
  ╎╎│╎│╎│   0x0010b538      000050e3       cmp r0, 0
  ────────< 0x0010b53c      9700000a       beq 0x10b7a0
  ────────> 0x0010b540      0500a0e1       mov r0, r5
  ╎╎│╎│╎│   0x0010b544      a3feffeb       bl 0x10afd8
  ╎╎│╎│╎│   0x0010b548      00a06ae0       rsb sl, sl, r0
  ╎╎│╎│╎│   0x0010b54c      64005ae3       cmp sl, 0x64                ; 'd'
  ────────< 0x0010b550      390000da       ble 0x10b63c
  ╎╎│╎│╎│   0x0010b554      060084e0       add r0, r4, r6
  ╎╎│╎│╎│   0x0010b558      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎╎│╎│╎│   0x0010b55c      030053e3       cmp r3, 3
  ────────< 0x0010b560      0c0000ca       bgt 0x10b598
  ╎╎│╎│╎│   0x0010b564      0c3095e5       ldr r3, [r5, 0xc]
  ╎╎│╎│╎│   0x0010b568      0310a0e3       mov r1, 3
  ╎╎│╎│╎│   0x0010b56c      14209de5       ldr r2, [sp, 0x14]
  ╎╎│╎│╎│   0x0010b570      00a08de5       str sl, [sp]
  ╎╎│╎│╎│   0x0010b574      24c095e5       ldr ip, [r5, 0x24]
  ╎╎│╎│╎│   0x0010b578      02e193e7       ldr lr, [r3, r2, lsl 2]
  ╎╎│╎│╎│   0x0010b57c      50239fe5       ldr r2, [0x0010b8d4]        ; [0x10b8d4:4]=0xffff9310
  ╎╎│╎│╎│   0x0010b580      50339fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
  ╎╎│╎│╎│   0x0010b584      022084e0       add r2, r4, r2
  ╎╎│╎│╎│   0x0010b588      04e08de5       str lr, [sp, 4]
  ╎╎│╎│╎│   0x0010b58c      033084e0       add r3, r4, r3
  ╎╎│╎│╎│   0x0010b590      08c08de5       str ip, [sp, 8]
  ╎╎│╎│╎│   0x0010b594      20e1ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────> 0x0010b598      0700a0e1       mov r0, r7
  ╎╎│╎│╎│   0x0010b59c      83e2ffeb       bl sym.imp.pthread_mutex_unlock
  ╎╎│╎│╎│   0x0010b5a0      000050e3       cmp r0, 0
  ────────< 0x0010b5a4      0000000a       beq 0x10b5ac
  ╎╎│╎│╎│   0x0010b5a8      7ee1ffeb       bl sym.imp.osal::MutexOps::handleUnlockError_int_
  ────────> 0x0010b5ac      000059e3       cmp sb, 0
  ────────< 0x0010b5b0      1a00000a       beq 0x10b620
  ╎╎│╎│╎│   0x0010b5b4      0500a0e1       mov r0, r5
  ╎╎│╎│╎│   0x0010b5b8      c4e8ffeb       bl method.osal::Mutex.unlock___const ; osal::Mutex::unlock() const
  ╎╎│╎│╎│   0x0010b5bc      000050e3       cmp r0, 0
  ────────< 0x0010b5c0      1600000a       beq 0x10b620
  ╎╎│╎│╎│   0x0010b5c4      10139fe5       ldr r1, [0x0010b8dc]        ; [0x10b8dc:4]=0xffff7df0
  ╎╎│╎│╎│   0x0010b5c8      00c0a0e3       mov ip, 0
  ╎╎│╎│╎│   0x0010b5cc      0c239fe5       ldr r2, [0x0010b8e0]        ; [0x10b8e0:4]=0xffff7e04
  ╎╎│╎│╎│   0x0010b5d0      0c00a0e1       mov r0, ip
  ╎╎│╎│╎│   0x0010b5d4      08e39fe5       ldr lr, [0x0010b8e4]        ; [0x10b8e4:4]=0xffff8e4c
  ╎╎│╎│╎│   0x0010b5d8      011084e0       add r1, r4, r1
  ╎╎│╎│╎│   0x0010b5dc      022084e0       add r2, r4, r2
  ╎╎│╎│╎│   0x0010b5e0      8230a0e3       mov r3, 0x82
  ╎╎│╎│╎│   0x0010b5e4      0e4084e0       add r4, r4, lr
  ╎╎│╎│╎│   0x0010b5e8      10108de8       stm sp, {r4, ip}
  ╎╎│╎│╎│   0x0010b5ec      31e1ffeb       bl sym.imp.ipl::GlobalErrorHandler::doAssert_bool__char_const__char_const__int__char_const__char_const__..._
  ────────< 0x0010b5f0      0a0000ea       b 0x10b620
  ╎╎│╎└───> 0x0010b5f4      ade1ffeb       bl sym.imp.osal::MutexOps::handleTryLockError_int_
  ╎╎│╎ ╎│   0x0010b5f8      ff0010e3       tst r0, 0xff
  ╎╎│└────< 0x0010b5fc      adffff0a       beq 0x10b4b8
  ╎╎│  ╎│   0x0010b600      060084e0       add r0, r4, r6
  ╎╎│  ╎│   0x0010b604      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎╎│  ╎│   0x0010b608      040053e3       cmp r3, 4
  ╎╎│ ┌───< 0x0010b60c      030000ca       bgt 0x10b620
  ╎╎│ │╎│   0x0010b610      d0229fe5       ldr r2, [0x0010b8e8]        ; [0x10b8e8:4]=0xffff9384
  ╎╎│ │╎│   0x0010b614      0410a0e3       mov r1, 4
  ╎╎│ │╎│   0x0010b618      022084e0       add r2, r4, r2
  ╎╎│ │╎│   0x0010b61c      4be2ffeb       bl sym.imp.tracing::Channel::logC_short__char_const_
  ────└───> 0x0010b620      18d08de2       add sp, sp, 0x18
  ╎╎│  ╎│   0x0010b624      f087bde8       pop {r4, r5, r6, r7, r8, sb, sl, pc}
  ╎╎└─────> 0x0010b628      a8e2ffeb       bl sym.imp.osal::MutexOps::handleLockError_int_
  ╎╎   ╎│   0x0010b62c      000050e3       cmp r0, 0
  ╎╎   ╎│   0x0010b630      0090a013       movne sb, 0
  └───────< 0x0010b634      a7ffff1a       bne 0x10b4d8
   └──────< 0x0010b638      a5ffffea       b 0x10b4d4
  ────────> 0x0010b63c      060084e0       add r0, r4, r6
       ╎│   0x0010b640      f833d0e1       ldrsh r3, [r0, 0x38]
       ╎│   0x0010b644      000053e3       cmp r3, 0
  ────────< 0x0010b648      d2ffffca       bgt 0x10b598
       ╎│   0x0010b64c      98229fe5       ldr r2, [0x0010b8ec]        ; [0x10b8ec:4]=0xffff9360
       ╎│   0x0010b650      0010a0e3       mov r1, 0
       ╎│   0x0010b654      7c329fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
       ╎│   0x0010b658      022084e0       add r2, r4, r2
       ╎│   0x0010b65c      00a08de5       str sl, [sp]
       ╎│   0x0010b660      033084e0       add r3, r4, r3
       ╎│   0x0010b664      ece0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010b668      caffffea       b 0x10b598
  ────────> 0x0010b66c      060084e0       add r0, r4, r6
       ╎│   0x0010b670      f833d0e1       ldrsh r3, [r0, 0x38]
       ╎│   0x0010b674      030053e3       cmp r3, 3
  ────────< 0x0010b678      b0ffffca       bgt 0x10b540
       ╎│   0x0010b67c      6c229fe5       ldr r2, [0x0010b8f0]        ; [0x10b8f0:4]=0xffff92dc
       ╎│   0x0010b680      0830a0e1       mov r3, r8
       ╎│   0x0010b684      0310a0e3       mov r1, 3
       ╎│   0x0010b688      022084e0       add r2, r4, r2
       ╎│   0x0010b68c      e2e0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010b690      aaffffea       b 0x10b540
       ╎└─> 0x0010b694      58229fe5       ldr r2, [0x0010b8f4]        ; [0x10b8f4:4]=0xffff90c8
       ╎    0x0010b698      0010a0e3       mov r1, 0
       ╎    0x0010b69c      34329fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
       ╎    0x0010b6a0      022084e0       add r2, r4, r2
       ╎    0x0010b6a4      00808de5       str r8, [sp]
       ╎    0x0010b6a8      033084e0       add r3, r4, r3
       ╎    0x0010b6ac      dae0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
       └──< 0x0010b6b0      7bffffea       b 0x10b4a4
  ────────> 0x0010b6b4      060084e0       add r0, r4, r6
            0x0010b6b8      f833d0e1       ldrsh r3, [r0, 0x38]
            0x0010b6bc      030053e3       cmp r3, 3
  ────────< 0x0010b6c0      9effffca       bgt 0x10b540
            0x0010b6c4      2c229fe5       ldr r2, [0x0010b8f8]        ; [0x10b8f8:4]=0xffff92ac
            0x0010b6c8      0310a0e3       mov r1, 3
            0x0010b6cc      022084e0       add r2, r4, r2
            0x0010b6d0      1ee2ffeb       bl sym.imp.tracing::Channel::logC_short__char_const_
  ────────< 0x0010b6d4      99ffffea       b 0x10b540
  ────────> 0x0010b6d8      1930d5e5       ldrb r3, [r5, 0x19]
            0x0010b6dc      000053e3       cmp r3, 0
        ┌─< 0x0010b6e0      1400001a       bne 0x10b738
       ┌──> 0x0010b6e4      1830d5e5       ldrb r3, [r5, 0x18]
      ┌───> 0x0010b6e8      000053e3       cmp r3, 0
     ┌────< 0x0010b6ec      1c00000a       beq 0x10b764
    ┌─────> 0x0010b6f0      2830d5e5       ldrb r3, [r5, 0x28]
    ╎│╎╎│   0x0010b6f4      000053e3       cmp r3, 0
   ┌──────< 0x0010b6f8      5a00001a       bne 0x10b868
  ┌───────> 0x0010b6fc      140095e5       ldr r0, [r5, 0x14]
  ╎│╎│╎╎│   0x0010b700      941000eb       bl 0x10f958
  ╎│╎│╎╎│   0x0010b704      000050e3       cmp r0, 0
  ────────< 0x0010b708      85ffff1a       bne 0x10b524
  ╎│╎│╎╎│   0x0010b70c      060084e0       add r0, r4, r6
  ╎│╎│╎╎│   0x0010b710      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎│╎│╎╎│   0x0010b714      040053e3       cmp r3, 4
  ────────< 0x0010b718      580000da       ble 0x10b880
  ────────> 0x0010b71c      140095e5       ldr r0, [r5, 0x14]
  ╎│╎│╎╎│   0x0010b720      e31300eb       bl 0x1106b4
  ╎│╎│╎╎│   0x0010b724      0030a0e3       mov r3, 0
  ╎│╎│╎╎│   0x0010b728      1830c5e5       strb r3, [r5, 0x18]
  ╎│╎│╎╎│   0x0010b72c      0130a0e3       mov r3, 1
  ╎│╎│╎╎│   0x0010b730      1930c5e5       strb r3, [r5, 0x19]
  ────────< 0x0010b734      7affffea       b 0x10b524
  ╎│╎│╎╎└─> 0x0010b738      0c3095e5       ldr r3, [r5, 0xc]
  ╎│╎│╎╎    0x0010b73c      14209de5       ldr r2, [sp, 0x14]
  ╎│╎│╎╎    0x0010b740      021193e7       ldr r1, [r3, r2, lsl 2]
  ╎│╎│╎╎    0x0010b744      6b1400eb       bl 0x1108f8
  ╎│╎│╎╎    0x0010b748      000050e3       cmp r0, 0
  ╎│╎│╎╎    0x0010b74c      1800c5e5       strb r0, [r5, 0x18]
  ╎│╎│╎╎┌─< 0x0010b750      3600001a       bne 0x10b830
  ╎│╎│╎╎│   0x0010b754      060084e0       add r0, r4, r6
  ╎│╎│╎╎│   0x0010b758      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎│╎│╎╎│   0x0010b75c      040053e3       cmp r3, 4
  ────────< 0x0010b760      4d0000da       ble 0x10b89c
  ╎│╎└────> 0x0010b764      060084e0       add r0, r4, r6
  ╎│╎ ╎╎│   0x0010b768      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎│╎ ╎╎│   0x0010b76c      040053e3       cmp r3, 4
  ────────< 0x0010b770      6bffffca       bgt 0x10b524
  ╎│╎ ╎╎│   0x0010b774      19e0d5e5       ldrb lr, [r5, 0x19]
  ╎│╎ ╎╎│   0x0010b778      0410a0e3       mov r1, 4
  ╎│╎ ╎╎│   0x0010b77c      78219fe5       ldr r2, [0x0010b8fc]        ; [0x10b8fc:4]=0xffff91d8
  ╎│╎ ╎╎│   0x0010b780      00c0a0e3       mov ip, 0
  ╎│╎ ╎╎│   0x0010b784      4c319fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
  ╎│╎ ╎╎│   0x0010b788      022084e0       add r2, r4, r2
  ╎│╎ ╎╎│   0x0010b78c      00c08de5       str ip, [sp]
  ╎│╎ ╎╎│   0x0010b790      033084e0       add r3, r4, r3
  ╎│╎ ╎╎│   0x0010b794      04e08de5       str lr, [sp, 4]
  ╎│╎ ╎╎│   0x0010b798      9fe0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010b79c      60ffffea       b 0x10b524
  ────────> 0x0010b7a0      100095e5       ldr r0, [r5, 0x10]
  ╎│╎ ╎╎│   0x0010b7a4      14109de5       ldr r1, [sp, 0x14]
  ╎│╎ ╎╎│   0x0010b7a8      5b0300eb       bl 0x10c51c
  ────────< 0x0010b7ac      63ffffea       b 0x10b540
  ────────> 0x0010b7b0      bf1300eb       bl 0x1106b4
  ╎│╎ ╎╎│   0x0010b7b4      060084e0       add r0, r4, r6
  ╎│╎ ╎╎│   0x0010b7b8      00c0a0e3       mov ip, 0
  ╎│╎ ╎╎│   0x0010b7bc      01e0a0e3       mov lr, 1
  ╎│╎ ╎╎│   0x0010b7c0      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎│╎ ╎╎│   0x0010b7c4      18c0c5e5       strb ip, [r5, 0x18]
  ╎│╎ ╎╎│   0x0010b7c8      020053e3       cmp r3, 2
  ╎│╎ ╎╎│   0x0010b7cc      19e0c5e5       strb lr, [r5, 0x19]
  ────────< 0x0010b7d0      53ffffca       bgt 0x10b524
  ╎│╎ ╎╎│   0x0010b7d4      2c8095e5       ldr r8, [r5, 0x2c]
  ╎│╎ ╎╎│   0x0010b7d8      0210a0e3       mov r1, 2
  ╎│╎ ╎╎│   0x0010b7dc      1c219fe5       ldr r2, [0x0010b900]        ; [0x10b900:4]=0xffff9248
  ╎│╎ ╎╎│   0x0010b7e0      f0309fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
  ╎│╎ ╎╎│   0x0010b7e4      022084e0       add r2, r4, r2
  ╎│╎ ╎╎│   0x0010b7e8      00c08de5       str ip, [sp]
  ╎│╎ ╎╎│   0x0010b7ec      033084e0       add r3, r4, r3
  ╎│╎ ╎╎│   0x0010b7f0      04e08de5       str lr, [sp, 4]
  ╎│╎ ╎╎│   0x0010b7f4      08808de5       str r8, [sp, 8]
  ╎│╎ ╎╎│   0x0010b7f8      87e0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010b7fc      48ffffea       b 0x10b524
  ────────> 0x0010b800      0c2095e5       ldr r2, [r5, 0xc]
  ╎│╎ ╎╎│   0x0010b804      0010a0e3       mov r1, 0
  ╎│╎ ╎╎│   0x0010b808      14309de5       ldr r3, [sp, 0x14]
  ╎│╎ ╎╎│   0x0010b80c      00308de5       str r3, [sp]
  ╎│╎ ╎╎│   0x0010b810      03c192e7       ldr ip, [r2, r3, lsl 2]
  ╎│╎ ╎╎│   0x0010b814      e8209fe5       ldr r2, [0x0010b904]        ; [0x10b904:4]=0xffff90e4
  ╎│╎ ╎╎│   0x0010b818      b8309fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
  ╎│╎ ╎╎│   0x0010b81c      022084e0       add r2, r4, r2
  ╎│╎ ╎╎│   0x0010b820      04c08de5       str ip, [sp, 4]
  ╎│╎ ╎╎│   0x0010b824      033084e0       add r3, r4, r3
  ╎│╎ ╎╎│   0x0010b828      7be0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010b82c      33ffffea       b 0x10b500
  ╎│╎ ╎╎└─> 0x0010b830      060084e0       add r0, r4, r6
  ╎│╎ ╎╎    0x0010b834      0030a0e3       mov r3, 0
  ╎│╎ ╎╎    0x0010b838      1930c5e5       strb r3, [r5, 0x19]
  ╎│╎ ╎╎    0x0010b83c      f823d0e1       ldrsh r2, [r0, 0x38]
  ╎│╎ ╎╎    0x0010b840      020052e3       cmp r2, 2
  ╎│└─────< 0x0010b844      a9ffffca       bgt 0x10b6f0
  ╎│  ╎╎    0x0010b848      00308de5       str r3, [sp]
  ╎│  ╎╎    0x0010b84c      0210a0e3       mov r1, 2
  ╎│  ╎╎    0x0010b850      b0209fe5       ldr r2, [0x0010b908]        ; [0x10b908:4]=0xffff915c
  ╎│  ╎╎    0x0010b854      7c309fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
  ╎│  ╎╎    0x0010b858      022084e0       add r2, r4, r2
  ╎│  ╎╎    0x0010b85c      033084e0       add r3, r4, r3
  ╎│  ╎╎    0x0010b860      6de0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ╎│  ╎└──< 0x0010b864      9effffea       b 0x10b6e4
  ╎└──────> 0x0010b868      140095e5       ldr r0, [r5, 0x14]
  ╎   ╎     0x0010b86c      241095e5       ldr r1, [r5, 0x24]
  ╎   ╎     0x0010b870      cb0f00eb       bl 0x10f7a4
  ╎   ╎     0x0010b874      0030a0e3       mov r3, 0
  ╎   ╎     0x0010b878      2830c5e5       strb r3, [r5, 0x28]
  └───────< 0x0010b87c      9effffea       b 0x10b6fc
  ────────> 0x0010b880      84209fe5       ldr r2, [0x0010b90c]        ; [0x10b90c:4]=0xffff91a4
      ╎     0x0010b884      0410a0e3       mov r1, 4
      ╎     0x0010b888      48309fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
      ╎     0x0010b88c      022084e0       add r2, r4, r2
      ╎     0x0010b890      033084e0       add r3, r4, r3
      ╎     0x0010b894      60e0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010b898      9fffffea       b 0x10b71c
  ────────> 0x0010b89c      0c3095e5       ldr r3, [r5, 0xc]
      ╎     0x0010b8a0      0410a0e3       mov r1, 4
      ╎     0x0010b8a4      14209de5       ldr r2, [sp, 0x14]
      ╎     0x0010b8a8      02c193e7       ldr ip, [r3, r2, lsl 2]
      ╎     0x0010b8ac      5c209fe5       ldr r2, [0x0010b910]        ; [0x10b910:4]=0xffff912c
      ╎     0x0010b8b0      20309fe5       ldr r3, [0x0010b8d8]        ; [0x10b8d8:4]=0xffff944c
      ╎     0x0010b8b4      022084e0       add r2, r4, r2
      ╎     0x0010b8b8      00c08de5       str ip, [sp]
      ╎     0x0010b8bc      033084e0       add r3, r4, r3
      ╎     0x0010b8c0      55e0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
      ╎     0x0010b8c4      1830d5e5       ldrb r3, [r5, 0x18]
      └───< 0x0010b8c8      86ffffea       b 0x10b6e8
            0x0010b8cc      50a10100       andeq sl, r1, r0, asr r1
            0x0010b8d0      10050000       andeq r0, r0, r0, lsl r5
            0x0010b8d4      1093ffff       invalid
            0x0010b8d8      4c94ffff       invalid
            0x0010b8dc      f07dffff       invalid
            0x0010b8e0      047effff       invalid
            0x0010b8e4      4c8effff       invalid
            0x0010b8e8      8493ffff       invalid
            0x0010b8ec      6093ffff       invalid
            0x0010b8f0      dc92ffff       invalid
            0x0010b8f4      c890ffff       invalid
            0x0010b8f8      ac92ffff       invalid
            0x0010b8fc      d891ffff       invalid
            0x0010b900      4892ffff       invalid
            0x0010b904      e490ffff       invalid
            0x0010b908      5c91ffff       invalid
            0x0010b90c      a491ffff       invalid
            0x0010b910      2c91ffff       invalid
            0x0010b914      f0402de9       push {r4, r5, r6, r7, lr}
            0x0010b918      d8409fe5       ldr r4, [0x0010b9f8]        ; [0x10b9f8:4]=0x19cb4
            0x0010b91c      0050a0e1       mov r5, r0
            0x0010b920      d4009fe5       ldr r0, [0x0010b9fc]        ; [0x10b9fc:4]=0x510
            0x0010b924      04408fe0       add r4, pc, r4
            0x0010b928      0cd04de2       sub sp, sp, 0xc
            0x0010b92c      000084e0       add r0, r4, r0
            0x0010b930      0160a0e1       mov r6, r1
            0x0010b934      0270a0e1       mov r7, r2
            0x0010b938      f833d0e1       ldrsh r3, [r0, 0x38]
            0x0010b93c      020053e3       cmp r3, 2
        ┌─< 0x0010b940      190000da       ble 0x10b9ac
       ┌──> 0x0010b944      0500a0e1       mov r0, r5
       ╎│   0x0010b948      87e0ffeb       bl sym.imp.pthread_mutex_lock
       ╎│   0x0010b94c      000050e3       cmp r0, 0
      ┌───< 0x0010b950      1000001a       bne 0x10b998
     ┌────> 0x0010b954      0120a0e3       mov r2, 1
    ┌─────> 0x0010b958      000056e3       cmp r6, 0
    ╎╎│╎│   0x0010b95c      0130a003       moveq r3, 1
    ╎╎│╎│   0x0010b960      0060a013       movne r6, 0
    ╎╎│╎│   0x0010b964      0c109505       ldreq r1, [r5, 0xc]
    ╎╎│╎│   0x0010b968      0360a001       moveq r6, r3
    ╎╎│╎│   0x0010b96c      1930c505       strbeq r3, [r5, 0x19]
    ╎╎│╎│   0x0010b970      00708105       streq r7, [r1]
    ╎╎│╎│   0x0010b974      000052e3       cmp r2, 0
   ┌──────< 0x0010b978      0300000a       beq 0x10b98c
   │╎╎│╎│   0x0010b97c      0500a0e1       mov r0, r5
   │╎╎│╎│   0x0010b980      d2e7ffeb       bl method.osal::Mutex.unlock___const ; osal::Mutex::unlock() const
   │╎╎│╎│   0x0010b984      000050e3       cmp r0, 0
  ┌───────< 0x0010b988      0e00001a       bne 0x10b9c8
  ─└──────> 0x0010b98c      0600a0e1       mov r0, r6
  │ ╎╎│╎│   0x0010b990      0cd08de2       add sp, sp, 0xc
  │ ╎╎│╎│   0x0010b994      f080bde8       pop {r4, r5, r6, r7, pc}
  │ ╎╎└───> 0x0010b998      cce1ffeb       bl sym.imp.osal::MutexOps::handleLockError_int_
  │ ╎╎ ╎│   0x0010b99c      000050e3       cmp r0, 0
  │ ╎╎ ╎│   0x0010b9a0      0020a013       movne r2, 0
  │ └─────< 0x0010b9a4      ebffff1a       bne 0x10b958
  │  └────< 0x0010b9a8      e9ffffea       b 0x10b954
  │    ╎└─> 0x0010b9ac      4c209fe5       ldr r2, [0x0010ba00]        ; [0x10ba00:4]=0xffff93b4
  │    ╎    0x0010b9b0      0210a0e3       mov r1, 2
  │    ╎    0x0010b9b4      0630a0e1       mov r3, r6
  │    ╎    0x0010b9b8      00708de5       str r7, [sp]
  │    ╎    0x0010b9bc      022084e0       add r2, r4, r2
  │    ╎    0x0010b9c0      15e0ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  │    └──< 0x0010b9c4      deffffea       b 0x10b944
  └───────> 0x0010b9c8      34109fe5       ldr r1, [0x0010ba04]        ; [0x10ba04:4]=0xffff7df0
            0x0010b9cc      00c0a0e3       mov ip, 0
            0x0010b9d0      30209fe5       ldr r2, [0x0010ba08]        ; [0x10ba08:4]=0xffff7e04
            0x0010b9d4      0c00a0e1       mov r0, ip
            0x0010b9d8      2ce09fe5       ldr lr, [0x0010ba0c]        ; [0x10ba0c:4]=0xffff8e4c
            0x0010b9dc      011084e0       add r1, r4, r1
            0x0010b9e0      022084e0       add r2, r4, r2
            0x0010b9e4      8230a0e3       mov r3, 0x82
            0x0010b9e8      0e4084e0       add r4, r4, lr
            0x0010b9ec      10108de8       stm sp, {r4, ip}
            0x0010b9f0      30e0ffeb       bl sym.imp.ipl::GlobalErrorHandler::doAssert_bool__char_const__char_const__int__char_const__char_const__..._
  ────────< 0x0010b9f4      e4ffffea       b 0x10b98c
            0x0010b9f8      b49c0100       strheq sb, [r1], -r4
            0x0010b9fc      10050000       andeq r0, r0, r0, lsl r5
            0x0010ba00      b493ffff       invalid
            0x0010ba04      f07dffff       invalid
            0x0010ba08      047effff       invalid
            0x0010ba0c      4c8effff       invalid
