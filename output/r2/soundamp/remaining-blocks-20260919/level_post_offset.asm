0x000afb16      0050           mov r0, r10
0x000afb18      0060           mov r0, r12
0x000afb1a      4056009c       movhi -25600, r0, r10
0x000afb1e      245e0888       movea -30712, gp, r11
0x000afb22      0c70           mov r12, r14
0x000afb24      c272           shl 2, r14
0x000afb26      cb71           add r11, r14
0x000afb28      2e7f0100       ld.w 0[r14], r15
0x000afb2c      ea79           cmp r10, r15
0x000afb2e      c62d           blt 0xafb86
0x000afb30      0c80           mov r12, r16
0x000afb32      c282           shl 2, r16
0x000afb34      cb81           add r11, r16
0x000afb36      308f0100       ld.w 0[r16], r17
0x000afb3a      400e0001       movhi 256, r0, r1
0x000afb3e      e18fc002       div r1, r17, r0
0x000afb42      f100           sxh r17
0x000afb44      f100           sxh r17
0x000afb46      046f24a2       ld.b -24028[gp], r13
0x000afb4a      d169           add r17, r13
0x000afb4c      200e9cff       movea -100, r0, r1
0x000afb50      e169           cmp r1, r13
0x000afb52      9e0d           bge 0xafb64
0x000afb54      2496d387       movea -30765, gp, r18
0x000afb58      cc91           add r12, r18
0x000afb5a      209e9cff       movea -100, r0, r19
0x000afb5e      529f0000       st.b r19, 0[r18]
0x000afb62      a515           br 0xafb86
0x000afb64      200e1a00       movea 26, r0, r1
0x000afb68      e169           cmp r1, r13
0x000afb6a      970d           ble 0xafb7c
0x000afb6c      2476d387       movea -30765, gp, r14
0x000afb70      cc71           add r12, r14
0x000afb72      207e1a00       movea 26, r0, r15
0x000afb76      4e7f0000       st.b r15, 0[r14]
0x000afb7a      e505           br 0xafb86
0x000afb7c      2486d387       movea -30765, gp, r16
0x000afb80      cc81           add r12, r16
0x000afb82      506f0000       st.b r13, 0[r16]
0x000afb86      0c8e0100       addi 1, r12, r17
0x000afb8a      1160           mov r17, r12
0x000afb8c      5461           and r20, r12
0x000afb8e      6462           cmp 4, r12
0x000afb90      96cd           blt 0xafb22
0x000afb92      40063f00       dispose 0, {lp}, lp
0x000afb96      80072100       prepare {lp}, 0
0x000afb9a      e5bd           br 0xafb16
