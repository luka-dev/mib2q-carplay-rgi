0x000d455a      63370100       st.w r6, 0[sp]
0x000d455e      633f0500       st.w r7, 4[sp]                          ; 4
0x000d4562      233f0500       ld.w 4[sp], r7                          ; 4
0x000d4566      275f0100       ld.w 0[r7], r11
0x000d456a      8b570100       ld.bu 0[r11], r10
0x000d456e      d50d           br 0xd4588
0x000d4570      2066a500       movea 165, r0, r12
0x000d4574      4467d2c3       st.b r12, -15406[gp]
0x000d4578      9515           br 0xd459a
0x000d457a      206e5a00       movea 90, r0, r13                       ; 'Z'
0x000d457e      446fd2c3       st.b r13, -15406[gp]
0x000d4582      c50d           br 0xd459a
0x000d4584      b50d           br 0xd459a
0x000d4586      a50d           br 0xd459a
0x000d4588      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000d458c      e151           cmp r1, r10
0x000d458e      e2f5           be 0xd457a
0x000d4590      200ea500       movea 165, r0, r1
0x000d4594      e151           cmp r1, r10
0x000d4596      d2ed           be 0xd4570
0x000d4598      e5f5           br 0xd4584
0x000d459a      0050           mov r0, r10
0x000d459c      44063f00       dispose 2, {lp}, lp
0x000d45a0      84072100       prepare {lp}, 2
0x000d45a4      b5dd           br 0xd455a
0x000d45a6      0000           nop
