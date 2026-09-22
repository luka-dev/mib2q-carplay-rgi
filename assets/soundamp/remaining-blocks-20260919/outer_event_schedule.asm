0x000c3990      b515           br 0xc39b6
0x000c3992      63370100       st.w r6, 0[sp]
0x000c3996      0132           mov 1, r6
0x000c3998      81ff7c9e       jarl 0xdd814, lp
0x000c399c      e3370100       ld.hu 0[sp], r6
0x000c39a0      bfff5c23       jarl 0xb5cfc, lp
0x000c39a4      0132           mov 1, r6
0x000c39a6      81ff8a9e       jarl 0xdd830, lp
0x000c39aa      0132           mov 1, r6
0x000c39ac      0a3a           mov 10, r7
0x000c39ae      81ff2699       jarl 0xdd2d4, lp
0x000c39b2      42063f00       dispose 1, {lp}, lp
0x000c39b6      82072100       prepare {lp}, 1
0x000c39ba      c5ed           br 0xc3992
