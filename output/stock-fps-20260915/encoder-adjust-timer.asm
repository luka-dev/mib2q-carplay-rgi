            0x0010a694      f04f2de9       push {r4, r5, r6, r7, r8, sb, sl, fp, lr}
            0x0010a698      30439fe5       ldr r4, [0x0010a9d0]        ; [0x10a9d0:4]=0x1af34
            0x0010a69c      30539fe5       ldr r5, [0x0010a9d4]        ; [0x10a9d4:4]=0x4a8
            0x0010a6a0      0070a0e1       mov r7, r0
            0x0010a6a4      04408fe0       add r4, pc, r4
            0x0010a6a8      14d04de2       sub sp, sp, 0x14
            0x0010a6ac      050084e0       add r0, r4, r5
            0x0010a6b0      0160a0e1       mov r6, r1
            0x0010a6b4      02a0a0e1       mov sl, r2
            0x0010a6b8      f833d0e1       ldrsh r3, [r0, 0x38]
            0x0010a6bc      010053e3       cmp r3, 1
        ┌─< 0x0010a6c0      270000da       ble 0x10a764
       ┌──> 0x0010a6c4      418f87e2       add r8, r7, 0x104
       ╎│   0x0010a6c8      0610a0e1       mov r1, r6
       ╎│   0x0010a6cc      0800a0e1       mov r0, r8
       ╎│   0x0010a6d0      0c208de2       add r2, sp, 0xc
       ╎│   0x0010a6d4      a70200eb       bl 0x10b178
       ╎│   0x0010a6d8      000050e3       cmp r0, 0
      ┌───< 0x0010a6dc      0d00001a       bne 0x10a718
      │╎│   0x0010a6e0      050084e0       add r0, r4, r5
      │╎│   0x0010a6e4      f833d0e1       ldrsh r3, [r0, 0x38]
      │╎│   0x0010a6e8      030053e3       cmp r3, 3
     ┌────< 0x0010a6ec      010000da       ble 0x10a6f8
  ┌┌┌─────> 0x0010a6f0      14d08de2       add sp, sp, 0x14
  ╎╎╎││╎│   0x0010a6f4      f08fbde8       pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}
  ╎╎╎└────> 0x0010a6f8      d8229fe5       ldr r2, [0x0010a9d8]        ; [0x10a9d8:4]=0xffff8d10
  ╎╎╎ │╎│   0x0010a6fc      0310a0e3       mov r1, 3
  ╎╎╎ │╎│   0x0010a700      d4329fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎╎ │╎│   0x0010a704      022084e0       add r2, r4, r2
  ╎╎╎ │╎│   0x0010a708      00608de5       str r6, [sp]
  ╎╎╎ │╎│   0x0010a70c      033084e0       add r3, r4, r3
  ╎╎╎ │╎│   0x0010a710      c1e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010a714      f5ffffea       b 0x10a6f0
  ╎╎╎ └───> 0x0010a718      049087e2       add sb, r7, 4
  ╎╎╎  ╎│   0x0010a71c      0c109de5       ldr r1, [sp, 0xc]
  ╎╎╎  ╎│   0x0010a720      0900a0e1       mov r0, sb
  ╎╎╎  ╎│   0x0010a724      8ee5ffeb       bl sym.imp.util::TimerGroup::isTimerValid_unsigned_int__const
  ╎╎╎  ╎│   0x0010a728      000050e3       cmp r0, 0
  ╎╎╎ ┌───< 0x0010a72c      1500001a       bne 0x10a788
  ╎╎╎ │╎│   0x0010a730      050084e0       add r0, r4, r5
  ╎╎╎ │╎│   0x0010a734      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎╎╎ │╎│   0x0010a738      030053e3       cmp r3, 3
  ────────< 0x0010a73c      ebffffca       bgt 0x10a6f0
  ╎╎╎ │╎│   0x0010a740      0cc09de5       ldr ip, [sp, 0xc]
  ╎╎╎ │╎│   0x0010a744      0310a0e3       mov r1, 3
  ╎╎╎ │╎│   0x0010a748      90229fe5       ldr r2, [0x0010a9e0]        ; [0x10a9e0:4]=0xffff8ce0
  ╎╎╎ │╎│   0x0010a74c      88329fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎╎ │╎│   0x0010a750      022084e0       add r2, r4, r2
  ╎╎╎ │╎│   0x0010a754      00c08de5       str ip, [sp]
  ╎╎╎ │╎│   0x0010a758      033084e0       add r3, r4, r3
  ╎╎╎ │╎│   0x0010a75c      aee4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010a760      e2ffffea       b 0x10a6f0
  ╎╎╎ │╎└─> 0x0010a764      78229fe5       ldr r2, [0x0010a9e4]        ; [0x10a9e4:4]=0xffff8b30
  ╎╎╎ │╎    0x0010a768      0110a0e3       mov r1, 1
  ╎╎╎ │╎    0x0010a76c      68329fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎╎ │╎    0x0010a770      022084e0       add r2, r4, r2
  ╎╎╎ │╎    0x0010a774      00608de5       str r6, [sp]
  ╎╎╎ │╎    0x0010a778      033084e0       add r3, r4, r3
  ╎╎╎ │╎    0x0010a77c      04a08de5       str sl, [sp, 4]
  ╎╎╎ │╎    0x0010a780      a5e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ╎╎╎ │└──< 0x0010a784      ceffffea       b 0x10a6c4
  ╎╎╎ └───> 0x0010a788      0900a0e1       mov r0, sb
  ╎╎╎       0x0010a78c      0c109de5       ldr r1, [sp, 0xc]
  ╎╎╎       0x0010a790      cde5ffeb       bl sym.imp.util::TimerGroup::getTimerInterval_unsigned_int__const
  ╎╎╎       0x0010a794      00005ae3       cmp sl, 0
  ╎╎╎       0x0010a798      00b0a0e1       mov fp, r0
  ╎╎╎   ┌─< 0x0010a79c      550000ba       blt 0x10a8f8
  ╎╎╎   │   0x0010a7a0      3c005ae3       cmp sl, 0x3c                ; '<'
  ╎╎╎  ┌──< 0x0010a7a4      390000ca       bgt 0x10a890
  ╎╎╎  ││   0x0010a7a8      0800a0e1       mov r0, r8
  ╎╎╎  ││   0x0010a7ac      0a10a0e1       mov r1, sl
  ╎╎╎  ││   0x0010a7b0      960400eb       bl 0x10ba10
  ╎╎╎  ││   0x0010a7b4      00005ae3       cmp sl, 0
  ╎╎╎ ┌───< 0x0010a7b8      550000da       ble 0x10a914
  ╎╎╎ │││   0x0010a7bc      0a10a0e1       mov r1, sl
  ╎╎╎ │││   0x0010a7c0      fa0fa0e3       mov r0, 0x3e8
  ╎╎╎ │││   0x0010a7c4      32e6ffeb       bl sym.imp.__aeabi_idiv
  ╎╎╎ │││   0x0010a7c8      0080a0e1       mov r8, r0
  ╎╎╎┌────> 0x0010a7cc      4daf87e2       add sl, r7, 0x134
  ╎╎╎╎│││   0x0010a7d0      0610a0e1       mov r1, r6
  ╎╎╎╎│││   0x0010a7d4      0a00a0e1       mov r0, sl
  ╎╎╎╎│││   0x0010a7d8      4f0700eb       bl 0x10c51c
  ╎╎╎╎│││   0x0010a7dc      0a00a0e1       mov r0, sl
  ╎╎╎╎│││   0x0010a7e0      0610a0e1       mov r1, r6
  ╎╎╎╎│││   0x0010a7e4      4a0900eb       bl 0x10cd14
  ╎╎╎╎│││   0x0010a7e8      000050e3       cmp r0, 0
  ────────< 0x0010a7ec      3000000a       beq 0x10a8b4
  ╎╎╎╎│││   0x0010a7f0      053084e0       add r3, r4, r5
  ╎╎╎╎│││   0x0010a7f4      f833d3e1       ldrsh r3, [r3, 0x38]
  ────────> 0x0010a7f8      020053e3       cmp r3, 2
  ────────< 0x0010a7fc      330000da       ble 0x10a8d0
  ────────> 0x0010a800      0900a0e1       mov r0, sb
  ╎╎╎╎│││   0x0010a804      0c109de5       ldr r1, [sp, 0xc]
  ╎╎╎╎│││   0x0010a808      a6e5ffeb       bl sym.imp.util::TimerGroup::stopTimer_unsigned_int_
  ╎╎╎╎│││   0x0010a80c      000050e3       cmp r0, 0
  ────────< 0x0010a810      0300000a       beq 0x10a824
  ╎╎╎╎│││   0x0010a814      050084e0       add r0, r4, r5
  ╎╎╎╎│││   0x0010a818      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎╎╎╎│││   0x0010a81c      030053e3       cmp r3, 3
  ────────< 0x0010a820      490000da       ble 0x10a94c
  ────────> 0x0010a824      0820a0e1       mov r2, r8
  ╎╎╎╎│││   0x0010a828      0900a0e1       mov r0, sb
  ╎╎╎╎│││   0x0010a82c      0c109de5       ldr r1, [sp, 0xc]
  ╎╎╎╎│││   0x0010a830      0130a0e3       mov r3, 1
  ╎╎╎╎│││   0x0010a834      38e5ffeb       bl sym.imp.util::TimerGroup::modifyTimer_unsigned_int__unsigned_int__bool_
  ╎╎╎╎│││   0x0010a838      000050e3       cmp r0, 0
  ────────< 0x0010a83c      0300000a       beq 0x10a850
  ╎╎╎╎│││   0x0010a840      050084e0       add r0, r4, r5
  ╎╎╎╎│││   0x0010a844      f833d0e1       ldrsh r3, [r0, 0x38]
  ╎╎╎╎│││   0x0010a848      030053e3       cmp r3, 3
  ────────< 0x0010a84c      370000da       ble 0x10a930
  ────────> 0x0010a850      0900a0e1       mov r0, sb
  ╎╎╎╎│││   0x0010a854      0c109de5       ldr r1, [sp, 0xc]
  ╎╎╎╎│││   0x0010a858      ffe4ffeb       bl sym.imp.util::TimerGroup::startTimer_unsigned_int_
  ╎╎╎╎│││   0x0010a85c      000050e3       cmp r0, 0
  └───────< 0x0010a860      a2ffff0a       beq 0x10a6f0
   ╎╎╎│││   0x0010a864      050084e0       add r0, r4, r5
   ╎╎╎│││   0x0010a868      f833d0e1       ldrsh r3, [r0, 0x38]
   ╎╎╎│││   0x0010a86c      030053e3       cmp r3, 3
   └──────< 0x0010a870      9effffca       bgt 0x10a6f0
    ╎╎│││   0x0010a874      6c219fe5       ldr r2, [0x0010a9e8]        ; [0x10a9e8:4]=0xffff8cb0
    ╎╎│││   0x0010a878      0310a0e3       mov r1, 3
    ╎╎│││   0x0010a87c      58319fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
    ╎╎│││   0x0010a880      022084e0       add r2, r4, r2
    ╎╎│││   0x0010a884      033084e0       add r3, r4, r3
    ╎╎│││   0x0010a888      63e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
    └─────< 0x0010a88c      97ffffea       b 0x10a6f0
     ╎│└──> 0x0010a890      050084e0       add r0, r4, r5
     ╎│ │   0x0010a894      f833d0e1       ldrsh r3, [r0, 0x38]
     ╎│ │   0x0010a898      030053e3       cmp r3, 3
     ╎│┌──< 0x0010a89c      310000da       ble 0x10a968
    ┌─────> 0x0010a8a0      0800a0e1       mov r0, r8
    ╎╎│││   0x0010a8a4      3c10a0e3       mov r1, 0x3c                ; '<'
    ╎╎│││   0x0010a8a8      580400eb       bl 0x10ba10
    ╎╎│││   0x0010a8ac      1080a0e3       mov r8, 0x10
    ╎└────< 0x0010a8b0      c5ffffea       b 0x10a7cc
  ────────> 0x0010a8b4      05a084e0       add sl, r4, r5
    ╎ │││   0x0010a8b8      f833dae1       ldrsh r3, [sl, 0x38]
    ╎ │││   0x0010a8bc      020053e3       cmp r3, 2
    ╎┌────< 0x0010a8c0      300000da       ble 0x10a988
   ┌──────> 0x0010a8c4      0120a0e3       mov r2, 1
   ╎╎││││   0x0010a8c8      1d21c7e5       strb r2, [r7, 0x11d]
  ────────< 0x0010a8cc      c9ffffea       b 0x10a7f8
  ────────> 0x0010a8d0      14219fe5       ldr r2, [0x0010a9ec]        ; [0x10a9ec:4]=0xffff8c20
   ╎╎││││   0x0010a8d4      050084e0       add r0, r4, r5
   ╎╎││││   0x0010a8d8      fc309fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
   ╎╎││││   0x0010a8dc      0210a0e3       mov r1, 2
   ╎╎││││   0x0010a8e0      022084e0       add r2, r4, r2
   ╎╎││││   0x0010a8e4      00b08de5       str fp, [sp]
   ╎╎││││   0x0010a8e8      033084e0       add r3, r4, r3
   ╎╎││││   0x0010a8ec      04808de5       str r8, [sp, 4]
   ╎╎││││   0x0010a8f0      49e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010a8f4      c1ffffea       b 0x10a800
   ╎╎│││└─> 0x0010a8f8      050084e0       add r0, r4, r5
   ╎╎│││    0x0010a8fc      f833d0e1       ldrsh r3, [r0, 0x38]
   ╎╎│││    0x0010a900      030053e3       cmp r3, 3
   ╎╎│││┌─< 0x0010a904      290000da       ble 0x10a9b0
  ┌───────> 0x0010a908      0800a0e1       mov r0, r8
  ╎╎╎││││   0x0010a90c      0010a0e3       mov r1, 0
  ╎╎╎││││   0x0010a910      3e0400eb       bl 0x10ba10
  ╎╎╎│└───> 0x0010a914      4d0f87e2       add r0, r7, 0x134
  ╎╎╎│ ││   0x0010a918      0610a0e1       mov r1, r6
  ╎╎╎│ ││   0x0010a91c      e70600eb       bl 0x10c4c0
  ╎╎╎│ ││   0x0010a920      053084e0       add r3, r4, r5
  ╎╎╎│ ││   0x0010a924      fa8fa0e3       mov r8, 0x3e8
  ╎╎╎│ ││   0x0010a928      f833d3e1       ldrsh r3, [r3, 0x38]
  ────────< 0x0010a92c      b1ffffea       b 0x10a7f8
  ────────> 0x0010a930      b8209fe5       ldr r2, [0x0010a9f0]        ; [0x10a9f0:4]=0xffff8c80
  ╎╎╎│ ││   0x0010a934      0310a0e3       mov r1, 3
  ╎╎╎│ ││   0x0010a938      9c309fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎╎│ ││   0x0010a93c      022084e0       add r2, r4, r2
  ╎╎╎│ ││   0x0010a940      033084e0       add r3, r4, r3
  ╎╎╎│ ││   0x0010a944      34e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010a948      c0ffffea       b 0x10a850
  ────────> 0x0010a94c      a0209fe5       ldr r2, [0x0010a9f4]        ; [0x10a9f4:4]=0xffff8c54
  ╎╎╎│ ││   0x0010a950      0310a0e3       mov r1, 3
  ╎╎╎│ ││   0x0010a954      80309fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎╎│ ││   0x0010a958      022084e0       add r2, r4, r2
  ╎╎╎│ ││   0x0010a95c      033084e0       add r3, r4, r3
  ╎╎╎│ ││   0x0010a960      2de4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ────────< 0x0010a964      aeffffea       b 0x10a824
  ╎╎╎│ └──> 0x0010a968      88209fe5       ldr r2, [0x0010a9f8]        ; [0x10a9f8:4]=0xffff8b98
  ╎╎╎│  │   0x0010a96c      0310a0e3       mov r1, 3
  ╎╎╎│  │   0x0010a970      64309fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎╎│  │   0x0010a974      022084e0       add r2, r4, r2
  ╎╎╎│  │   0x0010a978      00a08de5       str sl, [sp]
  ╎╎╎│  │   0x0010a97c      033084e0       add r3, r4, r3
  ╎╎╎│  │   0x0010a980      25e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ╎╎└─────< 0x0010a984      c5ffffea       b 0x10a8a0
  ╎╎ └────> 0x0010a988      6c209fe5       ldr r2, [0x0010a9fc]        ; [0x10a9fc:4]=0xffff8bcc
  ╎╎    │   0x0010a98c      0a00a0e1       mov r0, sl
  ╎╎    │   0x0010a990      44309fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎╎    │   0x0010a994      0210a0e3       mov r1, 2
  ╎╎    │   0x0010a998      022084e0       add r2, r4, r2
  ╎╎    │   0x0010a99c      00608de5       str r6, [sp]
  ╎╎    │   0x0010a9a0      033084e0       add r3, r4, r3
  ╎╎    │   0x0010a9a4      1ce4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  ╎╎    │   0x0010a9a8      f833dae1       ldrsh r3, [sl, 0x38]
  ╎└──────< 0x0010a9ac      c4ffffea       b 0x10a8c4
  ╎     └─> 0x0010a9b0      48209fe5       ldr r2, [0x0010aa00]        ; [0x10aa00:4]=0xffff8b64
  ╎         0x0010a9b4      0310a0e3       mov r1, 3
  ╎         0x0010a9b8      1c309fe5       ldr r3, [0x0010a9dc]        ; [0x10a9dc:4]=0xffff8af4
  ╎         0x0010a9bc      022084e0       add r2, r4, r2
  ╎         0x0010a9c0      00a08de5       str sl, [sp]
  ╎         0x0010a9c4      033084e0       add r3, r4, r3
  ╎         0x0010a9c8      13e4ffeb       bl sym.imp.tracing::Channel::logCV_short__char_const__..._
  └───────< 0x0010a9cc      cdffffea       b 0x10a908
            0x0010a9d0      34af0100       andeq sl, r1, r4, lsr pc
            0x0010a9d4      a8040000       andeq r0, r0, r8, lsr 9
            0x0010a9d8      108dffff       invalid
