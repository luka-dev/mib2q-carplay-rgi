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
