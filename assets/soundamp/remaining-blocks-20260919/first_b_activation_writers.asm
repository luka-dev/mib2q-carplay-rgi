0x000aa388      jr 0xaa624
0x000aa38c      st.w r6, 4[sp]                                         ; 4
0x000aa390      ld.hu 4[sp], r6                                        ; 4
0x000aa394      mov r6, r11
0x000aa396      jr 0xaa600
0x000aa39a      ld.bu -16783[gp], r10
0x000aa39e      shl 2, r10
0x000aa3a0      mov 0x6ea68, r13
0x000aa3a6      add r10, r13
0x000aa3a8      ld.w 0[r13], r14
0x000aa3ac      st.w r14, 0[sp]
0x000aa3b0      movea 19, r0, r6
0x000aa3b4      movea 0, sp, r15
0x000aa3b8      ld.w 0[r15], r7
0x000aa3bc      jarl 0xac648, lp
0x000aa3c0      jr 0xaa620
0x000aa3c4      ld.b -15760[gp], r16
0x000aa3c8      cmp r0, r16
0x000aa3ca      bne 0xaa416
0x000aa3cc      ld.bu -15760[gp], r17
0x000aa3d0      shl 2, r17
0x000aa3d2      mov 0x6eaa0, r18
0x000aa3d8      add r17, r18
0x000aa3da      ld.w 0[r18], r19
0x000aa3de      st.w r19, 0[sp]
0x000aa3e2      movea 0, sp, r29
0x000aa3e6      movea 16, r0, r6
0x000aa3ea      ld.w 0[r29], r7
0x000aa3ee      jarl 0xac648, lp
0x000aa3f2      ld.bu -15759[gp], r10
0x000aa3f6      shl 2, r10
0x000aa3f8      mov 0x6ea78, r13
0x000aa3fe      add r10, r13
0x000aa400      ld.w 0[r13], r14
0x000aa404      st.w r14, 0[sp]
0x000aa408      movea 20, r0, r6
0x000aa40c      ld.w 0[r29], r7
0x000aa410      jarl 0xac648, lp
0x000aa414      br 0xaa464
0x000aa416      ld.bu -15759[gp], r15
0x000aa41a      shl 2, r15
0x000aa41c      mov 0x6ea78, r16
0x000aa422      add r15, r16
0x000aa424      ld.w 0[r16], r17
0x000aa428      st.w r17, 0[sp]
0x000aa42c      movea 0, sp, r29
0x000aa430      movea 20, r0, r6
0x000aa434      ld.w 0[r29], r7
0x000aa438      jarl 0xac648, lp
0x000aa43c      mov 15, r6
0x000aa43e      jarl 0xdce98, lp
0x000aa442      ld.bu -15760[gp], r18
0x000aa446      shl 2, r18
0x000aa448      mov 0x6eaa0, r19
0x000aa44e      add r18, r19
0x000aa450      ld.w 0[r19], r10
0x000aa454      st.w r10, 0[sp]
0x000aa458      movea 16, r0, r6
0x000aa45c      ld.w 0[r29], r7
0x000aa460      jarl 0xac648, lp
0x000aa464      jr 0xaa620
0x000aa468      ld.bu -17810[gp], r13
0x000aa46c      mul 21, r13, r1
0x000aa470      mov 0x100008, r14
0x000aa476      add r13, r14
0x000aa478      ld.bu -17770[gp], r15
0x000aa47c      add r15, r14
0x000aa47e      ld.b 0[r14], r16
0x000aa482      add 1, r16
0x000aa484      mov r16, r6
0x000aa486      jarl 0xebca4, lp
0x000aa48a      mov r6, r16
0x000aa48c      st.w r16, 0[sp]
0x000aa490      movea 17, r0, r6
0x000aa494      movea 0, sp, r17
0x000aa498      ld.w 0[r17], r7
0x000aa49c      jarl 0xac648, lp
0x000aa4a0      jr 0xaa620
0x000aa4a4      ld.bu -15760[gp], r18
0x000aa4a8      shl 2, r18
0x000aa4aa      mov 0x6eaa0, r19
0x000aa4b0      add r18, r19
0x000aa4b2      ld.w 0[r19], r10
0x000aa4b6      st.w r10, 0[sp]
0x000aa4ba      movea 16, r0, r6
0x000aa4be      movea 0, sp, r13
0x000aa4c2      ld.w 0[r13], r7
0x000aa4c6      jarl 0xac648, lp
0x000aa4ca      jr 0xaa620
0x000aa4ce      movea -15758, gp, r15
0x000aa4d2      ld.bu -15740[gp], r14
0x000aa4d6      add r14, r15
0x000aa4d8      ld.b 0[r15], r16
0x000aa4dc      mov r16, r6
0x000aa4de      jarl 0xebca4, lp
0x000aa4e2      mov r6, r16
0x000aa4e4      st.w r16, 0[sp]
0x000aa4e8      movea 0, sp, r29
0x000aa4ec      movea 21, r0, r6
0x000aa4f0      ld.w 0[r29], r7
0x000aa4f4      jarl 0xac648, lp
0x000aa4f8      movea -15739, gp, r17
0x000aa4fc      ld.bu -15740[gp], r18
0x000aa500      add r18, r17
0x000aa502      ld.b 0[r17], r19
0x000aa506      mov r19, r6
0x000aa508      jarl 0xebca4, lp
0x000aa50c      mov r6, r19
0x000aa50e      st.w r19, 0[sp]
0x000aa512      movea 22, r0, r6
0x000aa516      ld.w 0[r29], r7
0x000aa51a      jarl 0xac648, lp
0x000aa51e      jr 0xaa620
0x000aa522      ld.b -15721[gp], r10
0x000aa526      mov r10, r6
0x000aa528      jarl 0xebca4, lp
0x000aa52c      mov r6, r10
0x000aa52e      st.w r10, 0[sp]
0x000aa532      movea 23, r0, r6
0x000aa536      movea 0, sp, r13
0x000aa53a      ld.w 0[r13], r7
0x000aa53e      jarl 0xac648, lp
0x000aa542      jr 0xaa620
0x000aa546      ld.b -15719[gp], r14
0x000aa54a      mov r14, r6
0x000aa54c      jarl 0xebca4, lp
0x000aa550      mov r6, r14
0x000aa552      st.w r14, 0[sp]
0x000aa556      movea 24, r0, r6
0x000aa55a      movea 0, sp, r15
0x000aa55e      ld.w 0[r15], r7
0x000aa562      jarl 0xac648, lp
0x000aa566      br 0xaa620
0x000aa568      ld.b -15718[gp], r16
0x000aa56c      addi 100, r16, r11
0x000aa570      cmp r0, r11
0x000aa572      ble 0xaa582
0x000aa574      mov r11, r6
0x000aa576      jarl 0xebca4, lp
0x000aa57a      mov r6, r18
0x000aa57c      st.w r18, 0[sp]
0x000aa580      br 0xaa588
0x000aa582      mov 0, r17
0x000aa584      st.w r17, 0[sp]
0x000aa588      movea 25, r0, r6
0x000aa58c      movea 0, sp, r19
0x000aa590      ld.w 0[r19], r7
0x000aa594      jarl 0xac648, lp
0x000aa598      br 0xaa620
0x000aa59a      mov 10, r11
0x000aa59c      ld.w -17832[gp], r10
0x000aa5a0      divu r11, r10, r0
0x000aa5a4      divu r11, r10, r0
0x000aa5a8      mov r10, r12
0x000aa5aa      movea 400, r0, r1
0x000aa5ae      cmp r1, r12
0x000aa5b0      bnl 0xaa5e2
0x000aa5b2      mov r12, r13
0x000aa5b4      cmp r0, r13
0x000aa5b6      bge 0xaa5d4
0x000aa5b8      shl 1, r13
0x000aa5ba      shr 1, r13
0x000aa5bc      mov r13, r6
0x000aa5be      jarl 0xebca4, lp
0x000aa5c2      mov r6, r13
0x000aa5c4      movhi 20224, r0, r14
0x000aa5c8      mov r14, r6
0x000aa5ca      mov r13, r7
0x000aa5cc      jarl 0xeba38, lp
0x000aa5d0      mov r6, r13
0x000aa5d2      br 0xaa5dc
0x000aa5d4      mov r13, r6
0x000aa5d6      jarl 0xebca4, lp
0x000aa5da      mov r6, r13
0x000aa5dc      st.w r13, 0[sp]
0x000aa5e0      br 0xaa5ea
0x000aa5e2      movhi 17352, r0, r15
0x000aa5e6      st.w r15, 0[sp]
0x000aa5ea      movea 27, r0, r6
0x000aa5ee      movea 0, sp, r16
0x000aa5f2      ld.w 0[r16], r7
0x000aa5f6      jarl 0xac648, lp
0x000aa5fa      br 0xaa620
0x000aa5fc      br 0xaa620
0x000aa5fe      br 0xaa620
0x000aa600      mov r11, r18
0x000aa602      movea 45, r0, r1                                       ; '-'
0x000aa606      sub r1, r18
0x000aa608      cmp 8, r18
0x000aa60a      bh 0xaa5fc
0x000aa60c      switch r18
0x000aa60e      st.b lp, -211[r11]
0x000aa612      andi 65243, r6, lp
0x000aa616      st.h lp, [r0]
0x000aa61a      ld.bu -84[r28], lp
0x000aa61e      tst1 7, 1604[r6]
0x000aa622      jmp [lp]
0x000aa624      prepare {r29, lp}, 2
0x000aa628      jr 0xaa38c
0x000ac648      br 0xac6c4
0x000ac64a      st.w r6, 0[sp]
0x000ac64e      ld.w 0[sp], r6
0x000ac652      st.w r7, 4[sp]                                         ; 4
0x000ac656      mov r0, r11
0x000ac658      mov 0xa0a0a0a, r10                                     ; '\n\n\n\n'
0x000ac65e      st.w r10, -17216[gp]
0x000ac662      ld.w -16832[gp], r15
0x000ac666      movhi 16384, r0, r1
0x000ac66a      xor r1, r15
0x000ac66c      st.w r15, -16832[gp]
0x000ac670      movhi 5, r0, r12
0x000ac674      mov 1, r13
0x000ac676      movea -17212, gp, r14
0x000ac67a      ld.w -16832[gp], r16
0x000ac67e      mov 0x50002, r1
0x000ac684      or r1, r16
0x000ac686      st.w r16, -17212[gp]
0x000ac68a      mov 2, r9
0x000ac68c      mov 0xc4ae1, r17
0x000ac692      st.w r17, -17208[gp]
0x000ac696      mov r0, r8
0x000ac698      st.w r6, -17204[gp]
0x000ac69c      mov 4, r7
0x000ac69e      ld.w 4[sp], r18                                        ; 4
0x000ac6a2      st.w r18, -17200[gp]
0x000ac6a6      jarl 0xa8d04, lp
0x000ac6aa      jarl 0xa8c6c, lp
0x000ac6ae      movea -17624, gp, r6
0x000ac6b2      movea 24, r0, r7
0x000ac6b6      jarl 0xe34d4, lp
0x000ac6ba      mov 4, r6
0x000ac6bc      jarl 0xdce98, lp
0x000ac6c0      dispose 2, {lp}, lp
0x000ac6c4      prepare {lp}, 2
0x000ac6c8      br 0xac64a
0x000ac6ca      nop
0x000ac818      br 0xac8a2
0x000ac81a      st.w r6, 0[sp]
0x000ac81e      mov 3, r29
0x000ac820      mov r0, r6
0x000ac822      mov 5, r7
0x000ac824      mov 1, r8
0x000ac826      jarl 0xa8bb4, lp
0x000ac82a      ld.hu 0[sp], r10
0x000ac82e      cmp 5, r10
0x000ac830      bne 0xac852
0x000ac832      movea -17216, gp, r11
0x000ac836      mov r29, r12
0x000ac838      addi 1, r12, r13
0x000ac83c      mov r13, r29
0x000ac83e      and r20, r29
0x000ac840      mov r12, r14
0x000ac842      shl 2, r14
0x000ac844      add r11, r14
0x000ac846      mov 0x20003, r15
0x000ac84c      st.w r15, 0[r14]
0x000ac850      br 0xac870
0x000ac852      movea -17216, gp, r11
0x000ac856      mov r29, r12
0x000ac858      addi 1, r12, r16
0x000ac85c      mov r16, r29
0x000ac85e      and r20, r29
0x000ac860      mov r12, r17
0x000ac862      shl 2, r17
0x000ac864      add r11, r17
0x000ac866      mov 0x20001, r18
0x000ac86c      st.w r18, 0[r17]
0x000ac870      mov r29, r12
0x000ac872      addi 1, r12, r19
0x000ac876      mov r19, r29
0x000ac878      and r20, r29
0x000ac87a      mov r12, r10
0x000ac87c      shl 2, r10
0x000ac87e      add r11, r10
0x000ac880      mov 0xc4ad9, r13
0x000ac886      st.w r13, 0[r10]
0x000ac88a      jarl 0xa8d04, lp
0x000ac88e      jarl 0xa8c6c, lp
0x000ac892      movea -17624, gp, r6
0x000ac896      movea 24, r0, r7
0x000ac89a      jarl 0xe34d4, lp
0x000ac89e      dispose 1, {r29, lp}, lp
0x000ac8a2      prepare {r29, lp}, 1
0x000ac8a6      br 0xac81a
0x000b6b76      st.b r8, -17771[gp]
0x000b6b7a      st.b r8, -17768[gp]
0x000b6b7e      movea 192, r0, r16
0x000b6b82      st.b r16, -15905[gp]
0x000b6b86      st.b r0, -15760[gp]
0x000b6b8a      mov 1, r19
0x000b6b8c      st.b r19, -17770[gp]
0x000b6b90      st.b r19, -17769[gp]
0x000b6b94      st.b r19, -14874[gp]
0x000b6b98      st.b r0, -14873[gp]
0x000b6b9c      st.b r0, -16783[gp]
0x000c27f0      ld.bu -15740[gp], r17
0x000c27f4      shl 2, r17
0x000c27f6      mov 0x95314, r16
0x000c27fc      add r17, r16
0x000c27fe      ld.b 0[r16], r18
0x000c2802      st.b r18, -15760[gp]
0x000c2806      ld.b -15760[gp], r10
0x000c280a      cmp 2, r10
0x000c280c      bne 0xc281a
0x000c280e      ld.bu -15905[gp], r19
0x000c2812      movea 192, r0, r1
0x000c2816      cmp r1, r19
0x000c2818      be 0xc2822
0x000c281a      ld.b -15760[gp], r15
0x000c281e      cmp 3, r15
0x000c2820      bl 0xc2826
0x000c2822      st.b r0, -15760[gp]
0x000c2826      ld.b -15760[gp], r17
0x000c282a      cmp 2, r17
0x000c282c      bne 0xc2840
0x000c282e      ld.b -15905[gp], r16
0x000c2832      andi 3, r16, r16
0x000c2836      cmp r0, r16
0x000c2838      bne 0xc2840
0x000c283a      mov 1, r18
0x000c283c      st.b r18, -15760[gp]
0x000d4c0e      st.w r6, 0[sp]
0x000d4c12      st.w r7, 4[sp]                                         ; 4
0x000d4c16      ld.w 4[sp], r12                                        ; 4
0x000d4c1a      ld.w 0[r12], r10
0x000d4c1e      ld.bu 4[r10], r11                                      ; 4
0x000d4c22      cmp r0, r11
0x000d4c24      be 0xd4c2e
0x000d4c26      cmp 1, r11
0x000d4c28      be 0xd4c2e
0x000d4c2a      cmp 2, r11
0x000d4c2c      bne 0xd4c3c
0x000d4c2e      st.b r11, -15760[gp]
0x000d4c32      movea 45, r0, r6                                       ; '-'
0x000d4c36      jarl 0xc3990, lp
0x000d4c3a      br 0xd4c5a
0x000d4c3c      movea -17832, gp, r6
0x000d4c40      ld.w 0[r12], r13
0x000d4c44      mov r13, r7
0x000d4c46      jarl 0x99d98, lp
0x000d4c4a      mov 3, r6
0x000d4c4c      mov 6, r7
0x000d4c4e      jarl 0xdd2d4, lp
0x000d4c52      movea 53, r0, r6                                       ; '5'
0x000d4c56      jarl 0xc3990, lp
0x000d4c5a      mov r0, r11
0x000d4c5c      mov r11, r10
0x000d4c5e      dispose 2, {lp}, lp
0x000a775e      mov r29, r6
0x000a7760      jarl 0xac818, lp
0x000a7764      jr 0xa7d1a
0x000a7768      mov r29, r6
0x000a776a      jarl 0xac818, lp
0x000a776e      jr 0xa7d1a
0x000a7772      mov r29, r6
0x000a7774      jarl 0xac8a8, lp
0x000a7778      jr 0xa7d1a
0x000a777c      mov r29, r6
0x000a79c2      cmp 5, r13
0x000a79c4      bne 0xa79ca
0x000a79c6      jr 0xa775e
0x000a7c0e      movea 44, r0, r1                                       ; ','
0x000a7c12      cmp r1, r13
0x000a7c14      bne 0xa7c1a
0x000a7c16      jr 0xa7768
0x000c28a8      br 0xc2964
0x000c28aa      st.w r6, 0[sp]
0x000c28ae      ld.bu 0[sp], r29
0x000c28b2      mov 2, r6
0x000c28b4      movea 24, r0, r7
0x000c28b8      mov r29, r8
0x000c28ba      mov r0, r9
0x000c28bc      jarl 0xea2f0, lp
0x000c28c0      mov 1, r6
0x000c28c2      jarl 0xdd814, lp
0x000c28c6      st.h r0, -16776[gp]
0x000c28ca      mov r0, r11
0x000c28cc      cmp 1, r29
0x000c28ce      be 0xc28d4
0x000c28d0      cmp 8, r29
0x000c28d2      bne 0xc28fe
0x000c28d4      mov r0, r12
0x000c28d6      movea 30, r0, r6
0x000c28da      movea -16772, gp, r14
0x000c28de      mov r11, r13
0x000c28e0      addi 1, r13, r10
0x000c28e4      mov r10, r11
0x000c28e6      and r21, r11
0x000c28e8      mov r13, r15
0x000c28ea      shl 1, r15
0x000c28ec      add r14, r15
0x000c28ee      st.h r12, 0[r15]
0x000c28f2      addi 1, r12, r16
0x000c28f6      mov r16, r12
0x000c28f8      and r21, r12
0x000c28fa      cmp r6, r12
0x000c28fc      blt 0xc28de
0x000c28fe      cmp 1, r29
0x000c2900      bne 0xc2956
0x000c2902      movea 30, r0, r12
0x000c2906      movea 33, r0, r6                                       ; '!'
0x000c290a      movea -16772, gp, r14
0x000c290e      mov r11, r13
0x000c2910      addi 1, r13, r17
0x000c2914      mov r17, r11
0x000c2916      and r21, r11
0x000c2918      mov r13, r18
0x000c291a      shl 1, r18
0x000c291c      add r14, r18
0x000c291e      st.h r12, 0[r18]
0x000c2922      addi 1, r12, r19
0x000c2926      mov r19, r12
0x000c2928      and r21, r12
0x000c292a      cmp r6, r12
0x000c292c      blt 0xc290e
0x000c292e      movea 33, r0, r12                                      ; '!'
0x000c2932      movea 54, r0, r6                                       ; '6'
0x000c2936      mov r11, r13
0x000c2938      addi 1, r13, r10
0x000c293c      mov r10, r11
0x000c293e      and r21, r11
0x000c2940      mov r13, r15
0x000c2942      shl 1, r15
0x000c2944      add r14, r15
0x000c2946      st.h r12, 0[r15]
0x000c294a      addi 1, r12, r16
0x000c294e      mov r16, r12
0x000c2950      and r21, r12
0x000c2952      cmp r6, r12
0x000c2954      blt 0xc2936
0x000c2956      st.h r11, -16774[gp]
0x000c295a      mov 1, r6
0x000c295c      jarl 0xdd830, lp
0x000c2960      dispose 1, {r29, lp}, lp
0x000c2964      prepare {r29, lp}, 1
0x000c2968      br 0xc28aa
0x000c264a      cmp 1, r29
0x000c264c      bne 0xc2680
0x000c264e      mov 9, r6
0x000c2650      mov 2, r7
0x000c2652      jarl 0xdd200, lp
0x000c2656      mov r29, r6
0x000c2658      jarl 0xc28a8, lp
0x000c265c      mov 5, r6
0x000c265e      mov 4, r7
0x000c2660      jarl 0xdd2d4, lp
0x000c2664      mov 2, r6
0x000c2666      movea 2000, r0, r7
0x000c266a      jarl 0xdcf34, lp
0x000c266e      mov r10, r11
0x000c2670      mov 3, r6
0x000c2672      mov 6, r7
0x000c2674      jarl 0xdd2d4, lp
0x000c2678      clr1 1, -17777[gp]
0x000c267c      clr1 3, -17777[gp]
0x000a74ec      br 0xa754e
0x000a74ee      ld.h -16776[gp], r10
0x000a74f2      ld.h -16774[gp], r11
0x000a74f6      cmp r11, r10
0x000a74f8      be 0xa7542
0x000a74fa      mov 1, r6
0x000a74fc      jarl 0xdd814, lp
0x000a7500      ld.hu -16776[gp], r12
0x000a7504      shl 1, r12
0x000a7506      movea -16772, gp, r13
0x000a750a      add r12, r13
0x000a750c      ld.hu 0[r13], r29
0x000a7510      ld.h -16776[gp], r14
0x000a7514      add 1, r14
0x000a7516      st.h r14, -16776[gp]
0x000a751a      ld.h -16776[gp], r15
0x000a751e      movea 376, r0, r1
0x000a7522      cmp r1, r15
0x000a7524      bl 0xa752a
0x000a7526      st.h r0, -16776[gp]
0x000a752a      mov 1, r6
0x000a752c      jarl 0xdd830, lp
0x000a7530      mov r29, r6
0x000a7532      jarl 0xa76b0, lp
0x000a7536      ld.h -16776[gp], r16
0x000a753a      ld.h -16774[gp], r17
0x000a753e      cmp r17, r16
0x000a7540      bne 0xa74fa
0x000a7542      mov 9, r6
0x000a7544      mov 2, r7
0x000a7546      jarl 0xdd2d4, lp
0x000a754a      dispose 0, {r29, lp}, lp
0x000a754e      prepare {r29, lp}, 0
0x000a7552      br 0xa74ee
0x000c2770      jr 0xc28a0
0x000c2774      mov 2, r6
0x000c2776      movea 19, r0, r7
0x000c277a      ld.bu -17810[gp], r8
0x000c277e      ld.bu -17770[gp], r9
0x000c2782      jarl 0xea2f0, lp
0x000c2786      movea 165, r0, r11
0x000c278a      ld.bu -17785[gp], r10
0x000c278e      cmp r11, r10
0x000c2790      bne 0xc2796
0x000c2792      jr 0xc289c
0x000c2796      movhi -1, gp, r1
0x000c279a      ld.bu -9484[r1], r15
0x000c279e      cmp r15, r11
0x000c27a0      bne 0xc27ea
0x000c27a2      mov 0x100008, r11
0x000c27a8      ld.bu -17810[gp], r16
0x000c27ac      mul 21, r16, r1
0x000c27b0      add r11, r16
0x000c27b2      ld.bu -17770[gp], r17
0x000c27b6      add r17, r16
0x000c27b8      ld.b 0[r16], r18
0x000c27bc      cmp -1, r18
0x000c27be      be 0xc27e2
0x000c27c0      ld.bu -17810[gp], r19
0x000c27c4      mul 21, r19, r1
0x000c27c8      add r11, r19
0x000c27ca      ld.bu -17770[gp], r10
0x000c27ce      add r10, r19
0x000c27d0      ld.b 0[r19], r15
0x000c27d4      mul 192, r15, r1
0x000c27d8      mov 0x101bb4, r14
0x000c27de      add r15, r14
0x000c27e0      br 0xc27e8
0x000c27e2      mov 0x8f33c, r14
0x000c27e8      br 0xc27f0
0x000c27ea      mov 0x8f33c, r14
0x000c27f0      ld.bu -15740[gp], r17
0x000c27f4      shl 2, r17
0x000c27f6      mov 0x95314, r16
0x000c27fc      add r17, r16
0x000c27fe      ld.b 0[r16], r18
0x000c2802      st.b r18, -15760[gp]
0x000c2806      ld.b -15760[gp], r10
0x000c280a      cmp 2, r10
0x000c280c      bne 0xc281a
0x000c280e      ld.bu -15905[gp], r19
0x000c2812      movea 192, r0, r1
0x000c2816      cmp r1, r19
0x000c2818      be 0xc2822
0x000c281a      ld.b -15760[gp], r15
0x000c281e      cmp 3, r15
0x000c2820      bl 0xc2826
0x000c2822      st.b r0, -15760[gp]
0x000c2826      ld.b -15760[gp], r17
0x000c282a      cmp 2, r17
0x000c282c      bne 0xc2840
0x000c282e      ld.b -15905[gp], r16
0x000c2832      andi 3, r16, r16
0x000c2836      cmp r0, r16
0x000c2838      bne 0xc2840
0x000c283a      mov 1, r18
0x000c283c      st.b r18, -15760[gp]
0x000c2840      mov r0, r13
0x000c2842      mov r0, r11
0x000c2844      movea -15904, gp, r6
0x000c2848      mov r13, r12
0x000c284a      shl 3, r12
0x000c284c      mov r6, r10
0x000c284e      add r12, r10
0x000c2850      add r11, r10
0x000c2852      ld.bu -15760[gp], r19
0x000c2856      shl 6, r19
0x000c2858      add r14, r19
0x000c285a      add r12, r19
0x000c285c      add r11, r19
0x000c285e      ld.b 0[r19], r15
0x000c2862      st.b r15, 0[r10]
0x000c2866      addi 1, r11, r17
0x000c286a      mov r17, r11
0x000c286c      and r20, r11
0x000c286e      cmp 8, r11
0x000c2870      blt 0xc2848
0x000c2872      addi 1, r13, r16
0x000c2876      mov r16, r13
0x000c2878      and r20, r13
0x000c287a      cmp 8, r13
0x000c287c      blt 0xc2842
0x000c287e      movea 47, r0, r6                                       ; '/'
0x000c2882      jarl 0xc3990, lp
0x000c2886      movea 48, r0, r6                                       ; '0'
0x000c288a      jarl 0xc3990, lp
0x000c288e      movea 22, r0, r6
0x000c2892      jarl 0xc3990, lp
0x000c2896      mov 15, r6
0x000c2898      jarl 0xc3990, lp
0x000c289c      dispose 0, {lp}, lp
0x000d002e      ld.w 0[r27], r16
0x000d0032      ld.b 5[r16], r17
0x000d0036      addi -33, r17, r17
0x000d003a      and r20, r17
0x000d003c      st.b r17, -15740[gp]
0x000d953a      ld.b -15336[gp], r15
0x000d953e      addi -33, r15, r15
0x000d9542      st.b r15, -15740[gp]
0x000d9546      st.b r0, -3272[gp]
0x000acfb4      ld.bu -17756[gp], r10
0x000acfb8      movea 165, r0, r1
0x000acfbc      cmp r1, r10
0x000acfbe      be 0xacfc4
0x000acfc0      jr 0xad0a2
0x000acfc4      movhi 18, r0, r1
0x000acfc8      ld.w 8[r1], r14
0x000acfcc      cmp r0, r14
0x000acfce      be 0xad0a2
0x000acfd0      movhi 18, r0, r1
0x000acfd4      ld.w 8[r1], r15
0x000acfd8      movea 8000, r0, r1                                     ; '@\x1f'
0x000acfdc      cmp r1, r15
0x000acfde      bh 0xad0a2
0x000acfe0      mov r0, r26
0x000acfe2      addi 80, r26, r16
0x000acfe6      movhi 18, r0, r1
0x000acfea      ld.w 8[r1], r17
0x000acfee      cmp r17, r16
0x000acff0      bh 0xacff8
0x000acff2      movea 80, r0, r27                                      ; 'P'
0x000acff6      br 0xad004
0x000acff8      movhi 18, r0, r1
0x000acffc      ld.w 8[r1], r27
0x000ad000      sub r26, r27
0x000ad002      and r21, r27
0x000ad004      mov 3, r28
0x000ad006      mov r27, r6
0x000ad008      mov 5, r7
0x000ad00a      mov 1, r8
0x000ad00c      jarl 0xa8bb4, lp
0x000ad010      movea -17216, gp, r29
0x000ad014      mov r28, r13
0x000ad016      addi 1, r13, r18
0x000ad01a      mov r18, r28
0x000ad01c      and r20, r28
0x000ad01e      mov r13, r19
0x000ad020      shl 2, r19
0x000ad022      add r29, r19
0x000ad024      st.w r27, 0[r19]
0x000ad028      mov r28, r11
0x000ad02a      addi 1, r11, r10
0x000ad02e      mov r10, r28
0x000ad030      and r20, r28
0x000ad032      mov r11, r14
0x000ad034      shl 2, r14
0x000ad036      add r29, r14
0x000ad038      mov 0xc3c6a, r15                                       ; 'j<\f'
0x000ad03e      add r26, r15
0x000ad040      st.w r15, 0[r14]
0x000ad044      mov r0, r12
0x000ad046      cmp r0, r27
0x000ad048      ble 0xad07a
0x000ad04a      mov 0x12000c, r11                                      ; '\f'
0x000ad050      mov r28, r16
0x000ad052      shl 2, r16
0x000ad054      add r29, r16
0x000ad056      mov r26, r17
0x000ad058      shl 2, r17
0x000ad05a      add r11, r17
0x000ad05c      ld.w 0[r17], r18
0x000ad060      st.w r18, 0[r16]
0x000ad064      addi 1, r28, r19
0x000ad068      mov r19, r28
0x000ad06a      and r20, r28
0x000ad06c      add 1, r26
0x000ad06e      addi 1, r12, r10
0x000ad072      mov r10, r12
0x000ad074      and r20, r12
0x000ad076      cmp r27, r12
0x000ad078      blt 0xad050
0x000ad07a      jarl 0xa8d04, lp
0x000ad07e      jarl 0xa8c6c, lp
0x000ad082      movea -17624, gp, r6
0x000ad086      mov r27, r14
0x000ad088      shl 2, r14
0x000ad08a      addi 24, r14, r14
0x000ad08e      and r21, r14
0x000ad090      mov r14, r7
0x000ad092      jarl 0xe34d4, lp
0x000ad096      movhi 18, r0, r1
0x000ad09a      ld.w 8[r1], r15
0x000ad09e      cmp r15, r26
0x000ad0a0      bl 0xacfe2
0x000ad0a2      dispose 0, {r26, r27, r28, r29, lp}, lp
