0x000b0066      2036a500       movea 165, r0, r6
0x000b006a      a4578dba       ld.bu -17779[gp], r10
0x000b006e      e651           cmp r6, r10
0x000b0070      ca05           bne 0xb0078
0x000b0072      846f95ba       ld.bu -17772[gp], r13
0x000b0076      b505           br 0xb007c
0x000b0078      846fd3c3       ld.bu -15406[gp], r13
0x000b007c      24670988       ld.w -30712[gp], r12
0x000b0080      400e0001       movhi 256, r0, r1
0x000b0084      e167c002       div r1, r12, r0
0x000b0088      0058           mov r0, r11
0x000b008a      440effff       movhi -1, gp, r1
0x000b008e      017f5cd7       ld.b -10404[r1], r15
0x000b0092      ef61           cmp r15, r12
0x000b0094      be15           bge 0xb00ba
0x000b0096      6f5a           cmp 15, r11
0x000b0098      9f15           bgt 0xb00ba
0x000b009a      440effff       movhi -1, gp, r1
0x000b009e      21765cd7       movea -10404, r1, r14
0x000b00a2      0b860100       addi 1, r11, r16
0x000b00a6      1058           mov r16, r11
0x000b00a8      5459           and r20, r11
0x000b00aa      0e88           mov r14, r17
0x000b00ac      cb89           add r11, r17
0x000b00ae      11970000       ld.b 0[r17], r18
0x000b00b2      ec91           cmp r12, r18
0x000b00b4      b705           ble 0xb00ba
0x000b00b6      6f5a           cmp 15, r11
0x000b00b8      d7f5           ble 0xb00a2
0x000b00ba      6f5a           cmp 15, r11
0x000b00bc      a705           ble 0xb00c0
0x000b00be      0f5a           mov 15, r11
0x000b00c0      849fbd87       ld.bu -30788[gp], r19
0x000b00c4      f331           cmp r19, r6
0x000b00c6      9a15           bne 0xb00e8
0x000b00c8      a457d7c1       ld.bu -15913[gp], r10
0x000b00cc      ea59           cmp r10, r11
0x000b00ce      da05           bne 0xb00d8
0x000b00d0      847fd7c1       ld.bu -15914[gp], r15
0x000b00d4      ed79           cmp r13, r15
0x000b00d6      920d           be 0xb00e8
0x000b00d8      446fd6c1       st.b r13, -15914[gp]
0x000b00dc      445fd7c1       st.b r11, -15913[gp]
0x000b00e0      20361100       movea 17, r0, r6
0x000b00e4      81ffac38       jarl 0xc3990, lp
0x000b00e8      40063f00       dispose 0, {lp}, lp
