0x000a903c      80078001       jr 0xa91bc
0x000a9040      00d0           mov r0, r26
0x000a9042      03e2           mov 3, r28
0x000a9044      1cd8           mov r28, r27
0x000a9046      20361000       movea 16, r0, r6
0x000a904a      053a           mov 5, r7
0x000a904c      0142           mov 1, r8
0x000a904e      bfff66fb       jarl 0xa8bb4, lp
0x000a9052      24eec0bc       movea -17216, gp, r29
0x000a9056      1b60           mov r27, r12
0x000a9058      0c560100       addi 1, r12, r10
0x000a905c      0ad8           mov r10, r27
0x000a905e      54d9           and r20, r27
0x000a9060      0c78           mov r12, r15
0x000a9062      c27a           shl 2, r15
0x000a9064      dd79           add r29, r15
0x000a9066      20861000       movea 16, r0, r16
0x000a906a      6f870100       st.w r16, 0[r15]
0x000a906e      1b58           mov r27, r11
0x000a9070      0b8e0100       addi 1, r11, r17
0x000a9074      11d8           mov r17, r27
0x000a9076      54d9           and r20, r27
0x000a9078      0b90           mov r11, r18
0x000a907a      c292           shl 2, r18
0x000a907c      dd91           add r29, r18
0x000a907e      1a98           mov r26, r19
0x000a9080      c39a           shl 3, r19
0x000a9082      21062a020c00   mov 0xc022a, r1
0x000a9088      c199           add r1, r19
0x000a908a      729f0100       st.w r19, 0[r18]
0x000a908e      0070           mov r0, r14
0x000a9090      0068           mov r0, r13
0x000a9092      0457dfc1       ld.b -15905[gp], r10
0x000a9096      ca560f00       andi 15, r10, r10
0x000a909a      e051           cmp r0, r10
0x000a909c      b21d           be 0xa90d2
0x000a909e      64d2           cmp 4, r26
0x000a90a0      961d           blt 0xa90d2
0x000a90a2      0d78           mov r13, r15
0x000a90a4      c37a           shl 3, r15
0x000a90a6      2486e0c1       movea -15904, gp, r16
0x000a90aa      cf81           add r15, r16
0x000a90ac      da81           add r26, r16
0x000a90ae      108f0000       ld.b 0[r16], r17
0x000a90b2      ea8f400a       mul 10, r17, r1
0x000a90b6      049720c2       ld.b -15840[gp], r18
0x000a90ba      c192           shl 1, r18
0x000a90bc      440effff       movhi -1, gp, r1
0x000a90c0      219e5ad5       movea -10918, r1, r19
0x000a90c4      d299           add r18, r19
0x000a90c6      33571200       ld.h 18[r19], r10                       ; 0x12
0x000a90ca      ca89           add r10, r17
0x000a90cc      1160           mov r17, r12
0x000a90ce      ec00           sxh r12
0x000a90d0      c535           br 0xa9138
0x000a90d2      a47fdfc1       ld.bu -15905[gp], r15
0x000a90d6      200ec000       movea 192, r0, r1
0x000a90da      e179           cmp r1, r15
0x000a90dc      ba25           bne 0xa9122
0x000a90de      048795ba       ld.b -17771[gp], r16
0x000a90e2      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000a90e6      e181           cmp r1, r16
0x000a90e8      da1d           bne 0xa9122
0x000a90ea      646a           cmp 4, r13
0x000a90ec      b61d           blt 0xa9122
0x000a90ee      676a           cmp 7, r13
0x000a90f0      9f1d           bgt 0xa9122
0x000a90f2      0d90           mov r13, r18
0x000a90f4      c392           shl 3, r18
0x000a90f6      249ee0c1       movea -15904, gp, r19
0x000a90fa      d299           add r18, r19
0x000a90fc      da99           add r26, r19
0x000a90fe      13570000       ld.b 0[r19], r10
0x000a9102      ea57400a       mul 10, r10, r1
0x000a9106      048f20c2       ld.b -15840[gp], r17
0x000a910a      c18a           shl 1, r17
0x000a910c      440effff       movhi -1, gp, r1
0x000a9110      217e80d5       movea -10880, r1, r15
0x000a9114      d179           add r17, r15
0x000a9116      2f871200       ld.h 18[r15], r16                       ; 0x12
0x000a911a      d051           add r16, r10
0x000a911c      0a60           mov r10, r12
0x000a911e      ec00           sxh r12
0x000a9120      c50d           br 0xa9138
0x000a9122      0d90           mov r13, r18
0x000a9124      c392           shl 3, r18
0x000a9126      249ee0c1       movea -15904, gp, r19
0x000a912a      d299           add r18, r19
0x000a912c      da99           add r26, r19
0x000a912e      138f0000       ld.b 0[r19], r17
0x000a9132      ea8f400a       mul 10, r17, r1
0x000a9136      1160           mov r17, r12
0x000a9138      200e22fc       movea -990, r0, r1
0x000a913c      e161           cmp r1, r12
0x000a913e      be05           bge 0xa9144
0x000a9140      206622fc       movea -990, r0, r12
0x000a9144      200ec800       movea 200, r0, r1
0x000a9148      e161           cmp r1, r12
0x000a914a      b705           ble 0xa9150
0x000a914c      2066c800       movea 200, r0, r12
0x000a9150      1b58           mov r27, r11
0x000a9152      0b7e0100       addi 1, r11, r15
0x000a9156      0fd8           mov r15, r27
0x000a9158      54d9           and r20, r27
0x000a915a      0b80           mov r11, r16
0x000a915c      c282           shl 2, r16
0x000a915e      dd81           add r29, r16
0x000a9160      0c50           mov r12, r10
0x000a9162      c252           shl 2, r10
0x000a9164      2a50           not r10, r10
0x000a9166      4152           add 1, r10
0x000a9168      3206588d0600   mov 0x68d58, r18
0x000a916e      ca91           add r10, r18
0x000a9170      329f4106       ld.w 1600[r18], r19                     ; 0x640 ; 1600
0x000a9174      709f0100       st.w r19, 0[r16]
0x000a9178      0d8e0100       addi 1, r13, r17
0x000a917c      1168           mov r17, r13
0x000a917e      5469           and r20, r13
0x000a9180      686a           cmp 8, r13
0x000a9182      868d           blt 0xa9092
0x000a9184      1a7e0100       addi 1, r26, r15
0x000a9188      0fd0           mov r15, r26
0x000a918a      54d1           and r20, r26
0x000a918c      0e560100       addi 1, r14, r10
0x000a9190      0a70           mov r10, r14
0x000a9192      5471           and r20, r14
0x000a9194      6272           cmp 2, r14
0x000a9196      be05           bge 0xa919c
0x000a9198      bf07f8fe       jr 0xa9090
0x000a919c      bfff68fb       jarl 0xa8d04, lp
0x000a91a0      bfffccfa       jarl 0xa8c6c, lp
0x000a91a4      243628bb       movea -17624, gp, r6
0x000a91a8      203e5800       movea 88, r0, r7                        ; 'X'
0x000a91ac      83ff28a3       jarl 0xe34d4, lp
0x000a91b0      68d2           cmp 8, r26
0x000a91b2      be05           bge 0xa91b8
0x000a91b4      bf0790fe       jr 0xa9044
0x000a91b8      4006ff30       dispose 0, {r26, r27, r28, r29, lp}, lp
0x000a91bc      8007e130       prepare {r26, r27, r28, r29, lp}, 0
0x000a91c0      bf0780fe       jr 0xa9040
