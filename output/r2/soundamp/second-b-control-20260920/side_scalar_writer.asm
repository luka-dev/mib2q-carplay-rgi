            0x000ac59e      0332           mov 3, r6
            0x000ac5a0      053a           mov 5, r7
            0x000ac5a2      0142           mov 1, r8
            0x000ac5a4      bfff10c6       jarl 0xa8bb4, lp
            0x000ac5a8      405e803f       movhi 16256, r0, r11
            0x000ac5ac      0362           mov 3, r12
            0x000ac5ae      0352           mov 3, r10
            0x000ac5b0      6457cdbc       st.w r10, -17204[gp]
            0x000ac5b4      046a           mov 4, r13
            0x000ac5b6      2f063a070c00   mov 0xc073a, r15
            0x000ac5bc      647fd1bc       st.w r15, -17200[gp]
            0x000ac5c0      0572           mov 5, r14
            0x000ac5c2      645fd5bc       st.w r11, -17196[gp]
            0x000ac5c6      0642           mov 6, r8
            0x000ac5c8      645fd9bc       st.w r11, -17192[gp]
            0x000ac5cc      073a           mov 7, r7
            0x000ac5ce      0832           mov 8, r6
            0x000ac5d0      645fddbc       st.w r11, -17188[gp]
            0x000ac5d4      bfff30c7       jarl 0xa8d04, lp
            0x000ac5d8      bfff94c6       jarl 0xa8c6c, lp
            0x000ac5dc      243628bb       movea -17624, gp, r6
            0x000ac5e0      203e2400       movea 36, r0, r7            ; '$'
            0x000ac5e4      83fff06e       jarl 0xe34d4, lp
            0x000ac5e8      0132           mov 1, r6
            0x000ac5ea      053a           mov 5, r7
            0x000ac5ec      0142           mov 1, r8
            0x000ac5ee      bfffc6c5       jarl 0xa8bb4, lp
            0x000ac5f2      036a           mov 3, r13
            0x000ac5f4      0182           mov 1, r16
            0x000ac5f6      6487cdbc       st.w r16, -17204[gp]
            0x000ac5fa      0462           mov 4, r12
            0x000ac5fc      3106304c0c00   mov 0xc4c30, r17            ; '0L\f'
            0x000ac602      648fd1bc       st.w r17, -17200[gp]
            0x000ac606      055a           mov 5, r11
            0x000ac608      0672           mov 6, r14
            0x000ac60a      440effff       movhi -1, gp, r1
            0x000ac60e      0197a2d7       ld.b -10334[r1], r18
            0x000ac612      c292           shl 2, r18
            0x000ac614      3290           not r18, r18
            0x000ac616      4192           add 1, r18
            0x000ac618      3306588d0600   mov 0x68d58, r19
            0x000ac61e      d299           add r18, r19
            0x000ac620      33574106       ld.w 1600[r19], r10         ; 0x640 ; 1600
            0x000ac624      6457d5bc       st.w r10, -17196[gp]
            0x000ac628      bfffdcc6       jarl 0xa8d04, lp
            0x000ac62c      bfff40c6       jarl 0xa8c6c, lp
            0x000ac630      243628bb       movea -17624, gp, r6
            0x000ac634      203e1c00       movea 28, r0, r7
            0x000ac638      83ff9c6e       jarl 0xe34d4, lp
