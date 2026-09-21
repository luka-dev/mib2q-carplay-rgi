0x000d57c8      80071a01       jr 0xd58e2
0x000d57cc      63372500       st.w r6, 36[sp]                         ; 0x24
0x000d57d0      e3372500       ld.hu 36[sp], r6                        ; 0x24
0x000d57d4      633f2900       st.w r7, 40[sp]                         ; 0x28
0x000d57d8      83c72900       ld.bu 40[sp], r24                       ; 0x28
0x000d57dc      63472d00       st.w r8, 44[sp]                         ; 0x2c
0x000d57e0      23472d00       ld.w 44[sp], r8                         ; 0x2c
0x000d57e4      634f3100       st.w r9, 48[sp]                         ; 0x30
0x000d57e8      234f3100       ld.w 48[sp], r9                         ; 0x30
0x000d57ec      00c8           mov r0, r25
0x000d57ee      e0c1           cmp r0, r24
0x000d57f0      da05           bne 0xd57fa
0x000d57f2      1956feff       addi -2, r25, r10
0x000d57f6      0ac8           mov r10, r25
0x000d57f8      f900           sxh r25
0x000d57fa      08771300       ld.b 19[r8], r14                        ; 0x13
0x000d57fe      200e7fff       movea -129, r0, r1
0x000d5802      4171           and r1, r14
0x000d5804      0e60           mov r14, r12
0x000d5806      5461           and r20, r12
0x000d5808      23d61400       movea 20, sp, r26
0x000d580c      23de0400       movea 4, sp, r27
0x000d5810      63370c00       st.h r6, 12[sp]                         ; 0xc
0x000d5814      087e1800       addi 24, r8, r15
0x000d5818      637f0500       st.w r15, 4[sp]                         ; 4
0x000d581c      634f1500       st.w r9, 20[sp]                         ; 0x14
0x000d5820      63071800       st.h r0, 24[sp]                         ; 0x18
0x000d5824      0058           mov r0, r11
0x000d5826      2d0634670900   mov 0x96734, r13
0x000d582c      ed670100       ld.hu 0[r13], r12
0x000d5830      486a           add 8, r13
0x000d5832      f561           cmp r21, r12
0x000d5834      aa05           bne 0xd5838
0x000d5836      0f5a           mov 15, r11
0x000d5838      fb870900       ld.hu 8[r27], r16
0x000d583c      f061           cmp r16, r12
0x000d583e      b205           be 0xd5844
0x000d5840      e059           cmp r0, r11
0x000d5842      d2f5           be 0xd582c
0x000d5844      586a           add -8, r13
0x000d5846      e059           cmp r0, r11
0x000d5848      ea15           bne 0xd5874
0x000d584a      2de70500       ld.w 4[r13], r28                        ; 4
0x000d584e      9cef0100       ld.bu 0[r28], r29
0x000d5852      4ce2           add 12, r28
0x000d5854      f4e9           cmp r20, r29
0x000d5856      8a0d           bne 0xd5866
0x000d5858      1a30           mov r26, r6
0x000d585a      043a           mov 4, r7
0x000d585c      1840           mov r24, r8
0x000d585e      bcffb645       jarl 0x99e14, lp
0x000d5862      0a58           mov r10, r11
0x000d5864      0f5a           mov 15, r11
0x000d5866      fdc1           cmp r29, r24
0x000d5868      b205           be 0xd586e
0x000d586a      e059           cmp r0, r11
0x000d586c      92f5           be 0xd584e
0x000d586e      54e2           add -12, r28
0x000d5870      63e70100       st.w r28, 0[sp]
0x000d5874      e059           cmp r0, r11
0x000d5876      da2d           bne 0xd58d0
0x000d5878      23e70100       ld.w 0[sp], r28
0x000d587c      bcef0100       ld.bu 1[r28], r29
0x000d5880      dd8ef000       andi 240, r29, r17
0x000d5884      e089           cmp r0, r17
0x000d5886      9215           be 0xd58a8
0x000d5888      3c670500       ld.w 4[r28], r12                        ; 4
0x000d588c      dd96f000       andi 240, r29, r18
0x000d5890      200ed000       movea 208, r0, r1
0x000d5894      e191           cmp r1, r18
0x000d5896      9a0d           bne 0xd58a8
0x000d5898      1a30           mov r26, r6
0x000d589a      1b38           mov r27, r7
0x000d589c      450e0d00       movhi 13, tp, r1
0x000d58a0      21fea658       movea 22694, r1, lp
0x000d58a4      6c00           jmp [r12]
0x000d58a6      0a58           mov r10, r11
0x000d58a8      e059           cmp r0, r11
0x000d58aa      ba15           bne 0xd58d0
0x000d58ac      dd9e0f00       andi 15, r29, r19
0x000d58b0      e099           cmp r0, r19
0x000d58b2      f20d           be 0xd58d0
0x000d58b4      3c670900       ld.w 8[r28], r12
0x000d58b8      dd760f00       andi 15, r29, r14
0x000d58bc      6272           cmp 2, r14
0x000d58be      9a0d           bne 0xd58d0
0x000d58c0      1a30           mov r26, r6
0x000d58c2      1b38           mov r27, r7
0x000d58c4      450e0d00       movhi 13, tp, r1
0x000d58c8      21fece58       movea 22734, r1, lp
0x000d58cc      6c00           jmp [r12]
0x000d58ce      0a58           mov r10, r11
0x000d58d0      6c5a           cmp 12, r11
0x000d58d2      ca05           bne 0xd58da
0x000d58d4      facf0500       ld.hu 4[r26], r25                       ; 4
0x000d58d8      f900           sxh r25
0x000d58da      1958           mov r25, r11
0x000d58dc      0b50           mov r11, r10
0x000d58de      5a06fff0       dispose 13, {r24, r25, r26, r27, r28, r29, lp}, lp
0x000d58e2      9a07e1f0       prepare {r24, r25, r26, r27, r28, r29, lp}, 13
0x000d58e6      bf07e6fe       jr 0xd57cc
0x000d58ea      0000           nop
