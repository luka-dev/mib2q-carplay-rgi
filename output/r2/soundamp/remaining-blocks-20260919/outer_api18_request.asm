0x000ca4b4      80070004       jr 0xca8b4
0x000ca4b8      63370500       st.w r6, 4[sp]                          ; 4
0x000ca4bc      23bf0500       ld.w 4[sp], r23                         ; 4
0x000ca4c0      633f0900       st.w r7, 8[sp]
0x000ca4c4      23c70900       ld.w 8[sp], r24
0x000ca4c8      00c8           mov r0, r25
0x000ca4ca      a4d795ba       ld.bu -17771[gp], r26
0x000ca4ce      84df97ba       ld.bu -17770[gp], r27
0x000ca4d2      a4e771be       ld.bu -16783[gp], r28
0x000ca4d6      0332           mov 3, r6
0x000ca4d8      203e1901       movea 281, r0, r7
0x000ca4dc      0040           mov r0, r8
0x000ca4de      0048           mov r0, r9
0x000ca4e0      81ff10fe       jarl 0xea2f0, lp
0x000ca4e4      38570100       ld.w 0[r24], r10
0x000ca4e8      0a6f0000       ld.b 0[r10], r13
0x000ca4ec      e069           cmp r0, r13
0x000ca4ee      ea0d           bne 0xca50a
0x000ca4f0      38770400       ld.h 4[r24], r14                        ; 4
0x000ca4f4      6872           cmp 8, r14
0x000ca4f6      920d           be 0xca508
0x000ca4f8      1730           mov r23, r6
0x000ca4fa      053a           mov 5, r7
0x000ca4fc      bcffecf8       jarl 0x99de8, lp
0x000ca500      0a58           mov r10, r11
0x000ca502      0f5a           mov 15, r11
0x000ca504      8007aa03       jr 0xca8ae
0x000ca508      d525           br 0xca552
0x000ca50a      387f0400       ld.h 4[r24], r15                        ; 4
0x000ca50e      637a           cmp 3, r15
0x000ca510      920d           be 0xca522
0x000ca512      1730           mov r23, r6
0x000ca514      053a           mov 5, r7
0x000ca516      bcffd2f8       jarl 0x99de8, lp
0x000ca51a      0a58           mov r10, r11
0x000ca51c      0f5a           mov 15, r11
0x000ca51e      80079003       jr 0xca8ae
0x000ca522      38870100       ld.w 0[r24], r16
0x000ca526      108f0000       ld.b 0[r16], r17
0x000ca52a      618a           cmp 1, r17
0x000ca52c      f105           bl 0xca53a
0x000ca52e      38970100       ld.w 0[r24], r18
0x000ca532      129f0000       ld.b 0[r18], r19
0x000ca536      669a           cmp 6, r19
0x000ca538      d30d           bnh 0xca552
0x000ca53a      1730           mov r23, r6
0x000ca53c      013a           mov 1, r7
0x000ca53e      386f0100       ld.w 0[r24], r13
0x000ca542      0d40           mov r13, r8
0x000ca544      014a           mov 1, r9
0x000ca546      bcff76f9       jarl 0x99ebc, lp
0x000ca54a      0a58           mov r10, r11
0x000ca54c      0f5a           mov 15, r11
0x000ca54e      80076003       jr 0xca8ae
0x000ca552      20b65a00       movea 90, r0, r22                       ; 'Z'
0x000ca556      025a           mov 2, r11
0x000ca558      16e8           mov r22, r29
0x000ca55a      38770100       ld.w 0[r24], r14
0x000ca55e      0e7f0000       ld.b 0[r14], r15
0x000ca562      e079           cmp r0, r15
0x000ca564      f205           be 0xca572
0x000ca566      38870100       ld.w 0[r24], r16
0x000ca56a      108f0000       ld.b 0[r16], r17
0x000ca56e      618a           cmp 1, r17
0x000ca570      ca1d           bne 0xca5a8
0x000ca572      38970100       ld.w 0[r24], r18
0x000ca576      cb91           add r11, r18
0x000ca578      92670100       ld.bu 0[r18], r12
0x000ca57c      0b9e0100       addi 1, r11, r19
0x000ca580      1358           mov r19, r11
0x000ca582      5459           and r20, r11
0x000ca584      43670300       st.b r12, 3[sp]
0x000ca588      e061           cmp r0, r12
0x000ca58a      f20d           be 0xca5a8
0x000ca58c      1730           mov r23, r6
0x000ca58e      0b6e0100       addi 1, r11, r13
0x000ca592      5469           and r20, r13
0x000ca594      0d38           mov r13, r7
0x000ca596      23460300       movea 3, sp, r8
0x000ca59a      014a           mov 1, r9
0x000ca59c      bcff20f9       jarl 0x99ebc, lp
0x000ca5a0      0a58           mov r10, r11
0x000ca5a2      0f5a           mov 15, r11
0x000ca5a4      80070a03       jr 0xca8ae
0x000ca5a8      38770100       ld.w 0[r24], r14
0x000ca5ac      0e7f0000       ld.b 0[r14], r15
0x000ca5b0      e079           cmp r0, r15
0x000ca5b2      f205           be 0xca5c0
0x000ca5b4      38870100       ld.w 0[r24], r16
0x000ca5b8      108f0000       ld.b 0[r16], r17
0x000ca5bc      628a           cmp 2, r17
0x000ca5be      fa25           bne 0xca60c
0x000ca5c0      38970100       ld.w 0[r24], r18
0x000ca5c4      cb91           add r11, r18
0x000ca5c6      92670100       ld.bu 0[r18], r12
0x000ca5ca      0b9e0100       addi 1, r11, r19
0x000ca5ce      1358           mov r19, r11
0x000ca5d0      5459           and r20, r11
0x000ca5d2      43670300       st.b r12, 3[sp]
0x000ca5d6      6162           cmp 1, r12
0x000ca5d8      a61d           blt 0xca60c
0x000ca5da      43670300       st.b r12, 3[sp]
0x000ca5de      6862           cmp 8, r12
0x000ca5e0      ef15           bgt 0xca60c
0x000ca5e2      846f6fba       ld.bu -17810[gp], r13
0x000ca5e6      f56f400a       mul 21, r13, r1
0x000ca5ea      2a0608001000   mov 0x100008, r10
0x000ca5f0      cd51           add r13, r10
0x000ca5f2      cc51           add r12, r10
0x000ca5f4      0a770000       ld.b 0[r10], r14
0x000ca5f8      7f72           cmp -1, r14
0x000ca5fa      920d           be 0xca60c
0x000ca5fc      847fe7c5       ld.bu -14874[gp], r15
0x000ca600      ef61           cmp r15, r12
0x000ca602      b205           be 0xca608
0x000ca604      20eea500       movea 165, r0, r29
0x000ca608      4467e6c5       st.b r12, -14874[gp]
0x000ca60c      38870100       ld.w 0[r24], r16
0x000ca610      108f0000       ld.b 0[r16], r17
0x000ca614      e089           cmp r0, r17
0x000ca616      f205           be 0xca624
0x000ca618      38970100       ld.w 0[r24], r18
0x000ca61c      129f0000       ld.b 0[r18], r19
0x000ca620      639a           cmp 3, r19
0x000ca622      aa25           bne 0xca666
0x000ca624      386f0100       ld.w 0[r24], r13
0x000ca628      cb69           add r11, r13
0x000ca62a      cdc70000       tst1 0, 0[r13]
0x000ca62e      e20d           be 0xca64a
0x000ca630      045798ba       ld.b -17768[gp], r10
0x000ca634      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000ca638      e151           cmp r1, r10
0x000ca63a      ba05           bne 0xca640
0x000ca63c      20eea500       movea 165, r0, r29
0x000ca640      2076a500       movea 165, r0, r14
0x000ca644      447798ba       st.b r14, -17768[gp]
0x000ca648      b50d           br 0xca65e
0x000ca64a      847f99ba       ld.bu -17768[gp], r15
0x000ca64e      200ea500       movea 165, r0, r1
0x000ca652      e179           cmp r1, r15
0x000ca654      ba05           bne 0xca65a
0x000ca656      20eea500       movea 165, r0, r29
0x000ca65a      44b798ba       st.b r22, -17768[gp]
0x000ca65e      0b860100       addi 1, r11, r16
0x000ca662      1058           mov r16, r11
0x000ca664      5459           and r20, r11
0x000ca666      388f0100       ld.w 0[r24], r17
0x000ca66a      11970000       ld.b 0[r17], r18
0x000ca66e      e091           cmp r0, r18
0x000ca670      f205           be 0xca67e
0x000ca672      389f0100       ld.w 0[r24], r19
0x000ca676      136f0000       ld.b 0[r19], r13
0x000ca67a      646a           cmp 4, r13
0x000ca67c      aa2d           bne 0xca6d0
0x000ca67e      38570100       ld.w 0[r24], r10
0x000ca682      cb51           add r11, r10
0x000ca684      8a670100       ld.bu 0[r10], r12
0x000ca688      6362           cmp 3, r12
0x000ca68a      9715           ble 0xca6ac
0x000ca68c      43670300       st.b r12, 3[sp]
0x000ca690      1730           mov r23, r6
0x000ca692      0b760100       addi 1, r11, r14
0x000ca696      5471           and r20, r14
0x000ca698      0e38           mov r14, r7
0x000ca69a      23460300       movea 3, sp, r8
0x000ca69e      014a           mov 1, r9
0x000ca6a0      bcff1cf8       jarl 0x99ebc, lp
0x000ca6a4      0a60           mov r10, r12
0x000ca6a6      0c58           mov r12, r11
0x000ca6a8      80070602       jr 0xca8ae
0x000ca6ac      43670300       st.b r12, 3[sp]
0x000ca6b0      847f39f3       ld.bu -3272[gp], r15
0x000ca6b4      ef61           cmp r15, r12
0x000ca6b6      920d           be 0xca6c8
0x000ca6b8      20eea500       movea 165, r0, r29
0x000ca6bc      446738f3       st.b r12, -3272[gp]
0x000ca6c0      446771be       st.b r12, -16783[gp]
0x000ca6c4      43670300       st.b r12, 3[sp]
0x000ca6c8      0b860100       addi 1, r11, r16
0x000ca6cc      1058           mov r16, r11
0x000ca6ce      5459           and r20, r11
0x000ca6d0      388f0100       ld.w 0[r24], r17
0x000ca6d4      11970000       ld.b 0[r17], r18
0x000ca6d8      e091           cmp r0, r18
0x000ca6da      f205           be 0xca6e8
0x000ca6dc      389f0100       ld.w 0[r24], r19
0x000ca6e0      136f0000       ld.b 0[r19], r13
0x000ca6e4      656a           cmp 5, r13
0x000ca6e6      da35           bne 0xca750
0x000ca6e8      38770100       ld.w 0[r24], r14
0x000ca6ec      cb71           add r11, r14
0x000ca6ee      8e670100       ld.bu 0[r14], r12
0x000ca6f2      ac00           sxb r12
0x000ca6f4      6c62           cmp 12, r12
0x000ca6f6      bf05           bgt 0xca6fc
0x000ca6f8      7462           cmp -12, r12
0x000ca6fa      9e15           bge 0xca71c
0x000ca6fc      1730           mov r23, r6
0x000ca6fe      0b560100       addi 1, r11, r10
0x000ca702      5451           and r20, r10
0x000ca704      0a38           mov r10, r7
0x000ca706      387f0100       ld.w 0[r24], r15
0x000ca70a      cb79           add r11, r15
0x000ca70c      0f40           mov r15, r8
0x000ca70e      014a           mov 1, r9
0x000ca710      bcffacf7       jarl 0x99ebc, lp
0x000ca714      0a60           mov r10, r12
0x000ca716      0c58           mov r12, r11
0x000ca718      80079601       jr 0xca8ae
0x000ca71c      048783c5       ld.b -14973[gp], r16
0x000ca720      f061           cmp r16, r12
0x000ca722      b205           be 0xca728
0x000ca724      20eea500       movea 165, r0, r29
0x000ca728      446783c5       st.b r12, -14973[gp]
0x000ca72c      7762           cmp -9, r12
0x000ca72e      de05           bge 0xca738
0x000ca730      178a           mov -9, r17
0x000ca732      448f20c2       st.b r17, -15840[gp]
0x000ca736      950d           br 0xca748
0x000ca738      6962           cmp 9, r12
0x000ca73a      d705           ble 0xca744
0x000ca73c      0992           mov 9, r18
0x000ca73e      449720c2       st.b r18, -15840[gp]
0x000ca742      b505           br 0xca748
0x000ca744      446720c2       st.b r12, -15840[gp]
0x000ca748      0b9e0100       addi 1, r11, r19
0x000ca74c      1358           mov r19, r11
0x000ca74e      5459           and r20, r11
0x000ca750      386f0100       ld.w 0[r24], r13
0x000ca754      0d770000       ld.b 0[r13], r14
0x000ca758      e071           cmp r0, r14
0x000ca75a      f205           be 0xca768
0x000ca75c      387f0100       ld.w 0[r24], r15
0x000ca760      0f570000       ld.b 0[r15], r10
0x000ca764      6652           cmp 6, r10
0x000ca766      da35           bne 0xca7d0
0x000ca768      38870100       ld.w 0[r24], r16
0x000ca76c      cb81           add r11, r16
0x000ca76e      90670100       ld.bu 0[r16], r12
0x000ca772      6962           cmp 9, r12
0x000ca774      9715           ble 0xca796
0x000ca776      43670200       st.b r12, 2[sp]
0x000ca77a      1730           mov r23, r6
0x000ca77c      0b8e0100       addi 1, r11, r17
0x000ca780      5489           and r20, r17
0x000ca782      1138           mov r17, r7
0x000ca784      23460200       movea 2, sp, r8
0x000ca788      014a           mov 1, r9
0x000ca78a      bcff32f7       jarl 0x99ebc, lp
0x000ca78e      0a60           mov r10, r12
0x000ca790      0c58           mov r12, r11
0x000ca792      80071c01       jr 0xca8ae
0x000ca796      43670200       st.b r12, 2[sp]
0x000ca79a      a49771c2       ld.bu -15759[gp], r18
0x000ca79e      f261           cmp r18, r12
0x000ca7a0      821d           be 0xca7d0
0x000ca7a2      20eea500       movea 165, r0, r29
0x000ca7a6      a4bf71c2       ld.bu -15759[gp], r23
0x000ca7aa      446771c2       st.b r12, -15759[gp]
0x000ca7ae      43670200       st.b r12, 2[sp]
0x000ca7b2      20363000       movea 48, r0, r6                        ; '0'
0x000ca7b6      bfffda91       jarl 0xc3990, lp
0x000ca7ba      049f71c2       ld.b -15759[gp], r19
0x000ca7be      629a           cmp 2, r19
0x000ca7c0      b205           be 0xca7c6
0x000ca7c2      62ba           cmp 2, r23
0x000ca7c4      ea05           bne 0xca7d0
0x000ca7c6      0432           mov 4, r6
0x000ca7c8      203e1100       movea 17, r0, r7
0x000ca7cc      81ff082b       jarl 0xdd2d4, lp
0x000ca7d0      20bea500       movea 165, r0, r23
0x000ca7d4      fdb9           cmp r29, r23
0x000ca7d6      ca0d           bne 0xca7ee
0x000ca7d8      386f0100       ld.w 0[r24], r13
0x000ca7dc      0d770000       ld.b 0[r13], r14
0x000ca7e0      447739f3       st.b r14, -3271[gp]
0x000ca7e4      2436a6f1       movea -3674, gp, r6
0x000ca7e8      013a           mov 1, r7
0x000ca7ea      81ff2216       jarl 0xdbe0c, lp
0x000ca7ee      a47f8dba       ld.bu -17779[gp], r15
0x000ca7f2      efb9           cmp r15, r23
0x000ca7f4      d205           be 0xca7fe
0x000ca7f6      0487e6c5       ld.b -14874[gp], r16
0x000ca7fa      448796ba       st.b r16, -17770[gp]
0x000ca7fe      a48f8dba       ld.bu -17779[gp], r17
0x000ca802      f1b9           cmp r17, r23
0x000ca804      da05           bne 0xca80e
0x000ca806      045796ba       ld.b -17770[gp], r10
0x000ca80a      e051           cmp r0, r10
0x000ca80c      e205           be 0xca818
0x000ca80e      049798ba       ld.b -17768[gp], r18
0x000ca812      449795ba       st.b r18, -17771[gp]
0x000ca816      b505           br 0xca81c
0x000ca818      44b795ba       st.b r22, -17771[gp]
0x000ca81c      a49f95ba       ld.bu -17771[gp], r19
0x000ca820      fa99           cmp r26, r19
0x000ca822      ea0d           bne 0xca83e
0x000ca824      846f97ba       ld.bu -17770[gp], r13
0x000ca828      fb69           cmp r27, r13
0x000ca82a      aa0d           bne 0xca83e
0x000ca82c      a47771be       ld.bu -16783[gp], r14
0x000ca830      fc71           cmp r28, r14
0x000ca832      ea05           bne 0xca83e
0x000ca834      20361600       movea 22, r0, r6
0x000ca838      bfff5891       jarl 0xc3990, lp
0x000ca83c      853d           br 0xca8ac
0x000ca83e      a47f95ba       ld.bu -17771[gp], r15
0x000ca842      fa79           cmp r26, r15
0x000ca844      9a25           bne 0xca886
0x000ca846      848797ba       ld.bu -17770[gp], r16
0x000ca84a      fb81           cmp r27, r16
0x000ca84c      da1d           bne 0xca886
0x000ca84e      a48f71be       ld.bu -16783[gp], r17
0x000ca852      fc89           cmp r28, r17
0x000ca854      921d           be 0xca886
0x000ca856      a45795ba       ld.bu -17771[gp], r10
0x000ca85a      eab9           cmp r10, r23
0x000ca85c      ca15           bne 0xca884
0x000ca85e      20362f00       movea 47, r0, r6                        ; '/'
0x000ca862      bfff2e91       jarl 0xc3990, lp
0x000ca866      20361800       movea 24, r0, r6
0x000ca86a      bfff2691       jarl 0xc3990, lp
0x000ca86e      20361600       movea 22, r0, r6
0x000ca872      bfff1e91       jarl 0xc3990, lp
0x000ca876      0f32           mov 15, r6
0x000ca878      bfff1891       jarl 0xc3990, lp
0x000ca87c      0532           mov 5, r6
0x000ca87e      043a           mov 4, r7
0x000ca880      81ff542a       jarl 0xdd2d4, lp
0x000ca884      c515           br 0xca8ac
0x000ca886      a49795ba       ld.bu -17771[gp], r18
0x000ca88a      fa91           cmp r26, r18
0x000ca88c      b20d           be 0xca8a2
0x000ca88e      849f97ba       ld.bu -17770[gp], r19
0x000ca892      fb99           cmp r27, r19
0x000ca894      fa05           bne 0xca8a2
0x000ca896      0132           mov 1, r6
0x000ca898      203e1c00       movea 28, r0, r7
0x000ca89c      81ff382a       jarl 0xdd2d4, lp
0x000ca8a0      e505           br 0xca8ac
0x000ca8a2      0132           mov 1, r6
0x000ca8a4      203e1400       movea 20, r0, r7
0x000ca8a8      81ff2c2a       jarl 0xdd2d4, lp
0x000ca8ac      1958           mov r25, r11
0x000ca8ae      0b50           mov r11, r10
0x000ca8b0      4606fff3       dispose 3, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000ca8b4      8607e1f3       prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 3
0x000ca8b8      bf0700fc       jr 0xca4b8
