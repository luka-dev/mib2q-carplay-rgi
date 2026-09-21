0x000c68e8      2066a500       movea 165, r0, r12
0x000c68ec      a45797c3       ld.bu -15465[gp], r10
0x000c68f0      ec51           cmp r12, r10
0x000c68f2      ca05           bne 0xc68fa
0x000c68f4      44076cc2       st.b r0, -15764[gp]
0x000c68f8      b515           br 0xc691e
0x000c68fa      206e1100       movea 17, r0, r13
0x000c68fe      245e97c3       movea -15465, gp, r11
0x000c6902      0b78           mov r11, r15
0x000c6904      cd79           add r13, r15
0x000c6906      8f870100       ld.bu 0[r15], r16
0x000c690a      f061           cmp r16, r12
0x000c690c      ba05           bne 0xc6912
0x000c690e      446f6cc2       st.b r13, -15764[gp]
0x000c6912      0d8effff       addi -1, r13, r17
0x000c6916      1168           mov r17, r13
0x000c6918      5469           and r20, r13
0x000c691a      646a           cmp 4, r13
0x000c691c      bef5           bge 0xc6902
0x000c691e      0058           mov r0, r11
0x000c6920      244697c3       movea -15465, gp, r8
0x000c6924      24766cc2       movea -15764, gp, r14
0x000c6928      0e90           mov r14, r18
0x000c692a      cb91           add r11, r18
0x000c692c      929f0100       ld.bu 0[r18], r19
0x000c6930      c899           add r8, r19
0x000c6932      93570100       ld.bu 0[r19], r10
0x000c6936      ea61           cmp r10, r12
0x000c6938      fa3d           bne 0xc69b6
0x000c693a      0e78           mov r14, r15
0x000c693c      cb79           add r11, r15
0x000c693e      8f870100       ld.bu 0[r15], r16
0x000c6942      248e30c5       movea -15056, gp, r17
0x000c6946      d089           add r16, r17
0x000c6948      11970000       ld.b 0[r17], r18
0x000c694c      e091           cmp r0, r18
0x000c694e      ca35           bne 0xc69b6
0x000c6950      243ebfc3       movea -15425, gp, r7
0x000c6954      0e98           mov r14, r19
0x000c6956      cb99           add r11, r19
0x000c6958      93570100       ld.bu 0[r19], r10
0x000c695c      c751           add r7, r10
0x000c695e      0a7f0000       ld.b 0[r10], r15
0x000c6962      e079           cmp r0, r15
0x000c6964      932d           bnh 0xc69b6
0x000c6966      243670c5       movea -14992, gp, r6
0x000c696a      0e80           mov r14, r16
0x000c696c      cb81           add r11, r16
0x000c696e      908f0100       ld.bu 0[r16], r17
0x000c6972      1148           mov r17, r9
0x000c6974      0790           mov r7, r18
0x000c6976      c991           add r9, r18
0x000c6978      929f0100       ld.bu 0[r18], r19
0x000c697c      b300           sxb r19
0x000c697e      b300           sxb r19
0x000c6980      136e9cff       addi -100, r19, r13
0x000c6984      0650           mov r6, r10
0x000c6986      c951           add r9, r10
0x000c6988      0a7f0000       ld.b 0[r10], r15
0x000c698c      737a           cmp -13, r15
0x000c698e      9715           ble 0xc69b0
0x000c6990      0e80           mov r14, r16
0x000c6992      cb81           add r11, r16
0x000c6994      908f0100       ld.bu 0[r16], r17
0x000c6998      c689           add r6, r17
0x000c699a      11970000       ld.b 0[r17], r18
0x000c699e      d269           add r18, r13
0x000c69a0      ed00           sxh r13
0x000c69a2      200e1a00       movea 26, r0, r1
0x000c69a6      e169           cmp r1, r13
0x000c69a8      b705           ble 0xc69ae
0x000c69aa      206e1a00       movea 26, r0, r13
0x000c69ae      b505           br 0xc69b4
0x000c69b0      206e9cff       movea -100, r0, r13
0x000c69b4      b505           br 0xc69ba
0x000c69b6      206e9cff       movea -100, r0, r13
0x000c69ba      243689c5       movea -14967, gp, r6
0x000c69be      0698           mov r6, r19
0x000c69c0      cb99           add r11, r19
0x000c69c2      536f0000       st.b r13, 0[r19]
0x000c69c6      a4578dba       ld.bu -17779[gp], r10
0x000c69ca      ea61           cmp r10, r12
0x000c69cc      a20d           be 0xc69e0
0x000c69ce      247e9ac2       movea -15718, gp, r15
0x000c69d2      cb79           add r11, r15
0x000c69d4      0680           mov r6, r16
0x000c69d6      cb81           add r11, r16
0x000c69d8      108f0000       ld.b 0[r16], r17
0x000c69dc      4f8f0000       st.b r17, 0[r15]
0x000c69e0      0b960100       addi 1, r11, r18
0x000c69e4      1258           mov r18, r11
0x000c69e6      5459           and r20, r11
0x000c69e8      645a           cmp 4, r11
0x000c69ea      f69d           blt 0xc6928
0x000c69ec      20363400       movea 52, r0, r6                        ; '4'
0x000c69f0      bfffa0cf       jarl 0xc3990, lp
0x000c69f4      049fbdc1       ld.b -15939[gp], r19
0x000c69f8      639a           cmp 3, r19
0x000c69fa      da1d           bne 0xc6a34
0x000c69fc      2456bfc3       movea -15425, gp, r10
0x000c6a00      84876dc2       ld.bu -15764[gp], r16
0x000c6a04      d051           add r16, r10
0x000c6a06      8a7f0100       ld.bu 0[r10], r15
0x000c6a0a      af00           sxb r15
0x000c6a0c      af00           sxb r15
0x000c6a0e      0f5e9cff       addi -100, r15, r11
0x000c6a12      0b88           mov r11, r17
0x000c6a14      ea8f4002       mul 10, r17, r0
0x000c6a18      f100           sxh r17
0x000c6a1a      24976cc1       ld.h -16020[gp], r18
0x000c6a1e      f289           cmp r18, r17
0x000c6a20      a20d           be 0xc6a34
0x000c6a22      0b98           mov r11, r19
0x000c6a24      ea9f4002       mul 10, r19, r0
0x000c6a28      649f6cc1       st.h r19, -16020[gp]
0x000c6a2c      20362a00       movea 42, r0, r6                        ; '*'
0x000c6a30      bfff60cf       jarl 0xc3990, lp
0x000c6a34      40063f00       dispose 0, {lp}, lp
0x000c6a38      80072100       prepare {lp}, 0
0x000c6a3c      bf07acfe       jr 0xc68e8
