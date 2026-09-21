0x000a7d2c      mov 10, r6
0x000a7d2e      mov 5, r7
0x000a7d30      mov 1, r8
0x000a7d32      jarl 0xa8bb4, lp
0x000a7d36      mov 5, r26
0x000a7d38      mov 10, r24
0x000a7d3a      st.w r24, -17204[gp]
0x000a7d3e      mov 0xc0414, r10
0x000a7d44      st.w r10, -17200[gp]
0x000a7d48      mov r26, r7
0x000a7d4a      mov r0, r12
0x000a7d4c      movhi -1, gp, r1
0x000a7d50      movea -11868, r1, r27
0x000a7d54      movhi -1, gp, r1
0x000a7d58      movea -11488, r1, r28
0x000a7d5c      ld.b -24111[gp], r15
0x000a7d60      mul 10, r15, r1
0x000a7d64      addi -82, r15, r15
0x000a7d68      mov r15, r14
0x000a7d6a      sxh r14
0x000a7d6c      mov r12, r13
0x000a7d6e      mul 38, r13, r0
0x000a7d72      mov r27, r16
0x000a7d74      add r13, r16
0x000a7d76      ld.b -15928[gp], r17
0x000a7d7a      shl 1, r17
0x000a7d7c      add r17, r16
0x000a7d7e      ld.h 18[r16], r18                                      ; 0x12
0x000a7d82      add r14, r18
0x000a7d84      sxh r18
0x000a7d86      mov r18, r6
0x000a7d88      mov r28, r19
0x000a7d8a      add r13, r19
0x000a7d8c      ld.b -15924[gp], r10
0x000a7d90      shl 1, r10
0x000a7d92      add r10, r19
0x000a7d94      ld.h 18[r19], r15                                      ; 0x12
0x000a7d98      mov r15, r11
0x000a7d9a      add r6, r11
0x000a7d9c      sxh r11
0x000a7d9e      cmp 3, r12
0x000a7da0      bne 0xa7dc6
0x000a7da2      tst1 0, -15920[gp]
0x000a7da6      be 0xa7dc2
0x000a7da8      ld.b -15919[gp], r17
0x000a7dac      shl 1, r17
0x000a7dae      movhi -1, gp, r1
0x000a7db2      movea -10994, r1, r16
0x000a7db6      add r17, r16
0x000a7db8      ld.h 18[r16], r18                                      ; 0x12
0x000a7dbc      add r18, r11
0x000a7dbe      sxh r11
0x000a7dc0      br 0xa7dc6
0x000a7dc2      movea -1000, r0, r11
0x000a7dc6      movea 400, r0, r1
0x000a7dca      cmp r1, r11
0x000a7dcc      ble 0xa7dd4
0x000a7dce      movea 400, r0, r11
0x000a7dd2      br 0xa7de0
0x000a7dd4      movea -1000, r0, r1
0x000a7dd8      cmp r1, r11
0x000a7dda      bge 0xa7de0
0x000a7ddc      movea -1000, r0, r11
0x000a7de0      movea -17216, gp, r29
0x000a7de4      mov 0x68d58, r25
0x000a7dea      mov r7, r10
0x000a7dec      shl 2, r10
0x000a7dee      add r29, r10
0x000a7df0      mov r11, r19
0x000a7df2      shl 2, r19
0x000a7df4      not r19, r19
0x000a7df6      add 1, r19
0x000a7df8      add r25, r19
0x000a7dfa      ld.w 1600[r19], r15                                    ; 0x640 ; 1600
0x000a7dfe      st.w r15, 0[r10]
0x000a7e02      addi 1, r7, r17
0x000a7e06      and r20, r17
0x000a7e08      mov r17, r7
0x000a7e0a      addi 1, r12, r16
0x000a7e0e      mov r16, r12
0x000a7e10      and r20, r12
0x000a7e12      cmp 10, r12
0x000a7e14      blt 0xa7d5c
0x000a7e16      jarl 0xa8d04, lp
0x000a7e1a      jarl 0xa8c6c, lp
0x000a7e1e      movea -17624, gp, r6
0x000a7e22      movea 64, r0, r7                                       ; '@'
0x000a7e26      jarl 0xe34d4, lp
0x000a7e2a      mov 10, r6
0x000a7e2c      mov 5, r7
0x000a7e2e      mov 1, r8
0x000a7e30      jarl 0xa8bb4, lp
0x000a7e34      st.w r24, -17204[gp]
0x000a7e38      mov 0xc203a, r18                                       ; ': \f'
0x000a7e3e      st.w r18, -17200[gp]
0x000a7e42      mov r26, r13
0x000a7e44      mov r0, r12
0x000a7e46      ld.bu -17806[gp], r11
0x000a7e4a      br 0xa7e64
0x000a7e4c      movea 400, r0, r11
0x000a7e50      ld.bu 3[sp], r12
0x000a7e54      ld.bu 2[sp], r13
0x000a7e58      mov 0x68d58, r25
0x000a7e5e      movea -17216, gp, r29
0x000a7e62      br 0xa7e70
0x000a7e64      movea -1000, r0, r1
0x000a7e68      cmp r1, r11
0x000a7e6a      bge 0xa7e70
0x000a7e6c      movea -1000, r0, r11
0x000a7e70      mov r13, r19
0x000a7e72      shl 2, r19
0x000a7e74      add r29, r19
0x000a7e76      mov r11, r10
0x000a7e78      shl 2, r10
0x000a7e7a      not r10, r10
0x000a7e7c      add 1, r10
0x000a7e7e      add r25, r10
0x000a7e80      ld.w 1600[r10], r15                                    ; 0x640 ; 1600
0x000a7e84      st.w r15, 0[r19]
0x000a7e88      addi 1, r13, r17
0x000a7e8c      mov r17, r13
0x000a7e8e      and r20, r13
0x000a7e90      addi 1, r12, r16
0x000a7e94      mov r16, r12
0x000a7e96      and r20, r12
0x000a7e98      cmp 10, r12
0x000a7e9a      blt 0xa7e46
0x000a7e9c      jarl 0xa8d04, lp
0x000a7ea0      jarl 0xa8c6c, lp
0x000a7ea4      movea -17624, gp, r6
0x000a7ea8      movea 64, r0, r7                                       ; '@'
0x000a7eac      jarl 0xe34d4, lp
0x000acaa0      mov 3, r25
0x000acaa2      mov r25, r26
0x000acaa4      movea 16, r0, r24
0x000acaa8      mov r24, r6
0x000acaaa      mov 5, r7
0x000acaac      mov 1, r8
0x000acaae      jarl 0xa8bb4, lp
0x000acab2      movea -17216, gp, r29
0x000acab6      movea 16, r0, r23
0x000acaba      mov r26, r10
0x000acabc      shl 2, r10
0x000acabe      add r29, r10
0x000acac0      st.w r23, 0[r10]
0x000acac4      addi 1, r26, r14
0x000acac8      mov r14, r26
0x000acaca      and r20, r26
0x000acacc      mov r26, r15
0x000acace      shl 2, r15
0x000acad0      add r29, r15
0x000acad2      mov 0xe45cc, r16
0x000acad8      st.w r16, 0[r15]
0x000acadc      addi 1, r26, r17
0x000acae0      mov r17, r26
0x000acae2      and r20, r26
0x000acae4      mov r0, r13
0x000acae6      mov 0x68b40, r27
0x000acaec      movhi -1, gp, r1
0x000acaf0      movea -9676, r1, r28
0x000acaf4      mov r27, r18
0x000acaf6      add r13, r18
0x000acaf8      ld.bu 0[r18], r12
0x000acafc      mov r13, r19
0x000acafe      shl 2, r19
0x000acb00      add r28, r19
0x000acb02      ld.b 0[r19], r10
0x000acb06      cmp -1, r10
0x000acb08      bne 0xacb1c
0x000acb0a      mov r26, r14
0x000acb0c      add r12, r14
0x000acb0e      shl 2, r14
0x000acb10      add r29, r14
0x000acb12      movhi 17096, r0, r15
0x000acb16      st.w r15, 0[r14]
0x000acb1a      br 0xacb4c
0x000acb1c      mov r13, r16
0x000acb1e      shl 2, r16
0x000acb20      add r28, r16
0x000acb22      ld.b 0[r16], r17
0x000acb26      mov 5, r1
0x000acb28      div r1, r17, r0
0x000acb2c      mov r17, r11
0x000acb2e      and r20, r11
0x000acb30      mov r26, r18
0x000acb32      add r12, r18
0x000acb34      shl 2, r18
0x000acb36      add r29, r18
0x000acb38      mov r11, r19
0x000acb3a      shl 2, r19
0x000acb3c      mov 0x6a528, r10
0x000acb42      add r19, r10
0x000acb44      ld.w 0[r10], r14
0x000acb48      st.w r14, 0[r18]
0x000acb4c      addi 1, r13, r15
0x000acb50      mov r15, r13
0x000acb52      and r20, r13
0x000acb54      cmp 15, r13
0x000acb56      ble 0xacaf4
0x000acb58      jarl 0xa8d04, lp
0x000acb5c      jarl 0xa8c6c, lp
0x000acb60      movea -17624, gp, r22
0x000acb64      mov r22, r6
0x000acb66      movea 88, r0, r7                                       ; 'X'
0x000acb6a      jarl 0xe34d4, lp
0x000acb6e      mov r25, r26
0x000acb70      mov r24, r6
0x000acb72      mov 5, r7
0x000acb74      mov 1, r8
0x000acb76      jarl 0xa8bb4, lp
0x000acb7a      mov r26, r16
0x000acb7c      shl 2, r16
0x000acb7e      add r29, r16
0x000acb80      st.w r23, 0[r16]
0x000acb84      addi 1, r26, r17
0x000acb88      mov r17, r26
0x000acb8a      and r20, r26
0x000acb8c      mov r26, r19
0x000acb8e      shl 2, r19
0x000acb90      add r29, r19
0x000acb92      mov 0xe45dc, r10
0x000acb98      st.w r10, 0[r19]
0x000acb9c      addi 1, r26, r18
0x000acba0      mov r18, r26
0x000acba2      and r20, r26
0x000acba4      mov r0, r13
0x000acba6      mov 0x6a980, r11
0x000acbac      mov r27, r14
0x000acbae      add r13, r14
0x000acbb0      ld.bu 0[r14], r12
0x000acbb4      mov r12, r15
0x000acbb6      add r26, r15
0x000acbb8      shl 2, r15
0x000acbba      add r29, r15
0x000acbbc      mov r13, r16
0x000acbbe      shl 2, r16
0x000acbc0      add r28, r16
0x000acbc2      ld.bu 1[r16], r17
0x000acbc6      shl 2, r17
0x000acbc8      add r11, r17
0x000acbca      ld.w 0[r17], r19
0x000acbce      st.w r19, 0[r15]
0x000acbd2      addi 1, r13, r10
0x000acbd6      mov r10, r13
0x000acbd8      and r20, r13
0x000acbda      cmp 15, r13
0x000acbdc      ble 0xacbac
0x000acbde      jarl 0xa8d04, lp
0x000acbe2      jarl 0xa8c6c, lp
0x000acbe6      mov r22, r6
0x000acbe8      movea 88, r0, r7                                       ; 'X'
0x000acbec      jarl 0xe34d4, lp
0x000acbf0      mov r25, r26
0x000acbf2      mov r24, r6
0x000acbf4      mov 5, r7
0x000acbf6      mov 1, r8
0x000acbf8      jarl 0xa8bb4, lp
0x000acbfc      mov r26, r18
0x000acbfe      shl 2, r18
0x000acc00      add r29, r18
0x000acc02      st.w r23, 0[r18]
0x000acc06      addi 1, r26, r14
0x000acc0a      mov r14, r26
0x000acc0c      and r20, r26
0x000acc0e      mov r26, r16
0x000acc10      shl 2, r16
0x000acc12      add r29, r16
0x000acc14      mov 0xe45ec, r17
0x000acc1a      st.w r17, 0[r16]
0x000acc1e      addi 1, r26, r15
0x000acc22      mov r15, r26
0x000acc24      and r20, r26
0x000acc26      mov r0, r13
0x000acc28      mov 0x6ad7c, r11
0x000acc2e      mov r27, r19
0x000acc30      add r13, r19
0x000acc32      ld.bu 0[r19], r12
0x000acc36      mov r12, r10
0x000acc38      add r26, r10
0x000acc3a      shl 2, r10
0x000acc3c      add r29, r10
0x000acc3e      mov r13, r18
0x000acc40      shl 2, r18
0x000acc42      add r28, r18
0x000acc44      ld.hu 2[r18], r14
0x000acc48      shl 2, r14
0x000acc4a      add r11, r14
0x000acc4c      ld.w 0[r14], r16
0x000acc50      st.w r16, 0[r10]
0x000acc54      addi 1, r13, r17
0x000acc58      mov r17, r13
0x000acc5a      and r20, r13
0x000acc5c      cmp 15, r13
0x000acc5e      ble 0xacc2e
0x000acc60      jarl 0xa8d04, lp
0x000acc64      jarl 0xa8c6c, lp
0x000acc68      mov r22, r6
0x000acc6a      movea 88, r0, r7                                       ; 'X'
0x000acc6e      jarl 0xe34d4, lp
0x000acc72      dispose 0, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000acc76      prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 0
0x000acc7a      jr 0xacaa0
0x000acc7e      nop
0x000aa144      mov 3, r28
0x000aa146      movea 16, r0, r6
0x000aa14a      mov 5, r7
0x000aa14c      mov 1, r8
0x000aa14e      jarl 0xa8bb4, lp
0x000aa152      movea -17216, gp, r29
0x000aa156      mov r28, r10
0x000aa158      shl 2, r10
0x000aa15a      add r29, r10
0x000aa15c      movea 16, r0, r14
0x000aa160      st.w r14, 0[r10]
0x000aa164      addi 1, r28, r15
0x000aa168      mov r15, r28
0x000aa16a      and r20, r28
0x000aa16c      mov r28, r16
0x000aa16e      shl 2, r16
0x000aa170      add r29, r16
0x000aa172      mov 0xe46d8, r17
0x000aa178      st.w r17, 0[r16]
0x000aa17c      addi 1, r28, r18
0x000aa180      mov r18, r28
0x000aa182      and r20, r28
0x000aa184      mov r0, r13
0x000aa186      mov 0x6a980, r11
0x000aa18c      movhi -1, gp, r1
0x000aa190      movea -13944, r1, r26
0x000aa194      mov 0x68b40, r27
0x000aa19a      mov r27, r19
0x000aa19c      add r13, r19
0x000aa19e      ld.bu 0[r19], r12
0x000aa1a2      mov r28, r10
0x000aa1a4      add r12, r10
0x000aa1a6      shl 2, r10
0x000aa1a8      add r29, r10
0x000aa1aa      mov r13, r14
0x000aa1ac      shl 2, r14
0x000aa1ae      add r26, r14
0x000aa1b0      ld.bu 1[r14], r15
0x000aa1b4      shl 2, r15
0x000aa1b6      add r11, r15
0x000aa1b8      ld.w 0[r15], r16
0x000aa1bc      st.w r16, 0[r10]
0x000aa1c0      addi 1, r13, r17
0x000aa1c4      mov r17, r13
0x000aa1c6      and r20, r13
0x000aa1c8      cmp 15, r13
0x000aa1ca      ble 0xaa19a
0x000aa1cc      jarl 0xa8d04, lp
0x000aa1d0      jarl 0xa8c6c, lp
0x000aa1d4      movea -17624, gp, r6
0x000aa1d8      movea 88, r0, r7                                       ; 'X'
0x000aa1dc      jarl 0xe34d4, lp
0x000aa1e0      mov 3, r28
0x000aa1e2      movea 16, r0, r6
0x000aa1e6      mov 5, r7
0x000aa1e8      mov 1, r8
0x000aa1ea      jarl 0xa8bb4, lp
0x000aa1ee      mov r28, r18
0x000aa1f0      shl 2, r18
0x000aa1f2      add r29, r18
0x000aa1f4      movea 16, r0, r19
0x000aa1f8      st.w r19, 0[r18]
0x000aa1fc      addi 1, r28, r14
0x000aa200      mov r14, r28
0x000aa202      and r20, r28
0x000aa204      mov r28, r15
0x000aa206      shl 2, r15
0x000aa208      add r29, r15
0x000aa20a      mov 0xe46e8, r10
0x000aa210      st.w r10, 0[r15]
0x000aa214      addi 1, r28, r16
0x000aa218      mov r16, r28
0x000aa21a      and r20, r28
0x000aa21c      mov r0, r13
0x000aa21e      mov 0x6ad7c, r11
0x000aa224      mov r27, r17
0x000aa226      add r13, r17
0x000aa228      ld.bu 0[r17], r12
0x000aa22c      mov r28, r18
0x000aa22e      add r12, r18
0x000aa230      shl 2, r18
0x000aa232      add r29, r18
0x000aa234      mov r13, r19
0x000aa236      shl 2, r19
0x000aa238      add r26, r19
0x000aa23a      ld.hu 2[r19], r14
0x000aa23e      shl 2, r14
0x000aa240      add r11, r14
0x000aa242      ld.w 0[r14], r15
0x000aa246      st.w r15, 0[r18]
0x000aa24a      addi 1, r13, r10
0x000aa24e      mov r10, r13
0x000aa250      and r20, r13
0x000aa252      cmp 15, r13
0x000aa254      ble 0xaa224
0x000aa256      jarl 0xa8d04, lp
0x000aa25a      jarl 0xa8c6c, lp
0x000aa25e      movea -17624, gp, r6
0x000aa262      movea 88, r0, r7                                       ; 'X'
0x000aa266      jarl 0xe34d4, lp
0x000aa26a      dispose 0, {r26, r27, r28, r29, lp}, lp
0x000aa26e      prepare {r26, r27, r28, r29, lp}, 0
0x000aa272      jr 0xaa144
0x000aa276      nop
0x000a8548      jarl 0x9afb8, lp
0x000a854c      st.b r10, -15915[gp]
0x000a8550      ld.b -15915[gp], r15
0x000a8554      movea 70, r0, r1                                       ; 'F'
0x000a8558      cmp r1, r15
0x000a855a      bl 0xa8570
0x000a855c      ld.bu -15915[gp], r16
0x000a8560      addi -70, r16, r16
0x000a8564      mov 5, r1
0x000a8566      div r1, r16, r0
0x000a856a      mov r16, r22
0x000a856c      and r20, r22
0x000a856e      br 0xa8572
0x000a8570      mov r0, r22
0x000a8572      movea 16, r0, r6
0x000a8576      mov 5, r7
0x000a8578      mov 1, r8
0x000a857a      jarl 0xa8bb4, lp
0x000a857e      movea 16, r0, r17
0x000a8582      st.w r17, -17204[gp]
0x000a8586      mov 0xe46c8, r18
0x000a858c      st.w r18, -17200[gp]
0x000a8590      mov 5, r23
0x000a8592      jarl 0x9b070, lp
0x000a8596      mov r10, r14
0x000a8598      movea 90, r0, r1                                       ; 'Z'
0x000a859c      cmp r1, r14
0x000a859e      setf z, r14
0x000a85a2      mov r0, r11
0x000a85a4      movhi -1, gp, r1
0x000a85a8      movea -13944, r1, r13
0x000a85ac      mov 0x68b40, r26
0x000a85b2      mov r26, r19
0x000a85b4      add r11, r19
0x000a85b6      ld.bu 0[r19], r12
0x000a85ba      mov r11, r15
0x000a85bc      shl 2, r15
0x000a85be      add r13, r15
0x000a85c0      ld.b 0[r15], r16
0x000a85c4      cmp -1, r16
0x000a85c6      bne 0xa85e0
0x000a85c8      mov r23, r17
0x000a85ca      add r12, r17
0x000a85cc      shl 2, r17
0x000a85ce      movea -17216, gp, r18
0x000a85d2      add r17, r18
0x000a85d4      movhi 17096, r0, r10
0x000a85d8      st.w r10, 0[r18]
0x000a85dc      jr 0xa86dc
0x000a85e0      mov 5, r25
0x000a85e2      mov r11, r19
0x000a85e4      shl 2, r19
0x000a85e6      add r13, r19
0x000a85e8      ld.b 0[r19], r15
0x000a85ec      div r25, r15, r0
0x000a85f0      ld.bu -16810[gp], r16
0x000a85f4      cmp r15, r16
0x000a85f6      ble 0xa860c
0x000a85f8      mov r11, r17
0x000a85fa      shl 2, r17
0x000a85fc      add r13, r17
0x000a85fe      ld.b 0[r17], r18
0x000a8602      div r25, r18, r0
0x000a8606      mov r18, r24
0x000a8608      and r20, r24
0x000a860a      br 0xa8610
0x000a860c      ld.bu -16810[gp], r24
0x000a8610      movhi -1, gp, r1
0x000a8614      movea -10583, r1, r29
0x000a8618      mov 0x6a3fc, r28
0x000a861e      mov 0x6a528, r27
0x000a8624      mov r24, r10
0x000a8626      shl 2, r10
0x000a8628      add r27, r10
0x000a862a      ld.w 0[r10], r19
0x000a862e      mov r29, r16
0x000a8630      add r11, r16
0x000a8632      ld.bu 0[r16], r15
0x000a8636      mul 100, r15, r1
0x000a863a      add r28, r15
0x000a863c      mov r22, r17
0x000a863e      shl 2, r17
0x000a8640      add r17, r15
0x000a8642      ld.w 0[r15], r18
0x000a8646      mov r18, r6
0x000a8648      mov r19, r7
0x000a864a      stsr psw, r8
0x000a864e      jarl 0xebaec, lp
0x000a8652      ldsr r6, psw
0x000a8656      bnl 0xa8694
0x000a8658      mov r23, r10
0x000a865a      add r12, r10
0x000a865c      shl 2, r10
0x000a865e      movea -17216, gp, r16
0x000a8662      add r10, r16
0x000a8664      mov r24, r17
0x000a8666      shl 2, r17
0x000a8668      add r27, r17
0x000a866a      ld.w 0[r17], r15
0x000a866e      mov r14, r19
0x000a8670      shl 6, r19
0x000a8672      mov 0x6a37c, r18
0x000a8678      add r19, r18
0x000a867a      mov r11, r10
0x000a867c      shl 2, r10
0x000a867e      add r10, r18
0x000a8680      ld.w 0[r18], r17
0x000a8684      mov r17, r6
0x000a8686      mov r15, r7
0x000a8688      jarl 0xeb984, lp
0x000a868c      mov r6, r15
0x000a868e      st.w r15, 0[r16]
0x000a8692      br 0xa86dc
0x000a8694      mov r23, r19
0x000a8696      add r12, r19
0x000a8698      shl 2, r19
0x000a869a      movea -17216, gp, r10
0x000a869e      add r19, r10
0x000a86a0      mov r29, r18
0x000a86a2      add r11, r18
0x000a86a4      ld.bu 0[r18], r17
0x000a86a8      mul 100, r17, r1
0x000a86ac      add r28, r17
0x000a86ae      mov r22, r16
0x000a86b0      shl 2, r16
0x000a86b2      add r16, r17
0x000a86b4      ld.w 0[r17], r15
0x000a86b8      mov r14, r19
0x000a86ba      shl 6, r19
0x000a86bc      mov 0x6a37c, r18
0x000a86c2      add r19, r18
0x000a86c4      mov r11, r16
0x000a86c6      shl 2, r16
0x000a86c8      add r16, r18
0x000a86ca      ld.w 0[r18], r17
0x000a86ce      mov r17, r6
0x000a86d0      mov r15, r7
0x000a86d2      jarl 0xeb984, lp
0x000a86d6      mov r6, r15
0x000a86d8      st.w r15, 0[r10]
0x000a86dc      addi 1, r11, r19
0x000a86e0      mov r19, r11
0x000a86e2      and r20, r11
0x000a86e4      cmp 15, r11
0x000a86e6      bgt 0xa86ec
0x000a86e8      jr 0xa85b2
0x000a86ec      jarl 0xa8d04, lp
0x000a86f0      jarl 0xa8c6c, lp
0x000a86f4      movea -17624, gp, r6
0x000a86f8      movea 88, r0, r7                                       ; 'X'
0x000a86fc      jarl 0xe34d4, lp
0x000a8700      dispose 0, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000a8704      prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 0
0x000a8708      jr 0xa8548
0x000a870c      jr 0xa88f8
0x000acc84      mov 3, r26
0x000acc86      movea 16, r0, r6
0x000acc8a      mov 5, r7
0x000acc8c      mov 1, r8
0x000acc8e      jarl 0xa8bb4, lp
0x000acc92      movea -17216, gp, r29
0x000acc96      mov r26, r13
0x000acc98      addi 1, r13, r10
0x000acc9c      mov r10, r26
0x000acc9e      and r20, r26
0x000acca0      mov r13, r14
0x000acca2      shl 2, r14
0x000acca4      add r29, r14
0x000acca6      movea 16, r0, r15
0x000accaa      st.w r15, 0[r14]
0x000accae      mov r26, r12
0x000accb0      addi 1, r12, r16
0x000accb4      mov r16, r26
0x000accb6      and r20, r26
0x000accb8      mov r12, r17
0x000accba      shl 2, r17
0x000accbc      add r29, r17
0x000accbe      mov 0xe4574, r18                                       ; 'tE\x0e'
0x000accc4      st.w r18, 0[r17]
0x000accc8      mov r0, r11
0x000accca      mov 0x6bd1c, r27
0x000accd0      mov 0x68b40, r28
0x000accd6      mov r28, r19
0x000accd8      add r11, r19
0x000accda      ld.bu 0[r19], r10
0x000accde      add r26, r10
0x000acce0      shl 2, r10
0x000acce2      add r29, r10
0x000acce4      mov r27, r14
0x000acce6      add r11, r14
0x000acce8      ld.bu 0[r14], r15
0x000accec      st.w r15, 0[r10]
0x000accf0      addi 1, r11, r16
0x000accf4      mov r16, r11
0x000accf6      and r20, r11
0x000accf8      cmp 15, r11
0x000accfa      ble 0xaccd6
0x000accfc      jarl 0xa8d04, lp
0x000acd00      jarl 0xa8c6c, lp
0x000acd04      movea -17624, gp, r6
0x000acd08      movea 88, r0, r7                                       ; 'X'
0x000acd0c      jarl 0xe34d4, lp
0x000acd10      mov 3, r26
0x000acd12      movea 16, r0, r6
0x000acd16      mov 5, r7
0x000acd18      mov 1, r8
0x000acd1a      jarl 0xa8bb4, lp
0x000acd1e      mov r26, r12
0x000acd20      addi 1, r12, r17
0x000acd24      mov r17, r26
0x000acd26      and r20, r26
0x000acd28      mov r12, r18
0x000acd2a      shl 2, r18
0x000acd2c      add r29, r18
0x000acd2e      movea 16, r0, r19
0x000acd32      st.w r19, 0[r18]
0x000acd36      mov r26, r11
0x000acd38      addi 1, r11, r14
0x000acd3c      mov r14, r26
0x000acd3e      and r20, r26
0x000acd40      mov r11, r10
0x000acd42      shl 2, r10
0x000acd44      add r29, r10
0x000acd46      mov 0xe461c, r15                                       ; '\x1cF\x0e'
0x000acd4c      st.w r15, 0[r10]
0x000acd50      mov r0, r13
0x000acd52      mov r28, r16
0x000acd54      add r13, r16
0x000acd56      ld.bu 0[r16], r17
0x000acd5a      add r26, r17
0x000acd5c      shl 2, r17
0x000acd5e      add r29, r17
0x000acd60      mov r27, r18
0x000acd62      add r13, r18
0x000acd64      ld.bu 0[r18], r19
0x000acd68      st.w r19, 0[r17]
0x000acd6c      addi 1, r13, r14
0x000acd70      mov r14, r13
0x000acd72      and r20, r13
0x000acd74      cmp 15, r13
0x000acd76      ble 0xacd52
0x000acd78      jarl 0xa8d04, lp
0x000acd7c      jarl 0xa8c6c, lp
0x000acd80      movea -17624, gp, r6
0x000acd84      movea 88, r0, r7                                       ; 'X'
0x000acd88      jarl 0xe34d4, lp
0x000acd8c      dispose 0, {r26, r27, r28, r29, lp}, lp
0x000acd90      prepare {r26, r27, r28, r29, lp}, 0
0x000acd94      jr 0xacc84
0x000acd98      jr 0xace9c
0x000acd9c      st.w r6, 0[sp]
0x000acda0      ld.hu 0[sp], r29
0x000acda4      cmp r21, r29
0x000acda6      bne 0xacdb0
0x000acda8      movea 40, r0, r26                                      ; '('
0x000acdac      mov r0, r27
0x000acdae      br 0xacdb4
0x000acdb0      mov 5, r26
0x000acdb2      mov r29, r27
0x000acdb4      mov 3, r28
0x000acdb6      mov r26, r6
0x000acdb8      mov 5, r7
0x000acdba      mov 1, r8
0x000acdbc      jarl 0xa8bb4, lp
0x000acdc0      movea -17216, gp, r11
0x000acdc4      mov r28, r12
0x000acdc6      addi 1, r12, r10
0x000acdca      mov r10, r28
0x000acdcc      and r20, r28
0x000acdce      mov r12, r13
0x000acdd0      shl 2, r13
0x000acdd2      add r11, r13
0x000acdd4      st.w r26, 0[r13]
0x000acdd8      cmp r21, r29
0x000acdda      bne 0xace34
0x000acddc      mov r28, r12
0x000acdde      addi 1, r12, r14
0x000acde2      mov r14, r28
0x000acde4      and r20, r28
0x000acde6      mov r12, r15
0x000acde8      shl 2, r15
0x000acdea      add r11, r15
0x000acdec      mov 0xe4594, r16
0x000acdf2      st.w r16, 0[r15]
0x000acdf6      mov r0, r29
0x000acdf8      mov 0x6bd2c, r17
0x000acdfe      add r27, r17
0x000ace00      ld.bu 0[r17], r18
0x000ace04      shl 3, r18
0x000ace06      movhi -1, gp, r1
0x000ace0a      movea -9612, r1, r19
0x000ace0e      add r18, r19
0x000ace10      mov r19, r6
0x000ace12      jarl 0xa96c4, lp
0x000ace16      mov r28, r6
0x000ace18      jarl 0xabf78, lp
0x000ace1c      mov r10, r28
0x000ace1e      addi 1, r27, r13
0x000ace22      mov r13, r27
0x000ace24      and r21, r27
0x000ace26      addi 1, r29, r14
0x000ace2a      mov r14, r29
0x000ace2c      and r20, r29
0x000ace2e      cmp 8, r29
0x000ace30      blt 0xacdf8
0x000ace32      br 0xace7c
0x000ace34      mov r28, r12
0x000ace36      addi 1, r12, r15
0x000ace3a      mov r15, r28
0x000ace3c      and r20, r28
0x000ace3e      mov r12, r16
0x000ace40      shl 2, r16
0x000ace42      add r11, r16
0x000ace44      mov 0x6bd2c, r17
0x000ace4a      add r27, r17
0x000ace4c      ld.bu 0[r17], r18
0x000ace50      mul 5, r18, r1
0x000ace54      mov 0xe4594, r1
0x000ace5a      add r1, r18
0x000ace5c      st.w r18, 0[r16]
0x000ace60      mov r27, r19
0x000ace62      shl 3, r19
0x000ace64      movhi -1, gp, r1
0x000ace68      movea -9612, r1, r10
0x000ace6c      add r19, r10
0x000ace6e      mov r10, r6
0x000ace70      jarl 0xa96c4, lp
0x000ace74      mov r28, r6
0x000ace76      jarl 0xabf78, lp
0x000ace7a      mov r10, r28
0x000ace7c      jarl 0xa8d04, lp
0x000ace80      jarl 0xa8c6c, lp
0x000ace84      movea -17624, gp, r6
0x000ace88      mov r26, r13
0x000ace8a      shl 2, r13
0x000ace8c      addi 24, r13, r13
0x000ace90      and r21, r13
0x000ace92      mov r13, r7
0x000ace94      jarl 0xe34d4, lp
0x000ace98      dispose 1, {r26, r27, r28, r29, lp}, lp
0x000ace9c      prepare {r26, r27, r28, r29, lp}, 1
0x000acea0      jr 0xacd9c
0x000acea4      jr 0xacfa8
0x000acea8      st.w r6, 0[sp]
0x000aceac      ld.hu 0[sp], r29
0x000aceb0      cmp r21, r29
0x000aceb2      bne 0xacebc
0x000aceb4      movea 40, r0, r26                                      ; '('
0x000aceb8      mov r0, r27
0x000aceba      br 0xacec0
0x000acebc      mov 5, r26
0x000acebe      mov r29, r27
0x000acec0      mov 3, r28
0x000acec2      mov r26, r6
0x000acec4      mov 5, r7
0x000acec6      mov 1, r8
0x000acec8      jarl 0xa8bb4, lp
0x000acecc      movea -17216, gp, r11
0x000aced0      mov r28, r12
0x000aced2      addi 1, r12, r10
0x000aced6      mov r10, r28
0x000aced8      and r20, r28
0x000aceda      mov r12, r13
0x000acedc      shl 2, r13
0x000acede      add r11, r13
0x000acee0      st.w r26, 0[r13]
0x000acee4      cmp r21, r29
0x000acee6      bne 0xacf40
0x000acee8      mov r28, r12
0x000aceea      addi 1, r12, r14
0x000aceee      mov r14, r28
0x000acef0      and r20, r28
0x000acef2      mov r12, r15
0x000acef4      shl 2, r15
0x000acef6      add r11, r15
0x000acef8      mov 0xe463c, r16                                       ; '<F\x0e'
0x000acefe      st.w r16, 0[r15]
0x000acf02      mov r0, r29
0x000acf04      mov 0x6bd2c, r17
0x000acf0a      add r27, r17
0x000acf0c      ld.bu 0[r17], r18
0x000acf10      shl 3, r18
0x000acf12      movhi -1, gp, r1
0x000acf16      movea -9548, r1, r19
0x000acf1a      add r18, r19
0x000acf1c      mov r19, r6
0x000acf1e      jarl 0xa96c4, lp
0x000acf22      mov r28, r6
0x000acf24      jarl 0xabf78, lp
0x000acf28      mov r10, r28
0x000acf2a      addi 1, r27, r13
0x000acf2e      mov r13, r27
0x000acf30      and r21, r27
0x000acf32      addi 1, r29, r14
0x000acf36      mov r14, r29
0x000acf38      and r20, r29
0x000acf3a      cmp 8, r29
0x000acf3c      blt 0xacf04
0x000acf3e      br 0xacf88
0x000acf40      mov r28, r12
0x000acf42      addi 1, r12, r15
0x000acf46      mov r15, r28
0x000acf48      and r20, r28
0x000acf4a      mov r12, r16
0x000acf4c      shl 2, r16
0x000acf4e      add r11, r16
0x000acf50      mov 0x6bd2c, r17
0x000acf56      add r27, r17
0x000acf58      ld.bu 0[r17], r18
0x000acf5c      mul 5, r18, r1
0x000acf60      mov 0xe463c, r1                                        ; '<F\x0e'
0x000acf66      add r1, r18
0x000acf68      st.w r18, 0[r16]
0x000acf6c      mov r27, r19
0x000acf6e      shl 3, r19
0x000acf70      movhi -1, gp, r1
0x000acf74      movea -9548, r1, r10
0x000acf78      add r19, r10
0x000acf7a      mov r10, r6
0x000acf7c      jarl 0xa96c4, lp
0x000acf80      mov r28, r6
0x000acf82      jarl 0xabf78, lp
0x000acf86      mov r10, r28
0x000acf88      jarl 0xa8d04, lp
0x000acf8c      jarl 0xa8c6c, lp
0x000acf90      movea -17624, gp, r6
0x000acf94      mov r26, r13
0x000acf96      shl 2, r13
0x000acf98      addi 24, r13, r13
0x000acf9c      and r21, r13
0x000acf9e      mov r13, r7
0x000acfa0      jarl 0xe34d4, lp
0x000acfa4      dispose 1, {r26, r27, r28, r29, lp}, lp
0x000acfa8      prepare {r26, r27, r28, r29, lp}, 1
0x000acfac      jr 0xacea8
0x000acfb0      jr 0xad0a6
