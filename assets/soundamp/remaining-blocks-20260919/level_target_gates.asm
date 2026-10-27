0x000aefc0      0060           mov r0, r12
0x000aefc2      24569ac2       movea -15718, gp, r10
0x000aefc6      245e0088       movea -30720, gp, r11
0x000aefca      0b78           mov r11, r15
0x000aefcc      cc79           add r12, r15
0x000aefce      0a80           mov r10, r16
0x000aefd0      cc81           add r12, r16
0x000aefd2      108f0000       ld.b 0[r16], r17
0x000aefd6      4f8f0000       st.b r17, 0[r15]
0x000aefda      0c960100       addi 1, r12, r18
0x000aefde      1260           mov r18, r12
0x000aefe0      5461           and r20, r12
0x000aefe2      6462           cmp 4, r12
0x000aefe4      b6f5           blt 0xaefca
0x000aefe6      20565a00       movea 90, r0, r10                       ; 'Z'
0x000aefea      849fe387       ld.bu -30750[gp], r19
0x000aefee      ea99           cmp r10, r19
0x000aeff0      ba0d           bne 0xaf006
0x000aeff2      a487e187       ld.bu -30751[gp], r16
0x000aeff6      f051           cmp r16, r10
0x000aeff8      fa05           bne 0xaf006
0x000aeffa      047f8fba       ld.b -17777[gp], r15
0x000aeffe      cf7e0300       andi 3, r15, r15
0x000af002      e079           cmp r0, r15
0x000af004      e20d           be 0xaf020
0x000af006      0060           mov r0, r12
0x000af008      20569cff       movea -100, r0, r10
0x000af00c      0b88           mov r11, r17
0x000af00e      cc89           add r12, r17
0x000af010      51570000       st.b r10, 0[r17]
0x000af014      0c960100       addi 1, r12, r18
0x000af018      1260           mov r18, r12
0x000af01a      5461           and r20, r12
0x000af01c      6462           cmp 4, r12
0x000af01e      f6f5           blt 0xaf00c
0x000af020      049f8fba       ld.b -17777[gp], r19
0x000af024      d39e3c00       andi 60, r19, r19
0x000af028      e099           cmp r0, r19
0x000af02a      d205           be 0xaf034
0x000af02c      20869cff       movea -100, r0, r16
0x000af030      44870088       st.b r16, -30720[gp]
0x000af034      0060           mov r0, r12
0x000af036      20569cff       movea -100, r0, r10
0x000af03a      0b78           mov r11, r15
0x000af03c      cc79           add r12, r15
0x000af03e      0f8f0000       ld.b 0[r15], r17
0x000af042      ea89           cmp r10, r17
0x000af044      f60d           blt 0xaf062
0x000af046      24966cc2       movea -15764, gp, r18
0x000af04a      cc91           add r12, r18
0x000af04c      929f0100       ld.bu 0[r18], r19
0x000af050      248697c3       movea -15465, gp, r16
0x000af054      d381           add r19, r16
0x000af056      907f0100       ld.bu 0[r16], r15
0x000af05a      200ea500       movea 165, r0, r1
0x000af05e      e179           cmp r1, r15
0x000af060      f205           be 0xaf06e
0x000af062      0b88           mov r11, r17
0x000af064      cc89           add r12, r17
0x000af066      20969cff       movea -100, r0, r18
0x000af06a      51970000       st.b r18, 0[r17]
0x000af06e      0c9e0100       addi 1, r12, r19
0x000af072      1360           mov r19, r12
0x000af074      5461           and r20, r12
0x000af076      6462           cmp 4, r12
0x000af078      96e5           blt 0xaf03a
