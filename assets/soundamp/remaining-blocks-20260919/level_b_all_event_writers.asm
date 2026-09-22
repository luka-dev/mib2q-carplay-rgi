0x000aa39a      a45771be       ld.bu -16783[gp], r10
0x000aa39e      c252           shl 2, r10
0x000aa3a0      2d0668ea0600   mov 0x6ea68, r13
0x000aa3a6      ca69           add r10, r13
0x000aa3a8      2d770100       ld.w 0[r13], r14
0x000aa3ac      63770100       st.w r14, 0[sp]
0x000aa3b0      20361300       movea 19, r0, r6
0x000aa3b4      237e0000       movea 0, sp, r15
0x000aa3b8      2f3f0100       ld.w 0[r15], r7
0x000aa3bc      80ff8c22       jarl 0xac648, lp
0x000aa3c0      80076002       jr 0xaa620
0x000aa3c4      048770c2       ld.b -15760[gp], r16
0x000aa3c8      e081           cmp r0, r16
0x000aa3ca      ea25           bne 0xaa416
0x000aa3cc      848f71c2       ld.bu -15760[gp], r17
0x000aa3d0      c28a           shl 2, r17
0x000aa3d2      3206a0ea0600   mov 0x6eaa0, r18
0x000aa3d8      d191           add r17, r18
0x000aa3da      329f0100       ld.w 0[r18], r19
0x000aa3de      639f0100       st.w r19, 0[sp]
0x000aa3e2      23ee0000       movea 0, sp, r29
0x000aa3e6      20361000       movea 16, r0, r6
0x000aa3ea      3d3f0100       ld.w 0[r29], r7
0x000aa3ee      80ff5a22       jarl 0xac648, lp
0x000aa3f2      a45771c2       ld.bu -15759[gp], r10
0x000aa3f6      c252           shl 2, r10
0x000aa3f8      2d0678ea0600   mov 0x6ea78, r13
0x000aa3fe      ca69           add r10, r13
0x000aa400      2d770100       ld.w 0[r13], r14
0x000aa404      63770100       st.w r14, 0[sp]
0x000aa408      20361400       movea 20, r0, r6
0x000aa40c      3d3f0100       ld.w 0[r29], r7
0x000aa410      80ff3822       jarl 0xac648, lp
0x000aa414      852d           br 0xaa464
0x000aa416      a47f71c2       ld.bu -15759[gp], r15
0x000aa41a      c27a           shl 2, r15
0x000aa41c      300678ea0600   mov 0x6ea78, r16
0x000aa422      cf81           add r15, r16
0x000aa424      308f0100       ld.w 0[r16], r17
0x000aa428      638f0100       st.w r17, 0[sp]
0x000aa42c      23ee0000       movea 0, sp, r29
0x000aa430      20361400       movea 20, r0, r6
0x000aa434      3d3f0100       ld.w 0[r29], r7
0x000aa438      80ff1022       jarl 0xac648, lp
0x000aa43c      0f32           mov 15, r6
0x000aa43e      83ff5a2a       jarl 0xdce98, lp
0x000aa442      849771c2       ld.bu -15760[gp], r18
0x000aa446      c292           shl 2, r18
0x000aa448      3306a0ea0600   mov 0x6eaa0, r19
0x000aa44e      d299           add r18, r19
0x000aa450      33570100       ld.w 0[r19], r10
0x000aa454      63570100       st.w r10, 0[sp]
0x000aa458      20361000       movea 16, r0, r6
0x000aa45c      3d3f0100       ld.w 0[r29], r7
0x000aa460      80ffe821       jarl 0xac648, lp
0x000aa464      8007bc01       jr 0xaa620
0x000aa468      846f6fba       ld.bu -17810[gp], r13
0x000aa46c      f56f400a       mul 21, r13, r1
0x000aa470      2e0608001000   mov 0x100008, r14
0x000aa476      cd71           add r13, r14
0x000aa478      847f97ba       ld.bu -17770[gp], r15
0x000aa47c      cf71           add r15, r14
0x000aa47e      0e870000       ld.b 0[r14], r16
0x000aa482      4182           add 1, r16
0x000aa484      1030           mov r16, r6
0x000aa486      84ff1e18       jarl 0xebca4, lp
0x000aa48a      0680           mov r6, r16
0x000aa48c      63870100       st.w r16, 0[sp]
0x000aa490      20361100       movea 17, r0, r6
0x000aa494      238e0000       movea 0, sp, r17
0x000aa498      313f0100       ld.w 0[r17], r7
0x000aa49c      80ffac21       jarl 0xac648, lp
0x000aa4a0      80078001       jr 0xaa620
0x000aa4a4      849771c2       ld.bu -15760[gp], r18
0x000aa4a8      c292           shl 2, r18
0x000aa4aa      3306a0ea0600   mov 0x6eaa0, r19
0x000aa4b0      d299           add r18, r19
0x000aa4b2      33570100       ld.w 0[r19], r10
0x000aa4b6      63570100       st.w r10, 0[sp]
0x000aa4ba      20361000       movea 16, r0, r6
0x000aa4be      236e0000       movea 0, sp, r13
0x000aa4c2      2d3f0100       ld.w 0[r13], r7
0x000aa4c6      80ff8221       jarl 0xac648, lp
0x000aa4ca      80075601       jr 0xaa620
0x000aa4ce      247e72c2       movea -15758, gp, r15
0x000aa4d2      847785c2       ld.bu -15740[gp], r14
0x000aa4d6      ce79           add r14, r15
0x000aa4d8      0f870000       ld.b 0[r15], r16
0x000aa4dc      1030           mov r16, r6
0x000aa4de      84ffc617       jarl 0xebca4, lp
0x000aa4e2      0680           mov r6, r16
0x000aa4e4      63870100       st.w r16, 0[sp]
0x000aa4e8      23ee0000       movea 0, sp, r29
0x000aa4ec      20361500       movea 21, r0, r6
0x000aa4f0      3d3f0100       ld.w 0[r29], r7
0x000aa4f4      80ff5421       jarl 0xac648, lp
0x000aa4f8      248e85c2       movea -15739, gp, r17
0x000aa4fc      849785c2       ld.bu -15740[gp], r18
0x000aa500      d289           add r18, r17
0x000aa502      119f0000       ld.b 0[r17], r19
0x000aa506      1330           mov r19, r6
0x000aa508      84ff9c17       jarl 0xebca4, lp
0x000aa50c      0698           mov r6, r19
0x000aa50e      639f0100       st.w r19, 0[sp]
0x000aa512      20361600       movea 22, r0, r6
0x000aa516      3d3f0100       ld.w 0[r29], r7
0x000aa51a      80ff2e21       jarl 0xac648, lp
0x000aa51e      80070201       jr 0xaa620
0x000aa522      045797c2       ld.b -15721[gp], r10
0x000aa526      0a30           mov r10, r6
0x000aa528      84ff7c17       jarl 0xebca4, lp
0x000aa52c      0650           mov r6, r10
0x000aa52e      63570100       st.w r10, 0[sp]
0x000aa532      20361700       movea 23, r0, r6
0x000aa536      236e0000       movea 0, sp, r13
0x000aa53a      2d3f0100       ld.w 0[r13], r7
0x000aa53e      80ff0a21       jarl 0xac648, lp
0x000aa542      8007de00       jr 0xaa620
0x000aa546      047799c2       ld.b -15719[gp], r14
0x000aa54a      0e30           mov r14, r6
0x000aa54c      84ff5817       jarl 0xebca4, lp
0x000aa550      0670           mov r6, r14
0x000aa552      63770100       st.w r14, 0[sp]
0x000aa556      20361800       movea 24, r0, r6
0x000aa55a      237e0000       movea 0, sp, r15
0x000aa55e      2f3f0100       ld.w 0[r15], r7
0x000aa562      80ffe620       jarl 0xac648, lp
0x000aa566      d55d           br 0xaa620
0x000aa568      04879ac2       ld.b -15718[gp], r16
0x000aa56c      105e6400       addi 100, r16, r11
0x000aa570      e059           cmp r0, r11
0x000aa572      870d           ble 0xaa582
0x000aa574      0b30           mov r11, r6
0x000aa576      84ff2e17       jarl 0xebca4, lp
0x000aa57a      0690           mov r6, r18
0x000aa57c      63970100       st.w r18, 0[sp]
0x000aa580      c505           br 0xaa588
0x000aa582      008a           mov 0, r17
0x000aa584      638f0100       st.w r17, 0[sp]
0x000aa588      20361900       movea 25, r0, r6
0x000aa58c      239e0000       movea 0, sp, r19
0x000aa590      333f0100       ld.w 0[r19], r7
0x000aa594      80ffb420       jarl 0xac648, lp
0x000aa598      c545           br 0xaa620
0x000aa59a      0a5a           mov 10, r11
0x000aa59c      245759ba       ld.w -17832[gp], r10
0x000aa5a0      eb57c202       divu r11, r10, r0
0x000aa5a4      eb57c202       divu r11, r10, r0
0x000aa5a8      0a60           mov r10, r12
0x000aa5aa      200e9001       movea 400, r0, r1
0x000aa5ae      e161           cmp r1, r12
0x000aa5b0      991d           bnl 0xaa5e2
0x000aa5b2      0c68           mov r12, r13
0x000aa5b4      e069           cmp r0, r13
0x000aa5b6      fe0d           bge 0xaa5d4
0x000aa5b8      c16a           shl 1, r13
0x000aa5ba      816a           shr 1, r13
0x000aa5bc      0d30           mov r13, r6
0x000aa5be      84ffe616       jarl 0xebca4, lp
0x000aa5c2      0668           mov r6, r13
0x000aa5c4      4076004f       movhi 20224, r0, r14
0x000aa5c8      0e30           mov r14, r6
0x000aa5ca      0d38           mov r13, r7
0x000aa5cc      84ff6c14       jarl 0xeba38, lp
0x000aa5d0      0668           mov r6, r13
0x000aa5d2      d505           br 0xaa5dc
0x000aa5d4      0d30           mov r13, r6
0x000aa5d6      84ffce16       jarl 0xebca4, lp
0x000aa5da      0668           mov r6, r13
0x000aa5dc      636f0100       st.w r13, 0[sp]
0x000aa5e0      d505           br 0xaa5ea
0x000aa5e2      407ec843       movhi 17352, r0, r15
0x000aa5e6      637f0100       st.w r15, 0[sp]
0x000aa5ea      20361b00       movea 27, r0, r6
0x000aa5ee      23860000       movea 0, sp, r16
0x000aa5f2      303f0100       ld.w 0[r16], r7
0x000aa5f6      80ff5220       jarl 0xac648, lp
0x000aa5fa      b515           br 0xaa620
0x000aa5fc      a515           br 0xaa620
0x000aa5fe      9515           br 0xaa620
