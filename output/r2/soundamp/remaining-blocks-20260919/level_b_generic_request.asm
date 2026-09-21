0x000d40c8      9535           br 0xd412a
0x000d40ca      63370100       st.w r6, 0[sp]
0x000d40ce      23df0100       ld.w 0[sp], r27
0x000d40d2      633f0500       st.w r7, 4[sp]                          ; 4
0x000d40d6      23e70500       ld.w 4[sp], r28                         ; 4
0x000d40da      0332           mov 3, r6
0x000d40dc      80ff3897       jarl 0xdd814, lp
0x000d40e0      00e8           mov r0, r29
0x000d40e2      1d58           mov r29, r11
0x000d40e4      c25a           shl 2, r11
0x000d40e6      2456c0c1       movea -15936, gp, r10
0x000d40ea      cb51           add r11, r10
0x000d40ec      0a30           mov r10, r6
0x000d40ee      3c670100       ld.w 0[r28], r12
0x000d40f2      cb61           add r11, r12
0x000d40f4      0c38           mov r12, r7
0x000d40f6      bcffa25c       jarl 0x99d98, lp
0x000d40fa      1d6e0100       addi 1, r29, r13
0x000d40fe      0de8           mov r13, r29
0x000d4100      54e9           and r20, r29
0x000d4102      62ea           cmp 2, r29
0x000d4104      f6ed           blt 0xd40e2
0x000d4106      20367101       movea 369, r0, r6
0x000d410a      beff86f8       jarl 0xc3990, lp
0x000d410e      3b770100       ld.w 0[r27], r14
0x000d4112      0e30           mov r14, r6
0x000d4114      243ec4c1       movea -15932, gp, r7
0x000d4118      bcff2c5c       jarl 0x99d44, lp
0x000d411c      047a           mov 4, r15
0x000d411e      7b7f0400       st.h r15, 4[r27]                        ; 4
0x000d4122      0c5a           mov 12, r11
0x000d4124      0b50           mov r11, r10
0x000d4126      4406ff10       dispose 2, {r27, r28, r29, lp}, lp
0x000d412a      8407e110       prepare {r27, r28, r29, lp}, 2
0x000d412e      e5cd           br 0xd40ca
0x000d4130      d525           br 0xd417a
0x000d4132      63370100       st.w r6, 0[sp]
0x000d4136      633f0500       st.w r7, 4[sp]                          ; 4
0x000d413a      23e70500       ld.w 4[sp], r28                         ; 4
0x000d413e      0332           mov 3, r6
0x000d4140      80ffd496       jarl 0xdd814, lp
0x000d4144      00e8           mov r0, r29
0x000d4146      1d58           mov r29, r11
0x000d4148      c25a           shl 2, r11
0x000d414a      2456c0c1       movea -15936, gp, r10
0x000d414e      cb51           add r11, r10
0x000d4150      0a30           mov r10, r6
0x000d4152      3c670100       ld.w 0[r28], r12
0x000d4156      cb61           add r11, r12
0x000d4158      0c38           mov r12, r7
0x000d415a      bcff3e5c       jarl 0x99d98, lp
0x000d415e      1d6e0100       addi 1, r29, r13
0x000d4162      0de8           mov r13, r29
0x000d4164      54e9           and r20, r29
0x000d4166      62ea           cmp 2, r29
0x000d4168      f6ed           blt 0xd4146
0x000d416a      20367101       movea 369, r0, r6
0x000d416e      beff22f8       jarl 0xc3990, lp
0x000d4172      0058           mov r0, r11
0x000d4174      0b50           mov r11, r10
0x000d4176      4406ff00       dispose 2, {r28, r29, lp}, lp
0x000d417a      8407e100       prepare {r28, r29, lp}, 2
0x000d417e      a5dd           br 0xd4132
