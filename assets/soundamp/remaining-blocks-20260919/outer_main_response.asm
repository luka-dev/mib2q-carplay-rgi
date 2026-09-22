0x00099be0      cb7ea000       andi 160, r11, r15
0x00099be4      5479           and r20, r15
0x00099be6      0fd1           or r15, r26
0x00099be8      cb5e5f00       andi 95, r11, r11
0x00099bec      cb864000       andi 64, r11, r16
0x00099bf0      e081           cmp r0, r16
0x00099bf2      fa4d           bne 0x99c90
0x00099bf4      cb8e0f00       andi 15, r11, r17
0x00099bf8      6f8a           cmp 15, r17
0x00099bfa      8a1d           bne 0x99c2a
0x00099bfc      cb961000       andi 16, r11, r18
0x00099c00      e091           cmp r0, r18
0x00099c02      ca15           bne 0x99c2a
0x00099c04      233f1500       ld.w 20[sp], r7                         ; 0x14
0x00099c08      079f0d00       ld.b 13[r7], r19                        ; 0xd
0x00099c0c      629a           cmp 2, r19
0x00099c0e      d20d           be 0x99c28
0x00099c10      07570d00       ld.b 13[r7], r10                        ; 0xd
0x00099c14      6352           cmp 3, r10
0x00099c16      920d           be 0x99c28
0x00099c18      a37f0f00       ld.bu 15[sp], r15                       ; 0xf
0x00099c1c      f479           cmp r20, r15
0x00099c1e      d205           be 0x99c28
0x00099c20      83870f00       ld.bu 14[sp], r16                       ; 0xe
0x00099c24      f481           cmp r20, r16
0x00099c26      aa05           bne 0x99c2a
0x00099c28      0058           mov r0, r11
0x00099c2a      e059           cmp r0, r11
0x00099c2c      c225           be 0x99c74
0x00099c2e      038f0d00       ld.b 13[sp], r17                        ; 0xd
0x00099c32      698a           cmp 9, r17
0x00099c34      d105           bl 0x99c3e
0x00099c36      cb961000       andi 16, r11, r18
0x00099c3a      e091           cmp r0, r18
0x00099c3c      c21d           be 0x99c74
0x00099c3e      23e71100       ld.w 16[sp], r28                        ; 0x10
0x00099c42      cb9e0f00       andi 15, r11, r19
0x00099c46      5c9f0c00       st.b r19, 12[r28]                       ; 0xc
0x00099c4a      23ef1500       ld.w 20[sp], r29                        ; 0x14
0x00099c4e      1c30           mov r28, r6
0x00099c50      1d38           mov r29, r7
0x00099c52      80ff5205       jarl 0x9a1a4, lp
0x00099c56      1c30           mov r28, r6
0x00099c58      1d38           mov r29, r7
0x00099c5a      83ff8a2a       jarl 0xcc6e4, lp
0x00099c5e      0a58           mov r10, r11
0x00099c60      e059           cmp r0, r11
0x00099c62      d205           be 0x99c6c
0x00099c64      1c30           mov r28, r6
0x00099c66      bfff9ae6       jarl 0x98300, lp
0x00099c6a      c505           br 0x99c72
0x00099c6c      1c30           mov r28, r6
0x00099c6e      bfff92eb       jarl 0x98800, lp
0x00099c72      e50d           br 0x99c8e
0x00099c74      23ef1500       ld.w 20[sp], r29                        ; 0x14
0x00099c78      0030           mov r0, r6
0x00099c7a      1d38           mov r29, r7
0x00099c7c      83ff682a       jarl 0xcc6e4, lp
0x00099c80      1d30           mov r29, r6
0x00099c82      83ff7a2a       jarl 0xcc6fc, lp
0x00099c86      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099c8a      bfff76eb       jarl 0x98800, lp
0x00099c8e      e505           br 0x99c9a
0x00099c90      0030           mov r0, r6
0x00099c92      233f1500       ld.w 20[sp], r7                         ; 0x14
0x00099c96      83ff4e2a       jarl 0xcc6e4, lp
0x00099c9a      da7e8000       andi 128, r26, r15
0x00099c9e      e079           cmp r0, r15
0x00099ca0      aa15           bne 0x99cc4
0x00099ca2      03870300       ld.b 3[sp], r16
0x00099ca6      e081           cmp r0, r16
0x00099ca8      e20d           be 0x99cc4
0x00099caa      9ad68000       ori 128, r26, r26
0x00099cae      248fe9a0       ld.w -24344[gp], r17
0x00099cb2      4c8a           add 12, r17
0x00099cb4      648fe9a0       st.w r17, -24344[gp]
0x00099cb8      0497eca0       ld.b -24340[gp], r18
0x00099cbc      5f92           add -1, r18
0x00099cbe      5491           and r20, r18
0x00099cc0      4497eca0       st.b r18, -24340[gp]
0x00099cc4      1a58           mov r26, r11
0x00099cc6      0b50           mov r11, r10
0x00099cc8      4c06fff3       dispose 6, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x00099ccc      8c07e1f3       prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 6
0x00099cd0      bf07a8fa       jr 0x99778
