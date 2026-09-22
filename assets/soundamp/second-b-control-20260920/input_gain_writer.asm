            0x000ac1c6      03ca           mov 3, r25
            0x000ac1c8      1930           mov r25, r6
            0x000ac1ca      053a           mov 5, r7
            0x000ac1cc      0142           mov 1, r8
            0x000ac1ce      bfffe6c9       jarl 0xa8bb4, lp
            0x000ac1d2      1a70           mov r26, r14
            0x000ac1d4      64cfcdbc       st.w r25, -17204[gp]
            0x000ac1d8      1b68           mov r27, r13
            0x000ac1da      1c60           mov r28, r12
            0x000ac1dc      3206b8040c00   mov 0xc04b8, r18
            0x000ac1e2      6497d1bc       st.w r18, -17200[gp]
            0x000ac1e6      0058           mov r0, r11
            0x000ac1e8      e0c9           cmp r0, r25
        ┌─< 0x000ac1ea      a31d           bnh 0xac21e
        │   0x000ac1ec      2d06c88b0600   mov 0x68bc8, r13
        │   0x000ac1f2      2476c0bc       movea -17216, gp, r14
       ┌──> 0x000ac1f6      0c78           mov r12, r15
       ╎│   0x000ac1f8      c27a           shl 2, r15
       ╎│   0x000ac1fa      ce79           add r14, r15
       ╎│   0x000ac1fc      0b98           mov r11, r19
       ╎│   0x000ac1fe      c29a           shl 2, r19
       ╎│   0x000ac200      cd99           add r13, r19
       ╎│   0x000ac202      33570100       ld.w 0[r19], r10
       ╎│   0x000ac206      6f570100       st.w r10, 0[r15]
       ╎│   0x000ac20a      0c860100       addi 1, r12, r16
       ╎│   0x000ac20e      1060           mov r16, r12
       ╎│   0x000ac210      5461           and r20, r12
       ╎│   0x000ac212      0b8e0100       addi 1, r11, r17
       ╎│   0x000ac216      1158           mov r17, r11
       ╎│   0x000ac218      5559           and r21, r11
       ╎│   0x000ac21a      f959           cmp r25, r11
       └──< 0x000ac21c      d1ed           bl 0xac1f6
        └─> 0x000ac21e      bfffe6ca       jarl 0xa8d04, lp
            0x000ac222      bfff4aca       jarl 0xa8c6c, lp
            0x000ac226      1d30           mov r29, r6
            0x000ac228      1990           mov r25, r18
            0x000ac22a      c292           shl 2, r18
            0x000ac22c      12961800       addi 24, r18, r18
            0x000ac230      5591           and r21, r18
            0x000ac232      1238           mov r18, r7
            0x000ac234      83ffa072       jarl 0xe34d4, lp
