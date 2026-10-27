0x000aa644      80074a02       jr 0xaa88e
0x000aa648      203ea500       movea 165, r0, r7
0x000aa64c      a45795ba       ld.bu -17771[gp], r10
0x000aa650      e751           cmp r7, r10
0x000aa652      ba0d           bne 0xaa668
0x000aa654      847fa5ba       ld.bu -17756[gp], r15
0x000aa658      ef39           cmp r15, r7
0x000aa65a      fa05           bne 0xaa668
0x000aa65c      048771be       ld.b -16783[gp], r16
0x000aa660      4182           add 1, r16
0x000aa662      10d8           mov r16, r27
0x000aa664      54d9           and r20, r27
0x000aa666      a505           br 0xaa66a
0x000aa668      00d8           mov r0, r27
0x000aa66a      2066a500       movea 165, r0, r12
0x000aa66e      0058           mov r0, r11
0x000aa670      206e5a00       movea 90, r0, r13                       ; 'Z'
0x000aa674      848f71be       ld.bu -16784[gp], r17
0x000aa678      ed89           cmp r13, r17
0x000aa67a      aa15           bne 0xaa69e
0x000aa67c      24965cbe       movea -16804, gp, r18
0x000aa680      cb91           add r11, r18
0x000aa682      1b98           mov r27, r19
0x000aa684      f49f4002       mul 20, r19, r0
0x000aa688      440effff       movhi -1, gp, r1
0x000aa68c      215608ca       movea -13816, r1, r10
0x000aa690      d351           add r19, r10
0x000aa692      cb51           add r11, r10
0x000aa694      0a7f0000       ld.b 0[r10], r15
0x000aa698      527f0000       st.b r15, 0[r18]
0x000aa69c      f55d           br 0xaa75a
0x000aa69e      848771be       ld.bu -16784[gp], r16
0x000aa6a2      200eaa00       movea 170, r0, r1
0x000aa6a6      e181           cmp r1, r16
0x000aa6a8      9a3d           bne 0xaa71a
0x000aa6aa      24765cbe       movea -16804, gp, r14
0x000aa6ae      440effff       movhi -1, gp, r1
0x000aa6b2      213608ca       movea -13816, r1, r6
0x000aa6b6      0e88           mov r14, r17
0x000aa6b8      cb89           add r11, r17
0x000aa6ba      119f0000       ld.b 0[r17], r19
0x000aa6be      1b50           mov r27, r10
0x000aa6c0      f4574002       mul 20, r10, r0
0x000aa6c4      c651           add r6, r10
0x000aa6c6      cb51           add r11, r10
0x000aa6c8      0a970000       ld.b 0[r10], r18
0x000aa6cc      f299           cmp r18, r19
0x000aa6ce      d70d           ble 0xaa6e8
0x000aa6d0      0e78           mov r14, r15
0x000aa6d2      cb79           add r11, r15
0x000aa6d4      0f40           mov r15, r8
0x000aa6d6      0880           mov r8, r16
0x000aa6d8      108f0000       ld.b 0[r16], r17
0x000aa6dc      5f8a           add -1, r17
0x000aa6de      488f0000       st.b r17, 0[r8]
0x000aa6e2      20665a00       movea 90, r0, r12                       ; 'Z'
0x000aa6e6      951d           br 0xaa718
0x000aa6e8      0e50           mov r14, r10
0x000aa6ea      cb51           add r11, r10
0x000aa6ec      0a9f0000       ld.b 0[r10], r19
0x000aa6f0      1b90           mov r27, r18
0x000aa6f2      f4974002       mul 20, r18, r0
0x000aa6f6      c691           add r6, r18
0x000aa6f8      cb91           add r11, r18
0x000aa6fa      127f0000       ld.b 0[r18], r15
0x000aa6fe      ef99           cmp r15, r19
0x000aa700      ce0d           bge 0xaa718
0x000aa702      0e80           mov r14, r16
0x000aa704      cb81           add r11, r16
0x000aa706      1040           mov r16, r8
0x000aa708      0888           mov r8, r17
0x000aa70a      11570000       ld.b 0[r17], r10
0x000aa70e      4152           add 1, r10
0x000aa710      48570000       st.b r10, 0[r8]
0x000aa714      20665a00       movea 90, r0, r12                       ; 'Z'
0x000aa718      9525           br 0xaa75a
0x000aa71a      849771be       ld.bu -16784[gp], r18
0x000aa71e      f491           cmp r20, r18
0x000aa720      da1d           bne 0xaa75a
0x000aa722      cb9e0100       andi 1, r11, r19
0x000aa726      619a           cmp 1, r19
0x000aa728      9a0d           bne 0xaa73a
0x000aa72a      247e5cbe       movea -16804, gp, r15
0x000aa72e      cb79           add r11, r15
0x000aa730      20869cff       movea -100, r0, r16
0x000aa734      4f870000       st.b r16, 0[r15]
0x000aa738      9515           br 0xaa75a
0x000aa73a      248e5cbe       movea -16804, gp, r17
0x000aa73e      cb89           add r11, r17
0x000aa740      1b50           mov r27, r10
0x000aa742      f4574002       mul 20, r10, r0
0x000aa746      440effff       movhi -1, gp, r1
0x000aa74a      219608ca       movea -13816, r1, r18
0x000aa74e      ca91           add r10, r18
0x000aa750      cb91           add r11, r18
0x000aa752      129f0000       ld.b 0[r18], r19
0x000aa756      519f0000       st.b r19, 0[r17]
0x000aa75a      20ee1400       movea 20, r0, r29
0x000aa75e      0b7e0100       addi 1, r11, r15
0x000aa762      0f58           mov r15, r11
0x000aa764      5459           and r20, r11
0x000aa766      fd59           cmp r29, r11
0x000aa768      e685           blt 0xaa674
0x000aa76a      848771be       ld.bu -16784[gp], r16
0x000aa76e      200eaa00       movea 170, r0, r1
0x000aa772      e181           cmp r1, r16
0x000aa774      fa05           bne 0xaa782
0x000aa776      ec39           cmp r12, r7
0x000aa778      da05           bne 0xaa782
0x000aa77a      20565a00       movea 90, r0, r10                       ; 'Z'
0x000aa77e      445770be       st.b r10, -16784[gp]
0x000aa782      03e2           mov 3, r28
0x000aa784      20361400       movea 20, r0, r6
0x000aa788      053a           mov 5, r7
0x000aa78a      0142           mov 1, r8
0x000aa78c      bfff28e4       jarl 0xa8bb4, lp
0x000aa790      2476c0bc       movea -17216, gp, r14
0x000aa794      1c68           mov r28, r13
0x000aa796      0d960100       addi 1, r13, r18
0x000aa79a      12e0           mov r18, r28
0x000aa79c      54e1           and r20, r28
0x000aa79e      0d88           mov r13, r17
0x000aa7a0      c28a           shl 2, r17
0x000aa7a2      ce89           add r14, r17
0x000aa7a4      209e1400       movea 20, r0, r19
0x000aa7a8      719f0100       st.w r19, 0[r17]
0x000aa7ac      1c58           mov r28, r11
0x000aa7ae      0b7e0100       addi 1, r11, r15
0x000aa7b2      0fe0           mov r15, r28
0x000aa7b4      54e1           and r20, r28
0x000aa7b6      0b80           mov r11, r16
0x000aa7b8      c282           shl 2, r16
0x000aa7ba      ce81           add r14, r16
0x000aa7bc      2a0684050e00   mov 0xe0584, r10
0x000aa7c2      70570100       st.w r10, 0[r16]
0x000aa7c6      0060           mov r0, r12
0x000aa7c8      24365cbe       movea -16804, gp, r6
0x000aa7cc      0690           mov r6, r18
0x000aa7ce      cc91           add r12, r18
0x000aa7d0      128f0000       ld.b 0[r18], r17
0x000aa7d4      ea8f400a       mul 10, r17, r1
0x000aa7d8      1158           mov r17, r11
0x000aa7da      e0d9           cmp r0, r27
0x000aa7dc      b725           ble 0xaa822
0x000aa7de      a49fdfc1       ld.bu -15905[gp], r19
0x000aa7e2      200ec000       movea 192, r0, r1
0x000aa7e6      e199           cmp r1, r19
0x000aa7e8      da1d           bne 0xaa822
0x000aa7ea      cc7e0100       andi 1, r12, r15
0x000aa7ee      617a           cmp 1, r15
0x000aa7f0      9a1d           bne 0xaa822
0x000aa7f2      048720c2       ld.b -15840[gp], r16
0x000aa7f6      c182           shl 1, r16
0x000aa7f8      440effff       movhi -1, gp, r1
0x000aa7fc      215634d5       movea -10956, r1, r10
0x000aa800      d051           add r16, r10
0x000aa802      2a971200       ld.h 18[r10], r18                       ; 0x12
0x000aa806      d259           add r18, r11
0x000aa808      eb00           sxh r11
0x000aa80a      200e22fc       movea -990, r0, r1
0x000aa80e      e159           cmp r1, r11
0x000aa810      be05           bge 0xaa816
0x000aa812      205e22fc       movea -990, r0, r11
0x000aa816      200ec800       movea 200, r0, r1
0x000aa81a      e159           cmp r1, r11
0x000aa81c      b705           ble 0xaa822
0x000aa81e      205ec800       movea 200, r0, r11
0x000aa822      1c68           mov r28, r13
0x000aa824      0d8e0100       addi 1, r13, r17
0x000aa828      11e0           mov r17, r28
0x000aa82a      54e1           and r20, r28
0x000aa82c      0d98           mov r13, r19
0x000aa82e      c29a           shl 2, r19
0x000aa830      ce99           add r14, r19
0x000aa832      0b78           mov r11, r15
0x000aa834      c27a           shl 2, r15
0x000aa836      2f78           not r15, r15
0x000aa838      417a           add 1, r15
0x000aa83a      3006588d0600   mov 0x68d58, r16
0x000aa840      cf81           add r15, r16
0x000aa842      30574106       ld.w 1600[r16], r10                     ; 0x640 ; 1600
0x000aa846      73570100       st.w r10, 0[r19]
0x000aa84a      0c960100       addi 1, r12, r18
0x000aa84e      1260           mov r18, r12
0x000aa850      5461           and r20, r12
0x000aa852      fd61           cmp r29, r12
0x000aa854      c6bd           blt 0xaa7cc
0x000aa856      bfffaee4       jarl 0xa8d04, lp
0x000aa85a      bfff12e4       jarl 0xa8c6c, lp
0x000aa85e      243628bb       movea -17624, gp, r6
0x000aa862      203e6800       movea 104, r0, r7                       ; 'h'
0x000aa866      83ff6e8c       jarl 0xe34d4, lp
0x000aa86a      048f20c2       ld.b -15840[gp], r17
0x000aa86e      1130           mov r17, r6
0x000aa870      84ff3414       jarl 0xebca4, lp
0x000aa874      0688           mov r6, r17
0x000aa876      638f0100       st.w r17, 0[sp]
0x000aa87a      20361200       movea 18, r0, r6
0x000aa87e      237e0000       movea 0, sp, r15
0x000aa882      2f3f0100       ld.w 0[r15], r7
0x000aa886      80ffc21d       jarl 0xac648, lp
0x000aa88a      4206ff10       dispose 1, {r27, r28, r29, lp}, lp
0x000aa88e      8207e110       prepare {r27, r28, r29, lp}, 1
0x000aa892      bf07b6fd       jr 0xaa648
0x000aa896      0000           nop
