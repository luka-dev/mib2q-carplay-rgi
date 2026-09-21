            0x000a80b8      00e8           mov r0, r29
            0x000a80ba      00d8           mov r0, r27
        ┌─> 0x000a80bc      0132           mov 1, r6
        ╎   0x000a80be      053a           mov 5, r7
        ╎   0x000a80c0      0142           mov 1, r8
        ╎   0x000a80c2      80fff20a       jarl 0xa8bb4, lp
        ╎   0x000a80c6      37066c890600   mov 0x6896c, r23
        ╎   0x000a80cc      0152           mov 1, r10
        ╎   0x000a80ce      6457cdbc       st.w r10, -17204[gp]
        ╎   0x000a80d2      1d78           mov r29, r15
        ╎   0x000a80d4      c27a           shl 2, r15
        ╎   0x000a80d6      d779           add r23, r15
        ╎   0x000a80d8      2f870100       ld.w 0[r15], r16
        ╎   0x000a80dc      6487d1bc       st.w r16, -17200[gp]
        ╎   0x000a80e0      1d8e0100       addi 1, r29, r17
        ╎   0x000a80e4      11e8           mov r17, r29
        ╎   0x000a80e6      54e9           and r20, r29
        ╎   0x000a80e8      0192           mov 1, r18
        ╎   0x000a80ea      6497d5bc       st.w r18, -17196[gp]
        ╎   0x000a80ee      065a           mov 6, r11
        ╎   0x000a80f0      80ff140c       jarl 0xa8d04, lp
        ╎   0x000a80f4      80ff780b       jarl 0xa8c6c, lp
        ╎   0x000a80f8      24b628bb       movea -17624, gp, r22
        ╎   0x000a80fc      1630           mov r22, r6
        ╎   0x000a80fe      203e1c00       movea 28, r0, r7
        ╎   0x000a8102      83ffd2b3       jarl 0xe34d4, lp
        ╎   0x000a8106      e0d9           cmp r0, r27
       ┌──< 0x000a8108      b205           be 0xa810e
       │╎   0x000a810a      64da           cmp 4, r27
      ┌───< 0x000a810c      9a0d           bne 0xa811e
      │└──> 0x000a810e      0a5a           mov 10, r11
      │ ╎   0x000a8110      0be0           mov r11, r28
      │ ╎   0x000a8112      20ce1400       movea 20, r0, r25
      │ ╎   0x000a8116      0bc0           mov r11, r24
      │ ╎   0x000a8118      40d62041       movhi 16672, r0, r26        ; ' A'
      │┌──< 0x000a811c      e505           br 0xa8128
      └───> 0x000a811e      01e2           mov 1, r28
       │╎   0x000a8120      02ca           mov 2, r25
       │╎   0x000a8122      01c2           mov 1, r24
       │╎   0x000a8124      40d68040       movhi 16512, r0, r26
       └──> 0x000a8128      1930           mov r25, r6
        ╎   0x000a812a      053a           mov 5, r7
        ╎   0x000a812c      0142           mov 1, r8
        ╎   0x000a812e      80ff860a       jarl 0xa8bb4, lp
        ╎   0x000a8132      64cfcdbc       st.w r25, -17204[gp]
        ╎   0x000a8136      1d98           mov r29, r19
        ╎   0x000a8138      c29a           shl 2, r19
        ╎   0x000a813a      d799           add r23, r19
        ╎   0x000a813c      33570100       ld.w 0[r19], r10
        ╎   0x000a8140      6457d1bc       st.w r10, -17200[gp]
        ╎   0x000a8144      055a           mov 5, r11
        ╎   0x000a8146      1d7e0100       addi 1, r29, r15
        ╎   0x000a814a      0fe8           mov r15, r29
        ╎   0x000a814c      54e9           and r20, r29
        ╎   0x000a814e      0060           mov r0, r12
        ╎   0x000a8150      e0e1           cmp r0, r28
       ┌──< 0x000a8152      9325           bnh 0xa8194
       │╎   0x000a8154      26068b42783f   mov 0x3f78428b, r6
       │╎   0x000a815a      246ec0bc       movea -17216, gp, r13
       │╎   0x000a815e      2e069efd833f   mov 0x3f83fd9e, r14
      ┌───> 0x000a8164      0b80           mov r11, r16
      ╎│╎   0x000a8166      c282           shl 2, r16
      ╎│╎   0x000a8168      cd81           add r13, r16
      ╎│╎   0x000a816a      70770100       st.w r14, 0[r16]
      ╎│╎   0x000a816e      0b8e0100       addi 1, r11, r17
      ╎│╎   0x000a8172      1158           mov r17, r11
      ╎│╎   0x000a8174      5459           and r20, r11
      ╎│╎   0x000a8176      0b90           mov r11, r18
      ╎│╎   0x000a8178      c292           shl 2, r18
      ╎│╎   0x000a817a      cd91           add r13, r18
      ╎│╎   0x000a817c      72370100       st.w r6, 0[r18]
      ╎│╎   0x000a8180      0b9e0100       addi 1, r11, r19
      ╎│╎   0x000a8184      1358           mov r19, r11
      ╎│╎   0x000a8186      5459           and r20, r11
      ╎│╎   0x000a8188      0c560100       addi 1, r12, r10
      ╎│╎   0x000a818c      0a60           mov r10, r12
      ╎│╎   0x000a818e      5461           and r20, r12
      ╎│╎   0x000a8190      fc61           cmp r28, r12
      └───< 0x000a8192      91ed           bl 0xa8164
       └──> 0x000a8194      80ff700b       jarl 0xa8d04, lp
        ╎   0x000a8198      80ffd40a       jarl 0xa8c6c, lp
        ╎   0x000a819c      1630           mov r22, r6
        ╎   0x000a819e      1978           mov r25, r15
        ╎   0x000a81a0      c27a           shl 2, r15
        ╎   0x000a81a2      0f7e1800       addi 24, r15, r15
        ╎   0x000a81a6      5579           and r21, r15
        ╎   0x000a81a8      0f38           mov r15, r7
        ╎   0x000a81aa      83ff2ab3       jarl 0xe34d4, lp
        ╎   0x000a81ae      1830           mov r24, r6
        ╎   0x000a81b0      053a           mov 5, r7
        ╎   0x000a81b2      0142           mov 1, r8
        ╎   0x000a81b4      80ff000a       jarl 0xa8bb4, lp
        ╎   0x000a81b8      64c7cdbc       st.w r24, -17204[gp]
        ╎   0x000a81bc      1d80           mov r29, r16
        ╎   0x000a81be      c282           shl 2, r16
        ╎   0x000a81c0      d781           add r23, r16
        ╎   0x000a81c2      308f0100       ld.w 0[r16], r17
        ╎   0x000a81c6      648fd1bc       st.w r17, -17200[gp]
        ╎   0x000a81ca      056a           mov 5, r13
        ╎   0x000a81cc      1d960100       addi 1, r29, r18
        ╎   0x000a81d0      12e8           mov r18, r29
        ╎   0x000a81d2      54e9           and r20, r29
        ╎   0x000a81d4      0060           mov r0, r12
        ╎   0x000a81d6      e0e1           cmp r0, r28
       ┌──< 0x000a81d8      a315           bnh 0xa81fc
       │╎   0x000a81da      245ec0bc       movea -17216, gp, r11
      ┌───> 0x000a81de      0d98           mov r13, r19
      ╎│╎   0x000a81e0      c29a           shl 2, r19
      ╎│╎   0x000a81e2      cb99           add r11, r19
      ╎│╎   0x000a81e4      73d70100       st.w r26, 0[r19]
      ╎│╎   0x000a81e8      0d560100       addi 1, r13, r10
      ╎│╎   0x000a81ec      0a68           mov r10, r13
      ╎│╎   0x000a81ee      5469           and r20, r13
      ╎│╎   0x000a81f0      0c7e0100       addi 1, r12, r15
      ╎│╎   0x000a81f4      0f60           mov r15, r12
      ╎│╎   0x000a81f6      5461           and r20, r12
      ╎│╎   0x000a81f8      fc61           cmp r28, r12
      └───< 0x000a81fa      a1f5           bl 0xa81de
       └──> 0x000a81fc      80ff080b       jarl 0xa8d04, lp
        ╎   0x000a8200      80ff6c0a       jarl 0xa8c6c, lp
        ╎   0x000a8204      1630           mov r22, r6
        ╎   0x000a8206      1880           mov r24, r16
        ╎   0x000a8208      c282           shl 2, r16
        ╎   0x000a820a      10861800       addi 24, r16, r16
        ╎   0x000a820e      5581           and r21, r16
        ╎   0x000a8210      1038           mov r16, r7
        ╎   0x000a8212      83ffc2b2       jarl 0xe34d4, lp
        ╎   0x000a8216      1b8e0100       addi 1, r27, r17
        ╎   0x000a821a      11d8           mov r17, r27
        ╎   0x000a821c      54d9           and r20, r27
        ╎   0x000a821e      65da           cmp 5, r27
       ┌──< 0x000a8220      be05           bge 0xa8226
       │└─< 0x000a8222      bf079afe       jr 0xa80bc
       └──> 0x000a8226      4006fff3       dispose 0, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
