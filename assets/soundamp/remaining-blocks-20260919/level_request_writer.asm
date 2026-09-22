0x000c5b64      63370100       st.w r6, 0[sp]
0x000c5b68      235f0100       ld.w 0[sp], r11
0x000c5b6c      633f0500       st.w r7, 4[sp]                          ; 4
0x000c5b70      23e70500       ld.w 4[sp], r28                         ; 4
0x000c5b74      3c570400       ld.h 4[r28], r10                        ; 4
0x000c5b78      6252           cmp 2, r10
0x000c5b7a      920d           be 0xc5b8c
0x000c5b7c      0b30           mov r11, r6
0x000c5b7e      053a           mov 5, r7
0x000c5b80      bdff6842       jarl 0x99de8, lp
0x000c5b84      0a58           mov r10, r11
0x000c5b86      0f5a           mov 15, r11
0x000c5b88      8007a601       jr 0xc5d2e
0x000c5b8c      0070           mov r0, r14
0x000c5b8e      3c7f0100       ld.w 0[r28], r15
0x000c5b92      0f870000       ld.b 0[r15], r16
0x000c5b96      e081           cmp r0, r16
0x000c5b98      ca05           bne 0xc5ba0
0x000c5b9a      0060           mov r0, r12
0x000c5b9c      0742           mov 7, r8
0x000c5b9e      853d           br 0xc5c0e
0x000c5ba0      0068           mov r0, r13
0x000c5ba2      3c8f0100       ld.w 0[r28], r17
0x000c5ba6      91970100       ld.bu 0[r17], r18
0x000c5baa      400e0900       movhi 9, r0, r1
0x000c5bae      819f2d6e       ld.bu 28204[r1], r19                    ; 0x6e2c
0x000c5bb2      f391           cmp r19, r18
0x000c5bb4      e215           be 0xc5be0
0x000c5bb6      676a           cmp 7, r13
0x000c5bb8      ce15           bge 0xc5be0
0x000c5bba      2c062c6e0900   mov 0x96e2c, r12
0x000c5bc0      0d560100       addi 1, r13, r10
0x000c5bc4      0a68           mov r10, r13
0x000c5bc6      5469           and r20, r13
0x000c5bc8      0c78           mov r12, r15
0x000c5bca      cd79           add r13, r15
0x000c5bcc      8f870100       ld.bu 0[r15], r16
0x000c5bd0      3c8f0100       ld.w 0[r28], r17
0x000c5bd4      91970100       ld.bu 0[r17], r18
0x000c5bd8      f281           cmp r18, r16
0x000c5bda      b205           be 0xc5be0
0x000c5bdc      676a           cmp 7, r13
0x000c5bde      96f5           blt 0xc5bc0
0x000c5be0      676a           cmp 7, r13
0x000c5be2      9615           blt 0xc5c04
0x000c5be4      0b30           mov r11, r6
0x000c5be6      0e9e0100       addi 1, r14, r19
0x000c5bea      5499           and r20, r19
0x000c5bec      1338           mov r19, r7
0x000c5bee      3c570100       ld.w 0[r28], r10
0x000c5bf2      ce51           add r14, r10
0x000c5bf4      0a40           mov r10, r8
0x000c5bf6      014a           mov 1, r9
0x000c5bf8      bdffc442       jarl 0x99ebc, lp
0x000c5bfc      0a58           mov r10, r11
0x000c5bfe      0f5a           mov 15, r11
0x000c5c00      80072e01       jr 0xc5d2e
0x000c5c04      0d60           mov r13, r12
0x000c5c06      0c7e0100       addi 1, r12, r15
0x000c5c0a      5479           and r20, r15
0x000c5c0c      0f40           mov r15, r8
0x000c5c0e      0172           mov 1, r14
0x000c5c10      3c8f0100       ld.w 0[r28], r17
0x000c5c14      b16f0100       ld.bu 1[r17], r13
0x000c5c18      200e7e00       movea 126, r0, r1                       ; '~'
0x000c5c1c      e169           cmp r1, r13
0x000c5c1e      9715           ble 0xc5c40
0x000c5c20      0b30           mov r11, r6
0x000c5c22      0e860100       addi 1, r14, r16
0x000c5c26      5481           and r20, r16
0x000c5c28      1038           mov r16, r7
0x000c5c2a      3c970100       ld.w 0[r28], r18
0x000c5c2e      ce91           add r14, r18
0x000c5c30      1240           mov r18, r8
0x000c5c32      014a           mov 1, r9
0x000c5c34      bdff8842       jarl 0x99ebc, lp
0x000c5c38      0a58           mov r10, r11
0x000c5c3a      0f5a           mov 15, r11
0x000c5c3c      8007f200       jr 0xc5d2e
0x000c5c40      20ee5a00       movea 90, r0, r29                       ; 'Z'
0x000c5c44      0058           mov r0, r11
0x000c5c46      0c38           mov r12, r7
0x000c5c48      e861           cmp r8, r12
0x000c5c4a      8955           bnl 0xc5cea
0x000c5c4c      26062c6e0900   mov 0x96e2c, r6
0x000c5c52      2476bfc3       movea -15425, gp, r14
0x000c5c56      0698           mov r6, r19
0x000c5c58      c799           add r7, r19
0x000c5c5a      137f0000       ld.b 0[r19], r15
0x000c5c5e      0f7edfff       addi -33, r15, r15
0x000c5c62      0f58           mov r15, r11
0x000c5c64      5459           and r20, r11
0x000c5c66      0e88           mov r14, r17
0x000c5c68      cb89           add r11, r17
0x000c5c6a      91870100       ld.bu 0[r17], r16
0x000c5c6e      f069           cmp r16, r13
0x000c5c70      b205           be 0xc5c76
0x000c5c72      20eea500       movea 165, r0, r29
0x000c5c76      0d60           mov r13, r12
0x000c5c78      e059           cmp r0, r11
0x000c5c7a      b205           be 0xc5c80
0x000c5c7c      635a           cmp 3, r11
0x000c5c7e      8725           ble 0xc5cbe
0x000c5c80      0e90           mov r14, r18
0x000c5c82      cb91           add r11, r18
0x000c5c84      92570100       ld.bu 0[r18], r10
0x000c5c88      ec51           cmp r12, r10
0x000c5c8a      890d           bnl 0xc5c9a
0x000c5c8c      0e98           mov r14, r19
0x000c5c8e      cb99           add r11, r19
0x000c5c90      137f0000       ld.b 0[r19], r15
0x000c5c94      af61           sub r15, r12
0x000c5c96      5461           and r20, r12
0x000c5c98      850d           br 0xc5ca8
0x000c5c9a      0e88           mov r14, r17
0x000c5c9c      cb89           add r11, r17
0x000c5c9e      11870000       ld.b 0[r17], r16
0x000c5ca2      ad81           sub r13, r16
0x000c5ca4      1060           mov r16, r12
0x000c5ca6      5461           and r20, r12
0x000c5ca8      0692           mov 6, r18
0x000c5caa      4497aec3       st.b r18, -15442[gp]
0x000c5cae      0c50           mov r12, r10
0x000c5cb0      ea574002       mul 10, r10, r0
0x000c5cb4      a252           sar 2, r10
0x000c5cb6      5551           and r21, r10
0x000c5cb8      6457acc3       st.h r10, -15444[gp]
0x000c5cbc      d50d           br 0xc5cd6
0x000c5cbe      0b98           mov r11, r19
0x000c5cc0      c29a           shl 2, r19
0x000c5cc2      2466acc3       movea -15444, gp, r12
0x000c5cc6      d361           add r19, r12
0x000c5cc8      027a           mov 2, r15
0x000c5cca      4c7f0200       st.b r15, 2[r12]
0x000c5cce      208e3200       movea 50, r0, r17                       ; '2'
0x000c5cd2      6c8f0000       st.h r17, 0[r12]
0x000c5cd6      0e80           mov r14, r16
0x000c5cd8      cb81           add r11, r16
0x000c5cda      506f0000       st.b r13, 0[r16]
0x000c5cde      07960100       addi 1, r7, r18
0x000c5ce2      5491           and r20, r18
0x000c5ce4      1238           mov r18, r7
0x000c5ce6      e839           cmp r8, r7
0x000c5ce8      f1b5           bl 0xc5c56
0x000c5cea      0332           mov 3, r6
0x000c5cec      203e3e00       movea 62, r0, r7                        ; '>'
0x000c5cf0      2456bfc3       movea -15425, gp, r10
0x000c5cf4      cb51           add r11, r10
0x000c5cf6      8a9f0100       ld.bu 0[r10], r19
0x000c5cfa      1340           mov r19, r8
0x000c5cfc      0b48           mov r11, r9
0x000c5cfe      82fff245       jarl 0xea2f0, lp
0x000c5d02      200ea500       movea 165, r0, r1
0x000c5d06      e1e9           cmp r1, r29
0x000c5d08      ca0d           bne 0xc5d20
0x000c5d0a      3c7f0100       ld.w 0[r28], r15
0x000c5d0e      0f8f0000       ld.b 0[r15], r17
0x000c5d12      448f7cf2       st.b r17, -3460[gp]
0x000c5d16      243698f1       movea -3688, gp, r6
0x000c5d1a      013a           mov 1, r7
0x000c5d1c      81fff060       jarl 0xdbe0c, lp
0x000c5d20      80ffc40b       jarl 0xc68e4, lp
0x000c5d24      0132           mov 1, r6
0x000c5d26      0b3a           mov 11, r7
0x000c5d28      81ffac75       jarl 0xdd2d4, lp
0x000c5d2c      0058           mov r0, r11
0x000c5d2e      0b50           mov r11, r10
0x000c5d30      4406ff00       dispose 2, {r28, r29, lp}, lp
0x000c5d34      8407e100       prepare {r28, r29, lp}, 2
0x000c5d38      bf072cfe       jr 0xc5b64
