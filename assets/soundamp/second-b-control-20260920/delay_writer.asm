            0x000a9c94      00d0           mov r0, r26
            0x000a9c96      03e2           mov 3, r28
        ┌─> 0x000a9c98      1cd8           mov r28, r27
        ╎   0x000a9c9a      20361000       movea 16, r0, r6
        ╎   0x000a9c9e      053a           mov 5, r7
        ╎   0x000a9ca0      0142           mov 1, r8
        ╎   0x000a9ca2      bfff12ef       jarl 0xa8bb4, lp
        ╎   0x000a9ca6      24eec0bc       movea -17216, gp, r29
        ╎   0x000a9caa      1b68           mov r27, r13
        ╎   0x000a9cac      0d560100       addi 1, r13, r10
        ╎   0x000a9cb0      0ad8           mov r10, r27
        ╎   0x000a9cb2      54d9           and r20, r27
        ╎   0x000a9cb4      0d70           mov r13, r14
        ╎   0x000a9cb6      c272           shl 2, r14
        ╎   0x000a9cb8      dd71           add r29, r14
        ╎   0x000a9cba      207e1000       movea 16, r0, r15
        ╎   0x000a9cbe      6e7f0100       st.w r15, 0[r14]
        ╎   0x000a9cc2      1b58           mov r27, r11
        ╎   0x000a9cc4      0b860100       addi 1, r11, r16
        ╎   0x000a9cc8      10d8           mov r16, r27
        ╎   0x000a9cca      54d9           and r20, r27
        ╎   0x000a9ccc      0b88           mov r11, r17
        ╎   0x000a9cce      c28a           shl 2, r17
        ╎   0x000a9cd0      dd89           add r29, r17
        ╎   0x000a9cd2      1a90           mov r26, r18
        ╎   0x000a9cd4      c292           shl 2, r18
        ╎   0x000a9cd6      3306688a0600   mov 0x68a68, r19
        ╎   0x000a9cdc      d299           add r18, r19
        ╎   0x000a9cde      33570100       ld.w 0[r19], r10
        ╎   0x000a9ce2      71570100       st.w r10, 0[r17]
        ╎   0x000a9ce6      0060           mov r0, r12
       ┌──> 0x000a9ce8      e0d1           cmp r0, r26
      ┌───< 0x000a9cea      aa1d           bne 0xa9d1e
      │╎╎   0x000a9cec      1b58           mov r27, r11
      │╎╎   0x000a9cee      0b760100       addi 1, r11, r14
      │╎╎   0x000a9cf2      0ed8           mov r14, r27
      │╎╎   0x000a9cf4      54d9           and r20, r27
      │╎╎   0x000a9cf6      0b78           mov r11, r15
      │╎╎   0x000a9cf8      c27a           shl 2, r15
      │╎╎   0x000a9cfa      dd79           add r29, r15
      │╎╎   0x000a9cfc      3006308b0600   mov 0x68b30, r16
      │╎╎   0x000a9d02      cc81           add r12, r16
      │╎╎   0x000a9d04      90970100       ld.bu 0[r16], r18
      │╎╎   0x000a9d08      c192           shl 1, r18
      │╎╎   0x000a9d0a      440effff       movhi -1, gp, r1
      │╎╎   0x000a9d0e      219e84d1       movea -11900, r1, r19
      │╎╎   0x000a9d12      d299           add r18, r19
      │╎╎   0x000a9d14      f38f0100       ld.hu 0[r19], r17
      │╎╎   0x000a9d18      6f8f0100       st.w r17, 0[r15]
     ┌────< 0x000a9d1c      c515           br 0xa9d44
     │└───> 0x000a9d1e      1b58           mov r27, r11
     │ ╎╎   0x000a9d20      0b560100       addi 1, r11, r10
     │ ╎╎   0x000a9d24      0ad8           mov r10, r27
     │ ╎╎   0x000a9d26      54d9           and r20, r27
     │ ╎╎   0x000a9d28      0b70           mov r11, r14
     │ ╎╎   0x000a9d2a      c272           shl 2, r14
     │ ╎╎   0x000a9d2c      dd71           add r29, r14
     │ ╎╎   0x000a9d2e      0c80           mov r12, r16
     │ ╎╎   0x000a9d30      c182           shl 1, r16
     │ ╎╎   0x000a9d32      440effff       movhi -1, gp, r1
     │ ╎╎   0x000a9d36      2196d4cc       movea -13100, r1, r18
     │ ╎╎   0x000a9d3a      d091           add r16, r18
     │ ╎╎   0x000a9d3c      f29f0100       ld.hu 0[r18], r19
     │ ╎╎   0x000a9d40      6e9f0100       st.w r19, 0[r14]
     └────> 0x000a9d44      0c7e0100       addi 1, r12, r15
       ╎╎   0x000a9d48      0f60           mov r15, r12
       ╎╎   0x000a9d4a      5461           and r20, r12
       ╎╎   0x000a9d4c      6f62           cmp 15, r12
       └──< 0x000a9d4e      d7cd           ble 0xa9ce8
        ╎   0x000a9d50      bfffb4ef       jarl 0xa8d04, lp
        ╎   0x000a9d54      bfff18ef       jarl 0xa8c6c, lp
        ╎   0x000a9d58      243628bb       movea -17624, gp, r6
        ╎   0x000a9d5c      203e5800       movea 88, r0, r7            ; 'X'
        ╎   0x000a9d60      83ff7497       jarl 0xe34d4, lp
        ╎   0x000a9d64      1a8e0100       addi 1, r26, r17
        ╎   0x000a9d68      11d0           mov r17, r26
        ╎   0x000a9d6a      54d1           and r20, r26
        ╎   0x000a9d6c      62d2           cmp 2, r26
        └─< 0x000a9d6e      d695           blt 0xa9c98
