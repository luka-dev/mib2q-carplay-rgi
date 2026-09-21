0x00099cd4      f515           br 0x99d02
0x00099cd6      63370100       st.w r6, 0[sp]
0x00099cda      23370100       ld.w 0[sp], r6
0x00099cde      633f0500       st.w r7, 4[sp]                          ; 4
0x00099ce2      233f0500       ld.w 4[sp], r7                          ; 4
0x00099ce6      0650           mov r6, r10
0x00099ce8      0a360100       addi 1, r10, r6
0x00099cec      e75f0100       ld.hu 0[r7], r11
0x00099cf0      a85a           sar 8, r11
0x00099cf2      4a5f0000       st.b r11, 0[r10]
0x00099cf6      27670000       ld.h 0[r7], r12
0x00099cfa      46670000       st.b r12, 0[r6]
0x00099cfe      44063f00       dispose 2, {lp}, lp
0x00099d02      84072100       prepare {lp}, 2
0x00099d06      85ed           br 0x99cd6
0x00099d08      b51d           br 0x99d3e
0x00099d0a      63370100       st.w r6, 0[sp]
0x00099d0e      23370100       ld.w 0[sp], r6
0x00099d12      633f0500       st.w r7, 4[sp]                          ; 4
0x00099d16      233f0500       ld.w 4[sp], r7                          ; 4
0x00099d1a      0750           mov r7, r10
0x00099d1c      0a3e0100       addi 1, r10, r7
0x00099d20      8a5f0100       ld.bu 0[r10], r11
0x00099d24      c85a           shl 8, r11
0x00099d26      665f0000       st.h r11, 0[r6]
0x00099d2a      0660           mov r6, r12
0x00099d2c      2c6f0000       ld.h 0[r12], r13
0x00099d30      87770100       ld.bu 0[r7], r14
0x00099d34      ce69           add r14, r13
0x00099d36      666f0000       st.h r13, 0[r6]
0x00099d3a      44063f00       dispose 2, {lp}, lp
0x00099d3e      84072100       prepare {lp}, 2
0x00099d42      c5e5           br 0x99d0a
0x00099d44      e525           br 0x99d90
0x00099d46      63370500       st.w r6, 4[sp]                          ; 4
0x00099d4a      23e70500       ld.w 4[sp], r28                         ; 4
0x00099d4e      633f0900       st.w r7, 8[sp]
0x00099d52      23ef0900       ld.w 8[sp], r29
0x00099d56      3d670100       ld.w 0[r29], r12
0x00099d5a      0058           mov r0, r11
0x00099d5c      8162           shr 1, r12
0x00099d5e      0b560100       addi 1, r11, r10
0x00099d62      0a58           mov r10, r11
0x00099d64      5459           and r20, r11
0x00099d66      6f5a           cmp 15, r11
0x00099d68      a7fd           ble 0x99d5c
0x00099d6a      63670200       st.h r12, 2[sp]
0x00099d6e      1c30           mov r28, r6
0x00099d70      233e0200       movea 2, sp, r7
0x00099d74      bfff60ff       jarl 0x99cd4, lp
0x00099d78      42e2           add 2, r28
0x00099d7a      3d6f0100       ld.w 0[r29], r13
0x00099d7e      636f0000       st.h r13, 0[sp]
0x00099d82      1c30           mov r28, r6
0x00099d84      233e0000       movea 0, sp, r7
0x00099d88      bfff4cff       jarl 0x99cd4, lp
0x00099d8c      4606ff00       dispose 3, {r28, r29, lp}, lp
0x00099d90      8607e100       prepare {r28, r29, lp}, 3
0x00099d94      95dd           br 0x99d46
0x00099d96      0000           nop
0x00099d98      d525           br 0x99de2
0x00099d9a      63370500       st.w r6, 4[sp]                          ; 4
0x00099d9e      633f0900       st.w r7, 8[sp]
0x00099da2      23ef0900       ld.w 8[sp], r29
0x00099da6      23360200       movea 2, sp, r6
0x00099daa      1d38           mov r29, r7
0x00099dac      bfff5cff       jarl 0x99d08, lp
0x00099db0      42ea           add 2, r29
0x00099db2      23360000       movea 0, sp, r6
0x00099db6      1d38           mov r29, r7
0x00099db8      bfff50ff       jarl 0x99d08, lp
0x00099dbc      e3670300       ld.hu 2[sp], r12
0x00099dc0      0058           mov r0, r11
0x00099dc2      c162           shl 1, r12
0x00099dc4      0b560100       addi 1, r11, r10
0x00099dc8      0a58           mov r10, r11
0x00099dca      5459           and r20, r11
0x00099dcc      6f5a           cmp 15, r11
0x00099dce      a7fd           ble 0x99dc2
0x00099dd0      e36f0100       ld.hu 0[sp], r13
0x00099dd4      0c69           or r12, r13
0x00099dd6      23770500       ld.w 4[sp], r14                         ; 4
0x00099dda      6e6f0100       st.w r13, 0[r14]
0x00099dde      46067f00       dispose 3, {r29, lp}, lp
0x00099de2      86076100       prepare {r29, lp}, 3
0x00099de6      a5dd           br 0x99d9a
