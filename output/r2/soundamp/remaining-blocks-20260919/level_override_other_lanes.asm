0x000c387a      015a           mov 1, r11
0x000c387c      20369cff       movea -100, r0, r6
0x000c3880      246685c2       movea -15739, gp, r12
0x000c3884      246e72c2       movea -15758, gp, r13
0x000c3888      24769ac2       movea -15718, gp, r14
0x000c388c      0e90           mov r14, r18
0x000c388e      cb91           add r11, r18
0x000c3890      52370000       st.b r6, 0[r18]
0x000c3894      0c78           mov r12, r15
0x000c3896      cb79           add r11, r15
0x000c3898      4f070000       st.b r0, 0[r15]
0x000c389c      0d80           mov r13, r16
0x000c389e      cb81           add r11, r16
0x000c38a0      50070000       st.b r0, 0[r16]
0x000c38a4      0b560100       addi 1, r11, r10
0x000c38a8      0a58           mov r10, r11
0x000c38aa      5459           and r20, r11
0x000c38ac      635a           cmp 3, r11
0x000c38ae      f7ed           ble 0xc388c
0x000c38b0      0132           mov 1, r6
0x000c38b2      0b3a           mov 11, r7
0x000c38b4      81ff209a       jarl 0xdd2d4, lp
0x000c38b8      20363100       movea 49, r0, r6                        ; '1'
0x000c38bc      80ffd400       jarl 0xc3990, lp
0x000c38c0      40063f00       dispose 0, {lp}, lp
0x000c38c4      80072100       prepare {lp}, 0
0x000c38c8      bf07c8fe       jr 0xc3790
