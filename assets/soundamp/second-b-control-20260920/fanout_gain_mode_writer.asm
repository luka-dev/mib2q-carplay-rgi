            0x000ac248      00e0           mov r0, r28
            0x000ac24a      01ea           mov 1, r29
        ┌─> 0x000ac24c      01da           mov 1, r27
        ╎   0x000ac24e      1b30           mov r27, r6
        ╎   0x000ac250      053a           mov 5, r7
        ╎   0x000ac252      0142           mov 1, r8
        ╎   0x000ac254      bfff60c9       jarl 0xa8bb4, lp
        ╎   0x000ac258      05ca           mov 5, r25
        ╎   0x000ac25a      3806a8890600   mov 0x689a8, r24
        ╎   0x000ac260      64dfcdbc       st.w r27, -17204[gp]
        ╎   0x000ac264      1c50           mov r28, r10
        ╎   0x000ac266      c252           shl 2, r10
        ╎   0x000ac268      d851           add r24, r10
        ╎   0x000ac26a      2a7f0100       ld.w 0[r10], r15
        ╎   0x000ac26e      647fd1bc       st.w r15, -17200[gp]
        ╎   0x000ac272      1970           mov r25, r14
        ╎   0x000ac274      1c860100       addi 1, r28, r16
        ╎   0x000ac278      10e0           mov r16, r28
        ╎   0x000ac27a      54e1           and r20, r28
        ╎   0x000ac27c      0068           mov r0, r13
        ╎   0x000ac27e      e0d9           cmp r0, r27
       ┌──< 0x000ac280      a31d           bnh 0xac2b4
       │╎   0x000ac282      2b06d48b0600   mov 0x68bd4, r11
       │╎   0x000ac288      2466c0bc       movea -17216, gp, r12
      ┌───> 0x000ac28c      0e88           mov r14, r17
      ╎│╎   0x000ac28e      c28a           shl 2, r17
      ╎│╎   0x000ac290      cc89           add r12, r17
      ╎│╎   0x000ac292      0d90           mov r13, r18
      ╎│╎   0x000ac294      c292           shl 2, r18
      ╎│╎   0x000ac296      cb91           add r11, r18
      ╎│╎   0x000ac298      329f0100       ld.w 0[r18], r19
      ╎│╎   0x000ac29c      719f0100       st.w r19, 0[r17]
      ╎│╎   0x000ac2a0      0e560100       addi 1, r14, r10
      ╎│╎   0x000ac2a4      0a70           mov r10, r14
      ╎│╎   0x000ac2a6      5471           and r20, r14
      ╎│╎   0x000ac2a8      0d7e0100       addi 1, r13, r15
      ╎│╎   0x000ac2ac      0f68           mov r15, r13
      ╎│╎   0x000ac2ae      5569           and r21, r13
      ╎│╎   0x000ac2b0      fb69           cmp r27, r13
      └───< 0x000ac2b2      d1ed           bl 0xac28c
       └──> 0x000ac2b4      bfff50ca       jarl 0xa8d04, lp
        ╎   0x000ac2b8      bfffb4c9       jarl 0xa8c6c, lp
        ╎   0x000ac2bc      24d628bb       movea -17624, gp, r26
        ╎   0x000ac2c0      1a30           mov r26, r6
        ╎   0x000ac2c2      1b80           mov r27, r16
        ╎   0x000ac2c4      c282           shl 2, r16
        ╎   0x000ac2c6      10861800       addi 24, r16, r16
        ╎   0x000ac2ca      5581           and r21, r16
        ╎   0x000ac2cc      1038           mov r16, r7
        ╎   0x000ac2ce      83ff0672       jarl 0xe34d4, lp
        ╎   0x000ac2d2      20de1000       movea 16, r0, r27
        ╎   0x000ac2d6      1bb8           mov r27, r23
        ╎   0x000ac2d8      1730           mov r23, r6
        ╎   0x000ac2da      053a           mov 5, r7
        ╎   0x000ac2dc      0142           mov 1, r8
        ╎   0x000ac2de      bfffd6c8       jarl 0xa8bb4, lp
        ╎   0x000ac2e2      64bfcdbc       st.w r23, -17204[gp]
        ╎   0x000ac2e6      1c90           mov r28, r18
        ╎   0x000ac2e8      c292           shl 2, r18
        ╎   0x000ac2ea      d891           add r24, r18
        ╎   0x000ac2ec      328f0100       ld.w 0[r18], r17
        ╎   0x000ac2f0      648fd1bc       st.w r17, -17200[gp]
        ╎   0x000ac2f4      1970           mov r25, r14
        ╎   0x000ac2f6      1c9e0100       addi 1, r28, r19
        ╎   0x000ac2fa      13e0           mov r19, r28
        ╎   0x000ac2fc      54e1           and r20, r28
        ╎   0x000ac2fe      0068           mov r0, r13
        ╎   0x000ac300      e0b9           cmp r0, r23
       ┌──< 0x000ac302      a31d           bnh 0xac336
       │╎   0x000ac304      2b06d88b0600   mov 0x68bd8, r11
       │╎   0x000ac30a      2466c0bc       movea -17216, gp, r12
      ┌───> 0x000ac30e      0e50           mov r14, r10
      ╎│╎   0x000ac310      c252           shl 2, r10
      ╎│╎   0x000ac312      cc51           add r12, r10
      ╎│╎   0x000ac314      0d78           mov r13, r15
      ╎│╎   0x000ac316      c27a           shl 2, r15
      ╎│╎   0x000ac318      cb79           add r11, r15
      ╎│╎   0x000ac31a      2f870100       ld.w 0[r15], r16
      ╎│╎   0x000ac31e      6a870100       st.w r16, 0[r10]
      ╎│╎   0x000ac322      0e960100       addi 1, r14, r18
      ╎│╎   0x000ac326      1270           mov r18, r14
      ╎│╎   0x000ac328      5471           and r20, r14
      ╎│╎   0x000ac32a      0d8e0100       addi 1, r13, r17
      ╎│╎   0x000ac32e      1168           mov r17, r13
      ╎│╎   0x000ac330      5569           and r21, r13
      ╎│╎   0x000ac332      f769           cmp r23, r13
      └───< 0x000ac334      d1ed           bl 0xac30e
       └──> 0x000ac336      bfffcec9       jarl 0xa8d04, lp
        ╎   0x000ac33a      bfff32c9       jarl 0xa8c6c, lp
        ╎   0x000ac33e      1a30           mov r26, r6
        ╎   0x000ac340      1798           mov r23, r19
        ╎   0x000ac342      c29a           shl 2, r19
        ╎   0x000ac344      139e1800       addi 24, r19, r19
        ╎   0x000ac348      5599           and r21, r19
        ╎   0x000ac34a      1338           mov r19, r7
        ╎   0x000ac34c      83ff8871       jarl 0xe34d4, lp
        ╎   0x000ac350      1bb8           mov r27, r23
        ╎   0x000ac352      1730           mov r23, r6
        ╎   0x000ac354      053a           mov 5, r7
        ╎   0x000ac356      0142           mov 1, r8
        ╎   0x000ac358      bfff5cc8       jarl 0xa8bb4, lp
        ╎   0x000ac35c      64bfcdbc       st.w r23, -17204[gp]
        ╎   0x000ac360      1c78           mov r28, r15
        ╎   0x000ac362      c27a           shl 2, r15
        ╎   0x000ac364      d879           add r24, r15
        ╎   0x000ac366      2f570100       ld.w 0[r15], r10
        ╎   0x000ac36a      6457d1bc       st.w r10, -17200[gp]
        ╎   0x000ac36e      1970           mov r25, r14
        ╎   0x000ac370      1c860100       addi 1, r28, r16
        ╎   0x000ac374      10e0           mov r16, r28
        ╎   0x000ac376      54e1           and r20, r28
        ╎   0x000ac378      0068           mov r0, r13
        ╎   0x000ac37a      e0b9           cmp r0, r23
       ┌──< 0x000ac37c      a31d           bnh 0xac3b0
       │╎   0x000ac37e      2b06188c0600   mov 0x68c18, r11
       │╎   0x000ac384      2466c0bc       movea -17216, gp, r12
      ┌───> 0x000ac388      0e90           mov r14, r18
      ╎│╎   0x000ac38a      c292           shl 2, r18
      ╎│╎   0x000ac38c      cc91           add r12, r18
      ╎│╎   0x000ac38e      0d88           mov r13, r17
      ╎│╎   0x000ac390      c28a           shl 2, r17
      ╎│╎   0x000ac392      cb89           add r11, r17
      ╎│╎   0x000ac394      319f0100       ld.w 0[r17], r19
      ╎│╎   0x000ac398      729f0100       st.w r19, 0[r18]
      ╎│╎   0x000ac39c      0e7e0100       addi 1, r14, r15
      ╎│╎   0x000ac3a0      0f70           mov r15, r14
      ╎│╎   0x000ac3a2      5471           and r20, r14
      ╎│╎   0x000ac3a4      0d560100       addi 1, r13, r10
      ╎│╎   0x000ac3a8      0a68           mov r10, r13
      ╎│╎   0x000ac3aa      5569           and r21, r13
      ╎│╎   0x000ac3ac      f769           cmp r23, r13
      └───< 0x000ac3ae      d1ed           bl 0xac388
       └──> 0x000ac3b0      bfff54c9       jarl 0xa8d04, lp
        ╎   0x000ac3b4      bfffb8c8       jarl 0xa8c6c, lp
        ╎   0x000ac3b8      1a30           mov r26, r6
        ╎   0x000ac3ba      1780           mov r23, r16
        ╎   0x000ac3bc      c282           shl 2, r16
        ╎   0x000ac3be      10861800       addi 24, r16, r16
        ╎   0x000ac3c2      5581           and r21, r16
        ╎   0x000ac3c4      1038           mov r16, r7
        ╎   0x000ac3c6      83ff0e71       jarl 0xe34d4, lp
        ╎   0x000ac3ca      1bb8           mov r27, r23
        ╎   0x000ac3cc      1730           mov r23, r6
        ╎   0x000ac3ce      053a           mov 5, r7
        ╎   0x000ac3d0      0142           mov 1, r8
        ╎   0x000ac3d2      bfffe2c7       jarl 0xa8bb4, lp
        ╎   0x000ac3d6      64bfcdbc       st.w r23, -17204[gp]
        ╎   0x000ac3da      1c88           mov r28, r17
        ╎   0x000ac3dc      c28a           shl 2, r17
        ╎   0x000ac3de      d889           add r24, r17
        ╎   0x000ac3e0      31970100       ld.w 0[r17], r18
        ╎   0x000ac3e4      6497d1bc       st.w r18, -17200[gp]
        ╎   0x000ac3e8      1970           mov r25, r14
        ╎   0x000ac3ea      1c9e0100       addi 1, r28, r19
        ╎   0x000ac3ee      13e0           mov r19, r28
        ╎   0x000ac3f0      54e1           and r20, r28
        ╎   0x000ac3f2      0068           mov r0, r13
        ╎   0x000ac3f4      e0b9           cmp r0, r23
       ┌──< 0x000ac3f6      a31d           bnh 0xac42a
       │╎   0x000ac3f8      2b06588c0600   mov 0x68c58, r11
       │╎   0x000ac3fe      2466c0bc       movea -17216, gp, r12
      ┌───> 0x000ac402      0e78           mov r14, r15
      ╎│╎   0x000ac404      c27a           shl 2, r15
      ╎│╎   0x000ac406      cc79           add r12, r15
      ╎│╎   0x000ac408      0d50           mov r13, r10
      ╎│╎   0x000ac40a      c252           shl 2, r10
      ╎│╎   0x000ac40c      cb51           add r11, r10
      ╎│╎   0x000ac40e      2a870100       ld.w 0[r10], r16
      ╎│╎   0x000ac412      6f870100       st.w r16, 0[r15]
      ╎│╎   0x000ac416      0e8e0100       addi 1, r14, r17
      ╎│╎   0x000ac41a      1170           mov r17, r14
      ╎│╎   0x000ac41c      5471           and r20, r14
      ╎│╎   0x000ac41e      0d960100       addi 1, r13, r18
      ╎│╎   0x000ac422      1268           mov r18, r13
      ╎│╎   0x000ac424      5569           and r21, r13
      ╎│╎   0x000ac426      f769           cmp r23, r13
      └───< 0x000ac428      d1ed           bl 0xac402
       └──> 0x000ac42a      bfffdac8       jarl 0xa8d04, lp
        ╎   0x000ac42e      bfff3ec8       jarl 0xa8c6c, lp
        ╎   0x000ac432      1a30           mov r26, r6
        ╎   0x000ac434      1798           mov r23, r19
        ╎   0x000ac436      c29a           shl 2, r19
        ╎   0x000ac438      139e1800       addi 24, r19, r19
        ╎   0x000ac43c      5599           and r21, r19
        ╎   0x000ac43e      1338           mov r19, r7
        ╎   0x000ac440      83ff9470       jarl 0xe34d4, lp
        ╎   0x000ac444      1d560100       addi 1, r29, r10
        ╎   0x000ac448      0ae8           mov r10, r29
        ╎   0x000ac44a      54e9           and r20, r29
        ╎   0x000ac44c      63ea           cmp 3, r29
       ┌──< 0x000ac44e      bf05           bgt 0xac454
       │└─< 0x000ac450      bf07fcfd       jr 0xac24c
