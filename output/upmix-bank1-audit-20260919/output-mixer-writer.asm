0x000a9ff4      mov r0, r23
0x000a9ff6      mov 3, r25
0x000a9ff8      mov r25, r24
0x000a9ffa      movea 16, r0, r6
0x000a9ffe      mov 5, r7
0x000aa000      mov 1, r8
0x000aa002      jarl 0xa8bb4, lp
0x000aa006      movea -17216, gp, r26
0x000aa00a      mov r24, r14
0x000aa00c      addi 1, r14, r10
0x000aa010      mov r10, r24
0x000aa012      and r20, r24
0x000aa014      mov r14, r15
0x000aa016      shl 2, r15
0x000aa018      add r26, r15
0x000aa01a      movea 16, r0, r16
0x000aa01e      st.w r16, 0[r15]
0x000aa022      mov r24, r11
0x000aa024      addi 1, r11, r17
0x000aa028      mov r17, r24
0x000aa02a      and r20, r24
0x000aa02c      mov r11, r18
0x000aa02e      shl 2, r18
0x000aa030      add r26, r18
0x000aa032      mov r23, r19
0x000aa034      shl 4, r19
0x000aa036      mov 0xe453a, r1                                        ; ':E\x0e'
0x000aa03c      add r1, r19
0x000aa03e      st.w r19, 0[r18]
0x000aa042      mov r23, r13
0x000aa044      shl 3, r13
0x000aa046      and r20, r13
0x000aa048      mov r13, r12
0x000aa04a      movhi -1, gp, r1
0x000aa04e      movea -13832, r1, r27
0x000aa052      movea -128, r0, r28
0x000aa056      mov r27, r10
0x000aa058      add r12, r10
0x000aa05a      ld.b 0[r10], r15
0x000aa05e      cmp r28, r15
0x000aa060      bne 0xaa066
0x000aa062      mov 0, r11
0x000aa064      br 0xaa0a4
0x000aa066      mov r27, r16
0x000aa068      add r12, r16
0x000aa06a      ld.b 0[r16], r17
0x000aa06e      shl 3, r17
0x000aa070      not r17, r17
0x000aa072      add 1, r17
0x000aa074      mov 0x68d58, r18
0x000aa07a      add r17, r18
0x000aa07c      ld.w 1600[r18], r11                                    ; 0x640 ; 1600
0x000aa080      movhi -1, gp, r1
0x000aa084      movea -13848, r1, r19
0x000aa088      add r12, r19
0x000aa08a      ld.b 0[r19], r10
0x000aa08e      movea 90, r0, r1                                       ; 'Z'
0x000aa092      cmp r1, r10
0x000aa094      bne 0xaa0a4
0x000aa096      mov r0, r15
0x000aa098      mov r11, r6
0x000aa09a      mov r15, r7
0x000aa09c      jarl 0xebb70, lp
0x000aa0a0      mov r6, r15
0x000aa0a2      mov r15, r11
0x000aa0a4      movhi -1, gp, r1
0x000aa0a8      movea -13116, r1, r29
0x000aa0ac      mov r24, r14
0x000aa0ae      addi 1, r14, r16
0x000aa0b2      mov r16, r24
0x000aa0b4      and r20, r24
0x000aa0b6      mov r14, r17
0x000aa0b8      shl 2, r17
0x000aa0ba      add r26, r17
0x000aa0bc      st.w r11, 0[r17]
0x000aa0c0      mov r29, r18
0x000aa0c2      add r12, r18
0x000aa0c4      ld.b 0[r18], r19
0x000aa0c8      cmp r19, r28
0x000aa0ca      bne 0xaa0d0
0x000aa0cc      mov 0, r11
0x000aa0ce      br 0xaa0ea
0x000aa0d0      mov r29, r10
0x000aa0d2      add r12, r10
0x000aa0d4      ld.b 0[r10], r15
0x000aa0d8      shl 3, r15
0x000aa0da      not r15, r15
0x000aa0dc      add 1, r15
0x000aa0de      mov 0x68d58, r16
0x000aa0e4      add r15, r16
0x000aa0e6      ld.w 1600[r16], r11                                    ; 0x640 ; 1600
0x000aa0ea      mov r24, r14
0x000aa0ec      addi 1, r14, r17
0x000aa0f0      mov r17, r24
0x000aa0f2      and r20, r24
0x000aa0f4      mov r14, r18
0x000aa0f6      shl 2, r18
0x000aa0f8      add r26, r18
0x000aa0fa      st.w r11, 0[r18]
0x000aa0fe      addi 1, r12, r19
0x000aa102      mov r19, r12
0x000aa104      and r20, r12
0x000aa106      addi 8, r13, r10
0x000aa10a      cmp r10, r12
0x000aa10c      blt 0xaa056
0x000aa10e      jarl 0xa8d04, lp
0x000aa112      jarl 0xa8c6c, lp
0x000aa116      movea -17624, gp, r6
0x000aa11a      movea 88, r0, r7                                       ; 'X'
0x000aa11e      jarl 0xe34d4, lp
0x000aa122      addi 1, r23, r15
0x000aa126      mov r15, r23
0x000aa128      and r20, r23
0x000aa12a      cmp 2, r23
0x000aa12c      bge 0xaa132
0x000aa12e      jr 0xa9ff8
0x000aa132      dispose 0, {r23, r24, r25, r26, r27, r28, r29, lp}, lp
