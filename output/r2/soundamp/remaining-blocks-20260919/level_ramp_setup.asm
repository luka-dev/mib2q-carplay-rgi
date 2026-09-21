0x000af134      20565a00       movea 90, r0, r10                       ; 'Z'
0x000af138      0ab0           mov r10, r22
0x000af13a      047f8fba       ld.b -17777[gp], r15
0x000af13e      0487bd87       ld.b -30787[gp], r16
0x000af142      f079           cmp r16, r15
0x000af144      c245           be 0xaf1cc
0x000af146      048f8fba       ld.b -17777[gp], r17
0x000af14a      e089           cmp r0, r17
0x000af14c      d205           be 0xaf156
0x000af14e      04978fba       ld.b -17777[gp], r18
0x000af152      6892           cmp 8, r18
0x000af154      820d           be 0xaf164
0x000af156      049f8fba       ld.b -17777[gp], r19
0x000af15a      e099           cmp r0, r19
0x000af15c      aa0d           bne 0xaf170
0x000af15e      c4dfbd87       tst1 3, -30787[gp]
0x000af162      f205           be 0xaf170
0x000af164      067a           mov 6, r15
0x000af166      447faec3       st.b r15, -15442[gp]
0x000af16a      4457bc87       st.b r10, -30788[gp]
0x000af16e      c505           br 0xaf176
0x000af170      0682           mov 6, r16
0x000af172      4487aec3       st.b r16, -15442[gp]
0x000af176      048f8fba       ld.b -17777[gp], r17
0x000af17a      e089           cmp r0, r17
0x000af17c      a215           be 0xaf1a0
0x000af17e      24970988       ld.w -30712[gp], r18
0x000af182      400e0001       movhi 256, r0, r1
0x000af186      e197c002       div r1, r18, r0
0x000af18a      049f0088       ld.b -30720[gp], r19
0x000af18e      b391           sub r19, r18
0x000af190      ea974002       mul 10, r18, r0
0x000af194      040a           mov 4, r1
0x000af196      e197c002       div r1, r18, r0
0x000af19a      6497acc3       st.h r18, -15444[gp]
0x000af19e      9515           br 0xaf1c0
0x000af1a0      247f0988       ld.w -30712[gp], r15
0x000af1a4      400e0001       movhi 256, r0, r1
0x000af1a8      e17fc002       div r1, r15, r0
0x000af1ac      04870088       ld.b -30720[gp], r16
0x000af1b0      af81           sub r15, r16
0x000af1b2      ea874002       mul 10, r16, r0
0x000af1b6      040a           mov 4, r1
0x000af1b8      e187c002       div r1, r16, r0
0x000af1bc      6487acc3       st.h r16, -15444[gp]
0x000af1c0      20b6a500       movea 165, r0, r22
0x000af1c4      048f8fba       ld.b -17777[gp], r17
0x000af1c8      448fbd87       st.b r17, -30787[gp]
0x000af1cc      0050           mov r0, r10
0x000af1ce      24660088       movea -30720, gp, r12
0x000af1d2      24760488       movea -30716, gp, r14
0x000af1d6      0c98           mov r12, r19
0x000af1d8      ca99           add r10, r19
0x000af1da      13970000       ld.b 0[r19], r18
0x000af1de      0e78           mov r14, r15
0x000af1e0      ca79           add r10, r15
0x000af1e2      0f870000       ld.b 0[r15], r16
0x000af1e6      f091           cmp r16, r18
0x000af1e8      fa05           bne 0xaf1f6
0x000af1ea      200ea500       movea 165, r0, r1
0x000af1ee      e1b1           cmp r1, r22
0x000af1f0      b205           be 0xaf1f6
0x000af1f2      80079e02       jr 0xaf490
0x000af1f6      24e60888       movea -30712, gp, r28
0x000af1fa      24defc87       movea -30724, gp, r27
0x000af1fe      40d60001       movhi 256, r0, r26
0x000af202      245eacc3       movea -15444, gp, r11
0x000af206      0e88           mov r14, r17
0x000af208      ca89           add r10, r17
0x000af20a      0c98           mov r12, r19
0x000af20c      ca99           add r10, r19
0x000af20e      137f0000       ld.b 0[r19], r15
0x000af212      517f0000       st.b r15, 0[r17]
0x000af216      0a68           mov r10, r13
0x000af218      c26a           shl 2, r13
0x000af21a      1b90           mov r27, r18
0x000af21c      ca91           add r10, r18
0x000af21e      1c80           mov r28, r16
0x000af220      cd81           add r13, r16
0x000af222      309f0100       ld.w 0[r16], r19
0x000af226      fa9fc002       div r26, r19, r0
0x000af22a      529f0000       st.b r19, 0[r18]
0x000af22e      248e1888       movea -30696, gp, r17
0x000af232      cd89           add r13, r17
0x000af234      247f35a1       ld.w -24268[gp], r15
0x000af238      717f0100       st.w r15, 0[r17]
0x000af23c      0b80           mov r11, r16
0x000af23e      cd81           add r13, r16
0x000af240      10970200       ld.b 2[r16], r18
0x000af244      e091           cmp r0, r18
0x000af246      e215           be 0xaf272
0x000af248      0a98           mov r10, r19
0x000af24a      c29a           shl 2, r19
0x000af24c      cb99           add r11, r19
0x000af24e      138f0200       ld.b 2[r19], r17
0x000af252      628a           cmp 2, r17
0x000af254      820d           be 0xaf264
0x000af256      0a78           mov r10, r15
0x000af258      c27a           shl 2, r15
0x000af25a      cb79           add r11, r15
0x000af25c      0f870200       ld.b 2[r15], r16
0x000af260      6182           cmp 1, r16
0x000af262      ca2d           bne 0xaf2ba
0x000af264      0a90           mov r10, r18
0x000af266      c292           shl 2, r18
0x000af268      cb91           add r11, r18
0x000af26a      329f0000       ld.h 0[r18], r19
0x000af26e      e099           cmp r0, r19
0x000af270      da25           bne 0xaf2ba
0x000af272      0a88           mov r10, r17
0x000af274      c28a           shl 2, r17
0x000af276      cb89           add r11, r17
0x000af278      71070000       st.h r0, 0[r17]
0x000af27c      0c78           mov r12, r15
0x000af27e      ca79           add r10, r15
0x000af280      0f870000       ld.b 0[r15], r16
0x000af284      1b90           mov r27, r18
0x000af286      ca91           add r10, r18
0x000af288      129f0000       ld.b 0[r18], r19
0x000af28c      f381           cmp r19, r16
0x000af28e      b70d           ble 0xaf2a4
0x000af290      0a88           mov r10, r17
0x000af292      c28a           shl 2, r17
0x000af294      247eec87       movea -30740, gp, r15
0x000af298      d179           add r17, r15
0x000af29a      4096001a       movhi 6656, r0, r18
0x000af29e      6f970100       st.w r18, 0[r15]
0x000af2a2      a50d           br 0xaf2b6
0x000af2a4      0a80           mov r10, r16
0x000af2a6      c282           shl 2, r16
0x000af2a8      249eec87       movea -30740, gp, r19
0x000af2ac      d099           add r16, r19
0x000af2ae      408e00e6       movhi -6656, r0, r17
0x000af2b2      738f0100       st.w r17, 0[r19]
0x000af2b6      8007da01       jr 0xaf490
0x000af2ba      0a78           mov r10, r15
0x000af2bc      c27a           shl 2, r15
0x000af2be      cb79           add r11, r15
0x000af2c0      0f970200       ld.b 2[r15], r18
0x000af2c4      6292           cmp 2, r18
0x000af2c6      8a2d           bne 0xaf316
0x000af2c8      0a80           mov r10, r16
0x000af2ca      c282           shl 2, r16
0x000af2cc      cb81           add r11, r16
0x000af2ce      309f0000       ld.h 0[r16], r19
0x000af2d2      200ed007       movea 2000, r0, r1
0x000af2d6      e199           cmp r1, r19
0x000af2d8      c305           bnh 0xaf2e0
0x000af2da      206ed007       movea 2000, r0, r13
0x000af2de      e505           br 0xaf2ea
0x000af2e0      0a88           mov r10, r17
0x000af2e2      c28a           shl 2, r17
0x000af2e4      cb89           add r11, r17
0x000af2e6      f16f0100       ld.hu 0[r17], r13
0x000af2ea      0a78           mov r10, r15
0x000af2ec      c27a           shl 2, r15
0x000af2ee      0f30           mov r15, r6
0x000af2f0      2496ec87       movea -30740, gp, r18
0x000af2f4      c691           add r6, r18
0x000af2f6      0c80           mov r12, r16
0x000af2f8      ca81           add r10, r16
0x000af2fa      109f0000       ld.b 0[r16], r19
0x000af2fe      d89a           shl 24, r19
0x000af300      1c88           mov r28, r17
0x000af302      c689           add r6, r17
0x000af304      317f0100       ld.w 0[r17], r15
0x000af308      af99           sub r15, r19
0x000af30a      ed9fc002       div r13, r19, r0
0x000af30e      729f0100       st.w r19, 0[r18]
0x000af312      80077e01       jr 0xaf490
0x000af316      0a80           mov r10, r16
0x000af318      c282           shl 2, r16
0x000af31a      cb81           add r11, r16
0x000af31c      108f0200       ld.b 2[r16], r17
0x000af320      658a           cmp 5, r17
0x000af322      aa25           bne 0xaf366
0x000af324      0c78           mov r12, r15
0x000af326      ca79           add r10, r15
0x000af328      0f970000       ld.b 0[r15], r18
0x000af32c      1b98           mov r27, r19
0x000af32e      ca99           add r10, r19
0x000af330      13870000       ld.b 0[r19], r16
0x000af334      f091           cmp r16, r18
0x000af336      c70d           ble 0xaf34e
0x000af338      0a88           mov r10, r17
0x000af33a      c28a           shl 2, r17
0x000af33c      247eec87       movea -30740, gp, r15
0x000af340      d179           add r17, r15
0x000af342      330699991900   mov 0x199999, r19
0x000af348      6f9f0100       st.w r19, 0[r15]
0x000af34c      b50d           br 0xaf362
0x000af34e      0a90           mov r10, r18
0x000af350      c292           shl 2, r18
0x000af352      2486ec87       movea -30740, gp, r16
0x000af356      d281           add r18, r16
0x000af358      31066766e6ff   mov 0xffe66667, r17
0x000af35e      708f0100       st.w r17, 0[r16]
0x000af362      80072e01       jr 0xaf490
0x000af366      0a78           mov r10, r15
0x000af368      c27a           shl 2, r15
0x000af36a      cb79           add r11, r15
0x000af36c      0f9f0200       ld.b 2[r15], r19
0x000af370      669a           cmp 6, r19
0x000af372      9a25           bne 0xaf3b4
0x000af374      0c90           mov r12, r18
0x000af376      ca91           add r10, r18
0x000af378      12870000       ld.b 0[r18], r16
0x000af37c      1b88           mov r27, r17
0x000af37e      ca89           add r10, r17
0x000af380      117f0000       ld.b 0[r17], r15
0x000af384      ef81           cmp r15, r16
0x000af386      c70d           ble 0xaf39e
0x000af388      0a98           mov r10, r19
0x000af38a      c29a           shl 2, r19
0x000af38c      2496ec87       movea -30740, gp, r18
0x000af390      d391           add r19, r18
0x000af392      310666666600   mov 0x666666, r17                       ; 'fff'
0x000af398      728f0100       st.w r17, 0[r18]
0x000af39c      b50d           br 0xaf3b2
0x000af39e      0a80           mov r10, r16
0x000af3a0      c282           shl 2, r16
0x000af3a2      247eec87       movea -30740, gp, r15
0x000af3a6      d079           add r16, r15
0x000af3a8      33069a9999ff   mov 0xff99999a, r19
0x000af3ae      6f9f0100       st.w r19, 0[r15]
0x000af3b2      f56d           br 0xaf490
0x000af3b4      0a90           mov r10, r18
0x000af3b6      c292           shl 2, r18
0x000af3b8      cb91           add r11, r18
0x000af3ba      128f0200       ld.b 2[r18], r17
0x000af3be      678a           cmp 7, r17
0x000af3c0      9a25           bne 0xaf402
0x000af3c2      0c80           mov r12, r16
0x000af3c4      ca81           add r10, r16
0x000af3c6      107f0000       ld.b 0[r16], r15
0x000af3ca      1b98           mov r27, r19
0x000af3cc      ca99           add r10, r19
0x000af3ce      13970000       ld.b 0[r19], r18
0x000af3d2      f279           cmp r18, r15
0x000af3d4      c70d           ble 0xaf3ec
0x000af3d6      0a88           mov r10, r17
0x000af3d8      c28a           shl 2, r17
0x000af3da      2486ec87       movea -30740, gp, r16
0x000af3de      d181           add r17, r16
0x000af3e0      3306cccccc00   mov 0xcccccc, r19
0x000af3e6      709f0100       st.w r19, 0[r16]
0x000af3ea      b50d           br 0xaf400
0x000af3ec      0a78           mov r10, r15
0x000af3ee      c27a           shl 2, r15
0x000af3f0      2496ec87       movea -30740, gp, r18
0x000af3f4      cf91           add r15, r18
0x000af3f6      3106343333ff   mov 0xff333334, r17
0x000af3fc      728f0100       st.w r17, 0[r18]
0x000af400      854d           br 0xaf490
0x000af402      0a80           mov r10, r16
0x000af404      c282           shl 2, r16
0x000af406      cb81           add r11, r16
0x000af408      309f0000       ld.h 0[r16], r19
0x000af40c      200ed007       movea 2000, r0, r1
0x000af410      e199           cmp r1, r19
0x000af412      c305           bnh 0xaf41a
0x000af414      206ed007       movea 2000, r0, r13
0x000af418      e505           br 0xaf424
0x000af41a      0a78           mov r10, r15
0x000af41c      c27a           shl 2, r15
0x000af41e      cb79           add r11, r15
0x000af420      ef6f0100       ld.hu 0[r15], r13
0x000af424      3d06588d0600   mov 0x68d58, r29
0x000af42a      0c90           mov r12, r18
0x000af42c      ca91           add r10, r18
0x000af42e      128f0000       ld.b 0[r18], r17
0x000af432      e88f440a       mul 40, r17, r1
0x000af436      3188           not r17, r17
0x000af438      418a           add 1, r17
0x000af43a      dd89           add r29, r17
0x000af43c      31bf4106       ld.w 1600[r17], r23                     ; 0x640 ; 1600
0x000af440      0ac0           mov r10, r24
0x000af442      c2c2           shl 2, r24
0x000af444      1c80           mov r28, r16
0x000af446      d881           add r24, r16
0x000af448      309f0100       ld.w 0[r16], r19
0x000af44c      fa9fc002       div r26, r19, r0
0x000af450      e89f4402       mul 40, r19, r0
0x000af454      3398           not r19, r19
0x000af456      419a           add 1, r19
0x000af458      dd99           add r29, r19
0x000af45a      337f4106       ld.w 1600[r19], r15                     ; 0x640 ; 1600
0x000af45e      17c8           mov r23, r25
0x000af460      0f30           mov r15, r6
0x000af462      1938           mov r25, r7
0x000af464      83ff0cc7       jarl 0xebb70, lp
0x000af468      06c8           mov r6, r25
0x000af46a      2496ec87       movea -30740, gp, r18
0x000af46e      d891           add r24, r18
0x000af470      1988           mov r25, r17
0x000af472      4086804b       movhi 19328, r0, r16
0x000af476      1030           mov r16, r6
0x000af478      1138           mov r17, r7
0x000af47a      83ff0ac5       jarl 0xeb984, lp
0x000af47e      0688           mov r6, r17
0x000af480      1130           mov r17, r6
0x000af482      83ff00c7       jarl 0xebb82, lp
0x000af486      0688           mov r6, r17
0x000af488      ed8fc002       div r13, r17, r0
0x000af48c      728f0100       st.w r17, 0[r18]
0x000af490      0a9e0100       addi 1, r10, r19
0x000af494      1350           mov r19, r10
0x000af496      5451           and r20, r10
0x000af498      6452           cmp 4, r10
0x000af49a      be05           bge 0xaf4a0
0x000af49c      bf073afd       jr 0xaf1d6
0x000af4a0      4006fff3       dispose 0, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000af4a4      8007e1f3       prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 0
0x000af4a8      bf078cfc       jr 0xaf134
