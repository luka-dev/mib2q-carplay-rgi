0x000b5cfc      b545           br 0xb5d82
0x000b5cfe      63370100       st.w r6, 0[sp]
0x000b5d02      e3370100       ld.hu 0[sp], r6
0x000b5d06      245e7cbe       movea -16772, gp, r11
0x000b5d0a      e46779be       ld.hu -16776[gp], r12
0x000b5d0e      e46f79be       ld.hu -16776[gp], r13
0x000b5d12      c16a           shl 1, r13
0x000b5d14      cb69           add r11, r13
0x000b5d16      ed770100       ld.hu 0[r13], r14
0x000b5d1a      e671           cmp r6, r14
0x000b5d1c      921d           be 0xb5d4e
0x000b5d1e      e47f7bbe       ld.hu -16774[gp], r15
0x000b5d22      ef61           cmp r15, r12
0x000b5d24      d215           be 0xb5d4e
0x000b5d26      20567801       movea 376, r0, r10
0x000b5d2a      0c860100       addi 1, r12, r16
0x000b5d2e      1060           mov r16, r12
0x000b5d30      5561           and r21, r12
0x000b5d32      ea61           cmp r10, r12
0x000b5d34      a605           blt 0xb5d38
0x000b5d36      0060           mov r0, r12
0x000b5d38      0c88           mov r12, r17
0x000b5d3a      c18a           shl 1, r17
0x000b5d3c      cb89           add r11, r17
0x000b5d3e      f1970100       ld.hu 0[r17], r18
0x000b5d42      f231           cmp r18, r6
0x000b5d44      d205           be 0xb5d4e
0x000b5d46      e49f7bbe       ld.hu -16774[gp], r19
0x000b5d4a      f361           cmp r19, r12
0x000b5d4c      faed           bne 0xb5d2a
0x000b5d4e      e46f7bbe       ld.hu -16774[gp], r13
0x000b5d52      ed61           cmp r13, r12
0x000b5d54      da15           bne 0xb5d7e
0x000b5d56      e4777bbe       ld.hu -16774[gp], r14
0x000b5d5a      c172           shl 1, r14
0x000b5d5c      cb71           add r11, r14
0x000b5d5e      6e370000       st.h r6, 0[r14]
0x000b5d62      247f7abe       ld.h -16774[gp], r15
0x000b5d66      417a           add 1, r15
0x000b5d68      5579           and r21, r15
0x000b5d6a      647f7abe       st.h r15, -16774[gp]
0x000b5d6e      24877abe       ld.h -16774[gp], r16
0x000b5d72      200e7801       movea 376, r0, r1
0x000b5d76      e181           cmp r1, r16
0x000b5d78      b105           bl 0xb5d7e
0x000b5d7a      64077abe       st.h r0, -16774[gp]
0x000b5d7e      42063f00       dispose 1, {lp}, lp
0x000b5d82      82072100       prepare {lp}, 1
0x000b5d86      c5bd           br 0xb5cfe
