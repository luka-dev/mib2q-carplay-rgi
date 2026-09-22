            0x000a9938      63370100       st.w r6, 0[sp]
            0x000a993c      e3370100       ld.hu 0[sp], r6
            0x000a9940      f531           cmp r21, r6
        ┌─< 0x000a9942      fa05           bne 0xa9950
        │   0x000a9944      06ba           mov 6, r23
        │   0x000a9946      20c61400       movea 20, r0, r24
        │   0x000a994a      04d2           mov 4, r26
        │   0x000a994c      00e8           mov r0, r29
       ┌──< 0x000a994e      d505           br 0xa9958
       │└─> 0x000a9950      01ba           mov 1, r23
       │    0x000a9952      05c2           mov 5, r24
       │    0x000a9954      01d2           mov 1, r26
       │    0x000a9956      06e8           mov r6, r29
       └──> 0x000a9958      00c8           mov r0, r25
            0x000a995a      e0b9           cmp r0, r23
        ┌─< 0x000a995c      8365           bnh 0xa9a1c
        │   0x000a995e      3606088a0600   mov 0x68a08, r22
       ┌──> 0x000a9964      1d50           mov r29, r10
       ╎│   0x000a9966      c252           shl 2, r10
       ╎│   0x000a9968      d651           add r22, r10
       ╎│   0x000a996a      2a6f0100       ld.w 0[r10], r13
       ╎│   0x000a996e      7f6a           cmp -1, r13
      ┌───< 0x000a9970      e24d           be 0xa9a0c
      │╎│   0x000a9972      03e2           mov 3, r28
      │╎│   0x000a9974      1830           mov r24, r6
      │╎│   0x000a9976      053a           mov 5, r7
      │╎│   0x000a9978      0142           mov 1, r8
      │╎│   0x000a997a      bfff3af2       jarl 0xa8bb4, lp
      │╎│   0x000a997e      1c60           mov r28, r12
      │╎│   0x000a9980      0c760100       addi 1, r12, r14
      │╎│   0x000a9984      0ee0           mov r14, r28
      │╎│   0x000a9986      54e1           and r20, r28
      │╎│   0x000a9988      0c78           mov r12, r15
      │╎│   0x000a998a      c27a           shl 2, r15
      │╎│   0x000a998c      2486c0bc       movea -17216, gp, r16
      │╎│   0x000a9990      cf81           add r15, r16
      │╎│   0x000a9992      70c70100       st.w r24, 0[r16]
      │╎│   0x000a9996      1c58           mov r28, r11
      │╎│   0x000a9998      0b8e0100       addi 1, r11, r17
      │╎│   0x000a999c      11e0           mov r17, r28
      │╎│   0x000a999e      54e1           and r20, r28
      │╎│   0x000a99a0      0b90           mov r11, r18
      │╎│   0x000a99a2      c292           shl 2, r18
      │╎│   0x000a99a4      249ec0bc       movea -17216, gp, r19
      │╎│   0x000a99a8      d299           add r18, r19
      │╎│   0x000a99aa      1d50           mov r29, r10
      │╎│   0x000a99ac      c252           shl 2, r10
      │╎│   0x000a99ae      d651           add r22, r10
      │╎│   0x000a99b0      2a6f0100       ld.w 0[r10], r13
      │╎│   0x000a99b4      736f0100       st.w r13, 0[r19]
      │╎│   0x000a99b8      00d8           mov r0, r27
      │╎│   0x000a99ba      e0d1           cmp r0, r26
     ┌────< 0x000a99bc      931d           bnh 0xa99ee
    ┌─────> 0x000a99be      1d70           mov r29, r14
    ╎││╎│   0x000a99c0      c372           shl 3, r14
    ╎││╎│   0x000a99c2      440effff       movhi -1, gp, r1
    ╎││╎│   0x000a99c6      217e70ca       movea -13712, r1, r15
    ╎││╎│   0x000a99ca      ce79           add r14, r15
    ╎││╎│   0x000a99cc      0f30           mov r15, r6
    ╎││╎│   0x000a99ce      bffff6fc       jarl 0xa96c4, lp
    ╎││╎│   0x000a99d2      1c30           mov r28, r6
    ╎││╎│   0x000a99d4      80ffa425       jarl 0xabf78, lp
    ╎││╎│   0x000a99d8      0ae0           mov r10, r28
    ╎││╎│   0x000a99da      1d860100       addi 1, r29, r16
    ╎││╎│   0x000a99de      10e8           mov r16, r29
    ╎││╎│   0x000a99e0      55e9           and r21, r29
    ╎││╎│   0x000a99e2      1b8e0100       addi 1, r27, r17
    ╎││╎│   0x000a99e6      11d8           mov r17, r27
    ╎││╎│   0x000a99e8      54d9           and r20, r27
    ╎││╎│   0x000a99ea      fad9           cmp r26, r27
    └─────< 0x000a99ec      91ed           bl 0xa99be
     └────> 0x000a99ee      bfff16f3       jarl 0xa8d04, lp
      │╎│   0x000a99f2      bfff7af2       jarl 0xa8c6c, lp
      │╎│   0x000a99f6      243628bb       movea -17624, gp, r6
      │╎│   0x000a99fa      1890           mov r24, r18
      │╎│   0x000a99fc      c292           shl 2, r18
      │╎│   0x000a99fe      12961800       addi 24, r18, r18
      │╎│   0x000a9a02      5591           and r21, r18
      │╎│   0x000a9a04      1238           mov r18, r7
      │╎│   0x000a9a06      83ffce9a       jarl 0xe34d4, lp
     ┌────< 0x000a9a0a      b505           br 0xa9a10
     │└───> 0x000a9a0c      dae9           add r26, r29
     │ ╎│   0x000a9a0e      55e9           and r21, r29
     └────> 0x000a9a10      199e0100       addi 1, r25, r19
       ╎│   0x000a9a14      13c8           mov r19, r25
       ╎│   0x000a9a16      54c9           and r20, r25
       ╎│   0x000a9a18      f7c9           cmp r23, r25
       └──< 0x000a9a1a      d1a5           bl 0xa9964
        └─> 0x000a9a1c      4206fff3       dispose 1, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
