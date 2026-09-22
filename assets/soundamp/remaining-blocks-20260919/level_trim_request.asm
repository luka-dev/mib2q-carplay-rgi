0x000c7870      63370100       st.w r6, 0[sp]
0x000c7874      23ef0100       ld.w 0[sp], r29
0x000c7878      633f0500       st.w r7, 4[sp]                          ; 4
0x000c787c      23e70500       ld.w 4[sp], r28                         ; 4
0x000c7880      3c570100       ld.w 0[r28], r10
0x000c7884      0a7f0000       ld.b 0[r10], r15
0x000c7888      e079           cmp r0, r15
0x000c788a      8a15           bne 0xc78aa
0x000c788c      3c870400       ld.h 4[r28], r16                        ; 4
0x000c7890      6982           cmp 9, r16
0x000c7892      920d           be 0xc78a4
0x000c7894      1d30           mov r29, r6
0x000c7896      053a           mov 5, r7
0x000c7898      bdff5025       jarl 0x99de8, lp
0x000c789c      0a58           mov r10, r11
0x000c789e      0f5a           mov 15, r11
0x000c78a0      8007de01       jr 0xc7a7e
0x000c78a4      0030           mov r0, r6
0x000c78a6      076a           mov 7, r13
0x000c78a8      c54d           br 0xc7940
0x000c78aa      3c8f0100       ld.w 0[r28], r17
0x000c78ae      91970100       ld.bu 0[r17], r18
0x000c78b2      f491           cmp r20, r18
0x000c78b4      8a15           bne 0xc78d4
0x000c78b6      3c9f0400       ld.h 4[r28], r19                        ; 4
0x000c78ba      639a           cmp 3, r19
0x000c78bc      920d           be 0xc78ce
0x000c78be      1d30           mov r29, r6
0x000c78c0      053a           mov 5, r7
0x000c78c2      bdff2625       jarl 0x99de8, lp
0x000c78c6      0a58           mov r10, r11
0x000c78c8      0f5a           mov 15, r11
0x000c78ca      8007b401       jr 0xc7a7e
0x000c78ce      0030           mov r0, r6
0x000c78d0      076a           mov 7, r13
0x000c78d2      f535           br 0xc7940
0x000c78d4      3c7f0400       ld.h 4[r28], r15                        ; 4
0x000c78d8      637a           cmp 3, r15
0x000c78da      920d           be 0xc78ec
0x000c78dc      1d30           mov r29, r6
0x000c78de      053a           mov 5, r7
0x000c78e0      bdff0825       jarl 0x99de8, lp
0x000c78e4      0a58           mov r10, r11
0x000c78e6      0f5a           mov 15, r11
0x000c78e8      80079601       jr 0xc7a7e
0x000c78ec      0058           mov r0, r11
0x000c78ee      3c870100       ld.w 0[r28], r16
0x000c78f2      908f0100       ld.bu 0[r16], r17
0x000c78f6      400e0900       movhi 9, r0, r1
0x000c78fa      81972d6e       ld.bu 28204[r1], r18                    ; 0x6e2c
0x000c78fe      f289           cmp r18, r17
0x000c7900      e215           be 0xc792c
0x000c7902      675a           cmp 7, r11
0x000c7904      ce15           bge 0xc792c
0x000c7906      2c062c6e0900   mov 0x96e2c, r12
0x000c790c      0b9e0100       addi 1, r11, r19
0x000c7910      1358           mov r19, r11
0x000c7912      5459           and r20, r11
0x000c7914      0c78           mov r12, r15
0x000c7916      cb79           add r11, r15
0x000c7918      8f570100       ld.bu 0[r15], r10
0x000c791c      3c870100       ld.w 0[r28], r16
0x000c7920      908f0100       ld.bu 0[r16], r17
0x000c7924      f151           cmp r17, r10
0x000c7926      b205           be 0xc792c
0x000c7928      675a           cmp 7, r11
0x000c792a      96f5           blt 0xc790c
0x000c792c      675a           cmp 7, r11
0x000c792e      c605           blt 0xc7936
0x000c7930      0058           mov r0, r11
0x000c7932      80074c01       jr 0xc7a7e
0x000c7936      0b30           mov r11, r6
0x000c7938      06960100       addi 1, r6, r18
0x000c793c      1268           mov r18, r13
0x000c793e      5469           and r20, r13
0x000c7940      023a           mov 2, r7
0x000c7942      0758           mov r7, r11
0x000c7944      0660           mov r6, r12
0x000c7946      ed31           cmp r13, r6
0x000c7948      d935           bnl 0xc79b2
0x000c794a      3c9f0100       ld.w 0[r28], r19
0x000c794e      937f0100       ld.bu 0[r19], r15
0x000c7952      f479           cmp r20, r15
0x000c7954      aa05           bne 0xc7958
0x000c7956      0758           mov r7, r11
0x000c7958      3c870100       ld.w 0[r28], r16
0x000c795c      cb81           add r11, r16
0x000c795e      90770100       ld.bu 0[r16], r14
0x000c7962      ae00           sxb r14
0x000c7964      7372           cmp -13, r14
0x000c7966      b605           blt 0xc796c
0x000c7968      6d72           cmp 13, r14
0x000c796a      a71d           ble 0xc799e
0x000c796c      3c570100       ld.w 0[r28], r10
0x000c7970      cb51           add r11, r10
0x000c7972      8a8f0100       ld.bu 0[r10], r17
0x000c7976      200e8000       movea 128, r0, r1
0x000c797a      e189           cmp r1, r17
0x000c797c      9215           be 0xc799e
0x000c797e      1d30           mov r29, r6
0x000c7980      0b960100       addi 1, r11, r18
0x000c7984      5491           and r20, r18
0x000c7986      1238           mov r18, r7
0x000c7988      3c9f0100       ld.w 0[r28], r19
0x000c798c      cb99           add r11, r19
0x000c798e      1340           mov r19, r8
0x000c7990      014a           mov 1, r9
0x000c7992      bdff2a25       jarl 0x99ebc, lp
0x000c7996      0a58           mov r10, r11
0x000c7998      0f5a           mov 15, r11
0x000c799a      8007e400       jr 0xc7a7e
0x000c799e      0b7e0100       addi 1, r11, r15
0x000c79a2      0f58           mov r15, r11
0x000c79a4      5459           and r20, r11
0x000c79a6      0c860100       addi 1, r12, r16
0x000c79aa      1060           mov r16, r12
0x000c79ac      5461           and r20, r12
0x000c79ae      ed61           cmp r13, r12
0x000c79b0      d1cd           bl 0xc794a
0x000c79b2      0758           mov r7, r11
0x000c79b4      20ee5a00       movea 90, r0, r29                       ; 'Z'
0x000c79b8      0070           mov r0, r14
0x000c79ba      0660           mov r6, r12
0x000c79bc      ed31           cmp r13, r6
0x000c79be      e93d           bnl 0xc7a3a
0x000c79c0      3c8f0100       ld.w 0[r28], r17
0x000c79c4      91970100       ld.bu 0[r17], r18
0x000c79c8      f491           cmp r20, r18
0x000c79ca      aa05           bne 0xc79ce
0x000c79cc      0758           mov r7, r11
0x000c79ce      33062c6e0900   mov 0x96e2c, r19
0x000c79d4      cc99           add r12, r19
0x000c79d6      13570000       ld.b 0[r19], r10
0x000c79da      0a56dfff       addi -33, r10, r10
0x000c79de      0a70           mov r10, r14
0x000c79e0      5471           and r20, r14
0x000c79e2      3c7f0100       ld.w 0[r28], r15
0x000c79e6      cb79           add r11, r15
0x000c79e8      8f870100       ld.bu 0[r15], r16
0x000c79ec      200e8000       movea 128, r0, r1
0x000c79f0      e181           cmp r1, r16
0x000c79f2      a21d           be 0xc7a26
0x000c79f4      243670c5       movea -14992, gp, r6
0x000c79f8      3c8f0100       ld.w 0[r28], r17
0x000c79fc      cb89           add r11, r17
0x000c79fe      91970100       ld.bu 0[r17], r18
0x000c7a02      b200           sxb r18
0x000c7a04      0698           mov r6, r19
0x000c7a06      ce99           add r14, r19
0x000c7a08      13570000       ld.b 0[r19], r10
0x000c7a0c      ea91           cmp r10, r18
0x000c7a0e      b205           be 0xc7a14
0x000c7a10      20eea500       movea 165, r0, r29
0x000c7a14      0678           mov r6, r15
0x000c7a16      ce79           add r14, r15
0x000c7a18      3c870100       ld.w 0[r28], r16
0x000c7a1c      cb81           add r11, r16
0x000c7a1e      108f0000       ld.b 0[r16], r17
0x000c7a22      4f8f0000       st.b r17, 0[r15]
0x000c7a26      0b9e0100       addi 1, r11, r19
0x000c7a2a      1358           mov r19, r11
0x000c7a2c      5459           and r20, r11
0x000c7a2e      0c960100       addi 1, r12, r18
0x000c7a32      1260           mov r18, r12
0x000c7a34      5461           and r20, r12
0x000c7a36      ed61           cmp r13, r12
0x000c7a38      c1c5           bl 0xc79c0
0x000c7a3a      0332           mov 3, r6
0x000c7a3c      203e4f00       movea 79, r0, r7                        ; 'O'
0x000c7a40      245670c5       movea -14992, gp, r10
0x000c7a44      ce51           add r14, r10
0x000c7a46      0a870000       ld.b 0[r10], r16
0x000c7a4a      1040           mov r16, r8
0x000c7a4c      0e48           mov r14, r9
0x000c7a4e      82ffa228       jarl 0xea2f0, lp
0x000c7a52      200ea500       movea 165, r0, r1
0x000c7a56      e1e9           cmp r1, r29
0x000c7a58      ca0d           bne 0xc7a70
0x000c7a5a      3c7f0100       ld.w 0[r28], r15
0x000c7a5e      0f8f0000       ld.b 0[r15], r17
0x000c7a62      448f21f3       st.b r17, -3295[gp]
0x000c7a66      243692f1       movea -3694, gp, r6
0x000c7a6a      013a           mov 1, r7
0x000c7a6c      81ffa043       jarl 0xdbe0c, lp
0x000c7a70      bfff74ee       jarl 0xc68e4, lp
0x000c7a74      0132           mov 1, r6
0x000c7a76      0b3a           mov 11, r7
0x000c7a78      81ff5c58       jarl 0xdd2d4, lp
0x000c7a7c      0058           mov r0, r11
0x000c7a7e      0b50           mov r11, r10
0x000c7a80      4406ff00       dispose 2, {r28, r29, lp}, lp
0x000c7a84      8407e100       prepare {r28, r29, lp}, 2
0x000c7a88      bf07e8fd       jr 0xc7870
