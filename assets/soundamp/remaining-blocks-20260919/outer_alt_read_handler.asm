0x000d42dc      c52d           br 0xd4334
0x000d42de      63370100       st.w r6, 0[sp]
0x000d42e2      23ef0100       ld.w 0[sp], r29
0x000d42e6      633f0500       st.w r7, 4[sp]                          ; 4
0x000d42ea      235f0500       ld.w 4[sp], r11                         ; 4
0x000d42ee      0752           mov 7, r10
0x000d42f0      4457a1c2       st.b r10, -15711[gp]
0x000d42f4      2436c0c1       movea -15936, gp, r6
0x000d42f8      2b670100       ld.w 0[r11], r12
0x000d42fc      0c38           mov r12, r7
0x000d42fe      bcff9a5a       jarl 0x99d98, lp
0x000d4302      20367201       movea 370, r0, r6
0x000d4306      beff8af6       jarl 0xc3990, lp
0x000d430a      20361000       movea 16, r0, r6
0x000d430e      203e4600       movea 70, r0, r7                        ; 'F'
0x000d4312      80ff928d       jarl 0xdd0a4, lp
0x000d4316      0a58           mov r10, r11
0x000d4318      3d6f0100       ld.w 0[r29], r13
0x000d431c      0d30           mov r13, r6
0x000d431e      243ea4c2       movea -15708, gp, r7
0x000d4322      bcff225a       jarl 0x99d44, lp
0x000d4326      0472           mov 4, r14
0x000d4328      7d770400       st.h r14, 4[r29]                        ; 4
0x000d432c      0c5a           mov 12, r11
0x000d432e      0b50           mov r11, r10
0x000d4330      44067f00       dispose 2, {r29, lp}, lp
0x000d4334      84076100       prepare {r29, lp}, 2
0x000d4338      b5d5           br 0xd42de
0x000d433a      0000           nop
