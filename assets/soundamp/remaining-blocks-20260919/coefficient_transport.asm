0x000a8bb4      8535           br 0xa8c14
0x000a8bb6      63370100       st.w r6, 0[sp]
0x000a8bba      e3370100       ld.hu 0[sp], r6
0x000a8bbe      633f0500       st.w r7, 4[sp]                          ; 4
0x000a8bc2      e33f0500       ld.hu 4[sp], r7                         ; 4
0x000a8bc6      63470900       st.w r8, 8[sp]
0x000a8bca      e3470900       ld.hu 8[sp], r8
0x000a8bce      2d060a0a0a0a   mov 0xa0a0a0a, r13                      ; '\n\n\n\n'
0x000a8bd4      646fc1bc       st.w r13, -17216[gp]
0x000a8bd8      247741be       ld.w -16832[gp], r14
0x000a8bdc      400e0040       movhi 16384, r0, r1
0x000a8be0      2171           xor r1, r14
0x000a8be2      647741be       st.w r14, -16832[gp]
0x000a8be6      067e0200       addi 2, r6, r15
0x000a8bea      e87f2002       mul r8, r15, r0
0x000a8bee      437a           add 3, r15
0x000a8bf0      cf7eff3f       andi 16383, r15, r15
0x000a8bf4      d07a           shl 16, r15
0x000a8bf6      0f60           mov r15, r12
0x000a8bf8      245ec4bc       movea -17212, gp, r11
0x000a8bfc      0780           mov r7, r16
0x000a8bfe      0c81           or r12, r16
0x000a8c00      248f41be       ld.w -16832[gp], r17
0x000a8c04      1181           or r17, r16
0x000a8c06      6487c5bc       st.w r16, -17212[gp]
0x000a8c0a      6447c9bc       st.w r8, -17208[gp]
0x000a8c0e      0352           mov 3, r10
0x000a8c10      46063f00       dispose 3, {lp}, lp
0x000a8c14      86072100       prepare {lp}, 3
0x000a8c18      f5cd           br 0xa8bb6
0x000a8c1a      0000           nop
0x000a8c1c      c525           br 0xa8c64
0x000a8c1e      63370100       st.w r6, 0[sp]
0x000a8c22      23370100       ld.w 0[sp], r6
0x000a8c26      633f0500       st.w r7, 4[sp]                          ; 4
0x000a8c2a      e33f0500       ld.hu 4[sp], r7                         ; 4
0x000a8c2e      2b060a0a0a0a   mov 0xa0a0a0a, r11                      ; '\n\n\n\n'
0x000a8c34      645fc1bc       st.w r11, -17216[gp]
0x000a8c38      246741be       ld.w -16832[gp], r12
0x000a8c3c      210603000400   mov 0x40003, r1
0x000a8c42      0161           or r1, r12
0x000a8c44      6467c5bc       st.w r12, -17212[gp]
0x000a8c48      246f41be       ld.w -16832[gp], r13
0x000a8c4c      400e0040       movhi 16384, r0, r1
0x000a8c50      2169           xor r1, r13
0x000a8c52      646f41be       st.w r13, -16832[gp]
0x000a8c56      6437c9bc       st.w r6, -17208[gp]
0x000a8c5a      643fcdbc       st.w r7, -17204[gp]
0x000a8c5e      0452           mov 4, r10
0x000a8c60      44063f00       dispose 2, {lp}, lp
0x000a8c64      84072100       prepare {lp}, 2
0x000a8c68      b5dd           br 0xa8c1e
0x000a8c6a      0000           nop
0x000a8c6c      854d           br 0xa8cfc
0x000a8c6e      0050           mov r0, r10
0x000a8c70      247fc5bc       ld.w -17212[gp], r15
0x000a8c74      907a           shr 16, r15
0x000a8c76      5579           and r21, r15
0x000a8c78      cf7eff3f       andi 16383, r15, r15
0x000a8c7c      0f760100       addi 1, r15, r14
0x000a8c80      0060           mov r0, r12
0x000a8c82      e071           cmp r0, r14
0x000a8c84      a33d           bnh 0xa8cf8
0x000a8c86      2436c0bc       movea -17216, gp, r6
0x000a8c8a      246e28bb       movea -17624, gp, r13
0x000a8c8e      0c80           mov r12, r16
0x000a8c90      c282           shl 2, r16
0x000a8c92      c681           add r6, r16
0x000a8c94      1058           mov r16, r11
0x000a8c96      0d88           mov r13, r17
0x000a8c98      ca89           add r10, r17
0x000a8c9a      2b970100       ld.w 0[r11], r18
0x000a8c9e      9892           shr 24, r18
0x000a8ca0      51970000       st.b r18, 0[r17]
0x000a8ca4      0a9e0100       addi 1, r10, r19
0x000a8ca8      1350           mov r19, r10
0x000a8caa      5551           and r21, r10
0x000a8cac      0d78           mov r13, r15
0x000a8cae      ca79           add r10, r15
0x000a8cb0      2b870100       ld.w 0[r11], r16
0x000a8cb4      9082           shr 16, r16
0x000a8cb6      4f870000       st.b r16, 0[r15]
0x000a8cba      0a8e0100       addi 1, r10, r17
0x000a8cbe      1150           mov r17, r10
0x000a8cc0      5551           and r21, r10
0x000a8cc2      0d90           mov r13, r18
0x000a8cc4      ca91           add r10, r18
0x000a8cc6      2b9f0100       ld.w 0[r11], r19
0x000a8cca      889a           shr 8, r19
0x000a8ccc      529f0000       st.b r19, 0[r18]
0x000a8cd0      0a7e0100       addi 1, r10, r15
0x000a8cd4      0f50           mov r15, r10
0x000a8cd6      5551           and r21, r10
0x000a8cd8      0d80           mov r13, r16
0x000a8cda      ca81           add r10, r16
0x000a8cdc      2b8f0100       ld.w 0[r11], r17
0x000a8ce0      508f0000       st.b r17, 0[r16]
0x000a8ce4      0a960100       addi 1, r10, r18
0x000a8ce8      1250           mov r18, r10
0x000a8cea      5551           and r21, r10
0x000a8cec      0c9e0100       addi 1, r12, r19
0x000a8cf0      1360           mov r19, r12
0x000a8cf2      5561           and r21, r12
0x000a8cf4      ee61           cmp r14, r12
0x000a8cf6      c1cd           bl 0xa8c8e
0x000a8cf8      40063f00       dispose 0, {lp}, lp
0x000a8cfc      80072100       prepare {lp}, 0
0x000a8d00      f5b5           br 0xa8c6e
0x000a8d02      0000           nop
0x000a8d04      852d           br 0xa8d54
0x000a8d06      2477c5bc       ld.w -17212[gp], r14
0x000a8d0a      9072           shr 16, r14
0x000a8d0c      ce6eff3f       andi 16383, r14, r13
0x000a8d10      0060           mov r0, r12
0x000a8d12      015a           mov 1, r11
0x000a8d14      616a           cmp 1, r13
0x000a8d16      f30d           bnh 0xa8d34
0x000a8d18      2456c0bc       movea -17216, gp, r10
0x000a8d1c      0b78           mov r11, r15
0x000a8d1e      c27a           shl 2, r15
0x000a8d20      ca79           add r10, r15
0x000a8d22      2f870100       ld.w 0[r15], r16
0x000a8d26      d061           add r16, r12
0x000a8d28      0b8e0100       addi 1, r11, r17
0x000a8d2c      1158           mov r17, r11
0x000a8d2e      5559           and r21, r11
0x000a8d30      ed59           cmp r13, r11
0x000a8d32      d1f5           bl 0xa8d1c
0x000a8d34      e061           cmp r0, r12
0x000a8d36      d305           bnh 0xa8d40
0x000a8d38      2c90           not r12, r18
0x000a8d3a      12660100       addi 1, r18, r12
0x000a8d3e      a505           br 0xa8d42
0x000a8d40      0060           mov r0, r12
0x000a8d42      0b98           mov r11, r19
0x000a8d44      c29a           shl 2, r19
0x000a8d46      2476c0bc       movea -17216, gp, r14
0x000a8d4a      d371           add r19, r14
0x000a8d4c      6e670100       st.w r12, 0[r14]
0x000a8d50      40063f00       dispose 0, {lp}, lp
0x000a8d54      80072100       prepare {lp}, 0
0x000a8d58      f5d5           br 0xa8d06
0x000a8d5a      0000           nop
