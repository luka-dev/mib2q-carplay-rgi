0x000a21d4      63370500       st.w r6, 4[sp]                          ; 4
0x000a21d8      23cf0500       ld.w 4[sp], r25                         ; 4
0x000a21dc      20d65a00       movea 90, r0, r26                       ; 'Z'
0x000a21e0      20761200       movea 18, r0, r14
0x000a21e4      99df1300       ld.bu 18[r25], r27                      ; 0x12
0x000a21e8      206e1500       movea 21, r0, r13
0x000a21ec      b9e71500       ld.bu 21[r25], r28                      ; 0x15
0x000a21f0      20661700       movea 23, r0, r12
0x000a21f4      1c58           mov r28, r11
0x000a21f6      854d           br 0xa2286
0x000a21f8      2056e2ff       movea -30, r0, r10
0x000a21fc      44578eba       st.b r10, -17778[gp]
0x000a2200      0932           mov 9, r6
0x000a2202      203e1d00       movea 29, r0, r7
0x000a2206      83ffceb0       jarl 0xdd2d4, lp
0x000a220a      0532           mov 5, r6
0x000a220c      203e5100       movea 81, r0, r7                        ; 'Q'
0x000a2210      1c40           mov r28, r8
0x000a2212      044f8eba       ld.b -17778[gp], r9
0x000a2216      84ffda80       jarl 0xea2f0, lp
0x000a221a      63cf0500       st.w r25, 4[sp]                         ; 4
0x000a221e      953d           br 0xa2290
0x000a2220      1978           mov r25, r15
0x000a2222      cc79           add r12, r15
0x000a2224      8fef0100       ld.bu 0[r15], r29
0x000a2228      bd00           sxb r29
0x000a222a      0532           mov 5, r6
0x000a222c      203e5100       movea 81, r0, r7                        ; 'Q'
0x000a2230      1c40           mov r28, r8
0x000a2232      1d48           mov r29, r9
0x000a2234      84ffbc80       jarl 0xea2f0, lp
0x000a2238      200e9cff       movea -100, r0, r1
0x000a223c      e1e9           cmp r1, r29
0x000a223e      d60d           blt 0xa2258
0x000a2240      200e1a00       movea 26, r0, r1
0x000a2244      e1e9           cmp r1, r29
0x000a2246      9f0d           bgt 0xa2258
0x000a2248      44ef8eba       st.b r29, -17778[gp]
0x000a224c      0932           mov 9, r6
0x000a224e      203e1d00       movea 29, r0, r7
0x000a2252      83ff82b0       jarl 0xdd2d4, lp
0x000a2256      850d           br 0xa2266
0x000a2258      1b30           mov r27, r6
0x000a225a      203e3100       movea 49, r0, r7                        ; '1'
0x000a225e      bfff06e4       jarl 0xa0664, lp
0x000a2262      20d6a500       movea 165, r0, r26
0x000a2266      63cf0500       st.w r25, 4[sp]                         ; 4
0x000a226a      b515           br 0xa2290
0x000a226c      1b30           mov r27, r6
0x000a226e      203e3100       movea 49, r0, r7                        ; '1'
0x000a2272      bffff2e3       jarl 0xa0664, lp
0x000a2276      20d6a500       movea 165, r0, r26
0x000a227a      63cf0500       st.w r25, 4[sp]                         ; 4
0x000a227e      950d           br 0xa2290
0x000a2280      a3d70300       ld.bu 3[sp], r26
0x000a2284      e505           br 0xa2290
0x000a2286      e059           cmp r0, r11
0x000a2288      82bd           be 0xa21f8
0x000a228a      635a           cmp 3, r11
0x000a228c      a2cd           be 0xa2220
0x000a228e      f5ed           br 0xa226c
0x000a2290      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000a2294      e1d1           cmp r1, r26
0x000a2296      da05           bne 0xa22a0
0x000a2298      23370500       ld.w 4[sp], r6                          ; 4
0x000a229c      80ff7005       jarl 0xa280c, lp
0x000a22a0      4406ff70       dispose 2, {r25, r26, r27, r28, r29, lp}, lp
0x000a22a4      8407e170       prepare {r25, r26, r27, r28, r29, lp}, 2
0x000a22a8      e595           br 0xa21d4
0x000a22aa      0000           nop
