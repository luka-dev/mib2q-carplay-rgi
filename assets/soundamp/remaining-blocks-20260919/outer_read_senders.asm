0x000ac6cc      e535           br 0xac738
0x000ac6ce      63070100       st.w r0, 0[sp]
0x000ac6d2      2437c1c1       ld.w -15936[gp], r6
0x000ac6d6      23560000       movea 0, sp, r10
0x000ac6da      2a3f0100       ld.w 0[r10], r7
0x000ac6de      bfff6aff       jarl 0xac648, lp
0x000ac6e2      2606e8950b00   mov 0xb95e8, r6
0x000ac6e8      013a           mov 1, r7
0x000ac6ea      bfff32c5       jarl 0xa8c1c, lp
0x000ac6ee      bfff16c6       jarl 0xa8d04, lp
0x000ac6f2      bfff7ac5       jarl 0xa8c6c, lp
0x000ac6f6      243628bb       movea -17624, gp, r6
0x000ac6fa      243ec0bc       movea -17216, gp, r7
0x000ac6fe      83ffd26e       jarl 0xe35d0, lp
0x000ac702      2467c1bc       ld.w -17216[gp], r12
0x000ac706      6467a5c2       st.w r12, -15708[gp]
0x000ac70a      a45fa1c2       ld.bu -15711[gp], r11
0x000ac70e      e50d           br 0xac72a
0x000ac710      0232           mov 2, r6
0x000ac712      0c3a           mov 12, r7
0x000ac714      83ffc00b       jarl 0xdd2d4, lp
0x000ac718      e50d           br 0xac734
0x000ac71a      0732           mov 7, r6
0x000ac71c      203e1000       movea 16, r0, r7
0x000ac720      83ffb40b       jarl 0xdd2d4, lp
0x000ac724      850d           br 0xac734
0x000ac726      f505           br 0xac734
0x000ac728      e505           br 0xac734
0x000ac72a      625a           cmp 2, r11
0x000ac72c      a2f5           be 0xac710
0x000ac72e      675a           cmp 7, r11
0x000ac730      d2f5           be 0xac71a
0x000ac732      a5fd           br 0xac726
0x000ac734      42063f00       dispose 1, {lp}, lp
0x000ac738      82072100       prepare {lp}, 1
0x000ac73c      95cd           br 0xac6ce
0x000ac73e      0000           nop
0x000ac740      8007d000       jr 0xac810
0x000ac744      63070100       st.w r0, 0[sp]
0x000ac748      2606feaf0008   mov 0x800affe, r6
0x000ac74e      23560000       movea 0, sp, r10
0x000ac752      2a3f0100       ld.w 0[r10], r7
0x000ac756      bffff2fe       jarl 0xac648, lp
0x000ac75a      2606e8950b00   mov 0xb95e8, r6
0x000ac760      013a           mov 1, r7
0x000ac762      bfffbac4       jarl 0xa8c1c, lp
0x000ac766      bfff9ec5       jarl 0xa8d04, lp
0x000ac76a      bfff02c5       jarl 0xa8c6c, lp
0x000ac76e      24eec0bc       movea -17216, gp, r29
0x000ac772      24e628bb       movea -17624, gp, r28
0x000ac776      1c30           mov r28, r6
0x000ac778      1d38           mov r29, r7
0x000ac77a      83ff566e       jarl 0xe35d0, lp
0x000ac77e      24cfc1bc       ld.w -17216[gp], r25
0x000ac782      00d0           mov r0, r26
0x000ac784      00d8           mov r0, r27
0x000ac786      1b68           mov r27, r13
0x000ac788      d969           add r25, r13
0x000ac78a      0d30           mov r13, r6
0x000ac78c      203e3c00       movea 60, r0, r7                        ; '<'
0x000ac790      bfff8cc4       jarl 0xa8c1c, lp
0x000ac794      bfff70c5       jarl 0xa8d04, lp
0x000ac798      bfffd4c4       jarl 0xa8c6c, lp
0x000ac79c      1c30           mov r28, r6
0x000ac79e      1d38           mov r29, r7
0x000ac7a0      83ff306e       jarl 0xe35d0, lp
0x000ac7a4      0060           mov r0, r12
0x000ac7a6      245ea4c2       movea -15708, gp, r11
0x000ac7aa      1a70           mov r26, r14
0x000ac7ac      c272           shl 2, r14
0x000ac7ae      cb71           add r11, r14
0x000ac7b0      0c78           mov r12, r15
0x000ac7b2      c27a           shl 2, r15
0x000ac7b4      dd79           add r29, r15
0x000ac7b6      2f870100       ld.w 0[r15], r16
0x000ac7ba      6e870100       st.w r16, 0[r14]
0x000ac7be      1a8e0100       addi 1, r26, r17
0x000ac7c2      11d0           mov r17, r26
0x000ac7c4      55d1           and r21, r26
0x000ac7c6      0c960100       addi 1, r12, r18
0x000ac7ca      1260           mov r18, r12
0x000ac7cc      5461           and r20, r12
0x000ac7ce      6f62           cmp 15, r12
0x000ac7d0      d6ed           blt 0xac7aa
0x000ac7d2      1b9e0f00       addi 15, r27, r19
0x000ac7d6      13d8           mov r19, r27
0x000ac7d8      54d9           and r20, r27
0x000ac7da      200e3c00       movea 60, r0, r1                        ; '<'
0x000ac7de      e1d9           cmp r1, r27
0x000ac7e0      b6d5           blt 0xac786
0x000ac7e2      a45fa1c2       ld.bu -15711[gp], r11
0x000ac7e6      e50d           br 0xac802
0x000ac7e8      0232           mov 2, r6
0x000ac7ea      0c3a           mov 12, r7
0x000ac7ec      83ffe80a       jarl 0xdd2d4, lp
0x000ac7f0      e50d           br 0xac80c
0x000ac7f2      0732           mov 7, r6
0x000ac7f4      203e1000       movea 16, r0, r7
0x000ac7f8      83ffdc0a       jarl 0xdd2d4, lp
0x000ac7fc      850d           br 0xac80c
0x000ac7fe      f505           br 0xac80c
0x000ac800      e505           br 0xac80c
0x000ac802      625a           cmp 2, r11
0x000ac804      a2f5           be 0xac7e8
0x000ac806      675a           cmp 7, r11
0x000ac808      d2f5           be 0xac7f2
0x000ac80a      a5fd           br 0xac7fe
0x000ac80c      4206ff70       dispose 1, {r25, r26, r27, r28, r29, lp}, lp
0x000ac810      8207e170       prepare {r25, r26, r27, r28, r29, lp}, 1
0x000ac814      859d           br 0xac744
0x000ac816      0000           nop
