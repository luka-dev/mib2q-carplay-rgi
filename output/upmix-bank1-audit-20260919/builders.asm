0x000ab850      st.w r6, 0[sp]
0x000ab854      ld.w 0[sp], r13
0x000ab858      st.w r7, 4[sp]                                         ; 4
0x000ab85c      ld.w 4[sp], r12                                        ; 4
0x000ab860      movhi 16256, r0, r11
0x000ab864      mov r11, r6
0x000ab866      mov r12, r7
0x000ab868      stsr psw, r8
0x000ab86c      jarl 0xebaec, lp
0x000ab870      ldsr r6, psw
0x000ab874      bl 0xab8ce
0x000ab876      mov 0, r10
0x000ab878      mov r12, r14
0x000ab87a      mov r13, r6
0x000ab87c      mov r14, r7
0x000ab87e      jarl 0xeba38, lp
0x000ab882      mov r6, r14
0x000ab884      st.w r14, -17240[gp]
0x000ab888      mov r12, r15
0x000ab88a      mov r13, r6
0x000ab88c      mov r15, r7
0x000ab88e      jarl 0xebb70, lp
0x000ab892      mov r6, r15
0x000ab894      st.w r15, -17236[gp]
0x000ab898      st.w r10, -17232[gp]
0x000ab89c      mov r11, r16
0x000ab89e      mov r13, r6
0x000ab8a0      mov r16, r7
0x000ab8a2      jarl 0xeba38, lp
0x000ab8a6      mov r6, r16
0x000ab8a8      mov r11, r17
0x000ab8aa      mov r16, r6
0x000ab8ac      mov r17, r7
0x000ab8ae      jarl 0xebbcc, lp
0x000ab8b2      mov r6, r17
0x000ab8b4      st.w r17, -17228[gp]
0x000ab8b8      mov r11, r18
0x000ab8ba      mov r13, r6
0x000ab8bc      mov r18, r7
0x000ab8be      jarl 0xebb70, lp
0x000ab8c2      mov r6, r18
0x000ab8c4      st.w r18, -17224[gp]
0x000ab8c8      st.w r10, -17220[gp]
0x000ab8cc      br 0xab932
0x000ab8ce      mov 0, r10
0x000ab8d0      mov r11, r19
0x000ab8d2      mov r12, r6
0x000ab8d4      mov r19, r7
0x000ab8d6      jarl 0xebbcc, lp
0x000ab8da      mov r6, r19
0x000ab8dc      mov r19, r12
0x000ab8de      mov r11, r14
0x000ab8e0      mov r13, r6
0x000ab8e2      mov r14, r7
0x000ab8e4      jarl 0xeba38, lp
0x000ab8e8      mov r6, r14
0x000ab8ea      st.w r14, -17240[gp]
0x000ab8ee      mov r11, r15
0x000ab8f0      mov r13, r6
0x000ab8f2      mov r15, r7
0x000ab8f4      jarl 0xebb70, lp
0x000ab8f8      mov r6, r15
0x000ab8fa      st.w r15, -17236[gp]
0x000ab8fe      st.w r10, -17232[gp]
0x000ab902      mov r12, r16
0x000ab904      mov r13, r6
0x000ab906      mov r16, r7
0x000ab908      jarl 0xeba38, lp
0x000ab90c      mov r6, r16
0x000ab90e      mov r11, r17
0x000ab910      mov r16, r6
0x000ab912      mov r17, r7
0x000ab914      jarl 0xebbcc, lp
0x000ab918      mov r6, r17
0x000ab91a      st.w r17, -17228[gp]
0x000ab91e      mov r12, r18
0x000ab920      mov r13, r6
0x000ab922      mov r18, r7
0x000ab924      jarl 0xebb70, lp
0x000ab928      mov r6, r18
0x000ab92a      st.w r18, -17224[gp]
0x000ab92e      st.w r10, -17220[gp]
0x000ab932      dispose 2, {lp}, lp
0x000ab936      prepare {lp}, 2
0x000ab93a      br 0xab850
0x000ab93c      jr 0xaba3e
0x000ab940      st.w r6, 0[sp]
0x000ab944      ld.w 0[sp], r11
0x000ab948      st.w r7, 4[sp]                                         ; 4
0x000ab94c      ld.w 4[sp], r10                                        ; 4
0x000ab950      movhi 16256, r0, r13
0x000ab954      mov r13, r6
0x000ab956      mov r10, r7
0x000ab958      stsr psw, r8
0x000ab95c      jarl 0xebaec, lp
0x000ab960      ldsr r6, psw
0x000ab964      bl 0xab9ca
0x000ab966      mov 0, r12
0x000ab968      mov r10, r14
0x000ab96a      mov r11, r6
0x000ab96c      mov r14, r7
0x000ab96e      jarl 0xeb984, lp
0x000ab972      mov r6, r14
0x000ab974      mov r14, r15
0x000ab976      mov r13, r6
0x000ab978      mov r15, r7
0x000ab97a      jarl 0xeba38, lp
0x000ab97e      mov r6, r15
0x000ab980      st.w r15, -17240[gp]
0x000ab984      mov r13, r16
0x000ab986      mov r14, r6
0x000ab988      mov r16, r7
0x000ab98a      jarl 0xebb70, lp
0x000ab98e      mov r6, r16
0x000ab990      st.w r16, -17236[gp]
0x000ab994      st.w r12, -17232[gp]
0x000ab998      mov r13, r17
0x000ab99a      mov r11, r6
0x000ab99c      mov r17, r7
0x000ab99e      jarl 0xeba38, lp
0x000ab9a2      mov r6, r17
0x000ab9a4      mov r13, r18
0x000ab9a6      mov r17, r6
0x000ab9a8      mov r18, r7
0x000ab9aa      jarl 0xebbcc, lp
0x000ab9ae      mov r6, r18
0x000ab9b0      st.w r18, -17228[gp]
0x000ab9b4      mov r13, r19
0x000ab9b6      mov r11, r6
0x000ab9b8      mov r19, r7
0x000ab9ba      jarl 0xebb70, lp
0x000ab9be      mov r6, r19
0x000ab9c0      st.w r19, -17224[gp]
0x000ab9c4      st.w r12, -17220[gp]
0x000ab9c8      br 0xaba3a
0x000ab9ca      mov 0, r12
0x000ab9cc      mov r13, r15
0x000ab9ce      mov r10, r6
0x000ab9d0      mov r15, r7
0x000ab9d2      jarl 0xebbcc, lp
0x000ab9d6      mov r6, r15
0x000ab9d8      mov r15, r10
0x000ab9da      mov r13, r16
0x000ab9dc      mov r11, r6
0x000ab9de      mov r16, r7
0x000ab9e0      jarl 0xeba38, lp
0x000ab9e4      mov r6, r16
0x000ab9e6      st.w r16, -17240[gp]
0x000ab9ea      mov r13, r17
0x000ab9ec      mov r11, r6
0x000ab9ee      mov r17, r7
0x000ab9f0      jarl 0xebb70, lp
0x000ab9f4      mov r6, r17
0x000ab9f6      st.w r17, -17236[gp]
0x000ab9fa      st.w r12, -17232[gp]
0x000ab9fe      mov r10, r14
0x000aba00      mov r11, r6
0x000aba02      mov r14, r7
0x000aba04      jarl 0xeb984, lp
0x000aba08      mov r6, r14
0x000aba0a      mov r13, r18
0x000aba0c      mov r14, r6
0x000aba0e      mov r18, r7
0x000aba10      jarl 0xeba38, lp
0x000aba14      mov r6, r18
0x000aba16      mov r13, r19
0x000aba18      mov r18, r6
0x000aba1a      mov r19, r7
0x000aba1c      jarl 0xebbcc, lp
0x000aba20      mov r6, r19
0x000aba22      st.w r19, -17228[gp]
0x000aba26      mov r13, r15
0x000aba28      mov r14, r6
0x000aba2a      mov r15, r7
0x000aba2c      jarl 0xebb70, lp
0x000aba30      mov r6, r15
0x000aba32      st.w r15, -17224[gp]
0x000aba36      st.w r12, -17220[gp]
0x000aba3a      dispose 2, {lp}, lp
0x000aba3e      prepare {lp}, 2
0x000aba42      jr 0xab940
0x000aba46      nop
0x000aba48      jr 0xabc46
0x000aba4c      st.w r6, 0[sp]
0x000aba50      ld.w 0[sp], r10
0x000aba54      st.w r7, 4[sp]                                         ; 4
0x000aba58      ld.w 4[sp], r13                                        ; 4
0x000aba5c      st.w r8, 8[sp]
0x000aba60      ld.w 8[sp], r14
0x000aba64      st.w r9, 12[sp]                                        ; 0xc
0x000aba68      ld.h 12[sp], r9                                        ; 0xc
0x000aba6c      cmp r0, r9
0x000aba6e      bge 0xaba74
0x000aba70      jr 0xabb56
0x000aba74      movhi 16384, r0, r29
0x000aba78      movhi 16256, r0, r12
0x000aba7c      mov r10, r15
0x000aba7e      mov r10, r11
0x000aba80      mov r15, r6
0x000aba82      mov r11, r7
0x000aba84      jarl 0xeb984, lp
0x000aba88      mov r6, r11
0x000aba8a      mov r10, r16
0x000aba8c      mov r14, r6
0x000aba8e      mov r16, r7
0x000aba90      jarl 0xeb984, lp
0x000aba94      mov r6, r16
0x000aba96      mov r13, r6
0x000aba98      mov r16, r7
0x000aba9a      jarl 0xeb984, lp
0x000aba9e      mov r6, r16
0x000abaa0      mov r16, r27
0x000abaa2      mov r11, r17
0x000abaa4      mov r27, r6
0x000abaa6      mov r17, r7
0x000abaa8      jarl 0xeba38, lp
0x000abaac      mov r6, r17
0x000abaae      mov r12, r6
0x000abab0      mov r17, r7
0x000abab2      jarl 0xeba38, lp
0x000abab6      mov r6, r17
0x000abab8      st.w r17, -17240[gp]
0x000ababc      mov r10, r18
0x000ababe      mov r29, r6
0x000abac0      mov r18, r7
0x000abac2      jarl 0xeb984, lp
0x000abac6      mov r6, r18
0x000abac8      mov r10, r19
0x000abaca      mov r18, r6
0x000abacc      mov r19, r7
0x000abace      jarl 0xeb984, lp
0x000abad2      mov r6, r19
0x000abad4      mov r29, r15
0x000abad6      mov r19, r6
0x000abad8      mov r15, r7
0x000abada      jarl 0xebb70, lp
0x000abade      mov r6, r15
0x000abae0      st.w r15, -17236[gp]
0x000abae4      mov r11, r16
0x000abae6      mov r27, r6
0x000abae8      mov r16, r7
0x000abaea      jarl 0xebb70, lp
0x000abaee      mov r6, r16
0x000abaf0      mov r12, r6
0x000abaf2      mov r16, r7
0x000abaf4      jarl 0xeba38, lp
0x000abaf8      mov r6, r16
0x000abafa      st.w r16, -17232[gp]
0x000abafe      mov r10, r28
0x000abb00      mov r13, r6
0x000abb02      mov r28, r7
0x000abb04      jarl 0xeb984, lp
0x000abb08      mov r6, r28
0x000abb0a      mov r11, r17
0x000abb0c      mov r28, r6
0x000abb0e      mov r17, r7
0x000abb10      jarl 0xeba38, lp
0x000abb14      mov r6, r17
0x000abb16      mov r12, r6
0x000abb18      mov r17, r7
0x000abb1a      jarl 0xeba38, lp
0x000abb1e      mov r6, r17
0x000abb20      mov r12, r18
0x000abb22      mov r17, r6
0x000abb24      mov r18, r7
0x000abb26      jarl 0xebbcc, lp
0x000abb2a      mov r6, r18
0x000abb2c      st.w r18, -17228[gp]
0x000abb30      ld.w -17236[gp], r19
0x000abb34      st.w r19, -17224[gp]
0x000abb38      mov r11, r15
0x000abb3a      mov r28, r6
0x000abb3c      mov r15, r7
0x000abb3e      jarl 0xebb70, lp
0x000abb42      mov r6, r15
0x000abb44      mov r12, r6
0x000abb46      mov r15, r7
0x000abb48      jarl 0xeba38, lp
0x000abb4c      mov r6, r15
0x000abb4e      st.w r15, -17220[gp]
0x000abb52      jr 0xabc42
0x000abb56      movhi 16384, r0, r29
0x000abb5a      movhi 16256, r0, r12
0x000abb5e      mov r12, r16
0x000abb60      mov r14, r6
0x000abb62      mov r16, r7
0x000abb64      jarl 0xebbcc, lp
0x000abb68      mov r6, r16
0x000abb6a      mov r16, r14
0x000abb6c      mov r10, r17
0x000abb6e      mov r10, r11
0x000abb70      mov r17, r6
0x000abb72      mov r11, r7
0x000abb74      jarl 0xeb984, lp
0x000abb78      mov r6, r11
0x000abb7a      mov r10, r27
0x000abb7c      mov r13, r6
0x000abb7e      mov r27, r7
0x000abb80      jarl 0xeb984, lp
0x000abb84      mov r6, r27
0x000abb86      mov r11, r18
0x000abb88      mov r27, r6
0x000abb8a      mov r18, r7
0x000abb8c      jarl 0xeba38, lp
0x000abb90      mov r6, r18
0x000abb92      mov r12, r6
0x000abb94      mov r18, r7
0x000abb96      jarl 0xeba38, lp
0x000abb9a      mov r6, r18
0x000abb9c      st.w r18, -17240[gp]
0x000abba0      mov r10, r19
0x000abba2      mov r29, r6
0x000abba4      mov r19, r7
0x000abba6      jarl 0xeb984, lp
0x000abbaa      mov r6, r19
0x000abbac      mov r10, r15
0x000abbae      mov r19, r6
0x000abbb0      mov r15, r7
0x000abbb2      jarl 0xeb984, lp
0x000abbb6      mov r6, r15
0x000abbb8      mov r29, r16
0x000abbba      mov r15, r6
0x000abbbc      mov r16, r7
0x000abbbe      jarl 0xebb70, lp
0x000abbc2      mov r6, r16
0x000abbc4      st.w r16, -17236[gp]
0x000abbc8      mov r11, r17
0x000abbca      mov r27, r6
0x000abbcc      mov r17, r7
0x000abbce      jarl 0xebb70, lp
0x000abbd2      mov r6, r17
0x000abbd4      mov r12, r6
0x000abbd6      mov r17, r7
0x000abbd8      jarl 0xeba38, lp
0x000abbdc      mov r6, r17
0x000abbde      st.w r17, -17232[gp]
0x000abbe2      mov r10, r18
0x000abbe4      mov r14, r6
0x000abbe6      mov r18, r7
0x000abbe8      jarl 0xeb984, lp
0x000abbec      mov r6, r18
0x000abbee      mov r13, r6
0x000abbf0      mov r18, r7
0x000abbf2      jarl 0xeb984, lp
0x000abbf6      mov r6, r18
0x000abbf8      mov r18, r28
0x000abbfa      mov r11, r19
0x000abbfc      mov r28, r6
0x000abbfe      mov r19, r7
0x000abc00      jarl 0xeba38, lp
0x000abc04      mov r6, r19
0x000abc06      mov r12, r6
0x000abc08      mov r19, r7
0x000abc0a      jarl 0xeba38, lp
0x000abc0e      mov r6, r19
0x000abc10      mov r12, r15
0x000abc12      mov r19, r6
0x000abc14      mov r15, r7
0x000abc16      jarl 0xebbcc, lp
0x000abc1a      mov r6, r15
0x000abc1c      st.w r15, -17228[gp]
0x000abc20      ld.w -17236[gp], r16
0x000abc24      st.w r16, -17224[gp]
0x000abc28      mov r11, r17
0x000abc2a      mov r28, r6
0x000abc2c      mov r17, r7
0x000abc2e      jarl 0xebb70, lp
0x000abc32      mov r6, r17
0x000abc34      mov r12, r6
0x000abc36      mov r17, r7
0x000abc38      jarl 0xeba38, lp
0x000abc3c      mov r6, r17
0x000abc3e      st.w r17, -17220[gp]
0x000abc42      dispose 4, {r27, r28, r29, lp}, lp
0x000abc46      prepare {r27, r28, r29, lp}, 4
0x000abc4a      jr 0xaba4c
0x000abc4e      nop
0x000abc50      br 0xabca0
0x000abc52      st.w r6, 0[sp]
0x000abc56      ld.w 0[sp], r12
0x000abc5a      movhi 16256, r0, r11
0x000abc5e      mov 0, r10
0x000abc60      st.w r11, -17240[gp]
0x000abc64      st.w r11, -17236[gp]
0x000abc68      st.w r10, -17232[gp]
0x000abc6c      mov r11, r13
0x000abc6e      mov r12, r6
0x000abc70      mov r13, r7
0x000abc72      jarl 0xeba38, lp
0x000abc76      mov r6, r13
0x000abc78      mov r11, r14
0x000abc7a      mov r13, r6
0x000abc7c      mov r14, r7
0x000abc7e      jarl 0xebbcc, lp
0x000abc82      mov r6, r14
0x000abc84      st.w r14, -17228[gp]
0x000abc88      mov r11, r15
0x000abc8a      mov r12, r6
0x000abc8c      mov r15, r7
0x000abc8e      jarl 0xebb70, lp
0x000abc92      mov r6, r15
0x000abc94      st.w r15, -17224[gp]
0x000abc98      st.w r10, -17220[gp]
0x000abc9c      dispose 1, {lp}, lp
0x000abca0      prepare {lp}, 1
0x000abca4      br 0xabc52
0x000abca6      nop
0x000abca8      br 0xabd04
0x000abcaa      st.w r6, 0[sp]
0x000abcae      ld.w 0[sp], r12
0x000abcb2      mov 0, r11
0x000abcb4      movhi 16256, r0, r10
0x000abcb8      st.w r12, -17240[gp]
0x000abcbc      mov r0, r13
0x000abcbe      mov r12, r6
0x000abcc0      mov r13, r7
0x000abcc2      jarl 0xebb70, lp
0x000abcc6      mov r6, r13
0x000abcc8      st.w r13, -17236[gp]
0x000abccc      st.w r11, -17232[gp]
0x000abcd0      mov r12, r14
0x000abcd2      mov r10, r6
0x000abcd4      mov r14, r7
0x000abcd6      jarl 0xeba38, lp
0x000abcda      mov r6, r14
0x000abcdc      mov r10, r15
0x000abcde      mov r14, r6
0x000abce0      mov r15, r7
0x000abce2      jarl 0xebbcc, lp
0x000abce6      mov r6, r15
0x000abce8      st.w r15, -17228[gp]
0x000abcec      mov r10, r16
0x000abcee      mov r12, r6
0x000abcf0      mov r16, r7
0x000abcf2      jarl 0xebb70, lp
0x000abcf6      mov r6, r16
0x000abcf8      st.w r16, -17224[gp]
0x000abcfc      st.w r11, -17220[gp]
0x000abd00      dispose 1, {lp}, lp
0x000abd04      prepare {lp}, 1
0x000abd08      br 0xabcaa
0x000abd0a      nop
0x000abd0c      br 0xabdb8
0x000abd0e      st.w r6, 0[sp]
0x000abd12      ld.w 0[sp], r11
0x000abd16      st.w r7, 4[sp]                                         ; 4
0x000abd1a      movhi 16384, r0, r29
0x000abd1e      movhi 16256, r0, r10
0x000abd22      st.w r10, -17240[gp]
0x000abd26      st.w r29, -17236[gp]
0x000abd2a      st.w r10, -17232[gp]
0x000abd2e      mov r11, r15
0x000abd30      mov r11, r12
0x000abd32      mov r15, r6
0x000abd34      mov r12, r7
0x000abd36      jarl 0xeb984, lp
0x000abd3a      mov r6, r12
0x000abd3c      mov r29, r14
0x000abd3e      mov r11, r6
0x000abd40      mov r14, r7
0x000abd42      jarl 0xeb984, lp
0x000abd46      mov r6, r14
0x000abd48      mov r14, r13
0x000abd4a      ld.w 4[sp], r16                                        ; 4
0x000abd4e      mov r16, r6
0x000abd50      mov r13, r7
0x000abd52      jarl 0xeb984, lp
0x000abd56      mov r6, r13
0x000abd58      mov r12, r17
0x000abd5a      mov r13, r6
0x000abd5c      mov r17, r7
0x000abd5e      jarl 0xeba38, lp
0x000abd62      mov r6, r17
0x000abd64      mov r10, r6
0x000abd66      mov r17, r7
0x000abd68      jarl 0xeba38, lp
0x000abd6c      mov r6, r17
0x000abd6e      mov r10, r18
0x000abd70      mov r17, r6
0x000abd72      mov r18, r7
0x000abd74      jarl 0xebbcc, lp
0x000abd78      mov r6, r18
0x000abd7a      st.w r18, -17228[gp]
0x000abd7e      mov r11, r19
0x000abd80      mov r14, r6
0x000abd82      mov r19, r7
0x000abd84      jarl 0xeb984, lp
0x000abd88      mov r6, r19
0x000abd8a      mov r29, r15
0x000abd8c      mov r19, r6
0x000abd8e      mov r15, r7
0x000abd90      jarl 0xebb70, lp
0x000abd94      mov r6, r15
0x000abd96      st.w r15, -17224[gp]
0x000abd9a      mov r12, r16
0x000abd9c      mov r13, r6
0x000abd9e      mov r16, r7
0x000abda0      jarl 0xebb70, lp
0x000abda4      mov r6, r16
0x000abda6      mov r10, r6
0x000abda8      mov r16, r7
0x000abdaa      jarl 0xeba38, lp
0x000abdae      mov r6, r16
0x000abdb0      st.w r16, -17220[gp]
0x000abdb4      dispose 2, {r29, lp}, lp
0x000abdb8      prepare {r29, lp}, 2
0x000abdbc      br 0xabd0e
0x000abdbe      nop
0x000abdc0      jr 0xabe84
0x000abdc4      st.w r6, 0[sp]
0x000abdc8      ld.w 0[sp], r10
0x000abdcc      st.w r7, 4[sp]                                         ; 4
0x000abdd0      movhi 16256, r0, r14
0x000abdd4      mov r10, r15
0x000abdd6      mov r10, r16
0x000abdd8      mov r15, r6
0x000abdda      mov r16, r7
0x000abddc      jarl 0xeb984, lp
0x000abde0      mov r6, r16
0x000abde2      st.w r16, -17240[gp]
0x000abde6      mov r10, r12
0x000abde8      movhi 16384, r0, r17
0x000abdec      mov r17, r6
0x000abdee      mov r12, r7
0x000abdf0      jarl 0xeb984, lp
0x000abdf4      mov r6, r12
0x000abdf6      mov r12, r13
0x000abdf8      mov r10, r6
0x000abdfa      mov r13, r7
0x000abdfc      jarl 0xeb984, lp
0x000abe00      mov r6, r13
0x000abe02      mov r0, r18
0x000abe04      mov r13, r6
0x000abe06      mov r18, r7
0x000abe08      jarl 0xebb70, lp
0x000abe0c      mov r6, r18
0x000abe0e      st.w r18, -17236[gp]
0x000abe12      ld.w -17240[gp], r19
0x000abe16      st.w r19, -17232[gp]
0x000abe1a      mov r12, r11
0x000abe1c      ld.w 4[sp], r15                                        ; 4
0x000abe20      mov r15, r6
0x000abe22      mov r11, r7
0x000abe24      jarl 0xeb984, lp
0x000abe28      mov r6, r11
0x000abe2a      ld.w -17240[gp], r16
0x000abe2e      mov r11, r6
0x000abe30      mov r16, r7
0x000abe32      jarl 0xeba38, lp
0x000abe36      mov r6, r16
0x000abe38      mov r14, r6
0x000abe3a      mov r16, r7
0x000abe3c      jarl 0xeba38, lp
0x000abe40      mov r6, r16
0x000abe42      mov r14, r17
0x000abe44      mov r16, r6
0x000abe46      mov r17, r7
0x000abe48      jarl 0xebbcc, lp
0x000abe4c      mov r6, r17
0x000abe4e      st.w r17, -17228[gp]
0x000abe52      movhi 16384, r0, r18
0x000abe56      mov r13, r6
0x000abe58      mov r18, r7
0x000abe5a      jarl 0xebb70, lp
0x000abe5e      mov r6, r18
0x000abe60      st.w r18, -17224[gp]
0x000abe64      ld.w -17240[gp], r19
0x000abe68      mov r11, r6
0x000abe6a      mov r19, r7
0x000abe6c      jarl 0xebb70, lp
0x000abe70      mov r6, r19
0x000abe72      mov r14, r6
0x000abe74      mov r19, r7
0x000abe76      jarl 0xeba38, lp
0x000abe7a      mov r6, r19
0x000abe7c      st.w r19, -17220[gp]
0x000abe80      dispose 2, {lp}, lp
0x000abe84      prepare {lp}, 2
0x000abe88      br 0xabdc4
0x000abe8a      nop
0x000abe8c      br 0xabf46
0x000abe8e      st.w r6, 0[sp]
0x000abe92      ld.w 0[sp], r10
0x000abe96      st.w r7, 4[sp]                                         ; 4
0x000abe9a      ld.w 4[sp], r14                                        ; 4
0x000abe9e      movhi 16256, r0, r13
0x000abea2      mov r10, r11
0x000abea4      mov r14, r6
0x000abea6      mov r11, r7
0x000abea8      jarl 0xeb984, lp
0x000abeac      mov r6, r11
0x000abeae      mov r10, r15
0x000abeb0      mov r10, r12
0x000abeb2      mov r15, r6
0x000abeb4      mov r12, r7
0x000abeb6      jarl 0xeb984, lp
0x000abeba      mov r6, r12
0x000abebc      mov r13, r16
0x000abebe      mov r11, r6
0x000abec0      mov r16, r7
0x000abec2      jarl 0xebb70, lp
0x000abec6      mov r6, r16
0x000abec8      mov r12, r6
0x000abeca      mov r16, r7
0x000abecc      jarl 0xeba38, lp
0x000abed0      mov r6, r16
0x000abed2      st.w r16, -17240[gp]
0x000abed6      mov r10, r17
0x000abed8      movhi 16384, r0, r18
0x000abedc      mov r18, r6
0x000abede      mov r17, r7
0x000abee0      jarl 0xeb984, lp
0x000abee4      mov r6, r17
0x000abee6      mov r10, r19
0x000abee8      mov r17, r6
0x000abeea      mov r19, r7
0x000abeec      jarl 0xeb984, lp
0x000abef0      mov r6, r19
0x000abef2      movhi 16384, r0, r15
0x000abef6      mov r19, r6
0x000abef8      mov r15, r7
0x000abefa      jarl 0xebb70, lp
0x000abefe      mov r6, r15
0x000abf00      st.w r15, -17236[gp]
0x000abf04      mov r13, r16
0x000abf06      mov r11, r6
0x000abf08      mov r16, r7
0x000abf0a      jarl 0xeba38, lp
0x000abf0e      mov r6, r16
0x000abf10      mov r12, r6
0x000abf12      mov r16, r7
0x000abf14      jarl 0xeba38, lp
0x000abf18      mov r6, r16
0x000abf1a      st.w r16, -17232[gp]
0x000abf1e      mov r13, r18
0x000abf20      ld.w -17232[gp], r17
0x000abf24      mov r17, r6
0x000abf26      mov r18, r7
0x000abf28      jarl 0xebbcc, lp
0x000abf2c      mov r6, r18
0x000abf2e      st.w r18, -17228[gp]
0x000abf32      ld.w -17236[gp], r19
0x000abf36      st.w r19, -17224[gp]
0x000abf3a      ld.w -17240[gp], r15
0x000abf3e      st.w r15, -17220[gp]
0x000abf42      dispose 2, {lp}, lp
0x000abf46      prepare {lp}, 2
0x000abf4a      br 0xabe8e
0x000abf4c      br 0xabf70
0x000abf4e      movhi 16256, r0, r11
0x000abf52      mov 0, r10
0x000abf54      st.w r11, -17240[gp]
0x000abf58      st.w r10, -17236[gp]
0x000abf5c      st.w r10, -17232[gp]
0x000abf60      st.w r11, -17228[gp]
0x000abf64      st.w r10, -17224[gp]
0x000abf68      st.w r10, -17220[gp]
0x000abf6c      dispose 0, {lp}, lp
0x000abf70      prepare {lp}, 0
0x000abf74      br 0xabf4e
0x000abf76      nop
0x000abf78      jr 0xac05a
0x000abf7c      st.w r6, 0[sp]
